"""CE-1: Storytelling production + coherence scoring.

Pure-Python curriculum module. The training loop calls `run_storytelling_drip`
at the same 200-stimulus cadence as the canonical / math / streaming drips.
For each call we:

  1. Pick a stage-appropriate story seed (deterministic but rotated).
  2. Compose the seed into the brain's input space and invoke
     brain.produce_text(intent) to get the brain's narrative reply.
  3. Score the reply for coherence via three lightweight signals:
        - lexical diversity      (distinct tokens / total tokens)
        - repetition penalty     (1 - max-trigram-frequency / total trigrams)
        - on-topic ratio         (overlap with seed's content words)
     The three are averaged into [0, 1].
  4. Feed the score back as a learning signal:
        - score >= 0.6   → brain.learn_language(produced_text)   (positive)
        - score <  0.6   → brain.train_language(seed, seed)       (corrective:
                            re-anchor to the seed so the brain learns the
                            target shape rather than its own collapse)

This is intentionally NOT Claude-as-teacher — that's CE-19. Here we only
need a deterministic, additional self-supervised signal that surfaces
mode-collapse / repetition / off-topic drift as a numeric reward.

Public API:
    run_storytelling_drip(brain, stage, *, composer, log_every=10)
    score_coherence(seed, produced) -> dict with diversity/repetition/on_topic/composite
    pick_seed(stage, rotation_index) -> (seed_text, content_words)

Stage 1 is a no-op (too early; vocabulary is still bootstrapping).
"""
from __future__ import annotations

import re
from typing import Iterable

# ---------------------------------------------------------------------------
# Stage-appropriate story seeds. Curated short, concrete, action-bearing
# prompts. Seeds for stage 2 stay in the everyday / sensory register; stage 3
# admits more abstract / multi-clause shapes.
# ---------------------------------------------------------------------------

_STAGE_SEEDS: dict[int, list[str]] = {
    1: [],  # no-op — vocabulary not ready
    2: [
        "The cat sat on the mat and watched the bird outside the window.",
        "The boy gave the apple to his sister and they walked to the river.",
        "The dog ran across the field chasing the red ball into the long grass.",
        "The mother rocked the baby to sleep while the rain fell on the roof.",
        "The old man planted a tree near the road and watered it every morning.",
        "The girl found a small stone shaped like a heart and put it in her pocket.",
        "The wind blew the leaves into the air and they fell down onto the path.",
        "The child held the lantern up high and looked into the dark forest.",
    ],
    3: [
        "The wanderer stood at the crossroads and chose the path that climbed into the hills.",
        "She remembered the promise she had made and turned the ship back toward the harbor.",
        "When the door finally opened, the room beyond was not what any of them had imagined.",
        "The young scholar began to suspect that the manuscript was not what its title claimed.",
        "Across the long valley a single fire burned, and they walked toward it without speaking.",
        "He wrote the letter at midnight, sealed it carefully, and put it in the drawer with the others.",
        "The old woman taught the boy how to listen for the river underneath the snow.",
        "Years later, when she returned to the city, the towers were taller but the song was the same.",
    ],
}


# A small stop-list so on-topic scoring focuses on content words. Kept short
# deliberately — false-negatives (rare content words filtered out) just make
# the score conservative, which is fine.
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


def _content_words(text: str) -> set[str]:
    return {t for t in _tokenize(text) if t not in _STOPWORDS and len(t) > 2}


def pick_seed(stage: int, rotation_index: int) -> tuple[str, set[str]]:
    """Deterministically pick a seed for `stage` cycling through `rotation_index`.

    Returns (seed_text, content_words). Empty seed for stages without seeds
    (currently stage 1)."""
    seeds = _STAGE_SEEDS.get(int(stage), [])
    if not seeds:
        return "", set()
    seed = seeds[int(rotation_index) % len(seeds)]
    return seed, _content_words(seed)


# ---------------------------------------------------------------------------
# Coherence scoring
# ---------------------------------------------------------------------------

def _diversity(tokens: list[str]) -> float:
    """Type-token ratio. Empty → 0.0; single-token → 1.0 (charitable for a
    minimum-length response)."""
    if not tokens:
        return 0.0
    return len(set(tokens)) / float(len(tokens))


def _repetition_penalty(tokens: list[str]) -> float:
    """min(distinct_trigram_ratio, 1 - max_trigram_freq).

    Catches two failure modes:
      - single-trigram domination (one phrase dwarfing all others): caught
        by 1 - max_count/total
      - cyclic repetition (a few trigrams repeated many times): caught by
        distinct/total — under cyclic collapse, distinct stays small
        relative to total even when no single trigram dominates.

    Below 3 tokens we conservatively return 1.0 (no trigrams to score)."""
    if len(tokens) < 3:
        return 1.0
    trigrams: dict[tuple[str, str, str], int] = {}
    for i in range(len(tokens) - 2):
        tri = (tokens[i], tokens[i + 1], tokens[i + 2])
        trigrams[tri] = trigrams.get(tri, 0) + 1
    total = sum(trigrams.values())
    if total <= 0:
        return 1.0
    distinct = len(trigrams)
    max_count = max(trigrams.values())
    distinct_ratio = distinct / float(total)
    domination_score = 1.0 - (max_count / float(total))
    return min(distinct_ratio, domination_score)


def _on_topic(seed_content: set[str], produced_tokens: list[str]) -> float:
    """Fraction of seed content words present somewhere in the produced text.

    If the seed has no content words, return 0.0 (we can't score it; treat
    as not-on-topic rather than falsely full-credit)."""
    if not seed_content:
        return 0.0
    produced_set = set(produced_tokens)
    hits = sum(1 for w in seed_content if w in produced_set)
    return hits / float(len(seed_content))


def score_coherence(seed: str, produced: str) -> dict:
    """Return a dict with diversity, repetition, on_topic, composite."""
    seed_content = _content_words(seed)
    produced_tokens = _tokenize(produced)
    diversity = _diversity(produced_tokens)
    repetition = _repetition_penalty(produced_tokens)
    on_topic = _on_topic(seed_content, produced_tokens)
    # Even weights — no signal is more privileged than the others. Mode
    # collapse drops repetition, vocab fragmentation drops diversity, and
    # off-topic drift drops on_topic.
    composite = (diversity + repetition + on_topic) / 3.0
    return {
        "diversity": diversity,
        "repetition": repetition,
        "on_topic": on_topic,
        "composite": composite,
        "tokens": len(produced_tokens),
    }


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

# Module-level rotation counter so seeds rotate even when callers don't pass
# their own. We persist this across drip calls within a single process; it is
# NOT a checkpointed value (re-rotating from 0 on resume is acceptable —
# storytelling is a soft signal).
_ROTATION_COUNTER: dict[int, int] = {1: 0, 2: 0, 3: 0}


def _bump_rotation(stage: int) -> int:
    cur = _ROTATION_COUNTER.get(int(stage), 0)
    _ROTATION_COUNTER[int(stage)] = cur + 1
    return cur


COHERENCE_PASS_THRESHOLD = 0.6


def run_storytelling_drip(brain, stage: int, *,
                          composer=None,
                          log_every: int = 10,
                          rotation_index: int | None = None,
                          ) -> dict | None:
    """One storytelling pass. Returns the score dict + metadata, or None
    when the stage has no seeds or production fails.

    `composer` is the same Composer used elsewhere in the training loop —
    we use it to turn the seed text into a 1024-dim intent vector for
    `brain.produce_text(intent)`. If composer is None we fall back to a
    zero vector (the brain still produces something — just less guided).

    `rotation_index` overrides the module rotation counter (useful for
    tests + deterministic replays); when None the counter advances.
    """
    seeds = _STAGE_SEEDS.get(int(stage), [])
    if not seeds:
        return None

    rot = rotation_index if rotation_index is not None else _bump_rotation(stage)
    seed, _ = pick_seed(stage, rot)
    if not seed:
        return None

    # Build the intent vector. The Composer is the one already in use in
    # immerse_athena — its compose(text=..., modality="text") returns a
    # numpy float32 array of length BRAIN_INPUT_DIM. We pass that as
    # the intent to produce_text.
    intent = None
    if composer is not None:
        try:
            intent = composer.compose(text=seed, modality="text")
        except Exception:
            intent = None

    if intent is None:
        # Fall back to a small zero-padded request — avoids a hard skip.
        try:
            import numpy as _np
            intent = _np.zeros(1024, dtype=_np.float32)
        except Exception:
            return None

    # Convert to plain Python list for the daemon RPC layer (BrainProxy
    # already accepts numpy via _to_list, but in-process Brain bindings
    # accept lists more reliably).
    try:
        intent_list = intent.tolist() if hasattr(intent, "tolist") else list(intent)
    except Exception:
        return None

    try:
        result = brain.produce_text(intent_list)
    except Exception as e:
        print(f"  [Story:s{stage}] produce_text failed: {e}")
        return None

    produced = (result or {}).get("text", "") if isinstance(result, dict) else ""
    confidence = (result or {}).get("confidence", 0.0) if isinstance(result, dict) else 0.0

    score = score_coherence(seed, produced)

    # Feedback signal. Above threshold we let the brain learn its own
    # production (positive consolidation). Below threshold we re-train on
    # the seed itself so the target shape is reinforced rather than
    # the collapsed reply.
    try:
        if score["composite"] >= COHERENCE_PASS_THRESHOLD and produced.strip():
            brain.learn_language(produced)
        else:
            try:
                brain.train_language(seed, seed)
            except TypeError:
                brain.train_language(text=seed, target_text=seed)
    except Exception as e:
        # Brain may not have train_language exposed in some test fixtures;
        # the score itself is still useful as a metric.
        print(f"  [Story:s{stage}] feedback signal failed: {e}")

    if int(rot) % max(1, int(log_every)) == 0:
        print(f"  [Story:s{stage}] seed#{rot} composite={score['composite']:.2f} "
              f"div={score['diversity']:.2f} rep={score['repetition']:.2f} "
              f"top={score['on_topic']:.2f} tok={score['tokens']} "
              f"conf={confidence:.2f}")

    return {
        "stage": int(stage),
        "rotation": int(rot),
        "seed": seed,
        "produced": produced,
        "confidence": float(confidence),
        **score,
    }
