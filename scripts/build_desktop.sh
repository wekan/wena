#!/usr/bin/env sh
# Optional host desktop build; release bootstrap targets remain separate.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
  echo "Usage: scripts/build_desktop.sh OUTPUT_EXECUTABLE" >&2
  exit 2
fi
command -v sdl2-config >/dev/null 2>&1 || { echo "SDL2 development files required" >&2; exit 1; }
python3 "$root_dir/scripts/verify_migrations.py"
python3 "$root_dir/scripts/verify_i18n_catalog.py"
python3 "$root_dir/scripts/generate_ui_i18n.py" --check
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -DNK_INPUT_MAX=256 \
  -I"$root_dir/third_party/nuklear" $(sdl2-config --cflags) \
  "$root_dir/client/desktop.c" "$root_dir/client/platform/sdl_nuklear.c" \
  "$root_dir/client/platform/theme.c" \
  "$root_dir/client/features/board.c" "$root_dir/client/features/card_details.c" \
  "$root_dir/client/features/card_archives.c" \
  "$root_dir/client/features/hierarchy_title.c" "$root_dir/client/features/hierarchy_mutation.c" \
  "$root_dir/client/features/language_picker.c" "$root_dir/client/features/card_move.c" \
  "$root_dir/client/features/card_mutation.c" "$root_dir/client/features/card_create.c" \
  "$root_dir/client/components/boards/board_layout.c" \
  "$root_dir/client/components/boards/board_header.c" \
  "$root_dir/client/components/sidebar/board_sidebar.c" \
  "$root_dir/client/components/lists/list_header.c" \
  "$root_dir/client/components/cards/card_body.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/models/model.c" "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" "$root_dir/models/list.c" "$root_dir/models/card.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/imports/i18n/catalog.c" \
  "$root_dir/imports/i18n/ui_catalog.c" "$root_dir/imports/i18n/locale.c" \
  "$root_dir/imports/i18n/language.c" \
  "$root_dir/server/sqlite_board.c" "$root_dir/server/sqlite_storage.c" \
  "$root_dir/server/sqlite_workspace.c" \
  "$root_dir/server/sqlite_persistence.c" "$root_dir/server/region_response.c" \
  "$root_dir/server/embedded_migration.c" "$root_dir/server/executable_path.c" \
  "$root_dir/server/sha256.c" -o "$1" $(sdl2-config --libs) -lsqlite3 -lm
python3 "$root_dir/scripts/embed_migrations.py" --executable "$1"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$1"
