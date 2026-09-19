#include "label.h"
#include <string.h>

int wena_label_name_valid(const char *name,size_t length)
{
    return name && (length == 0 ||
        wena_model_title_valid(name,length,WENA_LABEL_NAME_CAPACITY));
}
int wena_label_name_string_valid(const char *name)
{
    size_t length;
    if (!name) return 0;
    for (length=0;length<WENA_LABEL_NAME_CAPACITY && name[length];++length) {}
    return wena_label_name_valid(name,length);
}
int wena_label_init(WenaLabel *label,const char *id,const char *board_id,
    const char *name,const char *color,unsigned long position)
{
    WenaLabel candidate;
    if (!label || !wena_model_identifier_valid(id) ||
        !wena_model_identifier_valid(board_id) ||
        !wena_label_name_string_valid(name) ||
        !wena_color_valid(color) || position>WENA_LABEL_POSITION_MAX) return 0;
    memset(&candidate,0,sizeof(candidate));
    strcpy(candidate.id,id);strcpy(candidate.board_id,board_id);
    strcpy(candidate.name,name);strcpy(candidate.color,color);
    candidate.position=position;
    *label=candidate;
    return 1;
}
int wena_label_valid(const WenaLabel *label)
{
    return label && wena_model_identifier_valid(label->id) &&
        wena_model_identifier_valid(label->board_id) &&
        wena_label_name_string_valid(label->name) &&
        wena_color_valid(label->color) && label->position<=WENA_LABEL_POSITION_MAX;
}
