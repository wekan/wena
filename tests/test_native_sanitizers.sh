#!/usr/bin/env sh
# Opt-in instrumented run of existing native model/UI/SQLite regressions.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-sanitizers-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
WENA_SANITIZER_CC=$(command -v cc)
export WENA_SANITIZER_CC
mkdir "$test_dir/bin"
cat > "$test_dir/bin/cc" <<'CC'
#!/usr/bin/env sh
exec "$WENA_SANITIZER_CC" -fsanitize=address,undefined -fno-omit-frame-pointer -g "$@"
CC
chmod +x "$test_dir/bin/cc"
PATH="$test_dir/bin:$PATH"
export PATH
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}
UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}
export ASAN_OPTIONS UBSAN_OPTIONS
for suite in checklist_item_move nuklear_checklist_item_move svg checklist_move nuklear_checklist_move board_presentation sqlite_schema_v6 nuklear_checklist_order nuklear_board_settings board_settings_panel_sqlite checklist_badges checklist_order board_settings board_settings_panel checklist_summary label_badges version_boundaries labels_sqlite nuklear_labels sqlite_schema_v5 nuklear_checklist_batch labels_mutation labels checklist_batch sqlite_schema_v4 sqlite_schema_v3 sqlite_schema_v2 checklists nuklear_checklists checklists_sqlite collapse_preferences board_filter checklist_models checklist_item_titles checklist_mutation checklist_delete colors model_text models board_feature collapse parser_mutations sqlite_storage sqlite_hardening sqlite_backup sqlite_restore sqlite_board card_description nuklear_card_description card_description_sqlite card_description_mutation card_mutation card_editor_sqlite card_create card_create_persistence card_move_persistence card_move_reorder_ui nuklear_card_reorder card_move_reorder_sqlite card_reorder card_move card_move_sqlite card_create_sqlite sqlite_workspace ui_catalog nuklear_card_create panel_escape nuklear_title_keys nuklear_card_move language_picker language_storage sqlite_hierarchy_create hierarchy_title hierarchy_move_sqlite hierarchy_move_mutation hierarchy_move nuklear_hierarchy_move card_restore_persistence card_archives card_archives_sqlite nuklear_card_archives native_font native_theme native_feature_i18n dependency_report sdl_text_input; do
  printf 'Sanitizers: %s\n' "$suite"
  sh "$root_dir/tests/test_$suite.sh"
done
