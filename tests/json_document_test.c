#include "../imports/json/document.h"
#include "../models/text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void reject(WenaJsonDocument **doc,const char *text,size_t length)
{
 WenaJsonDocument *before;before=*doc;assert(!wena_json_parse(text,length,doc)&&*doc==before);
 assert(!strcmp((*doc)->raw,"{\"kept\":true}"));
}
static void structure(const WenaJsonDocument *d)
{
 size_t i,j,count;const WenaJsonNode *n;
 assert(d->count&&d->count<=WENA_JSON_MAX_NODES);
 for(i=0;i<d->count;++i){
  n=&d->nodes[i];assert(n->start<=d->length&&n->length<=d->length-n->start);
  if(n->next!=WENA_JSON_NONE)assert(n->next>i&&n->next<d->count);
  if(n->kind==WENA_JSON_STRING){assert(n->text_start<=d->string_length&&n->text_length<d->string_length-n->text_start);assert(d->strings[n->text_start+n->text_length]==0);}
  if(n->kind==WENA_JSON_OBJECT||n->kind==WENA_JSON_ARRAY){
   count=0;for(j=n->first;j!=WENA_JSON_NONE;j=d->nodes[j].next){assert(j>i&&j<d->count);++count;}
   assert(count==n->count*(n->kind==WENA_JSON_OBJECT?2u:1u));
  }else assert(n->first==WENA_JSON_NONE&&!n->count);
 }
}
int main(void)
{
 WenaJsonDocument *doc,*old;size_t index,length,i,j,at;unsigned long cp;const char *text;
 char *large,depth[80],mutated[160];unsigned char raw_nul[]={'"','a',0,'b','"'};
 const char *bad[]={""," ","{}{}","true false","[1,]","{\"x\":1,}","{x:1}","[", "{", "[1 2]",
  "{\"a\" 1}","01","-01","+1",".5","1.","1e","1e+","--1","NaN","Infinity","nul","TRUE","[/*comment*/1]",
  "\"unterminated","\"\\x41\"","\"\\u123\"","\"\\uGGGG\"","\"\\ud800\"","\"\\udc00\"","\"\\ud800\\u0041\"",
  "\"\n\"","\"\300\257\"","\"\355\240\200\"","\"\364\220\200\200\"","\"\200\"","\"\342\202\"",
  "{\"a\":1,\"\\u0061\":2}","{\"\":1,\"\":2}","{\"x\":{\"a\":1,\"a\":2}}","\357\273\277{}"};
 doc=NULL;
 text=" {\"n\":18446744073709551615,\"a\":[-0,1.20e+4,false,null,{}],\"s\":\"A\\u0000\\uD83D\\uDE00\\n\\t\\b\\f\\r\\/\\\\\\\"\",\"\":true} \r\n";
 assert(wena_json_parse(text,strlen(text),&doc));structure(doc);
 assert(doc->nodes[0].kind==WENA_JSON_OBJECT&&doc->nodes[0].count==4);
 assert(wena_json_member(doc,0,"n",1,&index)&&doc->nodes[index].kind==WENA_JSON_NUMBER);
 assert(doc->nodes[index].length==20&&!memcmp(doc->raw+doc->nodes[index].start,"18446744073709551615",20));
 assert(wena_json_member(doc,0,"a",1,&index)&&doc->nodes[index].count==5);
 assert(wena_json_member(doc,0,"s",1,&index));text=wena_json_string(doc,index,&length);
 assert(text&&length==14&&!memcmp(text,"A\0\360\237\230\200\n\t\b\f\r/\\\"",14));
 assert(wena_json_member(doc,0,"",0,&index)&&doc->nodes[index].kind==WENA_JSON_TRUE);
 index=999;assert(!wena_json_member(doc,0,"missing",7,&index)&&index==999);
 length=999;assert(!wena_json_string(doc,0,&length)&&length==999);
 assert(wena_json_parse("{\"a\\u0000b\":1}",14,&doc));assert(wena_json_member(doc,0,"a\0b",3,&index));
 strcpy(mutated,"[\"owned\",{\"v\":1}]");assert(wena_json_parse(mutated,strlen(mutated),&doc));strcpy(mutated,"changed");assert(!strcmp(doc->raw,"[\"owned\",{\"v\":1}]"));
 assert(wena_json_parse("{\"kept\":true}",13,&doc));
 for(i=0;i<sizeof(bad)/sizeof(bad[0]);++i)reject(&doc,bad[i],strlen(bad[i]));
 reject(&doc,(const char*)raw_nul,sizeof(raw_nul));reject(&doc,NULL,1);reject(&doc,"{}",(size_t)-1);
 for(i=0;i<32;++i)depth[i]='[';depth[32]='0';for(i=0;i<32;++i)depth[33+i]=']';
 assert(wena_json_parse(depth,65,&doc));structure(doc);
 memmove(depth+1,depth,65);depth[0]='[';depth[66]=']';old=doc;assert(!wena_json_parse(depth,67,&doc)&&doc==old);
 large=(char*)malloc(WENA_JSON_MAX_BYTES+1UL);assert(large);
 large[0]='[';at=1;
 for(i=0;i<WENA_JSON_MAX_NODES-1u;++i){if(i)large[at++]=',';large[at++]='0';}
 large[at++]=']';assert(wena_json_parse(large,at,&doc)&&doc->count==WENA_JSON_MAX_NODES);structure(doc);
 large[at-1]=',';large[at++]='0';large[at++]=']';old=doc;assert(!wena_json_parse(large,at,&doc)&&doc==old);
 memset(large,'a',WENA_JSON_MAX_BYTES);large[0]='"';large[WENA_JSON_MAX_BYTES-1]='"';
 assert(wena_json_parse(large,WENA_JSON_MAX_BYTES,&doc)&&doc->nodes[0].text_length==WENA_JSON_MAX_BYTES-2UL);free(large);
 /* Exercise every byte substitution, including invalid UTF-8 and separators.
  * Any successful mutation must still produce a bounded acyclic token tree. */
 text="{\"a\":[0,1.5,true,null,\"\\u0061\"],\"b\":{\"c\":\"text\"}}";
 for(i=0;i<strlen(text);++i)for(j=0;j<256u;++j){
  strcpy(mutated,text);mutated[i]=(char)j;
  if(wena_json_parse(mutated,strlen(text),&doc))structure(doc);
 }
 at=0;cp=99;assert(wena_text_utf8_next("\360\237\230\200",4,&at,&cp)&&at==4&&cp==128512UL);
 at=0;cp=99;assert(!wena_text_utf8_next("\355\240\200",3,&at,&cp)&&!at&&cp==99);
 assert(!wena_text_utf8_next("",0,&at,&cp));
 wena_json_free(doc);wena_json_free(NULL);
 puts("Shared JSON reader: exact numbers/order, Unicode, duplicate keys, bounds, atomic replacement and byte-mutation corpus passed");return 0;
}
