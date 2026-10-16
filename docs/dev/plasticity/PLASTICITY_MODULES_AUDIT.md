# Plasticity Modules Wiring Audit

**Date:** 2025-11-11
**Auditor:** Claude Code

## Summary

**Total Plasticity Modules:** 10
**Fully Wired & Active:** 3 ✅
**Partially Wired:** 2 ⚠️
**NOT Wired:** 5 ❌

---

## Module-by-Module Status

### ✅ FULLY WIRED & ACTIVE

#### 1. **STP (Short-Term Plasticity)**
**Location:** `plasticity/stp/nimcp_stp.h/c`
**Status:** ✅ ACTIVE

**Wiring:**
- ✅ Included: `neuralnet.c` line 45
- ✅ Integrated: Synapse-level STP state
- ✅ Called: Every synaptic transmission

**Usage:**
```c
// neuralnet.c:870-880
if (incoming_syn->enable_stp) {
    stp_update(&incoming_syn->stp, network->network_time);
    stp_modulation = stp_get_modulation(&incoming_syn->stp);
    if (pre_activity > 0.0f) {
        stp_process_spike(&incoming_syn->stp, network->network_time);
    }
}
synaptic_transmission *= stp_modulation;
```

**Evidence:** STP modulates EVERY synaptic transmission when enabled.

---

#### 2. **STDP (Spike-Timing-Dependent Plasticity)**
**Location:** `plasticity/stdp/nimcp_stdp.h`
**Status:** ✅ ACTIVE (via neuralnet.c internal implementation)

**Wiring:**
- ⚠️ NOT directly included (uses internal implementation)
- ✅ Integrated: Neuron-level STDP params
- ✅ Called: After every spike

**Usage:**
```c
// neuralnet.c:1043-1045
if (neuron->learning_rule & LEARNING_STDP) {
    neural_network_apply_stdp(network, neuron_id, timestamp);
}

// neuralnet.c:1312-1400 - Full STDP implementation
uint32_t neural_network_apply_stdp(network, neuron_id, timestamp) {
    int64_t dt = t_post - t_pre;
    if (|dt| > stdp_window) continue;
    float delta_w = compute_stdp_update(dt, &stdp_params);
    synapse->weight += delta_w;
}
```

**Evidence:** STDP updates weights after every spike when enabled.

---

#### 3. **Attention (Multihead Attention)**
**Location:** `plasticity/attention/nimcp_attention.h/c`
**Status:** ✅ ACTIVE

**Wiring:**
- ✅ Included: `brain.c` line 61
- ✅ Field: `brain->multihead_attention`
- ✅ Initialized: `init_attention_subsystem()` (brain.c:1405)
- ✅ Used: Multimodal processing, NLP integration

**Usage:**
```c
// brain.c:6179
if (brain->multihead_attention) {
    multihead_attention_forward(
        brain->multihead_attention,
        query, key, value,
        query_dim, num_heads,
        attended_output
    );
}
```

**Evidence:** Used for selective feature processing in multimodal and NLP systems.

---

### ⚠️ PARTIALLY WIRED

#### 4. **Adaptive Network**
**Location:** `plasticity/adaptive/nimcp_adaptive.h/c`
**Status:** ⚠️ WRAPPER ONLY (not true plasticity)

**Wiring:**
- ✅ Included: `brain.c` line 35
- ✅ Used: 46 references in brain.c
- ⚠️ **Not plasticity:** This is a network wrapper, not a learning rule

**Reality Check:**
`adaptive_network_t` is the main network container, not a plasticity mechanism. It wraps `neural_network_t` with additional features (pruning, growth). Should NOT be counted as a plasticity module.

---

#### 5. **Pink Noise Neuromodulation**
**Location:** `plasticity/neuromodulators/nimcp_neuromod_pink_noise.h/c`
**Status:** ⚠️ INFRASTRUCTURE EXISTS, MINIMAL USE

**Wiring:**
- ✅ Included: `brain.c` line 59
- ✅ Field: `brain->pink_noise` (likely)
- ✅ Initialized: `init_pink_noise_subsystem()` (brain.c:1282)
- ⚠️ Minimal usage: Only 2 direct references

**Issue:** Created but not actively modulating learning. Exists as subsystem but unclear if it affects cognitive processing.

---

### ❌ NOT WIRED TO BRAIN

#### 6. **BCM (Bienenstock-Cooper-Munro) Plasticity**
**Location:** `plasticity/bcm/nimcp_bcm.h/c`
**Status:** ❌ NOT WIRED

**Evidence:**
- ❌ NOT included in brain.c
- ❌ NOT included in neuralnet.c
- ❌ Zero references in CPU code
- ✅ Only exists in GPU code (`gpu/neuron/nimcp_gpu_kernels.cu:308-360`)

**Config:** `brain_config_t` has `enable_bcm` flag, but CPU code ignores it.

**Conclusion:** BCM exists ONLY for GPU acceleration, not used by cognitive modules on CPU.

---

#### 7. **Eligibility Traces**
**Location:** `plasticity/eligibility/nimcp_eligibility_trace.h/c`
**Status:** ❌ NOT WIRED

**Evidence:**
- ❌ NOT included in brain.c
- ❌ NOT included in neuralnet.c
- ❌ Zero references anywhere

**Config:** `brain_config_t` has `enable_eligibility_traces` flag (unused).

**Conclusion:** Implemented but never integrated. Dead code.

---

#### 8. **Neuromodulators (Core System)**
**Location:** `plasticity/neuromodulators/nimcp_neuromodulators.h/c`
**Status:** ❌ NOT ACTIVELY USED

**Evidence:**
- ✅ Included: `brain.c` line 60
- ✅ Field: `brain->neuromodulator_system`
- ✅ Initialized: `init_neuromodulator_system()` (brain.c:1277)
- ❌ BUT: No function calls to modulate learning

**Issue:** System exists but doesn't modulate plasticity. Created as infrastructure, not actively adjusting learning rates or thresholds based on dopamine/serotonin/etc.

---

#### 9. **Spatial Neuromodulation**
**Location:** `plasticity/neuromodulators/nimcp_spatial_neuromod.h/c`
**Status:** ❌ NOT WIRED (Part A Enhancement)

**Evidence:**
- ❌ NOT included in brain.c
- ❌ NOT included in neuralnet.c
- ❌ Zero references

**Note:** This was Part A3.2 of mathematical enhancements. Implementation exists but never wired to brain.

---

#### 10. **Pink Noise (Generator)**
**Location:** `plasticity/noise/nimcp_pink_noise.h/c`
**Status:** ❌ NOT DIRECTLY WIRED

**Evidence:**
- ❌ NOT included in brain.c
- ⚠️ Used indirectly via `neuromod_pink_noise` wrapper
- 7 references (all via wrapper)

**Reality:** This is a utility (pink noise generator), consumed by `neuromod_pink_noise`. Not a standalone module.

---

## Cognitive Module Usage Analysis

### Which Plasticity Mechanisms Are Used By Cognitive Modules?

**Curiosity:**
- ❌ No direct plasticity mechanism
- Uses learning via network weight updates (STDP)

**Knowledge:**
- ❌ No direct plasticity mechanism
- Relies on STDP for concept reinforcement

**Ethics:**
- ❌ No direct plasticity mechanism
- No adaptive moral learning

**Attention:**
- ✅ Uses multihead attention (attention module)
- Selective feature weighting

**Executive Function:**
- ❌ No direct plasticity mechanism

**Mirror Neurons:**
- ❌ No direct plasticity mechanism

**Working Memory:**
- ❌ No direct plasticity mechanism (just temporal decay)

---

## Key Findings

### ✅ What IS Being Used:
1. **STP** - Active in every synaptic transmission
2. **STDP** - Active for spike-based learning
3. **Attention** - Active for multimodal and NLP

### ❌ What is NOT Being Used:
1. **BCM** - GPU-only, not in CPU cognitive path
2. **Eligibility Traces** - Dead code, zero integration
3. **Spatial Neuromodulation** - Part A enhancement, not wired
4. **Neuromodulator System** - Created but not modulating
5. **Pink Noise Modulation** - Created but minimal effect

---

## Recommendations

### Critical Gaps:

1. **Neuromodulator system is dormant**
   - Dopamine should boost learning for rewarding experiences
   - Serotonin should gate plasticity
   - ACh should increase attention and encoding

2. **BCM not available on CPU**
   - Sliding threshold plasticity useful for stability
   - Currently GPU-only

3. **Eligibility traces unused**
   - Temporal credit assignment would improve learning
   - Implemented but never connected

4. **Spatial neuromodulation unused**
   - Part A3.2 exists but not integrated
   - Could enable location-based learning modulation

---

## Action Items

**High Priority:**
1. Wire neuromodulator_system to actually modulate learning rates
2. Connect dopamine → STDP learning rate scaling
3. Connect ACh → attention gating

**Medium Priority:**
4. Wire BCM to CPU cognitive path (or remove if GPU-only)
5. Integrate eligibility traces for temporal credit
6. Connect spatial_neuromod to brain regions

**Low Priority:**
7. Clarify pink_noise usage (is it actually affecting anything?)
8. Document which modules are utilities vs plasticity mechanisms

---

## Conclusion

**Only 3 out of 10 plasticity modules are actively used by cognitive processing:**
- STP (synaptic dynamics)
- STDP (spike-based learning)
- Attention (feature selection)

**The neuromodulator system exists but is effectively dormant** - it's initialized but doesn't modulate anything. This is the biggest gap: dopamine/serotonin/ACh should be dynamically adjusting learning, attention, and memory formation but currently do nothing.
