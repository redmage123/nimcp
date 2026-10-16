# Master Implementation Plan: All Options

**Date**: 2025-11-12
**Scope**: Execute all 4 post-Phase-C2.2 options
**Duration**: ~4-6 weeks

---

## Overview

This master plan covers comprehensive enhancement of the NIMCP neuromodulation system across four dimensions:

1. **Option 2: Integration** (Week 1) - Wire Phase C2.2 into existing systems
2. **Option 1: Phase C2.3** (Weeks 2-3) - Complete neuromodulator enhancement roadmap
3. **Option 4: Optimization** (Week 4) - Scale to large simulations
4. **Option 3: Validation** (Weeks 5-6) - Clinical and scientific validation

---

## OPTION 2: Integration with Existing Systems (Week 1)

### Priority: HIGH (immediate impact)

### 2.1 Dopamine-Modulated STDP (Day 1-2)

**Goal**: Learning rates modulated by dopamine concentration

**Current State**: STDP exists but doesn't use neuromodulator system

**Implementation Plan**:
```c
// File: src/plasticity/stdp/nimcp_stdp.c

// Modify stdp_update() to get dopamine level
float stdp_update_with_modulation(synapse_t* synapse,
                                   float dt_pre_post,
                                   neuromodulator_system_t* neuromod) {
    // Get dopamine concentration
    float da_conc = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);

    // Get phasic-tonic state for burst detection
    phasic_tonic_state_t* dopamine = &neuromod->dopamine_phasic_tonic;
    bool is_bursting = phasic_tonic_is_bursting(dopamine);

    // Modulate learning rate
    float base_lr = synapse->learning_rate;
    float modulated_lr = base_lr * (1.0f + da_conc * 100.0f);  // Scale to biological range

    // Apply burst amplification
    if (is_bursting) {
        modulated_lr *= 3.0f;  // 3x amplification during bursts
    }

    // Standard STDP with modulated learning rate
    float weight_change = 0.0f;
    if (dt_pre_post > 0) {
        // LTP: pre before post
        weight_change = modulated_lr * exp(-dt_pre_post / TAU_PLUS);
    } else {
        // LTD: post before pre
        weight_change = -modulated_lr * exp(dt_pre_post / TAU_MINUS);
    }

    return weight_change;
}
```

**Test Plan**:
- Unit tests: STDP with/without dopamine modulation
- Integration test: Reward learning task with burst amplification
- Expected: 3x faster learning during dopamine bursts

**Files to Create/Modify**:
- `src/plasticity/stdp/nimcp_stdp.c` (modify stdp_update)
- `src/plasticity/stdp/nimcp_stdp.h` (add neuromodulator parameter)
- `test/unit/test_stdp_modulation.cpp` (15 tests)
- `docs/STDP_MODULATION.md` (documentation)

**Deliverable**: Dopamine-modulated STDP with 3x learning amplification during bursts

---

### 2.2 Eligibility Traces with Burst-Triggered Consolidation (Day 2-3)

**Goal**: Eligibility traces converted to weight changes only during dopamine bursts

**Current State**: Eligibility traces exist (Phase 11) but not linked to bursts

**Implementation Plan**:
```c
// File: src/plasticity/eligibility/nimcp_eligibility_traces.c

// Modify eligibility_trace_update() to check for bursts
void eligibility_trace_consolidate(synapse_t* synapse,
                                    neuromodulator_system_t* neuromod,
                                    float dt) {
    // Get phasic-tonic state
    phasic_tonic_state_t* dopamine = &neuromod->dopamine_phasic_tonic;

    // Get burst amplitude
    float burst_amplitude = phasic_tonic_get_burst_amplitude(dopamine);

    // Only consolidate during bursts
    if (burst_amplitude > 0.001f) {  // Threshold: 1 nM burst
        // Convert eligibility trace to weight change
        float consolidation_rate = burst_amplitude * 1000.0f;  // Scale to [0-1]
        float weight_change = synapse->eligibility_trace * consolidation_rate * dt;

        // Apply to synapse
        synapse->weight += weight_change;
        synapse->weight = clamp(synapse->weight, 0.0f, 1.0f);

        // Decay eligibility trace after consolidation
        synapse->eligibility_trace *= 0.5f;  // 50% decay on consolidation
    }

    // Normal trace decay
    synapse->eligibility_trace *= exp(-dt / TAU_ELIGIBILITY);
}
```

**Test Plan**:
- Unit tests: Eligibility trace accumulation and consolidation
- Integration test: Three-factor learning (Hebbian + Eligibility + Dopamine)
- Expected: Weight changes only during reward delivery

**Files to Create/Modify**:
- `src/plasticity/eligibility/nimcp_eligibility_traces.c` (add consolidation)
- `src/plasticity/eligibility/nimcp_eligibility_traces.h` (new function)
- `test/unit/test_eligibility_consolidation.cpp` (12 tests)
- `docs/ELIGIBILITY_CONSOLIDATION.md` (documentation)

**Deliverable**: Burst-triggered eligibility trace consolidation

---

### 2.3 Per-Neuron Receptor Profiles (Day 3-4)

**Goal**: Each neuron has individual receptor expression profile

**Current State**: Only regional profiles (cortical, striatal)

**Implementation Plan**:
```c
// File: src/core/neuron/nimcp_neuron.h

typedef struct {
    // ... existing neuron fields ...

    // OPTION 2: Per-neuron receptor profile
    neuron_receptor_profile_t* receptor_profile;  // Individual expression pattern

} neuron_t;

// File: src/core/neuron/nimcp_neuron.c

neuron_t* neuron_create_with_receptor_profile(neuron_config_t* config,
                                                brain_region_t region) {
    neuron_t* neuron = neuron_create(config);

    // Allocate per-neuron receptor profile
    neuron->receptor_profile = malloc(sizeof(neuron_receptor_profile_t));

    // Initialize based on region with variation
    if (region == BRAIN_REGION_CORTEX) {
        *neuron->receptor_profile = receptor_profile_cortical();

        // Add 10% individual variation
        add_receptor_variation(neuron->receptor_profile, 0.1f);

    } else if (region == BRAIN_REGION_STRIATUM) {
        *neuron->receptor_profile = receptor_profile_striatal();
        add_receptor_variation(neuron->receptor_profile, 0.1f);
    }

    return neuron;
}

// Compute per-neuron modulation
void neuron_update_neuromodulation(neuron_t* neuron,
                                   neuromodulator_system_t* neuromod,
                                   float dt) {
    // Get dopamine concentration
    float da_conc = phasic_tonic_get_concentration(&neuromod->dopamine_phasic_tonic);

    // Compute receptor-mediated effects using neuron's profile
    dopamine_receptor_compute_modulation(&neuron->receptor_profile->dopamine,
                                         da_conc, dt);

    // Get net modulation
    float net_modulation = neuron->receptor_profile->dopamine.net_modulation;

    // Apply to neuron excitability
    neuron->excitability_modifier = 1.0f + net_modulation;
}
```

**Test Plan**:
- Unit tests: Per-neuron profile creation and variation
- Integration test: Population of neurons with diverse receptor expression
- Expected: 10% variation in receptor-mediated effects across neurons

**Files to Create/Modify**:
- `src/core/neuron/nimcp_neuron.h` (add receptor_profile field)
- `src/core/neuron/nimcp_neuron.c` (add profile initialization)
- `test/unit/test_per_neuron_receptors.cpp` (10 tests)
- `docs/PER_NEURON_RECEPTORS.md` (documentation)

**Deliverable**: Heterogeneous receptor expression across neuron population

---

## OPTION 1: Phase C2.3 Enhancements (Weeks 2-3)

### Priority: MEDIUM (completes roadmap)

### 1.1 Enhancement #3: Synaptic Vesicle Packaging (Days 5-7)

**Biological Fidelity**:
- RRP: 10-20 vesicles (immediately available)
- Recycling pool: 50-100 vesicles (refilling)
- Reserve pool: 500-1000 vesicles (long-term storage)
- Release probability: 0.1-0.5 (varies by synapse type)
- Quantal size: 5000-10000 molecules per vesicle

**Implementation Plan**:
```c
// File: src/include/plasticity/neuromodulators/nimcp_vesicle_pool.h

typedef struct {
    // Vesicle pools
    uint32_t readily_releasable_pool;  // RRP size
    uint32_t recycling_pool;           // Refilling
    uint32_t reserve_pool;             // Reserve

    // Release parameters
    float release_probability;         // Pr [0-1]
    float quantal_size;               // Molecules per vesicle

    // Refill dynamics
    float refill_rate;                // RRP refill (vesicles/sec)
    float mobilization_rate;          // Reserve → Recycling

    // Depletion state
    bool is_depleted;
    float depletion_factor;           // 0 = empty, 1 = full

    // Calcium-dependent facilitation
    float calcium_residual;           // Residual Ca2+ [µM]
    float base_pr;                    // Baseline Pr
    float facilitation_tau;           // Ca2+ decay time

} vesicle_pool_t;

// Release function
float vesicle_pool_release(vesicle_pool_t* pool, bool action_potential);
void vesicle_pool_refill(vesicle_pool_t* pool, float dt);
void vesicle_pool_update_facilitation(vesicle_pool_t* pool, float dt);
```

**Test Plan** (20 tests):
- Initialization (3 tests)
- Release dynamics (5 tests)
- Depletion and recovery (4 tests)
- Facilitation (4 tests)
- Depression (2 tests)
- Performance (2 tests)

**Files to Create**:
- `src/include/plasticity/neuromodulators/nimcp_vesicle_pool.h` (200 lines)
- `src/plasticity/neuromodulators/nimcp_vesicle_pool.c` (300 lines)
- `test/unit/test_vesicle_pool.cpp` (350 lines)
- `docs/PHASE_C2_3_ENH3_VESICLE_POOL.md` (500 lines)

**Deliverable**: Vesicle packaging with depletion and facilitation (20/20 tests)

---

### 1.2 Enhancement #4: Metabolic Pathways (Days 8-10)

**Biological Fidelity**:
- Synthesis: Tyrosine → L-DOPA → Dopamine (tyrosine hydroxylase rate-limiting)
- Degradation: MAO (mitochondrial), COMT (cytoplasmic)
- Reuptake: DAT (dopamine transporter), 90% recycled
- Half-life: Extracellular DA ~200ms, intracellular ~hours

**Implementation Plan**:
```c
// File: src/include/plasticity/neuromodulators/nimcp_metabolic_pathway.h

typedef struct {
    // Precursors
    float precursor_concentration;    // Tyrosine, Tryptophan [mM]
    float precursor_uptake_rate;      // Across blood-brain barrier

    // Synthesis
    float synthesis_enzyme_activity;  // TH, TPH activity [0-1]
    float synthesis_rate;             // Molecules/second

    // Degradation
    float mao_activity;               // Monoamine oxidase [0-1]
    float comt_activity;              // COMT [0-1]
    float degradation_rate;           // Molecules/second

    // Reuptake
    float transporter_density;        // DAT/SERT/NET per µm²
    float reuptake_rate;              // Molecules/second
    float reuptake_efficiency;        // Fraction recycled [0.8-0.95]

    // Metabolites
    float metabolite_concentration;   // DOPAC, 5-HIAA [µM]

} metabolic_pathway_t;

// Functions
void metabolic_pathway_synthesize(metabolic_pathway_t* pathway, float dt);
void metabolic_pathway_degrade(metabolic_pathway_t* pathway, float concentration, float dt);
float metabolic_pathway_reuptake(metabolic_pathway_t* pathway, float extracellular, float dt);
```

**Drug Simulations**:
- MAO inhibitors: `mao_activity = 0.1f` (90% inhibition)
- COMT inhibitors: `comt_activity = 0.1f`
- SSRIs: `reuptake_efficiency = 0.15f` (85% blockade)
- L-DOPA supplementation: `precursor_concentration *= 10.0f`

**Test Plan** (18 tests):
- Initialization (2 tests)
- Synthesis (4 tests)
- Degradation (4 tests)
- Reuptake (4 tests)
- Drug simulations (3 tests)
- Performance (1 test)

**Files to Create**:
- `src/include/plasticity/neuromodulators/nimcp_metabolic_pathway.h` (180 lines)
- `src/plasticity/neuromodulators/nimcp_metabolic_pathway.c` (280 lines)
- `test/unit/test_metabolic_pathway.cpp` (320 lines)
- `docs/PHASE_C2_3_ENH4_METABOLIC_PATHWAY.md` (450 lines)

**Deliverable**: Metabolic pathways with drug simulations (18/18 tests)

---

### 1.3 Enhancement #5: Quantum Properties (Days 11-13)

**Biological Motivation**:
- Quantum tunneling in electron transfer (oxidation-reduction)
- Coherence times: 10-100 femtoseconds in biological systems
- Potential role in receptor binding and signal transduction

**Implementation Plan**:
```c
// File: src/include/plasticity/neuromodulators/nimcp_quantum_neuromod.h

typedef struct {
    // Quantum tunneling
    float barrier_width;              // Potential barrier [Å]
    float barrier_height;             // Energy barrier [eV]
    float tunneling_probability;      // P(tunnel) [0-1]

    // Coherence
    float coherence_time;             // Decoherence time [fs]
    float coherence_decay_rate;       // 1/coherence_time

    // Quantum walk integration
    quantum_walker_t* diffusion_walker;  // Link to Phase C2.1
    bool use_quantum_diffusion;

    // Entanglement
    float entanglement_degree;        // 0 = classical, 1 = maximal
    uint32_t entangled_partner_id;    // ID of entangled neuron

} quantum_neuromod_t;

// Functions
float quantum_tunneling_probability(float barrier_width, float barrier_height);
void quantum_neuromod_update_coherence(quantum_neuromod_t* quantum, float dt);
void quantum_neuromod_diffuse(quantum_neuromod_t* quantum, spatial_grid_t* grid, float dt);
```

**Test Plan** (15 tests):
- Tunneling probability (3 tests)
- Coherence decay (3 tests)
- Quantum walk diffusion (4 tests)
- Entanglement (3 tests)
- Performance (2 tests)

**Files to Create**:
- `src/include/plasticity/neuromodulators/nimcp_quantum_neuromod.h` (150 lines)
- `src/plasticity/neuromodulators/nimcp_quantum_neuromod.c` (220 lines)
- `test/unit/test_quantum_neuromod.cpp` (280 lines)
- `docs/PHASE_C2_3_ENH5_QUANTUM_NEUROMOD.md` (400 lines)

**Deliverable**: Quantum properties in neuromodulation (15/15 tests)

---

## OPTION 4: Performance Optimization (Week 4)

### Priority: MEDIUM-HIGH (enables large-scale)

### 4.1 GPU Acceleration for Receptor Updates (Days 14-16)

**Goal**: 100x speedup for receptor computation

**Implementation Plan**:
```c
// File: src/gpu/nimcp_gpu_receptors.cu

__global__ void gpu_receptor_update_kernel(
    neuron_receptor_profile_t* profiles,
    float* dopamine_concentrations,
    uint32_t num_neurons,
    float dt
) {
    uint32_t neuron_id = blockIdx.x * blockDim.x + threadIdx.x;

    if (neuron_id < num_neurons) {
        neuron_receptor_profile_t* profile = &profiles[neuron_id];
        float da_conc = dopamine_concentrations[neuron_id];

        // Compute receptor modulation (embarrassingly parallel)
        dopamine_receptor_compute_modulation(&profile->dopamine, da_conc, dt);
    }
}

// Host function
void gpu_update_all_receptors(brain_t* brain, float dt) {
    // Transfer data to GPU
    cudaMemcpy(d_profiles, brain->neuron_profiles, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_concentrations, brain->dopamine_map, size, cudaMemcpyHostToDevice);

    // Launch kernel
    int threads_per_block = 256;
    int num_blocks = (brain->num_neurons + threads_per_block - 1) / threads_per_block;
    gpu_receptor_update_kernel<<<num_blocks, threads_per_block>>>(
        d_profiles, d_concentrations, brain->num_neurons, dt);

    // Transfer results back
    cudaMemcpy(brain->neuron_profiles, d_profiles, size, cudaMemcpyDeviceToHost);
}
```

**Expected Performance**:
- CPU (single-threaded): 5.9M updates/sec
- GPU (RTX 3090): 500M+ updates/sec (100x speedup)
- Scales to 10M neurons in real-time

**Test Plan** (8 tests):
- GPU initialization (2 tests)
- Correctness vs CPU (3 tests)
- Performance benchmarks (3 tests)

**Files to Create**:
- `src/gpu/nimcp_gpu_receptors.cu` (250 lines)
- `src/gpu/nimcp_gpu_receptors.h` (80 lines)
- `test/unit/test_gpu_receptors.cpp` (180 lines)
- `docs/GPU_ACCELERATION.md` (300 lines)

**Deliverable**: GPU-accelerated receptor updates (100x speedup)

---

### 4.2 SIMD Vectorization (Days 16-17)

**Goal**: 4-8x speedup using AVX2/AVX512

**Implementation Plan**:
```c
// File: src/plasticity/neuromodulators/nimcp_phasic_tonic_simd.c

#include <immintrin.h>  // AVX2 intrinsics

void phasic_tonic_update_batch_simd(phasic_tonic_state_t* states,
                                     uint32_t count,
                                     float dt) {
    // Process 8 states at a time with AVX2
    __m256 dt_vec = _mm256_set1_ps(dt);

    for (uint32_t i = 0; i < count; i += 8) {
        // Load 8 tonic levels
        __m256 tonic = _mm256_loadu_ps(&states[i].tonic_level);

        // Load 8 phasic bursts
        __m256 phasic = _mm256_loadu_ps(&states[i].phasic_burst);

        // Load decay constants
        __m256 decay = _mm256_loadu_ps(&states[i].burst_decay_tau);

        // Compute: phasic *= exp(-dt/tau)
        __m256 decay_factor = _mm256_div_ps(dt_vec, decay);
        decay_factor = _mm256_mul_ps(decay_factor, _mm256_set1_ps(-1.0f));
        // Use fast exp approximation
        decay_factor = _mm256_exp_ps(decay_factor);
        phasic = _mm256_mul_ps(phasic, decay_factor);

        // Store results
        _mm256_storeu_ps(&states[i].phasic_burst, phasic);
    }
}
```

**Expected Performance**:
- Scalar: 33.1M updates/sec
- AVX2 (8-wide): 200M+ updates/sec (6x speedup)
- AVX512 (16-wide): 400M+ updates/sec (12x speedup)

**Test Plan** (6 tests):
- SIMD correctness (3 tests)
- Performance benchmarks (3 tests)

**Files to Create**:
- `src/plasticity/neuromodulators/nimcp_phasic_tonic_simd.c` (180 lines)
- `test/unit/test_simd_phasic_tonic.cpp` (120 lines)
- `docs/SIMD_OPTIMIZATION.md` (200 lines)

**Deliverable**: SIMD-vectorized phasic-tonic updates (6-12x speedup)

---

## OPTION 3: Clinical Validation (Weeks 5-6)

### Priority: MEDIUM (validates implementation)

### 3.1 Replicate Schultz Experiments (Days 18-20)

**Goal**: Replicate key findings from Schultz et al. (1997)

**Experiments to Replicate**:

1. **Reward Delivery**:
   - Expected: Dopamine burst (~100 spikes/sec, 200ms duration)
   - NIMCP: Burst amplitude 1 µM, decay τ = 150ms

2. **Reward Omission**:
   - Expected: Dopamine dip (pause in firing, <5 spikes/sec)
   - NIMCP: Tonic dip to 30% baseline

3. **Reward Prediction**:
   - Expected: Burst shifts from reward to predictive cue
   - NIMCP: TD error encoding (cue → RPE → burst)

4. **Fully Predicted Reward**:
   - Expected: No dopamine response
   - NIMCP: TD error = 0 → no burst

**Test Plan**:
- Create experimental setup matching Schultz protocol
- Run 100 trials per condition
- Statistical analysis (t-tests, ANOVA)
- Generate figures matching Schultz publications

**Files to Create**:
- `validation/schultz_replication.c` (400 lines)
- `validation/schultz_analysis.py` (250 lines)
- `docs/SCHULTZ_REPLICATION_REPORT.md` (600 lines)

**Deliverable**: Schultz experiment replication with statistical validation

---

### 3.2 Receptor Binding Curves (Days 20-21)

**Goal**: Validate receptor binding against IUPHAR database

**Experiments**:
1. D1 dose-response: Kd = 5 nM (literature: 3-8 nM)
2. D2 dose-response: Kd = 0.5 nM (literature: 0.3-0.8 nM)
3. 5-HT1A dose-response: Kd = 2 nM (literature: 1-3 nM)
4. Hill coefficients: n = 1.0 (non-cooperative binding)

**Test Plan**:
- Vary dopamine concentration: 0.1 nM - 10 µM
- Measure receptor occupancy
- Fit Hill equation
- Compare Kd values to literature

**Files to Create**:
- `validation/receptor_binding_curves.c` (250 lines)
- `validation/binding_analysis.py` (180 lines)
- `docs/RECEPTOR_VALIDATION_REPORT.md` (400 lines)

**Deliverable**: Receptor binding validation report

---

### 3.3 Clinical Disorder Models (Days 22-28)

**Goal**: Comprehensive clinical disorder models

**Models to Create**:

1. **Major Depressive Disorder**:
   - Pathology: Reduced serotonin (50% tonic)
   - Symptoms: Anhedonia, low motivation
   - Treatment: SSRIs (85% reuptake blockade)
   - Expected outcome: Gradual recovery over 2-4 weeks

2. **Schizophrenia**:
   - Pathology: Hyperdopaminergic striatum (200% tonic)
   - Symptoms: Aberrant salience, positive symptoms
   - Treatment: D2 antagonists (80% blockade)
   - Expected outcome: Reduced positive symptoms, risk of negative symptoms

3. **Parkinson's Disease**:
   - Pathology: Dopamine depletion (10% remaining)
   - Symptoms: Motor impairment, bradykinesia
   - Treatment: L-DOPA (pulsatile bursts)
   - Expected outcome: Motor improvement with dyskinesia risk

4. **Addiction**:
   - Pathology: Exaggerated TD errors (RPE = 2-3x normal)
   - Symptoms: Craving, compulsive behavior
   - Treatment: Dopamine stabilization
   - Expected outcome: Reduced craving

5. **Alzheimer's Disease**:
   - Pathology: Cholinergic deficit (30% ACh)
   - Symptoms: Memory impairment
   - Treatment: Cholinesterase inhibitors
   - Expected outcome: Modest memory improvement

**Test Plan**:
- Implement each disorder pathology
- Run behavioral simulations
- Apply treatments
- Measure outcomes (symptom scores, behavioral metrics)

**Files to Create**:
- `validation/clinical_models/` (directory with 5 models)
- `validation/clinical_analysis.py` (500 lines)
- `docs/CLINICAL_VALIDATION_COMPREHENSIVE.md` (1000+ lines)

**Deliverable**: 5 validated clinical disorder models with treatment simulations

---

## Timeline Summary

| Week | Phase | Tasks | Deliverables |
|------|-------|-------|--------------|
| 1 | Option 2 | STDP modulation, eligibility traces, per-neuron profiles | 3 integrations, 37 tests |
| 2 | Option 1 | Vesicle packaging, metabolic pathways (start) | Enhancement #3, 20 tests |
| 3 | Option 1 | Metabolic pathways (finish), quantum properties | Enhancements #4-5, 33 tests |
| 4 | Option 4 | GPU acceleration, SIMD vectorization | 100x speedup, 14 tests |
| 5 | Option 3 | Schultz replication, receptor validation | 2 validation reports |
| 6 | Option 3 | Clinical disorder models | 5 disorder models |

---

## Success Metrics

### Option 2 (Integration)
- ✅ STDP learning rate increases 3x during dopamine bursts
- ✅ Eligibility traces consolidate only during bursts
- ✅ Per-neuron receptor variation: 10% coefficient of variation
- ✅ All integration tests passing (37/37)

### Option 1 (Phase C2.3)
- ✅ Vesicle depletion with high-frequency stimulation
- ✅ Metabolic drug simulations match expected effects
- ✅ Quantum tunneling probabilities match theoretical predictions
- ✅ All enhancement tests passing (53/53)

### Option 4 (Optimization)
- ✅ GPU: 100x speedup over CPU (5.9M → 500M+ updates/sec)
- ✅ SIMD: 6-8x speedup (33M → 200M+ updates/sec)
- ✅ Real-time simulation of 10M neurons

### Option 3 (Validation)
- ✅ Schultz experiments replicated with p < 0.01
- ✅ Receptor Kd values within ±20% of literature
- ✅ 5 clinical models with biologically plausible symptom profiles
- ✅ Treatment effects match clinical expectations

---

## Resource Requirements

### Compute
- CPU: Multi-core (8+ cores recommended)
- GPU: NVIDIA RTX 3090 or better (for GPU acceleration)
- RAM: 32 GB (64 GB for large-scale validation)

### Storage
- Code: +5000 lines (+15 new files)
- Tests: +1400 lines (+12 test files)
- Documentation: +5000 lines (+15 docs)
- Validation data: ~100 MB (experiment results)

### Dependencies
- CUDA Toolkit 11.0+ (for GPU acceleration)
- AVX2/AVX512 support (for SIMD)
- Python 3.8+ with numpy, matplotlib, scipy (for validation)

---

## Risk Mitigation

### Technical Risks
1. **GPU availability**: Fall back to OpenCL or CPU-only
2. **SIMD compatibility**: Auto-detect CPU features, graceful fallback
3. **Test failures**: Incremental development with continuous testing

### Scientific Risks
1. **Validation mismatches**: Parameter tuning, literature review
2. **Clinical model complexity**: Start with simplified models, add complexity

### Schedule Risks
1. **Scope creep**: Strict prioritization, MVP for each option
2. **Integration issues**: Daily integration testing

---

## Next Steps

Starting with **Option 2** (Integration) as it provides immediate value:

1. ✅ Create master plan (CURRENT)
2. → Implement dopamine-modulated STDP
3. → Wire eligibility traces with bursts
4. → Add per-neuron receptor profiles
5. → Continue with Option 1 (Phase C2.3)

---

**Master Plan Status**: ✅ COMPLETE
**Ready to Begin Execution**: Option 2.1 (Dopamine-Modulated STDP)
