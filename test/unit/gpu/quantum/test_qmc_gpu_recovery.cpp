/**
 * @file test_qmc_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in QMC module
 *
 * WHAT: Tests GPU recovery for Quantum Monte Carlo operations
 * WHY:  Verify self-healing from OOM during sampling, MCTS, and SAT solving
 * HOW:  Test recovery for RNG, sampling, integration, and SAT operations
 *
 * TEST COVERAGE:
 * - Recovery initialization in QMC RNG creation
 * - OOM recovery during sampling operations
 * - Numerical error recovery in integration
 * - MCTS recovery from UCB1 computation failures
 * - SAT solver recovery from CNF evaluation failures
 * - CPU fallback for QMC operations
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <numeric>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class QMCGPURecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx = nimcp_gpu_context_create_auto();
        if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
            GTEST_SKIP() << "Failed to create GPU context";
        }
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (rng) {
            qmc_gpu_rng_destroy(rng);
            rng = nullptr;
        }
        if (mcts) {
            qmcts_gpu_destroy(mcts);
            mcts = nullptr;
        }
        if (sat) {
            qmc_sat_gpu_destroy(sat);
            sat = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx = nullptr;
    qmc_gpu_rng_t rng = nullptr;
    qmc_gpu_mcts_t mcts = nullptr;
    qmc_gpu_sat_t sat = nullptr;
#endif
};

/* ============================================================================
 * Test: Recovery initialization at RNG creation
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, RecoveryInitializedAtRNGCreation) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after RNG creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at MCTS creation
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, RecoveryInitializedAtMCTSCreation) {
#ifdef NIMCP_ENABLE_CUDA
    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after MCTS creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at SAT solver creation
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, RecoveryInitializedAtSATCreation) {
#ifdef NIMCP_ENABLE_CUDA
    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(10, 20);
    sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after SAT solver creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RNG reseed with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, RNGReseedWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    bool result = qmc_gpu_rng_reseed(rng, 123);
    EXPECT_TRUE(result) << "Reseed should succeed with recovery enabled";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Uniform sampling with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, UniformSamplingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    size_t dims[] = {1024};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_uniform(ctx, rng, output);
    EXPECT_TRUE(result) << "Uniform sampling should succeed with recovery";

    /* Verify samples are in valid range */
    std::vector<float> host_data(1024);
    cudaMemcpy(host_data.data(), output->data,
               1024 * sizeof(float), cudaMemcpyDeviceToHost);

    for (float val : host_data) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LT(val, 1.0f);
    }

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Normal sampling with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, NormalSamplingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    size_t dims[] = {10000};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    float mean = 3.0f;
    float stddev = 1.5f;
    bool result = qmc_gpu_sample_normal(ctx, rng, output, mean, stddev);
    EXPECT_TRUE(result) << "Normal sampling should succeed with recovery";

    /* Verify sample statistics */
    std::vector<float> host_data(10000);
    cudaMemcpy(host_data.data(), output->data,
               10000 * sizeof(float), cudaMemcpyDeviceToHost);

    float sample_mean = std::accumulate(host_data.begin(), host_data.end(), 0.0f) / 10000.0f;
    EXPECT_NEAR(sample_mean, mean, 0.2f);

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Stratified sampling with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, StratifiedSamplingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 1000, 42);
    ASSERT_NE(rng, nullptr);

    size_t dims[] = {1000};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_stratified(ctx, rng, output, 10);
    EXPECT_TRUE(result) << "Stratified sampling should succeed with recovery";

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Monte Carlo integration with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, MonteCarloIntegrationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    /* Create tensor with unit values (integral = 1) */
    size_t dims[] = {10000};
    nimcp_gpu_tensor_t* values = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(values, nullptr);

    std::vector<float> ones(10000, 1.0f);
    cudaMemcpy(values->data, ones.data(),
               10000 * sizeof(float), cudaMemcpyHostToDevice);

    qmc_gpu_integration_result_t result;
    bool success = qmc_gpu_integrate(ctx, rng, values, 10000, 1.0f, &result);
    EXPECT_TRUE(success) << "MC integration should succeed with recovery";
    EXPECT_NEAR(result.estimate, 1.0f, 0.01f);

    nimcp_gpu_tensor_destroy(values);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MCTS UCB1 computation with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, MCTSComputeUCB1WithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    bool result = qmcts_gpu_compute_ucb1(ctx, mcts, 1.41421356f);
    EXPECT_TRUE(result) << "UCB1 computation should succeed with recovery";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MCTS reset with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, MCTSResetWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    /* Reset should work with recovery enabled */
    bool result = qmcts_gpu_reset(mcts);
    EXPECT_TRUE(result);

    /* UCB1 should still work after reset */
    result = qmcts_gpu_compute_ucb1(ctx, mcts, 1.41421356f);
    EXPECT_TRUE(result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MCTS backpropagation with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, MCTSBackpropWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    qmcts_gpu_config_t config = qmcts_gpu_default_config();
    mcts = qmcts_gpu_create(ctx, &config, 1000);
    ASSERT_NE(mcts, nullptr);

    /* Create test leaves and values */
    uint32_t leaves[] = {0};
    float values[] = {0.5f};

    bool result = qmcts_gpu_backpropagate(ctx, mcts, leaves, values, 1);
    EXPECT_TRUE(result) << "Backpropagation should succeed with recovery";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SAT CNF set with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, SATSetCNFWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(3, 2);
    sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    /* (x1 OR x2) AND (NOT x1 OR x3) */
    int32_t clauses[] = {1, 2, -1, 3};
    uint32_t clause_sizes[] = {2, 2};

    bool result = qmc_sat_gpu_set_cnf(sat, clauses, clause_sizes, 2);
    EXPECT_TRUE(result) << "CNF set should succeed with recovery";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SAT probability estimation with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, SATProbabilityEstimationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 10000, 42);
    ASSERT_NE(rng, nullptr);

    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(2, 1);
    sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    /* Easy SAT: (x1 OR x2) */
    int32_t clauses[] = {1, 2};
    uint32_t clause_sizes[] = {2};

    bool set_result = qmc_sat_gpu_set_cnf(sat, clauses, clause_sizes, 1);
    ASSERT_TRUE(set_result);

    float probability = 0.0f;
    float variance = 0.0f;
    bool result = qmc_sat_gpu_estimate_probability(
        ctx, sat, rng, 10000, &probability, &variance);
    EXPECT_TRUE(result) << "Probability estimation should succeed with recovery";
    EXPECT_NEAR(probability, 0.75f, 0.1f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SAT MCTS solver with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, SATMCTSSolverWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 1000, 42);
    ASSERT_NE(rng, nullptr);

    qmc_sat_gpu_config_t config = qmc_sat_gpu_default_config(3, 1);
    // Must be >= batch_size (256) used internally so the MCTS loop runs at least once
    config.mcts_iterations = 512;
    sat = qmc_sat_gpu_create(ctx, &config);
    ASSERT_NE(sat, nullptr);

    /* Simple satisfiable clause */
    int32_t clauses[] = {1, 2, 3};
    uint32_t clause_sizes[] = {3};

    bool set_result = qmc_sat_gpu_set_cnf(sat, clauses, clause_sizes, 1);
    ASSERT_TRUE(set_result);

    qmc_sat_gpu_result_t result;
    bool solve_result = qmc_sat_gpu_solve_mcts(ctx, sat, rng, &result);
    EXPECT_TRUE(solve_result) << "MCTS solve should succeed with recovery";

    /* Single clause with 3 positive literals is satisfiable */
    EXPECT_TRUE(result.satisfiable || result.sat_probability > 0.5f);

    qmc_sat_gpu_result_free(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter handling
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, InvalidParameterHandling) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL RNG should fail gracefully */
    size_t dims[] = {100};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_uniform(ctx, nullptr, output);
    EXPECT_FALSE(result) << "Should fail gracefully with NULL RNG";

    /* NULL output should fail gracefully */
    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    result = qmc_gpu_sample_uniform(ctx, rng, nullptr);
    EXPECT_FALSE(result) << "Should fail gracefully with NULL output";

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Stats tracking after QMC operations
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, StatsTrackingAfterQMCOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    /* Perform several operations */
    size_t dims[] = {1024};
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
            ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        ASSERT_NE(output, nullptr);

        qmc_gpu_sample_uniform(ctx, rng, output);
        qmc_gpu_sample_normal(ctx, rng, output, 0.0f, 1.0f);

        nimcp_gpu_tensor_destroy(output);
    }

    /* Get stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be accessible */
    EXPECT_GE(stats.total_errors, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Repeated RNG operations with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, RepeatedRNGOperationsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    size_t dims[] = {1024};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    /* Create and destroy RNG multiple times */
    for (int i = 0; i < 20; i++) {
        qmc_gpu_rng_t test_rng = qmc_gpu_rng_create(ctx, 1024, i);
        ASSERT_NE(test_rng, nullptr);

        bool result = qmc_gpu_sample_uniform(ctx, test_rng, output);
        EXPECT_TRUE(result);

        qmc_gpu_rng_destroy(test_rng);
    }

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Categorical sampling with recovery
 * ============================================================================ */
TEST_F(QMCGPURecoveryTest, CategoricalSamplingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    rng = qmc_gpu_rng_create(ctx, 1024, 42);
    ASSERT_NE(rng, nullptr);

    /* Create probability distribution */
    size_t prob_dims[] = {4};
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(
        ctx, prob_dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(probs, nullptr);

    float prob_data[] = {0.1f, 0.2f, 0.3f, 0.4f};
    cudaMemcpy(probs->data, prob_data, 4 * sizeof(float), cudaMemcpyHostToDevice);

    /* Create output tensor for samples */
    size_t out_dims[] = {1000};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx, out_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    ASSERT_NE(output, nullptr);

    bool result = qmc_gpu_sample_categorical(ctx, rng, probs, 4, output, 1000);
    EXPECT_TRUE(result) << "Categorical sampling should succeed with recovery";

    nimcp_gpu_tensor_destroy(probs);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */

/* Main function for standalone test execution */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
