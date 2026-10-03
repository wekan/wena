#ifndef WENA_SERVER_BOARD_SEARCH_H
#define WENA_SERVER_BOARD_SEARCH_H
/* WeKan's board Search (Boards.searchLists, searchCards): the board's lists
 * and cards whose title - or a card's description - contains the term,
 * ignoring case, newest first as WeKan sorts them (Wena keeps no creation
 * time, so the last made, the highest rowid, comes first). */
#include <sqlite3.h>
#include <stddef.h>
#include "../models/model.h"

int wena_board_search(sqlite3 *db, const char *board, const char *term,
                      WenaId *lists, size_t list_capacity, size_t *list_count,
                      WenaId *cards, size_t card_capacity, size_t *card_count);

#endif
