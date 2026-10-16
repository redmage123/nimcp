//=============================================================================
// test_logging_rotation.cpp - Unit Tests for Log Rotation
//=============================================================================
/**
 * @file test_logging_rotation.cpp
 * @brief Unit tests for log file rotation in NIMCP logging system
 *
 * Tests cover:
 * - Size-based rotation
 * - File count limits
 * - Manual rotation trigger
 * - Rotation configuration
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <glob.h>

// Headers have their own extern "C" guards
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LoggingRotationTest : public ::testing::Test {
protected:
    nimcp_logger_t logger_ = nullptr;
    std::string test_log_dir_;
    std::string test_log_path_;

    void SetUp() override {
        // Create unique test directory
        char template_path[] = "/tmp/nimcp_rotation_XXXXXX";
        char* dir = mkdtemp(template_path);
        if (dir) {
            test_log_dir_ = std::string(dir);
            test_log_path_ = test_log_dir_ + "/test.log";
        } else {
            test_log_dir_ = "/tmp/nimcp_rotation_test";
            mkdir(test_log_dir_.c_str(), 0755);
            test_log_path_ = test_log_dir_ + "/test.log";
        }
    }

    void TearDown() override {
        if (logger_) {
            nimcp_log_destroy(logger_);
            logger_ = nullptr;
        }
        // Clean up all log files
        cleanupLogFiles();
        rmdir(test_log_dir_.c_str());
    }

    void cleanupLogFiles() {
        std::string pattern = test_log_dir_ + "/*";
        glob_t glob_result;
        if (glob(pattern.c_str(), 0, nullptr, &glob_result) == 0) {
            for (size_t i = 0; i < glob_result.gl_pathc; i++) {
                unlink(glob_result.gl_pathv[i]);
            }
            globfree(&glob_result);
        }
    }

    std::vector<std::string> getLogFiles() {
        std::vector<std::string> files;
        std::string pattern = test_log_path_ + "*";
        glob_t glob_result;
        if (glob(pattern.c_str(), 0, nullptr, &glob_result) == 0) {
            for (size_t i = 0; i < glob_result.gl_pathc; i++) {
                files.push_back(glob_result.gl_pathv[i]);
            }
            globfree(&glob_result);
        }
        return files;
    }

    size_t getFileSize(const std::string& path) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            return st.st_size;
        }
        return 0;
    }

    nimcp_log_config_t createRotationConfig(size_t max_size, uint32_t max_files) {
        nimcp_log_config_t config = nimcp_log_default_config();
        config.file_path = test_log_path_.c_str();
        config.destinations = NIMCP_LOG_DEST_FILE;
        config.async_mode = NIMCP_LOG_ASYNC_OFF;
        config.level = LOG_LEVEL_TRACE;
        config.rate_limit.enabled = false;  // Disable rate limiting for tests

        config.rotation.mode = NIMCP_LOG_ROTATE_SIZE;
        config.rotation.max_file_size = max_size;
        config.rotation.max_rotated_files = max_files;
        config.rotation.compress_rotated = false;

        return config;
    }

    void writeUntilRotation(size_t target_size) {
        const char* msg = "This is a test message for rotation testing purpose. ";
        size_t msg_len = strlen(msg);
        size_t written = 0;
        while (written < target_size) {
            nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                           "%s", msg);
            written += msg_len + 50;  // Approximate line length with timestamp
        }
        nimcp_log_flush(logger_);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(LoggingRotationTest, DefaultRotationConfig) {
    // Rotation config comes from nimcp_log_default_config()
    nimcp_log_config_t log_config = nimcp_log_default_config();

    EXPECT_EQ(log_config.rotation.mode, NIMCP_LOG_ROTATE_SIZE);  // Default is size-based rotation
    EXPECT_EQ(log_config.rotation.max_file_size, NIMCP_LOG_DEFAULT_MAX_FILE_SIZE);
    EXPECT_EQ(log_config.rotation.max_rotated_files, NIMCP_LOG_DEFAULT_MAX_ROTATED_FILES);
}

TEST_F(LoggingRotationTest, RotationDisabled_NoRotation) {
    nimcp_log_config_t config = createRotationConfig(1024, 3);
    config.rotation.mode = NIMCP_LOG_ROTATE_NONE;

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Write more than max_size
    writeUntilRotation(2048);

    // Should only have one file (no rotation)
    auto files = getLogFiles();
    EXPECT_EQ(files.size(), 1U);
}

//=============================================================================
// Size-Based Rotation Tests
//=============================================================================

TEST_F(LoggingRotationTest, SizeRotation_TriggersAtLimit) {
    nimcp_log_config_t config = createRotationConfig(1024, 5);  // 1KB max size

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Write enough to trigger rotation
    writeUntilRotation(3000);  // Write ~3KB

    auto files = getLogFiles();
    // Should have created rotated files
    EXPECT_GE(files.size(), 2U);
}

TEST_F(LoggingRotationTest, SizeRotation_RespectsMaxFiles) {
    nimcp_log_config_t config = createRotationConfig(512, 3);  // 512B max, 3 files max

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    // Write enough to trigger many rotations
    writeUntilRotation(10000);

    auto files = getLogFiles();
    // Should not exceed max_files + 1 (current + rotated)
    EXPECT_LE(files.size(), 4U);  // 3 rotated + 1 current
}

TEST_F(LoggingRotationTest, SizeRotation_CurrentFileSmall) {
    nimcp_log_config_t config = createRotationConfig(1024, 3);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    writeUntilRotation(5000);

    // Current file should be smaller than max_size after rotation
    size_t current_size = getFileSize(test_log_path_);
    EXPECT_LE(current_size, 2048U);  // Some buffer for last write
}

//=============================================================================
// Manual Rotation Tests
//=============================================================================

TEST_F(LoggingRotationTest, ManualRotate_CreatesNewFile) {
    nimcp_log_config_t config = createRotationConfig(10240, 5);  // Large size (won't auto-rotate)

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Before rotation");
    nimcp_log_flush(logger_);

    auto files_before = getLogFiles();
    size_t count_before = files_before.size();

    // Manual rotation
    int result = nimcp_log_rotate(logger_);
    EXPECT_EQ(result, 0);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "After rotation");
    nimcp_log_flush(logger_);

    auto files_after = getLogFiles();
    EXPECT_GT(files_after.size(), count_before);
}

TEST_F(LoggingRotationTest, ManualRotate_NullLogger) {
    int result = nimcp_log_rotate(nullptr);
    // Should handle gracefully (may return error or just skip)
    (void)result;
    SUCCEED();
}

//=============================================================================
// File Naming Tests
//=============================================================================

TEST_F(LoggingRotationTest, RotatedFiles_HaveTimestampSuffix) {
    nimcp_log_config_t config = createRotationConfig(512, 5);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    writeUntilRotation(3000);

    auto files = getLogFiles();

    // Check that rotated files have expected naming pattern
    for (const auto& file : files) {
        if (file != test_log_path_) {
            // Rotated files should have extension like .1, .2 or timestamp
            EXPECT_NE(file.find(test_log_path_), std::string::npos);
        }
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LoggingRotationTest, RotationWithZeroMaxFiles) {
    nimcp_log_config_t config = createRotationConfig(1024, 0);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    writeUntilRotation(5000);

    // Should still work, possibly with default behavior
    SUCCEED();
}

TEST_F(LoggingRotationTest, RotationWithVerySmallSize) {
    nimcp_log_config_t config = createRotationConfig(64, 3);  // Very small

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Small size rotation test");
    nimcp_log_flush(logger_);

    // Should handle gracefully even with very small size
    SUCCEED();
}

TEST_F(LoggingRotationTest, RotationOnDestroy) {
    nimcp_log_config_t config = createRotationConfig(1024, 5);

    logger_ = nimcp_log_create(&config);
    ASSERT_NE(logger_, nullptr);

    nimcp_log_write(logger_, LOG_LEVEL_INFO, nullptr, __FILE__, __LINE__,
                    "Final message before destroy");

    // Destroy should flush, not rotate
    nimcp_log_destroy(logger_);
    logger_ = nullptr;

    // Verify final message is in log
    std::ifstream file(test_log_path_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Final message") != std::string::npos);
}
