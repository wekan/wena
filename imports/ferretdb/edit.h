#ifndef WENA_FERRETDB_EDIT_H
#define WENA_FERRETDB_EDIT_H
#include "sjson.h"
/* Replace one non-root typed value and its descriptor together. Keys, sibling
 * order and untouched bytes remain intact. JSON fragments use explicit lengths.
 * The entire SJSON result must validate before atomically replacing *output.
 * Source may equal *output. Pure in-memory editing; no database writes. */
int wena_sjson_edit(const WenaSjsonDocument *source,size_t value,
    const char *json,size_t json_length,const char *descriptor,size_t descriptor_length,
    WenaSjsonDocument **output);
#endif
