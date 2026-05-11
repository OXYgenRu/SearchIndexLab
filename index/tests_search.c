#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "index.h"
#include "search.h"
#include "../posting.h"

/* Заглушки для AVL/B-tree вынесены в index/avlbtree_stubs.c — общий файл
   с бенчмарком, чтобы не дублировать код. */

#define SUCCESS 0
#define FAILURE (-1)

#define TEST_HEADER "\n====================\n"
#define MAX_STUB_TITLE_LENGTH 512
#define DOC_COUNT_FOR_TOP_CAP 15
#define JSON_BUFFER_SIZE 1024


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


static int test_null_safety() {
    SearchResults *sr;

    sr = search(NULL, "anything");
    ASSERT_TRUE(sr != NULL, "search(NULL, q) should still return a SearchResults");
    ASSERT_INT_EQ(sr->total, 0, "total must be 0 when idx is NULL");
    freeSearchResults(sr);

    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    sr = search(idx, NULL);
    ASSERT_TRUE(sr != NULL, "search(idx, NULL) should still return a SearchResults");
    ASSERT_INT_EQ(sr->total, 0, "total must be 0 when query is NULL");
    freeSearchResults(sr);

    Vector *result = intersectPostings(NULL, 0);
    ASSERT_TRUE(result == NULL, "intersectPostings(NULL, 0) must return NULL");

    Vector *dummy = createVector(sizeof(PostingEntry));
    ASSERT_TRUE(dummy != NULL, "createVector returned NULL");
    Vector *lists[1] = {dummy};

    result = intersectPostings(lists, 0);
    ASSERT_TRUE(result == NULL, "intersectPostings with n=0 must return NULL");

    result = intersectPostings(lists, -1);
    ASSERT_TRUE(result == NULL, "intersectPostings with n<0 must return NULL");

    vectorFree(dummy);

    /* Эти три вызова не должны падать. */
    freeSearchResults(NULL);
    printResultsText(NULL);
    printResultsJSON(NULL);

    freeIndex(idx);
    return SUCCESS;
}


static int test_tokenizer_via_search() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    const char *tokens[] = {"foo", "bar", "baz"};
    indexDocument(idx, 1, "Doc1", tokens, 3);

    /* приведение к нижнему регистру + удаление не-word-символов: "FOO!!" -> "foo" */
    SearchResults *sr = search(idx, "FOO!!");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 1, "FOO!! should tokenize to foo and match doc 1");
    freeSearchResults(sr);

    /* фильтр len<3: "fo" отбрасывается -> токенов нет -> результатов нет */
    sr = search(idx, "fo");
    ASSERT_INT_EQ(sr->total, 0, "fo (len<3) should be filtered out");
    freeSearchResults(sr);

    /* запрос из одних пробелов -> токенов нет */
    sr = search(idx, "   ");
    ASSERT_INT_EQ(sr->total, 0, "whitespace-only query should yield 0 results");
    freeSearchResults(sr);

    /* пустой запрос */
    sr = search(idx, "");
    ASSERT_INT_EQ(sr->total, 0, "empty query should yield 0 results");
    freeSearchResults(sr);

    /* смешанный регистр + пунктуация в многословном запросе */
    sr = search(idx, "FOO, Bar!");
    ASSERT_INT_EQ(sr->total, 1, "FOO Bar should match doc 1 (lowercased AND)");
    freeSearchResults(sr);

    freeIndex(idx);
    return SUCCESS;
}


static int test_single_term_match() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    const char *tokens[] = {"python"};
    indexDocument(idx, 42, "Title 42", tokens, 1);

    SearchResults *sr = search(idx, "python");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 1, "expected 1 match");
    ASSERT_INT_EQ((int) sr->results->size, 1, "results->size should be 1");

    SearchResult *r = getVectorItem(sr->results, 0);
    ASSERT_TRUE(r != NULL, "result entry NULL");
    ASSERT_INT_EQ(r->doc_id, 42, "doc_id mismatch");
    ASSERT_TRUE(strcmp(r->title, "Title 42") == 0, "title mismatch");
    ASSERT_INT_EQ(r->score, 1, "score should be 1");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


static int test_two_term_and() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    const char *a_tokens[] = {"python", "list"};
    const char *b_tokens[] = {"python", "sort"};
    const char *c_tokens[] = {"list", "sort"};

    indexDocument(idx, 1, "DocA", a_tokens, 2);
    indexDocument(idx, 2, "DocB", b_tokens, 2);
    indexDocument(idx, 3, "DocC", c_tokens, 2);

    SearchResults *sr = search(idx, "python list");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 1, "expected exactly 1 intersected match");

    SearchResult *r = getVectorItem(sr->results, 0);
    ASSERT_TRUE(r != NULL, "result entry NULL");
    ASSERT_INT_EQ(r->doc_id, 1, "expected doc A (id=1)");
    ASSERT_INT_EQ(r->score, 2, "score = 1 (python) + 1 (list) = 2");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


static int test_term_not_in_index() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    const char *tokens[] = {"foo"};
    indexDocument(idx, 1, "Doc1", tokens, 1);

    SearchResults *sr = search(idx, "foo nonexistentterm");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 0, "expected 0 results when a query term is missing");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


static int test_tf_scoring() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    /* в документе D слово "python" встречается дважды, "list" один раз;
       в документе E каждое по одному разу. */
    const char *d_tokens[] = {"python", "python", "list"};
    const char *e_tokens[] = {"python", "list"};
    indexDocument(idx, 100, "DocD", d_tokens, 3);
    indexDocument(idx, 200, "DocE", e_tokens, 2);

    SearchResults *sr = search(idx, "python list");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 2, "expected 2 intersected results");

    SearchResult *first = getVectorItem(sr->results, 0);
    SearchResult *second = getVectorItem(sr->results, 1);
    ASSERT_TRUE(first != NULL, "first result NULL");
    ASSERT_TRUE(second != NULL, "second result NULL");

    ASSERT_INT_EQ(first->doc_id, 100, "doc D should rank first");
    ASSERT_INT_EQ(first->score, 3, "D score = 2 (python) + 1 (list) = 3");
    ASSERT_INT_EQ(second->doc_id, 200, "doc E should rank second");
    ASSERT_INT_EQ(second->score, 2, "E score = 1 + 1 = 2");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


static int test_top_10_cap() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    const char *tokens[] = {"hello"};
    for (int i = 1; i <= DOC_COUNT_FOR_TOP_CAP; i++) {
        char title[32];
        snprintf(title, sizeof(title), "Doc%d", i);
        indexDocument(idx, i, title, tokens, 1);
    }

    SearchResults *sr = search(idx, "hello");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, DOC_COUNT_FOR_TOP_CAP, "total should equal number of docs");
    ASSERT_INT_EQ((int) sr->results->size, 10, "results should be capped at 10");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


static int test_long_title_null_term() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    char long_title[MAX_STUB_TITLE_LENGTH];
    memset(long_title, 'A', sizeof(long_title) - 1);
    long_title[sizeof(long_title) - 1] = '\0';

    const char *tokens[] = {"longtitle"};
    indexDocument(idx, 7, long_title, tokens, 1);

    SearchResults *sr = search(idx, "longtitle");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 1, "expected 1 match");

    SearchResult *r = getVectorItem(sr->results, 0);
    ASSERT_TRUE(r != NULL, "result NULL");
    ASSERT_TRUE(r->title[MAX_TITLE_LEN - 1] == '\0',
                "SearchResult.title must be null-terminated at MAX_TITLE_LEN-1");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


static int test_json_escaping() {
    Index *idx = createIndex(TREE_RB);
    ASSERT_TRUE(idx != NULL, "createIndex returned NULL");

    const char *tokens[] = {"quoted"};
    indexDocument(idx, 1, "Hello \"world\"\n", tokens, 1);

    SearchResults *sr = search(idx, "quoted");
    ASSERT_TRUE(sr != NULL, "search returned NULL");
    ASSERT_INT_EQ(sr->total, 1, "expected 1 match");

    /* Перенаправляем stdout во временный файл, чтобы перехватить JSON-вывод. */
    fflush(stdout);
    int saved_stdout = dup(fileno(stdout));
    ASSERT_TRUE(saved_stdout >= 0, "dup stdout failed");

    FILE *tmp = tmpfile();
    ASSERT_TRUE(tmp != NULL, "tmpfile failed");

    if (dup2(fileno(tmp), fileno(stdout)) < 0) {
        close(saved_stdout);
        fclose(tmp);
        printf("FAIL: dup2 redirect failed\n");
        return FAILURE;
    }

    printResultsJSON(sr);

    fflush(stdout);
    dup2(saved_stdout, fileno(stdout));
    close(saved_stdout);

    rewind(tmp);
    char buffer[JSON_BUFFER_SIZE];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, tmp);
    buffer[bytes_read] = '\0';
    fclose(tmp);

    ASSERT_TRUE(strstr(buffer, "\\\"world\\\"") != NULL,
                "escaped quotes (\\\"world\\\") not found in JSON");
    ASSERT_TRUE(strstr(buffer, "\\n") != NULL,
                "escaped newline (\\n) not found in JSON");
    ASSERT_TRUE(strstr(buffer, "\"doc_id\":1") != NULL,
                "doc_id field missing or malformed");
    ASSERT_TRUE(strstr(buffer, "\"total\":1") != NULL,
                "total field missing or malformed");
    ASSERT_TRUE(strstr(buffer, "\"score\":1") != NULL,
                "score field missing or malformed");

    freeSearchResults(sr);
    freeIndex(idx);
    return SUCCESS;
}


int main() {
    int failed_test_count = 0;

    failed_test_count += runTest("null safety",                test_null_safety);
    failed_test_count += runTest("tokenizer via search",       test_tokenizer_via_search);
    failed_test_count += runTest("single term match",          test_single_term_match);
    failed_test_count += runTest("two-term AND",               test_two_term_and);
    failed_test_count += runTest("term not in index",          test_term_not_in_index);
    failed_test_count += runTest("TF scoring",                 test_tf_scoring);
    failed_test_count += runTest("top-10 cap",                 test_top_10_cap);
    failed_test_count += runTest("long title null-term",       test_long_title_null_term);
    failed_test_count += runTest("JSON escaping",              test_json_escaping);

    printf("\n\n\nFAILED TEST COUNT: %d\n", failed_test_count);
    return 0;
}
