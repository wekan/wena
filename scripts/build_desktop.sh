#!/usr/bin/env sh
# Optional host desktop build; release bootstrap targets remain separate.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
  echo "Usage: scripts/build_desktop.sh OUTPUT_EXECUTABLE" >&2
  exit 2
fi
command -v sdl2-config >/dev/null 2>&1 || { echo "SDL2 development files required" >&2; exit 1; }
python3 "$root_dir/scripts/check_dependencies.py" > /dev/null
python3 "$root_dir/scripts/compile_svg.py" --check
python3 "$root_dir/scripts/verify_migrations.py"
python3 "$root_dir/scripts/verify_i18n_catalog.py"
python3 "$root_dir/scripts/generate_ui_i18n.py" --check
python3 "$root_dir/scripts/generate_native_font.py" --check
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -DNK_INPUT_MAX=256 \
  -I"$root_dir/third_party/nuklear" $(sdl2-config --cflags) \
  "$root_dir/client/desktop.c" "$root_dir/client/platform/sdl_nuklear.c" \
  "$root_dir/imports/preferences/collapse.c" \
  "$root_dir/client/platform/font.c" \
  "$root_dir/client/platform/svg.c" "$root_dir/client/platform/theme.c" "$root_dir/client/platform/dependencies.c" \
  "$root_dir/client/features/boards/settings.c" "$root_dir/client/features/boards/settings_store.c" \
  "$root_dir/client/features/boards/settings_panel.c" "$root_dir/client/features/boards/presentation.c" \
  "$root_dir/client/features/board_filter.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/client/features/board.c" "$root_dir/client/features/card_details.c" \
  "$root_dir/client/features/checklists.c" "$root_dir/client/features/checklist_store.c" \
  "$root_dir/client/features/checklist_mutation.c" "$root_dir/client/features/checklists/summary.c" "$root_dir/client/features/checklists/badges.c" \
  "$root_dir/client/features/labels/panel.c" "$root_dir/client/features/labels/component.c" "$root_dir/client/features/labels/badges.c" \
  "$root_dir/client/features/labels/store.c" "$root_dir/client/features/labels/mutation.c" \
  "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/client/features/card_archives.c" \
  "$root_dir/client/features/card_description.c" "$root_dir/client/features/card_description_mutation.c" \
  "$root_dir/client/features/hierarchy_move.c" "$root_dir/client/features/hierarchy_move_mutation.c" \
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
  "$root_dir/server/sqlite_persistence.c" \
  "$root_dir/server/mutations/checklist_order.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/server/region_response.c" \
  "$root_dir/server/embedded_migration.c" "$root_dir/server/executable_path.c" \
  "$root_dir/server/sha256.c" -o "$1" $(sdl2-config --libs) -lsqlite3 -lm
python3 "$root_dir/scripts/embed_migrations.py" --executable "$1"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$1"
