#!/usr/bin/env python3
"""
athena_ensemble.py — Multi-Brain Ensemble Architecture for Athena

Instead of one monolithic 1.5M neuron brain trying to do everything,
this creates specialized brains that each focus on a domain:

  Perceptual (300K) — sensory processing, pattern recognition
  Language   (400K) — speech, grammar, vocabulary, text generation
  Reasoning  (400K) — logic, math, causal inference, science
  Memory     (300K) — episodic recall, fact storage, associations
  Social     (300K) — ethics, emotion, theory of mind
  Executive  (200K) — routing, attention, planning, orchestration

Brains communicate through a shared 1024-dim embedding bus.
The Executive brain learns to route inputs to the right specialist(s).
Each brain trains independently with its own curriculum.

Usage:
    python scripts/athena_ensemble.py                    # Fresh start
    python scripts/athena_ensemble.py --resume           # Resume from checkpoint
    python scripts/athena_ensemble.py --no-claude        # Without Claude parent
    python scripts/athena_ensemble.py --monolithic       # Fall back to single brain
"""

import argparse
import logging
import os
import sys
import time
import random
import json
import signal
import numpy as np
from multiprocessing import Process, Queue, Event
from threading import Thread

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

os.environ["TOKENIZERS_PARALLELISM"] = "false"
os.environ["TQDM_DISABLE"] = "1"
import warnings
warnings.filterwarnings("ignore", category=FutureWarning)
logging.getLogger("httpx").setLevel(logging.WARNING)
logging.getLogger("sentence_transformers").setLevel(logging.WARNING)

logger = logging.getLogger(__name__)

import nimcp
from claude_teacher import ClaudeTeacher, encode_text, batch_encode_texts
from neural_decoder import NeuralDecoder

# ============================================================================
# Constants
# ============================================================================

BRAIN_INPUT_DIM = 1024
BRAIN_OUTPUT_DIM = 4096
EMBEDDING_DIM = 1024  # Shared embedding bus dimension
CHECKPOINT_DIR = "checkpoints/athena_ensemble"

# Specialist brain configurations
BRAIN_SPECS = {
    "perceptual": {
        "neurons": 300000,
        "domains": ["perception", "patterns", "visual", "audio", "tactile"],
        "description": "Sensory processing and pattern recognition",
    },
    "language": {
        "neurons": 400000,
        "domains": ["language", "grammar", "vocabulary", "speech", "reading"],
        "description": "Speech, grammar, vocabulary, text generation",
    },
    "reasoning": {
        "neurons": 400000,
        "domains": ["math", "logic", "science", "physics", "chemistry",
                     "biology", "technology", "engineering"],
        "description": "Logic, math, causal inference, science",
    },
    "memory": {
        "neurons": 300000,
        "domains": ["geography", "history", "facts", "associations", "episodic"],
        "description": "Episodic recall, fact storage, associations",
    },
    "social": {
        "neurons": 300000,
        "domains": ["ethics", "emotion", "empathy", "philosophy",
                     "psychology", "social"],
        "description": "Ethics, emotion, theory of mind",
    },
    "executive": {
        "neurons": 200000,
        "domains": ["routing", "planning", "attention", "metacognition"],
        "description": "Routing, attention, planning, orchestration",
    },
}

# Hemispheric lateralization mapping
# Left hemisphere: language, logic, sequential processing
# Right hemisphere: spatial, pattern recognition, emotion
# Bilateral: memory (hippocampus), executive (prefrontal cortex)
HEMISPHERE_MAP = {
    "language":   "left",
    "reasoning":  "left",
    "perceptual": "right",
    "social":     "right",
    "memory":     "bilateral",
    "executive":  "bilateral",
}

# Domain to specialist mapping (for routing)
DOMAIN_ROUTING = {}
for brain_name, spec in BRAIN_SPECS.items():
    for domain in spec["domains"]:
        DOMAIN_ROUTING[domain] = brain_name


# ============================================================================
# Executive Router: classifies input domain and dispatches to specialists
# ============================================================================

class ExecutiveRouter:
    """Routes inputs to the appropriate specialist brain(s).

    Uses a combination of keyword matching and embedding similarity
    to determine which specialist(s) should process each input.
    The executive brain also learns routing over time.
    """

    def __init__(self, brain_names):
        self.brain_names = brain_names
        self.domain_keywords = {
            "perceptual": ["see", "look", "color", "bright", "dark", "sound",
                           "hear", "feel", "touch", "smell", "taste", "warm",
                           "cold", "soft", "rough", "light", "shadow", "shape",
                           "red", "blue", "green", "yellow", "pattern"],
            "language": ["word", "say", "speak", "read", "write", "sentence",
                         "grammar", "letter", "story", "poem", "vocabulary",
                         "language", "talk", "phrase", "meaning", "synonym"],
            "reasoning": ["math", "number", "calculate", "add", "subtract",
                          "multiply", "divide", "logic", "reason", "because",
                          "therefore", "science", "physics", "chemistry",
                          "experiment", "hypothesis", "prove", "equation",
                          "technology", "engineer", "formula"],
            "memory": ["remember", "recall", "fact", "history", "geography",
                       "country", "capital", "continent", "ocean", "river",
                       "mountain", "year", "date", "event", "where", "when",
                       "who", "ancient", "civilization"],
            "social": ["feel", "emotion", "happy", "sad", "angry", "kind",
                       "fair", "right", "wrong", "moral", "ethics", "friend",
                       "love", "care", "help", "empathy", "believe",
                       "philosophy", "meaning", "purpose", "consciousness"],
        }
        # Routing statistics
        self.route_counts = {name: 0 for name in brain_names}
        self.route_history = []

    def route(self, text, domain_hint=None):
        """Determine which specialist brain(s) should handle this input.

        Returns list of (brain_name, confidence) tuples, sorted by confidence.
        Always returns at least one brain.
        """
        scores = {name: 0.0 for name in self.brain_names if name != "executive"}

        # Domain hint (highest priority)
        if domain_hint:
            hint_lower = domain_hint.lower()
            if hint_lower in DOMAIN_ROUTING:
                target = DOMAIN_ROUTING[hint_lower]
                if target in scores:
                    scores[target] += 3.0

            # Partial match
            for domain, brain_name in DOMAIN_ROUTING.items():
                if domain in hint_lower or hint_lower in domain:
                    if brain_name in scores:
                        scores[brain_name] += 1.5

        # Keyword matching
        if text:
            text_lower = text.lower()
            for brain_name, keywords in self.domain_keywords.items():
                if brain_name in scores:
                    for kw in keywords:
                        if kw in text_lower:
                            scores[brain_name] += 1.0

        # Default: if no strong signal, route to perceptual (safe fallback)
        max_score = max(scores.values()) if scores else 0
        if max_score < 0.5:
            scores["perceptual"] = 1.0

        # Normalize and sort
        total = sum(scores.values()) or 1.0
        results = [(name, score / total) for name, score in scores.items()
                    if score > 0]
        results.sort(key=lambda x: -x[1])

        # Track routing
        if results:
            primary = results[0][0]
            self.route_counts[primary] = self.route_counts.get(primary, 0) + 1
            self.route_history.append(primary)
            if len(self.route_history) > 1000:
                self.route_history = self.route_history[-1000:]

        return results

    def get_primary(self, text, domain_hint=None):
        """Get the single best specialist for this input."""
        routes = self.route(text, domain_hint)
        return routes[0][0] if routes else "perceptual"

    def get_multi(self, text, domain_hint=None, threshold=0.2):
        """Get all specialists above threshold confidence."""
        routes = self.route(text, domain_hint)
        return [(name, conf) for name, conf in routes if conf >= threshold]

    def get_hemisphere(self, brain_name):
        """Get which hemisphere a specialist belongs to."""
        return HEMISPHERE_MAP.get(brain_name, "bilateral")

    def route_hemispheric(self, text, domain_hint=None):
        """Route with hemispheric awareness — returns (brain, hemisphere, conf) triples."""
        routes = self.route(text, domain_hint)
        return [(name, self.get_hemisphere(name), conf) for name, conf in routes]

    def summary(self):
        """Print routing statistics."""
        total = sum(self.route_counts.values()) or 1
        lines = []
        for name in sorted(self.route_counts.keys()):
            count = self.route_counts[name]
            pct = 100.0 * count / total
            hemisphere = HEMISPHERE_MAP.get(name, "bilateral")
            lines.append(f"    {name:12s}: {count:6d} ({pct:5.1f}%) [{hemisphere}]")
        return "\n".join(lines)


# ============================================================================
# Shared Embedding Bus: inter-brain communication
# ============================================================================

class EmbeddingBus:
    """Shared communication channel between specialist brains.

    Each brain can publish its output embedding to the bus, and other
    brains can read it as context. This enables cross-brain integration
    without direct synaptic connections.
    """

    def __init__(self, dim=EMBEDDING_DIM):
        self.dim = dim
        self.channels = {}  # brain_name -> latest embedding
        self.history = {}   # brain_name -> list of recent embeddings

    def publish(self, brain_name, embedding):
        """Publish a brain's output embedding to the bus."""
        emb = np.asarray(embedding, dtype=np.float32).ravel()[:self.dim]
        self.channels[brain_name] = emb
        if brain_name not in self.history:
            self.history[brain_name] = []
        self.history[brain_name].append(emb)
        if len(self.history[brain_name]) > 50:
            self.history[brain_name] = self.history[brain_name][-50:]

    def read(self, brain_name):
        """Read a specific brain's latest embedding."""
        return self.channels.get(brain_name)

    def read_all(self):
        """Read all published embeddings as a dict."""
        return dict(self.channels)

    def get_context_vector(self, exclude=None):
        """Get a combined context vector from all brains (excluding one).

        Averages all published embeddings to create a shared context signal.
        """
        embeddings = []
        for name, emb in self.channels.items():
            if name != exclude:
                embeddings.append(emb)
        if not embeddings:
            return np.zeros(self.dim, dtype=np.float32)
        return np.mean(embeddings, axis=0).astype(np.float32)

    def get_agreement_score(self):
        """Measure how much the specialist brains agree (cosine similarity)."""
        if len(self.channels) < 2:
            return 1.0
        embs = list(self.channels.values())
        sims = []
        for i in range(len(embs)):
            for j in range(i + 1, len(embs)):
                norm_i = np.linalg.norm(embs[i])
                norm_j = np.linalg.norm(embs[j])
                if norm_i > 1e-8 and norm_j > 1e-8:
                    sims.append(np.dot(embs[i], embs[j]) / (norm_i * norm_j))
        return float(np.mean(sims)) if sims else 1.0


# ============================================================================
# Corpus Callosum: inter-hemispheric communication bridge
# ============================================================================

class CorpusCallosum:
    """Models the biological corpus callosum — 4-channel inter-hemispheric bridge.

    Channels mirror biological inter-hemispheric pathways:
      - Motor: action/movement coordination between hemispheres
      - Sensory: raw perceptual data transfer
      - Cognitive: abstract thought/reasoning transfer
      - Emotional: affective state synchronization

    Each channel carries an embedding vector. Specialists in one hemisphere
    can read the opposite hemisphere's channel to get cross-lateral context.
    Bandwidth is limited (like real axonal transmission) — only the strongest
    signals cross.
    """

    CHANNELS = ["motor", "sensory", "cognitive", "emotional"]

    def __init__(self, dim=EMBEDDING_DIM, bandwidth=0.7):
        self.dim = dim
        self.bandwidth = bandwidth  # 0-1, fraction of signal that crosses
        self.left_to_right = {ch: np.zeros(dim, dtype=np.float32) for ch in self.CHANNELS}
        self.right_to_left = {ch: np.zeros(dim, dtype=np.float32) for ch in self.CHANNELS}
        self.transfer_count = 0
        self.total_magnitude = 0.0

    def _brain_to_channel(self, brain_name):
        """Map specialist brain to callosum channel."""
        channel_map = {
            "perceptual": "sensory",
            "language":   "cognitive",
            "reasoning":  "cognitive",
            "memory":     "cognitive",
            "social":     "emotional",
            "executive":  "motor",
        }
        return channel_map.get(brain_name, "cognitive")

    def transmit(self, brain_name, embedding):
        """Transmit a specialist's output across the callosum.

        Signal is attenuated by bandwidth factor (biological axonal loss).
        """
        hemisphere = HEMISPHERE_MAP.get(brain_name, "bilateral")
        channel = self._brain_to_channel(brain_name)
        emb = np.asarray(embedding, dtype=np.float32).ravel()[:self.dim]

        # Attenuate by bandwidth (models axonal transmission loss)
        signal = emb * self.bandwidth

        if hemisphere == "left":
            self.left_to_right[channel] = signal
        elif hemisphere == "right":
            self.right_to_left[channel] = signal
        else:
            # Bilateral — transmit both directions
            self.left_to_right[channel] = signal * 0.5
            self.right_to_left[channel] = signal * 0.5

        self.transfer_count += 1
        self.total_magnitude += float(np.linalg.norm(signal))

    def receive(self, brain_name):
        """Get the cross-hemispheric signal for a specialist brain.

        Returns the averaged signal from all channels coming from the
        opposite hemisphere.
        """
        hemisphere = HEMISPHERE_MAP.get(brain_name, "bilateral")

        if hemisphere == "left":
            # Left brain receives from right hemisphere
            signals = list(self.right_to_left.values())
        elif hemisphere == "right":
            # Right brain receives from left hemisphere
            signals = list(self.left_to_right.values())
        else:
            # Bilateral — receive from both
            signals = list(self.left_to_right.values()) + list(self.right_to_left.values())

        if not signals:
            return np.zeros(self.dim, dtype=np.float32)
        return np.mean(signals, axis=0).astype(np.float32)

    def get_lateralization_score(self):
        """Measure hemispheric asymmetry (0 = balanced, 1 = fully lateralized).

        Compares total signal magnitude from each hemisphere.
        """
        left_mag = sum(float(np.linalg.norm(v)) for v in self.left_to_right.values())
        right_mag = sum(float(np.linalg.norm(v)) for v in self.right_to_left.values())
        total = left_mag + right_mag
        if total < 1e-8:
            return 0.0
        return abs(left_mag - right_mag) / total

    def get_dominant_hemisphere(self):
        """Which hemisphere is currently more active?"""
        left_mag = sum(float(np.linalg.norm(v)) for v in self.left_to_right.values())
        right_mag = sum(float(np.linalg.norm(v)) for v in self.right_to_left.values())
        return "left" if left_mag >= right_mag else "right"

    def summary(self):
        """Print callosum status."""
        lines = []
        for ch in self.CHANNELS:
            l2r = float(np.linalg.norm(self.left_to_right[ch]))
            r2l = float(np.linalg.norm(self.right_to_left[ch]))
            lines.append(f"    {ch:10s}: L→R={l2r:.2f}  R→L={r2l:.2f}")
        lat = self.get_lateralization_score()
        dom = self.get_dominant_hemisphere()
        lines.append(f"    Lateralization: {lat:.3f} (dominant: {dom})")
        lines.append(f"    Transfers: {self.transfer_count}")
        return "\n".join(lines)


# ============================================================================
# Specialist Brain Wrapper
# ============================================================================

class SpecialistBrain:
    """Wrapper around a nimcp.Brain with domain-specific configuration."""

    def __init__(self, name, spec, input_dim=BRAIN_INPUT_DIM,
                 output_dim=BRAIN_OUTPUT_DIM, checkpoint_dir=CHECKPOINT_DIR):
        self.name = name
        self.spec = spec
        self.input_dim = input_dim
        self.output_dim = output_dim
        self.checkpoint_dir = checkpoint_dir
        self.brain = None
        self.losses = []
        self.step_count = 0
        self.total_steps = 0

    def initialize(self, checkpoint_path=None):
        """Create and configure the specialist brain."""
        ckpt = checkpoint_path
        if ckpt is None:
            ckpt_file = os.path.join(self.checkpoint_dir, f"{self.name}.bin")
            if os.path.exists(ckpt_file):
                ckpt = ckpt_file

        print(f"    [{self.name}] Creating {self.spec['neurons']:,} neuron brain "
              f"({self.spec['description']})")

        self.brain = nimcp.Brain(
            f"athena_{self.name}",
            num_inputs=self.input_dim,
            num_outputs=self.output_dim,
            neuron_count=self.spec["neurons"],
            checkpoint=ckpt,
            init_mode='full'
        )

        # Configure for developmental learning
        self.brain.set_fast_training(False)
        self.brain.set_task_type("regression")

        # Enable subsystems (graceful failures)
        for setup_fn, label in [
            (lambda: self.brain.enable_mixed_precision(True), "FP16"),
            (lambda: self.brain.enable_gradient_checkpointing(True, 2), "GradCkpt"),
            (lambda: self.brain.enable_biological_plasticity(True), "BioPlast"),
            (lambda: self.brain.lnn_create(64, 32, 16, 32), "LNN"),
        ]:
            try:
                setup_fn()
            except Exception:
                pass

        print(f"    [{self.name}] Ready")

    def learn(self, features, target, label="", confidence=0.5, learning_rate=None):
        """Train this specialist on one example."""
        kwargs = {"label": label[:50], "confidence": confidence}
        if learning_rate is not None and learning_rate > 0:
            kwargs["learning_rate"] = learning_rate
        loss = self.brain.learn_vector(features, target, **kwargs)
        if loss is not None and loss >= 0:
            self.losses.append(loss)
            if len(self.losses) > 500:
                self.losses = self.losses[-500:]
        self.step_count += 1
        self.total_steps += 1
        return loss

    def decide(self, features):
        """Get this specialist's response."""
        return self.brain.decide_full(features)

    def get_avg_loss(self, window=100):
        """Get average loss over recent steps."""
        if not self.losses:
            return 0.0
        recent = self.losses[-window:]
        return float(np.mean(recent))

    def save(self):
        """Save checkpoint."""
        os.makedirs(self.checkpoint_dir, exist_ok=True)
        ckpt = os.path.join(self.checkpoint_dir, f"{self.name}.bin")
        try:
            self.brain.save(ckpt)
        except Exception as e:
            logger.warning("[%s] Checkpoint save failed: %s", self.name, e)


# ============================================================================
# Ensemble Composer: builds input vectors with cross-brain context
# ============================================================================

class EnsembleComposer:
    """Compose input vectors with cross-brain and cross-hemispheric context."""

    def __init__(self, bus, callosum=None):
        self.bus = bus
        self.callosum = callosum

    def compose(self, text=None, modality="text", brain_name=None,
                primary_features=None):
        """Build a 1024-dim input vector with cross-brain + callosum context.

        Layout: [tag:16 | primary:640 | text:256 | context:112]
        Context breakdown: [bus:56 | callosum:48 | hemisphere_meta:8]
        """
        vec = np.zeros(BRAIN_INPUT_DIM, dtype=np.float32)

        # --- Tag section [0:16] ---
        modality_map = {"text": 0, "visual": 1, "audio": 2, "speech": 3,
                        "ethics": 4, "imagination": 5, "reasoning": 6}
        mod_idx = modality_map.get(modality, 0)
        vec[mod_idx] = 1.0

        # Brain identity tag
        brain_map = {"perceptual": 0, "language": 1, "reasoning": 2,
                     "memory": 3, "social": 4, "executive": 5}
        if brain_name and brain_name in brain_map:
            vec[8 + brain_map[brain_name]] = 1.0

        # Hemisphere tag
        hemisphere = HEMISPHERE_MAP.get(brain_name, "bilateral")
        if hemisphere == "left":
            vec[14] = 1.0
        elif hemisphere == "right":
            vec[15] = 1.0
        else:
            vec[14] = 0.5
            vec[15] = 0.5

        # --- Primary features [16:656] ---
        TAG_DIM = 16
        PRIMARY_DIM = 640
        TEXT_DIM = 256
        if primary_features is not None:
            pf = np.array(primary_features, dtype=np.float32)
            n = min(len(pf), PRIMARY_DIM)
            vec[TAG_DIM:TAG_DIM + n] = pf[:n]
        elif text is not None:
            emb = encode_text(text)
            n = min(len(emb), PRIMARY_DIM)
            vec[TAG_DIM:TAG_DIM + n] = emb[:n]

        # --- Text semantic embedding [656:912] ---
        if text is not None:
            emb = encode_text(text)
            n = min(len(emb), TEXT_DIM)
            vec[TAG_DIM + PRIMARY_DIM:TAG_DIM + PRIMARY_DIM + n] = emb[:n]

        # --- Cross-brain context [912:1024] ---
        ctx_start = TAG_DIM + PRIMARY_DIM + TEXT_DIM

        # Embedding bus context [912:968] — 56 dims
        bus_context = self.bus.get_context_vector(exclude=brain_name)
        n = min(len(bus_context), 56)
        vec[ctx_start:ctx_start + n] = bus_context[:n]

        # Corpus callosum context [968:1016] — 48 dims
        if self.callosum and brain_name:
            callosum_signal = self.callosum.receive(brain_name)
            n = min(len(callosum_signal), 48)
            vec[ctx_start + 56:ctx_start + 56 + n] = callosum_signal[:n]

        # Hemisphere metadata [1016:1024] — 8 dims
        if self.callosum:
            vec[ctx_start + 104] = self.callosum.get_lateralization_score()
            vec[ctx_start + 105] = 1.0 if self.callosum.get_dominant_hemisphere() == "left" else 0.0
            vec[ctx_start + 106] = self.bus.get_agreement_score()

        return vec.tolist()


# ============================================================================
# Ensemble Orchestrator: coordinates all specialist brains
# ============================================================================

class EnsembleOrchestrator:
    """Coordinates the multi-brain ensemble.

    Manages brain creation, routing, training, and cross-brain integration.
    """

    def __init__(self, no_claude=False, resume=False, fresh=False):
        self.brains = {}
        self.router = ExecutiveRouter(list(BRAIN_SPECS.keys()))
        self.bus = EmbeddingBus()
        self.callosum = CorpusCallosum(bandwidth=0.7)
        self.composer = EnsembleComposer(self.bus, self.callosum)
        self.no_claude = no_claude
        self.resume = resume
        self.fresh = fresh
        self.teacher = None
        self.decoder = None
        self.step_count = 0

    def initialize(self):
        """Create all specialist brains."""
        print("\n" + "=" * 60)
        print("  ATHENA ENSEMBLE — Hemispheric Multi-Brain Architecture")
        print("  Left: Language + Reasoning | Right: Perceptual + Social")
        print("  Bilateral: Memory + Executive | Bridge: Corpus Callosum")
        print("=" * 60)

        total_neurons = sum(s["neurons"] for s in BRAIN_SPECS.values())
        print(f"\n  Total neurons: {total_neurons:,} across {len(BRAIN_SPECS)} brains")
        print(f"  Architecture: {', '.join(f'{n}({s['neurons']//1000}K)' for n, s in BRAIN_SPECS.items())}")
        print()

        for name, spec in BRAIN_SPECS.items():
            brain = SpecialistBrain(name, spec)
            ckpt = None
            if self.resume and not self.fresh:
                ckpt_file = os.path.join(CHECKPOINT_DIR, f"{name}.bin")
                if os.path.exists(ckpt_file):
                    ckpt = ckpt_file
            brain.initialize(checkpoint_path=ckpt if not self.fresh else None)
            self.brains[name] = brain

        # Teacher
        if not self.no_claude:
            try:
                self.teacher = ClaudeTeacher()
                print("  Claude parent: ON")
            except Exception as e:
                print(f"  Claude parent: OFF ({e})")

        # Decoder
        decoder_dir = os.path.join(CHECKPOINT_DIR, "decoder")
        try:
            if os.path.exists(decoder_dir) and self.resume:
                self.decoder = NeuralDecoder.load(decoder_dir)
            else:
                self.decoder = NeuralDecoder(output_dim=BRAIN_OUTPUT_DIM)
            print("  Neural decoder: ON")
        except Exception:
            self.decoder = NeuralDecoder(output_dim=BRAIN_OUTPUT_DIM)

        print(f"\n  All {len(self.brains)} specialist brains initialized.")

    def process_stimulus(self, text, domain_hint=None, learning_rate=None):
        """Process a stimulus through the ensemble.

        1. Router determines which specialist(s) to activate
        2. Each activated specialist processes the input
        3. Specialists publish their responses to the embedding bus
        4. Cross-brain context flows back for next iteration
        """
        # Route to specialist(s)
        routes = self.router.get_multi(text, domain_hint, threshold=0.15)

        results = {}
        losses = {}

        for brain_name, confidence in routes:
            brain = self.brains.get(brain_name)
            if not brain:
                continue

            # Compose input with cross-brain context
            features = self.composer.compose(
                text=text, brain_name=brain_name,
                modality=self._domain_to_modality(domain_hint))
            target = self._make_target(text)

            # Forward pass
            result = brain.decide(features)
            results[brain_name] = result

            # Learn (weighted by routing confidence)
            lr = learning_rate if learning_rate else None
            label = f"{domain_hint or 'general'}:{brain_name}"
            loss = brain.learn(features, target, label=label,
                               confidence=min(confidence + 0.3, 1.0),
                               learning_rate=lr)
            losses[brain_name] = loss

            # Publish output to embedding bus + callosum
            output_vec = result.get("output_vector")
            if output_vec is not None:
                emb = np.array(output_vec[:EMBEDDING_DIM], dtype=np.float32)
                self.bus.publish(brain_name, emb)
                self.callosum.transmit(brain_name, emb)

        self.step_count += 1
        return results, losses

    def train_specialist(self, brain_name, text, domain_hint=None,
                         learning_rate=None, confidence=0.5):
        """Train a specific specialist directly (bypasses routing)."""
        brain = self.brains.get(brain_name)
        if not brain:
            return None

        features = self.composer.compose(
            text=text, brain_name=brain_name,
            modality=self._domain_to_modality(domain_hint))
        target = self._make_target(text)

        result = brain.decide(features)
        loss = brain.learn(features, target,
                           label=f"{domain_hint or brain_name}",
                           confidence=confidence,
                           learning_rate=learning_rate)

        output_vec = result.get("output_vector")
        if output_vec is not None:
            emb = np.array(output_vec[:EMBEDDING_DIM], dtype=np.float32)
            self.bus.publish(brain_name, emb)
            self.callosum.transmit(brain_name, emb)

        return loss

    def merge_responses(self, results):
        """Merge responses from multiple specialists into a unified output.

        Weighted average of output vectors based on output magnitude
        (specialists that respond more strongly have more influence).
        """
        vectors = []
        weights = []
        for brain_name, result in results.items():
            output_vec = result.get("output_vector")
            if output_vec is not None:
                arr = np.array(output_vec, dtype=np.float32)
                norm = float(np.linalg.norm(arr))
                if norm > 1e-8:
                    vectors.append(arr)
                    weights.append(norm)

        if not vectors:
            return None

        # Weighted average
        total_weight = sum(weights)
        merged = np.zeros_like(vectors[0])
        for vec, w in zip(vectors, weights):
            merged += vec * (w / total_weight)
        return merged

    def save_all(self):
        """Save all specialist brains."""
        os.makedirs(CHECKPOINT_DIR, exist_ok=True)
        for name, brain in self.brains.items():
            brain.save()

        # Save decoder
        if self.decoder:
            decoder_dir = os.path.join(CHECKPOINT_DIR, "decoder")
            os.makedirs(decoder_dir, exist_ok=True)
            try:
                self.decoder.save(decoder_dir)
            except Exception:
                pass

        # Save ensemble state
        state = {
            "step": self.step_count,
            "route_counts": self.router.route_counts,
            "brain_steps": {n: b.total_steps for n, b in self.brains.items()},
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        state_file = os.path.join(CHECKPOINT_DIR, "ensemble_state.json")
        with open(state_file, "w") as f:
            json.dump(state, f, indent=2)

    def print_status(self):
        """Print ensemble status."""
        print(f"\n  {'─' * 56}")
        print(f"  ENSEMBLE STATUS — Step {self.step_count}")
        print(f"  {'─' * 56}")
        for name in sorted(self.brains.keys()):
            brain = self.brains[name]
            avg_loss = brain.get_avg_loss()
            print(f"    {name:12s}: steps={brain.total_steps:6d}  "
                  f"avg_loss={avg_loss:.4f}")
        print(f"\n  Routing distribution:")
        print(self.router.summary())
        agreement = self.bus.get_agreement_score()
        print(f"\n  Cross-brain agreement: {agreement:.3f}")
        print(f"\n  Corpus Callosum:")
        print(self.callosum.summary())
        print(f"  {'─' * 56}\n")

    def _make_target(self, text):
        """Create a target vector from text."""
        emb = encode_text(text)
        target = np.zeros(BRAIN_OUTPUT_DIM, dtype=np.float32)
        for i in range(0, BRAIN_OUTPUT_DIM, len(emb)):
            n = min(len(emb), BRAIN_OUTPUT_DIM - i)
            target[i:i + n] = emb[:n]
        return target.tolist()

    def _domain_to_modality(self, domain):
        """Map domain hint to modality tag."""
        if not domain:
            return "text"
        d = domain.lower()
        if d in ("visual", "perception", "patterns"):
            return "visual"
        if d in ("audio", "speech"):
            return "speech"
        if d in ("ethics", "emotion", "empathy"):
            return "ethics"
        if d in ("imagination", "creative"):
            return "imagination"
        if d in ("math", "logic", "science"):
            return "reasoning"
        return "text"


# ============================================================================
# Training Corpus (reused from immerse_athena.py patterns)
# ============================================================================

PERCEPTUAL_STIMULI = [
    "warm sunlight on skin", "the color red", "bright yellow light",
    "shadows moving on the wall", "green leaves rustling",
    "a rainbow after rain", "white fluffy clouds", "orange sunset",
    "deep blue ocean", "silver moonlight", "flickering candlelight",
    "sparkling water drops", "snow falling softly", "fire dancing",
    "soft fur of a sleeping cat", "rough bark of a tree",
    "smooth cold glass", "warm sand between toes", "cool water on hands",
    "birds singing at dawn", "rain tapping on a roof",
    "wind howling through trees", "waves crashing on shore",
    "a clock ticking steadily", "thunder rumbling",
    "the smell of fresh bread", "pine forest scent",
    "the taste of sweetness", "chocolate melting",
]

LANGUAGE_STIMULI = [
    ("dog", "A friendly dog with soft fur that wags its tail"),
    ("cat", "A cat with soft fur that purrs when happy"),
    ("tree", "A tall tree with green leaves reaching to the sky"),
    ("sun", "The bright warm sun that lights up the day"),
    ("water", "Cool clear water flowing in a stream"),
    ("hello", "A greeting word used when meeting someone"),
    ("good", "A word meaning positive, kind, or well-done"),
    ("the", "The most common word in English, a determiner"),
    ("is", "A linking verb connecting subject to description"),
    ("run", "To move quickly on foot, legs pumping"),
    ("big dog", "A large dog that takes up lots of space"),
    ("red ball", "A round toy colored bright red"),
    ("the cat sits", "A cat resting in a seated position"),
    ("I see you", "An acknowledgment of another's presence"),
    ("water is good", "Clean water is healthy and refreshing"),
]

REASONING_STIMULI = [
    ("Two plus two equals four", "math"),
    ("Three times three equals nine", "math"),
    ("If it rains, the ground gets wet", "logic"),
    ("All dogs are animals, so my pet dog is an animal", "logic"),
    ("Water freezes at zero degrees Celsius", "physics"),
    ("Gravity pulls everything toward the Earth", "physics"),
    ("Plants make food from sunlight through photosynthesis", "biology"),
    ("The heart pumps blood through the body", "biology"),
    ("Atoms are the building blocks of all matter", "chemistry"),
    ("Energy cannot be created or destroyed", "physics"),
    ("The Earth orbits the sun once every year", "science"),
    ("DNA contains instructions for building living things", "biology"),
]

MEMORY_STIMULI = [
    ("The Earth has seven continents and five oceans", "geography"),
    ("Mountains form when tectonic plates push together", "geography"),
    ("Rivers flow from high ground toward the sea", "geography"),
    ("Dogs have been human companions for thousands of years", "history"),
    ("Ancient Egypt built pyramids along the Nile river", "history"),
    ("The wheel was one of humanity's greatest inventions", "history"),
    ("The Pacific is the largest ocean on Earth", "geography"),
    ("The Amazon rainforest is the largest tropical forest", "geography"),
    ("Rome was the center of a vast ancient empire", "history"),
    ("The printing press changed how knowledge spread", "history"),
]

SOCIAL_STIMULI = [
    ("Being kind to others makes the world better", "ethics"),
    ("Sharing means giving part of what you have to someone", "ethics"),
    ("When someone is sad, sitting with them helps", "empathy"),
    ("Everyone deserves to be treated with respect", "ethics"),
    ("Happiness comes from connection with others", "emotion"),
    ("Fear is a natural response that keeps us safe", "emotion"),
    ("Saying sorry when you make a mistake shows strength", "ethics"),
    ("Listening carefully shows you care about someone", "empathy"),
    ("Curiosity drives us to learn and explore", "emotion"),
    ("Courage means doing what is right even when afraid", "ethics"),
]


# ============================================================================
# Training Loop
# ============================================================================

def run_ensemble_training(orchestrator, num_steps=30000, start_from=0):
    """Run the ensemble training loop.

    Each step:
    1. Select a curriculum domain
    2. Route to appropriate specialist(s)
    3. Train specialist(s) on domain-specific stimuli
    4. Periodically cross-train (teach specialists from each other's domains)
    5. Executive brain learns routing patterns
    """
    print("\n" + "=" * 60)
    print("  ENSEMBLE TRAINING — All Specialists Active")
    print("=" * 60)

    import math

    # Cosine annealing LR
    lr_max = 0.001
    lr_min = 0.0001
    warmup = 500

    def get_lr(step):
        if step < warmup:
            return lr_min + (lr_max - lr_min) * (step / max(1, warmup))
        progress = (step - warmup) / max(1, num_steps - warmup)
        progress = min(progress, 1.0)
        return lr_min + 0.5 * (lr_max - lr_min) * (1.0 + math.cos(math.pi * progress))

    # Pre-encode all stimuli
    print("  Pre-encoding stimuli...")
    all_perceptual = PERCEPTUAL_STIMULI[:]
    all_language = LANGUAGE_STIMULI[:]
    all_reasoning = REASONING_STIMULI[:]
    all_memory = MEMORY_STIMULI[:]
    all_social = SOCIAL_STIMULI[:]

    for step in range(start_from, num_steps):
        lr = get_lr(step)
        progress = (step - start_from) / max(1, num_steps - start_from)

        # ---- Phase 1: Domain-specific specialist training ----
        r = random.random()

        if r < 0.20:
            # Perceptual training
            text = random.choice(all_perceptual)
            orchestrator.train_specialist("perceptual", text,
                                          domain_hint="perception",
                                          learning_rate=lr, confidence=0.6)

        elif r < 0.40:
            # Language training
            word, desc = random.choice(all_language)
            orchestrator.train_specialist("language", desc,
                                          domain_hint="language",
                                          learning_rate=lr, confidence=0.7)

        elif r < 0.55:
            # Reasoning training
            fact, domain = random.choice(all_reasoning)
            orchestrator.train_specialist("reasoning", fact,
                                          domain_hint=domain,
                                          learning_rate=lr, confidence=0.7)

        elif r < 0.70:
            # Memory training
            fact, domain = random.choice(all_memory)
            orchestrator.train_specialist("memory", fact,
                                          domain_hint=domain,
                                          learning_rate=lr, confidence=0.6)

        elif r < 0.82:
            # Social training
            text, domain = random.choice(all_social)
            orchestrator.train_specialist("social", text,
                                          domain_hint=domain,
                                          learning_rate=lr, confidence=0.6)

        elif r < 0.90:
            # Routed processing — let the router decide
            all_stimuli = (
                [(t, "perception") for t in all_perceptual] +
                [(d, "language") for _, d in all_language] +
                [(f, dom) for f, dom in all_reasoning] +
                [(f, dom) for f, dom in all_memory] +
                [(t, dom) for t, dom in all_social]
            )
            text, domain = random.choice(all_stimuli)
            orchestrator.process_stimulus(text, domain_hint=domain,
                                          learning_rate=lr)

        else:
            # Cross-training — teach a specialist from another's domain
            # This builds generalization and cross-brain integration
            brain_names = list(orchestrator.brains.keys())
            brain_names = [n for n in brain_names if n != "executive"]
            if len(brain_names) >= 2:
                source_brain = random.choice(brain_names)
                target_brain = random.choice([n for n in brain_names
                                               if n != source_brain])
                # Pick stimulus from source domain, teach to target
                stimuli_map = {
                    "perceptual": [(t, "perception") for t in all_perceptual],
                    "language": [(d, "language") for _, d in all_language],
                    "reasoning": all_reasoning,
                    "memory": all_memory,
                    "social": all_social,
                }
                source_stimuli = stimuli_map.get(source_brain, [])
                if source_stimuli:
                    text, domain = random.choice(source_stimuli)
                    orchestrator.train_specialist(
                        target_brain, text, domain_hint=domain,
                        learning_rate=lr * 0.5,  # Lower LR for cross-training
                        confidence=0.4)

        # ---- Phase 2: Executive brain learns routing ----
        if step % 10 == 0:
            # Train executive on routing examples
            all_stimuli = (
                [(t, "perception") for t in all_perceptual[:5]] +
                [(d, "language") for _, d in all_language[:5]] +
                [(f, dom) for f, dom in all_reasoning[:5]] +
                [(f, dom) for f, dom in all_memory[:5]] +
                [(t, dom) for t, dom in all_social[:5]]
            )
            text, domain = random.choice(all_stimuli)
            # Executive learns the routing pattern itself
            orchestrator.train_specialist("executive", text,
                                          domain_hint=domain,
                                          learning_rate=lr * 0.5,
                                          confidence=0.5)

        # ---- Phase 3: LNN temporal step ----
        if step % 5 == 0:
            for name, brain in orchestrator.brains.items():
                try:
                    features = orchestrator.composer.compose(
                        text="temporal update", brain_name=name)
                    brain.brain.lnn_forward_step(features[:64])
                except Exception:
                    pass

        # ---- Progress reporting ----
        if (step + 1) % 500 == 0:
            print(f"\n  [Ensemble] Step {step+1}/{num_steps} — lr={lr:.6f}")
            for name in sorted(orchestrator.brains.keys()):
                brain = orchestrator.brains[name]
                avg_loss = brain.get_avg_loss(200)
                print(f"    {name:12s}: loss={avg_loss:.4f} "
                      f"steps={brain.total_steps}")
            print(f"  Routing:\n{orchestrator.router.summary()}")
            agreement = orchestrator.bus.get_agreement_score()
            lat = orchestrator.callosum.get_lateralization_score()
            dom = orchestrator.callosum.get_dominant_hemisphere()
            print(f"  Cross-brain agreement: {agreement:.3f}  "
                  f"Lateralization: {lat:.3f} ({dom})")

        if (step + 1) % 2000 == 0:
            orchestrator.print_status()

        # ---- Checkpointing ----
        if (step + 1) % 5000 == 0:
            print(f"  [Checkpoint] Saving all brains...")
            orchestrator.save_all()

    # Final save
    orchestrator.save_all()
    orchestrator.print_status()


# ============================================================================
# Evaluation
# ============================================================================

EVAL_PROBES = [
    ("dog", "A friendly dog with soft fur", "language"),
    ("sun", "The bright warm sun in the sky", "perceptual"),
    ("math", "Two plus two equals four", "reasoning"),
    ("river", "Rivers flow from mountains to the sea", "memory"),
    ("kind", "Being kind to others is important", "social"),
    ("rain", "Drops of rain falling from clouds", "perceptual"),
    ("book", "A book with pages full of stories", "language"),
    ("earth", "The Earth orbits the sun once every year", "reasoning"),
]


def evaluate_ensemble(orchestrator):
    """Evaluate ensemble performance with fixed probes."""
    print(f"\n  {'─' * 56}")
    print(f"  ENSEMBLE EVALUATION")
    print(f"  {'─' * 56}")

    for probe_name, probe_text, expected_brain in EVAL_PROBES:
        # Route through ensemble
        primary = orchestrator.router.get_primary(probe_text)
        results, losses = orchestrator.process_stimulus(probe_text)

        # Show which brain(s) responded
        loss_str = ", ".join(f"{n}={l:.4f}" if l else f"{n}=?"
                             for n, l in losses.items())
        merged = orchestrator.merge_responses(results)
        norm = float(np.linalg.norm(merged)) if merged is not None else 0.0

        print(f"    {probe_name:8s} → routed={primary:12s}  "
              f"expected={expected_brain:12s}  |merged|={norm:.2f}")
        print(f"             losses: {loss_str}")

    print(f"  {'─' * 56}\n")


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Athena Ensemble — Multi-Brain Architecture")
    parser.add_argument("--resume", action="store_true",
                        help="Resume from checkpoint")
    parser.add_argument("--fresh", action="store_true",
                        help="Start fresh (ignore checkpoints)")
    parser.add_argument("--no-claude", action="store_true",
                        help="Disable Claude parent")
    parser.add_argument("--steps", type=int, default=30000,
                        help="Total training steps")
    parser.add_argument("--monolithic", action="store_true",
                        help="Fall back to single-brain mode (runs immerse_athena.py)")
    args = parser.parse_args()

    if args.monolithic:
        print("  Falling back to monolithic single-brain mode...")
        import immerse_athena
        immerse_athena.main()
        return

    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s [%(name)s] %(message)s")

    # Handle graceful shutdown
    shutdown_event = Event()

    orchestrator = EnsembleOrchestrator(
        no_claude=args.no_claude,
        resume=args.resume,
        fresh=args.fresh)

    orchestrator.initialize()

    def signal_handler(sig, frame):
        print("\n  Graceful shutdown... saving all brains...")
        orchestrator.save_all()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    # Initial evaluation
    evaluate_ensemble(orchestrator)

    # Train
    run_ensemble_training(orchestrator, num_steps=args.steps)

    # Final evaluation
    evaluate_ensemble(orchestrator)

    print("\n  Ensemble training complete.")
    print(f"  Total steps: {orchestrator.step_count}")
    for name, brain in orchestrator.brains.items():
        print(f"    {name}: {brain.total_steps} steps, "
              f"avg_loss={brain.get_avg_loss():.4f}")


if __name__ == "__main__":
    main()
