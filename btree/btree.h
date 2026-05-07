#pragma once

#include "../posting.h"

#define BTREE_T        3
#define BTREE_MAX_KEYS (2 * BTREE_T - 1)
#define BTREE_MAX_CH   (2 * BTREE_T)

typedef struct BTreeNode {
    char*             keys[BTREE_MAX_KEYS];
    Vector*           postings[BTREE_MAX_KEYS];
    struct BTreeNode* children[BTREE_MAX_CH];
    int               n;
    int               is_leaf;
} BTreeNode;

typedef struct {
    BTreeNode* root;
    int        size;
} BTree;

BTree*  createBTree(void);
void    freeBTree(BTree* tree);

void    btreeInsert(BTree* tree, const char* key, int doc_id, const char* title);
Vector* btreeSearch(const BTree* tree, const char* key);
void    btreeTraverse(
    const BTree* tree,
    void (*visit)(const char* key, Vector* postings, void* ctx),
    void* ctx
);
