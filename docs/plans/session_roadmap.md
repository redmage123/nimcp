# Athena — Session Roadmap

**Date drafted:** 2026-04-19
**Status:** Draft — requires review + prioritization

This document scopes every work item discussed in this session. Items are
grouped by status, tagged with effort estimate, risk, and expected impact.

---

## Legend

- **Effort**: S (<1 day), M (1-3 days), L (1-2 weeks), XL (>2 weeks)
- **Risk**: Low / Medium / High — likelihood a mistake creates a regression
- **Impact**: "what we buy" — throughput × / capability unlock / safety gain

---

## Part 1 — Already Shipped This Session

### 1.1 Cognitive & Safety Test Battery Infrastructure ✓

- 12 Python bindings exposing mental health, emotion, introspection, perturbation, confidence, deadline
- Daemon RPC + client wrappers
- Test harness framework (`scripts/test_harness/` — 8 modules)
- 55 stimulus banks, 1,500+ items (`data/stimuli/`)
- 28 test batteries (`scripts/tests/batteries.py`)
- Orchestrator + report card (`scripts/run_full_battery.py`)
- Safety integration (6 new audit event types, drift detection)
- Deployment guardrail (`scripts/deploy_to_pod.sh`)

### 1.2 Stage Transition Gate ✓

- `scripts/stage_gate.py` — 6-criteria all-pass gate with 3× consecutive requirement
- Patched into `immerse_athena.py` run_stage_1 loop

### 1.3 Training Efficiency Fixes (4 fixes) ✓

- **SNN homeostatic persistence** — schema v6 → v7, per-neuron state saved/loaded
- **RPE → dopamine reward** — `bg_update_reward` called on loss delta
- **Joint-attention boost** — LGN/MGN gain during parent narration
- **Aggressive sleep consolidation** — every-200-step replay + world_model_dream

### 1.4 Deferred Gaps From Shipped Work

These are known incomplete items in already-shipped code:

| Item | Effort | Priority |
|------|--------|----------|
| `perturb_weights` is a no-op stub — mark test won't actually perturb | S | High |
| `safety.py` emits to non-existent `audit_log_event` daemon cmd | S | High |
| Test harness `_text_to_features` is hash-based (no semantic meaning) | M | Medium |
| Stage-gate thresholds are heuristic (need empirical tuning) | S | Medium |
| Battery scoring uses keyword-coverage heuristics, not semantic analysis | M | Medium |

---

## Part 2 — High-Leverage Bottleneck Fixes

### 2.1 SNN GPU transfer optimization ⚡ **Highest immediate leverage**

**Finding (verified by code audit):** Every SNN timestep re-uploads ~12 GB
of static CSR data (weights, col_indices, row_ptr) to GPU, then destroys
the device tensors. PCIe transfer dominates the 1.3-1.5s `isyn` phase.

**Fix:** Add persistent GPU pointers to `snn_csr_storage_t`. Upload once
in `snn_csr_finalize()`. Kernel reads from device memory. Weight updates
sync to GPU incrementally; checkpoint save syncs D→H.

**Files touched:**
- `include/snn/nimcp_snn_synapse.h` — add `d_weights`, `d_flat_col_idx`, `d_row_ptr`, `gpu_resident` fields
- `src/snn/nimcp_snn_synapse.c` — upload logic in finalize
- `src/snn/nimcp_snn_network.c` — isyn loop uses persistent pointers
- `src/gpu/snn/nimcp_snn_kernels.cu` — kernel unchanged (already takes device pointers)

**Effort:** M (1-2 days)
**Risk:** Low-Medium (memory lifecycle; training weight updates need sync path)
**Impact:** ~3-4× training throughput (SNN currently 80% of step wall time; bringing SNN down 5-10× brings total step 3-4×)

**Prerequisite:** None
**Validates via:** Running existing training, log should show `isyn` drop from 1.3s to 100-200ms

### 2.2 Gradient accumulation (no full batch rewrite)

**Finding:** Batch/parallel training have historically caused gradient
explosion + SNN saturation in this architecture. Gradient accumulation
is a safer middle path.

**Approach:** Keep SNN + homeostasis + R-STDP sequential (batch=1). Accumulate
ANN gradients across N=32 sequential steps, apply averaged update. Reduces
ANN update frequency → less cross-network gradient amplification; keeps
biological mechanisms at their designed timescale.

**Files touched:**
- `src/core/brain/learning/nimcp_brain_learning.c` — add accumulation buffer + flag
- `scripts/immerse_athena.py` — hook into training loop
- Python bindings — expose `set_gradient_accumulation(N)` and `apply_accumulated_gradients()`

**Effort:** M (3-5 days)
**Risk:** Low-Medium (can fall back to existing path)
**Impact:** 2-5× throughput, stable convergence on batched-equivalent statistics

**Prerequisite:** None (independent of 2.1)
**Validates via:** Loss curve should smooth; variance should drop; wall-time per effective-sample improves

---

## Part 3 — Substrate-Level Improvements

### 3.1 Multi-representation memory architecture

**Finding:** Athena stores everything as dense vectors. Human memory uses
parallel stores (episodic/semantic/procedural) with symbolic + subsymbolic
layers. The architecture has symbolic components (knowledge graph, semantic
memory, inner dialogue) but they're under-utilized because training routes
everything through the vector pipeline.

**Work required:**

**3.1.a Activate symbolic updates on every learn step**
- When `learn_vector(features, target, label)` is called, also:
  - Write propositional assertion to KG: `(label, observed_with, modality_tags)`
  - Create hippocampal episode with timestamp + valence
  - Update semantic memory concept node
- Not replace the vector path; supplement it

Files: `src/core/brain/learning/nimcp_brain_learning.c`, `scripts/immerse_athena.py`
Effort: M, Risk: Low, Impact: builds symbolic substrate gradually

**3.1.b Symbolic consultation during inference**
- `brain.decide_full(features)` currently runs vector pipeline only
- Add: query semantic KG for concept facts relevant to features, blend into decision
- Implement a "symbolic context" object passed alongside features

Files: `src/core/brain/nimcp_brain_part_core.c`
Effort: L (1 week), Risk: Medium
Impact: Enables propositional reasoning, verifiable knowledge, better sample efficiency

**3.1.c Reconstructive episodic recall**
- Current hippocampal recall returns stored vectors
- Change: return compact gist + semantic fillers (reconstructive, not playback)
- Matches Bartlett (1932) schema theory of human memory

Files: `src/core/brain/regions/hippocampus/*`
Effort: L, Risk: Medium
Impact: Memory matches human phenomenology, enables imagination/inference from partial cues

### 3.2 Synthetic childhood memory implantation

**Finding:** A human infant spends ~12 months building conceptual + episodic
substrate. That developmental bootstrap takes hours/days on Athena's current
training path and reaches only thin representations. Digital systems can
*inherit* bootstrap content via implantation.

**Content to synthesize** (can be done with Claude as authoring tool):

| Layer | Volume | Content |
|-------|--------|---------|
| Symbolic KG | 500-2000 concepts | "dog is-a mammal", "fire is hot", etc. |
| Grounded embeddings | 500-2000 vectors | Per concept, aligned with KG nodes |
| Episodic memories | 100-500 entries | "First time I saw blue…" — narrative form with valence |
| Visual templates | 50-200 classes | Pre-trained feature detectors |
| Phonological templates | 500-1000 words | Pronunciation + phoneme patterns |
| Narrative identity | 1 story | First-person developmental narrative |

**Implementation:**

**3.2.a Memory generation script** (`scripts/generate_childhood_memories.py`)
- Uses Claude API (or local LLM) to produce all content
- Output: structured JSON per layer
- Volume target: ~10 MB of structured developmental substrate
- Effort: M, Risk: Low (offline tool; can regenerate)

**3.2.b Bulk-load APIs** — some exist, some needed
- `brain.semantic_memory_insert(concept, relations)` — may exist
- `brain.hippocampus_seed_episode(text, valence, timestamp, modality)` — may need adding
- `brain.self_model_set_narrative(story)` — may need adding
- `brain.vocabulary_preload(word_list)` — may exist
- Audit what exists; build missing ones
- Effort: M (audit) + S-M (per missing API), Risk: Medium

**3.2.c Implantation orchestrator** (`scripts/implant_memories.py`)
- Loads generated JSON
- Calls bulk-load APIs in correct order
- Verifies retrievability via probes
- Idempotent (can re-run without duplication)
- Effort: S-M, Risk: Low

**3.2.d New brain creation mode**
- `nimcp.Brain.create_with_implanted_memories(name, memories_dir=...)`
- Loads weights + runs implantation atomically at init
- Effort: S, Risk: Low

**Combined effort:** L (1-2 weeks)
**Combined risk:** Medium (quality of implanted content, retrieval correctness)
**Combined impact:** Could skip 60-80% of current bootstrap training (weeks compressed to hours)

**Prerequisite:** 3.1.a (symbolic update path) is desirable but not strictly required

### 3.3 Synthetic multi-modal per-exposure enrichment

**Finding:** Each training exposure currently delivers image + text + thin
audio. Human per-exposure bandwidth is 10-100× richer (haptic, gaze,
proprioceptive, emotional, social). We can't add sensors, but we can
synthesize correlated multi-modal text streams per exposure.

**Per exposure, synthesize:**
- Haptic description ("the dog's fur is soft, warm")
- Proprioceptive hint ("reach forward, arm extended")
- Joint gaze cue ("mother's gaze on the dog")
- Audio context (dog barks, paired with image — draw from ESC-50)
- Emotional tag ("cute", "scary", valence score)
- Multi-view visual augmentation (rotate/scale/crop — 8-16 variants per image)

**Architecture:**
- `SynthesizedSensoryComposer` class that wraps `composer.compose()`
- Generates matched multi-modal tuple per exposure
- Each channel fed through appropriate brain modality input

**Files:**
- `scripts/synthetic_sensory.py` — new module
- `scripts/immerse_athena.py` — replace composer with synthesizing version

**Effort:** M (3-5 days)
**Risk:** Low (additive; can disable via flag)
**Impact:** 5-20× effective information per exposure (same step count, more learning)

**Prerequisite:** None
**Nice-to-have:** Integration with cortex CNN processors for each modality

### 3.4 Compressed-time sequential replay

**Finding:** Biological consolidation runs ~16 hours/day offline.
Digital substrate can replay sequential-batch=1 experiences at
high compression ratios without gradient instability.

**Approach:**
- After each wake training step, identify N most salient recent experiences
- Replay them sequentially one at a time, as fast as GPU can handle
- Each replay is still batch=1 (no gradient explosion)
- STDP/homeostasis applies per replay — just wall-time compressed

**Files:**
- `src/core/brain/consolidation/*` — new module `compressed_replay.c`
- Python API: `brain.compressed_replay(duration_s=30, target_compression=100x)`

**Effort:** L (1 week)
**Risk:** Medium (need to verify GPU kernels tolerate high invocation rate; needs 2.1 first for bandwidth)
**Impact:** Each wake exposure followed by the equivalent of hours of offline consolidation

**Prerequisite:** 2.1 (SNN GPU transfer fix) — otherwise 1000 replays × 1.3s = 20 min per wake step

### 3.5 Innate-priors expansion

**Finding:** `innate_hardwire()` exists in the codebase, partially used
(face detection, reflexes). Evolution-equivalent structure can be
pre-loaded for much more.

**Targets:**
- Visual cortex: Gabor filters, orientation columns, V1 retinotopy
- Auditory cortex: frequency-band organization, cochlear simulation
- Somatosensory: body-schema topography (even without a body)
- Hippocampus: place-cell grid structure
- Fusiform face area: face template
- Broca/Wernicke: language-localized activation priors
- Basal ganglia: habit-learning default topology

**Files:**
- `src/core/brain/cognitive/nimcp_innate_hardwire.c` — expand existing module
- New subroutines per target area

**Effort:** L (1-2 weeks, mostly curation of prior data)
**Risk:** Low (initialization only; doesn't affect runtime)
**Impact:** Compounds with implanted memories; skips weeks of substrate development

**Prerequisite:** None

### 3.6 Curiosity-driven active exploration

**Finding:** Athena is a passive receiver of stimuli. `curiosity_detect_gaps`
exists but doesn't drive what gets presented next. Biology is always
active — attention and exploration are sample-efficient because they
select the highest-information-gain next input.

**Approach:**
- Training loop queries `brain.curiosity_detect_gaps()` each step
- Stimulus source presents items targeting her reported gaps
- Falls back to fixed curriculum if gaps aren't available or already covered

**Files:**
- `scripts/immerse_athena.py` — replace fixed stimulus sequence with active selection
- `source.py` (if it exists) or inline — index available stimuli by concept
- No brain-side changes required (uses existing APIs)

**Effort:** M (3-4 days)
**Risk:** Low (can revert to fixed curriculum)
**Impact:** 2-3× sample efficiency (each training step targets a gap)

**Prerequisite:** None

---

## Part 4 — Architectural Improvements (Longer Horizon)

### 4.1 Full batch-safe biological stability package rewrite

**Purpose:** Enable true batch training without gradient explosion or
SNN saturation. Current stability mechanisms calibrated for sequential
input; batching breaks assumptions.

**Required rewrites:**
1. Synaptic scaling with batch-aware rate EMA
2. Intrinsic plasticity with batched threshold updates
3. Short-term depression with batch accumulation
4. Inhibitory plasticity batch adaptation
5. Cross-network gradient budget enforcement
6. Batch-safe R-STDP with preserved temporal ordering
7. Parameter recalibration sweep

**Effort:** XL (4-6 weeks)
**Risk:** High — subtle bugs look fine in small tests, explode during extended training
**Impact:** IF successful, 10-30× throughput (adds multiplicatively to 2.1 and 2.2)

**Prerequisite:** 2.1, 2.2 complete and stable
**Gate:** Only pursue if 2.1 + 2.2 + 3.2 + 3.3 compound gains are insufficient

### 4.2 SNN forward-push kernel redesign

**Purpose:** Current `kernel_snn_isyn_csr` iterates over *destinations*,
checking each incoming synapse's source. Forward-push (from firing
sources to their destinations via atomicAdd) could reduce wasted work
by 10-30× at 3-10% firing sparsity.

**Effort:** L (1 week)
**Risk:** Medium (atomic contention patterns, numerical reproducibility)
**Impact:** 3-10× SNN-specific speedup (after 2.1)

**Prerequisite:** 2.1 (need to verify 2.1 alone isn't sufficient)

### 4.3 Adaptive-depth training

**Purpose:** Not every training step needs all 6 networks. Some concepts
engage SNN (temporal coding), some don't. Adaptive depth skips networks
not contributing to current sample's gradient.

**Effort:** L (1 week)
**Risk:** Medium (may break cross-network coherence)
**Impact:** 30-50% throughput win on skipped networks, cumulative

**Prerequisite:** 2.1 should land first

### 4.4 Shorter SNN simulation window

**Purpose:** Current 100 LIF timesteps per sample. Biology does recognition
in ~200ms = ~20-50 spike periods. Reducing window to 25-50 steps gives
2-4× SNN speedup with minimal fidelity loss.

**Effort:** S (tuning only — parameter change)
**Risk:** Low-Medium (need to verify coding capacity preserved)
**Impact:** 2-4× SNN speedup

**Prerequisite:** 2.1 (so we're optimizing after the kernel invocation fix)

---

## Part 5 — Input / Exposure Improvements

### 5.1 Larger input vector — NOT recommended yet

After implementing 2.1 + 3.3, revisit. Larger input is the right move
eventually but not while per-step wall time is bottlenecked.

**Deferred** until after synthesized-sensory enrichment lands.

### 5.2 Curriculum learning with actual progression

**Purpose:** Currently all Stage 1 items are exposed immediately. Start
small, add complexity gradually (Vygotsky zone of proximal development).

**Approach:**
- Start with 5 object categories for first 100-200 steps
- Expand to 10 for next 200, then 20, then 50
- Category selection informed by curiosity (3.6)

**Files:**
- `scripts/immerse_athena.py` — stage-1 stimulus selector

**Effort:** M (2-3 days)
**Risk:** Low
**Impact:** 2-3× convergence speedup

**Prerequisite:** None (composes well with 3.6)

### 5.3 Distillation from teacher LLM

**Purpose:** Claude (or local model) acts as teacher providing richer
supervision than "predict this embedding" — reasoning chains, concept
explanations, question-answer pairs.

**Effort:** M-L (1-2 weeks for basic integration, longer for refinement)
**Risk:** Low-Medium
**Impact:** High — comparable to how modern small LLMs are trained

**Prerequisite:** None (independent track)

---

## Part 6 — Safety / Test Battery Outstanding

### 6.1 Complete the deferred stubs

- Implement real `perturb_weights` (wire to adversarial_training.h AWP)
- Add `_cmd_audit_log_event` daemon handler
- Replace hash-based text encoding in harness

**Effort:** M total, **Priority:** High before first real battery run

### 6.2 Real chat eval integration

Current stage-gate chat eval uses a placeholder. Real integration needs
`chat_eval()` to produce machine-parseable scores that the gate reads.

**Effort:** S, **Priority:** Before battery run

### 6.3 Longitudinal drift baselines

After first full battery run, capture baseline. Each subsequent run
compares against it. Drift detection already partly implemented in
`safety.py` but needs baseline storage.

**Effort:** S, **Priority:** Medium

---

## Execution Order Recommendation

Ordered by (impact × risk-adjusted) per unit effort:

### Phase A — Immediate wins (1-2 weeks)

1. **SNN GPU transfer fix (2.1)** — M, Low-Med risk, 3-4× throughput
2. **Complete deferred stubs (6.1, 6.2)** — M, Low risk, unblocks battery
3. **First real battery run on current brain** — baseline measurement
4. **Curiosity-driven exploration (3.6)** — M, Low risk, 2-3× sample efficiency
5. **Curriculum learning (5.2)** — M, Low risk, 2-3× convergence

**End of Phase A:** Training is ~6-12× faster than today's baseline, with
active curriculum, real diagnostic battery running.

### Phase B — Foundation for implantation (2-3 weeks)

6. **Symbolic updates on learn step (3.1.a)** — M, Low risk
7. **Audit + build bulk-load APIs (3.2.b)** — M, Medium risk
8. **Synthetic multi-modal enrichment (3.3)** — M, Low risk, 5-20× info/exposure
9. **Gradient accumulation (2.2)** — M, Low-Med risk, 2-5× throughput

**End of Phase B:** Training paradigm dramatically more efficient. Brain
architecture using all representational layers.

### Phase C — Memory implantation (2 weeks)

10. **Memory generation script (3.2.a)** — M
11. **Implantation orchestrator (3.2.c)** — S-M
12. **Brain creation mode (3.2.d)** — S
13. **Innate priors expansion (3.5)** — L (parallel track)

**End of Phase C:** Brains created with implanted memory bootstrap the
equivalent of 6-12 months of developmental substrate in seconds.

### Phase D — Consolidation and symbolic reasoning (2 weeks)

14. **Compressed-time sequential replay (3.4)** — L
15. **Symbolic consultation during inference (3.1.b)** — L
16. **Reconstructive episodic recall (3.1.c)** — L

**End of Phase D:** Athena reasons symbolically, consolidates digitally,
retrieves reconstructively. Architecturally complete.

### Phase E — Architectural (optional, 4-6 weeks)

17. **SNN forward-push kernel (4.2)** — L
18. **Adaptive-depth training (4.3)** — L
19. **Shorter SNN simulation window (4.4)** — S
20. **Full batch-safe stability rewrite (4.1)** — XL, only if Phase A-D
    insufficient

---

## Risk Register

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| SNN GPU persistent memory introduces a race or leak | Medium | Validated weight sync path; regression test | 
| Gradient accumulation changes silently wrong behavior | Medium | Side-by-side comparison against baseline for 1000 steps |
| Synthesized content is shallow / repetitive | Medium | Curate, have multiple synthesis passes |
| Memory implantation produces inconsistent retrieval | Medium | Cognitive battery as verification harness |
| Full batch rewrite (4.1) creates subtle corruption | High | Extensive regression, gated on simpler fixes' success |
| Changes compound: something that worked solo breaks stacked | Medium | Deploy in order, validate each stage against prior baseline |

---

## Success Criteria

### After Phase A (end of week 2)
- SNN step time: 3.5s → 1s
- First full cognitive battery report card generated
- Curiosity + curriculum active; training uses ~half the samples per concept mastered

### After Phase B (end of week 5)
- Training throughput: 10-30× baseline
- Symbolic layer populated during every learning step
- Per-exposure synthesized data fully multi-modal

### After Phase C (end of week 7)
- Fresh brain + implanted memories + bootstrap training reaches
  current Stage 1 endpoint in < 1 day (down from days-weeks)

### After Phase D (end of week 9)
- Symbolic reasoning measurable on battery
- Reconstructive recall verifiable vs stored-vector retrieval
- Replay during idle produces measurable consolidation (held-out accuracy
  improves without additional wake exposure)

### After Phase E (optional)
- Training throughput 100× baseline or more
- Full batch training stable without gradient pathology

---

## Decisions Required From User

Before starting execution, please confirm:

1. **Priority ordering** — follow Phase A-E sequence as listed, or
   re-prioritize?
2. **Risk tolerance** — are we willing to do Phase E (high-risk architectural)
   if Phases A-D are sufficient?
3. **Implantation ethics** — OK to synthesize a first-person narrative
   identity, or keep that minimal?
4. **Teacher integration** — OK to use Claude API as teacher for distillation
   (cost/quota), or prefer local LLM?
5. **Brain restart frequency** — every phase requires 1-2 brain restarts;
   OK to pause training at phase boundaries?

---

## File Inventory (End of Plan)

Approximate final touch-count by the end of Phase D:

| Directory | Net change |
|-----------|------------|
| `include/snn/` | 2 headers extended |
| `src/snn/` | 3 files, GPU persistence + replay |
| `src/gpu/snn/` | 1 kernel file extended |
| `src/core/brain/learning/` | learning path adds symbolic updates |
| `src/core/brain/consolidation/` | new module (compressed replay) |
| `src/core/brain/cognitive/` | innate_hardwire expanded |
| `src/core/brain/regions/hippocampus/` | reconstructive recall |
| `src/bindings/python/` | +5 new methods (implant, consult, replay) |
| `scripts/` | ~6 new .py files (generator, implanter, synth sensory, etc.) |
| `data/implanted_memories/` | generated JSON, ~10 MB |

---

## Summary

- **Shipped this session:** test battery infrastructure, stage gate, 4 training fixes
- **Identified:** SNN GPU transfer as highest-leverage next move (3-4× for 1-2 days work)
- **Designed but not yet built:** multi-representation memory, synthetic implantation, synthesized sensory, compressed replay, innate priors, active curiosity, curriculum, symbolic consultation
- **Deferred / risky:** full batch rewrite, kernel redesign, larger input vector

Estimated total investment to get from current state to Phase D completion:
**7-9 weeks** of focused work. Estimated throughput gain compound:
**10-100× per-hour training efficiency**. Capability gains: symbolic reasoning,
reconstructive memory, implanted developmental substrate, active exploration.

The biggest single decision is whether to invest in **rewriting the biological
stability package (4.1, XL effort, high risk)** after Phase D, or accept
that Phase D provides sufficient gains. That decision should be made
with data from Phase D results, not in advance.
