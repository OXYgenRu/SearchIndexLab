#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index/index.h"
#include "index/search.h"

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
            "  %s search --type=<avl|rb|btree> [--index=PATH] [--json] \"query\"\n",
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

static char *getLine(FILE *file) {
    char buffer[BUFFER_SIZE];
    char *line = NULL;
    size_t length = 0;
    size_t capacity = 0;

    if (file == NULL) {
        return NULL;
    }

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        size_t buffer_length = strlen(buffer);
        int has_newline = 0;

        if (buffer_length > 0 && buffer[buffer_length - 1] == '\n') {
            has_newline = 1;
            buffer_length--;

            if (buffer_length > 0 && buffer[buffer_length - 1] == '\r') {
                buffer_length--;
            }
        }

        if (length + buffer_length + 1 > capacity) {
            size_t new_capacity = capacity;
            char *new_line;

            if (new_capacity == 0) {
                new_capacity = STR_MIN_CAPACITY;
            }

            while (new_capacity < length + buffer_length + 1) {
                new_capacity *= 2;
            }

            new_line = realloc(line, new_capacity);
            if (new_line == NULL) {
                printf(MEMORY_ALLOCATION_ERROR);
                free(line);
                return NULL;
            }

            line = new_line;
            capacity = new_capacity;
        }

        memcpy(line + length, buffer, buffer_length);
        length += buffer_length;
        line[length] = '\0';

        if (has_newline) {
            break;
        }
    }

    return line;
}

static void skipSpaces(const char **cursor) {
    if (cursor == NULL) {
        return;
    }

    if (*cursor == NULL) {
        return;
    }

    while (**cursor != '\0' && isspace((unsigned char) **cursor)) {
        (*cursor)++;
    }
}

static int appendChar(char **text, size_t *length, size_t *capacity, char value) {
    size_t new_capacity;

    if (text == NULL) {
        return FAILURE;
    }

    if (length == NULL) {
        return FAILURE;
    }

    if (capacity == NULL) {
        return FAILURE;
    }

    if (*length + 2 > *capacity) {
        new_capacity = *capacity;

        if (new_capacity == 0) {
            new_capacity = STR_MIN_CAPACITY;
        }

        while (new_capacity < *length + 2) {
            new_capacity *= 2;
        }

        char *new_text = realloc(*text, new_capacity);
        if (new_text == NULL) {
            printf(MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }

        *text = new_text;
        *capacity = new_capacity;
    }

    (*text)[*length] = value;
    (*length)++;
    (*text)[*length] = '\0';

    return SUCCESS;
}

static int parseJsonString(const char **cursor, char **result) {
    char *text = NULL;
    size_t length = 0;
    size_t capacity = 0;

    if (cursor == NULL) {
        return FAILURE;
    }

    if (*cursor == NULL) {
        return FAILURE;
    }

    if (result == NULL) {
        return FAILURE;
    }

    skipSpaces(cursor);

    if (**cursor != '"') {
        return FAILURE;
    }

    (*cursor)++;

    while (**cursor != '\0' && **cursor != '"') {
        char value = **cursor;

        if (value == '\\') {
            (*cursor)++;

            if (**cursor == '\0') {
                free(text);
                return FAILURE;
            }

            if (**cursor == '"' || **cursor == '\\' || **cursor == '/') {
                value = **cursor;
            } else if (**cursor == 'n' || **cursor == 'r' || **cursor == 't') {
                value = ' ';
            } else if (**cursor == 'u') {
                int hex_index;

                for (hex_index = 0; hex_index < 4; hex_index++) {
                    (*cursor)++;

                    if (**cursor == '\0') {
                        free(text);
                        return FAILURE;
                    }
                }

                value = '?';
            } else {
                value = **cursor;
            }
        }

        if (appendChar(&text, &length, &capacity, value) == FAILURE) {
            free(text);
            return FAILURE;
        }

        (*cursor)++;
    }

    if (**cursor != '"') {
        free(text);
        return FAILURE;
    }

    (*cursor)++;

    if (text == NULL) {
        text = malloc(1);
        if (text == NULL) {
            printf(MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }

        text[0] = '\0';
    }

    *result = text;

    return SUCCESS;
}

static const char *findFieldValue(const char *line, const char *field_name) {
    char pattern[128];

    if (line == NULL) {
        return NULL;
    }

    if (field_name == NULL) {
        return NULL;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", field_name);

    const char *field_position = strstr(line, pattern);
    if (field_position == NULL) {
        return NULL;
    }

    const char *colon_position = strchr(field_position, ':');
    if (colon_position == NULL) {
        return NULL;
    }

    return colon_position + 1;
}

static int parseStringField(const char *line, const char *field_name, char **value) {
    const char *cursor;

    if (line == NULL) {
        return FAILURE;
    }

    if (field_name == NULL) {
        return FAILURE;
    }

    if (value == NULL) {
        return FAILURE;
    }

    cursor = findFieldValue(line, field_name);
    if (cursor == NULL) {
        return FAILURE;
    }

    return parseJsonString(&cursor, value);
}

static int parseDocIdField(const char *line, int *doc_id) {
    char *doc_id_text;
    char *end_ptr;

    if (line == NULL) {
        return FAILURE;
    }

    if (doc_id == NULL) {
        return FAILURE;
    }

    doc_id_text = NULL;

    if (parseStringField(line, "doc_id", &doc_id_text) == FAILURE) {
        return FAILURE;
    }

    errno = 0;
    long parsed_value = strtol(doc_id_text, &end_ptr, 10);

    if (errno != 0) {
        free(doc_id_text);
        return FAILURE;
    }

    if (end_ptr == doc_id_text) {
        free(doc_id_text);
        return FAILURE;
    }

    if (*end_ptr != '\0') {
        free(doc_id_text);
        return FAILURE;
    }

    if (parsed_value < INT_MIN || parsed_value > INT_MAX) {
        free(doc_id_text);
        return FAILURE;
    }

    *doc_id = (int) parsed_value;

    free(doc_id_text);

    return SUCCESS;
}

static void freeTokens(char **tokens, int tokens_count) {
    if (tokens == NULL) {
        return;
    }

    for (int token_index = 0; token_index < tokens_count; token_index++) {
        free(tokens[token_index]);
    }

    free(tokens);
}

static int appendToken(char ***tokens, int *tokens_count, int *tokens_capacity, char *token) {
    if (tokens == NULL) {
        return FAILURE;
    }

    if (tokens_count == NULL) {
        return FAILURE;
    }

    if (tokens_capacity == NULL) {
        return FAILURE;
    }

    if (token == NULL) {
        return FAILURE;
    }

    if (*tokens_count + 1 > *tokens_capacity) {
        int new_capacity;
        char **new_tokens;
        if (*tokens_capacity == 0) {
            new_capacity = 8;
        } else {
            new_capacity = *tokens_capacity * 2;
        }

        new_tokens = realloc(*tokens, new_capacity * sizeof(char *));
        if (new_tokens == NULL) {
            printf(MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }

        *tokens = new_tokens;
        *tokens_capacity = new_capacity;
    }

    (*tokens)[*tokens_count] = token;
    (*tokens_count)++;

    return SUCCESS;
}

static int parseTokensField(const char *line, char ***tokens, int *tokens_count) {
    const char *cursor;
    char **items = NULL;
    int count = 0;
    int capacity = 0;

    if (line == NULL) {
        return FAILURE;
    }

    if (tokens == NULL) {
        return FAILURE;
    }

    if (tokens_count == NULL) {
        return FAILURE;
    }

    cursor = findFieldValue(line, "tokens");
    if (cursor == NULL) {
        return FAILURE;
    }

    skipSpaces(&cursor);

    if (*cursor != '[') {
        return FAILURE;
    }

    cursor++;

    while (*cursor != '\0') {
        char *token = NULL;

        skipSpaces(&cursor);

        if (*cursor == ']') {
            cursor++;
            *tokens = items;
            *tokens_count = count;
            return SUCCESS;
        }

        if (parseJsonString(&cursor, &token) == FAILURE) {
            freeTokens(items, count);
            return FAILURE;
        }

        if (appendToken(&items, &count, &capacity, token) == FAILURE) {
            free(token);
            freeTokens(items, count);
            return FAILURE;
        }

        skipSpaces(&cursor);

        if (*cursor == ',') {
            cursor++;
        } else if (*cursor == ']') {
            cursor++;
            *tokens = items;
            *tokens_count = count;
            return SUCCESS;
        } else {
            freeTokens(items, count);
            return FAILURE;
        }
    }

    freeTokens(items, count);

    return FAILURE;
}

static int parseDocumentLine(
    const char *line,
    int *doc_id,
    char **title,
    char ***tokens,
    int *tokens_count
) {
    if (line == NULL) {
        return FAILURE;
    }

    if (doc_id == NULL) {
        return FAILURE;
    }

    if (title == NULL) {
        return FAILURE;
    }

    if (tokens == NULL) {
        return FAILURE;
    }

    if (tokens_count == NULL) {
        return FAILURE;
    }

    *title = NULL;
    *tokens = NULL;
    *tokens_count = 0;

    if (parseDocIdField(line, doc_id) == FAILURE) {
        return FAILURE;
    }

    if (parseStringField(line, "title", title) == FAILURE) {
        return FAILURE;
    }

    if (parseTokensField(line, tokens, tokens_count) == FAILURE) {
        free(*title);
        *title = NULL;
        return FAILURE;
    }

    return SUCCESS;
}

static void runIndex(TreeType type, const char *data_path, const char *idx_path) {
    char *line;

    if (data_path == NULL) {
        return;
    }

    if (idx_path == NULL) {
        return;
    }

    Index *idx = createIndex(type);
    if (idx == NULL) {
        exit(FAILURE);
    }

    FILE *file = fopen(data_path, "r");
    if (file == NULL) {
        printf(FILE_OPEN_ERROR);
        freeIndex(idx);
        exit(FAILURE);
    }

    while ((line = getLine(file)) != NULL) {
        int doc_id;
        char *title;
        char **tokens;
        int tokens_count;

        if (line[0] == '\0') {
            free(line);
            continue;
        }

        if (parseDocumentLine(line, &doc_id, &title, &tokens, &tokens_count) == FAILURE) {
            printf(JSON_PARSE_ERROR);
            free(line);
            fclose(file);
            freeIndex(idx);
            exit(FAILURE);
        }

        indexDocument(idx, doc_id, title, tokens, tokens_count);

        free(title);
        freeTokens(tokens, tokens_count);
        free(line);
    }

    if (ferror(file)) {
        printf(FILE_OPEN_ERROR);
        fclose(file);
        freeIndex(idx);
        exit(FAILURE);
    }

    fclose(file);

    saveIndex(idx, idx_path);
    freeIndex(idx);
}

static void runSearch(TreeType type, const char *idx_path,
                      const char *query, int json_out) {
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

    SearchResults *search_results = search(idx, query);
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

        runSearch(type, idx_path, query, json_out);
    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        usage(argv[0]);
        return FAILURE;
    }

    return SUCCESS;
}
