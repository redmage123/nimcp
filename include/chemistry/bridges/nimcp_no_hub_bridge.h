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
// nimcp_no_hub_bridge.h - Nitric Oxide to Cognitive Hub Integration Bridge
//=============================================================================
/**
 * @file nimcp_no_hub_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and the cognitive
 *        hub for system-wide neural coordination
 *
 * WHAT: Connects NO gasotransmitter signaling with the cognitive hub to enable
 *       NO-mediated coordination across brain regions and cognitive systems.
 *
 * WHY:  Nitric oxide is ideal for cognitive hub integration:
 *       - Volume transmission enables brain-wide coordination
 *       - Multiple NOS isoforms provide diverse signaling modes
 *       - Rapid diffusion enables fast state changes
 *       - Vascular coupling links cognition to metabolism
 *       - cGMP pathway integrates with many cognitive processes
 *
 * HOW:  Hub integration pathways:
 *       1. Hub → NO: Cognitive demands trigger NO signaling patterns
 *       2. NO → Hub: NO state modulates hub routing/processing
 *       3. Regional: NO coordinates between brain regions
 *       4. Global: System-wide NO levels indicate cognitive load
 *
 * ARCHITECTURAL BASIS:
 * ```
 * COGNITIVE HUB                            NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Global Workspace access                ← NO gates cortical broadcasting
 * Attention allocation                   ← NO priority signaling
 * Cognitive load monitoring              → NO production tracks demand
 * Cross-region coordination              ← NO volume transmission
 * Metabolic resource allocation          ← eNOS vasodilation
 * State transitions                      ← NO triggers mode switches
 * ```
 *
 * HUB INTEGRATION POINTS:
 * - Global Workspace: NO gates access to conscious processing
 * - Attention System: NO modulates attention weights
 * - Executive Control: NO coordinates prefrontal regions
 * - Memory Systems: NO facilitates consolidation
 * - Emotional Systems: NO modulates limbic processing
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_HUB_BRIDGE_H
#define NIMCP_NO_HUB_BRIDGE_H

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
#define NO_HUB_MODULE_NAME              "no_hub_bridge"

/** Maximum hub nodes */
#define NO_HUB_MAX_NODES                128

/** Maximum regions */
#define NO_HUB_MAX_REGIONS              32

/** Maximum concurrent broadcasts */
#define NO_HUB_MAX_BROADCASTS           64

/** Default global workspace NO threshold */
#define NO_HUB_GW_THRESHOLD             40.0f   /* nM */

/** Default cognitive load NO scaling */
#define NO_HUB_LOAD_SCALE               0.01f   /* NO per load unit */

/** Bio-async module ID */
#define BIO_MODULE_NO_HUB               0x0E07

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive hub region type
 */
typedef enum {
    NO_HUB_REGION_PREFRONTAL = 0,       /**< Prefrontal cortex (executive) */
    NO_HUB_REGION_PARIETAL,             /**< Parietal cortex (attention) */
    NO_HUB_REGION_TEMPORAL,             /**< Temporal lobe (memory/language) */
    NO_HUB_REGION_OCCIPITAL,            /**< Occipital cortex (visual) */
    NO_HUB_REGION_LIMBIC,               /**< Limbic system (emotion) */
    NO_HUB_REGION_BASAL_GANGLIA,        /**< Basal ganglia (action selection) */
    NO_HUB_REGION_THALAMUS,             /**< Thalamus (relay) */
    NO_HUB_REGION_CEREBELLUM,           /**< Cerebellum (coordination) */
    NO_HUB_REGION_BRAINSTEM             /**< Brainstem (arousal) */
} no_hub_region_t;

/**
 * @brief Hub coordination mode
 */
typedef enum {
    NO_HUB_MODE_IDLE = 0,               /**< Low cognitive demand */
    NO_HUB_MODE_MONITORING,             /**< Background monitoring */
    NO_HUB_MODE_FOCUSED,                /**< Focused processing */
    NO_HUB_MODE_MULTITASK,              /**< Parallel processing */
    NO_HUB_MODE_INTEGRATION             /**< Cross-region integration */
} no_hub_mode_t;

/**
 * @brief Global workspace access state
 */
typedef enum {
    NO_HUB_GW_BLOCKED = 0,              /**< No GW access */
    NO_HUB_GW_COMPETING,                /**< Competing for access */
    NO_HUB_GW_BROADCASTING,             /**< Active broadcast */
    NO_HUB_GW_RECEIVING                 /**< Receiving broadcast */
} no_hub_gw_state_t;

/**
 * @brief NO broadcast type for hub coordination
 */
typedef enum {
    NO_HUB_BROADCAST_ALERT = 0,         /**< Attention alert */
    NO_HUB_BROADCAST_SYNC,              /**< Synchronization signal */
    NO_HUB_BROADCAST_RESOURCE,          /**< Resource allocation */
    NO_HUB_BROADCAST_TRANSITION,        /**< State transition */
    NO_HUB_BROADCAST_EMERGENCY          /**< Emergency/priority override */
} no_hub_broadcast_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-Hub bridge
 */
typedef struct {
    /** Global Workspace coupling */
    float gw_no_threshold_nm;                /**< NO for GW access */
    float gw_broadcast_strength;             /**< NO broadcast amplitude */
    float gw_competition_decay;              /**< Competition decay rate */

    /** Cognitive load mapping */
    float load_to_no_scale;                  /**< Load to NO production */
    float load_saturation;                   /**< NO saturation at high load */
    bool enable_load_feedback;               /**< Load modulates NO */

    /** Regional coordination */
    bool enable_regional_sync;               /**< Enable NO-based sync */
    float sync_radius_um;                    /**< Synchronization radius */
    float cross_region_delay_ms;             /**< Inter-region delay */

    /** Attention/priority */
    float attention_no_scale;                /**< Attention weight scaling */
    float priority_threshold_nm;             /**< Priority override threshold */

    /** Vascular/metabolic */
    bool enable_metabolic_coupling;          /**< Enable vascular effects */
    float metabolic_allocation_weight;       /**< Blood flow allocation */

    /** Features */
    bool enable_bio_async;                   /**< Bio-async messaging */
    float update_interval_ms;                /**< Update interval */
} no_hub_config_t;

/**
 * @brief Hub node with NO modulation
 */
typedef struct {
    uint32_t node_id;                        /**< Node ID */
    no_hub_region_t region;                  /**< Brain region */
    float position[3];                       /**< Position (um) */

    /** NO state */
    float local_no_nm;                       /**< Local NO concentration */
    float no_production_rate;                /**< Current NO production */
    uint32_t no_source_id;                   /**< Associated NO source */

    /** Hub state */
    no_hub_mode_t mode;                      /**< Current coordination mode */
    no_hub_gw_state_t gw_state;              /**< Global Workspace state */
    float cognitive_load;                    /**< Current cognitive load */

    /** Routing */
    float attention_weight;                  /**< Attention weight (0-1) */
    float broadcast_strength;                /**< Broadcast signal strength */
    float receive_sensitivity;               /**< Receive sensitivity */

    /** Connectivity */
    uint32_t connected_nodes[16];            /**< Connected hub nodes */
    float connection_weights[16];            /**< Connection strengths */
    uint32_t num_connections;

    /** Metabolic */
    float metabolic_demand;                  /**< Metabolic requirement */
    float blood_flow_factor;                 /**< Vasodilation factor */

    bool active;
} no_hub_node_t;

/**
 * @brief Regional coordination state
 */
typedef struct {
    no_hub_region_t region;                  /**< Region type */

    /** NO state */
    float mean_no_nm;                        /**< Mean NO in region */
    float no_gradient;                       /**< NO spatial gradient */
    float vasodilation;                      /**< Regional blood flow */

    /** Hub metrics */
    float cognitive_load;                    /**< Regional load */
    float attention_weight;                  /**< Regional attention */
    no_hub_mode_t dominant_mode;             /**< Dominant mode */

    /** Synchronization */
    float sync_coherence;                    /**< Sync with other regions */
    uint32_t active_nodes;                   /**< Active nodes in region */
    uint32_t broadcasting_nodes;             /**< Nodes broadcasting */
} no_hub_region_state_t;

/**
 * @brief Hub broadcast event
 */
typedef struct {
    uint32_t broadcast_id;                   /**< Broadcast ID */
    uint32_t source_node;                    /**< Originating node */
    no_hub_region_t source_region;           /**< Source region */
    no_hub_broadcast_t type;                 /**< Broadcast type */

    /** Signal */
    float no_amplitude;                      /**< NO signal amplitude */
    float propagation_radius_um;             /**< Propagation radius */
    float decay_rate;                        /**< Signal decay */

    /** Coverage */
    uint32_t target_nodes[NO_HUB_MAX_NODES]; /**< Nodes reached */
    uint32_t num_targets;                    /**< Count of targets */

    float start_time_ms;                     /**< Broadcast start */
    float duration_ms;                       /**< Expected duration */
    bool active;
} no_hub_broadcast_event_t;

/**
 * @brief Hub coordination event
 */
typedef struct {
    uint32_t event_id;                       /**< Event ID */
    uint32_t node_id;                        /**< Node involved */

    /** Event type */
    bool mode_changed;                       /**< Mode transition */
    bool gw_access_changed;                  /**< GW state changed */
    bool broadcast_sent;                     /**< Broadcast initiated */
    bool broadcast_received;                 /**< Broadcast received */

    /** Details */
    no_hub_mode_t new_mode;                  /**< New mode (if changed) */
    no_hub_gw_state_t new_gw_state;          /**< New GW state */
    float no_level;                          /**< NO at event */
    float cognitive_load;                    /**< Load at event */

    float event_time_ms;
} no_hub_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;                  /**< Total updates */
    uint64_t mode_transitions;               /**< Mode changes */
    uint64_t gw_accesses;                    /**< GW access events */
    uint64_t broadcasts_sent;                /**< Broadcasts initiated */
    uint64_t broadcasts_received;            /**< Broadcasts received */
    uint64_t sync_events;                    /**< Synchronization events */

    uint32_t active_nodes;                   /**< Active hub nodes */
    uint32_t broadcasting_nodes;             /**< Currently broadcasting */
    uint32_t focused_nodes;                  /**< In focused mode */

    float mean_cognitive_load;               /**< System mean load */
    float mean_no_level;                     /**< System mean NO */
    float system_coherence;                  /**< Global synchronization */
    float total_vasodilation;                /**< Total blood flow */

    float last_update_ms;
} no_hub_stats_t;

/** Opaque bridge handle */
typedef struct no_hub_bridge_struct no_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_default_config(no_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-Hub bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_hub_bridge_t* no_hub_bridge_create(
    const no_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_hub_bridge_destroy(no_hub_bridge_t* bridge);

//=============================================================================
// Node Management API
//=============================================================================

/**
 * @brief Register hub node
 *
 * WHAT: Adds node to cognitive hub with NO modulation
 * WHY:  Hub nodes participate in NO-mediated coordination
 * HOW:  Creates node with region assignment and position
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param region Brain region
 * @param position 3D position [x, y, z] in um
 * @param no_source_id Associated NO source (or 0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_register_node(
    no_hub_bridge_t* bridge,
    uint32_t node_id,
    no_hub_region_t region,
    const float position[3],
    uint32_t no_source_id
);

/**
 * @brief Unregister hub node
 *
 * @param bridge Bridge handle
 * @param node_id Node to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_unregister_node(
    no_hub_bridge_t* bridge,
    uint32_t node_id
);

/**
 * @brief Connect hub nodes
 *
 * WHAT: Creates connection between hub nodes
 * WHY:  Nodes coordinate via connections
 * HOW:  Adds bidirectional weighted connection
 *
 * @param bridge Bridge handle
 * @param node_a First node ID
 * @param node_b Second node ID
 * @param weight Connection weight (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_connect_nodes(
    no_hub_bridge_t* bridge,
    uint32_t node_a,
    uint32_t node_b,
    float weight
);

//=============================================================================
// NO State API
//=============================================================================

/**
 * @brief Set NO level for node
 *
 * WHAT: Updates NO concentration at hub node
 * WHY:  NO modulates hub processing and coordination
 * HOW:  Updates node state, triggers mode evaluation
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param no_level_nm NO concentration (nM)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_set_no_level(
    no_hub_bridge_t* bridge,
    uint32_t node_id,
    float no_level_nm
);

/**
 * @brief Set cognitive load for node
 *
 * WHAT: Updates cognitive load which drives NO production
 * WHY:  High load triggers more NO for coordination
 * HOW:  Sets load, which modulates NO production rate
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param load Cognitive load (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_set_cognitive_load(
    no_hub_bridge_t* bridge,
    uint32_t node_id,
    float load
);

//=============================================================================
// Global Workspace API
//=============================================================================

/**
 * @brief Request Global Workspace access
 *
 * WHAT: Node requests access to broadcast via GW
 * WHY:  GW access requires sufficient NO-mediated priority
 * HOW:  Enters competition based on NO level and load
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param priority Request priority (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_request_gw_access(
    no_hub_bridge_t* bridge,
    uint32_t node_id,
    float priority
);

/**
 * @brief Get Global Workspace state for node
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @return Current GW state
 */
NIMCP_EXPORT no_hub_gw_state_t no_hub_get_gw_state(
    const no_hub_bridge_t* bridge,
    uint32_t node_id
);

/**
 * @brief Check if node is broadcasting to GW
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @return true if broadcasting
 */
NIMCP_EXPORT bool no_hub_is_broadcasting(
    const no_hub_bridge_t* bridge,
    uint32_t node_id
);

//=============================================================================
// Broadcasting API
//=============================================================================

/**
 * @brief Initiate NO-mediated broadcast
 *
 * WHAT: Sends coordination signal via NO volume transmission
 * WHY:  NO enables rapid brain-wide signaling
 * HOW:  Creates broadcast event with propagation
 *
 * @param bridge Bridge handle
 * @param source_node Source node ID
 * @param type Broadcast type
 * @param amplitude Signal amplitude
 * @param[out] broadcast_id Assigned broadcast ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_broadcast(
    no_hub_bridge_t* bridge,
    uint32_t source_node,
    no_hub_broadcast_t type,
    float amplitude,
    uint32_t* broadcast_id
);

/**
 * @brief Get active broadcast information
 *
 * @param bridge Bridge handle
 * @param broadcast_id Broadcast ID
 * @param[out] event Broadcast event details
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_get_broadcast(
    const no_hub_bridge_t* bridge,
    uint32_t broadcast_id,
    no_hub_broadcast_event_t* event
);

//=============================================================================
// Regional Coordination API
//=============================================================================

/**
 * @brief Get regional state
 *
 * WHAT: Returns aggregated state for brain region
 * WHY:  Regional coordination is key hub function
 * HOW:  Aggregates across all nodes in region
 *
 * @param bridge Bridge handle
 * @param region Region type
 * @param[out] state Regional state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_get_region_state(
    const no_hub_bridge_t* bridge,
    no_hub_region_t region,
    no_hub_region_state_t* state
);

/**
 * @brief Get cross-region synchronization
 *
 * WHAT: Returns sync coherence between two regions
 * WHY:  NO helps coordinate between distant regions
 * HOW:  Measures NO-mediated phase coherence
 *
 * @param bridge Bridge handle
 * @param region_a First region
 * @param region_b Second region
 * @param[out] coherence Synchronization coherence (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_get_cross_region_sync(
    const no_hub_bridge_t* bridge,
    no_hub_region_t region_a,
    no_hub_region_t region_b,
    float* coherence
);

//=============================================================================
// Mode API
//=============================================================================

/**
 * @brief Get current coordination mode for node
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @return Current mode
 */
NIMCP_EXPORT no_hub_mode_t no_hub_get_mode(
    const no_hub_bridge_t* bridge,
    uint32_t node_id
);

/**
 * @brief Force mode transition (override)
 *
 * WHAT: Forces node to specific coordination mode
 * WHY:  Allow external mode control
 * HOW:  Sets mode directly, adjusts NO accordingly
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param mode Target mode
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_set_mode(
    no_hub_bridge_t* bridge,
    uint32_t node_id,
    no_hub_mode_t mode
);

/**
 * @brief Get system-wide dominant mode
 *
 * @param bridge Bridge handle
 * @return Dominant system mode
 */
NIMCP_EXPORT no_hub_mode_t no_hub_get_system_mode(
    const no_hub_bridge_t* bridge
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of NO-hub integration
 * WHY:  Advance coordination, broadcasts, GW competition
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_update(
    no_hub_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_reset(no_hub_bridge_t* bridge);

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
NIMCP_EXPORT int no_hub_get_stats(
    const no_hub_bridge_t* bridge,
    no_hub_stats_t* stats
);

/**
 * @brief Get node state
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param[out] node Node state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_get_node(
    const no_hub_bridge_t* bridge,
    uint32_t node_id,
    no_hub_node_t* node
);

/**
 * @brief Get system coherence (global sync measure)
 *
 * @param bridge Bridge handle
 * @param[out] coherence System coherence (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_get_system_coherence(
    const no_hub_bridge_t* bridge,
    float* coherence
);

/**
 * @brief Get attention weight for node
 *
 * @param bridge Bridge handle
 * @param node_id Node ID
 * @param[out] weight Attention weight
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_hub_get_attention_weight(
    const no_hub_bridge_t* bridge,
    uint32_t node_id,
    float* weight
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_HUB_BRIDGE_H */