#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/resource.h>

#include "../index/index.h"
#include "../index/search.h"
#include "../posting.h"
#include "../lab3/vector/generic.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"
#define VECTOR_ITEM_NULL_ERROR  "ERROR: vector returned NULL element\n"

#define DEFAULT_QUERIES        1000
#define DEFAULT_SEED           42
#define DEFAULT_DATA_PATH      "data/processed/docs.jsonl"
#define MAX_QUERY_LENGTH       3
#define INITIAL_TERMS_CAPACITY 1024
#define INITIAL_TOKEN_CAPACITY 32
#define TITLE_BUFFER_SIZE      1024
#define DOC_ID_BUFFER_SIZE     64
#define TOKEN_BUFFER_SIZE      128
#define QUERY_BUFFER_SIZE      1024

#define MS_PER_SEC 1000.0
#define NS_PER_MS  1000000.0

#define USAGE_TEXT                                                                \
    "Usage:\n"                                                                    \
    "  ./bench --type=<rb|avl|btree> [--data=PATH] [--docs=N]\n"                  \
    "          [--queries=N] [--seed=N] [--format=<csv|md>]\n"

/* ====================================================================
   JSONL parser — мини-машина для строк, которые выдаёт preprocess.py.
   Формат: {"doc_id": "123", "title": "...", "tokens": ["a","b",...]}
   ==================================================================== */

static const char *skipWhitespace(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

/* Ищет в строке шаблон "key":, возвращает указатель сразу после двоеточия
   (на следующий значимый символ). NULL если ключ не найден. */
static const char *findKey(const char *line, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(line, pattern);
    if (p == NULL) {
        return NULL;
    }
    p += strlen(pattern);
    p = skipWhitespace(p);
    if (*p != ':') {
        return NULL;
    }
    p++;
    return skipWhitespace(p);
}

/* Считывает JSON-строку из позиции p (должна указывать на открывающую "),
   раскрывая стандартные эскейпы. Пишет в out с null-терминатором. Возвращает
   указатель сразу после закрывающей ", либо NULL при синтаксической ошибке. */
static const char *readJsonString(const char *p, char *out, size_t cap) {
    if (p == NULL || out == NULL || cap == 0) {
        return NULL;
    }
    if (*p != '"') {
        return NULL;
    }
    p++;

    size_t i = 0;
    while (*p != '"') {
        if (*p == '\0') {
            return NULL;
        }
        char decoded;
        if (*p == '\\') {
            p++;
            if (*p == '\0') {
                return NULL;
            }
            switch (*p) {
                case '"':  decoded = '"';  break;
                case '\\': decoded = '\\'; break;
                case '/':  decoded = '/';  break;
                case 'b':  decoded = '\b'; break;
                case 'f':  decoded = '\f'; break;
                case 'n':  decoded = '\n'; break;
                case 'r':  decoded = '\r'; break;
                case 't':  decoded = '\t'; break;
                case 'u':
                    /* Юникод-эскейп: 4 hex-цифры. Для бенча содержимое не важно,
                       заменяем плейсхолдером и шагаем через 4 цифры. */
                    if (p[1] == '\0' || p[2] == '\0' || p[3] == '\0' || p[4] == '\0') {
                        return NULL;
                    }
                    p += 4;
                    decoded = '?';
                    break;
                default:
                    decoded = *p;
                    break;
            }
        } else {
            decoded = *p;
        }
        if (i + 1 < cap) {
            out[i++] = decoded;
        }
        p++;
    }
    out[i] = '\0';
    return p + 1;
}

typedef struct {
    int doc_id;
    char title[TITLE_BUFFER_SIZE];
    char **tokens;
    int n_tokens;
    int tokens_capacity;
} ParsedDoc;

static void resetParsedDoc(ParsedDoc *doc) {
    if (doc == NULL) {
        return;
    }
    if (doc->tokens != NULL) {
        for (int i = 0; i < doc->n_tokens; i++) {
            free(doc->tokens[i]);
        }
    }
    free(doc->tokens);
    doc->doc_id = 0;
    doc->title[0] = '\0';
    doc->tokens = NULL;
    doc->n_tokens = 0;
    doc->tokens_capacity = 0;
}

static int appendDocToken(ParsedDoc *doc, const char *token) {
    if (doc == NULL || token == NULL) {
        return FAILURE;
    }
    if (doc->n_tokens == doc->tokens_capacity) {
        int new_cap = doc->tokens_capacity == 0 ? INITIAL_TOKEN_CAPACITY : doc->tokens_capacity * 2;
        char **new_tokens = realloc(doc->tokens, sizeof(char *) * (size_t) new_cap);
        if (new_tokens == NULL) {
            printf("%s", MEMORY_ALLOCATION_ERROR);
            return FAILURE;
        }
        doc->tokens = new_tokens;
        doc->tokens_capacity = new_cap;
    }
    size_t len = strlen(token);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }
    memcpy(copy, token, len + 1);
    doc->tokens[doc->n_tokens] = copy;
    doc->n_tokens++;
    return SUCCESS;
}

static int parseDocLine(const char *line, ParsedDoc *out) {
    if (line == NULL || out == NULL) {
        return FAILURE;
    }
    resetParsedDoc(out);

    char doc_id_buf[DOC_ID_BUFFER_SIZE];
    const char *p = findKey(line, "doc_id");
    if (p == NULL) {
        return FAILURE;
    }
    p = readJsonString(p, doc_id_buf, sizeof(doc_id_buf));
    if (p == NULL) {
        return FAILURE;
    }
    out->doc_id = (int) strtol(doc_id_buf, NULL, 10);

    p = findKey(line, "title");
    if (p == NULL) {
        return FAILURE;
    }
    p = readJsonString(p, out->title, sizeof(out->title));
    if (p == NULL) {
        return FAILURE;
    }

    p = findKey(line, "tokens");
    if (p == NULL) {
        return FAILURE;
    }
    if (*p != '[') {
        return FAILURE;
    }
    p++;
    p = skipWhitespace(p);

    while (*p != ']' && *p != '\0') {
        char token_buf[TOKEN_BUFFER_SIZE];
        p = readJsonString(p, token_buf, sizeof(token_buf));
        if (p == NULL) {
            return FAILURE;
        }
        if (appendDocToken(out, token_buf) != SUCCESS) {
            return FAILURE;
        }
        p = skipWhitespace(p);
        if (*p == ',') {
            p++;
            p = skipWhitespace(p);
        }
    }
    if (*p != ']') {
        return FAILURE;
    }
    return SUCCESS;
}

/* ====================================================================
   Сбор всех термов из индекса для построения случайных запросов.
   ==================================================================== */

typedef struct {
    Vector *terms;     /* Vector<const char*>: указатели на key-строки в дереве */
    int has_error;
} TermCollectorContext;

static void collectTerm(const char *key, Vector *postings, void *ctx) {
    (void) postings;
    TermCollectorContext *tc = (TermCollectorContext *) ctx;
    if (tc == NULL || tc->has_error) {
        return;
    }
    if (key == NULL) {
        return;
    }
    /* Сохраняем именно указатель на ключ из дерева: дерево живёт всё время,
       пока крутятся запросы, так что копии не нужны. */
    const char *key_ptr = key;
    if (appendVectorItem(tc->terms, &key_ptr) != SUCCESS) {
        tc->has_error = 1;
    }
}

/* ====================================================================
   Замеры и форматирование.
   ==================================================================== */

static double elapsedMs(struct timespec t0, struct timespec t1) {
    double sec_diff = (double) (t1.tv_sec - t0.tv_sec);
    double nsec_diff = (double) (t1.tv_nsec - t0.tv_nsec);
    return sec_diff * MS_PER_SEC + nsec_diff / NS_PER_MS;
}

static long peakResidentKB(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
#ifdef __APPLE__
    /* На macOS ru_maxrss возвращается в байтах. */
    return usage.ru_maxrss / 1024;
#else
    /* На Linux — уже в килобайтах. */
    return usage.ru_maxrss;
#endif
}

static int buildRandomQuery(const Vector *terms, int n_words,
                            unsigned int *seed, char *out, size_t cap) {
    if (terms == NULL || out == NULL || cap == 0 || seed == NULL) {
        return FAILURE;
    }
    if (terms->size == 0) {
        return FAILURE;
    }

    size_t pos = 0;
    out[0] = '\0';

    for (int i = 0; i < n_words; i++) {
        int idx = (int) ((unsigned) rand_r(seed) % terms->size);
        const char **slot = getVectorItem((Vector *) terms, (size_t) idx);
        if (slot == NULL || *slot == NULL) {
            return FAILURE;
        }
        const char *term = *slot;
        size_t term_len = strlen(term);
        size_t need = term_len + (pos > 0 ? 1 : 0);
        if (pos + need + 1 >= cap) {
            break;
        }
        if (pos > 0) {
            out[pos] = ' ';
            pos++;
        }
        memcpy(out + pos, term, term_len);
        pos += term_len;
        out[pos] = '\0';
    }
    return SUCCESS;
}

/* ====================================================================
   CLI и точка входа.
   ==================================================================== */

typedef struct {
    const char *type_str;
    const char *data_path;
    int doc_limit;
    int n_queries;
    unsigned int seed;
    const char *format;
} BenchArgs;

static int parseArgs(int argc, char *argv[], BenchArgs *args) {
    if (args == NULL) {
        return FAILURE;
    }
    args->type_str = NULL;
    args->data_path = DEFAULT_DATA_PATH;
    args->doc_limit = 0;
    args->n_queries = DEFAULT_QUERIES;
    args->seed = DEFAULT_SEED;
    args->format = "csv";

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--type=", 7) == 0) {
            args->type_str = argv[i] + 7;
        } else if (strncmp(argv[i], "--data=", 7) == 0) {
            args->data_path = argv[i] + 7;
        } else if (strncmp(argv[i], "--docs=", 7) == 0) {
            args->doc_limit = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--queries=", 10) == 0) {
            args->n_queries = atoi(argv[i] + 10);
        } else if (strncmp(argv[i], "--seed=", 7) == 0) {
            args->seed = (unsigned int) atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--format=", 9) == 0) {
            args->format = argv[i] + 9;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return FAILURE;
        }
    }

    if (args->type_str == NULL) {
        fprintf(stderr, "missing --type=\n");
        return FAILURE;
    }
    return SUCCESS;
}

static void printResultsCSV(const char *type_str, int parsed_docs,
                            double indexing_ms, long rss_kb,
                            const double *search_avg_ms) {
    printf("metric,backend,docs,n_words,value\n");
    printf("indexing_time_ms,%s,%d,,%.3f\n", type_str, parsed_docs, indexing_ms);
    printf("peak_rss_kb,%s,%d,,%ld\n", type_str, parsed_docs, rss_kb);
    for (int k = 1; k <= MAX_QUERY_LENGTH; k++) {
        printf("search_avg_ms,%s,%d,%d,%.6f\n",
               type_str, parsed_docs, k, search_avg_ms[k - 1]);
    }
}

static void printResultsMarkdown(const char *type_str, int parsed_docs,
                                 double indexing_ms, long rss_kb,
                                 const double *search_avg_ms) {
    printf("| Метрика | Бэкенд | Документов | Слов | Значение |\n");
    printf("|---|---|---|---|---|\n");
    printf("| Индексация (мс) | %s | %d | — | %.3f |\n",
           type_str, parsed_docs, indexing_ms);
    printf("| Пиковая память (КБ) | %s | %d | — | %ld |\n",
           type_str, parsed_docs, rss_kb);
    for (int k = 1; k <= MAX_QUERY_LENGTH; k++) {
        printf("| Поиск (мс/запрос) | %s | %d | %d | %.6f |\n",
               type_str, parsed_docs, k, search_avg_ms[k - 1]);
    }
}

int main(int argc, char *argv[]) {
    BenchArgs args;
    if (parseArgs(argc, argv, &args) != SUCCESS) {
        fprintf(stderr, "%s", USAGE_TEXT);
        return 1;
    }

    /* Грейсфул-скип для бэкендов, которых ещё нет на main. */
    if (strcmp(args.type_str, "avl") == 0 || strcmp(args.type_str, "btree") == 0) {
        fprintf(stderr, "backend %s not implemented yet on this branch\n", args.type_str);
        return 0;
    }
    if (strcmp(args.type_str, "rb") != 0) {
        fprintf(stderr, "unknown backend: %s\n", args.type_str);
        return 1;
    }

    FILE *f = fopen(args.data_path, "r");
    if (f == NULL) {
        fprintf(stderr, "cannot open data file: %s (%s)\n",
                args.data_path, strerror(errno));
        return 1;
    }

    Index *idx = createIndex(TREE_RB);
    if (idx == NULL) {
        fprintf(stderr, "createIndex failed\n");
        fclose(f);
        return 1;
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len = 0;
    int parsed_docs = 0;
    int skipped_lines = 0;

    ParsedDoc doc = {0};

    /* === Замер 3.1: время индексации === */
    struct timespec t_index_start;
    struct timespec t_index_end;
    clock_gettime(CLOCK_MONOTONIC, &t_index_start);

    while ((line_len = getline(&line, &line_cap, f)) != -1) {
        if (args.doc_limit > 0 && parsed_docs >= args.doc_limit) {
            break;
        }
        if (line_len <= 1) {
            continue;
        }
        if (parseDocLine(line, &doc) != SUCCESS) {
            skipped_lines++;
            resetParsedDoc(&doc);
            continue;
        }
        indexDocument(idx, doc.doc_id, doc.title,
                      (const char **) doc.tokens, doc.n_tokens);
        parsed_docs++;
        resetParsedDoc(&doc);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_index_end);
    double indexing_ms = elapsedMs(t_index_start, t_index_end);

    free(line);
    resetParsedDoc(&doc);
    fclose(f);

    if (parsed_docs == 0) {
        fprintf(stderr, "no documents parsed\n");
        freeIndex(idx);
        return 1;
    }

    /* === Замер 3.3: пиковое потребление памяти === */
    long rss_kb = peakResidentKB();

    /* === Сбор термов для генерации запросов === */
    TermCollectorContext tc;
    tc.terms = createVector(sizeof(const char *));
    tc.has_error = 0;
    if (tc.terms == NULL) {
        printf("%s", MEMORY_ALLOCATION_ERROR);
        freeIndex(idx);
        return 1;
    }
    traverseIndex(idx, collectTerm, &tc);
    if (tc.has_error != 0 || tc.terms->size == 0) {
        fprintf(stderr, "term collection failed (size=%zu)\n", tc.terms->size);
        vectorFree(tc.terms);
        freeIndex(idx);
        return 1;
    }

    /* === Замер 3.2: среднее время поиска по длинам 1/2/3 слова === */
    double search_avg_ms[MAX_QUERY_LENGTH] = {0};

    for (int k = 1; k <= MAX_QUERY_LENGTH; k++) {
        unsigned int seed = args.seed + (unsigned int) k;
        struct timespec t_search_start;
        struct timespec t_search_end;

        clock_gettime(CLOCK_MONOTONIC, &t_search_start);
        for (int q = 0; q < args.n_queries; q++) {
            char query[QUERY_BUFFER_SIZE];
            if (buildRandomQuery(tc.terms, k, &seed, query, sizeof(query)) != SUCCESS) {
                continue;
            }
            SearchResults *sr = search(idx, query);
            freeSearchResults(sr);
        }
        clock_gettime(CLOCK_MONOTONIC, &t_search_end);

        double total_ms = elapsedMs(t_search_start, t_search_end);
        search_avg_ms[k - 1] = total_ms / (double) args.n_queries;
    }

    /* === Вывод === */
    if (strcmp(args.format, "md") == 0) {
        printResultsMarkdown(args.type_str, parsed_docs, indexing_ms,
                             rss_kb, search_avg_ms);
    } else {
        printResultsCSV(args.type_str, parsed_docs, indexing_ms,
                        rss_kb, search_avg_ms);
    }

    if (skipped_lines > 0) {
        fprintf(stderr, "skipped %d malformed lines\n", skipped_lines);
    }

    vectorFree(tc.terms);
    freeIndex(idx);
    return 0;
}
