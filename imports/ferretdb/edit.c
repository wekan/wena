#include "edit.h"
#include "../json/edit.h"
int wena_sjson_edit(const WenaSjsonDocument *source,size_t value,
    const char *json,size_t json_length,const char *descriptor,size_t descriptor_length,
    WenaSjsonDocument **output)
{
    WenaJsonEdit edits[2];WenaJsonDocument *candidate;int valid;
    if(!source||!value||value>=source->count||!output)return 0;
    edits[0].node=source->values[value].value;edits[0].json=json;edits[0].length=json_length;
    edits[1].node=source->values[value].schema;edits[1].json=descriptor;edits[1].length=descriptor_length;
    candidate=NULL;
    if(!wena_json_edit(source->json,edits,2,&candidate))return 0;
    valid=wena_sjson_parse(candidate->raw,candidate->length,output);
    wena_json_free(candidate);return valid;
}
