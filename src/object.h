//> Strings object-h
#ifndef gem_object_h
#define gem_object_h

#include <stdio.h>
#include "common.h"
//> Calls and Functions object-include-chunk
#include "chunk.h"
//< Calls and Functions object-include-chunk
//> Classes and Instances object-include-table
#include "table.h"
//< Classes and Instances object-include-table
#include "value.h"
//> Memory Safety
// Memory safety state for borrow checking
typedef enum {
  BORROW_NONE,       // Object is not borrowed
  BORROW_SHARED,     // Object has shared (immutable) borrows
  BORROW_EXCLUSIVE   // Object has an exclusive (mutable) borrow
} BorrowState;

typedef struct BorrowInfo {
  BorrowState state;
  int sharedCount;      // Number of shared borrows
  int scopeDepth;       // Scope depth where object was created
  bool isDropped;       // Whether object has been dropped
} BorrowInfo;
//< Memory Safety
//> obj-type-macro

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)
//< obj-type-macro
//> is-string

//> Methods and Initializers is-bound-method
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
//< Methods and Initializers is-bound-method
//> Classes and Instances is-class
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
//< Classes and Instances is-class
//> Closures is-closure
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
//< Closures is-closure
//> Calls and Functions is-function
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
//< Calls and Functions is-function
//> Hash Objects is-hash
#define IS_HASH(value)         isObjType(value, OBJ_HASH)
//< Hash Objects is-hash
//> Classes and Instances is-instance
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
//< Classes and Instances is-instance
//> Module System is-module
#define IS_MODULE(value)       isObjType(value, OBJ_MODULE)
//< Module System is-module
//> Calls and Functions is-native
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
//< Calls and Functions is-native
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
//< is-string
//> as-string

//> Methods and Initializers as-bound-method
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
//< Methods and Initializers as-bound-method
//> Classes and Instances as-class
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
//< Classes and Instances as-class
//> Closures as-closure
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
//< Closures as-closure
//> Calls and Functions as-function
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
//< Calls and Functions as-function
//> Hash Objects as-hash
#define AS_HASH(value)         ((ObjHash*)AS_OBJ(value))
//< Hash Objects as-hash
//> Classes and Instances as-instance
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
//< Classes and Instances as-instance
//> Module System as-module
#define AS_MODULE(value)       ((ObjModule*)AS_OBJ(value))
//< Module System as-module
//> Calls and Functions as-native
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
//< Calls and Functions as-native
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (AS_STRING(value)->chars)
//< as-string
//> obj-type

typedef enum {
//> Methods and Initializers obj-type-bound-method
  OBJ_BOUND_METHOD,
//< Methods and Initializers obj-type-bound-method
//> Classes and Instances obj-type-class
  OBJ_CLASS,
//< Classes and Instances obj-type-class
//> Closures obj-type-closure
  OBJ_CLOSURE,
//< Closures obj-type-closure
//> Calls and Functions obj-type-function
  OBJ_FUNCTION,
//< Calls and Functions obj-type-function
//> Hash Objects obj-type-hash
  OBJ_HASH,
//< Hash Objects obj-type-hash
//> Classes and Instances obj-type-instance
  OBJ_INSTANCE,
//< Classes and Instances obj-type-instance
//> Module System obj-type-module
  OBJ_MODULE,
//< Module System obj-type-module
//> Calls and Functions obj-type-native
  OBJ_NATIVE,
//< Calls and Functions obj-type-native
  OBJ_STRING,
//> Closures obj-type-upvalue
  OBJ_UPVALUE
//< Closures obj-type-upvalue
} ObjType;
//< obj-type

//> return-type-enum
typedef enum {
  RETURN_TYPE_INT,
  RETURN_TYPE_STRING,
  RETURN_TYPE_BOOL,
  RETURN_TYPE_VOID,
  RETURN_TYPE_FUNC,
  RETURN_TYPE_OBJ,
  RETURN_TYPE_HASH
} BaseType;

// New type system with mutability and nullability
typedef struct {
  BaseType baseType;
  bool isNullable;   // true if type ends with ? or ?!
  bool isMutable;    // true if type ends with ! or ?!
  ObjString* className; // For obj types, stores the specific class name
} GemType;

// Legacy typedef for compatibility
typedef GemType ReturnType;

// Helper macros for creating types
#define IMMUTABLE_NONNULL_TYPE(base) ((GemType){base, false, false, NULL})
#define NULLABLE_IMMUTABLE_TYPE(base) ((GemType){base, true, false, NULL})
#define MUTABLE_NONNULL_TYPE(base) ((GemType){base, false, true, NULL})
#define NULLABLE_MUTABLE_TYPE(base) ((GemType){base, true, true, NULL})

// Helper macros for class-specific object types
#define CLASS_IMMUTABLE_NONNULL_TYPE(className) ((GemType){RETURN_TYPE_OBJ, false, false, className})
#define CLASS_NULLABLE_IMMUTABLE_TYPE(className) ((GemType){RETURN_TYPE_OBJ, true, false, className})
#define CLASS_MUTABLE_NONNULL_TYPE(className) ((GemType){RETURN_TYPE_OBJ, false, true, className})
#define CLASS_NULLABLE_MUTABLE_TYPE(className) ((GemType){RETURN_TYPE_OBJ, true, true, className})

// Helper functions for type checking
static inline bool gemTypesEqual(GemType a, GemType b) {
  if (a.baseType != b.baseType || a.isNullable != b.isNullable || a.isMutable != b.isMutable) {
    return false;
  }
  
  // For object types, we'll use a separate function that can handle string comparison
  if (a.baseType == RETURN_TYPE_OBJ) {
    // Forward declaration - actual implementation in object.c
    extern bool gemClassNamesEqual(ObjString* a, ObjString* b);
    
    if (a.className == NULL && b.className == NULL) return true;
    if (a.className == NULL || b.className == NULL) return false;
    return gemClassNamesEqual(a.className, b.className);
  }
  
  return true;
}

static inline bool gemIsAssignmentCompatible(GemType varType, GemType valueType) {
  // Special case: nil can be assigned to any nullable variable
  if (valueType.baseType == RETURN_TYPE_VOID && valueType.isNullable) {
    return varType.isNullable;
  }
  
  // Base types must match
  if (varType.baseType != valueType.baseType) {
    return false;
  }
  
  // For object types, check class compatibility
  if (varType.baseType == RETURN_TYPE_OBJ) {
    // Forward declaration - actual implementation in object.c
    extern bool gemClassNamesEqual(ObjString* a, ObjString* b);
    
    // If variable type has no specific class (generic obj), accept any class
    if (varType.className == NULL) {
      return true;
    }
    
    // If value has no specific class but variable expects one, not compatible
    if (valueType.className == NULL) {
      return false;
    }
    
    // Both have specific classes - they must match
    return gemClassNamesEqual(varType.className, valueType.className);
  }
  
  // Nullable variable can accept non-nullable value, but not vice versa
  if (!varType.isNullable && valueType.isNullable) {
    return false;
  }
  
  // For assignment to work, we don't require exact mutability match
  // but the variable being assigned to doesn't need to be mutable 
  // (the mutability check happens at assignment time)
  return true;
}

// Redefine the macros to use the proper functions
#undef typesEqual
#undef isAssignmentCompatible
#define typesEqual(a, b) gemTypesEqual(a, b)
#define isAssignmentCompatible(a, b) gemIsAssignmentCompatible(a, b)

// Common type constants for backward compatibility
static const GemType TYPE_INT = {RETURN_TYPE_INT, false, false, NULL};
static const GemType TYPE_STRING = {RETURN_TYPE_STRING, false, false, NULL};
static const GemType TYPE_BOOL = {RETURN_TYPE_BOOL, false, false, NULL};
static const GemType TYPE_VOID = {RETURN_TYPE_VOID, false, false, NULL};
static const GemType TYPE_FUNC = {RETURN_TYPE_FUNC, false, false, NULL};
static const GemType TYPE_OBJ = {RETURN_TYPE_OBJ, false, false, NULL};
static const GemType TYPE_HASH = {RETURN_TYPE_HASH, false, false, NULL};

// Helper functions for backward compatibility
static inline GemType makeType(BaseType baseType) {
  return (GemType){baseType, false, false, NULL};
}

static inline GemType makeClassType(BaseType baseType, ObjString* className) {
  return (GemType){baseType, false, false, className};
}

static inline bool typeEqualsBase(GemType type, BaseType baseType) {
  return type.baseType == baseType;
}

static inline bool typeNotEqualsBase(GemType type, BaseType baseType) {
  return type.baseType != baseType;
}
//< return-type-enum

struct Obj {
  ObjType type;
//> next-field
  struct Obj* next;
//< next-field
//> Memory Safety Fields
  BorrowInfo borrowInfo;
  int refCount;         // Reference count for automatic cleanup
//< Memory Safety Fields
};
//> Calls and Functions obj-function

typedef struct {
  Obj obj;
  int arity;
//> Closures upvalue-count
  int upvalueCount;
//< Closures upvalue-count
  Chunk chunk;
  ObjString* name;
//> return-type-field
  ReturnType returnType;
//< return-type-field
} ObjFunction;
//< Calls and Functions obj-function
//> Calls and Functions obj-native

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;
//< Calls and Functions obj-native
//> obj-string

struct ObjString {
  Obj obj;
  int length;
  bool ownsChars;  // true if this string owns its character array, false for constant strings
//> Hash Tables obj-string-hash
  uint32_t hash;
//< Hash Tables obj-string-hash
  char* chars;     // points to character data (either embedded or external)
  char embedded[]; // flexible array member for owned strings (when ownsChars is true)
};
//< obj-string
//> Closures obj-upvalue
typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
//> closed-field
  Value closed;
//< closed-field
//> next-field
  struct ObjUpvalue* next;
//< next-field
} ObjUpvalue;
//< Closures obj-upvalue
//> Closures obj-closure
typedef struct {
  Obj obj;
  ObjFunction* function;
//> upvalue-fields
  ObjUpvalue** upvalues;
  int upvalueCount;
//< upvalue-fields
} ObjClosure;
//< Closures obj-closure
//> Classes and Instances obj-class

typedef struct {
  Obj obj;
  ObjString* name;
//> Methods and Initializers class-methods
  Table methods;
//< Methods and Initializers class-methods
} ObjClass;
//< Classes and Instances obj-class
//> Classes and Instances obj-instance

typedef struct {
  Obj obj;
  ObjClass* klass;
  Table fields; // [fields]
} ObjInstance;
//< Classes and Instances obj-instance

//> Hash Objects obj-hash
typedef struct {
  Obj obj;
  Table table; // Hash table for key-value pairs
} ObjHash;
//< Hash Objects obj-hash

//> Module System obj-module
typedef struct {
  Obj obj;
  ObjString* name;
  Table functions; // Functions defined in this module
} ObjModule;
//< Module System obj-module

//> Methods and Initializers obj-bound-method
typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure* method;
} ObjBoundMethod;

//< Methods and Initializers obj-bound-method
//> Methods and Initializers new-bound-method-h
ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method);
//< Methods and Initializers new-bound-method-h
//> Classes and Instances new-class-h
ObjClass* newClass(ObjString* name);
//< Classes and Instances new-class-h
//> Closures new-closure-h
ObjClosure* newClosure(ObjFunction* function);
//< Closures new-closure-h
//> Calls and Functions new-function-h
ObjFunction* newFunction();
//< Calls and Functions new-function-h
//> Classes and Instances new-instance-h
ObjInstance* newInstance(ObjClass* klass);
//< Classes and Instances new-instance-h
//> Hash Objects new-hash-h
ObjHash* newHash();
//< Hash Objects new-hash-h
//> Module System new-module-h
ObjModule* newModule(ObjString* name);
//< Module System new-module-h
//> Calls and Functions new-native-h
ObjNative* newNative(NativeFn function);
//< Calls and Functions new-native-h
//> take-string-h
ObjString* takeString(char* chars, int length);
//< take-string-h
//> constant-string-h
ObjString* constantString(const char* chars, int length);
//< constant-string-h
//> copy-string-h
ObjString* copyString(const char* chars, int length);
//> Closures new-upvalue-h
ObjUpvalue* newUpvalue(Value* slot);
//< Closures new-upvalue-h
//> print-object-h
void printObject(Value value);
//< print-object-h

//> String comparison for type system
bool gemClassNamesEqual(ObjString* a, ObjString* b);
//< String comparison for type system

//> Memory Safety Functions
// Memory safety function declarations
void initObjectMemorySafety(Obj* obj, int scopeDepth);
bool tryBorrowShared(Obj* obj);
bool tryBorrowExclusive(Obj* obj);
void releaseBorrow(Obj* obj, bool isExclusive);
void incrementRef(Obj* obj);
void decrementRef(Obj* obj);
bool isObjectDropped(Obj* obj);
void dropObject(Obj* obj);
void cleanupScopeObjects(int scopeDepth);
//< Memory Safety Functions

//< copy-string-h
//> is-obj-type
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

//< is-obj-type
#endif
