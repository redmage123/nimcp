# Python Modules Reference

**Last Updated:** 2026-04-19

Each module here lives in `scripts/` and addresses a specific gap in the
default training/learning pipeline. All modules are designed to be
**opt-in** — import and use where wanted, nothing breaks if unused.

## Target Brain Architecture

These modules are designed around the **current default apportionment**:

| Network | Neurons | Role |
|---------|--------:|------|
| ANN (adaptive) | 150,000 | Teacher / gradient backbone |
| SNN (spiking)  | 1,800,000 | Primary learner (R-STDP + homeostasis) |
| LNN (liquid)   | 512 | Temporal ODE integration |
| CNN per-cortex | ~500K params × 4 | Modality features |

All modules gracefully degrade if the brain's network configuration
differs — checks via `hasattr(brain, ...)` for optional APIs.

## scripts/curiosity_selector.py

**Purpose:** Augment fixed stimulus schedule with curiosity-aware selection.

**Class:** `CuriositySelector(brain, source, bias=0.4, recent_history_n=20)`

**Protocol:**
```python
from curiosity_selector import CuriositySelector

sel = CuriositySelector(brain, source, bias=0.4)
name, description = sel.pick_object()   # curiosity-biased or fallback
desc, expected = sel.pick_fact(preferred_domain="language")
stats = sel.stats()  # curiosity_hits / fallback_hits / curiosity_rate
```

**Fallback:** If `brain.curiosity_detect_gaps` raises or returns empty,
always falls through to `source.get_*()`. Zero breakage risk.

**Unit tests:** `tests/unit/test_curiosity_selector.py` (4 tests).

## scripts/curriculum.py

**Purpose:** Start with limited category variety, expand with training
progress. Prevents overwhelming a fresh brain with full-scope stimuli.

**Class:** `ProgressiveCurriculum(source, stages=None, category_extractor=None)`

**Default stages:**
```
(end_step, n_categories):
    (200,  5)
    (500,  10)
    (1000, 20)
    (∞,    unlimited)
```

**Usage:**
```python
from curriculum import ProgressiveCurriculum

c = ProgressiveCurriculum(source)
for step in range(total):
    c.advance(step)
    name, desc = c.pick_object()
```

**Unit tests:** `tests/unit/test_curriculum.py` (4 tests).

## scripts/symbolic_writer.py

**Purpose:** Engage the symbolic substrate (KG, semantic memory,
hippocampal episodes) alongside vector-gradient learning. Closes the gap
where training only exercises the vector pipeline.

**Class:** `SymbolicWriter(brain, episode_valence=0.1, verbose=False)`

**Usage:**
```python
from symbolic_writer import SymbolicWriter, symbolic_learn_vector

sw = SymbolicWriter(brain)
loss = brain.learn_vector(features, target, label="dog")
sw.record(label="dog", modality="visual",
          context={"description": "furry animal"})

# Or use the convenience wrapper:
loss = symbolic_learn_vector(brain, features, target,
                               label="dog", writer=sw,
                               context={"description": "..."})
```

**Tries (in order):** `semantic_memory_insert`, `kg_add_fact` / `ti_add_fact`,
`hippocampus_seed_episode`. Each best-effort.

**Unit tests:** `tests/unit/test_symbolic_writer.py` (4 tests).

## scripts/synthesized_sensory.py

**Purpose:** Enrich each training exposure from "image + text" to a
multi-channel tuple (haptic/gaze/audio/emotion/multi-view). Closes the
per-exposure information gap vs biological learning without real sensors.

**Class:** `SynthesizedSensoryComposer(base_composer=None, seed=1)`

**Usage:**
```python
from synthesized_sensory import SynthesizedSensoryComposer

comp = SynthesizedSensoryComposer()
rich = comp.compose_rich(name="dog", description="friendly furry animal")
# rich is a dict with keys:
#   text, haptic, audio_tag, gaze, emotion, visual_features,
#   category, visual_variants

# Multi-view augmentation:
views = comp.multi_view("dog", "...", n_variants=4)
# list of 4 rich dicts, each with a different "view" tag
```

**Category inference:** `_infer_category()` — word-boundary match across
8 categories (animal/plant/vehicle/toy/food/furniture/container/sky),
falls back to "default".

**Unit tests:** `tests/unit/test_synthesized_sensory.py` (5 tests).

## scripts/gradient_accumulator.py

**Purpose:** Preserve sequential (batch=1) training — required by Athena's
biological stability — while providing the *statistical* benefit of batched
training via rolling LR smoothing.

**Class:** `GradientAccumulator(window=32, min_lr=1e-6, max_lr=1e-1,
                                  target_grad_norm=1.0)`

**Usage:**
```python
from gradient_accumulator import GradientAccumulator, accumulating_learn_vector

acc = GradientAccumulator(window=32, target_grad_norm=1.0)
for features, target, label in stream:
    loss = accumulating_learn_vector(
        brain, features, target,
        label=label, accumulator=acc, base_lr=0.01)
# LR auto-scales based on recent gradient norms
```

**Mechanism:** Tracks per-sample loss + gradient norm in a sliding window.
`smoothed_lr(base_lr)` returns LR scaled by `target_grad_norm / mean_gn`,
clamped to [0.5×, 2×] per call and [min_lr, max_lr] absolute.

**Unit tests:** `tests/unit/test_gradient_accumulator.py` (6 tests).

## scripts/childhood_memories/

**Purpose:** Generate + implant synthetic developmental substrate (500-2000
concepts, 100-500 episodes, phonological templates, narrative identity).

### generator.py — `MemoryGenerator`

```python
from childhood_memories import MemoryGenerator

gen = MemoryGenerator(output_dir="data/implanted_memories/v1",
                      seed=42, vector_dim=1024,
                      reference_timestamp=1700000000.0)  # for determinism
summary = gen.generate_all()
# Writes 6 JSON files: concepts, kg_triples, episodes, phonological,
# narrative_identity, manifest
```

**Determinism (2026-04-19):** With fixed `seed` AND `reference_timestamp`,
output is byte-identical across runs.

### implanter.py — `MemoryImplanter`

```python
from childhood_memories import MemoryImplanter

imp = MemoryImplanter(brain, memory_dir="data/implanted_memories/v1")
result = imp.implant_all()
print(result.summary())
# e.g. "concepts: 48/48  kg: 110/110  episodes: 200/200  phono: 0/73  ..."
```

Tries `semantic_memory_insert`, `kg_add_fact`, `hippocampus_seed_episode`,
`vocabulary_add_word`, `self_model_set_narrative`. Best-effort per layer.

### verifier.py — `verify_retrievable`

```python
from childhood_memories import verify_retrievable

vr = verify_retrievable(brain, memory_dir="...", sample_n=20)
print(vr.summary())  # retrieval rates per layer
```

### run_implantation.py — CLI entry point

```bash
python3 scripts/childhood_memories/run_implantation.py generate \
    --output-dir data/implanted_memories/v1 --seed 42

python3 scripts/childhood_memories/run_implantation.py implant \
    --memory-dir data/implanted_memories/v1 \
    --socket /var/run/athena/brain.sock --verify

python3 scripts/childhood_memories/run_implantation.py full \
    --output-dir data/implanted_memories/v1 --verify
```

**Unit tests:** `tests/unit/test_childhood_memories.py` (6 tests).

## scripts/compressed_replay.py

**Purpose:** Replay salient recent experiences sequentially (batch=1) at
maximum GPU rate between wake steps. Simulates the compression of 8-hour
sleep-state consolidation into seconds of wall time.

**Class:** `CompressedReplayer(brain, capacity=1000, salience_bias=0.7,
                                 min_replay_interval_s=0.05)`

**Usage:**
```python
from compressed_replay import CompressedReplayer

r = CompressedReplayer(brain, capacity=1000)
# Record during normal training:
r.record_from_training_step(features, target, label="dog",
                              loss=0.3, baseline_loss=1.0)
# Or directly:
r.record(features, target, label="dog", salience=0.8)

# Replay burst (sequential, not parallel):
stats = r.replay_burst(n=50, target_wallclock_s=10)
```

**Key design:** Single-sample replay — no gradient explosion, no SNN
saturation. Salience-weighted sampling via `salience^(bias*3)`.

**Unit tests:** `tests/unit/test_compressed_replay.py` (6 tests).

## scripts/symbolic_consultation.py

**Purpose:** Augment vector-pipeline inference with symbolic substrate
queries. Makes the knowledge graph, semantic memory, and episodic memory
active participants at inference time, not just at learning time.

**Class:** `SymbolicConsultant(brain, vector_weight=0.6, symbolic_weight=0.4)`

**Usage:**
```python
from symbolic_consultation import SymbolicConsultant

consultant = SymbolicConsultant(brain)
result = consultant.decide(features, concept_hint="dog", query="dog")
# result.vector_output       — vector pipeline output
# result.symbolic_facts      — queried facts with sources
# result.blended_answer       — combined answer
# result.blended_confidence   — weighted confidence
```

Consults (in order): vector pipeline (`decide_full` / `predict`), KG
(`kg_query`), semantic memory (`semantic_memory_query`), episodic
(`episodic_memory_search`). Each best-effort.

**Unit tests:** `tests/unit/test_symbolic_consultation.py` (6 tests).

## scripts/reconstructive_recall.py

**Purpose:** Episodic recall that reconstructs plausible detail from a
compact gist + semantic schema, not pure playback. Bartlett (1932) schema
theory — matches biological phenomenology.

**Class:** `ReconstructiveRecaller(brain, schema_confidence_weight=0.3)`

**Usage:**
```python
from reconstructive_recall import ReconstructiveRecaller

recaller = ReconstructiveRecaller(brain)
memory = recaller.recall("dog", concept_for_schema="dog")
# memory.gist              — retrieved gist text
# memory.retrieved_details — directly stored attributes
# memory.schema_fillers    — filled in from semantic memory
# memory.confidence         — weighted by retrieved vs filled ratio
# memory.reconstructed      — True if schema fillers were used
print(memory.as_narrative())
```

**Key invariant:** Retrieved details always take priority over schema
fillers — a `color="brown"` recalled attribute isn't overwritten by
`color="varies"` from the schema.

**Unit tests:** `tests/unit/test_reconstructive_recall.py` (6 tests).

## scripts/innate_priors.py

**Purpose:** Evolution-equivalent structural priors applied at brain init.
V1 Gabor filters, auditory Mel bands, hippocampal place cells, fusiform
face template, somatosensory homunculus.

**Class:** `InnatePriors`

**Usage:**
```python
from innate_priors import InnatePriors, apply_all_priors

priors = InnatePriors()
v1 = priors.gabor_filters(n_orientations=8, n_scales=4, size=9)
audio = priors.frequency_band_filters(n_bands=32)
place = priors.place_cell_grid(n_cells=256, grid_size=16)
face = priors.face_detector_template(size=16)
body = priors.body_schema_map(n_regions=32)

# Or all at once:
summary = apply_all_priors(brain)
# summary["generated"]: names of all 5 prior sets
# summary["applied"]:   names applied via brain.innate_hardwire
```

Returns `PriorSet` objects with `.filters` (list of flat arrays) and
`.metadata` (generation params + description).

**Unit tests:** `tests/unit/test_innate_priors.py` (8 tests).

## scripts/stage_gate.py

**Purpose:** All-metrics-pass gate blocking stage-to-stage transitions
until ALL criteria pass 3 consecutive checks.

**Class:** `StageGate` with factory `stage_gate_for(stage: int)`

Six criteria for Stage 1:
1. Minimum steps (≥1500)
2. Mean loss < 1.0 over 200-step window
3. Loss plateau (Δ<5% across two successive 200-step windows)
4. ≥48/50 non-zero outputs
5. SNN sparsity in 0.88-0.99 (firing 1-12%)
6. Chat eval: coherence>0.30, similarity>0.20, cross-sim<0.90

Event logging via `log_gate_event(path, stage, step, result)`.

## See Also

- [10_training_paradigm.md](10_training_paradigm.md) — how these fit the training loop
- [60_test_infrastructure.md](60_test_infrastructure.md) — test coverage
- [../api/python_api.md](../api/python_api.md) — underlying Brain API
