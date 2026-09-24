CREATE TABLE actor_card_sections (
  actor_id TEXT NOT NULL CHECK (typeof(actor_id) = 'text' AND length(CAST(actor_id AS BLOB)) BETWEEN 1 AND 64 AND instr(actor_id, char(0)) = 0),
  card_id TEXT NOT NULL CHECK (typeof(card_id) = 'text' AND length(CAST(card_id AS BLOB)) BETWEEN 1 AND 64 AND instr(card_id, char(0)) = 0),
  section_key TEXT NOT NULL CHECK (typeof(section_key) = 'text' AND length(CAST(section_key AS BLOB)) BETWEEN 1 AND 128 AND instr(section_key, char(0)) = 0),
  collapsed INTEGER NOT NULL CHECK (typeof(collapsed) = 'integer' AND collapsed IN (0, 1)),
  version INTEGER NOT NULL DEFAULT 1 CHECK (typeof(version) = 'integer' AND version > 0),
  PRIMARY KEY (actor_id, card_id, section_key),
  FOREIGN KEY (actor_id) REFERENCES actors(id) ON DELETE RESTRICT,
  FOREIGN KEY (card_id) REFERENCES cards(id) ON DELETE RESTRICT
);
