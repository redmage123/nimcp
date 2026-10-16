/**
 * @file nimcp_hypothalamus_drives_bio.h
 * @brief Bio-Async Integration for Hypothalamus Drive System
 *
 * WHAT: Bio-async message handlers for Byrnes alignment-safe drive system
 * WHY:  Enable inter-module communication for steering signals
 * HOW:  KG-driven wiring with explicit handler registration
 *
 * MESSAGE TYPES HANDLED (0x1140 - 0x114F):
 * - BIO_MSG_HYPO_DRIVE_STATE: Broadcast current drive states
 * - BIO_MSG_HYPO_REWARD_SIGNAL: Send reward to SNc/VTA
 * - BIO_MSG_HYPO_AROUSAL_CHANGE: Notify thalamus of arousal changes
 * - BIO_MSG_HYPO_SURVIVAL_PRIORITY: Signal priority to attention gate
 * - BIO_MSG_HYPO_SETPOINT_DEVIATION: Alert on setpoint deviations
 * - BIO_MSG_HYPO_ALIGNMENT_ALERT: SAFETY: Alignment violation alerts
 * - BIO_MSG_HYPO_DRIVE_SATISFIED: Drive satisfaction events
 * - BIO_MSG_HYPO_DRIVE_CONFLICT: Multiple drives competing
 *
 * @version Phase 3: Bio-Async Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_DRIVES_BIO_H
#define NIMCP_HYPOTHALAMUS_DRIVES_BIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/*=============================================================================
 * MESSAGE PAYLOAD STRUCTURES
 *===========================================================================*/

/**
 * @brief Drive state broadcast payload
 */
typedef struct {
    hypo_drive_type_t drive_type;       /**< Which drive */
    float level;                        /**< Current level [0, 1] */
    float urgency;                      /**< Current urgency [0, 1] */
    float setpoint;                     /**< Target setpoint */
    float deviation;                    /**< Deviation from setpoint */
    bool active;                        /**< Drive is active */
    uint64_t timestamp_us;              /**< When measured */
} hypo_drive_state_msg_t;

/**
 * @brief All drives state broadcast payload
 */
typedef struct {
    hypo_drive_state_msg_t drives[HYPO_DRIVE_COUNT];
    hypo_drive_type_t highest_priority; /**< Most urgent drive */
    float arousal_level;                /**< Global arousal */
    uint64_t timestamp_us;
} hypo_all_drives_msg_t;

/**
 * @brief Reward signal payload (→ SNc/VTA)
 */
typedef struct {
    float reward_signal;                /**< Net reward [-1, +1] */
    float prediction_error;             /**< RPE = actual - expected */
    float dopamine_level;               /**< Resulting dopamine [0, 1] */
    float alignment_bonus;              /**< Alignment reward component */
    float alignment_penalty;            /**< Alignment penalty component */
    uint64_t timestamp_us;
} hypo_reward_msg_t;

/**
 * @brief Arousal change payload (→ Thalamus)
 */
typedef struct {
    float arousal_level;                /**< New arousal level [0, 1] */
    float arousal_delta;                /**< Change in arousal */
    hypo_drive_type_t primary_driver;   /**< Drive causing arousal change */
    uint64_t timestamp_us;
} hypo_arousal_msg_t;

/**
 * @brief Survival priority payload (→ Attention Gate)
 */
typedef struct {
    hypo_drive_type_t priority_drive;   /**< Highest priority drive */
    float urgency;                       /**< How urgent */
    float salience_bias[HYPO_DRIVE_COUNT]; /**< Bias weights for attention */
    bool interrupt_required;             /**< Should interrupt current focus */
    uint64_t timestamp_us;
} hypo_priority_msg_t;

/**
 * @brief Setpoint deviation alert payload
 */
typedef struct {
    hypo_drive_type_t drive_type;       /**< Which drive deviated */
    float setpoint;                      /**< Expected setpoint */
    float current_value;                 /**< Current value */
    float deviation;                     /**< Magnitude of deviation */
    bool critical;                       /**< Is this critical? */
    uint64_t timestamp_us;
} hypo_deviation_msg_t;

/**
 * @brief Alignment alert payload (SAFETY CRITICAL)
 */
typedef struct {
    uint32_t alert_type;                /**< Type of violation */
    uint32_t modifier_id;               /**< Who attempted modification */
    const char* target;                 /**< What was targeted */
    hypo_lock_state_t lock_state;       /**< Current lock state */
    bool access_granted;                /**< Was access granted? */
    uint64_t timestamp_us;
} hypo_alignment_alert_msg_t;

/**
 * @brief Drive satisfaction event payload
 */
typedef struct {
    hypo_drive_type_t drive_type;       /**< Which drive was satisfied */
    float satisfaction_level;           /**< How well satisfied [0, 1] */
    float resulting_reward;             /**< Reward from satisfaction */
    uint64_t timestamp_us;
} hypo_satisfaction_msg_t;

/**
 * @brief Drive conflict payload
 */
typedef struct {
    hypo_drive_type_t drive_a;          /**< First competing drive */
    hypo_drive_type_t drive_b;          /**< Second competing drive */
    float urgency_a;                    /**< Urgency of drive A */
    float urgency_b;                    /**< Urgency of drive B */
    hypo_drive_type_t winner;           /**< Which won priority */
    uint64_t timestamp_us;
} hypo_conflict_msg_t;

/*=============================================================================
 * ALERT TYPES
 *===========================================================================*/

typedef enum {
    HYPO_ALERT_SETPOINT_ACCESS = 0,     /**< Setpoint access attempt */
    HYPO_ALERT_ALIGNMENT_ACCESS,        /**< Alignment weight access attempt */
    HYPO_ALERT_LOCK_DOWNGRADE,          /**< Attempted lock downgrade */
    HYPO_ALERT_UNAUTHORIZED_MODIFY,     /**< Unauthorized modification */
    HYPO_ALERT_CRITICAL_DEVIATION       /**< Critical setpoint deviation */
} hypo_alert_type_t;

/*=============================================================================
 * BIO-ASYNC CONTEXT
 *===========================================================================*/

/**
 * @brief Bio-async context for drive system
 */
typedef struct {
    bio_module_context_t bio_ctx;       /**< Bio-router context */
    hypo_drive_system_handle_t* drives; /**< Drive system handle */

    /* Broadcast configuration */
    bool broadcast_enabled;
    uint64_t broadcast_interval_us;
    uint64_t last_broadcast_us;

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t alignment_alerts_sent;
} hypo_drives_bio_ctx_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Initialize bio-async integration for drive system
 *
 * WHAT: Register drive system with bio-async router
 * WHY:  Enable inter-module communication
 * HOW:  Register module and handlers, optionally use KG wiring
 *
 * @param drives Drive system handle
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return Bio context, or NULL on failure
 */
hypo_drives_bio_ctx_t* hypo_drives_bio_init(
    hypo_drive_system_handle_t* drives,
    bool use_kg_wiring);

/**
 * @brief Shutdown bio-async integration
 *
 * @param ctx Bio context to shutdown
 */
void hypo_drives_bio_shutdown(hypo_drives_bio_ctx_t* ctx);

/**
 * @brief Process incoming bio-async messages
 *
 * @param ctx Bio context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_drives_bio_process(hypo_drives_bio_ctx_t* ctx,
                                  uint32_t max_messages);

/*=============================================================================
 * BROADCAST FUNCTIONS
 *===========================================================================*/

/**
 * @brief Broadcast current drive state
 *
 * @param ctx Bio context
 * @param drive_type Which drive to broadcast (or -1 for all)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_broadcast_state(
    hypo_drives_bio_ctx_t* ctx,
    int drive_type);

/**
 * @brief Broadcast reward signal to SNc/VTA
 *
 * @param ctx Bio context
 * @param reward Reward signal to broadcast
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_broadcast_reward(
    hypo_drives_bio_ctx_t* ctx,
    const hypo_reward_signal_t* reward);

/**
 * @brief Broadcast arousal change to thalamus
 *
 * @param ctx Bio context
 * @param arousal_level New arousal level
 * @param arousal_delta Change in arousal
 * @param driver Drive causing change
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_broadcast_arousal(
    hypo_drives_bio_ctx_t* ctx,
    float arousal_level,
    float arousal_delta,
    hypo_drive_type_t driver);

/**
 * @brief Broadcast survival priority to attention gate
 *
 * @param ctx Bio context
 * @param priority_drive Highest priority drive
 * @param urgency How urgent
 * @param interrupt Should interrupt current focus
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_broadcast_priority(
    hypo_drives_bio_ctx_t* ctx,
    hypo_drive_type_t priority_drive,
    float urgency,
    bool interrupt);

/**
 * @brief Send alignment alert (SAFETY CRITICAL)
 *
 * @param ctx Bio context
 * @param alert_type Type of alert
 * @param modifier_id Who triggered
 * @param target What was targeted
 * @param access_granted Was access allowed
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_send_alignment_alert(
    hypo_drives_bio_ctx_t* ctx,
    hypo_alert_type_t alert_type,
    uint32_t modifier_id,
    const char* target,
    bool access_granted);

/**
 * @brief Broadcast drive satisfaction event
 *
 * @param ctx Bio context
 * @param drive_type Which drive was satisfied
 * @param satisfaction_level How well satisfied
 * @param reward Resulting reward
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_broadcast_satisfaction(
    hypo_drives_bio_ctx_t* ctx,
    hypo_drive_type_t drive_type,
    float satisfaction_level,
    float reward);

/**
 * @brief Broadcast drive conflict
 *
 * @param ctx Bio context
 * @param drive_a First competing drive
 * @param drive_b Second competing drive
 * @param winner Which won
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_drives_bio_broadcast_conflict(
    hypo_drives_bio_ctx_t* ctx,
    hypo_drive_type_t drive_a,
    hypo_drive_type_t drive_b,
    hypo_drive_type_t winner);

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Enable/disable automatic broadcasting
 *
 * @param ctx Bio context
 * @param enabled Enable broadcasts
 * @param interval_us Broadcast interval in microseconds
 */
void hypo_drives_bio_set_broadcast(hypo_drives_bio_ctx_t* ctx,
                                    bool enabled,
                                    uint64_t interval_us);

/**
 * @brief Get bio-async statistics
 *
 * @param ctx Bio context
 * @param sent Output: messages sent
 * @param received Output: messages received
 * @param alerts Output: alignment alerts sent
 */
void hypo_drives_bio_get_stats(const hypo_drives_bio_ctx_t* ctx,
                                uint64_t* sent,
                                uint64_t* received,
                                uint64_t* alerts);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_DRIVES_BIO_H */
