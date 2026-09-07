CREATE TABLE labels (
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  id TEXT NOT NULL CHECK (typeof(id) = 'text' AND length(CAST(id AS BLOB)) BETWEEN 1 AND 64 AND instr(id, char(0)) = 0),
  name TEXT NOT NULL DEFAULT '' CHECK (typeof(name) = 'text' AND length(CAST(name AS BLOB)) <= 128 AND instr(name, char(0)) = 0),
  color TEXT NOT NULL DEFAULT '' CHECK (typeof(color) = 'text' AND instr(color, char(0)) = 0 AND (color IN ('', 'white', 'green', 'yellow', 'orange', 'red', 'purple', 'blue', 'sky', 'lime', 'pink', 'black', 'silver', 'peachpuff', 'crimson', 'plum', 'darkgreen', 'slateblue', 'magenta', 'gold', 'navy', 'gray', 'saddlebrown', 'paleturquoise', 'mistyrose', 'indigo') OR (length(CAST(color AS BLOB)) = 7 AND substr(color, 1, 1) = '#' AND substr(color, 2) NOT GLOB '*[^0-9a-fA-F]*'))),
  position INTEGER NOT NULL CHECK (typeof(position) = 'integer' AND position BETWEEN 0 AND 2147483647),
  version INTEGER NOT NULL DEFAULT 1 CHECK (typeof(version) = 'integer' AND version > 0),
  created_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(created_at) = 'integer' AND created_at >= 0),
  updated_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(updated_at) = 'integer' AND updated_at >= created_at),
  PRIMARY KEY (board_id, id),
  UNIQUE (board_id, position),
  UNIQUE (board_id, name, color),
  FOREIGN KEY (board_id) REFERENCES boards(id) ON DELETE RESTRICT
);
CREATE TABLE card_labels (
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  card_id TEXT NOT NULL CHECK (typeof(card_id) = 'text' AND length(CAST(card_id AS BLOB)) BETWEEN 1 AND 64 AND instr(card_id, char(0)) = 0),
  label_id TEXT NOT NULL CHECK (typeof(label_id) = 'text' AND length(CAST(label_id AS BLOB)) BETWEEN 1 AND 64 AND instr(label_id, char(0)) = 0),
  PRIMARY KEY (board_id, card_id, label_id),
  FOREIGN KEY (board_id, card_id) REFERENCES cards(board_id, id) ON DELETE RESTRICT,
  FOREIGN KEY (board_id, label_id) REFERENCES labels(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX card_labels_label_cards_idx ON card_labels(board_id, label_id, card_id);
CREATE INDEX card_labels_card_order_idx ON card_labels(card_id, label_id, board_id);
