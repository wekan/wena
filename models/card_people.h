#ifndef WENA_CARD_PEOPLE_H
#define WENA_CARD_PEOPLE_H
#include "model.h"
#define WENA_BOARD_MEMBER_CAPACITY 2048u
#define WENA_CARD_PEOPLE_CAPACITY WENA_BOARD_MEMBER_CAPACITY
/* Assignment eligibility only, not actor authentication or write permission.
 * Role/authorization checks belong to the enclosing operation. */
typedef struct WenaBoardMember {
    WenaId board_id,actor_id;
    int active;
} WenaBoardMember;
typedef struct WenaCardPeople {
    WenaId board_id;
    size_t count;
    WenaId ids[WENA_CARD_PEOPLE_CAPACITY];
} WenaCardPeople;
/* Complete roster, with unique actor IDs. Order is not significant. */
int wena_board_members_valid(const WenaBoardMember *members,size_t count,const char *board);
int wena_card_people_init(WenaCardPeople *people,const char *board);
int wena_card_people_valid(const WenaCardPeople *people);
/* Shared member/assignee set operation, preserving existing order and appending
 * new IDs. Adding requires an active roster entry, including a repeated add;
 * removal also works for departed members. No-op succeeds with changed=0.
 * Output may alias input; failure preserves output and changed. Caller owns
 * bounded output storage (prefer heap); internal scratch is heap allocated. */
int wena_card_people_set(const WenaCardPeople *people,const WenaBoardMember *members,
    size_t count,const char *actor,int enabled,WenaCardPeople *output,int *changed);
/* WeKan Cards.move membership policy: retain original-order IDs active on the
 * destination board. Do not apply this implicitly to assignees or other fields.
 * Requires a different board; output may alias input, failure preserves output. */
int wena_card_people_transfer(const WenaCardPeople *people,const WenaBoardMember *members,
    size_t count,const char *target_board,WenaCardPeople *output);
#endif
