//> Hash Tables table-h
#ifndef gem_table_h
#define gem_table_h

#include "common.h"
#include "value.h"
//> entry

typedef struct {
  ObjString* key;
  Value value;
} Entry;
//< entry

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

//> init-table-h
void initTable(Table* table);
//> free-table-h
void freeTable(Table* table);
//< free-table-h
//> table-get-h
bool tableGet(Table* table, ObjString* key, Value* value);
//< table-get-h
//> table-set-h
bool tableSet(Table* table, ObjString* key, Value value);
//< table-set-h
//> table-delete-h
bool tableDelete(Table* table, ObjString* key);
//< table-delete-h
//> table-add-all-h
void tableAddAll(Table* from, Table* to);
//< table-add-all-h
//> table-find-string-h
ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash);
//< table-find-string-h

//< init-table-h
#endif
