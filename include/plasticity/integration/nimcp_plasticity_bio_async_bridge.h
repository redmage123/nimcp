/**
 * @file nimcp_plasticity_bio_async_bridge.h
 * @brief Unified Plasticity Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for all plasticity modules providing comprehensive
 *       message routing between plasticity mechanisms and all NIMCP modules via bio-router.
 *
 * WHY: Plasticity modules (STDP, BCM, homeostatic, STP, etc.) need to coordinate with each
 *      other and with the broader brain system. This bridge enables:
 *      - Event-driven weight update broadcasts to downstream modules
 *      - Reception of neuromodulatory signals for learning rate modulation
 *      - Coordination of consolidation events across mechanisms
 *      - Routing of eligibility trace updates for three-factor learning
 *
 * HOW: Registers plasticity as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming learning signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * PLASTICITY OUTPUT PATHWAYS:
 * ---------------------------
 * 1. Weight Update Signals:
 *    - STDP: Spike-timing-dependent weight changes
 *    - BCM: Rate-dependent modification threshold
 *    - Homeostatic: Synaptic scaling factors
 *    - Mapped to: PLASTICITY_MSG_WEIGHT_UPDATE, PLASTICITY_MSG_SCALING_EVENT
 *
 * 2. Consolidation Events:
 *    - Eligibility trace conversion to permanent weights
 *    - Memory consolidation during sleep
 *    - Mapped to: PLASTICITY_MSG_CONSOLIDATION, PLASTICITY_MSG_ELIGIBILITY_CONVERT
 *
 * 3. Learning State Signals:
 *    - Mechanism-level statistics (LTP/LTD counts)
 *    - Energy consumption metrics
 *    - Mapped to: PLASTICITY_MSG_STATE_UPDATE, PLASTICITY_MSG_ENERGY_REPORT
 *
 * PLASTICITY INPUT PATHWAYS:
 * --------------------------
 * 1. Neuromodulatory Signals:
 *    - Dopamine: Reward-modulated learning
 *    - Norepinephrine: Priority/urgency scaling
 *    - Acetylcholine: Attention-gated plasticity
 *    - Mapped to: Learning rate modulation requests
 *
 * 2. Spike Timing Signals:
 *    - Pre/post synaptic spike times
 *    - Dendritic spike events
 *    - Mapped to: STDP computation triggers
 *
 * 3. Homeostatic Control:
 *    - Target firing rate adjustments
 *    - Scaling factor requests
 *    - Mapped to: Homeostatic mechanism updates
 *
 * MESSAGE ROUTING ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                 PLASTICITY BIO-ASYNC BRIDGE                                |
 * +===========================================================================+
 * |                                                                            |
 * |   OUTBOUND (Plasticity -> Modules)                                        |
 * |   --------------------------------                                        |
 * |   +------------------+     +-------------------------------------------+  |
 * |   | Weight Updates   |---->| BIO_ROUTER: Broadcast to all subscribers  |  |
 * |   | Consolidation    |     |  - Memory systems, Attention, Executive   |  |
 * |   | Eligibility      |     |  - Neural populations, Learning monitors  |  |
 * |   | Energy Reports   |     |  - Curiosity, Introspection, Ethics       |  |
 * |   +------------------+     +-------------------------------------------+  |
 * |                                                                            |
 * |   INBOUND (Modules -> Plasticity)                                         |
 * |   --------------------------------                                        |
 * |   +-------------------------------------------+     +------------------+  |
 * |   | BIO_ROUTER: Receive from registered modules|---->| Learning Signal  |  |
 * |   |  - Spike timing events                    |     | Processing       |  |
 * |   |  - Neuromodulator levels                  |     | & Plasticity     |  |
 * |   |  - Homeostatic requests                   |     | Coordination     |  |
 * |   +-------------------------------------------+     +------------------+  |
 * |                                                                            |
 * +===========================================================================+
 * ```
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

#ifndef NIMCP_PLASTICITY_BIO_ASYNC_BRIDGE_H
#define NIMCP_PLASTICITY_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "plasticity/nimcp_plasticity_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief Plasticity bio-async bridge error codes
 */
typedef enum {
    PLASTICITY_BIO_OK = 0,                     /**< Success */
    PLASTICITY_BIO_ERROR_NULL_PARAM = -1,      /**< NULL parameter */
    PLASTICITY_BIO_ERROR_NOT_CONNECTED = -2,   /**< Bridge not connected */
    PLASTICITY_BIO_ERROR_ALREADY_CONNECTED = -3, /**< Already connected */
    PLASTICITY_BIO_ERROR_QUEUE_FULL = -4,      /**< Message queue full */
    PLASTICITY_BIO_ERROR_INVALID_TYPE = -5,    /**< Invalid message type */
    PLASTICITY_BIO_ERROR_NO_MEMORY = -6,       /**< Memory allocation failed */
    PLASTICITY_BIO_ERROR_SUBSCRIPTION_FULL = -7, /**< Subscription slots exhausted */
    PLASTICITY_BIO_ERROR_NOT_FOUND = -8,       /**< Item not found */
    PLASTICITY_BIO_ERROR_INVALID_STATE = -9,   /**< Invalid state for operation */
    PLASTICITY_BIO_ERROR_TIMEOUT = -10         /**< Operation timed out */
} plasticity_bio_error_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define PLASTICITY_BIO_MAX_SUBSCRIPTIONS        64

/** Maximum pending messages in inbox */
#define PLASTICITY_BIO_MAX_INBOX_SIZE           512

/** Maximum pending messages in outbox */
#define PLASTICITY_BIO_MAX_OUTBOX_SIZE          256

/** Default broadcast interval for learning state (ms) */
#define PLASTICITY_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define PLASTICITY_BIO_MESSAGE_TTL_MS           5000

/** Significant weight change threshold for broadcast */
#define PLASTICITY_BIO_WEIGHT_CHANGE_THRESHOLD  0.001f

/** Maximum registered plasticity modules */
#define PLASTICITY_BIO_MAX_MODULES              32

/** Module ID for plasticity bridge */
#define PLASTICITY_BIO_MODULE_ID                0x2100

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Plasticity bio-async message types
 *
 * WHAT: Message type enumeration for plasticity bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific plasticity event/signal
 */
typedef enum {
    PLASTICITY_MSG_WEIGHT_UPDATE = 0,      /**< Synaptic weight change event */
    PLASTICITY_MSG_CONSOLIDATION,          /**< Memory consolidation trigger */
    PLASTICITY_MSG_ELIGIBILITY_CONVERT,    /**< Eligibility to weight conversion */
    PLASTICITY_MSG_SCALING_EVENT,          /**< Homeostatic scaling applied */
    PLASTICITY_MSG_LTP_EVENT,              /**< Long-term potentiation occurred */
    PLASTICITY_MSG_LTD_EVENT,              /**< Long-term depression occurred */
    PLASTICITY_MSG_STATE_UPDATE,           /**< Plasticity state change */
    PLASTICITY_MSG_ENERGY_REPORT,          /**< Energy consumption report */
    PLASTICITY_MSG_CONFLICT_RESOLVED,      /**< Mechanism conflict resolved */
    PLASTICITY_MSG_SPIKE_TIMING,           /**< Spike timing for STDP */
    PLASTICITY_MSG_RATE_MODULATION,        /**< Learning rate modulation request */
    PLASTICITY_MSG_THRESHOLD_UPDATE,       /**< BCM threshold update */
    PLASTICITY_MSG_DENDRITIC_EVENT,        /**< Dendritic spike/plateau */
    PLASTICITY_MSG_METAPLASTICITY,         /**< Metaplasticity state change */
    PLASTICITY_MSG_STRUCTURAL_CHANGE,      /**< Structural plasticity event */
    PLASTICITY_MSG_BATCH_COMPLETE,         /**< Batch weight update complete */
    PLASTICITY_MSG_COUNT
} plasticity_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define PLASTICITY_BIO_SUB_WEIGHT_UPDATE       (1U << PLASTICITY_MSG_WEIGHT_UPDATE)
#define PLASTICITY_BIO_SUB_CONSOLIDATION       (1U << PLASTICITY_MSG_CONSOLIDATION)
#define PLASTICITY_BIO_SUB_ELIGIBILITY         (1U << PLASTICITY_MSG_ELIGIBILITY_CONVERT)
#define PLASTICITY_BIO_SUB_SCALING             (1U << PLASTICITY_MSG_SCALING_EVENT)
#define PLASTICITY_BIO_SUB_LTP                 (1U << PLASTICITY_MSG_LTP_EVENT)
#define PLASTICITY_BIO_SUB_LTD                 (1U << PLASTICITY_MSG_LTD_EVENT)
#define PLASTICITY_BIO_SUB_STATE               (1U << PLASTICITY_MSG_STATE_UPDATE)
#define PLASTICITY_BIO_SUB_ENERGY              (1U << PLASTICITY_MSG_ENERGY_REPORT)
#define PLASTICITY_BIO_SUB_CONFLICT            (1U << PLASTICITY_MSG_CONFLICT_RESOLVED)
#define PLASTICITY_BIO_SUB_SPIKE_TIMING        (1U << PLASTICITY_MSG_SPIKE_TIMING)
#define PLASTICITY_BIO_SUB_RATE_MOD            (1U << PLASTICITY_MSG_RATE_MODULATION)
#define PLASTICITY_BIO_SUB_THRESHOLD           (1U << PLASTICITY_MSG_THRESHOLD_UPDATE)
#define PLASTICITY_BIO_SUB_DENDRITIC           (1U << PLASTICITY_MSG_DENDRITIC_EVENT)
#define PLASTICITY_BIO_SUB_METAPLASTICITY      (1U << PLASTICITY_MSG_METAPLASTICITY)
#define PLASTICITY_BIO_SUB_STRUCTURAL          (1U << PLASTICITY_MSG_STRUCTURAL_CHANGE)
#define PLASTICITY_BIO_SUB_BATCH               (1U << PLASTICITY_MSG_BATCH_COMPLETE)
#define PLASTICITY_BIO_SUB_ALL                 (0xFFFFFFFFU)

/* ============================================================================
 * Plasticity Module Types
 * ============================================================================ */

/**
 * @brief Plasticity module types for registration
 *
 * WHAT: Identifies specific plasticity modules in the system
 * WHY:  Enables module-specific message routing and coordination
 */
typedef enum {
    PLASTICITY_MODULE_STDP = 0,            /**< Spike-timing-dependent plasticity */
    PLASTICITY_MODULE_BCM,                 /**< Bienenstock-Cooper-Munro */
    PLASTICITY_MODULE_HOMEOSTATIC,         /**< Homeostatic plasticity */
    PLASTICITY_MODULE_ELIGIBILITY,         /**< Eligibility traces */
    PLASTICITY_MODULE_DENDRITIC,           /**< Dendritic plasticity */
    PLASTICITY_MODULE_STP,                 /**< Short-term plasticity */
    PLASTICITY_MODULE_ADAPTIVE,            /**< Adaptive threshold */
    PLASTICITY_MODULE_PREDICTIVE,          /**< Predictive coding */
    PLASTICITY_MODULE_METAPLASTICITY,      /**< Metaplasticity */
    PLASTICITY_MODULE_STRUCTURAL,          /**< Structural plasticity */
    PLASTICITY_MODULE_HETEROSYNAPTIC,      /**< Heterosynaptic plasticity */
    PLASTICITY_MODULE_CALCIUM,             /**< Calcium dynamics */
    PLASTICITY_MODULE_PROTEIN,             /**< Protein synthesis */
    PLASTICITY_MODULE_COUNT
} plasticity_module_type_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Weight update message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t synapse_id;                   /**< Target synapse ID */
    uint32_t pre_neuron_id;                /**< Presynaptic neuron */
    uint32_t post_neuron_id;               /**< Postsynaptic neuron */

    float old_weight;                      /**< Weight before update */
    float new_weight;                      /**< Weight after update */
    float weight_change;                   /**< Delta weight */

    plasticity_module_type_t source_module; /**< Which mechanism generated update */
    bool is_potentiation;                  /**< LTP (true) or LTD (false) */

    uint64_t timestamp_us;                 /**< Update timestamp */
} plasticity_bio_weight_update_msg_t;

/**
 * @brief Consolidation event message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t num_synapses_consolidated;    /**< Number of synapses consolidated */
    float mean_weight_change;              /**< Mean weight change */
    float total_energy_cost;               /**< Energy consumed */

    plasticity_coordinator_state_t prior_state; /**< State before consolidation */
    plasticity_coordinator_state_t new_state;   /**< State after consolidation */

    bool triggered_by_sleep;               /**< Sleep-triggered consolidation */
    uint64_t consolidation_duration_ms;    /**< Duration of consolidation phase */

    uint64_t timestamp_us;                 /**< Event timestamp */
} plasticity_bio_consolidation_msg_t;

/**
 * @brief Eligibility trace conversion message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t synapse_id;                   /**< Synapse converted */
    float eligibility_trace;               /**< Trace value before conversion */
    float weight_change;                   /**< Resulting weight change */
    float reward_signal;                   /**< Reward signal that triggered conversion */

    bool dopamine_gated;                   /**< Was dopamine involved */
    float dopamine_level;                  /**< DA level at conversion [0, 1] */

    uint64_t trace_age_ms;                 /**< Age of eligibility trace */
    uint64_t timestamp_us;                 /**< Conversion timestamp */
} plasticity_bio_eligibility_msg_t;

/**
 * @brief Homeostatic scaling event message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t neuron_id;                    /**< Target neuron */
    float scaling_factor;                  /**< Applied scaling factor */
    float target_rate;                     /**< Target firing rate (Hz) */
    float actual_rate;                     /**< Actual firing rate (Hz) */
    float rate_deviation;                  /**< Deviation from target */

    uint32_t synapses_scaled;              /**< Number of synapses scaled */
    bool upscaling;                        /**< Upscaling (true) or downscaling (false) */

    uint64_t timestamp_us;                 /**< Event timestamp */
} plasticity_bio_scaling_msg_t;

/**
 * @brief LTP/LTD event message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t synapse_id;                   /**< Target synapse */
    float magnitude;                       /**< Change magnitude */
    float pre_spike_time;                  /**< Presynaptic spike time */
    float post_spike_time;                 /**< Postsynaptic spike time */
    float delta_t;                         /**< Spike timing difference */

    plasticity_module_type_t mechanism;    /**< Which mechanism generated */
    float neuromodulator_level;            /**< DA/NE/ACh modulation [0, 1] */

    bool is_triplet_stdp;                  /**< Triplet STDP rule used */
    uint64_t timestamp_us;                 /**< Event timestamp */
} plasticity_bio_ltp_ltd_msg_t;

/**
 * @brief State update message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    plasticity_coordinator_state_t old_state; /**< Previous state */
    plasticity_coordinator_state_t new_state; /**< New state */
    conflict_resolution_strategy_t strategy;  /**< Active conflict strategy */

    uint32_t active_mechanisms;            /**< Number of active mechanisms */
    float total_energy_rate;               /**< Current energy consumption */
    bool low_energy_mode;                  /**< Energy budget limiting */

    uint64_t time_in_state_ms;             /**< Time in new state */
    uint64_t timestamp_us;                 /**< Transition timestamp */
} plasticity_bio_state_msg_t;

/**
 * @brief Energy report message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    float total_energy_consumed;           /**< Total ATP units consumed */
    float energy_rate;                     /**< Current ATP/second */
    float energy_budget;                   /**< Budget limit */
    float budget_utilization;              /**< Utilization percentage */

    /* Per-mechanism breakdown */
    float stdp_energy;                     /**< STDP energy consumption */
    float bcm_energy;                      /**< BCM energy consumption */
    float homeostatic_energy;              /**< Homeostatic energy consumption */
    float other_energy;                    /**< Other mechanisms energy */

    bool budget_exceeded;                  /**< Budget was exceeded */
    uint64_t timestamp_us;                 /**< Report timestamp */
} plasticity_bio_energy_msg_t;

/**
 * @brief Conflict resolution message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t synapse_id;                   /**< Synapse with conflict */
    plasticity_module_type_t mechanism_a;  /**< First mechanism */
    plasticity_module_type_t mechanism_b;  /**< Second mechanism */

    float weight_change_a;                 /**< First mechanism proposal */
    float weight_change_b;                 /**< Second mechanism proposal */
    float resolved_change;                 /**< Final resolved change */

    conflict_resolution_strategy_t strategy; /**< Strategy used */
    bool immune_modulated;                 /**< Immune system influenced */

    uint64_t timestamp_us;                 /**< Resolution timestamp */
} plasticity_bio_conflict_msg_t;

/**
 * @brief Spike timing message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t neuron_id;                    /**< Neuron that spiked */
    bool is_presynaptic;                   /**< Pre (true) or post (false) */
    float spike_time_ms;                   /**< Precise spike time */
    float membrane_voltage;                /**< Voltage at spike */

    uint32_t target_synapse_count;         /**< Number of affected synapses */
    float firing_rate;                     /**< Current firing rate */

    uint64_t timestamp_us;                 /**< Event timestamp */
} plasticity_bio_spike_timing_msg_t;

/**
 * @brief Learning rate modulation request payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    float modulation_factor;               /**< Multiplicative factor [0, 10] */
    plasticity_module_type_t target_module; /**< Target module (or COUNT for all) */

    float dopamine_level;                  /**< DA contribution [0, 1] */
    float norepinephrine_level;            /**< NE contribution [0, 1] */
    float acetylcholine_level;             /**< ACh contribution [0, 1] */

    uint32_t requester_module;             /**< Who is requesting */
    uint32_t duration_ms;                  /**< Duration of modulation */

    uint64_t timestamp_us;                 /**< Request timestamp */
} plasticity_bio_rate_mod_msg_t;

/**
 * @brief Batch completion message payload
 */
typedef struct {
    bio_message_header_t header;           /**< Standard bio-async header */

    uint32_t batch_id;                     /**< Batch identifier */
    uint32_t synapses_updated;             /**< Number of synapses in batch */
    uint32_t ltp_count;                    /**< LTP events in batch */
    uint32_t ltd_count;                    /**< LTD events in batch */

    float mean_weight_change;              /**< Mean change across batch */
    float max_weight_change;               /**< Maximum change */
    float total_energy;                    /**< Energy for batch */

    uint64_t processing_time_us;           /**< Time to process batch */
    uint64_t timestamp_us;                 /**< Completion timestamp */
} plasticity_bio_batch_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;             /**< Subscribed module ID */
    uint32_t msg_type_mask;                /**< Bitmask of subscribed types */
    bool active;                           /**< Subscription active */
    uint64_t subscription_time;            /**< When subscribed */
    uint64_t messages_sent;                /**< Messages sent to this sub */
} plasticity_bio_subscription_t;

/* ============================================================================
 * Registered Module Entry
 * ============================================================================ */

/**
 * @brief Registered plasticity module entry
 */
typedef struct {
    plasticity_module_type_t type;         /**< Module type */
    const char* name;                      /**< Module name */
    void* handle;                          /**< Module handle (opaque) */
    bool enabled;                          /**< Module enabled */
    uint64_t updates_received;             /**< Updates from this module */
    uint64_t last_update_time;             /**< Last update timestamp */
} plasticity_bio_module_entry_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Plasticity bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t state_broadcast_interval_ms;    /**< State broadcast interval */
    uint32_t energy_report_interval_ms;      /**< Energy report interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state updates */
    bool enable_batch_mode;                  /**< Batch weight updates */
    uint32_t batch_size;                     /**< Synapses per batch */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float weight_change_threshold;           /**< Min change for broadcast */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Channel for urgent messages */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */
    uint32_t max_registered_modules;         /**< Maximum registered plasticity modules */

    /* Feature flags */
    bool enable_weight_broadcasts;           /**< Broadcast individual weight updates */
    bool enable_consolidation_broadcasts;    /**< Broadcast consolidation events */
    bool enable_ltp_ltd_broadcasts;          /**< Broadcast LTP/LTD events */
    bool enable_energy_tracking;             /**< Track and report energy */
    bool enable_conflict_broadcasts;         /**< Broadcast conflict resolutions */
    bool enable_logging;                     /**< Enable message logging */
} plasticity_bio_bridge_config_t;

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
    uint64_t weight_update_broadcasts;       /**< Weight update messages */
    uint64_t consolidation_broadcasts;       /**< Consolidation events */
    uint64_t ltp_events;                     /**< Total LTP events */
    uint64_t ltd_events;                     /**< Total LTD events */
    uint64_t scaling_events;                 /**< Homeostatic scaling events */
    uint64_t conflict_resolutions;           /**< Conflicts resolved */
    uint64_t batch_completions;              /**< Batch operations completed */

    /* Per-module counts */
    uint64_t stdp_updates;                   /**< Updates from STDP */
    uint64_t bcm_updates;                    /**< Updates from BCM */
    uint64_t homeostatic_updates;            /**< Updates from homeostatic */
    uint64_t eligibility_updates;            /**< Updates from eligibility */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */
    uint32_t registered_modules;             /**< Registered plasticity modules */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */
    float max_message_latency_us;            /**< Peak message latency */
    float avg_batch_process_time_us;         /**< Average batch processing time */

    /* Energy tracking */
    float total_energy_reported;             /**< Total energy reported */
    uint32_t energy_reports_sent;            /**< Number of energy reports */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} plasticity_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Plasticity bio-async bridge handle
 */
typedef struct plasticity_bio_async_bridge_struct plasticity_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for bio-async bridge configuration
 * WHY:  Easy initialization with biologically-realistic parameters
 * HOW:  Return struct with evidence-based timing and thresholds
 *
 * @param config Output configuration structure
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_default_config(plasticity_bio_bridge_config_t* config);

/**
 * @brief Create plasticity bio-async bridge
 *
 * WHAT: Initialize bio-async integration for plasticity modules
 * WHY:  Enable message routing between plasticity and all modules
 * HOW:  Allocate structure, initialize subscription registry, prepare handlers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
plasticity_bio_async_bridge_t* plasticity_bio_async_bridge_create(
    const plasticity_bio_bridge_config_t* config
);

/**
 * @brief Destroy plasticity bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from router, free subscriptions, release memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void plasticity_bio_async_bridge_destroy(plasticity_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to coordinator and router
 *
 * WHAT: Establish connections to plasticity coordinator and bio-router
 * WHY:  Enable bidirectional message flow
 * HOW:  Register with router, link to coordinator, set up handlers
 *
 * @param bridge Bio-async bridge
 * @param coordinator Plasticity coordinator
 * @param router Bio-router (NULL to use global)
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_connect(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_coordinator_t* coordinator,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Disconnect from bio-router
 * WHY:  Clean disconnection before shutdown
 * HOW:  Unregister handlers, clear module context
 *
 * @param bridge Bio-async bridge
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_disconnect(plasticity_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bio-async bridge
 * @return true if connected to both coordinator and router
 */
bool plasticity_bio_async_is_connected(const plasticity_bio_async_bridge_t* bridge);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register a plasticity module with the bridge
 *
 * WHAT: Add plasticity module to bridge's registry
 * WHY:  Enable module-specific message routing and coordination
 * HOW:  Store module handle and type, enable updates
 *
 * @param bridge Bio-async bridge
 * @param type Module type
 * @param name Module name
 * @param handle Module handle (opaque)
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_register_module(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_module_type_t type,
    const char* name,
    void* handle
);

/**
 * @brief Unregister a plasticity module
 *
 * WHAT: Remove plasticity module from bridge
 * WHY:  Clean unregistration when module is destroyed
 * HOW:  Remove entry from registry
 *
 * @param bridge Bio-async bridge
 * @param type Module type to unregister
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_unregister_module(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_module_type_t type
);

/**
 * @brief Enable/disable a registered module
 *
 * @param bridge Bio-async bridge
 * @param type Module type
 * @param enabled Enable (true) or disable (false)
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_set_module_enabled(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_module_type_t type,
    bool enabled
);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending messages from bio-router inbox
 * WHY:  Handle incoming learning signals and modulation requests
 * HOW:  Pop messages, dispatch to appropriate handlers, update state
 *
 * @param bridge Bio-async bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int plasticity_bio_async_process_inbox(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Perform periodic bridge updates
 * WHY:  Send scheduled broadcasts, expire old messages
 * HOW:  Check timers, send due broadcasts, cleanup expired entries
 *
 * @param bridge Bio-async bridge
 * @param delta_ms Time since last update
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_update(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Learning Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast weight update event
 *
 * WHAT: Send weight change event to all subscribers
 * WHY:  Notify system of synaptic modifications
 * HOW:  Package weight data, broadcast to interested modules
 *
 * @param bridge Bio-async bridge
 * @param synapse_id Synapse identifier
 * @param old_weight Previous weight
 * @param new_weight New weight
 * @param source_module Which mechanism generated the update
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_weight_update(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float old_weight,
    float new_weight,
    plasticity_module_type_t source_module
);

/**
 * @brief Broadcast consolidation event
 *
 * WHAT: Send consolidation event to all subscribers
 * WHY:  Notify system of memory consolidation
 * HOW:  Package consolidation data, broadcast on high-priority channel
 *
 * @param bridge Bio-async bridge
 * @param num_synapses Number of synapses consolidated
 * @param mean_change Mean weight change
 * @param triggered_by_sleep Whether sleep-triggered
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_consolidation(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t num_synapses,
    float mean_change,
    bool triggered_by_sleep
);

/**
 * @brief Broadcast LTP event
 *
 * WHAT: Send long-term potentiation event
 * WHY:  Notify downstream modules of synaptic strengthening
 * HOW:  Package LTP data with spike timing info
 *
 * @param bridge Bio-async bridge
 * @param synapse_id Synapse identifier
 * @param magnitude LTP magnitude
 * @param delta_t Spike timing difference
 * @param mechanism Source mechanism
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_ltp(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float magnitude,
    float delta_t,
    plasticity_module_type_t mechanism
);

/**
 * @brief Broadcast LTD event
 *
 * WHAT: Send long-term depression event
 * WHY:  Notify downstream modules of synaptic weakening
 * HOW:  Package LTD data with spike timing info
 *
 * @param bridge Bio-async bridge
 * @param synapse_id Synapse identifier
 * @param magnitude LTD magnitude (positive value)
 * @param delta_t Spike timing difference
 * @param mechanism Source mechanism
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_ltd(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float magnitude,
    float delta_t,
    plasticity_module_type_t mechanism
);

/**
 * @brief Broadcast homeostatic scaling event
 *
 * WHAT: Send synaptic scaling event
 * WHY:  Notify system of homeostatic adjustments
 * HOW:  Package scaling data, broadcast to subscribers
 *
 * @param bridge Bio-async bridge
 * @param neuron_id Target neuron
 * @param scaling_factor Applied scaling factor
 * @param target_rate Target firing rate
 * @param actual_rate Actual firing rate
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_scaling(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    float scaling_factor,
    float target_rate,
    float actual_rate
);

/**
 * @brief Broadcast eligibility trace conversion
 *
 * WHAT: Send eligibility-to-weight conversion event
 * WHY:  Notify system of three-factor learning event
 * HOW:  Package eligibility data with reward signal
 *
 * @param bridge Bio-async bridge
 * @param synapse_id Synapse identifier
 * @param trace_value Eligibility trace value
 * @param weight_change Resulting weight change
 * @param reward_signal Reward signal magnitude
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_eligibility_convert(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    float trace_value,
    float weight_change,
    float reward_signal
);

/**
 * @brief Broadcast state transition
 *
 * WHAT: Send plasticity state change event
 * WHY:  Notify system of learning phase transitions
 * HOW:  Package state info, broadcast to all subscribers
 *
 * @param bridge Bio-async bridge
 * @param old_state Previous state
 * @param new_state New state
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_state_change(
    plasticity_bio_async_bridge_t* bridge,
    plasticity_coordinator_state_t old_state,
    plasticity_coordinator_state_t new_state
);

/**
 * @brief Broadcast energy report
 *
 * WHAT: Send energy consumption report
 * WHY:  Enable metabolic monitoring and budgeting
 * HOW:  Package energy data, broadcast periodically
 *
 * @param bridge Bio-async bridge
 * @param total_energy Total energy consumed
 * @param energy_rate Current consumption rate
 * @param budget_utilization Percentage of budget used
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_energy_report(
    plasticity_bio_async_bridge_t* bridge,
    float total_energy,
    float energy_rate,
    float budget_utilization
);

/**
 * @brief Broadcast conflict resolution event
 *
 * WHAT: Send mechanism conflict resolution event
 * WHY:  Enable monitoring of inter-mechanism coordination
 * HOW:  Package conflict data with resolution outcome
 *
 * @param bridge Bio-async bridge
 * @param synapse_id Synapse with conflict
 * @param mech_a First mechanism
 * @param mech_b Second mechanism
 * @param change_a First mechanism proposal
 * @param change_b Second mechanism proposal
 * @param resolved Final resolved change
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_broadcast_conflict(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t synapse_id,
    plasticity_module_type_t mech_a,
    plasticity_module_type_t mech_b,
    float change_a,
    float change_b,
    float resolved
);

/**
 * @brief Notify spike timing for STDP
 *
 * WHAT: Report spike timing to plasticity system
 * WHY:  Enable spike-timing-dependent learning
 * HOW:  Package spike data, route to STDP modules
 *
 * @param bridge Bio-async bridge
 * @param neuron_id Neuron that spiked
 * @param is_presynaptic Whether presynaptic spike
 * @param spike_time_ms Spike time
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_notify_spike_timing(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t neuron_id,
    bool is_presynaptic,
    float spike_time_ms
);

/**
 * @brief Request learning rate modulation
 *
 * WHAT: Request modulation of learning rate
 * WHY:  Enable neuromodulator-gated learning
 * HOW:  Package modulation request, route to target modules
 *
 * @param bridge Bio-async bridge
 * @param modulation_factor Multiplicative factor
 * @param target_module Target module (COUNT for all)
 * @param dopamine DA level
 * @param norepinephrine NE level
 * @param acetylcholine ACh level
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_request_rate_modulation(
    plasticity_bio_async_bridge_t* bridge,
    float modulation_factor,
    plasticity_module_type_t target_module,
    float dopamine,
    float norepinephrine,
    float acetylcholine
);

/**
 * @brief Complete a batch of weight updates
 *
 * WHAT: Signal completion of batch weight updates
 * WHY:  Enable efficient batch processing and notification
 * HOW:  Package batch summary, broadcast completion
 *
 * @param bridge Bio-async bridge
 * @param batch_id Batch identifier
 * @param synapses_updated Number of synapses updated
 * @param ltp_count LTP events in batch
 * @param ltd_count LTD events in batch
 * @param mean_change Mean weight change
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_complete_batch(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t batch_id,
    uint32_t synapses_updated,
    uint32_t ltp_count,
    uint32_t ltd_count,
    float mean_change
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to plasticity messages
 *
 * WHAT: Register module to receive specific message types
 * WHY:  Enable selective message routing to interested modules
 * HOW:  Add entry to subscription registry with type mask
 *
 * @param bridge Bio-async bridge
 * @param module_id Module requesting subscription
 * @param msg_types Bitmask of message types (PLASTICITY_BIO_SUB_*)
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_subscribe_module(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from plasticity messages
 *
 * WHAT: Remove module subscription
 * WHY:  Clean unsubscription when module no longer needs messages
 * HOW:  Remove entry from subscription registry
 *
 * @param bridge Bio-async bridge
 * @param module_id Module to unsubscribe
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_unsubscribe_module(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 *
 * WHAT: Modify message types for existing subscription
 * WHY:  Allow dynamic subscription changes
 * HOW:  Update type mask in subscription registry
 *
 * @param bridge Bio-async bridge
 * @param module_id Module to update
 * @param msg_types New bitmask of message types
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_update_subscription(
    plasticity_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bio-async bridge
 * @param msg_type Message type to query
 * @return Number of subscribers for this type
 */
uint32_t plasticity_bio_async_get_subscriber_count(
    const plasticity_bio_async_bridge_t* bridge,
    plasticity_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bio-async bridge
 * @param stats Output statistics structure
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_get_stats(
    const plasticity_bio_async_bridge_t* bridge,
    plasticity_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bio-async bridge
 * @return PLASTICITY_BIO_OK on success, error code on failure
 */
int plasticity_bio_async_reset_stats(plasticity_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name string
 */
const char* plasticity_bio_msg_type_name(plasticity_bio_msg_type_t msg_type);

/**
 * @brief Get module type name
 *
 * @param module_type Module type
 * @return Human-readable name string
 */
const char* plasticity_bio_module_type_name(plasticity_module_type_t module_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bio-async bridge (NULL-safe)
 */
void plasticity_bio_async_print_summary(const plasticity_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_BIO_ASYNC_BRIDGE_H */
