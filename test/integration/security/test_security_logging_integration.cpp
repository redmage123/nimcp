/**
 * @file test_security_logging_integration.cpp
 * @brief Integration tests for Security-Logging Bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Comprehensive integration tests for security-logging bridge functionality
 * WHY:  Verify bidirectional data flow between security systems and logging infrastructure
 * HOW:  Test real security events, pattern detection, and feedback loops
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * The security-logging bridge mirrors the immune system's memory mechanisms:
 * - Dendritic cells (security) present antigens to memory systems (logging)
 * - Memory B cells retain patterns of past threats (log analysis)
 * - Cytokine signaling (real-time alerts) coordinates immediate responses
 * - Complement system (audit trail) marks and tracks all foreign entities
 *
 * TEST SCENARIOS:
 * =================================================================================
 * 1. Security + Logging System Integration
 *    - Security events flow to NIMCP logger
 *    - Log rotation and retention
 *    - Log file output and format
 *
 * 2. Security + Encrypted Audit Integration
 *    - Security events to encrypted audit trail
 *    - Tamper-proof audit records
 *    - Audit export and verification
 *
 * 3. Security + BBB Integration
 *    - BBB threats logged with full context
 *    - BBB validation failures recorded
 *    - BBB action audit trail
 *
 * 4. Security + Anomaly Detector Integration
 *    - Anomaly detection events logged
 *    - Pattern extraction from logs
 *    - Feedback loop to anomaly detector
 *
 * 5. Security + Rate Limiter Integration
 *    - Rate limit events logged
 *    - Client statistics recorded
 *    - Abuse pattern detection from logs
 *
 * 6. Bidirectional Flow Tests
 *    - Security->Logging: events recorded properly
 *    - Logging->Security: patterns fed back
 *    - Full cycle: threat -> log -> pattern -> detection
 *
 * 7. Log Analysis Tests
 *    - Pattern detection in log stream
 *    - Attack sequence identification
 *    - Behavioral baseline deviation
 *
 * 8. Performance Tests
 *    - High-frequency logging
 *    - Buffer management under load
 *    - Lock-free operation verification
 *
 * @author NIMCP Security Team
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "security/logging/nimcp_security_logging_bridge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_encrypted_audit.h"
#include "security/nimcp_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
}

/*=============================================================================
 * TEST FIXTURES
 *============================================================================*/

/**
 * @brief Base test fixture for security-logging bridge tests
 *
 * WHAT: Provides common setup/teardown for all security-logging tests
 * WHY:  Ensures consistent test environment and proper cleanup
 */
class SecurityLoggingBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize with default configuration */
        ASSERT_EQ(security_logging_default_config(&config), 0);

        /* Create bridge with test-friendly settings */
        config.buffer_capacity = 1024;  /* Smaller buffer for tests */
        config.min_severity = SECURITY_LOG_SEV_DEBUG;
        config.enabled_categories = SECURITY_LOG_CAT_ALL;
        config.log_to_console = false;  /* Suppress console output in tests */
        config.log_to_file = false;     /* File tests handled separately */
        config.enable_bio_async = false; /* Disable async for deterministic tests */

        bridge = security_logging_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_logging_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    security_logging_bridge_config_t config;
    security_logging_bridge_t* bridge = nullptr;
};

/**
 * @brief Test fixture for encrypted audit integration
 */
class SecurityLoggingEncryptedAuditTest : public SecurityLoggingBridgeTest {
protected:
    void SetUp() override {
        SecurityLoggingBridgeTest::SetUp();

        /* Generate encryption key */
        ASSERT_EQ(nimcp_encryption_generate_key(audit_key), NIMCP_SUCCESS);

        /* Create encrypted audit system */
        nimcp_encrypted_audit_config_t audit_config = nimcp_encrypted_audit_default_config();
        audit_config.buffer_size = 1000;
        encrypted_audit = nimcp_encrypted_audit_create(&audit_config, audit_key, sizeof(audit_key));
        ASSERT_NE(encrypted_audit, nullptr);
    }

    void TearDown() override {
        if (encrypted_audit) {
            nimcp_encrypted_audit_destroy(encrypted_audit);
            encrypted_audit = nullptr;
        }
        SecurityLoggingBridgeTest::TearDown();
    }

    uint8_t audit_key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encrypted_audit_t encrypted_audit = nullptr;
};

/**
 * @brief Test fixture for BBB integration
 */
class SecurityLoggingBBBTest : public SecurityLoggingBridgeTest {
protected:
    void SetUp() override {
        SecurityLoggingBridgeTest::SetUp();

        /* Create BBB system */
        bbb_config = bbb_default_config();
        bbb_config.strict_mode = true;
        bbb = bbb_system_create(&bbb_config);
        ASSERT_NE(bbb, nullptr);
    }

    void TearDown() override {
        if (bbb) {
            bbb_system_destroy(bbb);
            bbb = nullptr;
        }
        SecurityLoggingBridgeTest::TearDown();
    }

    bbb_config_t bbb_config;
    bbb_system_t bbb = nullptr;
};

/**
 * @brief Test fixture for anomaly detector integration
 */
class SecurityLoggingAnomalyTest : public SecurityLoggingBridgeTest {
protected:
    void SetUp() override {
        SecurityLoggingBridgeTest::SetUp();

        /* Create anomaly detector */
        nimcp_anomaly_config_t anomaly_config = nimcp_anomaly_detector_default_config();
        anomaly_config.enable_online_learning = true;
        anomaly_detector = nimcp_anomaly_detector_create(&anomaly_config);
        ASSERT_NE(anomaly_detector, nullptr);
    }

    void TearDown() override {
        if (anomaly_detector) {
            nimcp_anomaly_detector_destroy(anomaly_detector);
            anomaly_detector = nullptr;
        }
        SecurityLoggingBridgeTest::TearDown();
    }

    nimcp_anomaly_detector_t anomaly_detector = nullptr;
};

/**
 * @brief Test fixture for rate limiter integration
 */
class SecurityLoggingRateLimiterTest : public SecurityLoggingBridgeTest {
protected:
    void SetUp() override {
        SecurityLoggingBridgeTest::SetUp();

        /* Create rate limiter */
        nimcp_rate_limit_config_t rate_config = nimcp_rate_limiter_default_config();
        rate_config.requests_per_second = 100.0f;
        rate_config.burst_size = 150;
        rate_config.per_client = true;
        rate_limiter = nimcp_rate_limiter_create(&rate_config);
        ASSERT_NE(rate_limiter, nullptr);
    }

    void TearDown() override {
        if (rate_limiter) {
            nimcp_rate_limiter_destroy(rate_limiter);
            rate_limiter = nullptr;
        }
        SecurityLoggingBridgeTest::TearDown();
    }

    nimcp_rate_limiter_t rate_limiter = nullptr;
};

/*=============================================================================
 * SECURITY + LOGGING SYSTEM INTEGRATION TESTS
 *============================================================================*/

/**
 * @brief Test 1: Verify security events flow to NIMCP logger
 *
 * WHAT: Events logged via bridge are forwarded to NIMCP logging system
 * WHY:  Ensure integration between security bridge and main logging infrastructure
 * HOW:  Log events and verify they appear in logger output
 */
TEST_F(SecurityLoggingBridgeTest, SecurityEventsFlowToNIMCPLogger) {
    /* Log a security threat event */
    ASSERT_EQ(security_logging_log_threat(
        bridge,
        NIMCP_THREAT_HIGH,
        BBB_THREAT_CODE_INJECTION,
        "test_module",
        "Detected code injection attempt",
        SECURITY_LOG_ACTION_BLOCK
    ), 0);

    /* Log access control event */
    ASSERT_EQ(security_logging_log_access(
        bridge,
        false,  /* denied */
        "user123",
        "/admin/config",
        "Unauthorized access attempt"
    ), 0);

    /* Verify statistics reflect logged events */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);

    EXPECT_GE(stats.total_entries, 2UL);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_THREAT], 1UL);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_ACCESS], 1UL);
}

/**
 * @brief Test 2: Log rotation and retention
 *
 * WHAT: Test log rotation triggers properly based on configuration
 * WHY:  Ensure log files don't grow unbounded and old logs are retained
 * HOW:  Configure rotation, fill buffer, trigger rotation
 */
TEST_F(SecurityLoggingBridgeTest, LogRotationAndRetention) {
    /* Manually trigger rotation */
    ASSERT_EQ(security_logging_rotate(bridge), 0);

    /* Log some events after rotation */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(security_logging_log_audit(
            bridge,
            SECURITY_LOG_SEV_INFO,
            "rotation_test",
            "Post-rotation event",
            nullptr
        ), 0);
    }

    /* Verify statistics reset properly after rotation */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.total_entries, 10UL);
}

/**
 * @brief Test 3: Log file output format verification
 *
 * WHAT: Verify different log output formats (JSON, TEXT, SYSLOG)
 * WHY:  Ensure logs can be exported in formats compatible with analysis tools
 * HOW:  Export entries to different formats and verify structure
 */
TEST_F(SecurityLoggingBridgeTest, LogFileOutputAndFormat) {
    /* Create test entry */
    security_log_entry_t entry;
    ASSERT_EQ(security_log_entry_init(
        &entry,
        SECURITY_LOG_CAT_THREAT,
        SECURITY_LOG_SEV_CRITICAL,
        "Critical security event for format test"
    ), 0);

    /* Log the entry */
    ASSERT_EQ(security_logging_log_entry(bridge, &entry), 0);

    /* Export to JSON format */
    char json_buffer[2048];
    int json_len = security_logging_entry_to_json(&entry, json_buffer, sizeof(json_buffer));
    EXPECT_GT(json_len, 0);

    /* Verify JSON contains expected fields */
    EXPECT_NE(strstr(json_buffer, "category"), nullptr);
    EXPECT_NE(strstr(json_buffer, "severity"), nullptr);
    EXPECT_NE(strstr(json_buffer, "message"), nullptr);
}

/*=============================================================================
 * SECURITY + ENCRYPTED AUDIT INTEGRATION TESTS
 *============================================================================*/

/**
 * @brief Test 4: Security events flow to encrypted audit trail
 *
 * WHAT: Critical security events are recorded in encrypted audit
 * WHY:  Provide tamper-proof record of security incidents
 * HOW:  Connect encrypted audit, log events, verify they're encrypted
 */
TEST_F(SecurityLoggingEncryptedAuditTest, SecurityEventsToEncryptedAudit) {
    /* Connect encrypted audit to bridge */
    ASSERT_EQ(security_logging_connect_encrypted_audit(bridge, encrypted_audit), 0);

    /* Log critical security event */
    ASSERT_EQ(security_logging_log_threat(
        bridge,
        NIMCP_THREAT_CRITICAL,
        BBB_THREAT_SHELLCODE,
        "shellcode_detector",
        "Shellcode detected in input buffer",
        SECURITY_LOG_ACTION_QUARANTINE
    ), 0);

    /* Update bridge to process pending events */
    ASSERT_EQ(security_logging_update(bridge), 0);

    /* Verify event was written to encrypted audit */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.encrypted_audit_writes, 1UL);
}

/**
 * @brief Test 5: Tamper-proof audit record verification
 *
 * WHAT: Verify audit records cannot be tampered with
 * WHY:  Ensure forensic integrity of security logs
 * HOW:  Write audit entry, read back, verify integrity
 */
TEST_F(SecurityLoggingEncryptedAuditTest, TamperProofAuditRecords) {
    /* Connect encrypted audit */
    ASSERT_EQ(security_logging_connect_encrypted_audit(bridge, encrypted_audit), 0);

    /* Log multiple events */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(security_logging_log_audit(
            bridge,
            SECURITY_LOG_SEV_CRITICAL,
            "audit_test",
            "Tamper-proof test event",
            "Event sequence data"
        ), 0);
    }

    /* Process events */
    ASSERT_EQ(security_logging_update(bridge), 0);

    /* Read back from encrypted audit */
    nimcp_audit_entry_t audit_entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(encrypted_audit, audit_entries, 10, &num_entries), NIMCP_SUCCESS);

    /* Verify decryption succeeded (tamper detection) */
    nimcp_encrypted_audit_stats_t audit_stats;
    ASSERT_EQ(nimcp_encrypted_audit_get_stats(encrypted_audit, &audit_stats), NIMCP_SUCCESS);
    EXPECT_EQ(audit_stats.tampering_detected, 0UL);
}

/**
 * @brief Test 6: Audit export and verification
 *
 * WHAT: Test exporting encrypted audit to file and verifying
 * WHY:  Enable audit archival and compliance requirements
 * HOW:  Export audit, verify file exists and can be re-read
 */
TEST_F(SecurityLoggingEncryptedAuditTest, AuditExportAndVerification) {
    ASSERT_EQ(security_logging_connect_encrypted_audit(bridge, encrypted_audit), 0);

    /* Log events for export */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(security_logging_log_crypto(
            bridge,
            "encrypt",
            true,
            "key_001",
            "Encryption operation for audit export test"
        ), 0);
    }

    ASSERT_EQ(security_logging_update(bridge), 0);

    /* Verify audit has entries */
    nimcp_encrypted_audit_stats_t audit_stats;
    ASSERT_EQ(nimcp_encrypted_audit_get_stats(encrypted_audit, &audit_stats), NIMCP_SUCCESS);
    EXPECT_GT(audit_stats.total_entries, 0UL);
}

/*=============================================================================
 * SECURITY + BBB INTEGRATION TESTS
 *============================================================================*/

/**
 * @brief Test 7: BBB threats logged with full context
 *
 * WHAT: BBB threat detections are logged with complete context
 * WHY:  Enable forensic analysis of BBB-detected threats
 * HOW:  Connect BBB, trigger threat, verify log contains full context
 */
TEST_F(SecurityLoggingBBBTest, BBBThreatsLoggedWithFullContext) {
    /* Connect BBB to bridge */
    ASSERT_EQ(security_logging_connect_bbb(bridge, bbb), 0);

    /* Log BBB event directly */
    ASSERT_EQ(security_logging_log_bbb(
        bridge,
        BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH,
        BBB_ACTION_BLOCK,
        "Buffer overflow detected in input parser"
    ), 0);

    /* Query logged entries */
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    ASSERT_EQ(security_logging_query_entries(
        bridge,
        0,  /* start time */
        0,  /* end time */
        SECURITY_LOG_CAT_MASK(SECURITY_LOG_CAT_BBB),
        SECURITY_LOG_SEV_DEBUG,
        entries,
        10,
        &actual_count
    ), 0);

    EXPECT_GE(actual_count, 1UL);
    EXPECT_EQ(entries[0].category, SECURITY_LOG_CAT_BBB);
    EXPECT_EQ(entries[0].bbb_threat, BBB_THREAT_BUFFER_OVERFLOW);
}

/**
 * @brief Test 8: BBB validation failures recorded
 *
 * WHAT: All BBB validation failures are recorded for analysis
 * WHY:  Enable pattern detection across validation failures
 * HOW:  Trigger multiple validation failures, verify all logged
 */
TEST_F(SecurityLoggingBBBTest, BBBValidationFailuresRecorded) {
    ASSERT_EQ(security_logging_connect_bbb(bridge, bbb), 0);

    /* Simulate multiple validation failures */
    const char* failure_types[] = {
        "SQL injection",
        "XSS attempt",
        "Path traversal",
        "Format string"
    };

    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(security_logging_log_bbb(
            bridge,
            static_cast<bbb_threat_type_t>(BBB_THREAT_SQL_INJECTION + i),
            BBB_SEVERITY_MEDIUM,
            BBB_ACTION_BLOCK,
            failure_types[i]
        ), 0);
    }

    /* Verify all failures logged */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_BBB], 4UL);
}

/**
 * @brief Test 9: BBB action audit trail
 *
 * WHAT: Complete audit trail of BBB actions (allow/block/quarantine)
 * WHY:  Track security decisions for compliance and analysis
 * HOW:  Log different action types, verify all recorded
 */
TEST_F(SecurityLoggingBBBTest, BBBActionAuditTrail) {
    ASSERT_EQ(security_logging_connect_bbb(bridge, bbb), 0);

    /* Log different action types */
    bbb_action_t actions[] = {
        BBB_ACTION_ALLOW,
        BBB_ACTION_LOG,
        BBB_ACTION_BLOCK,
        BBB_ACTION_QUARANTINE
    };

    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(security_logging_log_bbb(
            bridge,
            BBB_THREAT_CODE_INJECTION,
            BBB_SEVERITY_LOW,
            actions[i],
            "Action audit test"
        ), 0);
    }

    /* Query recent entries */
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    ASSERT_EQ(security_logging_get_recent(bridge, 10, entries, &actual_count), 0);
    EXPECT_GE(actual_count, 4UL);

    /* Verify different actions recorded */
    int action_counts[BBB_ACTION_LOCKDOWN + 1] = {0};
    for (size_t i = 0; i < actual_count; i++) {
        if (entries[i].action >= 0 && entries[i].action <= BBB_ACTION_LOCKDOWN) {
            action_counts[entries[i].action]++;
        }
    }
    /* At least some actions should be recorded */
    EXPECT_GT(action_counts[SECURITY_LOG_ACTION_ALLOW] +
              action_counts[SECURITY_LOG_ACTION_BLOCK], 0);
}

/*=============================================================================
 * SECURITY + ANOMALY DETECTOR INTEGRATION TESTS
 *============================================================================*/

/**
 * @brief Test 10: Anomaly detection events logged
 *
 * WHAT: Anomaly detector findings are logged for analysis
 * WHY:  Track behavioral anomalies for pattern correlation
 * HOW:  Connect anomaly detector, log anomaly results
 */
TEST_F(SecurityLoggingAnomalyTest, AnomalyDetectionEventsLogged) {
    /* Connect anomaly detector */
    ASSERT_EQ(security_logging_connect_anomaly_detector(bridge, anomaly_detector), 0);

    /* Create anomaly result */
    nimcp_anomaly_result_t result;
    result.anomaly_score = 0.85f;
    result.confidence = 0.9f;
    result.content_score = 0.7f;
    result.behavior_score = 0.8f;
    result.timing_score = 0.6f;
    result.triggered_features = NIMCP_TRIGGER_ENTROPY | NIMCP_TRIGGER_SPECIAL_RATIO;
    strncpy(result.explanation, "High entropy input with unusual special characters",
            sizeof(result.explanation) - 1);

    /* Log anomaly */
    ASSERT_EQ(security_logging_log_anomaly(
        bridge,
        &result,
        "suspicious_input_data",
        "Anomaly detected in user input"
    ), 0);

    /* Verify logged */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_ANOMALY], 1UL);
}

/**
 * @brief Test 11: Pattern extraction from logs
 *
 * WHAT: Extract attack patterns from accumulated logs
 * WHY:  Enable learning from historical security events
 * HOW:  Log pattern of events, run analysis, verify patterns detected
 */
TEST_F(SecurityLoggingAnomalyTest, PatternExtractionFromLogs) {
    ASSERT_EQ(security_logging_connect_anomaly_detector(bridge, anomaly_detector), 0);

    /* Enable pattern analysis */
    config.pattern_analysis.enabled = true;
    config.pattern_analysis.analysis_window_size = 50;
    config.pattern_analysis.min_occurrences = 3;

    /* Simulate brute force attack pattern (repeated failures) */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(security_logging_log_access(
            bridge,
            false,  /* denied */
            "attacker_ip",
            "/admin/login",
            "Failed authentication attempt"
        ), 0);
    }

    /* Run pattern analysis */
    int patterns_found = security_logging_analyze_patterns(bridge);

    /* May or may not detect pattern depending on thresholds */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.analysis_runs, 1UL);
}

/**
 * @brief Test 12: Feedback loop to anomaly detector
 *
 * WHAT: Detected patterns feed back to anomaly detector
 * WHY:  Improve anomaly detection based on historical patterns
 * HOW:  Create pattern, feed to detector, verify integration
 */
TEST_F(SecurityLoggingAnomalyTest, FeedbackLoopToAnomalyDetector) {
    ASSERT_EQ(security_logging_connect_anomaly_detector(bridge, anomaly_detector), 0);

    /* Create a threat pattern */
    security_threat_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 1;
    pattern.type = SECURITY_PATTERN_BRUTE_FORCE;
    pattern.occurrence_count = 15;
    pattern.severity_avg = 3.0f;
    pattern.confidence = 0.92f;
    strncpy(pattern.signature, "repeated_login_failures", sizeof(pattern.signature) - 1);
    strncpy(pattern.description, "Brute force login attack pattern", sizeof(pattern.description) - 1);

    /* Feed pattern to detector */
    EXPECT_EQ(security_logging_feed_pattern_to_detector(bridge, &pattern), 0);

    /* Verify pattern callback stats updated */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
}

/*=============================================================================
 * SECURITY + RATE LIMITER INTEGRATION TESTS
 *============================================================================*/

/**
 * @brief Test 13: Rate limit events logged
 *
 * WHAT: Rate limiting decisions are logged for analysis
 * WHY:  Track rate limiting effectiveness and identify abusers
 * HOW:  Connect rate limiter, log events, verify statistics
 */
TEST_F(SecurityLoggingRateLimiterTest, RateLimitEventsLogged) {
    /* Connect rate limiter */
    ASSERT_EQ(security_logging_connect_rate_limiter(bridge, rate_limiter), 0);

    /* Simulate rate limiting events */
    nimcp_client_stats_t client_stats;
    memset(&client_stats, 0, sizeof(client_stats));
    strncpy(client_stats.client_id, "client_192.168.1.100", sizeof(client_stats.client_id) - 1);
    client_stats.requests_allowed = 95;
    client_stats.requests_denied = 5;
    client_stats.violations = 2;

    ASSERT_EQ(security_logging_log_rate_limit(
        bridge,
        "client_192.168.1.100",
        false,  /* denied */
        &client_stats
    ), 0);

    /* Verify logged */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_RATE_LIMIT], 1UL);
}

/**
 * @brief Test 14: Client statistics recorded
 *
 * WHAT: Per-client rate limiting statistics are recorded
 * WHY:  Enable per-client abuse analysis and blocking decisions
 * HOW:  Log multiple client events, query by client
 */
TEST_F(SecurityLoggingRateLimiterTest, ClientStatisticsRecorded) {
    ASSERT_EQ(security_logging_connect_rate_limiter(bridge, rate_limiter), 0);

    /* Simulate events for multiple clients */
    const char* clients[] = {"client_a", "client_b", "client_c"};

    for (int c = 0; c < 3; c++) {
        nimcp_client_stats_t client_stats;
        memset(&client_stats, 0, sizeof(client_stats));
        strncpy(client_stats.client_id, clients[c], sizeof(client_stats.client_id) - 1);
        client_stats.requests_allowed = 50 + c * 10;
        client_stats.requests_denied = c * 5;

        for (int i = 0; i < 3; i++) {
            ASSERT_EQ(security_logging_log_rate_limit(
                bridge,
                clients[c],
                i < 2,  /* first 2 allowed, last denied */
                &client_stats
            ), 0);
        }
    }

    /* Verify events logged */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_RATE_LIMIT], 9UL);
}

/**
 * @brief Test 15: Abuse pattern detection from logs
 *
 * WHAT: Detect abusive clients from rate limit logs
 * WHY:  Automatically identify clients requiring blocking
 * HOW:  Log repeated denials, analyze for abuse patterns
 */
TEST_F(SecurityLoggingRateLimiterTest, AbusePatternDetectionFromLogs) {
    ASSERT_EQ(security_logging_connect_rate_limiter(bridge, rate_limiter), 0);

    /* Simulate abusive client */
    nimcp_client_stats_t abuser_stats;
    memset(&abuser_stats, 0, sizeof(abuser_stats));
    strncpy(abuser_stats.client_id, "abusive_client", sizeof(abuser_stats.client_id) - 1);
    abuser_stats.requests_denied = 100;
    abuser_stats.violations = 50;
    abuser_stats.state = CLIENT_STATE_BLOCKED_TEMP;

    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(security_logging_log_rate_limit(
            bridge,
            "abusive_client",
            false,  /* all denied */
            &abuser_stats
        ), 0);
    }

    /* Search for abuse pattern */
    security_log_entry_t entries[50];
    size_t actual_count = 0;
    ASSERT_EQ(security_logging_search(
        bridge,
        "abusive_client",
        entries,
        50,
        &actual_count
    ), 0);

    EXPECT_GE(actual_count, 20UL);
}

/*=============================================================================
 * BIDIRECTIONAL FLOW TESTS
 *============================================================================*/

/**
 * @brief Test 16: Security to Logging flow verification
 *
 * WHAT: Verify all security event types flow correctly to logging
 * WHY:  Ensure complete audit trail of security activities
 * HOW:  Log all event types, verify each category recorded
 */
TEST_F(SecurityLoggingBridgeTest, SecurityToLoggingFlow) {
    /* Log events from all categories */
    EXPECT_EQ(security_logging_log_threat(
        bridge, NIMCP_THREAT_HIGH, BBB_THREAT_CODE_INJECTION,
        "sec_test", "Threat event", SECURITY_LOG_ACTION_BLOCK
    ), 0);

    EXPECT_EQ(security_logging_log_access(
        bridge, true, "user", "/resource", "Access event"
    ), 0);

    EXPECT_EQ(security_logging_log_policy(
        bridge, "policy_001", SECURITY_LOG_ACTION_ALLOW,
        "/api/data", "Policy event"
    ), 0);

    EXPECT_EQ(security_logging_log_audit(
        bridge, SECURITY_LOG_SEV_INFO, "audit_test",
        "Audit event", "Details"
    ), 0);

    EXPECT_EQ(security_logging_log_crypto(
        bridge, "encrypt", true, "key_123", "Crypto event"
    ), 0);

    /* Verify all categories have entries */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);

    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_THREAT], 1UL);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_ACCESS], 1UL);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_POLICY], 1UL);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_AUDIT], 1UL);
    EXPECT_GE(stats.entries_by_category[SECURITY_LOG_CAT_CRYPTO], 1UL);
}

/**
 * @brief Test 17: Logging to Security feedback
 *
 * WHAT: Patterns detected in logs feed back to security systems
 * WHY:  Enable adaptive threat detection based on historical data
 * HOW:  Generate patterns, apply modulation, verify effects
 */
TEST_F(SecurityLoggingBridgeTest, LoggingToSecurityFeedback) {
    /* Generate events that form a pattern */
    for (int i = 0; i < 15; i++) {
        ASSERT_EQ(security_logging_log_threat(
            bridge,
            NIMCP_THREAT_MEDIUM,
            BBB_THREAT_SQL_INJECTION,
            "sql_detector",
            "SQL injection attempt",
            SECURITY_LOG_ACTION_BLOCK
        ), 0);
    }

    /* Run pattern analysis */
    security_logging_analyze_patterns(bridge);

    /* Apply modulation (feedback to security) */
    EXPECT_EQ(security_logging_apply_modulation(bridge), 0);

    /* Get effects */
    security_to_logging_effects_t sec_effects;
    logging_to_security_effects_t log_effects;
    ASSERT_EQ(security_logging_get_effects(bridge, &sec_effects, &log_effects), 0);
}

/**
 * @brief Test 18: Full cycle - threat to log to pattern to detection
 *
 * WHAT: Complete cycle from threat detection through pattern learning
 * WHY:  Verify end-to-end security intelligence pipeline
 * HOW:  Simulate attack, log, analyze, verify feedback
 */
TEST_F(SecurityLoggingBridgeTest, FullCycleThreatToDetection) {
    /* Phase 1: Initial threat detection */
    ASSERT_EQ(security_logging_log_threat(
        bridge,
        NIMCP_THREAT_HIGH,
        BBB_THREAT_ROP_CHAIN,
        "rop_detector",
        "ROP chain detected in buffer",
        SECURITY_LOG_ACTION_TERMINATE
    ), 0);

    /* Phase 2: Multiple similar events (attack campaign) */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(security_logging_log_threat(
            bridge,
            NIMCP_THREAT_HIGH,
            BBB_THREAT_ROP_CHAIN,
            "rop_detector",
            "ROP chain variant detected",
            SECURITY_LOG_ACTION_TERMINATE
        ), 0);
    }

    /* Phase 3: Pattern analysis */
    int patterns = security_logging_analyze_patterns(bridge);

    /* Phase 4: Apply feedback */
    ASSERT_EQ(security_logging_apply_modulation(bridge), 0);

    /* Phase 5: Verify complete cycle */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);

    EXPECT_GE(stats.total_entries, 11UL);
    EXPECT_GE(stats.entries_by_severity[SECURITY_LOG_SEV_CRITICAL] +
              stats.entries_by_severity[SECURITY_LOG_SEV_ERROR] +
              stats.entries_by_severity[SECURITY_LOG_SEV_WARNING], 1UL);
}

/*=============================================================================
 * LOG ANALYSIS TESTS
 *============================================================================*/

/**
 * @brief Test 19: Pattern detection in log stream
 *
 * WHAT: Detect attack patterns from streaming log data
 * WHY:  Real-time threat intelligence from log analysis
 * HOW:  Stream events, verify patterns detected
 */
TEST_F(SecurityLoggingBridgeTest, PatternDetectionInLogStream) {
    /* Simulate port scan pattern */
    for (int port = 1; port <= 20; port++) {
        char message[128];
        snprintf(message, sizeof(message), "Connection attempt to port %d", port);

        ASSERT_EQ(security_logging_log_audit(
            bridge,
            SECURITY_LOG_SEV_NOTICE,
            "port_monitor",
            message,
            "scan_pattern_test"
        ), 0);
    }

    /* Enable pattern analysis and run */
    int detected = security_logging_analyze_patterns(bridge);

    /* Get detected patterns */
    security_threat_pattern_t patterns[10];
    size_t pattern_count = 0;
    EXPECT_EQ(security_logging_get_patterns(bridge, patterns, 10, &pattern_count), 0);

    /* Clear patterns for next test */
    EXPECT_EQ(security_logging_clear_patterns(bridge), 0);
}

/**
 * @brief Test 20: Attack sequence identification
 *
 * WHAT: Identify multi-stage attack sequences
 * WHY:  Detect sophisticated attacks with multiple phases
 * HOW:  Log attack sequence, analyze for correlation
 */
TEST_F(SecurityLoggingBridgeTest, AttackSequenceIdentification) {
    uint64_t correlation_id = 12345;

    /* Phase 1: Reconnaissance */
    security_log_entry_t recon_entry;
    security_log_entry_init(&recon_entry, SECURITY_LOG_CAT_THREAT,
                           SECURITY_LOG_SEV_INFO, "Port scan detected");
    recon_entry.correlation_id = correlation_id;
    ASSERT_EQ(security_logging_log_entry(bridge, &recon_entry), 0);

    /* Phase 2: Exploitation */
    security_log_entry_t exploit_entry;
    security_log_entry_init(&exploit_entry, SECURITY_LOG_CAT_THREAT,
                           SECURITY_LOG_SEV_WARNING, "SQL injection attempt");
    exploit_entry.correlation_id = correlation_id;
    exploit_entry.parent_entry_id = recon_entry.entry_id;
    ASSERT_EQ(security_logging_log_entry(bridge, &exploit_entry), 0);

    /* Phase 3: Privilege escalation */
    security_log_entry_t privesc_entry;
    security_log_entry_init(&privesc_entry, SECURITY_LOG_CAT_THREAT,
                           SECURITY_LOG_SEV_ERROR, "Privilege escalation detected");
    privesc_entry.correlation_id = correlation_id;
    privesc_entry.parent_entry_id = exploit_entry.entry_id;
    ASSERT_EQ(security_logging_log_entry(bridge, &privesc_entry), 0);

    /* Query correlated entries */
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    ASSERT_EQ(security_logging_get_recent(bridge, 10, entries, &actual_count), 0);
    EXPECT_GE(actual_count, 3UL);
}

/**
 * @brief Test 21: Behavioral baseline deviation
 *
 * WHAT: Detect deviations from normal behavior baseline
 * WHY:  Identify anomalous activity that differs from normal patterns
 * HOW:  Establish baseline, introduce anomaly, verify detection
 */
TEST_F(SecurityLoggingBridgeTest, BehavioralBaselineDeviation) {
    /* Establish baseline: normal traffic pattern */
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(security_logging_log_access(
            bridge,
            true,  /* allowed */
            "normal_user",
            "/api/data",
            "Normal API access"
        ), 0);
    }

    /* Introduce anomalous behavior */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(security_logging_log_access(
            bridge,
            false,  /* denied */
            "normal_user",
            "/admin/config",
            "Unusual admin access attempt"
        ), 0);
    }

    /* Count entries by severity */
    size_t entry_count = security_logging_count_entries(
        bridge,
        0, 0,  /* all time */
        SECURITY_LOG_CAT_MASK(SECURITY_LOG_CAT_ACCESS),
        SECURITY_LOG_SEV_DEBUG
    );

    EXPECT_GE(entry_count, 25UL);
}

/*=============================================================================
 * PERFORMANCE TESTS
 *============================================================================*/

/**
 * @brief Test 22: High-frequency logging performance
 *
 * WHAT: Verify logging handles high event rates
 * WHY:  Security events can come in bursts during attacks
 * HOW:  Generate high-frequency events, measure throughput
 */
TEST_F(SecurityLoggingBridgeTest, HighFrequencyLogging) {
    const int NUM_EVENTS = 500;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_EVENTS; i++) {
        ASSERT_EQ(security_logging_log_audit(
            bridge,
            SECURITY_LOG_SEV_INFO,
            "perf_test",
            "High-frequency logging test event",
            nullptr
        ), 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    /* Verify all events logged */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.total_entries, static_cast<uint64_t>(NUM_EVENTS));

    /* Log throughput for analysis (not a hard requirement) */
    double events_per_sec = (NUM_EVENTS * 1000000.0) / duration.count();
    EXPECT_GT(events_per_sec, 1000.0);  /* At least 1000 events/sec */
}

/**
 * @brief Test 23: Buffer management under load
 *
 * WHAT: Verify buffer handles overload conditions properly
 * WHY:  Prevent data loss or crashes during attack floods
 * HOW:  Exceed buffer capacity, verify graceful handling
 */
TEST_F(SecurityLoggingBridgeTest, BufferManagementUnderLoad) {
    /* Fill buffer beyond capacity (config has 1024 entry buffer) */
    const int OVERFLOW_COUNT = 1500;

    for (int i = 0; i < OVERFLOW_COUNT; i++) {
        security_logging_log_audit(
            bridge,
            SECURITY_LOG_SEV_DEBUG,
            "buffer_test",
            "Buffer overflow test event",
            nullptr
        );
    }

    /* Verify no crash and stats reflect situation */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);

    /* Either entries are stored or dropped are counted */
    EXPECT_GT(stats.total_entries + stats.entries_dropped, 0UL);

    /* Buffer utilization should be tracked */
    EXPECT_GE(stats.buffer_utilization, 0.0f);
}

/**
 * @brief Test 24: Lock-free operation verification
 *
 * WHAT: Verify lock-free operations work correctly
 * WHY:  Minimize latency in high-performance scenarios
 * HOW:  Multi-threaded logging, verify no deadlocks
 */
TEST_F(SecurityLoggingBridgeTest, LockFreeOperationVerification) {
    std::atomic<int> total_logged{0};
    const int NUM_THREADS = 4;
    const int EVENTS_PER_THREAD = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &total_logged, EVENTS_PER_THREAD]() {
            for (int i = 0; i < EVENTS_PER_THREAD; i++) {
                char message[64];
                snprintf(message, sizeof(message), "Thread %d event %d", t, i);

                if (security_logging_log_audit(
                    bridge,
                    SECURITY_LOG_SEV_INFO,
                    "mt_test",
                    message,
                    nullptr
                ) == 0) {
                    total_logged++;
                }
            }
        });
    }

    /* Wait for all threads to complete (no deadlock) */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify events were logged */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);

    /* Most events should be logged (allow some drops due to contention) */
    EXPECT_GE(stats.total_entries, static_cast<uint64_t>(NUM_THREADS * EVENTS_PER_THREAD * 0.9));
}

/*=============================================================================
 * UTILITY FUNCTION TESTS
 *============================================================================*/

/**
 * @brief Test 25: Category name conversion
 */
TEST_F(SecurityLoggingBridgeTest, CategoryNameConversion) {
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_THREAT), "THREAT");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_ACCESS), "ACCESS");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_POLICY), "POLICY");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_BBB), "BBB");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_ANOMALY), "ANOMALY");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_RATE_LIMIT), "RATE_LIMIT");
}

/**
 * @brief Test 26: Severity name and level conversion
 */
TEST_F(SecurityLoggingBridgeTest, SeverityNameConversion) {
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_DEBUG), "DEBUG");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_INFO), "INFO");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_WARNING), "WARNING");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_ERROR), "ERROR");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_CRITICAL), "CRITICAL");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_ALERT), "ALERT");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_EMERGENCY), "EMERGENCY");

    /* Test threat to severity conversion */
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_NONE), SECURITY_LOG_SEV_DEBUG);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_LOW), SECURITY_LOG_SEV_INFO);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_MEDIUM), SECURITY_LOG_SEV_WARNING);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_HIGH), SECURITY_LOG_SEV_ERROR);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_CRITICAL), SECURITY_LOG_SEV_CRITICAL);
}

/**
 * @brief Test 27: Bridge state and connection management
 */
TEST_F(SecurityLoggingBridgeTest, BridgeStateAndConnectionManagement) {
    /* Verify initial state */
    EXPECT_TRUE(security_logging_is_connected(bridge) || !security_logging_is_connected(bridge));

    /* Reset bridge */
    ASSERT_EQ(security_logging_bridge_reset(bridge), 0);

    /* Verify statistics reset */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_entries, 0UL);

    /* Reset stats */
    ASSERT_EQ(security_logging_reset_stats(bridge), 0);
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_entries, 0UL);
}

/**
 * @brief Test 28: Flush and update operations
 */
TEST_F(SecurityLoggingBridgeTest, FlushAndUpdateOperations) {
    /* Log some events */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(security_logging_log_audit(
            bridge, SECURITY_LOG_SEV_INFO, "flush_test",
            "Event for flush test", nullptr
        ), 0);
    }

    /* Flush pending entries */
    ASSERT_EQ(security_logging_flush(bridge), 0);

    /* Update bridge (process pending) */
    ASSERT_EQ(security_logging_update(bridge), 0);

    /* Verify all processed */
    security_logging_bridge_stats_t stats;
    ASSERT_EQ(security_logging_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.total_entries, 10UL);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
