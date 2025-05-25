//> Strings object-c
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
//> Hash Tables object-include-table
#include "table.h"
//< Hash Tables object-include-table
#include "value.h"
#include "vm.h"
//> allocate-obj

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)
//< allocate-obj
//> allocate-object

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
//> add-to-list
  
  object->next = vm.objects;
  vm.objects = object;
//< add-to-list
//> Memory Safety Initialization
  // Initialize memory safety fields
  object->borrowInfo.state = BORROW_NONE;
  object->borrowInfo.sharedCount = 0;
  object->borrowInfo.scopeDepth = 0; // Will be set by caller
  object->borrowInfo.isDropped = false;
  object->refCount = 1; // Start with 1 reference
//< Memory Safety Initialization
  return object;
}
//< allocate-object
//> Methods and Initializers new-bound-method
ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod,
                                       OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
//> Memory Safety Init Bound Method
  initObjectMemorySafety((Obj*)bound, vm.currentScopeDepth);
//< Memory Safety Init Bound Method
  return bound;
}
//< Methods and Initializers new-bound-method
//> Classes and Instances new-class
ObjClass* newClass(ObjString* name) {
  ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name; // [klass]
//> Methods and Initializers init-methods
  initTable(&klass->methods);
//< Methods and Initializers init-methods
//> Memory Safety Init Class
  initObjectMemorySafety((Obj*)klass, vm.currentScopeDepth);
//< Memory Safety Init Class
  return klass;
}
//< Classes and Instances new-class
//> Closures new-closure
ObjClosure* newClosure(ObjFunction* function) {
//> allocate-upvalue-array
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*,
                                   function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

//< allocate-upvalue-array
  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
//> init-upvalue-fields
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
//< init-upvalue-fields
//> Memory Safety Init Closure
  initObjectMemorySafety((Obj*)closure, vm.currentScopeDepth);
//< Memory Safety Init Closure
  return closure;
}
//< Closures new-closure
//> Calls and Functions new-function
ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
//> Closures init-upvalue-count
  function->upvalueCount = 0;
//< Closures init-upvalue-count
  function->name = NULL;
//> init-return-type
  function->returnType = TYPE_VOID; // Default to void
//< init-return-type
  initChunk(&function->chunk);
//> Memory Safety Init Function
  initObjectMemorySafety((Obj*)function, vm.currentScopeDepth);
//< Memory Safety Init Function
  return function;
}
//< Calls and Functions new-function
//> Classes and Instances new-instance
ObjInstance* newInstance(ObjClass* klass) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
//> Memory Safety Init Instance
  initObjectMemorySafety((Obj*)instance, vm.currentScopeDepth);
//< Memory Safety Init Instance
  return instance;
}
//< Classes and Instances new-instance
//> Hash Objects new-hash
ObjHash* newHash() {
  ObjHash* hash = ALLOCATE_OBJ(ObjHash, OBJ_HASH);
  initTable(&hash->table);
//> Memory Safety Init Hash
  initObjectMemorySafety((Obj*)hash, vm.currentScopeDepth);
//< Memory Safety Init Hash
  return hash;
}
//< Hash Objects new-hash
//> Module System new-module
ObjModule* newModule(ObjString* name) {
  ObjModule* module = ALLOCATE_OBJ(ObjModule, OBJ_MODULE);
  module->name = name;
  initTable(&module->functions);
//> Memory Safety Init Module
  initObjectMemorySafety((Obj*)module, vm.currentScopeDepth);
//< Memory Safety Init Module
  return module;
}
//< Module System new-module
//> Calls and Functions new-native
ObjNative* newNative(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
//> Memory Safety Init Native
  initObjectMemorySafety((Obj*)native, vm.currentScopeDepth);
//< Memory Safety Init Native
  return native;
}
//< Calls and Functions new-native

//> allocate-string
//> Hash Tables allocate-string
static ObjString* allocateString(char* chars, int length,
                                 uint32_t hash, bool ownsChars) {
//< Hash Tables allocate-string
  // Calculate allocation size based on whether we own the characters
  size_t objectSize = ownsChars ? 
    sizeof(ObjString) + (length + 1) * sizeof(char) :
    sizeof(ObjString);
    
  ObjString* string = (ObjString*)allocateObject(objectSize, OBJ_STRING);
  string->length = length;
  string->ownsChars = ownsChars;
//> Hash Tables allocate-store-hash
  string->hash = hash;
//< Hash Tables allocate-store-hash

  if (ownsChars) {
    // Copy characters into the flexible array member and point chars to it
    memcpy(string->embedded, chars, length);
    string->embedded[length] = '\0';
    string->chars = string->embedded;
  } else {
    // For constant strings, just point to the external data
    string->chars = chars;
  }

//> Memory Safety Init String
  initObjectMemorySafety((Obj*)string, vm.currentScopeDepth);
//< Memory Safety Init String
//> Hash Tables allocate-store-string
  tableSet(&vm.strings, string, NIL_VAL);
//< Hash Tables allocate-store-string
  return string;
}
//< allocate-string
//> Hash Tables hash-string
static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}
//< Hash Tables hash-string
//> take-string
ObjString* takeString(char* chars, int length) {
/* Strings take-string < Hash Tables take-string-hash
  return allocateString(chars, length);
*/
//> Hash Tables take-string-hash
  uint32_t hash = hashString(chars, length);
//> take-string-intern
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

//< take-string-intern
  return allocateString(chars, length, hash, true);
//< Hash Tables take-string-hash
}
//< take-string
//> constant-string
ObjString* constantString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) return interned;

  return allocateString((char*)chars, length, hash, false);
}
//< constant-string
ObjString* copyString(const char* chars, int length) {
//> Hash Tables copy-string-hash
  uint32_t hash = hashString(chars, length);
//> copy-string-intern
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) return interned;

//< copy-string-intern
//< Hash Tables copy-string-hash
  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
/* Strings object-c < Hash Tables copy-string-allocate
  return allocateString(heapChars, length);
*/
//> Hash Tables copy-string-allocate
  return allocateString(heapChars, length, hash, true);
//< Hash Tables copy-string-allocate
}
//> Closures new-upvalue
ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
//> init-closed
  upvalue->closed = NIL_VAL;
//< init-closed
  upvalue->location = slot;
//> init-next
  upvalue->next = NULL;
//< init-next
//> Memory Safety Init Upvalue
  initObjectMemorySafety((Obj*)upvalue, vm.currentScopeDepth);
//< Memory Safety Init Upvalue
  return upvalue;
}
//< Closures new-upvalue
//> Calls and Functions print-function-helper
static void printFunction(ObjFunction* function) {
//> print-script
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
//< print-script
  printf("<fn %s>", AS_CSTRING(OBJ_VAL(function->name)));
}
//< Calls and Functions print-function-helper
//> print-object
void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
//> Methods and Initializers print-bound-method
    case OBJ_BOUND_METHOD:
      printFunction(AS_BOUND_METHOD(value)->method->function);
      break;
//< Methods and Initializers print-bound-method
//> Classes and Instances print-class
    case OBJ_CLASS:
      printf("%s", AS_CSTRING(OBJ_VAL(AS_CLASS(value)->name)));
      break;
//< Classes and Instances print-class
//> Closures print-closure
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
//< Closures print-closure
//> Calls and Functions print-function
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
//< Calls and Functions print-function
//> Hash Objects print-hash
    case OBJ_HASH: {
      ObjHash* hash = AS_HASH(value);
      printf("{");
      bool first = true;
      for (int i = 0; i < hash->table.capacity; i++) {
        if (hash->table.entries[i].key != NULL) {
          if (!first) printf(", ");
          printf("\"%s\": ", AS_CSTRING(OBJ_VAL(hash->table.entries[i].key)));
          printValue(hash->table.entries[i].value);
          first = false;
        }
      }
      printf("}");
      break;
    }
//< Hash Objects print-hash
//> Classes and Instances print-instance
    case OBJ_INSTANCE:
      printf("%s instance",
             AS_CSTRING(OBJ_VAL(AS_INSTANCE(value)->klass->name)));
      break;
//< Classes and Instances print-instance
//> Module System print-module
    case OBJ_MODULE:
      printf("module %s", AS_CSTRING(OBJ_VAL(AS_MODULE(value)->name)));
      break;
//< Module System print-module
//> Calls and Functions print-native
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
//< Calls and Functions print-native
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
//> Closures print-upvalue
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
//< Closures print-upvalue
  }
}
//< print-object

//> Memory Safety Implementation
// Memory safety function implementations

void initObjectMemorySafety(Obj* obj, int scopeDepth) {
  obj->borrowInfo.scopeDepth = scopeDepth;
}

bool tryBorrowShared(Obj* obj) {
  if (obj->borrowInfo.isDropped) {
    return false; // Cannot borrow dropped object
  }
  
  if (obj->borrowInfo.state == BORROW_EXCLUSIVE) {
    return false; // Cannot share if exclusively borrowed
  }
  
  obj->borrowInfo.state = BORROW_SHARED;
  obj->borrowInfo.sharedCount++;
  return true;
}

bool tryBorrowExclusive(Obj* obj) {
  if (obj->borrowInfo.isDropped) {
    return false; // Cannot borrow dropped object
  }
  
  if (obj->borrowInfo.state != BORROW_NONE) {
    return false; // Cannot exclusively borrow if already borrowed
  }
  
  obj->borrowInfo.state = BORROW_EXCLUSIVE;
  return true;
}

void releaseBorrow(Obj* obj, bool isExclusive) {
  if (isExclusive) {
    obj->borrowInfo.state = BORROW_NONE;
  } else {
    obj->borrowInfo.sharedCount--;
    if (obj->borrowInfo.sharedCount == 0) {
      obj->borrowInfo.state = BORROW_NONE;
    }
  }
}

void incrementRef(Obj* obj) {
  obj->refCount++;
}

void decrementRef(Obj* obj) {
  obj->refCount--;
  if (obj->refCount <= 0 && !obj->borrowInfo.isDropped) {
    dropObject(obj);
  }
}

bool isObjectDropped(Obj* obj) {
  return obj->borrowInfo.isDropped;
}

void dropObject(Obj* obj) {
  if (obj->borrowInfo.isDropped) {
    return; // Already dropped
  }
  
  obj->borrowInfo.isDropped = true;
  // Note: We don't immediately free the memory here to prevent use-after-free
  // The object will be cleaned up when all references are gone
}

void cleanupScopeObjects(int scopeDepth) {
  // Walk through all objects and drop those created in this scope or deeper
  Obj* current = vm.objects;
  while (current != NULL) {
    if (current->borrowInfo.scopeDepth >= scopeDepth && 
        !current->borrowInfo.isDropped) {
      dropObject(current);
    }
    current = current->next;
  }
}
//< Memory Safety Implementation

//> String comparison function for type system
bool gemClassNamesEqual(ObjString* a, ObjString* b) {
  if (a == b) return true; // Same pointer
  if (a == NULL || b == NULL) return false; // One is null
  return a->length == b->length &&
         memcmp(AS_CSTRING(OBJ_VAL(a)), AS_CSTRING(OBJ_VAL(b)), a->length) == 0;
}
//< String comparison function for type system

//> is-obj-type
