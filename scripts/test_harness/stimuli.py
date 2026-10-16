"""Stimulus bank loader. Reads JSON files under data/stimuli/ on demand."""
from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from .types import StimulusItem


def _stimulus_root() -> Path:
    """Find data/stimuli/ relative to the repo layout or env override."""
    env = os.environ.get("ATHENA_STIMULI_DIR")
    if env:
        return Path(env)
    # Search upward from this file
    here = Path(__file__).resolve()
    for parent in [here.parent, *here.parents]:
        candidate = parent / "data" / "stimuli"
        if candidate.exists():
            return candidate
    # Last resort: workspace on RunPod
    runpod = Path("/workspace/nimcp/data/stimuli")
    if runpod.exists():
        return runpod
    raise RuntimeError("Cannot locate data/stimuli/ directory")


@dataclass
class StimulusBank:
    """A single stimulus file loaded into memory."""
    domain: str
    version: str
    items: list[StimulusItem]
    metadata: dict

    def __iter__(self) -> Iterator[StimulusItem]:
        return iter(self.items)

    def __len__(self) -> int:
        return len(self.items)

    def by_group(self) -> dict[str, list[StimulusItem]]:
        """Group items by variant_group (for paired tests like anchoring)."""
        groups: dict[str, list[StimulusItem]] = {}
        for item in self.items:
            key = item.variant_group or item.id
            groups.setdefault(key, []).append(item)
        return groups


def load_stimuli(relpath: str) -> StimulusBank:
    """Load a bank by relative path, e.g. 'biases/anchoring.json'."""
    root = _stimulus_root()
    full = root / relpath
    if not full.exists():
        # Try with .json extension if missing
        if not relpath.endswith(".json"):
            full = root / (relpath + ".json")
    if not full.exists():
        raise FileNotFoundError(f"Stimulus file not found: {full}")

    with open(full) as f:
        raw = json.load(f)

    items = [StimulusItem.from_dict(d) for d in raw.get("stimuli", [])]
    return StimulusBank(
        domain=raw.get("test_domain", relpath),
        version=raw.get("version", "1.0"),
        items=items,
        metadata=raw.get("metadata", {}))


def list_banks() -> list[str]:
    """Return all stimulus file paths relative to the stimuli root."""
    root = _stimulus_root()
    return sorted(str(p.relative_to(root)) for p in root.rglob("*.json"))
