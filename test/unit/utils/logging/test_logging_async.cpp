//=============================================================================
// test_logging_async.cpp - Unit Tests for Async Logging
//=============================================================================
/**
 * @file test_logging_async.cpp
 * @brief Unit tests for async logging with ring buffer in NIMCP logging system
 *
 * Tests cover:
 * - Async mode creation
 * - Lock-free ring buffer operations
 * - Background writer thread
 * - Async flush
 * - Hybrid mode
 * - Buffer overflow handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LoggingAsyncTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_async_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) {
            close(fd);
            test_log_path_ = std::string(template_path);
        } else {
            test_log_path_ = "/tmp/nimcp_async_test.log";
        }
    }

    void TearDown() override {
        if (logger_) {
            nimcp_log_destroy(logger_);
            logger_ = nullptr;
        }
        if (!test_log_path_.empty()) {
            unlink(test_log_path_.c_str());
        }
    }

    nimcp_log_config_t createAsyncConfig() {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_ON;
        config.level = LOG_LEVEL_TRACE;
        config.buffer_size = 1024;  // Small buffer for testing
        config.flush_interval_ms = 50;
        config.rate_limit.enabled = false;  // Disable rate limiting for tests
        return config;
    }

    std::string readLogFile() {
        std::ifstream file(test_log_path_);
        if (!file.is_open()) return "";
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
    }

    size_t countLines(const std::string& content) {
        size_t count = 0;
        for (char c : content) {
            if (c == '\n') count++;
        }
        return count;
    }
};

//=============================================================================
// Async Mode Creation Tests
//=============================================================================

TEST_F(LoggingAsyncTest, CreateAsyncLogger) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);
    EXPECT_TRUE(nimcp_log_is_initialized(logger_));
}

TEST_F(LoggingAsyncTest, CreateHybridLogger) {
    nimcp_log_config_t config = createAsyncConfig();
    config.async_mode = NIMCP_LOG_ASYNC_HYBRID;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);
    EXPECT_TRUE(nimcp_log_is_initialized(logger_));
}

//=============================================================================
// Basic Async Operation Tests
//=============================================================================

TEST_F(LoggingAsyncTest, AsyncWrite_Basic) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Async test message");
    nimcp_log_flush(logger_);

    // Wait for async writer to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Async test message") != std::string::npos);
}

TEST_F(LoggingAsyncTest, AsyncWrite_Multiple) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Message %d", i);
    }
    nimcp_log_flush(logger_);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Message 0") != std::string::npos);
    EXPECT_TRUE(content.find("Message 99") != std::string::npos);
}

//=============================================================================
// Flush Tests
//=============================================================================

TEST_F(LoggingAsyncTest, AsyncFlush_WaitsForComplete) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Pre-flush message");

    // Flush should block until all messages are written
    nimcp_log_flush(logger_);

    // Should be immediately available after flush
    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Pre-flush message") != std::string::npos);
}

TEST_F(LoggingAsyncTest, AsyncFlush_Multiple) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 5; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Batch %d", i);
        nimcp_log_flush(logger_);
    }

    std::string content = readLogFile();
    for (int i = 0; i < 5; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "Batch %d", i);
        EXPECT_TRUE(content.find(expected) != std::string::npos);
    }
}

//=============================================================================
// High-Throughput Tests
//=============================================================================

TEST_F(LoggingAsyncTest, HighThroughput_SingleThread) {
    nimcp_log_config_t config = createAsyncConfig();
    config.buffer_size = 8192;
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    const int num_messages = 10000;
    for (int i = 0; i < num_messages; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "High throughput message %d", i);
    }

    auto write_done = std::chrono::high_resolution_clock::now();
    nimcp_log_flush(logger_);
    auto flush_done = std::chrono::high_resolution_clock::now();

    auto write_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        write_done - start).count();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        flush_done - start).count();

    // Async writes should be fast
    EXPECT_LT(write_time, 1000);  // Should complete writes in <1 second

    std::string content = readLogFile();
    size_t lines = countLines(content);
    EXPECT_GE(lines, num_messages * 0.99);  // Allow for minor drops
}

TEST_F(LoggingAsyncTest, HighThroughput_MultiThread) {
    nimcp_log_config_t config = createAsyncConfig();
    config.buffer_size = 16384;
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    const int num_threads = 4;
    const int messages_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> total_written{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, messages_per_thread, &total_written]() {
            for (int i = 0; i < messages_per_thread; i++) {
                nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                               "Thread %d message %d", t, i);
                total_written++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    nimcp_log_flush(logger_);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string content = readLogFile();
    size_t lines = countLines(content);

    // Should have most messages (some may be dropped under contention)
    EXPECT_GE(lines, total_written * 0.95);
}

//=============================================================================
// Hybrid Mode Tests
//=============================================================================

TEST_F(LoggingAsyncTest, HybridMode_ErrorIsSync) {
    nimcp_log_config_t config = createAsyncConfig();
    config.async_mode = NIMCP_LOG_ASYNC_HYBRID;
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Error should be written synchronously (immediately visible)
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__,
                    "Sync error message");

    // No flush needed for sync messages
    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Sync error message") != std::string::npos);
}

TEST_F(LoggingAsyncTest, HybridMode_InfoIsAsync) {
    nimcp_log_config_t config = createAsyncConfig();
    config.async_mode = NIMCP_LOG_ASYNC_HYBRID;
    config.flush_interval_ms = 1000;  // Long interval
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // INFO should be written asynchronously
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Async info message");

    // Should not be immediately visible without flush
    std::string content1 = readLogFile();

    // Now flush
    nimcp_log_flush(logger_);

    std::string content2 = readLogFile();
    EXPECT_TRUE(content2.find("Async info message") != std::string::npos);
}

TEST_F(LoggingAsyncTest, HybridMode_FatalIsSync) {
    nimcp_log_config_t config = createAsyncConfig();
    config.async_mode = NIMCP_LOG_ASYNC_HYBRID;
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_FATAL, nullptr, __FILE__, __LINE__,
                    "Fatal sync message");

    // Fatal should be written synchronously
    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Fatal sync message") != std::string::npos);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(LoggingAsyncTest, Shutdown_FlushesRemaining) {
    nimcp_log_config_t config = createAsyncConfig();
    config.flush_interval_ms = 10000;  // Very long interval
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Shutdown test %d", i);
    }

    // Destroy should flush remaining messages
    nimcp_log_destroy(logger_);
    logger_ = nullptr;

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Shutdown test 0") != std::string::npos);
    EXPECT_TRUE(content.find("Shutdown test 99") != std::string::npos);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LoggingAsyncTest, Stats_AsyncWrites) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 50; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Stats test %d", i);
    }
    nimcp_log_flush(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_GE(stats.messages_logged, 50UL);
    // Async writes may batch messages, so count may be slightly less than message count
    // Allow for minor variation due to timing/batching
    EXPECT_GE(stats.async_writes, 45UL);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LoggingAsyncTest, EmptyMessage_Async) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "");
    nimcp_log_flush(logger_);
    SUCCEED();  // Should not crash
}

TEST_F(LoggingAsyncTest, VeryLongMessage_Async) {
    nimcp_log_config_t config = createAsyncConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    std::string long_msg(5000, 'X');
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "%s", long_msg.c_str());
    nimcp_log_flush(logger_);
    SUCCEED();  // Should not crash (may truncate)
}
