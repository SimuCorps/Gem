#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "jit.h"
#include "memory.h"
#include "debug.h"
#include "vm.h"

// Global JIT context
JitContext jitContext = {0};

// Simple x86-64 machine code buffer
typedef struct CodeBuffer {
    uint8_t* code;          // Executable memory
    size_t size;            // Current size
    size_t capacity;        // Total capacity
} CodeBuffer;

// x86-64 register encodings
typedef enum {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15
} X64Register;

// Forward declarations for code generation
static CodeBuffer* createCodeBuffer(size_t capacity);
static void freeCodeBuffer(CodeBuffer* buffer);
static void emitByte(CodeBuffer* buffer, uint8_t byte);
static void emitBytes(CodeBuffer* buffer, uint8_t* bytes, size_t count);
static void emitInt32(CodeBuffer* buffer, int32_t value);
static void emitInt64(CodeBuffer* buffer, int64_t value);

// x86-64 instruction encoding helpers
static void emitRex(CodeBuffer* buffer, bool w, uint8_t r, uint8_t x, uint8_t b);
static void emitModRM(CodeBuffer* buffer, uint8_t mod, uint8_t reg, uint8_t rm);
static void emitSib(CodeBuffer* buffer, uint8_t scale, uint8_t index, uint8_t base);

// High-level x86-64 instruction emitters
static void emitMovRegImm64(CodeBuffer* buffer, X64Register reg, int64_t imm);
static void emitMovRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitAddRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm);
static void emitSubRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm);
static void emitPushReg(CodeBuffer* buffer, X64Register reg);
static void emitPopReg(CodeBuffer* buffer, X64Register reg);
static void emitRet(CodeBuffer* buffer);
static void emitCallReg(CodeBuffer* buffer, X64Register reg);
static void emitMovRegMem(CodeBuffer* buffer, X64Register reg, X64Register base, int32_t offset);
static void emitMovMemReg(CodeBuffer* buffer, X64Register base, int32_t offset, X64Register reg);

// Bytecode to native compilation
static bool compileBytecodToNative(ObjClosure* closure, CodeBuffer* buffer);
static bool compileInstruction(CodeBuffer* buffer, uint8_t* ip, Chunk* chunk);

void initJIT() {
    jitContext.hotSpots = NULL;
    jitContext.compiledFunctions = NULL;
    jitContext.enabled = true;
    jitContext.totalCompilations = 0;
    jitContext.totalExecutions = 0;
}

void freeJIT() {
    // Free hot spots
    HotSpot* hotSpot = jitContext.hotSpots;
    while (hotSpot != NULL) {
        HotSpot* next = hotSpot->next;
        FREE(HotSpot, hotSpot);
        hotSpot = next;
    }
    
    // Free compiled functions
    JitFunction* function = jitContext.compiledFunctions;
    while (function != NULL) {
        JitFunction* next = function->next;
        if (function->nativeCode != NULL) {
            // Unmap executable memory
            munmap((void*)function->nativeCode, function->codeSize);
        }
        FREE(JitFunction, function);
        function = next;
    }
    
    jitContext.hotSpots = NULL;
    jitContext.compiledFunctions = NULL;
    jitContext.enabled = false;
}

void trackHotSpot(uint8_t* bytecode, bool isFunction) {
    if (!jitContext.enabled) return;
    
    HotSpot* hotSpot = findHotSpot(bytecode);
    if (hotSpot == NULL) {
        // Create new hot spot
        hotSpot = ALLOCATE(HotSpot, 1);
        hotSpot->bytecode = bytecode;
        hotSpot->hitCount = 1;
        hotSpot->isFunction = isFunction;
        hotSpot->next = jitContext.hotSpots;
        jitContext.hotSpots = hotSpot;
    } else {
        hotSpot->hitCount++;
    }
}

bool isHotSpot(uint8_t* bytecode) {
    HotSpot* hotSpot = findHotSpot(bytecode);
    if (hotSpot == NULL) return false;
    
    int threshold = hotSpot->isFunction ? JIT_HOT_THRESHOLD : JIT_HOT_LOOP_THRESHOLD;
    return hotSpot->hitCount >= threshold;
}

HotSpot* findHotSpot(uint8_t* bytecode) {
    HotSpot* current = jitContext.hotSpots;
    while (current != NULL) {
        if (current->bytecode == bytecode) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

bool shouldCompile(uint8_t* bytecode) {
    if (!jitContext.enabled) return false;
    if (findCompiledFunction(bytecode) != NULL) return false; // Already compiled
    return isHotSpot(bytecode);
}

JitFunction* findCompiledFunction(uint8_t* bytecode) {
    JitFunction* current = jitContext.compiledFunctions;
    while (current != NULL) {
        if (current->bytecodeStart <= bytecode && bytecode < current->bytecodeEnd) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

JitFunction* compileFunction(ObjClosure* closure) {
    if (!jitContext.enabled) return NULL;
    
    // Create code buffer
    CodeBuffer* buffer = createCodeBuffer(4096); // 4KB initial size
    if (buffer == NULL) return NULL;
    
    // Compile bytecode to native code
    if (!compileBytecodToNative(closure, buffer)) {
        freeCodeBuffer(buffer);
        return NULL;
    }
    
    // Create JIT function structure
    JitFunction* jitFunc = ALLOCATE(JitFunction, 1);
    if (jitFunc == NULL) {
        freeCodeBuffer(buffer);
        return NULL;
    }
    
    // Set up function metadata
    jitFunc->bytecodeStart = closure->function->chunk.code;
    jitFunc->bytecodeEnd = closure->function->chunk.code + closure->function->chunk.count;
    jitFunc->nativeCode = (JitCompiledFn)buffer->code;
    jitFunc->codeSize = buffer->size;
    jitFunc->callCount = 0;
    
    // Add to compiled functions list
    jitFunc->next = jitContext.compiledFunctions;
    jitContext.compiledFunctions = jitFunc;
    
    jitContext.totalCompilations++;
    
    // Don't free the buffer since we're using its executable memory
    free(buffer); // Just free the buffer struct, not the code
    
    return jitFunc;
}

InterpretResult executeJitFunction(JitFunction* function) {
    if (function == NULL || function->nativeCode == NULL) {
        return INTERPRET_RUNTIME_ERROR;
    }
    
    function->callCount++;
    jitContext.totalExecutions++;
    
    printf("JIT: Executing compiled function (call #%d)\n", function->callCount);
    
    // Call the compiled native function
    return function->nativeCode();
}

void printJitStats() {
    printf("\n=== JIT Statistics ===\n");
    printf("JIT Enabled: %s\n", jitContext.enabled ? "Yes" : "No");
    printf("Total Compilations: %d\n", jitContext.totalCompilations);
    printf("Total JIT Executions: %d\n", jitContext.totalExecutions);
    
    // Count hot spots
    int hotSpotCount = 0;
    HotSpot* hotSpot = jitContext.hotSpots;
    while (hotSpot != NULL) {
        hotSpotCount++;
        hotSpot = hotSpot->next;
    }
    printf("Hot Spots Tracked: %d\n", hotSpotCount);
    
    // Count compiled functions
    int compiledCount = 0;
    JitFunction* function = jitContext.compiledFunctions;
    while (function != NULL) {
        compiledCount++;
        function = function->next;
    }
    printf("Compiled Functions: %d\n", compiledCount);
    printf("======================\n\n");
}

// Code buffer management
static CodeBuffer* createCodeBuffer(size_t capacity) {
    CodeBuffer* buffer = malloc(sizeof(CodeBuffer));
    if (buffer == NULL) return NULL;
    
    // Allocate executable memory
    buffer->code = mmap(NULL, capacity, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buffer->code == MAP_FAILED) {
        free(buffer);
        return NULL;
    }
    
    buffer->size = 0;
    buffer->capacity = capacity;
    return buffer;
}

static void freeCodeBuffer(CodeBuffer* buffer) {
    if (buffer == NULL) return;
    
    if (buffer->code != NULL && buffer->code != MAP_FAILED) {
        munmap(buffer->code, buffer->capacity);
    }
    free(buffer);
}

static void emitByte(CodeBuffer* buffer, uint8_t byte) {
    if (buffer->size >= buffer->capacity) {
        // Buffer overflow - in a real implementation, we'd resize
        fprintf(stderr, "JIT: Code buffer overflow\n");
        return;
    }
    buffer->code[buffer->size++] = byte;
}

static void emitBytes(CodeBuffer* buffer, uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        emitByte(buffer, bytes[i]);
    }
}

static void emitInt32(CodeBuffer* buffer, int32_t value) {
    emitByte(buffer, value & 0xFF);
    emitByte(buffer, (value >> 8) & 0xFF);
    emitByte(buffer, (value >> 16) & 0xFF);
    emitByte(buffer, (value >> 24) & 0xFF);
}

static void emitInt64(CodeBuffer* buffer, int64_t value) {
    emitInt32(buffer, value & 0xFFFFFFFF);
    emitInt32(buffer, (value >> 32) & 0xFFFFFFFF);
}

// x86-64 instruction encoding
static void emitRex(CodeBuffer* buffer, bool w, uint8_t r, uint8_t x, uint8_t b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;  // 64-bit operand
    if (r & 8) rex |= 0x04;  // Extension of ModR/M reg field
    if (x & 8) rex |= 0x02;  // Extension of SIB index field
    if (b & 8) rex |= 0x01;  // Extension of ModR/M r/m field or SIB base field
    
    if (rex != 0x40 || w) {  // Only emit if necessary or for 64-bit
        emitByte(buffer, rex);
    }
}

static void emitModRM(CodeBuffer* buffer, uint8_t mod, uint8_t reg, uint8_t rm) {
    emitByte(buffer, (mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

// High-level instruction emitters
static void emitMovRegImm64(CodeBuffer* buffer, X64Register reg, int64_t imm) {
    // mov reg, imm64 (REX.W + B8+ rd io)
    emitRex(buffer, true, 0, 0, reg);
    emitByte(buffer, 0xB8 + (reg & 7));
    emitInt64(buffer, imm);
}

static void emitMovRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // mov dst, src (REX.W + 89 /r)
    emitRex(buffer, true, src, 0, dst);
    emitByte(buffer, 0x89);
    emitModRM(buffer, 3, src, dst);
}

static void emitPushReg(CodeBuffer* buffer, X64Register reg) {
    // push reg (50+ rd)
    if (reg >= R8) {
        emitRex(buffer, false, 0, 0, reg);
    }
    emitByte(buffer, 0x50 + (reg & 7));
}

static void emitPopReg(CodeBuffer* buffer, X64Register reg) {
    // pop reg (58+ rd)
    if (reg >= R8) {
        emitRex(buffer, false, 0, 0, reg);
    }
    emitByte(buffer, 0x58 + (reg & 7));
}

static void emitRet(CodeBuffer* buffer) {
    // ret (C3)
    emitByte(buffer, 0xC3);
}

static void emitAddRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm) {
    // add reg, imm32 (REX.W + 81 /0 id)
    emitRex(buffer, true, 0, 0, reg);
    emitByte(buffer, 0x81);
    emitModRM(buffer, 3, 0, reg);
    emitInt32(buffer, imm);
}

// Compile bytecode to native code
static bool compileBytecodToNative(ObjClosure* closure, CodeBuffer* buffer) {
    Chunk* chunk = &closure->function->chunk;
    
    // Function prologue
    emitPushReg(buffer, RBP);          // push rbp
    emitMovRegReg(buffer, RBP, RSP);   // mov rbp, rsp
    
    // Reserve space for local variables (simplified)
    emitAddRegImm32(buffer, RSP, -64); // sub rsp, 64 (reserve stack space)
    
    // Compile each bytecode instruction
    uint8_t* ip = chunk->code;
    uint8_t* end = chunk->code + chunk->count;
    
    while (ip < end) {
        if (!compileInstruction(buffer, ip, chunk)) {
            return false;
        }
        
        // Move to next instruction (simplified - real implementation needs proper decoding)
        uint8_t instruction = *ip;
        ip++; // Move past opcode
        
        // Skip operands based on instruction type
        switch (instruction) {
            case OP_CONSTANT:
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
                ip++; // Skip 1-byte operand
                break;
            case OP_CONSTANT_LONG:
                ip += 3; // Skip 3-byte operand
                break;
            case OP_GET_GLOBAL:
            case OP_SET_GLOBAL:
            case OP_DEFINE_GLOBAL:
                ip += 2; // Skip 2-byte operand (string constant index)
                break;
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_LOOP:
                ip += 2; // Skip 2-byte jump offset
                break;
            // Most other instructions have no operands
        }
    }
    
    // Function epilogue
    emitMovRegImm64(buffer, RAX, INTERPRET_OK); // mov rax, INTERPRET_OK
    emitAddRegImm32(buffer, RSP, 64);           // add rsp, 64 (restore stack)
    emitPopReg(buffer, RBP);                    // pop rbp
    emitRet(buffer);                            // ret
    
    return true;
}

static bool compileInstruction(CodeBuffer* buffer, uint8_t* ip, Chunk* chunk) {
    uint8_t instruction = *ip;
    
    switch (instruction) {
        case OP_CONSTANT: {
            // Load constant - for now, just emit a placeholder
            // In a real implementation, we'd load the constant and push it onto the VM stack
            break;
        }
        case OP_NIL:
        case OP_TRUE:
        case OP_FALSE: {
            // Load literal values - emit placeholder
            break;
        }
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE: {
            // Arithmetic operations - emit placeholder
            break;
        }
        case OP_RETURN: {
            // Return early from function
            emitMovRegImm64(buffer, RAX, INTERPRET_OK); // mov rax, INTERPRET_OK
            emitAddRegImm32(buffer, RSP, 64);           // add rsp, 64
            emitPopReg(buffer, RBP);                    // pop rbp
            emitRet(buffer);                            // ret
            break;
        }
        default:
            // For unimplemented instructions, just continue
            break;
    }
    
    return true;
}

void dumpJitFunction(JitFunction* function) {
    if (function == NULL) return;
    
    printf("JIT Function Dump:\n");
    printf("  Bytecode range: %p - %p\n", 
           (void*)function->bytecodeStart, (void*)function->bytecodeEnd);
    printf("  Native code: %p\n", (void*)function->nativeCode);
    printf("  Code size: %zu bytes\n", function->codeSize);
    printf("  Call count: %d\n", function->callCount);
    
    // Dump machine code bytes (first 64 bytes max)
    printf("  Machine code: ");
    size_t dumpSize = function->codeSize < 64 ? function->codeSize : 64;
    for (size_t i = 0; i < dumpSize; i++) {
        printf("%02x ", ((uint8_t*)function->nativeCode)[i]);
    }
    if (function->codeSize > 64) {
        printf("...");
    }
    printf("\n");
} 