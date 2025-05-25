//> Chunks of Bytecode chunk-h
#ifndef gem_chunk_h
#define gem_chunk_h

#include "common.h"
//> chunk-h-include-value
#include "value.h"
//< chunk-h-include-value
//> op-enum

typedef enum {
//> op-constant
  OP_CONSTANT,
  OP_CONSTANT_LONG,
//< op-constant
//> Types of Values literal-ops
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
//< Types of Values literal-ops
//> Global Variables pop-op
  OP_POP,
//< Global Variables pop-op
//> Local Variables get-local-op
  OP_GET_LOCAL,
//< Local Variables get-local-op
//> Local Variables set-local-op
  OP_SET_LOCAL,
//< Local Variables set-local-op
//> Global Variables get-global-op
  OP_GET_GLOBAL,
//< Global Variables get-global-op
//> Global Variables define-global-op
  OP_DEFINE_GLOBAL,
//< Global Variables define-global-op
//> Global Variables set-global-op
  OP_SET_GLOBAL,
//< Global Variables set-global-op
//> Closures upvalue-ops
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
//< Closures upvalue-ops
//> Classes and Instances property-ops
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
//< Classes and Instances property-ops
//> Superclasses get-super-op
  OP_GET_SUPER,
//< Superclasses get-super-op
//> Types of Values comparison-ops
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
//< Types of Values comparison-ops
//> A Virtual Machine binary-ops
  OP_ADD,
  OP_ADD_NUMBER,      // Optimized numeric addition (no type check needed)
  OP_ADD_STRING,      // Optimized string concatenation (no type check needed)
  OP_SUBTRACT,
  OP_SUBTRACT_NUMBER, // Optimized numeric subtraction (no type check needed)
  OP_MULTIPLY,
  OP_MULTIPLY_NUMBER, // Optimized numeric multiplication (no type check needed)
  OP_DIVIDE,
  OP_DIVIDE_NUMBER,   // Optimized numeric division (no type check needed)
  OP_MODULO,
  OP_MODULO_NUMBER,   // Optimized numeric modulo (no type check needed)
//> Types of Values not-op
  OP_NOT,
//< Types of Values not-op
//< A Virtual Machine binary-ops
//> A Virtual Machine negate-op
  OP_NEGATE,
  OP_NEGATE_NUMBER,   // Optimized numeric negation (no type check needed)
//< A Virtual Machine negate-op
//> String interpolation op
  OP_INTERPOLATE,
//< String interpolation op
//> Global Variables op-print
  OP_PRINT,
//< Global Variables op-print
//> Module System op-require
  OP_REQUIRE,
//< Module System op-require
//> Jumping Back and Forth jump-op
  OP_JUMP,
//< Jumping Back and Forth jump-op
//> Jumping Back and Forth jump-if-false-op
  OP_JUMP_IF_FALSE,
//< Jumping Back and Forth jump-if-false-op
//> Jumping Back and Forth loop-op
  OP_LOOP,
//< Jumping Back and Forth loop-op
//> Calls and Functions op-call
  OP_CALL,
//< Calls and Functions op-call
//> Methods and Initializers invoke-op
  OP_INVOKE,
//< Methods and Initializers invoke-op
//> Module System module-call-op
  OP_MODULE_CALL,
//< Module System module-call-op
//> Superclasses super-invoke-op
  OP_SUPER_INVOKE,
//< Superclasses super-invoke-op
//> Closures closure-op
  OP_CLOSURE,
//< Closures closure-op
//> Closures close-upvalue-op
  OP_CLOSE_UPVALUE,
//< Closures close-upvalue-op
  OP_RETURN,
//> Classes and Instances class-op
  OP_CLASS,
//< Classes and Instances class-op
//> Module System module-op
  OP_MODULE,
//< Module System module-op
//> Module System module-method-op
  OP_MODULE_METHOD,
//< Module System module-method-op
//> Superclasses inherit-op
  OP_INHERIT,
//< Superclasses inherit-op
//> Methods and Initializers method-op
  OP_METHOD
//< Methods and Initializers method-op
} OpCode;
//< op-enum
//> chunk-struct

typedef struct {
//> count-and-capacity
  int count;
  int capacity;
//< count-and-capacity
  uint8_t* code;
//> chunk-lines
  // Run-length encoded line information
  int* lineData;      // Array storing (count, line) pairs
  int lineCount;      // Number of pairs in lineData
  int lineCapacity;   // Capacity of lineData array
//< chunk-lines
//> chunk-constants
  ValueArray constants;
//< chunk-constants
} Chunk;
//< chunk-struct
//> init-chunk-h

void initChunk(Chunk* chunk);
//< init-chunk-h
//> free-chunk-h
void freeChunk(Chunk* chunk);
//< free-chunk-h
/* Chunks of Bytecode write-chunk-h < Chunks of Bytecode write-chunk-with-line-h
void writeChunk(Chunk* chunk, uint8_t byte);
*/
//> write-chunk-with-line-h
void writeChunk(Chunk* chunk, uint8_t byte, int line);
//< write-chunk-with-line-h
//> add-constant-h
int addConstant(Chunk* chunk, Value value);
void writeConstant(Chunk* chunk, Value value, int line);
//< add-constant-h
//> get-line-h
int getLine(Chunk* chunk, int instruction);
//< get-line-h

#endif
