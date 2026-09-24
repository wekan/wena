#ifndef WENA_CARD_PEOPLE_STORE_H
#define WENA_CARD_PEOPLE_STORE_H
#include "../models/card_people.h"
#include <sqlite3.h>
#define WENA_PERSON_MEMBERS 0
#define WENA_PERSON_ASSIGNEES 1
#define WENA_PERSON_FIELD_COUNT 2
typedef struct WenaMemberRoster {
    WenaId board_id;
    unsigned long board_version;
    size_t count;
    WenaBoardMember members[WENA_BOARD_MEMBER_CAPACITY];
    WenaTitle names[WENA_BOARD_MEMBER_CAPACITY];
    unsigned long versions[WENA_BOARD_MEMBER_CAPACITY],actor_versions[WENA_BOARD_MEMBER_CAPACITY];
    sqlite3_int64 created[WENA_BOARD_MEMBER_CAPACITY],updated[WENA_BOARD_MEMBER_CAPACITY];
} WenaMemberRoster;
typedef struct WenaCardPeopleSnapshot {
    WenaId card_id,list_id,swimlane_id;
    WenaTitle title;
    sqlite3_int64 position;
    unsigned long board_version,card_version;
    int archived;
    WenaCardPeople fields[WENA_PERSON_FIELD_COUNT];
    unsigned long positions[WENA_PERSON_FIELD_COUNT][WENA_CARD_PEOPLE_CAPACITY];
} WenaCardPeopleSnapshot;
/* Strict complete reads within a caller-owned transaction. Include inactive
 * roster entries and both card fields; retain order gaps and readable terminal
 * revisions. Reject malformed, dangling, foreign-scope and over-capacity rows.
 * Allocate these large output structures on the heap. Failed reads preserve
 * all output bytes and never end the caller's transaction. No writes, implicit
 * membership grants, authentication or authorization. */
int wena_sqlite_member_roster_read(sqlite3 *db,const char *board,WenaMemberRoster *output);
int wena_sqlite_card_people_read(sqlite3 *db,const char *board,const char *card,WenaCardPeopleSnapshot *output);
/* Transfer-only read while deferred foreign keys allow child rows to move
 * before their parent card. Metadata/version still describe parent_board;
 * field scopes must exactly match assignment_board. Normal callers use read. */
int wena_sqlite_card_people_read_staged(sqlite3 *db,const char *parent_board,
    const char *card,const char *assignment_board,WenaCardPeopleSnapshot *output);
#endif
