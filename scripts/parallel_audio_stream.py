#!/usr/bin/env python3
"""
Parallel Audio Stream — Feeds real audio to Athena alongside text training.

Runs as a separate process. Connects to the brain daemon and submits
audio sensory data in parallel with the main immerse_athena.py training.

This gives Athena multimodal grounding — she hears sounds associated
with the concepts she's learning from text. Uses ESC-50 environmental
audio dataset + optional parentese-style TTS.

Usage:
    nohup python3 scripts/parallel_audio_stream.py > nohup_audio.log 2>&1 &
"""

import os
import sys
import time
import random
import numpy as np
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from brain_client import BrainProxy

# Audio processing
try:
    import librosa
    HAS_LIBROSA = True
except ImportError:
    HAS_LIBROSA = False
    print("[Audio] librosa not available — using synthetic audio")

# ESC-50 dataset mapping (class → description)
ESC50_CLASSES = {
    0: "dog barking", 1: "rain falling", 2: "sea waves", 3: "baby crying",
    4: "clock ticking", 5: "person sneezing", 6: "helicopter flying",
    7: "chainsaw running", 8: "rooster crowing", 9: "fire crackling",
    10: "hand saw cutting", 11: "train passing", 12: "church bells ringing",
    13: "airplane flying", 14: "mouse clicking", 15: "frog croaking",
    16: "crow calling", 17: "cat meowing", 18: "glass breaking",
    19: "door knocking", 20: "hen clucking", 21: "laughing",
    22: "pig oinking", 23: "insects buzzing", 24: "sheep bleating",
    25: "engine idling", 26: "pouring water", 27: "wind blowing",
    28: "toilet flushing", 29: "brushing teeth", 30: "vacuum cleaning",
    31: "clock alarm", 32: "car horn honking", 33: "washing machine",
    34: "siren wailing", 35: "cow mooing", 36: "footsteps walking",
    37: "breathing heavily", 38: "coughing", 39: "typing on keyboard",
    40: "drinking sipping", 41: "clapping hands", 42: "crickets chirping",
    43: "snoring", 44: "thunderstorm", 45: "water dripping",
    46: "birds singing", 47: "door creaking", 48: "owl hooting",
    49: "children playing",
}


def generate_synthetic_audio(description, sample_rate=16000, duration=1.0, variation=0):
    """Generate synthetic audio with per-instance variation.

    Each call with a different variation produces a unique sound within
    the same category — different pitch, timing, amplitude, and noise.
    This gives the audio cortex diverse training examples per concept.

    Maps description keywords to frequency/amplitude patterns that the
    audio cortex CNN can learn to distinguish.
    """
    t = np.linspace(0, duration, int(sample_rate * duration), dtype=np.float32)
    audio = np.zeros_like(t)

    # Per-instance variation: shift pitch, timing, amplitude
    rng = np.random.RandomState(abs(hash(description) + variation) % (2**32 - 1))
    pitch_shift = 1.0 + rng.uniform(-0.2, 0.2)    # ±20% pitch variation
    amp_scale = 0.5 + rng.uniform(0, 0.5)           # 50-100% amplitude
    time_offset = rng.uniform(0, 0.3)               # 0-300ms onset delay
    noise_level = rng.uniform(0.01, 0.05)            # Background noise

    desc_lower = description.lower()

    # Apply time offset
    t_shifted = np.clip(t - time_offset, 0, duration)

    # Map common sounds to frequency patterns (with variation)
    if any(w in desc_lower for w in ["bird", "sing", "chirp", "whistle"]):
        freq = (3000 + 1000 * np.sin(2 * np.pi * (8 * pitch_shift) * t_shifted)) * pitch_shift
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted)
        # Add harmonics for richness
        audio += amp_scale * 0.3 * np.sin(2 * np.pi * freq * 1.5 * t_shifted)

    elif any(w in desc_lower for w in ["thunder", "rumble", "boom", "crash"]):
        base_freq = 100 * pitch_shift
        audio += amp_scale * np.sin(2 * np.pi * base_freq * t_shifted) * np.exp(-3 * t_shifted)
        audio += amp_scale * 0.3 * np.sin(2 * np.pi * base_freq * 0.5 * t_shifted) * np.exp(-2 * t_shifted)

    elif any(w in desc_lower for w in ["rain", "water", "drip", "splash", "pour"]):
        noise = rng.randn(len(t)).astype(np.float32) * 0.2 * amp_scale
        cutoff = 0.7 + rng.uniform(0, 0.2)
        for i in range(1, len(noise)):
            noise[i] = cutoff * noise[i-1] + (1 - cutoff) * noise[i]
        audio += noise

    elif any(w in desc_lower for w in ["wind", "breeze", "gust", "blow"]):
        noise = rng.randn(len(t)).astype(np.float32) * 0.15 * amp_scale
        mod_freq = 0.5 * pitch_shift
        mod = 0.5 + 0.5 * np.sin(2 * np.pi * mod_freq * t_shifted)
        audio += noise * mod

    elif any(w in desc_lower for w in ["bell", "chime", "ring", "clock"]):
        f1 = 880 * pitch_shift
        audio += amp_scale * np.sin(2 * np.pi * f1 * t_shifted) * np.exp(-2 * t_shifted)
        audio += amp_scale * 0.4 * np.sin(2 * np.pi * f1 * 2 * t_shifted) * np.exp(-3 * t_shifted)
        audio += amp_scale * 0.2 * np.sin(2 * np.pi * f1 * 3 * t_shifted) * np.exp(-4 * t_shifted)

    elif any(w in desc_lower for w in ["dog", "bark", "woof"]):
        n_barks = rng.randint(2, 5)
        for b in range(n_barks):
            start = b * rng.uniform(0.15, 0.35)
            dur = rng.uniform(0.08, 0.2)
            mask = ((t_shifted >= start) & (t_shifted < start + dur)).astype(np.float32)
            audio += amp_scale * np.sin(2 * np.pi * 400 * pitch_shift * t_shifted) * mask

    elif any(w in desc_lower for w in ["cat", "purr", "meow"]):
        freq = (400 + 200 * t_shifted / duration) * pitch_shift
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted) * np.exp(-1.5 * t_shifted)

    elif any(w in desc_lower for w in ["music", "piano", "guitar", "melody"]):
        notes = [262, 294, 330, 349, 392, 440, 494, 523]
        rng.shuffle(notes)
        for i, f in enumerate(notes[:4]):
            start = i * 0.25
            mask = ((t_shifted >= start) & (t_shifted < start + 0.2)).astype(np.float32)
            audio += amp_scale * np.sin(2 * np.pi * f * pitch_shift * t_shifted) * mask

    elif any(w in desc_lower for w in ["engine", "motor", "machine", "idle"]):
        base = 80 * pitch_shift
        audio += amp_scale * 0.3 * np.sin(2 * np.pi * base * t_shifted)
        audio += amp_scale * 0.2 * np.sin(2 * np.pi * base * 2 * t_shifted)
        audio += amp_scale * 0.15 * np.sin(2 * np.pi * base * 3 * t_shifted)
        audio += rng.randn(len(t)).astype(np.float32) * 0.05 * amp_scale

    elif any(w in desc_lower for w in ["siren", "alarm", "horn"]):
        freq = 600 * pitch_shift + 400 * np.sin(2 * np.pi * 3 * t_shifted)
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted)

    elif any(w in desc_lower for w in ["cry", "baby", "sob", "wail"]):
        freq = (500 + 200 * np.sin(2 * np.pi * 5 * t_shifted)) * pitch_shift
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted)
        audio *= (0.5 + 0.5 * np.sin(2 * np.pi * 2 * t_shifted))

    elif any(w in desc_lower for w in ["laugh", "giggle", "chuckle"]):
        for b in range(rng.randint(3, 8)):
            start = b * rng.uniform(0.08, 0.15)
            dur = 0.06
            mask = ((t_shifted >= start) & (t_shifted < start + dur)).astype(np.float32)
            f = rng.uniform(250, 450) * pitch_shift
            audio += amp_scale * np.sin(2 * np.pi * f * t_shifted) * mask

    elif any(w in desc_lower for w in ["foot", "step", "walk", "run"]):
        for b in range(rng.randint(2, 6)):
            start = b * rng.uniform(0.2, 0.5)
            click = rng.randn(int(sample_rate * 0.02)).astype(np.float32) * amp_scale
            idx = int(start * sample_rate)
            if idx + len(click) < len(audio):
                audio[idx:idx+len(click)] += click

    elif any(w in desc_lower for w in ["type", "keyboard", "click", "tap"]):
        for b in range(rng.randint(5, 15)):
            idx = rng.randint(0, len(audio) - 100)
            click = rng.randn(80).astype(np.float32) * amp_scale * 0.3
            audio[idx:idx+80] += click

    elif any(w in desc_lower for w in ["breath", "snore", "cough", "sneeze"]):
        noise = rng.randn(len(t)).astype(np.float32) * 0.15 * amp_scale
        env = np.exp(-((t_shifted - 0.3)**2) / 0.05)
        audio += noise * env

    elif any(w in desc_lower for w in ["insect", "cricket", "buzz", "bee"]):
        freq = rng.uniform(4000, 6000) * pitch_shift
        audio += amp_scale * 0.2 * np.sin(2 * np.pi * freq * t_shifted)
        mod = 0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 20 * t_shifted))
        audio *= mod

    elif any(w in desc_lower for w in ["frog", "croak", "ribbit"]):
        for b in range(rng.randint(2, 5)):
            start = b * rng.uniform(0.2, 0.4)
            dur = rng.uniform(0.1, 0.2)
            mask = ((t_shifted >= start) & (t_shifted < start + dur)).astype(np.float32)
            freq = rng.uniform(200, 400) * pitch_shift
            audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted) * mask

    elif any(w in desc_lower for w in ["cow", "moo"]):
        freq = (150 + 30 * np.sin(2 * np.pi * 1.5 * t_shifted)) * pitch_shift
        env = np.exp(-0.5 * t_shifted) * (t_shifted > time_offset).astype(np.float32)
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted) * env

    elif any(w in desc_lower for w in ["rooster", "cock", "crow"]):
        freq = np.where(t_shifted < 0.3, 800 * pitch_shift,
                        np.where(t_shifted < 0.6, 1200 * pitch_shift, 600 * pitch_shift))
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted) * np.exp(-1.5 * t_shifted)

    elif any(w in desc_lower for w in ["clap", "applause", "hand"]):
        for b in range(rng.randint(3, 10)):
            idx = rng.randint(0, len(audio) - 200)
            burst = rng.randn(150).astype(np.float32) * amp_scale * 0.4
            burst *= np.exp(-np.linspace(0, 5, 150))
            audio[idx:idx+150] += burst

    elif any(w in desc_lower for w in ["door", "knock", "creak"]):
        for b in range(rng.randint(2, 4)):
            idx = int((b * 0.3 + time_offset) * sample_rate)
            if idx + 400 < len(audio):
                burst = np.sin(2 * np.pi * 300 * pitch_shift * np.linspace(0, 0.025, 400))
                burst *= np.exp(-np.linspace(0, 8, 400)) * amp_scale
                audio[idx:idx+400] += burst

    elif any(w in desc_lower for w in ["glass", "break", "shatter"]):
        noise = rng.randn(len(t)).astype(np.float32) * amp_scale
        audio += noise * np.exp(-5 * t_shifted) * (t_shifted > time_offset).astype(np.float32)
        audio += amp_scale * 0.3 * np.sin(2 * np.pi * 2000 * pitch_shift * t_shifted) * np.exp(-8 * t_shifted)

    elif any(w in desc_lower for w in ["silence", "quiet", "still"]):
        audio += rng.randn(len(t)).astype(np.float32) * 0.01

    elif any(w in desc_lower for w in ["helicopter", "airplane", "fly"]):
        blade_freq = rng.uniform(15, 25) * pitch_shift
        audio += amp_scale * 0.3 * np.sin(2 * np.pi * blade_freq * t_shifted)
        audio += rng.randn(len(t)).astype(np.float32) * 0.1 * amp_scale

    elif any(w in desc_lower for w in ["train", "rail"]):
        for b in range(10):
            start = b * 0.1
            mask = ((t_shifted >= start) & (t_shifted < start + 0.05)).astype(np.float32)
            audio += amp_scale * 0.3 * rng.randn(len(t)).astype(np.float32) * mask

    elif any(w in desc_lower for w in ["fire", "crackle", "flame"]):
        noise = rng.randn(len(t)).astype(np.float32) * 0.15 * amp_scale
        pops = rng.random(len(t)) > 0.998
        audio += noise + pops.astype(np.float32) * rng.uniform(0.2, 0.5) * amp_scale

    elif any(w in desc_lower for w in ["owl", "hoot"]):
        for b in range(rng.randint(1, 3)):
            start = b * 0.4 + time_offset
            freq = 400 * pitch_shift
            env = np.exp(-((t_shifted - start - 0.15)**2) / 0.01)
            audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted) * env

    elif any(w in desc_lower for w in ["sheep", "bleat", "baa"]):
        freq = (300 + 100 * np.sin(2 * np.pi * 8 * t_shifted)) * pitch_shift
        env = np.exp(-2 * t_shifted) * (t_shifted > time_offset).astype(np.float32)
        audio += amp_scale * np.sin(2 * np.pi * freq * t_shifted) * env

    elif any(w in desc_lower for w in ["pig", "oink", "squeal"]):
        freq = (600 + 200 * np.sin(2 * np.pi * 12 * t_shifted)) * pitch_shift
        audio += amp_scale * 0.4 * np.sin(2 * np.pi * freq * t_shifted) * np.exp(-3 * t_shifted)

    elif any(w in desc_lower for w in ["hen", "chicken", "cluck"]):
        for b in range(rng.randint(3, 7)):
            start = b * rng.uniform(0.1, 0.2)
            dur = 0.05
            mask = ((t_shifted >= start) & (t_shifted < start + dur)).astype(np.float32)
            audio += amp_scale * np.sin(2 * np.pi * 500 * pitch_shift * t_shifted) * mask

    elif any(w in desc_lower for w in ["mouse", "squeak"]):
        freq = rng.uniform(3000, 5000) * pitch_shift
        env = np.exp(-((t_shifted - 0.2)**2) / 0.005) * amp_scale
        audio += np.sin(2 * np.pi * freq * t_shifted) * env

    elif any(w in desc_lower for w in ["vacuum", "clean"]):
        noise = rng.randn(len(t)).astype(np.float32) * 0.2 * amp_scale
        audio += noise + amp_scale * 0.1 * np.sin(2 * np.pi * 120 * pitch_shift * t_shifted)

    elif any(w in desc_lower for w in ["wash", "machine", "laundry"]):
        mod = 0.5 + 0.5 * np.sin(2 * np.pi * 0.8 * t_shifted)
        audio += rng.randn(len(t)).astype(np.float32) * 0.1 * amp_scale * mod

    elif any(w in desc_lower for w in ["toilet", "flush"]):
        noise = rng.randn(len(t)).astype(np.float32) * amp_scale * 0.3
        env = np.where(t_shifted < 0.3, t_shifted / 0.3, np.exp(-2 * (t_shifted - 0.3)))
        audio += noise * env

    elif any(w in desc_lower for w in ["drink", "sip", "swallow"]):
        for b in range(rng.randint(1, 4)):
            start = b * 0.3 + time_offset
            noise = rng.randn(int(sample_rate * 0.1)).astype(np.float32) * amp_scale * 0.2
            idx = int(start * sample_rate)
            if idx + len(noise) < len(audio):
                audio[idx:idx+len(noise)] += noise

    else:
        # Default: unique ambient tone based on description + variation
        h = (hash(description) + variation) % 2000
        freq = 150 + h * 0.3
        audio += amp_scale * 0.15 * np.sin(2 * np.pi * freq * pitch_shift * t_shifted)
        audio += amp_scale * 0.08 * np.sin(2 * np.pi * freq * 1.5 * pitch_shift * t_shifted)

    # Add background noise (every sound has some)
    audio += rng.randn(len(t)).astype(np.float32) * noise_level

    # Normalize
    peak = np.max(np.abs(audio))
    if peak > 0:
        audio = audio / peak * 0.8

    return audio.tolist()


# ========================================================================
# PARENTESE (Infant-Directed Speech) — synthetic speech with exaggerated
# pitch contours, slower tempo, wider pitch range, and repetitive patterns
# that mirror how parents naturally speak to infants.
#
# Key acoustic features of parentese:
#   - Higher base pitch (~300 Hz vs ~200 Hz adult-directed)
#   - Wider pitch excursions (swooping contours, up to 2× range)
#   - Slower tempo (50-70% of adult rate)
#   - Longer vowel durations (hyper-articulation)
#   - More repetition and simpler vocabulary
#   - Exaggerated emotional prosody (happy, soothing, warning)
# ========================================================================

PARENTESE_UTTERANCES = [
    # Greetings & attention (high rising contour)
    {"text": "Hiii baby!", "emotion": "happy", "contour": "rise",
     "vowels": [(0.0, "i", 0.3), (0.4, "ei", 0.2), (0.7, "i", 0.3)]},
    {"text": "Hello there, sweetheart!", "emotion": "happy", "contour": "rise-fall",
     "vowels": [(0.0, "eh", 0.15), (0.15, "oh", 0.2), (0.5, "ee", 0.15), (0.7, "ah", 0.2)]},
    {"text": "Look at you!", "emotion": "happy", "contour": "rise",
     "vowels": [(0.0, "oo", 0.2), (0.3, "ah", 0.15), (0.6, "oo", 0.3)]},
    {"text": "Good morning, little one!", "emotion": "happy", "contour": "sing-song",
     "vowels": [(0.0, "oo", 0.15), (0.2, "or", 0.2), (0.5, "ih", 0.1), (0.7, "uh", 0.2)]},
    {"text": "There you are!", "emotion": "happy", "contour": "rise-fall",
     "vowels": [(0.0, "eh", 0.15), (0.3, "oo", 0.2), (0.6, "ah", 0.25)]},

    # Naming objects (slow, clear, repeated)
    {"text": "That's a ball! Ball! See the ball?", "emotion": "teaching", "contour": "rise",
     "vowels": [(0.0, "ah", 0.2), (0.2, "aw", 0.25), (0.5, "aw", 0.2), (0.75, "aw", 0.2)]},
    {"text": "Look, a doggy! Doggy!", "emotion": "happy", "contour": "rise-fall",
     "vowels": [(0.0, "oo", 0.15), (0.2, "aw", 0.2), (0.45, "ee", 0.15), (0.7, "aw", 0.15), (0.85, "ee", 0.15)]},
    {"text": "Where's the kitty? There's the kitty!", "emotion": "happy", "contour": "question-answer",
     "vowels": [(0.0, "eh", 0.1), (0.15, "ih", 0.2), (0.45, "eh", 0.1), (0.6, "ih", 0.2), (0.85, "ee", 0.15)]},
    {"text": "That's your nose! Nose! Boop!", "emotion": "playful", "contour": "rise",
     "vowels": [(0.0, "ah", 0.15), (0.2, "oh", 0.25), (0.5, "oh", 0.2), (0.75, "oo", 0.2)]},
    {"text": "See the flower? Pretty flower!", "emotion": "teaching", "contour": "rise-fall",
     "vowels": [(0.0, "ee", 0.15), (0.2, "ow", 0.2), (0.45, "er", 0.15), (0.65, "ih", 0.1), (0.8, "ow", 0.15)]},

    # Encouragement (warm, sustained, melodic)
    {"text": "Good job! You did it!", "emotion": "praise", "contour": "rise-fall",
     "vowels": [(0.0, "oo", 0.2), (0.25, "ah", 0.15), (0.5, "oo", 0.15), (0.7, "ih", 0.2)]},
    {"text": "Yaaay! So clever!", "emotion": "praise", "contour": "rise",
     "vowels": [(0.0, "ei", 0.35), (0.5, "oh", 0.15), (0.7, "eh", 0.15), (0.85, "er", 0.15)]},
    {"text": "That's wonderful!", "emotion": "praise", "contour": "rise-fall",
     "vowels": [(0.0, "ah", 0.15), (0.2, "uh", 0.2), (0.5, "er", 0.15), (0.7, "oo", 0.2)]},
    {"text": "You're so strong!", "emotion": "praise", "contour": "rise",
     "vowels": [(0.0, "oo", 0.2), (0.3, "oh", 0.2), (0.6, "aw", 0.25)]},
    {"text": "Very good! Very good!", "emotion": "praise", "contour": "sing-song",
     "vowels": [(0.0, "eh", 0.15), (0.2, "ee", 0.1), (0.35, "oo", 0.2), (0.6, "eh", 0.1), (0.75, "oo", 0.2)]},

    # Soothing / comfort (low, slow, falling contour)
    {"text": "Shhhh, it's okay", "emotion": "soothe", "contour": "fall",
     "vowels": [(0.0, "sh", 0.35), (0.4, "ih", 0.15), (0.6, "oh", 0.2), (0.8, "ei", 0.2)]},
    {"text": "There there, everything's alright", "emotion": "soothe", "contour": "fall",
     "vowels": [(0.0, "eh", 0.2), (0.25, "eh", 0.2), (0.5, "ee", 0.1), (0.65, "aw", 0.2), (0.85, "ai", 0.15)]},
    {"text": "Mama's here, don't worry", "emotion": "soothe", "contour": "fall",
     "vowels": [(0.0, "ah", 0.2), (0.25, "ah", 0.2), (0.5, "ee", 0.15), (0.7, "oh", 0.15), (0.85, "ee", 0.15)]},
    {"text": "You're safe, little one", "emotion": "soothe", "contour": "fall",
     "vowels": [(0.0, "oo", 0.15), (0.2, "ei", 0.2), (0.5, "ih", 0.15), (0.7, "uh", 0.2)]},
    {"text": "Rock-a-bye, sleepy baby", "emotion": "soothe", "contour": "sing-song",
     "vowels": [(0.0, "ah", 0.15), (0.15, "ah", 0.1), (0.3, "ai", 0.2), (0.55, "ee", 0.15), (0.75, "ei", 0.15), (0.9, "ee", 0.1)]},

    # Warning / prohibition (sharp, short, falling)
    {"text": "No no no!", "emotion": "warning", "contour": "sharp-fall",
     "vowels": [(0.0, "oh", 0.15), (0.2, "oh", 0.15), (0.4, "oh", 0.2)]},
    {"text": "Careful!", "emotion": "warning", "contour": "sharp-fall",
     "vowels": [(0.0, "eh", 0.15), (0.2, "oo", 0.2)]},
    {"text": "Hot! Don't touch!", "emotion": "warning", "contour": "sharp-fall",
     "vowels": [(0.0, "ah", 0.15), (0.2, "oh", 0.15), (0.4, "uh", 0.2)]},
    {"text": "Uh oh! Be gentle!", "emotion": "warning", "contour": "fall",
     "vowels": [(0.0, "uh", 0.15), (0.15, "oh", 0.15), (0.35, "ee", 0.15), (0.55, "eh", 0.15), (0.75, "uh", 0.15)]},
    {"text": "Stop! Wait!", "emotion": "warning", "contour": "sharp-fall",
     "vowels": [(0.0, "ah", 0.2), (0.3, "ei", 0.2)]},

    # Questions (exaggerated rising intonation)
    {"text": "What's that? Hmm?", "emotion": "curious", "contour": "rise",
     "vowels": [(0.0, "uh", 0.15), (0.2, "ah", 0.2), (0.5, "mm", 0.25)]},
    {"text": "Do you want milk?", "emotion": "curious", "contour": "rise",
     "vowels": [(0.0, "oo", 0.15), (0.2, "oo", 0.15), (0.4, "ah", 0.15), (0.6, "ih", 0.2)]},
    {"text": "Are you hungry?", "emotion": "curious", "contour": "rise",
     "vowels": [(0.0, "ah", 0.15), (0.2, "oo", 0.15), (0.4, "uh", 0.2), (0.65, "ee", 0.2)]},
    {"text": "Where did it go?", "emotion": "curious", "contour": "rise",
     "vowels": [(0.0, "eh", 0.15), (0.2, "ih", 0.1), (0.35, "ih", 0.1), (0.5, "oh", 0.25)]},
    {"text": "Who's a good baby?", "emotion": "playful", "contour": "rise",
     "vowels": [(0.0, "oo", 0.2), (0.25, "ah", 0.15), (0.45, "oo", 0.15), (0.65, "ei", 0.15), (0.85, "ee", 0.15)]},

    # Narration / describing (melodic, varied)
    {"text": "The birdie is singing! Tweet tweet!", "emotion": "teaching", "contour": "sing-song",
     "vowels": [(0.0, "uh", 0.1), (0.15, "er", 0.15), (0.35, "ee", 0.1), (0.5, "ih", 0.15), (0.7, "ee", 0.15), (0.85, "ee", 0.15)]},
    {"text": "Splash! Water goes splash!", "emotion": "playful", "contour": "rise-fall",
     "vowels": [(0.0, "ah", 0.2), (0.3, "aw", 0.15), (0.5, "er", 0.1), (0.65, "oh", 0.1), (0.8, "ah", 0.2)]},
    {"text": "Up up up you go!", "emotion": "playful", "contour": "rise",
     "vowels": [(0.0, "uh", 0.15), (0.2, "uh", 0.15), (0.4, "uh", 0.15), (0.6, "oo", 0.15), (0.8, "oh", 0.2)]},
    {"text": "Round and round we go!", "emotion": "playful", "contour": "sing-song",
     "vowels": [(0.0, "ow", 0.2), (0.25, "ah", 0.1), (0.4, "ow", 0.2), (0.65, "ee", 0.1), (0.8, "oh", 0.2)]},
    {"text": "One, two, three!", "emotion": "teaching", "contour": "rise",
     "vowels": [(0.0, "uh", 0.2), (0.3, "oo", 0.2), (0.6, "ee", 0.3)]},

    # Emotional mirroring (exaggerated affect)
    {"text": "Aww, so cuuute!", "emotion": "happy", "contour": "rise-fall",
     "vowels": [(0.0, "aw", 0.25), (0.35, "oh", 0.1), (0.5, "oo", 0.3), (0.85, "oo", 0.15)]},
    {"text": "Ohhh, how beautiful!", "emotion": "happy", "contour": "rise-fall",
     "vowels": [(0.0, "oh", 0.3), (0.4, "ow", 0.15), (0.6, "oo", 0.2), (0.85, "oo", 0.15)]},
    {"text": "Wow! Amazing!", "emotion": "praise", "contour": "rise",
     "vowels": [(0.0, "ow", 0.25), (0.35, "ah", 0.15), (0.55, "ei", 0.2), (0.8, "ih", 0.15)]},
    {"text": "Uh oh, did you fall down?", "emotion": "soothe", "contour": "fall-rise",
     "vowels": [(0.0, "uh", 0.15), (0.15, "oh", 0.15), (0.35, "ih", 0.1), (0.5, "oo", 0.1), (0.65, "aw", 0.15), (0.85, "ow", 0.15)]},
    {"text": "Peek-a-boo! I see you!", "emotion": "playful", "contour": "rise-fall",
     "vowels": [(0.0, "ee", 0.15), (0.15, "ah", 0.1), (0.3, "oo", 0.2), (0.55, "ai", 0.1), (0.7, "ee", 0.1), (0.85, "oo", 0.15)]},

    # Feeding time
    {"text": "Mmm, yummy! Open wide!", "emotion": "happy", "contour": "rise-fall",
     "vowels": [(0.0, "mm", 0.2), (0.25, "uh", 0.15), (0.45, "ee", 0.15), (0.65, "oh", 0.15), (0.8, "ai", 0.2)]},
    {"text": "Here comes the spoon! Aaahh!", "emotion": "playful", "contour": "rise",
     "vowels": [(0.0, "ee", 0.15), (0.2, "uh", 0.1), (0.35, "uh", 0.15), (0.55, "oo", 0.2), (0.8, "ah", 0.2)]},
    {"text": "All done! Good eating!", "emotion": "praise", "contour": "fall",
     "vowels": [(0.0, "aw", 0.15), (0.2, "uh", 0.15), (0.4, "oo", 0.15), (0.6, "ee", 0.15), (0.8, "ih", 0.15)]},

    # Bedtime
    {"text": "Time for sleepy bye-bye", "emotion": "soothe", "contour": "fall",
     "vowels": [(0.0, "ai", 0.15), (0.2, "or", 0.1), (0.35, "ee", 0.2), (0.6, "ee", 0.1), (0.75, "ai", 0.15), (0.9, "ai", 0.1)]},
    {"text": "Close your eyes, sweet dreams", "emotion": "soothe", "contour": "fall",
     "vowels": [(0.0, "oh", 0.15), (0.2, "or", 0.1), (0.35, "ai", 0.2), (0.6, "ee", 0.15), (0.8, "ee", 0.2)]},
    {"text": "Twinkle twinkle little star", "emotion": "soothe", "contour": "sing-song",
     "vowels": [(0.0, "ih", 0.15), (0.15, "uh", 0.1), (0.3, "ih", 0.15), (0.45, "uh", 0.1), (0.6, "ih", 0.15), (0.8, "ah", 0.2)]},
]

# Vowel formant table — F1/F2/F3 frequencies (Hz) for synthesizing vowels
# These approximate average female (parent) vocal tract resonances
VOWEL_FORMANTS = {
    "ah": (850, 1200, 2800),   # as in "father"
    "eh": (550, 1800, 2600),   # as in "bed"
    "ee": (300, 2300, 3000),   # as in "see"
    "ih": (400, 2000, 2600),   # as in "sit"
    "oh": (500, 900, 2600),    # as in "go"
    "oo": (350, 800, 2500),    # as in "moon"
    "uh": (600, 1200, 2600),   # as in "but"
    "aw": (600, 1000, 2600),   # as in "saw"
    "ai": (700, 1200, 2800),   # as in "eye" (diphthong start)
    "ei": (500, 1900, 2700),   # as in "say"
    "ow": (650, 1100, 2700),   # as in "now"
    "or": (500, 1000, 2600),   # as in "more"
    "er": (500, 1400, 2500),   # as in "her"
    "mm": (250, 1000, 2500),   # nasal hum
    "sh": (0, 0, 0),           # noise-based (sibilant)
}


def generate_parentese_audio(utterance, sample_rate=16000, duration=1.5, variation=0):
    """Synthesize infant-directed speech (parentese) with formant synthesis.

    Parentese characteristics:
      - Base pitch ~280-350 Hz (higher than normal adult ~180-220 Hz)
      - Wide pitch excursions (±100 Hz, vs ±30 Hz in adult-directed)
      - Slower vowel transitions, elongated vowels
      - Exaggerated amplitude envelope (louder on stressed syllables)
      - More harmonic richness (breathier quality)
    """
    rng = np.random.RandomState(abs(hash(utterance["text"]) + variation) % (2**32 - 1))
    t = np.linspace(0, duration, int(sample_rate * duration), dtype=np.float32)
    audio = np.zeros_like(t)

    # Base pitch depends on emotion
    emotion = utterance.get("emotion", "happy")
    if emotion == "soothe":
        base_f0 = 250 + rng.uniform(-20, 20)  # Lower, calmer
    elif emotion == "warning":
        base_f0 = 320 + rng.uniform(-10, 10)  # Sharp, attention-grabbing
    elif emotion == "praise":
        base_f0 = 340 + rng.uniform(-15, 15)  # High, excited
    elif emotion == "curious":
        base_f0 = 300 + rng.uniform(-10, 10)  # Mid-high
    elif emotion == "playful":
        base_f0 = 330 + rng.uniform(-15, 15)  # Bouncy
    else:  # happy, teaching
        base_f0 = 310 + rng.uniform(-20, 20)

    # Per-instance variation
    base_f0 *= (1.0 + rng.uniform(-0.08, 0.08))

    # Build pitch contour (the "melody" of parentese)
    contour = utterance.get("contour", "rise-fall")
    f0_contour = np.full_like(t, base_f0)

    if contour == "rise":
        # Steadily rising — questions, excitement
        f0_contour += np.linspace(0, 120, len(t)) + 30 * np.sin(2 * np.pi * 3 * t / duration)
    elif contour == "fall":
        # Falling — soothing, statements
        f0_contour += np.linspace(40, -80, len(t)) + 20 * np.sin(2 * np.pi * 2 * t / duration)
    elif contour == "rise-fall":
        # Arch shape — most common parentese contour
        peak = 0.4 + rng.uniform(-0.1, 0.1)
        f0_contour += 120 * np.sin(np.pi * t / duration) + 25 * np.sin(2 * np.pi * 4 * t / duration)
    elif contour == "sharp-fall":
        # Quick drop — warnings
        f0_contour += 80 * np.exp(-4 * t / duration)
    elif contour == "sing-song":
        # Oscillating — nursery rhyme style
        f0_contour += 80 * np.sin(2 * np.pi * 2.5 * t / duration) + 30 * np.sin(2 * np.pi * 5 * t / duration)
    elif contour == "question-answer":
        # Rise in first half, fall in second
        mid = len(t) // 2
        f0_contour[:mid] += np.linspace(0, 100, mid)
        f0_contour[mid:] += np.linspace(100, -20, len(t) - mid)
    elif contour == "fall-rise":
        f0_contour += -60 * np.cos(np.pi * t / duration) + 30

    # Add micro-jitter (natural voice quality)
    f0_contour += rng.randn(len(t)).astype(np.float32) * 3.0

    # Ensure no negative frequencies
    f0_contour = np.clip(f0_contour, 80, 600)

    # Synthesize vowel segments using formant synthesis
    vowels = utterance.get("vowels", [])
    amplitude_env = np.zeros_like(t)

    for start_frac, vowel, dur_frac in vowels:
        start_idx = int(start_frac * len(t))
        dur_samples = int(dur_frac * len(t))
        end_idx = min(start_idx + dur_samples, len(t))
        if start_idx >= len(t):
            continue

        # Get formants for this vowel
        f1, f2, f3 = VOWEL_FORMANTS.get(vowel, (500, 1500, 2500))

        seg_t = t[start_idx:end_idx]
        seg_f0 = f0_contour[start_idx:end_idx]

        if vowel == "sh":
            # Sibilant noise
            seg_audio = rng.randn(end_idx - start_idx).astype(np.float32) * 0.15
            # High-pass character
            for i in range(1, len(seg_audio)):
                seg_audio[i] = 0.95 * seg_audio[i] - 0.85 * (seg_audio[i-1] if i > 0 else 0)
        elif vowel == "mm":
            # Nasal hum — just fundamental + gentle harmonics
            phase = np.cumsum(seg_f0 / sample_rate) * 2 * np.pi
            seg_audio = 0.6 * np.sin(phase) + 0.2 * np.sin(2 * phase) + 0.1 * np.sin(3 * phase)
        else:
            # Voiced vowel — glottal source + formant filtering
            phase = np.cumsum(seg_f0 / sample_rate) * 2 * np.pi

            # Glottal pulse train (source)
            source = np.zeros(end_idx - start_idx, dtype=np.float32)
            for h in range(1, 8):  # 7 harmonics
                amp = 1.0 / (h * h)  # -12 dB/octave rolloff
                source += amp * np.sin(h * phase).astype(np.float32)

            # Add breathiness (aspiration noise modulated by F0)
            breath = rng.randn(end_idx - start_idx).astype(np.float32) * 0.08
            source += breath

            # Simple formant resonance via additive synthesis
            # (True formant filtering would use IIR filters, but additive
            # approximation works well for training audio features)
            seg_audio = np.zeros(end_idx - start_idx, dtype=np.float32)
            for formant_freq, bandwidth, gain in [
                (f1, 80, 1.0), (f2, 100, 0.7), (f3, 120, 0.3)
            ]:
                if formant_freq > 0:
                    formant_phase = 2 * np.pi * formant_freq * seg_t
                    resonance = gain * np.sin(formant_phase).astype(np.float32)
                    # Modulate by source
                    seg_audio += resonance * np.abs(source)

        # Amplitude envelope: smooth onset/offset
        env = np.ones(end_idx - start_idx, dtype=np.float32)
        attack = min(int(0.02 * sample_rate), len(env) // 4)
        release = min(int(0.03 * sample_rate), len(env) // 4)
        if attack > 0:
            env[:attack] = np.linspace(0, 1, attack)
        if release > 0:
            env[-release:] = np.linspace(1, 0, release)

        # Emotion-based amplitude
        if emotion == "praise":
            env *= 1.2
        elif emotion == "soothe":
            env *= 0.6
        elif emotion == "warning":
            env *= 1.4

        audio[start_idx:end_idx] += seg_audio * env
        amplitude_env[start_idx:end_idx] = env

    # Add subtle background warmth (room tone)
    audio += rng.randn(len(t)).astype(np.float32) * 0.005

    # Normalize
    peak = np.max(np.abs(audio))
    if peak > 0:
        audio = audio / peak * 0.7

    return audio.tolist()


def audio_to_mel_features(audio_samples, n_mels=64, n_fft=512):
    """Convert audio samples to mel-spectrogram features for the audio cortex."""
    audio = np.array(audio_samples, dtype=np.float32)

    if HAS_LIBROSA:
        mel = librosa.feature.melspectrogram(y=audio, sr=16000,
                                              n_mels=n_mels, n_fft=n_fft)
        mel_db = librosa.power_to_db(mel, ref=np.max)
        # Flatten to 1D feature vector
        return mel_db.flatten()[:1024].tolist()
    else:
        # Simple FFT-based approximation
        n_frames = len(audio) // (n_fft // 2)
        features = []
        for i in range(min(n_frames, 16)):
            start = i * (n_fft // 2)
            frame = audio[start:start + n_fft]
            if len(frame) < n_fft:
                frame = np.pad(frame, (0, n_fft - len(frame)))
            fft = np.abs(np.fft.rfft(frame))[:n_mels]
            features.extend(fft.tolist())
        # Pad/truncate to 1024
        features = features[:1024]
        features.extend([0.0] * (1024 - len(features)))
        return features


def get_training_descriptions():
    """Get current training descriptions from the content cache."""
    cache_path = "content_cache.json"
    if os.path.exists(cache_path):
        with open(cache_path) as f:
            cache = json.load(f)
        descriptions = []
        for stage_key in ["0", "1"]:
            if stage_key in cache:
                stage = cache[stage_key]
                if "_narrations" in stage:
                    descriptions.extend(stage["_narrations"])
                if "_encouragements" in stage:
                    descriptions.extend(stage["_encouragements"])
        return descriptions
    return []


def main():
    print("=" * 60)
    print("  PARALLEL AUDIO STREAM")
    print("  Feeding real audio to Athena alongside text training")
    print("=" * 60)

    # Connect to brain daemon
    brain = BrainProxy()
    print(f"  Connected to brain daemon")

    # Build expanded description set — each class gets multiple phrasings
    descriptions = []
    for cls_id, cls_name in ESC50_CLASSES.items():
        descriptions.append(cls_name)
        # Add variations of each class
        words = cls_name.split()
        if len(words) >= 2:
            descriptions.append(f"loud {cls_name}")
            descriptions.append(f"soft {cls_name}")
            descriptions.append(f"distant {cls_name}")
            descriptions.append(f"{cls_name} nearby")

    # Add categories the ESC-50 misses
    extra = [
        "engine running", "motor humming", "machine whirring",
        "traffic noise", "construction drilling", "car passing",
        "cooking sizzling", "pot boiling", "knife chopping",
        "forest ambience", "ocean waves crashing", "river flowing",
        "hail on roof", "heavy rain downpour", "light drizzle",
        "baby cooing", "child giggling", "adult sighing",
        "piano chord", "guitar strum", "drum beat", "violin bow",
        "crowd murmuring", "audience applause", "stadium cheering",
        "door slamming", "window opening", "key turning in lock",
        "paper rustling", "pencil writing", "book pages turning",
        "zipper closing", "velcro ripping", "cloth tearing",
        "ice cracking", "snow crunching", "gravel underfoot",
        "bubble popping", "balloon inflating", "cork popping",
        "heartbeat steady", "heartbeat fast", "breathing slow",
    ]
    descriptions.extend(extra)

    # Also pull from training cache if available
    cached = get_training_descriptions()
    if cached:
        descriptions.extend(cached[:100])

    print(f"  {len(descriptions)} audio descriptions available "
          f"(50 ESC-50 × ~5 variants + {len(extra)} extra + cache)")

    # Audio stream rate — throttled to avoid overloading daemon socket.
    # The training script uses the same socket at ~3 steps/sec.
    audio_hz = 0.2  # 1 audio frame every 5 seconds (safe for socket backlog)
    step_interval = 1.0 / audio_hz

    print(f"  {len(PARENTESE_UTTERANCES)} parentese utterances available")
    print(f"  Alternating: environmental sounds ↔ parentese speech")

    step = 0
    parentese_step = 0
    env_step = 0
    while True:
        # Alternate between environmental sounds and parentese speech
        # Pattern: 2 environmental, 1 parentese (2:1 ratio)
        use_parentese = (step % 3 == 2)

        if use_parentese:
            # --- Parentese (infant-directed speech) ---
            utterance = PARENTESE_UTTERANCES[parentese_step % len(PARENTESE_UTTERANCES)]
            audio_samples = generate_parentese_audio(utterance, variation=step)
            mel_features = audio_to_mel_features(audio_samples)

            # Atomic: submit sensory + learn in one batch
            try:
                brain._send({"cmd": "batch", "commands": [
                    {"cmd": "submit_sensory", "modality": "speech", "data": audio_samples[:4000]},
                    {"cmd": "submit_sensory", "modality": "audio", "data": mel_features},
                    {"cmd": "learn_vector",
                     "features": mel_features,
                     "target": mel_features,
                     "label": f"parentese:{utterance['text'][:30]}",
                     "confidence": 0.8,
                     "learning_rate": 0.0002}
                ]})
            except Exception as e:
                if step < 5:
                    print(f"  [Parentese] Batch failed: {e}")

            parentese_step += 1

            if parentese_step % 20 == 0:
                print(f"  [Parentese] Step {parentese_step}: "
                      f"'{utterance['text'][:50]}' "
                      f"[{utterance['emotion']}/{utterance['contour']}]",
                      flush=True)
        else:
            # --- Environmental sounds ---
            desc = descriptions[env_step % len(descriptions)]
            audio_samples = generate_synthetic_audio(desc, variation=step)
            mel_features = audio_to_mel_features(audio_samples)

            # Atomic: submit sensory + learn in one batch to prevent the main training
            # script from consuming staged audio data between our submit and learn calls.
            try:
                brain._send({"cmd": "batch", "commands": [
                    {"cmd": "submit_sensory", "modality": "audio", "data": mel_features},
                    {"cmd": "submit_sensory", "modality": "speech", "data": audio_samples[:4000]},
                    {"cmd": "learn_vector",
                     "features": mel_features,
                     "target": mel_features,
                     "label": f"audio:{desc[:40]}",
                     "confidence": 0.6,
                     "learning_rate": 0.0002}
                ]})
            except Exception as e:
                if step < 5:
                    print(f"  [Audio] Batch failed: {e}")

            env_step += 1

            if env_step % 50 == 0:
                class_idx = (env_step - 1) % len(descriptions)
                print(f"  [Audio] Step {env_step}: class {class_idx}/{len(descriptions)} "
                      f"'{descriptions[class_idx][:50]}' "
                      f"({len(audio_samples)} samples, {len(mel_features)} mel features)",
                      flush=True)

        step += 1
        time.sleep(step_interval)


if __name__ == "__main__":
    main()
