#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "search.h"
#include "../lab4/hash_table/generic.h"
#include "../lab3/comparators.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define VECTOR_ITEM_NULL_ERROR  "ERROR: vector returned NULL element\n"

#define TOP_RESULTS_LIMIT      10
#define MIN_TOKEN_LENGTH       3
#define HT_FLAG_SIZE           1
#define INITIAL_TOKEN_CAPACITY 8
#define NS_PER_MS              1000000.0
#define MS_PER_SEC             1000.0

/* Раскладка слота в values-векторе HashTable — повторяет lab4/hash_table/generic.c:
   [флаг(1 байт) | ключ(key_size) | значение(val_size)]. Хелперы там объявлены
   как static, поэтому здесь продублированы для прямого обхода слотов во время
   пересечения posting-листов. */
static char *slotFlagPtr(void *slot) {
    return (char *) slot;
}

static void *slotKeyPtr(void *slot) {
    return (char *) slot + HT_FLAG_SIZE;
}

static void *slotValuePtr(void *slot, size_t key_size) {
    return (char *) slot + HT_FLAG_SIZE + key_size;
}

static int isWordChar(int c) {
    return isalnum(c) || c == '_';
}

/* Токенизатор, повторяющий preprocess.py: tolower, любой не-word байт
   заменяем на пробел, режем по пробелам, отбрасываем токены короче
   MIN_TOKEN_LENGTH. Возвращает malloc-нутый массив malloc-нутых C-строк;
   освобождать через freeTokens. */
static char **tokenizeQuery(const char *query, int *out_count) {
    if (out_count == NULL) {
        return NULL;
    }
    *out_count = 0;

    if (query == NULL) {
        return NULL;
    }

    size_t length = strlen(query);
    char *buffer = malloc(length + 1);
    if (buffer == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char) query[i];
        if (isWordChar(c)) {
            buffer[i] = (char) tolower(c);
        } else {
            buffer[i] = ' ';
        }
    }
    buffer[length] = '\0';

    int capacity = INITIAL_TOKEN_CAPACITY;
    char **tokens = malloc(sizeof(char *) * (size_t) capacity);
    if (tokens == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(buffer);
        return NULL;
    }

    int count = 0;
    char *save_ptr = NULL;
    char *token = strtok_r(buffer, " ", &save_ptr);
    while (token != NULL) {
        size_t token_length = strlen(token);
        if (token_length >= MIN_TOKEN_LENGTH) {
            if (count == capacity) {
                int new_capacity = capacity * 2;
                char **new_tokens = realloc(tokens, sizeof(char *) * (size_t) new_capacity);
                if (new_tokens == NULL) {
                    printf("%s", MEMORY_ALLOCATION_ERROR);
                    for (int j = 0; j < count; j++) {
                        free(tokens[j]);
                    }
                    free(tokens);
                    free(buffer);
                    return NULL;
                }
                tokens = new_tokens;
                capacity = new_capacity;
            }

            char *copy = malloc(token_length + 1);
            if (copy == NULL) {
                printf("%s", MEMORY_ALLOCATION_ERROR);
                for (int j = 0; j < count; j++) {
                    free(tokens[j]);
                }
                free(tokens);
                free(buffer);
                return NULL;
            }
            memcpy(copy, token, token_length + 1);
            tokens[count] = copy;
            count++;
        }
        token = strtok_r(NULL, " ", &save_ptr);
    }

    free(buffer);
    *out_count = count;
    return tokens;
}

static void freeTokens(char **tokens, int count) {
    if (tokens == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

/* Строит HashTable<int doc_id, int count> для одного posting-листа. Схлопывает
   повторяющиеся doc_id в один ключ, накапливая в значении число вхождений
   (= TF этого терма в документе). */
static HashTable *buildOccurrenceTable(Vector *list) {
    if (list == NULL) {
        return NULL;
    }
    HashTable *table = createHashTable(sizeof(int), sizeof(int));
    if (table == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    for (size_t i = 0; i < list->size; i++) {
        PostingEntry *entry = getVectorItem(list, i);
        if (entry == NULL) {
            printf("%s", VECTOR_ITEM_NULL_ERROR);
            freeHashTable(table);
            return NULL;
        }

        int doc_id = entry->doc_id;
        void *existing = getItemHashTable(table, &doc_id, HashInt, intEquals);
        if (existing != NULL) {
            int v;
            memcpy(&v, existing, sizeof(int));
            v++;
            memcpy(existing, &v, sizeof(int));
        } else {
            int one = 1;
            setItemHashTable(table, &doc_id, &one, HashInt, intEquals);
        }
    }
    return table;
}

/* Возвращаемый Vector содержит элементы SearchResult (doc_id, title, score=TF). */
Vector *intersectPostings(Vector **lists, int n) {
    if (lists == NULL) {
        return NULL;
    }
    if (n <= 0) {
        return NULL;
    }

    Vector *result = createVector(sizeof(SearchResult));
    if (result == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        if (lists[i] == NULL) {
            return result;
        }
    }

    HashTable *main_table = createHashTable(sizeof(int), sizeof(SearchResult));
    if (main_table == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        vectorFree(result);
        return NULL;
    }

    for (size_t i = 0; i < lists[0]->size; i++) {
        PostingEntry *entry = getVectorItem(lists[0], i);
        if (entry == NULL) {
            printf("%s", VECTOR_ITEM_NULL_ERROR);
            freeHashTable(main_table);
            vectorFree(result);
            return NULL;
        }

        int doc_id = entry->doc_id;
        void *existing = getItemHashTable(main_table, &doc_id, HashInt, intEquals);
        if (existing != NULL) {
            SearchResult local;
            memcpy(&local, existing, sizeof(SearchResult));
            local.score++;
            memcpy(existing, &local, sizeof(SearchResult));
        } else {
            SearchResult fresh;
            fresh.doc_id = entry->doc_id;
            strncpy(fresh.title, entry->title, MAX_TITLE_LEN - 1);
            fresh.title[MAX_TITLE_LEN - 1] = '\0';
            fresh.score = 1;
            setItemHashTable(main_table, &doc_id, &fresh, HashInt, intEquals);
        }
    }

    for (int round = 1; round < n; round++) {
        HashTable *temp = buildOccurrenceTable(lists[round]);
        if (temp == NULL) {
            freeHashTable(main_table);
            vectorFree(result);
            return NULL;
        }

        for (size_t slot_index = 0; slot_index < main_table->values->size; slot_index++) {
            void *slot = getVectorItem(main_table->values, slot_index);
            if (slot == NULL) {
                continue;
            }
            if (*slotFlagPtr(slot) != SLOT_OCCUPIED) {
                continue;
            }

            int doc_id;
            memcpy(&doc_id, slotKeyPtr(slot), sizeof(int));
            void *count_ptr = getItemHashTable(temp, &doc_id, HashInt, intEquals);
            if (count_ptr == NULL) {
                *slotFlagPtr(slot) = SLOT_DELETED;
                main_table->size--;
            } else {
                int count_value;
                memcpy(&count_value, count_ptr, sizeof(int));
                void *value_ptr = slotValuePtr(slot, main_table->key_size);
                SearchResult sr_local;
                memcpy(&sr_local, value_ptr, sizeof(SearchResult));
                sr_local.score += count_value;
                memcpy(value_ptr, &sr_local, sizeof(SearchResult));
            }
        }
        freeHashTable(temp);
    }

    for (size_t slot_index = 0; slot_index < main_table->values->size; slot_index++) {
        void *slot = getVectorItem(main_table->values, slot_index);
        if (slot == NULL) {
            continue;
        }
        if (*slotFlagPtr(slot) != SLOT_OCCUPIED) {
            continue;
        }
        SearchResult sr_local;
        memcpy(&sr_local, slotValuePtr(slot, main_table->key_size), sizeof(SearchResult));
        if (appendVectorItem(result, &sr_local) != SUCCESS) {
            printf("%s", MEMORY_ALLOCATION_ERROR);
            freeHashTable(main_table);
            vectorFree(result);
            return NULL;
        }
    }

    freeHashTable(main_table);
    return result;
}

static int cmpResultsDesc(const void *a, const void *b) {
    const SearchResult *ra = a;
    const SearchResult *rb = b;
    if (ra->score != rb->score) {
        return rb->score - ra->score;
    }
    if (ra->doc_id < rb->doc_id) {
        return -1;
    }
    if (ra->doc_id > rb->doc_id) {
        return 1;
    }
    return 0;
}

static double elapsedMs(struct timespec t0, struct timespec t1) {
    double sec_diff = (double) (t1.tv_sec - t0.tv_sec);
    double nsec_diff = (double) (t1.tv_nsec - t0.tv_nsec);
    return sec_diff * MS_PER_SEC + nsec_diff / NS_PER_MS;
}

SearchResults *search(Index *idx, const char *query) {
    SearchResults *sr = malloc(sizeof(SearchResults));
    if (sr == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    sr->results = createVector(sizeof(SearchResult));
    sr->total = 0;
    sr->time_ms = 0.0;

    if (sr->results == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(sr);
        return NULL;
    }

    struct timespec t0;
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    if (idx == NULL) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sr->time_ms = elapsedMs(t0, t1);
        return sr;
    }
    if (query == NULL) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sr->time_ms = elapsedMs(t0, t1);
        return sr;
    }

    int token_count = 0;
    char **tokens = tokenizeQuery(query, &token_count);
    if (tokens == NULL || token_count == 0) {
        freeTokens(tokens, token_count);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sr->time_ms = elapsedMs(t0, t1);
        return sr;
    }

    Vector **lists = malloc(sizeof(Vector *) * (size_t) token_count);
    if (lists == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        freeTokens(tokens, token_count);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sr->time_ms = elapsedMs(t0, t1);
        return sr;
    }

    int short_circuit_empty = 0;
    for (int i = 0; i < token_count; i++) {
        lists[i] = lookupTerm(idx, tokens[i]);
        if (lists[i] == NULL) {
            short_circuit_empty = 1;
            break;
        }
    }

    if (short_circuit_empty == 0) {
        Vector *all = intersectPostings(lists, token_count);
        if (all != NULL) {
            sr->total = (int) all->size;

            if (all->size > 0) {
                qsort(all->data, all->size, all->elem_size, cmpResultsDesc);

                size_t limit = all->size;
                if (limit > TOP_RESULTS_LIMIT) {
                    limit = TOP_RESULTS_LIMIT;
                }
                for (size_t i = 0; i < limit; i++) {
                    SearchResult *r = getVectorItem(all, i);
                    if (r == NULL) {
                        printf("%s", VECTOR_ITEM_NULL_ERROR);
                        break;
                    }
                    if (appendVectorItem(sr->results, r) != SUCCESS) {
                        printf("%s", MEMORY_ALLOCATION_ERROR);
                        break;
                    }
                }
            }
            vectorFree(all);
        }
    }

    free(lists);
    freeTokens(tokens, token_count);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    sr->time_ms = elapsedMs(t0, t1);

    return sr;
}

void printResultsText(const SearchResults *sr) {
    if (sr == NULL) {
        return;
    }
    printf("Время: %.1f мс | Найдено: %d документов\n\n", sr->time_ms, sr->total);

    if (sr->results == NULL) {
        return;
    }
    for (size_t i = 0; i < sr->results->size; i++) {
        SearchResult *r = getVectorItem(sr->results, i);
        if (r == NULL) {
            printf("%s", VECTOR_ITEM_NULL_ERROR);
            continue;
        }
        printf("%2zu. [id=%d] %s\n", i + 1, r->doc_id, r->title);
    }
}

static void printJsonEscapedString(const char *s) {
    if (s == NULL) {
        printf("\"\"");
        return;
    }
    putchar('"');
    for (size_t i = 0; s[i] != '\0'; i++) {
        unsigned char c = (unsigned char) s[i];
        switch (c) {
            case '"':
                printf("\\\"");
                break;
            case '\\':
                printf("\\\\");
                break;
            case '\b':
                printf("\\b");
                break;
            case '\f':
                printf("\\f");
                break;
            case '\n':
                printf("\\n");
                break;
            case '\r':
                printf("\\r");
                break;
            case '\t':
                printf("\\t");
                break;
            default:
                if (c < 0x20) {
                    printf("\\u%04x", c);
                } else {
                    putchar(c);
                }
                break;
        }
    }
    putchar('"');
}

void printResultsJSON(const SearchResults *sr) {
    if (sr == NULL) {
        printf("{\"total\":0,\"time_ms\":0,\"results\":[]}\n");
        return;
    }
    printf("{\"total\":%d,\"time_ms\":%.3f,\"results\":[", sr->total, sr->time_ms);

    if (sr->results != NULL) {
        for (size_t i = 0; i < sr->results->size; i++) {
            SearchResult *r = getVectorItem(sr->results, i);
            if (r == NULL) {
                continue;
            }
            if (i > 0) {
                putchar(',');
            }
            printf("{\"doc_id\":%d,\"title\":", r->doc_id);
            printJsonEscapedString(r->title);
            printf(",\"score\":%d}", r->score);
        }
    }
    printf("]}\n");
}

void freeSearchResults(SearchResults *sr) {
    if (sr == NULL) {
        return;
    }
    if (sr->results != NULL) {
        vectorFree(sr->results);
    }
    free(sr);
}
