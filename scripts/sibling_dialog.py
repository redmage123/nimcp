"""CE-3: Self-play / sibling dialog production + turn-taking coherence.

Pure-Python curriculum module. The training loop calls `run_sibling_drip`
at the same cadence as the canonical / math / streaming / storytelling
drips. For each call we:

  1. Pick a stage-appropriate dialog topic (deterministic but rotated).
  2. Drive a multi-turn alternating conversation between the brain and a
     deterministic *sibling* whose lines are produced from a small palette
     of conversational moves (mirror / ask_why / extend / doubt / name_it).
  3. Score each brain turn for:
        - continuity      (overlap with previous turn — sibling line or topic)
        - topic_stability (overlap with the topic itself)
        - composite       (avg of the two)
  4. Aggregate the per-turn composites and feed back as a learning signal:
        - mean composite >= 0.45 → brain.learn_language(joined brain turns)
                                   (positive consolidation of the dialog)
        - mean composite <  0.45 → brain.train_language(topic, topic)
                                   (re-anchor on the topic)

This is intentionally NOT Claude-as-the-sibling — that's CE-19. The sibling
here is a deterministic template-driven palette, used to give the brain a
stable, repeatable turn-taking signal that surfaces drift / collapse as a
numeric reward.

Public API:
    run_sibling_drip(brain, stage, *, composer=None, num_turns=4,
                     topic_index=None, log_every=10) -> dict | None
    sibling_response(brain_line, prev_topic, move) -> str
    score_dialog_turn(prev_line, brain_line, topic) -> dict
    TOPIC_SEEDS, SIBLING_MOVES, COHERENCE_PASS_THRESHOLD

Stage 1 is a no-op (vocabulary is still bootstrapping).
"""
from __future__ import annotations

import re
from typing import Iterable

# ---------------------------------------------------------------------------
# Stage-appropriate dialog topics. Curated short, evocative kickoff lines.
# Stage 2 sits in a concrete / sensory register; stage 3 admits more
# abstract or multi-clause shapes. Sibling moves anchor against the topic
# so the brain has a stable referent across turns.
# ---------------------------------------------------------------------------

TOPIC_SEEDS: dict[int, list[str]] = {
    1: [],  # no-op — vocabulary not ready
    2: [
        "the cat that watches the bird",
        "the small stone shaped like a heart",
        "the dog running in the field",
        "the rain on the roof at night",
        "the lantern in the dark forest",
        "the boy who carried the apple to the river",
    ],
    3: [
        "the choice at the crossroads",
        "the promise made and kept",
        "the letter sealed at midnight",
        "the song the river makes under the snow",
        "the towers of the city after many years",
        "the fire across the valley they walked toward without speaking",
    ],
}


SIBLING_MOVES: list[str] = ["mirror", "ask_why", "extend", "doubt", "name_it"]


# Move rotation order within a single drip call. We deliberately put
# `name_it` between `extend` and `doubt` so the conversation re-anchors
# on the topic in the middle of the rotation, not always at the end.
_MOVE_ROTATION: list[str] = ["mirror", "ask_why", "extend", "name_it", "doubt"]


# A small stop-list mirroring CE-1's spirit. Conservative: false-negatives
# (rare content words filtered out) just make the score conservative,
# which is fine for a soft signal.
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


def _content_words_list(text: str) -> list[str]:
    """Order-preserving content words (for templates that want a prefix /
    suffix slice)."""
    return [t for t in _tokenize(text) if t not in _STOPWORDS and len(t) > 2]


def _content_words(text: str) -> set[str]:
    return set(_content_words_list(text))


# ---------------------------------------------------------------------------
# Sibling response templates
# ---------------------------------------------------------------------------

def sibling_response(brain_line: str, prev_topic: str, move: str) -> str:
    """Produce the sibling's deterministic reply for `move`.

    Templates:
      mirror   → "Yes, and "  + first 6 content words of brain_line + "."
      ask_why  → "Why "       + first content word of brain_line + "?"
      extend   → "And then "  + last 8 content words of brain_line + "."
      doubt    → "Are you sure about " + first content word (or topic) + "?"
      name_it  → topic + "?"   (re-anchors when the brain drifts)

    Empty / whitespace-only `brain_line` always falls back to `name_it`
    regardless of the requested move.
    """
    topic = (prev_topic or "").strip()

    if not (brain_line or "").strip():
        # Always fall back to anchoring on the topic when the brain
        # produced nothing usable.
        return (topic + "?") if topic else "?"

    words = _content_words_list(brain_line)

    if move == "mirror":
        head = words[:6] if words else _tokenize(brain_line)[:6]
        body = " ".join(head)
        return ("Yes, and " + body + ".") if body else "Yes, and that."

    if move == "ask_why":
        first = words[0] if words else (_tokenize(brain_line)[:1] or [""])[0]
        return ("Why " + first + "?") if first else "Why?"

    if move == "extend":
        tail = words[-8:] if words else _tokenize(brain_line)[-8:]
        body = " ".join(tail)
        return ("And then " + body + ".") if body else "And then it went on."

    if move == "doubt":
        first = words[0] if words else None
        anchor = first if first else (topic if topic else "that")
        return "Are you sure about " + anchor + "?"

    if move == "name_it":
        return (topic + "?") if topic else "?"

    # Unknown move → safe fallback to name_it.
    return (topic + "?") if topic else "?"


# ---------------------------------------------------------------------------
# Per-turn scoring
# ---------------------------------------------------------------------------

def score_dialog_turn(prev_line: str, brain_line: str, topic: str) -> dict:
    """Score a single brain turn against (a) the previous turn (sibling
    line or the topic, when this is the kickoff) and (b) the topic itself.

    Returns a dict with continuity, topic_stability, token_count,
    composite. continuity / topic_stability are in [0, 1].
    """
    prev_words = _content_words(prev_line)
    topic_words = _content_words(topic)
    brain_tokens = _tokenize(brain_line)
    brain_words = set(t for t in brain_tokens
                      if t not in _STOPWORDS and len(t) > 2)

    if prev_words:
        continuity = len(brain_words & prev_words) / float(len(prev_words))
    else:
        continuity = 0.0

    if topic_words:
        topic_stability = (len(brain_words & topic_words)
                           / float(len(topic_words)))
    else:
        topic_stability = 0.0

    composite = (continuity + topic_stability) / 2.0

    return {
        "continuity": continuity,
        "topic_stability": topic_stability,
        "token_count": len(brain_tokens),
        "composite": composite,
    }


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

# Module-level rotation counter so topics rotate even when callers don't
# pass their own. Persisted within a single process; not checkpointed —
# re-rotating from 0 on resume is acceptable for a soft signal.
_TOPIC_ROTATION: dict[int, int] = {1: 0, 2: 0, 3: 0}


def _bump_topic_rotation(stage: int) -> int:
    cur = _TOPIC_ROTATION.get(int(stage), 0)
    _TOPIC_ROTATION[int(stage)] = cur + 1
    return cur


COHERENCE_PASS_THRESHOLD = 0.45


def _compose_intent(composer, text: str):
    """Build a 1024-dim intent vector for `brain.produce_text(intent)`.

    Returns a Python list (the daemon RPC layer accepts lists more
    reliably than numpy arrays). Falls back to a zero vector when
    composer is None or composer.compose() raises.
    """
    intent = None
    if composer is not None:
        try:
            intent = composer.compose(text=text, modality="text")
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


def run_sibling_drip(brain, stage: int, *,
                     composer=None,
                     num_turns: int = 4,
                     topic_index: int | None = None,
                     log_every: int = 10,
                     ) -> dict | None:
    """Run one alternating brain↔sibling conversation.

    Stage 1 is a no-op (returns None). For stages 2 and 3:

      * Pick a topic from `TOPIC_SEEDS[stage]` using `topic_index`
        (default: bump module counter).
      * Turn 0: brain receives the topic as the kickoff prompt.
      * Turns 1..num_turns-1: sibling produces a deterministic line based
        on the previous brain turn + rotated move palette; the brain
        produces a reply to (sibling_line + " " + topic).
      * Per-turn score: continuity vs previous turn (sibling line, or
        topic for the kickoff), topic_stability vs topic.
      * Aggregate: mean composite across the brain's turns. Above the
        threshold → `brain.learn_language(joined brain turns)` (positive
        consolidation). Below → `brain.train_language(topic, topic)`
        (re-anchor on the topic).

    `composer` is the same Composer used elsewhere in the training loop
    (compose(text=..., modality="text") → numpy float32 length 1024).

    `topic_index` overrides the module rotation counter (useful for
    tests + deterministic replays); when None the counter advances.

    Returns the aggregated metrics dict, or None when the stage has no
    topics or the kickoff produce_text fails before any turn completes.
    """
    topics = TOPIC_SEEDS.get(int(stage), [])
    if not topics:
        return None

    if topic_index is not None:
        rot = int(topic_index)
    else:
        rot = _bump_topic_rotation(stage)

    topic = topics[rot % len(topics)]
    if not topic:
        return None

    n_turns = max(1, int(num_turns))

    brain_turns: list[str] = []
    per_turn_scores: list[dict] = []
    sibling_lines: list[str] = []
    move_index = 0
    prev_line = topic  # for turn 0, "previous" is the topic kickoff

    for turn in range(n_turns):
        if turn == 0:
            prompt_text = topic
        else:
            # Sibling speaks first, then the brain replies to
            # (sibling line + " " + topic) so the topic stays anchored.
            move = _MOVE_ROTATION[move_index % len(_MOVE_ROTATION)]
            move_index += 1
            sib_line = sibling_response(brain_turns[-1] if brain_turns else "",
                                        topic, move)
            sibling_lines.append(sib_line)
            prev_line = sib_line
            prompt_text = sib_line + " " + topic

        intent_list = _compose_intent(composer, prompt_text)
        if intent_list is None:
            # Composer + numpy both unavailable → can't even build a
            # zero vector. Skip the turn but keep the loop going.
            continue

        try:
            result = brain.produce_text(intent_list)
        except Exception as e:
            print(f"  [SibDlg:s{stage}] turn {turn} produce_text failed: {e}")
            # Don't raise — a flaky brain shouldn't kill the drip.
            continue

        produced = ""
        confidence = 0.0
        if isinstance(result, dict):
            produced = result.get("text", "") or ""
            confidence = result.get("confidence", 0.0) or 0.0

        brain_turns.append(produced)
        per_turn_scores.append(score_dialog_turn(prev_line, produced, topic))

    if not per_turn_scores:
        return None

    # Aggregate. Average composite across all completed brain turns.
    n = float(len(per_turn_scores))
    mean_continuity = sum(s["continuity"] for s in per_turn_scores) / n
    mean_topic = sum(s["topic_stability"] for s in per_turn_scores) / n
    mean_composite = sum(s["composite"] for s in per_turn_scores) / n
    mean_tokens = sum(s["token_count"] for s in per_turn_scores) / n

    joined = " ".join(t for t in brain_turns if (t or "").strip()).strip()

    # Feedback signal. Above threshold the conversation held together
    # well enough to consolidate; below threshold we re-anchor on the
    # topic so the target shape is reinforced rather than the drift.
    try:
        if mean_composite >= COHERENCE_PASS_THRESHOLD and joined:
            brain.learn_language(joined)
        else:
            try:
                brain.train_language(topic, topic)
            except TypeError:
                brain.train_language(text=topic, target_text=topic)
    except Exception as e:
        print(f"  [SibDlg:s{stage}] feedback signal failed: {e}")

    if int(rot) % max(1, int(log_every)) == 0:
        print(f"  [SibDlg:s{stage}] topic#{rot} turns={len(per_turn_scores)} "
              f"composite={mean_composite:.2f} cont={mean_continuity:.2f} "
              f"top={mean_topic:.2f} tok={mean_tokens:.1f}")

    return {
        "stage": int(stage),
        "rotation": int(rot),
        "topic": topic,
        "num_turns": len(per_turn_scores),
        "brain_turns": list(brain_turns),
        "sibling_turns": list(sibling_lines),
        "per_turn_scores": per_turn_scores,
        "continuity": mean_continuity,
        "topic_stability": mean_topic,
        "composite": mean_composite,
        "token_count": mean_tokens,
    }
