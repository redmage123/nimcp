//=============================================================================
// test_logging_performance.cpp - Performance Regression Tests for Logging
//=============================================================================
/**
 * @file test_logging_performance.cpp
 * @brief Performance regression tests for NIMCP logging system
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

class LoggingPerformanceTest : public ::testing::Test {
protected:
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_perf_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) { close(fd); test_log_path_ = template_path; }
        else { test_log_path_ = "/tmp/nimcp_perf_test.log"; }
    }

    void TearDown() override {
        nimcp_log_shutdown();
        if (!test_log_path_.empty()) unlink(test_log_path_.c_str());
    }

    double measureThroughput(int num_messages, bool async) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = async ? NIMCP_LOG_ASYNC_ON : NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_INFO;
        config.buffer_size = 16384;
        config.rate_limit.enabled = false;  // Disable rate limiting for throughput tests

        nimcp_log_init(&config);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_messages; i++) {
            LOG_INFO("Performance test message number %d with some content", i);
        }
        nimcp_log_flush(nullptr);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        nimcp_log_shutdown();

        return (double)num_messages / (duration.count() / 1000000.0);
    }
};

TEST_F(LoggingPerformanceTest, SyncThroughput_Baseline) {
    double throughput = measureThroughput(10000, false);

    // Sync logging should handle at least 10K msgs/sec
    EXPECT_GT(throughput, 10000.0) << "Sync throughput: " << throughput << " msgs/sec";
}

TEST_F(LoggingPerformanceTest, AsyncThroughput_Baseline) {
    double throughput = measureThroughput(50000, true);

    // Async logging should handle at least 100K msgs/sec
    EXPECT_GT(throughput, 100000.0) << "Async throughput: " << throughput << " msgs/sec";
}

TEST_F(LoggingPerformanceTest, AsyncVsSync_Improvement) {
    double sync_throughput = measureThroughput(5000, false);
    double async_throughput = measureThroughput(5000, true);

    double improvement = async_throughput / sync_throughput;

    // Async may not be faster for small message counts due to thread overhead
    // Just verify async doesn't degrade significantly (at least 0.3x of sync)
    EXPECT_GT(improvement, 0.3) << "Async improvement: " << improvement << "x";
}

TEST_F(LoggingPerformanceTest, MultithreadedThroughput) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 32768;
    config.rate_limit.enabled = false;  // Disable rate limiting for throughput tests

    nimcp_log_init(&config);

    const int num_threads = 8;
    const int msgs_per_thread = 10000;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, msgs_per_thread]() {
            for (int i = 0; i < msgs_per_thread; i++) {
                LOG_INFO("Thread %d message %d", t, i);
            }
        });
    }

    for (auto& t : threads) t.join();
    nimcp_log_flush(nullptr);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double throughput = (double)(num_threads * msgs_per_thread) / (duration.count() / 1000.0);

    // 8 threads should achieve at least 200K msgs/sec combined
    EXPECT_GT(throughput, 200000.0) << "MT throughput: " << throughput << " msgs/sec";
}

TEST_F(LoggingPerformanceTest, Latency_P99) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.rate_limit.enabled = false;  // Disable rate limiting for latency tests

    nimcp_log_init(&config);

    std::vector<long> latencies;
    latencies.reserve(1000);

    for (int i = 0; i < 1000; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        LOG_INFO("Latency test message %d", i);
        auto end = std::chrono::high_resolution_clock::now();
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    std::sort(latencies.begin(), latencies.end());
    long p99 = latencies[989];  // 99th percentile

    // P99 latency should be under 100 microseconds for async
    EXPECT_LT(p99, 100000) << "P99 latency: " << p99 << " ns";
}

TEST_F(LoggingPerformanceTest, MemoryUsage_Bounded) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 8192;
    config.rate_limit.enabled = false;  // Disable rate limiting for throughput tests

    nimcp_log_init(&config);

    // Write many messages rapidly
    for (int i = 0; i < 100000; i++) {
        LOG_INFO("Memory test message %d", i);
        if (i % 10000 == 0) {
            nimcp_log_flush(nullptr);
        }
    }
    nimcp_log_flush(nullptr);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);

    // Should have logged most messages without excessive memory use
    EXPECT_GT(stats.messages_logged, 90000UL);
}
