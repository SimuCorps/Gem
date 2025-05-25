//> Chunks of Bytecode debug-c
#include <stdio.h>

#include "debug.h"
//> Closures debug-include-object
#include "object.h"
//< Closures debug-include-object
//> debug-include-value
#include "value.h"
//< debug-include-value

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);
  
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}
//> constant-instruction
static int constantInstruction(const char* name, Chunk* chunk,
                               int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int constantLongInstruction(const char* name, Chunk* chunk,
                                   int offset) {
  uint32_t constant = (chunk->code[offset + 1] << 16) |
                     (chunk->code[offset + 2] << 8) |
                     chunk->code[offset + 3];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 4;
}
//< constant-instruction
//> Methods and Initializers invoke-instruction
static int invokeInstruction(const char* name, Chunk* chunk,
                                int offset) {
  uint16_t constant = (uint16_t)(chunk->code[offset + 1] << 8);
  constant |= chunk->code[offset + 2];
  uint8_t argCount = chunk->code[offset + 3];
  printf("%-16s (%d args) %4d '", name, argCount, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 4;
}
//< Methods and Initializers invoke-instruction
//> simple-instruction
static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}
//< simple-instruction
//> Local Variables byte-instruction
static int byteInstruction(const char* name, Chunk* chunk,
                           int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2; // [debug]
}
//< Local Variables byte-instruction
//> Jumping Back and Forth jump-instruction
static int jumpInstruction(const char* name, int sign,
                           Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset,
         offset + 3 + sign * jump);
  return offset + 3;
}
//< Jumping Back and Forth jump-instruction
//> disassemble-instruction
int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
//> show-location
  if (offset > 0 &&
      getLine(chunk, offset) == getLine(chunk, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", getLine(chunk, offset));
  }
//< show-location
  
  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
//> disassemble-constant
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG:
      return constantLongInstruction("OP_CONSTANT_LONG", chunk, offset);
//< disassemble-constant
//> Types of Values disassemble-literals
    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
//< Types of Values disassemble-literals
//> Global Variables disassemble-pop
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
//< Global Variables disassemble-pop
//> Local Variables disassemble-local
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
//< Local Variables disassemble-local
//> Global Variables disassemble-get-global
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
//< Global Variables disassemble-get-global
//> Global Variables disassemble-define-global
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk,
                                 offset);
//< Global Variables disassemble-define-global
//> Global Variables disassemble-set-global
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
//< Global Variables disassemble-set-global
//> Closures disassemble-upvalue-ops
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
//< Closures disassemble-upvalue-ops
//> Classes and Instances disassemble-property-ops
    case OP_GET_PROPERTY:
      return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("OP_SET_PROPERTY", chunk, offset);
//< Classes and Instances disassemble-property-ops
//> Superclasses disassemble-get-super
    case OP_GET_SUPER:
      return constantInstruction("OP_GET_SUPER", chunk, offset);
//< Superclasses disassemble-get-super
//> Types of Values disassemble-comparison
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
//< Types of Values disassemble-comparison
//> A Virtual Machine disassemble-binary
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_ADD_NUMBER:
      return simpleInstruction("OP_ADD_NUMBER", offset);
    case OP_ADD_STRING:
      return simpleInstruction("OP_ADD_STRING", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_SUBTRACT_NUMBER:
      return simpleInstruction("OP_SUBTRACT_NUMBER", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_MULTIPLY_NUMBER:
      return simpleInstruction("OP_MULTIPLY_NUMBER", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_DIVIDE_NUMBER:
      return simpleInstruction("OP_DIVIDE_NUMBER", offset);
    case OP_MODULO:
      return simpleInstruction("OP_MODULO", offset);
    case OP_MODULO_NUMBER:
      return simpleInstruction("OP_MODULO_NUMBER", offset);
//> Types of Values disassemble-not
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
//< Types of Values disassemble-not
//< A Virtual Machine disassemble-binary
//> A Virtual Machine disassemble-negate
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_NEGATE_NUMBER:
      return simpleInstruction("OP_NEGATE_NUMBER", offset);
//< A Virtual Machine disassemble-negate
//> Global Variables disassemble-print
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);
//< Global Variables disassemble-print
//> Module System disassemble-require
    case OP_REQUIRE:
      return simpleInstruction("OP_REQUIRE", offset);
//< Module System disassemble-require
//> Jumping Back and Forth disassemble-jump
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
//< Jumping Back and Forth disassemble-jump
//> Jumping Back and Forth disassemble-loop
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
//< Jumping Back and Forth disassemble-loop
//> Calls and Functions disassemble-call
    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
//< Calls and Functions disassemble-call
//> Methods and Initializers disassemble-invoke
    case OP_INVOKE:
      return invokeInstruction("OP_INVOKE", chunk, offset);
//< Methods and Initializers disassemble-invoke
//> Superclasses disassemble-super-invoke
    case OP_SUPER_INVOKE:
      return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
//< Superclasses disassemble-super-invoke
//> Closures disassemble-closure
    case OP_CLOSURE: {
      offset++;
      uint16_t constant = (uint16_t)(chunk->code[offset] << 8);
      constant |= chunk->code[offset + 1];
      offset += 2;
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");
//> disassemble-upvalues

      ObjFunction* function = AS_FUNCTION(
          chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n",
               offset - 2, isLocal ? "local" : "upvalue", index);
      }
      
//< disassemble-upvalues
      return offset;
    }
//< Closures disassemble-closure
//> Closures disassemble-close-upvalue
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
//< Closures disassemble-close-upvalue
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
//> Classes and Instances disassemble-class
    case OP_CLASS:
      return constantInstruction("OP_CLASS", chunk, offset);
//< Classes and Instances disassemble-class
//> Module System disassemble-module
    case OP_MODULE:
      return constantInstruction("OP_MODULE", chunk, offset);
//< Module System disassemble-module
//> Module System disassemble-module-method
    case OP_MODULE_METHOD:
      return constantInstruction("OP_MODULE_METHOD", chunk, offset);
//< Module System disassemble-module-method
//> Module System disassemble-module-call
    case OP_MODULE_CALL:
      return invokeInstruction("OP_MODULE_CALL", chunk, offset);
//< Module System disassemble-module-call
//> Superclasses disassemble-inherit
    case OP_INHERIT:
      return simpleInstruction("OP_INHERIT", offset);
//< Superclasses disassemble-inherit
//> Methods and Initializers disassemble-method
    case OP_METHOD:
      return constantInstruction("OP_METHOD", chunk, offset);
//< Methods and Initializers disassemble-method
    case OP_INTERPOLATE:
      return byteInstruction("OP_INTERPOLATE", chunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
//< disassemble-instruction
