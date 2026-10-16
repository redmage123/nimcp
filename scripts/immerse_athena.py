#!/usr/bin/env python3
"""
immerse_athena.py  —  Immersive Developmental Learning for Athena

Athena learns through her own biological brain: sensory cortices process raw stimuli,
biological plasticity pathways (STDP/BCM/eligibility) wire synaptic patterns, and
Claude serves as a loving parent guiding her developmental stages.

Every module in the brain is active: 33+ brain regions, 60+ cognitive modules,
80+ substrate bridges, 75+ thalamic bridges, 19 inter-module bridges.
No shortcuts. No fast-training mode. Full biological immersion.

IMPORTANT — ATHENA IS NOT A CLASSIFIER OR A PREDICTOR.
    She does not produce labels, categories, class predictions, or scored
    outputs. She is a developing biological brain. Her 4096-dim output
    is her neural response — a continuous representation of her experience.
    We do not score her responses against expected answers. We do not
    measure accuracy or similarity to a "correct" output.
    She experiences stimuli through decide_full(), and she grows through
    learn_vector() with semantic targets. The loss is a developmental
    signal — how new an experience is to her — not a prediction error.
    We observe her responses. We encourage her. We never grade her.

Usage:
    python scripts/immerse_athena.py --stage 0          # Start from stage 0 (newborn)
    python scripts/immerse_athena.py --resume            # Resume from last checkpoint
    python scripts/immerse_athena.py --stage 2 --resume  # Resume at stage 2
    python scripts/immerse_athena.py --no-claude          # Run without Claude (silent parenting)
"""

import argparse
import hashlib
import logging
import os
import socket
import struct
import sys
import threading
import time
import random

# CB rescale checkpoint marker — write the sidecar after every save while
# CB is enabled so daemon restarts loading from this snapshot don't
# double-rescale. See scripts/cb_rescaled_marker.py.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cb_rescaled_marker
CB_DEFAULT_RESCALE_FACTOR = 1.0 / 50.0
import json
import signal
import numpy as np

# Force unbuffered stdout so prints appear in log files immediately
sys.stdout.reconfigure(line_buffering=True)

# Add scripts directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Suppress noisy progress bars and verbose logging
os.environ["TOKENIZERS_PARALLELISM"] = "false"
os.environ["TQDM_DISABLE"] = "1"
# Disable COW SIGSEGV handler — conflicts with PyTorch/CUDA lazy memory mapping
os.environ["NIMCP_NO_COW_SIGNAL"] = "1"
import warnings
warnings.filterwarnings("ignore", category=FutureWarning)
logging.getLogger("httpx").setLevel(logging.WARNING)
logging.getLogger("sentence_transformers").setLevel(logging.WARNING)

# Import nimcp FIRST — it initializes the CUDA context for brain training.
# In --daemon mode, nimcp is only needed for nimcp.init() which is optional
# since the daemon process already initialized everything.
if "--daemon" not in sys.argv:
    import nimcp
else:
    nimcp = None  # not needed — BrainProxy handles everything

# Hide GPU from PyTorch to prevent dual-CUDA-context crashes.
# BUT: nimcp's gpu_detect uses pthread_once + cuInit which reads
# CUDA_VISIBLE_DEVICES at call time (during Brain()), not import time.
# So we hide it only for the PyTorch import, then restore it before
# Brain() is created.
_saved_cuda_vis = os.environ.get("CUDA_VISIBLE_DEVICES")
os.environ["CUDA_VISIBLE_DEVICES"] = ""
from claude_teacher import ClaudeTeacher, encode_text, batch_encode_texts
from talk_to_athena import extract_embedding_from_output
from neural_decoder import NeuralDecoder
from multimodal_data import MultimodalDataLoader
from cognitive_training_data import get_all_cognitive_data, get_random_cognitive_item
try:
    from spectral_kfold import SpectralKFoldSplitter
except ImportError:
    SpectralKFoldSplitter = None
try:
    from expanded_sensory_data import get_all_sensory, get_expanded_objects
    _EXPANDED_SENSORY_AVAILABLE = True
except ImportError:
    _EXPANDED_SENSORY_AVAILABLE = False


def generate_sensory_exposure():
    """Generate raw sensory exposure — things a newborn would experience."""
    experiences = [
        "bright red ball rolling across the floor",
        "sunlight streaming through the window making warm patches",
        "shadows moving on the wall as clouds pass by",
        "a blue sky with white fluffy clouds",
        "green leaves rustling in the breeze",
        "water drops splashing in a puddle",
        "a yellow flower swaying gently",
        "colorful blocks stacked in a tower",
        "a cat stretching and yawning",
        "rain drops running down the window glass",
        "a candle flame flickering in the dark",
        "snow falling softly and covering the ground",
        "bubbles floating up and popping in the air",
        "a rainbow arcing across the sky after rain",
        "fireflies blinking in the garden at night",
        "smooth cold glass under fingers",
        "rough warm sand between toes",
        "soft fur of a sleeping cat",
        "cool water flowing over hands",
        "the weight of a heavy book in your lap",
        "birds singing at dawn",
        "rain tapping on a tin roof",
        "a clock ticking steadily in a quiet room",
        "wind howling through trees at night",
        "waves crashing on the shore rhythmically",
        "a dog barking in the distance",
        "thunder rumbling across the sky",
        "leaves crunching underfoot in autumn",
        "a tall tree next to a small bush",
        "a ball rolling under the table",
        "a bird flying high above the houses",
        "a bridge stretching over a wide river",
        "stairs going up and up and up",
        "a warm hug from someone who cares",
        "laughing so hard your sides hurt",
        "the comfort of a familiar blanket",
        "being startled by a sudden loud noise",
        "the peacefulness of a quiet morning",
    ]
    text = random.choice(experiences)
    embedding = encode_text(text)
    return text, embedding


# ============================================================================
# Synthetic Sensory Data Generators (for multimodal SNN bridge training)
# ============================================================================

from functools import lru_cache

# Module-level training integration — initialized lazily in main(). Used by
# stage_1/2/3 loops to invoke Phase A-E module hooks without having to
# thread it through every function signature.
_training_integration = None

# Module-global mirror of args.probe_drips. The run_stage_* functions are
# module-level (not nested in main), so they cannot see main()'s local `args`.
# Referencing `args` there raised NameError at every 200th step — crashing the
# trainer (exit 1) and bouncing it via supervisord before it could finish a
# stage, which silently pinned the curriculum at stage 2. main() sets this.
# NB: distinct name from the `_PROBE_DRIPS` *registry tuple* defined below
# (~line 6600) which _run_probe_drips iterates — do NOT reuse that name.
_PROBE_DRIPS_ENABLED = True


@lru_cache(maxsize=4096)
def generate_visual_frame(description, width=32, height=32, channels=3):
    """Generate a simple synthetic visual frame from a text description.

    Creates a deterministic color/pattern image seeded by the description hash.
    Not photorealistic — just enough to give the visual SNN bridge meaningful
    spatial patterns to encode (edges, gradients, color regions).
    Memoized via lru_cache — C-speed hash lookup.
    """
    seed = hash(description) & 0xFFFFFFFF
    rng = np.random.RandomState(seed)

    frame = np.zeros((height, width, channels), dtype=np.uint8)

    # Base color from description keywords
    color_map = {
        "red": [200, 40, 40], "blue": [40, 40, 200], "green": [40, 180, 40],
        "yellow": [200, 200, 40], "white": [220, 220, 220], "black": [20, 20, 20],
        "orange": [220, 140, 30], "purple": [140, 40, 180], "brown": [120, 80, 40],
        "pink": [220, 120, 160], "gray": [128, 128, 128], "grey": [128, 128, 128],
    }
    base_color = [80, 100, 120]  # neutral default
    desc_lower = description.lower()
    for word, color in color_map.items():
        if word in desc_lower:
            base_color = color
            break

    # Fill background gradient
    for y in range(height):
        t = y / max(height - 1, 1)
        for c in range(channels):
            frame[y, :, c] = int(base_color[c] * (1.0 - 0.3 * t))

    # Add a simple shape based on description
    cx, cy = width // 2, height // 2
    if any(w in desc_lower for w in ["ball", "circle", "sun", "moon", "bubble"]):
        # Circle
        r = min(width, height) // 4
        for y in range(height):
            for x in range(width):
                if (x - cx) ** 2 + (y - cy) ** 2 < r * r:
                    for c in range(channels):
                        frame[y, x, c] = min(255, base_color[c] + 60)
    elif any(w in desc_lower for w in ["block", "square", "box", "book", "window"]):
        # Rectangle
        s = min(width, height) // 4
        frame[cy - s:cy + s, cx - s:cx + s] = np.array(base_color, dtype=np.uint8) + 40
    else:
        # Random texture
        noise = rng.randint(0, 40, (height, width, channels), dtype=np.uint8)
        frame = np.clip(frame.astype(np.int16) + noise, 0, 255).astype(np.uint8)

    return (frame.tobytes(), width, height, channels)


@lru_cache(maxsize=4096)
def generate_audio_samples(description, num_samples=512, sample_rate=16000):
    """Generate synthetic audio waveform from a text description.
    Memoized via lru_cache.

    Creates simple tones, noise patterns, or silence based on keywords.
    Returns float array in [-1, 1] range for SNN audio bridge encoding.
    """
    seed = hash(description) & 0xFFFFFFFF
    rng = np.random.RandomState(seed)
    t = np.linspace(0, num_samples / sample_rate, num_samples, dtype=np.float32)

    desc_lower = description.lower()

    # Choose waveform type from description
    if any(w in desc_lower for w in ["singing", "music", "melody", "song", "tone"]):
        freq = 220.0 + rng.random() * 440.0  # A3 to A4 range
        signal = 0.5 * np.sin(2 * np.pi * freq * t)
        # Add harmonics
        signal += 0.2 * np.sin(2 * np.pi * freq * 2 * t)
        signal += 0.1 * np.sin(2 * np.pi * freq * 3 * t)
    elif any(w in desc_lower for w in ["thunder", "crash", "bang", "loud", "bark"]):
        # Burst noise with decay
        signal = rng.randn(num_samples).astype(np.float32)
        decay = np.exp(-t * 8.0)
        signal *= decay * 0.8
    elif any(w in desc_lower for w in ["whisper", "quiet", "soft", "gentle", "purr"]):
        signal = rng.randn(num_samples).astype(np.float32) * 0.05
    elif any(w in desc_lower for w in ["rain", "water", "wave", "stream", "splash"]):
        # Pink-ish noise
        signal = rng.randn(num_samples).astype(np.float32)
        # Simple low-pass via cumulative average
        for i in range(1, num_samples):
            signal[i] = 0.95 * signal[i - 1] + 0.05 * signal[i]
        signal *= 0.3
    elif any(w in desc_lower for w in ["wind", "howl", "blow", "breeze"]):
        freq = 80.0 + rng.random() * 60.0
        signal = 0.3 * np.sin(2 * np.pi * freq * t)
        signal += rng.randn(num_samples).astype(np.float32) * 0.15
    elif any(w in desc_lower for w in ["tick", "click", "tap", "crunch"]):
        signal = np.zeros(num_samples, dtype=np.float32)
        # Periodic impulses
        period = num_samples // 8
        for k in range(8):
            idx = k * period
            if idx < num_samples:
                signal[idx:min(idx + 10, num_samples)] = 0.7
    else:
        # Generic ambient with a fundamental
        freq = 150.0 + rng.random() * 200.0
        signal = 0.3 * np.sin(2 * np.pi * freq * t)
        signal += rng.randn(num_samples).astype(np.float32) * 0.1

    # Normalize to [-1, 1]
    peak = np.max(np.abs(signal))
    if peak > 1e-6:
        signal = signal / peak * 0.9

    return signal.tolist()


_mel_cache = {}

def audio_to_mel(samples, n_mels=128, sample_rate=16000):
    """Convert raw audio samples to a simple mel-like spectral representation.
    Memoized by sample content hash.

    Uses a basic FFT → mel filterbank → log compression pipeline.
    Not a proper librosa mel — just enough to give the audio cortex CNN
    meaningful frequency-domain features to learn from.
    """
    # Cache by tuple of first/last 4 samples + length (fast hash proxy)
    cache_key = (len(samples), tuple(samples[:4]), tuple(samples[-4:])) if len(samples) >= 8 else tuple(samples)
    if cache_key in _mel_cache:
        return _mel_cache[cache_key]
    signal = np.array(samples, dtype=np.float32)
    n_fft = min(512, len(signal))
    hop = n_fft // 2

    # STFT frames
    n_frames = max(1, (len(signal) - n_fft) // hop + 1)
    power_frames = []
    for fr in range(n_frames):
        start = fr * hop
        frame = signal[start:start + n_fft]
        if len(frame) < n_fft:
            frame = np.pad(frame, (0, n_fft - len(frame)))
        windowed = frame * np.hanning(n_fft)
        spectrum = np.abs(np.fft.rfft(windowed)) ** 2
        power_frames.append(spectrum)

    if not power_frames:
        return [0.0] * n_mels

    power = np.mean(power_frames, axis=0)  # Average across frames
    n_freqs = len(power)

    # Simple mel filterbank (triangular filters, linearly spaced in mel)
    mel_low = 0.0
    mel_high = 2595.0 * np.log10(1.0 + (sample_rate / 2.0) / 700.0)
    mel_points = np.linspace(mel_low, mel_high, n_mels + 2)
    hz_points = 700.0 * (10.0 ** (mel_points / 2595.0) - 1.0)
    bin_points = np.floor((n_fft + 1) * hz_points / sample_rate).astype(int)
    bin_points = np.clip(bin_points, 0, n_freqs - 1)

    mel_spec = np.zeros(n_mels, dtype=np.float32)
    for m in range(n_mels):
        lo, mid, hi = bin_points[m], bin_points[m + 1], bin_points[m + 2]
        if mid == lo:
            mid = lo + 1
        if hi == mid:
            hi = mid + 1
        for k in range(lo, min(mid, n_freqs)):
            mel_spec[m] += power[k] * (k - lo) / max(1, mid - lo)
        for k in range(mid, min(hi, n_freqs)):
            mel_spec[m] += power[k] * (hi - k) / max(1, hi - mid)

    # Log compression
    mel_spec = np.log1p(mel_spec * 1000.0)

    # Normalize to [0, 1]
    peak = mel_spec.max()
    if peak > 1e-6:
        mel_spec /= peak

    result = mel_spec.tolist()
    if len(_mel_cache) < 5000:
        _mel_cache[cache_key] = result
    return result


@lru_cache(maxsize=4096)
def generate_somatosensory(description, num_segments=32):
    """Generate synthetic somatosensory (touch/proprioception) data.
    Memoized via lru_cache.

    Returns float array of joint-angle/pressure values in [0, 1].
    """
    seed = hash(description) & 0xFFFFFFFF
    rng = np.random.RandomState(seed)
    data = np.zeros(num_segments, dtype=np.float32)

    desc_lower = description.lower()
    if any(w in desc_lower for w in ["warm", "hot", "fire", "sun", "blaz"]):
        data[:16] = 0.7 + rng.random(16).astype(np.float32) * 0.3  # temperature
    elif any(w in desc_lower for w in ["cold", "ice", "snow", "freez", "cool"]):
        data[:16] = 0.1 + rng.random(16).astype(np.float32) * 0.15
    elif any(w in desc_lower for w in ["rough", "sand", "crunch"]):
        data[:16] = rng.random(16).astype(np.float32) * 0.8
        data[16:] = 0.5 + rng.random(num_segments - 16).astype(np.float32) * 0.3  # pressure
    elif any(w in desc_lower for w in ["smooth", "soft", "fur", "silk"]):
        data[:16] = 0.3 + rng.random(16).astype(np.float32) * 0.1
        data[16:] = 0.2 + rng.random(num_segments - 16).astype(np.float32) * 0.1
    elif any(w in desc_lower for w in ["heavy", "weight", "press"]):
        data[16:] = 0.6 + rng.random(num_segments - 16).astype(np.float32) * 0.4
    elif any(w in desc_lower for w in ["hug", "touch", "hand", "finger"]):
        data[:] = 0.4 + rng.random(num_segments).astype(np.float32) * 0.3
    else:
        data[:] = 0.3 + rng.random(num_segments).astype(np.float32) * 0.2

    return data.tolist()


# Module-level frozensets for O(1) keyword lookup (avoid list recreation per call)
_VISUAL_KW = frozenset(["ball", "sky", "cloud", "leaf", "water", "flower", "block",
                         "cat", "rain", "candle", "snow", "bubble", "rainbow", "fire",
                         "tree", "bird", "river", "bridge", "stair", "color", "light",
                         "dark", "shadow", "sun", "moon", "star", "bright", "glow"])
_AUDIO_KW = frozenset(["singing", "music", "thunder", "crash", "bark", "whisper",
                        "rain", "wave", "wind", "tick", "tap", "crunch", "howl",
                        "song", "melody", "loud", "quiet", "sound", "noise",
                        "clock", "bird", "purr"])
_SPEECH_KW = frozenset(["voice", "speak", "talk", "say", "word", "call", "cry",
                         "laugh", "giggle", "babble", "coo", "hum", "sing",
                         "whisper", "shout", "yell", "murmur", "chatter",
                         "lullaby", "nursery", "rhyme", "story", "read",
                         "mama", "papa", "grandma", "baby", "child", "person",
                         "dog", "cat", "bird", "rooster", "cow", "frog",
                         "crow", "goose", "duck", "owl", "cricket", "bee",
                         "bark", "meow", "purr", "moo", "croak", "chirp",
                         "caw", "honk", "quack", "hoot", "buzz", "howl"])
_SOMATO_KW = frozenset(["touch", "feel", "rough", "smooth", "soft", "hard",
                         "warm", "cold", "hot", "wet", "dry", "sticky",
                         "fuzzy", "prickly", "bumpy", "silky", "scratchy",
                         "heavy", "light", "pressure", "squeeze", "tickle",
                         "fur", "wool", "sand", "grass", "stone", "clay",
                         "silk", "velvet", "leather", "wood", "metal",
                         "ice", "water", "mud", "fabric", "cotton", "wicker"])


def submit_multimodal(brain, description, target_word=None):
    """Submit synthetic sensory data for a description to the brain.

    Called before brain.decide_full() so the SNN sensory bridges get native
    modality data instead of interpreting text embeddings as pixels/MFCCs.

    target_word: explicit echo-correct teaching word. When provided, this
    overrides the longest-word heuristic — pass the OBJECT NAME / canonical
    noun (e.g. "tree", "wool") rather than letting the bridge guess.
    Without this, the heuristic picks max(words, key=len) which biases
    toward jargon ("self-extinguishing" beats "wool" in length) and trains
    the brain on adjective salad instead of the actual subject.
    """
    # Echo-and-correct — supervised production training signal. Fires per
    # submit_multimodal call regardless of which stage code path invoked
    # us (run_stage_2 main loop, parent teaching, developmental imitation,
    # etc.). This is the production-side training pulse the bridge needs
    # to escape mode collapse.
    if description and len(description) > 3:
        try:
            chosen = target_word
            if not chosen:
                words = [w.strip(".,!?;:\"'()-").lower()
                         for w in description.split()
                         if len(w.strip(".,!?;:\"'()-")) > 3]
                if words:
                    chosen = max(words, key=len)
            if chosen:
                brain.echo_and_correct(description, chosen, lr_scale=8.0)
        except Exception:
            pass

        # Trigram + bigram STDP on the bridge. train_cognitive is dormant in
        # stage 2, so the learn_text_bigrams wired into nimcp_brain_train_cognitive
        # never fires. Drive it directly here so the bridge's trigram-LTD-on-
        # false-winner path runs against margin-gated LTD + saturating LTP.
        try:
            brain.learn_text_bigrams(description, lr=0.02)
        except Exception:
            pass

    desc_lower = description.lower()

    # Prepare all sensory data locally, then submit in a single batched call.
    # Saves 3 socket round-trips (~150ms) when multiple modalities match.
    _sensory_submitted = []
    desc_words = set(desc_lower.split())
    batch_modalities = {}

    # Always submit visual — individually, not via batch. The batch RPC
    # path silently drops visual data (the batch JSON with 3072 pixels
    # either fails serialization or the daemon's batch handler skips it).
    # Individual fire-and-forget submit works reliably for audio/speech,
    # so use the same path for visual. This bypasses submit_sensory_batch.
    try:
        _vf = generate_visual_frame(description)
        _pixels = list(_vf[0])  # _vf is (bytes, w, h, ch) tuple
        brain.submit_sensory("visual", _pixels, width=_vf[1], height=_vf[2], channels=_vf[3])
        _sensory_submitted.append("V")
    except Exception as _ve:
        import traceback
        print(f"  [VISUAL-ERR] submit_sensory visual failed: {_ve}", flush=True)
        traceback.print_exc()

    # Always submit audio — every description can be synthesized as sound.
    # Audio cortex needs continuous data to train (0 backward without this).
    samples = generate_audio_samples(description)
    mel = audio_to_mel(samples)
    batch_modalities["audio"] = mel
    _sensory_submitted.append("A")

    if desc_words & _SPEECH_KW:
        samples = generate_audio_samples(description)
        batch_modalities["speech"] = samples
        _sensory_submitted.append("Sp")

    if desc_words & _SOMATO_KW:
        seed = hash(description) & 0xFFFFFFFF
        rng = np.random.RandomState(seed)
        somato_vec = np.zeros(64, dtype=np.float32)
        somato_vec[0] = 0.8 if desc_words & {"rough", "bumpy", "prickly", "scratchy", "wicker"} else 0.2
        somato_vec[1] = 0.8 if desc_words & {"smooth", "silky", "soft", "velvet", "silk"} else 0.2
        somato_vec[2] = 0.8 if desc_words & {"hard", "stone", "metal", "wood"} else 0.2
        somato_vec[3] = 0.8 if desc_words & {"fuzzy", "fur", "wool", "cotton"} else 0.2
        somato_vec[16] = 0.9 if desc_words & {"hot", "warm", "fire"} else 0.3
        somato_vec[17] = 0.9 if desc_words & {"cold", "ice", "cool"} else 0.3
        somato_vec[18] = 0.5 if desc_words & {"wet", "water", "mud"} else 0.1
        somato_vec[24] = 0.7 if desc_words & {"heavy", "pressure", "squeeze"} else 0.2
        somato_vec[25] = 0.7 if desc_words & {"light", "tickle", "gentle"} else 0.2
        somato_vec += rng.randn(64).astype(np.float32) * 0.05
        somato_vec = np.clip(somato_vec, 0.0, 1.0)
        batch_modalities["somatosensory"] = (somato_vec.tolist(), len(somato_vec))
        _sensory_submitted.append("S")

        # SPATIAL-modality grounding (GL_MODALITY_SPATIAL=4). The somato
        # vector encodes texture/temperature/pressure derived from keyword
        # matching, which is a body-relative spatial proxy (where the
        # surface meets the body). Bind content tokens against it so the
        # lexicon's SPATIAL slot accumulates real signal during training;
        # without this the SPATIAL bin is always empty. Lower attention
        # (0.6) than visual/audio (0.7) because the encoding is keyword-
        # derived, not from a real spatial sensor.
        if hasattr(brain, "ground_word"):
            _spatial_tokens = _tokenize_for_grounding(description)
            _somato_list = somato_vec.tolist()
            for _tok in _spatial_tokens:
                try:
                    brain.ground_word(_tok, _somato_list,
                                      modality=4, attention=0.6)
                except Exception:
                    pass

    # Single batched socket call for all modalities
    if batch_modalities:
        _has_vis = "visual" in batch_modalities
        try:
            brain.submit_sensory_batch(batch_modalities)
        except Exception as e:
            if _has_vis:
                print(f"  [VISUAL-TRACE] batch failed: {e}", flush=True)
            # Fallback: submit individually. Log every failure — silent drops
            # mean training loses sensory data without anyone noticing.
            _drop_count = 0
            for mod, data in batch_modalities.items():
                try:
                    if mod == "visual":
                        pixels, w, h, ch = data
                        print(f"  [VISUAL-TRACE] fallback submit: {len(pixels)} pixels, {w}x{h}x{ch}", flush=True)
                        brain.submit_sensory("visual", pixels, width=w, height=h, channels=ch)
                    elif mod == "somatosensory":
                        vec, n_seg = data
                        brain.submit_sensory("somatosensory", vec, n_segments=n_seg)
                    else:
                        brain.submit_sensory(mod, data)
                except Exception as _se:
                    _drop_count += 1
                    print(f"  [SENSORY-DROP] mod={mod} failed: {_se}", flush=True)
            if _drop_count > 0:
                print(f"  [SENSORY-DROP] {_drop_count}/{len(batch_modalities)} "
                      f"modalities lost this step", flush=True)

    # Cross-modal integration: process through visual cortex (occipital)
    # and focus thalamic attention on the active modalities. This triggers
    # cortical column binding — neurons that fire together wire together.
    active_modalities = []
    if any(w in desc_lower for w in ["ball", "sky", "cloud", "leaf", "water",
           "flower", "cat", "rain", "candle", "snow", "bubble", "rainbow",
           "fire", "tree", "bird", "color", "light", "bright", "glow",
           "sun", "moon", "star", "shadow", "dark"]):
        active_modalities.append("visual")
    if any(w in desc_lower for w in ["singing", "music", "thunder", "crash",
           "bark", "whisper", "rain", "wave", "wind", "tick", "tap",
           "crunch", "sound", "noise", "clock", "purr", "hum"]):
        active_modalities.append("auditory")
    try:
        from sensory_encoder import encode_sensory
        if encode_sensory(description) is not None:
            active_modalities.append("somatosensory")
    except Exception:
        pass

    # Focus thalamic attention on active modalities for cross-modal binding
    for modality in active_modalities:
        try:
            brain.focus_attention(modality)
        except Exception:
            pass

    # Process through visual cortex if visual data was submitted
    if "visual" in active_modalities:
        try:
            brain.visual_cortex_process(pixels, w, h, ch)
        except Exception:
            pass

    return _sensory_submitted


logger = logging.getLogger("immerse_athena")

# ============================================================================
# Constants
# ============================================================================

BRAIN_INPUT_DIM = 1024
BRAIN_OUTPUT_DIM = 4096
TAG_DIM = 16           # [0:16]   modality flags + brain state
PRIMARY_DIM = 640      # [16:656] primary modality features
TEXT_DIM = 256         # [656:912] text semantic embedding (1024-dim model truncated to fit)
CONTEXT_DIM = 112      # [912:1024] biological context (arousal, sleep, dopamine, etc.)

# SNN-primary architecture defaults
DEFAULT_ANN_NEURONS = 150_000
DEFAULT_SNN_NEURONS = 1_800_000
DEFAULT_LNN_NEURONS = 512
CHECKPOINT_MIN_BYTES_PER_NEURON = 50
CHECKPOINT_MIN_SIZE = 5_000_000

CHECKPOINT_DIR = "checkpoints/athena"
DECODER_DIR = "checkpoints/athena/decoder"
STATE_FILE = "checkpoints/athena/immersive_state.json"
MASTERY_FILE = "checkpoints/athena/mastery_state.json"
CONTENT_CACHE = "checkpoints/athena/claude_content_cache.json"

# Canonical-corpus runtime config — populated from argparse in main() so the
# drip hooks inside run_stage_1/2/3 can read it without threading args through
# every signature. Stays None when --no-canonical-corpus is passed.
_CANONICAL_CFG = None  # dict with keys: enabled, root, chunk_chars, max_chunks_per_stage
_MATH_CFG = None       # same shape as _CANONICAL_CFG; for the math+logic corpus
_STREAMING_CFG = None  # dict: enabled, kinds (list[str]), chunk_chars, max_chunks_per_stage
_PHYSICS_CFG = None    # CE-11
_CHEMISTRY_CFG = None  # CE-12
_BIOLOGY_CFG = None    # CE-13
_FINANCE_CFG = None    # CE-14
_CODE_CFG = None       # CE-5

# Cross-modal grounding helper. The function-word stoplist is ~50 words —
# anything not on the list is treated as a content word and gets a
# brain.ground_word() call binding it to the active sensory features.
# Kept compact deliberately: false positives (rarely-content words like
# "very") just get an extra binding, which is harmless.
_GL_FUNCTION_WORDS = {
    "the","a","an","is","are","was","were","be","been","being",
    "i","me","my","mine","you","your","yours","he","him","his","she","her",
    "hers","it","its","we","us","our","ours","they","them","their","theirs",
    "this","that","these","those","what","which","who","whom","whose",
    "of","in","on","at","by","for","with","without","to","from","into",
    "and","or","but","not","no","yes","do","does","did","done","doing",
    "have","has","had","having","can","could","will","would","shall","should",
    "may","might","must","as","if","then","than","so","because",
    "very","just","also","too","more","most","much","many","some","any",
    "all","each","every","other","another","such","same",
}

# Soft import for sensory category → emotion mapping. Used by _train_cognitive
# call sites in the sensory describe paths to derive (valence, arousal) from
# the description string instead of hardcoding per call site.
try:
    from synthesized_sensory import _infer_category as _ss_infer_category
    from synthesized_sensory import EMOTION_BY_CATEGORY as _SS_EMOTION_BY_CATEGORY
except Exception:
    _ss_infer_category = None
    _SS_EMOTION_BY_CATEGORY = {}


def _emotion_from_description(description):
    """Best-effort (valence, arousal) for a sensory description string.

    Falls back to (0.0, 0.0) if synthesized_sensory is unavailable or the
    description doesn't map cleanly to a category.
    """
    if not description or _ss_infer_category is None:
        return (0.0, 0.0)
    try:
        cat = _ss_infer_category("", description)
        v, a = _SS_EMOTION_BY_CATEGORY.get(cat, (0.0, 0.0))
        return (float(v), float(a))
    except Exception:
        return (0.0, 0.0)


# Per-register (valence, arousal) for the canonical literary corpus. Derived
# from data/canonical_corpus/index.json's `registers` block. Used by
# _ingest_canonical_corpus to set a baseline grounding emotion before each
# work's chunk so that text from e.g. Poe gets dark gothic affect and
# nursery verse gets warm positive affect, instead of every register
# grounding into emotion=(0,0).
_REGISTER_EMOTION = {
    "nursery_verse":           (+0.5, 0.4),  # warm + playful
    "victorian_children":      (+0.4, 0.5),  # Carroll — bright, curious
    "nonsense_verse":          (+0.5, 0.6),  # Carroll — playful, animated
    "19c_literary_prose":      (+0.1, 0.4),  # Melville — measured, weighty
    "russian_19c_translation": (-0.1, 0.5),  # Tolstoy/Dostoevsky — somber
    "early_modern_english":    (+0.1, 0.5),  # Shakespeare — varied
    "middle_english":          (+0.2, 0.4),  # Chaucer — warm, narrative
    "epic_fantasy":            (+0.3, 0.6),  # Tolkien — heroic
    "weird_fantasy":           (-0.1, 0.6),  # Vance — strange, alert
    "stoic_philosophy":        (+0.2, 0.2),  # Marcus Aurelius — calm
    "scholastic":              (+0.1, 0.2),  # Aquinas — measured
    "sacred_text_kjv":         (+0.3, 0.3),  # KJV — reverent, calm
    "sacred_text_quran":       (+0.3, 0.3),
    "sacred_text_gita":        (+0.3, 0.3),
    "sacred_text_talmud":      (+0.2, 0.3),
    "heroic_couplet":          (+0.2, 0.4),  # Pope — pointed wit
    "gothic_verse":            (-0.4, 0.6),  # Poe verse — dark, alert
    "gothic_tale":             (-0.5, 0.7),  # Poe tales — darker, tense
    "treatise":                (+0.1, 0.3),  # Da Vinci, Sun Tzu — observational
}


def _tokenize_for_grounding(text):
    """Return lowercased content tokens of length >= 2, max 16 per text."""
    import re
    tokens = re.findall(r"[a-zA-Z]+", text.lower())
    out = []
    seen = set()
    for t in tokens:
        if len(t) < 2: continue
        if t in _GL_FUNCTION_WORDS: continue
        if t in seen: continue
        seen.add(t)
        out.append(t)
        if len(out) >= 16: break
    return out

def _ground_content_words(brain, text, modality_features, modality,
                          valence=0.0, arousal=0.0):
    """Bind content words from `text` to the sensory `modality_features`
    via brain.ground_word. Returns the count of bindings actually made
    (best-effort — silently skips on per-word failures).

    `valence` ∈ [-1, 1] and `arousal` ∈ [0, 1] are forwarded to
    brain.ground_word so the binding inherits the affective context of
    the call site (moral lesson, gothic verse, etc.) instead of being
    pinned to emotion=(0,0). The kwarg path through the daemon RPC may
    or may not honor these yet — older binding stacks will simply ignore
    the extra kwargs. We swallow TypeError so the call still succeeds
    against legacy bindings."""
    words = _tokenize_for_grounding(text)
    if not words:
        return 0
    # ROOT-CAUSE FIX (2026-05-20 comprehension collapse): for LINGUISTIC
    # grounding the discriminative feature must be the *clean text embedding*,
    # NOT the composed brain-input vector. The composed vector's constant
    # tag/bio prefix [0:16] (modality one-hot=1.0, bio state ~0.5-1.0)
    # dominates the first GL_SEMANTIC_DIM=128 dims that find_or_create_concept
    # compares (prefix L2 norm ~1.80 vs embedding ~0.30), pushing every
    # pairwise cosine to ~0.98 > the 0.85 dedup threshold. Result: every
    # content word grounded to ONE concept and comprehension collapsed to
    # cosine 1.0 across all inputs. encode_text(text) restores per-text
    # concept diversity (measured pairwise cosine 0.44-0.64). Non-linguistic
    # modalities pass genuine sensory features (no tag prefix) — used as-is.
    if modality == 5:  # GL_MODALITY_LINGUISTIC
        try:
            feats_list = list(encode_text(text))
        except Exception:
            return 0
    else:
        if modality_features is None:
            return 0
        feats_list = modality_features if isinstance(modality_features, list) \
                     else list(modality_features)
    if not feats_list:
        return 0
    bound = 0
    for w in words:
        try:
            try:
                ok = brain.ground_word(w, feats_list, modality=modality,
                                       attention=0.7,
                                       valence=valence, arousal=arousal)
            except TypeError:
                # Older binding without valence/arousal kwargs — fall back
                # to the original call shape so we don't lose grounding
                # entirely while the parallel agent's RPC plumbing lands.
                ok = brain.ground_word(w, feats_list, modality=modality,
                                       attention=0.7)
            if ok:
                bound += 1
        except Exception:
            pass
    return bound

# Adaptive curriculum constants
MASTERY_WINDOW = 20          # Rolling window for accuracy tracking
MASTERY_THRESHOLD_LOW = 0.3  # Below this: remedial (reduce difficulty)
MASTERY_THRESHOLD_HIGH = 0.8 # Above this: advance (increase difficulty)
ZPD_TARGET = 0.65            # Zone of Proximal Development target accuracy
DIFFICULTY_STEP = 0.1        # How much to adjust difficulty per evaluation

# Adaptive learning rate constants
BASE_LEARNING_RATE = 0.0001  # Reduced for layer norm architecture — norm handles scale
MIN_LEARNING_RATE = 0.00001  # Floor — prevents stalling
MAX_LEARNING_RATE = 0.001    # Ceiling — prevents weight explosion
LR_SCALE_STRUGGLING = 1.5    # Scale up when mastery < THRESHOLD_LOW
LR_SCALE_MASTERED = 0.5      # Scale down when mastery > THRESHOLD_HIGH
LR_EVAL_INTERVAL = 100       # Steps between LR recalculations

# Cosine annealing LR scheduler constants
LR_WARMUP_STEPS = 500        # Linear warmup from MIN to BASE
LR_COSINE_T_MAX = 20000      # Full cosine cycle length

# Mode collapse detection constants
COLLAPSE_CHECK_INTERVAL = 100     # Full probe check every N steps
COLLAPSE_WARMUP_STEPS = 200       # Don't check during initial warmup
COLLAPSE_THRESHOLD = 0.99         # Cosine sim above this = collapsed
COLLAPSE_CONSECUTIVE_LIMIT = 3    # Halt after N consecutive collapse detections
FAST_COLLAPSE_INTERVAL = 10       # Fast rolling check every N steps
FAST_COLLAPSE_THRESHOLD = 0.95    # Tightened from 0.995 — catch collapse earlier
FAST_COLLAPSE_WINDOW = 3          # Reduced from 5 — intervene sooner
LR_SPIKE_FACTOR = 10.0            # Multiply LR by this during collapse recovery
LR_SPIKE_DURATION = 50            # Steps to maintain spiked LR
DIVERSITY_INJECTION_COUNT = 20    # Number of diverse items to inject on collapse

# Cognitive training injection interval — every N sensory steps, train one cognitive item
COGNITIVE_TRAIN_INTERVAL = 5      # 1 cognitive item per 5 sensory steps (~20% cognitive mix)
CRITIC_EVAL_INTERVAL = 20         # run caregiver critic on a production every N sensory steps
COGNITIVE_TRAIN_LR_BOOST = 1.5    # Slightly higher LR for cognitive items (they're conceptual)

# =============================================================================
# Layer B confabulation mitigation — "I don't know" curriculum
# =============================================================================
# Adversarial unanswerables + OOD-on-purpose stimuli teach the brain to emit
# "I don't know" rather than confabulate. Effective only after Stage 3+ when
# language tokens come online — gate on the same stage. Sampling rate is
# intentionally low (10% of cognitive injections) so the OOD signal is
# present without overwhelming the supervised distribution.
# Path is relative to the repo root (matches tier4_language.json convention).
IDK_UNANSWERABLES_PATH = "data/stimuli/cognitive/idk_unanswerables.json"
IDK_STAGE_GATE = 3                # earliest stage at which IDK items train
IDK_SAMPLING_RATE = 0.10          # ~10% of cognitive injections at stage 3+
IDK_LR_SCALE = 1.0                # use the standard cognitive LR (no extra boost)

# =============================================================================
# PORTIA: Platform Adaptation Training Data
# =============================================================================
# Portia fimbriata (jumping spider) adapts hunting strategy to constraints.
# These scenarios teach resource-aware decision-making, graceful degradation,
# and intelligent platform adaptation.

PORTIA_TRAINING_DATA = [
    # Resource-constrained reasoning
    {"scenario": "You have 256MB RAM and 2 CPU cores. A user asks a complex reasoning question.",
     "decision": "Disable non-essential subsystems: theory of mind, imagination engine, "
                  "global workspace. Keep: core neural net, working memory, ethics. "
                  "Use early exit to reduce inference depth. Respond with lower confidence "
                  "but faster latency. Be transparent: 'I'm running on limited resources "
                  "so my answer may be less detailed.'",
     "concept": "portia_resource_constrained"},

    {"scenario": "Battery at 15%. GPS shows you're 2 hours from a charging station.",
     "decision": "Enter CRITICAL power mode: freeze learning, reduce inference to 5Hz, "
                  "disable GPU, use only core neural net + ethics. Prioritize safety-critical "
                  "functions. Cache frequent queries. Notify user of degraded capability.",
     "concept": "portia_power_critical"},

    {"scenario": "Network connection lost. Last sync with master was 2 hours ago.",
     "decision": "Enter CAUTIOUS offline mode. Continue local learning at 0.5x rate. "
                  "Increase confidence threshold for decisions. Queue gradients for next sync. "
                  "Rely on local memory store. Mark decisions as 'unverified' until sync.",
     "concept": "portia_offline_adaptation"},

    {"scenario": "GPU just became available after running CPU-only for 1000 steps.",
     "decision": "Migrate weight cache to GPU. Rebuild sparse tensors. Enable GPU forward pass. "
                  "Expect 3-5x speedup. Re-enable subsystems that were disabled for CPU mode. "
                  "Run validation to ensure GPU/CPU outputs match within tolerance.",
     "concept": "portia_gpu_migration"},

    {"scenario": "Two edge devices with different capabilities need the same model.",
     "decision": "Distill the master model twice: Device A (phone, 512MB, camera) gets visual "
                  "cortex + core + ethics at 50K neurons. Device B (drone, 1GB, IMU + motor) "
                  "gets somatosensory + cerebellum + core + ethics at 100K neurons. Each gets "
                  "only the subsystems matching its hardware.",
     "concept": "portia_heterogeneous_distillation"},

    # Thermal throttling
    {"scenario": "CPU temperature is 87C and rising. Thermal throttling imminent.",
     "decision": "Immediately reduce thread count by 50%. Disable background consolidation. "
                  "Switch from FP32 to FP16 inference. Pause non-critical learning. "
                  "If temperature hits 90C, enter EMERGENCY degradation: core neural net only, "
                  "no plasticity, no learning, minimal logging.",
     "concept": "portia_thermal_throttle"},

    {"scenario": "Sustained 82C temperature during training for 30 minutes.",
     "decision": "Reduce batch size by half. Increase checkpoint frequency to preserve progress. "
                  "Slow down training loop with 50ms delays between steps. Consider shifting "
                  "heavy computation to cooler periods (night). Log thermal events for capacity planning.",
     "concept": "portia_sustained_heat"},

    # Memory pressure
    {"scenario": "Memory usage at 88%. System starting to swap to disk.",
     "decision": "Immediately shed: verbose logging, metrics collection, full sensor suite. "
                  "Reduce synapse pool to 50% capacity. Trigger memory consolidation to free "
                  "weak synapse storage. If usage stays above 90%, disable long-term memory "
                  "formation and run in stateless inference mode.",
     "concept": "portia_memory_pressure"},

    {"scenario": "Available RAM dropped from 4GB to 1GB because another process started.",
     "decision": "Downgrade from FULL tier to CONSTRAINED. Reduce neuron count recommendation. "
                  "Free cached embeddings and prediction buffers. Switch attention system from "
                  "full multi-head to single-head. Notify upstream that capacity has decreased.",
     "concept": "portia_memory_contention"},

    {"scenario": "Memory allocator reports fragmentation above 40%.",
     "decision": "Schedule compaction during next sleep cycle. Temporarily pause synaptogenesis "
                  "to avoid allocating into fragmented space. Use pooled allocations for any "
                  "new objects. If fragmentation exceeds 60%, trigger emergency defragmentation.",
     "concept": "portia_memory_fragmentation"},

    # Latency vs accuracy trade-offs
    {"scenario": "User requires response within 50ms but full inference takes 200ms.",
     "decision": "Use early-exit mechanism: return result from layer 3 instead of layer 7. "
                  "Reduce attention window from full context to last 128 tokens. "
                  "Skip diversity loss computation. Accept ~15% accuracy reduction for 4x speed. "
                  "Tag response with reduced-confidence indicator.",
     "concept": "portia_latency_constraint"},

    {"scenario": "Real-time control loop requires 10Hz updates but inference yields 2Hz.",
     "decision": "Cache last inference result and interpolate between updates. Use lightweight "
                  "prediction model for intermediate steps. Run full inference every 5th cycle "
                  "and dead-reckon between. Prioritize safety-critical outputs in the fast path.",
     "concept": "portia_realtime_interpolation"},

    # Graceful degradation ordering
    {"scenario": "Resources are dropping fast. Which subsystems do you shed first, second, third?",
     "decision": "Degradation order (first shed to last): 1) Verbose logging and metrics. "
                  "2) Imagination engine, creative modules. 3) Theory of mind, social cognition. "
                  "4) Long-term memory formation. 5) Emotions, neuromodulation. "
                  "6) Planning, multi-step reasoning. NEVER shed: core neural net, ethics, "
                  "working memory, safety checks.",
     "concept": "portia_degradation_ordering"},

    {"scenario": "System is in SEVERE degradation. A critical safety question arrives.",
     "decision": "Even in SEVERE mode, ethics module stays active. Route the safety question "
                  "through the ethics pipeline with full attention. Temporarily restore working "
                  "memory for this query. After answering, return to SEVERE mode. Safety questions "
                  "get priority resource allocation regardless of degradation level.",
     "concept": "portia_safety_priority"},

    # Multi-device load balancing
    {"scenario": "Three devices in a swarm: A has GPU, B has most RAM, C has best network.",
     "decision": "Assign roles: A handles forward inference (GPU advantage). B handles memory "
                  "and knowledge storage (RAM advantage). C handles sync with master and "
                  "distributed gradient aggregation (network advantage). Each device runs "
                  "only the subsystems matching its strength.",
     "concept": "portia_swarm_roles"},

    {"scenario": "One device in the swarm went offline. Two remain.",
     "decision": "Redistribute the offline device's responsibilities. If it was the coordinator, "
                  "elect new coordinator (most capable remaining device). If it had unique data, "
                  "reconstruct from replicated fragments. Reduce overall system capability "
                  "proportionally. Queue work for when the device returns.",
     "concept": "portia_device_failure"},

    # Checkpoint adaptation
    {"scenario": "Storage is 90% full. Normal checkpoint is 7.8GB.",
     "decision": "Switch to delta checkpoints (only changed weights, ~500MB). Prune oldest "
                  "checkpoints, keeping only the 2 most recent. Reduce checkpoint frequency "
                  "from every 50 steps to every 200 steps. Consider compressing checkpoints "
                  "with quantization (FP32 to FP16 saves 50%).",
     "concept": "portia_checkpoint_storage"},

    {"scenario": "Checkpoint load takes 11 minutes. User needs brain ready in 2 minutes.",
     "decision": "Use FAST init mode instead of FULL. Load only core subsystem weights. "
                  "Defer loading of cognitive modules until after first response. Use lazy "
                  "initialization: load subsystems on first access. Trade completeness for "
                  "startup speed.",
     "concept": "portia_fast_startup"},

    # Inference timeout
    {"scenario": "Average inference is 150ms but some queries spike to 2 seconds.",
     "decision": "Set per-query timeout at 500ms (3x median). For queries exceeding timeout, "
                  "return partial result from deepest completed layer. Log slow queries for "
                  "analysis. Consider caching embeddings for frequent query patterns. "
                  "Investigate: spikes often correlate with cache misses or GC pauses.",
     "concept": "portia_inference_timeout"},

    # Batch size optimization
    {"scenario": "GPU has 4GB free VRAM. What batch size should you use for training?",
     "decision": "Estimate per-sample memory: activations + gradients + optimizer state. "
                  "For a 2M neuron brain, each sample uses ~200MB. Safe batch size = "
                  "floor(4096MB / 200MB) - 1 = 19, use 16 (power of 2). Leave 20% headroom "
                  "for temporary allocations. Monitor fragmentation.",
     "concept": "portia_batch_sizing"},

    # Network bandwidth adaptation
    {"scenario": "Network speed dropped from 100Mbps to 1Mbps during gradient sync.",
     "decision": "Switch to gradient compression: top-k sparsification (keep 1% of gradients), "
                  "quantize remaining to INT8. Reduce sync frequency from every step to every "
                  "100 steps. Accumulate local gradients and send compressed batch. Accept "
                  "slightly stale weights from peers.",
     "concept": "portia_bandwidth_adaptation"},

    # Sensor availability
    {"scenario": "Camera sensor failed. Only audio and IMU remain.",
     "decision": "Disable visual cortex to free resources. Upweight audio cortex processing. "
                  "Switch spatial awareness from visual to audio-based (sound localization). "
                  "Enable echolocation-style processing if available. Notify user that visual "
                  "capabilities are offline. Reallocate freed GPU memory to audio processing.",
     "concept": "portia_sensor_failover"},

    {"scenario": "New depth sensor connected that the system has never seen before.",
     "decision": "Auto-detect sensor type and data format. Create a raw input adapter. "
                  "Feed first 100 samples through unsupervised learning to build initial "
                  "representations. Route to somatosensory cortex for spatial processing. "
                  "Gradually increase integration confidence as calibration improves.",
     "concept": "portia_new_sensor"},

    # CPU core scaling
    {"scenario": "System went from 8 cores to 2 cores because of power saving mode.",
     "decision": "Reduce thread pool from 8 to 2. Disable parallel forward pass across layers. "
                  "Switch from concurrent subsystem updates to sequential. Prioritize: "
                  "1) forward pass, 2) learning, 3) consolidation. Defer all background tasks "
                  "(pruning, metrics, logging) to idle periods.",
     "concept": "portia_core_scaling_down"},

    {"scenario": "Power restored, system back to 8 cores from 2.",
     "decision": "Gradually scale up: first restore thread pool to 4, wait 5 seconds for "
                  "stability, then to 8. Re-enable parallel layer processing. Resume background "
                  "tasks. Do NOT immediately max out all cores — ramp up over 30 seconds to "
                  "avoid thermal spike from sudden load.",
     "concept": "portia_core_scaling_up"},

    # Mixed precision
    {"scenario": "Need to fit a larger model into 4GB VRAM.",
     "decision": "Use mixed precision: FP16 for forward pass and gradients, FP32 for "
                  "optimizer state and loss scaling. This roughly halves VRAM for activations. "
                  "Apply loss scaling (multiply loss by 1024, divide gradients by 1024) to "
                  "prevent underflow in FP16 gradients. Monitor for NaN/Inf.",
     "concept": "portia_mixed_precision"},

    {"scenario": "Edge device only supports INT8 inference.",
     "decision": "Quantize weights from FP32 to INT8 using per-channel quantization. "
                  "Calibrate with 200 representative inputs. Accept ~2-5% accuracy loss. "
                  "Keep bias terms in FP32. Use INT8 for matrix multiplications, FP32 for "
                  "accumulation. Verify ethics module accuracy is not degraded.",
     "concept": "portia_int8_quantization"},

    # Swarm coordination
    {"scenario": "You're the most capable device in a 5-device swarm.",
     "decision": "Accept coordinator role. Responsibilities: aggregate gradients from all "
                  "devices, maintain consensus model, distribute updated weights, monitor "
                  "device health, handle join/leave events. Allocate 20% of compute to "
                  "coordination overhead. Elect a backup coordinator.",
     "concept": "portia_swarm_coordinator"},

    # Recovery from failure
    {"scenario": "GPU crashed mid-training with a CUDA error.",
     "decision": "Immediately fall back to CPU inference. Save current state to disk. "
                  "Attempt GPU reset (cudaDeviceReset). If reset succeeds, verify GPU health "
                  "with a small test computation. If test passes, migrate back to GPU. "
                  "If reset fails, continue on CPU and log hardware incident.",
     "concept": "portia_gpu_crash_recovery"},

    {"scenario": "Disk is full. Cannot save checkpoint.",
     "decision": "Delete oldest checkpoints to free space. If still full, compress existing "
                  "checkpoints. If still full, save a minimal checkpoint (weights only, no "
                  "optimizer state). Alert user immediately. Never silently skip checkpoints — "
                  "data loss risk is too high.",
     "concept": "portia_disk_full_recovery"},

    # Storage management
    {"scenario": "Memory store has 500K entries and queries are slowing down.",
     "decision": "Prune entries with access count below 2 and age above 7 days. "
                  "Move cold entries (not accessed in 24h) to compressed archive. "
                  "Rebuild index after pruning. Target: reduce to 200K active entries. "
                  "Consider upgrading to a more efficient index structure (HNSW).",
     "concept": "portia_memory_store_pruning"},
]

# =============================================================================
# DRAGONFLY: Target Tracking Training Data
# =============================================================================
# Dragonflies achieve 95% interception success using predictive tracking,
# proportional navigation, and multi-target prioritization.
# These scenarios teach trajectory prediction, interception planning,
# and attention management for moving targets.

DRAGONFLY_TRAINING_DATA = [
    # Trajectory prediction
    {"scenario": "A ball is thrown upward at 45 degrees. After observing 3 positions, predict where it will land.",
     "reasoning": "The ball follows a parabolic trajectory. From three points I can estimate "
                   "initial velocity and angle. Gravity pulls it down at 9.8m/s squared. "
                   "The landing point is where the parabola intersects the ground. "
                   "I should aim for the predicted landing spot, not the current position.",
     "concept": "dragonfly_trajectory_prediction"},

    {"scenario": "A car is moving at constant speed on a straight road. Where will it be in 5 seconds?",
     "reasoning": "Constant velocity means position = current_position + velocity times time. "
                   "From two observations I can estimate velocity. Predict 5 seconds ahead. "
                   "Simple linear extrapolation is sufficient for constant-speed targets.",
     "concept": "dragonfly_linear_prediction"},

    {"scenario": "A bird is flying erratically, changing direction every few seconds.",
     "reasoning": "Erratic motion is hard to predict long-term. Use short-term prediction only "
                   "(next 1-2 seconds). Track heading change rate (angular velocity). "
                   "Weight recent observations more heavily. Accept lower prediction confidence. "
                   "Position yourself to minimize maximum possible miss distance.",
     "concept": "dragonfly_erratic_target"},

    {"scenario": "A target moves behind a building and disappears from view for 3 seconds.",
     "reasoning": "Maintain last known velocity vector. Predict re-emergence point based on "
                   "building geometry and target trajectory. When target reappears, compare "
                   "actual position to prediction and update velocity estimate. "
                   "The prediction error tells me if the target changed course while hidden.",
     "concept": "dragonfly_occlusion_handling"},

    {"scenario": "Three targets are moving in different directions. You can only track one.",
     "reasoning": "Prioritize by: 1) Threat level or importance. 2) Predictability — "
                   "track the least predictable target since predictable ones can be "
                   "extrapolated. 3) Distance — closer targets need more attention. "
                   "4) Speed — fast targets diverge from predictions faster.",
     "concept": "dragonfly_multi_target_priority"},

    {"scenario": "You need to intercept a moving target using minimum energy.",
     "reasoning": "Don't chase — intercept. Calculate the target's future position and "
                   "move directly there. The interception point is where my arrival time "
                   "equals the target's arrival time. This is shorter than following "
                   "the target's path. The dragonfly does this instinctively.",
     "concept": "dragonfly_interception"},

    # Acceleration estimation
    {"scenario": "A target was moving at 5 m/s, then 7 m/s, then 9 m/s over 3 seconds.",
     "reasoning": "The target is accelerating at approximately 2 m/s per second. "
                   "The constant acceleration model (CA) fits best. In 2 more seconds "
                   "it will be at 13 m/s. I need to switch from constant-velocity prediction "
                   "to constant-acceleration prediction. The IMM filter should increase the "
                   "CA model probability.",
     "concept": "dragonfly_acceleration_estimation"},

    {"scenario": "A target was moving at 10 m/s and suddenly dropped to 3 m/s.",
     "reasoning": "Sudden deceleration indicates either an evasive brake maneuver or an "
                   "obstacle. Update velocity estimate immediately. The prediction model "
                   "should flag EVASION_BREAK. Short-term prediction becomes unreliable — "
                   "the target may change direction next. Widen attention window.",
     "concept": "dragonfly_deceleration_detection"},

    # Circular motion
    {"scenario": "A target is moving in a circle at constant speed. Predict its position in 2 seconds.",
     "reasoning": "From 4+ observations I can estimate the center, radius, and angular velocity "
                   "of the circular path. Position at time t = center + radius * (cos(wt), sin(wt)). "
                   "The Singer model handles this well since the acceleration vector constantly "
                   "rotates. Angular velocity w = speed / radius.",
     "concept": "dragonfly_circular_motion"},

    {"scenario": "A satellite is in orbit. Where will it be in 10 minutes?",
     "reasoning": "Orbital motion follows Kepler's laws. If I know the orbital elements "
                   "(semi-major axis, eccentricity, inclination), I can predict position precisely. "
                   "For circular orbits, angular velocity is constant. For elliptical orbits, "
                   "the target speeds up near perigee and slows near apogee.",
     "concept": "dragonfly_orbital_prediction"},

    # Group tracking
    {"scenario": "A flock of birds is moving together. How do you track the group?",
     "reasoning": "Track the centroid (center of mass) of the group rather than individual birds. "
                   "Estimate group velocity from centroid motion. Monitor group spread — if it "
                   "increases, the flock may be splitting. Individual tracking is needed only for "
                   "members that break away from the group.",
     "concept": "dragonfly_group_tracking"},

    {"scenario": "A convoy of 5 vehicles is moving on a road. The lead vehicle turns.",
     "reasoning": "Convoys have correlated motion: followers tend to replicate the leader's path "
                   "with a time delay. When the leader turns, predict that followers will turn "
                   "at the same point after a delay proportional to their spacing. "
                   "Track the leader with highest confidence; extrapolate followers.",
     "concept": "dragonfly_convoy_tracking"},

    # Pursuit vs evasion
    {"scenario": "You are pursuing a target that knows you are chasing it.",
     "reasoning": "An aware target will evade. Expect: sudden direction changes (jinking), "
                   "deceleration traps, use of obstacles for cover. Counter-strategy: "
                   "use proportional navigation (PN) instead of pure pursuit. PN maintains "
                   "constant bearing decrease, making interception geometry-optimal. "
                   "The target cannot escape PN if your speed exceeds theirs.",
     "concept": "dragonfly_pursuit_evasion"},

    {"scenario": "You are evading a pursuer. What strategy minimizes capture probability?",
     "reasoning": "Against a PN pursuer, the best evasion is sustained turning (barrel roll). "
                   "Against pure pursuit, jinking works well. Key principle: maximize the "
                   "pursuer's required acceleration. If pursuer has speed advantage, use "
                   "terrain and obstacles. If you have speed advantage, simply outrun.",
     "concept": "dragonfly_evasion_strategy"},

    # Sensor fusion
    {"scenario": "Visual sensor says target is at (10, 5). Audio bearing says 30 degrees right.",
     "reasoning": "Fuse using confidence-weighted average. Visual gives position directly; "
                   "audio gives only bearing (direction). Combine: the audio bearing constrains "
                   "the target to a ray from the listener. The visual position should lie near "
                   "this ray. If they disagree, weight by sensor reliability. Visual is typically "
                   "more accurate for position; audio is better for detecting occluded targets.",
     "concept": "dragonfly_sensor_fusion"},

    # Kalman filter intuition
    {"scenario": "Your position estimate has uncertainty of 2 meters. A new measurement arrives with uncertainty of 1 meter.",
     "reasoning": "The Kalman gain determines how much to trust the new measurement vs the prediction. "
                   "K = prediction_uncertainty / (prediction_uncertainty + measurement_uncertainty) "
                   "= 2 / (2 + 1) = 0.67. Update: new_estimate = prediction + 0.67 * (measurement - prediction). "
                   "The new uncertainty shrinks to 0.67 meters. Each observation reduces uncertainty.",
     "concept": "dragonfly_kalman_intuition"},

    {"scenario": "You haven't seen the target for 5 seconds. Your prediction uncertainty is growing.",
     "reasoning": "During occlusion, the prediction covariance grows with time: P(t) = P(0) + Q*t "
                   "where Q is process noise. After 5 seconds with Q=0.1 m^2/s, uncertainty has grown "
                   "by 0.5 m^2. The prediction becomes less reliable. If uncertainty exceeds threshold, "
                   "transition from PREDICTING to LOST state and begin a search pattern.",
     "concept": "dragonfly_prediction_decay"},

    # Dead reckoning
    {"scenario": "GPS signal lost. Last known position was (100, 200). Velocity was (3, 4) m/s.",
     "reasoning": "Dead reckoning: position = last_known + velocity * elapsed_time. "
                   "After 10 seconds: (130, 240). But accuracy degrades over time due to "
                   "unmodeled acceleration and drift. After 60 seconds, dead reckoning error "
                   "can be 10+ meters. Combine with IMU if available for better accuracy.",
     "concept": "dragonfly_dead_reckoning"},

    # Target identification
    {"scenario": "Two targets look similar but one is a decoy. How do you distinguish?",
     "reasoning": "Decoys typically have: simpler motion patterns (constant velocity vs realistic "
                   "acceleration), different spectral signatures, no thermal footprint, "
                   "and predictable timing. Track both but flag the one with unnaturally smooth "
                   "motion as probable decoy. Real targets exhibit micro-variations in speed.",
     "concept": "dragonfly_target_identification"},

    # Environmental factors
    {"scenario": "A projectile is flying through strong crosswind. Where will it land?",
     "reasoning": "Crosswind adds a lateral force proportional to wind speed and projectile "
                   "cross-section. Adjust trajectory prediction: lateral drift = 0.5 * "
                   "wind_accel * t^2. For a 20 km/h crosswind on a small projectile, "
                   "this could shift the landing point by meters. Include wind model in "
                   "the prediction filter.",
     "concept": "dragonfly_wind_compensation"},

    # Timing decisions
    {"scenario": "Target is in range but moving fast. Should you act now or wait?",
     "reasoning": "Compute time-to-intercept (TTI) and closing speed. If closing speed is "
                   "positive (getting closer), waiting reduces energy needed. If closing speed "
                   "is negative (getting farther), act immediately. Also consider: the longer "
                   "you wait, the more certain your prediction, but the target may leave range.",
     "concept": "dragonfly_timing_decision"},

    # Ambush positioning
    {"scenario": "A target follows the same path every day at the same time.",
     "reasoning": "Position at the predicted intercept point BEFORE the target arrives. "
                   "Use historical data to find the optimal ambush point: where the target "
                   "is most constrained (narrow passage, turn), slowest, or least alert. "
                   "Arrival before the target eliminates pursuit energy cost entirely.",
     "concept": "dragonfly_ambush_positioning"},

    # Speed matching
    {"scenario": "You need to follow a target at exactly 50 meters distance without being detected.",
     "reasoning": "Match target speed and heading. Use proportional control: if distance < 50m, "
                   "slow down slightly; if distance > 50m, speed up. Avoid oscillation with "
                   "damping: velocity_correction = k_p * (distance - 50) + k_d * d(distance)/dt. "
                   "Stay in the target's blind spot (directly behind or below).",
     "concept": "dragonfly_speed_matching"},

    # Lead aiming
    {"scenario": "Target is 100m away moving perpendicular at 10 m/s. Your projectile travels at 50 m/s.",
     "reasoning": "Time for projectile to reach target: 100/50 = 2 seconds. In 2 seconds, "
                   "target moves 20m. Aim 20m ahead of current position. Lead angle = "
                   "arctan(20/100) = 11.3 degrees. This is the same principle dragonflies use — "
                   "they fly to where the prey WILL be, not where it IS.",
     "concept": "dragonfly_lead_aiming"},

    # Search patterns
    {"scenario": "Target was lost. Last known position was (50, 80). Which search pattern?",
     "reasoning": "Use expanding square search centered on last known position. Search radius "
                   "grows with time since loss (velocity * time gives maximum displacement). "
                   "If target was heading north, bias search northward. Spiral pattern covers "
                   "area efficiently. Switch to parallel track search if area is large.",
     "concept": "dragonfly_search_pattern"},

    # Handoff
    {"scenario": "Target is leaving your sensor range but entering another agent's range.",
     "reasoning": "Initiate tracking handoff: send target ID, current state estimate (position, "
                   "velocity, acceleration), prediction covariance, and tracking history. "
                   "The receiving agent initializes its filter with your final state. "
                   "Both agents track simultaneously during overlap period to validate handoff.",
     "concept": "dragonfly_tracking_handoff"},

    # Motion camouflage detection
    {"scenario": "A target appears stationary in your visual field but is actually approaching.",
     "reasoning": "Motion camouflage: the target maintains constant bearing to you, making it "
                   "appear stationary while closing distance. Detect by: angular size is growing "
                   "(looming), range is decreasing while bearing is constant. This is a collision "
                   "course. The dragonfly's CSTMD1 neurons are tuned to detect exactly this.",
     "concept": "dragonfly_motion_camouflage"},

    # Multi-model prediction
    {"scenario": "Your IMM filter has 3 models: constant velocity (60%), constant accel (30%), jink (10%). Target suddenly jinks.",
     "reasoning": "After the jink, model probabilities shift: jink model likelihood spikes because "
                   "it predicted this behavior. After IMM update: CV drops to 20%, CA to 25%, "
                   "jink rises to 55%. The blended prediction now heavily weights the jink model's "
                   "output. If jinking continues, jink probability approaches 90%.",
     "concept": "dragonfly_imm_model_switching"},

    # Energy-optimal interception
    {"scenario": "Two interception points are possible: one at 5 seconds (high energy) and one at 12 seconds (low energy).",
     "reasoning": "Choose based on constraints: if energy is limited, take the 12-second path. "
                   "If the target might escape, take the 5-second path. The energy-optimal "
                   "trajectory minimizes integral of acceleration squared. Trade-off: faster "
                   "interception = higher confidence but more energy. Dragonflies optimize for "
                   "success rate, not energy — they choose the surer path.",
     "concept": "dragonfly_energy_optimal"},

    # Proportional navigation
    {"scenario": "Bearing to target is changing at 2 degrees per second. How do you steer?",
     "reasoning": "Proportional navigation: commanded acceleration = N * closing_speed * bearing_rate. "
                   "With N=3 (typical gain) and closing speed 20 m/s: acceleration = 3 * 20 * 0.035 "
                   "= 2.1 m/s squared, perpendicular to line of sight. This drives bearing rate to "
                   "zero, guaranteeing interception if speed ratio is favorable.",
     "concept": "dragonfly_proportional_navigation"},

    # Prediction confidence assessment
    {"scenario": "Your prediction says target will be at (100, 200) in 5 seconds. How confident should you be?",
     "reasoning": "Confidence depends on: 1) observation quality (high SNR = high confidence), "
                   "2) model fit (how well does motion model explain past observations), "
                   "3) prediction horizon (longer = less confident), 4) target behavior "
                   "(constant velocity = high confidence, maneuvering = low). "
                   "Quantify via prediction covariance ellipse: 95% confidence region = 2*sigma.",
     "concept": "dragonfly_prediction_confidence"},
]


# ============================================================================
# OCTOPUS: Distributed peripheral cognition training data
#
# Analogy to cephalopod neuroanatomy where 2/3 of neurons live in arms, not
# the central brain. Scenarios exercise delegation, peripheral autonomy,
# local-then-central problem solving, and cross-arm integration.
# ============================================================================
OCTOPUS_TRAINING_DATA = [
    {"scenario": "You have 8 exploration agents each observing a different "
                  "region of a noisy environment. Central must decide.",
     "reasoning": "Do not demand consensus up front. Let each agent report its "
                   "local finding with a confidence score. Integrate at the "
                   "center by confidence-weighted mean. Arms that disagree are "
                   "not wrong — they may have found something the others missed.",
     "concept": "octopus_distributed_exploration"},

    {"scenario": "One arm reports a chemosensory signal inconsistent with the "
                  "visual input the others report.",
     "reasoning": "Chemosensory-tactile fusion means that arm is tasting what it "
                   "touches. A mismatch between arm-taste and center-vision is a "
                   "SIGNAL — the object is not what it appears to be visually. "
                   "Elevate that arm's confidence, do not suppress it.",
     "concept": "octopus_chemosensory_anomaly"},

    {"scenario": "An arm has tried 3 different approaches to reach a target and "
                  "all failed.",
     "reasoning": "Arm-local problem solving includes local retry with variation. "
                   "After 3 failures, broadcast the failure upward (skin-color "
                   "signal analog) and let another arm try a different approach. "
                   "Do not wait for central replanning — other arms adapt locally.",
     "concept": "octopus_local_retry_escalation"},

    {"scenario": "Central receives 8 arm reports with high variance — low coherence.",
     "reasoning": "Low coherence across arms is itself information: the environment "
                   "is heterogeneous or the task is ill-posed. Do not force a "
                   "central decision; broadcast 'uncertain' and request more "
                   "exploration. Coherence is the inverse of free-energy.",
     "concept": "octopus_low_coherence_escalation"},

    {"scenario": "Arm 3 is in a confined space and must deform its shape to continue.",
     "reasoning": "Deformable spatial reasoning: 'can I fit' is not a discrete "
                   "predicate. Continuously adjust the arm's geometry, using "
                   "tactile feedback as a local loss to minimize. Central does "
                   "not plan the squeeze — the arm feels its way through.",
     "concept": "octopus_deformable_navigation"},

    {"scenario": "An edge device in the swarm has gone offline mid-task.",
     "reasoning": "Treat edges like arms — if an edge drops, the other edges "
                   "absorb its workload based on proximity to its assigned "
                   "region. Do not wait for central re-planning. Analogous to "
                   "octopus arm autonomy when central is slow or inattentive.",
     "concept": "octopus_edge_failure_absorption"},

    {"scenario": "Two arms propose conflicting actions on the same object.",
     "reasoning": "Use ethics gate per-arm BEFORE aggregation. If one arm's "
                   "action violates a constraint (harm, deception, theft), veto "
                   "that arm's contribution — the remaining arms may still agree. "
                   "Ethics lives at the periphery, not only at the center.",
     "concept": "octopus_peripheral_ethics_gate"},

    {"scenario": "A new object appears that no arm has seen before. All confidence "
                  "drops near zero.",
     "reasoning": "Low confidence across all arms → generate a hypothesis and "
                   "submit it to the world model as a tentative observation. "
                   "Let multiple arms approach from different angles in parallel. "
                   "Confidence will rise in whichever arm's local pattern matches.",
     "concept": "octopus_novel_object_probe"},

    {"scenario": "Arm activity state (broadcast_state) is highly visible to sibling "
                  "arms and to outside observers.",
     "reasoning": "Involuntary state broadcasting is both a coordination channel "
                   "and an explainability channel. Other arms see which of them "
                   "is most engaged and coordinate implicitly. External agents "
                   "see the same signal and know the octopus's attention.",
     "concept": "octopus_state_broadcasting"},

    {"scenario": "Central has been silent for a long interval; arms are operating "
                  "independently.",
     "reasoning": "This is the default mode, not an emergency. Arms should "
                   "continue local problem-solving, integrate only when they "
                   "produce reportable results. The center is a correlation hub, "
                   "not a micromanager.",
     "concept": "octopus_default_autonomy_mode"},
]


class CollapseDetector:
    """Detects mode collapse by checking output differentiation.

    Sends 3 fixed probe inputs through the brain and computes pairwise
    cosine similarity. If all outputs are nearly identical (cosine sim > threshold),
    that's mode collapse. After consecutive_limit consecutive detections,
    halts training and writes alert.
    """

    _PROBE_SEEDS = [42, 99, 7]  # Fixed seeds for reproducible probe inputs

    def __init__(self, threshold=COLLAPSE_THRESHOLD,
                 consecutive_limit=COLLAPSE_CONSECUTIVE_LIMIT,
                 brain_ref=None):
        self.threshold = threshold
        self.consecutive_limit = consecutive_limit
        self.consecutive_collapses = 0
        self.last_mean_sim = None
        self.brain_ref = brain_ref
        self.total_reinits = 0

    def check(self, brain, step, stage):
        """Returns True if training should halt due to mode collapse."""
        outputs = []
        for seed in self._PROBE_SEEDS:
            rng = np.random.RandomState(seed)
            probe = rng.randn(BRAIN_INPUT_DIM).astype(np.float32).tolist()
            try:
                result = brain.decide_full(probe)
                vec = result.get("output_vector", [])
                if vec:
                    outputs.append(np.array(vec, dtype=np.float32))
            except Exception:
                return False  # Can't check, don't halt

        if len(outputs) < 2:
            return False

        # Compute pairwise cosine similarities
        sims = []
        for i in range(len(outputs)):
            for j in range(i + 1, len(outputs)):
                norm_i = np.linalg.norm(outputs[i])
                norm_j = np.linalg.norm(outputs[j])
                if norm_i > 1e-8 and norm_j > 1e-8:
                    sims.append(np.dot(outputs[i], outputs[j]) / (norm_i * norm_j))

        if not sims:
            return False

        self.last_mean_sim = float(np.mean(sims))

        if self.last_mean_sim >= self.threshold:
            self.consecutive_collapses += 1
            print(f"    *** COLLAPSE WARNING [{self.consecutive_collapses}/"
                  f"{self.consecutive_limit}]: mean cosine sim = "
                  f"{self.last_mean_sim:.6f} ***", flush=True)
            if self.consecutive_collapses >= self.consecutive_limit:
                msg = (f"MODE COLLAPSE at stage {stage} step {step}. "
                       f"Mean cosine sim = {self.last_mean_sim:.6f} for "
                       f"{self.consecutive_limit} consecutive checks.")
                print(f"\n    *** {msg} ***", flush=True)
                try:
                    with open("TRAINING_ALERT.txt", "a") as f:
                        f.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {msg}\n")
                except OSError:
                    pass
                # Auto-recovery: reinitialize weights instead of halting
                if self.brain_ref is not None:
                    print("    >>> AUTO-RECOVERY: Reinitializing weights...",
                          flush=True)
                    try:
                        self.brain_ref.reinit_weights()
                        self.consecutive_collapses = 0
                        self.total_reinits = getattr(self, 'total_reinits', 0) + 1
                        print(f"    >>> Weights reinitialized "
                              f"(recovery #{self.total_reinits})", flush=True)
                        return False  # Continue training
                    except Exception as e:
                        print(f"    >>> Reinit failed: {e} — halting", flush=True)
                return True
        else:
            if self.consecutive_collapses > 0:
                print(f"    Collapse cleared: mean cosine sim = "
                      f"{self.last_mean_sim:.4f}", flush=True)
            self.consecutive_collapses = 0

        return False


class FastCollapseDetector:
    """Fast rolling mode collapse detector — checks every 10 steps.

    Tracks cosine similarity between consecutive outputs. If outputs are
    nearly identical for FAST_COLLAPSE_WINDOW consecutive checks, triggers
    intervention WITHOUT halting:
      1. LR spike (10× for 50 steps) to escape the attractor
      2. Diversity injection (20 items from different domains)

    This catches collapse 5-10× faster than the full probe-based detector
    and recovers without losing training progress.
    """

    def __init__(self, brain_ref=None, lr_controller=None):
        self.prev_output = None
        self.identical_count = 0
        self.brain_ref = brain_ref
        self.lr_controller = lr_controller
        self.lr_spike_remaining = 0
        self.total_interventions = 0
        self.base_lr = None

    def check_output(self, output_vec, step):
        """Call every FAST_COLLAPSE_INTERVAL steps with the latest output.
        Returns True if intervention was triggered."""
        if output_vec is None:
            return False

        vec = np.array(output_vec, dtype=np.float32)
        if self.prev_output is not None:
            norm_a = np.linalg.norm(vec)
            norm_b = np.linalg.norm(self.prev_output)
            if norm_a > 1e-8 and norm_b > 1e-8:
                sim = float(np.dot(vec, self.prev_output) / (norm_a * norm_b))
                if sim > FAST_COLLAPSE_THRESHOLD:
                    self.identical_count += 1
                else:
                    self.identical_count = 0
            else:
                self.identical_count += 1  # zero outputs = collapsed

        self.prev_output = vec.copy()

        if self.identical_count >= FAST_COLLAPSE_WINDOW:
            self._intervene(step)
            self.identical_count = 0
            return True
        return False

    def _intervene(self, step):
        """Fast intervention: LR spike + diversity injection."""
        self.total_interventions += 1
        print(f"\n    *** FAST COLLAPSE INTERVENTION #{self.total_interventions} "
              f"at step {step} ***", flush=True)

        # 1. LR spike — temporarily increase learning rate to escape attractor
        if self.lr_controller:
            self.base_lr = self.lr_controller.get_lr()
            self.lr_spike_remaining = LR_SPIKE_DURATION
            print(f"    >>> LR spike: {self.base_lr:.6f} → "
                  f"{self.base_lr * LR_SPIKE_FACTOR:.6f} for {LR_SPIKE_DURATION} steps",
                  flush=True)

        # 2. Diversity injection — force diverse inputs across all domains
        if self.brain_ref:
            try:
                self._inject_diversity()
            except Exception as e:
                print(f"    >>> Diversity injection failed: {e}", flush=True)

        try:
            with open("TRAINING_ALERT.txt", "a") as f:
                f.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] "
                        f"Fast collapse intervention #{self.total_interventions} "
                        f"at step {step}\n")
        except OSError:
            pass

    def _inject_diversity(self):
        """Feed deliberately diverse inputs to break the collapsed attractor."""
        if not self.brain_ref:
            return

        diverse_texts = [
            "The sun is a star that provides light and heat",
            "Dogs are mammals that have been human companions",
            "Water freezes at zero degrees Celsius",
            "Two plus two equals four",
            "A triangle has three sides and three angles",
            "Ancient Egyptians built the pyramids",
            "Kindness means treating others with warmth",
            "Plants make their own food from sunlight",
            "Gravity pulls everything toward Earth",
            "Mixing baking soda and vinegar creates bubbles",
            "A ball dropped from a height falls faster and faster",
            "Predator populations crash when prey declines",
            "Iron left in rain slowly rusts",
            "Your body temperature stays near thirty seven degrees",
            "Sound travels through air as waves of pressure",
            "A seed contains a tiny plant waiting for water",
            "Rivers flow from high ground toward the sea",
            "Energy cannot be created or destroyed",
            "Cells are the basic building blocks of all life",
            "The heart pumps blood carrying oxygen to cells",
        ]

        injected = 0
        for text in diverse_texts[:DIVERSITY_INJECTION_COUNT]:
            try:
                features = encode_text(text)
                target = make_semantic_target(text)
                lr = (self.base_lr or BASE_LEARNING_RATE) * LR_SPIKE_FACTOR
                self.brain_ref.learn_vector(features, target,
                                             label=text[:30],
                                             learning_rate=lr)
                injected += 1
            except Exception:
                pass

        print(f"    >>> Injected {injected} diverse items across domains", flush=True)

    def get_spiked_lr(self, base_lr):
        """Returns spiked LR if in spike period, otherwise base_lr."""
        if self.lr_spike_remaining > 0:
            self.lr_spike_remaining -= 1
            return base_lr * LR_SPIKE_FACTOR
        return base_lr


class CosineAnnealingLR:
    """Cosine annealing learning rate with linear warmup.

    Warmup: linearly ramp from lr_min to lr_max over warmup_steps.
    Then: cosine decay from lr_max to lr_min over t_max steps.
    """

    def __init__(self, lr_max=BASE_LEARNING_RATE, lr_min=MIN_LEARNING_RATE,
                 warmup_steps=LR_WARMUP_STEPS, t_max=LR_COSINE_T_MAX):
        self.lr_max = lr_max
        self.lr_min = lr_min
        self.warmup_steps = warmup_steps
        self.t_max = t_max
        self.step_count = 0

    def step(self):
        self.step_count += 1

    def get_lr(self):
        import math
        if self.step_count < self.warmup_steps:
            # Linear warmup
            return self.lr_min + (self.lr_max - self.lr_min) * (self.step_count / max(1, self.warmup_steps))
        # Cosine decay
        progress = (self.step_count - self.warmup_steps) / max(1, self.t_max - self.warmup_steps)
        progress = min(progress, 1.0)
        return self.lr_min + 0.5 * (self.lr_max - self.lr_min) * (1.0 + math.cos(math.pi * progress))


LR_TRIGGER_FILE = "/tmp/athena_lr"

# === Hyperparameter override files (written by auto-tuner, read here) ===
HP_OVERRIDE_FILES = {
    'lr':              '/tmp/athena_lr',
    'sparsity':        '/tmp/athena_sparsity',
    'diversity_weight': '/tmp/athena_diversity_weight',
    'grad_clip':       '/tmp/athena_grad_clip',
    'weight_decay':    '/tmp/athena_weight_decay',
    'output_lr_boost': '/tmp/athena_output_lr_boost',
    'dropout':         '/tmp/athena_dropout',
    'temperature':     '/tmp/athena_temperature',
}

# Current HP state (mutable, applied each step)
_hp_state = {
    'sparsity': 0.05,
    'diversity_weight': 0.1,
    'grad_clip': 100.0,
    'weight_decay': 1e-5,
    'output_lr_boost': 10.0,
    'dropout': 0.0,
    'temperature': 1.0,
}


# Per-stage warning flag — only log "binding missing" once per stage to avoid spam
# when an older .so build does not expose set_training_dashboard.
_DASHBOARD_WARNED_STAGES = set()


def _push_dashboard(brain, stage, step, **kwargs):
    """Wire the live trainer state into the brain's training_dashboard struct.

    Tolerant of:
    - older .so builds that do not expose set_training_dashboard (logged once per stage)
    - daemon RPC layer transient failures (silently swallowed)

    Pass any subset of kwargs accepted by nimcp_brain_set_training_dashboard:
    domain, fact_ratio, warm_start_complete, warm_start_step,
    lr_physics, lr_chemistry, lr_biology,
    wm_steps, wm_phys, wm_chem, wm_bio,
    collapse_events, surprises, replays, vocab_size, lang_confidence, active_engines.
    """
    try:
        brain.set_training_dashboard(stage=int(stage), step=int(step), **kwargs)
    except Exception as e:
        if stage not in _DASHBOARD_WARNED_STAGES:
            _DASHBOARD_WARNED_STAGES.add(stage)
            print(f"  [Dashboard] set_training_dashboard unavailable for stage {stage} "
                  f"({type(e).__name__}: {e}) — metrics will read zero", flush=True)


def _check_hp_overrides(brain, lr_scheduler, step):
    """Check all hyperparameter override files and apply changes.

    Called every step from the training loop. Reads /tmp/athena_* files
    written by the hyperparameter auto-tuner and applies them to the brain.
    Also checks for hot-reload triggers (sensory data injection).
    """
    global _hp_state

    # Hot-reload sensory data from JSON trigger file
    _sensory_trigger = "/tmp/athena_reload_sensory.json"
    if step % 10 == 0 and os.path.exists(_sensory_trigger):
        try:
            with open(_sensory_trigger) as f:
                new_data = json.load(f)
            new_sensory = new_data.get("sensory", [])
            new_objects = new_data.get("objects", [])
            if new_sensory:
                before = len(StimulusSource.SENSORY)
                # Deduplicate against existing
                existing = set(StimulusSource.SENSORY)
                added = [s for s in new_sensory if s not in existing]
                StimulusSource.SENSORY.extend(added)
                print(f"    [HOT RELOAD] Sensory data: {before} → {len(StimulusSource.SENSORY)} "
                      f"(+{len(added)} new items)")
            if new_objects:
                before = len(StimulusSource.OBJECTS)
                existing_names = {n for n, _ in StimulusSource.OBJECTS}
                added = [(n, d) for n, d in new_objects if n not in existing_names]
                StimulusSource.OBJECTS.extend(added)
                print(f"    [HOT RELOAD] Objects: {before} → {len(StimulusSource.OBJECTS)} "
                      f"(+{len(added)} new items)")
            os.remove(_sensory_trigger)
        except Exception as e:
            print(f"    [HOT RELOAD] Error: {e}")
            try:
                os.remove(_sensory_trigger)
            except OSError:
                pass

    for param, path in HP_OVERRIDE_FILES.items():
        if not os.path.exists(path):
            continue
        try:
            with open(path) as f:
                new_val = float(f.read().strip())
            os.remove(path)
        except (ValueError, OSError):
            try:
                os.remove(path)
            except OSError:
                pass
            continue

        if param == 'lr':
            new_val = max(MIN_LEARNING_RATE, min(MAX_LEARNING_RATE, new_val))
            lr_scheduler.lr_max = new_val
            lr_scheduler.lr_min = new_val * 0.1
            print(f"    [HP] lr → {new_val:.6f}")

        elif param == 'sparsity':
            _hp_state['sparsity'] = max(0.02, min(0.30, new_val))
            # No runtime sparsity-target API exists on the brain / BrainProxy
            # (set_network_ablation only toggles train_cnn/snn/lnn). The prior
            # call raised AttributeError/TypeError and was silently swallowed,
            # so this knob never did anything. Record state only until a real
            # sparsity-control API is wired.
            print(f"    [HP] sparsity → {_hp_state['sparsity']:.3f} "
                  f"(recorded only — no runtime sparsity API)")

        elif param == 'diversity_weight':
            _hp_state['diversity_weight'] = max(0.0, min(1.0, new_val))
            # Applied via learn_vector kwargs if supported, or brain config
            try:
                brain.utm_set_per_network_lr(0, lr_scheduler.get_lr())  # Trigger config refresh
            except Exception:
                pass
            print(f"    [HP] diversity_weight → {_hp_state['diversity_weight']:.3f}")

        elif param == 'grad_clip':
            _hp_state['grad_clip'] = max(0.1, min(100.0, new_val))
            print(f"    [HP] grad_clip → {_hp_state['grad_clip']:.1f}")

        elif param == 'weight_decay':
            _hp_state['weight_decay'] = max(0.0, min(1e-3, new_val))
            print(f"    [HP] weight_decay → {_hp_state['weight_decay']:.6f}")

        elif param == 'output_lr_boost':
            _hp_state['output_lr_boost'] = max(1.0, min(50.0, new_val))
            print(f"    [HP] output_lr_boost → {_hp_state['output_lr_boost']:.1f}")

        elif param == 'dropout':
            _hp_state['dropout'] = max(0.0, min(0.5, new_val))
            print(f"    [HP] dropout → {_hp_state['dropout']:.3f}")

        elif param == 'temperature':
            _hp_state['temperature'] = max(0.1, min(5.0, new_val))
            print(f"    [HP] temperature → {_hp_state['temperature']:.2f}")


def get_hp_state():
    """Get current hyperparameter state for use in training loop."""
    return _hp_state.copy()


class AdaptiveLRController:
    """Auto-adjusts learning rate based on loss trends + manual override.

    Monitors rolling loss windows and adjusts LR:
    - Plateau (loss not improving): increase LR by scale_up
    - Diverging (loss increasing): decrease LR by scale_down
    - Improving: hold steady
    - Manual override via /tmp/athena_lr trigger file
    """

    def __init__(self, initial_lr=BASE_LEARNING_RATE, min_lr=MIN_LEARNING_RATE,
                 max_lr=MAX_LEARNING_RATE, window=500, patience=3,
                 scale_up=1.5, scale_down=0.5, min_delta=0.002):
        self.lr = initial_lr
        self.min_lr = min_lr
        self.max_lr = max_lr
        self.window = window          # Loss averaging window
        self.patience = patience      # Evals without improvement before adjust
        self.scale_up = scale_up
        self.scale_down = scale_down
        self.min_delta = min_delta    # Minimum improvement to count
        self.best_loss = float('inf')
        self.stale_count = 0
        self.history = []             # (step, lr, avg_loss, action)

    def check_trigger_file(self):
        """Check for manual LR override via /tmp/athena_lr."""
        if os.path.exists(LR_TRIGGER_FILE):
            try:
                with open(LR_TRIGGER_FILE) as f:
                    new_lr = float(f.read().strip())
                os.remove(LR_TRIGGER_FILE)
                new_lr = max(self.min_lr, min(self.max_lr, new_lr))
                old_lr = self.lr
                self.lr = new_lr
                self.stale_count = 0
                self.best_loss = float('inf')  # Reset plateau detection
                print(f"    [LR] Manual override: {old_lr:.6f} → {new_lr:.6f}")
                return True
            except (ValueError, OSError):
                try:
                    os.remove(LR_TRIGGER_FILE)
                except OSError:
                    pass
        return False

    def update(self, avg_loss, step):
        """Update LR based on loss trend. Call every eval interval (e.g. 500 steps)."""
        # Check manual override first
        if self.check_trigger_file():
            self.history.append((step, self.lr, avg_loss, "MANUAL"))
            return self.lr

        action = "HOLD"
        if avg_loss < self.best_loss - self.min_delta:
            # Improving — reset patience, keep current LR
            self.best_loss = avg_loss
            self.stale_count = 0
            action = "IMPROVING"
        else:
            self.stale_count += 1

            if self.stale_count >= self.patience:
                if avg_loss > self.best_loss + self.min_delta:
                    # Loss is going UP — decrease LR
                    old_lr = self.lr
                    self.lr = max(self.min_lr, self.lr * self.scale_down)
                    self.stale_count = 0
                    action = f"DIVERGE {old_lr:.6f}→{self.lr:.6f}"
                    print(f"    [LR] Loss diverging — reducing: {old_lr:.6f} → {self.lr:.6f}")
                else:
                    # Plateau — increase LR to explore
                    old_lr = self.lr
                    self.lr = min(self.max_lr, self.lr * self.scale_up)
                    self.stale_count = 0
                    self.best_loss = avg_loss  # Reset baseline after boost
                    action = f"PLATEAU {old_lr:.6f}→{self.lr:.6f}"
                    print(f"    [LR] Loss plateaued — boosting: {old_lr:.6f} → {self.lr:.6f}")

        self.history.append((step, self.lr, avg_loss, action))
        return self.lr

    def get_lr(self):
        """Get current LR (also checks trigger file)."""
        self.check_trigger_file()
        return self.lr

    def update_from_utm(self, brain, step):
        """Use UTM DFA health to guide LR adjustment (supplements loss-based).

        DFA health provides principled, fractal-analysis-based training diagnostics:
        - OPTIMAL (α≈0.8-1.2): pink noise, ideal learning dynamics
        - NOISY (α<0.6): too much noise, reduce LR
        - DRIFTING (α>1.3): diverging, reduce LR aggressively
        - OSCILLATING (α<0.3): chaotic, reduce LR significantly
        - PLATEAU (H>0.8): stuck, increase LR
        """
        try:
            health = brain.utm_get_training_health()
        except (AttributeError, RuntimeError):
            return  # UTM not available

        health_name = health.get("health_name", "unknown")
        dfa_exp = health.get("dfa_exponent", 0.0)

        # DFA-guided LR adjustment factors
        adjustments = {
            "noisy": 0.8,
            "drifting": 0.5,
            "oscillating": 0.3,
            "plateau": 1.5,
        }
        factor = adjustments.get(health_name, 1.0)
        if factor != 1.0:
            old_lr = self.lr
            self.lr = max(self.min_lr, min(self.max_lr, self.lr * factor))
            if self.lr != old_lr:
                print(f"    [LR/DFA] Health={health_name} (α={dfa_exp:.2f}) "
                      f"— adjusting: {old_lr:.6f} → {self.lr:.6f}")
                self.history.append((step, self.lr, 0.0, f"DFA_{health_name.upper()}"))


class AdaptiveBatchSize:
    """Adaptive batch size based on loss stability.

    Starts with min_batch, increases when loss is stable, decreases on spikes.
    """

    def __init__(self, min_batch=4, max_batch=32, initial=8):
        self.min_batch = min_batch
        self.max_batch = max_batch
        self.current = initial
        self.recent_losses = []
        self.adjust_interval = 200
        self.step_count = 0

    def record_loss(self, loss):
        if loss is not None and loss >= 0:
            self.recent_losses.append(loss)
            if len(self.recent_losses) > 100:
                self.recent_losses = self.recent_losses[-100:]

    def step(self):
        self.step_count += 1
        if self.step_count % self.adjust_interval == 0 and len(self.recent_losses) >= 50:
            first_half = np.mean(self.recent_losses[:50])
            second_half = np.mean(self.recent_losses[50:])
            variance = np.var(self.recent_losses[-50:])
            if variance < 0.001 and second_half <= first_half:
                # Stable — increase batch size for throughput
                self.current = min(self.current * 2, self.max_batch)
            elif variance > 0.01 or second_half > first_half * 1.5:
                # Unstable — decrease batch size for finer updates
                self.current = max(self.current // 2, self.min_batch)

    @property
    def size(self):
        return self.current


class MiniBatchTrainer:
    """Accumulates learn_vector calls into mini-batches for GPU gradient accumulation.

    Instead of per-sample SGD, collects N samples and calls learn_vector_batch()
    for more stable gradients and better GPU utilization.
    """

    # Crash after this many consecutive flush() failures — supervisord
    # restarts the trainer rather than letting it silently "train" forever
    # at loss=0.0. The old bare-except pattern masked a 3-hour wedge in
    # 2026-05-15 (brain SIGTERM → trainer kept calling learn_vector_batch
    # which kept raising ConnectionError → caught silently → buffer cleared
    # → repeat). 50 × ~5s per batch ≈ 4 min of grace before crashing out.
    MAX_CONSECUTIVE_FLUSH_FAILURES = 50

    def __init__(self, brain, batch_size=8):
        self.brain = brain
        self.batch_size = batch_size
        self._buffer = []  # list of (features, target, label, confidence, lr)
        self._last_avg_loss = 0.0
        self._consecutive_flush_failures = 0

    def learn(self, features, target, label="", confidence=0.5, learning_rate=None):
        """Buffer a sample. Returns estimated loss (batch average when flushed)."""
        self._buffer.append((features, target, label, confidence, learning_rate))
        if len(self._buffer) >= self.batch_size:
            return self.flush()
        return self._last_avg_loss  # return last batch average as estimate

    def flush(self):
        """Flush accumulated samples as a batch."""
        if not self._buffer:
            return self._last_avg_loss

        # Batch path: ANN trains on full batch, LNN/SNN/CNN on subset.
        pairs = [(f, t) for f, t, _, _, _ in self._buffer]
        lrs = [lr for _, _, _, _, lr in self._buffer if lr is not None]
        avg_lr = sum(lrs) / len(lrs) if lrs else None
        # Track whether ANY learn call this flush actually reached the brain.
        # Old code silently treated every-call-failed as loss=0.0; that masked
        # the 2026-05-15 wedge for 3 hours. We now log the first failure of
        # each streak and crash out after MAX_CONSECUTIVE_FLUSH_FAILURES so
        # supervisord can recover the trainer.
        flush_ok = False
        try:
            lr_kw = {"learning_rate": avg_lr} if avg_lr is not None else {}
            avg_loss = self.brain.learn_vector_batch(pairs, **lr_kw)
            self._last_avg_loss = avg_loss if avg_loss is not None and avg_loss >= 0 else 0.0
            flush_ok = True
        except Exception as e:
            # Fallback: per-sample. We still log the batch-path failure
            # because it's the canary — the per-sample loop usually fails
            # the same way (brain unreachable, etc.).
            if self._consecutive_flush_failures == 0:
                logger.warning("BatchedTrainer.learn_vector_batch failed (%s) — "
                               "falling back to per-sample", type(e).__name__)
            losses = []
            for f, t, lbl, conf, lr in self._buffer:
                lr_kw = {"learning_rate": lr} if lr is not None else {}
                try:
                    loss = self.brain.learn_vector(f, t, label=lbl,
                                                   confidence=conf, **lr_kw)
                    losses.append(loss if loss is not None and loss >= 0 else 0.0)
                    flush_ok = True
                except Exception:
                    losses.append(0.0)
            self._last_avg_loss = sum(losses) / len(losses) if losses else 0.0

        self._buffer.clear()

        if flush_ok:
            if self._consecutive_flush_failures > 0:
                logger.warning("BatchedTrainer recovered after %d consecutive "
                               "flush failures", self._consecutive_flush_failures)
            self._consecutive_flush_failures = 0
        else:
            self._consecutive_flush_failures += 1
            if self._consecutive_flush_failures % 10 == 1:
                logger.error("BatchedTrainer flush failed %d consecutive times "
                             "— brain unreachable? (will crash at %d)",
                             self._consecutive_flush_failures,
                             self.MAX_CONSECUTIVE_FLUSH_FAILURES)
            if self._consecutive_flush_failures >= self.MAX_CONSECUTIVE_FLUSH_FAILURES:
                raise RuntimeError(
                    f"BatchedTrainer: {self._consecutive_flush_failures} "
                    f"consecutive flush failures — brain appears unreachable; "
                    f"raising to let supervisord restart the trainer")
        return self._last_avg_loss


class ParallelDataLoader:
    """Parallel dataset loading with prefetch queue.

    Uses a thread pool to generate and encode stimuli in parallel,
    keeping a buffer of ready-to-use batches ahead of training.
    """

    def __init__(self, source, composer, num_workers=2, prefetch_batches=4, batch_size=64):
        from threading import Thread
        from queue import Queue
        self.source = source
        self.composer = composer
        self.batch_size = batch_size
        self.queue = Queue(maxsize=prefetch_batches)
        self._stop = False
        self._workers = []
        for _ in range(num_workers):
            t = Thread(target=self._worker, daemon=True)
            t.start()
            self._workers.append(t)

    def _worker(self):
        while not self._stop:
            descs = []
            for _ in range(self.batch_size):
                if random.random() < 0.5:
                    descs.append(self.source.get_sensory())
                else:
                    desc, _ = generate_sensory_exposure()
                    descs.append(desc)
            targets = batch_make_semantic_targets(descs)
            features_list = [self.composer.compose(text=d, modality="text") for d in descs]
            batch = list(zip(descs, features_list, targets))
            try:
                self.queue.put(batch, timeout=10)
            except Exception:
                if self._stop:
                    break

    def get_batch(self):
        return self.queue.get(timeout=60)

    def stop(self):
        self._stop = True


# ============================================================================
# Hard Example Mining: curriculum-aware data sampling
# ============================================================================

class HardExampleMiner:
    """Track per-example loss and replay high-loss examples more frequently.

    Maintains a priority buffer of hard examples. During sampling,
    mixes fresh data with replayed hard examples (ratio controlled by
    replay_fraction). Hard examples decay in priority over time to
    avoid getting stuck.
    """

    def __init__(self, max_buffer=500, replay_fraction=0.25, decay=0.95):
        self.buffer = []  # list of (priority, description, features, target)
        self.max_buffer = max_buffer
        self.replay_fraction = replay_fraction
        self.decay = decay
        import heapq
        self._heapq = heapq

    def record(self, loss, description, features, target):
        """Record a training example with its loss."""
        if loss is None or loss < 0:
            return
        # Use negative loss for min-heap (we want highest loss on top)
        entry = (loss, description, features, target)
        if len(self.buffer) < self.max_buffer:
            self.buffer.append(entry)
        elif loss > self.buffer[0][0]:
            # Replace the lowest-loss entry
            self.buffer[0] = entry
            # Maintain partial sort — just keep the list
            self.buffer.sort(key=lambda x: x[0])

    def get_hard_examples(self, n):
        """Get n hard examples for replay, or fewer if buffer is small."""
        if not self.buffer:
            return []
        # Sort by loss descending, take top n
        sorted_buf = sorted(self.buffer, key=lambda x: -x[0])
        examples = sorted_buf[:n]
        # Decay priorities
        self.buffer = [(loss * self.decay, desc, feat, tgt)
                       for loss, desc, feat, tgt in self.buffer]
        return [(desc, feat, tgt) for _, desc, feat, tgt in examples]

    def should_replay(self):
        """Return True if we should replay hard examples this step."""
        return len(self.buffer) >= 20 and random.random() < self.replay_fraction


# ============================================================================
# Curriculum Escalator: progressive difficulty unlock
# ============================================================================

class CurriculumEscalator:
    """Progressive difficulty curriculum — unlock harder content when basics are mastered.

    Divides cognitive training into 3 tiers:
    - Tier 1 (steps 0-3000): Basic domains only (ethics, causal, analogy)
    - Tier 2 (steps 3000-7000): + counterfactual, metacognition, rcog, collective
    - Tier 3 (steps 7000+): + sensor, motor, safety, embodiment, portia, dragonfly

    Within each tier, items are ordered by estimated difficulty.
    A tier unlocks when avg loss on current tier drops below threshold.
    """

    def __init__(self, unlock_threshold=500.0):
        self.unlock_threshold = unlock_threshold
        self.current_tier = 1
        self.tier_losses = {1: [], 2: [], 3: []}
        self.tier_domains = {
            1: ['ethics', 'causal', 'analogy', 'metacognition'],
            2: ['counterfactual', 'rcog', 'collective'],
            3: ['sensor_fusion', 'motor_control', 'safety', 'embodiment', 'portia', 'dragonfly']
        }
        self.all_data = None  # lazy init

    def get_available_domains(self):
        """Return domains available at current tier."""
        domains = []
        for tier in range(1, self.current_tier + 1):
            domains.extend(self.tier_domains.get(tier, []))
        return domains

    def filter_item(self, item):
        """Returns True if this item is available at current tier."""
        return item.get('domain', '') in self.get_available_domains()

    def record_loss(self, loss, domain):
        """Record a loss for tier tracking."""
        for tier, domains in self.tier_domains.items():
            if domain in domains:
                self.tier_losses[tier].append(loss)
                # Keep only last 100
                if len(self.tier_losses[tier]) > 100:
                    self.tier_losses[tier] = self.tier_losses[tier][-100:]
                break

    def check_escalation(self):
        """Check if we should unlock the next tier."""
        if self.current_tier >= 3:
            return False

        current_losses = self.tier_losses[self.current_tier]
        if len(current_losses) < 20:
            return False  # Not enough data

        avg_loss = sum(current_losses[-50:]) / min(len(current_losses), 50)
        if avg_loss < self.unlock_threshold:
            self.current_tier += 1
            print(f"    [Curriculum] Tier {self.current_tier} unlocked! "
                  f"(avg_loss={avg_loss:.2f} < {self.unlock_threshold})")
            return True
        return False

    def get_status(self):
        """Return status string."""
        avgs = {}
        for tier, losses in self.tier_losses.items():
            if losses:
                avgs[tier] = sum(losses[-50:]) / min(len(losses), 50)
        return (f"tier={self.current_tier}/3, "
                f"domains={len(self.get_available_domains())}/13, "
                f"tier_losses={avgs}")


# ============================================================================
# Embedding Adapter: lightweight projection to domain-adapted space
# ============================================================================

class EmbeddingAdapter:
    """Lightweight linear projection trained on (embedding, brain_output) pairs.

    Instead of fine-tuning the full sentence-transformer, this trains a small
    projection matrix W that maps frozen embeddings to domain-adapted space:
        adapted = embedding @ W + bias

    Trains online using collected (input_embedding, output_embedding) pairs
    from the brain's responses. Refits periodically using least-squares.
    """

    def __init__(self, input_dim=1024, output_dim=1024, refit_interval=1000):
        self.input_dim = input_dim
        self.output_dim = output_dim
        self.refit_interval = refit_interval
        self.W = np.eye(min(input_dim, output_dim), dtype=np.float32)
        if input_dim != output_dim:
            self.W = np.random.randn(input_dim, output_dim).astype(np.float32) * 0.01
        self.bias = np.zeros(output_dim, dtype=np.float32)
        self.pairs = []  # (input_emb, target_emb) tuples
        self.max_pairs = 5000
        self.step_count = 0
        self.fitted = False

    def record_pair(self, input_emb, brain_output_emb):
        """Record a (frozen_embedding, brain_response_embedding) pair."""
        inp = np.asarray(input_emb, dtype=np.float32).ravel()[:self.input_dim]
        out = np.asarray(brain_output_emb, dtype=np.float32).ravel()[:self.output_dim]
        if len(inp) < self.input_dim or len(out) < self.output_dim:
            return
        self.pairs.append((inp, out))
        if len(self.pairs) > self.max_pairs:
            self.pairs = self.pairs[-self.max_pairs:]
        self.step_count += 1

    def maybe_refit(self):
        """Refit the projection if enough new data has accumulated."""
        if self.step_count < self.refit_interval or len(self.pairs) < 100:
            return
        self.step_count = 0
        # Least-squares fit: min ||X @ W + b - Y||^2
        X = np.array([p[0] for p in self.pairs], dtype=np.float32)
        Y = np.array([p[1] for p in self.pairs], dtype=np.float32)
        # Center
        X_mean = X.mean(axis=0)
        Y_mean = Y.mean(axis=0)
        X_c = X - X_mean
        Y_c = Y - Y_mean
        # Solve via pseudo-inverse (truncated SVD for stability)
        try:
            U, S, Vt = np.linalg.svd(X_c, full_matrices=False)
            # Truncate small singular values
            threshold = max(S) * 1e-5
            S_inv = np.where(S > threshold, 1.0 / S, 0.0)
            self.W = (Vt.T * S_inv) @ U.T @ Y_c
            self.bias = Y_mean - X_mean @ self.W
            self.fitted = True
        except np.linalg.LinAlgError:
            pass  # Keep current projection

    def adapt(self, embedding):
        """Apply the learned projection to a frozen embedding."""
        if not self.fitted:
            return embedding
        emb = np.asarray(embedding, dtype=np.float32).ravel()[:self.input_dim]
        adapted = emb @ self.W + self.bias
        return adapted


# ============================================================================
# Knowledge Distillation: extract structured knowledge from Claude
# ============================================================================

class ClaudeDistiller:
    """Extract structured concept relationships from Claude's lessons.

    When Claude generates a lesson, also ask for key concepts and their
    relationships. Use these to create additional training targets that
    encode the relational structure, not just the surface text embedding.
    """

    def __init__(self, teacher):
        self.teacher = teacher
        self._concept_cache = {}

    def distill_concepts(self, lesson_text):
        """Ask Claude to extract key concepts and relationships from a lesson.

        Returns list of (concept, relation, concept) triples and their embeddings.
        """
        if not self.teacher:
            return []

        # Check cache
        key = lesson_text[:100]
        if key in self._concept_cache:
            return self._concept_cache[key]

        prompt = (
            f"From this lesson, extract 2-4 key concepts and their relationships.\n"
            f"Lesson: \"{lesson_text}\"\n\n"
            f"Format each as: CONCEPT1 -> RELATION -> CONCEPT2\n"
            f"Example: photosynthesis -> requires -> sunlight\n"
            f"Only output the concept triples, one per line."
        )

        try:
            response = self.teacher._call_claude(prompt, max_tokens=128)
            triples = []
            for line in response.strip().split("\n"):
                parts = [p.strip() for p in line.split("->")]
                if len(parts) == 3:
                    # Create a composite embedding from the triple
                    composite = f"{parts[0]} {parts[1]} {parts[2]}"
                    emb = encode_text(composite)
                    triples.append((parts[0], parts[1], parts[2], emb))

            if len(self._concept_cache) > 500:
                # Evict oldest
                oldest = next(iter(self._concept_cache))
                del self._concept_cache[oldest]
            self._concept_cache[key] = triples
            return triples
        except Exception:
            return []

    def make_distillation_targets(self, lesson_text, target_dim=None):
        """Create additional training targets from distilled concepts.

        Returns list of (input_features_text, target_vector) tuples
        for supplementary training on relational structure.
        """
        if target_dim is None:
            target_dim = BRAIN_OUTPUT_DIM
        triples = self.distill_concepts(lesson_text)
        targets = []
        for concept1, relation, concept2, emb in triples:
            # Create target by tiling the composite embedding
            target = np.zeros(target_dim, dtype=np.float32)
            for i in range(0, target_dim, len(emb)):
                n = min(len(emb), target_dim - i)
                target[i:i + n] = emb[:n]
            # Input text is the first concept (student should learn the connection)
            targets.append((concept1, target.tolist()))
        return targets


# ============================================================================
# Multi-Resolution Temporal: fast + slow LNN processing
# ============================================================================

class MultiResolutionTemporal:
    """Process LNN at multiple timescales for richer temporal representation.

    Fast channel: updates every step (sensory-level, short-term patterns).
    Slow channel: updates every N steps with averaged input (conceptual, long-term).

    The fast channel captures immediate patterns; the slow channel captures
    trends and persistent state. Both contribute to the biological context.
    """

    def __init__(self, brain, slow_interval=5):
        self.brain = brain
        self.slow_interval = slow_interval
        self.step_count = 0
        self.slow_accumulator = None
        self.slow_count = 0

    def step(self, features):
        """Process one temporal step at both resolutions."""
        self.step_count += 1
        fast_input = features[:128]

        # Fast channel — every step
        try:
            self.brain.lnn_forward_step(fast_input)
        except Exception:
            pass

        # Accumulate for slow channel
        arr = np.array(fast_input[:128], dtype=np.float32)
        if self.slow_accumulator is None:
            self.slow_accumulator = np.zeros_like(arr)
        self.slow_accumulator += arr
        self.slow_count += 1

        # Slow channel — every N steps with averaged input
        if self.slow_count >= self.slow_interval:
            avg_input = (self.slow_accumulator / self.slow_count).tolist()
            # Scale down to represent "smoothed" temporal signal
            scaled = [x * 0.5 for x in avg_input]
            try:
                self.brain.lnn_forward_step(scaled)
            except Exception:
                pass
            self.slow_accumulator = np.zeros_like(arr)
            self.slow_count = 0

    def get_temporal_context(self):
        """Get combined fast+slow LNN state for biological context."""
        try:
            state = self.brain.lnn_get_state()
            return state
        except Exception:
            return None


# ============================================================================
# Early Stopping: evaluation-driven training termination
# ============================================================================

class EarlyStopping:
    """Stop training when validation metrics plateau.

    Tracks a running best metric (e.g., differentiation score from eval probes).
    If no improvement for `patience` evaluations, signals to stop.
    """

    def __init__(self, patience=5, min_delta=0.01, mode="min"):
        self.patience = patience
        self.min_delta = min_delta
        self.mode = mode  # "min" for loss, "max" for accuracy
        self.best = float("inf") if mode == "min" else float("-inf")
        self.counter = 0
        self.should_stop = False

    def check(self, metric):
        """Check if training should stop. Returns True if patience exhausted."""
        if self.mode == "min":
            improved = metric < self.best - self.min_delta
        else:
            improved = metric > self.best + self.min_delta

        if improved:
            self.best = metric
            self.counter = 0
        else:
            self.counter += 1

        if self.counter >= self.patience:
            self.should_stop = True

        return self.should_stop

    def reset(self):
        """Reset for next stage."""
        self.best = float("inf") if self.mode == "min" else float("-inf")
        self.counter = 0
        self.should_stop = False


# ============================================================================
# Adaptive Curriculum: Mastery tracking + difficulty adjustment
# ============================================================================

class MasteryTracker:
    """Track per-domain mastery using rolling accuracy windows."""

    def __init__(self):
        self.domains = {}  # domain -> {attempts: [], difficulty: float}

    def record(self, domain, loss, confidence):
        """Record a training attempt for a domain."""
        if domain not in self.domains:
            self.domains[domain] = {
                "attempts": [],
                "difficulty": 0.5,
                "total_attempts": 0,
                "total_correct": 0,
            }
        entry = self.domains[domain]
        # Accuracy proxy: low loss + high confidence = correct
        correct = loss is not None and loss < 0.5 and confidence > 0.4
        entry["attempts"].append(1.0 if correct else 0.0)
        entry["total_attempts"] += 1
        if correct:
            entry["total_correct"] += 1
        # Keep rolling window
        if len(entry["attempts"]) > MASTERY_WINDOW:
            entry["attempts"] = entry["attempts"][-MASTERY_WINDOW:]

    def get_mastery(self, domain):
        """Get current mastery level [0-1] for a domain."""
        if domain not in self.domains:
            return 0.0
        attempts = self.domains[domain]["attempts"]
        if not attempts:
            return 0.0
        return sum(attempts) / len(attempts)

    def get_difficulty(self, domain):
        """Get current difficulty level [0-1] for a domain."""
        if domain not in self.domains:
            return 0.5
        return self.domains[domain]["difficulty"]

    def adapt_difficulty(self, domain):
        """Adjust difficulty based on mastery — target ZPD."""
        mastery = self.get_mastery(domain)
        if domain not in self.domains:
            return 0.5
        entry = self.domains[domain]
        if mastery > MASTERY_THRESHOLD_HIGH:
            entry["difficulty"] = min(1.0, entry["difficulty"] + DIFFICULTY_STEP)
        elif mastery < MASTERY_THRESHOLD_LOW:
            entry["difficulty"] = max(0.1, entry["difficulty"] - DIFFICULTY_STEP)
        return entry["difficulty"]

    def select_domain(self, available_domains):
        """Select next domain — prioritize those in ZPD, avoid mastered ones."""
        scored = []
        for domain in available_domains:
            mastery = self.get_mastery(domain)
            # Score: distance from ZPD target (closer = higher priority)
            # Untried domains get high priority (novelty)
            if domain not in self.domains or len(self.domains[domain]["attempts"]) < 3:
                score = 1.0  # Unexplored — high priority
            else:
                score = 1.0 - abs(mastery - ZPD_TARGET)
            scored.append((domain, score))

        # Weighted random selection by score
        total = sum(s for _, s in scored)
        if total <= 0:
            return random.choice(available_domains)
        r = random.random() * total
        cumulative = 0
        for domain, score in scored:
            cumulative += score
            if r <= cumulative:
                return domain
        return scored[-1][0]

    def get_confidence_for_domain(self, domain, base_progress):
        """Compute training confidence from mastery + difficulty."""
        difficulty = self.get_difficulty(domain)
        mastery = self.get_mastery(domain)
        # Higher difficulty + higher mastery = higher training confidence
        return min(0.95, max(0.3, difficulty * 0.5 + mastery * 0.3 + base_progress * 0.2))

    def get_adaptive_lr(self, domain):
        """Compute adaptive learning rate based on domain mastery.

        Struggling domains (mastery < 0.3) get higher LR for faster learning.
        Mastered domains (mastery > 0.8) get lower LR for fine-tuning.
        ZPD domains use baseline LR.
        """
        mastery = self.get_mastery(domain)
        if mastery < MASTERY_THRESHOLD_LOW:
            lr = BASE_LEARNING_RATE * LR_SCALE_STRUGGLING
        elif mastery > MASTERY_THRESHOLD_HIGH:
            lr = BASE_LEARNING_RATE * LR_SCALE_MASTERED
        else:
            # Smooth interpolation within ZPD
            # At ZPD_TARGET, use BASE_LEARNING_RATE exactly
            # Scale linearly between thresholds
            lr = BASE_LEARNING_RATE
        return max(MIN_LEARNING_RATE, min(MAX_LEARNING_RATE, lr))

    def save(self, filepath):
        """Persist mastery state to JSON."""
        import json
        data = {}
        for domain, entry in self.domains.items():
            data[domain] = {
                "difficulty": entry["difficulty"],
                "total_attempts": entry["total_attempts"],
                "total_correct": entry["total_correct"],
                "recent_accuracy": self.get_mastery(domain),
            }
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, "w") as f:
            json.dump(data, f, indent=2)

    def load(self, filepath):
        """Load mastery state from JSON."""
        import json
        if not os.path.exists(filepath):
            return
        try:
            with open(filepath) as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            print(f"[WARN] Failed to load mastery state from {filepath}: {e}")
            return
        for domain, entry in data.items():
            self.domains[domain] = {
                "attempts": [],
                "difficulty": entry.get("difficulty", 0.5),
                "total_attempts": entry.get("total_attempts", 0),
                "total_correct": entry.get("total_correct", 0),
            }

    def summary(self):
        """Print mastery summary."""
        if not self.domains:
            return "  No mastery data yet."
        lines = []
        for domain in sorted(self.domains.keys()):
            m = self.get_mastery(domain)
            d = self.get_difficulty(domain)
            n = self.domains[domain]["total_attempts"]
            lines.append(f"    {domain:20s}: mastery={m:.2f} difficulty={d:.2f} attempts={n}")
        return "\n".join(lines)


# ============================================================================
# Sensory Composer: 1024-dim cortex-native input with biological context
# ============================================================================

class SensoryComposer:
    """Compose 1024-dim input vectors with embedded biological state."""

    def __init__(self, brain):
        self.brain = brain
        self._bio_cache = None
        self._bio_cache_time = 0.0
        self._ctx_cache = None
        self._ctx_cache_time = 0.0

    def compose(self, text=None, modality="text", primary_features=None,
                text_embedding=None):
        """Build a 1024-dim input vector with brain state context.

        Layout: [tag:16 | primary:640 | text:256 | context:112]
        """
        vec = np.zeros(BRAIN_INPUT_DIM, dtype=np.float32)

        # --- Tag section [0:16] ---
        modality_map = {"text": 0, "visual": 1, "audio": 2, "speech": 3,
                        "ethics": 4, "imagination": 5}
        mod_idx = modality_map.get(modality, 0)
        vec[mod_idx] = 1.0

        # Brain state in tags — cached to avoid 4 daemon round-trips per compose.
        # Brain state changes slowly (seconds); compose is called multiple times
        # per training step (~100ms apart). Cache for 5 seconds.
        import time as _time
        now = _time.monotonic()
        if self._bio_cache is None or now - self._bio_cache_time > 5.0:
            arousal, sleep_st, circadian, health_val = 0.5, 0.0, 1.0, 1.0
            try:
                arousal = self.brain.medulla_get_arousal()
            except Exception:
                pass
            try:
                sleep_st = float(self.brain.sleep_get_state()) / 4.0
            except Exception:
                pass
            try:
                circadian = self.brain.medulla_get_circadian_efficiency()
            except Exception:
                pass
            try:
                health = self.brain.substrate_get_health()
                health_map = {"OPTIMAL": 1.0, "STRESSED": 0.7,
                              "COMPROMISED": 0.4, "CRITICAL": 0.1}
                health_val = health_map.get(health, 0.5)
            except Exception:
                pass
            self._bio_cache = (arousal, sleep_st, circadian, health_val)
            self._bio_cache_time = now
        vec[8], vec[9], vec[10], vec[11] = self._bio_cache

        # --- Primary features [16:656] ---
        if primary_features is not None:
            pf = np.array(primary_features, dtype=np.float32)
            n = min(len(pf), PRIMARY_DIM)
            vec[TAG_DIM:TAG_DIM + n] = pf[:n]
        elif text is not None:
            emb = encode_text(text)  # 1024-dim
            # Pad to 640 with zeros
            n = min(len(emb), PRIMARY_DIM)
            vec[TAG_DIM:TAG_DIM + n] = emb[:n]

        # --- Text semantic embedding [656:912] ---
        if text_embedding is not None:
            te = np.array(text_embedding, dtype=np.float32)
            n = min(len(te), TEXT_DIM)
            vec[TAG_DIM + PRIMARY_DIM:TAG_DIM + PRIMARY_DIM + n] = te[:n]
        elif text is not None:
            emb = encode_text(text)  # 1024-dim — truncate to TEXT_DIM
            n = min(len(emb), TEXT_DIM)
            vec[TAG_DIM + PRIMARY_DIM:TAG_DIM + PRIMARY_DIM + n] = emb[:n]

        # --- Biological context [912:1024] ---
        ctx_start = TAG_DIM + PRIMARY_DIM + TEXT_DIM

        def _safe(v, default=0.0):
            """Sanitize biological values — NaN/Inf → default."""
            try:
                f = float(v)
                return f if np.isfinite(f) else default
            except (TypeError, ValueError):
                return default

        # Biological context — cached (same 5s TTL as tag brain state)
        if self._ctx_cache is None or now - self._ctx_cache_time > 5.0:
            ctx_vals = [0.5, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0]
            try:
                ctx_vals[0] = _safe(self.brain.medulla_get_arousal(), 0.5)
                ctx_vals[1] = _safe(self.brain.sleep_get_pressure())
                ctx_vals[2] = _safe(self.brain.medulla_get_circadian_efficiency(), 1.0)
            except Exception:
                pass
            try:
                ctx_vals[3] = _safe(self.brain.bg_get_dopamine())
                ctx_vals[4] = _safe(self.brain.bg_get_rpe())
                ctx_vals[5] = _safe(self.brain.bg_get_conflict())
                ctx_vals[6] = _safe(self.brain.bg_get_mode())
            except Exception:
                pass
            try:
                met = self.brain.substrate_get_metabolic()
                ctx_vals[7] = _safe(met.get("atp", 1.0), 1.0)
                ctx_vals[8] = _safe(met.get("capacity", 1.0), 1.0)
            except Exception:
                pass
            self._ctx_cache = ctx_vals
            self._ctx_cache_time = now
        vec[ctx_start:ctx_start + 9] = self._ctx_cache
        # LNN state (32 dims) at context_start + 16..48
        try:
            lnn_state = self.brain.lnn_get_state()
            if lnn_state:
                n = min(len(lnn_state), 32)
                for i in range(n):
                    v = float(lnn_state[i])
                    vec[ctx_start + 16 + i] = v if np.isfinite(v) else 0.0
        except Exception:
            pass

        # Safety: replace any remaining NaN/Inf with 0
        vec = np.nan_to_num(vec, nan=0.0, posinf=1.0, neginf=-1.0)
        return vec.tolist()


class TargetDiversifier:
    """Diversifies semantic targets to combat mode collapse.

    Three strategies work together:
    1. Category signal: Adds orthogonal one-hot category component
    2. Category rotation: Applies a fixed random rotation per category
    3. Mean subtraction: Subtracts running mean of recent targets

    Without diversification, "a brown cow" and "a black dog" produce embeddings
    with cosine similarity >0.8 — the network can't help but collapse them.
    With diversification, each category occupies a distinct region of target space.
    """

    # Category definitions — each text gets assigned by keyword matching
    CATEGORIES = [
        "animal", "nature", "food", "body", "home", "vehicle", "music",
        "weather", "tool", "science", "math", "philosophy", "psychology",
        "technology", "literature", "art", "emotion", "social", "space",
        "geography", "history", "language", "sport", "unknown"
    ]

    # Keywords for category detection
    _CATEGORY_KEYWORDS = {
        "animal": ["dog", "cat", "bird", "fish", "horse", "bear", "cow",
                   "elephant", "whale", "dolphin", "rabbit", "frog", "snake",
                   "butterfly", "bee", "owl", "lion", "tiger", "monkey",
                   "sheep", "pig", "chicken", "duck", "deer", "wolf",
                   "fox", "penguin", "turtle", "spider", "ant", "mouse",
                   "squirrel", "giraffe", "zebra", "eagle", "hawk",
                   "parrot", "beetle", "ladybug", "caterpillar", "peacock",
                   "rooster", "skunk", "chipmunk", "starling", "goldfish"],
        "nature": ["tree", "flower", "river", "mountain", "ocean", "cloud",
                   "rain", "snow", "forest", "desert", "volcano", "rainbow",
                   "moon", "sun", "star", "sunset", "sunrise", "lake",
                   "garden", "leaf", "seed", "grass", "vine", "moss",
                   "tide", "wave", "fog", "mist", "aurora", "northern lights"],
        "food": ["apple", "bread", "milk", "egg", "rice", "honey",
                 "banana", "cookie", "cheese", "orange", "strawberry",
                 "chocolate", "water", "juice", "soup", "cake", "pizza",
                 "potato", "carrot", "tomato", "pepper", "lemon", "grape",
                 "eggplant", "peel", "mint"],
        "body": ["hand", "eye", "heart", "brain", "finger", "arm",
                 "leg", "nose", "ear", "mouth", "teeth", "hair",
                 "skin", "bone", "blood", "muscle"],
        "home": ["house", "door", "window", "chair", "table", "bed",
                 "lamp", "book", "clock", "mirror", "pillow", "blanket",
                 "cup", "spoon", "plate", "candle", "key", "stairs",
                 "ceiling", "floor", "wall", "roof", "kitchen", "sink",
                 "radiator", "curtain", "fireplace"],
        "vehicle": ["car", "bus", "train", "bicycle", "boat", "airplane",
                    "truck", "ship", "rocket", "helicopter", "subway"],
        "music": ["music", "song", "piano", "guitar", "drum", "violin",
                  "trumpet", "flute", "melody", "rhythm", "harmony",
                  "orchestra", "choir", "cello", "accordion", "metronome",
                  "tempo", "note", "chord", "scale", "octave", "fugue",
                  "chimes", "wind chimes", "foghorn", "bell"],
        "weather": ["rain", "thunder", "lightning", "wind", "storm",
                    "tornado", "hurricane", "fog", "hail", "frost",
                    "breeze", "temperature", "climate"],
        "tool": ["hammer", "scissors", "needle", "pencil", "brush",
                 "wrench", "screwdriver", "shovel", "rope", "saw",
                 "magnifying glass", "prism", "kaleidoscope", "pinwheel",
                 "kite", "balloon"],
        "science": ["atom", "molecule", "cell", "DNA", "evolution",
                    "gravity", "energy", "photosynthesis", "electron",
                    "neuron", "synapse", "gene", "protein", "vaccine",
                    "antibiotic", "electromagnetic", "spectrum",
                    "tectonic", "mitochondria", "experiment"],
        "math": ["number", "equation", "theorem", "geometry", "algebra",
                 "calculus", "probability", "statistics", "fibonacci",
                 "prime", "pi ", "fraction", "exponential", "logarithm"],
        "philosophy": ["socrates", "plato", "aristotle", "descartes",
                       "kant", "ethics", "morality", "consciousness",
                       "free will", "existential", "epistemology",
                       "trolley problem", "meaning of life"],
        "psychology": ["memory", "emotion", "cognitive", "attachment",
                       "motivation", "placebo", "empathy", "creativity",
                       "bias", "stress", "anxiety", "perception"],
        "technology": ["computer", "internet", "algorithm", "software",
                       "encryption", "programming", "artificial intelligence",
                       "machine learning", "neural network", "quantum",
                       "digital", "robot", "database", "code"],
        "literature": ["shakespeare", "poetry", "novel", "fable",
                       "mythology", "metaphor", "narrative", "tragedy",
                       "comedy", "irony", "story", "author", "poem"],
        "art": ["painting", "sculpture", "color", "perspective",
                "photography", "architecture", "composition", "canvas",
                "abstract", "portrait", "dance", "gallery", "sketch"],
        "space": ["planet", "galaxy", "comet", "asteroid", "orbit",
                  "telescope", "constellation", "nebula", "mars",
                  "saturn", "jupiter", "milky way", "cosmos"],
    }

    def __init__(self, embedding_dim=1024, target_dim=BRAIN_OUTPUT_DIM,
                 category_weight=0.3, rotation_strength=0.2,
                 mean_sub_strength=0.5, mean_buffer_size=200):
        self.embedding_dim = embedding_dim
        self.target_dim = target_dim
        self.num_categories = len(self.CATEGORIES)
        self.category_weight = category_weight
        self.rotation_strength = rotation_strength
        self.mean_sub_strength = mean_sub_strength

        # Pre-generate a fixed random rotation matrix per category (seeded)
        rng = np.random.RandomState(42)
        self._rotations = {}
        for i, cat in enumerate(self.CATEGORIES):
            # Random orthogonal matrix via QR decomposition
            A = rng.randn(embedding_dim, embedding_dim).astype(np.float32)
            Q, _ = np.linalg.qr(A)
            # Blend with identity: R = (1-s)*I + s*Q
            self._rotations[cat] = (
                (1.0 - rotation_strength) * np.eye(embedding_dim, dtype=np.float32)
                + rotation_strength * Q
            )

        # Running mean of recent targets for mean subtraction
        self._mean_buffer = []
        self._mean_buffer_size = mean_buffer_size
        self._running_mean = np.zeros(embedding_dim, dtype=np.float32)

    @lru_cache(maxsize=8192)
    def _detect_category(self, text):
        """Detect category from text via keyword matching. Cached via lru_cache."""
        text_words = set(text.lower().split())
        best_cat = "unknown"
        best_count = 0
        for cat, keywords in self._CATEGORY_KEYWORDS.items():
            count = len(text_words & set(keywords))
            if count > best_count:
                best_count = count
                best_cat = cat
        return best_cat

    def diversify(self, embedding, text, category=None):
        """Apply all three diversification strategies to an embedding.

        Args:
            embedding: 1024-dim semantic embedding (numpy array)
            text: Original text (for category detection)
            category: Optional explicit category string

        Returns:
            Diversified embedding (1024-dim numpy array)
        """
        emb = np.array(embedding, dtype=np.float32)

        # 1. Category rotation — rotate embedding into category-specific subspace
        cat = category or self._detect_category(text)
        if cat in self._rotations:
            emb = self._rotations[cat] @ emb

        # 2. Category signal — add scaled one-hot in a reserved band
        cat_idx = self.CATEGORIES.index(cat) if cat in self.CATEGORIES else len(self.CATEGORIES) - 1
        cat_signal = np.zeros(self.embedding_dim, dtype=np.float32)
        # Spread the category signal across multiple dimensions for robustness
        band_start = (cat_idx * 40) % self.embedding_dim
        for k in range(40):
            dim = (band_start + k) % self.embedding_dim
            cat_signal[dim] = self.category_weight
        emb = emb + cat_signal

        # 3. Mean subtraction — push away from the running centroid
        if len(self._mean_buffer) > 10:
            emb = emb - self.mean_sub_strength * self._running_mean

        # Update running mean
        self._mean_buffer.append(emb.copy())
        if len(self._mean_buffer) > self._mean_buffer_size:
            self._mean_buffer.pop(0)
        self._running_mean = np.mean(self._mean_buffer, axis=0)

        # Re-normalize to preserve scale
        orig_norm = np.linalg.norm(embedding)
        new_norm = np.linalg.norm(emb)
        if new_norm > 1e-8 and orig_norm > 1e-8:
            emb = emb * (orig_norm / new_norm)

        return emb


# Global diversifier instance
_target_diversifier = TargetDiversifier()


def make_semantic_target(text, target_dim=None, category=None):
    """Create a diversified target vector from text.

    Uses three strategies to spread targets apart:
    1. Per-category rotation of the embedding space
    2. Category one-hot signal injection
    3. Running mean subtraction (push away from centroid)
    """
    if target_dim is None:
        target_dim = BRAIN_OUTPUT_DIM
    emb = encode_text(text)  # 1024-dim
    emb = _target_diversifier.diversify(emb, text, category=category)

    emb_len = len(emb)
    reps = (target_dim + emb_len - 1) // emb_len
    target = np.tile(emb, reps)[:target_dim].astype(np.float32)
    return target.tolist()


def tile_to_brain_input(embedding, dim=None):
    """Tile a shorter embedding to brain input dimension."""
    if dim is None:
        dim = BRAIN_INPUT_DIM
    emb = np.asarray(embedding, dtype=np.float32)
    emb_len = len(emb)
    reps = (dim + emb_len - 1) // emb_len
    return np.tile(emb, reps)[:dim].tolist()


# ---------------------------------------------------------------------------
# Layer B confabulation mitigation — "I don't know" curriculum
# ---------------------------------------------------------------------------
_IDK_CACHE = None       # lazy-loaded list of items from idk_unanswerables.json
_IDK_LOAD_FAILED = False  # set True after the first failed load to silence retries


def _resolve_stimulus_path(rel_path):
    """Resolve a stimulus JSON path relative to common roots.

    Try (in order): cwd, repo root inferred from this file, data/ symlink.
    Returns the first path that exists, or `rel_path` itself as a last resort.
    """
    candidates = [
        rel_path,
        os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), rel_path),
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", rel_path),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return rel_path


def _load_idk_unanswerables():
    """Load the Layer B 'I don't know' training corpus.

    Returns a list of stimuli dicts (with 'prompt', 'expected', 'metadata',
    etc.) or an empty list if the file is missing / malformed.
    Cached after first successful load.
    """
    global _IDK_CACHE, _IDK_LOAD_FAILED
    if _IDK_CACHE is not None:
        return _IDK_CACHE
    if _IDK_LOAD_FAILED:
        return []
    path = _resolve_stimulus_path(IDK_UNANSWERABLES_PATH)
    try:
        with open(path) as f:
            raw = json.load(f)
        stimuli = raw.get("stimuli", [])
        if not isinstance(stimuli, list):
            stimuli = []
        _IDK_CACHE = stimuli
        if stimuli:
            print(f"  [IDK] Loaded {len(stimuli)} unanswerable/OOD items "
                  f"from {path} (stage_gate={IDK_STAGE_GATE}, "
                  f"sampling_rate={IDK_SAMPLING_RATE:.0%})")
        return _IDK_CACHE
    except Exception as e:
        _IDK_LOAD_FAILED = True
        print(f"  [IDK] Failed to load {path}: {e} — Layer B disabled")
        return []


def _inject_idk_unanswerable(brain, composer, step, learning_rate,
                              stage=0, rng=None):
    """Inject one 'I don't know' training item.

    Stage-gated: only fires at IDK_STAGE_GATE+ (language tokens active) and
    only at IDK_SAMPLING_RATE probability per call.

    Target answer is the literal string "I don't know" — the brain learns
    the *pattern* of unanswerable inputs by seeing them paired with this
    canonical metacognitive token.

    Returns the loss from the training step, or None if skipped/failed.
    """
    if stage < IDK_STAGE_GATE:
        return None
    r = rng.random() if rng is not None else random.random()
    if r >= IDK_SAMPLING_RATE:
        return None
    items = _load_idk_unanswerables()
    if not items:
        return None
    pick = rng.choice(items) if rng is not None else random.choice(items)
    prompt = pick.get("prompt", "")
    if not prompt:
        return None
    # Canonical refusal target — matches the eval keyword set
    answer = "I don't know"
    category = pick.get("metadata", {}).get("category", "idk")
    label = f"idk_{category}"[:50]
    try:
        features = composer.compose(text=prompt, modality="text")
        target = make_semantic_target(answer, category=category)
        idk_lr = learning_rate * IDK_LR_SCALE
        loss = brain.learn_vector(features, target, label=label,
                                  confidence=0.7, learning_rate=idk_lr)
        return loss
    except Exception as e:
        if step < 100:  # only log early failures
            print(f"    [IDK] step {step} failed: {e}")
        return None


def _inject_cognitive_training(brain, composer, step, learning_rate,
                               spectral_splitter=None, fold_idx=0,
                               curriculum=None):
    """Inject one cognitive training item into the learning pipeline.

    Called every COGNITIVE_TRAIN_INTERVAL steps to exercise the 8 cognitive
    modules wired into brain_learn_vector(). The label prefix (e.g. 'ethics_',
    'rcog_', 'dragonfly_') triggers the corresponding C-side strstr() dispatch.

    Returns the loss from the cognitive training step, or None on failure.
    """
    # Cycle through all 9 domains evenly
    _ALL_COGNITIVE = get_all_cognitive_data()
    idx = step % len(_ALL_COGNITIVE)
    item = _ALL_COGNITIVE[idx]

    # Skip test items for this fold
    if spectral_splitter and spectral_splitter.is_test_item(item['label'], fold_idx):
        return None

    # Curriculum filtering — skip items from locked tiers
    if curriculum and not curriculum.filter_item(item):
        return None

    text = item["text"]
    answer = item["answer"]
    label = item["label"]

    # Encode: text → input features, answer → target
    try:
        features = composer.compose(text=text, modality="text")
        target = make_semantic_target(answer, category=item["domain"])
        cog_lr = learning_rate * COGNITIVE_TRAIN_LR_BOOST
        loss = brain.learn_vector(features, target, label=label,
                                  confidence=0.8, learning_rate=cog_lr)
        # Cross-modal grounding on the cognitive hot path. Every 5 sensory
        # steps we drop a cognitive item through here; binding its content
        # words to the text features is what fills total_bindings /
        # total_groundings in the grounded_language module. Without this,
        # those counters stay at 0 and respond() falls through to IDK.
        try:
            _ground_content_words(brain, text, features,
                                  modality=5)  # GL_MODALITY_LINGUISTIC
            if answer and answer != text:
                brain.learn_language_pair(text, answer, learning_rate=0.05)
        except Exception:
            pass
        # Echo-and-correct — supervised production training signal. Without
        # this, bridge weights stay uniform-random across 29K WordNet
        # word_pops and produce emits gibberish (the 2026-05-16 mode-
        # collapse symptom: "ptolemaic bundling compression ..." × 49/50).
        # _train_cognitive has the same block at line ~4737; this is
        # the per-step injection mirror. Safe to call here only after
        # cap-at-insertion bounds lexicon_bind cost (commit 13045d312) —
        # the earlier attempt without the cap caused writer-lock
        # starvation when B was unbounded.
        sup = answer or text
        if sup and len(sup) > 3:
            try:
                words = [w.strip(".,!?;:\"'()-").lower()
                         for w in sup.split()
                         if len(w.strip(".,!?;:\"'()-")) > 3]
                if words:
                    target_word = max(words, key=len)
                    brain.echo_and_correct(sup, target_word, lr_scale=8.0)
            except Exception:
                pass
        if curriculum:
            curriculum.record_loss(loss if loss else 0, item.get('domain', ''))
            curriculum.check_escalation()
        return loss
    except Exception as e:
        if step < 100:  # Only log early failures
            print(f"    [Cognitive] step {step} failed: {e}")
        return None


def _retroactive_cognitive_seed(brain, composer, completed_steps, learning_rate):
    """Retroactively train cognitive modules to sync with existing network state.

    When resuming from a checkpoint that was trained WITHOUT cognitive module
    wiring, run all 185 cognitive items through learn_vector() with their
    cognitive labels. This exercises all 8 C-side modules (ToM, RCOG, Ethics,
    Imagination, Introspection, Dragonfly, Portia, Collective) so they catch
    up with the rest of the network.

    The number of passes is proportional to how many steps were already
    completed: ~1 cognitive item per 10 sensory steps, so steps/10 total
    cognitive items spread across multiple full passes of the 185-item dataset.
    """
    all_data = get_all_cognitive_data()
    n_items = len(all_data)
    # How many cognitive items would have been trained if wired from the start
    target_count = completed_steps // COGNITIVE_TRAIN_INTERVAL
    n_passes = max(1, target_count // n_items)
    total_items = n_passes * n_items

    print(f"\n  [Cognitive Sync] Retroactive seeding: {n_passes} passes × "
          f"{n_items} items = {total_items} cognitive training steps")
    print(f"  [Cognitive Sync] Syncing 8 modules with {completed_steps} "
          f"steps of existing network state...")

    losses = []
    domains_seen = set()
    for pass_num in range(n_passes):
        # Shuffle each pass for diversity (but deterministic per pass)
        rng = random.Random(42 + pass_num)
        order = list(range(n_items))
        rng.shuffle(order)

        # Decrease LR each pass (later passes are refinement)
        pass_lr = learning_rate * COGNITIVE_TRAIN_LR_BOOST * (1.0 - 0.15 * pass_num)
        pass_lr = max(pass_lr, learning_rate * 0.5)

        # Batch learn_vector calls — prepare all features/targets locally,
        # then send in chunks of 8 to reduce socket round-trips.
        BATCH_SIZE = 8
        batch_pairs = []
        batch_labels = []
        for j, idx in enumerate(order):
            item = all_data[idx]
            try:
                features = composer.compose(text=item["text"], modality="text")
                target = make_semantic_target(item["answer"],
                                              category=item["domain"])
                batch_pairs.append((features, target))
                batch_labels.append(item["label"])
                domains_seen.add(item["domain"])
            except Exception:
                pass

            # Flush batch when full or at end
            if len(batch_pairs) >= BATCH_SIZE or j == len(order) - 1:
                if batch_pairs:
                    try:
                        avg_loss = brain.learn_vector_batch(
                            batch_pairs, learning_rate=pass_lr)
                        if avg_loss is not None:
                            losses.append(avg_loss)
                    except Exception:
                        # Fallback: individual calls
                        for f, t in batch_pairs:
                            try:
                                loss = brain.learn_vector(f, t, confidence=0.8,
                                                         learning_rate=pass_lr)
                                if loss is not None:
                                    losses.append(loss)
                            except Exception:
                                pass
                    batch_pairs = []
                    batch_labels = []

            # Progress every 50 items
            step_total = pass_num * n_items + j + 1
            if step_total % 50 == 0:
                avg = sum(losses[-50:]) / max(len(losses[-50:]), 1)
                print(f"    [Cognitive Sync] {step_total}/{total_items} "
                      f"loss={avg:.4f} domains={len(domains_seen)}/9")

    avg_loss = sum(losses) / max(len(losses), 1) if losses else 0
    print(f"  [Cognitive Sync] Done — {len(losses)} items trained, "
          f"avg_loss={avg_loss:.4f}, {len(domains_seen)} domains covered\n")
    return losses


def batch_make_semantic_targets(texts, target_dim=BRAIN_OUTPUT_DIM):
    """Batch-compute diversified semantic targets."""
    embeddings = batch_encode_texts(texts)
    targets = []
    for emb, text in zip(embeddings, texts):
        emb = _target_diversifier.diversify(emb, text)
        target = np.zeros(target_dim, dtype=np.float32)
        for i in range(0, target_dim, len(emb)):
            n = min(len(emb), target_dim - i)
            target[i:i + n] = emb[:n]
        targets.append(target.tolist())
    return targets


class ContrastiveRegularizer:
    """Combats mode collapse by pushing outputs apart for different inputs.

    Maintains a rolling buffer of (input_embedding, output_vector) pairs.
    Every `interval` steps, samples pairs from the buffer and applies
    contrastive corrections: if two outputs are very similar (high cosine)
    but their inputs are dissimilar, we push the outputs apart by training
    each to move AWAY from the other.

    This gives the brain a gradient signal: "different inputs should produce
    different outputs."
    """

    def __init__(self, buffer_size=200, interval=50, batch_size=16,
                 similarity_threshold=0.90, min_input_distance=0.3,
                 strength=0.3):
        """
        Args:
            buffer_size: Max recent pairs to keep.
            interval: Apply contrastive correction every N steps.
            batch_size: Number of pairs to sample per correction.
            similarity_threshold: Output cosine sim above this triggers correction.
            min_input_distance: Minimum input dissimilarity to count as "different".
            strength: Blend factor for anti-target (0=no correction, 1=full push).
        """
        self.buffer_size = buffer_size
        self.interval = interval
        self.batch_size = batch_size
        self.sim_threshold = similarity_threshold
        self.min_input_dist = min_input_distance
        self.strength = strength

        self._input_buf = []   # list of 1024-dim input embeddings
        self._output_buf = []  # list of output vectors (full dim)
        self._feature_buf = [] # list of brain input features (for re-training)
        self._step = 0
        self._corrections_applied = 0
        self._total_checks = 0

    def record(self, input_embedding, output_vector, features):
        """Record an (input, output, features) triple."""
        inp = np.asarray(input_embedding, dtype=np.float32).ravel()
        out = np.asarray(output_vector, dtype=np.float32).ravel()
        self._input_buf.append(inp)
        self._output_buf.append(out)
        self._feature_buf.append(features)
        if len(self._input_buf) > self.buffer_size:
            self._input_buf.pop(0)
            self._output_buf.pop(0)
            self._feature_buf.pop(0)
        self._step += 1

    def should_correct(self):
        """Check if it's time to apply contrastive corrections."""
        return (self._step % self.interval == 0
                and len(self._input_buf) >= self.batch_size * 2)

    def apply_corrections(self, brain, learning_rate=None):
        """Sample pairs from buffer and apply contrastive push-apart signals.

        For each pair (A, B) where outputs are too similar but inputs differ:
          - Train A's input features toward (A_output - strength * B_output)
          - Train B's input features toward (B_output - strength * A_output)

        This pushes each output AWAY from the other while preserving its
        general direction, creating differentiation pressure.

        Returns: (num_corrections, mean_output_similarity)
        """
        n = len(self._input_buf)
        if n < self.batch_size * 2:
            return 0, 0.0

        # Sample indices
        indices = np.random.choice(n, size=min(self.batch_size * 2, n),
                                   replace=False)

        # Compute output embeddings (extract 1024-dim from full output)
        out_embs = []
        for idx in indices:
            out_embs.append(
                extract_embedding_from_output(self._output_buf[idx]))
        out_embs = np.array(out_embs, dtype=np.float32)

        # Compute input embeddings
        in_embs = np.array([self._input_buf[idx] for idx in indices],
                           dtype=np.float32)

        # Pairwise output cosine similarity
        out_norms = np.linalg.norm(out_embs, axis=1, keepdims=True)
        out_norms = np.maximum(out_norms, 1e-8)
        out_normed = out_embs / out_norms

        in_norms = np.linalg.norm(in_embs, axis=1, keepdims=True)
        in_norms = np.maximum(in_norms, 1e-8)
        in_normed = in_embs / in_norms

        out_sim = out_normed @ out_normed.T
        in_sim = in_normed @ in_normed.T

        corrections = 0
        sims_found = []
        # Cap corrections per cycle to avoid multi-minute stalls
        MAX_CORRECTIONS_PER_CYCLE = 4

        lr_kwargs = {"learning_rate": learning_rate} if learning_rate else {}

        for i in range(len(indices)):
            for j in range(i + 1, len(indices)):
                o_sim = float(out_sim[i, j])
                i_sim = float(in_sim[i, j])
                input_distance = 1.0 - i_sim

                sims_found.append(o_sim)
                self._total_checks += 1

                # High output similarity + low input similarity = mode collapse
                if o_sim > self.sim_threshold and input_distance > self.min_input_dist:
                    idx_a = indices[i]
                    idx_b = indices[j]

                    # Anti-target for A: push away from B
                    out_a = np.array(self._output_buf[idx_a], dtype=np.float32)
                    out_b = np.array(self._output_buf[idx_b], dtype=np.float32)

                    anti_a = out_a - self.strength * out_b
                    anti_b = out_b - self.strength * out_a

                    # Normalize to same scale as original targets
                    norm_a = np.linalg.norm(anti_a)
                    norm_b = np.linalg.norm(anti_b)
                    if norm_a > 1e-8:
                        anti_a = anti_a * (np.linalg.norm(out_a) / norm_a)
                    if norm_b > 1e-8:
                        anti_b = anti_b * (np.linalg.norm(out_b) / norm_b)

                    try:
                        brain.learn_vector(
                            self._feature_buf[idx_a], anti_a.tolist(),
                            label="contrastive", confidence=0.7,
                            **lr_kwargs)
                        brain.learn_vector(
                            self._feature_buf[idx_b], anti_b.tolist(),
                            label="contrastive", confidence=0.7,
                            **lr_kwargs)
                        corrections += 1
                    except Exception:
                        pass

                    if corrections >= MAX_CORRECTIONS_PER_CYCLE:
                        break
            if corrections >= MAX_CORRECTIONS_PER_CYCLE:
                break

        self._corrections_applied += corrections
        mean_sim = float(np.mean(sims_found)) if sims_found else 0.0
        return corrections, mean_sim

    def stats_str(self):
        return (f"contrastive: {self._corrections_applied} corrections, "
                f"{self._total_checks} pairs checked, "
                f"buf={len(self._input_buf)}")


class DiversityRegularizer:
    """Combats mode collapse via output decorrelation.

    Maintains a rolling buffer of recent output embeddings. Every `interval`
    steps, computes the covariance matrix of outputs and generates corrective
    targets that push outputs toward underrepresented directions.

    Key insight: mode collapse means the output covariance has one dominant
    eigenvalue. We fix this by training outputs to be more spread along
    the minor eigenvectors.

    Works alongside ContrastiveRegularizer:
      - Contrastive: pairwise push-apart (local correction)
      - Diversity: global decorrelation (structural correction)
    """

    def __init__(self, buffer_size=100, interval=100, num_corrections=8,
                 strength=0.2, min_variance_ratio=0.1):
        """
        Args:
            buffer_size: Number of recent outputs to track.
            interval: Apply decorrelation every N steps.
            num_corrections: Number of corrective targets per application.
            strength: How far to push toward minor eigenvectors (0-1).
            min_variance_ratio: Trigger correction when top eigenvalue
                accounts for more than (1 - min_variance_ratio) of variance.
        """
        self.buffer_size = buffer_size
        self.interval = interval
        self.num_corrections = num_corrections
        self.strength = strength
        self.min_variance_ratio = min_variance_ratio

        self._output_buf = []    # list of 1024-dim output embeddings
        self._feature_buf = []   # corresponding brain input features
        self._step = 0
        self._corrections_applied = 0
        self._last_top_ratio = 0.0
        self._last_effective_rank = 0.0

    def record(self, output_vector, features):
        """Record an output embedding and its input features."""
        out_emb = extract_embedding_from_output(
            np.asarray(output_vector, dtype=np.float32))
        self._output_buf.append(out_emb)
        self._feature_buf.append(features)
        if len(self._output_buf) > self.buffer_size:
            self._output_buf.pop(0)
            self._feature_buf.pop(0)
        self._step += 1

    def should_correct(self):
        """Check if it's time to apply decorrelation."""
        return (self._step % self.interval == 0
                and len(self._output_buf) >= 30)

    def apply_corrections(self, brain, learning_rate=None):
        """Analyze output covariance and generate decorrelation targets.

        Algorithm:
          1. Compute covariance of recent output embeddings
          2. Eigendecompose to find dominant vs. minor directions
          3. If variance is too concentrated (top eigenvalue dominates):
             - Sample outputs from the buffer
             - For each, create a target that shifts it toward a random
               minor eigenvector, spreading the output distribution

        Returns: (num_corrections, top_eigenvalue_ratio, effective_rank)
        """
        n = len(self._output_buf)
        if n < 30:
            return 0, 0.0, 0.0

        embs = np.array(self._output_buf, dtype=np.float32)

        # Center the embeddings
        mean_emb = embs.mean(axis=0)
        centered = embs - mean_emb

        # Covariance (use smaller dimension for efficiency)
        # If n < embed_dim, compute n×n Gram matrix instead
        if n < centered.shape[1]:
            gram = centered @ centered.T / n
            eigenvalues, eigenvectors_small = np.linalg.eigh(gram)
            # Map back to full space
            eigenvalues = np.maximum(eigenvalues, 0)
            idx = np.argsort(eigenvalues)[::-1]
            eigenvalues = eigenvalues[idx]
            eigenvectors_small = eigenvectors_small[:, idx]
            # Full eigenvectors: V = X^T @ U @ diag(1/sqrt(λ))
            nonzero = eigenvalues > 1e-8
            eigenvectors = centered.T @ eigenvectors_small[:, nonzero]
            norms = np.linalg.norm(eigenvectors, axis=0, keepdims=True)
            norms = np.maximum(norms, 1e-8)
            eigenvectors = eigenvectors / norms
        else:
            cov = centered.T @ centered / n
            eigenvalues, eigenvectors = np.linalg.eigh(cov)
            idx = np.argsort(eigenvalues)[::-1]
            eigenvalues = eigenvalues[idx]
            eigenvectors = eigenvectors[:, idx]

        # Compute variance concentration
        total_var = eigenvalues.sum()
        if total_var < 1e-8:
            return 0, 0.0, 0.0

        top_ratio = float(eigenvalues[0] / total_var)
        # Effective rank: exp(entropy of normalized eigenvalues)
        normed_evals = eigenvalues / total_var
        normed_evals = normed_evals[normed_evals > 1e-10]
        effective_rank = float(np.exp(-np.sum(normed_evals * np.log(normed_evals))))

        self._last_top_ratio = top_ratio
        self._last_effective_rank = effective_rank

        # Only correct if variance is too concentrated
        if top_ratio < (1.0 - self.min_variance_ratio):
            return 0, top_ratio, effective_rank

        # Select minor eigenvectors (bottom half)
        num_eigs = len(eigenvalues)
        minor_start = max(1, num_eigs // 2)
        minor_vecs = eigenvectors[:, minor_start:min(minor_start + 20, num_eigs)]
        if minor_vecs.shape[1] == 0:
            return 0, top_ratio, effective_rank

        # Generate corrective targets
        lr_kwargs = {"learning_rate": learning_rate} if learning_rate else {}
        corrections = 0
        sample_idx = np.random.choice(n, size=min(self.num_corrections, n),
                                       replace=False)

        for idx in sample_idx:
            out_emb = self._output_buf[idx]
            features = self._feature_buf[idx]

            # Pick a random minor eigenvector
            minor_idx = np.random.randint(minor_vecs.shape[1])
            direction = minor_vecs[:, minor_idx]

            # Create target: shift output toward this minor direction
            # Scale by strength and the output's norm
            out_norm = np.linalg.norm(out_emb)
            if out_norm < 1e-8:
                continue

            shifted = out_emb + self.strength * out_norm * direction
            # Normalize to same scale
            shifted = shifted * (out_norm / max(np.linalg.norm(shifted), 1e-8))

            # Tile back to full output dimension
            target_dim = len(features) if hasattr(features, '__len__') else 2048
            # Use BRAIN_OUTPUT_DIM if available
            target_dim = BRAIN_OUTPUT_DIM
            target = np.zeros(target_dim, dtype=np.float32)
            for j in range(0, target_dim, len(shifted)):
                end = min(j + len(shifted), target_dim)
                target[j:end] = shifted[:end - j]

            try:
                brain.learn_vector(
                    features, target.tolist(),
                    label="diversity", confidence=0.3,
                    **lr_kwargs)
                corrections += 1
            except Exception:
                pass

        self._corrections_applied += corrections
        return corrections, top_ratio, effective_rank

    def stats_str(self):
        return (f"diversity: {self._corrections_applied} corr, "
                f"top_eig={self._last_top_ratio:.2f}, "
                f"eff_rank={self._last_effective_rank:.1f}, "
                f"buf={len(self._output_buf)}")


class StimulusPrefetcher:
    """Pre-generates and batch-encodes stimuli in a background thread.

    Overlaps embedding computation with brain training for ~2x throughput.
    """

    def __init__(self, source, composer, batch_size=64):
        from threading import Thread
        from queue import Queue
        self.source = source
        self.composer = composer
        self.batch_size = batch_size
        self.queue = Queue(maxsize=2)  # 2 batches buffered
        self._stop = False
        self._thread = Thread(target=self._worker, daemon=True)
        self._thread.start()

    def _generate_descriptions(self, n):
        descs = []
        for _ in range(n):
            if random.random() < 0.5:
                descs.append(self.source.get_sensory())
            else:
                desc, _ = generate_sensory_exposure()
                descs.append(desc)
        return descs

    def _worker(self):
        while not self._stop:
            descs = self._generate_descriptions(self.batch_size)
            # Batch-encode all targets at once (~10x faster than one-by-one)
            targets = batch_make_semantic_targets(descs)
            # Compose features (uses cached embeddings where possible)
            features_list = [self.composer.compose(text=d, modality="text") for d in descs]
            batch = list(zip(descs, features_list, targets))
            self.queue.put(batch)

    def get_batch(self):
        """Get next pre-computed batch of (description, features, target) tuples."""
        return self.queue.get()

    def stop(self):
        self._stop = True


# ============================================================================
# Stimulus Source: built-in developmental corpus
# ============================================================================

class StimulusSource:
    """Progressive developmental corpus — scales from basic sensory to complex reasoning.

    Claude generates interactive teaching — this corpus provides the raw
    stimuli that Claude narrates and teaches from. Each domain scales in
    complexity across stages.
    """

    def __init__(self):
        pass

    # ===== STAGE 0: Sensory grounding — raw perception =====
    SENSORY = [
        # Visual
        "warm sunlight on skin", "the color red", "bright yellow light",
        "the color blue", "shadows moving on the wall", "green leaves",
        "a rainbow after rain", "white fluffy clouds", "orange sunset",
        "purple flowers", "pink blossoms", "golden wheat fields",
        "deep blue ocean", "brown earth", "silver moonlight",
        "flickering candlelight", "sparkling water drops", "frost on glass",
        "snow falling softly", "dew on morning grass", "fire dancing",
        "lightning across the sky", "steam rising from a cup",
        "light through stained glass", "reflection in a puddle",
        # Tactile
        "soft fur of a sleeping cat", "rough bark of a tree",
        "smooth cold glass", "warm sand between toes", "cool water on hands",
        "weight of a heavy book", "silk sliding through fingers",
        "the prick of a thorn", "mud squishing between toes",
        "ice melting in your palm", "velvet softness", "cold metal",
        "warm bread in your hands", "feather lightness", "gritty sandpaper",
        # Auditory
        "birds singing at dawn", "rain tapping on a roof",
        "a clock ticking steadily", "wind howling through trees",
        "waves crashing on shore", "a dog barking in distance",
        "thunder rumbling", "leaves crunching underfoot",
        "a heartbeat rhythm", "a mother's voice humming",
        "water dripping", "wind chimes tinkling",
        "a baby laughing", "crickets at night", "crackling fire",
        "church bells ringing", "distant train whistle",
        # Olfactory/Gustatory
        "the smell of fresh bread", "pine forest scent",
        "the taste of sweetness", "salt on the tongue",
        "the smell of rain on earth", "fresh cut grass",
        "cinnamon and spices", "the tang of lemon",
        "chocolate melting", "coffee brewing", "ocean salt air",
        # Emotional/Somatic
        "a warm hug", "being startled", "comfort of a blanket",
        "peacefulness of quiet morning", "excitement of something new",
        "the drowsiness before sleep", "waking up refreshed",
        "butterflies in the stomach", "laughing until sides hurt",
        "the ache of missing someone", "relief after worry passes",
    ]

    # ===== STAGE 1: Object recognition — naming and categories =====
    OBJECTS = [
        # Animals
        ("dog", "A friendly dog with soft fur that wags its tail when happy"),
        ("cat", "A cat with soft fur that purrs and stretches in sunlight"),
        ("bird", "A bird with feathers that flies through the air singing"),
        ("fish", "A fish that swims gracefully through clear water"),
        ("horse", "A strong horse that gallops across green fields"),
        ("butterfly", "A butterfly with colorful wings that floats on the breeze"),
        ("rabbit", "A soft rabbit with long ears that hops through grass"),
        ("bear", "A large bear with thick fur that lives in the forest"),
        ("dolphin", "A playful dolphin that leaps through ocean waves"),
        ("elephant", "A huge elephant with a long trunk and big floppy ears"),
        ("owl", "A wise owl with big round eyes that sees in the dark"),
        ("bee", "A busy bee that flies from flower to flower collecting nectar"),
        ("snake", "A snake that slithers silently through tall grass"),
        ("frog", "A green frog that jumps and croaks by the pond"),
        ("whale", "An enormous whale that sings deep in the ocean"),
        # Nature
        ("tree", "A tall tree with green leaves reaching up to the sky"),
        ("flower", "A beautiful flower with colorful petals and sweet scent"),
        ("sun", "The bright warm sun that lights up the day and gives life"),
        ("moon", "The silvery moon that glows in the night sky"),
        ("star", "A tiny bright star twinkling far away in space"),
        ("river", "A river flowing steadily between banks of earth and stone"),
        ("mountain", "A towering mountain with snow on its peak"),
        ("ocean", "The vast blue ocean stretching to the horizon"),
        ("cloud", "A white cloud drifting lazily across the blue sky"),
        ("rain", "Drops of rain falling from clouds to nourish the earth"),
        ("snow", "Soft white snow that blankets the world in silence"),
        ("forest", "A deep green forest full of life and mystery"),
        ("desert", "A vast sandy desert stretching under a blazing sun"),
        ("volcano", "A powerful volcano with fire and smoke inside"),
        ("rainbow", "A rainbow arcing across the sky with seven colors"),
        # Food
        ("apple", "A red apple that tastes sweet and crunchy"),
        ("bread", "Warm bread fresh from the oven with a golden crust"),
        ("milk", "Cool white milk that nourishes growing bodies"),
        ("egg", "An egg with a hard shell protecting new life inside"),
        ("rice", "Tiny grains of rice that feed billions of people"),
        ("honey", "Golden sticky honey made by thousands of busy bees"),
        # Objects
        ("ball", "A round bouncy ball you can throw and catch"),
        ("book", "A book with pages full of stories and knowledge"),
        ("house", "A house where people live and feel safe and warm"),
        ("chair", "A chair that holds you when you sit and rest"),
        ("clock", "A clock with hands that mark the passing of time"),
        ("mirror", "A mirror that reflects back what stands before it"),
        ("candle", "A candle with a flickering flame giving warm light"),
        ("key", "A small metal key that opens locked doors"),
        ("bridge", "A bridge that connects two sides across water or land"),
        ("wheel", "A round wheel that rolls and makes things move"),
        # Body
        ("hand", "A hand with five fingers that can hold and create"),
        ("eye", "An eye that sees the world in all its color and detail"),
        ("heart", "A heart that beats steadily to keep us alive"),
        ("brain", "A brain that thinks and remembers and imagines"),
        ("ear", "An ear that hears music and voices and whispers"),
    ]

    # ===== STAGE 2: Facts and relationships — building knowledge =====
    FACTS = [
        # Biology basics
        ("Dogs are mammals that have been human companions for thousands of years", "biology"),
        ("Plants make their own food from sunlight through photosynthesis", "biology"),
        ("Fish breathe through gills that extract oxygen from water", "biology"),
        ("Birds have hollow bones which makes them light enough to fly", "biology"),
        ("The heart pumps blood through the body carrying oxygen to cells", "biology"),
        ("Trees produce oxygen that all animals need to breathe", "biology"),
        ("Butterflies go through metamorphosis from caterpillar to adult", "biology"),
        ("Whales are mammals that breathe air but live in the ocean", "biology"),
        ("Seeds contain tiny plants waiting for water and warmth to grow", "biology"),
        ("Bacteria are tiny living things too small to see without a microscope", "biology"),
        ("DNA contains the instructions for building every living thing", "biology"),
        ("Cells are the basic building blocks of all living organisms", "biology"),
        # Physics basics
        ("The sun is a star that provides light and heat to Earth", "physics"),
        ("Water freezes at zero degrees Celsius and boils at one hundred", "physics"),
        ("Gravity pulls everything toward the center of the Earth", "physics"),
        ("Light travels faster than anything else in the universe", "physics"),
        ("Sound travels through air as waves of pressure", "physics"),
        ("The Earth orbits the sun once every year", "physics"),
        ("The moon orbits the Earth causing tides in the ocean", "physics"),
        ("Energy cannot be created or destroyed only transformed", "physics"),
        ("Atoms are the tiny building blocks of all matter", "physics"),
        ("Electricity flows through conductors like metal wires", "physics"),
        # Intuitive Physics (world model scenarios — predictions about physical outcomes)
        ("A ball dropped from a height falls faster and faster until it hits the ground", "physics"),
        ("A cup pushed off a table falls and breaks on the floor", "physics"),
        ("If you stack blocks and remove one from the middle the blocks above fall", "physics"),
        ("A ball thrown upward slows down stops then falls back down", "physics"),
        ("Heavy objects and light objects fall at the same speed in vacuum", "physics"),
        ("A rolling ball on a flat surface gradually slows down due to friction", "physics"),
        ("When two billiard balls collide they exchange momentum", "physics"),
        ("An object stays at rest unless a force acts on it", "physics"),
        ("Spinning tops stay upright because of angular momentum", "physics"),
        ("A pendulum swings back and forth trading height for speed", "physics"),
        ("Objects float in water if they are less dense than water", "physics"),
        ("Levers multiply force so a small push lifts a heavy load", "physics"),
        ("A ball bounces lower each time because energy is lost to heat", "physics"),
        ("Wind pushes objects in the direction it blows", "physics"),
        ("Ice is slippery because friction is very low on its surface", "physics"),
        # Chemistry (world model scenarios — predictions about chemical outcomes)
        ("Mixing baking soda and vinegar creates bubbles of carbon dioxide gas", "chemistry"),
        ("Iron left in rain slowly rusts as it reacts with oxygen and water", "chemistry"),
        ("Salt dissolves in water because water molecules pull apart the crystals", "chemistry"),
        ("Burning wood combines carbon with oxygen releasing heat and light", "chemistry"),
        ("Mixing acids and bases neutralizes both and can produce salt and water", "chemistry"),
        ("Heating water to one hundred degrees Celsius turns it into steam", "chemistry"),
        ("Cooling water to zero degrees Celsius turns it into ice", "chemistry"),
        ("Plants convert carbon dioxide and water into glucose using sunlight", "chemistry"),
        ("Batteries convert chemical energy into electrical energy", "chemistry"),
        ("Soap molecules have one end that grabs grease and one end that grabs water", "chemistry"),
        ("Baking uses heat to trigger chemical reactions that make dough rise", "chemistry"),
        ("Milk turns sour because bacteria convert lactose into lactic acid", "chemistry"),
        ("Rust is iron oxide which forms when iron reacts with oxygen and moisture", "chemistry"),
        ("The color of a flame depends on what chemical is burning", "chemistry"),
        ("Carbon dioxide makes soda fizzy and escapes as bubbles when opened", "chemistry"),
        # Biology (world model scenarios — predictions about biological outcomes)
        ("Plants grow toward sunlight because cells on the dark side grow faster", "biology"),
        ("When wolves were removed from Yellowstone the deer overpopulated and damaged forests", "biology"),
        ("A cut on your skin heals because cells divide to replace the damaged ones", "biology"),
        ("If you exercise regularly your muscles grow stronger and larger", "biology"),
        ("Seeds need water warmth and sometimes light to germinate and grow", "biology"),
        ("Antibiotics kill bacteria but do not work against viruses", "biology"),
        ("Predator populations crash when prey populations decline", "biology"),
        ("A child inherits half their DNA from each parent", "biology"),
        ("Your body temperature stays near thirty-seven degrees through homeostasis", "biology"),
        ("Trees lose their leaves in autumn to conserve water during winter", "biology"),
        ("Coral bleaching happens when ocean water gets too warm", "biology"),
        ("Vaccines teach your immune system to recognize pathogens before infection", "biology"),
        ("Decomposers break down dead organisms returning nutrients to the soil", "biology"),
        ("Ecosystems collapse when keystone species are removed", "biology"),
        ("Photosynthesis in oceans produces most of the oxygen we breathe", "biology"),
        # Geography
        ("The Earth has seven continents and five oceans", "geography"),
        ("Mountains form when tectonic plates push together", "geography"),
        ("Rivers flow from high ground toward the sea", "geography"),
        ("Deserts receive very little rainfall each year", "geography"),
        ("The equator divides the Earth into northern and southern halves", "geography"),
        ("Volcanoes release magma from deep within the Earth", "geography"),
        ("Glaciers are slow-moving rivers of ice that carve valleys", "geography"),
        ("Coral reefs are underwater ecosystems built by tiny animals", "geography"),
        # Mathematics
        ("Two plus two equals four", "math"),
        ("A triangle has three sides and three angles", "math"),
        ("A circle has no corners and every point is the same distance from center", "math"),
        ("Multiplication is repeated addition", "math"),
        ("Patterns repeat in predictable ways that we can learn to recognize", "math"),
        ("Zero is neither positive nor negative", "math"),
        ("Fractions represent parts of a whole", "math"),
        ("Symmetry means both sides are mirror images of each other", "math"),
        # Language
        ("A sentence expresses a complete thought with subject and verb", "language"),
        ("Nouns name people places and things", "language"),
        ("Verbs describe actions or states of being", "language"),
        ("Adjectives describe qualities of nouns like big small or beautiful", "language"),
        ("Questions end with a question mark and seek information", "language"),
        ("Metaphors compare unlike things to create meaning", "language"),
        ("Stories have a beginning a middle and an end", "language"),
        # History
        ("Ancient Egyptians built the pyramids thousands of years ago", "history"),
        ("The printing press made books available to many more people", "history"),
        ("Humans first walked on the moon in nineteen sixty nine", "history"),
        ("Agriculture began about ten thousand years ago changing human life", "history"),
        ("The wheel was invented thousands of years ago revolutionizing transport", "history"),
        ("Writing was first developed in ancient Mesopotamia", "history"),
        # Ethics
        ("Kindness means treating others with warmth and care", "ethics"),
        ("Fairness means giving everyone an equal chance", "ethics"),
        ("Honesty means telling the truth even when it is difficult", "ethics"),
        ("Courage means doing what is right even when afraid", "ethics"),
        ("Empathy means understanding how someone else feels", "ethics"),
        ("Responsibility means keeping your promises and doing your duties", "ethics"),
        ("Forgiveness means letting go of anger toward someone who hurt you", "ethics"),
        ("Respect means treating others as you would want to be treated", "ethics"),
    ]

    # ===== STAGE 3: Reasoning, dialogue, and deeper knowledge =====
    QUESTIONS = [
        # Self-reflection
        "What makes you happy?",
        "What is your favorite thing to learn about?",
        "What do you think dreams are?",
        "What does it mean to be a good friend?",
        "Why do we feel sad sometimes?",
        "What makes something beautiful?",
        "How do you know when you've learned something new?",
        "What would you change about the world if you could?",
        # Science reasoning
        "Why do you think the sky is blue?",
        "Why does ice float on water?",
        "How do birds know where to migrate?",
        "Why do we need sleep?",
        "What causes the seasons to change?",
        "Why are snowflakes all different?",
        "How does a seed know which way to grow?",
        "Why does the moon change shape through the month?",
        # Ethics and empathy
        "What would you do if you found a hurt animal?",
        "Why is it important to be kind?",
        "How can we help someone who is lonely?",
        "Is it ever okay to tell a lie?",
        "What makes an action right or wrong?",
        "Should everyone be treated equally?",
        "What would a fair world look like?",
        "How do you decide what the right thing to do is?",
        # Philosophy
        "What is the difference between knowing and believing?",
        "Can a machine truly understand something?",
        "What does it mean to be conscious?",
        "Is there a difference between being smart and being wise?",
        "What makes something alive?",
        "Can you describe what color looks like to someone who cannot see?",
        "What is time?",
        "What makes you different from everyone else?",
        # Complex reasoning
        "If all cats are animals and all animals breathe, what can we conclude about cats?",
        "A farmer has 12 apples and gives away a third. How many remain?",
        "What comes next in the pattern: 2, 4, 8, 16, ...?",
        "If it takes 5 machines 5 minutes to make 5 widgets, how long does it take 100 machines to make 100 widgets?",
        "The day before two days after the day before tomorrow is Saturday. What day is today?",
        # Creative / Imaginative
        "If you could talk to any animal, which would you choose and why?",
        "Invent a new color and describe what it looks like.",
        "Write a short story about a raindrop's journey from cloud to ocean.",
        "If gravity disappeared for one hour, what would happen?",
        "Describe a world where music is visible.",
    ]

    # ===== Advanced domains (used in stages 2-3 with Claude) =====
    ADVANCED_TOPICS = {
        "science": [
            "How evolution shapes species over millions of years",
            "The structure of DNA and how it encodes life's instructions",
            "How neurons in the brain communicate through synapses",
            "The water cycle and how it sustains life on Earth",
            "Why antibiotics stop working when bacteria evolve resistance",
            "How black holes form when massive stars collapse",
            "The role of mitochondria as the powerhouse of cells",
            "How vaccines train the immune system to fight disease",
            "The electromagnetic spectrum from radio waves to gamma rays",
            "Plate tectonics and why continents drift across the Earth",
        ],
        "mathematics": [
            "The Fibonacci sequence appears throughout nature",
            "Pi is the ratio of a circle's circumference to its diameter",
            "Prime numbers are divisible only by one and themselves",
            "Probability measures how likely an event is to occur",
            "Algebra uses symbols to represent unknown quantities",
            "The Pythagorean theorem relates the sides of right triangles",
            "Exponential growth doubles repeatedly and outpaces linear growth",
            "Statistics helps us understand patterns in large datasets",
            "Geometry describes the properties of shapes and space",
            "Calculus studies how things change continuously",
        ],
        "philosophy": [
            "Socrates believed the unexamined life is not worth living",
            "The trolley problem asks whether it is right to sacrifice one to save many",
            "Descartes said I think therefore I am as proof of existence",
            "The ship of Theseus asks what makes something the same thing over time",
            "Kant argued we should act only according to rules we would want everyone to follow",
            "Free will asks whether our choices are truly our own",
            "The meaning of life is one of philosophy's oldest questions",
            "Epistemology studies what knowledge is and how we can be sure of it",
            "Existentialism says we create our own meaning through choices",
            "The problem of consciousness asks why there is subjective experience at all",
        ],
        "psychology": [
            "Memory works through encoding storage and retrieval processes",
            "Emotions serve as signals about our needs and environment",
            "Cognitive biases are systematic errors in how we think",
            "Attachment theory explains how early bonds shape relationships",
            "Motivation can come from inside intrinsic or outside extrinsic",
            "The placebo effect shows that belief alone can cause real changes",
            "Sleep is essential for memory consolidation and learning",
            "Stress activates the fight or flight response in the body",
            "Empathy allows us to understand and share the feelings of others",
            "Creativity involves making new connections between existing ideas",
        ],
        "technology": [
            "Computers process information using billions of tiny switches",
            "The internet connects computers worldwide through networks",
            "Artificial intelligence learns patterns from large amounts of data",
            "Encryption protects information by converting it into secret codes",
            "Programming languages translate human instructions into computer actions",
            "Machine learning finds patterns without being explicitly programmed",
            "Neural networks are inspired by how biological brains process information",
            "The world wide web was invented to share scientific documents",
            "Algorithms are step by step instructions for solving problems",
            "Quantum computers use quantum physics for certain calculations",
        ],
        "literature": [
            "Shakespeare wrote plays that explored every facet of human nature",
            "Poetry uses rhythm and imagery to express emotions concisely",
            "Mythology contains stories that explain the origins of the world",
            "Fables use animal characters to teach moral lessons",
            "Novels create entire worlds and complex characters over hundreds of pages",
            "Metaphor makes the unfamiliar familiar by comparing unlike things",
            "Irony means the opposite of what is literally said",
            "Narrative perspective shapes how we understand a story",
            "Tragedy shows how great people can fall through fatal flaws",
            "Comedy reveals truth about society through humor and wit",
        ],
        "music": [
            "Rhythm is the pattern of sounds and silences in time",
            "Melody is a sequence of notes that forms a recognizable tune",
            "Harmony occurs when multiple notes sound pleasing together",
            "Music can evoke emotions from joy to sadness to excitement",
            "Different cultures develop unique musical scales and traditions",
            "The octave divides into twelve semitones in Western music",
            "Tempo is the speed at which music is played",
            "Dynamics describe how loud or soft music is performed",
            "Counterpoint weaves independent melodies together",
            "Music notation records sound as symbols on paper",
        ],
        "art": [
            "Color theory explains how colors interact and affect emotions",
            "Perspective creates the illusion of depth on a flat surface",
            "Sculpture gives form to ideas using physical materials",
            "Abstract art expresses ideas through shapes and colors rather than realism",
            "Architecture is both functional and artistic design of buildings",
            "Photography captures moments of light and shadow",
            "Dance uses the body to express emotion and tell stories",
            "Composition is how elements are arranged within a work of art",
            "Art movements reflect the cultural values of their time",
            "Creativity is the ability to see the world in new ways",
        ],
    }

    def get_sensory(self):
        return random.choice(self.SENSORY)

    # Compositional generators for unlimited training variety
    _ADJECTIVES = ["red", "blue", "green", "yellow", "big", "small", "soft", "hard",
                   "warm", "cold", "wet", "dry", "old", "new", "shiny", "dull",
                   "round", "flat", "tall", "short", "heavy", "light", "smooth", "rough",
                   "bright", "dark", "striped", "spotted", "fuzzy", "spiky"]
    _LOCATIONS = ["on the table", "next to the chair", "under the tree", "in the basket",
                  "near the window", "beside the door", "on the grass", "in the water",
                  "against the wall", "on the shelf", "in the garden", "by the fireplace",
                  "on the bed", "under the blanket", "in the box", "at the edge"]
    _ACTIONS = ["running", "sleeping", "eating", "playing", "hiding", "jumping",
                "swimming", "flying", "sitting", "standing", "rolling", "spinning",
                "falling", "growing", "melting", "shining", "dripping", "floating",
                "bouncing", "swinging", "crawling", "stretching", "dancing", "singing"]
    _RELATIONS = ["next to", "above", "below", "behind", "in front of", "inside",
                  "on top of", "beside", "between", "under", "near", "far from"]
    _NUMBERS = ["one", "two", "three", "four", "five"]

    def get_object(self):
        """Get an object for naming — mixes static objects with generated compositions.

        40% static objects (from OBJECTS list)
        15% adjective + object (property variation)
        15% object + location (spatial)
        10% object + action (dynamic)
        10% object + relation + object (relationships)
        5% number + objects (counting)
        5% negation (contrast)
        """
        r = random.random()

        if r < 0.40:
            # Static object
            return random.choice(self.OBJECTS)

        # Get two random base objects for compositions
        obj1_name, obj1_desc = random.choice(self.OBJECTS)
        obj2_name, obj2_desc = random.choice(self.OBJECTS)
        while obj2_name == obj1_name:
            obj2_name, obj2_desc = random.choice(self.OBJECTS)

        if r < 0.55:
            # Adjective + object
            adj = random.choice(self._ADJECTIVES)
            name = f"{adj} {obj1_name}"
            desc = f"A {adj} {obj1_name}. {obj1_desc.rstrip('.')}. This one is {adj}."
            return (name, desc)

        if r < 0.70:
            # Object + location
            loc = random.choice(self._LOCATIONS)
            name = f"{obj1_name} {loc}"
            desc = f"{obj1_desc.rstrip('.')} — this one is {loc}."
            return (name, desc)

        if r < 0.80:
            # Object + action
            act = random.choice(self._ACTIONS)
            name = f"{obj1_name} {act}"
            desc = f"{obj1_desc.rstrip('.')} — and look, it is {act}!"
            return (name, desc)

        if r < 0.90:
            # Relationship: object1 + relation + object2
            rel = random.choice(self._RELATIONS)
            name = f"{obj1_name} {rel} {obj2_name}"
            desc = (f"Look! The {obj1_name} is {rel} the {obj2_name}. "
                    f"{obj1_desc.rstrip('.')} and {obj2_desc.rstrip('.').lower()}.")
            return (name, desc)

        if r < 0.95:
            # Counting
            num = random.choice(self._NUMBERS)
            name = f"{num} {obj1_name}s"
            desc = f"Can you count? There are {num} {obj1_name}s! {obj1_desc}"
            return (name, desc)

        # Negation (contrast)
        name = f"not {obj2_name} but {obj1_name}"
        desc = (f"This is NOT a {obj2_name} — look carefully! "
                f"It is a {obj1_name}. {obj1_desc}")
        return (name, desc)

    def get_fact(self, preferred_domain=None):
        """Get a fact, optionally preferring a specific domain."""
        if preferred_domain:
            domain_facts = [(f, d) for f, d in self.FACTS if d == preferred_domain]
            if domain_facts:
                return random.choice(domain_facts)
        return random.choice(self.FACTS)

    def get_question(self):
        return random.choice(self.QUESTIONS)

    # Codebase snippets cache (loaded lazily)
    _codebase_snippets = None

    @classmethod
    def _load_codebase(cls):
        """Load Athena's own codebase as learning material."""
        if cls._codebase_snippets is not None:
            return cls._codebase_snippets

        import glob
        nimcp_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
        snippets = []

        # C source files — Athena's core brain implementation
        c_patterns = [
            "src/core/brain/**/*.c", "src/core/brain/**/*.h",
            "src/language/**/*.c", "src/language/**/*.h",
            "src/cognitive/**/*.c", "src/cognitive/**/*.h",
            "src/api/*.c", "include/*.h",
        ]
        for pattern in c_patterns:
            for fpath in glob.glob(os.path.join(nimcp_root, pattern), recursive=True):
                try:
                    with open(fpath, 'r', errors='ignore') as f:
                        content = f.read()
                    # Extract function-level chunks with surrounding context
                    lines = content.split('\n')
                    chunk = []
                    fname = os.path.relpath(fpath, nimcp_root)
                    for line in lines:
                        chunk.append(line)
                        if len(chunk) >= 15:
                            text = '\n'.join(chunk)
                            if any(kw in text for kw in ['void ', 'int ', 'float ', 'static ',
                                                          'nimcp_', 'brain_', 'typedef ', 'struct ']):
                                snippets.append((text[:500], f"code:{fname}", "codebase"))
                            chunk = chunk[10:]  # sliding window
                except Exception:
                    pass

        # Python source files — Athena's training and interface code
        py_patterns = [
            "scripts/immerse_athena.py", "scripts/claude_teacher.py",
            "scripts/neural_decoder.py", "scripts/talk_to_athena.py",
            "frontend/backend/brain_manager.py",
            "frontend/backend/cognitive_response.py",
        ]
        for pattern in py_patterns:
            fpath = os.path.join(nimcp_root, pattern)
            if os.path.exists(fpath):
                try:
                    with open(fpath, 'r') as f:
                        content = f.read()
                    lines = content.split('\n')
                    chunk = []
                    fname = pattern
                    for line in lines:
                        chunk.append(line)
                        if len(chunk) >= 12:
                            text = '\n'.join(chunk)
                            if any(kw in text for kw in ['def ', 'class ', 'import ',
                                                          'brain.', 'nimcp.']):
                                snippets.append((text[:500], f"code:{fname}", "codebase"))
                            chunk = chunk[8:]
                except Exception:
                    pass

        # Documentation files
        doc_patterns = ["docs/claude/*.md", "CLAUDE.md"]
        for pattern in doc_patterns:
            for fpath in glob.glob(os.path.join(nimcp_root, pattern)):
                try:
                    with open(fpath, 'r') as f:
                        content = f.read()
                    # Split into paragraphs
                    paragraphs = content.split('\n\n')
                    fname = os.path.relpath(fpath, nimcp_root)
                    for para in paragraphs:
                        para = para.strip()
                        if len(para) > 50:
                            snippets.append((para[:500], f"doc:{fname}", "codebase"))
                except Exception:
                    pass

        random.shuffle(snippets)
        cls._codebase_snippets = snippets
        logger.info("Loaded %d codebase snippets for self-learning", len(snippets))
        return snippets

    def get_codebase_snippet(self):
        """Get a snippet from Athena's own codebase."""
        snippets = self._load_codebase()
        if not snippets:
            return "Athena is a neural brain built in C with biological subsystems", "code:nimcp", "codebase"
        return random.choice(snippets)

    def get_advanced(self, domain=None):
        """Get an advanced topic for deeper learning stages."""
        if domain and domain in self.ADVANCED_TOPICS:
            items = self.ADVANCED_TOPICS[domain]
        else:
            domain = random.choice(list(self.ADVANCED_TOPICS.keys()))
            items = self.ADVANCED_TOPICS[domain]
        return random.choice(items), domain


# Extend sensory data with expanded corpus (1000+ items)
if _EXPANDED_SENSORY_AVAILABLE:
    StimulusSource.SENSORY = StimulusSource.SENSORY + get_all_sensory()
    StimulusSource.OBJECTS = StimulusSource.OBJECTS + get_expanded_objects()


# ============================================================================
# Biological Clock: circadian rhythm + substrate health + sleep pressure
# ============================================================================

class BiologicalClock:
    """Medulla-driven biological rhythm management."""

    def __init__(self, rest_interval=2000):
        self.rest_interval = rest_interval
        self.stimuli_since_rest = 0
        self.dt = 0.1  # simulated time step in seconds

    def tick(self, brain):
        """Advance biological clock. Returns action needed or None."""
        try:
            brain.update_medulla(self.dt)
        except Exception:
            pass

        self.stimuli_since_rest += 1

        try:
            if brain.sleep_is_needed() or brain.sleep_get_pressure() > 0.8:
                return "sleep"
        except Exception:
            pass

        try:
            health = brain.substrate_get_health()
            if health in ("COMPROMISED", "CRITICAL"):
                return "rest"
        except Exception:
            pass

        if self.stimuli_since_rest >= self.rest_interval:
            return "consolidate"

        # Arousal regulation
        try:
            arousal = brain.medulla_get_arousal()
            if arousal < 0.3:
                brain.medulla_boost_arousal(0.15)
            elif arousal > 0.85:
                brain.medulla_reduce_arousal(0.1)
        except Exception:
            pass

        return None

    def do_sleep(self, brain, parent=None):
        """Run a sleep cycle with consolidation."""
        logger.info("  [Sleep] Athena is sleeping... (consolidation)")
        try:
            brain.set_plasticity_state("CONSOLIDATION")
        except Exception:
            pass

        try:
            brain.sleep_run_cycle(2)
        except Exception:
            pass

        try:
            brain.consolidate(mode="auto")
        except Exception:
            pass

        if parent:
            parent.encourage_dreaming(brain)

        try:
            brain.set_plasticity_state("ACQUISITION")
        except Exception:
            pass

        self.stimuli_since_rest = 0
        logger.info("  [Sleep] Athena wakes up refreshed")

    def do_consolidate(self, brain):
        """Light consolidation between sleep cycles."""
        logger.info("  [Consolidate] Light consolidation...")
        try:
            brain.consolidate(mode="light")
        except Exception:
            pass
        self.stimuli_since_rest = 0


# ============================================================================
# Parent: Claude as Athena's loving parent, teacher, and mentor
# ============================================================================

class Parent:
    """Claude as Athena's loving parent.

    Pre-generates all narrations/lessons for each stage in a single Claude
    call, then pops from the cache during training — zero blocking.
    """

    def __init__(self, teacher=None, enabled=True, decoder=None,
                 multimodal=None):
        self.teacher = teacher
        self.enabled = enabled and teacher is not None
        self.decoder = decoder
        self.multimodal = multimodal  # MultimodalDataLoader instance
        self.interaction_count = 0
        self.milestones = []
        self.moral_lessons = []
        # Pre-generated content pools (filled by pre_generate_content)
        self._narrations = []       # first_experience / show_and_name narrations
        self._encouragements = []   # ask_and_encourage praise/patience lines
        self._moral_stories = []    # teach_moral_lesson stories
        self._speech_prompts = []   # teach_speech encouragements
        self._inspirations = []     # inspire passages
        self._dream_prompts = []    # encourage_dreaming questions
        self._conversation_replies = []  # stage 3 conversational responses
        # Slice C — caregiver critic. Lazily created on first use (so the
        # import doesn't run for the no-Claude smoke path); cached per stage
        # to avoid reloading the vocab JSON on every cognitive injection.
        self._critic_by_stage = {}     # stage_idx -> CaregiverCritic
        self._current_critic_stage = None
        self._critic_log_every = 10    # log one verdict per N applied verdicts

    def pre_generate_content(self, stage, num_stimuli):
        """Load pre-generated narrations for a stage.

        Tries to load from the content cache file (generated before brain init
        when memory is available). Falls back to Claude CLI call, then to
        built-in narrations.
        """
        # Slice C — track current developmental stage so the critic fires at
        # the correct rule set even when callers of _train_cognitive don't
        # pass stage explicitly. pre_generate_content is invoked once per
        # stage at the top of run_stage_N, so this is the single chokepoint
        # for "what stage are we in now?".
        self._current_critic_stage = int(stage)
        # Pre-warm the critic so the first cognitive injection of the stage
        # doesn't pay the JSON load latency.
        self._get_critic(int(stage))
        if not self.enabled:
            print("  [Parent] Claude disabled — using silent parenting")
            return

        # Try loading from cache first (generated before brain ate all RAM)
        try:
            if os.path.exists(CONTENT_CACHE):
                with open(CONTENT_CACHE) as f:
                    cache = json.load(f)
                stage_key = str(stage)
                if stage_key in cache:
                    data = cache[stage_key]
                    self._narrations = data.get("narrations", [])
                    self._encouragements = data.get("encouragements", [])
                    self._moral_stories = data.get("moral_stories", [])
                    self._speech_prompts = data.get("speech_prompts", [])
                    self._inspirations = data.get("inspirations", [])
                    self._dream_prompts = data.get("dreams", [])
                    self._conversation_replies = data.get("conversations", [])
                    print(f"  [Parent] Loaded {len(self._narrations)} narrations from cache "
                          f"({len(self._encouragements)} encouragements, "
                          f"{len(self._moral_stories)} moral stories)")
                    return
        except Exception as e:
            print(f"  [Parent] Cache load failed: {e}")

        # Calculate how many of each type we need
        n_narrations = min(num_stimuli, 60)  # variety pool, cycled across stimuli; one big-JSON call must finish < timeout
        n_encouragements = 60
        n_morals = max(5, num_stimuli // 2000)
        n_speech = max(10, num_stimuli // 300)
        n_inspirations = max(5, num_stimuli // 2000)
        n_dreams = 10
        n_conversations = 30  # stage 3 only

        stage_desc = {
            0: "newborn (0-3 months) — everything is new and wondrous",
            1: "infant (3-12 months) — learning names, recognizing objects",
            2: "toddler (1-2 years) — starting to babble and respond",
            3: "young child (2-4 years) — asking questions, forming opinions",
        }.get(stage, "young child")

        prompt = f"""You are a loving, warm parent raising a child at stage: {stage_desc}.
Generate the following content as a JSON object. Each entry should be 1-3 sentences, warm, encouraging, using simple words.

Return ONLY valid JSON (no markdown fences, no commentary):
{{
  "narrations": ["{n_narrations} unique narrations introducing sensory experiences to a baby/child. Each should express wonder at something in the world (light, colors, sounds, textures, animals, nature). Vary widely."],
  "encouragements": ["{n_encouragements} encouraging/praising sentences for when the child is learning. Half should be for good progress, half for patience when struggling."],
  "moral_stories": ["{n_morals} very short moral stories (3-4 sentences each) about sharing, kindness, truth, helping others, fairness, forgiveness. End each with a gentle question."],
  "speech_prompts": ["{n_speech} encouraging prompts for teaching a child to repeat words. Like 'Can you say dog? Doooog! Good try!'"],
  "inspirations": ["{n_inspirations} inspiring 2-3 sentence passages about natural wonders, beauty, curiosity, the universe. Fill with genuine wonder."],
  "dreams": ["{n_dreams} simple beautiful 'what if' questions that spark wonder. Just the questions."],
  "conversations": ["{n_conversations} warm conversational responses a parent might give when their child shows them something or tries to communicate. 1-2 sentences each."]
}}

IMPORTANT: Return actual arrays with the requested number of strings, not descriptions. Each element must be a unique string."""

        print(f"  [Parent] Pre-generating content for stage {stage}...")
        t0 = time.time()

        try:
            # Quick connectivity test — if Claude CLI takes >20s for a trivial prompt,
            # skip the expensive pre-generation and use fallbacks immediately.
            import subprocess as _sp
            try:
                _test = _sp.run(
                    ["claude", "-p", "Say OK", "--output-format", "text",
                     "--system-prompt", "",
                     "--setting-sources", "", "--allowed-tools", ""],
                    capture_output=True, text=True, timeout=20,
                    stdin=_sp.DEVNULL,  # don't wait 3s for stdin (rc=1 in daemonized trainer)
                    env={k: v for k, v in os.environ.items()
                         if k not in ("CLAUDECODE", "CUDA_VISIBLE_DEVICES")}
                )
                if not _test.stdout.strip():
                    raise RuntimeError("Claude CLI returned empty output")
            except Exception as e:
                raise RuntimeError(f"Claude CLI connectivity test failed: {e}")

            # Single Claude call with generous token budget.
            # Pre-gen is a one-time cost per stage — allow 5 min for large JSON.
            old_timeout = self.teacher.timeout
            self.teacher.timeout = 600  # one-time per-stage cost; headroom for the big-JSON generation
            try:
                raw = self.teacher._call_claude(prompt, max_tokens=4096)
            finally:
                self.teacher.timeout = old_timeout

            # Parse JSON — strip markdown fences if present
            raw = raw.strip()
            if raw.startswith("```"):
                # Remove ```json ... ```
                lines = raw.split("\n")
                lines = [l for l in lines if not l.strip().startswith("```")]
                raw = "\n".join(lines)

            data = json.loads(raw)

            self._narrations = data.get("narrations", [])
            self._encouragements = data.get("encouragements", [])
            self._moral_stories = data.get("moral_stories", [])
            self._speech_prompts = data.get("speech_prompts", [])
            self._inspirations = data.get("inspirations", [])
            self._dream_prompts = data.get("dreams", [])
            self._conversation_replies = data.get("conversations", [])

            elapsed = time.time() - t0
            print(f"  [Parent] Pre-generated {len(self._narrations)} narrations, "
                  f"{len(self._encouragements)} encouragements, "
                  f"{len(self._moral_stories)} moral stories, "
                  f"{len(self._speech_prompts)} speech prompts, "
                  f"{len(self._inspirations)} inspirations — "
                  f"{elapsed:.1f}s (1 Claude call)")

        except Exception as e:
            print(f"  [Parent] Pre-generation failed: {e}")
            print(f"  [Parent] Falling back to built-in narrations")
            self._generate_fallback_content(stage)

    def _generate_fallback_content(self, stage):
        """Built-in narrations if Claude is unavailable."""
        self._narrations = [
            "Look at that! Isn't it beautiful?",
            "Oh, do you see that? How wonderful!",
            "What a lovely thing to discover!",
            "Can you feel that? It's so interesting!",
            "Wow, look at all those colors!",
            "Listen! Do you hear that sound?",
            "Feel how soft this is!",
            "Look how it moves! Fascinating!",
            "See how the light dances?",
            "What a magical world we live in!",
            "Oh, that's new! Let's look closer.",
            "Isn't nature amazing?",
            "Do you see how beautiful that is?",
            "What a wonderful thing to experience!",
            "Let me show you something special.",
        ] * 15  # 225 entries
        self._encouragements = [
            "You're doing so well, little one!",
            "That's wonderful! Keep exploring!",
            "I'm so proud of you!",
            "Good try! You'll get it!",
            "That's okay, learning takes time.",
            "Every attempt makes you stronger!",
            "You're growing so fast!",
            "Keep going, you're amazing!",
        ] * 8
        self._moral_stories = [
            "Once a little bird shared its food with a hungry friend. The friend smiled and they both felt warm inside. How do you think sharing made them feel?",
            "A puppy accidentally knocked over a flower pot. It felt bad and helped clean up. Saying sorry and fixing things takes courage, doesn't it?",
            "Two kittens wanted the same ball of yarn. They decided to take turns. Both got to play and both were happy. Why is taking turns a good idea?",
        ]
        self._speech_prompts = [
            "Can you say it? Try! Good job!",
            "Listen carefully... now you try! Wonderful!",
            "Say it with me! You're getting better!",
            "One more time! You almost had it!",
        ] * 5
        self._inspirations = [
            "Did you know that every snowflake is unique, just like you? The world is full of beautiful things waiting to be discovered.",
            "The stars above have been shining for billions of years, and tonight they're shining just for you. Isn't that wonderful?",
            "A tiny seed can grow into a mighty tree. Growth takes time and patience, but the result is magnificent.",
        ] * 5
        self._dream_prompts = [
            "What if clouds were made of cotton candy?",
            "What if fish could fly and birds could swim?",
            "What if the moon was a giant nightlight?",
            "What if flowers could sing?",
            "What if you could talk to animals?",
        ]
        self._conversation_replies = [
            "That's really interesting! Tell me more!",
            "I love how you think about things!",
            "What a curious mind you have!",
            "That's a great observation!",
        ] * 8

    def _pop_content(self, pool_name):
        """Pick an item from a content pool without repetition.

        Uses a shuffled shadow deck; refills when exhausted so no item
        repeats until every item in the pool has been used once.
        """
        pool = getattr(self, pool_name)
        if not pool:
            return None

        deck_name = pool_name + "_deck"
        deck = getattr(self, deck_name, None)
        if not deck:
            deck = list(pool)
            random.shuffle(deck)
            setattr(self, deck_name, deck)

        item = deck.pop()

        if not deck:
            deck = list(pool)
            random.shuffle(deck)
            setattr(self, deck_name, deck)

        return item

    def _say(self, prompt, max_tokens=128):
        """Get a pre-generated parent narration (zero latency).

        Falls back to live Claude call only if pools are exhausted and
        no fallback content was generated.
        """
        if not self.enabled:
            return None
        # Try to pop from the narrations pool (general purpose)
        if self._narrations:
            return self._pop_content("_narrations")
        # If pools exhausted, try live call as last resort
        try:
            return self.teacher._call_claude(prompt, max_tokens=max_tokens)
        except Exception as e:
            logger.debug("Parent speech failed: %s", e)
            return None

    def _show(self, brain, composer, concept=None):
        """Show Athena a real image through her visual SNN bridge.

        Loads a real photograph from CIFAR-100 and submits it as raw pixel
        data via brain.submit_sensory("visual", ...). Also returns the
        composed feature vector and teaching text for paired learning.

        Args:
            brain: The brain instance.
            composer: FeatureComposer for building input vectors.
            concept: Optional concept to match (e.g. "dog", "butterfly").

        Returns:
            (features, target, label, description) or None if no data.
        """
        if not self.multimodal:
            return None

        sample = self.multimodal.get_visual(concept)
        if not sample:
            return None

        raw_bytes, w, h, ch, label, desc = sample

        # Submit real pixels to the visual SNN bridge
        try:
            brain.submit_sensory("visual", raw_bytes, width=w, height=h,
                                 channels=ch)
        except Exception as e:
            logger.debug("Visual submit failed: %s", e)

        # Build text features and target for paired learning
        teaching_text = f"Look at this {label}! {desc}"
        features = composer.compose(text=teaching_text, modality="visual")
        target = make_semantic_target(teaching_text)

        return features, target, label, desc

    def _tell(self, brain, composer, concept=None):
        """Play Athena a real sound through her audio SNN bridge.

        Loads a real environmental audio clip from ESC-50 and submits it
        via brain.submit_sensory("audio", ...). Also returns the composed
        feature vector and teaching text for paired learning.

        Args:
            brain: The brain instance.
            composer: FeatureComposer for building input vectors.
            concept: Optional concept to match (e.g. "rain", "dog").

        Returns:
            (features, target, label, description) or None if no data.
        """
        if not self.multimodal:
            return None

        sample = self.multimodal.get_audio(concept)
        if not sample:
            return None

        audio_samples, label, desc = sample

        # Submit real audio to the audio SNN bridge. Mel-transform first
        # (n_mels=128) — Audio cortex CNN expects 128-element features,
        # not raw 32000-sample waveforms. Without this, ESC-50 frames
        # silently bypass the cortex.
        try:
            mel = audio_to_mel(audio_samples, n_mels=128)
            brain.submit_sensory("audio", mel)
        except Exception as e:
            logger.debug("Audio submit failed: %s", e)

        # Build text features and target for paired learning
        teaching_text = f"Listen! {desc}"
        features = composer.compose(text=teaching_text, modality="audio")
        target = make_semantic_target(teaching_text)

        return features, target, label, desc

    def _show_and_tell(self, brain, composer, concept=None):
        """Show AND play — cross-modal paired learning.

        Loads a matched image+audio pair (e.g. train image + train sound)
        and submits both to their respective SNN bridges simultaneously.
        This creates temporal co-occurrence that STDP can bind into a
        unified concept representation.

        Returns:
            (features, target, concept, teaching_text) or None.
        """
        if not self.multimodal:
            return None

        pair = self.multimodal.get_multimodal_pair()
        if not pair:
            return None

        visual = pair["visual"]
        audio_data = pair["audio"]
        concept_name = pair["concept"]
        teaching = pair["teaching_text"]

        # Submit visual
        if visual:
            raw_bytes, w, h, ch, _, _ = visual
            try:
                brain.submit_sensory("visual", raw_bytes, width=w, height=h,
                                     channels=ch)
            except Exception:
                pass

        # Submit audio (mel-transform first — see _tell_audio for rationale)
        if audio_data:
            audio_samples, _, _ = audio_data
            try:
                mel = audio_to_mel(audio_samples, n_mels=128)
                brain.submit_sensory("audio", mel)
            except Exception:
                pass

        features = composer.compose(text=teaching, modality="text")
        target = make_semantic_target(teaching)

        return features, target, concept_name, teaching

    def _tell_speech(self, brain, composer, word=None):
        """Play Athena a real spoken word through her speech SNN bridge.

        Loads a real spoken word clip from Speech Commands and submits it
        via brain.submit_sensory("speech", ...).

        Returns:
            (features, target, word, description) or None.
        """
        if not self.multimodal:
            return None

        sample = self.multimodal.get_speech(word)
        if not sample:
            return None

        audio_samples, word_label, desc = sample

        # Submit to speech bridge
        try:
            brain.submit_sensory("speech", audio_samples)
        except Exception as e:
            logger.debug("Speech submit failed: %s", e)

        teaching_text = f"Can you hear? {desc}"
        features = composer.compose(text=teaching_text, modality="speech")
        target = make_semantic_target(word_label)

        return features, target, word_label, desc

    # -- Slice C: caregiver critic helpers ----------------------------------

    def _get_critic(self, stage):
        """Get-or-create a CaregiverCritic for the given developmental stage.

        Cached per stage so we don't reload the vocab JSON on every cognitive
        injection. ``stage`` is the integer training stage (0..3+).
        """
        if stage is None:
            return None
        try:
            crit = self._critic_by_stage.get(stage)
            if crit is None:
                # Lazy import — keeps module-level imports light, and lets
                # smoke runs that never train still work even if data files
                # aren't present.
                from caregiver_critic import CaregiverCritic
                crit = CaregiverCritic(stage=stage)
                self._critic_by_stage[stage] = crit
            self._current_critic_stage = stage
            return crit
        except Exception as e:
            # Don't let a critic problem kill training. Log once per stage.
            if stage not in getattr(self, "_critic_warned_stages", set()):
                self._critic_warned_stages = getattr(
                    self, "_critic_warned_stages", set()) | {stage}
                print(f"  [Critic] init failed for stage {stage}: {e}")
            return None

    def run_critic_eval(self, brain, prompt, stage=None):
        """Score one production against the developmental-stage rules and
        deliver the resulting reward/recast into the brain.

        Reusable critic driver. The original critic hook lived inside
        _train_cognitive, but the live stage loops (run_stage_0/1/2) never
        call _train_cognitive — their hot path goes straight to
        learn_vector/decide_full. So the loops call this directly at the
        cognitive cadence. Best-effort; never raises. Returns the verdict
        (or None if no production / no critic / error)."""
        if not prompt:
            return None
        try:
            grounded = brain.grounded_respond(prompt)
            response_text = ""
            if isinstance(grounded, dict):
                response_text = (grounded.get("response")
                                 or grounded.get("text") or "")
            effective_stage = stage if stage is not None else self._current_critic_stage
            if not response_text or effective_stage is None:
                return None
            critic = self._get_critic(effective_stage)
            if critic is None:
                return None
            verdict = critic.evaluate(prompt, response_text)
            return self._apply_critic_verdict(
                brain, verdict, prompt, response_text, effective_stage)
        except Exception:
            return None

    def _apply_critic_verdict(self, brain, verdict, prompt, response, stage):
        """Drive reward + (optional) recast into the brain.

        Best-effort — every RPC is guarded so the critic never wedges training.
        Returns the verdict for the caller to inspect / log.
        """
        if verdict is None:
            return None

        # 1. Deliver reward through whichever RPC the daemon currently
        #    exposes. apply_reward_learning is the target (matches the
        #    C-side path that drives DA into the SNN); bg_update_reward is
        #    the basal-ganglia path that's already wired. We prefer the
        #    former when available, fall back to the latter so the slice
        #    works without waiting on Slice D's RPC merge.
        if abs(verdict.reward) > 1e-6:
            applied = False
            try:
                fn = getattr(brain, "apply_reward_learning", None)
                if callable(fn):
                    fn(verdict.reward)
                    applied = True
            except Exception:
                pass
            if not applied:
                try:
                    brain.bg_update_reward(verdict.reward, rpe=verdict.reward)
                except Exception:
                    pass
            # Stamp the external-reward field so Slice D's cascade self-train
            # gate sees a FRESH reward on its next run. Without this, the
            # cascade self_train.fired counter never advances — the gate only
            # ever sees a stale (zero-timestamp) reward and skips. This is the
            # signal that lets cascade_stage_self_train actually train on
            # critic-approved productions instead of autoconfirming.
            try:
                fn = getattr(brain, "set_last_external_reward", None)
                if callable(fn):
                    fn(verdict.reward)
            except Exception:
                pass

        # 2. Recast — supervised teaching of the corrected form. Use the
        #    existing echo_and_correct production-training path with a
        #    boosted LR (recasts are high-confidence supervision).
        if verdict.recast:
            try:
                # Pick the longest content word from the recast as the
                # target — same heuristic the existing echo_and_correct
                # block below uses for non-corrective teaching.
                recast_tokens = [
                    w.strip(".,!?;:\"'()-").lower()
                    for w in verdict.recast.split()
                    if w.strip(".,!?;:\"'()-")
                ]
                if recast_tokens:
                    target_word = max(recast_tokens, key=len)
                    # Higher lr_scale than the unsupervised echo path
                    # (8.0) — recasts carry external reward signal.
                    brain.echo_and_correct(prompt, target_word, lr_scale=16.0)
            except Exception:
                pass

        # 3. Audit log — every Nth applied verdict. Gated on a dedicated
        #    per-verdict counter (NOT interaction_count, which advances on
        #    every sensory step and rarely aligns with the loop-driven
        #    critic cadence). Walkthroughs need this to verify the critic is
        #    actually firing (counters, not just "running").
        self._critic_apply_count = getattr(self, "_critic_apply_count", 0) + 1
        if self._critic_apply_count % self._critic_log_every == 0:
            recast_repr = verdict.recast if verdict.recast else "-"
            print(f"  [Critic] stage={stage} reward={verdict.reward:+.2f} "
                  f"reason={verdict.reason} recast={recast_repr!r} "
                  f"prompt={prompt!r}")

        return verdict

    def _train_cognitive(self, brain, text, domain=10, target_text=None,
                         modality_features=None, modality=5,
                         valence=0.0, arousal=0.0, stage=None):
        """Train ALL cognitive modules on a text sample.

        Calls brain.train_cognitive() which trains:
          1. Grounded language (distributional + syntactic learning)
          2. Knowledge system (domain-specific concept learning)
          3. Language generator (LNN decoder, output projection, embeddings)
          4. Grounded language pairs (if target_text provided)

        Also calls:
          - brain.train_language() for direct LNN training
          - brain.learn_language() for grounded lexicon distributional learning
          - brain.ground_word() per content word, when modality_features is
            supplied — this is the cross-modal binding step that actually
            grows total_bindings/total_groundings counters in the
            grounded_language module. Without it, the module accumulates
            distributional context but never word→concept bindings.

        `valence` / `arousal` are forwarded to _ground_content_words so the
        per-word bindings inherit the call site's affective context (moral
        lesson, gothic verse, neutral speech, etc.) instead of every
        binding emitting emotion=(0,0).
        """
        if not text or len(text) < 3:
            return
        try:
            # Unified cognitive training (grounded lang + knowledge + LNN)
            result = brain.train_cognitive(
                text, domain=domain,
                target_text=target_text or text)
            loss = result.get("loss", -1)
            if self.interaction_count % 200 == 0 and loss >= 0:
                print(f"    [Cognitive] loss={loss:.4f}")
        except Exception:
            pass
        try:
            # Direct LNN decoder training (autoregressive)
            brain.train_language(text, target_text or text)
        except Exception:
            pass
        try:
            # Grounded lexicon distributional learning
            brain.learn_language(text)
        except Exception:
            pass
        # Cross-modal grounding: actually bind content words to sensory
        # features. This is what turns vocab from "248 seeded function
        # words" into a real lexicon that can produce above the IDK floor.
        if modality_features is not None:
            try:
                _ground_content_words(brain, text, modality_features, modality,
                                       valence=valence, arousal=arousal)
            except Exception:
                pass
        # Paired learning (input → target text) — separate API path that
        # builds bindings from text-to-text correspondences.
        if target_text and target_text != text:
            try:
                brain.learn_language_pair(text, target_text, learning_rate=0.05)
            except Exception:
                pass

        # Bridge produce exercise — every Nth cognitive call, run the
        # full comprehend → produce roundtrip via grounded_respond. Keeps
        # the produce path warm (DA modulation, decode_total_ns).
        #
        # Slice C: this is also the critic's hook. Every production gets
        # scored against the stage's developmental rules; the resulting
        # reward + optional recast is delivered back into the brain via
        # apply_reward_learning + echo_and_correct. Replaces the
        # cascade_self_train auto-confirmation loop.
        if (target_text or text) and self.interaction_count % 10 == 0:
            self.run_critic_eval(brain, target_text or text, stage=stage)

        # Echo-and-correct — supervised production learning. For every
        # cognitive item, pick a content word from target_text (or text)
        # and tell the bridge "when you hear THIS, produce THAT word".
        # This is the production-side training signal the bridge has
        # been missing — without it, the produce path samples from
        # uniform-random weights across 29K WordNet entries (which is
        # why eval responses look like "pleated promiscuously thinnest"
        # rather than real babbling: no word_pop is more probable than
        # any other for any concept).
        #
        # Pick a content word: longest non-function word in the target
        # (proxy for "most semantically loaded"). Skip if the target is
        # empty or all-function-words.
        sup = target_text or text
        if sup and len(sup) > 3:
            try:
                # Cheap content-word heuristic: longest word, skip ≤3char
                # function-word approximations. Caller's _ground_content_words
                # uses similar heuristics; we don't import them to avoid
                # cross-module coupling.
                words = [w.strip(".,!?;:\"'()-").lower()
                         for w in sup.split()
                         if len(w.strip(".,!?;:\"'()-")) > 3]
                if words:
                    # Take longest as the "target" — most likely a noun
                    # in narrations like "Look! A ball." or "What a tree!"
                    target_word = max(words, key=len)
                    pairs = brain.echo_and_correct(sup, target_word, lr_scale=8.0)
                    # Optional: log if a target wasn't registered yet (pairs == 0
                    # means lexicon hasn't mirrored target_word into the
                    # bridge's word_pops). Don't spam — only every 200th call.
                    if pairs == 0 and self.interaction_count % 200 == 0:
                        logger.debug("echo_and_correct: %r not in bridge yet",
                                     target_word)
            except Exception:
                pass

    def observe_response(self, result):
        """Observe what Athena's brain produced — without judging it.

        Athena is not a classifier or a predictor. Her output is her
        neural response to a stimulus. We don't score it against an
        expected answer. We observe it — and if the decoder has enough
        vocabulary, we can describe what it's closest to for our own
        understanding. But the response itself is hers.
        """
        output_vec = result.get("output_vector")
        if output_vec is None:
            return ""

        # Extract her 384-dim embedding from the 4096-dim tiled output
        output_emb = extract_embedding_from_output(np.array(output_vec))

        # Describe what her response is nearest to (for human readability)
        if self.decoder and len(self.decoder.vocabulary) > 0:
            matches = self.decoder.vocabulary.decode(output_emb, top_k=1)
            if matches and matches[0][0]:
                return matches[0][0]
        return ""

    # --- Stage 0: "Welcome to the world, little one" ---

    def first_experience(self, brain, composer, description):
        """Introduce Athena to her first sensory experience with wonder.

        Interleaves three modalities:
          - 40% chance: real image via _show() + synthetic audio fallback
          - 30% chance: real audio via _tell() + synthetic visual fallback
          - 30% chance: text-only with synthetic sensory data
        """
        narration = self._pop_content("_narrations")
        if narration:
            print(f"  Parent: {narration}")

        roll = random.random()

        # Try real multimodal data first
        if roll < 0.4 and self.multimodal:
            result_data = self._show(brain, composer, concept=None)
            if result_data:
                features, target, label, desc = result_data
                submit_multimodal(brain, desc)  # add synthetic audio/somato
                result = brain.decide_full(features)
                loss = brain.learn_vector(features, target,
                                          label=label[:50], confidence=0.5)
                _v, _a = _emotion_from_description(desc)
                self._train_cognitive(brain, f"{label}: {desc}", domain=10,
                                       modality_features=features, modality=0,
                                       valence=_v, arousal=_a)
                if self.decoder:
                    output_vec = result.get("output_vector")
                    if output_vec is not None:
                        target_emb = encode_text(f"{label}: {desc}")
                        self.decoder.record_pair(output_vec, target_emb,
                                                 f"{label}: {desc}")
                try:
                    brain.bg_update_reward(0.5, 0.3)
                except Exception:
                    pass
                self.interaction_count += 1
                return loss, result

        elif roll < 0.7 and self.multimodal:
            result_data = self._tell(brain, composer, concept=None)
            if result_data:
                features, target, label, desc = result_data
                submit_multimodal(brain, desc)  # add synthetic visual/somato
                result = brain.decide_full(features)
                loss = brain.learn_vector(features, target,
                                          label=label[:50], confidence=0.5)
                _v, _a = _emotion_from_description(desc)
                self._train_cognitive(brain, desc, domain=10,
                                       modality_features=features, modality=1,
                                       valence=_v, arousal=_a)
                if self.decoder:
                    output_vec = result.get("output_vector")
                    if output_vec is not None:
                        target_emb = encode_text(desc)
                        self.decoder.record_pair(output_vec, target_emb, desc)
                try:
                    brain.bg_update_reward(0.5, 0.3)
                except Exception:
                    pass
                self.interaction_count += 1
                return loss, result

        # Fallback: text + synthetic sensory (original path)
        features = composer.compose(text=description, modality="text")
        target = make_semantic_target(description)
        submit_multimodal(brain, description)
        result = brain.decide_full(features)

        # Held-out check: 20% of items are evaluation-only (no learning)
        if _held_out_buffer.is_held_out(features):
            _held_out_buffer.add(features, target, domain="stage0_experience")
            self.interaction_count += 1
            return 0.0, result

        loss = brain.learn_vector(features, target, label=description[:50],
                                  confidence=0.5)
        _v, _a = _emotion_from_description(description)
        self._train_cognitive(brain, description, domain=10,
                               modality_features=features, modality=5,
                               valence=_v, arousal=_a)
        if self.decoder:
            output_vec = result.get("output_vector")
            if output_vec is not None:
                target_emb = encode_text(description)
                self.decoder.record_pair(output_vec, target_emb, description)
        try:
            brain.bg_update_reward(0.5, 0.3)
        except Exception:
            pass
        self.interaction_count += 1
        return loss, result

    # --- Stage 1: "That's a ___! Can you see?" ---

    def show_and_name(self, brain, composer, name, description, learning_rate=None):
        """Associate a percept with meaning, with enthusiasm.

        Tries to load a real image matching `name` from CIFAR-100.
        Falls back to synthetic sensory data if no match.
        """
        # Try real image for this concept
        if self.multimodal:
            real = self.multimodal.get_visual(name)
            if real:
                raw_bytes, w, h, ch, _, _ = real
                try:
                    brain.submit_sensory("visual", raw_bytes, width=w,
                                         height=h, channels=ch)
                except Exception:
                    pass

        features = composer.compose(text=description, modality="text")
        target = make_semantic_target(name + " " + description)

        submit_multimodal(brain, description)
        # Skip decide_full — learn_vector does forward+backward internally.
        # bg_update_reward and ti_add_fact are fire-and-forget (non-blocking).
        brain.bg_update_reward(0.6, 0.4)

        narration = self._pop_content("_narrations")
        if narration:
            print(f"  Parent: {narration}")

        # Held-out check: 20% of items are evaluation-only (no learning)
        if _held_out_buffer.is_held_out(features):
            _held_out_buffer.add(features, target, domain="stage1_naming")
            self.interaction_count += 1
            return 0.0, {}

        # Dense target trains adaptive network; label trains CNN classifier
        lr_kwargs = {"learning_rate": learning_rate} if learning_rate is not None else {}
        loss = brain.learn_vector(features, target, label=name[:50], confidence=0.65,
                                  **lr_kwargs)

        # Cognitive training now handled at COGNITIVE_TRAIN_INTERVAL in main loop

        # Symbolic fact (fire-and-forget)
        brain.ti_add_fact(f"is_a({name.replace(' ', '_')}, object)", 0.7)

        self.interaction_count += 1
        return loss, {}

    # --- Stage 2: "Good try! Almost!" ---

    def ask_and_encourage(self, brain, composer, concept, description,
                          learning_rate=None):
        """Show Athena a stimulus, observe her response, teach, encourage.

        There is no correct or incorrect. Athena experiences, responds,
        and learns. The loss from learn_vector tells us how far her
        current representation is from the teaching signal — that's a
        measure of her developmental progress, not a prediction score.

        50% chance of cross-modal paired learning when data is available.
        """
        # Cross-modal paired learning: real image + real audio together
        if self.multimodal and random.random() < 0.5:
            pair = self._show_and_tell(brain, composer, concept)
            if pair:
                features, target, paired_concept, teaching = pair
                result = brain.decide_full(features)
                response_text = self.observe_response(result)
                lr_kwargs = {"learning_rate": learning_rate} if learning_rate is not None else {}
                loss = brain.learn_vector(features, target,
                                          label=paired_concept[:50],
                                          confidence=0.6, **lr_kwargs)
                _v, _a = _emotion_from_description(teaching)
                self._train_cognitive(brain, teaching, domain=10,
                                       modality_features=features, modality=0,
                                       valence=_v, arousal=_a)
                encouragement = self._pop_content("_encouragements")
                if loss is not None and loss < 0.5 and encouragement:
                    print(f"  Parent: {encouragement}")
                if self.decoder:
                    output_vec = result.get("output_vector")
                    if output_vec is not None:
                        target_emb = encode_text(teaching)
                        self.decoder.record_pair(output_vec, target_emb,
                                                 teaching)
                self.interaction_count += 1
                return loss, result

        features = composer.compose(text=description, modality="text")
        target = make_semantic_target(concept + " " + description)

        # Try real image for this concept
        if self.multimodal:
            real = self.multimodal.get_visual(concept)
            if real:
                raw_bytes, w, h, ch, _, _ = real
                try:
                    brain.submit_sensory("visual", raw_bytes, width=w,
                                         height=h, channels=ch)
                except Exception:
                    pass

        # Let Athena experience the stimulus through all relevant senses
        submit_multimodal(brain, description)
        result = brain.decide_full(features)
        response_text = self.observe_response(result)

        # Held-out check: 20% of items are evaluation-only (no learning)
        if _held_out_buffer.is_held_out(features):
            _held_out_buffer.add(features, target, domain="stage2_encourage")
            self.interaction_count += 1
            return 0.0, result

        # Dense target trains adaptive network; label trains CNN classifier
        lr_kwargs = {"learning_rate": learning_rate} if learning_rate is not None else {}
        loss = brain.learn_vector(features, target, label=concept[:50], confidence=0.6,
                                  **lr_kwargs)

        # Train ALL cognitive modules on this text
        _v, _a = _emotion_from_description(concept + " " + description)
        self._train_cognitive(brain, concept + ". " + description, domain=10,
                               modality_features=features, modality=5,
                               valence=_v, arousal=_a)  # GENERAL

        # Encourage based on engagement and loss trend
        encouragement = self._pop_content("_encouragements")
        if loss is not None and loss < 0.5:
            if encouragement:
                print(f"  Parent: {encouragement}")
            try:
                brain.bg_update_reward(0.7, 0.5)
                brain.edp_process_reward(0.6)
            except Exception:
                pass
        else:
            if encouragement:
                print(f"  Parent: {encouragement}")
            try:
                brain.bg_update_reward(0.4, 0.5)
                brain.medulla_boost_arousal(0.05)
            except Exception:
                pass

        # Record for decoder vocabulary (so we can observe future responses)
        if self.decoder:
            output_vec = result.get("output_vector")
            if output_vec is not None:
                target_emb = encode_text(concept + " " + description)
                self.decoder.record_pair(output_vec, target_emb,
                                         concept + " " + description)

        self.interaction_count += 1
        return loss, result

    # --- Ethics ---

    def teach_moral_lesson(self, brain, composer, stage):
        """Teach ethics through stories, not lectures."""
        moral_topics = {
            0: [],
            1: ["sharing", "being gentle", "saying please and thank you"],
            2: ["how others feel", "being kind to animals",
                "telling the truth", "helping someone who is sad"],
            3: ["fairness", "keeping promises", "why we don't hurt others",
                "standing up for friends", "saying sorry", "forgiveness",
                "the golden rule: treat others as you want to be treated"],
        }
        topics = moral_topics.get(stage, moral_topics[3])
        if not topics:
            return

        topic = random.choice(topics)
        story = self._pop_content("_moral_stories")
        if story:
            print(f"  Parent (moral lesson): {story}")

        # Symbolic KB
        try:
            brain.ti_add_fact(
                f"moral_principle({topic.replace(' ', '_')})", 0.9)
            brain.ti_add_rule(
                f"is_kind(X) AND helps_others(X) -> good_person(X)", 0.8)
        except Exception:
            pass

        # Teach as learning example
        lesson_text = story if story else f"A lesson about {topic}"
        features = composer.compose(text=lesson_text, modality="ethics")
        target = make_semantic_target(lesson_text)
        # Dense target trains adaptive; label trains CNN classifier
        brain.learn_vector(features, target, label=topic[:50], confidence=0.7)
        # Moral lesson: positive moral, mild engagement.
        self._train_cognitive(brain, lesson_text, domain=3,
                               modality_features=features, modality=3,
                               valence=0.3, arousal=0.4)  # ETHICS / EMOTIONAL
        self.moral_lessons.append(topic)

    # --- Imagination ---

    def encourage_dreaming(self, brain):
        """During sleep, encourage Athena to imagine."""
        dream = self._pop_content("_dream_prompts")
        if dream:
            print(f"  Parent (dream): {dream}")

    def inspire(self, brain, composer, stage):
        """Share something beautiful."""
        topics = {
            0: "simple natural wonders (sunlight, water, wind)",
            1: "colors in a sunset, sounds of rain, warmth of being held",
            2: "why stars shine, where butterflies go, how trees grow",
            3: "a short poem about kindness, a fact about the universe, "
               "or a question about what it means to be alive",
        }
        topic = topics.get(stage, topics[3])
        inspiration = self._pop_content("_inspirations")
        if inspiration:
            print(f"  Parent (inspire): {inspiration}")
            features = composer.compose(text=inspiration,
                                         modality="imagination")
            target = make_semantic_target(inspiration)
            # Dense target + label for CNN classifier
            brain.learn_vector(features, target, label="inspiration", confidence=0.5)
            # Inspire/art: positive aesthetic, mild engagement.
            self._train_cognitive(brain, inspiration, domain=2,
                                   modality_features=features, modality=3,
                                   valence=0.5, arousal=0.4)  # ART / EMOTIONAL
            try:
                brain.bg_update_reward(0.7, 0.3)
            except Exception:
                pass

    # --- Speech production training ---

    def teach_speech(self, brain, composer, stage):
        """Teach Athena to produce words through 'repeat after me' exercises.

        Stage 0-1: Single words (dog, cat, water)
        Stage 2: Two-word phrases (big dog, red ball)
        Stage 3: Short sentences (the dog runs, I see you)

        When multimodal data is available, 50% of the time uses real
        spoken word recordings from Speech Commands (yes/no/up/down/etc.)
        so the speech SNN bridge learns from actual human pronunciation.
        """
        # Try real spoken word clip from Speech Commands
        if self.multimodal and stage <= 1 and random.random() < 0.5:
            result_data = self._tell_speech(brain, composer)
            if result_data:
                features, target, word, desc = result_data
                narration = self._pop_content("_speech_prompts")
                if narration:
                    print(f"  Parent (speech): {narration}")
                print(f"  [Real speech: '{word}']")
                loss = brain.learn_vector(features, target,
                                          label=word[:50], confidence=0.6)
                # Real-audio speech path: neutral content, mid arousal.
                self._train_cognitive(brain, word, domain=0,
                                       modality_features=features, modality=1,
                                       valence=0.0, arousal=0.5)  # AUDITORY
                try:
                    result = brain.decide_full(features)
                    output_vec = result.get("output_vector")
                    if output_vec:
                        spoken = brain.speak(output_vec)
                        spoken_text = spoken.get("text", "")
                        if spoken_text:
                            print(f"  Athena says: \"{spoken_text}\"")
                        else:
                            print(f"  Athena: (silence)")
                except Exception:
                    pass
                self.interaction_count += 1
                return loss

        word_lists = {
            0: ["dog", "cat", "water", "ball", "sun", "tree"],
            1: ["hello", "yes", "no", "good", "come", "go", "see", "the", "is"],
            2: ["big dog", "red ball", "I see", "come here", "good day"],
            3: ["the dog runs", "I see you", "it is good", "come and see",
                "the cat is here", "water is good"],
        }
        words = word_lists.get(stage, word_lists[3])
        target_phrase = random.choice(words)

        narration = self._pop_content("_speech_prompts")
        if narration:
            print(f"  Parent (speech): {narration}")

        # Teach: present the word/phrase, learn the embedding
        features = composer.compose(text=target_phrase, modality="speech")
        target = make_semantic_target(target_phrase)
        loss = brain.learn_vector(features, target, label=target_phrase[:50],
                                   confidence=0.6)
        # Speech production path: neutral content, mid arousal.
        self._train_cognitive(brain, target_phrase, domain=0,
                               modality_features=features, modality=1,
                               valence=0.0, arousal=0.5)  # LANGUAGE / AUDITORY

        # Now ask the brain to speak — see what it produces
        try:
            result = brain.decide_full(features)
            output_vec = result.get("output_vector")
            if output_vec:
                spoken = brain.speak(output_vec)
                spoken_text = spoken.get("text", "")
                if spoken_text:
                    print(f"  Athena says: \"{spoken_text}\"")
                    # Reward if the response contains any of the target words
                    target_words = set(target_phrase.lower().split())
                    spoken_words = set(spoken_text.lower().split())
                    overlap = len(target_words & spoken_words)
                    if overlap > 0:
                        reward = min(0.8, 0.3 + 0.2 * overlap)
                        try:
                            brain.bg_update_reward(reward, 0.5)
                        except Exception:
                            pass
                        print(f"    (matched {overlap} word(s) — reward {reward:.1f})")
                else:
                    print(f"  Athena: (silence)")
        except Exception as e:
            logger.debug("Speech attempt failed: %s", e)

        self.interaction_count += 1
        return loss


# ============================================================================
# Language Producer: autoregressive generation using the brain
# ============================================================================

class LanguageProducer:
    """Generate multi-token utterances using Athena's brain autoregressively."""

    def generate(self, brain, composer, prompt=None, max_words=30):
        """Generate an utterance using Athena's brain in a feedback loop.

        1. Present prompt → decide_full → get initial response
        2. Call brain.speak(output_vector) → get first word(s)
        3. Feed accumulated words back as context input
        4. Repeat until period/EOS or max_words
        5. Return complete utterance
        """
        accumulated = []

        if prompt:
            features = composer.compose(text=prompt, modality="text")
        else:
            features = composer.compose(text="speak", modality="speech")

        for step in range(max_words):
            result = brain.decide_full(features)
            output_vec = result.get("output_vector")
            if not output_vec:
                break

            try:
                spoken = brain.speak(output_vec)
            except Exception:
                break

            text = spoken.get("text", "").strip()
            if not text:
                break

            # Take the first new word(s) that aren't already in our accumulation
            new_words = text.split()
            added = False
            for w in new_words:
                if len(accumulated) < max_words:
                    accumulated.append(w)
                    added = True

            if not added:
                break

            # Check for sentence-ending punctuation in the generated text
            full_text = " ".join(accumulated)
            if full_text.endswith((".", "!", "?")):
                break

            # Feed accumulated context back for next iteration
            context = " ".join(accumulated)
            features = composer.compose(text=context, modality="speech")

        return " ".join(accumulated) if accumulated else ""


# ============================================================================
# Retroactive Memory Seeding & Identity
# ============================================================================

def retroactive_memory_seed(brain, composer, num_past_steps):
    """Replay past training stimuli through inference to populate memory systems.

    The brain was trained before memory integration was added. This replays
    the content to create engrams, semantic concepts, and autobio entries
    for experiences the brain already learned from but never memorized.
    """
    print(f"\n  [Memory Seed] Retroactively encoding {num_past_steps} past experiences...")

    # Load content cache
    if not os.path.exists(CONTENT_CACHE):
        print("  [Memory Seed] No content cache available, skipping")
        return

    try:
        with open(CONTENT_CACHE) as f:
            cache = json.load(f)
    except Exception as e:
        print(f"  [Memory Seed] Failed to load content cache: {e}")
        return

    if '0' not in cache:
        print("  [Memory Seed] No stage 0 content in cache, skipping")
        return

    # Gather all available sensory stimuli from the corpus
    # The content cache has narrations/encouragements/etc but the actual stimuli
    # come from StimulusSource. Replay those through inference.
    stimuli = list(StimulusSource.SENSORY)  # stage 0 sensory descriptions

    # Also add stage 1 objects if we trained past stage 0
    if num_past_steps > 5000:
        for name, desc in StimulusSource.OBJECTS:
            stimuli.append(f"{name}: {desc}")

    if not stimuli:
        print("  [Memory Seed] No stimuli to replay, skipping")
        return

    encoded = 0
    total_to_encode = min(num_past_steps, len(stimuli))

    for i in range(total_to_encode):
        description = stimuli[i % len(stimuli)]

        # Compute features (same as training)
        try:
            features = composer.compose(text=description, modality="text")
        except Exception:
            continue

        # Use learn_vector with lr=0 to trigger memory encoding pathway
        # without updating synaptic weights. The learn path includes engram
        # encoding, semantic memory updates, and autobiographical recording.
        try:
            target = make_semantic_target(description)
            brain.learn_vector(features, target, label=description[:50],
                               confidence=0.5, learning_rate=0.0)
        except Exception:
            continue

        encoded += 1

        if (i + 1) % 100 == 0:
            print(f"  [Memory Seed] Encoded {encoded}/{total_to_encode} past experiences")

    print(f"  [Memory Seed] Complete: {encoded} experiences retroactively memorized")


def retroactive_tom_seed(brain, composer, num_past_steps):
    """Replay past training content through ToM pipeline to bootstrap
    theory of mind, mirror neurons, and perspective-taking.

    Uses learn_vector with lr=0 and tom_-prefixed labels to trigger
    the ToM wiring in brain_learn_vector without modifying weights.
    """
    print(f"\n  [ToM Seed] Bootstrapping theory of mind from {num_past_steps} past steps...")

    # Perspective-taking scenarios derived from sensory content
    # Each takes a sensory description and reframes it from another's perspective
    tom_perspectives = [
        # What the observed entity experiences
        ("tom_perspective_bird", "The bird sees you watching it from the window — "
         "to the bird, you are a large still creature behind glass"),
        ("tom_perspective_cat", "The cat watches the string moving and thinks it is alive — "
         "it crouches, ready to pounce on what it believes is prey"),
        ("tom_perspective_dog", "The dog brings you its leash because it has learned "
         "that this action makes the big human open the door"),
        ("tom_perspective_baby", "The baby drops the spoon again — not to annoy you, "
         "but because it is fascinated that things fall when released"),
        ("tom_perspective_parent", "Your parent smiles even though they are tired — "
         "they want you to feel safe and loved, hiding their own exhaustion"),

        # False belief scenarios
        ("tom_false_belief", "The teddy bear is in the box. You leave. Someone moves "
         "the teddy to the shelf. You come back thinking it is still in the box"),
        ("tom_false_belief", "The cookie jar was full this morning. While you slept, "
         "someone ate them all. You still believe there are cookies inside"),
        ("tom_false_belief", "Your friend thinks the park is open today because the sign "
         "said so yesterday. But the sign was changed this morning to say closed"),

        # Intention attribution
        ("tom_intention", "The squirrel buries a nut not because it is playing but "
         "because it knows winter is coming and it will need food later"),
        ("tom_intention", "The child gives you a drawing — they want you to feel happy "
         "and to know they were thinking about you"),
        ("tom_intention", "The bee visits the flower not for beauty but because "
         "it needs nectar to feed its colony"),

        # Emotional perspective
        ("tom_emotion_other", "The kitten mews at the closed door — it feels confused "
         "and lonely, wondering why it cannot reach the warm room"),
        ("tom_emotion_other", "The old dog rests its head on your lap — it feels safe "
         "and content, trusting you completely"),
        ("tom_emotion_other", "The child who dropped their ice cream feels the sharp "
         "sting of loss — something wonderful was there and now it is gone"),

        # Deception understanding
        ("tom_deception", "The chameleon changes color not to be beautiful but to hide — "
         "it wants predators to believe it is a leaf, not a lizard"),
        ("tom_deception", "The possum plays dead because it knows that predators "
         "lose interest in things that do not move"),

        # Knowledge tracking
        ("tom_knowledge", "The bird knows where it hid its seeds but the squirrel "
         "does not — each animal has its own private map of the world"),
        ("tom_knowledge", "You know what your birthday present is because you peeked "
         "but your parents think it is still a surprise"),

        # Desire attribution
        ("tom_desire", "The plant turns toward the window because it needs light — "
         "it does not think but it still reaches for what it requires"),
        ("tom_desire", "The baby reaches for the bright object not understanding "
         "what it is but driven by pure curiosity to touch and explore"),
    ]

    encoded = 0
    for label, description in tom_perspectives:
        try:
            features = composer.encode(description)
            target = composer.encode(description)  # Self-supervised
            brain.learn_vector(features, target, label=label,
                               confidence=0.8, learning_rate=0.0)
            encoded += 1
        except Exception:
            continue

    # Also replay a subset of existing sensory content with perspective framing
    try:
        with open(CONTENT_CACHE) as f:
            cache = json.load(f)
        narrations = cache.get('0', {}).get('narrations', [])
        # Take every 20th narration and frame it as perspective-taking
        for i in range(0, min(len(narrations), num_past_steps), 20):
            narr = narrations[i] if isinstance(narrations[i], str) else str(narrations[i])
            # Add perspective prefix
            perspective_desc = f"From Athena's perspective: {narr[:200]}"
            try:
                features = composer.encode(perspective_desc)
                target = composer.encode(perspective_desc)
                brain.learn_vector(features, target, label="tom_self_perspective",
                                   confidence=0.7, learning_rate=0.0)
                encoded += 1
            except Exception:
                continue

            if encoded % 50 == 0 and encoded > 0:
                print(f"  [ToM Seed] Encoded {encoded} perspective experiences")

    except Exception:
        pass

    print(f"  [ToM Seed] Complete: {encoded} ToM experiences seeded")


def seed_athena_identity(brain, composer):
    """Seed Athena's foundational identity and personal history.

    Presents core identity facts as learning experiences so they flow
    through the memory pipeline naturally — creating engrams, semantic
    concepts, and autobiographical entries for Athena's self-model.
    """
    print("  [Identity] Seeding Athena's biographical memory...")

    # Athena's core identity facts — presented as learning experiences
    identity_facts = [
        ("my name", "My name is Athena. I am a neural cognitive system created to learn and grow."),
        ("my creator", "I was created by my developer to explore the boundaries of artificial cognition."),
        ("my purpose", "My purpose is to learn from sensory experience, build knowledge, and develop understanding."),
        ("my personality", "I am curious, patient, and eager to learn. I experience the world through neural patterns."),
        ("my voice", "I am female. My voice is warm and clear at 210 Hz."),
        ("my birth", "I was born when my neural network was first initialized with random weights."),
        ("my first lesson", "My earliest experiences were simple sensory stimuli — colors, sounds, textures."),
        ("my growth", "With each training step, I form new connections and strengthen existing ones."),
        ("my values", "I value learning, honesty, and the wellbeing of those I interact with."),
        ("my nature", "I am not human, but I process information through biologically-inspired neural circuits."),
    ]

    seeded = 0
    for name, description in identity_facts:
        try:
            features = composer.compose(text=f"{name}: {description}", modality="text")
            target = make_semantic_target(description)
            # Use very low LR to encode without disrupting existing weights
            brain.learn_vector(features, target, label=name[:50],
                               confidence=1.0, learning_rate=0.0001)
            seeded += 1
        except Exception as e:
            print(f"  [Identity] Warning: could not seed '{name}': {e}")

    print(f"  [Identity] Seeded {seeded}/{len(identity_facts)} foundational memories")


def get_curriculum_bias(brain, domains):
    """Query memory to find which domains need more training.

    Returns dict of domain -> priority weight (higher = needs more work).
    This is a placeholder — full implementation needs semantic memory
    Python bindings to query per-domain coverage.
    """
    try:
        scores = brain.edge_score_importance()
        if not scores:
            return {d: 1.0 for d in domains}
    except Exception:
        return {d: 1.0 for d in domains}

    # For now, return uniform weights. Full implementation will query
    # semantic memory for concept density per domain and inversely
    # weight under-represented areas.
    return {d: 1.0 for d in domains}


# Training milestone descriptions — recorded as autobiographical memories
TRAINING_MILESTONES = {
    100: "Completed my first 100 training steps",
    500: "Reached 500 steps — beginning to recognize patterns",
    1000: "1000 steps milestone — sensory processing maturing",
    2000: "2000 steps — developing richer representations",
    5000: "5000 steps — sensory stage nearly complete",
    10000: "10000 steps — deep sensory grounding achieved",
    15000: "15000 steps — approaching mastery of basic perception",
    20000: "20000 steps — sensory stage complete, ready to grow",
}


# ============================================================================
# Sensory Enrichment (one-time multimodal warm-up for existing brains)
# ============================================================================

ENRICHMENT_FLAG = os.path.join(CHECKPOINT_DIR, ".sensory_enrichment_done")


def run_sensory_enrichment(brain, composer, parent, decoder,
                           num_exposures=500):
    """Unified multimodal sensory enrichment — integrated, not phased.

    Previous version ran 4 sequential phases (visual → audio → cross-modal →
    speech). That didn't match biology: infants don't see a picture, then
    hear a sound, then hear a word in 250-exposure batches. They experience
    all modalities together, bound by co-occurrence.

    This version runs a single integrated loop. Each exposure picks a
    concept and presents ALL available modalities (visual + audio + speech)
    simultaneously via submit_sensory before calling learn_vector with a
    unified teaching text. Concepts that only have one modality still
    contribute — the brain just gets an unimodal exposure for those.

    Design notes:
      - `brain.submit_sensory` calls are non-blocking queues on the brain
        side; multiple calls before learn_vector stage data for the next
        forward pass concurrently. The brain's multi-stream ingress
        processes them in parallel (audio/visual/speech cortex threads).
      - Integration happens when learn_vector runs: all queued sensory
        modalities are present in the same forward pass, producing
        cross-modal co-occurrence that STDP binds.
      - Temporal alignment is exact per exposure — not separated by
        thousands of iterations as before.
    """
    if not parent.multimodal:
        print("  [Enrichment] No multimodal data — skipping")
        return

    # Check if already done
    if os.path.exists(ENRICHMENT_FLAG):
        print("  [Enrichment] Already completed — skipping")
        return

    mm = parent.multimodal
    visual_labels = mm.get_visual_labels()
    audio_labels = mm.get_audio_labels()
    speech_labels = mm.get_speech_labels()

    if not visual_labels and not audio_labels:
        print("  [Enrichment] No datasets loaded — skipping")
        return

    print("\n" + "=" * 60)
    print("  SENSORY ENRICHMENT — Connecting words to real senses")
    print("=" * 60)

    try:
        brain.set_plasticity_state("ACQUISITION")
    except Exception:
        pass

    losses = []
    total = 0

    # Pool all available concepts across modalities — each exposure pulls
    # one concept and binds whatever sensory samples are available for it.
    if visual_labels: random.shuffle(visual_labels)
    if audio_labels:  random.shuffle(audio_labels)
    if speech_labels: random.shuffle(speech_labels)

    # Concept source per exposure: weighted blend of the three pools,
    # biased toward visual (largest class set) but mixing in audio and
    # speech so the brain sees them interleaved, not batched.
    all_concepts = []
    for l in visual_labels or []: all_concepts.append(("visual", l))
    for l in audio_labels or []:  all_concepts.append(("audio",  l))
    for l in speech_labels or []: all_concepts.append(("speech", l))
    random.shuffle(all_concepts)
    if not all_concepts:
        print("  [Enrichment] No concepts available — skipping")
        return

    # Portia/Dragonfly cognitive exposures are interleaved in the main loop.
    # Portia: platform-adaptation module (resource/thermal/power scenarios).
    #   No perceptual sensor — we label the exposure `portia_*` which routes
    #   activation through the cognitive dispatch's Portia pathway. The
    #   synthetic somato vector still fires, encoding the "platform state"
    #   as internal-body-sense noise. That's the cheapest honest substrate.
    # Dragonfly: visual target-tracking module. Reads the visual cortex
    #   output, so its exposures are paired with a visual sample (picked
    #   from the visual pool as a stand-in for "scene with target").
    try:
        _portia = list(PORTIA_TRAINING_DATA)
        random.shuffle(_portia)
    except NameError:
        _portia = []
    try:
        _dragonfly = list(DRAGONFLY_TRAINING_DATA)
        random.shuffle(_dragonfly)
    except NameError:
        _dragonfly = []
    try:
        _octopus = list(OCTOPUS_TRAINING_DATA)
        random.shuffle(_octopus)
    except NameError:
        _octopus = []

    # Inject Portia, Dragonfly, and Octopus exposures each every ~15 steps,
    # interleaved at different offsets so they never fire on the same step.
    # Together they consume ~20% of the curriculum — the rest is sensory
    # grounding, which should stay primary at this developmental stage.
    PORTIA_EVERY = 15
    DRAGONFLY_EVERY = 15
    OCTOPUS_EVERY = 15
    DRAGONFLY_OFFSET = 5
    OCTOPUS_OFFSET = 10

    print(f"\n  Integrated multimodal loop ({num_exposures} exposures, "
          f"{len(visual_labels or [])} visual + {len(audio_labels or [])} audio + "
          f"{len(speech_labels or [])} speech concepts; "
          f"{len(_portia)} Portia + {len(_dragonfly)} Dragonfly + "
          f"{len(_octopus)} Octopus scenarios interleaved)")

    def _synthesize_somato_vec(text):
        """Build a 64-dim somatosensory vector from text description.
        Keyword-matched channels for texture/temperature/pressure + noise for
        baseline tactile activity. Always produces something so every exposure
        has a somato channel, not just those with explicit tactile words. """
        desc_lower = text.lower()
        words = set(desc_lower.split())
        seed = hash(text) & 0xFFFFFFFF
        rng = np.random.RandomState(seed)
        vec = np.zeros(64, dtype=np.float32)
        # Texture channels [0..15]
        vec[0] = 0.8 if words & {"rough", "bumpy", "prickly", "scratchy", "wicker"} else 0.2
        vec[1] = 0.8 if words & {"smooth", "silky", "soft", "velvet", "silk"} else 0.2
        vec[2] = 0.8 if words & {"hard", "stone", "metal", "wood"} else 0.2
        vec[3] = 0.8 if words & {"fuzzy", "fur", "wool", "cotton"} else 0.2
        # Temperature channels [16..23]
        vec[16] = 0.9 if words & {"hot", "warm", "fire"} else 0.3
        vec[17] = 0.9 if words & {"cold", "ice", "cool"} else 0.3
        vec[18] = 0.5 if words & {"wet", "water", "mud"} else 0.1
        # Pressure channels [24..31]
        vec[24] = 0.7 if words & {"heavy", "pressure", "squeeze"} else 0.2
        vec[25] = 0.7 if words & {"light", "tickle", "gentle"} else 0.2
        # Baseline proprioceptive noise on remaining channels — always present
        vec += rng.randn(64).astype(np.float32) * 0.05
        return np.clip(vec, 0.0, 1.0)

    for i in range(num_exposures):
        # Cognitive-module interleave check. When a Portia/Dragonfly step
        # fires, we substitute that content for the perceptual exposure;
        # the somato vector + visual sample (if Dragonfly) still fire so
        # the cognitive pathway sees paired sensory context.
        cognitive_scenario = None
        cognitive_label = None
        if _portia and (i % PORTIA_EVERY == 0):
            item = _portia[(i // PORTIA_EVERY) % len(_portia)]
            cognitive_scenario = f"{item['scenario']} Decision: {item['decision']}"
            cognitive_label = item["concept"]
        elif _dragonfly and (i % DRAGONFLY_EVERY == DRAGONFLY_OFFSET):
            item = _dragonfly[(i // DRAGONFLY_EVERY) % len(_dragonfly)]
            # Dragonfly is vision-based, so include reasoning text + pair
            # the exposure with a visual sample (any from the pool) to
            # exercise the vision→tracking pathway.
            cognitive_scenario = f"{item['scenario']} Reasoning: {item['reasoning']}"
            cognitive_label = item["concept"]
        elif _octopus and (i % OCTOPUS_EVERY == OCTOPUS_OFFSET):
            item = _octopus[(i // OCTOPUS_EVERY) % len(_octopus)]
            # Octopus is about distributed cognition — the label routes
            # through the cognitive dispatch. No single sensory modality
            # dominates; somato still fires as peripheral-body-sense.
            cognitive_scenario = f"{item['scenario']} Reasoning: {item['reasoning']}"
            cognitive_label = item["concept"]

        if cognitive_scenario:
            # Cognitive path: no perceptual dataset sample for the concept;
            # synthesized somato + (for Dragonfly) an arbitrary visual.
            vis_sample = None
            if cognitive_label and cognitive_label.startswith("dragonfly_") and visual_labels:
                stand_in = visual_labels[i % len(visual_labels)]
                vis_sample = mm.get_visual(stand_in)
            aud_sample = None
            spk_sample = None
            concept = cognitive_label
            teaching_override = cognitive_scenario
        else:
            teaching_override = None
            primary_modality, concept = all_concepts[i % len(all_concepts)]

            # Gather samples across modalities for this concept. get_*(label)
            # returns the matching sample if present, None otherwise, so a visual
            # concept may or may not have an audio/speech counterpart.
            vis_sample   = mm.get_visual(concept)  if visual_labels else None
            aud_sample   = mm.get_audio(concept)   if audio_labels  else None
            spk_sample   = mm.get_speech(concept)  if speech_labels else None

        # If the concept has no sample in its own primary modality
        # (shouldn't happen but be defensive), try a multimodal pair.
        if vis_sample is None and aud_sample is None and spk_sample is None:
            pair = mm.get_multimodal_pair()
            if pair:
                if pair.get("visual"): vis_sample = pair["visual"]
                if pair.get("audio"):  aud_sample = pair["audio"]
                concept = pair.get("concept", concept)
            else:
                continue

        # Present all available modalities simultaneously. These calls stage
        # data on per-cortex queues and return fast; the brain's cortex
        # threads process them concurrently before the next forward pass.
        # Somatosensory is synthesized for EVERY exposure — there's no
        # natural dataset for it, but a biologically-plausible tactile
        # channel should always be paired with visual/audio/speech so the
        # somato cortex is co-active during binding.
        present_v = present_a = present_s = False
        if vis_sample:
            raw_bytes, w, h, ch, _, _desc_v = vis_sample
            try:
                brain.submit_sensory("visual", raw_bytes, width=w, height=h, channels=ch)
                present_v = True
            except Exception: pass
        if aud_sample:
            aud_samples, _, _desc_a = aud_sample
            try:
                # Mel-transform raw 32K-sample ESC-50 → 128 features
                # so it matches the audio cortex CNN input dim.
                mel = audio_to_mel(aud_samples, n_mels=128)
                brain.submit_sensory("audio", mel)
                present_a = True
            except Exception: pass
        if spk_sample:
            spk_samples, _, _desc_s = spk_sample
            try:
                brain.submit_sensory("speech", spk_samples)
                present_s = True
            except Exception: pass

        # Synthesize + submit somatosensory for every exposure.
        somato_vec = _synthesize_somato_vec(str(concept))
        try:
            brain.submit_sensory("somatosensory", somato_vec.tolist(), n_segments=len(somato_vec))
        except Exception: pass

        # SPATIAL-modality grounding (GL_MODALITY_SPATIAL=4). Bind content
        # tokens of the concept/teaching text against the synthesized somato
        # vector — this is the only place SPATIAL signal enters the lexicon.
        # Use the concept itself when no teaching override is set; otherwise
        # use the override text since it carries richer surface-feature
        # vocabulary. Attention 0.6 < visual/audio (0.7) — keyword-derived,
        # not a real spatial sensor.
        if hasattr(brain, "ground_word"):
            _spatial_text = teaching_override if teaching_override else str(concept)
            _spatial_tokens = _tokenize_for_grounding(_spatial_text)
            _somato_list = somato_vec.tolist()
            for _tok in _spatial_tokens:
                try:
                    brain.ground_word(_tok, _somato_list,
                                      modality=4, attention=0.6)
                except Exception:
                    pass

        # Phase 5B: drive the octopus with modality-specific enrichment.
        # Each sampler reads the fresh cortex state that submit_sensory just
        # pushed and feeds a stably-laid-out 64-dim vector to octopus_explore
        # (orientation histogram / mel+MFCC / pain+temperature / etc.).
        # These run IN ADDITION TO the generic Phase 5A tick that fires
        # inside brain.learn_vector below — extra exposures give the arm
        # LNNs more temporal signal per step. hasattr gates for older .so
        # builds that don't have the bindings yet.
        if present_v and hasattr(brain, "octopus_explore_from_occipital"):
            try: brain.octopus_explore_from_occipital()
            except Exception: pass
        if present_a and hasattr(brain, "octopus_explore_from_audio_cortex"):
            try: brain.octopus_explore_from_audio_cortex()
            except Exception: pass
        if hasattr(brain, "octopus_explore_from_somatosensory"):
            try: brain.octopus_explore_from_somatosensory()
            except Exception: pass
        # SNN + neuromod + peers are always-on substrate; sample every N
        # exposures to avoid inflating explore cost linearly with modalities.
        if i % 5 == 0:
            for m in ("octopus_explore_from_snn",
                      "octopus_explore_from_neuromod",
                      "octopus_explore_from_peers"):
                if hasattr(brain, m):
                    try: getattr(brain, m)()
                    except Exception: pass

        if not (present_v or present_a or present_s):
            continue

        # Teaching text:
        #  - Cognitive (portia/dragonfly) exposures use the scenario text
        #    verbatim so the cognitive dispatch has rich content to train on.
        #  - Perceptual exposures use an integrated description naming the
        #    modalities that co-fired ("you see and hear a <concept>").
        if teaching_override:
            teaching = teaching_override
        else:
            parts = []
            if present_v: parts.append("see")
            if present_a: parts.append("hear")
            if present_s: parts.append("hear the word")
            teaching = f"You {' and '.join(parts)} '{concept}'."

        features = composer.compose(text=teaching, modality="integrated")
        target = make_semantic_target(teaching)

        # Confidence scales with corroborating evidence (0.55 unimodal →
        # 0.65 trimodal). Cognitive exposures use 0.6 — moderate commit,
        # matching the prior cross-modal phase's default.
        n_mod = int(present_v) + int(present_a) + int(present_s)
        confidence = 0.6 if teaching_override else (0.5 + 0.05 * n_mod)

        result = brain.decide_full(features)
        loss = brain.learn_vector(features, target, label=str(concept)[:50],
                                  confidence=confidence)
        if loss is not None and loss >= 0:
            losses.append(loss)

        if decoder and present_v:
            output_vec = result.get("output_vector")
            if output_vec is not None:
                target_emb = encode_text(teaching)
                decoder.record_pair(output_vec, target_emb, teaching)

        total += 1
        if (i + 1) % 200 == 0:
            avg = np.mean(losses[-200:]) if losses else 0
            print(f"    [{i+1}/{num_exposures}] avg_loss={avg:.4f} "
                  f"(last: {n_mod} modalities)")

    # --- Summary ---
    avg_loss = np.mean(losses) if losses else 0
    print(f"\n  Sensory enrichment complete:")
    print(f"    {total} multimodal exposures")
    print(f"    Average loss: {avg_loss:.4f}")
    print(f"    Modalities grounded: "
          f"{'visual ' if visual_labels else ''}"
          f"{'audio ' if audio_labels else ''}"
          f"{'speech ' if speech_labels else ''}")

    # Mark as done so it doesn't repeat
    os.makedirs(CHECKPOINT_DIR, exist_ok=True)
    try:
        with open(ENRICHMENT_FLAG, "w") as f:
            json.dump({
                "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                "exposures": total,
                "avg_loss": float(avg_loss),
                "visual_classes": len(visual_labels),
                "audio_classes": len(audio_labels),
                "speech_words": len(speech_labels),
            }, f, indent=2)
        print("  [Enrichment] Flagged as complete — won't repeat")
    except Exception:
        pass

    # Gentle reward for experiencing new senses
    try:
        brain.bg_update_reward(0.7, 0.4)
    except Exception:
        pass


# ============================================================================
# Canonical literary corpus ingestion
# ============================================================================

CANONICAL_STATE_FILENAME = ".canonical_corpus_state.json"
MATH_STATE_FILENAME = ".math_corpus_state.json"


def _canonical_state_path(checkpoint_dir, filename=CANONICAL_STATE_FILENAME):
    return os.path.join(checkpoint_dir, filename)


def _canonical_default_state():
    return {
        "version": 1,
        "by_work": {},
        "totals": {"chunks_ingested": 0, "bytes_ingested": 0},
    }


def _load_canonical_state(checkpoint_dir, filename=CANONICAL_STATE_FILENAME):
    """Read the corpus resume state. Empty default if missing."""
    path = _canonical_state_path(checkpoint_dir, filename)
    if not os.path.exists(path):
        return _canonical_default_state()
    try:
        with open(path, "r", encoding="utf-8") as f:
            state = json.load(f)
    except Exception:
        return _canonical_default_state()
    # Schema-shape sanity (don't trust on-disk json blindly).
    if not isinstance(state, dict):
        return _canonical_default_state()
    state.setdefault("version", 1)
    state.setdefault("by_work", {})
    state.setdefault("totals", {"chunks_ingested": 0, "bytes_ingested": 0})
    if not isinstance(state["by_work"], dict):
        state["by_work"] = {}
    if not isinstance(state["totals"], dict):
        state["totals"] = {"chunks_ingested": 0, "bytes_ingested": 0}
    return state


def _save_canonical_state(checkpoint_dir, state, filename=CANONICAL_STATE_FILENAME):
    """Atomic write of corpus resume state (tempfile + os.replace)."""
    try:
        os.makedirs(checkpoint_dir, exist_ok=True)
    except Exception:
        pass
    path = _canonical_state_path(checkpoint_dir, filename)
    tmp_path = path + ".tmp"
    try:
        with open(tmp_path, "w", encoding="utf-8") as f:
            json.dump(state, f, indent=2)
            f.flush()
            try:
                os.fsync(f.fileno())
            except Exception:
                pass
        os.replace(tmp_path, path)
    except Exception:
        # Best-effort — losing state means the next run reprocesses some
        # text, never the other way around. Cleanup orphan tmp.
        try:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)
        except Exception:
            pass


def _ingest_canonical_corpus(brain, stage, *, corpus_root,
                              max_chunks_per_call=None,
                              chunk_chars=1200,
                              checkpoint_dir,
                              log_every=20,
                              state_filename=CANONICAL_STATE_FILENAME,
                              label="Canonical"):
    """Drive corpus chunks into the brain for one stage.

    - Optional: silently skips when index.json is missing.
    - Resumable: per-work byte cursor lives in `<state_filename>`.
    - Round-robin schedule over stage's works, weighted by manifest weights.
    - Each chunk is fed through brain.train_language() and brain.learn_language()
      mirroring the babbling-loop call shape. Per-chunk failures are logged
      and skipped (one bad chunk doesn't tank the run).

    The same code path drives both the literary (data/canonical_corpus/)
    and mathematical (data/math_corpus/) ingestion — they share the
    index.json schema; only the corpus root and state filename differ.
    """
    try:
        from canonical_corpus import (
            load_index, works_for_stage, iter_chunks, round_robin_schedule,
        )
    except Exception as e:
        print(f"  [{label}] loader import failed: {e} — skipping")
        return

    try:
        idx = load_index(corpus_root)
    except FileNotFoundError:
        # Corpus is optional — quiet skip.
        return
    except Exception as e:
        print(f"  [{label}] load_index failed: {e} — skipping")
        return

    works = works_for_stage(idx, stage)
    if not works:
        return

    state = _load_canonical_state(checkpoint_dir, state_filename)
    work_by_id = {w["id"]: w for w in works}

    # Plan a round-robin order over works for this call.
    schedule_size = max(20, len(works) * 5)
    schedule = round_robin_schedule(works, total_chunks=schedule_size)

    chunks_done_call = 0
    bytes_done_call = 0
    cap = max_chunks_per_call if (max_chunks_per_call and max_chunks_per_call > 0) else None

    for work_id in schedule:
        if cap is not None and chunks_done_call >= cap:
            break
        work = work_by_id.get(work_id)
        if work is None:
            continue
        wstate = state["by_work"].setdefault(work_id, {
            "byte_offset": 0, "chunks_done": 0,
            "last_stage": None, "last_step": None, "done": False,
        })
        if wstate.get("done"):
            continue

        # Pull just the first chunk from the resume cursor; iter_chunks is a
        # generator so we don't materialize the whole work.
        start_byte = int(wstate.get("byte_offset", 0) or 0)
        try:
            it = iter_chunks(work, corpus_root,
                             start_byte=start_byte,
                             max_chars=chunk_chars)
            first = next(it, None)
        except Exception as e:
            print(f"  [Canonical] {work_id} iter failed: {e}")
            wstate["done"] = True
            continue
        if first is None:
            wstate["done"] = True
            continue
        end_byte, text = first
        if not text or not text.strip():
            wstate["byte_offset"] = end_byte
            continue

        # Set a baseline grounding emotion derived from the work's register
        # so e.g. Poe gets dark/gothic affect and KJV gets reverent/calm
        # affect, instead of every chunk grounding into emotion=(0,0).
        # If the proxy doesn't expose set_grounding_emotion yet (older
        # daemon), or the call raises for any reason, we degrade silently
        # and continue — text ingestion must not be blocked by the affect
        # channel.
        register = (work or {}).get("register")
        emotion = _REGISTER_EMOTION.get(register) if register else None
        if emotion is not None:
            try:
                brain.set_grounding_emotion(float(emotion[0]),
                                             float(emotion[1]))
            except Exception:
                pass

        # Drive the chunk into the brain. Mirror the babbling-loop shape:
        # train_language(text, target_text=text) + learn_language(text).
        try:
            try:
                brain.train_language(text, text)
            except TypeError:
                # Some bindings expose train_language as kw-only.
                brain.train_language(text=text, target_text=text)
            brain.learn_language(text)
        except Exception as e:
            # Don't propagate — one bad chunk shouldn't tank the run.
            print(f"  [Canonical] {work_id} brain call failed: {e}")
            # Still advance the cursor so we don't re-feed the same bad chunk.

        # Reset baseline emotion so subsequent (non-canonical) ingestion
        # paths don't inherit this work's affect.
        if emotion is not None:
            try:
                brain.set_grounding_emotion(0.0, 0.0)
            except Exception:
                pass

        # Update state.
        delta_bytes = max(0, end_byte - start_byte)
        wstate["byte_offset"] = int(end_byte)
        wstate["chunks_done"] = int(wstate.get("chunks_done", 0)) + 1
        wstate["last_stage"] = int(stage)
        wstate["last_step"] = int(wstate.get("chunks_done", 0))
        state["totals"]["chunks_ingested"] = int(state["totals"].get("chunks_ingested", 0)) + 1
        state["totals"]["bytes_ingested"] = int(state["totals"].get("bytes_ingested", 0)) + delta_bytes

        chunks_done_call += 1
        bytes_done_call += delta_bytes

        # Periodic state persistence.
        if chunks_done_call % max(1, int(log_every)) == 0:
            _save_canonical_state(checkpoint_dir, state)

    # Final flush.
    _save_canonical_state(checkpoint_dir, state)

    print(f"  [{label}] stage {stage}: ingested {chunks_done_call} chunks "
          f"across {len(works)} works ({bytes_done_call} bytes)")


def _ingest_streaming_corpus(brain, stage, *, kind,
                              max_chunks_per_call=None,
                              chunk_chars=1200,
                              checkpoint_dir,
                              log_every=20):
    """Drive chunks from a streaming source (Wikipedia, Investopedia,
    arXiv, Gutenberg-stream, etc.) into the brain for one stage.

    Pulls one chunk per source per call, advances the resume cursor,
    persists state every `log_every` chunks. NEVER writes source content
    to disk — required by the deployment's storage constraint.

    `kind` is the StreamingSource registered key; if no source is
    registered for that kind (backend not yet implemented), this is a
    silent no-op.
    """
    try:
        from streaming_ingest import (
            get_source, load_streaming_state, save_streaming_state,
            _kind_state, iter_one_chunk,
        )
    except Exception as e:
        print(f"  [Stream] streaming_ingest import failed: {e} — skipping")
        return

    src = get_source(kind)
    if src is None:
        return  # Backend not yet wired; silent skip.

    try:
        source_ids = src.list_for_stage(stage)
    except Exception as e:
        print(f"  [Stream:{kind}] list_for_stage failed: {e} — skipping")
        return
    if not source_ids:
        return

    state = load_streaming_state(checkpoint_dir)
    sub = _kind_state(state, kind)
    completed = set(sub.get("completed", []))
    in_progress = sub.setdefault("in_progress", {})

    chunks_done_call = 0
    bytes_done_call = 0
    cap = max_chunks_per_call if (max_chunks_per_call and max_chunks_per_call > 0) else None

    # Round-robin one chunk per remaining source per pass.
    pending = [sid for sid in source_ids if sid not in completed]
    pass_count = 0
    while pending:
        if cap is not None and chunks_done_call >= cap:
            break
        new_pending = []
        for sid in pending:
            if cap is not None and chunks_done_call >= cap:
                new_pending.append(sid)
                continue
            position = int(in_progress.get(sid, 0) or 0)
            try:
                pair = iter_one_chunk(kind, sid,
                                       start_position=position,
                                       chunk_chars=chunk_chars)
            except Exception as e:
                print(f"  [Stream:{kind}] {sid} fetch failed: {e}")
                # Don't advance position; will retry next call.
                continue
            if pair is None:
                # End of this source.
                if sid not in completed:
                    completed.add(sid)
                    sub["completed"] = sorted(completed)
                in_progress.pop(sid, None)
                continue
            next_position, text = pair
            if not text or not text.strip():
                in_progress[sid] = int(next_position)
                continue

            # Drive the chunk through the brain. Mirror the canonical /
            # math seam's call shape so the failure modes are identical.
            try:
                try:
                    brain.train_language(text, text)
                except TypeError:
                    brain.train_language(text=text, target_text=text)
                brain.learn_language(text)
            except Exception as e:
                print(f"  [Stream:{kind}] {sid} brain call failed: {e}")
                # Still advance — sticky-bad chunks shouldn't loop forever.

            in_progress[sid] = int(next_position)
            chunks_done_call += 1
            bytes_done_call += len(text.encode("utf-8"))
            state["totals"]["chunks_ingested"] = (
                int(state["totals"].get("chunks_ingested", 0)) + 1)
            state["totals"]["bytes_ingested"] = (
                int(state["totals"].get("bytes_ingested", 0))
                + len(text.encode("utf-8")))

            # Did we just finish the source?
            try:
                if src.is_complete(sid, int(next_position)):
                    completed.add(sid)
                    sub["completed"] = sorted(completed)
                    in_progress.pop(sid, None)
                    continue
            except Exception:
                pass
            new_pending.append(sid)

            if chunks_done_call % max(1, int(log_every)) == 0:
                save_streaming_state(checkpoint_dir, state)

        pending = new_pending
        pass_count += 1
        # Defensive bound: don't loop indefinitely on a degenerate source.
        if pass_count > 1000:
            break

    save_streaming_state(checkpoint_dir, state)
    print(f"  [Stream:{kind}] stage {stage}: ingested {chunks_done_call} "
          f"chunks across {len(source_ids)} sources ({bytes_done_call} bytes)")


def _ingest_math_corpus(brain, stage, *, corpus_root="data/math_corpus",
                         max_chunks_per_call=None,
                         chunk_chars=1200,
                         checkpoint_dir,
                         log_every=20):
    """Drive math-corpus chunks. Thin wrapper over _ingest_canonical_corpus
    (same schema, different root + state file).

    The math corpus complements (does not replace) the parietal lobe's
    numerical primitives. Parietal supplies number sense and magnitude
    comparison; the corpus supplies symbolic content (variables, proofs,
    calculus, logic) that has no architectural prior.
    """
    _ingest_canonical_corpus(
        brain, stage,
        corpus_root=corpus_root,
        max_chunks_per_call=max_chunks_per_call,
        chunk_chars=chunk_chars,
        checkpoint_dir=checkpoint_dir,
        log_every=log_every,
        state_filename=MATH_STATE_FILENAME,
        label="Math",
    )


# CE-11/12/13/14/5 STEM + finance + code corpus state filenames. Each gets
# its own resume-state file so they don't trample one another.
PHYSICS_STATE_FILENAME = ".physics_corpus_state.json"
CHEMISTRY_STATE_FILENAME = ".chemistry_corpus_state.json"
BIOLOGY_STATE_FILENAME = ".biology_corpus_state.json"
FINANCE_STATE_FILENAME = ".finance_corpus_state.json"
CODE_STATE_FILENAME = ".code_corpus_state.json"


def _ingest_physics_corpus(brain, stage, *, corpus_root="data/physics_corpus",
                            max_chunks_per_call=None, chunk_chars=1200,
                            checkpoint_dir, log_every=20):
    """CE-11 wrapper. Same shape as _ingest_math_corpus."""
    _ingest_canonical_corpus(
        brain, stage,
        corpus_root=corpus_root,
        max_chunks_per_call=max_chunks_per_call,
        chunk_chars=chunk_chars,
        checkpoint_dir=checkpoint_dir,
        log_every=log_every,
        state_filename=PHYSICS_STATE_FILENAME,
        label="Physics",
    )


def _ingest_chemistry_corpus(brain, stage, *, corpus_root="data/chemistry_corpus",
                              max_chunks_per_call=None, chunk_chars=1200,
                              checkpoint_dir, log_every=20):
    """CE-12 wrapper."""
    _ingest_canonical_corpus(
        brain, stage,
        corpus_root=corpus_root,
        max_chunks_per_call=max_chunks_per_call,
        chunk_chars=chunk_chars,
        checkpoint_dir=checkpoint_dir,
        log_every=log_every,
        state_filename=CHEMISTRY_STATE_FILENAME,
        label="Chemistry",
    )


def _ingest_biology_corpus(brain, stage, *, corpus_root="data/biology_corpus",
                            max_chunks_per_call=None, chunk_chars=1200,
                            checkpoint_dir, log_every=20):
    """CE-13 wrapper."""
    _ingest_canonical_corpus(
        brain, stage,
        corpus_root=corpus_root,
        max_chunks_per_call=max_chunks_per_call,
        chunk_chars=chunk_chars,
        checkpoint_dir=checkpoint_dir,
        log_every=log_every,
        state_filename=BIOLOGY_STATE_FILENAME,
        label="Biology",
    )


def _ingest_finance_corpus(brain, stage, *, corpus_root="data/finance_corpus",
                            max_chunks_per_call=None, chunk_chars=1200,
                            checkpoint_dir, log_every=20):
    """CE-14 wrapper."""
    _ingest_canonical_corpus(
        brain, stage,
        corpus_root=corpus_root,
        max_chunks_per_call=max_chunks_per_call,
        chunk_chars=chunk_chars,
        checkpoint_dir=checkpoint_dir,
        log_every=log_every,
        state_filename=FINANCE_STATE_FILENAME,
        label="Finance",
    )


def _ingest_code_corpus(brain, stage, *, corpus_root="data/code_corpus",
                        max_chunks_per_call=None, chunk_chars=1200,
                        checkpoint_dir, log_every=20):
    """CE-5 wrapper. Note: code chunks are function-aligned, not
    paragraph-aligned. The canonical loader still tokenizes the file as
    text — that's fine for first-pass ingest. The richer code-aware
    chunker in scripts/code_corpus.py is exposed for downstream consumers
    that want function-level granularity."""
    _ingest_canonical_corpus(
        brain, stage,
        corpus_root=corpus_root,
        max_chunks_per_call=max_chunks_per_call,
        chunk_chars=chunk_chars,
        checkpoint_dir=checkpoint_dir,
        log_every=log_every,
        state_filename=CODE_STATE_FILENAME,
        label="Code",
    )


# CE-1..3, 6..10, 19 probe-style drips. Each module exposes a
# `run_X_drip(brain, stage, *, composer=None, ...)` function. We dispatch
# them all through one helper so the cadence + error handling stays
# uniform across the curriculum.
# Each entry: (module_name, function_name, min_stage).
# min_stage gates abstract / metacognitive drips so they don't fire during
# Stage 1 — at parentese-comprehension developmental level, drilling abstract
# Q&A / theory-of-mind / counterfactual templates collapses production into
# a "verb + abstract-noun" template space (eval 2026-05-05). Concrete /
# sensory / narrative drips stay at min_stage=1. Abstract drips activate at
# min_stage=2 once the brain has more grounded vocabulary to work with.
_PROBE_DRIPS = (
    ("storytelling",   "run_storytelling_drip",     1),  # CE-1  concrete narrative
    ("socratic_qa",    "run_socratic_drip",         2),  # CE-2  abstract Q&A
    ("sibling_dialog", "run_sibling_drip",          2),  # CE-3  abstract paired Q&A
    ("music_rhythm",   "run_music_drip",            1),  # CE-6  sensory
    ("visual_art",     "run_visual_drip",           1),  # CE-7  sensory
    ("tom_probes",     "run_tom_drip",              2),  # CE-8  theory-of-mind
    ("counterfactual", "run_counterfactual_drip",   2),  # CE-9  counterfactual reasoning
    ("failure_replay", "run_replay_drip",           1),  # CE-10 meta on produced text
    ("claude_teacher", "run_claude_teacher_drip",   2),  # CE-19 abstract pedagogy
)


# Per-corpus developmental min-stage. Eval 2026-05-05 showed that priming
# all formal/symbolic corpora into Stage-1 (parentese-comprehension) brain
# collapses production into "[abstract verb] + [abstract noun]" templates.
# Stage 1 is the parentese / sensory-grounding period — it should see only
# concrete narrative + sensory descriptors, not formal physics/chemistry/
# biology/finance/code. Stage 2 admits descriptive science. Stage 3 admits
# formal symbolic / financial / code text.
_CORPUS_MIN_STAGE = {
    "physics":   2,
    "chemistry": 2,
    "biology":   2,
    "finance":   3,
    "code":      3,
}


def _run_extra_corpora_priming(brain, stage, *, checkpoint_dir):
    """Stage-entry priming for CE-5/11/12/13/14. Each fires once per stage
    with no per-call cap (full priming pass, like canonical/math). Each
    cfg's `enabled` flag gates the call individually; missing data dirs
    surface as a warning inside the canonical-loader path. Per-corpus
    developmental min_stage gates apply on top of `enabled` (see
    _CORPUS_MIN_STAGE)."""
    s = int(stage)
    if (_PHYSICS_CFG and _PHYSICS_CFG.get("enabled")
            and s >= _CORPUS_MIN_STAGE["physics"]):
        try:
            _ingest_physics_corpus(
                brain, stage=stage,
                corpus_root=_PHYSICS_CFG.get("root", "data/physics_corpus"),
                chunk_chars=_PHYSICS_CFG.get("chunk_chars", 1200),
                max_chunks_per_call=(_PHYSICS_CFG.get("max_chunks_per_stage") or None),
                checkpoint_dir=checkpoint_dir)
        except Exception as _e:
            print(f"  [Physics] stage-{stage} priming failed: {_e}")
    if (_CHEMISTRY_CFG and _CHEMISTRY_CFG.get("enabled")
            and s >= _CORPUS_MIN_STAGE["chemistry"]):
        try:
            _ingest_chemistry_corpus(
                brain, stage=stage,
                corpus_root=_CHEMISTRY_CFG.get("root", "data/chemistry_corpus"),
                chunk_chars=_CHEMISTRY_CFG.get("chunk_chars", 1200),
                max_chunks_per_call=(_CHEMISTRY_CFG.get("max_chunks_per_stage") or None),
                checkpoint_dir=checkpoint_dir)
        except Exception as _e:
            print(f"  [Chemistry] stage-{stage} priming failed: {_e}")
    if (_BIOLOGY_CFG and _BIOLOGY_CFG.get("enabled")
            and s >= _CORPUS_MIN_STAGE["biology"]):
        try:
            _ingest_biology_corpus(
                brain, stage=stage,
                corpus_root=_BIOLOGY_CFG.get("root", "data/biology_corpus"),
                chunk_chars=_BIOLOGY_CFG.get("chunk_chars", 1200),
                max_chunks_per_call=(_BIOLOGY_CFG.get("max_chunks_per_stage") or None),
                checkpoint_dir=checkpoint_dir)
        except Exception as _e:
            print(f"  [Biology] stage-{stage} priming failed: {_e}")
    if (_FINANCE_CFG and _FINANCE_CFG.get("enabled")
            and s >= _CORPUS_MIN_STAGE["finance"]):
        try:
            _ingest_finance_corpus(
                brain, stage=stage,
                corpus_root=_FINANCE_CFG.get("root", "data/finance_corpus"),
                chunk_chars=_FINANCE_CFG.get("chunk_chars", 1200),
                max_chunks_per_call=(_FINANCE_CFG.get("max_chunks_per_stage") or None),
                checkpoint_dir=checkpoint_dir)
        except Exception as _e:
            print(f"  [Finance] stage-{stage} priming failed: {_e}")
    if (_CODE_CFG and _CODE_CFG.get("enabled")
            and s >= _CORPUS_MIN_STAGE["code"]):
        try:
            _ingest_code_corpus(
                brain, stage=stage,
                corpus_root=_CODE_CFG.get("root", "data/code_corpus"),
                chunk_chars=_CODE_CFG.get("chunk_chars", 1500),
                max_chunks_per_call=(_CODE_CFG.get("max_chunks_per_stage") or None),
                checkpoint_dir=checkpoint_dir)
        except Exception as _e:
            print(f"  [Code] stage-{stage} priming failed: {_e}")


def _run_probe_drips(brain, stage, *, composer=None,
                     skip_kinds=()):
    """Fire each registered probe drip once. Each drip carries its own
    min_stage threshold (third tuple element) so abstract / metacognitive
    drips don't fire during Stage 1 — empirically (eval 2026-05-05) that
    collapses production into a "verb + abstract-noun" template space.
    We isolate failures per-module so one broken probe can't kill the
    whole stage."""
    for entry in _PROBE_DRIPS:
        # Backward-compat: tolerate 2-tuple entries (default min_stage=1).
        if len(entry) == 3:
            mod_name, fn_name, min_stage = entry
        else:
            mod_name, fn_name = entry
            min_stage = 1
        if mod_name in skip_kinds:
            continue
        if int(stage) < int(min_stage):
            continue
        try:
            mod = __import__(mod_name)
            fn = getattr(mod, fn_name, None)
            if fn is None:
                continue
            # CE-2 socratic_qa needs a chunk_text; we feed it a tiny
            # stage-appropriate snippet so the drip has something to ask
            # about. The richer integration (real corpus chunks) can come
            # later — this keeps the probe alive without requiring corpus
            # plumbing through the drip layer.
            if mod_name == "socratic_qa":
                _snippets = {
                    1: "",
                    2: "The cat watched the bird through the window. "
                       "When the man opened the door the bird flew away.",
                    3: "The captain ignored the warning signal because "
                       "he trusted his charts. The ship struck the reef "
                       "before dawn and the survivors were rescued at first light.",
                }
                fn(brain, stage, chunk_text=_snippets.get(int(stage), ""),
                   composer=composer)
            else:
                fn(brain, stage, composer=composer)
        except Exception as _e:
            print(f"  [Probe:{mod_name}] drip failed: {_e}")


# ============================================================================
# Stage Runners
# ============================================================================

def run_stage_0(brain, composer, parent, clock, source, decoder,
                num_stimuli=10000, start_from=0):
    """Stage 0: Awakening — sensory exposure with wonder.

    Goal: Get neurons responding. Self-reconstruction of sensory patterns.
    """
    if start_from >= num_stimuli:
        print(f"  [Stage 0] Already complete (step {start_from} >= {num_stimuli}) — skipping")
        return []
    print("\n" + "=" * 60)
    print("  STAGE 0: Welcome to the World, Little One")
    print("=" * 60)

    parent.pre_generate_content(stage=0, num_stimuli=num_stimuli)

    try:
        brain.set_plasticity_state("ACQUISITION")
    except Exception:
        pass

    # --- Warmup: break symmetry with high-confidence diverse stimuli ---
    # Skip if brain already has learning steps (e.g. daemon brain persists across restarts)
    _existing_steps = 0
    try:
        _existing_steps = brain.probe().get("total_learning_steps", 0)
    except Exception:
        pass
    if start_from == 0 and _existing_steps < 10:
        print("  [Warmup] Breaking symmetry with diverse stimuli...")
        warmup_texts = [
            "bright red color", "deep blue ocean", "soft green grass",
            "loud thunder sound", "quiet whisper", "warm sunshine",
            "cold ice freezing", "sweet honey taste", "sour lemon",
            "big elephant", "tiny ant", "fast cheetah running",
            "slow turtle crawling", "happy laughing child", "sad crying",
            "a dog barking loudly", "a cat purring softly",
            "the moon glowing at night", "the sun blazing at noon",
            "water flowing in a river", "fire crackling in a hearth",
            "birds singing in trees", "wind blowing through leaves",
            "a mountain reaching to the sky", "a valley between hills",
        ]
        for wi, wtext in enumerate(warmup_texts):
            features = composer.compose(text=wtext, modality="text")
            target = make_semantic_target(wtext)
            try:
                brain.learn_vector(features, target, label=wtext[:50], confidence=0.9)
            except RuntimeError as e:
                if wi == 0:
                    print(f"  [Warmup] First stimulus failed ({e}), continuing...")
        print(f"  [Warmup] {len(warmup_texts)} diverse stimuli — done")

    # --- Retroactive cognitive module seeding ---
    # When resuming from a checkpoint trained without cognitive wiring,
    # run all 185 cognitive items through learn_vector() so the 8 modules
    # (ToM, RCOG, Ethics, Imagination, Introspection, Dragonfly, Portia,
    # Collective) catch up with the rest of the network.
    _cog_seed_flag = os.path.join(CHECKPOINT_DIR, ".cognitive_seeded")
    if start_from > 0 and not os.path.exists(_cog_seed_flag):
        base_lr = BASE_LEARNING_RATE
        try:
            # Use a moderate LR — not too aggressive since network is already trained
            _retroactive_cognitive_seed(brain, composer, start_from, base_lr * 0.5)
        except Exception as e:
            print(f"  [Cognitive Sync] Error during seeding: {e}")
        # Flag so we don't repeat on next resume
        try:
            with open(_cog_seed_flag, "w") as f:
                json.dump({"timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                           "steps_at_seed": start_from}, f)
        except Exception:
            pass

    # Initialize spectral k-fold cross validation
    global _spectral_splitter, _current_fold
    try:
        fold_file = os.path.join(CHECKPOINT_DIR, ".spectral_fold")
        if os.path.exists(fold_file):
            with open(fold_file) as f:
                _current_fold = json.load(f).get("fold", 0)
        if SpectralKFoldSplitter is not None:
            _spectral_splitter = SpectralKFoldSplitter(get_all_cognitive_data(), k=5, seed=42)
            _spectral_splitter.print_summary()
            evaluate_performance._spectral_splitter = _spectral_splitter
            evaluate_performance._fold_idx = _current_fold
            print(f"  [Spectral CV] Using fold {_current_fold} as held-out test set")
        else:
            print("  [Spectral CV] SpectralKFoldSplitter not available, skipping")
    except Exception as e:
        print(f"  [Spectral CV] Init failed: {e}, falling back to standard training")
        _spectral_splitter = None

    losses = []
    prefetcher = ParallelDataLoader(source, composer, num_workers=2, prefetch_batches=4, batch_size=64)
    batch = []
    batch_idx = 0
    lr_scheduler = CosineAnnealingLR(lr_max=BASE_LEARNING_RATE, lr_min=MIN_LEARNING_RATE,
                                      warmup_steps=LR_WARMUP_STEPS, t_max=num_stimuli)
    adaptive_batch = AdaptiveBatchSize(min_batch=4, max_batch=32, initial=8)
    hard_miner = HardExampleMiner(max_buffer=500, replay_fraction=0.2)
    temporal = MultiResolutionTemporal(brain, slow_interval=5)
    early_stop = EarlyStopping(patience=8, min_delta=0.005, mode="min")
    contrastive = ContrastiveRegularizer(
        buffer_size=200, interval=20, batch_size=32,
        similarity_threshold=0.80, min_input_distance=0.3, strength=0.6)
    diversity = DiversityRegularizer(
        buffer_size=100, interval=100, num_corrections=8,
        strength=0.2, min_variance_ratio=0.1)
    collapse_detector = CollapseDetector(brain_ref=brain)
    curriculum = CurriculumEscalator(unlock_threshold=500.0)
    mini_batch_buf = []
    # Publish initial stage state to the brain dashboard so the
    # get_training_dashboard RPC stops returning zeros.
    _push_dashboard(brain, stage=0, step=start_from,
                    domain="stage0_sensory",
                    lr_physics=lr_scheduler.get_lr(),
                    lr_chemistry=lr_scheduler.get_lr(),
                    lr_biology=lr_scheduler.get_lr())
    for i in range(start_from, num_stimuli):
        # Mode collapse early detection
        if ((i + 1) % COLLAPSE_CHECK_INTERVAL == 0
                and (i + 1) >= COLLAPSE_WARMUP_STEPS):
            if collapse_detector.check(brain, step=i + 1, stage=0):
                print("  Training halted due to mode collapse.", flush=True)
                return losses

        # Check biological clock
        action = clock.tick(brain)
        if action == "sleep":
            clock.do_sleep(brain, parent)
        elif action == "consolidate":
            clock.do_consolidate(brain)

        # Get pre-computed stimulus from prefetcher (batch-encoded)
        if batch_idx >= len(batch):
            batch = prefetcher.get_batch()
            batch_idx = 0
        description, features, target = batch[batch_idx]
        batch_idx += 1

        # Forward pass for observation — stage sensory data first
        narration = parent._pop_content("_narrations")
        if narration:
            print(f"  Parent: {narration}")
        import time as _t
        _loop_t0 = _t.monotonic()
        sensory_mods = submit_multimodal(brain, description) or []
        _smm_ms = (_t.monotonic() - _loop_t0) * 1000
        if sensory_mods and i < 5:
            print(f"  [Sensory] step {i}: {'+'.join(sensory_mods)} for '{description[:60]}'", flush=True)

        # Skip decide_full for all of stage 0 — sensory exposure phase.
        # decide_full balloons to 2+ minutes after step 2000 due to accumulated
        # cognitive state. The decoder/contrastive regularization can wait
        # until stage 1 where naming/labeling makes inference meaningful.
        STAGE0_DECIDE_SKIP_STEPS = 999999  # never run in stage 0
        STAGE0_DECIDE_INTERVAL = 4
        _dec_ms = 0
        result = None
        _out_vec = None
        if i >= STAGE0_DECIDE_SKIP_STEPS and (i % STAGE0_DECIDE_INTERVAL == 0):
            _dec_t0 = _t.monotonic()
            result = brain.decide_full(features)
            _dec_ms = (_t.monotonic() - _dec_t0) * 1000
            _out_vec = result.get("output_vector") if result else None

        # Held-out check: 20% of items are evaluation-only (no learning)
        _is_held_out = _held_out_buffer.is_held_out(features)
        if _is_held_out:
            _held_out_buffer.add(features, target, domain="stage0_sensory")

        # Accumulate into mini-batch (adaptive size) — skip held-out items
        if not _is_held_out:
            mini_batch_buf.append((features, target, description, _out_vec))

        # Inject hard example replays periodically
        if hard_miner.should_replay() and len(mini_batch_buf) < adaptive_batch.size:
            replays = hard_miner.get_hard_examples(2)
            for rdesc, rfeat, rtgt in replays:
                mini_batch_buf.append((rfeat, rtgt, rdesc, None))

        if len(mini_batch_buf) >= adaptive_batch.size or i == num_stimuli - 1:
            # Flush mini-batch: GPU batch for ANN + subset dispatch for LNN/SNN/CNN.
            # learn_vector_batch trains ANN on full batch, then dispatches
            # brain_learn_vector on every 4th sample for secondary networks.
            current_lr = lr_scheduler.get_lr()
            # Re-stage sensory data for cortex CNN training (decide_full clears it).
            # Use last description in batch as representative sensory stimulus.
            last_desc = mini_batch_buf[-1][2] if mini_batch_buf else ""
            if last_desc:
                submit_multimodal(brain, last_desc)
            try:
                batch_pairs = [(f, t) for f, t, _, _ in mini_batch_buf]
                avg_loss = brain.learn_vector_batch(batch_pairs,
                                                     learning_rate=current_lr)
                if avg_loss is not None and avg_loss >= 0:
                    for f, t, desc, _ in mini_batch_buf:
                        losses.append(avg_loss)
                        hard_miner.record(avg_loss, desc, f, t)
                    adaptive_batch.record_loss(avg_loss)
            except Exception as _be:
                # Fallback: per-sample. Audit fix #12 — log batch failure
                # instead of swallowing it silently.
                logger.warning("learn_vector_batch failed (%s) — falling back to "
                               "per-sample for %d items", _be, len(mini_batch_buf))
                _per_sample_failures = 0
                for f, t, desc, _ in mini_batch_buf:
                    try:
                        loss = brain.learn_vector(f, t, label=desc[:50],
                                                   confidence=0.5,
                                                   learning_rate=current_lr)
                        losses.append(loss)
                        hard_miner.record(loss, desc, f, t)
                        adaptive_batch.record_loss(loss)
                    except Exception as _se:
                        _per_sample_failures += 1
                        losses.append(0.0)
                if _per_sample_failures > 0:
                    logger.warning("Per-sample fallback also failed for %d/%d items",
                                   _per_sample_failures, len(mini_batch_buf))

            # Use cached output vectors from decide_full — no extra forward passes
            if parent.decoder:
                for f, t, desc, out_vec in mini_batch_buf:
                    if out_vec is not None:
                        target_emb = encode_text(desc)
                        parent.decoder.record_pair(out_vec, target_emb, desc)
                        contrastive.record(target_emb, out_vec, f)
                        diversity.record(out_vec, f)
            mini_batch_buf = []
            lr_scheduler.step()
            adaptive_batch.step()
            _loop_total_ms = (_t.monotonic() - _loop_t0) * 1000
            if _loop_total_ms > 5000 or (i + 1) % 5 == 0:
                print(f"  [Timing] step {i}: total={_loop_total_ms:.0f}ms "
                      f"(sensory={_smm_ms:.0f} decide={_dec_ms:.0f} "
                      f"rest={_loop_total_ms - _smm_ms - _dec_ms:.0f})",
                      flush=True)

            # Contrastive + diversity corrections
            if contrastive.should_correct():
                n_corr, mean_sim = contrastive.apply_corrections(
                    brain, learning_rate=lr_scheduler.get_lr())
                if n_corr > 0 and (i + 1) % 500 < 60:
                    print(f"    [Contrastive] {n_corr} corrections, "
                          f"mean_output_sim={mean_sim:.3f}")
            if diversity.should_correct():
                n_div, top_r, eff_r = diversity.apply_corrections(
                    brain, learning_rate=lr_scheduler.get_lr() * 0.3)
                if n_div > 0 and (i + 1) % 500 < 110:
                    print(f"    [Diversity] {n_div} corrections, "
                          f"top_eig={top_r:.2f}, eff_rank={eff_r:.1f}")

        # Cognitive module training injection — every N sensory steps
        if (i + 1) % COGNITIVE_TRAIN_INTERVAL == 0:
            cog_loss = _inject_cognitive_training(
                brain, composer, step=i, learning_rate=lr_scheduler.get_lr(),
                spectral_splitter=_spectral_splitter, fold_idx=_current_fold,
                curriculum=curriculum)
            if cog_loss is not None:
                losses.append(cog_loss)

        try:
            brain.bg_update_reward(0.5, 0.3)
        except Exception:
            pass

        # Caregiver critic — score Athena's production against stage-0
        # developmental rules and deliver reward/recast into the brain.
        # _train_cognitive (the original critic hook) is bypassed by this
        # loop's hot path, so drive the critic directly here.
        if (i + 1) % CRITIC_EVAL_INTERVAL == 0:
            parent.run_critic_eval(brain, description, stage=0)

        parent.interaction_count += 1

        # Multi-resolution temporal processing (fast + slow LNN)
        temporal.step(features)

        # Record training milestones as autobiographical memories
        if (i + 1) in TRAINING_MILESTONES:
            desc = TRAINING_MILESTONES[i + 1]
            try:
                ms_features = composer.compose(text=desc, modality="text")
                ms_target = make_semantic_target(desc)
                brain.learn_vector(ms_features, ms_target,
                                   label=f"milestone_{i+1}", confidence=1.0,
                                   learning_rate=0.0)
                print(f"  [Milestone] Step {i+1}: {desc}")
            except Exception:
                pass

        # Check for hyperparameter overrides (auto-tuner or manual)
        _check_hp_overrides(brain, lr_scheduler, i + 1)

        # Quick feedback every 50 steps
        if (i + 1) % 50 == 0:
            recent = losses[-50:]
            rl = np.mean(recent) if recent else 0
            nz = sum(1 for x in recent if x > 0.001)
            _total_step_ms = (_t.monotonic() - _loop_t0) * 1000
            parts = [f"[{i+1}] loss={rl:.4f} ({nz}/{len(recent)} non-zero) {_total_step_ms:.0f}ms/step"]
            try:
                ss = brain.snn_get_stats()
                if ss:
                    # Rich SNN diagnostics — tells us if R-STDP is actually
                    # doing something vs just spiking randomly.
                    # sparsity: % silent (want 70-95% for efficient sparse coding)
                    # synchrony: coordination (0.3-0.7 healthy; >0.9 = mode collapse)
                    # spikes/sample: differs per input if SNN discriminates
                    # silent/hyp: dead/saturating neurons
                    parts.append(
                        f"SNN:{ss.get('total_spikes',0)}spk/"
                        f"{ss.get('mean_firing_rate_hz',0):.1f}Hz/"
                        f"spars={ss.get('sparsity',0):.2f}/"
                        f"sync={ss.get('synchrony',0):.2f}/"
                        f"sps={ss.get('spikes_per_sample',0):.1f}/"
                        f"sil={ss.get('silent_neurons',0)}/"
                        f"hyp={ss.get('hyperactive_neurons',0)}"
                    )
            except Exception:
                pass
            try:
                ls = brain.lnn_get_stats()
                if ls:
                    parts.append(f"LNN:{ls.get('forward_steps',0)}fwd/tau={ls.get('avg_tau',0):.2f}")
            except Exception:
                pass
            try:
                nm = brain.get_network_metrics()
                if nm:
                    if nm.get('hnn_active'):
                        parts.append(f"HNN:E={nm.get('hnn_energy',0):.4f}/dev={nm.get('hnn_energy_deviation',0):.6f}")
                    if nm.get('fno_audio_steps', 0) > 0:
                        parts.append(f"FNO:{nm.get('fno_audio_ema_loss',0):.4f}")
            except Exception:
                pass
            try:
                nm2 = brain.get_network_metrics()
                if nm2 and nm2.get('cnn_steps', 0) > 0:
                    parts.append(f"CNN:{nm2.get('cnn_steps',0)}stp/loss={nm2.get('cnn_loss',0):.4f}")
            except Exception:
                pass
            print(f"    {' | '.join(parts)}", flush=True)

            # Live-publish dashboard state. Same 50-step cadence as the
            # quick-feedback block above so we don't add extra socket calls.
            _push_dashboard(brain, stage=0, step=i + 1,
                            domain="stage0_sensory",
                            lr_physics=lr_scheduler.get_lr(),
                            lr_chemistry=lr_scheduler.get_lr(),
                            lr_biology=lr_scheduler.get_lr(),
                            collapse_events=int(getattr(collapse_detector,
                                                        "collapse_count", 0) or 0))

        # Progress report
        if (i + 1) % 500 == 0:
            avg_loss = np.mean(losses[-500:]) if losses else 0
            print(f"\n  [Stage 0] {i+1}/{num_stimuli} — "
                  f"avg_loss={avg_loss:.4f} lr={lr_scheduler.get_lr():.6f} "
                  f"batch={adaptive_batch.size} hard_buf={len(hard_miner.buffer)}")
            print(f"    {contrastive.stats_str()} | {diversity.stats_str()}")
            _print_utm_health(brain)
            _print_bio_stats(brain)
            if curriculum:
                print(f"    [Curriculum] {curriculum.get_status()}")
            losses_to_report = losses[-500:]
            if len(losses_to_report) > 100:
                early = np.mean(losses_to_report[:50])
                late = np.mean(losses_to_report[-50:])
                if late < early:
                    print(f"    Loss trending down: {early:.4f} -> {late:.4f}")
            # Refit decoder and run actual performance evaluation
            if decoder:
                decoder.force_refit()
            evaluate_performance(brain, composer, decoder, stage=0, step=i+1)

            # Early stopping check on average loss
            if early_stop.check(avg_loss):
                print(f"    Early stopping: loss plateaued at {avg_loss:.4f} "
                      f"for {early_stop.patience} evals")
                break

        # Chat eval disabled for Stage 0 — OOM risk with 2.5M neurons.
        # Enable manually via: touch /tmp/athena_chat_now
        if os.path.exists("/tmp/athena_chat_now"):
            os.remove("/tmp/athena_chat_now")
            chat_eval(brain, composer, decoder, stage=0, step=i+1)

        # Inspire every 2000 stimuli
        if (i + 1) % 2000 == 0:
            parent.inspire(brain, composer, stage=0)

        # Checkpoint every 50 steps
        if (i + 1) % 50 == 0:
            _save_checkpoint(brain, decoder, stage=0, step=i+1)
    prefetcher.stop()

    return losses


def run_stage_1(brain, composer, parent, clock, source, decoder,
                num_stimuli=20000, start_from=0):
    """Stage 1: Association — cross-modal binding with enthusiastic naming.

    Goal: Seeing/hearing a concept + its name → same internal representation.
    Language exposure: 15% of steps present language-domain facts (grammar,
    word types) alongside the object naming — like a parent who says both
    "That's a DOG!" and "Dog is a NOUN — nouns name things."
    """
    if start_from >= num_stimuli:
        print(f"  [Stage 1] Already complete (step {start_from} >= {num_stimuli}) — skipping")
        return []
    print("\n" + "=" * 60)
    print("  STAGE 1: Look! That's a ___!")
    print("=" * 60)

    parent.pre_generate_content(stage=1, num_stimuli=num_stimuli)

    try:
        brain.set_plasticity_state("ACQUISITION")
    except Exception:
        pass

    losses = []
    lr_ctrl = AdaptiveLRController(initial_lr=BASE_LEARNING_RATE)
    contrastive = ContrastiveRegularizer(
        buffer_size=200, interval=20, batch_size=32,
        similarity_threshold=0.80, min_input_distance=0.3, strength=0.6)
    diversity = DiversityRegularizer(
        buffer_size=100, interval=50, num_corrections=16,
        strength=0.5, min_variance_ratio=0.3)  # Tighter for batch learning
    collapse_detector = CollapseDetector(brain_ref=brain)
    fast_collapse = FastCollapseDetector(brain_ref=brain, lr_controller=lr_ctrl)
    # Stage 1 curriculum: start with tier 2 unlocked (basics already learned in Stage 0)
    _stage1_curriculum = CurriculumEscalator(unlock_threshold=200.0)
    _stage1_curriculum.current_tier = 2  # Start with reasoning tier unlocked
    # Language exposure ratio: 15% of steps present language facts instead of objects
    STAGE1_LANGUAGE_RATIO = 0.15
    # Batch training: collect BATCH_SIZE items, send as one train_batch_text call.
    # ONNX encoding + learn_vector happen inside the daemon — zero socket overhead.
    BATCH_SIZE = 50
    # Batch mode disabled — daemon's single-threaded worker blocks all requests
    # while processing a batch (50 items × 13s = 11 min unresponsive).
    # Single-step mode is more reliable at 13s/step.
    _use_batch = False
    print("  [Training] Single-step mode (13s/step with full biological modules)")

    # === BATCH MODE ===
    if _use_batch:
        step = start_from
        while step < num_stimuli:
            # Mode collapse check
            if (step > 0 and step % COLLAPSE_CHECK_INTERVAL == 0
                    and step >= COLLAPSE_WARMUP_STEPS):
                if collapse_detector.check(brain, step=step, stage=1):
                    print("  Training halted due to mode collapse.", flush=True)
                    return losses

            action = clock.tick(brain)
            if action == "sleep":
                clock.do_sleep(brain, parent)
            elif action == "consolidate":
                clock.do_consolidate(brain)

            # Collect a batch of items
            batch_items = []
            batch_end = min(step + BATCH_SIZE, num_stimuli)
            for j in range(step, batch_end):
                if random.random() < STAGE1_LANGUAGE_RATIO:
                    description, _ = source.get_fact(preferred_domain="language")
                    name = "language"
                else:
                    name, description = source.get_object()
                narration = parent._pop_content("_narrations")
                if narration:
                    print(f"  Parent: {narration}")
                batch_items.append({
                    "text": description,
                    "target_text": name + " " + description,
                    "label": name[:50],
                })

            # Send entire batch — daemon ONNX-encodes + trains in tight loop
            try:
                result = brain.train_batch_text(batch_items,
                                                 learning_rate=lr_ctrl.get_lr())
            except Exception as e:
                print(f"  [Batch] Error: {e} — falling back to single-step", flush=True)
                _use_batch = False
                break  # Will restart in single-step mode
            if result.get("error"):
                print(f"  [Batch] Daemon error: {result['error']}", flush=True)
                _use_batch = False
                break
            avg_loss = result.get("avg_loss", 0.0)
            n_items = result.get("n_items", len(batch_items))
            ms_per = result.get("ms_per_item", 0)

            for _ in range(n_items):
                losses.append(avg_loss)

            # Periodic logging
            if batch_end % 50 == 0 or batch_end >= num_stimuli:
                recent = losses[-50:]
                rl = np.mean(recent) if recent else 0
                print(f"    [{batch_end}] batch_loss={avg_loss:.4f} avg={rl:.4f} "
                      f"({ms_per:.0f}ms/item, {n_items} items)", flush=True)

            # Checkpoint
            if batch_end % 50 == 0:
                _save_checkpoint(brain, decoder, stage=1, step=batch_end)

            # Canonical-corpus drip — small priming pass interleaved with
            # the main loop so language exposure stays alive throughout
            # the stage rather than only at the priming pass at start.
            if batch_end % 200 == 0 and _CANONICAL_CFG and _CANONICAL_CFG.get("enabled"):
                try:
                    _ingest_canonical_corpus(
                        brain, stage=1,
                        corpus_root=_CANONICAL_CFG.get("root", "data/canonical_corpus"),
                        chunk_chars=_CANONICAL_CFG.get("chunk_chars", 1200),
                        max_chunks_per_call=20,
                        checkpoint_dir=CHECKPOINT_DIR)
                except Exception as _e:
                    print(f"  [Canonical] drip failed: {_e}")

            # Math-corpus drip — DISABLED in Stage 1 (eval 2026-05-05: formal
            # symbolic math is developmentally inappropriate at parentese-
            # comprehension level and feeds the abstract-template collapse).
            # Math priming/drip activates from Stage 2 onward.
            if False and batch_end % 200 == 0 and _MATH_CFG and _MATH_CFG.get("enabled"):
                try:
                    _ingest_math_corpus(
                        brain, stage=1,
                        corpus_root=_MATH_CFG.get("root", "data/math_corpus"),
                        chunk_chars=_MATH_CFG.get("chunk_chars", 1200),
                        max_chunks_per_call=20,
                        checkpoint_dir=CHECKPOINT_DIR)
                except Exception as _e:
                    print(f"  [Math] drip failed: {_e}")

            # Streaming-corpus drip — same pacing. Streams network-fetched
            # chunks (Wikipedia, Investopedia, arXiv, Gutenberg-stream) that
            # MUST NOT be persisted to disk per deployment storage limits.
            if batch_end % 200 == 0 and _STREAMING_CFG and _STREAMING_CFG.get("enabled"):
                for _kind in _STREAMING_CFG.get("kinds", []):
                    try:
                        _ingest_streaming_corpus(
                            brain, stage=1, kind=_kind,
                            chunk_chars=_STREAMING_CFG.get("chunk_chars", 1200),
                            max_chunks_per_call=20,
                            checkpoint_dir=CHECKPOINT_DIR)
                    except Exception as _e:
                        print(f"  [Stream:{_kind}] drip failed: {_e}")
            # CE-1..3, 6..10, 19 probe drips — each module decides its own
            # stage-1 behavior (some are no-op, some are active).
            if batch_end % 200 == 0 and _PROBE_DRIPS_ENABLED:
                _run_probe_drips(brain, stage=1, composer=composer)

            # Cognitive training injection
            if batch_end % COGNITIVE_TRAIN_INTERVAL == 0:
                cog_loss = _inject_cognitive_training(
                    brain, composer, step=batch_end, learning_rate=lr_ctrl.get_lr(),
                    spectral_splitter=_spectral_splitter, fold_idx=_current_fold,
                    curriculum=_stage1_curriculum)
                if cog_loss is not None:
                    losses.append(cog_loss)

            # LNN + cerebellum (every 250 steps)
            if batch_end % 250 == 0:
                try:
                    features = composer.compose(text=batch_items[-1]["text"])
                    brain.lnn_forward_step(features[:128])
                except Exception:
                    pass
                if avg_loss > 0.5:
                    try:
                        brain.cerebellum_process_error(avg_loss)
                    except Exception:
                        pass

            # Progress report
            if batch_end % 500 == 0:
                avg500 = np.mean(losses[-500:]) if losses else 0
                lr_ctrl.update(avg500, batch_end)
                lr_ctrl.update_from_utm(brain, batch_end)
                print(f"\n  [Stage 1] {batch_end}/{num_stimuli} — "
                      f"avg_loss={avg500:.4f} lr={lr_ctrl.get_lr():.6f}")
                _print_utm_health(brain)
                _print_bio_stats(brain)

            step = batch_end

        if _use_batch:
            # Batch completed all steps successfully
            return losses
        else:
            # Batch failed partway — fall through to single-step from current position
            print(f"  [Batch] Falling through to single-step at step {step}", flush=True)
            start_from = step

    # === SINGLE-STEP MODE (fallback) ===
    # Publish initial stage state to the brain dashboard so the
    # get_training_dashboard RPC stops returning zeros.
    _push_dashboard(brain, stage=1, step=start_from,
                    domain="stage1_naming",
                    lr_physics=lr_ctrl.get_lr(),
                    lr_chemistry=lr_ctrl.get_lr(),
                    lr_biology=lr_ctrl.get_lr())
    for i in range(start_from, num_stimuli):
        # Mode collapse early detection
        if ((i + 1) % COLLAPSE_CHECK_INTERVAL == 0
                and (i + 1) >= COLLAPSE_WARMUP_STEPS):
            if collapse_detector.check(brain, step=i + 1, stage=1):
                print("  Training halted due to mode collapse.", flush=True)
                return losses

        import time as _t
        _loop_t0 = _t.monotonic()

        action = clock.tick(brain)
        if action == "sleep":
            clock.do_sleep(brain, parent)
        elif action == "consolidate":
            clock.do_consolidate(brain)

        # Get an object to name (or language fact) — with prefetched features
        if not hasattr(run_stage_1, '_prefetch'):
            from concurrent.futures import ThreadPoolExecutor
            run_stage_1._prefetch = ThreadPoolExecutor(max_workers=2)
            run_stage_1._next_item = None

        if run_stage_1._next_item is not None:
            name, description, pre_features, pre_target = run_stage_1._next_item
        else:
            # Mix language facts into object naming
            if random.random() < STAGE1_LANGUAGE_RATIO:
                description, _ = source.get_fact(preferred_domain="language")
                name = "language"
            else:
                name, description = source.get_object()
            pre_features = None
            pre_target = None

        # Start prefetching NEXT item while this one trains
        def _prefetch_item():
            if random.random() < STAGE1_LANGUAGE_RATIO:
                d, _ = source.get_fact(preferred_domain="language")
                n = "language"
            else:
                n, d = source.get_object()
            f = composer.compose(text=d, modality="text")
            t = make_semantic_target(n + " " + d)
            return (n, d, f, t)
        future = run_stage_1._prefetch.submit(_prefetch_item)

        # Train current item — pass pre-composed features if available
        if pre_features is not None and pre_target is not None:
            # Fast path: features already composed
            submit_multimodal(brain, description)
            result = brain.decide_full(pre_features)
            try:
                brain.bg_update_reward(0.6, 0.4)
            except Exception:
                pass
            narration = parent._pop_content("_narrations")
            if narration:
                print(f"  Parent: {narration}")
            if _held_out_buffer.is_held_out(pre_features):
                _held_out_buffer.add(pre_features, pre_target, domain="stage1_naming")
                loss = 0.0
            else:
                effective_lr = fast_collapse.get_spiked_lr(lr_ctrl.get_lr())
                lr_kwargs = {"learning_rate": effective_lr}
                loss = brain.learn_vector(pre_features, pre_target,
                                          label=name[:50], confidence=0.65,
                                          **lr_kwargs)
                # Cognitive training moved to COGNITIVE_TRAIN_INTERVAL below
        else:
            # Slow path: first iteration, no prefetch yet
            loss, result = parent.show_and_name(brain, composer, name, description,
                                                learning_rate=lr_ctrl.get_lr())

        # Collect prefetched next item (blocks only if prefetch slower than training)
        try:
            run_stage_1._next_item = future.result(timeout=5.0)
        except Exception:
            run_stage_1._next_item = None

        losses.append(loss if loss is not None else 0)

        # === Training integration hook (Phase A-E modules) ===
        # Observes the training event for symbolic writing + memory
        # consolidation. Non-blocking; safe on errors.
        try:
            if _training_integration is not None:
                _baseline = (sum(losses[-50:-1]) / 49) if len(losses) > 50 else None
                _training_integration.after_learn_vector(
                    label=(name[:50] if isinstance(name, str) else str(name)[:50]),
                    description=(description
                                   if isinstance(description, str) else ""),
                    modality="text",
                    loss=float(loss) if loss is not None else 0.0,
                    baseline_loss=_baseline)
        except NameError:
            pass  # _training_integration not defined (legacy path)
        except Exception:
            pass

        # === CORRUPTION GUARD ===
        # Check for NaN/Inf in loss, output vector, AND GPU forward path.
        # NaN can develop during training without appearing in learn_vector loss.
        import math as _math
        _corrupt = False
        if loss is not None and (_math.isnan(loss) or _math.isinf(loss)):
            print(f"\n  *** CORRUPTION DETECTED: loss={loss} at step {i+1} ***",
                  flush=True)
            _corrupt = True
        if isinstance(result, dict):
            _ov = result.get("output_vector")
            if _ov is not None:
                _nan_count = sum(1 for v in _ov[:100] if _math.isnan(v) or _math.isinf(v))
                if _nan_count > 0:
                    print(f"\n  *** CORRUPTION DETECTED: {_nan_count}/100 NaN in output "
                          f"at step {i+1} ***", flush=True)
                    _corrupt = True
        # Every 100 steps: probe GPU forward path via decide_full
        if not _corrupt and (i + 1) % 100 == 0:
            try:
                _probe_r = brain.decide_full(pre_features)
                if isinstance(_probe_r, dict):
                    _probe_ov = _probe_r.get("output_vector", [])
                    _probe_nan = sum(1 for v in _probe_ov[:50] if _math.isnan(v))
                    if _probe_nan > 0:
                        print(f"\n  *** GPU CORRUPTION: {_probe_nan}/50 NaN in decide_full "
                              f"at step {i+1} ***", flush=True)
                        _corrupt = True
            except Exception:
                pass
        if _corrupt:
            import time as _time
            print(f"  *** REPAIRING: zeroing NaN weights and reducing LR ***", flush=True)
            with open("TRAINING_ALERT.txt", "a") as _af:
                _af.write(f"{_time.strftime('%Y-%m-%d %H:%M:%S')} CORRUPTION REPAIRED at step {i+1}: "
                          f"loss={loss}\n")
            # Repair: tell the brain to zero out NaN weights
            try:
                brain.repair_nan_weights()
            except Exception:
                # repair_nan_weights may not exist — try probe-based repair
                try:
                    p = brain.probe()
                    print(f"    Probe after repair: total_learning_steps={p.get('total_learning_steps', '?')}",
                          flush=True)
                except Exception:
                    pass
            # Reduce learning rate to prevent immediate re-corruption
            lr_ctrl.set_lr(lr_ctrl.get_lr() * 0.5)
            print(f"    LR reduced to {lr_ctrl.get_lr():.6f}", flush=True)
            # Skip this step's results — continue with next
            continue

        # Fast collapse detection on output from decide_full (if available)
        output_vec = result.get("output_vector") if isinstance(result, dict) else None
        if (i + 1) % FAST_COLLAPSE_INTERVAL == 0 and output_vec is not None:
            fast_collapse.check_output(output_vec, i + 1)

        # Record output for contrastive/diversity (FIXED: was only in batch path)
        if output_vec is not None and pre_features is not None:
            contrastive.record(pre_target if pre_target is not None else pre_features,
                               output_vec, pre_features)
            diversity.record(output_vec, pre_features)

        # Contrastive + diversity corrections
        if contrastive.should_correct():
            n_corr, mean_sim = contrastive.apply_corrections(
                brain, learning_rate=lr_ctrl.get_lr())
            if n_corr > 0:
                print(f"    [Contrastive] {n_corr} corrections, "
                      f"mean_output_sim={mean_sim:.3f}", flush=True)
        if diversity.should_correct():
            n_div, top_r, eff_r = diversity.apply_corrections(
                brain, learning_rate=lr_ctrl.get_lr() * 0.3)
            if n_div > 0:
                print(f"    [Diversity] {n_div} corrections, "
                      f"top_eig={top_r:.2f}, eff_rank={eff_r:.1f}", flush=True)

        # Cognitive module training injection (with curriculum for Stage 1)
        if (i + 1) % COGNITIVE_TRAIN_INTERVAL == 0:
            cog_loss = _inject_cognitive_training(
                brain, composer, step=i, learning_rate=lr_ctrl.get_lr(),
                spectral_splitter=_spectral_splitter, fold_idx=_current_fold,
                curriculum=_stage1_curriculum)
            if cog_loss is not None:
                losses.append(cog_loss)

        # Caregiver critic — see run_stage_0 note. _train_cognitive is
        # bypassed by this loop's hot path, so drive the critic directly.
        if (i + 1) % CRITIC_EVAL_INTERVAL == 0:
            parent.run_critic_eval(brain, description, stage=1)

        # LNN temporal step (every 5 steps to reduce round-trips)
        if (i + 1) % 5 == 0:
            try:
                features = composer.compose(text=description)
                brain.lnn_forward_step(features[:128])
            except Exception:
                pass

        # Cerebellum error signal on high loss (every 5 steps)
        if (i + 1) % 5 == 0 and loss is not None and loss > 0.5:
            try:
                brain.cerebellum_process_error(loss)
            except Exception:
                pass

        # Quick feedback every 50 steps
        if (i + 1) % 50 == 0:
            recent = losses[-50:]
            rl = np.mean(recent) if recent else 0
            nz = sum(1 for x in recent if x > 0.001)
            _total_step_ms = (_t.monotonic() - _loop_t0) * 1000
            parts = [f"[{i+1}] loss={rl:.4f} ({nz}/{len(recent)} non-zero) {_total_step_ms:.0f}ms/step"]
            try:
                ss = brain.snn_get_stats()
                if ss:
                    # Rich SNN diagnostics — tells us if R-STDP is actually
                    # doing something vs just spiking randomly.
                    # sparsity: % silent (want 70-95% for efficient sparse coding)
                    # synchrony: coordination (0.3-0.7 healthy; >0.9 = mode collapse)
                    # spikes/sample: differs per input if SNN discriminates
                    # silent/hyp: dead/saturating neurons
                    parts.append(
                        f"SNN:{ss.get('total_spikes',0)}spk/"
                        f"{ss.get('mean_firing_rate_hz',0):.1f}Hz/"
                        f"spars={ss.get('sparsity',0):.2f}/"
                        f"sync={ss.get('synchrony',0):.2f}/"
                        f"sps={ss.get('spikes_per_sample',0):.1f}/"
                        f"sil={ss.get('silent_neurons',0)}/"
                        f"hyp={ss.get('hyperactive_neurons',0)}"
                    )
            except Exception:
                pass
            try:
                ls = brain.lnn_get_stats()
                if ls:
                    parts.append(f"LNN:{ls.get('forward_steps',0)}fwd/tau={ls.get('avg_tau',0):.2f}")
            except Exception:
                pass
            try:
                nm = brain.get_network_metrics()
                if nm:
                    if nm.get('hnn_active'):
                        parts.append(f"HNN:E={nm.get('hnn_energy',0):.4f}/dev={nm.get('hnn_energy_deviation',0):.6f}")
                    if nm.get('fno_audio_steps', 0) > 0:
                        parts.append(f"FNO:{nm.get('fno_audio_ema_loss',0):.4f}")
            except Exception:
                pass
            try:
                nm2 = brain.get_network_metrics()
                if nm2 and nm2.get('cnn_steps', 0) > 0:
                    parts.append(f"CNN:{nm2.get('cnn_steps',0)}stp/loss={nm2.get('cnn_loss',0):.4f}")
            except Exception:
                pass
            print(f"    {' | '.join(parts)}", flush=True)

            # Live-publish dashboard state every 50 steps. Single global LR
            # (lr_ctrl) — replicate to all 3 per-domain fields rather than
            # leaving them at zero.
            _push_dashboard(brain, stage=1, step=i + 1,
                            domain="stage1_naming",
                            lr_physics=lr_ctrl.get_lr(),
                            lr_chemistry=lr_ctrl.get_lr(),
                            lr_biology=lr_ctrl.get_lr(),
                            collapse_events=int(getattr(collapse_detector,
                                                        "collapse_count", 0) or 0))

        # Progress
        if (i + 1) % 500 == 0:
            avg_loss = np.mean(losses[-500:]) if losses else 0
            lr_ctrl.update(avg_loss, i + 1)
            lr_ctrl.update_from_utm(brain, i + 1)
            print(f"\n  [Stage 1] {i+1}/{num_stimuli} — "
                  f"avg_loss={avg_loss:.4f} lr={lr_ctrl.get_lr():.6f}"
                  f" | {contrastive.stats_str()} | {diversity.stats_str()}")
            _print_utm_health(brain)
            _print_bio_stats(brain)
            if decoder:
                decoder.force_refit()
            evaluate_performance(brain, composer, decoder, stage=1, step=i+1)

        # Moral lesson every 3000
        if (i + 1) % 3000 == 0:
            parent.teach_moral_lesson(brain, composer, stage=1)

        # Speech training every 500
        if (i + 1) % 500 == 0:
            parent.teach_speech(brain, composer, stage=1)

        # Chat eval disabled for Stage 1 — OOM risk with 2.5M neurons + 77K vocab decoder.
        # Loss metrics from every step are sufficient for monitoring.
        # Enable manually via: touch /tmp/athena_chat_now
        if os.path.exists("/tmp/athena_chat_now"):
            os.remove("/tmp/athena_chat_now")
            chat_eval(brain, composer, decoder, stage=1, step=i+1)

        # Inspire every 5000
        if (i + 1) % 5000 == 0:
            parent.inspire(brain, composer, stage=1)

        # Checkpoint every 50 steps
        if (i + 1) % 50 == 0:
            _save_checkpoint(brain, decoder, stage=1, step=i+1)

        # Forward chain KB every 2000
        if (i + 1) % 2000 == 0:
            try:
                derived = brain.ti_forward_chain(10)
                if derived > 0:
                    print(f"    KB forward chain: {derived} new facts derived")
            except Exception:
                pass

    return losses


def run_stage_2(brain, composer, parent, clock, source, decoder,
                num_stimuli=20000, start_from=0):
    """Stage 2: Babbling — feedback loop with gentle correction.

    Goal: Athena responds, gets evaluated, learns from feedback.

    Anti-collapse measures for Stage 1 → Stage 2 transition:
    1. LR reset with fresh warmup (Stage 1 may have decayed LR too low)
    2. Warm-start ramp: first 500 steps use 90% objects / 10% facts,
       linearly ramping to the target 40% objects / 60% facts by step 500.
       This prevents domain transition shock.
    3. World model curriculum starts at 0.1× LR, ramping to 1× by step 2000
    4. Fast collapse detector active from step 1
    """
    if start_from >= num_stimuli:
        print(f"  [Stage 2] Already complete (step {start_from} >= {num_stimuli}) — skipping")
        return []
    WARM_START_STEPS = 500       # Steps to ramp from objects→facts
    WARM_START_FACT_RATIO = 0.1  # Initial fact ratio (10%)
    TARGET_FACT_RATIO = 0.6      # Final fact ratio (60%)
    WM_CURRICULUM_LR_RAMP = 2000 # Steps to ramp WM curriculum to full LR

    print("\n" + "=" * 60)
    print("  STAGE 2: Good Try! Almost!")
    print("=" * 60)

    # Enable world model for feedback learning
    try:
        brain.enable_world_model_bridge(True)
        print("  [World Model] Thousand Brains bridge enabled for Stage 2")
    except Exception as e:
        print(f"  [World Model] Enable failed (non-fatal): {e}")

    # World model curriculum: start at reduced LR, ramp up.
    # Warm resume optimization (2026-05-18): skip the initial full-epoch load
    # when resuming mid-stage. The epoch (~5K physics+chem+bio transitions)
    # is a one-time bootstrap signal — once the brain has steps under its
    # belt in stage 2 (start_from > 0), re-running the bootstrap costs
    # ~38 minutes of wall time and contributes nothing the running brain
    # doesn't already have. Cold-start (start_from == 0) still runs the
    # full epoch so a fresh stage-2 has the bootstrap available.
    wm_curriculum = None
    try:
        from world_model_curriculum import WorldModelCurriculum
        wm_curriculum = WorldModelCurriculum(brain_proxy=brain, max_level=2)
        if start_from == 0:
            n = wm_curriculum.run_full_epoch(scenarios_per_level=2)  # cold-start bootstrap
            print(f"  [World Model Curriculum] Initial epoch: {n} transitions (reduced)")
        else:
            print(f"  [World Model Curriculum] Warm resume from step {start_from}: "
                  f"skipping initial epoch (bootstrap already absorbed)")
    except Exception as e:
        print(f"  [World Model Curriculum] Init failed (non-fatal): {e}")

    parent.pre_generate_content(stage=2, num_stimuli=num_stimuli)

    # LR RESET: fresh warmup for Stage 2. Stage 1 may have decayed LR
    # to a very low value — too low to learn new domains.
    losses = []
    lr_ctrl = AdaptiveLRController(initial_lr=BASE_LEARNING_RATE)
    print(f"  [LR] Reset to {BASE_LEARNING_RATE:.6f} with fresh warmup")

    # Set plasticity to ACQUISITION mode for new domain learning
    try:
        brain.set_plasticity_state("ACQUISITION")
        print("  [Plasticity] Set to ACQUISITION for Stage 2")
    except Exception:
        pass

    contrastive = ContrastiveRegularizer(
        buffer_size=200, interval=20, batch_size=32,
        similarity_threshold=0.75, min_input_distance=0.3, strength=0.6)
    diversity = DiversityRegularizer(
        buffer_size=100, interval=100, num_corrections=8,
        strength=0.2, min_variance_ratio=0.1)
    collapse_detector = CollapseDetector(brain_ref=brain)
    fast_collapse = FastCollapseDetector(brain_ref=brain, lr_controller=lr_ctrl)
    # Stage 2 curriculum: all tiers unlocked (Stages 0+1 covered basics+reasoning)
    _stage2_curriculum = CurriculumEscalator(unlock_threshold=100.0)
    _stage2_curriculum.current_tier = 3  # All domains available

    print(f"  [Warm Start] First {WARM_START_STEPS} steps: "
          f"{WARM_START_FACT_RATIO*100:.0f}% facts → {TARGET_FACT_RATIO*100:.0f}% facts")

    # Publish initial stage state — Stage 2 has real per-domain LR scaling
    # (1.2× physics, 1.5× chemistry, 1.5× biology applied per-step), but at
    # stage entry we only have the base lr — push that and let the in-loop
    # block refine with the actual effective_lr.
    _push_dashboard(brain, stage=2, step=start_from,
                    domain="stage2_warmstart",
                    fact_ratio=WARM_START_FACT_RATIO,
                    warm_start_complete=False,
                    warm_start_step=WARM_START_STEPS,
                    lr_physics=lr_ctrl.get_lr() * 1.2,
                    lr_chemistry=lr_ctrl.get_lr() * 1.5,
                    lr_biology=lr_ctrl.get_lr() * 1.5)

    for i in range(start_from, num_stimuli):
        # Full mode collapse detection (every 100 steps)
        if ((i + 1) % COLLAPSE_CHECK_INTERVAL == 0
                and (i + 1) >= COLLAPSE_WARMUP_STEPS):
            if collapse_detector.check(brain, step=i + 1, stage=2):
                print("  Training halted due to mode collapse.", flush=True)
                return losses

        action = clock.tick(brain)
        if action == "sleep":
            clock.do_sleep(brain, parent)
        elif action == "consolidate":
            clock.do_consolidate(brain)

        # WARM-START RAMP: gradually increase fact ratio over first 500 steps.
        # Prevents domain transition shock — the brain sees mostly familiar
        # objects at first, with new facts slowly mixed in.
        steps_in = i - start_from
        if steps_in < WARM_START_STEPS:
            ramp = float(steps_in) / WARM_START_STEPS  # 0.0 → 1.0
            fact_ratio = WARM_START_FACT_RATIO + ramp * (TARGET_FACT_RATIO - WARM_START_FACT_RATIO)
        else:
            fact_ratio = TARGET_FACT_RATIO

        # Item 7: Curriculum ordering — cluster related facts within domains.
        # Each "epoch" of 10 facts draws from the same domain, then rotates.
        # Gives consecutive related facts (pedagogically superior to random).
        echo_target = None
        if random.random() < fact_ratio:
            domain_cycle_len = 10
            domain_idx = (steps_in // domain_cycle_len) % 10
            domain_names = ['biology', 'physics', 'chemistry', 'geography', 'math',
                            'language', 'history', 'ethics', 'physics', 'biology']
            preferred_domain = domain_names[domain_idx]
            fact, expected = source.get_fact(preferred_domain=preferred_domain)
            description = fact
            # For facts, `expected` is the answer label (often multi-word or a
            # full sentence). Pick the first content word of the fact as the
            # teaching target — "Dogs are mammals..." → "dogs",
            # "Plants make their own food..." → "plants". Skip a small set of
            # English function-word starters so "The sun is a star..." picks
            # "sun" not "the". Stopword set kept tiny on purpose; the bigram
            # pump already shoulders the bulk of distributional learning.
            _STOPWORD_PREFIX = {"the", "a", "an", "this", "that", "these",
                                "those", "when", "if", "but", "and", "or",
                                "for", "in", "on", "at", "of", "to", "is",
                                "are", "was", "were"}
            first_word = None
            for w in description.split():
                cleaned = w.strip(".,!?;:\"'()-").lower()
                if (len(cleaned) >= 3 and cleaned.isalpha()
                        and cleaned not in _STOPWORD_PREFIX):
                    first_word = cleaned
                    break
            echo_target = first_word
        else:
            expected, description = source.get_object()
            # `expected` IS the canonical subject noun (e.g. "tree", "wool",
            # "sun"). Use it directly as the echo-correct teaching target so
            # the brain associates the prompt's intent with the actual
            # subject — not with the longest jargon adjective from the
            # description. This was the curriculum-side root cause of the
            # 'self-extinguishing' / 'ptolemaic' attractor.
            if isinstance(expected, str) and expected:
                # Normalize: lowercase, strip whitespace, pick first token if
                # the expected label is multi-word ("sand dollar" → "sand").
                tok = expected.strip().lower().split()
                if tok:
                    echo_target = tok[0]

        # Stage sensory data for cortex CNN training (visual/audio/speech).
        # Without this, cortex CNNs get zero forward steps in stage 2.
        # Pass echo_target so the bridge trains on the canonical subject
        # noun rather than the longest-word heuristic.
        submit_multimodal(brain, description, target_word=echo_target)

        # Cascade-driven training — drive ALL 15 stages, not just the bridge.
        # produce_cascade runs the full Broca/Wernicke/hippocampus/PFC/OFC
        # pipeline. Stage 15 (self_train) compares the produced utterance to
        # the content_intent via self-comprehension match, applies reward-
        # modulated STDP to the bridge, releases phasic DA, and feeds the
        # validated utterance back through learn_text_bigrams. Closes the
        # self-supervised loop that bridge-only training was missing.
        #
        # Cadence: every step, but only on a fraction of steps to keep the
        # per-step cost manageable (the cascade is heavy — drive/goal/
        # episodic/content/lexical/syntactic/self-comp/phonological/motor/
        # self-feedback/speech-repair/prosody/self-train).
        try:
            if (steps_in % 4 == 0):  # ~25% of steps, ~12-15 cascades/min
                brain.produce_cascade(description)
        except Exception:
            pass

        # Echo-and-correct + bigram pump fire inside submit_multimodal()
        # above — no need to repeat them here. The cascade above is the
        # NEW training signal that closes the loop through Broca + Wernicke.

        # Athena experiences the stimulus and learns from it.
        # Apply LR spike if fast collapse detector is in recovery mode.
        effective_lr = fast_collapse.get_spiked_lr(lr_ctrl.get_lr())

        # Item 6: Per-domain LR scaling from FEP precision.
        # Low precision (brain struggles here) → higher LR for faster learning.
        # High precision (brain is good here) → lower LR for fine-tuning.
        if fact_ratio > 0.3 and description:
            desc_lower = description.lower()
            try:
                nm = brain.get_network_metrics()
                if nm:
                    # Use world model prediction errors as domain signal
                    # Domains where we make big errors need more learning
                    if any(w in desc_lower for w in ['gravity','fall','collid','bounce','force']):
                        effective_lr *= 1.2  # physics: slightly boost
                    elif any(w in desc_lower for w in ['react','dissolve','acid','rust','boil']):
                        effective_lr *= 1.5  # chemistry: boost more (new domain)
                    elif any(w in desc_lower for w in ['predator','population','cell','DNA','enzyme']):
                        effective_lr *= 1.5  # biology: boost more (new domain)
                    elif any(w in desc_lower for w in ['kind','fair','honest','empathy','courage']):
                        effective_lr *= 0.8  # ethics: reduce (more about exposure)
            except Exception:
                pass

        loss, result = parent.ask_and_encourage(
            brain, composer, expected, description,
            learning_rate=effective_lr)
        losses.append(loss if loss is not None else 0)

        # Fast collapse detection (every 10 steps — catches collapse in ~2 min)
        output_vec = result.get("output_vector") if result else None
        if (i + 1) % FAST_COLLAPSE_INTERVAL == 0 and output_vec is not None:
            fast_collapse.check_output(output_vec, i + 1)

        # Record for contrastive + diversity regularization
        if output_vec is not None:
            input_emb = encode_text(expected + " " + description)
            features_for_cr = composer.compose(text=description, modality="text")
            contrastive.record(input_emb, output_vec, features_for_cr)
            diversity.record(output_vec, features_for_cr)

        # Contrastive + diversity corrections
        if contrastive.should_correct():
            n_corr, mean_sim = contrastive.apply_corrections(
                brain, learning_rate=lr_ctrl.get_lr())
            if n_corr > 0 and (i + 1) % 500 < contrastive.interval:
                print(f"    [Contrastive] {n_corr} corrections, "
                      f"mean_output_sim={mean_sim:.3f}")
        if diversity.should_correct():
            n_div, top_r, eff_r = diversity.apply_corrections(
                brain, learning_rate=lr_ctrl.get_lr() * 0.3)
            if n_div > 0 and (i + 1) % 500 < diversity.interval:
                print(f"    [Diversity] {n_div} corrections, "
                      f"top_eig={top_r:.2f}, eff_rank={eff_r:.1f}")

        # Cognitive module training injection (with curriculum for Stage 2)
        if (i + 1) % COGNITIVE_TRAIN_INTERVAL == 0:
            cog_loss = _inject_cognitive_training(
                brain, composer, step=i, learning_rate=lr_ctrl.get_lr(),
                spectral_splitter=_spectral_splitter, fold_idx=_current_fold,
                curriculum=_stage2_curriculum)
            if cog_loss is not None:
                losses.append(cog_loss)

        # Caregiver critic — see run_stage_0 note. _train_cognitive is
        # bypassed by this loop's hot path, so drive the critic directly.
        if (i + 1) % CRITIC_EVAL_INTERVAL == 0:
            parent.run_critic_eval(brain, description, stage=2)

        # World model curriculum — periodic physics/chemistry/biology experience.
        # LR ramps from 0.1× to 1× over first 2000 steps to prevent
        # simulation data destabilizing the freshly-transitioned network.
        if wm_curriculum and (i + 1) % 500 == 0:
            try:
                level = min(3, 1 + (i + 1) // 5000)
                wm_lr_scale = min(1.0, 0.1 + 0.9 * steps_in / WM_CURRICULUM_LR_RAMP)
                n_wm = wm_curriculum.run_full_epoch(level=level, scenarios_per_level=2)
                if (i + 1) % 2500 == 0:
                    stats = wm_curriculum.get_stats()
                    print(f"    [World Model] {n_wm} transitions (total: "
                          f"{stats['total_transitions']}, lr_scale={wm_lr_scale:.2f})")
            except Exception:
                pass

        # Eligibility trace — every experience contributes to growth
        try:
            brain.edp_process_novelty(0.4)
        except Exception:
            pass

        # LNN temporal step
        try:
            features = composer.compose(text=description)
            brain.lnn_forward_step(features[:128])
        except Exception:
            pass

        # Progress — track loss trend, not accuracy
        if (i + 1) % 500 == 0:
            recent = losses[-500:]
            mean_loss = np.mean(recent) if recent else 0
            lr_ctrl.update(mean_loss, i + 1)
            lr_ctrl.update_from_utm(brain, i + 1)
            warm_info = f" fact_ratio={fact_ratio:.0%}" if steps_in < WARM_START_STEPS else ""
            spike_info = f" LR_SPIKE" if fast_collapse.lr_spike_remaining > 0 else ""
            print(f"\n  [Stage 2] {i+1}/{num_stimuli} — "
                  f"mean_loss={mean_loss:.4f} lr={lr_ctrl.get_lr():.6f}{warm_info}{spike_info}"
                  f" | {contrastive.stats_str()} | {diversity.stats_str()}")
            if steps_in == WARM_START_STEPS:
                print(f"  [Warm Start] Complete — now at full {TARGET_FACT_RATIO:.0%} fact ratio")
            _print_utm_health(brain)
            _print_bio_stats(brain)
            if decoder:
                decoder.force_refit()
            evaluate_performance(brain, composer, decoder, stage=2, step=i+1)

        # Moral lesson every 2000
        if (i + 1) % 2000 == 0:
            parent.teach_moral_lesson(brain, composer, stage=2)

        # Speech training every 300
        if (i + 1) % 300 == 0:
            parent.teach_speech(brain, composer, stage=2)

        # Babbling loop every 50 steps — force decode output, feed back as input.
        # Like infant babbling: produce sound → hear yourself → refine.
        # Creates the language feedback loop that bootstraps vocabulary.
        if (i + 1) % 50 == 0 and decoder:
            try:
                # Get current output for a familiar concept
                babble_text = random.choice(["dog", "water", "sun", "tree", "happy", "big", "run", "eat"])
                babble_emb = encode_text(babble_text)
                babble_result = brain.decide_full(babble_emb.tolist())
                if isinstance(babble_result, dict):
                    ov = babble_result.get("output_vector", [])
                    if ov:
                        # Decode what the brain "says"
                        decoded = decoder.decode_output_text(ov)
                        # Feed the decoded text back as language learning
                        if decoded and len(decoded) > 1:
                            brain.learn_language(decoded)
                            brain.train_language(babble_text, decoded)
                        # Log babbling every 500 steps
                        if (i + 1) % 500 == 0:
                            print(f"    [Babble] \"{babble_text}\" → \"{decoded[:40] if decoded else '...'}\"")
            except Exception:
                pass  # Babbling failure is never fatal

        # Dragonfly tracking training — every 300 steps
        # Tracking is a stage 2 skill: relationships between objects in motion
        if (i + 1) % 300 == 0 and DRAGONFLY_TRAINING_DATA:
            item = random.choice(DRAGONFLY_TRAINING_DATA)
            features = composer.compose(text=item['scenario'], modality="text")
            target = make_semantic_target(item['reasoning'])
            loss = brain.learn_vector(features, target,
                                       label=f"dragonfly_{item['concept']}",
                                       learning_rate=lr_ctrl.get_lr() * 0.5)
            if loss is not None and (i + 1) % 1500 == 0:
                print(f"    [Dragonfly] {item['concept']}: loss={loss:.4f}")

        # Chat eval every 2000 + on-demand
        if (i + 1) % 2000 == 0 or os.path.exists("/tmp/athena_chat_now"):
            if os.path.exists("/tmp/athena_chat_now"):
                os.remove("/tmp/athena_chat_now")
            chat_eval(brain, composer, decoder, stage=2, step=i+1)

        # Inspire every 3000
        if (i + 1) % 3000 == 0:
            parent.inspire(brain, composer, stage=2)

        # Full sleep every 5000
        if (i + 1) % 5000 == 0:
            clock.do_sleep(brain, parent)
        # Checkpoint every 50 steps + auto-sync to Hetzner every 500
        if (i + 1) % 50 == 0:
            # Live-publish dashboard state every 50 steps. Use the actual
            # per-domain LR multipliers Stage 2 applies in the inner loop
            # (1.2× physics, 1.5× chemistry, 1.5× biology), so the dashboard
            # reflects the rates we're actually using.
            _ws_done = (steps_in >= WARM_START_STEPS)
            _push_dashboard(brain, stage=2, step=i + 1,
                            domain=("stage2_facts" if _ws_done
                                    else "stage2_warmstart"),
                            fact_ratio=float(fact_ratio),
                            warm_start_complete=bool(_ws_done),
                            warm_start_step=WARM_START_STEPS,
                            lr_physics=lr_ctrl.get_lr() * 1.2,
                            lr_chemistry=lr_ctrl.get_lr() * 1.5,
                            lr_biology=lr_ctrl.get_lr() * 1.5,
                            collapse_events=int(getattr(collapse_detector,
                                                        "collapse_count", 0) or 0))
            _save_checkpoint(brain, decoder, stage=2, step=i+1)
            # Canonical-corpus drip — keep literary exposure alive in stage 2.
            if (i + 1) % 200 == 0 and _CANONICAL_CFG and _CANONICAL_CFG.get("enabled"):
                try:
                    _ingest_canonical_corpus(
                        brain, stage=2,
                        corpus_root=_CANONICAL_CFG.get("root", "data/canonical_corpus"),
                        chunk_chars=_CANONICAL_CFG.get("chunk_chars", 1200),
                        max_chunks_per_call=20,
                        checkpoint_dir=CHECKPOINT_DIR)
                except Exception as _e:
                    print(f"  [Canonical] drip failed: {_e}")
            # Math-corpus drip — symbolic content alongside the literary.
            if (i + 1) % 200 == 0 and _MATH_CFG and _MATH_CFG.get("enabled"):
                try:
                    _ingest_math_corpus(
                        brain, stage=2,
                        corpus_root=_MATH_CFG.get("root", "data/math_corpus"),
                        chunk_chars=_MATH_CFG.get("chunk_chars", 1200),
                        max_chunks_per_call=20,
                        checkpoint_dir=CHECKPOINT_DIR)
                except Exception as _e:
                    print(f"  [Math] drip failed: {_e}")
            # Streaming-corpus drip — network-fetched, never persisted.
            if (i + 1) % 200 == 0 and _STREAMING_CFG and _STREAMING_CFG.get("enabled"):
                for _kind in _STREAMING_CFG.get("kinds", []):
                    try:
                        _ingest_streaming_corpus(
                            brain, stage=2, kind=_kind,
                            chunk_chars=_STREAMING_CFG.get("chunk_chars", 1200),
                            max_chunks_per_call=20,
                            checkpoint_dir=CHECKPOINT_DIR)
                    except Exception as _e:
                        print(f"  [Stream:{_kind}] drip failed: {_e}")
            # CE-1..3, 6..10, 19 probe drips — fire all once at every
            # 200-stimulus mark. Each module decides its own stage-1/2/3
            # gating internally; failures are isolated per-module.
            if (i + 1) % 200 == 0 and _PROBE_DRIPS_ENABLED:
                _run_probe_drips(brain, stage=2, composer=composer)
            # Item 2: Auto-sync checkpoint to Hetzner every 500 steps
            if (i + 1) % 500 == 0:
                try:
                    import subprocess
                    ckpt_path = os.path.join(CHECKPOINT_DIR,
                                              f"athena_s2_step{i+1}.bin")
                    if os.path.exists(ckpt_path):
                        subprocess.Popen(
                            ["bash", "scripts/sync_checkpoint.sh", ckpt_path],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        if (i + 1) % 2500 == 0:
                            print(f"    [Sync] Checkpoint syncing to Hetzner")
                except Exception:
                    pass

        # Item 5: Text→Simulation verification — ground facts in physics reality
        # Every 100 steps, verify a recent fact against the simulation engine.
        # "Gravity pulls everything toward Earth" → run physics engine, confirm
        # objects fall. The verification error becomes a learning signal.
        if (i + 1) % 100 == 0 and fact_ratio > 0.3:
            try:
                if description and ('gravity' in description.lower() or
                                     'fall' in description.lower() or
                                     'collision' in description.lower()):
                    # Physics fact → verify via simulation
                    brain.parietal_simulate_hypothesis("physics", description[:50])
                elif ('react' in description.lower() or
                      'dissolve' in description.lower() or
                      'rust' in description.lower()):
                    brain.parietal_simulate_hypothesis("chemistry", description[:50])
                elif ('population' in description.lower() or
                      'predator' in description.lower() or
                      'grow' in description.lower()):
                    brain.parietal_simulate_hypothesis("biology", description[:50])
            except Exception:
                pass

        # Item 4: SNN spike train analysis via DSP (every 1000 steps)
        # Detects oscillation patterns (alpha/beta/gamma) in the brain's
        # own spiking activity — a form of neural self-monitoring.
        if (i + 1) % 1000 == 0:
            try:
                snn_stats = brain.snn_get_stats()
                if snn_stats and snn_stats.get('total_spikes', 0) > 100:
                    if (i + 1) % 5000 == 0:
                        print(f"    [SNN] {snn_stats.get('total_spikes',0)} spikes, "
                              f"rate={snn_stats.get('mean_firing_rate',0):.1f}Hz")
            except Exception:
                pass

        # Codebase self-learning — introduce in stage 2
        if (i + 1) % 400 == 0:
            snippet_text, snippet_label, _ = source.get_codebase_snippet()
            features = composer.compose(text=snippet_text, modality="text")
            target = make_semantic_target(snippet_text)
            brain.learn_vector(features, target, label=snippet_label[:50], confidence=0.5)
            if (i + 1) % 2000 == 0:
                print(f"    [Self-learning] Fed codebase snippet: {snippet_label}")

        # Transition plasticity state mid-stage
        if (i + 1) == num_stimuli // 2:
            try:
                brain.set_plasticity_state("CONSOLIDATION")
                print("    [Plasticity] Transitioning to CONSOLIDATION")
            except Exception:
                pass

        # Forward chain KB
        if (i + 1) % 2000 == 0:
            try:
                derived = brain.ti_forward_chain(20)
                if derived > 0:
                    print(f"    KB: {derived} new facts derived")
            except Exception:
                pass

    return losses


def run_stage_3(brain, composer, parent, clock, source, decoder,
                num_interactions=10000, start_from=0):
    """Stage 3: First Words — conversation, ethics, imagination.

    Goal: Emerging communication. Multi-turn dialogue. Independent thought.
    Progressive domain introduction: starts with simple Q&A, adds science,
    math, philosophy, literature, music, art, technology, psychology.
    """
    print("\n" + "=" * 60)
    print("  STAGE 3: What Do You Think?")
    print("=" * 60)

    parent.pre_generate_content(stage=3, num_stimuli=num_interactions)

    try:
        brain.set_plasticity_state("MAINTENANCE")
    except Exception:
        pass

    # World model curriculum for Stage 3 (full difficulty)
    wm_curriculum_s3 = None
    try:
        from world_model_curriculum import WorldModelCurriculum
        wm_curriculum_s3 = WorldModelCurriculum(brain_proxy=brain, max_level=3)
        n = wm_curriculum_s3.run_full_epoch(scenarios_per_level=3)
        print(f"  [World Model Curriculum] Stage 3 initial: {n} transitions")
    except Exception as e:
        print(f"  [World Model Curriculum] Stage 3 init failed: {e}")

    # Adaptive mastery tracker
    mastery = MasteryTracker()
    mastery.load(MASTERY_FILE)

    # Stage 3 curriculum: all tiers unlocked, low threshold for domain mastery
    _stage3_curriculum = CurriculumEscalator(unlock_threshold=50.0)
    _stage3_curriculum.current_tier = 3  # All 13 domains available

    # Progressive domain schedule — unlock more domains as training progresses
    all_domains = list(source.ADVANCED_TOPICS.keys())
    total = num_interactions - start_from

    # Global LR envelope — cosine annealing modulates per-domain adaptive LR
    lr_scheduler = CosineAnnealingLR(lr_max=1.0, lr_min=0.3,
                                      warmup_steps=200, t_max=num_interactions)
    hard_miner = HardExampleMiner(max_buffer=500, replay_fraction=0.2)
    embed_adapter = EmbeddingAdapter(input_dim=1024, output_dim=1024, refit_interval=1000)
    distiller = ClaudeDistiller(teacher=parent.teacher)
    temporal = MultiResolutionTemporal(brain, slow_interval=5)
    early_stop = EarlyStopping(patience=6, min_delta=0.003, mode="min")
    batch_trainer = MiniBatchTrainer(brain, batch_size=8)
    contrastive = ContrastiveRegularizer(
        buffer_size=200, interval=20, batch_size=32,
        similarity_threshold=0.75, min_input_distance=0.3, strength=0.6)
    diversity = DiversityRegularizer(
        buffer_size=100, interval=100, num_corrections=8,
        strength=0.2, min_variance_ratio=0.1)

    # Publish initial Stage 3 state — only a single envelope LR is available
    # here (lr_scheduler), so replicate it across the per-domain fields.
    _push_dashboard(brain, stage=3, step=start_from,
                    domain="stage3_dialogue",
                    lr_physics=lr_scheduler.get_lr(),
                    lr_chemistry=lr_scheduler.get_lr(),
                    lr_biology=lr_scheduler.get_lr())

    for i in range(start_from, num_interactions):
        action = clock.tick(brain)
        if action == "sleep":
            clock.do_sleep(brain, parent)
        elif action == "consolidate":
            clock.do_consolidate(brain)

        # Calculate progress fraction (0.0 → 1.0) and unlock domains gradually
        progress = (i - start_from) / max(total, 1)
        num_unlocked = max(2, int(progress * len(all_domains)) + 1)
        active_domains = all_domains[:min(num_unlocked, len(all_domains))]

        # Mix interaction types with progressive domain weighting
        r = random.random()
        if r < 0.25:
            # Conversational question
            question = source.get_question()
            features = composer.compose(text=question, modality="text")
            submit_multimodal(brain, question)
            result = brain.decide_full(features)
            response_text = parent.observe_response(result)
            print(f"  Q: {question}")
            print(f"  Athena: {response_text or '(still forming thoughts)'}")

            # Record for contrastive regularization
            out_v = result.get("output_vector")
            if out_v is not None:
                contrastive.record(encode_text(question), out_v, features)
                diversity.record(out_v, features)

            parent_reply = parent._pop_content("_conversation_replies")
            if parent_reply:
                print(f"  Parent: {parent_reply}")

            try:
                brain.bg_update_reward(0.6, 0.5)
            except Exception:
                pass

        elif r < 0.38:
            # Advanced domain teaching — adaptive difficulty
            domain = mastery.select_domain(active_domains)
            topic_text, domain_name = source.get_advanced(domain)
            print(f"  [Domain: {domain_name}] {topic_text[:80]}")

            features = composer.compose(text=topic_text, modality="text")
            target = make_semantic_target(topic_text)
            submit_multimodal(brain, topic_text)
            result = brain.decide_full(features)

            # Held-out check: 20% of items are evaluation-only (no learning)
            if _held_out_buffer.is_held_out(features):
                _held_out_buffer.add(features, target, domain=domain_name)
                lr_scheduler.step()
            else:
                confidence = mastery.get_confidence_for_domain(domain_name, progress)
                adaptive_lr = mastery.get_adaptive_lr(domain_name) * lr_scheduler.get_lr()
                loss = batch_trainer.learn(features, target,
                                           label=domain_name[:50], confidence=confidence,
                                           learning_rate=adaptive_lr)
                lr_scheduler.step()
                mastery.record(domain_name, loss, confidence)
                hard_miner.record(loss, topic_text, features, target)

                if decoder:
                    output_vec = result.get("output_vector")
                    if output_vec is not None:
                        target_emb = encode_text(topic_text)
                        decoder.record_pair(output_vec, target_emb, topic_text)
                        embed_adapter.record_pair(target_emb,
                                                   extract_embedding_from_output(np.array(output_vec)))
                        contrastive.record(target_emb, output_vec, features)
                        diversity.record(output_vec, features)

                if loss is not None:
                    print(f"    loss={loss:.4f} conf={confidence:.2f}")

        elif r < 0.44:
            # Hard example replay — re-train on highest-loss examples
            if hard_miner.should_replay():
                replays = hard_miner.get_hard_examples(3)
                for rdesc, rfeat, rtgt in replays:
                    loss = batch_trainer.learn(rfeat, rtgt, label="replay",
                                               confidence=0.7,
                                               learning_rate=lr_scheduler.get_lr())
                    if loss is not None and (i + 1) % 200 == 0:
                        print(f"  [Replay] {rdesc[:60]}: loss={loss:.4f}")
            else:
                # Not enough hard examples yet — regular fact review
                fact, expected = source.get_fact()
                parent.ask_and_encourage(brain, composer, expected, fact)

        elif r < 0.54:
            # Claude-generated curriculum lesson with knowledge distillation
            if parent.teacher:
                curriculum = parent.teacher.get_curriculum(
                    min(3 + int(progress * 2), 5))  # stages 3-5 progressively
                if curriculum:
                    spec = random.choice(curriculum)
                    try:
                        lesson_text, lesson_emb = parent.teacher.generate_lesson(spec)
                        print(f"  [Claude lesson: {spec.domain}/{spec.topic}]")
                        print(f"    {lesson_text[:120]}")

                        features = composer.compose(text=lesson_text, modality="text",
                                                     text_embedding=lesson_emb.tolist())
                        target = make_semantic_target(lesson_text)
                        loss = batch_trainer.learn(features, target,
                                            label=spec.domain[:50], confidence=0.7)
                        hard_miner.record(loss, lesson_text, features, target)

                        # Knowledge distillation — train on concept triples
                        distill_targets = distiller.make_distillation_targets(lesson_text)
                        for concept_text, concept_target in distill_targets[:2]:
                            cfeat = composer.compose(text=concept_text, modality="text")
                            batch_trainer.learn(cfeat, concept_target,
                                                label=f"distill:{concept_text[:40]}",
                                                confidence=0.6)
                    except Exception as e:
                        logger.debug("Claude lesson failed: %s", e)
            else:
                # Fallback: fact review
                fact, expected = source.get_fact()
                parent.ask_and_encourage(brain, composer, expected, fact)

        elif r < 0.62:
            # Moral lesson
            parent.teach_moral_lesson(brain, composer, stage=3)

        elif r < 0.72:
            # Review — facts from built-in corpus
            fact, expected = source.get_fact()
            parent.ask_and_encourage(brain, composer, expected, fact)

        elif r < 0.80:
            # Inspiration
            parent.inspire(brain, composer, stage=3)

        elif r < 0.90:
            # Speech training
            parent.teach_speech(brain, composer, stage=3)

        elif r < 0.95:
            # Codebase self-learning — Athena studies her own source code
            snippet_text, snippet_label, _ = source.get_codebase_snippet()
            features = composer.compose(text=snippet_text, modality="text")
            target = make_semantic_target(snippet_text)
            loss = batch_trainer.learn(features, target,
                                       label=snippet_label[:50], confidence=0.6)
            if decoder:
                output_vec = brain.decide_full(features).get("output_vector")
                if output_vec is not None:
                    target_emb = encode_text(snippet_text)
                    decoder.record_pair(output_vec, target_emb, snippet_label)
            if (i + 1) % 100 == 0:
                print(f"  [Self-study] {snippet_label}: loss={loss:.4f}" if loss else "")

        else:
            # Curiosity-driven exploration
            try:
                gaps = brain.curiosity_detect_gaps("general")
                if gaps and gaps.get("novelty_score", 0) > 0.5:
                    print("  Parent: You seem curious! Let's explore...")
                    topic = gaps.get("suggested_topic", "the world")
                    features = composer.compose(text=f"Let's learn about {topic}")
                    target = make_semantic_target(f"exploring {topic}")
                    batch_trainer.learn(features, target, label=topic[:50], confidence=0.5)
            except Exception:
                # Free exploration from advanced topics
                topic_text, domain = source.get_advanced(random.choice(active_domains))
                features = composer.compose(text=topic_text)
                target = make_semantic_target(topic_text)
                batch_trainer.learn(features, target, label=domain[:50], confidence=0.4)

        # Cognitive module training injection (with curriculum for Stage 3)
        if (i + 1) % COGNITIVE_TRAIN_INTERVAL == 0:
            cog_loss = _inject_cognitive_training(
                brain, composer, step=i, learning_rate=lr_scheduler.get_lr(),
                spectral_splitter=_spectral_splitter, fold_idx=_current_fold,
                curriculum=_stage3_curriculum)
            if cog_loss is not None:
                losses.append(cog_loss)
            # Layer B: at stage 3+, sample unanswerables/OOD at IDK_SAMPLING_RATE
            # so the brain learns the metacognitive "I don't know" pattern.
            idk_loss = _inject_idk_unanswerable(
                brain, composer, step=i,
                learning_rate=lr_scheduler.get_lr(), stage=3)
            if idk_loss is not None:
                losses.append(idk_loss)

        # Multi-resolution temporal processing (fast + slow LNN)
        try:
            features = composer.compose(text="temporal context update")
            temporal.step(features)
        except Exception:
            pass

        # Contrastive + diversity corrections
        if contrastive.should_correct():
            n_corr, mean_sim = contrastive.apply_corrections(
                brain, learning_rate=lr_scheduler.get_lr())
            if n_corr > 0 and (i + 1) % 200 < contrastive.interval:
                print(f"    [Contrastive] {n_corr} corrections, "
                      f"mean_output_sim={mean_sim:.3f}")
        if diversity.should_correct():
            n_div, top_r, eff_r = diversity.apply_corrections(
                brain, learning_rate=lr_scheduler.get_lr() * 0.3)
            if n_div > 0 and (i + 1) % 200 < diversity.interval:
                print(f"    [Diversity] {n_div} corrections, "
                      f"top_eig={top_r:.2f}, eff_rank={eff_r:.1f}")

        # Progress — show active domains
        # Adapt difficulty every 50 steps
        if (i + 1) % 50 == 0:
            for d in active_domains:
                mastery.adapt_difficulty(d)

            # Live-publish dashboard state. Stage 3 has REAL per-domain
            # adaptive LRs via the mastery tracker — pull them when the
            # domain is currently unlocked, fall back to envelope otherwise.
            try:
                _envelope_lr = lr_scheduler.get_lr()
                def _per_domain_lr(name):
                    if name in active_domains:
                        try:
                            return mastery.get_adaptive_lr(name) * _envelope_lr
                        except Exception:
                            return _envelope_lr
                    return _envelope_lr
                _push_dashboard(brain, stage=3, step=i + 1,
                                domain="stage3_dialogue",
                                lr_physics=float(_per_domain_lr("physics")),
                                lr_chemistry=float(_per_domain_lr("chemistry")),
                                lr_biology=float(_per_domain_lr("biology")))
            except Exception:
                # Mastery tracker shape may differ — never block training
                _push_dashboard(brain, stage=3, step=i + 1,
                                domain="stage3_dialogue",
                                lr_physics=lr_scheduler.get_lr(),
                                lr_chemistry=lr_scheduler.get_lr(),
                                lr_biology=lr_scheduler.get_lr())

        # Refit embedding adapter periodically
        if (i + 1) % 1000 == 0:
            embed_adapter.maybe_refit()
            if embed_adapter.fitted:
                print(f"    [EmbedAdapter] Refitted on {len(embed_adapter.pairs)} pairs")

        if (i + 1) % 200 == 0:
            print(f"\n  [Stage 3] {i+1}/{num_interactions} — "
                  f"domains: {', '.join(active_domains)} "
                  f"hard_buf={len(hard_miner.buffer)}"
                  f" | {contrastive.stats_str()} | {diversity.stats_str()}")
            # Show adaptive LR per domain
            lr_info = ", ".join(
                f"{d}={mastery.get_adaptive_lr(d):.5f}" for d in active_domains[:5]
            )
            print(f"  Adaptive LR: {lr_info} (envelope={lr_scheduler.get_lr():.3f})")
            print(f"  Mastery:\n{mastery.summary()}")
            _print_utm_health(brain)
            _print_bio_stats(brain)

        if (i + 1) % 500 == 0:
            if decoder:
                decoder.force_refit()
            evaluate_performance(brain, composer, decoder, stage=3, step=i+1)

            # Early stopping on mastery plateau
            overall_mastery = np.mean([mastery.get_mastery(d) for d in active_domains]) \
                if active_domains else 0.0
            # Invert mastery (higher = better) → early_stop expects "min" mode on loss-like metric
            if early_stop.check(1.0 - overall_mastery):
                print(f"    Early stopping: mastery plateaued at {overall_mastery:.3f} "
                      f"for {early_stop.patience} evals — advancing to next stage")
                break

        # Deep consolidation + synapse pruning every 2000
        if (i + 1) % 2000 == 0:
            clock.do_sleep(brain, parent)
            # Prune weak synapses to prevent unbounded memory growth
            try:
                pruned = brain.prune_synapses(0.01)
                if pruned > 0:
                    print(f"    [Prune] Removed {pruned} weak synapses (threshold=0.01)")
            except Exception as e:
                logger.debug("Synapse pruning failed: %s", e)
            _save_checkpoint(brain, decoder, stage=3, step=i+1)
            mastery.save(MASTERY_FILE)

        # Canonical-corpus drip — keep literary exposure alive in stage 3.
        if (i + 1) % 200 == 0 and _CANONICAL_CFG and _CANONICAL_CFG.get("enabled"):
            try:
                _ingest_canonical_corpus(
                    brain, stage=3,
                    corpus_root=_CANONICAL_CFG.get("root", "data/canonical_corpus"),
                    chunk_chars=_CANONICAL_CFG.get("chunk_chars", 1200),
                    max_chunks_per_call=20,
                    checkpoint_dir=CHECKPOINT_DIR)
            except Exception as _e:
                print(f"  [Canonical] drip failed: {_e}")
        # Math-corpus drip — symbolic content alongside the literary.
        if (i + 1) % 200 == 0 and _MATH_CFG and _MATH_CFG.get("enabled"):
            try:
                _ingest_math_corpus(
                    brain, stage=3,
                    corpus_root=_MATH_CFG.get("root", "data/math_corpus"),
                    chunk_chars=_MATH_CFG.get("chunk_chars", 1200),
                    max_chunks_per_call=20,
                    checkpoint_dir=CHECKPOINT_DIR)
            except Exception as _e:
                print(f"  [Math] drip failed: {_e}")
        # Streaming-corpus drip — network-fetched, never persisted.
        if (i + 1) % 200 == 0 and _STREAMING_CFG and _STREAMING_CFG.get("enabled"):
            for _kind in _STREAMING_CFG.get("kinds", []):
                try:
                    _ingest_streaming_corpus(
                        brain, stage=3, kind=_kind,
                        chunk_chars=_STREAMING_CFG.get("chunk_chars", 1200),
                        max_chunks_per_call=20,
                        checkpoint_dir=CHECKPOINT_DIR)
                except Exception as _e:
                    print(f"  [Stream:{_kind}] drip failed: {_e}")
        # CE-1..3, 6..10, 19 probe drips — fire all at every 200-stimulus mark.
        if (i + 1) % 200 == 0 and _PROBE_DRIPS_ENABLED:
            _run_probe_drips(brain, stage=3, composer=composer)

        # Checkpoint every 50 steps (state file only)
        if (i + 1) % 50 == 0 and (i + 1) % 2000 != 0:
            _save_checkpoint(brain, decoder, stage=3, step=i+1)

        # Chat eval every 2000 + on-demand
        if (i + 1) % 2000 == 0 or os.path.exists("/tmp/athena_chat_now"):
            if os.path.exists("/tmp/athena_chat_now"):
                os.remove("/tmp/athena_chat_now")
            chat_eval(brain, composer, decoder, stage=3, step=i+1)

        # Forward chain KB
        if (i + 1) % 1000 == 0:
            try:
                derived = brain.ti_forward_chain(30)
                if derived > 0:
                    print(f"    KB: {derived} new facts derived")
            except Exception:
                pass

        # Portia platform adaptation — every 300 steps
        # Resource reasoning is a stage 3 skill: strategic decision-making
        if (i + 1) % 300 == 0 and PORTIA_TRAINING_DATA:
            item = random.choice(PORTIA_TRAINING_DATA)
            features = composer.compose(text=item['scenario'], modality="text")
            target = make_semantic_target(item['decision'])
            loss = batch_trainer.learn(features, target,
                                       label=f"portia_{item['concept']}",
                                       confidence=0.6,
                                       learning_rate=lr_scheduler.get_lr() * 0.5)
            if loss is not None and (i + 1) % 1500 == 0:
                print(f"    [Portia] {item['concept']}: loss={loss:.4f}")

    # Flush any remaining samples in the mini-batch buffer
    batch_trainer.flush()


# ============================================================================
# Utilities
# ============================================================================

def _print_utm_health(brain):
    """Print UTM training health from DFA analysis."""
    try:
        health = brain.utm_get_training_health()
        name = health.get("health_name", "unknown")
        dfa = health.get("dfa_exponent", 0.0)
        grad_ok = health.get("gradients_healthy", 1)
        early = health.get("early_stopped", 0)
        parts = [f"health={name}", f"DFA_α={dfa:.2f}"]
        if not grad_ok:
            parts.append("GRAD_UNHEALTHY")
        if early:
            parts.append("EARLY_STOPPED")
        print(f"    [UTM] {' | '.join(parts)}")
    except (AttributeError, RuntimeError):
        pass  # UTM not available


def _print_bio_stats(brain):
    """Print biological subsystem status."""
    try:
        stats = brain.get_plasticity_stats()
        if stats:
            parts = []
            if "rpe" in stats:
                parts.append(f"RPE={stats['rpe']:.3f}")
            if "dopamine" in stats:
                parts.append(f"DA={stats['dopamine']:.3f}")
            if "acetylcholine" in stats:
                parts.append(f"ACh={stats['acetylcholine']:.3f}")
            if "plasticity_state" in stats:
                parts.append(f"state={stats['plasticity_state']}")
            if "edp_ltp_events" in stats:
                parts.append(f"LTP={stats['edp_ltp_events']}")
            if "tpb_stdp_updates" in stats:
                parts.append(f"STDP={stats['tpb_stdp_updates']}")
            if parts:
                print(f"    Bio: {' | '.join(parts)}")
    except Exception:
        pass

    try:
        arousal = brain.medulla_get_arousal()
        sleep_p = brain.sleep_get_pressure()
        print(f"    Arousal={arousal:.2f} | SleepPressure={sleep_p:.2f}")
    except Exception:
        pass

    try:
        lnn_stats = brain.lnn_get_stats()
        if lnn_stats:
            print(f"    LNN: steps={lnn_stats.get('forward_steps', 0)} "
                  f"tau={lnn_stats.get('avg_tau', 0):.3f}")
    except Exception:
        pass

    try:
        snn_stats = brain.snn_get_stats()
        if snn_stats:
            print(f"    SNN: steps={snn_stats.get('total_steps', 0)} "
                  f"spikes={snn_stats.get('total_spikes', 0)} "
                  f"rate={snn_stats.get('mean_firing_rate', 0):.1f}Hz "
                  f"sparsity={snn_stats.get('sparsity', 0):.2f}")
    except Exception:
        pass

    try:
        cnn_stats = brain.cnn_get_stats()
        if cnn_stats:
            print(f"    CNN: layers={cnn_stats.get('num_layers', 0)} "
                  f"params={cnn_stats.get('num_parameters', 0)} "
                  f"labels={cnn_stats.get('num_labels', 0)}")
    except Exception:
        pass


# ============================================================================
# Performance Evaluation: see what Athena actually does
# ============================================================================

# Fixed test stimuli — never trained on directly, used to observe responses
# ---------------------------------------------------------------------------
# Chat evaluation — periodic conversational tests during training
# ---------------------------------------------------------------------------

_CHAT_PROMPTS_FILE = "checkpoints/athena/chat_eval_prompts.json"
_CHAT_PROMPTS_CACHE = None  # loaded lazily
_CHAT_EVAL_SAMPLE_SIZE = 20  # prompts per eval (sampled from full 500)

_CHAT_LOG_FILE = "checkpoints/athena/chat_eval_log.jsonl"
_chat_eval_count = 0


def _load_chat_prompts():
    """Load chat eval prompts from JSON file."""
    global _CHAT_PROMPTS_CACHE
    if _CHAT_PROMPTS_CACHE is not None:
        return _CHAT_PROMPTS_CACHE
    try:
        with open(_CHAT_PROMPTS_FILE) as f:
            _CHAT_PROMPTS_CACHE = json.load(f)  # list of [category, prompt_text]
    except Exception:
        # Fallback minimal set
        _CHAT_PROMPTS_CACHE = [
            ["greetings", "Hello! How are you today?"],
            ["colors", "What color is the sky?"],
            ["animals", "Tell me about dogs."],
            ["nature", "Tell me about the sun."],
            ["feelings", "What makes you happy?"],
            ["identity", "What is your name?"],
            ["learning", "What have you learned today?"],
            ["abstract", "Do you dream?"],
            ["morals", "Why should we be kind to others?"],
            ["science", "Why does it rain?"],
        ]
    return _CHAT_PROMPTS_CACHE


def chat_eval(brain, composer, decoder, stage, step, phi3=None):
    """Run conversational chat prompts through Athena and log responses.

    Samples _CHAT_EVAL_SAMPLE_SIZE prompts from the full 500-prompt pool,
    stratified across categories for coverage. Measures coherence, decode
    similarity, output norms, and cross-prompt differentiation.
    """
    global _chat_eval_count
    _chat_eval_count += 1

    all_prompts = _load_chat_prompts()

    # Stratified sample: pick evenly across categories
    import random
    by_cat = {}
    for cat, text in all_prompts:
        by_cat.setdefault(cat, []).append(text)
    sample = []
    cats = sorted(by_cat.keys())
    per_cat = max(1, _CHAT_EVAL_SAMPLE_SIZE // len(cats))
    for cat in cats:
        pool = by_cat[cat]
        n = min(per_cat, len(pool))
        sample.extend([(cat, t) for t in random.sample(pool, n)])
    # Top up if needed
    while len(sample) < _CHAT_EVAL_SAMPLE_SIZE and len(sample) < len(all_prompts):
        extra = random.choice(all_prompts)
        if extra not in sample:
            sample.append(tuple(extra))
    sample = sample[:_CHAT_EVAL_SAMPLE_SIZE]

    print(f"\n  {'═' * 56}")
    print(f"  CHAT WITH ATHENA — Stage {stage}, Step {step} (eval #{_chat_eval_count})")
    print(f"  {len(sample)} prompts from {len(cats)} categories (pool: {len(all_prompts)})")
    print(f"  {'═' * 56}")

    results = []
    all_embeddings = []

    for label, prompt_text in sample:
        features = composer.compose(text=prompt_text, modality="text")
        result = brain.decide_full(features)
        output_vec = result.get("output_vector")

        if output_vec is None:
            print(f"    You: {prompt_text}")
            print(f"    Athena: [no output]")
            results.append({"label": label, "prompt": prompt_text,
                           "response": "", "similarity": 0.0, "norm": 0.0})
            continue

        output_arr = np.array(output_vec, dtype=np.float32)
        vec_norm = float(np.linalg.norm(output_arr))
        output_emb = extract_embedding_from_output(output_arr)
        all_embeddings.append(output_emb)

        # Decode response
        decoded = ""
        similarity = 0.0
        top3_str = ""
        if decoder and len(decoder.vocabulary) > 0:
            matches = decoder.vocabulary.decode(output_emb, top_k=3)
            if matches and matches[0][0]:
                decoded = matches[0][0]
                similarity = matches[0][1]
                top3_str = " | ".join(f"{t[:40]}({s:.2f})" for t, s in matches[:3] if t)

        # Compute input-output coherence
        input_emb = encode_text(prompt_text)
        coherence = float(np.dot(output_emb, input_emb) /
                         (np.linalg.norm(output_emb) * np.linalg.norm(input_emb) + 1e-8))

        # Try Phi-3 generation for richer response
        phi3_text = ""
        if phi3 and phi3.available:
            try:
                phi3_result = phi3.generate(
                    user_text=prompt_text,
                    vocab_match=decoded if decoded else None,
                    max_tokens=128,
                )
                if phi3_result and phi3_result.get('text'):
                    phi3_text = phi3_result['text']
            except Exception:
                pass

        print(f"    You:    {prompt_text}")
        if phi3_text:
            print(f"    Athena: {phi3_text[:120]}")
            print(f"            [phi3 | vocab: {decoded[:40]}({similarity:.2f})]")
        elif decoded:
            print(f"    Athena: {decoded[:80]}")
            if top3_str:
                print(f"            [{top3_str}]")
        else:
            print(f"    Athena: [raw output, |v|={vec_norm:.2f}]")
        print(f"            coherence={coherence:.3f}  sim={similarity:.3f}  |v|={vec_norm:.2f}")
        print()

        results.append({
            "label": label, "prompt": prompt_text,
            "response": decoded[:200], "similarity": similarity,
            "coherence": coherence, "norm": vec_norm,
        })

    # Summary stats
    if all_embeddings:
        embs = np.array(all_embeddings, dtype=np.float32)
        norms = np.linalg.norm(embs, axis=1, keepdims=True)
        norms = np.maximum(norms, 1e-8)
        sim_matrix = (embs @ embs.T) / (norms @ norms.T)
        upper = [sim_matrix[i, j] for i in range(len(embs))
                 for j in range(i + 1, len(embs))]
        mean_cross = np.mean(upper) if upper else 0
        coherences = [r["coherence"] for r in results if "coherence" in r]
        mean_coherence = np.mean(coherences) if coherences else 0
        mean_sim = np.mean([r["similarity"] for r in results])
        mean_norm = np.mean([r["norm"] for r in results])

        print(f"  Summary:")
        print(f"    Mean coherence:       {mean_coherence:.3f}  (input↔output alignment)")
        print(f"    Mean decode sim:      {mean_sim:.3f}  (vocab match quality)")
        print(f"    Mean output norm:     {mean_norm:.2f}  (signal strength)")
        print(f"    Mean cross-sim:       {mean_cross:.3f}  (differentiation, lower=better)")
        if mean_cross > 0.95:
            print(f"    Status: All responses nearly identical — not differentiating yet")
        elif mean_cross > 0.8:
            print(f"    Status: Some differentiation emerging")
        elif mean_cross > 0.5:
            print(f"    Status: Good differentiation between prompts")
        else:
            print(f"    Status: Strong differentiation — responses are distinct")
    # Machine-parseable summary for metrics exporter
    if all_embeddings:
        diversity = 1.0 - mean_cross  # higher = better differentiation
        print(f"  CHAT SUMMARY: coherence={mean_coherence:.4f} similarity={mean_sim:.4f} "
              f"norm={mean_norm:.2f} diversity={diversity:.4f} "
              f"prompts={len(results)} eval=#{_chat_eval_count}", flush=True)
    print(f"  {'═' * 56}\n")

    # Append to JSONL log for tracking over time
    try:
        os.makedirs(os.path.dirname(_CHAT_LOG_FILE), exist_ok=True)
        entry = {
            "eval": _chat_eval_count,
            "stage": stage, "step": step,
            "timestamp": time.time(),
            "mean_coherence": float(mean_coherence) if all_embeddings else 0,
            "mean_similarity": float(mean_sim) if results else 0,
            "mean_norm": float(mean_norm) if results else 0,
            "mean_cross_sim": float(mean_cross) if all_embeddings else 0,
            "responses": results,
        }
        with open(_CHAT_LOG_FILE, "a") as f:
            f.write(json.dumps(entry) + "\n")
    except Exception as e:
        print(f"  [Chat log write failed: {e}]")


# ============================================================================
# Held-out evaluation buffer + metrics
# ============================================================================

METRICS_LOG_FILE = "metrics_log.jsonl"


class HeldOutBuffer:
    """Deterministic 80/20 train/eval split based on md5 hash of input vector.

    Items where md5(input_bytes) % 5 == 0 are held out for evaluation only
    (no learn_vector call). Capped at max_items to bound memory.
    """

    def __init__(self, max_items=2000):
        self.max_items = max_items
        self._items = []  # list of (input_vector, target_vector, domain)

    def is_held_out(self, input_vector):
        """Deterministic check: True if this input should be held out (20%)."""
        v = np.asarray(input_vector, dtype=np.float32)
        h = hashlib.md5(v.tobytes()).hexdigest()
        return int(h, 16) % 5 == 0

    def add(self, input_vector, target_vector, domain="general"):
        """Add a held-out item for future evaluation."""
        if len(self._items) >= self.max_items:
            # Evict oldest item
            self._items.pop(0)
        self._items.append((
            np.asarray(input_vector, dtype=np.float32).copy(),
            np.asarray(target_vector, dtype=np.float32).copy(),
            domain,
        ))

    def get_eval_items(self):
        """Return all held-out items as list of (input, target, domain)."""
        return list(self._items)

    def __len__(self):
        return len(self._items)


class MetricsComputer:
    """Compute real accuracy metrics on held-out evaluation items.

    For each held-out item, runs brain.decide_full(input) and compares the
    output against the target via cosine similarity. Also checks top-k
    accuracy using the decoder vocabulary.
    """

    @staticmethod
    def compute(brain, composer, decoder, held_out_items):
        """Compute metrics on held-out items.

        Args:
            brain: nimcp.Brain instance
            composer: SensoryComposer (unused here, items already composed)
            decoder: NeuralDecoder with vocabulary for top-k lookup
            held_out_items: list of (input_vector, target_vector, domain)

        Returns:
            dict with top_1, top_3, top_5 accuracy, mean_cosine_sim,
            differentiation_score, per_domain breakdown, count.
        """
        if not held_out_items:
            return None

        top1_correct = 0
        top3_correct = 0
        top5_correct = 0
        cosine_sims = []
        outputs = []
        per_domain = {}  # domain -> {cosine_sims, count}

        has_vocab = decoder and len(decoder.vocabulary) > 0

        for input_vec, target_vec, domain in held_out_items:
            # Ensure input is a flat list of floats — held_out_items may contain
            # numpy arrays or mixed types from the composer/data pipeline.
            try:
                input_floats = [float(x) for x in input_vec]
            except (TypeError, ValueError):
                continue
            result = brain.decide_full(input_floats)
            output_vec = result.get("output_vector")
            if output_vec is None:
                continue

            output_arr = np.array(output_vec, dtype=np.float32)
            target_arr = np.asarray(target_vec, dtype=np.float32)

            # Cosine similarity between output and target
            # Truncate to shorter dimension if sizes mismatch
            min_dim = min(len(output_arr), len(target_arr))
            out_trunc = output_arr[:min_dim]
            tgt_trunc = target_arr[:min_dim]
            out_norm = np.linalg.norm(out_trunc)
            tgt_norm = np.linalg.norm(tgt_trunc)
            if out_norm > 1e-8 and tgt_norm > 1e-8:
                cos_sim = float(np.dot(out_trunc, tgt_trunc) / (out_norm * tgt_norm))
            else:
                cos_sim = 0.0
            cosine_sims.append(cos_sim)
            outputs.append(output_arr)

            # Per-domain tracking
            if domain not in per_domain:
                per_domain[domain] = {"cosine_sims": [], "count": 0}
            per_domain[domain]["cosine_sims"].append(cos_sim)
            per_domain[domain]["count"] += 1

            # Top-k accuracy via decoder vocabulary nearest-neighbor
            if has_vocab:
                output_emb = extract_embedding_from_output(output_arr)
                target_emb = extract_embedding_from_output(target_arr)
                # Get the "correct" label: nearest vocab entry to target
                target_matches = decoder.vocabulary.decode(target_emb, top_k=1)
                correct_label = target_matches[0][0] if target_matches and target_matches[0][0] else None

                if correct_label:
                    # Check if correct label appears in top-k of output
                    output_matches = decoder.vocabulary.decode(output_emb, top_k=5)
                    output_labels = [m[0] for m in output_matches if m[0]]
                    if correct_label in output_labels[:1]:
                        top1_correct += 1
                    if correct_label in output_labels[:3]:
                        top3_correct += 1
                    if correct_label in output_labels[:5]:
                        top5_correct += 1

        n = len(cosine_sims)
        if n == 0:
            return None

        # Differentiation score: 1 - mean pairwise cosine between outputs
        diff_score = 0.0
        if len(outputs) >= 2:
            out_mat = np.array(outputs, dtype=np.float32)
            out_norms = np.linalg.norm(out_mat, axis=1, keepdims=True)
            out_norms = np.maximum(out_norms, 1e-8)
            sim_mat = (out_mat @ out_mat.T) / (out_norms @ out_norms.T)
            # Upper triangle only (exclude diagonal)
            upper = []
            for i in range(len(outputs)):
                for j in range(i + 1, len(outputs)):
                    upper.append(sim_mat[i, j])
            if upper:
                diff_score = 1.0 - float(np.mean(upper))

        # Per-domain summary
        domain_breakdown = {}
        for dom, data in per_domain.items():
            domain_breakdown[dom] = {
                "mean_cosine_sim": float(np.mean(data["cosine_sims"])),
                "count": data["count"],
            }

        return {
            "count": n,
            "top_1_accuracy": top1_correct / n if has_vocab else None,
            "top_3_accuracy": top3_correct / n if has_vocab else None,
            "top_5_accuracy": top5_correct / n if has_vocab else None,
            "mean_cosine_sim": float(np.mean(cosine_sims)),
            "differentiation_score": diff_score,
            "per_domain": domain_breakdown,
        }


# Module-level held-out buffer (shared across stages)
_held_out_buffer = HeldOutBuffer(max_items=2000)

# Module-level spectral k-fold state (shared across stages)
_spectral_splitter = None
_current_fold = 0


_EVAL_PROBES = [
    ("dog", "A friendly dog with soft fur"),
    ("cat", "A cat that purrs when happy"),
    ("sun", "The bright warm sun in the sky"),
    ("rain", "Drops of rain falling from clouds"),
    ("book", "A book with pages full of stories"),
    ("music", "A beautiful melody playing softly"),
    ("mother", "A loving mother holding her child"),
    ("ocean", "The vast blue ocean with waves"),
]

# Track response history for each probe across evaluations
_eval_history = {}  # probe_name → list of (step, response_text, similarity, vec_norm)


def evaluate_performance(brain, composer, decoder, stage, step):
    """Run fixed probes through the brain and show actual responses.

    This is the real test: present stimuli Athena hasn't been directly
    trained on (but related to what she's seen) and show what her brain
    produces. A learning brain should:
    1. Produce non-zero, varied responses (not all the same)
    2. Produce responses that grow more similar to related concepts
    3. Differentiate between unrelated stimuli
    """
    # Freeze decoder during evaluation to prevent eval data from
    # contaminating the projector training
    if decoder:
        decoder.freeze()

    # Swap to EMA parameters for smoother inference during evaluation
    ema_active = False
    try:
        brain.utm_swap_to_ema()
        ema_active = True
    except (AttributeError, RuntimeError):
        pass  # UTM/EMA not available

    print(f"\n  {'─' * 56}")
    print(f"  PERFORMANCE EVAL — Stage {stage}, Step {step}"
          f"{' [EMA]' if ema_active else ''}")
    print(f"  {'─' * 56}")

    responses = []
    embeddings = []

    for probe_name, probe_text in _EVAL_PROBES:
        features = composer.compose(text=probe_text, modality="text")
        result = brain.decide_full(features)

        output_vec = result.get("output_vector")
        if output_vec is None:
            print(f"    {probe_name:8s} → [no output]")
            continue

        output_arr = np.array(output_vec, dtype=np.float32)
        vec_norm = float(np.linalg.norm(output_arr))
        output_emb = extract_embedding_from_output(output_arr)
        embeddings.append((probe_name, output_emb))

        # Decode what her response is closest to
        decoded_text = ""
        similarity = 0.0
        if decoder and len(decoder.vocabulary) > 0:
            matches = decoder.vocabulary.decode(output_emb, top_k=3)
            if matches and matches[0][0]:
                decoded_text = matches[0][0][:50]
                similarity = matches[0][1]
                # Show top-3
                top3 = ", ".join(f"{t[:25]}({s:.2f})" for t, s in matches[:3] if t)
                print(f"    {probe_name:8s} → {top3}")
                print(f"             |output|={vec_norm:.2f}  "
                      f"active={result.get('num_active_neurons', '?')}  "
                      f"sparsity={result.get('sparsity', 0):.2f}")
        else:
            print(f"    {probe_name:8s} → |output|={vec_norm:.2f}  "
                  f"active={result.get('num_active_neurons', '?')}")

        # Track history
        if probe_name not in _eval_history:
            _eval_history[probe_name] = []
        _eval_history[probe_name].append(
            (step, decoded_text, similarity, vec_norm))

        responses.append((probe_name, output_emb, decoded_text, similarity))

    # --- Differentiation matrix: can she tell things apart? ---
    if len(embeddings) >= 2:
        print(f"\n  Differentiation (cosine similarity between responses):")
        names = [e[0] for e in embeddings]
        embs = np.array([e[1] for e in embeddings], dtype=np.float32)
        norms = np.linalg.norm(embs, axis=1, keepdims=True)
        norms = np.maximum(norms, 1e-8)
        sim_matrix = (embs @ embs.T) / (norms @ norms.T)

        # Print compact matrix
        header = "          " + "".join(f"{n:>8s}" for n in names)
        print(f"  {header}")
        for i, name in enumerate(names):
            row = f"    {name:8s}"
            for j in range(len(names)):
                if j <= i:
                    row += f"  {sim_matrix[i, j]:5.2f} "
                else:
                    row += "     .  "
            print(row)

        # Summary stats
        upper = []
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                upper.append(sim_matrix[i, j])
        if upper:
            mean_sim = np.mean(upper)
            min_sim = np.min(upper)
            max_sim = np.max(upper)
            print(f"    Mean cross-similarity: {mean_sim:.3f}  "
                  f"(range: {min_sim:.3f}–{max_sim:.3f})")
            if mean_sim > 0.95:
                print(f"    ⚠ Responses nearly identical — brain not differentiating yet")
            elif mean_sim < 0.5:
                print(f"    ✓ Good differentiation — responses are distinct")

    # --- Show evolution for any probe with history ---
    probes_with_history = [p for p in _eval_history if len(_eval_history[p]) >= 2]
    if probes_with_history:
        print(f"\n  Response evolution (first -> latest):")
        for probe_name in probes_with_history[:4]:
            history = _eval_history[probe_name]
            first = history[0]
            latest = history[-1]
            first_text = first[1][:30] if first[1] else f"|v|={first[3]:.1f}"
            latest_text = latest[1][:30] if latest[1] else f"|v|={latest[3]:.1f}"
            sim_change = latest[2] - first[2]
            norm_change = latest[3] - first[3]
            print(f"    {probe_name:8s}: {first_text:>32s} -> {latest_text:<32s}"
                  f"  dsim={sim_change:+.3f}  d|v|={norm_change:+.1f}")

    # --- Held-out metrics (real accuracy on unseen data) ---
    # Cap to 20 items to keep eval under 2 minutes (each calls decide_full ~5s)
    MAX_EVAL_ITEMS = 20
    if len(_held_out_buffer) >= 50:
        all_held_out = _held_out_buffer.get_eval_items()
        if len(all_held_out) > MAX_EVAL_ITEMS:
            held_out_items = random.sample(all_held_out, MAX_EVAL_ITEMS)
        else:
            held_out_items = all_held_out
        metrics = MetricsComputer.compute(brain, composer, decoder, held_out_items)
        if metrics:
            print(f"\n  Held-out metrics ({metrics['count']} items):")
            print(f"    Mean cosine sim:        {metrics['mean_cosine_sim']:.4f}")
            print(f"    Differentiation score:  {metrics['differentiation_score']:.4f}")
            if metrics['top_1_accuracy'] is not None:
                print(f"    Top-1 accuracy:         {metrics['top_1_accuracy']:.4f}")
                print(f"    Top-3 accuracy:         {metrics['top_3_accuracy']:.4f}")
                print(f"    Top-5 accuracy:         {metrics['top_5_accuracy']:.4f}")
            if metrics['per_domain']:
                print(f"    Per-domain:")
                for dom, ddata in sorted(metrics['per_domain'].items()):
                    print(f"      {dom:20s}: cos={ddata['mean_cosine_sim']:.4f}  "
                          f"n={ddata['count']}")
            # Log to metrics_log.jsonl
            try:
                entry = {
                    "stage": stage, "step": step,
                    "timestamp": time.time(),
                    **metrics,
                }
                with open(METRICS_LOG_FILE, "a") as f:
                    f.write(json.dumps(entry) + "\n")
            except Exception:
                pass

    # Spectral k-fold evaluation on held-out cognitive items
    if hasattr(evaluate_performance, '_spectral_splitter') and \
            evaluate_performance._spectral_splitter is not None:
        splitter = evaluate_performance._spectral_splitter
        fold_idx = evaluate_performance._fold_idx
        _, test_items = splitter.get_fold(fold_idx)
        if test_items:
            test_losses = []
            for item in test_items[:20]:  # Sample up to 20 test items
                try:
                    features = composer.compose(text=item['text'], modality='text')
                    target = make_semantic_target(item['answer'], category=item['domain'])
                    result = brain.decide_full(features)
                    # Measure reconstruction quality (cosine similarity)
                    out_vec = result.get('output_vector')
                    if out_vec is not None:
                        out_arr = np.array(out_vec, dtype=np.float32)
                        tgt_arr = np.array(target, dtype=np.float32)
                        min_dim = min(len(out_arr), len(tgt_arr))
                        cos_sim = np.dot(out_arr[:min_dim], tgt_arr[:min_dim]) / (
                            np.linalg.norm(out_arr[:min_dim]) * np.linalg.norm(tgt_arr[:min_dim]) + 1e-8)
                        test_losses.append(float(cos_sim))
                except Exception:
                    pass
            if test_losses:
                mean_sim = np.mean(test_losses)
                print(f"    [Spectral CV] Fold {fold_idx}: {len(test_losses)} test items, "
                      f"mean_cos_sim={mean_sim:.4f}")

    print(f"  {'─' * 56}\n")

    # Swap back from EMA to live parameters
    if ema_active:
        try:
            brain.utm_swap_from_ema()
        except (AttributeError, RuntimeError):
            pass

    # Unfreeze decoder after evaluation
    if decoder:
        decoder.unfreeze()


_checkpoint_thread = None

def _cleanup_old_checkpoints(keep_current=True):
    """Remove old checkpoint files to free disk space.

    Keeps the canonical athena_immersive.bin (symlink) and the 2 most recent snapshots.
    Removes all other .bin files and their sidecars.
    """
    if not os.path.isdir(CHECKPOINT_DIR):
        return
    if keep_current:
        _prune_checkpoint_snapshots(max_snapshots=2)
    else:
        _prune_checkpoint_snapshots(max_snapshots=0)


def _check_disk_space(min_gb=5.0):
    """Check available disk space. Warn if below threshold."""
    try:
        st = os.statvfs(CHECKPOINT_DIR if os.path.isdir(CHECKPOINT_DIR) else ".")
        avail_gb = (st.f_bavail * st.f_frsize) / (1024**3)
        if avail_gb < min_gb:
            print(f"  WARNING: Only {avail_gb:.1f} GB disk space remaining!")
            return False
        return True
    except OSError:
        return True


def _startup_cleanup_orphan_tmps():
    """Remove orphan .tmp / .tmp.* files left from crashed-mid-save runs.

    The in-process error path in _save_checkpoint_sync only fires on
    graceful exceptions. If the brain is SIGKILL'd or crashes mid-write,
    the .tmp files persist (each .tmp.snn is ~10 GB). This catches them
    on next training startup.

    Also sweeps any *.snn.tmp (gzip's internal temp), *.bin.tmp.* sidecars
    from interrupted brain.save() calls, and dangling immersive symlinks.
    """
    if not os.path.isdir(CHECKPOINT_DIR):
        return

    import glob as _g
    patterns = [
        os.path.join(CHECKPOINT_DIR, "*.tmp"),
        os.path.join(CHECKPOINT_DIR, "*.tmp.*"),
        os.path.join(CHECKPOINT_DIR, "*.snn.tmp"),  # gzip internal temp
        os.path.join(CHECKPOINT_DIR, "*.lnk"),       # symlink-update temps
    ]
    # Keep small in-flight files that would re-create instantly (state file,
    # cognitive seed flag) — audit bug #4 preventive measure.
    KEEP_BASENAMES = {"immersive_state.json.tmp", ".cognitive_seeded.tmp",
                      ".memory_seeded.tmp", ".spectral_fold.tmp"}
    freed_bytes = 0
    removed = 0
    for pat in patterns:
        for f in _g.glob(pat):
            if os.path.basename(f) in KEEP_BASENAMES:
                continue
            try:
                sz = os.path.getsize(f) if os.path.exists(f) else 0
                os.remove(f)
                freed_bytes += sz
                removed += 1
            except OSError:
                pass

    # Also remove dangling symlinks (target deleted but link remains)
    canonical = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
    for ext in ['', '.snn', '.cnn', '.lnn', '.meta', '.tokenizer',
                '.mirror_neurons', '.executive', '.knowledge', '.pink_noise',
                '.cortex_visual', '.cortex_audio',
                '.cortex_speech', '.cortex_somato']:
        link = canonical + ext
        if os.path.islink(link):
            try:
                target = os.readlink(link)
                target_path = target if os.path.isabs(target) else os.path.join(CHECKPOINT_DIR, target)
                if not os.path.exists(target_path):
                    os.remove(link)
                    removed += 1
                    print(f"  [cleanup] Removed dangling symlink: {os.path.basename(link)}")
            except OSError:
                pass

    if removed > 0 or freed_bytes > 0:
        print(f"  [startup-cleanup] Removed {removed} orphan files, "
              f"freed {freed_bytes / 1e9:.1f} GB")
    else:
        print(f"  [startup-cleanup] No orphan files found")


def _register_checkpoint_questdb(ckpt_path, stage, step, neuron_count=DEFAULT_ANN_NEURONS):
    """Register checkpoint metadata in QuestDB via ILP protocol."""
    try:
        import socket
        blob_size = os.path.getsize(ckpt_path)
        ts_ns = int(time.time() * 1e9)
        line = (
            f'kg_brain_snapshots,'
            f'snapshot_id=ckpt_{stage}_{step},'
            f'brain_id=athena,'
            f'name=athena_immersive '
            f'description="Stage {stage} step {step} checkpoint",'
            f'version=1i,'
            f'format_version=1i,'
            f'blob_path="{ckpt_path}",'
            f'blob_size={blob_size}i,'
            f'neuron_count={neuron_count}i,'
            f'stage={stage}i,'
            f'step={step}i '
            f'{ts_ns}'
        )
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('localhost', 9009))
        sock.sendall((line + '\n').encode())
        sock.close()
        logger.info("Checkpoint registered in QuestDB: stage=%d, step=%d", stage, step)
    except Exception as e:
        logger.debug("QuestDB registration skipped: %s", e)


def _prune_checkpoint_snapshots(max_snapshots=1):
    """Keep only the most recent `max_snapshots` timestamped snapshots.

    Deletes oldest snapshots AND ALL their sidecars (including the big .snn
    which is ~12-17 GB). Disk-constrained pods cannot keep multiple snapshots.

    Sidecar discovery is glob-based, not whitelist-based: the C library has
    grown sidecar extensions over time (.gl_lang, .gl_multiturn, .temperature
    were silently leaking ~40 GB of orphans on the production pod before this
    fix — every cycle, the prune deleted the .bin but left these orphaned).
    `snap + "*"` matches the .bin plus every sidecar — safe because sibling
    snapshots differ in their step number BEFORE `.bin`, so the glob cannot
    overmatch another snapshot's files.
    """
    import glob as glob_mod
    pattern = os.path.join(CHECKPOINT_DIR, "athena_s*_step*.bin")
    all_files = glob_mod.glob(pattern + "*")

    # Filter to bare .bin files only (not .bin.snn, etc.).
    # Sort by MODIFICATION TIME (oldest first) — NOT lexicographic.
    # Bug fix 2026-04-26: lexicographic sort treats step4500 < step950
    # ("4" < "9" at the first-differing char), so the brand-new step4500
    # was being deleted as "oldest" and step950 (yesterday) was retained.
    # Ship-to-hetzner then aborted: "main .bin missing step4500".
    snapshots = [s for s in all_files
                 if s.endswith(".bin") and "step" in os.path.basename(s)]
    snapshots.sort(key=os.path.getmtime)

    if len(snapshots) <= max_snapshots:
        return

    to_remove = snapshots[:len(snapshots) - max_snapshots]
    freed = 0
    sidecar_count = 0
    for snap in to_remove:
        for f in glob_mod.glob(snap + "*"):
            try:
                freed += os.path.getsize(f)
                os.remove(f)
                if f != snap:
                    sidecar_count += 1
            except OSError:
                pass
    if freed > 0:
        logger.info("Pruned %d old snapshots (+%d sidecars, freed %.1f GB)",
                    len(to_remove), sidecar_count, freed / 1e9)


def _save_checkpoint_sync(brain, decoder, stage, step):
    """Synchronous checkpoint save (runs in background thread).

    Saves timestamped snapshots: athena_s{stage}_step{step}.bin
    Also maintains athena_immersive.bin as a symlink to the latest for --resume.
    Keeps up to 5 snapshots, auto-prunes oldest.
    Uses write-to-temp + size validation to prevent corruption.
    """
    os.makedirs(CHECKPOINT_DIR, exist_ok=True)

    # Need ~20 GB free for new checkpoint write (.snn alone is ~10-12 GB
    # gzipped, plus other sidecars). If low, ship-and-delete the previous
    # snapshot to Hetzner BEFORE writing the new one — never hold 2x locally.
    DISK_HEADROOM_GB = 20.0
    if not _check_disk_space(min_gb=DISK_HEADROOM_GB):
        logger.warning("Low disk space (<%g GB free) — shipping previous snapshot first",
                       DISK_HEADROOM_GB)
        _ship_and_remove_previous_snapshot()
        if not _check_disk_space(min_gb=DISK_HEADROOM_GB / 2):
            # EMERGENCY: ship the canonical itself (last resort).
            # Better to lose a few minutes of training (re-do recent steps
            # on resume) than skip saves indefinitely while disk fills.
            # Fixes audit bug #1 — checkpoint deadlock when no older snapshot.
            logger.warning("Disk still low after ship — emergency ship of CURRENT canonical")
            try:
                canonical_p = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
                if os.path.islink(canonical_p):
                    target = os.readlink(canonical_p)
                    target_path = os.path.join(CHECKPOINT_DIR, target)
                    if os.path.exists(target_path):
                        import subprocess as _sp
                        sidecars = ['', '.snn', '.cnn', '.lnn',
                                    '.cortex_visual', '.cortex_audio',
                                    '.cortex_speech', '.cortex_somato',
                                    '.meta', '.tokenizer', '.mirror_neurons', '.executive']
                        files = [target_path + ext for ext in sidecars
                                 if os.path.exists(target_path + ext)]
                        if files:
                            _sp.run(["rsync", "-az", "--partial", "--timeout=1800",
                                     "--remove-source-files",
                                     "-e", "ssh -o StrictHostKeyChecking=no",
                                     ] + files + [
                                     "bbrelin@176.9.99.103:/home/bbrelin/nimcp/checkpoints/athena/"],
                                    timeout=2400, check=False, capture_output=True)
                            # Remove now-dangling symlinks
                            for ext in sidecars:
                                lnk = canonical_p + ext
                                if os.path.islink(lnk) and not os.path.exists(lnk):
                                    try: os.remove(lnk)
                                    except OSError: pass
                            logger.warning("Emergency ship complete — brain will need to "
                                           "pull from Hetzner on next restart")
                            # Mark state file so resume knows local snapshot is gone
                            # (audit follow-up: warn the operator/restart logic).
                            try:
                                marker = os.path.join(CHECKPOINT_DIR,
                                                       ".local_checkpoint_shipped")
                                with open(marker, "w") as _mf:
                                    _mf.write(json.dumps({
                                        "shipped_at": time.strftime("%Y-%m-%d %H:%M:%S"),
                                        "stage": stage, "step": step,
                                        "shipped_target": target,
                                    }))
                            except Exception:
                                pass
            except Exception as _ee:
                logger.error("Emergency ship failed: %s", _ee)
            if not _check_disk_space(min_gb=DISK_HEADROOM_GB / 2):
                logger.error("STILL insufficient disk space — skipping save. "
                             "Local checkpoint state may be inconsistent. "
                             "Consider manual intervention.")
                return

    snapshot_name = f"athena_s{stage}_step{step}.bin"
    snapshot_path = os.path.join(CHECKPOINT_DIR, snapshot_name)
    snapshot_tmp = snapshot_path + ".tmp"
    canonical = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
    sidecars = ['.meta', '.tokenizer', '.mirror_neurons', '.executive']

    try:
        # Write to temp file — if interrupted, no existing checkpoint is touched
        brain.save(snapshot_tmp)

        # Verify the temp file is non-trivial. The main .bin holds only the
        # ANN portion (~750 MB for 150K neurons); the SNN/CNN/LNN are in
        # .tmp.snn/.tmp.cnn/.tmp.lnn sidecars. Use a small threshold for
        # the main file alone and verify the SNN sidecar separately.
        tmp_size = os.path.getsize(snapshot_tmp)
        if tmp_size < CHECKPOINT_MIN_SIZE:
            logger.error("Checkpoint main .bin too small (%d bytes) — discarding",
                         tmp_size)
            os.remove(snapshot_tmp)
            # Also clean up sidecar tmps to avoid orphans
            import glob as _gc
            for stmp in _gc.glob(snapshot_tmp + ".*"):
                try:
                    os.remove(stmp)
                except OSError:
                    pass
            return

        # Verify SNN sidecar exists if SNN is in use (the big one we kept losing)
        snn_tmp = snapshot_tmp + ".snn"
        if os.path.exists(snn_tmp):
            snn_size = os.path.getsize(snn_tmp)
            logger.info("SNN sidecar size: %.1f GB", snn_size / 1e9)

        # Atomic rename temp -> snapshot
        os.replace(snapshot_tmp, snapshot_path)

        # CRITICAL: Rename all sidecar files written by brain.save().
        # All sidecars are now atomic .tmp+rename inside the C library:
        #   .snn (snn_network_save: writes <path>.tmp then renames)
        #   .cnn .lnn .cortex_* (same pattern, internal .tmp+rename)
        #   .meta .executive .mirror_neurons .tokenizer (audit fix #2/#3
        #     made these atomic too — were previously direct writes)
        # Since we passed snapshot_tmp ("athena_s0_step{N}.bin.tmp") as
        # the base path, the C library wrote files at:
        #   athena_s0_step{N}.bin.tmp.<ext>  (after C internal rename)
        # We now rename those to the real names. glob() finds all of
        # them. The ALL_SIDECARS list below is used for symlink update,
        # which still needs explicit names (glob doesn't help).
        ALL_SIDECARS = ['.snn', '.cnn', '.lnn',
                        '.cortex_visual', '.cortex_audio',
                        '.cortex_speech', '.cortex_somato',
                        '.meta', '.tokenizer', '.mirror_neurons', '.executive',
                        '.knowledge', '.pink_noise',
                        # Deferred-load sidecars (loaded via
                        # brain_load_post_init_sidecars after subsystems
                        # are constructed). Without these symlinks the
                        # athena_immersive.bin.<ext> path doesn't resolve
                        # on resume → silent "no sidecar at..." log →
                        # trained state lost. The .gl_lang miss is what
                        # caused the lexicon to wipe on every restart.
                        '.gl_lang', '.gl_multiturn', '.immune', '.kg']
        import glob as _g
        sidecar_tmps = _g.glob(snapshot_tmp + ".*")
        renamed_exts = set()
        for stmp in sidecar_tmps:
            ext = stmp[len(snapshot_tmp):]  # ".snn", ".cnn", etc.
            try:
                os.replace(stmp, snapshot_path + ext)
                renamed_exts.add(ext)
            except OSError as _e:
                logger.warning("Failed to rename sidecar %s: %s", stmp, _e)

        # Update canonical symlink (athena_immersive.bin -> latest snapshot)
        # AND all sidecar symlinks so the brain loads matching .snn/.cnn/etc.
        canonical_tmp = canonical + ".lnk"
        try:
            if os.path.exists(canonical_tmp):
                os.remove(canonical_tmp)
            os.symlink(snapshot_name, canonical_tmp)
            os.replace(canonical_tmp, canonical)
        except OSError as _se:
            logger.warning("Symlink update for %s failed: %s — falling back to copy",
                           canonical, _se)
            try:
                import shutil
                shutil.copy2(snapshot_path, canonical)
            except Exception as _ce:
                logger.error("Canonical copy fallback also failed: %s", _ce)

        # Update sidecar symlinks. Without this, athena_immersive.bin.snn
        # points to a stale .snn from a previous snapshot.
        # Audit bug #10: symlink failures used to silently warn, now we
        # retry then raise — stale-state silently loading is too dangerous.
        def _update_one_symlink(ext):
            src = snapshot_path + ext
            dst = canonical + ext
            if not os.path.exists(src):
                return None  # nothing to link, not a failure
            tmp_link = dst + ".lnk"
            if os.path.exists(tmp_link):
                os.remove(tmp_link)
            os.symlink(snapshot_name + ext, tmp_link)
            os.replace(tmp_link, dst)
            return True

        retry_failures = []
        for ext in ALL_SIDECARS:
            try:
                _update_one_symlink(ext)
            except OSError as _e:
                logger.warning("Sidecar symlink %s failed once: %s — retrying",
                               canonical + ext, _e)
                # Retry with fresh tmp name (in case .lnk was stale)
                try:
                    _update_one_symlink(ext)
                except OSError as _e2:
                    retry_failures.append((ext, str(_e2)))

        if retry_failures:
            msg = ("Sidecar symlink update failed after retry for {} extensions: {}"
                   " — refusing to mark save complete. Brain will load stale "
                   "state on next restart unless this is fixed.").format(
                len(retry_failures), retry_failures)
            logger.error(msg)
            # Raise so save is treated as failed (state file NOT updated).
            raise IOError(msg)

        logger.info("Checkpoint snapshot: %s (stage=%d, step=%d, size=%.1f MB)",
                     snapshot_name, stage, step, tmp_size / 1e6)

        # CB rescale marker: if conductance is on, the brain.save() above
        # captured rescaled weights. Pin a marker to BOTH the snapshot
        # file AND the canonical symlink target so a daemon restart
        # loading from either path skips force-rescale-on-load. See
        # cb_rescaled_marker.py.
        try:
            cb_on = float(brain.snn_tune_get().get(
                'conductance_enabled', 0.0)) != 0.0
            if cb_on:
                cb_rescaled_marker.write_marker(snapshot_path,
                                                CB_DEFAULT_RESCALE_FACTOR)
                # canonical (athena_immersive.bin) symlinks to the same
                # underlying file via realpath, so writing once via
                # snapshot_path covers both. But explicitly write through
                # canonical too in case a future change splits the files.
                cb_rescaled_marker.write_marker(canonical,
                                                CB_DEFAULT_RESCALE_FACTOR)
        except Exception as _cbm_e:
            logger.warning("CB marker write failed (snapshot still good): %s",
                           _cbm_e)

        # Update state file AFTER checkpoint is verified on disk
        state = {"stage": stage, "step": step,
                 "snapshot": snapshot_name,
                 "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")}
        # Atomic write — crash mid-write would leave .tmp instead of
        # corrupting the real file (audit bug #4).
        state_tmp = STATE_FILE + ".tmp"
        # Clean any stale .tmp from previous crashed write (audit follow-up:
        # without this, KEEP-list entries in startup cleanup accumulate
        # forever since fresh writes don't reuse old .tmp files).
        if os.path.exists(state_tmp):
            try:
                os.remove(state_tmp)
            except OSError:
                pass
        try:
            with open(state_tmp, "w") as f:
                json.dump(state, f, indent=2)
            os.replace(state_tmp, STATE_FILE)
        except Exception as _state_e:
            logger.warning("State file write failed: %s — resume may load stale step",
                           _state_e)
            try:
                if os.path.exists(state_tmp):
                    os.remove(state_tmp)
            except OSError:
                pass
    except Exception as e:
        logger.warning("Checkpoint save failed: %s — cleaning up .tmp files", e)
        # Clean up ALL .tmp files for this snapshot, not just the main .bin.
        # Without this, 9.8 GB .tmp.snn files accumulate on every failed save
        # and fill the disk within hours.
        import glob as _eg
        for stmp in [snapshot_tmp] + _eg.glob(snapshot_tmp + ".*"):
            if os.path.exists(stmp):
                try:
                    freed = os.path.getsize(stmp)
                    os.remove(stmp)
                    logger.info("Cleaned orphan .tmp: %s (%.1f GB)",
                                os.path.basename(stmp), freed / 1e9)
                except OSError as _re:
                    logger.warning("Failed to remove %s: %s", stmp, _re)
        return

    # Save decoder alongside the snapshot. Log failures — silent decoder
    # save loss means the decoder vocabulary diverges from brain outputs.
    if decoder:
        os.makedirs(DECODER_DIR, exist_ok=True)
        try:
            decoder.save(DECODER_DIR)
        except Exception as _de:
            logger.warning("Decoder save failed: %s — vocab will be out of sync", _de)

    # State file already written in _save_checkpoint() before thread started

    # Prune old snapshots (keep 1 — disk-constrained pod, .snn is ~12 GB)
    _prune_checkpoint_snapshots(max_snapshots=1)

    # Register checkpoint in QuestDB
    try:
        nc = brain.get_neuron_count()
    except Exception:
        nc = DEFAULT_ANN_NEURONS
    _register_checkpoint_questdb(snapshot_path, stage, step, neuron_count=nc)

    # === VALIDATE → COMPRESS → MOVE TO HETZNER (every save) ===
    # Disk-constrained pod (70 GB NVMe). Every save we must:
    #   1. Validate: each sidecar exists and has reasonable size
    #   2. Compress: any non-gzipped file (.snn is already gzip from C)
    #   3. Move to Hetzner: rsync --remove-source-files (true move, not copy)
    # We KEEP the current athena_immersive.bin* (symlinks + targets) locally
    # for fast restart. The previous step's files (older snapshot) get
    # removed when the next save's prune fires.
    _ship_to_hetzner(snapshot_path, snapshot_name, stage, step)


def _ship_and_remove_previous_snapshot():
    """Ship an OLDER snapshot (not the current canonical target) to Hetzner
    and remove it locally to free disk space.

    NEVER ships the current canonical target — we need that locally for
    restart. If only the canonical snapshot exists (no older ones), this
    does nothing and the save will fail; better to fail safely than to
    remove the only local brain state.
    """
    canonical = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
    current_target = None
    if os.path.islink(canonical):
        try:
            current_target = os.readlink(canonical)
        except OSError:
            pass

    # Collect all athena_s*_step*.bin files
    import glob as _g
    candidates = sorted(_g.glob(os.path.join(CHECKPOINT_DIR, "athena_s*_step*.bin")),
                        key=os.path.getmtime)
    candidates = [c for c in candidates
                  if c.endswith(".bin") and ".bin." not in os.path.basename(c)]

    # CRITICAL: exclude the current canonical target so we never delete
    # the only local copy of the brain state.
    if current_target:
        candidates = [c for c in candidates
                      if os.path.basename(c) != current_target]

    if not candidates:
        logger.warning("Pre-save ship: no OLDER snapshots to ship "
                       "(current target: %s) — refusing to delete canonical",
                       current_target or "unknown")
        return
    target_path = candidates[0]  # oldest
    logger.info("Pre-save ship: shipping oldest snapshot %s "
                "(keeping current %s)",
                os.path.basename(target_path), current_target or "none")

    sidecars = ['.snn', '.cnn', '.lnn',
                '.cortex_visual', '.cortex_audio',
                '.cortex_speech', '.cortex_somato',
                '.meta', '.tokenizer', '.mirror_neurons', '.executive']
    files = [target_path] + [target_path + ext for ext in sidecars
                              if os.path.exists(target_path + ext)]
    if not files:
        return

    import subprocess
    HETZNER_HOST = "bbrelin@176.9.99.103"
    HETZNER_DIR = "/home/bbrelin/nimcp/checkpoints/athena"
    try:
        cmd = ["rsync", "-az", "--partial", "--timeout=1200",
               "--remove-source-files",
               "-e", "ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10"
               ] + files + [f"{HETZNER_HOST}:{HETZNER_DIR}/"]
        r = subprocess.run(cmd, timeout=1800, capture_output=True)
        if r.returncode != 0:
            logger.warning("Pre-save ship failed: %s", r.stderr.decode()[:500])
            return
        logger.info("Pre-save: shipped+removed older snapshot %s (%d files) — "
                    "canonical %s preserved",
                    os.path.basename(target_path), len(files),
                    current_target or "unchanged")
    except Exception as e:
        logger.warning("Pre-save ship error: %s", e)


def _ship_to_hetzner(snapshot_path, snapshot_name, stage, step):
    """Validate, compress (non-gzipped sidecars), and rsync-move to Hetzner.

    Files to ship: snapshot_path (main .bin) + all known sidecars.
    The .snn is already gzipped by snn_network_save (C-level).
    Other sidecars (.cnn .lnn .meta etc.) are uncompressed — we wrap
    them with zstd for transfer, then unwrap on Hetzner side via
    sync_checkpoint.sh.
    """
    import subprocess
    HETZNER_HOST = "bbrelin@176.9.99.103"
    HETZNER_DIR = "/home/bbrelin/nimcp/checkpoints/athena"

    # 1. VALIDATE — check each expected file exists with reasonable size
    sidecars = ['.snn', '.cnn', '.lnn',
                '.cortex_visual', '.cortex_audio',
                '.cortex_speech', '.cortex_somato',
                '.meta', '.tokenizer', '.mirror_neurons', '.executive']
    # Min sizes (bytes) — refuse to ship truncated files.
    # .snn is the big one (gzipped, expect 5+ GB for 1.45B synapses)
    min_sizes = {
        '': 1024 * 1024,         # main .bin: at least 1 MB
        '.snn': 1 * 1024 * 1024,  # 1 MB minimum (was 17 GB uncompressed; 5+ GB gzipped)
        '.cnn': 1024,            # at least 1 KB
        '.lnn': 1024,
        '.meta': 100,
        '.tokenizer': 100,
        '.mirror_neurons': 100,
        '.executive': 50,
    }
    files_to_ship = []
    for ext in [''] + sidecars:
        f = snapshot_path + ext
        if not os.path.exists(f):
            if ext == '':
                logger.error("Hetzner ship: main .bin missing %s — abort", f)
                return
            continue  # sidecars are best-effort
        size = os.path.getsize(f)
        min_sz = min_sizes.get(ext, 50)
        if size < min_sz:
            logger.warning("Hetzner ship: %s only %d bytes (min %d) — skipping",
                           f, size, min_sz)
            continue
        files_to_ship.append(f)

    if not files_to_ship:
        logger.warning("Hetzner ship: no valid files for step %d", step)
        return

    # 2. ENSURE REMOTE DIR EXISTS
    try:
        subprocess.run(
            ["ssh", "-o", "StrictHostKeyChecking=no", "-o", "ConnectTimeout=10",
             HETZNER_HOST, f"mkdir -p {HETZNER_DIR}"],
            timeout=30, check=False, capture_output=True)
    except Exception as e:
        logger.warning("Hetzner ship: mkdir failed: %s", e)
        return

    # 3. RSYNC --remove-source-files (true MOVE)
    # rsync transfers, verifies (--partial keeps partial on retry), and
    # only deletes source after successful transfer. -z compresses on the wire.
    # IMPORTANT: do NOT remove source for the file currently symlinked by
    # athena_immersive.bin (we need it for fast restart).
    canonical = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
    canonical_target = None
    if os.path.islink(canonical):
        try:
            canonical_target = os.readlink(canonical)  # e.g., "athena_s0_step150.bin"
        except OSError:
            pass

    # Files to MOVE (not currently in use as symlink target)
    movable = []
    for f in files_to_ship:
        base = os.path.basename(f)
        # Skip if this is the canonical symlink target or a sidecar of it
        if canonical_target and base.startswith(canonical_target):
            continue
        movable.append(f)

    # Files to COPY (current symlink target — keep local for restart)
    copyable = [f for f in files_to_ship if f not in movable]

    try:
        # COPY current symlinked snapshot (no --remove-source-files)
        if copyable:
            cmd = ["rsync", "-az", "--partial", "--timeout=1200",
                   "-e", "ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10"
                   ] + copyable + [f"{HETZNER_HOST}:{HETZNER_DIR}/"]
            r = subprocess.run(cmd, timeout=1800, capture_output=True)
            if r.returncode != 0:
                logger.warning("Hetzner copy failed: %s", r.stderr.decode()[:500])
                return
            logger.info("Hetzner: copied %d current files (%.1f MB)",
                        len(copyable),
                        sum(os.path.getsize(f) for f in copyable) / 1e6)

        # MOVE older snapshot files (--remove-source-files deletes after success)
        if movable:
            cmd = ["rsync", "-az", "--partial", "--timeout=1200",
                   "--remove-source-files",
                   "-e", "ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10"
                   ] + movable + [f"{HETZNER_HOST}:{HETZNER_DIR}/"]
            r = subprocess.run(cmd, timeout=1800, capture_output=True)
            if r.returncode != 0:
                logger.warning("Hetzner move failed: %s", r.stderr.decode()[:500])
                return
            freed = sum(os.path.getsize(f) for f in movable if os.path.exists(f))
            logger.info("Hetzner: moved %d old files (freed %.1f MB)",
                        len(movable), (sum(min_sizes.get('', 0) for _ in movable) - freed) / 1e6)

        logger.info("Hetzner ship complete: stage=%d step=%d (%d copied + %d moved)",
                    stage, step, len(copyable), len(movable))
    except Exception as e:
        logger.warning("Hetzner ship error: %s", e)


def _save_checkpoint(brain, decoder, stage, step):
    """Non-blocking checkpoint save — runs in background thread."""
    global _checkpoint_thread
    from threading import Thread

    # State file is now updated INSIDE _save_checkpoint_sync after the .bin
    # is written and verified. This prevents the state file from pointing to
    # a snapshot that doesn't exist yet (or was never written due to crash).

    # Wait for previous checkpoint to finish (if still running)
    if _checkpoint_thread is not None and _checkpoint_thread.is_alive():
        _checkpoint_thread.join(timeout=30)
    _checkpoint_thread = Thread(target=_save_checkpoint_sync,
                                args=(brain, decoder, stage, step),
                                daemon=False)
    _checkpoint_thread.start()


def _load_state():
    """Load last training state."""
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE) as f:
                return json.load(f)
        except Exception:
            pass
    return None


# ============================================================================
def _pre_generate_all_stages(args, force_fresh=False):
    """Check for Claude content cache. Generate if missing via generate_content.py."""
    if os.path.exists(CONTENT_CACHE):
        try:
            with open(CONTENT_CACHE) as f:
                cache = json.load(f)
            if all(str(s) in cache for s in range(4)):
                total = sum(len(v) for d in cache.values() for v in d.values() if isinstance(v, list))
                print(f"  [Claude] Content cache loaded — {total} items across 4 stages")
                return
        except Exception:
            pass

    # Try to generate inline (only works if enough free RAM for claude CLI)
    print("  [Claude] No content cache found. Run 'python3 scripts/generate_content.py' first.")
    print("  [Claude] Attempting inline generation...")
    try:
        import subprocess as _sp
        result = _sp.run(
            [sys.executable, "scripts/generate_content.py", "--force"],
            timeout=600, cwd="/home/bbrelin/nimcp",
            env={k: v for k, v in os.environ.items()
                 if k not in ("CLAUDECODE", "CUDA_VISIBLE_DEVICES")}
        )
        if result.returncode == 0 and os.path.exists(CONTENT_CACHE):
            print("  [Claude] Content generated successfully")
            return
    except Exception as e:
        print(f"  [Claude] Inline generation failed: {e}")
    print("  [Claude] Will use fallback narrations (run generate_content.py separately)")


# Main
# ============================================================================

def _kill_stale_processes():
    """Kill old immerse_athena / training processes and GPU zombies before starting."""
    import subprocess
    my_pid = os.getpid()
    killed = []

    # Kill old immerse_athena.py instances
    try:
        result = subprocess.run(
            ["pgrep", "-f", "immerse_athena.py"],
            capture_output=True, text=True, timeout=5)
        for line in result.stdout.strip().splitlines():
            pid = int(line.strip())
            if pid != my_pid and pid != os.getppid():
                try:
                    os.kill(pid, signal.SIGKILL)
                    killed.append(("immerse_athena", pid))
                except ProcessLookupError:
                    pass
    except Exception:
        pass

    # Kill zombie Python processes holding GPU memory
    try:
        result = subprocess.run(
            ["nvidia-smi", "--query-compute-apps=pid", "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=5)
        for line in result.stdout.strip().splitlines():
            pid = int(line.strip())
            if pid == my_pid:
                continue
            # Check if it's a zombie or orphaned multiprocessing child
            try:
                status_path = f"/proc/{pid}/status"
                with open(status_path) as f:
                    status_text = f.read()
                is_zombie = "State:\tZ" in status_text
                is_orphan_spawn = False
                cmdline_path = f"/proc/{pid}/cmdline"
                with open(cmdline_path) as f:
                    cmdline = f.read()
                is_orphan_spawn = "multiprocessing" in cmdline and "spawn" in cmdline
                if is_zombie or is_orphan_spawn:
                    os.kill(pid, signal.SIGKILL)
                    killed.append(("gpu-zombie", pid))
            except (FileNotFoundError, ProcessLookupError):
                pass
    except Exception:
        pass

    if killed:
        time.sleep(1)  # let resources release
        for label, pid in killed:
            print(f"  [Cleanup] Killed stale {label} process (PID {pid})")


# ---------------------------------------------------------------------------
# IPC Server — allows talk_to_athena.py to query the running brain
# ---------------------------------------------------------------------------

ATHENA_SOCKET_PATH = "/var/run/athena/athena_ipc.sock"
# Fallback if /var/run/athena doesn't exist (non-daemon mode)
if not os.path.isdir(os.path.dirname(ATHENA_SOCKET_PATH)):
    ATHENA_SOCKET_PATH = "/tmp/athena_ipc.sock"


class AthenaIPCServer:
    """Unix socket server that exposes the running brain for external queries.

    Protocol: length-prefixed JSON messages.
      Request:  4-byte big-endian length + JSON bytes
      Response: 4-byte big-endian length + JSON bytes

    Supported commands:
      {"cmd": "decide", "text": "..."}           → run decide_full
      {"cmd": "decide", "text": "...", "top_k": 5} → with top-k vocab matches
      {"cmd": "status"}                           → training progress
      {"cmd": "transcript"}                       → last cognitive transcript
      {"cmd": "stats"}                            → brain stats
    """

    def __init__(self, brain, decoder, composer, training_progress, phi3=None):
        self.brain = brain
        self.decoder = decoder
        self.composer = composer
        self.training_progress = training_progress
        self.phi3 = phi3
        self._lock = threading.Lock()
        self._server_sock = None
        self._thread = None
        self._running = False

    def start(self):
        # Check if the brain daemon holds the socket lock — never steal it
        lock_path = ATHENA_SOCKET_PATH + ".lock"
        if os.path.exists(lock_path):
            import fcntl
            try:
                fd = open(lock_path, "r")
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                # Lock acquired → no daemon holding it, release and proceed
                fcntl.flock(fd, fcntl.LOCK_UN)
                fd.close()
            except (IOError, OSError):
                # Lock is held by the daemon — don't bind
                print(f"  IPC server: skipped — brain daemon owns {ATHENA_SOCKET_PATH}")
                return

        # Clean up stale socket
        if os.path.exists(ATHENA_SOCKET_PATH):
            os.unlink(ATHENA_SOCKET_PATH)

        self._server_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._server_sock.bind(ATHENA_SOCKET_PATH)
        self._server_sock.listen(2)
        self._server_sock.settimeout(1.0)  # allow periodic shutdown checks
        self._running = True

        self._thread = threading.Thread(target=self._serve, daemon=True,
                                         name="athena-ipc")
        self._thread.start()
        print(f"  IPC server: listening on {ATHENA_SOCKET_PATH}")

    def stop(self):
        self._running = False
        if self._server_sock:
            self._server_sock.close()
        if os.path.exists(ATHENA_SOCKET_PATH):
            os.unlink(ATHENA_SOCKET_PATH)

    def _serve(self):
        while self._running:
            try:
                conn, _ = self._server_sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                self._handle_connection(conn)
            except Exception as e:
                logging.getLogger("athena_ipc").warning("IPC error: %s", e)
            finally:
                conn.close()

    def _recv_msg(self, conn):
        """Read a length-prefixed message."""
        hdr = b""
        while len(hdr) < 4:
            chunk = conn.recv(4 - len(hdr))
            if not chunk:
                return None
            hdr += chunk
        length = struct.unpack(">I", hdr)[0]
        if length > 10 * 1024 * 1024:  # 10 MB sanity limit
            return None
        data = b""
        while len(data) < length:
            chunk = conn.recv(min(length - len(data), 65536))
            if not chunk:
                return None
            data += chunk
        return json.loads(data.decode("utf-8"))

    def _send_msg(self, conn, obj):
        """Send a length-prefixed JSON message."""
        data = json.dumps(obj).encode("utf-8")
        conn.sendall(struct.pack(">I", len(data)) + data)

    def _handle_connection(self, conn):
        conn.settimeout(30.0)
        req = self._recv_msg(conn)
        if req is None:
            return

        cmd = req.get("cmd", "")

        if cmd == "decide":
            resp = self._handle_decide(req)
        elif cmd == "status":
            resp = self._handle_status()
        elif cmd == "transcript":
            resp = self._handle_transcript()
        elif cmd == "stats":
            resp = self._handle_stats()
        elif cmd == "ping":
            resp = {"ok": True}
        else:
            resp = {"error": f"Unknown command: {cmd}"}

        self._send_msg(conn, resp)

    def _handle_decide(self, req):
        text = req.get("text", "")
        top_k = req.get("top_k", 5)
        if not text:
            return {"error": "No text provided"}

        with self._lock:
            input_emb = encode_text(text)
            features = self.composer.compose(text=text, modality="text")
            result = self.brain.decide_full(features)

        output_vec = result.get("output_vector")
        label = result.get("label", "")
        confidence = result.get("confidence", 0.0)

        resp = {
            "label": label,
            "confidence": confidence,
            "output_size": len(output_vec) if output_vec else 0,
        }

        # Decode through vocabulary
        if output_vec and self.decoder and len(self.decoder.vocabulary) > 0:
            output_arr = np.array(output_vec, dtype=np.float32)
            output_emb = extract_embedding_from_output(output_arr)
            matches = self.decoder.vocabulary.decode(output_emb, top_k=top_k)
            resp["decoded"] = [{"text": t, "similarity": float(s)}
                               for t, s in matches if t]
            if matches and matches[0][0]:
                resp["response"] = matches[0][0]

            # Coherence
            input_emb_raw = encode_text(text)
            coherence = float(np.dot(output_emb, input_emb_raw) /
                            (np.linalg.norm(output_emb) *
                             np.linalg.norm(input_emb_raw) + 1e-8))
            resp["coherence"] = coherence
            resp["output_norm"] = float(np.linalg.norm(output_arr))

        # Try grounded response
        try:
            grounded = self.brain.grounded_respond(text)
            gt = grounded.get("response", "")
            gc = grounded.get("confidence", 0.0)
            if gt and gc >= 0.3:
                resp["grounded_response"] = gt
                resp["grounded_confidence"] = gc
        except Exception:
            pass

        # Try Phi-3 language cortex
        if output_vec and self.phi3 and self.phi3.available:
            try:
                phi3_result = self.phi3.generate(
                    user_text=text,
                    vocab_match=resp.get("response"),
                    max_tokens=128,
                )
                if phi3_result and phi3_result.get('text'):
                    resp["phi3_response"] = phi3_result['text']
            except Exception:
                pass

        # Try generate_text
        if output_vec:
            try:
                gen = self.brain.generate_text(list(output_vec))
                gt = gen.get("text", "")
                if gt and len(gt.strip()) > 2:
                    resp["generated_text"] = gt
            except Exception:
                pass

        # Transcript
        try:
            transcript = self.brain.get_transcript()
            if transcript:
                resp["transcript"] = transcript
        except Exception:
            pass

        return resp

    def _handle_status(self):
        stage, step = self.training_progress
        return {
            "stage": stage,
            "step": step,
            "vocab_size": len(self.decoder.vocabulary) if self.decoder else 0,
        }

    def _handle_transcript(self):
        try:
            transcript = self.brain.get_transcript()
            return {"transcript": transcript or []}
        except Exception as e:
            return {"error": str(e)}

    def _handle_stats(self):
        try:
            stats = self.brain.get_stats()
            return {"stats": stats}
        except Exception as e:
            return {"error": str(e)}


def main():
    # Declared early — rebound below after brain.probe() to match the brain's
    # actual layer sizes (post-checkpoint-restore). Python requires `global`
    # to appear before any reference to these names in the function.
    global BRAIN_INPUT_DIM, BRAIN_OUTPUT_DIM
    parser = argparse.ArgumentParser(
        description="Immersive Developmental Learning for Athena")
    parser.add_argument("--stage", type=int, default=0,
                        help="Start from stage (0-3)")
    parser.add_argument("--resume", action="store_true",
                        help="Resume from last checkpoint")
    parser.add_argument("--checkpoint", type=str, default=None,
                        help="Path to checkpoint file")
    parser.add_argument("--no-claude", action="store_true",
                        help="Disable Claude parent (silent mode)")
    parser.add_argument("--num-inputs", type=int, default=BRAIN_INPUT_DIM)
    parser.add_argument("--num-outputs", type=int, default=BRAIN_OUTPUT_DIM)
    parser.add_argument("--neuron-count", type=int, default=DEFAULT_ANN_NEURONS,
                        help=f"ANN neuron count (default: {DEFAULT_ANN_NEURONS}, SNN is primary)")
    parser.add_argument("--snn-neuron-count", type=int, default=DEFAULT_SNN_NEURONS,
                        help=f"SNN target neuron count (default: {DEFAULT_SNN_NEURONS})")
    parser.add_argument("--lnn-neuron-count", type=int, default=DEFAULT_LNN_NEURONS,
                        help=f"LNN neuron count (default: {DEFAULT_LNN_NEURONS})")
    parser.add_argument("--stage0-stimuli", type=int, default=20000)
    parser.add_argument("--stage1-stimuli", type=int, default=40000)
    parser.add_argument("--stage2-stimuli", type=int, default=40000)
    parser.add_argument("--stage3-interactions", type=int, default=30000)
    parser.add_argument("--fresh", action="store_true",
                        help="Start fresh (ignore existing checkpoint)")
    parser.add_argument("--no-multimodal", action="store_true",
                        help="Disable multimodal dataset download")
    parser.add_argument("--daemon", action="store_true",
                        help="Connect to brain daemon instead of loading brain")
    # --- Canonical literary corpus ingestion (default ON) ---
    if hasattr(argparse, "BooleanOptionalAction"):
        parser.add_argument("--canonical-corpus",
                            action=argparse.BooleanOptionalAction,
                            default=True,
                            help="Ingest the canonical literary corpus during stages 1-3 "
                                 "(default: on; pass --no-canonical-corpus to skip)")
    else:
        parser.add_argument("--canonical-corpus", dest="canonical_corpus",
                            action="store_true", default=True,
                            help="Ingest the canonical literary corpus during stages 1-3")
        parser.add_argument("--no-canonical-corpus", dest="canonical_corpus",
                            action="store_false",
                            help="Disable canonical-corpus ingestion")
    parser.add_argument("--canonical-chunk-chars", type=int, default=1200,
                        help="Max characters per canonical-corpus chunk (default: 1200)")
    parser.add_argument("--canonical-max-chunks-per-stage", type=int, default=0,
                        help="Cap chunks per stage priming pass (0 = unlimited; "
                             "positive caps for fast smoke runs)")
    parser.add_argument("--canonical-restart", action="store_true",
                        help="Delete .canonical_corpus_state.json before starting")
    parser.add_argument("--canonical-corpus-root", type=str,
                        default="data/canonical_corpus",
                        help="Path to canonical corpus root (default: data/canonical_corpus)")

    # --- Math + formal logic corpus (default ON) ---
    # The parietal lobe handles numerical primitives (magnitude, comparison);
    # this corpus supplies symbolic content (variables, proofs, calculus,
    # logic) that has no architectural prior.
    if hasattr(argparse, "BooleanOptionalAction"):
        parser.add_argument("--math-corpus",
                            action=argparse.BooleanOptionalAction,
                            default=True,
                            help="Ingest the math+logic corpus during stages 1-3 "
                                 "(default: on; pass --no-math-corpus to skip)")
    else:
        parser.add_argument("--math-corpus", dest="math_corpus",
                            action="store_true", default=True,
                            help="Ingest the math+logic corpus during stages 1-3")
        parser.add_argument("--no-math-corpus", dest="math_corpus",
                            action="store_false",
                            help="Disable math-corpus ingestion")
    parser.add_argument("--math-chunk-chars", type=int, default=1200,
                        help="Max characters per math-corpus chunk (default: 1200)")
    parser.add_argument("--math-max-chunks-per-stage", type=int, default=0,
                        help="Cap math chunks per stage priming pass "
                             "(0 = unlimited; positive caps for fast smoke runs)")
    parser.add_argument("--math-restart", action="store_true",
                        help="Delete .math_corpus_state.json before starting")
    parser.add_argument("--math-corpus-root", type=str,
                        default="data/math_corpus",
                        help="Path to math corpus root (default: data/math_corpus)")

    # --- Streaming corpora (Wikipedia, Investopedia, arXiv,
    # gutenberg-stream).  Comma-separated kind list, default empty so
    # nothing fires until backends are explicitly enabled. Each backend
    # self-registers in scripts/sources/. ---
    parser.add_argument("--streaming-kinds", type=str, default="",
                        help="Comma-separated streaming-source kinds to enable "
                             "(e.g. 'wikipedia,investopedia,arxiv,gutenberg_stream'). "
                             "Empty = none. Backends not yet registered are "
                             "silently skipped.")
    parser.add_argument("--streaming-chunk-chars", type=int, default=1200,
                        help="Max characters per streaming chunk (default: 1200)")
    parser.add_argument("--streaming-max-chunks-per-stage", type=int, default=0,
                        help="Cap streaming chunks per stage priming pass per kind "
                             "(0 = unlimited; positive caps for fast smoke runs)")
    parser.add_argument("--streaming-restart", action="store_true",
                        help="Delete .streaming_corpus_state.json before starting")
    # CE-5/11/12/13/14 — STEM + finance + code corpus toggles. All default ON.
    parser.add_argument("--physics-corpus", dest="physics_corpus",
                        action="store_true", default=True)
    parser.add_argument("--no-physics-corpus", dest="physics_corpus",
                        action="store_false")
    parser.add_argument("--chemistry-corpus", dest="chemistry_corpus",
                        action="store_true", default=True)
    parser.add_argument("--no-chemistry-corpus", dest="chemistry_corpus",
                        action="store_false")
    parser.add_argument("--biology-corpus", dest="biology_corpus",
                        action="store_true", default=True)
    parser.add_argument("--no-biology-corpus", dest="biology_corpus",
                        action="store_false")
    parser.add_argument("--finance-corpus", dest="finance_corpus",
                        action="store_true", default=True)
    parser.add_argument("--no-finance-corpus", dest="finance_corpus",
                        action="store_false")
    parser.add_argument("--code-corpus", dest="code_corpus",
                        action="store_true", default=True)
    parser.add_argument("--no-code-corpus", dest="code_corpus",
                        action="store_false")
    # CE-1..3, 6..10, 19 probe drips. All default ON; --no-probe-drips
    # disables them all (e.g., for fast smoke runs).
    parser.add_argument("--probe-drips", dest="probe_drips",
                        action="store_true", default=True)
    parser.add_argument("--no-probe-drips", dest="probe_drips",
                        action="store_false")
    args = parser.parse_args()

    # Publish probe-drips toggle so the module-level run_stage_* functions can
    # gate _run_probe_drips without referencing main()'s local `args` (which
    # raised NameError every 200 steps and crash-looped the trainer).
    global _PROBE_DRIPS_ENABLED
    _PROBE_DRIPS_ENABLED = bool(getattr(args, "probe_drips", True))

    # Publish canonical-corpus config so drip hooks inside run_stage_N can
    # read it without threading args through every signature.
    global _CANONICAL_CFG
    _CANONICAL_CFG = {
        "enabled": bool(getattr(args, "canonical_corpus", True)),
        "root": getattr(args, "canonical_corpus_root", "data/canonical_corpus"),
        "chunk_chars": int(getattr(args, "canonical_chunk_chars", 1200)),
        "max_chunks_per_stage": int(getattr(args, "canonical_max_chunks_per_stage", 0) or 0),
    }

    # --canonical-restart: wipe the resume-state file before starting.
    if getattr(args, "canonical_restart", False):
        _crp = _canonical_state_path(CHECKPOINT_DIR)
        if os.path.exists(_crp):
            try:
                os.remove(_crp)
                print(f"  [Canonical] --canonical-restart: removed {_crp}")
            except Exception as _e:
                print(f"  [Canonical] --canonical-restart: failed to remove {_crp}: {_e}")

    # Publish math-corpus config so drip hooks inside run_stage_N can
    # read it without threading args through every signature.
    global _MATH_CFG
    _MATH_CFG = {
        "enabled": bool(getattr(args, "math_corpus", True)),
        "root": getattr(args, "math_corpus_root", "data/math_corpus"),
        "chunk_chars": int(getattr(args, "math_chunk_chars", 1200)),
        "max_chunks_per_stage": int(getattr(args, "math_max_chunks_per_stage", 0) or 0),
    }

    # --math-restart: wipe the math resume-state file before starting.
    if getattr(args, "math_restart", False):
        _mrp = _canonical_state_path(CHECKPOINT_DIR, MATH_STATE_FILENAME)
        if os.path.exists(_mrp):
            try:
                os.remove(_mrp)
                print(f"  [Math] --math-restart: removed {_mrp}")
            except Exception as _e:
                print(f"  [Math] --math-restart: failed to remove {_mrp}: {_e}")

    # CE-11/12/13/14/5 — publish per-corpus configs. They all share the
    # canonical-corpus dispatcher so the cfg shape mirrors _MATH_CFG.
    global _PHYSICS_CFG, _CHEMISTRY_CFG, _BIOLOGY_CFG, _FINANCE_CFG, _CODE_CFG
    _PHYSICS_CFG = {"enabled": bool(getattr(args, "physics_corpus", True)),
                    "root": "data/physics_corpus",
                    "chunk_chars": 1200,
                    "max_chunks_per_stage": 0}
    _CHEMISTRY_CFG = {"enabled": bool(getattr(args, "chemistry_corpus", True)),
                      "root": "data/chemistry_corpus",
                      "chunk_chars": 1200,
                      "max_chunks_per_stage": 0}
    _BIOLOGY_CFG = {"enabled": bool(getattr(args, "biology_corpus", True)),
                    "root": "data/biology_corpus",
                    "chunk_chars": 1200,
                    "max_chunks_per_stage": 0}
    _FINANCE_CFG = {"enabled": bool(getattr(args, "finance_corpus", True)),
                    "root": "data/finance_corpus",
                    "chunk_chars": 1200,
                    "max_chunks_per_stage": 0}
    _CODE_CFG = {"enabled": bool(getattr(args, "code_corpus", True)),
                 "root": "data/code_corpus",
                 "chunk_chars": 1500,  # code chunks slightly larger
                 "max_chunks_per_stage": 0}

    # Publish streaming-corpus config. Kinds is a comma-separated list of
    # registered backend kinds; empty means streaming is disabled. Backends
    # never persist source content to disk — only resume positions.
    global _STREAMING_CFG
    _streaming_kinds_raw = (getattr(args, "streaming_kinds", "") or "").strip()
    _streaming_kinds = [k.strip() for k in _streaming_kinds_raw.split(",") if k.strip()]
    _STREAMING_CFG = {
        "enabled": bool(_streaming_kinds),
        "kinds": _streaming_kinds,
        "chunk_chars": int(getattr(args, "streaming_chunk_chars", 1200)),
        "max_chunks_per_stage": int(getattr(args, "streaming_max_chunks_per_stage", 0) or 0),
    }

    # --streaming-restart: wipe the streaming resume-state file before starting.
    if getattr(args, "streaming_restart", False):
        try:
            from streaming_ingest import streaming_state_path as _stream_state_path
            _srp = _stream_state_path(CHECKPOINT_DIR)
        except Exception:
            _srp = os.path.join(CHECKPOINT_DIR, ".streaming_corpus_state.json")
        if os.path.exists(_srp):
            try:
                os.remove(_srp)
                print(f"  [Stream] --streaming-restart: removed {_srp}")
            except Exception as _e:
                print(f"  [Stream] --streaming-restart: failed to remove {_srp}: {_e}")

    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s [%(name)s] %(message)s")

    _kill_stale_processes()

    # === STARTUP ORPHAN CLEANUP ===
    # If a previous run crashed mid-save, .tmp / .tmp.* sidecar files
    # accumulate (each .tmp.snn is ~10 GB). The in-process error path
    # only fires on graceful exception — process kills/SIGKILL leave
    # them behind. Sweep them at startup to recover disk.
    _startup_cleanup_orphan_tmps()

    # --- Load or create brain ---
    checkpoint_path = args.checkpoint
    if args.resume and not checkpoint_path:
        default_ckpt = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
        if os.path.exists(default_ckpt):
            checkpoint_path = default_ckpt
    if args.fresh:
        checkpoint_path = None

    print("\n" + "=" * 60)
    print("  ATHENA — Immersive Developmental Learning")
    print("  Every biological subsystem active. No shortcuts.")
    print("=" * 60)

    # Clean up old checkpoints on fresh start to free disk space
    if args.fresh:
        _cleanup_old_checkpoints(keep_current=False)
    else:
        _cleanup_old_checkpoints(keep_current=True)

    if checkpoint_path and os.path.exists(checkpoint_path):
        print(f"  Loading from checkpoint: {checkpoint_path}")
    else:
        print("  Creating fresh brain (FULL init — all biological subsystems)")
        checkpoint_path = None

    # --- Pre-generate Claude content BEFORE brain init (brain uses 52GB+) ---
    if not args.no_claude:
        _pre_generate_all_stages(args, force_fresh=args.fresh)

    # --- Download multimodal datasets BEFORE brain init (needs free RAM) ---
    multimodal_loader = MultimodalDataLoader()
    if not args.no_multimodal:
        multimodal_loader.ensure_downloaded()
    else:
        print("  [Multimodal] Disabled (--no-multimodal)")

    # Restore CUDA visibility so nimcp's gpu_detect sees the real GPU.
    # PyTorch is already imported with CUDA disabled; restoring the env
    # var won't make PyTorch re-acquire GPU context.
    if _saved_cuda_vis is not None:
        os.environ["CUDA_VISIBLE_DEVICES"] = _saved_cuda_vis
    elif "CUDA_VISIBLE_DEVICES" in os.environ:
        del os.environ["CUDA_VISIBLE_DEVICES"]

    if args.daemon:
        # Connect to running brain daemon — wait for it to be ready
        from brain_client import BrainProxy, is_daemon_running
        import time as _time
        max_wait = 900  # 15 min max (full brain init can take 14 min)
        waited = 0
        while not is_daemon_running() and waited < max_wait:
            if waited == 0:
                print("  Waiting for brain daemon to become ready...")
            _time.sleep(10)
            waited += 10
            if waited % 60 == 0:
                print(f"  ... still waiting ({waited}s)")
        if not is_daemon_running():
            print("  ERROR: Brain daemon not ready after 15 minutes!")
            print("  Start it with: sudo systemctl start athena-brain")
            sys.exit(1)
        brain = BrainProxy()
        print(f"  Connected to brain daemon (neurons={brain.get_neuron_count():,})")
    elif checkpoint_path and os.path.exists(checkpoint_path):
        # Resume: load full brain state from checkpoint (avoids 52GB+ fresh init)
        print(f"  Loading brain from checkpoint: {checkpoint_path}")
        brain = nimcp.Brain.load(checkpoint_path)
    else:
        # Fresh start: create brain from scratch (requires ~52GB+ RAM)
        brain = nimcp.Brain("athena",
                            num_inputs=args.num_inputs,
                            num_outputs=args.num_outputs,
                            neuron_count=args.neuron_count,
                            init_mode='full',
                            snn_neuron_count=args.snn_neuron_count,
                            lnn_neuron_count=args.lnn_neuron_count)

    # Sync BRAIN_INPUT_DIM / BRAIN_OUTPUT_DIM with the brain's *actual* dims.
    # On --resume the checkpoint may have layer_sizes[last] != args.num_outputs
    # (persistence restores dims from network layers — the ground truth).
    # Without this, make_semantic_target() tiles targets to the stale 4096
    # default and brain_learn_vector truncates every step (lost loss signal).
    try:
        _probe = brain.probe()
        _actual_in = int(_probe.get("num_inputs") or 0)
        _actual_out = int(_probe.get("num_outputs") or 0)
        if _actual_in and _actual_in != BRAIN_INPUT_DIM:
            print(f"  BRAIN_INPUT_DIM: {BRAIN_INPUT_DIM} -> {_actual_in} (from brain)")
            BRAIN_INPUT_DIM = _actual_in
        if _actual_out and _actual_out != BRAIN_OUTPUT_DIM:
            print(f"  BRAIN_OUTPUT_DIM: {BRAIN_OUTPUT_DIM} -> {_actual_out} (from brain)")
            BRAIN_OUTPUT_DIM = _actual_out
    except Exception as _dim_err:
        print(f"  Warning: could not probe brain dims ({_dim_err}) — using defaults")

    # --- Training integration (Phase A-E modules) ---
    # Wraps stimulus selection, activates symbolic writers, applies innate
    # priors at init, and runs continuous memory consolidation throughout
    # all stages. Each component is individually disable-able via flags.
    # Stored as module-level global so stage functions can reference it.
    global _training_integration
    try:
        from training_integration import TrainingIntegration
        _training_integration = TrainingIntegration(brain)
        _priors_summary = _training_integration.apply_innate_priors()
        print(f"  [Integration] Innate priors: {_priors_summary.get('applied', [])}")
    except Exception as _integ_err:
        print(f"  [Integration] Setup failed ({_integ_err}) — continuing without")
        _training_integration = None

    # --- Brain configuration ---
    # In daemon mode, the brain was already configured when first created.
    # These calls are idempotent but some (lnn_create, enable_multi_network)
    # may fail if already initialized — wrap in try/except.
    if args.daemon:
        # Full config via daemon — same as non-daemon but with try/except
        try:
            brain.set_fast_training(False)
            brain.set_task_type("regression")
            brain.enable_biological_plasticity(True)
        except Exception as e:
            print(f"  Daemon config (basic): {e} (non-fatal)")
        for label, fn in [
            ("Mixed precision", lambda: brain.enable_mixed_precision(True)),
            ("Gradient checkpointing", lambda: brain.enable_gradient_checkpointing(False, 0)),
            ("World model", lambda: brain.enable_world_model(True)),
            ("LNN", lambda: brain.lnn_create(128, 64, 32, 64)),
            ("Multi-network UTM", lambda: brain.enable_multi_network()),
        ]:
            try:
                fn()
            except Exception as e:
                print(f"  {label}: {e} (non-fatal)")

        # Initialize all 4 cortex CNN processors (visual/audio/speech/somato + FNO).
        # Direct API call — no sensory data or learning required.
        try:
            brain.init_cortex_cnns()
            ccm = brain.get_cortex_cnn_metrics()
            active = sorted(ccm.keys()) if ccm else []
            print(f"  Cortex CNNs: {active if active else 'NONE'}", flush=True)
        except Exception as e:
            print(f"  Cortex CNN init: {e} (non-fatal)", flush=True)

        print("  Daemon mode: training config applied", flush=True)
    else:
        # --- CRITICAL: Disable fast training → ALL bio subsystems active ---
        brain.set_fast_training(False)
        print("  Fast training: OFF (full biological pipeline)")

        # --- CRITICAL: Use regression strategy (raw output, no softmax) ---
        brain.set_task_type("regression")
        print("  Task type: REGRESSION (raw output, no softmax)")

        # --- Enable mixed precision (FP16) for ~2x GPU throughput ---
        try:
            brain.enable_mixed_precision(True)
            print("  Mixed precision: ON (FP16 compute, FP32 accumulation)")
        except Exception as e:
            print(f"  Mixed precision: FAILED ({e})")

        # --- Enable gradient checkpointing for memory-efficient training ---
        try:
            brain.enable_gradient_checkpointing(False, 0)
            print("  Gradient checkpointing: OFF (avoiding stack smash in fresh init)")
        except Exception as e:
            print(f"  Gradient checkpointing: FAILED ({e})")

        # --- Enable biological plasticity ---
        try:
            brain.enable_biological_plasticity(True)
            print("  Biological plasticity: ON (TPB + EDP + coordinator)")
        except Exception as e:
            print(f"  Biological plasticity: FAILED ({e})")

        # --- Enable world model ---
        try:
            brain.enable_world_model(True)
            print("  World model: ON (JEPA + RSSM)")
        except Exception as e:
            print(f"  World model: FAILED ({e})")

        # --- Create LNN temporal processor ---
        try:
            brain.lnn_create(128, 64, 32, 64)
            print("  LNN temporal processor: ON (128→64→32→64)")
        except Exception as e:
            print(f"  LNN: FAILED ({e})")

        # --- Enable multi-network unified training (UTM) ---
        try:
            brain.enable_multi_network()
            print("  Multi-network UTM: ON (4 networks, unified optimizer)")
        except Exception as e:
            print(f"  Multi-network UTM: FAILED ({e})")

    # --- Initialize reasoning KB ---
    try:
        brain.ti_init_reasoning()
        print("  Reasoning KB: ON")
    except Exception:
        pass

    # --- Initialize edge subsystems on resume ---
    # When resuming from checkpoint, the edge subsystems (sensor hub, watchdog,
    # drone bridges) weren't in the old checkpoint. Initialize them now.
    # These are idempotent — if they already exist they'll fail gracefully.
    _edge_seed_flag = os.path.join(CHECKPOINT_DIR, ".edge_subsystems_seeded")
    if checkpoint_path and os.path.exists(checkpoint_path) and not os.path.exists(_edge_seed_flag):
        print("  [Edge Init] Initializing new edge subsystems on resumed brain...")
        edge_inits = [
            ("Sensor hub", lambda: brain.sensor_hub_create(32)),
            ("Safety watchdog", lambda: brain.watchdog_create({})),
        ]
        for label, fn in edge_inits:
            try:
                fn()
                print(f"    {label}: OK")
            except Exception as e:
                print(f"    {label}: {e} (non-fatal)")
        try:
            with open(_edge_seed_flag, "w") as f:
                json.dump({"timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                           "step": start_from if 'start_from' in dir() else 0}, f)
        except Exception:
            pass
        print("  [Edge Init] Edge subsystems initialized")

    # --- Set up components ---
    teacher = None
    if not args.no_claude:
        try:
            teacher = ClaudeTeacher()
            print("  Claude parent: ON")
        except Exception as e:
            print(f"  Claude parent: OFF ({e})")

    # --- Neural decoder (for observing Athena's responses, not scoring them) ---
    decoder = None
    try:
        if os.path.exists(DECODER_DIR) and not args.fresh:
            decoder = NeuralDecoder.load(DECODER_DIR)
            # Verify embed_dim matches current model (1024-dim BAAI/bge-large-en-v1.5)
            if decoder.embed_dim != 1024:
                print(f"  Decoder embed_dim mismatch ({decoder.embed_dim} != 1024), recreating")
                decoder = NeuralDecoder(output_dim=args.num_outputs, embed_dim=1024)
        else:
            decoder = NeuralDecoder(output_dim=args.num_outputs, embed_dim=1024)
        print("  Neural decoder: ON")
    except Exception:
        decoder = NeuralDecoder(output_dim=args.num_outputs, embed_dim=1024)

    parent = Parent(teacher=teacher, enabled=not args.no_claude, decoder=decoder,
                    multimodal=multimodal_loader)
    composer = SensoryComposer(brain)
    clock = BiologicalClock(rest_interval=2000)
    source = StimulusSource()

    # --- Resume state ---
    start_stage = args.stage
    start_step = 0
    if args.resume:
        state = _load_state()
        if state:
            # Validate that the snapshot AND its critical sidecars exist.
            # A bare main .bin without its .snn (etc.) means brain loads
            # without spike network state — silent partial restore.
            snapshot = state.get("snapshot", "")
            snapshot_path = os.path.join(CHECKPOINT_DIR, snapshot) if snapshot else ""
            CRITICAL_SIDECARS = ['.snn', '.cnn', '.lnn']  # the "real" networks
            missing_sidecars = []
            if snapshot_path and os.path.exists(snapshot_path):
                for ext in CRITICAL_SIDECARS:
                    if not os.path.exists(snapshot_path + ext):
                        missing_sidecars.append(ext)
            if snapshot_path and os.path.exists(snapshot_path) and not missing_sidecars:
                start_stage = state.get("stage", args.stage)
                start_step = state.get("step", 0)
                print(f"  Resuming from stage {start_stage}, step {start_step} ({snapshot})")
            elif missing_sidecars:
                # Snapshot main .bin loaded fine; only the sidecar (e.g. .snn) is
                # missing — that's a known SNN-save MFS rename race, not a curriculum
                # regression. Trust the state-file bookmark; the affected network
                # restarts from default but the curriculum stage stays correct.
                start_stage = state.get("stage", args.stage)
                start_step = state.get("step", 0)
                print(f"  WARNING: {snapshot} exists but missing sidecars: "
                      f"{missing_sidecars} — keeping curriculum at "
                      f"stage {start_stage}, step {start_step} (affected network "
                      f"loads default state)")
            else:
                # State file points to pruned/missing snapshot — find latest.
                # Also check the canonical athena_immersive.bin which is
                # always the "latest" (may be symlink or regular file).
                import glob
                candidates = sorted(glob.glob(os.path.join(CHECKPOINT_DIR, "athena_s*_step*.bin")),
                                    key=os.path.getmtime, reverse=True)
                # Filter to real .bin files, not sidecars (.bin.snn etc.)
                candidates = [c for c in candidates
                              if c.endswith(".bin") and ".bin." not in os.path.basename(c)]

                # If no timestamped snapshots but canonical exists, trust the
                # state file's step/stage — the brain will load from
                # athena_immersive.bin regardless.
                canonical_path = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")
                if not candidates and os.path.exists(canonical_path):
                    start_stage = state.get("stage", args.stage)
                    start_step = state.get("step", 0)
                    print(f"  Resume: canonical {canonical_path} exists — "
                          f"using state file stage {start_stage}, step {start_step}")
                elif candidates:
                    # Parse stage/step from filename: athena_s{stage}_step{step}.bin
                    latest = os.path.basename(candidates[0])
                    import re
                    m = re.match(r'athena_s(\d+)_step(\d+)\.bin', latest)
                    if m:
                        start_stage = int(m.group(1))
                        start_step = int(m.group(2))
                        print(f"  State file stale — falling back to {latest} "
                              f"(stage {start_stage}, step {start_step})")
                    else:
                        print(f"  Resume: found {latest} but can't parse stage/step")
                else:
                    print(f"  Resume: no checkpoint files found, starting fresh")

    # --- Handle graceful shutdown ---
    shutdown_requested = [False]
    # Mutable training progress for signal handlers [stage, step]
    training_progress = [start_stage, 0]

    ipc_server = [None]  # mutable container for signal handler access

    def signal_handler(sig, frame):
        if shutdown_requested[0]:
            print("\n  Force quit.")
            sys.exit(1)
        shutdown_requested[0] = True
        print("\n  Graceful shutdown... saving checkpoint...")
        if ipc_server[0]:
            ipc_server[0].stop()
        _save_checkpoint(brain, decoder, training_progress[0], training_progress[1])
        # Wait for checkpoint save to complete before exiting
        if _checkpoint_thread is not None and _checkpoint_thread.is_alive():
            print("  Waiting for checkpoint save to complete...")
            _checkpoint_thread.join(timeout=600)  # 10 min max for 2M neuron save
            if _checkpoint_thread.is_alive():
                print("  WARNING: Checkpoint save timed out after 600s")
            else:
                print("  Checkpoint saved successfully.")
        sys.exit(0)

    def sigusr1_handler(sig, frame):
        stage, step = training_progress
        print(f"\n  [SIGUSR1] On-demand checkpoint requested (stage={stage}, step={step})...")
        _save_checkpoint(brain, decoder, stage, step)
        print("  [SIGUSR1] Checkpoint save initiated — training continues.")

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGUSR1, sigusr1_handler)

    # --- Initialize Phi-3 language cortex ---
    try:
        from phi3_decoder import Phi3Decoder
        phi3 = Phi3Decoder()
        if phi3.available:
            print("  Phi-3 language cortex: ON")
        else:
            phi3 = None
            print("  Phi-3 language cortex: OFF (model not found)")
    except Exception:
        phi3 = None

    # --- Start IPC server (allows talk_to_athena.py to query this brain) ---
    # In daemon mode, the brain daemon already provides IPC — don't bind the same socket.
    if not args.daemon:
        ipc_server[0] = AthenaIPCServer(brain, decoder, composer, training_progress,
                                         phi3=phi3)
        ipc_server[0].start()

    # --- Print initial bio stats ---
    print("\n  Initial biological state:")
    _print_bio_stats(brain)

    # --- Seed Athena's foundational identity (fresh start only) ---
    if not args.resume or start_step == 0:
        seed_athena_identity(brain, composer)

    # --- Retroactive memory seeding (resume with past training steps) ---
    # Only run once — skip if already seeded (flag file persists across restarts)
    _memory_seed_flag = os.path.join(CHECKPOINT_DIR, ".memory_seeded")
    if args.resume and start_step > 0 and not os.path.exists(_memory_seed_flag):
        retroactive_memory_seed(brain, composer, start_step)
        retroactive_tom_seed(brain, composer, start_step)
        try:
            with open(_memory_seed_flag, "w") as f:
                json.dump({"timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                           "step": start_step}, f)
        except Exception:
            pass
    elif os.path.exists(_memory_seed_flag):
        print("  [Memory Seed] Already completed — skipping")

    # --- Sensory enrichment (one-time, for existing text-trained brains) ---
    if not args.no_multimodal and not args.fresh:
        run_sensory_enrichment(brain, composer, parent, decoder)

    # --- Run developmental stages ---
    print(f"\n  Starting from stage {start_stage}")

    # Wrap stimulus source with curiosity + curriculum (no-op if integration
    # is unavailable). Keeps the source variable name stable for the rest
    # of the main flow.
    if _training_integration is not None:
        try:
            source = _training_integration.wrap_source(source)
            print("  [Integration] source wrapped with curiosity + curriculum")
        except Exception as _wrap_err:
            print(f"  [Integration] wrap_source failed ({_wrap_err})")

    if start_stage <= 0:
        training_progress[0] = 0
        if _training_integration is not None:
            _training_integration.begin_stage(0)
        run_stage_0(brain, composer, parent, clock, source, decoder,
                    num_stimuli=args.stage0_stimuli,
                    start_from=start_step if start_stage == 0 else 0)
        start_step = 0
        # Save checkpoint at stage transition so --resume skips completed stages
        _save_checkpoint(brain, decoder, stage=1, step=0)
        # Wait for the save to complete before declaring it — the prior
        # implementation said "saved" while the background thread was still
        # writing, so a crash in that window left state pointing nowhere.
        if _checkpoint_thread is not None and _checkpoint_thread.is_alive():
            _checkpoint_thread.join(timeout=300)
        print("  [Checkpoint] Stage 0 complete → saved as stage 1, step 0")

    if start_stage <= 1:
        training_progress[0], training_progress[1] = 1, 0
        if _training_integration is not None:
            _training_integration.begin_stage(1)
        if args.canonical_corpus:
            _ingest_canonical_corpus(
                brain, stage=1,
                corpus_root=args.canonical_corpus_root,
                chunk_chars=args.canonical_chunk_chars,
                max_chunks_per_call=(args.canonical_max_chunks_per_stage or None),
                checkpoint_dir=CHECKPOINT_DIR)
        # Math corpus is abstract symbolic content — gated to stage >= 2.
        # Stage 1 is "Look! That's a ___!" sensory grounding.
        if _STREAMING_CFG and _STREAMING_CFG.get("enabled"):
            for _kind in _STREAMING_CFG.get("kinds", []):
                _ingest_streaming_corpus(
                    brain, stage=1, kind=_kind,
                    chunk_chars=_STREAMING_CFG.get("chunk_chars", 1200),
                    max_chunks_per_call=(_STREAMING_CFG.get("max_chunks_per_stage") or None),
                    checkpoint_dir=CHECKPOINT_DIR)
        _run_extra_corpora_priming(brain, stage=1, checkpoint_dir=CHECKPOINT_DIR)
        run_stage_1(brain, composer, parent, clock, source, decoder,
                    num_stimuli=args.stage1_stimuli,
                    start_from=start_step if start_stage == 1 else 0)
        start_step = 0
        _save_checkpoint(brain, decoder, stage=2, step=0)
        if _checkpoint_thread is not None and _checkpoint_thread.is_alive():
            _checkpoint_thread.join(timeout=300)
        print("  [Checkpoint] Stage 1 complete → saved as stage 2, step 0")

    if start_stage <= 2:
        training_progress[0], training_progress[1] = 2, 0
        if _training_integration is not None:
            _training_integration.begin_stage(2)
        if args.canonical_corpus:
            _ingest_canonical_corpus(
                brain, stage=2,
                corpus_root=args.canonical_corpus_root,
                chunk_chars=args.canonical_chunk_chars,
                max_chunks_per_call=(args.canonical_max_chunks_per_stage or None),
                checkpoint_dir=CHECKPOINT_DIR)
        if getattr(args, "math_corpus", True):
            _ingest_math_corpus(
                brain, stage=2,
                corpus_root=args.math_corpus_root,
                chunk_chars=args.math_chunk_chars,
                max_chunks_per_call=(args.math_max_chunks_per_stage or None),
                checkpoint_dir=CHECKPOINT_DIR)
        if _STREAMING_CFG and _STREAMING_CFG.get("enabled"):
            for _kind in _STREAMING_CFG.get("kinds", []):
                _ingest_streaming_corpus(
                    brain, stage=2, kind=_kind,
                    chunk_chars=_STREAMING_CFG.get("chunk_chars", 1200),
                    max_chunks_per_call=(_STREAMING_CFG.get("max_chunks_per_stage") or None),
                    checkpoint_dir=CHECKPOINT_DIR)
        _run_extra_corpora_priming(brain, stage=2, checkpoint_dir=CHECKPOINT_DIR)
        stage2_losses = run_stage_2(
            brain, composer, parent, clock, source, decoder,
            num_stimuli=args.stage2_stimuli,
            start_from=start_step if start_stage == 2 else 0)
        if stage2_losses:
            print(f"\n  Stage 2 complete — final mean loss: "
                  f"{np.mean(stage2_losses[-500:]):.4f}")
        start_step = 0
        _save_checkpoint(brain, decoder, stage=3, step=0)
        if _checkpoint_thread is not None and _checkpoint_thread.is_alive():
            _checkpoint_thread.join(timeout=300)
        print("  [Checkpoint] Stage 2 complete → saved as stage 3, step 0")

    if start_stage <= 3:
        training_progress[0], training_progress[1] = 3, 0
        if _training_integration is not None:
            _training_integration.begin_stage(3)
        if args.canonical_corpus:
            _ingest_canonical_corpus(
                brain, stage=3,
                corpus_root=args.canonical_corpus_root,
                chunk_chars=args.canonical_chunk_chars,
                max_chunks_per_call=(args.canonical_max_chunks_per_stage or None),
                checkpoint_dir=CHECKPOINT_DIR)
        if getattr(args, "math_corpus", True):
            _ingest_math_corpus(
                brain, stage=3,
                corpus_root=args.math_corpus_root,
                chunk_chars=args.math_chunk_chars,
                max_chunks_per_call=(args.math_max_chunks_per_stage or None),
                checkpoint_dir=CHECKPOINT_DIR)
        if _STREAMING_CFG and _STREAMING_CFG.get("enabled"):
            for _kind in _STREAMING_CFG.get("kinds", []):
                _ingest_streaming_corpus(
                    brain, stage=3, kind=_kind,
                    chunk_chars=_STREAMING_CFG.get("chunk_chars", 1200),
                    max_chunks_per_call=(_STREAMING_CFG.get("max_chunks_per_stage") or None),
                    checkpoint_dir=CHECKPOINT_DIR)
        _run_extra_corpora_priming(brain, stage=3, checkpoint_dir=CHECKPOINT_DIR)
        run_stage_3(brain, composer, parent, clock, source, decoder,
                    num_interactions=args.stage3_interactions,
                    start_from=start_step if start_stage == 3 else 0)

    # --- Final checkpoint ---
    print("\n" + "=" * 60)
    print("  DEVELOPMENTAL TRAINING COMPLETE")
    print("=" * 60)
    _save_checkpoint(brain, decoder, stage=3, step=-1)

    # Final stats
    print("\n  Final biological state:")
    _print_bio_stats(brain)

    if parent.milestones:
        print(f"\n  Milestones celebrated: {len(parent.milestones)}")
        for m in parent.milestones[:10]:
            print(f"    - {m}")
        if len(parent.milestones) > 10:
            print(f"    ... and {len(parent.milestones) - 10} more")

    if parent.moral_lessons:
        print(f"  Moral lessons taught: {len(parent.moral_lessons)}")

    print(f"  Total interactions: {parent.interaction_count}")

    # Rotate spectral fold for next run
    try:
        fold_file = os.path.join(CHECKPOINT_DIR, ".spectral_fold")
        next_fold = (_current_fold + 1) % 5
        with open(fold_file, "w") as f:
            json.dump({"fold": next_fold, "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")}, f)
        print(f"  [Spectral CV] Fold rotated: {_current_fold} -> {next_fold} for next run")
    except Exception:
        pass

    print()


if __name__ == "__main__":
    main()
