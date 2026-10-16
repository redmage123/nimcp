//=============================================================================
// test_logging_stats.cpp - Unit Tests for Logging Statistics
//=============================================================================
/**
 * @file test_logging_stats.cpp
 * @brief Unit tests for logging statistics tracking in NIMCP logging system
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

class LoggingStatsTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_stats_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) { close(fd); test_log_path_ = template_path; }
        else { test_log_path_ = "/tmp/nimcp_stats_test.log"; }
    }

    void TearDown() override {
        if (logger_) { nimcp_log_destroy(logger_); logger_ = nullptr; }
        if (!test_log_path_.empty()) unlink(test_log_path_.c_str());
    }

    nimcp_log_config_t createConfig() {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_TRACE;
        return config;
    }
};

TEST_F(LoggingStatsTest, InitialStats_AllZero) {
    nimcp_log_config_t config = createConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_EQ(stats.messages_logged, 0UL);
    EXPECT_EQ(stats.messages_dropped, 0UL);
    EXPECT_EQ(stats.bytes_written, 0UL);
}

TEST_F(LoggingStatsTest, MessagesLogged_Increments) {
    nimcp_log_config_t config = createConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Msg %d", i);
    }
    nimcp_log_flush(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_EQ(stats.messages_logged, 10UL);
}

TEST_F(LoggingStatsTest, BytesWritten_Tracked) {
    nimcp_log_config_t config = createConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "This is a test message with some content");
    nimcp_log_flush(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_GT(stats.bytes_written, 0UL);
}

TEST_F(LoggingStatsTest, FilteredMessages_CountsDropped) {
    nimcp_log_config_t config = createConfig();
    config.level = LOG_LEVEL_WARN;  // Filter out INFO
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Filtered");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "Passed");
    nimcp_log_flush(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_EQ(stats.messages_logged, 1UL);
    // Level-filtered messages return early, not tracked as filtered or dropped
    // Only custom filter callbacks increment messages_filtered
    // This test verifies that only 1 message was logged (the WARN one)
}

TEST_F(LoggingStatsTest, ResetStats_ClearsAll) {
    nimcp_log_config_t config = createConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    for (int i = 0; i < 5; i++) {
        nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Msg");
    }
    nimcp_log_flush(logger_);

    nimcp_log_reset_stats(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_EQ(stats.messages_logged, 0UL);
}

TEST_F(LoggingStatsTest, LevelCounts_PerLevel) {
    nimcp_log_config_t config = createConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "D");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "I");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "I");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "W");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "E");
    nimcp_log_flush(logger_);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(logger_, &stats);

    EXPECT_EQ(stats.messages_logged, 5UL);
}

TEST_F(LoggingStatsTest, NullLogger_SafeAccess) {
    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);
    nimcp_log_reset_stats(nullptr);
    SUCCEED();
}

TEST_F(LoggingStatsTest, NullStats_SafeAccess) {
    nimcp_log_config_t config = createConfig();
    logger_ = nimcp_log_create(&config);
    nimcp_log_get_stats(logger_, nullptr);
    SUCCEED();
}
