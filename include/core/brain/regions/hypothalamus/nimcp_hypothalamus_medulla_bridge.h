/**
 * @file nimcp_hypothalamus_medulla_bridge.h
 * @brief Hypothalamus <-> Medulla Bridge for Autonomic Control
 *
 * WHAT: Bridge between hypothalamus drives and medulla autonomic functions
 * WHY:  Steering subsystem must control autonomic outputs (arousal, protection)
 * HOW:  Maps drive states to medulla control signals, receives vital feedback
 *
 * BYRNES MODEL CONTEXT:
 * The hypothalamus (steering subsystem) controls the medulla's autonomic
 * functions. This is the interface between motivational drives and the
 * body's regulatory systems.
 *
 * HYPOTHALAMUS → MEDULLA:
 * - Drive urgency → arousal modulation
 * - Stress level → protection level
 * - Circadian phase → circadian sync
 * - Fatigue → sleep pressure
 *
 * MEDULLA → HYPOTHALAMUS:
 * - Arousal state → drive modulation
 * - Protection level → threat awareness
 * - Vital status → homeostatic feedback
 *
 * BIO-ASYNC MESSAGES:
 * - Sends: BIO_MSG_MEDULLA_AROUSAL_SET, BIO_MSG_MEDULLA_PROTECTION_SET
 * - Receives: BIO_MSG_MEDULLA_STATE, BIO_MSG_MEDULLA_VITAL_STATUS
 *
 * @version Phase 11: Brainstem/Medulla Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_MEDULLA_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_MEDULLA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/medulla/nimcp_medulla.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Arousal modulation scale from drives */
#define HYPO_MED_AROUSAL_SCALE       0.4f

/** Protection escalation threshold */
#define HYPO_MED_PROTECTION_THRESHOLD 0.75f

/** Circadian sync update interval (ms) */
#define HYPO_MED_CIRCADIAN_SYNC_MS   1000

/*=============================================================================
 * AUTONOMIC CONTROL TYPES
 *===========================================================================*/

/**
 * @brief Arousal control command to medulla
 */
typedef struct {
    float target_arousal;            /**< Target arousal level [0, 1] */
    float transition_rate;           /**< Rate of transition (0 = instant) */
    bool force_immediate;            /**< Force immediate transition */
    hypo_drive_type_t source_drive;  /**< Drive causing the command */
    uint64_t timestamp_us;
} hypo_medulla_arousal_cmd_t;

/**
 * @brief Protection control command to medulla
 */
typedef struct {
    protection_level_t target_level; /**< Target protection level */
    float stress_intensity;          /**< Stress intensity [0, 1] */
    hypo_drive_type_t threat_source; /**< Drive indicating threat */
    bool is_emergency;               /**< Emergency escalation */
    uint64_t timestamp_us;
} hypo_medulla_protection_cmd_t;

/**
 * @brief Circadian sync command to medulla
 */
typedef struct {
    float circadian_phase;           /**< Current circadian phase [0, 24) hours */
    float target_arousal_baseline;   /**< Circadian-based arousal baseline */
    bool is_sleep_pressure;          /**< Sleep pressure signal */
    float sleep_pressure_level;      /**< Sleep pressure [0, 1] */
    uint64_t timestamp_us;
} hypo_medulla_circadian_cmd_t;

/**
 * @brief Vital status from medulla
 */
typedef struct {
    medulla_state_t state;           /**< Medulla operational state */
    arousal_level_t arousal_level;   /**< Current arousal level enum */
    float arousal_value;             /**< Current arousal [0, 1] */
    protection_level_t protection;   /**< Current protection level */
    circadian_phase_t circadian;     /**< Current circadian phase */
    bool is_emergency;               /**< Emergency state active */
    uint64_t timestamp_us;
} hypo_medulla_status_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Medulla bridge configuration
 */
typedef struct {
    /* Arousal control */
    float arousal_scale;             /**< Drive->arousal scale factor */
    float arousal_baseline;          /**< Baseline arousal level */
    float arousal_max;               /**< Maximum arousal from drives */
    float arousal_transition_rate;   /**< Default transition rate */

    /* Protection control */
    float protection_threshold;      /**< Stress->protection threshold */
    bool enable_emergency_escalation; /**< Allow emergency escalation */

    /* Circadian integration */
    bool enable_circadian_sync;      /**< Sync circadian with medulla */
    uint32_t circadian_sync_interval_ms; /**< Sync interval */

    /* Fatigue/sleep integration */
    bool enable_fatigue_sleep;       /**< Map fatigue to sleep pressure */
    float fatigue_sleep_scale;       /**< Fatigue->sleep pressure scale */

    /* Bio-async */
    bool broadcast_enabled;          /**< Enable bio-async broadcasts */
} hypo_medulla_bridge_config_t;

/**
 * @brief Medulla bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_medulla_bridge_config_t config;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;   /**< Hypothalamus drives */
    medulla_t medulla;                     /**< Medulla module (optional) */

    /* Current medulla state (cached) */
    hypo_medulla_status_t medulla_status;

    /* Computed commands */
    hypo_medulla_arousal_cmd_t last_arousal_cmd;
    hypo_medulla_protection_cmd_t last_protection_cmd;
    hypo_medulla_circadian_cmd_t last_circadian_cmd;

    /* Timing */
    uint64_t last_circadian_sync_us;
    uint64_t last_update_us;

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t arousal_commands_sent;
    uint64_t protection_commands_sent;
    uint64_t circadian_syncs;
    uint64_t status_updates_received;

} hypo_medulla_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default medulla bridge configuration
 *
 * @return Default configuration
 */
hypo_medulla_bridge_config_t hypo_medulla_bridge_default_config(void);

/**
 * @brief Create medulla bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_medulla_bridge_t* hypo_medulla_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_medulla_bridge_config_t* config);

/**
 * @brief Destroy medulla bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_medulla_bridge_destroy(hypo_medulla_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_medulla_bridge_reset(hypo_medulla_bridge_t* bridge);

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update bridge and compute commands
 *
 * WHAT: Compute arousal and protection commands from drive states
 * WHY:  Drive urgencies should control medulla autonomic outputs
 * HOW:  Map drive urgencies to arousal/protection targets
 *
 * @param bridge Bridge context
 * @param dt_ms Time delta in milliseconds
 * @return 0 on success, -1 on error
 */
int hypo_medulla_bridge_update(hypo_medulla_bridge_t* bridge, float dt_ms);

/**
 * @brief Compute arousal command from drives
 *
 * @param bridge Bridge context
 * @return Arousal command
 */
hypo_medulla_arousal_cmd_t hypo_medulla_bridge_compute_arousal(
    hypo_medulla_bridge_t* bridge);

/**
 * @brief Compute protection command from drives
 *
 * @param bridge Bridge context
 * @return Protection command
 */
hypo_medulla_protection_cmd_t hypo_medulla_bridge_compute_protection(
    hypo_medulla_bridge_t* bridge);

/**
 * @brief Compute circadian sync command
 *
 * @param bridge Bridge context
 * @param circadian_phase Current circadian phase [0, 24) hours
 * @return Circadian command
 */
hypo_medulla_circadian_cmd_t hypo_medulla_bridge_compute_circadian(
    hypo_medulla_bridge_t* bridge,
    float circadian_phase);

/**
 * @brief Process medulla status update
 *
 * @param bridge Bridge context
 * @param status Status from medulla
 */
void hypo_medulla_bridge_process_status(
    hypo_medulla_bridge_t* bridge,
    const hypo_medulla_status_t* status);

/**
 * @brief Get current medulla status
 *
 * @param bridge Bridge context
 * @param status Output: current status
 * @return true if status is valid
 */
bool hypo_medulla_bridge_get_status(
    const hypo_medulla_bridge_t* bridge,
    hypo_medulla_status_t* status);

/*=============================================================================
 * MEDULLA CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to medulla module directly
 *
 * @param bridge Bridge context
 * @param medulla Medulla module handle
 * @return true on success
 */
bool hypo_medulla_bridge_connect(
    hypo_medulla_bridge_t* bridge,
    medulla_t medulla);

/**
 * @brief Send arousal command to medulla
 *
 * @param bridge Bridge context
 * @param cmd Arousal command
 * @return 0 on success, -1 on error
 */
int hypo_medulla_bridge_send_arousal(
    hypo_medulla_bridge_t* bridge,
    const hypo_medulla_arousal_cmd_t* cmd);

/**
 * @brief Send protection command to medulla
 *
 * @param bridge Bridge context
 * @param cmd Protection command
 * @return 0 on success, -1 on error
 */
int hypo_medulla_bridge_send_protection(
    hypo_medulla_bridge_t* bridge,
    const hypo_medulla_protection_cmd_t* cmd);

/**
 * @brief Request emergency shutdown via medulla
 *
 * @param bridge Bridge context
 * @param reason Reason for emergency
 * @return 0 on success, -1 on error
 */
int hypo_medulla_bridge_request_emergency(
    hypo_medulla_bridge_t* bridge,
    const char* reason);

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
bool hypo_medulla_bridge_register_bio(
    hypo_medulla_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_medulla_bridge_process_bio(
    hypo_medulla_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast arousal command
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_medulla_bridge_broadcast_arousal(
    hypo_medulla_bridge_t* bridge);

/**
 * @brief Broadcast protection command
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_medulla_bridge_broadcast_protection(
    hypo_medulla_bridge_t* bridge);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param arousal_cmds Output: arousal commands sent
 * @param protection_cmds Output: protection commands sent
 * @param circadian_syncs Output: circadian sync operations
 */
void hypo_medulla_bridge_get_stats(
    const hypo_medulla_bridge_t* bridge,
    uint64_t* arousal_cmds,
    uint64_t* protection_cmds,
    uint64_t* circadian_syncs);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_MEDULLA_BRIDGE_H */
