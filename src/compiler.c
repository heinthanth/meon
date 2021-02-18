#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "mem.h"
#include "ansi-color.h"

//#ifdef DEBUG_PRINT_CODE
#include "debug.h"
//#endif

typedef struct
{
    const char *source;
    const char *filename;
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // / * %
    PREC_POWER,      // ^
    PREC_UNARY,      // - !
    PREC_CALL,       // -> ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct
{
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct
{
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT
} function_t;

typedef struct Compiler
{
    struct Compiler *enclosing;
    ObjectFunction *function;
    function_t t;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

Parser parser;
Compiler *current = NULL;

static Chunk *currentChunk()
{
    return &current->function->chunk;
}

static char *getSourceLine(int line)
{
    char *s = strdup(parser.source);
    char *found;
    int index = 0;
    while ((found = strsep(&s, "\n")) != NULL)
    {
        if (index == line)
            return found;
        index++;
    }
    return "\0";
}

static void errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;
    parser.panicMode = true;

    fprintf(stderr, YEL "\nERROR:" RESET RED " %s\n\n" RESET, message);
    fprintf(stderr, YEL_UN YEL_HB "%s:\n\n" RESET, parser.filename);
    fprintf(stderr, YEL "%4d |" RESET " %s\n", token->line, getSourceLine(token->line - 1));
    fprintf(stderr, "       %*s", token->sourceIndex, "");
    fprintf(stderr, RED "^ found ERROR around here.\n\n" RESET);
    parser.hadError = true;
}

static void error(const char *message)
{
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.t != TOKEN_ERR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

static void expect(token_t t, const char *message)
{
    if (parser.current.t == t)
    {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(token_t t)
{
    return parser.current.t == t;
}

static bool match(token_t t)
{
    if (!check(t))
        return false;
    advance();
    return true;
}

static void emit_b(uint8_t b)
{
    writeChunk(currentChunk(), b, parser.previous.line);
}

static void emit_bs(uint8_t a, uint8_t b)
{
    emit_b(a);
    emit_b(b);
}

static void emitLoop(int loopStart)
{
    emit_b(OP_LOOP);

    int offset = currentChunk()->size - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emit_b((offset >> 8) & 0xff);
    emit_b(offset & 0xff);
}

static int emitJump(uint8_t instruction)
{
    emit_b(instruction);
    emit_b(0xff);
    emit_b(0xff);
    return currentChunk()->size - 2;
}

static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->size - offset - 2;
    if (jump > UINT16_MAX)
    {
        error("Too much code to jump over.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void emitReturn()
{
    emit_b(OP_NULL);
    emit_b(OP_RETURN);
}

static uint8_t makeConstant(Value value)
{
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emitConstant(Value value)
{
    emit_bs(OP_CONSTANT, makeConstant(value));
}

static void initCompiler(Compiler *compiler, function_t t)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->t = t;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (t != TYPE_SCRIPT)
    {
        current->function->name = cpString(parser.previous.start, parser.previous.length);
    }

    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
    local->isCaptured = false;
}

static ObjectFunction *endCompiler(int debugLevel)
{
    emitReturn();
    ObjectFunction *function = current->function;
    //#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError && debugLevel > 0)
    {
        disassembleChunk(currentChunk(), function->name != NULL
                                             ? function->name->chars
                                             : "[ script ]");
    }
    //#endif
    current = current->enclosing;
    return function;
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;
    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        if (current->locals[current->localCount - 1].isCaptured)
        {
            emit_b(OP_CLOSE_UPVALUE);
        }
        else
        {
            emit_b(OP_POP);
        }
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(token_t t);
static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token *name)
{
    return makeConstant(OBJ_VAL(cpString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal)
{
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++)
    {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
        {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name)
{
    if (compiler->enclosing == NULL)
        return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1)
    {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
    {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name)
{
    if (current->localCount == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void declareVariable()
{
    if (current->scopeDepth == 0)
        return;

    //token_t t = parse_variable_t("Expect variable type.");
    Token *name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (identifiersEqual(name, &local->name))
        {
            error("Variable exists with this name in this scope.");
        }
    }
    addLocal(*name);

    //printf("\n\nGOT VARIABLE TYPE => %d\n\n", variable_t);
}

static uint8_t parseVariable(const char *errorMessage)
{
    expect(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
//static void defineVariable(uint8_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        //emit_bs(OP_DEFINE_VAR_TYPE, makeConstant(NUMBER_VAL(variable_t)));
        //emit_b(OP_DEFINE_LOCAL);
        return;
    }
    //emit_bs(OP_DEFINE_VAR_TYPE, makeConstant(NUMBER_VAL(variable_t)));
    emit_bs(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RPAREN))
    {
        do
        {
            expression();
            if (argCount == 255)
            {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    expect(TOKEN_RPAREN, "Expect ')' after arguments.");
    return argCount;
}

static void logicAnd(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emit_b(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void binary(bool canAssign)
{
    // Remember the operator.
    token_t operator_t = parser.previous.t;

    // Compile the right operand.
    ParseRule *rule = getRule(operator_t);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // Emit the operator instruction.
    switch (operator_t)
    {
    case TOKEN_NOT_EQUAL:
        emit_bs(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL:
        emit_b(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emit_b(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emit_b(OP_GREATER_EQUAL);
        break;
    case TOKEN_LESS:
        emit_b(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emit_b(OP_LESS_EQUAL);
        break;
    case TOKEN_PLUS:
        emit_b(OP_ADD);
        break;
    case TOKEN_DOT:
        emit_b(OP_CONCAT);
        break;
    case TOKEN_MINUS:
        emit_b(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emit_b(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emit_b(OP_DIVIDE);
        break;
    case TOKEN_PERCENT:
        emit_b(OP_MODULO);
    default:
        return; // Unreachable.
    }
}

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emit_bs(OP_CALL, argCount);
}

static void literal(bool canAssign)
{
    switch (parser.previous.t)
    {
    case TOKEN_FALSE:
        emit_b(OP_FALSE);
        break;
    case TOKEN_TRUE:
        emit_b(OP_TRUE);
        break;
    case TOKEN_NULL:
        emit_b(OP_NULL);
        break;
    default:
        return; // Unreachable.
    }
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block(token_t end)
{
    while (!check(end) && !check(TOKEN_EOF))
    {
        declaration();
    }
    expect(end, "Expect 'end' after block.");
}

static void function(function_t t)
{
    Compiler compiler;
    initCompiler(&compiler, t);
    beginScope();

    // Compile the parameter list.
    expect(TOKEN_LPAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RPAREN))
    {
        do
        {
            current->function->argsCount++;
            if (current->function->argsCount > 255)
            {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t paramConstant = parseVariable("Expect parameter name.");
            defineVariable(paramConstant);
        } while (match(TOKEN_COMMA));
    }
    expect(TOKEN_RPAREN, "Expect ')' after parameters.");

    // The body.
    //expect(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(TOKEN_ENDFUNC);

    // Create the function object.
    ObjectFunction *function = endCompiler(0);
    emit_bs(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++)
    {
        emit_b(compiler.upvalues[i].isLocal ? 1 : 0);
        emit_b(compiler.upvalues[i].index);
    }
}

static void funcDeclaration()
{
    //token_t variable_t = parse_variable_t("Expect variable data type.");
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void varDeclaration()
{
    //token_t variable_t = parse_variable_t("Expect variable data type.");
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_ASSIGN))
    {
        expression();
    }
    else
    {
        emit_b(OP_NULL);
    }
    expect(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void expressionStatement()
{
    expression();
    expect(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_b(OP_POP);
}

int innermostLoopStart = -1;
int breakJump = -1;
int innermostLoopScopeDepth = 0;

static void forStatement()
{
    beginScope();

    expect(TOKEN_LPAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_LET))
    {
        varDeclaration();
    }
    else if (match(TOKEN_SEMICOLON))
    {
        // No initializer.
    }
    else
    {
        expressionStatement();
    }

    int surroundingLoopStart = innermostLoopStart;           // <--
    int surroundingLoopScopeDepth = innermostLoopScopeDepth; // <--
    int surroundingBreakJump = breakJump;

    innermostLoopStart = currentChunk()->size;     // <--
    innermostLoopScopeDepth = current->scopeDepth; // <--

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        expect(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emit_b(OP_POP); // Condition.
    }

    if (!match(TOKEN_RPAREN))
    {
        int bodyJump = emitJump(OP_JUMP);

        int incrementStart = currentChunk()->size;
        expression();
        emit_b(OP_POP);
        expect(TOKEN_RPAREN, "Expect ')' after for clauses.");

        emitLoop(innermostLoopStart);        // <--
        innermostLoopStart = incrementStart; // <--
        patchJump(bodyJump);
    }

    if (match(TOKEN_THEN))
    {
        statement();
    }
    else
    {
        while (!check(TOKEN_ENDFOR) && !check(TOKEN_EOF))
        {
            declaration();
        }
        expect(TOKEN_ENDFOR, "Expect 'endfor' after 'for' statement.");
    }

    emitLoop(innermostLoopStart); // <--

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emit_b(OP_POP); // Condition.
    }
    if (breakJump != -1)
    {
        patchJump(breakJump);
        emit_b(OP_POP); // Condition.
    }

    innermostLoopStart = surroundingLoopStart;           // <--
    innermostLoopScopeDepth = surroundingLoopScopeDepth; // <--
    breakJump = surroundingBreakJump;

    endScope();
}

static void whileStatement()
{
    int surroundingLoopStart = innermostLoopStart;           // <--
    int surroundingLoopScopeDepth = innermostLoopScopeDepth; // <--
    int surroundingBreakJump = breakJump;

    innermostLoopStart = currentChunk()->size;
    innermostLoopScopeDepth = current->scopeDepth;

    expect(TOKEN_LPAREN, "Expect '(' after 'while'.");
    expression();
    expect(TOKEN_RPAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emit_b(OP_POP);

    if (match(TOKEN_THEN))
    {
        statement();
    }
    else
    {
        while (!check(TOKEN_ENDWHILE) && !check(TOKEN_EOF))
        {
            declaration();
        }
        expect(TOKEN_ENDWHILE, "Expect 'endwhile' after 'while' statement.");
    }

    emitLoop(innermostLoopStart);
    patchJump(exitJump);
    emit_b(OP_POP);

    if (breakJump != -1)
    {
        patchJump(breakJump);
        //emit_b(OP_POP); // Condition.
    }

    innermostLoopStart = surroundingLoopStart;           // <--
    innermostLoopScopeDepth = surroundingLoopScopeDepth; // <--
    breakJump = surroundingBreakJump;
}

static void continueStatement()
{
    if (innermostLoopStart == -1)
    {
        error("Can't use 'continue' outside of a loop.");
    }

    expect(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    // Discard any locals created inside the loop.
    for (int i = current->localCount - 1;
         i >= 0 && current->locals[i].depth > innermostLoopScopeDepth;
         i--)
    {
        emit_b(OP_POP);
    }

    // Jump to top of current innermost loop.
    emitLoop(innermostLoopStart);
}

static void breakStatement()
{
    if (innermostLoopStart == -1)
    {
        error("Cannot use 'break' outside of a loop.");
        return;
    }

    expect(TOKEN_SEMICOLON, "Expected ';' after 'break'");

    // Discard any locals created inside the loop.
    for (int i = current->localCount - 1;
         i >= 0 && current->locals[i].depth > innermostLoopScopeDepth;
         i--)
    {
        emit_b(OP_POP);
    }

    breakJump = emitJump(OP_JUMP);
}

static void ifStatement()
{
    int *trueJump = NULL;
    int trueJumpSize = 0;

    expect(TOKEN_LPAREN, "Expect '(' after 'if'.");
    expression();
    expect(TOKEN_RPAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emit_b(OP_POP);

    if (match(TOKEN_THEN))
    {
        statement();
        patchJump(thenJump);
        emit_b(OP_POP);
    }
    else
    {
        while (!(check(TOKEN_ENDIF) || check(TOKEN_ELSEIF) || check(TOKEN_ELSE)) && !check(TOKEN_EOF))
        {
            declaration();
        }
        while (match(TOKEN_ELSEIF))
        {
            trueJump = GROW_ARRAY(int, trueJump, trueJumpSize, trueJumpSize + 1);
            trueJump[trueJumpSize++] = emitJump(OP_JUMP);

            patchJump(thenJump);
            emit_b(OP_POP);

            expect(TOKEN_LPAREN, "Expect '(' after 'if'.");
            expression();
            expect(TOKEN_RPAREN, "Expect ')' after condition.");

            thenJump = emitJump(OP_JUMP_IF_FALSE);
            emit_b(OP_POP);

            while (!(check(TOKEN_ENDIF) || check(TOKEN_ELSE) || check(TOKEN_ELSEIF)) && !check(TOKEN_EOF))
            {
                declaration();
            }
        }
        trueJump = GROW_ARRAY(int, trueJump, trueJumpSize, trueJumpSize + 1);
        trueJump[trueJumpSize++] = emitJump(OP_JUMP);

        patchJump(thenJump);
        emit_b(OP_POP);

        if (match(TOKEN_ELSE))
        {

            while (!check(TOKEN_ENDIF) && !check(TOKEN_EOF))
            {
                declaration();
            }
        }
        expect(TOKEN_ENDIF, "Expect 'endif' after 'if' statement.");

        for (int i = 0; i < trueJumpSize - 1; i++)
        {
            patchJump(trueJump[i]);
            emit_b(OP_POP);
        }
        patchJump(trueJump[trueJumpSize - 1]);
        FREE_ARRAY(int, trueJump, trueJumpSize);
    }
}

static void printStatement()
{
    expression();
    expect(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_b(OP_OUTPUT);
}

static void returnStatement()
{
    if (current->t == TYPE_SCRIPT)
    {
        error("Can't return from outside of a function.");
    }
    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        expression();
        expect(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_b(OP_RETURN);
    }
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.t != TOKEN_EOF)
    {
        if (parser.previous.t == TOKEN_SEMICOLON)
            return;
        switch (parser.current.t)
        {
        case TOKEN_OUTPUT:
        case TOKEN_LET:
            return;

        default:
            // Do nothing.
            ;
        }
        advance();
    }
}

static void declaration()
{
    if (match(TOKEN_FUNC))
    {
        return funcDeclaration();
    }
    else if (match(TOKEN_LET))
    {
        varDeclaration();
    }
    else
    {
        statement();
    }

    if (parser.panicMode)
        synchronize();
}

static void statement()
{
    if (match(TOKEN_OUTPUT))
    {
        printStatement();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else if (match(TOKEN_CONTINUE))
    {
        continueStatement();
    }
    else if (match(TOKEN_BREAK))
    {
        breakStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_BLOCK))
    {
        beginScope();
        block(TOKEN_ENDBLOCK);
        endScope();
    }
    else
    {
        expressionStatement();
    }
}

static void grouping(bool canAssign)
{
    expression();
    expect(TOKEN_RPAREN, "Expect ')' after expression.");
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void logicOr(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emit_b(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(cpString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(current, &name)) != -1)
    {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_ASSIGN))
    {
        expression();
        emit_bs(setOp, (uint8_t)arg);
    }
    else
    {
        emit_bs(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

static void power(bool canAssign)
{
    parsePrecedence(PREC_POWER);
    emit_b(OP_EXPONENT);
}

static void unary(bool canAssign)
{
    token_t opearator_t = parser.previous.t;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (opearator_t)
    {
    case TOKEN_NOT:
        emit_b(OP_NOT);
        break;
    case TOKEN_MINUS:
        emit_b(OP_NEGATE);
        break;
    default:
        return; // Unreachable.
    }
}

ParseRule rules[] = {
    [TOKEN_LPAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RPAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, binary, PREC_TERM},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_PERCENT] = {NULL, binary, PREC_FACTOR},
    [TOKEN_CARET] = {unary, power, PREC_POWER},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_NOT_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_ASSIGN] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING_LITERAL] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER_LITERAL] = {number, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, logicAnd, PREC_AND},
    [TOKEN_OR] = {NULL, logicOr, PREC_OR},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERR] = {NULL, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_THEN] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSEIF] = {NULL, NULL, PREC_NONE},
    [TOKEN_ENDIF] = {NULL, NULL, PREC_NONE},
    [TOKEN_BLOCK] = {NULL, NULL, PREC_NONE},
    [TOKEN_ENDBLOCK] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_ENDFOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ENDWHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNC] = {NULL, NULL, PREC_NONE},
    [TOKEN_ENDFUNC] = {NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.t)->prefix;
    if (prefixRule == NULL)
    {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.t)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.t)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_ASSIGN))
    {
        error("Invalid assignment target.");
    }
}

static ParseRule *getRule(token_t t)
{
    return &rules[t];
}

ObjectFunction *compile(const char *source, const char *filename, int debugLevel)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    // int line = -1;
    // for (;;)
    // {
    //     Token token = scanToken();
    //     if (token.line != line)
    //     {
    //         printf("%4d ", token.line);
    //         line = token.line;
    //     }
    //     else
    //     {
    //         printf("   | ");
    //     }
    //     printf("%2d '%.*s'\n", token.t, token.length, token.start);

    //     if (token.t == TOKEN_EOF)
    //         break;
    // }

    parser.source = source;
    parser.filename = filename;
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    ObjectFunction *function = endCompiler(debugLevel);
    return parser.hadError ? NULL : function;
}