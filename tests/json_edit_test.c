#include "../imports/json/edit.h"
#include "../imports/ferretdb/edit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(WenaJsonDocument **d,WenaJsonEdit *edits,size_t count)
{WenaJsonDocument *old;old=*d;assert(!wena_json_edit(*d,edits,count,d)&&*d==old);}
static void typed(WenaSjsonDocument **d,size_t index,const char *value,const char *schema,int valid)
{
    WenaSjsonDocument *old;old=*d;
    assert(wena_sjson_edit(*d,index,value,strlen(value),schema,strlen(schema),d)==valid);
    if(!valid)assert(*d==old);
}
int main(void)
{
    const char *input;WenaJsonDocument *d;WenaSjsonDocument *s;WenaJsonEdit edits[2];
    size_t a,b,i;char *large;WenaJsonDocument *separate;
    input=" { \"a\" : [1,2], \"b\" : 18446744073709551615 } ";d=NULL;
    assert(wena_json_parse(input,strlen(input),&d));
    assert(wena_json_member(d,0,"a",1,&a));assert(wena_json_member(d,0,"b",1,&b));
    edits[0].node=b;edits[0].json="false";edits[0].length=5;
    edits[1].node=a;edits[1].json="{\"nested\":true}";edits[1].length=strlen(edits[1].json);
    separate=NULL;assert(wena_json_edit(d,edits,2,&separate));
    assert(!strcmp(d->raw,input));assert(!strcmp(separate->raw," { \"a\" : {\"nested\":true}, \"b\" : false } "));
    wena_json_free(separate);
    edits[0].node=a;edits[1].node=d->nodes[a].first;fail(&d,edits,2);
    edits[1].node=a;fail(&d,edits,2);edits[0].node=d->count;fail(&d,edits,1);
    edits[0].node=a;edits[0].json="1,\"injected\":true";edits[0].length=strlen(edits[0].json);fail(&d,edits,1);
    edits[0].json="\"\\ud800\"";edits[0].length=strlen(edits[0].json);fail(&d,edits,1);
    edits[0].node=d->nodes[0].first;edits[0].json="\"b\"";edits[0].length=3;fail(&d,edits,1);
    edits[0].json="1";edits[0].length=1;fail(&d,edits,1);
    edits[0].node=a;edits[0].json=d->raw+d->nodes[b].start;edits[0].length=d->nodes[b].length;
    assert(wena_json_edit(d,edits,1,&d));assert(strstr(d->raw,"\"a\" : 18446744073709551615"));
    fail(&d,edits,0);fail(&d,edits,WENA_JSON_MAX_EDITS+1u);
    large=(char*)malloc(WENA_JSON_MAX_BYTES);assert(large);
    large[0]='\"';for(i=1;i<WENA_JSON_MAX_BYTES-1u;++i)large[i]='x';large[i]='\"';
    edits[0].node=a;edits[0].json=large;edits[0].length=WENA_JSON_MAX_BYTES;fail(&d,edits,1);
    edits[0].node=0;assert(wena_json_edit(d,edits,1,&d)==0); /* Original surrounding whitespace exceeds bound. */
    free(large);wena_json_free(d);
    s=NULL;input="{\"$s\":{\"p\":{\"a\":{\"t\":\"array\",\"i\":[{\"t\":\"int\"}]},\"b\":{\"t\":\"string\"}},\"$k\":[\"b\",\"a\"]},\"a\":[1],\"b\":\"keep\"}";
    assert(wena_sjson_parse(input,strlen(input),&s));assert(wena_sjson_member(s,0,"a",1,&a));
    b=s->values[a].first;
    typed(&s,b,"18446744073709551615","{\"t\":\"timestamp\"}",1);
    assert(strstr(s->json->raw,"\"a\":[18446744073709551615]"));
    assert(strstr(s->json->raw,"\"$k\":[\"b\",\"a\"]"));assert(strstr(s->json->raw,"\"b\":\"keep\""));
    typed(&s,b,"18446744073709551616","{\"t\":\"timestamp\"}",0);
    typed(&s,b,"\"text\"","{\"t\":\"int\"}",0);
    typed(&s,b,"1","{\"t\":\"unknown\"}",0);
    typed(&s,b,"1,2","{\"t\":\"int\"}",0);
    typed(&s,b,"1","{\"t\":\"int\"},\"other\":{}",0);
    typed(&s,0,"{}","{}",0);typed(&s,s->count,"null","{\"t\":\"null\"}",0);
    typed(&s,a,"{\"z\":true}","{\"t\":\"object\",\"$s\":{\"p\":{\"z\":{\"t\":\"bool\"}},\"$k\":[\"z\"]}}",1);
    assert(wena_sjson_member(s,a,"z",1,&b));assert(s->values[b].kind==WENA_SJSON_BOOL);
    typed(&s,b,"null","{\"t\":\"null\"}",1);assert(s->values[b].kind==WENA_SJSON_NULL);
    wena_sjson_free(s);puts("Atomic JSON and SJSON edits: passed");return 0;
}
