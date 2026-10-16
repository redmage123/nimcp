"""Response Composer — Phase 2+3 of the Communication Layer.

Transforms cognitive transcript entries into multi-sentence natural language
responses. Instead of decoding the brain's output vector (which may be
undertrained), the composer synthesizes from the cognitive modules that fired
during brain_decide() — reasoning chains, emotional state, prediction errors,
ethical concerns, curiosity, knowledge retrieval, etc.

Phase 3 adds conversation context awareness: follow-up phrasing, topic
continuity, returning-topic references, greeting/farewell handling,
confidence trajectory commentary, and conversation phase adaptation.

This means even an infant brain can produce meaningful responses, because the
cognitive modules operate on their own logic independent of output vector quality.

Usage:
    from response_composer import ResponseComposer
    from conversation_context import ConversationContext
    composer = ResponseComposer()
    ctx_engine = ConversationContext()
    signals = ctx_engine.process_turn(user_input, transcript, decision)
    response = composer.compose(transcript, decision, user_input,
                                vocab_response, context=signals)
    ctx_engine.record_response(response)
"""

import re
from typing import Optional


class ResponseComposer:
    """Compose natural language responses from cognitive transcript entries.

    The composer works in three stages:
    1. Rank transcript entries by salience
    2. Select top entries and map to sentence fragments via module-specific templates
    3. Assemble fragments into a coherent multi-sentence response
    """

    # Module-specific response templates.
    # Each template set is keyed by module name and contains:
    #   - patterns: list of (condition_fn, template_str) pairs
    #   - condition_fn receives the transcript entry dict
    #   - template_str uses {field} placeholders filled from entry
    TEMPLATES = {
        "reasoning": [
            (lambda e: e.get("confidence", 0) > 0.5,
             "I think this involves {summary_short}"),
            (lambda e: True,
             "My reasoning suggests {summary_short}"),
        ],
        "emotion": [
            (lambda e: _val(e, "valence") > 0.5,
             "I feel good about this"),
            (lambda e: _val(e, "valence") < -0.5,
             "Something about this concerns me"),
            (lambda e: _val(e, "arousal") > 0.7,
             "This really captures my attention"),
            (lambda e: True, None),  # Skip mild emotions
        ],
        "ethics": [
            (lambda e: "blocked" in e.get("summary", "").lower(),
             "I have an ethical concern about this"),
            (lambda e: e.get("confidence", 1.0) < 0.5,
             "I want to consider the ethical implications"),
            (lambda e: True, None),  # Skip if ethics just passed normally
        ],
        "epistemic": [
            (lambda e: "bias" in e.get("summary", "").lower(),
             "I should be careful — I may have a bias here"),
            (lambda e: e.get("confidence", 1.0) < 0.3,
             "I'm not very confident about this"),
            (lambda e: True, None),
        ],
        "predictive": [
            (lambda e: _val(e, "error") > 0.5,
             "This is surprising — it doesn't match my expectations"),
            (lambda e: _val(e, "error") > 0.2,
             "This is somewhat unexpected"),
            (lambda e: True, None),
        ],
        "curiosity": [
            (lambda e: _val(e, "drive") > 0.6,
             "I'm very curious about this"),
            (lambda e: _val(e, "novelty") > 0.5,
             "This feels new to me"),
            (lambda e: _val(e, "drive") > 0.3,
             "I find this interesting"),
            (lambda e: True, None),
        ],
        "engram": [
            (lambda e: e.get("confidence", 0) > 0.5,
             "I recall something related to this"),
            (lambda e: e.get("confidence", 0) > 0.2,
             "This reminds me of something"),
            (lambda e: True, None),
        ],
        "knowledge": [
            (lambda e: e.get("salience", 0) > 0.3,
             "I have some knowledge about this"),
            (lambda e: True, None),
        ],
        "grounded_language": [
            (lambda e: e.get("confidence", 0) > 0.5,
             "I understand the words you're using"),
            (lambda e: True, None),
        ],
        "fep_parietal": [
            (lambda e: e.get("salience", 0) > 0.5,
             "I'm actively updating my understanding"),
            (lambda e: True, None),
        ],
        "predictive_hierarchy": [
            (lambda e: _val(e, "loss") < 0.3,
             "My predictions are aligning well"),
            (lambda e: _val(e, "loss") > 0.7,
             "I'm still learning the patterns here"),
            (lambda e: True, None),
        ],
        "vae": [
            (lambda e: "ANOMALY" in e.get("summary", ""),
             "This input seems unusual to me"),
            (lambda e: True, None),
        ],
        "jepa": [
            (lambda e: _val(e, "loss") < 0.3,
             "I can predict what comes next here"),
            (lambda e: True, None),
        ],
        "creative": [
            (lambda e: e.get("salience", 0) > 0.4,
             "I have a creative insight about this"),
            (lambda e: True, None),
        ],
        "intuition": [
            (lambda e: e.get("salience", 0) > 0.4,
             "My intuition tells me something about this"),
            (lambda e: True, None),
        ],
    }

    # Transition phrases to connect sentences
    TRANSITIONS = [
        "",              # No transition (first sentence)
        " ",             # Simple space
        " And ",
        " Also, ",
        " Meanwhile, ",
        " At the same time, ",
    ]

    # Opening qualifiers based on overall confidence
    CONFIDENCE_OPENERS = [
        (0.8, ""),                          # High confidence — just say it
        (0.5, "I believe "),                # Medium confidence
        (0.3, "I'm not sure, but "),        # Low confidence
        (0.0, "I'm still learning, but "),  # Very low confidence
    ]

    def __init__(self, max_sentences=3, min_salience=0.15):
        self.max_sentences = max_sentences
        self.min_salience = min_salience

    def compose(self, transcript: list, decision: dict,
                user_input: str = "", vocab_response: str = "",
                context=None) -> Optional[str]:
        """Compose a response from transcript entries.

        Args:
            transcript: List of dicts from Brain.get_transcript()
            decision: Dict from Brain.decide_full() (label, confidence, etc.)
            user_input: The user's original input text
            vocab_response: Best vocabulary nearest-neighbor match (if any)
            context: ContextSignals from ConversationContext (Phase 3)

        Returns:
            Composed response string, or None if nothing meaningful to say
        """
        # Phase 3: Handle greetings and farewells directly
        if context:
            if context.is_greeting:
                return self._greeting_response(context)
            if context.is_farewell:
                return self._farewell_response(context)

        if not transcript:
            return None

        # Sort by salience descending
        ranked = sorted(transcript,
                        key=lambda e: e.get("salience", 0),
                        reverse=True)

        # Filter to entries above min salience that actually contributed
        relevant = [e for e in ranked if e.get("salience", 0) >= self.min_salience]

        if not relevant:
            return None

        # Generate sentence fragments from each relevant entry
        fragments = []
        used_modules = set()

        for entry in relevant:
            if len(fragments) >= self.max_sentences:
                break

            module = entry.get("module", "")
            if module in used_modules:
                continue  # One sentence per module

            fragment = self._generate_fragment(entry)
            if fragment:
                fragments.append(fragment)
                used_modules.add(module)

        if not fragments:
            return None

        # Determine overall confidence for opener
        confidence = decision.get("confidence", 0.0)
        opener = self._get_opener(confidence)

        # Phase 3: Override opener with context-aware phrasing
        if context:
            opener = self._context_opener(opener, context, confidence)

        # Assemble response
        response = self._assemble(fragments, opener, confidence,
                                  decision, vocab_response, user_input)

        # Phase 3: Append trajectory commentary if notable
        if context:
            response = self._add_trajectory_note(response, context)

        return response

    def _generate_fragment(self, entry: dict) -> Optional[str]:
        """Generate a sentence fragment from a single transcript entry."""
        module = entry.get("module", "")
        templates = self.TEMPLATES.get(module)

        if not templates:
            # Unknown module — use generic template if salience is high
            if entry.get("salience", 0) > 0.5 and entry.get("summary"):
                summary = entry["summary"]
                # Clean up C-style summaries
                summary = self._clean_summary(summary)
                return summary
            return None

        # Find first matching template
        for condition_fn, template_str in templates:
            try:
                if condition_fn(entry):
                    if template_str is None:
                        return None  # Explicitly skip
                    # Fill template
                    return self._fill_template(template_str, entry)
            except Exception:
                continue

        return None

    def _fill_template(self, template: str, entry: dict) -> str:
        """Fill a template string with entry data."""
        summary = entry.get("summary", "")
        # Create a short version of the summary (first clause)
        summary_short = self._clean_summary(summary)
        if len(summary_short) > 80:
            summary_short = summary_short[:77] + "..."

        return template.format(
            summary=summary,
            summary_short=summary_short,
            module=entry.get("module", "unknown"),
            salience=entry.get("salience", 0),
            confidence=entry.get("confidence", 0),
        )

    def _clean_summary(self, summary: str) -> str:
        """Clean a C-generated summary for natural language use."""
        if not summary:
            return ""
        # Remove technical prefixes
        summary = re.sub(r'^(WHAT|WHY|HOW|CONF|PROOF):\s*', '', summary)
        summary = re.sub(r'\s*\|\s*(WHY|HOW|CONF|PROOF):.*$', '', summary)
        # Remove brackets
        summary = re.sub(r'\[.*?\]', '', summary).strip()
        # Clean up double spaces
        summary = re.sub(r'\s+', ' ', summary).strip()
        # Lowercase first char if it's not a proper noun start
        if summary and summary[0].isupper() and len(summary) > 1:
            if summary[1].islower():
                summary = summary[0].lower() + summary[1:]
        return summary

    def _get_opener(self, confidence: float) -> str:
        """Get an opening qualifier based on confidence."""
        for threshold, opener in self.CONFIDENCE_OPENERS:
            if confidence >= threshold:
                return opener
        return ""

    def _assemble(self, fragments: list, opener: str, confidence: float,
                  decision: dict, vocab_response: str,
                  user_input: str) -> str:
        """Assemble fragments into a coherent response."""
        parts = []

        # If we have a meaningful vocab response (not a smoke label),
        # lead with it and add cognitive context
        label = decision.get("label", "")
        has_real_label = (label and "smoke" not in label.lower()
                         and "test_label" not in label.lower()
                         and len(label) > 3)
        has_real_vocab = (vocab_response and "smoke" not in vocab_response.lower()
                          and len(vocab_response) > 5)

        if has_real_vocab:
            # Lead with the decoded response, add cognitive framing
            parts.append(vocab_response)
            # Add up to 1 cognitive context sentence
            for frag in fragments[:1]:
                parts.append(frag)
        elif has_real_label and confidence > 0.2:
            # Use the label as a seed, wrap with cognitive framing
            if confidence > 0.6:
                parts.append(f"{opener}{label}.")
            else:
                parts.append(f"{opener}this relates to {label}.")
            for frag in fragments[:2]:
                parts.append(frag)
        else:
            # No good base response — compose entirely from cognitive signals
            if fragments:
                # Apply opener to first fragment
                first = fragments[0]
                if opener and first:
                    # Don't lowercase "I" (pronoun)
                    if first[0] == 'I' and (len(first) < 2 or not first[1].isalpha()):
                        first = opener + first
                    else:
                        first = opener + first[0].lower() + first[1:]
                parts.append(first)
                parts.extend(fragments[1:])

        if not parts:
            return None

        # Join with periods and proper punctuation
        result = self._join_sentences(parts)
        return result

    # ------------------------------------------------------------------
    # Phase 3: Context-aware generation
    # ------------------------------------------------------------------

    def _greeting_response(self, context) -> str:
        """Generate a greeting response."""
        if context.turn_index == 0:
            return "Hello. I'm ready to think with you."
        return "Hello again. What shall we explore?"

    def _farewell_response(self, context) -> str:
        """Generate a farewell response."""
        if context.turn_index > 5:
            return "Goodbye. This was a good conversation."
        return "Goodbye."

    def _context_opener(self, default_opener: str, context,
                        confidence: float) -> str:
        """Choose an opener based on conversation context."""
        # Returning to a prior topic (highest priority)
        if context.is_returning_topic:
            return "Going back to that — "

        # Topic shift
        if context.is_topic_shift and context.turn_index > 0:
            return "On this new topic — "

        # Follow-up: acknowledge continuity
        if context.is_followup:
            if context.topic_depth >= 3:
                return "Going deeper — "
            return "Yes, and "

        # Question: frame as answering
        if context.is_question and confidence > 0.3:
            return ""  # Let the content speak directly

        # Deepening phase: show engagement
        if context.conversation_phase == "deepening":
            if confidence > 0.5:
                return "Building on this — "
            return default_opener

        return default_opener

    def _add_trajectory_note(self, response: str, context) -> Optional[str]:
        """Optionally add a note about confidence/emotional trajectory."""
        if not response:
            return response

        # Only add trajectory notes after enough turns for a real trend
        if context.turn_index < 3:
            return response

        # Confidence improving notably
        if context.confidence_trend > 0.4:
            response += " I'm getting more confident about these topics."
        elif context.confidence_trend < -0.4:
            response += " I'm finding this increasingly challenging."

        return response

    def _join_sentences(self, parts: list) -> str:
        """Join sentence fragments into a natural paragraph."""
        if not parts:
            return ""

        sentences = []
        for i, part in enumerate(parts):
            s = part.strip()
            if not s:
                continue
            # Capitalize first letter
            s = s[0].upper() + s[1:] if s else s
            # Ensure ends with punctuation
            if s and s[-1] not in '.!?':
                s += '.'
            sentences.append(s)

        return " ".join(sentences)


def _val(entry: dict, key: str) -> float:
    """Extract a named value from transcript entry values.

    The C transcript stores values as parallel arrays (values[], value_labels[]).
    The Python binding flattens these into the entry dict. For now, we check
    the summary string for the value.
    """
    # First check if it's directly in the entry (future-proofed)
    if key in entry:
        return float(entry[key])

    # Parse from summary string (e.g., "Curiosity drive: 0.45 (novel input)")
    summary = entry.get("summary", "")
    # Try "key: value" pattern
    match = re.search(rf'{key}[=:]\s*([-\d.]+)', summary, re.IGNORECASE)
    if match:
        try:
            return float(match.group(1))
        except ValueError:
            pass

    # Try "key=value" pattern in summary
    match = re.search(rf'{key}\s*=\s*([-\d.]+)', summary)
    if match:
        try:
            return float(match.group(1))
        except ValueError:
            pass

    return 0.0
