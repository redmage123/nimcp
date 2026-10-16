//=============================================================================
// test_logging_levels.cpp - Unit Tests for Log Level Filtering
//=============================================================================
/**
 * @file test_logging_levels.cpp
 * @brief Unit tests for log level filtering in NIMCP enhanced logging system
 *
 * Tests cover:
 * - Log level thresholds
 * - Runtime level changes
 * - Level-based filtering
 * - All log levels (TRACE through FATAL)
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

class LoggingLevelsTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        char template_path[] = "/tmp/nimcp_log_level_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) {
            close(fd);
            test_log_path_ = std::string(template_path);
        } else {
            test_log_path_ = "/tmp/nimcp_level_test.log";
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

    nimcp_log_config_t createConfig(log_level_t level) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        config.level = level;
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
// Level Threshold Tests
//=============================================================================

TEST_F(LoggingLevelsTest, LevelInfo_FiltersDebugAndTrace) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_INFO);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_TRACE, nullptr, __FILE__, __LINE__, "TRACE_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "ERROR_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("TRACE_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("DEBUG_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("INFO_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("WARN_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("ERROR_MSG") != std::string::npos);
}

TEST_F(LoggingLevelsTest, LevelWarn_FiltersInfoDebugTrace) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_WARN);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_TRACE, nullptr, __FILE__, __LINE__, "TRACE_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "ERROR_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("TRACE_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("DEBUG_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("INFO_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("WARN_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("ERROR_MSG") != std::string::npos);
}

TEST_F(LoggingLevelsTest, LevelError_FiltersWarnInfoDebugTrace) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_ERROR);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_TRACE, nullptr, __FILE__, __LINE__, "TRACE_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "ERROR_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_FATAL, nullptr, __FILE__, __LINE__, "FATAL_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("TRACE_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("DEBUG_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("INFO_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("WARN_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("ERROR_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("FATAL_MSG") != std::string::npos);
}

TEST_F(LoggingLevelsTest, LevelTrace_AllowsAll) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_TRACE);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_TRACE, nullptr, __FILE__, __LINE__, "TRACE_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "ERROR_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_FATAL, nullptr, __FILE__, __LINE__, "FATAL_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("TRACE_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("DEBUG_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("INFO_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("WARN_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("ERROR_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("FATAL_MSG") != std::string::npos);
}

TEST_F(LoggingLevelsTest, LevelOff_FiltersAll) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_OFF);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_TRACE, nullptr, __FILE__, __LINE__, "TRACE_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "ERROR_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_FATAL, nullptr, __FILE__, __LINE__, "FATAL_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.empty() || (
        content.find("TRACE_MSG") == std::string::npos &&
        content.find("DEBUG_MSG") == std::string::npos &&
        content.find("INFO_MSG") == std::string::npos &&
        content.find("WARN_MSG") == std::string::npos &&
        content.find("ERROR_MSG") == std::string::npos &&
        content.find("FATAL_MSG") == std::string::npos));
}

//=============================================================================
// Runtime Level Change Tests
//=============================================================================

TEST_F(LoggingLevelsTest, SetLevel_RuntimeChange) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_ERROR);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Initially at ERROR level - INFO should be filtered
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_BEFORE");
    nimcp_log_flush(logger_);

    std::string content1 = readLogFile();
    EXPECT_TRUE(content1.find("INFO_BEFORE") == std::string::npos);

    // Change to INFO level
    nimcp_log_set_level(logger_, LOG_LEVEL_INFO);

    // Now INFO should pass
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_AFTER");
    nimcp_log_flush(logger_);

    std::string content2 = readLogFile();
    EXPECT_TRUE(content2.find("INFO_AFTER") != std::string::npos);
}

TEST_F(LoggingLevelsTest, GetLevel_ReturnsCurrentLevel) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_WARN);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    log_level_t level = nimcp_log_get_level(logger_);
    EXPECT_EQ(level, LOG_LEVEL_WARN);

    nimcp_log_set_level(logger_, LOG_LEVEL_DEBUG);
    level = nimcp_log_get_level(logger_);
    EXPECT_EQ(level, LOG_LEVEL_DEBUG);
}

//=============================================================================
// Level String Conversion Tests
//=============================================================================

TEST_F(LoggingLevelsTest, LevelToString_AllLevels) {
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_TRACE), "TRACE");
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_INFO), "INFO");
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_WARN), "WARN");
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_FATAL), "FATAL");
    EXPECT_STREQ(nimcp_log_level_name(LOG_LEVEL_OFF), "OFF");
}

TEST_F(LoggingLevelsTest, LevelFromString_AllLevels) {
    EXPECT_EQ(nimcp_log_level_from_string("TRACE"), LOG_LEVEL_TRACE);
    EXPECT_EQ(nimcp_log_level_from_string("trace"), LOG_LEVEL_TRACE);
    EXPECT_EQ(nimcp_log_level_from_string("DEBUG"), LOG_LEVEL_DEBUG);
    EXPECT_EQ(nimcp_log_level_from_string("debug"), LOG_LEVEL_DEBUG);
    EXPECT_EQ(nimcp_log_level_from_string("INFO"), LOG_LEVEL_INFO);
    EXPECT_EQ(nimcp_log_level_from_string("info"), LOG_LEVEL_INFO);
    EXPECT_EQ(nimcp_log_level_from_string("WARN"), LOG_LEVEL_WARN);
    EXPECT_EQ(nimcp_log_level_from_string("warn"), LOG_LEVEL_WARN);
    EXPECT_EQ(nimcp_log_level_from_string("WARNING"), LOG_LEVEL_WARN);
    EXPECT_EQ(nimcp_log_level_from_string("ERROR"), LOG_LEVEL_ERROR);
    EXPECT_EQ(nimcp_log_level_from_string("error"), LOG_LEVEL_ERROR);
    EXPECT_EQ(nimcp_log_level_from_string("FATAL"), LOG_LEVEL_FATAL);
    EXPECT_EQ(nimcp_log_level_from_string("fatal"), LOG_LEVEL_FATAL);
    EXPECT_EQ(nimcp_log_level_from_string("OFF"), LOG_LEVEL_OFF);
    EXPECT_EQ(nimcp_log_level_from_string("off"), LOG_LEVEL_OFF);
}

TEST_F(LoggingLevelsTest, LevelFromString_Invalid) {
    EXPECT_EQ(nimcp_log_level_from_string("INVALID"), LOG_LEVEL_INFO);  // Default
    EXPECT_EQ(nimcp_log_level_from_string(nullptr), LOG_LEVEL_INFO);   // Default
    EXPECT_EQ(nimcp_log_level_from_string(""), LOG_LEVEL_INFO);        // Default
}

//=============================================================================
// Level Output Format Tests
//=============================================================================

TEST_F(LoggingLevelsTest, LevelAppearsInOutput_Info) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_INFO);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Test message");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("INFO") != std::string::npos);
}

TEST_F(LoggingLevelsTest, LevelAppearsInOutput_Error) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_ERROR);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "Error message");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("ERROR") != std::string::npos);
}

//=============================================================================
// Module Level Override Tests
//=============================================================================

TEST_F(LoggingLevelsTest, ModuleLevel_Override) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_WARN);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Set DEBUG level for "TestModule" only
    nimcp_log_enable_module(logger_, "TestModule", LOG_LEVEL_DEBUG);

    // Module messages at DEBUG should pass
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, "TestModule", __FILE__, __LINE__, "MODULE_DEBUG");
    // Non-module DEBUG should be filtered
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "GENERAL_DEBUG");
    // Other module DEBUG should be filtered
    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, "OtherModule", __FILE__, __LINE__, "OTHER_DEBUG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("MODULE_DEBUG") != std::string::npos);
    EXPECT_TRUE(content.find("GENERAL_DEBUG") == std::string::npos);
    EXPECT_TRUE(content.find("OTHER_DEBUG") == std::string::npos);
}

TEST_F(LoggingLevelsTest, ModuleLevel_RemoveOverride) {
    nimcp_log_config_t config = createConfig(LOG_LEVEL_WARN);
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Set and then remove override
    nimcp_log_enable_module(logger_, "TestModule", LOG_LEVEL_DEBUG);
    nimcp_log_clear_module_filters(logger_);

    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, "TestModule", __FILE__, __LINE__, "AFTER_CLEAR");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("AFTER_CLEAR") == std::string::npos);
}
