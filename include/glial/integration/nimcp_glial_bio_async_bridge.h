/**
 * @file nimcp_glial_bio_async_bridge.h
 * @brief Glial Integration System Bio-Async Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for glial cells that provides message
 *       routing for astrocyte signaling, myelination events, microglia
 *       pruning, and metabolic coordination via the bio-router.
 *
 * WHY: Glial cells are critical support systems that regulate:
 *      - Synaptic modulation via astrocyte tripartite synapses
 *      - Signal propagation speed via oligodendrocyte myelination
 *      - Synaptic pruning and maintenance via microglia
 *      - Metabolic support and energy distribution
 *
 * HOW: Registers glial integration as a bio-router module, maintains
 *      subscription registry, provides typed message broadcast APIs, and
 *      processes incoming glial modulation requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * GLIAL OUTPUT PATHWAYS:
 * ----------------------
 * 1. Astrocyte Signaling:
 *    - Calcium waves propagating between astrocytes
 *    - Glutamate release/uptake modulation
 *    - Gliotransmitter release (D-serine, ATP, glutamate)
 *    - Mapped to: GLIAL_BIO_MSG_ASTROCYTE_SIGNAL, GLIAL_BIO_MSG_CALCIUM_WAVE
 *
 * 2. Myelination Events:
 *    - Oligodendrocyte wrapping of axons
 *    - Conduction velocity changes
 *    - Remyelination after injury
 *    - Mapped to: GLIAL_BIO_MSG_MYELINATION, GLIAL_BIO_MSG_CONDUCTION_CHANGE
 *
 * 3. Microglia Activities:
 *    - Synaptic surveillance and pruning
 *    - Inflammatory responses
 *    - Debris clearance
 *    - Mapped to: GLIAL_BIO_MSG_PRUNE_EVENT, GLIAL_BIO_MSG_SURVEILLANCE
 *
 * 4. Metabolic Support:
 *    - ATP/glucose delivery to neurons
 *    - Lactate shuttle operation
 *    - Mapped to: GLIAL_BIO_MSG_METABOLIC_SUPPORT
 *
 * GLIAL INPUT PATHWAYS:
 * ---------------------
 * 1. Neural Activity:
 *    - Synapse firing triggers astrocyte response
 *    - High activity triggers microglia surveillance
 *
 * 2. Immune Signals:
 *    - Cytokine activation of microglia
 *    - Inflammation triggers
 *
 * 3. Metabolic Demands:
 *    - ATP requests from active neurons
 *    - Energy state changes
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GLIAL_BIO_ASYNC_BRIDGE_H
#define NIMCP_GLIAL_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "glial/integration/nimcp_glial_integration.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define GLIAL_BIO_MAX_SUBSCRIPTIONS         64

/** Maximum pending messages in inbox */
#define GLIAL_BIO_MAX_INBOX_SIZE            256

/** Default broadcast interval for glial state (ms) */
#define GLIAL_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100

/** Message expiry time (ms) */
#define GLIAL_BIO_MESSAGE_TTL_MS            5000

/** Calcium wave propagation threshold */
#define GLIAL_BIO_CALCIUM_WAVE_THRESHOLD    0.5f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Glial bio-async message types
 *
 * WHAT: Message type enumeration for glial bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific glial function or event
 */
typedef enum {
    GLIAL_BIO_MSG_ASTROCYTE_SIGNAL = 0,     /**< Astrocyte activity signal */
    GLIAL_BIO_MSG_CALCIUM_WAVE,             /**< Calcium wave propagation */
    GLIAL_BIO_MSG_GLIOTRANSMITTER,          /**< Gliotransmitter release */
    GLIAL_BIO_MSG_SYNAPTIC_MODULATION,      /**< Tripartite synapse modulation */
    GLIAL_BIO_MSG_MYELINATION,              /**< Myelination event */
    GLIAL_BIO_MSG_CONDUCTION_CHANGE,        /**< Axon conduction velocity change */
    GLIAL_BIO_MSG_PRUNE_EVENT,              /**< Microglia pruning event */
    GLIAL_BIO_MSG_SURVEILLANCE,             /**< Microglia surveillance alert */
    GLIAL_BIO_MSG_INFLAMMATION,             /**< Inflammation state change */
    GLIAL_BIO_MSG_METABOLIC_SUPPORT,        /**< Metabolic support delivery */
    GLIAL_BIO_MSG_ATP_REQUEST,              /**< ATP request from neuron */
    GLIAL_BIO_MSG_QUERY_STATE,              /**< Query glial state */
    GLIAL_BIO_MSG_COUNT
} glial_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define GLIAL_BIO_SUB_ASTROCYTE_SIGNAL      (1U << GLIAL_BIO_MSG_ASTROCYTE_SIGNAL)
#define GLIAL_BIO_SUB_CALCIUM_WAVE          (1U << GLIAL_BIO_MSG_CALCIUM_WAVE)
#define GLIAL_BIO_SUB_GLIOTRANSMITTER       (1U << GLIAL_BIO_MSG_GLIOTRANSMITTER)
#define GLIAL_BIO_SUB_SYNAPTIC_MODULATION   (1U << GLIAL_BIO_MSG_SYNAPTIC_MODULATION)
#define GLIAL_BIO_SUB_MYELINATION           (1U << GLIAL_BIO_MSG_MYELINATION)
#define GLIAL_BIO_SUB_CONDUCTION_CHANGE     (1U << GLIAL_BIO_MSG_CONDUCTION_CHANGE)
#define GLIAL_BIO_SUB_PRUNE_EVENT           (1U << GLIAL_BIO_MSG_PRUNE_EVENT)
#define GLIAL_BIO_SUB_SURVEILLANCE          (1U << GLIAL_BIO_MSG_SURVEILLANCE)
#define GLIAL_BIO_SUB_INFLAMMATION          (1U << GLIAL_BIO_MSG_INFLAMMATION)
#define GLIAL_BIO_SUB_METABOLIC_SUPPORT     (1U << GLIAL_BIO_MSG_METABOLIC_SUPPORT)
#define GLIAL_BIO_SUB_ALL                   (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Astrocyte signal message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t astrocyte_id;                  /**< Source astrocyte ID */
    float calcium_level;                    /**< Intracellular calcium [0, 1] */
    float activity_level;                   /**< Overall activity [0, 1] */

    uint32_t synapse_count;                 /**< Number of synapses covered */
    float avg_modulation;                   /**< Average synaptic modulation */

    float position_x;                       /**< Spatial position X */
    float position_y;                       /**< Spatial position Y */
    float position_z;                       /**< Spatial position Z */

    uint64_t timestamp_us;                  /**< Signal timestamp */
} glial_bio_astrocyte_msg_t;

/**
 * @brief Calcium wave message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t wave_id;                       /**< Unique wave identifier */
    uint32_t source_astrocyte;              /**< Originating astrocyte */
    float wave_amplitude;                   /**< Wave intensity [0, 1] */
    float propagation_speed;                /**< Speed in um/s */

    float wave_radius;                      /**< Current wave radius (um) */
    float max_radius;                       /**< Maximum expected radius */

    uint32_t astrocytes_reached;            /**< Number of astrocytes affected */

    uint64_t wave_onset_us;                 /**< When wave started */
    uint64_t timestamp_us;                  /**< Current timestamp */
} glial_bio_calcium_wave_msg_t;

/**
 * @brief Myelination event message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t oligodendrocyte_id;            /**< Source oligodendrocyte */
    uint32_t neuron_id;                     /**< Target neuron */
    uint32_t axon_segment_id;               /**< Specific axon segment */

    float myelination_factor;               /**< Myelination strength [0, 1] */
    float previous_factor;                  /**< Previous myelination level */
    float conduction_multiplier;            /**< Speed improvement factor */

    bool is_remyelination;                  /**< Remyelination after damage */
    bool is_complete;                       /**< Full myelination achieved */

    uint64_t timestamp_us;                  /**< Event timestamp */
} glial_bio_myelination_msg_t;

/**
 * @brief Microglia pruning event message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t microglia_id;                  /**< Source microglia */
    uint32_t synapse_id;                    /**< Pruned synapse ID */
    uint32_t neuron_id;                     /**< Affected neuron */

    float synapse_weight_before;            /**< Weight before pruning */
    float activity_level;                   /**< Synapse activity level */

    uint32_t pruning_reason;                /**< Why pruned (0=weak, 1=tagged, etc.) */
    bool is_protective;                     /**< Protective vs destructive pruning */

    uint64_t timestamp_us;                  /**< Pruning timestamp */
} glial_bio_prune_msg_t;

/**
 * @brief Microglia surveillance message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t microglia_id;                  /**< Surveying microglia */
    uint32_t region_id;                     /**< Region being monitored */

    float surveillance_intensity;           /**< Surveillance level [0, 1] */
    float threat_detected;                  /**< Threat level [0, 1] */

    uint32_t synapses_monitored;            /**< Number of synapses checked */
    uint32_t anomalies_found;               /**< Number of anomalies detected */

    bool is_activated;                      /**< Microglia activated state */
    bool is_phagocytic;                     /**< In phagocytic mode */

    uint64_t timestamp_us;                  /**< Surveillance timestamp */
} glial_bio_surveillance_msg_t;

/**
 * @brief Metabolic support message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t astrocyte_id;                  /**< Providing astrocyte */
    uint32_t target_neuron_id;              /**< Target neuron (0 = broadcast) */

    float atp_delivered;                    /**< ATP amount delivered */
    float glucose_delivered;                /**< Glucose amount delivered */
    float lactate_delivered;                /**< Lactate shuttle amount */

    float energy_demand;                    /**< Requested energy level */
    float energy_supplied;                  /**< Actual energy supplied */
    float supply_efficiency;                /**< Supply vs demand ratio */

    uint64_t timestamp_us;                  /**< Delivery timestamp */
} glial_bio_metabolic_msg_t;

/**
 * @brief Inflammation state message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t region_id;                     /**< Affected region */
    float inflammation_level;               /**< Inflammation severity [0, 1] */
    float previous_level;                   /**< Previous inflammation level */

    uint32_t active_microglia;              /**< Number of activated microglia */
    float cytokine_concentration;           /**< Pro-inflammatory cytokines */

    bool is_acute;                          /**< Acute vs chronic */
    bool is_resolving;                      /**< Inflammation resolving */

    uint64_t onset_time_us;                 /**< When inflammation started */
    uint64_t timestamp_us;                  /**< Current timestamp */
} glial_bio_inflammation_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;              /**< Subscribed module ID */
    uint32_t msg_type_mask;                 /**< Bitmask of subscribed types */
    bool active;                            /**< Subscription active */
    uint64_t subscription_time;             /**< When subscribed */
    uint64_t messages_sent;                 /**< Messages sent to this sub */
} glial_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Glial bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t state_broadcast_interval_ms;    /**< State broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state changes */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t wave_channel;    /**< Channel for calcium waves */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_astrocyte_routing;           /**< Route astrocyte signals */
    bool enable_myelination_routing;         /**< Route myelination events */
    bool enable_pruning_routing;             /**< Route pruning events */
    bool enable_metabolic_routing;           /**< Route metabolic support */
    bool enable_logging;                     /**< Enable message logging */
} glial_bio_async_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                  /**< Total messages sent */
    uint64_t messages_received;              /**< Total messages received */
    uint64_t messages_dropped;               /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;                /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t astrocyte_signals_sent;         /**< Astrocyte signal broadcasts */
    uint64_t calcium_waves_sent;             /**< Calcium wave messages */
    uint64_t myelination_events_sent;        /**< Myelination event messages */
    uint64_t prune_events_sent;              /**< Pruning event messages */
    uint64_t metabolic_supports_sent;        /**< Metabolic support messages */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} glial_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Glial bio-async bridge handle
 */
typedef struct glial_bio_async_bridge_struct glial_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int glial_bio_async_default_config(glial_bio_async_config_t* config);

/**
 * @brief Create glial bio-async bridge
 */
glial_bio_async_bridge_t* glial_bio_async_bridge_create(
    const glial_bio_async_config_t* config
);

/**
 * @brief Destroy glial bio-async bridge
 */
void glial_bio_async_bridge_destroy(glial_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to glial integration and router
 */
int glial_bio_async_connect(
    glial_bio_async_bridge_t* bridge,
    glial_integration_t* glial_integration,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 */
int glial_bio_async_disconnect(glial_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 */
bool glial_bio_async_is_connected(const glial_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 */
int glial_bio_async_process_inbox(
    glial_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 */
int glial_bio_async_update(
    glial_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast astrocyte signal
 */
int glial_bio_async_broadcast_astrocyte_signal(
    glial_bio_async_bridge_t* bridge,
    uint32_t astrocyte_id,
    float calcium_level,
    float activity_level
);

/**
 * @brief Broadcast calcium wave
 */
int glial_bio_async_broadcast_calcium_wave(
    glial_bio_async_bridge_t* bridge,
    uint32_t wave_id,
    uint32_t source_astrocyte,
    float wave_amplitude
);

/**
 * @brief Broadcast myelination event
 */
int glial_bio_async_broadcast_myelination(
    glial_bio_async_bridge_t* bridge,
    uint32_t oligodendrocyte_id,
    uint32_t neuron_id,
    float myelination_factor
);

/**
 * @brief Broadcast pruning event
 */
int glial_bio_async_broadcast_prune_event(
    glial_bio_async_bridge_t* bridge,
    uint32_t microglia_id,
    uint32_t synapse_id,
    float synapse_weight_before
);

/**
 * @brief Broadcast surveillance alert
 */
int glial_bio_async_broadcast_surveillance(
    glial_bio_async_bridge_t* bridge,
    uint32_t microglia_id,
    uint32_t region_id,
    float threat_detected
);

/**
 * @brief Broadcast metabolic support
 */
int glial_bio_async_broadcast_metabolic_support(
    glial_bio_async_bridge_t* bridge,
    uint32_t astrocyte_id,
    uint32_t target_neuron_id,
    float atp_delivered
);

/**
 * @brief Broadcast inflammation state
 */
int glial_bio_async_broadcast_inflammation(
    glial_bio_async_bridge_t* bridge,
    uint32_t region_id,
    float inflammation_level,
    bool is_acute
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to glial messages
 */
int glial_bio_async_subscribe_module(
    glial_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from glial messages
 */
int glial_bio_async_unsubscribe_module(
    glial_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Get subscription count for message type
 */
uint32_t glial_bio_async_get_subscriber_count(
    const glial_bio_async_bridge_t* bridge,
    glial_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int glial_bio_async_get_stats(
    const glial_bio_async_bridge_t* bridge,
    glial_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 */
int glial_bio_async_reset_stats(glial_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 */
const char* glial_bio_msg_type_name(glial_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 */
void glial_bio_async_print_summary(const glial_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLIAL_BIO_ASYNC_BRIDGE_H */
