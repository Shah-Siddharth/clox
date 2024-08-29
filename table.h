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

// deletes an entry from the hash table
bool tableDelete(Table *table, StringObject *key);

//
StringObject *tableFindString(Table *table, const char *chars, int length, uint32_t hash)
{
    if (table->count == 0)
        return NULL;

    uint32_t index = hash % table->capacity;
    for (;;)
    {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL)
        {
            // stop if we find an empty non-tombstone bucket
            if (IS_NIL(entry->value))
                return NULL;
        }
        else if (entry->key->length == length &&
                 entry->key->hash == hash &&
                 memcmp(entry->key->chars, chars, length) == 0)
        {
            // we find the string
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

// retrieves a value from the hash table
// returns true if an entry with the given key is found. Returns false otherwise
// if entry exists, the value argument that was passed will point to the resulting value
bool tableGet(Table *table, StringObject *key, Value *value);

#endif