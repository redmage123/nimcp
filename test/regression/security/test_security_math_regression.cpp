/**
 * @file test_security_math_regression.cpp
 * @brief Regression tests for Mathematical Security Framework (NIMCP)
 *
 * Tests to ensure backward compatibility and consistent behavior:
 * - API backward compatibility
 * - Consistent results for same inputs
 * - Performance benchmarks
 * - Default configuration stability
 * - Mathematical correctness verification
 *
 * REGRESSION FOCUS:
 * 1. API contracts remain stable
 * 2. Mathematical properties are preserved
 * 3. Performance stays within bounds
 * 4. Default values remain unchanged
 *
 * Phase SC-3: Mathematical Security Framework
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_security_math.h"

#include <cstring>
#include <chrono>
#include <vector>
#include <string>
#include <cmath>
#include <random>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityMathRegressionTest : public ::testing::Test {
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

    // Helper to measure operation time
    template<typename Func>
    double measure_time_ms(Func&& func, int iterations = 1)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        return elapsed.count() / iterations;
    }

    nimcp_entropy_analyzer_t* entropy_analyzer = nullptr;
    nimcp_trust_network_t* trust_network = nullptr;
    nimcp_dp_context_t* dp_context = nullptr;
};

//=============================================================================
// API Backward Compatibility Tests - Entropy
//=============================================================================

TEST_F(SecurityMathRegressionTest, APIEntropyCreateDestroy)
{
    // API: nimcp_entropy_create() should work
    nimcp_entropy_analyzer_t* analyzer = nimcp_entropy_create();
    ASSERT_NE(analyzer, nullptr);

    // API: nimcp_entropy_init() with NULL config should use defaults
    EXPECT_EQ(nimcp_entropy_init(analyzer, nullptr), NIMCP_SUCCESS);

    // API: nimcp_entropy_destroy() should be safe
    nimcp_entropy_destroy(analyzer);

    // API: nimcp_entropy_destroy(NULL) should be safe
    nimcp_entropy_destroy(nullptr);
}

TEST_F(SecurityMathRegressionTest, APIEntropyCalculateSignature)
{
    // API: nimcp_entropy_calculate() should accept const void* and size_t
    const uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7};
    double entropy = nimcp_entropy_calculate(data, sizeof(data));
    EXPECT_GE(entropy, 0.0);
    EXPECT_LE(entropy, 8.0);

    // API: Should handle NULL gracefully
    double null_entropy = nimcp_entropy_calculate(nullptr, 0);
    EXPECT_EQ(null_entropy, 0.0);
}

TEST_F(SecurityMathRegressionTest, APIEntropyAnalyzeSignature)
{
    const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    nimcp_entropy_result_t result;

    // API: nimcp_entropy_analyze() full signature
    nimcp_result_t status = nimcp_entropy_analyze(
        entropy_analyzer, data, sizeof(data), &result);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Result structure must have these fields
    EXPECT_GE(result.entropy, 0.0);
    EXPECT_LE(result.entropy, 8.0);
    EXPECT_FALSE(result.is_anomaly);  // Normal data shouldn't be anomaly
}

//=============================================================================
// API Backward Compatibility Tests - Trust
//=============================================================================

TEST_F(SecurityMathRegressionTest, APITrustCreateDestroy)
{
    nimcp_trust_network_t* network = nimcp_trust_create();
    ASSERT_NE(network, nullptr);

    EXPECT_EQ(nimcp_trust_init(network, nullptr), NIMCP_SUCCESS);

    nimcp_trust_destroy(network);
    nimcp_trust_destroy(nullptr);  // Should be safe
}

TEST_F(SecurityMathRegressionTest, APITrustEntityWorkflow)
{
    // API: Register entity
    EXPECT_EQ(nimcp_trust_register_entity(trust_network, 1, "test_entity"), NIMCP_SUCCESS);

    // API: Record interaction
    EXPECT_EQ(nimcp_trust_record_interaction(trust_network, 1, true, 1.0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_trust_record_interaction(trust_network, 1, false, 1.0), NIMCP_SUCCESS);

    // API: Get score
    nimcp_trust_score_t score;
    EXPECT_EQ(nimcp_trust_get_score(trust_network, 1, &score), NIMCP_SUCCESS);

    // Score structure must have these fields
    EXPECT_GT(score.alpha, 0);
    EXPECT_GT(score.beta, 0);
    EXPECT_GE(score.expected_trust, 0.0);
    EXPECT_LE(score.expected_trust, 1.0);
}

TEST_F(SecurityMathRegressionTest, APITrustVoucherWorkflow)
{
    EXPECT_EQ(nimcp_trust_register_entity(trust_network, 1, "voucher"), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_trust_register_entity(trust_network, 2, "target"), NIMCP_SUCCESS);

    // API: Add voucher
    EXPECT_EQ(nimcp_trust_add_voucher(trust_network, 1, 2), NIMCP_SUCCESS);

    // API: Propagate
    EXPECT_EQ(nimcp_trust_propagate(trust_network), NIMCP_SUCCESS);

    // API: Check trusted status
    bool trusted = nimcp_trust_is_trusted(trust_network, 2, 0.3);
    EXPECT_TRUE(trusted);  // With default prior, should be trusted at low threshold
}

//=============================================================================
// API Backward Compatibility Tests - Differential Privacy
//=============================================================================

TEST_F(SecurityMathRegressionTest, APIDPCreateDestroy)
{
    nimcp_dp_context_t* ctx = nimcp_dp_create();
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(nimcp_dp_init(ctx, nullptr), NIMCP_SUCCESS);

    nimcp_dp_destroy(ctx);
    nimcp_dp_destroy(nullptr);  // Should be safe
}

TEST_F(SecurityMathRegressionTest, APIDPLaplaceSignature)
{
    nimcp_dp_result_t result;

    // API: nimcp_dp_add_laplace_noise() signature
    EXPECT_EQ(nimcp_dp_add_laplace_noise(dp_context, 100.0, 1.0, &result), NIMCP_SUCCESS);

    // Result structure must have these fields
    EXPECT_NE(result.noisy_value, 0);  // Highly unlikely to be exactly 0
    EXPECT_NE(result.noise_added, 0);  // Noise was added
    EXPECT_GT(result.epsilon_spent, 0);
}

TEST_F(SecurityMathRegressionTest, APIDPQueryFunctions)
{
    nimcp_dp_result_t result;

    // API: Count query
    EXPECT_EQ(nimcp_dp_count(dp_context, 1000, &result), NIMCP_SUCCESS);

    // API: Sum query
    EXPECT_EQ(nimcp_dp_sum(dp_context, 5000.0, 100.0, &result), NIMCP_SUCCESS);

    // API: Mean query
    EXPECT_EQ(nimcp_dp_mean(dp_context, 50.0, 100, 100.0, &result), NIMCP_SUCCESS);

    // API: Histogram query
    uint64_t histogram[] = {10, 20, 30, 40};
    double noisy_hist[4];
    EXPECT_EQ(nimcp_dp_histogram(dp_context, histogram, 4, noisy_hist), NIMCP_SUCCESS);
}

TEST_F(SecurityMathRegressionTest, APIDPBudgetFunctions)
{
    // API: Get remaining budget
    double budget = nimcp_dp_remaining_budget(dp_context);
    EXPECT_GT(budget, 0);

    // Spend some budget
    nimcp_dp_result_t result;
    nimcp_dp_count(dp_context, 100, &result);

    // API: Reset budget
    EXPECT_EQ(nimcp_dp_reset_budget(dp_context), NIMCP_SUCCESS);
    double reset_budget = nimcp_dp_remaining_budget(dp_context);
    EXPECT_GT(reset_budget, 0);
}

//=============================================================================
// Default Configuration Stability Tests
//=============================================================================

TEST_F(SecurityMathRegressionTest, EntropyDefaultConfig)
{
    nimcp_entropy_config_t config = nimcp_entropy_default_config();

    // Default values must remain stable
    EXPECT_EQ(config.deviation_threshold, 3.0);
    EXPECT_EQ(config.window_size, NIMCP_ENTROPY_WINDOW_SIZE);
    EXPECT_TRUE(config.track_baseline);
    EXPECT_EQ(config.baseline_samples, 10u);
}

TEST_F(SecurityMathRegressionTest, TrustDefaultConfig)
{
    nimcp_trust_config_t config = nimcp_trust_default_config();

    // Default values must remain stable
    EXPECT_EQ(config.prior_alpha, 1.0);
    EXPECT_EQ(config.prior_beta, 1.0);
    EXPECT_EQ(config.vouch_weight, 0.5);
    EXPECT_EQ(config.decay_rate, 0.01);
    EXPECT_EQ(config.propagation_damping, 0.8);
}

TEST_F(SecurityMathRegressionTest, DPDefaultConfig)
{
    nimcp_dp_config_t config = nimcp_dp_default_config();

    // Default values must remain stable
    EXPECT_EQ(config.epsilon, NIMCP_DP_DEFAULT_EPSILON);
    EXPECT_EQ(config.delta, NIMCP_DP_DEFAULT_DELTA);
    EXPECT_EQ(config.mechanism, NIMCP_DP_LAPLACE);
    EXPECT_EQ(config.total_budget, 10.0);
    EXPECT_TRUE(config.enforce_budget);
}

//=============================================================================
// Mathematical Correctness Tests
//=============================================================================

TEST_F(SecurityMathRegressionTest, EntropyMathematicalCorrectness)
{
    // Test 1: Uniform distribution should have maximum entropy
    std::vector<uint8_t> uniform(256);
    for (int i = 0; i < 256; i++) uniform[i] = i;
    double uniform_entropy = nimcp_entropy_calculate(uniform.data(), uniform.size());
    EXPECT_NEAR(uniform_entropy, 8.0, 0.001);  // H = log2(256) = 8

    // Test 2: Single value should have zero entropy
    std::vector<uint8_t> constant(1000, 42);
    double constant_entropy = nimcp_entropy_calculate(constant.data(), constant.size());
    EXPECT_NEAR(constant_entropy, 0.0, 0.001);

    // Test 3: Binary alphabet should have H <= 1
    std::vector<uint8_t> binary(1000);
    for (size_t i = 0; i < binary.size(); i++) {
        binary[i] = i % 2;
    }
    double binary_entropy = nimcp_entropy_calculate(binary.data(), binary.size());
    EXPECT_NEAR(binary_entropy, 1.0, 0.001);  // H = log2(2) = 1

    // Test 4: Mutual information I(X;X) = H(X)
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; i++) data[i] = i;
    double h_x = nimcp_entropy_calculate(data.data(), data.size());
    double mi_xx = nimcp_entropy_mutual_information(data.data(), data.data(), data.size());
    EXPECT_NEAR(mi_xx, h_x, 0.001);

    // Test 5: Joint entropy H(X,Y) >= max(H(X), H(Y))
    std::vector<uint8_t> data1(256), data2(256);
    for (int i = 0; i < 256; i++) {
        data1[i] = i;
        data2[i] = 255 - i;
    }
    double h_joint = nimcp_entropy_joint(data1.data(), data2.data(), data1.size());
    double h1 = nimcp_entropy_calculate(data1.data(), data1.size());
    double h2 = nimcp_entropy_calculate(data2.data(), data2.size());
    EXPECT_GE(h_joint, std::max(h1, h2) - 0.001);
}

TEST_F(SecurityMathRegressionTest, BayesianTrustMathematicalCorrectness)
{
    ASSERT_EQ(nimcp_trust_register_entity(trust_network, 1, "test"), NIMCP_SUCCESS);

    // Test 1: Prior should be (1,1) with E[Trust] = 0.5
    nimcp_trust_score_t initial;
    ASSERT_EQ(nimcp_trust_get_score(trust_network, 1, &initial), NIMCP_SUCCESS);
    EXPECT_NEAR(initial.expected_trust, 0.5, 0.001);
    EXPECT_EQ(initial.alpha, 1.0);
    EXPECT_EQ(initial.beta, 1.0);

    // Test 2: After one success: (2,1), E = 2/3
    nimcp_trust_record_interaction(trust_network, 1, true, 1.0);
    nimcp_trust_score_t after_success;
    ASSERT_EQ(nimcp_trust_get_score(trust_network, 1, &after_success), NIMCP_SUCCESS);
    EXPECT_NEAR(after_success.expected_trust, 2.0/3.0, 0.001);
    EXPECT_EQ(after_success.alpha, 2.0);
    EXPECT_EQ(after_success.beta, 1.0);

    // Test 3: After one failure: (2,2), E = 0.5
    nimcp_trust_record_interaction(trust_network, 1, false, 1.0);
    nimcp_trust_score_t after_failure;
    ASSERT_EQ(nimcp_trust_get_score(trust_network, 1, &after_failure), NIMCP_SUCCESS);
    EXPECT_NEAR(after_failure.expected_trust, 0.5, 0.001);

    // Test 4: P(Trust >= threshold) should be consistent
    double prob_above_05 = nimcp_trust_probability_above(&after_failure, 0.5);
    // For symmetric Beta(2,2), P(X >= 0.5) should be 0.5
    EXPECT_NEAR(prob_above_05, 0.5, 0.05);
}

TEST_F(SecurityMathRegressionTest, DifferentialPrivacyMathematicalCorrectness)
{
    // Test Laplace noise properties with many samples
    const int num_samples = 10000;
    double sum = 0;
    double sum_sq = 0;
    const double sensitivity = 1.0;

    for (int i = 0; i < num_samples; i++) {
        double noise = nimcp_random_laplace(sensitivity);
        sum += noise;
        sum_sq += noise * noise;
    }

    // Mean should be ~0
    double mean = sum / num_samples;
    EXPECT_NEAR(mean, 0.0, 0.1);

    // Variance should be 2*b^2 = 2 for b=1
    double variance = (sum_sq / num_samples) - (mean * mean);
    EXPECT_NEAR(variance, 2.0, 0.3);

    // Test Gaussian noise properties
    sum = 0;
    sum_sq = 0;
    const double stddev = 2.0;

    for (int i = 0; i < num_samples; i++) {
        double noise = nimcp_random_gaussian(0.0, stddev);
        sum += noise;
        sum_sq += noise * noise;
    }

    mean = sum / num_samples;
    EXPECT_NEAR(mean, 0.0, 0.2);

    variance = (sum_sq / num_samples) - (mean * mean);
    EXPECT_NEAR(variance, stddev * stddev, 0.5);
}

TEST_F(SecurityMathRegressionTest, BetaIncompleteCorrectness)
{
    // I_0(a,b) = 0 for all a,b > 0
    EXPECT_NEAR(nimcp_beta_incomplete(0.0, 1.0, 1.0), 0.0, 0.001);

    // I_1(a,b) = 1 for all a,b > 0
    EXPECT_NEAR(nimcp_beta_incomplete(1.0, 1.0, 1.0), 1.0, 0.001);

    // For Beta(1,1) = Uniform, I_x(1,1) = x
    EXPECT_NEAR(nimcp_beta_incomplete(0.5, 1.0, 1.0), 0.5, 0.001);
    EXPECT_NEAR(nimcp_beta_incomplete(0.25, 1.0, 1.0), 0.25, 0.001);

    // For symmetric Beta(a,a), I_0.5(a,a) = 0.5
    EXPECT_NEAR(nimcp_beta_incomplete(0.5, 2.0, 2.0), 0.5, 0.01);
    EXPECT_NEAR(nimcp_beta_incomplete(0.5, 5.0, 5.0), 0.5, 0.01);
}

TEST_F(SecurityMathRegressionTest, SafeLog2Correctness)
{
    EXPECT_NEAR(nimcp_safe_log2(1.0), 0.0, 0.001);
    EXPECT_NEAR(nimcp_safe_log2(2.0), 1.0, 0.001);
    EXPECT_NEAR(nimcp_safe_log2(8.0), 3.0, 0.001);
    EXPECT_NEAR(nimcp_safe_log2(256.0), 8.0, 0.001);

    // Safe for invalid inputs
    EXPECT_EQ(nimcp_safe_log2(0.0), 0.0);
    EXPECT_EQ(nimcp_safe_log2(-1.0), 0.0);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(SecurityMathRegressionTest, EntropyCalculationPerformance)
{
    std::vector<uint8_t> data(4096);
    std::mt19937 rng(42);
    for (auto& b : data) b = rng() % 256;

    double avg_time = measure_time_ms([&]() {
        nimcp_entropy_calculate(data.data(), data.size());
    }, 1000);

    // Should be fast: < 0.1ms per 4KB analysis
    EXPECT_LT(avg_time, 0.1);
}

TEST_F(SecurityMathRegressionTest, TrustUpdatePerformance)
{
    ASSERT_EQ(nimcp_trust_register_entity(trust_network, 1, "perf_test"), NIMCP_SUCCESS);

    double avg_time = measure_time_ms([&]() {
        nimcp_trust_record_interaction(trust_network, 1, true, 1.0);
    }, 10000);

    // Should be very fast: < 0.01ms per update
    EXPECT_LT(avg_time, 0.01);
}

TEST_F(SecurityMathRegressionTest, DPLaplaceNoisePerformance)
{
    double avg_time = measure_time_ms([&]() {
        nimcp_dp_result_t result;
        nimcp_dp_add_laplace_noise(dp_context, 100.0, 1.0, &result);
    }, 10000);

    // Should be fast: < 0.01ms per query
    EXPECT_LT(avg_time, 0.01);
}

TEST_F(SecurityMathRegressionTest, TrustPropagationPerformance)
{
    // Setup: 100 entities with voucher chains
    for (int i = 0; i < 100; i++) {
        nimcp_trust_register_entity(trust_network, i, ("entity_" + std::to_string(i)).c_str());
        if (i > 0) {
            nimcp_trust_add_voucher(trust_network, i - 1, i);
        }
        for (int j = 0; j < 5; j++) {
            nimcp_trust_record_interaction(trust_network, i, true, 1.0);
        }
    }

    double prop_time = measure_time_ms([&]() {
        nimcp_trust_propagate(trust_network);
    }, 10);

    // Should complete in reasonable time: < 100ms for 100 entities
    EXPECT_LT(prop_time, 100);
}

//=============================================================================
// Determinism and Reproducibility Tests
//=============================================================================

TEST_F(SecurityMathRegressionTest, EntropyDeterminism)
{
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Same input should always produce same entropy
    double entropy1 = nimcp_entropy_calculate(data.data(), data.size());
    double entropy2 = nimcp_entropy_calculate(data.data(), data.size());
    double entropy3 = nimcp_entropy_calculate(data.data(), data.size());

    EXPECT_EQ(entropy1, entropy2);
    EXPECT_EQ(entropy2, entropy3);
}

TEST_F(SecurityMathRegressionTest, TrustScoreDeterminism)
{
    // Same sequence of interactions should produce same trust
    auto run_trust_sequence = [](nimcp_trust_network_t* net) -> double {
        nimcp_trust_register_entity(net, 99, "determinism_test");
        nimcp_trust_record_interaction(net, 99, true, 1.0);
        nimcp_trust_record_interaction(net, 99, true, 1.0);
        nimcp_trust_record_interaction(net, 99, false, 1.0);

        nimcp_trust_score_t score;
        nimcp_trust_get_score(net, 99, &score);
        return score.expected_trust;
    };

    // Run on fresh networks
    nimcp_trust_network_t* net1 = nimcp_trust_create();
    nimcp_trust_init(net1, nullptr);
    double trust1 = run_trust_sequence(net1);
    nimcp_trust_destroy(net1);

    nimcp_trust_network_t* net2 = nimcp_trust_create();
    nimcp_trust_init(net2, nullptr);
    double trust2 = run_trust_sequence(net2);
    nimcp_trust_destroy(net2);

    EXPECT_EQ(trust1, trust2);
}

//=============================================================================
// Known Attack Pattern Detection (Regression Guards)
//=============================================================================

TEST_F(SecurityMathRegressionTest, DetectEncryptedDataPattern)
{
    // Encrypted/compressed data should have high entropy (> 7.5)
    // Note: Random data with 1024 bytes may not achieve perfect 8.0 entropy
    std::vector<uint8_t> encrypted(4096);  // Larger sample for better statistics
    std::mt19937 rng(12345);  // Fixed seed for reproducibility
    for (auto& b : encrypted) b = rng() % 256;

    double entropy = nimcp_entropy_calculate(encrypted.data(), encrypted.size());
    EXPECT_GT(entropy, 7.5);  // High entropy indicates potential encryption

    nimcp_entropy_result_t result;
    nimcp_entropy_analyze(entropy_analyzer, encrypted.data(), encrypted.size(), &result);

    // Analysis should confirm high entropy
    EXPECT_GT(result.entropy, 7.5);
}

TEST_F(SecurityMathRegressionTest, DetectCodePattern)
{
    // Typical code has entropy around 3.5-6.5 depending on structure
    // Repeating patterns with limited alphabet produce lower entropy
    std::vector<uint8_t> code_like(1024);
    for (size_t i = 0; i < code_like.size(); i++) {
        // Simulate code-like distribution (limited alphabet, some repetition)
        code_like[i] = "void main() { int x = 0; return x; }\n"[i % 37];
    }

    double entropy = nimcp_entropy_calculate(code_like.data(), code_like.size());
    // Code-like patterns have moderate entropy - not as high as random, not as low as constant
    EXPECT_GT(entropy, 3.0);
    EXPECT_LT(entropy, 7.0);
}

TEST_F(SecurityMathRegressionTest, DetectTamperingViaEntropyShift)
{
    // This test verifies that we can detect significant entropy changes
    // by directly comparing entropy values (baseline-based detection
    // requires multiple samples for stddev calculation)

    // Baseline: code-like entropy
    std::vector<uint8_t> baseline(1024);
    for (size_t i = 0; i < baseline.size(); i++) {
        baseline[i] = "function test() { console.log('hello'); }\n"[i % 42];
    }

    double baseline_entropy = nimcp_entropy_calculate(baseline.data(), baseline.size());

    // Tampered: replace with encrypted data
    std::vector<uint8_t> tampered(1024);
    std::mt19937 rng(99999);
    for (auto& b : tampered) b = rng() % 256;

    double tampered_entropy = nimcp_entropy_calculate(tampered.data(), tampered.size());

    // Significant entropy increase indicates tampering
    // Baseline should be around 3-4, tampered should be around 7.8
    EXPECT_GT(tampered_entropy - baseline_entropy, 3.0);  // Large entropy shift
    EXPECT_GT(tampered_entropy, 7.0);  // High entropy
    EXPECT_LT(baseline_entropy, 5.0);  // Code-like entropy
}

}  // namespace
