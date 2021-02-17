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
    case 'a':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'n':
                return detectReservedWord(2, 1, "d", TOKEN_AND);
            }
        }
        break;
    }
    case 'b':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'l':
                return detectReservedWord(2, 3, "ock", TOKEN_BLOCK);
            // case 'o':
            //     return detectReservedWord(2, 5, "olean", TOKEN_VT_BOOLEAN);
            case 'r':
                return detectReservedWord(2, 3, "eak", TOKEN_BREAK);
            }
        }
        break;
    }
    case 'c':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'o':
                return detectReservedWord(2, 6, "ntinue", TOKEN_CONTINUE);
            }
        }
        break;
    }
    case 'e':
    {
        if (scanner.current - scanner.start > 2)
        {
            switch (scanner.start[2])
            {
            case 'd':
            {
                if (scanner.current - scanner.start > 3)
                {
                    switch (scanner.start[3])
                    {
                    case 'b':
                        return detectReservedWord(4, 4, "lock", TOKEN_ENDBLOCK);
                    case 'i':
                        return detectReservedWord(4, 1, "f", TOKEN_ENDIF);
                    case 'f':
                    {
                        if (scanner.current - scanner.start > 4)
                        {
                            switch (scanner.start[4])
                            {
                            case 'o':
                                return detectReservedWord(5, 1, "r", TOKEN_ENDFOR);
                            case 'u':
                                return detectReservedWord(5, 2, "nc", TOKEN_ENDFUNC);
                            }
                        }
                    }
                    case 'w':
                        return detectReservedWord(4, 4, "hile", TOKEN_ENDWHILE);
                    }
                }
            }
            case 's':
            {
                if (scanner.current - scanner.start > 4)
                {
                    return detectReservedWord(4, 2, "if", TOKEN_ELSEIF);
                }
                else if (scanner.current - scanner.start > 3)
                {
                    return detectReservedWord(3, 1, "e", TOKEN_ELSE);
                }
            }
            }
            break;
        }
    }
    case 'f':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'a':
                return detectReservedWord(2, 3, "lse", TOKEN_FALSE);
            case 'o':
                return detectReservedWord(2, 1, "r", TOKEN_FOR);
            case 'u':
                return detectReservedWord(2, 2, "nc", TOKEN_FUNC);
            }
        }
        break;
    }
    case 'i':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'f':
                return TOKEN_IF;
            // case 'o':
            //     return detectReservedWord(2, 5, "olean", TOKEN_VT_BOOLEAN);
            }
        }
        break;
    }
    case 'l':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'e':
                return detectReservedWord(2, 1, "t", TOKEN_LET);
            }
        }
        break;
    }
    case 'n':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'u':
                return detectReservedWord(2, 2, "ll", TOKEN_NULL);
            }
        }
        break;
    }
    case 'o':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'u':
                return detectReservedWord(2, 4, "tput", TOKEN_OUTPUT);
            case 'r':
                return TOKEN_OR;
            }
        }
        break;
    }
    // case 's':
    // {
    //     if (scanner.current - scanner.start > 1)
    //     {
    //         switch (scanner.start[1])
    //         {
    //         case 't':
    //             return detectReservedWord(2, 4, "ring", TOKEN_VT_STRING);
    //         }
    //     }
    //     break;
    // }
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
    case 'w':
    {
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'h':
                return detectReservedWord(2, 3, "ile", TOKEN_WHILE);
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

    Token token;
    token.t = TOKEN_STRING_LITERAL;

    token.start = str;
    token.length = strLength;
    FREE_ARRAY(char, str, strLength);
    str = NULL;

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
    {
        token_t t = TOKEN_ASSIGN;
        if (match('='))
        {
            t = TOKEN_EQUAL;
        }
        else if (match('>'))
        {
            t = TOKEN_THEN;
        }
        return makeToken(t);
    }
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