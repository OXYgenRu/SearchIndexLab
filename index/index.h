#pragma once

#include "../posting.h"

typedef enum { TREE_AVL, TREE_RB, TREE_BTREE } TreeType;

typedef struct {
    void*    tree;
    TreeType type;
} Index;

Index*   createIndex(TreeType type);
void     insertTerm(Index* idx, const char* term, int doc_id, const char* title);
Vector*  lookupTerm(const Index* idx, const char* term);
void     indexDocument(Index* idx, int doc_id, const char* title,
                       const char** tokens, int n_tokens);
void     traverseIndex(
    const Index* idx,
    void (*visit)(const char* key, Vector* postings, void* ctx),
    void* ctx
);
void     saveIndex(const Index* idx, const char* path);
Index*   loadIndex(const char* path, TreeType type);
void     freeIndex(Index* idx);
