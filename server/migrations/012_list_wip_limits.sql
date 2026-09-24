CREATE TABLE list_wip_limits (
  list_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(list_id) = 'text' AND length(CAST(list_id AS BLOB)) BETWEEN 1 AND 64 AND instr(list_id, char(0)) = 0),
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  value INTEGER NOT NULL DEFAULT 1 CHECK (typeof(value) = 'integer' AND value BETWEEN 1 AND 2147483647),
  enabled INTEGER NOT NULL DEFAULT 0 CHECK (typeof(enabled) = 'integer' AND enabled IN (0, 1)),
  soft INTEGER NOT NULL DEFAULT 0 CHECK (typeof(soft) = 'integer' AND soft IN (0, 1)),
  FOREIGN KEY (board_id, list_id) REFERENCES lists(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX list_wip_limits_board_idx ON list_wip_limits(board_id, list_id);
