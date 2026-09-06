BEGIN IMMEDIATE;
CREATE TABLE schema_migrations (
  version INTEGER PRIMARY KEY,
  checksum TEXT NOT NULL UNIQUE,
  applied_at INTEGER NOT NULL
);
CREATE TABLE actors (
  id TEXT PRIMARY KEY,
  display_name TEXT NOT NULL,
  version INTEGER NOT NULL CHECK (version > 0)
);
CREATE TABLE sessions (
  id TEXT PRIMARY KEY,
  actor_id TEXT NOT NULL REFERENCES actors(id) ON DELETE RESTRICT,
  expires_at INTEGER NOT NULL,
  revoked INTEGER NOT NULL DEFAULT 0 CHECK (revoked IN (0, 1))
);
CREATE INDEX sessions_actor_idx ON sessions(actor_id);
CREATE TABLE boards (
  id TEXT PRIMARY KEY,
  title TEXT NOT NULL,
  version INTEGER NOT NULL CHECK (version > 0)
);
CREATE TABLE swimlanes (
  id TEXT PRIMARY KEY,
  board_id TEXT NOT NULL REFERENCES boards(id) ON DELETE RESTRICT,
  title TEXT NOT NULL,
  position INTEGER NOT NULL CHECK (position >= 0),
  version INTEGER NOT NULL CHECK (version > 0),
  UNIQUE (board_id, id),
  UNIQUE (board_id, position)
);
CREATE TABLE lists (
  id TEXT PRIMARY KEY,
  board_id TEXT NOT NULL REFERENCES boards(id) ON DELETE RESTRICT,
  title TEXT NOT NULL,
  position INTEGER NOT NULL CHECK (position >= 0),
  version INTEGER NOT NULL CHECK (version > 0),
  UNIQUE (board_id, id),
  UNIQUE (board_id, position)
);
CREATE TABLE cards (
  id TEXT PRIMARY KEY,
  board_id TEXT NOT NULL REFERENCES boards(id) ON DELETE RESTRICT,
  swimlane_id TEXT NOT NULL,
  list_id TEXT NOT NULL,
  title TEXT NOT NULL,
  position INTEGER NOT NULL CHECK (position >= 0),
  archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),
  version INTEGER NOT NULL CHECK (version > 0),
  UNIQUE (list_id, swimlane_id, position),
  FOREIGN KEY (board_id, swimlane_id) REFERENCES swimlanes(board_id, id) ON DELETE RESTRICT,
  FOREIGN KEY (board_id, list_id) REFERENCES lists(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX cards_board_idx ON cards(board_id);
CREATE INDEX cards_swimlane_idx ON cards(swimlane_id);
CREATE TABLE idempotency_keys (
  actor_id TEXT NOT NULL REFERENCES actors(id) ON DELETE RESTRICT,
  route TEXT NOT NULL,
  operation TEXT NOT NULL,
  request_version INTEGER NOT NULL CHECK (request_version > 0),
  response_checksum TEXT NOT NULL,
  committed_at INTEGER NOT NULL,
  PRIMARY KEY (actor_id, route, operation, request_version)
);
PRAGMA user_version = 1;
COMMIT;
