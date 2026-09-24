#include "edit.h"
#include <stdlib.h>
#include <string.h>
int wena_json_edit(const WenaJsonDocument *source,const WenaJsonEdit *edits,
    size_t count,WenaJsonDocument **output)
{
    size_t order[WENA_JSON_MAX_EDITS],i,j,index,start,end,total,at,position,length;
    WenaJsonDocument *fragment;char *text;int valid;
    if(!source||!edits||!count||count>WENA_JSON_MAX_EDITS||!output)return 0;
    total=source->length;
    for(i=0;i<count;++i){
        if(edits[i].node>=source->count)return 0;
        fragment=NULL;
        if(!wena_json_parse(edits[i].json,edits[i].length,&fragment))return 0;
        wena_json_free(fragment);
        order[i]=i;j=i;
        while(j&&source->nodes[edits[order[j-1]].node].start>source->nodes[edits[i].node].start){
            order[j]=order[j-1];--j;
        }
        order[j]=i;
    }
    end=0;
    /* Subtract before adding, so replacing large values never overflows and a
     * simultaneous shrink can make room for another node's expansion. */
    for(i=0;i<count;++i){
        index=order[i];start=source->nodes[edits[index].node].start;
        length=source->nodes[edits[index].node].length;
        if(start<end)return 0;
        end=start+length;total-=length;
    }
    for(i=0;i<count;++i){
        if(edits[i].length>WENA_JSON_MAX_BYTES-total)return 0;
        total+=edits[i].length;
    }
    text=(char*)malloc(total+1u);if(!text)return 0;
    at=position=0;
    for(i=0;i<count;++i){
        index=order[i];start=source->nodes[edits[index].node].start;
        length=start-position;memcpy(text+at,source->raw+position,length);at+=length;
        memcpy(text+at,edits[index].json,edits[index].length);at+=edits[index].length;
        position=start+source->nodes[edits[index].node].length;
    }
    length=source->length-position;memcpy(text+at,source->raw+position,length);at+=length;text[at]=0;
    valid=wena_json_parse(text,total,output);free(text);return valid;
}
