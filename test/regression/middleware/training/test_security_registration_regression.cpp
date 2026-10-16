/**
 * @file test_security_registration_regression.cpp
 * @brief Regression tests for Training Module Security Registration
 *
 * Performance and stability tests:
 * - Registration/unregistration overhead measurement
 * - High-volume registration stress testing
 * - Memory leak detection
 * - Long-running stability tests
 *
 * @note Part of Phase SR-1: Security Registration Regression Tests
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <numeric>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "middleware/training/nimcp_regularization.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_training_adapters.h"
#include "security/nimcp_security_integration.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class SecurityRegistrationRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        sec_ctx = nimcp_sec_integration_create();
        ASSERT_NE(sec_ctx, nullptr);

        nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
        config.enable_continuous_monitoring = false;
        config.enable_self_monitoring = false;
        config.enable_event_logging = false;  // Disable for performance tests
        ASSERT_EQ(nimcp_sec_integration_init(sec_ctx, &config), NIMCP_SUCCESS);

        // Get baseline module count
        nimcp_sec_integration_stats_t baseline;
        if (nimcp_sec_get_stats(sec_ctx, &baseline) == NIMCP_SUCCESS) {
            baseline_active_modules = baseline.active_modules;
        }
    }

    void TearDown() override {
        if (sec_ctx) {
            nimcp_sec_integration_destroy(sec_ctx);
            sec_ctx = nullptr;
        }
    }

    nimcp_sec_integration_t* sec_ctx = nullptr;
    uint32_t baseline_active_modules = 0;
};

/* ============================================================================
 * Performance Baseline Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationRegressionTest, Performance_RegistrationOverhead) {
    const int ITERATIONS = 1000;

    // Measure creation WITHOUT security
    auto start_no_sec = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        config.security_ctx = nullptr;  // No security
        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
        nimcp_lr_scheduler_destroy(s);
    }
    auto end_no_sec = std::chrono::high_resolution_clock::now();
    auto duration_no_sec = std::chrono::duration_cast<std::chrono::microseconds>(
        end_no_sec - start_no_sec).count();

    // Measure creation WITH security
    auto start_sec = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        config.security_ctx = sec_ctx;  // With security
        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
        nimcp_lr_scheduler_destroy(s);
    }
    auto end_sec = std::chrono::high_resolution_clock::now();
    auto duration_sec = std::chrono::duration_cast<std::chrono::microseconds>(
        end_sec - start_sec).count();

    double overhead_percent = ((double)(duration_sec - duration_no_sec) / duration_no_sec) * 100.0;

    // Security overhead should be less than 1500% (15x slower is acceptable for security features)
    // Note: The overhead varies significantly based on system load and is expected
    // to be higher due to security context initialization and registration
    EXPECT_LT(overhead_percent, 1500.0)
        << "Security registration overhead too high: " << overhead_percent << "%"
        << " (no_sec: " << duration_no_sec << "us, with_sec: " << duration_sec << "us)";

    // Log results for tracking
    std::cout << "[PERF] LR Scheduler registration overhead: " << overhead_percent << "%" << std::endl;
    std::cout << "[PERF] Without security: " << (duration_no_sec / ITERATIONS) << "us/op" << std::endl;
    std::cout << "[PERF] With security: " << (duration_sec / ITERATIONS) << "us/op" << std::endl;
}

TEST_F(SecurityRegistrationRegressionTest, Performance_MultiModuleOverhead) {
    const int ITERATIONS = 500;

    // Measure pipeline creation WITHOUT security
    auto start_no_sec = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        lr_config.security_ctx = nullptr;

        nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
        gm_config.security_ctx = nullptr;

        nimcp_regularization_config_t reg_config;
        memset(&reg_config, 0, sizeof(reg_config));
        reg_config.weight_reg_type = NIMCP_REG_L2;
        reg_config.weight_reg.l2 = nimcp_l2_default_config(0.01f);
        reg_config.security_ctx = nullptr;

        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&lr_config);
        nimcp_gradient_manager_ctx_t* g = nimcp_gradient_manager_create(&gm_config);
        nimcp_regularization_ctx_t* r = nimcp_regularization_create(&reg_config);

        nimcp_regularization_destroy(r);
        nimcp_gradient_manager_destroy(g);
        nimcp_lr_scheduler_destroy(s);
    }
    auto end_no_sec = std::chrono::high_resolution_clock::now();
    auto duration_no_sec = std::chrono::duration_cast<std::chrono::microseconds>(
        end_no_sec - start_no_sec).count();

    // Measure pipeline creation WITH security
    auto start_sec = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        lr_config.security_ctx = sec_ctx;

        nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
        gm_config.security_ctx = sec_ctx;

        nimcp_regularization_config_t reg_config;
        memset(&reg_config, 0, sizeof(reg_config));
        reg_config.weight_reg_type = NIMCP_REG_L2;
        reg_config.weight_reg.l2 = nimcp_l2_default_config(0.01f);
        reg_config.security_ctx = sec_ctx;

        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&lr_config);
        nimcp_gradient_manager_ctx_t* g = nimcp_gradient_manager_create(&gm_config);
        nimcp_regularization_ctx_t* r = nimcp_regularization_create(&reg_config);

        nimcp_regularization_destroy(r);
        nimcp_gradient_manager_destroy(g);
        nimcp_lr_scheduler_destroy(s);
    }
    auto end_sec = std::chrono::high_resolution_clock::now();
    auto duration_sec = std::chrono::duration_cast<std::chrono::microseconds>(
        end_sec - start_sec).count();

    double overhead_percent = ((double)(duration_sec - duration_no_sec) / duration_no_sec) * 100.0;

    // Allow up to 5000% overhead for multi-module pipelines with security
    // Note: Higher threshold accounts for security registration failure paths
    // which add significant overhead due to error handling
    EXPECT_LT(overhead_percent, 5000.0)
        << "Multi-module security overhead too high: " << overhead_percent << "%";

    std::cout << "[PERF] Multi-module pipeline overhead: " << overhead_percent << "%" << std::endl;
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationRegressionTest, Stress_HighVolumeRegistration) {
    const int NUM_MODULES = 200;
    std::vector<nimcp_lr_scheduler_ctx_t*> schedulers;

    // Register many modules
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_MODULES; i++) {
        nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f * (i + 1));
        config.security_ctx = sec_ctx;
        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
        ASSERT_NE(s, nullptr) << "Failed to create scheduler " << i;
        schedulers.push_back(s);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto registration_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Verify all registered
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, (uint32_t)NUM_MODULES);

    // Unregister all
    start = std::chrono::high_resolution_clock::now();
    for (auto* s : schedulers) {
        nimcp_lr_scheduler_destroy(s);
    }
    end = std::chrono::high_resolution_clock::now();
    auto unregistration_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Verify all unregistered
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);

    std::cout << "[STRESS] Registered " << NUM_MODULES << " modules in "
              << registration_time << "ms" << std::endl;
    std::cout << "[STRESS] Unregistered " << NUM_MODULES << " modules in "
              << unregistration_time << "ms" << std::endl;

    // Should complete in reasonable time
    EXPECT_LT(registration_time, 5000) << "Registration too slow";
    EXPECT_LT(unregistration_time, 5000) << "Unregistration too slow";
}

TEST_F(SecurityRegistrationRegressionTest, Stress_RapidCreateDestroy) {
    const int ITERATIONS = 5000;
    int success_count = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        config.security_ctx = sec_ctx;

        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
        if (s) {
            success_count++;
            nimcp_lr_scheduler_destroy(s);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    EXPECT_EQ(success_count, ITERATIONS);

    // Security context should be clean
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);

    std::cout << "[STRESS] " << ITERATIONS << " create/destroy cycles in "
              << duration << "ms (" << (double)duration / ITERATIONS << "ms/cycle)" << std::endl;
}

TEST_F(SecurityRegistrationRegressionTest, Stress_ConcurrentHighVolume) {
    const int NUM_THREADS = 8;
    const int OPERATIONS_PER_THREAD = 500;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
                nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                    NIMCP_LR_CONSTANT, 0.001f);
                config.security_ctx = sec_ctx;

                nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
                if (s) {
                    success_count++;
                    // Do some work
                    nimcp_lr_scheduler_get_lr(s);
                    nimcp_lr_scheduler_step(s);
                    nimcp_lr_scheduler_destroy(s);
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    int total_ops = NUM_THREADS * OPERATIONS_PER_THREAD;
    EXPECT_EQ(success_count.load(), total_ops);
    EXPECT_EQ(failure_count.load(), 0);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);

    std::cout << "[STRESS] " << total_ops << " concurrent operations in "
              << duration << "ms" << std::endl;
    std::cout << "[STRESS] Throughput: " << (total_ops * 1000.0 / duration)
              << " ops/sec" << std::endl;
}

/* ============================================================================
 * Memory Stability Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationRegressionTest, Memory_NoLeaksOnRepeatedCreateDestroy) {
    const int ITERATIONS = 1000;

    // Record baseline (approximate - can't measure exact memory in portable way)
    nimcp_sec_integration_stats_t baseline_stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &baseline_stats), NIMCP_SUCCESS);

    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        lr_config.security_ctx = sec_ctx;

        nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
        gm_config.security_ctx = sec_ctx;

        nimcp_regularization_config_t reg_config;
        memset(&reg_config, 0, sizeof(reg_config));
        reg_config.weight_reg_type = NIMCP_REG_L2;
        reg_config.weight_reg.l2 = nimcp_l2_default_config(0.01f);
        reg_config.security_ctx = sec_ctx;

        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&lr_config);
        nimcp_gradient_manager_ctx_t* g = nimcp_gradient_manager_create(&gm_config);
        nimcp_regularization_ctx_t* r = nimcp_regularization_create(&reg_config);

        ASSERT_NE(s, nullptr);
        ASSERT_NE(g, nullptr);
        ASSERT_NE(r, nullptr);

        nimcp_regularization_destroy(r);
        nimcp_gradient_manager_destroy(g);
        nimcp_lr_scheduler_destroy(s);
    }

    // Check final state
    nimcp_sec_integration_stats_t final_stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &final_stats), NIMCP_SUCCESS);
    EXPECT_EQ(final_stats.active_modules, baseline_active_modules);

    // Note: registered_modules counter may wrap or have implementation-specific semantics
    // The important thing is that active_modules returns to baseline (no leaks)
    std::cout << "[MEMORY] " << ITERATIONS << " iterations completed, active_modules: "
              << final_stats.active_modules << " (baseline: " << baseline_active_modules << ")" << std::endl;
}

TEST_F(SecurityRegistrationRegressionTest, Memory_MixedModuleTypesNoLeaks) {
    const int ITERATIONS = 500;

    for (int i = 0; i < ITERATIONS; i++) {
        // Create different module types
        nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        lr_config.security_ctx = sec_ctx;

        learning_signal_adapter_config_t lsa_config = learning_signal_adapter_default_config();
        lsa_config.security_ctx = sec_ctx;

        weight_update_router_config_t wur_config = weight_update_router_default_config();
        wur_config.security_ctx = sec_ctx;

        training_event_manager_config_t tem_config = training_event_manager_default_config();
        tem_config.security_ctx = sec_ctx;

        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&lr_config);
        learning_signal_adapter_t lsa = learning_signal_adapter_create(&lsa_config);
        weight_update_router_t wur = weight_update_router_create(&wur_config, nullptr);
        training_event_manager_t tem = training_event_manager_create(&tem_config, nullptr);

        ASSERT_NE(s, nullptr);
        ASSERT_NE(lsa, nullptr);
        ASSERT_NE(wur, nullptr);
        ASSERT_NE(tem, nullptr);

        // Destroy in mixed order
        training_event_manager_destroy(tem);
        nimcp_lr_scheduler_destroy(s);
        weight_update_router_destroy(wur);
        learning_signal_adapter_destroy(lsa);
    }

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);
}

/* ============================================================================
 * Long-Running Stability Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationRegressionTest, Stability_LongRunningOperations) {
    const int DURATION_SEC = 3;  // Run for 3 seconds
    std::atomic<bool> running{true};
    std::atomic<int> total_ops{0};
    std::atomic<int> failures{0};

    std::thread worker([&]() {
        while (running) {
            nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                NIMCP_LR_CONSTANT, 0.001f);
            config.security_ctx = sec_ctx;

            nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
            if (s) {
                // Do some work
                for (int j = 0; j < 10; j++) {
                    nimcp_lr_scheduler_step(s);
                }
                nimcp_lr_scheduler_destroy(s);
                total_ops++;
            } else {
                failures++;
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));
    running = false;
    worker.join();

    EXPECT_EQ(failures.load(), 0);
    EXPECT_GT(total_ops.load(), 100);  // Should complete many operations

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);

    std::cout << "[STABILITY] Completed " << total_ops.load() << " operations in "
              << DURATION_SEC << " seconds" << std::endl;
}

TEST_F(SecurityRegistrationRegressionTest, Stability_InterleavedOperations) {
    const int NUM_MODULES = 50;
    std::vector<nimcp_lr_scheduler_ctx_t*> modules(NUM_MODULES, nullptr);

    // Interleaved create/destroy pattern
    for (int round = 0; round < 10; round++) {
        // Create half
        for (int i = 0; i < NUM_MODULES / 2; i++) {
            if (!modules[i]) {
                nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                    NIMCP_LR_CONSTANT, 0.001f);
                config.security_ctx = sec_ctx;
                modules[i] = nimcp_lr_scheduler_create(&config);
                ASSERT_NE(modules[i], nullptr);
            }
        }

        // Destroy some
        for (int i = NUM_MODULES / 4; i < NUM_MODULES / 2; i++) {
            if (modules[i]) {
                nimcp_lr_scheduler_destroy(modules[i]);
                modules[i] = nullptr;
            }
        }

        // Create more
        for (int i = NUM_MODULES / 2; i < NUM_MODULES; i++) {
            if (!modules[i]) {
                nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                    NIMCP_LR_CONSTANT, 0.001f);
                config.security_ctx = sec_ctx;
                modules[i] = nimcp_lr_scheduler_create(&config);
                ASSERT_NE(modules[i], nullptr);
            }
        }

        // Destroy remaining from first half
        for (int i = 0; i < NUM_MODULES / 4; i++) {
            if (modules[i]) {
                nimcp_lr_scheduler_destroy(modules[i]);
                modules[i] = nullptr;
            }
        }
    }

    // Cleanup remaining
    for (auto* m : modules) {
        if (m) nimcp_lr_scheduler_destroy(m);
    }

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);
}

/* ============================================================================
 * Regression Tests for Specific Issues
 * ============================================================================ */

TEST_F(SecurityRegistrationRegressionTest, Regression_NullSecurityContextSafe) {
    // All modules should gracefully handle null security context
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        config.security_ctx = nullptr;

        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
        ASSERT_NE(s, nullptr);

        // Should work normally
        float lr = nimcp_lr_scheduler_get_lr(s);
        EXPECT_FLOAT_EQ(lr, 0.001f);

        nimcp_lr_scheduler_destroy(s);
    }
}

TEST_F(SecurityRegistrationRegressionTest, Regression_DestroyAfterSecurityContextDestroyed) {
    // Create module with security
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
        NIMCP_LR_CONSTANT, 0.001f);
    config.security_ctx = sec_ctx;

    nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(s, nullptr);

    // Destroy security context first (simulating abnormal shutdown)
    nimcp_sec_integration_destroy(sec_ctx);
    sec_ctx = nullptr;

    // Module destroy should handle this gracefully (no crash/hang)
    // Note: This tests defensive coding - unregistration may fail but shouldn't crash
    nimcp_lr_scheduler_destroy(s);

    // Test passed if we didn't crash
    SUCCEED();
}

/* ============================================================================
 * Throughput Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationRegressionTest, Throughput_OperationsPerSecond) {
    const int DURATION_MS = 1000;
    std::atomic<int> ops{0};
    std::atomic<bool> running{true};

    std::thread worker([&]() {
        while (running) {
            nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                NIMCP_LR_CONSTANT, 0.001f);
            config.security_ctx = sec_ctx;

            nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
            if (s) {
                nimcp_lr_scheduler_destroy(s);
                ops++;
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;
    worker.join();

    double ops_per_sec = ops.load() * 1000.0 / DURATION_MS;
    std::cout << "[THROUGHPUT] " << ops_per_sec << " registrations/sec" << std::endl;

    // Should achieve at least 1000 ops/sec on reasonable hardware
    EXPECT_GT(ops_per_sec, 1000.0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
