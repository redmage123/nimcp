# NIMCP Phase 5: Operational Integration
**Version**: 2.7.0 Phase 5
**Date**: 2025-11-08
**Status**: PLANNING → IMPLEMENTATION
**Priority**: 🔴 CRITICAL - System currently idle

---

## Executive Summary

**PROBLEM IDENTIFIED**: Integration test reveals system is "integrated on paper, idle in practice"
- All subsystems initialize ✅
- Zero spike activity ❌
- No learning occurring ❌
- Pipeline dormant ❌

**PHASE 5 GOAL**: Make the engine run - activate data flow through complete pipeline

**DELIVERABLE**: Working end-to-end system demonstrating emergent learning

---

## Critical Issues from Integration Test

### Issue 1: Zero Spike Activity 🔴 URGENT
```
Pattern A → 0 spikes
Pattern B → 0 spikes
Pattern C → 0 spikes
```

**Root Cause**: `spike_nlp_embed_to_spikes()` not generating spikes
- Embeddings too weak OR
- Neuron thresholds too high OR
- Rate coding scale incorrect

**Impact**: Entire learning pipeline idle without spikes

---

### Issue 2: No Unified Brain Interface 🔴 HIGH
**Current State**: User must manually call:
```c
spike_nlp_embed_to_spikes(...);
neural_network_compute_step(...);
stdp_apply(...);
neuromod_pink_update(...);
eligibility_trace_update(...);
```

**Problem**: Easy to miss steps, inconsistent integration, no guarantees

**Needed**: Single `brain_update()` call that coordinates everything

---

### Issue 3: No Output Decoding 🟡 MEDIUM
**Current**: Can inject inputs (embeddings → spikes) ✅
**Missing**: Cannot extract outputs (spikes → predictions) ❌

**Impact**: System learns but cannot communicate results

---

## Phase 5 Features

### Feature 1: Fix Spike Encoding (URGENT)
**File**: `src/nlp/nimcp_spike_nlp.c`

**Diagnosis Plan**:
1. Add debug logging to `spike_nlp_embed_to_spikes()`
2. Check neuron state accumulation
3. Verify threshold values
4. Test with known-good embeddings

**Fix Options**:
```c
// Option A: Lower threshold
if (state > 0.5f) spike();  // Was 1.0f?

// Option B: Scale embeddings
float rate = embedding[i] * SCALE_FACTOR;

// Option C: Direct spike injection
neuron_force_spike(neuron, timestamp);
```

**Success Criteria**: Generate 10+ spikes per pattern in integration test

---

### Feature 2: Unified Brain API (HIGH PRIORITY)
**Files**:
- `include/core/brain/nimcp_brain.h` (NEW)
- `src/core/brain/nimcp_brain.c` (NEW)

**API Design**:
```c
/**
 * @brief Unified brain interface coordinating all subsystems
 */
typedef struct brain_struct* brain_t;

typedef struct {
    // Network topology
    uint32_t num_neurons;
    float scale_free_exponent;  // -2.1 for scale-free

    // Learning parameters
    bool enable_stdp;
    bool enable_eligibility_traces;
    bool enable_neuromodulation;
    bool enable_attention;

    // I/O dimensions
    uint32_t input_dim;
    uint32_t output_dim;
} brain_config_t;

// Lifecycle
brain_t brain_create(const brain_config_t* config);
void brain_destroy(brain_t brain);

// Unified update loop
void brain_update(
    brain_t brain,
    const float* input_embedding,    // Input pattern
    uint32_t input_dim,
    float reward,                     // Reward signal (0 if none)
    uint64_t timestamp,               // Current time in ms
    float* output_prediction          // Output buffer (optional)
);

// Accessors
void brain_get_neuromodulators(brain_t brain,
    float* dopamine, float* serotonin,
    float* acetylcholine, float* norepinephrine);
void brain_get_stdp_stats(brain_t brain,
    uint64_t* ltp_events, uint64_t* ltd_events,
    float* avg_weight_change);
uint32_t brain_get_spike_count(brain_t brain);
```

**Implementation Algorithm**:
```c
void brain_update(brain_t brain, ...) {
    // STEP 1: Encode input → spikes
    uint32_t spikes = spike_nlp_embed_to_spikes(
        input_embedding, input_dim,
        brain->network, 0, brain->input_neurons,
        timestamp
    );

    // STEP 2: Propagate through network
    neural_network_compute_step(brain->network, timestamp);

    // STEP 3: Apply plasticity rules
    if (brain->enable_stdp) {
        // STDP applied automatically on spike events
    }

    // STEP 4: Update neuromodulators
    if (brain->enable_neuromodulation) {
        neuromod_pink_update(brain->neuromod,
            reward, 0.0f, 0.0f, 0.0f);
    }

    // STEP 5: Update eligibility traces
    if (brain->enable_eligibility_traces) {
        float dopamine = neuromod_pink_get_dopamine(brain->neuromod);
        // Update traces for all active synapses
        eligibility_update_all(brain->traces, reward, dopamine, timestamp);
    }

    // STEP 6: Decode output (if requested)
    if (output_prediction) {
        spike_nlp_decode_output(
            brain->network,
            brain->output_neurons, brain->output_dim,
            output_prediction
        );
    }
}
```

**Success Criteria**:
- Single function call replaces 6+ manual calls
- Integration test uses brain_update() exclusively
- All subsystems coordinated automatically

---

### Feature 3: Output Decoding (MEDIUM PRIORITY)
**File**: `src/nlp/nimcp_spike_nlp.c` (extend)

**New Function**:
```c
/**
 * @brief Decode spike activity to output vector
 *
 * WHAT: Convert neuron spike rates to continuous output representation
 * WHY: Close the loop - enable predictions and decision-making
 * HOW: Measure spike rates over window, normalize to [0,1]
 *
 * ALGORITHM:
 * 1. For each output neuron:
 *    a. Count spikes in recent time window (e.g., last 50ms)
 *    b. Compute firing rate: spikes / window_duration
 *    c. Normalize to [0, 1] range
 * 2. Write to output vector
 *
 * @param network Neural network
 * @param output_neuron_start First output neuron ID
 * @param output_dim Number of output dimensions
 * @param window_ms Time window for rate estimation (default: 50ms)
 * @param output Output vector [output_dim]
 */
void spike_nlp_decode_output(
    neural_network_t network,
    uint32_t output_neuron_start,
    uint32_t output_dim,
    uint32_t window_ms,
    float* output
);
```

**Implementation**:
```c
void spike_nlp_decode_output(...) {
    uint64_t current_time = neural_network_get_time(network);
    uint64_t window_start = current_time - window_ms;

    for (uint32_t i = 0; i < output_dim; i++) {
        neuron_t* neuron = neural_network_get_neuron(
            network, output_neuron_start + i
        );

        // Count spikes in window
        uint32_t spike_count = neuron_count_spikes_in_window(
            neuron, window_start, current_time
        );

        // Convert to rate (Hz)
        float rate = (float)spike_count / ((float)window_ms / 1000.0f);

        // Normalize to [0, 1] (assume max rate ~100 Hz)
        output[i] = fminf(rate / 100.0f, 1.0f);
    }
}
```

**Success Criteria**:
- Can extract continuous outputs from spike trains
- Outputs change based on learning
- Enable classification/prediction tasks

---

### Feature 4: Working End-to-End Demo (HIGH PRIORITY)
**File**: `examples/phase5_learning_demo.c`

**Demo Task**: XOR-like pattern classification
```
Input Pattern A (0,0) → Output: 0.0 (low activity)
Input Pattern B (0,1) → Output: 1.0 (high activity)
Input Pattern C (1,0) → Output: 1.0 (high activity)
Input Pattern D (1,1) → Output: 0.0 (low activity)
```

**Training Protocol**:
1. Present pattern → wait 100ms
2. Decode output prediction
3. Compute error
4. Apply reward (+1 if correct, -1 if wrong)
5. Repeat 1000 trials

**Success Metrics**:
- Accuracy > 75% after 1000 trials
- STDP events > 0
- Eligibility traces > 0
- Dopamine modulation visible
- **Proves emergent learning**

---

## Implementation Plan

### Week 1: Diagnosis & Spike Fix
**Days 1-2**: Diagnose spike encoding issue
- Add debug logging
- Test with various embeddings
- Identify root cause

**Days 3-4**: Fix spike generation
- Implement solution
- Verify 10+ spikes per pattern
- Update integration test

**Day 5**: Verify fix in integration test

---

### Week 2: Unified Brain API
**Days 1-2**: Implement brain.h/brain.c
- Create brain structure
- Implement brain_create/destroy
- Wire up all subsystems

**Days 3-4**: Implement brain_update()
- Unified update loop
- Coordinate all plasticity
- Add statistics tracking

**Day 5**: Update integration test to use brain API

---

### Week 3: Output Decoding
**Days 1-2**: Implement spike_nlp_decode_output()
- Spike rate calculation
- Window-based decoding
- Normalization

**Days 3-4**: Create neuron spike history
- Add spike buffer to neurons
- Efficient windowed queries
- Memory management

**Day 5**: Test output decoding in isolation

---

### Week 4: End-to-End Demo
**Days 1-2**: Create phase5_learning_demo.c
- XOR-like task
- Training loop
- Accuracy measurement

**Days 3-4**: Debug and tune
- Adjust learning rates
- Tune neuromodulation
- Optimize for learning

**Day 5**: Document results and benchmarks

---

## Success Criteria

### Must Have (Phase 5 Complete):
1. ✅ Integration test shows spike activity > 0
2. ✅ Integration test shows STDP events > 0
3. ✅ Unified brain_update() API working
4. ✅ Output decoding functional
5. ✅ End-to-end demo achieves >75% accuracy

### Nice to Have (Future):
- GPU-accelerated brain_update()
- Multi-region brain coordination
- Attention-based output selection
- Visualization tools

---

## Expected Outcomes

### Before Phase 5:
```
Integration Test Results:
- Spikes: 0
- STDP: 0 LTP, 0 LTD
- Learning: IDLE
- Verdict: "Integrated on paper, idle in practice" (6/10)
```

### After Phase 5:
```
Integration Test Results:
- Spikes: 100+ per trial
- STDP: 50+ LTP, 30+ LTD events
- Learning: ACTIVE
- Verdict: "Emergent learning demonstrated" (9/10)

Learning Demo Results:
- Accuracy: 85% (XOR-like task)
- Training time: 1000 trials
- Proof: Whole > Sum of Parts ✓
```

---

## Files to Create/Modify

### New Files:
- `include/core/brain/nimcp_brain.h` - Unified brain API
- `src/core/brain/nimcp_brain.c` - Implementation
- `examples/phase5_learning_demo.c` - End-to-end demo

### Modified Files:
- `src/nlp/nimcp_spike_nlp.c` - Fix encoding, add decoding
- `src/nlp/nimcp_spike_nlp.h` - Add decode_output prototype
- `src/core/neuralnet/nimcp_neuron.c` - Add spike history (if needed)
- `examples/full_system_integration_test.c` - Update to use brain API
- `CMakeLists.txt` - Add brain module
- `src/lib/CMakeLists.txt` - Link brain sources

### Documentation:
- `docs/features/PHASE_5_RESULTS.md` - Benchmarks and findings
- `docs/api/brain_api.md` - Brain API reference
- `CHANGELOG.md` - Phase 5 release notes

---

## Risk Mitigation

### Risk 1: Spike encoding unfixable
**Likelihood**: Low
**Mitigation**: Fallback to direct spike injection for testing
**Backup Plan**: Investigate neuron model parameters

### Risk 2: brain_update() too complex
**Likelihood**: Medium
**Mitigation**: Start simple, add features incrementally
**Backup Plan**: Keep manual integration as option

### Risk 3: Cannot achieve learning in demo
**Likelihood**: Medium
**Mitigation**: Start with easier task (pattern memorization)
**Backup Plan**: Tune hyperparameters systematically

---

## Timeline

**Start Date**: 2025-11-08
**Target Completion**: 2025-12-06 (4 weeks)

**Milestones**:
- Week 1: Spike encoding fixed ✅
- Week 2: Brain API working ✅
- Week 3: Output decoding ready ✅
- Week 4: Learning demo successful ✅

**Phase 5 Complete Criteria**: Integration test verdict upgrades from 6/10 to 9/10

---

## Next Phase Preview

**Phase 6: Optimization & Scale**
- GPU acceleration
- Distributed learning
- Large-scale benchmarks
- Production deployment

**But first**: Make Phase 5 work - prove the system can learn!

---

## References

- Systems Analysis: `docs/SYSTEMS_ANALYSIS.md`
- Integration Test: `examples/full_system_integration_test.c`
- Phase 4 (STDP): `docs/features/PHASE_4_PLAN.md`
- Spike NLP: `src/nlp/nimcp_spike_nlp.c`
