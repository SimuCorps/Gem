#ifndef gem_gcc_compiler_h
#define gem_gcc_compiler_h

#include "common.h"
#include "object.h"

typedef enum {
  GCC_COMPILE_OK,
  GCC_COMPILE_ERROR,
  GCC_COMPILE_LINK_ERROR
} GCCCompileResult;

// Initialize the GCC compiler
void initGCCCompiler();

// Free the GCC compiler resources
void freeGCCCompiler();

// Compile bytecode to native executable and run it
GCCCompileResult gccCompileAndRunBytecode(ObjFunction* function, const char* outputPath);

// Compile bytecode to native executable without running it
GCCCompileResult gccCompileBytecodeToExecutable(ObjFunction* function, const char* outputPath);

#endif 