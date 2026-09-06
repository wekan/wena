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
