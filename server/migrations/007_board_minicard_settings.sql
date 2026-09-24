CREATE TABLE board_minicard_settings (
  board_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  show_checklists INTEGER NOT NULL DEFAULT 1 CHECK (typeof(show_checklists) = 'integer' AND show_checklists IN (0, 1)),
  FOREIGN KEY (board_id) REFERENCES boards(id) ON DELETE RESTRICT
);
