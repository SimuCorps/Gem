//> Chunks of Bytecode debug-h
#ifndef gem_debug_h
#define gem_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif
