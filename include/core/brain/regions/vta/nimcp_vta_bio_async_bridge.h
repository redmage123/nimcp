/**
 * @file nimcp_vta_bio_async_bridge.h
 * @brief Ventral Tegmental Area Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Central bio-async integration for VTA that provides comprehensive
 *       message routing for dopamine signaling via the bio-router.
 *
 * WHY: The VTA is the brain's primary mesolimbic dopamine source and regulates:
 *      - Reward prediction and learning
 *      - Motivation and incentive salience
 *      - Value prediction updates
 *      - Plasticity gating for reward-based learning
 *
 * HOW: Registers VTA as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming reward signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * VTA OUTPUT PATHWAYS:
 * --------------------
 * 1. Mesolimbic pathway (reward):
 *    - Nucleus accumbens: Reward processing
 *    - Amygdala: Emotional value assignment
 *    - Mapped to: BIO_MSG_VTA_RPE, BIO_MSG_VTA_DOPAMINE_BURST
 *
 * 2. Mesocortical pathway (cognition):
 *    - Prefrontal cortex: Working memory, planning
 *    - Mapped to: BIO_MSG_VTA_VALUE_UPDATE, BIO_MSG_VTA_MOTIVATION_UPDATE
 *
 * 3. Nigrostriatal (motor):
 *    - Striatum: Action selection
 *    - Mapped to: BIO_MSG_VTA_LEARNING_SIGNAL
 *
 * VTA INPUT PATHWAYS:
 * -------------------
 * 1. Hypothalamic (homeostatic):
 *    - Lateral hypothalamus: Drive signals
 *    - Reward prediction inputs
 *
 * 2. Limbic (emotional):
 *    - Habenula: Negative RPE signals
 *    - Amygdala: Fear/reward interaction
 *
 * 3. Prefrontal (cognitive):
 *    - Top-down goal signals
 *    - Value predictions
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VTA_BIO_ASYNC_BRIDGE_H
#define NIMCP_VTA_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/vta/nimcp_vta_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VTA_BIO_MAX_SUBSCRIPTIONS        64
#define VTA_BIO_MAX_INBOX_SIZE           256
#define VTA_BIO_MAX_OUTBOX_SIZE          128
#define VTA_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50
#define VTA_BIO_MESSAGE_TTL_MS           5000
#define VTA_BIO_PHASIC_DA_THRESHOLD      0.7f

/* ============================================================================
 * Message Types
 * ============================================================================ */

typedef enum {
    VTA_BIO_MSG_DA_STATE = 0,           /**< Complete DA state broadcast */
    VTA_BIO_MSG_RPE,                    /**< Reward prediction error */
    VTA_BIO_MSG_DA_BURST,               /**< Phasic dopamine burst */
    VTA_BIO_MSG_DA_DIP,                 /**< Phasic dopamine dip */
    VTA_BIO_MSG_TONIC_CHANGE,           /**< Tonic DA baseline change */
    VTA_BIO_MSG_MOTIVATION,             /**< Motivation signal update */
    VTA_BIO_MSG_VALUE_UPDATE,           /**< Value prediction update */
    VTA_BIO_MSG_INCENTIVE_SALIENCE,     /**< Incentive salience signal */
    VTA_BIO_MSG_LEARNING_SIGNAL,        /**< DA-based learning signal */
    VTA_BIO_MSG_PLASTICITY_GATE,        /**< Dopamine plasticity gate */
    VTA_BIO_MSG_REQUEST_STATE,          /**< Request current DA state */
    VTA_BIO_MSG_MODULATE_DA,            /**< External DA modulation request */
    VTA_BIO_MSG_COUNT
} vta_bio_msg_type_t;

#define VTA_BIO_SUB_DA_STATE            (1U << VTA_BIO_MSG_DA_STATE)
#define VTA_BIO_SUB_RPE                 (1U << VTA_BIO_MSG_RPE)
#define VTA_BIO_SUB_DA_BURST            (1U << VTA_BIO_MSG_DA_BURST)
#define VTA_BIO_SUB_DA_DIP              (1U << VTA_BIO_MSG_DA_DIP)
#define VTA_BIO_SUB_TONIC_CHANGE        (1U << VTA_BIO_MSG_TONIC_CHANGE)
#define VTA_BIO_SUB_MOTIVATION          (1U << VTA_BIO_MSG_MOTIVATION)
#define VTA_BIO_SUB_VALUE_UPDATE        (1U << VTA_BIO_MSG_VALUE_UPDATE)
#define VTA_BIO_SUB_INCENTIVE_SALIENCE  (1U << VTA_BIO_MSG_INCENTIVE_SALIENCE)
#define VTA_BIO_SUB_LEARNING_SIGNAL     (1U << VTA_BIO_MSG_LEARNING_SIGNAL)
#define VTA_BIO_SUB_PLASTICITY_GATE     (1U << VTA_BIO_MSG_PLASTICITY_GATE)
#define VTA_BIO_SUB_ALL                 (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

typedef struct {
    bio_message_header_t header;
    float tonic_da_level;
    float phasic_da_level;
    float total_da_level;
    float motivation_level;
    float value_estimate;
    float incentive_salience;
    bool reward_predicted;
    bool phasic_mode_active;
    uint64_t timestamp_us;
} vta_bio_da_state_msg_t;

typedef struct {
    bio_message_header_t header;
    float rpe;                          /**< RPE value [-1, +1] */
    float predicted_value;              /**< Expected value */
    float actual_value;                 /**< Received value */
    float confidence;                   /**< Prediction confidence */
    uint32_t context_id;                /**< Learning context */
    uint64_t timestamp_us;
} vta_bio_rpe_msg_t;

typedef struct {
    bio_message_header_t header;
    float burst_magnitude;
    float reward_magnitude;
    uint32_t reward_source;
    uint32_t target_regions;
    uint64_t burst_onset_us;
    uint64_t expected_duration_us;
} vta_bio_da_burst_msg_t;

typedef struct {
    bio_message_header_t header;
    float dip_magnitude;
    float omission_severity;
    float expected_reward;
    uint32_t context_id;
    uint64_t dip_onset_us;
    uint64_t timestamp_us;
} vta_bio_da_dip_msg_t;

typedef struct {
    bio_message_header_t header;
    float gate_strength;
    float learning_rate_multiplier;
    bool gate_open;
    bool is_reward_gate;
    uint32_t target_module;
    uint64_t gate_duration_us;
    uint64_t timestamp_us;
} vta_bio_plasticity_gate_msg_t;

typedef struct {
    bio_message_header_t header;
    float motivation_level;
    float effort_willingness;
    float goal_proximity;
    uint32_t active_goal_id;
    uint64_t timestamp_us;
} vta_bio_motivation_msg_t;

typedef struct {
    bio_message_header_t header;
    float modulation_amount;
    bool is_relative;
    bool affect_tonic;
    bool affect_phasic;
    uint32_t requester_module;
    uint32_t request_reason;
    uint64_t timestamp_us;
} vta_bio_modulate_request_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

typedef struct {
    bio_module_id_t module_id;
    uint32_t msg_type_mask;
    bool active;
    uint64_t subscription_time;
    uint64_t messages_sent;
} vta_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

typedef struct {
    uint32_t da_broadcast_interval_ms;
    bool enable_auto_broadcast;
    uint32_t max_inbox_process_per_update;
    uint32_t message_ttl_ms;
    float phasic_da_threshold;
    nimcp_bio_channel_type_t default_channel;
    nimcp_bio_channel_type_t urgent_channel;
    uint32_t max_subscriptions;
    bool enable_rpe_routing;
    bool enable_motivation_routing;
    bool enable_plasticity_gating;
    bool enable_logging;
} vta_bio_async_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t broadcasts_sent;
    uint64_t da_state_broadcasts;
    uint64_t rpe_signals_sent;
    uint64_t da_bursts_sent;
    uint64_t da_dips_sent;
    uint64_t plasticity_gates_sent;
    uint32_t active_subscriptions;
    uint32_t peak_subscriptions;
    uint64_t last_broadcast_time_us;
    float avg_message_latency_us;
    float max_message_latency_us;
    uint64_t handler_errors;
    uint64_t routing_errors;
} vta_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

typedef struct vta_bio_async_bridge_struct vta_bio_async_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

int vta_bio_async_default_config(vta_bio_async_config_t* config);
vta_bio_async_bridge_t* vta_bio_async_bridge_create(const vta_bio_async_config_t* config);
void vta_bio_async_bridge_destroy(vta_bio_async_bridge_t* bridge);

int vta_bio_async_connect(vta_bio_async_bridge_t* bridge, nimcp_vta_adapter_t adapter, bio_router_t router);
int vta_bio_async_disconnect(vta_bio_async_bridge_t* bridge);
bool vta_bio_async_is_connected(const vta_bio_async_bridge_t* bridge);

int vta_bio_async_process_inbox(vta_bio_async_bridge_t* bridge, uint32_t max_messages);
int vta_bio_async_update(vta_bio_async_bridge_t* bridge, uint32_t delta_ms);

int vta_bio_async_broadcast_da_state(vta_bio_async_bridge_t* bridge);
int vta_bio_async_broadcast_rpe(vta_bio_async_bridge_t* bridge, float rpe, float predicted, float actual);
int vta_bio_async_broadcast_da_burst(vta_bio_async_bridge_t* bridge, float magnitude, float reward, uint32_t source);
int vta_bio_async_broadcast_da_dip(vta_bio_async_bridge_t* bridge, float magnitude, float expected);
int vta_bio_async_send_plasticity_gate(vta_bio_async_bridge_t* bridge, float gate_strength, float lr_multiplier, uint32_t target);
int vta_bio_async_broadcast_motivation(vta_bio_async_bridge_t* bridge, float motivation, float effort);
int vta_bio_async_broadcast_value_update(vta_bio_async_bridge_t* bridge, float new_value, uint32_t context_id);
int vta_bio_async_broadcast_tonic_shift(vta_bio_async_bridge_t* bridge, float new_tonic);

int vta_bio_async_subscribe_module(vta_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
int vta_bio_async_unsubscribe_module(vta_bio_async_bridge_t* bridge, uint32_t module_id);
int vta_bio_async_update_subscription(vta_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
uint32_t vta_bio_async_get_subscriber_count(const vta_bio_async_bridge_t* bridge, vta_bio_msg_type_t msg_type);

int vta_bio_async_get_stats(const vta_bio_async_bridge_t* bridge, vta_bio_async_stats_t* stats);
int vta_bio_async_reset_stats(vta_bio_async_bridge_t* bridge);
const char* vta_bio_msg_type_name(vta_bio_msg_type_t msg_type);
void vta_bio_async_print_summary(const vta_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_BIO_ASYNC_BRIDGE_H */
