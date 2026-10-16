/**
 * @file nimcp_sleep_bio_async_bridge.h
 * @brief Sleep-Wake System Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for sleep-wake system that provides
 *       message routing for sleep stages, consolidation events, and circadian
 *       coordination via the bio-router.
 *
 * WHY: The sleep-wake system regulates critical brain-wide processes:
 *      - Memory consolidation during NREM/REM sleep stages
 *      - Synaptic homeostasis and pruning
 *      - Circadian-driven cognitive modulation
 *      - Adenosine-mediated fatigue signaling
 *
 * HOW: Registers sleep system as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      sleep control requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * SLEEP OUTPUT PATHWAYS:
 * ----------------------
 * 1. Stage Transitions (System-wide):
 *    - Awake -> Drowsy -> Light NREM -> Deep NREM -> REM -> Awake
 *    - Oscillation frequency changes (Beta -> Alpha -> Theta -> Delta -> Theta)
 *    - Mapped to: BIO_MSG_SLEEP_STAGE_CHANGE
 *
 * 2. Consolidation Events:
 *    - Memory replay during NREM (sharp-wave ripples)
 *    - Creative binding during REM (theta oscillations)
 *    - Mapped to: BIO_MSG_SLEEP_CONSOLIDATION_EVENT, BIO_MSG_SLEEP_REPLAY_EVENT
 *
 * 3. Homeostatic Signals:
 *    - Adenosine accumulation (sleep pressure)
 *    - Synaptic downscaling during deep sleep
 *    - Mapped to: BIO_MSG_SLEEP_HOMEOSTASIS_UPDATE
 *
 * SLEEP INPUT PATHWAYS:
 * ---------------------
 * 1. Hypothalamic (circadian):
 *    - SCN circadian phase signals
 *    - Orexin/hypocretin arousal drive
 *
 * 2. Cognitive (learning):
 *    - Learning activity increases sleep pressure
 *    - Emotional salience affects replay priority
 *
 * 3. External (control):
 *    - Sleep/wake trigger requests
 *    - Stage override commands
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

#ifndef NIMCP_SLEEP_BIO_ASYNC_BRIDGE_H
#define NIMCP_SLEEP_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/nimcp_sleep_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define SLEEP_BIO_MAX_SUBSCRIPTIONS         64

/** Maximum pending messages in inbox */
#define SLEEP_BIO_MAX_INBOX_SIZE            256

/** Default broadcast interval for sleep state (ms) */
#define SLEEP_BIO_DEFAULT_BROADCAST_INTERVAL_MS  500

/** Message expiry time (ms) */
#define SLEEP_BIO_MESSAGE_TTL_MS            5000

/** Deep sleep consolidation threshold */
#define SLEEP_BIO_DEEP_SLEEP_THRESHOLD      0.7f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Sleep bio-async message types
 *
 * WHAT: Message type enumeration for sleep bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific sleep pathway or event
 */
typedef enum {
    SLEEP_BIO_MSG_STAGE_TRANSITION = 0,     /**< Sleep stage changed */
    SLEEP_BIO_MSG_CONSOLIDATION_START,      /**< Memory consolidation started */
    SLEEP_BIO_MSG_CONSOLIDATION_COMPLETE,   /**< Memory consolidation complete */
    SLEEP_BIO_MSG_REPLAY_EVENT,             /**< Memory replay occurred */
    SLEEP_BIO_MSG_HOMEOSTASIS_UPDATE,       /**< Synaptic homeostasis update */
    SLEEP_BIO_MSG_ADENOSINE_LEVEL,          /**< Adenosine/fatigue level */
    SLEEP_BIO_MSG_PRESSURE_UPDATE,          /**< Sleep pressure change */
    SLEEP_BIO_MSG_OSCILLATION_CHANGE,       /**< Brain oscillation frequency change */
    SLEEP_BIO_MSG_CIRCADIAN_SYNC,           /**< Circadian synchronization event */
    SLEEP_BIO_MSG_WAKE_REQUEST,             /**< External wake request */
    SLEEP_BIO_MSG_SLEEP_REQUEST,            /**< External sleep request */
    SLEEP_BIO_MSG_QUERY_STATE,              /**< Query current sleep state */
    SLEEP_BIO_MSG_COUNT
} sleep_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define SLEEP_BIO_SUB_STAGE_TRANSITION      (1U << SLEEP_BIO_MSG_STAGE_TRANSITION)
#define SLEEP_BIO_SUB_CONSOLIDATION_START   (1U << SLEEP_BIO_MSG_CONSOLIDATION_START)
#define SLEEP_BIO_SUB_CONSOLIDATION_COMPLETE (1U << SLEEP_BIO_MSG_CONSOLIDATION_COMPLETE)
#define SLEEP_BIO_SUB_REPLAY_EVENT          (1U << SLEEP_BIO_MSG_REPLAY_EVENT)
#define SLEEP_BIO_SUB_HOMEOSTASIS_UPDATE    (1U << SLEEP_BIO_MSG_HOMEOSTASIS_UPDATE)
#define SLEEP_BIO_SUB_ADENOSINE_LEVEL       (1U << SLEEP_BIO_MSG_ADENOSINE_LEVEL)
#define SLEEP_BIO_SUB_PRESSURE_UPDATE       (1U << SLEEP_BIO_MSG_PRESSURE_UPDATE)
#define SLEEP_BIO_SUB_OSCILLATION_CHANGE    (1U << SLEEP_BIO_MSG_OSCILLATION_CHANGE)
#define SLEEP_BIO_SUB_ALL                   (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Sleep stage transition message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    sleep_state_t previous_stage;           /**< Previous sleep stage */
    sleep_state_t current_stage;            /**< Current sleep stage */
    float transition_duration_ms;           /**< Transition duration */

    float sleep_pressure;                   /**< Current sleep pressure [0, 1] */
    float adenosine_level;                  /**< Adenosine concentration [0, 1] */
    float circadian_phase;                  /**< Circadian phase [0, 2*PI] */

    uint64_t timestamp_us;                  /**< Transition timestamp */
} sleep_bio_stage_transition_msg_t;

/**
 * @brief Consolidation event message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t memories_replayed;             /**< Number of memories replayed */
    uint32_t synapses_strengthened;         /**< Synapses that gained weight */
    uint32_t synapses_weakened;             /**< Synapses that lost weight */
    float consolidation_efficiency;         /**< Efficiency metric [0, 1] */

    bool is_emotional_priority;             /**< Emotional memories prioritized */
    bool is_novel_priority;                 /**< Novel memories prioritized */

    sleep_state_t during_stage;             /**< Which stage consolidation occurred */
    uint64_t duration_us;                   /**< Consolidation duration */
    uint64_t timestamp_us;                  /**< Event timestamp */
} sleep_bio_consolidation_msg_t;

/**
 * @brief Memory replay event message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t memory_id;                     /**< Memory being replayed */
    float replay_strength;                  /**< Replay intensity [0, 1] */
    float replay_speed_multiplier;          /**< Speed vs awake (e.g., 15x) */

    float emotional_salience;               /**< Emotional importance [0, 1] */
    float novelty_score;                    /**< Novelty of memory [0, 1] */

    sleep_state_t during_stage;             /**< Which stage replay occurred */
    uint64_t timestamp_us;                  /**< Replay timestamp */
} sleep_bio_replay_msg_t;

/**
 * @brief Homeostasis update message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    float total_weight_before;              /**< Total synaptic weight before */
    float total_weight_after;               /**< Total synaptic weight after */
    float downscaling_factor;               /**< Applied scaling factor */

    uint32_t synapses_pruned;               /**< Weak synapses removed */
    float pruning_threshold;                /**< Threshold used for pruning */
    float energy_saved_percent;             /**< Estimated energy savings */

    uint64_t timestamp_us;                  /**< Update timestamp */
} sleep_bio_homeostasis_msg_t;

/**
 * @brief Adenosine/pressure level message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    float adenosine_level;                  /**< Current adenosine [0, 1] */
    float sleep_pressure;                   /**< Overall sleep pressure [0, 1] */
    float accumulation_rate;                /**< Rate of pressure increase */
    float clearance_rate;                   /**< Rate of pressure decrease */

    float time_awake_hours;                 /**< Hours since last sleep */
    bool sleep_needed;                      /**< Pressure exceeds threshold */

    uint64_t timestamp_us;                  /**< Measurement timestamp */
} sleep_bio_adenosine_msg_t;

/**
 * @brief Oscillation change message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    float dominant_frequency_hz;            /**< Primary oscillation frequency */
    float frequency_power;                  /**< Power at dominant frequency */

    float delta_power;                      /**< Delta band power (0.5-4 Hz) */
    float theta_power;                      /**< Theta band power (4-8 Hz) */
    float alpha_power;                      /**< Alpha band power (8-13 Hz) */
    float beta_power;                       /**< Beta band power (13-30 Hz) */
    float gamma_power;                      /**< Gamma band power (30-100 Hz) */

    sleep_state_t current_stage;            /**< Associated sleep stage */
    uint64_t timestamp_us;                  /**< Measurement timestamp */
} sleep_bio_oscillation_msg_t;

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
} sleep_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Sleep bio-async bridge configuration
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
    nimcp_bio_channel_type_t consolidation_channel; /**< Channel for consolidation */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_consolidation_routing;       /**< Route consolidation events */
    bool enable_replay_routing;              /**< Route replay events */
    bool enable_homeostasis_routing;         /**< Route homeostasis updates */
    bool enable_oscillation_routing;         /**< Route oscillation changes */
    bool enable_logging;                     /**< Enable message logging */
} sleep_bio_async_config_t;

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
    uint64_t stage_transitions_sent;         /**< Stage transition broadcasts */
    uint64_t consolidation_events_sent;      /**< Consolidation event messages */
    uint64_t replay_events_sent;             /**< Replay event messages */
    uint64_t homeostasis_updates_sent;       /**< Homeostasis update messages */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} sleep_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Sleep bio-async bridge handle
 */
typedef struct sleep_bio_async_bridge_struct sleep_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int sleep_bio_async_default_config(sleep_bio_async_config_t* config);

/**
 * @brief Create sleep bio-async bridge
 */
sleep_bio_async_bridge_t* sleep_bio_async_bridge_create(
    const sleep_bio_async_config_t* config
);

/**
 * @brief Destroy sleep bio-async bridge
 */
void sleep_bio_async_bridge_destroy(sleep_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to sleep system and router
 */
int sleep_bio_async_connect(
    sleep_bio_async_bridge_t* bridge,
    sleep_system_t sleep_system,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 */
int sleep_bio_async_disconnect(sleep_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 */
bool sleep_bio_async_is_connected(const sleep_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 */
int sleep_bio_async_process_inbox(
    sleep_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 */
int sleep_bio_async_update(
    sleep_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast sleep stage transition
 */
int sleep_bio_async_broadcast_stage_transition(
    sleep_bio_async_bridge_t* bridge,
    sleep_state_t previous_stage,
    sleep_state_t current_stage
);

/**
 * @brief Broadcast consolidation start event
 */
int sleep_bio_async_broadcast_consolidation_start(
    sleep_bio_async_bridge_t* bridge,
    sleep_state_t during_stage
);

/**
 * @brief Broadcast consolidation complete event
 */
int sleep_bio_async_broadcast_consolidation_complete(
    sleep_bio_async_bridge_t* bridge,
    uint32_t memories_replayed,
    float efficiency
);

/**
 * @brief Broadcast memory replay event
 */
int sleep_bio_async_broadcast_replay(
    sleep_bio_async_bridge_t* bridge,
    uint32_t memory_id,
    float replay_strength,
    float emotional_salience
);

/**
 * @brief Broadcast homeostasis update
 */
int sleep_bio_async_broadcast_homeostasis(
    sleep_bio_async_bridge_t* bridge,
    float total_weight_before,
    float total_weight_after,
    uint32_t synapses_pruned
);

/**
 * @brief Broadcast adenosine level
 */
int sleep_bio_async_broadcast_adenosine(
    sleep_bio_async_bridge_t* bridge,
    float adenosine_level,
    float sleep_pressure
);

/**
 * @brief Broadcast oscillation change
 */
int sleep_bio_async_broadcast_oscillation(
    sleep_bio_async_bridge_t* bridge,
    float dominant_frequency_hz,
    sleep_state_t current_stage
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to sleep messages
 */
int sleep_bio_async_subscribe_module(
    sleep_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from sleep messages
 */
int sleep_bio_async_unsubscribe_module(
    sleep_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Get subscription count for message type
 */
uint32_t sleep_bio_async_get_subscriber_count(
    const sleep_bio_async_bridge_t* bridge,
    sleep_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int sleep_bio_async_get_stats(
    const sleep_bio_async_bridge_t* bridge,
    sleep_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 */
int sleep_bio_async_reset_stats(sleep_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 */
const char* sleep_bio_msg_type_name(sleep_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 */
void sleep_bio_async_print_summary(const sleep_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_BIO_ASYNC_BRIDGE_H */
