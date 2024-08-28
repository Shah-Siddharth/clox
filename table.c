#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// load factor for the hash table
#define TABLE_MAX_LOAD 0.75

// allocate array of empty buckets
static void adjustCapacity(Table *table, int capacity)
{
    // create a new array of desired capacity
    Entry *newEntries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++)
    {
        newEntries[i].key = NULL;
        newEntries[i].value = NIL_VAL;
    }

    // insert all entries from old array to new array
    for (int i = 0; i < table->capacity; i++)
    {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry *dest = findEntry(entry->key, newEntries, capacity);
        dest->key = entry->key;
        dest->value = entry->value;
    }

    // release memory for old entries array
    FREE_ARRAY(Entry, table->entries, table->capacity);

    // update hash table to point to new array
    table->entries = newEntries;
    table->capacity = capacity;
}

// looks up existing entries in the hash table
static Entry *findEntry(StringObject *key, Entry *entries, int capacity)
{
    uint32_t index = key->hash % capacity;
    for (;;)
    {
        Entry *entry = &entries[index];
        // key is already present or bucket is empty
        if (entry->key == key || entry->key == NULL)
        {
            return entry;
        }

        // linear probing (check the next buckets sequentially)
        index = (index + 1) % capacity;
    }
}

void initTable(Table *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table *table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

void tableAdd(Table *table, StringObject *key, Value value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry *entry = findEntry(key, table->entries, table->capacity);
    bool isNewKey = entry->key == NULL;
    if (isNewKey)
        table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void tableAddAll(Table *from, Table *to)
{
    for (int i = 0; i < from->capacity; i++)
    {
        Entry *entry = &from->entries[i];
        if (entry->key != NULL)
        {
            tableAdd(to, entry->key, entry->value);
        }
    }
}

bool tableDelete(Table *table, StringObject *key)
{
    if (table->count == 0)
        return false;

    // Find the entry
    Entry *entry = findEntry(key, table->entries, table->capacity);
    if (entry->key == NULL)
        return;

    // Delete the entry and place a tombstone (key = NULL and value = True)
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

bool tableGet(Table *table, StringObject *key, Value *value)
{
    if (table->count == 0)
        return false;

    Entry *entry = findEntry(key, table->entries, table->capacity);
    if (entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
}