#!/usr/bin/env python3
"""Shared local and CI command dispatcher for Wena."""

from concurrent.futures import ThreadPoolExecutor
import os
import platform
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config" / "targets.tsv"


def targets():
    result = []
    for number, line in enumerate(CATALOG.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise SystemExit(f"targets.tsv:{number}: expected five fields")
        result.append(dict(zip(("target", "name", "runner", "status", "kind"), fields)))
    return result


def host_target(system=None, machine=None):
    system = (system or platform.system()).lower()
    machine = (machine or platform.machine()).lower()
    machines = {
        "x86_64": "amd64", "amd64": "amd64", "aarch64": "arm64",
        "arm64": "arm64", "armv7l": "armhf", "armv8l": "armhf",
    }
    cpu = machines.get(machine)
    systems = {"linux": "linux", "darwin": "macos", "windows": "windows"}
    operating_system = systems.get(system)
    if not operating_system or not cpu:
        raise SystemExit(f"unsupported current host: {system}/{machine}")
    return f"{operating_system}-{cpu}"


def shell_command(script):
    if sys.platform == "win32":
        shell = shutil.which("sh")
        if not shell:
            raise SystemExit("target build requires sh from Git for Windows or MSYS2")
        return [shell, str(script)]
    return [str(script)]


def build_one(target):
    record = next((item for item in targets() if item["target"] == target), None)
    if record is None:
        raise SystemExit(f"unknown target: {target}")
    if record["status"] != "ready":
        raise SystemExit(f"target is cataloged but not ready: {target}")
    script = ROOT / ".github" / "release" / f"{target}.sh"
    if not script.is_file():
        raise SystemExit(f"{target}: ready target is missing {script.relative_to(ROOT)}")
    if sys.platform != "win32" and not script.stat().st_mode & 0o111:
        raise SystemExit(f"{target}: {script.relative_to(ROOT)} exists but is not executable")
    verification = subprocess.call(
        [sys.executable, str(ROOT / "scripts" / "verify_i18n_catalog.py")], cwd=ROOT
    )
    if verification:
        return verification
    verification = subprocess.call(
        [sys.executable, str(ROOT / "scripts" / "verify_migrations.py")], cwd=ROOT
    )
    if verification:
        return verification
    print(f"Building {record['name']} ({target})", flush=True)
    return subprocess.call(shell_command(script), cwd=ROOT)


def build(selection):
    if selection == "desktop-package":
        return subprocess.call([sys.executable, str(ROOT / "scripts/package_desktop.py")], cwd=ROOT)
    if selection == "desktop":
        print("Building local SDL2/SQLite desktop app (create or open workspace)", flush=True)
        output = ROOT / "dist" / "desktop" / ("wena-desktop.exe" if sys.platform == "win32" else "wena-desktop")
        output.parent.mkdir(parents=True, exist_ok=True)
        return subprocess.call(test_command(ROOT / "scripts" / "build_desktop.sh") + [str(output)], cwd=ROOT)
    selected = host_target() if selection == "host" else selection
    records = [item for item in targets() if item["status"] == "ready"]
    names = [item["target"] for item in records] if selected == "all" else [selected]
    for name in names:
        result = build_one(name)
        if result:
            return result
    return 0


def list_targets():
    for item in targets():
        print("{target}\t{status}\t{name}".format(**item))
    return 0


def choose(title, choices):
    while True:
        print(f"\n{title}")
        for key, label in choices:
            print(f"  {key}) {label}")
        answer = input("Select: ").strip().lower()
        if any(answer == key for key, _label in choices):
            return answer
        print("Invalid selection.")


def build_menu():
    ready = [item for item in targets() if item["status"] == "ready"]
    choices = [("h", "Current host"), ("a", "All ready targets"),
               ("d", "Local SDL2/SQLite desktop app (create or open workspace)"),
               ("p", "Verified Linux amd64 desktop package")]
    choices += [(str(index), f"{item['name']} ({item['target']})")
                for index, item in enumerate(ready, 1)]
    choices.append(("b", "Back"))
    answer = choose("Build", choices)
    if answer == "b":
        return
    selection = "host" if answer == "h" else "all" if answer == "a" else "desktop" if answer == "d" else "desktop-package" if answer == "p" else ready[int(answer) - 1]["target"]
    result = build(selection)
    if result:
        print(f"Build failed with exit code {result}.")


def tests_menu():
    answer = choose("Tests", [("1", "Strict-C89 models"),
                              ("2", "Locale normalization and fallback"),
                              ("3", "Language override and runtime switch"),
                              ("a", "All native/static suites"), ("b", "Back")])
    if answer == "1":
        result = run_test("models")
    elif answer == "2":
        result = run_test("locale")
    elif answer == "3":
        result = run_test("language")
    elif answer == "a":
        result = run_test("all")
    else:
        return
    if result:
        print(f"Tests failed with exit code {result}.")


def server_menu():
    answer = choose("Server", [("1", "Status"), ("b", "Back")])
    if answer == "1":
        print("Wena Server is not implemented yet; see ROADMAP.md.")


def tools_menu():
    answer = choose("Tools", [("1", "List target catalog"), ("b", "Back")])
    if answer == "1":
        list_targets()


# One catalog drives listing, named execution and the complete native run.
# Shell wrappers for capability/schema already include their Python helpers.
TEST_SUITES = (
    ('desktop-package', 'test_desktop_package.py', 'Linux amd64 desktop package extraction, integrity and deterministic metadata'),
    ('desktop', 'test_desktop.sh', 'Local SDL2/SQLite workspace creation, startup and event regressions'),
    ('label-badges', 'test_label_badges.sh', 'Pure cached label badge rendering, scope validation and click routing'),
    ('labels-sqlite', 'test_labels_sqlite.sh', 'Label UI and SQLite persistence callbacks, stale revisions and transaction rollback'),
    ('labels-mutation', 'test_labels_mutation.sh', 'Scoped board labels and card assignments with guarded SQLite rollback and replay'),
    ('nuklear-labels', 'test_nuklear_labels.sh', 'Real Nuklear label input, palette colors, contrast and explicit cancellation'),
    ('labels', 'test_labels.sh', 'Bounded board label editor, card assignment controls and draft lifecycle'),
    ('nuklear-checklist-batch', 'test_nuklear_checklist_batch.sh', 'Real Nuklear multiline batch mode, explicit save and retained failure drafts'),
    ('checklist-batch', 'test_checklist_batch.sh', 'Atomic bounded multiline checklist item creation and rollback'),
    ('checklist-badges', 'test_checklist_badges.sh', 'Pure opt-in checklist count badges with empty progress, scope and click routing'),
    ('nuklear-checklist-order', 'test_nuklear_checklist_order.sh', 'Real Nuklear checklist and item ordering forms, bounds and cancellation'),
    ('checklist-order', 'test_checklist_order.sh', 'Guarded checklist and child ordering with atomic compaction and stale sibling refusal'),
    ('checklist-summary', 'test_checklist_summary.sh', 'Bounded board checklist summary projection with exact visibility and terminal revision guards'),
    ('checklist-item-titles', 'test_checklist_item_titles.sh', 'Bounded canonical multiline checklist title parsing'),
    ('checklist-delete', 'test_checklist_delete.sh', 'Guarded confirmed checklist deletion, rollback and concurrent child changes'),
    ('checklist-mutation', 'test_checklist_mutation.sh', 'Guarded native checklist SQLite persistence and rollback'),
    ('nuklear-board-settings', 'test_nuklear_board_settings.sh', 'Real Nuklear board count checkbox, explicit save and readonly controls'),
    ('board-settings-panel-sqlite', 'test_board_settings_panel_sqlite.sh', 'Board setting UI persistence, stale revisions, rollback and reopen'),
    ('board-presentation', 'test_board_presentation.sh', 'Committed board view cache refresh, quiet-frame SQL bounds and explicit retry'),
    ('board-settings', 'test_board_settings.sh', 'Guarded explicit board checklist-count persistence, noop, terminal revisions and rollback'),
    ('board-settings-panel', 'test_board_settings_panel.sh', 'Native board checklist-count setting draft, explicit save and readonly lifecycle'),
    ('board-filter', 'test_board_filter.sh', 'Real Nuklear local card filter scope, validation and visibility'),
    ('checklists', 'test_checklists.sh', 'Native checklist draft editing and bounded callbacks'),
    ('nuklear-checklists', 'test_nuklear_checklists.sh', 'Real Nuklear checklist input and controls'),
    ('checklists-sqlite', 'test_checklists_sqlite.sh', 'Native checklist UI and SQLite guarded integration'),
    ('collapse-preferences', 'test_collapse_preferences.sh', 'Scoped atomic native collapse preferences and failed writes'),
    ('checklist-models', 'test_checklist_models.sh', 'Pure checklist and item scope, defaults and validation models'),
    ('colors', 'test_colors.sh', 'Canonical palette, strict custom colors and independently checked readable contrast'),
    ('model-text', 'test_model_text.sh', 'Shared ECMAScript trim and bounded label-name normalization'),
    ('models', 'test_models.sh', 'Strict-C89 model/unit and negative validation'),
    ('locale', 'test_locale.sh', 'OS locale normalization, fallback, and RTL direction'),
    ('language-picker', 'test_language_picker.sh', 'Real Nuklear language selection and persisted override'),
    ('language-storage', 'test_language_storage.sh', 'Exclusive POSIX locale settings publication and failure rollback'),
    ('language', 'test_language.sh', 'Persistent override and immediate runtime switching'),
    ('server-settings', 'test_server_settings.sh', 'Admin server address, ROOT_URL, and lifecycle state'),
    ('html4-render', 'test_legacy_html4_render.sh', 'ROOT_URL-scoped escaped Legacy HTML4 baseline'),
    ('capability-runtime', 'test_capability_runtime.sh', 'Node DOM harness for actual emitted drag/drop asset'),
    ('parser-mutations', 'test_parser_mutations.sh', 'Deterministic HTTP and region-parser mutations with exact-size buffers'),
    ('http-server', 'test_http_server.sh', 'Bounded parser and Admin-controlled IPv4 listener'),
    ('security', 'test_security.sh', 'Opaque sessions and scoped single-use CSRF audit'),
    ('router', 'test_router.sh', 'Read-only GET and protected mutation-intent gate'),
    ('platform-security', 'test_platform_security.sh', 'OS entropy and strict same-origin headers'),
    ('http-serving', 'test_http_serving.sh', 'Timed read-only HTML4 serving loop'),
    ('capability', 'test_capability.sh', 'Progressive drag/drop capability and baseline restore'),
    ('regions', 'test_region_response.sh', 'Bounded versioned visible-region response protocol'),
    ('domain-operation', 'test_domain_operation.sh', 'Verified intent to allowlisted domain callback'),
    ('persistence', 'test_persistence.sh', 'Atomic in-memory transaction and rollback contract'),
    ('sqlite-schema', 'test_sqlite_schema.sh', 'Versioned SQLite schema and migration golden'),
    ('sqlite-schema-v2', 'test_sqlite_schema_v2.sh', 'Atomic description schema and staged backup upgrade'),
    ('sqlite-schema-v3', 'test_sqlite_schema_v3.sh', 'Scoped checklist schema and atomic legacy upgrade'),
    ('sqlite-schema-v6', 'test_sqlite_schema_v6.sh', 'Default-off board checklist settings schema, atomic upgrades and staged restore'),
    ('sqlite-schema-v5', 'test_sqlite_schema_v5.sh', 'Scoped label catalog and assignment schema with atomic legacy upgrade'),
    ('sqlite-schema-v4', 'test_sqlite_schema_v4.sh', 'Bounded selected-card item queries and indexed schema upgrade'),
    ('version-boundaries', 'test_version_boundaries.sh', 'Shared terminal revision limits across native mutation families and reopen'),
    ('sqlite-form-validation', 'test_sqlite_form_validation.sh', 'Strict SQLite mutation input parsing and numeric limits'),
    ('hierarchy-move-sqlite', 'test_hierarchy_move_sqlite.sh', 'Hierarchy reorder UI and SQLite rollback, cache and reopen'),
    ('hierarchy-move-mutation', 'test_hierarchy_move_mutation.sh', 'Guarded hierarchy reorder adapter scope, version and replay'),
    ('hierarchy-move', 'test_hierarchy_move.sh', 'Bounded hierarchy reorder and card transfer interactions'),
    ('nuklear-hierarchy-move', 'test_nuklear_hierarchy_move.sh', 'Real Nuklear hierarchy destination selection and cancellation'),
    ('hierarchy-title', 'test_hierarchy_title.sh', 'Hierarchy create and rename SQLite adapters and real Nuklear input'),
    ('sqlite-hierarchy-create', 'test_sqlite_hierarchy_create.sh', 'Scoped SQLite list and swimlane creation persistence'),
    ('sqlite-workspace', 'test_sqlite_workspace.sh', 'Atomic no-clobber local SQLite workspace initialization'),
    ('sqlite-board', 'test_sqlite_board.sh', 'Bounded SQLite board snapshot with scope and reopen checks'),
    ('sqlite-hardening', 'test_sqlite_hardening.sh', 'Defensive SQLite connection settings and untrusted schema refusal'),
    ('sqlite-storage', 'test_sqlite_storage.sh', 'Checksummed atomic SQLite migration runner'),
    ('progressive', 'test_progressive_integration.sh', 'HTML4 fallback, DnD, POST, and multi-region integration'),
    ('migration-embed', 'test_migration_embedding.py', 'Pinned SQLite migration in every ready artifact'),
    ('sqlite-persistence', 'test_sqlite_persistence.sh', 'Transactional SQLite create/edit/archive adapter'),
    ('runtime', 'test_runtime.sh', 'Managed SQLite adapter and listener lifecycle'),
    ('embedded-migration', 'test_embedded_migration.sh', 'Runtime executable migration footer loader'),
    ('executable-path', 'test_executable_path.sh', 'Bounded platform executable discovery'),
    ('sqlite-backup', 'test_sqlite_backup.sh', 'Sqlite backup regression checks'),
    ('sqlite-restore', 'test_sqlite_restore.sh', 'Sqlite restore regression checks'),
    ('admin-storage', 'test_admin_storage.sh', 'Admin storage regression checks'),
    ('ferretdb-compat', 'test_ferretdb_compat.sh', 'Ferretdb compat regression checks'),
    ('wekan-compat-inventory', 'test_wekan_compat_inventory.py', 'Wekan compat inventory regression checks'),
    ('theme-parity', 'test_theme_color_parity.py', 'Theme parity regression checks'),
    ('collapse', 'test_collapse.sh', 'List collapse state, scope and responsive layout'),
    ('board-feature', 'test_board_feature.sh', 'Board feature regression checks'),
    ('panel-escape', 'test_panel_escape.sh', 'Focused move/archive Escape cancellation and held-key safety'),
    ('nuklear-title-keys', 'test_nuklear_title_keys.sh', 'Focused title editor Enter/Escape, held keys and error states'),
    ('nuklear-card-move', 'test_nuklear_card_move.sh', 'Real Nuklear list and swimlane selection, save and cancel'),
    ('nuklear-card-create', 'test_nuklear_card_create.sh', 'Real Nuklear create input typing, bounded rejection and cancel'),
    ('sdl-text-input', 'test_sdl_text_input.sh', 'Complete bounded SDL UTF-8 text events and malformed input rejection'),
    ('dependency-report', 'test_dependency_report.sh', 'Actual-process dependency diagnostics without SDL initialization'),
    ('dependency-check', 'test_dependencies.py', 'Pinned dependency provenance, inventory and runtime version classification'),
    ('native-feature-i18n', 'test_native_feature_i18n.sh', 'Localized native failures and empty states with preserved edit drafts'),
    ('native-font', 'test_native_font.sh', 'Pinned embedded font validation and real Unicode atlas bake'),
    ('native-theme', 'test_native_theme.sh', 'Canonical native theme colors, contrast and draw commands'),
    ('nuklear-board', 'test_nuklear_board.sh', 'Real Nuklear board visibility, clipping and mouse collapse geometry'),
    ('nuklear-editor', 'test_nuklear_editor.sh', 'Real Nuklear bounded editor interaction'),
    ('nuklear', 'test_nuklear_integration.sh', 'Nuklear regression checks'),
    ('card-editor-sqlite', 'test_card_editor_sqlite.sh', 'Guarded card title editor SQLite integration'),
    ('card-create-sqlite', 'test_card_create_sqlite.sh', 'Card create UI adapter scope, rollback, retry and reopen'),
    ('card-restore-persistence', 'test_card_restore_persistence.sh', 'Guarded archived card restoration scope, replay and persistence'),
    ('card-archives', 'test_card_archives.sh', 'Archived card selection and restore feature interactions'),
    ('card-archives-sqlite', 'test_card_archives_sqlite.sh', 'Archived card UI integration with SQLite restore adapter'),
    ('nuklear-card-archives', 'test_nuklear_card_archives.sh', 'Real Nuklear archived card selection and restore interaction'),
    ('card-move-reorder-ui', 'test_card_move_reorder_ui.sh', 'Indexed card move editor snapshot lifetime and cancellation'),
    ('nuklear-card-reorder', 'test_nuklear_card_reorder.sh', 'Real Nuklear indexed card destination and position selection'),
    ('card-move-reorder-sqlite', 'test_card_move_reorder_sqlite.sh', 'Indexed card reorder UI integration and persistence'),
    ('card-reorder', 'test_card_reorder.sh', 'Indexed card moves with gap compaction, replay and transactional rollback'),
    ('card-move', 'test_card_move.sh', 'Bounded card movement form destinations and cancellation'),
    ('card-move-sqlite', 'test_card_move_sqlite.sh', 'Card move UI integration with guarded SQLite adapter'),
    ('card-move-persistence', 'test_card_move_persistence.sh', 'Guarded card move scope, optimistic conflict and replay'),
    ('card-create-persistence', 'test_card_create_persistence.sh', 'Guarded native create adapter scope, replay, rollback and reopen'),
    ('card-create', 'test_card_create.sh', 'Bounded scoped card creation editor interactions'),
    ('card-description', 'test_card_description.sh', 'Bounded multiline card description editor state and validation'),
    ('nuklear-card-description', 'test_nuklear_card_description.sh', 'Real Nuklear multiline description typing, save and cancel'),
    ('card-description-sqlite', 'test_card_description_sqlite.sh', 'Description editor SQLite callbacks, rollback and reopen'),
    ('card-description-mutation', 'test_card_description_mutation.sh', 'Description persistence using isolated pinned schema-v2 fixture'),
    ('card-mutation', 'test_card_mutation.sh', 'Card mutation regression checks'),
    ('build-entrypoints', 'test_build_entrypoints.py', 'Build entrypoints regression checks'),
    ('collect-release-assets', 'test_collect_release_assets.py', 'Collect release assets regression checks'),
    ('generate-i18n-catalog', 'test_generate_i18n_catalog.py', 'Generate i18n catalog regression checks'),
    ('i18n-embedding', 'test_i18n_embedding.py', 'I18n embedding regression checks'),
    ('release-workflow', 'test_release_workflow.py', 'Release workflow regression checks'),
    ('source-structure', 'test_source_structure.py', 'Source structure regression checks'),
    ('target-catalog', 'test_target_catalog.py', 'Target catalog regression checks'),
    ('ui-catalog', 'test_ui_catalog.sh', 'Generated canonical offline UI translations and language fallback'),
    ('ui-contract', 'test_ui_contract.py', 'Ui contract regression checks'),
    ('verify-i18n-catalog', 'test_verify_i18n_catalog.py', 'Verify i18n catalog regression checks'),
    ('verify-release-assets', 'test_verify_release_assets.py', 'Verify release assets regression checks'),
)
SERIAL_SUITES = {"migration-embed", "i18n-embedding", "runtime", "embedded-migration", "desktop", "desktop-package"}
SOURCE_SUITES = {"theme-parity", "wekan-compat-inventory"}


def test_command(script):
    # Python scripts need neither executable mode nor an sh-compatible body.
    if script.suffix == ".py":
        return [sys.executable, str(script)]
    shell = shutil.which("sh")
    if not shell:
        raise OSError("native shell tests require sh")
    return [shell, str(script)]


def test_prerequisite(name):
    if name in {"language-storage", "collapse-preferences"} and os.name == "nt":
        return "requires POSIX symlink and file-mode semantics"
    if name == "desktop-package" and (platform.system() != "Linux" or platform.machine().lower() not in {"x86_64", "amd64"}):
        return "desktop packaging is currently verified only on Linux amd64"
    if name == "desktop-package" and not shutil.which("readelf"):
        return "requires readelf (binutils) for actual ELF runtime requirements"
    if name in {"nuklear", "desktop", "desktop-package", "sdl-text-input", "dependency-report"} and not shutil.which("sdl2-config"):
        return "requires SDL2 development files (sdl2-config)"
    if name in {"desktop", "desktop-package", "nuklear-board", "collapse-preferences", "nuklear-checklists", "nuklear-labels", "nuklear-board-settings", "label-badges", "nuklear-checklist-batch", "nuklear-checklist-order", "board-filter", "nuklear-editor", "nuklear-card-create", "nuklear-card-move", "nuklear-card-reorder", "nuklear-card-description", "nuklear-title-keys", "panel-escape", "nuklear-card-archives", "language-picker", "hierarchy-title", "nuklear-hierarchy-move", "native-theme", "native-font", "dependency-check", "native-feature-i18n", "sdl-text-input", "nuklear"} and not (ROOT / "third_party" / "nuklear" / "nuklear.h").is_file():
        return "requires initialized third_party/nuklear submodule"
    if name in SOURCE_SUITES:
        source = Path(os.environ.get("WEKAN_ROOT", str(ROOT.parents[1])))
        if not (source / "imports" / "lib" / "legacyHtml4.js").is_file():
            return "requires pinned WeKan source checkout at " + str(source)
    return None


def execute_test(record):
    name, filename, _description = record
    missing = test_prerequisite(name)
    if missing:
        return name, "SKIP", missing
    try:
        result = subprocess.run(test_command(ROOT / "tests" / filename),
                                cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=300)
        return name, "PASS" if result.returncode == 0 else "FAIL", result.stdout
    except (OSError, subprocess.TimeoutExpired) as error:
        return name, "FAIL", str(error)


def run_all_tests(jobs=4, suites=None, executor=None):
    if jobs < 1 or jobs > 32:
        raise ValueError("test jobs must be between 1 and 32")
    records = list(TEST_SUITES if suites is None else suites)
    execute = execute_test if executor is None else executor
    independent = [item for item in records if item[0] not in SERIAL_SUITES]
    serial = [item for item in records if item[0] in SERIAL_SUITES]
    # map preserves catalog order even if subprocesses finish out of order.
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(execute, independent))
    results += [execute(item) for item in serial]
    by_name = {result[0]: result for result in results}
    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    for name, _filename, _description in records:
        _, status, output = by_name[name]
        counts[status] += 1
        print(f"{status} {name}")
        if status != "PASS" and output:
            print(output.rstrip())
    print("Native suite summary: " + ", ".join(
        f"{counts[status]} {status.lower()}" for status in ("PASS", "FAIL", "SKIP")))
    print("Cross-target artifact tests are excluded: they require target-specific "
          "compilers/SDKs; select target-release-TARGET explicitly.")
    return 1 if counts["FAIL"] else 0


def run_test(name):
    if name == "sanitizers":
        script = ROOT / "tests" / "test_native_sanitizers.sh"
        # Freeze this long-running dispatcher before parallel agents add suites.
        command = test_command(script)
        return subprocess.call([command[0], "-c", script.read_text(encoding="utf-8"), str(script)], cwd=ROOT)
    if name == "all":
        return run_all_tests()
    if name.startswith("target-release-"):
        target = name[len("target-release-"):]
        if not any(item["target"] == target and item["status"] == "ready"
                   for item in targets()):
            raise SystemExit(f"unknown ready release-test target: {target}")
        script = ROOT / "tests" / ("test_" + target.replace("-", "_") + "_release.sh")
        return subprocess.call(test_command(script), cwd=ROOT)
    record = next((item for item in TEST_SUITES if item[0] == name), None)
    if record is None:
        raise SystemExit(f"unknown test suite: {name}")
    name, status, output = execute_test(record)
    print(f"{status} {name}")
    if output:
        print(output.rstrip())
    return 0 if status == "PASS" else 1


def menu():
    while True:
        answer = choose("Wena", [("1", "Build"), ("2", "Tests"),
                                  ("3", "Server"), ("4", "Tools"), ("q", "Quit")])
        if answer == "1":
            build_menu()
        elif answer == "2":
            tests_menu()
        elif answer == "3":
            server_menu()
        elif answer == "4":
            tools_menu()
        else:
            return 0


def usage():
    print("Usage: wena.py --list | build host|all|desktop|desktop-package|TARGET | tests --list|all|SUITE | server status | tools targets | menu", file=sys.stderr)
    return 2


def main(argv):
    if argv == ["--list"]:
        return list_targets()
    if argv == ["menu"]:
        return menu()
    if len(argv) == 2 and argv[0] == "build":
        return build(argv[1])
    if argv == ["tests", "--list"]:
        print("all\tAll native/static suites (four parallel workers; shared builds serial)")
        print("sanitizers\tOptional ASan/UBSan native model, UI and SQLite regression subset")
        for name, _filename, description in TEST_SUITES:
            print(f"{name}\t{description}")
        for item in targets():
            if item["status"] == "ready":
                print("target-release-" + item["target"] +
                      "\tExplicit artifact test; requires target compiler/SDK")
        return 0
    if len(argv) == 2 and argv[0] == "tests":
        return run_test(argv[1])
    if argv == ["server", "status"]:
        print("Wena Server is not implemented yet; see ROADMAP.md.")
        return 3
    if argv == ["tools", "targets"]:
        return list_targets()
    return usage()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
