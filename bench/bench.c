#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "metrics.h"
#include "../index/index.h"
#include "../index/index_builder.h"
#include "../index/search.h"

#define SUCCESS 0
#define FAILURE (-1)
#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL\n"

#define TYPE_COUNT             3
#define LIMIT_COUNT            3
#define QUERY_WORD_COUNT       3
#define QUERIES_PER_BATCH      1000
#define MAX_KEYS_FOR_QUERIES   5000

static const int         g_limits[LIMIT_COUNT] = {50000, 200000, 500000};
static const TreeType    g_types[TYPE_COUNT]   = {TREE_AVL, TREE_RB, TREE_BTREE};
static const char *const g_type_names[TYPE_COUNT] = {"avl", "rb", "btree"};

typedef struct {
    char **items;
    int    count;
    int    capacity;
} StringList;

static int stringListInit(StringList *list, int capacity) {
    list->items = malloc(capacity * sizeof(char *));
    if (list->items == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }
    list->count = 0;
    list->capacity = capacity;
    return SUCCESS;
}

static int stringListAppendCopy(StringList *list, const char *src) {
    if (list->count >= list->capacity) {
        return SUCCESS;
    }
    char *copy = malloc(strlen(src) + 1);
    if (copy == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }
    strcpy(copy, src);
    list->items[list->count++] = copy;
    return SUCCESS;
}

static void stringListFree(StringList *list) {
    if (list->items == NULL) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void collectKeyVisit(const char *key, Vector *postings, void *ctx) {
    (void) postings;
    StringList *list = (StringList *) ctx;
    if (list->count >= list->capacity) {
        return;
    }
    stringListAppendCopy(list, key);
}

static int collectKeys(const Index *idx, StringList *keys) {
    if (stringListInit(keys, MAX_KEYS_FOR_QUERIES) == FAILURE) {
        return FAILURE;
    }
    traverseIndex(idx, collectKeyVisit, keys);
    return SUCCESS;
}

static int generateQueries(const StringList *keys, int n_words, int n_queries, StringList *out) {
    if (keys->count == 0) {
        return FAILURE;
    }
    if (stringListInit(out, n_queries) == FAILURE) {
        return FAILURE;
    }
    for (int i = 0; i < n_queries; i++) {
        size_t total = 1;
        int indices[QUERY_WORD_COUNT];
        for (int w = 0; w < n_words; w++) {
            int index = ((i * 7919) + (w * 6151)) % keys->count;
            if (index < 0) {
                index += keys->count;
            }
            indices[w] = index;
            total += strlen(keys->items[index]) + 1;
        }
        char *buffer = malloc(total);
        if (buffer == NULL) {
            printf(MEMORY_ALLOCATION_ERROR);
            stringListFree(out);
            return FAILURE;
        }
        buffer[0] = '\0';
        for (int w = 0; w < n_words; w++) {
            if (w > 0) {
                strcat(buffer, " ");
            }
            strcat(buffer, keys->items[indices[w]]);
        }
        out->items[out->count++] = buffer;
    }
    return SUCCESS;
}

static int readQueriesFromFile(const char *path, int max_queries, StringList *out) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return FAILURE;
    }
    if (stringListInit(out, max_queries) == FAILURE) {
        fclose(file);
        return FAILURE;
    }

    char buffer[4096];
    while (out->count < max_queries && fgets(buffer, sizeof(buffer), file)) {
        size_t length = strlen(buffer);
        while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            buffer[--length] = '\0';
        }
        if (length == 0) {
            continue;
        }
        if (stringListAppendCopy(out, buffer) == FAILURE) {
            fclose(file);
            return FAILURE;
        }
    }
    fclose(file);
    return SUCCESS;
}

static void ensureTmpDir(void) {
    struct stat st;
    if (stat("bench", &st) != 0) {
        MKDIR("bench");
    }
    if (stat("bench/tmp", &st) != 0) {
        MKDIR("bench/tmp");
    }
}

static void runIndexExperiment(const char *data_path) {
    for (int li = 0; li < LIMIT_COUNT; li++) {
        int limit = g_limits[li];
        for (int ti = 0; ti < TYPE_COUNT; ti++) {
            Index *idx = createIndex(g_types[ti]);
            if (idx == NULL) {
                fprintf(stderr, "failed to create index for %s\n", g_type_names[ti]);
                continue;
            }

            double t_start = getCurrentTimeMs();
            int indexed = buildIndexFromJsonl(idx, data_path, limit);
            double t_end = getCurrentTimeMs();

            long memory_kb = getPeakMemoryKb();

            char idx_path[512];
            snprintf(idx_path, sizeof(idx_path),
                     "bench/tmp/index_%s_%d.txt",
                     g_type_names[ti], limit);
            saveIndex(idx, idx_path);

            int reported = indexed >= 0 ? indexed : 0;
            printf("index,%s,%d,,%.3f,%ld,\n",
                   g_type_names[ti],
                   reported,
                   t_end - t_start,
                   memory_kb);
            fflush(stdout);

            freeIndex(idx);
        }
    }
}

/* Поднимаем индекс для search-эксперимента: пробуем сначала loadIndex
   с сохранённого файла (TZ 13), при неудаче перестраиваем из JSONL.
   *out_docs возвращает фактическое количество документов в индексе. */
static int loadOrBuildIndex(TreeType type, const char *type_name,
                            const char *data_path, int max_limit,
                            Index **out_idx, int *out_docs) {
    char idx_path[512];
    snprintf(idx_path, sizeof(idx_path),
             "bench/tmp/index_%s_%d.txt", type_name, max_limit);

    Index *idx = loadIndex(idx_path, type);
    if (idx != NULL) {
        *out_idx = idx;
        *out_docs = max_limit;
        return SUCCESS;
    }

    idx = createIndex(type);
    if (idx == NULL) {
        return FAILURE;
    }
    int indexed = buildIndexFromJsonl(idx, data_path, max_limit);
    if (indexed < 0) {
        freeIndex(idx);
        return FAILURE;
    }
    *out_idx = idx;
    *out_docs = indexed;
    return SUCCESS;
}

static void runSearchExperiment(const char *data_path, const char *queries_dir) {
    int max_limit = g_limits[LIMIT_COUNT - 1];

    for (int ti = 0; ti < TYPE_COUNT; ti++) {
        Index *idx = NULL;
        int docs = 0;
        if (loadOrBuildIndex(g_types[ti], g_type_names[ti],
                             data_path, max_limit, &idx, &docs) == FAILURE) {
            fprintf(stderr, "failed to load/build index for %s\n", g_type_names[ti]);
            continue;
        }

        StringList keys;
        int has_keys = (collectKeys(idx, &keys) == SUCCESS);

        for (int wi = 0; wi < QUERY_WORD_COUNT; wi++) {
            int n_words = wi + 1;

            StringList queries = {0};
            int loaded = 0;
            if (queries_dir != NULL) {
                char qpath[512];
                snprintf(qpath, sizeof(qpath),
                         "%s/queries_%d.txt", queries_dir, n_words);
                if (readQueriesFromFile(qpath, QUERIES_PER_BATCH, &queries) == SUCCESS
                    && queries.count > 0) {
                    loaded = 1;
                }
            }
            if (!loaded && has_keys) {
                if (generateQueries(&keys, n_words, QUERIES_PER_BATCH, &queries) == FAILURE) {
                    continue;
                }
            }
            if (queries.count == 0) {
                stringListFree(&queries);
                continue;
            }

            double total_ms = 0.0;
            int executed = 0;
            for (int q = 0; q < queries.count; q++) {
                double t_start = getCurrentTimeMs();
                SearchResults *sr = search(idx, queries.items[q]);
                double t_end = getCurrentTimeMs();
                total_ms += t_end - t_start;
                executed++;
                freeSearchResults(sr);
            }

            long memory_kb = getPeakMemoryKb();
            double avg = executed > 0 ? total_ms / (double) executed : 0.0;
            printf("search,%s,%d,%d,%.3f,%ld,%d\n",
                   g_type_names[ti],
                   docs,
                   n_words,
                   avg,
                   memory_kb,
                   executed);
            fflush(stdout);

            stringListFree(&queries);
        }

        if (has_keys) {
            stringListFree(&keys);
        }
        freeIndex(idx);
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --data=PATH [--queries=DIR]\n"
        "\n"
        "Options:\n"
        "  --data=PATH      JSONL-файл с документами (preprocess.py)\n"
        "  --queries=DIR    директория с queries_1.txt, queries_2.txt, queries_3.txt\n"
        "                   (необязательно — иначе запросы генерируются из индекса)\n",
        prog);
}

int main(int argc, char **argv) {
    const char *data_path = NULL;
    const char *queries_dir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--data=", 7) == 0) {
            data_path = argv[i] + 7;
        } else if (strncmp(argv[i], "--queries=", 10) == 0) {
            queries_dir = argv[i] + 10;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (data_path == NULL) {
        usage(argv[0]);
        return 1;
    }

    ensureTmpDir();

    printf("kind,type,docs,query_words,time_ms,memory_kb,total\n");
    fflush(stdout);

    runIndexExperiment(data_path);
    runSearchExperiment(data_path, queries_dir);

    return 0;
}
