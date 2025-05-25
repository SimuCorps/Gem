//> A Virtual Machine vm-c
//> Types of Values include-stdarg
#include <stdarg.h>
//< Types of Values include-stdarg
//> vm-include-stdio
#include <stdio.h>
//> Strings vm-include-string
#include <string.h>
//< Strings vm-include-string
//> Calls and Functions vm-include-time
#include <time.h>
//< Calls and Functions vm-include-time
#include <math.h>
#include <stdlib.h>

//< vm-include-stdio

// Define this before including vm.h to prevent macro redefinition
#define VM_INTERNAL_FUNCTIONS 1

#include "common.h"
//> Scanning on Demand vm-include-compiler
#include "compiler.h"
//< Scanning on Demand vm-include-compiler
#include "scanner.h"
//> vm-include-debug
#include "debug.h"
//< vm-include-debug
//> Strings vm-include-object-memory
#include "object.h"
#include "memory.h"
//< Strings vm-include-object-memory
#include "vm.h"
//> JIT Integration include
#include "jit.h"
//< JIT Integration include

//> Embedded STL Modules
#ifdef WITH_STL
#include "embedded_stl.h"
#endif
//< Embedded STL Modules

//> Forward declarations
static Value peek(int distance);
//< Forward declarations

// Computed goto optimization detection
#ifdef __GNUC__
#define USE_COMPUTED_GOTO 1
#else
#define USE_COMPUTED_GOTO 0
#endif

VM vm; // [one]

//> Closure Cache for Recursive Functions
// Cache for avoiding closure recreation in recursive calls
static ObjClosure* cachedRecursiveClosure = NULL;
static ObjFunction* cachedRecursiveFunction = NULL;
//< Closure Cache for Recursive Functions

//> Calls and Functions clock-native
static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
//< Calls and Functions clock-native
//> reset-stack
#define INITIAL_STACK_CAPACITY 256

static void resetStack() {
#if FAST_STACK_ENABLED
  vm.stackTop = vm.fastStack;
#else
  vm.stackTop = vm.stack;
#endif
//> Calls and Functions reset-frame-count
  vm.frameCount = 0;
//< Calls and Functions reset-frame-count
//> Closures init-open-upvalues
  vm.openUpvalues = NULL;
//< Closures init-open-upvalues
}
//< reset-stack
//> Types of Values runtime-error
static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

/* Types of Values runtime-error < Calls and Functions runtime-error-temp
  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];
*/
/* Calls and Functions runtime-error-temp < Calls and Functions runtime-error-stack
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  size_t instruction = frame->ip - frame->function->chunk.code - 1;
  int line = frame->function->chunk.lines[instruction];
*/
/* Types of Values runtime-error < Calls and Functions runtime-error-stack
  fprintf(stderr, "[line %d] in script\n", line);
*/
//> Calls and Functions runtime-error-stack
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
/* Calls and Functions runtime-error-stack < Closures runtime-error-function
    ObjFunction* function = frame->function;
*/
//> Closures runtime-error-function
    ObjFunction* function = frame->closure->function;
//< Closures runtime-error-function
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", // [minus]
            getLine(&function->chunk, instruction));
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", AS_CSTRING(OBJ_VAL(function->name)));
    }
  }

//< Calls and Functions runtime-error-stack
  resetStack();
}
//< Types of Values runtime-error
//> Calls and Functions define-native
static void defineNative(const char* name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(peek(1)), peek(0));
  pop();
  pop();
}
//< Calls and Functions define-native

//> HTTP Native Functions
static Value httpGetNative(int argCount, Value* args) {
  if (argCount != 1) {
    runtimeError("httpGet() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  
  if (!IS_STRING(args[0])) {
    runtimeError("httpGet() argument must be a string.");
    return NIL_VAL;
  }
  
  const char* url = AS_CSTRING(args[0]);
  
  // For now, we'll use a simple system call to curl
  // In a production implementation, you'd want to use a proper HTTP library
  char command[1024];
  snprintf(command, sizeof(command), "curl -s \"%s\"", url);
  
  FILE* pipe = popen(command, "r");
  if (!pipe) {
    runtimeError("Failed to execute HTTP request.");
    return NIL_VAL;
  }
  
  // Read the response
  char* response = malloc(65536); // 64KB buffer
  if (!response) {
    pclose(pipe);
    runtimeError("Out of memory for HTTP response.");
    return NIL_VAL;
  }
  
  size_t totalRead = 0;
  size_t bytesRead;
  while ((bytesRead = fread(response + totalRead, 1, 1024, pipe)) > 0) {
    totalRead += bytesRead;
    if (totalRead >= 65535) break; // Leave space for null terminator
  }
  response[totalRead] = '\0';
  
  pclose(pipe);
  
  ObjString* result = copyString(response, (int)totalRead);
  free(response);
  
  return OBJ_VAL(result);
}

static Value httpPostNative(int argCount, Value* args) {
  if (argCount != 2) {
    runtimeError("httpPost() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  
  if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
    runtimeError("httpPost() arguments must be strings.");
    return NIL_VAL;
  }
  
  const char* url = AS_CSTRING(args[0]);
  const char* data = AS_CSTRING(args[1]);
  
  char command[2048];
  snprintf(command, sizeof(command), "curl -s -X POST -d \"%s\" \"%s\"", data, url);
  
  FILE* pipe = popen(command, "r");
  if (!pipe) {
    runtimeError("Failed to execute HTTP POST request.");
    return NIL_VAL;
  }
  
  char* response = malloc(65536);
  if (!response) {
    pclose(pipe);
    runtimeError("Out of memory for HTTP response.");
    return NIL_VAL;
  }
  
  size_t totalRead = 0;
  size_t bytesRead;
  while ((bytesRead = fread(response + totalRead, 1, 1024, pipe)) > 0) {
    totalRead += bytesRead;
    if (totalRead >= 65535) break;
  }
  response[totalRead] = '\0';
  
  pclose(pipe);
  
  ObjString* result = copyString(response, (int)totalRead);
  free(response);
  
  return OBJ_VAL(result);
}

static Value httpPutNative(int argCount, Value* args) {
  if (argCount != 2) {
    runtimeError("httpPut() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  
  if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
    runtimeError("httpPut() arguments must be strings.");
    return NIL_VAL;
  }
  
  const char* url = AS_CSTRING(args[0]);
  const char* data = AS_CSTRING(args[1]);
  
  char command[2048];
  snprintf(command, sizeof(command), "curl -s -X PUT -d \"%s\" \"%s\"", data, url);
  
  FILE* pipe = popen(command, "r");
  if (!pipe) {
    runtimeError("Failed to execute HTTP PUT request.");
    return NIL_VAL;
  }
  
  char* response = malloc(65536);
  if (!response) {
    pclose(pipe);
    runtimeError("Out of memory for HTTP response.");
    return NIL_VAL;
  }
  
  size_t totalRead = 0;
  size_t bytesRead;
  while ((bytesRead = fread(response + totalRead, 1, 1024, pipe)) > 0) {
    totalRead += bytesRead;
    if (totalRead >= 65535) break;
  }
  response[totalRead] = '\0';
  
  pclose(pipe);
  
  ObjString* result = copyString(response, (int)totalRead);
  free(response);
  
  return OBJ_VAL(result);
}

static Value httpDeleteNative(int argCount, Value* args) {
  if (argCount != 1) {
    runtimeError("httpDelete() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  
  if (!IS_STRING(args[0])) {
    runtimeError("httpDelete() argument must be a string.");
    return NIL_VAL;
  }
  
  const char* url = AS_CSTRING(args[0]);
  
  char command[1024];
  snprintf(command, sizeof(command), "curl -s -X DELETE \"%s\"", url);
  
  FILE* pipe = popen(command, "r");
  if (!pipe) {
    runtimeError("Failed to execute HTTP DELETE request.");
    return NIL_VAL;
  }
  
  char* response = malloc(65536);
  if (!response) {
    pclose(pipe);
    runtimeError("Out of memory for HTTP response.");
    return NIL_VAL;
  }
  
  size_t totalRead = 0;
  size_t bytesRead;
  while ((bytesRead = fread(response + totalRead, 1, 1024, pipe)) > 0) {
    totalRead += bytesRead;
    if (totalRead >= 65535) break;
  }
  response[totalRead] = '\0';
  
  pclose(pipe);
  
  ObjString* result = copyString(response, (int)totalRead);
  free(response);
  
  return OBJ_VAL(result);
}
//< HTTP Native Functions

void initVM() {
#if FAST_STACK_ENABLED
  // Fast stack is pre-allocated, just reset the pointer
  resetStack();
#else
//> Initialize dynamic stack
  vm.stack = ALLOCATE(Value, INITIAL_STACK_CAPACITY);
  vm.stackCapacity = INITIAL_STACK_CAPACITY;
//< Initialize dynamic stack
//> call-reset-stack
  resetStack();
//< call-reset-stack
#endif
//> Strings init-objects-root
  vm.objects = NULL;
//< Strings init-objects-root
//> Global Variables init-globals

  initTable(&vm.globals);
//< Global Variables init-globals
//> Hash Tables init-strings
  initTable(&vm.strings);
//< Hash Tables init-strings
//> Methods and Initializers init-init-string

//> null-init-string
  vm.initString = NULL;
//< null-init-string
  vm.initString = copyString("init", 4);
//< Methods and Initializers init-init-string
//> Calls and Functions define-native-clock

  defineNative("clock", clockNative);
//< Calls and Functions define-native-clock
//> HTTP Native Functions define
  defineNative("httpGet", httpGetNative);
  defineNative("httpPost", httpPostNative);
  defineNative("httpPut", httpPutNative);
  defineNative("httpDelete", httpDeleteNative);
//< HTTP Native Functions define
//> Initialize Compiler Tables
  initCompilerTables();
//< Initialize Compiler Tables
//> Memory Safety VM Init
  vm.currentScopeDepth = 0;
//< Memory Safety VM Init
//> Initialize Closure Cache
  // Initialize closure cache for recursive function optimization
  cachedRecursiveClosure = NULL;
  cachedRecursiveFunction = NULL;
//< Initialize Closure Cache
//> JIT Integration init
  // initJIT(); // Disabled - pure overhead without actual JIT execution
//< JIT Integration init
}

void freeVM() {
//> JIT Integration free
  // freeJIT(); // Disabled - pure overhead without actual JIT execution
//< JIT Integration free
#if !FAST_STACK_ENABLED
//> Free dynamic stack
  FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
//< Free dynamic stack
#endif
//> Global Variables free-globals
  freeTable(&vm.globals);
//< Global Variables free-globals
//> Hash Tables free-strings
  freeTable(&vm.strings);
//< Hash Tables free-strings
//> Methods and Initializers clear-init-string
  vm.initString = NULL;
//< Methods and Initializers clear-init-string
//> Strings call-free-objects
  freeObjects();
//< Strings call-free-objects
}
//> push
void push(Value value) {
#if FAST_STACK_ENABLED
  // Fast stack - no bounds checking in release builds
  #ifdef DEBUG
  // Only check bounds in debug builds
  if (vm.stackTop - vm.fastStack >= FAST_STACK_SIZE) {
    fprintf(stderr, "Stack overflow - increase FAST_STACK_SIZE\n");
    exit(74);
  }
  #endif
  *vm.stackTop++ = value;
#else
  // Legacy dynamic stack with bounds checking
  // Check if we need to grow the stack
  int currentSize = (int)(vm.stackTop - vm.stack);
  if (currentSize >= vm.stackCapacity) {
    int oldCapacity = vm.stackCapacity;
    vm.stackCapacity = GROW_CAPACITY(oldCapacity);
    
    // Save old stack pointer and slots offsets
    Value* oldStack = vm.stack;
    int stackTopOffset = currentSize;
    
    // Save frame slot offsets before reallocation
    int slotOffsets[FRAMES_MAX];
    for (int i = 0; i < vm.frameCount; i++) {
      slotOffsets[i] = (int)(vm.frames[i].slots - oldStack);
    }
    
    // Reallocate the stack
    vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
    
    // Restore stackTop pointer
    vm.stackTop = vm.stack + stackTopOffset;
    
    // Update all frame slots pointers since the stack has moved
    for (int i = 0; i < vm.frameCount; i++) {
      vm.frames[i].slots = vm.stack + slotOffsets[i];
    }
  }
  
  *vm.stackTop = value;
  vm.stackTop++;
#endif
}
//< push
//> pop
Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}
//< pop
//> Types of Values peek
static Value peek(int distance) {
#if FAST_STACK_ENABLED && (defined(NDEBUG) || defined(OPTIMIZE_FAST_STACK))
  // Ultra-fast peek - no bounds checking
  return vm.stackTop[-1 - distance];
#else
  // Safe peek with bounds checking
  return vm.stackTop[-1 - distance];
#endif
}
//< Types of Values peek
/* Calls and Functions call < Closures call-signature
static bool call(ObjFunction* function, int argCount) {
*/
//> Calls and Functions call
//> Closures call-signature
static bool call(ObjClosure* closure, int argCount) {
//< Closures call-signature
/* Calls and Functions check-arity < Closures check-arity
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        function->arity, argCount);
*/
//> Closures check-arity
#if !OPTIMIZE_SKIP_TYPE_CHECKS
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        closure->function->arity, argCount);
//< Closures check-arity
//> check-arity
    return false;
  }
#endif

//< check-arity

//> check-overflow
#if !OPTIMIZE_SKIP_TYPE_CHECKS
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }
#endif

//< check-overflow
  {
    CallFrame* frame = &vm.frames[vm.frameCount++];
/* Calls and Functions call < Closures call-init-closure
  frame->function = function;
  frame->ip = function->chunk.code;
*/
//> Closures call-init-closure
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
//< Closures call-init-closure
    frame->slots = vm.stackTop - argCount - 1;
  }
  return true;
}
//< Calls and Functions call
//> Calls and Functions call-value
static bool callValue(Value callee, int argCount) {
  // Fast path for closures (most common case in recursive functions)
  if (LIKELY(IS_OBJ(callee) && OBJ_TYPE(callee) == OBJ_CLOSURE)) {
    return call(AS_CLOSURE(callee), argCount);
  }
  
  // Slower path for other object types
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
//> Methods and Initializers call-bound-method
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
//> store-receiver
        vm.stackTop[-argCount - 1] = bound->receiver;
//< store-receiver
        return call(bound->method, argCount);
      }
//< Methods and Initializers call-bound-method
//> Classes and Instances call-class
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
//> Methods and Initializers call-init
        Value initializer;
        if (tableGet(&klass->methods, vm.initString,
                     &initializer)) {
          return call(AS_CLOSURE(initializer), argCount);
//> no-init-arity-error
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d.",
                       argCount);
          return false;
//< no-init-arity-error
        }
//< Methods and Initializers call-init
        return true;
      }
//< Classes and Instances call-class
//> Closures call-value-closure
      case OBJ_CLOSURE:
        // Already handled in fast path above
        return call(AS_CLOSURE(callee), argCount);
//< Closures call-value-closure
/* Calls and Functions call-value < Closures call-value-closure
      case OBJ_FUNCTION: // [switch]
        return call(AS_FUNCTION(callee), argCount);
*/
//> call-native
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
//< call-native
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}
//< Calls and Functions call-value
//> Methods and Initializers invoke-from-class
static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", AS_CSTRING(OBJ_VAL(name)));
    return false;
  }
  return call(AS_CLOSURE(method), argCount);
}
//< Methods and Initializers invoke-from-class
//> Methods and Initializers invoke
static bool invoke(ObjString* name, int argCount) {
  Value receiver = peek(argCount);
//> invoke-check-type

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }

//< invoke-check-type
  ObjInstance* instance = AS_INSTANCE(receiver);
//> invoke-field

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

//< invoke-field
  return invokeFromClass(instance->klass, name, argCount);
}
//< Methods and Initializers invoke
//> Methods and Initializers bind-method
static bool bindMethod(ObjClass* klass, ObjString* name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", AS_CSTRING(OBJ_VAL(name)));
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(peek(0),
                                         AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}
//< Methods and Initializers bind-method
//> Closures capture-upvalue
static ObjUpvalue* captureUpvalue(Value* local) {
//> look-for-existing-upvalue
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

//< look-for-existing-upvalue
  ObjUpvalue* createdUpvalue = newUpvalue(local);
//> insert-upvalue-in-list
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

//< insert-upvalue-in-list
  return createdUpvalue;
}
//< Closures capture-upvalue
//> Closures close-upvalues
static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}
//< Closures close-upvalues
//> Methods and Initializers define-method
static void defineMethod(ObjString* name) {
  Value method = peek(0);
  ObjClass* klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}
//< Methods and Initializers define-method
//> Types of Values is-falsey
static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}
//< Types of Values is-falsey
//> Strings concatenate
static void concatenate() {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, AS_CSTRING(OBJ_VAL(a)), a->length);
  memcpy(chars + a->length, AS_CSTRING(OBJ_VAL(b)), b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}
//< Strings concatenate

//> Memory Safety VM Functions
// Memory safety functions for VM
static bool checkObjectAccess(Value value, const char* operation) {
  if (!IS_OBJ(value)) {
    return true; // Not an object, access is safe
  }
  
  Obj* obj = AS_OBJ(value);
  if (isObjectDropped(obj)) {
    runtimeError("Cannot %s dropped object.", operation);
    return false;
  }
  
  return true;
}

static bool checkObjectMutation(Value value, const char* operation) {
  if (!IS_OBJ(value)) {
    return true; // Not an object, mutation is safe
  }
  
  Obj* obj = AS_OBJ(value);
  if (isObjectDropped(obj)) {
    runtimeError("Cannot %s dropped object.", operation);
    return false;
  }
  
  // For now, we'll allow mutations but this is where we could add
  // exclusive borrow checking in the future
  return true;
}
//< Memory Safety VM Functions

//> run
InterpretResult run() {
//> Calls and Functions run
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

/* A Virtual Machine run < Calls and Functions run
#define READ_BYTE() (*vm.ip++)
*/
#define READ_BYTE() (*frame->ip++)
/* A Virtual Machine read-constant < Calls and Functions run
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
*/

/* Jumping Back and Forth read-short < Calls and Functions run
#define READ_SHORT() \
    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
*/
#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

/* Calls and Functions run < Closures read-constant
#define READ_CONSTANT() \
    (frame->function->chunk.constants.values[READ_BYTE()])
*/
//> Closures read-constant
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_SHORT()])
//< Closures read-constant

//< Calls and Functions run
//> Global Variables read-string
#define READ_STRING() AS_STRING(READ_CONSTANT())
//< Global Variables read-string
/* A Virtual Machine binary-op < Types of Values binary-op
#define BINARY_OP(op) \
    do { \
      double b = pop(); \
      double a = pop(); \
      push(a op b); \
    } while (false)
*/
//> Types of Values binary-op
#if OPTIMIZE_SKIP_TYPE_CHECKS
// Optimized version without redundant type checks (static typing guarantees types)
#define BINARY_OP(valueType, op) \
    do { \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)
#else
// Original version with runtime type checks for debugging
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)
#endif
//< Types of Values binary-op

//> Ultra-Fast Arithmetic Operations
#if OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG)
// Ultra-fast arithmetic that bypasses all value manipulation overhead
// WARNING: These are UNSAFE and assume NaN boxing and valid numeric inputs
#define ULTRA_FAST_ADD() \
    do { \
      vm.stackTop -= 2; \
      double a = valueToNum(vm.stackTop[0]); \
      double b = valueToNum(vm.stackTop[1]); \
      vm.stackTop[0] = numToValue(a + b); \
      vm.stackTop++; \
    } while (false)

#define ULTRA_FAST_SUB() \
    do { \
      vm.stackTop -= 2; \
      double a = valueToNum(vm.stackTop[0]); \
      double b = valueToNum(vm.stackTop[1]); \
      vm.stackTop[0] = numToValue(a - b); \
      vm.stackTop++; \
    } while (false)

#define ULTRA_FAST_MUL() \
    do { \
      vm.stackTop -= 2; \
      double a = valueToNum(vm.stackTop[0]); \
      double b = valueToNum(vm.stackTop[1]); \
      vm.stackTop[0] = numToValue(a * b); \
      vm.stackTop++; \
    } while (false)

#define ULTRA_FAST_DIV() \
    do { \
      vm.stackTop -= 2; \
      double a = valueToNum(vm.stackTop[0]); \
      double b = valueToNum(vm.stackTop[1]); \
      vm.stackTop[0] = numToValue(a / b); \
      vm.stackTop++; \
    } while (false)

//> Even faster arithmetic using direct memory manipulation
#define HYPER_FAST_ADD() \
    do { \
      vm.stackTop--; \
      double* a = (double*)(vm.stackTop - 1); \
      double* b = (double*)vm.stackTop; \
      *a = *a + *b; \
    } while (false)

#define HYPER_FAST_SUB() \
    do { \
      vm.stackTop--; \
      double* a = (double*)(vm.stackTop - 1); \
      double* b = (double*)vm.stackTop; \
      *a = *a - *b; \
    } while (false)

#define HYPER_FAST_MUL() \
    do { \
      vm.stackTop--; \
      double* a = (double*)(vm.stackTop - 1); \
      double* b = (double*)vm.stackTop; \
      *a = *a * *b; \
    } while (false)

//> Extreme arithmetic optimization - bypass all overhead
#define EXTREME_FAST_ADD() \
    do { \
      --vm.stackTop; \
      *(double*)(vm.stackTop - 1) += *(double*)vm.stackTop; \
    } while (false)

#define EXTREME_FAST_SUB() \
    do { \
      --vm.stackTop; \
      *(double*)(vm.stackTop - 1) -= *(double*)vm.stackTop; \
    } while (false)

#define EXTREME_FAST_MUL() \
    do { \
      --vm.stackTop; \
      *(double*)(vm.stackTop - 1) *= *(double*)vm.stackTop; \
    } while (false)
//< Extreme arithmetic optimization - bypass all overhead
#endif
//< Ultra-Fast Arithmetic Operations

#if USE_COMPUTED_GOTO

  // Dispatch table for computed goto optimization
  static void* dispatchTable[] = {
    [OP_CONSTANT] = &&op_constant,
    [OP_CONSTANT_LONG] = &&op_constant_long,
    [OP_NIL] = &&op_nil,
    [OP_TRUE] = &&op_true,
    [OP_FALSE] = &&op_false,
    [OP_POP] = &&op_pop,
    [OP_GET_LOCAL] = &&op_get_local,
    [OP_SET_LOCAL] = &&op_set_local,
    [OP_GET_GLOBAL] = &&op_get_global,
    [OP_DEFINE_GLOBAL] = &&op_define_global,
    [OP_SET_GLOBAL] = &&op_set_global,
    [OP_GET_UPVALUE] = &&op_get_upvalue,
    [OP_SET_UPVALUE] = &&op_set_upvalue,
    [OP_GET_PROPERTY] = &&op_get_property,
    [OP_SET_PROPERTY] = &&op_set_property,
    [OP_GET_INDEX] = &&op_get_index,
    [OP_SET_INDEX] = &&op_set_index,
    [OP_HASH_LITERAL] = &&op_hash_literal,
    [OP_GET_SUPER] = &&op_get_super,
    [OP_EQUAL] = &&op_equal,
    [OP_GREATER] = &&op_greater,
    [OP_LESS] = &&op_less,
    [OP_ADD] = &&op_add,
    [OP_ADD_NUMBER] = &&op_add_number,
    [OP_ADD_STRING] = &&op_add_string,
    [OP_SUBTRACT] = &&op_subtract,
    [OP_SUBTRACT_NUMBER] = &&op_subtract_number,
    [OP_MULTIPLY] = &&op_multiply,
    [OP_MULTIPLY_NUMBER] = &&op_multiply_number,
    [OP_DIVIDE] = &&op_divide,
    [OP_DIVIDE_NUMBER] = &&op_divide_number,
    [OP_MODULO] = &&op_modulo,
    [OP_MODULO_NUMBER] = &&op_modulo_number,
    [OP_NOT] = &&op_not,
    [OP_NEGATE] = &&op_negate,
    [OP_NEGATE_NUMBER] = &&op_negate_number,
    [OP_INTERPOLATE] = &&op_interpolate,
    [OP_PRINT] = &&op_print,
    [OP_REQUIRE] = &&op_require,
    [OP_JUMP] = &&op_jump,
    [OP_JUMP_IF_FALSE] = &&op_jump_if_false,
    [OP_LOOP] = &&op_loop,
    [OP_CALL] = &&op_call,
    [OP_INVOKE] = &&op_invoke,
    [OP_SUPER_INVOKE] = &&op_super_invoke,
    [OP_CLOSURE] = &&op_closure,
    [OP_CLOSE_UPVALUE] = &&op_close_upvalue,
    [OP_RETURN] = &&op_return,
    [OP_CLASS] = &&op_class,
    [OP_MODULE] = &&op_module,
    [OP_MODULE_METHOD] = &&op_module_method,
    [OP_MODULE_CALL] = &&op_module_call,
    [OP_INHERIT] = &&op_inherit,
    [OP_METHOD] = &&op_method,
    [OP_TYPE_CAST] = &&op_type_cast,
  };

#define DISPATCH() \
  do { \
    goto *dispatchTable[READ_BYTE()]; \
  } while (false)

#ifdef DEBUG_TRACE_EXECUTION
#define TRACE() \
  do { \
    printf("          "); \
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) { \
      printf("[ "); \
      printValue(*slot); \
      printf(" ]"); \
    } \
    printf("\n"); \
    disassembleInstruction(&frame->closure->function->chunk, \
        (int)(frame->ip - frame->closure->function->chunk.code - 1)); \
  } while (false)
#else
#define TRACE() do {} while (false)
#endif

//> Branch Prediction Optimization
#if OPTIMIZE_BRANCH_PREDICTION && defined(NDEBUG)
#define LIKELY_DISPATCH() \
  do { \
    uint8_t instruction = READ_BYTE(); \
    if (LIKELY(instruction <= OP_DIVIDE_NUMBER)) { \
      goto *dispatchTable[instruction]; \
    } else { \
      goto *dispatchTable[instruction]; \
    } \
  } while (false)
#else
#define LIKELY_DISPATCH() DISPATCH()
#endif
//< Branch Prediction Optimization

  LIKELY_DISPATCH();

op_constant: {
  TRACE();
  Value constant = frame->closure->function->chunk.constants.values[READ_BYTE()];
  push(constant);
  DISPATCH();
}

op_constant_long: {
  TRACE();
  uint32_t constant = (READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE();
  push(frame->closure->function->chunk.constants.values[constant]);
  DISPATCH();
}

op_nil: {
  TRACE();
  push(NIL_VAL);
  DISPATCH();
}

op_true: {
  TRACE();
  push(BOOL_VAL(true));
  DISPATCH();
}

op_false: {
  TRACE();
  push(BOOL_VAL(false));
  DISPATCH();
}

op_pop: {
  TRACE();
  pop();
  DISPATCH();
}

op_get_local: {
  TRACE();
  uint8_t slot = READ_BYTE();
#if OPTIMIZE_INLINE_LOCALS && defined(NDEBUG)
  // Ultra-fast local access - direct slot access without function call overhead
  *vm.stackTop++ = frame->slots[slot];
#else
  push(frame->slots[slot]);
#endif
  LIKELY_DISPATCH();
}

op_set_local: {
  TRACE();
  uint8_t slot = READ_BYTE();
#if OPTIMIZE_INLINE_LOCALS && defined(NDEBUG)
  // Ultra-fast local assignment - direct slot access
  frame->slots[slot] = vm.stackTop[-1];
#else
  frame->slots[slot] = peek(0);
#endif
  LIKELY_DISPATCH();
}

op_get_global: {
  TRACE();
  ObjString* name = READ_STRING();
  Value value;
  if (!tableGet(&vm.globals, name, &value)) {
    runtimeError("Undefined variable '%s'.", AS_CSTRING(OBJ_VAL(name)));
    return INTERPRET_RUNTIME_ERROR;
  }
  push(value);
  DISPATCH();
}

op_define_global: {
  TRACE();
  ObjString* name = READ_STRING();
  tableSet(&vm.globals, name, peek(0));
  pop();
  DISPATCH();
}

op_set_global: {
  TRACE();
  ObjString* name = READ_STRING();
  if (tableSet(&vm.globals, name, peek(0))) {
    tableDelete(&vm.globals, name);
    runtimeError("Undefined variable '%s'.", AS_CSTRING(OBJ_VAL(name)));
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

op_get_upvalue: {
  TRACE();
  uint8_t slot = READ_BYTE();
  push(*frame->closure->upvalues[slot]->location);
  DISPATCH();
}

op_set_upvalue: {
  TRACE();
  uint8_t slot = READ_BYTE();
  *frame->closure->upvalues[slot]->location = peek(0);
  DISPATCH();
}

op_get_property: {
  TRACE();
//> get-not-instance
#if !OPTIMIZE_SKIP_TYPE_CHECKS
  if (!IS_INSTANCE(peek(0))) {
    runtimeError("Only instances have properties.");
    return INTERPRET_RUNTIME_ERROR;
  }
#endif

//< get-not-instance
#if !OPTIMIZE_SKIP_MEMORY_SAFETY
  // Check if the instance is still valid
  if (!checkObjectAccess(peek(0), "access property of")) {
    return INTERPRET_RUNTIME_ERROR;
  }
#endif
  
  ObjInstance* instance = AS_INSTANCE(peek(0));
  ObjString* name = READ_STRING();
  
  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    pop(); // Instance.
    push(value);
    DISPATCH();
  }

//< get-undefined
  if (!bindMethod(instance->klass, name)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

op_set_property: {
  TRACE();
//> set-not-instance
#if !OPTIMIZE_SKIP_TYPE_CHECKS
  if (!IS_INSTANCE(peek(1))) {
    runtimeError("Only instances have fields.");
    return INTERPRET_RUNTIME_ERROR;
  }
#endif

//< set-not-instance
#if !OPTIMIZE_SKIP_MEMORY_SAFETY
  // Check if the instance is still valid for mutation
  if (!checkObjectMutation(peek(1), "set property of")) {
    return INTERPRET_RUNTIME_ERROR;
  }
#endif
  
  ObjInstance* instance = AS_INSTANCE(peek(1));
  ObjString* name = READ_STRING();
  tableSet(&instance->fields, name, peek(0));
  Value value = pop();
  pop();
  push(value);
  DISPATCH();
}

op_get_super: {
  TRACE();
  ObjString* name = READ_STRING();
  ObjClass* superclass = AS_CLASS(pop());
  
  if (!bindMethod(superclass, name)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

op_equal: {
  TRACE();
  Value b = pop();
  Value a = pop();
  push(BOOL_VAL(valuesEqual(a, b)));
  DISPATCH();
}

op_greater: {
  TRACE();
  BINARY_OP(BOOL_VAL, >);
  DISPATCH();
}

op_less: {
  TRACE();
  BINARY_OP(BOOL_VAL, <);
  DISPATCH();
}

op_add: {
  TRACE();
#if OPTIMIZE_SKIP_TYPE_CHECKS
  // In optimized mode, compiler should emit specific opcodes instead of generic OP_ADD
  runtimeError("Generic OP_ADD used in optimized mode - compiler error.");
  return INTERPRET_RUNTIME_ERROR;
#else
  if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
    concatenate();
  } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
    double b = AS_NUMBER(pop());
    double a = AS_NUMBER(pop());
    push(NUMBER_VAL(a + b));
  } else {
    runtimeError(
        "Operands must be two numbers or two strings.");
    return INTERPRET_RUNTIME_ERROR;
  }
#endif
  DISPATCH();
}

op_add_number: {
  // Optimized numeric addition - no type check needed
#if OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG) && NAN_BOXING
  EXTREME_FAST_ADD();
#elif OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG)
  ULTRA_FAST_ADD();
#else
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  push(NUMBER_VAL(a + b));
#endif
  LIKELY_DISPATCH();
}

op_add_string: {
  TRACE();
  // Optimized string concatenation - no type check needed
  concatenate();
  DISPATCH();
}

op_subtract: {
  TRACE();
  BINARY_OP(NUMBER_VAL, -);
  DISPATCH();
}

op_subtract_number: {
  // Optimized numeric subtraction - no type check needed
#if OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG) && NAN_BOXING
  EXTREME_FAST_SUB();
#elif OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG)
  ULTRA_FAST_SUB();
#else
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  push(NUMBER_VAL(a - b));
#endif
  LIKELY_DISPATCH();
}

op_multiply: {
  TRACE();
  BINARY_OP(NUMBER_VAL, *);
  DISPATCH();
}

op_multiply_number: {
  // Optimized numeric multiplication - no type check needed
#if OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG) && NAN_BOXING
  EXTREME_FAST_MUL();
#elif OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG)
  ULTRA_FAST_MUL();
#else
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  push(NUMBER_VAL(a * b));
#endif
  LIKELY_DISPATCH();
}

op_divide: {
  TRACE();
  BINARY_OP(NUMBER_VAL, /);
  DISPATCH();
}

op_divide_number: {
  TRACE();
  // Optimized numeric division - no type check needed
#if OPTIMIZE_DIRECT_ARITHMETIC && defined(NDEBUG)
  ULTRA_FAST_DIV();
#else
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  push(NUMBER_VAL(a / b));
#endif
  DISPATCH();
}

op_modulo: {
  TRACE();
#if OPTIMIZE_SKIP_TYPE_CHECKS
  // Optimized version without type checks
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  if (b == 0.0) {
    runtimeError("Modulo by zero.");
    return INTERPRET_RUNTIME_ERROR;
  }
  push(NUMBER_VAL(fmod(a, b)));
#else
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
    runtimeError("Operands must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  if (b == 0.0) {
    runtimeError("Modulo by zero.");
    return INTERPRET_RUNTIME_ERROR;
  }
  push(NUMBER_VAL(fmod(a, b)));
#endif
  DISPATCH();
}

op_modulo_number: {
  TRACE();
  // Optimized numeric modulo - no type check needed
  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());
  if (b == 0.0) {
    runtimeError("Modulo by zero.");
    return INTERPRET_RUNTIME_ERROR;
  }
  push(NUMBER_VAL(fmod(a, b)));
  DISPATCH();
}

op_not: {
  TRACE();
  push(BOOL_VAL(isFalsey(pop())));
  DISPATCH();
}

op_negate: {
  TRACE();
#if OPTIMIZE_SKIP_TYPE_CHECKS
  // In optimized mode, compiler should emit OP_NEGATE_NUMBER instead
  runtimeError("Generic OP_NEGATE used in optimized mode - compiler error.");
  return INTERPRET_RUNTIME_ERROR;
#else
  if (!IS_NUMBER(peek(0))) {
    runtimeError("Operand must be a number.");
    return INTERPRET_RUNTIME_ERROR;
  }
  push(NUMBER_VAL(-AS_NUMBER(pop())));
#endif
  DISPATCH();
}

op_negate_number: {
  TRACE();
  // Optimized numeric negation - no type check needed
  push(NUMBER_VAL(-AS_NUMBER(pop())));
  DISPATCH();
}

op_print: {
  TRACE();
  printValue(pop());
  printf("\n");
  DISPATCH();
}

op_require: {
  TRACE();
  ObjString* filename = AS_STRING(pop());
  const char* filenameStr = AS_CSTRING(OBJ_VAL(filename));
  
  char* buffer = NULL;
  bool isEmbedded = false;
  
#ifdef WITH_STL
  // First, try to find the module in embedded STL
  const char* embeddedSource = getEmbeddedSTLModule(filenameStr);
  if (embeddedSource != NULL) {
    // Use embedded STL module
    size_t sourceLen = strlen(embeddedSource);
    buffer = (char*)malloc(sourceLen + 1);
    if (buffer == NULL) {
      runtimeError("Not enough memory to load embedded module \"%s\".", filenameStr);
      return INTERPRET_RUNTIME_ERROR;
    }
    strcpy(buffer, embeddedSource);
    isEmbedded = true;
  }
#endif
  
  // If not found in embedded STL, try to read from file
  if (buffer == NULL) {
    FILE* file = NULL;
    char* fullPath = NULL;
    
    // First try to open the file as specified
    file = fopen(filenameStr, "rb");
    
#ifdef WITH_STL
    // If file not found and we have STL support, try the standard library path
    if (file == NULL && STL_PATH != NULL) {
      // Check if the filename contains a path separator - if not, it might be a standard library module
      if (strchr(filenameStr, '/') == NULL && strchr(filenameStr, '\\') == NULL) {
        // Construct path: STL_PATH/filename.gem
        size_t stlPathLen = strlen(STL_PATH);
        size_t filenameLen = strlen(filenameStr);
        size_t fullPathLen = stlPathLen + 1 + filenameLen + 4 + 1; // +1 for '/', +4 for '.gem', +1 for '\0'
        
        fullPath = (char*)malloc(fullPathLen);
        if (fullPath != NULL) {
          snprintf(fullPath, fullPathLen, "%s/%s.gem", STL_PATH, filenameStr);
          file = fopen(fullPath, "rb");
        }
      }
    }
#endif
    
    if (file == NULL) {
      if (fullPath != NULL) {
        free(fullPath);
      }
      runtimeError("Could not open file \"%s\".", filenameStr);
      return INTERPRET_RUNTIME_ERROR;
    }
    
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    
    buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
      runtimeError("Not enough memory to read \"%s\".", filenameStr);
      fclose(file);
      if (fullPath != NULL) {
        free(fullPath);
      }
      return INTERPRET_RUNTIME_ERROR;
    }
    
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
      runtimeError("Could not read file \"%s\".", filenameStr);
      free(buffer);
      fclose(file);
      if (fullPath != NULL) {
        free(fullPath);
      }
      return INTERPRET_RUNTIME_ERROR;
    }
    
    buffer[bytesRead] = '\0';
    fclose(file);
    
    // Clean up path
    if (fullPath != NULL) {
      free(fullPath);
    }
  }
  
  // Compile and execute the module
  ObjFunction* function = compile(buffer);
  free(buffer);
  
  if (function == NULL) {
    runtimeError("Failed to compile \"%s\".", filenameStr);
    return INTERPRET_RUNTIME_ERROR;
  }
  
  // Create closure for the module
  ObjClosure* closure = newClosure(function);
  push(OBJ_VAL(closure));
  
  // Execute the module in the current context
  if (!call(closure, 0)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  
  // Update frame pointer
  frame = &vm.frames[vm.frameCount - 1];
  DISPATCH();
}

op_jump: {
  TRACE();
  uint16_t offset = READ_SHORT();
  frame->ip += offset;
  DISPATCH();
}

op_jump_if_false: {
  TRACE();
  uint16_t offset = READ_SHORT();
  if (isFalsey(peek(0))) frame->ip += offset;
  DISPATCH();
}

op_loop: {
  TRACE();
  uint16_t offset = READ_SHORT();
  
  // Removed JIT tracking for performance
  frame->ip -= offset;
  DISPATCH();
}

op_call: {
  int argCount = READ_BYTE();
  if (!callValue(peek(argCount), argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &vm.frames[vm.frameCount - 1];
  DISPATCH();
}

op_invoke: {
  ObjString* method = READ_STRING();
  int argCount = READ_BYTE();
  if (!invoke(method, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &vm.frames[vm.frameCount - 1];
  DISPATCH();
}

op_super_invoke: {
  ObjString* method = READ_STRING();
  int argCount = READ_BYTE();
  ObjClass* superclass = AS_CLASS(pop());
  if (!invokeFromClass(superclass, method, argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &vm.frames[vm.frameCount - 1];
  DISPATCH();
}

op_closure: {
  ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
  
  // Optimization: Reuse closure for recursive functions without upvalues
  if (function->upvalueCount == 0 && cachedRecursiveFunction == function && cachedRecursiveClosure != NULL) {
    // Reuse the existing closure for this function
    push(OBJ_VAL(cachedRecursiveClosure));
    DISPATCH();
  }
  
  ObjClosure* closure = newClosure(function);
  push(OBJ_VAL(closure));
  
  // Cache this closure if it has no upvalues (good candidate for reuse)
  if (function->upvalueCount == 0) {
    cachedRecursiveFunction = function;
    cachedRecursiveClosure = closure;
  }
  
  for (int i = 0; i < closure->upvalueCount; i++) {
    uint8_t isLocal = READ_BYTE();
    uint8_t index = READ_BYTE();
    if (isLocal) {
      closure->upvalues[i] =
          captureUpvalue(frame->slots + index);
    } else {
      closure->upvalues[i] = frame->closure->upvalues[index];
    }
  }
  DISPATCH();
}

op_close_upvalue: {
  closeUpvalues(vm.stackTop - 1);
  pop();
  DISPATCH();
}

op_return: {
  Value result = pop();
  
  closeUpvalues(frame->slots);
  vm.frameCount--;
  if (vm.frameCount == 0) {
    pop();
    return INTERPRET_OK;
  }

  vm.stackTop = frame->slots;
  push(result);
  frame = &vm.frames[vm.frameCount - 1];
  DISPATCH();
}

op_class: {
  TRACE();
  push(OBJ_VAL(newClass(READ_STRING())));
  DISPATCH();
}

op_module: {
  TRACE();
  push(OBJ_VAL(newModule(READ_STRING())));
  DISPATCH();
}

op_module_method: {
  TRACE();
  ObjString* methodName = READ_STRING();
  ObjClosure* method = AS_CLOSURE(peek(0));
  ObjModule* module = AS_MODULE(peek(1));
  
  tableSet(&module->functions, methodName, OBJ_VAL(method));
  pop(); // Method closure
  DISPATCH();
}

op_module_call: {
  TRACE();
  ObjString* methodName = READ_STRING();
  int argCount = READ_BYTE();
  
  Value moduleValue = peek(argCount);
  if (!IS_MODULE(moduleValue)) {
    runtimeError("Can only call methods on modules.");
    return INTERPRET_RUNTIME_ERROR;
  }
  
  ObjModule* module = AS_MODULE(moduleValue);
  Value method;
  if (!tableGet(&module->functions, methodName, &method)) {
    runtimeError("Undefined method '%s' in module '%s'.", 
                 AS_CSTRING(OBJ_VAL(methodName)), 
                 AS_CSTRING(OBJ_VAL(module->name)));
    return INTERPRET_RUNTIME_ERROR;
  }
  
  if (!IS_CLOSURE(method)) {
    runtimeError("Module method is not a function.");
    return INTERPRET_RUNTIME_ERROR;
  }
  
  // Replace the module on the stack with the method
  vm.stackTop[-argCount - 1] = method;
  
  if (!call(AS_CLOSURE(method), argCount)) {
    return INTERPRET_RUNTIME_ERROR;
  }
  frame = &vm.frames[vm.frameCount - 1];
  DISPATCH();
}

op_inherit: {
  TRACE();
  Value superclass = peek(1);
  if (!IS_CLASS(superclass)) {
    runtimeError("Superclass must be a class.");
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjClass* subclass = AS_CLASS(peek(0));
  tableAddAll(&AS_CLASS(superclass)->methods,
              &subclass->methods);
  pop(); // Subclass.
  DISPATCH();
}

op_method: {
  TRACE();
  defineMethod(READ_STRING());
  DISPATCH();
}

op_interpolate: {
  TRACE();
  int partCount = READ_BYTE();
  
  // Calculate total length needed
  int totalLength = 0;
  Value* parts = vm.stackTop - partCount;
  
  for (int i = 0; i < partCount; i++) {
    Value part = parts[i];
    if (IS_STRING(part)) {
      totalLength += AS_STRING(part)->length;
    } else if (IS_NUMBER(part)) {
      // Convert number to string to get length
      char buffer[32];
      int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(part));
      totalLength += len;
    } else if (IS_BOOL(part)) {
      totalLength += AS_BOOL(part) ? 4 : 5; // "true" or "false"
    } else if (IS_NIL(part)) {
      totalLength += 3; // "nil"
    } else {
      totalLength += 8; // "[object]" for other types
    }
  }
  
  // Allocate result string
  char* result = ALLOCATE(char, totalLength + 1);
  result[0] = '\0';
  int pos = 0;
  
  // Concatenate all parts
  for (int i = 0; i < partCount; i++) {
    Value part = parts[i];
    if (IS_STRING(part)) {
      ObjString* str = AS_STRING(part);
      memcpy(result + pos, AS_CSTRING(part), str->length);
      pos += str->length;
    } else if (IS_NUMBER(part)) {
      int len = snprintf(result + pos, totalLength - pos + 1, "%.15g", AS_NUMBER(part));
      pos += len;
    } else if (IS_BOOL(part)) {
      const char* boolStr = AS_BOOL(part) ? "true" : "false";
      int len = strlen(boolStr);
      memcpy(result + pos, boolStr, len);
      pos += len;
    } else if (IS_NIL(part)) {
      memcpy(result + pos, "nil", 3);
      pos += 3;
    } else {
      memcpy(result + pos, "[object]", 8);
      pos += 8;
    }
  }
  
  result[totalLength] = '\0';
  
  // Pop all parts from stack
  for (int i = 0; i < partCount; i++) {
    pop();
  }
  
  // Push result
  ObjString* interpolated = takeString(result, totalLength);
  push(OBJ_VAL(interpolated));
  DISPATCH();
}

op_hash_literal: {
  TRACE();
  int pairCount = READ_BYTE();
  ObjHash* hash = newHash();
  
  // Pop key-value pairs from stack and add to hash
  for (int i = 0; i < pairCount; i++) {
    Value value = pop();
    Value key = pop();
    
    // Convert key to string if it's not already
    ObjString* keyString;
    if (IS_STRING(key)) {
      keyString = AS_STRING(key);
    } else if (IS_NUMBER(key)) {
      // Convert number to string
      char buffer[32];
      int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(key));
      keyString = copyString(buffer, len);
    } else {
      runtimeError("Hash keys must be strings or numbers.");
      return INTERPRET_RUNTIME_ERROR;
    }
    
    tableSet(&hash->table, keyString, value);
  }
  
  push(OBJ_VAL(hash));
  DISPATCH();
}

op_get_index: {
  TRACE();
  Value index = pop();
  Value hashValue = pop();
  
  if (!IS_HASH(hashValue)) {
    runtimeError("Only hashes support indexing.");
    return INTERPRET_RUNTIME_ERROR;
  }
  
  ObjHash* hash = AS_HASH(hashValue);
  
  // Convert index to string if it's not already
  ObjString* keyString;
  if (IS_STRING(index)) {
    keyString = AS_STRING(index);
  } else if (IS_NUMBER(index)) {
    // Convert number to string
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(index));
    keyString = copyString(buffer, len);
  } else {
    runtimeError("Hash keys must be strings or numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }
  
  Value value;
  if (tableGet(&hash->table, keyString, &value)) {
    push(value);
  } else {
    push(NIL_VAL);
  }
  DISPATCH();
}

op_set_index: {
  TRACE();
  Value value = pop();
  Value index = pop();
  Value hashValue = pop();
  
  if (!IS_HASH(hashValue)) {
    runtimeError("Only hashes support indexing.");
    return INTERPRET_RUNTIME_ERROR;
  }
  
  ObjHash* hash = AS_HASH(hashValue);
  
  // Convert index to string if it's not already
  ObjString* keyString;
  if (IS_STRING(index)) {
    keyString = AS_STRING(index);
  } else if (IS_NUMBER(index)) {
    // Convert number to string
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(index));
    keyString = copyString(buffer, len);
  } else {
    runtimeError("Hash keys must be strings or numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }
  
  tableSet(&hash->table, keyString, value);
  push(value); // Assignment returns the assigned value
  DISPATCH();
}

op_type_cast: {
  TRACE();
  TokenType targetType = (TokenType)READ_BYTE();
  Value value = pop();
  
  switch (targetType) {
    case TOKEN_RETURNTYPE_INT: {
      if (IS_NUMBER(value)) {
        // Already a number, just push it back
        push(value);
      } else if (IS_STRING(value)) {
        // Try to parse string as number
        char* endptr;
        double num = strtod(AS_CSTRING(value), &endptr);
        if (*endptr == '\0') {
          push(NUMBER_VAL(num));
        } else {
          runtimeError("Cannot cast string '%s' to int.", AS_CSTRING(value));
          return INTERPRET_RUNTIME_ERROR;
        }
      } else {
        runtimeError("Cannot cast value to int.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case TOKEN_RETURNTYPE_STRING: {
      if (IS_STRING(value)) {
        // Already a string, just push it back
        push(value);
      } else if (IS_NUMBER(value)) {
        // Convert number to string
        char buffer[32];
        int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(value));
        ObjString* str = copyString(buffer, len);
        push(OBJ_VAL(str));
      } else if (IS_BOOL(value)) {
        const char* boolStr = AS_BOOL(value) ? "true" : "false";
        ObjString* str = copyString(boolStr, strlen(boolStr));
        push(OBJ_VAL(str));
      } else if (IS_NIL(value)) {
        ObjString* str = copyString("nil", 3);
        push(OBJ_VAL(str));
      } else {
        runtimeError("Cannot cast value to string.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case TOKEN_RETURNTYPE_BOOL: {
      if (IS_BOOL(value)) {
        // Already a bool, just push it back
        push(value);
      } else {
        // In Ruby semantics, only false and nil are falsey
        // All other values (including 0) are truthy
        push(BOOL_VAL(!isFalsey(value)));
      }
      break;
    }
    case TOKEN_RETURNTYPE_HASH: {
      if (IS_HASH(value)) {
        // Already a hash, just push it back
        push(value);
      } else {
        runtimeError("Cannot cast value to hash.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    default:
      runtimeError("Unknown target type for cast.");
      return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

#else
  // Fallback to switch statement if computed goto is not available
  for (;;) {
//> trace-execution
#ifdef DEBUG_TRACE_EXECUTION
//> trace-stack
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
//< trace-stack
/* A Virtual Machine trace-execution < Calls and Functions trace-execution
    disassembleInstruction(vm.chunk,
                           (int)(vm.ip - vm.chunk->code));
*/
/* Calls and Functions trace-execution < Closures disassemble-instruction
    disassembleInstruction(&frame->function->chunk,
        (int)(frame->ip - frame->function->chunk.code));
*/
//> Closures disassemble-instruction
    disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
//< Closures disassemble-instruction
#endif

//< trace-execution
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
//> op-constant
      case OP_CONSTANT: {
        Value constant = frame->closure->function->chunk.constants.values[READ_BYTE()];
        push(constant);
        break;
      }
      case OP_CONSTANT_LONG: {
        uint32_t constant = (READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE();
        push(frame->closure->function->chunk.constants.values[constant]);
        break;
      }
//< op-constant
//> Types of Values interpret-literals
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
//< Types of Values interpret-literals
//> Global Variables interpret-pop
      case OP_POP: pop(); break;
//< Global Variables interpret-pop
//> Local Variables interpret-get-local
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
//< Local Variables interpret-get-local
//> Local Variables interpret-set-local
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
//< Local Variables interpret-set-local
//> Global Variables interpret-get-global
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", AS_CSTRING(OBJ_VAL(name)));
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
//< Global Variables interpret-get-global
//> Global Variables interpret-define-global
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
//< Global Variables interpret-define-global
//> Global Variables interpret-set-global
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name); // [delete]
          runtimeError("Undefined variable '%s'.", AS_CSTRING(OBJ_VAL(name)));
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
//< Global Variables interpret-set-global
//> Closures interpret-get-upvalue
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
//< Closures interpret-get-upvalue
//> Closures interpret-set-upvalue
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
//< Closures interpret-set-upvalue
//> Classes and Instances interpret-get-property
      case OP_GET_PROPERTY: {
//> get-not-instance
#if !OPTIMIZE_SKIP_TYPE_CHECKS
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }
#endif

//< get-not-instance
#if !OPTIMIZE_SKIP_MEMORY_SAFETY
        // Check if the instance is still valid
        if (!checkObjectAccess(peek(0), "access property of")) {
          return INTERPRET_RUNTIME_ERROR;
        }
#endif
        
        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjString* name = READ_STRING();
        
        Value value;
        if (tableGet(&instance->fields, name, &value)) {
          pop(); // Instance.
          push(value);
          break;
        }
//> get-undefined

//< get-undefined
        if (!bindMethod(instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
//< Classes and Instances interpret-get-property
//> Classes and Instances interpret-set-property
      case OP_SET_PROPERTY: {
//> set-not-instance
#if !OPTIMIZE_SKIP_TYPE_CHECKS
        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }
#endif

//< set-not-instance
#if !OPTIMIZE_SKIP_MEMORY_SAFETY
        // Check if the instance is still valid for mutation
        if (!checkObjectMutation(peek(1), "set property of")) {
          return INTERPRET_RUNTIME_ERROR;
        }
#endif
        
        ObjInstance* instance = AS_INSTANCE(peek(1));
        ObjString* name = READ_STRING();
        tableSet(&instance->fields, name, peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }
//< Classes and Instances interpret-set-property
//> Superclasses interpret-get-super
      case OP_GET_SUPER: {
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop());
        
        if (!bindMethod(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
//< Superclasses interpret-get-super
//> Types of Values interpret-equal
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
//< Types of Values interpret-equal
//> Types of Values interpret-comparison
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
//< Types of Values interpret-comparison
/* A Virtual Machine op-binary < Types of Values op-arithmetic
      case OP_ADD:      BINARY_OP(+); break;
      case OP_SUBTRACT: BINARY_OP(-); break;
      case OP_MULTIPLY: BINARY_OP(*); break;
      case OP_DIVIDE:   BINARY_OP(/); break;
*/
/* A Virtual Machine op-negate < Types of Values op-negate
      case OP_NEGATE:   push(-pop()); break;
*/
/* Types of Values op-arithmetic < Strings add-strings
      case OP_ADD:      BINARY_OP(NUMBER_VAL, +); break;
*/
//> Strings add-strings
      case OP_ADD: {
#if OPTIMIZE_SKIP_TYPE_CHECKS
        // In optimized mode, compiler should emit specific opcodes instead of generic OP_ADD
        runtimeError("Generic OP_ADD used in optimized mode - compiler error.");
        return INTERPRET_RUNTIME_ERROR;
#else
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtimeError(
              "Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
#endif
        break;
      }
      case OP_ADD_NUMBER: {
        // Optimized numeric addition - no type check needed
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
        break;
      }
      case OP_ADD_STRING: {
        // Optimized string concatenation - no type check needed
        concatenate();
        break;
      }
//< Strings add-strings
//> Types of Values op-arithmetic
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_SUBTRACT_NUMBER: {
        // Optimized numeric subtraction - no type check needed
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a - b));
        break;
      }
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_MULTIPLY_NUMBER: {
        // Optimized numeric multiplication - no type check needed
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a * b));
        break;
      }
      case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
      case OP_DIVIDE_NUMBER: {
        // Optimized numeric division - no type check needed
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a / b));
        break;
      }
      case OP_MODULO: {
#if OPTIMIZE_SKIP_TYPE_CHECKS
        // Optimized version without type checks
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        if (b == 0.0) {
          runtimeError("Modulo by zero.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(fmod(a, b)));
#else
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtimeError("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        if (b == 0.0) {
          runtimeError("Modulo by zero.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(fmod(a, b)));
#endif
        break;
      }
      case OP_MODULO_NUMBER: {
        // Optimized numeric modulo - no type check needed
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        if (b == 0.0) {
          runtimeError("Modulo by zero.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(fmod(a, b)));
        break;
      }
//< Types of Values op-arithmetic
//> Types of Values op-not
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
//< Types of Values op-not
//> Types of Values op-negate
      case OP_NEGATE:
#if OPTIMIZE_SKIP_TYPE_CHECKS
        // In optimized mode, compiler should emit OP_NEGATE_NUMBER instead
        runtimeError("Generic OP_NEGATE used in optimized mode - compiler error.");
        return INTERPRET_RUNTIME_ERROR;
#else
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
#endif
        break;
      case OP_NEGATE_NUMBER:
        // Optimized numeric negation - no type check needed
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
//< Types of Values op-negate
//> Global Variables interpret-print
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
//< Global Variables interpret-print
//> Jumping Back and Forth op-jump
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
//< Jumping Back and Forth op-jump
//> Jumping Back and Forth op-jump-if-false
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
//< Jumping Back and Forth op-jump-if-false
//> Jumping Back and Forth op-loop
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        
        // Removed JIT tracking for performance
        frame->ip -= offset;
        break;
      }
//< Jumping Back and Forth op-loop
//> Calls and Functions interpret-call
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
//< Calls and Functions interpret-call
//> Methods and Initializers interpret-invoke
      case OP_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
//< Methods and Initializers interpret-invoke
//> Superclasses interpret-super-invoke
      case OP_SUPER_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
//< Superclasses interpret-super-invoke
//> Closures interpret-closure
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        
        // Optimization: Reuse closure for recursive functions without upvalues
        if (function->upvalueCount == 0 && cachedRecursiveFunction == function && cachedRecursiveClosure != NULL) {
          // Reuse the existing closure for this function
          push(OBJ_VAL(cachedRecursiveClosure));
          DISPATCH();
        }
        
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));
        
        // Cache this closure if it has no upvalues (good candidate for reuse)
        if (function->upvalueCount == 0) {
          cachedRecursiveFunction = function;
          cachedRecursiveClosure = closure;
        }
        
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] =
                captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        DISPATCH();
      }
//< Closures interpret-closure
//> Closures interpret-close-upvalue
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
//< Closures interpret-close-upvalue
      case OP_RETURN: {
        Value result = pop();
        
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
//> Classes and Instances interpret-class
      case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STRING())));
        break;
//< Classes and Instances interpret-class
//> Module System interpret-module
      case OP_MODULE:
        push(OBJ_VAL(newModule(READ_STRING())));
        break;
//< Module System interpret-module
//> Module System interpret-module-method
      case OP_MODULE_METHOD: {
        ObjString* methodName = READ_STRING();
        ObjClosure* method = AS_CLOSURE(peek(0));
        ObjModule* module = AS_MODULE(peek(1));
        
        tableSet(&module->functions, methodName, OBJ_VAL(method));
        pop(); // Method closure
        break;
      }
//< Module System interpret-module-method
//> Module System interpret-module-call
      case OP_MODULE_CALL: {
        ObjString* methodName = READ_STRING();
        int argCount = READ_BYTE();
        
        Value moduleValue = peek(argCount);
        if (!IS_MODULE(moduleValue)) {
          runtimeError("Can only call methods on modules.");
          return INTERPRET_RUNTIME_ERROR;
        }
        
        ObjModule* module = AS_MODULE(moduleValue);
        Value method;
        if (!tableGet(&module->functions, methodName, &method)) {
          runtimeError("Undefined method '%s' in module '%s'.", 
                       AS_CSTRING(OBJ_VAL(methodName)), 
                       AS_CSTRING(OBJ_VAL(module->name)));
          return INTERPRET_RUNTIME_ERROR;
        }
        
        if (!IS_CLOSURE(method)) {
          runtimeError("Module method is not a function.");
          return INTERPRET_RUNTIME_ERROR;
        }
        
        // Replace the module on the stack with the method
        vm.stackTop[-argCount - 1] = method;
        
        if (!call(AS_CLOSURE(method), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
//< Module System interpret-module-call
//> Module System interpret-require
      case OP_REQUIRE: {
        ObjString* filename = AS_STRING(pop());
        const char* filenameStr = AS_CSTRING(OBJ_VAL(filename));
        
        char* buffer = NULL;
        bool isEmbedded = false;
        
      #ifdef WITH_STL
        // First, try to find the module in embedded STL
        const char* embeddedSource = getEmbeddedSTLModule(filenameStr);
        if (embeddedSource != NULL) {
          // Use embedded STL module
          size_t sourceLen = strlen(embeddedSource);
          buffer = (char*)malloc(sourceLen + 1);
          if (buffer == NULL) {
            runtimeError("Not enough memory to load embedded module \"%s\".", filenameStr);
            return INTERPRET_RUNTIME_ERROR;
          }
          strcpy(buffer, embeddedSource);
          isEmbedded = true;
        }
      #endif
        
        // If not found in embedded STL, try to read from file
        if (buffer == NULL) {
          FILE* file = NULL;
          char* fullPath = NULL;
          
          // First try to open the file as specified
          file = fopen(filenameStr, "rb");
          
      #ifdef WITH_STL
          // If file not found and we have STL support, try the standard library path
          if (file == NULL && STL_PATH != NULL) {
            // Check if the filename contains a path separator - if not, it might be a standard library module
            if (strchr(filenameStr, '/') == NULL && strchr(filenameStr, '\\') == NULL) {
              // Construct path: STL_PATH/filename.gem
              size_t stlPathLen = strlen(STL_PATH);
              size_t filenameLen = strlen(filenameStr);
              size_t fullPathLen = stlPathLen + 1 + filenameLen + 4 + 1; // +1 for '/', +4 for '.gem', +1 for '\0'
              
              fullPath = (char*)malloc(fullPathLen);
              if (fullPath != NULL) {
                snprintf(fullPath, fullPathLen, "%s/%s.gem", STL_PATH, filenameStr);
                file = fopen(fullPath, "rb");
              }
            }
          }
      #endif
          
          if (file == NULL) {
            if (fullPath != NULL) {
              free(fullPath);
            }
            runtimeError("Could not open file \"%s\".", filenameStr);
            return INTERPRET_RUNTIME_ERROR;
          }
          
          fseek(file, 0L, SEEK_END);
          size_t fileSize = ftell(file);
          rewind(file);
          
          buffer = (char*)malloc(fileSize + 1);
          if (buffer == NULL) {
            runtimeError("Not enough memory to read \"%s\".", filenameStr);
            fclose(file);
            if (fullPath != NULL) {
              free(fullPath);
            }
            return INTERPRET_RUNTIME_ERROR;
          }
          
          size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
          if (bytesRead < fileSize) {
            runtimeError("Could not read file \"%s\".", filenameStr);
            free(buffer);
            fclose(file);
            if (fullPath != NULL) {
              free(fullPath);
            }
            return INTERPRET_RUNTIME_ERROR;
          }
          
          buffer[bytesRead] = '\0';
          fclose(file);
          
          // Clean up path
          if (fullPath != NULL) {
            free(fullPath);
          }
        }
        
        // Compile and execute the module
        ObjFunction* function = compile(buffer);
        free(buffer);
        
        if (function == NULL) {
          runtimeError("Failed to compile \"%s\".", filenameStr);
          return INTERPRET_RUNTIME_ERROR;
        }
        
        // Create closure for the module
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));
        
        // Execute the module in the current context
        if (!call(closure, 0)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        
        // Update frame pointer
        frame = &vm.frames[vm.frameCount - 1];
        DISPATCH();
      }
//< Module System interpret-require
//> Superclasses interpret-inherit
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeError("Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjClass* subclass = AS_CLASS(peek(0));
        tableAddAll(&AS_CLASS(superclass)->methods,
                    &subclass->methods);
        pop(); // Subclass.
        break;
      }
      //< Classes and Instances interpret-set-property
      //> Hash Objects interpret-get-index
      case OP_GET_INDEX: {
        Value index = pop();
        Value hashValue = pop();
        
        if (!IS_HASH(hashValue)) {
          runtimeError("Only hashes support indexing.");
          return INTERPRET_RUNTIME_ERROR;
        }
        
        ObjHash* hash = AS_HASH(hashValue);
        
        // Convert index to string if it's not already
        ObjString* keyString;
        if (IS_STRING(index)) {
          keyString = AS_STRING(index);
        } else if (IS_NUMBER(index)) {
          // Convert number to string
          char buffer[32];
          int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(index));
          keyString = copyString(buffer, len);
        } else {
          runtimeError("Hash keys must be strings or numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        
        Value value;
        if (tableGet(&hash->table, keyString, &value)) {
          push(value);
        } else {
          push(NIL_VAL);
        }
        break;
      }
      //< Hash Objects interpret-get-index
      //> Hash Objects interpret-set-index
      case OP_SET_INDEX: {
        Value value = pop();
        Value index = pop();
        Value hashValue = pop();
        
        if (!IS_HASH(hashValue)) {
          runtimeError("Only hashes support indexing.");
          return INTERPRET_RUNTIME_ERROR;
        }
        
        ObjHash* hash = AS_HASH(hashValue);
        
        // Convert index to string if it's not already
        ObjString* keyString;
        if (IS_STRING(index)) {
          keyString = AS_STRING(index);
        } else if (IS_NUMBER(index)) {
          // Convert number to string
          char buffer[32];
          int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(index));
          keyString = copyString(buffer, len);
        } else {
          runtimeError("Hash keys must be strings or numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        
        tableSet(&hash->table, keyString, value);
        push(value); // Assignment returns the assigned value
        break;
      }
      //< Hash Objects interpret-set-index
      //> Hash Objects interpret-hash-literal
      case OP_HASH_LITERAL: {
        int pairCount = READ_BYTE();
        ObjHash* hash = newHash();
        
        // Pop key-value pairs from stack and add to hash
        for (int i = 0; i < pairCount; i++) {
          Value value = pop();
          Value key = pop();
          
          // Convert key to string if it's not already
          ObjString* keyString;
          if (IS_STRING(key)) {
            keyString = AS_STRING(key);
          } else if (IS_NUMBER(key)) {
            // Convert number to string
            char buffer[32];
            int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(key));
            keyString = copyString(buffer, len);
          } else {
            runtimeError("Hash keys must be strings or numbers.");
            return INTERPRET_RUNTIME_ERROR;
          }
          
          tableSet(&hash->table, keyString, value);
        }
        
        push(OBJ_VAL(hash));
        break;
      }
      //< Hash Objects interpret-hash-literal
      case OP_TYPE_CAST: {
        TokenType targetType = (TokenType)READ_BYTE();
        Value value = pop();
        
        switch (targetType) {
          case TOKEN_RETURNTYPE_INT: {
            if (IS_NUMBER(value)) {
              // Already a number, just push it back
              push(value);
            } else if (IS_STRING(value)) {
              // Try to parse string as number
              char* endptr;
              double num = strtod(AS_CSTRING(value), &endptr);
              if (*endptr == '\0') {
                push(NUMBER_VAL(num));
              } else {
                runtimeError("Cannot cast string '%s' to int.", AS_CSTRING(value));
                return INTERPRET_RUNTIME_ERROR;
              }
            } else {
              runtimeError("Cannot cast value to int.");
              return INTERPRET_RUNTIME_ERROR;
            }
            break;
          }
          case TOKEN_RETURNTYPE_STRING: {
            if (IS_STRING(value)) {
              // Already a string, just push it back
              push(value);
            } else if (IS_NUMBER(value)) {
              // Convert number to string
              char buffer[32];
              int len = snprintf(buffer, sizeof(buffer), "%.15g", AS_NUMBER(value));
              ObjString* str = copyString(buffer, len);
              push(OBJ_VAL(str));
            } else if (IS_BOOL(value)) {
              const char* boolStr = AS_BOOL(value) ? "true" : "false";
              ObjString* str = copyString(boolStr, strlen(boolStr));
              push(OBJ_VAL(str));
            } else if (IS_NIL(value)) {
              ObjString* str = copyString("nil", 3);
              push(OBJ_VAL(str));
            } else {
              runtimeError("Cannot cast value to string.");
              return INTERPRET_RUNTIME_ERROR;
            }
            break;
          }
          case TOKEN_RETURNTYPE_BOOL: {
            if (IS_BOOL(value)) {
              // Already a bool, just push it back
              push(value);
            } else {
              // In Ruby semantics, only false and nil are falsey
              // All other values (including 0) are truthy
              push(BOOL_VAL(!isFalsey(value)));
            }
            break;
          }
          case TOKEN_RETURNTYPE_HASH: {
            if (IS_HASH(value)) {
              // Already a hash, just push it back
              push(value);
            } else {
              runtimeError("Cannot cast value to hash.");
              return INTERPRET_RUNTIME_ERROR;
            }
            break;
          }
          default:
            runtimeError("Unknown target type for cast.");
            return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
    }
  }
#endif

#undef READ_BYTE
//> Jumping Back and Forth undef-read-short
#undef READ_SHORT
//< Jumping Back and Forth undef-read-short
//> undef-read-constant
#undef READ_CONSTANT
//< undef-read-constant
//> Global Variables undef-read-string
#undef READ_STRING
//< Global Variables undef-read-string
//> undef-binary-op
#undef BINARY_OP
//< undef-binary-op
}
//< run
//> omit
void hack(bool b) {
  // Hack to avoid unused function error. run() is not used in the
  // scanning chapter.
  run();
  if (b) hack(false);
}
//< omit
//> interpret
/* A Virtual Machine interpret < Scanning on Demand vm-interpret-c
InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
*/
//> Scanning on Demand vm-interpret-c
InterpretResult interpret(const char* source) {
/* Scanning on Demand vm-interpret-c < Compiling Expressions interpret-chunk
  compile(source);
  return INTERPRET_OK;
*/
/* Compiling Expressions interpret-chunk < Calls and Functions interpret-stub
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;
*/
//> Calls and Functions interpret-stub
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
//< Calls and Functions interpret-stub
/* Calls and Functions interpret-stub < Calls and Functions interpret
  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.stack;
*/
/* Calls and Functions interpret < Closures interpret
  call(function, 0);
*/
//> Closures interpret
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);
//< Closures interpret
//< Scanning on Demand vm-interpret-c
//> Compiling Expressions interpret-chunk

/* Compiling Expressions interpret-chunk < Calls and Functions end-interpret
  InterpretResult result = run();

  freeChunk(&chunk);
  return result;
*/
//> Calls and Functions end-interpret
  return run();
//< Calls and Functions end-interpret
//< Compiling Expressions interpret-chunk
}
//< interpret

//> Inline Cache Helper Functions
#if INLINE_CACHE_ENABLED
static InlineCache* getCallCache(uint32_t callSiteId) {
  uint32_t index = callSiteId % INLINE_CACHE_SIZE;
  return &vm.globalCallCache[index];
}

static bool tryInlineCache(uint32_t callSiteId, ObjClosure* closure, int argCount) {
  InlineCache* cache = getCallCache(callSiteId);
  
  if (cache->callSiteId == callSiteId && cache->closure == closure) {
    // Cache hit! Direct call without lookup overhead
    cache->hitCount++;
    
    // Optimized call with cached closure
    #if !OPTIMIZE_SKIP_TYPE_CHECKS
    if (argCount != closure->function->arity) {
      runtimeError("Expected %d arguments but got %d.",
          closure->function->arity, argCount);
      return false;
    }
    #endif

    #if !OPTIMIZE_SKIP_TYPE_CHECKS
    if (vm.frameCount == FRAMES_MAX) {
      runtimeError("Stack overflow.");
      return false;
    }
    #endif

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
  }
  
  return false; // Cache miss
}

static void updateCallCache(uint32_t callSiteId, ObjClosure* closure) {
  InlineCache* cache = getCallCache(callSiteId);
  cache->callSiteId = callSiteId;
  cache->closure = closure;
  cache->hitCount = 1;
}
#endif
//< Inline Cache Helper Functions

//> Memoization Helper Functions
#if MEMO_CACHE_ENABLED
static uint32_t memoHash(ObjClosure* function, Value argument) {
  // Simple hash combining function pointer and argument value
  uint64_t hash = (uint64_t)(uintptr_t)function;
  if (IS_NUMBER(argument)) {
    hash ^= (uint64_t)AS_NUMBER(argument);
  }
  return (uint32_t)(hash % MEMO_CACHE_SIZE);
}

static bool tryMemoCache(ObjClosure* closure, Value argument, Value* result) {
  uint32_t index = memoHash(closure, argument);
  MemoEntry* entry = &vm.memoCache[index];
  
  if (entry->valid && entry->function == closure && valuesEqual(entry->argument, argument)) {
    *result = entry->result;
    return true; // Cache hit!
  }
  
  return false; // Cache miss
}

static void updateMemoCache(ObjClosure* closure, Value argument, Value result) {
  uint32_t index = memoHash(closure, argument);
  MemoEntry* entry = &vm.memoCache[index];
  
  entry->function = closure;
  entry->argument = argument;
  entry->result = result;
  entry->valid = true;
}
#endif
//< Memoization Helper Functions
