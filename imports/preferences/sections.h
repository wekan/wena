#ifndef WENA_CARD_SECTION_PREFERENCES_H
#define WENA_CARD_SECTION_PREFERENCES_H
#include "../../models/card_section.h"
#include "../../server/sqlite_board.h"
/* *output starts NULL; failed reads preserve the old owned immutable snapshot.
 * One scoped read transaction, bounded to 128 entries per card, no frame I/O. */
int wena_card_sections_load(sqlite3 *database,const char *actor,const char *board,
    WenaCardSectionsSnapshot **output);
/* A local presentation preference, not a canonical card/board mutation.
 * Expected version zero means absent. No-op does not insert/increment anything.
 * Requires an active scoped card, actor and matching preference revision.
 * Does not change card/board revisions; caller refreshes only after success. */
int wena_card_section_save(sqlite3 *database,const char *actor,const char *board,
    const char *card,const char *key,unsigned long expected,int collapsed);
#endif
