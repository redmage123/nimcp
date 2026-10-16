#!/usr/bin/env python3
"""
Streaming Dataset Integration — Feeds internet datasets to Athena in real-time.

Streams from HuggingFace datasets, no local storage required.
Runs as a parallel process alongside training, submitting data via daemon socket.

Datasets streamed:
  Audio:     AudioCaps (46K captions), FSD50K (51K clips), Common Voice
  Visual:    COCO Captions (330K), Conceptual Captions (3.3M)
  Knowledge: ConceptNet (21M edges), ATOMIC (877K inferences)
  Embodiment: Something-Something V2 (220K action descriptions)

Usage:
    nohup python3 -u scripts/streaming_datasets.py > nohup_streaming.log 2>&1 &
"""

import os
import sys
import time
import random
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.stdout.reconfigure(line_buffering=True)

# Suppress noisy logs
os.environ["TOKENIZERS_PARALLELISM"] = "false"
os.environ["HF_DATASETS_CACHE"] = "/tmp/hf_cache"

from brain_client import BrainProxy

# Try to import datasets library
try:
    from datasets import load_dataset
    HAS_DATASETS = True
except ImportError:
    HAS_DATASETS = False
    print("[WARN] 'datasets' library not installed. Run: pip install datasets")

# Try to import sentence encoder
try:
    from claude_teacher import encode_text
    HAS_ENCODER = True
except ImportError:
    HAS_ENCODER = False

import numpy as np


# =============================================================================
# Dataset Streamers — each yields (text, modality, label) tuples
# =============================================================================

def stream_commonsense_qa():
    """CommonsenseQA: multiple-choice commonsense reasoning questions."""
    try:
        ds = load_dataset("commonsense_qa", split="train", streaming=True)
        for item in ds:
            q = item.get("question", "")
            choices = item.get("choices", {})
            answer_key = item.get("answerKey", "")
            if q and choices:
                labels = choices.get("label", [])
                texts = choices.get("text", [])
                answer_idx = labels.index(answer_key) if answer_key in labels else 0
                answer = texts[answer_idx] if answer_idx < len(texts) else ""
                text = f"Question: {q} Answer: {answer}"
                yield text, "reasoning", f"csqa_{q[:25].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [CommonsenseQA] Failed to load: {e}")
        return


def stream_hellaswag():
    """HellaSwag: sentence completion for commonsense reasoning."""
    try:
        ds = load_dataset("hellaswag", split="train", streaming=True)
        for item in ds:
            ctx = item.get("ctx", "")
            activity = item.get("activity_label", "")
            if ctx and len(ctx) > 20:
                text = f"{activity}: {ctx}" if activity else ctx
                yield text, "reasoning", f"hellaswag_{activity[:20].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [HellaSwag] Failed to load: {e}")
        return


def stream_wikitext():
    """WikiText-2: Wikipedia text for language understanding."""
    try:
        ds = load_dataset("wikitext", name="wikitext-2-v1", split="train", streaming=True)
        for item in ds:
            text = item.get("text", "").strip()
            if text and len(text) > 50 and not text.startswith("="):
                # Take first 200 chars of each paragraph
                text = text[:200]
                yield text, "knowledge", f"wiki_{text[:25].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [WikiText] Failed to load: {e}")
        return


def stream_emotion():
    """Emotion dataset: text with emotion labels for emotional learning."""
    try:
        ds = load_dataset("SetFit/emotion", split="train", streaming=True)
        for item in ds:
            text = item.get("text", "")
            label = item.get("label_text", "")
            if text and label:
                yield f"{text} [emotion: {label}]", "ethics", f"emotion_{label}_{text[:20].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [Emotion] Failed to load: {e}")
        return


def stream_imdb():
    """IMDB: movie reviews for sentiment understanding."""
    try:
        ds = load_dataset("stanfordnlp/imdb", split="train", streaming=True)
        for item in ds:
            text = item.get("text", "")
            label = item.get("label", 0)
            sentiment = "positive" if label == 1 else "negative"
            if text and len(text) > 50:
                # Take first 200 chars
                text = text[:200]
                yield f"{text} [sentiment: {sentiment}]", "ethics", f"imdb_{sentiment}_{text[:15].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [IMDB] Failed to load: {e}")
        return


def stream_ag_news():
    """AG News: news articles across 4 topics for world knowledge."""
    try:
        ds = load_dataset("ag_news", split="train", streaming=True)
        topics = {0: "world", 1: "sports", 2: "business", 3: "science"}
        for item in ds:
            text = item.get("text", "")
            label = item.get("label", 0)
            topic = topics.get(label, "unknown")
            if text and len(text) > 30:
                text = text[:200]
                yield text, "knowledge", f"news_{topic}_{text[:20].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [AGNews] Failed to load: {e}")
        return


def stream_stsb():
    """STS Benchmark: sentence similarity pairs for semantic understanding."""
    try:
        ds = load_dataset("sentence-transformers/stsb", split="train", streaming=True)
        for item in ds:
            s1 = item.get("sentence1", "")
            s2 = item.get("sentence2", "")
            score = item.get("score", 0)
            if s1 and s2:
                sim = "similar" if score > 3.5 else "different"
                text = f"{s1} and {s2} are {sim}"
                yield text, "analogy", f"stsb_{sim}_{s1[:15].replace(' ', '_').lower()}"
    except Exception as e:
        print(f"  [STSB] Failed to load: {e}")
        return


def stream_embodiment_templates():
    """Action templates for embodied understanding (no external dataset needed)."""
    templates = [
        "Pushing something from left to right", "Pulling something from right to left",
        "Picking something up", "Putting something down",
        "Turning something upside down", "Poking something so it falls over",
        "Throwing something in the air", "Catching something falling",
        "Tearing something into two pieces", "Folding something in half",
        "Pouring water from one cup to another", "Scooping sand with a spoon",
        "Rolling a ball across the floor", "Squeezing a sponge",
        "Spinning a top on the table", "Tilting a glass until water spills",
        "Dropping an egg and it breaks", "Lifting a heavy box off the ground",
        "Stacking blocks into a tower", "Taking a book out of a bag",
        "Opening a door by turning the handle", "Closing a laptop lid",
        "Bending a wire into a circle", "Twisting a bottle cap to open it",
        "Shaking a jar to mix the contents", "Sliding a drawer open",
        "Flipping a pancake in a pan", "Peeling a banana from the top",
        "Threading a needle with thread", "Tying shoelaces into a bow",
        "Balancing a ball on a stick", "Bouncing a ball on the floor",
        "Kicking a ball toward a goal", "Swinging a bat at a ball",
        "Climbing up stairs one step at a time", "Crawling under a table",
        "Jumping over a puddle", "Walking along a narrow beam",
        "Standing on one foot without falling", "Sitting down in a chair",
    ]
    while True:
        random.shuffle(templates)
        for t in templates:
            yield t, "embodiment", f"embodiment_{t[:25].replace(' ', '_').lower()}"


# =============================================================================
# Interleaved Streamer — round-robin across all datasets
# =============================================================================

class InterleavedStreamer:
    """Interleaves multiple dataset streams with configurable weights."""

    def __init__(self):
        self.streams = []
        self.weights = []
        self.names = []
        self.counts = {}
        self.errors = {}

    def add_stream(self, name, generator_fn, weight=1.0):
        self.streams.append(generator_fn)
        self.weights.append(weight)
        self.names.append(name)
        self.counts[name] = 0
        self.errors[name] = 0

    def _init_iterators(self):
        """Initialize all stream iterators."""
        self.iterators = []
        for i, stream_fn in enumerate(self.streams):
            try:
                it = iter(stream_fn())
                self.iterators.append(it)
            except Exception as e:
                print(f"  [Streamer] Failed to init {self.names[i]}: {e}")
                self.iterators.append(iter([]))  # Empty iterator

    def __iter__(self):
        self._init_iterators()
        # Weighted round-robin
        total_weight = sum(self.weights)
        probs = [w / total_weight for w in self.weights]

        while True:
            # Pick a stream based on weights
            idx = random.choices(range(len(self.iterators)), weights=probs, k=1)[0]
            try:
                item = next(self.iterators[idx])
                self.counts[self.names[idx]] += 1
                yield item
            except StopIteration:
                # Stream exhausted — try to restart
                try:
                    self.iterators[idx] = iter(self.streams[idx]())
                    item = next(self.iterators[idx])
                    self.counts[self.names[idx]] += 1
                    yield item
                except Exception:
                    self.errors[self.names[idx]] += 1
                    # Skip to next stream
                    continue
            except Exception as e:
                self.errors[self.names[idx]] += 1
                if self.errors[self.names[idx]] <= 3:
                    print(f"  [Streamer] {self.names[idx]} error: {e}")
                continue

    def get_stats(self):
        return {name: {"count": self.counts[name], "errors": self.errors[name]}
                for name in self.names}


# =============================================================================
# Feature encoding
# =============================================================================

def encode_features(text, modality="text"):
    """Encode text to brain input features."""
    if HAS_ENCODER:
        emb = encode_text(text)
        # Tile to brain input dim (1024)
        features = np.zeros(1024, dtype=np.float32)
        emb_arr = np.array(emb, dtype=np.float32)
        for i in range(0, 1024, len(emb_arr)):
            n = min(len(emb_arr), 1024 - i)
            features[i:i+n] = emb_arr[:n]
        return features.tolist()
    else:
        # Hash-based fallback
        h = hash(text)
        rng = np.random.RandomState(abs(h) % (2**32 - 1))
        return rng.randn(1024).astype(np.float32).tolist()


def make_target(text, dim=4096):
    """Make semantic target vector."""
    if HAS_ENCODER:
        emb = encode_text(text)
        target = np.zeros(dim, dtype=np.float32)
        emb_arr = np.array(emb, dtype=np.float32)
        for i in range(0, dim, len(emb_arr)):
            n = min(len(emb_arr), dim - i)
            target[i:i+n] = emb_arr[:n]
        return target.tolist()
    else:
        h = hash(text)
        rng = np.random.RandomState(abs(h) % (2**32 - 1))
        return rng.randn(dim).astype(np.float32).tolist()


# =============================================================================
# Main
# =============================================================================

def main():
    print("=" * 60)
    print("  STREAMING DATASET INTEGRATION")
    print("  Feeding internet datasets to Athena in real-time")
    print("=" * 60)

    if not HAS_DATASETS:
        print("ERROR: 'datasets' library required. Install with:")
        print("  pip install datasets")
        sys.exit(1)

    # Connect to brain daemon
    brain = BrainProxy()
    print(f"  Connected to brain daemon")

    # Build interleaved streamer — 8 verified working datasets
    streamer = InterleavedStreamer()
    streamer.add_stream("CommonsenseQA", stream_commonsense_qa, weight=2.5)
    streamer.add_stream("HellaSwag", stream_hellaswag, weight=2.0)
    streamer.add_stream("WikiText", stream_wikitext, weight=2.0)
    streamer.add_stream("Emotion", stream_emotion, weight=1.5)
    streamer.add_stream("IMDB", stream_imdb, weight=1.5)
    streamer.add_stream("AGNews", stream_ag_news, weight=2.0)
    streamer.add_stream("STSB", stream_stsb, weight=1.5)
    streamer.add_stream("Embodiment", stream_embodiment_templates, weight=1.0)

    print(f"  Configured {len(streamer.streams)} dataset streams")
    print(f"  Weights: {dict(zip(streamer.names, streamer.weights))}")

    # Rate limit: 1 item per 5 seconds (conservative — main training is primary)
    rate_hz = 0.2
    step_interval = 1.0 / rate_hz

    # Anti-overfitting: LR must be LOWER than main training (currently 0.000010).
    # Streaming data augments, it doesn't drive learning.
    base_lr = 0.000002  # 5x lower than main training LR
    lr_decay = 0.9999  # Decay per step

    # Track seen items to avoid repeating
    seen_hashes = set()
    duplicate_count = 0

    step = 0
    for text, modality, label in streamer:
        # Deduplicate — never train on the same text twice
        text_hash = hash(text) % (2**32)
        if text_hash in seen_hashes:
            duplicate_count += 1
            continue
        seen_hashes.add(text_hash)
        if len(seen_hashes) > 100000:
            # Prevent memory bloat — reset after 100K unique items
            seen_hashes.clear()

        # Decaying learning rate
        current_lr = base_lr * (lr_decay ** step)
        if current_lr < 1e-6:
            current_lr = 1e-6  # Floor

        # Encode and submit
        try:
            features = encode_features(text, modality)
            target = make_target(text)
            brain.learn_vector(features, target, label=label[:50],
                               confidence=0.3, learning_rate=current_lr)
        except Exception as e:
            if step < 10:
                print(f"  [Stream] Submit failed at step {step}: {e}")

        # Also submit as sensory data if audio/visual
        if modality == "audio":
            try:
                from parallel_audio_stream import generate_synthetic_audio, audio_to_mel_features
                audio = generate_synthetic_audio(text, variation=step)
                mel = audio_to_mel_features(audio)
                brain.submit_sensory("audio", mel)
            except Exception:
                pass

        step += 1

        if step % 100 == 0:
            stats = streamer.get_stats()
            total = sum(s["count"] for s in stats.values())
            active = sum(1 for s in stats.values() if s["count"] > 0)
            print(f"  [Stream] Step {step}: {total} items from {active}/{len(stats)} streams "
                  f"(lr={current_lr:.7f}, unique={len(seen_hashes)}, dupes={duplicate_count})")
            for name, s in stats.items():
                if s["count"] > 0 or s["errors"] > 0:
                    print(f"    {name}: {s['count']} items, {s['errors']} errors")

        time.sleep(step_interval)


if __name__ == "__main__":
    main()
