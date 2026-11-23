"""Append-only record of every run: what ran, on which code, and what it scored."""
from __future__ import annotations

import json
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCOREBOARD = Path(os.environ.get("ATHENA_SCOREBOARD", ROOT / "runs" / "scoreboard.jsonl"))


def code_version() -> str:
    """Commit hash, with '+dirty' when the working tree has changes."""
    try:
        head = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                              capture_output=True, text=True, check=True).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--", "athena"], cwd=ROOT,
                               capture_output=True, text=True).stdout.strip()
        return head + ("+dirty" if dirty else "")
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def record(task: str, kind: str, name: str, metrics: dict, config: dict | None = None,
           seconds: float | None = None, path: Path | None = None) -> dict:
    entry = {
        "time": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "task": task,
        "kind": kind,          # "baseline" | "train" | "ablation"
        "name": name,
        "code": code_version(),
        "metrics": metrics,
        "config": config or {},
        "seconds": None if seconds is None else round(seconds, 1),
    }
    path = path or SCOREBOARD
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a") as f:
        f.write(json.dumps(entry) + "\n")
    return entry


def load(path: Path | None = None, task: str | None = None) -> list[dict]:
    path = path or SCOREBOARD
    if not path.exists():
        return []
    rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    return [r for r in rows if task is None or r["task"] == task]


def best_baseline(task: str, metric: str = "top1", path: Path | None = None) -> tuple[str, float] | None:
    rows = [r for r in load(path, task) if r["kind"] == "baseline" and metric in r["metrics"]]
    if not rows:
        return None
    best = max(rows, key=lambda r: r["metrics"][metric])
    return best["name"], best["metrics"][metric]
