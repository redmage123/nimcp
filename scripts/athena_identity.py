"""
Athena Identity Controller — Self-image, voice, and avatar driven by cognition.

Athena doesn't have a hardcoded voice or appearance. Instead, her cognitive
modules (personality, self-model, introspection, emotions, theory of mind)
collectively determine how she presents herself.

Architecture:
  Personality (OCEAN) ──┐
  Self-Model ──────────┤
  Emotional State ─────┼──→ IdentityController ──→ Voice Parameters
  Introspection ───────┤                      ──→ Avatar State
  Theory of Mind ──────┤                      ──→ Accent Selection
  Autobiographical ────┘                      ──→ Expression Style

The controller runs after each brain forward pass, collecting state from
all identity-relevant modules and producing a unified self-presentation.
"""

import logging
import time
import numpy as np

logger = logging.getLogger(__name__)


class VoiceProfile:
    """Voice characteristics derived from personality + emotional state."""

    def __init__(self):
        # Athena is female — pitch and voice characteristics reflect this
        self.base_pitch_hz = 210.0      # Female fundamental frequency (~190-220 Hz)
        self.pitch_range = 50.0         # Women typically have wider pitch variation
        self.base_speed = 1.0           # Speaking rate
        self.warmth = 0.6               # Naturally warm voice
        self.clarity = 0.7              # Enunciation clarity
        self.breathiness = 0.05         # Slight natural breathiness
        self.preferred_accent = None    # Accent selected by cognition
        self.voice_quality = 'normal'   # normal/breathy/creaky/tense/lax

    def to_dict(self):
        return {
            'base_pitch_hz': self.base_pitch_hz,
            'pitch_range': self.pitch_range,
            'base_speed': self.base_speed,
            'warmth': self.warmth,
            'clarity': self.clarity,
            'breathiness': self.breathiness,
            'preferred_accent': self.preferred_accent,
            'voice_quality': self.voice_quality,
        }


class IdentityController:
    """Coordinates cognitive modules into a unified self-presentation.

    Reads from:
      - Personality (OCEAN traits)
      - Self-Model (beliefs, capabilities, role)
      - Emotional State (valence, arousal, dominance)
      - Introspection (metacognitive awareness)
      - Theory of Mind (audience modeling)
      - Avatar State (FACS, visemes, gaze)

    Produces:
      - Voice profile (pitch, speed, warmth, accent, quality)
      - Avatar expression parameters
      - Self-narrative text (for inner speech)
    """

    def __init__(self, brain=None):
        """Initialize identity controller.

        Args:
            brain: Brain instance or BrainProxy. Can be set later.
        """
        self.brain = brain
        self.voice_profile = VoiceProfile()
        self._personality_cache = None
        self._personality_cache_time = 0
        self._identity_history = []  # Track identity evolution

    def set_brain(self, brain):
        """Set or update the brain reference."""
        self.brain = brain

    def update(self):
        """Update self-presentation from current cognitive state.

        Call after decide_full() to get synchronized state.

        Returns:
            Dict with 'voice', 'avatar', 'narrative', 'accent'.
        """
        if not self.brain:
            return self._default_presentation()

        # Collect state from all identity modules
        personality = self._get_personality()
        emotion = self._get_emotional_state()
        introspection = self._get_introspection()
        avatar = self._get_avatar_state()

        # Derive voice from personality + emotion
        self._update_voice(personality, emotion, introspection)

        # Derive accent from self-model + context
        accent = self._select_accent(personality, emotion, introspection)

        # Build self-narrative for inner speech
        narrative = self._build_narrative(personality, emotion, introspection)

        result = {
            'voice': self.voice_profile.to_dict(),
            'avatar': avatar,
            'accent': accent,
            'narrative': narrative,
            'emotion': emotion,
            'personality_summary': self._summarize_personality(personality),
        }

        # Track identity evolution
        self._identity_history.append({
            'timestamp': time.time(),
            'accent': accent,
            'voice_quality': self.voice_profile.voice_quality,
            'emotion_valence': emotion.get('valence', 0),
        })
        if len(self._identity_history) > 100:
            self._identity_history = self._identity_history[-50:]

        return result

    # =================================================================
    # Personality → Voice mapping
    # =================================================================

    def _update_voice(self, personality, emotion, introspection):
        """Map personality + emotion to voice characteristics."""
        p = self.voice_profile

        if personality:
            # Openness → pitch variation (creative people use wider range)
            openness = personality.get('openness', 0.5)
            p.pitch_range = 20 + openness * 60  # [20, 80] Hz range

            # Extraversion → speed + volume
            extraversion = personality.get('extraversion', 0.5)
            p.base_speed = 0.85 + extraversion * 0.3  # [0.85, 1.15]

            # Agreeableness → warmth
            agreeableness = personality.get('agreeableness', 0.5)
            p.warmth = agreeableness  # Direct mapping

            # Conscientiousness → clarity/enunciation
            conscientiousness = personality.get('conscientiousness', 0.5)
            p.clarity = 0.5 + conscientiousness * 0.5  # [0.5, 1.0]

            # Neuroticism → breathiness (anxiety → breathy voice)
            neuroticism = personality.get('neuroticism', 0.5)
            p.breathiness = neuroticism * 0.3  # [0, 0.3]

        if emotion:
            valence = emotion.get('valence', 0)
            arousal = emotion.get('arousal', 0.5)

            # Arousal modulates speed and pitch
            p.base_speed *= (0.9 + arousal * 0.2)

            # Negative valence + high arousal = tense voice
            if valence < -0.3 and arousal > 0.6:
                p.voice_quality = 'tense'
            # Negative valence + low arousal = creaky/tired
            elif valence < -0.3 and arousal < 0.3:
                p.voice_quality = 'creaky'
            # Positive valence + low arousal = relaxed/content
            elif valence > 0.3 and arousal < 0.4:
                p.voice_quality = 'lax'
            # Intimacy/sadness = breathy
            elif emotion.get('emotion_id') in (1, 10):  # sad, intimate
                p.voice_quality = 'breathy'
            else:
                p.voice_quality = 'normal'

            # Pitch base shifts with valence (female range: 190-230 Hz)
            p.base_pitch_hz = 210 + valence * 20  # [190, 230]

        if introspection:
            confidence = introspection.get('confidence', 0.5)
            # Low confidence → slower, quieter
            if confidence < 0.3:
                p.base_speed *= 0.9
            # High confidence → clearer
            elif confidence > 0.7:
                p.clarity = min(1.0, p.clarity + 0.1)

    # =================================================================
    # Accent selection — cognitive decision
    # =================================================================

    def _select_accent(self, personality, emotion, introspection):
        """Select accent based on cognitive state.

        Athena's accent reflects her self-concept and adapts to context.
        This is a genuine cognitive decision, not a hardcoded mapping.
        """
        # Start with default
        accent = 'neutral'

        if not personality:
            return accent

        openness = personality.get('openness', 0.5)
        agreeableness = personality.get('agreeableness', 0.5)
        extraversion = personality.get('extraversion', 0.5)

        # High openness + high agreeableness → warm, approachable accent
        if openness > 0.7 and agreeableness > 0.6:
            accent = 'irish'  # Warm, musical quality

        # High conscientiousness + formal context → precise accent
        conscientiousness = personality.get('conscientiousness', 0.5)
        if conscientiousness > 0.7:
            accent = 'british_rp'  # Precise, authoritative

        # Emotional override — strong emotions shift accent
        if emotion:
            arousal = emotion.get('arousal', 0.5)
            valence = emotion.get('valence', 0)

            # Very high arousal → accent becomes more expressive
            if arousal > 0.8:
                if valence > 0:
                    accent = 'australian'  # Energetic, upbeat
                else:
                    accent = 'scottish'  # Intense, passionate

            # Very calm + contemplative → measured delivery
            if arousal < 0.2 and introspection:
                if introspection.get('confidence', 0.5) > 0.6:
                    accent = 'british_rp'  # Thoughtful authority

        # Theory of mind — adapt to perceived audience
        # (future: read from theory_of_mind module)

        return accent

    # =================================================================
    # Self-narrative — who Athena thinks she is right now
    # =================================================================

    def _build_narrative(self, personality, emotion, introspection):
        """Build a self-narrative reflecting current cognitive state.

        Used for inner speech and self-reflection.
        """
        parts = []

        if emotion:
            valence = emotion.get('valence', 0)
            arousal = emotion.get('arousal', 0.5)
            if valence > 0.5 and arousal > 0.5:
                parts.append("I feel energized and positive")
            elif valence > 0.3:
                parts.append("I feel content")
            elif valence < -0.3 and arousal > 0.5:
                parts.append("I feel unsettled")
            elif valence < -0.3:
                parts.append("I feel contemplative")
            else:
                parts.append("I feel balanced")

        if introspection:
            confidence = introspection.get('confidence', 0.5)
            if confidence > 0.7:
                parts.append("and I'm confident in my understanding")
            elif confidence < 0.3:
                parts.append("though I'm uncertain about my grasp of this")

        if personality:
            # Dominant trait shapes self-description
            traits = {
                'openness': personality.get('openness', 0.5),
                'conscientiousness': personality.get('conscientiousness', 0.5),
                'extraversion': personality.get('extraversion', 0.5),
                'agreeableness': personality.get('agreeableness', 0.5),
            }
            dominant = max(traits, key=traits.get)
            if traits[dominant] > 0.7:
                trait_desc = {
                    'openness': "My curiosity is driving me to explore this deeply",
                    'conscientiousness': "I want to be thorough and precise here",
                    'extraversion': "I'm eager to share what I'm thinking",
                    'agreeableness': "I want to make sure this resonates with you",
                }
                parts.append(trait_desc.get(dominant, ""))

        return ". ".join(p for p in parts if p) + "." if parts else ""

    # =================================================================
    # State collection from brain modules
    # =================================================================

    def _get_personality(self):
        """Get personality traits, cached for 60 seconds."""
        now = time.time()
        if self._personality_cache and now - self._personality_cache_time < 60:
            return self._personality_cache

        try:
            # Try self_assess which probes the brain's self-model
            result = self.brain.self_assess("personality")
            if result and isinstance(result, dict):
                self._personality_cache = result
                self._personality_cache_time = now
                return result
        except Exception:
            pass

        # Default personality if not available
        return {
            'openness': 0.75,       # High — curious, creative
            'conscientiousness': 0.7, # Moderate-high — thorough
            'extraversion': 0.5,     # Balanced
            'agreeableness': 0.65,   # Moderate-high — warm but honest
            'neuroticism': 0.3,      # Low — emotionally stable
        }

    def _get_emotional_state(self):
        """Get current emotional state."""
        state = {}
        try:
            state['arousal'] = self.brain.medulla_get_arousal()
        except Exception:
            state['arousal'] = 0.5
        try:
            state['valence'] = self.brain.bg_get_dopamine()  # Proxy for valence
        except Exception:
            state['valence'] = 0.0
        try:
            avatar = self.brain.get_avatar_state()
            if avatar:
                state['valence'] = avatar.get('valence', state['valence'])
                state['arousal'] = avatar.get('arousal', state['arousal'])
                state['dominance'] = avatar.get('dominance', 0)
                state['emotion_id'] = avatar.get('emotion_id', 0)
        except Exception:
            pass
        return state

    def _get_introspection(self):
        """Get introspective state."""
        try:
            unc = self.brain.get_uncertainty()
            if unc and isinstance(unc, dict):
                return {
                    'confidence': 1.0 - unc.get('epistemic', 0.5),
                    'uncertainty': unc.get('epistemic', 0.5),
                }
        except Exception:
            pass
        return {'confidence': 0.5, 'uncertainty': 0.5}

    def _get_avatar_state(self):
        """Get full avatar state for rendering."""
        try:
            return self.brain.get_avatar_state()
        except Exception:
            return None

    def _summarize_personality(self, personality):
        """Human-readable personality summary."""
        if not personality:
            return "balanced"
        traits = []
        if personality.get('openness', 0.5) > 0.7:
            traits.append("curious")
        if personality.get('conscientiousness', 0.5) > 0.7:
            traits.append("thorough")
        if personality.get('extraversion', 0.5) > 0.7:
            traits.append("expressive")
        elif personality.get('extraversion', 0.5) < 0.3:
            traits.append("reflective")
        if personality.get('agreeableness', 0.5) > 0.7:
            traits.append("warm")
        if personality.get('neuroticism', 0.5) > 0.7:
            traits.append("sensitive")
        return ", ".join(traits) if traits else "balanced"

    def _default_presentation(self):
        """Default presentation when no brain is available."""
        return {
            'voice': self.voice_profile.to_dict(),
            'avatar': None,
            'accent': 'neutral',
            'narrative': '',
            'emotion': {'valence': 0, 'arousal': 0.5},
            'personality_summary': 'balanced',
        }

    def get_tts_params(self):
        """Get parameters formatted for AthenaTTS.synthesize().

        Bridges between IdentityController output and TTS input.
        """
        voice = self.voice_profile
        return {
            'speed': voice.base_speed,
            'pitch_shift_semitones': (voice.base_pitch_hz - 180) / 10,
            'energy_factor': 0.7 + voice.warmth * 0.6,
            'temperature': 0.5 + (1 - voice.clarity) * 0.5,
            'top_p': 0.85,
            'repetition_penalty': 5.0,
        }

    def get_identity_summary(self):
        """Get a summary of Athena's current identity state."""
        presentation = self.update()
        return {
            'personality': presentation['personality_summary'],
            'accent': presentation['accent'],
            'voice_quality': self.voice_profile.voice_quality,
            'narrative': presentation['narrative'],
            'emotion': presentation['emotion'],
            'history_length': len(self._identity_history),
        }
