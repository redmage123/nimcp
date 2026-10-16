# Athena Training Pipeline — What Gets Passed to the Brain

**Version**: 2.6.3
**Date**: March 2026

This document describes exactly what data flows into and out of Athena's brain during developmental training via `scripts/immerse_athena.py`.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Embedding Model](#2-embedding-model)
3. [Input Vector Layout (1024-dim)](#3-input-vector-layout-1024-dim)
4. [Target Vector Layout (2048-dim)](#4-target-vector-layout-2048-dim)
5. [Stimulus Sources](#5-stimulus-sources)
6. [Per-Step Execution Flow](#6-per-step-execution-flow)
7. [Stage 0: Sensory Awakening](#7-stage-0-sensory-awakening)
8. [Stage 1: Association](#8-stage-1-association)
9. [Stage 2: Conceptual](#9-stage-2-conceptual)
10. [Stage 3: Reasoning](#10-stage-3-reasoning)
11. [Biological Clock and Sleep Cycles](#11-biological-clock-and-sleep-cycles)
12. [Decoder and Vocabulary Bank](#12-decoder-and-vocabulary-bank)
13. [Key Design Observations](#13-key-design-observations)

---

## 1. Overview

Athena is not trained as a classifier or predictor. She is a developing biological brain. Her 2048-dim output is a continuous neural response — not a label or class prediction. Training uses `learn_vector()` with dense semantic targets and MSE loss. The loss represents how new an experience is, not prediction error in the traditional sense.

The training pipeline is structured as four developmental stages modeled on infant cognitive development:

| Stage | Name | Goal | Stimuli |
|-------|------|------|---------|
| 0 | Sensory Awakening | Ground perception in raw sensory experience | Short sensory descriptions |
| 1 | Association | Bind percepts to names (cross-modal) | Object name + rich description pairs |
| 2 | Conceptual | Learn abstract relationships and categories | Conceptual relationships and properties |
| 3 | Reasoning | Causal inference, analogy, abstraction | Questions requiring multi-step reasoning |

Each stage runs for 20,000 stimuli by default.

---

## 2. Embedding Model

All text is converted to dense vectors using **sentence-transformers `all-MiniLM-L6-v2`**, which produces **384-dimensional** embeddings.

```python
# From scripts/claude_teacher.py
from sentence_transformers import SentenceTransformer
model = SentenceTransformer('all-MiniLM-L6-v2')
embedding = model.encode(text, convert_to_numpy=True)  # -> float32[384]
```

This model is used everywhere: input composition, target generation, and decoder vocabulary matching.

---

## 3. Input Vector Layout (1024-dim)

The `SensoryComposer` class (`immerse_athena.py:125`) builds a 1024-dimensional input vector with four sections:

```
[0                16               528              912             1024]
 |--- Tag (16) ---|-- Primary (512) --|-- Text (384) --|-- Context (112) --|
```

### Tag Section `[0:16]` — Modality Flags and Brain State

| Index | Content |
|-------|---------|
| 0 | Text modality flag (1.0 if text input) |
| 1 | Visual modality flag |
| 2 | Audio modality flag |
| 3 | Speech modality flag |
| 4 | Ethics modality flag |
| 5 | Imagination modality flag |
| 6-7 | Reserved (zero) |
| 8 | Live arousal level from medulla |
| 9 | Sleep state (0.0-1.0, from `sleep_get_state() / 4.0`) |
| 10 | Circadian efficiency |
| 11 | Substrate health (OPTIMAL=1.0, STRESSED=0.7, COMPROMISED=0.4, CRITICAL=0.1) |
| 12-15 | Reserved (zero) |

### Primary Features `[16:528]` — 512 dims

For text-modality stimuli, this is the **384-dim MiniLM embedding** of the stimulus text, zero-padded to 512 dims. For other modalities, raw feature vectors can be placed here directly.

### Text Semantic Embedding `[528:912]` — 384 dims

The **same 384-dim MiniLM embedding** of the stimulus text. For text-modality input, this is identical to the primary features section. This duplication is by design — it ensures the semantic content is available in a fixed, known location regardless of what occupies the primary features slot in multi-modal scenarios.

### Biological Context `[912:1024]` — 112 dims

Live readings from Athena's brain subsystems, queried at each step:

| Offset | Dims | Source |
|--------|------|--------|
| +0 | 1 | Arousal (`medulla_get_arousal()`) |
| +1 | 1 | Sleep pressure (`sleep_get_pressure()`) |
| +2 | 1 | Circadian efficiency (`medulla_get_circadian_efficiency()`) |
| +3 | 1 | Dopamine level (`bg_get_dopamine()`) |
| +4 | 1 | Reward prediction error (`bg_get_rpe()`) |
| +5 | 1 | Conflict signal (`bg_get_conflict()`) |
| +6 | 1 | Basal ganglia mode (`bg_get_mode()`) |
| +7 | 1 | ATP level (`substrate_get_metabolic()["atp"]`) |
| +8 | 1 | Metabolic capacity (`substrate_get_metabolic()["capacity"]`) |
| +9-15 | 7 | Reserved (zero) |
| +16-47 | 32 | Liquid Neural Network state (`lnn_get_state()`) |
| +48-111 | 64 | Reserved (zero) |

These context dims create a feedback loop: the brain's internal state influences its next input, which is how biological brains work — arousal, fatigue, reward signals all modulate perception.

---

## 4. Target Vector Layout (2048-dim)

The target is generated by `make_semantic_target()` (`immerse_athena.py:220`):

```python
def make_semantic_target(text, target_dim=2048):
    emb = encode_text(text)  # 384-dim MiniLM embedding
    target = np.zeros(target_dim, dtype=np.float32)
    for i in range(0, target_dim, len(emb)):
        n = min(len(emb), target_dim - i)
        target[i:i + n] = emb[:n]
    return target.tolist()
```

The 384-dim embedding is **tiled** across 2048 dims:

```
[emb(384) | emb(384) | emb(384) | emb(384) | emb(384) | emb(128)]
 0         384        768        1152       1536       1920    2048
```

This tiling ensures the semantic signal is present across all output neurons, giving the multi-layer diamond architecture consistent gradient signal throughout the output layer.

### What varies by stage

| Stage | Target text | Example |
|-------|-------------|---------|
| 0 | The stimulus description | `"warm sunlight on skin"` |
| 1 | Name + description concatenated | `"dog A friendly dog with soft fur..."` |
| 2 | Conceptual relationship text | `"dogs are animals that are loyal"` |
| 3 | Full reasoning answer | `"The ice melts because heat transfers..."` |

---

## 5. Stimulus Sources

### Stage 0: Two pools (50/50 random selection)

**Pool A — `StimulusSource.SENSORY`** (81 entries):
Hardcoded sensory descriptions spanning five modality categories:
- **Visual** (25): "warm sunlight on skin", "the color red", "flickering candlelight", "frost on glass"
- **Tactile** (15): "soft fur of a sleeping cat", "rough bark of a tree", "ice melting in your palm"
- **Auditory** (17): "birds singing at dawn", "rain tapping on a roof", "a heartbeat rhythm"
- **Olfactory/Gustatory** (11): "the smell of fresh bread", "chocolate melting", "ocean salt air"
- **Emotional/Somatic** (11): "a warm hug", "butterflies in the stomach", "waking up refreshed"

**Pool B — `generate_sensory_exposure()`** (38 entries):
Similar hardcoded descriptions with more spatial/dynamic content:
- "bright red ball rolling across the floor"
- "shadows moving on the wall as clouds pass by"
- "bubbles floating up and popping in the air"
- "waves crashing on the shore rhythmically"

Each step randomly selects from Pool A or Pool B with equal probability.

### Stage 1: `StimulusSource.OBJECTS`

(name, description) pairs across categories:
- **Animals** (15): ("dog", "A friendly dog with soft fur that wags its tail when happy")
- **Nature** (15): ("tree", "A tall tree with green leaves reaching up to the sky")
- **Food** (6): ("apple", "A red apple that tastes sweet and crunchy")
- **Objects** (10): ("book", "A book with pages full of stories and knowledge")
- **Body** (5): ("hand", "A hand with five fingers that can hold and create")

### Stages 2-3

Increasingly complex stimuli involving relationships, categories, causation, and multi-step reasoning. See `StimulusSource` class in `immerse_athena.py` for the full corpus.

---

## 6. Per-Step Execution Flow

For each of the 20,000 stimuli in Stage 0, the following happens:

```
1. clock.tick(brain)
   |-- May trigger sleep cycle or memory consolidation

2. Pick stimulus text (50% StimulusSource.SENSORY, 50% generate_sensory_exposure)

3. parent.first_experience(brain, composer, description)
   |
   |-- narration = pop pre-generated parent speech (e.g., "Look how it moves!")
   |-- features = composer.compose(text=description, modality="text")
   |     |-- encode_text(description) -> 384-dim MiniLM embedding
   |     |-- Pack into 1024-dim: [tag:16 | primary:512 | text:384 | context:112]
   |     |-- Context section filled with live brain state readings
   |
   |-- target = make_semantic_target(description)
   |     |-- encode_text(description) -> 384-dim
   |     |-- Tile to 2048-dim
   |
   |-- result = brain.decide_full(features)
   |     |-- Full inference through all 33 brain regions
   |     |-- Returns 2048-dim output vector + decision metadata
   |
   |-- loss = brain.learn_vector(features, target, label=description[:50], confidence=0.5)
   |     |-- MSE loss between actual output and tiled embedding target
   |     |-- Backpropagation through multi-layer diamond architecture
   |     |-- tanh output activation, gradient clipping at 5.0
   |
   |-- brain.bg_update_reward(0.5, 0.3)
   |     |-- Gentle positive reward to basal ganglia
   |     |-- Reward=0.5 (mild positive), confidence=0.3
   |
   |-- decoder.record_pair(output_vec, target_emb, description)
         |-- Stores (output, embedding, text) for vocabulary bank

4. brain.lnn_forward_step(features[:128])
   |-- Liquid Neural Network temporal context update
   |-- Uses first 128 dims of input as ODE forcing function

5. Every 500 steps: progress report + performance evaluation
6. Every 2000 steps: parent.inspire() — motivational interaction
7. Every 5000 steps: checkpoint saved to disk
```

---

## 7. Stage 0: Sensory Awakening

**Goal**: Ground the brain in raw sensory perception. Like a newborn experiencing light, sound, touch for the first time.

**Configuration**:
- 20,000 stimuli (default)
- Confidence: 0.5 (low — these are gentle impressions, not strong corrections)
- Reward: 0.5 reward, 0.3 confidence (existence is rewarded)
- Plasticity state: default (EXPLORATION implied)

**Warmup phase** (first run only, steps 0-74):
25 diverse stimuli repeated 3x each with higher confidence (0.9 first pass, 0.85 repeats) to break weight symmetry and establish distinct initial representations.

**What the brain learns**: To produce output embeddings that approximate the MiniLM semantic encoding of sensory descriptions. The multi-layer diamond architecture learns a mapping from the structured 1024-dim input space to the 2048-dim tiled semantic space, while biological mechanisms (STDP, homeostatic plasticity, sleep consolidation) shape the internal representations.

---

## 8. Stage 1: Association

**Goal**: Cross-modal binding — seeing/hearing a concept and its name should produce the same internal representation.

**Key difference from Stage 0**: The target combines name + description (`"dog A friendly dog with soft fur..."`), so the brain must associate compact labels with rich descriptions.

**Configuration**:
- 20,000 stimuli
- Plasticity state: ACQUISITION
- Reward: 0.6 reward, 0.4 confidence (slightly stronger reinforcement)
- Cerebellum error signal when loss > 0.5

---

## 9. Stage 2: Conceptual

**Goal**: Abstract relationships — "dogs are animals", "rain comes from clouds", category membership, properties.

**Key difference**: Stimuli involve relationships between concepts rather than raw percepts or simple naming.

---

## 10. Stage 3: Reasoning

**Goal**: Causal inference, analogy, and multi-step reasoning.

**Key difference**: Uses `brain.decide_full()` for Q&A probes where the brain must reason about "why" questions. Performance evaluation checks whether the brain's output embedding is closest to the correct answer in the decoder vocabulary.

---

## 11. Biological Clock and Sleep Cycles

The `BiologicalClock` manages circadian-like rhythms:

- **Tick**: Called every step, tracks elapsed time
- **Sleep**: Periodically triggers sleep cycles where the brain consolidates memories (`brain.sleep_cycle()`)
- **Consolidation**: Memory replay and synaptic scaling between sleep cycles

This means training is not continuous — the brain periodically "rests", during which no new stimuli are presented but internal reorganization occurs.

---

## 12. Decoder and Vocabulary Bank

The `NeuralDecoder` (`scripts/neural_decoder.py`) maintains a `VocabularyBank` that maps brain outputs to human-readable text:

- **Size**: Up to 115,000 entries
- **Each entry**: (text, 384-dim embedding) pair
- **Lookup**: Brute-force cosine similarity over all entries
- **Purpose**: Display only — the decoder does not influence training

During training, `decoder.record_pair()` stores the brain's actual output alongside the target embedding and stimulus text. Periodically (`decoder.force_refit()`), the decoder rebuilds its internal mappings.

The decoder is how we observe what Athena's responses are "closest to" in semantic space, but this is purely for human interpretability — the brain has no access to the decoder.

---

## 13. Key Design Observations

### Duplicated embedding in input
For text-modality stimuli, the MiniLM embedding appears twice in the input vector: once in the primary features slot `[16:400]` and once in the text embedding slot `[528:912]`. This is intentional — in multi-modal scenarios (Stage 2+), the primary slot would contain visual or audio features while the text slot always carries the semantic embedding. For Stage 0 (text-only), the duplication provides redundant signal.

### Autoencoder-like objective
The core learning task is: MiniLM embedding in → tiled MiniLM embedding out. The brain learns an approximate identity mapping through its full biological architecture. The value is not in the mapping itself but in the internal representations that form along the way — shaped by STDP, homeostatic plasticity, sleep consolidation, immune monitoring, and the multi-layer diamond topology.

### Biological context as feedback loop
The 112-dim context section creates a closed loop: the brain's state (arousal, dopamine, fatigue) feeds into its next input, which affects its processing, which changes its state. This is fundamentally different from standard neural networks where inputs are independent of model state.

### Confidence as developmental signal
Stage 0 uses low confidence (0.5) — gentle impressions. Stage 1 increases to higher confidence. This mimics how infant learning progresses from passive exposure to active, confident teaching.

### Reward is unconditional in Stage 0
Every stimulus gets `bg_update_reward(0.5, 0.3)` — mild positive reward regardless of performance. Existence is rewarded. This establishes a positive baseline before later stages introduce performance-dependent reward signals.

---

## Source Files

| File | Role |
|------|------|
| `scripts/immerse_athena.py` | Main training orchestrator |
| `scripts/claude_teacher.py` | `encode_text()`, Claude integration, lesson specs |
| `scripts/neural_decoder.py` | `NeuralDecoder`, `VocabularyBank` for output interpretation |
| `scripts/talk_to_athena.py` | `extract_embedding_from_output()` utility |

---

*This document describes the training pipeline as of NIMCP v0.9.0-beta (March 2026).*
