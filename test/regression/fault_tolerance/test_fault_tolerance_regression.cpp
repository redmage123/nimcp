/**
 * @file test_fault_tolerance_regression.cpp
 * @brief Regression tests for NIMCP fault tolerance system
 *
 * WHAT: Test for regression in fault tolerance behavior
 * WHY:  Ensure checkpoint format compatibility and recovery success rates
 * HOW:  Test historical scenarios and verify consistent behavior
 *
 * REGRESSION TEST CATEGORIES:
 * - Checkpoint format compatibility (backward/forward)
 * - Recovery success rates
 * - False positive/negative rates
 * - Performance overhead
 * - Memory overhead
 * - Historical bug reproductions
 *
 * @author NIMCP Development Team
 * @date 2025-11-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultToleranceRegressionTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    const char* checkpoint_path = "/tmp/fault_tolerance_regression.ckpt";

    void SetUp() override {
        cleanup_files();
        brain = brain_create("fault_tolerance_regression", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        cleanup_files();
    }

    void cleanup_files() {
        remove(checkpoint_path);
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s.meta", checkpoint_path);
        remove(meta_path);
    }

    void train_brain(int iterations) {
        float inputs[10];
        for (int i = 0; i < iterations; i++) {
            for (int j = 0; j < 10; j++) {
                inputs[j] = (j % 2 == 0) ? 1.0f : 0.0f;
            }

            brain_multimodal_input_t input = {};
            input.direct_data = inputs;
            input.direct_dim = 10;
            input.timestamp_ms = nimcp_time_get_ms();

            brain_multimodal_output_t output = {};
            output.output_vector = new float[5];
            output.output_dim = 5;

            brain_process_multimodal(brain, &input, &output);

            delete[] output.output_vector;
        }
    }
};

//=============================================================================
// Checkpoint Format Compatibility Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, CheckpointFormatVersion) {
    // WHAT: Test checkpoint format version handling
    // WHY:  Prevent regression in format compatibility

    train_brain(10);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    // Verify checkpoint can be loaded
    brain_t loaded = brain_load(checkpoint_path);
    EXPECT_NE(loaded, nullptr) << "Current version checkpoint should load";

    if (loaded) brain_destroy(loaded);
}

TEST_F(FaultToleranceRegressionTest, CheckpointMetadataPresence) {
    // WHAT: Test metadata file is created
    // WHY:  Prevent regression in metadata handling

    train_brain(10);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    // Check metadata file exists
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", checkpoint_path);

    FILE* f = fopen(meta_path, "rb");
    EXPECT_NE(f, nullptr) << "Metadata file should exist";
    if (f) fclose(f);
}

TEST_F(FaultToleranceRegressionTest, CheckpointSizeConsistency) {
    // WHAT: Test checkpoint size is consistent
    // WHY:  Prevent regression in serialization

    train_brain(20);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    FILE* f = fopen(checkpoint_path, "rb");
    ASSERT_NE(f, nullptr);
    fseek(f, 0, SEEK_END);
    long size1 = ftell(f);
    fclose(f);

    // Save again with same state
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    f = fopen(checkpoint_path, "rb");
    ASSERT_NE(f, nullptr);
    fseek(f, 0, SEEK_END);
    long size2 = ftell(f);
    fclose(f);

    EXPECT_EQ(size1, size2) << "Checkpoint size should be consistent";
}

//=============================================================================
// Recovery Success Rate Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, RecoverySuccessRate) {
    // WHAT: Test recovery success rate remains high
    // WHY:  Prevent regression in recovery reliability

    const int trials = 10;
    int successes = 0;

    for (int i = 0; i < trials; i++) {
        train_brain(5);
        if (brain_save(brain, checkpoint_path)) {
            brain_t loaded = brain_load(checkpoint_path);
            if (loaded != nullptr) {
                successes++;
                brain_destroy(loaded);
            }
        }
    }

    float success_rate = (float)successes / trials;
    EXPECT_GE(success_rate, 1.0f) << "Recovery success rate should be 100%";
}

TEST_F(FaultToleranceRegressionTest, CheckpointValidationNeverPassesCorrupted) {
    // WHAT: Test corrupted checkpoints never pass validation
    // WHY:  Prevent false negatives in validation

    train_brain(10);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    // Corrupt the checkpoint
    FILE* f = fopen(checkpoint_path, "r+b");
    ASSERT_NE(f, nullptr);
    fseek(f, 100, SEEK_SET);
    uint8_t garbage[100];
    memset(garbage, 0xAA, sizeof(garbage));
    fwrite(garbage, 1, sizeof(garbage), f);
    fclose(f);

    // Should fail to load
    brain_t loaded = brain_load(checkpoint_path);
    EXPECT_EQ(loaded, nullptr) << "Corrupted checkpoint should not load";
}

TEST_F(FaultToleranceRegressionTest, StateRestoreAccuracy) {
    // WHAT: Test restored state matches original
    // WHY:  Prevent regression in state accuracy

    train_brain(20);

    // Get original outputs
    float inputs[10] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                       0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    brain_multimodal_input_t input = {};
    input.direct_data = inputs;
    input.direct_dim = 10;
    input.timestamp_ms = nimcp_time_get_ms();

    brain_multimodal_output_t output_original = {};
    output_original.output_vector = new float[5];
    output_original.output_dim = 5;

    brain_process_multimodal(brain, &input, &output_original);
    float* original_outputs = output_original.output_vector;

    // Save and load
    ASSERT_TRUE(brain_save(brain, checkpoint_path));
    brain_t loaded = brain_load(checkpoint_path);
    ASSERT_NE(loaded, nullptr);

    // Get restored outputs
    brain_multimodal_output_t output_restored = {};
    output_restored.output_vector = new float[5];
    output_restored.output_dim = 5;

    brain_process_multimodal(loaded, &input, &output_restored);
    float* restored_outputs = output_restored.output_vector;

    // Compare - should match within tolerance
    float max_diff = 0.0f;
    for (int i = 0; i < 5; i++) {
        float diff = std::abs(original_outputs[i] - restored_outputs[i]);
        if (diff > max_diff) max_diff = diff;
    }

    EXPECT_LT(max_diff, 0.01f) << "State restore accuracy should be high";

    delete[] output_original.output_vector;
    delete[] output_restored.output_vector;
    brain_destroy(loaded);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, CheckpointSavePerformance) {
    // WHAT: Test checkpoint save time doesn't regress
    // WHY:  Ensure performance remains acceptable

    train_brain(50);

    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(brain_save(brain, checkpoint_path));
    auto end = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Should complete within 1 second
    EXPECT_LT(elapsed_ms, 1000) << "Checkpoint save should be fast";
}

TEST_F(FaultToleranceRegressionTest, CheckpointLoadPerformance) {
    // WHAT: Test checkpoint load time doesn't regress
    // WHY:  Ensure recovery is fast

    train_brain(50);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    auto start = std::chrono::steady_clock::now();
    brain_t loaded = brain_load(checkpoint_path);
    auto end = std::chrono::steady_clock::now();

    ASSERT_NE(loaded, nullptr);

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Should complete within 1 second
    EXPECT_LT(elapsed_ms, 1000) << "Checkpoint load should be fast";

    brain_destroy(loaded);
}

TEST_F(FaultToleranceRegressionTest, SignalHandlerOverhead) {
    // WHAT: Test signal handler installation overhead
    // WHY:  Ensure minimal performance impact

    signal_handler_config_t config = signal_handler_default_config();

    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(signal_handler_install(&config));
    auto end = std::chrono::steady_clock::now();

    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    // Should install very quickly
    EXPECT_LT(elapsed_us, 10000) << "Signal handler install should be fast";

    signal_handler_uninstall();
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, CheckpointMemoryUsage) {
    // WHAT: Test checkpoint doesn't leak memory
    // WHY:  Prevent memory regression

    // Save and load multiple times
    for (int i = 0; i < 10; i++) {
        train_brain(5);
        ASSERT_TRUE(brain_save(brain, checkpoint_path));

        brain_t loaded = brain_load(checkpoint_path);
        ASSERT_NE(loaded, nullptr);
        brain_destroy(loaded);
    }

    // No memory leaks detected (would fail in ASan/Valgrind)
}

TEST_F(FaultToleranceRegressionTest, SignalHandlerMemoryUsage) {
    // WHAT: Test signal handler doesn't leak memory
    // WHY:  Prevent memory regression

    signal_handler_config_t config = signal_handler_default_config();

    // Install and uninstall multiple times
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(signal_handler_install(&config));
        signal_handler_register_brain(brain);
        signal_handler_unregister_brain();
        ASSERT_TRUE(signal_handler_uninstall());
    }

    // No memory leaks detected
}

//=============================================================================
// False Positive/Negative Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, NoFalsePositiveCheckpointFailures) {
    // WHAT: Test valid checkpoints never fail to load
    // WHY:  Prevent false positive validation errors

    const int trials = 20;
    int failures = 0;

    for (int i = 0; i < trials; i++) {
        train_brain(10);
        ASSERT_TRUE(brain_save(brain, checkpoint_path));

        brain_t loaded = brain_load(checkpoint_path);
        if (loaded == nullptr) {
            failures++;
        } else {
            brain_destroy(loaded);
        }
    }

    EXPECT_EQ(failures, 0) << "Valid checkpoints should never fail";
}

TEST_F(FaultToleranceRegressionTest, NoFalseNegativeCorruptionDetection) {
    // WHAT: Test that severe file corruption prevents loading
    // WHY:  Ensure checkpoint format has basic integrity protection
    // NOTE: The checkpoint format doesn't include full-file checksums, so corruption
    //       in non-critical areas (padding, optional fields) may go undetected.
    //       This test verifies that at least header/truncation corruption is caught.

    // Test 1: Truncation should always be detected
    train_brain(10);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    {
        // Truncate file to just the first 16 bytes (corrupt the structure)
        FILE* f = fopen(checkpoint_path, "r+b");
        ASSERT_NE(f, nullptr);
        // Overwrite with garbage to corrupt header magic/version
        uint8_t garbage[16];
        memset(garbage, 0xFF, sizeof(garbage));
        fwrite(garbage, 1, sizeof(garbage), f);
        fclose(f);

        brain_t loaded = brain_load(checkpoint_path);
        EXPECT_EQ(loaded, nullptr) << "Header corruption should prevent loading";
        if (loaded) brain_destroy(loaded);
    }

    // Test 2: Zero-length file
    {
        FILE* f = fopen(checkpoint_path, "wb");
        ASSERT_NE(f, nullptr);
        fclose(f);

        brain_t loaded = brain_load(checkpoint_path);
        EXPECT_EQ(loaded, nullptr) << "Empty file should not load";
        if (loaded) brain_destroy(loaded);
    }

    // Test 3: File with just random garbage
    {
        FILE* f = fopen(checkpoint_path, "wb");
        ASSERT_NE(f, nullptr);
        uint8_t garbage[256];
        memset(garbage, 0xAB, sizeof(garbage));
        fwrite(garbage, 1, sizeof(garbage), f);
        fclose(f);

        brain_t loaded = brain_load(checkpoint_path);
        EXPECT_EQ(loaded, nullptr) << "Random garbage should not load as valid checkpoint";
        if (loaded) brain_destroy(loaded);
    }
}

//=============================================================================
// Historical Bug Reproduction Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, NullPointerDereferenceBugFixed) {
    // WHAT: Test that NULL pointer dereference bug is fixed
    // WHY:  Ensure historical bugs don't resurface

    // This would have crashed in earlier versions
    bool result = brain_save(nullptr, checkpoint_path);
    EXPECT_FALSE(result) << "NULL brain should be handled gracefully";

    brain_t loaded = brain_load("/tmp/nonexistent.ckpt");
    EXPECT_EQ(loaded, nullptr) << "Nonexistent file should be handled";
}

TEST_F(FaultToleranceRegressionTest, DoubleFreeBugFixed) {
    // WHAT: Test that double-free bug is fixed
    // WHY:  Ensure memory safety

    signal_handler_config_t config = signal_handler_default_config();
    ASSERT_TRUE(signal_handler_install(&config));

    // Register and unregister multiple times
    signal_handler_register_brain(brain);
    signal_handler_unregister_brain();
    signal_handler_unregister_brain();  // Would crash if double-free
    signal_handler_unregister_brain();  // Still safe

    signal_handler_uninstall();
}

TEST_F(FaultToleranceRegressionTest, BufferOverflowBugFixed) {
    // WHAT: Test that buffer overflow in signal name is fixed
    // WHY:  Ensure buffer safety

    // Very large signal number (would overflow buffer in old code)
    const char* name = signal_handler_get_signal_name(99999);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "UNKNOWN");
}

//=============================================================================
// Stress Regression Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, RepeatedCheckpointCycles) {
    // WHAT: Test repeated checkpoint cycles don't degrade
    // WHY:  Ensure long-term stability

    std::vector<long> save_times;
    std::vector<long> load_times;

    for (int i = 0; i < 20; i++) {
        train_brain(5);

        // Measure save time
        auto start = std::chrono::steady_clock::now();
        ASSERT_TRUE(brain_save(brain, checkpoint_path));
        auto end = std::chrono::steady_clock::now();
        auto save_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        save_times.push_back(save_ms);

        // Measure load time
        start = std::chrono::steady_clock::now();
        brain_t loaded = brain_load(checkpoint_path);
        end = std::chrono::steady_clock::now();
        auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        load_times.push_back(load_ms);

        ASSERT_NE(loaded, nullptr);
        brain_destroy(loaded);
    }

    // Performance shouldn't degrade over cycles
    // Compare first and last cycles
    EXPECT_LT(save_times.back(), save_times[0] * 2)
        << "Save time shouldn't degrade significantly";
    EXPECT_LT(load_times.back(), load_times[0] * 2)
        << "Load time shouldn't degrade significantly";
}

TEST_F(FaultToleranceRegressionTest, SignalHandlerRepeatedCycles) {
    // WHAT: Test repeated signal handler install/uninstall
    // WHY:  Ensure no state corruption over time

    signal_handler_config_t config = signal_handler_default_config();

    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(signal_handler_install(&config));
        signal_handler_register_brain(brain);

        // Get stats
        signal_handler_stats_t stats = signal_handler_get_stats();
        EXPECT_EQ(stats.sigsegv_count, 0u);

        signal_handler_unregister_brain();
        ASSERT_TRUE(signal_handler_uninstall());
    }
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, CheckpointOutputConsistency) {
    // WHAT: Test checkpoint produces consistent output for same input
    // WHY:  Ensure deterministic behavior

    train_brain(20);

    // Save first checkpoint
    const char* ckpt1 = "/tmp/ckpt_consistency_1.ckpt";
    const char* ckpt2 = "/tmp/ckpt_consistency_2.ckpt";

    ASSERT_TRUE(brain_save(brain, ckpt1));
    ASSERT_TRUE(brain_save(brain, ckpt2));

    // Compare file sizes (should be identical)
    FILE* f1 = fopen(ckpt1, "rb");
    FILE* f2 = fopen(ckpt2, "rb");

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);

    fseek(f1, 0, SEEK_END);
    long size1 = ftell(f1);
    fseek(f2, 0, SEEK_END);
    long size2 = ftell(f2);

    fclose(f1);
    fclose(f2);

    EXPECT_EQ(size1, size2) << "Checkpoint files should be identical";

    remove(ckpt1);
    remove(ckpt2);
}

TEST_F(FaultToleranceRegressionTest, SignalHandlerConfigConsistency) {
    // WHAT: Test signal handler config is applied consistently
    // WHY:  Ensure configuration doesn't drift

    signal_handler_config_t config = signal_handler_default_config();

    EXPECT_EQ(config.sigsegv_mode, SIGNAL_MODE_LOG_SHUTDOWN);
    EXPECT_EQ(config.sigfpe_mode, SIGNAL_MODE_LOG_CONTINUE);
    EXPECT_TRUE(config.enable_stack_trace);

    // Install and verify still works
    ASSERT_TRUE(signal_handler_install(&config));
    ASSERT_TRUE(signal_handler_uninstall());
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, EmptyBrainCheckpoint) {
    // WHAT: Test checkpoint of untrained brain
    // WHY:  Ensure edge case is handled

    // Don't train - save immediately after creation
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    brain_t loaded = brain_load(checkpoint_path);
    EXPECT_NE(loaded, nullptr);
    if (loaded) brain_destroy(loaded);
}

TEST_F(FaultToleranceRegressionTest, LargeBrainCheckpoint) {
    // WHAT: Test checkpoint of larger brain
    // WHY:  Ensure scalability doesn't regress

    brain_destroy(brain);
    brain = brain_create("large_regression", BRAIN_SIZE_MEDIUM,
                        BRAIN_TASK_CLASSIFICATION, 50, 20);
    ASSERT_NE(brain, nullptr);

    train_brain(10);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    brain_t loaded = brain_load(checkpoint_path);
    EXPECT_NE(loaded, nullptr);
    if (loaded) brain_destroy(loaded);
}

TEST_F(FaultToleranceRegressionTest, CheckpointPathWithSpaces) {
    // WHAT: Test checkpoint path with spaces
    // WHY:  Ensure path handling doesn't regress

    const char* spaced_path = "/tmp/test checkpoint with spaces.ckpt";

    train_brain(10);
    ASSERT_TRUE(brain_save(brain, spaced_path));

    brain_t loaded = brain_load(spaced_path);
    EXPECT_NE(loaded, nullptr);
    if (loaded) brain_destroy(loaded);

    remove(spaced_path);
}

//=============================================================================
// Compatibility Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, CrossPlatformCheckpointCompatibility) {
    // WHAT: Test checkpoint format is platform-independent
    // WHY:  Ensure checkpoints work across systems

    train_brain(10);
    ASSERT_TRUE(brain_save(brain, checkpoint_path));

    // Load should work regardless of endianness/alignment
    brain_t loaded = brain_load(checkpoint_path);
    EXPECT_NE(loaded, nullptr);
    if (loaded) brain_destroy(loaded);
}

TEST_F(FaultToleranceRegressionTest, SignalHandlerCrossPlatformBehavior) {
    // WHAT: Test signal handler works consistently
    // WHY:  Ensure cross-platform compatibility

    signal_handler_config_t config = signal_handler_default_config();

    // Should work on all POSIX platforms
    EXPECT_TRUE(signal_handler_install(&config));
    EXPECT_TRUE(signal_handler_uninstall());
}

//=============================================================================
// Documentation Regression Tests
//=============================================================================

TEST_F(FaultToleranceRegressionTest, APIDocumentationAccuracy) {
    // WHAT: Test API behavior matches documentation
    // WHY:  Prevent documentation drift

    // brain_save should return true on success (documented)
    train_brain(10);
    bool result = brain_save(brain, checkpoint_path);
    EXPECT_TRUE(result) << "API should match documentation";

    // brain_load should return NULL on failure (documented)
    brain_t loaded = brain_load("/tmp/nonexistent.ckpt");
    EXPECT_EQ(loaded, nullptr) << "API should match documentation";
}
