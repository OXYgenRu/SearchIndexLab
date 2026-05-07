#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rbtree.h"
#include "../posting.h"


#define SUCCESS 0
#define FAILURE (-1)

#define TEST_HEADER "\n====================\n"

#define ASSERT_TRUE(condition, msg)                                      \
    do {                                                                 \
        if (!(condition)) {                                              \
            printf("FAIL: %s (line %d)\n", msg, __LINE__);               \
            return FAILURE;                                              \
        }                                                                \
    } while (0)

#define ASSERT_INT_EQ(actual_value, expected_value, msg)                 \
    do {                                                                 \
        if ((actual_value) != (expected_value)) {                        \
            printf("FAIL: %s (line %d): expected %d, got %d\n",          \
                   msg, __LINE__, (expected_value), (actual_value));     \
            return FAILURE;                                              \
        }                                                                \
    } while (0)

#define ASSERT_PTR_EQ(actual_value, expected_value, msg)                 \
    do {                                                                 \
        if ((actual_value) != (expected_value)) {                        \
            printf("FAIL: %s (line %d): pointers differ\n", msg, __LINE__); \
            return FAILURE;                                              \
        }                                                                \
    } while (0)

static int runTest(const char *name, int (*fn)()) {
    printf(TEST_HEADER);
    printf("RUN: %s\n", name);

    int result = fn();

    if (result == SUCCESS) {
        printf("OK : %s\n", name);
    } else {
        printf("BAD: %s\n", name);
    }

    printf(TEST_HEADER);
    return result;
}

/* Извлекает doc_id первого элемента из списка постингов */
static int firstPostingDocId(const Vector* list) {
    if (list == NULL) return -1;
    PostingEntry* entry = getVectorItem((Vector*)list, 0);
    if (entry == NULL) return -1;
    return entry->doc_id;
}

/* Колбэк для сбора всех ключей при обходе дерева
   Собирает их в одну строку через запятую */
static char collected_keys[1024];
static int  collect_index;

static void collector(const char* key, Vector* postings, void* ctx) {
    (void)postings;  /* не используем список постингов в этой функции */
    (void)ctx;       /* контекст тоже не нужен */
    if (key == NULL) return;
    size_t len = strlen(key);
    /* проверяем что в буфере еще есть место для нового ключа */
    if (collect_index + len + 1 < (int)sizeof(collected_keys)) {
        /* если это не первый ключ, добавляем запятую-разделитель */
        if (collect_index > 0) {
            collected_keys[collect_index++] = ',';
        }
        /* копируем сам ключ */
        memcpy(collected_keys + collect_index, key, len);
        collect_index += len;
        collected_keys[collect_index] = '\0';
    }
}

/* Проверяем что freeRBTree спокойно переносит NULL без краша */
static int test_free_null_safety() {
    freeRBTree(NULL);
    return SUCCESS;
}

/* Проверяем что новое дерево создается правильно
   root должен указывать на nil, размер = 0 */
static int test_create_rbtree_basic() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");
    ASSERT_TRUE(tree->root == tree->nil, "initial root must be nil sentinel");
    ASSERT_INT_EQ(tree->size, 0, "initial size must be 0");

    Vector* res = rbSearch(tree, "anything");
    ASSERT_TRUE(res == NULL, "search on empty tree should return NULL");

    freeRBTree(tree);
    return SUCCESS;
}

/* Вставляем один элемент и проверяем что поиск его находит */
static int test_insert_single() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    rbInsert(tree, "test", 10, "Title10");

    Vector* pl = rbSearch(tree, "test");
    ASSERT_TRUE(pl != NULL, "search for existing key must return a list");
    ASSERT_INT_EQ((int)pl->size, 1, "posting list must have one entry");

    PostingEntry* entry = getVectorItem(pl, 0);
    ASSERT_TRUE(entry != NULL, "entry must not be NULL");
    ASSERT_INT_EQ(entry->doc_id, 10, "doc_id mismatch");
    ASSERT_TRUE(strcmp(entry->title, "Title10") == 0, "title mismatch");

    freeRBTree(tree);
    return SUCCESS;
}

/* Вставляем несколько разных ключей и проверяем каждый */
static int test_insert_multiple_distinct() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    rbInsert(tree, "banana", 1, "B");
    rbInsert(tree, "apple",  2, "A");
    rbInsert(tree, "cherry", 3, "C");

    ASSERT_INT_EQ(tree->size, 3, "size must be 3 after three inserts");

    Vector* pl = rbSearch(tree, "apple");
    ASSERT_TRUE(pl != NULL, "apple not found");
    ASSERT_INT_EQ(firstPostingDocId(pl), 2, "apple doc_id wrong");

    pl = rbSearch(tree, "banana");
    ASSERT_TRUE(pl != NULL, "banana not found");
    ASSERT_INT_EQ(firstPostingDocId(pl), 1, "banana doc_id wrong");

    pl = rbSearch(tree, "cherry");
    ASSERT_TRUE(pl != NULL, "cherry not found");
    ASSERT_INT_EQ(firstPostingDocId(pl), 3, "cherry doc_id wrong");

    freeRBTree(tree);
    return SUCCESS;
}

/* Одинаковые ключи должны добавляться в один список постингов */
static int test_insert_duplicate_key() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    rbInsert(tree, "key", 10, "t1");
    rbInsert(tree, "key", 20, "t2");

    ASSERT_INT_EQ(tree->size, 1, "size must be 1 for duplicate key");

    Vector* pl = rbSearch(tree, "key");
    ASSERT_TRUE(pl != NULL, "posting list must exist");
    ASSERT_INT_EQ((int)pl->size, 2, "posting list must contain two entries");

    PostingEntry* e0 = getVectorItem(pl, 0);
    PostingEntry* e1 = getVectorItem(pl, 1);
    ASSERT_TRUE(e0 != NULL && e1 != NULL, "entries must not be NULL");
    ASSERT_INT_EQ(e0->doc_id, 10, "first doc_id wrong");
    ASSERT_INT_EQ(e1->doc_id, 20, "second doc_id wrong");
    ASSERT_TRUE(strcmp(e0->title, "t1") == 0, "first title wrong");
    ASSERT_TRUE(strcmp(e1->title, "t2") == 0, "second title wrong");

    freeRBTree(tree);
    return SUCCESS;
}

/* Поиск несуществующего ключа должен вернуть NULL */
static int test_search_nonexistent() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    rbInsert(tree, "present", 1, "t");
    Vector* pl = rbSearch(tree, "absent");
    ASSERT_TRUE(pl == NULL, "search for absent key must return NULL");

    freeRBTree(tree);
    return SUCCESS;
}

/* При обходе дерева ключи должны выдаваться в отсортированном порядке */
static int test_traverse_order() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    /* вставляем в хаотичном порядке */
    rbInsert(tree, "zulu",   1, "");
    rbInsert(tree, "alpha",  2, "");
    rbInsert(tree, "mike",   3, "");
    rbInsert(tree, "bravo",  4, "");

    /* сбрасываем буфер и обходим дерево */
    memset(collected_keys, 0, sizeof(collected_keys));
    collect_index = 0;
    rbTraverse(tree, collector, NULL);

    /* проверяем что ключи вышли в алфавитном порядке */
    ASSERT_TRUE(strcmp(collected_keys, "alpha,bravo,mike,zulu") == 0,
                "traversal order must be sorted");

    freeRBTree(tree);
    return SUCCESS;
}

/* Корень дерева должен быть черным после каждой вставки
   Это свойство красно-черного дерева */
static int test_root_is_black() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    rbInsert(tree, "first", 1, "");
    ASSERT_TRUE(tree->root->color == RB_BLACK,
                "root must be black after first insert");

    rbInsert(tree, "second", 2, "");
    ASSERT_TRUE(tree->root->color == RB_BLACK,
                "root must remain black after second insert");

    rbInsert(tree, "third", 3, "");
    ASSERT_TRUE(tree->root->color == RB_BLACK,
                "root must remain black after third insert");

    freeRBTree(tree);
    return SUCCESS;
}

/* Проверяем что функции спокойно переносят NULL в разных местах
   и не валятся с ошибкой */
static int test_null_arguments() {
    RBTree* tree = createRBTree();
    ASSERT_TRUE(tree != NULL, "createRBTree returned NULL");

    /* пытаемся вставить с NULL ключом и NULL названием
       должны быть игнорированы */
    rbInsert(tree, NULL, 0, "title");
    rbInsert(tree, "valid", 1, NULL);

    /* ничего не должно было добавиться */
    ASSERT_TRUE(rbSearch(tree, NULL) == NULL, "search NULL must return NULL");
    ASSERT_TRUE(rbSearch(tree, "valid") == NULL, "insert with NULL title should not insert");

    /* теперь вставляем нормальный элемент */
    rbInsert(tree, "valid", 1, "title");
    Vector* pl = rbSearch(tree, "valid");
    ASSERT_TRUE(pl != NULL, "valid key should be found after proper insert");
    ASSERT_INT_EQ(firstPostingDocId(pl), 1, "valid doc_id mismatch");

    /* ищем с NULL ключом и на NULL дереве
       все должно вернуть NULL */
    ASSERT_TRUE(rbSearch(tree, NULL) == NULL, "search NULL must return NULL");
    ASSERT_TRUE(rbSearch(NULL, "valid") == NULL, "search on NULL tree must return NULL");

    /* обход NULL дерева и обход с NULL колбэком
       не должны упасть */
    rbTraverse(NULL, collector, NULL);
    rbTraverse(tree, NULL, NULL);

    freeRBTree(tree);
    return SUCCESS;
}

int main() {
    int failed_test_count = 0;

    failed_test_count += runTest("freeRBTree NULL safety",      test_free_null_safety);
    failed_test_count += runTest("createRBTree basic",          test_create_rbtree_basic);
    failed_test_count += runTest("insert single",               test_insert_single);
    failed_test_count += runTest("insert multiple distinct",    test_insert_multiple_distinct);
    failed_test_count += runTest("insert duplicate key",        test_insert_duplicate_key);
    failed_test_count += runTest("search non-existent",         test_search_nonexistent);
    failed_test_count += runTest("traverse order",              test_traverse_order);
    failed_test_count += runTest("root is black",               test_root_is_black);
    failed_test_count += runTest("null arguments handling",     test_null_arguments);

    printf("\n\n\nFAILED TEST COUNT: %d\n", failed_test_count);
    return 0;
}