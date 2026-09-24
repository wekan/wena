#include "../imports/ferretdb/sjson.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void scalar(const char *type,const char *extra,const char *value,int valid)
{
    char input[2048];WenaSjsonDocument *d;size_t index;int result;
    sprintf(input,"{\"$s\":{\"p\":{\"v\":{\"t\":\"%s\"%s}},\"$k\":[\"v\"]},\"v\":%s}",type,extra,value);
    d=NULL;result=wena_sjson_parse(input,strlen(input),&d);assert(result==valid);
    if(valid){assert(wena_sjson_member(d,0,"v",1,&index));assert(d->count>=2);}
    wena_sjson_free(d);
}
static void reject(const char *input,WenaSjsonDocument **d)
{WenaSjsonDocument *old;old=*d;assert(!wena_sjson_parse(input,strlen(input),d));assert(*d==old);}
int main(void)
{
    WenaSjsonDocument *d;size_t a,b,i,j;char mutated[512];const char *s;
    scalar("int","","2147483647",1);scalar("int","","-2147483648",1);
    scalar("int","","2147483648",0);scalar("int","","-2147483649",0);
    scalar("int","","1.0",0);scalar("int","","1e0",0);
    scalar("long","","9223372036854775807",1);scalar("long","","-9223372036854775808",1);
    scalar("long","","9223372036854775808",0);scalar("long","","-9223372036854775809",0);
    scalar("date","","-9223372036854775808",1);scalar("date","","9223372036854775808",0);
    scalar("timestamp","","18446744073709551615",1);scalar("timestamp","","18446744073709551616",0);
    scalar("timestamp","","-0",0);scalar("timestamp","","0",1);
    scalar("double","","1.7976931348623157e308",1);scalar("double","","1e309",0);
    scalar("double","","-1e309",0);scalar("double","","1e-9999",1);
    scalar("double","","0e9999",1);scalar("double","","\"NaN\"",1);
    scalar("double","","\"N\\u0061N\"",0);scalar("double","","\"Inf\"",0);
    scalar("string","","\"a\\u0000b\"",1);scalar("string","","1",0);
    scalar("bool","","true",1);scalar("bool","","false",1);scalar("bool","","0",0);
    scalar("null","","null",1);scalar("null","","false",0);
    scalar("objectId","","\"0123456789abcdefABCDEF01\"",1);
    scalar("objectId","","\"0123456789abcdefABCDEF0g\"",0);scalar("objectId","","\"01\"",0);
    scalar("binData",",\"s\":255","\"YWJj\"",1);scalar("binData",",\"s\":0","\"\"",1);
    scalar("binData",",\"s\":0","\"YQ==\\r\\n\"",1);scalar("binData",",\"s\":0","\"YWI=\"",1);
    scalar("binData",",\"s\":0","\"YQ=A\"",0);scalar("binData",",\"s\":0","\"YQ==YQ==\"",0);
    scalar("binData",",\"s\":0","\"YQ\"",0);scalar("binData",",\"s\":0","\"Y Q==\"",0);
    scalar("binData",",\"s\":256","\"\"",0);scalar("binData",",\"s\":-1","\"\"",0);
    scalar("binData","","\"\"",0);scalar("binData",",\"s\":0","null",1);
    scalar("regex",",\"o\":\"im\"","\"a.*\"",1);scalar("regex","","\"a\"",0);
    scalar("regex",",\"o\":0","\"a\"",0);scalar("unknown","","null",0);
    scalar("int",",\"extra\":0","1",0);scalar("array",",\"i\":[]","[]",1);
    scalar("array",",\"i\":[]","[1]",0);scalar("array",",\"i\":[{\"t\":\"int\"}]","[1]",1);
    scalar("array",",\"i\":[{\"t\":\"int\"}]","[\"1\"]",0);
    scalar("object",",\"$s\":{}","{}",1);scalar("object",",\"$s\":{}","{\"a\":1}",0);
    scalar("object",",\"$s\":{}","null",1);
    d=NULL;s="{\"$s\":{}}";assert(wena_sjson_parse(s,strlen(s),&d));assert(d->count==1);
    reject("{}",&d);reject("[]",&d);reject("{\"$s\":{},\"a\":1}",&d);
    reject("{\"$s\":{\"p\":{}}}",&d);
    reject("{\"$s\":{\"p\":{\"a\":{\"t\":\"int\"}},\"$k\":[\"b\"]},\"a\":1}",&d);
    reject("{\"$s\":{\"p\":{\"a\":{\"t\":\"int\"},\"b\":{\"t\":\"int\"}},\"$k\":[\"a\",\"a\"]},\"a\":1,\"b\":2}",&d);
    s="{\"b\":18446744073709551615,\"a\":{\"z\":[1,\"x\"]},\"$s\":{\"p\":{\"b\":{\"t\":\"timestamp\"},\"a\":{\"t\":\"object\",\"$s\":{\"p\":{\"z\":{\"t\":\"array\",\"i\":[{\"t\":\"int\"},{\"t\":\"string\"}]}},\"$k\":[\"z\"]}}},\"$k\":[\"a\",\"b\"]}}";
    assert(wena_sjson_parse(s,strlen(s),&d));assert(d->count==6);
    assert(wena_sjson_member(d,0,"a",1,&a));assert(wena_sjson_member(d,0,"b",1,&b));
    assert(d->values[0].first==a&&d->values[a].next==b);
    assert(d->json->nodes[d->values[b].value].length==20);
    assert(!memcmp(d->json->raw+d->json->nodes[d->values[b].value].start,"18446744073709551615",20));
    /* Exercise every byte substitution in a nested valid document. */
    for(i=0;i<strlen(s);++i)for(j=0;j<256;++j){
        WenaSjsonDocument *probe;strcpy(mutated,s);mutated[i]=(char)j;probe=NULL;
        if(wena_sjson_parse(mutated,strlen(s),&probe)){
            size_t k;assert(probe->count<=probe->json->count);
            for(k=0;k<probe->count;++k){assert(probe->values[k].value<probe->json->count);
                assert(probe->values[k].next==WENA_JSON_NONE||probe->values[k].next<probe->count);}
        }
        wena_sjson_free(probe);
    }
    wena_sjson_free(d);puts("SJSON typed snapshots: passed");return 0;
}
