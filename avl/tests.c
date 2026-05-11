#include "avl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCESS 0
#define FAILURE (-1)


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

#define ASSERT_STR_EQ(actual_value, expected_value, msg)                 \
    do {                                                                 \
        if (strcmp((actual_value), (expected_value)) != 0) {             \
            printf("FAIL: %s (line %d): expected \"%s\", got \"%s\"\n",  \
                   msg, __LINE__, (expected_value), (actual_value));     \
            return FAILURE;                                              \
        }                                                                \
    } while (0)

static int runTest(const char *name, int (*fn)()) {
    printf("RUN: %s\n", name);
    int result = fn();
    if (result == SUCCESS) {
        printf("OK : %s\n", name);
    } else {
        printf("BAD: %s\n", name);
    }
    return result;
}

#define TRAVERSE_MAX_KEYS 64

typedef struct {
    char keys[TRAVERSE_MAX_KEYS][MAX_TITLE_LEN];
    int  count;
} TraverseCtx;

static void collect_key(const char *key, Vector *postings, void *ctx) {
    TraverseCtx *tc = (TraverseCtx *) ctx;
    (void) postings;
    if (tc->count >= TRAVERSE_MAX_KEYS) {
        return;
    }
    strncpy(tc->keys[tc->count], key, MAX_TITLE_LEN - 1);
    tc->keys[tc->count][MAX_TITLE_LEN - 1] = '\0';
    tc->count++;
}

static int test_createAVLTree_basic() {
    AVLTree *tree = createAVLTree();

    ASSERT_TRUE(tree != NULL, "tree must be created");
    ASSERT_TRUE(tree->root == NULL, "new tree root must be NULL");
    ASSERT_INT_EQ(tree->size, 0, "new tree size must be 0");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlInsert_single() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "hello", 1, "Document One");

    ASSERT_INT_EQ(tree->size, 1, "size must be 1 after single insert");
    ASSERT_TRUE(tree->root != NULL, "root must not be NULL after insert");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlSearch_found() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "hello", 1, "Document One");

    Vector *postings = avlSearch(tree, "hello");
    ASSERT_TRUE(postings != NULL, "search must find inserted key");
    ASSERT_INT_EQ((int) postings->size, 1, "postings must have one entry");

    PostingEntry *entry = getVectorItem(postings, 0);
    ASSERT_TRUE(entry != NULL, "entry must not be NULL");
    ASSERT_INT_EQ(entry->doc_id, 1, "doc_id must match");
    ASSERT_STR_EQ(entry->title, "Document One", "title must match");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlSearch_not_found() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "hello", 1, "Document One");

    Vector *postings = avlSearch(tree, "world");
    ASSERT_TRUE(postings == NULL, "search for missing key must return NULL");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlSearch_empty_tree() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    Vector *postings = avlSearch(tree, "anything");
    ASSERT_TRUE(postings == NULL, "search in empty tree must return NULL");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlInsert_multiple_unique_keys() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "apple",      1, "Apple Doc");
    avlInsert(tree, "banana",     2, "Banana Doc");
    avlInsert(tree, "cherry",     3, "Cherry Doc");
    avlInsert(tree, "date",       4, "Date Doc");
    avlInsert(tree, "elderberry", 5, "Elderberry Doc");

    ASSERT_INT_EQ(tree->size, 5, "size must be 5 after five unique inserts");

    ASSERT_TRUE(avlSearch(tree, "apple")      != NULL, "apple must be found");
    ASSERT_TRUE(avlSearch(tree, "banana")     != NULL, "banana must be found");
    ASSERT_TRUE(avlSearch(tree, "cherry")     != NULL, "cherry must be found");
    ASSERT_TRUE(avlSearch(tree, "date")       != NULL, "date must be found");
    ASSERT_TRUE(avlSearch(tree, "elderberry") != NULL, "elderberry must be found");

    PostingEntry *apple_entry = getVectorItem(avlSearch(tree, "apple"), 0);
    ASSERT_TRUE(apple_entry != NULL, "apple entry must not be NULL");
    ASSERT_INT_EQ(apple_entry->doc_id, 1, "apple doc_id must match");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlInsert_duplicate_key_appends() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "word", 1, "First Doc");
    avlInsert(tree, "word", 2, "Second Doc");
    avlInsert(tree, "word", 3, "Third Doc");

    ASSERT_INT_EQ(tree->size, 1, "duplicate inserts must not increase size");

    Vector *postings = avlSearch(tree, "word");
    ASSERT_TRUE(postings != NULL, "postings must be found");
    ASSERT_INT_EQ((int) postings->size, 3, "postings must have three entries");

    PostingEntry *first  = getVectorItem(postings, 0);
    PostingEntry *second = getVectorItem(postings, 1);
    PostingEntry *third  = getVectorItem(postings, 2);

    ASSERT_TRUE(first  != NULL, "first entry must not be NULL");
    ASSERT_TRUE(second != NULL, "second entry must not be NULL");
    ASSERT_TRUE(third  != NULL, "third entry must not be NULL");
    ASSERT_INT_EQ(first->doc_id,  1, "first doc_id must match");
    ASSERT_INT_EQ(second->doc_id, 2, "second doc_id must match");
    ASSERT_INT_EQ(third->doc_id,  3, "third doc_id must match");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlSearch_returns_original_pointer() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "key", 1, "Doc");

    Vector *first_call  = avlSearch(tree, "key");
    Vector *second_call = avlSearch(tree, "key");

    ASSERT_TRUE(first_call  != NULL, "first search must not return NULL");
    ASSERT_TRUE(second_call != NULL, "second search must not return NULL");
    ASSERT_TRUE(first_call == second_call, "search must return same pointer on repeated calls");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlInsert_right_skewed_sequence() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "a", 1, "Doc A");
    avlInsert(tree, "b", 2, "Doc B");
    avlInsert(tree, "c", 3, "Doc C");
    avlInsert(tree, "d", 4, "Doc D");
    avlInsert(tree, "e", 5, "Doc E");

    ASSERT_INT_EQ(tree->size, 5, "size must be 5");

    ASSERT_TRUE(avlSearch(tree, "a") != NULL, "a must be found");
    ASSERT_TRUE(avlSearch(tree, "b") != NULL, "b must be found");
    ASSERT_TRUE(avlSearch(tree, "c") != NULL, "c must be found");
    ASSERT_TRUE(avlSearch(tree, "d") != NULL, "d must be found");
    ASSERT_TRUE(avlSearch(tree, "e") != NULL, "e must be found");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlInsert_left_skewed_sequence() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "e", 5, "Doc E");
    avlInsert(tree, "d", 4, "Doc D");
    avlInsert(tree, "c", 3, "Doc C");
    avlInsert(tree, "b", 2, "Doc B");
    avlInsert(tree, "a", 1, "Doc A");

    ASSERT_INT_EQ(tree->size, 5, "size must be 5");

    ASSERT_TRUE(avlSearch(tree, "a") != NULL, "a must be found");
    ASSERT_TRUE(avlSearch(tree, "c") != NULL, "c must be found");
    ASSERT_TRUE(avlSearch(tree, "e") != NULL, "e must be found");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlInsert_zigzag_sequence() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "c", 3, "Doc C");
    avlInsert(tree, "a", 1, "Doc A");
    avlInsert(tree, "b", 2, "Doc B");

    ASSERT_INT_EQ(tree->size, 3, "size must be 3");

    ASSERT_TRUE(avlSearch(tree, "a") != NULL, "a must be found");
    ASSERT_TRUE(avlSearch(tree, "b") != NULL, "b must be found");
    ASSERT_TRUE(avlSearch(tree, "c") != NULL, "c must be found");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlTraverse_visits_all_nodes() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "beta",  2, "Doc B");
    avlInsert(tree, "alpha", 1, "Doc A");
    avlInsert(tree, "gamma", 3, "Doc G");

    TraverseCtx ctx;
    ctx.count = 0;

    avlTraverse(tree, collect_key, &ctx);

    ASSERT_INT_EQ(ctx.count, 3, "traverse must visit all 3 nodes");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlTraverse_inorder() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, "delta",   4, "Doc D");
    avlInsert(tree, "alpha",   1, "Doc A");
    avlInsert(tree, "charlie", 3, "Doc C");
    avlInsert(tree, "bravo",   2, "Doc B");
    avlInsert(tree, "echo",    5, "Doc E");

    TraverseCtx ctx;
    ctx.count = 0;

    avlTraverse(tree, collect_key, &ctx);

    ASSERT_INT_EQ(ctx.count, 5, "traverse must visit all 5 nodes");

    const char *expected_order[] = { "alpha", "bravo", "charlie", "delta", "echo" };
    for (int i = 0; i < 5; i++) {
        ASSERT_STR_EQ(ctx.keys[i], expected_order[i], "in-order traverse must produce sorted key order");
    }

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_title_null_termination() {
    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    char long_title[512];
    memset(long_title, 'A', sizeof(long_title));
    long_title[sizeof(long_title) - 1] = '\0';

    avlInsert(tree, "key", 1, long_title);

    Vector *postings = avlSearch(tree, "key");
    ASSERT_TRUE(postings != NULL, "postings must be found");

    PostingEntry *entry = getVectorItem(postings, 0);
    ASSERT_TRUE(entry != NULL, "entry must not be NULL");
    ASSERT_TRUE(entry->title[MAX_TITLE_LEN - 1] == '\0', "title must be null-terminated");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_freeAVLTree_null_safety() {
    freeAVLTree(NULL);
    return SUCCESS;
}

static int test_avlInsert_null_guards() {
    avlInsert(NULL, "key", 1, "Doc");

    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlInsert(tree, NULL, 1, "Doc");
    ASSERT_INT_EQ(tree->size, 0, "insert with NULL key must not change size");

    avlInsert(tree, "key", 1, NULL);
    ASSERT_INT_EQ(tree->size, 0, "insert with NULL title must not change size");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlSearch_null_guards() {
    Vector *result = avlSearch(NULL, "key");
    ASSERT_TRUE(result == NULL, "search on NULL tree must return NULL");

    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    result = avlSearch(tree, NULL);
    ASSERT_TRUE(result == NULL, "search with NULL key must return NULL");

    freeAVLTree(tree);
    return SUCCESS;
}

static int test_avlTraverse_null_guards() {
    avlTraverse(NULL, collect_key, NULL);

    AVLTree *tree = createAVLTree();
    ASSERT_TRUE(tree != NULL, "tree must be created");

    avlTraverse(tree, NULL, NULL);

    freeAVLTree(tree);
    return SUCCESS;
}

int main() {
    int failed_test_count = 0;

    failed_test_count += runTest("createAVLTree basic", test_createAVLTree_basic);
    failed_test_count += runTest("avlInsert single",test_avlInsert_single);
    failed_test_count += runTest("avlSearch found", test_avlSearch_found);
    failed_test_count += runTest("avlSearch not found", test_avlSearch_not_found);
    failed_test_count += runTest("avlSearch empty tree", test_avlSearch_empty_tree);
    failed_test_count += runTest("avlInsert multiple unique keys", test_avlInsert_multiple_unique_keys);
    failed_test_count += runTest("avlInsert duplicate key appends", test_avlInsert_duplicate_key_appends);
    failed_test_count += runTest("avlSearch returns original pointer", test_avlSearch_returns_original_pointer);
    failed_test_count += runTest("avlInsert right-skewed sequence", test_avlInsert_right_skewed_sequence);
    failed_test_count += runTest("avlInsert left-skewed sequence", test_avlInsert_left_skewed_sequence);
    failed_test_count += runTest("avlInsert zigzag sequence", test_avlInsert_zigzag_sequence);
    failed_test_count += runTest("avlTraverse visits all nodes", test_avlTraverse_visits_all_nodes);
    failed_test_count += runTest("avlTraverse inorder", test_avlTraverse_inorder);
    failed_test_count += runTest("title null termination", test_title_null_termination);
    failed_test_count += runTest("freeAVLTree NULL safety", test_freeAVLTree_null_safety);
    failed_test_count += runTest("avlInsert NULL guards", test_avlInsert_null_guards);
    failed_test_count += runTest("avlSearch NULL guards", test_avlSearch_null_guards);
    failed_test_count += runTest("avlTraverse NULL guards", test_avlTraverse_null_guards);

    printf("\n\n\nFAILED TEST COUNT: %d\n", failed_test_count);
    return 0;
}