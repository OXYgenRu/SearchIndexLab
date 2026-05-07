#include <string.h>
#include "posting.h"

Vector* createPostingList(void) {
    return createVector(sizeof(PostingEntry));
}

void appendPosting(Vector* list, int doc_id, const char* title) {
    if (!list) return;
    PostingEntry entry;
    entry.doc_id = doc_id;
    strncpy(entry.title, title, MAX_TITLE_LEN - 1);
    entry.title[MAX_TITLE_LEN - 1] = '\0';
    appendVectorItem(list, &entry);
}

Vector* clonePostingList(const Vector* src) {
    if (!src) return NULL;
    Vector* clone = createPostingList();
    for (size_t i = 0; i < src->size; i++)
        appendVectorItem(clone, getVectorItem((Vector*)src, i));
    return clone;
}
