#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stddef.h>

#include "jit.h"
#include "memory.h"
#include "debug.h"
#include "vm.h"

// Global JIT context
JitContext jitContext = {0};

// Performance timing
static struct timeval jitStartTime;
static bool jitTimerActive = false;

// Efficient x86-64 machine code buffer
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

// VM register mapping for efficient access
#define VM_REG RDI              // VM pointer (first argument)
#define FRAME_REG RSI           // CallFrame pointer (second argument)
#define STACK_REG R12           // VM stack pointer (cached)
#define TEMP_REG_1 RAX          // Temporary register 1
#define TEMP_REG_2 RDX          // Temporary register 2
#define TEMP_REG_3 RCX          // Temporary register 3

// Forward declarations for code generation
static CodeBuffer* createCodeBuffer(size_t capacity);
static void freeCodeBuffer(CodeBuffer* buffer);
static void emitByte(CodeBuffer* buffer, uint8_t byte);
static void emitInt32(CodeBuffer* buffer, int32_t value);
static void emitInt64(CodeBuffer* buffer, int64_t value);

// x86-64 instruction encoding
static void emitRex(CodeBuffer* buffer, bool w, uint8_t r, uint8_t x, uint8_t b);
static void emitModRM(CodeBuffer* buffer, uint8_t mod, uint8_t reg, uint8_t rm);

// Essential x86-64 instruction emitters
static void emitMovRegImm64(CodeBuffer* buffer, X64Register reg, int64_t imm);
static void emitMovRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitMovRegMem(CodeBuffer* buffer, X64Register reg, X64Register base, int32_t offset);
static void emitMovMemReg(CodeBuffer* buffer, X64Register base, int32_t offset, X64Register reg);
static void emitAddRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitSubRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitCmpRegReg(CodeBuffer* buffer, X64Register reg1, X64Register reg2);
static void emitJe(CodeBuffer* buffer, int32_t offset);
static void emitJne(CodeBuffer* buffer, int32_t offset);
static void emitJmp(CodeBuffer* buffer, int32_t offset);
static void emitPushReg(CodeBuffer* buffer, X64Register reg);
static void emitPopReg(CodeBuffer* buffer, X64Register reg);
static void emitRet(CodeBuffer* buffer);
static void emitCallImm(CodeBuffer* buffer, void* target);

// Helper functions for immediate values
static void emitAddRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm);
static void emitSubRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm);

// SSE floating point instructions
static void emitMovsdRegMem(CodeBuffer* buffer, X64Register reg, X64Register base, int32_t offset);
static void emitMovsdMemReg(CodeBuffer* buffer, X64Register base, int32_t offset, X64Register reg);
static void emitAddsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitSubsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitMulsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);
static void emitDivsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src);

// Bytecode compilation
static bool compileFunctionToNative(ObjClosure* closure, CodeBuffer* buffer);
static bool compileInstruction(CodeBuffer* buffer, uint8_t** ip, Chunk* chunk);

// VM stack operations (efficient)
static void emitVMPush(CodeBuffer* buffer, X64Register valueReg) {
    // vm->stackTop[0] = value; vm->stackTop++;
    emitMovMemReg(buffer, STACK_REG, 0, valueReg);
    emitAddRegImm32(buffer, STACK_REG, 8);
    emitMovMemReg(buffer, VM_REG, offsetof(VM, stackTop), STACK_REG);
}

static void emitVMPop(CodeBuffer* buffer, X64Register valueReg) {
    // vm->stackTop--; value = vm->stackTop[0];
    emitSubRegImm32(buffer, STACK_REG, 8);
    emitMovMemReg(buffer, VM_REG, offsetof(VM, stackTop), STACK_REG);
    emitMovRegMem(buffer, valueReg, STACK_REG, 0);
}

static void emitVMPeek(CodeBuffer* buffer, X64Register valueReg, int distance) {
    // value = vm->stackTop[-1-distance];
    int offset = -(1 + distance) * 8;
    emitMovRegMem(buffer, valueReg, STACK_REG, offset);
}

// Function prologue and epilogue
static void emitFunctionPrologue(CodeBuffer* buffer) {
    // Standard function prologue
    emitPushReg(buffer, RBP);
    emitMovRegReg(buffer, RBP, RSP);
    
    // Save callee-saved registers
    emitPushReg(buffer, RBX);
    emitPushReg(buffer, R12);
    emitPushReg(buffer, R13);
    emitPushReg(buffer, R14);
    emitPushReg(buffer, R15);
    
    // Cache VM stack pointer in R12
    emitMovRegMem(buffer, STACK_REG, VM_REG, offsetof(VM, stackTop));
}

static void emitFunctionEpilogue(CodeBuffer* buffer) {
    // Update VM stack pointer
    emitMovMemReg(buffer, VM_REG, offsetof(VM, stackTop), STACK_REG);
    
    // Restore callee-saved registers
    emitPopReg(buffer, R15);
    emitPopReg(buffer, R14);
    emitPopReg(buffer, R13);
    emitPopReg(buffer, R12);
    emitPopReg(buffer, RBX);
    
    // Standard function epilogue
    emitMovRegReg(buffer, RSP, RBP);
    emitPopReg(buffer, RBP);
    
    // Return INTERPRET_OK
    emitMovRegImm64(buffer, RAX, INTERPRET_OK);
    emitRet(buffer);
}

// Helper to add immediate value with proper encoding
static void emitAddRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm) {
    if (reg >= R8) {
        emitRex(buffer, true, 0, 0, reg);
    } else {
        emitRex(buffer, true, 0, 0, 0);
    }
    
    if (imm >= -128 && imm <= 127) {
        // Use 8-bit immediate
        emitByte(buffer, 0x83);
        emitModRM(buffer, 3, 0, reg & 7);
        emitByte(buffer, imm & 0xFF);
    } else {
        // Use 32-bit immediate
        emitByte(buffer, 0x81);
        emitModRM(buffer, 3, 0, reg & 7);
        emitInt32(buffer, imm);
    }
}

static void emitSubRegImm32(CodeBuffer* buffer, X64Register reg, int32_t imm) {
    if (reg >= R8) {
        emitRex(buffer, true, 0, 0, reg);
    } else {
        emitRex(buffer, true, 0, 0, 0);
    }
    
    if (imm >= -128 && imm <= 127) {
        // Use 8-bit immediate
        emitByte(buffer, 0x83);
        emitModRM(buffer, 3, 5, reg & 7);
        emitByte(buffer, imm & 0xFF);
    } else {
        // Use 32-bit immediate
        emitByte(buffer, 0x81);
        emitModRM(buffer, 3, 5, reg & 7);
        emitInt32(buffer, imm);
    }
}

// JIT API Implementation
void initJIT() {
    jitContext.enabled = true;
    jitContext.hotSpots = NULL;
    jitContext.compiledFunctions = NULL;
    jitContext.blacklistedFunctions = NULL;
    jitContext.defaultOptLevel = JIT_OPT_BASIC;
    jitContext.totalCompilations = 0;
    jitContext.totalExecutions = 0;
    jitContext.totalOptimizations = 0;
    jitContext.totalCompileTime = 0.0;
    jitContext.totalExecutionTime = 0.0;
    jitContext.allocator = NULL;
    
    // No output for performance
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
            munmap((void*)function->nativeCode, function->codeSize);
        }
        FREE(JitFunction, function);
        function = next;
    }
    
    // Free blacklisted functions
    BlacklistedFunction* blacklisted = jitContext.blacklistedFunctions;
    while (blacklisted != NULL) {
        BlacklistedFunction* next = blacklisted->next;
        FREE(BlacklistedFunction, blacklisted);
        blacklisted = next;
    }
    
    // Clear allocator
    if (jitContext.allocator != NULL) {
        FREE(RegisterAllocator, jitContext.allocator);
    }
    
    // Clear context
    memset(&jitContext, 0, sizeof(JitContext));
    
    // No output for performance
}

void trackHotSpot(uint8_t* bytecode, bool isFunction) {
    if (!jitContext.enabled) return;
    
    HotSpot* hotSpot = findHotSpot(bytecode);
    if (hotSpot == NULL) {
        hotSpot = ALLOCATE(HotSpot, 1);
        if (hotSpot == NULL) return;
        
        hotSpot->bytecode = bytecode;
        hotSpot->hitCount = 1;
        hotSpot->isFunction = isFunction;
        hotSpot->isLoop = false;
        hotSpot->optLevel = JIT_OPT_NONE;
        hotSpot->next = jitContext.hotSpots;
        jitContext.hotSpots = hotSpot;
    } else {
        hotSpot->hitCount++;
        
        // Promote hot spot if it gets hot enough
        if (hotSpot->hitCount >= JIT_HOT_THRESHOLD && hotSpot->optLevel == JIT_OPT_NONE) {
            promoteHotSpot(hotSpot);
        }
    }
}

void trackLoopBackEdge(uint8_t* bytecode) {
    if (!jitContext.enabled) return;
    
    HotSpot* hotSpot = findHotSpot(bytecode);
    if (hotSpot == NULL) {
        hotSpot = ALLOCATE(HotSpot, 1);
        if (hotSpot == NULL) return;
        
        hotSpot->bytecode = bytecode;
        hotSpot->hitCount = 1;
        hotSpot->isFunction = false;
        hotSpot->isLoop = true;
        hotSpot->optLevel = JIT_OPT_NONE;
        hotSpot->next = jitContext.hotSpots;
        jitContext.hotSpots = hotSpot;
    } else {
        hotSpot->hitCount++;
        
        // Loops get promoted faster
        if (hotSpot->hitCount >= JIT_HOT_LOOP_THRESHOLD && hotSpot->optLevel == JIT_OPT_NONE) {
            promoteHotSpot(hotSpot);
        }
    }
}

bool isHotSpot(uint8_t* bytecode) {
    HotSpot* hotSpot = findHotSpot(bytecode);
    if (hotSpot == NULL) return false;
    
    if (hotSpot->isFunction) {
        return hotSpot->hitCount >= JIT_HOT_THRESHOLD;
    } else {
        return hotSpot->hitCount >= JIT_HOT_LOOP_THRESHOLD;
    }
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

void promoteHotSpot(HotSpot* hotSpot) {
    if (hotSpot->optLevel < JIT_OPT_AGGRESSIVE) {
        hotSpot->optLevel++;
        // Removed debug output for performance
    }
}

bool shouldCompile(uint8_t* bytecode) {
    if (!jitContext.enabled) return false;
    if (findCompiledFunction(bytecode) != NULL) return false;
    if (isBlacklisted(bytecode)) return false;
    return isHotSpot(bytecode);
}

bool shouldRecompile(JitFunction* function) {
    if (!jitContext.enabled || function == NULL) return false;
    
    HotSpot* hotSpot = findHotSpot(function->bytecodeStart);
    if (hotSpot != NULL && hotSpot->optLevel > function->optLevel) {
        return true;
    }
    
    return function->callCount > JIT_HOT_THRESHOLD * 5 && function->optLevel < JIT_OPT_AGGRESSIVE;
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
    return compileFunctionWithOptLevel(closure, jitContext.defaultOptLevel);
}

JitFunction* compileFunctionWithOptLevel(ObjClosure* closure, JitOptLevel optLevel) {
    if (!jitContext.enabled || closure == NULL) return NULL;
    
    if (isBlacklisted(closure->function->chunk.code)) {
        return NULL;
    }
    
    startJitTimer();
    
    CodeBuffer* buffer = createCodeBuffer(4096);
    if (buffer == NULL) return NULL;
    
    // No debug output for performance
    
    if (!compileFunctionToNative(closure, buffer)) {
        freeCodeBuffer(buffer);
        addToBlacklist(closure->function->chunk.code);
        return NULL;
    }
    
    JitFunction* jitFunc = ALLOCATE(JitFunction, 1);
    if (jitFunc == NULL) {
        freeCodeBuffer(buffer);
        return NULL;
    }
    
    jitFunc->bytecodeStart = closure->function->chunk.code;
    jitFunc->bytecodeEnd = closure->function->chunk.code + closure->function->chunk.count;
    jitFunc->nativeCode = (JitCompiledFn)buffer->code;
    jitFunc->codeSize = buffer->size;
    jitFunc->callCount = 0;
    jitFunc->optLevel = optLevel;
    jitFunc->avgExecutionTime = 0.0;
    jitFunc->isInlined = false;
    jitFunc->paramCount = closure->function->arity;
    jitFunc->localCount = 0;
    
    jitFunc->next = jitContext.compiledFunctions;
    jitContext.compiledFunctions = jitFunc;
    
    jitContext.totalCompilations++;
    double compileTime = stopJitTimer();
    jitContext.totalCompileTime += compileTime;
    
    free(buffer);
    return jitFunc;
}

InterpretResult executeJitFunction(JitFunction* function, VM* vm, CallFrame* frame) {
    if (function == NULL || function->nativeCode == NULL) {
        return INTERPRET_RUNTIME_ERROR;
    }
    
    function->callCount++;
    jitContext.totalExecutions++;
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    InterpretResult result = function->nativeCode(vm, frame);
    
    gettimeofday(&end, NULL);
    double executionTime = (end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec);
    updateExecutionStats(function, executionTime);
    
    return result;
}

// Register allocation (simplified for performance)
void initRegisterAllocator(RegisterAllocator* allocator) {
    for (int i = 0; i < JIT_MAX_REGISTERS; i++) {
        allocator->registers[i].state = REG_FREE;
        allocator->registers[i].value = NIL_VAL;
        allocator->registers[i].lastUse = -1;
        allocator->registers[i].isDirty = false;
    }
    allocator->nextSpillSlot = 0;
    allocator->currentInstruction = 0;
}

int allocateRegister(RegisterAllocator* allocator, Value value) {
    for (int i = 0; i < JIT_MAX_REGISTERS; i++) {
        if (allocator->registers[i].state == REG_FREE) {
            allocator->registers[i].state = REG_ALLOCATED;
            allocator->registers[i].value = value;
            allocator->registers[i].lastUse = allocator->currentInstruction;
            allocator->registers[i].isDirty = false;
            return i;
        }
    }
    
    int spillReg = findBestRegisterToSpill(allocator);
    spillRegister(allocator, spillReg);
    
    allocator->registers[spillReg].state = REG_ALLOCATED;
    allocator->registers[spillReg].value = value;
    allocator->registers[spillReg].lastUse = allocator->currentInstruction;
    allocator->registers[spillReg].isDirty = false;
    
    return spillReg;
}

void freeRegister(RegisterAllocator* allocator, int reg) {
    if (reg >= 0 && reg < JIT_MAX_REGISTERS) {
        allocator->registers[reg].state = REG_FREE;
        allocator->registers[reg].value = NIL_VAL;
        allocator->registers[reg].isDirty = false;
    }
}

void spillRegister(RegisterAllocator* allocator, int reg) {
    if (reg >= 0 && reg < JIT_MAX_REGISTERS) {
        allocator->registers[reg].state = REG_SPILLED;
        allocator->nextSpillSlot++;
    }
}

int findBestRegisterToSpill(RegisterAllocator* allocator) {
    int bestReg = 0;
    int oldestUse = allocator->registers[0].lastUse;
    
    for (int i = 1; i < JIT_MAX_REGISTERS; i++) {
        if (allocator->registers[i].lastUse < oldestUse) {
            oldestUse = allocator->registers[i].lastUse;
            bestReg = i;
        }
    }
    
    return bestReg;
}

// Performance monitoring
void startJitTimer() {
    gettimeofday(&jitStartTime, NULL);
    jitTimerActive = true;
}

double stopJitTimer() {
    if (!jitTimerActive) return 0.0;
    
    struct timeval endTime;
    gettimeofday(&endTime, NULL);
    jitTimerActive = false;
    
    return (endTime.tv_sec - jitStartTime.tv_sec) * 1000000.0 + 
           (endTime.tv_usec - jitStartTime.tv_usec);
}

void updateExecutionStats(JitFunction* function, double executionTime) {
    if (function == NULL) return;
    
    if (function->avgExecutionTime == 0.0) {
        function->avgExecutionTime = executionTime;
    } else {
        function->avgExecutionTime = (function->avgExecutionTime * 0.9) + (executionTime * 0.1);
    }
    
    jitContext.totalExecutionTime += executionTime;
}

void printJitStats() {
    printf("=== JIT Statistics ===\n");
    printf("Enabled: %s\n", jitContext.enabled ? "Yes" : "No");
    printf("Total Compilations: %d\n", jitContext.totalCompilations);
    printf("Total Executions: %d\n", jitContext.totalExecutions);
    printf("Total Optimizations: %d\n", jitContext.totalOptimizations);
    printf("Total Compile Time: %.2f ms\n", jitContext.totalCompileTime / 1000.0);
    printf("Total Execution Time Saved: %.2f ms\n", jitContext.totalExecutionTime / 1000.0);
    
    if (jitContext.totalCompilations > 0) {
        printf("Average Compile Time: %.2f ms\n", 
               (jitContext.totalCompileTime / jitContext.totalCompilations) / 1000.0);
    }
    
    if (jitContext.totalExecutions > 0) {
        printf("Average Execution Time: %.2f μs\n", 
               jitContext.totalExecutionTime / jitContext.totalExecutions);
    }
    
    // Count hot spots
    int hotSpotCount = 0;
    HotSpot* hotSpot = jitContext.hotSpots;
    while (hotSpot != NULL) {
        hotSpotCount++;
        hotSpot = hotSpot->next;
    }
    printf("Hot Spots: %d\n", hotSpotCount);
    
    // Count compiled functions
    int compiledFunctionCount = 0;
    JitFunction* function = jitContext.compiledFunctions;
    while (function != NULL) {
        compiledFunctionCount++;
        function = function->next;
    }
    printf("Compiled Functions: %d\n", compiledFunctionCount);
    
    // Count blacklisted functions
    int blacklistedCount = 0;
    BlacklistedFunction* blacklisted = jitContext.blacklistedFunctions;
    while (blacklisted != NULL) {
        blacklistedCount++;
        blacklisted = blacklisted->next;
    }
    printf("Blacklisted Functions: %d\n", blacklistedCount);
    printf("======================\n");
}

void printDetailedJitStats() {
    printJitStats();
    
    printf("\n=== Detailed Function Stats ===\n");
    JitFunction* function = jitContext.compiledFunctions;
    int index = 0;
    while (function != NULL) {
        printf("Function %d:\n", index++);
        printf("  Bytecode: %p - %p\n", (void*)function->bytecodeStart, (void*)function->bytecodeEnd);
        printf("  Code Size: %zu bytes\n", function->codeSize);
        printf("  Call Count: %d\n", function->callCount);
        printf("  Optimization Level: %d\n", function->optLevel);
        printf("  Average Execution Time: %.2f μs\n", function->avgExecutionTime);
        printf("  Parameters: %d\n", function->paramCount);
        printf("  Locals: %d\n", function->localCount);
        printf("  Inlined: %s\n", function->isInlined ? "Yes" : "No");
        printf("\n");
        function = function->next;
    }
    
    printf("=== Hot Spots ===\n");
    HotSpot* hotSpot = jitContext.hotSpots;
    index = 0;
    while (hotSpot != NULL) {
        printf("Hot Spot %d:\n", index++);
        printf("  Bytecode: %p\n", (void*)hotSpot->bytecode);
        printf("  Hit Count: %d\n", hotSpot->hitCount);
        printf("  Type: %s\n", hotSpot->isFunction ? "Function" : (hotSpot->isLoop ? "Loop" : "Other"));
        printf("  Optimization Level: %d\n", hotSpot->optLevel);
        printf("\n");
        hotSpot = hotSpot->next;
    }
    printf("===============================\n");
}

// Code buffer management
static CodeBuffer* createCodeBuffer(size_t capacity) {
    CodeBuffer* buffer = malloc(sizeof(CodeBuffer));
    if (buffer == NULL) return NULL;
    
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
        // Buffer overflow - should not happen with proper sizing
        return;
    }
    buffer->code[buffer->size++] = byte;
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
    if (w) rex |= 0x08;
    if (r >= 8) rex |= 0x04;
    if (x >= 8) rex |= 0x02;
    if (b >= 8) rex |= 0x01;
    
    if (rex != 0x40 || w) {
        emitByte(buffer, rex);
    }
}

static void emitModRM(CodeBuffer* buffer, uint8_t mod, uint8_t reg, uint8_t rm) {
    emitByte(buffer, (mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

// Essential x86-64 instruction emitters
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

static void emitMovRegMem(CodeBuffer* buffer, X64Register reg, X64Register base, int32_t offset) {
    // mov reg, [base + offset] (REX.W + 8B /r)
    emitRex(buffer, true, reg, 0, base);
    emitByte(buffer, 0x8B);
    
    if (offset == 0 && (base & 7) != RBP) {
        emitModRM(buffer, 0, reg, base);
    } else if (offset >= -128 && offset <= 127) {
        emitModRM(buffer, 1, reg, base);
        emitByte(buffer, offset & 0xFF);
    } else {
        emitModRM(buffer, 2, reg, base);
        emitInt32(buffer, offset);
    }
}

static void emitMovMemReg(CodeBuffer* buffer, X64Register base, int32_t offset, X64Register reg) {
    // mov [base + offset], reg (REX.W + 89 /r)
    emitRex(buffer, true, reg, 0, base);
    emitByte(buffer, 0x89);
    
    if (offset == 0 && (base & 7) != RBP) {
        emitModRM(buffer, 0, reg, base);
    } else if (offset >= -128 && offset <= 127) {
        emitModRM(buffer, 1, reg, base);
        emitByte(buffer, offset & 0xFF);
    } else {
        emitModRM(buffer, 2, reg, base);
        emitInt32(buffer, offset);
    }
}

static void emitAddRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // add dst, src (REX.W + 01 /r)
    emitRex(buffer, true, src, 0, dst);
    emitByte(buffer, 0x01);
    emitModRM(buffer, 3, src, dst);
}

static void emitSubRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // sub dst, src (REX.W + 29 /r)
    emitRex(buffer, true, src, 0, dst);
    emitByte(buffer, 0x29);
    emitModRM(buffer, 3, src, dst);
}

static void emitCmpRegReg(CodeBuffer* buffer, X64Register reg1, X64Register reg2) {
    // cmp reg1, reg2 (REX.W + 39 /r)
    emitRex(buffer, true, reg2, 0, reg1);
    emitByte(buffer, 0x39);
    emitModRM(buffer, 3, reg2, reg1);
}

static void emitJe(CodeBuffer* buffer, int32_t offset) {
    // je offset (0F 84 cd)
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x84);
    emitInt32(buffer, offset);
}

static void emitJne(CodeBuffer* buffer, int32_t offset) {
    // jne offset (0F 85 cd)
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x85);
    emitInt32(buffer, offset);
}

static void emitJmp(CodeBuffer* buffer, int32_t offset) {
    // jmp offset (E9 cd)
    emitByte(buffer, 0xE9);
    emitInt32(buffer, offset);
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

static void emitCallImm(CodeBuffer* buffer, void* target) {
    // call target (E8 cd)
    emitByte(buffer, 0xE8);
    // Calculate relative offset from next instruction
    int64_t offset = (int64_t)target - (int64_t)(buffer->code + buffer->size + 4);
    emitInt32(buffer, (int32_t)offset);
}

// SSE floating point instructions (simplified)
static void emitMovsdRegMem(CodeBuffer* buffer, X64Register reg, X64Register base, int32_t offset) {
    // movsd xmm_reg, [base + offset] (F2 REX 0F 10 /r)
    emitByte(buffer, 0xF2);
    emitRex(buffer, false, reg, 0, base);
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x10);
    
    if (offset == 0 && (base & 7) != RBP) {
        emitModRM(buffer, 0, reg, base);
    } else if (offset >= -128 && offset <= 127) {
        emitModRM(buffer, 1, reg, base);
        emitByte(buffer, offset & 0xFF);
    } else {
        emitModRM(buffer, 2, reg, base);
        emitInt32(buffer, offset);
    }
}

static void emitMovsdMemReg(CodeBuffer* buffer, X64Register base, int32_t offset, X64Register reg) {
    // movsd [base + offset], xmm_reg (F2 REX 0F 11 /r)
    emitByte(buffer, 0xF2);
    emitRex(buffer, false, reg, 0, base);
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x11);
    
    if (offset == 0 && (base & 7) != RBP) {
        emitModRM(buffer, 0, reg, base);
    } else if (offset >= -128 && offset <= 127) {
        emitModRM(buffer, 1, reg, base);
        emitByte(buffer, offset & 0xFF);
    } else {
        emitModRM(buffer, 2, reg, base);
        emitInt32(buffer, offset);
    }
}

static void emitAddsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // addsd xmm_dst, xmm_src (F2 0F 58 /r)
    emitByte(buffer, 0xF2);
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x58);
    emitModRM(buffer, 3, dst, src);
}

static void emitSubsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // subsd xmm_dst, xmm_src (F2 0F 5C /r)
    emitByte(buffer, 0xF2);
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x5C);
    emitModRM(buffer, 3, dst, src);
}

static void emitMulsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // mulsd xmm_dst, xmm_src (F2 0F 59 /r)
    emitByte(buffer, 0xF2);
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x59);
    emitModRM(buffer, 3, dst, src);
}

static void emitDivsdRegReg(CodeBuffer* buffer, X64Register dst, X64Register src) {
    // divsd xmm_dst, xmm_src (F2 0F 5E /r)
    emitByte(buffer, 0xF2);
    emitByte(buffer, 0x0F);
    emitByte(buffer, 0x5E);
    emitModRM(buffer, 3, dst, src);
}

// Bytecode compilation - much more efficient implementation
static bool compileFunctionToNative(ObjClosure* closure, CodeBuffer* buffer) {
    Chunk* chunk = &closure->function->chunk;
    
    emitFunctionPrologue(buffer);
    
    uint8_t* ip = chunk->code;
    uint8_t* end = chunk->code + chunk->count;
    
    while (ip < end) {
        if (!compileInstruction(buffer, &ip, chunk)) {
            return false;
        }
    }
    
    emitFunctionEpilogue(buffer);
    return true;
}

static bool compileInstruction(CodeBuffer* buffer, uint8_t** ip, Chunk* chunk) {
    uint8_t instruction = **ip;
    (*ip)++;
    
    switch (instruction) {
        case OP_CONSTANT: {
            uint8_t constantIndex = **ip;
            (*ip)++;
            
            if (constantIndex >= chunk->constants.count) return false;
            Value constant = chunk->constants.values[constantIndex];
            
            emitMovRegImm64(buffer, TEMP_REG_1, (int64_t)constant);
            emitVMPush(buffer, TEMP_REG_1);
            break;
        }
        
        case OP_CONSTANT_LONG: {
            uint32_t constantIndex = ((*ip)[0] << 16) | ((*ip)[1] << 8) | (*ip)[2];
            (*ip) += 3;
            
            if (constantIndex >= chunk->constants.count) return false;
            Value constant = chunk->constants.values[constantIndex];
            
            emitMovRegImm64(buffer, TEMP_REG_1, (int64_t)constant);
            emitVMPush(buffer, TEMP_REG_1);
            break;
        }
        
        case OP_NIL:
            emitMovRegImm64(buffer, TEMP_REG_1, (int64_t)NIL_VAL);
            emitVMPush(buffer, TEMP_REG_1);
            break;
            
        case OP_TRUE:
            emitMovRegImm64(buffer, TEMP_REG_1, (int64_t)BOOL_VAL(true));
            emitVMPush(buffer, TEMP_REG_1);
            break;
            
        case OP_FALSE:
            emitMovRegImm64(buffer, TEMP_REG_1, (int64_t)BOOL_VAL(false));
            emitVMPush(buffer, TEMP_REG_1);
            break;
            
        case OP_POP:
            emitVMPop(buffer, TEMP_REG_1);
            break;
            
        case OP_GET_LOCAL: {
            uint8_t slot = **ip;
            (*ip)++;
            
            // Load from frame->slots[slot]
            int32_t offset = offsetof(CallFrame, slots) + slot * sizeof(Value);
            emitMovRegMem(buffer, TEMP_REG_1, FRAME_REG, offset);
            emitVMPush(buffer, TEMP_REG_1);
            break;
        }
        
        case OP_SET_LOCAL: {
            uint8_t slot = **ip;
            (*ip)++;
            
            // Pop value and store in frame->slots[slot]
            emitVMPeek(buffer, TEMP_REG_1, 0);  // Peek instead of pop to leave on stack
            int32_t offset = offsetof(CallFrame, slots) + slot * sizeof(Value);
            emitMovMemReg(buffer, FRAME_REG, offset, TEMP_REG_1);
            break;
        }
        
        case OP_GET_GLOBAL: {
            // For now, fall back to interpreter for global access
            // This requires calling into the VM's global table lookup
            return false;
        }
        
        case OP_SET_GLOBAL: {
            // For now, fall back to interpreter for global access
            return false;
        }
        
        case OP_DEFINE_GLOBAL: {
            // For now, fall back to interpreter for global access
            return false;
        }
        
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE: {
            // Generic arithmetic - fall back to interpreter for type checking
            return false;
        }
        
        case OP_ADD_NUMBER: {
            // Optimized number addition - implement with SSE
            emitVMPop(buffer, TEMP_REG_2);  // Second operand
            emitVMPop(buffer, TEMP_REG_1);  // First operand
            
            // For now, assume they are already numbers and just add the raw values
            // This is a simplified implementation - real version would extract doubles
            emitAddRegReg(buffer, TEMP_REG_1, TEMP_REG_2);
            emitVMPush(buffer, TEMP_REG_1);
            break;
        }
        
        case OP_SUBTRACT_NUMBER: {
            emitVMPop(buffer, TEMP_REG_2);  // Second operand
            emitVMPop(buffer, TEMP_REG_1);  // First operand
            emitSubRegReg(buffer, TEMP_REG_1, TEMP_REG_2);
            emitVMPush(buffer, TEMP_REG_1);
            break;
        }
        
        case OP_MULTIPLY_NUMBER:
        case OP_DIVIDE_NUMBER: {
            // These require proper floating point handling
            return false;
        }
        
        case OP_NEGATE:
        case OP_NEGATE_NUMBER: {
            // For now, fall back to interpreter
            return false;
        }
        
        case OP_EQUAL: {
            emitVMPop(buffer, TEMP_REG_2);
            emitVMPop(buffer, TEMP_REG_1);
            emitCmpRegReg(buffer, TEMP_REG_1, TEMP_REG_2);
            
            // Set result based on comparison
            emitMovRegImm64(buffer, TEMP_REG_3, (int64_t)BOOL_VAL(false));
            emitJne(buffer, 10);  // Skip next instruction if not equal
            emitMovRegImm64(buffer, TEMP_REG_3, (int64_t)BOOL_VAL(true));
            
            emitVMPush(buffer, TEMP_REG_3);
            break;
        }
        
        case OP_GREATER:
        case OP_LESS: {
            // For now, fall back to interpreter for proper comparison
            return false;
        }
        
        case OP_NOT: {
            // For now, fall back to interpreter
            return false;
        }
        
        case OP_PRINT: {
            // Fall back to interpreter for print
            return false;
        }
        
        case OP_JUMP: {
            uint16_t offset = ((*ip)[0] << 8) | (*ip)[1];
            (*ip) += 2;
            emitJmp(buffer, offset);
            break;
        }
        
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = ((*ip)[0] << 8) | (*ip)[1];
            (*ip) += 2;
            
            emitVMPop(buffer, TEMP_REG_1);
            // Check if value is falsey (simplified - just check if 0)
            emitMovRegImm64(buffer, TEMP_REG_2, 0);
            emitCmpRegReg(buffer, TEMP_REG_1, TEMP_REG_2);
            emitJe(buffer, offset);
            break;
        }
        
        case OP_LOOP: {
            uint16_t offset = ((*ip)[0] << 8) | (*ip)[1];
            (*ip) += 2;
            emitJmp(buffer, -(int32_t)offset);
            break;
        }
        
        case OP_CALL: {
            // Fall back to interpreter for function calls
            return false;
        }
        
        case OP_RETURN:
            emitFunctionEpilogue(buffer);
            return true;
            
        default:
            // For unimplemented instructions, fall back to interpreter
            return false;
    }
    
    return true;
}

// Blacklist management
void addToBlacklist(uint8_t* bytecode) {
    if (isBlacklisted(bytecode)) return;
    
    BlacklistedFunction* blacklisted = ALLOCATE(BlacklistedFunction, 1);
    if (blacklisted == NULL) return;
    
    blacklisted->bytecodeStart = bytecode;
    blacklisted->next = jitContext.blacklistedFunctions;
    jitContext.blacklistedFunctions = blacklisted;
}

bool isBlacklisted(uint8_t* bytecode) {
    BlacklistedFunction* current = jitContext.blacklistedFunctions;
    while (current != NULL) {
        if (current->bytecodeStart == bytecode) {
            return true;
        }
        current = current->next;
    }
    return false;
}

void clearBlacklist() {
    BlacklistedFunction* current = jitContext.blacklistedFunctions;
    while (current != NULL) {
        BlacklistedFunction* next = current->next;
        FREE(BlacklistedFunction, current);
        current = next;
    }
    jitContext.blacklistedFunctions = NULL;
}

// Optimization passes (placeholder implementations)
void applyBasicOptimizations(uint8_t* bytecode, size_t length) {
    // Basic optimizations would go here
}

void applyAdvancedOptimizations(uint8_t* bytecode, size_t length) {
    // Advanced optimizations would go here
}

void applyAggressiveOptimizations(uint8_t* bytecode, size_t length) {
    // Aggressive optimizations would go here
}

// Debug functions
void dumpJitFunction(JitFunction* function) {
    if (function == NULL) return;
    
    printf("JIT Function Dump:\n");
    printf("  Bytecode: %p - %p\n", (void*)function->bytecodeStart, (void*)function->bytecodeEnd);
    printf("  Native Code: %p\n", (void*)function->nativeCode);
    printf("  Code Size: %zu bytes\n", function->codeSize);
    printf("  Call Count: %d\n", function->callCount);
    printf("  Optimization Level: %d\n", function->optLevel);
    printf("  Average Execution Time: %.2f μs\n", function->avgExecutionTime);
    
    // Dump machine code bytes
    printf("  Machine Code: ");
    uint8_t* code = (uint8_t*)function->nativeCode;
    for (size_t i = 0; i < function->codeSize && i < 32; i++) {
        printf("%02X ", code[i]);
    }
    if (function->codeSize > 32) {
        printf("...");
    }
    printf("\n");
}

void enableJitProfiling() {
    // Enable profiling
}

void disableJitProfiling() {
    // Disable profiling
} 