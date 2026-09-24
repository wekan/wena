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

int wena_label_catalogue_valid(const WenaLabel *labels,size_t count,const char *board)
{
    size_t i,j;
    if(!wena_model_identifier_valid(board)||count>WENA_BOARD_LABEL_CAPACITY||(!labels&&count))return 0;
    for(i=0;i<count;++i){
        if(!wena_label_valid(&labels[i])||strcmp(labels[i].board_id,board)||
            (i&&labels[i-1].position>=labels[i].position))return 0;
        for(j=0;j<i;++j)if(!strcmp(labels[i].id,labels[j].id)||
            (!strcmp(labels[i].name,labels[j].name)&&!strcmp(labels[i].color,labels[j].color)))return 0;
    }
    return 1;
}

int wena_label_transfer_map(const WenaLabel *source,size_t source_count,const char *source_board,
    const WenaId *assigned,size_t assigned_count,const WenaLabel *destination,
    size_t destination_count,const char *destination_board,unsigned char *output)
{
    unsigned char selected[WENA_BOARD_LABEL_CAPACITY],mapped[WENA_BOARD_LABEL_CAPACITY];size_t i,j;
    if(!output||assigned_count>source_count||(!assigned&&assigned_count)||
        !wena_label_catalogue_valid(source,source_count,source_board)||
        !wena_label_catalogue_valid(destination,destination_count,destination_board)||
        !strcmp(source_board,destination_board))return 0;
    memset(selected,0,sizeof(selected));memset(mapped,0,sizeof(mapped));
    for(i=0;i<assigned_count;++i){
        if(!wena_model_identifier_valid(assigned[i]))return 0;
        for(j=0;j<source_count;++j)if(!strcmp(assigned[i],source[j].id))break;
        if(j==source_count||selected[j])return 0;
        selected[j]=1;
    }
    for(i=0;i<destination_count;++i)if(destination[i].name[0]){
        for(j=0;j<source_count;++j)if(selected[j]&&!strcmp(destination[i].name,source[j].name)){
            mapped[i]=1;break;
        }
    }
    memcpy(output,mapped,sizeof(mapped));return 1;
}
