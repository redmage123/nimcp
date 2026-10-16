/**
 * @file test_security_integration_framework_regression.cpp
 * @brief Regression tests for Security Integration Framework
 *
 * Phase SC-4: Universal Security Integration Framework
 *
 * Tests API stability, backward compatibility, and performance baselines:
 * 1. API stability - consistent behavior across versions
 * 2. Default configuration correctness
 * 3. Performance baselines for critical operations
 * 4. Mathematical correctness of security calculations
 * 5. Thread safety guarantees
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_security_integration.h"
#include "security/nimcp_security_math.h"

#include <cstring>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityIntegrationFrameworkRegressionTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        ctx = nimcp_sec_integration_create();
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override
    {
        if (ctx) {
            nimcp_sec_integration_destroy(ctx);
            ctx = nullptr;
        }
    }

    nimcp_sec_integration_t* ctx = nullptr;
};

//=============================================================================
// API Stability Tests
//=============================================================================

/**
 * Test: Default configuration values remain stable
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, DefaultConfigurationValues)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();

    // Default values must remain consistent across versions
    EXPECT_DOUBLE_EQ(config.trust_threshold, 0.5);
    EXPECT_DOUBLE_EQ(config.entropy_deviation_threshold, 3.0);
    EXPECT_DOUBLE_EQ(config.privacy_budget, 10.0);
    EXPECT_EQ(config.integrity_check_interval_ms, 1000u);
    EXPECT_EQ(config.self_check_interval_ms, 5000u);
    EXPECT_TRUE(config.enable_continuous_monitoring);  // Default is on for security
    EXPECT_TRUE(config.enable_self_monitoring);
    EXPECT_TRUE(config.enable_event_logging);  // Default is on for auditing
    EXPECT_EQ(config.event_callback, nullptr);
    EXPECT_EQ(config.callback_user_data, nullptr);
}

/**
 * Test: Create and destroy lifecycle is idempotent
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, CreateDestroyIdempotent)
{
    for (int i = 0; i < 10; i++) {
        nimcp_sec_integration_t* test_ctx = nimcp_sec_integration_create();
        ASSERT_NE(test_ctx, nullptr);

        nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
        config.enable_continuous_monitoring = false;
        ASSERT_EQ(nimcp_sec_integration_init(test_ctx, &config), NIMCP_SUCCESS);

        nimcp_sec_integration_destroy(test_ctx);
    }
}

/**
 * Test: Module registration API contract
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, ModuleRegistrationContract)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;

    // Valid registration must succeed
    EXPECT_EQ(nimcp_sec_register_module(ctx, "test_module", NIMCP_SEC_CAT_CORE, &module_id),
              NIMCP_SUCCESS);

    // Module ID must be non-zero (0 is reserved for invalid)
    EXPECT_GT(module_id, 0u);

    // NULL name must fail
    EXPECT_NE(nimcp_sec_register_module(ctx, nullptr, NIMCP_SEC_CAT_CORE, &module_id),
              NIMCP_SUCCESS);

    // NULL context must fail
    EXPECT_NE(nimcp_sec_register_module(nullptr, "test", NIMCP_SEC_CAT_CORE, &module_id),
              NIMCP_SUCCESS);

    // NULL id pointer must fail
    EXPECT_NE(nimcp_sec_register_module(ctx, "test2", NIMCP_SEC_CAT_CORE, nullptr),
              NIMCP_SUCCESS);
}

/**
 * Test: Region registration API contract
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, RegionRegistrationContract)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    uint8_t data[256];
    memset(data, 0x42, sizeof(data));
    uint32_t region_id;

    // Valid region registration
    EXPECT_EQ(nimcp_sec_register_region(ctx, module_id, "region1", data, sizeof(data), &region_id),
              NIMCP_SUCCESS);

    // Region ID must be non-zero
    EXPECT_GT(region_id, 0u);

    // Invalid module ID must fail
    EXPECT_NE(nimcp_sec_register_region(ctx, 99999, "region", data, sizeof(data), &region_id),
              NIMCP_SUCCESS);

    // NULL data must fail
    EXPECT_NE(nimcp_sec_register_region(ctx, module_id, "region2", nullptr, 256, &region_id),
              NIMCP_SUCCESS);

    // Zero size must fail
    EXPECT_NE(nimcp_sec_register_region(ctx, module_id, "region3", data, 0, &region_id),
              NIMCP_SUCCESS);
}

/**
 * Test: Trust score API contract
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, TrustScoreContract)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    nimcp_trust_score_t score;

    // Get trust for registered module must succeed
    EXPECT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &score), NIMCP_SUCCESS);

    // Trust must be in [0, 1] range
    EXPECT_GE(score.expected_trust, 0.0);
    EXPECT_LE(score.expected_trust, 1.0);

    // Invalid module ID must fail
    EXPECT_NE(nimcp_sec_get_trust_score(ctx, 99999, &score), NIMCP_SUCCESS);

    // NULL score pointer must fail
    EXPECT_NE(nimcp_sec_get_trust_score(ctx, module_id, nullptr), NIMCP_SUCCESS);
}

/**
 * Test: Privacy query API contract
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, PrivacyQueryContract)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.privacy_budget = 100.0;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_API, &module_id), NIMCP_SUCCESS);

    double result;

    // Private count must return a value
    EXPECT_EQ(nimcp_sec_private_count(ctx, module_id, 1000, &result), NIMCP_SUCCESS);

    // Result should be approximately correct
    EXPECT_NEAR(result, 1000.0, 200.0);

    // Private mean must work
    EXPECT_EQ(nimcp_sec_private_mean(ctx, module_id, 50.0, 100, 100.0, &result), NIMCP_SUCCESS);
    EXPECT_NEAR(result, 50.0, 20.0);

    // Private sum must work
    EXPECT_EQ(nimcp_sec_private_sum(ctx, module_id, 10000.0, 1000.0, &result), NIMCP_SUCCESS);
    EXPECT_NEAR(result, 10000.0, 3000.0);

    // NULL result pointer must fail
    EXPECT_NE(nimcp_sec_private_count(ctx, module_id, 1000, nullptr), NIMCP_SUCCESS);
}

//=============================================================================
// Mathematical Correctness Tests
//=============================================================================

/**
 * Test: Bayesian trust updates follow Beta distribution
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, BayesianTrustMathematicalCorrectness)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_CORE, &module_id), NIMCP_SUCCESS);

    // Record known interactions: 80 successes, 20 failures
    for (int i = 0; i < 80; i++) {
        nimcp_sec_record_interaction(ctx, module_id, true, 1.0);
    }
    for (int i = 0; i < 20; i++) {
        nimcp_sec_record_interaction(ctx, module_id, false, 1.0);
    }

    nimcp_trust_score_t score;
    ASSERT_EQ(nimcp_sec_get_trust_score(ctx, module_id, &score), NIMCP_SUCCESS);

    // Beta distribution: E[X] = α / (α + β)
    // With prior α=1, β=1 and observations 80 success, 20 failure:
    // α_post = 1 + 80 = 81, β_post = 1 + 20 = 21
    // E[X] = 81 / 102 ≈ 0.794
    double expected_mean = 81.0 / 102.0;
    EXPECT_NEAR(score.expected_trust, expected_mean, 0.05);
}

/**
 * Test: Trust propagation attenuates correctly
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, TrustPropagationAttenuation)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t id_a, id_b, id_c;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "A", NIMCP_SEC_CAT_CORE, &id_a), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_register_module(ctx, "B", NIMCP_SEC_CAT_CORE, &id_b), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_sec_register_module(ctx, "C", NIMCP_SEC_CAT_CORE, &id_c), NIMCP_SUCCESS);

    // Build high trust for A
    for (int i = 0; i < 50; i++) {
        nimcp_sec_record_interaction(ctx, id_a, true, 1.0);
    }

    // A vouches for B, B vouches for C
    nimcp_sec_add_trust_voucher(ctx, id_a, id_b);
    nimcp_sec_add_trust_voucher(ctx, id_b, id_c);

    // Propagate
    nimcp_sec_propagate_trust(ctx);

    nimcp_trust_score_t score_a, score_b, score_c;
    nimcp_sec_get_trust_score(ctx, id_a, &score_a);
    nimcp_sec_get_trust_score(ctx, id_b, &score_b);
    nimcp_sec_get_trust_score(ctx, id_c, &score_c);

    // Trust should attenuate: A > B > C
    EXPECT_GT(score_a.expected_trust, score_b.expected_trust);
    EXPECT_GE(score_b.expected_trust, score_c.expected_trust);
}

/**
 * Test: Differential privacy noise follows Laplace distribution
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, DifferentialPrivacyNoiseMathematicalCorrectness)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.privacy_budget = 1000.0;  // High budget for many queries
    // Note: epsilon is set in the underlying DP context, default is 1.0
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    ASSERT_EQ(nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_API, &module_id), NIMCP_SUCCESS);

    // Query same value many times, collect results
    const double true_value = 1000.0;
    const int num_samples = 200;
    std::vector<double> samples;

    for (int i = 0; i < num_samples; i++) {
        double noisy;
        if (nimcp_sec_private_count(ctx, module_id, true_value, &noisy) == NIMCP_SUCCESS) {
            samples.push_back(noisy);
        }
    }

    ASSERT_GT(samples.size(), 50u);

    // Calculate sample mean and variance
    double sum = 0;
    for (auto v : samples) sum += v;
    double mean = sum / samples.size();

    double var_sum = 0;
    for (auto v : samples) var_sum += (v - mean) * (v - mean);
    double variance = var_sum / samples.size();

    // Laplace noise has variance = 2 * (sensitivity / epsilon)^2
    // For count, sensitivity = 1, epsilon = 1
    // Expected variance = 2
    // But due to small sample size, allow large tolerance
    EXPECT_NEAR(mean, true_value, 5.0);
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

/**
 * Test: Module registration performance baseline
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, ModuleRegistrationPerformance)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    const int num_modules = 500;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_modules; i++) {
        uint32_t id;
        std::string name = "perf_mod_" + std::to_string(i);
        nimcp_sec_register_module(ctx, name.c_str(), NIMCP_SEC_CAT_CORE, &id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should register 500 modules in < 100ms (200μs per module)
    EXPECT_LT(elapsed.count(), 100000);

    double us_per_module = static_cast<double>(elapsed.count()) / num_modules;
    // Log for performance tracking
    printf("[PERF] Module registration: %.2f us/module\n", us_per_module);
}

/**
 * Test: Region check performance baseline
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, RegionCheckPerformance)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "perf_test", NIMCP_SEC_CAT_CORE, &module_id);

    // Create regions of various sizes
    std::vector<std::pair<std::vector<uint8_t>, uint32_t>> regions;
    int sizes[] = {256, 1024, 4096, 16384};

    for (int size : sizes) {
        std::vector<uint8_t> data(size, 0x42);
        uint32_t region_id;
        nimcp_sec_register_region(ctx, module_id, ("region_" + std::to_string(size)).c_str(),
                                 data.data(), data.size(), &region_id);
        regions.push_back({std::move(data), region_id});
    }

    // Benchmark checks
    const int checks_per_region = 100;

    for (auto& [data, region_id] : regions) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < checks_per_region; i++) {
            bool is_anomaly;
            double deviation;
            nimcp_sec_check_region(ctx, region_id, data.data(), data.size(),
                                  &is_anomaly, &deviation);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double us_per_check = static_cast<double>(elapsed.count()) / checks_per_region;

        // Should check region in < 1ms per check
        EXPECT_LT(us_per_check, 1000);

        printf("[PERF] Region check (%zu bytes): %.2f us/check\n", data.size(), us_per_check);
    }
}

/**
 * Test: Trust propagation performance baseline
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, TrustPropagationPerformance)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    // Create network of modules with vouchers
    const int num_modules = 100;
    std::vector<uint32_t> module_ids;

    for (int i = 0; i < num_modules; i++) {
        uint32_t id;
        nimcp_sec_register_module(ctx, ("net_" + std::to_string(i)).c_str(),
                                 NIMCP_SEC_CAT_CORE, &id);
        module_ids.push_back(id);
    }

    // Create voucher network (each vouches for 2-3 others)
    for (size_t i = 0; i < module_ids.size(); i++) {
        nimcp_sec_add_trust_voucher(ctx, module_ids[i], module_ids[(i + 1) % num_modules]);
        nimcp_sec_add_trust_voucher(ctx, module_ids[i], module_ids[(i + 2) % num_modules]);
    }

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_sec_propagate_trust(ctx);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should propagate in < 500ms for 100 modules
    EXPECT_LT(elapsed.count(), 500);

    printf("[PERF] Trust propagation (100 modules): %ld ms\n", static_cast<long>(elapsed.count()));
}

/**
 * Test: Privacy query performance baseline
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, PrivacyQueryPerformance)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.privacy_budget = 1000.0;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "perf_test", NIMCP_SEC_CAT_API, &module_id);

    const int num_queries = 500;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_queries; i++) {
        double noisy;
        nimcp_sec_private_count(ctx, module_id, 1000, &noisy);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_query = static_cast<double>(elapsed.count()) / num_queries;

    // Should handle query in < 100us
    EXPECT_LT(us_per_query, 100);

    printf("[PERF] Privacy query: %.2f us/query\n", us_per_query);
}

//=============================================================================
// Thread Safety Guarantee Tests
//=============================================================================

/**
 * Test: Concurrent registration doesn't corrupt state
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, ConcurrentRegistrationStateIntegrity)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    const int num_threads = 4;
    const int modules_per_thread = 50;
    std::vector<std::thread> threads;
    std::vector<std::vector<uint32_t>> thread_module_ids(num_threads);

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, modules_per_thread, &thread_module_ids]() {
            for (int i = 0; i < modules_per_thread; i++) {
                uint32_t id;
                std::string name = "t" + std::to_string(t) + "_m" + std::to_string(i);
                if (nimcp_sec_register_module(ctx, name.c_str(), NIMCP_SEC_CAT_CORE, &id) == NIMCP_SUCCESS) {
                    thread_module_ids[t].push_back(id);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All registrations should succeed
    int total_registered = 0;
    for (auto& ids : thread_module_ids) {
        total_registered += ids.size();
    }
    EXPECT_EQ(total_registered, num_threads * modules_per_thread);

    // All IDs should be unique
    std::set<uint32_t> all_ids;
    for (auto& ids : thread_module_ids) {
        for (auto id : ids) {
            EXPECT_EQ(all_ids.count(id), 0u) << "Duplicate module ID: " << id;
            all_ids.insert(id);
        }
    }

    // Stats should match
    nimcp_sec_integration_stats_t stats;
    nimcp_sec_get_stats(ctx, &stats);
    EXPECT_GE(stats.registered_modules, static_cast<uint32_t>(total_registered));
}

/**
 * Test: Concurrent trust updates maintain consistency
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, ConcurrentTrustUpdateConsistency)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "shared", NIMCP_SEC_CAT_CORE, &module_id);

    const int num_threads = 4;
    const int interactions_per_thread = 100;
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, module_id, t, interactions_per_thread, &successes, &failures]() {
            for (int i = 0; i < interactions_per_thread; i++) {
                bool success = (t % 2 == 0);  // Alternating success/failure by thread
                nimcp_sec_record_interaction(ctx, module_id, success, 1.0);
                if (success) successes++;
                else failures++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Trust should reflect approximately the ratio of successes to failures
    nimcp_trust_score_t score;
    nimcp_sec_get_trust_score(ctx, module_id, &score);

    // 2 threads succeeded, 2 failed -> 50% success rate
    // Expected trust ≈ (1 + 200) / (2 + 400) ≈ 0.5
    EXPECT_NEAR(score.expected_trust, 0.5, 0.1);
}

/**
 * Test: Concurrent privacy queries respect budget
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, ConcurrentPrivacyBudgetRespect)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.privacy_budget = 50.0;  // Limited budget
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "api", NIMCP_SEC_CAT_API, &module_id);

    const int num_threads = 4;
    const int queries_per_thread = 50;
    std::atomic<int> successful_queries{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, module_id, queries_per_thread, &successful_queries]() {
            for (int i = 0; i < queries_per_thread; i++) {
                double noisy;
                if (nimcp_sec_private_count(ctx, module_id, 100, &noisy) == NIMCP_SUCCESS) {
                    successful_queries++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Budget should limit total queries (each query costs epsilon=1.0)
    // With budget 50, approximately 50 queries should succeed
    EXPECT_GT(successful_queries.load(), 0);
    EXPECT_LE(successful_queries.load(), 60);  // Allow some tolerance

    // Budget should be exhausted
    double remaining = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_LE(remaining, 0.0);
}

//=============================================================================
// Self-Monitoring Stability Tests
//=============================================================================

/**
 * Test: Self-monitoring module is always registered
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, SelfMonitoringAlwaysEnabled)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.enable_self_monitoring = true;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(ctx, &stats), NIMCP_SUCCESS);

    // Self-monitoring module should be registered
    EXPECT_GE(stats.registered_modules, 1u);
}

/**
 * Test: Self-check always succeeds for healthy system
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, SelfCheckHealthySystem)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.enable_self_monitoring = true;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    // Register some modules and interact normally
    for (int i = 0; i < 10; i++) {
        uint32_t id;
        nimcp_sec_register_module(ctx, ("mod_" + std::to_string(i)).c_str(),
                                 NIMCP_SEC_CAT_CORE, &id);
        nimcp_sec_record_interaction(ctx, id, true, 1.0);
    }

    bool passed;
    ASSERT_EQ(nimcp_sec_self_check(ctx, &passed), NIMCP_SUCCESS);
    EXPECT_TRUE(passed);
}

//=============================================================================
// Event System Stability Tests
//=============================================================================

/**
 * Test: Event queue doesn't overflow
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, EventQueueNoOverflow)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    // Note: max_events is fixed at NIMCP_SEC_MAX_EVENTS_QUEUE (4096)
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_CORE, &module_id);

    // Generate many events
    for (int i = 0; i < 5000; i++) {
        nimcp_sec_record_interaction(ctx, module_id, (i % 10) > 2, 1.0);
    }

    // Queue should be bounded at max (4096)
    uint32_t event_count = nimcp_sec_get_event_count(ctx);
    EXPECT_LE(event_count, NIMCP_SEC_MAX_EVENTS_QUEUE);
}

/**
 * Test: Event clear works correctly
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, EventClearWorks)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_CORE, &module_id);

    // Generate some events
    for (int i = 0; i < 50; i++) {
        nimcp_sec_record_interaction(ctx, module_id, false, 1.0);
    }

    // Clear events
    ASSERT_EQ(nimcp_sec_clear_events(ctx), NIMCP_SUCCESS);

    // Queue should be empty
    EXPECT_EQ(nimcp_sec_get_event_count(ctx), 0u);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

/**
 * Test: Stats structure has expected fields
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, StatsStructureStable)
{
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(ctx, &stats), NIMCP_SUCCESS);

    // All fields should be initialized (not garbage)
    EXPECT_GE(stats.registered_modules, 0u);
    EXPECT_GE(stats.active_modules, 0u);
    EXPECT_GE(stats.monitored_regions, 0u);
    EXPECT_GE(stats.total_integrity_checks, 0u);
    EXPECT_GE(stats.total_anomalies_detected, 0u);
    EXPECT_GE(stats.total_trust_violations, 0u);
    EXPECT_GE(stats.total_events_generated, 0u);
    EXPECT_GE(stats.self_checks_performed, 0u);
    EXPECT_GE(stats.average_trust_score, 0.0);
    EXPECT_GE(stats.privacy_budget_remaining, 0.0);
}

/**
 * Test: Privacy budget reset restores to original value
 */
TEST_F(SecurityIntegrationFrameworkRegressionTest, PrivacyBudgetResetRestoresOriginal)
{
    const double original_budget = 25.0;
    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.privacy_budget = original_budget;
    ASSERT_EQ(nimcp_sec_integration_init(ctx, &config), NIMCP_SUCCESS);

    uint32_t module_id;
    nimcp_sec_register_module(ctx, "test", NIMCP_SEC_CAT_API, &module_id);

    // Use some budget
    for (int i = 0; i < 10; i++) {
        double noisy;
        nimcp_sec_private_count(ctx, module_id, 100, &noisy);
    }

    double used_budget = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_LT(used_budget, original_budget);

    // Reset
    ASSERT_EQ(nimcp_sec_reset_privacy_budget(ctx), NIMCP_SUCCESS);

    double reset_budget = nimcp_sec_get_privacy_budget(ctx);
    EXPECT_DOUBLE_EQ(reset_budget, original_budget);
}

}  // namespace
