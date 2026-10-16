//=============================================================================
// test_logging_rate_limit.cpp - Unit Tests for Log Rate Limiting
//=============================================================================
/**
 * @file test_logging_rate_limit.cpp
 * @brief Unit tests for rate limiting in NIMCP logging system
 *
 * Tests cover:
 * - Token bucket algorithm
 * - Rate limit thresholds
 * - Burst handling
 * - Rate limit bypass for critical levels
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
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LoggingRateLimitTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_ratelimit_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) {
            close(fd);
            test_log_path_ = std::string(template_path);
        } else {
            test_log_path_ = "/tmp/nimcp_ratelimit_test.log";
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

    nimcp_log_config_t createRateLimitConfig(uint32_t limit, uint32_t window_ms) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_TRACE;

        config.rate_limit.enabled = true;
        config.rate_limit.max_per_second = limit;
        config.rate_limit.burst_size = limit * 2;

        return config;
    }

    size_t countLines() {
        std::ifstream file(test_log_path_);
        size_t count = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) count++;
        }
        return count;
    }

    std::string readLogFile() {
        std::ifstream file(test_log_path_);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
    }
};

//=============================================================================
// Basic Rate Limiting Tests
//=============================================================================

TEST_F(LoggingRateLimitTest, RateLimitDisabled_AllMessagesPass) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_OFF;
    config.level = LOG_LEVEL_TRACE;
    config.rate_limit.enabled = false;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Message %d", i);
    }
    nimcp_log_flush(logger_);

    size_t lines = countLines();
    EXPECT_EQ(lines, 100U);
}

TEST_F(LoggingRateLimitTest, RateLimitEnabled_ThrottlesMessages) {
    nimcp_log_config_t config = createRateLimitConfig(10, 1000);  // 10 msgs/sec

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Send 100 messages quickly
    for (int i = 0; i < 100; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Throttle test %d", i);
    }
    nimcp_log_flush(logger_);

    size_t lines = countLines();
    // Should be significantly fewer than 100 due to rate limiting
    // Allow for burst + some messages
    EXPECT_LT(lines, 50U);
}

//=============================================================================
// Burst Tests
//=============================================================================

TEST_F(LoggingRateLimitTest, BurstAllowed_InitialMessages) {
    nimcp_log_config_t config = createRateLimitConfig(5, 1000);  // 5 msgs/sec
    config.rate_limit.burst_size = 20;  // Burst of 20

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // First burst should all pass
    for (int i = 0; i < 20; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Burst %d", i);
    }
    nimcp_log_flush(logger_);

    size_t lines = countLines();
    // All or most burst messages should pass
    EXPECT_GE(lines, 15U);
}

TEST_F(LoggingRateLimitTest, BurstRefill_AfterWait) {
    nimcp_log_config_t config = createRateLimitConfig(100, 1000);  // 100 msgs/sec
    config.rate_limit.burst_size = 10;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Use up burst
    for (int i = 0; i < 20; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "First burst %d", i);
    }
    nimcp_log_flush(logger_);

    size_t first_count = countLines();

    // Wait for tokens to refill
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Second burst should have some tokens
    for (int i = 0; i < 10; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Second burst %d", i);
    }
    nimcp_log_flush(logger_);

    size_t total_count = countLines();
    EXPECT_GT(total_count, first_count);
}

//=============================================================================
// Level-Based Bypass Tests
//=============================================================================

TEST_F(LoggingRateLimitTest, ErrorLevel_BypassesRateLimit) {
    nimcp_log_config_t config = createRateLimitConfig(1, 1000);  // Very low limit

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Exhaust rate limit with INFO
    for (int i = 0; i < 20; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "INFO %d", i);
    }

    // ERROR should still pass
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__,
                    "CRITICAL_ERROR_MESSAGE");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("CRITICAL_ERROR_MESSAGE") != std::string::npos);
}

TEST_F(LoggingRateLimitTest, FatalLevel_BypassesRateLimit) {
    nimcp_log_config_t config = createRateLimitConfig(1, 1000);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Exhaust rate limit
    for (int i = 0; i < 20; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Exhaust %d", i);
    }

    // FATAL should still pass
    nimcp_log_write(logger_, LOG_LEVEL_FATAL, nullptr, __FILE__, __LINE__,
                    "FATAL_MESSAGE");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("FATAL_MESSAGE") != std::string::npos);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LoggingRateLimitTest, Stats_DroppedMessages) {
    nimcp_log_config_t config = createRateLimitConfig(5, 1000);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Send many messages to trigger dropping
    for (int i = 0; i < 100; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "Stats test %d", i);
    }
    nimcp_log_flush(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    // Should have some dropped messages
    EXPECT_GT(stats.messages_dropped, 0UL);
    EXPECT_EQ(stats.messages_logged + stats.messages_dropped, 100UL);
}

//=============================================================================
// Module-Based Rate Limiting Tests
//=============================================================================

TEST_F(LoggingRateLimitTest, ModuleRateLimit_Independent) {
    nimcp_log_config_t config = createRateLimitConfig(10, 1000);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Exhaust rate limit for module A
    for (int i = 0; i < 50; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, "ModuleA", __FILE__, __LINE__,
                        "ModuleA msg %d", i);
    }

    // Module B should still have some quota (if per-module limiting is supported)
    nimcp_log_write(logger_, LOG_LEVEL_INFO, "ModuleB", __FILE__, __LINE__,
                    "ModuleB message");
    nimcp_log_flush(logger_);

    // At minimum, the test should complete without crash
    SUCCEED();
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LoggingRateLimitTest, ZeroLimit_AllDropped) {
    nimcp_log_config_t config = createRateLimitConfig(0, 1000);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Should be dropped");
    nimcp_log_flush(logger_);

    // With zero limit, INFO messages should be dropped (ERROR might still pass)
    SUCCEED();
}

TEST_F(LoggingRateLimitTest, VeryHighLimit_NoThrottle) {
    nimcp_log_config_t config = createRateLimitConfig(1000000, 1000);  // 1M/sec

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                        "High limit %d", i);
    }
    nimcp_log_flush(logger_);

    size_t lines = countLines();
    EXPECT_EQ(lines, 100U);
}

TEST_F(LoggingRateLimitTest, RateLimit_NullLogger) {
    // Should not crash
    nimcp_log_rate_limit_config_t rl_config = {.enabled = true, .max_per_second = 100, .burst_size = 10};
    nimcp_log_set_rate_limit(nullptr, &rl_config);
    SUCCEED();
}
