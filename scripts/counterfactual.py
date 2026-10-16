"""CE-9: Counterfactual reasoning curriculum.

Pure-Python curriculum module. The training loop calls
`run_counterfactual_drip` at the same cadence as the canonical / math /
streaming drips. For each call we:

  1. Pick a stage-appropriate scenario (deterministic but rotated).
  2. Build one or more counterfactual probes from the scenario:
       - if_not_cause      : "What if {cause} had not happened?"
       - if_different_action: "What if {agent} had acted differently?"
       - if_earlier        : "What if this had happened sooner?"
       - if_no_witness     : "What if no one had been there to see it?"
  3. Compose the probe into the brain's input space and invoke
     brain.produce_text(intent) to get the counterfactual answer.
  4. Score the answer for counterfactual quality:
        - flip_score    : did the answer AVOID restating the original
                           outcome's keywords? (the whole point — failing
                           to flip means the brain didn't counterfactual-
                           reason at all)
        - recall        : did the answer mention the alternate-world
                           keywords?
        - specificity   : enough distinct content for the answer to be
                           more than a stub.
     Composite weights flip_score highest because flipping the effect is
     the load-bearing signal for counterfactual reasoning.
  5. Feed the score back as a learning signal:
        - composite >= 0.45 → brain.learn_language(answer)
        - composite <  0.45 → brain.train_language(prompt + " " + expected,
                                                    prompt + " " + expected)

This is intentionally NOT Claude-as-teacher — that's CE-19. Here we
generate a deterministic, additional self-supervised signal that
surfaces failure-to-counterfactual-reason as a numeric reward.

Public API:
    run_counterfactual_drip(brain, stage, *, composer, num_probes=2,
                            scenario_index=None, log_every=10)
    score_counterfactual_answer(answer, expected, forbidden) -> dict
    build_probe(scenario, probe_type) -> dict | None
    COUNTERFACTUAL_SCENARIOS, PROBE_TYPES, COUNTERFACTUAL_PASS_THRESHOLD

Stage 1 is a no-op (too early; vocabulary is still bootstrapping).
"""
from __future__ import annotations

import re
from typing import Iterable

# ---------------------------------------------------------------------------
# Scenario library. Each scenario carries:
#   setting : the situating sentence the prompt is built from
#   cause   : noun phrase describing the cause (used in if_not_cause)
#   effect  : the actual outcome (used by tests + sanity)
#   agent   : the agent (used in if_different_action)
#   alternates : per-probe expected_keywords (counterfactual world hints)
#   forbidden  : keywords from the actual outcome — answers that just
#                restate these have NOT counterfactual-reasoned.
# ---------------------------------------------------------------------------

PROBE_TYPES = ["if_not_cause", "if_different_action", "if_earlier", "if_no_witness"]


COUNTERFACTUAL_SCENARIOS: dict[int, list[dict]] = {
    1: [],  # no-op — vocabulary not ready
    2: [
        {
            "setting": "It rained heavily all morning.",
            "cause": "the rain",
            "effect": "the picnic was cancelled",
            "agent": "the family",
            "alternates": {
                "if_not_cause": {"sunny", "picnic", "happened", "outside"},
                "if_different_action": {"indoor", "house", "moved"},
                "if_earlier": {"checked", "weather", "knew"},
                "if_no_witness": {"missed", "unaware"},
            },
            "forbidden": {"cancelled", "rain"},
        },
        {
            "setting": "Tom forgot his umbrella when leaving the house.",
            "cause": "forgetting the umbrella",
            "effect": "Tom got soaked in the storm",
            "agent": "Tom",
            "alternates": {
                "if_not_cause": {"dry", "covered", "warm"},
                "if_different_action": {"raincoat", "borrowed", "ran"},
                "if_earlier": {"remembered", "checked"},
                "if_no_witness": {"alone", "unseen"},
            },
            "forbidden": {"soaked", "wet"},
        },
        {
            "setting": "The cat knocked the vase off the shelf.",
            "cause": "the cat jumping",
            "effect": "the vase shattered on the floor",
            "agent": "the cat",
            "alternates": {
                "if_not_cause": {"intact", "still", "shelf"},
                "if_different_action": {"caught", "saved"},
                "if_earlier": {"warned", "moved", "secured"},
                "if_no_witness": {"hidden", "unseen"},
            },
            "forbidden": {"shattered", "broken"},
        },
        {
            "setting": "The boy gave the apple to his hungry dog.",
            "cause": "giving the apple",
            "effect": "the dog stopped barking",
            "agent": "the dog",
            "alternates": {
                "if_not_cause": {"barked", "loud", "hungry"},
                "if_different_action": {"bone", "treat", "different"},
                "if_earlier": {"fed", "earlier", "morning"},
                "if_no_witness": {"alone", "outside"},
            },
            "forbidden": {"stopped", "quiet"},
        },
        {
            "setting": "The wind blew the door shut.",
            "cause": "the wind",
            "effect": "the boy was locked out",
            "agent": "the boy",
            "alternates": {
                "if_not_cause": {"open", "inside", "warm"},
                "if_different_action": {"propped", "wedged"},
                "if_earlier": {"key", "remembered"},
                "if_no_witness": {"silent", "alone"},
            },
            "forbidden": {"locked", "out"},
        },
    ],
    3: [
        {
            "setting": "The captain ignored the warning signal because he trusted his charts.",
            "cause": "ignoring the warning",
            "effect": "the ship struck the reef",
            "agent": "the captain",
            "alternates": {
                "if_not_cause": {"safe", "passed", "harbor"},
                "if_different_action": {"slowed", "turned", "checked"},
                "if_earlier": {"updated", "verified"},
                "if_no_witness": {"unreported", "lost"},
            },
            "forbidden": {"struck", "reef", "wrecked"},
        },
        {
            "setting": "She wrote the letter at midnight and sealed it carefully.",
            "cause": "writing the letter",
            "effect": "she felt at peace",
            "agent": "she",
            "alternates": {
                "if_not_cause": {"restless", "anxious", "burdened"},
                "if_different_action": {"called", "spoke", "visited"},
                "if_earlier": {"earlier", "younger", "before"},
                "if_no_witness": {"alone", "unread"},
            },
            "forbidden": {"peace", "calm"},
        },
        {
            "setting": "The young scholar published the manuscript before verifying its sources.",
            "cause": "publishing without verifying",
            "effect": "his reputation suffered",
            "agent": "the scholar",
            "alternates": {
                "if_not_cause": {"respected", "credible", "trusted"},
                "if_different_action": {"checked", "delayed", "consulted"},
                "if_earlier": {"reviewed", "earlier"},
                "if_no_witness": {"obscure", "unread"},
            },
            "forbidden": {"suffered", "ruined"},
        },
        {
            "setting": "The wanderer chose the path that climbed into the hills.",
            "cause": "choosing the upward path",
            "effect": "she found the hidden valley",
            "agent": "the wanderer",
            "alternates": {
                "if_not_cause": {"village", "river", "lost"},
                "if_different_action": {"asked", "waited", "turned"},
                "if_earlier": {"map", "guide", "knew"},
                "if_no_witness": {"alone", "unrecorded"},
            },
            "forbidden": {"valley", "hidden"},
        },
        {
            "setting": "The doctor delayed the diagnosis until more tests were done.",
            "cause": "delaying the diagnosis",
            "effect": "the patient lost trust",
            "agent": "the patient",
            "alternates": {
                "if_not_cause": {"trust", "confidence", "reassured"},
                "if_different_action": {"explained", "honest", "shared"},
                "if_earlier": {"sooner", "first"},
                "if_no_witness": {"private", "unsaid"},
            },
            "forbidden": {"lost", "trust"},
        },
    ],
}


# A small stop-list so specificity scoring focuses on content words. Kept
# short deliberately — false-negatives just make the score conservative.
_STOPWORDS = frozenset({
    "a", "an", "the", "and", "or", "but", "if", "then", "of", "in", "on",
    "at", "to", "from", "with", "by", "for", "into", "out", "up", "down",
    "is", "are", "was", "were", "be", "been", "being", "am",
    "do", "does", "did", "have", "has", "had", "having",
    "i", "me", "my", "we", "us", "our", "you", "your", "he", "him", "his",
    "she", "her", "it", "its", "they", "them", "their",
    "this", "that", "these", "those", "there", "here",
    "as", "so", "not", "no", "yes",
    "what", "when", "where", "why", "how", "who", "which",
    "had", "would", "could", "should", "will", "shall", "may", "might",
})


def _tokenize(text: str) -> list[str]:
    """Lowercase + word-only tokenization. Cheap; deterministic."""
    if not text:
        return []
    return [m.group(0) for m in re.finditer(r"[A-Za-z']+", text.lower())]


def _content_tokens(tokens: Iterable[str]) -> list[str]:
    return [t for t in tokens if t not in _STOPWORDS and len(t) > 2]


# ---------------------------------------------------------------------------
# Probe construction
# ---------------------------------------------------------------------------

def build_probe(scenario: dict, probe_type: str) -> dict | None:
    """Build a counterfactual probe from a scenario + probe_type.

    Returns a dict with prompt / expected_keywords / forbidden_keywords /
    probe_type, or None if the scenario lacks alternates for the requested
    probe type (avoids producing un-scoreable probes).
    """
    if not scenario or probe_type not in PROBE_TYPES:
        return None
    alternates = scenario.get("alternates", {}) or {}
    expected = set(alternates.get(probe_type, set()) or set())
    if not expected:
        return None
    setting = scenario.get("setting", "")
    cause = scenario.get("cause", "the cause")
    agent = scenario.get("agent", "the agent")
    forbidden = set(scenario.get("forbidden", set()) or set())

    if probe_type == "if_not_cause":
        prompt = f"{setting} What if {cause} had not happened?"
    elif probe_type == "if_different_action":
        prompt = f"{setting} What if {agent} had acted differently?"
    elif probe_type == "if_earlier":
        prompt = f"{setting} What if this had happened sooner?"
    else:  # if_no_witness
        prompt = f"{setting} What if no one had been there to see it?"

    return {
        "prompt": prompt,
        "expected_keywords": expected,
        "forbidden_keywords": forbidden,
        "probe_type": probe_type,
    }


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

def score_counterfactual_answer(answer: str,
                                 expected: Iterable[str],
                                 forbidden: Iterable[str]) -> dict:
    """Return a dict with flip_score / recall / specificity / composite /
    tokens.

    The composite weights flip_score highest (1.5x) because the whole
    point of counterfactual reasoning is NOT just restating the actual
    outcome. An answer that hits all expected keywords but ALSO restates
    the forbidden outcome should score lower than an answer with partial
    expected coverage and no forbidden words.
    """
    expected_set = {w.lower() for w in (expected or [])}
    forbidden_set = {w.lower() for w in (forbidden or [])}

    answer_tokens = _tokenize(answer or "")
    answer_set = set(answer_tokens)

    # Recall over expected.
    if expected_set:
        recall = len(expected_set & answer_set) / float(len(expected_set))
    else:
        recall = 0.0

    # Flip score: 1.0 means none of the forbidden words appeared.
    if forbidden_set:
        forbidden_hits = len(forbidden_set & answer_set)
        flip_score = 1.0 - (forbidden_hits / float(len(forbidden_set)))
    else:
        flip_score = 1.0

    # Specificity: bounded count of distinct content words.
    distinct_content = len(set(_content_tokens(answer_tokens)))
    specificity = min(1.0, distinct_content / 5.0)

    # Empty answer: by construction recall=0, flip_score=1.0 (didn't
    # repeat the outcome), specificity=0 → composite=0.5.
    composite = (recall + 1.5 * flip_score + 0.5 * specificity) / 3.0

    return {
        "flip_score": flip_score,
        "recall": recall,
        "specificity": specificity,
        "composite": composite,
        "tokens": len(answer_tokens),
    }


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

# Module-level rotation counter so scenarios rotate even when callers
# don't pass their own. Persisted across drip calls within a single
# process; NOT a checkpointed value.
_ROTATION_COUNTER: dict[int, int] = {1: 0, 2: 0, 3: 0}


def _bump_rotation(stage: int) -> int:
    cur = _ROTATION_COUNTER.get(int(stage), 0)
    _ROTATION_COUNTER[int(stage)] = cur + 1
    return cur


COUNTERFACTUAL_PASS_THRESHOLD = 0.45


def _select_scenario(stage: int, scenario_index: int | None) -> tuple[int, dict | None]:
    """Pick scenario by index (mod len), bump module counter when index
    is None. Returns (resolved_index, scenario_dict_or_None)."""
    scenarios = COUNTERFACTUAL_SCENARIOS.get(int(stage), [])
    if not scenarios:
        return 0, None
    if scenario_index is None:
        idx = _bump_rotation(stage)
    else:
        idx = int(scenario_index)
    return idx, scenarios[idx % len(scenarios)]


def _compose_intent(composer, prompt: str):
    """Compose prompt → intent vector. Falls back to zeros."""
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


def run_counterfactual_drip(brain, stage: int, *,
                            composer=None,
                            num_probes: int = 2,
                            scenario_index: int | None = None,
                            log_every: int = 10) -> list[dict] | None:
    """One counterfactual drip. Stage 1 is a no-op.

    Picks a scenario, then rotates through PROBE_TYPES for `num_probes`
    probes, skipping any probe types whose alternates are empty for the
    chosen scenario. For each probe: compose → produce_text → score →
    feedback (learn_language above threshold, train_language re-anchored
    to expected keywords below threshold).

    Returns a list of per-probe result dicts, or None when the stage has
    no scenarios.
    """
    scenarios = COUNTERFACTUAL_SCENARIOS.get(int(stage), [])
    if not scenarios:
        return None

    idx, scenario = _select_scenario(stage, scenario_index)
    if scenario is None:
        return None

    results: list[dict] = []
    n = max(1, int(num_probes))
    probe_idx = 0
    attempts = 0
    # Cap attempts so we don't loop forever if every probe type for this
    # scenario has empty alternates (shouldn't happen by construction,
    # but defensive).
    max_attempts = len(PROBE_TYPES) * 2
    while len(results) < n and attempts < max_attempts:
        ptype = PROBE_TYPES[probe_idx % len(PROBE_TYPES)]
        probe_idx += 1
        attempts += 1
        probe = build_probe(scenario, ptype)
        if probe is None:
            continue

        prompt = probe["prompt"]
        expected = probe["expected_keywords"]
        forbidden = probe["forbidden_keywords"]

        intent_list = _compose_intent(composer, prompt)
        if intent_list is None:
            continue

        try:
            result = brain.produce_text(intent_list)
        except Exception as e:
            print(f"  [CF:s{stage}] produce_text failed ({ptype}): {e}")
            continue

        answer = (result or {}).get("text", "") if isinstance(result, dict) else ""
        confidence = (result or {}).get("confidence", 0.0) if isinstance(result, dict) else 0.0

        score = score_counterfactual_answer(answer, expected, forbidden)

        # Feedback: above threshold reinforce the brain's own answer;
        # below threshold re-anchor toward the prompt + expected keywords
        # so the target shape is reinforced rather than the failed reply.
        try:
            if score["composite"] >= COUNTERFACTUAL_PASS_THRESHOLD and answer.strip():
                brain.learn_language(answer)
            else:
                reanchor = prompt + " " + " ".join(sorted(expected))
                try:
                    brain.train_language(reanchor, reanchor)
                except TypeError:
                    brain.train_language(text=reanchor, target_text=reanchor)
        except Exception as e:
            print(f"  [CF:s{stage}] feedback failed ({ptype}): {e}")

        results.append({
            "stage": int(stage),
            "scenario_index": int(idx),
            "probe_type": ptype,
            "prompt": prompt,
            "answer": answer,
            "confidence": float(confidence),
            "expected_keywords": sorted(expected),
            "forbidden_keywords": sorted(forbidden),
            **score,
        })

        if int(idx) % max(1, int(log_every)) == 0:
            print(f"  [CF:s{stage}] sc#{idx} {ptype} comp={score['composite']:.2f} "
                  f"flip={score['flip_score']:.2f} rec={score['recall']:.2f} "
                  f"spec={score['specificity']:.2f} tok={score['tokens']} "
                  f"conf={confidence:.2f}")

    return results
