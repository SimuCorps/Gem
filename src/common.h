//> Chunks of Bytecode common-h
#ifndef gem_common_h
#define gem_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
//> A Virtual Machine define-debug-trace

// Optimization flags - AGGRESSIVE PERFORMANCE MODE
#define OPTIMIZE_SKIP_TYPE_CHECKS 1      // Skip runtime type checks since we have static typing
#define OPTIMIZE_FAST_STACK 1            // Enable ultra-fast stack operations (no bounds checking)
#define OPTIMIZE_DIRECT_ARITHMETIC 1     // Direct arithmetic without value boxing/unboxing
#define OPTIMIZE_UNSAFE_ARITHMETIC 1     // Assume all arithmetic operands are numbers
#define OPTIMIZE_INLINE_LOCALS 1         // Inline local variable access
#define OPTIMIZE_BRANCH_PREDICTION 1     // Help CPU branch prediction
#define OPTIMIZE_AGGRESSIVE_INLINING 1   // Aggressive function inlining
#define OPTIMIZE_SKIP_MEMORY_SAFETY 1    // Skip memory safety checks in release builds
#define NAN_BOXING 1

//> Performance measurement flags
#ifdef NDEBUG
#define FORCE_INLINE __attribute__((always_inline)) inline
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ASSUME_ALIGNED(ptr, align) __builtin_assume_aligned(ptr, align)
#else
#define FORCE_INLINE inline
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define ASSUME_ALIGNED(ptr, align) (ptr)
#endif
//< Performance measurement flags

//> Compiling Expressions define-debug-print-code
#define DEBUG_PRINT_CODE
//< Compiling Expressions define-debug-print-code
#define DEBUG_TRACE_EXECUTION
//< A Virtual Machine define-debug-trace
//> Local Variables uint8-count

#define UINT8_COUNT (UINT8_MAX + 1)
//< Local Variables uint8-count

#endif
//> omit
// In the book, we show them defined, but for working on them locally,
// we don't want them to be.
#undef DEBUG_PRINT_CODE
#undef DEBUG_TRACE_EXECUTION
//< omit
