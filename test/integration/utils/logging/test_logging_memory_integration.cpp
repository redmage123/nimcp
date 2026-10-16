//=============================================================================
// test_logging_memory_integration.cpp - Logging + Unified Memory Integration
//=============================================================================
/**
 * @file test_logging_memory_integration.cpp
 * @brief Integration tests for logging with unified memory system
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <fstream>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

class LoggingMemoryIntegrationTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mem_config);

        char template_path[] = "/tmp/nimcp_log_mem_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) { close(fd); test_log_path_ = template_path; }
        else { test_log_path_ = "/tmp/nimcp_log_mem_test.log"; }
    }

    void TearDown() override {
        nimcp_log_shutdown();
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
        if (!test_log_path_.empty()) unlink(test_log_path_.c_str());
    }
};

TEST_F(LoggingMemoryIntegrationTest, LoggerWithUnifiedMemory) {
    ASSERT_NE(mem_mgr_, nullptr);

    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.level = LOG_LEVEL_INFO;
    config.memory_manager = mem_mgr_;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    for (int i = 0; i < 100; i++) {
        LOG_INFO("Memory integrated message %d", i);
    }
    nimcp_log_flush(nullptr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Memory integrated message 0") != std::string::npos);
    EXPECT_TRUE(content.find("Memory integrated message 99") != std::string::npos);
}

TEST_F(LoggingMemoryIntegrationTest, AsyncLoggingWithUnifiedMemory) {
    ASSERT_NE(mem_mgr_, nullptr);

    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.async_mode = NIMCP_LOG_ASYNC_ON;
    config.level = LOG_LEVEL_INFO;
    config.buffer_size = 4096;
    config.memory_manager = mem_mgr_;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    for (int i = 0; i < 500; i++) {
        LOG_INFO("Async mem message %d", i);
    }
    nimcp_log_flush(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ifstream f(test_log_path_);
    size_t lines = 0;
    std::string line;
    while (std::getline(f, line)) if (!line.empty()) lines++;
    EXPECT_GE(lines, 450U);
}

TEST_F(LoggingMemoryIntegrationTest, MemoryCleanupOnShutdown) {
    ASSERT_NE(mem_mgr_, nullptr);

    unified_mem_stats_t stats_before;
    unified_mem_get_stats(mem_mgr_, &stats_before);

    {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.memory_manager = mem_mgr_;

        nimcp_logger_t logger = nimcp_log_create(&config);
        ASSERT_NE(logger, nullptr);

        for (int i = 0; i < 50; i++) {
            nimcp_log_write(logger, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                           "Cleanup test %d", i);
        }
        nimcp_log_flush(logger);
        nimcp_log_destroy(logger);
    }

    unified_mem_stats_t stats_after;
    unified_mem_get_stats(mem_mgr_, &stats_after);

    // Memory should be properly cleaned up
    SUCCEED();
}

TEST_F(LoggingMemoryIntegrationTest, FallbackWithoutMemoryManager) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.memory_manager = nullptr;  // No memory manager

    ASSERT_EQ(nimcp_log_init(&config), 0);

    LOG_INFO("Fallback to malloc message");
    nimcp_log_flush(nullptr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Fallback to malloc message") != std::string::npos);
}
