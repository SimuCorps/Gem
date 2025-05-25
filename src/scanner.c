//> Scanning on Demand scanner-c
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

Scanner scanner;
//> init-scanner
void initScanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
  scanner.interpolationDepth = 0;
  scanner.inInterpolatedString = false;
  scanner.pendingInterpolationStart = false;
}
//< init-scanner
//> is-alpha
static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
          c == '_';
}
//< is-alpha
//> is-digit
static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}
//< is-digit
//> is-at-end
static bool isAtEnd() {
  return *scanner.current == '\0';
}
//< is-at-end
//> advance
static char advance() {
  scanner.current++;
  return scanner.current[-1];
}
//< advance
//> peek
static char peek() {
  return *scanner.current;
}
//< peek
//> peek-next
static char peekNext() {
  if (isAtEnd()) return '\0';
  return scanner.current[1];
}
//< peek-next
//> match
static bool match(char expected) {
  if (isAtEnd()) return false;
  if (*scanner.current != expected) return false;
  scanner.current++;
  return true;
}
//< match
//> make-token
static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}
//< make-token
//> error-token
static Token errorToken(const char* message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}
//< error-token
//> skip-whitespace
static void skipWhitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
//> newline
      // Don't skip newlines anymore - they are significant for statement termination
//< newline
//> comment
      case '#':
        // Only treat # as comment if it's not followed by {
        if (peekNext() != '{') {
          // A comment goes until the end of the line.
          while (peek() != '\n' && !isAtEnd()) advance();
          break;
        } else {
          // This is part of interpolation syntax, don't skip it
          return;
        }
//< comment
      default:
        return;
    }
  }
}
//< skip-whitespace
//> check-keyword
static TokenType checkKeyword(int start, int length,
    const char* rest, TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}
//< check-keyword
//> identifier-type
static TokenType identifierType() {
//> keywords
  switch (scanner.start[0]) {
    case 'a':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'n': return checkKeyword(2, 1, "d", TOKEN_AND);
          case 's': return checkKeyword(2, 0, "", TOKEN_AS);
        }
      }
      break;
    case 'b':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'e': return checkKeyword(2, 3, "gin", TOKEN_BEGIN);
          case 'o': return checkKeyword(2, 2, "ol", TOKEN_RETURNTYPE_BOOL);
        }
      }
      break;
    case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
    case 'd': return checkKeyword(1, 2, "ef", TOKEN_DEF);
    case 'e':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'l': 
            if (scanner.current - scanner.start > 3) {
              if (scanner.start[2] == 's') {
                if (scanner.current - scanner.start == 4) {
                  return checkKeyword(3, 1, "e", TOKEN_ELSE);
                } else if (scanner.current - scanner.start == 5) {
                  return checkKeyword(3, 2, "if", TOKEN_ELSIF);
                }
              }
            }
            break;
          case 'n': return checkKeyword(2, 1, "d", TOKEN_END);
        }
      }
      break;
//> keyword-f
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
          case 'u':
            if (scanner.current - scanner.start > 2) {
              switch (scanner.start[2]) {
                case 'n': 
                  if (scanner.current - scanner.start == 3) {
                    return checkKeyword(2, 1, "n", TOKEN_FUN);
                  } else if (scanner.current - scanner.start == 4) {
                    return checkKeyword(2, 2, "nc", TOKEN_RETURNTYPE_FUNC);
                  }
                  break;
              }
            }
            break;
        }
      }
      break;
//< keyword-f
    case 'h': return checkKeyword(1, 3, "ash", TOKEN_RETURNTYPE_HASH);
    case 'i':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'f': 
            if (scanner.current - scanner.start == 2) {
              return TOKEN_IF;
            }
            break;
          case 'n': return checkKeyword(2, 1, "t", TOKEN_RETURNTYPE_INT);
        }
      }
      break;
    case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
    case 'o':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'b': return checkKeyword(2, 1, "j", TOKEN_RETURNTYPE_OBJ);
          case 'r': return checkKeyword(2, 0, "", TOKEN_OR);
        }
      }
      break;
    case 'p': return checkKeyword(1, 3, "uts", TOKEN_PUTS);
    case 'r':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'e':
            if (scanner.current - scanner.start > 2) {
              switch (scanner.start[2]) {
                case 'q': return checkKeyword(3, 4, "uire", TOKEN_REQUIRE);
                case 't': return checkKeyword(3, 3, "urn", TOKEN_RETURN);
              }
            }
            break;
        }
      }
      break;
    case 's':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'u': return checkKeyword(2, 3, "per", TOKEN_SUPER);
          case 't': return checkKeyword(2, 4, "ring", TOKEN_RETURNTYPE_STRING);
        }
      }
      break;
//> keyword-t
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
//< keyword-t
    case 'v': 
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'o': return checkKeyword(2, 2, "id", TOKEN_RETURNTYPE_VOID);
        }
      }
      break;
    case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    case 'm': 
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'o': return checkKeyword(2, 4, "dule", TOKEN_MODULE);
          case 'u': return checkKeyword(2, 1, "t", TOKEN_MUT);
        }
      }
      break;
  }

//< keywords
  return TOKEN_IDENTIFIER;
}
//< identifier-type
//> identifier
static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  return makeToken(identifierType());
}
//< identifier
//> number
static Token number() {
  while (isDigit(peek())) advance();

  // Look for a fractional part.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume the ".".
    advance();

    while (isDigit(peek())) advance();
  }

  return makeToken(TOKEN_NUMBER);
}
//< number
//> string
static Token string() {
  // Scan for string content until we hit #{, " or end of input
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') scanner.line++;
    
    // Check for interpolation start
    if (peek() == '#' && peekNext() == '{') {
      // We found an interpolation start
      if (scanner.current == scanner.start + 1) {
        // This is right after the opening quote with no content before interpolation
        // Return string part with just the quote, then prepare for interpolation start
        scanner.inInterpolatedString = true;
        return makeToken(TOKEN_STRING_PART);
      } else {
        // We have some string content before interpolation
        // Return the string part up to this point (excluding the #)
        scanner.inInterpolatedString = true;
        scanner.pendingInterpolationStart = true;
        return makeToken(TOKEN_STRING_PART);
      }
    }
    
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated string.");

  // The closing quote.
  advance();
  if (!scanner.inInterpolatedString) {
    return makeToken(TOKEN_STRING);
  } else {
    scanner.inInterpolatedString = false;
    return makeToken(TOKEN_STRING);
  }
}

// Handle string continuation after interpolation
static Token stringContinuation() {
  // We're continuing a string after an interpolation ended
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') scanner.line++;
    
    // Check for another interpolation start
    if (peek() == '#' && peekNext() == '{') {
      if (scanner.current - scanner.start == 0) {
        // Start of new interpolation immediately
        advance(); // consume #
        advance(); // consume {
        scanner.interpolationDepth++;
        return makeToken(TOKEN_INTERPOLATION_START);
      } else {
        // String content before next interpolation
        return makeToken(TOKEN_STRING_PART);
      }
    }
    
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated string.");

  // The closing quote.
  advance();
  scanner.inInterpolatedString = false; // Reset flag
  return makeToken(TOKEN_STRING);
}
//< string
//> scan-token
Token scanToken() {
  scanner.start = scanner.current;

  if (isAtEnd()) return makeToken(TOKEN_EOF);

  // If we have a pending interpolation start, handle it
  if (scanner.pendingInterpolationStart) {
    scanner.pendingInterpolationStart = false;
    // We should be positioned at the # character
    if (peek() == '#' && peekNext() == '{') {
      advance(); // consume #
      advance(); // consume {
      scanner.interpolationDepth++;
      return makeToken(TOKEN_INTERPOLATION_START);
    } else {
      return errorToken("Expected interpolation start.");
    }
  }

  // If we just finished an interpolation and we're in an interpolated string,
  // continue scanning for string content BEFORE skipping whitespace
  if (scanner.inInterpolatedString && scanner.interpolationDepth == 0) {
    return stringContinuation();
  }

  // Check if we need to handle immediate interpolation start (for strings like "#{...)
  if (scanner.inInterpolatedString && peek() == '#' && peekNext() == '{') {
    advance(); // consume #
    advance(); // consume {
    scanner.interpolationDepth++;
    return makeToken(TOKEN_INTERPOLATION_START);
  }

//> call-skip-whitespace
  skipWhitespace();
//< call-skip-whitespace
  scanner.start = scanner.current;

  if (isAtEnd()) return makeToken(TOKEN_EOF);

//> scan-char
  
  char c = advance();
//> scan-identifier
  if (isAlpha(c)) return identifier();
//< scan-identifier
//> scan-number
  if (isDigit(c)) return number();
//< scan-number

  switch (c) {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': 
      if (scanner.interpolationDepth > 0) {
        scanner.interpolationDepth--;
        if (scanner.inInterpolatedString) {
          // After closing interpolation, continue scanning string content
          return makeToken(TOKEN_INTERPOLATION_END);
        }
        return makeToken(TOKEN_INTERPOLATION_END);
      }
      return makeToken(TOKEN_RIGHT_BRACE);
    case '[': return makeToken(TOKEN_LEFT_BRACKET);
    case ']': return makeToken(TOKEN_RIGHT_BRACKET);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case '\n': 
      scanner.line++;
      return makeToken(TOKEN_NEWLINE);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);
    case '-': return makeToken(TOKEN_MINUS);
    case '+': return makeToken(TOKEN_PLUS);
    case '/': return makeToken(TOKEN_SLASH);
    case '*': return makeToken(TOKEN_STAR);
    case '%': return makeToken(TOKEN_PERCENT);
    case ':': return makeToken(TOKEN_COLON);
//> two-char
    case '!':
      return makeToken(
          match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
      return makeToken(
          match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
      return makeToken(
          match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
      return makeToken(
          match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '?':
      return makeToken(
          match('!') ? TOKEN_QUESTION_BANG : TOKEN_QUESTION);
//< two-char
//> scan-string
    case '"': return string();
//< scan-string
  }
//< scan-char

  return errorToken("Unexpected character.");
}
//< scan-token
