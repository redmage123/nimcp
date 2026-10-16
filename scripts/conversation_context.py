"""Conversation Context Engine — Phase 3 of the Communication Layer.

Tracks multi-turn conversation state: topic threads, follow-ups, topic shifts,
emotional/confidence trajectories, and reference resolution. Provides context
signals to the ResponseComposer so it can generate contextually appropriate
responses ("As I mentioned...", "Going back to...", "Yes, and...").

Usage:
    from conversation_context import ConversationContext
    ctx = ConversationContext()
    signals = ctx.process_turn(user_input, transcript, decision)
    response = composer.compose(transcript, decision, user_input,
                                vocab_response, context=signals)
    ctx.record_response(response)
"""

import math
import re
import time
from collections import Counter
from typing import Optional


# Stop words to ignore when extracting topic keywords
_STOP_WORDS = frozenset({
    "i", "me", "my", "we", "you", "your", "he", "she", "it", "they", "them",
    "a", "an", "the", "this", "that", "these", "those",
    "is", "am", "are", "was", "were", "be", "been", "being",
    "do", "does", "did", "have", "has", "had", "will", "would", "shall",
    "should", "can", "could", "may", "might", "must",
    "not", "no", "nor", "and", "or", "but", "if", "then", "so", "because",
    "of", "in", "on", "at", "to", "for", "with", "by", "from", "about",
    "into", "through", "during", "before", "after", "above", "below",
    "up", "down", "out", "off", "over", "under", "again", "further",
    "what", "which", "who", "whom", "when", "where", "why", "how",
    "all", "each", "every", "both", "few", "more", "most", "other",
    "some", "such", "than", "too", "very", "just", "also", "now",
    "here", "there", "well", "really", "quite", "still",
    "tell", "say", "said", "think", "know", "like", "want", "need",
    "get", "got", "go", "going", "come", "make", "take", "see", "look",
    "let", "please", "hi", "hello", "hey", "ok", "okay", "yes", "yeah", "no",
})

# Pronouns that might refer to prior topics
_ANAPHORIC = frozenset({
    "it", "that", "this", "they", "them", "those", "these",
    "he", "she", "its", "their", "his", "her",
})

# Follow-up indicators
_FOLLOWUP_PHRASES = [
    r"\bwhat about\b", r"\bhow about\b", r"\band what\b",
    r"\bwhat else\b", r"\btell me more\b", r"\bmore about\b",
    r"\bcan you explain\b", r"\bwhy is that\b", r"\bwhat do you mean\b",
    r"\belaborate\b", r"\bgo on\b", r"\bcontinue\b",
    r"\byes but\b", r"\byes and\b", r"\bbut what\b", r"\bbut how\b",
    r"\bbut why\b", r"\balso\b",
    r"^and\b", r"^but\b", r"^so\b",  # sentence-initial connectors
    r"\band how\b", r"\band why\b", r"\band when\b",
]

# Topic shift indicators
_SHIFT_PHRASES = [
    r"\bchanging (the )?topic\b", r"\bsomething else\b",
    r"\bdifferent question\b", r"\bnew topic\b",
    r"\bby the way\b", r"\banyway\b", r"\bmoving on\b",
    r"\blet'?s talk about\b", r"\bwhat about (?!that|this|it)\b",
    r"\bforget that\b", r"\bnever ?mind\b",
]


def _fuzzy_word_match(word: str, candidates: set, threshold: float = 0.65) -> str:
    """Find the best fuzzy match for a misspelled word among candidates.

    Uses two complementary approaches:
    1. Character-set (fuzzy Jaccard) — catches visual/keyboard typos
    2. Positional character similarity — catches transpositions

    Returns the best matching candidate, or None if below threshold.
    """
    if len(word) < 3:
        return None

    best_match = None
    best_score = threshold

    for candidate in candidates:
        if len(candidate) < 3:
            continue

        # Quick length filter
        len_ratio = min(len(word), len(candidate)) / max(len(word), len(candidate))
        if len_ratio < 0.6:
            continue

        # Character frequency similarity (fuzzy Jaccard)
        freq_w = Counter(word)
        freq_c = Counter(candidate)
        all_chars = set(freq_w) | set(freq_c)
        sum_min = sum(min(freq_w.get(ch, 0), freq_c.get(ch, 0)) for ch in all_chars)
        sum_max = sum(max(freq_w.get(ch, 0), freq_c.get(ch, 0)) for ch in all_chars)
        charset_sim = (sum_min / sum_max * (0.5 + 0.5 * len_ratio)) if sum_max else 0

        # Positional match (sliding window — same as C phonological approach)
        pos_matches = 0
        for i, ch in enumerate(word):
            center = int(i * len(candidate) / len(word))
            window = candidate[max(0, center - 2):center + 3]
            if ch in window:
                pos_matches += 1
        positional_sim = (pos_matches / len(word) * len_ratio) if word else 0

        score = max(charset_sim, positional_sim)
        if score > best_score:
            best_score = score
            best_match = candidate

    return best_match


class TopicThread:
    """A thread of related conversation turns on a topic."""

    __slots__ = ("keywords", "first_turn", "last_turn", "turn_count",
                 "avg_confidence", "avg_salience", "dominant_module")

    def __init__(self, keywords: set, turn_idx: int):
        self.keywords = keywords
        self.first_turn = turn_idx
        self.last_turn = turn_idx
        self.turn_count = 1
        self.avg_confidence = 0.0
        self.avg_salience = 0.0
        self.dominant_module = ""

    def overlap(self, other_keywords: set) -> float:
        """Jaccard similarity between keyword sets."""
        if not self.keywords or not other_keywords:
            return 0.0
        intersection = self.keywords & other_keywords
        union = self.keywords | other_keywords
        return len(intersection) / len(union) if union else 0.0

    def merge(self, keywords: set, turn_idx: int, confidence: float,
              salience: float, module: str):
        """Absorb a new turn into this thread."""
        self.keywords |= keywords
        self.last_turn = turn_idx
        n = self.turn_count
        self.avg_confidence = (self.avg_confidence * n + confidence) / (n + 1)
        self.avg_salience = (self.avg_salience * n + salience) / (n + 1)
        self.turn_count += 1
        if module:
            self.dominant_module = module


class ContextSignals:
    """Signals provided to the ResponseComposer for context-aware generation."""

    __slots__ = (
        "is_followup", "is_topic_shift", "is_returning_topic",
        "is_greeting", "is_farewell", "is_question",
        "turn_index", "topic_keywords", "prior_topic_keywords",
        "confidence_trend", "emotional_trend",
        "prior_response", "prior_user_input",
        "topic_depth", "conversation_phase",
        "referenced_topic",
    )

    def __init__(self):
        self.is_followup = False
        self.is_topic_shift = False
        self.is_returning_topic = False
        self.is_greeting = False
        self.is_farewell = False
        self.is_question = False
        self.turn_index = 0
        self.topic_keywords = set()
        self.prior_topic_keywords = set()
        self.confidence_trend = 0.0      # positive = improving, negative = declining
        self.emotional_trend = 0.0       # positive = warming, negative = cooling
        self.prior_response = ""
        self.prior_user_input = ""
        self.topic_depth = 0             # how many turns on current topic
        self.conversation_phase = "open" # open, exploring, deepening, closing
        self.referenced_topic = None     # TopicThread if returning to prior topic


class ConversationContext:
    """Multi-turn conversation context engine.

    Tracks:
    - Topic threads (keyword overlap + similarity)
    - Follow-up vs topic shift detection
    - Confidence/emotional trajectory across turns
    - Conversation phase (opening, exploring, deepening, closing)
    - Anaphoric reference resolution (what does "it" refer to?)
    """

    def __init__(self, max_turns: int = 50, topic_threshold: float = 0.25):
        self.max_turns = max_turns
        self.topic_threshold = topic_threshold

        # Turn history
        self.turns = []  # list of dicts

        # Topic threads
        self.threads = []          # list of TopicThread
        self.active_thread = None  # current TopicThread
        self.turn_index = 0

        # Trajectory tracking
        self._confidences = []
        self._valences = []

    def process_turn(self, user_input: str, transcript: list,
                     decision: dict) -> ContextSignals:
        """Process a new user turn and return context signals.

        Call this BEFORE composing the response. Then call record_response()
        after the response is generated.
        """
        signals = ContextSignals()
        signals.turn_index = self.turn_index

        # Extract keywords from user input
        keywords = self._extract_keywords(user_input)
        signals.topic_keywords = keywords

        # Fuzzy-correct keywords against known topic vocabulary
        keywords = self._correct_keywords(keywords)
        signals.topic_keywords = keywords

        # Classify input type
        signals.is_greeting = self._is_greeting(user_input)
        signals.is_farewell = self._is_farewell(user_input)
        signals.is_question = self._is_question(user_input)

        # Get prior context
        if self.turns:
            last = self.turns[-1]
            signals.prior_response = last.get("response", "")
            signals.prior_user_input = last.get("user_input", "")
            signals.prior_topic_keywords = last.get("keywords", set())

        # Detect follow-up vs topic shift
        signals.is_followup = self._is_followup(user_input, keywords)
        signals.is_topic_shift = self._is_topic_shift(user_input, keywords)

        # Resolve followup vs topic-shift conflicts:
        # Explicit followup ("tell me more", "and how...") beats implicit shift
        # Explicit shift ("something else", "new topic") beats implicit followup
        if signals.is_followup and signals.is_topic_shift:
            explicit_followup = self._is_followup_explicit(user_input)
            explicit_shift = self._is_topic_shift_explicit(user_input)
            if explicit_followup and not explicit_shift:
                signals.is_topic_shift = False
            else:
                signals.is_followup = False

        # Check for returning to a previous topic (can happen with or without
        # explicit topic shift — e.g., "going back to calculus")
        if self.threads and len(self.threads) > 1:
            for thread in reversed(self.threads):
                if thread is self.active_thread:
                    continue
                if thread.overlap(keywords) > self.topic_threshold:
                    signals.is_returning_topic = True
                    signals.is_topic_shift = False  # it's a return, not a shift
                    signals.is_followup = False
                    signals.referenced_topic = thread
                    break

        # Update topic threads
        confidence = decision.get("confidence", 0.0)
        top_salience = 0.0
        dominant_mod = ""
        if transcript:
            best = max(transcript, key=lambda e: e.get("salience", 0))
            top_salience = best.get("salience", 0.0)
            dominant_mod = best.get("module", "")

        self._update_threads(keywords, confidence, top_salience, dominant_mod,
                             signals)

        # Topic depth
        if self.active_thread:
            signals.topic_depth = self.active_thread.turn_count

        # Trajectory
        self._confidences.append(confidence)
        valence = self._extract_valence(transcript)
        self._valences.append(valence)
        signals.confidence_trend = self._compute_trend(self._confidences)
        signals.emotional_trend = self._compute_trend(self._valences)

        # Conversation phase
        signals.conversation_phase = self._determine_phase(signals)

        # Store turn (response filled in by record_response)
        self.turns.append({
            "user_input": user_input,
            "keywords": keywords,
            "confidence": confidence,
            "top_salience": top_salience,
            "dominant_module": dominant_mod,
            "response": "",
            "time": time.time(),
        })
        if len(self.turns) > self.max_turns:
            self.turns = self.turns[-self.max_turns:]

        self.turn_index += 1
        return signals

    def record_response(self, response: str):
        """Record the generated response for the current turn."""
        if self.turns:
            self.turns[-1]["response"] = response or ""

    def get_summary(self) -> dict:
        """Get a summary of conversation state for debugging."""
        return {
            "turns": self.turn_index,
            "active_topic": (list(self.active_thread.keywords)[:5]
                             if self.active_thread else []),
            "threads": len(self.threads),
            "confidence_trend": (self._compute_trend(self._confidences)
                                 if self._confidences else 0.0),
        }

    # ------------------------------------------------------------------
    # Keyword extraction
    # ------------------------------------------------------------------

    def _extract_keywords(self, text: str) -> set:
        """Extract meaningful keywords from text."""
        words = re.findall(r"[a-zA-Z']+", text.lower())
        keywords = set()
        for w in words:
            w = w.strip("'")
            if len(w) > 2 and w not in _STOP_WORDS:
                keywords.add(w)
        return keywords

    # ------------------------------------------------------------------
    # Input classification
    # ------------------------------------------------------------------

    def _is_greeting(self, text: str) -> bool:
        t = text.lower().strip().rstrip("!.")
        return t in {"hi", "hello", "hey", "greetings", "good morning",
                     "good afternoon", "good evening", "howdy", "sup",
                     "what's up", "yo"}

    def _is_farewell(self, text: str) -> bool:
        t = text.lower().strip().rstrip("!.")
        return t in {"bye", "goodbye", "good bye", "see you", "later",
                     "farewell", "goodnight", "good night", "take care",
                     "quit", "exit"}

    def _is_question(self, text: str) -> bool:
        t = text.strip()
        if t.endswith("?"):
            return True
        t_lower = t.lower()
        return any(t_lower.startswith(w) for w in
                   ("what ", "why ", "how ", "when ", "where ", "who ",
                    "which ", "is ", "are ", "do ", "does ", "can ",
                    "could ", "would ", "should ", "will "))

    def _is_followup(self, text: str, keywords: set) -> bool:
        """Detect if this is a follow-up to the previous turn.

        Returns True for explicit follow-up phrases, anaphoric references,
        or high keyword overlap with previous turn.
        """
        t_lower = text.lower()

        # Explicit follow-up phrases
        for pattern in _FOLLOWUP_PHRASES:
            if re.search(pattern, t_lower):
                return True

        # Anaphoric references with few new keywords suggest follow-up
        words = set(re.findall(r"[a-z']+", t_lower))
        has_anaphoric = bool(words & _ANAPHORIC)
        if has_anaphoric and len(keywords) <= 2:
            return True

        # High keyword overlap with previous turn
        if self.turns:
            prior_kw = self.turns[-1].get("keywords", set())
            if prior_kw and keywords:
                overlap = len(keywords & prior_kw) / max(len(keywords), 1)
                if overlap > 0.5:
                    return True

        return False

    def _is_followup_explicit(self, text: str) -> bool:
        """Check if follow-up is from an explicit phrase (not just overlap)."""
        t_lower = text.lower()
        for pattern in _FOLLOWUP_PHRASES:
            if re.search(pattern, t_lower):
                return True
        return False

    def _is_topic_shift(self, text: str, keywords: set) -> bool:
        """Detect if this is a topic shift."""
        t_lower = text.lower()

        # Explicit shift phrases
        for pattern in _SHIFT_PHRASES:
            if re.search(pattern, t_lower):
                return True

        # Low keyword overlap with recent turns = implicit shift
        if self.turns and keywords:
            recent_kw = set()
            for turn in self.turns[-3:]:
                recent_kw |= turn.get("keywords", set())
            if recent_kw:
                overlap = len(keywords & recent_kw) / max(len(keywords), 1)
                if overlap < 0.1 and len(keywords) >= 2:
                    return True

        return False

    def _is_topic_shift_explicit(self, text: str) -> bool:
        """Check if topic shift is from an explicit phrase (not just low overlap)."""
        t_lower = text.lower()
        for pattern in _SHIFT_PHRASES:
            if re.search(pattern, t_lower):
                return True
        return False

    # ------------------------------------------------------------------
    # Topic thread management
    # ------------------------------------------------------------------

    def _correct_keywords(self, keywords: set) -> set:
        """Fuzzy-correct keywords against known topic vocabulary.

        If a keyword doesn't match any existing topic word but fuzzy-matches
        one, replace it. This lets "mathmatics" match the "mathematics" thread.
        """
        if not self.threads:
            return keywords

        # Build vocabulary of all known topic keywords
        known = set()
        for thread in self.threads:
            known |= thread.keywords

        corrected = set()
        for kw in keywords:
            if kw in known:
                corrected.add(kw)
            else:
                match = _fuzzy_word_match(kw, known)
                if match:
                    corrected.add(match)  # Use the corrected form
                    corrected.add(kw)     # Keep original too
                else:
                    corrected.add(kw)

        return corrected

    def _update_threads(self, keywords: set, confidence: float,
                        salience: float, module: str,
                        signals: ContextSignals):
        """Update topic threads based on current turn keywords."""
        if not keywords:
            return

        # If returning to a previous topic, reactivate that thread
        if signals.is_returning_topic and signals.referenced_topic:
            self.active_thread = signals.referenced_topic
            self.active_thread.merge(keywords, self.turn_index,
                                     confidence, salience, module)
            return

        # Check if current keywords fit the active thread
        if self.active_thread:
            overlap = self.active_thread.overlap(keywords)
            if overlap >= self.topic_threshold or signals.is_followup:
                self.active_thread.merge(keywords, self.turn_index,
                                         confidence, salience, module)
                return

        # Check all threads for a match
        best_thread = None
        best_overlap = 0.0
        for thread in self.threads:
            o = thread.overlap(keywords)
            if o > best_overlap:
                best_overlap = o
                best_thread = thread

        if best_thread and best_overlap >= self.topic_threshold:
            self.active_thread = best_thread
            best_thread.merge(keywords, self.turn_index,
                              confidence, salience, module)
        else:
            # New topic thread
            thread = TopicThread(keywords.copy(), self.turn_index)
            thread.avg_confidence = confidence
            thread.avg_salience = salience
            thread.dominant_module = module
            self.threads.append(thread)
            self.active_thread = thread

    # ------------------------------------------------------------------
    # Trajectory analysis
    # ------------------------------------------------------------------

    def _extract_valence(self, transcript: list) -> float:
        """Extract emotional valence from transcript."""
        for entry in (transcript or []):
            if entry.get("module") == "emotion":
                summary = entry.get("summary", "")
                match = re.search(r'valence[=:]\s*([-\d.]+)',
                                  summary, re.IGNORECASE)
                if match:
                    try:
                        return float(match.group(1))
                    except ValueError:
                        pass
        return 0.0

    def _compute_trend(self, values: list, window: int = 5) -> float:
        """Compute trend direction from recent values.

        Returns positive for upward trend, negative for downward.
        Magnitude roughly in [-1, 1].
        """
        if len(values) < 2:
            return 0.0
        recent = values[-window:]
        n = len(recent)
        if n < 2:
            return 0.0
        # Simple linear regression slope
        x_mean = (n - 1) / 2.0
        y_mean = sum(recent) / n
        num = sum((i - x_mean) * (v - y_mean) for i, v in enumerate(recent))
        den = sum((i - x_mean) ** 2 for i in range(n))
        if den < 1e-8:
            return 0.0
        slope = num / den
        # Normalize: slope of 0.1 per turn is "moderate"
        return max(-1.0, min(1.0, slope * 5.0))

    # ------------------------------------------------------------------
    # Conversation phase
    # ------------------------------------------------------------------

    def _determine_phase(self, signals: ContextSignals) -> str:
        """Determine the current conversation phase."""
        if signals.is_greeting:
            return "greeting"
        if signals.is_farewell:
            return "closing"
        if self.turn_index == 0:
            return "opening"
        if signals.topic_depth >= 3:
            return "deepening"
        if signals.is_topic_shift:
            return "shifting"
        return "exploring"
