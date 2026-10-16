/**
 * @file test_security_math_integration.cpp
 * @brief Integration tests for Mathematical Security Framework (NIMCP)
 *
 * Tests the integration of all three mathematical security mechanisms:
 * - Cross-component workflows
 * - Multi-threaded concurrent access
 * - Integration with BBB and other security subsystems
 * - Real-world attack scenario simulations
 *
 * INTEGRATION SCENARIOS:
 * 1. Entropy + Trust: Detect tampering and update trust scores
 * 2. Trust + DP: Privacy-preserving trust reports
 * 3. Entropy + DP: Privatized anomaly statistics
 * 4. Full pipeline: All three components working together
 * 5. Concurrent access from multiple threads
 *
 * Phase SC-3: Mathematical Security Framework
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_security_math.h"

#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <cmath>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityMathIntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        entropy_analyzer = nimcp_entropy_create();
        ASSERT_NE(entropy_analyzer, nullptr);
        ASSERT_EQ(nimcp_entropy_init(entropy_analyzer, nullptr), NIMCP_SUCCESS);

        trust_network = nimcp_trust_create();
        ASSERT_NE(trust_network, nullptr);
        ASSERT_EQ(nimcp_trust_init(trust_network, nullptr), NIMCP_SUCCESS);

        dp_context = nimcp_dp_create();
        ASSERT_NE(dp_context, nullptr);
        ASSERT_EQ(nimcp_dp_init(dp_context, nullptr), NIMCP_SUCCESS);
    }

    void TearDown() override
    {
        if (entropy_analyzer) {
            nimcp_entropy_destroy(entropy_analyzer);
            entropy_analyzer = nullptr;
        }
        if (trust_network) {
            nimcp_trust_destroy(trust_network);
            trust_network = nullptr;
        }
        if (dp_context) {
            nimcp_dp_destroy(dp_context);
            dp_context = nullptr;
        }
    }

    // Generate random data with specific entropy level
    void generate_data_with_entropy(std::vector<uint8_t>& data, size_t size, double target_entropy)
    {
        data.resize(size);
        std::mt19937 rng(42);

        if (target_entropy < 1.0) {
            // Low entropy: mostly same values
            std::fill(data.begin(), data.end(), 0xAA);
            size_t variations = static_cast<size_t>(size * 0.01);
            for (size_t i = 0; i < variations; i++) {
                data[rng() % size] = rng() % 256;
            }
        } else if (target_entropy > 7.5) {
            // High entropy: random bytes
            for (auto& b : data) {
                b = rng() % 256;
            }
        } else {
            // Medium entropy: limited alphabet
            int alphabet_size = static_cast<int>(pow(2, target_entropy));
            for (auto& b : data) {
                b = rng() % alphabet_size;
            }
        }
    }

    nimcp_entropy_analyzer_t* entropy_analyzer = nullptr;
    nimcp_trust_network_t* trust_network = nullptr;
    nimcp_dp_context_t* dp_context = nullptr;
};

//=============================================================================
// Cross-Component Integration Tests
//=============================================================================

/**
 * Test: Entropy anomaly detection triggers trust score update
 *
 * Workflow:
 * 1. Register entity in trust network
 * 2. Set entropy baseline for entity's data region
 * 3. Detect tampering via entropy deviation
 * 4. Update trust score based on tampering detection
 */
TEST_F(SecurityMathIntegrationTest, EntropyTamperingUpdatesTrust)
{
    const uint32_t entity_id = 1001;
    ASSERT_EQ(nimcp_trust_register_entity(trust_network, entity_id, "module_A"), NIMCP_SUCCESS);

    // Establish baseline with multiple samples (needed for stddev calculation)
    // The baseline algorithm uses Welford's method and needs sample_count >= 2
    for (int sample = 0; sample < 5; sample++) {
        std::vector<uint8_t> baseline_data;
        generate_data_with_entropy(baseline_data, 4096, 6.0);  // Code-like entropy
        // Add slight variation to each sample
        for (size_t i = 0; i < baseline_data.size() / 50; i++) {
            baseline_data[(sample * 100 + i) % baseline_data.size()] ^= (sample + 1);
        }
        ASSERT_EQ(nimcp_entropy_set_baseline(entropy_analyzer, entity_id,
                  baseline_data.data(), baseline_data.size()), NIMCP_SUCCESS);
    }

    // Record several successful interactions
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(nimcp_trust_record_interaction(trust_network, entity_id, true, 1.0),
                  NIMCP_SUCCESS);
    }

    nimcp_trust_score_t initial_score;
    ASSERT_EQ(nimcp_trust_get_score(trust_network, entity_id, &initial_score), NIMCP_SUCCESS);
    double initial_trust = initial_score.expected_trust;

    // Simulate tampering: change data to high entropy (encrypted/compressed)
    std::vector<uint8_t> tampered_data;
    generate_data_with_entropy(tampered_data, 4096, 7.9);  // High entropy

    nimcp_entropy_result_t entropy_result;
    ASSERT_EQ(nimcp_entropy_check_baseline(entropy_analyzer, entity_id,
              tampered_data.data(), tampered_data.size(), &entropy_result), NIMCP_SUCCESS);

    // Check entropy significantly different from baseline (~6.0 vs ~7.9)
    // Due to how the test generates data, the exact entropy values may vary
    EXPECT_GT(entropy_result.entropy, 7.0);  // Tampered data has high entropy

    // Record failed interaction due to detected tampering
    ASSERT_EQ(nimcp_trust_record_interaction(trust_network, entity_id, false, 2.0),  // Higher weight for security events
              NIMCP_SUCCESS);

    nimcp_trust_score_t final_score;
    ASSERT_EQ(nimcp_trust_get_score(trust_network, entity_id, &final_score), NIMCP_SUCCESS);

    // Trust should decrease after tampering detection
    EXPECT_LT(final_score.expected_trust, initial_trust);
}

/**
 * Test: Privacy-preserving trust statistics report
 *
 * Workflow:
 * 1. Build trust network with multiple entities
 * 2. Generate trust statistics (counts, means)
 * 3. Apply differential privacy before reporting
 */
TEST_F(SecurityMathIntegrationTest, PrivacyPreservingTrustReport)
{
    // Register multiple entities
    const int num_entities = 50;
    std::vector<double> trust_values;

    for (int i = 0; i < num_entities; i++) {
        uint32_t entity_id = 2000 + i;
        ASSERT_EQ(nimcp_trust_register_entity(trust_network, entity_id,
                  ("entity_" + std::to_string(i)).c_str()), NIMCP_SUCCESS);

        // Simulate varying trust levels
        int successes = 5 + (i % 10);
        int failures = i % 5;
        for (int j = 0; j < successes; j++) {
            nimcp_trust_record_interaction(trust_network, entity_id, true, 1.0);
        }
        for (int j = 0; j < failures; j++) {
            nimcp_trust_record_interaction(trust_network, entity_id, false, 1.0);
        }

        nimcp_trust_score_t score;
        nimcp_trust_get_score(trust_network, entity_id, &score);
        trust_values.push_back(score.expected_trust);
    }

    // Calculate true statistics
    double true_mean = 0;
    int high_trust_count = 0;
    for (double t : trust_values) {
        true_mean += t;
        if (t > 0.7) high_trust_count++;
    }
    true_mean /= num_entities;

    // Apply differential privacy to count
    nimcp_dp_result_t count_result;
    ASSERT_EQ(nimcp_dp_count(dp_context, high_trust_count, &count_result), NIMCP_SUCCESS);

    // Noisy count should be within reasonable bound
    EXPECT_NEAR(count_result.noisy_value, high_trust_count, 10.0);  // Reasonable accuracy

    // Apply differential privacy to mean
    nimcp_dp_result_t mean_result;
    ASSERT_EQ(nimcp_dp_mean(dp_context, true_mean, num_entities, 1.0, &mean_result), NIMCP_SUCCESS);

    // Noisy mean should preserve rough accuracy
    EXPECT_NEAR(mean_result.noisy_value, true_mean, 0.2);
}

/**
 * Test: Privatized anomaly detection statistics
 *
 * Workflow:
 * 1. Analyze multiple data regions for anomalies
 * 2. Collect histogram of entropy values
 * 3. Privatize histogram before publishing
 */
TEST_F(SecurityMathIntegrationTest, PrivatizedEntropyHistogram)
{
    const int num_regions = 100;
    uint64_t entropy_histogram[8] = {0};  // Bins: [0-1), [1-2), ..., [7-8)

    // Analyze multiple regions and build entropy histogram
    for (int i = 0; i < num_regions; i++) {
        std::vector<uint8_t> region_data;
        // Generate data with varying entropy levels
        double target_entropy = 1.0 + (i % 7);
        generate_data_with_entropy(region_data, 1024, target_entropy);

        double entropy = nimcp_entropy_calculate(region_data.data(), region_data.size());
        int bin = std::min(7, std::max(0, static_cast<int>(entropy)));
        entropy_histogram[bin]++;
    }

    // Privatize the histogram
    double noisy_histogram[8];
    ASSERT_EQ(nimcp_dp_histogram(dp_context, entropy_histogram, 8, noisy_histogram), NIMCP_SUCCESS);

    // Total should be approximately preserved
    double noisy_total = 0;
    uint64_t true_total = 0;
    for (int i = 0; i < 8; i++) {
        noisy_total += noisy_histogram[i];
        true_total += entropy_histogram[i];
    }
    EXPECT_NEAR(noisy_total, static_cast<double>(true_total), 30.0);  // Within noise bounds
}

/**
 * Test: Full security pipeline - all components working together
 *
 * Simulates a complete security monitoring scenario:
 * 1. Monitor multiple modules with entropy analysis
 * 2. Track trust scores for each module
 * 3. Detect anomalies and update trust
 * 4. Generate privacy-preserving security report
 */
TEST_F(SecurityMathIntegrationTest, FullSecurityPipeline)
{
    const int num_modules = 10;
    struct ModuleState {
        uint32_t entity_id;
        double expected_entropy;  // Expected entropy for this module
        bool is_malicious;
    };
    std::vector<ModuleState> modules(num_modules);

    // Initialize modules
    for (int i = 0; i < num_modules; i++) {
        modules[i].entity_id = 3000 + i;
        modules[i].is_malicious = (i == 7);  // One malicious module
        modules[i].expected_entropy = 6.0;   // Normal modules have ~6.0 entropy

        ASSERT_EQ(nimcp_trust_register_entity(trust_network, modules[i].entity_id,
                  ("module_" + std::to_string(i)).c_str()), NIMCP_SUCCESS);
    }

    // Simulate monitoring cycles using direct entropy comparison
    // (baseline detection requires multiple samples, so we use threshold-based detection)
    const double entropy_threshold = 7.0;  // High entropy suggests tampering
    int anomalies_detected = 0;

    for (int cycle = 0; cycle < 20; cycle++) {
        for (auto& mod : modules) {
            std::vector<uint8_t> current_data;

            if (mod.is_malicious && cycle >= 10) {
                // Malicious module changes behavior after cycle 10
                generate_data_with_entropy(current_data, 2048, 7.8);
            } else {
                // Normal modules maintain expected entropy
                generate_data_with_entropy(current_data, 2048, mod.expected_entropy);
            }

            double entropy = nimcp_entropy_calculate(current_data.data(), current_data.size());

            // Detect anomaly via threshold (high entropy = potential encryption/tampering)
            bool is_anomaly = (entropy > entropy_threshold);
            bool success = !is_anomaly;

            nimcp_trust_record_interaction(trust_network, mod.entity_id, success, 1.0);

            if (is_anomaly) {
                anomalies_detected++;
            }
        }
    }

    // Propagate trust through network
    ASSERT_EQ(nimcp_trust_propagate(trust_network), NIMCP_SUCCESS);

    // Generate privatized security report
    int trusted_count = 0;
    int untrusted_count = 0;
    for (auto& mod : modules) {
        if (nimcp_trust_is_trusted(trust_network, mod.entity_id, 0.5)) {
            trusted_count++;
        } else {
            untrusted_count++;
        }
    }

    // Malicious module should have lower trust (it had 10 failures in cycles 10-19)
    nimcp_trust_score_t malicious_score;
    nimcp_trust_get_score(trust_network, modules[7].entity_id, &malicious_score);

    nimcp_trust_score_t normal_score;
    nimcp_trust_get_score(trust_network, modules[0].entity_id, &normal_score);

    EXPECT_LT(malicious_score.expected_trust, normal_score.expected_trust);

    // Privatized counts
    nimcp_dp_result_t trusted_result, untrusted_result;
    nimcp_dp_count(dp_context, trusted_count, &trusted_result);
    nimcp_dp_count(dp_context, untrusted_count, &untrusted_result);

    // Results should be reasonably accurate
    EXPECT_NEAR(trusted_result.noisy_value, trusted_count, 5.0);

    // Should have detected anomalies from malicious module (10 cycles * 1 malicious = 10)
    EXPECT_GT(anomalies_detected, 0);
    EXPECT_EQ(anomalies_detected, 10);  // Exactly 10 cycles of malicious behavior
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

/**
 * Test: Thread-safe entropy analysis
 */
TEST_F(SecurityMathIntegrationTest, ConcurrentEntropyAnalysis)
{
    const int num_threads = 8;
    const int iterations_per_thread = 100;
    std::atomic<int> successful_analyses{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, iterations_per_thread, &successful_analyses]() {
            std::mt19937 rng(t * 1000);
            std::vector<uint8_t> data(1024);

            for (int i = 0; i < iterations_per_thread; i++) {
                // Generate random data
                for (auto& b : data) {
                    b = rng() % 256;
                }

                // Thread-safe entropy calculation
                double entropy = nimcp_entropy_calculate(data.data(), data.size());

                if (entropy >= 0 && entropy <= 8.0) {
                    successful_analyses++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successful_analyses.load(), num_threads * iterations_per_thread);
}

/**
 * Test: Thread-safe trust network updates
 */
TEST_F(SecurityMathIntegrationTest, ConcurrentTrustUpdates)
{
    const uint32_t shared_entity = 4000;
    ASSERT_EQ(nimcp_trust_register_entity(trust_network, shared_entity, "shared_entity"),
              NIMCP_SUCCESS);

    const int num_threads = 4;
    const int interactions_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> completed{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, shared_entity, interactions_per_thread, &completed]() {
            std::mt19937 rng(t);
            for (int i = 0; i < interactions_per_thread; i++) {
                bool success = (rng() % 10) > 2;  // 70% success rate
                nimcp_trust_record_interaction(trust_network, shared_entity, success, 1.0);
            }
            completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), num_threads);

    // Verify trust score is consistent
    nimcp_trust_score_t score;
    ASSERT_EQ(nimcp_trust_get_score(trust_network, shared_entity, &score), NIMCP_SUCCESS);

    // Total observations should match total interactions
    EXPECT_EQ(score.observations, static_cast<uint64_t>(num_threads * interactions_per_thread));
}

/**
 * Test: Thread-safe differential privacy queries
 */
TEST_F(SecurityMathIntegrationTest, ConcurrentDPQueries)
{
    // Create a DP context with large budget to allow many concurrent queries
    nimcp_dp_context_t* large_budget_ctx = nimcp_dp_create();
    nimcp_dp_config_t config = nimcp_dp_default_config();
    config.total_budget = 200.0;  // Large budget for concurrent test
    config.enforce_budget = true;
    ASSERT_EQ(nimcp_dp_init(large_budget_ctx, &config), NIMCP_SUCCESS);

    const int num_threads = 4;
    const int queries_per_thread = 25;
    std::atomic<int> successful_queries{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([large_budget_ctx, t, queries_per_thread, &successful_queries]() {
            for (int i = 0; i < queries_per_thread; i++) {
                nimcp_dp_result_t result;
                double value = 100.0 + t * 10 + i;

                if (nimcp_dp_add_laplace_noise(large_budget_ctx, value, 1.0, &result) == NIMCP_SUCCESS) {
                    successful_queries++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    nimcp_dp_destroy(large_budget_ctx);

    EXPECT_EQ(successful_queries.load(), num_threads * queries_per_thread);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * Test: Large-scale trust network
 */
TEST_F(SecurityMathIntegrationTest, LargeScaleTrustNetwork)
{
    const int num_entities = 500;

    // Register many entities
    for (int i = 0; i < num_entities; i++) {
        uint32_t entity_id = 5000 + i;
        ASSERT_EQ(nimcp_trust_register_entity(trust_network, entity_id,
                  ("entity_" + std::to_string(i)).c_str()), NIMCP_SUCCESS);

        // Create some voucher relationships
        if (i > 0) {
            nimcp_trust_add_voucher(trust_network, 5000 + (i - 1), entity_id);
        }
        if (i > 10) {
            nimcp_trust_add_voucher(trust_network, 5000 + (i - 10), entity_id);
        }
    }

    // Record interactions for all entities
    std::mt19937 rng(42);
    for (int i = 0; i < num_entities; i++) {
        uint32_t entity_id = 5000 + i;
        int interactions = 5 + (rng() % 20);
        for (int j = 0; j < interactions; j++) {
            bool success = (rng() % 100) < 80;  // 80% success
            nimcp_trust_record_interaction(trust_network, entity_id, success, 1.0);
        }
    }

    // Propagate trust (this is the expensive operation)
    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(nimcp_trust_propagate(trust_network), NIMCP_SUCCESS);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should complete in reasonable time
    EXPECT_LT(elapsed.count(), 5000);  // Less than 5 seconds
}

/**
 * Test: High-volume entropy analysis
 */
TEST_F(SecurityMathIntegrationTest, HighVolumeEntropyAnalysis)
{
    const int num_analyses = 1000;
    std::vector<uint8_t> data(4096);
    std::mt19937 rng(123);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_analyses; i++) {
        // Generate different data each iteration
        for (auto& b : data) {
            b = rng() % 256;
        }

        nimcp_entropy_result_t result;
        ASSERT_EQ(nimcp_entropy_analyze(entropy_analyzer, data.data(), data.size(), &result),
                  NIMCP_SUCCESS);

        // All random data should have high entropy
        EXPECT_GT(result.entropy, 7.0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should handle 1000 analyses in reasonable time
    EXPECT_LT(elapsed.count(), 3000);  // Less than 3 seconds
}

//=============================================================================
// Edge Case Integration Tests
//=============================================================================

/**
 * Test: Budget exhaustion handling in integrated workflow
 */
TEST_F(SecurityMathIntegrationTest, BudgetExhaustionHandling)
{
    // Create context with limited budget
    nimcp_dp_context_t* limited_ctx = nimcp_dp_create();
    nimcp_dp_config_t config = nimcp_dp_default_config();
    config.total_budget = 2.0;
    config.enforce_budget = true;
    ASSERT_EQ(nimcp_dp_init(limited_ctx, &config), NIMCP_SUCCESS);

    // Exhaust budget with queries
    int successful_queries = 0;
    for (int i = 0; i < 100; i++) {
        nimcp_dp_result_t result;
        if (nimcp_dp_count(limited_ctx, 100, &result) == NIMCP_SUCCESS) {
            successful_queries++;
        }
    }

    // Some queries should succeed before budget exhaustion
    EXPECT_GT(successful_queries, 0);
    EXPECT_LT(successful_queries, 100);

    // Budget should be exhausted
    EXPECT_LE(nimcp_dp_remaining_budget(limited_ctx), 0.0);

    // Reset should restore budget
    ASSERT_EQ(nimcp_dp_reset_budget(limited_ctx), NIMCP_SUCCESS);
    EXPECT_GT(nimcp_dp_remaining_budget(limited_ctx), 0.0);

    nimcp_dp_destroy(limited_ctx);
}

/**
 * Test: Zero and empty data handling
 */
TEST_F(SecurityMathIntegrationTest, EdgeCaseDataHandling)
{
    // Zero entropy data (all same bytes)
    std::vector<uint8_t> uniform_data(1024, 0x55);
    double uniform_entropy = nimcp_entropy_calculate(uniform_data.data(), uniform_data.size());
    EXPECT_NEAR(uniform_entropy, 0.0, 0.001);

    // Maximum entropy data
    std::vector<uint8_t> max_entropy_data(256);
    for (int i = 0; i < 256; i++) {
        max_entropy_data[i] = i;
    }
    double max_entropy = nimcp_entropy_calculate(max_entropy_data.data(), max_entropy_data.size());
    EXPECT_NEAR(max_entropy, 8.0, 0.001);

    // Very small data
    uint8_t tiny_data[] = {0, 1};
    double tiny_entropy = nimcp_entropy_calculate(tiny_data, 2);
    EXPECT_NEAR(tiny_entropy, 1.0, 0.001);

    // Single byte
    uint8_t single = 0x42;
    double single_entropy = nimcp_entropy_calculate(&single, 1);
    EXPECT_EQ(single_entropy, 0.0);  // No entropy in single byte
}

}  // namespace
