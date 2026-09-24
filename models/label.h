#ifndef WENA_LABEL_H
#define WENA_LABEL_H
#include "model.h"
#include "color.h"
#define WENA_LABEL_NAME_CAPACITY 129u
#define WENA_LABEL_POSITION_MAX 2147483647UL
#define WENA_BOARD_LABEL_CAPACITY 128u

typedef struct WenaLabel {
    WenaId id;
    WenaId board_id;
    char name[WENA_LABEL_NAME_CAPACITY];
    char color[WENA_COLOR_CAPACITY];
    unsigned long position;
} WenaLabel;
int wena_label_name_valid(const char *name,size_t length);
int wena_label_name_string_valid(const char *name);
int wena_label_init(WenaLabel *label,const char *id,const char *board_id,
    const char *name,const char *color,unsigned long position);
int wena_label_valid(const WenaLabel *label);
/* Complete, position-ordered board catalogue: unique IDs and name/color pairs. */
int wena_label_catalogue_valid(const WenaLabel *labels,size_t count,const char *board);
/* Cross-board move semantics from WeKan Cards.move: retain every destination
 * label whose nonempty name exactly matches an assigned source label. Colors
 * and IDs need not match. Output is a fixed WENA_BOARD_LABEL_CAPACITY array of
 * 0/1 flags indexed by destination catalogue order; unused flags become zero.
 * Reject malformed catalogues, duplicate/unknown assignments or equal boards.
 * Failure preserves output. No allocation, I/O or model mutation. */
int wena_label_transfer_map(const WenaLabel *source,size_t source_count,const char *source_board,
    const WenaId *assigned,size_t assigned_count,const WenaLabel *destination,
    size_t destination_count,const char *destination_board,unsigned char *output);
#endif
