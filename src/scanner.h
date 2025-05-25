//> Scanning on Demand scanner-h
#ifndef gem_scanner_h
#define gem_scanner_h
//> token-type

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
  TOKEN_SEMICOLON, TOKEN_NEWLINE, TOKEN_SLASH, TOKEN_STAR, TOKEN_PERCENT,
  TOKEN_COLON,
  // One or two character tokens.
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,
  // Type suffix tokens.
  TOKEN_QUESTION, TOKEN_QUESTION_BANG,
  // Literals.
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
  // String interpolation tokens.
  TOKEN_STRING_PART, TOKEN_INTERPOLATION_START, TOKEN_INTERPOLATION_END,
  // Keywords.
  TOKEN_AND, TOKEN_BEGIN, TOKEN_CLASS, TOKEN_DEF, TOKEN_ELSE, TOKEN_ELSIF,
  TOKEN_END, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_MODULE, TOKEN_NIL, TOKEN_OR,
  TOKEN_PUTS, TOKEN_REQUIRE, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
  // Return type keywords.
  TOKEN_RETURNTYPE_INT, TOKEN_RETURNTYPE_STRING, TOKEN_RETURNTYPE_BOOL, 
  TOKEN_RETURNTYPE_VOID, TOKEN_RETURNTYPE_FUNC, TOKEN_RETURNTYPE_OBJ,

  TOKEN_ERROR, TOKEN_EOF
} TokenType;
//< token-type
//> token-struct

typedef struct {
  TokenType type;
  const char* start;
  int length;
  int line;
} Token;
//< token-struct

typedef struct {
  const char* start;
  const char* current;
  int line;
  int interpolationDepth; // Track nested interpolation depth
  bool inInterpolatedString; // Track if we're inside an interpolated string
  bool pendingInterpolationStart; // Track if we need to scan #{
} Scanner;

void initScanner(const char* source);
//> scan-token-h
Token scanToken();
//< scan-token-h

#endif
