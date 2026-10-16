/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_nvc_hub_bridge.h - Neurovascular Coupling to Cognitive Hub Bridge
//=============================================================================
/**
 * @file nimcp_nvc_hub_bridge.h
 * @brief Bridge between Neurovascular Coupling and Cognitive Hub
 *
 * WHAT: Connects hemodynamic state with cognitive hub functions, enabling
 *       blood flow to modulate executive processing, integration, and
 *       global workspace dynamics.
 *
 * WHY:  Cognitive hubs are metabolically expensive regions that:
 *       - Integrate information from multiple modalities
 *       - Maintain working memory representations
 *       - Support executive control and attention
 *       - Enable conscious access through global broadcast
 *       - Require sustained blood flow for complex cognition
 *
 * HOW:  Metabolic modulation of cognitive function:
 *       1. CBF → Integration capacity and bandwidth
 *       2. Oxygen → Working memory maintenance
 *       3. Metabolic state → Executive function efficiency
 *       4. BOLD → Global workspace engagement indicator
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           COGNITIVE HUB FUNCTIONS
 * ─────────────────────────────────────────────────────────────────
 * Prefrontal CBF                    → Executive control capacity
 * Parietal perfusion                → Attention/spatial integration
 * Temporal-parietal junction        → Theory of mind, social cognition
 * Posterior cingulate               → Default mode, self-reflection
 * Insula perfusion                  → Interoception, emotional awareness
 * Metabolic reserve                 → Sustained cognitive effort
 * ```
 *
 * COGNITIVE HUB REGIONS:
 * - DLPFC: Executive control, working memory
 * - PPC: Attention, spatial processing
 * - TPJ: Social cognition, perspective taking
 * - PCC: Default mode hub, self-referential
 * - Insula: Interoceptive awareness
 * - ACC: Conflict monitoring, error detection
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_HUB_BRIDGE_H
#define NIMCP_NVC_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define NVC_HUB_MODULE_NAME             "nvc_hub_bridge"

/** Maximum cognitive hubs tracked */
#define NVC_HUB_MAX_HUBS                16

/** Maximum integration pathways */
#define NVC_HUB_MAX_PATHWAYS            64

/** Maximum broadcast channels */
#define NVC_HUB_MAX_BROADCASTS          32

/** Minimum CBF for full cognitive capacity (% baseline) */
#define NVC_HUB_MIN_CBF_FULL            0.85f

/** CBF threshold for degraded function */
#define NVC_HUB_CBF_DEGRADED            0.7f

/** CBF threshold for cognitive failure */
#define NVC_HUB_CBF_FAILURE             0.5f

/** Working memory metabolic cost factor */
#define NVC_HUB_WM_METABOLIC_COST       0.05f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive hub type
 */
typedef enum {
    NVC_HUB_DLPFC = 0,                   /**< Dorsolateral prefrontal cortex */
    NVC_HUB_PPC,                         /**< Posterior parietal cortex */
    NVC_HUB_TPJ,                         /**< Temporoparietal junction */
    NVC_HUB_PCC,                         /**< Posterior cingulate cortex */
    NVC_HUB_INSULA,                      /**< Insular cortex */
    NVC_HUB_ACC,                         /**< Anterior cingulate cortex */
    NVC_HUB_FRONTOPOLAR,                 /**< Frontopolar cortex */
    NVC_HUB_PRECUNEUS                    /**< Precuneus */
} nvc_hub_type_t;

/**
 * @brief Cognitive capacity state
 */
typedef enum {
    NVC_HUB_CAPACITY_FULL = 0,           /**< Full cognitive capacity */
    NVC_HUB_CAPACITY_REDUCED,            /**< Reduced capacity */
    NVC_HUB_CAPACITY_MINIMAL,            /**< Minimal capacity */
    NVC_HUB_CAPACITY_FAILED              /**< Cognitive failure */
} nvc_hub_capacity_t;

/**
 * @brief Global workspace state
 */
typedef enum {
    NVC_GW_INACTIVE = 0,                 /**< No global broadcast */
    NVC_GW_COMPETING,                    /**< Representations competing */
    NVC_GW_IGNITING,                     /**< Ignition in progress */
    NVC_GW_BROADCASTING,                 /**< Active global broadcast */
    NVC_GW_FADING                        /**< Broadcast fading */
} nvc_hub_gw_state_t;

/**
 * @brief Integration pathway type
 */
typedef enum {
    NVC_PATHWAY_BOTTOM_UP = 0,           /**< Sensory → Hub */
    NVC_PATHWAY_TOP_DOWN,                /**< Hub → Sensory/Motor */
    NVC_PATHWAY_LATERAL,                 /**< Hub ↔ Hub */
    NVC_PATHWAY_BROADCAST                /**< Global workspace broadcast */
} nvc_hub_pathway_type_t;

/**
 * @brief Executive function type
 */
typedef enum {
    NVC_EXEC_WORKING_MEMORY = 0,
    NVC_EXEC_ATTENTION,
    NVC_EXEC_INHIBITION,
    NVC_EXEC_FLEXIBILITY,
    NVC_EXEC_PLANNING,
    NVC_EXEC_COUNT
} nvc_hub_exec_function_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-Hub bridge
 */
typedef struct {
    /** Capacity thresholds */
    float full_capacity_cbf;             /**< CBF for full capacity (% base) */
    float reduced_capacity_cbf;          /**< CBF for reduced capacity */
    float minimal_capacity_cbf;          /**< CBF for minimal capacity */

    /** Executive function costs */
    float wm_metabolic_cost;             /**< WM maintenance cost */
    float attention_metabolic_cost;      /**< Attention cost */
    float inhibition_metabolic_cost;     /**< Inhibition cost */
    float flexibility_metabolic_cost;    /**< Cognitive flexibility cost */

    /** Global workspace parameters */
    bool enable_global_workspace;        /**< Model global workspace */
    float ignition_threshold;            /**< Activation for ignition */
    float broadcast_duration_ms;         /**< Broadcast duration */
    float broadcast_metabolic_cost;      /**< Metabolic cost of broadcast */

    /** Integration parameters */
    float pathway_metabolic_scale;       /**< Pathway metabolic scaling */
    float integration_bandwidth_base;    /**< Base integration bandwidth */

    /** Fatigue modeling */
    bool enable_cognitive_fatigue;       /**< Model cognitive fatigue */
    float fatigue_accumulation_rate;     /**< Fatigue build-up rate */
    float fatigue_recovery_rate;         /**< Fatigue recovery rate */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_hub_config_t;

/**
 * @brief Cognitive hub state
 */
typedef struct {
    nvc_hub_type_t hub_type;             /**< Hub type */
    uint32_t hub_id;                     /**< Hub ID */
    uint32_t nvc_unit_id;                /**< Mapped NVC unit */

    /** Metabolic state */
    float cbf_ratio;                     /**< Current CBF/baseline */
    float oxygen_level;                  /**< Local O2 (0-1) */
    float metabolic_reserve;             /**< Available metabolic reserve */

    /** Capacity state */
    nvc_hub_capacity_t capacity;         /**< Current capacity state */
    float effective_capacity;            /**< Capacity level (0-1) */

    /** Executive function efficiency */
    float exec_efficiency[NVC_EXEC_COUNT]; /**< Per-function efficiency */
    float overall_efficiency;            /**< Overall efficiency (0-1) */

    /** Working memory */
    float wm_capacity;                   /**< WM capacity (items) */
    float wm_maintenance_load;           /**< Current maintenance load */
    float wm_decay_rate;                 /**< Decay rate from metabolism */

    /** Integration */
    float integration_bandwidth;         /**< Current integration bandwidth */
    float active_pathways;               /**< Number of active pathways */

    /** Fatigue */
    float fatigue_level;                 /**< Accumulated fatigue (0-1) */
    float time_at_high_load_ms;          /**< Time under high load */
} nvc_hub_state_t;

/**
 * @brief Integration pathway state
 */
typedef struct {
    uint32_t pathway_id;                 /**< Pathway identifier */
    uint32_t source_hub;                 /**< Source hub ID */
    uint32_t target_hub;                 /**< Target hub/region ID */
    nvc_hub_pathway_type_t type;         /**< Pathway type */

    /** Pathway strength */
    float base_strength;                 /**< Base connection strength */
    float effective_strength;            /**< Metabolically-scaled strength */
    float metabolic_modulation;          /**< Metabolic modulation factor */

    /** Bandwidth */
    float bandwidth;                     /**< Information bandwidth */
    float utilization;                   /**< Current utilization (0-1) */
} nvc_hub_pathway_state_t;

/**
 * @brief Global workspace state
 */
typedef struct {
    nvc_hub_gw_state_t state;            /**< Current GW state */
    float activation_level;              /**< Global activation level */

    /** Broadcast info */
    uint32_t broadcasting_hub;           /**< Hub driving broadcast */
    float broadcast_strength;            /**< Broadcast intensity */
    float broadcast_duration_ms;         /**< Time since broadcast start */

    /** Metabolic constraints */
    float broadcast_metabolic_cost;      /**< Metabolic cost of broadcast */
    float available_metabolic_budget;    /**< Available for broadcast */
    bool broadcast_sustainable;          /**< Can sustain broadcast */

    /** Competition */
    uint32_t competing_hubs;             /**< Number of competing hubs */
    float competition_resolution_time;   /**< Time to resolution */
} nvc_hub_gw_broadcast_t;

/**
 * @brief Cognitive load estimate
 */
typedef struct {
    float total_load;                    /**< Total cognitive load (0-1) */
    float wm_load;                       /**< Working memory component */
    float attention_load;                /**< Attention component */
    float executive_load;                /**< Executive control component */

    /** Metabolic demand */
    float metabolic_demand;              /**< Demanded metabolic resources */
    float metabolic_supply;              /**< Available supply */
    float demand_supply_ratio;           /**< Demand/supply ratio */

    /** Sustainability */
    float sustainable_duration_ms;       /**< How long load is sustainable */
    bool at_capacity;                    /**< Operating at maximum */
} nvc_hub_cognitive_load_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t capacity_transitions;       /**< Capacity state changes */
    uint64_t gw_ignitions;               /**< Global workspace ignitions */
    uint64_t gw_failed_ignitions;        /**< Failed ignition attempts */
    uint64_t pathway_updates;            /**< Pathway modulations */

    /** Averages */
    float mean_capacity;                 /**< Mean capacity across hubs */
    float mean_efficiency;               /**< Mean executive efficiency */
    float mean_fatigue;                  /**< Mean fatigue level */
    float mean_wm_capacity;              /**< Mean WM capacity */

    /** Extremes */
    float min_capacity;                  /**< Minimum capacity reached */
    float max_fatigue;                   /**< Maximum fatigue reached */

    /** Global workspace */
    float gw_time_broadcasting_pct;      /**< % time in broadcast */
    float mean_broadcast_duration_ms;    /**< Mean broadcast duration */

    float last_update_ms;                /**< Last update timestamp */
} nvc_hub_stats_t;

/** Opaque bridge handle */
typedef struct nvc_hub_bridge_struct nvc_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_default_config(nvc_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-Hub bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes metabolic coupling for cognitive function
 * HOW:  Creates internal state for hub and pathway tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_hub_bridge_t* nvc_hub_bridge_create(
    const nvc_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_hub_bridge_destroy(nvc_hub_bridge_t* bridge);

//=============================================================================
// Hub Management API
//=============================================================================

/**
 * @brief Register cognitive hub with NVC unit
 *
 * WHAT: Maps cognitive hub to vascular territory
 * WHY:  Enables local metabolic modulation of cognition
 * HOW:  Creates hub entry with NVC mapping
 *
 * @param bridge Bridge handle
 * @param hub_type Type of cognitive hub
 * @param nvc_unit_id NVC unit ID
 * @param hub_id Output hub ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_register_hub(
    nvc_hub_bridge_t* bridge,
    nvc_hub_type_t hub_type,
    uint32_t nvc_unit_id,
    uint32_t* hub_id
);

/**
 * @brief Unregister cognitive hub
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to unregister
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_unregister_hub(
    nvc_hub_bridge_t* bridge,
    uint32_t hub_id
);

/**
 * @brief Create integration pathway
 *
 * WHAT: Establishes pathway between hubs/regions
 * WHY:  Enables integration capacity modeling
 * HOW:  Creates pathway with metabolic constraints
 *
 * @param bridge Bridge handle
 * @param source_hub Source hub ID
 * @param target_hub Target hub/region ID
 * @param type Pathway type
 * @param base_strength Base connection strength
 * @param pathway_id Output pathway ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_create_pathway(
    nvc_hub_bridge_t* bridge,
    uint32_t source_hub,
    uint32_t target_hub,
    nvc_hub_pathway_type_t type,
    float base_strength,
    uint32_t* pathway_id
);

//=============================================================================
// Metabolic State API
//=============================================================================

/**
 * @brief Update metabolic state from NVC
 *
 * WHAT: Receives blood flow state for cognitive modulation
 * WHY:  Perfusion affects cognitive capacity
 * HOW:  Converts CBF/OEF to capacity and efficiency
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit providing state
 * @param cbf Cerebral blood flow
 * @param cbf_baseline Baseline CBF
 * @param oef Oxygen extraction fraction
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_update_from_nvc(
    nvc_hub_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf,
    float cbf_baseline,
    float oef
);

/**
 * @brief Get hub state
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query
 * @param state Output hub state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_get_state(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id,
    nvc_hub_state_t* state
);

//=============================================================================
// Cognitive Capacity API
//=============================================================================

/**
 * @brief Get cognitive capacity for hub
 *
 * WHAT: Returns current cognitive capacity
 * WHY:  Capacity determines processing ability
 * HOW:  Derived from metabolic state
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query
 * @return Capacity level (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_hub_get_capacity(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id
);

/**
 * @brief Get executive function efficiency
 *
 * WHAT: Returns efficiency for specific executive function
 * WHY:  Different functions have different metabolic needs
 * HOW:  Based on hub type and metabolic state
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query
 * @param function Executive function type
 * @return Efficiency (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_hub_get_exec_efficiency(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id,
    nvc_hub_exec_function_t function
);

/**
 * @brief Get working memory capacity
 *
 * WHAT: Returns current WM capacity
 * WHY:  WM is metabolically expensive
 * HOW:  Scales with oxygen and metabolic reserve
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query (typically DLPFC)
 * @return WM capacity (items), or -1 on error
 */
NIMCP_EXPORT float nvc_hub_get_wm_capacity(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id
);

/**
 * @brief Get cognitive load estimate
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query
 * @param load Output cognitive load
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_get_cognitive_load(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id,
    nvc_hub_cognitive_load_t* load
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Get pathway state
 *
 * @param bridge Bridge handle
 * @param pathway_id Pathway to query
 * @param state Output pathway state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_get_pathway_state(
    const nvc_hub_bridge_t* bridge,
    uint32_t pathway_id,
    nvc_hub_pathway_state_t* state
);

/**
 * @brief Get effective pathway strength
 *
 * WHAT: Returns metabolically-modulated pathway strength
 * WHY:  Integration depends on hub metabolic state
 * HOW:  Base strength scaled by source/target metabolism
 *
 * @param bridge Bridge handle
 * @param pathway_id Pathway to query
 * @return Effective strength (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_hub_get_pathway_strength(
    const nvc_hub_bridge_t* bridge,
    uint32_t pathway_id
);

/**
 * @brief Get integration bandwidth for hub
 *
 * WHAT: Returns total integration bandwidth
 * WHY:  Bandwidth limits information integration
 * HOW:  Sum of active pathway bandwidths
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query
 * @return Integration bandwidth, or -1 on error
 */
NIMCP_EXPORT float nvc_hub_get_integration_bandwidth(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id
);

//=============================================================================
// Global Workspace API
//=============================================================================

/**
 * @brief Get global workspace state
 *
 * @param bridge Bridge handle
 * @param gw Output global workspace state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_get_gw_state(
    const nvc_hub_bridge_t* bridge,
    nvc_hub_gw_broadcast_t* gw
);

/**
 * @brief Attempt global workspace ignition
 *
 * WHAT: Initiates global broadcast attempt
 * WHY:  Broadcast requires sufficient metabolic resources
 * HOW:  Checks metabolic budget, initiates if possible
 *
 * @param bridge Bridge handle
 * @param hub_id Hub attempting broadcast
 * @param activation_level Activation strength
 * @return 0 on success (ignition started), -1 on failure
 */
NIMCP_EXPORT int nvc_hub_attempt_gw_ignition(
    nvc_hub_bridge_t* bridge,
    uint32_t hub_id,
    float activation_level
);

/**
 * @brief Check if broadcast is metabolically sustainable
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to check
 * @param duration_ms Desired duration
 * @return true if sustainable
 */
NIMCP_EXPORT bool nvc_hub_can_sustain_broadcast(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id,
    float duration_ms
);

//=============================================================================
// Fatigue API
//=============================================================================

/**
 * @brief Get fatigue level
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to query
 * @return Fatigue level (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_hub_get_fatigue(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id
);

/**
 * @brief Check if hub is fatigued
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to check
 * @return true if significantly fatigued
 */
NIMCP_EXPORT bool nvc_hub_is_fatigued(
    const nvc_hub_bridge_t* bridge,
    uint32_t hub_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of cognitive dynamics
 * WHY:  Progress fatigue, GW states, pathway modulation
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_update(
    nvc_hub_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_reset(nvc_hub_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_hub_get_stats(
    const nvc_hub_bridge_t* bridge,
    nvc_hub_stats_t* stats
);

/**
 * @brief Check if any hub has failed capacity
 *
 * @param bridge Bridge handle
 * @return true if any hub in failed state
 */
NIMCP_EXPORT bool nvc_hub_has_capacity_failure(
    const nvc_hub_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_HUB_BRIDGE_H */