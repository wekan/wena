CREATE TABLE checklists (
  id TEXT NOT NULL PRIMARY KEY CHECK (typeof(id) = 'text' AND length(CAST(id AS BLOB)) BETWEEN 1 AND 64 AND instr(id, char(0)) = 0),
  board_id TEXT NOT NULL,
  card_id TEXT NOT NULL,
  title TEXT NOT NULL CHECK (typeof(title) = 'text' AND length(CAST(title AS BLOB)) BETWEEN 1 AND 128 AND instr(title, char(0)) = 0),
  position INTEGER NOT NULL CHECK (typeof(position) = 'integer' AND position BETWEEN 0 AND 2147483647),
  hide_checked_items INTEGER NOT NULL DEFAULT 0 CHECK (typeof(hide_checked_items) = 'integer' AND hide_checked_items IN (0, 1)),
  hide_all_items INTEGER NOT NULL DEFAULT 0 CHECK (typeof(hide_all_items) = 'integer' AND hide_all_items IN (0, 1)),
  show_on_minicard INTEGER DEFAULT NULL CHECK (show_on_minicard IS NULL OR (typeof(show_on_minicard) = 'integer' AND show_on_minicard IN (0, 1))),
  version INTEGER NOT NULL DEFAULT 1 CHECK (typeof(version) = 'integer' AND version > 0),
  created_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(created_at) = 'integer' AND created_at >= 0),
  updated_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(updated_at) = 'integer' AND updated_at >= 0),
  UNIQUE (board_id, card_id, id),
  UNIQUE (card_id, position),
  FOREIGN KEY (board_id, card_id) REFERENCES cards(board_id, id) ON DELETE RESTRICT
);
CREATE TABLE checklist_items (
  id TEXT NOT NULL PRIMARY KEY CHECK (typeof(id) = 'text' AND length(CAST(id AS BLOB)) BETWEEN 1 AND 64 AND instr(id, char(0)) = 0),
  board_id TEXT NOT NULL,
  card_id TEXT NOT NULL,
  checklist_id TEXT NOT NULL,
  title TEXT NOT NULL CHECK (typeof(title) = 'text' AND length(CAST(title AS BLOB)) BETWEEN 1 AND 128 AND instr(title, char(0)) = 0),
  position INTEGER NOT NULL CHECK (typeof(position) = 'integer' AND position BETWEEN 0 AND 2147483647),
  is_finished INTEGER NOT NULL DEFAULT 0 CHECK (typeof(is_finished) = 'integer' AND is_finished IN (0, 1)),
  version INTEGER NOT NULL DEFAULT 1 CHECK (typeof(version) = 'integer' AND version > 0),
  created_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(created_at) = 'integer' AND created_at >= 0),
  updated_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(updated_at) = 'integer' AND updated_at >= 0),
  UNIQUE (checklist_id, position),
  FOREIGN KEY (board_id, card_id, checklist_id) REFERENCES checklists(board_id, card_id, id) ON DELETE RESTRICT
);
