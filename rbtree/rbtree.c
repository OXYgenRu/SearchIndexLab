#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rbtree.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"

/* Создаёт новый узел с заданным ключом.
   Инициализирует структуру, выделяет память под ключ и лист постингов */
static RBNode *createNode(RBTree *tree, const char *key) {
    RBNode *node = malloc(sizeof(RBNode));
    if (node == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    node->key = strdup(key);
    if (node->key == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(node);
        return NULL;
    }

    /* Новый узел всегда красный (красно-чёрное дерево) */
    node->color    = RB_RED;
    /* Создаём пустой список документов с этим ключом */
    node->postings = createPostingList();
    if (node->postings == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(node->key);
        free(node);
        return NULL;
    }

    /* Указатели на потомков и родителя — на sentinel nil */
    node->left   = tree->nil;
    node->right  = tree->nil;
    node->parent = tree->nil;

    return node;
}

/* Освобождает память узла: ключ, список постингов и сам узел */
static void freeNode(RBTree *tree, RBNode *node) {
    if (node == NULL || node == tree->nil) {
        return;
    }
    free(node->key);
    vectorFree(node->postings);
    free(node);
}

/* Поворот влево: узел x опускается вниз, его правый сын y поднимается на место x */
static void rotateLeft(RBTree *tree, RBNode *x) {
    RBNode *y = x->right;

    /* Левого потомка y переносим в качестве правого потомка x */
    x->right = y->left;
    if (y->left != tree->nil) {
        y->left->parent = x;
    }

    /* y займёт место x в дереве */
    y->parent = x->parent;

    if (x->parent == tree->nil) {
        /* Если x был корнем, y становится корнем */
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    /* Переставляем узлы: x становится левым потомком y */
    y->left   = x;
    x->parent = y;
}

/* Поворот вправо: зеркальная операция для поворота влево */
static void rotateRight(RBTree *tree, RBNode *x) {
    RBNode *y = x->left;

    /* Правого потомка y переносим в качестве левого потомка x */
    x->left = y->right;
    if (y->right != tree->nil) {
        y->right->parent = x;
    }

    /* y займёт место x в дереве */
    y->parent = x->parent;

    if (x->parent == tree->nil) {
        tree->root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }

    /* x становится правым потомком y */
    y->right  = x;
    x->parent = y;
}

/* Восстанавливает свойства красно-чёрного дерева после вставки.
   Проверяет конфликты (красный родитель с красным потомком) и исправляет их. */
static void insertFixup(RBTree *tree, RBNode *z) {
    /* Цикл: пока у узла z красный родитель — нужно исправлять */
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            /* Родитель z — левый ребёнок деда */
            RBNode *y = z->parent->parent->right;  /* дядя z */

            if (y->color == RB_RED) {
                /* Дядя красный: перекрашиваем дядю и родителя в чёрный,
                   деда в красный, и двигаемся выше */
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                /* Дядя чёрный: нужны ротации */
                if (z == z->parent->right) {
                    /* z — правый потомок: ротируем родителя влево,
                       чтобы привести к следующему случаю */
                    z = z->parent;
                    rotateLeft(tree, z);
                }
                /* z — левый потомок: ротируем деда вправо и перекрашиваем */
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rotateRight(tree, z->parent->parent);
            }
        } else {
            /* Зеркальный случай: родитель z — правый ребёнок деда */
            RBNode *y = z->parent->parent->left;

            if (y->color == RB_RED) {
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotateRight(tree, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rotateLeft(tree, z->parent->parent);
            }
        }
    }
    /* Корень всегда должен быть чёрным */
    tree->root->color = RB_BLACK;
}

/* Рекурсивно обходит поддерево и освобождает каждый узел.
   Сначала левое подderevo, потом правое, потом сам узел */
static void freeSubtree(RBTree *tree, RBNode *node) {
    if (node == tree->nil) {
        return;
    }
    freeSubtree(tree, node->left);
    freeSubtree(tree, node->right);
    freeNode(tree, node);
}

/* Инфиксный обход (in-order): левое поддерево -> сам узел -> правое поддерево.
   Гарантирует перебор элементов в отсортированном порядке ключей.
   Вызывает visit-функцию для каждого узла */
static void traverseSubtree(
    const RBTree *tree,
    RBNode       *node,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void         *ctx
) {
    if (node == tree->nil) {
        return;
    }
    traverseSubtree(tree, node->left, visit, ctx);
    visit(node->key, node->postings, ctx);
    traverseSubtree(tree, node->right, visit, ctx);
}

/* Создаёт новое пустое красно-чёрное дерево.
   Инициализирует sentinel nil и другие поля */
RBTree *createRBTree(void) {
    RBTree *tree = malloc(sizeof(RBTree));
    if (tree == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return NULL;
    }

    /* Создаём sentinel nil — спецузел, на который указывают листья.
       Всегда чёрный и используется как граница дерева */
    tree->nil = malloc(sizeof(RBNode));
    if (tree->nil == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        free(tree);
        return NULL;
    }

    /* Инициализируем nil: всё указывает на себя же */
    tree->nil->color    = RB_BLACK;
    tree->nil->key      = NULL;
    tree->nil->postings = NULL;
    tree->nil->left     = tree->nil;
    tree->nil->right    = tree->nil;
    tree->nil->parent   = tree->nil;

    tree->root = tree->nil;
    tree->size = 0;

    return tree;
}

void freeRBTree(RBTree *tree) {
    if (tree == NULL) {
        return;
    }
    freeSubtree(tree, tree->root);
    free(tree->nil);
    free(tree);
}

void rbInsert(RBTree *tree, const char *key, int doc_id, const char *title) {
    if (tree == NULL) {
        return;
    }
    if (key == NULL) {
        return;
    }
    if (title == NULL) {
    return;
    }

    /* Ищем: если ключ уже есть — просто добавляем в posting list */
    RBNode *current = tree->root;
    while (current != tree->nil) {
        int cmp = strcmp(key, current->key);
        if (cmp == 0) {
            appendPosting(current->postings, doc_id, title);
            return;
        }
        if (cmp < 0) {
            current = current->left;
        } else {
            current = current->right;
        }
    }

    /* Ключ не найден — создаём новый узел */
    RBNode *z = createNode(tree, key);
    if (z == NULL) {
        return;
    }
    appendPosting(z->postings, doc_id, title);

    /* Обычная вставка в BST */
    RBNode *parent = tree->nil;
    current = tree->root;

    while (current != tree->nil) {
        parent = current;
        if (strcmp(z->key, current->key) < 0) {
            current = current->left;
        } else {
            current = current->right;
        }
    }

    z->parent = parent;

    if (parent == tree->nil) {
        tree->root = z;
    } else if (strcmp(z->key, parent->key) < 0) {
        parent->left = z;
    } else {
        parent->right = z;
    }

    tree->size++;
    insertFixup(tree, z);
}

Vector *rbSearch(const RBTree *tree, const char *key) {
    if (tree == NULL) {
        return NULL;
    }
    if (key == NULL) {
        return NULL;
    }

    RBNode *current = tree->root;

    while (current != tree->nil) {
        int cmp = strcmp(key, current->key);
        if (cmp == 0) {
            return current->postings;
        }
        if (cmp < 0) {
            current = current->left;
        } else {
            current = current->right;
        }
    }

    return NULL;
}

void rbTraverse(
    const RBTree *tree,
    void (*visit)(const char *key, Vector *postings, void *ctx),
    void         *ctx
) {
    if (tree == NULL) {
        return;
    }
    if (visit == NULL) {
        return;
    }
    traverseSubtree(tree, tree->root, visit, ctx);
}