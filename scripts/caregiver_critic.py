"""Caregiver critic — stage-aware reward signal for brain language productions.

Part of the Option 1 architectural rebuild (Slice C). See
``docs/claude/option1-rebuild.md`` for the full design.

The brain produces language via the SNN↔language bridge. The bridge is a
transport (Slice A) and concept binding is in the registry (Slice B); the
brain's learning signal on its own productions has historically come from
``cascade_self_train`` auto-confirming whatever was emitted. That biases the
system toward mode collapse onto whatever word salad it stumbles into first.

This module replaces self-confirmation with a developmentally-anchored
critic. It evaluates a (prompt, response) pair against per-stage rules
(Brown's stages I-III) and returns a ``CriticVerdict`` containing:

* ``reward`` in [-1.0, +1.0]
* ``recast``  — the correct form if the response was wrong-but-correctable
* ``reason``  — a one-line human-readable diagnostic for audit logs

The caller (``immerse_athena.py``) is expected to:

1. Feed the verdict's reward to ``brain.apply_reward_learning(reward, ...)``
   (or the existing ``brain.bg_update_reward(reward)`` fallback — see the
   integration in immerse_athena for which RPC is wired at merge time).
2. If a recast is present, use the existing supervised-production path
   (``brain.echo_and_correct(prompt, recast_word, lr_scale=N)``) to actually
   teach the corrected form.

Stage rules (anchored to Brown's stages — see vocab JSON files for sources):

* **Stage 0** (12-18 mo, holophrastic): exactly ONE word from a 50-word
  caregiver-frequency vocab. Multi-word or out-of-vocab → negative reward.
* **Stage 1** (18-24 mo, two-word floor): TWO content words is the developmental
  MINIMUM, not a ceiling. A clean two-word utterance is judged by template
  (AGENT+ACTION, MODIFIER+NOUN, ACTION+PATIENT, POSSESSOR+POSSESSED); longer
  utterances or copula sentences ("the grass is green") are emerging
  multi-word speech and are judged by SVO grammaticality — rewarded when
  grammatical, punished + recast when inverted. Function words are allowed.
* **Stage 2** (24-36 mo, telegraphic): 3-5 content words in SVO order;
  function words optional. Inversion / interrogative-for-declarative
  triggers a recast.
* **Stage 3+** (36 mo+): rule-based grammaticality. If ``llm_callable`` is
  provided, delegate to LLM judge for nuanced grammaticality (fallback to
  rule-based when LLM unavailable).

The critic NEVER touches the brain directly — it returns a verdict and the
caller drives the learning. This keeps Slice C purely Python and avoids
contention with other slices' C-side work.
"""
from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable


DATA_DIR_DEFAULT = Path(__file__).resolve().parent.parent / "data"


@dataclass
class CriticVerdict:
    """Result of evaluating one (prompt, response) pair.

    Attributes:
        reward:  Scalar in [-1.0, +1.0]. Sign maps to dopamine direction;
                 magnitude maps to update strength.
        recast:  Corrected form when the response was wrong-but-fixable
                 (word-order error, slight out-of-stage word). ``None`` when
                 no correction makes sense (e.g. response was already
                 correct, or response was so far off there is no obvious
                 recast).
        reason:  Short human-readable diagnostic. Logged every N steps so
                 walkthroughs can audit the critic's decisions.
    """
    reward: float
    recast: str | None
    reason: str


# Module-level constants (tuned in line with the stage table in
# docs/claude/option1-rebuild.md Slice E).
STAGE_RULES: dict[int, dict] = {
    0: {"min_words": 1, "max_words": 1, "vocab_size_target": 50},
    # Stage 1: two content words is the FLOOR, not a ceiling. Longer,
    # well-formed utterances are developmental progress (max_words=None).
    1: {"min_words": 2, "max_words": None, "vocab_size_target": 200},
    2: {"min_words": 3, "max_words": 5, "vocab_size_target": 800},
}

# Copulas / linking verbs. Their presence signals a copula construction
# ("grass is green") rather than telegraphic two-word speech, so a stage-1
# response containing one is routed to the SVO grammaticality check.
_COPULAS = {"is", "are", "was", "were", "am", "be"}

# Tokens we strip when comparing words to the vocab. Production from the
# brain can contain trailing punctuation from generation; the vocab files
# are punctuation-free.
_PUNCT_STRIP = ".,!?;:\"'()-_"


def _normalize(text: str) -> list[str]:
    """Lowercase, strip punctuation, split on whitespace. Empty tokens removed."""
    if not text:
        return []
    cleaned = text.lower().strip()
    tokens = re.split(r"\s+", cleaned)
    out = []
    for t in tokens:
        t = t.strip(_PUNCT_STRIP)
        if t:
            out.append(t)
    return out


def _normalize_prompt(prompt: str) -> str:
    """Canonicalize a prompt for hint lookup — lowercased, whitespace-normalized."""
    if not prompt:
        return ""
    p = prompt.lower().strip()
    # Collapse internal whitespace; preserve trailing '?' since hints are
    # keyed on the question form.
    p = re.sub(r"\s+", " ", p)
    return p


def _looks_inverted_question(response_tokens: list[str]) -> bool:
    """Heuristic: response starts with copula/aux when a declarative was asked.

    Used by stage 2 to detect "is grass green" responses when the prompt
    asks for a declarative ("what color is grass?" → "grass is green").
    """
    if not response_tokens:
        return False
    return response_tokens[0] in {"is", "are", "was", "were", "am", "be",
                                  "do", "does", "did", "have", "has", "had",
                                  "can", "could", "will", "would", "should"}


class CaregiverCritic:
    """Stage-aware critic. One instance per stage; cache on the caller side."""

    def __init__(self,
                 stage: int = 0,
                 vocab_path: str | None = None,
                 llm_callable: Callable[[str], str] | None = None,
                 data_dir: str | Path | None = None):
        """
        Args:
            stage:  Developmental stage (0, 1, 2, 3+). Determines which rule
                    set + vocab is loaded.
            vocab_path:  Optional explicit path to stage vocab JSON. If
                    ``None``, the default ``data/stage_vocab_<stage>.json`` is
                    used.
            llm_callable:  Optional ``(prompt) -> str`` callable used only at
                    stage 3+ when finer grammaticality judging is needed.
                    Stage 0-2 NEVER call the LLM (deterministic rules only).
            data_dir:  Override for the data directory (testing).
        """
        self._data_dir = Path(data_dir) if data_dir else DATA_DIR_DEFAULT
        self._llm_callable = llm_callable
        self.stage = -1
        self._vocab: set[str] = set()
        self._function_words: set[str] = set()
        self._agents: set[str] = set()
        self._actions: set[str] = set()
        self._patients: set[str] = set()
        self._modifiers: set[str] = set()
        self._possessors: set[str] = set()
        self._possesseds: set[str] = set()
        self._subjects: set[str] = set()
        self._verbs: set[str] = set()
        self._objects: set[str] = set()
        self._semantic_hints: dict[str, str] = {}
        self._vocab_path_override = vocab_path
        self.set_stage(stage)

    # -- public API ----------------------------------------------------------

    def set_stage(self, stage: int) -> None:
        """Switch to the given stage. Reloads vocab from disk."""
        self.stage = int(stage)
        # Vocab path: explicit override (one-shot, only honored on first
        # construction) or per-stage default.
        if self._vocab_path_override and self._vocab_path_override:
            path = Path(self._vocab_path_override)
            # Subsequent set_stage calls fall back to per-stage default —
            # an explicit override only applies to the *initial* stage.
            self._vocab_path_override = None
        else:
            # Stages 3+ reuse stage 2 vocab as the floor.
            file_stage = stage if stage <= 2 else 2
            path = self._data_dir / f"stage_vocab_{file_stage}.json"
        self._load_vocab(path)

    def evaluate(self, prompt: str, response: str) -> CriticVerdict:
        """Score (prompt, response). Returns a CriticVerdict.

        Returns reward 0.0 with reason ``empty_response`` when the response
        has no extractable tokens — this is neither praised nor punished
        (no signal). Callers should treat 0.0 as "do not apply reward".
        """
        if response is None or not response.strip():
            return CriticVerdict(reward=0.0, recast=None, reason="empty_response")

        tokens = _normalize(response)
        if not tokens:
            return CriticVerdict(reward=0.0, recast=None, reason="empty_response")

        if self.stage == 0:
            return self._evaluate_stage_0(prompt, tokens)
        if self.stage == 1:
            return self._evaluate_stage_1(prompt, tokens)
        if self.stage == 2:
            return self._evaluate_stage_2(prompt, tokens)
        return self._evaluate_stage_3plus(prompt, response, tokens)

    # -- stage 0 -------------------------------------------------------------

    def _evaluate_stage_0(self, prompt: str, tokens: list[str]) -> CriticVerdict:
        """Stage 0: exactly ONE in-vocab word, ideally a hint-match."""
        hint = self._lookup_hint(prompt)

        # Out-of-vocab check is FIRST — even a single OOV word means the
        # brain is producing words it shouldn't know yet.
        oov = [t for t in tokens if t not in self._vocab]
        if oov:
            return CriticVerdict(
                reward=-1.0,
                recast=hint,
                reason=f"out_of_stage_vocab:{','.join(oov[:3])}",
            )

        # Multi-word at stage 0 = too advanced.
        if len(tokens) > 1:
            return CriticVerdict(
                reward=-0.5,
                recast=hint,
                reason=f"too_many_words:{len(tokens)}",
            )

        word = tokens[0]
        if hint is not None and word == hint:
            return CriticVerdict(
                reward=1.0,
                recast=None,
                reason=f"correct_answer:{word}",
            )
        # In-vocab single word but not the semantic match — still partial
        # credit (the brain produced a real word).
        return CriticVerdict(
            reward=0.3,
            recast=hint,
            reason=f"in_vocab_wrong_referent:{word}",
        )

    # -- stage 1 -------------------------------------------------------------

    def _evaluate_stage_1(self, prompt: str, tokens: list[str]) -> CriticVerdict:
        """Stage 1: TWO content words is the developmental FLOOR, not a ceiling.

        - Below two content words → too simple for stage 1 (mild negative).
        - Exactly two content words, no copula → telegraphic two-word speech,
          judged by the AGENT/ACTION/MODIFIER/POSSESSOR templates.
        - Three+ content words, OR a copula present → emerging multi-word
          speech (developmental PROGRESS). Judged by SVO grammaticality:
          grammatical → reward, inverted → punish + recast. NEVER punished
          purely for exceeding two words.

        Function words ("the", "is", ...) are allowed: they are stripped
        before the content-word count + OOV check, so "the grass is green"
        is judged on grammar rather than vocab restriction.
        """
        hint = self._lookup_hint(prompt)

        # Function words are always allowed — they don't count as OOV and
        # don't count toward the two-content-word floor.
        content = [t for t in tokens if t not in self._function_words]

        oov = [t for t in content if t not in self._vocab]
        if oov:
            return CriticVerdict(
                reward=-1.0,
                recast=hint,
                reason=f"out_of_stage_vocab:{','.join(oov[:3])}",
            )

        if len(content) < 2:
            return CriticVerdict(
                reward=-0.5,
                recast=hint,
                reason=f"below_two_word_floor:{len(content)}",
            )

        has_copula = any(t in _COPULAS for t in tokens)

        # Clean two-word telegraphic case (no copula) → template grammar.
        if len(content) == 2 and not has_copula:
            return self._evaluate_two_word_templates(content, hint)

        # Otherwise: emerging multi-word / copula speech → SVO grammaticality.
        return self._evaluate_stage_1_multiword(prompt, tokens, content, hint)

    def _evaluate_two_word_templates(self,
                                     content: list[str],
                                     hint: str | None) -> CriticVerdict:
        """Telegraphic two-word grammar (the stage-1 canonical case)."""
        w1, w2 = content
        # Templates the critic accepts:
        #   AGENT + ACTION        ("dog run")
        #   MODIFIER + NOUN       ("big dog")
        #   ACTION + PATIENT      ("eat food")
        #   POSSESSOR + POSSESSED ("mama hat")
        agent_action = w1 in self._agents and w2 in self._actions
        modifier_noun = w1 in self._modifiers and (
            w2 in self._agents or w2 in self._patients)
        action_patient = w1 in self._actions and w2 in self._patients
        possessor_possessed = (w1 in self._possessors and
                               w2 in self._possesseds)

        if agent_action or modifier_noun or action_patient or possessor_possessed:
            return CriticVerdict(
                reward=1.0,
                recast=None,
                reason="valid_template",
            )

        # Detect inversion errors: if reversed pair would match a template,
        # the response is well-typed-but-misordered. Emit a recast.
        rev_agent_action = w2 in self._agents and w1 in self._actions
        rev_modifier_noun = w2 in self._modifiers and (
            w1 in self._agents or w1 in self._patients)
        rev_action_patient = w2 in self._actions and w1 in self._patients
        rev_possessor_possessed = (w2 in self._possessors and
                                   w1 in self._possesseds)

        if rev_agent_action or rev_modifier_noun or rev_action_patient or rev_possessor_possessed:
            return CriticVerdict(
                reward=-1.0,
                recast=f"{w2} {w1}",
                reason=f"word_order_error:{w1}_{w2}",
            )

        return CriticVerdict(
            reward=-0.7,
            recast=hint,
            reason=f"no_valid_template:{w1}_{w2}",
        )

    def _evaluate_stage_1_multiword(self,
                                    prompt: str,
                                    tokens: list[str],
                                    content: list[str],
                                    hint: str | None) -> CriticVerdict:
        """Stage 1 emerging multi-word / copula speech (3+ content words, or a
        copula present). Beyond two-word telegraphic — rewarded when
        grammatical, never punished for length alone.

        Subject/verb detection is vocab-derived (the stage-1 vocab has no
        explicit subjects/verbs lists): a "subject-like" word is any in-vocab
        content word that isn't an action/modifier; a "verb-like" word is any
        action or copula.
        """
        # Inverted question: "is grass green" for a declarative prompt.
        if _looks_inverted_question(tokens) and not prompt.strip().lower().startswith(
                ("is ", "are ", "was ", "were ", "am ", "do ", "does ", "did ")):
            recast = self._svo_recast_from_inversion(tokens) or hint
            return CriticVerdict(-1.0, recast, "interrogative_for_declarative")

        # Subject-predicate inversion: "the green is grass".
        if (content and content[0] in self._modifiers
                and any(self._subject_like(w) for w in content[1:])
                and any(t in {"is", "are", "was", "were"} for t in tokens)):
            recast = self._svo_recast_modifier_subject(content, tokens) or hint
            return CriticVerdict(-1.0, recast, "subject_predicate_inversion")

        has_subject = any(self._subject_like(w) for w in content)
        has_verb = (any(self._verb_like(w) for w in content)
                    or any(t in _COPULAS for t in tokens))
        if has_subject and has_verb:
            return CriticVerdict(1.0, None, "valid_svo")

        return CriticVerdict(
            reward=-0.3,
            recast=hint,
            reason=f"missing_svo_structure:subj={has_subject},verb={has_verb}",
        )

    def _subject_like(self, w: str) -> bool:
        """True if ``w`` can head a noun phrase (explicit subject, or an
        in-vocab content noun — not a verb/action/modifier/function word)."""
        if w in self._subjects:
            return True
        return (w in self._vocab
                and w not in self._actions
                and w not in self._verbs
                and w not in self._modifiers
                and w not in self._function_words
                and w not in _COPULAS)

    def _verb_like(self, w: str) -> bool:
        """True if ``w`` can serve as a predicate verb (verb, action, copula)."""
        return w in self._verbs or w in self._actions or w in _COPULAS

    # -- stage 2 -------------------------------------------------------------

    def _evaluate_stage_2(self, prompt: str, tokens: list[str]) -> CriticVerdict:
        """Stage 2: 3-5 content words in SVO order. Function words optional."""
        rules = STAGE_RULES[2]
        hint = self._lookup_hint(prompt)

        oov = [t for t in tokens if t not in self._vocab and t not in self._function_words]
        if oov:
            return CriticVerdict(
                reward=-1.0,
                recast=hint,
                reason=f"out_of_stage_vocab:{','.join(oov[:3])}",
            )

        if len(tokens) < rules["min_words"] or len(tokens) > rules["max_words"]:
            return CriticVerdict(
                reward=-0.5,
                recast=hint,
                reason=f"wrong_word_count:{len(tokens)}",
            )

        # Strip function words for SVO structural check.
        content = [t for t in tokens if t not in self._function_words]
        if len(content) < 2:
            return CriticVerdict(
                reward=-0.5,
                recast=hint,
                reason=f"too_few_content_words:{len(content)}",
            )

        # Inversion detection: declarative was asked, response starts with
        # copula/aux — looks like "is grass green" instead of "grass is green".
        if _looks_inverted_question(tokens) and not prompt.strip().lower().startswith(
                ("is ", "are ", "was ", "were ", "am ", "do ", "does ", "did ")):
            # Build a recast by swapping copula and following noun if pattern
            # is "<copula> <subject> <predicate>". Fall back to hint.
            recast = self._svo_recast_from_inversion(tokens) or hint
            return CriticVerdict(
                reward=-1.0,
                recast=recast,
                reason="interrogative_for_declarative",
            )

        # Subject-predicate inversion: "the green is grass" — first content
        # word is a known modifier and a later content word is a known subject
        # → likely inverted.
        if (content[0] in self._modifiers
                and any(w in self._subjects for w in content[1:])
                and any(w in {"is", "are", "was", "were"} for w in tokens)):
            recast = self._svo_recast_modifier_subject(content, tokens) or hint
            return CriticVerdict(
                reward=-1.0,
                recast=recast,
                reason="subject_predicate_inversion",
            )

        # Basic SVO check: at least one content word is a known subject and
        # one is a known verb (or copula present in tokens).
        has_subject = any(w in self._subjects for w in content)
        has_verb = any(w in self._verbs for w in content) or \
                   any(w in {"is", "are", "was", "were", "am", "be"} for w in tokens)

        if has_subject and has_verb:
            return CriticVerdict(
                reward=1.0,
                recast=None,
                reason="valid_svo",
            )

        return CriticVerdict(
            reward=-0.3,
            recast=hint,
            reason=f"missing_svo_structure:subj={has_subject},verb={has_verb}",
        )

    # -- stage 3+ ------------------------------------------------------------

    def _evaluate_stage_3plus(self,
                              prompt: str,
                              response: str,
                              tokens: list[str]) -> CriticVerdict:
        """Stage 3+: delegate to LLM judge if available, else rule-based SVO."""
        if self._llm_callable is not None:
            try:
                judge_prompt = (
                    f"You are a developmental linguist evaluating a child's "
                    f"response. Question: {prompt!r}. Response: {response!r}. "
                    f"Reply with a single line: reward=<float in [-1,1]>, "
                    f"recast=<corrected form or NONE>, reason=<short reason>."
                )
                raw = self._llm_callable(judge_prompt)
                return self._parse_llm_verdict(raw)
            except Exception as e:  # noqa: BLE001 — LLM failures fall through.
                fallback = self._evaluate_stage_2(prompt, tokens)
                return CriticVerdict(
                    reward=fallback.reward,
                    recast=fallback.recast,
                    reason=f"llm_fallback:{type(e).__name__}",
                )
        # No LLM — reuse stage-2 rule-based check (still works for declaratives).
        return self._evaluate_stage_2(prompt, tokens)

    # -- helpers -------------------------------------------------------------

    def _load_vocab(self, path: Path) -> None:
        """Load a stage vocab JSON file. Idempotent."""
        if not path.exists():
            # Soft failure: stay with empty vocab. Caller can still get
            # verdicts (everything is OOV); useful in tests with synthetic
            # vocabs that don't ship with the repo.
            self._vocab = set()
            self._function_words = set()
            self._agents = set()
            self._actions = set()
            self._patients = set()
            self._modifiers = set()
            self._possessors = set()
            self._possesseds = set()
            self._subjects = set()
            self._verbs = set()
            self._objects = set()
            self._semantic_hints = {}
            return
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        self._vocab = set(w.lower() for w in data.get("words", []))
        self._function_words = set(w.lower() for w in data.get("function_words", []))
        self._agents = set(w.lower() for w in data.get("agents", []))
        self._actions = set(w.lower() for w in data.get("actions", []))
        self._patients = set(w.lower() for w in data.get("patients", []))
        self._modifiers = set(w.lower() for w in data.get("modifiers", []))
        self._possessors = set(w.lower() for w in data.get("possessors", []))
        self._possesseds = set(w.lower() for w in data.get("possesseds", []))
        self._subjects = set(w.lower() for w in data.get("subjects", []))
        self._verbs = set(w.lower() for w in data.get("verbs", []))
        self._objects = set(w.lower() for w in data.get("objects", []))
        raw_hints = data.get("semantic_hints", {}) or {}
        self._semantic_hints = {
            _normalize_prompt(k): v.lower() for k, v in raw_hints.items()
        }

    def _lookup_hint(self, prompt: str) -> str | None:
        """Return the expected response (lowercased) for this prompt, if any."""
        key = _normalize_prompt(prompt)
        if not key:
            return None
        return self._semantic_hints.get(key)

    def _svo_recast_from_inversion(self, tokens: list[str]) -> str | None:
        """Reconstruct ``"<subject> <copula> <predicate>"`` from inverted form.

        Best-effort: "is grass green" → "grass is green".
        """
        if len(tokens) < 3:
            return None
        cop, subj, *rest = tokens
        if cop in {"is", "are", "was", "were", "am"} and self._subject_like(subj):
            return " ".join([subj, cop] + rest)
        return None

    def _svo_recast_modifier_subject(self,
                                     content: list[str],
                                     tokens: list[str]) -> str | None:
        """Reconstruct ``"<subject> <copula> <modifier>"`` from inverted form.

        Best-effort: "the green is grass" → "grass is green". We scan the
        content words for a known subject and reorder.
        """
        modifier = next((w for w in content if w in self._modifiers), None)
        subject = next((w for w in content if self._subject_like(w)), None)
        copula = next((w for w in tokens if w in {"is", "are", "was", "were"}), "is")
        if modifier and subject:
            return f"{subject} {copula} {modifier}"
        return None

    @staticmethod
    def _parse_llm_verdict(raw: str) -> CriticVerdict:
        """Parse ``"reward=X, recast=Y, reason=Z"`` line. Defensive."""
        reward = 0.0
        recast: str | None = None
        reason = "llm_parse_default"
        if not raw:
            return CriticVerdict(reward, recast, reason)
        m_r = re.search(r"reward\s*=\s*([\-+]?\d*\.?\d+)", raw)
        if m_r:
            try:
                reward = max(-1.0, min(1.0, float(m_r.group(1))))
            except ValueError:
                pass
        m_rc = re.search(r"recast\s*=\s*([^,\n]+)", raw)
        if m_rc:
            v = m_rc.group(1).strip().strip("\"'")
            recast = None if v.upper() == "NONE" else v
        m_re = re.search(r"reason\s*=\s*(.+)", raw)
        if m_re:
            reason = m_re.group(1).strip()
        return CriticVerdict(reward, recast, reason)


__all__ = ["CaregiverCritic", "CriticVerdict", "STAGE_RULES"]
