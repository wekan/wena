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
for suite in models board_feature collapse sqlite_board card_mutation card_editor_sqlite card_create card_create_persistence card_move_persistence card_move card_move_sqlite card_create_sqlite sqlite_workspace ui_catalog nuklear_card_create nuklear_card_move language_picker sqlite_hierarchy_create hierarchy_title card_restore_persistence card_archives card_archives_sqlite nuklear_card_archives native_theme sdl_text_input; do
  printf 'Sanitizers: %s\n' "$suite"
  sh "$root_dir/tests/test_$suite.sh"
done
