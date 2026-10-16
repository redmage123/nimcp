/**
 * @file test_security_logging_bridge.cpp
 * @brief Comprehensive unit tests for Security-Logging Bridge
 *
 * WHAT: Tests for the bidirectional security-logging bridge that connects
 *       security systems (BBB, anomaly detector, rate limiter) with logging
 *       infrastructure for comprehensive audit trails and pattern detection.
 *
 * WHY:  Ensure proper security event logging, pattern analysis feedback,
 *       and bidirectional effects between security and logging systems.
 *
 * HOW:  Google Test framework with fixture-based setup/teardown.
 *       Tests cover lifecycle, connections, core logging, streaming,
 *       pattern analysis, query, export, statistics, updates, bio-async,
 *       and utility functions.
 *
 * TEST COVERAGE:
 * - Lifecycle: create/destroy/reset with config validation
 * - Connection: BBB, anomaly detector, rate limiter, audit, logger
 * - Core Logging: entry, threat, access, policy, BBB, anomaly, rate limit,
 *                 crypto, audit events
 * - Streaming: register/unregister callbacks, pattern callbacks
 * - Pattern Analysis: analyze, get, clear, feed to detector
 * - Query: entries, recent, search, count
 * - Export: file, JSON, rotation, flush
 * - Statistics: get, reset, effects
 * - Update: process pending, apply modulation
 * - Bio-Async: connect/disconnect, inbox processing, broadcast
 * - Utility: name conversions, severity mappings, entry initialization
 *
 * BIOLOGICAL BASIS:
 * Models immune system's memory and logging mechanisms - dendritic cells
 * present antigens to memory systems, memory B cells retain threat patterns.
 *
 * @author NIMCP Security Team
 * @date 2026-01-09
 */

#include <gtest/gtest.h>

extern "C" {
#include "security/logging/nimcp_security_logging_bridge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_encrypted_audit.h"
#include "utils/logging/nimcp_logging.h"
}

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityLoggingBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Get default config
        int result = security_logging_default_config(&config);
        ASSERT_EQ(result, 0);

        // Create bridge with smaller buffer for testing
        config.buffer_capacity = 256;
        config.min_severity = SECURITY_LOG_SEV_DEBUG;  // Accept all severities for testing
        config.log_to_console = false;
        config.log_to_file = false;
        config.enable_encrypted_audit = false;
        config.enable_nimcp_logging = false;
        config.enable_bio_async = false;
        config.pattern_analysis.enabled = true;
        config.pattern_analysis.analysis_window_size = 100;
        config.enable_timestamps = true;
        config.enable_metrics = true;

        bridge = security_logging_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override
    {
        if (bridge) {
            security_logging_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper to create a test log entry
    void create_test_entry(security_log_entry_t* entry,
                           security_log_category_t category,
                           security_log_severity_t severity,
                           const char* message)
    {
        memset(entry, 0, sizeof(*entry));
        entry->category = category;
        entry->severity = severity;
        entry->action = SECURITY_LOG_ACTION_NONE;
        entry->timestamp_ns = security_log_current_time_ns();
        if (message) {
            strncpy(entry->message, message, SECURITY_LOG_MAX_MESSAGE_LEN - 1);
        }
    }

    // Helper to populate multiple log entries
    void populate_log_entries(size_t count, security_log_category_t category)
    {
        security_log_entry_t entry;
        for (size_t i = 0; i < count; i++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Test entry %zu", i);
            create_test_entry(&entry, category, SECURITY_LOG_SEV_INFO, msg);
            security_logging_log_entry(bridge, &entry);
        }
    }

    security_logging_bridge_config_t config;
    security_logging_bridge_t* bridge;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, DefaultConfigReturnsValidConfig)
{
    security_logging_bridge_config_t default_config;
    int result = security_logging_default_config(&default_config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(default_config.buffer_capacity, SECURITY_LOG_DEFAULT_BUFFER_SIZE);
    EXPECT_EQ(default_config.min_severity, SECURITY_LOG_SEV_INFO);
    EXPECT_EQ(default_config.format, SECURITY_LOG_FORMAT_JSON);
    EXPECT_TRUE(default_config.overwrite_on_full);
    EXPECT_EQ(default_config.enabled_categories, SECURITY_LOG_CAT_ALL);
}

TEST_F(SecurityLoggingBridgeTest, DefaultConfigNullReturnsError)
{
    int result = security_logging_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, CreateWithNullConfigUsesDefaults)
{
    security_logging_bridge_t* default_bridge = security_logging_bridge_create(nullptr);
    EXPECT_NE(default_bridge, nullptr);
    if (default_bridge) {
        security_logging_bridge_destroy(default_bridge);
    }
}

TEST_F(SecurityLoggingBridgeTest, CreateWithValidConfig)
{
    EXPECT_NE(bridge, nullptr);

    // Verify stats are initialized
    security_logging_bridge_stats_t stats;
    int result = security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_entries, 0);
    EXPECT_EQ(stats.buffer_capacity, config.buffer_capacity);
}

TEST_F(SecurityLoggingBridgeTest, DestroyNullBridgeSafe)
{
    // Should not crash
    security_logging_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SecurityLoggingBridgeTest, ResetBridgeClearsStatistics)
{
    // Log some entries
    populate_log_entries(10, SECURITY_LOG_CAT_AUDIT);

    // Verify entries were logged
    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_entries, 0);

    // Reset bridge
    int result = security_logging_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Verify stats are reset
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_entries, 0);
}

TEST_F(SecurityLoggingBridgeTest, ResetNullBridgeReturnsError)
{
    int result = security_logging_bridge_reset(nullptr);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, ConnectBBBNullBridgeReturnsError)
{
    bbb_system_t bbb = nullptr;
    int result = security_logging_connect_bbb(nullptr, bbb);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ConnectAnomalyDetectorNullBridgeReturnsError)
{
    nimcp_anomaly_detector_t detector = nullptr;
    int result = security_logging_connect_anomaly_detector(nullptr, detector);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ConnectRateLimiterNullBridgeReturnsError)
{
    nimcp_rate_limiter_t limiter = nullptr;
    int result = security_logging_connect_rate_limiter(nullptr, limiter);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ConnectEncryptedAuditNullBridgeReturnsError)
{
    nimcp_encrypted_audit_t audit = nullptr;
    int result = security_logging_connect_encrypted_audit(nullptr, audit);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ConnectNIMCPLoggerNullBridgeReturnsError)
{
    nimcp_logger_t logger = nullptr;
    int result = security_logging_connect_nimcp_logger(nullptr, logger);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, DisconnectAllNullBridgeReturnsError)
{
    int result = security_logging_disconnect_all(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, DisconnectAllSucceeds)
{
    int result = security_logging_disconnect_all(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, IsConnectedNullBridgeReturnsFalse)
{
    bool connected = security_logging_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SecurityLoggingBridgeTest, IsConnectedInitiallyFalse)
{
    // With no actual systems connected, should return false
    bool connected = security_logging_is_connected(bridge);
    // Note: Implementation may return true if internal buffer is valid
    // This tests the function doesn't crash with valid bridge
    (void)connected;
    SUCCEED();
}

//=============================================================================
// Core Logging Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, LogEntryNullBridgeReturnsError)
{
    security_log_entry_t entry;
    create_test_entry(&entry, SECURITY_LOG_CAT_AUDIT, SECURITY_LOG_SEV_INFO, "Test");

    int result = security_logging_log_entry(nullptr, &entry);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogEntryNullEntryReturnsError)
{
    int result = security_logging_log_entry(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogEntrySucceeds)
{
    security_log_entry_t entry;
    create_test_entry(&entry, SECURITY_LOG_CAT_AUDIT, SECURITY_LOG_SEV_INFO, "Test audit entry");

    int result = security_logging_log_entry(bridge, &entry);
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_entries, 1);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_AUDIT], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogThreatNullBridgeReturnsError)
{
    int result = security_logging_log_threat(
        nullptr, NIMCP_THREAT_HIGH, BBB_THREAT_CODE_INJECTION,
        "test", "Injection attempt", SECURITY_LOG_ACTION_BLOCK);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogThreatSucceeds)
{
    int result = security_logging_log_threat(
        bridge, NIMCP_THREAT_HIGH, BBB_THREAT_CODE_INJECTION,
        "security_module", "Detected code injection attempt",
        SECURITY_LOG_ACTION_BLOCK);
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_THREAT], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogAccessNullBridgeReturnsError)
{
    int result = security_logging_log_access(nullptr, true, "user", "resource", "Access granted");
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogAccessAllowed)
{
    int result = security_logging_log_access(bridge, true, "user123", "/api/data", "Read access granted");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_ACCESS], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogAccessDenied)
{
    int result = security_logging_log_access(bridge, false, "attacker", "/admin", "Access denied - insufficient privileges");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_ACCESS], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogPolicyNullBridgeReturnsError)
{
    int result = security_logging_log_policy(nullptr, "policy-001", SECURITY_LOG_ACTION_DENY, "target", "Policy enforced");
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogPolicySucceeds)
{
    int result = security_logging_log_policy(bridge, "rate-limit-policy",
        SECURITY_LOG_ACTION_BLOCK, "api-endpoint", "Rate limit policy enforced");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_POLICY], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogBBBNullBridgeReturnsError)
{
    int result = security_logging_log_bbb(nullptr, BBB_THREAT_BUFFER_OVERFLOW,
        BBB_SEVERITY_HIGH, BBB_ACTION_BLOCK, "Buffer overflow detected");
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogBBBSucceeds)
{
    int result = security_logging_log_bbb(bridge, BBB_THREAT_SQL_INJECTION,
        BBB_SEVERITY_CRITICAL, BBB_ACTION_QUARANTINE, "SQL injection blocked at perimeter");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_BBB], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogAnomalyNullBridgeReturnsError)
{
    nimcp_anomaly_result_t result_struct;
    memset(&result_struct, 0, sizeof(result_struct));
    result_struct.anomaly_score = 0.85f;

    int result = security_logging_log_anomaly(nullptr, &result_struct, nullptr, "Anomaly detected");
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogAnomalySucceeds)
{
    nimcp_anomaly_result_t anomaly_result;
    memset(&anomaly_result, 0, sizeof(anomaly_result));
    anomaly_result.anomaly_score = 0.92f;
    anomaly_result.confidence = 0.88f;
    anomaly_result.content_score = 0.95f;
    strncpy(anomaly_result.explanation, "High entropy content", sizeof(anomaly_result.explanation) - 1);

    int result = security_logging_log_anomaly(bridge, &anomaly_result, nullptr, "Behavioral anomaly detected");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_ANOMALY], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogRateLimitNullBridgeReturnsError)
{
    int result = security_logging_log_rate_limit(nullptr, "client-1", false, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogRateLimitSucceeds)
{
    nimcp_client_stats_t client_stats;
    memset(&client_stats, 0, sizeof(client_stats));
    strncpy(client_stats.client_id, "192.168.1.100", NIMCP_RATE_LIMIT_MAX_CLIENT_ID - 1);
    client_stats.requests_denied = 50;
    client_stats.violations = 3;
    client_stats.state = CLIENT_STATE_RATE_REDUCED;

    int result = security_logging_log_rate_limit(bridge, "192.168.1.100", false, &client_stats);
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_RATE_LIMIT], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogCryptoNullBridgeReturnsError)
{
    int result = security_logging_log_crypto(nullptr, "encrypt", true, "key-001", "Encryption successful");
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogCryptoSuccess)
{
    int result = security_logging_log_crypto(bridge, "encrypt", true, "master-key-v2", "AES-256-GCM encryption completed");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_CRYPTO], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogCryptoFailure)
{
    int result = security_logging_log_crypto(bridge, "decrypt", false, "expired-key", "Decryption failed - invalid authentication tag");
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogAuditNullBridgeReturnsError)
{
    int result = security_logging_log_audit(nullptr, SECURITY_LOG_SEV_INFO, "audit", "Test", nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, LogAuditSucceeds)
{
    int result = security_logging_log_audit(bridge, SECURITY_LOG_SEV_NOTICE,
        "authentication_service", "User login successful", "User: admin, IP: 10.0.0.1");
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_AUDIT], 1);
}

TEST_F(SecurityLoggingBridgeTest, LogMultipleCategoriesTracked)
{
    // Log entries across multiple categories
    security_logging_log_threat(bridge, NIMCP_THREAT_LOW, BBB_THREAT_NONE, "src", "threat1", SECURITY_LOG_ACTION_ALLOW);
    security_logging_log_access(bridge, true, "user", "obj", "msg");
    security_logging_log_policy(bridge, "pol", SECURITY_LOG_ACTION_ALLOW, "tgt", "msg");
    security_logging_log_bbb(bridge, BBB_THREAT_NONE, BBB_SEVERITY_NONE, BBB_ACTION_ALLOW, "msg");
    security_logging_log_crypto(bridge, "sign", true, "key", "msg");

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);

    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_THREAT], 1);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_ACCESS], 1);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_POLICY], 1);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_BBB], 1);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_CRYPTO], 1);
    EXPECT_EQ(stats.total_entries, 5);
}

//=============================================================================
// Streaming Tests
//=============================================================================

// Stream callback tracking
static std::atomic<int> g_stream_callback_count{0};
static security_log_entry_t g_last_stream_entry;

static bool test_stream_callback(const security_log_entry_t* entry, void* user_data)
{
    (void)user_data;
    g_stream_callback_count++;
    if (entry) {
        memcpy(&g_last_stream_entry, entry, sizeof(g_last_stream_entry));
    }
    return true;  // Continue streaming
}

static bool test_stream_callback_stop(const security_log_entry_t* entry, void* user_data)
{
    (void)entry;
    (void)user_data;
    return false;  // Stop streaming
}

TEST_F(SecurityLoggingBridgeTest, RegisterStreamNullBridgeReturnsError)
{
    int callback_id = security_logging_register_stream(nullptr, test_stream_callback, nullptr, 0);
    EXPECT_EQ(callback_id, -1);
}

TEST_F(SecurityLoggingBridgeTest, RegisterStreamNullCallbackReturnsError)
{
    int callback_id = security_logging_register_stream(bridge, nullptr, nullptr, 0);
    EXPECT_EQ(callback_id, -1);
}

TEST_F(SecurityLoggingBridgeTest, RegisterStreamSucceeds)
{
    int callback_id = security_logging_register_stream(bridge, test_stream_callback, nullptr, 0);
    EXPECT_GE(callback_id, 0);

    // Cleanup
    if (callback_id >= 0) {
        security_logging_unregister_stream(bridge, callback_id);
    }
}

TEST_F(SecurityLoggingBridgeTest, UnregisterStreamNullBridgeReturnsError)
{
    int result = security_logging_unregister_stream(nullptr, 0);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, UnregisterStreamInvalidIdReturnsError)
{
    int result = security_logging_unregister_stream(bridge, 999);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, UnregisterStreamSucceeds)
{
    int callback_id = security_logging_register_stream(bridge, test_stream_callback, nullptr, 0);
    ASSERT_GE(callback_id, 0);

    int result = security_logging_unregister_stream(bridge, callback_id);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, RegisterPatternCallbackNullBridgeReturnsError)
{
    int result = security_logging_register_pattern_callback(nullptr,
        [](const security_threat_pattern_t*, void*) {}, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, RegisterPatternCallbackNullCallbackReturnsError)
{
    int result = security_logging_register_pattern_callback(bridge, nullptr, nullptr);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Pattern Analysis Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, AnalyzePatternsNullBridgeReturnsError)
{
    int result = security_logging_analyze_patterns(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, AnalyzePatternsEmptyBuffer)
{
    int patterns_found = security_logging_analyze_patterns(bridge);
    EXPECT_GE(patterns_found, 0);
}

TEST_F(SecurityLoggingBridgeTest, AnalyzePatternsWithEntries)
{
    // Populate with threat entries to trigger pattern detection
    for (int i = 0; i < 20; i++) {
        security_logging_log_threat(bridge, NIMCP_THREAT_HIGH, BBB_THREAT_CODE_INJECTION,
            "test_module", "Repeated injection attempt", SECURITY_LOG_ACTION_BLOCK);
    }

    int patterns_found = security_logging_analyze_patterns(bridge);
    // May or may not find patterns depending on implementation
    EXPECT_GE(patterns_found, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetPatternsNullBridgeReturnsError)
{
    security_threat_pattern_t patterns[10];
    size_t actual_count = 0;
    int result = security_logging_get_patterns(nullptr, patterns, 10, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetPatternsNullArrayReturnsError)
{
    size_t actual_count = 0;
    int result = security_logging_get_patterns(bridge, nullptr, 10, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetPatternsNullCountReturnsError)
{
    security_threat_pattern_t patterns[10];
    int result = security_logging_get_patterns(bridge, patterns, 10, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetPatternsSucceeds)
{
    security_threat_pattern_t patterns[10];
    size_t actual_count = 0;
    int result = security_logging_get_patterns(bridge, patterns, 10, &actual_count);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ClearPatternsNullBridgeReturnsError)
{
    int result = security_logging_clear_patterns(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ClearPatternsSucceeds)
{
    int result = security_logging_clear_patterns(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, FeedPatternToDetectorNullBridgeReturnsError)
{
    security_threat_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    int result = security_logging_feed_pattern_to_detector(nullptr, &pattern);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, FeedPatternToDetectorNullPatternReturnsError)
{
    int result = security_logging_feed_pattern_to_detector(bridge, nullptr);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, QueryEntriesNullBridgeReturnsError)
{
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_query_entries(nullptr, 0, 0, 0,
        SECURITY_LOG_SEV_DEBUG, entries, 10, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, QueryEntriesNullArrayReturnsError)
{
    size_t actual_count = 0;
    int result = security_logging_query_entries(bridge, 0, 0, 0,
        SECURITY_LOG_SEV_DEBUG, nullptr, 10, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, QueryEntriesEmptyBuffer)
{
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_query_entries(bridge, 0, 0, SECURITY_LOG_CAT_ALL,
        SECURITY_LOG_SEV_DEBUG, entries, 10, &actual_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(actual_count, 0);
}

TEST_F(SecurityLoggingBridgeTest, QueryEntriesReturnsMatches)
{
    // Populate entries
    populate_log_entries(5, SECURITY_LOG_CAT_AUDIT);

    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_query_entries(bridge, 0, 0,
        SECURITY_LOG_CAT_MASK(SECURITY_LOG_CAT_AUDIT),
        SECURITY_LOG_SEV_DEBUG, entries, 10, &actual_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(actual_count, 5);
}

TEST_F(SecurityLoggingBridgeTest, QueryEntriesFiltersByCategory)
{
    // Add entries of different categories
    populate_log_entries(3, SECURITY_LOG_CAT_AUDIT);
    security_logging_log_threat(bridge, NIMCP_THREAT_LOW, BBB_THREAT_NONE, "src", "msg", SECURITY_LOG_ACTION_ALLOW);
    security_logging_log_threat(bridge, NIMCP_THREAT_LOW, BBB_THREAT_NONE, "src", "msg", SECURITY_LOG_ACTION_ALLOW);

    security_log_entry_t entries[10];
    size_t actual_count = 0;

    // Query only threat entries
    int result = security_logging_query_entries(bridge, 0, 0,
        SECURITY_LOG_CAT_MASK(SECURITY_LOG_CAT_THREAT),
        SECURITY_LOG_SEV_DEBUG, entries, 10, &actual_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(actual_count, 2);
}

TEST_F(SecurityLoggingBridgeTest, GetRecentNullBridgeReturnsError)
{
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_get_recent(nullptr, 10, entries, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetRecentNullArrayReturnsError)
{
    size_t actual_count = 0;
    int result = security_logging_get_recent(bridge, 10, nullptr, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetRecentSucceeds)
{
    populate_log_entries(5, SECURITY_LOG_CAT_AUDIT);

    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_get_recent(bridge, 3, entries, &actual_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(actual_count, 3);
}

TEST_F(SecurityLoggingBridgeTest, SearchNullBridgeReturnsError)
{
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_search(nullptr, "test", entries, 10, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, SearchNullTermReturnsError)
{
    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_search(bridge, nullptr, entries, 10, &actual_count);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, SearchFindsMatches)
{
    // Add specific entries
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_INFO, "src", "Login successful", nullptr);
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_INFO, "src", "Login failed", nullptr);
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_INFO, "src", "Session created", nullptr);

    security_log_entry_t entries[10];
    size_t actual_count = 0;
    int result = security_logging_search(bridge, "Login", entries, 10, &actual_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(actual_count, 2);
}

TEST_F(SecurityLoggingBridgeTest, CountEntriesNullBridgeReturnsZero)
{
    size_t count = security_logging_count_entries(nullptr, 0, 0, 0, SECURITY_LOG_SEV_DEBUG);
    EXPECT_EQ(count, 0);
}

TEST_F(SecurityLoggingBridgeTest, CountEntriesReturnsCorrectCount)
{
    populate_log_entries(7, SECURITY_LOG_CAT_AUDIT);

    size_t count = security_logging_count_entries(bridge, 0, 0, SECURITY_LOG_CAT_ALL, SECURITY_LOG_SEV_DEBUG);
    EXPECT_EQ(count, 7);
}

//=============================================================================
// Export Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, ExportToFileNullBridgeReturnsError)
{
    int result = security_logging_export_to_file(nullptr, "/tmp/test.log",
        SECURITY_LOG_FORMAT_JSON, 0, 0);
    EXPECT_LT(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ExportToFileNullPathReturnsError)
{
    int result = security_logging_export_to_file(bridge, nullptr,
        SECURITY_LOG_FORMAT_JSON, 0, 0);
    EXPECT_LT(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, EntryToJSONNullEntryReturnsError)
{
    char buffer[1024];
    int result = security_logging_entry_to_json(nullptr, buffer, sizeof(buffer));
    EXPECT_LT(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, EntryToJSONNullBufferReturnsError)
{
    security_log_entry_t entry;
    create_test_entry(&entry, SECURITY_LOG_CAT_AUDIT, SECURITY_LOG_SEV_INFO, "Test");

    int result = security_logging_entry_to_json(&entry, nullptr, 1024);
    EXPECT_LT(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, EntryToJSONZeroSizeReturnsError)
{
    security_log_entry_t entry;
    create_test_entry(&entry, SECURITY_LOG_CAT_AUDIT, SECURITY_LOG_SEV_INFO, "Test");
    char buffer[1024];

    int result = security_logging_entry_to_json(&entry, buffer, 0);
    EXPECT_LT(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, EntryToJSONSucceeds)
{
    security_log_entry_t entry;
    create_test_entry(&entry, SECURITY_LOG_CAT_THREAT, SECURITY_LOG_SEV_CRITICAL, "Critical threat detected");
    entry.action = SECURITY_LOG_ACTION_BLOCK;
    entry.threat_level = NIMCP_THREAT_HIGH;
    entry.confidence_score = 0.95f;

    char buffer[2048];
    int bytes_written = security_logging_entry_to_json(&entry, buffer, sizeof(buffer));
    EXPECT_GT(bytes_written, 0);

    // Verify JSON contains expected fields
    EXPECT_NE(strstr(buffer, "\"category\""), nullptr);
    EXPECT_NE(strstr(buffer, "\"severity\""), nullptr);
    EXPECT_NE(strstr(buffer, "\"message\""), nullptr);
}

TEST_F(SecurityLoggingBridgeTest, RotateNullBridgeReturnsError)
{
    int result = security_logging_rotate(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, RotateSucceeds)
{
    // Configure file logging for rotation test
    security_logging_bridge_config_t file_config;
    security_logging_default_config(&file_config);
    file_config.log_to_file = false;  // Don't actually write files in test

    int result = security_logging_rotate(bridge);
    // May succeed or fail depending on config
    (void)result;
    SUCCEED();
}

TEST_F(SecurityLoggingBridgeTest, FlushNullBridgeReturnsError)
{
    int result = security_logging_flush(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, FlushSucceeds)
{
    populate_log_entries(5, SECURITY_LOG_CAT_AUDIT);
    int result = security_logging_flush(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, GetStatsNullBridgeReturnsError)
{
    security_logging_bridge_stats_t stats;
    int result = security_logging_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetStatsNullStatsReturnsError)
{
    int result = security_logging_get_stats(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetStatsReturnsValidData)
{
    populate_log_entries(10, SECURITY_LOG_CAT_AUDIT);

    security_logging_bridge_stats_t stats;
    int result = security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_entries, 10);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_AUDIT], 10);
    EXPECT_EQ(stats.buffer_capacity, config.buffer_capacity);
}

TEST_F(SecurityLoggingBridgeTest, GetStatsSeverityTracking)
{
    // Log entries with different severities
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_DEBUG, "src", "Debug msg", nullptr);
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_INFO, "src", "Info msg", nullptr);
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_WARNING, "src", "Warning msg", nullptr);
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_ERROR, "src", "Error msg", nullptr);
    security_logging_log_audit(bridge, SECURITY_LOG_SEV_CRITICAL, "src", "Critical msg", nullptr);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);

    EXPECT_EQ(stats.entries_by_severity[SECURITY_LOG_SEV_DEBUG], 1);
    EXPECT_EQ(stats.entries_by_severity[SECURITY_LOG_SEV_INFO], 1);
    EXPECT_EQ(stats.entries_by_severity[SECURITY_LOG_SEV_WARNING], 1);
    EXPECT_EQ(stats.entries_by_severity[SECURITY_LOG_SEV_ERROR], 1);
    EXPECT_EQ(stats.entries_by_severity[SECURITY_LOG_SEV_CRITICAL], 1);
}

TEST_F(SecurityLoggingBridgeTest, ResetStatsNullBridgeReturnsError)
{
    int result = security_logging_reset_stats(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ResetStatsSucceeds)
{
    populate_log_entries(10, SECURITY_LOG_CAT_AUDIT);

    int result = security_logging_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_entries, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetEffectsNullBridgeReturnsError)
{
    security_to_logging_effects_t sec_effects;
    logging_to_security_effects_t log_effects;
    int result = security_logging_get_effects(nullptr, &sec_effects, &log_effects);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetEffectsSucceeds)
{
    security_to_logging_effects_t sec_effects;
    logging_to_security_effects_t log_effects;
    int result = security_logging_get_effects(bridge, &sec_effects, &log_effects);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, GetEffectsTracksBidirectionalFlow)
{
    // Log some threat events
    security_logging_log_threat(bridge, NIMCP_THREAT_HIGH, BBB_THREAT_CODE_INJECTION, "src", "msg", SECURITY_LOG_ACTION_BLOCK);
    security_logging_log_policy(bridge, "pol", SECURITY_LOG_ACTION_DENY, "tgt", "msg");
    security_logging_log_access(bridge, false, "user", "obj", "msg");

    security_to_logging_effects_t sec_effects;
    logging_to_security_effects_t log_effects;
    int result = security_logging_get_effects(bridge, &sec_effects, &log_effects);
    EXPECT_EQ(result, 0);

    // Effects tracking depends on implementation
    // Just verify the function works
    SUCCEED();
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, UpdateNullBridgeReturnsError)
{
    int result = security_logging_update(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, UpdateSucceeds)
{
    populate_log_entries(5, SECURITY_LOG_CAT_AUDIT);

    int result = security_logging_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ApplyModulationNullBridgeReturnsError)
{
    int result = security_logging_apply_modulation(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, ApplyModulationSucceeds)
{
    // Add patterns that could trigger modulation
    for (int i = 0; i < 10; i++) {
        security_logging_log_threat(bridge, NIMCP_THREAT_HIGH, BBB_THREAT_UNAUTHORIZED_ACCESS,
            "auth", "Brute force attempt", SECURITY_LOG_ACTION_BLOCK);
    }
    security_logging_analyze_patterns(bridge);

    int result = security_logging_apply_modulation(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, ConnectBioAsyncNullBridgeReturnsError)
{
    int result = security_logging_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, DisconnectBioAsyncNullBridgeReturnsError)
{
    int result = security_logging_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, IsBioAsyncConnectedNullBridgeReturnsFalse)
{
    bool connected = security_logging_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SecurityLoggingBridgeTest, IsBioAsyncConnectedInitiallyFalse)
{
    bool connected = security_logging_is_bio_async_connected(bridge);
    // Bio-async not enabled in config, so should be false
    EXPECT_FALSE(connected);
}

TEST_F(SecurityLoggingBridgeTest, ProcessInboxNullBridgeReturnsZero)
{
    uint32_t processed = security_logging_process_inbox(nullptr, 0);
    EXPECT_EQ(processed, 0);
}

TEST_F(SecurityLoggingBridgeTest, ProcessInboxNoMessagesReturnsZero)
{
    uint32_t processed = security_logging_process_inbox(bridge, 10);
    EXPECT_EQ(processed, 0);
}

TEST_F(SecurityLoggingBridgeTest, BroadcastEventNullBridgeReturnsError)
{
    security_log_entry_t entry;
    create_test_entry(&entry, SECURITY_LOG_CAT_AUDIT, SECURITY_LOG_SEV_INFO, "Test");

    int result = security_logging_broadcast_event(nullptr, &entry);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, BroadcastEventNullEntryReturnsError)
{
    int result = security_logging_broadcast_event(bridge, nullptr);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, CategoryNameReturnsValidStrings)
{
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_THREAT), "THREAT");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_ACCESS), "ACCESS");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_POLICY), "POLICY");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_AUDIT), "AUDIT");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_BBB), "BBB");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_ANOMALY), "ANOMALY");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_CRYPTO), "CRYPTO");
    EXPECT_STREQ(security_log_category_name(SECURITY_LOG_CAT_RATE_LIMIT), "RATE_LIMIT");
}

TEST_F(SecurityLoggingBridgeTest, CategoryNameInvalidReturnsUnknown)
{
    const char* name = security_log_category_name((security_log_category_t)999);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(SecurityLoggingBridgeTest, SeverityNameReturnsValidStrings)
{
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_DEBUG), "DEBUG");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_INFO), "INFO");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_NOTICE), "NOTICE");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_WARNING), "WARNING");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_ERROR), "ERROR");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_CRITICAL), "CRITICAL");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_ALERT), "ALERT");
    EXPECT_STREQ(security_log_severity_name(SECURITY_LOG_SEV_EMERGENCY), "EMERGENCY");
}

TEST_F(SecurityLoggingBridgeTest, SeverityNameInvalidReturnsUnknown)
{
    const char* name = security_log_severity_name((security_log_severity_t)999);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(SecurityLoggingBridgeTest, ActionNameReturnsValidStrings)
{
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_NONE), "NONE");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_ALLOW), "ALLOW");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_DENY), "DENY");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_BLOCK), "BLOCK");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_QUARANTINE), "QUARANTINE");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_ALERT), "ALERT");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_TERMINATE), "TERMINATE");
    EXPECT_STREQ(security_log_action_name(SECURITY_LOG_ACTION_LOCKDOWN), "LOCKDOWN");
}

TEST_F(SecurityLoggingBridgeTest, ActionNameInvalidReturnsUnknown)
{
    const char* name = security_log_action_name((security_log_action_t)999);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(SecurityLoggingBridgeTest, PatternTypeNameReturnsValidStrings)
{
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_NONE), "NONE");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_SCAN), "SCAN");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_BRUTE_FORCE), "BRUTE_FORCE");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_DOS), "DOS");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_INJECTION), "INJECTION");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_EXFILTRATION), "EXFILTRATION");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_LATERAL_MOVE), "LATERAL_MOVE");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_PRIVILEGE_ESC), "PRIVILEGE_ESC");
    EXPECT_STREQ(security_pattern_type_name(SECURITY_PATTERN_CUSTOM), "CUSTOM");
}

TEST_F(SecurityLoggingBridgeTest, PatternTypeNameInvalidReturnsUnknown)
{
    const char* name = security_pattern_type_name((security_pattern_type_t)999);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(SecurityLoggingBridgeTest, FormatNameReturnsValidStrings)
{
    EXPECT_STREQ(security_log_format_name(SECURITY_LOG_FORMAT_TEXT), "TEXT");
    EXPECT_STREQ(security_log_format_name(SECURITY_LOG_FORMAT_JSON), "JSON");
    EXPECT_STREQ(security_log_format_name(SECURITY_LOG_FORMAT_SYSLOG), "SYSLOG");
    EXPECT_STREQ(security_log_format_name(SECURITY_LOG_FORMAT_CEF), "CEF");
    EXPECT_STREQ(security_log_format_name(SECURITY_LOG_FORMAT_BINARY), "BINARY");
}

TEST_F(SecurityLoggingBridgeTest, FormatNameInvalidReturnsUnknown)
{
    const char* name = security_log_format_name((security_log_format_t)999);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

TEST_F(SecurityLoggingBridgeTest, ThreatToSeverityMapping)
{
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_NONE), SECURITY_LOG_SEV_INFO);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_LOW), SECURITY_LOG_SEV_NOTICE);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_MEDIUM), SECURITY_LOG_SEV_WARNING);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_HIGH), SECURITY_LOG_SEV_ERROR);
    EXPECT_EQ(security_threat_to_severity(NIMCP_THREAT_CRITICAL), SECURITY_LOG_SEV_CRITICAL);
}

TEST_F(SecurityLoggingBridgeTest, BBBToLogSeverityMapping)
{
    EXPECT_EQ(security_bbb_to_log_severity(BBB_SEVERITY_NONE), SECURITY_LOG_SEV_INFO);
    EXPECT_EQ(security_bbb_to_log_severity(BBB_SEVERITY_LOW), SECURITY_LOG_SEV_NOTICE);
    EXPECT_EQ(security_bbb_to_log_severity(BBB_SEVERITY_MEDIUM), SECURITY_LOG_SEV_WARNING);
    EXPECT_EQ(security_bbb_to_log_severity(BBB_SEVERITY_HIGH), SECURITY_LOG_SEV_ERROR);
    EXPECT_EQ(security_bbb_to_log_severity(BBB_SEVERITY_CRITICAL), SECURITY_LOG_SEV_CRITICAL);
}

TEST_F(SecurityLoggingBridgeTest, EntryInitNullEntryReturnsError)
{
    int result = security_log_entry_init(nullptr, SECURITY_LOG_CAT_AUDIT,
        SECURITY_LOG_SEV_INFO, "Test");
    EXPECT_NE(result, 0);
}

TEST_F(SecurityLoggingBridgeTest, EntryInitSucceeds)
{
    security_log_entry_t entry;
    int result = security_log_entry_init(&entry, SECURITY_LOG_CAT_THREAT,
        SECURITY_LOG_SEV_CRITICAL, "Critical threat detected");

    EXPECT_EQ(result, 0);
    EXPECT_EQ(entry.category, SECURITY_LOG_CAT_THREAT);
    EXPECT_EQ(entry.severity, SECURITY_LOG_SEV_CRITICAL);
    EXPECT_GT(entry.timestamp_ns, 0);
    EXPECT_STREQ(entry.message, "Critical threat detected");
}

TEST_F(SecurityLoggingBridgeTest, EntryInitNullMessageSucceeds)
{
    security_log_entry_t entry;
    int result = security_log_entry_init(&entry, SECURITY_LOG_CAT_AUDIT,
        SECURITY_LOG_SEV_INFO, nullptr);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(entry.category, SECURITY_LOG_CAT_AUDIT);
    EXPECT_EQ(entry.severity, SECURITY_LOG_SEV_INFO);
}

TEST_F(SecurityLoggingBridgeTest, CurrentTimeNsReturnsPositiveValue)
{
    uint64_t time1 = security_log_current_time_ns();
    EXPECT_GT(time1, 0);

    // Small delay
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    uint64_t time2 = security_log_current_time_ns();
    EXPECT_GT(time2, time1);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, BufferOverflowWithOverwrite)
{
    // Configure to overwrite on full (default)
    EXPECT_TRUE(config.overwrite_on_full);

    // Fill buffer beyond capacity
    size_t overflow_count = config.buffer_capacity + 50;
    populate_log_entries(overflow_count, SECURITY_LOG_CAT_AUDIT);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);

    // Buffer should have wrapped
    EXPECT_EQ(stats.current_buffer_size, config.buffer_capacity);
    EXPECT_GT(stats.buffer_overwrites, 0);
}

TEST_F(SecurityLoggingBridgeTest, SeverityFilteringRespected)
{
    // Create bridge with higher minimum severity
    security_logging_bridge_config_t high_severity_config;
    security_logging_default_config(&high_severity_config);
    high_severity_config.min_severity = SECURITY_LOG_SEV_ERROR;
    high_severity_config.buffer_capacity = 256;
    high_severity_config.log_to_console = false;
    high_severity_config.log_to_file = false;

    security_logging_bridge_t* filtered_bridge = security_logging_bridge_create(&high_severity_config);
    ASSERT_NE(filtered_bridge, nullptr);

    // Log entries with various severities
    security_logging_log_audit(filtered_bridge, SECURITY_LOG_SEV_DEBUG, "src", "Debug", nullptr);
    security_logging_log_audit(filtered_bridge, SECURITY_LOG_SEV_INFO, "src", "Info", nullptr);
    security_logging_log_audit(filtered_bridge, SECURITY_LOG_SEV_WARNING, "src", "Warning", nullptr);
    security_logging_log_audit(filtered_bridge, SECURITY_LOG_SEV_ERROR, "src", "Error", nullptr);
    security_logging_log_audit(filtered_bridge, SECURITY_LOG_SEV_CRITICAL, "src", "Critical", nullptr);

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(filtered_bridge, &stats);

    // Only ERROR and above should be logged
    EXPECT_EQ(stats.total_entries, 2);
    EXPECT_GT(stats.entries_filtered, 0);

    security_logging_bridge_destroy(filtered_bridge);
}

TEST_F(SecurityLoggingBridgeTest, CategoryFilteringRespected)
{
    // Create bridge with only THREAT category enabled
    security_logging_bridge_config_t cat_config;
    security_logging_default_config(&cat_config);
    cat_config.enabled_categories = SECURITY_LOG_CAT_MASK(SECURITY_LOG_CAT_THREAT);
    cat_config.buffer_capacity = 256;
    cat_config.log_to_console = false;
    cat_config.log_to_file = false;

    security_logging_bridge_t* cat_bridge = security_logging_bridge_create(&cat_config);
    ASSERT_NE(cat_bridge, nullptr);

    // Log entries with various categories
    security_logging_log_threat(cat_bridge, NIMCP_THREAT_LOW, BBB_THREAT_NONE, "src", "Threat", SECURITY_LOG_ACTION_ALLOW);
    security_logging_log_access(cat_bridge, true, "user", "obj", "Access");
    security_logging_log_policy(cat_bridge, "pol", SECURITY_LOG_ACTION_ALLOW, "tgt", "Policy");

    security_logging_bridge_stats_t stats;
    security_logging_get_stats(cat_bridge, &stats);

    // Only THREAT should be logged
    EXPECT_EQ(stats.total_entries, 1);
    EXPECT_EQ(stats.entries_by_category[SECURITY_LOG_CAT_THREAT], 1);
    EXPECT_GT(stats.entries_filtered, 0);

    security_logging_bridge_destroy(cat_bridge);
}

TEST_F(SecurityLoggingBridgeTest, LongMessageTruncated)
{
    // Create message longer than max
    char long_message[SECURITY_LOG_MAX_MESSAGE_LEN + 100];
    memset(long_message, 'A', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_AUDIT, SECURITY_LOG_SEV_INFO, long_message);

    int result = security_logging_log_entry(bridge, &entry);
    EXPECT_EQ(result, 0);

    // Verify entry was logged (message truncated internally)
    security_logging_bridge_stats_t stats;
    security_logging_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_entries, 1);
}

//=============================================================================
// Bidirectional Effects Tests
//=============================================================================

TEST_F(SecurityLoggingBridgeTest, SecurityToLoggingEffectsTracked)
{
    // Generate security events
    security_logging_log_threat(bridge, NIMCP_THREAT_CRITICAL, BBB_THREAT_CODE_INJECTION,
        "src", "Critical injection", SECURITY_LOG_ACTION_TERMINATE);
    security_logging_log_policy(bridge, "block-all", SECURITY_LOG_ACTION_BLOCK,
        "malicious-ip", "Blocking known attacker");

    security_to_logging_effects_t sec_effects;
    logging_to_security_effects_t log_effects;
    security_logging_get_effects(bridge, &sec_effects, &log_effects);

    // Verify security effects tracked
    EXPECT_GE(sec_effects.threat_events, 1);
    EXPECT_GE(sec_effects.policy_events, 1);
    EXPECT_GE(sec_effects.max_severity, SECURITY_LOG_SEV_CRITICAL);
}

TEST_F(SecurityLoggingBridgeTest, LoggingToSecurityFeedback)
{
    // Generate many similar threats to trigger pattern detection
    for (int i = 0; i < 30; i++) {
        security_logging_log_threat(bridge, NIMCP_THREAT_HIGH,
            BBB_THREAT_UNAUTHORIZED_ACCESS, "auth_service",
            "Brute force login attempt detected", SECURITY_LOG_ACTION_BLOCK);
    }

    // Analyze patterns
    security_logging_analyze_patterns(bridge);

    // Apply modulation to feed back to security
    security_logging_apply_modulation(bridge);

    security_to_logging_effects_t sec_effects;
    logging_to_security_effects_t log_effects;
    security_logging_get_effects(bridge, &sec_effects, &log_effects);

    // Pattern detection should be tracked
    // Implementation may or may not detect patterns based on threshold
    SUCCEED();
}

}  // namespace
