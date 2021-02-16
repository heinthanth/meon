#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
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
} Local;

typedef struct
{
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler *current = NULL;

Chunk *compilingChunk;

static Chunk *currentChunk()
{
    return compilingChunk;
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

static void emitReturn()
{
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

static void initCompiler(Compiler *compiler)
{
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void endCompiler(int debugLevel)
{
    emitReturn();
    //#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError && debugLevel > 0)
    {
        disassembleChunk(currentChunk(), "disassmbler");
    }
    //#endif
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
        emit_b(OP_POP);
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(token_t t);
static void parsePrecedence(Precedence precedence);

static token_t parse_variable_t(const char *errorMessage)
{
    token_t t = TOKEN_ERR;
    if (parser.current.t == TOKEN_VT_STRING || parser.current.t == TOKEN_VT_NUMBER || parser.current.t == TOKEN_VT_BOOLEAN)
    {
        t = parser.current.t;
        advance();
    }
    else
    {
        errorAtCurrent(errorMessage);
    }
    return t;
}

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
}

static void declareVariable(token_t variable_t)
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

static uint8_t parseVariable(const char *errorMessage, token_t variable_t)
{
    expect(TOKEN_IDENTIFIER, errorMessage);

    declareVariable(variable_t);
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global, token_t variable_t)
//static void defineVariable(uint8_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        emit_bs(OP_DEFINE_LOCAL, makeConstant(NUMBER_VAL(variable_t)));
        return;
    }
    emit_bs(OP_DEFINE_VAR_TYPE, makeConstant(NUMBER_VAL(variable_t)));
    emit_bs(OP_DEFINE_GLOBAL, global);
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

static void varDeclaration()
{
    token_t variable_t = parse_variable_t("Expect variable data type.");
    uint8_t global = parseVariable("Expect variable name.", variable_t);

    if (match(TOKEN_ASSIGN))
    {
        expression();
    }
    else
    {
        switch (variable_t)
        {
        case TOKEN_VT_STRING:
            emitConstant(OBJ_VAL(cpString("", 0)));
            break;
        case TOKEN_VT_NUMBER:
            emitConstant(NUMBER_VAL(0.0));
            break;
        case TOKEN_VT_BOOLEAN:
            emitConstant(BOOL_VAL(false));
            break;
        default:;
        }
    }
    expect(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global, variable_t);
}

static void expressionStatement()
{
    expression();
    expect(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_b(OP_POP);
}

static void printStatement()
{
    expression();
    expect(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_b(OP_OUTPUT);
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
    if (match(TOKEN_LET))
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
    [TOKEN_LPAREN] = {grouping, NULL, PREC_NONE},
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
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
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

bool compile(const char *source, const char *filename, Chunk *chunk, int debugLevel)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
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
    compilingChunk = chunk;

    parser.source = source;
    parser.filename = filename;
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    endCompiler(debugLevel);
    return !parser.hadError;
}