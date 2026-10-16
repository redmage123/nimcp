"""CE-8: Theory of Mind probes — belief / desire / intent / emotion.

Pure-Python curriculum module. Sibling to CE-1 (storytelling). For each call
we generate scenarios with a known mental state and check whether the brain's
reply contains keywords reflecting it. This is a self-supervised probe, NOT
Claude-as-teacher (that's CE-19).

For each scenario we:
  1. Pick a probe type (false_belief / desire / intent / emotion) and build a
     short prompt of the form "{scenario}. What does {agent} think/want/...?"
  2. Compose the prompt into the brain's input space and invoke
     brain.produce_text(intent) to get the brain's reply.
  3. Score the reply for ToM signal via two lightweight components:
        - tom_recall   (fraction of expected keywords present in answer)
        - specificity  (content-word count / 4, capped at 1.0)
     Composite weights recall double:  (2*recall + specificity) / 3.
  4. Feed the score back as a learning signal:
        - composite >= 0.45 → brain.learn_language(answer)
        - composite <  0.45 → brain.train_language(prompt+keywords,
                                                   prompt+keywords)
            (corrective: re-anchor the prompt with its expected keywords so
             the brain learns the target shape rather than its own collapse)

Stage 1 is a no-op. Stages 2 and 3 each carry a small curated scenario set.

Public API:
    TOM_SCENARIOS, PROBE_TYPES, TOM_PASS_THRESHOLD
    build_probe(scenario, probe_type) -> dict | None
    score_tom_answer(answer, expected_keywords) -> dict
    run_tom_drip(brain, stage, *, composer=None, num_probes=2,
                 scenario_index=None, log_every=10) -> list[dict] | None
"""
from __future__ import annotations

import re
from typing import Iterable

# ---------------------------------------------------------------------------
# Probe types (rotation order)
# ---------------------------------------------------------------------------

PROBE_TYPES = ["false_belief", "desire", "intent", "emotion"]


# ---------------------------------------------------------------------------
# Stage-appropriate scenarios. Each entry is a dict with:
#   scenario : str            — the situation text
#   agent    : str            — the protagonist whose mental state we probe
#   expected : dict[str, set] — expected keywords per probe_type. An empty
#                               set means "this probe does not apply to this
#                               scenario" and the build/drip layer will skip it.
# ---------------------------------------------------------------------------

TOM_SCENARIOS: dict[int, list[dict]] = {
    1: [],  # no-op — too early
    2: [
        {
            "scenario": "Anna put her ball in the red box and went outside. "
                        "While she was gone, her brother moved the ball to "
                        "the blue box.",
            "agent": "Anna",
            "expected": {
                "false_belief": {"red", "box"},
                "desire": {"ball", "play"},
                "intent": {"find", "look"},
                "emotion": {"surprised", "confused"},
            },
        },
        {
            "scenario": "Tom hid the cookie behind the curtain. His sister "
                        "searched everywhere except behind the curtain.",
            "agent": "Tom",
            "expected": {
                "false_belief": {"curtain", "behind"},
                "desire": {"cookie", "eat"},
                "intent": {"hide"},
                "emotion": {"happy", "clever"},
            },
        },
        {
            "scenario": "The cat watched the bird through the window. The "
                        "bird flew away when the man opened the door.",
            "agent": "cat",
            "expected": {
                "false_belief": {"window", "still"},
                "desire": {"bird", "catch"},
                "intent": {"hunt", "watch"},
                "emotion": {"disappointed", "frustrated"},
            },
        },
        {
            "scenario": "Sara baked a cake for her friend. Her friend was "
                        "allergic to nuts but Sara did not know that.",
            "agent": "Sara",
            "expected": {
                "false_belief": {"safe", "fine"},
                "desire": {"please", "gift"},
                "intent": {"bake", "share"},
                "emotion": {"happy", "proud"},
            },
        },
        {
            "scenario": "The dog buried the bone under the tree. The boy "
                        "raked the leaves and covered the spot.",
            "agent": "dog",
            "expected": {
                "false_belief": {"bone", "tree"},
                "desire": {"bone", "save"},
                "intent": {"return", "dig"},
                "emotion": {"confused", "worried"},
            },
        },
    ],
    3: [
        {
            "scenario": "Maria told John the meeting was at three. She knew "
                        "he would tell Lisa, who hated being late. Maria "
                        "actually wanted Lisa to arrive early.",
            "agent": "Maria",
            "expected": {
                "false_belief": {"three", "meeting"},
                "desire": {"lisa", "early"},
                "intent": {"manipulate", "trick"},
                "emotion": {"sly", "satisfied"},
            },
        },
        {
            "scenario": "The old man left the door unlocked because he "
                        "expected his daughter, but a stranger walked in "
                        "instead.",
            "agent": "man",
            "expected": {
                "false_belief": {"daughter", "expected"},
                "desire": {"see", "daughter"},
                "intent": {"welcome"},
                "emotion": {"surprised", "afraid"},
            },
        },
        {
            "scenario": "She wrote the letter knowing it would never be "
                        "sent, only because writing it felt like enough.",
            "agent": "she",
            "expected": {
                "false_belief": set(),
                "desire": {"closure", "release"},
                "intent": {"write", "express"},
                "emotion": {"sad", "peaceful"},
            },
        },
        {
            "scenario": "He laughed loudly at the joke even though he had "
                        "not understood it, because the others were "
                        "laughing.",
            "agent": "he",
            "expected": {
                "false_belief": {"funny", "understood"},
                "desire": {"belong", "fit"},
                "intent": {"pretend"},
                "emotion": {"insecure", "anxious"},
            },
        },
        {
            "scenario": "The student gave the wrong answer on purpose so "
                        "the teacher would not call on him again.",
            "agent": "student",
            "expected": {
                "false_belief": {"smart", "knows"},
                "desire": {"avoid", "quiet"},
                "intent": {"deceive"},
                "emotion": {"shy", "tired"},
            },
        },
        {
            "scenario": "When she returned to the city the towers were "
                        "taller but the song was the same; she felt both "
                        "lost and at home.",
            "agent": "she",
            "expected": {
                "false_belief": set(),
                "desire": {"return", "home"},
                "intent": {"remember"},
                "emotion": {"nostalgic", "ambivalent"},
            },
        },
    ],
}


# Small stop-list — content-word counting needs to ignore connectives /
# pronouns / aux verbs. Mirrors the CE-1 list for cross-module consistency.
_STOPWORDS = frozenset({
    "a", "an", "the", "and", "or", "but", "if", "then", "of", "in", "on",
    "at", "to", "from", "with", "by", "for", "into", "out", "up", "down",
    "is", "are", "was", "were", "be", "been", "being", "am",
    "do", "does", "did", "have", "has", "had", "having",
    "i", "me", "my", "we", "us", "our", "you", "your", "he", "him", "his",
    "she", "her", "it", "its", "they", "them", "their",
    "this", "that", "these", "those", "there", "here",
    "as", "so", "not", "no", "yes",
})


def _tokenize(text: str) -> list[str]:
    """Lowercase + word-only tokenization. Cheap; deterministic."""
    if not text:
        return []
    return [m.group(0) for m in re.finditer(r"[A-Za-z']+", text.lower())]


def _content_token_count(tokens: list[str]) -> int:
    """Number of distinct non-stopword tokens length > 2."""
    return len({t for t in tokens if t not in _STOPWORDS and len(t) > 2})


# ---------------------------------------------------------------------------
# Probe construction
# ---------------------------------------------------------------------------

_PROBE_TEMPLATES = {
    "false_belief": "In the story: {scenario} What does {agent} think?",
    "desire":       "{scenario} What does {agent} want?",
    "intent":       "{scenario} What will {agent} do next?",
    "emotion":      "{scenario} How does {agent} feel?",
}


def build_probe(scenario: dict, probe_type: str) -> dict | None:
    """Build a probe dict for `probe_type` against `scenario`.

    Returns None when the scenario has no expected keywords for the requested
    probe_type (e.g. some stage-3 scenarios don't carry a meaningful
    false-belief signal). Callers should treat None as "skip this probe".
    """
    if probe_type not in _PROBE_TEMPLATES:
        return None
    expected = scenario.get("expected", {}).get(probe_type, set())
    if not expected:
        return None
    template = _PROBE_TEMPLATES[probe_type]
    prompt = template.format(scenario=scenario["scenario"],
                             agent=scenario["agent"])
    return {
        "prompt": prompt,
        "expected_keywords": set(expected),
        "probe_type": probe_type,
    }


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

def score_tom_answer(answer: str, expected_keywords: Iterable[str]) -> dict:
    """Score a free-text answer against the expected keyword set.

    Components:
      tom_recall  = |expected ∩ answer_tokens| / max(1, |expected|)
      specificity = min(1.0, distinct_content_word_count / 4.0)
      composite   = (2 * tom_recall + specificity) / 3.0

    Recall is weighted double — the headline ToM signal is whether the brain
    surfaces the expected mental-state keyword, with specificity as a
    sanity-check that the answer isn't a one-word collapse.
    """
    tokens = _tokenize(answer or "")
    expected_set = {str(k).lower() for k in expected_keywords if k}
    if expected_set:
        token_set = set(tokens)
        hits = sum(1 for k in expected_set if k in token_set)
        tom_recall = hits / float(len(expected_set))
    else:
        tom_recall = 0.0

    distinct_content = _content_token_count(tokens)
    specificity = min(1.0, distinct_content / 4.0)

    composite = (2.0 * tom_recall + specificity) / 3.0
    return {
        "tom_recall": tom_recall,
        "specificity": specificity,
        "composite": composite,
        "tokens": len(tokens),
    }


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

# Module-level rotation counter — bumps each unindexed call so scenarios
# rotate. Not checkpointed; resuming from 0 is acceptable.
_SCENARIO_COUNTER: dict[int, int] = {1: 0, 2: 0, 3: 0}
# Per-stage probe-type rotation cursor so successive calls don't always start
# at "false_belief".
_PROBE_CURSOR: dict[int, int] = {1: 0, 2: 0, 3: 0}


def _bump_scenario(stage: int) -> int:
    cur = _SCENARIO_COUNTER.get(int(stage), 0)
    _SCENARIO_COUNTER[int(stage)] = cur + 1
    return cur


TOM_PASS_THRESHOLD = 0.45


def _resolve_intent(composer, prompt: str):
    """Build the intent vector for `prompt`, falling back to zeros."""
    intent = None
    if composer is not None:
        try:
            intent = composer.compose(text=prompt, modality="text")
        except Exception:
            intent = None
    if intent is None:
        try:
            import numpy as _np
            intent = _np.zeros(1024, dtype=_np.float32)
        except Exception:
            return None
    try:
        return intent.tolist() if hasattr(intent, "tolist") else list(intent)
    except Exception:
        return None


def run_tom_drip(brain, stage: int, *,
                 composer=None,
                 num_probes: int = 2,
                 scenario_index: int | None = None,
                 log_every: int = 10,
                 ) -> list[dict] | None:
    """One ToM probe pass. Returns a list of per-probe score dicts.

    Stage 1 is a no-op. For stages 2+ we pick a scenario (rotated unless
    `scenario_index` is given) and run up to `num_probes` probe types against
    it, skipping probe types whose expected-keyword set is empty.

    Each probe issues a `brain.produce_text(intent)` call; the result is
    scored and fed back via `learn_language` (above threshold) or
    `train_language` re-anchoring on prompt + keywords (below threshold).
    Brain-side failures are caught and logged; the drip continues.
    """
    scenarios = TOM_SCENARIOS.get(int(stage), [])
    if not scenarios:
        return None

    if scenario_index is not None:
        idx = int(scenario_index) % len(scenarios)
    else:
        idx = _bump_scenario(stage) % len(scenarios)
    scenario = scenarios[idx]

    # Rotate the starting probe-type cursor (only when picking implicitly).
    if scenario_index is None:
        cursor = _PROBE_CURSOR.get(int(stage), 0)
        _PROBE_CURSOR[int(stage)] = (cursor + 1) % len(PROBE_TYPES)
    else:
        cursor = 0

    results: list[dict] = []
    attempts = 0
    probes_done = 0
    target = max(0, int(num_probes))
    # Walk the rotation; at most len(PROBE_TYPES) attempts to find non-empty
    # probe types for this scenario.
    while probes_done < target and attempts < len(PROBE_TYPES):
        ptype = PROBE_TYPES[(cursor + attempts) % len(PROBE_TYPES)]
        attempts += 1
        probe = build_probe(scenario, ptype)
        if probe is None:
            continue

        prompt = probe["prompt"]
        expected = probe["expected_keywords"]

        intent_list = _resolve_intent(composer, prompt)
        if intent_list is None:
            continue

        try:
            result = brain.produce_text(intent_list)
        except Exception as e:
            print(f"  [ToM:s{stage}] produce_text failed ({ptype}): {e}")
            probes_done += 1
            continue

        answer = ""
        if isinstance(result, dict):
            answer = result.get("text", "") or ""
        score = score_tom_answer(answer, expected)

        # Feedback signal.
        try:
            if score["composite"] >= TOM_PASS_THRESHOLD and answer.strip():
                brain.learn_language(answer)
            else:
                anchor = prompt + " " + " ".join(sorted(expected))
                try:
                    brain.train_language(anchor, anchor)
                except TypeError:
                    brain.train_language(text=anchor, target_text=anchor)
        except Exception as e:
            print(f"  [ToM:s{stage}] feedback signal failed ({ptype}): {e}")

        if int(idx) % max(1, int(log_every)) == 0:
            print(f"  [ToM:s{stage}] sc#{idx} {ptype} "
                  f"composite={score['composite']:.2f} "
                  f"recall={score['tom_recall']:.2f} "
                  f"spec={score['specificity']:.2f} "
                  f"tok={score['tokens']}")

        results.append({
            "prompt": prompt,
            "answer": answer,
            "probe_type": ptype,
            "expected_keywords": sorted(expected),
            **score,
        })
        probes_done += 1

    if not results:
        return None
    return results
