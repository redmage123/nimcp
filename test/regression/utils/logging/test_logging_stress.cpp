//=============================================================================
// test_logging_stress.cpp - Stress/Endurance Tests for Logging
//=============================================================================
/**
 * @file test_logging_stress.cpp
 * @brief Stress and endurance tests for NIMCP logging system
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <glob.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

class LoggingStressTest : public ::testing::Test {
protected:
    std::string test_dir_;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_stress_XXXXXX";
        char* dir = mkdtemp(template_path);
        test_dir_ = dir ? dir : "/tmp/nimcp_stress_test";
        if (!dir) mkdir(test_dir_.c_str(), 0755);
        test_log_path_ = test_dir_ + "/stress.log";
    }

    void TearDown() override {
        nimcp_log_shutdown();
        std::string pattern = test_dir_ + "/*";
        glob_t g;
        if (glob(pattern.c_str(), 0, nullptr, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) unlink(g.gl_pathv[i]);
            globfree(&g);
        }
        rmdir(test_dir_.c_str());
    }
};

TEST_F(LoggingStressTest, HighVolumeAsync) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 65536;
    config.rate_limit.enabled = false;  // Disable rate limiting for stress tests

    ASSERT_EQ(nimcp_log_init(&config), 0);

    const int total_messages = 500000;

    for (int i = 0; i < total_messages; i++) {
        LOG_INFO("High volume stress test message %d", i);
    }
    nimcp_log_flush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);

    // Should have logged at least 95% of messages
    EXPECT_GE(stats.messages_logged, (uint64_t)(total_messages * 0.95));
}

TEST_F(LoggingStressTest, ConcurrentStress) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 65536;
    config.rate_limit.enabled = false;  // Disable rate limiting for stress tests

    ASSERT_EQ(nimcp_log_init(&config), 0);

    const int num_threads = 16;
    const int msgs_per_thread = 10000;
    std::vector<std::thread> threads;
    std::atomic<int> completed{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, msgs_per_thread, &completed]() {
            for (int i = 0; i < msgs_per_thread; i++) {
                LOG_INFO("Concurrent stress T%d M%d", t, i);
            }
            completed++;
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(completed.load(), num_threads);

    nimcp_log_flush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);

    EXPECT_GE(stats.messages_logged, (uint64_t)(num_threads * msgs_per_thread * 0.9));
}

TEST_F(LoggingStressTest, RapidRotation) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_OFF;
    config.level = LOG_LEVEL_INFO;
    config.rotation.mode = NIMCP_LOG_ROTATE_SIZE;
    config.rotation.max_file_size = 4096;  // Small size for rapid rotation
    config.rotation.max_rotated_files = 10;
    config.rate_limit.enabled = false;  // Disable rate limiting for stress tests

    ASSERT_EQ(nimcp_log_init(&config), 0);

    for (int i = 0; i < 10000; i++) {
        LOG_INFO("Rapid rotation stress test with longer message content %d", i);
    }
    nimcp_log_flush(nullptr);

    // Count rotated files
    glob_t g;
    std::string pattern = test_log_path_ + "*";
    ASSERT_EQ(glob(pattern.c_str(), 0, nullptr, &g), 0);

    // Should have created multiple rotated files
    EXPECT_GE(g.gl_pathc, 2U);
    EXPECT_LE(g.gl_pathc, 12U);  // max_files + current + some buffer
    globfree(&g);
}

TEST_F(LoggingStressTest, MixedLevelStress) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_HYBRID;
    config.level = LOG_LEVEL_TRACE;
    config.buffer_size = 32768;
    config.rate_limit.enabled = false;  // Disable rate limiting for stress tests

    ASSERT_EQ(nimcp_log_init(&config), 0);

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < 5000; i++) {
                switch (i % 6) {
                    case 0: LOG_TRACE("Thread %d trace %d", t, i); break;
                    case 1: LOG_DEBUG("Thread %d debug %d", t, i); break;
                    case 2: LOG_INFO("Thread %d info %d", t, i); break;
                    case 3: LOG_WARN("Thread %d warn %d", t, i); break;
                    case 4: LOG_ERROR("Thread %d error %d", t, i); break;
                    case 5: LOG_FATAL("Thread %d fatal %d", t, i); break;
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    nimcp_log_flush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);
    EXPECT_GE(stats.messages_logged, 35000UL);
}

TEST_F(LoggingStressTest, LongRunningEndurance) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 16384;
    config.rate_limit.enabled = false;  // Disable rate limiting for stress tests

    ASSERT_EQ(nimcp_log_init(&config), 0);

    std::atomic<bool> running{true};
    std::atomic<uint64_t> total_logged{0};
    std::vector<std::thread> threads;

    // Run for 2 seconds
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([t, &running, &total_logged]() {
            int count = 0;
            while (running) {
                LOG_INFO("Endurance test T%d C%d", t, count++);
                total_logged++;
                if (count % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    running = false;

    for (auto& t : threads) t.join();
    nimcp_log_flush(nullptr);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);

    // Should have logged many messages without issues
    EXPECT_GT(total_logged.load(), 10000UL);
    EXPECT_GE(stats.messages_logged, total_logged.load() * 0.9);
}

TEST_F(LoggingStressTest, RapidCreateDestroy) {
    for (int cycle = 0; cycle < 50; cycle++) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_ON;
        config.level = LOG_LEVEL_INFO;

        nimcp_logger_t logger = nimcp_log_create(&config);
        ASSERT_NE(logger, nullptr);

        for (int i = 0; i < 100; i++) {
            nimcp_log_write(logger, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                           "Cycle %d msg %d", cycle, i);
        }
        nimcp_log_flush(logger);
        nimcp_log_destroy(logger);
    }
    SUCCEED();
}
