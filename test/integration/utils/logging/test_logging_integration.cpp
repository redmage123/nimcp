//=============================================================================
// test_logging_integration.cpp - Integration Tests for Logging System
//=============================================================================
/**
 * @file test_logging_integration.cpp
 * @brief Integration tests for NIMCP enhanced logging system
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <fstream>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>
#include <glob.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

class LoggingIntegrationTest : public ::testing::Test {
protected:
    std::string test_dir_;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_int_log_XXXXXX";
        char* dir = mkdtemp(template_path);
        test_dir_ = dir ? dir : "/tmp/nimcp_int_log_test";
        if (!dir) mkdir(test_dir_.c_str(), 0755);
        test_log_path_ = test_dir_ + "/test.log";
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

    size_t countLogLines() {
        std::ifstream f(test_log_path_);
        size_t n = 0;
        std::string line;
        while (std::getline(f, line)) if (!line.empty()) n++;
        return n;
    }
};

TEST_F(LoggingIntegrationTest, FullWorkflow_SyncLogging) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_OFF;
    config.level = LOG_LEVEL_DEBUG;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARN("Warning message");
    LOG_ERROR("Error message");
    nimcp_log_flush(nullptr);

    EXPECT_EQ(countLogLines(), 4U);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);
    EXPECT_EQ(stats.messages_logged, 4UL);
}

TEST_F(LoggingIntegrationTest, FullWorkflow_AsyncLogging) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 4096;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    for (int i = 0; i < 1000; i++) {
        LOG_INFO("Async message %d", i);
    }
    nimcp_log_flush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t lines = countLogLines();
    EXPECT_GE(lines, 950U);
}

TEST_F(LoggingIntegrationTest, FullWorkflow_WithRotation) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_OFF;
    config.level = LOG_LEVEL_INFO;
    config.rotation.mode = NIMCP_LOG_ROTATE_SIZE;
    config.rotation.max_file_size = 1024;
    config.rotation.max_rotated_files = 3;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    for (int i = 0; i < 100; i++) {
        LOG_INFO("Rotation test message number %d with padding", i);
    }
    nimcp_log_flush(nullptr);

    glob_t g;
    std::string pattern = test_log_path_ + "*";
    ASSERT_EQ(glob(pattern.c_str(), 0, nullptr, &g), 0);
    EXPECT_GE(g.gl_pathc, 2U);
    globfree(&g);
}

TEST_F(LoggingIntegrationTest, FullWorkflow_JsonFormat) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.format = NIMCP_LOG_FORMAT_JSON;
    config.level = LOG_LEVEL_INFO;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    LOG_INFO("JSON test message");
    LOG_MODULE_INFO("TestModule", "Module JSON message");
    nimcp_log_flush(nullptr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("{") != std::string::npos);
    EXPECT_TRUE(content.find("\"level\"") != std::string::npos);
}

TEST_F(LoggingIntegrationTest, MultiThreaded_Production) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_DEBUG;
    config.buffer_size = 16384;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < 100; i++) {
                LOG_MODULE_INFO("Worker", "Thread %d iteration %d", t, i);
            }
        });
    }

    for (auto& t : threads) t.join();
    nimcp_log_flush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GE(countLogLines(), 750U);
}

TEST_F(LoggingIntegrationTest, ContextTracking_Integration) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.level = LOG_LEVEL_INFO;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    // Set context for subsequent log writes
    nimcp_log_set_context_id(nullptr, "REQ-12345");

    nimcp_log_write(nullptr, LOG_LEVEL_INFO, "Handler", __FILE__, __LINE__,
                    "Processing request");
    nimcp_log_write(nullptr, LOG_LEVEL_INFO, "Handler", __FILE__, __LINE__,
                    "Request completed");
    nimcp_log_flush(nullptr);

    // Clear context
    nimcp_log_set_context_id(nullptr, nullptr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Processing request") != std::string::npos);
}

TEST_F(LoggingIntegrationTest, RateLimit_Integration) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.level = LOG_LEVEL_INFO;
    config.rate_limit.enabled = true;
    config.rate_limit.max_per_second = 10;
    config.rate_limit.burst_size = 20;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    for (int i = 0; i < 100; i++) {
        LOG_INFO("Rate limited message %d", i);
    }
    nimcp_log_flush(nullptr);

    nimcp_log_stats_t stats;
    nimcp_log_get_stats(nullptr, &stats);
    EXPECT_LT(stats.messages_logged, 50UL);
    EXPECT_GT(stats.messages_dropped, 0UL);
}
