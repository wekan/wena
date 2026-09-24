#include "card_section.h"
#include <stdlib.h>
#include <string.h>

int wena_card_section_key_valid(const char *key)
{
    size_t index;unsigned char c;
    if (!key || !key[0]) return 0;
    for (index=0;index<WENA_SECTION_KEY_CAPACITY;++index) {
        c=(unsigned char)key[index];if (!c) return 1;
        if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return 0;
    }
    return 0;
}
int wena_card_section_checklist_key(const char *id,char *output,size_t capacity)
{
    if (!output || !wena_model_identifier_valid(id) || strlen(id)+sizeof("checklist-")>capacity) return 0;
    strcpy(output,"checklist-");strcat(output,id);return 1;
}
void wena_card_sections_free(WenaCardSectionsSnapshot *snapshot)
{ if (snapshot) {free(snapshot->entries);free(snapshot);} }
int wena_card_section_compare(const char *card,const char *key,const WenaCardSectionPreference *entry)
{ int order;order=strcmp(card,entry->card_id);return order?order:strcmp(key,entry->key); }
const WenaCardSectionPreference *wena_card_sections_find(
    const WenaCardSectionsSnapshot *snapshot,const char *card,const char *key)
{
    size_t first,last,middle;int order;
    if (!snapshot || snapshot->count>snapshot->capacity || (snapshot->count && !snapshot->entries) ||
        !wena_model_identifier_valid(card) || !wena_card_section_key_valid(key)) return NULL;
    first=0;last=snapshot->count;
    while (first<last) {middle=first+(last-first)/2;order=wena_card_section_compare(card,key,&snapshot->entries[middle]);
        if (!order) return &snapshot->entries[middle];
        if (order<0) last=middle;else first=middle+1;
    }
    return NULL;
}
