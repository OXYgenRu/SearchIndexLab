#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCESS 0
#define FAILURE (-1)

#define MEMORY_ALLOCATION_ERROR \
    "ERROR: allocated memory is NULL\n"

#define TEST_HEADER "\n====================\n"

#define ASSERT_TRUE(condition, msg)                                      \
    do {                                                                 \
        if (!(condition)) {                                              \
            printf("FAIL: %s (line %d)\n", msg, __LINE__);               \
            return FAILURE;                                              \
        }                                                                \
    } while (0)

#define ASSERT_FALSE(condition, msg) \
    ASSERT_TRUE(!(condition), msg)

#define ASSERT_NOT_NULL(ptr, msg) \
    ASSERT_TRUE((ptr) != NULL, msg)

#define ASSERT_INT_EQ(actual, expected, msg)                             \
    do {                                                                 \
        if ((actual) != (expected)) {                                    \
            printf("FAIL: %s (line %d): expected %d, got %d\n",          \
                   msg, __LINE__, (expected), (actual));                 \
            return FAILURE;                                              \
        }                                                                \
    } while (0)

#define LARGE_INSERT_COUNT 30

static int runTest(const char* name, int (*fn)()) {
    printf(TEST_HEADER);
    printf("RUN: %s\n", name);

    int result = fn();

    if (result == SUCCESS) {
        printf("OK : %s\n", name);
    } else {
        printf("BAD: %s\n", name);
    }

    printf(TEST_HEADER);

    return result == SUCCESS ? 0 : 1;
}

static int test_create_and_free_basic() {
    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must be created");
    ASSERT_NOT_NULL(tree->root, "root must be created");

    ASSERT_INT_EQ(tree->size, 0, "size mismatch");
    ASSERT_INT_EQ(tree->root->n, 0, "root key count mismatch");

    ASSERT_TRUE(
            tree->root->is_leaf,
            "root must be leaf"
    );

    freeBTree(tree);

    return SUCCESS;
}

static int test_free_null_safety() {
    freeBTree(NULL);

    return SUCCESS;
}

static int test_insert_search_single_item() {
    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must be created");

    btreeInsert(tree, "apple", 101, "apple title");

    ASSERT_INT_EQ(tree->size, 1, "size mismatch");

    Vector* postings = btreeSearch(tree, "apple");

    ASSERT_NOT_NULL(postings, "postings must exist");

    ASSERT_INT_EQ(postings->size, 1, "posting size mismatch");

    PostingEntry* entry = getVectorItem(postings, 0);

    ASSERT_NOT_NULL(entry, "entry must exist");

    ASSERT_INT_EQ(entry->doc_id, 101, "doc_id mismatch");

    ASSERT_TRUE(
            strcmp(entry->title, "apple title") == 0,
            "title mismatch"
    );

    ASSERT_TRUE(
            btreeSearch(tree, "banana") == NULL,
            "missing key must return NULL"
    );

    freeBTree(tree);

    return SUCCESS;
}

static int test_insert_duplicate_key() {
    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must be created");

    btreeInsert(tree, "apple", 1, "first");
    btreeInsert(tree, "apple", 2, "second");

    ASSERT_INT_EQ(tree->size, 1, "size mismatch");

    Vector* postings = btreeSearch(tree, "apple");

    ASSERT_NOT_NULL(postings, "postings must exist");

    ASSERT_INT_EQ(postings->size, 2, "posting size mismatch");

    PostingEntry* first = getVectorItem(postings, 0);
    PostingEntry* second = getVectorItem(postings, 1);

    ASSERT_NOT_NULL(first, "first entry must exist");
    ASSERT_NOT_NULL(second, "second entry must exist");

    ASSERT_INT_EQ(first->doc_id, 1, "first doc_id mismatch");
    ASSERT_INT_EQ(second->doc_id, 2, "second doc_id mismatch");

    freeBTree(tree);

    return SUCCESS;
}

static int test_insert_cause_root_split() {
    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must be created");

    const char* keys[] = {
            "a",
            "b",
            "d",
            "e",
            "f",
            "c"
    };

    int key_count = 6;

    for (int i = 0; i < key_count; i++) {
        btreeInsert(tree, keys[i], i, "title");
    }

    ASSERT_FALSE(
            tree->root->is_leaf,
            "root must not be leaf"
    );

    ASSERT_INT_EQ(
            tree->root->n,
            1,
            "root key count mismatch"
    );

    ASSERT_TRUE(
            strcmp(tree->root->keys[0], "c") == 0,
            "median mismatch"
    );

    for (int i = 0; i < key_count; i++) {
        ASSERT_NOT_NULL(
                btreeSearch(tree, keys[i]),
                "all keys must exist"
        );
    }

    freeBTree(tree);

    return SUCCESS;
}

typedef struct {
    char* keys[64];
    int count;
} TraverseContext;

static void visitCollector(
        const char* key,
        Vector* postings,
        void* ctx
) {
    (void)postings;

    TraverseContext* context = (TraverseContext*)ctx;

    if (context == NULL || key == NULL) {
        return;
    }

    char* copy = strdup(key);

    if (copy == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return;
    }

    context->keys[context->count] = copy;
    context->count++;
}

static int test_traverse() {
    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must be created");

    const char* inserted[] = {
            "mango",
            "apple",
            "zebra",
            "grape",
            "banana"
    };

    const char* expected[] = {
            "apple",
            "banana",
            "grape",
            "mango",
            "zebra"
    };

    for (int i = 0; i < 5; i++) {
        btreeInsert(tree, inserted[i], i, "title");
    }

    TraverseContext context = {0};

    btreeTraverse(tree, visitCollector, &context);

    ASSERT_INT_EQ(context.count, 5, "count mismatch");

    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(
                strcmp(context.keys[i], expected[i]) == 0,
                "order mismatch"
        );

        free(context.keys[i]);
    }

    freeBTree(tree);

    return SUCCESS;
}

static int test_null_safety() {
    ASSERT_TRUE(
            btreeSearch(NULL, "abc") == NULL,
            "search NULL tree failed"
    );

    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must exist");

    ASSERT_TRUE(
            btreeSearch(tree, NULL) == NULL,
            "search NULL key failed"
    );

    btreeTraverse(NULL, visitCollector, NULL);
    btreeTraverse(tree, NULL, NULL);

    btreeInsert(NULL, "abc", 1, "title");
    btreeInsert(tree, NULL, 1, "title");
    btreeInsert(tree, "abc", 1, NULL);

    freeBTree(tree);

    return SUCCESS;
}

static int test_large_insertions() {
    BTree* tree = createBTree();

    ASSERT_NOT_NULL(tree, "tree must be created");

    char key[32];

    for (int i = 0; i < LARGE_INSERT_COUNT; i++) {
        snprintf(key, sizeof(key), "key_%03d", i);

        btreeInsert(tree, key, i, "title");
    }

    ASSERT_INT_EQ(
            tree->size,
            LARGE_INSERT_COUNT,
            "size mismatch"
    );

    for (int i = 0; i < LARGE_INSERT_COUNT; i++) {
        snprintf(key, sizeof(key), "key_%03d", i);

        Vector* postings = btreeSearch(tree, key);

        ASSERT_NOT_NULL(postings, "key missing");

        ASSERT_INT_EQ(
                postings->size,
                1,
                "posting size mismatch"
        );
    }

    freeBTree(tree);

    return SUCCESS;
}

int main() {
    int failed_test_count = 0;

    failed_test_count += runTest(
            "test_create_and_free_basic",
            test_create_and_free_basic
    );

    failed_test_count += runTest(
            "test_free_null_safety",
            test_free_null_safety
    );

    failed_test_count += runTest(
            "test_insert_search_single_item",
            test_insert_search_single_item
    );

    failed_test_count += runTest(
            "test_insert_duplicate_key",
            test_insert_duplicate_key
    );

    failed_test_count += runTest(
            "test_insert_cause_root_split",
            test_insert_cause_root_split
    );

    failed_test_count += runTest(
            "test_traverse",
            test_traverse
    );

    failed_test_count += runTest(
            "test_null_safety",
            test_null_safety
    );

    failed_test_count += runTest(
            "test_large_insertions",
            test_large_insertions
    );

    if (failed_test_count == 0) {
        printf("\n\nALL TESTS PASSED\n");
    } else {
        printf(
                "\n\nFAILED TEST COUNT: %d\n",
                failed_test_count
        );
    }

    return failed_test_count > 0 ? 1 : 0;
}