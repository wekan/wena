CREATE TABLE list_colors (
  list_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(list_id) = 'text' AND length(CAST(list_id AS BLOB)) BETWEEN 1 AND 64 AND instr(list_id, char(0)) = 0),
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  color TEXT NOT NULL DEFAULT '' CHECK (typeof(color) = 'text' AND instr(color, char(0)) = 0 AND (color IN ('', 'white', 'green', 'yellow', 'orange', 'red', 'purple', 'blue', 'sky', 'lime', 'pink', 'black', 'silver', 'peachpuff', 'crimson', 'plum', 'darkgreen', 'slateblue', 'magenta', 'gold', 'navy', 'gray', 'saddlebrown', 'paleturquoise', 'mistyrose', 'indigo') OR (length(CAST(color AS BLOB)) = 7 AND substr(color, 1, 1) = '#' AND substr(color, 2) NOT GLOB '*[^0-9a-fA-F]*'))),
  FOREIGN KEY (board_id, list_id) REFERENCES lists(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX list_colors_board_idx ON list_colors(board_id, list_id);
CREATE TABLE swimlane_colors (
  swimlane_id TEXT NOT NULL PRIMARY KEY CHECK (typeof(swimlane_id) = 'text' AND length(CAST(swimlane_id AS BLOB)) BETWEEN 1 AND 64 AND instr(swimlane_id, char(0)) = 0),
  board_id TEXT NOT NULL CHECK (typeof(board_id) = 'text' AND length(CAST(board_id AS BLOB)) BETWEEN 1 AND 64 AND instr(board_id, char(0)) = 0),
  color TEXT NOT NULL DEFAULT '' CHECK (typeof(color) = 'text' AND instr(color, char(0)) = 0 AND (color IN ('', 'white', 'green', 'yellow', 'orange', 'red', 'purple', 'blue', 'sky', 'lime', 'pink', 'black', 'silver', 'peachpuff', 'crimson', 'plum', 'darkgreen', 'slateblue', 'magenta', 'gold', 'navy', 'gray', 'saddlebrown', 'paleturquoise', 'mistyrose', 'indigo') OR (length(CAST(color AS BLOB)) = 7 AND substr(color, 1, 1) = '#' AND substr(color, 2) NOT GLOB '*[^0-9a-fA-F]*'))),
  FOREIGN KEY (board_id, swimlane_id) REFERENCES swimlanes(board_id, id) ON DELETE RESTRICT
);
CREATE INDEX swimlane_colors_board_idx ON swimlane_colors(board_id, swimlane_id);
