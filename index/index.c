// Полностью на ваше усмотрение (только переиспользуйте код из предыдущих лабораторных, если он вам подходит)
#include <errno.h>

#include "index.h"
#include "../avl/avl.h"
#include "../btree/btree.h"
#include "../rbtree/rbtree.h"

#define INDEX_FIELD_SEPARATOR '\t'
#define VECTOR_ITEM_NULL_ERROR "ERROR: vector returned NULL element\n"
#define FILE_OPEN_ERROR "ERROR: cannot open file\n"
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define SUCCESS 0
#define FAILURE (-1)
#define BUFFER_SIZE 1024
#define STR_MIN_CAPACITY 128


static char *getLine(FILE *f) {
    if (!f) {
        return NULL;
    }
    char buffer[BUFFER_SIZE];
    char *line = NULL;
    size_t length = 0;
    size_t capacity = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        size_t buffer_len = strlen(buffer);
        int has_nl = 0;
        if (buffer_len > 0 && buffer[buffer_len - 1] == '\n') {
            has_nl = 1;
        }

        // убрали \n и \r на конце, в винде в конце может ставиться \r\n
        if (has_nl) {
            buffer_len--;
            if (buffer_len > 0 && buffer[buffer_len - 1] == '\r') {
                buffer_len--;
            }
        }

        if (length + buffer_len + 1 > capacity || !line) {
            size_t new_capacity = capacity;
            if (new_capacity == 0) {
                new_capacity = STR_MIN_CAPACITY;
            }
            while (new_capacity < length + buffer_len + 1) {
                new_capacity *= 2;
            }

            char *new_line = realloc(line, new_capacity);
            if (!new_line) {
                printf(MEMORY_ALLOCATION_ERROR);
                return NULL;
            }
            line = new_line;
            capacity = new_capacity;
        }
        memcpy(line + length, buffer, buffer_len);
        length += buffer_len;
        line[length] = '\0';

        if (has_nl) {
            break;
        }
    }
    if (!line) {
        return NULL;
    }
    return line;
}

typedef struct {
    FILE *file;
    int has_error;
} SaveIndexContext;

Index *createIndex(TreeType type) {
    Index *idx = malloc(sizeof(Index));
    if (idx == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    idx->type = type;

    switch (type) {
        case TREE_AVL:
            idx->tree = createAVLTree();
            break;
        case TREE_RB:
            idx->tree = createRBTree();
            break;
        case TREE_BTREE:
            idx->tree = createBTree();
            break;
        default:
            free(idx);
            return NULL;
    }
    if (idx->tree == NULL) {
        free(idx);
        return NULL;
    }
    return idx;
}

void insertTerm(Index *idx, const char *term, int doc_id, const char *title) {
    if (!idx || !term || !title) {
        return;
    }
    switch (idx->type) {
        case TREE_AVL:
            avlInsert(idx->tree, term, doc_id, title);
            break;
        case TREE_RB:
            rbInsert(idx->tree, term, doc_id, title);
            break;
        case TREE_BTREE:
            btreeInsert(idx->tree, term, doc_id, title);
            break;
        default:
            return;
    }
}

Vector *lookupTerm(const Index *idx, const char *term) {
    if (!idx || !term) {
        return NULL;
    }
    switch (idx->type) {
        case TREE_AVL:
            return avlSearch(idx->tree, term);
        case TREE_RB:
            return rbSearch(idx->tree, term);
        case TREE_BTREE:
            return btreeSearch(idx->tree, term);
        default:
            return NULL;
    }
}

void indexDocument(Index *idx, int doc_id, const char *title,
                   const char **tokens, int n_tokens) {
    if (!idx || !tokens || !title) {
        return;
    }
    if (n_tokens < 0) {
        return;
    }
    for (int token_index = 0; token_index < n_tokens; token_index++) {
        if (tokens[token_index] == NULL) {
            continue;
        }

        insertTerm(idx, tokens[token_index], doc_id, title);
    }
}

void traverseIndex(
    const Index *idx,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void *ctx
) {
    if (!idx || !visit || !ctx) {
        return;
    }
    switch (idx->type) {
        case TREE_AVL:
            avlTraverse(idx->tree, visit, ctx);
            break;
        case TREE_RB:
            rbTraverse(idx->tree, visit, ctx);
            break;
        case TREE_BTREE:
            btreeTraverse(idx->tree, visit, ctx);
            break;
        default:
            return;
    }
}


static void saveIndexEntry(const char *key, Vector *postings, void *ctx) {
    SaveIndexContext *save_context;
    size_t posting_index;

    if (ctx == NULL) {
        return;
    }

    save_context = (SaveIndexContext *) ctx;

    if (key == NULL) {
        save_context->has_error = 1;
        return;
    }

    if (postings == NULL) {
        save_context->has_error = 1;
        return;
    }

    for (posting_index = 0; posting_index < postings->size; posting_index++) {
        PostingEntry *entry = getVectorItem(postings, posting_index);
        if (entry == NULL) {
            printf(VECTOR_ITEM_NULL_ERROR);
            save_context->has_error = 1;
            return;
        }

        if (fprintf(
                save_context->file,
                "%s%c%d%c%s\n",
                key,
                INDEX_FIELD_SEPARATOR,
                entry->doc_id,
                INDEX_FIELD_SEPARATOR,
                entry->title
            ) < 0) {
            save_context->has_error = 1;
            return;
        }
    }
}


void saveIndex(const Index *idx, const char *path) {
    SaveIndexContext save_context;

    if (idx == NULL) {
        return;
    }

    if (path == NULL) {
        return;
    }

    save_context.file = fopen(path, "w");
    save_context.has_error = 0;

    if (save_context.file == NULL) {
        printf(FILE_OPEN_ERROR);
        return;
    }

    traverseIndex(idx, saveIndexEntry, &save_context);

    if (fclose(save_context.file) != 0) {
        printf(FILE_OPEN_ERROR);
        return;
    }

    if (save_context.has_error) {
        printf(FILE_OPEN_ERROR);
    }
}

static int parseIndexLine(char *line, char **term, int *doc_id, char **title) {
    char *end_ptr;

    if (line == NULL) {
        return FAILURE;
    }

    if (term == NULL) {
        return FAILURE;
    }

    if (doc_id == NULL) {
        return FAILURE;
    }

    if (title == NULL) {
        return FAILURE;
    }

    char *first_separator = strchr(line, INDEX_FIELD_SEPARATOR);
    if (first_separator == NULL) {
        return FAILURE;
    }

    char *second_separator = strchr(first_separator + 1, INDEX_FIELD_SEPARATOR);
    if (second_separator == NULL) {
        return FAILURE;
    }

    *first_separator = '\0';
    *second_separator = '\0';

    *term = line;
    char *doc_id_text = first_separator + 1;
    *title = second_separator + 1;

    errno = 0;
    long parsed_doc_id = strtol(doc_id_text, &end_ptr, 10);

    if (errno != 0) {
        return FAILURE;
    }

    if (end_ptr == doc_id_text) {
        return FAILURE;
    }

    if (*end_ptr != '\0') {
        return FAILURE;
    }

    *doc_id = (int) parsed_doc_id;

    return SUCCESS;
}

Index *loadIndex(const char *path, TreeType type) {
    Index *idx;
    FILE *file;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        printf(FILE_OPEN_ERROR);
        return NULL;
    }

    idx = createIndex(type);
    if (idx == NULL) {
        fclose(file);
        return NULL;
    }

    while (1) {
        char *line;
        char *term;
        char *title;
        int doc_id;

        line = getLine(file);
        if (line == NULL) {
            break;
        }

        if (line[0] == '\0') {
            free(line);
            continue;
        }

        if (parseIndexLine(line, &term, &doc_id, &title) == FAILURE) {
            free(line);
            continue;
        }

        insertTerm(idx, term, doc_id, title);
        free(line);
    }

    if (ferror(file)) {
        printf(FILE_OPEN_ERROR);
        fclose(file);
        freeIndex(idx);
        return NULL;
    }

    fclose(file);

    return idx;
}

void freeIndex(Index *idx) {
    if (idx == NULL) {
        return;
    }

    if (idx->tree != NULL) {
        switch (idx->type) {
            case TREE_AVL:
                freeAVLTree(idx->tree);
                break;

            case TREE_RB:
                freeRBTree(idx->tree);
                break;

            case TREE_BTREE:
                freeBTree(idx->tree);
                break;

            default:
                break;
        }
    }

    free(idx);
}
