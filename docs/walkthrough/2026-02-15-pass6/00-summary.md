# Pass 6 Walkthrough Summary

**Date**: 2026-02-15
**Scope**: Full source tree (`src/`)
**Method**: 20+ parallel review agents covering all directories

## Aggregate Findings

| Section | File | P1 | P2 | P3 |
|---------|------|----|----|-----|
| Core brain (main) | 01a-core-brain-main.md | 5 | 15 | 0 |
| Core brain (subcortical) | 01b-core-brain-subcortical.md | 6 | 22 | 0 |
| Cortical columns + brain oscillations | 02a-cortical-columns.md | 0 | 8 | 0 |
| Brain regions | 02b-brain-regions.md | 21 | ~485 | 0 |
| Cognitive immune + memory | 03-cognitive-immune-memory.md | 3 | ~400 | 3 |
| Cognitive attention | 04a1-attention.md | 8 | 19 | 0 |
| Cognitive curiosity | 04a1-curiosity.md | 0 | 2 | 0 |
| Cognitive emotion + emotional_tagging | 04a2-emotion.md | 2 | 17 | 0 |
| Emotion tensor + consolidation | 04a2-tensor-consolidation.md | 5 | 31 | 0 |
| Executive + sleep_wake | 04b1-executive-sleep.md | 2 | 9 | 0 |
| Reasoning + salience + working_memory | 04b2-reasoning-salience-wm.md | 38 | ~175 | 6 |
| Cognitive parietal + imagination + misc | 05-cognitive-parietal-imagination-misc.md | 4 | ~476 | 6 |
| Cognitive integration + bridges | 06-cognitive-integration-bridges.md | 15 | ~62 | ~6 |
| Mesh coordinator + MSP | 07a1-mesh-coord.md | 6 | 18 | 0 |
| Mesh ordering + channel + topology | 07a1-mesh-ordering.md | 16 | 16 | 0 |
| Mesh bridges | 07a2-mesh-bridges.md | 5 | 45 | 14 |
| Networking protocol + p2p | 07b1-networking-protocol.md | 1 | 37 | 0 |
| Networking NLP + immune | 07b2-networking-nlp.md | 8 | 67 | 0 |
| NLP bridges | 07b3-nlp-bridges.md | 1 | 19 | 0 |
| Middleware + plasticity + dragonfly | 08-middleware-plasticity-dragonfly.md | 7 | ~55 | 1 |
| Utils + security + GPU + glial | 09-utils-security-gpu-glial.md | 7 | 36 | 8 |
| Remaining src dirs | 10-remaining-src.md | 5 | ~80 | 8 |
| **TOTAL** | | **~165+** | **~2094+** | **~52+** |

## Top P1 Categories

### 1. Division by Zero (~90+ instances)
- **Brain regions**: 21 div-by-zero in neuromodulator regions (locus coeruleus, raphe, VTA, cerebellum) - dt, neuron count, weight normalization
- **Reasoning/salience/wm**: 27 unguarded divisions (salience_snn_bridge: 9, salience: 7)
- **Attention**: 5 div-by-zero (dt_ms==0, attention_mod==0, sequence_length==0)
- **Physics**: 3 div-by-zero (num_points==1 pattern)
- **Emotion tensor/consolidation**: wrong return type

### 2. Race Conditions / Thread Safety (~25+ instances)
- **Mesh**: No thread safety in cross_channel.c, kg_routing_bridge; unprotected reads across multiple files
- **Attention**: Static `prev_focus` shared across instances; substrate bridge reads without mutex
- **Cognitive integration**: Static counters without sync in 8 files
- **NLP**: Race on global `g_phoneme_lexicon` lazy init
- **API**: Brain module lazy init without mutex
- **Networking**: `heartbeat_running` flag, router queue stats, `g_bbb_system` globals

### 3. Deadlocks (~4 instances)
- **Mesh brain integration**: `unregister_brain()` self-deadlock (non-recursive mutex)
- **Cognitive immune**: `autobio_immune_bridge_update()` self-deadlock
- **Networking p2p**: `p2p_node_reconnect_unhealthy()` guaranteed deadlock
- **Middleware**: Wrong mutex field in omni_training_bridge

### 4. Use-After-Free / Memory Corruption (~8 instances)
- **Mesh endorsement**: Returns internal pointers after releasing mutex
- **Cognitive memory**: realloc double-pointer bug in collective_memory
- **Cognitive immune**: mutex field mismatch (bridge->mutex vs bridge->base.mutex)
- **Middleware**: UAF in signal_wrapper, memset destroys mutex in flow_tracker
- **Middleware optimizers**: NULL deref after realloc

### 5. Buffer Overflow / Stack Overflow (~5 instances)
- **Attention plasticity**: Unchecked head_idx access
- **NLP dialect**: VLA stack overflow (unbounded float array[dim])
- **NLP semantic**: Unaligned memory access on strict-alignment architectures
- **Physics dynamical**: uint32_t underflow -> massive OOB writes
- **Parietal financial**: Stack-allocated MACD/Bollinger arrays

### 6. Integer Overflow (~3 instances)
- **Parietal mathematical genius**: uint64 overflow in modular pow
- **NLP**: Missing overflow checks on uint32_t multiplications
- **Cognitive integration**: game_theory_executive_bridge

## Top P2 Systemic Patterns

### 1. Wrong Error Codes (~1200+ instances)
- ~500+ `NIMCP_ERROR_NO_MEMORY` used for NULL parameter checks (should be NULL_POINTER)
- ~500+ `NIMCP_ERROR_INVALID_PARAM` used for NULL checks (should be NULL_POINTER)
- ~200+ `NIMCP_ERROR_NULL_POINTER` used after allocation failures (should be NO_MEMORY)

### 2. False Positive NIMCP_THROW_TO_IMMUNE (~400+ instances)
- Search/lookup not-found paths
- Validation rejection paths
- Worker thread normal exit
- Capacity limit reached
- Disabled feature checks

### 3. Wrong Function Names in Throw Messages (~400+ instances)
- 261 "unknown:" function names (imagination, creative, shadow, mirror modules)
- ~50 copy-paste wrong names in NLP compression
- ~55 wrong destroy-function names in non-destroy functions
- ~18 wrong names in middleware routing/detector files

### 4. FEP Bridge Return Convention (~14 instances)
- FEP bridges should return 0/-1, not NIMCP_ERROR_* codes

## Coverage Gaps
- **Plasticity**: Only ~5% reviewed (5/97 files) - needs dedicated pass

## Recommendations

### Immediate (P1 fixes)
1. Add div-by-zero guards to all ~90 identified locations
2. Fix 4 deadlocks (use recursive mutex or _unlocked() helpers)
3. Fix UAF in mesh endorsement, collective_memory realloc, signal_wrapper
4. Add mutex to cross_channel, kg_routing_bridge, phoneme_lexicon init
5. Fix VLA stack overflow in dialect_learning (cap dimension)
6. Fix uint32_t underflow in dynamical_systems

### Automated (P2 bulk fixes)
1. Script: Fix ~1200 wrong error codes (same pattern as Pass 5 bulk fixes)
2. Script: Remove ~400 false positive throws
3. Script: Fix ~400 wrong function names in throw messages
4. Script: Fix FEP bridge return conventions
