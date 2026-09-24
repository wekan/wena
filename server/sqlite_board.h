#ifndef WENA_SQLITE_BOARD_H
#define WENA_SQLITE_BOARD_H

#include "../models/board.h"
#include "../models/swimlane.h"
#include "../models/list.h"
#include "../models/card.h"
#include <sqlite3.h>

#define WENA_SQLITE_BOARD_MAX_SWIMLANES 64u
#define WENA_SQLITE_BOARD_MAX_LISTS 128u
#define WENA_SQLITE_BOARD_MAX_CARDS 2048u

/* Allocate this owned snapshot on the heap on small-stack platforms. Lists in
 * schema v1 are board-wide (empty model swimlane_id); cards retain their lane.
 * Archived cards are included with their flag for callers' archive views. */
typedef struct WenaSqliteBoardSnapshot {
    WenaBoard board;
    WenaSwimlane swimlanes[WENA_SQLITE_BOARD_MAX_SWIMLANES];
    size_t swimlane_count;
    WenaList lists[WENA_SQLITE_BOARD_MAX_LISTS];
    size_t list_count;
    WenaCard cards[WENA_SQLITE_BOARD_MAX_CARDS];
    size_t card_count;
} WenaSqliteBoardSnapshot;

/* Read only; caller is responsible for authenticating/authorizing board access.
 * Owns a consistent read transaction and refuses a caller-owned transaction.
 * Any failure, including capacity exhaustion, leaves output byte-for-byte intact.
 * Does not open/migrate databases, create actors, or grant authorization. */
/* Read into caller-owned scratch inside an existing transaction. No allocation,
 * commit or rollback. Failure may partially overwrite scratch; publish it only
 * after both this read and the surrounding transaction's commit succeed. */
int wena_sqlite_board_read_transaction(sqlite3 *database,const char *board_id,
    WenaSqliteBoardSnapshot *scratch);
int wena_sqlite_board_load(sqlite3 *database, const char *board_id,
                           WenaSqliteBoardSnapshot *output);
#endif
