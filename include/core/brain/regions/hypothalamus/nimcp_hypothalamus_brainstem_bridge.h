/**
 * @file nimcp_hypothalamus_brainstem_bridge.h
 * @brief Hypothalamus <-> Brainstem Bridge for Arousal and Pain/Pleasure
 *
 * WHAT: Bidirectional bridge between hypothalamus drives and brainstem arousal
 * WHY:  Steering subsystem (hypothalamus) must modulate brainstem arousal
 * HOW:  Maps drive urgency to arousal, pain/pleasure to reward, stress to protection
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem (hypothalamus) connects to the brainstem reticular
 * formation to control arousal levels. High survival drive urgency increases
 * arousal (fight/flight). Pain signals feed into the reward system.
 *
 * BIDIRECTIONAL SIGNALS:
 * Hypothalamus → Brainstem:
 * - Drive urgency → arousal boost
 * - Stress level → protection mode
 * - Circadian phase → arousal targets
 *
 * Brainstem → Hypothalamus:
 * - Pain signals → negative reward
 * - Pleasure signals → positive reward
 * - Arousal state → drive modulation
 *
 * BIO-ASYNC MESSAGES:
 * - Sends: BIO_MSG_BRAINSTEM_AROUSAL_REQUEST, BIO_MSG_BRAINSTEM_PROTECTION_REQUEST
 * - Receives: BIO_MSG_BRAINSTEM_PAIN, BIO_MSG_BRAINSTEM_PLEASURE, BIO_MSG_BRAINSTEM_AROUSAL_STATE
 *
 * @version Phase 11: Brainstem/Medulla Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_BRAINSTEM_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_BRAINSTEM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum pain/pleasure sources to track */
#define HYPO_BS_MAX_PAIN_SOURCES       16

/** Arousal boost scale from survival drives */
#define HYPO_BS_SURVIVAL_AROUSAL_SCALE 0.3f

/** Pain to negative reward conversion factor */
#define HYPO_BS_PAIN_REWARD_FACTOR     -2.0f

/** Pleasure to positive reward conversion factor */
#define HYPO_BS_PLEASURE_REWARD_FACTOR 1.5f

/*=============================================================================
 * PAIN/PLEASURE TYPES
 *===========================================================================*/

/**
 * @brief Pain signal source types
 *
 * BIOLOGICAL: Different types of nociceptive/aversive signals
 */
typedef enum {
    HYPO_PAIN_NOCICEPTIVE = 0,      /**< Physical damage signal */
    HYPO_PAIN_THERMAL,               /**< Temperature extreme */
    HYPO_PAIN_CHEMICAL,              /**< Chemical irritant */
    HYPO_PAIN_VISCERAL,              /**< Internal organ distress */
    HYPO_PAIN_SOCIAL,                /**< Social pain (rejection) */
    HYPO_PAIN_PREDICTION_ERROR,      /**< Large prediction error (aversive) */
    HYPO_PAIN_COUNT
} hypo_pain_type_t;

/**
 * @brief Pleasure signal source types
 *
 * BIOLOGICAL: Different types of hedonic/rewarding signals
 */
typedef enum {
    HYPO_PLEASURE_CONSUMATORY = 0,  /**< Food/water consumption */
    HYPO_PLEASURE_SOCIAL,            /**< Social connection */
    HYPO_PLEASURE_THERMAL_COMFORT,   /**< Temperature comfort */
    HYPO_PLEASURE_SAFETY,            /**< Threat eliminated */
    HYPO_PLEASURE_MASTERY,           /**< Skill acquisition */
    HYPO_PLEASURE_CURIOSITY,         /**< Information gain */
    HYPO_PLEASURE_COUNT
} hypo_pleasure_type_t;

/*=============================================================================
 * SIGNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Pain signal from brainstem
 */
typedef struct {
    hypo_pain_type_t type;          /**< Pain type */
    float intensity;                 /**< Pain intensity [0, 1] */
    float duration_ms;               /**< Expected duration */
    uint32_t source_id;              /**< Source identifier */
    uint64_t timestamp_us;
} hypo_pain_signal_t;

/**
 * @brief Pleasure signal from brainstem
 */
typedef struct {
    hypo_pleasure_type_t type;      /**< Pleasure type */
    float intensity;                 /**< Pleasure intensity [0, 1] */
    float duration_ms;               /**< Expected duration */
    uint32_t source_id;              /**< Source identifier */
    uint64_t timestamp_us;
} hypo_pleasure_signal_t;

/**
 * @brief Arousal request to brainstem
 */
typedef struct {
    float target_arousal;            /**< Requested arousal level [0, 1] */
    float urgency;                   /**< Request urgency [0, 1] */
    hypo_drive_type_t source_drive;  /**< Drive causing the request */
    bool is_emergency;               /**< Emergency arousal request */
    uint64_t timestamp_us;
} hypo_arousal_request_t;

/**
 * @brief Protection request to brainstem
 */
typedef struct {
    uint32_t protection_level;       /**< Requested protection level */
    float stress_level;              /**< Current stress level [0, 1] */
    hypo_drive_type_t threat_source; /**< Drive indicating threat */
    bool is_critical;                /**< Critical threat */
    uint64_t timestamp_us;
} hypo_protection_request_t;

/**
 * @brief Arousal state from brainstem (feedback)
 */
typedef struct {
    float current_arousal;           /**< Current arousal level [0, 1] */
    uint32_t arousal_level_enum;     /**< Arousal level category */
    uint32_t protection_level;       /**< Current protection level */
    bool in_emergency;               /**< Emergency state active */
    uint64_t timestamp_us;
} hypo_arousal_state_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Brainstem bridge configuration
 */
typedef struct {
    /* Arousal modulation */
    float survival_arousal_scale;    /**< Scale for survival->arousal */
    float stress_arousal_scale;      /**< Scale for stress->arousal */
    float circadian_arousal_bias;    /**< Circadian contribution */

    /* Pain/pleasure processing */
    float pain_reward_factor;        /**< Pain->reward conversion */
    float pleasure_reward_factor;    /**< Pleasure->reward conversion */
    float pain_drive_boost;          /**< Pain boosts SAFETY drive */

    /* Thresholds */
    float emergency_threshold;       /**< Urgency for emergency arousal */
    float protection_threshold;      /**< Stress for protection request */

    /* Integration */
    bool enable_pain_reward;         /**< Convert pain to negative reward */
    bool enable_pleasure_reward;     /**< Convert pleasure to positive reward */
    bool enable_arousal_feedback;    /**< Use arousal state for drive modulation */
    bool broadcast_enabled;          /**< Enable bio-async broadcasts */
} hypo_brainstem_bridge_config_t;

/**
 * @brief Brainstem bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_brainstem_bridge_config_t config;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;   /**< Hypothalamus drives */
    void* brainstem;                       /**< Brainstem module (optional) */

    /* Pain/pleasure tracking */
    hypo_pain_signal_t active_pain[HYPO_BS_MAX_PAIN_SOURCES];
    uint32_t pain_count;
    hypo_pleasure_signal_t active_pleasure[HYPO_BS_MAX_PAIN_SOURCES];
    uint32_t pleasure_count;

    /* Current state */
    float current_arousal;
    uint32_t current_protection;
    float total_pain;                /**< Aggregate pain level */
    float total_pleasure;            /**< Aggregate pleasure level */

    /* Computed outputs */
    float pain_reward_contribution;  /**< Pain contribution to reward */
    float pleasure_reward_contribution; /**< Pleasure contribution to reward */
    float arousal_request;           /**< Computed arousal request */

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t pain_signals_received;
    uint64_t pleasure_signals_received;
    uint64_t arousal_requests_sent;
    uint64_t protection_requests_sent;

} hypo_brainstem_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default brainstem bridge configuration
 *
 * @return Default configuration
 */
hypo_brainstem_bridge_config_t hypo_brainstem_bridge_default_config(void);

/**
 * @brief Create brainstem bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_brainstem_bridge_t* hypo_brainstem_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_brainstem_bridge_config_t* config);

/**
 * @brief Destroy brainstem bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_brainstem_bridge_destroy(hypo_brainstem_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_brainstem_bridge_reset(hypo_brainstem_bridge_t* bridge);

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update bridge state from drives
 *
 * WHAT: Compute arousal requests from drive urgencies
 * WHY:  Survival drives should increase arousal (fight/flight)
 * HOW:  Map drive urgencies to arousal target, check for emergency
 *
 * @param bridge Bridge context
 * @return Computed arousal request
 */
hypo_arousal_request_t hypo_brainstem_bridge_update(hypo_brainstem_bridge_t* bridge);

/**
 * @brief Process incoming pain signal
 *
 * WHAT: Handle pain signal from brainstem
 * WHY:  Pain provides negative reward signal
 * HOW:  Store signal, update aggregate, compute reward contribution
 *
 * @param bridge Bridge context
 * @param signal Pain signal
 * @return Reward contribution from this pain signal (negative)
 */
float hypo_brainstem_bridge_process_pain(
    hypo_brainstem_bridge_t* bridge,
    const hypo_pain_signal_t* signal);

/**
 * @brief Process incoming pleasure signal
 *
 * WHAT: Handle pleasure signal from brainstem
 * WHY:  Pleasure provides positive reward signal
 * HOW:  Store signal, update aggregate, compute reward contribution
 *
 * @param bridge Bridge context
 * @param signal Pleasure signal
 * @return Reward contribution from this pleasure signal (positive)
 */
float hypo_brainstem_bridge_process_pleasure(
    hypo_brainstem_bridge_t* bridge,
    const hypo_pleasure_signal_t* signal);

/**
 * @brief Process arousal state feedback from brainstem
 *
 * WHAT: Handle arousal state update from brainstem
 * WHY:  Arousal state affects drive processing
 * HOW:  Store state, optionally modulate drives based on arousal
 *
 * @param bridge Bridge context
 * @param state Arousal state from brainstem
 */
void hypo_brainstem_bridge_process_arousal_state(
    hypo_brainstem_bridge_t* bridge,
    const hypo_arousal_state_t* state);

/**
 * @brief Get total reward contribution from pain/pleasure
 *
 * @param bridge Bridge context
 * @return Combined pain (negative) and pleasure (positive) reward
 */
float hypo_brainstem_bridge_get_reward_contribution(
    const hypo_brainstem_bridge_t* bridge);

/**
 * @brief Check if emergency arousal is needed
 *
 * @param bridge Bridge context
 * @return true if survival drives require emergency arousal
 */
bool hypo_brainstem_bridge_needs_emergency(
    const hypo_brainstem_bridge_t* bridge);

/*=============================================================================
 * BRAINSTEM CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to brainstem module
 *
 * @param bridge Bridge context
 * @param brainstem Brainstem module handle
 * @return true on success
 */
bool hypo_brainstem_bridge_connect(
    hypo_brainstem_bridge_t* bridge,
    void* brainstem);

/**
 * @brief Send arousal request to brainstem
 *
 * @param bridge Bridge context
 * @param request Arousal request to send
 * @return 0 on success, -1 on error
 */
int hypo_brainstem_bridge_send_arousal_request(
    hypo_brainstem_bridge_t* bridge,
    const hypo_arousal_request_t* request);

/**
 * @brief Send protection request to brainstem
 *
 * @param bridge Bridge context
 * @param request Protection request to send
 * @return 0 on success, -1 on error
 */
int hypo_brainstem_bridge_send_protection_request(
    hypo_brainstem_bridge_t* bridge,
    const hypo_protection_request_t* request);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return true on success
 */
bool hypo_brainstem_bridge_register_bio(
    hypo_brainstem_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_brainstem_bridge_process_bio(
    hypo_brainstem_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast arousal request
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_brainstem_bridge_broadcast_arousal(
    hypo_brainstem_bridge_t* bridge);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param pain_signals Output: total pain signals received
 * @param pleasure_signals Output: total pleasure signals received
 * @param arousal_requests Output: total arousal requests sent
 */
void hypo_brainstem_bridge_get_stats(
    const hypo_brainstem_bridge_t* bridge,
    uint64_t* pain_signals,
    uint64_t* pleasure_signals,
    uint64_t* arousal_requests);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get string representation of pain type
 */
const char* hypo_pain_type_string(hypo_pain_type_t type);

/**
 * @brief Get string representation of pleasure type
 */
const char* hypo_pleasure_type_string(hypo_pleasure_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_BRAINSTEM_BRIDGE_H */
