#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../../lab3/vector/generic.h"
#include "generic.h"
#include "math.h"

#define DJB2_START_HASH 5381
#define DJB2_STEP 33
#define INTERNAL_HASH_CONSTANT 0.6180339887
#define FLAG_SIZE 1
#define NOT_FOUND (-1)

#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define SUCCESS 0
#define FAILURE (-1)

// Хеширование методом умножения
static size_t InternalHashFunc(int key_hash, size_t table_capacity) {
    size_t hash = (size_t) key_hash;
    double fraction = (double) hash * INTERNAL_HASH_CONSTANT -
                      floor((double) hash * INTERNAL_HASH_CONSTANT);
    return (size_t) ((double) table_capacity * fraction);
}

// Функция получения хеша для шага для разрешения коллизий, всегда нечетное, чтобы быть взаимнопростым с capacity
static size_t StepHashFunc(size_t key_hash, size_t table_capacity) {
    size_t step = 1 + (key_hash % (table_capacity - 1));
    if (step % 2 == 0) {
        step++;
    }
    return step;
}

Vector *createVectorWithSize(size_t elem_size, size_t size) {
    //    Vector *new_vector = createVector(elem_size);
    //    void *free_slot = malloc(elem_size);
    //    memset(free_slot, 0, new_vector->elem_size);
    //    if (!free_slot) {
    //        return NULL;
    //    }
    //    for (int i = 0; i < size; i++) {
    //        appendVectorItem(new_vector, free_slot);
    //    }
    //    memset(new_vector->data, 0, new_vector->capacity * new_vector->elem_size);
    //    return new_vector;

    Vector *new_vector = malloc(sizeof(Vector));
    if (!new_vector) {
        return NULL;
    }
    new_vector->size = size;
    new_vector->capacity = size;
    new_vector->elem_size = elem_size;
    new_vector->data = malloc(new_vector->capacity * new_vector->elem_size);
    if (!new_vector->data) {
        free(new_vector);
        return NULL;
    }
    memset(new_vector->data, 0, new_vector->capacity * new_vector->elem_size);
    return new_vector;
}

static char *getSlotType(void *slot) {
    return (char *) slot;
}

static void *getSlotKey(void *slot) {
    return ((char *) slot) + FLAG_SIZE;
}

static void *getSlotValue(void *slot, size_t key_size) {
    return ((char *) slot) + FLAG_SIZE + key_size;
}

static void writeToSlot(void *slot, char slot_type, void *key, void *value, size_t key_size, size_t val_size) {
    memcpy(slot, &slot_type, FLAG_SIZE);
    memcpy(getSlotValue(slot, key_size), value, val_size);
    memcpy(getSlotKey(slot), key, key_size);
}


int HashInt(const void *key) {
    int k = *(const int *) key;
    if (k >= 0) {
        return k;
    }
    return -k;
}

int HashString(const void *key) {
    const unsigned char *str = (const unsigned char *) key;
    size_t hash = DJB2_START_HASH;
    size_t element = *str;

    while (element) {
        hash = (hash * DJB2_STEP) + element;
        element = *(++str);
    }
    return (int) hash;
}

HashTable *createHashTable(size_t key_size, size_t val_size) {
    HashTable *new_table = malloc(sizeof(HashTable));
    if (!new_table) {
        return NULL;
    }
    new_table->values = createVectorWithSize(FLAG_SIZE + key_size + val_size, TABLE_MIN_SIZE);
    if (!new_table->values) {
        free(new_table);
        return NULL;
    }
    new_table->key_size = key_size;
    new_table->val_size = val_size;
    new_table->size = 0;
    new_table->capacity = TABLE_MIN_SIZE;
    return new_table;
}


void setItemHashTable(HashTable *table, void *key, void *data, HashFunc hash, CmpFunc cmp) {
    if (!table || !key || !data || !hash || !cmp) {
        return;
    }

    if (table->size * 2 > table->capacity) {
        rehashHashTable(table, hash, cmp);
    }
    size_t first_deleted_index = NOT_FOUND;
    bool index_not_found = true;
    int key_hash = hash(key);
    size_t multiplicative_hash = InternalHashFunc(key_hash, table->capacity);
    size_t step_hash = StepHashFunc(key_hash, table->capacity);
    for (int i = 0; i < (int) table->capacity; i++) {
        size_t index = (multiplicative_hash + i * step_hash) % (size_t) table->capacity;
        void *slot = getVectorItem(table->values, index);
        char slot_type = *getSlotType(slot);

        if (slot_type == SLOT_DELETED && index_not_found) {
            first_deleted_index = index;
            index_not_found = false;
            continue;
        }
        if (slot_type == SLOT_OCCUPIED) {
            if (cmp(getSlotKey(slot), key)) {
                memcpy(getSlotValue(slot, table->key_size), data, table->val_size);
                return;
            }
        }
        if (slot_type == SLOT_EMPTY) {
            if (!index_not_found) {
                void *first_deleted_slot = getVectorItem(table->values, first_deleted_index);
                writeToSlot(first_deleted_slot, SLOT_OCCUPIED, key, data, table->key_size, table->val_size);
                table->size++;
                return;
            }
            writeToSlot(slot, SLOT_OCCUPIED, key, data, table->key_size, table->val_size);
            table->size++;
            return;
        }
    }
    if (!index_not_found) {
        void *first_deleted_slot = getVectorItem(table->values, first_deleted_index);
        writeToSlot(first_deleted_slot, SLOT_OCCUPIED, key, data, table->key_size, table->val_size);
        table->size++;
        return;
    }
    printf("setItemHashTable: no free slot found. Capacity: %zu, Size: %zu, multiplicative hash: %zu, step hash: %zu\n",
           table->capacity, table->size, multiplicative_hash, step_hash);
    exit(FAILURE);
}

void rehashHashTable(HashTable *table, HashFunc hash, CmpFunc cmp) {
    if (!table || !hash || !cmp) {
        return;
    }

    size_t new_capacity = table->capacity * 2;
    Vector *new_values = createVectorWithSize(FLAG_SIZE + table->key_size + table->val_size, new_capacity);
    if (!new_values) {
        return;
    }

    Vector *old_values = table->values;
    table->values = new_values;
    table->capacity = new_capacity;
    table->size = 0;

    for (size_t i = 0; i < old_values->size; i++) {
        void *slot = getVectorItem(old_values, i);
        if (*getSlotType(slot) == SLOT_OCCUPIED) {
            setItemHashTable(table, getSlotKey(slot), getSlotValue(slot, table->key_size), hash, cmp);
        }
    }
    vectorFree(old_values);
}

void *getItemHashTable(HashTable *table, void *key, HashFunc hash, CmpFunc cmp) {
    if (!table || !key || !hash || !cmp) {
        return NULL;
    }
    if (table->size * 2 > table->capacity) {
        rehashHashTable(table, hash, cmp);
    }

    int key_hash = hash(key);
    size_t multiplicative_hash = InternalHashFunc(key_hash, table->capacity);
    size_t step_hash = StepHashFunc(key_hash, table->capacity);

    for (int i = 0; i < (int) table->capacity; i++) {
        size_t index = ((multiplicative_hash + i * step_hash)) % table->capacity;
        void *slot = getVectorItem(table->values, index);
        char slot_type = *getSlotType(slot);

        if (slot_type == SLOT_EMPTY) {
            return NULL;
        }
        if (slot_type == SLOT_DELETED) {
            continue;
        }

        void *slot_key = getSlotKey(slot);
        if (!cmp(key, slot_key)) {
            continue;
        }
        return getSlotValue(slot, table->key_size);
    }
    return NULL;
}

void *popItemHashTable(HashTable *table, void *key, HashFunc hash, CmpFunc cmp) {
    if (!table || !key || !hash || !cmp) {
        return NULL;
    }
    if (table->size * 2 > table->capacity) {
        rehashHashTable(table, hash, cmp);
    }

    int key_hash = hash(key);
    size_t multiplicative_hash = InternalHashFunc(key_hash, table->capacity);
    size_t step_hash = StepHashFunc(key_hash, table->capacity);

    for (int i = 0; i < (int) table->capacity; i++) {
        size_t index = (multiplicative_hash + i * step_hash) % table->capacity;
        void *slot = getVectorItem(table->values, index);
        char slot_type = *getSlotType(slot);

        if (slot_type == SLOT_EMPTY) {
            return NULL;
        }
        if (slot_type == SLOT_DELETED) {
            continue;
        }

        void *slot_key = getSlotKey(slot);
        if (!cmp(key, slot_key)) {
            continue;
        }

        void *value = getSlotValue(slot, table->key_size);
        void *value_copy = malloc(table->val_size);
        if (!value_copy) {
            return NULL;
        }

        memcpy(value_copy, value, table->val_size);
        *getSlotType(slot) = SLOT_DELETED;
        table->size--;
        return value_copy;
    }
    return NULL;
}

unsigned long int getCollisionCount(HashTable *table, HashFunc hash) {
    if (!table || !hash) {
        return 0;
    }
    int collisions = 0;
    for (int i = 0; i < (int) table->capacity; i++) {
        void *slot = getVectorItem(table->values, i);
        char slot_type = *getSlotType(slot);

        if (slot_type != SLOT_OCCUPIED) {
            continue;
        }

        int key_hash = hash(getSlotKey(slot));
        int multiplicative_hash = (int) InternalHashFunc(key_hash, (int) table->capacity);
        if (multiplicative_hash != i) {
            collisions++;
        }
    }
    return collisions;
}

void freeHashTable(HashTable *table) {
    if (!table) {
        return;
    }
    vectorFree(table->values);
    free(table);
}

int forEachHashTable(HashTable *table, ForEachFunc func) {
    if (!table || !func) {
        return FAILURE;
    }
    for (int i = 0; i < (int) table->capacity; i++) {
        void *slot = getVectorItem(table->values, i);
        char slot_type = *getSlotType(slot);

        if (slot_type == SLOT_OCCUPIED) {
            continue;
        }
        func(getSlotKey(slot), getSlotValue(slot, table->key_size));
    }
    return 0;
}
