#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avl.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"

static int node_height(const AVLNode *node) {
    if (node == NULL) {
        return 0;
    }
    return node->height;
}

static int max_of(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

static void recalc_height(AVLNode *node) {
    if (node == NULL) {
        return;
    }
    node->height = 1 + max_of(node_height(node->left), node_height(node->right));
}

static int balance_of(const AVLNode *node) {
    if (node == NULL) {
        return 0;
    }
    return node_height(node->left) - node_height(node->right);
}

static AVLNode *rotate_right(AVLNode *y) {
    if (y == NULL) {
        return NULL;
    }
    if (y->left == NULL) {
        return y;
    }
    AVLNode *x   = y->left;
    AVLNode *mid = x->right;
    x->right = y;
    y->left  = mid;
    recalc_height(y);
    recalc_height(x);
    return x;
}

static AVLNode *rotate_left(AVLNode *x) {
    if (x == NULL) {
        return NULL;
    }
    if (x->right == NULL) {
        return x;
    }
    AVLNode *y   = x->right;
    AVLNode *mid = y->left;
    y->left  = x;
    x->right = mid;
    recalc_height(x);
    recalc_height(y);
    return y;
}

static AVLNode *rebalance(AVLNode *node) {
    if (node == NULL) {
        return NULL;
    }
    recalc_height(node);
    int bf = balance_of(node);

    if (bf > 1) {
        if (balance_of(node->left) < 0) {
            node->left = rotate_left(node->left);
        }
        return rotate_right(node);
    }
    if (bf < -1) {
        if (balance_of(node->right) > 0) {
            node->right = rotate_right(node->right);
        }
        return rotate_left(node);
    }
    return node;
}

static AVLNode *make_node(const char *key, int doc_id, const char *title) {
    AVLNode *node = malloc(sizeof(AVLNode));
    if (node == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    node->key = malloc(strlen(key) + 1);
    if (node->key == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(node);
        return NULL;
    }
    strcpy(node->key, key);

    node->postings = createPostingList();
    if (node->postings == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(node->key);
        free(node);
        return NULL;
    }
    appendPosting(node->postings, doc_id, title);

    node->left   = NULL;
    node->right  = NULL;
    node->height = 1;
    return node;
}

static void destroy_node(AVLNode *node) {
    if (node == NULL) {
        return;
    }
    free(node->key);
    if (vectorFree(node->postings) != SUCCESS) {
        printf("ERROR: vectorFree failed in destroy_node\n");
    }
    free(node);
}

static AVLNode *insert_recursive(
    AVLNode    *node,
    const char *key,
    int         doc_id,
    const char *title,
    int        *new_key,
    int        *err
) {
    if (node == NULL) {
        AVLNode *created = make_node(key, doc_id, title);
        if (created == NULL) {
            *err = 1;
            return NULL;
        }
        *new_key = 1;
        return created;
    }

    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        AVLNode *updated_left = insert_recursive(node->left, key, doc_id, title, new_key, err);
        if (*err) {
            return node;
        }
        node->left = updated_left;
    } else if (cmp > 0) {
        AVLNode *updated_right = insert_recursive(node->right, key, doc_id, title, new_key, err);
        if (*err) {
            return node;
        }
        node->right = updated_right;
    } else {
        appendPosting(node->postings, doc_id, title);
        return node;
    }

    return rebalance(node);
}

static Vector *search_recursive(const AVLNode *node, const char *key) {
    if (node == NULL) {
        return NULL;
    }
    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        return search_recursive(node->left, key);
    }
    if (cmp > 0) {
        return search_recursive(node->right, key);
    }
    return node->postings;
}

static void traverse_recursive(
    const AVLNode *node,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void *ctx
) {
    if (node == NULL) {
        return;
    }
    traverse_recursive(node->left, visit, ctx);
    visit(node->key, node->postings, ctx);
    traverse_recursive(node->right, visit, ctx);
}

static void free_recursive(AVLNode *node) {
    if (node == NULL) {
        return;
    }
    free_recursive(node->left);
    free_recursive(node->right);
    destroy_node(node);
}

AVLTree *createAVLTree(void) {
    AVLTree *tree = malloc(sizeof(AVLTree));
    if (tree == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    tree->root = NULL;
    tree->size = 0;
    return tree;
}

void freeAVLTree(AVLTree *tree) {
    if (tree == NULL) {
        return;
    }
    free_recursive(tree->root);
    free(tree);
}

void avlInsert(AVLTree *tree, const char *key, int doc_id, const char *title) {
    if (tree == NULL) {
        return;
    }
    if (key == NULL) {
        return;
    }
    if (title == NULL) {
        return;
    }

    int new_key = 0;
    int err     = 0;

    AVLNode *new_root = insert_recursive(tree->root, key, doc_id, title, &new_key, &err);
    if (err) {
        return;
    }

    tree->root  = new_root;
    tree->size += new_key;
}

Vector *avlSearch(const AVLTree *tree, const char *key) {
    if (tree == NULL) {
        return NULL;
    }
    if (key == NULL) {
        return NULL;
    }
    return search_recursive(tree->root, key);
}

void avlTraverse(
    const AVLTree *tree,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void *ctx
) {
    if (tree == NULL) {
        return;
    }
    if (visit == NULL) {
        return;
    }
    traverse_recursive(tree->root, visit, ctx);
}