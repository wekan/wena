#include "sjson.h"
#include <errno.h>
#include <float.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
static int field(const WenaJsonDocument *d,size_t object,const char *key,size_t *out)
{return wena_json_member(d,object,key,strlen(key),out);}
static int text_is(const WenaJsonDocument *d,size_t index,const char *expected)
{
    const char *s;size_t n;s=wena_json_string(d,index,&n);
    return s&&n==strlen(expected)&&!memcmp(s,expected,n);
}
static WenaSjsonKind type(const WenaJsonDocument *d,size_t descriptor)
{
    static const char *names[]={"object","array","double","string","binData","objectId",
        "bool","date","null","regex","int","timestamp","long"};
    size_t index,i;if(!field(d,descriptor,"t",&index))return (WenaSjsonKind)0;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i)if(text_is(d,index,names[i]))return (WenaSjsonKind)(i+1u);
    return (WenaSjsonKind)0;
}
static int integer(const WenaJsonDocument *d,size_t index,const char *positive,const char *negative)
{
    const WenaJsonNode *n;const char *s,*limit;size_t length,i;
    n=&d->nodes[index];if(n->kind!=WENA_JSON_NUMBER)return 0;
    s=d->raw+n->start;length=n->length;limit=positive;
    if(*s=='-'){if(!negative)return 0;limit=negative;++s;--length;}
    if(!length)return 0;
    for(i=0;i<length;++i)if(s[i]<'0'||s[i]>'9')return 0;
    return length<strlen(limit)||(length==strlen(limit)&&memcmp(s,limit,length)<=0);
}
static int descriptor_valid(const WenaJsonDocument*,size_t,unsigned char*);
static int schema_valid(const WenaJsonDocument *d,size_t schema,unsigned char *seen)
{
    size_t properties,keys,key,descriptor;const char *name;size_t length;
    if(d->nodes[schema].kind!=WENA_JSON_OBJECT)return 0;
    if(!d->nodes[schema].count)return 1;
    if(d->nodes[schema].count!=2||!field(d,schema,"p",&properties)||!field(d,schema,"$k",&keys)||
        d->nodes[properties].kind!=WENA_JSON_OBJECT||d->nodes[keys].kind!=WENA_JSON_ARRAY||
        d->nodes[properties].count!=d->nodes[keys].count)return 0;
    for(key=d->nodes[keys].first;key!=WENA_JSON_NONE;key=d->nodes[key].next){
        name=wena_json_string(d,key,&length);
        if(!name||!wena_json_member(d,properties,name,length,&descriptor)||seen[descriptor])return 0;
        seen[descriptor]=1;
        if(!descriptor_valid(d,descriptor,seen))return 0;
    }
    return 1;
}
static int descriptor_valid(const WenaJsonDocument *d,size_t descriptor,unsigned char *seen)
{
    WenaSjsonKind kind;size_t extra,child;
    if(d->nodes[descriptor].kind!=WENA_JSON_OBJECT)return 0;
    kind=type(d,descriptor);if(!kind)return 0;
    if(kind==WENA_SJSON_OBJECT){
        return d->nodes[descriptor].count==2&&field(d,descriptor,"$s",&extra)&&schema_valid(d,extra,seen);
    }
    if(kind==WENA_SJSON_ARRAY){
        if(d->nodes[descriptor].count!=2||!field(d,descriptor,"i",&extra)||d->nodes[extra].kind!=WENA_JSON_ARRAY)return 0;
        for(child=d->nodes[extra].first;child!=WENA_JSON_NONE;child=d->nodes[child].next)
            if(!descriptor_valid(d,child,seen))return 0;
        return 1;
    }
    if(kind==WENA_SJSON_BINARY)return d->nodes[descriptor].count==2&&field(d,descriptor,"s",&extra)&&integer(d,extra,"255",NULL);
    if(kind==WENA_SJSON_REGEX)return d->nodes[descriptor].count==2&&field(d,descriptor,"o",&extra)&&d->nodes[extra].kind==WENA_JSON_STRING;
    return d->nodes[descriptor].count==1;
}
static int base64_digit(unsigned char c)
{return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='+'||c=='/';}
static int binary_valid(const char *s,size_t length)
{
    size_t i,used;unsigned char group[4],c;int ended;
    used=0;ended=0;
    for(i=0;i<length;++i){
        c=(unsigned char)s[i];if(c=='\r'||c=='\n')continue;
        if(ended)return 0;group[used++]=c;
        if(used!=4)continue;
        if(!base64_digit(group[0])||!base64_digit(group[1])||
            (!base64_digit(group[2])&&group[2]!='=')||
            (!base64_digit(group[3])&&group[3]!='=')||
            (group[2]=='='&&group[3]!='='))return 0;
        ended=group[3]=='=';used=0;
    }
    return used==0;
}
static int finite_number(const WenaJsonDocument *d,size_t index)
{
    const WenaJsonNode *n;const char *decimal;char *text,*end;size_t size,i,used;double value;int valid;
    n=&d->nodes[index];if(n->kind!=WENA_JSON_NUMBER)return 0;
    decimal=localeconv()->decimal_point;size=strlen(decimal);
    if(!size||size>16u)return 0;
    text=(char*)malloc(n->length+size+1u);if(!text)return 0;
    used=0;
    for(i=0;i<n->length;++i){
        if(d->raw[n->start+i]=='.'){memcpy(text+used,decimal,size);used+=size;}
        else text[used++]=d->raw[n->start+i];
    }
    text[used]=0;errno=0;value=strtod(text,&end);
    valid=end==text+used&&value<=DBL_MAX&&value>=-DBL_MAX&&
        !(errno==ERANGE&&(value==DBL_MAX||value==-DBL_MAX));
    free(text);return valid;
}
static int scalar_valid(const WenaJsonDocument *d,size_t index,WenaSjsonKind kind)
{
    const WenaJsonNode *n;const char *s;size_t length,i;unsigned char c;
    n=&d->nodes[index];s=wena_json_string(d,index,&length);
    switch(kind){
    case WENA_SJSON_STRING:case WENA_SJSON_REGEX:return s!=NULL;
    case WENA_SJSON_BOOL:return n->kind==WENA_JSON_TRUE||n->kind==WENA_JSON_FALSE;
    case WENA_SJSON_NULL:return n->kind==WENA_JSON_NULL;
    case WENA_SJSON_INT32:return integer(d,index,"2147483647","2147483648");
    case WENA_SJSON_DATE:case WENA_SJSON_INT64:return integer(d,index,"9223372036854775807","9223372036854775808");
    case WENA_SJSON_TIMESTAMP:return integer(d,index,"18446744073709551615",NULL);
    case WENA_SJSON_DOUBLE:
        return (n->kind==WENA_JSON_STRING&&n->length==5&&!memcmp(d->raw+n->start,"\"NaN\"",5))||finite_number(d,index);
    case WENA_SJSON_BINARY:return s&&binary_valid(s,length);
    case WENA_SJSON_OBJECT_ID:
        if(!s||length!=24)return 0;
        for(i=0;i<length;++i){c=(unsigned char)s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}
        return 1;
    default:return 0;
    }
}
static size_t read_value(WenaSjsonDocument*,size_t,size_t,size_t,int);
static void append(WenaSjsonDocument *d,size_t parent,size_t *last,size_t child)
{
    if(*last==WENA_JSON_NONE)d->values[parent].first=child;
    else d->values[*last].next=child;
    *last=child;++d->values[parent].count;
}
static int read_object(WenaSjsonDocument *out,size_t parent,size_t value,size_t schema,int root)
{
    WenaJsonDocument *d;size_t properties,keys,key,descriptor,child,data,last,length;const char *name;
    d=out->json;
    if(d->nodes[value].kind!=WENA_JSON_OBJECT)return 0;
    if(!d->nodes[schema].count)return d->nodes[value].count==(root?1u:0u);
    if(!field(d,schema,"p",&properties)||!field(d,schema,"$k",&keys)||
        d->nodes[value].count!=d->nodes[keys].count+(root?1u:0u))return 0;
    last=WENA_JSON_NONE;
    for(key=d->nodes[keys].first;key!=WENA_JSON_NONE;key=d->nodes[key].next){
        name=wena_json_string(d,key,&length);
        if(!name||(root&&length==2&&!memcmp(name,"$s",2))||
            !wena_json_member(d,properties,name,length,&descriptor)||
            !wena_json_member(d,value,name,length,&data))return 0;
        child=read_value(out,data,descriptor,key,0);if(child==WENA_JSON_NONE)return 0;
        append(out,parent,&last,child);
    }
    return 1;
}
static size_t read_value(WenaSjsonDocument *out,size_t data,size_t descriptor,size_t name,int root)
{
    WenaJsonDocument *d;WenaSjsonKind kind;size_t index,extra,element,child,last,item;
    d=out->json;kind=root?WENA_SJSON_OBJECT:type(d,descriptor);
    if(out->count>=d->count)return WENA_JSON_NONE;
    index=out->count++;out->values[index].kind=kind;out->values[index].value=data;
    out->values[index].schema=descriptor;out->values[index].name=name;
    out->values[index].first=out->values[index].next=WENA_JSON_NONE;
    /* Pinned FerretDB interprets JSON null as BSON null even with another
     * known descriptor (notably a nil binary slice). Retain original metadata. */
    if(!root&&d->nodes[data].kind==WENA_JSON_NULL){out->values[index].kind=WENA_SJSON_NULL;return index;}
    if(kind==WENA_SJSON_OBJECT){
        extra=descriptor;if(!root&&!field(d,descriptor,"$s",&extra))return WENA_JSON_NONE;
        if(!read_object(out,index,data,extra,root))return WENA_JSON_NONE;
    }else if(kind==WENA_SJSON_ARRAY){
        if(d->nodes[data].kind!=WENA_JSON_ARRAY||!field(d,descriptor,"i",&extra)||
            d->nodes[data].count!=d->nodes[extra].count)return WENA_JSON_NONE;
        last=WENA_JSON_NONE;element=d->nodes[extra].first;
        for(item=d->nodes[data].first;item!=WENA_JSON_NONE;item=d->nodes[item].next){
            child=read_value(out,item,element,WENA_JSON_NONE,0);if(child==WENA_JSON_NONE)return child;
            append(out,index,&last,child);element=d->nodes[element].next;
        }
    }else if(!scalar_valid(d,data,kind))return WENA_JSON_NONE;
    return index;
}
void wena_sjson_free(WenaSjsonDocument *d)
{if(d){wena_json_free(d->json);free(d->values);free(d);}}
int wena_sjson_parse(const char *input,size_t length,WenaSjsonDocument **output)
{
    WenaSjsonDocument *d;unsigned char *seen;size_t schema;int valid;
    if(!output)return 0;
    d=(WenaSjsonDocument*)calloc(1,sizeof(*d));if(!d)return 0;
    if(!wena_json_parse(input,length,&d->json)||!field(d->json,0,"$s",&schema)){wena_sjson_free(d);return 0;}
    seen=(unsigned char*)calloc(d->json->count,1);if(!seen){wena_sjson_free(d);return 0;}
    valid=schema_valid(d->json,schema,seen);free(seen);
    if(!valid){wena_sjson_free(d);return 0;}
    d->values=(WenaSjsonValue*)calloc(d->json->count,sizeof(*d->values));
    if(!d->values||read_value(d,0,schema,WENA_JSON_NONE,1)==WENA_JSON_NONE){wena_sjson_free(d);return 0;}
    wena_sjson_free(*output);*output=d;return 1;
}
int wena_sjson_member(const WenaSjsonDocument *d,size_t object,const char *key,size_t length,size_t *value)
{
    size_t child,n;const char *name;
    if(!d||!key||!value||object>=d->count||d->values[object].kind!=WENA_SJSON_OBJECT)return 0;
    for(child=d->values[object].first;child!=WENA_JSON_NONE;child=d->values[child].next){
        name=wena_json_string(d->json,d->values[child].name,&n);
        if(name&&n==length&&!memcmp(name,key,length)){*value=child;return 1;}
    }
    return 0;
}
