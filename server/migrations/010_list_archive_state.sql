CREATE TABLE list_archive_state (
  list_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(list_id) = 'text' AND length(CAST(list_id AS BLOB)) BETWEEN 1 AND 64 AND instr(list_id, char(0)) = 0),
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  archived INTEGER NOT NULL DEFAULT 0 CHECK (typeof(archived) = 'integer' AND archived IN (0, 1)),
  archived_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(archived_at) = 'integer' AND archived_at >= 0),
  FOREIGN KEY (board_id, list_id) REFERENCES lists(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX list_archive_board_idx ON list_archive_state(board_id, archived, list_id);
