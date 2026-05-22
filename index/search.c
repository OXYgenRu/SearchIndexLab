#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "search.h"
#include "../lab4/hash_table/generic.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"

#define MIN_TOKEN_LENGTH 3

typedef struct {
    char **items;
    int    count;
    int    capacity;
} TokenList;

static int isTokenChar(int c) {
    if (c == '_') {
        return 1;
    }
    return isalnum(c);
}

static int tokenListInit(TokenList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return SUCCESS;
}

static int tokenListAppend(TokenList *list, const char *start, size_t length) {
    if (length < (size_t) MIN_TOKEN_LENGTH) {
        return SUCCESS;
    }

    if (list->count >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        char **new_items = realloc(list->items, new_capacity * sizeof(char *));
        if (new_items == NULL) {
            printf(MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    char *token = malloc(length + 1);
    if (token == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }

    for (size_t i = 0; i < length; i++) {
        token[i] = (char) tolower((unsigned char) start[i]);
    }
    token[length] = '\0';

    list->items[list->count] = token;
    list->count++;
    return SUCCESS;
}

static void tokenListFree(TokenList *list) {
    if (list->items == NULL) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int tokenizeQuery(const char *query, TokenList *list) {
    if (query == NULL || list == NULL) {
        return FAILURE;
    }
    tokenListInit(list);

    const char *start = NULL;
    for (const char *p = query;; p++) {
        int c = (unsigned char) *p;
        int is_word = (c != '\0') && isTokenChar(c);

        if (is_word) {
            if (start == NULL) {
                start = p;
            }
        } else {
            if (start != NULL) {
                if (tokenListAppend(list, start, (size_t) (p - start)) == FAILURE) {
                    tokenListFree(list);
                    return FAILURE;
                }
                start = NULL;
            }
            if (c == '\0') {
                break;
            }
        }
    }
    return SUCCESS;
}

static HashTable *buildCountTable(Vector *list) {
    HashTable *table = createHashTable(sizeof(int), sizeof(int));
    if (table == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    for (size_t i = 0; i < list->size; i++) {
        PostingEntry *entry = getVectorItem(list, i);
        if (entry == NULL) {
            continue;
        }
        int doc_id = entry->doc_id;
        int *existing = getItemHashTable(table, &doc_id, HashInt, intEquals);
        int new_count = (existing != NULL ? *existing : 0) + 1;
        setItemHashTable(table, &doc_id, &new_count, HashInt, intEquals);
    }
    return table;
}

static void copyTitle(char *dst, const char *src) {
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, SEARCH_TITLE_LENGTH - 1);
    dst[SEARCH_TITLE_LENGTH - 1] = '\0';
}

Vector *intersectPostings(Vector **lists, int n) {
    Vector *results = createVector(sizeof(SearchResult));
    if (results == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    if (lists == NULL || n <= 0) {
        return results;
    }
    for (int i = 0; i < n; i++) {
        if (lists[i] == NULL || lists[i]->size == 0) {
            return results;
        }
    }

    HashTable **tables = malloc(n * sizeof(HashTable *));
    if (tables == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return results;
    }
    for (int i = 0; i < n; i++) {
        tables[i] = buildCountTable(lists[i]);
        if (tables[i] == NULL) {
            for (int j = 0; j < i; j++) {
                freeHashTable(tables[j]);
            }
            free(tables);
            return results;
        }
    }

    int shortest = 0;
    for (int i = 1; i < n; i++) {
        if (lists[i]->size < lists[shortest]->size) {
            shortest = i;
        }
    }

    HashTable *seen = createHashTable(sizeof(int), sizeof(int));
    if (seen == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        for (int i = 0; i < n; i++) {
            freeHashTable(tables[i]);
        }
        free(tables);
        return results;
    }

    Vector *shortest_list = lists[shortest];
    for (size_t j = 0; j < shortest_list->size; j++) {
        PostingEntry *entry = getVectorItem(shortest_list, j);
        if (entry == NULL) {
            continue;
        }
        int doc_id = entry->doc_id;

        if (getItemHashTable(seen, &doc_id, HashInt, intEquals) != NULL) {
            continue;
        }

        int in_all = 1;
        int score = 0;
        for (int i = 0; i < n; i++) {
            int *count = getItemHashTable(tables[i], &doc_id, HashInt, intEquals);
            if (count == NULL) {
                in_all = 0;
                break;
            }
            score += *count;
        }
        if (!in_all) {
            continue;
        }

        SearchResult result;
        result.doc_id = doc_id;
        copyTitle(result.title, entry->title);
        result.score = score;
        appendVectorItem(results, &result);

        int flag = 1;
        setItemHashTable(seen, &doc_id, &flag, HashInt, intEquals);
    }

    freeHashTable(seen);
    for (int i = 0; i < n; i++) {
        freeHashTable(tables[i]);
    }
    free(tables);

    return results;
}

static int compareSearchResults(const void *a, const void *b) {
    const SearchResult *ra = (const SearchResult *) a;
    const SearchResult *rb = (const SearchResult *) b;

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

static double currentTimeMs(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return (double) clock() * 1000.0 / (double) CLOCKS_PER_SEC;
    }
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
}

static SearchResults *createEmptyResults(double time_ms) {
    SearchResults *sr = malloc(sizeof(SearchResults));
    if (sr == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    sr->results = createVector(sizeof(SearchResult));
    sr->total = 0;
    sr->time_ms = time_ms;
    if (sr->results == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        free(sr);
        return NULL;
    }
    return sr;
}

SearchResults *search(Index *idx, const char *query) {
    double t_start = currentTimeMs();

    if (idx == NULL || query == NULL) {
        return createEmptyResults(currentTimeMs() - t_start);
    }

    TokenList tokens;
    if (tokenizeQuery(query, &tokens) == FAILURE) {
        return createEmptyResults(currentTimeMs() - t_start);
    }
    if (tokens.count == 0) {
        tokenListFree(&tokens);
        return createEmptyResults(currentTimeMs() - t_start);
    }

    Vector **lists = malloc(tokens.count * sizeof(Vector *));
    if (lists == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        tokenListFree(&tokens);
        return createEmptyResults(currentTimeMs() - t_start);
    }

    int all_found = 1;
    for (int i = 0; i < tokens.count; i++) {
        lists[i] = lookupTerm(idx, tokens.items[i]);
        if (lists[i] == NULL) {
            all_found = 0;
            break;
        }
    }

    if (!all_found) {
        free(lists);
        tokenListFree(&tokens);
        return createEmptyResults(currentTimeMs() - t_start);
    }

    Vector *all = intersectPostings(lists, tokens.count);
    free(lists);
    tokenListFree(&tokens);

    if (all == NULL) {
        return createEmptyResults(currentTimeMs() - t_start);
    }

    if (all->size > 1) {
        qsort(all->data, all->size, sizeof(SearchResult), compareSearchResults);
    }

    SearchResults *sr = malloc(sizeof(SearchResults));
    if (sr == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        vectorFree(all);
        return NULL;
    }
    sr->total = (int) all->size;
    sr->results = createVector(sizeof(SearchResult));
    if (sr->results == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        vectorFree(all);
        free(sr);
        return NULL;
    }

    size_t limit = all->size < (size_t) SEARCH_TOP_LIMIT ? all->size : (size_t) SEARCH_TOP_LIMIT;
    for (size_t i = 0; i < limit; i++) {
        SearchResult *item = getVectorItem(all, i);
        if (item == NULL) {
            continue;
        }
        appendVectorItem(sr->results, item);
    }
    vectorFree(all);

    sr->time_ms = currentTimeMs() - t_start;
    return sr;
}

void printResultsText(const SearchResults *sr) {
    if (sr == NULL) {
        return;
    }
    printf("Время: %.3f мс | Найдено: %d документов\n\n", sr->time_ms, sr->total);
    if (sr->results == NULL) {
        return;
    }
    for (size_t i = 0; i < sr->results->size; i++) {
        SearchResult *item = getVectorItem(sr->results, i);
        if (item == NULL) {
            continue;
        }
        printf("%2zu. [id=%d] %s\n", i + 1, item->doc_id, item->title);
    }
}

static void writeJsonEscaped(FILE *out, const char *s) {
    fputc('"', out);
    if (s == NULL) {
        fputc('"', out);
        return;
    }
    for (const unsigned char *p = (const unsigned char *) s; *p != '\0'; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n",  out); break;
            case '\r': fputs("\\r",  out); break;
            case '\t': fputs("\\t",  out); break;
            case '\b': fputs("\\b",  out); break;
            case '\f': fputs("\\f",  out); break;
            default:
                if (c < 0x20) {
                    fprintf(out, "\\u%04x", c);
                } else {
                    fputc(c, out);
                }
                break;
        }
    }
    fputc('"', out);
}

void printResultsJSON(const SearchResults *sr) {
    if (sr == NULL) {
        printf("{\"total\": 0, \"time_ms\": 0.0, \"results\": []}\n");
        return;
    }
    printf("{\n");
    printf("  \"total\": %d,\n", sr->total);
    printf("  \"time_ms\": %.3f,\n", sr->time_ms);
    printf("  \"results\": [");
    if (sr->results != NULL && sr->results->size > 0) {
        printf("\n");
        for (size_t i = 0; i < sr->results->size; i++) {
            SearchResult *item = getVectorItem(sr->results, i);
            if (item == NULL) {
                continue;
            }
            printf("    {\"doc_id\": %d, \"title\": ", item->doc_id);
            writeJsonEscaped(stdout, item->title);
            printf(", \"score\": %d}", item->score);
            if (i + 1 < sr->results->size) {
                printf(",");
            }
            printf("\n");
        }
        printf("  ");
    }
    printf("]\n}\n");
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
