# Plasticity Models Wiring Audit

**Date:** 2025-11-11
**Auditor:** Claude Code

## Summary

**Total Plasticity Models:** 9
- **Fully Wired & Active:** TBD
- **Partially Wired:** TBD
- **NOT Wired:** TBD

---

## Plasticity Model Inventory

### 1. STDP (Spike-Timing-Dependent Plasticity)
**Location:** `plasticity/stdp/nimcp_stdp.h/c`
**Status:** ⏳ CHECKING
**What It Does:** Hebbian learning based on spike timing
**Expected Integration:** Neural network weight updates after processing

### 2. BCM (Bienenstock-Cooper-Munro)
**Location:** `plasticity/bcm/nimcp_bcm.h/c`
**Status:** ⏳ CHECKING
**What It Does:** Homeostatic plasticity, prevents runaway excitation
**Expected Integration:** Weight normalization after STDP

### 3. STP (Short-Term Plasticity)
**Location:** `plasticity/stp/nimcp_stp.h/c`
**Status:** ⏳ CHECKING
**What It Does:** Synaptic depression/facilitation on short timescales
**Expected Integration:** Synapse efficacy modulation during processing

### 4. Eligibility Traces
**Location:** `plasticity/eligibility/nimcp_eligibility_trace.h/c`
**Status:** ⏳ CHECKING
**What It Does:** Credit assignment for delayed rewards
**Expected Integration:** Learning with temporal credit assignment

### 5. Adaptive Plasticity
**Location:** `plasticity/adaptive/nimcp_adaptive.h/c`
**Status:** ⏳ CHECKING
**What It Does:** Meta-learning, learning rate adaptation
**Expected Integration:** Adjust learning rates based on performance

### 6. Neuromodulators (System)
**Location:** `plasticity/neuromodulators/nimcp_neuromodulators.h/c`
**Status:** ✅ WIRED (Verified)
**What It Does:** Dopamine, serotonin, acetylcholine, norepinephrine, GABA, glutamate
**Integration:**
- ✅ Created: `init_neuromodulator_system()` (line 1285)
- ✅ Initialized: `brain_create_custom()` (line 2295)
- ✅ Destroyed: `brain_destroy()` (line 2617)
- ✅ Getter: `brain_get_neuromodulator_system()` (line 563)
- ✅ Synced: `brain_sync_neuromodulators()` (line 5877)

### 7. Pink Noise Neuromodulation
**Location:** `plasticity/neuromodulators/nimcp_neuromod_pink_noise.h/c`
**Status:** ✅ WIRED (Verified)
**What It Does:** 1/f noise for realistic neuromodulator fluctuations
**Integration:**
- ✅ Created: `init_pink_noise_neuromodulator()` (line 1216)
- ✅ Saved: `brain_snapshot()` (line 4638)
- ✅ Loaded: `brain_load()` (line 4975)
- ✅ Getter: `brain_get_pink_noise()` (line 5999)

### 8. Spatial Neuromodulation
**Location:** `plasticity/neuromodulators/nimcp_spatial_neuromod.h/c`
**Status:** ⏳ CHECKING
**What It Does:** Location-dependent neuromodulator effects
**Expected Integration:** Modulate plasticity based on neuron location

### 9. Attention-Based Plasticity
**Location:** `plasticity/attention/nimcp_attention.h/c`
**Status:** ✅ WIRED (Verified - Multihead Attention)
**What It Does:** Attention-weighted learning
**Integration:** Integrated with multihead attention in brain processing

---

## Integration Point Analysis

### Where Plasticity SHOULD Be Applied

**1. Neural Network Forward Pass** (`brain_process_multimodal`)
- After STAGE 3 (neural network processing)
- Apply STP (short-term synaptic dynamics)
- Apply attention gating (if enabled)

**2. After Network Output** (`process_neural_network`)
- STDP: Update weights based on spike timing
- BCM: Normalize weights to prevent runaway
- Eligibility traces: Mark synapses for reward-based learning

**3. Learning Phase** (`brain_learn`, `brain_train`)
- Adaptive plasticity: Adjust learning rates
- Spatial neuromodulation: Location-dependent updates
- Neuromodulator influence: DA/5-HT modulation of learning

**4. Consolidation** (`brain_consolidation`)
- BCM homeostasis
- Eligibility trace decay
- STP recovery

---

## Next Steps

1. ✅ Verify neuromodulator wiring (COMPLETE)
2. ⏳ Check STDP application in neural network
3. ⏳ Check BCM application
4. ⏳ Check STP application
5. ⏳ Wire spatial neuromodulation (if not wired)
6. ⏳ Wire eligibility traces (if not wired)
7. ⏳ Wire adaptive plasticity (if not wired)
8. ⏳ Create comprehensive integration tests
9. ⏳ Create regression tests for plasticity

---

## Action Items

- [ ] Audit STDP wiring in neuralnet.c
- [ ] Audit BCM wiring in neuralnet.c
- [ ] Audit STP wiring in synapse processing
- [ ] Audit adaptive plasticity wiring
- [ ] Audit spatial neuromodulation wiring
- [ ] Audit eligibility trace wiring
- [ ] Create integration tests for all plasticity models
- [ ] Create regression tests
- [ ] Document integration architecture
