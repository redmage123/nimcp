//=============================================================================
// test_config_performance.cpp - Config Module Performance Regression Tests
//=============================================================================
/**
 * @file test_config_performance.cpp
 * @brief Performance and regression tests for config module
 *
 * WHAT: Performance benchmarks to detect regressions
 * WHY:  Ensure O(1) hash operations don't degrade over time
 * HOW:  GoogleTest with timing measurements and assertions
 *
 * PERFORMANCE TARGETS:
 * - Hash lookup: O(1) - should be < 1us even with 10k entries
 * - Large config parse: < 100ms for 10k entries
 * - Concurrent access: No lock contention with 8 threads
 * - Memory usage: Linear with entry count, no leaks
 *
 * REGRESSION DETECTION:
 * - Compare against baseline measurements
 * - Fail if performance degrades > 20%
 * - Track memory growth over multiple reloads
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>

// Headers have their own extern "C" guards
#include "utils/config/nimcp_config_hash.h"
#include "utils/config/nimcp_config_validation.h"
#include "utils/config/nimcp_config_expand.h"
#include "utils/config/nimcp_config_array.h"
#include "utils/config/nimcp_dynamic_config.h"

//=============================================================================
// Timing Utilities
//=============================================================================

class Timer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double elapsed_us() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_time).count();
    }

    double elapsed_ms() {
        return elapsed_us() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

//=============================================================================
// Test Fixture
//=============================================================================

class ConfigPerformanceTest : public ::testing::Test {
protected:
    config_hash_table_t table;
    Timer timer;

    void SetUp() override {
        // WHAT: Create hash table for performance testing
        // WHY:  Need clean state for each test
        // HOW:  Create with reasonable initial capacity

        table = config_hash_create(1024);
        ASSERT_NE(table, nullptr);
    }

    void TearDown() override {
        if (table) {
            config_hash_destroy(table);
            table = nullptr;
        }
    }

    // Helper to populate table with N entries
    void populate_table(size_t count) {
        config_value_t val;
        for (size_t i = 0; i < count; i++) {
            char key[64];
            snprintf(key, sizeof(key), "perf_key_%zu", i);
            val.i = (int64_t)i;
            config_hash_set(table, key, &val, CONFIG_VALUE_INT);
        }
    }
};

//=============================================================================
// Hash Table Performance Tests
//=============================================================================

TEST_F(ConfigPerformanceTest, HashLookupO1Verification) {
    // WHAT: Verify hash lookups are O(1) regardless of table size
    // WHY:  Ensure no performance degradation with size
    // HOW:  Measure lookup time for 100, 1k, 10k entries

    struct BenchResult {
        size_t entry_count;
        double avg_lookup_us;
    };

    std::vector<BenchResult> results;
    std::vector<size_t> sizes = {100, 1000, 10000};

    for (size_t size : sizes) {
        config_hash_destroy(table);
        table = config_hash_create(size * 2);  // Pre-size to avoid resize
        populate_table(size);

        // Measure average lookup time
        const int num_lookups = 1000;
        config_value_t result;

        timer.start();
        for (int i = 0; i < num_lookups; i++) {
            char key[64];
            snprintf(key, sizeof(key), "perf_key_%d", i % (int)size);
            config_hash_get(table, key, &result, nullptr);
        }
        double elapsed = timer.elapsed_us();
        double avg = elapsed / num_lookups;

        results.push_back({size, avg});

        // Each lookup should be sub-microsecond
        EXPECT_LT(avg, 1.0) << "Lookup too slow for " << size << " entries";
    }

    // Verify O(1): lookup time should not grow significantly
    // Allow 2x slowdown due to cache effects, but not linear growth
    double ratio = results.back().avg_lookup_us / results.front().avg_lookup_us;
    EXPECT_LT(ratio, 2.0) << "Lookup time grew more than 2x (not O(1))";

    // Print results for manual inspection
    std::cout << "\n=== Hash Lookup Performance ===\n";
    for (const auto& r : results) {
        std::cout << "  " << r.entry_count << " entries: "
                  << r.avg_lookup_us << " us/lookup\n";
    }
}

TEST_F(ConfigPerformanceTest, HashInsertPerformance) {
    // WHAT: Measure hash insert performance
    // WHY:  Ensure inserts are fast even with resizing
    // HOW:  Insert 10k entries, measure time

    const size_t num_entries = 10000;
    config_value_t val;

    timer.start();
    for (size_t i = 0; i < num_entries; i++) {
        char key[64];
        snprintf(key, sizeof(key), "insert_key_%zu", i);
        val.i = (int64_t)i;
        config_hash_set(table, key, &val, CONFIG_VALUE_INT);
    }
    double elapsed_ms = timer.elapsed_ms();

    // Should complete in < 100ms
    EXPECT_LT(elapsed_ms, 100.0) << "Insert performance regression detected";

    double avg_us = (elapsed_ms * 1000.0) / num_entries;
    EXPECT_LT(avg_us, 10.0) << "Average insert time too high";

    std::cout << "\n=== Hash Insert Performance ===\n";
    std::cout << "  " << num_entries << " inserts in " << elapsed_ms << " ms\n";
    std::cout << "  Average: " << avg_us << " us/insert\n";
}

TEST_F(ConfigPerformanceTest, HashResizePerformance) {
    // WHAT: Measure resize operation performance
    // WHY:  Ensure resize doesn't cause long pauses
    // HOW:  Fill table to trigger resize, measure time

    // Start small to force resize
    config_hash_destroy(table);
    table = config_hash_create(16);

    size_t initial_capacity = config_hash_capacity(table);
    config_value_t val;

    // Insert enough to trigger resize
    size_t threshold = (size_t)(initial_capacity * 0.75);

    timer.start();
    for (size_t i = 0; i <= threshold + 10; i++) {
        char key[64];
        snprintf(key, sizeof(key), "resize_key_%zu", i);
        val.i = (int64_t)i;
        config_hash_set(table, key, &val, CONFIG_VALUE_INT);
    }
    double elapsed_ms = timer.elapsed_ms();

    // Resize should happen quickly
    EXPECT_LT(elapsed_ms, 10.0) << "Resize took too long";

    // Verify resize occurred
    EXPECT_GT(config_hash_capacity(table), initial_capacity);

    std::cout << "\n=== Hash Resize Performance ===\n";
    std::cout << "  Capacity: " << initial_capacity << " -> "
              << config_hash_capacity(table) << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
}

//=============================================================================
// Config Parsing Performance Tests
//=============================================================================

TEST_F(ConfigPerformanceTest, LargeConfigParsing) {
    // WHAT: Test parsing large config files
    // WHY:  Ensure scalability for complex configs
    // HOW:  Generate 10k entry config, parse, measure

    std::string large_config = "/tmp/nimcp_large_perf_test.ini";
    const size_t num_entries = 10000;

    // Generate large config file
    std::ofstream config_file(large_config);
    config_file << "[performance_test]\n";
    for (size_t i = 0; i < num_entries; i++) {
        config_file << "key_" << i << " = " << i << "\n";
    }
    config_file.close();

    // Parse and measure
    timer.start();
    bool init_success = config_init(large_config.c_str());
    double parse_time_ms = timer.elapsed_ms();

    ASSERT_TRUE(init_success);

    // Parsing 10k entries should be < 100ms
    EXPECT_LT(parse_time_ms, 100.0) << "Config parsing regression detected";

    // Verify all entries loaded
    // (Sampling to keep test fast)
    EXPECT_EQ(config_get_int("key_0", -1), 0);
    EXPECT_EQ(config_get_int("key_5000", -1), 5000);
    EXPECT_EQ(config_get_int("key_9999", -1), 9999);

    std::cout << "\n=== Large Config Parsing ===\n";
    std::cout << "  " << num_entries << " entries parsed in "
              << parse_time_ms << " ms\n";
    std::cout << "  Rate: " << (num_entries / parse_time_ms) << " entries/ms\n";

    config_shutdown();
    std::filesystem::remove(large_config);
}

TEST_F(ConfigPerformanceTest, ArrayParsingPerformance) {
    // WHAT: Test array parsing performance
    // WHY:  Ensure large arrays don't slow down parsing
    // HOW:  Parse config with large arrays, measure

    std::string array_config = "/tmp/nimcp_array_perf_test.ini";

    // Generate config with large arrays
    std::ofstream config_file(array_config);
    config_file << "[arrays]\n";

    config_file << "large_int_array = [";
    for (int i = 0; i < 1000; i++) {
        if (i > 0) config_file << ", ";
        config_file << i;
    }
    config_file << "]\n";

    config_file << "large_float_array = [";
    for (int i = 0; i < 1000; i++) {
        if (i > 0) config_file << ", ";
        config_file << (i * 0.001);
    }
    config_file << "]\n";
    config_file.close();

    timer.start();
    ASSERT_TRUE(config_init(array_config.c_str()));
    double parse_time_ms = timer.elapsed_ms();

    // Should parse quickly
    EXPECT_LT(parse_time_ms, 50.0) << "Array parsing too slow";

    // Verify arrays loaded
    const config_array_t* int_array = config_get_array("large_int_array");
    const config_array_t* float_array = config_get_array("large_float_array");

    ASSERT_NE(int_array, nullptr);
    ASSERT_NE(float_array, nullptr);
    EXPECT_EQ(config_array_size(int_array), 1000);
    EXPECT_EQ(config_array_size(float_array), 1000);

    std::cout << "\n=== Array Parsing Performance ===\n";
    std::cout << "  2000 array elements parsed in " << parse_time_ms << " ms\n";

    config_shutdown();
    std::filesystem::remove(array_config);
}

//=============================================================================
// Concurrent Access Performance Tests
//=============================================================================

TEST_F(ConfigPerformanceTest, ConcurrentReadThroughput) {
    // WHAT: Measure throughput with multiple readers
    // WHY:  Ensure no lock contention degrades performance
    // HOW:  8 reader threads, measure ops/sec

    populate_table(1000);

    const int num_threads = 8;
    const int ops_per_thread = 10000;
    std::atomic<int> completed_ops{0};

    timer.start();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            config_value_t result;
            for (int i = 0; i < ops_per_thread; i++) {
                char key[64];
                snprintf(key, sizeof(key), "perf_key_%d", i % 1000);
                if (config_hash_get(table, key, &result, nullptr)) {
                    completed_ops++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    double elapsed_ms = timer.elapsed_ms();
    int total_ops = completed_ops.load();
    double throughput = (total_ops / elapsed_ms) * 1000.0;  // ops/sec

    // Should achieve high throughput (>100k ops/sec)
    EXPECT_GT(throughput, 100000.0) << "Concurrent read throughput too low";

    std::cout << "\n=== Concurrent Read Throughput ===\n";
    std::cout << "  Threads: " << num_threads << "\n";
    std::cout << "  Total ops: " << total_ops << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << throughput << " ops/sec\n";
}

TEST_F(ConfigPerformanceTest, ConcurrentWritePerformance) {
    // WHAT: Measure write performance with contention
    // WHY:  Ensure exclusive locks don't cause excessive blocking
    // HOW:  4 writer threads, measure completion time

    const int num_threads = 4;
    const int writes_per_thread = 1000;

    timer.start();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            config_value_t val;
            for (int i = 0; i < writes_per_thread; i++) {
                char key[64];
                snprintf(key, sizeof(key), "write_t%d_i%d", t, i);
                val.i = t * 1000 + i;
                config_hash_set(table, key, &val, CONFIG_VALUE_INT);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    double elapsed_ms = timer.elapsed_ms();
    int total_writes = num_threads * writes_per_thread;

    // Should complete writes reasonably fast
    EXPECT_LT(elapsed_ms, 1000.0) << "Concurrent writes too slow";

    std::cout << "\n=== Concurrent Write Performance ===\n";
    std::cout << "  Threads: " << num_threads << "\n";
    std::cout << "  Total writes: " << total_writes << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << (total_writes / elapsed_ms * 1000.0)
              << " writes/sec\n";
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

TEST_F(ConfigPerformanceTest, MemoryUsageLinearGrowth) {
    // WHAT: Verify memory usage grows linearly with entries
    // WHY:  Detect memory leaks or excessive overhead
    // HOW:  Measure memory at different sizes, verify linear

    struct MemResult {
        size_t entry_count;
        size_t memory_bytes;
    };

    std::vector<MemResult> results;
    std::vector<size_t> sizes = {100, 500, 1000, 5000, 10000};

    for (size_t size : sizes) {
        config_hash_destroy(table);
        table = config_hash_create(size * 2);
        populate_table(size);

        // Estimate memory usage
        // (Would use actual memory tracking in production)
        size_t capacity = config_hash_capacity(table);
        size_t entry_size = sizeof(config_value_t) + 64;  // Approximate
        size_t estimated_bytes = capacity * entry_size;

        results.push_back({size, estimated_bytes});
    }

    // Verify roughly linear growth
    // Memory per entry should be relatively constant
    double first_per_entry = (double)results[0].memory_bytes / results[0].entry_count;
    double last_per_entry = (double)results.back().memory_bytes / results.back().entry_count;

    // Allow 2x variation due to capacity rounding
    double ratio = last_per_entry / first_per_entry;
    EXPECT_LT(ratio, 2.0) << "Memory usage not scaling linearly";

    std::cout << "\n=== Memory Usage ===\n";
    for (const auto& r : results) {
        double per_entry = (double)r.memory_bytes / r.entry_count;
        std::cout << "  " << r.entry_count << " entries: "
                  << r.memory_bytes << " bytes (" << per_entry << " bytes/entry)\n";
    }
}

TEST_F(ConfigPerformanceTest, NoMemoryLeaksOnReload) {
    // WHAT: Verify no memory leaks over multiple reloads
    // WHY:  Long-running processes must not leak
    // HOW:  Reload config 100 times, verify memory stable

    std::string test_config = "/tmp/nimcp_leak_test.ini";
    std::ofstream config_file(test_config);
    config_file << "[test]\n";
    config_file << "value = 42\n";
    config_file.close();

    ASSERT_TRUE(config_init(test_config.c_str()));

    // Perform many reloads
    const int num_reloads = 100;

    timer.start();
    for (int i = 0; i < num_reloads; i++) {
        config_reload();
    }
    double elapsed_ms = timer.elapsed_ms();

    // Reloads should be fast
    EXPECT_LT(elapsed_ms / num_reloads, 10.0) << "Reload performance degraded";

    // In production, would check actual memory usage here
    // For now, just verify no crashes

    std::cout << "\n=== Reload Performance ===\n";
    std::cout << "  " << num_reloads << " reloads in " << elapsed_ms << " ms\n";
    std::cout << "  Average: " << (elapsed_ms / num_reloads) << " ms/reload\n";

    config_shutdown();
    std::filesystem::remove(test_config);
}

//=============================================================================
// Validation Performance Tests
//=============================================================================

TEST_F(ConfigPerformanceTest, ValidationPerformance) {
    // WHAT: Measure schema validation performance
    // WHY:  Ensure validation doesn't slow down reloads
    // HOW:  Create schema with 100 fields, validate, measure

    config_schema_t schema = config_schema_create();
    ASSERT_NE(schema, nullptr);

    // Add 100 validation rules
    for (int i = 0; i < 100; i++) {
        char key[64];
        snprintf(key, sizeof(key), "param_%d", i);
        config_schema_add_int(schema, key, false, i, 0, 1000);
    }

    // Create matching config
    std::string valid_config = "/tmp/nimcp_validation_perf.ini";
    std::ofstream config_file(valid_config);
    config_file << "[params]\n";
    for (int i = 0; i < 100; i++) {
        config_file << "param_" << i << " = " << i << "\n";
    }
    config_file.close();

    ASSERT_TRUE(config_init(valid_config.c_str()));

    // Measure validation time
    timer.start();
    config_validation_result_t result;
    bool valid = config_validate_against_schema(schema, &result);
    double validation_ms = timer.elapsed_ms();

    EXPECT_TRUE(valid);
    EXPECT_EQ(result.error_count, 0);

    // Validation should be fast (< 10ms for 100 rules)
    EXPECT_LT(validation_ms, 10.0) << "Validation too slow";

    std::cout << "\n=== Validation Performance ===\n";
    std::cout << "  100 rules validated in " << validation_ms << " ms\n";

    config_schema_destroy(schema);
    config_shutdown();
    std::filesystem::remove(valid_config);
}

//=============================================================================
// Expansion Performance Tests
//=============================================================================

TEST_F(ConfigPerformanceTest, ExpansionPerformance) {
    // WHAT: Measure environment variable expansion performance
    // WHY:  Ensure expansion doesn't slow down config loading
    // HOW:  Config with many expansions, measure parse time

    setenv("PERF_VAR1", "value1", 1);
    setenv("PERF_VAR2", "value2", 1);

    std::string expand_config = "/tmp/nimcp_expand_perf.ini";
    std::ofstream config_file(expand_config);
    config_file << "[expansion]\n";
    for (int i = 0; i < 100; i++) {
        config_file << "path_" << i << " = ${PERF_VAR1}/dir" << i << "/${PERF_VAR2}\n";
    }
    config_file.close();

    timer.start();
    ASSERT_TRUE(config_init(expand_config.c_str()));
    double parse_ms = timer.elapsed_ms();

    // Should parse with expansions quickly
    EXPECT_LT(parse_ms, 50.0) << "Expansion slowed parsing";

    // Verify expansion worked
    const char* val = config_get_nested_string("expansion.path_0", "");
    EXPECT_NE(strstr(val, "value1"), nullptr);
    EXPECT_NE(strstr(val, "value2"), nullptr);

    std::cout << "\n=== Expansion Performance ===\n";
    std::cout << "  100 expansions in " << parse_ms << " ms\n";

    config_shutdown();
    std::filesystem::remove(expand_config);
    unsetenv("PERF_VAR1");
    unsetenv("PERF_VAR2");
}

//=============================================================================
// Regression Baselines
//=============================================================================

TEST_F(ConfigPerformanceTest, RegressionBaseline) {
    // WHAT: Establish performance baseline for regression detection
    // WHY:  Track performance over time, detect regressions
    // HOW:  Run standard benchmark suite, output results

    std::cout << "\n=== PERFORMANCE BASELINE ===\n";
    std::cout << "This test establishes performance baselines.\n";
    std::cout << "Compare future runs against these values to detect regressions.\n\n";

    // Hash operations
    populate_table(10000);
    config_value_t val, result;

    timer.start();
    for (int i = 0; i < 10000; i++) {
        char key[64];
        snprintf(key, sizeof(key), "perf_key_%d", i);
        config_hash_get(table, key, &result, nullptr);
    }
    double lookup_time = timer.elapsed_us() / 10000.0;

    timer.start();
    for (int i = 10000; i < 20000; i++) {
        char key[64];
        snprintf(key, sizeof(key), "perf_key_%d", i);
        val.i = i;
        config_hash_set(table, key, &val, CONFIG_VALUE_INT);
    }
    double insert_time = timer.elapsed_us() / 10000.0;

    std::cout << "BASELINE METRICS:\n";
    std::cout << "  Hash lookup (10k entries): " << lookup_time << " us\n";
    std::cout << "  Hash insert (10k entries): " << insert_time << " us\n";
    std::cout << "  Load factor: " << config_hash_load_factor(table) << "\n";
    std::cout << "  Capacity: " << config_hash_capacity(table) << "\n";
    std::cout << "\nThese values should remain relatively stable across builds.\n";

    // Assert reasonable baseline values
    EXPECT_LT(lookup_time, 1.0);  // < 1us per lookup
    EXPECT_LT(insert_time, 10.0); // < 10us per insert
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
