#ifndef WENA_DIRECTORY_H
#define WENA_DIRECTORY_H
#include "model.h"
#define WENA_DIRECTORY_PAGE_CAPACITY 32u
typedef enum WenaDirectoryKind {
    WENA_DIRECTORY_BOARDS=1,
    WENA_DIRECTORY_ACTORS=2,
    WENA_DIRECTORY_CARDS=3
} WenaDirectoryKind;
typedef struct WenaDirectoryRow {
    WenaId id;
    WenaTitle title;
    unsigned long version;
} WenaDirectoryRow;
typedef struct WenaDirectoryPage {
    WenaDirectoryKind kind;
    WenaId board_id; /* Required for cards; empty for global directories. */
    size_t total,first,count,page,page_size;
    WenaDirectoryRow rows[WENA_DIRECTORY_PAGE_CAPACITY];
} WenaDirectoryPage;
int wena_directory_page_valid(const WenaDirectoryPage *page);
#endif
