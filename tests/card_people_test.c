#include "../models/card_people.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void member(WenaBoardMember *m,const char *board,const char *actor,int active)
{memset(m,0,sizeof(*m));strcpy(m->board_id,board);strcpy(m->actor_id,actor);m->active=active;}
static void exhaustive(WenaCardPeople *p,WenaCardPeople *out,WenaBoardMember *members)
{
    unsigned int mask,enabled,active_mask;size_t i,j,n;int changed,found;char id[2];
    id[1]=0;
    for(mask=0;mask<8;++mask)for(active_mask=0;active_mask<8;++active_mask){
        assert(wena_card_people_init(p,"board"));
        for(i=0;i<3;++i){
            id[0]=(char)('a'+i);member(&members[i],"board",id,(active_mask&(1u<<i))!=0);
            if(mask&(1u<<i))strcpy(p->ids[p->count++],id);
        }
        for(i=0;i<3;++i)for(enabled=0;enabled<2;++enabled){
            id[0]=(char)('a'+i);changed=99;memset(out,0x5a,sizeof(*out));
            if(enabled&&!(active_mask&(1u<<i))){
                assert(!wena_card_people_set(p,members,3,id,(int)enabled,out,&changed));
                assert(changed==99);
                for(j=0;j<sizeof(*out);++j)assert(((unsigned char*)out)[j]==0x5a);
                continue;
            }
            assert(wena_card_people_set(p,members,3,id,(int)enabled,out,&changed));
            found=(mask&(1u<<i))!=0;assert(changed==(found!=(int)enabled));n=0;
            for(j=0;j<p->count;++j)if(enabled||strcmp(p->ids[j],id))assert(!strcmp(out->ids[n++],p->ids[j]));
            if(enabled&&!found)assert(!strcmp(out->ids[n++],id));
            assert(n==out->count&&wena_card_people_valid(out));
            assert(wena_card_people_set(out,members,3,id,(int)enabled,out,&changed)&&!changed);
        }
        for(i=0;i<3;++i)strcpy(members[i].board_id,"target");
        assert(wena_card_people_transfer(p,members,3,"target",out));n=0;
        for(i=0;i<3;++i)if((mask&active_mask)&(1u<<i)){id[0]=(char)('a'+i);assert(!strcmp(out->ids[n++],id));}
        assert(n==out->count&&!strcmp(out->board_id,"target"));
    }
}
int main(void)
{
    WenaCardPeople *p,*out,*before;WenaBoardMember *members;size_t i;int changed;char id[32];
    p=(WenaCardPeople*)malloc(sizeof(*p));out=(WenaCardPeople*)malloc(sizeof(*out));
    before=(WenaCardPeople*)malloc(sizeof(*before));members=(WenaBoardMember*)calloc(WENA_BOARD_MEMBER_CAPACITY,sizeof(*members));
    assert(p&&out&&before&&members);exhaustive(p,out,members);
    assert(wena_card_people_init(p,"board"));
    assert(wena_card_people_set(p,NULL,0,"absent",0,p,&changed)&&!changed&&!p->count);
    member(&members[0],"board","alice",1);member(&members[1],"board","bob",0);
    assert(wena_card_people_set(p,members,2,"alice",1,p,&changed)&&changed);
    *before=*p;changed=99;
    assert(!wena_card_people_set(p,members,2,"bob",1,p,&changed)&&changed==99&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_set(p,members,2,"absent",1,p,&changed)&&changed==99&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_set(p,members,2,"bad/id",0,p,&changed)&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_set(p,members,2,"alice",2,p,&changed)&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_set(p,NULL,2,"alice",0,p,&changed)&&!memcmp(p,before,sizeof(*p)));
    members[1].active=2;assert(!wena_card_people_set(p,members,2,"alice",0,p,&changed));members[1].active=0;
    strcpy(members[1].actor_id,"alice");assert(!wena_card_people_set(p,members,2,"alice",0,p,&changed));
    member(&members[1],"foreign","bob",1);assert(!wena_card_people_set(p,members,2,"alice",0,p,&changed));
    assert(!memcmp(p,before,sizeof(*p))&&changed==99);
    assert(!wena_card_people_transfer(p,members,2,"target",p)&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_transfer(p,members,1,"board",p)&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_transfer(p,NULL,1,"target",p)&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_transfer(p,members,WENA_BOARD_MEMBER_CAPACITY+1,"target",p));
    members[0].active=0;assert(wena_card_people_set(p,members,1,p->ids[0],0,p,&changed)&&changed&&!p->count);
    assert(wena_card_people_set(p,NULL,0,"alice",0,p,&changed)&&!changed);
    assert(wena_card_people_transfer(p,NULL,0,"empty",p)&&!strcmp(p->board_id,"empty")&&!p->count);
    assert(wena_card_people_init(p,p->board_id)&&!strcmp(p->board_id,"empty"));
    *before=*p;assert(!wena_card_people_init(p,"bad/id")&&!memcmp(p,before,sizeof(*p)));
    /* Bounded validation rejects unterminated identifiers, duplicate sets and overflow. */
    p->count=1;memset(p->ids[0],'x',sizeof(WenaId));assert(!wena_card_people_valid(p));
    strcpy(p->ids[0],"alice");strcpy(p->ids[1],"alice");p->count=2;assert(!wena_card_people_valid(p));
    p->count=WENA_CARD_PEOPLE_CAPACITY+1;assert(!wena_card_people_valid(p));
    *before=*p;assert(!wena_card_people_transfer(p,NULL,0,"target",p)&&!memcmp(p,before,sizeof(*p)));
    assert(!wena_card_people_valid(NULL)&&!wena_board_members_valid(NULL,1,"board"));
    assert(wena_card_people_init(p,"source"));
    for(i=0;i<WENA_CARD_PEOPLE_CAPACITY;++i){
        sprintf(id,"person%lu",(unsigned long)i);strcpy(p->ids[p->count++],id);
        member(&members[WENA_CARD_PEOPLE_CAPACITY-1-i],"target",id,1);
    }
    assert(wena_card_people_transfer(p,members,WENA_BOARD_MEMBER_CAPACITY,"target",out));
    assert(out->count==p->count&&!memcmp(out->ids,p->ids,sizeof(p->ids)));
    assert(wena_card_people_set(out,members,WENA_BOARD_MEMBER_CAPACITY,out->ids[2047],1,out,&changed)&&!changed);
    /* A full card can contain departed members: adding a new eligible member
     * must fail rather than silently drop one of the retained IDs. */
    member(&members[0],"source","new",1);*before=*p;changed=99;
    assert(!wena_card_people_set(p,members,1,"new",1,p,&changed)&&changed==99&&!memcmp(p,before,sizeof(*p)));
    assert(wena_card_people_set(p,members,1,p->ids[0],0,p,&changed)&&changed&&p->count==2047);
    assert(wena_card_people_set(p,members,1,"new",1,p,&changed)&&changed&&p->count==2048&&!strcmp(p->ids[2047],"new"));
    assert(wena_card_people_transfer(p,NULL,0,"target",p)&&!p->count);
    free(p);free(out);free(before);free(members);
    puts("Shared card people eligibility, stable sets, transfers and capacity passed");return 0;
}
