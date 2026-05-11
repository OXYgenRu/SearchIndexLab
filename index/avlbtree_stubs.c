/* Заглушки-пустышки для AVL и B-tree.
   index.c ссылается на эти символы, но сами реализации AVL/B-tree живут в
   ветках других ребят и пока не вмёрджены в main. Заглушки позволяют
   test_search и bench линковаться сейчас. Когда настоящие реализации
   появятся — этот файл нужно убрать из соответствующих целей Makefile. */

#include <stddef.h>

#include "../avl/avl.h"
#include "../btree/btree.h"

AVLTree *createAVLTree(void) {
    return NULL;
}

void freeAVLTree(AVLTree *tree) {
    (void) tree;
}

void avlInsert(AVLTree *tree, const char *key, int doc_id, const char *title) {
    (void) tree;
    (void) key;
    (void) doc_id;
    (void) title;
}

Vector *avlSearch(const AVLTree *tree, const char *key) {
    (void) tree;
    (void) key;
    return NULL;
}

void avlTraverse(
    const AVLTree *tree,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void *ctx
) {
    (void) tree;
    (void) visit;
    (void) ctx;
}

BTree *createBTree(void) {
    return NULL;
}

void freeBTree(BTree *tree) {
    (void) tree;
}

void btreeInsert(BTree *tree, const char *key, int doc_id, const char *title) {
    (void) tree;
    (void) key;
    (void) doc_id;
    (void) title;
}

Vector *btreeSearch(const BTree *tree, const char *key) {
    (void) tree;
    (void) key;
    return NULL;
}

void btreeTraverse(
    const BTree *tree,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void *ctx
) {
    (void) tree;
    (void) visit;
    (void) ctx;
}
