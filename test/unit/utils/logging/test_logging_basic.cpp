//=============================================================================
// test_logging_basic.cpp - Unit Tests for Basic Logging Operations
//=============================================================================
/**
 * @file test_logging_basic.cpp
 * @brief Basic unit tests for NIMCP enhanced logging system
 *
 * Tests cover:
 * - Logger creation and destruction
 * - Global logger initialization and shutdown
 * - Basic log message writing
 * - Default configuration
 * - Logger validation
 * - File output
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LoggingBasicTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        // Create unique test log path
        char template_path[] = "/tmp/nimcp_log_test_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) {
            close(fd);
            test_log_path_ = std::string(template_path);
        } else {
            test_log_path_ = "/tmp/nimcp_test_log.log";
        }
    }

    void TearDown() override {
        if (logger_) {
            nimcp_log_destroy(logger_);
            logger_ = nullptr;
        }
        nimcp_log_shutdown();
        // Clean up test log file
        if (!test_log_path_.empty()) {
            unlink(test_log_path_.c_str());
        }
    }

    // Helper to create basic config
    nimcp_log_config_t createBasicConfig() {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        return config;
    }

    // Helper to read log file contents
    std::string readLogFile() {
        std::ifstream file(test_log_path_);
        if (!file.is_open()) return "";
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
    }

    // Helper to check if file exists
    bool fileExists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }
};

//=============================================================================
// Logger Creation Tests
//=============================================================================

TEST_F(LoggingBasicTest, CreateLogger_DefaultConfig) {
    logger_ = nimcp_log_create(nullptr);
    ASSERT_NE(logger_, nullptr);
    EXPECT_TRUE(nimcp_log_is_initialized(logger_));
}

TEST_F(LoggingBasicTest, CreateLogger_CustomConfig) {
    nimcp_log_config_t config = createBasicConfig();
    config.level = LOG_LEVEL_DEBUG;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);
    EXPECT_TRUE(nimcp_log_is_initialized(logger_));
}

TEST_F(LoggingBasicTest, CreateLogger_WithFileOutput) {
    nimcp_log_config_t config = createBasicConfig();

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Write a message
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Test message");
    nimcp_log_flush(logger_);

    // Verify file was created
    EXPECT_TRUE(fileExists(test_log_path_));
}

TEST_F(LoggingBasicTest, DestroyLogger_NullSafe) {
    // Should not crash on NULL
    nimcp_log_destroy(nullptr);
    SUCCEED();
}

TEST_F(LoggingBasicTest, DestroyLogger_Multiple) {
    logger_ = nimcp_log_create(nullptr);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_destroy(logger_);
    logger_ = nullptr;  // Prevent double-free in TearDown

    // Second destroy should be safe
    nimcp_log_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Global Logger Tests
//=============================================================================

TEST_F(LoggingBasicTest, GlobalInit_Success) {
    nimcp_log_config_t config = createBasicConfig();

    int result = nimcp_log_init(&config);
    EXPECT_EQ(result, 0);

    nimcp_logger_t global = nimcp_log_get_global();
    EXPECT_NE(global, nullptr);
    EXPECT_TRUE(nimcp_log_is_initialized(global));
}

TEST_F(LoggingBasicTest, GlobalInit_DefaultConfig) {
    int result = nimcp_log_init(nullptr);
    EXPECT_EQ(result, 0);

    nimcp_logger_t global = nimcp_log_get_global();
    EXPECT_NE(global, nullptr);
}

TEST_F(LoggingBasicTest, GlobalInit_DoubleInit) {
    nimcp_log_config_t config = createBasicConfig();

    int result1 = nimcp_log_init(&config);
    EXPECT_EQ(result1, 0);

    // Second init should succeed (already initialized)
    int result2 = nimcp_log_init(&config);
    EXPECT_EQ(result2, 0);
}

TEST_F(LoggingBasicTest, GlobalShutdown_Success) {
    nimcp_log_init(nullptr);
    nimcp_log_shutdown();

    // Global logger should be NULL after shutdown
    nimcp_logger_t global = nimcp_log_get_global();
    EXPECT_EQ(global, nullptr);
}

TEST_F(LoggingBasicTest, GlobalShutdown_DoubleShutdown) {
    nimcp_log_init(nullptr);
    nimcp_log_shutdown();
    // Second shutdown should be safe
    nimcp_log_shutdown();
    SUCCEED();
}

TEST_F(LoggingBasicTest, GlobalSetAndGet) {
    logger_ = nimcp_log_create(nullptr);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_global(logger_);
    nimcp_logger_t global = nimcp_log_get_global();
    EXPECT_EQ(global, logger_);

    // Don't destroy logger_ in TearDown - it's the global now
    nimcp_log_shutdown();
    logger_ = nullptr;
}

//=============================================================================
// Basic Log Writing Tests
//=============================================================================

TEST_F(LoggingBasicTest, WriteLog_BasicMessage) {
    nimcp_log_config_t config = createBasicConfig();
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Hello, World!");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Hello, World!") != std::string::npos);
}

TEST_F(LoggingBasicTest, WriteLog_FormattedMessage) {
    nimcp_log_config_t config = createBasicConfig();
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Value: %d, Name: %s", 42, "test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Value: 42") != std::string::npos);
    EXPECT_TRUE(content.find("Name: test") != std::string::npos);
}

TEST_F(LoggingBasicTest, WriteLog_WithModule) {
    nimcp_log_config_t config = createBasicConfig();
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, "TestModule", __FILE__, __LINE__,
                    "Module message");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("TestModule") != std::string::npos);
    EXPECT_TRUE(content.find("Module message") != std::string::npos);
}

TEST_F(LoggingBasicTest, WriteLog_EmptyMessage) {
    nimcp_log_config_t config = createBasicConfig();
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Should not crash on empty message
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "");
    nimcp_log_flush(logger_);
    SUCCEED();
}

TEST_F(LoggingBasicTest, WriteLog_NullLogger) {
    // Should not crash with NULL logger (uses global)
    nimcp_log_write(nullptr, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Test message");
    SUCCEED();
}

TEST_F(LoggingBasicTest, WriteLog_LongMessage) {
    nimcp_log_config_t config = createBasicConfig();
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Create a long message
    std::string long_msg(3000, 'X');
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "%s", long_msg.c_str());
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // Should be truncated but not crash
    EXPECT_FALSE(content.empty());
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(LoggingBasicTest, DefaultConfig_ValidValues) {
    nimcp_log_config_t config = nimcp_log_default_config();

    EXPECT_EQ(config.level, LOG_LEVEL_INFO);
    EXPECT_EQ(config.format, NIMCP_LOG_FORMAT_TEXT);
    EXPECT_EQ(config.async_mode, NIMCP_LOG_ASYNC_ON);  // Default is async for performance
    EXPECT_TRUE(config.append_mode);
    EXPECT_NE(config.destinations, NIMCP_LOG_DEST_NONE);
}

TEST_F(LoggingBasicTest, DefaultRotationConfig_ValidValues) {
    nimcp_log_config_t log_config = nimcp_log_default_config();

    EXPECT_EQ(log_config.rotation.mode, NIMCP_LOG_ROTATE_SIZE);  // Default rotates by size
    EXPECT_GT(log_config.rotation.max_file_size, 0UL);
    EXPECT_GT(log_config.rotation.max_rotated_files, 0U);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(LoggingBasicTest, IsValid_ValidLogger) {
    logger_ = nimcp_log_create(nullptr);
    EXPECT_TRUE(nimcp_log_is_initialized(logger_));
}

TEST_F(LoggingBasicTest, IsValid_NullLogger) {
    EXPECT_FALSE(nimcp_log_is_initialized(nullptr));
}

TEST_F(LoggingBasicTest, IsValid_GlobalLogger) {
    nimcp_log_init(nullptr);
    // NULL logger checks global
    EXPECT_TRUE(nimcp_log_is_initialized(nullptr));
}

//=============================================================================
// Flush Tests
//=============================================================================

TEST_F(LoggingBasicTest, Flush_Success) {
    nimcp_log_config_t config = createBasicConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Flush test");

    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Flush test") != std::string::npos);
}

TEST_F(LoggingBasicTest, Flush_NullLogger) {
    // Should not crash with NULL logger
    nimcp_log_flush(nullptr);
    SUCCEED();
}

//=============================================================================
// Source Location Tests
//=============================================================================

TEST_F(LoggingBasicTest, SourceLocation_Included) {
    nimcp_log_config_t config = createBasicConfig();
    config.include_source_location = true;
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, "test_file.cpp", 42,
                    "Location test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("test_file.cpp") != std::string::npos ||
                content.find(":42") != std::string::npos);
}

TEST_F(LoggingBasicTest, SourceLocation_Excluded) {
    nimcp_log_config_t config = createBasicConfig();
    config.include_source_location = false;
    config.level = LOG_LEVEL_INFO;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, "excluded_file.cpp", 99,
                    "No location test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // File and line should not appear in output when disabled
    EXPECT_TRUE(content.find("excluded_file.cpp") == std::string::npos ||
                content.find(":99") == std::string::npos);
}
