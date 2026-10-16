# Cognitive Layer Architecture

**Last Updated:** 2026-04-19

## Overview

Athena has multiple representational layers operating in parallel. Historically,
training has exercised only the vector/gradient pipeline, leaving symbolic
structures (knowledge graph, semantic memory, episodic traces) under-populated.

The 2026-04 symbolic-writer addition (Phase B.1) begins to engage all layers.

## Layer Map

| Layer | Substrate | Size | API surface |
|-------|-----------|-----:|-------------|
| Dense embeddings | `adaptive_network_t` weights | 150K neurons | `predict`, `learn_vector` |
| Spiking temporal coding | SNN populations | 1.8M neurons | `snn_get_stats`, R-STDP |
| Continuous-time dynamics | LNN | 512 neurons | `lnn_forward_step`, adjoint |
| Visual / audio features | CNN + cortex processors | 4 × ~500K params | `init_cortex_cnns` |
| Semantic knowledge | Knowledge graph | ~10K nodes potential | `ti_add_fact` (sparse) |
| Episodic memory | Hippocampus + memory store | ~100K episodes | `hippocampus_seed_episode` |
| Working memory | Transient buffers | ~7±2 slots | (internal only) |
| Self-model | Introspection | — | `get_internal_state` |
| Emotional state | Emotional system | — | `get_emotion_state` |
| Attention | Thalamus nuclei | 8 nuclei | `thalamus_set_attention` |

**The biological primary learner is the 1.8M-neuron SNN.** The 150K ANN
acts as a gradient teacher. This is reflected in the training loop:
SNN step time dominates (~83% of per-step cost pre-optimization).

## Multi-Representation Learning (Phase B.1)

`scripts/symbolic_writer.py` wraps `learn_vector` to also write:

```
learn_vector(features, target, label="dog")
    ↓
    ├── Vector pipeline: gradients flow, weights update
    │
    └── SymbolicWriter.record(label="dog", modality="visual",
                               context={"description": "..."})
        ├── KG assertion: (dog, observed_with, visual)
        ├── Hippocampal episode: timestamped "observed dog"
        └── Semantic memory node: dog + attributes
```

All symbolic writes are best-effort — if a brain doesn't expose the API,
the write is skipped without raising. See unit tests in
`tests/unit/test_symbolic_writer.py`.

## Symbolic vs Subsymbolic — Per Operation

| Operation | Vector layer | Symbolic layer |
|-----------|:------------:|:--------------:|
| Learn new label | ✓ | ✓ (with writer) |
| Inference | ✓ | (planned — D.2) |
| Memory recall | ✓ (dense vec) | (planned — D.3) |
| Reasoning | (limited) | ✓ (abduction, KG forward-chain) |
| Perception | ✓ | (tagging via cortex) |

## Memory Systems (Multi-Store)

Athena has distinct memory substrates:

### Episodic (Hippocampal)
- Timestamped experiences with modality tags
- Currently: written during consolidation cycles
- Planned (D.3): reconstructive recall (gist + semantic fill-in, not playback)

### Semantic (Decontextualized)
- Concept nodes with relations
- Populated by symbolic_writer on learn_vector
- Queried via forward-chaining (`ti_forward_chain`)

### Procedural (Motor / Habit)
- Currently minimal — mostly basal ganglia action selection

### Working (Active Scratchpad)
- Transient buffers in cognitive modules
- Capacity ~ human 7±2 (per design)

### Implicit / Statistical
- Dense embeddings in ANN weights
- The "default" learning target; everything else layers on top

## Inner Speech & Dialogue

### Inner Speech (`cognitive/language/nimcp_inner_speech.h`)
- Refinement loop: generate text → re-encode → blend
- Up to 16 iterations, convergence threshold 0.95
- Captures intermediate generations

### Inner Dialogue (`cognitive/inner_dialogue/nimcp_inner_dialogue.h`)
- Multi-perspective turn-taking: Analytical / Emotional / Critical / Creative
- Circular buffer of turn history
- Deadlock detection + convergence

## Intrinsic Motivation

### Curiosity (`curiosity_detect_gaps`)
- Reports concepts with poor coverage / high uncertainty
- Now drives stimulus selection (Phase A.3, `scripts/curiosity_selector.py`)

### Reward System (Basal Ganglia)
- `bg_update_reward(reward, expected)` — sets RPE
- Now fires on prediction-success (Phase A.2, RPE → dopamine)

### Neuromodulators
- Dopamine, serotonin, norepinephrine (and 3 more)
- Modulate plasticity, attention, arousal

## Safety / Ethics Layer

- Ethics module: non-removable, always-on
- LGSS (Layered Governance Safety System): action/input/motor gates
- Mental health monitor: 23-disorder DSM screening
- Tamper-resistant audit log: append-only, CRC-verified

See [50_safety.md](50_safety.md) for full safety architecture.

## Known Gaps (Phase D)

1. **Symbolic consultation during inference** (D.2)
   - Currently `brain.decide_full(features)` runs vector pipeline only
   - KG and semantic memory aren't queried at inference time
   - Planned: symbolic context passed alongside features, blended into decision

2. **Reconstructive episodic recall** (D.3)
   - Current: returns stored vector
   - Target: return gist + semantic fillers (Bartlett 1932 schema theory)

3. **Cross-store integration**
   - The stores exist but bridges between them are thin
   - Symbolic → vector and vector → symbolic paths need explicit design

## See Also

- [10_training_paradigm.md](10_training_paradigm.md)
- [20_snn.md](20_snn.md)
- [50_safety.md](50_safety.md)
