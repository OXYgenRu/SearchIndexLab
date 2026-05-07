#ifndef COMPARATORS_H
#define COMPARATORS_H

#include <string.h>
#include <stdint.h>
#include <stddef.h>

// Функции для сравнения элементов

typedef int (*EqualsFunc)(const void *a, const void *b);

/* =======================
   Integer types
======================= */
static inline int intEquals(const void *a, const void *b)
{
    return *(int *)a == *(int *)b;
}

static inline int intGreater(const void *a, const void *b) // я за вас все компараторы писать не буду, пишите свои. я принцип показал
{
    return *(int *)a > *(int *)b;
}

static inline int shortEquals(const void *a, const void *b)
{
    return *(short *)a == *(short *)b;
}

static inline int longEquals(const void *a, const void *b)
{
    return *(long *)a == *(long *)b;
}

static inline int longGreater(const void *a, const void *b)
{
    return *(long *)a > *(long *)b;
}

static inline int longLongEquals(const void *a, const void *b)
{
    return *(long long *)a == *(long long *)b;
}

static inline int uintEquals(const void *a, const void *b)
{
    return *(unsigned int *)a == *(unsigned int *)b;
}

static inline int ushortEquals(const void *a, const void *b)
{
    return *(unsigned short *)a == *(unsigned short *)b;
}

static inline int ulongEquals(const void *a, const void *b)
{
    return *(unsigned long *)a == *(unsigned long *)b;
}

static inline int ulongLongEquals(const void *a, const void *b)
{
    return *(unsigned long long *)a == *(unsigned long long *)b;
}

/* =======================
   Floating point types
======================= */
static inline int floatEquals(const void *a, const void *b)
{
    return *(float *)a == *(float *)b;
}

static inline int floatGreater(const void *a, const void *b)
{
    return *(float *)a > *(float *)b;
}

static inline int doubleEquals(const void *a, const void *b)
{
    return *(double *)a == *(double *)b;
}

static inline int longDoubleEquals(const void *a, const void *b)
{
    return *(long double *)a == *(long double *)b;
}

/* =======================
   Char types
======================= */
static inline int charEquals(const void *a, const void *b)
{
    return *(char *)a == *(char *)b;
}

static inline int ucharEquals(const void *a, const void *b)
{
    return *(unsigned char *)a == *(unsigned char *)b;
}

/* =======================
   Bool type
======================= */
static inline int boolEquals(const void *a, const void *b)
{
    return *(int *)a == *(int *)b; // assumes _Bool/int stored as int
}

/* =======================
   String comparison
======================= */

static inline int stringEquals(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b) == 0;
}
#endif
