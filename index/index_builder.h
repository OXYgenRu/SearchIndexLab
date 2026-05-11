#pragma once

#include "index.h"

#define SUCCESS 0
#define FAILURE (-1)

int buildIndexFromJsonl(Index *idx, const char *path, int limit);