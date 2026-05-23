#include "levenshtein.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MEMORY_ALLOCATION_ERROR "ERROR: allocated memory is NULL"
#define FAILURE (-1)

static int minInt(int a, int b) {
    return (a < b) ? a : b;
}

static int minOfThree(int a, int b, int c) {
    return minInt(minInt(a, b), c);
}

static int **allocateTable(int rows, int cols) {
    int **new_table = malloc(sizeof(int *) * rows);
    if (!new_table) {
        free(new_table);
        printf(MEMORY_ALLOCATION_ERROR);
        return NULL;
    }
    for (int i = 0; i < rows; i++) {
        new_table[i] = malloc(sizeof(int) * cols);
        if (!new_table[i]) {
            printf(MEMORY_ALLOCATION_ERROR);

            for (int j = 0; j < i; j++) {
                free(new_table[j]);
            }
            free(new_table);
            return NULL;
        }
        for (int j = 0; j < cols; j++) {
            new_table[i][j] = 0;
        }
    }
    for (int i = 0; i < rows; i++) {
        new_table[i][0] = i;
    }
    for (int i = 0; i < cols; i++) {
        new_table[0][i] = i;
    }
    return new_table;
}

static void freeTable(int **table, int rows) {
    if (!table) {
        return;
    }
    for (int i = 0; i < rows; i++) {
        free(table[i]);
    }
    free(table);
}

int levenshteinDistance(const char *s1, const char *s2) {
    if (!s1 || !s2) {
        return FAILURE;
    }
    size_t n_len = strlen(s1);
    size_t m_len = strlen(s2);
    if (n_len > INT_MAX || m_len > INT_MAX) {
        return FAILURE;
    }

    int n = (int) n_len;
    int m = (int) m_len;
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }

    int **table = allocateTable(n + 1, m + 1);
    if (!table) {
        return FAILURE;
    }
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            if (s1[i - 1] == s2[j - 1]) {
                table[i][j] = table[i - 1][j - 1];
                continue;
            }
            table[i][j] = 1 + minOfThree(table[i - 1][j], table[i - 1][j - 1], table[i][j - 1]);
        }
    }
    int result = table[n][m];
    freeTable(table, n + 1);
    return result;
}
