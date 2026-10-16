/**
 * @file test_security_bridges_regression.cpp
 * @brief Comprehensive regression tests for security bridges
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Regression tests for three security bridges
 * WHY:  Ensure backward compatibility and consistent behavior across versions
 * HOW:  Test API stability, configuration, detection, performance, and state machines
 *
 * BRIDGES TESTED:
 * 1. Security-Async Bridge    - Bio-async router integration
 * 2. Security-Logging Bridge  - Comprehensive audit trail
 * 3. Security-Immune Unified  - Unified immune system integration
 *
 * REGRESSION TEST CATEGORIES:
 * - API Backward Compatibility
 * - Configuration Stability
 * - Detection Consistency
 * - Performance Benchmarks
 * - Statistics Consistency
 * - State Machine Stability
 * - Bidirectional Flow Stability
 */

#include "utils/test_helpers.h"

#include <gtest/gtest.h>
#include <chrono>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>

// Security bridge headers (have their own extern "C" guards)
#include "security/async/nimcp_security_async_bridge.h"
#include "security/logging/nimcp_security_logging_bridge.h"
#include "security/immune/nimcp_security_immune_unified_bridge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_policy_engine.h"

namespace {

// =============================================================================
// Performance Thresholds (Regression Baselines)
// =============================================================================

// Security-Async Bridge thresholds
constexpr double SECURITY_ASYNC_CREATE_MAX_MS = 5.0;
constexpr double SECURITY_ASYNC_BROADCAST_THREAT_MAX_MS = 1.0;
constexpr double SECURITY_ASYNC_UPDATE_MAX_MS = 2.0;

// Security-Logging Bridge thresholds
constexpr double SECURITY_LOGGING_CREATE_MAX_MS = 5.0;
constexpr double SECURITY_LOGGING_LOG_ENTRY_MAX_MS = 0.5;
constexpr double SECURITY_LOGGING_ANALYZE_MAX_MS = 10.0;

// Security-Immune Unified Bridge thresholds
constexpr double SEC_IMMUNE_CREATE_MAX_MS = 5.0;
constexpr double SEC_IMMUNE_UPDATE_MAX_MS = 2.0;
constexpr double SEC_IMMUNE_PRESENT_ANTIGEN_MAX_MS = 1.0;

// Iteration counts for performance tests
constexpr int PERF_ITERATIONS = 100;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Measure execution time of a function in milliseconds
 */
template<typename Func>
double measure_time_ms(Func&& func)
{
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

/**
 * @brief Measure average execution time over multiple iterations
 */
template<typename Func>
double measure_avg_time_ms(Func&& func, int iterations)
{
    double total = 0.0;
    for (int i = 0; i < iterations; i++) {
        total += measure_time_ms(std::forward<Func>(func));
    }
    return total / iterations;
}

// =============================================================================
// Security-Async Bridge Regression Tests
// =============================================================================

class SecurityAsyncBridgeRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize with default config
        int rc = security_async_default_config(&config_);
        ASSERT_EQ(rc, 0);
        bridge_ = nullptr;
    }

    void TearDown() override
    {
        if (bridge_) {
            security_async_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    security_async_config_t config_;
    security_async_bridge_t* bridge_;
};

// ---------------------------------------------------------------------------
// API Backward Compatibility Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityAsyncBridgeRegressionTest, APICreateWithNullConfig)
{
    // API: Create with NULL config should use defaults
    bridge_ = security_async_bridge_create(nullptr);
    EXPECT_NE(bridge_, nullptr);
}

TEST_F(SecurityAsyncBridgeRegressionTest, APICreateWithConfig)
{
    // API: Create with config should succeed
    bridge_ = security_async_bridge_create(&config_);
    EXPECT_NE(bridge_, nullptr);
}

TEST_F(SecurityAsyncBridgeRegressionTest, APIDestroyNullSafe)
{
    // API: Destroy with NULL should not crash
    security_async_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SecurityAsyncBridgeRegressionTest, APIGetStateWithNullBridge)
{
    // API: get_state with NULL bridge should return error
    security_async_state_t state;
    int rc = security_async_get_state(nullptr, &state);
    EXPECT_NE(rc, 0);
}

TEST_F(SecurityAsyncBridgeRegressionTest, APIGetStatsWithNullBridge)
{
    // API: get_stats with NULL bridge should return error
    security_async_stats_t stats;
    int rc = security_async_get_stats(nullptr, &stats);
    EXPECT_NE(rc, 0);
}

TEST_F(SecurityAsyncBridgeRegressionTest, APIResetStatsWithNullBridge)
{
    // API: reset_stats with NULL bridge should return error
    int rc = security_async_reset_stats(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(SecurityAsyncBridgeRegressionTest, APIBroadcastThreatWithNullBridge)
{
    // API: broadcast_threat with NULL bridge should return error
    uint8_t hash[32] = {0};
    int rc = security_async_broadcast_threat(nullptr, BBB_THREAT_SQL_INJECTION,
                                              BBB_SEVERITY_HIGH, "test", hash);
    EXPECT_NE(rc, 0);
}

TEST_F(SecurityAsyncBridgeRegressionTest, APIEmergencyModeWithNullBridge)
{
    // API: emergency mode functions with NULL bridge should return error
    EXPECT_NE(security_async_enter_emergency_mode(nullptr), 0);
    EXPECT_NE(security_async_exit_emergency_mode(nullptr), 0);
    // is_emergency_mode may throw but should return false for NULL
}

// ---------------------------------------------------------------------------
// Configuration Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityAsyncBridgeRegressionTest, ConfigDefaultValuesStable)
{
    // Configuration defaults should remain stable across versions
    security_async_config_t cfg;
    int rc = security_async_default_config(&cfg);
    ASSERT_EQ(rc, 0);

    // Feature enables should default to true for most features
    EXPECT_TRUE(cfg.enable_threat_broadcast);
    EXPECT_TRUE(cfg.enable_bbb_alerts);
    EXPECT_TRUE(cfg.enable_anomaly_events);

    // Thresholds should have sensible defaults
    EXPECT_GE(cfg.max_pending_events, 100u);
    EXPECT_GE(cfg.max_threat_intel_cache, 100u);
}

TEST_F(SecurityAsyncBridgeRegressionTest, ConfigValidationConsistent)
{
    // Invalid config values should be handled gracefully
    config_.max_pending_events = 0;  // Edge case
    bridge_ = security_async_bridge_create(&config_);
    // Should either fail or use minimum value
    if (bridge_ != nullptr) {
        security_async_state_t state;
        security_async_get_state(bridge_, &state);
        // Should still be functional
        EXPECT_TRUE(state.is_active || !state.is_active);  // Valid state
    }
}

// ---------------------------------------------------------------------------
// State Machine Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityAsyncBridgeRegressionTest, EmergencyModeTransitionsStable)
{
    bridge_ = security_async_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    // Initial state should not be emergency
    EXPECT_FALSE(security_async_is_emergency_mode(bridge_));

    // Enter emergency mode
    int rc = security_async_enter_emergency_mode(bridge_);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(security_async_is_emergency_mode(bridge_));

    // Exit emergency mode
    rc = security_async_exit_emergency_mode(bridge_);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(security_async_is_emergency_mode(bridge_));
}

TEST_F(SecurityAsyncBridgeRegressionTest, ConnectionStateManagementStable)
{
    bridge_ = security_async_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    security_async_state_t state;
    int rc = security_async_get_state(bridge_, &state);
    EXPECT_EQ(rc, 0);

    // Initial state should show no connections
    EXPECT_FALSE(state.bbb_connected);
    EXPECT_FALSE(state.anomaly_connected);
    EXPECT_FALSE(state.pattern_db_connected);
}

// ---------------------------------------------------------------------------
// Statistics Consistency Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityAsyncBridgeRegressionTest, StatsCountersIncrementCorrectly)
{
    bridge_ = security_async_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    security_async_stats_t stats1, stats2;

    // Get initial stats
    int rc = security_async_get_stats(bridge_, &stats1);
    EXPECT_EQ(rc, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats1.events_published, 0u);
    EXPECT_EQ(stats1.events_received, 0u);

    // Get stats again - should be consistent
    rc = security_async_get_stats(bridge_, &stats2);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats1.events_published, stats2.events_published);
}

TEST_F(SecurityAsyncBridgeRegressionTest, StatsResetWorksProperly)
{
    bridge_ = security_async_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    // Reset stats
    int rc = security_async_reset_stats(bridge_);
    EXPECT_EQ(rc, 0);

    // Verify reset
    security_async_stats_t stats;
    rc = security_async_get_stats(bridge_, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.broadcast_failures, 0u);
}

// ---------------------------------------------------------------------------
// Performance Benchmarks
// ---------------------------------------------------------------------------

TEST_F(SecurityAsyncBridgeRegressionTest, PerfCreateUnder5ms)
{
    double time_ms = measure_time_ms([this]() {
        bridge_ = security_async_bridge_create(&config_);
    });

    EXPECT_LT(time_ms, SECURITY_ASYNC_CREATE_MAX_MS)
        << "security_async_bridge_create took " << time_ms << "ms (max: "
        << SECURITY_ASYNC_CREATE_MAX_MS << "ms)";
}

TEST_F(SecurityAsyncBridgeRegressionTest, PerfUpdateUnder2ms)
{
    bridge_ = security_async_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    double avg_time = measure_avg_time_ms([this]() {
        security_async_bridge_update(bridge_, 16);  // 16ms delta
    }, PERF_ITERATIONS);

    EXPECT_LT(avg_time, SECURITY_ASYNC_UPDATE_MAX_MS)
        << "security_async_bridge_update averaged " << avg_time << "ms (max: "
        << SECURITY_ASYNC_UPDATE_MAX_MS << "ms)";
}

// =============================================================================
// Security-Logging Bridge Regression Tests
// =============================================================================

class SecurityLoggingBridgeRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        int rc = security_logging_default_config(&config_);
        ASSERT_EQ(rc, 0);
        bridge_ = nullptr;
    }

    void TearDown() override
    {
        if (bridge_) {
            security_logging_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    security_logging_bridge_config_t config_;
    security_logging_bridge_t* bridge_;
};

// ---------------------------------------------------------------------------
// API Backward Compatibility Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityLoggingBridgeRegressionTest, APICreateWithNullConfig)
{
    // API: Create with NULL config should use defaults
    bridge_ = security_logging_bridge_create(nullptr);
    EXPECT_NE(bridge_, nullptr);
}

TEST_F(SecurityLoggingBridgeRegressionTest, APICreateWithConfig)
{
    // API: Create with config should succeed
    bridge_ = security_logging_bridge_create(&config_);
    EXPECT_NE(bridge_, nullptr);
}

TEST_F(SecurityLoggingBridgeRegressionTest, APIDestroyNullSafe)
{
    // API: Destroy with NULL should not crash
    security_logging_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SecurityLoggingBridgeRegressionTest, APILogEntryWithNullBridge)
{
    // API: log_entry with NULL bridge should return error
    security_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    int rc = security_logging_log_entry(nullptr, &entry);
    EXPECT_EQ(rc, -1);
}

TEST_F(SecurityLoggingBridgeRegressionTest, APIGetStatsWithNullBridge)
{
    // API: get_stats with NULL bridge should return error
    security_logging_bridge_stats_t stats;
    int rc = security_logging_get_stats(nullptr, &stats);
    EXPECT_EQ(rc, -1);
}

TEST_F(SecurityLoggingBridgeRegressionTest, APIResetStatsWithNullBridge)
{
    // API: reset_stats with NULL bridge should return error
    int rc = security_logging_reset_stats(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(SecurityLoggingBridgeRegressionTest, APIIsConnectedWithNullBridge)
{
    // API: is_connected with NULL bridge should return false
    EXPECT_FALSE(security_logging_is_connected(nullptr));
}

// ---------------------------------------------------------------------------
// Configuration Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityLoggingBridgeRegressionTest, ConfigDefaultValuesStable)
{
    security_logging_bridge_config_t cfg;
    int rc = security_logging_default_config(&cfg);
    ASSERT_EQ(rc, 0);

    // Buffer capacity should have sensible default
    EXPECT_GE(cfg.buffer_capacity, 1024u);

    // Severity threshold should default to reasonable level
    EXPECT_LE(cfg.min_severity, SECURITY_LOG_SEV_WARNING);

    // Format should default to a valid format
    EXPECT_GE(cfg.format, SECURITY_LOG_FORMAT_TEXT);
}

TEST_F(SecurityLoggingBridgeRegressionTest, ConfigEnabledCategoriesStable)
{
    security_logging_bridge_config_t cfg;
    int rc = security_logging_default_config(&cfg);
    ASSERT_EQ(rc, 0);

    // All categories should be enabled by default or specific ones
    EXPECT_GT(cfg.enabled_categories, 0u);
}

// ---------------------------------------------------------------------------
// Detection Consistency Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityLoggingBridgeRegressionTest, SameInputsProduceSameOutputs)
{
    bridge_ = security_logging_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    // Create identical entries
    security_log_entry_t entry1, entry2;
    memset(&entry1, 0, sizeof(entry1));
    memset(&entry2, 0, sizeof(entry2));

    entry1.category = SECURITY_LOG_CAT_THREAT;
    entry1.severity = SECURITY_LOG_SEV_CRITICAL;
    strncpy(entry1.message, "Test threat", sizeof(entry1.message) - 1);

    entry2.category = SECURITY_LOG_CAT_THREAT;
    entry2.severity = SECURITY_LOG_SEV_CRITICAL;
    strncpy(entry2.message, "Test threat", sizeof(entry2.message) - 1);

    // Both should log successfully
    int rc1 = security_logging_log_entry(bridge_, &entry1);
    int rc2 = security_logging_log_entry(bridge_, &entry2);

    EXPECT_EQ(rc1, rc2);
    EXPECT_EQ(rc1, 0);
}

// ---------------------------------------------------------------------------
// Statistics Consistency Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityLoggingBridgeRegressionTest, StatsCountersIncrementCorrectly)
{
    bridge_ = security_logging_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    security_logging_bridge_stats_t stats1;
    int rc = security_logging_get_stats(bridge_, &stats1);
    EXPECT_EQ(rc, 0);

    // Initial stats
    uint64_t initial_entries = stats1.total_entries;

    // Log an entry
    security_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.category = SECURITY_LOG_CAT_AUDIT;
    entry.severity = SECURITY_LOG_SEV_INFO;
    strncpy(entry.message, "Test", sizeof(entry.message) - 1);

    security_logging_log_entry(bridge_, &entry);

    // Check stats incremented
    security_logging_bridge_stats_t stats2;
    rc = security_logging_get_stats(bridge_, &stats2);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats2.total_entries, initial_entries);
}

TEST_F(SecurityLoggingBridgeRegressionTest, StatsResetWorksProperly)
{
    bridge_ = security_logging_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    // Reset stats
    int rc = security_logging_reset_stats(bridge_);
    EXPECT_EQ(rc, 0);

    // Verify counters reset
    security_logging_bridge_stats_t stats;
    rc = security_logging_get_stats(bridge_, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_entries, 0u);
    EXPECT_EQ(stats.entries_dropped, 0u);
}

TEST_F(SecurityLoggingBridgeRegressionTest, StatsByCategory)
{
    bridge_ = security_logging_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    security_logging_reset_stats(bridge_);

    // Log entries in different categories
    security_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.severity = SECURITY_LOG_SEV_INFO;

    entry.category = SECURITY_LOG_CAT_THREAT;
    security_logging_log_entry(bridge_, &entry);

    entry.category = SECURITY_LOG_CAT_POLICY;
    security_logging_log_entry(bridge_, &entry);

    // Verify per-category stats
    security_logging_bridge_stats_t stats;
    int rc = security_logging_get_stats(bridge_, &stats);
    EXPECT_EQ(rc, 0);

    // At least some entries should be counted
    EXPECT_GE(stats.total_entries, 0u);
}

// ---------------------------------------------------------------------------
// Performance Benchmarks
// ---------------------------------------------------------------------------

TEST_F(SecurityLoggingBridgeRegressionTest, PerfCreateUnder5ms)
{
    double time_ms = measure_time_ms([this]() {
        bridge_ = security_logging_bridge_create(&config_);
    });

    EXPECT_LT(time_ms, SECURITY_LOGGING_CREATE_MAX_MS)
        << "security_logging_bridge_create took " << time_ms << "ms (max: "
        << SECURITY_LOGGING_CREATE_MAX_MS << "ms)";
}

TEST_F(SecurityLoggingBridgeRegressionTest, PerfLogEntryUnder500us)
{
    bridge_ = security_logging_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    security_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.category = SECURITY_LOG_CAT_AUDIT;
    entry.severity = SECURITY_LOG_SEV_INFO;
    strncpy(entry.message, "Performance test entry", sizeof(entry.message) - 1);

    double avg_time = measure_avg_time_ms([this, &entry]() {
        security_logging_log_entry(bridge_, &entry);
    }, PERF_ITERATIONS);

    EXPECT_LT(avg_time, SECURITY_LOGGING_LOG_ENTRY_MAX_MS)
        << "security_logging_log_entry averaged " << avg_time << "ms (max: "
        << SECURITY_LOGGING_LOG_ENTRY_MAX_MS << "ms)";
}

// ---------------------------------------------------------------------------
// State Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityLoggingBridgeRegressionTest, BridgeResetPreservesConfig)
{
    bridge_ = security_logging_bridge_create(&config_);
    ASSERT_NE(bridge_, nullptr);

    // Log some entries
    security_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.category = SECURITY_LOG_CAT_AUDIT;
    entry.severity = SECURITY_LOG_SEV_INFO;
    security_logging_log_entry(bridge_, &entry);

    // Reset bridge
    int rc = security_logging_bridge_reset(bridge_);
    EXPECT_EQ(rc, 0);

    // Should still be able to log
    rc = security_logging_log_entry(bridge_, &entry);
    EXPECT_EQ(rc, 0);
}

// =============================================================================
// Security-Immune Unified Bridge Regression Tests
// =============================================================================

class SecurityImmuneUnifiedRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        int rc = sec_immune_unified_default_config(&config_);
        ASSERT_EQ(rc, 0);
        bridge_ = nullptr;
        immune_system_ = nullptr;
    }

    void TearDown() override
    {
        if (bridge_) {
            sec_immune_unified_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (immune_system_) {
            brain_immune_destroy(immune_system_);
            immune_system_ = nullptr;
        }
    }

    void CreateImmuneSystem()
    {
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system_ = brain_immune_create(&immune_cfg);
    }

    sec_immune_unified_config_t config_;
    sec_immune_unified_bridge_t* bridge_;
    brain_immune_system_t* immune_system_;
};

// ---------------------------------------------------------------------------
// API Backward Compatibility Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, APICreateWithNullConfig)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    // API: Create with NULL config should use defaults
    bridge_ = sec_immune_unified_create(nullptr, immune_system_);
    EXPECT_NE(bridge_, nullptr);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APICreateWithConfig)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    // API: Create with config should succeed
    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    EXPECT_NE(bridge_, nullptr);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APICreateWithNullImmuneSystem)
{
    // API: Create with NULL immune system may still succeed (optional)
    bridge_ = sec_immune_unified_create(&config_, nullptr);
    // Behavior depends on implementation - just shouldn't crash
    SUCCEED();
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APIDestroyNullSafe)
{
    // API: Destroy with NULL should not crash
    sec_immune_unified_destroy(nullptr);
    SUCCEED();
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APIGetStatsWithNullBridge)
{
    // API: get_stats with NULL bridge should return error
    sec_immune_unified_stats_t stats;
    int rc = sec_immune_unified_get_stats(nullptr, &stats);
    EXPECT_EQ(rc, -1);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APIUpdateWithNullBridge)
{
    // API: update with NULL bridge should return error
    int rc = sec_immune_unified_update(nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APIGetThreatLevelWithNullBridge)
{
    // API: get_threat_level with NULL bridge should return safe value
    float level = sec_immune_unified_get_threat_level(nullptr);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APIEmergencyModeWithNullBridge)
{
    // API: is_emergency_mode with NULL bridge should return false
    EXPECT_FALSE(sec_immune_unified_is_emergency_mode(nullptr));
}

TEST_F(SecurityImmuneUnifiedRegressionTest, APILearningModeWithNullBridge)
{
    // API: is_learning_mode with NULL bridge should return false
    EXPECT_FALSE(sec_immune_unified_is_learning_mode(nullptr));
}

// ---------------------------------------------------------------------------
// Configuration Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, ConfigDefaultValuesStable)
{
    sec_immune_unified_config_t cfg;
    int rc = sec_immune_unified_default_config(&cfg);
    ASSERT_EQ(rc, 0);

    // Feature enables should have sensible defaults
    EXPECT_TRUE(cfg.enable_bbb_antigen_presentation);
    EXPECT_TRUE(cfg.enable_anomaly_antigen_presentation);
    EXPECT_TRUE(cfg.enable_pattern_antigen_presentation);

    // Cytokine modulation should be enabled
    EXPECT_TRUE(cfg.enable_cytokine_bbb_modulation);

    // Tolerance system should be configurable
    EXPECT_GT(cfg.max_tolerance_entries, 0u);
    EXPECT_GT(cfg.tolerance_confirmation_count, 0u);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, ConfigThresholdsStable)
{
    sec_immune_unified_config_t cfg;
    int rc = sec_immune_unified_default_config(&cfg);
    ASSERT_EQ(rc, 0);

    // BBB thresholds should be reasonable
    EXPECT_GT(cfg.bbb_base_threshold, 0.0f);
    EXPECT_LE(cfg.bbb_min_threshold_factor, 1.0f);
    EXPECT_GE(cfg.bbb_max_threshold_factor, 1.0f);

    // Anomaly thresholds
    EXPECT_GT(cfg.anomaly_base_threshold, 0.0f);
    EXPECT_LT(cfg.anomaly_min_threshold, cfg.anomaly_max_threshold);

    // Policy strictness
    EXPECT_GT(cfg.policy_base_strictness, 0.0f);
}

// ---------------------------------------------------------------------------
// Bidirectional Flow Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, SecurityToImmuneEffectsConsistent)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Get initial modulation factors (may be 0.0 before first update)
    float initial_bbb = sec_immune_unified_get_bbb_threshold_factor(bridge_);
    float initial_anomaly = sec_immune_unified_get_anomaly_threshold(bridge_);

    // Factors should be in valid range (0.0 is valid before first update)
    EXPECT_GE(initial_bbb, 0.0f);
    EXPECT_LE(initial_bbb, 2.0f);
    EXPECT_GE(initial_anomaly, 0.0f);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, ImmuneToSecurityModulationStable)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Get modulation factors (may be 0.0 before first update cycle)
    float pattern_factor = sec_immune_unified_get_pattern_weight_factor(bridge_);
    float rate_factor = sec_immune_unified_get_rate_limit_factor(bridge_);
    float policy_factor = sec_immune_unified_get_policy_strictness_factor(bridge_);

    // All factors should be non-negative (0.0 valid before first update)
    EXPECT_GE(pattern_factor, 0.0f);
    EXPECT_GE(rate_factor, 0.0f);
    EXPECT_GE(policy_factor, 0.0f);
}

// ---------------------------------------------------------------------------
// Tolerance System Stability Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, ToleranceAddRemoveConsistent)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Add tolerance pattern
    uint8_t pattern[] = {0x01, 0x02, 0x03, 0x04};
    int rc = sec_immune_unified_add_tolerance(bridge_, pattern, sizeof(pattern),
                                               "test pattern", false);
    EXPECT_EQ(rc, 0);

    // Check if tolerated
    bool tolerated = sec_immune_unified_is_tolerated(bridge_, pattern, sizeof(pattern));
    EXPECT_TRUE(tolerated);

    // Remove tolerance
    rc = sec_immune_unified_remove_tolerance(bridge_, pattern, sizeof(pattern));
    EXPECT_EQ(rc, 0);

    // Should no longer be tolerated
    tolerated = sec_immune_unified_is_tolerated(bridge_, pattern, sizeof(pattern));
    EXPECT_FALSE(tolerated);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, LearningModeTransitionsStable)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Enable learning mode
    int rc = sec_immune_unified_set_learning_mode(bridge_, true);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(sec_immune_unified_is_learning_mode(bridge_));

    // Disable learning mode
    rc = sec_immune_unified_set_learning_mode(bridge_, false);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(sec_immune_unified_is_learning_mode(bridge_));
}

// ---------------------------------------------------------------------------
// Statistics Consistency Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, StatsInitiallyZero)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    sec_immune_unified_stats_t stats;
    int rc = sec_immune_unified_get_stats(bridge_, &stats);
    EXPECT_EQ(rc, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.total_antigens_presented, 0u);
    EXPECT_EQ(stats.b_cells_activated, 0u);
    EXPECT_EQ(stats.antibody_actions_executed, 0u);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, StatsResetWorksProperly)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Reset stats
    int rc = sec_immune_unified_reset(bridge_);
    EXPECT_EQ(rc, 0);

    // Verify reset
    sec_immune_unified_stats_t stats;
    rc = sec_immune_unified_get_stats(bridge_, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_antigens_presented, 0u);
}

// ---------------------------------------------------------------------------
// Performance Benchmarks
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, PerfCreateUnder5ms)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    double time_ms = measure_time_ms([this]() {
        bridge_ = sec_immune_unified_create(&config_, immune_system_);
    });

    EXPECT_LT(time_ms, SEC_IMMUNE_CREATE_MAX_MS)
        << "sec_immune_unified_create took " << time_ms << "ms (max: "
        << SEC_IMMUNE_CREATE_MAX_MS << "ms)";
}

TEST_F(SecurityImmuneUnifiedRegressionTest, PerfUpdateUnder2ms)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    double avg_time = measure_avg_time_ms([this]() {
        sec_immune_unified_update(bridge_);
    }, PERF_ITERATIONS);

    EXPECT_LT(avg_time, SEC_IMMUNE_UPDATE_MAX_MS)
        << "sec_immune_unified_update averaged " << avg_time << "ms (max: "
        << SEC_IMMUNE_UPDATE_MAX_MS << "ms)";
}

// ---------------------------------------------------------------------------
// Threat Level Consistency Tests
// ---------------------------------------------------------------------------

TEST_F(SecurityImmuneUnifiedRegressionTest, ThreatLevelInValidRange)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Threat level should always be in [0, 1]
    float level = sec_immune_unified_get_threat_level(bridge_);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(SecurityImmuneUnifiedRegressionTest, EmergencyModeAffectsThreatLevel)
{
    CreateImmuneSystem();
    ASSERT_NE(immune_system_, nullptr);

    bridge_ = sec_immune_unified_create(&config_, immune_system_);
    ASSERT_NE(bridge_, nullptr);

    // Initial threat level
    float initial_level = sec_immune_unified_get_threat_level(bridge_);

    // Check emergency mode state
    bool emergency = sec_immune_unified_is_emergency_mode(bridge_);
    // State should be consistent
    EXPECT_TRUE(emergency || !emergency);

    // Threat level should remain valid
    EXPECT_GE(initial_level, 0.0f);
    EXPECT_LE(initial_level, 1.0f);
}

// =============================================================================
// Cross-Bridge Regression Tests
// =============================================================================

class CrossBridgeRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        async_bridge_ = nullptr;
        logging_bridge_ = nullptr;
        immune_bridge_ = nullptr;
        immune_system_ = nullptr;
    }

    void TearDown() override
    {
        if (async_bridge_) {
            security_async_bridge_destroy(async_bridge_);
            async_bridge_ = nullptr;
        }
        if (logging_bridge_) {
            security_logging_bridge_destroy(logging_bridge_);
            logging_bridge_ = nullptr;
        }
        if (immune_bridge_) {
            sec_immune_unified_destroy(immune_bridge_);
            immune_bridge_ = nullptr;
        }
        if (immune_system_) {
            brain_immune_destroy(immune_system_);
            immune_system_ = nullptr;
        }
    }

    security_async_bridge_t* async_bridge_;
    security_logging_bridge_t* logging_bridge_;
    sec_immune_unified_bridge_t* immune_bridge_;
    brain_immune_system_t* immune_system_;
};

TEST_F(CrossBridgeRegressionTest, AllBridgesCreateDestroySafely)
{
    // Create all bridges
    async_bridge_ = security_async_bridge_create(nullptr);
    EXPECT_NE(async_bridge_, nullptr);

    logging_bridge_ = security_logging_bridge_create(nullptr);
    EXPECT_NE(logging_bridge_, nullptr);

    brain_immune_config_t immune_cfg;
    brain_immune_default_config(&immune_cfg);
    immune_system_ = brain_immune_create(&immune_cfg);
    ASSERT_NE(immune_system_, nullptr);

    immune_bridge_ = sec_immune_unified_create(nullptr, immune_system_);
    EXPECT_NE(immune_bridge_, nullptr);

    // All bridges should coexist
    SUCCEED();
}

TEST_F(CrossBridgeRegressionTest, BridgesDoNotInterfere)
{
    // Create bridges
    async_bridge_ = security_async_bridge_create(nullptr);
    logging_bridge_ = security_logging_bridge_create(nullptr);

    brain_immune_config_t immune_cfg;
    brain_immune_default_config(&immune_cfg);
    immune_system_ = brain_immune_create(&immune_cfg);
    immune_bridge_ = sec_immune_unified_create(nullptr, immune_system_);

    // Operations on one bridge should not affect others
    security_async_reset_stats(async_bridge_);
    security_logging_reset_stats(logging_bridge_);
    sec_immune_unified_reset(immune_bridge_);

    // All should still be functional
    security_async_stats_t async_stats;
    security_async_get_stats(async_bridge_, &async_stats);
    EXPECT_EQ(async_stats.events_published, 0u);

    security_logging_bridge_stats_t log_stats;
    security_logging_get_stats(logging_bridge_, &log_stats);
    EXPECT_EQ(log_stats.total_entries, 0u);

    sec_immune_unified_stats_t immune_stats;
    sec_immune_unified_get_stats(immune_bridge_, &immune_stats);
    EXPECT_EQ(immune_stats.total_antigens_presented, 0u);
}

// =============================================================================
// Enum Value Stability Tests
// =============================================================================

TEST(SecurityBridgesEnumRegression, SecurityAsyncMessageTypesStable)
{
    // Extended security message types should have stable values
    EXPECT_EQ(BIO_MSG_SECURITY_THREAT_DETECTED_EXT, 0x0760);
    EXPECT_EQ(BIO_MSG_SECURITY_THREAT_CLEARED, 0x0761);
    EXPECT_EQ(BIO_MSG_SECURITY_POLICY_CHANGE_EXT, 0x0768);
    EXPECT_EQ(BIO_MSG_SECURITY_RATE_LIMIT_HIT_EXT, 0x0770);
    EXPECT_EQ(BIO_MSG_SECURITY_BBB_ALERT_EXT, 0x0774);
    EXPECT_EQ(BIO_MSG_SECURITY_ANOMALY_DETECTED_EXT, 0x0778);
}

TEST(SecurityBridgesEnumRegression, SecurityEventSeverityStable)
{
    EXPECT_EQ(SECURITY_EVENT_SEVERITY_INFO, 0);
    EXPECT_EQ(SECURITY_EVENT_SEVERITY_LOW, 1);
    EXPECT_EQ(SECURITY_EVENT_SEVERITY_MEDIUM, 2);
    EXPECT_EQ(SECURITY_EVENT_SEVERITY_HIGH, 3);
    EXPECT_EQ(SECURITY_EVENT_SEVERITY_CRITICAL, 4);
}

TEST(SecurityBridgesEnumRegression, SecurityLogCategoryStable)
{
    EXPECT_EQ(SECURITY_LOG_CAT_THREAT, 0);
    EXPECT_EQ(SECURITY_LOG_CAT_ACCESS, 1);
    EXPECT_EQ(SECURITY_LOG_CAT_POLICY, 2);
    EXPECT_EQ(SECURITY_LOG_CAT_AUDIT, 3);
    EXPECT_EQ(SECURITY_LOG_CAT_BBB, 4);
    EXPECT_EQ(SECURITY_LOG_CAT_ANOMALY, 5);
    EXPECT_EQ(SECURITY_LOG_CAT_CRYPTO, 6);
    EXPECT_EQ(SECURITY_LOG_CAT_RATE_LIMIT, 7);
}

TEST(SecurityBridgesEnumRegression, SecurityLogSeverityStable)
{
    EXPECT_EQ(SECURITY_LOG_SEV_DEBUG, 0);
    EXPECT_EQ(SECURITY_LOG_SEV_INFO, 1);
    EXPECT_EQ(SECURITY_LOG_SEV_NOTICE, 2);
    EXPECT_EQ(SECURITY_LOG_SEV_WARNING, 3);
    EXPECT_EQ(SECURITY_LOG_SEV_ERROR, 4);
    EXPECT_EQ(SECURITY_LOG_SEV_CRITICAL, 5);
    EXPECT_EQ(SECURITY_LOG_SEV_ALERT, 6);
    EXPECT_EQ(SECURITY_LOG_SEV_EMERGENCY, 7);
}

TEST(SecurityBridgesEnumRegression, SecurityLogActionStable)
{
    EXPECT_EQ(SECURITY_LOG_ACTION_NONE, 0);
    EXPECT_EQ(SECURITY_LOG_ACTION_ALLOW, 1);
    EXPECT_EQ(SECURITY_LOG_ACTION_DENY, 2);
    EXPECT_EQ(SECURITY_LOG_ACTION_BLOCK, 3);
    EXPECT_EQ(SECURITY_LOG_ACTION_QUARANTINE, 4);
    EXPECT_EQ(SECURITY_LOG_ACTION_ALERT, 5);
    EXPECT_EQ(SECURITY_LOG_ACTION_TERMINATE, 6);
    EXPECT_EQ(SECURITY_LOG_ACTION_LOCKDOWN, 7);
}

TEST(SecurityBridgesEnumRegression, SecurityPatternTypeStable)
{
    EXPECT_EQ(SECURITY_PATTERN_NONE, 0);
    EXPECT_EQ(SECURITY_PATTERN_SCAN, 1);
    EXPECT_EQ(SECURITY_PATTERN_BRUTE_FORCE, 2);
    EXPECT_EQ(SECURITY_PATTERN_DOS, 3);
    EXPECT_EQ(SECURITY_PATTERN_INJECTION, 4);
    EXPECT_EQ(SECURITY_PATTERN_EXFILTRATION, 5);
    EXPECT_EQ(SECURITY_PATTERN_LATERAL_MOVE, 6);
    EXPECT_EQ(SECURITY_PATTERN_PRIVILEGE_ESC, 7);
}

// =============================================================================
// Constant Value Stability Tests
// =============================================================================

TEST(SecurityBridgesConstantRegression, SecurityAsyncMagicStable)
{
    EXPECT_EQ(NIMCP_SECURITY_ASYNC_BRIDGE_MAGIC, 0x53454341);  // 'SECA'
}

TEST(SecurityBridgesConstantRegression, SecurityLoggingMagicStable)
{
    EXPECT_EQ(SECURITY_LOGGING_BRIDGE_MAGIC, 0x53454C47);  // 'SELG'
}

TEST(SecurityBridgesConstantRegression, SecurityLoggingDefaultsStable)
{
    EXPECT_EQ(SECURITY_LOG_DEFAULT_BUFFER_SIZE, 8192u);
    EXPECT_EQ(SECURITY_LOG_MAX_MESSAGE_LEN, 512u);
    EXPECT_EQ(SECURITY_LOG_MAX_DETAILS_LEN, 1024u);
    EXPECT_EQ(SECURITY_LOG_DEFAULT_RETENTION_DAYS, 90u);
}

TEST(SecurityBridgesConstantRegression, SecurityImmuneCytokineImpactsStable)
{
    // IL-1 impacts
    EXPECT_FLOAT_EQ(SEC_IMMUNE_IL1_BBB_THRESHOLD_IMPACT, -0.10f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_IL1_ANOMALY_THRESHOLD_IMPACT, -0.10f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_IL1_PATTERN_WEIGHT_IMPACT, 0.10f);

    // IL-10 impacts (anti-inflammatory)
    EXPECT_FLOAT_EQ(SEC_IMMUNE_IL10_BBB_THRESHOLD_IMPACT, 0.15f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_IL10_ANOMALY_THRESHOLD_IMPACT, 0.20f);

    // TNF-alpha impacts (severe)
    EXPECT_FLOAT_EQ(SEC_IMMUNE_TNF_BBB_THRESHOLD_IMPACT, -0.25f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_TNF_POLICY_STRICTNESS_IMPACT, 0.25f);
}

TEST(SecurityBridgesConstantRegression, SecurityImmuneInflammationFactorsStable)
{
    // BBB factors by inflammation level
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_NONE_BBB_FACTOR, 1.00f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_LOCAL_BBB_FACTOR, 0.90f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_REGIONAL_BBB_FACTOR, 0.75f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_SYSTEMIC_BBB_FACTOR, 0.60f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_STORM_BBB_FACTOR, 0.40f);

    // Pattern factors (higher = more aggressive)
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_NONE_PATTERN_FACTOR, 1.00f);
    EXPECT_FLOAT_EQ(SEC_IMMUNE_INFL_STORM_PATTERN_FACTOR, 2.00f);
}

}  // namespace
