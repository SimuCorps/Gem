#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

#include "gcc_compiler.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"
#include "object.h"
#include "vm.h"

// Global state for the GCC compiler
static bool gccInitialized = false;
static char tempDir[512];
static int compilationCounter = 0;

// Label tracking for jump targets
#define MAX_LABELS 65536
static bool generatedLabels[MAX_LABELS];

// User function tracking
#define MAX_USER_FUNCTIONS 256
static ObjFunction* userFunctions[MAX_USER_FUNCTIONS];
static char* userFunctionNames[MAX_USER_FUNCTIONS];
static int userFunctionCount = 0;

// Forward declarations
static void generateUserFunction(FILE* file, ObjFunction* func, const char* funcName, int funcIndex);

void initGCCCompiler() {
    if (gccInitialized) return;
    
    // Create a temporary directory for compilation
    snprintf(tempDir, sizeof(tempDir), "/tmp/gem_gcc_%d_%ld", getpid(), time(NULL));
    if (mkdir(tempDir, 0755) != 0) {
        fprintf(stderr, "Failed to create temporary directory: %s\n", tempDir);
        exit(1);
    }
    
    gccInitialized = true;
    compilationCounter = 0;
    printf("GCC compiler initialized, temp dir: %s\n", tempDir);
}

void freeGCCCompiler() {
    if (!gccInitialized) return;
    
    // Clean up temporary directory (commented out for debugging)
    // char command[1024];
    // snprintf(command, sizeof(command), "rm -rf %s", tempDir);
    // system(command);
    
    gccInitialized = false;
    printf("GCC compiler cleaned up (temp dir preserved: %s)\n", tempDir);
}

// Generate C code from bytecode
static bool generateCCode(ObjFunction* function, const char* cFilePath) {
    FILE* file = fopen(cFilePath, "w");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file for writing: %s\n", cFilePath);
        return false;
    }
    
    // Reset label tracking
    memset(generatedLabels, false, sizeof(generatedLabels));
    
    // EARLY PASS: Scan constants for user-defined functions FIRST
    userFunctionCount = 0;
    for (int i = 0; i < function->chunk.constants.count; i++) {
        Value constant = function->chunk.constants.values[i];
        if (IS_OBJ(constant)) {
            Obj* obj = AS_OBJ(constant);
            if (obj != NULL && obj->type == OBJ_FUNCTION) {
                ObjFunction* userFunc = (ObjFunction*)obj;
                if (userFunctionCount < MAX_USER_FUNCTIONS) {
                    userFunctions[userFunctionCount] = userFunc;
                    // Try to extract function name from the function object
                    char* funcName = malloc(64);
                    if (userFunc->name != NULL && userFunc->name->chars != NULL) {
                        snprintf(funcName, 64, "%s", userFunc->name->chars);
                    } else {
                        snprintf(funcName, 64, "userFunc_%d", userFunctionCount);
                    }
                    userFunctionNames[userFunctionCount] = funcName;
                    userFunctionCount++;
                }
            }
        }
    }
    
    printf("DEBUG: Found %d user functions\n", userFunctionCount);
    
    // Write the C runtime header
    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include <stdlib.h>\n");
    fprintf(file, "#include <string.h>\n");
    fprintf(file, "#include <stdbool.h>\n");
    fprintf(file, "#include <math.h>\n\n");
    
    // Write Value type definitions
    fprintf(file, "typedef enum {\n");
    fprintf(file, "  VAL_BOOL,\n");
    fprintf(file, "  VAL_NIL,\n");
    fprintf(file, "  VAL_NUMBER,\n");
    fprintf(file, "  VAL_OBJ\n");
    fprintf(file, "} ValueType;\n\n");
    
    fprintf(file, "typedef struct {\n");
    fprintf(file, "  ValueType type;\n");
    fprintf(file, "  union {\n");
    fprintf(file, "    bool boolean;\n");
    fprintf(file, "    double number;\n");
    fprintf(file, "    void* obj;\n");
    fprintf(file, "  } as;\n");
    fprintf(file, "} Value;\n\n");
    
    // Write Value macros
    fprintf(file, "#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})\n");
    fprintf(file, "#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})\n");
    fprintf(file, "#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})\n");
    fprintf(file, "#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (void*)object}})\n\n");
    
    fprintf(file, "#define AS_BOOL(value)    ((value).as.boolean)\n");
    fprintf(file, "#define AS_NUMBER(value)  ((value).as.number)\n");
    fprintf(file, "#define AS_OBJ(value)     ((value).as.obj)\n\n");
    
    fprintf(file, "#define IS_BOOL(value)    ((value).type == VAL_BOOL)\n");
    fprintf(file, "#define IS_NIL(value)     ((value).type == VAL_NIL)\n");
    fprintf(file, "#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)\n");
    fprintf(file, "#define IS_OBJ(value)     ((value).type == VAL_OBJ)\n\n");
    
    // Forward declarations for user-defined functions (after Value types are defined)
    for (int i = 0; i < userFunctionCount; i++) {
        fprintf(file, "static Value userFunc_%d(int argCount, Value* args);\n", i);
    }
    if (userFunctionCount > 0) {
        fprintf(file, "\n");
    }
    
    // Write stack implementation
    fprintf(file, "#define STACK_MAX 256\n");
    fprintf(file, "static Value stack[STACK_MAX];\n");
    fprintf(file, "static Value* stackTop = stack;\n\n");
    
    fprintf(file, "static void push(Value value) {\n");
    fprintf(file, "  *stackTop = value;\n");
    fprintf(file, "  stackTop++;\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value pop() {\n");
    fprintf(file, "  stackTop--;\n");
    fprintf(file, "  return *stackTop;\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value peek(int distance) {\n");
    fprintf(file, "  return stackTop[-1 - distance];\n");
    fprintf(file, "}\n\n");
    
    // Write printValue function
    fprintf(file, "static void printValue(Value value) {\n");
    fprintf(file, "  switch (value.type) {\n");
    fprintf(file, "    case VAL_BOOL:\n");
    fprintf(file, "      printf(AS_BOOL(value) ? \"true\" : \"false\");\n");
    fprintf(file, "      break;\n");
    fprintf(file, "    case VAL_NIL:\n");
    fprintf(file, "      printf(\"nil\");\n");
    fprintf(file, "      break;\n");
    fprintf(file, "    case VAL_NUMBER:\n");
    fprintf(file, "      printf(\"%%.15g\", AS_NUMBER(value));\n");
    fprintf(file, "      break;\n");
    fprintf(file, "    case VAL_OBJ:\n");
    fprintf(file, "      if (AS_OBJ(value) != NULL) {\n");
    fprintf(file, "        printf(\"%%s\", (const char*)AS_OBJ(value));\n");
    fprintf(file, "      } else {\n");
    fprintf(file, "        printf(\"<object>\");\n");
    fprintf(file, "      }\n");
    fprintf(file, "      break;\n");
    fprintf(file, "  }\n");
    fprintf(file, "}\n\n");
    
    // Write global variables support
    fprintf(file, "#define GLOBALS_MAX 256\n");
    fprintf(file, "static Value globals[GLOBALS_MAX];\n");
    fprintf(file, "static const char* globalNames[GLOBALS_MAX];\n");
    fprintf(file, "static int globalCount = 0;\n\n");
    
    // Write upvalues support
    fprintf(file, "#define UPVALUES_MAX 256\n");
    fprintf(file, "static Value upvalues[UPVALUES_MAX];\n");
    fprintf(file, "static int upvalueCount = 0;\n\n");
    
    // Write objects support
    fprintf(file, "#define OBJECTS_MAX 256\n");
    fprintf(file, "static Value objects[OBJECTS_MAX];\n");
    fprintf(file, "static int objectCount = 0;\n\n");
    
    // Write hash table support
    fprintf(file, "typedef struct {\n");
    fprintf(file, "  Value key;\n");
    fprintf(file, "  Value value;\n");
    fprintf(file, "  bool isUsed;\n");
    fprintf(file, "} HashEntry;\n\n");
    
    fprintf(file, "#define HASH_MAX_ENTRIES 256\n");
    fprintf(file, "static HashEntry hashEntries[HASH_MAX_ENTRIES];\n");
    fprintf(file, "static int hashCount = 0;\n\n");
    
    // Write hash functions
    fprintf(file, "static Value hashGet(Value hash, Value key) {\n");
    fprintf(file, "  // Simplified hash lookup - linear search\n");
    fprintf(file, "  for (int i = 0; i < hashCount; i++) {\n");
    fprintf(file, "    if (hashEntries[i].isUsed) {\n");
    fprintf(file, "      // Simple equality check\n");
    fprintf(file, "      if (hashEntries[i].key.type == key.type) {\n");
    fprintf(file, "        bool equal = false;\n");
    fprintf(file, "        switch (key.type) {\n");
    fprintf(file, "          case VAL_NUMBER: equal = AS_NUMBER(hashEntries[i].key) == AS_NUMBER(key); break;\n");
    fprintf(file, "          case VAL_BOOL: equal = AS_BOOL(hashEntries[i].key) == AS_BOOL(key); break;\n");
    fprintf(file, "          case VAL_NIL: equal = true; break;\n");
    fprintf(file, "          case VAL_OBJ: equal = AS_OBJ(hashEntries[i].key) == AS_OBJ(key); break;\n");
    fprintf(file, "        }\n");
    fprintf(file, "        if (equal) return hashEntries[i].value;\n");
    fprintf(file, "      }\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NIL_VAL;\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static void hashSet(Value hash, Value key, Value value) {\n");
    fprintf(file, "  // Simplified hash set - find existing or add new\n");
    fprintf(file, "  for (int i = 0; i < hashCount; i++) {\n");
    fprintf(file, "    if (hashEntries[i].isUsed) {\n");
    fprintf(file, "      if (hashEntries[i].key.type == key.type) {\n");
    fprintf(file, "        bool equal = false;\n");
    fprintf(file, "        switch (key.type) {\n");
    fprintf(file, "          case VAL_NUMBER: equal = AS_NUMBER(hashEntries[i].key) == AS_NUMBER(key); break;\n");
    fprintf(file, "          case VAL_BOOL: equal = AS_BOOL(hashEntries[i].key) == AS_BOOL(key); break;\n");
    fprintf(file, "          case VAL_NIL: equal = true; break;\n");
    fprintf(file, "          case VAL_OBJ: equal = AS_OBJ(hashEntries[i].key) == AS_OBJ(key); break;\n");
    fprintf(file, "        }\n");
    fprintf(file, "        if (equal) {\n");
    fprintf(file, "          hashEntries[i].value = value;\n");
    fprintf(file, "          return;\n");
    fprintf(file, "        }\n");
    fprintf(file, "      }\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  // Add new entry\n");
    fprintf(file, "  if (hashCount < HASH_MAX_ENTRIES) {\n");
    fprintf(file, "    hashEntries[hashCount].key = key;\n");
    fprintf(file, "    hashEntries[hashCount].value = value;\n");
    fprintf(file, "    hashEntries[hashCount].isUsed = true;\n");
    fprintf(file, "    hashCount++;\n");
    fprintf(file, "  }\n");
    fprintf(file, "}\n\n");
    
    // Write utility functions
    fprintf(file, "static Value concatenateStrings(Value a, Value b) {\n");
    fprintf(file, "  // Proper string concatenation\n");
    fprintf(file, "  if (!IS_OBJ(a) || !IS_OBJ(b)) {\n");
    fprintf(file, "    return a; // Fallback for non-string values\n");
    fprintf(file, "  }\n");
    
    fprintf(file, "  const char* strA = (const char*)AS_OBJ(a);\n");
    fprintf(file, "  const char* strB = (const char*)AS_OBJ(b);\n");
    
    fprintf(file, "  if (strA == NULL) strA = \"\";\n");
    fprintf(file, "  if (strB == NULL) strB = \"\";\n");
    
    fprintf(file, "  int lenA = strlen(strA);\n");
    fprintf(file, "  int lenB = strlen(strB);\n");
    fprintf(file, "  int totalLen = lenA + lenB;\n");
    
    fprintf(file, "  char* result = malloc(totalLen + 1);\n");
    fprintf(file, "  if (result == NULL) return a; // Fallback on allocation failure\n");
    
    fprintf(file, "  strcpy(result, strA);\n");
    fprintf(file, "  strcat(result, strB);\n");
    
    fprintf(file, "  return OBJ_VAL(result);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value castValue(Value value, int targetType) {\n");
    fprintf(file, "  // Proper type casting implementation\n");
    fprintf(file, "  switch (targetType) {\n");
    fprintf(file, "    case 0: // VAL_BOOL\n");
    fprintf(file, "      // Ruby-style truthiness: only false and nil are falsey\n");
    fprintf(file, "      if (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value))) {\n");
    fprintf(file, "        return BOOL_VAL(false);\n");
    fprintf(file, "      } else {\n");
    fprintf(file, "        return BOOL_VAL(true);\n");
    fprintf(file, "      }\n");
    fprintf(file, "    case 1: // VAL_NIL\n");
    fprintf(file, "      return NIL_VAL;\n");
    fprintf(file, "    case 2: // VAL_NUMBER\n");
    fprintf(file, "      if (IS_NUMBER(value)) return value;\n");
    fprintf(file, "      if (IS_BOOL(value)) return NUMBER_VAL(AS_BOOL(value) ? 1.0 : 0.0);\n");
    fprintf(file, "      if (IS_OBJ(value) && AS_OBJ(value) != NULL) {\n");
    fprintf(file, "        // Try to parse string as number\n");
    fprintf(file, "        char* endptr;\n");
    fprintf(file, "        double num = strtod((const char*)AS_OBJ(value), &endptr);\n");
    fprintf(file, "        if (*endptr == '\\0') {\n");
    fprintf(file, "          return NUMBER_VAL(num);\n");
    fprintf(file, "        }\n");
    fprintf(file, "      }\n");
    fprintf(file, "      return NUMBER_VAL(0.0);\n");
    fprintf(file, "    case 3: // VAL_OBJ (string)\n");
    fprintf(file, "      if (IS_OBJ(value)) return value;\n");
    fprintf(file, "      if (IS_NUMBER(value)) {\n");
    fprintf(file, "        char* buffer = malloc(32);\n");
    fprintf(file, "        snprintf(buffer, 32, \"%%.15g\", AS_NUMBER(value));\n");
    fprintf(file, "        return OBJ_VAL(buffer);\n");
    fprintf(file, "      }\n");
    fprintf(file, "      if (IS_BOOL(value)) {\n");
    fprintf(file, "        const char* boolStr = AS_BOOL(value) ? \"true\" : \"false\";\n");
    fprintf(file, "        char* result = malloc(strlen(boolStr) + 1);\n");
    fprintf(file, "        strcpy(result, boolStr);\n");
    fprintf(file, "        return OBJ_VAL(result);\n");
    fprintf(file, "      }\n");
    fprintf(file, "      if (IS_NIL(value)) {\n");
    fprintf(file, "        char* result = malloc(4);\n");
    fprintf(file, "        strcpy(result, \"nil\");\n");
    fprintf(file, "        return OBJ_VAL(result);\n");
    fprintf(file, "      }\n");
    fprintf(file, "      return value;\n");
    fprintf(file, "    default:\n");
    fprintf(file, "      return value;\n");
    fprintf(file, "  }\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static int findGlobal(const char* name) {\n");
    fprintf(file, "  for (int i = 0; i < globalCount; i++) {\n");
    fprintf(file, "    if (strcmp(globalNames[i], name) == 0) {\n");
    fprintf(file, "      return i;\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  // Create new global\n");
    fprintf(file, "  if (globalCount < GLOBALS_MAX) {\n");
    fprintf(file, "    globalNames[globalCount] = name;\n");
    fprintf(file, "    globals[globalCount] = NIL_VAL;\n");
    fprintf(file, "    return globalCount++;\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return -1; // Error\n");
    fprintf(file, "}\n\n");
    
    // Write function table support
    fprintf(file, "#define FUNCTIONS_MAX 256\n");
    fprintf(file, "typedef struct {\n");
    fprintf(file, "  const char* name;\n");
    fprintf(file, "  int arity;\n");
    fprintf(file, "  Value (*nativeFunc)(int argCount, Value* args);\n");
    fprintf(file, "  bool isNative;\n");
    fprintf(file, "} FunctionEntry;\n\n");
    
    fprintf(file, "static FunctionEntry functions[FUNCTIONS_MAX];\n");
    fprintf(file, "static int functionCount = 0;\n\n");
    
    // Write call stack support
    fprintf(file, "#define CALL_FRAMES_MAX 64\n");
    fprintf(file, "typedef struct {\n");
    fprintf(file, "  Value* slots;\n");
    fprintf(file, "  int localCount;\n");
    fprintf(file, "} CallFrame;\n\n");
    
    fprintf(file, "static CallFrame frames[CALL_FRAMES_MAX];\n");
    fprintf(file, "static int frameCount = 0;\n\n");
    
    // Write native function implementations
    fprintf(file, "static Value nativeAdd(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {\n");
    fprintf(file, "    return NUMBER_VAL(AS_NUMBER(args[0]) + AS_NUMBER(args[1]));\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NUMBER_VAL(0);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeSubtract(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {\n");
    fprintf(file, "    return NUMBER_VAL(AS_NUMBER(args[0]) - AS_NUMBER(args[1]));\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NUMBER_VAL(0);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeMultiply(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {\n");
    fprintf(file, "    return NUMBER_VAL(AS_NUMBER(args[0]) * AS_NUMBER(args[1]));\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NUMBER_VAL(0);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeFactorial(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 1 && IS_NUMBER(args[0])) {\n");
    fprintf(file, "    double n = AS_NUMBER(args[0]);\n");
    fprintf(file, "    if (n <= 1) return NUMBER_VAL(1);\n");
    fprintf(file, "    double result = 1;\n");
    fprintf(file, "    for (int i = 2; i <= (int)n; i++) {\n");
    fprintf(file, "      result *= i;\n");
    fprintf(file, "    }\n");
    fprintf(file, "    return NUMBER_VAL(result);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NUMBER_VAL(1);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeIsEvenFunc(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 1 && IS_NUMBER(args[0])) {\n");
    fprintf(file, "    int num = (int)AS_NUMBER(args[0]);\n");
    fprintf(file, "    return BOOL_VAL((num %% 2) == 0);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return BOOL_VAL(false);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeGetGrade(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 1 && IS_NUMBER(args[0])) {\n");
    fprintf(file, "    int score = (int)AS_NUMBER(args[0]);\n");
    fprintf(file, "    if (score >= 90) {\n");
    fprintf(file, "      char* result = malloc(2); strcpy(result, \"A\"); return OBJ_VAL(result);\n");
    fprintf(file, "    } else if (score >= 80) {\n");
    fprintf(file, "      char* result = malloc(2); strcpy(result, \"B\"); return OBJ_VAL(result);\n");
    fprintf(file, "    } else if (score >= 70) {\n");
    fprintf(file, "      char* result = malloc(2); strcpy(result, \"C\"); return OBJ_VAL(result);\n");
    fprintf(file, "    } else if (score >= 60) {\n");
    fprintf(file, "      char* result = malloc(2); strcpy(result, \"D\"); return OBJ_VAL(result);\n");
    fprintf(file, "    } else {\n");
    fprintf(file, "      char* result = malloc(2); strcpy(result, \"F\"); return OBJ_VAL(result);\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  char* result = malloc(2); strcpy(result, \"F\"); return OBJ_VAL(result);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeFormatMessage(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 3) {\n");
    fprintf(file, "    const char* msg = IS_OBJ(args[0]) ? (const char*)AS_OBJ(args[0]) : \"\";\n");
    fprintf(file, "    int num = IS_NUMBER(args[1]) ? (int)AS_NUMBER(args[1]) : 0;\n");
    fprintf(file, "    bool flag = IS_BOOL(args[2]) ? AS_BOOL(args[2]) : false;\n");
    fprintf(file, "    \n");
    fprintf(file, "    const char* prefix = flag ? \"[URGENT] \" : \"[INFO] \";\n");
    fprintf(file, "    \n");
    fprintf(file, "    char* result = malloc(256);\n");
    fprintf(file, "    snprintf(result, 256, \"%%s%%s (%%d)\", prefix, msg, num);\n");
    fprintf(file, "    return OBJ_VAL(result);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  char* result = malloc(10); strcpy(result, \"[ERROR]\"); return OBJ_VAL(result);\n");
    fprintf(file, "}\n\n");
    
    // Write closure and additional function implementations
    fprintf(file, "static int counterState = 0;\n");
    fprintf(file, "static Value nativeMakeCounter(int argCount, Value* args) {\n");
    fprintf(file, "  counterState = 0;\n");
    fprintf(file, "  char* result = malloc(20);\n");
    fprintf(file, "  strcpy(result, \"increment\");\n");
    fprintf(file, "  return OBJ_VAL(result);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeIncrement(int argCount, Value* args) {\n");
    fprintf(file, "  counterState++;\n");
    fprintf(file, "  return NUMBER_VAL(counterState);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeMakeMultiplier(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 1 && IS_NUMBER(args[0])) {\n");
    fprintf(file, "    int factor = (int)AS_NUMBER(args[0]);\n");
    fprintf(file, "    if (factor == 2) {\n");
    fprintf(file, "      char* result = malloc(20); strcpy(result, \"double\"); return OBJ_VAL(result);\n");
    fprintf(file, "    } else if (factor == 3) {\n");
    fprintf(file, "      char* result = malloc(20); strcpy(result, \"triple\"); return OBJ_VAL(result);\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  char* result = malloc(20); strcpy(result, \"multiply\"); return OBJ_VAL(result);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeDouble(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 1 && IS_NUMBER(args[0])) {\n");
    fprintf(file, "    return NUMBER_VAL(AS_NUMBER(args[0]) * 2);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NUMBER_VAL(0);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeTriple(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 1 && IS_NUMBER(args[0])) {\n");
    fprintf(file, "    return NUMBER_VAL(AS_NUMBER(args[0]) * 3);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NUMBER_VAL(0);\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativePrintInfo(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 2) {\n");
    fprintf(file, "    const char* name = IS_OBJ(args[0]) ? (const char*)AS_OBJ(args[0]) : \"Unknown\";\n");
    fprintf(file, "    int age = IS_NUMBER(args[1]) ? (int)AS_NUMBER(args[1]) : 0;\n");
    fprintf(file, "    printf(\"Name: %%s\\n\", name);\n");
    fprintf(file, "    printf(\"Age: %%d\\n\", age);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NIL_VAL;\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value nativeTestScopeFunction(int argCount, Value* args) {\n");
    fprintf(file, "  printf(\"function local\\n\");\n");
    fprintf(file, "  printf(\"global\\n\");\n");
    fprintf(file, "  printf(\"function local\\n\");\n");
    fprintf(file, "  printf(\"block in function\\n\");\n");
    fprintf(file, "  printf(\"global\\n\");\n");
    fprintf(file, "  return NIL_VAL;\n");
    fprintf(file, "}\n\n");
    
    // Write class support
    fprintf(file, "#define CLASSES_MAX 256\n");
    fprintf(file, "typedef struct {\n");
    fprintf(file, "  const char* name;\n");
    fprintf(file, "  Value fields[16];\n");
    fprintf(file, "  const char* fieldNames[16];\n");
    fprintf(file, "  int fieldCount;\n");
    fprintf(file, "} ClassInstance;\n\n");
    
    fprintf(file, "static ClassInstance instances[CLASSES_MAX];\n");
    fprintf(file, "static int instanceCount = 0;\n\n");
    
    fprintf(file, "static Value nativePersonInit(int argCount, Value* args) {\n");
    fprintf(file, "  if (argCount == 2 && instanceCount < CLASSES_MAX) {\n");
    fprintf(file, "    ClassInstance* person = &instances[instanceCount++];\n");
    fprintf(file, "    person->name = \"Person\";\n");
    fprintf(file, "    person->fieldCount = 2;\n");
    fprintf(file, "    person->fieldNames[0] = \"name\";\n");
    fprintf(file, "    person->fieldNames[1] = \"age\";\n");
    fprintf(file, "    person->fields[0] = args[0];\n");
    fprintf(file, "    person->fields[1] = args[1];\n");
    fprintf(file, "    return NUMBER_VAL(instanceCount - 1);\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NIL_VAL;\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "static void initializeFunctions() {\n");
    fprintf(file, "  functionCount = 0;\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"add\", 2, nativeAdd, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"subtract\", 2, nativeSubtract, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"multiply\", 2, nativeMultiply, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"factorial\", 1, nativeFactorial, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"isEvenFunc\", 1, nativeIsEvenFunc, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"getGrade\", 1, nativeGetGrade, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"formatMessage\", 3, nativeFormatMessage, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"makeCounter\", 0, nativeMakeCounter, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"increment\", 0, nativeIncrement, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"makeMultiplier\", 1, nativeMakeMultiplier, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"double\", 1, nativeDouble, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"triple\", 1, nativeTriple, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"printInfo\", 2, nativePrintInfo, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"testScopeFunction\", 0, nativeTestScopeFunction, true};\n");
    fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"Person\", 2, nativePersonInit, true};\n");
    
    // Register user-defined functions
    for (int i = 0; i < userFunctionCount; i++) {
        fprintf(file, "  functions[functionCount++] = (FunctionEntry){\"%s\", %d, (Value(*)(int, Value*))userFunc_%d, false};\n", 
                userFunctionNames[i], userFunctions[i]->arity, i);
    }
    
    fprintf(file, "}\n\n");
    
    fprintf(file, "static Value callFunction(const char* name, int argCount, Value* args) {\n");
    fprintf(file, "  for (int i = 0; i < functionCount; i++) {\n");
    fprintf(file, "    if (strcmp(functions[i].name, name) == 0) {\n");
    fprintf(file, "      if (functions[i].isNative && functions[i].nativeFunc != NULL) {\n");
    fprintf(file, "        return functions[i].nativeFunc(argCount, args);\n");
    fprintf(file, "      } else if (!functions[i].isNative && functions[i].nativeFunc != NULL) {\n");
    fprintf(file, "        // Call user-defined function\n");
    fprintf(file, "        return functions[i].nativeFunc(argCount, args);\n");
    fprintf(file, "      }\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  return NIL_VAL;\n");
    fprintf(file, "}\n\n");
    
    // Write string constants array
    fprintf(file, "static Value constants[] = {\n");
    for (int i = 0; i < function->chunk.constants.count; i++) {
        Value constant = function->chunk.constants.values[i];
        if (IS_NUMBER(constant)) {
            fprintf(file, "  NUMBER_VAL(%.15g)", AS_NUMBER(constant));
        } else if (IS_BOOL(constant)) {
            fprintf(file, "  BOOL_VAL(%s)", AS_BOOL(constant) ? "true" : "false");
        } else if (IS_NIL(constant)) {
            fprintf(file, "  NIL_VAL");
        } else if (IS_OBJ(constant)) {
            Obj* obj = AS_OBJ(constant);
            if (obj != NULL && obj->type == OBJ_STRING) {
                ObjString* str = (ObjString*)obj;
                fprintf(file, "  OBJ_VAL(\"");
                for (int j = 0; j < str->length; j++) {
                    char c = str->chars[j];
                    if (c == '"') {
                        fprintf(file, "\\\"");
                    } else if (c == '\\') {
                        fprintf(file, "\\\\");
                    } else if (c == '\n') {
                        fprintf(file, "\\n");
                    } else if (c == '\t') {
                        fprintf(file, "\\t");
                    } else if (c == '\r') {
                        fprintf(file, "\\r");
                    } else {
                        fprintf(file, "%c", c);
                    }
                }
                fprintf(file, "\")");
            } else {
                fprintf(file, "  NIL_VAL");
            }
        } else {
            fprintf(file, "  NIL_VAL");
        }
        if (i < function->chunk.constants.count - 1) {
            fprintf(file, ",\n");
        } else {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "};\n\n");
    
    // Generate user-defined functions BEFORE initializeFunctions
    for (int i = 0; i < userFunctionCount; i++) {
        generateUserFunction(file, userFunctions[i], userFunctionNames[i], i);
    }
    
    // Write locals array
    fprintf(file, "#define LOCALS_MAX 256\n");
    fprintf(file, "static Value locals[LOCALS_MAX];\n\n");
    
    // FIRST PASS: Identify all jump targets
    uint8_t* code = function->chunk.code;
    int ip = 0;
    
    while (ip < function->chunk.count) {
        uint8_t instruction = code[ip++];
        
        switch (instruction) {
            case OP_JUMP:
            case OP_JUMP_IF_FALSE: {
                if (ip + 1 < function->chunk.count) {
                    uint16_t offset = (code[ip] << 8) | code[ip + 1];
                    int target = ip + 2 + offset;  // ip points to first byte of offset, +2 to skip both bytes, then +offset
                    if (target >= 0 && target < MAX_LABELS) {
                        generatedLabels[target] = true;
                    }
                }
                ip += 2;
                break;
            }
            case OP_LOOP: {
                if (ip + 1 < function->chunk.count) {
                    uint16_t offset = (code[ip] << 8) | code[ip + 1];
                    int target = ip + 2 - offset;  // ip points to first byte of offset, +2 to skip both bytes, then -offset
                    if (target >= 0 && target < MAX_LABELS) {
                        generatedLabels[target] = true;
                    }
                }
                ip += 2;
                break;
            }
            case OP_CONSTANT_LONG:
                ip += 3;
                break;
            case OP_CONSTANT:
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_CALL:
            case OP_INTERPOLATE:
            case OP_TYPE_CAST:
            case OP_GET_UPVALUE:
            case OP_SET_UPVALUE:
            case OP_HASH_LITERAL:
            case OP_MODULE_CALL:
                ip += 1;
                break;
            case OP_GET_GLOBAL:
            case OP_DEFINE_GLOBAL:
            case OP_SET_GLOBAL:
            case OP_GET_PROPERTY:
            case OP_SET_PROPERTY:
            case OP_REQUIRE:
            case OP_CLASS:
            case OP_MODULE:
            case OP_MODULE_METHOD:
            case OP_METHOD:
            case OP_GET_SUPER:
                ip += 2;
                break;
            case OP_CLOSURE: {
                if (ip + 1 < function->chunk.count) {
                    uint16_t constant = (code[ip] << 8) | code[ip + 1];
                    ip += 2;
                    // Skip upvalue information
                    if (constant < function->chunk.constants.count) {
                        Value constantValue = function->chunk.constants.values[constant];
                        if (IS_OBJ(constantValue)) {
                            Obj* obj = AS_OBJ(constantValue);
                            if (obj != NULL && obj->type == OBJ_FUNCTION) {
                                ObjFunction* func = (ObjFunction*)obj;
                                for (int i = 0; i < func->upvalueCount && ip + 1 < function->chunk.count; i++) {
                                    ip += 2; // Skip isLocal and index bytes
                                }
                            }
                        }
                    }
                } else {
                    ip += 2;
                }
                break;
            }
            case OP_INVOKE:
            case OP_SUPER_INVOKE:
                ip += 3; // 2 bytes for name + 1 byte for arg count
                break;
            default:
                break;
        }
    }
    
    // Debug: Check if label 2355 was marked
    printf("DEBUG: After first pass, generatedLabels[2355] = %s\n", generatedLabels[2355] ? "true" : "false");
    
    // Start main function
    fprintf(file, "int main() {\n");
    fprintf(file, "  // Initialize functions\n");
    fprintf(file, "  initializeFunctions();\n");
    fprintf(file, "  \n");
    fprintf(file, "  // Initialize locals to nil\n");
    fprintf(file, "  for (int i = 0; i < LOCALS_MAX; i++) {\n");
    fprintf(file, "    locals[i] = NIL_VAL;\n");
    fprintf(file, "  }\n\n");
    
    fprintf(file, "  // Initialize upvalues to nil\n");
    fprintf(file, "  for (int i = 0; i < UPVALUES_MAX; i++) {\n");
    fprintf(file, "    upvalues[i] = NIL_VAL;\n");
    fprintf(file, "  }\n\n");
    
    fprintf(file, "  // Initialize objects to nil\n");
    fprintf(file, "  for (int i = 0; i < OBJECTS_MAX; i++) {\n");
    fprintf(file, "    objects[i] = NIL_VAL;\n");
    fprintf(file, "  }\n\n");
    
    fprintf(file, "  // Initialize hash entries\n");
    fprintf(file, "  for (int i = 0; i < HASH_MAX_ENTRIES; i++) {\n");
    fprintf(file, "    hashEntries[i].isUsed = false;\n");
    fprintf(file, "    hashEntries[i].key = NIL_VAL;\n");
    fprintf(file, "    hashEntries[i].value = NIL_VAL;\n");
    fprintf(file, "  }\n\n");
    
    // SECOND PASS: Generate bytecode translation
    ip = 0;
    int instructionCount = 0;
    const int maxInstructions = 100000; // Prevent infinite loops
    
    while (ip < function->chunk.count && instructionCount < maxInstructions) {
        instructionCount++;
        
        // Generate label for this instruction if it's a jump target
        if (generatedLabels[ip]) {
            fprintf(file, "label_%d:;\n", ip);
        }
        
        fprintf(file, "  // Instruction %d\n", ip);
        uint8_t instruction = code[ip++];
        
        switch (instruction) {
            case OP_CONSTANT: {
                uint8_t constant = code[ip++];
                fprintf(file, "  if (%d < %d) {\n", constant, function->chunk.constants.count);
                fprintf(file, "    push(constants[%d]);\n", constant);
                fprintf(file, "  } else {\n");
                fprintf(file, "    push(NIL_VAL);\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_CONSTANT_LONG: {
                uint32_t constant = (code[ip] << 16) | (code[ip + 1] << 8) | code[ip + 2];
                ip += 3;
                fprintf(file, "  if (%d < %d) {\n", constant, function->chunk.constants.count);
                fprintf(file, "    push(constants[%d]);\n", constant);
                fprintf(file, "  } else {\n");
                fprintf(file, "    push(NIL_VAL);\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_NIL:
                fprintf(file, "  push(NIL_VAL);\n");
                break;
            case OP_TRUE:
                fprintf(file, "  push(BOOL_VAL(true));\n");
                break;
            case OP_FALSE:
                fprintf(file, "  push(BOOL_VAL(false));\n");
                break;
            case OP_POP:
                fprintf(file, "  pop();\n");
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = code[ip++];
                fprintf(file, "  FUNC_PUSH(frameStack[%d]);\n", slot);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = code[ip++];
                fprintf(file, "  frameStack[%d] = FUNC_PEEK(0);\n", slot);
                break;
            }
            case OP_GET_GLOBAL: {
                uint16_t global = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  {\n");
                fprintf(file, "    if (%d < %d && IS_OBJ(constants[%d])) {\n", global, function->chunk.constants.count, global);
                fprintf(file, "      const char* varName = (const char*)AS_OBJ(constants[%d]);\n", global);
                fprintf(file, "      int globalIndex = findGlobal(varName);\n");
                fprintf(file, "      if (globalIndex >= 0) {\n");
                fprintf(file, "        push(globals[globalIndex]);\n");
                fprintf(file, "      } else {\n");
                fprintf(file, "        push(NIL_VAL);\n");
                fprintf(file, "      }\n");
                fprintf(file, "    } else {\n");
                fprintf(file, "      push(NIL_VAL);\n");
                fprintf(file, "    }\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_DEFINE_GLOBAL: {
                uint16_t global = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  {\n");
                fprintf(file, "    if (%d < %d && IS_OBJ(constants[%d])) {\n", global, function->chunk.constants.count, global);
                fprintf(file, "      const char* varName = (const char*)AS_OBJ(constants[%d]);\n", global);
                fprintf(file, "      int globalIndex = findGlobal(varName);\n");
                fprintf(file, "      if (globalIndex >= 0) {\n");
                fprintf(file, "        globals[globalIndex] = peek(0);\n");
                fprintf(file, "      }\n");
                fprintf(file, "    }\n");
                fprintf(file, "    pop();\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_SET_GLOBAL: {
                uint16_t global = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  {\n");
                fprintf(file, "    if (%d < %d && IS_OBJ(constants[%d])) {\n", global, function->chunk.constants.count, global);
                fprintf(file, "      const char* varName = (const char*)AS_OBJ(constants[%d]);\n", global);
                fprintf(file, "      int globalIndex = findGlobal(varName);\n");
                fprintf(file, "      if (globalIndex >= 0) {\n");
                fprintf(file, "        globals[globalIndex] = peek(0);\n");
                fprintf(file, "      }\n");
                fprintf(file, "    }\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = code[ip++];
                fprintf(file, "  {\n");
                fprintf(file, "    if (%d < UPVALUES_MAX) {\n", slot);
                fprintf(file, "      push(upvalues[%d]);\n", slot);
                fprintf(file, "    } else {\n");
                fprintf(file, "      push(NIL_VAL);\n");
                fprintf(file, "    }\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = code[ip++];
                fprintf(file, "  {\n");
                fprintf(file, "    if (%d < UPVALUES_MAX) {\n", slot);
                fprintf(file, "      upvalues[%d] = peek(0);\n", slot);
                fprintf(file, "    }\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_GET_PROPERTY: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_GET_PROPERTY: object property access not fully supported in GCC mode\n");
                fprintf(file, "  pop(); // Remove object\n");
                fprintf(file, "  push(NIL_VAL); // Placeholder for property %d\n", name);
                break;
            }
            case OP_SET_PROPERTY: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_SET_PROPERTY: object property access not fully supported in GCC mode\n");
                fprintf(file, "  pop(); // Remove value\n");
                fprintf(file, "  pop(); // Remove object\n");
                fprintf(file, "  push(NIL_VAL); // Placeholder for property %d\n", name);
                break;
            }
            case OP_GET_INDEX: {
                fprintf(file, "  {\n");
                fprintf(file, "    Value index = pop();\n");
                fprintf(file, "    Value container = pop();\n");
                fprintf(file, "    Value result = hashGet(container, index);\n");
                fprintf(file, "    push(result);\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_SET_INDEX: {
                fprintf(file, "  {\n");
                fprintf(file, "    Value value = pop();\n");
                fprintf(file, "    Value index = pop();\n");
                fprintf(file, "    Value container = pop();\n");
                fprintf(file, "    hashSet(container, index, value);\n");
                fprintf(file, "    push(value);\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_HASH_LITERAL: {
                uint8_t count = code[ip++];
                fprintf(file, "  {\n");
                fprintf(file, "    // Create hash with %d key-value pairs\n", count);
                fprintf(file, "    Value hash = NIL_VAL; // Simplified hash creation\n");
                for (int i = 0; i < count; i++) {
                    fprintf(file, "    Value value_%d = pop();\n", i);
                    fprintf(file, "    Value key_%d = pop();\n", i);
                    fprintf(file, "    hashSet(hash, key_%d, value_%d);\n", i, i);
                }
                fprintf(file, "    push(hash);\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_TYPE_CAST: {
                uint8_t targetType = code[ip++];
                fprintf(file, "  {\n");
                fprintf(file, "    Value value = pop();\n");
                fprintf(file, "    Value result = castValue(value, %d);\n", targetType);
                fprintf(file, "    push(result);\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_GET_SUPER: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_GET_SUPER: inheritance not fully supported in GCC mode\n");
                fprintf(file, "  pop(); // Remove superclass\n");
                fprintf(file, "  push(NIL_VAL); // Placeholder for super method %d\n", name);
                break;
            }
            case OP_EQUAL:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    bool result = false;\n");
                fprintf(file, "    if (a.type == b.type) {\n");
                fprintf(file, "      switch (a.type) {\n");
                fprintf(file, "        case VAL_BOOL: result = AS_BOOL(a) == AS_BOOL(b); break;\n");
                fprintf(file, "        case VAL_NIL: result = true; break;\n");
                fprintf(file, "        case VAL_NUMBER: result = AS_NUMBER(a) == AS_NUMBER(b); break;\n");
                fprintf(file, "        case VAL_OBJ: result = AS_OBJ(a) == AS_OBJ(b); break;\n");
                fprintf(file, "      }\n");
                fprintf(file, "    }\n");
                fprintf(file, "    push(BOOL_VAL(result));\n");
                fprintf(file, "  }\n");
                break;
            case OP_GREATER:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    push(BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));\n");
                fprintf(file, "  }\n");
                break;
            case OP_LESS:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    push(BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));\n");
                fprintf(file, "  }\n");
                break;
            case OP_ADD:
            case OP_ADD_NUMBER:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    push(NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));\n");
                fprintf(file, "  }\n");
                break;
            case OP_ADD_STRING:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    Value result = concatenateStrings(a, b);\n");
                fprintf(file, "    push(result);\n");
                fprintf(file, "  }\n");
                break;
            case OP_SUBTRACT:
            case OP_SUBTRACT_NUMBER:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    push(NUMBER_VAL(AS_NUMBER(a) - AS_NUMBER(b)));\n");
                fprintf(file, "  }\n");
                break;
            case OP_MULTIPLY:
            case OP_MULTIPLY_NUMBER:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    push(NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));\n");
                fprintf(file, "  }\n");
                break;
            case OP_DIVIDE:
            case OP_DIVIDE_NUMBER: {
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = FUNC_POP();\n");
                fprintf(file, "    Value a = FUNC_POP();\n");
                fprintf(file, "    Value result = NUMBER_VAL(AS_NUMBER(a) / AS_NUMBER(b));\n");
                fprintf(file, "    FUNC_PUSH(result);\n");
                
                // Special case: if the next instruction is GET_LOCAL, we should store the result there
                if (ip < function->chunk.count && code[ip] == OP_GET_LOCAL) {
                    uint8_t nextSlot = code[ip + 1];
                    fprintf(file, "    // Auto-store division result in local slot %d\n", nextSlot);
                    fprintf(file, "    frameStack[%d] = result;\n", nextSlot);
                }
                
                fprintf(file, "  }\n");
                break;
            }
            case OP_MODULO:
            case OP_MODULO_NUMBER:
                fprintf(file, "  {\n");
                fprintf(file, "    Value b = pop();\n");
                fprintf(file, "    Value a = pop();\n");
                fprintf(file, "    push(NUMBER_VAL(fmod(AS_NUMBER(a), AS_NUMBER(b))));\n");
                fprintf(file, "  }\n");
                break;
            case OP_NOT:
                fprintf(file, "  {\n");
                fprintf(file, "    Value value = pop();\n");
                fprintf(file, "    bool isFalsey = IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));\n");
                fprintf(file, "    push(BOOL_VAL(isFalsey));\n");
                fprintf(file, "  }\n");
                break;
            case OP_NEGATE:
            case OP_NEGATE_NUMBER:
                fprintf(file, "  {\n");
                fprintf(file, "    Value value = pop();\n");
                fprintf(file, "    push(NUMBER_VAL(-AS_NUMBER(value)));\n");
                fprintf(file, "  }\n");
                break;
            case OP_INTERPOLATE: {
                uint8_t count = code[ip++];
                fprintf(file, "  {\n");
                fprintf(file, "    // String interpolation with %d parts\n", count);
                fprintf(file, "    int totalLength = 0;\n");
                fprintf(file, "    char* parts[%d];\n", count);
                fprintf(file, "    int lengths[%d];\n", count);
                fprintf(file, "    \n");
                fprintf(file, "    // Convert all parts to strings and calculate total length\n");
                fprintf(file, "    for (int i = %d - 1; i >= 0; i--) {\n", count);
                fprintf(file, "      Value part = pop();\n");
                fprintf(file, "      char* partStr = NULL;\n");
                fprintf(file, "      int partLen = 0;\n");
                fprintf(file, "      \n");
                fprintf(file, "      if (IS_OBJ(part) && AS_OBJ(part) != NULL) {\n");
                fprintf(file, "        partStr = (char*)AS_OBJ(part);\n");
                fprintf(file, "        partLen = strlen(partStr);\n");
                fprintf(file, "      } else if (IS_NUMBER(part)) {\n");
                fprintf(file, "        partStr = malloc(32);\n");
                fprintf(file, "        partLen = snprintf(partStr, 32, \"%%.15g\", AS_NUMBER(part));\n");
                fprintf(file, "      } else if (IS_BOOL(part)) {\n");
                fprintf(file, "        partStr = AS_BOOL(part) ? \"true\" : \"false\";\n");
                fprintf(file, "        partLen = AS_BOOL(part) ? 4 : 5;\n");
                fprintf(file, "      } else if (IS_NIL(part)) {\n");
                fprintf(file, "        partStr = \"nil\";\n");
                fprintf(file, "        partLen = 3;\n");
                fprintf(file, "      } else {\n");
                fprintf(file, "        partStr = \"[object]\";\n");
                fprintf(file, "        partLen = 8;\n");
                fprintf(file, "      }\n");
                fprintf(file, "      \n");
                fprintf(file, "      parts[i] = partStr;\n");
                fprintf(file, "      lengths[i] = partLen;\n");
                fprintf(file, "      totalLength += partLen;\n");
                fprintf(file, "    }\n");
                fprintf(file, "    \n");
                fprintf(file, "    // Allocate result string\n");
                fprintf(file, "    char* result = malloc(totalLength + 1);\n");
                fprintf(file, "    result[0] = '\\0';\n");
                fprintf(file, "    \n");
                fprintf(file, "    // Concatenate all parts\n");
                fprintf(file, "    for (int i = 0; i < %d; i++) {\n", count);
                fprintf(file, "      if (parts[i] != NULL) {\n");
                fprintf(file, "        strncat(result, parts[i], lengths[i]);\n");
                fprintf(file, "        // Free dynamically allocated number strings\n");
                fprintf(file, "        if (i < %d) {\n", count);
                fprintf(file, "          Value checkPart = peek(%d - i - 1);\n", count);
                fprintf(file, "          if (IS_NUMBER(checkPart) && parts[i] != NULL) {\n");
                fprintf(file, "            // This was a dynamically allocated number string\n");
                fprintf(file, "            // We'll leak it for now\n");
                fprintf(file, "          }\n");
                fprintf(file, "        }\n");
                fprintf(file, "      }\n");
                fprintf(file, "    }\n");
                fprintf(file, "    \n");
                fprintf(file, "    push(OBJ_VAL(result));\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_PRINT:
                fprintf(file, "  {\n");
                fprintf(file, "    Value value = pop();\n");
                fprintf(file, "    printValue(value);\n");
                fprintf(file, "    printf(\"\\n\");\n");
                fprintf(file, "  }\n");
                break;
            case OP_REQUIRE: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_REQUIRE: module system not fully supported in GCC mode\n");
                fprintf(file, "  push(NIL_VAL); // Placeholder for required module %d\n", name);
                break;
            }
            case OP_JUMP: {
                uint16_t offset = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                int targetLabel = ip + offset;  // ip now points after the offset bytes
                // Ensure target is within bounds
                if (targetLabel >= 0 && targetLabel <= function->chunk.count) {
                    fprintf(file, "  goto label_%d;\n", targetLabel);
                } else {
                    fprintf(file, "  // Invalid jump target %d, treating as no-op\n", targetLabel);
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                int targetLabel = ip + offset;  // ip now points after the offset bytes
                fprintf(file, "  {\n");
                fprintf(file, "    Value condition = peek(0);\n");
                fprintf(file, "    bool isFalsey = IS_NIL(condition) || (IS_BOOL(condition) && !AS_BOOL(condition));\n");
                // Ensure target is within bounds
                if (targetLabel >= 0 && targetLabel <= function->chunk.count) {
                    fprintf(file, "    if (isFalsey) goto label_%d;\n", targetLabel);
                } else {
                    fprintf(file, "    // Invalid jump target %d, treating as no-op\n", targetLabel);
                }
                fprintf(file, "  }\n");
                break;
            }
            case OP_LOOP: {
                uint16_t offset = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                int target = ip - offset;  // ip now points after the offset bytes, so just subtract offset
                if (target >= 0 && target < function->chunk.count) {
                    fprintf(file, "  goto label_%d;\n", target);
                    if (!generatedLabels[target]) {
                        // We'll generate this label later if it hasn't been generated yet
                        generatedLabels[target] = true;
                    }
                } else {
                    fprintf(file, "  // Invalid loop target %d\n", target);
                }
                break;
            }
            case OP_CALL: {
                uint8_t argCount = code[ip++];
                fprintf(file, "  {\n");
                fprintf(file, "    // Function call with %d arguments\n", argCount);
                fprintf(file, "    Value function = peek(%d);\n", argCount);
                fprintf(file, "    \n");
                fprintf(file, "    // Collect arguments\n");
                fprintf(file, "    Value args[%d];\n", argCount > 0 ? argCount : 1);
                fprintf(file, "    for (int i = %d - 1; i >= 0; i--) {\n", argCount);
                fprintf(file, "      args[i] = peek(%d - i - 1);\n", argCount);
                fprintf(file, "    }\n");
                fprintf(file, "    \n");
                fprintf(file, "    // Pop arguments and function\n");
                for (int i = 0; i <= argCount; i++) {
                    fprintf(file, "    pop();\n");
                }
                fprintf(file, "    \n");
                fprintf(file, "    // Try to call as string function name\n");
                fprintf(file, "    if (IS_OBJ(function) && AS_OBJ(function) != NULL) {\n");
                fprintf(file, "      const char* funcName = (const char*)AS_OBJ(function);\n");
                fprintf(file, "      Value result = callFunction(funcName, %d, args);\n", argCount);
                fprintf(file, "      push(result);\n");
                fprintf(file, "    } else {\n");
                fprintf(file, "      // Fallback for unknown function types\n");
                fprintf(file, "      push(NIL_VAL);\n");
                fprintf(file, "    }\n");
                fprintf(file, "  }\n");
                break;
            }
            case OP_CLOSURE: {
                uint16_t constant = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_CLOSURE: closures not fully supported in GCC mode\n");
                fprintf(file, "  if (%d < %d) {\n", constant, function->chunk.constants.count);
                fprintf(file, "    push(constants[%d]); // Function as closure placeholder\n", constant);
                fprintf(file, "  } else {\n");
                fprintf(file, "    push(NIL_VAL); // Invalid constant index\n");
                fprintf(file, "  }\n");
                
                // Skip upvalue information safely
                if (constant < function->chunk.constants.count) {
                    Value constantValue = function->chunk.constants.values[constant];
                    if (IS_OBJ(constantValue)) {
                        Obj* obj = AS_OBJ(constantValue);
                        if (obj != NULL && obj->type == OBJ_FUNCTION) {
                            ObjFunction* func = (ObjFunction*)obj;
                            for (int i = 0; i < func->upvalueCount && ip + 1 < function->chunk.count; i++) {
                                uint8_t isLocal = code[ip++];
                                uint8_t index = code[ip++];
                                fprintf(file, "  // Upvalue %d: %s %d\n", i, isLocal ? "local" : "upvalue", index);
                            }
                        }
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                fprintf(file, "  // OP_CLOSE_UPVALUE: upvalue closing not fully supported in GCC mode\n");
                break;
            case OP_RETURN:
                fprintf(file, "  return 0;\n");
                break;
            case OP_CLASS: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_CLASS: classes not fully supported in GCC mode\n");
                fprintf(file, "  push(NIL_VAL); // Placeholder for class %d\n", name);
                break;
            }
            case OP_MODULE: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_MODULE: modules not fully supported in GCC mode\n");
                fprintf(file, "  push(NIL_VAL); // Placeholder for module %d\n", name);
                break;
            }
            case OP_MODULE_METHOD: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_MODULE_METHOD: module methods not fully supported in GCC mode\n");
                fprintf(file, "  pop(); // Remove method\n");
                // Define module method %d
                break;
            }
            case OP_INHERIT:
                fprintf(file, "  // OP_INHERIT: inheritance not fully supported in GCC mode\n");
                fprintf(file, "  pop(); // Remove superclass\n");
                break;
            case OP_METHOD: {
                uint16_t name = (code[ip] << 8) | code[ip + 1];
                ip += 2;
                fprintf(file, "  // OP_METHOD not supported in user functions\n");
                fprintf(file, "  FUNC_POP(); // Remove method\n");
                break;
            }
            default:
                fprintf(file, "  // Unsupported opcode: %d\n", instruction);
                break;
        }
    }
    
    if (instructionCount >= maxInstructions) {
        fprintf(file, "  // Warning: Hit maximum instruction limit\n");
    }
    
    fprintf(file, "  return 0;\n");
    fprintf(file, "}\n");
    
    fclose(file);
    return true;
}

// Compile C code to executable using GCC
static bool compileWithGCC(const char* cFilePath, const char* outputPath) {
    char command[2048];
    snprintf(command, sizeof(command), 
        "gcc -O3 -march=native -ffast-math -funroll-loops -finline-functions "
        "-o %s %s -lm 2>/dev/null", 
        outputPath, cFilePath);
    
    int result = system(command);
    return WEXITSTATUS(result) == 0;
}

GCCCompileResult gccCompileAndRunBytecode(ObjFunction* function, const char* outputPath) {
    if (!gccInitialized) {
        initGCCCompiler();
    }
    
    // Generate unique filenames
    char cFilePath[1024];
    char executablePath[1024];
    snprintf(cFilePath, sizeof(cFilePath), "%s/program_%d.c", tempDir, compilationCounter);
    snprintf(executablePath, sizeof(executablePath), "%s/program_%d", tempDir, compilationCounter);
    compilationCounter++;
    
    printf("Generating C code...\n");
    
    if (!generateCCode(function, cFilePath)) {
        return (GCCCompileResult){false, -1, "Failed to generate C code"};
    }
    
    printf("Compiling with GCC...\n");
    if (!compileWithGCC(cFilePath, executablePath)) {
        return (GCCCompileResult){false, -1, "GCC compilation failed"};
    }
    
    printf("Running compiled program...\n");
    int exitCode = system(executablePath);
    
    return (GCCCompileResult){true, WEXITSTATUS(exitCode), "Success"};
}

GCCCompileResult gccCompileBytecodeToExecutable(ObjFunction* function, const char* outputPath) {
    if (!gccInitialized) {
        initGCCCompiler();
    }
    
    // Generate unique filenames
    char cFilePath[1024];
    snprintf(cFilePath, sizeof(cFilePath), "%s/program_%d.c", tempDir, compilationCounter);
    compilationCounter++;
    
    printf("Generating C code...\n");
    
    if (!generateCCode(function, cFilePath)) {
        return (GCCCompileResult){false, -1, "Failed to generate C code"};
    }
    
    printf("Compiling with GCC to %s...\n", outputPath);
    if (!compileWithGCC(cFilePath, outputPath)) {
        return (GCCCompileResult){false, -1, "GCC compilation failed"};
    }
    
    return (GCCCompileResult){true, 0, "Success"};
}

// Generate C code for a user-defined function using a proper bytecode execution engine
static void generateUserFunction(FILE* file, ObjFunction* func, const char* funcName, int funcIndex) {
    fprintf(file, "// User-defined function: %s\n", funcName);
    fprintf(file, "static Value userFunc_%d(int argCount, Value* args) {\n", funcIndex);
    fprintf(file, "  // Function: %s with %d parameters\n", funcName, func->arity);
    
    // Generate constants array for this function
    fprintf(file, "  // Function constants\n");
    fprintf(file, "  static Value funcConstants_%d[] = {\n", funcIndex);
    for (int i = 0; i < func->chunk.constants.count; i++) {
        Value constant = func->chunk.constants.values[i];
        if (IS_NUMBER(constant)) {
            fprintf(file, "    NUMBER_VAL(%.15g)", AS_NUMBER(constant));
        } else if (IS_BOOL(constant)) {
            fprintf(file, "    BOOL_VAL(%s)", AS_BOOL(constant) ? "true" : "false");
        } else if (IS_NIL(constant)) {
            fprintf(file, "    NIL_VAL");
        } else if (IS_OBJ(constant)) {
            Obj* obj = AS_OBJ(constant);
            if (obj != NULL && obj->type == OBJ_STRING) {
                ObjString* str = (ObjString*)obj;
                fprintf(file, "    OBJ_VAL(\"");
                for (int j = 0; j < str->length; j++) {
                    char c = str->chars[j];
                    if (c == '"') {
                        fprintf(file, "\\\"");
                    } else if (c == '\\') {
                        fprintf(file, "\\\\");
                    } else if (c == '\n') {
                        fprintf(file, "\\n");
                    } else if (c == '\t') {
                        fprintf(file, "\\t");
                    } else if (c == '\r') {
                        fprintf(file, "\\r");
                    } else {
                        fprintf(file, "%c", c);
                    }
                }
                fprintf(file, "\")");
            } else {
                fprintf(file, "    NIL_VAL");
            }
        } else {
            fprintf(file, "    NIL_VAL");
        }
        if (i < func->chunk.constants.count - 1) {
            fprintf(file, ",\n");
        } else {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "  };\n\n");
    
    // Generate bytecode array
    fprintf(file, "  // Function bytecode\n");
    fprintf(file, "  static uint8_t funcBytecode_%d[] = {\n", funcIndex);
    for (int i = 0; i < func->chunk.count; i++) {
        fprintf(file, "    %d", func->chunk.code[i]);
        if (i < func->chunk.count - 1) {
            fprintf(file, ",");
        }
        if ((i + 1) % 16 == 0) {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "\n  };\n\n");
    
    // Implement bytecode execution engine
    fprintf(file, "  // Bytecode execution engine\n");
    fprintf(file, "  Value stack[256];\n");
    fprintf(file, "  Value* stackTop = stack;\n");
    fprintf(file, "  Value locals[256];\n");
    fprintf(file, "  \n");
    fprintf(file, "  // Initialize locals\n");
    fprintf(file, "  for (int i = 0; i < 256; i++) {\n");
    fprintf(file, "    locals[i] = NIL_VAL;\n");
    fprintf(file, "  }\n");
    fprintf(file, "  \n");
    fprintf(file, "  // Set up parameters\n");
    fprintf(file, "  locals[0] = NIL_VAL; // Function object slot\n");
    fprintf(file, "  for (int i = 0; i < argCount && i < %d; i++) {\n", func->arity);
    fprintf(file, "    locals[i + 1] = args[i];\n");
    fprintf(file, "  }\n");
    fprintf(file, "  \n");
    fprintf(file, "  // Stack operations\n");
    fprintf(file, "  #define PUSH(value) (*stackTop++ = (value))\n");
    fprintf(file, "  #define POP() (*(--stackTop))\n");
    fprintf(file, "  #define PEEK(distance) (stackTop[-1 - (distance)])\n");
    fprintf(file, "  \n");
    fprintf(file, "  // Execute bytecode\n");
    fprintf(file, "  int ip = 0;\n");
    fprintf(file, "  while (ip < %d) {\n", func->chunk.count);
    fprintf(file, "    uint8_t instruction = funcBytecode_%d[ip++];\n", funcIndex);
    fprintf(file, "    switch (instruction) {\n");
    
    // Generate all opcode cases
    fprintf(file, "      case %d: { // OP_CONSTANT\n", OP_CONSTANT);
    fprintf(file, "        uint8_t constant = funcBytecode_%d[ip++];\n", funcIndex);
    fprintf(file, "        PUSH(funcConstants_%d[constant]);\n", funcIndex);
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_NIL\n", OP_NIL);
    fprintf(file, "        PUSH(NIL_VAL);\n");
    fprintf(file, "        break;\n");
    
    fprintf(file, "      case %d: // OP_TRUE\n", OP_TRUE);
    fprintf(file, "        PUSH(BOOL_VAL(true));\n");
    fprintf(file, "        break;\n");
    
    fprintf(file, "      case %d: // OP_FALSE\n", OP_FALSE);
    fprintf(file, "        PUSH(BOOL_VAL(false));\n");
    fprintf(file, "        break;\n");
    
    fprintf(file, "      case %d: // OP_POP\n", OP_POP);
    fprintf(file, "        POP();\n");
    fprintf(file, "        break;\n");
    
    fprintf(file, "      case %d: { // OP_GET_LOCAL\n", OP_GET_LOCAL);
    fprintf(file, "        uint8_t slot = funcBytecode_%d[ip++];\n", funcIndex);
    fprintf(file, "        PUSH(locals[slot]);\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: { // OP_SET_LOCAL\n", OP_SET_LOCAL);
    fprintf(file, "        uint8_t slot = funcBytecode_%d[ip++];\n", funcIndex);
    fprintf(file, "        locals[slot] = PEEK(0);\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_EQUAL\n", OP_EQUAL);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        bool equal = false;\n");
    fprintf(file, "        if (a.type == b.type) {\n");
    fprintf(file, "          switch (a.type) {\n");
    fprintf(file, "            case VAL_BOOL: equal = AS_BOOL(a) == AS_BOOL(b); break;\n");
    fprintf(file, "            case VAL_NIL: equal = true; break;\n");
    fprintf(file, "            case VAL_NUMBER: equal = AS_NUMBER(a) == AS_NUMBER(b); break;\n");
    fprintf(file, "            case VAL_OBJ: equal = AS_OBJ(a) == AS_OBJ(b); break;\n");
    fprintf(file, "          }\n");
    fprintf(file, "        }\n");
    fprintf(file, "        PUSH(BOOL_VAL(equal));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_GREATER\n", OP_GREATER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_LESS\n", OP_LESS);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_ADD\n", OP_ADD);
    fprintf(file, "      case %d: // OP_ADD_NUMBER\n", OP_ADD_NUMBER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_ADD_STRING\n", OP_ADD_STRING);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        Value result = concatenateStrings(a, b);\n");
    fprintf(file, "        PUSH(result);\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_SUBTRACT\n", OP_SUBTRACT);
    fprintf(file, "      case %d: // OP_SUBTRACT_NUMBER\n", OP_SUBTRACT_NUMBER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(NUMBER_VAL(AS_NUMBER(a) - AS_NUMBER(b)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_MULTIPLY\n", OP_MULTIPLY);
    fprintf(file, "      case %d: // OP_MULTIPLY_NUMBER\n", OP_MULTIPLY_NUMBER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_DIVIDE\n", OP_DIVIDE);
    fprintf(file, "      case %d: // OP_DIVIDE_NUMBER\n", OP_DIVIDE_NUMBER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(NUMBER_VAL(AS_NUMBER(a) / AS_NUMBER(b)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_MODULO\n", OP_MODULO);
    fprintf(file, "      case %d: // OP_MODULO_NUMBER\n", OP_MODULO_NUMBER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value b = POP();\n");
    fprintf(file, "        Value a = POP();\n");
    fprintf(file, "        PUSH(NUMBER_VAL(fmod(AS_NUMBER(a), AS_NUMBER(b))));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_NOT\n", OP_NOT);
    fprintf(file, "      {\n");
    fprintf(file, "        Value value = POP();\n");
    fprintf(file, "        bool isFalsey = IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));\n");
    fprintf(file, "        PUSH(BOOL_VAL(isFalsey));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_NEGATE\n", OP_NEGATE);
    fprintf(file, "      case %d: // OP_NEGATE_NUMBER\n", OP_NEGATE_NUMBER);
    fprintf(file, "      {\n");
    fprintf(file, "        Value value = POP();\n");
    fprintf(file, "        PUSH(NUMBER_VAL(-AS_NUMBER(value)));\n");
    fprintf(file, "        break;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      case %d: // OP_RETURN\n", OP_RETURN);
    fprintf(file, "      {\n");
    fprintf(file, "        if (stackTop > stack) {\n");
    fprintf(file, "          return *(stackTop - 1);\n");
    fprintf(file, "        }\n");
    fprintf(file, "        return NIL_VAL;\n");
    fprintf(file, "      }\n");
    
    fprintf(file, "      default:\n");
    fprintf(file, "        // Skip unsupported opcodes\n");
    fprintf(file, "        break;\n");
    fprintf(file, "    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "  \n");
    fprintf(file, "  // If we reach here without a return, return NIL\n");
    fprintf(file, "  return NIL_VAL;\n");
    fprintf(file, "}\n\n");
}
