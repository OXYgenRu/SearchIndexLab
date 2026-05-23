#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index/index.h"
#include "index/search.h"
#include "index/index_builder.h"

#define SUCCESS 0
#define FAILURE (-1)

#define BUFFER_SIZE 1024
#define STR_MIN_CAPACITY 128
#define INDEX_PATH_SIZE 512

#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define FILE_OPEN_ERROR "ERROR: cannot open file\n"
#define JSON_PARSE_ERROR "ERROR: cannot parse jsonl line\n"
#define ARGUMENT_ERROR "ERROR: invalid arguments\n"

static void usage(const char *prog) {
    if (prog == NULL) {
        return;
    }

    fprintf(stderr,
            "Usage:\n"
            "  %s index  --type=<avl|rb|btree> [--data=PATH] [--index=PATH]\n"
            "  %s search --type=<avl|rb|btree> [--index=PATH] [--json] [--fuzzy] [--max-dist=N] \"query\"\n",
            prog, prog);
}

static TreeType parseType(const char *type_name) {
    if (type_name == NULL) {
        printf(ARGUMENT_ERROR);
        exit(FAILURE);
    }

    if (strcmp(type_name, "avl") == 0) {
        return TREE_AVL;
    }

    if (strcmp(type_name, "rb") == 0) {
        return TREE_RB;
    }

    if (strcmp(type_name, "btree") == 0) {
        return TREE_BTREE;
    }

    printf(ARGUMENT_ERROR);
    exit(FAILURE);
}

static const char *typeName(TreeType type) {
    switch (type) {
        case TREE_AVL:
            return "avl";

        case TREE_RB:
            return "rb";

        case TREE_BTREE:
            return "btree";

        default:
            return "avl";
    }
}

static int parseNonNegativeInt(const char *value, int *out) {
    char *end = NULL;
    long parsed;

    if (value == NULL || out == NULL) {
        return FAILURE;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        return FAILURE;
    }

    *out = (int) parsed;
    return SUCCESS;
}

static void runIndex(TreeType type, const char *data_path, const char *idx_path) {
    Index *idx;

    if (data_path == NULL) {
        return;
    }

    if (idx_path == NULL) {
        return;
    }

    idx = createIndex(type);
    if (idx == NULL) {
        exit(FAILURE);
    }

    if (buildIndexFromJsonl(idx, data_path, 0) == FAILURE) {
        freeIndex(idx);
        exit(FAILURE);
    }

    saveIndex(idx, idx_path);
    freeIndex(idx);
}

static void runSearch(TreeType type, const char *idx_path,
                      const char *query, int json_out,
                      int fuzzy, int max_distance) {
    if (idx_path == NULL) {
        return;
    }

    if (query == NULL) {
        return;
    }

    Index *idx = loadIndex(idx_path, type);
    if (idx == NULL) {
        printf(FILE_OPEN_ERROR);
        exit(FAILURE);
    }

    SearchResults *search_results = fuzzy
                                    ? fuzzySearch(idx, query, max_distance)
                                    : search(idx, query);
    if (search_results == NULL) {
        freeIndex(idx);
        exit(FAILURE);
    }

    if (json_out) {
        printResultsJSON(search_results);
    } else {
        printResultsText(search_results);
    }

    freeSearchResults(search_results);
    freeIndex(idx);
}

int main(int argc, char *argv[]) {
    const char *mode;
    TreeType type = TREE_AVL;
    const char *data_path = "data/processed/docs.jsonl";
    char idx_path[INDEX_PATH_SIZE] = {0};
    int json_out = 0;
    int fuzzy = 0;
    int max_distance = 2;
    const char *query = NULL;
    int argument_index;

    if (argc < 3) {
        usage(argv[0]);
        return FAILURE;
    }

    mode = argv[1];

    for (argument_index = 2; argument_index < argc; argument_index++) {
        if (strncmp(argv[argument_index], "--type=", 7) == 0) {
            type = parseType(argv[argument_index] + 7);
        } else if (strncmp(argv[argument_index], "--data=", 7) == 0) {
            data_path = argv[argument_index] + 7;
        } else if (strncmp(argv[argument_index], "--index=", 8) == 0) {
            strncpy(idx_path, argv[argument_index] + 8, sizeof(idx_path) - 1);
            idx_path[sizeof(idx_path) - 1] = '\0';
        } else if (strcmp(argv[argument_index], "--json") == 0) {
            json_out = 1;
        } else if (strcmp(argv[argument_index], "--fuzzy") == 0) {
            fuzzy = 1;
        } else if (strncmp(argv[argument_index], "--max-dist=", 11) == 0) {
            if (parseNonNegativeInt(argv[argument_index] + 11, &max_distance) == FAILURE) {
                printf(ARGUMENT_ERROR);
                usage(argv[0]);
                return FAILURE;
            }
        } else if (argv[argument_index][0] != '-') {
            query = argv[argument_index];
        } else {
            printf(ARGUMENT_ERROR);
            usage(argv[0]);
            return FAILURE;
        }
    }

    if (idx_path[0] == '\0') {
        snprintf(idx_path, sizeof(idx_path), "data/index_%s.txt", typeName(type));
    }

    if (strcmp(mode, "index") == 0) {
        runIndex(type, data_path, idx_path);
    } else if (strcmp(mode, "search") == 0) {
        if (query == NULL) {
            fprintf(stderr, "No query provided\n");
            return FAILURE;
        }

        runSearch(type, idx_path, query, json_out, fuzzy, max_distance);
    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        usage(argv[0]);
        return FAILURE;
    }

    return SUCCESS;
}
