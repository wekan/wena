CREATE TABLE swimlane_archive_state (
  swimlane_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(swimlane_id) = 'text' AND length(CAST(swimlane_id AS BLOB)) BETWEEN 1 AND 64 AND instr(swimlane_id, char(0)) = 0),
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  archived INTEGER NOT NULL DEFAULT 0 CHECK (typeof(archived) = 'integer' AND archived IN (0, 1)),
  archived_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(archived_at) = 'integer' AND archived_at >= 0),
  FOREIGN KEY (board_id, swimlane_id) REFERENCES swimlanes(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX swimlane_archive_board_idx ON swimlane_archive_state(board_id, archived, swimlane_id);
CREATE TABLE card_archive_state (
  card_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(card_id) = 'text' AND length(CAST(card_id AS BLOB)) BETWEEN 1 AND 64 AND instr(card_id, char(0)) = 0),
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  archived_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(archived_at) = 'integer' AND archived_at >= 0),
  FOREIGN KEY (board_id, card_id) REFERENCES cards(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX card_archive_board_idx ON card_archive_state(board_id, card_id, archived_at);
