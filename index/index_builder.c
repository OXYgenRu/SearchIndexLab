#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index_builder.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define FILE_OPEN_ERROR         "ERROR: cannot open file\n"

#define INITIAL_LINE_CAPACITY   1024
#define INITIAL_TOKENS_CAPACITY 16

typedef struct {
    char **items;
    int    count;
    int    capacity;
} TokenArray;

static int tokensInit(TokenArray *t) {
    t->items = malloc(INITIAL_TOKENS_CAPACITY * sizeof(char *));
    if (t->items == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }
    t->count = 0;
    t->capacity = INITIAL_TOKENS_CAPACITY;
    return SUCCESS;
}

static int tokensAppend(TokenArray *t, char *token) {
    if (t->count >= t->capacity) {
        int new_capacity = t->capacity * 2;
        char **new_items = realloc(t->items, new_capacity * sizeof(char *));
        if (new_items == NULL) {
            printf(MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }
        t->items = new_items;
        t->capacity = new_capacity;
    }
    t->items[t->count++] = token;
    return SUCCESS;
}

static void tokensFree(TokenArray *t) {
    if (t->items == NULL) {
        return;
    }
    for (int i = 0; i < t->count; i++) {
        free(t->items[i]);
    }
    free(t->items);
    t->items = NULL;
    t->count = 0;
    t->capacity = 0;
}

static const char *skipWhitespace(const char *p) {
    while (*p != '\0' && isspace((unsigned char) *p)) {
        p++;
    }
    return p;
}

/* читает JSON-строку начиная с позиции *p, ожидая что *p == '"';
   при успехе устанавливает *out на свежевыделенную строку, возвращает
   указатель сразу после закрывающей кавычки */
static const char *readJsonString(const char *p, char **out) {
    *out = NULL;
    if (*p != '"') {
        return NULL;
    }
    p++;

    size_t capacity = 64;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    while (*p != '\0' && *p != '"') {
        char c;
        if (*p == '\\') {
            p++;
            if (*p == '\0') {
                free(buffer);
                return NULL;
            }
            switch (*p) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'u':
                    /* \uXXXX — пропускаем 4 шестнадцатеричных символа,
                       подставляя '?' (для нашего ASCII-датасета достаточно) */
                    for (int i = 0; i < 4 && p[1] != '\0'; i++) {
                        p++;
                    }
                    c = '?';
                    break;
                default:
                    c = *p;
                    break;
            }
            p++;
        } else {
            c = *p++;
        }

        if (length + 1 >= capacity) {
            size_t new_capacity = capacity * 2;
            char *new_buffer = realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                printf(MEMORY_ALLOCATION_ERROR);
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        buffer[length++] = c;
    }

    if (*p != '"') {
        free(buffer);
        return NULL;
    }
    buffer[length] = '\0';
    *out = buffer;
    return p + 1;
}

/* находит ключ "name" в строке (требуется чтобы перед ":" были пробелы),
   возвращает позицию сразу после двоеточия */
static const char *findKey(const char *line, const char *name) {
    size_t name_len = strlen(name);
    const char *p = line;
    while ((p = strchr(p, '"')) != NULL) {
        if (strncmp(p + 1, name, name_len) == 0 && p[1 + name_len] == '"') {
            p += 1 + name_len + 1;
            p = skipWhitespace(p);
            if (*p != ':') {
                return NULL;
            }
            p++;
            return skipWhitespace(p);
        }
        p++;
    }
    return NULL;
}

static int parseDocId(const char *line, int *doc_id) {
    const char *p = findKey(line, "doc_id");
    if (p == NULL) {
        return FAILURE;
    }
    char *value = NULL;
    if (*p == '"') {
        if (readJsonString(p, &value) == NULL || value == NULL) {
            return FAILURE;
        }
        char *end;
        long parsed = strtol(value, &end, 10);
        if (*value == '\0' || *end != '\0') {
            free(value);
            return FAILURE;
        }
        *doc_id = (int) parsed;
        free(value);
        return SUCCESS;
    }
    char *end;
    long parsed = strtol(p, &end, 10);
    if (end == p) {
        return FAILURE;
    }
    *doc_id = (int) parsed;
    return SUCCESS;
}

static int parseTitle(const char *line, char **title_out) {
    const char *p = findKey(line, "title");
    if (p == NULL) {
        return FAILURE;
    }
    if (*p != '"') {
        return FAILURE;
    }
    if (readJsonString(p, title_out) == NULL || *title_out == NULL) {
        return FAILURE;
    }
    return SUCCESS;
}

static int parseTokens(const char *line, TokenArray *tokens) {
    const char *p = findKey(line, "tokens");
    if (p == NULL) {
        return FAILURE;
    }
    if (*p != '[') {
        return FAILURE;
    }
    p++;
    p = skipWhitespace(p);

    if (tokensInit(tokens) == FAILURE) {
        return FAILURE;
    }

    while (*p != ']' && *p != '\0') {
        char *token = NULL;
        if (*p != '"') {
            tokensFree(tokens);
            return FAILURE;
        }
        p = readJsonString(p, &token);
        if (p == NULL || token == NULL) {
            tokensFree(tokens);
            return FAILURE;
        }
        if (tokensAppend(tokens, token) == FAILURE) {
            free(token);
            tokensFree(tokens);
            return FAILURE;
        }
        p = skipWhitespace(p);
        if (*p == ',') {
            p++;
            p = skipWhitespace(p);
        }
    }
    if (*p != ']') {
        tokensFree(tokens);
        return FAILURE;
    }
    return SUCCESS;
}

static char *readLine(FILE *file) {
    size_t capacity = INITIAL_LINE_CAPACITY;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    int c;
    while ((c = fgetc(file)) != EOF && c != '\n') {
        if (length + 1 >= capacity) {
            size_t new_capacity = capacity * 2;
            char *new_buffer = realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                printf(MEMORY_ALLOCATION_ERROR);
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        buffer[length++] = (char) c;
    }
    if (c == EOF && length == 0) {
        free(buffer);
        return NULL;
    }
    if (length > 0 && buffer[length - 1] == '\r') {
        length--;
    }
    buffer[length] = '\0';
    return buffer;
}

int buildIndexFromJsonl(Index *idx, const char *path, int limit) {
    if (idx == NULL || path == NULL) {
        return FAILURE;
    }
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf(FILE_OPEN_ERROR);
        return FAILURE;
    }

    int count = 0;

    while (limit <= 0 || count < limit) {
        char *line = readLine(file);
        if (line == NULL) {
            break;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        int doc_id = 0;
        char *title = NULL;
        TokenArray tokens = {0};

        if (parseDocId(line, &doc_id) == FAILURE) {
            free(line);
            continue;
        }
        if (parseTitle(line, &title) == FAILURE) {
            free(line);
            continue;
        }
        if (parseTokens(line, &tokens) == FAILURE) {
            free(title);
            free(line);
            continue;
        }

        indexDocument(idx, doc_id, title, (const char **) tokens.items, tokens.count);

        tokensFree(&tokens);
        free(title);
        free(line);
        count++;
    }

    fclose(file);
    return count;
}
