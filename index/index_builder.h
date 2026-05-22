#pragma once

#include "index.h"

/* Читает docs.jsonl и наполняет idx через indexDocument.
   limit > 0 — читать только первые limit документов;
   limit <= 0 — читать весь файл.
   Возвращает количество проиндексированных документов (>= 0)
   или -1 при ошибке открытия файла / выделения памяти. */
int buildIndexFromJsonl(Index *idx, const char *path, int limit);
