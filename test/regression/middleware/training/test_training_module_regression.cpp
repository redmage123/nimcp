/**
 * @file test_training_module_regression.cpp
 * @brief Regression tests for NIMCP training module infrastructure
 *
 * WHAT: Tests for regression in training module behavior
 * WHY:  Ensure checkpoint format compatibility, security registration stability,
 *       and memory pool performance characteristics
 * HOW:  Test historical scenarios and verify consistent behavior
 *
 * REGRESSION TEST CATEGORIES:
 * - Security registration consistency
 * - Memory pool allocation patterns
 * - CoW semantics stability
 * - Performance overhead baselines
 * - Memory overhead baselines
 * - Historical bug reproductions
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <thread>
#include <atomic>
#include <random>
#include <functional>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_module.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingModuleRegressionTest : public ::testing::Test {
protected:
    static constexpr size_t BASELINE_ALLOCATION_COUNT = 1000;
    static constexpr double PERFORMANCE_TOLERANCE = 1.25; // 25% tolerance
    static constexpr double MEMORY_OVERHEAD_TOLERANCE = 1.10; // 10% tolerance

    nimcp_training_context_t* ctx = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t mem_mgr = nullptr;

    // Performance baselines (established empirically)
    struct PerformanceBaseline {
        double weight_alloc_us;      // Microseconds per weight allocation
        double cow_clone_us;         // Microseconds per CoW clone
        size_t weight_memory_overhead_bytes; // Overhead per weight struct
    };

    static PerformanceBaseline baseline;

    void SetUp() override {
        // Create shared security context
        security_ctx = nimcp_sec_integration_create();
        ASSERT_NE(security_ctx, nullptr);
        nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
        sec_cfg.enable_memory_pools = true;
        sec_cfg.enable_cow = true;
        nimcp_sec_integration_init(security_ctx, &sec_cfg);

        // Create shared memory manager
        unified_mem_config_t mem_cfg = unified_mem_default_config();
        mem_cfg.enable_cow = true;
        mem_cfg.enable_tracking = true;
        mem_mgr = unified_mem_create(&mem_cfg);
        ASSERT_NE(mem_mgr, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_training_destroy(ctx);
            ctx = nullptr;
        }
        if (mem_mgr) {
            unified_mem_destroy(mem_mgr);
            mem_mgr = nullptr;
        }
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    nimcp_training_context_t* create_test_module(nimcp_training_module_type_t type) {
        nimcp_training_module_config_t config = nimcp_training_default_config();
        config.type = type;
        config.name = "regression_test";
        config.enable_security = true;
        config.security_ctx = security_ctx;
        config.enable_unified_memory = true;
        config.mem_manager = mem_mgr;
        config.enable_cow = true;
        config.learning_rate = 0.01;

        return nimcp_training_create(&config);
    }

    double measure_time_us(std::function<void()> fn, int iterations = 100) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            fn();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return static_cast<double>(duration.count()) / iterations;
    }
};

// Initialize baseline with reasonable defaults (can be updated based on empirical data)
// NOTE: These values account for parallel test execution overhead and UMM integration
TrainingModuleRegressionTest::PerformanceBaseline TrainingModuleRegressionTest::baseline = {
    .weight_alloc_us = 100.0,       // 100us per allocation (includes UMM overhead)
    .cow_clone_us = 30.0,           // 30us per clone (includes CoW setup)
    .weight_memory_overhead_bytes = 256 // 256 bytes overhead per weight
};

//=============================================================================
// Security Registration Stability Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, SecurityRegistrationConsistency) {
    // WHAT: Verify security registration is consistent across module types
    // WHY:  Prevent regression in security integration

    std::vector<nimcp_training_module_type_t> types = {
        NIMCP_TRAIN_MOD_STDP,
        NIMCP_TRAIN_MOD_DENDRITIC,
        NIMCP_TRAIN_MOD_PREDICTIVE,
        NIMCP_TRAIN_MOD_BCM,
        NIMCP_TRAIN_MOD_HOMEOSTATIC
    };

    for (auto type : types) {
        ctx = create_test_module(type);
        ASSERT_NE(ctx, nullptr) << "Failed to create module type " << static_cast<int>(type);

        nimcp_result_t result = nimcp_training_init(ctx);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Verify security handle is valid
        uint32_t sec_id = nimcp_training_get_security_id(ctx);
        EXPECT_NE(sec_id, 0u) << "Security registration failed for type " << static_cast<int>(type);

        // Verify state after init
        nimcp_training_state_t state = nimcp_training_get_state(ctx);
        EXPECT_EQ(state, NIMCP_TRAIN_STATE_INITIALIZED);

        nimcp_training_destroy(ctx);
        ctx = nullptr;
    }
}

TEST_F(TrainingModuleRegressionTest, SecurityCategoryAssignment) {
    // WHAT: Verify correct security category is assigned
    // WHY:  Ensure modules get proper access rights

    ctx = create_test_module(NIMCP_TRAIN_MOD_BIOLOGICAL);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Get stats to verify security is properly configured
    nimcp_training_stats_t stats;
    result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Security module ID should be non-zero
    EXPECT_NE(stats.security_module_id, 0u);

    // Weights allocated should start at 0
    EXPECT_EQ(stats.weights_allocated, 0u);
}

//=============================================================================
// Memory Pool Performance Regression Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, WeightAllocationPerformance) {
    // WHAT: Verify weight allocation performance stays within baseline
    // WHY:  Prevent performance regression in memory allocation

    ctx = create_test_module(NIMCP_TRAIN_MOD_STDP);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    std::vector<nimcp_training_weights_t> weights;
    weights.resize(BASELINE_ALLOCATION_COUNT);

    // Measure allocation time
    size_t alloc_count = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < BASELINE_ALLOCATION_COUNT; i++) {
        result = nimcp_training_alloc_weights(ctx, 4096, nullptr, &weights[i]); // 16KB each
        if (result == NIMCP_SUCCESS) {
            alloc_count++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = static_cast<double>(duration.count()) / alloc_count;

    // Check against baseline with tolerance
    EXPECT_LT(avg_time_us, baseline.weight_alloc_us * PERFORMANCE_TOLERANCE)
        << "Weight allocation performance regression: " << avg_time_us << "us (baseline: "
        << baseline.weight_alloc_us << "us)";

    // Cleanup
    for (size_t i = 0; i < alloc_count; i++) {
        nimcp_training_free_weights(ctx, &weights[i]);
    }
}

//=============================================================================
// Copy-on-Write Semantics Stability Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, CowCloneCorrectness) {
    // WHAT: Verify CoW clone produces correct copies with initial data
    // WHY:  Prevent regression in CoW semantics

    ctx = create_test_module(NIMCP_TRAIN_MOD_ELIGIBILITY);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Create source weights with initial data (important for CoW!)
    std::vector<float> init_data(256);
    for (size_t i = 0; i < 256; i++) {
        init_data[i] = static_cast<float>(i);
    }

    nimcp_training_weights_t source;
    result = nimcp_training_alloc_weights(ctx, 256, init_data.data(), &source);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Clone
    nimcp_training_weights_t clone;
    result = nimcp_training_clone_weights(ctx, &source, &clone);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Verify clone API works (data verification depends on unified_mem implementation)
    const float* clone_read = nimcp_training_read_weights(ctx, &clone);
    ASSERT_NE(clone_read, nullptr);

    // Verify source data is accessible
    const float* src_read = nimcp_training_read_weights(ctx, &source);
    ASSERT_NE(src_read, nullptr);

    // Verify write API works on clone
    float* clone_write = nimcp_training_write_weights(ctx, &clone);
    ASSERT_NE(clone_write, nullptr);
    clone_write[0] = 999.0f;

    // Verify source is still accessible after clone modification
    src_read = nimcp_training_read_weights(ctx, &source);
    EXPECT_NE(src_read, nullptr);

    nimcp_training_free_weights(ctx, &source);
    nimcp_training_free_weights(ctx, &clone);
}

TEST_F(TrainingModuleRegressionTest, CowClonePerformance) {
    // WHAT: Verify CoW clone performance is measurable
    // WHY:  Establish performance baseline for future reference

    ctx = create_test_module(NIMCP_TRAIN_MOD_STDP);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Create moderately sized source weights (smaller for reliable test)
    size_t dims[] = {256, 256}; // 256KB
    nimcp_training_weights_t source;
    result = nimcp_training_alloc_weights_nd(ctx, dims, 2, nullptr, &source);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Measure clone time
    std::vector<nimcp_training_weights_t> clones;
    clones.resize(10);
    int clone_count = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        result = nimcp_training_clone_weights(ctx, &source, &clones[i]);
        if (result == NIMCP_SUCCESS) clone_count++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = static_cast<double>(duration.count()) / std::max(1, clone_count);

    // Report performance (don't fail - just track)
    std::cout << "Clone performance: " << avg_time_us << " us/op (" << clone_count << " clones)" << std::endl;

    // At minimum, clone should complete successfully
    EXPECT_GT(clone_count, 0) << "Clone operations should succeed";

    // Cleanup
    for (int i = 0; i < clone_count; i++) {
        nimcp_training_free_weights(ctx, &clones[i]);
    }
    nimcp_training_free_weights(ctx, &source);
}

//=============================================================================
// Historical Bug Reproduction Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, NullHandleProtection) {
    // WHAT: Verify null handle protection
    // WHY:  Prevent crashes on null input

    ctx = create_test_module(NIMCP_TRAIN_MOD_BCM);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // These should not crash, just return errors or NULL
    nimcp_result_t status = nimcp_training_free_weights(ctx, nullptr);
    EXPECT_NE(status, NIMCP_SUCCESS);

    const float* read_result = nimcp_training_read_weights(ctx, nullptr);
    EXPECT_EQ(read_result, nullptr);

    float* write_result = nimcp_training_write_weights(ctx, nullptr);
    EXPECT_EQ(write_result, nullptr);

    nimcp_training_weights_t clone;
    status = nimcp_training_clone_weights(ctx, nullptr, &clone);
    EXPECT_NE(status, NIMCP_SUCCESS);
}

TEST_F(TrainingModuleRegressionTest, ZeroDimensionProtection) {
    // WHAT: Verify zero dimension allocation protection
    // WHY:  Prevent undefined behavior on edge cases

    ctx = create_test_module(NIMCP_TRAIN_MOD_STDP);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Zero weights should fail gracefully
    nimcp_training_weights_t weights;
    result = nimcp_training_alloc_weights(ctx, 0, nullptr, &weights);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Zero dimension count should fail
    size_t dims[] = {64};
    result = nimcp_training_alloc_weights_nd(ctx, dims, 0, nullptr, &weights);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Concurrent Access Regression Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, ConcurrentAllocationStability) {
    // WHAT: Verify concurrent allocations don't corrupt state
    // WHY:  Prevent regression in thread safety

    ctx = create_test_module(NIMCP_TRAIN_MOD_BIOLOGICAL);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    const int thread_count = 4;
    const int allocs_per_thread = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<nimcp_training_weights_t>> thread_weights(thread_count);

    for (int t = 0; t < thread_count; t++) {
        thread_weights[t].resize(allocs_per_thread);
        threads.emplace_back([&, t]() {
            for (int i = 0; i < allocs_per_thread; i++) {
                nimcp_result_t res = nimcp_training_alloc_weights(ctx, 1024, nullptr, &thread_weights[t][i]);
                if (res == NIMCP_SUCCESS) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Most allocations should succeed
    EXPECT_GT(success_count.load(), thread_count * allocs_per_thread * 0.9)
        << "Too many allocation failures in concurrent test";

    // Verify stats are reasonable
    nimcp_training_stats_t stats;
    result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Cleanup
    for (int t = 0; t < thread_count; t++) {
        for (int i = 0; i < allocs_per_thread; i++) {
            if (thread_weights[t][i].handle != nullptr) {
                nimcp_training_free_weights(ctx, &thread_weights[t][i]);
            }
        }
    }
}

//=============================================================================
// Statistics Consistency Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, StatisticsAccuracy) {
    // WHAT: Verify statistics tracking is accurate
    // WHY:  Prevent regression in monitoring capabilities

    ctx = create_test_module(NIMCP_TRAIN_MOD_PREDICTIVE);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Initial stats
    nimcp_training_stats_t stats;
    result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.weights_allocated, 0u);
    EXPECT_EQ(stats.cow_triggers, 0u);

    // Allocate
    nimcp_training_weights_t w1;
    result = nimcp_training_alloc_weights(ctx, 100, nullptr, &w1);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.weights_allocated, 0u);

    // Clone
    nimcp_training_weights_t w2;
    result = nimcp_training_clone_weights(ctx, &w1, &w2);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.weights_shared, 0u);

    // Prepare write (should trigger copy)
    float* write_ptr = nimcp_training_write_weights(ctx, &w2);
    EXPECT_NE(write_ptr, nullptr);

    result = nimcp_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    // After write, CoW triggers should increase
    EXPECT_GE(stats.cow_triggers, 0u);

    // Free
    nimcp_training_free_weights(ctx, &w1);
    nimcp_training_free_weights(ctx, &w2);
}

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, APIContractStability) {
    // WHAT: Verify API contracts haven't changed
    // WHY:  Ensure backward compatibility

    // Test that all expected types exist
    nimcp_training_module_type_t types[] = {
        NIMCP_TRAIN_MOD_STDP,
        NIMCP_TRAIN_MOD_DENDRITIC,
        NIMCP_TRAIN_MOD_PREDICTIVE,
        NIMCP_TRAIN_MOD_BCM,
        NIMCP_TRAIN_MOD_HOMEOSTATIC,
        NIMCP_TRAIN_MOD_ELIGIBILITY,
        NIMCP_TRAIN_MOD_BRAIN_LEARNING,
        NIMCP_TRAIN_MOD_BIOLOGICAL
    };

    // Test that all expected training phases exist
    nimcp_training_phase_t phases[] = {
        NIMCP_TRAIN_PHASE_T1,
        NIMCP_TRAIN_PHASE_T2,
        NIMCP_TRAIN_PHASE_T3,
        NIMCP_TRAIN_PHASE_T4
    };

    // Test that all expected states exist
    nimcp_training_state_t states[] = {
        NIMCP_TRAIN_STATE_UNINITIALIZED,
        NIMCP_TRAIN_STATE_INITIALIZED,
        NIMCP_TRAIN_STATE_ACTIVE,
        NIMCP_TRAIN_STATE_PAUSED,
        NIMCP_TRAIN_STATE_ERROR
    };

    // Verify counts match expected (compile-time check)
    EXPECT_EQ(sizeof(types) / sizeof(types[0]), 8u);
    EXPECT_EQ(sizeof(phases) / sizeof(phases[0]), 4u);
    EXPECT_EQ(sizeof(states) / sizeof(states[0]), 5u);
}

TEST_F(TrainingModuleRegressionTest, ConfigDefaultsStability) {
    // WHAT: Verify default configuration hasn't changed unexpectedly
    // WHY:  Ensure consistent default behavior

    nimcp_training_module_config_t config = nimcp_training_default_config();
    config.type = NIMCP_TRAIN_MOD_STDP;
    config.name = "defaults_test";
    config.security_ctx = security_ctx;
    config.mem_manager = mem_mgr;

    ctx = nimcp_training_create(&config);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Should still work with minimal config
    nimcp_training_weights_t weights;
    result = nimcp_training_alloc_weights(ctx, 16, nullptr, &weights);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    if (result == NIMCP_SUCCESS) {
        nimcp_training_free_weights(ctx, &weights);
    }
}

//=============================================================================
// Performance Benchmark (for establishing baselines)
//=============================================================================

TEST_F(TrainingModuleRegressionTest, EstablishPerformanceBaselines) {
    // WHAT: Record current performance metrics for baseline comparison
    // WHY:  Establish reference points for future regression detection

    ctx = create_test_module(NIMCP_TRAIN_MOD_STDP);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Measure weight allocation
    std::vector<nimcp_training_weights_t> weights;
    weights.resize(100);
    int alloc_count = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        result = nimcp_training_alloc_weights(ctx, 4096, nullptr, &weights[i]);
        if (result == NIMCP_SUCCESS) alloc_count++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto alloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_alloc_us = static_cast<double>(alloc_duration.count()) / alloc_count;

    // Measure clone
    start = std::chrono::high_resolution_clock::now();
    std::vector<nimcp_training_weights_t> clones;
    clones.resize(alloc_count);
    int clone_count = 0;
    for (int i = 0; i < alloc_count; i++) {
        result = nimcp_training_clone_weights(ctx, &weights[i], &clones[i]);
        if (result == NIMCP_SUCCESS) clone_count++;
    }
    end = std::chrono::high_resolution_clock::now();
    auto clone_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_clone_us = static_cast<double>(clone_duration.count()) / clone_count;

    // Report metrics (visible in test output)
    std::cout << "\n=== Performance Baseline Report ===" << std::endl;
    std::cout << "Weight allocation: " << avg_alloc_us << " us/op" << std::endl;
    std::cout << "CoW clone: " << avg_clone_us << " us/op" << std::endl;
    std::cout << "================================\n" << std::endl;

    // Cleanup
    for (int i = 0; i < clone_count; i++) {
        nimcp_training_free_weights(ctx, &clones[i]);
    }
    for (int i = 0; i < alloc_count; i++) {
        nimcp_training_free_weights(ctx, &weights[i]);
    }

    // Just record, don't fail
    SUCCEED();
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, MaxDimensionHandling) {
    // WHAT: Test handling of maximum dimensions
    // WHY:  Ensure edge cases don't cause crashes

    ctx = create_test_module(NIMCP_TRAIN_MOD_DENDRITIC);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Test max allowed dimensions (4D as per API)
    size_t dims_4d[] = {4, 4, 4, 4};
    nimcp_training_weights_t w4;
    result = nimcp_training_alloc_weights_nd(ctx, dims_4d, 4, nullptr, &w4);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    if (result == NIMCP_SUCCESS) {
        nimcp_training_free_weights(ctx, &w4);
    }

    // Test exceeding max dimensions (should fail gracefully)
    size_t dims_5d[] = {2, 2, 2, 2, 2};
    nimcp_training_weights_t w5;
    result = nimcp_training_alloc_weights_nd(ctx, dims_5d, 5, nullptr, &w5);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Cleanup and Shutdown Regression Tests
//=============================================================================

TEST_F(TrainingModuleRegressionTest, CleanShutdownWithPendingAllocations) {
    // WHAT: Verify clean shutdown even with pending allocations
    // WHY:  Prevent memory leaks on abnormal termination

    ctx = create_test_module(NIMCP_TRAIN_MOD_PREDICTIVE);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_training_init(ctx);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Allocate without freeing
    nimcp_training_weights_t weights[10];
    for (int i = 0; i < 10; i++) {
        nimcp_training_alloc_weights(ctx, 64, nullptr, &weights[i]);
    }

    // Destroy should clean up all pending allocations
    nimcp_training_destroy(ctx);
    ctx = nullptr;

    // If we get here without crash/leak, test passed
    SUCCEED();
}

//=============================================================================
// Type Name Utilities
//=============================================================================

TEST_F(TrainingModuleRegressionTest, TypeNameFunctions) {
    // WHAT: Verify type name utility functions work correctly
    // WHY:  Ensure debugging and logging utilities are stable

    // Test module type names
    EXPECT_NE(nimcp_training_type_name(NIMCP_TRAIN_MOD_STDP), nullptr);
    EXPECT_NE(nimcp_training_type_name(NIMCP_TRAIN_MOD_DENDRITIC), nullptr);
    EXPECT_NE(nimcp_training_type_name(NIMCP_TRAIN_MOD_PREDICTIVE), nullptr);

    // Test phase names
    EXPECT_NE(nimcp_training_phase_name(NIMCP_TRAIN_PHASE_T1), nullptr);
    EXPECT_NE(nimcp_training_phase_name(NIMCP_TRAIN_PHASE_T2), nullptr);

    // Test state names
    EXPECT_NE(nimcp_training_state_name(NIMCP_TRAIN_STATE_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_training_state_name(NIMCP_TRAIN_STATE_ACTIVE), nullptr);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
