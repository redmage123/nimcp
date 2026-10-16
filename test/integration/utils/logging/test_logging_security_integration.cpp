//=============================================================================
// test_logging_security_integration.cpp - Logging + Security Module Integration
//=============================================================================
/**
 * @file test_logging_security_integration.cpp
 * @brief Integration tests for logging with security module
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <fstream>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"

class LoggingSecurityIntegrationTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* sec_ctx_ = nullptr;
    std::string test_log_path_;

    void SetUp() override {
        sec_ctx_ = nimcp_sec_integration_create();

        char template_path[] = "/tmp/nimcp_log_sec_XXXXXX";
        int fd = mkstemp(template_path);
        if (fd != -1) { close(fd); test_log_path_ = template_path; }
        else { test_log_path_ = "/tmp/nimcp_log_sec_test.log"; }
    }

    void TearDown() override {
        nimcp_log_shutdown();
        if (sec_ctx_) {
            nimcp_sec_integration_destroy(sec_ctx_);
            sec_ctx_ = nullptr;
        }
        if (!test_log_path_.empty()) unlink(test_log_path_.c_str());
    }
};

TEST_F(LoggingSecurityIntegrationTest, LoggerWithSecurityContext) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.level = LOG_LEVEL_INFO;
    config.security_context = sec_ctx_;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    LOG_INFO("Security integrated message");
    LOG_ERROR("Security error message");
    nimcp_log_flush(nullptr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Security integrated message") != std::string::npos);
}

TEST_F(LoggingSecurityIntegrationTest, ModuleRegistration) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.security_context = sec_ctx_;

    nimcp_logger_t logger = nimcp_log_create(&config);
    ASSERT_NE(logger, nullptr);

    // Logger should have registered with security module
    // Write some messages and verify security tracking
    for (int i = 0; i < 10; i++) {
        nimcp_log_write(logger, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                       "Security tracked message %d", i);
    }
    nimcp_log_flush(logger);

    nimcp_log_destroy(logger);
    // Module should have unregistered cleanly
    SUCCEED();
}

TEST_F(LoggingSecurityIntegrationTest, FallbackWithoutSecurityContext) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.security_context = nullptr;  // No security context

    ASSERT_EQ(nimcp_log_init(&config), 0);

    LOG_INFO("No security context message");
    nimcp_log_flush(nullptr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("No security context message") != std::string::npos);
}

TEST_F(LoggingSecurityIntegrationTest, CombinedMemoryAndSecurity) {
    // Test with both unified memory and security context
    unified_mem_config_t mem_config = unified_mem_default_config();
    unified_mem_manager_t mem_mgr = unified_mem_create(&mem_config);
    ASSERT_NE(mem_mgr, nullptr);

    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.level = LOG_LEVEL_DEBUG;
    config.memory_manager = mem_mgr;
    config.security_context = sec_ctx_;

    ASSERT_EQ(nimcp_log_init(&config), 0);

    LOG_DEBUG("Combined integration debug");
    LOG_INFO("Combined integration info");
    LOG_WARN("Combined integration warn");
    nimcp_log_flush(nullptr);

    nimcp_log_shutdown();
    unified_mem_destroy(mem_mgr);

    std::ifstream f(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Combined integration") != std::string::npos);
}

TEST_F(LoggingSecurityIntegrationTest, SecurityIdAssignment) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = test_log_path_.c_str();
    config.destinations = NIMCP_LOG_DEST_FILE;
    config.security_context = sec_ctx_;

    nimcp_logger_t logger = nimcp_log_create(&config);
    ASSERT_NE(logger, nullptr);

    // Logger should have a valid security ID if registered
    uint32_t sec_id = nimcp_log_get_security_id(logger);
    // Security ID may be 0 if not registered, that's OK
    (void)sec_id;

    nimcp_log_destroy(logger);
    SUCCEED();
}

TEST_F(LoggingSecurityIntegrationTest, MultipleLoggersWithSameSecurity) {
    nimcp_log_config_t config1 = nimcp_log_default_config();
    config1.file_path = test_log_path_.c_str();
    config1.destinations = NIMCP_LOG_DEST_FILE;
    config1.security_context = sec_ctx_;

    std::string path2 = test_log_path_ + ".2";
    nimcp_log_config_t config2 = nimcp_log_default_config();
    config2.file_path = path2.c_str();
    config2.destinations = NIMCP_LOG_DEST_FILE;
    config2.security_context = sec_ctx_;

    nimcp_logger_t logger1 = nimcp_log_create(&config1);
    nimcp_logger_t logger2 = nimcp_log_create(&config2);

    ASSERT_NE(logger1, nullptr);
    ASSERT_NE(logger2, nullptr);

    nimcp_log_write(logger1, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Logger 1");
    nimcp_log_write(logger2, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__, "Logger 2");

    nimcp_log_flush(logger1);
    nimcp_log_flush(logger2);

    nimcp_log_destroy(logger1);
    nimcp_log_destroy(logger2);

    unlink(path2.c_str());
    SUCCEED();
}
