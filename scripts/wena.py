#!/usr/bin/env python3
"""Shared local and CI command dispatcher for Wena."""

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
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
        result.append(dict(zip(("target", "name", "job", "status", "release"), fields)))
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


def toolchain_module():
    import importlib.util
    spec = importlib.util.spec_from_file_location("toolchain", ROOT / "scripts" / "toolchain.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def prepare_toolchain(target):
    """Install what building target needs on this computer: a toolchain.Plan,
    or None after saying why it cannot be built here."""
    toolchain = toolchain_module()
    try:
        return toolchain.prepare(target)
    except toolchain.Unavailable as error:
        print(f"{target}: cannot build here: {error}", file=sys.stderr, flush=True)
        return None


def build_one(target):
    record = next((item for item in targets() if item["target"] == target), None)
    if record is None:
        raise SystemExit(f"unknown target: {target}")
    if record["status"] != "ready":
        raise SystemExit(f"target is cataloged but not ready: {target}")
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
    plan = prepare_toolchain(target)
    if plan is None:
        return UNAVAILABLE
    print(f"Building {record['name']} ({target}) into release/{record['release']}", flush=True)
    environment = plan.environment()
    for command in plan.commands:
        # Looked up on the build's PATH, which can name tools this process's does not.
        program = shutil.which(command[0], path=environment.get("PATH")) or command[0]
        status = subprocess.call([program, *command[1:]], cwd=ROOT, env=environment)
        if status:
            return status
    return 0


def build(selection):
    if selection == "desktop-package":
        return subprocess.call([sys.executable, str(ROOT / "scripts/package_desktop.py")], cwd=ROOT)
    if selection == "desktop":
        print("Building local SDL2/SQLite desktop app (create or open workspace)", flush=True)
        output = ROOT / "dist" / "desktop" / ("wena-desktop.exe" if sys.platform == "win32" else "wena-desktop")
        output.parent.mkdir(parents=True, exist_ok=True)
        plan = prepare_toolchain("desktop")
        if plan is None:
            return 1
        return subprocess.call(test_command(ROOT / "scripts" / "build_desktop.sh") + [str(output)],
                               cwd=ROOT, env=plan.environment())
    selected = host_target() if selection == "host" else selection
    if selected != "all":
        result = build_one(selected)
        return 1 if result == UNAVAILABLE else result
    # All: every target this computer can build; the others are only listed.
    skipped, failed = [], []
    for item in targets():
        if item["status"] != "ready":
            continue
        result = build_one(item["target"])
        if result == UNAVAILABLE:
            skipped.append(item["target"])
        elif result:
            failed.append(item["target"])
    if skipped:
        print("Not buildable on this computer: " + ", ".join(skipped), flush=True)
    if failed:
        print("Failed: " + ", ".join(failed), flush=True)
    return 1 if failed else 0


# build_one's result for a target this computer cannot build (no exit code is negative 1000).
UNAVAILABLE = -1000


def install(selection):
    """Install what building selection needs, without building it."""
    selected = host_target() if selection == "host" else selection
    names = ([item["target"] for item in targets() if item["status"] == "ready"]
             if selected == "all" else [selected])
    ready = [name for name in names if prepare_toolchain(name) is not None]
    for name in ready:
        print(f"{name}: ready", flush=True)
    return 0 if len(ready) == len(names) or selected == "all" else 1


DEFAULT_ACTOR = "local-user"
DEFAULT_BOARD = "my-board"
DEFAULT_BOARD_TITLE = "My board"


def desktop_binary(root=ROOT, windows=None):
    """The Nuklear desktop app that 1) Build > d) writes."""
    windows = sys.platform == "win32" if windows is None else windows
    return Path(root) / "dist" / "desktop" / ("wena-desktop.exe" if windows else "wena-desktop")


def workspace_path(environ=None, system=None, home=None):
    """Where Run keeps its local board: WENA_DATABASE, or the user's data folder."""
    environ = os.environ if environ is None else environ
    if environ.get("WENA_DATABASE"):
        return Path(environ["WENA_DATABASE"]).expanduser().absolute()
    system = (system or platform.system()).lower()
    home = Path(home) if home else Path.home()
    if system == "darwin":
        base = home / "Library" / "Application Support" / "Wena"
    elif system == "windows":
        base = Path(environ.get("APPDATA") or home / "AppData" / "Roaming") / "Wena"
    else:
        base = Path(environ.get("XDG_DATA_HOME") or home / ".local" / "share") / "wena"
    return base / "wena.sqlite"


def desktop_arguments(database):
    """Open the local board, creating it on the first run."""
    arguments = ["--database", str(database), "--actor", DEFAULT_ACTOR, "--board", DEFAULT_BOARD]
    if not Path(database).exists():
        arguments += ["--create", "--title", DEFAULT_BOARD_TITLE]
    return arguments


def log_directory(root=ROOT, now=None):
    """This run's debug folder: .tools/log/wena/YYYY-MM-DD_HH-MM-SS, in the
    .tools folder Wena is checked out in, or the one inside it otherwise."""
    root = Path(root)
    tools = root.parent if root.parent.name == ".tools" else root / ".tools"
    return tools / "log" / "wena" / (now or datetime.now()).strftime("%Y-%m-%d_%H-%M-%S")


def run(args=(), root=ROOT, database=None, now=None):
    """2) Run: the native Nuklear desktop. Given arguments are passed as they are;
    without any, it opens as a double-click does: WeKan's files - WRITABLE_PATH,
    else wekan-files beside the program - with db/wekan.sqlite in FerretDB's
    format. `database` names a Wena workspace file instead."""
    binary = desktop_binary(root)
    if not binary.is_file():
        print(f"{binary.relative_to(root)} is not built yet; build it first with 1) Build, "
              "then d) Local SDL2/SQLite desktop app.", file=sys.stderr)
        return 1
    if sys.platform != "win32" and not os.access(binary, os.X_OK):
        print(f"{binary.relative_to(root)} exists but is not executable.", file=sys.stderr)
        return 1
    args = list(args)
    if not args and database:
        database = Path(database)
        database.parent.mkdir(parents=True, exist_ok=True)
        args = desktop_arguments(database)
        print(f"Opening board {DEFAULT_BOARD} in {database}", flush=True)
    elif not args:
        writable = os.environ.get("WRITABLE_PATH")
        files = Path(writable) if writable else binary.parent / "wekan-files"
        if writable and files.name not in ("files", "wekan-files"):
            files = files / "files"
        print(f"Opening WeKan's files in {files} (database {files / 'db' / 'wekan.sqlite'})", flush=True)
    logs = log_directory(root, now)
    logs.mkdir(parents=True, exist_ok=True)
    command = [str(binary), *args]
    print(f"Running {binary.relative_to(root)}", flush=True)
    print(f"Debug log: {logs}", flush=True)
    # The desktop writes desktop.log there (WENA_LOG_DIR); everything it prints
    # is also kept in run.log, with how it ended.
    with open(logs / "run.log", "w", encoding="utf-8") as record:
        record.write("command: " + " ".join(command) + "\n")
        record.flush()
        process = subprocess.Popen(command, cwd=root, env=dict(os.environ, WENA_LOG_DIR=str(logs)),
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, errors="replace")
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            record.write(line)
            record.flush()
        status = process.wait()
        ended = f"killed by signal {-status}" if status < 0 else f"exit code {status}"
        record.write(f"ended: {ended}\n")
    return status


def git_output(*args, root=ROOT):
    result = subprocess.run(["git", "-C", str(root), *args], text=True, capture_output=True)
    return result.stdout.strip() if result.returncode == 0 else None


def github_repository(remote):
    """OWNER/REPO from an SSH or HTTPS GitHub remote, or None."""
    for prefix in ("git@github.com:", "https://github.com/", "ssh://git@github.com/"):
        if remote and remote.startswith(prefix):
            name = remote[len(prefix):]
            name = name[:-4] if name.endswith(".git") else name
            return name if name.count("/") == 1 and all(name.split("/")) else None
    return None


def release_version_module(root=ROOT):
    import importlib.util
    spec = importlib.util.spec_from_file_location("release_version", Path(root) / "scripts" / "release_version.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def release(mode="next", root=ROOT, run=subprocess.run, sleep=None, today=None):
    """3) Release, the human's step: GitHub builds and publishes; this starts it.

    next:    name the Upcoming CHANGELOG section after the next version
             (v0.01, v0.02, ... v9.99, v10.00), commit "Prepare vX release",
             push the branch and start release-all.yml with that version,
             which tags it, publishes the release with the section as its notes
             and attaches the desktop executable of every platform.
    missing: push the branch and start release-all-missing.yml, which
             builds only the files the newest release lacks - those whose
             build failed - and attaches each as it is built."""
    import time
    sleep = sleep or time.sleep
    if mode not in ("next", "missing"):
        print(f"unknown release mode: {mode} (next or missing)", file=sys.stderr)
        return 2
    if not shutil.which("gh"):
        print("Release needs the GitHub CLI: https://cli.github.com (brew install gh)", file=sys.stderr)
        return 1
    if run(["gh", "auth", "status", "-h", "github.com"], capture_output=True).returncode != 0:
        print("gh is not logged in to github.com. Run: gh auth login", file=sys.stderr)
        return 1
    repository = github_repository(git_output("remote", "get-url", "origin", root=root))
    branch = git_output("rev-parse", "--abbrev-ref", "HEAD", root=root)
    if not repository or not branch or branch == "HEAD":
        print("Release needs a branch with a GitHub origin remote.", file=sys.stderr)
        return 1
    if git_output("status", "--porcelain", root=root):
        print("Commit or stash your changes first: a release builds only what is committed.", file=sys.stderr)
        return 1
    name = "release-all.yml" if mode == "next" else "release-all-missing.yml"
    workflow = ["gh", "workflow", "run", name, "-R", repository, "--ref", branch]
    if mode == "next":
        versions = release_version_module(root)
        changelog_path = Path(root) / "CHANGELOG.md"
        changelog = changelog_path.read_text(encoding="utf-8")
        listed = run(["gh", "release", "list", "-R", repository, "--limit", "1000",
                      "--json", "tagName", "--jq", ".[].tagName"], capture_output=True, text=True)
        if listed.returncode != 0:
            print("Could not list the releases on GitHub; nothing changed.", file=sys.stderr)
            return 1
        version = versions.next_version(listed.stdout.split(), changelog)
        try:
            renamed = versions.name_release(changelog, version, today)
        except ValueError as error:
            print(f"{error}; nothing changed.", file=sys.stderr)
            return 1
        changelog_path.write_text(renamed, encoding="utf-8")
        for command in (["git", "-C", str(root), "add", "CHANGELOG.md"],
                        ["git", "-C", str(root), "commit", "-q", "-m", f"Prepare {version} release"]):
            if run(command).returncode != 0:
                print(f"{' '.join(command[3:])} failed; CHANGELOG.md is renamed but not committed.", file=sys.stderr)
                return 1
        workflow += ["-f", f"version={version}"]
        print(f"Prepared {version}.", flush=True)
    print(f"Pushing {branch} to {repository} ...", flush=True)
    if run(["git", "-C", str(root), "push", "origin", branch]).returncode != 0:
        print(f"Push failed; nothing was started. Push {branch} and rerun.", file=sys.stderr)
        return 1
    for attempt in range(1, 4):
        if run(workflow).returncode == 0:
            print(f"Started {name}. Follow it at https://github.com/{repository}/actions", flush=True)
            return 0
        if attempt < 3:
            print(f"Attempt {attempt}/3 failed; retrying in 5 seconds.", file=sys.stderr)
            sleep(5)
    print(f"Could not start {name}. A token needs the workflow scope "
          "(gh auth refresh -h github.com -s workflow), and the workflow must be on "
          f"{branch}. Start it at https://github.com/{repository}/actions", file=sys.stderr)
    return 1


def release_menu():
    answer = choose("Release (GitHub builds an executable for every platform)", [
        ("n", "Release next version: number Upcoming, commit, push, build and publish"),
        ("m", "Build missing files for the newest release"),
        ("b", "Back")])
    if answer == "b":
        return
    result = release("next" if answer == "n" else "missing")
    if result:
        print(f"Release did not start (exit code {result}).")


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
               ("p", "Verified Linux amd64 desktop package"),
               ("i", "Install what all ready targets and the desktop app need")]
    choices += [(str(index), f"{item['name']} ({item['target']})")
                for index, item in enumerate(ready, 1)]
    choices.append(("b", "Back"))
    answer = choose("Build", choices)
    if answer == "b":
        return
    if answer == "i":
        install("all")
        install("desktop")
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
    ('selected-people', 'test_selected_people.sh', 'Atomic single/bulk people assignment, replay, whole-selection guards and capacity'),
    ('card-people-mutation', 'test_card_people_mutation.sh', 'Shared member/assignee writer, exact snapshot guards, ordering and rollback'),
    ('card-people-store', 'test_card_people_store.sh', 'Strict member rosters and card people snapshots, corruption, WAL consistency and capacity'),
    ('card-people', 'test_card_people.sh', 'Shared member/assignee eligibility, stable set operations and cross-board membership mapping'),
    ('checklist-item-move', 'test_checklist_item_move.sh', 'Guarded checklist item transfer across cards or same-card parents'),
    ('nuklear-checklist-item-move', 'test_nuklear_checklist_item_move.sh', 'Real Nuklear/SQLite item destination selectors and guarded lifecycle'),
    ('svg', 'test_svg.sh', 'MIT SVG conversion, native vector scaling, theme tokens and lossless documentation captures'),
    ('nuklear-cross-board-destination', 'test_nuklear_cross_board_destination.sh', 'Shared paginated board/card chooser and guarded checklist/item moves'),
    ('checklist-insert', 'test_checklist_insert.sh', 'Exact transfer ordinals through shared guarded order compaction'),
    ('checklist-cross-board', 'test_checklist_cross_board.sh', 'Reuse transfer regression suites across distinct boards'),
    ('checklist-move', 'test_checklist_move.sh', 'Atomic whole-checklist transfer with source/destination guards, rollback and reopen'),
    ('nuklear-checklist-move', 'test_nuklear_checklist_move.sh', 'Real Nuklear and SQLite cross-card selection, cancel, stale revisions and refresh'),
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
    ('nuklear-hierarchy-drag', 'test_nuklear_hierarchy_drag.sh', 'Shared guarded drag lifecycle for list and swimlane ordering'),
    ('nuklear-card-drag', 'test_nuklear_card_drag.sh', 'Same-column card drag with shared snapshots and guarded saves'),
    ('nuklear-reorder-drag', 'test_nuklear_reorder_drag.sh', 'Shared exact-ID drag reorder control'),
    ('nuklear-directory-picker', 'test_nuklear_directory_picker.sh', 'Shared lazy board/actor picker with SQL-free frames'),
    ('nuklear-paginated-table', 'test_nuklear_paginated_table.sh', 'Reusable paginated table navigation, row intents and bounds'),
    ('nuklear-checklist-contents', 'test_nuklear_checklist_contents.sh', 'Expanded checklist preview visibility and SQL-free rendering'),
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
    ('nuklear-swimlane-resize', 'test_nuklear_swimlane_resize.sh', 'Dragging the bar below a swimlane changes and stores its height'),
    ('nuklear-options', 'test_nuklear_options.py', 'One set of Nuklear options in every unit, so nk_context has one layout'),
    ('sql-sources', 'test_sql_sources.py', 'No test or application SQL comes from outside the program (CodeQL cpp/sql-injection)'),
    ('debug-log', 'test_debug_log.sh', 'Desktop debug log folder, default board file and crash signal record'),
    ('aga-palette', 'test_aga_palette.sh', 'AmigaOS 3 AGA palette: WeKan colors exact, every color near, frame conversion'),
    ('amiga-aga', 'test_amiga_aga.py', 'AmigaOS 3 AGA build: SDL patch, desktop AGA branch, catalog and release check'),
    ('release-link-flags', 'test_release_link_flags.py', "Haiku links SDL's C++ runtime, no other release system does"),
    ('attach-release-files', 'test_attach_release_files.py', 'Each release file attached as soon as its build job finishes'),
    ('member-settings', 'test_member_settings.sh', "WeKan's member menu, Edit Profile and Change Settings"),
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
    ('card-sections', 'test_card_sections.sh', 'Reusable actor/card section preferences and schema-v8 upgrade'),
    ('sqlite-schema-v11', 'test_sqlite_schema_v11.sh', 'Immutable hierarchy colors migration, all-prefix upgrades and failure rollback'),
    ('sqlite-schema-v12', 'test_sqlite_schema_v12.sh', 'Immutable list WIP migration, all-prefix upgrades and failure rollback'),
    ('sqlite-schema-v14', 'test_sqlite_schema_v14.sh', 'Board membership and card people constraints, all-prefix upgrades and atomic rollback'),
    ('sqlite-schema-v13', 'test_sqlite_schema_v13.sh', 'Immutable swimlane/card archive metadata, all-prefix upgrades and rollback'),
    ('sqlite-schema-v10', 'test_sqlite_schema_v10.sh', 'List archive state upgrades, constraints and transaction rollback'),
    ('sqlite-schema-v9', 'test_sqlite_schema_v9.sh', 'Card collapse preferences upgrade and rollback'),
    ('sqlite-schema-v7', 'test_sqlite_schema_v7.sh', 'Additive minicard preferences upgrade and rollback'),
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
    ('sqlite-directory', 'test_sqlite_directory.sh', 'Shared bounded directory paging for boards and actors'),
    ('hierarchy-colors', 'test_hierarchy_colors.sh', 'Shared guarded list/swimlane colors, scope, versions, replay, corruption and rollback'),
    ('list-wip', 'test_list_wip.sh', 'Guarded list WIP settings, active counts, replay and rollback'),
    ('list-archive', 'test_list_archive.sh', 'Guarded list archive/restore and atomic archived-list board snapshots'),
    ('card-archive-state', 'test_card_archive_state.sh', 'Guarded card archive timestamps, legacy restore, monotonicity and rollback'),
    ('swimlane-archive', 'test_swimlane_archive.sh', 'Atomic lane card cascades, timestamp ties, WIP, corruption and rollback'),
    ('sqlite-board', 'test_sqlite_board.sh', 'Bounded SQLite board snapshot with scope and reopen checks'),
    ('sqlite-hardening', 'test_sqlite_hardening.sh', 'Defensive SQLite connection settings and untrusted schema refusal'),
    ('sqlite-storage', 'test_sqlite_storage.sh', 'Checksummed atomic SQLite migration runner'),
    ('compiled-bundle', 'test_compiled_bundle.sh', 'Desktop migration bundle compiled in and checked against the lock'),
    ('progressive', 'test_progressive_integration.sh', 'HTML4 fallback, DnD, POST, and multi-region integration'),
    ('migration-embed', 'test_migration_embedding.py', 'Pinned SQLite migration lock, generated registry and stale-source refusal'),
    ('sqlite-persistence', 'test_sqlite_persistence.sh', 'Transactional SQLite create/edit/archive adapter'),
    ('runtime', 'test_runtime.sh', 'Managed SQLite adapter and listener lifecycle'),
    ('embedded-migration', 'test_embedded_migration.sh', 'Runtime executable migration footer loader'),
    ('executable-path', 'test_executable_path.sh', 'Bounded platform executable discovery'),
    ('bsd-sources', 'test_bsd_sources.py', 'Every desktop source compiles against FreeBSD, OpenBSD and NetBSD headers, and with GCC on NetBSD'),
    ('extract-archive', 'test_extract_archive.py', 'Release archives unpacked without tar --strip-components, which OpenBSD lacks'),
    ('sqlite-backup', 'test_sqlite_backup.sh', 'Sqlite backup regression checks'),
    ('sqlite-restore', 'test_sqlite_restore.sh', 'Sqlite restore regression checks'),
    ('admin-storage', 'test_admin_storage.sh', 'Admin storage regression checks'),
    ('sjson', 'test_sjson.sh', 'Shared typed FerretDB SJSON validation with exact integers and ordered fields'),
    ('json-edit', 'test_json_edit.sh', 'Atomic shared JSON edits and typed SJSON replacements'),
    ('json-document', 'test_json_document.sh', 'Bounded shared JSON reader preserving numeric precision and object order'),
    ('ferretdb-scan', 'test_ferretdb_scan.sh', 'Bounded read-only typed scans of every mapped FerretDB collection'),
    ('all-boards', 'test_all_boards.sh', "WeKan's All Boards page: sections, counts, open, star, Add Board, archive"),
    ('board-search', 'test_board_search.sh', "WeKan's board Search: lists and cards by title or description"),
    ('search-sidebar', 'test_search_sidebar.sh', "WeKan's Search sidebar: field, Enter, results, opening a card"),
    ('notifications-drawer', 'test_notifications_drawer.sh', "WeKan's notifications drawer: lines, unread count, read"),
    ('board-views', 'test_board_views.sh', "WeKan's board views: the menu's 35 views, a report chart drawn"),
    ('wekan-views', 'test_wekan_views.sh', "WeKan's records for the board views, from wekan.sqlite"),
    ('charts', 'test_charts.py', "Wena's report charts against WeKan's own calculations in Node"),
    ('view-rows', 'test_view_rows.py', "Wena's Table, Calendar, Time, Timeline, Gantt and Scrum views against WeKan's code"),
    ('image-decode', 'test_image_decode.py', "PNG, GIF and JPEG decoded for the Map view, pixel for pixel"),
    ('wekan-sync', 'test_wekan_sync.sh', "WeKan's documents and Wena's tables: import, changed fields only, new documents, deletes"),
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
    ('nuklear-card-insert', 'test_nuklear_card_insert.sh', 'Real Move form exact insertion, bounded input and snapshot guards'),
    ('nuklear-card-reorder', 'test_nuklear_card_reorder.sh', 'Real Nuklear indexed card destination and position selection'),
    ('card-move-reorder-sqlite', 'test_card_move_reorder_sqlite.sh', 'Indexed card reorder UI integration and persistence'),
    ('card-actions', 'test_card_actions.sh', "WeKan's Card Actions popup: items, top, bottom, move, archive and negatives"),
    ('nuklear-card-selection', 'test_nuklear_card_selection.sh', 'Shared paginated native card selection panel'),
    ('card-selection', 'test_card_selection.sh', 'Shared scoped card multiselection and pruning'),
    ('card-order', 'test_card_order.sh', 'Shared ordered card snapshots and stale detection'),
    ('wip-limit', 'test_wip_limit.sh', 'Shared WIP decisions, editor transitions and arithmetic bounds'),
    ('card-insert', 'test_card_insert.sh', 'Guarded exact-position card insertion across lists and swimlanes'),
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
    ('wekan-files', 'test_wekan_files.sh', "WeKan's files directory: WRITABLE_PATH, wekan-files beside the executable, db/wekan.sqlite"),
    ('ferretdb-sqlite', 'test_ferretdb_sqlite.sh', "FerretDB's SQLite storage: table names, DDL and documents with their $s"),
    ('ferretdb-roundtrip', 'test_ferretdb_roundtrip.py', 'FerretDB reads what Wena writes and Wena keeps what FerretDB wrote (skips without FerretDB)'),
    ('generated-sources', 'test_generated_sources.py', 'Generated headers (notices, translations, font, SVGs) match their sources'),
    ('wekan-ui-parity', 'test_wekan_ui_parity.py', 'Colors match WeKan, from its captured board UI and stylesheets'),
    ('amiga-desktop', 'test_amiga_desktop.py', 'AmigaOS 4, AROS and AmigaOS 3 desktop builds: pinned images and sources, platform branches, SQLite without WAL'),
    ('mobile-desktop', 'test_mobile_desktop.py', 'Android APK and iOS IPA builds: pins, manifest, Info.plist, platform branches, refusals'),
    ('toolchain', 'test_toolchain.py', 'Per-OS install of compilers, SDKs, NDK and Docker before a build'),
    ('generate-i18n-catalog', 'test_generate_i18n_catalog.py', 'Generate i18n catalog regression checks'),
    ('release-workflow', 'test_release_workflow.py', 'Release workflow regression checks'),
    ('source-structure', 'test_source_structure.py', 'Source structure regression checks'),
    ('target-catalog', 'test_target_catalog.py', 'Target catalog regression checks'),
    ('ui-catalog', 'test_ui_catalog.sh', 'Generated canonical offline UI translations and language fallback'),
    ('ui-contract', 'test_ui_contract.py', 'Ui contract regression checks'),
    ('verify-i18n-catalog', 'test_verify_i18n_catalog.py', 'Verify i18n catalog regression checks'),
)
SERIAL_SUITES = {"desktop", "desktop-package"}
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
    if name in {"nuklear-checklist-contents", "nuklear-paginated-table", "language-storage", "collapse-preferences"} and os.name == "nt":
        return "requires POSIX symlink and file-mode semantics"
    if name == "desktop-package" and (platform.system() != "Linux" or platform.machine().lower() not in {"x86_64", "amd64"}):
        return "desktop packaging is currently verified only on Linux amd64"
    if name == "desktop-package" and not shutil.which("readelf"):
        return "requires readelf (binutils) for actual ELF runtime requirements"
    if name in {"nuklear", "desktop", "desktop-package", "sdl-text-input", "dependency-report"} and not shutil.which("sdl2-config"):
        return "requires SDL2 development files (sdl2-config)"
    if name in {"nuklear-swimlane-resize", "nuklear-card-selection", "nuklear-hierarchy-drag", "nuklear-cross-board-destination", "nuklear-directory-picker", "nuklear-card-drag", "nuklear-reorder-drag", "nuklear-checklist-contents", "nuklear-paginated-table", "svg", "nuklear-checklist-item-move", "nuklear-checklist-move", "desktop", "desktop-package", "nuklear-board", "collapse-preferences", "nuklear-checklists", "nuklear-labels", "nuklear-board-settings", "label-badges", "nuklear-checklist-batch", "nuklear-checklist-order", "board-filter", "nuklear-editor", "nuklear-card-create", "nuklear-card-move", "nuklear-card-reorder", "nuklear-card-description", "nuklear-title-keys", "panel-escape", "nuklear-card-archives", "language-picker", "hierarchy-title", "nuklear-hierarchy-move", "native-theme", "native-font", "dependency-check", "native-feature-i18n", "sdl-text-input", "nuklear"} and not (ROOT / "third_party" / "nuklear" / "nuklear.h").is_file():
        return "requires initialized third_party/nuklear submodule"
    if name in SOURCE_SUITES:
        parent = ROOT.parents[1] if len(ROOT.parents) > 1 else ROOT.parent
        source = Path(os.environ.get("WEKAN_ROOT", str(parent)))
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
    print("Release builds are not tests: build one with `build TARGET`, or all with `build all`.")
    return 1 if counts["FAIL"] else 0


def run_test(name):
    if name == "sanitizers":
        script = ROOT / "tests" / "test_native_sanitizers.sh"
        # Freeze this long-running dispatcher before parallel agents add suites.
        command = test_command(script)
        return subprocess.call([command[0], "-c", script.read_text(encoding="utf-8"), str(script)], cwd=ROOT)
    if name == "all":
        return run_all_tests()
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
        answer = choose("Wena", [("1", "Build"), ("2", "Run"), ("3", "Release"),
                                  ("4", "Tests"), ("5", "Server"), ("6", "Tools"),
                                  ("q", "Quit")])
        if answer == "1":
            build_menu()
        elif answer == "2":
            result = run()
            if result:
                print(f"Run ended with exit code {result}.")
        elif answer == "3":
            release_menu()
        elif answer == "4":
            tests_menu()
        elif answer == "5":
            server_menu()
        elif answer == "6":
            tools_menu()
        else:
            return 0


def usage():
    print("Usage: wena.py --list | build host|all|desktop|desktop-package|TARGET | install host|all|desktop|TARGET | run [ARGS...] | release [next|missing] | tests --list|all|SUITE | server status | tools targets | menu", file=sys.stderr)
    return 2


def main(argv):
    if argv == ["--list"]:
        return list_targets()
    if argv == ["menu"]:
        return menu()
    if len(argv) == 2 and argv[0] == "build":
        return build(argv[1])
    if len(argv) == 2 and argv[0] == "install":
        return install(argv[1])
    if argv[:1] == ["run"]:
        return run(argv[1:])
    if argv[:1] == ["release"] and len(argv) <= 2:
        return release(argv[1] if len(argv) > 1 else "next")
    if argv == ["tests", "--list"]:
        print("all\tAll native/static suites (four parallel workers; shared builds serial)")
        print("sanitizers\tOptional ASan/UBSan native model, UI and SQLite regression subset")
        for name, _filename, description in TEST_SUITES:
            print(f"{name}\t{description}")
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
