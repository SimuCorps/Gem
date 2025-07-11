//> Scanning on Demand compiler-h
#ifndef gem_compiler_h
#define gem_compiler_h

//> Strings compiler-include-object
#include "object.h"
//< Strings compiler-include-object
//> Compiling Expressions compile-h
#include "vm.h"

//< Compiling Expressions compile-h
/* Scanning on Demand compiler-h < Compiling Expressions compile-h
void compile(const char* source);
*/
/* Compiling Expressions compile-h < Calls and Functions compile-h
bool compile(const char* source, Chunk* chunk);
*/
//> Calls and Functions compile-h
ObjFunction* compile(const char* source);
//< Calls and Functions compile-h
//> Global Variable Table Initialization
void initCompilerTables();
//< Global Variable Table Initialization

#endif
