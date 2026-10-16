//=============================================================================
// test_logging_thread_safety.cpp - Thread Safety Tests for Logging
//=============================================================================
/**
 * @file test_logging_thread_safety.cpp
 * @brief Comprehensive thread safety tests for NIMCP logging system
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
#include <string>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

class LoggingThreadSafetyTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_thread_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) { close(fd); test_log_path_ = template_path; }
        else { test_log_path_ = "/tmp/nimcp_thread_test.log"; }
    }

    void TearDown() override {
        if (logger_) { nimcp_log_destroy(logger_); logger_ = nullptr; }
        if (!test_log_path_.empty()) unlink(test_log_path_.c_str());
    }

    nimcp_log_config_t createConfig(bool async = false) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = async ? NIMCP_LOG_ASYNC_ON : NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_TRACE;
        config.buffer_size = 8192;
        return config;
    }

    size_t countLines() {
        std::ifstream file(test_log_path_);
        size_t count = 0;
        std::string line;
        while (std::getline(file, line)) { if (!line.empty()) count++; }
        return count;
    }
};

TEST_F(LoggingThreadSafetyTest, ConcurrentWrites_Sync) {
    nimcp_log_config_t config = createConfig(false);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    const int num_threads = 8;
    const int msgs_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, msgs_per_thread, &total]() {
            for (int i = 0; i < msgs_per_thread; i++) {
                nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                               "Thread %d message %d", t, i);
                total++;
            }
        });
    }

    for (auto& t : threads) t.join();
    nimcp_log_flush(logger_);

    EXPECT_EQ(total.load(), num_threads * msgs_per_thread);
    EXPECT_EQ(countLines(), static_cast<size_t>(num_threads * msgs_per_thread));
}

TEST_F(LoggingThreadSafetyTest, ConcurrentWrites_Async) {
    nimcp_log_config_t config = createConfig(true);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    const int num_threads = 8;
    const int msgs_per_thread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, msgs_per_thread]() {
            for (int i = 0; i < msgs_per_thread; i++) {
                nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                               "Async T%d M%d", t, i);
            }
        });
    }

    for (auto& t : threads) t.join();
    nimcp_log_flush(logger_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t lines = countLines();
    EXPECT_GE(lines, static_cast<size_t>(num_threads * msgs_per_thread * 0.95));
}

TEST_F(LoggingThreadSafetyTest, ConcurrentLevelChange) {
    nimcp_log_config_t config = createConfig(false);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Msg");
            }
        });
    }

    // Level changer thread
    threads.emplace_back([this, &running]() {
        while (running) {
            nimcp_log_set_level(logger_, LOG_LEVEL_DEBUG);
            nimcp_log_set_level(logger_, LOG_LEVEL_WARN);
            nimcp_log_set_level(logger_, LOG_LEVEL_INFO);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) t.join();
    nimcp_log_flush(logger_);
    SUCCEED();  // No crashes
}

TEST_F(LoggingThreadSafetyTest, ConcurrentFlush) {
    nimcp_log_config_t config = createConfig(true);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "M");
            }
        });
    }

    // Flusher threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_log_flush(logger_);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) t.join();
    SUCCEED();
}

TEST_F(LoggingThreadSafetyTest, ConcurrentStatsAccess) {
    nimcp_log_config_t config = createConfig(false);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Writers
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "M");
            }
        });
    }

    // Stats readers
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            nimcp_log_stats_t stats;
            while (running) {
                nimcp_log_get_stats(logger_, &stats);
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) t.join();
    SUCCEED();
}

TEST_F(LoggingThreadSafetyTest, RaceCondition_CreateDestroy) {
    // Test rapid create/destroy doesn't cause issues
    for (int i = 0; i < 10; i++) {
        nimcp_log_config_t config = createConfig(false);
        nimcp_logger_t l = nimcp_log_create(&config);
        ASSERT_NE(l, nullptr);
        nimcp_log_write(l, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Quick test");
        nimcp_log_destroy(l);
    }
    SUCCEED();
}

TEST_F(LoggingThreadSafetyTest, GlobalLogger_ConcurrentAccess) {
    nimcp_log_config_t config = createConfig(false);
    nimcp_log_init(&config);

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < 50; i++) {
                nimcp_log_write(nullptr, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                               "Global T%d M%d", t, i);
            }
        });
    }

    for (auto& t : threads) t.join();
    nimcp_log_shutdown();
    SUCCEED();
}

TEST_F(LoggingThreadSafetyTest, HighContention_NoDeadlock) {
    nimcp_log_config_t config = createConfig(true);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> ops{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 16; t++) {
        threads.emplace_back([this, &running, &ops, t]() {
            while (running) {
                nimcp_log_write(logger_, (log_level_t)(t % 5), nullptr, __FILE__, __LINE__,
                               "High contention test message from thread %d", t);
                ops++;
            }
        });
    }

    // Run for 200ms
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    for (auto& t : threads) t.join();
    nimcp_log_flush(logger_);

    EXPECT_GT(ops.load(), 1000);  // Should have processed many ops
}
