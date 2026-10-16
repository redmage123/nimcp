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
// nimcp_nvc_substrate_bridge.h - Neurovascular Coupling to Bio-Async Bridge
//=============================================================================
/**
 * @file nimcp_nvc_substrate_bridge.h
 * @brief Bridge between Neurovascular Coupling and Bio-Async Messaging Substrate
 *
 * WHAT: Connects hemodynamic signals with the bio-async messaging system,
 *       enabling vascular events to trigger asynchronous biological messages.
 *
 * WHY:  Neurovascular coupling generates signals that must propagate through
 *       the biological computation substrate:
 *       - Blood flow changes trigger metabolic messages
 *       - BOLD signals indicate regional activity states
 *       - Astrocyte waves coordinate distributed processing
 *       - BBB permeability affects neuromodulator availability
 *
 * HOW:  Message-based integration:
 *       1. NVC events → Bio-async messages with priority/routing
 *       2. Metabolic state → Message propagation constraints
 *       3. Astrocyte signals → Broadcast coordination messages
 *       4. BBB state → Filter for neuromodulator messages
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           BIO-ASYNC MESSAGING
 * ─────────────────────────────────────────────────────────────────
 * CBF change events                 → Metabolic state messages
 * BOLD signal peaks                 → Activity burst notifications
 * Astrocyte Ca2+ waves              → Gliotransmitter broadcast
 * Vasodilation/constriction         → Regional coordination signals
 * BBB permeability changes          → Neuromodulator gating
 * HRF dynamics                      → Temporal message scheduling
 * ```
 *
 * MESSAGE TYPES:
 * - METABOLIC_STATE: Current O2/glucose/ATP levels
 * - PERFUSION_EVENT: Significant blood flow changes
 * - ASTROCYTE_BROADCAST: Gliotransmitter wave signals
 * - BBB_PERMEABILITY: Blood-brain barrier state changes
 * - HEMODYNAMIC_RESPONSE: HRF phase transitions
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_SUBSTRATE_BRIDGE_H
#define NIMCP_NVC_SUBSTRATE_BRIDGE_H

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
#define NVC_SUBSTRATE_MODULE_NAME       "nvc_substrate_bridge"

/** Maximum message channels */
#define NVC_SUBSTRATE_MAX_CHANNELS      32

/** Maximum pending messages */
#define NVC_SUBSTRATE_MAX_PENDING       256

/** Maximum message payload size (bytes) */
#define NVC_SUBSTRATE_MAX_PAYLOAD       128

/** Default message TTL (ms) */
#define NVC_SUBSTRATE_DEFAULT_TTL       1000.0f

/** CBF change threshold for message generation (%) */
#define NVC_SUBSTRATE_CBF_THRESHOLD     10.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Message type for NVC events
 */
typedef enum {
    NVC_MSG_METABOLIC_STATE = 0,         /**< Metabolic level update */
    NVC_MSG_PERFUSION_EVENT,             /**< Significant CBF change */
    NVC_MSG_ASTROCYTE_BROADCAST,         /**< Gliotransmitter wave */
    NVC_MSG_BBB_PERMEABILITY,            /**< BBB state change */
    NVC_MSG_HEMODYNAMIC_RESPONSE,        /**< HRF phase change */
    NVC_MSG_OXYGEN_ALERT,                /**< Hypoxia/hyperoxia warning */
    NVC_MSG_GLUCOSE_ALERT,               /**< Glucose level warning */
    NVC_MSG_VASOACTIVE_SIGNAL            /**< Vasoactive compound release */
} nvc_substrate_msg_type_t;

/**
 * @brief Message priority level
 */
typedef enum {
    NVC_PRIORITY_LOW = 0,                /**< Background metabolic updates */
    NVC_PRIORITY_NORMAL,                 /**< Standard state changes */
    NVC_PRIORITY_HIGH,                   /**< Important transitions */
    NVC_PRIORITY_CRITICAL                /**< Urgent metabolic alerts */
} nvc_substrate_priority_t;

/**
 * @brief Message routing mode
 */
typedef enum {
    NVC_ROUTE_LOCAL = 0,                 /**< Local to NVC unit region */
    NVC_ROUTE_NEIGHBORS,                 /**< Adjacent regions */
    NVC_ROUTE_BROADCAST,                 /**< System-wide broadcast */
    NVC_ROUTE_TARGETED                   /**< Specific destination */
} nvc_substrate_routing_t;

/**
 * @brief BBB permeability state
 */
typedef enum {
    NVC_BBB_INTACT = 0,                  /**< Normal barrier function */
    NVC_BBB_PERMEABLE,                   /**< Increased permeability */
    NVC_BBB_COMPROMISED                  /**< Barrier dysfunction */
} nvc_substrate_bbb_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-Substrate bridge
 */
typedef struct {
    /** Message generation */
    float cbf_change_threshold;          /**< CBF change for message (%) */
    float bold_change_threshold;         /**< BOLD change for message (%) */
    float metabolic_update_interval_ms;  /**< Metabolic state update rate */

    /** Priority assignment */
    float critical_cbf_threshold;        /**< CBF for critical priority (%) */
    float critical_o2_threshold;         /**< O2 for critical priority */

    /** Routing */
    bool enable_local_routing;           /**< Enable local messages */
    bool enable_broadcast;               /**< Enable broadcast messages */
    float broadcast_threshold;           /**< Event magnitude for broadcast */

    /** BBB integration */
    bool enable_bbb_filtering;           /**< Filter messages by BBB state */
    float bbb_baseline_permeability;     /**< Baseline BBB permeability */

    /** Astrocyte messaging */
    bool enable_astrocyte_messaging;     /**< Generate astrocyte messages */
    float astrocyte_wave_threshold;      /**< Ca2+ threshold for message */

    /** Message constraints */
    float default_ttl_ms;                /**< Default message TTL */
    uint32_t max_messages_per_update;    /**< Rate limit per update */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_substrate_config_t;

/**
 * @brief NVC message structure
 */
typedef struct {
    /** Header */
    nvc_substrate_msg_type_t type;       /**< Message type */
    nvc_substrate_priority_t priority;   /**< Priority level */
    nvc_substrate_routing_t routing;     /**< Routing mode */

    /** Source */
    uint32_t source_nvc_unit;            /**< Originating NVC unit */
    uint32_t target_id;                  /**< Target (if targeted routing) */

    /** Timing */
    float timestamp_ms;                  /**< Generation timestamp */
    float ttl_ms;                        /**< Time to live */

    /** Payload - type-specific data */
    union {
        struct {
            float oxygen;
            float glucose;
            float atp;
        } metabolic;

        struct {
            float cbf_change_percent;
            float cbv_change_percent;
            bool is_increase;
        } perfusion;

        struct {
            float calcium_amplitude;
            float wave_velocity;
            float spatial_extent;
        } astrocyte;

        struct {
            nvc_substrate_bbb_state_t state;
            float permeability;
            float duration_ms;
        } bbb;

        struct {
            uint8_t phase;  /* 0=rest, 1=rise, 2=peak, 3=undershoot, 4=recovery */
            float bold_signal;
            float time_since_onset_ms;
        } hemodynamic;

        struct {
            uint8_t signal_type;  /* 0=NO, 1=prostaglandin, 2=K+, 3=adenosine */
            float concentration;
        } vasoactive;

        uint8_t raw[NVC_SUBSTRATE_MAX_PAYLOAD];
    } payload;

    /** Delivery tracking */
    uint32_t sequence_number;            /**< Sequence for ordering */
    bool delivered;                      /**< Delivery confirmation */
} nvc_substrate_message_t;

/**
 * @brief Message channel state
 */
typedef struct {
    uint32_t channel_id;                 /**< Channel identifier */
    nvc_substrate_msg_type_t type_filter; /**< Message type for channel */
    uint32_t nvc_unit_id;                /**< Associated NVC unit */

    /** Statistics */
    uint64_t messages_sent;              /**< Messages sent on channel */
    uint64_t messages_received;          /**< Messages received */
    uint64_t messages_dropped;           /**< Messages dropped (TTL, overflow) */

    /** State */
    bool active;                         /**< Channel is active */
    float last_message_time_ms;          /**< Last message timestamp */
} nvc_substrate_channel_t;

/**
 * @brief BBB filter state
 */
typedef struct {
    uint32_t nvc_unit_id;                /**< NVC unit */
    nvc_substrate_bbb_state_t state;     /**< Current BBB state */
    float permeability;                  /**< Current permeability (0-1) */

    /** Filtering rules */
    bool blocks_small_molecules;         /**< Blocks small neuromodulators */
    bool blocks_large_molecules;         /**< Blocks peptides/proteins */
    bool blocks_ions;                    /**< Blocks ionic signals */
} nvc_substrate_bbb_filter_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t messages_generated;         /**< Total messages created */
    uint64_t messages_delivered;         /**< Messages successfully delivered */
    uint64_t messages_expired;           /**< Messages expired (TTL) */
    uint64_t messages_filtered;          /**< Messages filtered by BBB */

    /** By type */
    uint64_t metabolic_messages;
    uint64_t perfusion_messages;
    uint64_t astrocyte_messages;
    uint64_t alert_messages;

    /** Performance */
    float mean_delivery_latency_ms;      /**< Mean message latency */
    float message_rate_per_second;       /**< Current message rate */
    uint32_t pending_message_count;      /**< Messages awaiting delivery */

    float last_update_ms;                /**< Last update timestamp */
} nvc_substrate_stats_t;

/** Opaque bridge handle */
typedef struct nvc_substrate_bridge_struct nvc_substrate_bridge_t;

/** Message callback type */
typedef void (*nvc_substrate_callback_t)(
    const nvc_substrate_message_t* message,
    void* user_data
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_default_config(nvc_substrate_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-Substrate bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes messaging connection between NVC and bio-async
 * HOW:  Creates message queues and channel infrastructure
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_substrate_bridge_t* nvc_substrate_bridge_create(
    const nvc_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_substrate_bridge_destroy(nvc_substrate_bridge_t* bridge);

//=============================================================================
// Channel Management API
//=============================================================================

/**
 * @brief Create message channel for NVC unit
 *
 * WHAT: Establishes messaging channel for NVC unit
 * WHY:  Enables message generation/reception for region
 * HOW:  Creates channel with type filtering
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit ID
 * @param type_filter Message type to handle (or 0xFF for all)
 * @param channel_id Output channel ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_create_channel(
    nvc_substrate_bridge_t* bridge,
    uint32_t nvc_unit_id,
    nvc_substrate_msg_type_t type_filter,
    uint32_t* channel_id
);

/**
 * @brief Close message channel
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to close
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_close_channel(
    nvc_substrate_bridge_t* bridge,
    uint32_t channel_id
);

/**
 * @brief Register message callback
 *
 * WHAT: Registers callback for incoming messages
 * WHY:  Enables async message handling
 * HOW:  Callback invoked on message receipt
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to monitor
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_register_callback(
    nvc_substrate_bridge_t* bridge,
    uint32_t channel_id,
    nvc_substrate_callback_t callback,
    void* user_data
);

//=============================================================================
// Message Generation API
//=============================================================================

/**
 * @brief Generate metabolic state message
 *
 * WHAT: Creates metabolic state message from NVC unit
 * WHY:  Broadcasts current O2/glucose/ATP levels
 * HOW:  Packages state into message, queues for delivery
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id Source NVC unit
 * @param oxygen Oxygen level (0-1)
 * @param glucose Glucose level (0-1)
 * @param atp ATP level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_send_metabolic_state(
    nvc_substrate_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float oxygen,
    float glucose,
    float atp
);

/**
 * @brief Generate perfusion event message
 *
 * WHAT: Creates message for significant CBF change
 * WHY:  Notifies system of blood flow changes
 * HOW:  Triggers on threshold crossing
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id Source NVC unit
 * @param cbf_change_percent CBF change (% of baseline)
 * @param cbv_change_percent CBV change (% of baseline)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_send_perfusion_event(
    nvc_substrate_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf_change_percent,
    float cbv_change_percent
);

/**
 * @brief Generate astrocyte broadcast message
 *
 * WHAT: Broadcasts astrocyte calcium wave signal
 * WHY:  Coordinates gliotransmitter effects
 * HOW:  Uses broadcast routing for wide distribution
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id Source NVC unit
 * @param calcium_amplitude Ca2+ wave amplitude
 * @param wave_velocity Propagation velocity
 * @param spatial_extent Wave spatial extent
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_send_astrocyte_broadcast(
    nvc_substrate_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float calcium_amplitude,
    float wave_velocity,
    float spatial_extent
);

/**
 * @brief Send custom message
 *
 * @param bridge Bridge handle
 * @param message Message to send
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_send_message(
    nvc_substrate_bridge_t* bridge,
    const nvc_substrate_message_t* message
);

//=============================================================================
// BBB Filter API
//=============================================================================

/**
 * @brief Set BBB state for NVC unit
 *
 * WHAT: Updates blood-brain barrier state
 * WHY:  BBB state affects message filtering
 * HOW:  Changes permeability and filtering rules
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit
 * @param state BBB state
 * @param permeability Permeability value (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_set_bbb_state(
    nvc_substrate_bridge_t* bridge,
    uint32_t nvc_unit_id,
    nvc_substrate_bbb_state_t state,
    float permeability
);

/**
 * @brief Get BBB filter state
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit
 * @param filter Output BBB filter state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_get_bbb_filter(
    const nvc_substrate_bridge_t* bridge,
    uint32_t nvc_unit_id,
    nvc_substrate_bbb_filter_t* filter
);

//=============================================================================
// Message Reception API
//=============================================================================

/**
 * @brief Poll for pending messages
 *
 * WHAT: Retrieves pending messages for channel
 * WHY:  Alternative to callback-based reception
 * HOW:  Returns oldest pending message
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to poll
 * @param message Output message (if available)
 * @return 1 if message available, 0 if none, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_poll_message(
    nvc_substrate_bridge_t* bridge,
    uint32_t channel_id,
    nvc_substrate_message_t* message
);

/**
 * @brief Get pending message count for channel
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to query
 * @return Number of pending messages, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_pending_count(
    const nvc_substrate_bridge_t* bridge,
    uint32_t channel_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update for message processing
 * WHY:  Deliver messages, expire old ones, update stats
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_update(
    nvc_substrate_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_reset(nvc_substrate_bridge_t* bridge);

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
NIMCP_EXPORT int nvc_substrate_get_stats(
    const nvc_substrate_bridge_t* bridge,
    nvc_substrate_stats_t* stats
);

/**
 * @brief Get channel state
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to query
 * @param channel Output channel state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_substrate_get_channel_state(
    const nvc_substrate_bridge_t* bridge,
    uint32_t channel_id,
    nvc_substrate_channel_t* channel
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_SUBSTRATE_BRIDGE_H */