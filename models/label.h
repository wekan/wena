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
#endif
