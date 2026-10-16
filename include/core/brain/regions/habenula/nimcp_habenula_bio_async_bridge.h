/**
 * @file nimcp_habenula_bio_async_bridge.h
 * @brief Habenula Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Central bio-async integration for Habenula that provides comprehensive
 *       message routing for aversive signaling via the bio-router.
 *
 * WHY: The Habenula is the brain's "anti-reward" center and regulates:
 *      - Negative reward prediction error signaling
 *      - Inhibition of VTA/SNc dopamine neurons
 *      - Avoidance learning and punishment detection
 *      - Depression-like states and disappointment responses
 *
 * HOW: Registers Habenula as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming aversive signals.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * HABENULA OUTPUT PATHWAYS:
 * -------------------------
 * 1. Inhibitory to VTA/SNc (dopamine suppression):
 *    - Lateral habenula → RMTg → VTA/SNc inhibition
 *    - Mapped to: BIO_MSG_HABENULA_VTA_INHIBIT
 *
 * 2. Inhibitory to Raphe (serotonin modulation):
 *    - Lateral habenula → Raphe nuclei
 *    - Mapped to: BIO_MSG_HABENULA_RAPHE_INHIBIT
 *
 * 3. Avoidance/aversive signaling:
 *    - Widespread aversive outcome signaling
 *    - Mapped to: BIO_MSG_HABENULA_NEGATIVE_RPE, BIO_MSG_HABENULA_PUNISHMENT_SIGNAL
 *
 * HABENULA INPUT PATHWAYS:
 * ------------------------
 * 1. Limbic (emotional/reward):
 *    - Globus pallidus: Reward expectation violations
 *    - Lateral hypothalamus: Drive-related inputs
 *
 * 2. Prefrontal (cognitive):
 *    - Expectation signals and prediction errors
 *    - Mapped to: Response to reward omissions
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HABENULA_BIO_ASYNC_BRIDGE_H
#define NIMCP_HABENULA_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HAB_BIO_MAX_SUBSCRIPTIONS        64
#define HAB_BIO_MAX_INBOX_SIZE           256
#define HAB_BIO_DEFAULT_BROADCAST_INTERVAL_MS  100
#define HAB_BIO_MESSAGE_TTL_MS           5000
#define HAB_BIO_NEGATIVE_RPE_THRESHOLD   -0.2f

/* ============================================================================
 * Message Types
 * ============================================================================ */

typedef enum {
    HAB_BIO_MSG_STATE = 0,              /**< Complete habenula state broadcast */
    HAB_BIO_MSG_NEGATIVE_RPE,           /**< Negative reward prediction error */
    HAB_BIO_MSG_PUNISHMENT,             /**< Punishment detection signal */
    HAB_BIO_MSG_DISAPPOINTMENT,         /**< Disappointment/omission signal */
    HAB_BIO_MSG_AVOIDANCE_TRIGGER,      /**< Avoidance behavior trigger */
    HAB_BIO_MSG_VTA_INHIBIT,            /**< VTA/SNc inhibition signal */
    HAB_BIO_MSG_RAPHE_INHIBIT,          /**< Raphe inhibition signal */
    HAB_BIO_MSG_AVERSIVE_LEARNING,      /**< Aversive learning signal */
    HAB_BIO_MSG_RELIEF,                 /**< Relief from expected punishment */
    HAB_BIO_MSG_PLASTICITY_GATE,        /**< Habenula plasticity gate */
    HAB_BIO_MSG_REQUEST_STATE,          /**< Request current state */
    HAB_BIO_MSG_MODULATE,               /**< External modulation request */
    HAB_BIO_MSG_COUNT
} hab_bio_msg_type_t;

#define HAB_BIO_SUB_STATE               (1U << HAB_BIO_MSG_STATE)
#define HAB_BIO_SUB_NEGATIVE_RPE        (1U << HAB_BIO_MSG_NEGATIVE_RPE)
#define HAB_BIO_SUB_PUNISHMENT          (1U << HAB_BIO_MSG_PUNISHMENT)
#define HAB_BIO_SUB_DISAPPOINTMENT      (1U << HAB_BIO_MSG_DISAPPOINTMENT)
#define HAB_BIO_SUB_AVOIDANCE_TRIGGER   (1U << HAB_BIO_MSG_AVOIDANCE_TRIGGER)
#define HAB_BIO_SUB_VTA_INHIBIT         (1U << HAB_BIO_MSG_VTA_INHIBIT)
#define HAB_BIO_SUB_RAPHE_INHIBIT       (1U << HAB_BIO_MSG_RAPHE_INHIBIT)
#define HAB_BIO_SUB_AVERSIVE_LEARNING   (1U << HAB_BIO_MSG_AVERSIVE_LEARNING)
#define HAB_BIO_SUB_RELIEF              (1U << HAB_BIO_MSG_RELIEF)
#define HAB_BIO_SUB_PLASTICITY_GATE     (1U << HAB_BIO_MSG_PLASTICITY_GATE)
#define HAB_BIO_SUB_ALL                 (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

typedef struct {
    bio_message_header_t header;
    float activation_level;             /**< Overall habenula activation [0, 1] */
    float negative_rpe;                 /**< Current negative RPE [0, 1] */
    float disappointment_level;         /**< Disappointment intensity [0, 1] */
    float avoidance_drive;              /**< Avoidance motivation [0, 1] */
    float vta_inhibition_output;        /**< VTA inhibition strength [0, 1] */
    float raphe_inhibition_output;      /**< Raphe inhibition strength [0, 1] */
    bool punishment_detected;
    bool aversive_state_active;
    uint64_t timestamp_us;
} hab_bio_state_msg_t;

typedef struct {
    bio_message_header_t header;
    float negative_rpe;                 /**< Negative RPE magnitude [0, 1] */
    float expected_value;               /**< What was expected */
    float actual_value;                 /**< What was received (less) */
    uint32_t context_id;
    uint64_t timestamp_us;
} hab_bio_negative_rpe_msg_t;

typedef struct {
    bio_message_header_t header;
    float punishment_intensity;         /**< Punishment strength [0, 1] */
    uint32_t punishment_type;           /**< Type of punishment */
    bool unexpected;                    /**< Was punishment unexpected */
    uint64_t punishment_onset_us;
    uint64_t timestamp_us;
} hab_bio_punishment_msg_t;

typedef struct {
    bio_message_header_t header;
    float disappointment_level;         /**< Disappointment magnitude [0, 1] */
    float expected_reward;              /**< Expected reward that was omitted */
    float actual_reward;                /**< Actual (zero) reward */
    uint32_t context_id;
    uint64_t timestamp_us;
} hab_bio_disappointment_msg_t;

typedef struct {
    bio_message_header_t header;
    float avoidance_strength;           /**< How strongly to avoid [0, 1] */
    uint32_t stimulus_id;               /**< What to avoid */
    bool urgent;                        /**< Requires immediate action */
    uint64_t timestamp_us;
} hab_bio_avoidance_trigger_msg_t;

typedef struct {
    bio_message_header_t header;
    float inhibition_strength;          /**< How much to inhibit [0, 1] */
    uint32_t target_module;             /**< VTA or Raphe module ID */
    uint64_t inhibition_duration_us;
    uint64_t timestamp_us;
} hab_bio_inhibit_msg_t;

typedef struct {
    bio_message_header_t header;
    float relief_magnitude;             /**< Relief intensity [0, 1] */
    float expected_punishment;          /**< Expected punishment that didn't occur */
    uint32_t context_id;
    uint64_t timestamp_us;
} hab_bio_relief_msg_t;

typedef struct {
    bio_message_header_t header;
    float gate_strength;
    float learning_rate_multiplier;
    bool gate_open;
    bool is_aversive_gate;
    uint32_t target_module;
    uint64_t gate_duration_us;
    uint64_t timestamp_us;
} hab_bio_plasticity_gate_msg_t;

typedef struct {
    bio_message_header_t header;
    float modulation_amount;
    bool is_relative;
    uint32_t requester_module;
    uint32_t request_reason;
    uint64_t timestamp_us;
} hab_bio_modulate_request_msg_t;

/* ============================================================================
 * Configuration and Statistics
 * ============================================================================ */

typedef struct {
    bio_module_id_t module_id;
    uint32_t msg_type_mask;
    bool active;
    uint64_t subscription_time;
    uint64_t messages_sent;
} hab_bio_subscription_t;

typedef struct {
    uint32_t state_broadcast_interval_ms;
    bool enable_auto_broadcast;
    uint32_t max_inbox_process_per_update;
    uint32_t message_ttl_ms;
    float negative_rpe_threshold;
    nimcp_bio_channel_type_t default_channel;
    uint32_t max_subscriptions;
    bool enable_vta_inhibition;
    bool enable_raphe_inhibition;
    bool enable_avoidance_routing;
    bool enable_plasticity_gating;
    bool enable_logging;
} hab_bio_async_config_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t broadcasts_sent;
    uint64_t state_broadcasts;
    uint64_t negative_rpe_signals;
    uint64_t punishment_signals;
    uint64_t disappointment_signals;
    uint64_t vta_inhibitions_sent;
    uint64_t raphe_inhibitions_sent;
    uint64_t relief_signals;
    uint64_t plasticity_gates_sent;
    uint32_t active_subscriptions;
    uint32_t peak_subscriptions;
    uint64_t last_broadcast_time_us;
    uint64_t handler_errors;
    uint64_t routing_errors;
} hab_bio_async_stats_t;

typedef struct hab_bio_async_bridge_struct hab_bio_async_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

int hab_bio_async_default_config(hab_bio_async_config_t* config);
hab_bio_async_bridge_t* hab_bio_async_bridge_create(const hab_bio_async_config_t* config);
void hab_bio_async_bridge_destroy(hab_bio_async_bridge_t* bridge);

int hab_bio_async_connect(hab_bio_async_bridge_t* bridge, nimcp_habenula_adapter_t adapter, bio_router_t router);
int hab_bio_async_disconnect(hab_bio_async_bridge_t* bridge);
bool hab_bio_async_is_connected(const hab_bio_async_bridge_t* bridge);

int hab_bio_async_process_inbox(hab_bio_async_bridge_t* bridge, uint32_t max_messages);
int hab_bio_async_update(hab_bio_async_bridge_t* bridge, uint32_t delta_ms);

int hab_bio_async_broadcast_state(hab_bio_async_bridge_t* bridge);
int hab_bio_async_broadcast_negative_rpe(hab_bio_async_bridge_t* bridge, float rpe, float expected, float actual);
int hab_bio_async_broadcast_punishment(hab_bio_async_bridge_t* bridge, float intensity, uint32_t type, bool unexpected);
int hab_bio_async_broadcast_disappointment(hab_bio_async_bridge_t* bridge, float level, float expected_reward);
int hab_bio_async_send_avoidance_trigger(hab_bio_async_bridge_t* bridge, float strength, uint32_t stimulus_id, bool urgent);
int hab_bio_async_send_vta_inhibition(hab_bio_async_bridge_t* bridge, float strength);
int hab_bio_async_send_raphe_inhibition(hab_bio_async_bridge_t* bridge, float strength);
int hab_bio_async_broadcast_relief(hab_bio_async_bridge_t* bridge, float magnitude, float expected_punishment);
int hab_bio_async_send_plasticity_gate(hab_bio_async_bridge_t* bridge, float gate_strength, float lr_mult, uint32_t target);

int hab_bio_async_subscribe_module(hab_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
int hab_bio_async_unsubscribe_module(hab_bio_async_bridge_t* bridge, uint32_t module_id);
int hab_bio_async_update_subscription(hab_bio_async_bridge_t* bridge, uint32_t module_id, uint32_t msg_types);
uint32_t hab_bio_async_get_subscriber_count(const hab_bio_async_bridge_t* bridge, hab_bio_msg_type_t msg_type);

int hab_bio_async_get_stats(const hab_bio_async_bridge_t* bridge, hab_bio_async_stats_t* stats);
int hab_bio_async_reset_stats(hab_bio_async_bridge_t* bridge);
const char* hab_bio_msg_type_name(hab_bio_msg_type_t msg_type);
void hab_bio_async_print_summary(const hab_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_BIO_ASYNC_BRIDGE_H */
