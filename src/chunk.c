//> Chunks of Bytecode chunk-c
#include <stdlib.h>

#include "chunk.h"
//> chunk-c-include-memory
#include "memory.h"
//< chunk-c-include-memory
#include "vm.h"

void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
//> chunk-null-lines
  chunk->lineData = NULL;
  chunk->lineCount = 0;
  chunk->lineCapacity = 0;
//< chunk-null-lines
//> chunk-init-constant-array
  initValueArray(&chunk->constants);
//< chunk-init-constant-array
}
//> free-chunk
void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
//> chunk-free-lines
  FREE_ARRAY(int, chunk->lineData, chunk->lineCapacity);
//< chunk-free-lines
//> chunk-free-constants
  freeValueArray(&chunk->constants);
//< chunk-free-constants
  initChunk(chunk);
}
//< free-chunk
/* Chunks of Bytecode write-chunk < Chunks of Bytecode write-chunk-with-line
void writeChunk(Chunk* chunk, uint8_t byte) {
*/
//> write-chunk
//> write-chunk-with-line
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
//< write-chunk-with-line
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code,
        oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  
//> chunk-write-line
  // Handle run-length encoded line information
  if (chunk->lineCount == 0 || 
      chunk->lineData[chunk->lineCount * 2 - 1] != line) {
    // Need to add a new (count, line) pair
    if (chunk->lineCapacity < (chunk->lineCount + 1) * 2) {
      int oldCapacity = chunk->lineCapacity;
      chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
      if (chunk->lineCapacity < 8) chunk->lineCapacity = 8;
      chunk->lineData = GROW_ARRAY(int, chunk->lineData,
          oldCapacity, chunk->lineCapacity);
    }
    
    chunk->lineData[chunk->lineCount * 2] = 1;       // count
    chunk->lineData[chunk->lineCount * 2 + 1] = line; // line number
    chunk->lineCount++;
  } else {
    // Increment the count for the current line
    chunk->lineData[(chunk->lineCount - 1) * 2]++;
  }
//< chunk-write-line
  chunk->count++;
}
//< write-chunk
//> add-constant
int addConstant(Chunk* chunk, Value value) {
  push(value);
  writeValueArray(&chunk->constants, value);
  pop();
  return chunk->constants.count - 1;
}
//< add-constant

void writeConstant(Chunk* chunk, Value value, int line) {
  int constant = addConstant(chunk, value);
  
  if (constant < 256) {
    // Use single-byte OP_CONSTANT for indices 0-255
    writeChunk(chunk, OP_CONSTANT, line);
    writeChunk(chunk, (uint8_t)constant, line);
  } else if (constant <= 0xFFFFFF) {
    // Use 24-bit OP_CONSTANT_LONG for indices 256-16777215
    writeChunk(chunk, OP_CONSTANT_LONG, line);
    writeChunk(chunk, (constant >> 16) & 0xff, line);  // High byte
    writeChunk(chunk, (constant >> 8) & 0xff, line);   // Middle byte
    writeChunk(chunk, constant & 0xff, line);          // Low byte
  } else {
    // This should never happen in practice, but handle it gracefully
    // We could emit an error or fall back to multiple constants
    // For now, just use the long format with truncated index
    writeChunk(chunk, OP_CONSTANT_LONG, line);
    writeChunk(chunk, (constant >> 16) & 0xff, line);
    writeChunk(chunk, (constant >> 8) & 0xff, line);
    writeChunk(chunk, constant & 0xff, line);
  }
}
//> get-line
int getLine(Chunk* chunk, int instruction) {
  if (instruction < 0 || instruction >= chunk->count) {
    return -1; // Invalid instruction index
  }
  
  int currentInstruction = 0;
  
  // Walk through the run-length encoded line data
  for (int i = 0; i < chunk->lineCount; i++) {
    int count = chunk->lineData[i * 2];
    int line = chunk->lineData[i * 2 + 1];
    
    if (currentInstruction + count > instruction) {
      // The instruction is within this run
      return line;
    }
    
    currentInstruction += count;
  }
  
  return -1; // Should never reach here if data is consistent
}
//< get-line
