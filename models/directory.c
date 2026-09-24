#include "directory.h"
#include <string.h>
int wena_directory_page_valid(const WenaDirectoryPage *page)
{
    size_t last,count,index;
    if (!page || (page->kind!=WENA_DIRECTORY_BOARDS && page->kind!=WENA_DIRECTORY_ACTORS) ||
        !page->page_size || page->page_size>WENA_DIRECTORY_PAGE_CAPACITY) return 0;
    last=page->total ? (page->total-1)/page->page_size : 0;
    if (page->page>last || page->first!=page->page*page->page_size) return 0;
    count=page->total-page->first;
    if (count>page->page_size) count=page->page_size;
    if (page->count!=count) return 0;
    for(index=0;index<count;++index) {
        const WenaDirectoryRow *row;
        row=&page->rows[index];
        if (!wena_model_identifier_valid(row->id) ||
            !wena_model_title_string_valid(row->title,sizeof(row->title)) ||
            !row->version || row->version>WENA_VERSION_READ_MAX ||
            (index && strcmp(page->rows[index-1].id,row->id)>=0)) return 0;
    }
    return 1;
}
