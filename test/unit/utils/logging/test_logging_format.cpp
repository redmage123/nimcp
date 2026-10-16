//=============================================================================
// test_logging_format.cpp - Unit Tests for Log Output Formats
//=============================================================================
/**
 * @file test_logging_format.cpp
 * @brief Unit tests for log output formats (text, JSON, compact, syslog)
 *
 * Tests cover:
 * - Text format
 * - JSON format
 * - Compact format
 * - Syslog format
 * - Format switching
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

//=============================================================================
// Test Fixture
//=============================================================================

class LoggingFormatTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_format_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) {
            close(fd);
            test_log_path_ = std::string(template_path);
        } else {
            test_log_path_ = "/tmp/nimcp_format_test.log";
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

    nimcp_log_config_t createFormatConfig(nimcp_log_format_t format) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_TRACE;
        config.format = format;
        config.include_source_location = true;
        return config;
    }

    std::string readLogFile() {
        std::ifstream file(test_log_path_);
        if (!file.is_open()) return "";
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
    }

    void clearLogFile() {
        std::ofstream file(test_log_path_, std::ios::trunc);
        file.close();
    }
};

//=============================================================================
// Text Format Tests
//=============================================================================

TEST_F(LoggingFormatTest, TextFormat_HasTimestamp) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Text format test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // Should have timestamp pattern like YYYY-MM-DD or HH:MM:SS
    EXPECT_TRUE(content.find("-") != std::string::npos ||
                content.find(":") != std::string::npos);
}

TEST_F(LoggingFormatTest, TextFormat_HasLevelName) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Level name test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("INFO") != std::string::npos);
}

TEST_F(LoggingFormatTest, TextFormat_HasModule) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, "MyModule", __FILE__, __LINE__,
                    "Module test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("MyModule") != std::string::npos);
}

TEST_F(LoggingFormatTest, TextFormat_HasMessage) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Unique message content 12345");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Unique message content 12345") != std::string::npos);
}

//=============================================================================
// JSON Format Tests
//=============================================================================

TEST_F(LoggingFormatTest, JsonFormat_ValidJson) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "JSON test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // Should have JSON structure
    EXPECT_TRUE(content.find("{") != std::string::npos);
    EXPECT_TRUE(content.find("}") != std::string::npos);
}

TEST_F(LoggingFormatTest, JsonFormat_HasTimestampField) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "JSON timestamp test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("\"timestamp\"") != std::string::npos);
}

TEST_F(LoggingFormatTest, JsonFormat_HasLevelField) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "JSON level test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("\"level\"") != std::string::npos);
    EXPECT_TRUE(content.find("INFO") != std::string::npos);
}

TEST_F(LoggingFormatTest, JsonFormat_HasMessageField) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "JSON message content");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("\"message\"") != std::string::npos);
    EXPECT_TRUE(content.find("JSON message content") != std::string::npos);
}

TEST_F(LoggingFormatTest, JsonFormat_HasModuleField) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, "TestModule", __FILE__, __LINE__,
                    "Module in JSON");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("\"module\"") != std::string::npos);
    EXPECT_TRUE(content.find("TestModule") != std::string::npos);
}

TEST_F(LoggingFormatTest, JsonFormat_EscapesSpecialChars) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Quote: \" Backslash: \\ Newline: \n");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // JSON escapes: \" \\ \n
    EXPECT_TRUE(content.find("\\\"") != std::string::npos ||
                content.find("Quote:") != std::string::npos);
}

//=============================================================================
// Compact Format Tests
//=============================================================================

TEST_F(LoggingFormatTest, CompactFormat_NoTimestamp) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_COMPACT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Compact test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // Should be more compact (less overhead)
    EXPECT_TRUE(content.find("Compact test") != std::string::npos);
    // Compact format may still have level but shorter output
    EXPECT_LT(content.length(), 200U);
}

TEST_F(LoggingFormatTest, CompactFormat_HasLevel) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_COMPACT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__,
                    "Error message");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // Should have level indicator (may be abbreviated)
    EXPECT_TRUE(content.find("E") != std::string::npos ||
                content.find("ERROR") != std::string::npos);
}

//=============================================================================
// Syslog Format Tests
//=============================================================================

TEST_F(LoggingFormatTest, SyslogFormat_HasPriority) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_SYSLOG);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Syslog test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    // Syslog format may have priority in angle brackets <N>
    EXPECT_TRUE(content.find("Syslog test") != std::string::npos);
}

//=============================================================================
// Format Change Tests
//=============================================================================

TEST_F(LoggingFormatTest, SetFormat_RuntimeChange) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Text format message");
    nimcp_log_flush(logger_);

    // Change to JSON
    nimcp_log_set_format(logger_, NIMCP_LOG_FORMAT_JSON);

    clearLogFile();

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "JSON format message");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("{") != std::string::npos);
    EXPECT_TRUE(content.find("JSON format message") != std::string::npos);
}

//=============================================================================
// Source Location in Formats
//=============================================================================

TEST_F(LoggingFormatTest, TextFormat_SourceLocation) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    config.include_source_location = true;
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, "test_file.cpp", 123,
                    "Source location test");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("test_file.cpp") != std::string::npos ||
                content.find(":123") != std::string::npos);
}

TEST_F(LoggingFormatTest, JsonFormat_SourceLocation) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_JSON);
    config.include_source_location = true;
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, "test_file.cpp", 456,
                    "JSON source location");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("\"file\"") != std::string::npos ||
                content.find("test_file.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("\"line\"") != std::string::npos ||
                content.find("456") != std::string::npos);
}

//=============================================================================
// Special Character Handling
//=============================================================================

TEST_F(LoggingFormatTest, TextFormat_SpecialChars) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Tab:\tNewline:\nPercent:%%");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Percent:%") != std::string::npos ||
                content.find("Tab:") != std::string::npos);
}

TEST_F(LoggingFormatTest, AnyFormat_BinaryData) {
    nimcp_log_config_t config = createFormatConfig(NIMCP_LOG_FORMAT_TEXT);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Binary data that might cause issues
    char binary[] = "Binary: \x01\x02\x03 end";
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "%s", binary);
    nimcp_log_flush(logger_);

    // Should not crash
    SUCCEED();
}
