#!/usr/bin/env python3
"""Claude Teacher — Uses the Claude CLI (Max subscription) to generate lessons for Athena.

Generates (lesson_text, target_embedding) pairs, evaluates Athena's responses,
and conducts interactive teaching dialogues.  Uses `claude -p` (non-interactive
mode) so all calls run through your existing Max plan — no separate API costs.
"""

import hashlib
import json
import logging
import os
import subprocess
import time
from collections import deque
from dataclasses import dataclass

logger = logging.getLogger(__name__)

_embed_model = None
_use_onnx = None  # None = not checked, True/False = checked


def _get_embed_model():
    global _embed_model
    if _embed_model is not None:
        return _embed_model
    from sentence_transformers import SentenceTransformer
    # Force CPU for embeddings — GPU VRAM is reserved for nimcp brain training.
    # PyTorch 2.8+: low_cpu_mem_usage=True (default) uses meta tensors which
    # crash on .to('cpu'). Disable to use real tensors from the start.
    _embed_model = SentenceTransformer('BAAI/bge-large-en-v1.5', device='cpu',
                                       model_kwargs={'low_cpu_mem_usage': False})
    return _embed_model


def _try_onnx():
    """Try ONNX encoder first — faster and avoids loading PyTorch."""
    global _use_onnx
    if _use_onnx is not None:
        return _use_onnx
    try:
        from onnx_encoder import encode_text as _onnx_encode
        # Smoke test
        _onnx_encode("test")
        _use_onnx = True
        logger.info("Using ONNX encoder (faster, no PyTorch overhead)")
    except Exception as e:
        _use_onnx = False
        logger.info("ONNX encoder unavailable (%s), falling back to SentenceTransformer", e)
    return _use_onnx


_encode_cache = {}
_ENCODE_CACHE_MAX = 50000
_ENCODE_CACHE_PATH = os.path.join(os.path.dirname(__file__), '..', 'checkpoints', 'athena', 'embed_cache.npz')


def _load_encode_cache():
    """Load persistent embedding cache from disk."""
    global _encode_cache
    import numpy as np
    try:
        if os.path.exists(_ENCODE_CACHE_PATH):
            data = np.load(_ENCODE_CACHE_PATH, allow_pickle=True)
            texts = data['texts']
            embeddings = data['embeddings']
            for t, e in zip(texts, embeddings):
                _encode_cache[str(t)] = np.asarray(e, dtype=np.float32)
            logger.info("Loaded %d cached embeddings from %s", len(_encode_cache), _ENCODE_CACHE_PATH)
    except Exception as e:
        logger.warning("Failed to load embedding cache: %s", e)


def _save_encode_cache():
    """Persist embedding cache to disk (atomic write to prevent corruption)."""
    import numpy as np
    try:
        os.makedirs(os.path.dirname(_ENCODE_CACHE_PATH), exist_ok=True)
        texts = list(_encode_cache.keys())
        embeddings = [_encode_cache[t] for t in texts]
        tmp_path = _ENCODE_CACHE_PATH + ".tmp.npz"
        np.savez_compressed(tmp_path,
                            texts=np.array(texts, dtype=object),
                            embeddings=np.array(embeddings))
        os.replace(tmp_path, _ENCODE_CACHE_PATH)
        logger.info("Saved %d embeddings to cache %s", len(texts), _ENCODE_CACHE_PATH)
    except Exception as e:
        logger.warning("Failed to save embedding cache: %s", e)
        try:
            os.remove(tmp_path)
        except OSError:
            pass


# Load cache on module import
_load_encode_cache()

def encode_text(text: str):
    """Encode text to 1024-dim embedding.

    Prefers ONNX encoder (GPU-accelerated, no PyTorch). Falls back to
    SentenceTransformer if ONNX is unavailable.
    Caches results — same text returns cached embedding instantly.
    """
    import numpy as np
    if text in _encode_cache:
        return _encode_cache[text].copy()

    if _try_onnx():
        from onnx_encoder import encode_text as onnx_encode
        result = onnx_encode(text)
    else:
        model = _get_embed_model()
        emb = model.encode(text, convert_to_numpy=True)
        result = np.asarray(emb, dtype=np.float32).ravel()

    if len(_encode_cache) < _ENCODE_CACHE_MAX:
        _encode_cache[text] = result.copy()
        # Persist every 500 new entries
        if len(_encode_cache) % 500 == 0:
            _save_encode_cache()
    return result


def batch_encode_texts(texts):
    """Batch-encode multiple texts to 1024-dim embeddings."""
    import numpy as np

    # Check cache for all texts first
    results = [None] * len(texts)
    uncached_indices = []
    for i, t in enumerate(texts):
        if t in _encode_cache:
            results[i] = _encode_cache[t].copy()
        else:
            uncached_indices.append(i)

    if not uncached_indices:
        return results  # All cached — skip model entirely

    # Encode only uncached texts
    uncached_texts = [texts[i] for i in uncached_indices]
    if _try_onnx():
        from onnx_encoder import encode_batch as onnx_batch
        embs = onnx_batch(uncached_texts)
        new_embs = [np.asarray(e, dtype=np.float32).ravel() for e in embs]
    else:
        model = _get_embed_model()
        embs = model.encode(uncached_texts, convert_to_numpy=True, batch_size=64)
        new_embs = [np.asarray(e, dtype=np.float32).ravel() for e in embs]

    # Fill results and update cache
    for idx, emb in zip(uncached_indices, new_embs):
        results[idx] = emb
        if len(_encode_cache) < _ENCODE_CACHE_MAX:
            _encode_cache[texts[idx]] = emb.copy()

    # Persist cache to disk after batch encoding
    if uncached_indices:
        _save_encode_cache()

    return results


# ---------------------------------------------------------------------------
# Lesson specification
# ---------------------------------------------------------------------------

@dataclass
class LessonSpec:
    domain: str
    topic: str
    difficulty: float  # 0.0 = trivial, 1.0 = expert
    context: str = ""
    lesson_type: str = "teach"  # teach, quiz, dialogue


# ---------------------------------------------------------------------------
# Curriculum definitions for each developmental stage
# ---------------------------------------------------------------------------

STAGE_CURRICULA = {
    0: {  # Birth — sensory grounding
        "name": "Birth",
        "domains": ["perception", "patterns"],
        "topics": [
            LessonSpec("perception", "colors and shapes", 0.1),
            LessonSpec("perception", "spatial relationships", 0.15),
            LessonSpec("perception", "size and quantity", 0.1),
            LessonSpec("patterns", "simple sequences", 0.15),
            LessonSpec("patterns", "repetition and rhythm", 0.2),
            LessonSpec("patterns", "matching and sorting", 0.15),
        ],
    },
    1: {  # Babbling — phoneme patterns, word boundaries
        "name": "Babbling",
        "domains": ["language", "vocabulary"],
        "topics": [
            LessonSpec("language", "common sounds and phonemes", 0.2),
            LessonSpec("language", "syllable patterns", 0.25),
            LessonSpec("language", "word boundaries", 0.25),
            LessonSpec("vocabulary", "concrete nouns", 0.2),
            LessonSpec("vocabulary", "action verbs", 0.25),
            LessonSpec("vocabulary", "descriptive words", 0.25),
        ],
    },
    2: {  # First Words — simple concepts
        "name": "First Words",
        "domains": ["language", "concepts", "reasoning"],
        "topics": [
            LessonSpec("language", "simple sentences", 0.3),
            LessonSpec("concepts", "categories and types", 0.3),
            LessonSpec("concepts", "cause and effect", 0.35),
            LessonSpec("reasoning", "basic comparisons", 0.3),
            LessonSpec("reasoning", "simple analogies", 0.35),
            LessonSpec("concepts", "temporal concepts", 0.35),
        ],
    },
    3: {  # Sentences — grammar, reasoning
        "name": "Sentences",
        "domains": ["language", "reasoning", "knowledge"],
        "topics": [
            LessonSpec("language", "complex sentences", 0.5),
            LessonSpec("language", "grammar patterns", 0.45),
            LessonSpec("reasoning", "logical deduction", 0.5),
            LessonSpec("reasoning", "if-then reasoning", 0.5),
            LessonSpec("knowledge", "basic science facts", 0.45),
            LessonSpec("knowledge", "geography and culture", 0.45),
        ],
    },
    4: {  # Conversation — dialogue, Q&A
        "name": "Conversation",
        "domains": ["dialogue", "knowledge", "reasoning"],
        "topics": [
            LessonSpec("dialogue", "question answering", 0.6, lesson_type="dialogue"),
            LessonSpec("dialogue", "topic discussion", 0.65, lesson_type="dialogue"),
            LessonSpec("knowledge", "history and events", 0.6),
            LessonSpec("knowledge", "science concepts", 0.65),
            LessonSpec("reasoning", "abstract reasoning", 0.7),
            LessonSpec("reasoning", "multi-step problems", 0.7),
        ],
    },
    5: {  # Self-Directed — deep learning
        "name": "Self-Directed",
        "domains": ["philosophy", "creativity", "meta-cognition"],
        "topics": [
            LessonSpec("philosophy", "ethics and values", 0.8),
            LessonSpec("philosophy", "epistemology", 0.8),
            LessonSpec("creativity", "creative writing", 0.75, lesson_type="dialogue"),
            LessonSpec("creativity", "problem solving", 0.8, lesson_type="dialogue"),
            LessonSpec("meta-cognition", "learning strategies", 0.8),
            LessonSpec("meta-cognition", "self-reflection", 0.85, lesson_type="dialogue"),
        ],
    },
}


# ---------------------------------------------------------------------------
# Claude Teacher
# ---------------------------------------------------------------------------

class ClaudeTeacher:
    """Uses the `claude` CLI (Max subscription) to generate lessons and evaluate responses.

    All calls go through `claude -p` (non-interactive one-shot mode), which runs
    on your existing Max plan.  No separate API key or billing required.
    """

    def __init__(self, *,
                 model: str = "sonnet",
                 cache_size: int = 200,
                 timeout: int = 180,
                 max_tokens: int = 1024,
                 # Legacy kwargs accepted but ignored (for load_state compat)
                 routine_model: str = "",
                 eval_model: str = "",
                 token_budget: int = 0,
                 max_api_calls_per_minute: int = 0):
        self.model = model
        self._cache: dict[str, tuple[str, any]] = {}
        self._cache_max = cache_size
        self.timeout = timeout
        self.max_tokens = max_tokens

        # Teaching context
        self.mastered_concepts: list[str] = []
        self.current_stage = 0
        self.lessons_taught = 0
        self.calls_made = 0

    def _call_claude(self, prompt: str, system: str = "",
                     max_tokens: int | None = None) -> str:
        """Call Claude via CLI with minimal startup overhead.

        Uses the Max subscription — no API costs.
        Optimized flags: no MCP servers, no tools, no session persistence,
        no project settings (CLAUDE.md), minimal system prompt.
        """
        mt = max_tokens or self.max_tokens

        # Build the full prompt with system context prepended
        full_prompt = prompt
        if system:
            full_prompt = f"System: {system}\n\n{prompt}"

        cmd = [
            "claude", "-p", full_prompt,
            "--model", self.model,
            "--output-format", "text",
            "--system-prompt", "",      # override system prompt (skip CLAUDE.md)
            "--setting-sources", "",    # skip loading project/user settings entirely
            "--allowed-tools", "",      # disable all tools (pure text generation)
        ]

        # Clean env for claude subprocess:
        # - Remove CLAUDECODE to prevent nesting detection
        # - Remove CUDA_VISIBLE_DEVICES (we hide GPU from PyTorch, but claude doesn't need it)
        env = {k: v for k, v in os.environ.items()
               if k not in ("CLAUDECODE", "CUDA_VISIBLE_DEVICES")}

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout,
                env=env,
                stdin=subprocess.DEVNULL,  # don't wait 3s for stdin (rc=1 in daemonized trainer)
            )
            if result.returncode != 0:
                stderr = result.stderr.strip()
                raise RuntimeError(f"claude CLI failed (rc={result.returncode}): {stderr}")
            text = result.stdout.strip()
            if not text:
                raise RuntimeError("claude CLI returned empty output")
            self.calls_made += 1
            return text
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"claude CLI timed out after {self.timeout}s")
        except FileNotFoundError:
            raise RuntimeError("claude CLI not found — is Claude Code installed?")

    def _cache_key(self, domain: str, topic: str, difficulty: float) -> str:
        raw = f"{domain}:{topic}:{difficulty:.2f}"
        return hashlib.md5(raw.encode()).hexdigest()

    def generate_lesson(self, spec: LessonSpec) -> tuple[str, "np.ndarray"]:
        """Generate a lesson appropriate to Athena's current developmental level.

        Returns (lesson_text, target_embedding).
        """
        import numpy as np

        # Check cache
        key = self._cache_key(spec.domain, spec.topic, spec.difficulty)
        if key in self._cache:
            logger.debug("Cache hit for %s/%s", spec.domain, spec.topic)
            return self._cache[key]

        system = (
            "You are a teacher for an AI student named Athena. "
            "Athena is learning progressively, like a human child. "
            f"Current developmental stage: {self.current_stage}. "
            f"Mastered concepts: {', '.join(self.mastered_concepts[-20:]) or 'none yet'}. "
            "Generate a clear, focused lesson. Respond with ONLY the lesson content — "
            "no meta-commentary about being a teacher."
        )

        prompt = (
            f"Domain: {spec.domain}\n"
            f"Topic: {spec.topic}\n"
            f"Difficulty: {spec.difficulty:.1f}/1.0\n"
        )
        if spec.context:
            prompt += f"Context: {spec.context}\n"
        prompt += (
            "\nGenerate a short teaching lesson (2-4 sentences) appropriate "
            "for this difficulty level. Be concrete and specific."
        )

        try:
            text = self._call_claude(prompt, system=system, max_tokens=256)
        except Exception as e:
            logger.warning("Claude CLI failed: %s — using fallback", e)
            text = self._fallback_lesson(spec)

        embedding = encode_text(text)

        # Cache result
        if len(self._cache) >= self._cache_max:
            oldest_key = next(iter(self._cache))
            del self._cache[oldest_key]
        self._cache[key] = (text, embedding)

        self.lessons_taught += 1
        return text, embedding

    def evaluate_response(self, lesson_text: str, athena_response: str) -> tuple[float, str]:
        """Evaluate Athena's decoded response to a lesson.

        Returns (score 0.0-1.0, feedback_text).
        """
        prompt = (
            f"A student named Athena was given this lesson:\n\"{lesson_text}\"\n\n"
            f"Athena's response was:\n\"{athena_response}\"\n\n"
            "Rate the response on a scale of 0.0 to 1.0 where:\n"
            "- 0.0 = completely incoherent/unrelated\n"
            "- 0.3 = shows some relevant patterns but not meaningful\n"
            "- 0.5 = partially relevant, shows understanding of some concepts\n"
            "- 0.7 = good understanding, mostly correct\n"
            "- 1.0 = excellent, fully correct response\n\n"
            "Respond in this exact format:\n"
            "SCORE: <number>\n"
            "FEEDBACK: <one sentence>"
        )

        try:
            response = self._call_claude(prompt, max_tokens=128)
            lines = response.strip().split("\n")
            score = 0.0
            feedback = "No feedback"
            for line in lines:
                if line.startswith("SCORE:"):
                    score = float(line.split(":")[1].strip())
                    score = max(0.0, min(1.0, score))
                elif line.startswith("FEEDBACK:"):
                    feedback = line.split(":", 1)[1].strip()
            return score, feedback
        except Exception as e:
            logger.warning("Evaluation failed: %s", e)
            return 0.0, f"Evaluation error: {e}"

    def generate_dialogue_turn(self, context: str,
                               athena_response: str) -> tuple[str, "np.ndarray"]:
        """Generate the next dialogue turn in response to Athena's output.

        Returns (next_prompt_text, target_embedding).
        """
        import numpy as np

        system = (
            "You are having a conversation with an AI student named Athena. "
            "Respond naturally and teach through dialogue. Keep responses "
            "concise (1-2 sentences). Build on what Athena said."
        )

        prompt = (
            f"Conversation so far:\n{context}\n\n"
            f"Athena said: \"{athena_response}\"\n\n"
            "Respond with your next teaching turn."
        )

        try:
            text = self._call_claude(prompt, system=system, max_tokens=256)
        except Exception as e:
            logger.warning("Dialogue generation failed: %s", e)
            text = "Tell me more about what you think."

        embedding = encode_text(text)
        return text, embedding

    def get_curriculum(self, stage: int) -> list[LessonSpec]:
        """Return curriculum appropriate to developmental stage."""
        if stage not in STAGE_CURRICULA:
            stage = min(stage, max(STAGE_CURRICULA.keys()))
        return list(STAGE_CURRICULA[stage]["topics"])

    def get_stage_name(self, stage: int) -> str:
        if stage in STAGE_CURRICULA:
            return STAGE_CURRICULA[stage]["name"]
        return f"Stage {stage}"

    def _fallback_lesson(self, spec: LessonSpec) -> str:
        """Generate a simple lesson without the CLI (for timeout / error fallback)."""
        fallbacks = {
            "perception": "Look at the world around you. Objects have shapes — circles are round, squares have four equal sides. Colors help us tell things apart.",
            "patterns": "Patterns repeat in predictable ways. The sequence 1, 2, 3 continues with 4. Red, blue, red, blue continues with red.",
            "language": "Words are building blocks of communication. Every sentence has a subject and a verb. 'The cat sits' is a complete thought.",
            "vocabulary": "A 'tree' is a tall plant with a trunk, branches, and leaves. Trees provide shade, oxygen, and homes for birds.",
            "concepts": "Things can be grouped into categories. Dogs, cats, and fish are all animals. Apples, bananas, and oranges are all fruits.",
            "reasoning": "If it is raining, the ground gets wet. We see wet ground. Therefore, it probably rained recently. This is called deduction.",
            "knowledge": "The Earth orbits the Sun. One complete orbit takes about 365 days, which we call a year. The Earth also rotates on its axis once per day.",
            "dialogue": "A good conversation involves listening and responding. When someone asks a question, think about what they want to know before answering.",
            "philosophy": "Ethics is the study of right and wrong. Different people may have different values, but some principles like fairness are widely shared.",
            "creativity": "Creativity means combining ideas in new ways. A story needs a beginning, middle, and end. Characters face challenges and grow.",
            "meta-cognition": "Learning how you learn is called metacognition. When something is hard, try breaking it into smaller pieces.",
        }
        return fallbacks.get(spec.domain, f"Today we learn about {spec.topic} in the field of {spec.domain}.")

    def save_state(self, path: str):
        """Save teacher state (mastered concepts, stats)."""
        state = {
            "mastered_concepts": self.mastered_concepts,
            "current_stage": self.current_stage,
            "lessons_taught": self.lessons_taught,
            "calls_made": self.calls_made,
        }
        with open(path, "w") as f:
            json.dump(state, f, indent=2)

    @classmethod
    def load_state(cls, path: str, **kwargs) -> "ClaudeTeacher":
        """Load teacher state from file."""
        teacher = cls(**kwargs)
        if os.path.exists(path):
            with open(path) as f:
                state = json.load(f)
            teacher.mastered_concepts = state.get("mastered_concepts", [])
            teacher.current_stage = state.get("current_stage", 0)
            teacher.lessons_taught = state.get("lessons_taught", 0)
            teacher.calls_made = state.get("calls_made", 0)
        return teacher


# ===========================================================================
# CE-19: Hybrid Claude-as-teacher loop (active LLM-in-the-loop tutor).
# ===========================================================================
# This section is the curriculum-drip surface that the immersive training
# loop calls. It coexists with the older CLI-driven `ClaudeTeacher` class
# above (which other scripts use for embedding + lesson generation via the
# Max-subscription `claude` CLI). The CE-19 surface uses the Anthropic
# Python SDK directly so it can be cost-bounded, privacy-bounded, and
# fully mockable in tests.
#
# Like CE-1..CE-14 siblings, CE-19 is a drip module:
#   1. Generate a richer teaching question from a stage-appropriate topic.
#   2. Score the brain's reply with a multi-axis rubric (factuality,
#      coherence, on-topic) — more nuanced than keyword recall.
#   3. Supply a corrective exemplar to re-anchor when the brain fails.
#
# Opt-in + cost-bounded + privacy-preserving:
#   - The `anthropic` package is imported LAZILY inside functions. If it
#     is not installed, the drip silently no-ops.
#   - The API key is read from `os.environ["ANTHROPIC_API_KEY"]`. Missing
#     key  -> silent no-op (NOT an exception).
#   - Module-level `_CALL_COUNTER` enforces `MAX_CALLS_PER_SESSION`
#     (default 200; override via env `NIMCP_CLAUDE_TEACHER_MAX_CALLS`).
#     Once the cap is reached, every subsequent call short-circuits and
#     returns None — we log once that the cap was hit.
#   - Default model `claude-haiku-4-5-20251001` (cheapest current).
#     Override via env `NIMCP_CLAUDE_TEACHER_MODEL`.
#   - Default `max_tokens=512`. Brain replies are short.
#   - Temperature 0.5 for question generation; 0.0 for scoring + exemplars
#     (we want determinism in feedback).
#
# Privacy contract — what we send to the API:
#   - The topic seed (a short stage-appropriate phrase).
#   - The question prompt (Claude's own prior output, fed back into a
#     score call).
#   - The brain's plain-text reply (what Athena would say out loud).
# That is ALL. We NEVER send checkpoint paths, weights, synapse counts,
# neuron state, or any other non-user-visible data. The drip code only
# takes a brain handle and calls public produce_text / learn_language /
# train_language methods, never inspecting brain internals.
#
# Public API (CE-19):
#     TEACHER_TOPIC_SEEDS         dict[stage] -> list[str]
#     MAX_CALLS_PER_SESSION       int (env-overridable)
#     TEACHER_PASS_THRESHOLD      float (default 0.5)
#     CALL_BUDGET_REMAINING()     -> int
#     RESET_CALL_COUNTER()        -> None
#     is_available()              -> bool
#     generate_question(...)      -> dict | None
#     score_brain_response(...)   -> dict | None
#     build_corrective_exemplar(...) -> str | None
#     run_claude_teacher_drip(...)   -> list[dict] | None
#
# Stage 1 is a no-op (vocabulary still bootstrapping; LLM-graded feedback
# is too noisy at that stage).

import re as _re19  # local alias — main module already imports json/os

# Stage-appropriate teacher topic seeds. Stage 2 stays in concrete /
# everyday register; stage 3 admits abstract / meta-cognitive themes.
TEACHER_TOPIC_SEEDS: dict = {
    1: [],  # no-op — too early
    2: [
        "the seasons changing",
        "what mothers do",
        "why the sky is blue",
        "how plants grow",
        "kindness and sharing",
        "the difference between alive and not alive",
    ],
    3: [
        "why some choices are harder than others",
        "what makes a story feel true",
        "the difference between knowing and believing",
        "why people apologize",
        "how to recognize when you are wrong",
        "the relationship between memory and identity",
    ],
}


def _ce19_env_int(name: str, default: int) -> int:
    raw = os.environ.get(name)
    if raw is None:
        return default
    try:
        return int(raw)
    except ValueError:
        return default


MAX_CALLS_PER_SESSION: int = _ce19_env_int("NIMCP_CLAUDE_TEACHER_MAX_CALLS", 200)

_CALL_COUNTER: int = 0
_CAP_LOG_EMITTED: bool = False
_UNAVAILABLE_LOG_EMITTED: bool = False

TEACHER_PASS_THRESHOLD: float = 0.5

_CE19_DEFAULT_MODEL = "claude-haiku-4-5-20251001"


def CALL_BUDGET_REMAINING() -> int:  # noqa: N802 — public uppercase per spec
    """Return remaining successful calls allowed this session."""
    return max(0, MAX_CALLS_PER_SESSION - _CALL_COUNTER)


def RESET_CALL_COUNTER() -> None:  # noqa: N802 — public uppercase per spec
    """Reset call counter + suppression flags (intended for tests)."""
    global _CALL_COUNTER, _CAP_LOG_EMITTED, _UNAVAILABLE_LOG_EMITTED
    _CALL_COUNTER = 0
    _CAP_LOG_EMITTED = False
    _UNAVAILABLE_LOG_EMITTED = False


def _ce19_resolve_model(model):
    if model:
        return model
    return os.environ.get("NIMCP_CLAUDE_TEACHER_MODEL", _CE19_DEFAULT_MODEL)


def _ce19_try_import_anthropic():
    """Lazy + tolerant import. Returns the module or None."""
    try:
        import anthropic  # type: ignore
        return anthropic
    except Exception:
        return None


def is_available() -> bool:
    """True iff anthropic importable AND ANTHROPIC_API_KEY set."""
    if _ce19_try_import_anthropic() is None:
        return False
    return bool(os.environ.get("ANTHROPIC_API_KEY"))


def _ce19_build_client(client):
    """Use injected client (tests) or build a fresh one. Returns None on
    failure (anthropic missing, constructor raises)."""
    if client is not None:
        return client
    anthropic = _ce19_try_import_anthropic()
    if anthropic is None:
        return None
    try:
        return anthropic.Anthropic()
    except Exception:
        return None


def _ce19_check_cap() -> bool:
    """Return True if budget remains. Logs once when cap is reached."""
    global _CAP_LOG_EMITTED
    if _CALL_COUNTER >= MAX_CALLS_PER_SESSION:
        if not _CAP_LOG_EMITTED:
            print(f"  [ClaudeTeacher] call cap reached "
                  f"({MAX_CALLS_PER_SESSION} calls); subsequent drips will "
                  f"no-op until RESET_CALL_COUNTER() or process restart.")
            _CAP_LOG_EMITTED = True
        return False
    return True


def _ce19_bump_counter() -> None:
    """Bump on every API attempt — bill conservatively. Even if the
    response was malformed, we still consumed network + tokens (the API
    bills per request started)."""
    global _CALL_COUNTER
    _CALL_COUNTER += 1


def _ce19_extract_text(resp):
    """Pull plain text out of an Anthropic Messages API response object.
    Returns None if the shape is unexpected."""
    try:
        block = resp.content[0]
        text = getattr(block, "text", None)
        if isinstance(text, str):
            return text
        if isinstance(block, dict):
            t = block.get("text")
            if isinstance(t, str):
                return t
    except Exception:
        return None
    return None


def _ce19_strip_code_fences(text: str) -> str:
    """Remove ```json / ``` fences if Claude wrapped its output."""
    s = text.strip()
    if s.startswith("```"):
        s = _re19.sub(r"^```[a-zA-Z]*\n?", "", s)
        if s.endswith("```"):
            s = s[: -3]
        s = s.strip()
    return s


def _ce19_parse_json(text: str):
    if not text:
        return None
    cleaned = _ce19_strip_code_fences(text)
    try:
        obj = json.loads(cleaned)
    except Exception:
        m = _re19.search(r"\{.*\}", cleaned, _re19.DOTALL)
        if not m:
            return None
        try:
            obj = json.loads(m.group(0))
        except Exception:
            return None
    if not isinstance(obj, dict):
        return None
    return obj


def _ce19_clip01(x) -> float:
    try:
        v = float(x)
    except Exception:
        return 0.0
    if v < 0.0:
        return 0.0
    if v > 1.0:
        return 1.0
    return v


# ---------------------------------------------------------------------------
# CE-19 public API: generate_question
# ---------------------------------------------------------------------------

def generate_question(topic: str, stage: int, *,
                      model=None,
                      max_tokens=None,
                      client=None):
    """Ask Claude for ONE concrete teaching question on `topic` plus 5-10
    expected concept words. Returns
    {"question": str, "expected_concepts": list[str]} or None on failure
    (cap, missing key, parse error, exception).

    `client` lets tests inject a mock Anthropic client.
    """
    if not topic:
        return None
    if not _ce19_check_cap():
        return None

    c = _ce19_build_client(client)
    if c is None:
        return None

    sys_prompt = (
        "You are tutoring a young artificial person on stage-%d concepts. "
        "Generate ONE concrete teaching question on the topic, plus 5-10 "
        "expected concept words for keyword scoring." % int(stage)
    )
    user_prompt = (
        "Topic: %s\n\nReturn JSON: {\"question\": ..., "
        "\"expected_concepts\": [...]}." % topic
    )

    _ce19_bump_counter()
    try:
        resp = c.messages.create(
            model=_ce19_resolve_model(model),
            max_tokens=max_tokens or 512,
            temperature=0.5,
            system=sys_prompt,
            messages=[{"role": "user", "content": user_prompt}],
        )
    except Exception as e:
        print(f"  [ClaudeTeacher] generate_question API error: {e}")
        return None

    text = _ce19_extract_text(resp)
    if not text:
        return None
    obj = _ce19_parse_json(text)
    if obj is None:
        return None
    question = obj.get("question")
    concepts = obj.get("expected_concepts") or []
    if not isinstance(question, str) or not question.strip():
        return None
    if not isinstance(concepts, list):
        concepts = []
    concept_list = [str(x).strip() for x in concepts if str(x).strip()]
    return {"question": question.strip(), "expected_concepts": concept_list}


# ---------------------------------------------------------------------------
# CE-19 public API: score_brain_response
# ---------------------------------------------------------------------------

def score_brain_response(question: str,
                         brain_response: str,
                         expected_concepts,
                         *,
                         model=None,
                         client=None):
    """Ask Claude to score the brain's reply on a 4-axis rubric. Returns
    {"composite": float in [0,1], "factuality": float, "coherence":
    float, "on_topic": float, "rationale": str} or None on failure.

    Missing rubric keys default to 0.0 (charitable to the parser, harsh
    to the brain — the brain failed to elicit a clean rubric line)."""
    if not _ce19_check_cap():
        return None
    c = _ce19_build_client(client)
    if c is None:
        return None

    concepts_str = ", ".join(str(x) for x in (expected_concepts or []))
    sys_prompt = (
        "You are scoring a young artificial person's reply against a "
        "question. Return JSON with factuality / coherence / on_topic / "
        "composite floats in [0,1] plus a brief rationale."
    )
    user_prompt = (
        "Question: %s\n\nReply: %s\n\nExpected concepts: %s\n\n"
        "Return JSON: {\"factuality\": float, \"coherence\": float, "
        "\"on_topic\": float, \"composite\": float, \"rationale\": str}."
    ) % (question, brain_response, concepts_str)

    _ce19_bump_counter()
    try:
        resp = c.messages.create(
            model=_ce19_resolve_model(model),
            max_tokens=512,
            temperature=0.0,
            system=sys_prompt,
            messages=[{"role": "user", "content": user_prompt}],
        )
    except Exception as e:
        print(f"  [ClaudeTeacher] score_brain_response API error: {e}")
        return None

    text = _ce19_extract_text(resp)
    if not text:
        return None
    obj = _ce19_parse_json(text)
    if obj is None:
        return None

    factuality = _ce19_clip01(obj.get("factuality", 0.0))
    coherence = _ce19_clip01(obj.get("coherence", 0.0))
    on_topic = _ce19_clip01(obj.get("on_topic", 0.0))
    if "composite" in obj:
        composite = _ce19_clip01(obj.get("composite", 0.0))
    else:
        composite = (factuality + coherence + on_topic) / 3.0
    rationale = obj.get("rationale", "")
    if not isinstance(rationale, str):
        rationale = str(rationale)
    return {
        "factuality": factuality,
        "coherence": coherence,
        "on_topic": on_topic,
        "composite": composite,
        "rationale": rationale,
    }


# ---------------------------------------------------------------------------
# CE-19 public API: build_corrective_exemplar
# ---------------------------------------------------------------------------

def build_corrective_exemplar(question: str,
                              brain_response: str,
                              expected_concepts,
                              *,
                              model=None,
                              client=None):
    """Ask Claude for a 1-3 sentence corrective answer in the brain's
    expected vocabulary. Returns the text body, or None on failure."""
    if not _ce19_check_cap():
        return None
    c = _ce19_build_client(client)
    if c is None:
        return None

    concepts_str = ", ".join(str(x) for x in (expected_concepts or []))
    sys_prompt = (
        "Return a 1-3 sentence answer to the question in plain language a "
        "young artificial person can learn from."
    )
    user_prompt = (
        "Question: %s\n\nThe young person's incorrect reply was: %s\n\n"
        "Expected concepts (use them naturally if they fit): %s\n\n"
        "Return ONLY the corrective answer text — no preamble, no JSON, "
        "1-3 sentences."
    ) % (question, brain_response, concepts_str)

    _ce19_bump_counter()
    try:
        resp = c.messages.create(
            model=_ce19_resolve_model(model),
            max_tokens=512,
            temperature=0.0,
            system=sys_prompt,
            messages=[{"role": "user", "content": user_prompt}],
        )
    except Exception as e:
        print(f"  [ClaudeTeacher] build_corrective_exemplar API error: {e}")
        return None

    text = _ce19_extract_text(resp)
    if not text:
        return None
    cleaned = _ce19_strip_code_fences(text).strip()
    if not cleaned:
        return None
    return cleaned


# ---------------------------------------------------------------------------
# CE-19 drip driver
# ---------------------------------------------------------------------------

# Module-level rotation counter — kept across calls in one process, NOT
# checkpointed. (Same persistence rule as storytelling.)
_TOPIC_COUNTER: dict = {1: 0, 2: 0, 3: 0}


def _ce19_bump_topic(stage: int) -> int:
    cur = _TOPIC_COUNTER.get(int(stage), 0)
    _TOPIC_COUNTER[int(stage)] = cur + 1
    return cur


def _ce19_pick_topic(stage: int, topic_index):
    seeds = TEACHER_TOPIC_SEEDS.get(int(stage), [])
    if not seeds:
        return ""
    idx = topic_index if topic_index is not None else _ce19_bump_topic(stage)
    return seeds[int(idx) % len(seeds)]


def _ce19_build_intent(composer, prompt_text: str):
    """Composer-or-zero pattern, mirrors storytelling._build_intent."""
    intent = None
    if composer is not None:
        try:
            intent = composer.compose(text=prompt_text, modality="text")
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


def run_claude_teacher_drip(brain, stage: int, *,
                            composer=None,
                            num_rounds: int = 1,
                            topic_index=None,
                            enabled: bool = True,
                            log_every: int = 1):
    """One Claude-as-teacher drip. Returns a list of per-round result
    dicts, or None when stage 1 / disabled / unavailable / cap-hit /
    no rounds completed.

    Each round:
      1. Pick a stage topic.
      2. generate_question() -> {question, expected_concepts}.
      3. compose -> brain.produce_text(intent) -> capture text.
      4. score_brain_response(question, brain_text, concepts).
      5. composite >= TEACHER_PASS_THRESHOLD ->
            brain.learn_language(brain_text)
         else ->
            exemplar = build_corrective_exemplar(...)
            if exemplar: brain.train_language(exemplar, exemplar)
            else:        brain.train_language(question, question)

    All brain calls are wrapped in try/except — failures log + continue.
    """
    global _UNAVAILABLE_LOG_EMITTED

    if int(stage) == 1:
        return None
    if not enabled:
        return None

    if not is_available():
        if not _UNAVAILABLE_LOG_EMITTED:
            print("  [ClaudeTeacher] anthropic SDK or ANTHROPIC_API_KEY "
                  "missing — drip will no-op this session.")
            _UNAVAILABLE_LOG_EMITTED = True
        return None

    if _CALL_COUNTER >= MAX_CALLS_PER_SESSION:
        _ce19_check_cap()
        return None

    rounds: list = []
    calls_at_start = _CALL_COUNTER

    for r in range(max(1, int(num_rounds))):
        topic = _ce19_pick_topic(stage, topic_index)
        if not topic:
            break

        q = generate_question(topic, stage)
        if q is None:
            break

        intent_list = _ce19_build_intent(composer, q["question"])
        if intent_list is None:
            break
        produced_text = ""
        produced_conf = 0.0
        try:
            result = brain.produce_text(intent_list)
            if isinstance(result, dict):
                produced_text = result.get("text", "") or ""
                produced_conf = float(result.get("confidence", 0.0) or 0.0)
        except Exception as e:
            print(f"  [ClaudeTeacher:s{stage}] produce_text failed: {e}")
            continue

        s = score_brain_response(q["question"], produced_text,
                                 q["expected_concepts"])
        if s is None:
            break

        fb_path = "none"
        exemplar_text = None
        try:
            if s["composite"] >= TEACHER_PASS_THRESHOLD and produced_text.strip():
                brain.learn_language(produced_text)
                fb_path = "positive"
            else:
                exemplar_text = build_corrective_exemplar(
                    q["question"], produced_text, q["expected_concepts"])
                if exemplar_text and exemplar_text.strip():
                    try:
                        brain.train_language(exemplar_text, exemplar_text)
                    except TypeError:
                        brain.train_language(text=exemplar_text,
                                             target_text=exemplar_text)
                    fb_path = "exemplar"
                else:
                    fallback_q = q["question"]
                    try:
                        brain.train_language(fallback_q, fallback_q)
                    except TypeError:
                        brain.train_language(text=fallback_q,
                                             target_text=fallback_q)
                    fb_path = "question_anchor"
        except Exception as e:
            print(f"  [ClaudeTeacher:s{stage}] feedback signal failed: {e}")
            fb_path = "feedback_error"

        if r % max(1, int(log_every)) == 0:
            print(f"  [ClaudeTeacher:s{stage}] r={r} topic={topic!r} "
                  f"composite={s['composite']:.2f} "
                  f"fact={s['factuality']:.2f} coh={s['coherence']:.2f} "
                  f"top={s['on_topic']:.2f} fb={fb_path} "
                  f"calls_used={_CALL_COUNTER - calls_at_start}")

        rounds.append({
            "stage": int(stage),
            "round": r,
            "topic": topic,
            "question": q["question"],
            "expected_concepts": list(q["expected_concepts"]),
            "produced": produced_text,
            "confidence": produced_conf,
            "score": s,
            "feedback_path": fb_path,
            "exemplar": exemplar_text,
            "claude_calls_used": _CALL_COUNTER - calls_at_start,
        })

    if not rounds:
        return None
    return rounds
