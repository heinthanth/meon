#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
    const char *source;
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

typedef void (*ParseFn)();
typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;

Chunk *compilingChunk;

static Chunk *currentChunk()
{
    return compilingChunk;
}

static void errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;
    parser.panicMode = true;

    //fprintf(stderr, "%.*s\n", token->lexemeLength, token->lexeme);
    fprintf(stderr, "point to [%d, %d]\n", token->line, token->sourceIndex);

    fprintf(stderr, "[line %d] Error", token->line);
    if (token->t == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->t == TOKEN_ERR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
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

static void endCompiler()
{
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), "disassmbler");
    }
#endif
}

static void expression();
static ParseRule *getRule(token_t t);
static void parsePrecedence(Precedence precedence);

static void binary()
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

static void literal()
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

static void grouping()
{
    expression();
    expect(TOKEN_RPAREN, "Expect ')' after expression.");
}

static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string()
{
    emitConstant(OBJ_VAL(cpString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void power()
{
    parsePrecedence(PREC_POWER);
    emit_b(OP_EXPONENT);
}

static void unary()
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
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
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

    prefixRule();

    while (precedence <= getRule(parser.current.t)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.t)->infix;
        infixRule();
    }
}

static ParseRule *getRule(token_t t)
{
    return &rules[t];
}

bool compile(const char *source, Chunk *chunk)
{
    initScanner(source);
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
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    expect(TOKEN_EOF, "Expect end of expression.");

    endCompiler();
    return !parser.hadError;
}