# Canonical offline translations

`wekan-i18n.bin` is generated, not a separately maintained text catalog. Its only
source is `wekan/imports/i18n/data/*.i18n.json` at the revision pinned in
`config/i18n-lock.json`. The bundle records every source filename and SHA-256,
retains canonical English key order and exact translated UTF-8 values, and is
compressed to keep every single-file Wena executable within the documented 12 MiB
translation-data budget.

The translations and generator are distributed under WeKan's MIT license,
copyright Lauri Ojansivu and contributors. Regenerate from a checkout of the pinned
revision with:

```sh
python3 scripts/generate_i18n_catalog.py \
  --source ../wekan/imports/i18n/data \
  --source-revision REVISION \
  --output imports/i18n/wekan-i18n.bin
```

Update the lock deliberately after regeneration. Every local and CI target build
runs `scripts/verify_i18n_catalog.py`; missing, corrupt, incomplete, unexpectedly
large, or lock-mismatched data stops the build before compilation.

## Runtime translations for implemented UI

`ui_catalog.c` looks up the control, page-heading, and common UI text keys declared
in `imports/ui/page_contract.c`. `ui_catalog_data.h` is a deterministic generated
subset of the same pinned binary catalog for every canonical language. It adds
no manually maintained translations and needs no runtime decompressor, heap
allocation, or additional dependency. Returned UTF-8 strings have static lifetime;
the language state is read on each lookup so switching language immediately
changes subsequent labels. Unknown languages and empty translated values fall
back to canonical English; unknown keys return `NULL` for the caller's fallback.
The existing locale resolver preserves exact canonical tags, including underscore
and modifier spellings, before normalizing detected operating-system locales.

Regenerate after adding a canonical key to the implemented UI contract:

```sh
python3 scripts/generate_ui_i18n.py
python3 scripts/generate_ui_i18n.py --check
sh tests/test_ui_catalog.sh
```

The generator verifies the pinned catalog identity and key order, checks selected
placeholder inventories, and rejects parameterized UI keys until a formatter is
implemented. The desktop build checks the generated data before compilation.
The runtime test compares every compiled translation byte against the canonical
source values and tests persisted selection of every catalog language plus
language changes through the optional page-contract translation callback. This
subset does not claim translation of every planned feature, font coverage, or
complete bidirectional shaping support.
