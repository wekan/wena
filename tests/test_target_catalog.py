#!/usr/bin/env python3
"""Validate the shared release/build target catalog."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config" / "targets.tsv"
WORKFLOW = ROOT / ".github" / "workflows" / "release-all.yml"


def main() -> None:
    records = []
    for number, line in enumerate(CATALOG.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        assert len(fields) == 5, f"targets.tsv:{number}: expected five fields"
        target, name, runner, status, artifact = fields
        assert re.fullmatch(r"[a-z0-9][a-z0-9_-]*", target)
        assert name and runner and artifact
        assert status in {"ready", "planned"}
        records.append(fields)

    targets = [record[0] for record in records]
    assert len(targets) == len(set(targets)), "duplicate target key"
    assert len(records) == 29, "review the roadmap whenever target coverage changes"

    workflow = WORKFLOW.read_text(encoding="utf-8")
    workflow_targets = re.findall(
        r"^\s+- target: ([a-z0-9_-]+)$", workflow, re.MULTILINE
    )
    ready_targets = [record[0] for record in records if record[3] == "ready"]
    assert workflow_targets == ready_targets, "ready targets and workflow matrix differ"


if __name__ == "__main__":
    main()
