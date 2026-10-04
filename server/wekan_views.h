#ifndef WENA_SERVER_WEKAN_VIEWS_H
#define WENA_SERVER_WEKAN_VIEWS_H
/* The records WeKan's board views read, from WeKan's documents in the
 * attached FerretDB file (server/wekan_sync.h): the board's cards with every
 * field the views use, its lists (not archived, by sort), swimlanes, labels,
 * the users, its activities and its card change history - the same records
 * server/lib/boardChartData.js loads. Cards removed with a recorded snapshot
 * are included with their deletedAt, as WeKan's withRemovedCards does. */
#include <sqlite3.h>
#include "../models/view_data.h"

int wena_wekan_views_load(sqlite3 *db, const char *board, WenaViewData *data);

/* The cards of the boards `actor` is an active member of, not archived, each
 * with its board's title: the calendar of several boards. */
int wena_wekan_views_load_all(sqlite3 *db, const char *actor, WenaViewData *data);

#endif
