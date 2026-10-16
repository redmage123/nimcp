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
// nimcp_ephaptic_hub_bridge.h - Ephaptic to Cognitive Hub Integration Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_hub_bridge.h
 * @brief Bridge between Ephaptic coupling and Cognitive Hub integration system
 *
 * WHAT: Connects ephaptic field dynamics with the cognitive hub system,
 *       enabling field-mediated cross-modal integration and binding.
 *
 * WHY:  Bridges the gap between:
 *       - Ephaptic synchronization (population coordination)
 *       - Cognitive hubs (cross-modal integration nodes)
 *       - Binding by synchrony (feature integration)
 *       Ephaptic coherence provides a mechanism for binding distributed
 *       representations into unified cognitive percepts.
 *
 * HOW:  Integration mechanisms:
 *       1. Phase coherence enables cross-modal binding
 *       2. Gamma synchronization gates hub integration
 *       3. Field patterns route information between hubs
 *       4. Coherence quality determines binding strength
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      COGNITIVE HUBS
 * ─────────────────────────────────────────────────────────────────────
 * Gamma coherence (40 Hz)             -> Binding window (integration)
 * Phase coherence                     -> Binding strength
 * Cross-region synchrony              -> Hub-to-hub communication
 * LFP phase                          -> Integration timing
 * Spatial field pattern               -> Hub routing topology
 * Coherence transitions               -> Binding/unbinding events
 * ```
 *
 * KEY MECHANISMS:
 * - Binding by synchrony: Coherent neurons bind into unified percept
 * - Hub integration: Coherence gates information flow through hubs
 * - Cross-modal binding: Gamma synchrony enables multi-modal integration
 * - Phase-locked routing: Information routes at specific phases
 *
 * REFERENCES:
 * - Fries (2005) "A mechanism for cognitive dynamics: neuronal communication
 *   through neuronal coherence"
 * - Singer & Gray (1995) "Visual feature integration and the temporal
 *   correlation hypothesis"
 * - Varela et al. (2001) "The brainweb: phase synchronization and large-scale integration"
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_HUB_BRIDGE_H
#define NIMCP_EPHAPTIC_HUB_BRIDGE_H

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
#define EPHAPTIC_HUB_MODULE_NAME            "ephaptic_hub_bridge"

/** Maximum cognitive hubs */
#define EPHAPTIC_HUB_MAX_HUBS               32

/** Maximum hub connections */
#define EPHAPTIC_HUB_MAX_CONNECTIONS        256

/** Maximum binding groups */
#define EPHAPTIC_HUB_MAX_BINDINGS           64

/** Maximum modalities */
#define EPHAPTIC_HUB_MAX_MODALITIES         8

/** Default gamma coherence threshold for binding */
#define EPHAPTIC_HUB_DEFAULT_GAMMA_THRESH   0.5f

/** Default binding window duration (ms) */
#define EPHAPTIC_HUB_DEFAULT_BIND_WINDOW    25.0f  /* ~40 Hz gamma */

/** Default phase tolerance for synchrony (radians) */
#define EPHAPTIC_HUB_DEFAULT_PHASE_TOL      0.52f  /* ~30 degrees */

/** Default binding strength decay (ms) */
#define EPHAPTIC_HUB_DEFAULT_BIND_TAU       100.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive hub type
 */
typedef enum {
    EPHAPTIC_HUB_TYPE_SENSORY = 0,            /**< Sensory integration hub */
    EPHAPTIC_HUB_TYPE_MOTOR,                  /**< Motor coordination hub */
    EPHAPTIC_HUB_TYPE_ASSOCIATION,            /**< Association cortex hub */
    EPHAPTIC_HUB_TYPE_EXECUTIVE,              /**< Executive/prefrontal hub */
    EPHAPTIC_HUB_TYPE_MEMORY,                 /**< Memory (hippocampal) hub */
    EPHAPTIC_HUB_TYPE_ATTENTION,              /**< Attention control hub */
    EPHAPTIC_HUB_TYPE_CUSTOM                  /**< Custom hub type */
} ephaptic_hub_type_t;

/**
 * @brief Modality type
 */
typedef enum {
    EPHAPTIC_HUB_MODAL_VISUAL = 0,            /**< Visual modality */
    EPHAPTIC_HUB_MODAL_AUDITORY,              /**< Auditory modality */
    EPHAPTIC_HUB_MODAL_SOMATOSENSORY,         /**< Somatosensory modality */
    EPHAPTIC_HUB_MODAL_MOTOR,                 /**< Motor modality */
    EPHAPTIC_HUB_MODAL_LINGUISTIC,            /**< Linguistic modality */
    EPHAPTIC_HUB_MODAL_SEMANTIC,              /**< Semantic/conceptual */
    EPHAPTIC_HUB_MODAL_EPISODIC               /**< Episodic memory */
} ephaptic_hub_modality_t;

/**
 * @brief Binding mode
 */
typedef enum {
    EPHAPTIC_HUB_BIND_GAMMA = 0,              /**< Gamma coherence binding */
    EPHAPTIC_HUB_BIND_PHASE,                  /**< Phase-lock binding */
    EPHAPTIC_HUB_BIND_BURST,                  /**< Sync burst binding */
    EPHAPTIC_HUB_BIND_COMBINED                /**< Combined binding criteria */
} ephaptic_hub_bind_mode_t;

/**
 * @brief Integration mode
 */
typedef enum {
    EPHAPTIC_HUB_INTEG_GATED = 0,             /**< Coherence-gated integration */
    EPHAPTIC_HUB_INTEG_WEIGHTED,              /**< Coherence-weighted integration */
    EPHAPTIC_HUB_INTEG_PHASE_LOCKED,          /**< Phase-locked integration */
    EPHAPTIC_HUB_INTEG_CONTINUOUS             /**< Continuous integration */
} ephaptic_hub_integ_mode_t;

/**
 * @brief Routing mode between hubs
 */
typedef enum {
    EPHAPTIC_HUB_ROUTE_DIRECT = 0,            /**< Direct hub-to-hub routing */
    EPHAPTIC_HUB_ROUTE_COHERENT,              /**< Route to coherent hubs */
    EPHAPTIC_HUB_ROUTE_PHASE,                 /**< Phase-dependent routing */
    EPHAPTIC_HUB_ROUTE_BROADCAST              /**< Broadcast to all hubs */
} ephaptic_hub_route_mode_t;

/**
 * @brief Binding state
 */
typedef enum {
    EPHAPTIC_HUB_BIND_STATE_UNBOUND = 0,      /**< Not bound */
    EPHAPTIC_HUB_BIND_STATE_FORMING,          /**< Binding forming */
    EPHAPTIC_HUB_BIND_STATE_BOUND,            /**< Actively bound */
    EPHAPTIC_HUB_BIND_STATE_DISSOLVING        /**< Binding dissolving */
} ephaptic_hub_bind_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-hub bridge
 */
typedef struct {
    /** Binding parameters */
    ephaptic_hub_bind_mode_t bind_mode;        /**< Binding mode */
    float gamma_coherence_threshold;            /**< Gamma threshold for binding */
    float phase_tolerance;                      /**< Phase tolerance (radians) */
    float binding_window_ms;                    /**< Binding window duration */
    float binding_decay_tau_ms;                 /**< Binding strength decay */

    /** Integration parameters */
    ephaptic_hub_integ_mode_t integ_mode;      /**< Integration mode */
    float integration_threshold;                /**< Coherence for integration */
    float integration_rate;                     /**< Integration speed */

    /** Routing parameters */
    ephaptic_hub_route_mode_t route_mode;      /**< Inter-hub routing mode */
    float route_coherence_threshold;            /**< Coherence for routing */
    float route_phase_window;                   /**< Phase window for routing */

    /** Cross-modal parameters */
    bool enable_cross_modal;                    /**< Enable cross-modal binding */
    float cross_modal_threshold;                /**< Cross-modal coherence threshold */
    float cross_modal_weight;                   /**< Cross-modal binding weight */

    /** Update parameters */
    float update_interval_ms;                   /**< Bridge update interval */
} ephaptic_hub_config_t;

/**
 * @brief Cognitive hub state
 */
typedef struct {
    uint32_t hub_id;                            /**< Hub identifier */
    ephaptic_hub_type_t type;                  /**< Hub type */
    uint32_t modalities;                        /**< Bitmask of modalities */
    float position[3];                          /**< Hub location (mm) */
    float coherence;                            /**< Local coherence */
    float gamma_power;                          /**< Local gamma power */
    float lfp_phase;                            /**< Local LFP phase */
    float integration_level;                    /**< Current integration [0,1] */
    uint32_t active_bindings;                   /**< Number of active bindings */
    bool is_integrating;                        /**< Currently integrating */
} ephaptic_hub_state_t;

/**
 * @brief Binding group (set of bound representations)
 */
typedef struct {
    uint32_t binding_id;                        /**< Binding identifier */
    ephaptic_hub_bind_state_t state;           /**< Binding state */
    uint32_t hub_ids[8];                        /**< Participating hubs */
    uint32_t hub_count;                         /**< Number of hubs */
    uint32_t modalities;                        /**< Bound modalities (bitmask) */
    float coherence;                            /**< Binding coherence */
    float strength;                             /**< Binding strength [0,1] */
    float formation_time_ms;                    /**< When binding formed */
    float duration_ms;                          /**< Duration so far */
    float mean_phase;                           /**< Mean phase of group */
} ephaptic_hub_binding_t;

/**
 * @brief Hub-to-hub connection
 */
typedef struct {
    uint32_t source_hub;                        /**< Source hub ID */
    uint32_t target_hub;                        /**< Target hub ID */
    float coherence;                            /**< Connection coherence */
    float phase_lag;                            /**< Phase lag (radians) */
    float weight;                               /**< Connection weight */
    bool active;                                /**< Is connection active */
} ephaptic_hub_connection_t;

/**
 * @brief Integration event
 */
typedef struct {
    float event_time_ms;                        /**< Event timestamp */
    uint32_t hub_id;                            /**< Hub that integrated */
    uint32_t modalities_integrated;             /**< Modalities integrated */
    float coherence_at_event;                   /**< Coherence level */
    float gamma_power;                          /**< Gamma power */
    uint32_t binding_count;                     /**< Bindings involved */
    float integration_strength;                 /**< Strength of integration */
} ephaptic_hub_integ_event_t;

/**
 * @brief Routing decision
 */
typedef struct {
    uint32_t source_hub;                        /**< Source hub */
    uint32_t target_hubs[8];                    /**< Target hubs */
    uint32_t target_count;                      /**< Number of targets */
    float route_weights[8];                     /**< Per-target weights */
    bool phase_gated;                           /**< Is phase-gated */
    float target_phase;                         /**< Target phase if gated */
} ephaptic_hub_routing_t;

/**
 * @brief Cross-modal binding result
 */
typedef struct {
    bool binding_formed;                        /**< Was binding formed */
    uint32_t binding_id;                        /**< Binding ID if formed */
    uint32_t modality_a;                        /**< First modality */
    uint32_t modality_b;                        /**< Second modality */
    float cross_coherence;                      /**< Cross-modal coherence */
    float phase_difference;                     /**< Phase difference */
    float binding_strength;                     /**< Binding strength */
} ephaptic_hub_cross_modal_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t bindings_formed;                   /**< Total bindings formed */
    uint64_t bindings_dissolved;                /**< Bindings dissolved */
    uint64_t integrations;                      /**< Integration events */
    uint64_t cross_modal_bindings;              /**< Cross-modal bindings */
    uint64_t routes_computed;                   /**< Routing decisions */
    float avg_binding_duration_ms;              /**< Average binding duration */
    float avg_binding_strength;                 /**< Average binding strength */
    float avg_coherence;                        /**< Average coherence */
    float avg_gamma_power;                      /**< Average gamma power */
    float last_update_ms;                       /**< Last update timestamp */
} ephaptic_hub_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_hub_bridge_struct ephaptic_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biologically plausible hub parameters
 * HOW:  Based on binding-by-synchrony literature
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_default_config(ephaptic_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-hub bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable ephaptic-mediated cognitive integration
 * HOW:  Sets up hubs, bindings, routing tables
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_hub_bridge_t* ephaptic_hub_bridge_create(
    const ephaptic_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_hub_bridge_destroy(ephaptic_hub_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_bridge_reset(ephaptic_hub_bridge_t* bridge);

//=============================================================================
// Hub Management API
//=============================================================================

/**
 * @brief Register cognitive hub
 *
 * WHAT: Adds hub to bridge for integration
 * WHY:  Track ephaptic state per hub
 * HOW:  Stores hub with type and modalities
 *
 * @param bridge     Bridge handle
 * @param hub_id     Hub identifier
 * @param type       Hub type
 * @param modalities Bitmask of modalities (1 << ephaptic_hub_modality_t)
 * @param position   Hub position (mm)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_register(
    ephaptic_hub_bridge_t* bridge,
    uint32_t hub_id,
    ephaptic_hub_type_t type,
    uint32_t modalities,
    const float position[3]
);

/**
 * @brief Get hub state
 *
 * @param bridge Bridge handle
 * @param hub_id Hub identifier
 * @param state  Output hub state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_get_state(
    const ephaptic_hub_bridge_t* bridge,
    uint32_t hub_id,
    ephaptic_hub_state_t* state
);

/**
 * @brief Update hub ephaptic state
 *
 * WHAT: Updates ephaptic measurements for hub
 * WHY:  Provide current state for binding/integration
 * HOW:  Updates coherence, gamma, phase
 *
 * @param bridge      Bridge handle
 * @param hub_id      Hub identifier
 * @param coherence   Current coherence
 * @param gamma_power Current gamma power
 * @param lfp_phase   Current LFP phase
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_update_state(
    ephaptic_hub_bridge_t* bridge,
    uint32_t hub_id,
    float coherence,
    float gamma_power,
    float lfp_phase
);

/**
 * @brief Connect hubs
 *
 * WHAT: Creates connection between hubs
 * WHY:  Define hub network topology
 * HOW:  Stores bidirectional connection
 *
 * @param bridge     Bridge handle
 * @param hub_a      First hub ID
 * @param hub_b      Second hub ID
 * @param weight     Connection weight [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_connect(
    ephaptic_hub_bridge_t* bridge,
    uint32_t hub_a,
    uint32_t hub_b,
    float weight
);

//=============================================================================
// Binding API
//=============================================================================

/**
 * @brief Check for binding opportunity
 *
 * WHAT: Determines if binding conditions are met
 * WHY:  Detect when representations can bind
 * HOW:  Checks coherence, gamma, phase alignment
 *
 * BIOLOGICAL: Binding by synchrony occurs when gamma oscillations
 * are coherent across representations to be bound together.
 *
 * @param bridge        Bridge handle
 * @param hub_ids       Hubs to potentially bind
 * @param hub_count     Number of hubs
 * @param can_bind      Output: can binding occur
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_check_binding(
    ephaptic_hub_bridge_t* bridge,
    const uint32_t* hub_ids,
    uint32_t hub_count,
    bool* can_bind
);

/**
 * @brief Form binding between hubs
 *
 * WHAT: Creates binding group from coherent hubs
 * WHY:  Bind representations into unified percept
 * HOW:  Creates binding record, tracks coherence
 *
 * @param bridge    Bridge handle
 * @param hub_ids   Hubs to bind
 * @param hub_count Number of hubs
 * @param binding   Output binding record
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_form_binding(
    ephaptic_hub_bridge_t* bridge,
    const uint32_t* hub_ids,
    uint32_t hub_count,
    ephaptic_hub_binding_t* binding
);

/**
 * @brief Get binding state
 *
 * @param bridge     Bridge handle
 * @param binding_id Binding identifier
 * @param binding    Output binding state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_get_binding(
    const ephaptic_hub_bridge_t* bridge,
    uint32_t binding_id,
    ephaptic_hub_binding_t* binding
);

/**
 * @brief Get active binding count
 *
 * @param bridge Bridge handle
 * @return Number of active bindings, or 0 on error
 */
NIMCP_EXPORT uint32_t ephaptic_hub_get_binding_count(
    const ephaptic_hub_bridge_t* bridge
);

/**
 * @brief Dissolve binding
 *
 * WHAT: Dissolves an active binding
 * WHY:  Unbind representations when coherence fails
 * HOW:  Marks binding as dissolving, then unbound
 *
 * @param bridge     Bridge handle
 * @param binding_id Binding to dissolve
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_dissolve_binding(
    ephaptic_hub_bridge_t* bridge,
    uint32_t binding_id
);

//=============================================================================
// Cross-Modal Binding API
//=============================================================================

/**
 * @brief Check for cross-modal binding
 *
 * WHAT: Checks if cross-modal binding is possible
 * WHY:  Multi-sensory integration via coherence
 * HOW:  Checks cross-frequency coherence between modalities
 *
 * @param bridge     Bridge handle
 * @param modality_a First modality
 * @param modality_b Second modality
 * @param result     Output cross-modal result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_check_cross_modal(
    ephaptic_hub_bridge_t* bridge,
    ephaptic_hub_modality_t modality_a,
    ephaptic_hub_modality_t modality_b,
    ephaptic_hub_cross_modal_t* result
);

/**
 * @brief Form cross-modal binding
 *
 * WHAT: Creates binding across modalities
 * WHY:  Integrate multi-sensory information
 * HOW:  Links hubs from different modalities
 *
 * @param bridge     Bridge handle
 * @param modality_a First modality
 * @param modality_b Second modality
 * @param binding    Output binding record
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_form_cross_modal(
    ephaptic_hub_bridge_t* bridge,
    ephaptic_hub_modality_t modality_a,
    ephaptic_hub_modality_t modality_b,
    ephaptic_hub_binding_t* binding
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Process integration at hub
 *
 * WHAT: Performs coherence-gated integration
 * WHY:  Integrate bound information at hub
 * HOW:  Weighted sum based on binding strength
 *
 * @param bridge Bridge handle
 * @param hub_id Hub to integrate at
 * @param event  Output integration event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_integrate(
    ephaptic_hub_bridge_t* bridge,
    uint32_t hub_id,
    ephaptic_hub_integ_event_t* event
);

/**
 * @brief Get integration level at hub
 *
 * @param bridge    Bridge handle
 * @param hub_id    Hub identifier
 * @param level     Output: integration level [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_get_integration_level(
    const ephaptic_hub_bridge_t* bridge,
    uint32_t hub_id,
    float* level
);

//=============================================================================
// Routing API
//=============================================================================

/**
 * @brief Compute routing from hub
 *
 * WHAT: Determines routing from source hub
 * WHY:  Phase-coherent information routing
 * HOW:  Routes to coherent connected hubs
 *
 * @param bridge    Bridge handle
 * @param source_id Source hub ID
 * @param lfp_phase Current LFP phase
 * @param routing   Output routing decision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_compute_routing(
    ephaptic_hub_bridge_t* bridge,
    uint32_t source_id,
    float lfp_phase,
    ephaptic_hub_routing_t* routing
);

/**
 * @brief Get connection state
 *
 * @param bridge     Bridge handle
 * @param source_hub Source hub ID
 * @param target_hub Target hub ID
 * @param connection Output connection state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_get_connection(
    const ephaptic_hub_bridge_t* bridge,
    uint32_t source_hub,
    uint32_t target_hub,
    ephaptic_hub_connection_t* connection
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay bindings, update coherence, process events
 * HOW:  Called during simulation step
 *
 * @param bridge  Bridge handle
 * @param dt_ms   Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_update(
    ephaptic_hub_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update all bindings
 *
 * WHAT: Updates binding strengths based on coherence
 * WHY:  Bindings should strengthen or weaken with coherence
 * HOW:  Applies decay, checks dissolution thresholds
 *
 * @param bridge Bridge handle
 * @return Number of bindings updated, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_update_bindings(ephaptic_hub_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats  Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_get_stats(
    const ephaptic_hub_bridge_t* bridge,
    ephaptic_hub_stats_t* stats
);

/**
 * @brief Get average binding strength
 *
 * @param bridge   Bridge handle
 * @param strength Output: average binding strength
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_hub_get_avg_binding_strength(
    const ephaptic_hub_bridge_t* bridge,
    float* strength
);

/**
 * @brief Get hub count
 *
 * @param bridge Bridge handle
 * @return Number of registered hubs, or 0 on error
 */
NIMCP_EXPORT uint32_t ephaptic_hub_get_hub_count(
    const ephaptic_hub_bridge_t* bridge
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active
 */
NIMCP_EXPORT bool ephaptic_hub_is_active(const ephaptic_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_HUB_BRIDGE_H */