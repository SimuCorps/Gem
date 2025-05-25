#ifndef gem_jit_h
#define gem_jit_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "vm.h"

// Forward declarations
typedef struct JitContext JitContext;
typedef struct JitFunction JitFunction;
typedef struct HotSpot HotSpot;

// JIT compilation settings
#define JIT_HOT_THRESHOLD 100       // Function calls before JIT compilation
#define JIT_HOT_LOOP_THRESHOLD 50   // Loop iterations before JIT compilation
#define JIT_MAX_INLINE_SIZE 50      // Maximum bytecode instructions to inline

// JIT function signature - matches VM bytecode function signature
typedef InterpretResult (*JitCompiledFn)(void);

// Hot spot tracking for tiered compilation
typedef struct HotSpot {
    uint8_t* bytecode;      // Pointer to bytecode location
    int hitCount;           // Number of times this spot was hit
    bool isFunction;        // True if this is a function entry, false if loop
    struct HotSpot* next;   // Linked list of hot spots
} HotSpot;

// Compiled JIT function
typedef struct JitFunction {
    uint8_t* bytecodeStart;         // Start of original bytecode
    uint8_t* bytecodeEnd;           // End of original bytecode
    JitCompiledFn nativeCode;       // Compiled native function pointer
    size_t codeSize;                // Size of generated machine code
    int callCount;                  // Number of times this function was called
    struct JitFunction* next;       // Linked list of compiled functions
} JitFunction;

// JIT context - contains all JIT state
typedef struct JitContext {
    HotSpot* hotSpots;              // Hot spot tracking
    JitFunction* compiledFunctions; // Compiled function cache
    bool enabled;                   // Whether JIT is enabled
    int totalCompilations;          // Statistics
    int totalExecutions;            // Statistics
} JitContext;

// Global JIT context
extern JitContext jitContext;

// JIT API functions
void initJIT();
void freeJIT();

// Hot spot detection and management
void trackHotSpot(uint8_t* bytecode, bool isFunction);
bool isHotSpot(uint8_t* bytecode);
HotSpot* findHotSpot(uint8_t* bytecode);

// JIT compilation
JitFunction* compileFunction(ObjClosure* closure);
JitFunction* findCompiledFunction(uint8_t* bytecode);
bool shouldCompile(uint8_t* bytecode);

// JIT execution
InterpretResult executeJitFunction(JitFunction* function);

// JIT statistics and debugging
void printJitStats();
void dumpJitFunction(JitFunction* function);

#endif 