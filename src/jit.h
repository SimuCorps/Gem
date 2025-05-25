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
typedef struct RegisterAllocator RegisterAllocator;

// JIT compilation settings - optimized for performance
#define JIT_HOT_THRESHOLD 50        // Higher threshold to avoid premature compilation
#define JIT_HOT_LOOP_THRESHOLD 20   // Higher threshold for loops
#define JIT_MAX_INLINE_SIZE 100     
#define JIT_MAX_REGISTERS 16        
#define JIT_STACK_SLOTS 256         

// JIT optimization levels
typedef enum {
    JIT_OPT_NONE = 0,      
    JIT_OPT_BASIC = 1,     
    JIT_OPT_ADVANCED = 2,  
    JIT_OPT_AGGRESSIVE = 3 
} JitOptLevel;

// Register allocation state
typedef enum {
    REG_FREE = 0,
    REG_ALLOCATED = 1,
    REG_SPILLED = 2
} RegisterState;

typedef struct {
    RegisterState state;
    Value value;           
    int lastUse;          
    bool isDirty;         
} RegisterInfo;

// Register allocator
typedef struct RegisterAllocator {
    RegisterInfo registers[JIT_MAX_REGISTERS];
    int nextSpillSlot;
    int currentInstruction;
} RegisterAllocator;

// JIT function signature
typedef InterpretResult (*JitCompiledFn)(VM* vm, CallFrame* frame);

// Hot spot tracking for tiered compilation
typedef struct HotSpot {
    uint8_t* bytecode;      
    int hitCount;           
    bool isFunction;        
    bool isLoop;            
    JitOptLevel optLevel;   
    struct HotSpot* next;   
} HotSpot;

// Compiled JIT function
typedef struct JitFunction {
    uint8_t* bytecodeStart;         
    uint8_t* bytecodeEnd;           
    JitCompiledFn nativeCode;       
    size_t codeSize;                
    int callCount;                  
    JitOptLevel optLevel;           
    double avgExecutionTime;        
    bool isInlined;                 
    int localCount;                 
    int paramCount;                 
    struct JitFunction* next;       
} JitFunction;

// Blacklisted function entry
typedef struct BlacklistedFunction {
    uint8_t* bytecodeStart;         
    struct BlacklistedFunction* next; 
} BlacklistedFunction;

// JIT context
typedef struct JitContext {
    HotSpot* hotSpots;              
    JitFunction* compiledFunctions; 
    BlacklistedFunction* blacklistedFunctions; 
    bool enabled;                   
    JitOptLevel defaultOptLevel;    
    int totalCompilations;          
    int totalExecutions;            
    int totalOptimizations;         
    double totalCompileTime;        
    double totalExecutionTime;      
    RegisterAllocator* allocator;   
} JitContext;

// Global JIT context
extern JitContext jitContext;

// JIT API functions
void initJIT();
void freeJIT();

// Hot spot detection and management
void trackHotSpot(uint8_t* bytecode, bool isFunction);
void trackLoopBackEdge(uint8_t* bytecode);
bool isHotSpot(uint8_t* bytecode);
HotSpot* findHotSpot(uint8_t* bytecode);
void promoteHotSpot(HotSpot* hotSpot);

// JIT compilation
JitFunction* compileFunction(ObjClosure* closure);
JitFunction* compileFunctionWithOptLevel(ObjClosure* closure, JitOptLevel optLevel);
JitFunction* findCompiledFunction(uint8_t* bytecode);
bool shouldCompile(uint8_t* bytecode);
bool shouldRecompile(JitFunction* function);

// JIT execution
InterpretResult executeJitFunction(JitFunction* function, VM* vm, CallFrame* frame);

// Register allocation
void initRegisterAllocator(RegisterAllocator* allocator);
int allocateRegister(RegisterAllocator* allocator, Value value);
void freeRegister(RegisterAllocator* allocator, int reg);
void spillRegister(RegisterAllocator* allocator, int reg);
int findBestRegisterToSpill(RegisterAllocator* allocator);

// Optimization passes
void applyBasicOptimizations(uint8_t* bytecode, size_t length);
void applyAdvancedOptimizations(uint8_t* bytecode, size_t length);
void applyAggressiveOptimizations(uint8_t* bytecode, size_t length);

// Performance monitoring
void startJitTimer();
double stopJitTimer();
void updateExecutionStats(JitFunction* function, double executionTime);
void printJitStats();
void printDetailedJitStats();

// Blacklist management
void addToBlacklist(uint8_t* bytecode);
bool isBlacklisted(uint8_t* bytecode);
void clearBlacklist();

// Debug functions
void dumpJitFunction(JitFunction* function);
void enableJitProfiling();
void disableJitProfiling();

#endif 