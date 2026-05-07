#include "generic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define SUCCESS 0
#define FAILURE (-1)

GenericList *createList(size_t elem_size) {
    GenericList *new_list = malloc(sizeof(GenericList));
    if (!new_list) {
        printf(MEMORY_ALLOCATION_ERROR);
        exit(EXIT_FAILURE);
    }
    new_list->head = NULL;
    new_list->elem_size = elem_size;
    return new_list;
}

void appendItem(GenericList *list, void *data) {
    if (!list) { // O(1)
        printf("GenericList is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (!data) { // O(1)
        printf("Data pointer is NULL!\n");
        exit(EXIT_FAILURE);
    }

    Node *new_node = malloc(sizeof(Node)); // O(1) - константа
    if (!new_node) {  // O(1)
        printf(MEMORY_ALLOCATION_ERROR);
        exit(EXIT_FAILURE);
    }
    new_node->next = NULL;  // O(1)
    new_node->data = malloc(list->elem_size);  // O(1) - значительно меньше, чем размер листа
    if (!new_node->data) { // O(1) - константа
        printf(MEMORY_ALLOCATION_ERROR);
        exit(EXIT_FAILURE);
    }
    memcpy(new_node->data, data, list->elem_size); // O(1) - константа

    if (!list->head) { // O(1)
        list->head = new_node;
        return;
    }

    Node *current_node = list->head; // O(1)
    while (current_node->next != NULL) { // O(n)
        current_node = current_node->next; // O(n) * O(1)
    }
    current_node->next = new_node; // O(1)
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(n)
    Оценка снизу - Ω(1)
    */
}

int findItem(GenericList *list, void *value, EqualsFunc cmp) {
    if (!list) { // O(1)
        printf("GenericList is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (!value) { // O(1)
        printf("Value is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (!cmp) { // O(1)
        printf("Comparator function is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (!list->head) { // O(1)
        return FAILURE;
    }
    Node *current_node = list->head; // O(1)
    int index = 0; // O(1)
    while (current_node) { // O(n)
        if (cmp(current_node->data, value)) {  // O(n) * O(1)
            return index;
        }
        current_node = current_node->next; // O(n) * O(1)
        index++; // O(n) * O(1)
    }
    return FAILURE;
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(n)
    Оценка снизу - Ω(1)
    */
}

void *popItem(GenericList *list, size_t index) {
    // k = index
    if (!list) { // O(1)
        printf("GenericList is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (!list->head) { // O(1)
        return NULL;
    }
    Node *previous_node = NULL; // O(1)
    Node *current_node = list->head; // O(1)
    size_t i = 0;
    while (current_node && i < index) { // O(n)
        previous_node = current_node; // O(n) * O(1)
        current_node = current_node->next; // O(n) * O(1)
        i++; // O(n)
    }

    if (!current_node) { // O(1)
        return NULL;
    }
    void *copy = malloc(list->elem_size); // O(1)
    if (!copy) { // O(1)
        printf(MEMORY_ALLOCATION_ERROR);
        exit(EXIT_FAILURE);
    }
    memcpy(copy, current_node->data, list->elem_size); // O(1)

    if (previous_node) { // O(1)
        previous_node->next = current_node->next;
    } else {
        list->head = current_node->next;
    }

    free(current_node->data); // O(1)
    free(current_node); // O(1)
    return copy;
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(k)
    Оценка снизу - Ω(1)
    */
}

void freeList(GenericList *list) {
    if (!list) {
        return;
    }
    Node *current_node = list->head;
    while (current_node) {
        Node *next_node = current_node->next;
        free(current_node->data);
        free(current_node);
        current_node = next_node;
    }
    free(list);
}

unsigned int listLength(GenericList *list) {
    if (!list) { // O(1)
        return 0;
    }
    Node *current_node = list->head; // O(1)
    unsigned int length = 0; // O(1)
    while (current_node) { // O(n)
        current_node = current_node->next; // O(n) * O(1)
        length++; // O(n) * O(1)
    }
    return length;
    /*
    Оценка сверху - O(n)
    Точная оценка - Θ(n) - нельзя точно оценить
    Оценка снизу - Ω(1)
    */
}
