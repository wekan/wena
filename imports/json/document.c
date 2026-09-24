#include "document.h"
#include "../../models/text.h"
#include <stdlib.h>
#include <string.h>
typedef struct Parser {WenaJsonDocument *doc;size_t offset;} Parser;
static void space(Parser *p)
{
    while(p->offset<p->doc->length) {
        char c;c=p->doc->raw[p->offset];
        if(c!=' ' && c!='\t' && c!='\r' && c!='\n') break;
        ++p->offset;
    }
}
static size_t node(Parser *p,WenaJsonKind kind)
{
    WenaJsonDocument *d;WenaJsonNode *grown;size_t index,capacity;
    d=p->doc;if(d->count==WENA_JSON_MAX_NODES)return WENA_JSON_NONE;
    if(d->count==d->capacity){
        capacity=d->capacity?d->capacity*2u:32u;
        if(capacity>WENA_JSON_MAX_NODES)capacity=WENA_JSON_MAX_NODES;
        grown=(WenaJsonNode*)realloc(d->nodes,capacity*sizeof(*grown));
        if(!grown)return WENA_JSON_NONE;
        d->nodes=grown;d->capacity=capacity;
    }
    index=d->count++;memset(&d->nodes[index],0,sizeof(d->nodes[index]));
    d->nodes[index].kind=kind;d->nodes[index].start=p->offset;
    d->nodes[index].first=d->nodes[index].next=WENA_JSON_NONE;
    return index;
}
static int hex4(Parser *p,unsigned long *value)
{
    size_t i;unsigned long v;unsigned char c;
    if(p->doc->length-p->offset<4u)return 0;
    v=0;
    for(i=0;i<4u;++i){
        c=(unsigned char)p->doc->raw[p->offset++];v*=16UL;
        if(c>='0'&&c<='9')v+=(unsigned long)(c-'0');
        else if(c>='a'&&c<='f')v+=(unsigned long)(c-'a'+10);
        else if(c>='A'&&c<='F')v+=(unsigned long)(c-'A'+10);
        else return 0;
    }
    *value=v;return 1;
}
static void scalar(WenaJsonDocument *d,unsigned long cp)
{
    char *s;s=d->strings+d->string_length;
    if(cp<128UL){s[0]=(char)cp;d->string_length+=1u;}
    else if(cp<2048UL){s[0]=(char)(192UL|(cp>>6));s[1]=(char)(128UL|(cp&63UL));d->string_length+=2u;}
    else if(cp<65536UL){s[0]=(char)(224UL|(cp>>12));s[1]=(char)(128UL|((cp>>6)&63UL));s[2]=(char)(128UL|(cp&63UL));d->string_length+=3u;}
    else{s[0]=(char)(240UL|(cp>>18));s[1]=(char)(128UL|((cp>>12)&63UL));s[2]=(char)(128UL|((cp>>6)&63UL));s[3]=(char)(128UL|(cp&63UL));d->string_length+=4u;}
}
static size_t string(Parser *p)
{
    size_t index,start;unsigned long cp,low;unsigned char c;WenaJsonDocument *d;
    d=p->doc;index=node(p,WENA_JSON_STRING);if(index==WENA_JSON_NONE)return index;
    start=d->string_length;d->nodes[index].text_start=start;++p->offset;
    while(p->offset<d->length){
        c=(unsigned char)d->raw[p->offset];
        if(c=='"'){
            ++p->offset;d->nodes[index].length=p->offset-d->nodes[index].start;
            d->nodes[index].text_length=d->string_length-start;
            d->strings[d->string_length++]=0;return index;
        }
        if(c<32)return WENA_JSON_NONE;
        if(c=='\\'){
            if(++p->offset==d->length)return WENA_JSON_NONE;
            c=(unsigned char)d->raw[p->offset++];
            if(c=='"'||c=='\\'||c=='/')cp=c;
            else if(c=='b')cp=8;else if(c=='f')cp=12;else if(c=='n')cp=10;
            else if(c=='r')cp=13;else if(c=='t')cp=9;
            else if(c=='u'){
                if(!hex4(p,&cp))return WENA_JSON_NONE;
                if(cp>=55296UL&&cp<=56319UL){
                    if(d->length-p->offset<6u || d->raw[p->offset]!='\\' || d->raw[p->offset+1]!='u')return WENA_JSON_NONE;
                    p->offset+=2u;
                    if(!hex4(p,&low)||low<56320UL||low>57343UL)return WENA_JSON_NONE;
                    cp=65536UL+((cp-55296UL)<<10)+(low-56320UL);
                }else if(cp>=56320UL&&cp<=57343UL)return WENA_JSON_NONE;
            }else return WENA_JSON_NONE;
        }else if(!wena_text_utf8_next(d->raw,d->length,&p->offset,&cp))return WENA_JSON_NONE;
        /* Decoding never expands source bytes; each string terminator consumes
         * less space than its two source quotes, even for an escaped NUL. */
        scalar(d,cp);
    }
    return WENA_JSON_NONE;
}
static int digit(char c){return c>='0'&&c<='9';}
static int number(Parser *p)
{
    size_t start;const char *s;size_t n;
    s=p->doc->raw;n=p->doc->length;
    if(s[p->offset]=='-' && ++p->offset==n)return 0;
    if(s[p->offset]=='0')++p->offset;
    else{
        if(s[p->offset]<'1'||s[p->offset]>'9')return 0;
        while(p->offset<n&&digit(s[p->offset]))++p->offset;
    }
    if(p->offset<n&&s[p->offset]=='.'){
        start=++p->offset;while(p->offset<n&&digit(s[p->offset]))++p->offset;
        if(start==p->offset)return 0;
    }
    if(p->offset<n&&(s[p->offset]=='e'||s[p->offset]=='E')){
        ++p->offset;if(p->offset<n&&(s[p->offset]=='+'||s[p->offset]=='-'))++p->offset;
        start=p->offset;while(p->offset<n&&digit(s[p->offset]))++p->offset;
        if(start==p->offset)return 0;
    }
    return 1;
}
static int equal_key(const WenaJsonDocument *d,size_t a,size_t b)
{
    return d->nodes[a].text_length==d->nodes[b].text_length &&
        !memcmp(d->strings+d->nodes[a].text_start,d->strings+d->nodes[b].text_start,d->nodes[a].text_length);
}
static void append(WenaJsonDocument *d,size_t parent,size_t *last,size_t child)
{
    if(*last==WENA_JSON_NONE)d->nodes[parent].first=child;
    else d->nodes[*last].next=child;
    *last=child;
}
static size_t value(Parser *p,unsigned int depth)
{
    size_t index,key,child,last,previous;WenaJsonKind kind;char c,end;const char *literal;
    space(p);if(p->offset==p->doc->length || depth>WENA_JSON_MAX_DEPTH)return WENA_JSON_NONE;
    c=p->doc->raw[p->offset];if(c=='"')return string(p);
    if(c=='{'||c=='['){
        kind=c=='{'?WENA_JSON_OBJECT:WENA_JSON_ARRAY;end=c=='{'?'}':']';
        index=node(p,kind);if(index==WENA_JSON_NONE)return index;
        ++p->offset;space(p);last=WENA_JSON_NONE;
        if(p->offset<p->doc->length&&p->doc->raw[p->offset]==end)++p->offset;
        else for(;;){
            if(kind==WENA_JSON_OBJECT){
                if(p->offset==p->doc->length||p->doc->raw[p->offset]!='"')return WENA_JSON_NONE;
                key=string(p);if(key==WENA_JSON_NONE)return key;
                for(previous=p->doc->nodes[index].first;previous!=WENA_JSON_NONE;
                    previous=p->doc->nodes[p->doc->nodes[previous].next].next)
                    if(equal_key(p->doc,previous,key))return WENA_JSON_NONE;
                space(p);if(p->offset==p->doc->length||p->doc->raw[p->offset++]!=':')return WENA_JSON_NONE;
                append(p->doc,index,&last,key);
            }
            child=value(p,depth+1u);if(child==WENA_JSON_NONE)return child;
            append(p->doc,index,&last,child);++p->doc->nodes[index].count;
            space(p);if(p->offset==p->doc->length)return WENA_JSON_NONE;
            c=p->doc->raw[p->offset++];if(c==end)break;
            if(c!=',')return WENA_JSON_NONE;
            space(p);
        }
    }else{
        literal=NULL;
        if(c=='t'){kind=WENA_JSON_TRUE;literal="true";}
        else if(c=='f'){kind=WENA_JSON_FALSE;literal="false";}
        else if(c=='n'){kind=WENA_JSON_NULL;literal="null";}
        else if(c=='-'||digit(c))kind=WENA_JSON_NUMBER;
        else return WENA_JSON_NONE;
        index=node(p,kind);if(index==WENA_JSON_NONE)return index;
        if(literal){
            child=strlen(literal);
            if(child>p->doc->length-p->offset||memcmp(p->doc->raw+p->offset,literal,child))return WENA_JSON_NONE;
            p->offset+=child;
        }else if(!number(p))return WENA_JSON_NONE;
    }
    p->doc->nodes[index].length=p->offset-p->doc->nodes[index].start;return index;
}
void wena_json_free(WenaJsonDocument *d)
{if(d){free(d->raw);free(d->strings);free(d->nodes);free(d);}}
int wena_json_parse(const char *input,size_t length,WenaJsonDocument **output)
{
    Parser p;WenaJsonDocument *candidate;
    if(!input||!output||!length||length>WENA_JSON_MAX_BYTES)return 0;
    candidate=(WenaJsonDocument*)calloc(1,sizeof(*candidate));if(!candidate)return 0;
    candidate->raw=(char*)malloc(length+1u);candidate->strings=(char*)malloc(length+1u);
    if(!candidate->raw||!candidate->strings){wena_json_free(candidate);return 0;}
    memcpy(candidate->raw,input,length);candidate->raw[length]=0;candidate->length=length;
    p.doc=candidate;p.offset=0;
    if(value(&p,0)==WENA_JSON_NONE){wena_json_free(candidate);return 0;}
    space(&p);if(p.offset!=length){wena_json_free(candidate);return 0;}
    wena_json_free(*output);*output=candidate;return 1;
}
const char *wena_json_string(const WenaJsonDocument *d,size_t index,size_t *length)
{
    if(!d||!length||index>=d->count||d->nodes[index].kind!=WENA_JSON_STRING)return NULL;
    *length=d->nodes[index].text_length;return d->strings+d->nodes[index].text_start;
}
int wena_json_member(const WenaJsonDocument *d,size_t object,const char *key,
    size_t length,size_t *out)
{
    size_t index;
    if(!d||!key||!out||object>=d->count||d->nodes[object].kind!=WENA_JSON_OBJECT)return 0;
    for(index=d->nodes[object].first;index!=WENA_JSON_NONE;
        index=d->nodes[d->nodes[index].next].next)
        if(d->nodes[index].text_length==length&&!memcmp(d->strings+d->nodes[index].text_start,key,length)){
            *out=d->nodes[index].next;return 1;
        }
    return 0;
}
