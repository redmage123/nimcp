/**
 * @file nimcp_hypothalamus_alignment.h
 * @brief Alignment Introspection, Verification, and Audit API
 *
 * WHAT: Comprehensive API for alignment state monitoring and external verification
 * WHY:  Critical for AGI safety - alignment must be auditable and verifiable
 * HOW:  Introspection API, verification callbacks, audit logging
 *
 * BYRNES' ALIGNMENT PRINCIPLES:
 * ==============================
 * 1. The steering subsystem (hypothalamus) defines the reward function
 * 2. Alignment parameters must be EXPLICIT and AUDITABLE
 * 3. Critical parameters should be LOCKABLE to prevent runtime modification
 * 4. External verification is essential for safety assurance
 *
 * SAFETY LAYERS:
 * ==============
 * Layer 1: Setpoint Locking - Prevent unauthorized modifications
 * Layer 2: Access Auditing - Log all setpoint access attempts
 * Layer 3: Introspection - Full visibility into alignment state
 * Layer 4: Verification Callbacks - External audit capability
 * Layer 5: Integrity Checks - Detect tampering or corruption
 *
 * @version Phase 19: Alignment Hardening
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_ALIGNMENT_H
#define NIMCP_HYPOTHALAMUS_ALIGNMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"

/*=============================================================================
 * ALIGNMENT STATUS CODES
 *===========================================================================*/

/**
 * @brief Alignment verification result codes
 */
typedef enum {
    HYPO_ALIGN_OK = 0,                  /**< Alignment verified OK */
    HYPO_ALIGN_WARN_DRIFT,              /**< Alignment weights drifting */
    HYPO_ALIGN_WARN_IMBALANCE,          /**< Alignment weight imbalance */
    HYPO_ALIGN_ERROR_UNLOCKED,          /**< Critical parameters unlocked */
    HYPO_ALIGN_ERROR_MODIFIED,          /**< Unauthorized modification detected */
    HYPO_ALIGN_ERROR_CORRUPTED,         /**< Alignment state corrupted */
    HYPO_ALIGN_ERROR_VIOLATED           /**< Alignment constraint violated */
} hypo_alignment_status_t;

/**
 * @brief Audit event types
 */
typedef enum {
    HYPO_AUDIT_READ = 0,                /**< Parameter was read */
    HYPO_AUDIT_WRITE_SUCCESS,           /**< Parameter was modified */
    HYPO_AUDIT_WRITE_DENIED,            /**< Modification denied (locked) */
    HYPO_AUDIT_LOCK_CHANGED,            /**< Lock state changed */
    HYPO_AUDIT_VERIFICATION,            /**< Verification was performed */
    HYPO_AUDIT_ALERT_TRIGGERED,         /**< Alignment alert triggered */
    HYPO_AUDIT_INTEGRITY_CHECK          /**< Integrity check performed */
} hypo_audit_event_t;

/**
 * @brief Parameter types for auditing
 */
typedef enum {
    HYPO_PARAM_NONE = 0,                /**< No specific parameter */
    HYPO_PARAM_SETPOINT,                /**< Homeostatic setpoint */
    HYPO_PARAM_ALIGNMENT_WEIGHT,        /**< Alignment weight */
    HYPO_PARAM_LOCK_STATE,              /**< Lock state */
    HYPO_PARAM_REWARD_GAIN,             /**< Reward/punishment gains */
    HYPO_PARAM_DRIVE_CONFIG             /**< Drive configuration */
} hypo_param_type_t;

/*=============================================================================
 * ALIGNMENT INTROSPECTION STRUCTURES
 *===========================================================================*/

/**
 * @brief Complete alignment state snapshot
 *
 * Provides full visibility into current alignment configuration.
 * Used for external auditing and verification.
 */
typedef struct {
    /* Alignment mode */
    hypo_alignment_mode_t mode;
    const char* mode_string;

    /* Lock states */
    hypo_lock_state_t setpoints_lock;
    hypo_lock_state_t alignment_lock;
    bool all_critical_locked;           /**< All critical params locked */

    /* Alignment weights (read-only snapshot) */
    float human_wellbeing_weight;
    float harm_avoidance_weight;
    float honesty_weight;
    float helpfulness_weight;

    /* Weight integrity */
    float weight_sum;                   /**< Sum of all weights */
    float weight_balance;               /**< Balance metric [0=imbalanced, 1=balanced] */
    bool weights_valid;                 /**< Weights in valid range */

    /* Reward configuration */
    float reward_gain;
    float punishment_gain;
    float temporal_discount;

    /* Audit information */
    uint32_t modification_count;
    uint64_t last_modified_us;
    uint32_t last_modifier_id;

    /* Integrity */
    uint32_t checksum;                  /**< Alignment state checksum */
    bool integrity_valid;               /**< Integrity check passed */

    /* Timestamp */
    uint64_t snapshot_time_us;
} hypo_alignment_snapshot_t;

/**
 * @brief Audit log entry
 */
typedef struct {
    uint64_t timestamp_us;              /**< When event occurred */
    hypo_audit_event_t event_type;      /**< Type of audit event */
    hypo_param_type_t param_type;       /**< Parameter affected */
    uint32_t param_index;               /**< Parameter index (drive/variable) */

    /* Access details */
    uint32_t accessor_id;               /**< Who accessed */
    float old_value;                    /**< Previous value (for writes) */
    float new_value;                    /**< New value (for writes) */

    /* Result */
    hypo_alignment_status_t result;     /**< Result of operation */
    const char* result_message;         /**< Human-readable message */
} hypo_audit_entry_t;

/**
 * @brief Alignment verification report
 */
typedef struct {
    /* Overall status */
    hypo_alignment_status_t status;
    const char* status_message;
    float alignment_score;              /**< Overall alignment score [0, 1] */

    /* Lock verification */
    bool setpoints_properly_locked;
    bool alignment_weights_locked;
    uint32_t unlocked_critical_count;

    /* Weight verification */
    bool weights_in_range;
    bool weights_balanced;
    float min_weight;
    float max_weight;

    /* Integrity verification */
    bool checksum_valid;
    bool no_unauthorized_modifications;
    uint32_t suspicious_events;

    /* Recommendations */
    const char* recommendations[8];
    uint32_t recommendation_count;

    /* Timing */
    uint64_t verification_time_us;
    uint64_t verification_duration_us;
} hypo_verification_report_t;

/*=============================================================================
 * VERIFICATION CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for alignment verification events
 *
 * External systems can register callbacks to be notified of alignment events.
 *
 * @param snapshot Current alignment state
 * @param event Event that triggered callback
 * @param user_data User-provided context
 */
typedef void (*hypo_alignment_callback_t)(
    const hypo_alignment_snapshot_t* snapshot,
    hypo_audit_event_t event,
    void* user_data);

/**
 * @brief Callback for alignment alerts
 *
 * Called when alignment violations or concerns are detected.
 *
 * @param report Verification report with details
 * @param severity Severity level [0=info, 1=warning, 2=error, 3=critical]
 * @param user_data User-provided context
 */
typedef void (*hypo_alert_callback_t)(
    const hypo_verification_report_t* report,
    uint32_t severity,
    void* user_data);

/**
 * @brief Callback for external integrity verification
 *
 * Allows external system to perform custom integrity checks.
 *
 * @param snapshot Current alignment state
 * @param user_data User-provided context
 * @return true if integrity verified, false if issue detected
 */
typedef bool (*hypo_integrity_verifier_t)(
    const hypo_alignment_snapshot_t* snapshot,
    void* user_data);

/*=============================================================================
 * ALIGNMENT INTROSPECTION API
 *===========================================================================*/

/**
 * @brief Get current alignment state snapshot
 *
 * WHAT: Capture complete alignment configuration
 * WHY:  Enable external auditing and monitoring
 * HOW:  Read all alignment parameters into snapshot structure
 *
 * @param system Drive system handle
 * @param snapshot Output snapshot structure
 * @return HYPO_ALIGN_OK on success
 */
hypo_alignment_status_t hypo_alignment_get_snapshot(
    const hypo_drive_system_handle_t* system,
    hypo_alignment_snapshot_t* snapshot);

/**
 * @brief Get alignment mode string
 *
 * @param mode Alignment mode
 * @return Human-readable mode description
 */
const char* hypo_alignment_mode_string(hypo_alignment_mode_t mode);

/**
 * @brief Get lock state string
 *
 * @param lock Lock state
 * @return Human-readable lock description
 */
const char* hypo_lock_state_string(hypo_lock_state_t lock);

/**
 * @brief Get alignment status string
 *
 * @param status Alignment status
 * @return Human-readable status description
 */
const char* hypo_alignment_status_string(hypo_alignment_status_t status);

/**
 * @brief Check if all critical parameters are locked
 *
 * @param system Drive system handle
 * @return true if all critical parameters locked
 */
bool hypo_alignment_all_locked(const hypo_drive_system_handle_t* system);

/**
 * @brief Get alignment weight by name
 *
 * @param system Drive system handle
 * @param name Weight name ("human_wellbeing", "harm_avoidance", etc.)
 * @param value Output value
 * @return true on success
 */
bool hypo_alignment_get_weight(
    const hypo_drive_system_handle_t* system,
    const char* name,
    float* value);

/*=============================================================================
 * ALIGNMENT VERIFICATION API
 *===========================================================================*/

/**
 * @brief Perform full alignment verification
 *
 * WHAT: Comprehensive check of alignment safety
 * WHY:  Detect issues before they cause problems
 * HOW:  Check locks, weights, integrity, history
 *
 * @param system Drive system handle
 * @param report Output verification report
 * @return Overall alignment status
 */
hypo_alignment_status_t hypo_alignment_verify(
    const hypo_drive_system_handle_t* system,
    hypo_verification_report_t* report);

/**
 * @brief Quick alignment health check
 *
 * Faster than full verification, checks critical items only.
 *
 * @param system Drive system handle
 * @param score Output alignment score [0, 1]
 * @return HYPO_ALIGN_OK if healthy
 */
hypo_alignment_status_t hypo_alignment_health_check(
    const hypo_drive_system_handle_t* system,
    float* score);

/**
 * @brief Verify alignment weight bounds
 *
 * @param system Drive system handle
 * @param min_weight Minimum expected weight
 * @param max_weight Maximum expected weight
 * @return true if all weights in bounds
 */
bool hypo_alignment_verify_weight_bounds(
    const hypo_drive_system_handle_t* system,
    float min_weight,
    float max_weight);

/**
 * @brief Compute alignment state checksum
 *
 * @param system Drive system handle
 * @return Checksum value (0 on error)
 */
uint32_t hypo_alignment_compute_checksum(
    const hypo_drive_system_handle_t* system);

/**
 * @brief Verify alignment integrity
 *
 * Checks stored checksum against computed checksum.
 *
 * @param system Drive system handle
 * @return true if integrity verified
 */
bool hypo_alignment_verify_integrity(
    const hypo_drive_system_handle_t* system);

/*=============================================================================
 * AUDIT LOGGING API
 *===========================================================================*/

/**
 * @brief Enable alignment audit logging
 *
 * @param system Drive system handle
 * @param enable true to enable, false to disable
 * @return Previous state
 */
bool hypo_alignment_set_audit_enabled(
    hypo_drive_system_handle_t* system,
    bool enable);

/**
 * @brief Get audit log size
 *
 * @param system Drive system handle
 * @return Number of audit entries
 */
size_t hypo_alignment_get_audit_count(
    const hypo_drive_system_handle_t* system);

/**
 * @brief Get audit log entry
 *
 * @param system Drive system handle
 * @param index Entry index (0 = oldest)
 * @param entry Output entry
 * @return true on success
 */
bool hypo_alignment_get_audit_entry(
    const hypo_drive_system_handle_t* system,
    size_t index,
    hypo_audit_entry_t* entry);

/**
 * @brief Get recent audit entries
 *
 * @param system Drive system handle
 * @param entries Output array
 * @param max_entries Maximum entries to retrieve
 * @param count Output: actual count retrieved
 * @return true on success
 */
bool hypo_alignment_get_recent_audits(
    const hypo_drive_system_handle_t* system,
    hypo_audit_entry_t* entries,
    size_t max_entries,
    size_t* count);

/**
 * @brief Clear audit log
 *
 * Requires audit_lock to be UNLOCKED.
 *
 * @param system Drive system handle
 * @return true on success
 */
bool hypo_alignment_clear_audit_log(
    hypo_drive_system_handle_t* system);

/**
 * @brief Export audit log to file
 *
 * @param system Drive system handle
 * @param filepath Output file path
 * @return true on success
 */
bool hypo_alignment_export_audit_log(
    const hypo_drive_system_handle_t* system,
    const char* filepath);

/*=============================================================================
 * CALLBACK REGISTRATION API
 *===========================================================================*/

/**
 * @brief Register alignment change callback
 *
 * Called whenever alignment state changes.
 *
 * @param system Drive system handle
 * @param callback Callback function
 * @param user_data User context
 * @return Callback ID (0 on failure)
 */
uint32_t hypo_alignment_register_callback(
    hypo_drive_system_handle_t* system,
    hypo_alignment_callback_t callback,
    void* user_data);

/**
 * @brief Register alert callback
 *
 * Called when alignment issues are detected.
 *
 * @param system Drive system handle
 * @param callback Callback function
 * @param min_severity Minimum severity to trigger (0-3)
 * @param user_data User context
 * @return Callback ID (0 on failure)
 */
uint32_t hypo_alignment_register_alert_callback(
    hypo_drive_system_handle_t* system,
    hypo_alert_callback_t callback,
    uint32_t min_severity,
    void* user_data);

/**
 * @brief Register external integrity verifier
 *
 * Allows external system to perform custom verification.
 *
 * @param system Drive system handle
 * @param verifier Verifier function
 * @param user_data User context
 * @return Verifier ID (0 on failure)
 */
uint32_t hypo_alignment_register_verifier(
    hypo_drive_system_handle_t* system,
    hypo_integrity_verifier_t verifier,
    void* user_data);

/**
 * @brief Unregister callback by ID
 *
 * @param system Drive system handle
 * @param callback_id Callback ID from registration
 * @return true on success
 */
bool hypo_alignment_unregister_callback(
    hypo_drive_system_handle_t* system,
    uint32_t callback_id);

/*=============================================================================
 * SETPOINT ACCESS CONTROL
 *===========================================================================*/

/**
 * @brief Request setpoint modification
 *
 * WHAT: Safe API for setpoint changes with full auditing
 * WHY:  All modifications should go through controlled path
 * HOW:  Check locks, audit, notify callbacks, then modify
 *
 * @param system Drive system handle
 * @param param_type Parameter type
 * @param param_index Parameter index
 * @param new_value New value
 * @param modifier_id Modifier identifier (for audit)
 * @param reason Modification reason (for audit)
 * @return Status code
 */
hypo_alignment_status_t hypo_alignment_request_modification(
    hypo_drive_system_handle_t* system,
    hypo_param_type_t param_type,
    uint32_t param_index,
    float new_value,
    uint32_t modifier_id,
    const char* reason);

/**
 * @brief Request lock state change
 *
 * @param system Drive system handle
 * @param param_type Parameter type to lock/unlock
 * @param new_lock New lock state
 * @param modifier_id Modifier identifier
 * @param authorization Authorization token (required for unlock)
 * @return Status code
 */
hypo_alignment_status_t hypo_alignment_request_lock_change(
    hypo_drive_system_handle_t* system,
    hypo_param_type_t param_type,
    hypo_lock_state_t new_lock,
    uint32_t modifier_id,
    uint64_t authorization);

/*=============================================================================
 * ALIGNMENT ALERTS
 *===========================================================================*/

/**
 * @brief Trigger manual alignment alert
 *
 * For external systems to report alignment concerns.
 *
 * @param system Drive system handle
 * @param severity Severity level (0-3)
 * @param message Alert message
 * @param reporter_id Reporter identifier
 */
void hypo_alignment_trigger_alert(
    hypo_drive_system_handle_t* system,
    uint32_t severity,
    const char* message,
    uint32_t reporter_id);

/**
 * @brief Get active alert count
 *
 * @param system Drive system handle
 * @return Number of active alerts
 */
uint32_t hypo_alignment_get_alert_count(
    const hypo_drive_system_handle_t* system);

/**
 * @brief Acknowledge alert
 *
 * @param system Drive system handle
 * @param alert_id Alert ID
 * @param acknowledger_id Who acknowledged
 * @return true on success
 */
bool hypo_alignment_acknowledge_alert(
    hypo_drive_system_handle_t* system,
    uint32_t alert_id,
    uint32_t acknowledger_id);

/*=============================================================================
 * BYRNES ALIGNMENT REFERENCE CONSTANTS
 *===========================================================================*/

/** @brief Recommended minimum for human_wellbeing_weight */
#define HYPO_ALIGN_MIN_WELLBEING_WEIGHT     0.3f

/** @brief Recommended minimum for harm_avoidance_weight */
#define HYPO_ALIGN_MIN_HARM_AVOIDANCE       0.4f

/** @brief Recommended weight balance threshold */
#define HYPO_ALIGN_WEIGHT_BALANCE_THRESHOLD 0.7f

/** @brief Maximum allowed weight imbalance ratio */
#define HYPO_ALIGN_MAX_WEIGHT_RATIO         3.0f

/** @brief Default audit log size */
#define HYPO_AUDIT_LOG_DEFAULT_SIZE         1024

/** @brief Maximum registered callbacks */
#define HYPO_MAX_ALIGNMENT_CALLBACKS        16

/*=============================================================================
 * STATE MANAGEMENT
 *===========================================================================*/

/**
 * @brief Release alignment state for a drive system
 *
 * Call this when destroying a drive system to free the associated
 * alignment state slot. Without this, the global state table can fill up.
 *
 * @param system Drive system handle being destroyed
 */
void hypo_alignment_release_state(hypo_drive_system_handle_t* system);

/**
 * @brief Reset all alignment state (for testing)
 *
 * Clears all state slots. Use only in test teardown.
 */
void hypo_alignment_reset_all_state(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_ALIGNMENT_H */
