/**
 * @file nimcp_ofc_bio_async_bridge.h
 * @brief Orbitofrontal Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for OFC that provides comprehensive
 *       message routing for value-based decision signals via bio-router.
 *
 * WHY: The OFC needs to communicate:
 *      - Value computations for economic decision-making
 *      - Decision outcomes to executive systems
 *      - Reward prediction errors for learning
 *      - Reversal learning events for adaptive behavior
 *      - Expected value signals for planning
 *      - Context changes for flexible behavior
 *
 * HOW: Registers OFC as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * OFC OUTPUT SIGNALS:
 * ------------------
 * 1. Value signals:
 *    - Economic value of stimuli/options
 *    - Social reward valuation
 *    - Mapped to: OFC_BIO_MSG_VALUE_UPDATE
 *
 * 2. Decision outcomes:
 *    - Choice made between alternatives
 *    - Confidence and reaction time
 *    - Mapped to: OFC_BIO_MSG_DECISION_EVENT
 *
 * 3. Prediction errors:
 *    - Reward prediction error (RPE)
 *    - Drives learning and adaptation
 *    - Mapped to: OFC_BIO_MSG_RPE
 *
 * 4. Reversal learning:
 *    - Contingency changes detected
 *    - Triggers behavioral flexibility
 *    - Mapped to: OFC_BIO_MSG_REVERSAL
 *
 * OFC INPUT SIGNALS:
 * -----------------
 * 1. Sensory input:
 *    - From all sensory modalities
 *    - Reward-associated stimuli
 *
 * 2. Emotion modulation:
 *    - From amygdala
 *    - Valence and arousal
 *
 * 3. Drive signals:
 *    - From hypothalamus
 *    - Motivational state
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

#ifndef NIMCP_OFC_BIO_ASYNC_BRIDGE_H
#define NIMCP_OFC_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** OFC module identifier for bio-router registration */
#define OFC_BIO_MODULE_ID               0x5000

/** Maximum number of module subscriptions */
#define OFC_BIO_MAX_SUBSCRIPTIONS       64

/** Maximum pending messages in inbox */
#define OFC_BIO_MAX_INBOX_SIZE          256

/** Maximum pending messages in outbox */
#define OFC_BIO_MAX_OUTBOX_SIZE         128

/** Default broadcast interval for value state (ms) */
#define OFC_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define OFC_BIO_MESSAGE_TTL_MS          5000

/** Decision confidence threshold */
#define OFC_BIO_CONFIDENCE_THRESHOLD    0.6f

/** RPE significance threshold */
#define OFC_BIO_RPE_SIGNIFICANCE        0.1f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief OFC bio-async message types
 *
 * WHAT: Message type enumeration for OFC bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific OFC output pathway
 */
typedef enum {
    OFC_BIO_MSG_VALUE_UPDATE = 0,       /**< Value signal broadcast */
    OFC_BIO_MSG_DECISION_EVENT,         /**< Decision made */
    OFC_BIO_MSG_RPE,                    /**< Reward prediction error */
    OFC_BIO_MSG_REVERSAL,               /**< Reversal learning triggered */
    OFC_BIO_MSG_EXPECTED_VALUE,         /**< Expected value broadcast */
    OFC_BIO_MSG_CONTEXT_CHANGE,         /**< Context boundary detected */
    OFC_BIO_MSG_RISK_SIGNAL,            /**< Risk assessment signal */
    OFC_BIO_MSG_SOCIAL_VALUE,           /**< Social reward signal */
    OFC_BIO_MSG_COUNT
} ofc_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define OFC_BIO_SUB_VALUE_UPDATE        (1U << OFC_BIO_MSG_VALUE_UPDATE)
#define OFC_BIO_SUB_DECISION_EVENT      (1U << OFC_BIO_MSG_DECISION_EVENT)
#define OFC_BIO_SUB_RPE                 (1U << OFC_BIO_MSG_RPE)
#define OFC_BIO_SUB_REVERSAL            (1U << OFC_BIO_MSG_REVERSAL)
#define OFC_BIO_SUB_EXPECTED_VALUE      (1U << OFC_BIO_MSG_EXPECTED_VALUE)
#define OFC_BIO_SUB_CONTEXT_CHANGE      (1U << OFC_BIO_MSG_CONTEXT_CHANGE)
#define OFC_BIO_SUB_RISK_SIGNAL         (1U << OFC_BIO_MSG_RISK_SIGNAL)
#define OFC_BIO_SUB_SOCIAL_VALUE        (1U << OFC_BIO_MSG_SOCIAL_VALUE)
#define OFC_BIO_SUB_ALL                 (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Value update message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Value information */
    uint32_t stimulus_id;               /**< Stimulus/option identifier */
    float integrated_value;             /**< Combined utility estimate */
    float expected_value;               /**< Expected outcome value */
    float probability;                  /**< Probability of reward */
    float magnitude;                    /**< Reward magnitude */

    /* Temporal factors */
    float temporal_discount;            /**< Hyperbolic discount factor */
    float delay_ms;                     /**< Delay to reward (ms) */

    /* Confidence */
    float confidence;                   /**< Value estimate confidence */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} ofc_bio_value_msg_t;

/**
 * @brief Decision event message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Decision outcome */
    uint32_t chosen_option;             /**< Selected option ID */
    float decision_value;               /**< Value of chosen option */
    float confidence;                   /**< Decision confidence */
    float reaction_time_ms;             /**< Simulated reaction time */

    /* Alternative info */
    uint32_t num_alternatives;          /**< Number of options considered */
    float unchosen_value;               /**< Highest unchosen value */

    /* Decision type */
    uint32_t decision_type;             /**< Type of decision made */
    bool was_risky;                     /**< Risky choice selected */

    uint64_t timestamp_us;              /**< Decision timestamp */
} ofc_bio_decision_msg_t;

/**
 * @brief Reward prediction error message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* RPE components */
    float prediction_error;             /**< RPE value (received - expected) */
    float received_reward;              /**< Actually received reward */
    float expected_reward;              /**< Previously expected reward */

    /* Context */
    uint32_t stimulus_id;               /**< Associated stimulus */
    bool positive_surprise;             /**< True if RPE > 0 */

    /* Learning signal */
    float learning_signal;              /**< Scaled signal for plasticity */
    float cumulative_error;             /**< Running error average */

    uint64_t timestamp_us;              /**< Error timestamp */
} ofc_bio_rpe_msg_t;

/**
 * @brief Reversal learning message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Reversal info */
    uint32_t old_high_value_stimulus;   /**< Previously high-value option */
    uint32_t new_high_value_stimulus;   /**< New high-value option */
    float reversal_strength;            /**< Magnitude of contingency change */

    /* Detection metrics */
    float detection_confidence;         /**< Confidence in reversal detection */
    uint32_t trials_to_detect;          /**< Number of trials to detect */

    /* Behavioral adjustment */
    float exploration_boost;            /**< Increased exploration signal */
    bool requires_relearning;           /**< Full relearning needed */

    uint64_t timestamp_us;              /**< Reversal timestamp */
} ofc_bio_reversal_msg_t;

/**
 * @brief Expected value message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Expected value state */
    uint32_t stimulus_id;               /**< Stimulus identifier */
    float expected_value;               /**< Current expected value */
    float value_variance;               /**< Uncertainty in value */

    /* Components */
    float probability_component;        /**< Probability contribution */
    float magnitude_component;          /**< Magnitude contribution */
    float delay_component;              /**< Temporal discount contribution */
    float risk_component;               /**< Risk adjustment */

    uint64_t timestamp_us;              /**< Computation timestamp */
} ofc_bio_expected_value_msg_t;

/**
 * @brief Context change message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Context information */
    uint32_t old_context_id;            /**< Previous context identifier */
    uint32_t new_context_id;            /**< New context identifier */
    float context_similarity;           /**< Similarity between contexts */

    /* Change type */
    bool is_boundary;                   /**< Clear boundary detected */
    bool is_gradual;                    /**< Gradual context shift */

    /* Behavioral implications */
    float value_reset_factor;           /**< How much to reset values */
    float generalization_factor;        /**< Transfer from old context */

    uint64_t timestamp_us;              /**< Change timestamp */
} ofc_bio_context_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;          /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Bitmask of subscribed types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} ofc_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief OFC bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;          /**< Value state broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast value state */
    bool enable_decision_broadcast;          /**< Broadcast on every decision */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float rpe_threshold;                     /**< RPE significance threshold */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t rpe_channel;     /**< Channel for RPE events */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_value_broadcast;             /**< Broadcast value updates */
    bool enable_rpe_broadcast;               /**< Broadcast prediction errors */
    bool enable_reversal_broadcast;          /**< Broadcast reversal events */
    bool enable_context_broadcast;           /**< Enable context change alerts */
    bool enable_logging;                     /**< Enable message logging */
} ofc_bio_async_config_t;

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
    uint64_t value_broadcasts;               /**< Value update broadcasts */
    uint64_t decision_broadcasts;            /**< Decision event notifications */
    uint64_t rpe_broadcasts;                 /**< RPE broadcasts */
    uint64_t reversal_broadcasts;            /**< Reversal event broadcasts */
    uint64_t context_broadcasts;             /**< Context change broadcasts */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */
    float max_message_latency_us;            /**< Peak message latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} ofc_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief OFC bio-async bridge handle
 */
typedef struct ofc_bio_async_bridge_struct ofc_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Simplify bridge setup with reasonable starting values
 * HOW:  Populate config struct with OFC-appropriate defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_default_config(ofc_bio_async_config_t* config);

/**
 * @brief Create OFC bio-async bridge
 *
 * WHAT: Allocate and initialize OFC bio-async bridge
 * WHY:  Bridge enables OFC to communicate via bio-router
 * HOW:  Allocate memory, initialize state, prepare queues
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
ofc_bio_async_bridge_t* ofc_bio_async_bridge_create(
    const ofc_bio_async_config_t* config
);

/**
 * @brief Destroy OFC bio-async bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Prevent memory leaks and dangling connections
 * HOW:  Disconnect, free queues, release memory
 *
 * @param bridge Bridge to destroy
 */
void ofc_bio_async_bridge_destroy(ofc_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to OFC and router
 *
 * WHAT: Establish connection between OFC and bio-router
 * WHY:  Enable message routing for OFC signals
 * HOW:  Register with router, set up handlers
 *
 * @param bridge Bridge handle
 * @param ofc OFC instance to connect
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_connect(
    ofc_bio_async_bridge_t* bridge,
    void* ofc,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Remove OFC from bio-router
 * WHY:  Clean disconnection before shutdown
 * HOW:  Unregister module, clear handlers
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_disconnect(ofc_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * WHAT: Query connection status
 * WHY:  Verify bridge is ready for messaging
 * HOW:  Check internal connection state
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool ofc_bio_async_is_connected(const ofc_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Handle pending messages in OFC inbox
 * WHY:  Process incoming signals from other modules
 * HOW:  Dequeue messages, invoke handlers, update state
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int ofc_bio_async_process_inbox(
    ofc_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Periodic bridge state update
 * WHY:  Handle timing-based broadcasts and maintenance
 * HOW:  Check intervals, trigger broadcasts, update stats
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_update(
    ofc_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast value update
 *
 * WHAT: Send value computation result to subscribers
 * WHY:  Inform other modules of current value estimates
 * HOW:  Package value state, route to subscribers
 *
 * @param bridge Bridge handle
 * @param stimulus_id Stimulus identifier
 * @param value Current integrated value
 * @param confidence Value confidence
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_broadcast_value(
    ofc_bio_async_bridge_t* bridge,
    uint32_t stimulus_id,
    float value,
    float confidence
);

/**
 * @brief Broadcast decision event
 *
 * WHAT: Announce decision outcome to subscribers
 * WHY:  Inform executive and motor systems of choice
 * HOW:  Package decision info, high-priority broadcast
 *
 * @param bridge Bridge handle
 * @param chosen_option Selected option ID
 * @param value Decision value
 * @param confidence Decision confidence
 * @param reaction_time_ms Reaction time
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_broadcast_decision(
    ofc_bio_async_bridge_t* bridge,
    uint32_t chosen_option,
    float value,
    float confidence,
    float reaction_time_ms
);

/**
 * @brief Broadcast reward prediction error
 *
 * WHAT: Send RPE signal to subscribers
 * WHY:  Drive learning in downstream systems
 * HOW:  Package RPE data, route on dopamine channel
 *
 * @param bridge Bridge handle
 * @param stimulus_id Associated stimulus
 * @param rpe Prediction error value
 * @param received Received reward
 * @param expected Expected reward
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_broadcast_rpe(
    ofc_bio_async_bridge_t* bridge,
    uint32_t stimulus_id,
    float rpe,
    float received,
    float expected
);

/**
 * @brief Broadcast reversal learning event
 *
 * WHAT: Announce contingency reversal detection
 * WHY:  Trigger behavioral flexibility in other modules
 * HOW:  Package reversal info, high-priority broadcast
 *
 * @param bridge Bridge handle
 * @param old_stimulus Previously high-value stimulus
 * @param new_stimulus New high-value stimulus
 * @param reversal_strength Magnitude of change
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_broadcast_reversal(
    ofc_bio_async_bridge_t* bridge,
    uint32_t old_stimulus,
    uint32_t new_stimulus,
    float reversal_strength
);

/**
 * @brief Broadcast expected value
 *
 * WHAT: Send expected value computation
 * WHY:  Support planning and prediction in other modules
 * HOW:  Package expected value components, broadcast
 *
 * @param bridge Bridge handle
 * @param stimulus_id Stimulus identifier
 * @param expected_value Current expected value
 * @param variance Value uncertainty
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_broadcast_expected_value(
    ofc_bio_async_bridge_t* bridge,
    uint32_t stimulus_id,
    float expected_value,
    float variance
);

/**
 * @brief Broadcast context change
 *
 * WHAT: Announce context boundary detection
 * WHY:  Signal need for behavioral adaptation
 * HOW:  Package context info, broadcast to subscribers
 *
 * @param bridge Bridge handle
 * @param old_context Previous context ID
 * @param new_context New context ID
 * @param similarity Context similarity measure
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_broadcast_context_change(
    ofc_bio_async_bridge_t* bridge,
    uint32_t old_context,
    uint32_t new_context,
    float similarity
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to OFC messages
 *
 * WHAT: Register module for OFC message reception
 * WHY:  Allow modules to receive specific OFC signals
 * HOW:  Add subscription entry with type mask
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use OFC_BIO_SUB_* macros)
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_subscribe_module(
    ofc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from OFC messages
 *
 * WHAT: Remove module subscription
 * WHY:  Clean up when module no longer needs OFC signals
 * HOW:  Find and remove subscription entry
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_unsubscribe_module(
    ofc_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 *
 * WHAT: Change subscribed message types for module
 * WHY:  Allow dynamic subscription adjustment
 * HOW:  Find entry, update type mask
 *
 * @param bridge Bridge handle
 * @param module_id Module to update
 * @param msg_types New bitmask of message types
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_update_subscription(
    ofc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * WHAT: Query number of subscribers for type
 * WHY:  Monitor subscription state
 * HOW:  Count active subscriptions matching type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type to query
 * @return Number of subscribers
 */
uint32_t ofc_bio_async_get_subscriber_count(
    const ofc_bio_async_bridge_t* bridge,
    ofc_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve current bridge statistics
 * WHY:  Monitor bridge performance and health
 * HOW:  Copy internal stats to output struct
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_get_stats(
    const ofc_bio_async_bridge_t* bridge,
    ofc_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero out stats counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int ofc_bio_async_reset_stats(ofc_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * WHAT: Get human-readable name for message type
 * WHY:  Support logging and debugging
 * HOW:  Lookup in static name table
 *
 * @param msg_type Message type
 * @return Static string name
 */
const char* ofc_bio_msg_type_name(ofc_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * WHAT: Output bridge state summary
 * WHY:  Debug and diagnostic support
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void ofc_bio_async_print_summary(const ofc_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OFC_BIO_ASYNC_BRIDGE_H */
