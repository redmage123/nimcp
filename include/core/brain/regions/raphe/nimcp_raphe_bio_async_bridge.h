/**
 * @file nimcp_raphe_bio_async_bridge.h
 * @brief Raphe Nuclei Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Central bio-async integration for Raphe nuclei that provides comprehensive
 *       message routing for serotonin signaling via the bio-router.
 *
 * WHY: The Raphe nuclei are the brain's primary serotonin source and regulate:
 *      - Mood and emotional stability
 *      - Impulse control and patience
 *      - Circadian rhythm modulation
 *      - Pain perception and social behavior
 *
 * HOW: Registers Raphe as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming mood signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * RAPHE OUTPUT PATHWAYS:
 * ----------------------
 * 1. Forebrain projections (mood/cognition):
 *    - Prefrontal cortex: Impulse control
 *    - Limbic system: Mood regulation
 *    - Mapped to: BIO_MSG_RAPHE_MOOD_CHANGE, BIO_MSG_RAPHE_IMPULSE_CONTROL
 *
 * 2. Brainstem projections (pain/sleep):
 *    - Descending pain modulation
 *    - Sleep/wake regulation
 *    - Mapped to: BIO_MSG_RAPHE_PAIN_MODULATION, BIO_MSG_RAPHE_CIRCADIAN_MODULATION
 *
 * 3. Limbic projections (social):
 *    - Amygdala: Social fear modulation
 *    - Hypothalamus: Social behavior
 *    - Mapped to: BIO_MSG_RAPHE_SOCIAL_SIGNAL, BIO_MSG_RAPHE_ANXIETY_STATE
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RAPHE_BIO_ASYNC_BRIDGE_H
#define NIMCP_RAPHE_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/raphe/nimcp_raphe_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define RAPHE_BIO_MAX_SUBSCRIPTIONS        64
#define RAPHE_BIO_MAX_INBOX_SIZE           256
#define RAPHE_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100
#define RAPHE_BIO_MESSAGE_TTL_MS           5000
#define RAPHE_BIO_MOOD_CHANGE_THRESHOLD    0.1f

/* ============================================================================
 * Message Types
 * ============================================================================ */

typedef enum {
    RAPHE_BIO_MSG_5HT_STATE = 0,        /**< Complete 5-HT state broadcast */
    RAPHE_BIO_MSG_MOOD_CHANGE,          /**< Mood state change */
    RAPHE_BIO_MSG_IMPULSE_CONTROL,      /**< Impulse control signal */
    RAPHE_BIO_MSG_PATIENCE,             /**< Patience/delay tolerance */
    RAPHE_BIO_MSG_TONIC_SHIFT,          /**< Tonic 5-HT baseline shift */
    RAPHE_BIO_MSG_CIRCADIAN_MOD,        /**< Circadian rhythm influence */
    RAPHE_BIO_MSG_PAIN_MOD,             /**< Pain perception modulation */
    RAPHE_BIO_MSG_ANXIETY,              /**< Anxiety state update */
    RAPHE_BIO_MSG_SOCIAL,               /**< Social behavior modulation */
    RAPHE_BIO_MSG_PLASTICITY_GATE,      /**< Serotonin plasticity gate */
    RAPHE_BIO_MSG_REQUEST_STATE,        /**< Request current 5-HT state */
    RAPHE_BIO_MSG_MODULATE_5HT,         /**< External 5-HT modulation request */
    RAPHE_BIO_MSG_COUNT
} raphe_bio_msg_type_t;

#define RAPHE_BIO_SUB_5HT_STATE         (1U << RAPHE_BIO_MSG_5HT_STATE)
#define RAPHE_BIO_SUB_MOOD_CHANGE       (1U << RAPHE_BIO_MSG_MOOD_CHANGE)
#define RAPHE_BIO_SUB_IMPULSE_CONTROL   (1U << RAPHE_BIO_MSG_IMPULSE_CONTROL)
#define RAPHE_BIO_SUB_PATIENCE          (1U << RAPHE_BIO_MSG_PATIENCE)
#define RAPHE_BIO_SUB_CIRCADIAN_MOD     (1U << RAPHE_BIO_MSG_CIRCADIAN_MOD)
#define RAPHE_BIO_SUB_PAIN_MOD          (1U << RAPHE_BIO_MSG_PAIN_MOD)
#define RAPHE_BIO_SUB_ANXIETY           (1U << RAPHE_BIO_MSG_ANXIETY)
#define RAPHE_BIO_SUB_SOCIAL            (1U << RAPHE_BIO_MSG_SOCIAL)
#define RAPHE_BIO_SUB_PLASTICITY_GATE   (1U << RAPHE_BIO_MSG_PLASTICITY_GATE)
#define RAPHE_BIO_SUB_ALL               (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

typedef struct {
    bio_message_header_t header;
    float tonic_5ht_level;
    float total_5ht_level;
    float mood_level;                   /**< Positive mood [0, 1] */
    float impulse_inhibition;           /**< Impulse control strength [0, 1] */
    float patience_level;               /**< Delay tolerance [0, 1] */
    float anxiety_level;                /**< Anxiety [0, 1] */
    float social_confidence;            /**< Social confidence [0, 1] */
    bool stress_activated;
    uint64_t timestamp_us;
} raphe_bio_5ht_state_msg_t;

typedef struct {
    bio_message_header_t header;
    float previous_mood;
    float current_mood;
    float change_rate;
    uint32_t trigger_source;
    uint64_t timestamp_us;
} raphe_bio_mood_change_msg_t;

typedef struct {
    bio_message_header_t header;
    float impulse_inhibition;           /**< How much to inhibit [0, 1] */
    float urgency_threshold;            /**< Threshold for action [0, 1] */
    bool inhibit_active;
    uint64_t duration_us;
    uint64_t timestamp_us;
} raphe_bio_impulse_control_msg_t;

typedef struct {
    bio_message_header_t header;
    float patience_level;
    float delay_tolerance_ms;
    float reward_discounting;           /**< Temporal discounting factor */
    uint64_t timestamp_us;
} raphe_bio_patience_msg_t;

typedef struct {
    bio_message_header_t header;
    float anxiety_level;
    float social_anxiety;
    float generalized_anxiety;
    bool panic_threshold_reached;
    uint64_t timestamp_us;
} raphe_bio_anxiety_msg_t;

typedef struct {
    bio_message_header_t header;
    float social_confidence;
    float social_approach;              /**< Approach vs avoidance tendency */
    float dominance_signal;             /**< Social dominance [0, 1] */
    uint64_t timestamp_us;
} raphe_bio_social_msg_t;

typedef struct {
    bio_message_header_t header;
    float gate_strength;
    float learning_rate_multiplier;
    bool gate_open;
    uint32_t target_module;
    uint64_t gate_duration_us;
    uint64_t timestamp_us;
} raphe_bio_plasticity_gate_msg_t;

typedef struct {
    bio_message_header_t header;
    float modulation_amount;
    bool is_relative;
    uint32_t requester_module;
    uint32_t request_reason;
    uint64_t timestamp_us;
} raphe_bio_modulate_request_msg_t;

/* ============================================================================
 * Configuration and Statistics
 * ============================================================================ */

typedef struct {
    bio_module_id_t module_id;
    uint32_t msg_type_mask;
    bool active;
    uint64_t subscription_time;
    uint64_t messages_sent;
} raphe_bio_subscription_t;

typedef struct {
    uint32_t ht_broadcast_interval_ms;
    bool enable_auto_broadcast;
    uint32_t max_inbox_process_per_update;
    uint32_t message_ttl_ms;
    float mood_change_threshold;
    nimcp_bio_channel_type_t default_channel;
    uint32_t max_subscriptions;
    bool enable_mood_routing;
    bool enable_social_routing;
    bool enable_plasticity_gating;
    bool enable_logging;
} raphe_bio_async_config_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t broadcasts_sent;
    uint64_t ht_state_broadcasts;
    uint64_t mood_changes_sent;
    uint64_t impulse_signals_sent;
    uint64_t social_signals_sent;
    uint64_t plasticity_gates_sent;
    uint32_t active_subscriptions;
    uint32_t peak_subscriptions;
    uint64_t last_broadcast_time_us;
    uint64_t handler_errors;
    uint64_t routing_errors;
} raphe_bio_async_stats_t;

typedef struct raphe_bio_async_bridge_struct raphe_bio_async_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

int raphe_bio_async_default_config(raphe_bio_async_config_t* config);
raphe_bio_async_bridge_t* raphe_bio_async_bridge_create(const raphe_bio_async_config_t* config);
void raphe_bio_async_bridge_destroy(raphe_bio_async_bridge_t* bridge);

int raphe_bio_async_connect(raphe_bio_async_bridge_t* bridge, nimcp_raphe_adapter_t adapter, bio_router_t router);
int raphe_bio_async_disconnect(raphe_bio_async_bridge_t* bridge);
bool raphe_bio_async_is_connected(const raphe_bio_async_bridge_t* bridge);

int raphe_bio_async_process_inbox(raphe_bio_async_bridge_t* bridge, uint32_t max_messages);
int raphe_bio_async_update(raphe_bio_async_bridge_t* bridge, uint32_t delta_ms);

int raphe_bio_async_broadcast_5ht_state(raphe_bio_async_bridge_t* bridge);
int raphe_bio_async_broadcast_mood_change(raphe_bio_async_bridge_t* bridge, float prev_mood, float cur_mood, uint32_t source);
int raphe_bio_async_broadcast_impulse_control(raphe_bio_async_bridge_t* bridge, float inhibition, bool active);
int raphe_bio_async_broadcast_patience(raphe_bio_async_bridge_t* bridge, float patience, float delay_tolerance);
int raphe_bio_async_broadcast_anxiety(raphe_bio_async_bridge_t* bridge, float anxiety, float social_anxiety);
int raphe_bio_async_broadcast_social(raphe_bio_async_bridge_t* bridge, float confidence, float approach);
int raphe_bio_async_send_plasticity_gate(raphe_bio_async_bridge_t* bridge, float gate_strength, float lr_mult, uint32_t target);
int raphe_bio_async_broadcast_tonic_shift(raphe_bio_async_bridge_t* bridge, float new_tonic);

int raphe_bio_async_subscribe_module(raphe_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
int raphe_bio_async_unsubscribe_module(raphe_bio_async_bridge_t* bridge, uint32_t module_id);
int raphe_bio_async_update_subscription(raphe_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
uint32_t raphe_bio_async_get_subscriber_count(const raphe_bio_async_bridge_t* bridge, raphe_bio_msg_type_t msg_type);

int raphe_bio_async_get_stats(const raphe_bio_async_bridge_t* bridge, raphe_bio_async_stats_t* stats);
int raphe_bio_async_reset_stats(raphe_bio_async_bridge_t* bridge);
const char* raphe_bio_msg_type_name(raphe_bio_msg_type_t msg_type);
void raphe_bio_async_print_summary(const raphe_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_BIO_ASYNC_BRIDGE_H */
