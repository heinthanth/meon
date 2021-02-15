#ifndef meon_table_h
#define meon_table_h

#include "common.h"
#include "value.h"

typedef struct
{
    ObjectString *k;
    Value v;
} TableItem;

typedef struct
{
    int size;
    int maxSize;
    TableItem *items;
} Table;

void initTable(Table *table);
void freeTable(Table *table);
bool tableGet(Table* table, ObjectString* k, Value* v);
bool tableSet(Table *table, ObjectString *k, Value v);
void tableAddAll(Table* from, Table* to);
ObjectString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
bool tableDelete(Table* table, ObjectString* k);

#endif