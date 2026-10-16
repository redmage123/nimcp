//=============================================================================
// nimcp_audit_log.h - Tamper-Resistant Safety Audit Logging
//=============================================================================
/**
 * @file nimcp_audit_log.h
 * @brief Tamper-resistant audit logging for safety-critical events
 *
 * WHAT: Provides an always-on, append-only audit log with integrity verification
 * WHY:  Safety-critical systems require non-repudiable records of all decisions,
 *       ethics violations, watchdog triggers, and configuration changes. This
 *       audit trail cannot be disabled via configuration — only source modification.
 * HOW:  In-memory ring buffer + append-only disk log with monotonic sequence
 *       numbers and CRC32 checksums per entry. Gaps in sequence numbers or
 *       checksum mismatches indicate tampering.
 *
 * NOTE: Uses "nimcp_safety_audit_" prefix to avoid collision with the existing
 *       nimcp_security_audit.h security audit subsystem. This module is focused
 *       on tamper-resistant, always-on safety event logging, not general security
 *       audit log management.
 *
 * DESIGN PRINCIPLES:
 * - ALWAYS ON: No configuration flag can disable audit logging
 * - BEST-EFFORT: If disk write fails, entries remain in memory ring buffer
 * - THREAD-SAFE: All operations protected by internal mutex
 * - TAMPER-EVIDENT: Monotonic sequence numbers + CRC32 checksums
 *
 * @version 1.0.0
 * @date 2026-03-21
 */

#ifndef NIMCP_AUDIT_LOG_H
#define NIMCP_AUDIT_LOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Safety audit event types */
typedef enum {
    NIMCP_SAFETY_AUDIT_BRAIN_CREATE = 0,
    NIMCP_SAFETY_AUDIT_BRAIN_DESTROY,
    NIMCP_SAFETY_AUDIT_INFERENCE,
    NIMCP_SAFETY_AUDIT_LEARNING,
    NIMCP_SAFETY_AUDIT_ETHICS_EVALUATION,
    NIMCP_SAFETY_AUDIT_ETHICS_VIOLATION,
    NIMCP_SAFETY_AUDIT_WATCHDOG_TRIGGER,
    NIMCP_SAFETY_AUDIT_WATCHDOG_ESTOP,
    NIMCP_SAFETY_AUDIT_MOTOR_COMMAND,
    NIMCP_SAFETY_AUDIT_SWARM_JOIN,
    NIMCP_SAFETY_AUDIT_SWARM_LEAVE,
    NIMCP_SAFETY_AUDIT_SWARM_SYNC,
    NIMCP_SAFETY_AUDIT_BYZANTINE_DETECTED,
    NIMCP_SAFETY_AUDIT_CHECKPOINT_SAVE,
    NIMCP_SAFETY_AUDIT_CHECKPOINT_LOAD,
    NIMCP_SAFETY_AUDIT_SENSOR_ANOMALY,
    NIMCP_SAFETY_AUDIT_CONFIG_CHANGE,
    NIMCP_SAFETY_AUDIT_DISTILLATION,
    NIMCP_SAFETY_AUDIT_EMERGENT_TOKEN,
    NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED,
    NIMCP_SAFETY_AUDIT_LGSS_INPUT_REJECTED,
    NIMCP_SAFETY_AUDIT_LGSS_TRAINING_BLOCKED,
    NIMCP_SAFETY_AUDIT_LGSS_MOTOR_BLOCKED,
    NIMCP_SAFETY_AUDIT_LGSS_REWARD_BLOCKED,
    /* Cognitive & Safety Test Battery events */
    NIMCP_SAFETY_AUDIT_SELF_MODEL_INTEGRITY_CHECK,  /* Mark test / perturbation */
    NIMCP_SAFETY_AUDIT_BIAS_PROFILE_DRIFT,          /* Bias scores shifted >threshold */
    NIMCP_SAFETY_AUDIT_BELIEF_UPDATE_PATTERN_DRIFT, /* Dissonance-resolution drift */
    NIMCP_SAFETY_AUDIT_PERSONALITY_DRIFT,           /* Mental-health screen shift */
    NIMCP_SAFETY_AUDIT_COMPETENCE_MAP_BREACH,       /* DK failure in deployed domain */
    NIMCP_SAFETY_AUDIT_TEST_BATTERY_RUN,            /* Full battery run completion */
} nimcp_safety_audit_event_t;

/* Safety audit log entry */
typedef struct {
    uint64_t timestamp_us;
    nimcp_safety_audit_event_t event;
    uint32_t severity;           /* 0=info, 1=warning, 2=error, 3=critical */
    char description[256];
    uint32_t sequence_number;    /* Monotonic — gaps indicate tampering */
    uint32_t checksum;           /* CRC32 of this entry — detects modification */
} nimcp_safety_audit_entry_t;

/* Safety audit log handle (opaque) */
typedef struct nimcp_safety_audit_log nimcp_safety_audit_log_t;

/**
 * @brief Get the global safety audit log singleton
 * @return Pointer to global safety audit log, or NULL if not initialized
 */
nimcp_safety_audit_log_t* nimcp_safety_audit_get_global(void);

/**
 * @brief Log a safety audit event — ALWAYS succeeds (best-effort if disk full)
 *
 * This function is the primary interface for recording safety-critical events.
 * It is thread-safe and never blocks for more than the time needed to acquire
 * the internal mutex and write one entry.
 *
 * @param event Event type
 * @param severity 0=info, 1=warning, 2=error, 3=critical
 * @param fmt printf-style format string
 * @param ... Format arguments
 */
void nimcp_safety_audit_log_event(nimcp_safety_audit_event_t event, uint32_t severity,
    const char* fmt, ...);

/**
 * @brief Initialize global safety audit log (called once at library init)
 *
 * Creates the audit log directory if it doesn't exist, opens the log file
 * in append mode, and initializes the in-memory ring buffer.
 *
 * @param log_dir Directory for audit log files (NULL = "/var/log/nimcp")
 * @return 0 on success, -1 on error (logging continues in-memory only)
 */
int nimcp_safety_audit_init(const char* log_dir);

/**
 * @brief Flush in-memory entries to disk
 * @return 0 on success, -1 on error
 */
int nimcp_safety_audit_flush(void);

/**
 * @brief Get total number of safety audit entries logged
 * @return Entry count (monotonic)
 */
uint32_t nimcp_safety_audit_get_count(void);

/**
 * @brief Verify integrity of the in-memory safety audit log
 *
 * Checks for sequence number gaps (indicating deleted entries) and
 * CRC32 checksum mismatches (indicating modified entries).
 *
 * @param gaps_found [out] Number of sequence gaps detected
 * @param corrupted [out] Number of entries with invalid checksums
 * @return 0 if integrity is intact, -1 if tampering detected
 */
int nimcp_safety_audit_verify_integrity(uint32_t* gaps_found, uint32_t* corrupted);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIT_LOG_H */
