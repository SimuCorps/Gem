//> Scanning on Demand compiler-c
#include <stdio.h>
//> Compiling Expressions compiler-include-stdlib
#include <stdlib.h>
//< Compiling Expressions compiler-include-stdlib
//> Local Variables compiler-include-string
#include <string.h>
//< Local Variables compiler-include-string

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
//> Compiling Expressions include-debug

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif
//< Compiling Expressions include-debug
//> Compiling Expressions parser

typedef struct {
  Token current;
  Token previous;
//> had-error-field
  bool hadError;
//< had-error-field
//> panic-mode-field
  bool panicMode;
//< panic-mode-field
} Parser;
//> precedence

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_TERNARY,     // ?:
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;
//< precedence
//> parse-fn-type

//< parse-fn-type
/* Compiling Expressions parse-fn-type < Global Variables parse-fn-type
typedef void (*ParseFn)();
*/
//> Global Variables parse-fn-type
typedef void (*ParseFn)(bool canAssign);
//< Global Variables parse-fn-type
//> parse-rule

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;
//< parse-rule
//> Local Variables local-struct

typedef struct {
  Token name;
  int depth;
//> Closures is-captured-field
  bool isCaptured;
//< Closures is-captured-field
//> Variable Type Tracking
  ReturnType type;
//< Variable Type Tracking
} Local;
//< Local Variables local-struct
//> Closures upvalue-struct
typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;
//< Closures upvalue-struct
//> Calls and Functions function-type-enum
typedef enum {
  TYPE_FUNCTION,
//> Methods and Initializers initializer-type-enum
  TYPE_INITIALIZER,
//< Methods and Initializers initializer-type-enum
//> Methods and Initializers method-type-enum
  TYPE_METHOD,
//< Methods and Initializers method-type-enum
  TYPE_SCRIPT
} FunctionType;
//< Calls and Functions function-type-enum
//> Local Variables compiler-struct

/* Local Variables compiler-struct < Calls and Functions enclosing-field
typedef struct {
*/
//> Calls and Functions enclosing-field
typedef struct Compiler {
  struct Compiler* enclosing;
//< Calls and Functions enclosing-field
//> Calls and Functions function-fields
  ObjFunction* function;
  FunctionType type;

//< Calls and Functions function-fields
  Local locals[UINT8_COUNT];
  int localCount;
//> Closures upvalues-array
  Upvalue upvalues[UINT8_COUNT];
//< Closures upvalues-array
  int scopeDepth;
} Compiler;
//< Local Variables compiler-struct
//> Methods and Initializers class-compiler-struct

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
//> Superclasses has-superclass
  bool hasSuperclass;
//< Superclasses has-superclass
  ObjString* className;
} ClassCompiler;
//< Methods and Initializers class-compiler-struct

//> Global Variable Type Tracking
typedef struct {
  ObjString* name;
  ReturnType type;
} GlobalVar;

typedef struct {
  GlobalVar globals[UINT8_COUNT];
  int count;
} GlobalVarTable;

typedef struct {
  ObjString* fieldName;
  ReturnType fieldType;
} FieldSignature;

typedef struct {
  ObjString* className;
  FieldSignature fields[UINT8_COUNT];
  int fieldCount;
} ClassFieldSignature;

typedef struct {
  ClassFieldSignature classes[UINT8_COUNT];
  int classCount;
} ClassFieldTable;

typedef struct {
  ObjString* functionName;
  ReturnType returnType;
} FunctionSignature;

typedef struct {
  FunctionSignature functions[UINT8_COUNT];
  int count;
} FunctionTable;

typedef struct {
  ObjString* className;
  ObjString* methodName;
  ReturnType returnType;
} MethodSignature;

typedef struct {
  MethodSignature methods[UINT8_COUNT];
  int count;
} MethodTable;

typedef struct {
  ObjString* className;
} ClassEntry;

typedef struct {
  ClassEntry classes[UINT8_COUNT];
  int count;
} ClassTable;

typedef struct {
  ObjString* moduleName;
  ObjString* functionName;
  ReturnType returnType;
} ModuleFunctionSignature;

typedef struct {
  ModuleFunctionSignature functions[UINT8_COUNT];
  int count;
} ModuleFunctionTable;

static GlobalVarTable globalVars;
static ClassFieldTable classFields;
static FunctionTable globalFunctions;
static MethodTable globalMethods;
static ClassTable globalClasses;
static ModuleFunctionTable moduleFunctions;
static ObjString* lastAccessedIdentifier = NULL; // Track for type inference
//< Global Variable Type Tracking

Parser parser;
//< Compiling Expressions parser
//> Local Variables current-compiler
Compiler* current = NULL;
//< Local Variables current-compiler
//> Methods and Initializers current-class
ClassCompiler* currentClass = NULL;
//< Methods and Initializers current-class
//> Expression Type Tracking
static ReturnType lastExpressionType = {RETURN_TYPE_VOID, false, false};
//< Expression Type Tracking
//> Compiling Expressions compiling-chunk
/* Compiling Expressions compiling-chunk < Calls and Functions current-chunk
Chunk* compilingChunk;

static Chunk* currentChunk() {
  return compilingChunk;
}
*/
//> Calls and Functions current-chunk

static Chunk* currentChunk() {
  return &current->function->chunk;
}
//< Calls and Functions current-chunk

//< Compiling Expressions compiling-chunk
//> Compiling Expressions error-at
static void errorAt(Token* token, const char* message) {
//> check-panic-mode
  if (parser.panicMode) return;
//< check-panic-mode
//> set-panic-mode
  parser.panicMode = true;
//< set-panic-mode
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}
//< Compiling Expressions error-at
//> Compiling Expressions error
static void error(const char* message) {
  errorAt(&parser.previous, message);
}
//< Compiling Expressions error
//> Compiling Expressions error-at-current
static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}
//< Compiling Expressions error-at-current
//> Compiling Expressions advance

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}
//< Compiling Expressions advance
//> Compiling Expressions consume
static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}
//< Compiling Expressions consume
//> Global Variables check
static bool check(TokenType type) {
  return parser.current.type == type;
}
//< Global Variables check
//> Global Variables match
static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}
//< Global Variables match
//> Compiling Expressions emit-byte
static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}
//< Compiling Expressions emit-byte
//> Compiling Expressions emit-bytes
static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}
//< Compiling Expressions emit-bytes
//> Jumping Back and Forth emit-loop
static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}
//< Jumping Back and Forth emit-loop
//> Jumping Back and Forth emit-jump
static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}
//< Jumping Back and Forth emit-jump
//> Compiling Expressions emit-return
static void emitReturn() {
/* Calls and Functions return-nil < Methods and Initializers return-this
  emitByte(OP_NIL);
*/
//> Methods and Initializers return-this
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }

//< Methods and Initializers return-this
  emitByte(OP_RETURN);
}
//< Compiling Expressions emit-return
//> Compiling Expressions make-constant
static uint16_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT16_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint16_t)constant;
}
//< Compiling Expressions make-constant
//> Compiling Expressions emit-constant
static void emitConstant(Value value) {
  writeConstant(currentChunk(), value, parser.previous.line);
}
//< Compiling Expressions emit-constant
//> Jumping Back and Forth patch-jump
static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}
//< Jumping Back and Forth patch-jump
//> Local Variables init-compiler
/* Local Variables init-compiler < Calls and Functions init-compiler
static void initCompiler(Compiler* compiler) {
*/
//> Calls and Functions init-compiler
static void initCompiler(Compiler* compiler, FunctionType type) {
//> store-enclosing
  compiler->enclosing = current;
//< store-enclosing
  compiler->function = NULL;
  compiler->type = type;
//< Calls and Functions init-compiler
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
//> Calls and Functions init-function
  compiler->function = newFunction();
//< Calls and Functions init-function
  current = compiler;
//> Calls and Functions init-function-name
  if (type != TYPE_SCRIPT) {
    current->function->name = copyString(parser.previous.start,
                                         parser.previous.length);
  }
//< Calls and Functions init-function-name
//> Calls and Functions init-function-slot

  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
//> Closures init-zero-local-is-captured
  local->isCaptured = false;
//< Closures init-zero-local-is-captured
//> Methods and Initializers slot-zero
  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
    // Set the type of 'this' to be an object of the current class
    if (currentClass != NULL) {
      local->type = CLASS_IMMUTABLE_NONNULL_TYPE(currentClass->className);
    } else {
      local->type = TYPE_VOID;
    }
  } else {
    local->name.start = "";
    local->name.length = 0;
    local->type = TYPE_VOID;
  }
//< Methods and Initializers slot-zero
//< Calls and Functions init-function-slot
}
//< Local Variables init-compiler
//> Compiling Expressions end-compiler
/* Compiling Expressions end-compiler < Calls and Functions end-compiler
static void endCompiler() {
*/
//> Calls and Functions end-compiler
static ObjFunction* endCompiler() {
//< Calls and Functions end-compiler
  emitReturn();
//> Calls and Functions end-function
  ObjFunction* function = current->function;

//< Calls and Functions end-function
//> dump-chunk
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
/* Compiling Expressions dump-chunk < Calls and Functions disassemble-end
    disassembleChunk(currentChunk(), "code");
*/
//> Calls and Functions disassemble-end
    disassembleChunk(currentChunk(), function->name != NULL
        ? AS_CSTRING(OBJ_VAL(function->name)) : "<script>");
//< Calls and Functions disassemble-end
  }
#endif
//< dump-chunk
//> Calls and Functions return-function

//> restore-enclosing
  current = current->enclosing;
//< restore-enclosing
  return function;
//< Calls and Functions return-function
}
//< Compiling Expressions end-compiler
//> Local Variables begin-scope
static void beginScope() {
  current->scopeDepth++;
//> Memory Safety Scope Begin
  vm.currentScopeDepth++;
//< Memory Safety Scope Begin
}
//< Local Variables begin-scope
//> Local Variables end-scope
static void endScope() {
  current->scopeDepth--;
//> Memory Safety Scope End
  vm.currentScopeDepth--;
  
  // Clean up objects created in this scope
  cleanupScopeObjects(vm.currentScopeDepth + 1);
//< Memory Safety Scope End
//> pop-locals

  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth >
            current->scopeDepth) {
/* Local Variables pop-locals < Closures end-scope
    emitByte(OP_POP);
*/
//> Closures end-scope
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
//< Closures end-scope
    current->localCount--;
  }
//< pop-locals
}
//< Local Variables end-scope
//> Compiling Expressions forward-declarations

static void expression();
static void statement();
static void declaration();
static void gemBlock();
static void printStatement();
static void requireStatement();
static void returnStatement();
static void beginStatement();
static void interpolatedString(bool canAssign);
static void typedVarDeclaration();
//< Global Variables forward-declarations

static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
//> Return Type Forward Declarations
static GemType tokenToBaseType(TokenType token);
static ReturnType tokenToReturnType(TokenType token);
static ReturnType inferExpressionType();
static void initGlobalVarTable();
static void addGlobalVar(ObjString* name, ReturnType type);
static ReturnType getGlobalVarType(ObjString* name);
static void setGlobalVarType(ObjString* name, ReturnType type);
static void initClassFieldTable();
static void addClassField(ObjString* className, ObjString* fieldName, ReturnType fieldType);
static ReturnType getClassFieldType(ObjString* className, ObjString* fieldName);
static void initFunctionTable();
static void addFunction(ObjString* name, ReturnType returnType);
static ReturnType getFunctionReturnType(ObjString* name);
static void initClassTable();
static void addClass(ObjString* name);
static bool isClass(ObjString* name);
static void initMethodTable();
static void addMethod(ObjString* className, ObjString* methodName, ReturnType returnType);
static ReturnType getMethodReturnType(ObjString* className, ObjString* methodName);
static void initModuleFunctionTable();
static void addModuleFunction(ObjString* moduleName, ObjString* functionName, ReturnType returnType);
static ReturnType getModuleFunctionReturnType(ObjString* moduleName, ObjString* functionName);
#ifdef WITH_STL
static const char* getCompilerEmbeddedSTLModule(const char* moduleName);
#endif
//< Return Type Forward Declarations

//< Compiling Expressions forward-declarations
//> Global Variables identifier-constant
static uint16_t identifierConstant(Token* name) {
  return makeConstant(OBJ_VAL(copyString(name->start,
                                         name->length)));
}
//< Global Variables identifier-constant
//> Local Variables identifiers-equal
static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}
//< Local Variables identifiers-equal
//> Local Variables resolve-local
static int resolveLocal(Compiler* compiler, Token* name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
//> own-initializer-error
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
//< own-initializer-error
      return i;
    }
  }

  return -1;
}
//< Local Variables resolve-local
//> Closures add-upvalue
static int addUpvalue(Compiler* compiler, uint8_t index,
                      bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
//> existing-upvalue

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

//< existing-upvalue
//> too-many-upvalues
  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

//< too-many-upvalues
  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}
//< Closures add-upvalue
//> Closures resolve-upvalue
static int resolveUpvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
//> mark-local-captured
    compiler->enclosing->locals[local].isCaptured = true;
//< mark-local-captured
    return addUpvalue(compiler, (uint8_t)local, true);
  }

//> resolve-upvalue-recurse
  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }
  
//< resolve-upvalue-recurse
  return -1;
}
//< Closures resolve-upvalue
//> Local Variables add-local
static void addLocal(Token name, ReturnType type) {
//> too-many-locals
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

//< too-many-locals
  Local* local = &current->locals[current->localCount++];
  local->name = name;
/* Local Variables add-local < Local Variables declare-undefined
  local->depth = current->scopeDepth;
*/
//> declare-undefined
  local->depth = -1;
//< declare-undefined
//> Closures init-is-captured
  local->isCaptured = false;
//< Closures init-is-captured
//> Variable Type Storage
  local->type = type;
//< Variable Type Storage
}
//< Local Variables add-local
//> Local Variables declare-variable
static void declareVariable() {
  if (current->scopeDepth == 0) return;

  Token* name = &parser.previous;
//> existing-in-scope
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break; // [negative]
    }
    
    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

//< existing-in-scope
  addLocal(*name, TYPE_VOID);
}

// New function for typed variable declarations
static void typedDeclareVariable(ReturnType type) {
  if (current->scopeDepth == 0) return;

  Token* name = &parser.previous;
//> existing-in-scope-typed
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break; // [negative]
    }
    
    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

//< existing-in-scope-typed
  addLocal(*name, type);
}
//< Local Variables declare-variable
//> Global Variables parse-variable
static uint16_t parseVariable(const char* errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
//> Local Variables parse-local

  declareVariable();
  if (current->scopeDepth > 0) return 0;

//< Local Variables parse-local
  return identifierConstant(&parser.previous);
}
//< Global Variables parse-variable
//> Local Variables mark-initialized
static void markInitialized() {
//> Calls and Functions check-depth
  if (current->scopeDepth == 0) return;
//< Calls and Functions check-depth
  current->locals[current->localCount - 1].depth =
      current->scopeDepth;
}
//< Local Variables mark-initialized
//> Global Variables define-variable
static void defineVariable(uint16_t global) {
//> Local Variables define-variable
  if (current->scopeDepth > 0) {
//> define-local
    markInitialized();
//< define-local
    return;
  }

//< Local Variables define-variable
  emitByte(OP_DEFINE_GLOBAL);
  emitByte((global >> 8) & 0xff);  // High byte
  emitByte(global & 0xff);         // Low byte
}
//< Global Variables define-variable
//> Calls and Functions argument-list
static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
//> arg-limit
      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }
//< arg-limit
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}
//< Calls and Functions argument-list
//> Jumping Back and Forth and
static void and_(bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
  lastExpressionType = TYPE_BOOL;
}
//< Jumping Back and Forth and
//> Ternary conditional
static void ternary(bool canAssign) {
  // At this point, the condition has been evaluated and is on the stack
  
  // Jump to false branch if condition is false
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP); // Pop the condition
  
  // Parse the true expression
  parsePrecedence(PREC_TERNARY + 1);
  ReturnType trueType = lastExpressionType;
  
  // Jump over the false branch
  int endJump = emitJump(OP_JUMP);
  
  // False branch
  patchJump(elseJump);
  emitByte(OP_POP); // Pop the condition
  
  consume(TOKEN_COLON, "Expect ':' after true expression in ternary conditional.");
  
  // Parse the false expression
  parsePrecedence(PREC_TERNARY + 1);
  ReturnType falseType = lastExpressionType;
  
  patchJump(endJump);
  
  // For type inference, we'll use the true type if both are compatible
  // In a more sophisticated system, we'd find the common type
  if (typesEqual(trueType, falseType)) {
    lastExpressionType = trueType;
  } else {
    // Conservative fallback - use void type if types don't match
    lastExpressionType = TYPE_VOID;
  }
}
//< Ternary conditional
//> Compiling Expressions binary
/* Compiling Expressions binary < Global Variables binary
static void binary() {
*/
//> Global Variables binary
static void binary(bool canAssign) {
//< Global Variables binary
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
//> Types of Values comparison-operators
    case TOKEN_BANG_EQUAL:    
      emitBytes(OP_EQUAL, OP_NOT); 
      lastExpressionType = TYPE_BOOL;
      break;
    case TOKEN_EQUAL_EQUAL:   
      emitByte(OP_EQUAL); 
      lastExpressionType = TYPE_BOOL;
      break;
    case TOKEN_GREATER:       
      emitByte(OP_GREATER); 
      lastExpressionType = TYPE_BOOL;
      break;
    case TOKEN_GREATER_EQUAL: 
      emitBytes(OP_LESS, OP_NOT); 
      lastExpressionType = TYPE_BOOL;
      break;
    case TOKEN_LESS:          
      emitByte(OP_LESS); 
      lastExpressionType = TYPE_BOOL;
      break;
    case TOKEN_LESS_EQUAL:    
      emitBytes(OP_GREATER, OP_NOT); 
      lastExpressionType = TYPE_BOOL;
      break;
//< Types of Values comparison-operators
    case TOKEN_PLUS:          
#if OPTIMIZE_SKIP_TYPE_CHECKS
      // Emit optimized opcodes based on operand types
      if (typeEqualsBase(lastExpressionType, TYPE_STRING.baseType)) {
        emitByte(OP_ADD_STRING);
        lastExpressionType = TYPE_STRING; // String concatenation
      } else {
        emitByte(OP_ADD_NUMBER);
        lastExpressionType = TYPE_INT; // Numeric addition
      }
#else
      emitByte(OP_ADD); 
      // For +, we need to check if we're dealing with strings or numbers
      // This is a simplified heuristic - in a real implementation we'd need
      // proper type analysis of both operands
      if (typeEqualsBase(lastExpressionType, TYPE_STRING.baseType)) {
        lastExpressionType = TYPE_STRING; // String concatenation
      } else {
        lastExpressionType = TYPE_INT; // Numeric addition
      }
#endif
      break;
    case TOKEN_MINUS:         
#if OPTIMIZE_SKIP_TYPE_CHECKS
      emitByte(OP_SUBTRACT_NUMBER); 
#else
      emitByte(OP_SUBTRACT); 
#endif
      lastExpressionType = TYPE_INT;
      break;
    case TOKEN_STAR:          
#if OPTIMIZE_SKIP_TYPE_CHECKS
      emitByte(OP_MULTIPLY_NUMBER); 
#else
      emitByte(OP_MULTIPLY); 
#endif
      lastExpressionType = TYPE_INT;
      break;
    case TOKEN_SLASH:         
#if OPTIMIZE_SKIP_TYPE_CHECKS
      emitByte(OP_DIVIDE_NUMBER); 
#else
      emitByte(OP_DIVIDE); 
#endif
      lastExpressionType = TYPE_INT;
      break;
    case TOKEN_PERCENT:       
#if OPTIMIZE_SKIP_TYPE_CHECKS
      emitByte(OP_MODULO_NUMBER); 
#else
      emitByte(OP_MODULO); 
#endif
      lastExpressionType = TYPE_INT;
      break;
    default: return; // Unreachable.
  }
}
//< Compiling Expressions binary
//> Calls and Functions compile-call
static void call(bool canAssign) {
  // Save the identifier before parsing arguments
  ObjString* calledIdentifier = lastAccessedIdentifier;
  
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
  
  // Determine the return type based on what was called
  if (calledIdentifier != NULL) {
    // Check if it's a class instantiation
    if (isClass(calledIdentifier)) {
      // Set class-specific object type - use generic obj type with className for flexibility
      // This allows the object to be assigned to either mutable or immutable variables
      lastExpressionType = (ReturnType){RETURN_TYPE_OBJ, false, false, calledIdentifier};
    } else {
      // Check if it's a function call and get its return type
      ReturnType functionReturnType = getFunctionReturnType(calledIdentifier);
      lastExpressionType = functionReturnType;
    }
  } else {
    // Conservative fallback if we can't determine what was called
    lastExpressionType = TYPE_VOID;
  }
  
  // Reset the tracked identifier
  lastAccessedIdentifier = NULL;
}
//< Calls and Functions compile-call
//> Classes and Instances compile-dot
static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint16_t name = identifierConstant(&parser.previous);
//> Track Property Name for Type Inference
  Token propertyName = parser.previous;
//< Track Property Name for Type Inference

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    
    // Track field assignments if we're in a class and accessing this.fieldName
    if (currentClass != NULL) {
      ObjString* fieldName = copyString(propertyName.start, propertyName.length);
      ReturnType expressionType = inferExpressionType();
      if (!typesEqual(expressionType, TYPE_VOID)) {
        addClassField(currentClass->className, fieldName, expressionType);
      }
    }
    
    emitByte(OP_SET_PROPERTY);
    emitByte((name >> 8) & 0xff);  // High byte
    emitByte(name & 0xff);         // Low byte
//> Methods and Initializers parse-call
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    
    // Check if this is a module function call
    // For now, we'll use a simple heuristic: if the previous expression type 
    // suggests it's a module, use OP_MODULE_CALL
    if (lastAccessedIdentifier != NULL) {
      // Check if the last accessed identifier is a module
      // For simplicity, assume any identifier starting with capital letter is a module
      if (lastAccessedIdentifier->chars[0] >= 'A' && lastAccessedIdentifier->chars[0] <= 'Z') {
        emitByte(OP_MODULE_CALL);
        emitByte((name >> 8) & 0xff);  // High byte
        emitByte(name & 0xff);         // Low byte
        emitByte(argCount);
        
        // Use proper module function signature lookup instead of hardcoded nonsense
        ObjString* methodNameString = copyString(propertyName.start, propertyName.length);
        ReturnType functionReturnType = getModuleFunctionReturnType(lastAccessedIdentifier, methodNameString);
        if (functionReturnType.baseType != TYPE_VOID.baseType) {
          lastExpressionType = functionReturnType;
        } else {
          lastExpressionType = TYPE_VOID; // fallback if function not found in module table
        }
        return;
      }
    }
    
    // Regular method call
    emitByte(OP_INVOKE);
    emitByte((name >> 8) & 0xff);  // High byte
    emitByte(name & 0xff);         // Low byte
    emitByte(argCount);
    
    // For method calls, try to determine the return type based on the object's class
    if (lastExpressionType.baseType == TYPE_OBJ.baseType && lastExpressionType.className != NULL) {
      // We know the specific class, look up the method in that class
      ObjString* methodNameString = copyString(propertyName.start, propertyName.length);
      ReturnType methodReturnType = getMethodReturnType(lastExpressionType.className, methodNameString);
      lastExpressionType = methodReturnType;
    } else if (currentClass != NULL && lastExpressionType.baseType == TYPE_OBJ.baseType) {
      // Fallback: if we're in a method and accessing an obj, assume it's the current class
      ObjString* methodNameString = copyString(propertyName.start, propertyName.length);
      ReturnType methodReturnType = getMethodReturnType(currentClass->className, methodNameString);
      lastExpressionType = methodReturnType;
    } else {
      lastExpressionType = TYPE_VOID; // Conservative fallback for method calls
    }
//< Methods and Initializers parse-call
  } else {
    emitByte(OP_GET_PROPERTY);
    emitByte((name >> 8) & 0xff);  // High byte
    emitByte(name & 0xff);         // Low byte
    
    // Look up field type if we're accessing a field
    if (currentClass != NULL) {
      ObjString* fieldName = copyString(propertyName.start, propertyName.length);
      ReturnType fieldType = getClassFieldType(currentClass->className, fieldName);
      lastExpressionType = fieldType;
    } else {
      lastExpressionType = TYPE_VOID; // Conservative for unknown context
    }
  }
}
//< Classes and Instances compile-dot
//> Types of Values parse-literal
/* Types of Values parse-literal < Global Variables parse-literal
static void literal() {
*/
//> Global Variables parse-literal
static void literal(bool canAssign) {
//< Global Variables parse-literal
  switch (parser.previous.type) {
    case TOKEN_FALSE: 
      emitByte(OP_FALSE); 
      lastExpressionType = TYPE_BOOL;
      break;
    case TOKEN_NIL: 
      emitByte(OP_NIL); 
      lastExpressionType = NULLABLE_IMMUTABLE_TYPE(TYPE_VOID.baseType);
      break;
    case TOKEN_TRUE: 
      emitByte(OP_TRUE); 
      lastExpressionType = TYPE_BOOL;
      break;
    default: return; // Unreachable.
  }
}
//< Types of Values parse-literal
//> Compiling Expressions grouping
/* Compiling Expressions grouping < Global Variables grouping
static void grouping() {
*/
//> Global Variables grouping
static void grouping(bool canAssign) {
//< Global Variables grouping
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}
//< Compiling Expressions grouping
/* Compiling Expressions number < Global Variables number
static void number() {
*/
//> Compiling Expressions number
//> Global Variables number
static void number(bool canAssign) {
//< Global Variables number
  double value = strtod(parser.previous.start, NULL);
/* Compiling Expressions number < Types of Values const-number-val
  emitConstant(value);
*/
//> Types of Values const-number-val
  emitConstant(NUMBER_VAL(value));
//< Types of Values const-number-val
  lastExpressionType = TYPE_INT;
}
//< Compiling Expressions number
//> Jumping Back and Forth or
static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
  lastExpressionType = TYPE_BOOL;
}
//< Jumping Back and Forth or
/* Strings parse-string < Global Variables string
static void string() {
*/
//> Strings parse-string
//> Global Variables string
static void string(bool canAssign) {
//< Global Variables string
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                  parser.previous.length - 2)));
  lastExpressionType = TYPE_STRING;
}
//< Strings parse-string

//> String interpolation function
static void interpolatedString(bool canAssign) {
  int partCount = 0;
  
  // Handle the first string part (TOKEN_STRING_PART)
  // Check if this is an empty string part (just a quote for strings starting with interpolation)
  if (parser.previous.length == 1 && parser.previous.start[0] == '"') {
    // Empty string part - don't emit anything
  } else {
    // Remove the leading quote from the string part
    emitConstant(OBJ_VAL(constantString(parser.previous.start + 1, parser.previous.length - 1)));
    partCount++;
  }
  
  // Process interpolations and string parts
  while (true) {
    if (match(TOKEN_INTERPOLATION_START)) {
      // Parse the expression inside the interpolation
      expression();
      partCount++;
      
      consume(TOKEN_INTERPOLATION_END, "Expect '}' after interpolation expression.");
      
      // Check if there's more content after this interpolation
      if (check(TOKEN_STRING_PART)) {
        advance(); // consume the string part
        // STRING_PART tokens don't have quotes, so copy as-is
        emitConstant(OBJ_VAL(constantString(parser.previous.start, parser.previous.length)));
        partCount++;
      } else if (check(TOKEN_STRING)) {
        advance(); // consume the final string
        // Final STRING token: remove only the trailing quote
        int finalLength = parser.previous.length - 1;
        const char* finalStart = parser.previous.start;
        emitConstant(OBJ_VAL(constantString(finalStart, finalLength)));
        partCount++;
        break; // End of interpolated string
      } else {
        break; // No more string content
      }
    } else {
      break; // No more interpolations
    }
  }
  
  // Emit the interpolate instruction with the part count
  emitBytes(OP_INTERPOLATE, partCount);
  lastExpressionType = TYPE_STRING;
}
//< String interpolation function

/* Global Variables read-named-variable < Global Variables named-variable-signature
static void namedVariable(Token name) {
*/
//> Global Variables named-variable-signature
static void namedVariable(Token name, bool canAssign) {
//< Global Variables named-variable-signature
/* Global Variables read-named-variable < Local Variables named-local
  uint8_t arg = identifierConstant(&name);
*/
//> Global Variables read-named-variable
//> Local Variables named-local
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  uint16_t globalArg = 0;  // For global variable indices
  
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    globalArg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
//< Local Variables named-local
/* Global Variables read-named-variable < Global Variables named-variable
  emitBytes(OP_GET_GLOBAL, arg);
*/
//> named-variable

/* Global Variables named-variable < Global Variables named-variable-can-assign
  if (match(TOKEN_EQUAL)) {
*/
//> named-variable-can-assign
  if (canAssign && match(TOKEN_EQUAL)) {
//< named-variable-can-assign
    expression();
    
    // Type check for local variables
    if (arg != -1 && getOp == OP_GET_LOCAL) {
      // This is a local variable assignment - check mutability first, then types
      Local* local = &current->locals[arg];
      
      // Check mutability - variables must be mutable to be reassigned
      if (!local->type.isMutable) {
        error("Cannot assign to immutable variable.");
      }
      
      ReturnType expressionType = inferExpressionType();
      if (!typesEqual(local->type, TYPE_VOID) && // Allow assignments to untyped vars (from old 'var' declarations)
          !typesEqual(expressionType, TYPE_VOID) && // Allow conservative inference 
          !isAssignmentCompatible(local->type, expressionType)) {
        error("Cannot assign value of incompatible type to variable.");
      }
    }
    
    // Type check for global variables
    if (getOp == OP_GET_GLOBAL) {
      // This is a global variable assignment - check mutability first, then types
      ObjString* varName = copyString(name.start, name.length);
      ReturnType globalType = getGlobalVarType(varName);
      ReturnType expressionType = inferExpressionType();
      
      // Check mutability - variables must be mutable to be reassigned
      if (!globalType.isMutable) {
        error("Cannot assign to immutable variable.");
      }
      
      if (!typesEqual(globalType, TYPE_VOID) && // Allow assignments to untyped vars
          !typesEqual(expressionType, TYPE_VOID) && // Allow conservative inference 
          !isAssignmentCompatible(globalType, expressionType)) {
        error("Cannot assign value of incompatible type to variable.");
      }
    }
    
    if (setOp == OP_SET_GLOBAL) {
      emitByte(setOp);
      emitByte((globalArg >> 8) & 0xff);  // High byte
      emitByte(globalArg & 0xff);         // Low byte
    } else {
      emitBytes(setOp, (uint8_t)arg);
    }
  } else {
/* Global Variables named-variable < Local Variables emit-get
    emitBytes(OP_GET_GLOBAL, arg);
*/
//> Local Variables emit-get
    if (getOp == OP_GET_GLOBAL) {
      emitByte(getOp);
      emitByte((globalArg >> 8) & 0xff);  // High byte
      emitByte(globalArg & 0xff);         // Low byte
    } else {
      emitBytes(getOp, (uint8_t)arg);
    }
    
    // Set expression type when accessing variables
    if (getOp == OP_GET_LOCAL && arg != -1) {
      // For local variables, get the type from the Local struct
      Local* local = &current->locals[arg];
      lastExpressionType = local->type;
    } else if (getOp == OP_GET_GLOBAL) {
      // For global variables, get the type from the global variable table
      ObjString* varName = copyString(name.start, name.length);
      lastExpressionType = getGlobalVarType(varName);
    } else {
      // For upvalues and other cases, use conservative default
      lastExpressionType = TYPE_VOID;
    }
//< Local Variables emit-get
  }
//< named-variable
}
//< Global Variables read-named-variable
/* Global Variables variable-without-assign < Global Variables variable
static void variable() {
  namedVariable(parser.previous);
}
*/
//> Global Variables variable
static void variable(bool canAssign) {
  // Track the identifier for type inference in call expressions
  lastAccessedIdentifier = copyString(parser.previous.start, parser.previous.length);
  namedVariable(parser.previous, canAssign);
}
//< Global Variables variable
//> Superclasses synthetic-token
static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}
//< Superclasses synthetic-token
//> Superclasses super
static void super_(bool canAssign) {
//> super-errors
  if (currentClass == NULL) {
    error("Can't use 'super' outside of a class.");
  } else if (!currentClass->hasSuperclass) {
    error("Can't use 'super' in a class with no superclass.");
  }

//< super-errors
  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint16_t name = identifierConstant(&parser.previous);
//> Track Super Method Name
  Token methodNameToken = parser.previous;
//< Track Super Method Name
//> super-get
  
  namedVariable(syntheticToken("this"), false);
/* Superclasses super-get < Superclasses super-invoke
  namedVariable(syntheticToken("super"), false);
  emitBytes(OP_GET_SUPER, name);
*/
//< super-get
//> super-invoke
  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitByte(OP_SUPER_INVOKE);
    emitByte((name >> 8) & 0xff);  // High byte
    emitByte(name & 0xff);         // Low byte
    emitByte(argCount);
    
    // Try to infer method return type based on method name
    // This is a simplified approach - in a full implementation we'd need
    // proper method signature tracking
    if (methodNameToken.length == 7 && memcmp(methodNameToken.start, "getName", 7) == 0) {
      lastExpressionType = TYPE_STRING;
    } else if (methodNameToken.length == 7 && memcmp(methodNameToken.start, "process", 7) == 0) {
      lastExpressionType = TYPE_VOID;
    } else {
      lastExpressionType = TYPE_VOID; // Conservative fallback for method calls
    }
  } else {
    namedVariable(syntheticToken("super"), false);
    emitByte(OP_GET_SUPER);
    emitByte((name >> 8) & 0xff);  // High byte
    emitByte(name & 0xff);         // Low byte
    
    // For property access, we can't easily determine the type
    lastExpressionType = TYPE_VOID; // Conservative for property access
  }
//< super-invoke
}
//< Superclasses super
//> Methods and Initializers this
static void this_(bool canAssign) {
//> this-outside-class
  if (currentClass == NULL) {
    error("Can't use 'this' outside of a class.");
    return;
  }
  
//< this-outside-class
  variable(false);
} // [this]
//< Methods and Initializers this
//> Compiling Expressions unary
/* Compiling Expressions unary < Global Variables unary
static void unary() {
*/
//> Global Variables unary
static void unary(bool canAssign) {
//< Global Variables unary
  TokenType operatorType = parser.previous.type;

  // Compile the operand.
/* Compiling Expressions unary < Compiling Expressions unary-operand
  expression();
*/
//> unary-operand
  parsePrecedence(PREC_UNARY);
//< unary-operand

  // Emit the operator instruction.
  switch (operatorType) {
//> Types of Values compile-not
    case TOKEN_BANG: emitByte(OP_NOT); break;
//< Types of Values compile-not
    case TOKEN_MINUS: 
#if OPTIMIZE_SKIP_TYPE_CHECKS
      emitByte(OP_NEGATE_NUMBER); 
#else
      emitByte(OP_NEGATE); 
#endif
      break;
    default: return; // Unreachable.
  }
}
//< Compiling Expressions unary
//> Compiling Expressions rules
ParseRule rules[] = {
/* Compiling Expressions rules < Calls and Functions infix-left-paren
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
*/
//> Calls and Functions infix-left-paren
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
//< Calls and Functions infix-left-paren
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, // [big]
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Classes and Instances table-dot
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
*/
//> Classes and Instances table-dot
  [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
//< Classes and Instances table-dot
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_PERCENT]       = {NULL,     binary, PREC_FACTOR},
/* Compiling Expressions rules < Types of Values table-not
  [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-not
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
//< Types of Values table-not
/* Compiling Expressions rules < Types of Values table-equal
  [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-equal
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
//< Types of Values table-equal
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Types of Values table-comparisons
  [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-comparisons
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
//< Types of Values table-comparisons
  [TOKEN_QUESTION]      = {NULL,     ternary, PREC_TERNARY},
  [TOKEN_QUESTION_BANG] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COLON]         = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Global Variables table-identifier
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
*/
//> Global Variables table-identifier
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
//< Global Variables table-identifier
/* Compiling Expressions rules < Strings table-string
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
*/
//> Strings table-string
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
//< Strings table-string
  [TOKEN_STRING_PART]   = {interpolatedString, NULL, PREC_NONE},
  [TOKEN_INTERPOLATION_START] = {NULL, NULL, PREC_NONE},
  [TOKEN_INTERPOLATION_END] = {NULL, NULL, PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
/* Compiling Expressions rules < Jumping Back and Forth table-and
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
*/
//> Jumping Back and Forth table-and
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
//< Jumping Back and Forth table-and
  [TOKEN_BEGIN]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DEF]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSIF]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_END]           = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Types of Values table-false
  [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-false
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
//< Types of Values table-false
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Types of Values table-nil
  [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-nil
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
//< Types of Values table-nil
/* Compiling Expressions rules < Jumping Back and Forth table-or
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
*/
//> Jumping Back and Forth table-or
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
//< Jumping Back and Forth table-or
  [TOKEN_PUTS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Superclasses table-super
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
*/
//> Superclasses table-super
  [TOKEN_SUPER]         = {super_,   NULL,   PREC_NONE},
//< Superclasses table-super
/* Compiling Expressions rules < Methods and Initializers table-this
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
*/
//> Methods and Initializers table-this
  [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
//< Methods and Initializers table-this
/* Compiling Expressions rules < Types of Values table-true
  [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-true
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
//< Types of Values table-true
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};
//< Compiling Expressions rules
//> Compiling Expressions parse-precedence
static void parsePrecedence(Precedence precedence) {
/* Compiling Expressions parse-precedence < Compiling Expressions precedence-body
  // What goes here?
*/
//> precedence-body
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

/* Compiling Expressions precedence-body < Global Variables prefix-rule
  prefixRule();
*/
//> Global Variables prefix-rule
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);
//< Global Variables prefix-rule
//> infix

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
/* Compiling Expressions infix < Global Variables infix-rule
    infixRule();
*/
//> Global Variables infix-rule
    infixRule(canAssign);
//< Global Variables infix-rule
  }
//> Global Variables invalid-assign

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
//< Global Variables invalid-assign
//< infix
//< precedence-body
}
//< Compiling Expressions parse-precedence
//> Compiling Expressions get-rule
static ParseRule* getRule(TokenType type) {
  return &rules[type];
}
//< Compiling Expressions get-rule
//> Compiling Expressions expression
static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}
//< Compiling Expressions expression
//> Local Variables block
static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}
//< Local Variables block
//> Calls and Functions compile-function
static ReturnType function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope(); // [no-end-scope]

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
//> parameters
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      
      // Parse parameter type (int, string, bool, etc.) 
      ReturnType paramType = IMMUTABLE_NONNULL_TYPE(TYPE_VOID.baseType);
      if (check(TOKEN_RETURNTYPE_INT) || check(TOKEN_RETURNTYPE_STRING) || 
          check(TOKEN_RETURNTYPE_BOOL) || check(TOKEN_RETURNTYPE_FUNC) ||
          check(TOKEN_RETURNTYPE_OBJ)) {
        TokenType typeToken = parser.current.type;
        advance(); // Consume the type token first
        paramType = tokenToBaseType(typeToken);
      } else {
        errorAtCurrent("Expect parameter type (int, string, bool, func, or obj).");
      }
      
      consume(TOKEN_IDENTIFIER, "Expect parameter name.");
      typedDeclareVariable(paramType);
      uint8_t constant = identifierConstant(&parser.previous);
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
//< parameters
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  
  // Parse return type - expect a return type token
  ReturnType functionReturnType = IMMUTABLE_NONNULL_TYPE(TYPE_VOID.baseType);
  if (check(TOKEN_RETURNTYPE_INT) || check(TOKEN_RETURNTYPE_STRING) || 
      check(TOKEN_RETURNTYPE_BOOL) || check(TOKEN_RETURNTYPE_VOID) || 
      check(TOKEN_RETURNTYPE_FUNC) || check(TOKEN_RETURNTYPE_OBJ)) {
    // Store the return type in the function object
    TokenType typeToken = parser.current.type;
    advance(); // Consume the type token first
    functionReturnType = tokenToBaseType(typeToken);
    current->function->returnType = functionReturnType;
  } else {
    errorAtCurrent("Expect return type after function parameters.");
  }
  
  gemBlock();

  ObjFunction* function = endCompiler();
//> Closures emit-closure
  uint16_t constant = makeConstant(OBJ_VAL(function));
  emitByte(OP_CLOSURE);
  emitByte((constant >> 8) & 0xff);  // High byte
  emitByte(constant & 0xff);         // Low byte
//< Closures emit-closure
//> Closures capture-upvalues

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
//< Closures capture-upvalues
  
  return functionReturnType;
}
//< Calls and Functions compile-function
//> Methods and Initializers method
static void method() {
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  Token methodName = parser.previous;
  uint16_t constant = identifierConstant(&parser.previous);
//> method-body

//< method-body
/* Methods and Initializers method-body < Methods and Initializers method-type
  FunctionType type = TYPE_FUNCTION;
*/
//> method-type
  FunctionType type = TYPE_METHOD;
//< method-type
//> initializer-name
  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }
  
//< initializer-name
//> method-body
  ReturnType returnType = function(type);
  
  // Register the method with its return type for type inference
  if (currentClass != NULL) {
    ObjString* methodNameString = copyString(methodName.start, methodName.length);
    addMethod(currentClass->className, methodNameString, returnType);
  }
//< method-body
  emitByte(OP_METHOD);
  emitByte((constant >> 8) & 0xff);  // High byte
  emitByte(constant & 0xff);         // Low byte
}
//< Methods and Initializers method
//> Classes and Instances class-declaration
static void classDeclaration() {
  consume(TOKEN_IDENTIFIER, "Expect class name.");
//> Methods and Initializers class-name
  Token className = parser.previous;
//< Methods and Initializers class-name
  uint16_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  // Register this as a class for type inference
  if (current->scopeDepth == 0) {
    ObjString* classNameString = copyString(className.start, className.length);
    addClass(classNameString);
  }

  emitByte(OP_CLASS);
  emitByte((nameConstant >> 8) & 0xff);  // High byte
  emitByte(nameConstant & 0xff);         // Low byte
  defineVariable(nameConstant);

//> Methods and Initializers create-class-compiler
  ClassCompiler classCompiler;
//> Superclasses init-has-superclass
  classCompiler.hasSuperclass = false;
//< Superclasses init-has-superclass
  classCompiler.className = copyString(className.start, className.length);
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

//< Methods and Initializers create-class-compiler
//> Superclasses compile-superclass
  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);
//> inherit-self

    if (identifiersEqual(&className, &parser.previous)) {
      error("A class can't inherit from itself.");
    }

//< inherit-self
//> superclass-variable
    beginScope();
    addLocal(syntheticToken("super"), TYPE_VOID);
    defineVariable(0);
    
//< superclass-variable
    namedVariable(className, false);
    emitByte(OP_INHERIT);
//> set-has-superclass
    classCompiler.hasSuperclass = true;
//< set-has-superclass
  }
  
//< Superclasses compile-superclass
//> Methods and Initializers load-class
  namedVariable(className, false);
//< Methods and Initializers load-class
//> Methods and Initializers class-body
  while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
    if (match(TOKEN_DEF)) {
      method();
    } else {
      errorAtCurrent("Expect method definition in class body.");
    }
  }
//< Methods and Initializers class-body
  consume(TOKEN_END, "Expect 'end' after class body.");
//> Methods and Initializers pop-class
  emitByte(OP_POP);
//< Methods and Initializers pop-class
//> Superclasses end-superclass-scope

  if (classCompiler.hasSuperclass) {
    endScope();
  }
//< Superclasses end-superclass-scope
//> Methods and Initializers pop-enclosing

  currentClass = currentClass->enclosing;
//< Methods and Initializers pop-enclosing
}
//< Classes and Instances class-declaration
//> Module System module-declaration
static void moduleDeclaration() {
  consume(TOKEN_IDENTIFIER, "Expect module name.");
  Token moduleName = parser.previous;
  uint16_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  emitByte(OP_MODULE);
  emitByte((nameConstant >> 8) & 0xff);  // High byte
  emitByte(nameConstant & 0xff);         // Low byte
  defineVariable(nameConstant);

  // Load the module object onto the stack for method definitions
  namedVariable(moduleName, false);

  // Parse module body - store functions in the module
  while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
    if (match(TOKEN_DEF)) {
      consume(TOKEN_IDENTIFIER, "Expect function name.");
      Token functionName = parser.previous;
      uint16_t constant = identifierConstant(&parser.previous);

      ReturnType functionReturnType = function(TYPE_FUNCTION);
      
      // Register the function signature in the module function table
      ObjString* moduleNameString = copyString(moduleName.start, moduleName.length);
      ObjString* functionNameString = copyString(functionName.start, functionName.length);
      addModuleFunction(moduleNameString, functionNameString, functionReturnType);
      
      // Store the function in the module using OP_MODULE_METHOD
      emitByte(OP_MODULE_METHOD);
      emitByte((constant >> 8) & 0xff);  // High byte
      emitByte(constant & 0xff);         // Low byte
    } else {
      errorAtCurrent("Only function definitions are allowed in module bodies.");
    }
  }
  
  consume(TOKEN_END, "Expect 'end' after module body.");
  
  // Pop the module object from the stack
  emitByte(OP_POP);
}
//< Module System module-declaration

//> Global Variables var-declaration
static void varDeclaration() {
  uint16_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(global);
}
//< Global Variables var-declaration

//> Global Variables expression-statement
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}
//< Global Variables expression-statement

//> Jumping Back and Forth for-statement
static void forStatement() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  
  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else if (match(TOKEN_RETURNTYPE_INT) || match(TOKEN_RETURNTYPE_STRING) || match(TOKEN_RETURNTYPE_BOOL) ||
             match(TOKEN_RETURNTYPE_FUNC) || match(TOKEN_RETURNTYPE_OBJ)) {
    // Handle typed variable declarations in for loop: for (int i = 0; ...)
    typedVarDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Condition.
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  // Parse multiple statements until 'end'
  while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
    declaration();
  }
  
  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // Condition.
  }

  consume(TOKEN_END, "Expect 'end' after block.");
  endScope();
}
//< Jumping Back and Forth for-statement

//> Global Variables print-statement
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}
//< Global Variables print-statement

//> Module System require-statement
static void requireStatement() {
  // Parse the string literal after require
  if (!check(TOKEN_STRING)) {
    error("Expect string literal after 'require'.");
    return;
  }
  
  advance(); // consume the string token
  Token modulePathToken = parser.previous;
  
  // Extract the module path string (remove quotes)
  char* modulePath = malloc(modulePathToken.length - 1); // -2 for quotes, +1 for null terminator
  memcpy(modulePath, modulePathToken.start + 1, modulePathToken.length - 2);
  modulePath[modulePathToken.length - 2] = '\0';
  
  char* moduleSource = NULL;
  bool isEmbedded = false;
  
#ifdef WITH_STL
  // First, try to find the module in embedded STL
  const char* embeddedSource = getCompilerEmbeddedSTLModule(modulePath);
  if (embeddedSource != NULL) {
    // Use embedded STL module
    size_t sourceLen = strlen(embeddedSource);
    moduleSource = malloc(sourceLen + 1);
    if (moduleSource == NULL) {
      error("Not enough memory to load embedded module.");
      free(modulePath);
      return;
    }
    strcpy(moduleSource, embeddedSource);
    isEmbedded = true;
  }
#endif
  
  // If not found in embedded STL, try to read from file
  if (moduleSource == NULL) {
    // Try to load the module at compile time to extract signatures
    FILE* file = fopen(modulePath, "rb");
    
#ifdef WITH_STL
    // If file not found and we have STL support, try the standard library path
    if (file == NULL && STL_PATH != NULL) {
      // Check if the filename contains a path separator - if not, it might be a standard library module
      if (strchr(modulePath, '/') == NULL && strchr(modulePath, '\\') == NULL) {
        // Construct path: STL_PATH/filename.gem
        size_t stlPathLen = strlen(STL_PATH);
        size_t modulePathLen = strlen(modulePath);
        size_t fullPathLen = stlPathLen + 1 + modulePathLen + 4 + 1; // +1 for '/', +4 for '.gem', +1 for '\0'
        
        char* fullPath = malloc(fullPathLen);
        snprintf(fullPath, fullPathLen, "%s/%s.gem", STL_PATH, modulePath);
        file = fopen(fullPath, "rb");
        
        if (file != NULL) {
          free(modulePath);
          modulePath = fullPath;
        } else {
          free(fullPath);
        }
      }
    }
#endif
    
    if (file != NULL) {
      // Load and parse the module to extract function signatures
      fseek(file, 0L, SEEK_END);
      size_t fileSize = ftell(file);
      rewind(file);
      
      moduleSource = malloc(fileSize + 1);
      fread(moduleSource, sizeof(char), fileSize, file);
      moduleSource[fileSize] = '\0';
      fclose(file);
    }
  }
  
  if (moduleSource != NULL) {
    // Use a separate compilation to extract module signatures
    // This is a simplified parser that only looks for module function signatures
    const char* source = moduleSource;
    const char* current = source;
    
    // Simple lexer to find module declarations
    while (*current != '\0') {
      // Skip whitespace
      while (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r') {
        current++;
      }
      
      // Look for "module" keyword
      if (strncmp(current, "module", 6) == 0 && 
          (current[6] == ' ' || current[6] == '\t' || current[6] == '\n')) {
        current += 6;
        
        // Skip whitespace
        while (*current == ' ' || *current == '\t') current++;
        
        // Extract module name
        const char* moduleNameStart = current;
        while (*current && *current != ' ' && *current != '\t' && *current != '\n' && *current != '\r') {
          current++;
        }
        ObjString* moduleName = copyString(moduleNameStart, current - moduleNameStart);
        
        // Look for function definitions in this module
        while (*current != '\0' && strncmp(current, "end", 3) != 0) {
          // Skip whitespace and comments
          while (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r') {
            current++;
          }
          
          // Look for "def" keyword
          if (strncmp(current, "def", 3) == 0 && 
              (current[3] == ' ' || current[3] == '\t')) {
            current += 3;
            
            // Skip whitespace
            while (*current == ' ' || *current == '\t') current++;
            
            // Extract function name
            const char* funcNameStart = current;
            while (*current && *current != '(' && *current != ' ' && *current != '\t') {
              current++;
            }
            ObjString* functionName = copyString(funcNameStart, current - funcNameStart);
            
            // Skip to return type (simplified - assume int for now)
            // In a real implementation, we'd parse the full signature
            ReturnType returnType = TYPE_INT;
            
            // Register the function signature
            addModuleFunction(moduleName, functionName, returnType);
            
            // Skip to end of line or function body
            while (*current && *current != '\n') {
              current++;
            }
          } else {
            current++;
          }
        }
        break; // Found the module, we're done
      } else {
        current++;
      }
    }
    
    free(moduleSource);
  }
  
  free(modulePath);
  
  // Emit the string constant for runtime execution
  emitConstant(OBJ_VAL(copyString(modulePathToken.start + 1, modulePathToken.length - 2)));
  
  consume(TOKEN_SEMICOLON, "Expect ';' after require statement.");
  emitByte(OP_REQUIRE);
}
//< Module System require-statement

//> Calls and Functions return-statement
static void returnStatement() {
//> return-from-script
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

//< return-from-script
  if (match(TOKEN_SEMICOLON)) {
    // Empty return statement - should only be allowed for void functions
    if (!typesEqual(current->function->returnType, TYPE_VOID)) {
      error("Cannot return empty value from non-void function.");
    }
    emitReturn();
  } else {
//> Methods and Initializers return-from-init
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer.");
    }

//< Methods and Initializers return-from-init
    // Check that void functions don't return values
    if (typesEqual(current->function->returnType, TYPE_VOID)) {
      error("Cannot return a value from void function.");
    }
    
    expression();
    
    // Type check the returned expression
    ReturnType expressionType = inferExpressionType();
    if (!isAssignmentCompatible(current->function->returnType, expressionType) && 
        !typesEqual(expressionType, TYPE_VOID)) { // Allow conservative inference
      error("Return type mismatch: function expects different type.");
    }
    
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}
//< Calls and Functions return-statement

//> Return Type Helpers
static GemType tokenToBaseType(TokenType token) {
  BaseType baseType;
  switch (token) {
    case TOKEN_RETURNTYPE_INT: baseType = TYPE_INT.baseType; break;
    case TOKEN_RETURNTYPE_STRING: baseType = TYPE_STRING.baseType; break;
    case TOKEN_RETURNTYPE_BOOL: baseType = TYPE_BOOL.baseType; break;
    case TOKEN_RETURNTYPE_VOID: baseType = TYPE_VOID.baseType; break;
    case TOKEN_RETURNTYPE_FUNC: baseType = TYPE_FUNC.baseType; break;
    case TOKEN_RETURNTYPE_OBJ: baseType = TYPE_OBJ.baseType; break;
    default: baseType = TYPE_VOID.baseType; break; // Should never happen
  }
  
  // Start with immutable, non-nullable type
  GemType type = IMMUTABLE_NONNULL_TYPE(baseType);
  
  // Check for suffixes
  if (check(TOKEN_QUESTION_BANG)) {
    advance(); // consume '?!'
    type.isNullable = true;
    type.isMutable = true;
  } else if (check(TOKEN_QUESTION)) {
    advance(); // consume '?'
    type.isNullable = true;
    
    // Check for '!' after '?' to get '?!'
    if (check(TOKEN_BANG)) {
      advance(); // consume '!'
      type.isMutable = true;
    }
  } else if (check(TOKEN_BANG)) {
    advance(); // consume '!'
    type.isMutable = true;
  }
  
  return type;
}

// Legacy function for compatibility
static ReturnType tokenToReturnType(TokenType token) {
  return tokenToBaseType(token);
}

static ReturnType inferExpressionType() {
  // If we have a tracked expression type from operations, use it
  if (lastExpressionType.baseType != RETURN_TYPE_VOID) {
    return lastExpressionType;
  }
  
  // Fallback to token-based inference for simple literals
  BaseType baseType;
  switch (parser.previous.type) {
    case TOKEN_NUMBER: 
      baseType = TYPE_INT.baseType;
      break;
    case TOKEN_STRING: 
      baseType = TYPE_STRING.baseType;
      break;
    case TOKEN_TRUE:
    case TOKEN_FALSE: 
      baseType = TYPE_BOOL.baseType;
      break;
    case TOKEN_NIL: 
      baseType = TYPE_VOID.baseType;
      return NULLABLE_IMMUTABLE_TYPE(baseType); // nil is nullable
    default: 
      baseType = TYPE_VOID.baseType; // Conservative default
      break;
  }
  return IMMUTABLE_NONNULL_TYPE(baseType);
}
//< Return Type Helpers

//> Global Variable Type Functions
static void initGlobalVarTable() {
  globalVars.count = 0;
}

static void addGlobalVar(ObjString* name, ReturnType type) {
  if (globalVars.count >= UINT8_COUNT) {
    error("Too many global variables.");
    return;
  }
  
  globalVars.globals[globalVars.count].name = name;
  globalVars.globals[globalVars.count].type = type;
  globalVars.count++;
}

static ReturnType getGlobalVarType(ObjString* name) {
  for (int i = 0; i < globalVars.count; i++) {
    if (globalVars.globals[i].name->length == name->length &&
        memcmp(AS_CSTRING(OBJ_VAL(globalVars.globals[i].name)), AS_CSTRING(OBJ_VAL(name)), name->length) == 0) {
      return globalVars.globals[i].type;
    }
  }
  return TYPE_VOID; // Not found or untyped
}

static void setGlobalVarType(ObjString* name, ReturnType type) {
  for (int i = 0; i < globalVars.count; i++) {
    if (globalVars.globals[i].name->length == name->length &&
        memcmp(AS_CSTRING(OBJ_VAL(globalVars.globals[i].name)), AS_CSTRING(OBJ_VAL(name)), name->length) == 0) {
      globalVars.globals[i].type = type;
      return;
    }
  }
  // If not found, add it
  addGlobalVar(name, type);
}

//> Class Field Table Functions
static void initClassFieldTable() {
  classFields.classCount = 0;
}

static void addClassField(ObjString* className, ObjString* fieldName, ReturnType fieldType) {
  // Find or create class field signature
  ClassFieldSignature* classSignature = NULL;
  
  for (int i = 0; i < classFields.classCount; i++) {
    if (classFields.classes[i].className->length == className->length &&
        memcmp(AS_CSTRING(OBJ_VAL(classFields.classes[i].className)), AS_CSTRING(OBJ_VAL(className)), className->length) == 0) {
      classSignature = &classFields.classes[i];
      break;
    }
  }
  
  if (classSignature == NULL) {
    // Create new class field signature
    if (classFields.classCount >= UINT8_COUNT) {
      error("Too many classes.");
      return;
    }
    classSignature = &classFields.classes[classFields.classCount++];
    classSignature->className = className;
    classSignature->fieldCount = 0;
  }
  
  // Check if field already exists, if so update its type
  for (int i = 0; i < classSignature->fieldCount; i++) {
    if (classSignature->fields[i].fieldName->length == fieldName->length &&
        memcmp(AS_CSTRING(OBJ_VAL(classSignature->fields[i].fieldName)), AS_CSTRING(OBJ_VAL(fieldName)), fieldName->length) == 0) {
      classSignature->fields[i].fieldType = fieldType;
      return;
    }
  }
  
  // Add new field signature
  if (classSignature->fieldCount >= UINT8_COUNT) {
    error("Too many fields in class.");
    return;
  }
  
  FieldSignature* field = &classSignature->fields[classSignature->fieldCount++];
  field->fieldName = fieldName;
  field->fieldType = fieldType;
}

static ReturnType getClassFieldType(ObjString* className, ObjString* fieldName) {
  // Look in the specified class
  for (int i = 0; i < classFields.classCount; i++) {
    if (classFields.classes[i].className->length == className->length &&
        memcmp(AS_CSTRING(OBJ_VAL(classFields.classes[i].className)), AS_CSTRING(OBJ_VAL(className)), className->length) == 0) {
      ClassFieldSignature* classSignature = &classFields.classes[i];
      
      for (int j = 0; j < classSignature->fieldCount; j++) {
        if (classSignature->fields[j].fieldName->length == fieldName->length &&
            memcmp(AS_CSTRING(OBJ_VAL(classSignature->fields[j].fieldName)), AS_CSTRING(OBJ_VAL(fieldName)), fieldName->length) == 0) {
          return classSignature->fields[j].fieldType;
        }
      }
      break;
    }
  }
  
  return TYPE_VOID; // Not found - conservative default
}
//< Class Field Table Functions
//< Global Variable Type Functions

//> Function and Class Tracking Functions
static void initFunctionTable() {
  globalFunctions.count = 0;
}

static void addFunction(ObjString* name, ReturnType returnType) {
  if (globalFunctions.count >= UINT8_COUNT) {
    error("Too many global functions.");
    return;
  }
  
  globalFunctions.functions[globalFunctions.count].functionName = name;
  globalFunctions.functions[globalFunctions.count].returnType = returnType;
  globalFunctions.count++;
}

static ReturnType getFunctionReturnType(ObjString* name) {
  for (int i = 0; i < globalFunctions.count; i++) {
    if (globalFunctions.functions[i].functionName->length == name->length &&
        memcmp(AS_CSTRING(OBJ_VAL(globalFunctions.functions[i].functionName)), AS_CSTRING(OBJ_VAL(name)), name->length) == 0) {
      return globalFunctions.functions[i].returnType;
    }
  }
  return TYPE_VOID; // Not found - conservative default
}

static void initClassTable() {
  globalClasses.count = 0;
}

static void addClass(ObjString* name) {
  if (globalClasses.count >= UINT8_COUNT) {
    error("Too many global classes.");
    return;
  }
  
  globalClasses.classes[globalClasses.count].className = name;
  globalClasses.count++;
}

static bool isClass(ObjString* name) {
  for (int i = 0; i < globalClasses.count; i++) {
    if (globalClasses.classes[i].className->length == name->length &&
        memcmp(AS_CSTRING(OBJ_VAL(globalClasses.classes[i].className)), AS_CSTRING(OBJ_VAL(name)), name->length) == 0) {
      return true;
    }
  }
  return false;
}

static void initMethodTable() {
  globalMethods.count = 0;
}

static void addMethod(ObjString* className, ObjString* methodName, ReturnType returnType) {
  if (globalMethods.count >= UINT8_COUNT) {
    error("Too many global methods.");
    return;
  }
  
  globalMethods.methods[globalMethods.count].className = className;
  globalMethods.methods[globalMethods.count].methodName = methodName;
  globalMethods.methods[globalMethods.count].returnType = returnType;
  globalMethods.count++;
}

static ReturnType getMethodReturnType(ObjString* className, ObjString* methodName) {
  for (int i = 0; i < globalMethods.count; i++) {
    if (globalMethods.methods[i].className->length == className->length &&
        memcmp(AS_CSTRING(OBJ_VAL(globalMethods.methods[i].className)), AS_CSTRING(OBJ_VAL(className)), className->length) == 0 &&
        globalMethods.methods[i].methodName->length == methodName->length &&
        memcmp(AS_CSTRING(OBJ_VAL(globalMethods.methods[i].methodName)), AS_CSTRING(OBJ_VAL(methodName)), methodName->length) == 0) {
      return globalMethods.methods[i].returnType;
    }
  }
  return TYPE_VOID; // Not found - conservative default
}
//< Function and Class Tracking Functions

//> Module Function Tracking Functions
static void initModuleFunctionTable() {
  moduleFunctions.count = 0;
}

static void addModuleFunction(ObjString* moduleName, ObjString* functionName, ReturnType returnType) {
  if (moduleFunctions.count >= UINT8_COUNT) {
    error("Too many module functions.");
    return;
  }
  
  moduleFunctions.functions[moduleFunctions.count].moduleName = moduleName;
  moduleFunctions.functions[moduleFunctions.count].functionName = functionName;
  moduleFunctions.functions[moduleFunctions.count].returnType = returnType;
  moduleFunctions.count++;
}

static ReturnType getModuleFunctionReturnType(ObjString* moduleName, ObjString* functionName) {
  for (int i = 0; i < moduleFunctions.count; i++) {
    if (moduleFunctions.functions[i].moduleName->length == moduleName->length &&
        memcmp(AS_CSTRING(OBJ_VAL(moduleFunctions.functions[i].moduleName)), AS_CSTRING(OBJ_VAL(moduleName)), moduleName->length) == 0 &&
        moduleFunctions.functions[i].functionName->length == functionName->length &&
        memcmp(AS_CSTRING(OBJ_VAL(moduleFunctions.functions[i].functionName)), AS_CSTRING(OBJ_VAL(functionName)), functionName->length) == 0) {
      return moduleFunctions.functions[i].returnType;
    }
  }
  return TYPE_VOID; // Not found - conservative default
}
//< Module Function Tracking Functions

// New function for typed variable declarations
static void typedVarDeclaration() {
  // The type token has already been consumed by the caller
  // We need to reconstruct the type and check for suffixes
  ReturnType declaredType;
  BaseType baseType;
  
  switch (parser.previous.type) {
    case TOKEN_RETURNTYPE_INT: baseType = TYPE_INT.baseType; break;
    case TOKEN_RETURNTYPE_STRING: baseType = TYPE_STRING.baseType; break;
    case TOKEN_RETURNTYPE_BOOL: baseType = TYPE_BOOL.baseType; break;
    case TOKEN_RETURNTYPE_FUNC: baseType = TYPE_FUNC.baseType; break;
    case TOKEN_RETURNTYPE_OBJ: baseType = TYPE_OBJ.baseType; break;
    default: 
      error("Unknown type in variable declaration.");
      return;
  }
  
  // Start with immutable, non-nullable type
  declaredType = IMMUTABLE_NONNULL_TYPE(baseType);
  
  // Check for suffixes 
  if (check(TOKEN_QUESTION_BANG)) {
    advance(); // consume '?!'
    declaredType.isNullable = true;
    declaredType.isMutable = true;
  } else if (check(TOKEN_QUESTION)) {
    advance(); // consume '?'
    declaredType.isNullable = true;
    
    // Check for '!' after '?' to get '?!'
    if (check(TOKEN_BANG)) {
      advance(); // consume '!'
      declaredType.isMutable = true;
    }
  } else if (check(TOKEN_BANG)) {
    advance(); // consume '!'
    declaredType.isMutable = true;
  }
  
  consume(TOKEN_IDENTIFIER, "Expect variable name.");
  Token variableName = parser.previous; // Store the variable name token
  typedDeclareVariable(declaredType);
  uint16_t global = 0;
  if (current->scopeDepth == 0) {
    global = identifierConstant(&variableName);
    // Track the type for global variables
    ObjString* name = copyString(variableName.start, variableName.length);
    addGlobalVar(name, declaredType);
  }

  if (match(TOKEN_EQUAL)) {
    expression();
    // Capture the expression type immediately before anything can reset it
    ReturnType expressionType = lastExpressionType;
    
    // Type checking: verify the expression type matches the declared type
    if (!isAssignmentCompatible(declaredType, expressionType)) {
      error("Cannot assign value of incompatible type to variable.");
    }
    
    // For obj variables, if we're assigning a class-specific type, update the variable's type
    if (declaredType.baseType == RETURN_TYPE_OBJ && declaredType.className == NULL &&
        expressionType.baseType == RETURN_TYPE_OBJ && expressionType.className != NULL) {
      // Update the variable to have the specific class type, but preserve mutability and nullability
      ReturnType updatedType = expressionType;
      updatedType.isMutable = declaredType.isMutable;    // Preserve original mutability
      updatedType.isNullable = declaredType.isNullable; // Preserve original nullability
      declaredType = updatedType;
      
      // Update the local variable type if it's a local variable
      if (current->scopeDepth > 0 && current->localCount > 0) {
        current->locals[current->localCount - 1].type = declaredType;
      }
      
      // Update global variable type if it's global
      if (current->scopeDepth == 0) {
        ObjString* name = copyString(variableName.start, variableName.length);
        setGlobalVarType(name, declaredType);
      }
    }
  } else {
    // For nullable variables, default to nil
    if (declaredType.isNullable) {
      emitByte(OP_NIL);
    } else {
      error("Non-nullable variables must be initialized.");
    }
  }
  consume(TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(global);
}

//> Local Variables gem-block
static void gemBlock() {
  while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
    declaration();
  }
  
  consume(TOKEN_END, "Expect 'end' after block.");
}
//< Local Variables gem-block

//> Control Flow Statements
static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after if condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  
  // Parse the if body - check for else/elsif before consuming end
  while (!check(TOKEN_ELSE) && !check(TOKEN_ELSIF) && !check(TOKEN_END) && !check(TOKEN_EOF)) {
    declaration();
  }

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE)) {
    // Parse the else body
    while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
      declaration();
    }
  } else if (match(TOKEN_ELSIF)) {
    ifStatement();
    return; // Don't consume end here, let the recursive call handle it
  }
  
  consume(TOKEN_END, "Expect 'end' after if statement.");
  patchJump(elseJump);
}

static void whileStatement() {
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  gemBlock();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
}

static void beginStatement() {
  gemBlock();
}

static void defDeclaration() {
  consume(TOKEN_IDENTIFIER, "Expect function name.");
  Token functionName = parser.previous;
  uint16_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  ReturnType functionReturnType = function(TYPE_FUNCTION);
  
  // Register the function with its return type for type inference
  if (current->scopeDepth == 0) {
    ObjString* nameString = copyString(functionName.start, functionName.length);
    addFunction(nameString, functionReturnType);
  }

  defineVariable(nameConstant);
}
//< Control Flow Statements

//> Statements
static void statement() {
  if (match(TOKEN_PUTS)) {
    printStatement();
  } else if (match(TOKEN_REQUIRE)) {
    requireStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_BEGIN)) {
    beginStatement();
  } else {
    expressionStatement();
  }
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_MODULE:
      case TOKEN_DEF:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_RETURNTYPE_INT:
      case TOKEN_RETURNTYPE_STRING:
      case TOKEN_RETURNTYPE_BOOL:
      case TOKEN_RETURNTYPE_FUNC:
      case TOKEN_RETURNTYPE_OBJ:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PUTS:
      case TOKEN_RETURN:
      case TOKEN_BEGIN:
        return;

      default:
        ; // Do nothing.
    }

    advance();
  }
}

static void declaration() {
  if (match(TOKEN_CLASS)) {
    classDeclaration();
  } else if (match(TOKEN_MODULE)) {
    moduleDeclaration();
  } else if (match(TOKEN_DEF)) {
    defDeclaration();
  } else if (match(TOKEN_FUN)) {
    defDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else if (match(TOKEN_RETURNTYPE_INT) || match(TOKEN_RETURNTYPE_STRING) || 
             match(TOKEN_RETURNTYPE_BOOL) || match(TOKEN_RETURNTYPE_FUNC) ||
             match(TOKEN_RETURNTYPE_OBJ)) {
    typedVarDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode) synchronize();
}
//< Statements

//> Compilation Functions
void initCompilerTables() {
  initGlobalVarTable();
  initClassFieldTable();
  initFunctionTable();
  initClassTable();
  initMethodTable();
  initModuleFunctionTable();
}

ObjFunction* compile(const char* source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
}
//< Compilation Functions

//> Embedded STL Modules for Compiler
#ifdef WITH_STL
#include "embedded_stl.h"

// Function to get embedded STL module source for compiler
static const char* getCompilerEmbeddedSTLModule(const char* moduleName) {
    return getEmbeddedSTLModule(moduleName);
}
#endif
//< Embedded STL Modules for Compiler