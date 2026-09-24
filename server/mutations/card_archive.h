#ifndef WENA_MUTATION_CARD_ARCHIVE_H
#define WENA_MUTATION_CARD_ARCHIVE_H
#include <sqlite3.h>
/* Caller owns snapshot/transaction. Zero expected accepts any valid revision.
 * Missing legacy metadata means unknown time (zero); failure preserves outputs. */
int wena_sqlite_card_archive_read(sqlite3 *db,const char *board,const char *card,
    unsigned long expected,int *archived,sqlite3_int64 *at);
/* Monotonic timestamp after prior, using the database clock. Reject overflow. */
int wena_sqlite_archive_time_after(sqlite3 *db,sqlite3_int64 prior,sqlite3_int64 *at);
/* Caller owns guarded transaction. Preserve time on restore. A floor allows a
 * swimlane cascade to stamp cards strictly after its own archive timestamp.
 * Legacy schemas retain flag-only behavior. No-op/stale states are rejected. */
int wena_sqlite_card_archive_change(sqlite3 *db,const char *board,const char *card,
    unsigned long expected,int archived,sqlite3_int64 floor);
#endif
