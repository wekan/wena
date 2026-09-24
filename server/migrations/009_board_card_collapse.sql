CREATE TABLE board_card_collapse_settings (
  board_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  allow_collapse INTEGER NOT NULL DEFAULT 1 CHECK (typeof(allow_collapse) = 'integer' AND allow_collapse IN (0, 1)),
  FOREIGN KEY (board_id) REFERENCES boards(id) ON DELETE RESTRICT
);
