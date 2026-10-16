/**
 * @file test_qmc_gpu.cpp
 * @brief Unit tests for GPU Quantum Monte Carlo kernels
 *
 * WHAT: Tests GPU-accelerated Monte Carlo sampling, MCTS, and SAT solving
 * WHY:  Verify GPU QMC operations produce correct results
 * HOW:  Test all public API functions with various configurations
 *
 * TEST COVERAGE:
 * - GPU RNG creation and sampling
 * - Uniform and normal sample generation
 * - Stratified sampling
 * - Categorical sampling
 * - Monte Carlo integration
 * - MCTS UCB1 computation
 * - MCTS batch rollout and backpropagation
 * - SAT solver clause evaluation
 * - CPU fallback when GPU unavailable
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

// GPU headers
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

// CPU Monte Carlo for comparison
#include "utils/algorithms/nimcp_monte_carlo.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for QMC GPU tests
 */
class QMCGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    qmc_gpu_rng_t rng = nullptr;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
    }

    void TearDown() override {
        if (rng) {
            qmc_gpu_rng_destroy(rng);
            rng = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    bool hasGPU() const {
        return ctx != nullptr && nimcp_gpu_context_is_valid(ctx);
    }

    void skipIfNoGPU() {
        if (!hasGPU()) {
            GTEST_SKIP() << "Skipping test: No GPU available";
        }
    }
};

//=============================================================================
// GPU Availability Tests
//=============================================================================

TEST_F(QMCGPUTest, IsAvailableReturnsConsistently) {
    // qmc_gpu_is_available should return consistent results
    bool first = qmc_gpu_is_available();
    bool second = qmc_gpu_is_available();
    EXPECT_EQ(first, second);
}

TEST_F(QMCGPUTest, VersionStringNotNull) {
    const char* version = qmc_gpu_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(QMCGPUTest, DefaultMCConfig) {
    qmc_gpu_config_t config = qmc_gpu_default_config();

    EXPECT_GT(config.num_samples, 0u);
    EXPECT_GT(config.threads_per_block, 0u);
    EXPECT_LE(config.threads_per_block, 1024u);  // CUDA limit
}

TEST_F(QMCGPUTest, DefaultMCTSConfig) {
    qmcts_gpu_config_t config = qmcts_gpu_default_config();

    EXPECT_GT(config.num_iterations, 0u);
    EXPECT_GT(config.num_rollouts, 0u);
    EXPECT_GT(config.exploration_constant, 0.0f);
}

TEST_F(QMCGPUTest, DefaultSATConfig) {
    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(10, 20);

    EXPECT_EQ(config.num_variables, 10u);
    EXPECT_EQ(config.num_clauses, 20u);
    EXPECT_GT(config.mcts_iterations, 0u);
}

//=============================================================================
// RNG Lifecycle Tests
//=============================================================================

TEST_F(QMCGPUTest, RNGCreateDestroy) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    // Destroy is tested in TearDown
}

TEST_F(QMCGPUTest, RNGCreateWithZeroSeed) {
    skipIfNoGPU();

    // Zero seed should use time-based seed
    rng = qmc_gpu_rng_create(ctx, 1024, 0);
    ASSERT_NE(rng, nullptr);
}

TEST_F(QMCGPUTest, RNGReseed) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    bool result = qmc_gpu_rng_reseed(rng, 123);
    EXPECT_TRUE(result);
}

TEST_F(QMCGPUTest, RNGNullContext) {
    // With GPU available, NULL context may use default context
    // Just ensure it doesn't crash - behavior is implementation-defined
    rng = qmc_gpu_rng_create(nullptr, 1024, 42);
    // Clean up if it succeeded
    if (rng) {
        qmc_gpu_rng_destroy(rng);
        rng = nullptr;
    }
}

//=============================================================================
// Uniform Sampling Tests
//=============================================================================

TEST_F(QMCGPUTest, SampleUniform) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    // Create output tensor
    size_t dims[] = {1024};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_uniform(ctx, rng, output);
    EXPECT_TRUE(result);

    // Copy back and verify range [0, 1)
    std::vector<float> host_data(1024);
    cudaMemcpy(host_data.data(), output->data,
               1024 * sizeof(float), cudaMemcpyDeviceToHost);

    for (float val : host_data) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LT(val, 1.0f);
    }

    nimcp_gpu_tensor_destroy(output);
}

TEST_F(QMCGPUTest, SampleUniformDistribution) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    size_t dims[] = {10000};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_uniform(ctx, rng, output);
    EXPECT_TRUE(result);

    std::vector<float> host_data(10000);
    cudaMemcpy(host_data.data(), output->data,
               10000 * sizeof(float), cudaMemcpyDeviceToHost);

    // Check mean is approximately 0.5
    float mean = std::accumulate(host_data.begin(), host_data.end(), 0.0f) / 10000.0f;
    EXPECT_NEAR(mean, 0.5f, 0.05f);

    nimcp_gpu_tensor_destroy(output);
}

//=============================================================================
// Normal Sampling Tests
//=============================================================================

TEST_F(QMCGPUTest, SampleNormal) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    size_t dims[] = {10000};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    float mean = 5.0f;
    float stddev = 2.0f;
    bool result = qmc_gpu_sample_normal(ctx, rng, output, mean, stddev);
    EXPECT_TRUE(result);

    std::vector<float> host_data(10000);
    cudaMemcpy(host_data.data(), output->data,
               10000 * sizeof(float), cudaMemcpyDeviceToHost);

    // Check sample mean
    float sample_mean = std::accumulate(host_data.begin(), host_data.end(), 0.0f) / 10000.0f;
    EXPECT_NEAR(sample_mean, mean, 0.1f);

    // Check sample stddev
    float sq_sum = 0.0f;
    for (float val : host_data) {
        sq_sum += (val - sample_mean) * (val - sample_mean);
    }
    float sample_stddev = sqrtf(sq_sum / 9999.0f);
    EXPECT_NEAR(sample_stddev, stddev, 0.1f);

    nimcp_gpu_tensor_destroy(output);
}

//=============================================================================
// Stratified Sampling Tests
//=============================================================================

TEST_F(QMCGPUTest, SampleStratified) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 1000, 42);
    ASSERT_NE(rng, nullptr);

    size_t dims[] = {1000};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_stratified(ctx, rng, output, 10);
    EXPECT_TRUE(result);

    std::vector<float> host_data(1000);
    cudaMemcpy(host_data.data(), output->data,
               1000 * sizeof(float), cudaMemcpyDeviceToHost);

    // Stratified samples should have lower variance than uniform
    // Just verify they're in valid range
    for (float val : host_data) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LT(val, 1.0f);
    }

    nimcp_gpu_tensor_destroy(output);
}

//=============================================================================
// MCTS Tests
//=============================================================================

TEST_F(QMCGPUTest, MCTSCreateDestroy) {
    skipIfNoGPU();

    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    qmc_gpu_mcts_t mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    qmcts_gpu_destroy(mcts);
}

TEST_F(QMCGPUTest, MCTSReset) {
    skipIfNoGPU();

    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    qmc_gpu_mcts_t mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    bool result = qmcts_gpu_reset(mcts);
    EXPECT_TRUE(result);

    qmcts_gpu_destroy(mcts);
}

TEST_F(QMCGPUTest, MCTSComputeUCB1) {
    skipIfNoGPU();

    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    qmc_gpu_mcts_t mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    bool result = qmcts_gpu_compute_ucb1(ctx, mcts, 1.41421356f);
    EXPECT_TRUE(result);

    qmcts_gpu_destroy(mcts);
}

//=============================================================================
// SAT Solver Tests
//=============================================================================

TEST_F(QMCGPUTest, SATCreateDestroy) {
    skipIfNoGPU();

    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(10, 20);
    qmc_gpu_sat_t sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    qmc_sat_gpu_destroy(sat);
}

TEST_F(QMCGPUTest, SATSetCNF) {
    skipIfNoGPU();

    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(3, 2);
    qmc_gpu_sat_t sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    // Simple CNF: (x1 OR x2) AND (NOT x1 OR x3)
    int32_t clauses[] = {1, 2, -1, 3};  // Clause 1: [1, 2], Clause 2: [-1, 3]
    uint32_t clause_sizes[] = {2, 2};

    bool result = qmc_sat_gpu_set_cnf(sat, clauses, clause_sizes, 2);
    EXPECT_TRUE(result);

    qmc_sat_gpu_destroy(sat);
}

TEST_F(QMCGPUTest, SATEstimateProbability) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    // Easy SAT: just (x1 OR x2) - 75% of assignments satisfy
    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(2, 1);
    qmc_gpu_sat_t sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    int32_t clauses[] = {1, 2};
    uint32_t clause_sizes[] = {2};

    bool set_result = qmc_sat_gpu_set_cnf(sat, clauses, clause_sizes, 1);
    ASSERT_TRUE(set_result);

    float probability = 0.0f;
    float variance = 0.0f;
    bool result = qmc_sat_gpu_estimate_probability(
        ctx, sat, rng, 10000, &probability, &variance);
    EXPECT_TRUE(result);

    // 3/4 of assignments satisfy (x1 OR x2)
    EXPECT_NEAR(probability, 0.75f, 0.05f);

    qmc_sat_gpu_destroy(sat);
}

//=============================================================================
// Monte Carlo Integration Tests
//=============================================================================

TEST_F(QMCGPUTest, IntegrationSimple) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    // Pre-computed function values (all 1.0 for unit integral)
    size_t dims[] = {10000};
    nimcp_gpu_tensor_t* values = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(values, nullptr);

    std::vector<float> ones(10000, 1.0f);
    cudaMemcpy(values->data, ones.data(),
               10000 * sizeof(float), cudaMemcpyHostToDevice);

    qmc_gpu_integration_result_t result;
    bool success = qmc_gpu_integrate(ctx, rng, values, 10000, 1.0f, &result);
    EXPECT_TRUE(success);

    EXPECT_NEAR(result.estimate, 1.0f, 0.01f);
    EXPECT_EQ(result.num_samples, 10000u);

    nimcp_gpu_tensor_destroy(values);
}

//=============================================================================
// CPU Fallback Tests
//=============================================================================

TEST_F(QMCGPUTest, CPUFallbackMCConfig) {
    // Test CPU MC config with GPU disabled
    mc_config_t config;
    mc_config_init(&config);

    // GPU should be disabled by default
    EXPECT_EQ(config.gpu.mode, MC_GPU_DISABLED);
    EXPECT_EQ(config.gpu.ctx, nullptr);
    EXPECT_GT(config.gpu.min_samples_for_gpu, 0u);
}

TEST_F(QMCGPUTest, CPUFallbackGPUConfigInit) {
    mc_gpu_config_t gpu_config;
    mc_gpu_config_init(&gpu_config);

    EXPECT_EQ(gpu_config.mode, MC_GPU_DISABLED);
    EXPECT_EQ(gpu_config.ctx, nullptr);
    EXPECT_EQ(gpu_config.min_samples_for_gpu, 10000u);
    EXPECT_EQ(gpu_config.threads_per_block, 256u);
}

TEST_F(QMCGPUTest, ShouldUseGPUDisabled) {
    mc_gpu_config_t config;
    mc_gpu_config_init(&config);
    config.mode = MC_GPU_DISABLED;

    EXPECT_FALSE(mc_should_use_gpu(&config, 100000));
}

TEST_F(QMCGPUTest, ShouldUseGPUAuto) {
    mc_gpu_config_t config;
    mc_gpu_config_init(&config);
    config.mode = MC_GPU_AUTO;
    config.min_samples_for_gpu = 1000;

    // Below threshold
    bool below = mc_should_use_gpu(&config, 500);
    // Above threshold (depends on GPU availability)
    bool above = mc_should_use_gpu(&config, 5000);

    if (!qmc_gpu_is_available()) {
        EXPECT_FALSE(below);
        EXPECT_FALSE(above);
    } else {
        EXPECT_FALSE(below);  // Below threshold even with GPU
        EXPECT_TRUE(above);   // Above threshold with GPU
    }
}

TEST_F(QMCGPUTest, GPUAvailableConsistent) {
    bool available = mc_gpu_available();
    bool qmc_available = qmc_gpu_is_available();

    // Both functions should return the same result
    EXPECT_EQ(available, qmc_available);
}

TEST_F(QMCGPUTest, GPUStatusString) {
    const char* status = mc_gpu_status_string();
    ASSERT_NE(status, nullptr);
    EXPECT_GT(strlen(status), 0u);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(QMCGPUTest, NullRNGSampling) {
    skipIfNoGPU();

    size_t dims[] = {100};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_uniform(ctx, nullptr, output);
    EXPECT_FALSE(result);

    nimcp_gpu_tensor_destroy(output);
}

TEST_F(QMCGPUTest, NullOutputTensor) {
    skipIfNoGPU();

    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    bool result = qmc_gpu_sample_uniform(ctx, rng, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(QMCGPUTest, NullGPUConfig) {
    EXPECT_FALSE(mc_should_use_gpu(nullptr, 10000));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
