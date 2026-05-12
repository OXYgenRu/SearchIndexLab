#pragma once

#include "../posting.h"
#include "index.h"

#define SEARCH_TOP_LIMIT      10
#define SEARCH_TITLE_LENGTH   256

typedef struct {
    int  doc_id;
    char title[SEARCH_TITLE_LENGTH];
    int  score;
} SearchResult;

typedef struct {
    Vector *results;
    int     total;
    double  time_ms;
} SearchResults;

Vector*        intersectPostings(Vector **lists, int n);
SearchResults* search(Index *idx, const char *query);
void           printResultsText(const SearchResults *sr);
void           printResultsJSON(const SearchResults *sr);
void           freeSearchResults(SearchResults *sr);
