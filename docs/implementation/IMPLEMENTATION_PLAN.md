# NIMCP Complete System Integration - Implementation Plan

## Executive Summary

Based on comprehensive analysis of all NIMCP subsystems, the system is approximately **70-75% complete**. This plan addresses critical integration gaps across 4 phases to achieve a fully wired, production-ready neural simulation system.

---

## Phase 1: Critical Learning Path (Priority: CRITICAL)

### Objective
Wire core neural components (dendrites, synapses, axons) to plasticity mechanisms and connect glial cells to immune system.

### 1.1 Neural Component ↔ Plasticity Bridges

These bridges are **critical** because dendrites, synapses, and axons are currently isolated from all plasticity mechanisms - meaning learning cannot propagate through the network structure.

#### 1.1.1 Dendrite-Plasticity Bridge

**Files to Create:**
- `include/plasticity/bridges/nimcp_dendrite_plasticity_bridge.h`
- `src/plasticity/bridges/nimcp_dendrite_plasticity_bridge.c`
- `test/unit/plasticity/bridges/test_dendrite_plasticity_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_dendritic_spine_t* dendrite;
    plasticity_orchestrator_t* plasticity_orch;

    // Plasticity type connections
    stdp_state_t* stdp;
    bcm_state_t* bcm;
    homeostatic_state_t* homeostatic;
    dendritic_plasticity_state_t* dendritic;

    // Configuration
    dendrite_plasticity_config_t config;

    // Metrics
    float local_calcium_level;
    float spine_growth_rate;
    float branch_point_plasticity;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} dendrite_plasticity_bridge_t;
```

**API Functions:**
```c
dendrite_plasticity_bridge_t* dendrite_plasticity_create(
    const dendrite_plasticity_config_t* config,
    nimcp_dendritic_spine_t* dendrite,
    plasticity_orchestrator_t* orch);
void dendrite_plasticity_destroy(dendrite_plasticity_bridge_t* bridge);

// Plasticity connections
int dendrite_plasticity_connect_stdp(dendrite_plasticity_bridge_t* bridge, stdp_state_t* stdp);
int dendrite_plasticity_connect_bcm(dendrite_plasticity_bridge_t* bridge, bcm_state_t* bcm);
int dendrite_plasticity_connect_homeostatic(dendrite_plasticity_bridge_t* bridge, homeostatic_state_t* h);
int dendrite_plasticity_connect_dendritic(dendrite_plasticity_bridge_t* bridge, dendritic_plasticity_state_t* d);

// Update functions
int dendrite_plasticity_update_calcium(dendrite_plasticity_bridge_t* bridge, float calcium_influx);
int dendrite_plasticity_apply_stdp(dendrite_plasticity_bridge_t* bridge, float pre_time, float post_time);
int dendrite_plasticity_apply_structural(dendrite_plasticity_bridge_t* bridge);

// Bio-async
int dendrite_plasticity_connect_bio_async(dendrite_plasticity_bridge_t* bridge);
int dendrite_plasticity_disconnect_bio_async(dendrite_plasticity_bridge_t* bridge);
```

**Integration Points:**
- Receives calcium signals from dendritic spines
- Applies STDP based on pre/post spike timing
- Modulates spine growth via structural plasticity
- Reports to plasticity orchestrator for coordination

---

#### 1.1.2 Synapse-Plasticity Bridge

**Files to Create:**
- `include/plasticity/bridges/nimcp_synapse_plasticity_bridge.h`
- `src/plasticity/bridges/nimcp_synapse_plasticity_bridge.c`
- `test/unit/plasticity/bridges/test_synapse_plasticity_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_synapse_t* synapse;
    plasticity_orchestrator_t* plasticity_orch;

    // All 17 plasticity mechanisms
    stdp_state_t* stdp;
    bcm_state_t* bcm;
    homeostatic_state_t* homeostatic;
    stp_state_t* stp;
    metaplasticity_state_t* meta;
    eligibility_state_t* eligibility;
    heterosynaptic_state_t* hetero;
    synaptic_scaling_state_t* scaling;
    synaptic_tagging_state_t* tagging;
    calcium_dynamics_state_t* calcium;
    neuromodulator_state_t* neuromod;
    metabolic_state_t* metabolic;
    structural_state_t* structural;
    gliotransmission_state_t* glial;
    spike_frequency_adaptation_state_t* sfa;
    intrinsic_excitability_state_t* intrinsic;
    dendritic_plasticity_state_t* dendritic;

    // State
    synapse_plasticity_config_t config;
    float weight_delta_accumulator;
    uint64_t last_pre_spike_time;
    uint64_t last_post_spike_time;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} synapse_plasticity_bridge_t;
```

**API Functions:**
```c
synapse_plasticity_bridge_t* synapse_plasticity_create(
    const synapse_plasticity_config_t* config,
    nimcp_synapse_t* synapse,
    plasticity_orchestrator_t* orch);
void synapse_plasticity_destroy(synapse_plasticity_bridge_t* bridge);

// Connect all 17 plasticity mechanisms
int synapse_plasticity_connect_stdp(synapse_plasticity_bridge_t* bridge, stdp_state_t* stdp);
int synapse_plasticity_connect_bcm(synapse_plasticity_bridge_t* bridge, bcm_state_t* bcm);
int synapse_plasticity_connect_stp(synapse_plasticity_bridge_t* bridge, stp_state_t* stp);
int synapse_plasticity_connect_homeostatic(synapse_plasticity_bridge_t* bridge, homeostatic_state_t* h);
int synapse_plasticity_connect_metaplasticity(synapse_plasticity_bridge_t* bridge, metaplasticity_state_t* m);
int synapse_plasticity_connect_eligibility(synapse_plasticity_bridge_t* bridge, eligibility_state_t* e);
int synapse_plasticity_connect_heterosynaptic(synapse_plasticity_bridge_t* bridge, heterosynaptic_state_t* h);
int synapse_plasticity_connect_scaling(synapse_plasticity_bridge_t* bridge, synaptic_scaling_state_t* s);
int synapse_plasticity_connect_tagging(synapse_plasticity_bridge_t* bridge, synaptic_tagging_state_t* t);
int synapse_plasticity_connect_calcium(synapse_plasticity_bridge_t* bridge, calcium_dynamics_state_t* c);
int synapse_plasticity_connect_neuromod(synapse_plasticity_bridge_t* bridge, neuromodulator_state_t* n);
int synapse_plasticity_connect_metabolic(synapse_plasticity_bridge_t* bridge, metabolic_state_t* m);
int synapse_plasticity_connect_structural(synapse_plasticity_bridge_t* bridge, structural_state_t* s);
int synapse_plasticity_connect_glial(synapse_plasticity_bridge_t* bridge, gliotransmission_state_t* g);
int synapse_plasticity_connect_sfa(synapse_plasticity_bridge_t* bridge, spike_frequency_adaptation_state_t* s);
int synapse_plasticity_connect_intrinsic(synapse_plasticity_bridge_t* bridge, intrinsic_excitability_state_t* i);
int synapse_plasticity_connect_dendritic(synapse_plasticity_bridge_t* bridge, dendritic_plasticity_state_t* d);

// Update functions
int synapse_plasticity_on_pre_spike(synapse_plasticity_bridge_t* bridge, uint64_t spike_time);
int synapse_plasticity_on_post_spike(synapse_plasticity_bridge_t* bridge, uint64_t spike_time);
int synapse_plasticity_apply_accumulated(synapse_plasticity_bridge_t* bridge);
float synapse_plasticity_get_effective_weight(synapse_plasticity_bridge_t* bridge);

// Bulk operations
int synapse_plasticity_connect_all(synapse_plasticity_bridge_t* bridge, plasticity_orchestrator_t* orch);
```

**Integration Points:**
- Central hub for all plasticity → synapse interactions
- Accumulates weight changes from all mechanisms
- Coordinates with orchestrator for timing
- Reports metrics to cognitive systems

---

#### 1.1.3 Axon-Plasticity Bridge

**Files to Create:**
- `include/plasticity/bridges/nimcp_axon_plasticity_bridge.h`
- `src/plasticity/bridges/nimcp_axon_plasticity_bridge.c`
- `test/unit/plasticity/bridges/test_axon_plasticity_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_axon_t* axon;
    plasticity_orchestrator_t* plasticity_orch;

    // Axon-relevant plasticity
    structural_state_t* structural;
    intrinsic_excitability_state_t* intrinsic;
    metabolic_state_t* metabolic;

    // Myelin connection
    nimcp_myelin_sheath_t* myelin;

    // State
    axon_plasticity_config_t config;
    float conduction_velocity;
    float branch_growth_rate;
    float myelination_factor;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} axon_plasticity_bridge_t;
```

**API Functions:**
```c
axon_plasticity_bridge_t* axon_plasticity_create(
    const axon_plasticity_config_t* config,
    nimcp_axon_t* axon,
    plasticity_orchestrator_t* orch);
void axon_plasticity_destroy(axon_plasticity_bridge_t* bridge);

// Plasticity connections
int axon_plasticity_connect_structural(axon_plasticity_bridge_t* bridge, structural_state_t* s);
int axon_plasticity_connect_intrinsic(axon_plasticity_bridge_t* bridge, intrinsic_excitability_state_t* i);
int axon_plasticity_connect_metabolic(axon_plasticity_bridge_t* bridge, metabolic_state_t* m);

// Myelin integration
int axon_plasticity_connect_myelin(axon_plasticity_bridge_t* bridge, nimcp_myelin_sheath_t* myelin);
int axon_plasticity_update_myelination(axon_plasticity_bridge_t* bridge);

// Update functions
int axon_plasticity_update_conduction(axon_plasticity_bridge_t* bridge);
int axon_plasticity_apply_structural(axon_plasticity_bridge_t* bridge);
float axon_plasticity_get_conduction_velocity(axon_plasticity_bridge_t* bridge);
```

**Integration Points:**
- Modulates conduction velocity based on activity
- Coordinates with myelin for myelination plasticity
- Reports metabolic demands to metabolic plasticity
- Enables axonal sprouting/pruning via structural plasticity

---

### 1.2 Glial ↔ Immune Bridges

Glial cells (astrocytes, microglia, oligodendrocytes) need connection to brain immune system.

#### 1.2.1 Oligodendrocytes-Immune Bridge

**Files to Create:**
- `include/glial/immune/nimcp_oligodendrocytes_immune_bridge.h`
- `src/glial/immune/nimcp_oligodendrocytes_immune_bridge.c`
- `test/unit/glial/immune/test_oligodendrocytes_immune_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    oligodendrocyte_t* oligo;
    brain_immune_system_t* immune_system;

    // State
    oligo_immune_config_t config;
    oligo_cytokine_effects_t cytokine_effects;

    // Metrics
    float myelination_rate;
    float progenitor_recruitment;
    float remyelination_capacity;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} oligo_immune_bridge_t;

typedef struct {
    float il1_myelination_reduction;    // IL-1 reduces myelination
    float il6_progenitor_inhibition;    // IL-6 inhibits OPCs
    float tnf_oligodendrocyte_death;    // TNF causes cell death
    float il10_protection_factor;       // IL-10 is protective
    float ifn_gamma_demyelination;      // IFN-γ causes demyelination
} oligo_cytokine_effects_t;
```

**Biological Basis:**
- Inflammation (IL-1, TNF, IFN-γ) damages oligodendrocytes and myelin
- This models MS-like demyelination in neuroinflammation
- IL-10 provides protective anti-inflammatory effects
- Microglia-oligodendrocyte crosstalk is critical for remyelination

---

#### 1.2.2 Myelin-Immune Bridge

**Files to Create:**
- `include/glial/immune/nimcp_myelin_immune_bridge.h`
- `src/glial/immune/nimcp_myelin_immune_bridge.c`
- `test/unit/glial/immune/test_myelin_immune_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_myelin_sheath_t* myelin;
    brain_immune_system_t* immune_system;

    // State
    myelin_immune_config_t config;
    myelin_cytokine_effects_t cytokine_effects;

    // Metrics
    float sheath_integrity;
    float conduction_efficiency;
    float repair_rate;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} myelin_immune_bridge_t;
```

**Biological Basis:**
- Models autoimmune demyelination (MS pathophysiology)
- Cytokine storm → rapid myelin degradation
- Remyelination depends on inflammation resolution

---

#### 1.2.3 Astrocytes-Immune Bridge

**Files to Create:**
- `include/glial/immune/nimcp_astrocytes_immune_bridge.h`
- `src/glial/immune/nimcp_astrocytes_immune_bridge.c`
- `test/unit/glial/immune/test_astrocytes_immune_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_astrocyte_t* astrocyte;
    brain_immune_system_t* immune_system;

    // Reactive astrogliosis state
    astrocyte_reactivity_t reactivity_state;

    // Cytokine effects
    astro_immune_config_t config;
    astro_cytokine_effects_t cytokine_effects;

    // Metrics
    float glutamate_clearance_rate;
    float gliotransmitter_release;
    float scar_formation_progress;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} astro_immune_bridge_t;

typedef enum {
    ASTRO_QUIESCENT,      // Normal state
    ASTRO_A1_REACTIVE,    // Neurotoxic (inflammation-induced)
    ASTRO_A2_REACTIVE,    // Neuroprotective (ischemia-induced)
    ASTRO_SCAR_FORMING    // Chronic inflammation → glial scar
} astrocyte_reactivity_t;
```

**Biological Basis:**
- Models reactive astrogliosis (A1/A2 phenotypes)
- IL-1α, TNF, C1q from microglia → A1 (neurotoxic)
- A2 astrocytes release neurotrophins
- Chronic inflammation → glial scar (barrier to regeneration)

---

#### 1.2.4 Microglia-Immune Bridge Enhancement

**Existing file to enhance:**
- `include/glial/immune/nimcp_microglia_immune_bridge.h`
- `src/glial/immune/nimcp_microglia_immune_bridge.c`

**Additions needed:**
```c
// Connect microglia to oligodendrocytes for remyelination signaling
int microglia_immune_connect_oligodendrocytes(
    microglia_immune_bridge_t* bridge,
    oligo_immune_bridge_t* oligo_bridge);

// Connect microglia to astrocytes for A1/A2 polarization
int microglia_immune_connect_astrocytes(
    microglia_immune_bridge_t* bridge,
    astro_immune_bridge_t* astro_bridge);

// M1/M2 polarization effects on other glial cells
int microglia_immune_propagate_polarization(microglia_immune_bridge_t* bridge);
```

---

### Phase 1 Summary

| Component | Files | Tests | Priority |
|-----------|-------|-------|----------|
| Dendrite-Plasticity Bridge | 2 | 30+ | CRITICAL |
| Synapse-Plasticity Bridge | 2 | 50+ | CRITICAL |
| Axon-Plasticity Bridge | 2 | 25+ | CRITICAL |
| Oligodendrocytes-Immune | 2 | 20+ | HIGH |
| Myelin-Immune | 2 | 20+ | HIGH |
| Astrocytes-Immune | 2 | 25+ | HIGH |
| Microglia-Immune Enhancement | - | 10+ | HIGH |

**Total: 12 new files, ~180 tests**

---

## Phase 2: Sleep/Consolidation Integration

### Objective
Wire glial cells to sleep system for sleep-dependent plasticity and memory consolidation.

### 2.1 Astrocytes-Sleep Bridge

**Files to Create:**
- `include/glial/sleep/nimcp_astrocytes_sleep_bridge.h`
- `src/glial/sleep/nimcp_astrocytes_sleep_bridge.c`
- `test/unit/glial/sleep/test_astrocytes_sleep_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_astrocyte_t* astrocyte;
    sleep_system_t* sleep_system;

    // State
    astro_sleep_config_t config;

    // Sleep-related functions
    float adenosine_level;           // Sleep pressure marker
    float glymphatic_clearance;      // Waste clearance rate
    float synaptic_renormalization;  // Sleep-dependent downscaling

    // Metrics
    float sleep_pressure_contribution;
    float waste_clearance_efficiency;
    float metabolic_restoration;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} astro_sleep_bridge_t;
```

**API Functions:**
```c
// Core lifecycle
astro_sleep_bridge_t* astro_sleep_create(
    const astro_sleep_config_t* config,
    nimcp_astrocyte_t* astrocyte,
    sleep_system_t* sleep);
void astro_sleep_destroy(astro_sleep_bridge_t* bridge);

// Adenosine/sleep pressure
int astro_sleep_accumulate_adenosine(astro_sleep_bridge_t* bridge, float activity_level);
float astro_sleep_get_sleep_pressure(astro_sleep_bridge_t* bridge);
int astro_sleep_clear_adenosine(astro_sleep_bridge_t* bridge, float sleep_depth);

// Glymphatic system
int astro_sleep_enable_glymphatic(astro_sleep_bridge_t* bridge, bool is_sleeping);
float astro_sleep_get_clearance_rate(astro_sleep_bridge_t* bridge);
int astro_sleep_process_waste(astro_sleep_bridge_t* bridge, float waste_level);

// Synaptic renormalization
int astro_sleep_initiate_downscaling(astro_sleep_bridge_t* bridge);
float astro_sleep_get_renormalization_factor(astro_sleep_bridge_t* bridge);
```

**Biological Basis:**
- Astrocytes release adenosine → sleep pressure
- Glymphatic system (astrocyte-dependent) clears waste during sleep
- Astrocytes regulate synaptic downscaling during NREM

---

### 2.2 Microglia-Sleep Bridge

**Files to Create:**
- `include/glial/sleep/nimcp_microglia_sleep_bridge.h`
- `src/glial/sleep/nimcp_microglia_sleep_bridge.c`
- `test/unit/glial/sleep/test_microglia_sleep_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_microglia_t* microglia;
    sleep_system_t* sleep_system;

    // State
    microglia_sleep_config_t config;

    // Sleep functions
    float synaptic_pruning_rate;     // Sleep-dependent pruning
    float debris_phagocytosis;       // Enhanced during sleep
    float surveillance_mode;         // Reduced during sleep

    // Metrics
    float pruning_efficiency;
    float clearance_capacity;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} microglia_sleep_bridge_t;
```

**API Functions:**
```c
// Core lifecycle
microglia_sleep_bridge_t* microglia_sleep_create(
    const microglia_sleep_config_t* config,
    nimcp_microglia_t* microglia,
    sleep_system_t* sleep);
void microglia_sleep_destroy(microglia_sleep_bridge_t* bridge);

// Synaptic pruning
int microglia_sleep_enable_pruning_mode(microglia_sleep_bridge_t* bridge, sleep_stage_t stage);
int microglia_sleep_prune_weak_synapses(microglia_sleep_bridge_t* bridge, float threshold);
int microglia_sleep_protect_strong_synapses(microglia_sleep_bridge_t* bridge);

// Debris clearance
int microglia_sleep_enhance_phagocytosis(microglia_sleep_bridge_t* bridge, bool is_sleeping);
float microglia_sleep_get_clearance_rate(microglia_sleep_bridge_t* bridge);
```

**Biological Basis:**
- Microglia prune synapses during sleep (especially weak ones)
- Phagocytosis of debris enhanced during NREM
- Critical for memory consolidation (removing noise)

---

### 2.3 Synapse-Sleep Bridge

**Files to Create:**
- `include/plasticity/sleep/nimcp_synapse_sleep_bridge.h`
- `src/plasticity/sleep/nimcp_synapse_sleep_bridge.c`
- `test/unit/plasticity/sleep/test_synapse_sleep_bridge.cpp`

**Struct Definition:**
```c
typedef struct {
    // Core connections
    nimcp_synapse_t* synapse;
    sleep_system_t* sleep_system;
    synapse_plasticity_bridge_t* plasticity_bridge;

    // State
    synapse_sleep_config_t config;

    // Sleep-dependent plasticity
    float replay_potentiation;       // Memory replay during REM
    float homeostatic_downscaling;   // NREM downscaling
    float consolidation_tag;         // Tagged for protection

    // Metrics
    float pre_sleep_weight;
    float post_sleep_weight;
    float consolidation_strength;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} synapse_sleep_bridge_t;
```

**API Functions:**
```c
// Core lifecycle
synapse_sleep_bridge_t* synapse_sleep_create(
    const synapse_sleep_config_t* config,
    nimcp_synapse_t* synapse,
    sleep_system_t* sleep,
    synapse_plasticity_bridge_t* plasticity);
void synapse_sleep_destroy(synapse_sleep_bridge_t* bridge);

// Memory replay (REM)
int synapse_sleep_enable_replay_mode(synapse_sleep_bridge_t* bridge, bool is_rem);
int synapse_sleep_process_replay(synapse_sleep_bridge_t* bridge, float replay_strength);
int synapse_sleep_consolidate(synapse_sleep_bridge_t* bridge);

// Homeostatic downscaling (NREM)
int synapse_sleep_enable_downscaling(synapse_sleep_bridge_t* bridge, bool is_nrem);
int synapse_sleep_apply_downscaling(synapse_sleep_bridge_t* bridge, float factor);
int synapse_sleep_protect_tagged(synapse_sleep_bridge_t* bridge);

// Tagging/capture
int synapse_sleep_tag_for_consolidation(synapse_sleep_bridge_t* bridge);
bool synapse_sleep_is_tagged(synapse_sleep_bridge_t* bridge);
```

**Biological Basis:**
- REM: Memory replay strengthens important connections
- NREM: Homeostatic downscaling (SHY hypothesis)
- Synaptic tagging protects important memories from downscaling

---

### 2.4 Systems Consolidation-Sleep Integration

**Existing file to enhance:**
- `include/cognitive/memory/nimcp_systems_consolidation.h`
- `src/cognitive/memory/nimcp_systems_consolidation.c`

**Additions:**
```c
// Connect systems consolidation to sleep stages
int systems_consolidation_connect_sleep(
    systems_consolidation_t* sc,
    sleep_system_t* sleep);

// Stage-specific consolidation
int systems_consolidation_process_nrem(systems_consolidation_t* sc);  // Hippocampal replay
int systems_consolidation_process_rem(systems_consolidation_t* sc);   // Cortical integration

// Memory transfer (hippocampus → cortex)
int systems_consolidation_transfer_memories(systems_consolidation_t* sc, float sleep_quality);
```

---

### Phase 2 Summary

| Component | Files | Tests | Priority |
|-----------|-------|-------|----------|
| Astrocytes-Sleep Bridge | 2 | 30+ | HIGH |
| Microglia-Sleep Bridge | 2 | 25+ | HIGH |
| Synapse-Sleep Bridge | 2 | 35+ | HIGH |
| Systems Consolidation Enhancement | - | 15+ | MEDIUM |

**Total: 6 new files, ~105 tests**

---

## Phase 3: Bio-Async Activation

### Objective
Activate the 495+ defined bio-async module IDs by implementing startup sequencing, message handlers, and cross-module communication.

### 3.1 Bio Orchestrator Startup Implementation

**File to enhance:**
- `src/async/nimcp_bio_orchestrator.c`

**Current State:** Function `bio_orchestrator_execute_startup()` calls individual modules but ~450 modules defined in `nimcp_bio_messages.h` are never registered.

**Implementation:**
```c
int bio_orchestrator_execute_startup(bio_orchestrator_t* orch) {
    if (!orch) return -1;

    // Phase 1: Core modules
    bio_orchestrator_start_core_modules(orch);     // Oscillations, Cortical, etc.

    // Phase 2: Plasticity modules
    bio_orchestrator_start_plasticity_modules(orch);  // All 17 plasticity mechanisms

    // Phase 3: Cognitive modules
    bio_orchestrator_start_cognitive_modules(orch);   // Memory, Attention, etc.

    // Phase 4: Integration bridges
    bio_orchestrator_start_bridge_modules(orch);      // All immune, FEP, training bridges

    // Phase 5: Glial modules
    bio_orchestrator_start_glial_modules(orch);       // Astrocytes, Microglia, etc.

    // Phase 6: LNN/SNN modules
    bio_orchestrator_start_neural_modules(orch);      // LNN, SNN

    return 0;
}

// Helper for each category
static int bio_orchestrator_start_plasticity_modules(bio_orchestrator_t* orch) {
    // Register and start each plasticity module
    for (int i = 0; i < NUM_PLASTICITY_MECHANISMS; i++) {
        bio_module_info_t info = get_plasticity_module_info(i);
        bio_router_register_module(&info);
    }
    return 0;
}
```

---

### 3.2 Immune Bridge Bio-Async Registration

**Total: 71 immune bridges need bio-async registration**

**Pattern to apply to each bridge:**
```c
// In each immune bridge's connect_bio_async function:
int xxx_immune_connect_bio_async(xxx_immune_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (bridge->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_XXX,  // From nimcp_bio_messages.h
        .module_name = "xxx_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge,
        .message_handler = xxx_immune_message_handler  // NEW: Add handler
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (bridge->bio_ctx) {
        bridge->bio_async_enabled = true;
    }
    return 0;
}

// Message handler for receiving async messages
static int xxx_immune_message_handler(bio_message_t* msg, void* user_data) {
    xxx_immune_bridge_t* bridge = (xxx_immune_bridge_t*)user_data;

    switch (msg->type) {
        case BIO_MSG_CYTOKINE_UPDATE:
            return xxx_immune_process_cytokine(bridge, msg);
        case BIO_MSG_INFLAMMATION_CHANGE:
            return xxx_immune_process_inflammation(bridge, msg);
        // ... other message types
    }
    return 0;
}
```

---

### 3.3 Cross-Module Message Types

**Add to nimcp_bio_messages.h:**
```c
// Immune-related message types
typedef enum {
    BIO_MSG_CYTOKINE_UPDATE = 0x1000,
    BIO_MSG_INFLAMMATION_CHANGE,
    BIO_MSG_ANTIGEN_DETECTED,
    BIO_MSG_IMMUNE_RESPONSE_STARTED,
    BIO_MSG_IMMUNE_RESPONSE_COMPLETE,

    // Plasticity messages
    BIO_MSG_WEIGHT_CHANGE = 0x2000,
    BIO_MSG_LTP_INDUCED,
    BIO_MSG_LTD_INDUCED,
    BIO_MSG_METAPLASTICITY_SHIFT,

    // Sleep messages
    BIO_MSG_SLEEP_STAGE_CHANGE = 0x3000,
    BIO_MSG_CONSOLIDATION_START,
    BIO_MSG_CONSOLIDATION_COMPLETE,
    BIO_MSG_REPLAY_EVENT,

    // Glial messages
    BIO_MSG_GLIOTRANSMITTER_RELEASE = 0x4000,
    BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
    BIO_MSG_MICROGLIA_ACTIVATION,
    BIO_MSG_MYELIN_DAMAGE,

    // FEP messages
    BIO_MSG_FREE_ENERGY_UPDATE = 0x5000,
    BIO_MSG_PREDICTION_ERROR,
    BIO_MSG_BELIEF_UPDATE,
} bio_message_type_t;
```

---

### 3.4 Module Registration Sweep

**Files requiring bio-async activation (categorized):**

| Category | Count | Status |
|----------|-------|--------|
| Immune Bridges (cognitive) | 24 | Need handlers |
| Immune Bridges (perception) | 3 | Need handlers |
| Immune Bridges (plasticity) | 17 | Need handlers |
| Immune Bridges (middleware) | 6 | Need handlers |
| Immune Bridges (core) | 3 | Need handlers |
| FEP Bridges | 12 | Need registration |
| Training Bridges | 8 | Need handlers |
| Sleep Bridges | 6 | Need registration |
| Substrate Bridges | 0 | Phase 4 |
| Pink Noise Bridges | 12 | Partially done |

---

### Phase 3 Summary

| Component | Files to Modify | New Handlers | Priority |
|-----------|----------------|--------------|----------|
| Bio Orchestrator Startup | 1 | N/A | CRITICAL |
| Immune Bridge Handlers | 53 | 53 | HIGH |
| FEP Bridge Registration | 12 | 12 | MEDIUM |
| Training Bridge Handlers | 8 | 8 | MEDIUM |
| Message Type Definitions | 1 | N/A | HIGH |

**Total: ~74 files modified, 73 new message handlers**

---

## Phase 4: Substrate & Pink Noise Completion

### Objective
Add substrate bridges to all 17 plasticity mechanisms and complete remaining pink noise integrations.

### 4.1 Plasticity Substrate Bridges

Each plasticity mechanism needs connection to neural substrate (ATP, temperature, membrane state).

**Files to Create (15 missing):**

| Mechanism | Header | Source | Tests |
|-----------|--------|--------|-------|
| BCM | `nimcp_bcm_substrate_bridge.h` | `.c` | 25 |
| Homeostatic | `nimcp_homeostatic_substrate_bridge.h` | `.c` | 25 |
| Metaplasticity | `nimcp_metaplasticity_substrate_bridge.h` | `.c` | 25 |
| Eligibility | `nimcp_eligibility_substrate_bridge.h` | `.c` | 20 |
| Heterosynaptic | `nimcp_heterosynaptic_substrate_bridge.h` | `.c` | 20 |
| Synaptic Scaling | `nimcp_synaptic_scaling_substrate_bridge.h` | `.c` | 20 |
| Synaptic Tagging | `nimcp_synaptic_tagging_substrate_bridge.h` | `.c` | 25 |
| Calcium Dynamics | `nimcp_calcium_dynamics_substrate_bridge.h` | `.c` | 30 |
| Neuromodulator | `nimcp_neuromodulator_substrate_bridge.h` | `.c` | 25 |
| Structural | `nimcp_structural_substrate_bridge.h` | `.c` | 20 |
| Gliotransmission | `nimcp_gliotransmission_substrate_bridge.h` | `.c` | 20 |
| SFA | `nimcp_sfa_substrate_bridge.h` | `.c` | 20 |
| Intrinsic Excitability | `nimcp_intrinsic_substrate_bridge.h` | `.c` | 20 |
| Dendritic | `nimcp_dendritic_substrate_bridge.h` | `.c` | 25 |
| Metabolic | `nimcp_metabolic_substrate_bridge.h` | `.c` | 25 |

**Common Pattern:**
```c
typedef struct {
    // Connections
    xxx_state_t* mechanism;
    neural_substrate_t* substrate;

    // Configuration
    xxx_substrate_config_t config;

    // Substrate effects
    float atp_modulation;      // Low ATP → reduced plasticity
    float temperature_scaling; // Temp affects rate constants
    float membrane_factor;     // Membrane state effects
    float ion_concentration_effect;

    // Threading
    nimcp_mutex_t* mutex;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
} xxx_substrate_bridge_t;

// Standard API
xxx_substrate_bridge_t* xxx_substrate_create(config, mechanism, substrate);
void xxx_substrate_destroy(bridge);
int xxx_substrate_connect_bio_async(bridge);
int xxx_substrate_update(bridge);
float xxx_substrate_get_atp_modulation(bridge);
float xxx_substrate_get_temperature_scaling(bridge);
```

---

### 4.2 Missing Pink Noise Bridges

**Currently implemented (12):**
- STDP, STP, Calcium, Dendritic, Heterosynaptic
- Metabolic, Spatial Neuromod, Vesicle Packaging
- Oscillations, Population Coding
- Ensemble Uncertainty, Systems Consolidation

**Missing (9):**

| Module | Header | Source | Tests |
|--------|--------|--------|-------|
| BCM | `nimcp_bcm_pink_noise_bridge.h` | `.c` | 20 |
| Homeostatic | `nimcp_homeostatic_pink_noise_bridge.h` | `.c` | 20 |
| Metaplasticity | `nimcp_metaplasticity_pink_noise_bridge.h` | `.c` | 20 |
| Eligibility | `nimcp_eligibility_pink_noise_bridge.h` | `.c` | 20 |
| Synaptic Scaling | `nimcp_synaptic_scaling_pink_noise_bridge.h` | `.c` | 20 |
| Synaptic Tagging | `nimcp_synaptic_tagging_pink_noise_bridge.h` | `.c` | 20 |
| Structural | `nimcp_structural_pink_noise_bridge.h` | `.c` | 20 |
| Gliotransmission | `nimcp_gliotransmission_pink_noise_bridge.h` | `.c` | 20 |
| SFA | `nimcp_sfa_pink_noise_bridge.h` | `.c` | 20 |

---

### 4.3 LNN Training Adapter Completion

**File to enhance:**
- `include/lnn/nimcp_lnn_training.h`
- `src/lnn/nimcp_lnn_training.c`

**Missing functionality:**
```c
// Connect LNN to training pipeline
int lnn_training_connect_pipeline(lnn_training_t* t, training_pipeline_t* pipeline);
int lnn_training_connect_optimizer(lnn_training_t* t, nimcp_optimizer_t* opt);
int lnn_training_connect_gradient_manager(lnn_training_t* t, gradient_manager_t* gm);

// Connect LNN to immune system
int lnn_training_connect_immune(lnn_training_t* t, training_immune_system_t* immune);

// Connect LNN to plasticity
int lnn_training_connect_plasticity(lnn_training_t* t, plasticity_orchestrator_t* orch);
```

---

### Phase 4 Summary

| Component | Files | Tests | Priority |
|-----------|-------|-------|----------|
| Plasticity Substrate Bridges | 30 | 345 | MEDIUM |
| Pink Noise Bridges | 18 | 180 | LOW |
| LNN Training Adapter | 2 | 40 | MEDIUM |

**Total: 50 new files, ~565 tests**

---

## Implementation Order & Dependencies

```
Phase 1 (CRITICAL)
│
├── 1.1 Dendrite-Plasticity Bridge ◄── No dependencies
├── 1.2 Synapse-Plasticity Bridge ◄── Depends on Dendrite bridge
├── 1.3 Axon-Plasticity Bridge ◄── No dependencies
│
├── 1.4 Oligodendrocytes-Immune ◄── Needs brain_immune_system
├── 1.5 Myelin-Immune ◄── Needs oligodendrocytes bridge
├── 1.6 Astrocytes-Immune ◄── Needs brain_immune_system
└── 1.7 Microglia Enhancement ◄── Needs oligo + astro bridges
    │
    ▼
Phase 2 (HIGH)
│
├── 2.1 Astrocytes-Sleep ◄── Needs astro-immune from Phase 1
├── 2.2 Microglia-Sleep ◄── Needs microglia enhancement
├── 2.3 Synapse-Sleep ◄── Needs synapse-plasticity from Phase 1
└── 2.4 Systems Consolidation ◄── Needs all sleep bridges
    │
    ▼
Phase 3 (HIGH)
│
├── 3.1 Bio Orchestrator ◄── No dependencies
├── 3.2 Immune Bridge Handlers ◄── Needs Phase 1 glial-immune
├── 3.3 Message Type Definitions ◄── No dependencies
└── 3.4 Cross-module Registration ◄── Needs orchestrator
    │
    ▼
Phase 4 (MEDIUM)
│
├── 4.1 Substrate Bridges ◄── Can parallelize
├── 4.2 Pink Noise Bridges ◄── Can parallelize
└── 4.3 LNN Training ◄── Needs orchestrator from Phase 3
```

---

## Test Strategy

### Unit Tests (Per Bridge)
Each bridge requires:
1. Creation/destruction lifecycle
2. Configuration validation
3. Connection to target modules
4. Update function correctness
5. Bio-async registration/handler
6. Thread safety
7. Error handling

### Integration Tests (Cross-Bridge)
1. Dendrite → Synapse → Axon plasticity propagation
2. Glial → Immune → Plasticity cascade
3. Sleep → Consolidation → Memory integration
4. Bio-async message flow verification
5. Full learning loop with all bridges active

### E2E Tests
1. Complete learning episode with all systems
2. Sleep cycle with consolidation
3. Immune response cascade
4. Pink noise + substrate modulation
5. Full system startup/shutdown sequence

---

## File Count Summary

| Phase | New Headers | New Sources | New Tests | Modified Files |
|-------|-------------|-------------|-----------|----------------|
| 1 | 6 | 6 | 6 | 1 |
| 2 | 3 | 3 | 3 | 2 |
| 3 | 0 | 0 | 0 | 74 |
| 4 | 24 | 24 | 24 | 2 |
| **Total** | **33** | **33** | **33** | **79** |

**Total new files: 99**
**Total modified files: 79**
**Estimated new tests: ~850**

---

## Success Criteria

### Phase 1 Complete When:
- [ ] All 3 neural component bridges compile and pass tests
- [ ] All 4 glial-immune bridges compile and pass tests
- [ ] Integration test: spike → dendrite → synapse → axon plasticity works
- [ ] Integration test: inflammation → glial → plasticity cascade works

### Phase 2 Complete When:
- [ ] All 3 glial-sleep bridges compile and pass tests
- [ ] Synapse-sleep bridge integrates with Phase 1 synapse bridge
- [ ] Integration test: sleep stage → glial → consolidation works
- [ ] Systems consolidation processes hippocampal replays

### Phase 3 Complete When:
- [ ] Bio orchestrator starts all modules in correct order
- [ ] All 71 immune bridges have message handlers
- [ ] Cross-module messages are delivered correctly
- [ ] System can start up and shut down cleanly

### Phase 4 Complete When:
- [ ] All 15 substrate bridges compile and pass tests
- [ ] All 9 pink noise bridges compile and pass tests
- [ ] LNN connects to training pipeline
- [ ] Full system runs with all modulations active

---

## Final Notes

This plan represents approximately **850 new tests** and **178 file modifications** to achieve a fully wired NIMCP system. The phases are ordered by criticality and dependency, with Phase 1 being absolutely essential for basic learning functionality.

After completing all phases, the NIMCP system will have:
- Complete dendrite → synapse → axon plasticity pipeline
- Bidirectional glial ↔ immune integration
- Sleep-dependent consolidation with glial support
- Full bio-async messaging between all modules
- Neural substrate affecting all plasticity mechanisms
- Pink noise biological variability across all systems
