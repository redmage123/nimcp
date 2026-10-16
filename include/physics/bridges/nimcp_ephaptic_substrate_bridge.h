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
// nimcp_ephaptic_substrate_bridge.h - Ephaptic to Bio-Async Substrate Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_substrate_bridge.h
 * @brief Low-level substrate bridge between Ephaptic coupling and Bio-async messaging
 *
 * WHAT: Provides substrate-level integration between ephaptic field dynamics
 *       and the bio-async messaging infrastructure, enabling field-aware
 *       message routing, priority, and timing.
 *
 * WHY:  Bridges the gap between:
 *       - Physical ephaptic field propagation
 *       - Logical message passing infrastructure
 *       - Timing-sensitive neural communication
 *       The substrate bridge ensures that message timing and routing
 *       respect the underlying ephaptic field dynamics.
 *
 * HOW:  Integration mechanisms:
 *       1. Field strength modulates message priority
 *       2. Phase coherence determines message grouping
 *       3. Spatial field patterns guide routing topology
 *       4. Sync events trigger message bursts
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      BIO-ASYNC SUBSTRATE
 * ─────────────────────────────────────────────────────────────────────
 * Electric field strength             -> Message priority weighting
 * Phase coherence                     -> Message bundling/grouping
 * Spatial field gradient              -> Routing direction bias
 * LFP phase                          -> Message timing synchronization
 * Synchronization events              -> Burst message transmission
 * Field decay (spatial)               -> Message TTL computation
 * ```
 *
 * KEY MECHANISMS:
 * - Field-priority mapping: Strong fields = high priority messages
 * - Coherence bundling: Coherent sources bundle messages together
 * - Gradient routing: Field gradients influence routing decisions
 * - Phase-locked timing: Messages align with LFP phase for synchrony
 *
 * REFERENCES:
 * - Buzsaki (2006) "Rhythms of the Brain" - oscillation-based communication
 * - Fries (2015) "Rhythms for cognition" - communication through coherence
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_SUBSTRATE_BRIDGE_H
#define NIMCP_EPHAPTIC_SUBSTRATE_BRIDGE_H

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
#define EPHAPTIC_SUBSTRATE_MODULE_NAME      "ephaptic_substrate_bridge"

/** Maximum queued messages */
#define EPHAPTIC_SUBSTRATE_MAX_QUEUE        1024

/** Maximum message bundles */
#define EPHAPTIC_SUBSTRATE_MAX_BUNDLES      128

/** Maximum routing targets */
#define EPHAPTIC_SUBSTRATE_MAX_TARGETS      256

/** Default priority scaling factor */
#define EPHAPTIC_SUBSTRATE_DEFAULT_PRIORITY_SCALE   2.0f

/** Default coherence threshold for bundling */
#define EPHAPTIC_SUBSTRATE_DEFAULT_BUNDLE_THRESHOLD 0.6f

/** Default phase window for timing alignment (radians) */
#define EPHAPTIC_SUBSTRATE_DEFAULT_PHASE_WINDOW     0.52f

/** Default base TTL (ms) */
#define EPHAPTIC_SUBSTRATE_DEFAULT_BASE_TTL         100.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Message priority mode
 */
typedef enum {
    EPHAPTIC_SUB_PRIORITY_FIXED = 0,      /**< Fixed priority */
    EPHAPTIC_SUB_PRIORITY_FIELD,          /**< Priority from field strength */
    EPHAPTIC_SUB_PRIORITY_COHERENCE,      /**< Priority from coherence */
    EPHAPTIC_SUB_PRIORITY_COMBINED        /**< Combined field + coherence */
} ephaptic_substrate_priority_mode_t;

/**
 * @brief Message bundling mode
 */
typedef enum {
    EPHAPTIC_SUB_BUNDLE_NONE = 0,         /**< No bundling */
    EPHAPTIC_SUB_BUNDLE_COHERENT,         /**< Bundle by coherence */
    EPHAPTIC_SUB_BUNDLE_SPATIAL,          /**< Bundle by spatial proximity */
    EPHAPTIC_SUB_BUNDLE_PHASE             /**< Bundle by phase alignment */
} ephaptic_substrate_bundle_mode_t;

/**
 * @brief Routing mode based on field
 */
typedef enum {
    EPHAPTIC_SUB_ROUTE_BROADCAST = 0,     /**< Broadcast to all */
    EPHAPTIC_SUB_ROUTE_GRADIENT,          /**< Route along field gradient */
    EPHAPTIC_SUB_ROUTE_COHERENT,          /**< Route to coherent targets */
    EPHAPTIC_SUB_ROUTE_NEAREST            /**< Route to spatially nearest */
} ephaptic_substrate_route_mode_t;

/**
 * @brief Timing synchronization mode
 */
typedef enum {
    EPHAPTIC_SUB_TIMING_ASYNC = 0,        /**< Asynchronous (no sync) */
    EPHAPTIC_SUB_TIMING_PHASE_LOCK,       /**< Phase-locked to LFP */
    EPHAPTIC_SUB_TIMING_BURST,            /**< Burst on sync events */
    EPHAPTIC_SUB_TIMING_GATED             /**< Gated by coherence */
} ephaptic_substrate_timing_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-substrate bridge
 */
typedef struct {
    /** Priority configuration */
    ephaptic_substrate_priority_mode_t priority_mode; /**< Priority computation */
    float priority_scale;                              /**< Field->priority scale */
    float min_priority;                                /**< Minimum priority */
    float max_priority;                                /**< Maximum priority */

    /** Bundling configuration */
    ephaptic_substrate_bundle_mode_t bundle_mode;     /**< Bundling mode */
    float coherence_bundle_threshold;                  /**< Coherence for bundling */
    float spatial_bundle_radius;                       /**< Spatial radius (mm) */
    float phase_bundle_window;                         /**< Phase window (rad) */
    uint32_t max_bundle_size;                          /**< Max messages per bundle */

    /** Routing configuration */
    ephaptic_substrate_route_mode_t route_mode;       /**< Routing mode */
    float gradient_threshold;                          /**< Min gradient for routing */
    float coherence_route_threshold;                   /**< Coherence for route target */

    /** Timing configuration */
    ephaptic_substrate_timing_mode_t timing_mode;     /**< Timing sync mode */
    float phase_lock_target;                           /**< Target phase (rad) */
    float phase_lock_window;                           /**< Phase window (rad) */
    float burst_coherence_threshold;                   /**< Coherence for burst */

    /** TTL configuration */
    float base_ttl_ms;                                 /**< Base message TTL */
    float field_ttl_scale;                             /**< Field->TTL scaling */
    bool adaptive_ttl;                                 /**< Adapt TTL to conditions */

    /** Update parameters */
    float update_interval_ms;                          /**< Bridge update interval */
} ephaptic_substrate_config_t;

/**
 * @brief Message with ephaptic metadata
 */
typedef struct {
    uint32_t message_id;                               /**< Message identifier */
    uint32_t source_id;                                /**< Source module ID */
    uint32_t target_id;                                /**< Target module ID (0=any) */
    float position[3];                                 /**< Source position (mm) */
    float priority;                                    /**< Computed priority */
    float ttl_ms;                                      /**< Time-to-live */
    float creation_phase;                              /**< LFP phase at creation */
    float field_strength;                              /**< Field at source */
    float coherence;                                   /**< Coherence at source */
    bool in_bundle;                                    /**< Part of a bundle */
    uint32_t bundle_id;                                /**< Bundle ID if bundled */
} ephaptic_substrate_message_t;

/**
 * @brief Message bundle
 */
typedef struct {
    uint32_t bundle_id;                                /**< Bundle identifier */
    uint32_t message_count;                            /**< Messages in bundle */
    uint32_t* message_ids;                             /**< Array of message IDs */
    float coherence;                                   /**< Bundle coherence level */
    float mean_phase;                                  /**< Mean phase of messages */
    float mean_priority;                               /**< Mean priority */
    float creation_time_ms;                            /**< Bundle creation time */
    float centroid[3];                                 /**< Spatial centroid (mm) */
} ephaptic_substrate_bundle_t;

/**
 * @brief Routing decision result
 */
typedef struct {
    uint32_t target_count;                             /**< Number of targets */
    uint32_t* target_ids;                              /**< Target module IDs */
    float* target_weights;                             /**< Per-target weights */
    float gradient_direction[3];                       /**< Field gradient dir */
    bool is_broadcast;                                 /**< Is broadcast message */
} ephaptic_substrate_routing_t;

/**
 * @brief Timing decision result
 */
typedef struct {
    float scheduled_time_ms;                           /**< When to send */
    float delay_ms;                                    /**< Delay from now */
    bool wait_for_phase;                               /**< Waiting for phase */
    float target_phase;                                /**< Target phase if waiting */
    bool immediate;                                    /**< Send immediately */
    bool in_burst;                                     /**< Part of burst */
} ephaptic_substrate_timing_t;

/**
 * @brief Burst event
 */
typedef struct {
    float event_time_ms;                               /**< Event timestamp */
    float coherence;                                   /**< Coherence at event */
    uint32_t messages_sent;                            /**< Messages in burst */
    uint32_t bundles_sent;                             /**< Bundles in burst */
    float burst_duration_ms;                           /**< Burst duration */
    float mean_priority;                               /**< Mean message priority */
} ephaptic_substrate_burst_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_queued;                          /**< Total messages queued */
    uint64_t messages_sent;                            /**< Total messages sent */
    uint64_t messages_dropped;                         /**< Messages dropped (TTL) */
    uint64_t bundles_created;                          /**< Bundles created */
    uint64_t burst_events;                             /**< Burst transmissions */
    uint64_t phase_locked_sends;                       /**< Phase-locked sends */
    float avg_priority;                                /**< Average priority */
    float avg_ttl_ms;                                  /**< Average TTL */
    float avg_delay_ms;                                /**< Average delay */
    float queue_utilization;                           /**< Queue usage [0,1] */
    float last_update_ms;                              /**< Last update time */
} ephaptic_substrate_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_substrate_bridge_struct ephaptic_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide reasonable substrate behavior
 * HOW:  Balanced settings for general use
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_default_config(
    ephaptic_substrate_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-substrate bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable field-aware messaging substrate
 * HOW:  Sets up queues, bundling, routing tables
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_substrate_bridge_t* ephaptic_substrate_bridge_create(
    const ephaptic_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_substrate_bridge_destroy(
    ephaptic_substrate_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_bridge_reset(
    ephaptic_substrate_bridge_t* bridge
);

//=============================================================================
// Message Queue API
//=============================================================================

/**
 * @brief Queue message with ephaptic metadata
 *
 * WHAT: Adds message to substrate queue
 * WHY:  Message will be processed with field-aware timing
 * HOW:  Computes priority, TTL, timing based on ephaptic state
 *
 * @param bridge        Bridge handle
 * @param source_id     Source module ID
 * @param target_id     Target module ID (0 for broadcast)
 * @param position      Source position (mm)
 * @param field_strength Current field at source
 * @param coherence     Current coherence
 * @param lfp_phase     Current LFP phase
 * @param message       Output message metadata
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_queue_message(
    ephaptic_substrate_bridge_t* bridge,
    uint32_t source_id,
    uint32_t target_id,
    const float position[3],
    float field_strength,
    float coherence,
    float lfp_phase,
    ephaptic_substrate_message_t* message
);

/**
 * @brief Get queue depth
 *
 * @param bridge Bridge handle
 * @return Current queue depth, or 0 on error
 */
NIMCP_EXPORT uint32_t ephaptic_substrate_queue_depth(
    const ephaptic_substrate_bridge_t* bridge
);

/**
 * @brief Clear message queue
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_clear_queue(
    ephaptic_substrate_bridge_t* bridge
);

//=============================================================================
// Bundling API
//=============================================================================

/**
 * @brief Create message bundle from queue
 *
 * WHAT: Groups compatible messages into bundle
 * WHY:  Coherent messages should travel together
 * HOW:  Applies bundling mode criteria
 *
 * @param bridge  Bridge handle
 * @param bundle  Output bundle (if created)
 * @return 1 if bundle created, 0 if not, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_create_bundle(
    ephaptic_substrate_bridge_t* bridge,
    ephaptic_substrate_bundle_t* bundle
);

/**
 * @brief Get pending bundle count
 *
 * @param bridge Bridge handle
 * @return Number of pending bundles
 */
NIMCP_EXPORT uint32_t ephaptic_substrate_bundle_count(
    const ephaptic_substrate_bridge_t* bridge
);

/**
 * @brief Free bundle resources
 *
 * @param bundle Bundle to free
 */
NIMCP_EXPORT void ephaptic_substrate_bundle_free(
    ephaptic_substrate_bundle_t* bundle
);

//=============================================================================
// Routing API
//=============================================================================

/**
 * @brief Compute routing for message
 *
 * WHAT: Determines message routing based on field
 * WHY:  Field gradients can guide message flow
 * HOW:  Applies routing mode with field state
 *
 * @param bridge        Bridge handle
 * @param message       Message to route
 * @param field_gradient Current field gradient
 * @param routing       Output routing decision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_compute_routing(
    ephaptic_substrate_bridge_t* bridge,
    const ephaptic_substrate_message_t* message,
    const float field_gradient[3],
    ephaptic_substrate_routing_t* routing
);

/**
 * @brief Register routing target
 *
 * @param bridge    Bridge handle
 * @param target_id Target module ID
 * @param position  Target position (mm)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_register_target(
    ephaptic_substrate_bridge_t* bridge,
    uint32_t target_id,
    const float position[3]
);

/**
 * @brief Free routing result resources
 *
 * @param routing Routing to free
 */
NIMCP_EXPORT void ephaptic_substrate_routing_free(
    ephaptic_substrate_routing_t* routing
);

//=============================================================================
// Timing API
//=============================================================================

/**
 * @brief Compute timing for message
 *
 * WHAT: Determines when message should be sent
 * WHY:  Phase-locked timing for synchronous communication
 * HOW:  Applies timing mode with LFP phase
 *
 * @param bridge    Bridge handle
 * @param message   Message to time
 * @param lfp_phase Current LFP phase
 * @param coherence Current coherence
 * @param timing    Output timing decision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_compute_timing(
    ephaptic_substrate_bridge_t* bridge,
    const ephaptic_substrate_message_t* message,
    float lfp_phase,
    float coherence,
    ephaptic_substrate_timing_t* timing
);

/**
 * @brief Check for burst condition
 *
 * WHAT: Determines if burst transmission should occur
 * WHY:  High coherence triggers rapid message burst
 * HOW:  Compares coherence to burst threshold
 *
 * @param bridge    Bridge handle
 * @param coherence Current coherence
 * @return true if burst should occur
 */
NIMCP_EXPORT bool ephaptic_substrate_should_burst(
    ephaptic_substrate_bridge_t* bridge,
    float coherence
);

/**
 * @brief Trigger burst transmission
 *
 * WHAT: Sends all queued messages immediately
 * WHY:  Capitalize on high-coherence window
 * HOW:  Processes entire queue with high priority
 *
 * @param bridge  Bridge handle
 * @param event   Output burst event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_trigger_burst(
    ephaptic_substrate_bridge_t* bridge,
    ephaptic_substrate_burst_event_t* event
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process queue, expire TTLs, update timing
 * HOW:  Called during simulation step
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step in milliseconds
 * @param lfp_phase Current LFP phase
 * @param coherence Current coherence
 * @return Number of messages processed, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_update(
    ephaptic_substrate_bridge_t* bridge,
    float dt_ms,
    float lfp_phase,
    float coherence
);

/**
 * @brief Process pending messages
 *
 * WHAT: Processes messages ready for delivery
 * WHY:  Actually deliver messages that are due
 * HOW:  Checks timing, applies routing, marks sent
 *
 * @param bridge      Bridge handle
 * @param current_phase Current LFP phase
 * @return Number of messages sent, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_process_pending(
    ephaptic_substrate_bridge_t* bridge,
    float current_phase
);

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
NIMCP_EXPORT int ephaptic_substrate_get_stats(
    const ephaptic_substrate_bridge_t* bridge,
    ephaptic_substrate_stats_t* stats
);

/**
 * @brief Get current priority scaling
 *
 * @param bridge Bridge handle
 * @param scale  Output: current priority scale
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_substrate_get_priority_scale(
    const ephaptic_substrate_bridge_t* bridge,
    float* scale
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active
 */
NIMCP_EXPORT bool ephaptic_substrate_is_active(
    const ephaptic_substrate_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_SUBSTRATE_BRIDGE_H */