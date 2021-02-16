#ifndef meon_scanner_h
#define meon_scanner_h

typedef enum
{
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_PERCENT,
  TOKEN_CARET,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  
  TOKEN_NOT,
  TOKEN_NOT_EQUAL,
  TOKEN_EQUAL,
  TOKEN_ASSIGN,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,

  TOKEN_DOT,
  TOKEN_COMMA,
  TOKEN_SEMICOLON,

  TOKEN_NUMBER_LITERAL,
  TOKEN_STRING_LITERAL,
  TOKEN_IDENTIFIER,
  TOKEN_TRUE,
  TOKEN_FALSE,

  TOKEN_VT_STRING,
  TOKEN_VT_NUMBER,
  TOKEN_VT_BOOLEAN,

  TOKEN_OUTPUT,
  TOKEN_LET,

  TOKEN_EOF,
  TOKEN_ERR,
} token_t;

typedef struct
{
  token_t t;
  const char *start;
  int length;
  int line;
  int sourceIndex;
} Token;

void initScanner(const char *source);
Token scanToken();

#endif