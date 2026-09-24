CREATE TABLE board_members (
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0 AND board_id NOT GLOB '*[^A-Za-z0-9_-]*'),
  actor_id TEXT NOT NULL CHECK (typeof(actor_id) = 'text' AND length(CAST(actor_id AS BLOB)) BETWEEN 1 AND 64 AND instr(actor_id, char(0)) = 0 AND actor_id NOT GLOB '*[^A-Za-z0-9_-]*'),
  active INTEGER NOT NULL DEFAULT 1 CHECK (typeof(active) = 'integer' AND active IN (0, 1)),
  version INTEGER NOT NULL DEFAULT 1 CHECK (typeof(version) = 'integer' AND version > 0),
  created_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(created_at) = 'integer' AND created_at >= 0),
  updated_at INTEGER NOT NULL DEFAULT 0 CHECK (typeof(updated_at) = 'integer' AND updated_at >= created_at),
  PRIMARY KEY (board_id, actor_id),
  FOREIGN KEY (board_id) REFERENCES boards(id) ON DELETE RESTRICT,
  FOREIGN KEY (actor_id) REFERENCES actors(id) ON DELETE RESTRICT
);
CREATE INDEX board_members_actor_idx ON board_members(actor_id, active, board_id);
CREATE TABLE card_people (
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0 AND board_id NOT GLOB '*[^A-Za-z0-9_-]*'),
  card_id TEXT NOT NULL CHECK (typeof(card_id) = 'text' AND length(CAST(card_id AS BLOB)) BETWEEN 1 AND 64 AND instr(card_id, char(0)) = 0 AND card_id NOT GLOB '*[^A-Za-z0-9_-]*'),
  field TEXT NOT NULL CHECK (typeof(field) = 'text' AND field IN ('members', 'assignees') AND instr(field, char(0)) = 0),
  actor_id TEXT NOT NULL CHECK (typeof(actor_id) = 'text' AND length(CAST(actor_id AS BLOB)) BETWEEN 1 AND 64 AND instr(actor_id, char(0)) = 0 AND actor_id NOT GLOB '*[^A-Za-z0-9_-]*'),
  position INTEGER NOT NULL CHECK (typeof(position) = 'integer' AND position BETWEEN 0 AND 2147483647),
  PRIMARY KEY (board_id, card_id, field, actor_id),
  UNIQUE (board_id, card_id, field, position),
  FOREIGN KEY (board_id, card_id) REFERENCES cards(board_id, id) ON DELETE RESTRICT,
  FOREIGN KEY (actor_id) REFERENCES actors(id) ON DELETE RESTRICT
);
CREATE INDEX card_people_actor_idx ON card_people(board_id, actor_id, field, card_id);
CREATE INDEX card_people_card_order_idx ON card_people(card_id, field, position, board_id, actor_id);
