#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct
{
    StringObject *key;
    Value value;
} Entry;

typedef struct
{
    int count;
    int capacity;
    Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

// adds an entry to the hash table.
// if an entry is already present for the key, it is overwritten by the new value
// returns true if the entry was added
bool tableAdd(Table *table, StringObject *key, Value value);

// add all entries of one hash table to another
void tableAddAll(Table *from, Table *to);
#endif