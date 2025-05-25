//> A Virtual Machine vm-h
#ifndef gem_vm_h
#define gem_vm_h

/* A Virtual Machine vm-h < Calls and Functions vm-include-object
#include "chunk.h"
*/
//> Calls and Functions vm-include-object
#include "object.h"
//< Calls and Functions vm-include-object
//> Hash Tables vm-include-table
#include "table.h"
//< Hash Tables vm-include-table
//> vm-include-value
#include "value.h"
//< vm-include-value
//> stack-max

// Forward declaration to avoid circular includes
typedef struct JitFunction JitFunction;

//> Fast Stack Configuration
// Use a large fixed stack to eliminate bounds checking
#define FAST_STACK_SIZE (1024 * 1024)  // 1M stack slots - eliminates reallocation
#define FAST_STACK_ENABLED 1            // Enable fast stack optimizations
//< Fast Stack Configuration

//< stack-max
/* A Virtual Machine stack-max < Calls and Functions frame-max
#define STACK_MAX 256
*/
//> Calls and Functions frame-max
#define FRAMES_MAX 64
//< Calls and Functions frame-max
//> Calls and Functions call-frame

//> Inline Cache for Function Calls
#define INLINE_CACHE_SIZE 256
#define INLINE_CACHE_ENABLED 1

typedef struct InlineCache {
    ObjClosure* closure;    // Cached function pointer
    uint32_t callSiteId;    // Call site identifier
    uint32_t hitCount;      // Number of cache hits
} InlineCache;

//> Memoization Cache for Recursive Functions
#define MEMO_CACHE_SIZE 1024
#define MEMO_CACHE_ENABLED 0  // Disabled for now due to overhead

typedef struct MemoEntry {
    ObjClosure* function;   // Function being memoized
    Value argument;         // Single argument (for fibonacci)
    Value result;           // Cached result
    bool valid;             // Whether this entry is valid
} MemoEntry;
//< Memoization Cache for Recursive Functions

typedef struct {
/* Calls and Functions call-frame < Closures call-frame-closure
  ObjFunction* function;
*/
//> Closures call-frame-closure
  ObjClosure* closure;
//< Closures call-frame-closure
  uint8_t* ip;
  Value* slots;
//> Inline Cache per frame
  InlineCache* callCache;  // Inline cache for this frame
//< Inline Cache per frame
} CallFrame;
//< Calls and Functions call-frame

typedef struct {
/* A Virtual Machine vm-h < Calls and Functions frame-array
  Chunk* chunk;
*/
/* A Virtual Machine ip < Calls and Functions frame-array
  uint8_t* ip;
*/
//> Calls and Functions frame-array
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  
//< Calls and Functions frame-array
//> vm-stack
#if FAST_STACK_ENABLED
  Value fastStack[FAST_STACK_SIZE];  // Fixed-size stack for maximum performance
  Value* stackTop;                   // Only need stackTop pointer
#else
  Value* stack;                      // Dynamic stack (legacy)
  Value* stackTop;
  int stackCapacity;
#endif
//< vm-stack
//> Global Variables vm-globals
  Table globals;
//< Global Variables vm-globals
//> Hash Tables vm-strings
  Table strings;
//< Hash Tables vm-strings
//> Module System vm-modules
  Table modules;  // Track loaded modules by path
//< Module System vm-modules
//> Methods and Initializers vm-init-string
  ObjString* initString;
//< Methods and Initializers vm-init-string
//> Closures open-upvalues-field
  ObjUpvalue* openUpvalues;
//< Closures open-upvalues-field
//> Strings objects-root
  Obj* objects;
//< Strings objects-root
//> Memory Safety VM Fields
  int currentScopeDepth; // Track current scope depth for memory safety
//< Memory Safety VM Fields
//> Inline Cache global array
#if INLINE_CACHE_ENABLED  
  InlineCache globalCallCache[INLINE_CACHE_SIZE];
  uint32_t nextCallSiteId;
#endif
//> Memoization Cache global array
#if MEMO_CACHE_ENABLED
  MemoEntry memoCache[MEMO_CACHE_SIZE];
#endif
//< Memoization Cache global array
//< Inline Cache global array
} VM;

//> interpret-result
typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

//< interpret-result
//> Strings extern-vm
extern VM vm;

//< Strings extern-vm
void initVM();
void freeVM();
/* A Virtual Machine interpret-h < Scanning on Demand vm-interpret-h
InterpretResult interpret(Chunk* chunk);
*/
//> Scanning on Demand vm-interpret-h
InterpretResult interpret(const char* source);
//< Scanning on Demand vm-interpret-h
//> Module System run-h
InterpretResult run();
//< Module System run-h
//> push-pop
void push(Value value);
Value pop();

//> Fast Stack Macros
#if FAST_STACK_ENABLED
// Ultra-fast stack operations - no bounds checking, direct pointer manipulation
#define FAST_PUSH(value) (*vm.stackTop++ = (value))
#define FAST_POP() (*--vm.stackTop)
#define FAST_PEEK(distance) (vm.stackTop[-1 - (distance)])

// Only replace function calls with macros when not defining the functions themselves
#ifndef VM_INTERNAL_FUNCTIONS
#if defined(NDEBUG) || defined(OPTIMIZE_FAST_STACK)
#define push(value) FAST_PUSH(value)
#define pop() FAST_POP()
#endif
#endif
#endif
//< Fast Stack Macros
//< push-pop

#endif
