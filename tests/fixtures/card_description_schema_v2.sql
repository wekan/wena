CREATE UNIQUE INDEX cards_board_id_unique ON cards(board_id, id);
CREATE TABLE card_descriptions (
  card_id TEXT NOT NULL PRIMARY KEY,
  board_id TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '' CHECK (
    typeof(description) = 'text' AND
    length(CAST(description AS BLOB)) <= 1024 AND
    instr(description, char(0)) = 0
  ),
  FOREIGN KEY (board_id, card_id) REFERENCES cards(board_id, id) ON DELETE RESTRICT
);
