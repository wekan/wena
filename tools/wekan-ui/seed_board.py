#!/usr/bin/env python3
"""Give a new Wena workspace the board WeKan's capture shows.

    tools/wekan-ui/seed_board.py DATABASE

DATABASE is a workspace the desktop has just created (its one board, lane
and list). It becomes the board of WeKan's Playwright fixture
(tests/playwright/fixtures.js): "E2E Board" with the swimlane "Default",
lists A, B and C, and the cards Alpha, Beta and Gamma - so the two can be
compared screen for screen.
"""
import sqlite3
import sys


def main(argv):
    if len(argv) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    db = sqlite3.connect(argv[0])
    board, = db.execute("SELECT id FROM boards").fetchone()
    lane, = db.execute("SELECT id FROM swimlanes WHERE board_id = ?", (board,)).fetchone()
    first, = db.execute("SELECT id FROM lists WHERE board_id = ?", (board,)).fetchone()
    with db:
        db.execute("UPDATE boards SET title = 'E2E Board', version = version + 1 WHERE id = ?", (board,))
        db.execute("UPDATE swimlanes SET title = 'Default', version = version + 1 WHERE id = ?", (lane,))
        db.execute("UPDATE lists SET title = 'List A', version = version + 1 WHERE id = ?", (first,))
        lists = [first]
        for position, title in ((1, "List B"), (2, "List C")):
            ident = "list-" + title[-1].lower()
            db.execute("INSERT INTO lists (id, board_id, title, position, version) VALUES (?, ?, ?, ?, 1)",
                       (ident, board, title, position))
            lists.append(ident)
        for list_id, title in zip(lists, ("Alpha Card", "Beta Card", "Gamma Card")):
            db.execute("INSERT INTO cards (id, board_id, swimlane_id, list_id, title, position, archived, version)"
                       " VALUES (?, ?, ?, ?, ?, 0, 0, 1)",
                       ("card-" + title.split()[0].lower(), board, lane, list_id, title))
    print(f"seeded {argv[0]}: E2E Board, Default, List A/B/C, Alpha/Beta/Gamma Card")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
