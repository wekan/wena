#include "store.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

WenaLabelSnapshot *wena_label_snapshot_create(void)
{
    return (WenaLabelSnapshot *)calloc(1,sizeof(WenaLabelSnapshot));
}
void wena_label_snapshot_free(WenaLabelSnapshot *snapshot)
{
    free(snapshot);
}
int wena_label_snapshot_valid(const WenaLabelSnapshot *snapshot,
    const char *board_id,const char *card_id)
{
    size_t i;
    int card_mode;
    card_mode=card_id && card_id[0];
    if (!snapshot || !wena_model_identifier_valid(board_id) ||
        !wena_model_identifier_valid(snapshot->board_id) ||
        strcmp(snapshot->board_id,board_id) || !snapshot->board_version ||
        snapshot->board_version > WENA_VERSION_READ_MAX ||
        !wena_label_catalogue_valid(snapshot->labels,snapshot->label_count,board_id)) return 0;
    if (card_mode) {
        if (!wena_model_identifier_valid(card_id) ||
            !wena_model_identifier_valid(snapshot->card_id) ||
            strcmp(snapshot->card_id,card_id) || !snapshot->card_version ||
            snapshot->card_version > WENA_VERSION_READ_MAX) return 0;
    } else if (snapshot->card_id[0] || snapshot->card_version) return 0;
    for (i=0;i<snapshot->label_count;++i) {
        if (!snapshot->label_versions[i] ||
            snapshot->label_versions[i] > WENA_VERSION_READ_MAX ||
            (snapshot->assigned[i]!=0 && snapshot->assigned[i]!=1) ||
            snapshot->assigned_card_counts[i]>(unsigned long)LONG_MAX ||
            (snapshot->assigned[i] && !snapshot->assigned_card_counts[i]) ||
            (!card_mode && snapshot->assigned[i])) return 0;
    }
    return 1;
}

WenaLabelBoardSnapshot *wena_label_board_snapshot_create(void)
{
    return (WenaLabelBoardSnapshot *)calloc(1,sizeof(WenaLabelBoardSnapshot));
}
void wena_label_board_snapshot_free(WenaLabelBoardSnapshot *snapshot)
{
    free(snapshot);
}
size_t wena_label_board_card_index(const WenaLabelBoardSnapshot *snapshot,const char *card_id)
{
    size_t low,high,middle;
    int compared;
    if (!snapshot || snapshot->card_count>WENA_LABEL_BOARD_CARD_CAPACITY) return 0;
    if (!wena_model_identifier_valid(card_id)) return snapshot->card_count;
    low=0;high=snapshot->card_count;
    while (low<high) {
        middle=low+(high-low)/2;
        compared=strcmp(snapshot->card_ids[middle],card_id);
        if (!compared) return middle;
        if (compared<0) low=middle+1;else high=middle;
    }
    return snapshot->card_count;
}
const unsigned char *wena_label_board_assignments(const WenaLabelBoardSnapshot *snapshot,
    const char *board_id,const char *card_id)
{
    size_t index;
    if (!snapshot || snapshot->card_count>WENA_LABEL_BOARD_CARD_CAPACITY ||
        !wena_model_identifier_valid(board_id) ||
        !wena_model_identifier_valid(snapshot->catalogue.board_id) ||
        strcmp(snapshot->catalogue.board_id,board_id)) return NULL;
    index=wena_label_board_card_index(snapshot,card_id);
    return index<snapshot->card_count?snapshot->assignments[index]:NULL;
}
int wena_label_board_snapshot_valid(const WenaLabelBoardSnapshot *snapshot,const char *board_id)
{
    size_t card,label;
    unsigned long assigned[WENA_BOARD_LABEL_CAPACITY];
    unsigned char mask;
    if (!snapshot || snapshot->card_count>WENA_LABEL_BOARD_CARD_CAPACITY ||
        !wena_label_snapshot_valid(&snapshot->catalogue,board_id,NULL)) return 0;
    memset(assigned,0,sizeof(assigned));
    for (card=0;card<snapshot->card_count;++card) {
        if (!wena_model_identifier_valid(snapshot->card_ids[card]) ||
            !snapshot->card_versions[card] || snapshot->card_versions[card]>WENA_VERSION_READ_MAX ||
            (card && strcmp(snapshot->card_ids[card-1],snapshot->card_ids[card])>=0)) return 0;
        for (label=0;label<WENA_BOARD_LABEL_CAPACITY;++label) {
            mask=(unsigned char)(1u<<(label%8u));
            if (snapshot->assignments[card][label/8u]&mask) {
                if (label>=snapshot->catalogue.label_count) return 0;
                ++assigned[label];
            }
        }
    }
    for (label=0;label<snapshot->catalogue.label_count;++label)
        if (assigned[label]!=snapshot->catalogue.assigned_card_counts[label]) return 0;
    return 1;
}

int wena_label_selection_snapshot_valid(const WenaLabelSelectionSnapshot *snapshot,const char *board)
{
    size_t i,j;
    if(!snapshot||!snapshot->card_count||snapshot->card_count>WENA_LABEL_BOARD_CARD_CAPACITY||
        !wena_label_snapshot_valid(&snapshot->catalogue,board,NULL))return 0;
    for(i=0;i<snapshot->card_count;++i){
        if(!wena_model_identifier_valid(snapshot->cards[i].id)||!snapshot->cards[i].version||
            snapshot->cards[i].version>WENA_VERSION_MUTATE_MAX)return 0;
        for(j=0;j<i;++j)if(!strcmp(snapshot->cards[i].id,snapshot->cards[j].id))return 0;
    }
    for(i=0;i<WENA_BOARD_LABEL_CAPACITY;++i)
        if(snapshot->assigned_counts[i]>snapshot->card_count||
            (i>=snapshot->catalogue.label_count&&snapshot->assigned_counts[i])||
            snapshot->assigned_counts[i]>snapshot->catalogue.assigned_card_counts[i])return 0;
    return 1;
}
