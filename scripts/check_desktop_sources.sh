#!/usr/bin/env sh
# The checks every desktop build runs before compiling: pinned dependencies,
# compiled-in SVGs, migrations, translations, font and licenses are what the sources say.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$root_dir/scripts/check_dependencies.py" > /dev/null
python3 "$root_dir/scripts/compile_svg.py" --check
python3 "$root_dir/scripts/verify_migrations.py"
python3 "$root_dir/scripts/verify_i18n_catalog.py"
python3 "$root_dir/scripts/generate_ui_i18n.py" --check
python3 "$root_dir/scripts/generate_native_font.py" --check
python3 "$root_dir/scripts/generate_notices.py" --check
