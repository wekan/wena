CREATE TABLE board_settings (
  board_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  show_checklist_count INTEGER NOT NULL DEFAULT 0 CHECK (typeof(show_checklist_count) = 'integer' AND show_checklist_count IN (0, 1)),
  FOREIGN KEY (board_id) REFERENCES boards(id) ON DELETE RESTRICT
);
