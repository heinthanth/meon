#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "object.h"
#include "table.h"
#include "vm.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table)
{
    table->size = 0;
    table->maxSize = 0;
    table->items = NULL;
}

void freeTable(Table *table)
{
    FREE_ARRAY(TableItem, table->items, table->maxSize);
    initTable(table);
}

static TableItem *findTableItem(TableItem *entries, int maxSize, ObjectString *k)
{
    uint32_t index = k->hash % maxSize;
    TableItem *tombstone = NULL;

    for (;;)
    {
        TableItem *item = &entries[index];
        if (item->k == NULL)
        {
            if (IS_NULL(item->v))
            {
                // Empty item.
                return tombstone != NULL ? tombstone : item;
            }
            else
            {
                // We found a tombstone.
                if (tombstone == NULL)
                    tombstone = item;
            }
        }
        else if (item->k == k)
        {
            // We found the k.
            return item;
        }
        index = (index + 1) % maxSize;
    }
}

bool tableGet(Table *table, ObjectString *k, Value *v)
{
    if (table->size == 0)
        return false;

    TableItem *item = findTableItem(table->items, table->maxSize, k);
    if (item->k == NULL)
        return false;

    *v = item->v;
    return true;
}

static void adjustMaxSize(Table *table, int maxSize)
{
    TableItem *entries = ALLOCATE(TableItem, maxSize);
    for (int i = 0; i < maxSize; i++)
    {
        entries[i].k = NULL;
        entries[i].v = NULL_VAL;
    }

    table->size = 0;
    for (int i = 0; i < table->maxSize; i++)
    {
        TableItem *item = &table->items[i];
        if (item->k == NULL)
            continue;

        TableItem *dest = findTableItem(entries, maxSize, item->k);
        dest->k = item->k;
        dest->k = item->k;
        table->size++;
    }

    FREE_ARRAY(TableItem, table->items, table->maxSize);
    table->items = entries;
    table->maxSize = maxSize;
}

bool tableSet(Table *table, ObjectString *k, Value v)
{
    if (table->size + 1 > table->maxSize * TABLE_MAX_LOAD)
    {
        int s = GROW_ARRAY_SIZE(table->maxSize);
        adjustMaxSize(table, s);
    }

    TableItem *item = findTableItem(table->items, table->maxSize, k);

    bool isNew = item->k == NULL;
    if (isNew && IS_NULL(item->v))
        table->size++;

    item->k = k;
    item->v = v;
    return isNew;
}

bool tableDelete(Table *table, ObjectString *k)
{
    if (table->size == 0)
        return false;

    // Find the item.
    TableItem *item = findTableItem(table->items, table->maxSize, k);
    if (item->k == NULL)
        return false;

    // Place a tombstone in the item.
    item->k = NULL;
    item->v = BOOL_VAL(true);

    return true;
}

void tableAddAll(Table *from, Table *to)
{
    for (int i = 0; i < from->maxSize; i++)
    {
        TableItem *item = &from->items[i];
        if (item->k != NULL)
        {
            tableSet(to, item->k, item->v);
        }
    }
}

ObjectString *tableFindString(Table *table, const char *chars,
                           int length, uint32_t hash)
{
    if (table->size == 0)
        return NULL;

    uint32_t index = hash % table->maxSize;

    for (;;)
    {
        TableItem *item = &table->items[index];

        if (item->k == NULL)
        {
            // Stop if we find an empty non-tombstone item.
            if (IS_NULL(item->v))
                return NULL;
        }
        else if (item->k->length == length &&
                 item->k->hash == hash &&
                 memcmp(item->k->chars, chars, length) == 0)
        {
            // We found it.
            return item->k;
        }

        index = (index + 1) % table->maxSize;
    }
}