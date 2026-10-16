/**
 * @file nimcp_lc_bio_async_bridge.h
 * @brief Locus Coeruleus Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Central bio-async integration for locus coeruleus that provides comprehensive
 *       message routing for norepinephrine signaling via the bio-router.
 *
 * WHY: The locus coeruleus is the brain's primary norepinephrine source and regulates:
 *      - Arousal and alertness levels system-wide
 *      - Attention and gain modulation
 *      - Stress responses and vigilance
 *      - Plasticity gating during salient events
 *
 * HOW: Registers LC as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming modulation requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * LC OUTPUT PATHWAYS:
 * -------------------
 * 1. Cortical projections (widespread):
 *    - Prefrontal cortex: Executive function modulation
 *    - Parietal cortex: Attention enhancement
 *    - Mapped to: BIO_MSG_LC_GAIN_MODULATION, BIO_MSG_LC_ATTENTION_BIAS
 *
 * 2. Limbic projections:
 *    - Amygdala: Fear/threat response amplification
 *    - Hippocampus: Memory encoding modulation
 *    - Mapped to: BIO_MSG_LC_AROUSAL_CHANGE, BIO_MSG_LC_PLASTICITY_GATE
 *
 * 3. Brainstem/Autonomic:
 *    - Autonomic nuclei: Fight-or-flight responses
 *    - Mapped to: BIO_MSG_LC_STRESS_RESPONSE
 *
 * LC INPUT PATHWAYS:
 * ------------------
 * 1. Hypothalamic (homeostatic):
 *    - Orexin/hypocretin from lateral hypothalamus
 *    - Arousal drive signals
 *
 * 2. Limbic (emotional):
 *    - Amygdala: Threat/novelty signals
 *    - CRF from PVN during stress
 *
 * 3. Prefrontal (cognitive):
 *    - Top-down attention control
 *    - Task-related modulation
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

#ifndef NIMCP_LC_BIO_ASYNC_BRIDGE_H
#define NIMCP_LC_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define LC_BIO_MAX_SUBSCRIPTIONS        64

/** Maximum pending messages in inbox */
#define LC_BIO_MAX_INBOX_SIZE           256

/** Maximum pending messages in outbox */
#define LC_BIO_MAX_OUTBOX_SIZE          128

/** Default broadcast interval for NE state (ms) */
#define LC_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define LC_BIO_MESSAGE_TTL_MS           5000

/** Phasic burst threshold for urgent messaging */
#define LC_BIO_PHASIC_BURST_THRESHOLD   0.7f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief LC bio-async message types
 *
 * WHAT: Message type enumeration for LC bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific LC output pathway
 */
typedef enum {
    LC_BIO_MSG_NE_STATE = 0,            /**< Complete NE state broadcast */
    LC_BIO_MSG_AROUSAL_CHANGE,          /**< Arousal level change */
    LC_BIO_MSG_ALERTNESS_UPDATE,        /**< Alertness modulation */
    LC_BIO_MSG_PHASIC_BURST,            /**< Phasic NE burst (novelty/salience) */
    LC_BIO_MSG_TONIC_SHIFT,             /**< Tonic baseline change */
    LC_BIO_MSG_GAIN_MODULATION,         /**< Neural gain modulation signal */
    LC_BIO_MSG_STRESS_RESPONSE,         /**< Stress-induced NE release */
    LC_BIO_MSG_VIGILANCE_UPDATE,        /**< Vigilance state update */
    LC_BIO_MSG_ATTENTION_BIAS,          /**< Attention bias from NE */
    LC_BIO_MSG_PLASTICITY_GATE,         /**< Plasticity gating signal */
    LC_BIO_MSG_REQUEST_STATE,           /**< Request current NE state */
    LC_BIO_MSG_MODULATE_NE,             /**< External NE modulation request */
    LC_BIO_MSG_COUNT
} lc_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define LC_BIO_SUB_NE_STATE             (1U << LC_BIO_MSG_NE_STATE)
#define LC_BIO_SUB_AROUSAL_CHANGE       (1U << LC_BIO_MSG_AROUSAL_CHANGE)
#define LC_BIO_SUB_ALERTNESS_UPDATE     (1U << LC_BIO_MSG_ALERTNESS_UPDATE)
#define LC_BIO_SUB_PHASIC_BURST         (1U << LC_BIO_MSG_PHASIC_BURST)
#define LC_BIO_SUB_TONIC_SHIFT          (1U << LC_BIO_MSG_TONIC_SHIFT)
#define LC_BIO_SUB_GAIN_MODULATION      (1U << LC_BIO_MSG_GAIN_MODULATION)
#define LC_BIO_SUB_STRESS_RESPONSE      (1U << LC_BIO_MSG_STRESS_RESPONSE)
#define LC_BIO_SUB_VIGILANCE_UPDATE     (1U << LC_BIO_MSG_VIGILANCE_UPDATE)
#define LC_BIO_SUB_ATTENTION_BIAS       (1U << LC_BIO_MSG_ATTENTION_BIAS)
#define LC_BIO_SUB_PLASTICITY_GATE      (1U << LC_BIO_MSG_PLASTICITY_GATE)
#define LC_BIO_SUB_ALL                  (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief NE state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Core NE levels */
    float tonic_ne_level;               /**< Tonic baseline NE [0, 1] */
    float phasic_ne_level;              /**< Phasic NE component [0, 1] */
    float total_ne_level;               /**< Combined NE level [0, 1] */

    /* Functional outputs */
    float arousal_level;                /**< Current arousal [0, 1] */
    float alertness;                    /**< Alertness level [0, 1] */
    float vigilance;                    /**< Vigilance/sustained attention [0, 1] */
    float gain_modulation;              /**< Neural gain factor [0.5, 2.0] */

    /* State flags */
    bool phasic_mode_active;            /**< Currently in phasic mode */
    bool stress_activated;              /**< Stress response active */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} lc_bio_ne_state_msg_t;

/**
 * @brief Arousal change message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float previous_arousal;             /**< Previous arousal level */
    float current_arousal;              /**< Current arousal level */
    float change_rate;                  /**< Rate of change per second */

    uint32_t trigger_source;            /**< What triggered the change */
    uint64_t timestamp_us;              /**< Change timestamp */
} lc_bio_arousal_change_msg_t;

/**
 * @brief Phasic burst message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float burst_magnitude;              /**< Burst intensity [0, 1] */
    float novelty_score;                /**< Novelty/salience score [0, 1] */
    float surprise_component;           /**< Unexpected event component */

    uint32_t trigger_source;            /**< What triggered the burst */
    uint32_t target_regions;            /**< Bitmask of target brain regions */

    uint64_t burst_onset_us;            /**< When burst began */
    uint64_t expected_duration_us;      /**< Expected burst duration */
} lc_bio_phasic_burst_msg_t;

/**
 * @brief Gain modulation message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float gain_factor;                  /**< Neural gain multiplier [0.5, 2.0] */
    float signal_to_noise;              /**< Signal-to-noise ratio improvement */

    uint32_t target_module;             /**< Target module (0 = broadcast) */
    bool is_targeted;                   /**< Targeted vs broadcast */

    uint64_t timestamp_us;              /**< Modulation timestamp */
} lc_bio_gain_modulation_msg_t;

/**
 * @brief Stress response message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float stress_intensity;             /**< Stress intensity [0, 1] */
    float ne_release_rate;              /**< NE release rate [0, 1] */

    bool is_acute;                      /**< Acute vs chronic stress */
    bool fight_or_flight_mode;          /**< Fight-or-flight activated */

    uint32_t stressor_type;             /**< Type of stressor */
    uint64_t stress_onset_us;           /**< When stress began */
    uint64_t timestamp_us;              /**< Current timestamp */
} lc_bio_stress_response_msg_t;

/**
 * @brief Plasticity gate message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float gate_strength;                /**< Gating strength [0, 1] */
    float learning_rate_multiplier;     /**< LR multiplier from NE [0.5, 3.0] */
    bool gate_open;                     /**< Whether plasticity gate is open */

    uint32_t target_module;             /**< Target plasticity module */
    uint64_t gate_duration_us;          /**< Expected gate duration */
    uint64_t timestamp_us;              /**< Gate timestamp */
} lc_bio_plasticity_gate_msg_t;

/**
 * @brief NE modulation request payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    float modulation_amount;            /**< Amount to modulate [-1, 1] */
    bool is_relative;                   /**< Relative vs absolute */
    bool affect_tonic;                  /**< Affect tonic baseline */
    bool affect_phasic;                 /**< Affect phasic response */

    uint32_t requester_module;          /**< Who is requesting */
    uint32_t request_reason;            /**< Why modulation requested */

    uint64_t timestamp_us;              /**< Request timestamp */
} lc_bio_modulate_request_msg_t;

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
} lc_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief LC bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t ne_broadcast_interval_ms;       /**< NE state broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast NE state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float phasic_burst_threshold;            /**< Threshold for phasic messages */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Channel for urgent messages */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_gain_modulation;             /**< Enable gain modulation routing */
    bool enable_stress_routing;              /**< Enable stress signal routing */
    bool enable_plasticity_gating;           /**< Enable plasticity gating */
    bool enable_logging;                     /**< Enable message logging */
} lc_bio_async_config_t;

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
    uint64_t ne_state_broadcasts;            /**< NE state broadcasts */
    uint64_t phasic_bursts_sent;             /**< Phasic burst notifications */
    uint64_t gain_modulations_sent;          /**< Gain modulation signals */
    uint64_t stress_responses_sent;          /**< Stress response broadcasts */
    uint64_t plasticity_gates_sent;          /**< Plasticity gate signals */

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
} lc_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief LC bio-async bridge handle
 */
typedef struct lc_bio_async_bridge_struct lc_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int lc_bio_async_default_config(lc_bio_async_config_t* config);

/**
 * @brief Create LC bio-async bridge
 */
lc_bio_async_bridge_t* lc_bio_async_bridge_create(
    const lc_bio_async_config_t* config
);

/**
 * @brief Destroy LC bio-async bridge
 */
void lc_bio_async_bridge_destroy(lc_bio_async_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to LC adapter and router
 */
int lc_bio_async_connect(
    lc_bio_async_bridge_t* bridge,
    nimcp_lc_adapter_t adapter,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 */
int lc_bio_async_disconnect(lc_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 */
bool lc_bio_async_is_connected(const lc_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 */
int lc_bio_async_process_inbox(
    lc_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 */
int lc_bio_async_update(
    lc_bio_async_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast complete NE state to all subscribers
 */
int lc_bio_async_broadcast_ne_state(lc_bio_async_bridge_t* bridge);

/**
 * @brief Broadcast arousal change
 */
int lc_bio_async_broadcast_arousal_change(
    lc_bio_async_bridge_t* bridge,
    float previous_arousal,
    float current_arousal,
    uint32_t trigger_source
);

/**
 * @brief Broadcast phasic burst (novelty/salience event)
 */
int lc_bio_async_broadcast_phasic_burst(
    lc_bio_async_bridge_t* bridge,
    float magnitude,
    float novelty_score,
    uint32_t trigger_source
);

/**
 * @brief Send gain modulation signal
 */
int lc_bio_async_send_gain_modulation(
    lc_bio_async_bridge_t* bridge,
    float gain_factor,
    uint32_t target_module
);

/**
 * @brief Broadcast stress response
 */
int lc_bio_async_broadcast_stress_response(
    lc_bio_async_bridge_t* bridge,
    float stress_intensity,
    bool is_acute
);

/**
 * @brief Send plasticity gate signal
 */
int lc_bio_async_send_plasticity_gate(
    lc_bio_async_bridge_t* bridge,
    float gate_strength,
    float lr_multiplier,
    uint32_t target_module
);

/**
 * @brief Broadcast vigilance update
 */
int lc_bio_async_broadcast_vigilance(
    lc_bio_async_bridge_t* bridge,
    float vigilance_level
);

/**
 * @brief Broadcast tonic baseline shift
 */
int lc_bio_async_broadcast_tonic_shift(
    lc_bio_async_bridge_t* bridge,
    float new_tonic_level
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to LC messages
 */
int lc_bio_async_subscribe_module(
    lc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from LC messages
 */
int lc_bio_async_unsubscribe_module(
    lc_bio_async_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 */
int lc_bio_async_update_subscription(
    lc_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 */
uint32_t lc_bio_async_get_subscriber_count(
    const lc_bio_async_bridge_t* bridge,
    lc_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int lc_bio_async_get_stats(
    const lc_bio_async_bridge_t* bridge,
    lc_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 */
int lc_bio_async_reset_stats(lc_bio_async_bridge_t* bridge);

/**
 * @brief Get message type name
 */
const char* lc_bio_msg_type_name(lc_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 */
void lc_bio_async_print_summary(const lc_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_BIO_ASYNC_BRIDGE_H */
