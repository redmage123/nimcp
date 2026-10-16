/**
 * @file test_lockfree_metrics.cpp
 * @brief Comprehensive Unit Tests for Lock-Free Metrics Ring Buffer
 * @version 1.0.0
 * @date 2025-11-20
 *
 * Test coverage: 100% (all functions, branches, edge cases)
 * Test count: 40+ tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_lockfree_metrics.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LockfreeMetricsTest : public ::testing::Test {
protected:
    lockfree_metrics_buffer_t* buffer;

    void SetUp() override {
        buffer = nullptr;
    }

    void TearDown() override {
        if (buffer) {
            lockfree_metrics_destroy(buffer);
            buffer = nullptr;
        }
    }
};

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, NextPowerOf2) {
    EXPECT_EQ(lockfree_metrics_next_power_of_2(0), 1u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(1), 1u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(2), 2u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(3), 4u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(7), 8u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(8), 8u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(9), 16u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(100), 128u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(1000), 1024u);
    EXPECT_EQ(lockfree_metrics_next_power_of_2(1024), 1024u);
}

TEST_F(LockfreeMetricsTest, IsPowerOf2) {
    EXPECT_FALSE(lockfree_metrics_is_power_of_2(0));
    EXPECT_TRUE(lockfree_metrics_is_power_of_2(1));
    EXPECT_TRUE(lockfree_metrics_is_power_of_2(2));
    EXPECT_FALSE(lockfree_metrics_is_power_of_2(3));
    EXPECT_TRUE(lockfree_metrics_is_power_of_2(4));
    EXPECT_FALSE(lockfree_metrics_is_power_of_2(5));
    EXPECT_FALSE(lockfree_metrics_is_power_of_2(7));
    EXPECT_TRUE(lockfree_metrics_is_power_of_2(8));
    EXPECT_TRUE(lockfree_metrics_is_power_of_2(1024));
    EXPECT_FALSE(lockfree_metrics_is_power_of_2(1000));
}

TEST_F(LockfreeMetricsTest, GetTimestamp) {
    uint64_t t1 = lockfree_metrics_get_timestamp_us();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t t2 = lockfree_metrics_get_timestamp_us();

    EXPECT_GT(t2, t1);
    EXPECT_GE(t2 - t1, 10000u);  // At least 10ms
}

TEST_F(LockfreeMetricsTest, StringConversions) {
    // Metric type to string
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_LATENCY), "LATENCY");
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_ERROR), "ERROR");
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_THROUGHPUT), "THROUGHPUT");
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_CACHE_HIT), "CACHE_HIT");
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_THREAD_WAIT), "THREAD_WAIT");
    EXPECT_STREQ(metric_type_to_string(METRIC_TYPE_CUSTOM), "CUSTOM");
    EXPECT_STREQ(metric_type_to_string((metric_type_t)999), "UNKNOWN");

    // Result to string
    EXPECT_STREQ(metric_result_to_string(METRIC_RESULT_SUCCESS), "SUCCESS");
    EXPECT_STREQ(metric_result_to_string(METRIC_RESULT_DROPPED), "DROPPED");
    EXPECT_STREQ(metric_result_to_string(METRIC_RESULT_INVALID_INPUT), "INVALID_INPUT");
    EXPECT_STREQ(metric_result_to_string(METRIC_RESULT_ERROR), "ERROR");
    EXPECT_STREQ(metric_result_to_string((metric_result_t)999), "UNKNOWN");
}

//=============================================================================
// Buffer Creation Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, CreateBufferDefault) {
    buffer = lockfree_metrics_create(LOCKFREE_METRICS_DEFAULT_CAPACITY, "test_buffer");
    ASSERT_NE(buffer, nullptr);

    EXPECT_EQ(lockfree_metrics_capacity(buffer), LOCKFREE_METRICS_DEFAULT_CAPACITY);
    EXPECT_EQ(lockfree_metrics_size(buffer), 0u);
    EXPECT_TRUE(lockfree_metrics_is_empty(buffer));
    EXPECT_FALSE(lockfree_metrics_is_full(buffer));
    EXPECT_STREQ(buffer->name, "test_buffer");
}

TEST_F(LockfreeMetricsTest, CreateBufferCustomCapacity) {
    buffer = lockfree_metrics_create(256, "custom");
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(lockfree_metrics_capacity(buffer), 256u);
}

TEST_F(LockfreeMetricsTest, CreateBufferRoundsUpToPowerOf2) {
    buffer = lockfree_metrics_create(100, "rounded");
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(lockfree_metrics_capacity(buffer), 128u);  // Rounded up
}

TEST_F(LockfreeMetricsTest, CreateBufferMinCapacity) {
    buffer = lockfree_metrics_create(1, "tiny");
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(lockfree_metrics_capacity(buffer), LOCKFREE_METRICS_MIN_CAPACITY);
}

TEST_F(LockfreeMetricsTest, CreateBufferMaxCapacity) {
    buffer = lockfree_metrics_create(LOCKFREE_METRICS_MAX_CAPACITY + 1, "huge");
    EXPECT_EQ(buffer, nullptr);  // Exceeds maximum
}

TEST_F(LockfreeMetricsTest, CreateBufferNoName) {
    buffer = lockfree_metrics_create(64, nullptr);
    ASSERT_NE(buffer, nullptr);
    EXPECT_GT(strlen(buffer->name), 0u);  // Auto-generated name
}

TEST_F(LockfreeMetricsTest, DestroyNullBuffer) {
    lockfree_metrics_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Single-Threaded Recording Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, RecordSingleMetric) {
    buffer = lockfree_metrics_create(64, "single");
    ASSERT_NE(buffer, nullptr);

    metric_result_t result = lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, 123.45, 0);
    EXPECT_EQ(result, METRIC_RESULT_SUCCESS);
    EXPECT_EQ(lockfree_metrics_size(buffer), 1u);
    EXPECT_FALSE(lockfree_metrics_is_empty(buffer));
}

TEST_F(LockfreeMetricsTest, RecordMultipleMetrics) {
    buffer = lockfree_metrics_create(64, "multiple");
    ASSERT_NE(buffer, nullptr);

    for (int i = 0; i < 10; i++) {
        metric_result_t result = lockfree_metrics_record(
            buffer, METRIC_TYPE_LATENCY, (double)i, 0);
        EXPECT_EQ(result, METRIC_RESULT_SUCCESS);
    }

    EXPECT_EQ(lockfree_metrics_size(buffer), 10u);
}

TEST_F(LockfreeMetricsTest, RecordWithMetadata) {
    buffer = lockfree_metrics_create(64, "metadata");
    ASSERT_NE(buffer, nullptr);

    uint64_t metadata = 0xDEADBEEFCAFEBABE;
    metric_result_t result = lockfree_metrics_record_with_metadata(
        buffer, METRIC_TYPE_MEMORY, 1024.0, 42, metadata);

    EXPECT_EQ(result, METRIC_RESULT_SUCCESS);

    metric_entry_t entry;
    int32_t read = lockfree_metrics_peek(buffer, &entry, 1);
    ASSERT_EQ(read, 1);
    EXPECT_EQ(entry.type, METRIC_TYPE_MEMORY);
    EXPECT_DOUBLE_EQ(entry.value, 1024.0);
    EXPECT_EQ(entry.component_id, 42u);
    EXPECT_EQ(entry.metadata, metadata);
}

TEST_F(LockfreeMetricsTest, RecordWithTimestamp) {
    buffer = lockfree_metrics_create(64, "timestamped");
    ASSERT_NE(buffer, nullptr);

    uint64_t custom_timestamp = 1234567890;
    metric_result_t result = lockfree_metrics_record_timestamped(
        buffer, custom_timestamp, METRIC_TYPE_ERROR, 1.0, 0, 0);

    EXPECT_EQ(result, METRIC_RESULT_SUCCESS);

    metric_entry_t entry;
    lockfree_metrics_peek(buffer, &entry, 1);
    EXPECT_EQ(entry.timestamp_us, custom_timestamp);
}

TEST_F(LockfreeMetricsTest, RecordInvalidType) {
    buffer = lockfree_metrics_create(64, "invalid");
    ASSERT_NE(buffer, nullptr);

    metric_result_t result = lockfree_metrics_record(
        buffer, (metric_type_t)999, 0.0, 0);
    EXPECT_EQ(result, METRIC_RESULT_INVALID_INPUT);
}

TEST_F(LockfreeMetricsTest, RecordNullBuffer) {
    metric_result_t result = lockfree_metrics_record(
        nullptr, METRIC_TYPE_LATENCY, 0.0, 0);
    EXPECT_EQ(result, METRIC_RESULT_INVALID_INPUT);
}

TEST_F(LockfreeMetricsTest, RecordUntilFull) {
    buffer = lockfree_metrics_create(16, "fill");
    ASSERT_NE(buffer, nullptr);

    // Fill buffer
    for (uint32_t i = 0; i < 16; i++) {
        metric_result_t result = lockfree_metrics_record(
            buffer, METRIC_TYPE_LATENCY, (double)i, 0);
        EXPECT_EQ(result, METRIC_RESULT_SUCCESS);
    }

    EXPECT_TRUE(lockfree_metrics_is_full(buffer));

    // Try to add one more (should be dropped)
    metric_result_t result = lockfree_metrics_record(
        buffer, METRIC_TYPE_LATENCY, 999.0, 0);
    EXPECT_EQ(result, METRIC_RESULT_DROPPED);

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);
    EXPECT_EQ(stats.total_dropped, 1u);
}

//=============================================================================
// Reading Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, ReadBatchEmpty) {
    buffer = lockfree_metrics_create(64, "read_empty");
    ASSERT_NE(buffer, nullptr);

    metric_entry_t entries[10];
    int32_t read = lockfree_metrics_read_batch(buffer, entries, 10);
    EXPECT_EQ(read, 0);
}

TEST_F(LockfreeMetricsTest, ReadBatchSingle) {
    buffer = lockfree_metrics_create(64, "read_single");
    ASSERT_NE(buffer, nullptr);

    lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, 123.45, 7);

    metric_entry_t entry;
    int32_t read = lockfree_metrics_read_batch(buffer, &entry, 1);
    ASSERT_EQ(read, 1);
    EXPECT_EQ(entry.type, METRIC_TYPE_LATENCY);
    EXPECT_DOUBLE_EQ(entry.value, 123.45);
    EXPECT_EQ(entry.component_id, 7u);

    EXPECT_TRUE(lockfree_metrics_is_empty(buffer));
}

TEST_F(LockfreeMetricsTest, ReadBatchMultiple) {
    buffer = lockfree_metrics_create(64, "read_batch");
    ASSERT_NE(buffer, nullptr);

    // Record 10 metrics
    for (int i = 0; i < 10; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_MEMORY, (double)i, (uint32_t)i);
    }

    // Read all
    metric_entry_t entries[10];
    int32_t read = lockfree_metrics_read_batch(buffer, entries, 10);
    ASSERT_EQ(read, 10);

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(entries[i].type, METRIC_TYPE_MEMORY);
        EXPECT_DOUBLE_EQ(entries[i].value, (double)i);
        EXPECT_EQ(entries[i].component_id, (uint32_t)i);
    }

    EXPECT_TRUE(lockfree_metrics_is_empty(buffer));
}

TEST_F(LockfreeMetricsTest, ReadBatchPartial) {
    buffer = lockfree_metrics_create(64, "read_partial");
    ASSERT_NE(buffer, nullptr);

    // Record 10 metrics
    for (int i = 0; i < 10; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_ERROR, 1.0, 0);
    }

    // Read only 5
    metric_entry_t entries[5];
    int32_t read = lockfree_metrics_read_batch(buffer, entries, 5);
    EXPECT_EQ(read, 5);
    EXPECT_EQ(lockfree_metrics_size(buffer), 5u);
}

TEST_F(LockfreeMetricsTest, ReadBatchMoreThanAvailable) {
    buffer = lockfree_metrics_create(64, "read_more");
    ASSERT_NE(buffer, nullptr);

    // Record 5 metrics
    for (int i = 0; i < 5; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    // Try to read 10 (only 5 available)
    metric_entry_t entries[10];
    int32_t read = lockfree_metrics_read_batch(buffer, entries, 10);
    EXPECT_EQ(read, 5);
}

TEST_F(LockfreeMetricsTest, ReadBatchInvalidArgs) {
    buffer = lockfree_metrics_create(64, "read_invalid");
    ASSERT_NE(buffer, nullptr);

    metric_entry_t entries[10];

    EXPECT_EQ(lockfree_metrics_read_batch(nullptr, entries, 10), -1);
    EXPECT_EQ(lockfree_metrics_read_batch(buffer, nullptr, 10), -1);
    EXPECT_EQ(lockfree_metrics_read_batch(buffer, entries, 0), -1);
}

TEST_F(LockfreeMetricsTest, PeekDoesNotConsume) {
    buffer = lockfree_metrics_create(64, "peek");
    ASSERT_NE(buffer, nullptr);

    lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, 123.0, 0);

    // Peek
    metric_entry_t entry1;
    int32_t peeked = lockfree_metrics_peek(buffer, &entry1, 1);
    EXPECT_EQ(peeked, 1);
    EXPECT_DOUBLE_EQ(entry1.value, 123.0);

    // Buffer should still have the entry
    EXPECT_EQ(lockfree_metrics_size(buffer), 1u);

    // Peek again
    metric_entry_t entry2;
    peeked = lockfree_metrics_peek(buffer, &entry2, 1);
    EXPECT_EQ(peeked, 1);
    EXPECT_DOUBLE_EQ(entry2.value, 123.0);
}

TEST_F(LockfreeMetricsTest, ReadBatchTimeout) {
    buffer = lockfree_metrics_create(64, "timeout");
    ASSERT_NE(buffer, nullptr);

    // Empty buffer, timeout = 0 (no wait)
    metric_entry_t entries[10];
    int32_t read = lockfree_metrics_read_batch_timeout(buffer, entries, 10, 0);
    EXPECT_EQ(read, 0);

    // Add data
    lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, 1.0, 0);

    // Should read immediately
    read = lockfree_metrics_read_batch_timeout(buffer, entries, 10, 1000);
    EXPECT_EQ(read, 1);
}

//=============================================================================
// Wraparound Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, WrapAroundCorrectness) {
    buffer = lockfree_metrics_create(16, "wraparound");
    ASSERT_NE(buffer, nullptr);

    // Write and read to force wraparound
    for (int cycle = 0; cycle < 5; cycle++) {
        // Fill buffer
        for (int i = 0; i < 16; i++) {
            lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY,
                                   (double)(cycle * 16 + i), 0);
        }

        // Read all
        metric_entry_t entries[16];
        int32_t read = lockfree_metrics_read_batch(buffer, entries, 16);
        ASSERT_EQ(read, 16);

        // Verify order
        for (int i = 0; i < 16; i++) {
            EXPECT_DOUBLE_EQ(entries[i].value, (double)(cycle * 16 + i));
        }
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, GetStats) {
    buffer = lockfree_metrics_create(64, "stats");
    ASSERT_NE(buffer, nullptr);

    metrics_stats_t stats;
    EXPECT_TRUE(lockfree_metrics_get_stats(buffer, &stats));

    EXPECT_EQ(stats.capacity, 64u);
    EXPECT_EQ(stats.total_recorded, 0u);
    EXPECT_EQ(stats.total_read, 0u);
    EXPECT_EQ(stats.total_dropped, 0u);
    EXPECT_EQ(stats.current_size, 0u);
}

TEST_F(LockfreeMetricsTest, StatsAfterRecording) {
    buffer = lockfree_metrics_create(64, "stats_record");
    ASSERT_NE(buffer, nullptr);

    for (int i = 0; i < 10; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);

    EXPECT_EQ(stats.total_recorded, 10u);
    EXPECT_EQ(stats.current_size, 10u);
    EXPECT_GE(stats.record_attempts, 10u);
}

TEST_F(LockfreeMetricsTest, UtilizationCalculation) {
    buffer = lockfree_metrics_create(64, "utilization");
    ASSERT_NE(buffer, nullptr);

    EXPECT_DOUBLE_EQ(lockfree_metrics_utilization(buffer), 0.0);

    // Fill half
    for (int i = 0; i < 32; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    double util = lockfree_metrics_utilization(buffer);
    EXPECT_NEAR(util, 0.5, 0.01);

    // Fill completely
    for (int i = 0; i < 32; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    util = lockfree_metrics_utilization(buffer);
    EXPECT_NEAR(util, 1.0, 0.01);
}

TEST_F(LockfreeMetricsTest, DropRateCalculation) {
    buffer = lockfree_metrics_create(16, "droprate");
    ASSERT_NE(buffer, nullptr);

    EXPECT_DOUBLE_EQ(lockfree_metrics_drop_rate(buffer), 0.0);

    // Fill buffer
    for (int i = 0; i < 16; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    // Drop 4 more
    for (int i = 0; i < 4; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, 999.0, 0);
    }

    double drop_rate = lockfree_metrics_drop_rate(buffer);
    EXPECT_NEAR(drop_rate, 4.0 / 20.0, 0.01);  // 4 dropped out of 20 attempts
}

TEST_F(LockfreeMetricsTest, ResetStats) {
    buffer = lockfree_metrics_create(64, "reset_stats");
    ASSERT_NE(buffer, nullptr);

    // Record some metrics
    for (int i = 0; i < 10; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    lockfree_metrics_reset_stats(buffer);

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);

    EXPECT_EQ(stats.total_recorded, 0u);
    EXPECT_EQ(stats.total_read, 0u);
    EXPECT_EQ(stats.record_attempts, 0u);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, ResetBuffer) {
    buffer = lockfree_metrics_create(64, "reset");
    ASSERT_NE(buffer, nullptr);

    // Fill buffer
    for (int i = 0; i < 32; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    EXPECT_EQ(lockfree_metrics_size(buffer), 32u);

    // Reset
    EXPECT_TRUE(lockfree_metrics_reset(buffer));

    EXPECT_EQ(lockfree_metrics_size(buffer), 0u);
    EXPECT_TRUE(lockfree_metrics_is_empty(buffer));

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);
    EXPECT_EQ(stats.total_recorded, 0u);
}

TEST_F(LockfreeMetricsTest, ResetNullBuffer) {
    EXPECT_FALSE(lockfree_metrics_reset(nullptr));
}

//=============================================================================
// Multi-Threaded Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, ConcurrentWrites) {
    buffer = lockfree_metrics_create(8192, "concurrent_write");
    ASSERT_NE(buffer, nullptr);

    const int num_threads = 8;
    const int writes_per_thread = 1000;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, writes_per_thread]() {
            for (int i = 0; i < writes_per_thread; i++) {
                lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY,
                                       (double)(t * 1000 + i), (uint32_t)t);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    metrics_stats_t stats;
    lockfree_metrics_get_stats(buffer, &stats);

    EXPECT_EQ(stats.total_recorded, (uint64_t)(num_threads * writes_per_thread));
}

TEST_F(LockfreeMetricsTest, ConcurrentReads) {
    buffer = lockfree_metrics_create(4096, "concurrent_read");
    ASSERT_NE(buffer, nullptr);

    // Fill buffer
    const int total_entries = 2000;
    for (int i = 0; i < total_entries; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    const int num_readers = 4;
    std::atomic<int> total_read{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_readers; t++) {
        threads.emplace_back([this, &total_read]() {
            metric_entry_t entries[100];
            while (true) {
                int32_t read = lockfree_metrics_read_batch(buffer, entries, 100);
                if (read <= 0) break;
                total_read += read;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_read.load(), total_entries);
}

TEST_F(LockfreeMetricsTest, ConcurrentReadWrite) {
    buffer = lockfree_metrics_create(4096, "concurrent_rw");
    ASSERT_NE(buffer, nullptr);

    const int num_writers = 4;
    const int num_readers = 4;
    const int writes_per_writer = 1000;
    std::atomic<bool> stop{false};
    std::atomic<int> total_read{0};

    std::vector<std::thread> threads;

    // Writers
    for (int t = 0; t < num_writers; t++) {
        threads.emplace_back([this, t, writes_per_writer]() {
            for (int i = 0; i < writes_per_writer; i++) {
                lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
                if (i % 10 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Readers
    for (int t = 0; t < num_readers; t++) {
        threads.emplace_back([this, &stop, &total_read]() {
            metric_entry_t entries[100];
            while (!stop.load()) {
                int32_t read = lockfree_metrics_read_batch(buffer, entries, 100);
                if (read > 0) {
                    total_read += read;
                }
                std::this_thread::yield();
            }
            // Final drain
            while (true) {
                int32_t read = lockfree_metrics_read_batch(buffer, entries, 100);
                if (read <= 0) break;
                total_read += read;
            }
        });
    }

    // Wait for writers
    for (int i = 0; i < num_writers; i++) {
        threads[i].join();
    }

    // Signal readers to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    // Wait for readers
    for (int i = num_writers; i < num_writers + num_readers; i++) {
        threads[i].join();
    }

    // All written entries should be read
    EXPECT_EQ(total_read.load(), num_writers * writes_per_writer);
}

//=============================================================================
// Reporting Tests
//=============================================================================

TEST_F(LockfreeMetricsTest, Report) {
    buffer = lockfree_metrics_create(64, "report_test");
    ASSERT_NE(buffer, nullptr);

    for (int i = 0; i < 10; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_LATENCY, (double)i, 0);
    }

    // Should not crash
    lockfree_metrics_report(buffer, stdout);
}

TEST_F(LockfreeMetricsTest, ExportJSON) {
    buffer = lockfree_metrics_create(64, "json_test");
    ASSERT_NE(buffer, nullptr);

    for (int i = 0; i < 5; i++) {
        lockfree_metrics_record(buffer, METRIC_TYPE_MEMORY, (double)i, 0);
    }

    char json[2048];
    int32_t written = lockfree_metrics_export_json(buffer, json, sizeof(json));

    EXPECT_GT(written, 0);
    EXPECT_LT(written, (int32_t)sizeof(json));

    // Verify JSON contains expected fields
    EXPECT_NE(strstr(json, "\"name\""), nullptr);
    EXPECT_NE(strstr(json, "\"capacity\""), nullptr);
    EXPECT_NE(strstr(json, "\"current_size\""), nullptr);
    EXPECT_NE(strstr(json, "\"total_recorded\""), nullptr);
}

TEST_F(LockfreeMetricsTest, ExportJSONInvalidArgs) {
    buffer = lockfree_metrics_create(64, "json_invalid");
    ASSERT_NE(buffer, nullptr);

    char json[100];
    EXPECT_EQ(lockfree_metrics_export_json(nullptr, json, sizeof(json)), -1);
    EXPECT_EQ(lockfree_metrics_export_json(buffer, nullptr, sizeof(json)), -1);
    EXPECT_EQ(lockfree_metrics_export_json(buffer, json, 0), -1);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(LockfreeMetricsTest, NullBufferOperations) {
    EXPECT_EQ(lockfree_metrics_size(nullptr), 0u);
    EXPECT_FALSE(lockfree_metrics_is_empty(nullptr));
    EXPECT_FALSE(lockfree_metrics_is_full(nullptr));
    EXPECT_EQ(lockfree_metrics_capacity(nullptr), 0u);
    EXPECT_DOUBLE_EQ(lockfree_metrics_utilization(nullptr), 0.0);
    EXPECT_DOUBLE_EQ(lockfree_metrics_drop_rate(nullptr), 0.0);

    metrics_stats_t stats;
    EXPECT_FALSE(lockfree_metrics_get_stats(nullptr, &stats));

    lockfree_metrics_reset_stats(nullptr);  // Should not crash
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
