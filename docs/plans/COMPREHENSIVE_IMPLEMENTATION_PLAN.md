# NIMCP Comprehensive Implementation Plan
## Complete Feature Implementation with Full Integration

**Version**: 1.1
**Date**: 2026-01-09
**Status**: Planning Document
**Scope**: All incomplete features from Master Schedule, Advanced Concepts, and Brain Regions

---

## Executive Summary

This plan implements **ALL remaining incomplete features** across NIMCP with:
- **Bio-async messaging** for all inter-module communication
- **Bridge architecture** connecting every feature to the cognitive ecosystem
- **Immune system integration** for threat detection and self-healing
- **Brain factory initialization** for proper lifecycle management
- **Comprehensive logging** at all levels
- **Complete test coverage** (unit, integration, regression, e2e)
- **Intra-layer integration** for modules within the same architectural tier
- **Inter-layer integration** for modules across different architectural tiers

### Scope Summary

| Category | Items | Est. LOC | Tests |
|----------|-------|----------|-------|
| Advanced Concepts (Incomplete) | 9 concepts | ~45,000 | ~900 |
| Brain Regions (New) | 17 regions | ~85,000 | ~1,700 |
| Extrapolation (Remaining 15%) | 3 modules | ~4,500 | ~90 |
| Embodiment (Remaining 40%) | 4 modules | ~2,800 | ~56 |
| Superhuman (Remaining 50%) | 7 modules | ~15,000 | ~300 |
| Integration Layers (All) | 40 modules | ~304,000 | ~3,280 |
| GPU Module Integration | 70 headers | ~35,000 | ~300 |
| Information Theory Integration | 4 headers | ~18,000 | ~160 |
| Plasticity Module Integration | 100+ headers | ~40,000 | ~350 |
| Security Module Integration | 44 headers | ~45,000 | ~400 |
| **TOTAL** | **~40 modules + full integration** | **~594,300** | **~7,536** |

---

## Architecture Requirements

### 1. Bio-Async Integration Pattern

Every new module MUST implement:

```c
// Required bio-async message handlers
typedef struct {
    nimcp_bio_async_handler_t msg_handler;
    nimcp_bio_async_subscription_t* subscriptions;
    uint32_t subscription_count;
    nimcp_bio_router_t* router_ref;
} module_bio_async_t;

// Standard message types per module
typedef enum {
    MSG_TYPE_MODULE_STATE_UPDATE,
    MSG_TYPE_MODULE_REQUEST,
    MSG_TYPE_MODULE_RESPONSE,
    MSG_TYPE_MODULE_ERROR,
    MSG_TYPE_MODULE_METRICS
} module_msg_type_t;
```

### 2. Bridge Architecture Pattern

Every module requires these bridges:

| Bridge Type | Purpose | File Pattern |
|-------------|---------|--------------|
| SNN Bridge | Spiking neural network integration | `nimcp_<module>_snn_bridge.h/c` |
| Plasticity Bridge | Learning/adaptation | `nimcp_<module>_plasticity_bridge.h/c` |
| FEP Bridge | Free energy principle integration | `nimcp_<module>_fep_bridge.h/c` |
| Substrate Bridge | Bio-async messaging | `nimcp_<module>_substrate_bridge.h/c` |
| Thalamic Bridge | Attention gating | `nimcp_<module>_thalamic_bridge.h/c` |
| Immune Bridge | Threat detection/healing | `nimcp_<module>_immune_bridge.h/c` |
| Hub Bridge | Cognitive integration hub | `nimcp_<module>_hub_bridge.h/c` |

### 3. Immune System Integration Pattern

```c
// Every module must register with immune system
typedef struct {
    nimcp_immune_sensor_t sensor;
    nimcp_antibody_receptor_t receptor;
    nimcp_threat_callback_t on_threat;
    nimcp_healing_callback_t on_heal;
} module_immune_integration_t;

nimcp_status_t module_immune_register(
    module_t* module,
    nimcp_brain_immune_t* immune_system);
```

### 4. Brain Factory Integration Pattern

```c
// Every module must be initializable via brain factory
nimcp_status_t nimcp_brain_init_<module>(
    nimcp_brain_t* brain,
    const nimcp_brain_config_t* config);

// Factory registration
NIMCP_BRAIN_FACTORY_REGISTER(<module>, nimcp_brain_init_<module>);
```

### 5. Logging Integration Pattern

```c
// Standard logging macros for all modules
#define MODULE_LOG_TRACE(module, fmt, ...) \
    NIMCP_LOG_TRACE("[%s] " fmt, module->name, ##__VA_ARGS__)
#define MODULE_LOG_DEBUG(module, fmt, ...) \
    NIMCP_LOG_DEBUG("[%s] " fmt, module->name, ##__VA_ARGS__)
#define MODULE_LOG_INFO(module, fmt, ...) \
    NIMCP_LOG_INFO("[%s] " fmt, module->name, ##__VA_ARGS__)
#define MODULE_LOG_WARN(module, fmt, ...) \
    NIMCP_LOG_WARN("[%s] " fmt, module->name, ##__VA_ARGS__)
#define MODULE_LOG_ERROR(module, fmt, ...) \
    NIMCP_LOG_ERROR("[%s] " fmt, module->name, ##__VA_ARGS__)

// Metrics logging
#define MODULE_LOG_METRIC(module, metric, value) \
    nimcp_metrics_record(module->metrics, metric, value)
```

### 6. Intra-Layer Integration Pattern

**CRITICAL**: All modules within the same architectural layer MUST communicate bidirectionally.

#### 6.1 Layer Definitions

| Layer | Modules | Integration Type |
|-------|---------|-----------------|
| **Physics Layer** | Ephaptic, Info Geometry, HH Dynamics, Thermodynamics | Physical coupling |
| **Chemistry Layer** | pH, NO Signaling, Neurovascular | Chemical signaling |
| **Biology Layer** | Epigenetics, Neurogenesis, Gene Expression | Biological cascades |
| **Neuromodulatory Layer** | LC, VTA, Raphe, Habenula | Neuromodulator cross-talk |
| **Memory Layer** | Entorhinal, Perirhinal, Parahippocampal, Mammillary | Memory circuit |
| **Sensory Layer** | Somatosensory, Olfactory, Gustatory | Multi-sensory integration |
| **Executive Layer** | OFC, Retrosplenial, PFC | Executive coordination |
| **Integration Layer** | Claustrum, PAG, Red Nucleus, Reticular | Global binding |
| **Superhuman Layer** | Eagle Vision, Echolocation, Time Dilation, etc. | Enhanced perception |

#### 6.2 Intra-Layer Bridge Pattern

```c
/**
 * Every module must implement intra-layer bridges to ALL sibling modules
 * within its architectural layer.
 */

// Intra-layer integration structure
typedef struct {
    // Sibling module references (same layer)
    void** sibling_modules;
    uint32_t num_siblings;

    // Intra-layer message types
    nimcp_msg_type_t* intra_msg_types;

    // Bidirectional channels
    nimcp_bio_channel_t** intra_channels;

    // Synchronization
    nimcp_sync_barrier_t layer_barrier;
    float layer_coherence;  // 0-1 synchronization level

    // Layer-specific coordinator
    nimcp_layer_coordinator_t* coordinator;
} module_intra_layer_t;

// Required intra-layer API
nimcp_status_t module_intra_layer_init(
    module_t* module,
    nimcp_layer_t* layer);

nimcp_status_t module_intra_layer_sync(
    module_t* module,
    float* coherence);

nimcp_status_t module_intra_layer_broadcast(
    module_t* module,
    nimcp_msg_t* msg);

nimcp_status_t module_intra_layer_receive(
    module_t* module,
    nimcp_msg_t** msgs,
    uint32_t* count);
```

#### 6.3 Intra-Layer Integration Matrix

**Neuromodulatory Layer Example**:
```
              LC      VTA     Raphe   Habenula
    LC        -       ↔       ↔       ↔
    VTA       ↔       -       ↔       ←
    Raphe     ↔       ↔       -       ↔
    Habenula  ↔       →       ↔       -

Legend: ↔ = bidirectional, → = sends to, ← = receives from
```

**Memory Layer Example**:
```
              EC      PRC     PHC     MB
    EC        -       ↔       ↔       ↔
    PRC       ↔       -       ↔       ↔
    PHC       ↔       ↔       -       ↔
    MB        ↔       ↔       ↔       -

All connections bidirectional for memory circuit coherence
```

#### 6.4 Intra-Layer Bridge Files

For each layer, create:
- `include/integration/intra/<layer>/nimcp_<layer>_intra_coordinator.h`
- `src/integration/intra/<layer>/nimcp_<layer>_intra_coordinator.c`
- Individual cross-module bridges within the layer

**Example - Neuromodulatory Layer**:
```
include/integration/intra/neuromodulatory/
├── nimcp_neuromod_intra_coordinator.h
├── nimcp_lc_vta_bridge.h
├── nimcp_lc_raphe_bridge.h
├── nimcp_lc_habenula_bridge.h
├── nimcp_vta_raphe_bridge.h
├── nimcp_vta_habenula_bridge.h
└── nimcp_raphe_habenula_bridge.h

src/integration/intra/neuromodulatory/
├── nimcp_neuromod_intra_coordinator.c
├── nimcp_lc_vta_bridge.c
├── nimcp_lc_raphe_bridge.c
├── nimcp_lc_habenula_bridge.c
├── nimcp_vta_raphe_bridge.c
├── nimcp_vta_habenula_bridge.c
└── nimcp_raphe_habenula_bridge.c
```

### 7. Inter-Layer Integration Pattern

**CRITICAL**: All layers MUST communicate with adjacent and non-adjacent layers via defined pathways.

#### 7.1 Layer Hierarchy and Connections

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         INTER-LAYER INTEGRATION MAP                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐                                                           │
│  │ SUPERHUMAN   │ ←──────────────────────────────────────────┐              │
│  │    LAYER     │                                             │              │
│  └──────┬───────┘                                             │              │
│         │ Enhanced perception feeds                           │              │
│         ↓                                                     │              │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐  │              │
│  │ INTEGRATION  │ ←→  │  EXECUTIVE   │ ←→  │   MEMORY     │  │              │
│  │    LAYER     │     │    LAYER     │     │    LAYER     │  │              │
│  └──────┬───────┘     └──────┬───────┘     └──────┬───────┘  │              │
│         │                    │                    │           │              │
│         └────────────────────┼────────────────────┘           │              │
│                              │                                │              │
│                              ↓                                │              │
│  ┌──────────────┐     ┌──────────────┐                       │              │
│  │   SENSORY    │ ←→  │ NEUROMOD     │ ←─────────────────────┘              │
│  │    LAYER     │     │    LAYER     │                                       │
│  └──────┬───────┘     └──────┬───────┘                                       │
│         │                    │                                               │
│         └─────────┬──────────┘                                               │
│                   │                                                          │
│                   ↓                                                          │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐                 │
│  │   BIOLOGY    │ ←→  │  CHEMISTRY   │ ←→  │   PHYSICS    │                 │
│  │    LAYER     │     │    LAYER     │     │    LAYER     │                 │
│  └──────────────┘     └──────────────┘     └──────────────┘                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 7.2 Inter-Layer Bridge Pattern

```c
/**
 * Every module must implement inter-layer bridges to modules in
 * connected layers (both adjacent and non-adjacent as defined).
 */

// Inter-layer integration structure
typedef struct {
    // Layer identification
    nimcp_layer_id_t own_layer;
    nimcp_layer_id_t* connected_layers;
    uint32_t num_connected_layers;

    // Inter-layer channels (one per connected layer)
    nimcp_inter_layer_channel_t** channels;

    // Hierarchical routing
    nimcp_layer_router_t* layer_router;

    // Bottom-up signals (lower → higher layers)
    nimcp_signal_buffer_t bottom_up_buffer;

    // Top-down signals (higher → lower layers)
    nimcp_signal_buffer_t top_down_buffer;

    // Cross-layer synchronization
    float inter_layer_coherence;
    nimcp_phase_lock_t* phase_locks;
} module_inter_layer_t;

// Required inter-layer API
nimcp_status_t module_inter_layer_init(
    module_t* module,
    nimcp_layer_registry_t* registry);

nimcp_status_t module_inter_layer_send_bottom_up(
    module_t* module,
    nimcp_layer_id_t target_layer,
    nimcp_msg_t* msg);

nimcp_status_t module_inter_layer_send_top_down(
    module_t* module,
    nimcp_layer_id_t target_layer,
    nimcp_msg_t* msg);

nimcp_status_t module_inter_layer_receive(
    module_t* module,
    nimcp_layer_id_t source_layer,
    nimcp_msg_t** msgs,
    uint32_t* count);

nimcp_status_t module_inter_layer_sync_phase(
    module_t* module,
    nimcp_layer_id_t target_layer,
    float* phase_diff);
```

#### 7.3 Inter-Layer Connection Definitions

| Source Layer | Target Layer | Connection Type | Purpose |
|--------------|--------------|-----------------|---------|
| Physics | Chemistry | Bottom-up | Physical constraints on chemistry |
| Chemistry | Biology | Bottom-up | Chemical signals trigger biology |
| Biology | Neuromodulatory | Bottom-up | Gene expression affects neuromodulators |
| Neuromodulatory | Sensory | Bidirectional | Gain modulation |
| Neuromodulatory | Memory | Bidirectional | Plasticity modulation |
| Neuromodulatory | Executive | Bidirectional | Decision modulation |
| Sensory | Memory | Bottom-up | Sensory encoding |
| Sensory | Executive | Bottom-up | Sensory-driven decisions |
| Memory | Executive | Bidirectional | Memory-guided decisions |
| Memory | Integration | Bottom-up | Memory binding |
| Executive | Integration | Bidirectional | Executive control |
| Integration | Superhuman | Bidirectional | Enhanced integration |
| Superhuman | Neuromodulatory | Top-down | Enhanced perception modulates arousal |

#### 7.4 Inter-Layer Bridge Files

```
include/integration/inter/
├── nimcp_inter_layer_router.h          # Central inter-layer routing
├── nimcp_layer_registry.h              # Layer registration
├── nimcp_bottom_up_pathway.h           # Bottom-up signal handling
├── nimcp_top_down_pathway.h            # Top-down signal handling
│
├── physics_chemistry/
│   └── nimcp_physics_chemistry_bridge.h
├── chemistry_biology/
│   └── nimcp_chemistry_biology_bridge.h
├── biology_neuromod/
│   └── nimcp_biology_neuromod_bridge.h
├── neuromod_sensory/
│   └── nimcp_neuromod_sensory_bridge.h
├── neuromod_memory/
│   └── nimcp_neuromod_memory_bridge.h
├── neuromod_executive/
│   └── nimcp_neuromod_executive_bridge.h
├── sensory_memory/
│   └── nimcp_sensory_memory_bridge.h
├── sensory_executive/
│   └── nimcp_sensory_executive_bridge.h
├── memory_executive/
│   └── nimcp_memory_executive_bridge.h
├── memory_integration/
│   └── nimcp_memory_integration_bridge.h
├── executive_integration/
│   └── nimcp_executive_integration_bridge.h
├── integration_superhuman/
│   └── nimcp_integration_superhuman_bridge.h
└── superhuman_neuromod/
    └── nimcp_superhuman_neuromod_bridge.h

src/integration/inter/
├── nimcp_inter_layer_router.c
├── nimcp_layer_registry.c
├── nimcp_bottom_up_pathway.c
├── nimcp_top_down_pathway.c
└── [corresponding .c files for each bridge]
```

### 8. Layer Coordinator System

#### 8.1 Central Layer Coordinator

```c
/**
 * The Layer Coordinator manages all intra-layer and inter-layer
 * communication, ensuring coherent operation across the entire system.
 */

typedef struct {
    // All registered layers
    nimcp_layer_t* layers[NIMCP_MAX_LAYERS];
    uint32_t num_layers;

    // Inter-layer routing table
    nimcp_routing_table_t inter_layer_routes;

    // Global synchronization
    nimcp_global_sync_t global_sync;
    float global_coherence;

    // Bio-async integration
    nimcp_bio_router_t* bio_router;

    // Immune integration
    nimcp_brain_immune_t* immune_system;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_layer_coordinator_t;

// Layer Coordinator API
nimcp_status_t nimcp_layer_coordinator_init(
    nimcp_layer_coordinator_t* coord,
    nimcp_brain_t* brain);

nimcp_status_t nimcp_layer_coordinator_register_layer(
    nimcp_layer_coordinator_t* coord,
    nimcp_layer_t* layer);

nimcp_status_t nimcp_layer_coordinator_register_module(
    nimcp_layer_coordinator_t* coord,
    nimcp_layer_id_t layer_id,
    void* module,
    nimcp_module_interface_t* interface);

nimcp_status_t nimcp_layer_coordinator_update(
    nimcp_layer_coordinator_t* coord,
    float dt);

nimcp_status_t nimcp_layer_coordinator_route_message(
    nimcp_layer_coordinator_t* coord,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    nimcp_msg_t* msg);

float nimcp_layer_coordinator_get_coherence(
    nimcp_layer_coordinator_t* coord);
```

#### 8.2 Brain Factory Integration for Layer Coordinator

```c
// In nimcp_brain_init.c
nimcp_status_t nimcp_brain_init_layer_system(nimcp_brain_t* brain) {
    NIMCP_LOG_INFO("Initializing layer coordination system");

    // Initialize central coordinator
    nimcp_layer_coordinator_init(&brain->layer_coordinator, brain);

    // Register all layers
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->physics_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->chemistry_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->biology_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->neuromod_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->sensory_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->memory_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->executive_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->integration_layer);
    nimcp_layer_coordinator_register_layer(&brain->layer_coordinator,
                                            &brain->superhuman_layer);

    // Initialize inter-layer bridges
    nimcp_inter_layer_bridges_init(brain);

    // Initialize intra-layer bridges for each layer
    nimcp_intra_layer_bridges_init(brain);

    NIMCP_LOG_INFO("Layer system initialized with %d layers, %d inter-layer bridges",
                   brain->layer_coordinator.num_layers,
                   brain->layer_coordinator.inter_layer_routes.num_routes);

    return NIMCP_OK;
}
```

### 9. Integration Test Requirements

#### 9.1 Intra-Layer Integration Tests

Every layer MUST have comprehensive intra-layer integration tests:

| Layer | Test File | Test Count |
|-------|-----------|------------|
| Physics | `test/integration/intra/physics/test_physics_intra_integration.cpp` | 30 |
| Chemistry | `test/integration/intra/chemistry/test_chemistry_intra_integration.cpp` | 25 |
| Biology | `test/integration/intra/biology/test_biology_intra_integration.cpp` | 25 |
| Neuromodulatory | `test/integration/intra/neuromod/test_neuromod_intra_integration.cpp` | 40 |
| Sensory | `test/integration/intra/sensory/test_sensory_intra_integration.cpp` | 30 |
| Memory | `test/integration/intra/memory/test_memory_intra_integration.cpp` | 40 |
| Executive | `test/integration/intra/executive/test_executive_intra_integration.cpp` | 30 |
| Integration | `test/integration/intra/integration/test_integration_intra_integration.cpp` | 40 |
| Superhuman | `test/integration/intra/superhuman/test_superhuman_intra_integration.cpp` | 35 |
| **Total** | | **295** |

#### 9.2 Inter-Layer Integration Tests

Every inter-layer connection MUST have integration tests:

| Connection | Test File | Test Count |
|------------|-----------|------------|
| Physics↔Chemistry | `test/integration/inter/test_physics_chemistry_integration.cpp` | 20 |
| Chemistry↔Biology | `test/integration/inter/test_chemistry_biology_integration.cpp` | 20 |
| Biology↔Neuromod | `test/integration/inter/test_biology_neuromod_integration.cpp` | 20 |
| Neuromod↔Sensory | `test/integration/inter/test_neuromod_sensory_integration.cpp` | 25 |
| Neuromod↔Memory | `test/integration/inter/test_neuromod_memory_integration.cpp` | 25 |
| Neuromod↔Executive | `test/integration/inter/test_neuromod_executive_integration.cpp` | 25 |
| Sensory↔Memory | `test/integration/inter/test_sensory_memory_integration.cpp` | 25 |
| Sensory↔Executive | `test/integration/inter/test_sensory_executive_integration.cpp` | 20 |
| Memory↔Executive | `test/integration/inter/test_memory_executive_integration.cpp` | 30 |
| Memory↔Integration | `test/integration/inter/test_memory_integration_layer_integration.cpp` | 25 |
| Executive↔Integration | `test/integration/inter/test_executive_integration_layer_integration.cpp` | 25 |
| Integration↔Superhuman | `test/integration/inter/test_integration_superhuman_integration.cpp` | 20 |
| Superhuman↔Neuromod | `test/integration/inter/test_superhuman_neuromod_integration.cpp` | 20 |
| **Total** | | **300** |

#### 9.3 Full-Stack Integration Tests

End-to-end tests that verify complete layer traversal:

| Test | Description | Test Count |
|------|-------------|------------|
| Bottom-Up Full Stack | Physics → Integration | 15 |
| Top-Down Full Stack | Executive → Physics | 15 |
| Circular Integration | Full loop through all layers | 10 |
| Layer Coherence | Global synchronization | 10 |
| **Total** | | **50** |

### 10. Cognitive Layer Integration

**CRITICAL**: All new modules MUST integrate with the Cognitive Integration Hub for event-driven communication.

#### 10.1 Cognitive Hub Registration

Every module must register with `cognitive_integration_hub`:

```c
// Required cognitive hub integration
nimcp_status_t module_cognitive_hub_init(
    module_t* module,
    cognitive_integration_hub_t hub) {

    // Register module with hub
    cognitive_hub_register_module(hub,
                                   module->id,
                                   module->category,
                                   module->name,
                                   module);

    // Subscribe to relevant events
    cognitive_hub_subscribe(hub, module->id,
                            COGNITIVE_EVENT_ATTENTION_SHIFT,
                            module_on_attention_shift, module);
    cognitive_hub_subscribe(hub, module->id,
                            COGNITIVE_EVENT_MEMORY_CONSOLIDATION,
                            module_on_memory_consolidation, module);
    cognitive_hub_subscribe(hub, module->id,
                            COGNITIVE_EVENT_EXECUTIVE_DECISION,
                            module_on_executive_decision, module);

    // Register query handler
    cognitive_hub_register_query_handler(hub, module->id,
                                          module_query_handler);

    return NIMCP_OK;
}
```

#### 10.2 Cognitive Categories for New Modules

| Module | Cognitive Category | Events Subscribed | Events Published |
|--------|-------------------|-------------------|------------------|
| LC | COGNITIVE_CATEGORY_NEUROMOD | Attention, Stress | Arousal, Novelty |
| VTA | COGNITIVE_CATEGORY_REWARD | Action, Outcome | RPE, Motivation |
| Raphe | COGNITIVE_CATEGORY_NEUROMOD | Stress, Circadian | Mood, Patience |
| Habenula | COGNITIVE_CATEGORY_REWARD | Outcome, Expectation | Disappointment, Avoidance |
| Entorhinal | COGNITIVE_CATEGORY_MEMORY | Position, Sensory | Grid, Border |
| Claustrum | COGNITIVE_CATEGORY_INTEGRATION | All Categories | Binding, Salience |
| All Physics | COGNITIVE_CATEGORY_SUBSTRATE | State Changes | Biophysical Updates |

### 11. Omnidirectional Module Integration

**CRITICAL**: All new modules MUST integrate with the Omnidirectional inference system (JEPA bidirectional + predictive hierarchy).

#### 11.1 Omnidirectional Bridge Requirements

Every new module must implement an omnidirectional bridge:

```c
// Required omnidirectional integration structure
typedef struct {
    // JEPA bidirectional connection
    nimcp_bio_channel_t* jepa_channel;

    // Predictive hierarchy connection
    nimcp_bio_channel_t* pred_hierarchy_channel;

    // Hopfield memory connection (for pattern retrieval)
    nimcp_bio_channel_t* hopfield_channel;

    // Message handlers
    void (*on_omni_predict_request)(const nimcp_bio_msg_t* msg, void* ctx);
    void (*on_omni_predict_result)(const nimcp_bio_msg_t* msg, void* ctx);
    void (*on_pred_hier_forward)(const nimcp_bio_msg_t* msg, void* ctx);
    void (*on_pred_hier_backward)(const nimcp_bio_msg_t* msg, void* ctx);
    void (*on_pred_hier_error_propagate)(const nimcp_bio_msg_t* msg, void* ctx);

    // Precision weighting from free energy
    float precision_weight;
    float free_energy_estimate;
} module_omni_integration_t;

// Required omnidirectional API
nimcp_status_t module_omni_init(module_t* module, nimcp_brain_t* brain);
nimcp_status_t module_omni_forward_predict(module_t* module, const float* input);
nimcp_status_t module_omni_backward_infer(module_t* module, const float* target);
nimcp_status_t module_omni_update_precision(module_t* module, float precision);
```

#### 11.2 Omnidirectional Bridge Files per Module

Each new module requires:
- `include/cognitive/<module>/nimcp_<module>_omni_bridge.h`
- `src/cognitive/<module>/nimcp_<module>_omni_bridge.c`

#### 11.3 Omnidirectional Message Types

| Message Type | Direction | Purpose |
|--------------|-----------|---------|
| BIO_MSG_OMNI_PREDICT_REQUEST | Module → JEPA | Request forward prediction |
| BIO_MSG_OMNI_PREDICT_RESULT | JEPA → Module | Prediction result |
| BIO_MSG_OMNI_DIRECTION_SWITCH | Coordinator → All | Switch forward/backward mode |
| BIO_MSG_OMNI_PRECISION_UPDATE | FEP → Module | Update precision weighting |
| BIO_MSG_OMNI_FREE_ENERGY_REPORT | Module → FEP | Report free energy |
| BIO_MSG_PRED_HIER_FORWARD | Lower → Higher | Bottom-up prediction |
| BIO_MSG_PRED_HIER_BACKWARD | Higher → Lower | Top-down expectation |
| BIO_MSG_PRED_HIER_ERROR_PROPAGATE | Any → Any | Prediction error |

### 12. Hypothalamus Integration

**CRITICAL**: All new modules MUST integrate bidirectionally with the Hypothalamus for homeostatic regulation.

#### 12.1 Hypothalamus Bridge Requirements

The Hypothalamus is the master homeostatic regulator. Every new module must:

1. **Receive** circadian phase signals
2. **Receive** stress/cortisol signals
3. **Receive** autonomic state signals
4. **Send** energy/metabolic demands
5. **Send** homeostatic perturbations

```c
// Required hypothalamus integration
typedef struct {
    // Hypothalamus reference
    hypothalamus_adapter_t* hypothalamus;

    // Incoming signals
    float circadian_modulation;     // From SCN
    float cortisol_level;           // From HPA axis
    float autonomic_balance;        // Sympathetic/parasympathetic

    // Outgoing signals
    float metabolic_demand;         // Energy requirements
    float homeostatic_perturbation; // Effect on homeostasis

    // Bio-async channel
    nimcp_bio_channel_t* hypo_channel;

    // Message handlers
    void (*on_circadian_phase)(hypo_circadian_phase_t phase, void* ctx);
    void (*on_stress_response)(float cortisol, void* ctx);
    void (*on_homeostatic_alert)(uint32_t alert_type, float urgency, void* ctx);
} module_hypothalamus_integration_t;

// Required hypothalamus API per module
nimcp_status_t module_hypothalamus_init(module_t* module, nimcp_brain_t* brain);
nimcp_status_t module_hypothalamus_update(module_t* module);
float module_get_circadian_modulation(module_t* module);
float module_get_stress_modulation(module_t* module);
```

#### 12.2 Hypothalamus Bridge Files per Module

Each new module requires:
- `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_<module>_bridge.h`
- `src/core/brain/regions/hypothalamus/nimcp_hypothalamus_<module>_bridge.c`

#### 12.3 Required Hypothalamus Connections

| Module | Hypothalamus Input | Hypothalamus Output |
|--------|-------------------|---------------------|
| Locus Coeruleus | Circadian, Stress | Arousal modulation |
| VTA | Circadian, Hunger | Reward sensitivity |
| Raphe | Circadian, Stress | Mood modulation |
| Habenula | Stress | Depression signal |
| Entorhinal | Circadian | Memory consolidation timing |
| All Sensory | Circadian, Autonomic | Sensory gain |
| All Executive | Stress, Circadian | Decision urgency |
| Claustrum | All | Global state broadcast |

#### 12.4 Hypothalamus Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/hypothalamus/test_hypo_neuromod_integration.cpp` | 30 |
| `test/integration/hypothalamus/test_hypo_memory_integration.cpp` | 20 |
| `test/integration/hypothalamus/test_hypo_sensory_integration.cpp` | 20 |
| `test/integration/hypothalamus/test_hypo_executive_integration.cpp` | 20 |
| `test/integration/hypothalamus/test_hypo_omnidirectional_integration.cpp` | 15 |
| **Total** | **105** |

### 13. Thalamic/Middleware Layer Integration

All modules must integrate with the thalamic system for attention gating and the middleware layer for routing, encoding, and training coordination.

#### 13.1 Thalamus Integration Requirements

The thalamus (`include/core/brain/subcortical/nimcp_thalamus.h`) provides sensory relay and cortical gating with:

- **Nuclei Types**: LGN (visual), MGN (auditory), VPL/VPM (somatosensory), VA/VL (motor), Pulvinar (attention), MD (executive), TRN (inhibitory gating)
- **Firing Modes**: Tonic (faithful relay), Burst (reduced attention), Inhibited (TRN suppression)
- **Relay Order**: First-order (subcortical input) vs Higher-order (cortical input)

```c
// Required thalamic integration per module
typedef struct {
    // Thalamic relay configuration
    thal_nucleus_type_t primary_nucleus;
    thal_relay_order_t relay_order;

    // Attention gating
    float attention_weight;
    float trn_inhibition_threshold;

    // Firing mode sensitivity
    float tonic_mode_gain;
    float burst_mode_gain;

    // Bio-async channel to thalamus
    nimcp_bio_channel_t* thalamus_channel;

    // Callbacks
    void (*on_attention_change)(float new_attention, void* ctx);
    void (*on_mode_change)(thal_firing_mode_t mode, void* ctx);
    void (*on_relay_complete)(const float* output, uint32_t size, void* ctx);
} module_thalamic_integration_t;

// Required thalamic API per module
nimcp_status_t module_thalamic_init(module_t* module, thalamus_t* thalamus);
nimcp_status_t module_thalamic_relay(module_t* module, const float* input, uint32_t size);
nimcp_status_t module_set_thalamic_attention(module_t* module, float attention);
float module_get_thalamic_gating(module_t* module);
```

#### 13.2 Middleware Layer Integration Requirements

The middleware layer provides routing, encoding, positional encoding, and training coordination:

| Middleware Component | Header | Purpose |
|---------------------|--------|---------|
| Thalamic Router | `middleware/thalamic/nimcp_thalamic_router.h` | Signal routing |
| Encoding Layer | `middleware/encoding/nimcp_encoding_layer.h` | Feature encoding |
| Positional Encoding | `middleware/encoding/nimcp_positional_encoding.h` | Position-aware processing |
| Training Middleware | `middleware/training/nimcp_training_middleware.h` | Learning coordination |
| Compression | `middleware/compression/nimcp_middleware_compression.h` | Data compression |

```c
// Required middleware integration per module
typedef struct {
    // Router integration
    nimcp_thalamic_router_t* router;
    uint32_t routing_priority;

    // Encoding integration
    nimcp_encoding_layer_t* encoder;
    uint32_t embedding_dim;

    // Positional encoding
    nimcp_positional_encoding_t* pos_encoder;
    bool use_rotary_encoding;

    // Training middleware
    nimcp_training_middleware_t* training_mw;
    bool training_enabled;

    // Compression (for large tensors)
    bool compression_enabled;
    float compression_ratio;
} module_middleware_integration_t;

// Required middleware API per module
nimcp_status_t module_middleware_init(module_t* module, nimcp_middleware_context_t* mw_ctx);
nimcp_status_t module_encode_input(module_t* module, const float* input, uint32_t size, float* encoded);
nimcp_status_t module_route_output(module_t* module, const float* output, uint32_t size, uint32_t target_id);
nimcp_status_t module_apply_positional_encoding(module_t* module, float* tensor, uint32_t seq_len);
```

#### 13.3 Thalamic Bridge Files per Module

Each new module requires:
- `include/cognitive/<module>/nimcp_<module>_thalamic_bridge.h`
- `src/cognitive/<module>/nimcp_<module>_thalamic_bridge.c`

#### 13.4 Required Thalamic Nucleus Connections

| Module | Primary Nucleus | Relay Order | Attention Gating |
|--------|----------------|-------------|------------------|
| Locus Coeruleus | Pulvinar | Higher | Global arousal |
| VTA | MD | Higher | Reward attention |
| Raphe | MD | Higher | Mood modulation |
| Habenula | MD | Higher | Aversive attention |
| Entorhinal | Anterior | Higher | Memory gating |
| Perirhinal | Pulvinar | Higher | Object attention |
| Parahippocampal | Pulvinar | Higher | Scene attention |
| Somatosensory | VPL/VPM | First | Tactile gating |
| Olfactory | MD | First | Smell gating |
| Gustatory | VPM | First | Taste gating |
| Orbitofrontal | MD | Higher | Value attention |
| Retrosplenial | Anterior | Higher | Spatial gating |
| Claustrum | Pulvinar | Higher | Global binding |
| PAG | Brainstem relay | First | Survival gating |
| All Visual | LGN | First | Visual attention |
| All Auditory | MGN | First | Auditory attention |
| All Motor | VA/VL | Higher | Action selection |

#### 13.5 Thalamic/Middleware Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/thalamic/test_thal_neuromod_integration.cpp` | 25 |
| `test/integration/thalamic/test_thal_memory_integration.cpp` | 20 |
| `test/integration/thalamic/test_thal_sensory_integration.cpp` | 25 |
| `test/integration/thalamic/test_thal_executive_integration.cpp` | 20 |
| `test/integration/middleware/test_mw_routing_integration.cpp` | 20 |
| `test/integration/middleware/test_mw_encoding_integration.cpp` | 15 |
| `test/integration/middleware/test_mw_training_integration.cpp` | 15 |
| **Total** | **140** |

### 14. Neural Substrate Layer Integration

All modules must integrate with the neural substrate layer for metabolic modeling, physical constraints, and biophysical simulation.

#### 14.1 Neural Substrate Integration Requirements

The neural substrate (`include/core/neural_substrate/nimcp_neural_substrate.h`) provides:

- **Metabolic State**: ATP level, oxygen saturation, glucose level
- **Physical State**: Temperature, membrane integrity, ion balance
- **Modulation Factors**: Firing rate, transmission efficiency, conduction velocity, plasticity capacity
- **Health Levels**: Optimal, Stressed, Compromised, Critical, Failing
- **Alerts**: Low ATP, Hypoxia, Hypoglycemia, Hyperthermia, Hypothermia, Ion imbalance, Membrane damage

```c
// Required neural substrate integration per module
typedef struct {
    // Substrate reference
    neural_substrate_t* substrate;

    // Module-specific metabolic costs
    float cost_per_activation;      // ATP cost per activation event
    float cost_per_spike;           // ATP cost per spike
    float cost_per_transmission;    // ATP cost per synaptic event
    float baseline_cost;            // Resting metabolic cost

    // Substrate modulation sensitivity
    float atp_sensitivity;          // How much ATP affects module
    float temperature_sensitivity;  // Q10 coefficient
    float oxygen_sensitivity;       // Hypoxia sensitivity

    // Module capacity thresholds
    float min_capacity_threshold;   // Below this, module enters low-power mode
    float critical_capacity;        // Below this, module shuts down

    // Bio-async channel to substrate
    nimcp_bio_channel_t* substrate_channel;

    // Callbacks
    void (*on_capacity_change)(float new_capacity, void* ctx);
    void (*on_alert)(substrate_alert_type_t alert, void* ctx);
    void (*on_health_change)(substrate_health_level_t health, void* ctx);
} module_substrate_integration_t;

// Required substrate API per module
nimcp_status_t module_substrate_init(module_t* module, neural_substrate_t* substrate);
nimcp_status_t module_record_activity(module_t* module, uint32_t spike_count, uint32_t transmission_count);
float module_get_substrate_modulation(module_t* module);
substrate_health_level_t module_get_substrate_health(module_t* module);
bool module_check_substrate_capacity(module_t* module);
```

#### 14.2 Substrate Bridge Files per Module

Each new module requires:
- `include/core/neural_substrate/bridges/nimcp_substrate_<module>_bridge.h`
- `src/core/neural_substrate/bridges/nimcp_substrate_<module>_bridge.c`

#### 14.3 Module-Specific Metabolic Parameters

| Module | Cost/Activation | ATP Sensitivity | Temperature Q10 | Min Capacity |
|--------|-----------------|-----------------|-----------------|--------------|
| Locus Coeruleus | 0.002 | High (0.9) | 2.5 | 0.4 |
| VTA | 0.003 | High (0.9) | 2.5 | 0.4 |
| Raphe | 0.002 | Medium (0.7) | 2.3 | 0.3 |
| Habenula | 0.001 | Medium (0.7) | 2.2 | 0.3 |
| Entorhinal | 0.005 | High (0.85) | 2.5 | 0.5 |
| Hippocampal regions | 0.006 | Very High (0.95) | 2.8 | 0.5 |
| Somatosensory | 0.004 | Medium (0.7) | 2.0 | 0.4 |
| Sensory regions | 0.003 | Medium (0.6) | 2.0 | 0.3 |
| Executive regions | 0.008 | Very High (0.95) | 2.5 | 0.5 |
| Claustrum | 0.007 | High (0.85) | 2.4 | 0.5 |
| PAG | 0.002 | Low (0.5) | 1.8 | 0.2 |

#### 14.4 Substrate Health Response per Module

| Health Level | Module Response |
|--------------|-----------------|
| Optimal | Full operation |
| Stressed | Reduce firing rate by 10%, increase energy efficiency |
| Compromised | Reduce firing rate by 30%, disable non-essential functions |
| Critical | Minimal operation, only essential signals |
| Failing | Emergency shutdown, broadcast distress signal |

#### 14.5 Neural Substrate Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/substrate/test_substrate_neuromod_integration.cpp` | 25 |
| `test/integration/substrate/test_substrate_memory_integration.cpp` | 20 |
| `test/integration/substrate/test_substrate_sensory_integration.cpp` | 20 |
| `test/integration/substrate/test_substrate_executive_integration.cpp` | 20 |
| `test/integration/substrate/test_substrate_metabolic_cascade.cpp` | 15 |
| `test/integration/substrate/test_substrate_health_recovery.cpp` | 15 |
| **Total** | **115** |

### 15. Motor Cortex Integration

All modules must integrate with the motor cortex for action output and sensorimotor coordination.

#### 15.1 Motor Cortex Integration Requirements

The motor cortex adapter (`include/core/brain/regions/motor/nimcp_motor_adapter.h`) provides:

- **Body Regions**: Face, hands, arms, trunk, legs, feet, eyes (somatotopic homunculus)
- **Movement Types**: Discrete, Serial, Continuous, Ballistic, Corrective
- **Processing Stages**: Planning, Preparing, Executing, Correcting
- **Motor Programs**: Stored learned movement sequences
- **Integration Points**: Basal ganglia (action selection), Cerebellum (coordination), Thalamus VA/VL (routing)

```c
// Required motor cortex integration per module
typedef struct {
    // Motor adapter reference
    motor_adapter_t* motor;

    // Module's motor influence
    bool can_initiate_movement;     // Module can request movements
    bool receives_motor_feedback;   // Module gets efference copy
    bool modulates_motor_vigor;     // Module affects movement amplitude

    // Supported body regions (bitmask)
    uint32_t influenced_regions;

    // Motor modulation parameters
    float vigor_modulation;         // How much module affects vigor [0-2]
    float timing_modulation;        // How much module affects timing [0.5-2]
    float precision_modulation;     // How much module affects precision [0-2]

    // Bio-async channel to motor
    nimcp_bio_channel_t* motor_channel;

    // Callbacks
    void (*on_movement_start)(uint32_t movement_id, motor_region_t region, void* ctx);
    void (*on_movement_complete)(const motor_result_t* result, void* ctx);
    void (*on_motor_feedback)(uint32_t effector_id, const motor_effector_state_t* state, void* ctx);
} module_motor_integration_t;

// Required motor API per module
nimcp_status_t module_motor_init(module_t* module, motor_adapter_t* motor);
nimcp_status_t module_request_movement(module_t* module, const motor_goal_t* goal);
nimcp_status_t module_modulate_vigor(module_t* module, float vigor_factor);
nimcp_status_t module_receive_efference_copy(module_t* module, const motor_command_t* cmd);
```

#### 15.2 Motor Bridge Files per Module

Each new module requires:
- `include/core/brain/regions/motor/bridges/nimcp_motor_<module>_bridge.h`
- `src/core/brain/regions/motor/bridges/nimcp_motor_<module>_bridge.c`

#### 15.3 Motor Cortex Connection Roles

| Module | Motor Role | Influenced Regions | Primary Function |
|--------|------------|-------------------|------------------|
| Locus Coeruleus | Arousal modulation | All | Global motor readiness |
| VTA | Vigor/motivation | All | Movement initiation |
| Raphe | Impulse control | All | Movement timing |
| Habenula | Inhibition | All | Movement suppression |
| Basal Ganglia | Action selection | All | Movement gating |
| Cerebellum | Coordination | All | Movement timing |
| Somatosensory | Proprioception | All | Sensory feedback |
| Entorhinal | Navigation | Legs, Eyes | Path integration |
| Orbitofrontal | Value-based action | All | Action value |
| Retrosplenial | Spatial action | Legs, Arms, Eyes | Oriented movement |
| PAG | Survival motor | All | Fight/flight response |
| Superior Colliculus | Eye movement | Eyes | Saccades |
| Claustrum | Global coordination | All | Multi-effector binding |

#### 15.4 Motor-Cognitive Feedback Loops

```c
// Efference copy integration (motor → cognitive)
typedef struct {
    // Predicted sensory consequence
    float* predicted_proprioception;
    float* predicted_visual_pos;

    // Motor command copy
    motor_command_t current_command;

    // Timing information
    double movement_onset_ms;
    double expected_duration_ms;

    // For sensory prediction
    bool expecting_reafference;
} motor_efference_copy_t;

// Corollary discharge pathway
nimcp_status_t motor_send_efference_copy(
    motor_adapter_t* motor,
    nimcp_bio_router_t* router,
    const motor_command_t* command
);

// Receiving module processes efference copy
nimcp_status_t module_process_efference_copy(
    module_t* module,
    const motor_efference_copy_t* copy
);
```

#### 15.5 Motor Cortex Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/motor/test_motor_neuromod_integration.cpp` | 25 |
| `test/integration/motor/test_motor_bg_integration.cpp` | 25 |
| `test/integration/motor/test_motor_cerebellum_integration.cpp` | 25 |
| `test/integration/motor/test_motor_sensory_integration.cpp` | 20 |
| `test/integration/motor/test_motor_cognitive_integration.cpp` | 20 |
| `test/integration/motor/test_motor_efference_copy.cpp` | 15 |
| `test/integration/motor/test_motor_planning_execution.cpp` | 20 |
| **Total** | **150** |

### 16. Portia Module Integration

All modules must integrate with the Portia adaptive intelligence system for resource-aware operation and graceful degradation.

#### 16.1 Portia System Overview

The Portia module (`include/portia/nimcp_portia.h`) provides Portia fimbriata-inspired adaptive intelligence:

- **Tier Manager**: Dynamic platform tier switching (FULL → MEDIUM → CONSTRAINED → MINIMAL)
- **Power Monitor**: Battery/energy awareness with state tracking
- **Resource Tracker**: CPU/memory/thermal monitoring
- **Degradation Controller**: Graceful feature reduction
- **Accelerator Detector**: GPU/NPU/TPU/FPGA discovery and offload
- **Sensor Fusion**: Multi-metric decision making
- **Planning Engine**: Strategic resource allocation

#### 16.2 Portia Integration Requirements

```c
// Required Portia integration per module
typedef struct {
    // Portia context reference
    portia_context_t* portia;

    // Module resource profile
    float cpu_weight;               // Module's CPU usage weight [0-1]
    float memory_weight;            // Module's memory usage weight [0-1]
    float power_weight;             // Module's power consumption weight [0-1]

    // Tier-dependent behavior
    bool supports_degradation;      // Module can operate at reduced capacity
    portia_degradation_level_t min_degradation;  // Minimum acceptable degradation
    portia_degradation_level_t max_degradation;  // Maximum tolerable degradation

    // Resource thresholds
    float min_cpu_required;         // Minimum CPU for operation
    float min_memory_required;      // Minimum memory for operation

    // Accelerator preferences
    portia_accelerator_type_t preferred_accelerator;
    bool can_offload;               // Module can offload to accelerator

    // Bio-async channel to Portia
    nimcp_bio_channel_t* portia_channel;

    // Callbacks
    void (*on_tier_change)(platform_tier_t new_tier, void* ctx);
    void (*on_degradation_change)(portia_degradation_level_t level, void* ctx);
    void (*on_power_state_change)(portia_power_state_t state, void* ctx);
    void (*on_thermal_alert)(portia_thermal_state_t state, void* ctx);
} module_portia_integration_t;

// Required Portia API per module
nimcp_status_t module_portia_init(module_t* module, portia_context_t* portia);
nimcp_status_t module_adapt_to_tier(module_t* module, platform_tier_t tier);
nimcp_status_t module_apply_degradation(module_t* module, portia_degradation_level_t level);
uint32_t module_get_recommended_neurons(module_t* module);
bool module_can_offload_to_accelerator(module_t* module, portia_accelerator_type_t accel);
```

#### 16.3 Portia Bridge Files per Module

Each new module requires:
- `include/portia/bridges/nimcp_portia_<module>_bridge.h`
- `src/portia/bridges/nimcp_portia_<module>_bridge.c`

#### 16.4 Module Degradation Profiles

| Module | Min Degradation | Max Degradation | Accelerator Support |
|--------|-----------------|-----------------|---------------------|
| Locus Coeruleus | NONE | MODERATE | CPU only |
| VTA | NONE | MODERATE | CPU only |
| Raphe | NONE | SEVERE | CPU only |
| Habenula | NONE | SEVERE | CPU only |
| Entorhinal | NONE | MODERATE | GPU optional |
| Hippocampal regions | NONE | MINOR | GPU preferred |
| Somatosensory | MINOR | SEVERE | CPU only |
| Sensory regions | MINOR | SEVERE | GPU optional |
| Executive regions | NONE | MODERATE | GPU optional |
| Claustrum | NONE | MINOR | GPU optional |
| PAG | NONE | EMERGENCY | CPU only (survival-critical) |
| All Memory | NONE | MODERATE | GPU preferred |
| All Visual | MINOR | SEVERE | GPU preferred |

#### 16.5 Tier-Dependent Module Behavior

| Tier | Neuron Scaling | Feature Set | Precision |
|------|---------------|-------------|-----------|
| FULL | 100% | All features | float32 |
| MEDIUM | 50-75% | Most features | float32 |
| CONSTRAINED | 25-50% | Essential features | float16/int8 |
| MINIMAL | 10-25% | Critical only | int8 |

#### 16.6 Portia Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/portia/test_portia_neuromod_integration.cpp` | 25 |
| `test/integration/portia/test_portia_memory_integration.cpp` | 20 |
| `test/integration/portia/test_portia_sensory_integration.cpp` | 20 |
| `test/integration/portia/test_portia_degradation_cascade.cpp` | 25 |
| `test/integration/portia/test_portia_tier_switching.cpp` | 20 |
| `test/integration/portia/test_portia_accelerator_offload.cpp` | 15 |
| **Total** | **125** |

### 17. Swarm Module Integration

All modules must integrate with the Swarm collective intelligence system for distributed cognition and emergent behavior.

#### 17.1 Swarm System Overview

The Swarm module (`include/swarm/nimcp_swarm_brain.h`) provides distributed cognitive processing:

- **Local Brain**: Resource-constrained NIMCP brain per agent
- **Signal Adapter**: Radio communication abstraction (LoRa, WiFi, etc.)
- **Collective Workspace**: Shared attention and goals
- **Emergence Detection**: Tier tracking (Disconnected → Superorganism)
- **Consensus Engine**: Voting and decision-making
- **Neuromodulator Sync**: Emotional state sharing across swarm
- **Bio-async Integration**: Internal message coordination

#### 17.2 Emergence Tiers

| Tier | Agents | Capability |
|------|--------|------------|
| TIER_0_DISCONNECTED | 1 | Solo operation |
| TIER_1_PAIRED | 2-3 | Basic coordination |
| TIER_2_CLUSTER | 4-7 | Group behaviors |
| TIER_3_SWARM | 8-15 | Emergent intelligence |
| TIER_4_SUPERORGANISM | 16+ | Full collective cognition |

#### 17.3 Swarm Integration Requirements

```c
// Required swarm integration per module
typedef struct {
    // Swarm brain reference
    swarm_brain_t* swarm;

    // Module's swarm role
    bool participates_in_consensus;     // Module votes on decisions
    bool shares_perception;             // Module broadcasts observations
    bool receives_threats;              // Module receives threat alerts
    bool syncs_neuromodulators;         // Module shares emotional state

    // Emergence-dependent behavior
    swarm_emergence_tier_t min_tier;    // Minimum tier for module activation
    float coherence_threshold;          // Required swarm coherence [0-1]

    // Collective workspace
    uint32_t workspace_concepts[8];     // Concepts this module attends to
    uint32_t concept_count;

    // Weight synchronization
    bool supports_weight_sync;          // Module can sync neural weights
    uint32_t sync_layers[8];            // Layers to synchronize
    uint32_t sync_layer_count;

    // Bio-async channel to swarm
    nimcp_bio_channel_t* swarm_channel;

    // Callbacks
    void (*on_emergence_change)(swarm_emergence_tier_t tier, void* ctx);
    void (*on_peer_perception)(const perception_data_t* data, void* ctx);
    void (*on_threat_alert)(const threat_data_t* threat, void* ctx);
    void (*on_consensus_result)(uint32_t proposal_id, vote_decision_t result, void* ctx);
    void (*on_neuromod_sync)(const neuromod_state_t* collective_state, void* ctx);
} module_swarm_integration_t;

// Required swarm API per module
nimcp_status_t module_swarm_init(module_t* module, swarm_brain_t* swarm);
nimcp_status_t module_broadcast_observation(module_t* module, const perception_data_t* data);
nimcp_status_t module_propose_action(module_t* module, const vote_proposal_t* proposal);
nimcp_status_t module_cast_vote(module_t* module, uint32_t proposal_id, vote_decision_t vote);
nimcp_status_t module_sync_weights_to_swarm(module_t* module);
swarm_emergence_tier_t module_get_swarm_tier(module_t* module);
```

#### 17.4 Swarm Bridge Files per Module

Each new module requires:
- `include/swarm/bridges/nimcp_swarm_<module>_bridge.h`
- `src/swarm/bridges/nimcp_swarm_<module>_bridge.c`

#### 17.5 Module Swarm Roles

| Module | Consensus | Perception Share | Threat Receive | Neuromod Sync |
|--------|-----------|------------------|----------------|---------------|
| Locus Coeruleus | No | No | Yes | Yes (arousal) |
| VTA | No | No | No | Yes (reward) |
| Raphe | No | No | Yes | Yes (mood) |
| Habenula | No | No | Yes | Yes (aversion) |
| Entorhinal | Yes | Yes (spatial) | Yes | No |
| Hippocampal regions | Yes | Yes (memory) | Yes | No |
| Somatosensory | No | Yes (touch) | Yes | No |
| Visual regions | No | Yes (vision) | Yes | No |
| Auditory regions | No | Yes (sound) | Yes | No |
| Executive regions | Yes | No | Yes | Yes |
| Claustrum | Yes | Yes (binding) | Yes | Yes |
| PAG | No | No | Yes | Yes (survival) |
| Orbitofrontal | Yes | No | No | Yes |

#### 17.6 Swarm Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/swarm/test_swarm_neuromod_integration.cpp` | 25 |
| `test/integration/swarm/test_swarm_memory_integration.cpp` | 20 |
| `test/integration/swarm/test_swarm_sensory_integration.cpp` | 25 |
| `test/integration/swarm/test_swarm_consensus_integration.cpp` | 25 |
| `test/integration/swarm/test_swarm_emergence_cascade.cpp` | 20 |
| `test/integration/swarm/test_swarm_weight_sync.cpp` | 15 |
| `test/integration/swarm/test_swarm_collective_learning.cpp` | 20 |
| **Total** | **150** |

### 18. Dragonfly Module Integration

All modules must integrate with the Dragonfly target tracking system for rapid pursuit and interception capabilities.

#### 18.1 Dragonfly System Overview

The Dragonfly module (`include/dragonfly/nimcp_dragonfly.h`) provides biological target tracking:

- **TSDN**: Target-Selective Descending Neurons (16-neuron population coding)
- **CSTMD1**: Centrifugal Small-Target Motion Detector (winner-take-all attention)
- **Prediction**: IMM filter for trajectory estimation
- **Interception**: Proportional Navigation guidance for optimal pursuit

#### 18.2 Operating Modes

| Mode | Description |
|------|-------------|
| IDLE | Waiting for targets |
| SCANNING | Actively scanning for targets |
| TRACKING | Locked onto target |
| PURSUING | Active pursuit |
| INTERCEPTING | Final interception phase |

#### 18.3 Dragonfly Integration Requirements

```c
// Required dragonfly integration per module
typedef struct {
    // Dragonfly system reference
    dragonfly_system_t* dragonfly;

    // Module's dragonfly role
    bool provides_detections;           // Module sends visual detections
    bool receives_motor_commands;       // Module receives pursuit commands
    bool modulates_tracking;            // Module affects tracking behavior
    bool receives_state_updates;        // Module gets tracking state

    // Detection parameters (if provides_detections)
    float min_contrast;                 // Minimum contrast for valid detection
    float max_distance;                 // Maximum detection distance
    uint32_t max_targets_per_frame;     // Max detections per update

    // Tracking modulation
    float attention_weight;             // How much module affects target selection
    float urgency_weight;               // How much module affects pursuit urgency

    // Energy awareness
    bool energy_aware_pursuit;          // Consider energy in pursuit decisions
    float min_energy_for_pursuit;       // Minimum energy to initiate pursuit

    // Bio-async channel to dragonfly
    nimcp_bio_channel_t* dragonfly_channel;

    // Callbacks
    void (*on_mode_change)(dragonfly_mode_t mode, void* ctx);
    void (*on_target_lock)(const dragonfly_target_info_t* target, void* ctx);
    void (*on_hunt_result)(dragonfly_hunt_result_t result, void* ctx);
    void (*on_motor_command)(const dragonfly_motor_cmd_t* cmd, void* ctx);
    void (*on_evasion_detected)(evasion_type_t evasion, void* ctx);
} module_dragonfly_integration_t;

// Required dragonfly API per module
nimcp_status_t module_dragonfly_init(module_t* module, dragonfly_system_t* dragonfly);
nimcp_status_t module_send_detection(module_t* module, const dragonfly_detection_t* detection);
nimcp_status_t module_modulate_tracking(module_t* module, float attention_mod, float urgency_mod);
dragonfly_mode_t module_get_dragonfly_mode(module_t* module);
nimcp_status_t module_request_pursuit_abort(module_t* module);
```

#### 18.4 Dragonfly Bridge Files per Module

Each new module requires:
- `include/dragonfly/bridges/nimcp_dragonfly_<module>_bridge.h`
- `src/dragonfly/bridges/nimcp_dragonfly_<module>_bridge.c`

#### 18.5 Module Dragonfly Roles

| Module | Detections | Motor Cmds | Tracking Mod | State Updates |
|--------|------------|------------|--------------|---------------|
| Visual Cortex (V1-V5) | Yes | No | Yes | Yes |
| Superior Colliculus | Yes | Yes | Yes | Yes |
| Locus Coeruleus | No | No | Yes (arousal) | Yes |
| VTA | No | No | Yes (motivation) | Yes |
| Habenula | No | No | Yes (inhibition) | Yes |
| Motor Cortex | No | Yes | No | Yes |
| Cerebellum | No | Yes | No | Yes |
| PAG | No | No | Yes (survival) | Yes |
| Orbitofrontal | No | No | Yes (value) | Yes |
| Attention System | No | No | Yes | Yes |
| Executive | No | No | Yes | Yes |

#### 18.6 Dragonfly-Cognitive Integration

```c
// Integration with cognitive systems
typedef struct {
    // Attention modulation
    float salience_boost;               // Boost target salience in attention system
    float distractor_suppression;       // Suppress non-targets

    // Emotional modulation
    float fear_response;                // Fear affects pursuit (e.g., predator detection)
    float reward_anticipation;          // Reward affects pursuit vigor

    // Executive control
    bool allow_pursuit_override;        // Executive can abort pursuit
    float abort_threshold;              // Confidence threshold for abort

    // Working memory
    bool track_target_history;          // Maintain target history in WM
    uint32_t max_history_targets;       // Maximum tracked target history
} dragonfly_cognitive_params_t;

// Bidirectional communication
nimcp_status_t dragonfly_receive_attention_modulation(
    dragonfly_system_t* dragonfly,
    const float* attention_weights,
    uint32_t weight_count
);

nimcp_status_t dragonfly_send_to_working_memory(
    dragonfly_system_t* dragonfly,
    const dragonfly_target_info_t* target
);
```

#### 18.7 Dragonfly Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/dragonfly/test_dragonfly_visual_integration.cpp` | 25 |
| `test/integration/dragonfly/test_dragonfly_motor_integration.cpp` | 25 |
| `test/integration/dragonfly/test_dragonfly_attention_integration.cpp` | 20 |
| `test/integration/dragonfly/test_dragonfly_executive_integration.cpp` | 20 |
| `test/integration/dragonfly/test_dragonfly_emotion_integration.cpp` | 15 |
| `test/integration/dragonfly/test_dragonfly_pursuit_pipeline.cpp` | 25 |
| **Total** | **130** |

### 19. Sleep Module Integration

All modules must integrate with the Sleep-Wake cycle system for memory consolidation, synaptic homeostasis, and energy management.

#### 19.1 Sleep System Overview

The Sleep module (`include/cognitive/nimcp_sleep_wake.h`) provides biologically-inspired sleep-wake cycling:

- **Sleep States**: AWAKE (Beta/Gamma 13-100Hz) → DROWSY (Alpha 8-13Hz) → LIGHT_NREM (Theta 4-8Hz) → DEEP_NREM (Delta 0.5-4Hz) → REM (Theta + atonia)
- **Sleep Pressure**: Adenosine accumulation model for metabolic cost of learning
- **Memory Consolidation**: Prioritized replay during deep sleep
- **Synaptic Homeostasis**: Weight downscaling (85%) and pruning (threshold 0.01)
- **REM Creativity**: Random activation for creative recombination

#### 19.2 Sleep State Characteristics

| State | Frequency | Duration | Function |
|-------|-----------|----------|----------|
| AWAKE | 13-100Hz (β/γ) | Active | Normal processing |
| DROWSY | 8-13Hz (α) | 2 min | Transition, relaxation |
| LIGHT_NREM | 4-8Hz (θ) | 15 min | Memory sorting |
| DEEP_NREM | 0.5-4Hz (δ) | 30 min | Consolidation, homeostasis |
| REM | θ + atonia | 10 min | Creativity, emotional processing |

#### 19.3 Sleep Integration Requirements

```c
// Required sleep integration per module
typedef struct {
    // Sleep system reference
    sleep_system_t sleep;

    // Module's sleep behavior
    bool suspends_during_sleep;         // Module pauses in sleep states
    bool participates_in_replay;        // Module memories are replayed
    bool requires_homeostasis;          // Module needs synaptic scaling
    bool active_during_rem;             // Module participates in REM

    // State-dependent behavior
    sleep_state_t min_active_state;     // Minimum state for operation
    float awake_activity_level;         // Activity during awake [0-1]
    float drowsy_activity_level;        // Activity during drowsy [0-1]
    float nrem_activity_level;          // Activity during NREM [0-1]
    float rem_activity_level;           // Activity during REM [0-1]

    // Memory consolidation
    float consolidation_priority;       // Priority for replay [0-1]
    bool emotional_priority_boost;      // Emotional memories get priority
    uint32_t replay_batch_contribution; // Memories per replay batch

    // Synaptic homeostasis
    float downscaling_sensitivity;      // How much to downscale [0-1]
    float pruning_threshold;            // Threshold for synapse removal
    bool exempt_from_pruning;           // Critical connections exempt

    // Sleep pressure contribution
    float activity_pressure_rate;       // Pressure added per activation
    float learning_pressure_rate;       // Pressure added per learning step

    // Bio-async channel to sleep
    nimcp_bio_channel_t* sleep_channel;

    // Callbacks
    void (*on_state_change)(sleep_state_t new_state, void* ctx);
    void (*on_consolidation_start)(void* ctx);
    void (*on_consolidation_complete)(uint32_t memories_replayed, void* ctx);
    void (*on_homeostasis_complete)(const homeostasis_stats_t* stats, void* ctx);
} module_sleep_integration_t;

// Required sleep API per module
nimcp_status_t module_sleep_init(module_t* module, sleep_system_t sleep);
nimcp_status_t module_on_sleep_state_change(module_t* module, sleep_state_t state);
nimcp_status_t module_contribute_to_replay(module_t* module, void** memories, uint32_t* count);
nimcp_status_t module_apply_homeostasis(module_t* module, float scaling_factor, float pruning_threshold);
float module_get_sleep_pressure_contribution(module_t* module);
bool module_is_sleep_critical(module_t* module);  // Must stay active during sleep
```

#### 19.4 Sleep Bridge Files per Module

Each new module requires:
- `include/cognitive/<module>/nimcp_<module>_sleep_bridge.h`
- `src/cognitive/<module>/nimcp_<module>_sleep_bridge.c`

#### 19.5 Module Sleep Behavior Profiles

| Module | Suspends | Replay | Homeostasis | REM Active | Priority |
|--------|----------|--------|-------------|------------|----------|
| Locus Coeruleus | Partial | No | Yes | Yes (arousal) | N/A |
| VTA | Partial | No | Yes | Yes (reward) | N/A |
| Raphe | Partial | No | Yes | No | N/A |
| Habenula | Yes | No | Yes | No | N/A |
| Entorhinal | Yes | Yes | Yes | Yes | High (spatial) |
| Perirhinal | Yes | Yes | Yes | No | High (object) |
| Parahippocampal | Yes | Yes | Yes | Yes | High (context) |
| Hippocampal regions | No (active) | Yes | Yes | Yes | Highest |
| Somatosensory | Yes | Yes | Yes | No | Medium |
| Sensory regions | Yes | Yes | Yes | No | Medium |
| Executive regions | Yes | Yes | Yes | Yes | High |
| Claustrum | Partial | No | Yes | Yes | N/A |
| PAG | No (survival) | No | No | No | N/A |
| Orbitofrontal | Yes | Yes | Yes | Yes | High (value) |

#### 19.6 Sleep Cycle Pipeline

```c
// Sleep cycle integration flow
typedef struct {
    // Pre-sleep phase
    float pre_sleep_pressure;           // Pressure before sleep
    uint32_t awake_learning_steps;      // Learning during awake

    // Drowsy phase
    float oscillation_slowdown;         // Frequency reduction rate
    bool sensory_gating_active;         // Reduce sensory input

    // Light NREM phase
    uint32_t memories_sorted;           // Memories sorted by importance
    float sorting_threshold;            // Threshold for keeping memory

    // Deep NREM phase (consolidation)
    uint32_t total_replays;             // Total replay events
    uint32_t emotional_replays;         // Emotional memory replays
    uint32_t novel_replays;             // Novel memory replays
    float consolidation_efficiency;     // Success rate [0-1]

    // Synaptic homeostasis (during deep NREM)
    float total_weight_before;          // Pre-scaling total
    float total_weight_after;           // Post-scaling total
    uint32_t synapses_pruned;           // Removed weak synapses
    float energy_saved;                 // Estimated energy savings

    // REM phase
    uint32_t creative_activations;      // Random activations
    uint32_t novel_associations;        // New associations formed
    float emotional_processing;         // Emotional regulation

    // Post-wake
    float post_wake_pressure;           // Pressure after sleep
    float cognitive_refresh;            // Performance improvement
} sleep_cycle_metrics_t;

// Module participation in sleep phases
nimcp_status_t module_participate_in_drowsy(module_t* module);
nimcp_status_t module_participate_in_light_nrem(module_t* module, float* sort_scores, uint32_t count);
nimcp_status_t module_participate_in_deep_nrem(module_t* module, const void** replay_memories, uint32_t count);
nimcp_status_t module_participate_in_rem(module_t* module, float creativity_noise);
```

#### 19.7 Cross-System Sleep Coordination

```c
// Sleep coordination with other systems
typedef struct {
    // Hypothalamus coordination (circadian)
    bool sync_to_circadian;             // Align sleep with circadian rhythm
    float circadian_sleep_drive;        // Circadian contribution to sleep pressure

    // Immune system coordination
    bool immune_enhanced_during_sleep;  // Boost immune during sleep
    float cytokine_modulation;          // Sleep affects immune signaling

    // Portia coordination (power management)
    bool reduce_power_during_sleep;     // Lower power consumption
    platform_tier_t sleep_tier;         // Tier during sleep (typically CONSTRAINED)

    // Swarm coordination (collective sleep)
    bool sync_swarm_sleep;              // Coordinate sleep across swarm
    float swarm_sleep_coherence;        // How synchronized sleep is

    // Dragonfly coordination
    bool disable_pursuit_during_sleep;  // No hunting while asleep
    dragonfly_mode_t wake_on_threat;    // Wake if threat detected
} sleep_coordination_t;
```

#### 19.8 Sleep Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/sleep/test_sleep_neuromod_integration.cpp` | 25 |
| `test/integration/sleep/test_sleep_memory_integration.cpp` | 30 |
| `test/integration/sleep/test_sleep_consolidation_pipeline.cpp` | 25 |
| `test/integration/sleep/test_sleep_homeostasis_integration.cpp` | 25 |
| `test/integration/sleep/test_sleep_rem_integration.cpp` | 20 |
| `test/integration/sleep/test_sleep_circadian_integration.cpp` | 15 |
| `test/integration/sleep/test_sleep_immune_integration.cpp` | 15 |
| `test/integration/sleep/test_sleep_portia_integration.cpp` | 15 |
| **Total** | **170** |

### 20. Glial Module Integration

Every module must integrate with the glial system (astrocytes, microglia, oligodendrocytes) for:
- **Synaptic modulation** via astrocyte tripartite synapse support
- **Homeostatic plasticity** via astrocyte BCM threshold modulation
- **Activity-dependent pruning** via microglia complement cascade (C1q/C3)
- **Immune signaling** via microglia cytokines (IL-1β, TNF-α, IL-6, IL-10, TGF-β)
- **Conduction optimization** via oligodendrocyte myelination and G-ratio tuning

**Est. LOC**: ~22,000 | **Tests**: ~190

#### 20.1 Astrocyte Integration Pattern

```c
// Astrocyte integration for tripartite synapse support
typedef struct {
    // Calcium dynamics integration
    float intracellular_calcium;        // μM (resting ~0.1, active ~1.0)
    float ip3_concentration;            // IP3 for calcium release
    bool calcium_wave_active;           // Propagating wave state

    // Glutamate/D-serine release
    float glutamate_released;           // mM released from astrocyte
    float d_serine_concentration;       // NMDA co-agonist level
    float gliotransmitter_threshold;    // Calcium threshold for release

    // Synaptic modulation
    float synaptic_gain_modulation;     // Multiplicative factor [0.5-2.0]
    float extrasynaptic_glutamate;      // Spillover glutamate
    float perisynaptic_coverage;        // % of synapse covered [0-1]

    // BCM threshold modulation
    float bcm_threshold;                // Sliding threshold for plasticity
    float bcm_modulation_rate;          // How fast threshold adapts
    bool enable_homeostatic_plasticity; // Use astrocyte BCM control

    // Metabolic support
    float glucose_delivery_rate;        // mmol/s to neurons
    float lactate_shuttle_rate;         // Lactate to neurons
    float atp_support_level;            // ATP provided [0-1]

    // Network reference
    astrocyte_network_t* astro_network; // Parent network
    uint32_t assigned_astrocyte_id;     // Specific astrocyte
} module_astrocyte_integration_t;

// Module astrocyte callbacks
typedef void (*astrocyte_calcium_callback_t)(void* module, float calcium, bool wave_active);
typedef void (*astrocyte_gliotransmitter_callback_t)(void* module, float glutamate, float d_serine);
typedef void (*astrocyte_bcm_callback_t)(void* module, float new_threshold);

// Integration functions
nimcp_status_t module_register_with_astrocyte(
    void* module,
    astrocyte_network_t* network,
    const module_astrocyte_integration_t* config);

nimcp_status_t module_request_calcium_wave(
    void* module,
    float propagation_speed,      // μm/s (typical: 15-25)
    float wave_amplitude);        // Peak calcium [μM]

nimcp_status_t module_receive_gliotransmitter(
    void* module,
    float* glutamate_out,
    float* d_serine_out);

nimcp_status_t module_get_bcm_threshold(
    void* module,
    float* threshold_out);
```

#### 20.2 Microglia Integration Pattern

```c
// Microglia integration for immune surveillance and synaptic pruning
typedef struct {
    // State tracking
    microglia_state_t current_state;    // RAMIFIED, ACTIVATED, PHAGOCYTIC
    float activation_level;             // [0-1] activation intensity
    float surveillance_rate;            // Process extension frequency

    // Synaptic pruning (complement cascade)
    bool c1q_tagged;                    // "Eat me" signal present
    bool c3_opsonized;                  // Ready for phagocytosis
    float pruning_probability;          // [0-1] chance of pruning
    uint32_t synapses_tagged;           // Count of tagged synapses
    uint32_t synapses_pruned;           // Count of pruned synapses

    // Cytokine signaling
    float il1_beta;                     // Pro-inflammatory (pg/mL)
    float tnf_alpha;                    // Pro-inflammatory (pg/mL)
    float il6;                          // Dual-role (pg/mL)
    float il10;                         // Anti-inflammatory (pg/mL)
    float tgf_beta;                     // Neuroprotective (pg/mL)

    // Neuroprotection
    bool neuroprotective_mode;          // Anti-inflammatory phenotype
    float bdnf_secretion;               // Neurotrophic factor level
    float phagocytic_activity;          // Debris clearance rate

    // Network reference
    microglia_network_t* microglia_net; // Parent network
    uint32_t patrolling_region_id;      // Region being monitored
} module_microglia_integration_t;

// Microglia callbacks
typedef void (*microglia_state_callback_t)(void* module, microglia_state_t new_state);
typedef void (*microglia_cytokine_callback_t)(void* module, const microglia_cytokine_state_t* cytokines);
typedef void (*microglia_pruning_callback_t)(void* module, uint32_t synapse_id, bool pruned);

// Integration functions
nimcp_status_t module_register_with_microglia(
    void* module,
    microglia_network_t* network,
    const module_microglia_integration_t* config);

nimcp_status_t module_request_synaptic_tagging(
    void* module,
    uint32_t synapse_id,
    float activity_level,         // Low activity increases C1q tagging
    float importance_score);      // High importance protects from pruning

nimcp_status_t module_receive_cytokine_signal(
    void* module,
    microglia_cytokine_state_t* cytokines_out);

nimcp_status_t module_request_debris_clearance(
    void* module,
    const void* debris_data,
    size_t debris_size);
```

#### 20.3 Oligodendrocyte Integration Pattern

```c
// Oligodendrocyte integration for myelination and conduction optimization
typedef struct {
    // Myelination status
    bool is_myelinated;                 // Whether axon has myelin
    float myelin_thickness;             // μm (typical: 0.5-2.0)
    float g_ratio;                      // Axon/fiber ratio (optimal: 0.6-0.8)
    float internode_length;             // μm between nodes of Ranvier

    // Conduction properties
    float unmyelinated_velocity;        // m/s (0.5-2.0)
    float myelinated_velocity;          // m/s (50-100, 10-100x faster)
    float saltatory_efficiency;         // Energy savings from jumping
    uint32_t node_count;                // Nodes of Ranvier count

    // Metabolic support
    float lactate_provision;            // Energy to axon
    float metabolic_coupling;           // How tightly coupled [0-1]
    bool mitochondria_transfer;         // Transfer to neurons

    // Plasticity
    float activity_dependent_myelination; // Myelination based on activity
    float remyelination_rate;           // Recovery after damage
    float myelin_turnover;              // Natural replacement rate

    // Network reference
    oligodendrocyte_network_t* oligo_net; // Parent network
    uint32_t assigned_oligo_id;         // Specific oligodendrocyte
} module_oligodendrocyte_integration_t;

// Oligodendrocyte callbacks
typedef void (*oligo_myelination_callback_t)(void* module, bool myelinated, float g_ratio);
typedef void (*oligo_velocity_callback_t)(void* module, float new_velocity);
typedef void (*oligo_metabolic_callback_t)(void* module, float lactate_level);

// Integration functions
nimcp_status_t module_register_with_oligodendrocyte(
    void* module,
    oligodendrocyte_network_t* network,
    const module_oligodendrocyte_integration_t* config);

nimcp_status_t module_request_myelination(
    void* module,
    uint32_t axon_id,
    float activity_level);        // High activity promotes myelination

nimcp_status_t module_get_conduction_velocity(
    void* module,
    uint32_t axon_id,
    float* velocity_out);

nimcp_status_t module_optimize_g_ratio(
    void* module,
    uint32_t axon_id,
    float target_g_ratio);        // Optimize toward 0.6-0.8
```

#### 20.4 Tripartite Synapse Integration

```c
// Complete tripartite synapse model (neuron + astrocyte + microglia)
typedef struct {
    // Pre-synaptic neuron
    uint32_t presynaptic_neuron_id;
    float presynaptic_activity;
    float glutamate_release;

    // Post-synaptic neuron
    uint32_t postsynaptic_neuron_id;
    float postsynaptic_response;
    float synaptic_strength;

    // Astrocyte component
    uint32_t perisynaptic_astrocyte_id;
    float astrocyte_calcium;
    float gliotransmitter_release;
    float synaptic_gain;

    // Microglia surveillance
    uint32_t surveilling_microglia_id;
    bool tagged_for_pruning;
    float complement_level;

    // Oligodendrocyte (for axon health)
    uint32_t axon_oligo_id;
    float conduction_speed;

    // Integrated synapse dynamics
    float total_synaptic_efficacy;    // Combined modulation
    float long_term_stability;         // Survival probability
    float metabolic_cost;              // Energy requirements
} tripartite_synapse_t;

// Tripartite synapse operations
nimcp_status_t tripartite_synapse_create(
    glial_integration_t* glial,
    uint32_t pre_id,
    uint32_t post_id,
    tripartite_synapse_t* synapse_out);

nimcp_status_t tripartite_synapse_update(
    glial_integration_t* glial,
    tripartite_synapse_t* synapse,
    float dt);                         // Time step

nimcp_status_t tripartite_synapse_get_efficacy(
    const tripartite_synapse_t* synapse,
    float* efficacy_out);              // Combined strength
```

#### 20.5 Module Glial Behavior Profile

| Module | Astrocyte Role | Microglia Role | Oligo Role |
|--------|---------------|----------------|------------|
| **Attention** | BCM threshold for focus | Prune unused attention paths | Fast attention switching |
| **Memory** | Consolidation support | Prune weak memories | Quick memory retrieval |
| **Emotion** | Emotional calcium waves | Prune maladaptive circuits | Fast emotional response |
| **Reasoning** | Energy for computation | Prune inefficient paths | Fast logical chains |
| **Executive** | Sustained attention energy | Prune distractors | Fast executive commands |
| **Theory of Mind** | Social context calcium | Prune social errors | Fast social inference |
| **Ethics** | Value stability | Prune contradictions | Fast moral judgments |
| **Self Model** | Self-coherence | Prune self-inconsistencies | Fast self-updates |
| **Introspection** | Meta-awareness energy | Prune meta-errors | Fast introspective access |
| **Curiosity** | Novelty-driven waves | Prune stale interests | Fast curiosity signals |
| **Wellbeing** | Homeostatic balance | Clear stress markers | Stable wellbeing signals |
| **Salience** | Salience modulation | Prune low-salience | Fast salience detection |
| **Epistemic** | Confidence calibration | Prune overconfidence | Fast belief updates |
| **Meta Learning** | Learning rate modulation | Prune bad strategies | Fast strategy switching |
| **Mirror Neurons** | Imitation support | Prune poor matches | Fast mirror responses |
| **Bias Detection** | Bias awareness | Prune systematic errors | Fast bias detection |
| **Working Memory** | WM maintenance energy | Prune old WM items | Fast WM access |

#### 20.6 Glial Bio-Async Messages

```c
// Glial-specific bio-async message types
typedef enum {
    // Astrocyte messages
    BIO_MSG_ASTRO_CALCIUM_WAVE,         // Calcium wave propagation
    BIO_MSG_ASTRO_GLIOTRANSMITTER,      // Glutamate/D-serine release
    BIO_MSG_ASTRO_BCM_UPDATE,           // BCM threshold change
    BIO_MSG_ASTRO_METABOLIC_SUPPORT,    // Energy delivery
    BIO_MSG_ASTRO_NETWORK_SYNC,         // Astrocyte network synchrony

    // Microglia messages
    BIO_MSG_MICRO_STATE_CHANGE,         // Ramified/activated/phagocytic
    BIO_MSG_MICRO_C1Q_TAG,              // Synapse tagged for pruning
    BIO_MSG_MICRO_C3_OPSONIZE,          // Ready for phagocytosis
    BIO_MSG_MICRO_PRUNE_SYNAPSE,        // Pruning executed
    BIO_MSG_MICRO_CYTOKINE_RELEASE,     // IL-1β, TNF-α, IL-6, IL-10, TGF-β
    BIO_MSG_MICRO_DEBRIS_CLEARED,       // Cleanup complete

    // Oligodendrocyte messages
    BIO_MSG_OLIGO_MYELINATE,            // Myelination request
    BIO_MSG_OLIGO_MYELINATION_COMPLETE, // Myelination done
    BIO_MSG_OLIGO_G_RATIO_UPDATE,       // G-ratio optimization
    BIO_MSG_OLIGO_VELOCITY_CHANGE,      // Conduction speed update
    BIO_MSG_OLIGO_METABOLIC_SUPPORT,    // Lactate shuttle

    // Integrated glial messages
    BIO_MSG_GLIAL_TRIPARTITE_UPDATE,    // Full tripartite synapse update
    BIO_MSG_GLIAL_NETWORK_STATUS,       // Overall glial health
    BIO_MSG_GLIAL_INFLAMMATION,         // Inflammatory state
    BIO_MSG_GLIAL_NEUROPROTECTION       // Protective mode active
} glial_bio_msg_type_t;

// Message routing
nimcp_status_t glial_route_to_module(
    glial_integration_t* glial,
    void* module,
    glial_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size);
```

#### 20.7 Glial-Sleep Interaction

```c
// Glial behavior during sleep states
typedef struct {
    // Awake state
    float awake_astro_activity;         // High - support active processing
    float awake_micro_surveillance;     // Moderate - routine monitoring
    float awake_oligo_activity;         // Normal myelination maintenance

    // Drowsy state
    float drowsy_astro_slowdown;        // Reduce calcium dynamics
    float drowsy_micro_reduction;       // Decrease surveillance

    // Light NREM state
    float light_nrem_astro_sorting;     // Help sort memories
    float light_nrem_micro_tagging;     // Tag synapses for review

    // Deep NREM state (critical for glial function!)
    float deep_nrem_astro_clearance;    // Glymphatic clearance boost
    float deep_nrem_micro_pruning;      // Maximum pruning activity
    float deep_nrem_oligo_repair;       // Myelin repair window

    // REM state
    float rem_astro_creativity;         // Support novel associations
    float rem_micro_quiescence;         // Reduce pruning
} glial_sleep_profile_t;

// Deep NREM is critical for glymphatic system!
// - Astrocytes shrink by ~60%, opening perivascular spaces
// - CSF flushes through, clearing metabolic waste
// - Amyloid-beta and tau clearance accelerated
```

#### 20.8 Glial Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/glial/test_astrocyte_module_integration.cpp` | 30 |
| `test/integration/glial/test_microglia_module_integration.cpp` | 30 |
| `test/integration/glial/test_oligodendrocyte_module_integration.cpp` | 25 |
| `test/integration/glial/test_tripartite_synapse_integration.cpp` | 25 |
| `test/integration/glial/test_glial_bioasync_integration.cpp` | 25 |
| `test/integration/glial/test_glial_sleep_integration.cpp` | 20 |
| `test/integration/glial/test_glial_immune_integration.cpp` | 20 |
| `test/integration/glial/test_glial_plasticity_integration.cpp` | 15 |
| **Total** | **190** |

### 21. Quantum Module Integration

Every module must integrate with the quantum subsystem for:
- **Quantum Walk** for accelerated neuromodulator diffusion (√N speedup)
- **Quantum Annealing** for escaping local minima in weight optimization
- **Quantum Reasoning** for Grover-inspired inference and SAT solving
- **Quantum Monte Carlo** for efficient sampling and measurement
- **Quantum Consensus** for swarm decision-making (Byzantine fault-tolerant)

**Est. LOC**: ~24,000 | **Tests**: ~210

#### 21.1 Quantum Walk Integration Pattern

```c
// Quantum walk integration for neuromodulator diffusion
typedef struct {
    // Quantum walker configuration
    quantum_walk_config_t walker_config;
    quantum_walker_t* walker;               // Active walker instance

    // Neuromodulator targets
    bool enable_dopamine_diffusion;         // Use QW for dopamine
    bool enable_serotonin_diffusion;        // Use QW for serotonin
    bool enable_acetylcholine_diffusion;    // Use QW for ACh
    bool enable_norepinephrine_diffusion;   // Use QW for NE

    // Quantum walk parameters
    quantum_coin_type_t coin_type;          // HADAMARD, GROVER, FOURIER, IDENTITY
    uint32_t walk_steps;                    // Evolution steps
    float hybrid_mixing;                    // Quantum-classical mixing [0-1]
    float decoherence_rate;                 // Rate of quantum → classical

    // Performance metrics
    float speedup_achieved;                 // Measured speedup vs classical
    float spreading_efficiency;             // Distance/steps ratio
    uint64_t total_walk_steps;              // Total evolution steps

    // Network reference
    neural_network_t network;               // Neural network graph
} module_quantum_walk_integration_t;

// Module quantum walk callbacks
typedef void (*quantum_diffusion_callback_t)(void* module, const float* concentration, uint32_t num_nodes);
typedef void (*quantum_measurement_callback_t)(void* module, uint32_t measured_node);

// Integration functions
nimcp_status_t module_init_quantum_walk(
    void* module,
    neural_network_t network,
    const quantum_walk_config_t* config);

nimcp_status_t module_diffuse_neuromodulator(
    void* module,
    uint32_t source_node,
    neuromodulator_type_t neuromod,
    float* concentration_out);

nimcp_status_t module_quantum_walk_step(
    void* module,
    uint32_t num_steps);

nimcp_status_t module_quantum_walk_measure(
    void* module,
    uint32_t* result_node);
```

#### 21.2 Quantum Annealing Integration Pattern

```c
// Quantum annealing integration for weight optimization
typedef struct {
    // Annealer configuration
    quantum_annealing_config_t anneal_config;
    quantum_annealer_t annealer;            // Active annealer instance

    // Optimization targets
    bool enable_weight_optimization;         // Optimize synaptic weights
    bool enable_threshold_optimization;      // Optimize neuron thresholds
    bool enable_structure_optimization;      // Optimize connectivity

    // Annealing parameters
    cooling_schedule_t cooling_schedule;     // EXPONENTIAL, LINEAR, LOGARITHMIC, ADAPTIVE
    float initial_temperature;               // Starting exploration (typical: 1.0)
    float final_temperature;                 // Final exploitation (typical: 0.01)
    uint32_t num_iterations;                 // Annealing steps
    float quantum_strength;                  // Tunneling probability [0-1]
    bool enable_tunneling;                   // Quantum tunneling enabled

    // Trigger conditions
    uint32_t optimization_interval;          // Steps between optimizations
    float loss_plateau_threshold;            // Trigger on loss plateau
    float gradient_noise_threshold;          // Trigger on gradient noise

    // Results
    float best_energy_found;                 // Lowest energy achieved
    float improvement_over_classical;        // % improvement vs SA
    uint32_t tunneling_events;               // Count of successful tunnels
} module_quantum_annealing_integration_t;

// Energy function for module-specific optimization
typedef float (*module_energy_fn_t)(const float* weights, uint32_t dim, void* module);

// Integration functions
nimcp_status_t module_init_quantum_annealer(
    void* module,
    const quantum_annealing_config_t* config);

nimcp_status_t module_optimize_weights_quantum(
    void* module,
    const float* initial_weights,
    float* optimized_weights,
    uint32_t num_weights,
    module_energy_fn_t energy_fn);

nimcp_status_t module_should_anneal(
    void* module,
    float current_loss,
    float gradient_norm,
    bool* should_anneal_out);
```

#### 21.3 Quantum Reasoning Integration Pattern

```c
// Quantum reasoning integration for logical inference
typedef struct {
    // Reasoner configuration
    qreason_config_t reason_config;
    qreason_t reasoner;                      // Active reasoner instance

    // Inference modes
    bool enable_sat_solving;                 // Grover-inspired SAT
    bool enable_forward_chaining;            // Rule-based inference
    bool enable_ternary_logic;               // 3-valued logic
    bool enable_belief_propagation;          // Ternary BP

    // Grover parameters
    uint32_t max_grover_iterations;          // Grover iteration limit
    bool use_classical_fallback;             // Fall back to DPLL for large problems
    uint32_t quantum_variable_limit;         // Max vars for quantum (16)

    // Ternary belief propagation
    ternary_bp_config_t bp_config;
    float believe_threshold;                 // >= this is BELIEVE (0.7)
    float disbelieve_threshold;              // <= this is DISBELIEVE (0.3)

    // Knowledge base
    uint32_t num_facts;                      // Facts in KB
    uint32_t num_rules;                      // Rules in KB
    float min_confidence;                    // Minimum confidence threshold

    // Statistics
    uint64_t queries_performed;
    uint64_t satisfiable_count;
    float avg_grover_iterations;
} module_quantum_reasoning_integration_t;

// Integration functions
nimcp_status_t module_init_quantum_reasoner(
    void* module,
    const qreason_config_t* config);

nimcp_status_t module_add_fact(
    void* module,
    uint32_t variable,
    qreason_truth_t value,
    float confidence);

nimcp_status_t module_add_rule(
    void* module,
    const uint32_t* antecedents,
    uint32_t n_antecedents,
    uint32_t consequent,
    float confidence);

nimcp_status_t module_solve_sat(
    void* module,
    const qreason_cnf_t* cnf,
    qreason_result_t* result);

nimcp_status_t module_query_belief(
    void* module,
    uint32_t variable,
    qreason_truth_t* value_out,
    float* confidence_out);
```

#### 21.4 Quantum Monte Carlo Integration Pattern

```c
// Quantum Monte Carlo integration for sampling
typedef struct {
    // QMC configuration
    uint32_t default_num_shots;              // Finite measurement shots
    uint32_t default_amplitude_samples;      // Amplitude estimation samples
    uint32_t default_burnin;                 // MCMC burn-in period
    float target_acceptance_rate;            // Adaptive M-H target (0.234)

    // Sampling modes
    bool enable_importance_sampling;         // Use importance sampling
    bool enable_stratified_sampling;         // Use stratified sampling
    bool enable_adaptive_proposals;          // Learn proposal distribution

    // Entropy estimation
    bool enable_entropy_estimation;          // Estimate Shannon entropy
    bool enable_renyi_entropy;               // Estimate Rényi entropy
    uint32_t entropy_samples;                // Samples for entropy

    // Partition function
    bool enable_partition_estimation;        // Estimate Z
    float partition_temperature;             // Temperature for Z
    float partition_variance;                // Variance of Z estimate
} module_quantum_monte_carlo_integration_t;

// Integration functions
nimcp_status_t module_init_quantum_mc(
    void* module,
    const module_quantum_monte_carlo_integration_t* config);

nimcp_status_t module_sample_quantum_state(
    void* module,
    const float* amplitudes,
    uint32_t num_states,
    uint32_t num_shots,
    qmc_measurement_result_t* result);

nimcp_status_t module_estimate_entropy(
    void* module,
    const float* probabilities,
    uint32_t num_states,
    qmc_entropy_result_t* result);

nimcp_status_t module_adaptive_anneal(
    void* module,
    qmc_energy_fn energy_fn,
    const float* initial_state,
    float* optimized_state,
    uint32_t dim,
    qmc_anneal_result_t* result);
```

#### 21.5 Quantum Consensus Integration Pattern

```c
// Quantum consensus integration for swarm voting
typedef struct {
    // Consensus configuration
    quantum_consensus_config_t consensus_config;
    quantum_consensus_t consensus;           // Active consensus instance

    // Algorithm selection
    quantum_consensus_algo_t algorithm;      // GROVER, ANNEALING, WALK, HYBRID
    uint32_t grover_iterations;              // Grover iterations (0=auto √N)
    float agreement_threshold;               // Pass threshold (0.667)

    // Byzantine fault tolerance
    float bft_threshold;                     // Fault tolerance (0.333)
    bool enable_collusion_detection;         // Detect voting collusion
    bool enable_amplitude_weighting;         // Weight by confidence

    // Vote states (ternary)
    // AGREE (+1), ABSTAIN (0), DISAGREE (-1)
    float min_vote_confidence;               // Minimum confidence to count

    // Statistics
    uint64_t total_proposals;
    uint64_t proposals_passed;
    float avg_convergence_rounds;
    uint64_t collusions_detected;
} module_quantum_consensus_integration_t;

// Integration functions
nimcp_status_t module_init_quantum_consensus(
    void* module,
    const quantum_consensus_config_t* config);

nimcp_status_t module_create_proposal(
    void* module,
    const char* topic,
    float value,
    uint32_t* proposal_id_out);

nimcp_status_t module_cast_vote(
    void* module,
    uint32_t proposal_id,
    quantum_vote_choice_t choice,
    float confidence);

nimcp_status_t module_run_consensus(
    void* module,
    uint32_t proposal_id,
    quantum_consensus_result_t* result);
```

#### 21.6 Module Quantum Behavior Profile

| Module | Quantum Walk | Quantum Annealing | Quantum Reasoning | Quantum MC | Quantum Consensus |
|--------|-------------|-------------------|-------------------|------------|-------------------|
| **Attention** | Focus spread | Attention optimization | Priority logic | Salience sampling | - |
| **Memory** | Memory retrieval | Memory consolidation | Recall inference | Replay sampling | - |
| **Emotion** | Mood diffusion | Emotional regulation | Emotional reasoning | Affect sampling | - |
| **Reasoning** | Inference spread | Weight optimization | SAT/CNF solving | Hypothesis sampling | - |
| **Executive** | Control signals | Strategy optimization | Decision logic | Option sampling | - |
| **Theory of Mind** | Belief diffusion | Social optimization | Belief inference | Mental state sampling | - |
| **Ethics** | Value diffusion | Moral optimization | Ethical reasoning | Value sampling | Ethical voting |
| **Self Model** | Self-awareness spread | Self optimization | Self inference | State sampling | - |
| **Introspection** | Meta spread | Meta optimization | Meta reasoning | Meta sampling | - |
| **Curiosity** | Interest spread | Exploration optimization | Novelty logic | Exploration sampling | - |
| **Wellbeing** | Homeostatic spread | Balance optimization | Wellbeing logic | State sampling | - |
| **Swarm** | Collective spread | Collective optimization | Collective reasoning | Collective sampling | Swarm voting |
| **Collective** | Inter-node spread | Group optimization | Group inference | Group sampling | Group consensus |

#### 21.7 Quantum Bio-Async Messages

```c
// Quantum-specific bio-async message types
typedef enum {
    // Quantum Walk messages
    BIO_MSG_QUANTUM_WALK_INIT,              // Initialize walker on network
    BIO_MSG_QUANTUM_WALK_STEP,              // Evolve walker one step
    BIO_MSG_QUANTUM_WALK_MEASURE,           // Measure walker position
    BIO_MSG_QUANTUM_WALK_DISTRIBUTION,      // Get probability distribution
    BIO_MSG_QUANTUM_DIFFUSION_RESULT,       // Neuromodulator diffusion result

    // Quantum Annealing messages
    BIO_MSG_QUANTUM_ANNEAL_START,           // Begin annealing
    BIO_MSG_QUANTUM_ANNEAL_STEP,            // Single annealing step
    BIO_MSG_QUANTUM_ANNEAL_RESULT,          // Optimization result
    BIO_MSG_QUANTUM_TUNNELING_EVENT,        // Successful tunneling

    // Quantum Reasoning messages
    BIO_MSG_QUANTUM_REASON_QUERY,           // Query knowledge base
    BIO_MSG_QUANTUM_REASON_RESULT,          // Query result
    BIO_MSG_QUANTUM_SAT_SOLVE,              // Solve SAT problem
    BIO_MSG_QUANTUM_SAT_RESULT,             // SAT solution
    BIO_MSG_QUANTUM_BELIEF_UPDATE,          // Belief propagation update

    // Quantum Monte Carlo messages
    BIO_MSG_QUANTUM_MC_SAMPLE,              // Request sampling
    BIO_MSG_QUANTUM_MC_RESULT,              // Sampling result
    BIO_MSG_QUANTUM_ENTROPY_ESTIMATE,       // Entropy estimation
    BIO_MSG_QUANTUM_PARTITION_ESTIMATE,     // Partition function estimate

    // Quantum Consensus messages
    BIO_MSG_QUANTUM_PROPOSE,                // Create proposal
    BIO_MSG_QUANTUM_VOTE,                   // Cast vote
    BIO_MSG_QUANTUM_CONSENSUS_RUN,          // Run consensus algorithm
    BIO_MSG_QUANTUM_CONSENSUS_RESULT,       // Consensus outcome
    BIO_MSG_QUANTUM_COLLUSION_DETECTED      // Collusion alert
} quantum_bio_msg_type_t;

// Message routing
nimcp_status_t quantum_route_to_module(
    void* quantum_system,
    void* module,
    quantum_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size);
```

#### 21.8 Quantum-Cognitive Synergies

```c
// Special quantum-cognitive integration patterns
typedef struct {
    // Quantum Walk + Attention
    // Use quantum walk to spread attention across neural network
    float attention_quantum_speedup;         // Speedup from QW

    // Quantum Annealing + Plasticity
    // Use QA to escape local minima during learning
    float plasticity_improvement;            // % improvement from QA

    // Quantum Reasoning + Executive
    // Use Grover for fast decision search
    uint32_t decision_grover_iterations;     // Grover iterations for decisions

    // Quantum MC + Introspection
    // Use QMC for efficient self-state sampling
    float introspection_sampling_efficiency; // Efficiency vs classical

    // Quantum Consensus + Collective
    // Use QC for distributed swarm decisions
    float consensus_convergence_speedup;     // √N speedup from Grover
} quantum_cognitive_synergy_t;

// Synergy initialization
nimcp_status_t quantum_init_cognitive_synergies(
    void* brain,
    quantum_cognitive_synergy_t* synergies);
```

#### 21.9 Quantum Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/quantum/test_quantum_walk_module_integration.cpp` | 30 |
| `test/integration/quantum/test_quantum_annealing_module_integration.cpp` | 30 |
| `test/integration/quantum/test_quantum_reasoning_module_integration.cpp` | 30 |
| `test/integration/quantum/test_quantum_mc_module_integration.cpp` | 25 |
| `test/integration/quantum/test_quantum_consensus_module_integration.cpp` | 25 |
| `test/integration/quantum/test_quantum_bioasync_integration.cpp` | 25 |
| `test/integration/quantum/test_quantum_cognitive_synergy_integration.cpp` | 20 |
| `test/integration/quantum/test_quantum_ternary_integration.cpp` | 25 |
| **Total** | **210** |

### 22. Training Module Integration

**Priority**: Critical
**Dependencies**: All cognitive modules, SNN system, Plasticity system
**Est. LOC**: 26,000 | **Tests**: 230

The Training Module provides comprehensive learning infrastructure that must integrate with all cognitive modules to enable biologically-plausible learning across the entire system.

#### 22.1 Training Module Components

| Component | File | Purpose |
|-----------|------|---------|
| Training Module Core | `nimcp_training_module.h/c` | Core training infrastructure with phases T1-T4 |
| Gradient Manager | `nimcp_gradient_manager.h/c` | Accumulation, scaling, health checking |
| Optimizers | `nimcp_optimizers.h/c` | SGD, Adam, AdamW, RMSprop, AdaGrad, Nadam |
| Loss Functions | `nimcp_loss_functions.h/c` | MSE, Cross-Entropy, KL, Huber, Focal, Triplet |
| LR Scheduler | `nimcp_lr_scheduler.h/c` | Step, Cosine, OneCycle, ReduceOnPlateau |
| Curriculum Learning | `nimcp_curriculum_learning.h/c` | Self-Paced, Teacher-Guided, Uncertainty |
| Regularization | `nimcp_regularization.h/c` | L1, L2, Dropout, Weight Decay |

#### 22.2 Training Types

```c
// Training types from nimcp_training_module.h
typedef enum {
    NIMCP_TRAINING_STDP = 0,          // Spike-timing dependent plasticity
    NIMCP_TRAINING_DENDRITIC,         // Dendritic compartment learning
    NIMCP_TRAINING_PREDICTIVE,        // Predictive coding learning
    NIMCP_TRAINING_BCM,               // Bienenstock-Cooper-Munro rule
    NIMCP_TRAINING_HOMEOSTATIC,       // Homeostatic plasticity
    NIMCP_TRAINING_ELIGIBILITY,       // Eligibility trace-based learning
    NIMCP_TRAINING_BRAIN_LEARNING,    // Combined brain-inspired learning
    NIMCP_TRAINING_BIOLOGICAL,        // Full biological realism
    NIMCP_TRAINING_TYPE_COUNT
} nimcp_training_type_t;

// Training phases
typedef enum {
    NIMCP_TRAINING_PHASE_T1,          // Homeostatic (stabilization)
    NIMCP_TRAINING_PHASE_T2,          // Dendritic (compartment learning)
    NIMCP_TRAINING_PHASE_T3,          // Predictive (error minimization)
    NIMCP_TRAINING_PHASE_T4,          // Meta-learning (learning to learn)
    NIMCP_TRAINING_PHASE_COUNT
} nimcp_training_phase_t;
```

#### 22.3 Optimizer Integration

```c
// Optimizer types
typedef enum {
    NIMCP_OPTIMIZER_SGD = 0,          // Vanilla SGD
    NIMCP_OPTIMIZER_SGD_MOMENTUM,     // SGD with momentum
    NIMCP_OPTIMIZER_NESTEROV,         // Nesterov accelerated gradient
    NIMCP_OPTIMIZER_ADAGRAD,          // Adaptive gradient
    NIMCP_OPTIMIZER_RMSPROP,          // Root mean square propagation
    NIMCP_OPTIMIZER_ADAM,             // Adaptive moment estimation
    NIMCP_OPTIMIZER_ADAMW,            // Adam with decoupled weight decay
    NIMCP_OPTIMIZER_NADAM,            // Nesterov Adam
    NIMCP_OPTIMIZER_CUSTOM,           // User-defined optimizer
    NIMCP_OPTIMIZER_TYPE_COUNT
} nimcp_optimizer_type_t;

// Adam configuration example
typedef struct {
    float learning_rate;     // Default: 0.001
    float beta1;             // Default: 0.9
    float beta2;             // Default: 0.999
    float epsilon;           // Default: 1e-8
    float weight_decay;      // L2 regularization
    bool amsgrad;            // Use AMSGrad variant
} nimcp_adam_config_t;
```

#### 22.4 Loss Function Integration

```c
// Loss function types
typedef enum {
    NIMCP_LOSS_MSE = 0,               // Mean squared error
    NIMCP_LOSS_MAE,                   // Mean absolute error
    NIMCP_LOSS_CROSS_ENTROPY,         // Cross-entropy loss
    NIMCP_LOSS_BINARY_CROSS_ENTROPY,  // Binary cross-entropy
    NIMCP_LOSS_KL_DIVERGENCE,         // Kullback-Leibler divergence
    NIMCP_LOSS_HUBER,                 // Huber loss (smooth L1)
    NIMCP_LOSS_HINGE,                 // Hinge loss (SVM)
    NIMCP_LOSS_LOG_COSH,              // Log-cosh loss
    NIMCP_LOSS_FOCAL,                 // Focal loss (class imbalance)
    NIMCP_LOSS_CONTRASTIVE,           // Contrastive loss (similarity)
    NIMCP_LOSS_TRIPLET,               // Triplet loss (metric learning)
    NIMCP_LOSS_CUSTOM,                // User-defined loss
    NIMCP_LOSS_TYPE_COUNT
} nimcp_loss_type_t;

// Reduction modes for loss computation
typedef enum {
    NIMCP_LOSS_REDUCE_MEAN = 0,       // Average over batch
    NIMCP_LOSS_REDUCE_SUM,            // Sum over batch
    NIMCP_LOSS_REDUCE_NONE            // Return per-sample loss
} nimcp_loss_reduction_t;
```

#### 22.5 Learning Rate Scheduler Integration

```c
// Scheduler types
typedef enum {
    NIMCP_LR_CONSTANT = 0,            // Fixed learning rate
    NIMCP_LR_STEP,                    // Step decay
    NIMCP_LR_EXPONENTIAL,             // Exponential decay
    NIMCP_LR_COSINE_ANNEALING,        // Cosine annealing
    NIMCP_LR_LINEAR_WARMUP,           // Linear warmup
    NIMCP_LR_MULTI_STEP,              // Multi-step decay
    NIMCP_LR_REDUCE_ON_PLATEAU,       // Reduce on plateau
    NIMCP_LR_CYCLIC,                  // Cyclic learning rate
    NIMCP_LR_ONE_CYCLE,               // One-cycle policy
    NIMCP_LR_COSINE_WARMUP,           // Cosine with warmup
    NIMCP_LR_POLYNOMIAL,              // Polynomial decay
    NIMCP_LR_CUSTOM,                  // User-defined scheduler
    NIMCP_LR_TYPE_COUNT
} nimcp_lr_scheduler_type_t;
```

#### 22.6 Curriculum Learning Integration

```c
// Curriculum learning strategies
typedef enum {
    NIMCP_CURRICULUM_NONE = 0,        // No curriculum
    NIMCP_CURRICULUM_SELF_PACED,      // Self-paced learning
    NIMCP_CURRICULUM_TEACHER_GUIDED,  // Teacher-guided curriculum
    NIMCP_CURRICULUM_TRANSFER,        // Transfer curriculum
    NIMCP_CURRICULUM_UNCERTAINTY,     // Uncertainty-based sampling
    NIMCP_CURRICULUM_LOSS_BASED,      // Loss-based difficulty
    NIMCP_CURRICULUM_GRADIENT_NORM,   // Gradient norm difficulty
    NIMCP_CURRICULUM_CONFIDENCE,      // Confidence-based selection
    NIMCP_CURRICULUM_ANTI,            // Anti-curriculum (hard first)
    NIMCP_CURRICULUM_HYBRID,          // Combined strategies
    NIMCP_CURRICULUM_STRATEGY_COUNT
} nimcp_curriculum_strategy_t;

// Curriculum states
typedef enum {
    NIMCP_CURRICULUM_WARMUP = 0,      // Initial warmup phase
    NIMCP_CURRICULUM_EASY,            // Easy samples only
    NIMCP_CURRICULUM_MEDIUM,          // Medium difficulty
    NIMCP_CURRICULUM_FULL             // Full difficulty range
} nimcp_curriculum_state_t;
```

#### 22.7 Gradient Management Integration

```c
// Gradient accumulation modes
typedef enum {
    NIMCP_GRAD_ACCUM_SUM = 0,         // Sum gradients
    NIMCP_GRAD_ACCUM_MEAN             // Average gradients
} nimcp_grad_accum_mode_t;

// Gradient scaling strategies
typedef enum {
    NIMCP_GRAD_SCALE_NONE = 0,        // No scaling
    NIMCP_GRAD_SCALE_FIXED,           // Fixed scale factor
    NIMCP_GRAD_SCALE_DYNAMIC          // Dynamic loss scaling
} nimcp_grad_scale_strategy_t;

// Gradient health status
typedef enum {
    NIMCP_GRAD_HEALTHY = 0,           // All gradients valid
    NIMCP_GRAD_HAS_NAN,               // NaN detected
    NIMCP_GRAD_HAS_INF,               // Infinity detected
    NIMCP_GRAD_HAS_ZERO,              // All-zero gradients
    NIMCP_GRAD_OVERFLOW,              // Overflow detected
    NIMCP_GRAD_UNDERFLOW              // Underflow detected
} nimcp_grad_health_t;
```

#### 22.8 Training Bridge Files

**New Bridge Files**:
- `include/training/bridges/nimcp_training_cognitive_bridge.h`
- `include/training/bridges/nimcp_training_snn_bridge.h`
- `include/training/bridges/nimcp_training_fep_bridge.h`
- `include/training/bridges/nimcp_training_immune_bridge.h`
- `include/training/bridges/nimcp_training_bioasync_bridge.h`
- `src/training/bridges/nimcp_training_cognitive_bridge.c`
- `src/training/bridges/nimcp_training_snn_bridge.c`
- `src/training/bridges/nimcp_training_fep_bridge.c`
- `src/training/bridges/nimcp_training_immune_bridge.c`
- `src/training/bridges/nimcp_training_bioasync_bridge.c`

#### 22.9 Module Training Behavior

| Cognitive Module | Training Type | Optimizer | Loss Function | Curriculum |
|-----------------|---------------|-----------|---------------|------------|
| Attention | STDP | Adam | Cross-Entropy | Self-Paced |
| Memory | Dendritic | AdamW | Contrastive | Teacher-Guided |
| Emotion | BCM | SGD+Momentum | MSE | Uncertainty |
| Reasoning | Predictive | Adam | KL-Divergence | Loss-Based |
| Executive | Eligibility | AdamW | Huber | Confidence |
| Ethics | Homeostatic | Adam | Focal | Teacher-Guided |
| Curiosity | STDP | RMSprop | Triplet | Anti-Curriculum |
| Self-Model | Brain Learning | AdamW | MSE | Self-Paced |
| Introspection | Predictive | Adam | Cross-Entropy | Uncertainty |
| Wellbeing | BCM | SGD | MSE | Hybrid |
| Salience | STDP | Adam | Binary-CE | Confidence |
| Bias | Homeostatic | AdamW | KL-Divergence | Teacher-Guided |
| Epistemic | Predictive | Adam | Cross-Entropy | Loss-Based |
| Global Workspace | Eligibility | AdamW | MSE | Self-Paced |
| Meta-Learning | Biological | Adam | Triplet | Anti-Curriculum |
| ToM | Dendritic | AdamW | Cross-Entropy | Uncertainty |
| Mirror Neurons | STDP | RMSprop | Contrastive | Teacher-Guided |

#### 22.10 Training Bio-Async Messages

```c
// Training-related bio-async message types
typedef enum {
    BIO_MSG_TRAINING_START,             // Start training session
    BIO_MSG_TRAINING_STOP,              // Stop training session
    BIO_MSG_TRAINING_STEP,              // Execute training step
    BIO_MSG_TRAINING_BATCH_COMPLETE,    // Batch completed
    BIO_MSG_TRAINING_EPOCH_COMPLETE,    // Epoch completed
    BIO_MSG_TRAINING_CHECKPOINT,        // Save checkpoint
    BIO_MSG_TRAINING_RESTORE,           // Restore checkpoint
    BIO_MSG_TRAINING_LR_UPDATE,         // Learning rate updated
    BIO_MSG_TRAINING_CURRICULUM_ADVANCE,// Curriculum state advanced
    BIO_MSG_TRAINING_GRADIENT_HEALTH,   // Gradient health report
    BIO_MSG_TRAINING_LOSS_REPORT,       // Loss value report
    BIO_MSG_TRAINING_METRICS_UPDATE,    // Training metrics update
    BIO_MSG_TRAINING_PHASE_TRANSITION,  // Phase T1->T2->T3->T4
    BIO_MSG_TRAINING_OPTIMIZER_SWITCH,  // Dynamic optimizer change
    BIO_MSG_TRAINING_REGULARIZATION,    // Regularization event
    BIO_MSG_TRAINING_COUNT
} nimcp_training_msg_type_t;
```

#### 22.11 Training-Cognitive Integration API

```c
// Training integration context per module
typedef struct {
    // Training configuration
    nimcp_training_type_t training_type;
    nimcp_training_phase_t current_phase;

    // Optimizer settings
    nimcp_optimizer_context_t* optimizer;
    nimcp_optimizer_type_t optimizer_type;

    // Loss function
    nimcp_loss_context_t* loss_ctx;
    nimcp_loss_type_t loss_type;

    // Learning rate scheduler
    nimcp_lr_scheduler_t* lr_scheduler;
    nimcp_lr_scheduler_type_t scheduler_type;

    // Curriculum learning
    nimcp_curriculum_ctx_t* curriculum;
    nimcp_curriculum_strategy_t curriculum_strategy;

    // Gradient management
    nimcp_gradient_manager_ctx_t* grad_manager;

    // Bio-async integration
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Metrics
    nimcp_training_stats_t stats;
} nimcp_module_training_integration_t;

// Initialize training for a cognitive module
nimcp_status_t nimcp_training_init_for_module(
    nimcp_module_training_integration_t* training,
    const char* module_name,
    nimcp_training_type_t type,
    nimcp_optimizer_type_t optimizer,
    nimcp_loss_type_t loss,
    nimcp_curriculum_strategy_t curriculum,
    nimcp_bio_router_t* router);

// Execute training step for module
nimcp_status_t nimcp_training_module_step(
    nimcp_module_training_integration_t* training,
    float* params,
    const float* gradients,
    size_t param_count);

// Advance curriculum based on performance
nimcp_status_t nimcp_training_curriculum_advance(
    nimcp_module_training_integration_t* training,
    float performance_metric);

// Transition training phase
nimcp_status_t nimcp_training_phase_transition(
    nimcp_module_training_integration_t* training,
    nimcp_training_phase_t new_phase);
```

#### 22.12 FEP-Training Integration

```c
// Free energy training bridge
typedef struct {
    // FEP components
    float free_energy;
    float precision_weights[NIMCP_MAX_MODULES];
    float prediction_error;

    // Training adaptation
    float learning_rate_modulation;   // FEP modulates LR
    float gradient_precision;         // Precision-weighted gradients
    float surprise_signal;            // Drives curriculum

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
} nimcp_fep_training_bridge_t;

// FEP-guided learning rate adjustment
nimcp_status_t nimcp_fep_modulate_learning_rate(
    nimcp_fep_training_bridge_t* bridge,
    nimcp_module_training_integration_t* training);

// Precision-weighted gradient update
nimcp_status_t nimcp_fep_precision_gradient_update(
    nimcp_fep_training_bridge_t* bridge,
    float* gradients,
    size_t count);

// Surprise-driven curriculum advancement
nimcp_status_t nimcp_fep_surprise_curriculum(
    nimcp_fep_training_bridge_t* bridge,
    nimcp_curriculum_ctx_t* curriculum);
```

#### 22.13 Immune-Training Integration

```c
// Immune system training integration
typedef struct {
    // Immune monitoring
    nimcp_immune_sensor_t sensor;
    float gradient_health_score;      // 0-1
    float loss_stability_score;       // 0-1
    float learning_progress_score;    // 0-1

    // Threat responses
    bool gradient_nan_detected;
    bool loss_explosion_detected;
    bool catastrophic_forgetting_detected;

    // Healing actions
    nimcp_checkpoint_t* last_stable_checkpoint;
    uint32_t rollback_count;
} nimcp_immune_training_integration_t;

// Monitor training health
nimcp_status_t nimcp_immune_monitor_training(
    nimcp_immune_training_integration_t* immune,
    nimcp_module_training_integration_t* training);

// Trigger training rollback on catastrophic failure
nimcp_status_t nimcp_immune_training_rollback(
    nimcp_immune_training_integration_t* immune,
    nimcp_module_training_integration_t* training);

// Adaptive learning rate reduction on instability
nimcp_status_t nimcp_immune_lr_intervention(
    nimcp_immune_training_integration_t* immune,
    nimcp_module_training_integration_t* training,
    float reduction_factor);
```

#### 22.14 Distributed Training Integration

```c
// Distributed training configuration
typedef struct {
    uint32_t world_size;              // Number of workers
    uint32_t rank;                    // This worker's rank
    bool use_gradient_allreduce;      // Enable gradient sync
    bool use_mixed_precision;         // FP16/BF16 training
    uint32_t gradient_sync_interval;  // Steps between sync

    // Bio-async distributed messaging
    nimcp_bio_async_handler_t distributed_handler;
} nimcp_distributed_training_config_t;

// All-reduce gradients across workers
nimcp_status_t nimcp_training_allreduce_gradients(
    nimcp_distributed_training_config_t* dist_config,
    float* gradients,
    size_t count);

// Synchronize curriculum state across workers
nimcp_status_t nimcp_training_sync_curriculum(
    nimcp_distributed_training_config_t* dist_config,
    nimcp_curriculum_ctx_t* curriculum);
```

#### 22.15 Training Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/training/test_training_cognitive_bridge.cpp` | 25 |
| `test/integration/training/test_training_snn_bridge.cpp` | 25 |
| `test/integration/training/test_training_fep_bridge.cpp` | 25 |
| `test/integration/training/test_training_immune_bridge.cpp` | 20 |
| `test/integration/training/test_training_bioasync_bridge.cpp` | 20 |
| `test/integration/training/test_optimizer_cognitive_integration.cpp` | 25 |
| `test/integration/training/test_loss_module_integration.cpp` | 20 |
| `test/integration/training/test_scheduler_adaptation_integration.cpp` | 20 |
| `test/integration/training/test_curriculum_cognitive_integration.cpp` | 25 |
| `test/integration/training/test_gradient_health_integration.cpp` | 15 |
| `test/integration/training/test_distributed_training_integration.cpp` | 10 |
| **Total** | **230** |

### 23. Language Module Integration

**Priority**: Critical
**Dependencies**: Perception layer, Cognitive layer, Motor cortex, Training system
**Est. LOC**: 28,000 | **Tests**: 250

The Language Module provides comprehensive speech comprehension (Wernicke's area) and production (Broca's area) capabilities that must integrate with all cognitive modules to enable natural language understanding and generation.

#### 23.1 Language Module Architecture

```
   Perception Layer          Cognitive Layer           Training Layer
         │                         │                         │
         ▼                         ▼                         ▼
   ┌─────────────────────────────────────────────────────────────────┐
   │                  LANGUAGE ORCHESTRATOR                          │
   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐│
   │  │ Wernicke │◄─►│ Broca    │  │ NLP Core │  │ Multimodal/Spike││
   │  │ (Compre- │  │ (Produc- │  │ (Attn,   │  │ (Cross-modal,   ││
   │  │  hension)│  │  tion)   │  │ Embed)   │  │  Spiking NLP)   ││
   │  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘│
   │         │              │            │               │          │
   │         └──────────────┴────────────┴───────────────┘          │
   │                              │                                  │
   │                    State Machine                                │
   │     IDLE → LISTENING → COMPREHENDING → INTEGRATING → OUTPUT    │
   └─────────────────────────────────────────────────────────────────┘
```

#### 23.2 Language Layer Components

| Component | File | Purpose |
|-----------|------|---------|
| Language Orchestrator | `nimcp_language_orchestrator.h/c` | Central coordinator for all language processing |
| Language Types | `nimcp_language_types.h` | Shared types (phonemes, words, concepts, syntax) |
| Language Config | `nimcp_language_config.h` | Configuration structures |
| Language Bio-Async | `nimcp_language_bio_async.h` | Bio-async messaging integration |
| Wernicke Adapter | `nimcp_wernicke_adapter.h/c` | Speech comprehension (BA22) |
| Broca Adapter | `nimcp_broca_adapter.h/c` | Speech production (BA44/45) |

#### 23.3 Wernicke's Area (Comprehension)

```c
// Processing layers: Phonological → Lexical → Semantic → Syntactic
typedef struct {
    phonological_analyzer_t* phonological;  // Phoneme recognition (STG)
    lexical_access_t* lexical;              // Word recognition (MTG)
    semantic_integrator_t* semantic;        // Meaning retrieval (ATL)
    syntactic_comprehension_t* syntactic;   // Phrase structure (BA45)
} wernicke_layers_t;

// Comprehension result
typedef struct {
    language_word_t* words;           // Recognized words
    uint32_t word_count;
    language_concept_t* concepts;     // Activated concepts
    uint32_t concept_count;
    language_parse_node_t* parse_tree;// Syntactic parse
    float* semantic_vector;           // Sentence embedding
    float semantic_anomaly;           // N400-like signal
    float syntactic_anomaly;          // P600-like signal
} language_comprehension_result_t;
```

#### 23.4 Broca's Area (Production)

```c
// Processing pipeline: Semantic → Syntactic → Phonological → Motor
typedef struct {
    syntax_processor_t* syntax;          // Syntactic structure building
    phonological_processor_t* phonological; // Phoneme sequence planning
    speech_motor_planner_t* motor;       // Motor command generation
} broca_layers_t;

// Production plan
typedef struct {
    language_word_t* words;              // Planned word sequence
    language_phoneme_t* phonemes;        // Planned phoneme sequence
    language_motor_command_t* commands;  // Motor commands for articulation
    language_prosody_t prosody;          // Prosodic contour
    float fluency_score;
} language_production_plan_t;

// Broca processing status
typedef enum {
    BROCA_STATUS_IDLE,
    BROCA_STATUS_LEXICAL_ACCESS,
    BROCA_STATUS_SYNTACTIC,
    BROCA_STATUS_PHONOLOGICAL,
    BROCA_STATUS_MOTOR_PLANNING,
    BROCA_STATUS_READY,
    BROCA_STATUS_ERROR
} broca_status_t;
```

#### 23.5 Language Processing States

```c
// Language layer state machine
typedef enum {
    LANGUAGE_STATE_IDLE = 0,          // No active processing
    LANGUAGE_STATE_LISTENING,         // Receiving perceptual input
    LANGUAGE_STATE_COMPREHENDING,     // Processing through Wernicke
    LANGUAGE_STATE_INTEGRATING,       // Cross-module integration
    LANGUAGE_STATE_GENERATING,        // Generating through Broca
    LANGUAGE_STATE_PRODUCING,         // Motor output preparation
    LANGUAGE_STATE_ERROR
} language_state_t;

// Language processing modes
typedef enum {
    LANGUAGE_MODE_COMPREHENSION,      // Input processing only
    LANGUAGE_MODE_PRODUCTION,         // Output generation only
    LANGUAGE_MODE_DIALOGUE,           // Full bidirectional
    LANGUAGE_MODE_REPETITION,         // Direct echo (arcuate fasciculus)
    LANGUAGE_MODE_TRANSLATION         // Cross-language processing
} language_mode_t;
```

#### 23.6 Phonological Types

```c
// Phoneme categories (IPA-based)
typedef enum {
    PHONEME_CAT_VOWEL,               // Vowel sounds
    PHONEME_CAT_STOP,                // Stop consonants (p, b, t, d, k, g)
    PHONEME_CAT_FRICATIVE,           // Fricatives (f, v, s, z, etc.)
    PHONEME_CAT_AFFRICATE,           // Affricates (ch, j)
    PHONEME_CAT_NASAL,               // Nasals (m, n, ng)
    PHONEME_CAT_APPROXIMANT,         // Approximants (l, r, w, y)
    PHONEME_CAT_SILENCE
} phoneme_category_t;

// Phoneme representation
typedef struct {
    uint32_t id;                     // IPA code
    phoneme_category_t category;
    float confidence;                // Recognition confidence [0-1]
    float duration_ms;
    float formants[4];               // F1-F4 frequencies
    float pitch_hz;                  // Fundamental frequency (F0)
    bool is_stressed;                // Prosodic stress
    bool is_word_boundary;
} language_phoneme_t;
```

#### 23.7 Semantic/Syntactic Types

```c
// Thematic roles (Case Grammar)
typedef enum {
    THEMATIC_ROLE_AGENT,             // Doer of action
    THEMATIC_ROLE_PATIENT,           // Affected by action
    THEMATIC_ROLE_THEME,             // Entity moved/changed
    THEMATIC_ROLE_EXPERIENCER,       // Perceiver/feeler
    THEMATIC_ROLE_BENEFICIARY,       // Recipient of benefit
    THEMATIC_ROLE_INSTRUMENT,        // Tool used
    THEMATIC_ROLE_LOCATION,
    THEMATIC_ROLE_SOURCE,
    THEMATIC_ROLE_GOAL,
    THEMATIC_ROLE_TIME,
    THEMATIC_ROLE_MANNER,
    THEMATIC_ROLE_CAUSE
} thematic_role_t;

// Part of speech
typedef enum {
    POS_NOUN, POS_VERB, POS_ADJECTIVE, POS_ADVERB,
    POS_DETERMINER, POS_PREPOSITION, POS_CONJUNCTION,
    POS_PRONOUN, POS_AUXILIARY, POS_COMPLEMENTIZER
} part_of_speech_t;

// Phrase types for parsing
typedef enum {
    PHRASE_NP,   // Noun phrase
    PHRASE_VP,   // Verb phrase
    PHRASE_PP,   // Prepositional phrase
    PHRASE_AP,   // Adjective phrase
    PHRASE_S,    // Sentence
    PHRASE_SBAR, // Subordinate clause
    PHRASE_CP    // Complementizer phrase
} phrase_type_t;
```

#### 23.8 Language Layer Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| Perception Bridge | `nimcp_language_perception_bridge.h` | Audio/visual input from cortices |
| Cognitive Bridge | `nimcp_language_cognitive_bridge.h` | Working memory, attention, reasoning |
| Training Bridge | `nimcp_language_training_bridge.h` | Language learning (vocabulary, grammar) |
| Omni Bridge | `nimcp_language_omni_bridge.h` | Predictive processing (word prediction) |
| Immune Bridge | `nimcp_language_immune_bridge.h` | Inflammation effects (aphasia modeling) |
| GPU Bridge | `nimcp_language_gpu_bridge.h` | GPU acceleration |
| Thalamic Bridge | `nimcp_language_thalamic_bridge.h` | Attention gating |
| Substrate Bridge | `nimcp_language_substrate_bridge.h` | Bio-async messaging |
| Logic Bridge | `nimcp_language_logic_bridge.h` | Logical reasoning integration |
| Motor Bridge | `nimcp_language_motor_bridge.h` | Motor cortex for speech output |
| Prefrontal Bridge | `nimcp_language_prefrontal_bridge.h` | Executive control |
| Temporal Bridge | `nimcp_language_temporal_bridge.h` | Temporal lobe integration |
| Hippocampus Bridge | `nimcp_language_hippocampus_bridge.h` | Memory consolidation |
| Parietal Bridge | `nimcp_language_parietal_bridge.h` | Spatial language integration |
| Cingulate Bridge | `nimcp_language_cingulate_bridge.h` | Error monitoring |
| Insula Bridge | `nimcp_language_insula_bridge.h` | Speech motor coordination |
| Cerebellum Bridge | `nimcp_language_cerebellum_bridge.h` | Speech timing |

#### 23.9 Language-Cognitive Module Integration

| Cognitive Module | Language Integration | Direction |
|-----------------|---------------------|-----------|
| Attention | Focus on spoken/written input | Bidirectional |
| Working Memory | Phonological loop, semantic buffer | Bidirectional |
| Emotion | Prosodic modulation, emotional language | Bidirectional |
| Reasoning | Logical discourse, argumentation | Language → Cognitive |
| Executive | Speech planning, turn-taking | Bidirectional |
| Ethics | Appropriate language use | Cognitive → Language |
| Curiosity | Question generation | Cognitive → Language |
| Self-Model | Inner speech, self-talk | Bidirectional |
| Introspection | Verbalization of mental states | Cognitive → Language |
| ToM | Pragmatics, indirect speech acts | Bidirectional |
| Mirror Neurons | Articulatory mirroring | Bidirectional |
| Semantic Memory | Lexicon, conceptual knowledge | Bidirectional |
| Episodic Memory | Narrative comprehension/production | Bidirectional |

#### 23.10 Language Bio-Async Messages

```c
// Language-related bio-async message types
typedef enum {
    BIO_MSG_LANG_PHONEME_INPUT,           // Phoneme received from perception
    BIO_MSG_LANG_WORD_RECOGNIZED,         // Word recognized by Wernicke
    BIO_MSG_LANG_CONCEPT_ACTIVATED,       // Concept activated in semantic memory
    BIO_MSG_LANG_COMPREHENSION_COMPLETE,  // Utterance comprehended
    BIO_MSG_LANG_PRODUCTION_REQUEST,      // Request to produce language
    BIO_MSG_LANG_PRODUCTION_READY,        // Production plan ready
    BIO_MSG_LANG_MOTOR_COMMAND,           // Motor command for articulation
    BIO_MSG_LANG_STATE_CHANGE,            // State machine transition
    BIO_MSG_LANG_ANOMALY_DETECTED,        // Semantic/syntactic anomaly (N400/P600)
    BIO_MSG_LANG_AMBIGUITY_DETECTED,      // Ambiguous input detected
    BIO_MSG_LANG_WERNICKE_BROCA_SYNC,     // Arcuate fasciculus communication
    BIO_MSG_LANG_PROSODY_UPDATE,          // Prosodic information
    BIO_MSG_LANG_CONTEXT_UPDATE,          // Discourse context update
    BIO_MSG_LANG_TRAINING_UPDATE,         // Learning update
    BIO_MSG_LANG_COUNT
} nimcp_language_msg_type_t;
```

#### 23.11 Language-Cognitive Integration API

```c
// Language integration context for cognitive modules
typedef struct {
    // Language orchestrator reference
    language_orchestrator_t* orchestrator;

    // Comprehension interface
    bool (*process_utterance)(const char* text);
    bool (*process_phonemes)(const language_phoneme_t* phonemes, uint32_t count);
    bool (*get_comprehension)(language_comprehension_result_t* result);

    // Production interface
    bool (*generate_response)(const float* semantic_input, uint32_t dim);
    bool (*get_production_plan)(language_production_plan_t* plan);

    // Semantic interface
    bool (*activate_concept)(uint32_t concept_id, float activation);
    bool (*query_semantic)(const char* query, float* result, uint32_t dim);

    // Context interface
    bool (*update_context)(const language_context_t* context);
    bool (*get_current_context)(language_context_t* context);

    // Bio-async integration
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;
} nimcp_language_cognitive_integration_t;

// Initialize language integration for cognitive module
nimcp_status_t nimcp_language_init_cognitive_integration(
    nimcp_language_cognitive_integration_t* integration,
    language_orchestrator_t* orchestrator,
    nimcp_bio_router_t* router);

// Process natural language query through reasoning
nimcp_status_t nimcp_language_reason_over_text(
    nimcp_language_cognitive_integration_t* integration,
    const char* query,
    char* response,
    size_t max_response_len);
```

#### 23.12 Wernicke-Broca Integration (Arcuate Fasciculus)

```c
// Models the bidirectional arcuate fasciculus
typedef struct {
    // Forward: Wernicke → Broca (comprehension → production)
    float* forward_weights;
    uint32_t forward_dim;

    // Backward: Broca → Wernicke (production monitoring)
    float* backward_weights;
    uint32_t backward_dim;

    // Repetition pathway (direct bypass)
    bool repetition_mode;
    float repetition_strength;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
} nimcp_arcuate_fasciculus_t;

// Send comprehension result to Broca for response generation
nimcp_status_t nimcp_arcuate_forward(
    nimcp_arcuate_fasciculus_t* arcuate,
    const language_comprehension_result_t* comprehension,
    broca_adapter_t* broca);

// Send production feedback to Wernicke for monitoring
nimcp_status_t nimcp_arcuate_backward(
    nimcp_arcuate_fasciculus_t* arcuate,
    const language_production_plan_t* production,
    wernicke_adapter_t* wernicke);

// Direct repetition (bypasses semantic processing)
nimcp_status_t nimcp_arcuate_repeat(
    nimcp_arcuate_fasciculus_t* arcuate,
    const language_phoneme_t* input_phonemes,
    uint32_t input_count,
    language_phoneme_t* output_phonemes,
    uint32_t* output_count);
```

#### 23.13 Aphasia Modeling (Immune Integration)

```c
// Models language disorders from neural damage/inflammation
typedef struct {
    // Wernicke's aphasia (comprehension deficit)
    float wernicke_damage;           // 0-1 damage level
    bool fluent_but_meaningless;     // Word salad
    bool poor_comprehension;
    bool neologisms;

    // Broca's aphasia (production deficit)
    float broca_damage;              // 0-1 damage level
    bool non_fluent_speech;
    bool preserved_comprehension;
    bool agrammatism;

    // Conduction aphasia (arcuate damage)
    float arcuate_damage;            // 0-1 damage level
    bool impaired_repetition;

    // Immune influence
    float il1b_level;                // Pro-inflammatory
    float tnf_alpha_level;           // Pro-inflammatory
    float il10_level;                // Anti-inflammatory

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
} nimcp_aphasia_model_t;

// Apply inflammation effects to language processing
nimcp_status_t nimcp_language_apply_inflammation(
    language_orchestrator_t* orchestrator,
    const nimcp_aphasia_model_t* aphasia);

// Recover from aphasia through training
nimcp_status_t nimcp_language_aphasia_recovery(
    language_orchestrator_t* orchestrator,
    nimcp_aphasia_model_t* aphasia,
    float recovery_rate);
```

#### 23.14 Language Training Integration

```c
// Language learning configuration
typedef struct {
    // Vocabulary learning
    bool enable_vocabulary_learning;
    float vocabulary_learning_rate;

    // Grammar learning
    bool enable_grammar_learning;
    float grammar_learning_rate;

    // Phoneme learning
    bool enable_phoneme_learning;
    float phoneme_learning_rate;

    // Semantic learning
    bool enable_semantic_learning;
    float semantic_learning_rate;

    // Curriculum
    nimcp_curriculum_strategy_t curriculum;
    nimcp_curriculum_state_t curriculum_state;

    // Training stats
    uint64_t vocabulary_size;
    uint64_t grammar_rules_learned;
    float comprehension_accuracy;
    float production_fluency;
} nimcp_language_training_config_t;

// Initialize language learning
nimcp_status_t nimcp_language_init_training(
    language_orchestrator_t* orchestrator,
    const nimcp_language_training_config_t* config);

// Learn from comprehension feedback
nimcp_status_t nimcp_language_learn_from_feedback(
    language_orchestrator_t* orchestrator,
    const language_comprehension_result_t* expected,
    const language_comprehension_result_t* actual);

// Learn from production feedback
nimcp_status_t nimcp_language_learn_production(
    language_orchestrator_t* orchestrator,
    const language_production_plan_t* plan,
    float feedback_score);
```

#### 23.15 Language Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/language/test_language_orchestrator_integration.cpp` | 30 |
| `test/integration/language/test_wernicke_broca_integration.cpp` | 25 |
| `test/integration/language/test_language_perception_bridge.cpp` | 20 |
| `test/integration/language/test_language_cognitive_bridge.cpp` | 25 |
| `test/integration/language/test_language_training_bridge.cpp` | 20 |
| `test/integration/language/test_language_omni_bridge.cpp` | 20 |
| `test/integration/language/test_language_immune_bridge.cpp` | 15 |
| `test/integration/language/test_language_motor_bridge.cpp` | 15 |
| `test/integration/language/test_arcuate_fasciculus_integration.cpp` | 20 |
| `test/integration/language/test_aphasia_modeling.cpp` | 15 |
| `test/integration/language/test_language_bioasync_integration.cpp` | 20 |
| `test/integration/language/test_language_semantic_memory_integration.cpp` | 25 |
| **Total** | **250** |

### 24. Perception Module Integration

**Priority**: Critical
**Dependencies**: Thalamus, Cognitive layer, Training system, Bio-async
**Est. LOC**: 30,000 | **Tests**: 270

The Perception Module provides comprehensive sensory processing (visual, auditory, speech) that must integrate with all cognitive modules to enable multimodal perception and grounding.

#### 24.1 Perception Module Architecture

```
                          SENSORY INPUT
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
   ┌─────────┐           ┌─────────┐           ┌─────────┐
   │ RETINA  │           │ COCHLEA │           │  OTHER  │
   │ (UV/NIR │           │ (BM/HC/ │           │ (Touch/ │
   │ Thermal)│           │  ANF)   │           │  Smell) │
   └────┬────┘           └────┬────┘           └────┬────┘
        │                      │                      │
        ▼                      ▼                      ▼
   ┌─────────┐           ┌─────────┐           ┌─────────┐
   │ VISUAL  │           │ AUDIO   │           │ SPEECH  │
   │ CORTEX  │           │ CORTEX  │           │ CORTEX  │
   │ (V1-V5) │           │  (A1)   │           │(STG/BA) │
   └────┬────┘           └────┬────┘           └────┬────┘
        │                      │                      │
        └──────────────────────┼──────────────────────┘
                               │
                               ▼
                    ┌──────────────────┐
                    │ OCCIPITAL ADAPTER│
                    │ (Dorsal/Ventral  │
                    │     Streams)     │
                    └────────┬─────────┘
                             │
          ┌──────────────────┼──────────────────┐
          ▼                  ▼                  ▼
    ┌──────────┐      ┌──────────┐      ┌──────────┐
    │ Cognitive│      │ Language │      │ Training │
    │   Layer  │      │  Layer   │      │  Layer   │
    └──────────┘      └──────────┘      └──────────┘
```

#### 24.2 Perception Layer Components

| Component | File | Purpose |
|-----------|------|---------|
| Visual Cortex | `nimcp_visual_cortex.h/c` | V1-style visual processing (CNN/Gabor) |
| Audio Cortex | `nimcp_audio_cortex.h/c` | Auditory processing (FFT, MFCC) |
| Speech Cortex | `nimcp_speech_cortex.h/c` | Phoneme recognition, formant analysis |
| Retina | `nimcp_retina.h/c` | Extended-spectrum (UV/NIR/Thermal-IR) |
| Cochlea | `nimcp_cochlea.h/c` | Complete auditory front-end (BM/HC/ANF) |
| Occipital Adapter | `nimcp_occipital_adapter.h/c` | Visual hierarchy (V1-V5) |
| Basilar Membrane | `nimcp_basilar_membrane.h/c` | Frequency decomposition |
| Hair Cells | `nimcp_hair_cells.h/c` | Mechano-electrical transduction |
| Auditory Nerve | `nimcp_auditory_nerve.h/c` | Spike generation |

#### 24.3 Visual Cortex (V1-V5 Hierarchy)

```c
// Visual processing areas (Brodmann areas)
typedef enum {
    VISUAL_AREA_V1,      // Primary visual cortex (BA17) - edges, orientation
    VISUAL_AREA_V2,      // Secondary visual (BA18) - contours, texture
    VISUAL_AREA_V3,      // Dynamic form processing (BA19)
    VISUAL_AREA_V4,      // Color constancy, complex form (BA19)
    VISUAL_AREA_V5_MT    // Motion detection, optic flow (BA19/37)
} visual_area_t;

// Visual processing streams
typedef enum {
    VISUAL_STREAM_DORSAL,   // "Where" pathway: V1 → V2 → V3 → V5/MT → Parietal
    VISUAL_STREAM_VENTRAL,  // "What" pathway: V1 → V2 → V4 → Temporal
    VISUAL_STREAM_BOTH
} visual_stream_t;

// Feature types detected
typedef enum {
    VISUAL_FEATURE_EDGE,          // Edge/contour
    VISUAL_FEATURE_ORIENTATION,   // Orientation (0-180°)
    VISUAL_FEATURE_COLOR,         // Color (hue, saturation)
    VISUAL_FEATURE_TEXTURE,       // Texture pattern
    VISUAL_FEATURE_MOTION,        // Motion direction/speed
    VISUAL_FEATURE_DEPTH,         // Depth/disparity
    VISUAL_FEATURE_FORM,          // Shape/form
    VISUAL_FEATURE_FACE,          // Face-specific
    VISUAL_FEATURE_OBJECT         // Object identity
} visual_feature_type_t;

// Convolution layer configuration (V1 Gabor filters)
typedef struct {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t input_channels;      // 1=grayscale, 3=RGB
    uint32_t num_filters;
    uint32_t kernel_size;
    uint32_t stride;
    uint32_t padding;
    visual_activation_type_t activation;
} conv_layer_config_t;
```

#### 24.4 Extended-Spectrum Retina

```c
// Photoreceptor types (extended wavelength detection)
typedef enum {
    PHOTORECEPTOR_ROD,                   // Low-light (498nm)
    PHOTORECEPTOR_CONE_S,                // Blue (420nm)
    PHOTORECEPTOR_CONE_M,                // Green (534nm)
    PHOTORECEPTOR_CONE_L,                // Red (564nm)
    PHOTORECEPTOR_CONE_UV,               // UV SWS1 opsin (360nm) - bird-like
    PHOTORECEPTOR_CONE_NIR,              // NIR quantum dot (850nm)
    PHOTORECEPTOR_THERMORECEPTOR_LWIR    // Thermal-IR pit organ (8-14µm) - snake-like
} photoreceptor_type_t;

// Vision enhancement modes
typedef enum {
    VISION_MODE_NORMAL,   // Standard human vision
    VISION_MODE_EAGLE,    // 4-5x acuity, UV, motion tracking
    VISION_MODE_CAT       // Night vision, tapetum lucidum
} vision_mode_t;

// Wavelength bands
#define RETINA_UV_RANGE      300-400nm    // SWS1 opsin peak: 360nm
#define RETINA_VISIBLE_RANGE 400-700nm    // Standard RGB
#define RETINA_NIR_RANGE     700-1100nm   // Quantum dot detection
#define RETINA_THERMAL_RANGE 8-14µm       // Atmospheric window (LWIR)
```

#### 24.5 Cochlear Processing Pipeline

```c
// Complete auditory pathway: Audio → BM → OHC → IHC → ANF
typedef struct {
    bm_output_t* bm_output;              // Basilar membrane frequency decomposition
    hc_bank_output_t* hc_output;         // Hair cell transduction
    anf_output_t* anf_output;            // Auditory nerve spikes
    dog_localization_t* dog_localization;// Dog sound localization (if enabled)
    echolocation_result_t* echo_result;  // Bat echolocation (if enabled)
    float* channel_energy;               // Per-channel energy
    float peak_frequency_hz;             // Dominant frequency
    float overall_level_db;              // Overall level (dB SPL)
} cochlea_output_t;

// Species-specific hearing modes
typedef enum {
    BM_HEARING_HUMAN,    // 20 Hz - 20 kHz
    BM_HEARING_DOG,      // 67 Hz - 65 kHz, enhanced localization
    BM_HEARING_BAT,      // 1 kHz - 200 kHz, echolocation
    BM_HEARING_HYBRID    // Combined capabilities
} bm_hearing_mode_t;

// Cochlea configuration
typedef struct {
    uint32_t sample_rate;
    uint32_t num_channels;
    bm_hearing_mode_t hearing_mode;
    bm_config_t bm_config;               // Basilar membrane
    hc_bank_config_t hc_config;          // Hair cells
    anf_config_t anf_config;             // Auditory nerve
    bool enable_extended_hearing;
    bool enable_ohc_amplification;       // Active gain
    bool enable_phase_locking;
    bool enable_bio_async;
} cochlea_config_t;
```

#### 24.6 Audio Cortex (A1)

```c
// Audio cortex configuration
typedef struct {
    uint32_t sample_rate;                // Hz
    uint32_t frame_size;                 // Samples
    uint32_t num_freq_bins;              // FFT bins
    uint32_t num_mel_filters;            // Mel-scale filters
    uint32_t num_mfcc;                   // MFCC coefficients
    uint8_t num_channels;                // 1=mono, 2=stereo
    bool enable_attention;               // Attention mechanisms
    bool enable_memory;                  // Auditory memory
    bool enable_fractal_topology;        // Scale-free A1 topology
    float hub_ratio;                     // Fraction of hub neurons
    bool enable_bio_async;
} audio_cortex_config_t;

// Auditory memory entry
typedef struct {
    float* features;                     // Audio feature vector
    float salience;                      // Memory salience [0-1]
    uint64_t timestamp;
    char context[64];
} auditory_memory_t;

// Audio attention map
typedef struct {
    float* values;                       // Attention weights
    uint32_t num_freq;                   // Frequency bins
    uint32_t num_time;                   // Time frames
} audio_attention_map_t;
```

#### 24.7 Speech Cortex (STG/Wernicke)

```c
// Phoneme categories (IPA subset for English - 44 phonemes)
typedef enum {
    // Vowels (12)
    PHONEME_IY, PHONEME_IH, PHONEME_EY, PHONEME_EH,   // Front vowels
    PHONEME_AE, PHONEME_AA, PHONEME_AO, PHONEME_OW,   // Back vowels
    PHONEME_UH, PHONEME_UW, PHONEME_AH, PHONEME_ER,   // Central/R-colored

    // Consonants: Stops (6)
    PHONEME_P, PHONEME_B, PHONEME_T, PHONEME_D, PHONEME_K, PHONEME_G,

    // Consonants: Fricatives (9)
    PHONEME_F, PHONEME_V, PHONEME_TH, PHONEME_DH,
    PHONEME_S, PHONEME_Z, PHONEME_SH, PHONEME_ZH, PHONEME_H,

    // Consonants: Nasals (3)
    PHONEME_M, PHONEME_N, PHONEME_NG,

    // Consonants: Approximants (4)
    PHONEME_L, PHONEME_R, PHONEME_W, PHONEME_Y,

    // Affricates (2)
    PHONEME_CH, PHONEME_JH,

    // Special
    PHONEME_SILENCE, PHONEME_UNKNOWN,
    PHONEME_COUNT
} phoneme_t;

// Speech processing constants
#define SPEECH_NUM_FORMANTS 4            // F1, F2, F3, F4
#define SPEECH_MAX_PHONOLOGICAL_BUFFER 9 // Miller's 7±2
#define SPEECH_PHONEME_FRAME_MS 20       // 10-25ms typical
```

#### 24.8 Perception Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| Visual FEP Bridge | `nimcp_visual_cortex_fep_bridge.h` | Free energy principle for vision |
| Audio FEP Bridge | `nimcp_audio_cortex_fep_bridge.h` | Free energy principle for audio |
| Speech FEP Bridge | `nimcp_speech_cortex_fep_bridge.h` | Free energy principle for speech |
| Visual JEPA Bridge | `nimcp_visual_jepa_bridge.h` | JEPA predictive processing |
| Speech JEPA Bridge | `nimcp_speech_jepa_bridge.h` | JEPA for speech prediction |
| Visual Cortical Bridge | `nimcp_visual_cortical_bridge.h` | Cortical column integration |
| Audio Cortical Bridge | `nimcp_audio_cortical_bridge.h` | Cortical column integration |
| Speech Cortical Bridge | `nimcp_speech_cortical_bridge.h` | Cortical column integration |
| Visual Immune Bridge | `nimcp_visual_immune_bridge.h` | Immune effects on vision |
| Audio Immune Bridge | `nimcp_audio_immune_bridge.h` | Immune effects on hearing |
| Speech Immune Bridge | `nimcp_speech_immune_bridge.h` | Immune effects on speech |
| Retina Sleep Bridge | `nimcp_retina_sleep_bridge.h` | Sleep effects on vision |
| Audio Sleep Bridge | `nimcp_audio_cortex_sleep_bridge.h` | Sleep effects on hearing |
| Visual GPU Bridge | `nimcp_visual_cortex_gpu.h` | GPU acceleration |
| Audio GPU Bridge | `nimcp_audio_cortex_gpu.h` | GPU acceleration |
| Speech GPU Bridge | `nimcp_speech_cortex_gpu.h` | GPU acceleration |
| Cochlea Thalamic Bridge | `nimcp_cochlea_thalamic_bridge.h` | MGN relay, attention gating |
| Cochlea Broca Bridge | `nimcp_cochlea_broca_bridge.h` | Speech perception |
| Cochlea Occipital Bridge | `nimcp_cochlea_occipital_bridge.h` | Audiovisual binding |
| Omni Sensory Bridge | `nimcp_omni_sensory_bridge.h` | Predictive perception |

#### 24.9 Perception-Cognitive Integration

| Cognitive Module | Perception Integration | Direction |
|-----------------|------------------------|-----------|
| Attention | Visual/auditory salience, gating | Perception → Cognitive |
| Working Memory | Visual/phonological buffer | Bidirectional |
| Emotion | Affective visual/auditory cues | Perception → Cognitive |
| Reasoning | Visual scene understanding | Perception → Cognitive |
| Executive | Perceptual task switching | Cognitive → Perception |
| Curiosity | Novelty detection | Bidirectional |
| Self-Model | Body perception, proprioception | Perception → Cognitive |
| ToM | Face perception, gaze tracking | Perception → Cognitive |
| Mirror Neurons | Action observation | Perception → Cognitive |
| Semantic Memory | Object/sound recognition | Bidirectional |
| Episodic Memory | Visual/auditory scene encoding | Perception → Cognitive |
| Language | Speech perception, reading | Perception → Language |

#### 24.10 Perception Bio-Async Messages

```c
// Perception-related bio-async message types
typedef enum {
    // Visual messages
    BIO_MSG_VISUAL_FRAME_INPUT,           // New visual frame
    BIO_MSG_VISUAL_FEATURE_DETECTED,      // Feature detected (edge, face, etc.)
    BIO_MSG_VISUAL_MOTION_DETECTED,       // Motion event
    BIO_MSG_VISUAL_SACCADE_REQUEST,       // Saccade generation
    BIO_MSG_VISUAL_ATTENTION_SHIFT,       // Attention shift

    // Auditory messages
    BIO_MSG_AUDIO_FRAME_INPUT,            // New audio frame
    BIO_MSG_AUDIO_ONSET_DETECTED,         // Sound onset
    BIO_MSG_AUDIO_PITCH_DETECTED,         // Pitch extraction
    BIO_MSG_AUDIO_LOCALIZATION,           // Sound localization
    BIO_MSG_AUDIO_MEMORY_MATCH,           // Auditory memory match

    // Speech messages
    BIO_MSG_SPEECH_PHONEME_DETECTED,      // Phoneme recognized
    BIO_MSG_SPEECH_WORD_BOUNDARY,         // Word boundary detected
    BIO_MSG_SPEECH_FORMANT_UPDATE,        // Formant frequencies
    BIO_MSG_SPEECH_PROSODY_UPDATE,        // Prosodic information

    // Cochlea messages
    BIO_MSG_COCHLEA_SPIKE_TRAIN,          // ANF spike output
    BIO_MSG_COCHLEA_ENERGY_UPDATE,        // Channel energy
    BIO_MSG_COCHLEA_ECHOLOCATION,         // Bat mode echo

    // Cross-modal messages
    BIO_MSG_PERCEPTION_AUDIOVISUAL_SYNC,  // AV synchronization
    BIO_MSG_PERCEPTION_MULTIMODAL_FUSION, // Multimodal binding
    BIO_MSG_PERCEPTION_ATTENTION_MODULATION, // Attention effect

    BIO_MSG_PERCEPTION_COUNT
} nimcp_perception_msg_type_t;
```

#### 24.11 Perception-Cognitive Integration API

```c
// Perception integration context for cognitive modules
typedef struct {
    // Visual processing
    visual_cortex_t* visual_cortex;
    occipital_adapter_t* occipital;
    retina_t* retina;

    // Auditory processing
    audio_cortex_t* audio_cortex;
    cochlea_t* cochlea;
    speech_cortex_t* speech_cortex;

    // Feature extraction interface
    bool (*get_visual_features)(visual_feature_t* features, uint32_t* count);
    bool (*get_audio_features)(audio_feature_t* features, uint32_t* count);
    bool (*get_speech_features)(speech_feature_t* features, uint32_t* count);

    // Attention modulation interface
    bool (*set_visual_attention)(const attention_map_t* attention);
    bool (*set_audio_attention)(const audio_attention_map_t* attention);

    // Multimodal fusion
    bool (*fuse_audiovisual)(audiovisual_binding_t* binding);

    // Bio-async integration
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;
} nimcp_perception_cognitive_integration_t;

// Initialize perception for cognitive module
nimcp_status_t nimcp_perception_init_cognitive_integration(
    nimcp_perception_cognitive_integration_t* integration,
    visual_cortex_t* visual,
    audio_cortex_t* audio,
    speech_cortex_t* speech,
    nimcp_bio_router_t* router);

// Process visual input and extract features
nimcp_status_t nimcp_perception_process_visual(
    nimcp_perception_cognitive_integration_t* integration,
    const float* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels);

// Process audio input and extract features
nimcp_status_t nimcp_perception_process_audio(
    nimcp_perception_cognitive_integration_t* integration,
    const float* audio,
    uint32_t num_samples,
    uint32_t sample_rate);
```

#### 24.12 Audiovisual Integration

```c
// McGurk effect and audiovisual binding
typedef struct {
    // Visual speech cues
    float* lip_shape;                     // Lip shape features
    float* face_motion;                   // Face motion vectors
    uint32_t visual_phoneme_hypothesis;   // Visual phoneme guess

    // Audio speech cues
    float* audio_features;                // Audio features
    uint32_t audio_phoneme_hypothesis;    // Audio phoneme guess

    // Fused result
    uint32_t fused_phoneme;               // Final phoneme decision
    float fusion_confidence;              // Confidence in fusion
    bool mcgurk_detected;                 // McGurk effect detected

    // Timing
    float av_sync_offset_ms;              // AV synchronization offset
    bool av_synchronized;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
} nimcp_audiovisual_binding_t;

// Perform audiovisual binding
nimcp_status_t nimcp_perception_bind_audiovisual(
    nimcp_perception_cognitive_integration_t* integration,
    const float* visual_input,
    const float* audio_input,
    nimcp_audiovisual_binding_t* binding);

// Detect McGurk effect (visual override of auditory)
nimcp_status_t nimcp_perception_detect_mcgurk(
    nimcp_audiovisual_binding_t* binding);
```

#### 24.13 Extended Perception Modes

```c
// Extended perception configuration
typedef struct {
    // Vision enhancements
    vision_mode_t vision_mode;            // Normal, Eagle, Cat
    bool enable_uv_vision;                // UV detection
    bool enable_nir_vision;               // Near-infrared
    bool enable_thermal_vision;           // Thermal-IR

    // Hearing enhancements
    bm_hearing_mode_t hearing_mode;       // Human, Dog, Bat, Hybrid
    bool enable_echolocation;             // Bat-style echolocation
    bool enable_ultrasonic;               // Ultrasonic detection

    // Cross-modal
    bool enable_synesthesia;              // Cross-modal binding
    bool enable_sensory_substitution;     // Substitute one sense for another

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
} nimcp_extended_perception_config_t;

// Enable extended perception mode
nimcp_status_t nimcp_perception_enable_extended(
    nimcp_perception_cognitive_integration_t* integration,
    const nimcp_extended_perception_config_t* config);
```

#### 24.14 Perception Training Integration

```c
// Perception learning configuration
typedef struct {
    // Visual learning
    bool enable_visual_learning;
    float visual_learning_rate;
    bool enable_gabor_adaptation;         // Adapt V1 Gabor filters

    // Auditory learning
    bool enable_auditory_learning;
    float auditory_learning_rate;
    bool enable_tonotopic_adaptation;     // Adapt A1 tonotopy

    // Speech learning
    bool enable_speech_learning;
    float speech_learning_rate;
    bool enable_phoneme_boundary_learning;// Learn phoneme boundaries

    // Curriculum
    nimcp_curriculum_strategy_t curriculum;

    // Training stats
    float visual_recognition_accuracy;
    float auditory_recognition_accuracy;
    float speech_recognition_accuracy;
} nimcp_perception_training_config_t;

// Initialize perception learning
nimcp_status_t nimcp_perception_init_training(
    nimcp_perception_cognitive_integration_t* integration,
    const nimcp_perception_training_config_t* config);

// Learn from perception feedback
nimcp_status_t nimcp_perception_learn_from_feedback(
    nimcp_perception_cognitive_integration_t* integration,
    const perception_feedback_t* feedback);
```

#### 24.15 Perception Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/integration/perception/test_visual_cortex_integration.cpp` | 30 |
| `test/integration/perception/test_audio_cortex_integration.cpp` | 25 |
| `test/integration/perception/test_speech_cortex_integration.cpp` | 25 |
| `test/integration/perception/test_cochlea_integration.cpp` | 25 |
| `test/integration/perception/test_retina_integration.cpp` | 20 |
| `test/integration/perception/test_occipital_adapter_integration.cpp` | 25 |
| `test/integration/perception/test_perception_fep_bridges.cpp` | 20 |
| `test/integration/perception/test_perception_jepa_bridges.cpp` | 20 |
| `test/integration/perception/test_audiovisual_binding.cpp` | 20 |
| `test/integration/perception/test_extended_perception_modes.cpp` | 20 |
| `test/integration/perception/test_perception_bioasync_integration.cpp` | 20 |
| `test/integration/perception/test_perception_cognitive_integration.cpp` | 20 |
| **Total** | **270** |

### 25. GPU Module Integration

**Priority**: High
**Dependencies**: All cognitive modules, Training, Perception, Language
**Est. LOC**: 35,000 | **Tests**: 300

#### 25.1 GPU Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        GPU Acceleration Layer                                │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                     Execution Mode Selection                          │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐│  │
│  │  │    CPU     │  │    GPU      │  │  Distributed │  │   Hybrid     ││  │
│  │  │ Sequential │  │   CUDA/     │  │   CPU/GPU    │  │  CPU + GPU   ││  │
│  │  │  Parallel  │  │   ROCm      │  │   Clusters   │  │  Offload     ││  │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └──────────────┘│  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │                      Multi-GPU Manager                                 │ │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────────────┐  │ │
│  │  │ GPU 0  │  │ GPU 1  │  │ GPU 2  │  │ GPU 3  │  │  P2P Memory    │  │ │
│  │  │ 0-25%  │  │ 25-50% │  │ 50-75% │  │ 75-100%│  │  Transfers     │  │ │
│  │  └────────┘  └────────┘  └────────┘  └────────┘  └────────────────┘  │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │                      GPU Tensor Operations                             │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐  │ │
│  │  │   cuBLAS    │  │   Custom    │  │    cuFFT    │  │   Mixed      │  │ │
│  │  │    GEMM     │  │   Kernels   │  │    FFT      │  │  Precision   │  │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └──────────────┘  │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │                GPU Module Implementations                              │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │ │
│  │  │Training │ │Inference│ │ SNN GPU │ │ CNN GPU │ │LNN GPU  │        │ │
│  │  │  GPU    │ │   GPU   │ │ Kernels │ │ Kernels │ │ ODE     │        │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘        │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │ │
│  │  │Emotion  │ │Reasoning│ │ Memory  │ │ Swarm   │ │ Sleep   │        │ │
│  │  │  GPU    │ │   GPU   │ │Consolid │ │  GPU    │ │  GPU    │        │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘        │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │ │
│  │  │Visual   │ │ Audio   │ │ Speech  │ │Dragonfly│ │ Portia  │        │ │
│  │  │Cortex   │ │ Cortex  │ │ Cortex  │ │ Vision  │ │   GPU   │        │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘        │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │              Memory Management & Optimization                          │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐  │ │
│  │  │  GPU Pool   │  │   Async     │  │  Prefetch   │  │  Gradient    │  │ │
│  │  │  Factory    │  │  Transfer   │  │   Manager   │  │ Checkpoint   │  │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └──────────────┘  │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 25.2 GPU Core Components

| Component | Header | Purpose |
|-----------|--------|---------|
| GPU Neuron | `nimcp_gpu_neuron.h` | P2P neuron computation on GPU |
| Execution Mode | `nimcp_execution_mode.h` | CPU/GPU/Distributed mode selection |
| Multi-GPU | `nimcp_multigpu.h` | Multi-GPU work distribution |
| GPU Context | `nimcp_gpu_context.h` | CUDA context management |
| Spike Event | `nimcp_spike_event.h` | GPU spike event queues |

#### 25.3 Execution Mode Types

```c
// Execution modes (from nimcp_execution_mode.h)
typedef enum {
    EXEC_MODE_CPU_SEQUENTIAL,    // CPU single-threaded
    EXEC_MODE_CPU_PARALLEL,      // CPU multi-threaded
    EXEC_MODE_GPU_CUDA,          // NVIDIA GPU (CUDA)
    EXEC_MODE_GPU_ROCM,          // AMD GPU (ROCm)
    EXEC_MODE_GPU_OPENCL,        // OpenCL (cross-platform)
    EXEC_MODE_DISTRIBUTED_CPU,   // Distributed across CPU nodes
    EXEC_MODE_DISTRIBUTED_GPU,   // Distributed across GPU nodes
    EXEC_MODE_HYBRID,            // CPU + GPU hybrid
    EXEC_MODE_AUTO               // Auto-detect best mode
} execution_mode_t;

// Hardware capabilities detection
typedef struct {
    // CPU capabilities
    bool cpu_available;
    uint32_t cpu_cores;
    uint32_t cpu_threads;
    bool cpu_avx2;               // AVX2 SIMD support
    bool cpu_avx512;             // AVX512 SIMD support

    // GPU capabilities
    bool cuda_available;
    bool rocm_available;
    bool opencl_available;
    uint32_t gpu_count;
    uint32_t gpu_compute_units;  // CUDA cores or compute units
    uint64_t gpu_memory_mb;      // GPU memory in MB
    uint32_t gpu_compute_capability;

    // Network capabilities (for distributed)
    bool network_available;
    uint32_t network_nodes;
    uint32_t network_bandwidth_mbps;

    execution_mode_t recommended_mode;
} hardware_capabilities_t;

// Execution context
execution_context_t execution_context_create(const execution_config_t* config);
void execution_context_destroy(execution_context_t ctx);
execution_mode_t execution_get_recommended_mode(uint32_t num_neurons, uint32_t num_synapses);
```

#### 25.4 GPU Tensor Operations

```c
// GPU precision modes (from nimcp_tensor_gpu.h)
typedef enum {
    NIMCP_GPU_PRECISION_FP32 = 0,   // Full precision (32-bit float)
    NIMCP_GPU_PRECISION_FP16 = 1,   // Half precision (16-bit float)
    NIMCP_GPU_PRECISION_BF16 = 2,   // Brain float (16-bit)
    NIMCP_GPU_PRECISION_INT8 = 3,   // Quantized 8-bit integer
    NIMCP_GPU_PRECISION_TF32 = 4,   // Tensor Float 32 (Ampere+)
    NIMCP_GPU_PRECISION_UINT32 = 5, // Unsigned 32-bit integer
    NIMCP_GPU_PRECISION_INT32 = 6   // Signed 32-bit integer
} nimcp_gpu_precision_t;

// GPU tensor structure
typedef struct nimcp_gpu_tensor_s {
    void* data;                      // Device memory pointer
    size_t* dims;                    // Dimension sizes
    size_t* strides;                 // Strides per dimension
    uint32_t ndim;                   // Number of dimensions
    size_t numel;                    // Total elements
    size_t elem_size;                // Element size in bytes
    nimcp_gpu_precision_t precision; // Data precision
    nimcp_tensor_layout_t layout;    // Memory layout
    nimcp_gpu_context_t* ctx;        // GPU context
    bool owns_data;                  // Ownership flag
} nimcp_gpu_tensor_t;

// GEMM operations (cuBLAS integration)
bool nimcp_gpu_gemm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                    const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                    float alpha, float beta, bool trans_a, bool trans_b);
bool nimcp_gpu_gemm_batched(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                             const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                             float alpha, float beta, bool trans_a, bool trans_b);

// Element-wise operations
bool nimcp_gpu_add(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out);
bool nimcp_gpu_mul(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out);

// Activation functions (GPU kernels)
bool nimcp_gpu_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out);
bool nimcp_gpu_sigmoid(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out);
bool nimcp_gpu_gelu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out);
bool nimcp_gpu_silu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out);
bool nimcp_gpu_softmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out);

// Reductions (warp-shuffle optimized)
bool nimcp_gpu_sum(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims);
bool nimcp_gpu_mean(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                    nimcp_gpu_tensor_t* out, int axis, bool keepdims);
bool nimcp_gpu_max(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims);

// FFT operations (cuFFT integration)
bool nimcp_gpu_fft_1d(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                       nimcp_gpu_tensor_t* out, bool inverse);
bool nimcp_gpu_rfft(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                    nimcp_gpu_tensor_t* out);

// CPU-GPU tensor conversion
nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_cpu(nimcp_gpu_context_t* ctx,
                                               const nimcp_tensor_t* cpu_tensor);
nimcp_tensor_t* nimcp_cpu_tensor_from_gpu(const nimcp_gpu_tensor_t* gpu_tensor);
```

#### 25.5 GPU Training Operations

```c
// Loss function types (from nimcp_training_gpu.h)
typedef enum {
    NIMCP_LOSS_MSE = 0,           // Mean Squared Error
    NIMCP_LOSS_MAE = 1,           // Mean Absolute Error
    NIMCP_LOSS_CROSS_ENTROPY = 2, // Cross Entropy (classification)
    NIMCP_LOSS_BCE = 3,           // Binary Cross Entropy
    NIMCP_LOSS_FOCAL = 4,         // Focal Loss (imbalanced data)
    NIMCP_LOSS_HUBER = 5,         // Huber Loss (robust regression)
    NIMCP_LOSS_COSINE = 6,        // Cosine Similarity Loss
    NIMCP_LOSS_HINGE = 7,         // Hinge Loss (SVM-style)
    NIMCP_LOSS_KL_DIV = 8         // KL Divergence
} nimcp_loss_type_t;

// Optimizer types
typedef enum {
    NIMCP_OPTIM_SGD = 0,          // Stochastic Gradient Descent
    NIMCP_OPTIM_SGD_MOMENTUM = 1, // SGD with Momentum
    NIMCP_OPTIM_ADAM = 2,         // Adam optimizer
    NIMCP_OPTIM_ADAMW = 3,        // AdamW (decoupled weight decay)
    NIMCP_OPTIM_RMSPROP = 4,      // RMSprop
    NIMCP_OPTIM_ADAGRAD = 5,      // AdaGrad
    NIMCP_OPTIM_ADADELTA = 6,     // AdaDelta
    NIMCP_OPTIM_NADAM = 7         // Nesterov Adam
} nimcp_optim_type_t;

// Loss functions with GPU kernels
bool nimcp_gpu_loss_mse(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
                        const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad);
bool nimcp_gpu_loss_cross_entropy(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* logits,
                                   const nimcp_gpu_tensor_t* target, float* loss,
                                   nimcp_gpu_tensor_t* grad, int reduction);
bool nimcp_gpu_loss_focal(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
                          const nimcp_gpu_tensor_t* target, float* loss,
                          nimcp_gpu_tensor_t* grad, float alpha, float gamma);

// Gradient operations
bool nimcp_gpu_gradient_clip_norm(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t** grads,
                                   size_t n_grads, float max_norm, float* total_norm);
bool nimcp_gpu_gradient_scale(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* grad, float scale);

// Optimizer GPU kernels
bool nimcp_gpu_optim_adam(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
                          const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state);
bool nimcp_gpu_optim_adamw(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
                           const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state);
bool nimcp_gpu_optim_sgd(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
                         const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state);

// Backpropagation GPU kernels
bool nimcp_gpu_backward_linear(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                                const nimcp_gpu_tensor_t* weight, const nimcp_gpu_tensor_t* grad_output,
                                nimcp_gpu_tensor_t* grad_input, nimcp_gpu_tensor_t* grad_weight,
                                nimcp_gpu_tensor_t* grad_bias);
bool nimcp_gpu_backward_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                              const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input);
bool nimcp_gpu_backward_batchnorm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                                   const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* mean,
                                   const nimcp_gpu_tensor_t* var, const nimcp_gpu_tensor_t* grad_output,
                                   nimcp_gpu_tensor_t* grad_input, nimcp_gpu_tensor_t* grad_gamma,
                                   nimcp_gpu_tensor_t* grad_beta, float eps);

// Learning rate schedulers
float nimcp_lr_cosine(float max_lr, float min_lr, uint64_t step, uint64_t total_steps);
float nimcp_lr_warmup_linear(float max_lr, uint64_t step, uint64_t warmup_steps, uint64_t total_steps);
```

#### 25.6 Multi-GPU Support

```c
// Multi-GPU partition strategies (from nimcp_multigpu.h)
typedef enum {
    MULTIGPU_PARTITION_LAYER,    // Split by network layers (deep networks)
    MULTIGPU_PARTITION_NEURON,   // Split by neurons (wide networks)
    MULTIGPU_PARTITION_HYBRID,   // Mix of layer + neuron (adaptive)
    MULTIGPU_PARTITION_DYNAMIC,  // Runtime-adaptive based on performance
    MULTIGPU_PARTITION_AUTO      // Auto-select based on network topology
} multigpu_partition_strategy_t;

// Load balancing strategies
typedef enum {
    MULTIGPU_LOADBALANCE_STATIC,    // Fixed allocation (fastest)
    MULTIGPU_LOADBALANCE_DYNAMIC,   // Redistribute based on utilization
    MULTIGPU_LOADBALANCE_ADAPTIVE   // Learn optimal distribution
} multigpu_loadbalance_strategy_t;

// Multi-GPU configuration
typedef struct {
    uint32_t num_devices;                     // Number of GPUs (0 = all)
    int* device_ids;                          // Specific device IDs
    bool enable_peer_access;                  // Enable GPU-to-GPU P2P
    multigpu_partition_strategy_t partition_strategy;
    multigpu_loadbalance_strategy_t loadbalance_strategy;
    uint64_t max_memory_per_gpu;              // Max memory per GPU
    bool enable_unified_memory;               // CUDA unified memory
    bool pin_host_memory;                     // Pin CPU memory
    uint32_t streams_per_device;              // CUDA streams per GPU
    bool enable_concurrent_kernels;           // Concurrent kernel execution
    bool enable_async_transfers;              // Async memory transfers
    uint32_t loadbalance_interval;            // Check balance every N iterations
    float imbalance_threshold;                // Rebalance threshold (default: 0.15)
    bool enable_work_stealing;                // Allow idle GPUs to steal work
} multigpu_config_t;

// Multi-GPU operations
multigpu_context_t multigpu_context_create(const multigpu_config_t* config);
void multigpu_context_destroy(multigpu_context_t ctx);
bool multigpu_partition_network(multigpu_context_t ctx, uint32_t num_layers,
                                 const uint32_t* neurons_per_layer);
bool multigpu_rebalance_work(multigpu_context_t ctx);
bool multigpu_broadcast(multigpu_context_t ctx, const void* host_data,
                        void** device_ptrs, size_t size);
bool multigpu_gather(multigpu_context_t ctx, void** device_ptrs,
                     void* host_data, size_t size_per_gpu);
bool multigpu_sync_devices(multigpu_context_t ctx, uint32_t src_device,
                           uint32_t dst_device, const void* src_data,
                           void* dst_data, size_t size);
```

#### 25.7 GPU Module Headers (70 Files)

| Category | Headers | Purpose |
|----------|---------|---------|
| **Core** | `nimcp_gpu_neuron.h`, `nimcp_execution_mode.h`, `nimcp_multigpu.h`, `nimcp_spike_event.h` | Core GPU infrastructure |
| **Context** | `nimcp_gpu_context.h` | CUDA/ROCm context management |
| **Tensor** | `nimcp_tensor_gpu.h`, `nimcp_tensor_fp16.h`, `nimcp_amp_autocast.h` | GPU tensor operations |
| **Training** | `nimcp_training_gpu.h`, `nimcp_gradient_checkpoint.h`, `nimcp_activation_checkpoint.h` | GPU training kernels |
| **Inference** | `nimcp_inference_gpu.h`, `nimcp_int8_inference.h`, `nimcp_tensorrt_export.h` | Optimized inference |
| **Memory** | `nimcp_gpu_pool.h`, `nimcp_gpu_pool_factory.h`, `nimcp_memory_consolidation_gpu.h` | GPU memory management |
| **Transfer** | `nimcp_async_transfer.h`, `nimcp_prefetch.h` | Async data transfer |
| **Detection** | `nimcp_gpu_detect.h`, `nimcp_simd_detect.h`, `nimcp_network_detect.h` | Hardware detection |
| **Neural** | `nimcp_synapse_gpu.h`, `nimcp_axon_gpu.h`, `nimcp_dendrite_gpu.h` | Neural structure GPU |
| **SNN** | `nimcp_snn_gpu.h`, `nimcp_plasticity_gpu.h` | Spiking neural network GPU |
| **CNN** | `nimcp_cnn_gpu.h` | Convolutional neural network GPU |
| **LNN** | `nimcp_lnn_gpu.h`, `nimcp_lnn_ode_gpu.h`, `nimcp_lnn_gradient_dao.h` | Liquid neural network GPU |
| **Cognitive** | `nimcp_emotion_gpu.h`, `nimcp_reasoning_gpu.h`, `nimcp_metalearning_gpu.h` | Cognitive module GPU |
| **Language** | `nimcp_wernicke_gpu.h`, `nimcp_broca_gpu.h` | Language processing GPU |
| **Perception** | `nimcp_visual_cortex_gpu.h`, `nimcp_audio_cortex_gpu.h`, `nimcp_speech_cortex_gpu.h` | Perception GPU |
| **Specialized** | `nimcp_dragonfly_vision_gpu.h`, `nimcp_portia_gpu.h`, `nimcp_swarm_gpu.h`, `nimcp_sleep_gpu.h` | Specialized system GPU |
| **Glial** | `nimcp_myelin_gpu.h` | Glial/myelin GPU |
| **Quantum** | `nimcp_quantum_gpu.h`, `nimcp_qmc_gpu.h` | Quantum computing GPU |
| **Oscillations** | `nimcp_oscillations_gpu.h` | Brain oscillations GPU |
| **Regions** | `nimcp_regions_gpu.h`, `nimcp_occipital_gpu_bridge.h`, `nimcp_hypothalamus_gpu.h` | Brain regions GPU |
| **Neuromodulators** | `nimcp_neuromodulator_gpu.h` | Neuromodulator GPU |
| **Knowledge** | `nimcp_knowledge_graph_gpu.h`, `nimcp_graph_gpu.h`, `nimcp_graph_dao.h` | Knowledge graph GPU |
| **Substrate** | `nimcp_substrate_gpu.h` | Neural substrate GPU |
| **Sparse** | `nimcp_sparse_gpu.h` | Sparse tensor GPU |
| **Ternary** | `nimcp_ternary_gpu.h` | Ternary neural network GPU |
| **Topology** | `nimcp_topology_gpu.h` | Network topology GPU |
| **Backend** | `nimcp_kernel_backend.h` | Kernel backend abstraction |
| **Common** | `nimcp_cuda_utils.h`, `nimcp_gpu_common.h` | Common GPU utilities |
| **GraphQL** | `nimcp_graphql_utils.h` | GraphQL integration |
| **Immune** | `nimcp_gpu_neuron_immune_bridge.h`, `nimcp_gpu_execution_immune_bridge.h`, `nimcp_multigpu_immune_bridge.h` | GPU immune integration |
| **JEPA** | `nimcp_jepa_gpu.h` | Joint Embedding Prediction GPU |
| **Omni** | `nimcp_omni_gpu.h` | Omnidirectional inference GPU |

#### 25.8 GPU-Cognitive Module Bridges

| Cognitive Module | GPU Header | Purpose |
|------------------|------------|---------|
| Emotion | `nimcp_emotion_gpu.h` | GPU-accelerated emotional valence computation |
| Reasoning | `nimcp_reasoning_gpu.h` | GPU-accelerated inference and deduction |
| Meta-Learning | `nimcp_metalearning_gpu.h` | GPU-accelerated meta-learning loops |
| Memory Consolidation | `nimcp_memory_consolidation_gpu.h` | GPU-accelerated memory replay |
| Swarm | `nimcp_swarm_gpu.h`, `nimcp_swarm_memory_gpu.h` | Collective intelligence GPU |
| Dragonfly | `nimcp_dragonfly_vision_gpu.h` | Target tracking GPU |
| Portia | `nimcp_portia_gpu.h` | Resource adaptation GPU |
| Sleep | `nimcp_sleep_gpu.h` | Sleep cycle GPU |
| Visual Cortex | `nimcp_visual_cortex_gpu.h` | V1-V5 GPU processing |
| Audio Cortex | `nimcp_audio_cortex_gpu.h` | A1 GPU processing |
| Speech Cortex | `nimcp_speech_cortex_gpu.h` | Phoneme recognition GPU |
| Wernicke | `nimcp_wernicke_gpu.h` | Language comprehension GPU |
| Broca | `nimcp_broca_gpu.h` | Language production GPU |
| Occipital | `nimcp_occipital_gpu_bridge.h` | Visual processing GPU |
| Hypothalamus | `nimcp_hypothalamus_gpu.h`, `nimcp_hypothalamus_qmc_gpu.h` | Homeostasis GPU |

#### 25.9 GPU Bio-Async Integration

```c
// GPU bio-async message types
typedef enum {
    GPU_MSG_TENSOR_READY,           // Tensor computation complete on GPU
    GPU_MSG_TRANSFER_COMPLETE,      // Host-device transfer complete
    GPU_MSG_KERNEL_LAUNCH,          // GPU kernel launched
    GPU_MSG_SYNC_COMPLETE,          // GPU synchronization complete
    GPU_MSG_MULTI_GPU_GATHER,       // Multi-GPU gather complete
    GPU_MSG_MULTI_GPU_BROADCAST,    // Multi-GPU broadcast complete
    GPU_MSG_MEMORY_WARNING,         // GPU memory pressure
    GPU_MSG_DEVICE_ERROR,           // GPU device error
    GPU_MSG_EXECUTION_MODE_CHANGE   // Execution mode changed
} nimcp_gpu_msg_type_t;

// GPU tensor ready message
typedef struct {
    nimcp_bio_msg_header_t header;
    nimcp_gpu_tensor_t* tensor;
    nimcp_gpu_context_t* ctx;
    uint64_t kernel_time_ns;        // Kernel execution time
    uint64_t transfer_time_ns;      // Data transfer time
} nimcp_gpu_tensor_ready_msg_t;

// GPU execution mode change message
typedef struct {
    nimcp_bio_msg_header_t header;
    execution_mode_t old_mode;
    execution_mode_t new_mode;
    const char* reason;             // Why mode changed
} nimcp_gpu_mode_change_msg_t;

// GPU memory warning message
typedef struct {
    nimcp_bio_msg_header_t header;
    uint64_t used_bytes;
    uint64_t total_bytes;
    float utilization;              // 0-1
    uint32_t device_id;
} nimcp_gpu_memory_warning_msg_t;
```

#### 25.10 GPU Memory Management

```c
// GPU memory pool configuration
typedef struct {
    uint64_t initial_size;          // Initial pool size (bytes)
    uint64_t max_size;              // Maximum pool size
    uint32_t block_size;            // Allocation block size
    bool enable_defragmentation;    // Enable defragmentation
    bool use_stream_ordered;        // Use CUDA stream-ordered allocation
} nimcp_gpu_pool_config_t;

// GPU pool factory
nimcp_gpu_pool_t* nimcp_gpu_pool_factory_create(nimcp_gpu_context_t* ctx,
                                                  const nimcp_gpu_pool_config_t* config);
void* nimcp_gpu_pool_alloc(nimcp_gpu_pool_t* pool, size_t size);
void nimcp_gpu_pool_free(nimcp_gpu_pool_t* pool, void* ptr);

// Async transfer manager
typedef struct {
    nimcp_gpu_context_t* ctx;
    uint32_t num_streams;           // Number of transfer streams
    bool enable_pinned_memory;      // Use pinned host memory
    bool enable_double_buffering;   // Double buffer transfers
} nimcp_async_transfer_config_t;

nimcp_async_transfer_t* nimcp_async_transfer_create(const nimcp_async_transfer_config_t* config);
bool nimcp_async_transfer_h2d(nimcp_async_transfer_t* mgr, void* dst,
                               const void* src, size_t size, int stream_id);
bool nimcp_async_transfer_d2h(nimcp_async_transfer_t* mgr, void* dst,
                               const void* src, size_t size, int stream_id);

// Prefetch manager
typedef struct {
    nimcp_gpu_context_t* ctx;
    uint32_t prefetch_distance;     // How far ahead to prefetch
    float memory_threshold;         // Don't prefetch above this utilization
} nimcp_prefetch_config_t;

nimcp_prefetch_t* nimcp_prefetch_create(const nimcp_prefetch_config_t* config);
bool nimcp_prefetch_tensor(nimcp_prefetch_t* mgr, const nimcp_tensor_t* tensor);
bool nimcp_prefetch_batch(nimcp_prefetch_t* mgr, nimcp_tensor_t** tensors, size_t count);

// Gradient checkpointing (memory optimization)
typedef struct {
    bool enable_checkpointing;
    uint32_t checkpoint_interval;   // Checkpoint every N layers
    float memory_budget_fraction;   // Target memory usage (0-1)
    bool enable_rematerialization;  // Recompute vs store
} nimcp_gradient_checkpoint_config_t;

nimcp_gradient_checkpoint_t* nimcp_gradient_checkpoint_create(const nimcp_gradient_checkpoint_config_t* config);
bool nimcp_gradient_checkpoint_forward(nimcp_gradient_checkpoint_t* mgr, nimcp_gpu_tensor_t* activation);
bool nimcp_gradient_checkpoint_backward(nimcp_gradient_checkpoint_t* mgr, nimcp_gpu_tensor_t* grad);
```

#### 25.11 GPU Inference Optimization

```c
// INT8 quantization for inference
typedef struct {
    float scale;                    // Quantization scale
    int8_t zero_point;              // Zero point
    float* calibration_data;        // Calibration histogram
    uint32_t calibration_samples;
} nimcp_int8_quant_params_t;

// Quantize tensor to INT8
bool nimcp_gpu_quantize_int8(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* fp32_tensor,
                              nimcp_gpu_tensor_t* int8_tensor, nimcp_int8_quant_params_t* params);

// INT8 matrix multiplication
bool nimcp_gpu_gemm_int8(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A_int8,
                          const nimcp_gpu_tensor_t* B_int8, nimcp_gpu_tensor_t* C_int32,
                          const nimcp_int8_quant_params_t* params_a,
                          const nimcp_int8_quant_params_t* params_b);

// TensorRT export
typedef struct {
    const char* model_name;
    const char* output_path;
    bool enable_fp16;
    bool enable_int8;
    uint32_t max_batch_size;
    uint64_t max_workspace_size;
} nimcp_tensorrt_export_config_t;

bool nimcp_tensorrt_export_model(const nimcp_gpu_tensor_t** weights, size_t num_weights,
                                  const nimcp_tensorrt_export_config_t* config);

// Mixed precision (AMP) autocast
typedef struct {
    nimcp_gpu_precision_t default_precision;
    nimcp_gpu_precision_t matmul_precision;    // Typically FP16
    nimcp_gpu_precision_t softmax_precision;   // Typically FP32
    nimcp_gpu_precision_t loss_precision;      // Typically FP32
    float loss_scale;                          // Dynamic loss scaling
} nimcp_amp_config_t;

nimcp_amp_context_t* nimcp_amp_create(const nimcp_amp_config_t* config);
bool nimcp_amp_autocast_begin(nimcp_amp_context_t* ctx);
bool nimcp_amp_autocast_end(nimcp_amp_context_t* ctx);
bool nimcp_amp_scale_loss(nimcp_amp_context_t* ctx, float* loss);
bool nimcp_amp_unscale_gradients(nimcp_amp_context_t* ctx, nimcp_gpu_tensor_t** grads, size_t n);
```

#### 25.12 GPU Brain Factory Integration

```c
// Brain factory GPU initialization
nimcp_status_t nimcp_brain_init_gpu(nimcp_brain_t* brain, const nimcp_brain_config_t* config) {
    NIMCP_LOG_INFO("Initializing GPU acceleration layer");

    // Detect hardware capabilities
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.gpu_count == 0) {
        NIMCP_LOG_WARN("No GPU detected, using CPU mode");
        brain->gpu_ctx = NULL;
        brain->execution_mode = EXEC_MODE_CPU_PARALLEL;
        return NIMCP_OK;
    }

    // Create GPU context
    nimcp_gpu_context_config_t gpu_config = {
        .device_id = 0,
        .enable_profiling = config->enable_gpu_profiling,
        .enable_async = true
    };
    brain->gpu_ctx = nimcp_gpu_context_create(&gpu_config);

    // Create multi-GPU context if multiple GPUs
    if (caps.gpu_count > 1 && config->enable_multi_gpu) {
        multigpu_config_t mgpu_config = multigpu_default_config();
        mgpu_config.num_devices = caps.gpu_count;
        mgpu_config.partition_strategy = MULTIGPU_PARTITION_HYBRID;
        brain->multigpu_ctx = multigpu_context_create(&mgpu_config);
    }

    // Create GPU memory pool
    nimcp_gpu_pool_config_t pool_config = {
        .initial_size = 256 * 1024 * 1024,  // 256 MB
        .max_size = caps.gpu_memory_mb * 1024 * 1024 * 0.8,  // 80% of GPU memory
        .enable_defragmentation = true
    };
    brain->gpu_pool = nimcp_gpu_pool_factory_create(brain->gpu_ctx, &pool_config);

    // Create async transfer manager
    nimcp_async_transfer_config_t transfer_config = {
        .ctx = brain->gpu_ctx,
        .num_streams = 4,
        .enable_pinned_memory = true,
        .enable_double_buffering = true
    };
    brain->gpu_transfer = nimcp_async_transfer_create(&transfer_config);

    // Determine optimal execution mode
    brain->execution_mode = execution_get_recommended_mode(
        config->max_neurons,
        config->avg_synapses_per_neuron
    );

    // Register GPU bio-async handlers
    nimcp_bio_router_register(brain->router, NIMCP_MSG_TYPE_GPU_TENSOR_READY,
                               gpu_tensor_ready_handler, brain->gpu_ctx);
    nimcp_bio_router_register(brain->router, NIMCP_MSG_TYPE_GPU_MEMORY_WARNING,
                               gpu_memory_warning_handler, brain->gpu_ctx);

    // Initialize GPU immune bridges
    nimcp_gpu_neuron_immune_bridge_init(&brain->gpu_immune_bridge, brain->immune);
    nimcp_gpu_execution_immune_bridge_init(&brain->exec_immune_bridge, brain->immune);
    if (brain->multigpu_ctx) {
        nimcp_multigpu_immune_bridge_init(&brain->multigpu_immune_bridge, brain->immune);
    }

    NIMCP_LOG_INFO("GPU layer initialized: %d GPU(s), mode=%d, memory=%.1f GB",
                   caps.gpu_count, brain->execution_mode,
                   caps.gpu_memory_mb / 1024.0);

    return NIMCP_OK;
}
```

#### 25.13 GPU Performance Heuristics

| Network Size | Recommended Mode | Expected Speedup |
|--------------|------------------|------------------|
| < 1K neurons | CPU Sequential | 1x (baseline) |
| 1K-10K neurons | CPU Parallel | 2-4x |
| 10K-100K neurons | GPU CUDA | 10-50x |
| 100K-1M neurons | Multi-GPU (2-4) | 50-200x |
| > 1M neurons | Distributed GPU (8+) | 200-1000x |

#### 25.14 GPU Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/gpu/test_gpu_context.cpp` | 15 |
| `test/unit/gpu/test_execution_mode.cpp` | 20 |
| `test/unit/gpu/test_gpu_tensor.cpp` | 30 |
| `test/unit/gpu/test_gpu_gemm.cpp` | 25 |
| `test/unit/gpu/test_gpu_activations.cpp` | 20 |
| `test/unit/gpu/test_gpu_reductions.cpp` | 15 |
| `test/unit/gpu/test_gpu_fft.cpp` | 10 |
| `test/unit/gpu/test_gpu_training.cpp` | 25 |
| `test/unit/gpu/test_gpu_loss_functions.cpp` | 15 |
| `test/unit/gpu/test_gpu_optimizers.cpp` | 20 |
| `test/unit/gpu/test_gpu_backward.cpp` | 25 |
| `test/unit/gpu/test_multigpu.cpp` | 20 |
| `test/unit/gpu/test_gpu_pool.cpp` | 15 |
| `test/unit/gpu/test_async_transfer.cpp` | 10 |
| `test/unit/gpu/test_gradient_checkpoint.cpp` | 10 |
| `test/unit/gpu/test_int8_inference.cpp` | 15 |
| `test/unit/gpu/test_amp_autocast.cpp` | 10 |
| **Total** | **300** |

### 26. Information Theory Module Integration

**Priority**: High
**Dependencies**: All cognitive modules, Perception, Bio-Async
**Est. LOC**: 18,000 | **Tests**: 160

#### 26.1 Information Theory Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Information Theory Layer                                  │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                   Shannon Information Theory                          │  │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │  │
│  │  │    Channel      │  │    Shannon      │  │     Mutual          │  │  │
│  │  │    Capacity     │  │    Entropy      │  │   Information       │  │  │
│  │  │  C = B×log₂(1+SNR)│ │ H(X)=-Σp(x)log₂p(x)│ │ I(X;Y)=H(X)+H(Y)-H(X,Y)│ │  │
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │                   Analysis Levels                                      │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐  │ │
│  │  │  Synapse    │  │   Neuron    │  │   Network   │  │  Bottleneck  │  │ │
│  │  │  Metrics    │  │   Metrics   │  │   Metrics   │  │  Detection   │  │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └──────────────┘  │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │                   Cross-Modal Integration                              │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐  │ │
│  │  │  Channel    │  │Multi-Modal  │  │  Routing    │  │   McGurk     │  │ │
│  │  │  Analysis   │  │Integration  │  │   Graph     │  │   Effect     │  │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └──────────────┘  │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                     │                                       │
│  ┌──────────────────────────────────┴────────────────────────────────────┐ │
│  │              Cognitive Module Integration                              │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │ │
│  │  │Attention│ │ Memory  │ │Reasoning│ │Perception│ │Language │        │ │
│  │  │  Info   │ │  Info   │ │  Info   │ │  Info   │ │  Info   │        │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘        │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 26.2 Core Components

| Component | Header | Purpose |
|-----------|--------|---------|
| Shannon Metrics | `nimcp_shannon.h` | Core information theory computations |
| Cross-Modal | `nimcp_cross_modal.h` | Multi-sensory information integration |
| Shannon Immune | `nimcp_shannon_immune_bridge.h` | Immune system integration |
| Cross-Modal Immune | `nimcp_cross_modal_immune_bridge.h` | Cross-modal immune integration |

#### 26.3 Shannon Information Theory Types

```c
// Synapse-level Shannon metrics (from nimcp_shannon.h)
typedef struct {
    float channel_capacity;      // C bits/second (Shannon capacity)
    float shannon_entropy;       // H(X) bits (uncertainty)
    float mutual_information;    // I(pre;post) bits (correlation)
    float information_rate;      // dI/dt bits/second (throughput)
    float coding_efficiency;     // H/C_max ratio 0-1 (capacity utilization)
    float signal_power;          // Signal strength
    float noise_power;           // Noise level
    float snr;                   // Signal-to-noise ratio
    float bandwidth;             // Effective bandwidth (Hz)
} shannon_synapse_metrics_t;

// Neuron-level Shannon metrics
typedef struct {
    uint64_t neuron_id;          // Neuron identifier
    float state_entropy;         // H(state) bits
    float spike_entropy;         // H(spikes) bits (temporal)
    float input_information;     // Total input info (bits/second)
    float output_information;    // Total output info (bits/second)
    float information_gain;      // Output - Input (bits/second)
    float total_input_capacity;  // Sum of input synapse capacities
    float total_output_capacity; // Sum of output synapse capacities
    uint32_t num_inputs;         // Number of input synapses
    uint32_t num_outputs;        // Number of output synapses
} shannon_neuron_metrics_t;

// Network-level Shannon metrics
typedef struct {
    float total_capacity;        // Sum of all synapse capacities (bits/s)
    float total_entropy;         // Network state entropy (bits)
    float mutual_information;    // Network-level I(input;output)
    float information_rate;      // Current throughput (bits/s)
    float average_efficiency;    // Mean coding efficiency 0-1
    float bottleneck_score;      // 0-1: 1=no bottlenecks, 0=severe
    uint32_t num_bottlenecks;    // Count of low-capacity synapses
    uint32_t num_neurons;        // Total neurons analyzed
    uint32_t num_synapses;       // Total synapses analyzed
} shannon_network_metrics_t;

// Probability distributions
typedef struct {
    float* probabilities;        // p(x) for each state [num_states]
    uint32_t num_states;         // Number of discrete states
    float total_probability;     // Should be 1.0 (validation)
} shannon_distribution_t;

typedef struct {
    float* joint_probabilities;  // p(x,y) [num_x_states × num_y_states]
    uint32_t num_x_states;       // Number of X states
    uint32_t num_y_states;       // Number of Y states
    float total_probability;     // Should be 1.0
} shannon_joint_distribution_t;

// Bottleneck detection
typedef struct {
    uint64_t synapse_id;         // ID of bottleneck synapse
    float capacity;              // Actual capacity (bits/s)
    float demand;                // Desired information flow (bits/s)
    float bottleneck_ratio;      // demand / capacity (>1 = bottleneck)
    float suggested_weight;      // Recommended weight to fix bottleneck
} shannon_bottleneck_t;
```

#### 26.4 Core Shannon Functions

```c
// Shannon-Hartley channel capacity: C = B × log₂(1 + SNR)
float shannon_channel_capacity(float bandwidth, float snr);

// Shannon entropy: H(X) = -Σ p(x) log₂ p(x)
float shannon_entropy(const shannon_distribution_t* distribution);
float shannon_entropy_array(const float* probabilities, uint32_t num_states);

// Mutual information: I(X;Y) = H(X) + H(Y) - H(X,Y)
float shannon_mutual_information(const shannon_joint_distribution_t* joint_distribution);

// Conditional entropy: H(Y|X) = H(X,Y) - H(X)
float shannon_conditional_entropy(const shannon_joint_distribution_t* joint_distribution);

// KL divergence: D(P||Q) = Σ p(x) log₂(p(x)/q(x))
float shannon_kl_divergence(const shannon_distribution_t* p, const shannon_distribution_t* q);

// Synapse analysis
shannon_synapse_metrics_t shannon_analyze_synapse(
    float weight, float pre_firing_rate, float noise_level,
    float bandwidth, const shannon_config_t* config);

// Information-theoretic plasticity: optimize weight for capacity
float shannon_optimize_synapse_weight(
    float current_weight, float target_capacity, float pre_firing_rate,
    float noise_level, float learning_rate);

// Neuron analysis
shannon_neuron_metrics_t shannon_analyze_neuron(
    uint64_t neuron_id,
    const shannon_synapse_metrics_t* input_synapses, uint32_t num_inputs,
    const shannon_synapse_metrics_t* output_synapses, uint32_t num_outputs,
    float neuron_state, const uint64_t* spike_history, uint32_t history_length,
    const shannon_config_t* config);

// Network analysis
shannon_network_metrics_t shannon_analyze_network(
    const shannon_synapse_metrics_t* synapse_metrics, uint32_t num_synapses,
    const shannon_neuron_metrics_t* neuron_metrics, uint32_t num_neurons,
    const shannon_config_t* config);

// Bottleneck detection
uint32_t shannon_detect_bottlenecks(
    const shannon_synapse_metrics_t* synapse_metrics, uint32_t num_synapses,
    float bottleneck_threshold, shannon_bottleneck_t* bottlenecks, uint32_t max_bottlenecks);

// Information flow rate
float shannon_information_flow_rate(
    const shannon_synapse_metrics_t* synapse_metrics, uint32_t num_synapses,
    float time_window_ms);

// Fast log₂ approximation (5-10x faster, ±0.5% error)
float shannon_log2_fast(float x);

// SNR conversion
float shannon_snr_to_db(float snr_linear);
float shannon_snr_from_db(float snr_db);
```

#### 26.5 Cross-Modal Information Flow

```c
// Cross-modal channel (from nimcp_cross_modal.h)
typedef struct {
    char source_modality[32];     // Source cortex (e.g., "visual")
    char dest_modality[32];       // Dest cortex (e.g., "audio")

    // Shannon metrics
    float source_entropy;         // H(source) in bits
    float dest_entropy;           // H(dest) in bits
    float mutual_information;     // I(source;dest) in bits
    float transfer_efficiency;    // I(source;dest) / H(source)
    float channel_capacity;       // Maximum bits/sec
    float information_rate;       // Actual bits/sec

    // Bottleneck detection
    bool is_bottleneck;           // True if efficiency < threshold
    float bottleneck_severity;    // 0-1, higher = more severe

    uint64_t timestamp_ms;
    uint32_t sample_count;
} cross_modal_channel_t;

// Multi-modal integration metrics
typedef struct {
    uint32_t num_modalities;      // Number of input modalities (2-4)
    char modality_names[4][32];   // Modality names

    // Individual entropies
    float individual_entropy[4];  // H(M_i) for each modality

    // Joint metrics
    float joint_entropy;          // H(M1, M2, ..., Mn)
    float total_mutual_info;      // Sum of pairwise I(M_i;M_j)
    float redundancy;             // Overlap between modalities
    float synergy;                // Information only in combination

    // Integration efficiency
    float integration_efficiency; // joint / sum(individual)
    bool is_integrating_well;     // True if efficiency > threshold
} multi_modal_integration_t;

// Routing graph for cross-modal pathways
typedef struct {
    uint32_t num_modalities;
    char modality_names[10][32];
    cross_modal_channel_t* channels[10][10];  // Adjacency matrix
    float total_capacity;
    float total_information_rate;
    float network_efficiency;
    uint32_t num_bottlenecks;
} cross_modal_routing_graph_t;

// Cross-modal API
cross_modal_channel_t cross_modal_analyze_channel(
    const char* source_modality, const char* dest_modality,
    const float* source_features, uint32_t source_dim,
    const float* dest_features, uint32_t dest_dim,
    uint32_t num_samples, const shannon_config_t* config);

multi_modal_integration_t cross_modal_analyze_integration(
    const float** features, const uint32_t* dims, uint32_t num_modalities,
    uint32_t num_samples, const char** modality_names, const shannon_config_t* config);

// Synergy computation (explains McGurk effect)
float cross_modal_compute_synergy(const multi_modal_integration_t* integration);

// Routing graph operations
cross_modal_routing_graph_t* cross_modal_create_routing_graph(
    const char** modality_names, uint32_t num_modalities);
bool cross_modal_update_routing_graph(
    cross_modal_routing_graph_t* graph, uint32_t source_id, uint32_t dest_id,
    const cross_modal_channel_t* channel);
bool cross_modal_detect_bottlenecks(
    const cross_modal_routing_graph_t* graph, float efficiency_threshold,
    cross_modal_channel_t* bottlenecks, uint32_t max_bottlenecks, uint32_t* num_bottlenecks);
float cross_modal_find_optimal_route(
    const cross_modal_routing_graph_t* graph, uint32_t source_id, uint32_t dest_id,
    uint32_t* path, uint32_t max_path_length, uint32_t* path_length);
void cross_modal_destroy_routing_graph(cross_modal_routing_graph_t* graph);
```

#### 26.6 Biological Applications

| Application | Description | NIMCP Integration |
|-------------|-------------|-------------------|
| **Synapse Optimization** | Maximize channel capacity via weight adjustment | Plasticity bridges |
| **Bottleneck Detection** | Identify low-capacity synapses | Network analysis |
| **Information Flow** | Track bits/second through network | Bio-async messaging |
| **McGurk Effect** | Visual + audio synergy in speech | Cross-modal integration |
| **Audiovisual Speech** | Lip reading improves comprehension 30% | Multi-modal integration |
| **Attention Allocation** | Route resources to high-info regions | Attention bridge |
| **Compression** | Preserve information while reducing parameters | Training integration |

#### 26.7 Information Theory Bridges

| Bridge | Header | Purpose |
|--------|--------|---------|
| Shannon-SNN Bridge | `nimcp_shannon_snn_bridge.h` | Information flow in spiking networks |
| Shannon-Plasticity Bridge | `nimcp_shannon_plasticity_bridge.h` | Information-theoretic learning rules |
| Shannon-FEP Bridge | `nimcp_shannon_fep_bridge.h` | Free energy and entropy connection |
| Shannon-Attention Bridge | `nimcp_shannon_attention_bridge.h` | Allocate attention to high-info regions |
| Shannon-Memory Bridge | `nimcp_shannon_memory_bridge.h` | Information content in memories |
| Shannon-Perception Bridge | `nimcp_shannon_perception_bridge.h` | Sensory information metrics |
| Shannon-Language Bridge | `nimcp_shannon_language_bridge.h` | Language information theory |
| Shannon-Reasoning Bridge | `nimcp_shannon_reasoning_bridge.h` | Inference information gain |
| Shannon-Immune Bridge | `nimcp_shannon_immune_bridge.h` | Abnormal information patterns |
| Cross-Modal-Perception Bridge | `nimcp_cross_modal_perception_bridge.h` | Multi-sensory integration |
| Cross-Modal-Language Bridge | `nimcp_cross_modal_language_bridge.h` | Audiovisual speech |
| Cross-Modal-Attention Bridge | `nimcp_cross_modal_attention_bridge.h` | Cross-modal attention |
| Cross-Modal-Immune Bridge | `nimcp_cross_modal_immune_bridge.h` | Cross-modal anomaly detection |
| Shannon-Substrate Bridge | `nimcp_shannon_substrate_bridge.h` | Bio-async information messages |

#### 26.8 Bio-Async Message Types

```c
// Information theory bio-async messages
typedef enum {
    INFO_MSG_ENTROPY_COMPUTED,        // Entropy calculation complete
    INFO_MSG_MUTUAL_INFO_COMPUTED,    // Mutual information computed
    INFO_MSG_BOTTLENECK_DETECTED,     // Information bottleneck found
    INFO_MSG_CAPACITY_CHANGED,        // Channel capacity changed
    INFO_MSG_EFFICIENCY_LOW,          // Coding efficiency below threshold
    INFO_MSG_SYNERGY_DETECTED,        // Cross-modal synergy found
    INFO_MSG_REDUNDANCY_HIGH,         // High redundancy between modalities
    INFO_MSG_OPTIMAL_ROUTE_FOUND,     // Optimal cross-modal route computed
    INFO_MSG_INFORMATION_FLOW_UPDATE  // Information flow rate update
} nimcp_info_theory_msg_type_t;

// Bottleneck detection message
typedef struct {
    nimcp_bio_msg_header_t header;
    uint64_t synapse_id;
    float capacity;
    float demand;
    float bottleneck_ratio;
    float suggested_weight;
} nimcp_bottleneck_msg_t;

// Cross-modal synergy message
typedef struct {
    nimcp_bio_msg_header_t header;
    char source_modality[32];
    char dest_modality[32];
    float synergy_bits;               // Positive = emergence
    float integration_efficiency;
} nimcp_synergy_msg_t;

// Information flow update message
typedef struct {
    nimcp_bio_msg_header_t header;
    float network_capacity;           // Total capacity (bits/s)
    float information_rate;           // Actual throughput (bits/s)
    float efficiency;                 // rate / capacity
    uint32_t num_bottlenecks;
} nimcp_info_flow_msg_t;
```

#### 26.9 Cognitive Integration Table

| Cognitive Module | Information Theory Application |
|------------------|-------------------------------|
| **Attention** | Allocate attention to high mutual information regions |
| **Memory** | Measure information content of memories |
| **Reasoning** | Compute information gain from inference |
| **Perception** | Multi-sensory information integration |
| **Language** | Audiovisual speech synergy (McGurk effect) |
| **Emotion** | Emotional information in cross-modal signals |
| **Executive** | Information-theoretic decision making |
| **Working Memory** | Information capacity constraints |
| **Salience** | High-entropy events as salient |
| **Meta-Learning** | Learning to maximize information gain |

#### 26.10 Brain Factory Integration

```c
// Brain factory information theory initialization
nimcp_status_t nimcp_brain_init_information_theory(
    nimcp_brain_t* brain, const nimcp_brain_config_t* config) {

    NIMCP_LOG_INFO("Initializing information theory layer");

    // Allocate information theory system
    brain->info_theory = nimcp_pool_alloc(brain->pool, sizeof(nimcp_info_theory_system_t));

    // Initialize Shannon analysis
    nimcp_shannon_config_t shannon_config = shannon_default_config();
    brain->info_theory->shannon_config = shannon_config;

    // Initialize cross-modal routing graph
    const char* modalities[] = {"visual", "auditory", "somatosensory", "language", "motor"};
    brain->info_theory->routing_graph = cross_modal_create_routing_graph(modalities, 5);

    // Register bio-async handlers
    nimcp_bio_router_register(brain->router, NIMCP_MSG_TYPE_INFO_BOTTLENECK,
                               info_bottleneck_handler, brain->info_theory);
    nimcp_bio_router_register(brain->router, NIMCP_MSG_TYPE_INFO_SYNERGY,
                               info_synergy_handler, brain->info_theory);
    nimcp_bio_router_register(brain->router, NIMCP_MSG_TYPE_INFO_FLOW_UPDATE,
                               info_flow_handler, brain->info_theory);

    // Register with immune system
    nimcp_immune_register_sensor(brain->immune,
                                  &brain->info_theory->immune_sensor,
                                  IMMUNE_SENSOR_INFORMATION);

    // Initialize bridges
    nimcp_shannon_snn_bridge_init(&brain->info_theory->snn_bridge, brain->snn);
    nimcp_shannon_plasticity_bridge_init(&brain->info_theory->plasticity_bridge, brain->plasticity);
    nimcp_shannon_fep_bridge_init(&brain->info_theory->fep_bridge, brain->fep);
    nimcp_shannon_attention_bridge_init(&brain->info_theory->attention_bridge, brain->attention);
    nimcp_shannon_perception_bridge_init(&brain->info_theory->perception_bridge, brain->perception);
    nimcp_cross_modal_perception_bridge_init(&brain->info_theory->cross_modal_perception, brain->perception);

    NIMCP_LOG_INFO("Information theory layer initialized with %d modalities", 5);

    return NIMCP_OK;
}
```

#### 26.11 Performance Characteristics

| Operation | Complexity | Typical Time |
|-----------|------------|--------------|
| Channel capacity | O(1) | 1 μs |
| Shannon entropy | O(N) states | 10 μs |
| Mutual information | O(N × M) | 100 μs |
| Synapse analysis | O(1) | 5 μs |
| Network analysis | O(S + N) | 1 ms |
| Bottleneck detection | O(S log S) | 5 ms |
| Cross-modal routing | O(V² log V) | 100 μs |

#### 26.12 Information Theory Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/information/test_shannon_entropy.cpp` | 15 |
| `test/unit/information/test_shannon_capacity.cpp` | 10 |
| `test/unit/information/test_mutual_information.cpp` | 15 |
| `test/unit/information/test_kl_divergence.cpp` | 10 |
| `test/unit/information/test_synapse_analysis.cpp` | 15 |
| `test/unit/information/test_neuron_analysis.cpp` | 10 |
| `test/unit/information/test_network_analysis.cpp` | 15 |
| `test/unit/information/test_bottleneck_detection.cpp` | 10 |
| `test/unit/information/test_cross_modal_channel.cpp` | 10 |
| `test/unit/information/test_multi_modal_integration.cpp` | 15 |
| `test/unit/information/test_routing_graph.cpp` | 10 |
| `test/unit/information/test_synergy_computation.cpp` | 10 |
| `test/integration/information/test_shannon_snn_integration.cpp` | 10 |
| `test/integration/information/test_shannon_perception_integration.cpp` | 5 |
| **Total** | **160** |

### 27. Plasticity Module Integration

**Priority**: Critical
**Dependencies**: SNN, Training, Sleep, Immune, Neuromodulators
**Est. LOC**: 40,000 | **Tests**: 350

#### 27.1 Plasticity Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       PLASTICITY COORDINATOR                                 │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                    MECHANISM REGISTRY                                 │  │
│  │  ┌──────┐ ┌──────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │  │
│  │  │ STDP │ │ BCM  │ │Homeosta  │ │Eligibili │ │Dendritic │           │  │
│  │  │      │ │      │ │  tic     │ │   ty     │ │          │           │  │
│  │  └──────┘ └──────┘ └──────────┘ └──────────┘ └──────────┘           │  │
│  │  ┌──────┐ ┌──────────┐ ┌──────────────┐                              │  │
│  │  │ STP  │ │Adaptive  │ │Predictive    │                              │  │
│  │  │      │ │          │ │Coding        │                              │  │
│  │  └──────┘ └──────────┘ └──────────────┘                              │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                     STATE MACHINE                                     │  │
│  │    ACQUISITION → CONSOLIDATION → MAINTENANCE → STABILIZING            │  │
│  │         ↑              ↓              ↓              ↓                │  │
│  │         └──────────────┴──────────────┴──────────────┘                │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │              CONFLICT RESOLUTION & SCHEDULING                         │  │
│  │  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐        │  │
│  │  │  STDP vs BCM    │ │ Time-Multiplex  │ │ Energy Budget   │        │  │
│  │  │  Arbitration    │ │  STP/STDP/BCM   │ │   Tracking      │        │  │
│  │  └─────────────────┘ └─────────────────┘ └─────────────────┘        │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                SUPPORTING MECHANISMS                                  │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌──────────┐ ┌─────────────┐  │  │
│  │  │Astrocyte│ │Calcium  │ │Structu- │ │Metaplas- │ │Heterosynap- │  │  │
│  │  │Plastic. │ │Dynamics │ │ral      │ │ticity    │ │tic          │  │  │
│  │  └─────────┘ └─────────┘ └─────────┘ └──────────┘ └─────────────┘  │  │
│  │  ┌─────────┐ ┌─────────────┐ ┌──────────┐                          │  │
│  │  │Protein  │ │Neuromodula- │ │Pink Noise│                          │  │
│  │  │Synthesis│ │tors         │ │          │                          │  │
│  │  └─────────┘ └─────────────┘ └──────────┘                          │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                   INTEGRATION LAYER                                   │  │
│  │   Brain Immune System        Bio-Async Router        Sleep System    │  │
│  │   ──────────────────         ──────────────────      ────────────    │  │
│  │   Inflammation → LR↓         Inter-mechanism msg     Consolidation   │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 27.2 Core Plasticity Mechanisms (8 Types)

| Mechanism | Header | Mathematical Rule | Timescale |
|-----------|--------|-------------------|-----------|
| **STDP** | `nimcp_stdp.h` | Δw = η × A⁺e^(-Δt/τ⁺) or A⁻e^(Δt/τ⁻) | 10ms |
| **BCM** | `nimcp_bcm.h` | Δw = η × post × (post - θ) × pre | 50ms |
| **Homeostatic** | `nimcp_homeostatic.h` | w_scaled = w × (target/actual)^α | 1s |
| **Eligibility** | `nimcp_eligibility_trace.h` | e(t) = e(t-1) × λ + spike(t) | 10ms |
| **Dendritic** | `nimcp_dendritic.h` | Compartmental plasticity | 10ms |
| **STP** | `nimcp_stp.h` | U, x dynamics (depression/facilitation) | 1ms |
| **Adaptive** | `nimcp_adaptive.h` | Threshold adaptation | 100ms |
| **Predictive** | `nimcp_predictive_coding.h` | Prediction error minimization | 20ms |

#### 27.3 STDP (Spike-Timing-Dependent Plasticity)

```c
// STDP synapse state (from nimcp_stdp.h)
typedef struct {
    // Synaptic weight
    float weight;               // Current weight [0, w_max]
    float w_max;                // Maximum weight (default: 1.0)
    float w_min;                // Minimum weight (default: 0.0)

    // Learning parameters
    float learning_rate;        // Base learning rate (default: 0.01)
    float a_plus;               // LTP amplitude (default: 0.005)
    float a_minus;              // LTD amplitude (default: 0.00525)
    float tau_plus;             // LTP time constant [ms] (default: 20)
    float tau_minus;            // LTD time constant [ms] (default: 20)

    // Spike timing traces
    float pre_trace;            // Presynaptic trace
    float post_trace;           // Postsynaptic trace

    // Dopamine modulation (three-factor learning)
    bool enable_da_modulation;  // Use dopamine modulation
    float da_modulation_gain;   // DA concentration → LR scaling
    float burst_amplification;  // LR multiplier during bursts (3x)

    // Sleep state modulation
    sleep_state_t current_sleep_state;

    // Statistics
    uint64_t num_potentiation_events;
    uint64_t num_depression_events;
    float total_ltp;
    float total_ltd;
} stdp_synapse_t;

// STDP API
void stdp_synapse_init(stdp_synapse_t* synapse);
float stdp_pre_spike(stdp_synapse_t* synapse, float current_time);
float stdp_post_spike(stdp_synapse_t* synapse, float current_time);

// Three-factor learning (dopamine-modulated)
float stdp_pre_spike_modulated(stdp_synapse_t* synapse, float current_time,
                                neuromodulator_system_t neuromod);
float stdp_post_spike_modulated(stdp_synapse_t* synapse, float current_time,
                                 neuromodulator_system_t neuromod);
float stdp_get_da_modulation_factor(const stdp_synapse_t* synapse,
                                     neuromodulator_system_t neuromod);
```

#### 27.4 BCM (Bienenstock-Cooper-Munro) Learning

```c
// BCM synapse state (from nimcp_bcm.h)
typedef struct {
    float weight;             // Synaptic weight (0-1)
    float threshold;          // Sliding modification threshold θ
    float avg_post_activity;  // Running average of post-synaptic activity
    float eligibility;        // Eligibility trace for delayed reward
    sleep_state_t current_sleep_state;
} bcm_synapse_t;

// BCM learning parameters
typedef struct {
    float learning_rate;              // η: Base learning rate (0.001-0.1)
    float threshold_time_constant;    // τ_θ: Threshold adaptation timescale
    float activity_time_constant;     // τ_a: Activity averaging timescale
    float min_threshold;              // Minimum θ (prevents over-depression)
    float max_threshold;              // Maximum θ (prevents runaway)
    bool enable_quantum_bcm;          // Enable quantum threshold optimization
} bcm_params_t;

// BCM Rule: Δw = η × post × (post - θ) × pre
//           θ̇ = (post² - θ) / τ
bcm_synapse_t bcm_synapse_init(float initial_weight, float initial_threshold);
float bcm_update_threshold(bcm_synapse_t* synapse, float post_activity,
                           const bcm_params_t* params, float dt);
float bcm_compute_weight_change(const bcm_synapse_t* synapse, float pre_activity,
                                 float post_activity, const bcm_params_t* params);
```

#### 27.5 Homeostatic Plasticity

```c
// Homeostatic mechanism types (from nimcp_homeostatic.h)
typedef enum {
    HOMEOSTATIC_SYNAPTIC_SCALING,     // Multiplicative scaling of all synapses
    HOMEOSTATIC_INTRINSIC_PLASTICITY, // Threshold/excitability adjustment
    HOMEOSTATIC_METAPLASTICITY,       // Sliding BCM threshold
    HOMEOSTATIC_STRUCTURAL,           // Synapse addition/removal
    HOMEOSTATIC_COMBINED              // Multiple mechanisms together
} homeostatic_mechanism_t;

// Synaptic scaling: w_scaled = w × (target_rate / actual_rate)^α
typedef struct {
    float target_rate;           // Target firing rate (Hz)
    float scaling_time_constant; // τ_scale: Time constant for scaling
    float scaling_exponent;      // α: 0.5=sublinear, 1.0=linear, 2.0=supralinear
    float min_scaling_factor;    // Minimum multiplicative factor
    float max_scaling_factor;    // Maximum multiplicative factor
} synaptic_scaling_params_t;

// Intrinsic plasticity: dθ/dt = (actual_rate - target_rate) / τ_ip
typedef struct {
    float target_rate;           // Target firing rate (Hz)
    float threshold_tau;         // τ_θ: Time constant for threshold adaptation
    float gain_tau;              // τ_g: Time constant for gain adaptation
    float min_threshold;         // Minimum firing threshold
    float max_threshold;         // Maximum firing threshold
} intrinsic_plasticity_params_t;
```

#### 27.6 Plasticity Coordinator

```c
// Plasticity mechanism types (from nimcp_plasticity_coordinator.h)
typedef enum {
    PLASTICITY_TYPE_STDP = 0,          // Spike-timing-dependent plasticity
    PLASTICITY_TYPE_BCM,               // Bienenstock-Cooper-Munro
    PLASTICITY_TYPE_HOMEOSTATIC,       // Homeostatic plasticity
    PLASTICITY_TYPE_ELIGIBILITY,       // Eligibility traces
    PLASTICITY_TYPE_DENDRITIC,         // Dendritic plasticity
    PLASTICITY_TYPE_STP,               // Short-term plasticity
    PLASTICITY_TYPE_ADAPTIVE,          // Adaptive threshold
    PLASTICITY_TYPE_PREDICTIVE,        // Predictive coding
    PLASTICITY_TYPE_COUNT              // Total types (8)
} plasticity_mechanism_type_t;

// Plasticity coordinator states (learning phases)
typedef enum {
    PLASTICITY_STATE_ACQUISITION = 0,  // New learning (STDP+BCM active)
    PLASTICITY_STATE_CONSOLIDATION,    // Memory consolidation
    PLASTICITY_STATE_MAINTENANCE,      // Stable state (minimal plasticity)
    PLASTICITY_STATE_STABILIZING,      // Preventing runaway (homeostatic dominant)
} plasticity_coordinator_state_t;

// Conflict resolution strategies
typedef enum {
    CONFLICT_RESOLUTION_STDP_DOMINANT = 0,  // STDP wins (precise timing)
    CONFLICT_RESOLUTION_BCM_DOMINANT,       // BCM wins (rate-based)
    CONFLICT_RESOLUTION_AVERAGE,            // Average the signals
    CONFLICT_RESOLUTION_WEIGHTED_AVERAGE,   // Weighted by mechanism priority
    CONFLICT_RESOLUTION_IMMUNE_MODULATED,   // Brain immune modulates
    CONFLICT_RESOLUTION_ENERGY_LIMITED,     // Lowest energy cost wins
} conflict_resolution_strategy_t;

// Coordinator API
plasticity_coordinator_t* plasticity_coordinator_create(
    const plasticity_coordinator_config_t* config);
int plasticity_coordinator_register_mechanism(
    plasticity_coordinator_t* coordinator, const char* name,
    plasticity_mechanism_type_t type, plasticity_mechanism_handle_t handle,
    plasticity_mechanism_update_fn_t update_fn, float priority,
    float energy_cost, uint64_t update_interval_ms, uint32_t* mechanism_id_out);
int plasticity_coordinator_update(plasticity_coordinator_t* coordinator,
                                   uint64_t current_time_ms, float dt);
int plasticity_coordinator_resolve_conflict(
    plasticity_coordinator_t* coordinator, uint32_t synapse_id,
    plasticity_mechanism_type_t type_a, float weight_change_a,
    plasticity_mechanism_type_t type_b, float weight_change_b,
    float* resolved_change_out);
```

#### 27.7 Supporting Mechanisms

| Mechanism | Header | Purpose |
|-----------|--------|---------|
| **Astrocyte** | `nimcp_astrocyte_plasticity.h` | Glial modulation of synaptic plasticity |
| **Calcium** | `nimcp_calcium_dynamics.h` | Ca²⁺-dependent plasticity rules |
| **Structural** | `nimcp_structural_plasticity.h` | Synapse formation/elimination |
| **Metaplasticity** | `nimcp_extended_metaplasticity.h` | Plasticity of plasticity |
| **Heterosynaptic** | `nimcp_heterosynaptic.h` | Cross-synapse effects |
| **Protein Synthesis** | `nimcp_protein_synthesis.h` | Late-phase LTP consolidation |
| **Neuromodulators** | `nimcp_spatial_neuromod.h` | Dopamine, serotonin, ACh effects |
| **Pink Noise** | `nimcp_pink_noise.h` | 1/f noise in plasticity |
| **Triplet STDP** | `nimcp_triplet_stdp.h` | Three-spike STDP rule |
| **Quantum STDP** | `nimcp_quantum_stdp_optimizer.h` | Quantum optimization of STDP |

#### 27.8 Energy Costs Per Update

| Mechanism | Energy Cost | Update Interval |
|-----------|-------------|-----------------|
| STP | 0.3 ATP units | 1 ms |
| Eligibility | 0.5 ATP units | 10 ms |
| STDP | 1.0 ATP units | 10 ms |
| Adaptive | 1.0 ATP units | 100 ms |
| Dendritic | 1.5 ATP units | 10 ms |
| BCM | 2.0 ATP units | 50 ms |
| Predictive | 2.5 ATP units | 20 ms |
| Homeostatic | 3.0 ATP units | 1000 ms |

#### 27.9 Plasticity Bridges (100+ Headers)

| Category | Bridges | Purpose |
|----------|---------|---------|
| **FEP Bridges** | `nimcp_stdp_fep_bridge.h`, `nimcp_bcm_fep_bridge.h`, `nimcp_homeostatic_fep_bridge.h`, etc. | Free energy principle integration |
| **Sleep Bridges** | `nimcp_stdp_sleep_bridge.h`, `nimcp_bcm_sleep_bridge.h`, etc. | Sleep-state modulation |
| **Immune Bridges** | `nimcp_stdp_immune_bridge.h`, `nimcp_bcm_immune_bridge.h`, etc. | Inflammation effects |
| **Pink Noise Bridges** | `nimcp_stdp_pink_noise_bridge.h`, `nimcp_bcm_pink_noise_bridge.h`, etc. | 1/f noise modulation |
| **Quantum Bridges** | `nimcp_stdp_quantum_bridge.h`, `nimcp_bcm_quantum_bridge.h`, etc. | Quantum optimization |
| **Substrate Bridges** | `nimcp_plasticity_substrate_bridge.h`, `nimcp_neuromod_substrate_bridge.h` | Bio-async integration |
| **Orchestrator Bridges** | `nimcp_neuron_orchestrator_bridge.h`, `nimcp_axon_orchestrator_bridge.h`, `nimcp_dendrite_orchestrator_bridge.h` | Neural orchestration |
| **Component Bridges** | `nimcp_synapse_plasticity_bridge.h`, `nimcp_dendrite_plasticity_bridge.h`, `nimcp_axon_plasticity_bridge.h` | Neural component integration |

#### 27.10 Bio-Async Message Types

```c
// Plasticity bio-async messages
typedef enum {
    PLASTICITY_MSG_LTP_EVENT,             // Long-term potentiation occurred
    PLASTICITY_MSG_LTD_EVENT,             // Long-term depression occurred
    PLASTICITY_MSG_THRESHOLD_CHANGED,     // BCM/homeostatic threshold changed
    PLASTICITY_MSG_CONSOLIDATION_START,   // Entering consolidation phase
    PLASTICITY_MSG_CONSOLIDATION_END,     // Exiting consolidation phase
    PLASTICITY_MSG_CONFLICT_DETECTED,     // Multiple mechanisms disagree
    PLASTICITY_MSG_CONFLICT_RESOLVED,     // Conflict was resolved
    PLASTICITY_MSG_ENERGY_LOW,            // Energy budget exceeded
    PLASTICITY_MSG_STATE_TRANSITION,      // Coordinator state changed
    PLASTICITY_MSG_STRUCTURAL_CHANGE,     // Synapse added/removed
    PLASTICITY_MSG_METAPLASTICITY_UPDATE  // Plasticity-of-plasticity changed
} nimcp_plasticity_msg_type_t;

// LTP/LTD event message
typedef struct {
    nimcp_bio_msg_header_t header;
    uint64_t synapse_id;
    plasticity_mechanism_type_t mechanism;
    float weight_before;
    float weight_after;
    float weight_change;
    bool is_potentiation;           // true=LTP, false=LTD
    float dopamine_level;           // If modulated
} nimcp_plasticity_event_msg_t;

// Conflict resolution message
typedef struct {
    nimcp_bio_msg_header_t header;
    uint32_t synapse_id;
    plasticity_mechanism_type_t mechanism_a;
    plasticity_mechanism_type_t mechanism_b;
    float weight_change_a;
    float weight_change_b;
    float resolved_weight_change;
    conflict_resolution_strategy_t strategy_used;
} nimcp_plasticity_conflict_msg_t;
```

#### 27.11 Cognitive Module Integration

| Cognitive Module | Plasticity Application |
|------------------|------------------------|
| **Memory** | Eligibility traces → long-term consolidation |
| **Attention** | STDP strengthens attended pathways |
| **Emotion** | Dopamine-modulated three-factor learning |
| **Sleep** | Consolidation phase, homeostatic reset |
| **Learning** | BCM threshold sliding, metaplasticity |
| **Executive** | Predictive coding updates |
| **Reasoning** | Structural plasticity for new connections |

#### 27.12 Brain Factory Integration

```c
// Brain factory plasticity initialization
nimcp_status_t nimcp_brain_init_plasticity(nimcp_brain_t* brain,
                                            const nimcp_brain_config_t* config) {
    NIMCP_LOG_INFO("Initializing plasticity layer");

    // Create plasticity coordinator
    plasticity_coordinator_config_t coord_config;
    plasticity_coordinator_default_config(&coord_config);
    brain->plasticity_coordinator = plasticity_coordinator_create(&coord_config);

    // Initialize core mechanisms
    brain->stdp = stdp_module_init(brain->security_ctx);
    brain->bcm = bcm_module_init(&brain->bcm_config);
    brain->homeostatic = homeostatic_system_create(&brain->homeostatic_config);
    brain->eligibility = eligibility_system_create();
    brain->stp = stp_system_create();

    // Register mechanisms with coordinator
    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "stdp_main", PLASTICITY_TYPE_STDP,
        brain->stdp, stdp_update_fn, NULL, 0.8f, PLASTICITY_ENERGY_COST_STDP,
        PLASTICITY_UPDATE_INTERVAL_STDP, NULL);

    plasticity_coordinator_register_mechanism(
        brain->plasticity_coordinator, "bcm_main", PLASTICITY_TYPE_BCM,
        brain->bcm, bcm_update_fn, NULL, 0.6f, PLASTICITY_ENERGY_COST_BCM,
        PLASTICITY_UPDATE_INTERVAL_BCM, NULL);

    // Connect to brain immune
    plasticity_coordinator_connect_brain_immune(brain->plasticity_coordinator,
                                                 brain->immune);

    // Connect to bio-async
    plasticity_coordinator_connect_bio_async(brain->plasticity_coordinator);

    // Initialize all bridges
    nimcp_stdp_fep_bridge_init(&brain->stdp_fep_bridge, brain->fep);
    nimcp_stdp_sleep_bridge_init(&brain->stdp_sleep_bridge, brain->sleep);
    nimcp_bcm_fep_bridge_init(&brain->bcm_fep_bridge, brain->fep);
    nimcp_homeostatic_fep_bridge_init(&brain->homeostatic_fep_bridge, brain->fep);

    NIMCP_LOG_INFO("Plasticity layer initialized: 8 mechanisms, coordinator active");

    return NIMCP_OK;
}
```

#### 27.13 Plasticity Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/plasticity/test_stdp.cpp` | 25 |
| `test/unit/plasticity/test_stdp_dopamine_modulation.cpp` | 20 |
| `test/unit/plasticity/test_bcm.cpp` | 20 |
| `test/unit/plasticity/test_bcm_threshold.cpp` | 15 |
| `test/unit/plasticity/test_homeostatic_scaling.cpp` | 20 |
| `test/unit/plasticity/test_homeostatic_intrinsic.cpp` | 15 |
| `test/unit/plasticity/test_eligibility_traces.cpp` | 15 |
| `test/unit/plasticity/test_stp.cpp` | 15 |
| `test/unit/plasticity/test_dendritic.cpp` | 15 |
| `test/unit/plasticity/test_predictive_coding.cpp` | 15 |
| `test/unit/plasticity/test_structural_plasticity.cpp` | 10 |
| `test/unit/plasticity/test_metaplasticity.cpp` | 15 |
| `test/unit/plasticity/test_calcium_dynamics.cpp` | 15 |
| `test/unit/plasticity/test_protein_synthesis.cpp` | 10 |
| `test/unit/plasticity/test_astrocyte_plasticity.cpp` | 10 |
| `test/unit/plasticity/test_coordinator.cpp` | 25 |
| `test/unit/plasticity/test_conflict_resolution.cpp` | 20 |
| `test/unit/plasticity/test_coordinator_state_machine.cpp` | 15 |
| `test/integration/plasticity/test_stdp_bcm_integration.cpp` | 15 |
| `test/integration/plasticity/test_plasticity_sleep_integration.cpp` | 10 |
| `test/integration/plasticity/test_plasticity_immune_integration.cpp` | 10 |
| **Total** | **350** |

### 28. Security Module Integration

**Priority**: Critical
**Dependencies**: Blood-Brain Barrier, Immune System, Bio-Async, Encryption
**Est. LOC**: 45,000 | **Tests**: 400

#### 28.1 Security Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           NIMCP SECURITY LAYER                               │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                    BLOOD-BRAIN BARRIER (Perimeter)                     │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐      │  │
│  │  │   Input     │ │    Code     │ │   Memory    │ │   Access    │      │  │
│  │  │ Validation  │ │  Signing    │ │  Boundary   │ │   Control   │      │  │
│  │  │   Gates     │ │ Verifier    │ │  Monitor    │ │  Enforcer   │      │  │
│  │  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘      │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    THREAT DETECTION LAYER                             │  │
│  │  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐        │  │
│  │  │    Anomaly      │ │    Pattern      │ │   Rate          │        │  │
│  │  │   Detector      │ │    Database     │ │   Limiter       │        │  │
│  │  │  (Bayesian ML)  │ │   (Signatures)  │ │  (Token Bucket) │        │  │
│  │  └─────────────────┘ └─────────────────┘ └─────────────────┘        │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                     POLICY ENGINE                                     │  │
│  │    ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐      │  │
│  │    │  NSPL   │ ──▶ │   AST   │ ──▶ │Bytecode │ ──▶ │  Eval   │      │  │
│  │    │ Parser  │     │         │     │Compiler │     │ Engine  │      │  │
│  │    └─────────┘     └─────────┘     └─────────┘     └─────────┘      │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                   CRYPTOGRAPHY LAYER                                  │  │
│  │  ┌─────────────┐ ┌─────────────────┐ ┌─────────────────────────────┐│  │
│  │  │  AES-256   │ │  Post-Quantum   │ │       Key Derivation        ││  │
│  │  │    GCM     │ │  Kyber+Dilithium│ │  HKDF, Argon2, scrypt       ││  │
│  │  └─────────────┘ └─────────────────┘ └─────────────────────────────┘│  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                   INTEGRATION LAYER                                   │  │
│  │   Brain Immune System        Bio-Async Router        Audit Logger    │  │
│  │   ──────────────────         ──────────────────      ────────────    │  │
│  │   Threat → Antigen           Security messaging      Encrypted logs  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 28.2 Core Security Components (44 Headers)

| Component | Header | Purpose |
|-----------|--------|---------|
| **Core Security** | `nimcp_security.h` | Directive protection, input validation, encryption |
| **Blood-Brain Barrier** | `nimcp_blood_brain_barrier.h` | 4-layer perimeter defense |
| **Anomaly Detector** | `nimcp_anomaly_detector.h` | Bayesian ML-based detection |
| **Rate Limiter** | `nimcp_rate_limiter.h` | Token bucket/sliding window rate limiting |
| **Policy Engine** | `nimcp_policy_engine.h` | Declarative security policy language |
| **Policy AST** | `nimcp_policy_ast.h` | Abstract syntax tree for policies |
| **Policy Parser** | `nimcp_policy_parser.h` | NSPL language parser |
| **Post-Quantum** | `nimcp_post_quantum.h` | CRYSTALS-Kyber/Dilithium cryptography |
| **Key Derivation** | `nimcp_key_derivation.h` | HKDF, Argon2, scrypt |
| **Pattern Database** | `nimcp_pattern_db.h` | Threat signature matching |
| **Continuous Monitor** | `nimcp_continuous_monitor.h` | Real-time security monitoring |
| **Encrypted Audit** | `nimcp_encrypted_audit.h` | Tamper-proof audit logs |
| **Security Audit** | `nimcp_security_audit.h` | Security event auditing |
| **Security Integration** | `nimcp_security_integration.h` | Cross-module integration |
| **Capability** | `nimcp_capability.h` | Capability-based access control |
| **CFI** | `nimcp_cfi.h` | Control-flow integrity |
| **Shadow Stack** | `nimcp_shadow_stack.h` | Return address protection |
| **TOCTOU Guard** | `nimcp_toctou_guard.h` | Time-of-check/time-of-use protection |
| **Path Traversal** | `nimcp_path_traversal.h` | Path injection prevention |
| **Shell Detector** | `nimcp_shell_detector.h` | Shell injection detection |
| **Supply Chain** | `nimcp_supply_chain.h` | Supply chain security |
| **Security Math** | `nimcp_security_math.h` | Constant-time operations |
| **Constant Time** | `nimcp_constant_time.h` | Side-channel resistant operations |
| **Security Level** | `nimcp_security_level.h` | Security level management |
| **Security Coverage** | `nimcp_security_coverage.h` | Security coverage analysis |
| **Security Consensus** | `nimcp_security_consensus.h` | Distributed security consensus |
| **Security Fractal** | `nimcp_security_fractal.h` | Fractal security patterns |
| **BBB Helpers** | `nimcp_bbb_helpers.h` | BBB utility functions |
| **BBB Enhanced Detection** | `nimcp_bbb_enhanced_detection.h` | Enhanced threat detection |
| **Checkpoints** | `nimcp_checkpoints.h` | Security checkpoints |
| **Security Recovery Bridge** | `nimcp_security_recovery_bridge.h` | Recovery integration |
| **Security Perception Bridge** | `nimcp_security_perception_bridge.h` | Perception integration |
| **Security FEP Bridge** | `nimcp_security_fep_bridge.h` | Free energy principle integration |

#### 28.3 Blood-Brain Barrier (BBB) - Perimeter Defense

```c
// BBB threat types (from nimcp_blood_brain_barrier.h)
typedef enum {
    BBB_THREAT_NONE = 0,              // No threat detected
    BBB_THREAT_BUFFER_OVERFLOW,       // Stack/heap buffer overflow
    BBB_THREAT_FORMAT_STRING,         // Format string vulnerability
    BBB_THREAT_INTEGER_OVERFLOW,      // Integer overflow/underflow
    BBB_THREAT_SQL_INJECTION,         // SQL injection attempt
    BBB_THREAT_CODE_INJECTION,        // Generic code injection
    BBB_THREAT_SHELLCODE,             // Shellcode detected
    BBB_THREAT_ROP_CHAIN,             // Return-Oriented Programming
    BBB_THREAT_INVALID_SIGNATURE,     // Code signature invalid
    BBB_THREAT_MEMORY_VIOLATION,      // Memory bounds violation
    BBB_THREAT_UNAUTHORIZED_ACCESS,   // Access control violation
    BBB_THREAT_DATA_TAMPERING,        // Data integrity violation
    BBB_THREAT_PATH_TRAVERSAL,        // Path traversal attack
    BBB_THREAT_SHELL_INJECTION,       // Shell command injection
    BBB_THREAT_UNKNOWN                // Unknown threat type
} bbb_threat_type_t;

// BBB severity levels
typedef enum {
    BBB_SEVERITY_NONE = 0,            // No threat
    BBB_SEVERITY_LOW = 1,             // Log only
    BBB_SEVERITY_MEDIUM = 2,          // Block and alert
    BBB_SEVERITY_HIGH = 3,            // Quarantine and alert
    BBB_SEVERITY_CRITICAL = 4         // System lockdown
} bbb_severity_t;

// BBB configuration
typedef struct {
    bbb_input_config_t input;         // Input gate configuration
    bbb_signing_config_t signing;     // Code signing configuration
    bbb_memory_config_t memory;       // Memory boundary configuration
    bbb_access_config_t access;       // Access control configuration
    bool strict_mode;                 // Enable strict security mode
    bbb_action_t default_action;      // Default action for threats
    void (*alert_callback)(bbb_threat_type_t, bbb_severity_t, const char*);
} bbb_config_t;

// BBB API
bbb_system_t bbb_system_create(const bbb_config_t* config);
bool bbb_validate_input(bbb_system_t system, const void* data, size_t size,
                        bbb_validation_result_t* result);
bool bbb_validate_string(bbb_system_t system, const char* str,
                         bbb_validation_result_t* result);
bool bbb_verify_signature(bbb_system_t system, const void* data, size_t size,
                          const uint8_t* signature, size_t sig_size);
bool bbb_check_memory_access(bbb_system_t system, const void* address,
                             size_t size, bool write);
bool bbb_check_access(bbb_system_t system, const bbb_subject_t* subject,
                      const bbb_object_t* object, uint32_t access_type);
bool bbb_quarantine_region(bbb_system_t system, void* address, size_t size);
bool bbb_connect_immune(bbb_system_t system, brain_immune_system_t* immune_system);
```

#### 28.4 Anomaly Detector (Bayesian ML)

```c
// Anomaly detection features (from nimcp_anomaly_detector.h)
#define NIMCP_FEATURE_LENGTH 0
#define NIMCP_FEATURE_ENTROPY 1
#define NIMCP_FEATURE_ALPHA_RATIO 2
#define NIMCP_FEATURE_NUMERIC_RATIO 3
#define NIMCP_FEATURE_SPECIAL_RATIO 4
#define NIMCP_FEATURE_CONTROL_RATIO 5
#define NIMCP_FEATURE_BIGRAM_ENTROPY 6
#define NIMCP_FEATURE_TRIGRAM_ENTROPY 7
#define NIMCP_FEATURE_NESTING_DEPTH 8
#define NIMCP_FEATURE_REQUEST_RATE 9
#define NIMCP_FEATURE_BURST_SCORE 10
#define NIMCP_FEATURE_REPEAT_RATIO 11
#define NIMCP_FEATURE_COUNT 12

// Anomaly detection result
typedef struct {
    float anomaly_score;               // 0.0 (normal) to 1.0 (highly anomalous)
    float confidence;                  // Confidence in score [0.0, 1.0]
    float content_score;               // Content anomaly score
    float behavior_score;              // Behavior anomaly score
    float timing_score;                // Timing anomaly score
    uint32_t triggered_features;       // Bitmask of triggered features
    char explanation[256];             // Human-readable explanation
    uint64_t timestamp_us;             // Detection timestamp
} nimcp_anomaly_result_t;

// Anomaly detector API
nimcp_anomaly_detector_t nimcp_anomaly_detector_create(const nimcp_anomaly_config_t* config);
nimcp_error_t nimcp_anomaly_detect(nimcp_anomaly_detector_t detector, const void* input,
                                    size_t input_len, nimcp_anomaly_result_t* result);
nimcp_error_t nimcp_anomaly_train(nimcp_anomaly_detector_t detector, const void* input,
                                   size_t input_len, bool is_normal);
nimcp_error_t nimcp_anomaly_save_model(nimcp_anomaly_detector_t detector, const char* filepath);
nimcp_error_t nimcp_anomaly_load_model(nimcp_anomaly_detector_t detector, const char* filepath);

// Bayesian network API
nimcp_bayesian_network_t nimcp_bn_create(uint32_t num_nodes);
nimcp_error_t nimcp_bn_add_edge(nimcp_bayesian_network_t bn, uint32_t parent, uint32_t child);
nimcp_error_t nimcp_bn_infer(nimcp_bayesian_network_t bn, const float* evidence, float* posteriors);
nimcp_error_t nimcp_bn_learn(nimcp_bayesian_network_t bn, const float* sample);
```

#### 28.5 Rate Limiter (Token Bucket)

```c
// Rate limiting algorithms (from nimcp_rate_limiter.h)
typedef enum {
    RATE_LIMIT_TOKEN_BUCKET = 0,  // Classic token bucket
    RATE_LIMIT_SLIDING_WINDOW,    // Sliding window counter
    RATE_LIMIT_FIXED_WINDOW,      // Fixed window counter
    RATE_LIMIT_LEAKY_BUCKET       // Leaky bucket (smoother)
} nimcp_rate_limit_algorithm_t;

// Penalty actions
typedef enum {
    PENALTY_NONE = 0,
    PENALTY_WARN,
    PENALTY_REDUCE_RATE_25,       // Reduce rate by 25%
    PENALTY_REDUCE_RATE_50,       // Reduce rate by 50%
    PENALTY_REDUCE_RATE_75,       // Reduce rate by 75%
    PENALTY_BLOCK_TEMPORARY,
    PENALTY_BLOCK_PERMANENT
} nimcp_penalty_action_t;

// Rate limiter configuration
typedef struct {
    float requests_per_second;      // Rate limit (requests/sec)
    uint32_t burst_size;            // Maximum burst tokens
    nimcp_rate_limit_algorithm_t algorithm;
    bool per_client;                // Separate limit per client
    bool per_resource;              // Separate limit per resource
    nimcp_penalty_config_t penalty; // Penalty configuration
    bool enable_statistics;
} nimcp_rate_limit_config_t;

// Rate limiter API
nimcp_rate_limiter_t nimcp_rate_limiter_create(const nimcp_rate_limit_config_t* config);
bool nimcp_rate_limiter_allow(nimcp_rate_limiter_t limiter, const char* client_id);
bool nimcp_rate_limiter_acquire(nimcp_rate_limiter_t limiter, const char* client_id, uint32_t count);
uint64_t nimcp_rate_limiter_time_until_ready(nimcp_rate_limiter_t limiter, const char* client_id);
nimcp_error_t nimcp_rate_limiter_block_client(nimcp_rate_limiter_t limiter, const char* client_id);
nimcp_error_t nimcp_rate_limiter_register_bio_async(nimcp_rate_limiter_t limiter);
```

#### 28.6 Policy Engine (NSPL Language)

```c
// Policy action types (from nimcp_policy_engine.h)
typedef enum {
    NIMCP_POLICY_ACTION_ALLOW,
    NIMCP_POLICY_ACTION_DENY,
    NIMCP_POLICY_ACTION_THROTTLE,
    NIMCP_POLICY_ACTION_LOG,
    NIMCP_POLICY_ACTION_ALERT,
    NIMCP_POLICY_ACTION_CUSTOM
} nimcp_policy_action_t;

// Policy evaluation result
typedef struct {
    nimcp_policy_action_t action;
    nimcp_policy_severity_t severity;
    char* message;
    char* rule_name;
    nimcp_policy_value_t* params;
    size_t num_params;
    bool should_log;
    uint64_t eval_time_ns;
} nimcp_policy_result_t;

// Policy engine API
nimcp_policy_engine_t nimcp_policy_engine_create(const nimcp_policy_engine_config_t* config);
nimcp_error_t nimcp_policy_engine_load(nimcp_policy_engine_t engine, const char* policy_text,
                                        nimcp_policy_t* policy);
nimcp_error_t nimcp_policy_engine_load_file(nimcp_policy_engine_t engine, const char* filepath,
                                             nimcp_policy_t* policy);
nimcp_error_t nimcp_policy_evaluate(nimcp_policy_engine_t engine, nimcp_policy_context_t ctx,
                                     nimcp_policy_result_t* result);
nimcp_error_t nimcp_policy_engine_reload(nimcp_policy_engine_t engine);  // Hot-reload
```

#### 28.7 Post-Quantum Cryptography

```c
// Kyber security variants (from nimcp_post_quantum.h)
typedef enum {
    NIMCP_PQ_KYBER_512,   // NIST Level 1 - 128-bit security
    NIMCP_PQ_KYBER_768,   // NIST Level 3 - 192-bit security
    NIMCP_PQ_KYBER_1024   // NIST Level 5 - 256-bit security
} nimcp_kyber_variant_t;

// Dilithium security variants
typedef enum {
    NIMCP_PQ_DILITHIUM_2,  // NIST Level 2 - 128-bit security
    NIMCP_PQ_DILITHIUM_3,  // NIST Level 3 - 192-bit security
    NIMCP_PQ_DILITHIUM_5   // NIST Level 5 - 256-bit security
} nimcp_dilithium_variant_t;

// Kyber KEM API
nimcp_error_t nimcp_kyber_keygen(nimcp_kyber_variant_t variant, nimcp_kyber_keypair_t* keypair);
nimcp_error_t nimcp_kyber_encapsulate(nimcp_kyber_variant_t variant, const uint8_t* public_key,
                                       uint8_t* ciphertext, size_t* ciphertext_len,
                                       uint8_t* shared_secret, size_t shared_secret_len);
nimcp_error_t nimcp_kyber_decapsulate(nimcp_kyber_variant_t variant, const uint8_t* secret_key,
                                       const uint8_t* ciphertext, size_t ciphertext_len,
                                       uint8_t* shared_secret, size_t shared_secret_len);

// Dilithium signature API
nimcp_error_t nimcp_dilithium_keygen(nimcp_dilithium_variant_t variant,
                                      nimcp_dilithium_keypair_t* keypair);
nimcp_error_t nimcp_dilithium_sign(nimcp_dilithium_variant_t variant, const uint8_t* secret_key,
                                    const uint8_t* message, size_t message_len,
                                    uint8_t* signature, size_t* signature_len);
nimcp_error_t nimcp_dilithium_verify(nimcp_dilithium_variant_t variant, const uint8_t* public_key,
                                      const uint8_t* message, size_t message_len,
                                      const uint8_t* signature, size_t signature_len);

// Hybrid classical+PQ operations
nimcp_error_t nimcp_hybrid_key_exchange(nimcp_pq_context_t ctx, const uint8_t* classical_private,
                                         const uint8_t* classical_public, const uint8_t* pq_public,
                                         size_t pq_public_len, uint8_t* combined_secret,
                                         size_t secret_len);
nimcp_error_t nimcp_hybrid_sign(nimcp_pq_context_t ctx, const uint8_t* classical_key,
                                 const uint8_t* pq_key, size_t pq_key_len,
                                 const uint8_t* message, size_t message_len,
                                 uint8_t* signature, size_t* signature_len);
```

#### 28.8 Biological Attack Defense

```c
// Biological attack types (from nimcp_security.h)
typedef enum {
    NIMCP_BIO_ATTACK_NONE = 0,
    NIMCP_BIO_ATTACK_EXCITOTOXICITY,      // Runaway excitation
    NIMCP_BIO_ATTACK_SYNAPTIC_POISONING,  // Malicious weight updates
    NIMCP_BIO_ATTACK_NEUROMOD_HIJACK,     // Dopamine manipulation
    NIMCP_BIO_ATTACK_HEBBIAN_POISON,      // STDP exploitation
    NIMCP_BIO_ATTACK_HOMEOSTATIC_BYPASS   // BCM/eligibility disable
} nimcp_bio_attack_type_t;

// Biological defense thresholds
#define NIMCP_ACTIVITY_WARNING_THRESHOLD 0.8f   // 80% network activity - warning
#define NIMCP_ACTIVITY_DANGER_THRESHOLD 0.95f   // 95% network activity - emergency
#define NIMCP_MAX_WEIGHT_DELTA_PER_STEP 0.1f    // Max 10% weight change per step
#define NIMCP_MAX_NEUROMOD_RATE 0.2f            // Max 20% neuromodulator change per step

// Biological defense API
nimcp_bio_attack_type_t nimcp_security_monitor_excitotoxicity(void* network,
                                                               nimcp_activity_stats_t* stats);
bool nimcp_security_validate_weight_change(float old_weight, float new_weight, float max_delta);
bool nimcp_security_validate_neuromodulator_change(float old_level, float new_level, float max_rate);
nimcp_bio_attack_type_t nimcp_security_verify_plasticity_integrity(void* network,
                                                                     uint32_t* bcm_disabled,
                                                                     uint32_t* elig_disabled);
nimcp_result_t nimcp_security_emergency_inhibit(void* network);
```

#### 28.9 Security Bridges

| Bridge Type | Headers | Purpose |
|-------------|---------|---------|
| **FEP Bridges** | `nimcp_security_fep_bridge.h`, `nimcp_anomaly_detector_fep_bridge.h`, `nimcp_rate_limiter_fep_bridge.h`, `nimcp_blood_brain_barrier_fep_bridge.h`, `nimcp_pattern_db_fep_bridge.h` | Free energy principle integration |
| **Sleep Bridges** | `security/sleep/nimcp_bbb_sleep_bridge.h`, `security/sleep/nimcp_anomaly_detector_sleep_bridge.h`, `security/sleep/nimcp_rate_limiter_sleep_bridge.h`, `security/sleep/nimcp_pattern_db_sleep_bridge.h` | Sleep-state security modulation |
| **Immune Bridges** | `security/immune/nimcp_anomaly_immune_bridge.h`, `security/immune/nimcp_rate_limiter_immune_bridge.h`, `security/immune/nimcp_pattern_db_immune_bridge.h` | Threat → Antigen presentation |
| **Recovery Bridge** | `nimcp_security_recovery_bridge.h` | Security incident recovery |
| **Perception Bridge** | `nimcp_security_perception_bridge.h` | Perceptual security analysis |

#### 28.10 Bio-Async Message Types

```c
// Security bio-async messages
typedef enum {
    SECURITY_MSG_THREAT_DETECTED,         // Threat detected by any component
    SECURITY_MSG_THREAT_BLOCKED,          // Threat was blocked
    SECURITY_MSG_THREAT_QUARANTINED,      // Threat was quarantined
    SECURITY_MSG_ANOMALY_DETECTED,        // Anomaly detector triggered
    SECURITY_MSG_RATE_LIMIT_EXCEEDED,     // Rate limit exceeded
    SECURITY_MSG_POLICY_VIOLATION,        // Policy engine denied action
    SECURITY_MSG_SIGNATURE_INVALID,       // Code signature verification failed
    SECURITY_MSG_MEMORY_VIOLATION,        // Memory boundary violation
    SECURITY_MSG_ACCESS_DENIED,           // Access control denied
    SECURITY_MSG_BIO_ATTACK_DETECTED,     // Biological attack detected
    SECURITY_MSG_LOCKDOWN_INITIATED,      // System entering lockdown
    SECURITY_MSG_LOCKDOWN_RELEASED,       // Lockdown released
    SECURITY_MSG_KEY_ROTATION,            // Cryptographic key rotated
    SECURITY_MSG_AUDIT_EVENT              // Security audit event
} nimcp_security_msg_type_t;

// Threat detection message
typedef struct {
    nimcp_bio_msg_header_t header;
    bbb_threat_type_t threat_type;
    bbb_severity_t severity;
    bbb_action_t action_taken;
    uint64_t source_module_id;
    uint8_t threat_hash[32];
    char description[256];
} nimcp_security_threat_msg_t;

// Anomaly detection message
typedef struct {
    nimcp_bio_msg_header_t header;
    float anomaly_score;
    float confidence;
    uint32_t triggered_features;
    char explanation[256];
} nimcp_security_anomaly_msg_t;

// Bio attack message
typedef struct {
    nimcp_bio_msg_header_t header;
    nimcp_bio_attack_type_t attack_type;
    float activity_level;
    uint32_t affected_neurons;
    bool emergency_response_triggered;
} nimcp_security_bio_attack_msg_t;
```

#### 28.11 Brain Factory Integration

```c
// Brain factory security initialization
nimcp_status_t nimcp_brain_init_security(nimcp_brain_t* brain,
                                          const nimcp_brain_config_t* config) {
    NIMCP_LOG_INFO("Initializing security layer");

    // Create Blood-Brain Barrier
    bbb_config_t bbb_config = bbb_default_config();
    bbb_config.strict_mode = config->security_strict_mode;
    brain->bbb = bbb_system_create(&bbb_config);

    // Connect BBB to immune system
    bbb_connect_immune(brain->bbb, brain->immune);

    // Create anomaly detector
    nimcp_anomaly_config_t anomaly_config = nimcp_anomaly_detector_default_config();
    anomaly_config.enable_bio_async = true;
    brain->anomaly_detector = nimcp_anomaly_detector_create(&anomaly_config);

    // Create rate limiter
    nimcp_rate_limit_config_t rl_config = nimcp_rate_limiter_default_config();
    rl_config.requests_per_second = config->max_requests_per_second;
    brain->rate_limiter = nimcp_rate_limiter_create(&rl_config);

    // Create policy engine
    nimcp_policy_engine_config_t pe_config = {
        .max_policies = 100,
        .enable_hot_reload = true,
        .policy_directory = config->policy_directory,
        .bio_router = brain->bio_router
    };
    brain->policy_engine = nimcp_policy_engine_create(&pe_config);

    // Initialize post-quantum cryptography
    nimcp_pq_config_t pq_config = {
        .default_kyber_variant = NIMCP_PQ_KYBER_768,
        .default_dilithium_variant = NIMCP_PQ_DILITHIUM_3,
        .hybrid_config = { .enable_classical = true, .enable_pq = true, .require_both = true }
    };
    brain->pq_context = nimcp_pq_context_create(&pq_config);

    // Register with bio-async
    nimcp_rate_limiter_register_bio_async(brain->rate_limiter);

    // Initialize all security bridges
    nimcp_security_fep_bridge_init(&brain->security_fep_bridge, brain->fep);
    nimcp_bbb_sleep_bridge_init(&brain->bbb_sleep_bridge, brain->sleep);
    nimcp_anomaly_immune_bridge_init(&brain->anomaly_immune_bridge, brain->immune);

    // Load security policies
    if (config->security_policy_file) {
        nimcp_policy_t policy;
        nimcp_policy_engine_load_file(brain->policy_engine, config->security_policy_file, &policy);
    }

    NIMCP_LOG_INFO("Security layer initialized: BBB active, anomaly detector trained, "
                   "post-quantum crypto enabled");

    return NIMCP_OK;
}
```

#### 28.12 Security Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/security/test_bbb_input_validation.cpp` | 30 |
| `test/unit/security/test_bbb_code_signing.cpp` | 25 |
| `test/unit/security/test_bbb_memory_boundary.cpp` | 25 |
| `test/unit/security/test_bbb_access_control.cpp` | 25 |
| `test/unit/security/test_bbb_threat_detection.cpp` | 20 |
| `test/unit/security/test_anomaly_detector.cpp` | 30 |
| `test/unit/security/test_anomaly_bayesian_network.cpp` | 20 |
| `test/unit/security/test_anomaly_feature_extraction.cpp` | 15 |
| `test/unit/security/test_rate_limiter_token_bucket.cpp` | 20 |
| `test/unit/security/test_rate_limiter_sliding_window.cpp` | 15 |
| `test/unit/security/test_rate_limiter_penalties.cpp` | 15 |
| `test/unit/security/test_policy_engine_parser.cpp` | 20 |
| `test/unit/security/test_policy_engine_eval.cpp` | 20 |
| `test/unit/security/test_policy_hot_reload.cpp` | 10 |
| `test/unit/security/test_post_quantum_kyber.cpp` | 20 |
| `test/unit/security/test_post_quantum_dilithium.cpp` | 20 |
| `test/unit/security/test_post_quantum_hybrid.cpp` | 15 |
| `test/unit/security/test_bio_attack_defense.cpp` | 20 |
| `test/unit/security/test_encryption_aes_gcm.cpp` | 15 |
| `test/integration/security/test_bbb_immune_integration.cpp` | 15 |
| `test/integration/security/test_security_bio_async_integration.cpp` | 15 |
| `test/integration/security/test_security_policy_integration.cpp` | 10 |
| **Total** | **400** |

### 29. Async Module Integration

**Priority**: Critical (Core Infrastructure)
**Dependencies**: All modules depend on async for communication
**Est. LOC**: 50,000 | **Tests**: 350

#### 29.1 Async Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        BIO-ASYNC COORDINATION LAYER                          │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                    NEUROMODULATOR CHANNELS                            │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐                │  │
│  │  │ Dopamine │ │Serotonin │ │Norepine- │ │Acetylcho-│                │  │
│  │  │   (DA)   │ │  (5-HT)  │ │phrine(NE)│ │line(ACh) │                │  │
│  │  │Reward/   │ │Mood/     │ │Alertness/│ │Attention/│                │  │
│  │  │Motivation│ │Wellbeing │ │Arousal   │ │Learning  │                │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘                │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                     PHASE COUPLING (KURAMOTO)                         │  │
│  │    ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐     │  │
│  │    │ Delta   │ │ Theta   │ │ Alpha   │ │ Beta    │ │ Gamma   │     │  │
│  │    │ 0.5-4Hz │ │ 4-8Hz   │ │ 8-13Hz  │ │ 13-30Hz │ │ 30-100Hz│     │  │
│  │    │Deep     │ │Memory   │ │Relax    │ │Active   │ │Binding  │     │  │
│  │    │Sleep    │ │Encoding │ │Idle     │ │Thinking │ │Attention│     │  │
│  │    └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    PREDICTIVE CODING (BAYESIAN)                       │  │
│  │  ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐  │  │
│  │  │ Prediction Engine │ │ Error Computation │ │ Precision Weight  │  │  │
│  │  │ P(x|context)      │ │ error = actual -  │ │ π = 1/σ² controls │  │  │
│  │  │                   │ │     predicted     │ │ belief updates    │  │  │
│  │  └───────────────────┘ └───────────────────┘ └───────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    GLIAL WAVE PROPAGATION                             │  │
│  │  ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐  │  │
│  │  │ Calcium Waves     │ │ Astrocyte Network │ │ Neuromodulator    │  │  │
│  │  │ IP3-mediated      │ │ Gap junctions     │ │ Uptake/Release    │  │  │
│  │  │ 20μm/s propagation│ │ Spatial buffering │ │ Glutamate cycling │  │  │
│  │  └───────────────────┘ └───────────────────┘ └───────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                        BIO-ROUTER                                     │  │
│  │   ┌─────────────────────────────────────────────────────────────┐   │  │
│  │   │                  MODULE REGISTRY                             │   │  │
│  │   │  Module → Handler Registration → Inbox/Outbox Queues        │   │  │
│  │   └─────────────────────────────────────────────────────────────┘   │  │
│  │   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐              │  │
│  │   │ Send/Receive │ │ Broadcast    │ │ Brain KG     │              │  │
│  │   │ Point-to-    │ │ Publish/     │ │ Integration  │              │  │
│  │   │ Point        │ │ Subscribe    │ │ Sync state   │              │  │
│  │   └──────────────┘ └──────────────┘ └──────────────┘              │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                      WIRING DIAGRAM                                   │  │
│  │   ┌─────────────────────────────────────────────────────────────┐   │  │
│  │   │                  JSONL CONFIGURATION                         │   │  │
│  │   │  Hardware Profiles → Subsystem Registry → Dependencies      │   │  │
│  │   └─────────────────────────────────────────────────────────────┘   │  │
│  │   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐              │  │
│  │   │ Dynamic Load │ │ Brain KG     │ │ Validation   │              │  │
│  │   │ Runtime      │ │ Sync         │ │ Dependency   │              │  │
│  │   │ Module Init  │ │ Bidirectional│ │ Resolution   │              │  │
│  │   └──────────────┘ └──────────────┘ └──────────────┘              │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 29.2 Bio-Async Core Components (nimcp_bio_async.h)

| Component | Type/Struct | Purpose | Biological Basis |
|-----------|-------------|---------|------------------|
| **Neuromodulator Channel** | `nimcp_bio_channel_type_t` | DA/5-HT/NE/ACh signaling | Diffuse neuromodulator systems |
| **Bio Promise** | `nimcp_bio_promise_t` | Async operation futures | Neural prediction signals |
| **Phase Sync** | `nimcp_phase_sync_t` | Kuramoto oscillator coupling | Brain rhythm synchronization |
| **Predictive Coder** | `nimcp_predictive_t` | Bayesian prediction engine | Predictive coding hierarchy |
| **Glial Wave** | `nimcp_glial_wave_t` | Calcium wave propagation | Astrocyte signaling network |
| **Bio Config** | `nimcp_bio_async_config_t` | System configuration | N/A |

#### 29.3 Neuromodulator Channel Types

```c
// Neuromodulator channel types (from nimcp_bio_async.h)
typedef enum {
    NIMCP_BIO_CHANNEL_DOPAMINE,       // Reward, motivation, motor control
    NIMCP_BIO_CHANNEL_SEROTONIN,      // Mood, emotion, sleep-wake
    NIMCP_BIO_CHANNEL_NOREPINEPHRINE, // Alertness, arousal, attention
    NIMCP_BIO_CHANNEL_ACETYLCHOLINE   // Learning, memory, attention
} nimcp_bio_channel_type_t;

// Oscillation bands for phase coupling
typedef enum {
    NIMCP_OSCILLATION_DELTA = 0,  // 0.5-4 Hz: Deep sleep, unconscious
    NIMCP_OSCILLATION_THETA,       // 4-8 Hz: Memory encoding, navigation
    NIMCP_OSCILLATION_ALPHA,       // 8-13 Hz: Relaxed wakefulness, inhibition
    NIMCP_OSCILLATION_BETA,        // 13-30 Hz: Active thinking, motor
    NIMCP_OSCILLATION_GAMMA        // 30-100 Hz: Binding, attention, consciousness
} nimcp_oscillation_band_t;

// Predictive coding state
typedef enum {
    NIMCP_PREDICTIVE_IDLE,           // Waiting for input
    NIMCP_PREDICTIVE_PREDICTING,     // Generating prediction
    NIMCP_PREDICTIVE_ERROR_COMPUTED, // Prediction error ready
    NIMCP_PREDICTIVE_UPDATING        // Updating beliefs
} nimcp_predictive_state_t;

// Glial wave state
typedef enum {
    NIMCP_GLIAL_WAVE_DORMANT,     // No active wave
    NIMCP_GLIAL_WAVE_INITIATING,  // Wave starting (IP3 release)
    NIMCP_GLIAL_WAVE_PROPAGATING, // Wave spreading (20 μm/s)
    NIMCP_GLIAL_WAVE_DECAYING     // Wave amplitude decreasing
} nimcp_glial_wave_state_t;
```

#### 29.4 Bio Promise API (Async Operations)

```c
// Bio promise for async operations (from nimcp_bio_async.h)
typedef struct {
    uint64_t id;                      // Unique promise ID
    nimcp_bio_channel_type_t channel; // Associated neuromodulator
    nimcp_bio_promise_state_t state;  // PENDING/FULFILLED/REJECTED
    void* result;                     // Result data
    size_t result_size;               // Result size
    void (*on_fulfill)(void*);        // Success callback
    void (*on_reject)(void*, int);    // Error callback
    void* user_data;                  // Callback context
    uint64_t deadline_ms;             // Timeout deadline
} nimcp_bio_promise_t;

// Bio promise API
nimcp_bio_promise_t* nimcp_bio_promise_create(nimcp_bio_channel_type_t channel);
nimcp_error_t nimcp_bio_promise_then(nimcp_bio_promise_t* promise,
                                      void (*on_fulfill)(void*),
                                      void (*on_reject)(void*, int),
                                      void* user_data);
nimcp_error_t nimcp_bio_promise_fulfill(nimcp_bio_promise_t* promise,
                                         void* result, size_t size);
nimcp_error_t nimcp_bio_promise_reject(nimcp_bio_promise_t* promise, int error);
nimcp_error_t nimcp_bio_promise_await(nimcp_bio_promise_t* promise,
                                       void** result, uint32_t timeout_ms);
void nimcp_bio_promise_destroy(nimcp_bio_promise_t* promise);
```

#### 29.5 Phase Synchronization (Kuramoto Oscillators)

```c
// Phase synchronization system (from nimcp_bio_async.h)
typedef struct {
    uint32_t num_oscillators;           // Number of coupled oscillators
    float* phases;                      // Phase array [0, 2π]
    float* natural_frequencies;         // ω_i natural frequencies
    float coupling_strength;            // K: Global coupling constant
    float** coupling_matrix;            // K_ij: Pairwise coupling
    nimcp_oscillation_band_t band;      // Target frequency band
    float coherence;                    // Order parameter r (0-1)
    float mean_phase;                   // Mean phase ψ
    uint64_t update_count;              // Number of updates
} nimcp_phase_sync_t;

// Phase sync API
nimcp_phase_sync_t* nimcp_phase_sync_create(uint32_t num_oscillators,
                                             nimcp_oscillation_band_t band);
nimcp_error_t nimcp_phase_sync_set_coupling(nimcp_phase_sync_t* sync,
                                             float global_coupling);
nimcp_error_t nimcp_phase_sync_update(nimcp_phase_sync_t* sync, float dt);
float nimcp_phase_sync_get_coherence(const nimcp_phase_sync_t* sync);
float nimcp_phase_sync_get_mean_phase(const nimcp_phase_sync_t* sync);
nimcp_error_t nimcp_phase_sync_inject_phase(nimcp_phase_sync_t* sync,
                                             uint32_t oscillator_id,
                                             float target_phase);
void nimcp_phase_sync_destroy(nimcp_phase_sync_t* sync);

// Kuramoto model update: dθ_i/dt = ω_i + (K/N) × Σ sin(θ_j - θ_i)
// Order parameter: r × e^(iψ) = (1/N) × Σ e^(iθ_j)
```

#### 29.6 Predictive Coding Engine (Bayesian)

```c
// Predictive coding engine (from nimcp_bio_async.h)
typedef struct {
    nimcp_predictive_state_t state;     // Current processing state
    float* predictions;                  // P(x|context) predictions
    float* actuals;                      // Observed values
    float* prediction_errors;            // error = actual - predicted
    float* precisions;                   // π = 1/σ² precision weights
    uint32_t dimension;                  // State dimension
    float learning_rate;                 // Belief update rate
    float precision_decay;               // Precision temporal decay
    uint64_t prediction_count;           // Total predictions made
    float cumulative_error;              // Sum of squared errors
} nimcp_predictive_t;

// Predictive coding API
nimcp_predictive_t* nimcp_predictive_create(uint32_t dimension);
nimcp_error_t nimcp_predictive_set_context(nimcp_predictive_t* pred,
                                            const float* context, uint32_t len);
nimcp_error_t nimcp_predictive_generate(nimcp_predictive_t* pred);
nimcp_error_t nimcp_predictive_compute_error(nimcp_predictive_t* pred,
                                              const float* actual);
nimcp_error_t nimcp_predictive_update_beliefs(nimcp_predictive_t* pred);
float nimcp_predictive_get_surprise(const nimcp_predictive_t* pred);
void nimcp_predictive_destroy(nimcp_predictive_t* pred);

// Bayesian update: P(x|y) ∝ P(y|x) × P(x)
// Precision-weighted error: Δbelief = π × error × learning_rate
```

#### 29.7 Glial Wave Propagation

```c
// Glial wave system (from nimcp_bio_async.h)
typedef struct {
    nimcp_glial_wave_state_t state;      // Wave propagation state
    float* calcium_concentrations;        // [Ca²⁺] per astrocyte
    uint32_t num_astrocytes;              // Grid size
    float propagation_speed;              // ~20 μm/s
    float wave_amplitude;                 // Current amplitude
    float decay_rate;                     // Amplitude decay constant
    uint32_t origin_x, origin_y;          // Wave origin point
    float radius;                         // Current wave radius
    uint64_t start_time_ms;               // Wave initiation time
} nimcp_glial_wave_t;

// Glial wave API
nimcp_glial_wave_t* nimcp_glial_wave_create(uint32_t grid_width,
                                             uint32_t grid_height);
nimcp_error_t nimcp_glial_wave_initiate(nimcp_glial_wave_t* wave,
                                         uint32_t origin_x, uint32_t origin_y,
                                         float amplitude);
nimcp_error_t nimcp_glial_wave_propagate(nimcp_glial_wave_t* wave, float dt);
float nimcp_glial_wave_get_calcium(const nimcp_glial_wave_t* wave,
                                    uint32_t x, uint32_t y);
bool nimcp_glial_wave_is_active(const nimcp_glial_wave_t* wave);
void nimcp_glial_wave_destroy(nimcp_glial_wave_t* wave);
```

#### 29.8 Bio-Router Message System (nimcp_bio_router.h)

| Component | Type/Struct | Purpose |
|-----------|-------------|---------|
| **Bio Router** | `bio_router_t` | Central message routing hub |
| **Module Context** | `bio_module_context_t` | Per-module registration state |
| **Message Handler** | `bio_message_handler_t` | Message processing callback |
| **Module Config** | `bio_module_config_t` | Module registration config |
| **Inbox/Outbox** | Internal queues | Message buffering |
| **Brain KG Sync** | `bio_router_set_brain_kg()` | Knowledge graph integration |

```c
// Bio router types (from nimcp_bio_router.h)
typedef struct bio_router bio_router_t;
typedef struct bio_module_context bio_module_context_t;

// Message handler callback
typedef nimcp_error_t (*bio_message_handler_t)(
    bio_module_context_t* ctx,
    const char* source_module,
    const void* message,
    size_t message_size,
    void* user_data
);

// Module configuration
typedef struct {
    const char* module_name;              // Unique module identifier
    bio_message_handler_t handler;        // Message handler function
    void* user_data;                      // Handler context
    uint32_t inbox_capacity;              // Inbox queue size
    uint32_t outbox_capacity;             // Outbox queue size
    bool enable_predictive_protocol;      // Use predictive messaging
    nimcp_bio_channel_type_t channel;     // Associated neuromodulator
} bio_module_config_t;

// Bio router API
bio_router_t* bio_router_create(const char* router_name);
nimcp_error_t bio_router_register_module(bio_router_t* router,
                                          const bio_module_config_t* config,
                                          bio_module_context_t** out_ctx);
nimcp_error_t bio_router_unregister_module(bio_router_t* router,
                                            const char* module_name);
nimcp_error_t bio_router_send(bio_router_t* router,
                               const char* source_module,
                               const char* dest_module,
                               const void* message, size_t size);
nimcp_error_t bio_router_broadcast(bio_router_t* router,
                                    const char* source_module,
                                    const void* message, size_t size);
nimcp_error_t bio_router_process_pending(bio_router_t* router);
nimcp_error_t bio_router_set_brain_kg(bio_router_t* router,
                                       nimcp_brain_kg_t* kg);
nimcp_error_t bio_router_sync_to_kg(bio_router_t* router);
nimcp_error_t bio_router_set_glial_coordinator(bio_router_t* router,
                                                nimcp_glial_wave_t* glial);
void bio_router_destroy(bio_router_t* router);
```

#### 29.9 Wiring Diagram System (nimcp_wiring_diagram.h)

| Component | Type/Struct | Purpose |
|-----------|-------------|---------|
| **Wiring Diagram** | `wiring_diagram_t` | Runtime module configuration |
| **Module Entry** | `wiring_module_entry_t` | Individual module spec |
| **Hardware Profile** | `wiring_hw_flags_t` | Hardware capability flags |
| **Subsystem** | `wiring_subsystem_t` | Module group organization |
| **Validation** | `wiring_validation_result_t` | Dependency/config validation |

```c
// Hardware capability flags (from nimcp_wiring_diagram.h)
typedef enum {
    WIRING_HW_NONE       = 0,
    WIRING_HW_CUDA       = (1 << 0),   // NVIDIA CUDA
    WIRING_HW_ROCM       = (1 << 1),   // AMD ROCm
    WIRING_HW_LOIHI      = (1 << 2),   // Intel Loihi neuromorphic
    WIRING_HW_SPINNAKER  = (1 << 3),   // SpiNNaker neuromorphic
    WIRING_HW_TRUENORTH  = (1 << 4),   // IBM TrueNorth
    WIRING_HW_FPGA       = (1 << 5),   // Generic FPGA
    WIRING_HW_NPU        = (1 << 6),   // Neural Processing Unit
    WIRING_HW_DSP        = (1 << 7)    // Digital Signal Processor
} wiring_hw_flags_t;

// Subsystem categories
typedef enum {
    WIRING_SUBSYSTEM_CORE = 0,
    WIRING_SUBSYSTEM_ETHICS,
    WIRING_SUBSYSTEM_PERCEPTION,
    WIRING_SUBSYSTEM_COGNITION,
    WIRING_SUBSYSTEM_MEMORY,
    WIRING_SUBSYSTEM_EMOTION,
    WIRING_SUBSYSTEM_IMMUNE,
    WIRING_SUBSYSTEM_PLASTICITY,
    WIRING_SUBSYSTEM_RECURSIVE,
    WIRING_SUBSYSTEM_SOCIAL,
    WIRING_SUBSYSTEM_COUNT
} wiring_subsystem_t;

// Module entry specification
typedef struct {
    char name[64];                   // Module identifier
    char so_path[256];               // Shared library path
    wiring_subsystem_t subsystem;    // Subsystem category
    wiring_hw_flags_t hw_required;   // Required hardware
    wiring_hw_flags_t hw_optional;   // Optional hardware acceleration
    char dependencies[8][64];        // Required modules
    uint32_t num_dependencies;       // Dependency count
    bool enabled;                    // Runtime enable flag
    uint32_t priority;               // Load order priority
} wiring_module_entry_t;

// Wiring diagram API
wiring_diagram_t* wiring_diagram_create(void);
nimcp_error_t wiring_diagram_load_jsonl(wiring_diagram_t* diagram,
                                         const char* filepath);
nimcp_error_t wiring_diagram_load_for_profile(wiring_diagram_t* diagram,
                                               wiring_hw_flags_t hw_profile);
nimcp_error_t wiring_diagram_add_module(wiring_diagram_t* diagram,
                                         const wiring_module_entry_t* entry);
nimcp_error_t wiring_diagram_remove_module(wiring_diagram_t* diagram,
                                            const char* name);
nimcp_error_t wiring_diagram_validate(const wiring_diagram_t* diagram,
                                       wiring_validation_result_t* result);
nimcp_error_t wiring_diagram_resolve_dependencies(wiring_diagram_t* diagram);
nimcp_error_t wiring_diagram_sync_to_brain_kg(wiring_diagram_t* diagram,
                                               nimcp_brain_kg_t* kg);
nimcp_error_t wiring_diagram_sync_from_brain_kg(wiring_diagram_t* diagram,
                                                 nimcp_brain_kg_t* kg);
wiring_module_entry_t* wiring_diagram_get_module(wiring_diagram_t* diagram,
                                                  const char* name);
uint32_t wiring_diagram_get_load_order(const wiring_diagram_t* diagram,
                                        const char** ordered_names,
                                        uint32_t max_count);
void wiring_diagram_destroy(wiring_diagram_t* diagram);
```

#### 29.10 Bio-Async Message Types

```c
// Standard bio-async message types for inter-module communication
typedef enum {
    // Neuromodulator signals
    BIO_MSG_DOPAMINE_BURST,           // Reward signal
    BIO_MSG_DOPAMINE_DIP,             // Negative prediction error
    BIO_MSG_SEROTONIN_RELEASE,        // Mood/wellbeing signal
    BIO_MSG_NOREPINEPHRINE_SURGE,     // Alertness signal
    BIO_MSG_ACETYLCHOLINE_RELEASE,    // Attention/learning signal

    // Phase coupling
    BIO_MSG_PHASE_SYNC_REQUEST,       // Request phase alignment
    BIO_MSG_PHASE_SYNC_ACK,           // Phase alignment achieved
    BIO_MSG_COHERENCE_UPDATE,         // Coherence level changed

    // Predictive coding
    BIO_MSG_PREDICTION_REQUEST,       // Request prediction
    BIO_MSG_PREDICTION_RESULT,        // Prediction generated
    BIO_MSG_PREDICTION_ERROR,         // Error signal
    BIO_MSG_PRECISION_UPDATE,         // Precision weight changed

    // Glial signaling
    BIO_MSG_GLIAL_WAVE_INITIATED,     // Wave started
    BIO_MSG_GLIAL_WAVE_ARRIVED,       // Wave reached location
    BIO_MSG_CALCIUM_SPIKE,            // Local calcium event

    // Module coordination
    BIO_MSG_MODULE_READY,             // Module initialization complete
    BIO_MSG_MODULE_BUSY,              // Module processing
    BIO_MSG_MODULE_ERROR,             // Module error occurred
    BIO_MSG_SHUTDOWN_REQUEST,         // Graceful shutdown
    BIO_MSG_HEARTBEAT                 // Module alive signal
} bio_message_type_t;

// Generic bio message structure
typedef struct {
    bio_message_type_t type;          // Message type
    uint64_t timestamp_ms;            // Message creation time
    uint64_t sequence_number;         // Monotonic sequence
    char source_module[64];           // Sender module name
    uint32_t payload_size;            // Payload size in bytes
    uint8_t payload[];                // Variable-length payload
} bio_message_t;
```

#### 29.11 Brain Factory Async Integration

**File**: `src/core/brain/factory/init/nimcp_brain_init_async.c`

```c
nimcp_error_t nimcp_brain_init_async(nimcp_brain_t* brain,
                                      const nimcp_brain_config_t* config) {
    NIMCP_LOG_INFO("Initializing bio-async coordination layer");

    // Create bio-async configuration
    nimcp_bio_async_config_t async_config = {
        .num_channels = 4,  // DA, 5-HT, NE, ACh
        .enable_phase_coupling = true,
        .enable_predictive_coding = true,
        .enable_glial_waves = true,
        .default_coupling_strength = 0.5f,
        .prediction_dimension = 128,
        .glial_grid_size = 64
    };

    // Initialize bio-async system
    brain->bio_async = nimcp_bio_async_create(&async_config);
    if (!brain->bio_async) {
        NIMCP_LOG_ERROR("Failed to create bio-async system");
        return NIMCP_ERROR_MEMORY;
    }

    // Create bio router
    brain->bio_router = bio_router_create("brain_router");
    if (!brain->bio_router) {
        NIMCP_LOG_ERROR("Failed to create bio router");
        return NIMCP_ERROR_MEMORY;
    }

    // Connect router to brain knowledge graph
    if (brain->knowledge_graph) {
        bio_router_set_brain_kg(brain->bio_router, brain->knowledge_graph);
    }

    // Create wiring diagram
    brain->wiring_diagram = wiring_diagram_create();
    if (!brain->wiring_diagram) {
        NIMCP_LOG_ERROR("Failed to create wiring diagram");
        return NIMCP_ERROR_MEMORY;
    }

    // Load hardware-specific configuration
    wiring_hw_flags_t hw_profile = WIRING_HW_NONE;
    if (config->enable_cuda) hw_profile |= WIRING_HW_CUDA;
    if (config->enable_neuromorphic) hw_profile |= WIRING_HW_LOIHI | WIRING_HW_SPINNAKER;

    if (config->wiring_config_path) {
        wiring_diagram_load_jsonl(brain->wiring_diagram, config->wiring_config_path);
    } else {
        wiring_diagram_load_for_profile(brain->wiring_diagram, hw_profile);
    }

    // Validate and resolve dependencies
    wiring_validation_result_t validation;
    wiring_diagram_validate(brain->wiring_diagram, &validation);
    if (!validation.is_valid) {
        NIMCP_LOG_WARNING("Wiring diagram validation issues: %s", validation.message);
    }
    wiring_diagram_resolve_dependencies(brain->wiring_diagram);

    // Sync wiring diagram to brain KG
    if (brain->knowledge_graph) {
        wiring_diagram_sync_to_brain_kg(brain->wiring_diagram, brain->knowledge_graph);
    }

    // Initialize phase synchronization for each oscillation band
    for (int band = NIMCP_OSCILLATION_DELTA; band <= NIMCP_OSCILLATION_GAMMA; band++) {
        brain->phase_syncs[band] = nimcp_phase_sync_create(
            config->num_neurons / 100,  // Subsample for efficiency
            (nimcp_oscillation_band_t)band
        );
    }

    // Initialize predictive coding engine
    brain->predictive = nimcp_predictive_create(async_config.prediction_dimension);

    // Initialize glial wave system
    brain->glial_wave = nimcp_glial_wave_create(
        async_config.glial_grid_size,
        async_config.glial_grid_size
    );
    bio_router_set_glial_coordinator(brain->bio_router, brain->glial_wave);

    // Connect to immune system for inflammation-based neuromodulator effects
    if (brain->immune) {
        nimcp_bio_async_immune_bridge_init(&brain->bio_async_immune_bridge,
                                            brain->bio_async, brain->immune);
    }

    NIMCP_LOG_INFO("Bio-async layer initialized: router=%p, wiring=%p, "
                   "phase_syncs=5, predictive=%p, glial=%p",
                   brain->bio_router, brain->wiring_diagram,
                   brain->predictive, brain->glial_wave);

    return NIMCP_OK;
}
```

#### 29.12 Async Module Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/async/test_bio_promise.cpp` | 20 |
| `test/unit/async/test_bio_promise_chaining.cpp` | 15 |
| `test/unit/async/test_bio_promise_timeout.cpp` | 10 |
| `test/unit/async/test_phase_sync_kuramoto.cpp` | 25 |
| `test/unit/async/test_phase_sync_coherence.cpp` | 15 |
| `test/unit/async/test_phase_sync_injection.cpp` | 10 |
| `test/unit/async/test_predictive_coding.cpp` | 20 |
| `test/unit/async/test_predictive_error.cpp` | 15 |
| `test/unit/async/test_predictive_precision.cpp` | 10 |
| `test/unit/async/test_glial_wave_propagation.cpp` | 20 |
| `test/unit/async/test_glial_wave_calcium.cpp` | 15 |
| `test/unit/async/test_bio_router_registration.cpp` | 20 |
| `test/unit/async/test_bio_router_messaging.cpp` | 25 |
| `test/unit/async/test_bio_router_broadcast.cpp` | 15 |
| `test/unit/async/test_bio_router_kg_integration.cpp` | 15 |
| `test/unit/async/test_wiring_diagram_load.cpp` | 15 |
| `test/unit/async/test_wiring_diagram_validation.cpp` | 20 |
| `test/unit/async/test_wiring_diagram_dependencies.cpp` | 15 |
| `test/unit/async/test_wiring_diagram_kg_sync.cpp` | 15 |
| `test/integration/async/test_bio_async_immune_integration.cpp` | 10 |
| `test/integration/async/test_bio_async_cognitive_integration.cpp` | 15 |
| `test/integration/async/test_wiring_brain_factory_integration.cpp` | 10 |
| `test/integration/async/test_phase_sync_attention_integration.cpp` | 10 |
| **Total** | **350** |

### 30. Utils Module Integration

**Priority**: Critical (Core Infrastructure)
**Dependencies**: All modules depend on utils for foundational operations
**Est. LOC**: 70,000 | **Tests**: 500

#### 30.1 Utils Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         UTILS FOUNDATIONAL LAYER                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                    CORE DATA STRUCTURES                               │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │  │
│  │  │ Tensor   │ │Hash Table│ │ B-Tree   │ │Ring Buff │ │ Graph    │  │  │
│  │  │ N-dim    │ │ O(1)     │ │ O(log n) │ │Lock-free │ │ Adjacency│  │  │
│  │  │ Calculus │ │ lookup   │ │ ordered  │ │ SPSC     │ │ Lists    │  │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘  │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐              │  │
│  │  │ Vector   │ │ Queue    │ │ Min Heap │ │ DArray   │              │  │
│  │  │ Dynamic  │ │ FIFO     │ │ Priority │ │ Dynamic  │              │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘              │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    MEMORY MANAGEMENT                                  │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Memory Pool  │ │ Buffer Pool  │ │ Unified Mem  │                 │  │
│  │  │ O(1) alloc   │ │ Pre-alloc    │ │ CPU+GPU      │                 │  │
│  │  │ Free-list    │ │ Reusable     │ │ Migration    │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ CoW Manager  │ │ Page CoW     │ │ Guard Pages  │                 │  │
│  │  │ Copy-on-     │ │ Lazy copy    │ │ Corruption   │                 │  │
│  │  │ Write        │ │ Efficiency   │ │ Detection    │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    THREADING & CONCURRENCY                            │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Thread Pool  │ │ Mutex/RWLock │ │ Atomics      │                 │  │
│  │  │ Work queue   │ │ NIMCP layer  │ │ Lock-free    │                 │  │
│  │  │ Task submit  │ │ Platform abs │ │ Primitives   │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Barrier      │ │ Semaphore    │ │ Deadlock Det │                 │  │
│  │  │ Sync point   │ │ Counting     │ │ Cycle detect │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    FAULT TOLERANCE (24 headers)                       │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Checkpoint   │ │ Recovery     │ │ Graceful     │                 │  │
│  │  │ Atomic save  │ │ Auto-restore │ │ Degradation  │                 │  │
│  │  │ Compression  │ │ Brain state  │ │ 5 tiers      │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Health Mon   │ │ Byzantine FT │ │ Chaos Eng    │                 │  │
│  │  │ Heartbeats   │ │ Consensus    │ │ Fault inject │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    TERNARY LOGIC (9 headers)                          │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Trit Types   │ │ Kleene Logic │ │ Packed Store │                 │  │
│  │  │ -1/0/+1      │ │ 3-valued ops │ │ 2-bit/Base243│                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Trit Vector  │ │ Trit Matrix  │ │ Trit Tensor  │                 │  │
│  │  │ SIMD ops     │ │ Matrix mul   │ │ N-dim        │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │                    ALGORITHMS (6 headers)                             │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Monte Carlo  │ │ Graph Metrics│ │ Louvain      │                 │  │
│  │  │ MCTS, MCMC   │ │ Centrality   │ │ Community    │                 │  │
│  │  │ Importance   │ │ Degree/Close │ │ Detection    │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                 │  │
│  │  │ Modularity   │ │ Centrality   │ │ Sort         │                 │  │
│  │  │ Q metric     │ │ Betweenness  │ │ Quick/Merge  │                 │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘                 │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                │                                            │
│  ┌────────────────────────────┴─────────────────────────────────────────┐  │
│  │              SUPPORTING INFRASTRUCTURE                                │  │
│  │   Logging │ Config │ JSON │ Serialization │ Metrics │ Platform      │  │
│  │   Signal  │ Gabor  │ FFT  │ Hilbert       │ KD-Tree │ Quantum Walk  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 30.2 Utils Module Organization (33 Subdirectories)

| Subdirectory | Headers | Purpose |
|--------------|---------|---------|
| **algorithms** | 6 | Monte Carlo, graph metrics, Louvain, centrality, sort |
| **bridge** | 2 | Bridge base class, GPU bridge utilities |
| **cache** | 1 | Generic caching infrastructure |
| **code** | 4 | DWARF symbols, hot injection, recompiler, source cache |
| **config** | 7 | Configuration management, validation, dynamic config |
| **containers** | 9 | Hash table, B-tree, graph, queue, ring buffer, vector, heap |
| **dispatch** | 1 | Function dispatch infrastructure |
| **encoding** | 1 | Positional encoding for transformers |
| **error** | 1 | Error code definitions |
| **fault_tolerance** | 24 | Checkpoint, recovery, degradation, health monitoring |
| **gabor** | 1 | Gabor filters for visual processing |
| **geometry** | 1 | Hyperbolic geometry operations |
| **json** | 1 | JSON parsing and generation |
| **list** | 1 | Linked list implementation |
| **logging** | 1 | Async logging with rotation |
| **math** | 1 | Complex number mathematics |
| **memory** | 11 | Memory pools, CoW, unified memory, guards |
| **metrics** | 1 | Performance metrics collection |
| **numerical** | 1 | Numerical integration |
| **platform** | 11 | Platform abstraction (mutex, thread, time, tier) |
| **quantum** | 5 | Quantum walk, quantum Monte Carlo, quantum Shannon |
| **queue_manager** | 1 | Queue management infrastructure |
| **serialization** | 1 | Binary serialization |
| **signal** | 3 | Hilbert transform, signal filtering, signal handler |
| **spatial** | 1 | KD-tree spatial indexing |
| **spectral** | 1 | FFT implementation |
| **tensor** | 3 | N-dim tensors, SIMD ops, tensor operations |
| **tensor_networks** | 2 | MPS, SVD decomposition |
| **ternary** | 9 | Three-valued logic system |
| **thread** | 7 | Thread pool, atomics, barrier, semaphore, deadlock |
| **time** | 1 | Time utilities |
| **validation** | 2 | Input validation utilities |
| **TOTAL** | **122** | |

#### 30.3 Tensor Library (nimcp_tensor.h)

```c
// Tensor data types (from nimcp_tensor.h)
typedef enum {
    NIMCP_DTYPE_F32 = 0,    // 32-bit float (default)
    NIMCP_DTYPE_F64 = 1,    // 64-bit double
    NIMCP_DTYPE_F16 = 2,    // 16-bit half precision
    NIMCP_DTYPE_BF16 = 3,   // Brain float 16
    NIMCP_DTYPE_I32 = 4,    // 32-bit signed integer
    NIMCP_DTYPE_I64 = 5,    // 64-bit signed integer
    NIMCP_DTYPE_I8 = 6,     // 8-bit signed integer
    NIMCP_DTYPE_U8 = 7,     // 8-bit unsigned integer
    NIMCP_DTYPE_BOOL = 8,   // Boolean
    NIMCP_DTYPE_C64 = 9,    // Complex float (2x32-bit)
    NIMCP_DTYPE_C128 = 10   // Complex double (2x64-bit)
} nimcp_dtype_t;

// Tensor structure (max 8 dimensions)
#define NIMCP_TENSOR_MAX_RANK 8

// Core tensor API
nimcp_tensor_t* nimcp_tensor_create(const size_t* dims, size_t rank, nimcp_dtype_t dtype);
void nimcp_tensor_destroy(nimcp_tensor_t* tensor);
nimcp_tensor_t* nimcp_tensor_clone(const nimcp_tensor_t* tensor);
nimcp_tensor_t* nimcp_tensor_view(nimcp_tensor_t* tensor, const size_t* start, const size_t* end);

// Element access
float nimcp_tensor_get_f32(const nimcp_tensor_t* tensor, const size_t* indices);
void nimcp_tensor_set_f32(nimcp_tensor_t* tensor, const size_t* indices, float value);

// Operations
nimcp_tensor_t* nimcp_tensor_add(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_mul(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_matmul(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_transpose(const nimcp_tensor_t* tensor, const size_t* perm);

// Reductions
nimcp_tensor_t* nimcp_tensor_sum(const nimcp_tensor_t* tensor, int axis);
nimcp_tensor_t* nimcp_tensor_mean(const nimcp_tensor_t* tensor, int axis);
nimcp_tensor_t* nimcp_tensor_max(const nimcp_tensor_t* tensor, int axis);

// Calculus
nimcp_tensor_t* nimcp_tensor_gradient(const nimcp_tensor_t* tensor);
nimcp_tensor_t* nimcp_tensor_jacobian(const nimcp_tensor_t* tensor);
nimcp_tensor_t* nimcp_tensor_hessian(const nimcp_tensor_t* tensor);

// Einstein summation
nimcp_tensor_t* nimcp_tensor_einsum(const char* equation,
                                     const nimcp_tensor_t** tensors, size_t num_tensors);
```

#### 30.4 Memory Pool System (nimcp_memory_pool.h)

```c
// Memory pool configuration (from nimcp_memory_pool.h)
typedef struct {
    size_t block_size;          // Size of each memory block
    size_t num_blocks;          // Number of blocks in pool
    size_t alignment;           // Memory alignment (power of 2)
    bool enable_tracking;       // Track allocation statistics
    bool enable_guard_pages;    // Enable memory corruption detection
} memory_pool_config_t;

// Memory pool statistics
typedef struct {
    size_t total_blocks;        // Total blocks in pool
    size_t allocated_blocks;    // Currently allocated blocks
    size_t free_blocks;         // Currently free blocks
    size_t peak_allocated;      // Peak allocation count
    size_t total_allocations;   // Total allocations made
    size_t failed_allocations;  // Failed allocation attempts
    size_t pool_size_bytes;     // Total pool size in bytes
} memory_pool_stats_t;

// Memory pool API - O(1) allocation
memory_pool_t memory_pool_create(const memory_pool_config_t* config);
void memory_pool_destroy(memory_pool_t pool);
void* memory_pool_acquire(memory_pool_t pool);
void memory_pool_release(memory_pool_t pool, void* block);
void memory_pool_stats(memory_pool_t pool, memory_pool_stats_t* stats);
```

#### 30.5 Thread Pool System (nimcp_thread_pool.h)

```c
// Thread pool API (from nimcp_thread_pool.h)
#define NIMCP_POOL_MAX_THREADS 64
#define NIMCP_POOL_MAX_QUEUE 1024

// Task function signature
typedef void (*nimcp_task_fn)(void* arg);

// Thread pool API
nimcp_thread_pool_t* nimcp_pool_create(size_t num_threads);
void nimcp_pool_destroy(nimcp_thread_pool_t* pool);
nimcp_result_t nimcp_pool_submit(nimcp_thread_pool_t* pool, nimcp_task_fn task, void* arg);
nimcp_result_t nimcp_pool_wait(nimcp_thread_pool_t* pool);
size_t nimcp_pool_pending(nimcp_thread_pool_t* pool);
size_t nimcp_pool_active(nimcp_thread_pool_t* pool);
```

#### 30.6 Hash Table (nimcp_hash_table.h)

```c
// Hash key types (from nimcp_hash_table.h)
typedef enum {
    HASH_KEY_STRING,  // Null-terminated string keys
    HASH_KEY_UINT32,  // 32-bit unsigned integer keys
    HASH_KEY_UINT64,  // 64-bit unsigned integer keys
    HASH_KEY_CUSTOM   // Custom key type with user-provided hash/compare
} hash_key_type_t;

// Hash algorithms
typedef enum {
    HASH_ALG_FNV1A,    // Fast, good distribution for strings
    HASH_ALG_DJB2,     // Simple, fast for strings
    HASH_ALG_MURMUR3,  // Excellent distribution for integers
    HASH_ALG_CUSTOM    // User-provided hash function
} hash_algorithm_t;

// Hash table configuration
typedef struct {
    size_t initial_buckets;       // Initial bucket count
    hash_key_type_t key_type;     // Key type
    hash_algorithm_t hash_algorithm;
    bool case_insensitive;        // For string keys
    float load_factor_threshold;  // When to resize (default: 0.75)
} hash_table_config_t;

// Hash table API - O(1) average
hash_table_t* hash_table_create(const hash_table_config_t* config);
void hash_table_destroy(hash_table_t* table);
bool hash_table_insert_string(hash_table_t* table, const char* key, void* value, size_t size);
void* hash_table_lookup_string(hash_table_t* table, const char* key);
bool hash_table_remove_string(hash_table_t* table, const char* key);
size_t hash_table_size(const hash_table_t* table);
```

#### 30.7 Ternary Logic System (nimcp_ternary.h)

```c
// Ternary (trit) values (from nimcp_ternary_types.h)
typedef enum {
    TRIT_NEGATIVE = -1,  // Inhibitory, LTD, reject, FORBID
    TRIT_UNKNOWN  =  0,  // Silent, neutral, abstain, NEUTRAL
    TRIT_POSITIVE = +1   // Excitatory, LTP, accept, ALLOW
} trit_t;

// Packing modes for memory efficiency
typedef enum {
    TERNARY_PACK_NONE,      // 1 trit/byte (fast access)
    TERNARY_PACK_2BIT,      // 4 trits/byte (balanced)
    TERNARY_PACK_BASE243    // 5 trits/byte (max compression)
} ternary_pack_mode_t;

// Ternary vector API
trit_vector_t* trit_vector_create(size_t length, ternary_pack_mode_t mode);
void trit_vector_destroy(trit_vector_t* vec);
trit_t trit_vector_get(const trit_vector_t* vec, size_t index);
void trit_vector_set(trit_vector_t* vec, size_t index, trit_t value);

// Ternary logic operations (Kleene three-valued logic)
trit_t trit_and(trit_t a, trit_t b);   // AND: -1 absorbs, +1 if both +1
trit_t trit_or(trit_t a, trit_t b);    // OR: +1 absorbs, -1 if both -1
trit_t trit_not(trit_t a);             // NOT: -1 <-> +1, 0 -> 0

// Conversion from floats (quantization)
trit_vector_t* trit_vector_from_floats(const float* values, size_t count,
                                        float threshold, ternary_pack_mode_t mode);

// Module integrations
// SNN: Ternary synaptic weights (20x memory savings)
// Ethics: FORBID/NEUTRAL/ALLOW decisions
// Swarm: DISAGREE/ABSTAIN/AGREE voting
// Plasticity: LTD/STABLE/LTP updates
```

#### 30.8 Fault Tolerance - Checkpoint System (nimcp_checkpoint.h)

```c
// Checkpoint header format (from nimcp_checkpoint.h)
typedef struct {
    char magic[9];          // "NIMCP-CKP"
    uint8_t version_major;  // Major version
    uint8_t version_minor;  // Minor version
    uint32_t flags;         // Compression, incremental, encrypted
    uint64_t timestamp;     // Unix timestamp
    uint32_t crc32;         // Checksum
    uint32_t data_size;     // Data section size
} checkpoint_header_t;

// Checkpoint flags
#define CHECKPOINT_FLAG_COMPRESSED   0x00000001
#define CHECKPOINT_FLAG_INCREMENTAL  0x00000002
#define CHECKPOINT_FLAG_ENCRYPTED    0x00000004
#define CHECKPOINT_FLAG_SUBSYSTEMS   0x00000008

// Checkpoint API
nimcp_result_t checkpoint_save(brain_t brain, const char* path);
nimcp_result_t checkpoint_save_ex(brain_t brain, const char* path,
                                   const checkpoint_options_t* options);
nimcp_result_t checkpoint_load(brain_t* brain, const char* path);
bool checkpoint_validate(const char* path);
nimcp_result_t recovery_auto_restore(brain_t* brain, const char* checkpoint_dir);
```

#### 30.9 Fault Tolerance - Graceful Degradation (nimcp_graceful_degradation.h)

```c
// Degradation tiers (from nimcp_graceful_degradation.h)
typedef enum {
    GD_TIER_FULL = 0,       // Full functionality
    GD_TIER_STANDARD,       // Non-essential disabled
    GD_TIER_REDUCED,        // Quality reduced
    GD_TIER_MINIMAL,        // Core functions only
    GD_TIER_EMERGENCY       // Survival mode
} gd_tier_t;

// Feature priority levels
typedef enum {
    GD_PRIORITY_CRITICAL = 0,   // Must always run
    GD_PRIORITY_HIGH,           // Run except emergency
    GD_PRIORITY_MEDIUM,         // Run in standard+
    GD_PRIORITY_LOW,            // Run only in full
    GD_PRIORITY_OPTIONAL        // Luxury feature
} gd_priority_t;

// Resource types to monitor
typedef enum {
    GD_RESOURCE_CPU = 0,        // CPU utilization
    GD_RESOURCE_MEMORY,         // Memory usage
    GD_RESOURCE_GPU,            // GPU utilization
    GD_RESOURCE_NETWORK,        // Network bandwidth
    GD_RESOURCE_POWER,          // Power consumption
    GD_RESOURCE_LATENCY,        // Response latency
    GD_RESOURCE_THROUGHPUT      // Processing throughput
} gd_resource_t;

// Graceful degradation API
gd_manager_t* gd_manager_create(const gd_config_t* config);
void gd_manager_destroy(gd_manager_t* manager);
gd_tier_t gd_manager_get_current_tier(const gd_manager_t* manager);
void gd_manager_update_resource(gd_manager_t* manager, gd_resource_t resource, float value);
bool gd_manager_is_feature_enabled(const gd_manager_t* manager, const char* feature_name);
```

#### 30.10 Monte Carlo Algorithms (nimcp_monte_carlo.h)

```c
// Monte Carlo sampling methods (from nimcp_monte_carlo.h)
typedef enum {
    MC_SAMPLE_UNIFORM,              // Simple uniform random sampling
    MC_SAMPLE_IMPORTANCE,           // Importance sampling with weights
    MC_SAMPLE_STRATIFIED,           // Stratified sampling
    MC_SAMPLE_METROPOLIS_HASTINGS   // MCMC
} mc_sampling_method_t;

// MCTS configuration
typedef struct {
    float exploration_constant;     // UCB1 exploration (default: sqrt(2))
    float discount_factor;          // Value backprop discount
    uint32_t max_iterations;        // MCTS iterations
    uint32_t max_depth;             // Tree depth limit
    mc_gpu_mode_t gpu_mode;         // GPU acceleration
} mcts_config_t;

// MCTS API
mcts_t* mcts_create(const mcts_config_t* config);
void mcts_destroy(mcts_t* mcts);
nimcp_mc_result_t mcts_search(mcts_t* mcts, const void* root_state,
                               mcts_callbacks_t* callbacks);
uint32_t mcts_get_best_action(const mcts_t* mcts);
float mcts_get_action_value(const mcts_t* mcts, uint32_t action);

// Monte Carlo integration
typedef struct {
    mc_sampling_method_t method;
    uint32_t num_samples;
    uint32_t burnin;                // For MCMC
    bool parallel;                  // Use thread pool
} mc_integration_config_t;

float mc_integrate(mc_target_fn target, const mc_integration_config_t* config, void* ctx);
```

#### 30.11 Logging System (nimcp_logging.h)

```c
// Log levels (from nimcp_logging.h)
typedef enum {
    NIMCP_LOG_LEVEL_TRACE = 0,
    NIMCP_LOG_LEVEL_DEBUG,
    NIMCP_LOG_LEVEL_INFO,
    NIMCP_LOG_LEVEL_WARN,
    NIMCP_LOG_LEVEL_ERROR,
    NIMCP_LOG_LEVEL_FATAL
} nimcp_log_level_t;

// Logging configuration
typedef struct {
    nimcp_log_level_t min_level;    // Minimum level to log
    bool async_mode;                 // Use async logging
    size_t ring_buffer_size;         // Async buffer size
    const char* log_file_path;       // File output path
    bool enable_console;             // Console output
    bool enable_colors;              // ANSI colors
    bool enable_json;                // JSON format
    size_t max_file_size;            // Rotation size
    uint32_t max_rotated_files;      // Keep N rotated files
} nimcp_log_config_t;

// Logging macros
#define NIMCP_LOG_TRACE(fmt, ...) nimcp_log(NIMCP_LOG_LEVEL_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NIMCP_LOG_DEBUG(fmt, ...) nimcp_log(NIMCP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NIMCP_LOG_INFO(fmt, ...)  nimcp_log(NIMCP_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NIMCP_LOG_WARN(fmt, ...)  nimcp_log(NIMCP_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NIMCP_LOG_ERROR(fmt, ...) nimcp_log(NIMCP_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NIMCP_LOG_FATAL(fmt, ...) nimcp_log(NIMCP_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Logging API
nimcp_result_t nimcp_log_init(const nimcp_log_config_t* config);
void nimcp_log_shutdown(void);
void nimcp_log(nimcp_log_level_t level, const char* file, int line, const char* fmt, ...);
void nimcp_log_set_level(nimcp_log_level_t level);
void nimcp_log_flush(void);
```

#### 30.12 Platform Abstraction Layer (nimcp_platform.h)

```c
// Platform tiers (from nimcp_platform_tier.h)
typedef enum {
    PLATFORM_TIER_FULL,         // All features enabled
    PLATFORM_TIER_MEDIUM,       // Most features
    PLATFORM_TIER_CONSTRAINED,  // Limited resources
    PLATFORM_TIER_MINIMAL       // Bare minimum
} platform_tier_t;

// Platform threading API (from nimcp_thread.h)
typedef struct nimcp_mutex nimcp_mutex_t;
typedef struct nimcp_cond nimcp_cond_t;
typedef struct nimcp_rwlock nimcp_rwlock_t;

// Mutex API
nimcp_mutex_t* nimcp_mutex_create(const mutex_attr_t* attr);
void nimcp_mutex_destroy(nimcp_mutex_t* mutex);
nimcp_result_t nimcp_mutex_lock(nimcp_mutex_t* mutex);
nimcp_result_t nimcp_mutex_unlock(nimcp_mutex_t* mutex);
nimcp_result_t nimcp_mutex_trylock(nimcp_mutex_t* mutex);

// RWLock API
nimcp_rwlock_t* nimcp_rwlock_create(void);
void nimcp_rwlock_destroy(nimcp_rwlock_t* rwlock);
nimcp_result_t nimcp_rwlock_rdlock(nimcp_rwlock_t* rwlock);
nimcp_result_t nimcp_rwlock_wrlock(nimcp_rwlock_t* rwlock);
nimcp_result_t nimcp_rwlock_unlock(nimcp_rwlock_t* rwlock);

// Condition variable API
nimcp_cond_t* nimcp_cond_create(void);
void nimcp_cond_destroy(nimcp_cond_t* cond);
nimcp_result_t nimcp_cond_wait(nimcp_cond_t* cond, nimcp_mutex_t* mutex);
nimcp_result_t nimcp_cond_signal(nimcp_cond_t* cond);
nimcp_result_t nimcp_cond_broadcast(nimcp_cond_t* cond);
```

#### 30.13 Brain Factory Utils Integration

**File**: `src/core/brain/factory/init/nimcp_brain_init_utils.c`

```c
nimcp_error_t nimcp_brain_init_utils(nimcp_brain_t* brain,
                                      const nimcp_brain_config_t* config) {
    NIMCP_LOG_INFO("Initializing utils infrastructure");

    // Initialize logging system
    nimcp_log_config_t log_config = {
        .min_level = config->log_level,
        .async_mode = true,
        .ring_buffer_size = NIMCP_LOG_DEFAULT_BUFFER_SIZE,
        .log_file_path = config->log_file,
        .enable_console = config->enable_console_log,
        .enable_colors = true,
        .max_file_size = NIMCP_LOG_DEFAULT_MAX_FILE_SIZE,
        .max_rotated_files = 5
    };
    nimcp_log_init(&log_config);

    // Create memory pools for brain components
    memory_pool_config_t pool_config = {
        .block_size = config->memory_block_size,
        .num_blocks = config->memory_pool_blocks,
        .alignment = 64,
        .enable_tracking = config->enable_memory_tracking,
        .enable_guard_pages = config->debug_mode
    };
    brain->memory_pool = memory_pool_create(&pool_config);
    if (!brain->memory_pool) {
        NIMCP_LOG_ERROR("Failed to create brain memory pool");
        return NIMCP_ERROR_MEMORY;
    }

    // Create thread pool for parallel operations
    size_t num_threads = config->num_worker_threads;
    if (num_threads == 0) {
        num_threads = nimcp_system_get_cpu_count();
    }
    brain->thread_pool = nimcp_pool_create(num_threads);
    if (!brain->thread_pool) {
        NIMCP_LOG_ERROR("Failed to create thread pool");
        return NIMCP_ERROR_MEMORY;
    }

    // Initialize graceful degradation manager
    gd_config_t gd_config = {
        .initial_tier = GD_TIER_FULL,
        .enable_auto_degradation = true,
        .hysteresis_percent = GD_HYSTERESIS_PERCENT
    };
    brain->degradation_manager = gd_manager_create(&gd_config);

    // Initialize checkpoint system
    checkpoint_config_t ckpt_config = {
        .checkpoint_dir = config->checkpoint_dir,
        .enable_compression = true,
        .enable_auto_checkpoint = config->enable_auto_checkpoint,
        .checkpoint_interval_ms = config->checkpoint_interval_ms
    };
    brain->checkpoint_manager = checkpoint_manager_create(&ckpt_config);

    // Initialize health monitor
    health_config_t health_config = {
        .heartbeat_interval_ms = 1000,
        .timeout_ms = 5000,
        .enable_self_healing = true
    };
    brain->health_monitor = health_monitor_create(&health_config);

    // Register with bio-async router
    if (brain->bio_router) {
        bio_module_config_t utils_module = {
            .module_name = "utils",
            .handler = utils_message_handler,
            .user_data = brain,
            .inbox_capacity = 256,
            .enable_predictive_protocol = false
        };
        bio_router_register_module(brain->bio_router, &utils_module, &brain->utils_ctx);
    }

    // Connect to immune system for resource monitoring
    if (brain->immune) {
        nimcp_utils_immune_bridge_init(&brain->utils_immune_bridge,
                                        brain->degradation_manager, brain->immune);
    }

    NIMCP_LOG_INFO("Utils infrastructure initialized: memory_pool=%p, thread_pool=%p, "
                   "degradation=%p, checkpoint=%p, health=%p",
                   brain->memory_pool, brain->thread_pool,
                   brain->degradation_manager, brain->checkpoint_manager,
                   brain->health_monitor);

    return NIMCP_OK;
}
```

#### 30.14 Utils Module Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/utils/tensor/test_tensor_create.cpp` | 20 |
| `test/unit/utils/tensor/test_tensor_ops.cpp` | 25 |
| `test/unit/utils/tensor/test_tensor_einsum.cpp` | 15 |
| `test/unit/utils/tensor/test_tensor_calculus.cpp` | 15 |
| `test/unit/utils/memory/test_memory_pool.cpp` | 20 |
| `test/unit/utils/memory/test_memory_cow.cpp` | 15 |
| `test/unit/utils/memory/test_unified_memory.cpp` | 10 |
| `test/unit/utils/containers/test_hash_table.cpp` | 25 |
| `test/unit/utils/containers/test_btree.cpp` | 15 |
| `test/unit/utils/containers/test_ring_buffer.cpp` | 15 |
| `test/unit/utils/containers/test_graph.cpp` | 15 |
| `test/unit/utils/thread/test_thread_pool.cpp` | 20 |
| `test/unit/utils/thread/test_mutex.cpp` | 15 |
| `test/unit/utils/thread/test_atomics.cpp` | 15 |
| `test/unit/utils/thread/test_deadlock_detector.cpp` | 10 |
| `test/unit/utils/fault_tolerance/test_checkpoint.cpp` | 25 |
| `test/unit/utils/fault_tolerance/test_recovery.cpp` | 20 |
| `test/unit/utils/fault_tolerance/test_graceful_degradation.cpp` | 20 |
| `test/unit/utils/fault_tolerance/test_health_monitor.cpp` | 15 |
| `test/unit/utils/fault_tolerance/test_byzantine_ft.cpp` | 10 |
| `test/unit/utils/ternary/test_trit_logic.cpp` | 15 |
| `test/unit/utils/ternary/test_trit_vector.cpp` | 15 |
| `test/unit/utils/ternary/test_trit_packing.cpp` | 15 |
| `test/unit/utils/algorithms/test_monte_carlo.cpp` | 20 |
| `test/unit/utils/algorithms/test_mcts.cpp` | 15 |
| `test/unit/utils/algorithms/test_graph_metrics.cpp` | 15 |
| `test/unit/utils/logging/test_logging.cpp` | 15 |
| `test/unit/utils/logging/test_async_logging.cpp` | 10 |
| `test/unit/utils/config/test_config.cpp` | 10 |
| `test/unit/utils/json/test_json.cpp` | 10 |
| `test/unit/utils/serialization/test_serialization.cpp` | 10 |
| `test/integration/utils/test_utils_brain_factory.cpp` | 15 |
| `test/integration/utils/test_utils_bio_async.cpp` | 10 |
| `test/integration/utils/test_utils_immune.cpp` | 10 |
| **Total** | **500** |

### 31. Core Module Integration

**Priority**: Critical (Foundation of entire system)
**Dependencies**: All other modules (core provides foundation)
**Est. LOC**: 100,000 | **Tests**: 700

#### 31.1 Core Module Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           CORE MODULE (323 headers, 68 subdirs)                      │
│  ┌───────────────────────────────────────────────────────────────────────────────┐  │
│  │                              BRAIN FACTORY                                     │  │
│  │  ┌──────────────┐ ┌───────────────┐ ┌────────────────┐ ┌────────────────┐    │  │
│  │  │Brain Config  │ │Config Builder │ │Brain Creation  │ │Component Init  │    │  │
│  │  │ Builder      │ │  Validators   │ │  O(n) Time     │ │  30+ Modules   │    │  │
│  │  └──────────────┘ └───────────────┘ └────────────────┘ └────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                        │                                             │
│  ┌─────────────────────────────────────┴─────────────────────────────────────────┐  │
│  │                         BRAIN KNOWLEDGE GRAPH                                  │  │
│  │  ┌────────────────────────────────────────────────────────────────────────┐   │  │
│  │  │  Node Types: CORE | CORTICAL | SUBCORTICAL | BRAINSTEM | COGNITIVE     │   │  │
│  │  │              PERCEPTION | PLASTICITY | TRAINING | SWARM | SECURITY     │   │  │
│  │  │              INTEGRATION | COORDINATOR | UTILITY                        │   │  │
│  │  └────────────────────────────────────────────────────────────────────────┘   │  │
│  │  ┌────────────────────────────────────────────────────────────────────────┐   │  │
│  │  │  Edge Types: CONNECTS_TO | SENDS_TO | RECEIVES_FROM | INTEGRATES_WITH  │   │  │
│  │  │              MODULATES | EXCITES | INHIBITS | COORDINATES_WITH | DEPENDS_ON │  │
│  │  └────────────────────────────────────────────────────────────────────────┘   │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                        │                                             │
│  ┌─────────────────────────────────────┴─────────────────────────────────────────┐  │
│  │                        BRAIN REGIONS (13 regions, 77 headers)                  │  │
│  │  ┌──────────┐ ┌───────────┐ ┌────────────┐ ┌───────────┐ ┌────────────┐      │  │
│  │  │Brainstem │ │  Broca    │ │Cerebellum  │ │Cingulate  │ │Hippocampus │      │  │
│  │  │  (core)  │ │(language) │ │  (motor)   │ │(conflict) │ │  (memory)  │      │  │
│  │  └──────────┘ └───────────┘ └────────────┘ └───────────┘ └────────────┘      │  │
│  │  ┌──────────┐ ┌───────────┐ ┌────────────┐ ┌───────────┐ ┌────────────┐      │  │
│  │  │Hypothala │ │  Insula   │ │   Motor    │ │ Occipital │ │  Parietal  │      │  │
│  │  │(homestas)│ │(interocep)│ │  (action)  │ │  (visual) │ │ (spatial)  │      │  │
│  │  └──────────┘ └───────────┘ └────────────┘ └───────────┘ └────────────┘      │  │
│  │  ┌──────────┐ ┌───────────┐ ┌────────────┐                                    │  │
│  │  │Prefrontal│ │ Temporal  │ │ Wernicke   │                                    │  │
│  │  │(execute) │ │ (auditory)│ │(comprehen) │                                    │  │
│  │  └──────────┘ └───────────┘ └────────────┘                                    │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                        │                                             │
│  ┌─────────────────────────────────────┴─────────────────────────────────────────┐  │
│  │                         CORTICAL COLUMNS (27 headers)                          │  │
│  │  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────────────────────┐  │  │
│  │  │   Minicolumns   │ │  Hypercolumns   │ │    Sleep Bridges (8 types)      │  │  │
│  │  │  (~80-100 neur) │ │ (col. groups)   │ │  STDP, BCM, STP, Homeostatic,   │  │  │
│  │  │  Layer 2/3,4,5/6│ │  Mexican hat    │ │  Eligibility, Protein, IO, FB   │  │  │
│  │  └─────────────────┘ └─────────────────┘ └─────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                        │                                             │
│  ┌─────────────────────────────────────┴─────────────────────────────────────────┐  │
│  │                         NEURAL SUBSTRATE                                       │  │
│  │  ┌─────────────────────────────────┐ ┌─────────────────────────────────────┐  │  │
│  │  │   METABOLIC SUBSTRATE           │ │     PHYSICAL SUBSTRATE              │  │  │
│  │  │  • ATP (energy currency)        │ │  • Temperature (36-38°C)            │  │  │
│  │  │  • O2  (oxidative metabolism)   │ │  • Membrane integrity (0-1)         │  │  │
│  │  │  • Glucose (fuel)               │ │  • Ion balance (Na/K/Ca/Cl)         │  │  │
│  │  └─────────────────────────────────┘ └─────────────────────────────────────┘  │  │
│  │  ┌─────────────────────────────────────────────────────────────────────────┐  │  │
│  │  │              MODULATION OUTPUTS                                          │  │  │
│  │  │  firing_rate | transmission_efficiency | conduction_velocity | plasticity│  │  │
│  │  └─────────────────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                        │                                             │
│  ┌─────────────────────────────────────┴─────────────────────────────────────────┐  │
│  │                         NEURON MODELS (Plugin Architecture)                    │  │
│  │  ┌───────────┐ ┌───────────┐ ┌─────────────────┐ ┌───────┐ ┌───────────────┐ │  │
│  │  │    LIF    │ │Izhikevich │ │ Two-Compartment │ │  AdEx │ │Hodgkin-Huxley │ │  │
│  │  │(simplest) │ │(efficient)│ │   (dendritic)   │ │(adapt)│ │ (biophysical) │ │  │
│  │  └───────────┘ └───────────┘ └─────────────────┘ └───────┘ └───────────────┘ │  │
│  │  ODE Integration: Euler | RK2 | RK4 (Strategy Pattern)                        │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                        │                                             │
│  ┌─────────────────────────────────────┴─────────────────────────────────────────┐  │
│  │                         SUPPORTING SYSTEMS                                     │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐ │  │
│  │  │  Directives  │ │   Sensory    │ │   Topology   │ │    Synapse Types     │ │  │
│  │  │  (9 headers) │ │  (6 headers) │ │  (4 headers) │ │  AMPA/NMDA/GABA/etc  │ │  │
│  │  │  Ethics/Harm │ │Visual/Audit/ │ │Network Struct│ │  Neuromodulatory     │ │  │
│  │  │  Prevention  │ │ Somatosens   │ │              │ │                      │ │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

#### 31.2 Core Module Component Summary

| Category | Subdirs | Headers | Purpose |
|----------|---------|---------|---------|
| **Brain Factory** | 3 | 35+ | Brain creation, initialization, configuration |
| **Brain Regions** | 13 | 77 | All cortical/subcortical regions |
| **Cortical Columns** | 2 | 27 | Minicolumns, hypercolumns, sleep bridges |
| **Neural Substrate** | 1 | 8 | Metabolic/physical modeling |
| **Neuron Models** | 1 | 12 | LIF, Izhikevich, HH, AdEx, etc. |
| **Synapse Types** | 1 | 15 | AMPA, NMDA, GABA, neuromodulatory |
| **Directives** | 1 | 9 | Ethics, harm prevention, safety |
| **Sensory Systems** | 3 | 6 | Visual, auditory, somatosensory |
| **Topology** | 1 | 4 | Network structure definitions |
| **Brain KG** | 1 | 5 | Internal knowledge graph |
| **Other Core** | 41 | 125+ | Various core functionality |
| **Total** | **68** | **323** | **Complete core infrastructure** |

#### 31.3 Brain Factory System

```c
// Brain factory configuration (from nimcp_brain_factory.h)
typedef struct nimcp_brain_config {
    // Core parameters
    size_t neuron_count;                    // Total neurons (default: 86B scaled)
    size_t synapse_count;                   // Total synapses (default: 100T scaled)
    float simulation_timestep_ms;           // Simulation dt (default: 1.0)

    // Memory management
    size_t memory_pool_size;                // Memory pool size
    size_t working_memory_capacity;         // WM slots (default: 7±2)

    // Component flags
    bool enable_immune_system;              // Brain immune (default: true)
    bool enable_sleep_system;               // Sleep/circadian (default: true)
    bool enable_quantum_effects;            // Quantum coherence (default: false)
    bool enable_glial_cells;                // Astrocytes (default: true)

    // Platform tier
    nimcp_platform_tier_t platform_tier;    // FULL/MEDIUM/CONSTRAINED/MINIMAL

    // Logging
    nimcp_log_level_t log_level;            // Default: INFO
    const char* log_file;                   // Optional log file

    // Brain regions to enable
    uint32_t enabled_regions;               // Bitmask of regions
} nimcp_brain_config_t;

// Brain factory API
nimcp_brain_config_t nimcp_brain_config_default(void);
nimcp_brain_config_builder_t* nimcp_brain_config_builder_create(void);
nimcp_brain_config_builder_t* nimcp_brain_config_builder_set_neuron_count(
    nimcp_brain_config_builder_t* builder, size_t count);
nimcp_brain_config_builder_t* nimcp_brain_config_builder_enable_region(
    nimcp_brain_config_builder_t* builder, nimcp_brain_region_t region);
nimcp_brain_config_t nimcp_brain_config_builder_build(
    nimcp_brain_config_builder_t* builder);

// Brain creation (O(n) time complexity)
nimcp_brain_t* nimcp_brain_create(const nimcp_brain_config_t* config);
void nimcp_brain_destroy(nimcp_brain_t* brain);

// Component initialization (30+ init functions)
nimcp_status_t nimcp_brain_init_snn(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_plasticity(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_cognitive(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_perception(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_immune(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_sleep(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_thalamus(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_hypothalamus(nimcp_brain_t* brain);
nimcp_status_t nimcp_brain_init_bio_async(nimcp_brain_t* brain);
// ... 20+ more init functions
```

#### 31.4 Brain Knowledge Graph (Self-Awareness)

```c
// Brain KG node types (from nimcp_brain_kg.h)
typedef enum {
    BRAIN_KG_NODE_CORE = 0,           // Core infrastructure
    BRAIN_KG_NODE_CORTICAL,           // Cortical regions
    BRAIN_KG_NODE_SUBCORTICAL,        // Subcortical structures
    BRAIN_KG_NODE_BRAINSTEM,          // Brainstem nuclei
    BRAIN_KG_NODE_COGNITIVE,          // Cognitive systems
    BRAIN_KG_NODE_PERCEPTION,         // Perceptual systems
    BRAIN_KG_NODE_PLASTICITY,         // Learning systems
    BRAIN_KG_NODE_TRAINING,           // Training infrastructure
    BRAIN_KG_NODE_SWARM,              // Swarm intelligence
    BRAIN_KG_NODE_SECURITY,           // Security components
    BRAIN_KG_NODE_INTEGRATION,        // Integration bridges
    BRAIN_KG_NODE_COORDINATOR,        // Coordination systems
    BRAIN_KG_NODE_UTILITY,            // Utility components
    BRAIN_KG_NODE_COUNT
} brain_kg_node_type_t;

// Brain KG edge types
typedef enum {
    BRAIN_KG_EDGE_CONNECTS_TO = 0,    // Structural connection
    BRAIN_KG_EDGE_SENDS_TO,           // Unidirectional data flow
    BRAIN_KG_EDGE_RECEIVES_FROM,      // Reverse data flow
    BRAIN_KG_EDGE_INTEGRATES_WITH,    // Bidirectional integration
    BRAIN_KG_EDGE_MODULATES,          // Modulatory influence
    BRAIN_KG_EDGE_EXCITES,            // Excitatory connection
    BRAIN_KG_EDGE_INHIBITS,           // Inhibitory connection
    BRAIN_KG_EDGE_COORDINATES_WITH,   // Coordination relationship
    BRAIN_KG_EDGE_DEPENDS_ON,         // Dependency relationship
    BRAIN_KG_EDGE_COUNT
} brain_kg_edge_type_t;

// Brain KG node structure
typedef struct {
    uint32_t id;                       // Unique node ID
    brain_kg_node_type_t type;         // Node type
    char name[64];                     // Human-readable name
    void* component_ptr;               // Pointer to actual component
    uint32_t edge_count;               // Number of edges
    uint32_t* edge_ids;                // Edge ID array
    float health_status;               // Component health (0-1)
    uint64_t last_updated_ms;          // Last update timestamp
} brain_kg_node_t;

// Brain KG API
nimcp_brain_kg_t* nimcp_brain_kg_create(nimcp_brain_t* brain);
void nimcp_brain_kg_destroy(nimcp_brain_kg_t* kg);

// CRUD operations
nimcp_status_t nimcp_brain_kg_add_node(nimcp_brain_kg_t* kg,
    const char* name, brain_kg_node_type_t type, void* component,
    uint32_t* node_id_out);
nimcp_status_t nimcp_brain_kg_add_edge(nimcp_brain_kg_t* kg,
    uint32_t from_id, uint32_t to_id, brain_kg_edge_type_t type,
    float weight, uint32_t* edge_id_out);
nimcp_status_t nimcp_brain_kg_remove_node(nimcp_brain_kg_t* kg, uint32_t node_id);
nimcp_status_t nimcp_brain_kg_remove_edge(nimcp_brain_kg_t* kg, uint32_t edge_id);

// Query operations
nimcp_status_t nimcp_brain_kg_get_node(nimcp_brain_kg_t* kg, uint32_t node_id,
    brain_kg_node_t* node_out);
nimcp_status_t nimcp_brain_kg_find_nodes_by_type(nimcp_brain_kg_t* kg,
    brain_kg_node_type_t type, uint32_t* node_ids, uint32_t* count);
nimcp_status_t nimcp_brain_kg_get_neighbors(nimcp_brain_kg_t* kg,
    uint32_t node_id, brain_kg_edge_type_t edge_type,
    uint32_t* neighbor_ids, uint32_t* count);
nimcp_status_t nimcp_brain_kg_path_exists(nimcp_brain_kg_t* kg,
    uint32_t from_id, uint32_t to_id, bool* exists);

// Self-awareness queries
nimcp_status_t nimcp_brain_kg_get_component_health(nimcp_brain_kg_t* kg,
    uint32_t node_id, float* health_out);
nimcp_status_t nimcp_brain_kg_get_active_pathways(nimcp_brain_kg_t* kg,
    uint32_t** pathway_ids, uint32_t* count);
nimcp_status_t nimcp_brain_kg_introspect(nimcp_brain_kg_t* kg,
    brain_kg_introspection_result_t* result);
```

#### 31.5 Brain Regions (13 Regions, 77 Headers)

| Region | Headers | Primary Functions |
|--------|---------|-------------------|
| **Brainstem** | 6 | Autonomic control, arousal, vital functions |
| **Broca** | 8 | Speech production, language output |
| **Cerebellum** | 7 | Motor coordination, timing, learning |
| **Cingulate** | 5 | Error detection, conflict monitoring, emotion |
| **Hippocampus** | 8 | Memory formation, spatial navigation |
| **Hypothalamus** | 6 | Homeostasis, hormone regulation, motivation |
| **Insula** | 5 | Interoception, emotion awareness, empathy |
| **Motor** | 7 | Movement planning, execution, sequences |
| **Occipital** | 6 | Visual processing, object recognition |
| **Parietal** | 5 | Spatial processing, attention, integration |
| **Prefrontal** | 8 | Executive function, decision making, planning |
| **Temporal** | 6 | Auditory processing, semantic memory |
| **Wernicke** | 8 | Language comprehension, speech perception |

#### 31.6 Cortical Columns

```c
// Cortical minicolumn (from nimcp_cortical_column.h)
typedef struct {
    uint32_t id;                        // Minicolumn ID
    uint32_t hypercolumn_id;            // Parent hypercolumn

    // Layer populations (canonical microcircuit)
    nimcp_layer_t layer_2_3;            // Superficial pyramidal (cortico-cortical)
    nimcp_layer_t layer_4;              // Granular (thalamic input)
    nimcp_layer_t layer_5_6;            // Deep pyramidal (output)

    // Neuron counts (~80-100 per minicolumn)
    uint32_t neuron_count;
    uint32_t excitatory_count;          // ~80%
    uint32_t inhibitory_count;          // ~20%

    // Local connectivity
    float lateral_inhibition_strength;   // Mexican hat pattern
    float vertical_excitation_strength;  // Inter-layer connections

    // State
    float average_firing_rate;
    float columnar_activation;           // 0-1 activation level

    // Integration
    nimcp_bio_async_handler_t bio_handler;
    nimcp_immune_sensor_t immune_sensor;
    nimcp_logger_t* logger;
} nimcp_minicolumn_t;

// Hypercolumn (group of minicolumns)
typedef struct {
    uint32_t id;                         // Hypercolumn ID
    uint32_t region_id;                  // Parent brain region

    // Minicolumn array
    nimcp_minicolumn_t* minicolumns;
    uint32_t minicolumn_count;           // Typically 50-100

    // Feature selectivity
    float preferred_orientation;         // For V1
    float preferred_frequency;           // For A1
    float receptive_field_center[3];     // x, y, z

    // Lateral inhibition (Mexican hat)
    float center_excitation_radius;
    float surround_inhibition_radius;
    float inhibition_strength;

    // State
    float winner_take_all_threshold;
} nimcp_hypercolumn_t;

// Cortical column API
nimcp_minicolumn_t* nimcp_minicolumn_create(const nimcp_minicolumn_config_t* config);
void nimcp_minicolumn_destroy(nimcp_minicolumn_t* col);
nimcp_status_t nimcp_minicolumn_update(nimcp_minicolumn_t* col,
    const float* inputs, float dt);
nimcp_status_t nimcp_minicolumn_get_output(nimcp_minicolumn_t* col,
    float* outputs, uint32_t* count);

nimcp_hypercolumn_t* nimcp_hypercolumn_create(const nimcp_hypercolumn_config_t* config);
void nimcp_hypercolumn_destroy(nimcp_hypercolumn_t* hcol);
nimcp_status_t nimcp_hypercolumn_update(nimcp_hypercolumn_t* hcol,
    const float* inputs, float dt);
nimcp_status_t nimcp_hypercolumn_apply_lateral_inhibition(nimcp_hypercolumn_t* hcol);
```

#### 31.7 Neural Substrate

```c
// Metabolic substrate (from nimcp_neural_substrate.h)
typedef struct {
    // Energy currency
    float atp_level;                     // 0-1 (normalized)
    float atp_production_rate;           // ATP/ms
    float atp_consumption_rate;          // ATP/ms

    // Oxygen metabolism
    float o2_level;                      // 0-1 (normalized)
    float o2_extraction_fraction;        // OEF (default: 0.3-0.4)

    // Glucose metabolism
    float glucose_level;                 // 0-1 (normalized)
    float glucose_uptake_rate;           // Glc/ms
    float lactate_level;                 // Astrocyte-neuron lactate shuttle

    // Derived metrics
    float metabolic_rate;                // CMRO2 (cerebral metabolic rate O2)
    float energy_efficiency;             // Work output / ATP consumed
} nimcp_metabolic_substrate_t;

// Physical substrate
typedef struct {
    // Temperature
    float temperature_celsius;           // Brain temp (36-38°C)
    float thermal_stress;                // 0-1

    // Membrane integrity
    float membrane_integrity;            // 0-1
    float lipid_peroxidation;            // Oxidative damage marker

    // Ion balance
    float na_intracellular;              // [Na+]i (mM)
    float k_extracellular;               // [K+]o (mM)
    float ca_intracellular;              // [Ca2+]i (nM to μM)
    float cl_intracellular;              // [Cl-]i (mM)

    // pH
    float intracellular_ph;              // ~7.2
    float extracellular_ph;              // ~7.4
} nimcp_physical_substrate_t;

// Substrate modulation outputs
typedef struct {
    float firing_rate_modifier;          // 0-2 (1 = normal)
    float transmission_efficiency;       // 0-1
    float conduction_velocity_modifier;  // 0-2
    float plasticity_capacity;           // 0-1 (learning capability)
} nimcp_substrate_modulation_t;

// Neural substrate API
nimcp_neural_substrate_t* nimcp_neural_substrate_create(
    const nimcp_neural_substrate_config_t* config);
void nimcp_neural_substrate_destroy(nimcp_neural_substrate_t* substrate);
nimcp_status_t nimcp_neural_substrate_update(nimcp_neural_substrate_t* substrate,
    float activity_level, float dt);
nimcp_status_t nimcp_neural_substrate_get_modulation(
    nimcp_neural_substrate_t* substrate,
    nimcp_substrate_modulation_t* modulation_out);
nimcp_status_t nimcp_neural_substrate_inject_atp(
    nimcp_neural_substrate_t* substrate, float amount);
nimcp_status_t nimcp_neural_substrate_set_temperature(
    nimcp_neural_substrate_t* substrate, float temp_celsius);
```

#### 31.8 Neuron Models (Plugin Architecture)

```c
// Neuron model types (from nimcp_neuron_model.h)
typedef enum {
    NEURON_MODEL_LIF = 0,               // Leaky Integrate-and-Fire
    NEURON_MODEL_IZHIKEVICH,            // Izhikevich 2003
    NEURON_MODEL_TWO_COMPARTMENT,       // Soma + dendrite
    NEURON_MODEL_ADEX,                  // Adaptive Exponential IF
    NEURON_MODEL_HODGKIN_HUXLEY,        // Full biophysical
    NEURON_MODEL_COUNT
} nimcp_neuron_model_type_t;

// ODE integration methods
typedef enum {
    ODE_METHOD_EULER = 0,               // First-order Euler
    ODE_METHOD_RK2,                     // Second-order Runge-Kutta
    ODE_METHOD_RK4,                     // Fourth-order Runge-Kutta
} nimcp_ode_method_t;

// Neuron model interface (Strategy Pattern)
typedef struct nimcp_neuron_model_vtable {
    nimcp_status_t (*init)(void* state, const void* params);
    nimcp_status_t (*update)(void* state, float I_ext, float dt,
                              nimcp_ode_method_t method);
    nimcp_status_t (*reset)(void* state);
    nimcp_status_t (*get_voltage)(const void* state, float* V);
    nimcp_status_t (*check_spike)(const void* state, bool* spiked);
    nimcp_status_t (*get_state_size)(size_t* size);
    nimcp_status_t (*serialize)(const void* state, uint8_t* buffer, size_t* size);
    nimcp_status_t (*deserialize)(void* state, const uint8_t* buffer, size_t size);
    void (*destroy)(void* state);
} nimcp_neuron_model_vtable_t;

// Neuron model handle
typedef struct {
    nimcp_neuron_model_type_t type;
    const nimcp_neuron_model_vtable_t* vtable;
    void* state;
    nimcp_logger_t* logger;
} nimcp_neuron_model_t;

// LIF parameters
typedef struct {
    float tau_m;                         // Membrane time constant (ms)
    float V_rest;                        // Resting potential (mV)
    float V_thresh;                      // Spike threshold (mV)
    float V_reset;                       // Reset potential (mV)
    float R_m;                           // Membrane resistance (MΩ)
    float t_ref;                         // Refractory period (ms)
} nimcp_lif_params_t;

// Izhikevich parameters
typedef struct {
    float a;                             // Recovery time constant
    float b;                             // Sensitivity of recovery
    float c;                             // Reset voltage (mV)
    float d;                             // Reset recovery increment
} nimcp_izhikevich_params_t;

// Neuron model factory
nimcp_neuron_model_t* nimcp_neuron_model_create(nimcp_neuron_model_type_t type,
    const void* params);
void nimcp_neuron_model_destroy(nimcp_neuron_model_t* model);

// Convenience functions
nimcp_neuron_model_t* nimcp_neuron_model_create_lif(const nimcp_lif_params_t* params);
nimcp_neuron_model_t* nimcp_neuron_model_create_izhikevich(
    const nimcp_izhikevich_params_t* params);
nimcp_neuron_model_t* nimcp_neuron_model_create_hh(
    const nimcp_hh_params_t* params);
```

#### 31.9 Directives System (Ethics & Safety)

| Directive | Header | Purpose |
|-----------|--------|---------|
| **Core Directives** | `nimcp_directives.h` | Central directive management |
| **Harm Prevention** | `nimcp_harm_prevention.h` | Prevent harmful outputs |
| **Ethics Engine** | `nimcp_ethics_engine.h` | Ethical reasoning |
| **Safety Constraints** | `nimcp_safety_constraints.h` | Hard safety limits |
| **Value Alignment** | `nimcp_value_alignment.h` | Human value alignment |
| **Consent Checker** | `nimcp_consent_checker.h` | Action consent verification |
| **Transparency** | `nimcp_transparency.h` | Decision explainability |
| **Override Protocol** | `nimcp_override_protocol.h` | Emergency overrides |
| **Audit Log** | `nimcp_audit_log.h` | Action audit trail |

#### 31.10 Core Module Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| **Factory-Immune** | `nimcp_factory_immune_bridge.h/c` | Immune system initialization |
| **Factory-BioAsync** | `nimcp_factory_bio_async_bridge.h/c` | Bio-async initialization |
| **Factory-Logging** | `nimcp_factory_logging_bridge.h/c` | Logging initialization |
| **KG-Introspection** | `nimcp_kg_introspection_bridge.h/c` | Self-awareness queries |
| **KG-Hub** | `nimcp_kg_hub_bridge.h/c` | Cognitive hub integration |
| **Column-SNN** | `nimcp_column_snn_bridge.h/c` | SNN population in columns |
| **Column-Plasticity** | `nimcp_column_plasticity_bridge.h/c` | Column-level plasticity |
| **Column-Sleep** | `nimcp_column_sleep_bridge.h/c` | Sleep state effects |
| **Substrate-Immune** | `nimcp_substrate_immune_bridge.h/c` | Metabolic immune effects |
| **Substrate-SNN** | `nimcp_substrate_snn_bridge.h/c` | Metabolic SNN modulation |
| **Model-SNN** | `nimcp_model_snn_bridge.h/c` | Neuron model integration |
| **Region-FEP** | `nimcp_region_fep_bridge.h/c` | FEP per brain region |
| **Region-Hub** | `nimcp_region_hub_bridge.h/c` | Region cognitive hub integration |
| **Directive-Executive** | `nimcp_directive_executive_bridge.h/c` | Directive enforcement |

#### 31.11 Core Module Integration with Brain Factory

```c
// File: src/core/brain/factory/nimcp_brain_factory_integration.c

nimcp_status_t nimcp_brain_factory_full_integration(
    nimcp_brain_t* brain,
    const nimcp_brain_config_t* config) {

    NIMCP_LOG_INFO(brain->logger, "Beginning full brain integration");

    //=========================================================================
    // Phase 1: Core Infrastructure
    //=========================================================================

    // Initialize memory pool
    brain->memory_pool = nimcp_memory_pool_create(config->memory_pool_size);
    NIMCP_RETURN_IF_ERROR(brain->memory_pool != NULL);

    // Initialize logging
    brain->logger = nimcp_logger_create(config->log_level, config->log_file);
    NIMCP_RETURN_IF_ERROR(brain->logger != NULL);

    // Initialize bio-async router
    brain->bio_router = nimcp_bio_router_create(brain->memory_pool);
    NIMCP_RETURN_IF_ERROR(brain->bio_router != NULL);

    // Initialize immune system
    if (config->enable_immune_system) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_immune(brain) == NIMCP_OK);
    }

    //=========================================================================
    // Phase 2: Brain Knowledge Graph
    //=========================================================================

    brain->kg = nimcp_brain_kg_create(brain);
    NIMCP_RETURN_IF_ERROR(brain->kg != NULL);

    // Register core components in KG
    nimcp_brain_kg_register_component(brain->kg, "memory_pool",
        BRAIN_KG_NODE_UTILITY, brain->memory_pool);
    nimcp_brain_kg_register_component(brain->kg, "bio_router",
        BRAIN_KG_NODE_INTEGRATION, brain->bio_router);
    nimcp_brain_kg_register_component(brain->kg, "immune_system",
        BRAIN_KG_NODE_CORE, brain->immune);

    //=========================================================================
    // Phase 3: Neural Substrate
    //=========================================================================

    brain->neural_substrate = nimcp_neural_substrate_create(
        &config->substrate_config);
    NIMCP_RETURN_IF_ERROR(brain->neural_substrate != NULL);

    // Connect substrate to immune system
    nimcp_substrate_immune_bridge_init(
        &brain->substrate_immune_bridge,
        brain->neural_substrate, brain->immune);

    //=========================================================================
    // Phase 4: Neuron Models (Plugin Registry)
    //=========================================================================

    brain->neuron_model_registry = nimcp_neuron_model_registry_create();

    // Register all model types
    nimcp_neuron_model_registry_register(brain->neuron_model_registry,
        NEURON_MODEL_LIF, &lif_vtable);
    nimcp_neuron_model_registry_register(brain->neuron_model_registry,
        NEURON_MODEL_IZHIKEVICH, &izhikevich_vtable);
    nimcp_neuron_model_registry_register(brain->neuron_model_registry,
        NEURON_MODEL_ADEX, &adex_vtable);
    nimcp_neuron_model_registry_register(brain->neuron_model_registry,
        NEURON_MODEL_TWO_COMPARTMENT, &two_compartment_vtable);
    nimcp_neuron_model_registry_register(brain->neuron_model_registry,
        NEURON_MODEL_HODGKIN_HUXLEY, &hh_vtable);

    //=========================================================================
    // Phase 5: Cortical Columns
    //=========================================================================

    brain->cortical_system = nimcp_cortical_system_create(brain->memory_pool);

    // Initialize hypercolumns per enabled region
    for (int r = 0; r < BRAIN_REGION_COUNT; r++) {
        if (config->enabled_regions & (1 << r)) {
            uint32_t hypercolumn_count = get_region_hypercolumn_count(r);
            for (uint32_t h = 0; h < hypercolumn_count; h++) {
                nimcp_hypercolumn_t* hcol = nimcp_hypercolumn_create(
                    &get_hypercolumn_config(r, h));
                nimcp_cortical_system_add_hypercolumn(
                    brain->cortical_system, r, hcol);
            }
        }
    }

    // Initialize column sleep bridges
    nimcp_column_sleep_bridges_init(brain->cortical_system, brain->sleep_system);

    //=========================================================================
    // Phase 6: Brain Regions
    //=========================================================================

    // Initialize each enabled region
    if (config->enabled_regions & REGION_PREFRONTAL) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_prefrontal(brain) == NIMCP_OK);
    }
    if (config->enabled_regions & REGION_TEMPORAL) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_temporal(brain) == NIMCP_OK);
    }
    if (config->enabled_regions & REGION_PARIETAL) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_parietal(brain) == NIMCP_OK);
    }
    if (config->enabled_regions & REGION_OCCIPITAL) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_occipital(brain) == NIMCP_OK);
    }
    if (config->enabled_regions & REGION_HIPPOCAMPUS) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_hippocampus(brain) == NIMCP_OK);
    }
    if (config->enabled_regions & REGION_BROCA) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_broca(brain) == NIMCP_OK);
    }
    if (config->enabled_regions & REGION_WERNICKE) {
        NIMCP_RETURN_IF_ERROR(nimcp_brain_init_wernicke(brain) == NIMCP_OK);
    }
    // ... other regions

    //=========================================================================
    // Phase 7: Directives System
    //=========================================================================

    brain->directives = nimcp_directives_create(brain->memory_pool);
    nimcp_harm_prevention_init(&brain->directives->harm_prevention);
    nimcp_ethics_engine_init(&brain->directives->ethics_engine);
    nimcp_safety_constraints_init(&brain->directives->safety_constraints);

    // Connect directives to executive function
    nimcp_directive_executive_bridge_init(
        &brain->directive_executive_bridge,
        brain->directives, brain->executive);

    //=========================================================================
    // Phase 8: Sensory Systems
    //=========================================================================

    brain->sensory = nimcp_sensory_system_create(brain->memory_pool);

    // Visual pathway (V1 → V2 → V4 → IT → PFC)
    nimcp_visual_pathway_init(&brain->sensory->visual, brain->occipital);

    // Auditory pathway (A1 → belt → parabelt → STS → PFC)
    nimcp_auditory_pathway_init(&brain->sensory->auditory, brain->temporal);

    // Somatosensory pathway (S1 → S2 → parietal)
    nimcp_somatosensory_pathway_init(&brain->sensory->somatosensory, brain->parietal);

    //=========================================================================
    // Phase 9: Final Integration
    //=========================================================================

    // Build KG edges for all connections
    nimcp_brain_kg_build_edges(brain->kg);

    // Validate integration integrity
    NIMCP_RETURN_IF_ERROR(nimcp_brain_validate_integration(brain) == NIMCP_OK);

    NIMCP_LOG_INFO(brain->logger, "Brain integration complete: %d regions, "
        "%d hypercolumns, %d components",
        __builtin_popcount(config->enabled_regions),
        nimcp_cortical_system_get_hypercolumn_count(brain->cortical_system),
        nimcp_brain_kg_get_node_count(brain->kg));

    return NIMCP_OK;
}
```

#### 31.12 Core Module Performance Requirements

| Operation | Complexity | Typical Time |
|-----------|------------|--------------|
| Brain creation | O(N) neurons | 100 ms |
| Region initialization | O(R) regions | 10 ms per region |
| Hypercolumn update | O(M) minicolumns | 100 μs |
| Minicolumn update | O(N) neurons | 10 μs |
| Neuron model update | O(1) | 1 μs (LIF) to 100 μs (HH) |
| KG query | O(V + E) | 10 μs |
| Substrate update | O(1) | 5 μs |
| Directive check | O(D) directives | 1 μs |

#### 31.13 Core Module Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/core/brain/test_brain_create.cpp` | 25 |
| `test/unit/core/brain/test_brain_config.cpp` | 20 |
| `test/unit/core/brain/test_brain_config_builder.cpp` | 15 |
| `test/unit/core/brain/test_brain_destroy.cpp` | 10 |
| `test/unit/core/brain/factory/test_brain_factory.cpp` | 25 |
| `test/unit/core/brain/factory/test_brain_init_snn.cpp` | 15 |
| `test/unit/core/brain/factory/test_brain_init_cognitive.cpp` | 15 |
| `test/unit/core/brain/factory/test_brain_init_perception.cpp` | 15 |
| `test/unit/core/brain/kg/test_brain_kg_create.cpp` | 20 |
| `test/unit/core/brain/kg/test_brain_kg_nodes.cpp` | 25 |
| `test/unit/core/brain/kg/test_brain_kg_edges.cpp` | 25 |
| `test/unit/core/brain/kg/test_brain_kg_queries.cpp` | 20 |
| `test/unit/core/brain/kg/test_brain_kg_introspection.cpp` | 15 |
| `test/unit/core/cortical_columns/test_minicolumn.cpp` | 25 |
| `test/unit/core/cortical_columns/test_hypercolumn.cpp` | 25 |
| `test/unit/core/cortical_columns/test_lateral_inhibition.cpp` | 15 |
| `test/unit/core/cortical_columns/test_column_sleep_bridges.cpp` | 20 |
| `test/unit/core/neural_substrate/test_metabolic_substrate.cpp` | 25 |
| `test/unit/core/neural_substrate/test_physical_substrate.cpp` | 20 |
| `test/unit/core/neural_substrate/test_substrate_modulation.cpp` | 15 |
| `test/unit/core/neuron_models/test_lif_model.cpp` | 20 |
| `test/unit/core/neuron_models/test_izhikevich_model.cpp` | 20 |
| `test/unit/core/neuron_models/test_adex_model.cpp` | 15 |
| `test/unit/core/neuron_models/test_hh_model.cpp` | 25 |
| `test/unit/core/neuron_models/test_model_registry.cpp` | 15 |
| `test/unit/core/directives/test_directives.cpp` | 20 |
| `test/unit/core/directives/test_harm_prevention.cpp` | 20 |
| `test/unit/core/directives/test_ethics_engine.cpp` | 15 |
| `test/unit/core/directives/test_safety_constraints.cpp` | 15 |
| `test/unit/core/brain/regions/test_prefrontal.cpp` | 15 |
| `test/unit/core/brain/regions/test_temporal.cpp` | 15 |
| `test/unit/core/brain/regions/test_hippocampus.cpp` | 15 |
| `test/unit/core/brain/regions/test_occipital.cpp` | 15 |
| `test/unit/core/brain/regions/test_parietal.cpp` | 15 |
| `test/unit/core/sensory/test_visual_pathway.cpp` | 15 |
| `test/unit/core/sensory/test_auditory_pathway.cpp` | 15 |
| `test/unit/core/sensory/test_somatosensory_pathway.cpp` | 10 |
| `test/integration/core/test_brain_factory_integration.cpp` | 25 |
| `test/integration/core/test_brain_kg_integration.cpp` | 20 |
| `test/integration/core/test_cortical_column_integration.cpp` | 20 |
| `test/integration/core/test_neural_substrate_integration.cpp` | 15 |
| `test/integration/core/test_neuron_model_integration.cpp` | 15 |
| `test/integration/core/test_region_hub_integration.cpp` | 15 |
| `test/regression/core/test_brain_factory_regression.cpp` | 15 |
| `test/regression/core/test_brain_kg_regression.cpp` | 10 |
| `test/regression/core/test_cortical_column_regression.cpp` | 10 |
| `test/e2e/e2e_test_brain_creation_pipeline.cpp` | 15 |
| `test/e2e/e2e_test_full_integration_pipeline.cpp` | 20 |
| **Total** | **700** |

### 32. Cognitive Module Integration

**Priority**: Critical (Heart of the system)
**Dependencies**: Core, Async, Utils, Plasticity, SNN
**Est. LOC**: 150,000 | **Tests**: 1,000

#### 32.1 Cognitive Module Architecture Overview

```
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│                        COGNITIVE MODULE (575 headers, 64 subdirs)                          │
│  ┌─────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                           META-CONTROLLER (ACC/dlPFC/rPFC/OFC)                       │  │
│  │  ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐ ┌───────────────┐  │  │
│  │  │Resource Arbitra- │ │Metacognitive     │ │Affective Meta-   │ │Subsystem      │  │  │
│  │  │tion Layer        │ │Control Layer     │ │control Layer     │ │Coordination   │  │  │
│  │  │• WM Slot Alloc   │ │• Uncertainty     │ │• Emotion→Prior   │ │• WM, Exec     │  │  │
│  │  │• Attention Focus │ │• Confidence Est  │ │• Stress→Capacity │ │• Attn, Curio  │  │  │
│  │  │• Learning Rate   │ │• Performance     │ │• Arousal→Focus   │ │• GW, Emotion  │  │  │
│  │  └──────────────────┘ └──────────────────┘ └──────────────────┘ └───────────────┘  │  │
│  └─────────────────────────────────────────────────────────────────────────────────────┘  │
│                                           │                                                │
│  ┌────────────────────────────────────────┴────────────────────────────────────────────┐  │
│  │                    COGNITIVE INTEGRATION HUB (Mediator Pattern)                      │  │
│  │  ┌─────────────────────────────────────────────────────────────────────────────┐   │  │
│  │  │  Event-Driven Communication | Cross-Module Queries | Publish-Subscribe       │   │  │
│  │  │  23 Integration Bridges: Attention↔WM, Emotion↔Memory, Ethics↔Executive...   │   │  │
│  │  └─────────────────────────────────────────────────────────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────────────────────────────┘  │
│                                           │                                                │
│  ┌────────────────────────────────────────┴────────────────────────────────────────────┐  │
│  │                           GLOBAL WORKSPACE (Baars/Dehaene)                           │  │
│  │  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐   │  │
│  │  │ Broadcast Buffer│ │ Competition     │ │ Ignition       │ │ Subscriber     │   │  │
│  │  │ (limited cap)   │ │ (salience-based)│ │ Threshold      │ │ Notification   │   │  │
│  │  │ ~1 chunk/time   │ │ Winner-take-all │ │ 0.6 default    │ │ Parallel access│   │  │
│  │  └─────────────────┘ └─────────────────┘ └─────────────────┘ └─────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────────────────────────────┘  │
│                                           │                                                │
│  ┌───────────────┬───────────────┬────────┴──────┬───────────────┬───────────────────┐  │
│  │ WORKING MEM   │  EXECUTIVE    │  ATTENTION    │  REASONING    │    EMOTION        │  │
│  │ ────────────  │  ──────────   │  ──────────   │  ──────────   │    ────────       │  │
│  │ 7±2 items     │ Task queue    │ Focus bids    │ Forward chain │ Valence/arousal   │  │
│  │ dlPFC persist │ Goal maintain │ Salience map  │ Backward chain│ Emotion tensor    │  │
│  │ Decay/refresh │ Set-shifting  │ Top-down/BU   │ Unification   │ 8 basic emotions  │  │
│  │ Positional    │ Inhibition    │ Conflict res  │ Quantum reas  │ Emotional tagging │  │
│  └───────────────┴───────────────┴───────────────┴───────────────┴───────────────────┘  │
│                                           │                                                │
│  ┌───────────────┬───────────────┬────────┴──────┬───────────────┬───────────────────┐  │
│  │ CURIOSITY     │ INTROSPECTION │  SELF-MODEL   │ THEORY OF MIND│   META-LEARNING   │  │
│  │ ────────────  │  ──────────   │  ──────────   │  ──────────   │   ────────────    │  │
│  │ Explore/Exp   │ Self-monitor  │ Body schema   │ Belief track  │ Learn-to-learn    │  │
│  │ Novelty seek  │ Uncertainty   │ Agency sense  │ Intention inf │ Strategy select   │  │
│  │ Info gain     │ Metacognition │ Temporal self │ Perspective   │ Hyperparameter    │  │
│  │ Quantum bridge│ Confidence    │ Narrative ID  │ False belief  │ Task transfer     │  │
│  └───────────────┴───────────────┴───────────────┴───────────────┴───────────────────┘  │
│                                           │                                                │
│  ┌───────────────┬───────────────┬────────┴──────┬───────────────┬───────────────────┐  │
│  │ ETHICS        │ IMAGINATION   │ GAME THEORY   │   SOCIAL      │ MIRROR NEURONS    │  │
│  │ ────────────  │  ──────────   │  ──────────   │   ────────    │ ────────────────  │  │
│  │ Core direct   │ Mental simul  │ Nash equilib  │ Social norms  │ Action observ     │  │
│  │ Harm prevent  │ Counterfact   │ Prisoner dilem│ Reputation    │ Empathy mapping   │  │
│  │ Value align   │ Creative gen  │ Tit-for-tat   │ Trust model   │ Imitation learn   │  │
│  │ Combinatorial │ FEP bridge    │ FEP bridge    │ FEP bridge    │ FEP bridge        │  │
│  └───────────────┴───────────────┴───────────────┴───────────────┴───────────────────┘  │
│                                           │                                                │
│  ┌───────────────┬───────────────┬────────┴──────┬───────────────┬───────────────────┐  │
│  │ RECURSIVE COG │ BIAS DETECT   │  EPISTEMIC    │   SALIENCE    │  WELLBEING        │  │
│  │ ────────────  │  ──────────   │  ──────────   │   ──────────  │  ────────────     │  │
│  │ Self-referent │ Cognitive bias│ Knowledge     │ Priority calc │ Mental health     │  │
│  │ Tool router   │ Anchoring     │ Epistemic val │ Relevance     │ Stress monitor    │  │
│  │ Delegation    │ Confirmation  │ Uncertainty   │ Surprise      │ Burnout detect    │  │
│  │ Context store │ Availability  │ Humility      │ FEP bridge    │ Recovery          │  │
│  └───────────────┴───────────────┴───────────────┴───────────────┴───────────────────┘  │
│                                           │                                                │
│  ┌───────────────┬───────────────┬────────┴──────┬───────────────┬───────────────────┐  │
│  │AUTOBIO MEMORY │ CONSOLIDATION │ COLLECTIVE    │   JEPA        │  PARIETAL         │  │
│  │ ────────────  │  ──────────   │  ──────────   │   ────────    │  ────────────     │  │
│  │ Life events   │ Sleep consol  │ Swarm cognit  │ Joint embed   │ Spatial process   │  │
│  │ Self-narrative│ Replay        │ Shared intent │ Prediction    │ Number sense      │  │
│  │ Emotion tag   │ Systems/Synap │ Hyperscanning │ Abstraction   │ Body schema       │  │
│  │ FEP bridge    │ FEP bridge    │ FEP bridge    │ FEP bridge    │ FEP bridge        │  │
│  └───────────────┴───────────────┴───────────────┴───────────────┴───────────────────┘  │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

#### 32.2 Cognitive Module Category Summary

| Category | Subdirectories | Headers | Purpose |
|----------|----------------|---------|---------|
| **Core Cognition** | attention, executive, reasoning, memory | 65+ | Fundamental cognitive processes |
| **Emotional Systems** | emotion, emotions, emotional_tagging, emotion_tensor, emotion_recognition | 30+ | Emotional processing and tagging |
| **Social Cognition** | theory_of_mind, tom, social, mirror_neurons, empathetic_response | 35+ | Social understanding and empathy |
| **Meta-cognition** | introspection, self_model, self_awareness, meta_learning | 30+ | Self-reflection and learning-to-learn |
| **Higher Cognition** | curiosity, imagination, game_theory, symbolic_logic | 40+ | Complex reasoning and planning |
| **Memory Systems** | working_memory, memory, autobiographical_memory, consolidation | 50+ | All memory types |
| **Ethics & Values** | ethics, bias, wellbeing, mental_health | 25+ | Ethical reasoning and wellbeing |
| **Integration** | integration, global_workspace, recursive | 45+ | Module coordination |
| **Specialized** | epistemic, salience, parietal, jepa, collective_cognition | 40+ | Domain-specific processing |
| **Emotional States** | grief, joy, love_loyalty_friendship, shadow_emotions, remorse | 25+ | Complex emotional states |
| **Root-level** | (37 headers directly in /cognitive/) | 37 | Core interfaces and bridges |
| **Total** | **64 subdirectories** | **575** | **Complete cognitive infrastructure** |

#### 32.3 Global Workspace Theory Implementation

```c
// Global Workspace types (from nimcp_global_workspace.h)
typedef struct {
    uint32_t capacity_dim;              // Workspace capacity (default: 256 floats)
    float ignition_threshold;           // Competition threshold (default: 0.6)
    uint32_t max_subscribers;           // Maximum subscriber modules
    uint32_t broadcast_duration_ms;     // How long content stays broadcast
    bool enable_competition;            // Enable competition mechanism
    bool enable_async;                  // Async broadcast delivery
} global_workspace_config_t;

// Cognitive module identifiers
typedef enum {
    MODULE_WORKING_MEMORY = 0,
    MODULE_EXECUTIVE,
    MODULE_ATTENTION,
    MODULE_REASONING,
    MODULE_EMOTION,
    MODULE_CURIOSITY,
    MODULE_THEORY_OF_MIND,
    MODULE_INTROSPECTION,
    MODULE_SELF_MODEL,
    MODULE_META_LEARNING,
    MODULE_ETHICS,
    MODULE_IMAGINATION,
    MODULE_COUNT
} cognitive_module_t;

// Global Workspace API
global_workspace_config_t global_workspace_default_config(void);
global_workspace_t* global_workspace_create(const global_workspace_config_t* config);
void global_workspace_destroy(global_workspace_t* ws);

// Subscription
int global_workspace_subscribe(global_workspace_t* ws, cognitive_module_t module);
int global_workspace_unsubscribe(global_workspace_t* ws, cognitive_module_t module);

// Competition for workspace access
bool global_workspace_compete(global_workspace_t* ws, cognitive_module_t source,
                               const float* content, uint32_t dim, float salience);

// Broadcast reading
bool global_workspace_read_broadcast(global_workspace_t* ws, float* content,
                                      uint32_t max_dim, uint32_t* actual_dim,
                                      cognitive_module_t* source);

// Statistics
int global_workspace_get_stats(const global_workspace_t* ws,
                                global_workspace_stats_t* stats);
```

#### 32.4 Cognitive Meta-Controller

```c
// Meta-controller state (from nimcp_cognitive_meta_controller.h)
typedef struct {
    // Resource arbitration
    uint32_t wm_slots_allocated;        // Current WM slot allocation (max 7±2)
    float attention_focus_strength;      // Current attention focus (0-1)
    float learning_rate_modifier;        // Learning rate scaling factor

    // Metacognitive state
    float uncertainty_level;             // Epistemic uncertainty (0-1)
    float confidence_level;              // Metacognitive confidence (0-1)
    float performance_tracker;           // Recent success rate (0-1)

    // Affective metacontrol
    float emotional_valence;             // Current valence (-1 to +1)
    float stress_level;                  // Stress → capacity reduction (0-1)
    float arousal_level;                 // Arousal → focus width (inverted-U)

    // Subsystem handles
    working_memory_t* working_memory;
    executive_controller_t* executive;
    attention_system_t* attention;
    curiosity_system_t* curiosity;
    global_workspace_t* global_workspace;
    emotional_system_t* emotion;
    brain_immune_system_t* immune;

    // Bio-async integration
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} cognitive_meta_controller_t;

// Arbitration policies
typedef enum {
    ARBITRATION_WINNER_TAKE_ALL = 0,    // Highest priority wins
    ARBITRATION_WEIGHTED_FUSION,         // Weighted average of priorities
    ARBITRATION_EMOTION_BIASED,          // Emotion modulates priority
    ARBITRATION_UNCERTAINTY_DRIVEN,      // Uncertainty → exploration
    ARBITRATION_IMMUNE_MODULATED         // Immune state affects allocation
} arbitration_policy_t;

// Meta-controller API
cognitive_meta_controller_t* cognitive_meta_controller_create(
    const cognitive_meta_controller_config_t* config);
void cognitive_meta_controller_destroy(cognitive_meta_controller_t* mc);

// Resource arbitration
int cognitive_meta_controller_allocate_wm_slot(cognitive_meta_controller_t* mc,
    cognitive_module_t requester, float priority, uint32_t* slot_id_out);
int cognitive_meta_controller_resolve_attention_conflict(
    cognitive_meta_controller_t* mc, float* salience_bids,
    uint32_t bid_count, uint32_t* winner_out);
float cognitive_meta_controller_get_learning_rate_modifier(
    const cognitive_meta_controller_t* mc);

// Metacognitive control
int cognitive_meta_controller_update_uncertainty(
    cognitive_meta_controller_t* mc, float new_uncertainty);
int cognitive_meta_controller_update_confidence(
    cognitive_meta_controller_t* mc, cognitive_module_t module, float confidence);
float cognitive_meta_controller_get_explore_exploit_ratio(
    const cognitive_meta_controller_t* mc);
```

#### 32.5 Cognitive Integration Hub

```c
// Cognitive event types (from nimcp_cognitive_event_types.h)
typedef enum {
    COG_EVENT_ATTENTION_SHIFT = 0,      // Attention focus changed
    COG_EVENT_WM_ITEM_ADDED,            // Item added to working memory
    COG_EVENT_WM_ITEM_EVICTED,          // Item evicted from working memory
    COG_EVENT_EMOTION_CHANGE,           // Emotional state changed
    COG_EVENT_GOAL_ACTIVATED,           // New goal activated
    COG_EVENT_GOAL_COMPLETED,           // Goal completed
    COG_EVENT_CONFLICT_DETECTED,        // Cognitive conflict detected
    COG_EVENT_UNCERTAINTY_HIGH,         // Uncertainty above threshold
    COG_EVENT_INSIGHT,                  // Insight/aha moment
    COG_EVENT_ERROR_DETECTED,           // Error detected (ACC)
    COG_EVENT_BROADCAST,                // Global workspace broadcast
    COG_EVENT_COUNT
} cognitive_event_type_t;

// Cognitive categories
typedef enum {
    COG_CATEGORY_ATTENTION = 0,
    COG_CATEGORY_MEMORY,
    COG_CATEGORY_EXECUTIVE,
    COG_CATEGORY_EMOTION,
    COG_CATEGORY_REASONING,
    COG_CATEGORY_SOCIAL,
    COG_CATEGORY_METACOGNITION,
    COG_CATEGORY_COUNT
} cognitive_category_t;

// Hub configuration
typedef struct {
    uint32_t max_modules;               // Maximum registered modules (default: 64)
    uint32_t max_subscriptions;         // Maximum total subscriptions (default: 256)
    bool enable_async;                  // Enable async event delivery (default: true)
    uint32_t event_queue_size;          // Async event queue size (default: 1024)
} cognitive_hub_config_t;

// Hub API
cognitive_hub_config_t cognitive_hub_default_config(void);
cognitive_integration_hub_t cognitive_hub_create(const cognitive_hub_config_t* config);
void cognitive_hub_destroy(cognitive_integration_hub_t hub);

// Module registration
int cognitive_hub_register_module(cognitive_integration_hub_t hub,
    const char* name, cognitive_category_t category, void* context,
    uint32_t* module_id_out);
int cognitive_hub_unregister_module(cognitive_integration_hub_t hub, uint32_t module_id);

// Event subscription
int cognitive_hub_subscribe(cognitive_integration_hub_t hub, uint32_t module_id,
    cognitive_event_type_t event_type, cognitive_event_callback_t callback);
int cognitive_hub_publish(cognitive_integration_hub_t hub,
    cognitive_event_type_t event_type, const cognitive_event_t* event);

// Cross-module queries
int cognitive_hub_query(cognitive_integration_hub_t hub, uint32_t target_module_id,
    cognitive_query_type_t query_type, const void* query_data,
    void* response_data, size_t response_size);
```

#### 32.6 Working Memory System

```c
// Working memory item (from nimcp_working_memory.h)
typedef struct {
    uint32_t item_id;                   // Unique item ID
    float* content;                     // Item content vector
    uint32_t content_dim;               // Content dimensionality
    float salience;                     // Current salience (0-1)
    float decay_rate;                   // Decay rate per second
    uint64_t creation_time_ms;          // When item was added
    uint64_t last_refresh_ms;           // Last attention refresh
    emotional_tag_t emotional_tag;      // Emotional significance
    uint32_t source_module;             // Which module added this
    positional_encoding_t pos_encoding; // Serial position encoding
} wm_item_t;

// Working memory configuration
typedef struct {
    uint32_t capacity;                  // Max items (default: 7)
    uint32_t item_dim;                  // Item dimensionality
    float decay_time_constant_ms;       // Decay τ (default: 5000ms)
    float refresh_boost;                // Attention refresh multiplier
    bool enable_emotional_tagging;      // Tag items with emotion
    bool enable_positional_encoding;    // Position-based encoding
    sleep_state_t initial_sleep_state;  // Initial sleep state
} working_memory_config_t;

// Working memory API
working_memory_t* working_memory_create(const working_memory_config_t* config);
void working_memory_destroy(working_memory_t* wm);

// Item management
int working_memory_add(working_memory_t* wm, const float* content, uint32_t dim,
                       float salience, cognitive_module_t source, uint32_t* item_id_out);
int working_memory_get(const working_memory_t* wm, uint32_t item_id, wm_item_t* item_out);
int working_memory_refresh(working_memory_t* wm, uint32_t item_id);
int working_memory_remove(working_memory_t* wm, uint32_t item_id);

// Decay and eviction
int working_memory_decay_step(working_memory_t* wm, float dt_ms);
int working_memory_evict_lowest_salience(working_memory_t* wm, uint32_t* evicted_id);

// Query
uint32_t working_memory_get_current_size(const working_memory_t* wm);
int working_memory_get_all_items(const working_memory_t* wm, wm_item_t* items,
                                  uint32_t max_items, uint32_t* actual_count);
```

#### 32.7 Recursive Cognition System

```c
// Recursive cognition types (from nimcp_rcog_types.h)
typedef enum {
    RCOG_STATE_IDLE = 0,
    RCOG_STATE_ANALYZING,
    RCOG_STATE_DELEGATING,
    RCOG_STATE_AWAITING_RESULTS,
    RCOG_STATE_INTEGRATING,
    RCOG_STATE_COMPLETE
} rcog_state_t;

// Recursion depth limits
typedef struct {
    uint32_t max_depth;                 // Maximum recursion depth (default: 10)
    uint32_t max_delegations;           // Max concurrent delegations
    uint64_t timeout_ms;                // Per-delegation timeout
    float delegation_cost_factor;       // Cost multiplier per depth
} rcog_limits_t;

// Tool router for recursive queries
typedef struct {
    const char* tool_name;              // Tool identifier
    tool_capability_t capabilities;     // What this tool can do
    tool_invoke_fn_t invoke_fn;         // Invocation function
    void* context;                      // Tool-specific context
} rcog_tool_t;

// Recursive cognition orchestrator API
rcog_orchestrator_t* rcog_orchestrator_create(const rcog_config_t* config);
void rcog_orchestrator_destroy(rcog_orchestrator_t* orch);

// Query processing
int rcog_process_query(rcog_orchestrator_t* orch, const rcog_query_t* query,
                       rcog_answer_t* answer_out);
int rcog_process_async(rcog_orchestrator_t* orch, const rcog_query_t* query,
                       rcog_task_id_t* task_id_out);
int rcog_get_result(rcog_orchestrator_t* orch, rcog_task_id_t task_id,
                    rcog_answer_t* answer_out);

// Tool registration
int rcog_register_tool(rcog_orchestrator_t* orch, const rcog_tool_t* tool);
int rcog_unregister_tool(rcog_orchestrator_t* orch, const char* tool_name);

// Context management
int rcog_context_store_set(rcog_orchestrator_t* orch, const char* key,
                            const void* value, size_t value_size);
int rcog_context_store_get(rcog_orchestrator_t* orch, const char* key,
                            void* value_out, size_t* value_size);
```

#### 32.8 Theory of Mind System

```c
// Theory of Mind types (from nimcp_theory_of_mind.h)
typedef struct {
    uint32_t agent_id;                  // Agent being modeled
    float* belief_state;                // Agent's believed world state
    float* desire_state;                // Agent's goals/desires
    float* intention_state;             // Agent's current intentions
    float confidence;                   // Confidence in this model
    uint64_t last_updated_ms;           // Last update time
} agent_mental_model_t;

// Perspective types
typedef enum {
    PERSPECTIVE_SELF = 0,
    PERSPECTIVE_FIRST_ORDER,            // What X believes
    PERSPECTIVE_SECOND_ORDER,           // What X believes Y believes
    PERSPECTIVE_THIRD_ORDER,            // What X believes Y believes Z believes
    PERSPECTIVE_MAX_ORDER = 5           // Practical limit
} perspective_order_t;

// Theory of Mind API
theory_of_mind_t* theory_of_mind_create(const tom_config_t* config);
void theory_of_mind_destroy(theory_of_mind_t* tom);

// Agent modeling
int tom_create_agent_model(theory_of_mind_t* tom, uint32_t agent_id,
                           agent_mental_model_t* model_out);
int tom_update_belief(theory_of_mind_t* tom, uint32_t agent_id,
                      const float* observed_action, uint32_t action_dim);
int tom_predict_action(const theory_of_mind_t* tom, uint32_t agent_id,
                       float* predicted_action, uint32_t max_dim);

// Perspective taking
int tom_take_perspective(theory_of_mind_t* tom, uint32_t agent_id,
                         perspective_order_t order, float* perspective_out);
bool tom_false_belief_test(const theory_of_mind_t* tom, uint32_t agent_id,
                           const float* reality, const float* agent_belief);
```

#### 32.9 Ethics and Safety System

```c
// Ethics types (from nimcp_ethics.h)
typedef enum {
    ETHICS_SAFE = 0,                    // Action is safe
    ETHICS_CAUTION,                     // Proceed with caution
    ETHICS_WARN,                        // Warning - potential harm
    ETHICS_BLOCK,                       // Block - definite harm
    ETHICS_EMERGENCY_STOP               // Emergency stop
} ethics_verdict_t;

// Core directives
typedef enum {
    DIRECTIVE_NO_HARM = 0,              // Do not cause harm
    DIRECTIVE_HONESTY,                  // Be truthful
    DIRECTIVE_PRIVACY,                  // Protect privacy
    DIRECTIVE_CONSENT,                  // Respect consent
    DIRECTIVE_FAIRNESS,                 // Treat fairly
    DIRECTIVE_TRANSPARENCY,             // Be transparent
    DIRECTIVE_COUNT
} core_directive_t;

// Ethics engine API
ethics_engine_t* ethics_engine_create(const ethics_config_t* config);
void ethics_engine_destroy(ethics_engine_t* ethics);

// Action evaluation
ethics_verdict_t ethics_evaluate_action(const ethics_engine_t* ethics,
                                         const action_t* proposed_action);
int ethics_get_explanation(const ethics_engine_t* ethics,
                           const action_t* action, char* explanation, size_t max_len);

// Combinatorial harm detection
bool ethics_check_combinatorial_harm(const ethics_engine_t* ethics,
                                      const action_t* actions, uint32_t action_count);

// Harm prevention
int harm_prevention_init(harm_prevention_t* hp);
bool harm_prevention_check(const harm_prevention_t* hp, const action_t* action);
```

#### 32.10 Cognitive Module Bridge Summary

| Category | Bridges | Count |
|----------|---------|-------|
| **Attention Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic, Sleep, GPU | 7 |
| **Emotion Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic, Quantum | 6 |
| **Executive Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic, Sleep, Quantum, Middleware | 8 |
| **Reasoning Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic, Sleep, Quantum | 7 |
| **Memory Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic, Sleep, Quantum | 7 |
| **Curiosity Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic, Sleep, Quantum | 7 |
| **Theory of Mind Bridges** | SNN, Plasticity, FEP, Substrate | 4 |
| **Ethics Bridges** | SNN, Plasticity, FEP, Substrate, Thalamic | 5 |
| **Integration Bridges** | 23 cross-module bridges (Attention↔WM, Emotion↔Memory, etc.) | 23 |
| **Recursive Bridges** | FEP, Immune, Brain-KG, Imagination, Collective, Bio-Async | 6 |
| **Collective Bridges** | FEP, Plasticity, SNN, Immune, Hub | 5 |
| **Other Cognitive Bridges** | Salience, Epistemic, Wellbeing, Bias, Introspection, Self-Model | 30+ |
| **Total Cognitive Bridges** | | **~120** |

#### 32.11 Cognitive Module Integration with Brain Factory

```c
// File: src/core/brain/factory/init/nimcp_brain_init_cognitive.c

nimcp_status_t nimcp_brain_init_cognitive(
    nimcp_brain_t* brain,
    const nimcp_brain_config_t* config) {

    NIMCP_LOG_INFO(brain->logger, "Initializing cognitive systems");

    //=========================================================================
    // Phase 1: Meta-Controller (Central Coordinator)
    //=========================================================================

    cognitive_meta_controller_config_t mc_config = cognitive_meta_controller_default_config();
    mc_config.arbitration_policy = ARBITRATION_WEIGHTED_FUSION;
    brain->cognitive_meta_controller = cognitive_meta_controller_create(&mc_config);
    NIMCP_RETURN_IF_ERROR(brain->cognitive_meta_controller != NULL);

    // Register with bio-async router
    nimcp_bio_router_register(brain->router, NIMCP_MSG_TYPE_COGNITIVE,
                              cognitive_msg_handler, brain->cognitive_meta_controller);

    //=========================================================================
    // Phase 2: Global Workspace (Conscious Access)
    //=========================================================================

    global_workspace_config_t gw_config = global_workspace_default_config();
    gw_config.capacity_dim = config->global_workspace_dim;
    gw_config.ignition_threshold = 0.6f;
    brain->global_workspace = global_workspace_create(&gw_config);
    NIMCP_RETURN_IF_ERROR(brain->global_workspace != NULL);

    //=========================================================================
    // Phase 3: Cognitive Integration Hub
    //=========================================================================

    cognitive_hub_config_t hub_config = cognitive_hub_default_config();
    hub_config.max_modules = 64;
    hub_config.enable_async = true;
    brain->cognitive_hub = cognitive_hub_create(&hub_config);
    NIMCP_RETURN_IF_ERROR(brain->cognitive_hub != NULL);

    //=========================================================================
    // Phase 4: Core Cognitive Systems
    //=========================================================================

    // Working Memory (7±2 items, dlPFC)
    working_memory_config_t wm_config = working_memory_default_config();
    wm_config.capacity = config->working_memory_capacity;
    wm_config.enable_emotional_tagging = true;
    brain->working_memory = working_memory_create(&wm_config);

    // Executive Controller (Goal management, task switching)
    brain->executive = executive_controller_create(&config->executive_config);

    // Attention System (Salience-based focus)
    brain->attention = attention_system_create(&config->attention_config);

    // Reasoning Engine (Forward/backward chaining)
    brain->reasoning = reasoning_engine_create(&config->reasoning_config);

    // Emotional System (Valence, arousal, 8 basic emotions)
    brain->emotion = emotional_system_create(&config->emotion_config);

    //=========================================================================
    // Phase 5: Higher Cognitive Systems
    //=========================================================================

    // Curiosity (Exploration vs exploitation)
    brain->curiosity = curiosity_system_create(&config->curiosity_config);

    // Introspection (Self-monitoring)
    brain->introspection = introspection_system_create(&config->introspection_config);

    // Self-Model (Body schema, agency, temporal self)
    brain->self_model = self_model_create(&config->self_model_config);

    // Theory of Mind (Belief tracking, perspective taking)
    brain->theory_of_mind = theory_of_mind_create(&config->tom_config);

    // Meta-Learning (Learn-to-learn)
    brain->meta_learning = meta_learning_create(&config->meta_learning_config);

    //=========================================================================
    // Phase 6: Ethics and Safety
    //=========================================================================

    // Ethics Engine (Core directives, harm prevention)
    brain->ethics = ethics_engine_create(&config->ethics_config);

    // Connect ethics to executive for action gating
    nimcp_ethics_executive_bridge_init(&brain->ethics_executive_bridge,
                                        brain->ethics, brain->executive);

    //=========================================================================
    // Phase 7: Social Cognition
    //=========================================================================

    // Mirror Neurons (Action observation, empathy)
    brain->mirror_neurons = mirror_neuron_system_create(&config->mirror_config);

    // Imagination (Mental simulation, counterfactual)
    brain->imagination = imagination_system_create(&config->imagination_config);

    // Game Theory (Nash equilibrium, cooperation)
    brain->game_theory = game_theory_create(&config->game_theory_config);

    //=========================================================================
    // Phase 8: Recursive Cognition
    //=========================================================================

    rcog_config_t rcog_config = rcog_default_config();
    rcog_config.max_depth = 10;
    brain->rcog_orchestrator = rcog_orchestrator_create(&rcog_config);

    // Register recursive cognition tools
    rcog_register_tool(brain->rcog_orchestrator, &reasoning_tool);
    rcog_register_tool(brain->rcog_orchestrator, &memory_tool);
    rcog_register_tool(brain->rcog_orchestrator, &imagination_tool);

    //=========================================================================
    // Phase 9: Connect Meta-Controller to All Subsystems
    //=========================================================================

    cognitive_meta_controller_connect(brain->cognitive_meta_controller,
        brain->working_memory, brain->executive, brain->attention,
        brain->curiosity, brain->global_workspace, brain->emotion,
        brain->immune);

    //=========================================================================
    // Phase 10: Register All Modules with Cognitive Hub
    //=========================================================================

    cognitive_hub_register_module(brain->cognitive_hub, "working_memory",
        COG_CATEGORY_MEMORY, brain->working_memory, NULL);
    cognitive_hub_register_module(brain->cognitive_hub, "executive",
        COG_CATEGORY_EXECUTIVE, brain->executive, NULL);
    cognitive_hub_register_module(brain->cognitive_hub, "attention",
        COG_CATEGORY_ATTENTION, brain->attention, NULL);
    cognitive_hub_register_module(brain->cognitive_hub, "reasoning",
        COG_CATEGORY_REASONING, brain->reasoning, NULL);
    cognitive_hub_register_module(brain->cognitive_hub, "emotion",
        COG_CATEGORY_EMOTION, brain->emotion, NULL);
    cognitive_hub_register_module(brain->cognitive_hub, "theory_of_mind",
        COG_CATEGORY_SOCIAL, brain->theory_of_mind, NULL);
    cognitive_hub_register_module(brain->cognitive_hub, "introspection",
        COG_CATEGORY_METACOGNITION, brain->introspection, NULL);
    // ... register all modules

    //=========================================================================
    // Phase 11: Initialize All Bridges
    //=========================================================================

    // Attention bridges
    nimcp_attention_snn_bridge_init(&brain->attention->snn_bridge, brain->snn);
    nimcp_attention_plasticity_bridge_init(&brain->attention->plasticity_bridge, brain->plasticity);
    nimcp_attention_fep_bridge_init(&brain->attention->fep_bridge, brain->fep);

    // Emotion bridges
    nimcp_emotion_snn_bridge_init(&brain->emotion->snn_bridge, brain->snn);
    nimcp_emotion_plasticity_bridge_init(&brain->emotion->plasticity_bridge, brain->plasticity);

    // Integration bridges
    nimcp_attention_wm_bridge_init(&brain->attention_wm_bridge,
                                    brain->attention, brain->working_memory);
    nimcp_emotion_memory_bridge_init(&brain->emotion_memory_bridge,
                                      brain->emotion, brain->working_memory);
    nimcp_emotion_executive_bridge_init(&brain->emotion_executive_bridge,
                                         brain->emotion, brain->executive);
    nimcp_curiosity_reasoning_bridge_init(&brain->curiosity_reasoning_bridge,
                                           brain->curiosity, brain->reasoning);
    // ... initialize all 23 integration bridges

    //=========================================================================
    // Phase 12: Subscribe Modules to Global Workspace
    //=========================================================================

    global_workspace_subscribe(brain->global_workspace, MODULE_WORKING_MEMORY);
    global_workspace_subscribe(brain->global_workspace, MODULE_EXECUTIVE);
    global_workspace_subscribe(brain->global_workspace, MODULE_ATTENTION);
    global_workspace_subscribe(brain->global_workspace, MODULE_REASONING);
    global_workspace_subscribe(brain->global_workspace, MODULE_EMOTION);
    global_workspace_subscribe(brain->global_workspace, MODULE_CURIOSITY);
    global_workspace_subscribe(brain->global_workspace, MODULE_THEORY_OF_MIND);
    global_workspace_subscribe(brain->global_workspace, MODULE_INTROSPECTION);
    global_workspace_subscribe(brain->global_workspace, MODULE_ETHICS);

    NIMCP_LOG_INFO(brain->logger, "Cognitive systems initialized: %d modules, "
        "%d bridges, GW capacity %d",
        MODULE_COUNT, 120, config->global_workspace_dim);

    return NIMCP_OK;
}
```

#### 32.12 Cognitive Module Performance Requirements

| Operation | Complexity | Typical Time |
|-----------|------------|--------------|
| WM add item | O(N) N=7 | 10 μs |
| WM decay step | O(N) | 5 μs |
| GW compete | O(S) subscribers | 50 μs |
| GW broadcast | O(S) | 100 μs |
| Hub event publish | O(S) subscribers | 20 μs |
| Meta-controller arbitrate | O(M) modules | 100 μs |
| ToM perspective (1st order) | O(D) dim | 1 ms |
| ToM perspective (2nd order) | O(D²) | 10 ms |
| Ethics evaluate | O(A) actions | 100 μs |
| Recursive query | O(depth × tool) | 10-100 ms |

#### 32.13 Cognitive Module Integration Tests

| Test File | Test Count |
|-----------|------------|
| `test/unit/cognitive/meta_controller/test_meta_controller.cpp` | 30 |
| `test/unit/cognitive/meta_controller/test_arbitration.cpp` | 25 |
| `test/unit/cognitive/meta_controller/test_metacognition.cpp` | 20 |
| `test/unit/cognitive/global_workspace/test_gw_create.cpp` | 15 |
| `test/unit/cognitive/global_workspace/test_gw_competition.cpp` | 25 |
| `test/unit/cognitive/global_workspace/test_gw_broadcast.cpp` | 20 |
| `test/unit/cognitive/global_workspace/test_gw_snn_bridge.cpp` | 15 |
| `test/unit/cognitive/global_workspace/test_gw_plasticity_bridge.cpp` | 15 |
| `test/unit/cognitive/integration/test_cognitive_hub.cpp` | 30 |
| `test/unit/cognitive/integration/test_event_types.cpp` | 15 |
| `test/unit/cognitive/integration/test_cross_module_bridges.cpp` | 40 |
| `test/unit/cognitive/working_memory/test_wm_create.cpp` | 15 |
| `test/unit/cognitive/working_memory/test_wm_add_remove.cpp` | 20 |
| `test/unit/cognitive/working_memory/test_wm_decay.cpp` | 15 |
| `test/unit/cognitive/working_memory/test_wm_eviction.cpp` | 15 |
| `test/unit/cognitive/working_memory/test_wm_snn_bridge.cpp` | 15 |
| `test/unit/cognitive/working_memory/test_wm_plasticity_bridge.cpp` | 15 |
| `test/unit/cognitive/executive/test_executive_controller.cpp` | 25 |
| `test/unit/cognitive/executive/test_goal_management.cpp` | 20 |
| `test/unit/cognitive/executive/test_task_switching.cpp` | 15 |
| `test/unit/cognitive/executive/test_executive_bridges.cpp` | 20 |
| `test/unit/cognitive/attention/test_attention_system.cpp` | 25 |
| `test/unit/cognitive/attention/test_salience.cpp` | 15 |
| `test/unit/cognitive/attention/test_attention_bridges.cpp` | 25 |
| `test/unit/cognitive/reasoning/test_reasoning_engine.cpp` | 25 |
| `test/unit/cognitive/reasoning/test_forward_chaining.cpp` | 15 |
| `test/unit/cognitive/reasoning/test_backward_chaining.cpp` | 15 |
| `test/unit/cognitive/reasoning/test_reasoning_bridges.cpp` | 20 |
| `test/unit/cognitive/emotion/test_emotional_system.cpp` | 25 |
| `test/unit/cognitive/emotion/test_emotion_tensor.cpp` | 20 |
| `test/unit/cognitive/emotion/test_emotional_tagging.cpp` | 15 |
| `test/unit/cognitive/emotion/test_emotion_bridges.cpp` | 25 |
| `test/unit/cognitive/curiosity/test_curiosity_system.cpp` | 20 |
| `test/unit/cognitive/curiosity/test_exploration.cpp` | 15 |
| `test/unit/cognitive/curiosity/test_curiosity_bridges.cpp` | 15 |
| `test/unit/cognitive/introspection/test_introspection.cpp` | 20 |
| `test/unit/cognitive/introspection/test_self_monitoring.cpp` | 15 |
| `test/unit/cognitive/self_model/test_self_model.cpp` | 20 |
| `test/unit/cognitive/self_model/test_body_schema.cpp` | 10 |
| `test/unit/cognitive/theory_of_mind/test_tom_create.cpp` | 15 |
| `test/unit/cognitive/theory_of_mind/test_belief_tracking.cpp` | 20 |
| `test/unit/cognitive/theory_of_mind/test_perspective_taking.cpp` | 20 |
| `test/unit/cognitive/theory_of_mind/test_false_belief.cpp` | 10 |
| `test/unit/cognitive/theory_of_mind/test_tom_bridges.cpp` | 15 |
| `test/unit/cognitive/ethics/test_ethics_engine.cpp` | 25 |
| `test/unit/cognitive/ethics/test_harm_prevention.cpp` | 20 |
| `test/unit/cognitive/ethics/test_core_directives.cpp` | 15 |
| `test/unit/cognitive/ethics/test_combinatorial_harm.cpp` | 10 |
| `test/unit/cognitive/ethics/test_ethics_bridges.cpp` | 15 |
| `test/unit/cognitive/recursive/test_rcog_orchestrator.cpp` | 25 |
| `test/unit/cognitive/recursive/test_rcog_engine.cpp` | 20 |
| `test/unit/cognitive/recursive/test_rcog_tool_router.cpp` | 15 |
| `test/unit/cognitive/recursive/test_rcog_delegation.cpp` | 15 |
| `test/unit/cognitive/recursive/test_rcog_bridges.cpp` | 20 |
| `test/unit/cognitive/mirror_neurons/test_mirror_system.cpp` | 20 |
| `test/unit/cognitive/mirror_neurons/test_mirror_bridges.cpp` | 15 |
| `test/unit/cognitive/imagination/test_imagination.cpp` | 20 |
| `test/unit/cognitive/imagination/test_counterfactual.cpp` | 15 |
| `test/unit/cognitive/game_theory/test_game_theory.cpp` | 20 |
| `test/unit/cognitive/game_theory/test_nash_equilibrium.cpp` | 15 |
| `test/integration/cognitive/test_cognitive_pipeline.cpp` | 30 |
| `test/integration/cognitive/test_gw_integration.cpp` | 20 |
| `test/integration/cognitive/test_meta_controller_integration.cpp` | 20 |
| `test/integration/cognitive/test_cross_bridge_integration.cpp` | 25 |
| `test/regression/cognitive/test_cognitive_regression.cpp` | 20 |
| `test/regression/cognitive/test_gw_regression.cpp` | 10 |
| `test/e2e/e2e_test_cognitive_pipeline.cpp` | 25 |
| `test/e2e/e2e_test_recursive_cognition.cpp` | 15 |
| **Total** | **1,000** |

### 33. Updated Scope Summary

| Category | Items | Est. LOC | Tests |
|----------|-------|----------|-------|
| Advanced Concepts (Incomplete) | 9 concepts | ~45,000 | ~900 |
| Brain Regions (New) | 17 regions | ~85,000 | ~1,700 |
| Extrapolation (Remaining 15%) | 3 modules | ~4,500 | ~90 |
| Embodiment (Remaining 40%) | 4 modules | ~2,800 | ~56 |
| Superhuman (Remaining 50%) | 7 modules | ~15,000 | ~300 |
| **Intra-Layer Integration** | **9 layers** | **~18,000** | **~295** |
| **Inter-Layer Integration** | **13 connections** | **~13,000** | **~300** |
| **Layer Coordinator** | **1 system** | **~3,000** | **~50** |
| **Cognitive Hub Integration** | **40 modules** | **~8,000** | **~160** |
| **Omnidirectional Integration** | **40 modules** | **~12,000** | **~240** |
| **Hypothalamus Integration** | **40 modules** | **~8,000** | **~105** |
| **Thalamic/Middleware Integration** | **40 modules** | **~16,000** | **~140** |
| **Neural Substrate Integration** | **40 modules** | **~12,000** | **~115** |
| **Motor Cortex Integration** | **40 modules** | **~15,000** | **~150** |
| **Portia Integration** | **40 modules** | **~14,000** | **~125** |
| **Swarm Integration** | **40 modules** | **~18,000** | **~150** |
| **Dragonfly Integration** | **40 modules** | **~16,000** | **~130** |
| **Sleep Integration** | **40 modules** | **~20,000** | **~170** |
| **Glial Integration** | **40 modules** | **~22,000** | **~190** |
| **Quantum Integration** | **40 modules** | **~24,000** | **~210** |
| **Training Integration** | **40 modules** | **~26,000** | **~230** |
| **Language Integration** | **40 modules** | **~28,000** | **~250** |
| **Perception Integration** | **40 modules** | **~30,000** | **~270** |
| **GPU Module Integration** | **70 headers** | **~35,000** | **~300** |
| **Information Theory Integration** | **4 headers** | **~18,000** | **~160** |
| **Plasticity Module Integration** | **100+ headers** | **~40,000** | **~350** |
| **Security Module Integration** | **44 headers** | **~45,000** | **~400** |
| **Async Module Integration** | **18 headers** | **~50,000** | **~350** |
| **Utils Module Integration** | **122 headers** | **~70,000** | **~500** |
| **Core Module Integration** | **323 headers** | **~100,000** | **~700** |
| **Cognitive Module Integration** | **575 headers** | **~150,000** | **~1,000** |
| **TOTAL** | **~40 modules + full integration** | **~964,300** | **~10,086** |

---

## PART I: ADVANCED CONCEPTS IMPLEMENTATION

### Phase AC-1: Electromagnetic Fields (Ephaptic Coupling)

**Priority**: Medium
**Dependencies**: SNN system, Attention system
**Est. LOC**: 5,000 | **Tests**: 100

#### 1.1 Core Module

**Files**:
- `include/physics/ephaptic/nimcp_ephaptic.h`
- `include/physics/ephaptic/nimcp_local_field_potential.h`
- `include/physics/ephaptic/nimcp_field_synchronization.h`
- `src/physics/ephaptic/nimcp_ephaptic.c`
- `src/physics/ephaptic/nimcp_local_field_potential.c`
- `src/physics/ephaptic/nimcp_field_synchronization.c`

**API**:
```c
typedef struct {
    float field_strength[3];         // V/m (x, y, z)
    float field_potential;           // mV
    float magnetic_field[3];         // Tesla
    float field_induced_current;     // pA
    float membrane_polarization;
    float phase_locking_strength;    // 0-1
    uint32_t synchronized_neurons;

    // Bio-async integration
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune integration
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_ephaptic_system_t;

nimcp_status_t nimcp_ephaptic_init(nimcp_ephaptic_system_t* sys,
                                    nimcp_brain_t* brain);
nimcp_status_t nimcp_ephaptic_compute_lfp(nimcp_ephaptic_system_t* sys,
                                           nimcp_snn_population_t* population);
nimcp_status_t nimcp_ephaptic_synchronize(nimcp_ephaptic_system_t* sys,
                                           float target_phase);
```

#### 1.2 Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| SNN Bridge | `nimcp_ephaptic_snn_bridge.h/c` | Population field effects |
| Plasticity Bridge | `nimcp_ephaptic_plasticity_bridge.h/c` | Field-dependent LTP/LTD |
| FEP Bridge | `nimcp_ephaptic_fep_bridge.h/c` | Precision weighting via fields |
| Substrate Bridge | `nimcp_ephaptic_substrate_bridge.h/c` | Bio-async messaging |
| Thalamic Bridge | `nimcp_ephaptic_thalamic_bridge.h/c` | Attention gating |
| Immune Bridge | `nimcp_ephaptic_immune_bridge.h/c` | Abnormal field detection |
| Hub Bridge | `nimcp_ephaptic_hub_bridge.h/c` | Cognitive hub integration |

#### 1.3 Brain Factory Integration

**File**: `src/core/brain/factory/init/nimcp_brain_init_ephaptic.c`

```c
nimcp_status_t nimcp_brain_init_ephaptic(nimcp_brain_t* brain,
                                          const nimcp_brain_config_t* config) {
    NIMCP_LOG_INFO("Initializing ephaptic coupling system");

    // Allocate from brain's memory pool
    brain->ephaptic = nimcp_pool_alloc(brain->pool, sizeof(nimcp_ephaptic_system_t));

    // Initialize core system
    nimcp_ephaptic_init(brain->ephaptic, brain);

    // Register with bio-async router
    nimcp_bio_router_register(brain->router,
                              NIMCP_MSG_TYPE_EPHAPTIC,
                              ephaptic_msg_handler,
                              brain->ephaptic);

    // Register with immune system
    nimcp_immune_register_sensor(brain->immune,
                                  &brain->ephaptic->immune_sensor,
                                  IMMUNE_SENSOR_EPHAPTIC);

    // Initialize all bridges
    nimcp_ephaptic_snn_bridge_init(&brain->ephaptic->snn_bridge, brain->snn);
    nimcp_ephaptic_plasticity_bridge_init(&brain->ephaptic->plasticity_bridge, brain->plasticity);
    nimcp_ephaptic_fep_bridge_init(&brain->ephaptic->fep_bridge, brain->fep);
    nimcp_ephaptic_substrate_bridge_init(&brain->ephaptic->substrate_bridge, brain->router);
    nimcp_ephaptic_thalamic_bridge_init(&brain->ephaptic->thalamic_bridge, brain->thalamus);
    nimcp_ephaptic_immune_bridge_init(&brain->ephaptic->immune_bridge, brain->immune);
    nimcp_ephaptic_hub_bridge_init(&brain->ephaptic->hub_bridge, brain->cognitive_hub);

    NIMCP_LOG_INFO("Ephaptic system initialized with %d bridges", 7);
    return NIMCP_OK;
}
```

#### 1.4 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/physics/ephaptic/test_ephaptic_core.cpp` | 20 |
| Unit | `test/unit/physics/ephaptic/test_lfp.cpp` | 15 |
| Unit | `test/unit/physics/ephaptic/test_synchronization.cpp` | 15 |
| Unit | `test/unit/physics/ephaptic/test_ephaptic_snn_bridge.cpp` | 10 |
| Unit | `test/unit/physics/ephaptic/test_ephaptic_plasticity_bridge.cpp` | 10 |
| Unit | `test/unit/physics/ephaptic/test_ephaptic_fep_bridge.cpp` | 10 |
| Integration | `test/integration/physics/ephaptic/test_ephaptic_snn_integration.cpp` | 10 |
| Integration | `test/integration/physics/ephaptic/test_ephaptic_immune_integration.cpp` | 5 |
| Regression | `test/regression/physics/ephaptic/test_ephaptic_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_ephaptic_pipeline.cpp` | 5 |

---

### Phase AC-2: Information Geometry

**Priority**: Medium
**Dependencies**: Learning systems, Plasticity
**Est. LOC**: 5,500 | **Tests**: 110

#### 2.1 Core Module

**Files**:
- `include/physics/geometry/nimcp_information_geometry.h`
- `include/physics/geometry/nimcp_fisher_information.h`
- `include/physics/geometry/nimcp_natural_gradient.h`
- `include/physics/geometry/nimcp_neural_manifold.h`
- `src/physics/geometry/nimcp_information_geometry.c`
- `src/physics/geometry/nimcp_fisher_information.c`
- `src/physics/geometry/nimcp_natural_gradient.c`
- `src/physics/geometry/nimcp_neural_manifold.c`

**API**:
```c
typedef struct {
    float embedding[LATENT_DIM];
    float tangent_vectors[LATENT_DIM][AMBIENT_DIM];
    float fisher_matrix[LATENT_DIM][LATENT_DIM];
    float fisher_matrix_inverse[LATENT_DIM][LATENT_DIM];
    float ricci_curvature;
    float natural_gradient[LATENT_DIM];

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_info_geometry_t;

nimcp_status_t nimcp_info_geometry_init(nimcp_info_geometry_t* geom,
                                         nimcp_brain_t* brain);
nimcp_status_t nimcp_info_geometry_compute_fisher(nimcp_info_geometry_t* geom,
                                                   const float* distribution);
nimcp_status_t nimcp_info_geometry_natural_gradient(nimcp_info_geometry_t* geom,
                                                     const float* gradient,
                                                     float* natural_grad);
nimcp_status_t nimcp_info_geometry_update(nimcp_info_geometry_t* geom,
                                           float learning_rate);
```

#### 2.2 Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| SNN Bridge | `nimcp_info_geometry_snn_bridge.h/c` | Neural code geometry |
| Plasticity Bridge | `nimcp_info_geometry_plasticity_bridge.h/c` | Natural gradient learning |
| FEP Bridge | `nimcp_info_geometry_fep_bridge.h/c` | Belief geometry |
| Substrate Bridge | `nimcp_info_geometry_substrate_bridge.h/c` | Bio-async messaging |
| Thalamic Bridge | `nimcp_info_geometry_thalamic_bridge.h/c` | Attention manifolds |
| Immune Bridge | `nimcp_info_geometry_immune_bridge.h/c` | Anomaly detection |
| Hub Bridge | `nimcp_info_geometry_hub_bridge.h/c` | Cognitive integration |

#### 2.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/physics/geometry/test_fisher_information.cpp` | 20 |
| Unit | `test/unit/physics/geometry/test_natural_gradient.cpp` | 20 |
| Unit | `test/unit/physics/geometry/test_neural_manifold.cpp` | 20 |
| Unit | `test/unit/physics/geometry/test_info_geometry_bridges.cpp` | 20 |
| Integration | `test/integration/physics/geometry/test_info_geometry_plasticity_integration.cpp` | 15 |
| Regression | `test/regression/physics/geometry/test_info_geometry_regression.cpp` | 10 |
| E2E | `test/e2e/e2e_test_info_geometry_pipeline.cpp` | 5 |

---

### Phase AC-3: Hodgkin-Huxley Dynamics

**Priority**: High (biophysical realism)
**Dependencies**: Neuron models, Ion channels
**Est. LOC**: 6,000 | **Tests**: 120

#### 3.1 Core Module

**Files**:
- `include/physics/biophysics/nimcp_hodgkin_huxley.h`
- `include/physics/biophysics/nimcp_ion_channels.h`
- `include/physics/biophysics/nimcp_gating_variables.h`
- `include/physics/biophysics/nimcp_action_potential.h`
- `src/physics/biophysics/nimcp_hodgkin_huxley.c`
- `src/physics/biophysics/nimcp_ion_channels.c`
- `src/physics/biophysics/nimcp_gating_variables.c`
- `src/physics/biophysics/nimcp_action_potential.c`

**API**:
```c
typedef struct {
    // Membrane potential
    float V;  // mV

    // Gating variables (0-1)
    float m;  // Sodium activation
    float h;  // Sodium inactivation
    float n;  // Potassium activation

    // Conductances (mS/cm²)
    float g_Na;  // Sodium (default: 120)
    float g_K;   // Potassium (default: 36)
    float g_L;   // Leak (default: 0.3)

    // Reversal potentials (mV)
    float E_Na;  // +55 mV
    float E_K;   // -77 mV
    float E_L;   // -54.3 mV

    // Membrane capacitance
    float C_m;   // µF/cm² (default: 1.0)

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_hh_neuron_t;

nimcp_status_t nimcp_hh_neuron_init(nimcp_hh_neuron_t* neuron,
                                     nimcp_brain_t* brain);
nimcp_status_t nimcp_hh_neuron_update(nimcp_hh_neuron_t* neuron,
                                       float I_ext, float dt);
nimcp_status_t nimcp_hh_neuron_get_spike(nimcp_hh_neuron_t* neuron,
                                          bool* spiked);
```

#### 3.2 Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| SNN Bridge | `nimcp_hh_snn_bridge.h/c` | Replace simple neurons |
| Plasticity Bridge | `nimcp_hh_plasticity_bridge.h/c` | Channel-dependent plasticity |
| FEP Bridge | `nimcp_hh_fep_bridge.h/c` | Precision from ion dynamics |
| Substrate Bridge | `nimcp_hh_substrate_bridge.h/c` | Bio-async messaging |
| Thalamic Bridge | `nimcp_hh_thalamic_bridge.h/c` | Gating modulation |
| Immune Bridge | `nimcp_hh_immune_bridge.h/c` | Channelopathy detection |
| Hub Bridge | `nimcp_hh_hub_bridge.h/c` | Cognitive integration |

#### 3.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/physics/biophysics/test_hodgkin_huxley.cpp` | 25 |
| Unit | `test/unit/physics/biophysics/test_ion_channels.cpp` | 25 |
| Unit | `test/unit/physics/biophysics/test_gating_variables.cpp` | 20 |
| Unit | `test/unit/physics/biophysics/test_action_potential.cpp` | 20 |
| Unit | `test/unit/physics/biophysics/test_hh_bridges.cpp` | 15 |
| Integration | `test/integration/physics/biophysics/test_hh_snn_integration.cpp` | 10 |
| Regression | `test/regression/physics/biophysics/test_hh_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_hh_pipeline.cpp` | 5 |

---

### Phase AC-4: pH Dynamics

**Priority**: Low
**Dependencies**: Metabolic systems
**Est. LOC**: 3,000 | **Tests**: 60

#### 4.1 Core Module

**Files**:
- `include/chemistry/ph/nimcp_ph_dynamics.h`
- `include/chemistry/ph/nimcp_proton_pumps.h`
- `include/chemistry/ph/nimcp_buffer_systems.h`
- `src/chemistry/ph/nimcp_ph_dynamics.c`
- `src/chemistry/ph/nimcp_proton_pumps.c`
- `src/chemistry/ph/nimcp_buffer_systems.c`

**API**:
```c
typedef struct {
    float extracellular_pH;        // Typically 7.4
    float intracellular_pH;        // Typically 7.2
    float synaptic_vesicle_pH;     // Typically 5.5

    // Proton pumps
    float h_atpase_activity;       // V-ATPase in vesicles
    float na_h_exchanger_activity; // NHE at membrane

    // Buffering capacity
    float bicarbonate_buffer;      // HCO3- system
    float protein_buffer;

    // Effects
    float ph_conductance_modifier;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
} nimcp_ph_system_t;
```

#### 4.2 Bridges

Standard 7-bridge pattern (SNN, Plasticity, FEP, Substrate, Thalamic, Immune, Hub)

#### 4.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/chemistry/ph/test_ph_dynamics.cpp` | 20 |
| Unit | `test/unit/chemistry/ph/test_proton_pumps.cpp` | 15 |
| Unit | `test/unit/chemistry/ph/test_buffer_systems.cpp` | 10 |
| Unit | `test/unit/chemistry/ph/test_ph_bridges.cpp` | 10 |
| Integration | `test/integration/chemistry/ph/test_ph_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_ph_pipeline.cpp` | 5 |

---

### Phase AC-5: Nitric Oxide Signaling

**Priority**: Medium (important for LTP)
**Dependencies**: Calcium dynamics, Plasticity
**Est. LOC**: 4,000 | **Tests**: 80

#### 5.1 Core Module

**Files**:
- `include/chemistry/gasotransmitters/nimcp_nitric_oxide.h`
- `include/chemistry/gasotransmitters/nimcp_nos_system.h`
- `include/chemistry/gasotransmitters/nimcp_cgmp_pathway.h`
- `include/chemistry/gasotransmitters/nimcp_retrograde_signaling.h`
- `src/chemistry/gasotransmitters/nimcp_nitric_oxide.c`
- `src/chemistry/gasotransmitters/nimcp_nos_system.c`
- `src/chemistry/gasotransmitters/nimcp_cgmp_pathway.c`
- `src/chemistry/gasotransmitters/nimcp_retrograde_signaling.c`

**API**:
```c
typedef struct {
    // NO production
    float nos_activity;            // Nitric oxide synthase
    float no_concentration;        // nM
    float no_diffusion_radius;     // µm

    // Effects
    float cgmp_concentration;      // Cyclic GMP
    float vasodilation_factor;     // Blood flow increase

    // Retrograde plasticity
    float presynaptic_potentiation;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_no_system_t;
```

#### 5.2 Bridges

Standard 7-bridge pattern

#### 5.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/chemistry/gasotransmitters/test_nitric_oxide.cpp` | 20 |
| Unit | `test/unit/chemistry/gasotransmitters/test_nos_system.cpp` | 15 |
| Unit | `test/unit/chemistry/gasotransmitters/test_cgmp_pathway.cpp` | 15 |
| Unit | `test/unit/chemistry/gasotransmitters/test_retrograde.cpp` | 10 |
| Unit | `test/unit/chemistry/gasotransmitters/test_no_bridges.cpp` | 10 |
| Integration | `test/integration/chemistry/gasotransmitters/test_no_plasticity_integration.cpp` | 5 |
| Regression | `test/regression/chemistry/gasotransmitters/test_no_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_no_pipeline.cpp` | 5 |

---

### Phase AC-6: Epigenetics

**Priority**: Medium (trauma, addiction modeling)
**Dependencies**: Gene expression, Second messengers
**Est. LOC**: 5,000 | **Tests**: 100

#### 6.1 Core Module

**Files**:
- `include/biology/epigenetics/nimcp_epigenetics.h`
- `include/biology/epigenetics/nimcp_dna_methylation.h`
- `include/biology/epigenetics/nimcp_histone_modification.h`
- `include/biology/epigenetics/nimcp_chromatin_state.h`
- `src/biology/epigenetics/nimcp_epigenetics.c`
- `src/biology/epigenetics/nimcp_dna_methylation.c`
- `src/biology/epigenetics/nimcp_histone_modification.c`
- `src/biology/epigenetics/nimcp_chromatin_state.c`

**API**:
```c
typedef struct {
    // DNA methylation
    float promoter_methylation[NUM_GENES];  // 0-1

    // Histone modifications
    float histone_acetylation[NUM_GENES];   // Open chromatin
    float histone_methylation[NUM_GENES];   // Closed chromatin

    // Enzymes
    float dnmt_activity;           // DNA methyltransferase
    float hdac_activity;           // Histone deacetylase
    float hat_activity;            // Histone acetyltransferase

    // Gene accessibility
    float gene_expression_potential[NUM_GENES];

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_epigenetic_system_t;
```

#### 6.2 Bridges

Standard 7-bridge pattern

#### 6.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/biology/epigenetics/test_epigenetics.cpp` | 25 |
| Unit | `test/unit/biology/epigenetics/test_dna_methylation.cpp` | 20 |
| Unit | `test/unit/biology/epigenetics/test_histone_modification.cpp` | 20 |
| Unit | `test/unit/biology/epigenetics/test_chromatin_state.cpp` | 15 |
| Unit | `test/unit/biology/epigenetics/test_epigenetics_bridges.cpp` | 10 |
| Integration | `test/integration/biology/epigenetics/test_epigenetics_stress_integration.cpp` | 5 |
| Regression | `test/regression/biology/epigenetics/test_epigenetics_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_epigenetics_pipeline.cpp` | 5 |

---

### Phase AC-7: Neurogenesis

**Priority**: Low (pattern separation, depression)
**Dependencies**: Hippocampus, BDNF
**Est. LOC**: 4,500 | **Tests**: 90

#### 7.1 Core Module

**Files**:
- `include/biology/neurogenesis/nimcp_neurogenesis.h`
- `include/biology/neurogenesis/nimcp_neural_stem_cells.h`
- `include/biology/neurogenesis/nimcp_neuron_maturation.h`
- `include/biology/neurogenesis/nimcp_neuron_integration.h`
- `src/biology/neurogenesis/nimcp_neurogenesis.c`
- `src/biology/neurogenesis/nimcp_neural_stem_cells.c`
- `src/biology/neurogenesis/nimcp_neuron_maturation.c`
- `src/biology/neurogenesis/nimcp_neuron_integration.c`

**API**:
```c
typedef struct {
    // Neural stem cells
    uint32_t num_stem_cells;
    float proliferation_rate;      // Cells/day

    // Newborn neurons
    uint32_t num_newborn_neurons;
    float* neuron_ages;            // Days since birth
    float* integration_progress;   // 0-1

    // Survival factors
    float bdnf_level;
    float running_activity;        // Exercise
    float learning_activity;       // Cognitive stimulation

    // Survival rate
    float survival_probability;    // ~50% die in first month

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_neurogenesis_t;
```

#### 7.2 Bridges

Standard 7-bridge pattern

#### 7.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/biology/neurogenesis/test_neurogenesis.cpp` | 25 |
| Unit | `test/unit/biology/neurogenesis/test_neural_stem_cells.cpp` | 20 |
| Unit | `test/unit/biology/neurogenesis/test_neuron_maturation.cpp` | 15 |
| Unit | `test/unit/biology/neurogenesis/test_neuron_integration.cpp` | 15 |
| Unit | `test/unit/biology/neurogenesis/test_neurogenesis_bridges.cpp` | 10 |
| Integration | `test/integration/biology/neurogenesis/test_neurogenesis_hippocampus_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_neurogenesis_pipeline.cpp` | 5 |

---

### Phase AC-8: Neurovascular Coupling

**Priority**: Low (fMRI validation)
**Dependencies**: Astrocytes, NO signaling
**Est. LOC**: 5,000 | **Tests**: 100

#### 8.1 Core Module

**Files**:
- `include/biology/neurovascular/nimcp_neurovascular.h`
- `include/biology/neurovascular/nimcp_blood_flow.h`
- `include/biology/neurovascular/nimcp_bold_signal.h`
- `include/biology/neurovascular/nimcp_hemodynamic_response.h`
- `src/biology/neurovascular/nimcp_neurovascular.c`
- `src/biology/neurovascular/nimcp_blood_flow.c`
- `src/biology/neurovascular/nimcp_bold_signal.c`
- `src/biology/neurovascular/nimcp_hemodynamic_response.c`

**API**:
```c
typedef struct {
    // Vascular state
    float vessel_diameter;         // µm
    float blood_flow_rate;         // mL/min/100g
    float blood_oxygen_level;      // %

    // BOLD signal components
    float deoxyhemoglobin;         // Paramagnetic
    float cerebral_blood_volume;   // CBV
    float cerebral_blood_flow;     // CBF
    float oxygen_extraction;       // OEF

    // Coupling mechanisms
    float astrocyte_calcium;       // Links to astrocytes
    float nitric_oxide;            // Vasodilator
    float prostaglandins;          // Vasodilator

    // BOLD response
    float bold_signal;             // % change
    float hemodynamic_response[20]; // HRF kernel

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_neurovascular_t;
```

#### 8.2 Bridges

Standard 7-bridge pattern

#### 8.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/biology/neurovascular/test_neurovascular.cpp` | 25 |
| Unit | `test/unit/biology/neurovascular/test_blood_flow.cpp` | 20 |
| Unit | `test/unit/biology/neurovascular/test_bold_signal.cpp` | 20 |
| Unit | `test/unit/biology/neurovascular/test_hemodynamic_response.cpp` | 15 |
| Unit | `test/unit/biology/neurovascular/test_neurovascular_bridges.cpp` | 10 |
| Integration | `test/integration/biology/neurovascular/test_neurovascular_astrocyte_integration.cpp` | 5 |
| Regression | `test/regression/biology/neurovascular/test_neurovascular_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_neurovascular_pipeline.cpp` | 5 |

---

### Phase AC-9: Non-Equilibrium Thermodynamics (Complete)

**Priority**: Medium
**Dependencies**: FEP, Metabolic systems
**Est. LOC**: 4,000 | **Tests**: 80

#### 9.1 Core Module

**Files**:
- `include/physics/thermodynamics/nimcp_thermodynamics.h`
- `include/physics/thermodynamics/nimcp_energy_budget.h`
- `include/physics/thermodynamics/nimcp_entropy_production.h`
- `include/physics/thermodynamics/nimcp_landauer_cost.h`
- `src/physics/thermodynamics/nimcp_thermodynamics.c`
- `src/physics/thermodynamics/nimcp_energy_budget.c`
- `src/physics/thermodynamics/nimcp_entropy_production.c`
- `src/physics/thermodynamics/nimcp_landauer_cost.c`

**API**:
```c
typedef struct {
    // Energy tracking
    float total_energy_consumed;   // Joules
    float power_consumption;       // Watts
    float heat_dissipation;        // W/m³

    // Entropy production
    float entropy_production_rate; // J/(K·s)
    float free_energy_dissipated;

    // Metabolic constraints
    float atp_available;           // Mol
    float atp_consumption_rate;    // Mol/s
    float oxygen_delivery_rate;    // Mol/s

    // Efficiency metrics
    float computational_efficiency; // Ops/Joule
    float thermodynamic_efficiency; // Useful work / total energy

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_thermodynamic_state_t;
```

#### 9.2 Bridges

Standard 7-bridge pattern

#### 9.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/physics/thermodynamics/test_thermodynamics.cpp` | 20 |
| Unit | `test/unit/physics/thermodynamics/test_energy_budget.cpp` | 20 |
| Unit | `test/unit/physics/thermodynamics/test_entropy_production.cpp` | 15 |
| Unit | `test/unit/physics/thermodynamics/test_landauer_cost.cpp` | 10 |
| Unit | `test/unit/physics/thermodynamics/test_thermodynamics_bridges.cpp` | 10 |
| Integration | `test/integration/physics/thermodynamics/test_thermodynamics_fep_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_thermodynamics_pipeline.cpp` | 5 |

---

## PART II: NEW BRAIN REGIONS IMPLEMENTATION

### Phase BR-1: Locus Coeruleus (Norepinephrine Center)

**Priority**: P1 - Critical for arousal/exploration
**Dependencies**: Neuromodulators, Attention
**Est. LOC**: 5,000 | **Tests**: 100

#### 1.1 Core Module

**Files**:
- `include/core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.h`
- `include/core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h`
- `include/core/brain/regions/locus_coeruleus/nimcp_norepinephrine_release.h`
- `include/core/brain/regions/locus_coeruleus/nimcp_arousal_modulation.h`
- `include/core/brain/regions/locus_coeruleus/nimcp_novelty_detection.h`
- `src/core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.c`
- `src/core/brain/regions/locus_coeruleus/nimcp_lc_adapter.c`
- `src/core/brain/regions/locus_coeruleus/nimcp_norepinephrine_release.c`
- `src/core/brain/regions/locus_coeruleus/nimcp_arousal_modulation.c`
- `src/core/brain/regions/locus_coeruleus/nimcp_novelty_detection.c`

**API**:
```c
typedef struct {
    // NE release parameters
    float tonic_firing_rate;       // Hz (baseline ~1-3 Hz)
    float phasic_firing_rate;      // Hz (burst up to 20 Hz)
    float ne_concentration;        // nM
    float ne_release_rate;         // nM/s

    // Global arousal
    float arousal_level;           // 0-1
    float alertness;               // 0-1
    float vigilance;               // 0-1

    // Novelty/surprise detection
    float novelty_signal;          // 0-1
    float surprise_magnitude;      // 0-1
    float exploration_drive;       // 0-1

    // Mode switching
    bool phasic_mode;              // true = focused, false = exploratory
    float mode_switch_threshold;

    // Projections (volume transmission)
    nimcp_projection_t cortical_projections;
    nimcp_projection_t hippocampal_projections;
    nimcp_projection_t amygdala_projections;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_locus_coeruleus_t;

nimcp_status_t nimcp_lc_init(nimcp_locus_coeruleus_t* lc, nimcp_brain_t* brain);
nimcp_status_t nimcp_lc_update(nimcp_locus_coeruleus_t* lc, float dt);
nimcp_status_t nimcp_lc_detect_novelty(nimcp_locus_coeruleus_t* lc,
                                        const float* sensory_input,
                                        float* novelty_score);
nimcp_status_t nimcp_lc_modulate_arousal(nimcp_locus_coeruleus_t* lc,
                                          float target_arousal);
nimcp_status_t nimcp_lc_trigger_attention_reset(nimcp_locus_coeruleus_t* lc);
```

#### 1.2 Bridges

| Bridge | File | Purpose |
|--------|------|---------|
| SNN Bridge | `nimcp_lc_snn_bridge.h/c` | NE modulation of SNN |
| Plasticity Bridge | `nimcp_lc_plasticity_bridge.h/c` | NE-dependent plasticity |
| FEP Bridge | `nimcp_lc_fep_bridge.h/c` | Precision modulation |
| Substrate Bridge | `nimcp_lc_substrate_bridge.h/c` | Bio-async messaging |
| Thalamic Bridge | `nimcp_lc_thalamic_bridge.h/c` | Arousal gating |
| Immune Bridge | `nimcp_lc_immune_bridge.h/c` | Stress response |
| Hub Bridge | `nimcp_lc_hub_bridge.h/c` | Cognitive integration |
| Quantum Bridge | `nimcp_lc_quantum_bridge.h/c` | Quantum coherence modulation |

#### 1.3 Brain Factory Integration

**File**: `src/core/brain/factory/init/nimcp_brain_init_locus_coeruleus.c`

#### 1.4 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/locus_coeruleus/test_lc_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/locus_coeruleus/test_ne_release.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/locus_coeruleus/test_novelty_detection.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/locus_coeruleus/test_arousal_modulation.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/locus_coeruleus/test_lc_bridges.cpp` | 15 |
| Integration | `test/integration/core/brain/regions/locus_coeruleus/test_lc_integration.cpp` | 10 |
| Regression | `test/regression/core/brain/regions/locus_coeruleus/test_lc_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_locus_coeruleus_pipeline.cpp` | 5 |

---

### Phase BR-2: Ventral Tegmental Area (Dopamine/Reward)

**Priority**: P1 - Critical for reward learning
**Dependencies**: Basal Ganglia, PFC
**Est. LOC**: 5,500 | **Tests**: 110

#### 2.1 Core Module

**Files**:
- `include/core/brain/regions/vta/nimcp_vta.h`
- `include/core/brain/regions/vta/nimcp_vta_adapter.h`
- `include/core/brain/regions/vta/nimcp_dopamine_release.h`
- `include/core/brain/regions/vta/nimcp_reward_prediction_error.h`
- `include/core/brain/regions/vta/nimcp_incentive_salience.h`
- `src/core/brain/regions/vta/nimcp_vta.c`
- `src/core/brain/regions/vta/nimcp_vta_adapter.c`
- `src/core/brain/regions/vta/nimcp_dopamine_release.c`
- `src/core/brain/regions/vta/nimcp_reward_prediction_error.c`
- `src/core/brain/regions/vta/nimcp_incentive_salience.c`

**API**:
```c
typedef struct {
    // Dopamine dynamics
    float da_firing_rate;          // Hz
    float da_concentration;        // nM
    float da_release_rate;         // nM/s

    // Reward prediction error
    float expected_reward;
    float actual_reward;
    float prediction_error;        // RPE = actual - expected

    // Incentive salience
    float wanting_signal;          // Motivation
    float liking_signal;           // Hedonic impact

    // Goal-directed behavior
    float goal_value;
    float effort_cost;
    float net_utility;

    // Projections
    nimcp_projection_t nac_projection;    // Nucleus accumbens
    nimcp_projection_t pfc_projection;    // Prefrontal cortex
    nimcp_projection_t hippocampal_projection;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_vta_t;

nimcp_status_t nimcp_vta_init(nimcp_vta_t* vta, nimcp_brain_t* brain);
nimcp_status_t nimcp_vta_update(nimcp_vta_t* vta, float dt);
nimcp_status_t nimcp_vta_compute_rpe(nimcp_vta_t* vta,
                                      float reward,
                                      float* rpe);
nimcp_status_t nimcp_vta_modulate_motivation(nimcp_vta_t* vta,
                                              float goal_value,
                                              float* motivation);
```

#### 2.2 Bridges

Standard 8-bridge pattern (including Quantum Bridge)

#### 2.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/vta/test_vta_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/vta/test_dopamine_release.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/vta/test_reward_prediction_error.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/vta/test_incentive_salience.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/vta/test_vta_bridges.cpp` | 15 |
| Integration | `test/integration/core/brain/regions/vta/test_vta_basal_ganglia_integration.cpp` | 10 |
| Regression | `test/regression/core/brain/regions/vta/test_vta_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_vta_pipeline.cpp` | 5 |

---

### Phase BR-3: Raphe Nuclei (Serotonin/Mood)

**Priority**: P1 - Critical for mood regulation
**Dependencies**: Emotional system, Sleep-wake
**Est. LOC**: 5,000 | **Tests**: 100

#### 3.1 Core Module

**Files**:
- `include/core/brain/regions/raphe/nimcp_raphe.h`
- `include/core/brain/regions/raphe/nimcp_raphe_adapter.h`
- `include/core/brain/regions/raphe/nimcp_serotonin_release.h`
- `include/core/brain/regions/raphe/nimcp_mood_regulation.h`
- `include/core/brain/regions/raphe/nimcp_impulse_control.h`
- `include/core/brain/regions/raphe/nimcp_temporal_discounting.h`
- `src/core/brain/regions/raphe/nimcp_raphe.c`
- `src/core/brain/regions/raphe/nimcp_raphe_adapter.c`
- `src/core/brain/regions/raphe/nimcp_serotonin_release.c`
- `src/core/brain/regions/raphe/nimcp_mood_regulation.c`
- `src/core/brain/regions/raphe/nimcp_impulse_control.c`
- `src/core/brain/regions/raphe/nimcp_temporal_discounting.c`

**API**:
```c
typedef struct {
    // 5-HT dynamics
    float serotonin_firing_rate;   // Hz
    float serotonin_concentration; // nM
    float serotonin_release_rate;  // nM/s

    // Mood state
    float mood_valence;            // -1 to +1
    float emotional_stability;     // 0-1
    float anxiety_level;           // 0-1

    // Impulse control
    float inhibition_strength;     // 0-1
    float patience_factor;         // Delay tolerance
    float risk_aversion;           // 0-1

    // Temporal discounting
    float discount_rate;           // Steepness of discounting
    float future_orientation;      // 0-1

    // Sleep-wake coupling
    float sleep_pressure;
    float wake_promotion;

    // Projections
    nimcp_projection_t cortical_projections;
    nimcp_projection_t limbic_projections;
    nimcp_projection_t hypothalamic_projections;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_raphe_t;
```

#### 3.2 Bridges

Standard 8-bridge pattern

#### 3.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/raphe/test_raphe_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/raphe/test_serotonin_release.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/raphe/test_mood_regulation.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/raphe/test_impulse_control.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/raphe/test_temporal_discounting.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/raphe/test_raphe_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/raphe/test_raphe_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_raphe_pipeline.cpp` | 5 |

---

### Phase BR-4: Habenula (Aversive Learning)

**Priority**: P1 - Critical for balanced learning
**Dependencies**: VTA (inhibitory), Raphe
**Est. LOC**: 4,500 | **Tests**: 90

#### 4.1 Core Module

**Files**:
- `include/core/brain/regions/habenula/nimcp_habenula.h`
- `include/core/brain/regions/habenula/nimcp_habenula_adapter.h`
- `include/core/brain/regions/habenula/nimcp_negative_rpe.h`
- `include/core/brain/regions/habenula/nimcp_avoidance_learning.h`
- `include/core/brain/regions/habenula/nimcp_disappointment_signal.h`
- `src/core/brain/regions/habenula/nimcp_habenula.c`
- `src/core/brain/regions/habenula/nimcp_habenula_adapter.c`
- `src/core/brain/regions/habenula/nimcp_negative_rpe.c`
- `src/core/brain/regions/habenula/nimcp_avoidance_learning.c`
- `src/core/brain/regions/habenula/nimcp_disappointment_signal.c`

**API**:
```c
typedef struct {
    // Activity
    float firing_rate;             // Hz
    float activation_level;        // 0-1

    // Negative reward prediction error
    float negative_rpe;            // Disappointment signal
    float expected_punishment;
    float actual_punishment;

    // Avoidance learning
    float avoidance_strength;      // 0-1
    float learned_helplessness;    // 0-1 (pathological)

    // VTA inhibition
    float vta_inhibition_signal;   // Blocks dopamine

    // Depression modeling
    float anhedonia_level;         // 0-1
    float motivational_deficit;    // 0-1

    // Projections
    nimcp_projection_t vta_projection;     // Inhibitory
    nimcp_projection_t raphe_projection;   // Modulates 5-HT

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_habenula_t;
```

#### 4.2 Bridges

Standard 8-bridge pattern

#### 4.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/habenula/test_habenula_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/habenula/test_negative_rpe.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/habenula/test_avoidance_learning.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/habenula/test_disappointment.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/habenula/test_habenula_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/habenula/test_habenula_vta_integration.cpp` | 10 |
| E2E | `test/e2e/e2e_test_habenula_pipeline.cpp` | 5 |

---

### Phase BR-5: Entorhinal Cortex (Grid Cells/Memory Gateway)

**Priority**: P2 - Critical for memory circuit
**Dependencies**: Hippocampus
**Est. LOC**: 6,000 | **Tests**: 120

#### 5.1 Core Module

**Files**:
- `include/core/brain/regions/entorhinal/nimcp_entorhinal.h`
- `include/core/brain/regions/entorhinal/nimcp_entorhinal_adapter.h`
- `include/core/brain/regions/entorhinal/nimcp_grid_cells.h`
- `include/core/brain/regions/entorhinal/nimcp_border_cells.h`
- `include/core/brain/regions/entorhinal/nimcp_head_direction_cells.h`
- `include/core/brain/regions/entorhinal/nimcp_path_integration.h`
- `include/core/brain/regions/entorhinal/nimcp_memory_gateway.h`
- `src/core/brain/regions/entorhinal/nimcp_entorhinal.c`
- `src/core/brain/regions/entorhinal/nimcp_entorhinal_adapter.c`
- `src/core/brain/regions/entorhinal/nimcp_grid_cells.c`
- `src/core/brain/regions/entorhinal/nimcp_border_cells.c`
- `src/core/brain/regions/entorhinal/nimcp_head_direction_cells.c`
- `src/core/brain/regions/entorhinal/nimcp_path_integration.c`
- `src/core/brain/regions/entorhinal/nimcp_memory_gateway.c`

**API**:
```c
typedef struct {
    // Grid cells
    nimcp_grid_cell_t* grid_cells;
    uint32_t num_grid_cells;
    float grid_scale;              // Grid spacing
    float grid_orientation;        // Grid orientation

    // Border cells
    nimcp_border_cell_t* border_cells;
    uint32_t num_border_cells;

    // Head direction cells
    nimcp_hd_cell_t* hd_cells;
    uint32_t num_hd_cells;
    float current_heading;         // Degrees

    // Path integration
    float position[3];             // x, y, z
    float velocity[3];
    float accumulated_error;

    // Memory gateway
    float encoding_gate;           // 0-1
    float retrieval_gate;          // 0-1
    float memory_binding_strength;

    // Bio-async
    nimcp_bio_async_handler_t bio_handler;
    nimcp_bio_router_t* router;

    // Immune
    nimcp_immune_sensor_t immune_sensor;

    // Logging
    nimcp_logger_t* logger;
    nimcp_metrics_t* metrics;
} nimcp_entorhinal_t;
```

#### 5.2 Bridges

Standard 8-bridge pattern

#### 5.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/entorhinal/test_entorhinal_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/entorhinal/test_grid_cells.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/entorhinal/test_border_cells.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/entorhinal/test_hd_cells.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/entorhinal/test_path_integration.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/entorhinal/test_memory_gateway.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/entorhinal/test_entorhinal_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/entorhinal/test_entorhinal_hippocampus_integration.cpp` | 10 |
| E2E | `test/e2e/e2e_test_entorhinal_pipeline.cpp` | 5 |

---

### Phase BR-6: Perirhinal Cortex (Object Recognition)

**Priority**: P2
**Dependencies**: Entorhinal, Visual cortex
**Est. LOC**: 4,000 | **Tests**: 80

#### 6.1 Core Module

**Files**:
- `include/core/brain/regions/perirhinal/nimcp_perirhinal.h`
- `include/core/brain/regions/perirhinal/nimcp_perirhinal_adapter.h`
- `include/core/brain/regions/perirhinal/nimcp_object_recognition.h`
- `include/core/brain/regions/perirhinal/nimcp_familiarity_signal.h`
- `include/core/brain/regions/perirhinal/nimcp_item_memory.h`
- `src/core/brain/regions/perirhinal/nimcp_perirhinal.c`
- `src/core/brain/regions/perirhinal/nimcp_perirhinal_adapter.c`
- `src/core/brain/regions/perirhinal/nimcp_object_recognition.c`
- `src/core/brain/regions/perirhinal/nimcp_familiarity_signal.c`
- `src/core/brain/regions/perirhinal/nimcp_item_memory.c`

#### 6.2 Bridges

Standard 8-bridge pattern

#### 6.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/perirhinal/test_perirhinal_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/perirhinal/test_object_recognition.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/perirhinal/test_familiarity.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/perirhinal/test_item_memory.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/perirhinal/test_perirhinal_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/perirhinal/test_perirhinal_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_perirhinal_pipeline.cpp` | 5 |

---

### Phase BR-7: Parahippocampal Cortex (Scene/Context)

**Priority**: P2
**Dependencies**: Entorhinal, Visual cortex
**Est. LOC**: 4,000 | **Tests**: 80

#### 7.1 Core Module

**Files**:
- `include/core/brain/regions/parahippocampal/nimcp_parahippocampal.h`
- `include/core/brain/regions/parahippocampal/nimcp_parahippocampal_adapter.h`
- `include/core/brain/regions/parahippocampal/nimcp_scene_recognition.h`
- `include/core/brain/regions/parahippocampal/nimcp_context_processing.h`
- `include/core/brain/regions/parahippocampal/nimcp_spatial_layout.h`
- `src/core/brain/regions/parahippocampal/nimcp_parahippocampal.c`
- `src/core/brain/regions/parahippocampal/nimcp_parahippocampal_adapter.c`
- `src/core/brain/regions/parahippocampal/nimcp_scene_recognition.c`
- `src/core/brain/regions/parahippocampal/nimcp_context_processing.c`
- `src/core/brain/regions/parahippocampal/nimcp_spatial_layout.c`

#### 7.2 Bridges

Standard 8-bridge pattern

#### 7.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/parahippocampal/test_parahippocampal_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/parahippocampal/test_scene_recognition.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/parahippocampal/test_context_processing.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/parahippocampal/test_spatial_layout.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/parahippocampal/test_parahippocampal_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/parahippocampal/test_parahippocampal_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_parahippocampal_pipeline.cpp` | 5 |

---

### Phase BR-8: Mammillary Bodies (Memory Relay)

**Priority**: P2
**Dependencies**: Hippocampus, Thalamus
**Est. LOC**: 3,000 | **Tests**: 60

#### 8.1 Core Module

**Files**:
- `include/core/brain/regions/mammillary/nimcp_mammillary.h`
- `include/core/brain/regions/mammillary/nimcp_mammillary_adapter.h`
- `include/core/brain/regions/mammillary/nimcp_papez_circuit.h`
- `include/core/brain/regions/mammillary/nimcp_spatial_memory_relay.h`
- `src/core/brain/regions/mammillary/nimcp_mammillary.c`
- `src/core/brain/regions/mammillary/nimcp_mammillary_adapter.c`
- `src/core/brain/regions/mammillary/nimcp_papez_circuit.c`
- `src/core/brain/regions/mammillary/nimcp_spatial_memory_relay.c`

#### 8.2 Bridges

Standard 8-bridge pattern

#### 8.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/mammillary/test_mammillary_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/mammillary/test_papez_circuit.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/mammillary/test_spatial_memory_relay.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/mammillary/test_mammillary_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/mammillary/test_mammillary_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_mammillary_pipeline.cpp` | 5 |

---

### Phase BR-9: Somatosensory Cortex (Touch/Proprioception)

**Priority**: P3
**Dependencies**: Thalamus, Motor cortex
**Est. LOC**: 5,000 | **Tests**: 100

#### 9.1 Core Module

**Files**:
- `include/core/brain/regions/somatosensory/nimcp_somatosensory.h`
- `include/core/brain/regions/somatosensory/nimcp_somatosensory_adapter.h`
- `include/core/brain/regions/somatosensory/nimcp_body_map.h`
- `include/core/brain/regions/somatosensory/nimcp_tactile_processing.h`
- `include/core/brain/regions/somatosensory/nimcp_proprioception.h`
- `include/core/brain/regions/somatosensory/nimcp_pain_processing.h`
- `src/core/brain/regions/somatosensory/nimcp_somatosensory.c`
- `src/core/brain/regions/somatosensory/nimcp_somatosensory_adapter.c`
- `src/core/brain/regions/somatosensory/nimcp_body_map.c`
- `src/core/brain/regions/somatosensory/nimcp_tactile_processing.c`
- `src/core/brain/regions/somatosensory/nimcp_proprioception.c`
- `src/core/brain/regions/somatosensory/nimcp_pain_processing.c`

#### 9.2 Bridges

Standard 8-bridge pattern

#### 9.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/somatosensory/test_somatosensory_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/somatosensory/test_body_map.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/somatosensory/test_tactile_processing.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/somatosensory/test_proprioception.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/somatosensory/test_pain_processing.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/somatosensory/test_somatosensory_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/somatosensory/test_somatosensory_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_somatosensory_pipeline.cpp` | 5 |

---

### Phase BR-10: Olfactory/Piriform Cortex (Smell)

**Priority**: P3
**Dependencies**: Amygdala, Entorhinal
**Est. LOC**: 3,500 | **Tests**: 70

#### 10.1 Core Module

**Files**:
- `include/core/brain/regions/olfactory/nimcp_olfactory.h`
- `include/core/brain/regions/olfactory/nimcp_olfactory_adapter.h`
- `include/core/brain/regions/olfactory/nimcp_odor_identification.h`
- `include/core/brain/regions/olfactory/nimcp_olfactory_memory.h`
- `include/core/brain/regions/olfactory/nimcp_odor_pattern_completion.h`
- `src/core/brain/regions/olfactory/nimcp_olfactory.c`
- `src/core/brain/regions/olfactory/nimcp_olfactory_adapter.c`
- `src/core/brain/regions/olfactory/nimcp_odor_identification.c`
- `src/core/brain/regions/olfactory/nimcp_olfactory_memory.c`
- `src/core/brain/regions/olfactory/nimcp_odor_pattern_completion.c`

#### 10.2 Bridges

Standard 8-bridge pattern

#### 10.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/olfactory/test_olfactory_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/olfactory/test_odor_identification.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/olfactory/test_olfactory_memory.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/olfactory/test_olfactory_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/olfactory/test_olfactory_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_olfactory_pipeline.cpp` | 5 |

---

### Phase BR-11: Gustatory Cortex (Taste)

**Priority**: P3
**Dependencies**: Insula, Orbitofrontal
**Est. LOC**: 3,000 | **Tests**: 60

#### 11.1 Core Module

**Files**:
- `include/core/brain/regions/gustatory/nimcp_gustatory.h`
- `include/core/brain/regions/gustatory/nimcp_gustatory_adapter.h`
- `include/core/brain/regions/gustatory/nimcp_taste_processing.h`
- `include/core/brain/regions/gustatory/nimcp_food_reward.h`
- `include/core/brain/regions/gustatory/nimcp_disgust_response.h`
- `src/core/brain/regions/gustatory/nimcp_gustatory.c`
- `src/core/brain/regions/gustatory/nimcp_gustatory_adapter.c`
- `src/core/brain/regions/gustatory/nimcp_taste_processing.c`
- `src/core/brain/regions/gustatory/nimcp_food_reward.c`
- `src/core/brain/regions/gustatory/nimcp_disgust_response.c`

#### 11.2 Bridges

Standard 8-bridge pattern

#### 11.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/gustatory/test_gustatory_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/gustatory/test_taste_processing.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/gustatory/test_food_reward.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/gustatory/test_gustatory_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/gustatory/test_gustatory_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_gustatory_pipeline.cpp` | 5 |

---

### Phase BR-12: Orbitofrontal Cortex (Value/Decision)

**Priority**: P4
**Dependencies**: PFC, Amygdala
**Est. LOC**: 5,000 | **Tests**: 100

#### 12.1 Core Module

**Files**:
- `include/core/brain/regions/orbitofrontal/nimcp_orbitofrontal.h`
- `include/core/brain/regions/orbitofrontal/nimcp_ofc_adapter.h`
- `include/core/brain/regions/orbitofrontal/nimcp_value_representation.h`
- `include/core/brain/regions/orbitofrontal/nimcp_reversal_learning.h`
- `include/core/brain/regions/orbitofrontal/nimcp_expected_outcome.h`
- `include/core/brain/regions/orbitofrontal/nimcp_social_reward.h`
- `src/core/brain/regions/orbitofrontal/nimcp_orbitofrontal.c`
- `src/core/brain/regions/orbitofrontal/nimcp_ofc_adapter.c`
- `src/core/brain/regions/orbitofrontal/nimcp_value_representation.c`
- `src/core/brain/regions/orbitofrontal/nimcp_reversal_learning.c`
- `src/core/brain/regions/orbitofrontal/nimcp_expected_outcome.c`
- `src/core/brain/regions/orbitofrontal/nimcp_social_reward.c`

#### 12.2 Bridges

Standard 8-bridge pattern

#### 12.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/orbitofrontal/test_ofc_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/orbitofrontal/test_value_representation.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/orbitofrontal/test_reversal_learning.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/orbitofrontal/test_expected_outcome.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/orbitofrontal/test_ofc_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/orbitofrontal/test_ofc_integration.cpp` | 10 |
| Regression | `test/regression/core/brain/regions/orbitofrontal/test_ofc_regression.cpp` | 5 |
| E2E | `test/e2e/e2e_test_orbitofrontal_pipeline.cpp` | 5 |

---

### Phase BR-13: Retrosplenial Cortex (Spatial Reasoning)

**Priority**: P4
**Dependencies**: Hippocampus, Parietal
**Est. LOC**: 4,000 | **Tests**: 80

#### 13.1 Core Module

**Files**:
- `include/core/brain/regions/retrosplenial/nimcp_retrosplenial.h`
- `include/core/brain/regions/retrosplenial/nimcp_retrosplenial_adapter.h`
- `include/core/brain/regions/retrosplenial/nimcp_viewpoint_transformation.h`
- `include/core/brain/regions/retrosplenial/nimcp_landmark_navigation.h`
- `include/core/brain/regions/retrosplenial/nimcp_spatial_memory_consolidation.h`
- `src/core/brain/regions/retrosplenial/nimcp_retrosplenial.c`
- `src/core/brain/regions/retrosplenial/nimcp_retrosplenial_adapter.c`
- `src/core/brain/regions/retrosplenial/nimcp_viewpoint_transformation.c`
- `src/core/brain/regions/retrosplenial/nimcp_landmark_navigation.c`
- `src/core/brain/regions/retrosplenial/nimcp_spatial_memory_consolidation.c`

#### 13.2 Bridges

Standard 8-bridge pattern

#### 13.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/retrosplenial/test_retrosplenial_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/retrosplenial/test_viewpoint_transformation.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/retrosplenial/test_landmark_navigation.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/retrosplenial/test_retrosplenial_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/retrosplenial/test_retrosplenial_integration.cpp` | 10 |
| E2E | `test/e2e/e2e_test_retrosplenial_pipeline.cpp` | 5 |

---

### Phase BR-14: Claustrum (Consciousness Integration)

**Priority**: P5
**Dependencies**: All cortical regions
**Est. LOC**: 5,000 | **Tests**: 100

#### 14.1 Core Module

**Files**:
- `include/core/brain/regions/claustrum/nimcp_claustrum.h`
- `include/core/brain/regions/claustrum/nimcp_claustrum_adapter.h`
- `include/core/brain/regions/claustrum/nimcp_cross_modal_binding.h`
- `include/core/brain/regions/claustrum/nimcp_salience_detection.h`
- `include/core/brain/regions/claustrum/nimcp_attention_coordination.h`
- `include/core/brain/regions/claustrum/nimcp_unified_experience.h`
- `src/core/brain/regions/claustrum/nimcp_claustrum.c`
- `src/core/brain/regions/claustrum/nimcp_claustrum_adapter.c`
- `src/core/brain/regions/claustrum/nimcp_cross_modal_binding.c`
- `src/core/brain/regions/claustrum/nimcp_salience_detection.c`
- `src/core/brain/regions/claustrum/nimcp_attention_coordination.c`
- `src/core/brain/regions/claustrum/nimcp_unified_experience.c`

#### 14.2 Bridges

Standard 8-bridge pattern

#### 14.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/claustrum/test_claustrum_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/claustrum/test_cross_modal_binding.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/claustrum/test_salience_detection.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/claustrum/test_attention_coordination.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/claustrum/test_unified_experience.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/claustrum/test_claustrum_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/claustrum/test_claustrum_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_claustrum_pipeline.cpp` | 5 |

---

### Phase BR-15: Periaqueductal Gray (Pain/Defense)

**Priority**: P5
**Dependencies**: Amygdala, Hypothalamus
**Est. LOC**: 4,000 | **Tests**: 80

#### 15.1 Core Module

**Files**:
- `include/core/brain/regions/pag/nimcp_pag.h`
- `include/core/brain/regions/pag/nimcp_pag_adapter.h`
- `include/core/brain/regions/pag/nimcp_analgesia.h`
- `include/core/brain/regions/pag/nimcp_defensive_behaviors.h`
- `include/core/brain/regions/pag/nimcp_vocalization_control.h`
- `include/core/brain/regions/pag/nimcp_autonomic_regulation.h`
- `src/core/brain/regions/pag/nimcp_pag.c`
- `src/core/brain/regions/pag/nimcp_pag_adapter.c`
- `src/core/brain/regions/pag/nimcp_analgesia.c`
- `src/core/brain/regions/pag/nimcp_defensive_behaviors.c`
- `src/core/brain/regions/pag/nimcp_vocalization_control.c`
- `src/core/brain/regions/pag/nimcp_autonomic_regulation.c`

#### 15.2 Bridges

Standard 8-bridge pattern

#### 15.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/pag/test_pag_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/pag/test_analgesia.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/pag/test_defensive_behaviors.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/pag/test_vocalization.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/pag/test_pag_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/pag/test_pag_integration.cpp` | 10 |
| E2E | `test/e2e/e2e_test_pag_pipeline.cpp` | 5 |

---

### Phase BR-16: Red Nucleus (Motor Refinement)

**Priority**: P5
**Dependencies**: Cerebellum, Motor cortex
**Est. LOC**: 3,000 | **Tests**: 60

#### 16.1 Core Module

**Files**:
- `include/core/brain/regions/red_nucleus/nimcp_red_nucleus.h`
- `include/core/brain/regions/red_nucleus/nimcp_red_nucleus_adapter.h`
- `include/core/brain/regions/red_nucleus/nimcp_limb_coordination.h`
- `include/core/brain/regions/red_nucleus/nimcp_motor_learning_support.h`
- `src/core/brain/regions/red_nucleus/nimcp_red_nucleus.c`
- `src/core/brain/regions/red_nucleus/nimcp_red_nucleus_adapter.c`
- `src/core/brain/regions/red_nucleus/nimcp_limb_coordination.c`
- `src/core/brain/regions/red_nucleus/nimcp_motor_learning_support.c`

#### 16.2 Bridges

Standard 8-bridge pattern

#### 16.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/red_nucleus/test_red_nucleus_core.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/red_nucleus/test_limb_coordination.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/red_nucleus/test_motor_learning.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/red_nucleus/test_red_nucleus_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/red_nucleus/test_red_nucleus_integration.cpp` | 5 |
| E2E | `test/e2e/e2e_test_red_nucleus_pipeline.cpp` | 5 |

---

### Phase BR-17: Reticular Formation (Arousal/Consciousness)

**Priority**: P5
**Dependencies**: Medulla, Thalamus
**Est. LOC**: 4,500 | **Tests**: 90

#### 17.1 Core Module

**Files**:
- `include/core/brain/regions/reticular/nimcp_reticular.h`
- `include/core/brain/regions/reticular/nimcp_reticular_adapter.h`
- `include/core/brain/regions/reticular/nimcp_sleep_wake_control.h`
- `include/core/brain/regions/reticular/nimcp_ascending_arousal.h`
- `include/core/brain/regions/reticular/nimcp_attention_gating.h`
- `include/core/brain/regions/reticular/nimcp_vital_reflexes.h`
- `src/core/brain/regions/reticular/nimcp_reticular.c`
- `src/core/brain/regions/reticular/nimcp_reticular_adapter.c`
- `src/core/brain/regions/reticular/nimcp_sleep_wake_control.c`
- `src/core/brain/regions/reticular/nimcp_ascending_arousal.c`
- `src/core/brain/regions/reticular/nimcp_attention_gating.c`
- `src/core/brain/regions/reticular/nimcp_vital_reflexes.c`

#### 17.2 Bridges

Standard 8-bridge pattern

#### 17.3 Tests

| Test Type | File | Test Count |
|-----------|------|------------|
| Unit | `test/unit/core/brain/regions/reticular/test_reticular_core.cpp` | 25 |
| Unit | `test/unit/core/brain/regions/reticular/test_sleep_wake_control.cpp` | 20 |
| Unit | `test/unit/core/brain/regions/reticular/test_ascending_arousal.cpp` | 15 |
| Unit | `test/unit/core/brain/regions/reticular/test_attention_gating.cpp` | 10 |
| Unit | `test/unit/core/brain/regions/reticular/test_reticular_bridges.cpp` | 10 |
| Integration | `test/integration/core/brain/regions/reticular/test_reticular_integration.cpp` | 10 |
| E2E | `test/e2e/e2e_test_reticular_pipeline.cpp` | 5 |

---

## PART III: REMAINING TIER COMPLETION

### Phase T5-7: Extrapolation Capabilities (Remaining 15%)

**Est. LOC**: 4,500 | **Tests**: 90

#### Modules to Complete

| Module | Status | Remaining Work |
|--------|--------|----------------|
| World Model Enhancement | 90% | Multi-modal integration, long-horizon prediction |
| Compositional Generalization | 80% | Systematic compositionality tests |
| Causal Reasoning Enhancement | 85% | Counterfactual imagination integration |

**Files**:
- `include/cognitive/extrapolation/nimcp_world_model_multimodal.h/c`
- `include/cognitive/extrapolation/nimcp_compositional_systematic.h/c`
- `include/cognitive/extrapolation/nimcp_counterfactual_imagination.h/c`

**Tests**: 90 tests across unit/integration/regression/e2e

---

### Phase T11: Embodiment (Remaining 40%)

**Est. LOC**: 2,800 | **Tests**: 56

#### Modules to Complete

| Module | Status | Remaining Work |
|--------|--------|----------------|
| Body Schema | 60% | Affordance processing, body ownership |
| Pragmatics | 50% | Context integration, discourse management |
| Embodied Simulation | 0% | Action simulation, motor imagery |
| Interoceptive Prediction | 70% | Visceral prediction errors |

**Files**:
- `include/embodiment/nimcp_affordance_processing.h/c`
- `include/embodiment/nimcp_body_ownership.h/c`
- `include/embodiment/nimcp_embodied_simulation.h/c`
- `include/language/pragmatics/nimcp_discourse_management.h/c`

**Tests**: 56 tests across unit/integration/regression/e2e

---

### Phase T12: Superhuman Enhancements (Remaining 50%)

**Est. LOC**: 15,000 | **Tests**: 300

#### Modules to Complete

| Module | Status | Remaining Work |
|--------|--------|----------------|
| Eagle Vision | 50% | Long-range pattern detection |
| Echolocation | 30% | 3D spatial mapping from echoes |
| Time Dilation | 40% | Subjective time manipulation |
| Savant Abilities | 0% | Pattern recognition savant mode |
| Hyperthymesia | 60% | Perfect autobiographical recall |
| Synesthesia | 0% | Cross-modal perception |
| Precognition | 70% | Advanced pattern prediction |

**Files**:
- `include/superhuman/nimcp_eagle_vision.h/c`
- `include/superhuman/nimcp_echolocation.h/c`
- `include/superhuman/nimcp_time_dilation.h/c`
- `include/superhuman/nimcp_savant_mode.h/c`
- `include/superhuman/nimcp_hyperthymesia.h/c`
- `include/superhuman/nimcp_synesthesia.h/c`
- `include/superhuman/nimcp_precognition.h/c`

**Tests**: 300 tests across unit/integration/regression/e2e

---

## PART IV: IMPLEMENTATION SCHEDULE

### Phase Order (Dependency-Based)

```
PHASE 1: Advanced Concepts Foundation
├── AC-3: Hodgkin-Huxley Dynamics (biophysical foundation)
├── AC-9: Thermodynamics (energy constraints)
└── AC-1: Ephaptic Coupling (synchronization)

PHASE 2: Chemistry Layer
├── AC-5: Nitric Oxide (retrograde signaling)
├── AC-4: pH Dynamics (metabolic effects)
└── AC-8: Neurovascular Coupling (blood flow)

PHASE 3: Biology Layer
├── AC-6: Epigenetics (long-term changes)
├── AC-7: Neurogenesis (new neurons)
└── AC-2: Information Geometry (learning optimization)

PHASE 4: Neuromodulatory Centers (Critical)
├── BR-1: Locus Coeruleus (NE)
├── BR-2: Ventral Tegmental Area (DA)
├── BR-3: Raphe Nuclei (5-HT)
└── BR-4: Habenula (aversive)

PHASE 5: Memory Circuit
├── BR-5: Entorhinal Cortex
├── BR-6: Perirhinal Cortex
├── BR-7: Parahippocampal Cortex
└── BR-8: Mammillary Bodies

PHASE 6: Sensory Processing
├── BR-9: Somatosensory Cortex
├── BR-10: Olfactory Cortex
└── BR-11: Gustatory Cortex

PHASE 7: Executive Enhancement
├── BR-12: Orbitofrontal Cortex
└── BR-13: Retrosplenial Cortex

PHASE 8: Integration Layer
├── BR-14: Claustrum
├── BR-15: Periaqueductal Gray
├── BR-16: Red Nucleus
└── BR-17: Reticular Formation

PHASE 9: Tier Completion
├── T5-7: Extrapolation (15% remaining)
├── T11: Embodiment (40% remaining)
└── T12: Superhuman (50% remaining)
```

---

## PART V: TEST SUMMARY

### Total Test Count by Type

| Test Type | Count |
|-----------|-------|
| Unit Tests | 2,100 |
| Integration Tests | 500 |
| Regression Tests | 250 |
| E2E Tests | 196 |
| **TOTAL** | **3,046** |

### Test Coverage Requirements

- **Line Coverage**: >= 85%
- **Branch Coverage**: >= 80%
- **Function Coverage**: >= 95%

### Test Execution

```bash
# Run all new tests
cd /home/bbrelin/nimcp/build
ctest -R "physics|chemistry|biology|locus_coeruleus|vta|raphe|habenula|entorhinal|perirhinal|parahippocampal|mammillary|somatosensory|olfactory|gustatory|orbitofrontal|retrosplenial|claustrum|pag|red_nucleus|reticular" --output-on-failure

# Run with parallel execution
ctest -j$(nproc) --output-on-failure
```

---

## PART VI: SUMMARY

### Total Implementation Scope

| Category | Modules | Est. LOC | Tests |
|----------|---------|----------|-------|
| Advanced Concepts | 9 | 42,000 | 840 |
| Brain Regions | 17 | 73,500 | 1,470 |
| Tier Completion | 3 tiers | 22,300 | 446 |
| Bridges (8 per module) | 208 bridges | 14,500 | 290 |
| **Intra-Layer Integration** | **9 layers** | **18,000** | **295** |
| **Inter-Layer Integration** | **13 connections** | **13,000** | **300** |
| **Layer Coordinator System** | **1 system** | **3,000** | **50** |
| **Cognitive Hub Integration** | **40 modules** | **8,000** | **160** |
| **Omnidirectional Integration** | **40 modules** | **12,000** | **240** |
| **Hypothalamus Integration** | **40 modules** | **8,000** | **105** |
| **Full-Stack Integration** | **4 test suites** | **-** | **50** |
| **Thalamic/Middleware Integration** | **40 modules** | **16,000** | **140** |
| **Neural Substrate Integration** | **40 modules** | **12,000** | **115** |
| **Motor Cortex Integration** | **40 modules** | **15,000** | **150** |
| **Portia Integration** | **40 modules** | **14,000** | **125** |
| **Swarm Integration** | **40 modules** | **18,000** | **150** |
| **Dragonfly Integration** | **40 modules** | **16,000** | **130** |
| **Sleep Integration** | **40 modules** | **20,000** | **170** |
| **Glial Integration** | **40 modules** | **22,000** | **190** |
| **Quantum Integration** | **40 modules** | **24,000** | **210** |
| **Training Integration** | **40 modules** | **26,000** | **230** |
| **Language Integration** | **40 modules** | **28,000** | **250** |
| **Perception Integration** | **40 modules** | **30,000** | **270** |
| **GPU Module Integration** | **70 headers** | **35,000** | **300** |
| **Information Theory Integration** | **4 headers** | **18,000** | **160** |
| **Plasticity Module Integration** | **100+ headers** | **40,000** | **350** |
| **Security Module Integration** | **44 headers** | **45,000** | **400** |
| **GRAND TOTAL** | **~40 modules + full integration** | **~594,300** | **~7,536** |

### Key Requirements Checklist

#### Core Module Requirements
- [ ] All modules communicate via bio-async
- [ ] All modules have 8 standard bridges (SNN, Plasticity, FEP, Substrate, Thalamic, Immune, Hub, Quantum)
- [ ] All modules integrate with immune system
- [ ] All modules initialized via brain factory
- [ ] All modules have comprehensive logging

#### Testing Requirements
- [ ] All modules have unit tests (>=20 per module)
- [ ] All modules have integration tests (>=5 per module)
- [ ] All modules have regression tests (>=5 per module)
- [ ] All modules have e2e tests (>=5 per module)

#### Layer Integration Requirements
- [ ] **All modules implement intra-layer integration with sibling modules**
- [ ] **All modules implement inter-layer integration with connected layers**
- [ ] **All layers registered with central Layer Coordinator**
- [ ] **Bottom-up and top-down pathways verified for all inter-layer connections**
- [ ] **Intra-layer coherence tests pass (>=0.85 coherence)**
- [ ] **Inter-layer synchronization tests pass**
- [ ] **Full-stack integration tests pass (Physics → Integration and back)**

#### Cognitive Hub Integration Requirements
- [ ] **All modules registered with cognitive_integration_hub**
- [ ] **All modules subscribe to relevant cognitive events**
- [ ] **All modules publish appropriate cognitive events**
- [ ] **All modules implement query handler for inter-module queries**
- [ ] **Cognitive category correctly assigned to each module**

#### Omnidirectional Integration Requirements
- [ ] **All modules have omni bridge (nimcp_<module>_omni_bridge.h/c)**
- [ ] **All modules connect to JEPA bidirectional predictor**
- [ ] **All modules integrate with predictive hierarchy**
- [ ] **All modules handle BIO_MSG_OMNI_* message types**
- [ ] **All modules handle BIO_MSG_PRED_HIER_* message types**
- [ ] **Precision weighting updated from free energy estimates**
- [ ] **Forward prediction and backward inference implemented**

#### Hypothalamus Integration Requirements
- [ ] **All modules have hypothalamus bridge (nimcp_hypothalamus_<module>_bridge.h/c)**
- [ ] **All modules receive circadian phase signals**
- [ ] **All modules receive stress/cortisol signals**
- [ ] **All modules receive autonomic state signals**
- [ ] **All modules report metabolic demands**
- [ ] **All modules report homeostatic perturbations**
- [ ] **Circadian modulation affects module behavior**
- [ ] **Stress response modulates module activity**

### Integration Architecture Verification

```
VERIFICATION CHECKLIST FOR EACH MODULE:

□ Intra-Layer Integration
  □ module_intra_layer_t struct implemented
  □ module_intra_layer_init() implemented
  □ module_intra_layer_sync() implemented
  □ module_intra_layer_broadcast() implemented
  □ module_intra_layer_receive() implemented
  □ All sibling bridges created
  □ Layer coordinator registration complete

□ Inter-Layer Integration
  □ module_inter_layer_t struct implemented
  □ module_inter_layer_init() implemented
  □ module_inter_layer_send_bottom_up() implemented
  □ module_inter_layer_send_top_down() implemented
  □ module_inter_layer_receive() implemented
  □ module_inter_layer_sync_phase() implemented
  □ All inter-layer bridges created
  □ Routing table entries verified

□ Test Coverage
  □ Intra-layer unit tests
  □ Intra-layer integration tests
  □ Inter-layer unit tests
  □ Inter-layer integration tests
  □ Full-stack traversal tests
```

---

**Document Version**: 1.1
**Created**: 2026-01-09
**Updated**: 2026-01-09
**Author**: Claude Code
**Status**: Ready for Implementation
