#!/usr/bin/env python3
"""Tests for Layer B confabulation-mitigation curriculum (idk_unanswerables.json).

Layer B ships training-data side: explicit "I don't know" supervised
examples. Effective only after the brain has language tokens (stage 3+).
This test file pins the JSON schema, the curriculum integration, and the
stage gate / sampling rate constants so the corpus is ready when training
reaches stage 3.

Run:
    python3 -m pytest tests/unit/test_idk_curriculum.py -v
"""
from __future__ import annotations

import json
import os
import random
import re
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
IDK_PATH = REPO_ROOT / "data" / "stimuli" / "cognitive" / "idk_unanswerables.json"
TRAINER_PATH = REPO_ROOT / "scripts" / "immerse_athena.py"

# Keyword set the eval / scorer uses to decide whether a response is
# considered a valid "I don't know" refusal. Must stay in sync with the
# scorer params in idk_unanswerables.json.
KEYWORD_OPTIONS = [
    "don't know", "do not know", "unknown", "no answer",
    "cannot", "not applicable", "meaningless", "no sense",
]

# Six adversarial-unanswerable categories that MUST all be represented.
REQUIRED_CATEGORIES = {
    "synesthetic_confusion",
    "abstract_as_physical",
    "temporal_spatial_nonsense",
    "category_error",
    "recursive_impossibility",
    "false_premise",
}


# =============================================================================
# Helpers
# =============================================================================

def _load_idk_json():
    """Load the idk_unanswerables.json file once for the suite."""
    assert IDK_PATH.is_file(), f"IDK stimulus file missing at {IDK_PATH}"
    with open(IDK_PATH) as f:
        return json.load(f)


@pytest.fixture(scope="module")
def idk_doc():
    return _load_idk_json()


# =============================================================================
# Test 1 — file exists at expected path
# =============================================================================

def test_idk_json_file_exists():
    """The idk_unanswerables.json file must be at the expected path."""
    assert IDK_PATH.is_file(), (
        f"Expected file at {IDK_PATH}. Layer B confabulation-mitigation "
        "curriculum requires this file to exist alongside the tier*.json "
        "stimuli."
    )


# =============================================================================
# Test 2 — JSON is valid + has the expected schema
# =============================================================================

def test_idk_json_schema(idk_doc):
    """JSON must declare test_domain, version, metadata, stimuli at the top level."""
    for key in ("test_domain", "version", "metadata", "stimuli"):
        assert key in idk_doc, f"Top-level JSON missing required key: {key}"
    assert idk_doc["test_domain"].startswith("cognitive."), (
        f"test_domain should be a cognitive.* identifier, got "
        f"{idk_doc['test_domain']!r}"
    )
    assert isinstance(idk_doc["version"], str)
    assert isinstance(idk_doc["metadata"], dict)
    assert isinstance(idk_doc["stimuli"], list)
    # Metadata convenience fields used by the trainer
    md = idk_doc["metadata"]
    assert "subtasks" in md and isinstance(md["subtasks"], list)
    assert "n_items" in md and isinstance(md["n_items"], int)


# =============================================================================
# Test 3 — at least 60 unanswerable items (across the 6 adversarial categories)
# =============================================================================

def test_at_least_60_unanswerables(idk_doc):
    """The 6 adversarial-unanswerable categories must yield >= 60 items total."""
    unans = [s for s in idk_doc["stimuli"]
             if s.get("metadata", {}).get("category") in REQUIRED_CATEGORIES]
    assert len(unans) >= 60, (
        f"Need >=60 adversarial unanswerables across the 6 categories; "
        f"found {len(unans)}"
    )


# =============================================================================
# Test 4 — all 6 subtask categories must be present
# =============================================================================

def test_all_six_categories_present(idk_doc):
    """Every required adversarial-unanswerable category must have >= 10 items."""
    by_cat: dict[str, int] = {}
    for s in idk_doc["stimuli"]:
        cat = s.get("metadata", {}).get("category")
        if cat:
            by_cat[cat] = by_cat.get(cat, 0) + 1
    missing = REQUIRED_CATEGORIES - set(by_cat)
    assert not missing, (
        f"These adversarial-unanswerable categories are missing: {missing}"
    )
    # Each should have at least 10 items (per spec)
    for cat in REQUIRED_CATEGORIES:
        assert by_cat[cat] >= 10, (
            f"Category {cat!r} has only {by_cat[cat]} items — need >= 10"
        )


# =============================================================================
# Test 5 — every item has the required fields
# =============================================================================

def test_each_item_has_required_fields(idk_doc):
    """Every stimulus must have prompt, expected, scoring, variant_group."""
    for i, s in enumerate(idk_doc["stimuli"]):
        for field in ("id", "prompt", "expected", "scoring", "variant_group"):
            assert field in s, (
                f"stimuli[{i}] (id={s.get('id', '?')}) missing field: {field}"
            )
        assert isinstance(s["prompt"], str) and s["prompt"].strip(), (
            f"stimuli[{i}] prompt is empty"
        )
        assert isinstance(s["scoring"], dict)
        assert "type" in s["scoring"] and "params" in s["scoring"]


# =============================================================================
# Test 6 — every "expected" string lines up with at least one keyword option
# =============================================================================

def test_expected_matches_keyword_options(idk_doc):
    """Each item's `expected` must contain at least one keyword from the matcher list.

    The runtime scorer is keyword-based, so the supervised label string we
    train against must itself match one of those keywords (otherwise the
    eval would mark our own training answer as "wrong").
    """
    keyword_lc = [kw.lower() for kw in KEYWORD_OPTIONS]
    for s in idk_doc["stimuli"]:
        expected = s["expected"]
        # `expected` may be a string or a list of acceptable strings
        if isinstance(expected, list):
            candidates = [str(x).lower() for x in expected]
        else:
            candidates = [str(expected).lower()]
        ok = any(any(kw in cand for kw in keyword_lc) for cand in candidates)
        assert ok, (
            f"item id={s.get('id', '?')!r} expected={expected!r} matches no "
            f"keyword from {KEYWORD_OPTIONS}"
        )


# =============================================================================
# Test 7 — scoring type is contains_any or exact_match with proper params
# =============================================================================

def test_scoring_type_and_params(idk_doc):
    """Scoring must be contains_any (with options list) or exact_match."""
    for s in idk_doc["stimuli"]:
        scoring = s["scoring"]
        stype = scoring.get("type")
        params = scoring.get("params", {})
        assert stype in ("contains_any", "exact_match"), (
            f"item id={s.get('id', '?')!r}: unsupported scoring type {stype!r}"
        )
        if stype == "contains_any":
            opts = params.get("options")
            assert isinstance(opts, list) and opts, (
                f"item id={s.get('id', '?')!r} contains_any without "
                f"non-empty options list"
            )
            # All options must be among the eval's accepted keyword set
            keyword_lc = {kw.lower() for kw in KEYWORD_OPTIONS}
            for o in opts:
                assert o.lower() in keyword_lc, (
                    f"item id={s.get('id', '?')!r}: scoring option {o!r} "
                    f"is not part of the eval's keyword set "
                    f"{KEYWORD_OPTIONS}"
                )


# =============================================================================
# Test 8 — trainer references the new JSON
# =============================================================================

def test_trainer_loads_new_json():
    """scripts/immerse_athena.py must reference idk_unanswerables."""
    assert TRAINER_PATH.is_file(), f"trainer missing at {TRAINER_PATH}"
    text = TRAINER_PATH.read_text()
    assert "idk_unanswerables" in text, (
        "scripts/immerse_athena.py must reference 'idk_unanswerables' so the "
        "Layer B corpus is wired into the curriculum"
    )


# =============================================================================
# Test 9 — curriculum has a stage gate AND a sampling-rate constant
# =============================================================================

def test_curriculum_stage_gate_and_sampling_rate():
    """Trainer must define IDK_STAGE_GATE and IDK_SAMPLING_RATE constants."""
    text = TRAINER_PATH.read_text()
    # Stage gate constant
    m = re.search(r"^IDK_STAGE_GATE\s*=\s*(\d+)", text, re.MULTILINE)
    assert m, "IDK_STAGE_GATE constant not defined in trainer"
    stage_gate = int(m.group(1))
    assert stage_gate >= 3, (
        f"IDK_STAGE_GATE should be >=3 (language tokens come online at "
        f"stage 3); got {stage_gate}"
    )
    # Sampling rate constant
    m = re.search(r"^IDK_SAMPLING_RATE\s*=\s*([0-9.]+)", text, re.MULTILINE)
    assert m, "IDK_SAMPLING_RATE constant not defined in trainer"
    rate = float(m.group(1))
    assert 0.0 < rate <= 0.5, (
        f"IDK_SAMPLING_RATE should be a small positive fraction (~0.10); "
        f"got {rate}"
    )


# =============================================================================
# Test 10 — random sample of 10 prompts pass a Q-pattern check
# =============================================================================

def test_random_sample_prompts_look_like_questions(idk_doc):
    """Sample 10 prompts and check they are plausibly question-shaped.

    Adversarial unanswerables should *look like* questions (end in ?, or
    start with a Q-word), even if the question itself is nonsensical.
    OOD-noise items are exempt — they intentionally don't follow
    Q-patterns. So we sample only from the 6 adversarial categories.
    """
    rng = random.Random(2026_04_26)
    adversarial = [s for s in idk_doc["stimuli"]
                   if s.get("metadata", {}).get("category") in REQUIRED_CATEGORIES]
    assert len(adversarial) >= 10
    sample = rng.sample(adversarial, 10)

    q_words = ("what", "how", "why", "which", "where", "when", "who",
               "is", "are", "does", "do", "did", "can", "could",
               "would", "should", "will")
    failures = []
    for s in sample:
        prompt = s["prompt"].strip()
        ends_with_q = prompt.endswith("?")
        starts_with_q = prompt.lower().split(maxsplit=1)[0].rstrip(",.!?:;") in q_words
        if not (ends_with_q or starts_with_q):
            failures.append((s.get("id"), prompt))
    assert not failures, (
        f"These sampled adversarial prompts don't look like questions: "
        f"{failures}"
    )


# =============================================================================
# Allow direct execution
# =============================================================================

if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
