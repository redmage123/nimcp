/**
 * @file nimcp_cortical_substrate_bridge.h
 * @brief Bridge between neural substrate and cortical column system
 *
 * WHAT: Bidirectional integration layer connecting metabolic/thermal substrate
 *       with cortical column architecture (laminar processing, hierarchical
 *       organization, sparse coding, and columnar competition).
 *
 * WHY: Cortical columns are the fundamental computational units of neocortex,
 *      requiring sustained ATP for laminar processing across 6 layers, winner-
 *      take-all competition, sparse distributed coding, and hierarchical message
 *      passing. ATP depletion, hyperthermia, and metabolic stress degrade these
 *      sophisticated cortical computations.
 *
 * HOW: Monitors substrate state (ATP, temperature, metabolic stress) and
 *      modulates cortical parameters:
 *      - Column fidelity: Precision of columnar processing
 *      - Layer-specific gain: Per-layer metabolic modulation (L1-L6)
 *      - Competition efficiency: Winner-take-all sharpness
 *      - Sparsity modulation: Sparse coding pattern quality
 *      - Hierarchical depth: Multi-level processing capacity
 *
 * BIOLOGICAL BASIS:
 * - Cortical columns are metabolically expensive (dense synaptic matrices)
 * - Layer IV (thalamic input) is most ATP-dependent (Q10=2.3)
 * - Layers II/III (cortico-cortical) have highest Q10 (2.8) due to extensive
 *   lateral connections and association processing
 * - Layer V (cortical output) requires sustained ATP for pyramidal output
 * - ATP depletion reduces competitive inhibition → less sparse coding
 * - Hyperthermia impairs hierarchical processing (fever degrades abstraction)
 * - Metabolic stress causes layer-specific degradation based on Q10
 *
 * Integration Points:
 * ┌──────────────────┐
 * │ Neural Substrate │
 * │  - ATP levels    │──┐
 * │  - Temperature   │  │
 * │  - Metabolic Q10 │  │
 * └──────────────────┘  │
 *                       │
 *                       ▼
 *           ┌──────────────────────┐
 *           │ Cortical Substrate   │
 *           │      Bridge          │
 *           └──────────────────────┘
 *                       │
 *                       ▼
 * ┌──────────────────────────────────┐
 * │    Cortical Column System        │
 * │  - Column pools                  │
 * │  - Laminar structure (L1-L6)     │
 * │  - Cortical hierarchy            │
 * │  - Sparse coding                 │
 * └──────────────────────────────────┘
 */

#ifndef NIMCP_CORTICAL_SUBSTRATE_BRIDGE_H
#define NIMCP_CORTICAL_SUBSTRATE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations for cortical types (opaque pointers) */
typedef struct cortical_column_pool cortical_column_pool_t;
typedef struct laminar_structure laminar_structure_t;
typedef struct cortical_hierarchy cortical_hierarchy_t;
typedef struct cortical_sparse_coding_system cortical_sparse_coding_system_t;

/**
 * Bio-async module ID for cortical substrate bridge
 * Range: 0x1200-0x12FF (substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_CORTICAL 0x1210

/**
 * Number of cortical layers modulated
 */
#define CORTICAL_SUBSTRATE_NUM_LAYERS 5

/**
 * ATP threshold constants
 * WHAT: Critical ATP levels that trigger cortical processing impairment
 * WHY: Cortical columns require high metabolic support for laminar computation
 */
#define CORTICAL_SUBSTRATE_ATP_FULL 0.8f      /* Full cortical capacity */
#define CORTICAL_SUBSTRATE_ATP_REDUCED 0.5f   /* Reduced cortical processing */
#define CORTICAL_SUBSTRATE_ATP_CRITICAL 0.3f  /* Severely impaired columns */

/**
 * Temperature Q10 coefficients for cortical layers
 * WHAT: Layer-specific temperature sensitivity
 * WHY: Different cortical layers have distinct metabolic profiles
 * HOW: Q10 values represent 10°C metabolic rate scaling per layer
 *
 * BIOLOGICAL:
 * - Layer I (Molecular): Sparse, low metabolism (Q10=2.0)
 * - Layers II/III (Supra-granular): Dense cortico-cortical, highest Q10 (2.8)
 * - Layer IV (Granular): Thalamic input, moderate Q10 (2.3)
 * - Layer V (Infra-granular): Pyramidal output, high Q10 (2.5)
 * - Layer VI (Infra-granular): Cortico-thalamic feedback (Q10=2.4)
 */
#define CORTICAL_SUBSTRATE_Q10_LAYER_I 2.0f      /* Molecular layer */
#define CORTICAL_SUBSTRATE_Q10_LAYER_II_III 2.8f /* Supra-granular (highest) */
#define CORTICAL_SUBSTRATE_Q10_LAYER_IV 2.3f     /* Granular (thalamic input) */
#define CORTICAL_SUBSTRATE_Q10_LAYER_V 2.5f      /* Infra-granular (output) */
#define CORTICAL_SUBSTRATE_Q10_LAYER_VI 2.4f     /* Infra-granular (feedback) */

/**
 * Cortical substrate effects
 * WHAT: Computed metabolic/thermal effects on cortical processing
 * WHY: Provides quantified impact of substrate state on cortical columns
 * HOW: Values in [0-1] range, where 1.0 = optimal, 0.0 = fully impaired
 */
typedef struct {
    float column_fidelity;          /* Precision of columnar processing [0-1] */
    float layer_gain[CORTICAL_SUBSTRATE_NUM_LAYERS]; /* Per-layer metabolic gain (L1-L6) [0-1] */
    float competition_efficiency;   /* Winner-take-all sharpness [0-1] */
    float sparsity_modulation;      /* Sparse coding pattern quality [0.5-2.0] */
    float hierarchical_depth;       /* Multi-level processing capacity [0-1] */
    bool is_impaired;               /* Overall impairment flag */
} cortical_substrate_effects_t;

/**
 * Cortical substrate configuration
 * WHAT: Bridge behavior configuration
 * WHY: Allows tuning of substrate-cortical coupling
 * HOW: Enable/disable specific modulations and set sensitivities
 */
typedef struct {
    bool enable_column_fidelity_modulation;  /* Modulate column fidelity */
    bool enable_layer_gain_modulation;       /* Modulate per-layer gain */
    bool enable_competition_modulation;      /* Modulate competition efficiency */
    bool enable_sparsity_modulation;         /* Modulate sparse coding */
    bool enable_hierarchical_modulation;     /* Modulate hierarchical processing */
    bool enable_bio_async;                   /* Enable bio-async messaging */
    float atp_sensitivity;                   /* ATP impact scaling [0-2] */
    float temperature_sensitivity;           /* Temperature impact scaling [0-2] */
} cortical_substrate_config_t;

/**
 * Cortical substrate statistics
 * WHAT: Runtime monitoring metrics
 * WHY: Track bridge performance and cortical state over time
 * HOW: Accumulated counters and averages
 */
typedef struct {
    uint64_t update_count;              /* Number of updates performed */
    uint64_t impairment_events;         /* Times cortex became impaired */
    uint64_t competition_downgrades;    /* Times competition weakened */
    float avg_column_fidelity;          /* Running average column fidelity */
    float avg_competition_efficiency;   /* Running average competition */
    float avg_sparsity_modulation;      /* Running average sparsity */
    float avg_hierarchical_depth;       /* Running average depth */
    float min_fidelity_observed;        /* Lowest fidelity observed */
    float max_fidelity_observed;        /* Highest fidelity observed */
} cortical_substrate_stats_t;

/**
 * Cortical substrate bridge
 * WHAT: Main integration structure connecting substrate and cortical system
 * WHY: Encapsulates bidirectional substrate-cortical coupling
 * HOW: Holds pointers to both systems, computed effects, and state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    neural_substrate_t* substrate;                  /* Neural substrate system */
    void* columns;                                  /* Cortical column pool (optional) */
    void* laminar;                                  /* Laminar structure (optional) */
    void* hierarchy;                                /* Cortical hierarchy (optional) */
    void* sparse;                                   /* Sparse coding system (optional) */
    cortical_substrate_effects_t effects;           /* Current effects */
    cortical_substrate_config_t config;             /* Configuration */
    cortical_substrate_stats_t stats;               /* Statistics */} cortical_substrate_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * Get default configuration
 * WHAT: Provides sensible default bridge settings
 * WHY: Ensures safe initialization with validated parameters
 * HOW: Sets all modulations enabled, moderate sensitivities
 *
 * @param config Output configuration structure
 */
void cortical_substrate_default_config(cortical_substrate_config_t* config);

/**
 * Create cortical substrate bridge
 * WHAT: Allocates and initializes bridge between substrate and cortical system
 * WHY: Establishes bidirectional integration layer
 * HOW: Validates inputs, allocates structure, initializes mutex
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param substrate Neural substrate system (must be non-NULL)
 * @return Bridge pointer on success, NULL on failure
 */
cortical_substrate_bridge_t* cortical_substrate_bridge_create(
    const cortical_substrate_config_t* config,
    neural_substrate_t* substrate
);

/**
 * Destroy cortical substrate bridge
 * WHAT: Cleans up bridge resources
 * WHY: Prevents memory leaks, disconnects systems
 * HOW: Disconnects bio-async, destroys mutex, frees structure
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void cortical_substrate_bridge_destroy(cortical_substrate_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * Connect cortical column pool
 */
int cortical_substrate_connect_columns(
    cortical_substrate_bridge_t* bridge,
    void* columns
);

/**
 * Connect laminar structure
 */
int cortical_substrate_connect_laminar(
    cortical_substrate_bridge_t* bridge,
    void* laminar
);

/**
 * Connect cortical hierarchy
 */
int cortical_substrate_connect_hierarchy(
    cortical_substrate_bridge_t* bridge,
    void* hierarchy
);

/**
 * Connect sparse coding system
 */
int cortical_substrate_connect_sparse(
    cortical_substrate_bridge_t* bridge,
    void* sparse
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * Connect to bio-async router
 */
int cortical_substrate_connect_bio_async(cortical_substrate_bridge_t* bridge);

/**
 * Disconnect from bio-async router
 */
int cortical_substrate_disconnect_bio_async(cortical_substrate_bridge_t* bridge);

/**
 * Check bio-async connection status
 */
bool cortical_substrate_is_bio_async_connected(const cortical_substrate_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * Update cortical substrate effects
 * WHAT: Recomputes substrate effects on cortical system based on current state
 * WHY: Keeps cortical modulation synchronized with substrate changes
 * HOW: Reads substrate ATP/temperature, computes effects, updates stats
 *
 * @param bridge Cortical substrate bridge
 * @return 0 on success, negative on error
 */
int cortical_substrate_update(cortical_substrate_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * Get current column fidelity
 */
float cortical_substrate_get_column_fidelity(const cortical_substrate_bridge_t* bridge);

/**
 * Get layer-specific gain
 */
float cortical_substrate_get_layer_gain(
    const cortical_substrate_bridge_t* bridge,
    int layer_index
);

/**
 * Get current competition efficiency
 */
float cortical_substrate_get_competition_efficiency(const cortical_substrate_bridge_t* bridge);

/**
 * Get current sparsity modulation
 */
float cortical_substrate_get_sparsity_modulation(const cortical_substrate_bridge_t* bridge);

/**
 * Get current hierarchical depth
 */
float cortical_substrate_get_hierarchical_depth(const cortical_substrate_bridge_t* bridge);

/**
 * Get all cortical substrate effects
 */
int cortical_substrate_get_effects(
    const cortical_substrate_bridge_t* bridge,
    cortical_substrate_effects_t* effects
);

/**
 * Check if cortical system is impaired
 */
bool cortical_substrate_is_impaired(const cortical_substrate_bridge_t* bridge);

/**
 * Get bridge statistics
 */
int cortical_substrate_get_stats(
    const cortical_substrate_bridge_t* bridge,
    cortical_substrate_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_SUBSTRATE_BRIDGE_H */
