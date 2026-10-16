"""CE-10: Failure-mode replay + autobiographical journaling.

Pure-Python collector + replayer module. Other CE modules (CE-1
storytelling, CE-2 socratic_qa, CE-3 sibling_dialog, CE-8 tom_probes,
CE-9 counterfactual) score the brain's productions and route below-
threshold cases through a corrective `train_language(prompt, prompt)`
re-anchor. CE-10 is the layer that *remembers* those low-coherence
productions across drip calls and selectively *replays* them later --
equivalent to autobiographical "I tried this and it didn't work; let
me try again" memory.

This module is a collector + replayer, not another scoring driver.
Other CE modules hand failures here via `record_failure(...)`. The
drip function pulls a few of them back and re-targets the brain on
them with corrected exemplars.

Storage policy: in-memory only. The journal is a bounded deque and
deliberately is not persisted to disk -- matches the project's
storage policy that autobiographical memory lives in RAM.

Public API:
    record_failure(*, source, prompt, produced, expected_keywords,
                    composite, stage, metadata=None) -> int
    journal_size() -> int
    journal_clear() -> None
    journal_summary() -> dict
    run_replay_drip(brain, stage, *, composer=None,
                     max_replays=3, log_every=10) -> list[dict] | None
    REPLAY_PASS_THRESHOLD

This is intentionally NOT Claude-as-teacher (CE-19). Pure Python +
deterministic.

Stage 1 is a no-op (vocabulary still bootstrapping; no point
replaying failures yet).
"""
from __future__ import annotations

import re
import time
from collections import deque
from typing import Iterable

# ---------------------------------------------------------------------------
# Module-level state
# ---------------------------------------------------------------------------

# Bounded ring of failure entries. 512 is generous: at the typical 200-
# stimulus drip cadence and ~6 CE drip sources, even pessimistic failure
# rates stay well within bound for many minutes of training.
_JOURNAL: deque = deque(maxlen=512)

# Prompt -> last-seen monotonic timestamp. Used for the dedup window so
# that a stuck brain producing the same failure on every drip doesn't
# flood the journal.
_RECENT_PROMPTS: dict[str, float] = {}

# Monotonic id assigned to each accepted entry. Starts at 1; survives
# eviction (we never re-use ids).
_NEXT_ID: int = 1

_DEDUP_WINDOW_SECONDS = 30.0

REPLAY_PASS_THRESHOLD = 0.55


# ---------------------------------------------------------------------------
# Tokenizer / scoring helpers (kept self-contained to avoid cross-coupling
# with sibling CE modules; the shape mirrors counterfactual / storytelling).
# ---------------------------------------------------------------------------

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
    if not text:
        return []
    return [m.group(0) for m in re.finditer(r"[A-Za-z']+", text.lower())]


def _content_tokens(tokens: Iterable[str]) -> list[str]:
    return [t for t in tokens if t not in _STOPWORDS and len(t) > 2]


def _score_replay(attempt: str, expected_keywords: Iterable[str] | None) -> dict:
    """Score a replay attempt with the same composite shape used by the
    sibling CE modules: composite = (2*recall + specificity) / 3.0.

    When `expected_keywords` is empty/None we have no recall target, so
    composite reflects specificity alone -- a partial signal but
    deterministic."""
    expected_set = {w.lower() for w in (expected_keywords or []) if w}
    tokens = _tokenize(attempt or "")
    token_set = set(tokens)

    if expected_set:
        recall = len(expected_set & token_set) / float(len(expected_set))
    else:
        recall = 0.0

    distinct_content = len(set(_content_tokens(tokens)))
    specificity = min(1.0, distinct_content / 5.0)

    if expected_set:
        composite = (2.0 * recall + specificity) / 3.0
    else:
        # No keyword target: composite is just specificity, scaled into
        # the same [0, 1] range. We still return the recall slot for
        # consistency.
        composite = specificity

    return {
        "recall": recall,
        "specificity": specificity,
        "composite": composite,
        "tokens": len(tokens),
    }


# ---------------------------------------------------------------------------
# Journal API
# ---------------------------------------------------------------------------

def _prune_recent_prompts(now: float) -> None:
    """Drop stale entries from the dedup map. Called from `record_failure`
    so the map doesn't grow unbounded over a long process lifetime."""
    stale = [p for p, ts in _RECENT_PROMPTS.items()
             if now - ts > _DEDUP_WINDOW_SECONDS]
    for p in stale:
        _RECENT_PROMPTS.pop(p, None)


def record_failure(*, source: str, prompt: str, produced: str,
                   expected_keywords: set[str] | None,
                   composite: float,
                   stage: int,
                   metadata: dict | None = None) -> int:
    """Append a failure entry to the in-memory journal.

    Returns the assigned entry id, or -1 if the entry was deduplicated
    (same `prompt` was recorded within the last 30 seconds).

    `source` should be the originating CE module: "storytelling" |
    "socratic_qa" | "sibling_dialog" | "tom_probes" | "counterfactual"
    | other free-form string for future CE modules.
    """
    global _NEXT_ID

    now = time.monotonic()
    _prune_recent_prompts(now)

    if prompt:
        last_seen = _RECENT_PROMPTS.get(prompt)
        if last_seen is not None and (now - last_seen) <= _DEDUP_WINDOW_SECONDS:
            # Refresh the dedup timestamp so the suppression window is
            # rolling rather than fixed -- a stuck brain hitting the same
            # prompt every cycle should keep getting dedup'd, not slip
            # through after exactly 30s of repeated misses.
            _RECENT_PROMPTS[prompt] = now
            return -1
        _RECENT_PROMPTS[prompt] = now

    entry_id = _NEXT_ID
    _NEXT_ID += 1

    entry = {
        "id": entry_id,
        "source": str(source),
        "prompt": str(prompt),
        "produced": str(produced or ""),
        "expected_keywords": sorted(expected_keywords) if expected_keywords else [],
        "composite": float(composite),
        "stage": int(stage),
        "ts": now,
    }
    if metadata is not None:
        entry["metadata"] = dict(metadata)
    _JOURNAL.append(entry)
    return entry_id


def journal_size() -> int:
    return len(_JOURNAL)


def journal_clear() -> None:
    """Empty the journal and the dedup window. Id counter is NOT reset
    -- ids remain monotonic across clears within the same process."""
    _JOURNAL.clear()
    _RECENT_PROMPTS.clear()


def journal_summary() -> dict:
    """Read-only aggregator. Does NOT mutate the journal.

    Returns:
        {
          "total": int,
          "by_source": {source_name: count, ...},
          "by_stage":  {1: count, 2: count, 3: count},
          "mean_composite": float,
        }
    """
    total = len(_JOURNAL)
    by_source: dict[str, int] = {}
    by_stage: dict[int, int] = {1: 0, 2: 0, 3: 0}
    composite_sum = 0.0
    for entry in _JOURNAL:
        src = entry.get("source", "")
        by_source[src] = by_source.get(src, 0) + 1
        st = int(entry.get("stage", 0))
        by_stage[st] = by_stage.get(st, 0) + 1
        composite_sum += float(entry.get("composite", 0.0))
    mean_composite = (composite_sum / total) if total > 0 else 0.0
    return {
        "total": total,
        "by_source": by_source,
        "by_stage": by_stage,
        "mean_composite": mean_composite,
    }


# ---------------------------------------------------------------------------
# Replay driver
# ---------------------------------------------------------------------------

def _compose_intent(composer, prompt: str):
    """Compose prompt -> intent vector. Falls back to zeros on failure."""
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


def _replay_weight(entry: dict, now: float) -> float:
    """Combined weight = (1 - composite) + recency_factor.

    - lower composite -> higher need to replay (1 - composite)
    - more recent     -> higher recency_factor; recency falls linearly
                          to 0 after 600s.
    """
    composite = float(entry.get("composite", 0.0))
    ts = float(entry.get("ts", now))
    age = max(0.0, now - ts)
    recency_factor = max(0.0, 1.0 - (age / 600.0))
    return (1.0 - composite) + recency_factor


def _select_replay_candidates(stage: int, max_replays: int,
                              now: float) -> list[dict]:
    """Filter journal to entries matching `stage`, then return up to
    `max_replays` of them ordered by descending weight."""
    candidates = [e for e in _JOURNAL if int(e.get("stage", -1)) == int(stage)]
    if not candidates:
        return []
    # Deterministic sort: primary key is weight (descending). Use entry
    # id as a stable tiebreaker so equally-weighted entries pick in a
    # repeatable order.
    candidates.sort(
        key=lambda e: (-_replay_weight(e, now), int(e.get("id", 0))),
    )
    return candidates[:max(1, int(max_replays))]


def _build_exemplar(entry: dict) -> str:
    """Corrective exemplar string. Prefers expected_keywords; otherwise
    falls back to the original `produced` (anchor on what we have)."""
    prompt = entry.get("prompt", "") or ""
    expected = entry.get("expected_keywords", []) or []
    if expected:
        return prompt + " " + " ".join(expected)
    produced = entry.get("produced", "") or ""
    return prompt + " " + produced


def run_replay_drip(brain, stage: int, *,
                    composer=None,
                    max_replays: int = 3,
                    log_every: int = 10) -> list[dict] | None:
    """One replay drip pass. Stage 1 is a no-op.

    Pulls up to `max_replays` past failures from the journal that match
    `stage`, weighted by recency + inverse composite. For each one:
    composes a corrective exemplar, asks the brain to produce, scores
    the new attempt, then either:
      - new composite >= REPLAY_PASS_THRESHOLD (0.55):
            brain.learn_language(attempt) AND drop the entry from the
            journal -- the brain produces this correctly now, so we no
            longer need to replay it.
      - new composite <  REPLAY_PASS_THRESHOLD:
            brain.train_language(exemplar, exemplar) AND keep the entry
            -- still worth replaying later.

    Brain-call failures are caught + logged; they neither promote nor
    demote the journal entry.

    Returns a list of per-replay dicts, or None when stage==1 or there
    are no journal entries matching `stage`.
    """
    if int(stage) <= 1:
        return None

    now = time.monotonic()
    candidates = _select_replay_candidates(stage, max_replays, now)
    if not candidates:
        return None

    results: list[dict] = []
    drop_ids: list[int] = []

    for i, entry in enumerate(candidates):
        entry_id = int(entry.get("id", -1))
        source = str(entry.get("source", ""))
        original_composite = float(entry.get("composite", 0.0))
        expected = entry.get("expected_keywords", []) or []

        exemplar = _build_exemplar(entry)
        intent_list = _compose_intent(composer, exemplar)
        if intent_list is None:
            # Couldn't even build an intent -- skip without journal
            # changes. Don't raise.
            continue

        attempt = ""
        confidence = 0.0
        try:
            result = brain.produce_text(intent_list)
            if isinstance(result, dict):
                attempt = result.get("text", "") or ""
                confidence = float(result.get("confidence", 0.0) or 0.0)
        except Exception as e:
            print(f"  [Replay:s{stage}] produce_text failed (id={entry_id}): {e}")
            # Still record a result row so the caller can see we tried.
            results.append({
                "entry_id": entry_id,
                "source": source,
                "original_composite": original_composite,
                "replay_attempt": "",
                "new_composite": 0.0,
                "retained": True,
            })
            continue

        score = _score_replay(attempt, expected)
        new_composite = float(score["composite"])

        retained = True
        try:
            if new_composite >= REPLAY_PASS_THRESHOLD and attempt.strip():
                brain.learn_language(attempt)
                drop_ids.append(entry_id)
                retained = False
            else:
                try:
                    brain.train_language(exemplar, exemplar)
                except TypeError:
                    brain.train_language(text=exemplar, target_text=exemplar)
        except Exception as e:
            print(f"  [Replay:s{stage}] feedback failed (id={entry_id}): {e}")
            # Failure path doesn't change journal state -- keep the entry.
            retained = True

        results.append({
            "entry_id": entry_id,
            "source": source,
            "original_composite": original_composite,
            "replay_attempt": attempt,
            "new_composite": new_composite,
            "retained": retained,
            "confidence": confidence,
        })

        if int(i) % max(1, int(log_every)) == 0:
            print(f"  [Replay:s{stage}] id={entry_id} src={source} "
                  f"orig={original_composite:.2f} new={new_composite:.2f} "
                  f"retain={retained} tok={score['tokens']}")

    # Drop promoted entries. We can't drop in-place during iteration
    # because deque doesn't support arbitrary deletion cheaply; rebuild
    # only when necessary.
    if drop_ids:
        drop_set = set(drop_ids)
        survivors = [e for e in _JOURNAL if int(e.get("id", -1)) not in drop_set]
        _JOURNAL.clear()
        _JOURNAL.extend(survivors)

    return results
