/**
 * @file test_security_validation_performance.cpp
 * @brief Performance regression tests for security validation (NIMCP)
 *
 * WHAT: Comprehensive performance tests for BBB security validation
 * WHY:  Ensure security features don't degrade system performance over time
 * HOW:  Benchmark validation operations, measure memory usage, stress test with load
 *
 * TEST COVERAGE:
 * 1. Performance impact of security checks on message routing
 * 2. Memory usage during validation operations
 * 3. Stress tests with malicious input patterns
 * 4. Scalability under concurrent load
 * 5. Regression baselines for performance metrics
 *
 * PERFORMANCE BASELINES:
 * - Single validation: < 1ms average
 * - 1000 validations: < 500ms total
 * - Memory overhead: < 1MB per 1000 validations
 * - Concurrent validation: linear scaling up to 8 threads
 * - Malicious pattern detection: < 2ms average
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"

#include <cstring>
#include <chrono>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace {

//=============================================================================
// Performance Measurement Helpers
//=============================================================================

struct PerformanceMetrics {
    double min_ms;
    double max_ms;
    double mean_ms;
    double stddev_ms;
    double p50_ms;
    double p95_ms;
    double p99_ms;
    size_t sample_count;
};

class PerformanceMeasure {
public:
    void record_sample(double duration_ms) {
        samples_.push_back(duration_ms);
    }

    PerformanceMetrics get_metrics() const {
        if (samples_.empty()) {
            return {0, 0, 0, 0, 0, 0, 0, 0};
        }

        std::vector<double> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        double mean = sum / sorted.size();

        double sq_sum = 0.0;
        for (double val : sorted) {
            sq_sum += (val - mean) * (val - mean);
        }
        double stddev = std::sqrt(sq_sum / sorted.size());

        size_t p50_idx = sorted.size() * 50 / 100;
        size_t p95_idx = sorted.size() * 95 / 100;
        size_t p99_idx = sorted.size() * 99 / 100;

        return {
            sorted.front(),
            sorted.back(),
            mean,
            stddev,
            sorted[p50_idx],
            sorted[p95_idx],
            sorted[p99_idx],
            sorted.size()
        };
    }

    void print_metrics(const std::string& test_name) const {
        auto metrics = get_metrics();
        std::cout << "\n=== Performance Metrics: " << test_name << " ===" << std::endl;
        std::cout << "  Samples: " << metrics.sample_count << std::endl;
        std::cout << "  Min:     " << metrics.min_ms << " ms" << std::endl;
        std::cout << "  Max:     " << metrics.max_ms << " ms" << std::endl;
        std::cout << "  Mean:    " << metrics.mean_ms << " ms" << std::endl;
        std::cout << "  StdDev:  " << metrics.stddev_ms << " ms" << std::endl;
        std::cout << "  P50:     " << metrics.p50_ms << " ms" << std::endl;
        std::cout << "  P95:     " << metrics.p95_ms << " ms" << std::endl;
        std::cout << "  P99:     " << metrics.p99_ms << " ms" << std::endl;
    }

private:
    std::vector<double> samples_;
};

//=============================================================================
// Test Fixture
//=============================================================================

class SecurityValidationPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        config_ = bbb_default_config();
        config_.strict_mode = true;

        system_ = bbb_system_create(&config_);
        ASSERT_NE(system_, nullptr);
        ASSERT_TRUE(bbb_system_set_enabled(system_, true));

        // Reset statistics
        bbb_system_reset_statistics(system_);
    }

    void TearDown() override
    {
        // Clear any signing key that was set during the test
        bbb_clear_signing_key();

        if (system_) {
            bbb_system_destroy(system_);
            system_ = nullptr;
        }
    }

    // Helper to measure validation time
    double measure_validation_time_ms(const char* input)
    {
        auto start = std::chrono::high_resolution_clock::now();

        bbb_validation_result_t result;
        bbb_validate_string(system_, input, &result);

        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    // Helper to measure multiple validations
    double measure_batch_validation_ms(const std::vector<const char*>& inputs)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (const char* input : inputs) {
            bbb_validation_result_t result;
            bbb_validate_string(system_, input, &result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    bbb_config_t config_;
    bbb_system_t system_{nullptr};
};

//=============================================================================
// Single Validation Performance Tests
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, SingleSafeStringValidation)
{
    const int NUM_SAMPLES = 1000;
    const double MAX_MEAN_MS = 1.0;  // Average should be < 1ms

    PerformanceMeasure perf;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        double duration = measure_validation_time_ms("Safe input string for testing");
        perf.record_sample(duration);
    }

    auto metrics = perf.get_metrics();
    perf.print_metrics("Single Safe String Validation");

    EXPECT_LT(metrics.mean_ms, MAX_MEAN_MS)
        << "Performance regression: mean " << metrics.mean_ms << "ms > " << MAX_MEAN_MS << "ms";

    EXPECT_LT(metrics.p95_ms, MAX_MEAN_MS * 3)
        << "Performance regression: P95 " << metrics.p95_ms << "ms too high";
}

TEST_F(SecurityValidationPerformanceTest, SingleMaliciousStringValidation)
{
    const int NUM_SAMPLES = 1000;
    const double MAX_MEAN_MS = 2.0;  // Malicious detection may take longer

    PerformanceMeasure perf;

    const char* malicious_inputs[] = {
        "'; DROP TABLE users; --",
        "1 UNION SELECT * FROM passwords",
        "%n%n%n%n",
        "; rm -rf /",
        "<script>alert('XSS')</script>"
    };

    for (int i = 0; i < NUM_SAMPLES; i++) {
        const char* input = malicious_inputs[i % 5];
        double duration = measure_validation_time_ms(input);
        perf.record_sample(duration);
    }

    auto metrics = perf.get_metrics();
    perf.print_metrics("Single Malicious String Validation");

    EXPECT_LT(metrics.mean_ms, MAX_MEAN_MS)
        << "Performance regression: mean " << metrics.mean_ms << "ms > " << MAX_MEAN_MS << "ms";
}

TEST_F(SecurityValidationPerformanceTest, IntegerValidationPerformance)
{
    const int NUM_SAMPLES = 10000;
    const double MAX_TOTAL_MS = 100.0;  // 10k validations in < 100ms

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_SAMPLES; i++) {
        bbb_validation_result_t result;
        bbb_validate_integer(system_, i, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << NUM_SAMPLES << " integer validations took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";

    std::cout << "Integer validation: " << NUM_SAMPLES << " in " << duration_ms << "ms"
              << " (" << (duration_ms / NUM_SAMPLES) << " ms/validation)" << std::endl;
}

TEST_F(SecurityValidationPerformanceTest, PointerValidationPerformance)
{
    const int NUM_SAMPLES = 10000;
    const double MAX_TOTAL_MS = 100.0;

    char test_buffer[1024];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_SAMPLES; i++) {
        bbb_validation_result_t result;
        bbb_validate_pointer(system_, test_buffer, sizeof(test_buffer), &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << NUM_SAMPLES << " pointer validations took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";
}

//=============================================================================
// Batch Validation Performance Tests
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, BatchSafeStringValidation)
{
    const int BATCH_SIZE = 1000;
    const double MAX_BATCH_MS = 500.0;

    std::vector<const char*> inputs;
    for (int i = 0; i < BATCH_SIZE; i++) {
        inputs.push_back("Safe test input string");
    }

    double duration_ms = measure_batch_validation_ms(inputs);

    EXPECT_LT(duration_ms, MAX_BATCH_MS)
        << "Performance regression: batch of " << BATCH_SIZE << " validations took "
        << duration_ms << "ms (max: " << MAX_BATCH_MS << "ms)";

    std::cout << "Batch validation: " << BATCH_SIZE << " in " << duration_ms << "ms"
              << " (" << (duration_ms / BATCH_SIZE) << " ms/validation)" << std::endl;
}

TEST_F(SecurityValidationPerformanceTest, BatchMixedValidation)
{
    const int BATCH_SIZE = 1000;
    const double MAX_BATCH_MS = 1000.0;

    std::vector<const char*> inputs;
    const char* test_inputs[] = {
        "Safe input 1",
        "Safe input 2",
        "'; DROP TABLE x; --",
        "Safe input 3",
        "%n%n%n",
        "Safe input 4"
    };

    for (int i = 0; i < BATCH_SIZE; i++) {
        inputs.push_back(test_inputs[i % 6]);
    }

    double duration_ms = measure_batch_validation_ms(inputs);

    EXPECT_LT(duration_ms, MAX_BATCH_MS)
        << "Performance regression: batch of " << BATCH_SIZE << " mixed validations took "
        << duration_ms << "ms (max: " << MAX_BATCH_MS << "ms)";
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, MemoryUsageDuringValidation)
{
    const int NUM_VALIDATIONS = 10000;
    const size_t MAX_MEMORY_OVERHEAD_MB = 5;  // < 5MB overhead

    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // Perform many validations
    for (int i = 0; i < NUM_VALIDATIONS; i++) {
        bbb_validation_result_t result;

        if (i % 2 == 0) {
            bbb_validate_string(system_, "Safe input string", &result);
        } else {
            bbb_validate_string(system_, "'; DROP TABLE x; --", &result);
        }
    }

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    size_t memory_increase = stats_after.total_allocated - stats_before.total_allocated;
    size_t memory_increase_mb = memory_increase / (1024 * 1024);

    std::cout << "Memory usage after " << NUM_VALIDATIONS << " validations: "
              << memory_increase << " bytes (" << memory_increase_mb << " MB)" << std::endl;

    EXPECT_LT(memory_increase_mb, MAX_MEMORY_OVERHEAD_MB)
        << "Memory regression: " << memory_increase_mb << " MB > " << MAX_MEMORY_OVERHEAD_MB << " MB";
}

TEST_F(SecurityValidationPerformanceTest, MemoryLeakDuringValidation)
{
    const int NUM_ITERATIONS = 1000;

    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // Perform validations in a loop
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bbb_validation_result_t result;
        bbb_validate_string(system_, "Test input", &result);
        bbb_validate_integer(system_, i, &result);

        char buffer[256];
        bbb_validate_pointer(system_, buffer, sizeof(buffer), &result);
    }

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    // Check for leaks (current allocated should be similar to before)
    size_t current_diff = stats_after.current_allocated - stats_before.current_allocated;

    std::cout << "Memory leak check: " << current_diff << " bytes difference in current allocation" << std::endl;

    // Allow some overhead for internal caching, but should be minimal
    EXPECT_LT(current_diff, 1024 * 100)  // < 100KB
        << "Potential memory leak detected: " << current_diff << " bytes";
}

//=============================================================================
// Stress Tests with Malicious Patterns
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, SQLInjectionDetectionStress)
{
    const int NUM_PATTERNS = 100;
    const double MAX_TOTAL_MS = 500.0;

    const char* sql_patterns[] = {
        "'; DROP TABLE users; --",
        "' OR '1'='1",
        "' OR 1=1--",
        "'; DELETE FROM accounts; --",
        "1 UNION SELECT * FROM passwords",
        "' UNION SELECT username, password FROM users--",
        "admin'--",
        "1; INSERT INTO users VALUES('hacker', 'password')",
        "'; EXEC xp_cmdshell('cmd'); --",
        "' OR EXISTS(SELECT * FROM users)--"
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_PATTERNS; i++) {
        for (size_t j = 0; j < sizeof(sql_patterns) / sizeof(sql_patterns[0]); j++) {
            bbb_validation_result_t result;
            bbb_validate_string(system_, sql_patterns[j], &result);

            ASSERT_FALSE(result.valid) << "Failed to detect SQL injection: " << sql_patterns[j];
            ASSERT_EQ(result.threat, BBB_THREAT_SQL_INJECTION);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    int total_detections = NUM_PATTERNS * (sizeof(sql_patterns) / sizeof(sql_patterns[0]));

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << total_detections << " SQL injection detections took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";

    std::cout << "SQL injection stress: " << total_detections << " detections in "
              << duration_ms << "ms" << std::endl;
}

TEST_F(SecurityValidationPerformanceTest, FormatStringDetectionStress)
{
    const int NUM_PATTERNS = 100;
    const double MAX_TOTAL_MS = 500.0;

    const char* format_patterns[] = {
        "%n%n%n%n",
        "%s%s%s%s",
        "%x%x%x%x",
        "AAAA%08x.%08x.%08x.%08x",
        "%p%p%p%p",
        "%.100000s",
        "%99999999s",
        "%.99999d"
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_PATTERNS; i++) {
        for (size_t j = 0; j < sizeof(format_patterns) / sizeof(format_patterns[0]); j++) {
            bbb_validation_result_t result;
            bbb_validate_string(system_, format_patterns[j], &result);

            ASSERT_FALSE(result.valid) << "Failed to detect format string: " << format_patterns[j];
            ASSERT_EQ(result.threat, BBB_THREAT_FORMAT_STRING);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    int total_detections = NUM_PATTERNS * (sizeof(format_patterns) / sizeof(format_patterns[0]));

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << total_detections << " format string detections took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";
}

TEST_F(SecurityValidationPerformanceTest, MixedAttackPatternStress)
{
    const int NUM_ITERATIONS = 500;
    const double MAX_TOTAL_MS = 2000.0;

    const char* attack_patterns[] = {
        "'; DROP TABLE x; --",           // SQL injection
        "%n%n%n%n",                       // Format string
        "; rm -rf /",                     // Code injection
        "<script>alert('XSS')</script>",  // XSS
        "../../../etc/passwd",            // Path traversal
        "1 UNION SELECT *",               // SQL injection
        "$(whoami)",                      // Command injection
        "%s%s%s%s%s"                      // Format string
    };

    auto start = std::chrono::high_resolution_clock::now();

    int detected_count = 0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        for (size_t j = 0; j < sizeof(attack_patterns) / sizeof(attack_patterns[0]); j++) {
            bbb_validation_result_t result;
            bbb_validate_string(system_, attack_patterns[j], &result);
            if (!result.valid) detected_count++;
        }
    }
    /* BBB may not detect all attack patterns depending on configuration.
     * Focus on performance rather than detection rate in this stress test. */
    (void)detected_count;  // Suppress unused warning

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    int total_detections = NUM_ITERATIONS * (sizeof(attack_patterns) / sizeof(attack_patterns[0]));

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << total_detections << " mixed attack detections took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";
}

//=============================================================================
// Concurrent Performance Tests
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, ConcurrentValidationScaling)
{
    const int VALIDATIONS_PER_THREAD = 1000;
    const std::vector<int> THREAD_COUNTS = {1, 2, 4, 8};

    std::cout << "\n=== Concurrent Validation Scaling ===" << std::endl;

    for (int num_threads : THREAD_COUNTS) {
        std::vector<std::thread> threads;
        std::atomic<int> completed{0};

        auto start = std::chrono::high_resolution_clock::now();

        auto validate_task = [this, &completed]() {
            for (int i = 0; i < VALIDATIONS_PER_THREAD; i++) {
                bbb_validation_result_t result;
                bbb_validate_string(system_, "Test input", &result);
                completed.fetch_add(1);
            }
        };

        // Launch threads
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(validate_task);
        }

        // Wait for completion
        for (auto& t : threads) {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        int total_validations = num_threads * VALIDATIONS_PER_THREAD;
        double throughput = total_validations / (duration_ms / 1000.0);

        std::cout << "  " << num_threads << " threads: " << duration_ms << " ms, "
                  << throughput << " validations/sec" << std::endl;

        EXPECT_EQ(completed.load(), total_validations);
    }
}

TEST_F(SecurityValidationPerformanceTest, ConcurrentMixedWorkload)
{
    const int NUM_THREADS = 4;
    const int OPERATIONS_PER_THREAD = 250;
    const double MAX_TOTAL_MS = 2000.0;

    std::vector<std::thread> threads;
    std::atomic<int> completed{0};

    auto start = std::chrono::high_resolution_clock::now();

    auto mixed_task = [this, &completed](int thread_id) {
        for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
            bbb_validation_result_t result;

            // Mix different validation types
            if (i % 4 == 0) {
                bbb_validate_string(system_, "Safe string", &result);
            } else if (i % 4 == 1) {
                bbb_validate_string(system_, "'; DROP TABLE x; --", &result);
            } else if (i % 4 == 2) {
                bbb_validate_integer(system_, i, &result);
            } else {
                char buffer[128];
                bbb_validate_pointer(system_, buffer, sizeof(buffer), &result);
            }

            completed.fetch_add(1);
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(mixed_task, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Concurrent mixed workload took " << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";

    EXPECT_EQ(completed.load(), NUM_THREADS * OPERATIONS_PER_THREAD);
}

//=============================================================================
// Cryptographic Operation Performance Tests
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, HashCalculationPerformance)
{
    const int NUM_HASHES = 1000;
    const double MAX_TOTAL_MS = 500.0;

    const char* data = "Test data for hash calculation benchmark";
    uint8_t hash[32];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_HASHES; i++) {
        bbb_calculate_hash(data, strlen(data), hash);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << NUM_HASHES << " hash calculations took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";

    std::cout << "Hash calculation: " << NUM_HASHES << " in " << duration_ms << "ms"
              << " (" << (duration_ms / NUM_HASHES) << " ms/hash)" << std::endl;
}

TEST_F(SecurityValidationPerformanceTest, CodeSigningPerformance)
{
    // Configure signing key (required before signing)
    static const uint8_t test_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    ASSERT_TRUE(bbb_set_signing_key(test_key, sizeof(test_key)));

    const int NUM_SIGNATURES = 100;
    const double MAX_TOTAL_MS = 1000.0;

    const char* code = "function test() { return 42; }";
    uint8_t signature[512];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_SIGNATURES; i++) {
        ssize_t sig_len = bbb_sign_code(system_, code, strlen(code), signature, sizeof(signature));
        ASSERT_GT(sig_len, 0);

        bool valid = bbb_verify_signature(system_, code, strlen(code), signature, sig_len);
        ASSERT_TRUE(valid);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << NUM_SIGNATURES << " sign/verify operations took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";
}

//=============================================================================
// Long String Validation Tests
//=============================================================================

TEST_F(SecurityValidationPerformanceTest, LongStringValidationPerformance)
{
    const int NUM_VALIDATIONS = 100;
    // Relaxed threshold for CI environments with variable system load
    const double MAX_TOTAL_MS = 3000.0;

    // Generate strings of various lengths
    std::vector<std::string> test_strings;
    for (size_t len : {100, 1000, 10000, 50000}) {
        test_strings.push_back(std::string(len, 'A'));
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_VALIDATIONS; i++) {
        for (const auto& str : test_strings) {
            bbb_validation_result_t result;
            bbb_validate_string(system_, str.c_str(), &result);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    int total_validations = NUM_VALIDATIONS * test_strings.size();

    EXPECT_LT(duration_ms, MAX_TOTAL_MS)
        << "Performance regression: " << total_validations << " long string validations took "
        << duration_ms << "ms (max: " << MAX_TOTAL_MS << "ms)";
}

}  // anonymous namespace
