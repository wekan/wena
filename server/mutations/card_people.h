#ifndef WENA_MUTATION_CARD_PEOPLE_H
#define WENA_MUTATION_CARD_PEOPLE_H
#include "../card_people_store.h"
#include "common.h"
/* Shared preflight over strict reader snapshots: 0 invalid, 1 changed, 2 no-op.
 * Does not write or inspect the database. Output survives failure. */
int wena_card_person_plan(const WenaMemberRoster *roster,const WenaCardPeopleSnapshot *current,
    int field,const char *actor,int enabled,WenaCardPeopleSnapshot *output);
/* Exact native selection, including a one-card span; caller owns transaction. */
int wena_sqlite_selected_people_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version);
/* Caller-owned guarded write transaction; 0 failure, 1 changed, 2 no-op.
 * Revalidate exact roster/card captures and active card parents. Add/remove one
 * person in either field through the same writer. Preserve order gaps, append
 * at last position+1, and advance only a changed card's revision. Verify both
 * fields, card metadata and unchanged roster after writes. Output can alias
 * expected; it is published only on success and never on failure.
 * Caller supplies authentication, authorization, idempotency, the final board
 * revision advance and complete batch verification. Roll back the ENTIRE
 * transaction after any failure; no partial changes may be committed. Output
 * remains staged until caller's commit succeeds. */
int wena_sqlite_card_person_set(sqlite3 *db,const WenaMemberRoster *roster,
    const WenaCardPeopleSnapshot *expected,int field,const char *actor,int enabled,
    WenaCardPeopleSnapshot *output);
/* Caller-owned cross-board transaction: filter members by active target
 * membership, preserve assignees and original positions. Move only child scopes;
 * caller subsequently moves the parent and advances card/board revisions before
 * commit, or rolls everything back. FK enforcement is deferred, never disabled. */
int wena_sqlite_card_people_reboard(sqlite3 *db,const char *board,const char *card,const char *target);
#endif
