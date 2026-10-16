"""
Athena TTS — Brain-state modulated text-to-speech with voice cloning and accents.

Architecture:
  Brain Output (4096d) → Phi-3 (text) + Brain State → AthenaTTS → Audio

  AthenaTTS wraps Coqui XTTS with:
  - Voice cloning from reference audio
  - Accent library (precomputed speaker embeddings)
  - Brain-state prosody modulation (pitch, speed, energy from neural state)
  - Streaming support for real-time conversation

Dependencies:
  - Coqui TTS (XTTS model, auto-downloaded on first use)
  - Phi-3 decoder (for text generation)
  - Brain daemon (for state queries)
"""

import logging
import os
import time
import numpy as np
from pathlib import Path

logger = logging.getLogger(__name__)

# Paths
MODELS_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "models")
ACCENT_DIR = os.path.join(MODELS_DIR, "accents")
DEFAULT_VOICE_DIR = os.path.join(MODELS_DIR, "voices")


class BrainStateProsody:
    """Maps Athena's brain state to speech prosody parameters.

    Converts neural state (arousal, dopamine, confidence, emotions, SNN spikes)
    into concrete TTS parameters (speed, pitch, energy).
    """

    # Default neutral prosody
    NEUTRAL = {
        'speed': 1.0,
        'pitch_shift_semitones': 0.0,
        'energy_factor': 1.0,
        'temperature': 0.75,
        'top_p': 0.85,
        'repetition_penalty': 5.0,
    }

    @staticmethod
    def from_brain_state(brain_state):
        """Convert brain state dict to prosody parameters.

        Args:
            brain_state: Dict with keys like 'arousal', 'dopamine',
                        'confidence', 'snn_spikes', 'active_modules', etc.

        Returns:
            Dict of prosody parameters for TTS.
        """
        if not brain_state:
            return dict(BrainStateProsody.NEUTRAL)

        params = dict(BrainStateProsody.NEUTRAL)

        arousal = brain_state.get('arousal', 0.5)
        dopamine = brain_state.get('dopamine', 0.5)
        confidence = brain_state.get('confidence', 0.5)
        snn_spikes = brain_state.get('snn_spikes', 0)

        # Arousal → speed + pitch
        # High arousal: faster, slightly higher pitch
        # Low arousal: slower, lower pitch
        params['speed'] = 0.85 + arousal * 0.3  # [0.85, 1.15]
        params['pitch_shift_semitones'] = (arousal - 0.5) * 4  # [-2, +2] semitones

        # Dopamine → energy + warmth
        # High dopamine: more energetic, warmer tone
        params['energy_factor'] = 0.8 + dopamine * 0.4  # [0.8, 1.2]

        # Confidence → temperature (determinism) + speed stability
        # High confidence: lower temp (more consistent), steady pace
        # Low confidence: higher temp (more variation), slightly slower
        params['temperature'] = max(0.3, min(1.0, 1.0 - confidence * 0.6))
        if confidence < 0.3:
            params['speed'] *= 0.9  # Slow down when uncertain

        # SNN spike rate modulates expressiveness
        if snn_spikes > 2000:
            params['energy_factor'] *= 1.1  # High neural activity = more expressive

        # Emotional state modulation
        active = brain_state.get('active_modules', [])
        if 'curiosity' in active:
            params['pitch_shift_semitones'] += 1.0  # Rising intonation
        if 'fear' in active or 'anxiety' in active:
            params['speed'] *= 1.15  # Faster when anxious
            params['energy_factor'] *= 0.85  # Slightly quieter

        return params


class AccentLibrary:
    """Manages precomputed accent embeddings for voice style control.

    Stores speaker embeddings from reference audio clips of different accents.
    Supports blending between accents and loading custom accent samples.
    """

    # Built-in accent profiles (will be populated from reference audio)
    ACCENT_CATALOG = {
        'neutral': 'Standard American English',
        'british_rp': 'British Received Pronunciation',
        'scottish': 'Scottish English',
        'irish': 'Irish English',
        'australian': 'Australian English',
        'southern_us': 'Southern United States',
        'new_york': 'New York accent',
        'indian': 'Indian English',
        'french': 'French-accented English',
        'german': 'German-accented English',
    }

    def __init__(self, accent_dir=None):
        self.accent_dir = accent_dir or ACCENT_DIR
        self._embeddings = {}  # accent_name -> (speaker_embedding, gpt_cond_latent)
        os.makedirs(self.accent_dir, exist_ok=True)

    def list_accents(self):
        """List available accents (loaded + catalog)."""
        loaded = list(self._embeddings.keys())
        catalog = list(self.ACCENT_CATALOG.keys())
        return {
            'loaded': loaded,
            'available': catalog,
            'descriptions': self.ACCENT_CATALOG,
        }

    def load_accent(self, name, audio_path, tts_model):
        """Load an accent from a reference audio file.

        Args:
            name: Accent name (e.g., 'british_rp').
            audio_path: Path to reference audio file (6+ seconds).
            tts_model: Loaded XTTS model for computing embeddings.

        Returns:
            True if loaded successfully.
        """
        if not os.path.isfile(audio_path):
            logger.error("Accent audio not found: %s", audio_path)
            return False

        try:
            gpt_cond_latent, speaker_embedding = tts_model.get_conditioning_latents(
                audio_path=[audio_path]
            )
            self._embeddings[name] = {
                'speaker_embedding': speaker_embedding,
                'gpt_cond_latent': gpt_cond_latent,
                'audio_path': audio_path,
            }

            # Cache to disk
            cache_path = os.path.join(self.accent_dir, f"{name}.npz")
            np.savez(cache_path,
                     speaker_embedding=speaker_embedding.cpu().numpy(),
                     gpt_cond_latent=gpt_cond_latent.cpu().numpy())
            logger.info("Loaded accent '%s' from %s", name, audio_path)
            return True
        except Exception as e:
            logger.error("Failed to load accent '%s': %s", name, e)
            return False

    def load_cached(self, name):
        """Load accent from cached embeddings on disk."""
        cache_path = os.path.join(self.accent_dir, f"{name}.npz")
        if not os.path.isfile(cache_path):
            return False
        try:
            import torch
            data = np.load(cache_path)
            self._embeddings[name] = {
                'speaker_embedding': torch.tensor(data['speaker_embedding']),
                'gpt_cond_latent': torch.tensor(data['gpt_cond_latent']),
                'audio_path': None,
            }
            logger.info("Loaded cached accent '%s'", name)
            return True
        except Exception as e:
            logger.error("Failed to load cached accent '%s': %s", name, e)
            return False

    def get_accent(self, name):
        """Get accent embeddings by name."""
        if name not in self._embeddings:
            self.load_cached(name)
        return self._embeddings.get(name)

    def blend_accents(self, accents_weights):
        """Blend multiple accents with weights.

        Args:
            accents_weights: Dict of {accent_name: weight}.
                            Weights are normalized to sum to 1.

        Returns:
            Blended (speaker_embedding, gpt_cond_latent) or None.
        """
        import torch
        total_weight = sum(accents_weights.values())
        if total_weight <= 0:
            return None

        blended_speaker = None
        blended_gpt = None

        for name, weight in accents_weights.items():
            accent = self.get_accent(name)
            if accent is None:
                continue
            w = weight / total_weight
            se = accent['speaker_embedding']
            gl = accent['gpt_cond_latent']

            if blended_speaker is None:
                blended_speaker = se * w
                blended_gpt = gl * w
            else:
                blended_speaker = blended_speaker + se * w
                blended_gpt = blended_gpt + gl * w

        if blended_speaker is None:
            return None
        return {
            'speaker_embedding': blended_speaker,
            'gpt_cond_latent': blended_gpt,
        }


class AthenaTTS:
    """Brain-state modulated TTS for Athena.

    Combines:
    - Coqui XTTS for high-quality voice synthesis + cloning
    - Brain state prosody modulation
    - Accent library for voice style control
    - Phi-3 for text generation (optional, uses pre-generated text if provided)

    Lazy-loads the XTTS model on first use (~1.5GB).
    Runs on CPU by default to keep GPU free for brain training.
    """

    def __init__(self, voice_sample=None, accent='neutral', device='cpu'):
        """Initialize Athena TTS.

        Args:
            voice_sample: Path to reference audio for Athena's voice (6+ sec).
            accent: Default accent name.
            device: 'cpu' or 'cuda'. Default 'cpu' to preserve GPU for brain.
        """
        self.voice_sample = voice_sample
        self.default_accent = accent
        self.device = device
        self._model = None
        self._available = None
        self._voice_embeddings = None  # Athena's base voice
        self.accent_library = AccentLibrary()
        self.prosody = BrainStateProsody()

    @property
    def available(self):
        """Check if TTS is available."""
        if self._available is None:
            try:
                import TTS  # noqa: F401
                self._available = True
            except ImportError:
                self._available = False
                logger.info("Coqui TTS not available")
        return self._available

    def _load_model(self):
        """Load XTTS model lazily."""
        if self._model is not None:
            return True
        if not self.available:
            return False

        try:
            from TTS.api import TTS
            logger.info("Loading XTTS model (this may download ~1.5GB on first use)...")
            t0 = time.time()
            self._model = TTS("tts_models/multilingual/multi-dataset/xtts_v2")
            if self.device == 'cpu':
                self._model.to('cpu')
            logger.info("XTTS loaded in %.1fs", time.time() - t0)

            # Load Athena's voice if provided
            if self.voice_sample and os.path.isfile(self.voice_sample):
                self._load_voice(self.voice_sample)

            # Load cached accents
            for name in self.accent_library.ACCENT_CATALOG:
                self.accent_library.load_cached(name)

            return True
        except Exception as e:
            logger.error("Failed to load XTTS: %s", e)
            self._available = False
            return False

    def _load_voice(self, audio_path):
        """Load Athena's base voice from reference audio."""
        try:
            model = self._model.synthesizer.tts_model
            gpt_cond_latent, speaker_embedding = model.get_conditioning_latents(
                audio_path=[audio_path]
            )
            self._voice_embeddings = {
                'speaker_embedding': speaker_embedding,
                'gpt_cond_latent': gpt_cond_latent,
            }
            logger.info("Loaded Athena voice from %s", audio_path)
        except Exception as e:
            logger.error("Failed to load voice: %s", e)

    def synthesize(self, text, brain_state=None, accent=None,
                   output_path=None, stream=False):
        """Synthesize speech from text with brain-state modulation.

        Args:
            text: Text to speak.
            brain_state: Dict of brain state for prosody modulation.
            accent: Accent name or dict of {accent: weight} for blending.
            output_path: Save audio to file. If None, returns numpy array.
            stream: If True, yield audio chunks for streaming.

        Returns:
            Dict with 'audio' (numpy array), 'sample_rate', 'duration',
            'prosody' (applied parameters), or generator if stream=True.
        """
        if not self._load_model():
            return None

        # Get prosody parameters from brain state
        params = self.prosody.from_brain_state(brain_state)

        # Determine voice/accent embeddings
        voice = self._get_voice_for_accent(accent)

        try:
            model = self._model.synthesizer.tts_model

            if voice:
                # Use XTTS low-level API with speaker embeddings
                result = model.inference(
                    text=text,
                    language="en",
                    gpt_cond_latent=voice['gpt_cond_latent'],
                    speaker_embedding=voice['speaker_embedding'],
                    temperature=params['temperature'],
                    speed=params['speed'],
                    top_p=params['top_p'],
                    repetition_penalty=params['repetition_penalty'],
                    enable_text_splitting=True,
                )
                audio = result['wav']
            else:
                # Fallback: use default XTTS voice
                audio = self._model.tts(
                    text=text,
                    language="en",
                    speed=params['speed'],
                )
                audio = np.array(audio)

            sample_rate = 24000  # XTTS outputs 24kHz

            # Apply post-processing from brain state
            audio = self._apply_prosody_post(audio, sample_rate, params)

            if output_path:
                import soundfile as sf
                sf.write(output_path, audio, sample_rate)
                logger.info("Saved audio to %s (%.1fs)",
                           output_path, len(audio) / sample_rate)

            return {
                'audio': audio,
                'sample_rate': sample_rate,
                'duration': len(audio) / sample_rate,
                'prosody': params,
                'accent': accent or self.default_accent,
            }

        except Exception as e:
            logger.error("TTS synthesis failed: %s", e)
            return None

    def _get_voice_for_accent(self, accent):
        """Get voice embeddings for the specified accent."""
        if accent is None:
            return self._voice_embeddings

        # Accent blending
        if isinstance(accent, dict):
            blended = self.accent_library.blend_accents(accent)
            if blended:
                return blended

        # Single accent
        if isinstance(accent, str):
            accent_data = self.accent_library.get_accent(accent)
            if accent_data:
                return accent_data

        # Fallback to Athena's base voice
        return self._voice_embeddings

    def _apply_prosody_post(self, audio, sample_rate, params):
        """Apply post-synthesis prosody modifications.

        Handles pitch shifting and energy scaling that can't be done
        in the XTTS generation itself.
        """
        audio = np.array(audio, dtype=np.float32)

        # Energy scaling
        energy = params.get('energy_factor', 1.0)
        if abs(energy - 1.0) > 0.01:
            audio = audio * energy

        # Pitch shifting (if librosa available)
        pitch_shift = params.get('pitch_shift_semitones', 0.0)
        if abs(pitch_shift) > 0.1:
            try:
                import librosa
                audio = librosa.effects.pitch_shift(
                    audio, sr=sample_rate, n_steps=pitch_shift)
            except ImportError:
                pass  # Skip pitch shift without librosa

        # Clip to prevent distortion
        audio = np.clip(audio, -1.0, 1.0)

        return audio

    def register_accent(self, name, audio_path):
        """Register a new accent from a reference audio sample.

        Args:
            name: Accent name (e.g., 'cockney', 'texan').
            audio_path: Path to 6+ second reference audio.

        Returns:
            True if successful.
        """
        if not self._load_model():
            return False
        model = self._model.synthesizer.tts_model
        return self.accent_library.load_accent(name, audio_path, model)

    def set_voice(self, audio_path):
        """Set Athena's base voice from a new reference sample."""
        self.voice_sample = audio_path
        if self._model:
            self._load_voice(audio_path)

    def speak(self, text, brain=None, accent=None, output_path=None):
        """High-level: generate speech from text with brain state.

        Collects brain state automatically if brain proxy provided.

        Args:
            text: Text to speak.
            brain: BrainProxy or Brain instance for state queries.
            accent: Accent name or blend dict.
            output_path: Save to file.

        Returns:
            Dict with audio data and metadata.
        """
        brain_state = None
        if brain:
            brain_state = self._collect_brain_state(brain)

        return self.synthesize(text, brain_state=brain_state,
                              accent=accent, output_path=output_path)

    def _collect_brain_state(self, brain):
        """Collect brain state for prosody modulation."""
        state = {}
        try:
            state['arousal'] = brain.medulla_get_arousal()
        except Exception:
            state['arousal'] = 0.5
        try:
            state['dopamine'] = brain.bg_get_dopamine()
        except Exception:
            pass
        try:
            snn = brain.snn_get_stats()
            if snn:
                state['snn_spikes'] = snn.get('total_spikes', 0)
        except Exception:
            pass
        try:
            cs = brain.get_cognitive_stats()
            if cs:
                state['active_modules'] = [k for k, v in cs.items()
                                           if v.get('steps', 0) > 0]
        except Exception:
            pass
        return state

    def unload(self):
        """Free model memory."""
        if self._model is not None:
            del self._model
            self._model = None
            self._voice_embeddings = None
            logger.info("XTTS model unloaded")


def full_pipeline(text, brain=None, phi3=None, tts=None, accent=None,
                  output_path=None):
    """Complete Athena speech pipeline: Brain → Phi-3 → TTS → Audio.

    Args:
        text: Input text/question.
        brain: BrainProxy for brain state + decide_full.
        phi3: Phi3Decoder for text generation.
        tts: AthenaTTS for speech synthesis.
        accent: Accent name or blend.
        output_path: Save audio file.

    Returns:
        Dict with 'response_text', 'audio', 'brain_state', 'prosody'.
    """
    result = {
        'input': text,
        'response_text': '',
        'audio': None,
        'brain_state': None,
        'prosody': None,
    }

    # Step 1: Generate response via Phi-3 + Brain
    response_text = text  # Default: speak the input directly
    if phi3 and brain:
        try:
            from hybrid_decoder import HybridDecoder
            hybrid = HybridDecoder(phi3_decoder=phi3, brain=brain)
            decoded = hybrid.respond(text, brain=brain)
            if decoded.get('text'):
                response_text = decoded['text']
        except Exception as e:
            logger.error("Phi-3 generation failed: %s", e)

    result['response_text'] = response_text

    # Step 2: Synthesize speech
    if tts and tts.available:
        audio_result = tts.speak(response_text, brain=brain,
                                accent=accent, output_path=output_path)
        if audio_result:
            result['audio'] = audio_result.get('audio')
            result['prosody'] = audio_result.get('prosody')
            result['brain_state'] = audio_result.get('brain_state')

    return result
