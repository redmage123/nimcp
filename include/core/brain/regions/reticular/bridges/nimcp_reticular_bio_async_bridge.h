/**
 * @file nimcp_reticular_bio_async_bridge.h
 * @brief Reticular Formation Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for the Reticular Formation that provides
 *       comprehensive message routing for arousal, neuromodulation, autonomic
 *       regulation, reflex control, and sleep-wake transitions via bio-router.
 *
 * WHY: The reticular formation is the brainstem's master controller:
 *      - Broadcast arousal state changes to cortex, thalamus, hypothalamus
 *      - Route neuromodulator releases (5-HT, NE, ACh, DA) system-wide
 *      - Signal autonomic adjustments to visceral control centers
 *      - Coordinate sleep-wake transitions with hypothalamus and cortex
 *      - Alert defensive systems on reflex triggers
 *
 * HOW: Registers reticular formation as a bio-router module, maintains
 *      subscription registry, provides typed message broadcast APIs,
 *      and processes incoming arousal modulation requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * RETICULAR OUTPUT PATHWAYS:
 * --------------------------
 * 1. Ascending Reticular Activating System (ARAS):
 *    - Arousal signals to thalamus and cortex
 *    - Wakefulness maintenance
 *    - Mapped to: RETICULAR_BIO_MSG_AROUSAL_CHANGE
 *
 * 2. Neuromodulatory Nuclei:
 *    - Raphe (5-HT), LC (NE), PPN (ACh), VTA (DA)
 *    - Mood, vigilance, attention, reward
 *    - Mapped to: RETICULAR_BIO_MSG_NEUROMODULATOR
 *
 * 3. Autonomic Centers:
 *    - Cardiovascular, respiratory, vasomotor
 *    - Sympathetic/parasympathetic balance
 *    - Mapped to: RETICULAR_BIO_MSG_AUTONOMIC
 *
 * 4. Motor Control:
 *    - Reticulospinal tracts for postural tone
 *    - REM atonia via pontine RF
 *    - Mapped to: RETICULAR_BIO_MSG_MOTOR_TONE
 *
 * 5. Reflex Centers:
 *    - Swallowing, coughing, startle
 *    - Mapped to: RETICULAR_BIO_MSG_REFLEX_TRIGGER
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RETICULAR_BIO_ASYNC_BRIDGE_H
#define NIMCP_RETICULAR_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/reticular/nimcp_reticular.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for reticular formation in bio-async system */
#define BIO_MODULE_ID_RETICULAR         0x5005

/** Maximum number of module subscriptions */
#define RETICULAR_BRIDGE_MAX_SUBSCRIPTIONS     64

/** Maximum pending messages in inbox/outbox */
#define RETICULAR_BRIDGE_MAX_INBOX_SIZE        256
#define RETICULAR_BRIDGE_MAX_OUTBOX_SIZE       128

/** Default broadcast interval (ms) */
#define RETICULAR_BRIDGE_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define RETICULAR_BRIDGE_MESSAGE_TTL_MS        5000

/** Arousal change threshold for broadcast */
#define RETICULAR_BRIDGE_AROUSAL_THRESHOLD     0.05f

/** Modulator release threshold for broadcast */
#define RETICULAR_BRIDGE_MODULATOR_THRESHOLD   0.1f

/* ============================================================================
 * Message Payload Structures
 * Uses message types from nimcp_reticular.h: reticular_bio_msg_type_t
 * ============================================================================ */

/**
 * @brief Arousal change message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Arousal state */
    reticular_arousal_state_t current_state;  /**< Current arousal state */
    reticular_arousal_state_t previous_state; /**< Previous arousal state */
    float arousal_level;                /**< Continuous arousal [0, 1] */
    float arousal_delta;                /**< Change from previous */
    float arousal_momentum;             /**< Rate of change */

    /* Contextual info */
    bool is_transition;                 /**< State transition occurring */
    bool is_emergency;                  /**< Emergency arousal change */

    uint64_t timestamp_us;              /**< Change timestamp */
} reticular_bridge_arousal_msg_t;

/**
 * @brief Neuromodulator release message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Modulator identity */
    reticular_modulator_t modulator;    /**< Which neuromodulator */
    reticular_nucleus_t source_nucleus; /**< Releasing nucleus */

    /* Release parameters */
    float concentration;                /**< Current concentration [0, 1] */
    float release_rate;                 /**< Release rate */
    float baseline;                     /**< Baseline level */
    float delta;                        /**< Change from baseline */

    /* Effect parameters */
    float target_effect;                /**< Expected target effect */
    bool is_tonic;                      /**< Tonic vs phasic release */
    bool is_burst;                      /**< Burst release */

    uint64_t timestamp_us;              /**< Release timestamp */
} reticular_bridge_modulator_msg_t;

/**
 * @brief Reflex activation message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Reflex identity */
    reticular_reflex_t reflex_type;     /**< Which reflex */

    /* Activation parameters */
    float stimulus_strength;            /**< Triggering stimulus [0, 1] */
    float response_magnitude;           /**< Response strength [0, 1] */
    float threshold;                    /**< Current threshold */

    /* State */
    bool is_active;                     /**< Currently active */
    bool is_habituated;                 /**< Has habituated */
    uint32_t trigger_count;             /**< Total triggers */

    uint64_t timestamp_us;              /**< Trigger timestamp */
} reticular_bridge_reflex_msg_t;

/**
 * @brief Autonomic state change message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Autonomic function */
    reticular_autonomic_t function;     /**< Which autonomic function */

    /* Balance parameters */
    float sympathetic_tone;             /**< Sympathetic activity [0, 1] */
    float parasympathetic_tone;         /**< Parasympathetic activity [0, 1] */
    float balance;                      /**< Balance [-1 para, +1 symp] */
    float balance_delta;                /**< Change in balance */

    /* Setpoints */
    float setpoint;                     /**< Target value */
    float current_value;                /**< Current measured value */

    /* Flags */
    bool is_stress_response;            /**< Stress-induced change */
    bool is_recovery;                   /**< Recovery to baseline */

    uint64_t timestamp_us;              /**< Change timestamp */
} reticular_bridge_autonomic_msg_t;

/**
 * @brief Sleep-wake transition message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Transition info */
    reticular_arousal_state_t from_state;   /**< Departing state */
    reticular_arousal_state_t to_state;     /**< Entering state */

    /* Sleep metrics */
    float circadian_drive;              /**< Circadian alerting [0, 1] */
    float homeostatic_pressure;         /**< Sleep pressure [0, 1] */
    float sleep_propensity;             /**< Combined sleep drive [0, 1] */

    /* Transition details */
    bool is_wake_to_sleep;              /**< Falling asleep */
    bool is_sleep_to_wake;              /**< Waking up */
    bool is_rem_entry;                  /**< Entering REM */
    bool is_rem_exit;                   /**< Exiting REM */
    float transition_progress;          /**< Progress [0, 1] */

    uint64_t timestamp_us;              /**< Transition timestamp */
} reticular_bridge_sleep_wake_msg_t;

/**
 * @brief Motor tone adjustment message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Tone parameters */
    float postural_tone;                /**< Antigravity tone [0, 1] */
    float limb_tone;                    /**< Limb muscle tone [0, 1] */
    float atonia_level;                 /**< REM atonia [0, 1] */
    float locomotor_drive;              /**< MLR drive [0, 1] */
    float startle_readiness;            /**< Startle response [0, 1] */

    /* State flags */
    bool rem_atonia_active;             /**< REM sleep muscle inhibition */
    bool is_startle_triggered;          /**< Startle reflex active */
    bool is_freeze_response;            /**< Defensive freeze */

    /* Change info */
    float tone_delta;                   /**< Change magnitude */

    uint64_t timestamp_us;              /**< Adjustment timestamp */
} reticular_bridge_motor_tone_msg_t;

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
} reticular_bridge_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Reticular Formation bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t arousal_broadcast_interval_ms;  /**< Arousal broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state changes */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Thresholds */
    float arousal_change_threshold;          /**< Min arousal delta for broadcast */
    float modulator_change_threshold;        /**< Min modulator delta for broadcast */

    /* Priority settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Urgent channel */
    nimcp_bio_channel_type_t modulator_channel; /**< Modulator events channel */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum subscriptions */

    /* Feature flags */
    bool enable_arousal_broadcast;           /**< Enable arousal routing */
    bool enable_modulator_broadcast;         /**< Enable modulator routing */
    bool enable_reflex_broadcast;            /**< Enable reflex alerts */
    bool enable_autonomic_broadcast;         /**< Enable autonomic routing */
    bool enable_sleep_wake_broadcast;        /**< Enable sleep-wake routing */
    bool enable_motor_tone_broadcast;        /**< Enable motor tone routing */
    bool enable_logging;                     /**< Enable logging */
} reticular_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t broadcasts_sent;

    /* Per-type counts */
    uint64_t arousal_broadcasts;
    uint64_t modulator_releases;
    uint64_t reflex_triggers;
    uint64_t autonomic_changes;
    uint64_t sleep_wake_transitions;
    uint64_t motor_tone_adjustments;

    /* Subscription stats */
    uint32_t active_subscriptions;
    uint32_t peak_subscriptions;

    /* Timing stats */
    uint64_t last_broadcast_time_us;
    float avg_message_latency_us;

    /* Errors */
    uint64_t handler_errors;
    uint64_t routing_errors;
} reticular_bridge_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct reticular_bridge_struct reticular_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int reticular_bridge_default_config(reticular_bridge_config_t* config);

/**
 * @brief Create reticular formation bio-async bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
reticular_bridge_t* reticular_bridge_create(const reticular_bridge_config_t* config);

/**
 * @brief Destroy reticular formation bio-async bridge
 * @param bridge Bridge to destroy
 */
void reticular_bridge_destroy(reticular_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to reticular formation and router
 * @param bridge Bridge handle
 * @param reticular Reticular formation instance
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int reticular_bridge_connect(
    reticular_bridge_t* bridge,
    nimcp_reticular_t* reticular,
    bio_router_t router);

/**
 * @brief Disconnect bridge from router
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int reticular_bridge_disconnect(reticular_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge to check
 * @return true if connected
 */
bool reticular_bridge_is_connected(const reticular_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int reticular_bridge_process_inbox(reticular_bridge_t* bridge, uint32_t max_messages);

/**
 * @brief Update bridge state and auto-broadcasts
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int reticular_bridge_update(reticular_bridge_t* bridge, uint32_t delta_ms);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast arousal level change
 * @param bridge Bridge handle
 * @param level Current arousal level [0, 1]
 * @param state Current arousal state
 * @param is_transition True if state transition
 * @return 0 on success, -1 on error
 */
int reticular_bridge_broadcast_arousal(
    reticular_bridge_t* bridge,
    float level,
    reticular_arousal_state_t state,
    bool is_transition);

/**
 * @brief Broadcast neuromodulator release
 * @param bridge Bridge handle
 * @param modulator Which neuromodulator
 * @param concentration Current concentration [0, 1]
 * @param is_burst True if burst release
 * @return 0 on success, -1 on error
 */
int reticular_bridge_broadcast_modulator(
    reticular_bridge_t* bridge,
    reticular_modulator_t modulator,
    float concentration,
    bool is_burst);

/**
 * @brief Broadcast reflex trigger
 * @param bridge Bridge handle
 * @param reflex Which reflex
 * @param stimulus Stimulus strength [0, 1]
 * @param response Response magnitude [0, 1]
 * @return 0 on success, -1 on error
 */
int reticular_bridge_broadcast_reflex(
    reticular_bridge_t* bridge,
    reticular_reflex_t reflex,
    float stimulus,
    float response);

/**
 * @brief Broadcast autonomic state change
 * @param bridge Bridge handle
 * @param function Which autonomic function
 * @param sympathetic Sympathetic tone [0, 1]
 * @param parasympathetic Parasympathetic tone [0, 1]
 * @return 0 on success, -1 on error
 */
int reticular_bridge_broadcast_autonomic(
    reticular_bridge_t* bridge,
    reticular_autonomic_t function,
    float sympathetic,
    float parasympathetic);

/**
 * @brief Broadcast sleep-wake transition
 * @param bridge Bridge handle
 * @param from_state Departing arousal state
 * @param to_state Entering arousal state
 * @param progress Transition progress [0, 1]
 * @return 0 on success, -1 on error
 */
int reticular_bridge_broadcast_sleep_wake(
    reticular_bridge_t* bridge,
    reticular_arousal_state_t from_state,
    reticular_arousal_state_t to_state,
    float progress);

/**
 * @brief Broadcast motor tone adjustment
 * @param bridge Bridge handle
 * @param postural Postural tone [0, 1]
 * @param atonia Atonia level [0, 1]
 * @param rem_active REM atonia active
 * @return 0 on success, -1 on error
 */
int reticular_bridge_broadcast_motor_tone(
    reticular_bridge_t* bridge,
    float postural,
    float atonia,
    bool rem_active);

/* ============================================================================
 * Subscription API
 * ============================================================================ */

/**
 * @brief Subscribe module to reticular messages
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use RETICULAR_BIO_SUB_* from nimcp_reticular.h)
 * @return 0 on success, -1 on error
 */
int reticular_bridge_subscribe_module(
    reticular_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types);

/**
 * @brief Unsubscribe module from reticular messages
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int reticular_bridge_unsubscribe_module(reticular_bridge_t* bridge, uint32_t module_id);

/**
 * @brief Get subscription count for message type
 * @param bridge Bridge handle
 * @param msg_type Message type to query (reticular_bio_msg_type_t from nimcp_reticular.h)
 * @return Number of subscribers
 */
uint32_t reticular_bridge_get_subscriber_count(
    const reticular_bridge_t* bridge,
    reticular_bio_msg_type_t msg_type);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int reticular_bridge_get_stats(
    const reticular_bridge_t* bridge,
    reticular_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int reticular_bridge_reset_stats(reticular_bridge_t* bridge);

/**
 * @brief Get message type name
 * @param msg_type Message type (reticular_bio_msg_type_t from nimcp_reticular.h)
 * @return Static string name
 */
const char* reticular_bridge_msg_type_name(reticular_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 * @param bridge Bridge handle
 */
void reticular_bridge_print_summary(const reticular_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETICULAR_BIO_ASYNC_BRIDGE_H */
