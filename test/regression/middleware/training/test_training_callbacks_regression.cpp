/**
 * @file test_training_callbacks_regression.cpp
 * @brief Regression tests for Training Callbacks Module (Phase TCB-1)
 *
 * Tests cover:
 * - Performance benchmarks (callback execution overhead)
 * - Memory usage stability
 * - Large-scale training simulation
 * - Long-running stability
 * - Memory leak detection
 * - Throughput benchmarks
 * - Latency measurements
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <numeric>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_callbacks.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::milli>;

constexpr float EPSILON = 1e-5f;
constexpr int WARMUP_ITERATIONS = 100;
constexpr int BENCHMARK_ITERATIONS = 10000;
constexpr int STRESS_ITERATIONS = 100000;

// Performance thresholds (microseconds)
constexpr double MAX_CALLBACK_OVERHEAD_US = 50.0;      // Max overhead per callback
constexpr double MAX_REGISTRATION_TIME_US = 100.0;    // Max registration time
constexpr double MAX_FIRE_TIME_US = 500.0;            // Max fire time with many callbacks
constexpr double MAX_METRICS_UPDATE_US = 10.0;        // Max metrics update time

// Global tracking
std::atomic<int> g_callback_count{0};

/**
 * @brief Test fixture for callback regression tests
 */
class TrainingCallbacksRegressionTest : public ::testing::Test {
protected:
    tcb_context_t* ctx = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;

    void SetUp() override {
        g_callback_count.store(0);

        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
            nimcp_sec_integration_init(security_ctx, &sec_cfg);
        }
    }

    void TearDown() override {
        if (ctx) {
            tcb_destroy(ctx);
            ctx = nullptr;
        }
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    void CreateDefaultContext() {
        tcb_config_t config = tcb_config_default();
        ctx = tcb_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void CreateContextWithSecurity() {
        tcb_config_t config = tcb_config_default();
        config.security_ctx = security_ctx;
        ctx = tcb_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    double MeasureTimeUs(std::function<void()> fn) {
        auto start = Clock::now();
        fn();
        auto end = Clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    void Warmup(std::function<void()> fn, int iterations = WARMUP_ITERATIONS) {
        for (int i = 0; i < iterations; i++) {
            fn();
        }
    }
};

// =============================================================================
// Minimal Callback for Benchmarking
// =============================================================================

static tcb_action_t minimal_callback(const tcb_event_t* event) {
    (void)event;
    g_callback_count++;
    return TCB_ACTION_CONTINUE;
}

static tcb_action_t heavy_callback(const tcb_event_t* event) {
    (void)event;
    // Simulate some work
    volatile float result = 0.0f;
    for (int i = 0; i < 100; i++) {
        result += sqrtf((float)i);
    }
    g_callback_count++;
    return TCB_ACTION_CONTINUE;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Perf_ContextCreationDestruction) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        tcb_config_t config = tcb_config_default();
        tcb_context_t* temp = tcb_create(&config);
        tcb_destroy(temp);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = Clock::now();
        tcb_config_t config = tcb_config_default();
        tcb_context_t* temp = tcb_create(&config);
        tcb_destroy(temp);
        auto end = Clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }

    double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    double max_time = *std::max_element(times.begin(), times.end());

    std::sort(times.begin(), times.end());
    double p99 = times[times.size() * 99 / 100];

    printf("Context create/destroy: avg=%.2fus, p99=%.2fus, max=%.2fus\n",
           avg, p99, max_time);

    EXPECT_LT(avg, 1000.0);  // Should be under 1ms average
    EXPECT_LT(p99, 5000.0);  // P99 under 5ms
}

TEST_F(TrainingCallbacksRegressionTest, Perf_CallbackRegistration) {
    CreateDefaultContext();

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                           minimal_callback, nullptr, "warmup");
        tcb_unregister(ctx, id);
    }

    // Benchmark registration
    for (int i = 0; i < BENCHMARK_ITERATIONS / 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "cb_%d", i);

        auto start = Clock::now();
        uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                           minimal_callback, nullptr, name);
        auto end = Clock::now();

        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());

        if (id == 0) break;  // Hit capacity
    }

    if (!times.empty()) {
        double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        double max_time = *std::max_element(times.begin(), times.end());

        printf("Callback registration: avg=%.2fus, max=%.2fus (n=%zu)\n",
               avg, max_time, times.size());

        EXPECT_LT(avg, MAX_REGISTRATION_TIME_US);
    }
}

TEST_F(TrainingCallbacksRegressionTest, Perf_EventFiring_SingleCallback) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        minimal_callback, nullptr, "test");

    // Warmup
    Warmup([this]() {
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    });

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    g_callback_count.store(0);
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = Clock::now();
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        auto end = Clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }

    double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    std::sort(times.begin(), times.end());
    double p99 = times[times.size() * 99 / 100];

    printf("Single callback fire: avg=%.2fus, p99=%.2fus\n", avg, p99);

    EXPECT_EQ(g_callback_count.load(), BENCHMARK_ITERATIONS);
    EXPECT_LT(avg, MAX_CALLBACK_OVERHEAD_US);
}

TEST_F(TrainingCallbacksRegressionTest, Perf_EventFiring_ManyCallbacks) {
    CreateDefaultContext();

    // Register max callbacks
    for (int i = 0; i < TCB_MAX_CALLBACKS_PER_EVENT; i++) {
        char name[32];
        snprintf(name, sizeof(name), "cb_%d", i);
        tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                            minimal_callback, nullptr, name);
    }

    // Warmup
    Warmup([this]() {
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    });

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = Clock::now();
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        auto end = Clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }

    double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    std::sort(times.begin(), times.end());
    double p99 = times[times.size() * 99 / 100];
    double per_callback = avg / TCB_MAX_CALLBACKS_PER_EVENT;

    printf("Multi-callback fire (%d cbs): avg=%.2fus, p99=%.2fus, per_cb=%.2fus\n",
           TCB_MAX_CALLBACKS_PER_EVENT, avg, p99, per_callback);

    EXPECT_LT(avg, MAX_FIRE_TIME_US);
    EXPECT_LT(per_callback, MAX_CALLBACK_OVERHEAD_US);
}

TEST_F(TrainingCallbacksRegressionTest, Perf_MetricsUpdate) {
    CreateDefaultContext();

    // Warmup
    Warmup([this]() {
        tcb_update_metrics(ctx, 1.0f, 0.01f, 0, 0.5f);
    });

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = Clock::now();
        tcb_update_metrics(ctx, 1.0f / (i + 1), 0.01f, i, 0.5f);
        auto end = Clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }

    double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    std::sort(times.begin(), times.end());
    double p99 = times[times.size() * 99 / 100];

    printf("Metrics update: avg=%.2fus, p99=%.2fus\n", avg, p99);

    EXPECT_LT(avg, MAX_METRICS_UPDATE_US);
}

// =============================================================================
// Memory Stability Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Memory_RepeatedCreateDestroy) {
    // Test for memory leaks by repeated create/destroy cycles
    for (int cycle = 0; cycle < 1000; cycle++) {
        tcb_config_t config = tcb_config_default();
        tcb_context_t* temp = tcb_create(&config);
        ASSERT_NE(temp, nullptr);

        // Register some callbacks
        for (int i = 0; i < 10; i++) {
            tcb_register_simple(temp, TCB_EVENT_STEP_COMPLETE,
                                minimal_callback, nullptr, "test");
        }

        // Fire some events
        for (int i = 0; i < 100; i++) {
            tcb_fire_event(temp, TCB_EVENT_STEP_COMPLETE, nullptr);
        }

        tcb_destroy(temp);
    }

    // If we get here without crashes or valgrind errors, the test passes
    SUCCEED();
}

TEST_F(TrainingCallbacksRegressionTest, Memory_RepeatedRegistration) {
    CreateDefaultContext();

    // Repeatedly register and unregister callbacks
    for (int cycle = 0; cycle < 10000; cycle++) {
        std::vector<uint32_t> ids;

        // Register batch
        for (int i = 0; i < 10; i++) {
            uint32_t id = tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                                               minimal_callback, nullptr, "test");
            if (id != 0) {
                ids.push_back(id);
            }
        }

        // Unregister all
        for (uint32_t id : ids) {
            tcb_unregister(ctx, id);
        }
    }

    // Should have no active callbacks
    EXPECT_EQ(tcb_get_callback_count(ctx, TCB_EVENT_STEP_COMPLETE), 0u);
}

// =============================================================================
// Long-Running Stability Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Stability_LongTrainingSimulation) {
    CreateDefaultContext();

    // Register callbacks
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        minimal_callback, nullptr, "step");
    tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED,
                        minimal_callback, nullptr, "loss");

    g_callback_count.store(0);

    auto start = Clock::now();

    // Simulate long training run
    for (int step = 0; step < STRESS_ITERATIONS; step++) {
        // Update metrics
        float loss = 1.0f / (step + 1);
        tcb_update_metrics(ctx, loss, 0.01f, step, 0.5f);

        // Fire callbacks
        tcb_fire_event(ctx, TCB_EVENT_LOSS_COMPUTED, nullptr);
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);

        // Occasionally check early stopping
        if (step % 1000 == 0) {
            tcb_should_stop(ctx, loss);
        }
    }

    auto end = Clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    printf("Long training simulation: %d steps in %.2fms (%.2f steps/sec)\n",
           STRESS_ITERATIONS, elapsed_ms,
           STRESS_ITERATIONS / (elapsed_ms / 1000.0));

    // 2 callbacks per step
    EXPECT_EQ(g_callback_count.load(), STRESS_ITERATIONS * 2);

    // Should be able to process at least 10000 steps/sec
    double steps_per_sec = STRESS_ITERATIONS / (elapsed_ms / 1000.0);
    EXPECT_GT(steps_per_sec, 10000.0);
}

TEST_F(TrainingCallbacksRegressionTest, Stability_MetricsAccuracy) {
    CreateDefaultContext();

    // Run many updates and verify metrics stay accurate
    for (int step = 0; step < 10000; step++) {
        float expected_loss = 1.0f / (step + 1);
        tcb_update_metrics(ctx, expected_loss, 0.01f, step, 0.5f);
    }

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    EXPECT_EQ(metrics.step, 9999u);
    EXPECT_NEAR(metrics.loss, 1.0f / 10000, 0.001f);
    EXPECT_LT(metrics.min_loss, 0.001f);  // Smallest loss recorded
}

// =============================================================================
// Throughput Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Throughput_CallbacksFired) {
    CreateDefaultContext();

    // Register callbacks for multiple event types
    for (int i = 0; i < TCB_MAX_CALLBACKS_PER_EVENT; i++) {
        tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                            minimal_callback, nullptr, "step");
    }

    g_callback_count.store(0);

    auto start = Clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    }

    auto end = Clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    int total_callbacks = g_callback_count.load();
    double callbacks_per_sec = total_callbacks / (elapsed_ms / 1000.0);

    printf("Callback throughput: %d callbacks in %.2fms (%.0f callbacks/sec)\n",
           total_callbacks, elapsed_ms, callbacks_per_sec);

    // Should process at least 100k callbacks/sec
    EXPECT_GT(callbacks_per_sec, 100000.0);
}

TEST_F(TrainingCallbacksRegressionTest, Throughput_EventsPerSecond) {
    CreateDefaultContext();

    // Single callback per event
    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        minimal_callback, nullptr, "test");

    g_callback_count.store(0);

    auto start = Clock::now();

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
    }

    auto end = Clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double events_per_sec = STRESS_ITERATIONS / (elapsed_ms / 1000.0);

    printf("Event throughput: %d events in %.2fms (%.0f events/sec)\n",
           STRESS_ITERATIONS, elapsed_ms, events_per_sec);

    // Should process at least 500k events/sec with single callback
    EXPECT_GT(events_per_sec, 500000.0);
}

// =============================================================================
// Statistics Accuracy Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Stats_AccuracyAfterManyEvents) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        minimal_callback, nullptr, "test");
    tcb_register_simple(ctx, TCB_EVENT_LOSS_COMPUTED,
                        minimal_callback, nullptr, "loss");

    // Fire many events
    for (int i = 0; i < 50000; i++) {
        tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
        if (i % 2 == 0) {
            tcb_fire_event(ctx, TCB_EVENT_LOSS_COMPUTED, nullptr);
        }
    }

    tcb_stats_t stats;
    tcb_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_callbacks_fired, 50000u + 25000u);
    EXPECT_EQ(stats.callbacks_by_event[TCB_EVENT_STEP_COMPLETE], 50000u);
    EXPECT_EQ(stats.callbacks_by_event[TCB_EVENT_LOSS_COMPUTED], 25000u);
}

// =============================================================================
// Edge Case Regression Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, EdgeCase_ZeroLoss) {
    CreateDefaultContext();

    for (int i = 0; i < 100; i++) {
        tcb_update_metrics(ctx, 0.0f, 0.01f, i, 0.5f);
    }

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    EXPECT_NEAR(metrics.loss, 0.0f, EPSILON);
    EXPECT_NEAR(metrics.loss_ema, 0.0f, 0.01f);
}

TEST_F(TrainingCallbacksRegressionTest, EdgeCase_NegativeLoss) {
    CreateDefaultContext();

    // Some loss functions can produce negative values (e.g., log-likelihood)
    for (int i = 0; i < 100; i++) {
        tcb_update_metrics(ctx, -1.0f - i * 0.01f, 0.01f, i, 0.5f);
    }

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    EXPECT_LT(metrics.loss, 0.0f);
    EXPECT_LT(metrics.min_loss, -1.0f);
}

TEST_F(TrainingCallbacksRegressionTest, EdgeCase_LargeLoss) {
    CreateDefaultContext();

    for (int i = 0; i < 100; i++) {
        tcb_update_metrics(ctx, 1e10f, 0.01f, i, 0.5f);
    }

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    EXPECT_GT(metrics.loss, 1e9f);
}

TEST_F(TrainingCallbacksRegressionTest, EdgeCase_TinyLearningRate) {
    CreateDefaultContext();

    for (int i = 0; i < 100; i++) {
        tcb_update_metrics(ctx, 1.0f, 1e-10f, i, 0.5f);
    }

    tcb_metrics_t metrics;
    tcb_get_metrics(ctx, &metrics);

    EXPECT_NEAR(metrics.learning_rate, 1e-10f, 1e-12f);
}

// =============================================================================
// Concurrent Stress Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Concurrent_StressTest) {
    CreateDefaultContext();

    tcb_register_simple(ctx, TCB_EVENT_STEP_COMPLETE,
                        minimal_callback, nullptr, "test");

    std::atomic<bool> running{true};
    std::atomic<int> total_fires{0};
    std::atomic<int> total_updates{0};

    std::vector<std::thread> threads;

    // Multiple fire threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running, &total_fires]() {
            while (running.load()) {
                tcb_fire_event(ctx, TCB_EVENT_STEP_COMPLETE, nullptr);
                total_fires++;
            }
        });
    }

    // Multiple update threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running, &total_updates, t]() {
            uint64_t step = t * 1000000;
            while (running.load()) {
                tcb_update_metrics(ctx, 1.0f / (step + 1), 0.01f, step, 0.5f);
                step++;
                total_updates++;
            }
        });
    }

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running.store(false);

    for (auto& t : threads) {
        t.join();
    }

    printf("Concurrent stress: %d fires, %d updates\n",
           total_fires.load(), total_updates.load());

    // Should process significant number of operations
    EXPECT_GT(total_fires.load(), 100000);
    EXPECT_GT(total_updates.load(), 10000);
}

// =============================================================================
// Security Context Regression Tests
// =============================================================================

TEST_F(TrainingCallbacksRegressionTest, Security_RepeatedContexts) {
    // Test that security registration/unregistration works correctly
    for (int i = 0; i < 100; i++) {
        tcb_config_t config = tcb_config_default();
        config.security_ctx = security_ctx;
        tcb_context_t* temp = tcb_create(&config);
        ASSERT_NE(temp, nullptr);

        tcb_register_simple(temp, TCB_EVENT_STEP_COMPLETE,
                            minimal_callback, nullptr, "test");
        tcb_fire_event(temp, TCB_EVENT_STEP_COMPLETE, nullptr);

        tcb_destroy(temp);
    }

    SUCCEED();
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
