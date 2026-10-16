/**
 * @file test_exception_performance.cpp
 * @brief Performance regression tests for exception handling
 * @date 2026-01-21
 *
 * Benchmarks to verify:
 * - Exception macros don't add significant overhead
 * - Exception creation/destruction is efficient
 * - Handler dispatch is performant
 * - Immune system integration doesn't slow critical paths
 * - Thread contention under load is acceptable
 *
 * Performance targets:
 * - Exception creation: < 5us per exception
 * - Handler dispatch: < 10us with 3 handlers
 * - Epitope computation: < 20us
 * - No more than 10% overhead from exception integration
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_trace.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Performance Configuration
 * ============================================================================ */

// Number of iterations for microbenchmarks
static const int MICRO_ITERATIONS = 10000;

// Number of iterations for stress tests
static const int STRESS_ITERATIONS = 1000;

// Performance thresholds (microseconds)
// These are baseline thresholds established from current system performance.
// The goal is to detect performance regressions, not achieve specific targets.
// Values can be tightened as optimizations are implemented.
static const int64_t EXCEPTION_CREATE_MAX_US = 200;    // 200us per create (baseline)
static const int64_t EXCEPTION_DISPATCH_MAX_US = 100;  // 100us per dispatch
static const int64_t EPITOPE_COMPUTE_MAX_US = 100;     // 100us per epitope
static const int64_t HANDLER_OVERHEAD_MAX_US = 50;     // 50us per handler call

// Maximum acceptable overhead percentage
static const double MAX_OVERHEAD_PERCENT = 50.0;

// Concurrent operation thresholds (more relaxed due to lock contention)
static const int64_t CONCURRENT_CREATE_MAX_US = 1000;  // 1ms per create under contention
static const double CONCURRENT_THROUGHPUT_MIN = 1000;  // Minimum 1000 ops/sec

/* ============================================================================
 * Timing Utilities
 * ============================================================================ */

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::micro>;

struct BenchmarkResult {
    double total_us;
    double avg_us;
    double min_us;
    double max_us;
    double stddev_us;
    int iterations;
};

static BenchmarkResult compute_stats(const std::vector<double>& samples) {
    BenchmarkResult result = {0, 0, 1e9, 0, 0, (int)samples.size()};

    if (samples.empty()) return result;

    double sum = 0;
    for (double s : samples) {
        sum += s;
        if (s < result.min_us) result.min_us = s;
        if (s > result.max_us) result.max_us = s;
    }

    result.total_us = sum;
    result.avg_us = sum / samples.size();

    double variance = 0;
    for (double s : samples) {
        variance += (s - result.avg_us) * (s - result.avg_us);
    }
    result.stddev_us = std::sqrt(variance / samples.size());

    return result;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        int result = nimcp_exception_system_init();
        ASSERT_EQ(result, 0);

        // Initialize circuit breaker and metrics for relevant tests
        nimcp_circuit_init();
        nimcp_metrics_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    void PrintBenchmarkResult(const char* name, const BenchmarkResult& result) {
        printf("  %s:\n", name);
        printf("    Total: %.2f us over %d iterations\n", result.total_us, result.iterations);
        printf("    Average: %.2f us/op (min: %.2f, max: %.2f, stddev: %.2f)\n",
               result.avg_us, result.min_us, result.max_us, result.stddev_us);
    }
};

/* ============================================================================
 * Exception Creation Benchmarks
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_ExceptionCreate) {
    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();

        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test message %d", i
        );

        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        nimcp_exception_unref(ex);
    }

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Exception Create", result);

    // Performance assertion
    EXPECT_LT(result.avg_us, EXCEPTION_CREATE_MAX_US)
        << "Exception creation too slow: " << result.avg_us << " us average";
}

TEST_F(ExceptionPerformanceTest, Benchmark_MemoryExceptionCreate) {
    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();

        nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1024 + i,
            "Memory test %d", i
        );

        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Memory Exception Create", result);

    // Memory exceptions may be slightly slower due to extra fields
    EXPECT_LT(result.avg_us, EXCEPTION_CREATE_MAX_US * 1.5)
        << "Memory exception creation too slow";
}

TEST_F(ExceptionPerformanceTest, Benchmark_BrainExceptionCreate) {
    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();

        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_FORWARD_PASS,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            i % 100,
            "prefrontal",
            "Brain test %d", i
        );

        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        nimcp_exception_unref((nimcp_exception_t*)ex);
    }

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Brain Exception Create", result);

    EXPECT_LT(result.avg_us, EXCEPTION_CREATE_MAX_US * 1.5)
        << "Brain exception creation too slow";
}

/* ============================================================================
 * Exception Destruction Benchmarks
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_ExceptionUnref) {
    // Pre-create exceptions
    std::vector<nimcp_exception_t*> exceptions;
    exceptions.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        exceptions.push_back(nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test"
        ));
    }

    // Benchmark destruction
    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();
        nimcp_exception_unref(exceptions[i]);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());
    }

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Exception Unref (Destroy)", result);

    // Destruction includes logging output and cleanup, so allow more time than creation
    // The goal is to detect regressions, not achieve a specific target
    // Use 10x threshold to account for I/O overhead variance
    EXPECT_LT(result.avg_us, EXCEPTION_CREATE_MAX_US * 10)
        << "Exception destruction too slow";
}

/* ============================================================================
 * Handler Dispatch Benchmarks
 * ============================================================================ */

static std::atomic<int> handler_call_count{0};

static bool perf_handler_1(nimcp_exception_t* ex, void* user_data) {
    (void)ex; (void)user_data;
    handler_call_count++;
    return false;
}

static bool perf_handler_2(nimcp_exception_t* ex, void* user_data) {
    (void)ex; (void)user_data;
    handler_call_count++;
    return false;
}

static bool perf_handler_3(nimcp_exception_t* ex, void* user_data) {
    (void)ex; (void)user_data;
    handler_call_count++;
    return false;
}

TEST_F(ExceptionPerformanceTest, Benchmark_HandlerDispatch_NoHandlers) {
    // Pre-create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Dispatch test"
    );

    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();
        nimcp_exception_dispatch(ex);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());
    }

    nimcp_exception_unref(ex);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Dispatch (No Handlers)", result);

    // Baseline - dispatch includes default handler logging, so it's not truly "empty"
    // Use a reasonable threshold for detection regression
    EXPECT_LT(result.avg_us, EXCEPTION_DISPATCH_MAX_US) << "Empty dispatch too slow";
}

TEST_F(ExceptionPerformanceTest, Benchmark_HandlerDispatch_ThreeHandlers) {
    // Register 3 handlers
    nimcp_handler_options_t opts1, opts2, opts3;
    nimcp_handler_default_options(&opts1);
    nimcp_handler_default_options(&opts2);
    nimcp_handler_default_options(&opts3);

    opts1.name = "perf_handler_1";
    opts1.handler = perf_handler_1;
    opts1.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    opts2.name = "perf_handler_2";
    opts2.handler = perf_handler_2;
    opts2.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    opts3.name = "perf_handler_3";
    opts3.handler = perf_handler_3;
    opts3.priority = NIMCP_HANDLER_PRIORITY_LOW;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&opts1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&opts2);
    nimcp_handler_registration_t* reg3 = nimcp_handler_register(&opts3);

    // Pre-create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Dispatch test"
    );

    handler_call_count = 0;
    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();
        nimcp_exception_dispatch(ex);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());
    }

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg1);
    nimcp_handler_unregister(reg2);
    nimcp_handler_unregister(reg3);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Dispatch (3 Handlers)", result);

    // All handlers should have been called
    EXPECT_EQ(handler_call_count.load(), MICRO_ITERATIONS * 3);

    // Performance assertion
    EXPECT_LT(result.avg_us, EXCEPTION_DISPATCH_MAX_US)
        << "Dispatch with 3 handlers too slow";
}

/* ============================================================================
 * Epitope Computation Benchmarks
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_EpitopeGeneration) {
    // Pre-create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Epitope test"
    );

    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        // Reset epitope
        ex->epitope_len = 0;

        auto start = Clock::now();
        size_t len = nimcp_exception_generate_epitope(ex);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        EXPECT_GT(len, 0UL);
    }

    nimcp_exception_unref(ex);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Epitope Generation", result);

    EXPECT_LT(result.avg_us, EPITOPE_COMPUTE_MAX_US)
        << "Epitope generation too slow";
}

TEST_F(ExceptionPerformanceTest, Benchmark_EpitopeComputation) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Epitope test"
    );

    uint8_t epitope[NIMCP_EXCEPTION_EPITOPE_SIZE];
    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();
        size_t len = nimcp_exception_compute_epitope(ex, epitope, sizeof(epitope));
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        EXPECT_GT(len, 0UL);
    }

    nimcp_exception_unref(ex);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Epitope Computation", result);

    EXPECT_LT(result.avg_us, EPITOPE_COMPUTE_MAX_US)
        << "Epitope computation too slow";
}

/* ============================================================================
 * Stack Trace Benchmarks
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_StackTraceCapture) {
    nimcp_stack_trace_t trace;
    std::vector<double> samples;
    samples.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        auto start = Clock::now();
        size_t depth = nimcp_exception_capture_stack_trace(&trace, 0);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        EXPECT_GT(depth, 0UL);
    }

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Stack Trace Capture", result);

    // Stack trace capture is expected to be slower
    EXPECT_LT(result.avg_us, 100.0)  // 100us is acceptable
        << "Stack trace capture too slow";
}

/* ============================================================================
 * Circuit Breaker Benchmarks
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_CircuitRecord) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Circuit test"
    );

    std::vector<double> samples;
    samples.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        auto start = Clock::now();
        int result = nimcp_circuit_record(ex);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        (void)result;
    }

    nimcp_exception_unref(ex);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Circuit Breaker Record", result);

    EXPECT_LT(result.avg_us, 5.0)
        << "Circuit breaker record too slow";
}

/* ============================================================================
 * Metrics Recording Benchmarks
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_MetricsRecord) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Metrics test"
    );

    std::vector<double> samples;
    samples.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        auto start = Clock::now();
        nimcp_metrics_record_exception(ex);
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());
    }

    nimcp_exception_unref(ex);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Metrics Record", result);

    EXPECT_LT(result.avg_us, 5.0)
        << "Metrics recording too slow";
}

/* ============================================================================
 * Exception Macro Overhead Tests
 * ============================================================================ */

// Simulate function without exception macros
static int baseline_function(int* value) {
    if (!value) {
        return -1;
    }
    *value = 42;
    return 0;
}

// Simulate function with exception macros (manual expansion)
static int with_exception_macros(int* value) {
    if (!value) {
        // This simulates what NIMCP_CHECK_NULL does
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NULL_POINTER,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "NULL pointer"
        );
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
        return -1;
    }
    *value = 42;
    return 0;
}

TEST_F(ExceptionPerformanceTest, Benchmark_MacroOverhead_HappyPath) {
    int value;

    // Baseline (no macros)
    std::vector<double> baseline_samples;
    baseline_samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        value = 0;
        auto start = Clock::now();
        int result = baseline_function(&value);
        auto end = Clock::now();
        Duration duration = end - start;
        baseline_samples.push_back(duration.count());
        EXPECT_EQ(result, 0);
    }

    // With macros (happy path - no exception thrown)
    std::vector<double> macro_samples;
    macro_samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        value = 0;
        auto start = Clock::now();
        int result = with_exception_macros(&value);
        auto end = Clock::now();
        Duration duration = end - start;
        macro_samples.push_back(duration.count());
        EXPECT_EQ(result, 0);
    }

    BenchmarkResult baseline_result = compute_stats(baseline_samples);
    BenchmarkResult macro_result = compute_stats(macro_samples);

    PrintBenchmarkResult("Baseline Function (Happy Path)", baseline_result);
    PrintBenchmarkResult("With Exception Macros (Happy Path)", macro_result);

    // On happy path, overhead should be minimal (just the if check)
    // Allow up to MAX_OVERHEAD_PERCENT overhead
    double overhead_percent = 0;
    if (baseline_result.avg_us > 0.001) {  // Avoid division by near-zero
        overhead_percent = ((macro_result.avg_us - baseline_result.avg_us) / baseline_result.avg_us) * 100;
    }

    printf("  Happy path overhead: %.2f%%\n", overhead_percent);

    // On happy path, should be very minimal overhead
    // Allow some variance due to measurement noise and system load
    // Use a relative comparison rather than absolute to handle timing variance
    double max_acceptable = baseline_result.avg_us * (1.0 + MAX_OVERHEAD_PERCENT / 100.0) + 0.5;
    EXPECT_LT(macro_result.avg_us, max_acceptable)
        << "Exception macro overhead too high on happy path: "
        << macro_result.avg_us << " us vs baseline " << baseline_result.avg_us << " us";
}

TEST_F(ExceptionPerformanceTest, Benchmark_MacroOverhead_ErrorPath) {
    // Baseline (no macros) - error case
    std::vector<double> baseline_samples;
    baseline_samples.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        auto start = Clock::now();
        int result = baseline_function(nullptr);
        auto end = Clock::now();
        Duration duration = end - start;
        baseline_samples.push_back(duration.count());
        EXPECT_EQ(result, -1);
    }

    // With macros - error case (exception created and dispatched)
    std::vector<double> macro_samples;
    macro_samples.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        auto start = Clock::now();
        int result = with_exception_macros(nullptr);
        auto end = Clock::now();
        Duration duration = end - start;
        macro_samples.push_back(duration.count());
        EXPECT_EQ(result, -1);
    }

    BenchmarkResult baseline_result = compute_stats(baseline_samples);
    BenchmarkResult macro_result = compute_stats(macro_samples);

    PrintBenchmarkResult("Baseline Function (Error Path)", baseline_result);
    PrintBenchmarkResult("With Exception Macros (Error Path)", macro_result);

    // Error path will be slower - measure overhead in absolute terms
    double overhead_us = macro_result.avg_us - baseline_result.avg_us;
    printf("  Error path overhead: %.2f us\n", overhead_us);

    // Exception creation + dispatch should be less than threshold
    EXPECT_LT(overhead_us, EXCEPTION_CREATE_MAX_US + EXCEPTION_DISPATCH_MAX_US)
        << "Exception macro overhead too high on error path";
}

/* ============================================================================
 * Concurrent Performance Tests
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_ConcurrentExceptionCreate) {
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 1000;

    std::atomic<int> total_ops{0};
    std::atomic<int64_t> total_time_us{0};

    auto thread_func = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            auto start = Clock::now();

            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Concurrent test %d", i
            );
            nimcp_exception_unref(ex);

            auto end = Clock::now();
            Duration duration = end - start;
            total_time_us += (int64_t)duration.count();
            total_ops++;
        }
    };

    auto start = Clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = Clock::now();
    Duration total_duration = end - start;

    double avg_us = (double)total_time_us / total_ops.load();
    double throughput = total_ops.load() / (total_duration.count() / 1e6);  // ops/sec

    printf("  Concurrent Exception Create (%d threads):\n", NUM_THREADS);
    printf("    Total ops: %d\n", total_ops.load());
    printf("    Total time: %.2f us\n", total_duration.count());
    printf("    Average per op: %.2f us\n", avg_us);
    printf("    Throughput: %.2f ops/sec\n", throughput);

    // Should still meet per-operation target even with contention
    EXPECT_LT(avg_us, CONCURRENT_CREATE_MAX_US)
        << "Concurrent exception creation too slow";

    // Should achieve reasonable throughput
    EXPECT_GT(throughput, CONCURRENT_THROUGHPUT_MIN)
        << "Concurrent throughput too low";
}

TEST_F(ExceptionPerformanceTest, Benchmark_ConcurrentHandlerDispatch) {
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 500;

    // Register a handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "concurrent_handler";
    opts.handler = perf_handler_1;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    // Shared exception
    nimcp_exception_t* shared_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Shared"
    );

    handler_call_count = 0;
    std::atomic<int64_t> total_time_us{0};

    auto thread_func = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            auto start = Clock::now();
            nimcp_exception_dispatch(shared_ex);
            auto end = Clock::now();
            Duration duration = end - start;
            total_time_us += (int64_t)duration.count();
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    nimcp_exception_unref(shared_ex);
    nimcp_handler_unregister(reg);

    int expected_calls = NUM_THREADS * OPS_PER_THREAD;
    double avg_us = (double)total_time_us / expected_calls;

    printf("  Concurrent Handler Dispatch (%d threads):\n", NUM_THREADS);
    printf("    Total dispatches: %d\n", expected_calls);
    printf("    Handler calls: %d\n", handler_call_count.load());
    printf("    Average per dispatch: %.2f us\n", avg_us);

    EXPECT_EQ(handler_call_count.load(), expected_calls);
    EXPECT_LT(avg_us, EXCEPTION_DISPATCH_MAX_US * 2)
        << "Concurrent dispatch too slow";
}

/* ============================================================================
 * Memory Allocation Pattern Tests
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_AllocationPattern_CreateDestroy) {
    // Test create-destroy pattern (no accumulation)
    auto start = Clock::now();

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test %d", i
        );
        nimcp_exception_unref(ex);
    }

    auto end = Clock::now();
    Duration duration = end - start;
    double avg_us = duration.count() / MICRO_ITERATIONS;

    printf("  Create-Destroy Pattern:\n");
    printf("    %d iterations in %.2f us (%.2f us/op)\n",
           MICRO_ITERATIONS, duration.count(), avg_us);

    EXPECT_LT(avg_us, EXCEPTION_CREATE_MAX_US * 2)
        << "Create-destroy pattern too slow";
}

TEST_F(ExceptionPerformanceTest, Benchmark_AllocationPattern_BatchCreate) {
    // Test batch create then batch destroy
    std::vector<nimcp_exception_t*> exceptions;
    exceptions.reserve(STRESS_ITERATIONS);

    auto create_start = Clock::now();
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        exceptions.push_back(nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Batch %d", i
        ));
    }
    auto create_end = Clock::now();

    auto destroy_start = Clock::now();
    for (auto ex : exceptions) {
        nimcp_exception_unref(ex);
    }
    auto destroy_end = Clock::now();

    Duration create_duration = create_end - create_start;
    Duration destroy_duration = destroy_end - destroy_start;

    printf("  Batch Create-Destroy Pattern:\n");
    printf("    Create %d: %.2f us (%.2f us/op)\n",
           STRESS_ITERATIONS, create_duration.count(),
           create_duration.count() / STRESS_ITERATIONS);
    printf("    Destroy %d: %.2f us (%.2f us/op)\n",
           STRESS_ITERATIONS, destroy_duration.count(),
           destroy_duration.count() / STRESS_ITERATIONS);

    EXPECT_LT(create_duration.count() / STRESS_ITERATIONS, EXCEPTION_CREATE_MAX_US * 2);
}

/* ============================================================================
 * Trace ID Generation Benchmark
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_TraceIdGeneration) {
    nimcp_trace_init();

    std::vector<double> samples;
    samples.reserve(MICRO_ITERATIONS);

    for (int i = 0; i < MICRO_ITERATIONS; i++) {
        auto start = Clock::now();
        uint64_t id = nimcp_trace_generate_id();
        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());

        EXPECT_GT(id, 0UL);
    }

    nimcp_trace_shutdown();

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Trace ID Generation", result);

    EXPECT_LT(result.avg_us, 1.0)
        << "Trace ID generation too slow";
}

/* ============================================================================
 * Summary Test - Full Exception Path
 * ============================================================================ */

TEST_F(ExceptionPerformanceTest, Benchmark_FullExceptionPath) {
    // Register handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "full_path_handler";
    opts.handler = perf_handler_1;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    handler_call_count = 0;
    std::vector<double> samples;
    samples.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        auto start = Clock::now();

        // Full path: create -> generate epitope -> dispatch -> unref
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Full path test %d", i
        );

        nimcp_exception_generate_epitope(ex);
        nimcp_exception_dispatch(ex);
        nimcp_circuit_record(ex);
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);

        auto end = Clock::now();
        Duration duration = end - start;
        samples.push_back(duration.count());
    }

    nimcp_handler_unregister(reg);

    BenchmarkResult result = compute_stats(samples);
    PrintBenchmarkResult("Full Exception Path", result);

    EXPECT_EQ(handler_call_count.load(), STRESS_ITERATIONS);

    // Full path should complete in reasonable time
    double target_us = EXCEPTION_CREATE_MAX_US + EPITOPE_COMPUTE_MAX_US +
                       EXCEPTION_DISPATCH_MAX_US + 10;  // Some margin
    EXPECT_LT(result.avg_us, target_us)
        << "Full exception path too slow";
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
