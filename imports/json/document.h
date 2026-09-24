#ifndef WENA_JSON_DOCUMENT_H
#define WENA_JSON_DOCUMENT_H
#include <stddef.h>
#define WENA_JSON_MAX_BYTES 1048576UL
#define WENA_JSON_MAX_NODES 4096u
#define WENA_JSON_MAX_DEPTH 32u
#define WENA_JSON_NONE ((size_t)-1)
typedef enum WenaJsonKind {
    WENA_JSON_OBJECT=1,WENA_JSON_ARRAY,WENA_JSON_STRING,WENA_JSON_NUMBER,
    WENA_JSON_TRUE,WENA_JSON_FALSE,WENA_JSON_NULL
} WenaJsonKind;
typedef struct WenaJsonNode {
    WenaJsonKind kind;
    size_t start,length; /* Exact original JSON bytes, including string quotes. */
    size_t text_start,text_length; /* Decoded UTF-8, strings only; may contain NUL. */
    size_t first,next,count; /* Object children alternate key/value; count is pairs. */
} WenaJsonNode;
typedef struct WenaJsonDocument {
    char *raw,*strings;
    size_t length,string_length,count,capacity;
    WenaJsonNode *nodes;
} WenaJsonDocument;
/* Own a bounded strict JSON snapshot. Initialize *output=NULL, then free or
 * replace with this API. Success replaces it; failure preserves it unchanged.
 * Root is node 0. Object order and number spelling/precision are preserved.
 * Reject duplicate decoded keys, non-scalar Unicode, invalid UTF-8 and trailing
 * values. No SQL, locale-dependent conversion, filesystem or network access. */
int wena_json_parse(const char *input,size_t length,WenaJsonDocument **output);
void wena_json_free(WenaJsonDocument*);
/* Return a borrowed decoded string plus explicit byte length, or NULL. */
const char *wena_json_string(const WenaJsonDocument*,size_t node,size_t *length);
int wena_json_member(const WenaJsonDocument*,size_t object,const char *key,
    size_t key_length,size_t *value);
#endif
