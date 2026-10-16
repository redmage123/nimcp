# Cross-Bridge Integration

**Last Updated**: 2026-03-11

## Overview

Cross-bridge integration connects all major NIMCP subsystems via bridge files, thalamic routing, and bio-async messaging. The bridge system has grown significantly since initial implementation, reflecting the project's expanding cognitive architecture.

## Bridge Counts (approximate)

| Category | Count | Location |
|----------|-------|----------|
| Total bridge files (src) | ~1065 | `src/` (all `*bridge*` files) |
| Total bridge headers (include) | ~1184 | `include/` (all `*bridge*` files) |
| SNN cross-modal bridges | 42 | `src/snn/bridges/` |
| Thalamic routing bridges | ~78 | Various `*thalamic*bridge*` files |
| Inter-module bridges | 19 | `include/integration/inter/` |
| Plasticity bridges | ~79 | `src/**/plasticity/*bridge*` |

## SNN Bridges (42 files in `src/snn/bridges/`)

Bridges connecting the Spiking Neural Network layer to cognitive modules:

attention, audio, autobiographical, bcm, buffer, cortical, cross_modal_align, curiosity, emotional_tagging, emotion, empathetic, executive, free_energy, global_workspace, grief, hippocampus, homeostatic, introspection, joy, language, love, medulla, memory, mental_health, meta_learning, population, prefrontal, reasoning, routing, scaling, self_model, shadow, sleep, somatosensory, speech, stdp, thalamic, tom, training, training_integration, visual, wellbeing

## Bridge Boilerplate Macros

Defined in `include/utils/bridge/nimcp_bridge_boilerplate.h`:

| Macro | Purpose |
|-------|---------|
| `BRIDGE_BOILERPLATE(MODULE, CATEGORY)` | Full: health agent + mesh registration + heartbeat |
| `BRIDGE_BOILERPLATE_MESH_ONLY(MODULE, CATEGORY)` | Health agent + mesh registration (no heartbeat) |
| `BRIDGE_BOILERPLATE_MINIMAL(MODULE)` | Health agent only (no mesh, no heartbeat) |

All bridges share a common `bridge_base_t` (from `nimcp_bridge_base.h`) with a per-instance `mutex` field.

## Integration Directions

| Integration | Direction | Effect |
|-------------|-----------|--------|
| Perception -> Cognitive | visual_confidence -> attention_focus |
| Cortical -> Cognitive | burst_rate -> epistemic_uncertainty |
| Perception -> Portia-Swarm | confidence modifier [0.5, 1.5] |
| Cortical -> Portia-Swarm | threshold modifier [0.7, 1.3] |
| SNN -> Language | spike patterns -> word-concept binding (STDP). As of 2026-05-14 (`ac47802b0`) `apply_stdp` is driven from the training tick (`brain_tick_lang_bridge_spike_routing`), not just inference/API/cascade. See [grounded-language.md](grounded-language.md#training-loop-plasticity-wiring-2026-05-14-commit-ac47802b0). |
| SNN -> Creative | imagination-driven production |
| Neuromodulator -> Sensory/Emotion/WM/etc. | 11+ inter-module bridges |

## Bio-Async IDs

- `BIO_MODULE_PERCEPTION_TRAINING = 0x0523`
- `BIO_MODULE_CORTICAL_TRAINING = 0x0524`

## Key Cross-Modal Files

| File | Purpose |
|------|---------|
| `src/snn/bridges/nimcp_snn_cross_modal_align.c` | Cross-modal temporal alignment |
| `src/core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.c` | Audiovisual cross-modal processing |
| `src/information/immune/nimcp_cross_modal_immune_bridge.c` | Cross-modal immune bridge |
| `include/snn/bridges/nimcp_snn_language_bridge.h` | SNN-language bridge (spike-driven word-concept binding) |

## Mesh Adapter Integration

Bridges register as mesh participants via `BRIDGE_BOILERPLATE*` macros, using `MESH_ADAPTER_CATEGORY_*` constants for categorization. The mesh provides discovery and routing between bridge instances.

## GOTCHAs

- FEP bridges return `0`/`-1` (not NIMCP error codes)
- Bridge mutexes are per-instance (Level 4 in lock hierarchy)
- Callbacks must be invoked AFTER releasing bridge mutex to prevent deadlock
- Some bridges use recursive mutexes for re-entrant callback patterns
