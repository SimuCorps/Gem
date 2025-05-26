#ifndef gem_llvm_compiler_h
#define gem_llvm_compiler_h

#include "common.h"
#include "object.h"
#include "chunk.h"

#ifdef WITH_LLVM

// LLVM compilation result
typedef enum {
    LLVM_COMPILE_OK,
    LLVM_COMPILE_ERROR,
    LLVM_COMPILE_LINK_ERROR
} LLVMCompileResult;

// Initialize LLVM backend
void initLLVMCompiler();

// Cleanup LLVM backend
void freeLLVMCompiler();

// Compile bytecode to native executable and run it
LLVMCompileResult compileAndRunBytecode(ObjFunction* function, const char* outputPath);

// Compile bytecode to LLVM IR (for debugging)
LLVMCompileResult compileBytecodeToIR(ObjFunction* function, const char* outputPath);

#else

// Stub implementations when LLVM is not available
typedef enum {
    LLVM_COMPILE_OK,
    LLVM_COMPILE_ERROR,
    LLVM_COMPILE_LINK_ERROR
} LLVMCompileResult;

static inline void initLLVMCompiler() {
    fprintf(stderr, "LLVM support not available - interpreter was built without LLVM\n");
}

static inline void freeLLVMCompiler() {
    // No-op
}

static inline LLVMCompileResult compileAndRunBytecode(ObjFunction* function, const char* outputPath) {
    (void)function; (void)outputPath;
    fprintf(stderr, "LLVM compilation not available - interpreter was built without LLVM\n");
    return LLVM_COMPILE_ERROR;
}

static inline LLVMCompileResult compileBytecodeToIR(ObjFunction* function, const char* outputPath) {
    (void)function; (void)outputPath;
    fprintf(stderr, "LLVM compilation not available - interpreter was built without LLVM\n");
    return LLVM_COMPILE_ERROR;
}

#endif // WITH_LLVM

#endif 