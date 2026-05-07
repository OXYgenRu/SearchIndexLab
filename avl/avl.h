#pragma once

#include "../posting.h"

typedef struct AVLNode {
    char*           key;
    int             height;
    Vector*         postings;
    struct AVLNode* left;
    struct AVLNode* right;
} AVLNode;

typedef struct {
    AVLNode* root;
    int      size;
} AVLTree;

AVLTree* createAVLTree(void);
void     freeAVLTree(AVLTree* tree);

void    avlInsert(AVLTree* tree, const char* key, int doc_id, const char* title);
Vector* avlSearch(const AVLTree* tree, const char* key);
void    avlTraverse(
    const AVLTree* tree,
    void (*visit)(const char* key, Vector* postings, void* ctx),
    void* ctx
);
