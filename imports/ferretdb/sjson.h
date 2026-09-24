#ifndef WENA_FERRETDB_SJSON_H
#define WENA_FERRETDB_SJSON_H
#include "../json/document.h"
typedef enum WenaSjsonKind {
    WENA_SJSON_OBJECT=1,WENA_SJSON_ARRAY,WENA_SJSON_DOUBLE,WENA_SJSON_STRING,
    WENA_SJSON_BINARY,WENA_SJSON_OBJECT_ID,WENA_SJSON_BOOL,WENA_SJSON_DATE,
    WENA_SJSON_NULL,WENA_SJSON_REGEX,WENA_SJSON_INT32,WENA_SJSON_TIMESTAMP,WENA_SJSON_INT64
} WenaSjsonKind;
typedef struct WenaSjsonValue {
    WenaSjsonKind kind;
    size_t value,schema,name; /* JSON node indexes; unnamed root/array entries use NONE. */
    size_t first,next,count; /* Object children follow $k; arrays retain item order. */
} WenaSjsonValue;
typedef struct WenaSjsonDocument {
    WenaJsonDocument *json;
    WenaSjsonValue *values;
    size_t count;
} WenaSjsonDocument;
/* Read-only typed view of pinned FerretDB v1 SJSON. All schema and values must
 * validate before publication. Initialize *output=NULL. Failure preserves it;
 * success replaces it. Exact number text, binary/regex metadata and field order
 * remain owned in json. No database access, writes or index/owner-lock promises. */
int wena_sjson_parse(const char *input,size_t length,WenaSjsonDocument **output);
void wena_sjson_free(WenaSjsonDocument*);
int wena_sjson_member(const WenaSjsonDocument*,size_t object,const char *key,
    size_t key_length,size_t *value);
#endif
