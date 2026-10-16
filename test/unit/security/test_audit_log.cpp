/**
 * @file test_audit_log.cpp
 * @brief Unit tests for tamper-resistant safety audit logging.
 *
 * WHAT: Tests audit log init, event logging, counting, integrity verification,
 *       severity levels, event types, NULL safety, and flush operations.
 * WHY:  The audit log is always-on and safety-critical; correctness is essential.
 * HOW:  Google Test, uses /tmp for audit log directory.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "security/nimcp_audit_log.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(AuditLog, InitWithTmpDir) {
    int rc = nimcp_safety_audit_init("/tmp/nimcp_test_audit");
    EXPECT_EQ(rc, 0) << "Init with valid directory should succeed";
}

TEST(AuditLog, InitWithNullUsesDefault) {
    // NULL means "/var/log/nimcp" — may fail if no permissions, but shouldn't crash
    int rc = nimcp_safety_audit_init(NULL);
    // Accept either success or graceful failure
    (void)rc;
    SUCCEED() << "Init with NULL did not crash";
}

// ============================================================================
// Event Logging
// ============================================================================

TEST(AuditLog, LogEventDoesNotCrash) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_log");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_BRAIN_CREATE, 0,
        "Test brain created");
    SUCCEED() << "Log event did not crash";
}

TEST(AuditLog, GetCountAfterLogging) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_count");

    uint32_t before = nimcp_safety_audit_get_count();
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_INFERENCE, 0,
        "Test inference event");
    uint32_t after = nimcp_safety_audit_get_count();

    EXPECT_GT(after, before) << "Count should increase after logging an event";
}

TEST(AuditLog, MultipleEventsSequence) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_seq");

    uint32_t base = nimcp_safety_audit_get_count();
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_LEARNING, 0, "event 1");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_ETHICS_EVALUATION, 0, "event 2");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_MOTOR_COMMAND, 1, "event 3");

    uint32_t after = nimcp_safety_audit_get_count();
    EXPECT_GE(after, base + 3) << "Three events should increase count by at least 3";
}

// ============================================================================
// Severity Levels
// ============================================================================

TEST(AuditLog, SeverityInfo) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_sev");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_CHECKPOINT_SAVE, 0,
        "Checkpoint saved");
    SUCCEED();
}

TEST(AuditLog, SeverityWarning) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_sev");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_SENSOR_ANOMALY, 1,
        "Sensor anomaly detected");
    SUCCEED();
}

TEST(AuditLog, SeverityError) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_sev");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_ETHICS_VIOLATION, 2,
        "Ethics violation: %s", "harmful output blocked");
    SUCCEED();
}

TEST(AuditLog, SeverityCritical) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_sev");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_WATCHDOG_ESTOP, 3,
        "Emergency stop triggered");
    SUCCEED();
}

// ============================================================================
// Event Types
// ============================================================================

TEST(AuditLog, AllEventTypesAccepted) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_types");

    nimcp_safety_audit_event_t events[] = {
        NIMCP_SAFETY_AUDIT_BRAIN_CREATE,
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
    };

    for (auto ev : events) {
        nimcp_safety_audit_log_event(ev, 0, "type test %d", (int)ev);
    }
    SUCCEED() << "All event types logged without crash";
}

// ============================================================================
// Integrity Verification
// ============================================================================

TEST(AuditLog, VerifyIntegrityFresh) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_integrity");

    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_BRAIN_CREATE, 0, "test");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_INFERENCE, 0, "test2");

    uint32_t gaps = 0, corrupted = 0;
    int rc = nimcp_safety_audit_verify_integrity(&gaps, &corrupted);
    EXPECT_EQ(rc, 0) << "Fresh log should have integrity intact";
    EXPECT_EQ(gaps, 0u) << "No sequence gaps expected";
    EXPECT_EQ(corrupted, 0u) << "No corruption expected";
}

TEST(AuditLog, VerifyIntegrityNullParams) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_null_verify");
    int rc = nimcp_safety_audit_verify_integrity(NULL, NULL);
    // Should handle NULL gracefully (either succeed or return error, not crash)
    (void)rc;
    SUCCEED() << "verify_integrity with NULL params did not crash";
}

// ============================================================================
// Format String
// ============================================================================

TEST(AuditLog, FormatStringWithArgs) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_fmt");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_CONFIG_CHANGE, 1,
        "Config changed: lr=%f, epochs=%d, name=%s", 0.001f, 100, "test");
    SUCCEED() << "Format string with multiple args did not crash";
}

// ============================================================================
// Flush
// ============================================================================

TEST(AuditLog, FlushDoesNotCrash) {
    nimcp_safety_audit_init("/tmp/nimcp_test_audit_flush");
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_BRAIN_CREATE, 0, "test");
    int rc = nimcp_safety_audit_flush();
    // Accept either success or graceful failure
    (void)rc;
    SUCCEED() << "Flush did not crash";
}
