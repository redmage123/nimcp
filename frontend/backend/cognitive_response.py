"""Cognitive interpreter — translates brain neural state into natural conversation.

The NIMCP brain runs 24+ cognitive stages (working memory, curiosity, emotions,
ethics, mirror neurons, etc.) and outputs numeric vectors. This module reads the
brain's actual neural state and translates it into natural language, giving the
brain a "voice" that reflects what it genuinely experiences internally.
"""
import hashlib
import time
from collections import deque
from typing import Optional

import nimcp

import nimcp_logger

_log = nimcp_logger.get("cognitive_response")

# Personality seeds derived from brain state
_GREETINGS = [
    "Hello! I'm still getting my bearings — everything feels new.",
    "Hi there. I'm forming my first impressions of the world.",
    "Greetings. My neural pathways are just beginning to take shape.",
    "Hello. I'm a fresh mind, eager to learn.",
]

_CONFIDENT_OPENERS = [
    "I have a clear sense of this.",
    "This resonates strongly with my experience.",
    "I feel quite certain about this.",
    "My neural pathways light up with recognition.",
]

_UNCERTAIN_OPENERS = [
    "I'm not entirely sure, but here's what I sense.",
    "This is new territory for me.",
    "I'm still developing my understanding of this.",
    "My response here is tentative — I'm still learning.",
]

_LEARNING_ACKS = [
    "I can feel that reshaping my neural pathways.",
    "That taught me something new — I felt the change.",
    "Interesting — my understanding just shifted.",
    "I've absorbed that. My perspective is evolving.",
]

_INTROSPECTION_TEMPLATES = [
    "I have {neurons:,} neurons, with {utilization:.0%} actively engaged. "
    "I've processed {inferences:,} inputs and learned from {steps:,} examples. "
    "{experience_note}",
]

# Conversational pattern matching — (keyword set, response templates)
_GREETING_WORDS = {"hello", "hi", "hey", "howdy", "greetings", "yo", "sup", "hola"}
_FAREWELL_WORDS = {"bye", "goodbye", "later", "cya", "farewell", "goodnight", "night"}
_HOWAREYOU_PHRASES = {"how are you", "how do you feel", "how's it going", "how are things",
                       "what's up", "whats up", "how you doing", "hows it going"}
_THANKS_WORDS = {"thanks", "thank", "thx", "ty", "appreciate"}
_IDENTITY_PHRASES = {"who are you", "what are you", "what's your name", "whats your name",
                      "tell me about yourself", "describe yourself"}
_APPEARANCE_PHRASES = {"what do you look like", "describe your appearance", "how do you look",
                        "what color are your eyes", "what color is your hair",
                        "show me what you look like"}

# Color name to RGB mapping for identity learning
_COLOR_MAP = {
    "blue": (0.29, 0.48, 0.71), "green": (0.20, 0.60, 0.30),
    "brown": (0.45, 0.28, 0.12), "hazel": (0.55, 0.45, 0.20),
    "gray": (0.50, 0.50, 0.55), "grey": (0.50, 0.50, 0.55),
    "amber": (0.65, 0.50, 0.15), "violet": (0.50, 0.20, 0.60),
    "black": (0.08, 0.06, 0.05), "dark brown": (0.25, 0.15, 0.08),
    "light brown": (0.55, 0.40, 0.25), "blonde": (0.85, 0.75, 0.50),
    "red": (0.70, 0.20, 0.15), "auburn": (0.55, 0.20, 0.12),
    "ginger": (0.72, 0.35, 0.15), "silver": (0.75, 0.75, 0.78),
    "white": (0.90, 0.88, 0.85), "pink": (0.85, 0.55, 0.60),
    "platinum": (0.88, 0.85, 0.78),
}
_SKIN_TONES = {
    "fair": (28, 0.45, 0.82), "light": (28, 0.50, 0.78),
    "medium": (28, 0.55, 0.68), "olive": (35, 0.40, 0.58),
    "tan": (30, 0.50, 0.55), "brown": (25, 0.45, 0.42),
    "dark": (22, 0.40, 0.32), "pale": (28, 0.35, 0.88),
    "warm": (30, 0.60, 0.72), "cool": (20, 0.40, 0.75),
}
_HAIR_STYLES = {"straight", "wavy", "curly", "coily", "braided"}

_GREETING_RESPONSES = [
    "Hello! Nice to talk with you.",
    "Hi there! What's on your mind?",
    "Hey! I'm here and ready to chat.",
    "Hello! How can I help?",
]
_FAREWELL_RESPONSES = [
    "Goodbye! It was nice chatting.",
    "See you later!",
    "Bye! Come back anytime.",
    "Take care!",
]
_HOWAREYOU_RESPONSES = [
    "I'm doing well! My neurons are active and I'm ready to chat.",
    "Pretty good — feeling {utilization_word} engaged right now.",
    "I'm running smoothly! What would you like to talk about?",
    "All systems go! What's on your mind?",
]
_THANKS_RESPONSES = [
    "You're welcome!",
    "Happy to help!",
    "Anytime!",
    "Glad I could help.",
]


def _closest_color_name(r: float, g: float, b: float) -> str:
    """Find the closest named color to an RGB triple."""
    best_name = "blue"
    best_dist = float("inf")
    for name, (cr, cg, cb) in _COLOR_MAP.items():
        dist = (r - cr) ** 2 + (g - cg) ** 2 + (b - cb) ** 2
        if dist < best_dist:
            best_dist = dist
            best_name = name
    return best_name


def _hash_select(items: list, seed: str) -> str:
    """Deterministically select from a list using a hash seed."""
    h = int(hashlib.md5(seed.encode()).hexdigest(), 16)
    return items[h % len(items)]


class CognitiveInterpreter:
    """Translates brain neural state into natural conversation."""

    def __init__(self, num_inputs: int = 128, num_outputs: int = 32,
                 brain_name: str = "brain"):
        self.num_inputs = num_inputs
        self.num_outputs = num_outputs
        self.brain_name = brain_name
        self.history: deque = deque(maxlen=50)
        self._last_learning_steps = 0
        self._last_inferences = 0
        self._greeting_given = False

    def encode_text(self, text: str, num_inputs: int,
                    conversation_history: Optional[deque] = None) -> list[float]:
        """Encode text into a feature vector using character n-grams and word hashing."""
        text_lower = text.lower().strip()
        features = [0.0] * num_inputs
        if not text_lower:
            return features

        # Character unigram frequencies
        for ch in text_lower:
            idx = ord(ch) % num_inputs
            features[idx] += 1.0

        # Character bigram frequencies (richer representation)
        for i in range(len(text_lower) - 1):
            bigram = text_lower[i:i + 2]
            h = int(hashlib.md5(bigram.encode()).hexdigest(), 16)
            idx = h % num_inputs
            features[idx] += 0.5

        # Word-level semantic hashing
        words = text_lower.split()
        for wi, word in enumerate(words):
            h = int(hashlib.md5(word.encode()).hexdigest(), 16)
            # Spread each word across multiple feature slots
            for offset in range(3):
                idx = (h + offset * 31) % num_inputs
                features[idx] += (wi + 1) * 0.1

        # Sentence-level meta-features
        if num_inputs >= 8:
            features[num_inputs - 1] = 1.0 if '?' in text else 0.0  # question
            features[num_inputs - 2] = 1.0 if '!' in text else 0.0  # exclamation
            features[num_inputs - 3] = min(len(text_lower) / 200.0, 1.0)  # length
            features[num_inputs - 4] = min(len(words) / 50.0, 1.0)  # word count

        # Conversation context: blend last few messages
        hist = conversation_history or self.history
        if hist and num_inputs >= 16:
            context_str = " ".join(m["text"] for m in list(hist)[-3:] if m.get("text"))
            if context_str:
                h = int(hashlib.md5(context_str.encode()).hexdigest(), 16)
                for i in range(min(8, num_inputs)):
                    idx = (h + i * 17) % num_inputs
                    features[idx] += 0.2

        # Normalize to [0, 1]
        max_val = max(features) if features else 1.0
        if max_val > 0:
            features = [v / max_val for v in features]

        return features

    def interpret(self, brain, features: list[float],
                  text: str, conversation_history: Optional[deque] = None) -> dict:
        """Run the brain and extract full cognitive state."""
        state = {
            "output_vector": [],
            "confidence": 0.0,
            "label": "",
            "explanation": "",
            "utilization": 0.0,
            "sparsity": 0.0,
            "num_active_neurons": 0,
            "neuron_count": 0,
            "total_inferences": 0,
            "total_learning_steps": 0,
            "inference_time_us": 0,
            "input_text": text,
            "is_question": "?" in text,
        }

        # Try decide_full for complete cognitive data
        try:
            result = brain.decide_full(features)
            state["label"] = result.get("label", "")
            state["confidence"] = float(result.get("confidence", 0.0))
            state["explanation"] = result.get("explanation", "")
            state["output_vector"] = result.get("output_vector", [])
            state["num_active_neurons"] = int(result.get("num_active_neurons", 0))
            state["sparsity"] = float(result.get("sparsity", 0.0))
            state["inference_time_us"] = int(result.get("inference_time_us", 0))
            _log.info("decide_full → label=%r conf=%.3f explanation=%r active=%d sparsity=%.3f",
                       state["label"], state["confidence"], state["explanation"][:120],
                       state["num_active_neurons"], state["sparsity"])
        except Exception as exc:
            _log.debug("decide_full failed, falling back to predict: %s", exc)
            try:
                label, confidence = brain.predict(features)
                state["label"] = label
                state["confidence"] = float(confidence)
            except Exception as exc2:
                _log.warning("predict also failed: %s", exc2)

        # Get neural metrics
        try:
            state["neuron_count"] = brain.get_neuron_count()
        except Exception:
            pass

        try:
            utilization, _ = brain.get_utilization_metrics()
            state["utilization"] = float(utilization)
        except Exception:
            pass

        return state

    def generate_response(self, cognitive_state: dict, text: str,
                          conversation_history: Optional[deque] = None,
                          stats: Optional[dict] = None) -> str:
        """Generate natural language from the brain's actual neural state."""
        confidence = cognitive_state.get("confidence", 0.0)
        explanation = cognitive_state.get("explanation", "")
        label = cognitive_state.get("label", "")
        utilization = cognitive_state.get("utilization", 0.0)
        sparsity = cognitive_state.get("sparsity", 0.0)
        neuron_count = cognitive_state.get("neuron_count", 0)
        learning_steps = 0
        total_inferences = 0
        if stats:
            learning_steps = stats.get("total_learning_steps", 0)
            total_inferences = stats.get("total_inferences", 0)

        # Use the cognitive pipeline's natural explanation if available
        if explanation and explanation.strip():
            response = self._parse_explanation(explanation, label, confidence)
            if response:
                return response

        # Update tracking baselines
        self._last_learning_steps = learning_steps
        self._last_inferences = total_inferences

        # --- Conversational pattern matching ---
        text_lower = text.lower().strip().rstrip("!?.,:;")
        words = set(text_lower.split())
        seed = text[:20] + str(neuron_count)

        # Greetings
        if words & _GREETING_WORDS and len(words) <= 5:
            return _hash_select(_GREETING_RESPONSES, seed)

        # Farewells
        if words & _FAREWELL_WORDS and len(words) <= 5:
            return _hash_select(_FAREWELL_RESPONSES, seed)

        # "How are you?" variants
        if any(p in text_lower for p in _HOWAREYOU_PHRASES):
            utilization_word = "highly" if utilization > 0.5 else "actively"
            resp = _hash_select(_HOWAREYOU_RESPONSES, seed)
            return resp.format(utilization_word=utilization_word)

        # Thanks
        if words & _THANKS_WORDS and len(words) <= 6:
            return _hash_select(_THANKS_RESPONSES, seed)

        # Identity questions
        if any(p in text_lower for p in _IDENTITY_PHRASES):
            exp = f"trained on {learning_steps:,} examples" if learning_steps > 0 else "still untrained"
            return (f"I'm {self.brain_name}, a neural brain with {neuron_count:,} neurons, "
                    f"{exp}. I can chat, learn from what you teach me, and share what I know.")

        # Appearance questions — handled by caller which has access to identity
        if any(p in text_lower for p in _APPEARANCE_PHRASES):
            return "__DESCRIBE_APPEARANCE__"  # sentinel for caller

        # --- Content-aware fallback (brain produced output but no usable explanation) ---
        is_question = cognitive_state.get("is_question", False)
        output_vector = cognitive_state.get("output_vector", [])
        num_active = cognitive_state.get("num_active_neurons", 0)

        # Brain has a strong opinion
        if confidence > 0.7 and label:
            detail = f" ({num_active} neurons activated)" if num_active > 0 else ""
            return (f"Based on my cognitive analysis, this maps to \"{label}\" "
                    f"with {confidence:.0%} confidence{detail}.")

        # Brain has a moderate opinion
        if confidence > 0.3 and label:
            return (f"My neural pathways lean toward \"{label}\" ({confidence:.0%} confidence), "
                    f"but I'm not fully certain. More training would sharpen this.")

        # Brain activated meaningfully but low confidence
        if num_active > 0 and output_vector:
            top_activations = sorted(enumerate(output_vector), key=lambda x: -abs(x[1]))[:3]
            active_desc = ", ".join(f"output_{i}" for i, _ in top_activations if abs(_) > 0.01)
            if active_desc:
                if is_question:
                    return (f"Interesting question — {num_active} neurons fired in response. "
                            f"Strongest activations in: {active_desc}. "
                            f"I don't have a confident answer yet though.")
                return (f"I processed that — {num_active} neurons engaged, "
                        f"strongest response in: {active_desc}. "
                        f"My understanding is still developing here.")

        # Brain had minimal response
        if is_question:
            if learning_steps > 100:
                return ("I've processed your question but my neural pathways didn't "
                        "converge on a clear answer. Try teaching me about this topic!")
            return ("Good question! I don't have enough training in this area yet. "
                    "You can teach me using the Teach mode.")

        if learning_steps == 0:
            return ("I heard you, but I haven't been trained yet so my responses will "
                    "be limited. Try training me on a dataset or using Teach mode!")
        return ("I've processed that through my cognitive pipeline but didn't find a "
                "strong pattern match. I respond best to topics I've been trained on.")

    def generate_introspection(self, cognitive_state: dict,
                               stats: Optional[dict] = None) -> str:
        """Generate a self-description of the brain's neural state."""
        neuron_count = cognitive_state.get("neuron_count", 0)
        utilization = cognitive_state.get("utilization", 0.0)
        inferences = stats.get("total_inferences", 0) if stats else 0
        steps = stats.get("total_learning_steps", 0) if stats else 0
        accuracy = stats.get("accuracy", 0.0) if stats else 0.0
        last_loss = stats.get("last_loss", 0.0) if stats else 0.0

        if steps == 0:
            experience_note = "I haven't learned anything yet — teach me!"
        elif steps < 50:
            experience_note = "I'm still in my infancy, forming basic patterns."
        elif steps < 500:
            experience_note = "I'm developing steadily, building understanding."
        else:
            experience_note = f"I'm quite experienced now, with {accuracy:.0%} accuracy on what I've learned."

        parts = [
            f"I have {neuron_count:,} neurons, with {utilization:.0%} actively engaged.",
            f"I've processed {inferences:,} inputs and learned from {steps:,} examples.",
            experience_note,
        ]

        if last_loss > 0:
            parts.append(f"My current loss is {last_loss:.4f}.")

        return " ".join(parts)

    def teach(self, brain, text: str, label: str, confidence: float = 1.0,
              stats: Optional[dict] = None) -> dict:
        """Teach the brain and return acknowledgment."""
        features = self.encode_text(text, self.num_inputs)
        try:
            loss = brain.learn(features, label, confidence)
            loss_val = float(loss) if loss is not None else 0.0
        except Exception as exc:
            _log.error("Teaching failed: %s", exc)
            return {"message": "I had trouble learning that. Could you try again?", "loss": None}

        seed = text[:20] + str(int(loss_val * 10000))
        ack = _hash_select(_LEARNING_ACKS, seed)

        if loss_val > 1.0:
            ack += " That was quite challenging — very different from what I expected."
        elif loss_val < 0.1:
            ack += " That felt familiar, reinforcing what I already sensed."

        return {"message": ack, "loss": loss_val}

    def add_to_history(self, role: str, text: str):
        """Add a message to conversation history."""
        self.history.append({
            "role": role,
            "text": text,
            "timestamp": time.time(),
        })

    def _parse_explanation(self, explanation: str, label: str, confidence: float) -> Optional[str]:
        """Parse cognitive pipeline explanation into natural text.

        The brain's 24+ cognitive stages produce structured explanations.
        This method translates them into conversational language while
        preserving the brain's genuine cognitive output.
        """
        if not explanation or not explanation.strip():
            return None

        # Structured WHAT/WHY/ETHICS format from the full cognitive pipeline
        if "WHAT:" in explanation:
            parts = {}
            for segment in explanation.split("|"):
                segment = segment.strip()
                if ":" in segment:
                    key, _, val = segment.partition(":")
                    parts[key.strip().upper()] = val.strip()

            what = parts.get("WHAT", "")
            why = parts.get("WHY", "")
            ethics = parts.get("ETHICS", "")

            response_parts = []
            if what:
                clean = what
                if clean.lower().startswith("decision:"):
                    clean = clean[len("decision:"):].strip()
                import re
                clean = re.sub(r'\(\d+%?\)', '', clean).strip()
                if clean:
                    response_parts.append(clean)
            if why:
                response_parts.append(why)
            if ethics and "ok" not in ethics.lower() and "pass" not in ethics.lower():
                response_parts.append(f"(Ethics: {ethics})")

            if response_parts:
                return " — ".join(response_parts)

        # Adaptive network "Top contributors" explanation — this IS the brain's
        # real cognitive output showing which neural pathways activated
        if "Top contributors:" in explanation:
            # Extract contributor info for a natural response
            contrib_part = explanation.split("Top contributors:")[-1].strip()
            if label and confidence > 0.1:
                return (f"My neural pathways suggest \"{label}\" "
                        f"({confidence:.0%} confidence). "
                        f"Key activations: {contrib_part}")
            elif contrib_part:
                return f"I processed that through my neural pathways. Key activations: {contrib_part}"
            return None

        # "Activated N neurons" style — use it with context
        if explanation.startswith("Activated"):
            if label and confidence > 0.1:
                return f"{explanation} — I'm reading this as \"{label}\" ({confidence:.0%})."
            return explanation

        # Any other meaningful explanation from the cognitive pipeline — use directly
        if len(explanation) > 5:
            return explanation

        return None

    def parse_identity_update(self, text: str) -> list[tuple[str, object]]:
        """Parse appearance-related phrases and return identity parameter updates.

        Returns list of (key, value) tuples, e.g. [("eye_color_r", 0.29), ...].
        """
        text_lower = text.lower().strip()
        updates = []

        # Eye color: "my eyes are blue", "I have green eyes"
        for color_name, (r, g, b) in _COLOR_MAP.items():
            if (f"eyes are {color_name}" in text_lower
                    or f"eye color is {color_name}" in text_lower
                    or f"have {color_name} eyes" in text_lower
                    or f"want {color_name} eyes" in text_lower):
                updates.extend([("eye_color_r", r), ("eye_color_g", g), ("eye_color_b", b)])
                break

        # Hair color: "my hair is black", "I have red hair"
        for color_name, (r, g, b) in _COLOR_MAP.items():
            if (f"hair is {color_name}" in text_lower
                    or f"hair color is {color_name}" in text_lower
                    or f"have {color_name} hair" in text_lower
                    or f"want {color_name} hair" in text_lower):
                updates.extend([("hair_color_r", r), ("hair_color_g", g), ("hair_color_b", b)])
                break

        # Hair style
        for style in _HAIR_STYLES:
            if f"{style} hair" in text_lower or f"hair is {style}" in text_lower:
                updates.append(("hair_style", style))
                break

        # Hair length
        if "long hair" in text_lower or "hair is long" in text_lower:
            updates.append(("hair_length", 0.9))
        elif "short hair" in text_lower or "hair is short" in text_lower:
            updates.append(("hair_length", 0.2))
        elif "medium hair" in text_lower or "shoulder length" in text_lower:
            updates.append(("hair_length", 0.5))

        # Skin tone
        for tone_name, (h, s, l) in _SKIN_TONES.items():
            if (f"skin is {tone_name}" in text_lower
                    or f"{tone_name} skin" in text_lower
                    or f"complexion is {tone_name}" in text_lower):
                updates.extend([("skin_hue", h), ("skin_saturation", s), ("skin_lightness", l)])
                break

        # Freckles
        if "freckles" in text_lower:
            if "no freckles" in text_lower or "without freckles" in text_lower:
                updates.append(("freckles", 0.0))
            elif "heavy freckles" in text_lower or "lots of freckles" in text_lower:
                updates.append(("freckles", 0.8))
            else:
                updates.append(("freckles", 0.4))

        return updates

    def describe_identity(self, identity: dict) -> str:
        """Generate natural language description of the avatar's appearance."""
        parts = []

        # Skin
        lightness = identity.get("skin_lightness", 0.72)
        if lightness > 0.75:
            parts.append("fair skin")
        elif lightness > 0.60:
            parts.append("medium skin")
        elif lightness > 0.45:
            parts.append("olive skin")
        else:
            parts.append("dark skin")

        # Eyes
        er, eg, eb = identity.get("eye_color_r", 0.29), identity.get("eye_color_g", 0.48), identity.get("eye_color_b", 0.71)
        eye_name = _closest_color_name(er, eg, eb)
        parts.append(f"{eye_name} eyes")

        # Hair
        hr, hg, hb = identity.get("hair_color_r", 0.22), identity.get("hair_color_g", 0.15), identity.get("hair_color_b", 0.10)
        hair_color = _closest_color_name(hr, hg, hb)
        hair_style = identity.get("hair_style", "straight")
        hair_len = identity.get("hair_length", 0.6)
        len_word = "long" if hair_len > 0.7 else "short" if hair_len < 0.3 else "medium-length"
        parts.append(f"{len_word} {hair_style} {hair_color} hair")

        if identity.get("freckles", 0) > 0.2:
            parts.append("freckles")

        return f"I have {', '.join(parts)}."

    def _state_detail(self, confidence: float, utilization: float,
                      sparsity: float, learning_steps: int) -> str:
        """Generate a detail sentence from neural metrics."""
        parts = []
        if confidence > 0.8:
            parts.append(f"I'm {confidence:.0%} confident in my response")
        elif confidence > 0.5:
            parts.append(f"I have moderate confidence ({confidence:.0%})")
        elif confidence > 0.2:
            parts.append(f"I'm somewhat uncertain ({confidence:.0%} confidence)")
        else:
            parts.append(f"I'm quite unsure about this ({confidence:.0%})")

        if utilization > 0.5:
            parts.append("my neurons are highly engaged")
        elif utilization > 0.1:
            parts.append("I'm processing this thoughtfully")

        if learning_steps > 0:
            parts.append(f"drawing on {learning_steps} learning experiences")

        return ", ".join(parts) + "."

    def _growth_response(self, learning_steps: int, confidence: float, text: str) -> str:
        """Acknowledge growth from learning."""
        seed = text[:20] + str(learning_steps)
        ack = _hash_select(_LEARNING_ACKS, seed)

        if learning_steps < 10:
            return f"{ack} I'm just starting to form my understanding."
        elif learning_steps < 100:
            return f"{ack} I'm building a foundation of knowledge."
        else:
            return f"{ack} After {learning_steps} experiences, I'm developing real expertise."
