#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "generic.h"

#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define SUCCESS 0
#define FAILURE (-1)

const int grow_factor = 2;
const int shrink_factor = grow_factor * grow_factor;

// Вспомогательная функция для изменения размера
static bool needToResize(Vector *vector, bool *increase) {
    if (!vector) {
        printf("Vector is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (!increase) {
        printf("Increase flag is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (vector->size == vector->capacity) {
        *increase = true;
        return true;
    }
    if (vector->capacity > MIN_SIZE && vector->size <= vector->capacity / shrink_factor) {
        *increase = false;
        return true;
    }
    *increase = false;
    return false;
}

// Определяем увеличивать размер или уменьшать
static int resize(Vector *vector, bool increase) {
    if (!vector) {
        return FAILURE;
    }
    size_t new_capacity = 0;
    if (increase) {
        if (vector->capacity == 0) {
            new_capacity = MIN_SIZE;
        } else {
            new_capacity = vector->capacity * grow_factor;
        }
    } else {
        if (vector->capacity <= MIN_SIZE) {
            return SUCCESS;
        }
        new_capacity = vector->capacity / grow_factor;
        if (new_capacity < MIN_SIZE) {
            new_capacity = MIN_SIZE;
        }
        if (new_capacity < vector->size) {
            new_capacity = vector->size;
        }
    }
    if (vector->capacity == new_capacity) {
        return SUCCESS;
    }
    size_t new_data_size = new_capacity * vector->elem_size;
    void *new_data = realloc(vector->data, new_data_size);
    if (!new_data) {
        return FAILURE;
    }
    vector->data = new_data;
    vector->capacity = new_capacity;
    return SUCCESS;
}

Vector *createVector(size_t elem_size) {
    Vector *new_vector = malloc(sizeof(Vector));
    if (!new_vector) {
        return NULL;
    }
    new_vector->size = 0;
    new_vector->capacity = MIN_SIZE;
    new_vector->elem_size = elem_size;
    new_vector->data = malloc(new_vector->capacity * new_vector->elem_size);
    if (!new_vector->data) {
        free(new_vector);
        return NULL;
    }
    return new_vector;
}

int appendVectorItem(Vector *vector, void *el) {
    // n = vector->size
    if (!vector) { // O(1)
        return FAILURE;
    }
    if (!el) { // O(1)
        return FAILURE;
    }
    bool increase = false; // O(1)
    if (needToResize(vector, &increase) && resize(vector, increase)) { // O(n)
        return FAILURE;
    }
    memcpy((char *) vector->data + vector->size * vector->elem_size, el,
           vector->elem_size); // O(1) так как размер элемента значительно меньше всего вектора и можно взять за константу
    vector->size++; // O(1)
    return SUCCESS; // O(1)
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(1)
    Оценка снизу - Ω(1)
    */
}

void *getVectorItem(Vector *vector, size_t index) {
    if (!vector) { // O(1)
        return NULL;
    }
    if (index >= vector->size) {// O(1)
        return NULL;
    }
    return (char *) vector->data + index * vector->elem_size; // O(1)
    /*
    Оценка сверху - O(1)
    Точная оценка - Θ(1)
    Оценка снизу - Ω(1)
    */
}

int setVectorItem(Vector *vector, size_t index, void *value) {
    if (!vector) { // O(1)
        return FAILURE;
    }
    if (!value) { // O(1)
        return FAILURE;
    }
    if (index >= vector->size) { // O(1)
        return FAILURE;
    }
    memcpy((char *) vector->data + index * vector->elem_size, value,
           vector->elem_size); // O(1) так как размер элемента значительно меньше всего вектора и можно взять за константу
    /*
    Оценка сверху - O(1)
    Точная оценка - Θ(1)
    Оценка снизу - Ω(1)
    */
    return SUCCESS;
}

void *popVectorItem(Vector *vector, size_t index) {
    if (!vector) { // O(1)
        return NULL;
    }
    if (index >= vector->size) { // O(1)
        return NULL;
    }
    void *copy = malloc(vector->elem_size); // O(1)
    if (!copy) {
        return NULL;
    }
    char *target = (char *) vector->data + index * vector->elem_size; // O(1)
    memcpy(copy, target, vector->elem_size); // O(1)
    if (index < vector->size - 1) {
        memmove(target, target + vector->elem_size, (vector->size - index - 1) * vector->elem_size); // O(n)
    }
    vector->size--; // O(1)
    bool increase = false;  // O(1)
    if (needToResize(vector, &increase) && resize(vector, increase)) { // O(n)
        return NULL;
    }
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(n) - нельзя точно определить, но в среднем удаление из вектора долгая операция, возьмем за n
    Оценка снизу - Ω(1)
    */
    return copy;

}

long int findVectorItem(Vector *vector, void *value, EqualsFunc cmp) {
    if (!vector) {  // O(1)
        return FAILURE;
    }
    if (!value) {  // O(1)
        return FAILURE;
    }
    for (size_t i = 0; i < vector->size; i++) {  // O(n)
        void *current_value = (char *) vector->data + i * vector->elem_size; // O(n) *  O(1)

        int is_equal = 0; // O(n) * O(1)
        if (cmp) { // O(n) * O(1)
            is_equal = cmp(value, current_value); // O(n) *  O(1)
        } else {
            is_equal = (memcmp(value, current_value, vector->elem_size) == 0); // O(n) *  O(1)
        }
        if (is_equal) { // O(n) *  O(1)
            return (long int) i;
        }
    }
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(n) - нельзя точно определить, но в среднем удаление из вектора долгая операция, возьмем за n
    Оценка снизу - Ω(1)
    */
    return FAILURE;
}

int vectorFree(Vector *vector) {
    if (!vector) {
        return FAILURE;
    }
    free(vector->data);
    free(vector);
    return SUCCESS;
}