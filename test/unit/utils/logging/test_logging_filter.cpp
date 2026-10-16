//=============================================================================
// test_logging_filter.cpp - Unit Tests for Log Filtering
//=============================================================================
/**
 * @file test_logging_filter.cpp
 * @brief Unit tests for custom log filtering in NIMCP logging system
 *
 * Tests cover:
 * - Filter callback
 * - Module filtering
 * - Pattern-based filtering
 * - Filter chaining
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <atomic>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Filter Callback Helpers
//=============================================================================

static std::atomic<int> g_filter_call_count{0};
static std::string g_last_filtered_module;
static std::string g_last_filtered_message;

static bool test_allow_all_filter(log_level_t level, const char* module,
                                   const char* message, void* user_data) {
    g_filter_call_count++;
    return true;
}

static bool test_block_all_filter(log_level_t level, const char* module,
                                   const char* message, void* user_data) {
    g_filter_call_count++;
    return false;
}

static bool test_block_debug_filter(log_level_t level, const char* module,
                                     const char* message, void* user_data) {
    g_filter_call_count++;
    return level != LOG_LEVEL_DEBUG;
}

static bool test_module_filter(log_level_t level, const char* module,
                                const char* message, void* user_data) {
    g_filter_call_count++;
    if (module) {
        g_last_filtered_module = module;
    }
    // Only allow "AllowedModule"
    return module && strcmp(module, "AllowedModule") == 0;
}

static bool test_keyword_filter(log_level_t level, const char* module,
                                 const char* message, void* user_data) {
    g_filter_call_count++;
    if (message) {
        g_last_filtered_message = message;
        // Block messages containing "SECRET"
        return strstr(message, "SECRET") == nullptr;
    }
    return true;
}

static bool test_user_data_filter(log_level_t level, const char* module,
                                   const char* message, void* user_data) {
    g_filter_call_count++;
    int* threshold = (int*)user_data;
    return threshold && level >= *threshold;
}

//=============================================================================
// Test Fixture
//=============================================================================

class LoggingFilterTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        g_filter_call_count = 0;
        g_last_filtered_module.clear();
        g_last_filtered_message.clear();

        char template_path[] = "/tmp/nimcp_log_filter_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) {
            close(fd);
            test_log_path_ = std::string(template_path);
        } else {
            test_log_path_ = "/tmp/nimcp_filter_test.log";
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

    nimcp_log_config_t createFilterConfig() {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_TRACE;
        return config;
    }

    std::string readLogFile() {
        std::ifstream file(test_log_path_);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
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
};

//=============================================================================
// Basic Filter Tests
//=============================================================================

TEST_F(LoggingFilterTest, NoFilter_AllPass) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Message 1");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Message 2");
    nimcp_log_flush(logger_);

    EXPECT_EQ(countLines(), 2U);
}

TEST_F(LoggingFilterTest, AllowAllFilter_CallbackInvoked) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_allow_all_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Test");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "Test");
    nimcp_log_flush(logger_);

    EXPECT_EQ(g_filter_call_count, 2);
    EXPECT_EQ(countLines(), 2U);
}

TEST_F(LoggingFilterTest, BlockAllFilter_NonePass) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_block_all_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Blocked 1");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "Blocked 2");
    nimcp_log_flush(logger_);

    EXPECT_EQ(g_filter_call_count, 2);
    EXPECT_EQ(countLines(), 0U);
}

//=============================================================================
// Level-Based Filter Tests
//=============================================================================

TEST_F(LoggingFilterTest, BlockDebugFilter_OnlyDebugBlocked) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_block_debug_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("DEBUG_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("INFO_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("WARN_MSG") != std::string::npos);
}

//=============================================================================
// Module Filter Tests
//=============================================================================

TEST_F(LoggingFilterTest, ModuleFilter_OnlyAllowedPass) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_module_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, "AllowedModule", __FILE__, __LINE__,
                    "ALLOWED_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, "BlockedModule", __FILE__, __LINE__,
                    "BLOCKED_MSG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "NO_MODULE_MSG");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("ALLOWED_MSG") != std::string::npos);
    EXPECT_TRUE(content.find("BLOCKED_MSG") == std::string::npos);
    EXPECT_TRUE(content.find("NO_MODULE_MSG") == std::string::npos);
}

//=============================================================================
// Keyword Filter Tests
//=============================================================================

TEST_F(LoggingFilterTest, KeywordFilter_BlocksSecret) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_keyword_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Normal message");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "This contains SECRET data");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Another normal message");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Normal message") != std::string::npos);
    EXPECT_TRUE(content.find("SECRET") == std::string::npos);
    EXPECT_TRUE(content.find("Another normal message") != std::string::npos);
}

//=============================================================================
// User Data Tests
//=============================================================================

TEST_F(LoggingFilterTest, UserDataFilter_UsesThreshold) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    int threshold = LOG_LEVEL_WARN;
    nimcp_log_set_filter(logger_, test_user_data_filter, &threshold);

    nimcp_log_write(logger_, LOG_LEVEL_DEBUG, nullptr, __FILE__, __LINE__, "DEBUG");
    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "INFO");
    nimcp_log_write(logger_, LOG_LEVEL_WARN, nullptr, __FILE__, __LINE__, "WARN");
    nimcp_log_write(logger_, LOG_LEVEL_ERROR, nullptr, __FILE__, __LINE__, "ERROR");
    nimcp_log_flush(logger_);

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("DEBUG") == std::string::npos);
    EXPECT_TRUE(content.find("INFO") == std::string::npos);
    EXPECT_TRUE(content.find("WARN") != std::string::npos);
    EXPECT_TRUE(content.find("ERROR") != std::string::npos);
}

//=============================================================================
// Clear Filter Tests
//=============================================================================

TEST_F(LoggingFilterTest, ClearFilter_RemovesFilter) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_block_all_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "BLOCKED");
    nimcp_log_flush(logger_);
    EXPECT_EQ(countLines(), 0U);

    // Clear filter
    nimcp_log_set_filter(logger_, nullptr, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "PASSED");
    nimcp_log_flush(logger_);
    EXPECT_EQ(countLines(), 1U);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LoggingFilterTest, FilterCallback_NullLogger) {
    // Should not crash
    nimcp_log_set_filter(nullptr, test_allow_all_filter, nullptr);
    SUCCEED();
}

TEST_F(LoggingFilterTest, FilterCallback_EmptyMessage) {
    nimcp_log_config_t config = createFilterConfig();
    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_set_filter(logger_, test_keyword_filter, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "");
    nimcp_log_flush(logger_);

    // Should not crash
    SUCCEED();
}
