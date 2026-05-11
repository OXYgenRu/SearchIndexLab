#define _POSIX_C_SOURCE 200809L

#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"

#define SUCCESS 0
#define FAILURE (-1)

static BTreeNode* createBTreeNode(int is_leaf);
static void freeBTreeNode(BTreeNode* node);

static Vector* btreeSearchInNode(BTreeNode* node, const char* key);

static void btreeTraverseNode(
        BTreeNode* node,
        void (*visit)(const char*, Vector*, void*),
        void* ctx
);

static int btreeSplitChild(
        BTreeNode* parent,
        int child_index,
        BTreeNode* full_child
);

static int btreeInsertNonFull(
        BTreeNode* node,
        const char* key,
        int doc_id,
        const char* title
);

BTree* createBTree(void) {
    BTree* tree = (BTree*)malloc(sizeof(BTree));

    if (tree == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    BTreeNode* root = createBTreeNode(1);

    if (root == NULL) {
        free(tree);
        return NULL;
    }

    tree->root = root;
    tree->size = 0;

    return tree;
}

void freeBTree(BTree* tree) {
    if (tree == NULL) {
        return;
    }

    freeBTreeNode(tree->root);

    free(tree);
}

void btreeInsert(
        BTree* tree,
        const char* key,
        int doc_id,
        const char* title
) {
    if (tree == NULL || key == NULL || title == NULL) {
        return;
    }

    Vector* postings = btreeSearch(tree, key);

    if (postings != NULL) {
        appendPosting(postings, doc_id, title);
        return;
    }

    BTreeNode* root = tree->root;

    if (root == NULL) {
        return;
    }

    if (root->n == BTREE_MAX_KEYS) {
        BTreeNode* new_root = createBTreeNode(0);

        if (new_root == NULL) {
            return;
        }

        new_root->children[0] = root;
        tree->root = new_root;

        if (btreeSplitChild(new_root, 0, root) == FAILURE) {
            tree->root = root;
            free(new_root);
            return;
        }

        if (btreeInsertNonFull(new_root, key, doc_id, title) == FAILURE) {
            return;
        }
    } else {
        if (btreeInsertNonFull(root, key, doc_id, title) == FAILURE) {
            return;
        }
    }

    tree->size++;
}

Vector* btreeSearch(const BTree* tree, const char* key) {
    if (tree == NULL || tree->root == NULL || key == NULL) {
        return NULL;
    }

    return btreeSearchInNode(tree->root, key);
}

void btreeTraverse(
        const BTree* tree,
        void (*visit)(const char* key, Vector* postings, void* ctx),
        void* ctx
) {
    if (tree == NULL || tree->root == NULL || visit == NULL) {
        return;
    }

    btreeTraverseNode(tree->root, visit, ctx);
}

static BTreeNode* createBTreeNode(int is_leaf) {
    BTreeNode* node = (BTreeNode*)malloc(sizeof(BTreeNode));

    if (node == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    node->is_leaf = is_leaf;
    node->n = 0;

    for (int i = 0; i < BTREE_MAX_KEYS; i++) {
        node->keys[i] = NULL;
        node->postings[i] = NULL;
    }

    for (int i = 0; i < BTREE_MAX_CH; i++) {
        node->children[i] = NULL;
    }

    return node;
}

static void freeBTreeNode(BTreeNode* node) {
    if (node == NULL) {
        return;
    }

    if (!node->is_leaf) {
        for (int i = 0; i <= node->n; i++) {
            freeBTreeNode(node->children[i]);
        }
    }

    for (int i = 0; i < node->n; i++) {
        free(node->keys[i]);
        vectorFree(node->postings[i]);
    }

    free(node);
}

static Vector* btreeSearchInNode(BTreeNode* node, const char* key) {
    if (node == NULL || key == NULL) {
        return NULL;
    }

    int i = 0;

    while (i < node->n && strcmp(key, node->keys[i]) > 0) {
        i++;
    }

    if (i < node->n && strcmp(key, node->keys[i]) == 0) {
        return node->postings[i];
    }

    if (node->is_leaf) {
        return NULL;
    }

    return btreeSearchInNode(node->children[i], key);
}

static int btreeSplitChild(
        BTreeNode* parent,
        int child_index,
        BTreeNode* full_child
) {
    if (parent == NULL || full_child == NULL) {
        return FAILURE;
    }

    BTreeNode* new_sibling = createBTreeNode(full_child->is_leaf);

    if (new_sibling == NULL) {
        return FAILURE;
    }

    new_sibling->n = BTREE_T - 1;

    for (int j = 0; j < BTREE_T - 1; j++) {
        new_sibling->keys[j] =
                full_child->keys[j + BTREE_T];

        new_sibling->postings[j] =
                full_child->postings[j + BTREE_T];

        full_child->keys[j + BTREE_T] = NULL;
        full_child->postings[j + BTREE_T] = NULL;
    }

    if (!full_child->is_leaf) {
        for (int j = 0; j < BTREE_T; j++) {
            new_sibling->children[j] =
                    full_child->children[j + BTREE_T];

            full_child->children[j + BTREE_T] = NULL;
        }
    }

    full_child->n = BTREE_T - 1;

    for (int j = parent->n; j >= child_index + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }

    parent->children[child_index + 1] = new_sibling;

    for (int j = parent->n - 1; j >= child_index; j--) {
        parent->keys[j + 1] = parent->keys[j];
        parent->postings[j + 1] = parent->postings[j];
    }

    parent->keys[child_index] =
            full_child->keys[BTREE_T - 1];

    parent->postings[child_index] =
            full_child->postings[BTREE_T - 1];

    full_child->keys[BTREE_T - 1] = NULL;
    full_child->postings[BTREE_T - 1] = NULL;

    parent->n++;

    return SUCCESS;
}

static int btreeInsertNonFull(
        BTreeNode* node,
        const char* key,
        int doc_id,
        const char* title
) {
    if (node == NULL || key == NULL || title == NULL) {
        return FAILURE;
    }

    int i = node->n - 1;

    if (node->is_leaf) {
        while (i >= 0 && strcmp(key, node->keys[i]) < 0) {
            node->keys[i + 1] = node->keys[i];
            node->postings[i + 1] = node->postings[i];
            i--;
        }

        char* key_copy = strdup(key);

        if (key_copy == NULL) {
            printf("%s", MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }

        Vector* postings = createPostingList();

        if (postings == NULL) {
            free(key_copy);
            return FAILURE;
        }

        appendPosting(postings, doc_id, title);

        node->keys[i + 1] = key_copy;
        node->postings[i + 1] = postings;

        node->n++;

        return SUCCESS;
    }

    while (i >= 0 && strcmp(key, node->keys[i]) < 0) {
        i--;
    }

    i++;

    if (node->children[i] == NULL) {
        return FAILURE;
    }

    if (node->children[i]->n == BTREE_MAX_KEYS) {
        if (
                btreeSplitChild(node, i, node->children[i])
                == FAILURE
                ) {
            return FAILURE;
        }

        if (strcmp(key, node->keys[i]) > 0) {
            i++;
        }
    }

    return btreeInsertNonFull(
            node->children[i],
            key,
            doc_id,
            title
    );
}

static void btreeTraverseNode(
        BTreeNode* node,
        void (*visit)(const char*, Vector*, void*),
        void* ctx
) {
    if (node == NULL || visit == NULL) {
        return;
    }

    int i;

    for (i = 0; i < node->n; i++) {
        if (!node->is_leaf) {
            btreeTraverseNode(node->children[i], visit, ctx);
        }

        visit(node->keys[i], node->postings[i], ctx);
    }

    if (!node->is_leaf) {
        btreeTraverseNode(node->children[i], visit, ctx);
    }
}