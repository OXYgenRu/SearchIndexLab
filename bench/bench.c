#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
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
#define PATH_BUFFER_SIZE       512
#define COMMAND_BUFFER_SIZE    4096

static const int         g_limits[LIMIT_COUNT] = {50000, 100000, 150000};
static const TreeType    g_types[TYPE_COUNT]   = {TREE_AVL, TREE_RB, TREE_BTREE};
static const char *const g_type_names[TYPE_COUNT] = {"avl", "rb", "btree"};

typedef struct {
    char **items;
    int    count;
    int    capacity;
} StringList;

static int stringListInit(StringList *list, int capacity) {
    if (list == NULL || capacity <= 0) {
        return FAILURE;
    }

    list->items = malloc((size_t) capacity * sizeof(char *));
    if (list->items == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }

    list->count = 0;
    list->capacity = capacity;
    return SUCCESS;
}

static int stringListAppendCopy(StringList *list, const char *src) {
    char *copy;

    if (list == NULL || src == NULL) {
        return FAILURE;
    }

    if (list->count >= list->capacity) {
        return SUCCESS;
    }

    copy = malloc(strlen(src) + 1);
    if (copy == NULL) {
        printf(MEMORY_ALLOCATION_ERROR);
        return FAILURE;
    }

    strcpy(copy, src);
    list->items[list->count++] = copy;
    return SUCCESS;
}

static void stringListFree(StringList *list) {
    if (list == NULL || list->items == NULL) {
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
    StringList *list = (StringList *) ctx;

    (void) postings;

    if (list == NULL || list->count >= list->capacity) {
        return;
    }

    stringListAppendCopy(list, key);
}

static int collectKeys(const Index *idx, StringList *keys) {
    if (idx == NULL || keys == NULL) {
        return FAILURE;
    }

    if (stringListInit(keys, MAX_KEYS_FOR_QUERIES) == FAILURE) {
        return FAILURE;
    }

    traverseIndex(idx, collectKeyVisit, keys);
    return keys->count > 0 ? SUCCESS : FAILURE;
}

static int generateQueries(const StringList *keys, int n_words, int n_queries, StringList *out) {
    if (keys == NULL || out == NULL || keys->count == 0) {
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

static int ensureDir(const char *path) {
    struct stat st;

    if (path == NULL) {
        return FAILURE;
    }

    if (stat(path, &st) == 0) {
        return SUCCESS;
    }

    return MKDIR(path) == 0 ? SUCCESS : FAILURE;
}

static int ensureBenchDirs(void) {
    if (ensureDir("bench") == FAILURE) {
        return FAILURE;
    }

    if (ensureDir("bench/tmp") == FAILURE) {
        return FAILURE;
    }

    if (ensureDir("bench/queries") == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}

static int makeQueryDirPath(char *buffer, size_t buffer_size, const char *queries_root, int limit) {
    if (buffer == NULL || queries_root == NULL) {
        return FAILURE;
    }

    if (snprintf(buffer, buffer_size, "%s/%d", queries_root, limit) >= (int) buffer_size) {
        return FAILURE;
    }

    return SUCCESS;
}

static int makeQueryPath(char *buffer, size_t buffer_size, const char *queries_dir, int n_words) {
    if (buffer == NULL || queries_dir == NULL) {
        return FAILURE;
    }

    if (snprintf(buffer, buffer_size, "%s/queries_%d.txt", queries_dir, n_words) >= (int) buffer_size) {
        return FAILURE;
    }

    return SUCCESS;
}

static int makeIndexPath(char *buffer, size_t buffer_size, const char *type_name, int limit) {
    if (buffer == NULL || type_name == NULL) {
        return FAILURE;
    }

    if (snprintf(buffer, buffer_size, "bench/tmp/index_%s_%d.txt", type_name, limit) >= (int) buffer_size) {
        return FAILURE;
    }

    return SUCCESS;
}

static long long fileSizeBytes(const char *path) {
    struct stat st;

    if (path == NULL) {
        return 0;
    }

    if (stat(path, &st) != 0) {
        return 0;
    }

    return (long long) st.st_size;
}

static int typeFromName(const char *type_name, TreeType *type) {
    if (type_name == NULL || type == NULL) {
        return FAILURE;
    }

    for (int i = 0; i < TYPE_COUNT; i++) {
        if (strcmp(type_name, g_type_names[i]) == 0) {
            *type = g_types[i];
            return SUCCESS;
        }
    }

    return FAILURE;
}

static const char *nameFromType(TreeType type) {
    for (int i = 0; i < TYPE_COUNT; i++) {
        if (g_types[i] == type) {
            return g_type_names[i];
        }
    }

    return "unknown";
}

static Index *buildIndexForLimit(TreeType type, const char *data_path, int limit,
                                 double *build_ms, int *docs) {
    Index *idx;
    double t_start;
    double t_end;
    int result;

    if (data_path == NULL || limit <= 0) {
        return NULL;
    }

    idx = createIndex(type);
    if (idx == NULL) {
        return NULL;
    }

    t_start = getCurrentTimeMs();
    result = buildIndexFromJsonl(idx, data_path, limit);
    t_end = getCurrentTimeMs();

    if (build_ms != NULL) {
        *build_ms = t_end - t_start;
    }

    if (docs != NULL) {
        *docs = result;
    }

    if (result == FAILURE) {
        freeIndex(idx);
        return NULL;
    }

    return idx;
}

static int readQueriesFromFile(const char *path, int max_queries, StringList *out) {
    FILE *file;
    char buffer[4096];

    if (path == NULL || out == NULL) {
        return FAILURE;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return FAILURE;
    }

    if (stringListInit(out, max_queries) == FAILURE) {
        fclose(file);
        return FAILURE;
    }

    while (out->count < max_queries && fgets(buffer, sizeof(buffer), file)) {
        size_t length = strlen(buffer);

        while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            buffer[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        if (stringListAppendCopy(out, buffer) == FAILURE) {
            stringListFree(out);
            fclose(file);
            return FAILURE;
        }
    }

    fclose(file);
    return out->count > 0 ? SUCCESS : FAILURE;
}

static int writeQueriesToFile(const char *path, const StringList *queries) {
    FILE *file;

    if (path == NULL || queries == NULL) {
        return FAILURE;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return FAILURE;
    }

    for (int i = 0; i < queries->count; i++) {
        if (fprintf(file, "%s\n", queries->items[i]) < 0) {
            fclose(file);
            return FAILURE;
        }
    }

    return fclose(file) == 0 ? SUCCESS : FAILURE;
}

static void printCsvHeader(void) {
    printf("phase,type,limit,docs,query_words,time_ms,time_us,peak_memory_kb,index_bytes,total,status\n");
    fflush(stdout);
}

static void printCsvRow(const char *phase, const char *type_name, int limit, int docs,
                        int query_words, double time_ms, long memory_kb,
                        long long index_bytes, int total, const char *status) {
    printf("%s,%s,%d,%d,%d,%.6f,%.3f,%ld,%lld,%d,%s\n",
           phase,
           type_name,
           limit,
           docs,
           query_words,
           time_ms,
           time_ms * 1000.0,
           memory_kb,
           index_bytes,
           total,
           status);
    fflush(stdout);
}

static int runSingleIndex(TreeType type, const char *data_path, int limit) {
    const char *type_name = nameFromType(type);
    char idx_path[PATH_BUFFER_SIZE];
    double build_ms = 0.0;
    int docs = 0;
    long memory_kb;
    long long index_bytes;
    Index *idx;

    if (ensureBenchDirs() == FAILURE) {
        return FAILURE;
    }

    idx = buildIndexForLimit(type, data_path, limit, &build_ms, &docs);
    if (idx == NULL) {
        printCsvRow("index", type_name, limit, 0, 0, 0.0, getPeakMemoryKb(), 0, 0, "build_failed");
        return FAILURE;
    }

    memory_kb = getPeakMemoryKb();

    if (makeIndexPath(idx_path, sizeof(idx_path), type_name, limit) == FAILURE) {
        freeIndex(idx);
        return FAILURE;
    }

    saveIndex(idx, idx_path);
    index_bytes = fileSizeBytes(idx_path);

    printCsvRow("index", type_name, limit, docs, 0, build_ms, memory_kb, index_bytes, 0, "ok");
    freeIndex(idx);
    return SUCCESS;
}

static int runSingleQueries(TreeType type, const char *data_path, int limit,
                            const char *queries_dir) {
    const char *type_name = nameFromType(type);
    double build_ms = 0.0;
    int docs = 0;
    long memory_kb;
    Index *idx;
    StringList keys = {0};

    if (queries_dir == NULL) {
        return FAILURE;
    }

    if (ensureBenchDirs() == FAILURE || ensureDir(queries_dir) == FAILURE) {
        return FAILURE;
    }

    idx = buildIndexForLimit(type, data_path, limit, &build_ms, &docs);
    if (idx == NULL) {
        printCsvRow("queries", type_name, limit, 0, 0, 0.0, getPeakMemoryKb(), 0, 0, "build_failed");
        return FAILURE;
    }

    if (collectKeys(idx, &keys) == FAILURE) {
        freeIndex(idx);
        printCsvRow("queries", type_name, limit, docs, 0, build_ms, getPeakMemoryKb(), 0, 0, "no_keys");
        return FAILURE;
    }

    for (int wi = 0; wi < QUERY_WORD_COUNT; wi++) {
        int n_words = wi + 1;
        char qpath[PATH_BUFFER_SIZE];
        StringList queries = {0};

        if (makeQueryPath(qpath, sizeof(qpath), queries_dir, n_words) == FAILURE ||
            generateQueries(&keys, n_words, QUERIES_PER_BATCH, &queries) == FAILURE ||
            writeQueriesToFile(qpath, &queries) == FAILURE) {
            stringListFree(&queries);
            stringListFree(&keys);
            freeIndex(idx);
            printCsvRow("queries", type_name, limit, docs, n_words, build_ms,
                        getPeakMemoryKb(), 0, 0, "write_failed");
            return FAILURE;
        }

        memory_kb = getPeakMemoryKb();
        printCsvRow("queries", type_name, limit, docs, n_words, build_ms,
                    memory_kb, 0, queries.count, "ok");
        stringListFree(&queries);
    }

    stringListFree(&keys);
    freeIndex(idx);
    return SUCCESS;
}

static int loadOrGenerateQueries(Index *idx, const char *queries_dir, int n_words,
                                 StringList *queries) {
    char qpath[PATH_BUFFER_SIZE];

    if (queries == NULL) {
        return FAILURE;
    }

    if (queries_dir != NULL &&
        makeQueryPath(qpath, sizeof(qpath), queries_dir, n_words) == SUCCESS &&
        readQueriesFromFile(qpath, QUERIES_PER_BATCH, queries) == SUCCESS) {
        return SUCCESS;
    }

    StringList keys = {0};
    if (collectKeys(idx, &keys) == FAILURE) {
        return FAILURE;
    }

    int result = generateQueries(&keys, n_words, QUERIES_PER_BATCH, queries);
    stringListFree(&keys);
    return result;
}

static int runSingleSearch(TreeType type, const char *data_path, int limit,
                           const char *queries_dir) {
    const char *type_name = nameFromType(type);
    double build_ms = 0.0;
    int docs = 0;
    Index *idx;

    if (ensureBenchDirs() == FAILURE) {
        return FAILURE;
    }

    idx = buildIndexForLimit(type, data_path, limit, &build_ms, &docs);
    if (idx == NULL) {
        printCsvRow("search", type_name, limit, 0, 0, 0.0, getPeakMemoryKb(), 0, 0, "build_failed");
        return FAILURE;
    }

    for (int wi = 0; wi < QUERY_WORD_COUNT; wi++) {
        int n_words = wi + 1;
        StringList queries = {0};
        double total_ms = 0.0;
        int executed = 0;

        if (loadOrGenerateQueries(idx, queries_dir, n_words, &queries) == FAILURE) {
            printCsvRow("search", type_name, limit, docs, n_words, 0.0,
                        getPeakMemoryKb(), 0, 0, "queries_failed");
            continue;
        }

        for (int q = 0; q < queries.count; q++) {
            double t_start = getCurrentTimeMs();
            SearchResults *sr = search(idx, queries.items[q]);
            double t_end = getCurrentTimeMs();

            if (sr == NULL) {
                continue;
            }

            total_ms += t_end - t_start;
            executed++;
            freeSearchResults(sr);
        }

        double avg_ms = executed > 0 ? total_ms / (double) executed : 0.0;
        printCsvRow("search", type_name, limit, docs, n_words, avg_ms,
                    getPeakMemoryKb(), 0, executed, executed > 0 ? "ok" : "search_failed");

        stringListFree(&queries);
    }

    freeIndex(idx);
    (void) build_ms;
    return SUCCESS;
}

#ifndef _WIN32
static int runCommand(const char *command) {
    int result;

    if (command == NULL) {
        return FAILURE;
    }

    result = system(command);
    return result == 0 ? SUCCESS : FAILURE;
}
#endif

static int runChild(const char *exe_path, const char *phase, const char *type_name,
                    int limit, const char *data_path, const char *queries_dir) {
#ifdef _WIN32
    char phase_arg[64];
    char type_arg[64];
    char limit_arg[64];
    char data_arg[PATH_BUFFER_SIZE];
    char queries_arg[PATH_BUFFER_SIZE];
    const char *args[9];
    int argc = 0;
    intptr_t result;

    if (snprintf(phase_arg, sizeof(phase_arg), "--phase=%s", phase) >= (int) sizeof(phase_arg) ||
        snprintf(type_arg, sizeof(type_arg), "--type=%s", type_name) >= (int) sizeof(type_arg) ||
        snprintf(limit_arg, sizeof(limit_arg), "--limit=%d", limit) >= (int) sizeof(limit_arg) ||
        snprintf(data_arg, sizeof(data_arg), "--data=%s", data_path) >= (int) sizeof(data_arg)) {
        return FAILURE;
    }

    args[argc++] = exe_path;
    args[argc++] = "--single";
    args[argc++] = phase_arg;
    args[argc++] = type_arg;
    args[argc++] = limit_arg;
    args[argc++] = data_arg;

    if (queries_dir != NULL) {
        if (snprintf(queries_arg, sizeof(queries_arg), "--queries=%s", queries_dir) >= (int) sizeof(queries_arg)) {
            return FAILURE;
        }
        args[argc++] = queries_arg;
    }

    args[argc] = NULL;
    result = _spawnvp(_P_WAIT, exe_path, args);
    return result == 0 ? SUCCESS : FAILURE;
#else
    char command[COMMAND_BUFFER_SIZE];
    int written;

    if (queries_dir != NULL) {
        written = snprintf(command, sizeof(command),
                           "\"%s\" --single --phase=%s --type=%s --limit=%d --data=\"%s\" --queries=\"%s\"",
                           exe_path, phase, type_name, limit, data_path, queries_dir);
    } else {
        written = snprintf(command, sizeof(command),
                           "\"%s\" --single --phase=%s --type=%s --limit=%d --data=\"%s\"",
                           exe_path, phase, type_name, limit, data_path);
    }

    if (written < 0 || written >= (int) sizeof(command)) {
        return FAILURE;
    }

    return runCommand(command);
#endif
}

static int runAll(const char *exe_path, const char *data_path, const char *queries_root) {
    int failures = 0;

    if (ensureBenchDirs() == FAILURE) {
        return FAILURE;
    }

    if (queries_root == NULL) {
        queries_root = "bench/queries";
    }

    if (ensureDir(queries_root) == FAILURE) {
        return FAILURE;
    }

    printCsvHeader();

    for (int li = 0; li < LIMIT_COUNT; li++) {
        int limit = g_limits[li];
        char queries_dir[PATH_BUFFER_SIZE];

        if (makeQueryDirPath(queries_dir, sizeof(queries_dir), queries_root, limit) == FAILURE ||
            ensureDir(queries_dir) == FAILURE) {
            return FAILURE;
        }

        if (runChild(exe_path, "queries", "avl", limit, data_path, queries_dir) == FAILURE) {
            failures++;
        }

        for (int ti = 0; ti < TYPE_COUNT; ti++) {
            if (runChild(exe_path, "index", g_type_names[ti], limit, data_path, NULL) == FAILURE) {
                failures++;
            }
        }

        for (int ti = 0; ti < TYPE_COUNT; ti++) {
            if (runChild(exe_path, "search", g_type_names[ti], limit, data_path, queries_dir) == FAILURE) {
                failures++;
            }
        }
    }

    return failures == 0 ? SUCCESS : FAILURE;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --data=PATH [--queries=DIR]\n"
        "  %s --run-all --data=PATH [--queries=DIR]\n"
        "  %s --single --phase=<queries|index|search> --type=<avl|rb|btree> --limit=N --data=PATH [--queries=DIR]\n"
        "\n"
        "Default --run-all uses limits 50000, 100000, 150000 and starts a fresh process for each measurement.\n",
        prog, prog, prog);
}

int main(int argc, char **argv) {
    const char *data_path = NULL;
    const char *queries_dir = NULL;
    const char *phase = NULL;
    const char *type_name = "avl";
    int limit = 0;
    int single_mode = 0;
    int run_all = 1;
    TreeType type;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run-all") == 0) {
            run_all = 1;
            single_mode = 0;
        } else if (strcmp(argv[i], "--single") == 0) {
            single_mode = 1;
            run_all = 0;
        } else if (strncmp(argv[i], "--data=", 7) == 0) {
            data_path = argv[i] + 7;
        } else if (strncmp(argv[i], "--queries=", 10) == 0) {
            queries_dir = argv[i] + 10;
        } else if (strncmp(argv[i], "--phase=", 8) == 0) {
            phase = argv[i] + 8;
        } else if (strncmp(argv[i], "--type=", 7) == 0) {
            type_name = argv[i] + 7;
        } else if (strncmp(argv[i], "--limit=", 8) == 0) {
            limit = atoi(argv[i] + 8);
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

    if (single_mode) {
        if (phase == NULL || limit <= 0 || typeFromName(type_name, &type) == FAILURE) {
            usage(argv[0]);
            return 1;
        }

        if (strcmp(phase, "queries") == 0) {
            return runSingleQueries(type, data_path, limit, queries_dir) == SUCCESS ? 0 : 1;
        }

        if (strcmp(phase, "index") == 0) {
            return runSingleIndex(type, data_path, limit) == SUCCESS ? 0 : 1;
        }

        if (strcmp(phase, "search") == 0) {
            return runSingleSearch(type, data_path, limit, queries_dir) == SUCCESS ? 0 : 1;
        }

        usage(argv[0]);
        return 1;
    }

    if (run_all) {
        return runAll(argv[0], data_path, queries_dir) == SUCCESS ? 0 : 1;
    }

    usage(argv[0]);
    return 1;
}
