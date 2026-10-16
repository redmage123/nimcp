"""CE-2: Socratic Q&A on ingested texts.

Pure-Python curriculum module. The training loop calls
`run_socratic_drip` on the same cadence as the canonical / math /
streaming drips after a chunk of corpus text has been ingested. For
each call we:

  1. Generate up to N deterministic factoid questions FROM the chunk
     using simple regex/frequency heuristics (no LLM).
  2. Compose each question into the brain's input space and invoke
     brain.produce_text(intent) to get the brain's reply.
  3. Score the reply for fact-recall via three lightweight signals:
        - keyword_recall : fraction of the question's expected
                           keywords present in the answer
        - specificity    : penalty against one-token / collapsed
                           replies (distinct content words / 5)
        - composite      : (2*recall + specificity) / 3 — recall is
                           weighted higher because this is fact-recall,
                           not creativity
  4. Feed the score back as a learning signal:
        - composite >= 0.5 → brain.learn_language(answer)         (positive)
        - composite <  0.5 → brain.train_language(qk, qk)          (corrective:
                            re-anchor on question + correct keywords)

Like CE-1 storytelling, this is intentionally NOT Claude-as-teacher —
that's CE-19. Here we only need a deterministic, additional
self-supervised signal that tests whether the freshly ingested chunk
left a recoverable factoid trace.

Public API:
    generate_questions(chunk_text, *, max_questions=3) -> list[dict]
    score_answer(answer_text, expected_keywords) -> dict
    run_socratic_drip(brain, stage, *, chunk_text, composer=None,
                      max_questions=3, log_every=10) -> list[dict] | None
    COMPREHENSION_PASS_THRESHOLD = 0.5

Stage 1 is a no-op (too early; lexicon is still bootstrapping).
"""
from __future__ import annotations

import re
from typing import Iterable

# ---------------------------------------------------------------------------
# Stop-words. Same flavor as storytelling._STOPWORDS — small, deliberate;
# false-negatives only make scoring conservative.
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
    "near", "across", "through", "over", "under", "about",
    "what", "who", "where", "when", "why", "how",
    "tell", "about",
})


# A small location/preposition set used by the where-question heuristic.
_WHERE_PREPS = frozenset({"in", "at", "on", "near", "across", "through"})


# Min chunk length to bother with — below this we skip cleanly.
_MIN_CHUNK_LEN = 25

# Min keyword length: only words >= 4 chars qualify as content keywords.
_MIN_KEYWORD_LEN = 4


# ---------------------------------------------------------------------------
# Tokenization helpers
# ---------------------------------------------------------------------------

def _tokenize(text: str) -> list[str]:
    """Lowercase + word-only tokenization. Cheap; deterministic."""
    if not text:
        return []
    return [m.group(0) for m in re.finditer(r"[A-Za-z']+", text.lower())]


def _content_tokens(text: str) -> list[str]:
    """Tokens that are content-bearing: non-stopword and >= MIN_KEYWORD_LEN."""
    return [t for t in _tokenize(text)
            if t not in _STOPWORDS and len(t) >= _MIN_KEYWORD_LEN]


def _content_word_set(text: str) -> set[str]:
    return set(_content_tokens(text))


def _top_content_words(text: str, n: int) -> list[str]:
    """Top-n content words by frequency, deterministic ordering ties broken
    by first-occurrence."""
    tokens = _content_tokens(text)
    if not tokens:
        return []
    freq: dict[str, int] = {}
    first_seen: dict[str, int] = {}
    for i, t in enumerate(tokens):
        freq[t] = freq.get(t, 0) + 1
        if t not in first_seen:
            first_seen[t] = i
    # Sort: highest freq first; ties by earliest occurrence.
    ordered = sorted(freq.keys(),
                     key=lambda w: (-freq[w], first_seen[w]))
    return ordered[:n]


# ---------------------------------------------------------------------------
# Question generators
# ---------------------------------------------------------------------------

def _qkw_valid(kws: Iterable[str]) -> set[str]:
    """Keep only keywords that are non-stopword and >= MIN_KEYWORD_LEN."""
    return {k.lower() for k in kws
            if k and k.lower() not in _STOPWORDS
            and len(k) >= _MIN_KEYWORD_LEN}


def _gen_what_is(chunk: str) -> dict | None:
    """'What is X?' on the most-frequent content word."""
    top = _top_content_words(chunk, 1)
    if not top:
        return None
    word = top[0]
    kws = _qkw_valid({word})
    if not kws:
        return None
    return {
        "question": f"What is {word}?",
        "expected_keywords": kws,
        "kind": "what_is",
    }


# Pre-compiled subject-verb regex. We require:
#   - a capitalized word (the subject) of length >= 2
#   - one or two whitespace
#   - a lowercase verb-shaped word ending in 's'/'ed'/'ing' or one of a tiny
#     set of bare verbs we sometimes see (ran, sat, gave, had, went, came...)
# Keep it cheap; this is heuristic, not parsing.
_SV_RE = re.compile(
    r"\b([A-Z][a-zA-Z]{2,})\s+"
    r"([a-z]+(?:s|ed|ing)|ran|sat|gave|had|went|came|made|saw|took|told|said|grew|fell|rose|lay)\b"
)


def _gen_who_did(chunk: str) -> dict | None:
    """'Who/What [verb]?' — fires on a capitalized subject + lowercase verb."""
    m = _SV_RE.search(chunk)
    if not m:
        return None
    subject = m.group(1)
    verb = m.group(2)
    kws = _qkw_valid({subject})
    if not kws:
        return None
    # 'Who' if the subject looks like a person-ish proper noun (first letter
    # uppercase, rest lowercase); 'What' otherwise. Cheap heuristic.
    pronoun = "Who" if subject[1:].islower() else "What"
    return {
        "question": f"{pronoun} {verb}?",
        "expected_keywords": kws,
        "kind": "who_did",
    }


# Preposition + capitalized-noun pattern for the where-question.
_WHERE_RE = re.compile(
    r"\b(" + "|".join(_WHERE_PREPS) + r")\s+(?:the\s+)?([A-Z][a-zA-Z]{3,})\b"
)


def _gen_where(chunk: str) -> dict | None:
    """'Where did this happen?' — fires when a preposition is followed by a
    capitalized location-shaped word."""
    m = _WHERE_RE.search(chunk)
    if not m:
        return None
    place = m.group(2)
    kws = _qkw_valid({place})
    if not kws:
        return None
    return {
        "question": "Where did this happen?",
        "expected_keywords": kws,
        "kind": "where",
    }


def _gen_summary(chunk: str) -> dict | None:
    """'Tell me about this passage.' — always-valid fallback. Expected
    keywords = top 5 content words."""
    top = _top_content_words(chunk, 5)
    kws = _qkw_valid(top)
    if not kws:
        return None
    return {
        "question": "Tell me about this passage.",
        "expected_keywords": kws,
        "kind": "summary",
    }


def generate_questions(chunk_text: str, *, max_questions: int = 3) -> list[dict]:
    """Generate up to `max_questions` deterministic factoid questions FROM
    the chunk.

    Returns a list of dicts:
        {"question": str, "expected_keywords": set[str], "kind": str}

    Returns [] for too-short chunks (< MIN_CHUNK_LEN chars) or chunks
    with no content words.

    Question kinds, in priority order:
        what_is, who_did, where, summary
    The summary fallback always tries last so the result is non-empty
    whenever the chunk has any content words.
    """
    if not chunk_text or len(chunk_text.strip()) < _MIN_CHUNK_LEN:
        return []
    if max_questions is None or max_questions <= 0:
        return []

    out: list[dict] = []
    seen_questions: set[str] = set()

    # Order matters — what_is is the cheapest specific probe, then SVO,
    # then location, then the summary fallback.
    for gen in (_gen_what_is, _gen_who_did, _gen_where, _gen_summary):
        if len(out) >= max_questions:
            break
        try:
            q = gen(chunk_text)
        except Exception:
            q = None
        if not q:
            continue
        # De-dup on the question text — guards against the (rare) case where
        # two heuristics happen to produce identical text.
        if q["question"] in seen_questions:
            continue
        seen_questions.add(q["question"])
        out.append(q)

    return out[:max_questions]


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

def score_answer(answer_text: str, expected_keywords) -> dict:
    """Score an answer against the expected keywords.

    Returns a dict with:
        keyword_recall : 0..1 — fraction of expected keywords present
        specificity    : 0..1 — distinct-content-word count / 5, capped
        composite      : 0..1 — (2*recall + specificity) / 3
        tokens         : int — total tokens in the answer

    Empty answer → all 0.0 (composite included). If expected_keywords
    is empty/None, recall is 0.0 (we cannot credit unknown targets)."""
    if expected_keywords is None:
        expected = set()
    else:
        # Normalize to a lowercase set; only count content-shaped keywords.
        expected = {str(k).lower() for k in expected_keywords
                    if k and len(str(k)) >= _MIN_KEYWORD_LEN}

    answer_tokens = _tokenize(answer_text or "")
    answer_set = set(answer_tokens)
    n_tokens = len(answer_tokens)

    if not answer_text or not answer_text.strip() or n_tokens == 0:
        return {
            "keyword_recall": 0.0,
            "specificity": 0.0,
            "composite": 0.0,
            "tokens": 0,
        }

    if expected:
        hits = sum(1 for kw in expected if kw in answer_set)
        recall = hits / float(len(expected))
    else:
        recall = 0.0

    distinct_content = len({t for t in answer_tokens
                            if t not in _STOPWORDS and len(t) >= _MIN_KEYWORD_LEN})
    specificity = min(1.0, distinct_content / 5.0)

    composite = (2.0 * recall + specificity) / 3.0

    return {
        "keyword_recall": recall,
        "specificity": specificity,
        "composite": composite,
        "tokens": n_tokens,
    }


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

# Module-level call counter so log lines fire deterministically every
# `log_every` calls. Like storytelling's _ROTATION_COUNTER, this is NOT
# checkpointed — re-zeroing on resume is fine.
_DRIP_COUNTER: int = 0


COMPREHENSION_PASS_THRESHOLD = 0.5


def _intent_from_text(composer, text: str):
    """Convert text → intent vector via composer; fall back to zeros(1024).
    Returns a Python list (BrainProxy + in-process bindings both accept lists
    reliably)."""
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


def run_socratic_drip(brain, stage: int, *,
                      chunk_text: str,
                      composer=None,
                      max_questions: int = 3,
                      log_every: int = 10,
                      ) -> list[dict] | None:
    """One Socratic Q&A pass on `chunk_text`.

    Stage 1 is a no-op (returns None). Stage 2/3 generates up to
    `max_questions` factoid questions from the chunk, asks the brain
    for each, scores each answer, and feeds back the score:
        composite >= 0.5 → brain.learn_language(answer)
        composite <  0.5 → brain.train_language(q + " " + kws,
                                                q + " " + kws)
    Brain failures are caught + logged + skipped — never raise.

    Returns a list of per-question result dicts:
        {question, answer, expected_keywords (sorted list),
         composite, keyword_recall, specificity, tokens, kind}
    or [] if no questions could be generated for the chunk.
    Returns None for stage 1.
    """
    global _DRIP_COUNTER

    if int(stage) <= 1:
        return None

    if not chunk_text or len(chunk_text.strip()) < _MIN_CHUNK_LEN:
        return []

    questions = generate_questions(chunk_text, max_questions=max_questions)
    if not questions:
        return []

    results: list[dict] = []

    for q in questions:
        _DRIP_COUNTER += 1

        question_text = q["question"]
        expected = q["expected_keywords"]
        kind = q["kind"]

        intent_list = _intent_from_text(composer, question_text)
        if intent_list is None:
            # Numpy is missing AND composer didn't give us a vector — skip
            # this question but keep iterating; other questions may still
            # work if a composer is supplied later.
            continue

        try:
            result = brain.produce_text(intent_list)
        except Exception as e:
            print(f"  [Socratic:s{stage}] produce_text failed on "
                  f"'{question_text}': {e}")
            continue

        answer = ""
        if isinstance(result, dict):
            answer = result.get("text", "") or ""

        score = score_answer(answer, expected)

        # Feedback. Above threshold → positive consolidation on the brain's
        # own answer. Below threshold → re-anchor on the question + the
        # correct keywords so the brain learns the target shape rather than
        # its own collapsed reply.
        try:
            if score["composite"] >= COMPREHENSION_PASS_THRESHOLD and answer.strip():
                brain.learn_language(answer)
            else:
                anchor = question_text + " " + " ".join(sorted(expected))
                try:
                    brain.train_language(anchor, anchor)
                except TypeError:
                    brain.train_language(text=anchor, target_text=anchor)
        except Exception as e:
            # Brain may not have learn/train_language exposed in some test
            # fixtures; the score itself is still useful as a metric.
            print(f"  [Socratic:s{stage}] feedback signal failed: {e}")

        if _DRIP_COUNTER % max(1, int(log_every)) == 0:
            print(f"  [Socratic:s{stage}] q#{_DRIP_COUNTER} kind={kind} "
                  f"composite={score['composite']:.2f} "
                  f"recall={score['keyword_recall']:.2f} "
                  f"spec={score['specificity']:.2f} "
                  f"tok={score['tokens']}")

        results.append({
            "question": question_text,
            "answer": answer,
            "expected_keywords": sorted(expected),
            "kind": kind,
            "composite": score["composite"],
            "keyword_recall": score["keyword_recall"],
            "specificity": score["specificity"],
            "tokens": score["tokens"],
            "stage": int(stage),
        })

    return results
