#!/usr/bin/env python3
"""Collect verified matrix outputs into deterministic Wena release assets."""

import argparse
import hashlib
from pathlib import Path
import shutil


def ready_targets(catalog: Path):
    records = []
    seen_targets = set()
    seen_assets = set()
    for number, line in enumerate(catalog.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise SystemExit(f"{catalog}:{number}: expected five fields")
        target, display_name, _runner, status, artifact_kind = fields
        if status != "ready":
            continue
        source_name = "wena.exe" if target.startswith("windows-") else "wena"
        asset_name = f"wena-{target}" + (".exe" if source_name.endswith(".exe") else "")
        if target in seen_targets:
            raise SystemExit(f"duplicate ready target: {target}")
        if asset_name in seen_assets:
            raise SystemExit(f"duplicate release asset name: {asset_name}")
        seen_targets.add(target)
        seen_assets.add(asset_name)
        records.append((target, display_name, artifact_kind, source_name, asset_name))
    if not records:
        raise SystemExit("target catalog has no ready targets")
    return records


def artifact_entries(path: Path):
    return sorted(path.rglob("*"))


def sha256(path: Path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def collect(catalog: Path, incoming: Path, output: Path, repository: str, commit: str):
    targets = ready_targets(catalog)
    expected_dirs = {f"wena-{target}" for target, *_rest in targets}
    actual_dirs = {item.name for item in incoming.iterdir()} if incoming.is_dir() else set()
    missing_dirs = sorted(expected_dirs - actual_dirs)
    unexpected_dirs = sorted(actual_dirs - expected_dirs)
    if missing_dirs:
        raise SystemExit("missing Actions artifacts: " + ", ".join(missing_dirs))
    if unexpected_dirs:
        raise SystemExit("unexpected Actions artifacts: " + ", ".join(unexpected_dirs))
    if output.exists() and any(output.iterdir()):
        raise SystemExit(f"release asset output is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    manifest = ["target\tdisplay_name\tartifact_kind\tasset\tbytes\tsha256\tsource_artifact\trepository\tcommit"]
    checksums = []
    for target, display_name, artifact_kind, source_name, asset_name in targets:
        artifact_dir = incoming / f"wena-{target}"
        source = artifact_dir / source_name
        entries = artifact_entries(artifact_dir)
        if entries != [source] or source.is_symlink() or not source.is_file():
            found = ", ".join(str(item.relative_to(artifact_dir)) for item in entries) or "none"
            raise SystemExit(f"{target}: expected only {source_name}; found {found}")
        destination = output / asset_name
        if destination.exists():
            raise SystemExit(f"duplicate release asset: {asset_name}")
        shutil.copyfile(source, destination)
        destination.chmod(source.stat().st_mode & 0o777)
        digest = sha256(destination)
        size = destination.stat().st_size
        manifest.append("\t".join((target, display_name, artifact_kind, asset_name,
                                    str(size), digest, f"wena-{target}", repository, commit)))
        checksums.append(f"{digest}  {asset_name}")

    (output / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")
    (output / "MANIFEST.tsv").write_text("\n".join(manifest) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--incoming", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    collect(args.catalog, args.incoming, args.output, args.repository, args.commit)


if __name__ == "__main__":
    main()
