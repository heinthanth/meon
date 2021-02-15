#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"
#include "mem.h"

typedef struct
{
    const char *start;
    const char *current;
    int line;
    int sourceIndex;
} Scanner;

Scanner scanner;

void initScanner(const char *source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.sourceIndex = -1;
}

static bool isEOF()
{
    return *scanner.current == '\0';
}

static char advance()
{
    scanner.current++;
    scanner.sourceIndex++;
    return scanner.current[-1];
}

static char peek()
{
    return *scanner.current;
}

static char peekNext()
{
    if (isEOF())
        return '\0';
    return scanner.current[1];
}

static bool match(char expected)
{
    if (isEOF())
        return false;
    if (*scanner.current != expected)
        return false;
    scanner.current++;
    return true;
}

static Token makeToken(token_t t)
{
    Token token;
    token.t = t;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    token.sourceIndex = scanner.sourceIndex;
    return token;
}

static Token errorToken(const char *message)
{
    Token token;
    token.t = TOKEN_ERR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    token.sourceIndex = scanner.sourceIndex;
    return token;
}

static void skipWhitespace()
{
    for (;;)
    {
        char c = peek();
        //printf("skip => %d, sourceIndex => %d\n", (int)c, scanner.sourceIndex);
        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
            advance();
            break;
        case '\n':
        {
            scanner.line++;
            scanner.sourceIndex = -1;
            advance();
            break;
        }
        case '/':
            if (peekNext() == '/')
            {
                // A comment goes until the end of the line.
                while (peek() != '\n' && !isEOF())
                    advance();
            }
            else
            {
                return;
            }
            break;
        default:
            return;
        }
    }
}

static token_t detectReservedWord(int start, int length, const char *rest, token_t t)
{
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0)
    {
        return t;
    }
    return TOKEN_IDENTIFIER;
}

static token_t detectIdentifier()
{
    switch (scanner.start[0])
    {
    case 'f':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'a':
                return detectReservedWord(2, 3, "lse", TOKEN_TRUE);
            }
        }
        break;
    }
    case 't':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'r':
                return detectReservedWord(2, 2, "ue", TOKEN_TRUE);
            }
        }
        break;
    }
    }
    return TOKEN_IDENTIFIER;
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static Token makeIdentifier()
{
    while (isAlpha(peek()) || isDigit(peek()))
        advance();

    return makeToken(detectIdentifier());
}

static Token makeNumber()
{
    while (isDigit(peek()))
        advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext()))
    {
        // Consume the ".".
        advance();

        while (isDigit(peek()))
            advance();
    }

    return makeToken(TOKEN_NUMBER_LITERAL);
}

static Token makeString()
{
#define CONCAT_STRING(pointer, c, length)                    \
    pointer = GROW_ARRAY(char, pointer, length, length + 1); \
    str[length++] = c;

    char *str = NULL;
    int strLength = 0;
    bool shouldEscape = false;

    // opening quote
    CONCAT_STRING(str, '"', strLength);

    while ((peek() != '"' || shouldEscape) && !isEOF() && !(peek() == '\n'))
    {
        if (shouldEscape)
        {
            char c = peek();
            switch (c)
            {
            case 'n':
            {
                CONCAT_STRING(str, '\n', strLength);
                shouldEscape = false;
                break;
            }
            case 't':
            {
                CONCAT_STRING(str, '\t', strLength);
                shouldEscape = false;
                break;
            }
            default:
            {
                CONCAT_STRING(str, '\\', strLength);
                CONCAT_STRING(str, peek(), strLength);
                shouldEscape = false;
                break;
            }
            }
        }
        else
        {
            if (peek() == '\\')
            {
                shouldEscape = true;
            }
            else
            {
                CONCAT_STRING(str, peek(), strLength);
            }
        }
        advance();
    }

    if (isEOF() || peek() == '\n')
        return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    CONCAT_STRING(str, '"', strLength);

    printf("strLength %d\n", strLength);

    Token token;
    token.t = TOKEN_STRING_LITERAL;

    token.start = str;
    token.length = strLength;
    FREE_ARRAY(char, str, strLength);

    token.line = scanner.line;
    token.sourceIndex = scanner.sourceIndex;

    return token;
#undef CONCAT_STRING
}

Token scanToken()
{
    skipWhitespace();

    scanner.start = scanner.current;

    if (isEOF())
        return makeToken(TOKEN_EOF);

    char c = advance();

    if (isAlpha(c))
        return makeIdentifier();
    if (isDigit(c))
        return makeNumber();

    switch (c)
    {
    case '(':
        return makeToken(TOKEN_LPAREN);
    case ')':
        return makeToken(TOKEN_RPAREN);
    case '-':
        return makeToken(TOKEN_MINUS);
    case '+':
        return makeToken(TOKEN_PLUS);
    case '/':
        return makeToken(TOKEN_SLASH);
    case '*':
        return makeToken(TOKEN_STAR);
    case '%':
        return makeToken(TOKEN_PERCENT);
    case '^':
        return makeToken(TOKEN_CARET);
    case '.':
        return makeToken(TOKEN_DOT);
    case ',':
        return makeToken(TOKEN_COMMA);
    case ';':
        return makeToken(TOKEN_SEMICOLON);
    case '!':
        return makeToken(match('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
    case '=':
        return makeToken(match('=') ? TOKEN_EQUAL : TOKEN_ASSIGN);
    case '<':
    {
        token_t t = TOKEN_LESS;
        if (match('='))
        {
            t = TOKEN_LESS_EQUAL;
        }
        else if (match('>'))
        {
            t = TOKEN_NOT_EQUAL;
        }
        return makeToken(t);
    }
    case '>':
        return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"':
        return makeString();
    }
    return errorToken("Unexpected character.");
}