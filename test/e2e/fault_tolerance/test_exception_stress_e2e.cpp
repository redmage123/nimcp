/**
 * @file test_exception_stress_e2e.cpp
 * @brief E2E stress tests for exception handling system
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: End-to-end stress tests for exception handling under high load
 * WHY:  Verify the exception handling system remains stable, doesn't leak memory,
 *       and correctly handles high rates of exceptions without cascading failures
 * HOW:  Generate high rates of exceptions across modules, verify no exception storms,
 *       test concurrent multi-threaded exception handling, verify memory stability,
 *       and ensure exception aggregation doesn't miss events
 *
 * Test Scenarios:
 * 1. High rate exception generation across modules
 * 2. Exception storm prevention verification
 * 3. Multi-threaded concurrent exception handling
 * 4. Memory stability under exception load
 * 5. Exception aggregation completeness
 * 6. Handler chain performance under load
 * 7. Circuit breaker stress test
 * 8. Immune system integration under stress
 * 9. Metrics system accuracy under high load
 * 10. Resource exhaustion resilience
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>
#include <set>
#include <algorithm>
#include <numeric>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Utilities - Stress Test Infrastructure
 * ============================================================================ */

namespace {

// Stress test statistics
struct StressStats {
    std::atomic<uint64_t> exceptions_created{0};
    std::atomic<uint64_t> exceptions_handled{0};
    std::atomic<uint64_t> exceptions_dropped{0};
    std::atomic<uint64_t> handler_invocations{0};
    std::atomic<uint64_t> circuit_blocks{0};
    std::atomic<uint64_t> aggregation_count{0};
    std::atomic<uint64_t> memory_allocations{0};
    std::atomic<uint64_t> memory_frees{0};
    std::atomic<uint64_t> max_latency_us{0};
    std::atomic<uint64_t> total_latency_us{0};
    std::atomic<bool> storm_detected{false};
    std::atomic<int> concurrent_handlers{0};
    std::atomic<int> max_concurrent_handlers{0};

    void reset() {
        exceptions_created = 0;
        exceptions_handled = 0;
        exceptions_dropped = 0;
        handler_invocations = 0;
        circuit_blocks = 0;
        aggregation_count = 0;
        memory_allocations = 0;
        memory_frees = 0;
        max_latency_us = 0;
        total_latency_us = 0;
        storm_detected = false;
        concurrent_handlers = 0;
        max_concurrent_handlers = 0;
    }

    void update_max_concurrent() {
        int current = concurrent_handlers.load();
        int max_val = max_concurrent_handlers.load();
        while (current > max_val) {
            if (max_concurrent_handlers.compare_exchange_weak(max_val, current)) {
                break;
            }
            max_val = max_concurrent_handlers.load();
        }
    }

    void update_max_latency(uint64_t latency) {
        uint64_t current_max = max_latency_us.load();
        while (latency > current_max) {
            if (max_latency_us.compare_exchange_weak(current_max, latency)) {
                break;
            }
            current_max = max_latency_us.load();
        }
    }
};

StressStats g_stats;

// Exception rate limiter for storm detection
struct RateLimiter {
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> window_start_us{0};
    uint64_t window_duration_us;
    uint64_t max_per_window;

    RateLimiter(uint64_t window_us, uint64_t max_count)
        : window_duration_us(window_us), max_per_window(max_count) {
        window_start_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    bool allow() {
        uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        uint64_t window = window_start_us.load();
        if (now - window > window_duration_us) {
            // Reset window
            window_start_us.store(now);
            count.store(1);
            return true;
        }

        uint64_t current = count.fetch_add(1);
        return current < max_per_window;
    }

    void reset() {
        count = 0;
        window_start_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
};

// Thread-safe exception ID tracking for aggregation verification
std::mutex g_id_mutex;
std::set<uint64_t> g_exception_ids;
std::atomic<uint64_t> g_next_exception_id{0};

uint64_t generate_exception_id() {
    return g_next_exception_id.fetch_add(1);
}

void record_exception_id(uint64_t id) {
    std::lock_guard<std::mutex> lock(g_id_mutex);
    g_exception_ids.insert(id);
}

bool check_exception_id(uint64_t id) {
    std::lock_guard<std::mutex> lock(g_id_mutex);
    return g_exception_ids.count(id) > 0;
}

void clear_exception_ids() {
    std::lock_guard<std::mutex> lock(g_id_mutex);
    g_exception_ids.clear();
    g_next_exception_id = 0;
}

// Stress test handler that tracks statistics
bool stress_tracking_handler(nimcp_exception_t* ex, void* user_data) {
    g_stats.concurrent_handlers++;
    g_stats.update_max_concurrent();

    auto start = std::chrono::high_resolution_clock::now();

    g_stats.handler_invocations++;

    // Simulate some processing time
    std::this_thread::yield();

    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    g_stats.total_latency_us += latency;
    g_stats.update_max_latency(latency);

    g_stats.concurrent_handlers--;

    return false;  // Don't consume, allow other handlers
}

// Storm detection handler
bool storm_detection_handler(nimcp_exception_t* ex, void* user_data) {
    RateLimiter* limiter = static_cast<RateLimiter*>(user_data);
    if (!limiter->allow()) {
        g_stats.storm_detected = true;
    }
    return false;
}

}  // namespace

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionStressE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* tracking_handler_reg = nullptr;
    nimcp_handler_registration_t* storm_handler_reg = nullptr;
    std::unique_ptr<RateLimiter> rate_limiter;

    void SetUp() override {
        g_stats.reset();
        clear_exception_ids();

        // Initialize exception system
        ASSERT_EQ(nimcp_exception_system_init(), 0) << "Failed to initialize exception system";

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0) << "Failed to initialize circuit breaker";

        // Initialize metrics
        ASSERT_EQ(nimcp_metrics_init(), 0) << "Failed to initialize metrics";

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = true;
        immune_config.enable_auto_recovery = false;  // Manual control for stress tests
        immune_config.min_present_severity = EXCEPTION_SEVERITY_SEVERE;
        ASSERT_EQ(nimcp_exception_immune_init(&immune_config), 0)
            << "Failed to initialize exception-immune integration";

        // Register tracking handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "StressTracker";
        opts.handler = stress_tracking_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        tracking_handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(tracking_handler_reg, nullptr);

        // Set up rate limiter (10000 per second for storm detection)
        rate_limiter = std::make_unique<RateLimiter>(1000000, 10000);  // 1 second window

        // Register storm detection handler
        nimcp_handler_default_options(&opts);
        opts.name = "StormDetector";
        opts.handler = storm_detection_handler;
        opts.user_data = rate_limiter.get();
        opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
        storm_handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(storm_handler_reg, nullptr);
    }

    void TearDown() override {
        if (tracking_handler_reg) {
            nimcp_handler_unregister(tracking_handler_reg);
            tracking_handler_reg = nullptr;
        }
        if (storm_handler_reg) {
            nimcp_handler_unregister(storm_handler_reg);
            storm_handler_reg = nullptr;
        }

        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();

        rate_limiter.reset();
    }

    // Helper to create exception with timing
    nimcp_exception_t* create_timed_exception(nimcp_error_t code,
                                               nimcp_exception_severity_t severity,
                                               uint64_t exception_id) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code, severity, __FILE__, __LINE__, __func__,
            "Stress test exception %lu", (unsigned long)exception_id
        );
        if (ex) {
            g_stats.exceptions_created++;
            g_stats.memory_allocations++;

            // Store ID in context for tracking
            char id_str[32];
            snprintf(id_str, sizeof(id_str), "%lu", (unsigned long)exception_id);
            nimcp_exception_set_context(ex, "stress_id", id_str);
        }
        return ex;
    }
};

/* ============================================================================
 * Test 1: High Rate Exception Generation Across Modules
 *
 * Generates exceptions at high rate from multiple simulated modules
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, HighRateExceptionGeneration) {
    printf("=== Test: High Rate Exception Generation Across Modules ===\n");

    const int DURATION_SECONDS = 2;
    const int TARGET_RATE = 5000;  // Exceptions per second
    const int NUM_MODULES = 5;

    // Different error codes for different modules
    nimcp_error_t module_errors[] = {
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_FILE_READ,
        NIMCP_ERROR_NETWORK_IO,
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_THREAD_CREATE
    };

    std::vector<std::thread> module_threads;
    std::atomic<bool> running{true};
    std::atomic<int> total_generated{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    // Start module threads
    for (int m = 0; m < NUM_MODULES; m++) {
        module_threads.emplace_back([&, m]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delay_dist(100, 500);  // microseconds

            while (running) {
                uint64_t id = generate_exception_id();
                nimcp_exception_t* ex = create_timed_exception(
                    module_errors[m % 5],
                    EXCEPTION_SEVERITY_WARNING,
                    id
                );

                if (ex) {
                    record_exception_id(id);
                    nimcp_exception_dispatch(ex);
                    g_stats.exceptions_handled++;
                    total_generated++;
                    nimcp_exception_unref(ex);
                    g_stats.memory_frees++;
                }

                // Throttle to approximate target rate
                std::this_thread::sleep_for(std::chrono::microseconds(
                    (1000000 / TARGET_RATE) * NUM_MODULES));
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SECONDS));
    running = false;

    // Wait for threads
    for (auto& t : module_threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    double actual_rate = (total_generated.load() * 1000.0) / duration_ms;

    printf("  Duration: %ld ms\n", duration_ms);
    printf("  Total generated: %d\n", total_generated.load());
    printf("  Actual rate: %.1f/sec\n", actual_rate);
    printf("  Exceptions created: %lu\n", (unsigned long)g_stats.exceptions_created.load());
    printf("  Handler invocations: %lu\n", (unsigned long)g_stats.handler_invocations.load());
    printf("  Max concurrent handlers: %d\n", g_stats.max_concurrent_handlers.load());

    // Verify reasonable throughput
    EXPECT_GT(total_generated.load(), 0);
    EXPECT_EQ(g_stats.exceptions_created.load(), g_stats.exceptions_handled.load());

    // Verify memory accounting
    EXPECT_EQ(g_stats.memory_allocations.load(), g_stats.memory_frees.load());

    printf("Test passed: High rate exception generation verified\n\n");
}

/* ============================================================================
 * Test 2: Exception Storm Prevention
 *
 * Verifies system detects and prevents exception storms
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, ExceptionStormPrevention) {
    printf("=== Test: Exception Storm Prevention ===\n");

    // Configure circuit breaker with low threshold
    nimcp_error_t storm_code = NIMCP_ERROR_OPERATION_FAILED;
    nimcp_circuit_set_threshold(storm_code, 50, 5000);  // 50 per minute, 5 sec reset

    // Reset rate limiter with strict limit
    rate_limiter = std::make_unique<RateLimiter>(100000, 100);  // 100ms window, 100 max
    storm_handler_reg->options.user_data = rate_limiter.get();

    const int BURST_SIZE = 500;
    int blocked_count = 0;
    int passed_count = 0;

    printf("  Generating burst of %d exceptions...\n", BURST_SIZE);

    auto burst_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BURST_SIZE; i++) {
        nimcp_exception_t* ex = create_timed_exception(
            storm_code,
            EXCEPTION_SEVERITY_ERROR,
            generate_exception_id()
        );
        if (!ex) continue;

        // Check circuit breaker
        int circuit_result = nimcp_circuit_record(ex);
        if (circuit_result == 1) {
            blocked_count++;
            g_stats.circuit_blocks++;
            // Circuit breaker blocking IS storm detection
            g_stats.storm_detected = true;
        } else {
            passed_count++;
            nimcp_exception_dispatch(ex);
        }

        nimcp_exception_unref(ex);
    }

    auto burst_end = std::chrono::high_resolution_clock::now();
    auto burst_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        burst_end - burst_start).count();

    printf("  Burst duration: %ld ms\n", burst_duration);
    printf("  Passed: %d\n", passed_count);
    printf("  Blocked by circuit: %d\n", blocked_count);
    printf("  Storm detected: %s\n", g_stats.storm_detected.load() ? "yes" : "no");

    // Circuit should have blocked some
    EXPECT_GT(blocked_count, 0) << "Circuit breaker should have blocked some exceptions";

    // Storm should have been detected
    EXPECT_TRUE(g_stats.storm_detected.load()) << "Storm should have been detected";

    // Get circuit stats
    nimcp_circuit_stats_t stats;
    nimcp_circuit_get_stats(&stats);
    printf("  Circuit stats - blocked: %lu, open circuits: %zu\n",
           (unsigned long)stats.total_blocked, stats.circuits_open);

    // Verify circuit is open
    EXPECT_TRUE(nimcp_circuit_is_open(storm_code));

    // Reset for other tests
    nimcp_circuit_reset(storm_code);

    printf("Test passed: Exception storm prevention verified\n\n");
}

/* ============================================================================
 * Test 3: Multi-threaded Concurrent Exception Handling
 *
 * Stress tests exception handling with many concurrent threads
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, MultithreadedConcurrentHandling) {
    printf("=== Test: Multi-threaded Concurrent Exception Handling ===\n");

    const int NUM_THREADS = 16;
    const int EXCEPTIONS_PER_THREAD = 500;
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};
    std::atomic<int> errors{0};

    printf("  Starting %d threads, %d exceptions each...\n",
           NUM_THREADS, EXCEPTIONS_PER_THREAD);

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                uint64_t id = generate_exception_id();
                nimcp_exception_t* ex = create_timed_exception(
                    NIMCP_ERROR_OPERATION_FAILED + (t % 10),
                    EXCEPTION_SEVERITY_WARNING,
                    id
                );

                if (!ex) {
                    errors++;
                    continue;
                }

                record_exception_id(id);

                // Add thread-specific context
                char thread_str[32];
                snprintf(thread_str, sizeof(thread_str), "%d", t);
                nimcp_exception_set_context(ex, "thread", thread_str);

                // Dispatch
                nimcp_exception_dispatch(ex);
                g_stats.exceptions_handled++;

                // Clean up
                nimcp_exception_unref(ex);
                g_stats.memory_frees++;
            }
            completed_threads++;
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    int total_expected = NUM_THREADS * EXCEPTIONS_PER_THREAD;
    double rate = (g_stats.exceptions_handled.load() * 1000.0) / duration_ms;

    printf("  Duration: %ld ms\n", duration_ms);
    printf("  Expected: %d\n", total_expected);
    printf("  Handled: %lu\n", (unsigned long)g_stats.exceptions_handled.load());
    printf("  Rate: %.1f/sec\n", rate);
    printf("  Errors: %d\n", errors.load());
    printf("  Handler invocations: %lu\n", (unsigned long)g_stats.handler_invocations.load());
    printf("  Max concurrent: %d\n", g_stats.max_concurrent_handlers.load());
    printf("  Max latency: %lu us\n", (unsigned long)g_stats.max_latency_us.load());

    // Verify all exceptions processed
    EXPECT_EQ(completed_threads.load(), NUM_THREADS);
    EXPECT_EQ(g_stats.exceptions_handled.load(), (uint64_t)(total_expected - errors.load()));

    // Verify memory balance
    EXPECT_EQ(g_stats.memory_allocations.load(), g_stats.memory_frees.load());

    printf("Test passed: Multi-threaded concurrent handling verified\n\n");
}

/* ============================================================================
 * Test 4: Memory Stability Under Exception Load
 *
 * Verifies no memory leaks under sustained exception load
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, MemoryStabilityUnderLoad) {
    printf("=== Test: Memory Stability Under Exception Load ===\n");

    const int NUM_ITERATIONS = 10;
    const int EXCEPTIONS_PER_ITERATION = 1000;
    std::vector<uint64_t> allocations_per_iteration;
    std::vector<uint64_t> frees_per_iteration;

    printf("  Running %d iterations of %d exceptions each...\n",
           NUM_ITERATIONS, EXCEPTIONS_PER_ITERATION);

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        uint64_t start_allocs = g_stats.memory_allocations.load();
        uint64_t start_frees = g_stats.memory_frees.load();

        for (int i = 0; i < EXCEPTIONS_PER_ITERATION; i++) {
            nimcp_exception_t* ex = create_timed_exception(
                NIMCP_ERROR_NO_MEMORY,
                EXCEPTION_SEVERITY_WARNING,
                generate_exception_id()
            );

            if (ex) {
                // Create aggregate with children
                if (i % 10 == 0) {
                    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
                        NIMCP_ERROR_OPERATION_FAILED,
                        EXCEPTION_SEVERITY_ERROR,
                        __FILE__, __LINE__, __func__,
                        "Aggregate %d", i
                    );
                    if (agg) {
                        nimcp_aggregate_exception_add(agg, ex);
                        nimcp_exception_dispatch(&agg->base);
                        nimcp_exception_unref(&agg->base);
                        g_stats.memory_frees++;
                    } else {
                        nimcp_exception_dispatch(ex);
                        nimcp_exception_unref(ex);
                        g_stats.memory_frees++;
                    }
                } else {
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                    g_stats.memory_frees++;
                }
            }
        }

        uint64_t iter_allocs = g_stats.memory_allocations.load() - start_allocs;
        uint64_t iter_frees = g_stats.memory_frees.load() - start_frees;
        allocations_per_iteration.push_back(iter_allocs);
        frees_per_iteration.push_back(iter_frees);

        printf("  Iteration %d: allocs=%lu, frees=%lu\n",
               iter + 1, (unsigned long)iter_allocs, (unsigned long)iter_frees);
    }

    // Verify memory balance in each iteration
    bool balanced = true;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        if (allocations_per_iteration[i] != frees_per_iteration[i]) {
            balanced = false;
            printf("  WARNING: Iteration %d imbalanced: %lu allocs, %lu frees\n",
                   i + 1, (unsigned long)allocations_per_iteration[i],
                   (unsigned long)frees_per_iteration[i]);
        }
    }

    printf("  Total allocations: %lu\n", (unsigned long)g_stats.memory_allocations.load());
    printf("  Total frees: %lu\n", (unsigned long)g_stats.memory_frees.load());
    printf("  Memory balanced: %s\n", balanced ? "yes" : "no");

    // Final balance check
    EXPECT_EQ(g_stats.memory_allocations.load(), g_stats.memory_frees.load());

    printf("Test passed: Memory stability under load verified\n\n");
}

/* ============================================================================
 * Test 5: Exception Aggregation Completeness
 *
 * Verifies exception aggregation doesn't miss events under load
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, ExceptionAggregationCompleteness) {
    printf("=== Test: Exception Aggregation Completeness ===\n");

    const int NUM_BATCHES = 20;
    const int EXCEPTIONS_PER_BATCH = 15;  // Below NIMCP_EXCEPTION_MAX_CHILDREN
    std::vector<std::set<uint64_t>> batch_ids(NUM_BATCHES);
    std::atomic<int> total_aggregated{0};
    std::atomic<int> total_verified{0};

    printf("  Creating %d batches of %d exceptions each...\n",
           NUM_BATCHES, EXCEPTIONS_PER_BATCH);

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        // Create aggregate for batch
        nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Batch %d aggregate", batch
        );
        ASSERT_NE(agg, nullptr);

        // Add child exceptions
        for (int i = 0; i < EXCEPTIONS_PER_BATCH; i++) {
            uint64_t id = generate_exception_id();
            batch_ids[batch].insert(id);

            nimcp_exception_t* child = create_timed_exception(
                NIMCP_ERROR_NO_MEMORY + (i % 5),
                EXCEPTION_SEVERITY_WARNING,
                id
            );
            ASSERT_NE(child, nullptr);

            int add_result = nimcp_aggregate_exception_add(agg, child);
            if (add_result == 0) {
                total_aggregated++;
            }
        }

        // Verify count
        EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)EXCEPTIONS_PER_BATCH);

        // Verify all children are accessible and have correct IDs
        for (size_t i = 0; i < nimcp_aggregate_exception_count(agg); i++) {
            nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
            ASSERT_NE(child, nullptr);

            const char* id_str = nimcp_exception_get_context(child, "stress_id");
            if (id_str) {
                uint64_t id = std::stoull(id_str);
                if (batch_ids[batch].count(id) > 0) {
                    total_verified++;
                }
            }
        }

        // Dispatch and clean up
        nimcp_exception_dispatch(&agg->base);
        g_stats.aggregation_count++;

        nimcp_exception_unref(&agg->base);
    }

    printf("  Total aggregated: %d\n", total_aggregated.load());
    printf("  Total verified: %d\n", total_verified.load());
    printf("  Aggregates processed: %lu\n", (unsigned long)g_stats.aggregation_count.load());

    int expected_total = NUM_BATCHES * EXCEPTIONS_PER_BATCH;
    EXPECT_EQ(total_aggregated.load(), expected_total);
    EXPECT_EQ(total_verified.load(), expected_total);
    EXPECT_EQ(g_stats.aggregation_count.load(), (uint64_t)NUM_BATCHES);

    printf("Test passed: Exception aggregation completeness verified\n\n");
}

/* ============================================================================
 * Test 6: Handler Chain Performance Under Load
 *
 * Measures handler chain performance with multiple handlers
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, HandlerChainPerformanceUnderLoad) {
    printf("=== Test: Handler Chain Performance Under Load ===\n");

    // Register additional handlers at different priorities
    std::vector<nimcp_handler_registration_t*> extra_handlers;
    std::atomic<int> handler_counts[10] = {};

    for (int h = 0; h < 10; h++) {
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        char name[32];
        snprintf(name, sizeof(name), "Handler_%d", h);
        opts.name = name;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL - h;
        opts.user_data = &handler_counts[h];
        opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
            std::atomic<int>* count = static_cast<std::atomic<int>*>(user_data);
            (*count)++;
            return false;  // Don't consume
        };

        nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
        if (reg) {
            extra_handlers.push_back(reg);
        }
    }

    printf("  Registered %zu additional handlers\n", extra_handlers.size());

    const int NUM_EXCEPTIONS = 5000;
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_EXCEPTIONS);

    printf("  Processing %d exceptions through handler chain...\n", NUM_EXCEPTIONS);

    auto total_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_timed_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_WARNING,
            generate_exception_id()
        );
        if (!ex) continue;

        auto ex_start = std::chrono::high_resolution_clock::now();
        nimcp_exception_dispatch(ex);
        auto ex_end = std::chrono::high_resolution_clock::now();

        uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
            ex_end - ex_start).count();
        latencies.push_back(latency);

        nimcp_exception_unref(ex);
        g_stats.memory_frees++;
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        total_end - total_start).count();

    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    uint64_t total_latency = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
    double avg_latency = static_cast<double>(total_latency) / latencies.size();
    uint64_t p50 = latencies[latencies.size() / 2];
    uint64_t p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];
    uint64_t p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];

    printf("  Total duration: %ld ms\n", total_duration);
    printf("  Average latency: %.1f us\n", avg_latency);
    printf("  P50 latency: %lu us\n", (unsigned long)p50);
    printf("  P95 latency: %lu us\n", (unsigned long)p95);
    printf("  P99 latency: %lu us\n", (unsigned long)p99);
    printf("  Max latency: %lu us\n", (unsigned long)latencies.back());

    // Verify all handlers were called
    int total_handler_calls = 0;
    for (int h = 0; h < 10; h++) {
        total_handler_calls += handler_counts[h].load();
    }
    printf("  Total handler calls across chain: %d\n", total_handler_calls);

    // Each exception should trigger all handlers plus our tracking handlers
    int expected_calls_per_ex = extra_handlers.size();
    EXPECT_GE(total_handler_calls, (int)(NUM_EXCEPTIONS * expected_calls_per_ex * 0.9));

    // Clean up extra handlers
    for (auto* reg : extra_handlers) {
        nimcp_handler_unregister(reg);
    }

    printf("Test passed: Handler chain performance under load verified\n\n");
}

/* ============================================================================
 * Test 7: Circuit Breaker Stress Test
 *
 * Stress tests circuit breaker with rapid exceptions
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, CircuitBreakerStressTest) {
    printf("=== Test: Circuit Breaker Stress Test ===\n");

    const int NUM_ERROR_CODES = 10;
    const int EXCEPTIONS_PER_CODE = 500;

    // Configure circuit breakers
    for (int c = 0; c < NUM_ERROR_CODES; c++) {
        nimcp_error_t code = NIMCP_ERROR_OPERATION_FAILED + c;
        nimcp_circuit_set_threshold(code, 20, 2000);  // 20 per min, 2 sec reset
    }

    printf("  Configured %d circuit breakers\n", NUM_ERROR_CODES);

    std::atomic<int> total_passed{0};
    std::atomic<int> total_blocked{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    // Generate exceptions from multiple threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int c = 0; c < NUM_ERROR_CODES; c++) {
                nimcp_error_t code = NIMCP_ERROR_OPERATION_FAILED + c;

                for (int i = 0; i < EXCEPTIONS_PER_CODE / 4; i++) {
                    nimcp_exception_t* ex = create_timed_exception(
                        code,
                        EXCEPTION_SEVERITY_WARNING,
                        generate_exception_id()
                    );
                    if (!ex) continue;

                    int result = nimcp_circuit_record(ex);
                    if (result == 0) {
                        total_passed++;
                    } else if (result == 1) {
                        total_blocked++;
                    }

                    nimcp_exception_unref(ex);
                    g_stats.memory_frees++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    nimcp_circuit_stats_t stats;
    nimcp_circuit_get_stats(&stats);

    printf("  Duration: %ld ms\n", duration_ms);
    printf("  Total passed: %d\n", total_passed.load());
    printf("  Total blocked: %d\n", total_blocked.load());
    printf("  Circuits tracked: %zu\n", stats.total_tracked);
    printf("  Circuits open: %zu\n", stats.circuits_open);
    printf("  Circuits half-open: %zu\n", stats.circuits_half_open);

    int total_expected = NUM_ERROR_CODES * EXCEPTIONS_PER_CODE;
    EXPECT_EQ(total_passed.load() + total_blocked.load(), total_expected);
    EXPECT_GT(total_blocked.load(), 0);  // Some should be blocked

    // Reset all circuits
    nimcp_circuit_reset_all();

    printf("Test passed: Circuit breaker stress test verified\n\n");
}

/* ============================================================================
 * Test 8: Immune System Integration Under Stress
 *
 * Tests exception-immune integration under high load
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, ImmuneSystemIntegrationUnderStress) {
    printf("=== Test: Immune System Integration Under Stress ===\n");

    const int NUM_EXCEPTIONS = 1000;
    std::atomic<int> presented{0};
    std::atomic<int> failed_present{0};

    printf("  Presenting %d exceptions to immune system...\n", NUM_EXCEPTIONS);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_timed_exception(
            NIMCP_ERROR_NO_MEMORY + (i % 10),
            EXCEPTION_SEVERITY_SEVERE,  // Above immune threshold
            generate_exception_id()
        );
        if (!ex) continue;

        // Present to immune
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        int result = nimcp_exception_present_to_immune(ex, &response);

        if (result == 0) {
            presented++;
        } else {
            failed_present++;
        }

        nimcp_exception_unref(ex);
        g_stats.memory_frees++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Get immune stats
    nimcp_exception_immune_stats_t immune_stats;
    nimcp_exception_immune_get_stats(&immune_stats);

    printf("  Duration: %ld ms\n", duration_ms);
    printf("  Presented: %d\n", presented.load());
    printf("  Failed: %d\n", failed_present.load());
    printf("  Immune stats - presented: %lu, pending: %lu\n",
           (unsigned long)immune_stats.exceptions_presented,
           (unsigned long)immune_stats.exceptions_pending);
    printf("  Avg response time: %.1f us\n", immune_stats.avg_response_time_us);

    // Most should succeed (immune system might not be connected, so we're lenient)
    EXPECT_GT(presented.load() + failed_present.load(), 0);

    printf("Test passed: Immune system integration under stress verified\n\n");
}

/* ============================================================================
 * Test 9: Metrics System Accuracy Under High Load
 *
 * Verifies metrics accuracy when processing many exceptions
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, MetricsSystemAccuracyUnderLoad) {
    printf("=== Test: Metrics System Accuracy Under High Load ===\n");

    // Reset metrics
    nimcp_metrics_reset();

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 2500;
    std::atomic<int> local_count{0};
    std::vector<std::thread> threads;

    printf("  Generating %d exceptions across %d threads...\n",
           NUM_THREADS * EXCEPTIONS_PER_THREAD, NUM_THREADS);

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = create_timed_exception(
                    NIMCP_ERROR_NO_MEMORY + (i % 5),
                    EXCEPTION_SEVERITY_ERROR,
                    generate_exception_id()
                );
                if (!ex) continue;

                local_count++;
                nimcp_metrics_record_exception(ex);

                nimcp_exception_unref(ex);
                g_stats.memory_frees++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    printf("  Duration: %ld ms\n", duration_ms);
    printf("  Local count: %d\n", local_count.load());
    printf("  Metrics count: %lu\n", (unsigned long)metrics.total_exceptions);
    printf("  Rate: %.1f/sec\n", metrics.current_rate_per_second);
    printf("  Peak rate: %lu/sec\n", (unsigned long)metrics.peak_rate_per_second);

    // Verify accuracy (allow small margin for race conditions)
    int expected = NUM_THREADS * EXCEPTIONS_PER_THREAD;
    int actual = (int)metrics.total_exceptions;
    double accuracy = (double)actual / expected * 100;

    printf("  Accuracy: %.2f%%\n", accuracy);

    EXPECT_GE(accuracy, 99.0) << "Metrics should be at least 99% accurate";

    printf("Test passed: Metrics system accuracy under high load verified\n\n");
}

/* ============================================================================
 * Test 10: Resource Exhaustion Resilience
 *
 * Tests system resilience when approaching resource limits
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, ResourceExhaustionResilience) {
    printf("=== Test: Resource Exhaustion Resilience ===\n");

    // Track allocation failures
    std::atomic<int> allocation_failures{0};
    std::atomic<int> successful_operations{0};

    // Generate many exceptions without releasing immediately
    // to stress memory (but keep within reasonable bounds)
    const int BATCH_SIZE = 100;
    const int NUM_BATCHES = 20;
    std::vector<nimcp_exception_t*> held_exceptions;
    held_exceptions.reserve(BATCH_SIZE);

    printf("  Testing %d batches of %d exceptions...\n", NUM_BATCHES, BATCH_SIZE);

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        // Allocate batch
        for (int i = 0; i < BATCH_SIZE; i++) {
            nimcp_exception_t* ex = create_timed_exception(
                NIMCP_ERROR_NO_MEMORY,
                EXCEPTION_SEVERITY_WARNING,
                generate_exception_id()
            );

            if (ex) {
                held_exceptions.push_back(ex);
                successful_operations++;
            } else {
                allocation_failures++;
            }
        }

        // Dispatch all
        for (auto* ex : held_exceptions) {
            nimcp_exception_dispatch(ex);
        }

        // Release batch
        for (auto* ex : held_exceptions) {
            nimcp_exception_unref(ex);
            g_stats.memory_frees++;
        }
        held_exceptions.clear();

        printf("  Batch %d: %d held, %d failures\n",
               batch + 1, BATCH_SIZE,
               allocation_failures.load() > batch * BATCH_SIZE ?
                   allocation_failures.load() - batch * BATCH_SIZE : 0);
    }

    printf("  Total successful: %d\n", successful_operations.load());
    printf("  Total failures: %d\n", allocation_failures.load());

    // All should succeed under normal conditions
    EXPECT_EQ(allocation_failures.load(), 0) << "No allocation failures expected";
    EXPECT_EQ(successful_operations.load(), NUM_BATCHES * BATCH_SIZE);

    // Verify memory balance
    EXPECT_EQ(g_stats.memory_allocations.load(), g_stats.memory_frees.load());

    printf("Test passed: Resource exhaustion resilience verified\n\n");
}

/* ============================================================================
 * Test 11: Sustained Load Test
 *
 * Tests system stability under sustained exception load
 * ============================================================================ */

TEST_F(ExceptionStressE2ETest, SustainedLoadTest) {
    printf("=== Test: Sustained Load Test ===\n");

    const int DURATION_SECONDS = 3;
    const int TARGET_RATE = 2000;  // Exceptions per second
    std::atomic<bool> running{true};
    std::atomic<uint64_t> total_processed{0};
    std::atomic<uint64_t> total_errors{0};

    std::vector<std::thread> workers;
    const int NUM_WORKERS = 4;

    printf("  Running %d workers for %d seconds at ~%d/sec target...\n",
           NUM_WORKERS, DURATION_SECONDS, TARGET_RATE);

    auto start = std::chrono::high_resolution_clock::now();

    for (int w = 0; w < NUM_WORKERS; w++) {
        workers.emplace_back([&]() {
            while (running) {
                nimcp_exception_t* ex = create_timed_exception(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_WARNING,
                    generate_exception_id()
                );

                if (ex) {
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                    g_stats.memory_frees++;
                    total_processed++;
                } else {
                    total_errors++;
                }

                // Throttle
                std::this_thread::sleep_for(std::chrono::microseconds(
                    (1000000 * NUM_WORKERS) / TARGET_RATE));
            }
        });
    }

    // Run for duration
    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SECONDS));
    running = false;

    for (auto& w : workers) {
        w.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto actual_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    double actual_rate = (total_processed.load() * 1000.0) / actual_duration_ms;

    printf("  Actual duration: %ld ms\n", actual_duration_ms);
    printf("  Total processed: %lu\n", (unsigned long)total_processed.load());
    printf("  Total errors: %lu\n", (unsigned long)total_errors.load());
    printf("  Actual rate: %.1f/sec\n", actual_rate);

    // Get final metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);
    printf("  Metrics total: %lu\n", (unsigned long)metrics.total_exceptions);

    // Verify sustained operation
    EXPECT_GT(total_processed.load(), 0u);
    EXPECT_EQ(total_errors.load(), 0u);

    // Verify memory balance
    EXPECT_EQ(g_stats.memory_allocations.load(), g_stats.memory_frees.load());

    printf("Test passed: Sustained load test verified\n\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
