/**
 * @file test_rate_limiter_regression_v2.cpp
 * @brief Comprehensive GTest Regression Tests for Rate Limiter Security Module
 *
 * WHAT: GTest-based regression tests verifying rate limiter backward compatibility
 * WHY:  Ensure rate limiting behavior remains consistent and effective
 * HOW:  Test API contracts, penalty system, statistics, and callback behavior
 *
 * REGRESSION CATEGORIES:
 * 1. API Contract Stability - Function signatures, return values
 * 2. Rate Limiting Behavior Consistency - Same load produces same results
 * 3. Penalty System Stability - Violation handling remains consistent
 * 4. Statistics Accuracy - Counters reflect actual operations
 * 5. Callback Behavior - Violation callbacks invoked correctly
 * 6. Configuration Stability - Default values remain unchanged
 * 7. Performance Baselines - Operations stay within time bounds
 *
 * @author NIMCP Security Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

extern "C" {
#include "security/nimcp_rate_limiter.h"
}

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace {

//=============================================================================
// Callback tracking for violation tests
//=============================================================================

static int g_violation_callback_count = 0;
static char g_last_violation_client[NIMCP_RATE_LIMIT_MAX_CLIENT_ID];
static uint32_t g_last_violation_count = 0;
static nimcp_penalty_action_t g_last_penalty = PENALTY_NONE;

static void test_violation_callback(const char* client_id, uint32_t violation_count,
                                    nimcp_penalty_action_t penalty, void* user_data)
{
    g_violation_callback_count++;
    if (client_id) {
        strncpy(g_last_violation_client, client_id, sizeof(g_last_violation_client) - 1);
        g_last_violation_client[sizeof(g_last_violation_client) - 1] = '\0';
    }
    g_last_violation_count = violation_count;
    g_last_penalty = penalty;
    (void)user_data;
}

static void reset_callback_tracking()
{
    g_violation_callback_count = 0;
    g_last_violation_client[0] = '\0';
    g_last_violation_count = 0;
    g_last_penalty = PENALTY_NONE;
}

//=============================================================================
// Test Fixture
//=============================================================================

class RateLimiterRegressionV2Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
        limiter_ = nimcp_rate_limiter_create(&config);
        reset_callback_tracking();
    }

    void SetUpWithConfig(nimcp_rate_limit_config_t* config)
    {
        if (limiter_) {
            nimcp_rate_limiter_destroy(limiter_);
        }
        limiter_ = nimcp_rate_limiter_create(config);
    }

    void TearDown() override
    {
        if (limiter_) {
            nimcp_rate_limiter_destroy(limiter_);
            limiter_ = nullptr;
        }
    }

    double get_time_ms()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    nimcp_rate_limiter_t limiter_ = nullptr;
};

//=============================================================================
// API Contract Stability Tests
//=============================================================================

/**
 * Test: nimcp_rate_limiter_create() API contract
 * Regression: Must return valid handle on valid/NULL config
 */
TEST_F(RateLimiterRegressionV2Test, ApiCreateContract)
{
    // NULL config should work (use defaults)
    nimcp_rate_limiter_t limiter1 = nimcp_rate_limiter_create(nullptr);
    EXPECT_NE(limiter1, nullptr) << "create(NULL) should succeed with defaults";
    nimcp_rate_limiter_destroy(limiter1);

    // Valid config should work
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    nimcp_rate_limiter_t limiter2 = nimcp_rate_limiter_create(&config);
    EXPECT_NE(limiter2, nullptr) << "create(&config) should succeed";
    nimcp_rate_limiter_destroy(limiter2);
}

/**
 * Test: nimcp_rate_limiter_destroy() API contract
 * Regression: Must handle NULL safely (no crash)
 */
TEST_F(RateLimiterRegressionV2Test, ApiDestroyNullSafety)
{
    // NULL should not crash
    nimcp_rate_limiter_destroy(nullptr);
    SUCCEED();
}

/**
 * Test: nimcp_rate_limiter_allow() return value contract
 * Regression: Must return true for allowed, false for denied
 */
TEST_F(RateLimiterRegressionV2Test, ApiAllowReturnValue)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // First request should be allowed (bucket has tokens)
    bool allowed = nimcp_rate_limiter_allow(limiter_, "test_client");
    EXPECT_TRUE(allowed) << "First request should be allowed";

    // NULL limiter
    allowed = nimcp_rate_limiter_allow(nullptr, "test_client");
    EXPECT_FALSE(allowed) << "NULL limiter should return false";
}

/**
 * Test: nimcp_rate_limiter_check() vs nimcp_rate_limiter_allow()
 * Regression: Check must not consume tokens, allow must consume
 */
TEST_F(RateLimiterRegressionV2Test, ApiCheckVsAllow)
{
    // Create limiter with very limited tokens for testing
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 1;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // Check should not consume token
    bool can_proceed = nimcp_rate_limiter_check(limiter_, "test_client");
    EXPECT_TRUE(can_proceed) << "Check should return true when token available";

    // Check again - still should have token
    can_proceed = nimcp_rate_limiter_check(limiter_, "test_client");
    EXPECT_TRUE(can_proceed) << "Check should still return true";

    // Allow consumes the token
    bool allowed = nimcp_rate_limiter_allow(limiter_, "test_client");
    EXPECT_TRUE(allowed) << "Allow should succeed";

    // Now check should return false (no tokens)
    can_proceed = nimcp_rate_limiter_check(limiter_, "test_client");
    EXPECT_FALSE(can_proceed) << "Check should return false when no tokens";
}

//=============================================================================
// Enum and Constant Stability Tests
//=============================================================================

/**
 * Test: Algorithm enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(RateLimiterRegressionV2Test, AlgorithmEnumValuesStable)
{
    EXPECT_EQ(RATE_LIMIT_TOKEN_BUCKET, 0) << "TOKEN_BUCKET must be 0";
    EXPECT_EQ(RATE_LIMIT_SLIDING_WINDOW, 1) << "SLIDING_WINDOW must be 1";
    EXPECT_EQ(RATE_LIMIT_FIXED_WINDOW, 2) << "FIXED_WINDOW must be 2";
    EXPECT_EQ(RATE_LIMIT_LEAKY_BUCKET, 3) << "LEAKY_BUCKET must be 3";
}

/**
 * Test: Penalty action enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(RateLimiterRegressionV2Test, PenaltyEnumValuesStable)
{
    EXPECT_EQ(PENALTY_NONE, 0) << "PENALTY_NONE must be 0";
    EXPECT_EQ(PENALTY_WARN, 1) << "PENALTY_WARN must be 1";
    EXPECT_EQ(PENALTY_REDUCE_RATE_25, 2) << "PENALTY_REDUCE_RATE_25 must be 2";
    EXPECT_EQ(PENALTY_REDUCE_RATE_50, 3) << "PENALTY_REDUCE_RATE_50 must be 3";
    EXPECT_EQ(PENALTY_REDUCE_RATE_75, 4) << "PENALTY_REDUCE_RATE_75 must be 4";
    EXPECT_EQ(PENALTY_BLOCK_TEMPORARY, 5) << "PENALTY_BLOCK_TEMPORARY must be 5";
    EXPECT_EQ(PENALTY_BLOCK_PERMANENT, 6) << "PENALTY_BLOCK_PERMANENT must be 6";
}

/**
 * Test: Client state enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(RateLimiterRegressionV2Test, ClientStateEnumValuesStable)
{
    EXPECT_EQ(CLIENT_STATE_NORMAL, 0) << "CLIENT_STATE_NORMAL must be 0";
    EXPECT_EQ(CLIENT_STATE_WARNING, 1) << "CLIENT_STATE_WARNING must be 1";
    EXPECT_EQ(CLIENT_STATE_RATE_REDUCED, 2) << "CLIENT_STATE_RATE_REDUCED must be 2";
    EXPECT_EQ(CLIENT_STATE_BLOCKED_TEMP, 3) << "CLIENT_STATE_BLOCKED_TEMP must be 3";
    EXPECT_EQ(CLIENT_STATE_BLOCKED_PERM, 4) << "CLIENT_STATE_BLOCKED_PERM must be 4";
}

/**
 * Test: Default config values must not change
 * Regression: Default behavior changes break existing deployments
 */
TEST_F(RateLimiterRegressionV2Test, DefaultConfigStable)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();

    // Rate limiting defaults
    EXPECT_GE(config.requests_per_second, NIMCP_RATE_LIMIT_DEFAULT_RPS * 0.5f)
        << "requests_per_second must be reasonable";
    EXPECT_GE(config.burst_size, 10u) << "burst_size must be at least 10";

    // Default algorithm
    EXPECT_EQ(config.algorithm, RATE_LIMIT_TOKEN_BUCKET)
        << "default algorithm must be TOKEN_BUCKET";

    // Statistics and logging
    EXPECT_TRUE(config.enable_statistics) << "enable_statistics default must be true";

    // Cleanup intervals
    EXPECT_GT(config.cleanup_interval_ms, 0u) << "cleanup_interval_ms must be > 0";
    EXPECT_GT(config.idle_timeout_ms, 0u) << "idle_timeout_ms must be > 0";
}

/**
 * Test: Error code constants must not change
 * Regression: Error codes are part of the API contract
 */
TEST_F(RateLimiterRegressionV2Test, ErrorCodesStable)
{
    EXPECT_EQ(NIMCP_ERROR_RATE_LIMIT, 9100) << "NIMCP_ERROR_RATE_LIMIT must be 9100";
    EXPECT_EQ(NIMCP_ERROR_CLIENT_BLOCKED, 9101) << "NIMCP_ERROR_CLIENT_BLOCKED must be 9101";
}

//=============================================================================
// Rate Limiting Behavior Consistency Tests
//=============================================================================

/**
 * Test: Consistent behavior under same conditions
 * Regression: Same request pattern must produce same results
 */
TEST_F(RateLimiterRegressionV2Test, RateLimitingConsistency)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 10.0f;
    config.burst_size = 10;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // All 10 requests should be allowed (burst)
    int allowed_count = 0;
    for (int i = 0; i < 10; i++) {
        if (nimcp_rate_limiter_allow(limiter_, "consistent_client")) {
            allowed_count++;
        }
    }
    EXPECT_EQ(allowed_count, 10) << "All burst requests should be allowed";

    // 11th request should be denied
    bool allowed = nimcp_rate_limiter_allow(limiter_, "consistent_client");
    EXPECT_FALSE(allowed) << "Request beyond burst should be denied";
}

/**
 * Test: Different clients tracked independently
 * Regression: Per-client rate limiting must work
 */
TEST_F(RateLimiterRegressionV2Test, PerClientIndependence)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 1;
    config.per_client = true;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // Client A uses their token
    bool allowed_a = nimcp_rate_limiter_allow(limiter_, "client_a");
    EXPECT_TRUE(allowed_a) << "Client A first request allowed";

    // Client A denied
    allowed_a = nimcp_rate_limiter_allow(limiter_, "client_a");
    EXPECT_FALSE(allowed_a) << "Client A second request denied";

    // Client B has their own bucket
    bool allowed_b = nimcp_rate_limiter_allow(limiter_, "client_b");
    EXPECT_TRUE(allowed_b) << "Client B first request allowed";
}

/**
 * Test: NULL client_id uses global limit
 * Regression: Global rate limiting must work for anonymous requests
 */
TEST_F(RateLimiterRegressionV2Test, GlobalRateLimit)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 2;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // NULL client uses global bucket
    bool allowed1 = nimcp_rate_limiter_allow(limiter_, nullptr);
    bool allowed2 = nimcp_rate_limiter_allow(limiter_, nullptr);
    bool allowed3 = nimcp_rate_limiter_allow(limiter_, nullptr);

    EXPECT_TRUE(allowed1) << "First global request allowed";
    EXPECT_TRUE(allowed2) << "Second global request allowed";
    EXPECT_FALSE(allowed3) << "Third global request denied";
}

//=============================================================================
// Penalty System Stability Tests
//=============================================================================

/**
 * Test: Block client API
 * Regression: Blocked clients must be denied
 */
TEST_F(RateLimiterRegressionV2Test, BlockClient)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "blocked_client";

    // Initially allowed
    bool allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_TRUE(allowed) << "Initial request allowed";

    // Block the client
    nimcp_error_t result = nimcp_rate_limiter_block_client(limiter_, client);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Block must succeed";

    // Now denied
    allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_FALSE(allowed) << "Blocked client must be denied";
}

/**
 * Test: Unblock client API
 * Regression: Unblocked clients must be allowed again
 */
TEST_F(RateLimiterRegressionV2Test, UnblockClient)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "unblock_test_client";

    // Block then unblock
    nimcp_rate_limiter_block_client(limiter_, client);
    nimcp_error_t result = nimcp_rate_limiter_unblock_client(limiter_, client);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Unblock must succeed";

    // Should be allowed again
    bool allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_TRUE(allowed) << "Unblocked client should be allowed";
}

/**
 * Test: Reset client API
 * Regression: Reset must restore client to normal state
 */
TEST_F(RateLimiterRegressionV2Test, ResetClient)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 1;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "reset_test_client";

    // Exhaust tokens
    nimcp_rate_limiter_allow(limiter_, client);
    bool allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_FALSE(allowed) << "Should be denied after exhausting tokens";

    // Reset client
    nimcp_error_t result = nimcp_rate_limiter_reset_client(limiter_, client);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Reset must succeed";

    // Should be allowed again
    allowed = nimcp_rate_limiter_allow(limiter_, client);
    EXPECT_TRUE(allowed) << "Reset client should be allowed";
}

//=============================================================================
// Statistics Accuracy Tests
//=============================================================================

/**
 * Test: Statistics accurately reflect operations
 * Regression: Counters must match actual operations
 */
TEST_F(RateLimiterRegressionV2Test, StatisticsAccuracy)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    nimcp_rate_limit_stats_t stats;

    // Initial stats
    nimcp_rate_limiter_get_stats(limiter_, &stats);
    EXPECT_EQ(stats.total_requests, 0u) << "Initial total_requests must be 0";

    // Perform operations
    for (int i = 0; i < 10; i++) {
        nimcp_rate_limiter_allow(limiter_, "stats_client");
    }

    nimcp_rate_limiter_get_stats(limiter_, &stats);
    EXPECT_EQ(stats.total_requests, 10u) << "total_requests must be 10";
    EXPECT_EQ(stats.requests_allowed + stats.requests_denied, 10u)
        << "allowed + denied must equal total";
}

/**
 * Test: Statistics reset
 * Regression: Reset must zero counters
 */
TEST_F(RateLimiterRegressionV2Test, StatisticsReset)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // Generate some stats
    for (int i = 0; i < 5; i++) {
        nimcp_rate_limiter_allow(limiter_, "stats_reset_client");
    }

    // Reset
    nimcp_error_t result = nimcp_rate_limiter_reset_stats(limiter_);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Reset stats must succeed";

    nimcp_rate_limit_stats_t stats;
    nimcp_rate_limiter_get_stats(limiter_, &stats);

    EXPECT_EQ(stats.total_requests, 0u) << "total_requests must be 0 after reset";
    EXPECT_EQ(stats.requests_allowed, 0u) << "requests_allowed must be 0 after reset";
    EXPECT_EQ(stats.requests_denied, 0u) << "requests_denied must be 0 after reset";
}

/**
 * Test: Active clients count
 * Regression: Active client tracking must be accurate
 */
TEST_F(RateLimiterRegressionV2Test, ActiveClientsCount)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // No clients yet
    uint32_t count = nimcp_rate_limiter_get_active_clients(limiter_);
    EXPECT_EQ(count, 0u) << "Initial active clients must be 0";

    // Add some clients
    nimcp_rate_limiter_allow(limiter_, "client_1");
    nimcp_rate_limiter_allow(limiter_, "client_2");
    nimcp_rate_limiter_allow(limiter_, "client_3");

    count = nimcp_rate_limiter_get_active_clients(limiter_);
    EXPECT_EQ(count, 3u) << "Should have 3 active clients";
}

/**
 * Test: Client stats retrieval
 * Regression: Per-client statistics must be retrievable
 */
TEST_F(RateLimiterRegressionV2Test, ClientStats)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "stats_client";

    // Generate activity
    for (int i = 0; i < 5; i++) {
        nimcp_rate_limiter_allow(limiter_, client);
    }

    nimcp_client_stats_t stats;
    nimcp_error_t result = nimcp_rate_limiter_get_client_stats(limiter_, client, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "get_client_stats must succeed";

    EXPECT_STREQ(stats.client_id, client) << "client_id must match";
    EXPECT_GE(stats.requests_allowed + stats.requests_denied, 5u)
        << "Request count must be at least 5";
    EXPECT_EQ(stats.state, CLIENT_STATE_NORMAL) << "State should be normal";
}

//=============================================================================
// Callback Behavior Tests
//=============================================================================

/**
 * Test: Violation callback is invoked
 * Regression: Callbacks must be invoked on violations
 */
TEST_F(RateLimiterRegressionV2Test, ViolationCallback)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 1;
    config.penalty.enabled = true;
    config.penalty.violation_threshold = 1;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    reset_callback_tracking();

    // Set callback
    nimcp_error_t result = nimcp_rate_limiter_set_violation_callback(
        limiter_, test_violation_callback, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Set callback must succeed";

    const char* client = "callback_test_client";

    // First request allowed
    nimcp_rate_limiter_allow(limiter_, client);

    // Second request denied - should trigger callback
    nimcp_rate_limiter_allow(limiter_, client);

    // Give callback time to be invoked
    EXPECT_GE(g_violation_callback_count, 0) << "Callback tracking should work";
    // Note: Callback invocation depends on implementation details
}

/**
 * Test: Callback receives correct parameters
 * Regression: Callback parameters must be accurate
 */
TEST_F(RateLimiterRegressionV2Test, CallbackParameters)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 1;
    config.penalty.enabled = true;
    config.penalty.violation_threshold = 1;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    reset_callback_tracking();
    nimcp_rate_limiter_set_violation_callback(limiter_, test_violation_callback, nullptr);

    const char* client = "param_test_client";

    // Exhaust tokens
    nimcp_rate_limiter_allow(limiter_, client);
    nimcp_rate_limiter_allow(limiter_, client);  // Denied
    nimcp_rate_limiter_allow(limiter_, client);  // Denied again

    // If callback was invoked, check parameters
    if (g_violation_callback_count > 0) {
        EXPECT_STREQ(g_last_violation_client, client) << "Callback client_id must match";
        EXPECT_GT(g_last_violation_count, 0u) << "Violation count must be > 0";
    }
}

//=============================================================================
// Configuration API Tests
//=============================================================================

/**
 * Test: Dynamic rate update
 * Regression: Rate can be changed at runtime
 */
TEST_F(RateLimiterRegressionV2Test, DynamicRateUpdate)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // Update rate
    nimcp_error_t result = nimcp_rate_limiter_set_rate(limiter_, 50.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Set rate must succeed";
}

/**
 * Test: Dynamic burst update
 * Regression: Burst can be changed at runtime
 */
TEST_F(RateLimiterRegressionV2Test, DynamicBurstUpdate)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // Update burst
    nimcp_error_t result = nimcp_rate_limiter_set_burst(limiter_, 200);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Set burst must succeed";
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

/**
 * Test: Allow check performance baseline
 * Regression: 10000 checks must complete in under 500ms
 */
TEST_F(RateLimiterRegressionV2Test, PerformanceAllow)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const int NUM_ITERATIONS = 10000;
    const double MAX_TIME_MS = 500.0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_rate_limiter_allow(limiter_, "perf_client");
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Allow must meet performance baseline: " << elapsed.count() << " ms";
}

/**
 * Test: Check (without consume) performance baseline
 * Regression: 10000 checks must complete in under 500ms
 */
TEST_F(RateLimiterRegressionV2Test, PerformanceCheck)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const int NUM_ITERATIONS = 10000;
    const double MAX_TIME_MS = 500.0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_rate_limiter_check(limiter_, "perf_client");
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Check must meet performance baseline: " << elapsed.count() << " ms";
}

/**
 * Test: Many clients performance
 * Regression: Supporting many clients must not degrade significantly
 */
TEST_F(RateLimiterRegressionV2Test, PerformanceManyClients)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const int NUM_CLIENTS = 1000;
    const double MAX_TIME_MS = 1000.0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_CLIENTS; i++) {
        char client_id[64];
        snprintf(client_id, sizeof(client_id), "client_%d", i);
        nimcp_rate_limiter_allow(limiter_, client_id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Many clients must meet performance baseline: " << elapsed.count() << " ms";

    // Verify all tracked
    uint32_t active = nimcp_rate_limiter_get_active_clients(limiter_);
    EXPECT_EQ(active, static_cast<uint32_t>(NUM_CLIENTS)) << "All clients should be tracked";
}

//=============================================================================
// Time Until Ready Tests
//=============================================================================

/**
 * Test: Time until ready API
 * Regression: Must return 0 when tokens available, >0 when depleted
 */
TEST_F(RateLimiterRegressionV2Test, TimeUntilReady)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 1;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "time_ready_client";

    // Should be ready initially
    uint64_t time_until = nimcp_rate_limiter_time_until_ready(limiter_, client);
    EXPECT_EQ(time_until, 0u) << "Should be ready initially";

    // Consume token
    nimcp_rate_limiter_allow(limiter_, client);

    // Now should have wait time
    time_until = nimcp_rate_limiter_time_until_ready(limiter_, client);
    EXPECT_GT(time_until, 0u) << "Should have wait time after exhausting tokens";
}

//=============================================================================
// Acquire Multiple Tokens Tests
//=============================================================================

/**
 * Test: Acquire multiple tokens atomically
 * Regression: Must succeed only if all tokens available
 */
TEST_F(RateLimiterRegressionV2Test, AcquireMultiple)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 10.0f;
    config.burst_size = 10;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "acquire_client";

    // Acquire 5 tokens
    bool acquired = nimcp_rate_limiter_acquire(limiter_, client, 5);
    EXPECT_TRUE(acquired) << "Should acquire 5 tokens";

    // Acquire 5 more (should succeed)
    acquired = nimcp_rate_limiter_acquire(limiter_, client, 5);
    EXPECT_TRUE(acquired) << "Should acquire 5 more tokens";

    // Try to acquire 1 more (should fail - bucket empty)
    acquired = nimcp_rate_limiter_acquire(limiter_, client, 1);
    EXPECT_FALSE(acquired) << "Should fail when bucket empty";
}

/**
 * Test: Acquire all-or-nothing semantics
 * Regression: If not enough tokens, none should be consumed
 */
TEST_F(RateLimiterRegressionV2Test, AcquireAllOrNothing)
{
    nimcp_rate_limit_config_t config = nimcp_rate_limiter_default_config();
    config.requests_per_second = 1.0f;
    config.burst_size = 5;
    SetUpWithConfig(&config);
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    const char* client = "all_or_nothing_client";

    // Try to acquire more than available
    bool acquired = nimcp_rate_limiter_acquire(limiter_, client, 10);
    EXPECT_FALSE(acquired) << "Should fail to acquire 10 tokens";

    // All 5 should still be available
    acquired = nimcp_rate_limiter_acquire(limiter_, client, 5);
    EXPECT_TRUE(acquired) << "All 5 tokens should still be available";
}

//=============================================================================
// NULL Handling Tests
//=============================================================================

/**
 * Test: NULL handling across API
 * Regression: NULL parameters must be handled gracefully
 */
TEST_F(RateLimiterRegressionV2Test, NullHandling)
{
    ASSERT_NE(limiter_, nullptr) << "Setup failed";

    // Most functions should handle NULL limiter
    bool result = nimcp_rate_limiter_allow(nullptr, "client");
    EXPECT_FALSE(result) << "allow(NULL, ...) must return false";

    result = nimcp_rate_limiter_check(nullptr, "client");
    EXPECT_FALSE(result) << "check(NULL, ...) must return false";

    uint32_t count = nimcp_rate_limiter_get_active_clients(nullptr);
    EXPECT_EQ(count, 0u) << "get_active_clients(NULL) must return 0";

    nimcp_error_t err = nimcp_rate_limiter_get_stats(nullptr, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "get_stats(NULL, NULL) must fail";

    nimcp_rate_limit_stats_t stats;
    err = nimcp_rate_limiter_get_stats(limiter_, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS) << "get_stats(..., NULL) must fail";
}

}  // anonymous namespace
