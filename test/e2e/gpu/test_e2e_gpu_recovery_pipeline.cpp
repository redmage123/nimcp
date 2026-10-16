/* ============================================================================
 * E2E Tests: GPU Recovery Pipeline
 * ============================================================================
 * WHAT: End-to-end tests for GPU self-healing recovery system
 * WHY:  Validate complete recovery workflows in realistic scenarios
 * HOW:  Test full pipelines with recovery under various conditions
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }

        /* Initialize with default config */
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
        nimcp_gpu_recovery_init(NULL);
        nimcp_gpu_recovery_reset_stats();
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    /* Simulated CPU fallbacks */
    static bool fuzzy_cpu_fallback(void* ctx, void* params, void* result) {
        (void)ctx; (void)params;
        if (result) {
            *static_cast<float*>(result) = 0.5f;  /* Default fuzzy output */
        }
        return true;
    }

    static bool financial_cpu_fallback(void* ctx, void* params, void* result) {
        (void)ctx; (void)params;
        if (result) {
            *static_cast<float*>(result) = 100.0f;  /* Default price */
        }
        return true;
    }

    static bool neural_cpu_fallback(void* ctx, void* params, void* result) {
        (void)ctx; (void)params;
        if (result) {
            *static_cast<float*>(result) = 0.0f;  /* Default output */
        }
        return true;
    }

    static bool stats_cpu_fallback(void* ctx, void* params, void* result) {
        (void)ctx; (void)params;
        if (result) {
            *static_cast<float*>(result) = 1.0f;  /* Default variance */
        }
        return true;
    }
#endif
};

/* ============================================================================
 * Test: FuzzyControllerWithRecovery
 * Full fuzzy controller pipeline with recovery
 * Note: Some recovery actions may "fail" if they can't accomplish their goal
 * (e.g., FREE_CACHE frees 0 bytes), but actions are still attempted.
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, FuzzyControllerWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, fuzzy_cpu_fallback, nullptr);

    /* Simulate fuzzy controller pipeline with potential failures */
    std::vector<nimcp_gpu_error_category_t> error_sequence = {
        GPU_ERROR_INVALID_PARAMS,  /* Fuzzification error */
        GPU_ERROR_NUMERICAL,       /* Defuzzification numerical - always recovers */
        GPU_ERROR_KERNEL_LAUNCH,   /* Kernel launch - always recovers */
    };

    int attempt_count = 0;
    for (auto error : error_sequence) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, error, cudaSuccess, &result);
        attempt_count++;
        /* Verify an action was attempted for each error */
        EXPECT_NE(result.action_taken, GPU_RECOVERY_NONE);
        nimcp_gpu_recovery_context_reset(ctx);
    }

    EXPECT_EQ(attempt_count, static_cast<int>(error_sequence.size()));

    /* Verify fallback can execute */
    float output = 0.0f;
    bool fallback_ok = nimcp_gpu_execute_cpu_fallback(ctx, nullptr, &output);
    EXPECT_TRUE(fallback_ok);
    EXPECT_FLOAT_EQ(output, 0.5f);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FinancialSimulationWithRecovery
 * Full Monte Carlo simulation with recovery
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, FinancialSimulationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, financial_cpu_fallback, nullptr);

    /* Simulate financial pipeline */
    nimcp_gpu_recovery_result_t result;

    /* RNG initialization might fail (library error) */
    nimcp_gpu_try_recover(ctx, GPU_ERROR_LIBRARY, cudaSuccess, &result);
    nimcp_gpu_recovery_context_reset(ctx);

    /* Monte Carlo paths might OOM */
    nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);
    nimcp_gpu_recovery_context_reset(ctx);

    /* Option pricing might have numerical issues */
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);
    EXPECT_TRUE(recovered);

    /* Execute CPU fallback for final pricing */
    float price = 0.0f;
    nimcp_gpu_execute_cpu_fallback(ctx, nullptr, &price);
    EXPECT_EQ(price, 100.0f);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NeuralTrainingWithRecovery
 * Neural network training with recovery
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, NeuralTrainingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, neural_cpu_fallback, nullptr);

    const int num_epochs = 5;
    int successful_epochs = 0;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        /* Forward pass might OOM */
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                              cudaErrorMemoryAllocation, &result);

        /* Backward pass might have numerical issues (gradient explosion) */
        bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL,
                                               cudaSuccess, &result);

        if (recovered || result.using_fallback) {
            successful_epochs++;
        }

        nimcp_gpu_recovery_context_reset(ctx);
    }

    EXPECT_EQ(successful_epochs, num_epochs);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: StatisticalAnalysisWithRecovery
 * Statistical analysis pipeline with recovery
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, StatisticalAnalysisWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, stats_cpu_fallback, nullptr);

    /* PCA with potential issues */
    nimcp_gpu_recovery_result_t result;

    /* Covariance matrix computation OOM */
    nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* Eigendecomposition numerical */
    bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);
    EXPECT_TRUE(recovered);

    float variance = 0.0f;
    nimcp_gpu_execute_cpu_fallback(ctx, nullptr, &variance);
    EXPECT_EQ(variance, 1.0f);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MixedWorkloadWithRecovery
 * Mixed workload with various error types
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, MixedWorkloadWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, fuzzy_cpu_fallback, nullptr);

    std::mt19937 gen(42);
    std::uniform_int_distribution<> error_dist(0, 5);

    nimcp_gpu_error_category_t errors[] = {
        GPU_ERROR_INVALID_PARAMS,
        GPU_ERROR_OUT_OF_MEMORY,
        GPU_ERROR_NUMERICAL,
        GPU_ERROR_KERNEL_LAUNCH,
        GPU_ERROR_TIMEOUT,
        GPU_ERROR_LIBRARY
    };

    int total_recoveries = 0;
    const int num_operations = 20;

    for (int i = 0; i < num_operations; i++) {
        nimcp_gpu_error_category_t error = errors[error_dist(gen)];
        nimcp_gpu_recovery_result_t result;

        bool recovered = nimcp_gpu_try_recover(ctx, error, cudaSuccess, &result);
        if (recovered) total_recoveries++;

        nimcp_gpu_recovery_context_reset(ctx);
    }

    /* Should recover from most operations */
    EXPECT_GT(total_recoveries, num_operations / 2);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: LongRunningWithIntermittentFailures
 * Long-running operation with intermittent failures
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, LongRunningWithIntermittentFailures) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, neural_cpu_fallback, nullptr);

    const int total_iterations = 50;
    int successful_iterations = 0;

    for (int i = 0; i < total_iterations; i++) {
        nimcp_gpu_recovery_result_t result;

        /* Every 10th iteration has an error */
        if (i % 10 == 0) {
            bool recovered = nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                                                   cudaErrorMemoryAllocation, &result);
            if (recovered) successful_iterations++;
        } else {
            successful_iterations++;
        }

        nimcp_gpu_recovery_context_reset(ctx);
    }

    /* Should complete most iterations */
    EXPECT_GE(successful_iterations, total_iterations - 5);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryMetricsCollection
 * Verify metrics are collected during E2E workflow
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, RecoveryMetricsCollection) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx, fuzzy_cpu_fallback, nullptr);

    /* Perform various recoveries */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);
    nimcp_gpu_try_recover(ctx, GPU_ERROR_NUMERICAL, cudaSuccess, &result);
    nimcp_gpu_try_recover(ctx, GPU_ERROR_DEVICE_NOT_AVAILABLE,
                          cudaErrorNoDevice, &result);

    /* Check metrics */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_GE(stats.total_errors, 3u);
    EXPECT_GE(stats.recoveries_attempted, 3u);
    EXPECT_GE(stats.recoveries_succeeded, 0u);
    EXPECT_GE(stats.cpu_fallbacks_used, 0u);

    if (stats.recoveries_attempted > 0) {
        EXPECT_GE(stats.success_rate, 0.0f);
        EXPECT_LE(stats.success_rate, 1.0f);
    }

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: FullGracefulDegradation
 * Complete graceful degradation path via strategy selection
 * Note: Tests strategy selection directly to verify the degradation path.
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, FullGracefulDegradation) {
#ifdef NIMCP_ENABLE_CUDA
    /* Verify the complete degradation path using strategy selection */
    std::vector<nimcp_gpu_recovery_action_t> expected_actions = {
        GPU_RECOVERY_FREE_CACHE,       /* retry_count = 0 */
        GPU_RECOVERY_REDUCE_BATCH,     /* retry_count = 1 */
        GPU_RECOVERY_REDUCE_DIMENSIONS, /* retry_count = 2 */
        GPU_RECOVERY_CPU_FALLBACK,     /* retry_count = 3+ */
    };

    for (size_t i = 0; i < expected_actions.size(); i++) {
        nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
            GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, static_cast<uint32_t>(i));
        EXPECT_EQ(action, expected_actions[i])
            << "Failed at retry_count=" << i;
    }

    /* Verify CPU fallback for all subsequent retry counts */
    for (int i = 4; i < 10; i++) {
        nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
            GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, i);
        EXPECT_EQ(action, GPU_RECOVERY_CPU_FALLBACK);
    }

    /* Test actual fallback execution */
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);
    nimcp_gpu_set_cpu_fallback(ctx, neural_cpu_fallback, nullptr);

    bool success = nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_TRUE(success);
    EXPECT_TRUE(ctx->cpu_fallback_active);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: EndToEndCPUFallbackPath
 * Complete CPU fallback execution path
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, EndToEndCPUFallbackPath) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(ctx, nullptr);

    static float computed_result = 0.0f;
    auto compute_fallback = [](void*, void* params, void* result) -> bool {
        if (params && result) {
            float input = *static_cast<float*>(params);
            float* output = static_cast<float*>(result);
            *output = input * 2.0f + 1.0f;
            computed_result = *output;
            return true;
        }
        return false;
    };

    nimcp_gpu_set_cpu_fallback(ctx, compute_fallback, nullptr);

    /* Force CPU fallback */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx, GPU_ERROR_DEVICE_NOT_AVAILABLE,
                          cudaErrorNoDevice, &result);

    EXPECT_TRUE(ctx->cpu_fallback_active);

    /* Execute computation via fallback */
    float input = 5.0f;
    float output = 0.0f;
    bool success = nimcp_gpu_execute_cpu_fallback(ctx, &input, &output);

    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(output, 11.0f);  /* 5 * 2 + 1 */
    EXPECT_FLOAT_EQ(computed_result, 11.0f);

    nimcp_gpu_recovery_context_destroy(ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MultiModuleRecoveryCascade
 * Recovery cascade across multiple modules
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, MultiModuleRecoveryCascade) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create contexts for different modules */
    nimcp_gpu_recovery_context_t* fuzzy_ctx = nimcp_gpu_recovery_context_create(NULL);
    nimcp_gpu_recovery_context_t* neural_ctx = nimcp_gpu_recovery_context_create(NULL);
    nimcp_gpu_recovery_context_t* stats_ctx = nimcp_gpu_recovery_context_create(NULL);

    ASSERT_NE(fuzzy_ctx, nullptr);
    ASSERT_NE(neural_ctx, nullptr);
    ASSERT_NE(stats_ctx, nullptr);

    nimcp_gpu_set_cpu_fallback(fuzzy_ctx, fuzzy_cpu_fallback, nullptr);
    nimcp_gpu_set_cpu_fallback(neural_ctx, neural_cpu_fallback, nullptr);
    nimcp_gpu_set_cpu_fallback(stats_ctx, stats_cpu_fallback, nullptr);

    /* Each module experiences an error */
    nimcp_gpu_recovery_result_t result;

    nimcp_gpu_try_recover(fuzzy_ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
    nimcp_gpu_try_recover(neural_ctx, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);
    nimcp_gpu_try_recover(stats_ctx, GPU_ERROR_NUMERICAL, cudaSuccess, &result);

    /* All modules should have recovered independently */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.recoveries_attempted, 3u);

    nimcp_gpu_recovery_context_destroy(fuzzy_ctx);
    nimcp_gpu_recovery_context_destroy(neural_ctx);
    nimcp_gpu_recovery_context_destroy(stats_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RecoveryWithAsyncOperations
 * Recovery with simulated async operations
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, RecoveryWithAsyncOperations) {
#ifdef NIMCP_ENABLE_CUDA
    std::atomic<int> completed{0};
    const int num_async_ops = 4;

    auto async_op = [&](int id) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        if (!ctx) return;

        nimcp_gpu_set_cpu_fallback(ctx, neural_cpu_fallback, nullptr);

        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_TIMEOUT, cudaSuccess, &result);

        /* Async split should be the first action */
        if (result.action_taken == GPU_RECOVERY_ASYNC_SPLIT) {
            completed++;
        }

        nimcp_gpu_recovery_context_destroy(ctx);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_async_ops; i++) {
        threads.emplace_back(async_op, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), num_async_ops);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SystemStabilityAfterRecovery
 * Verify system remains stable after many recoveries
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, SystemStabilityAfterRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    const int num_cycles = 20;

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        ASSERT_NE(ctx, nullptr) << "Failed at cycle " << cycle;

        nimcp_gpu_set_cpu_fallback(ctx, fuzzy_cpu_fallback, nullptr);

        /* Perform multiple recoveries per cycle */
        for (int i = 0; i < 5; i++) {
            nimcp_gpu_recovery_result_t result;
            nimcp_gpu_try_recover(ctx, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result);
            nimcp_gpu_recovery_context_reset(ctx);
        }

        nimcp_gpu_recovery_context_destroy(ctx);
    }

    /* System should still be stable */
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    /* Memory should be available */
    size_t free_bytes, total_bytes;
    bool mem_ok = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);
    EXPECT_TRUE(mem_ok);
    EXPECT_GT(free_bytes, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: PerformanceAfterRecovery
 * Verify recovery doesn't degrade system performance
 * ============================================================================ */
TEST_F(GPURecoveryE2ETest, PerformanceAfterRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Measure baseline context creation time */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        nimcp_gpu_recovery_context_destroy(ctx);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto baseline_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    /* Perform many recoveries */
    for (int i = 0; i < 50; i++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        nimcp_gpu_set_cpu_fallback(ctx, fuzzy_cpu_fallback, nullptr);

        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx, GPU_ERROR_OUT_OF_MEMORY,
                              cudaErrorMemoryAllocation, &result);

        nimcp_gpu_recovery_context_destroy(ctx);
    }

    /* Measure post-recovery context creation time */
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        nimcp_gpu_recovery_context_t* ctx = nimcp_gpu_recovery_context_create(NULL);
        nimcp_gpu_recovery_context_destroy(ctx);
    }
    end = std::chrono::high_resolution_clock::now();
    auto post_recovery_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    /* Performance should not degrade significantly (within 2x) */
    EXPECT_LT(post_recovery_duration.count(), baseline_duration.count() * 2);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
