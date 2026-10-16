/**
 * @file test_fault_tolerance_exception_integration.cpp
 * @brief Integration tests for fault tolerance module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * Tests integration between:
 * - Checkpoint and recovery systems with exception handling
 * - Health monitor exception handling during continuous monitoring
 * - Fast recovery exception handling with brain context
 *
 * @author NIMCP Team
 * @date 2025-01-25
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "utils/fault_tolerance/nimcp_fast_recovery.h"
#include "utils/exception/nimcp_exception.h"
#include "core/brain/nimcp_brain.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class FaultToleranceExceptionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any pending exceptions
        nimcp_exception_clear_current();

        // Reset fast recovery stats
        fast_recovery_reset_stats();

        // Create temp directory for tests
        snprintf(temp_dir, sizeof(temp_dir), "/tmp/nimcp_ft_int_test_%d", getpid());
        mkdir(temp_dir, 0755);
    }

    void TearDown() override {
        // Clean up temp directory
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
        (void)system(cmd);

        // Destroy any created monitors
        if (monitor) {
            health_monitor_destroy(monitor);
            monitor = nullptr;
        }

        // Destroy any created brains
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    char temp_dir[256];
    health_monitor_t monitor = nullptr;
    brain_t brain = nullptr;

    // Helper to create a minimal test brain
    brain_t create_test_brain() {
        return brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    }

    // Helper to create a valid checkpoint file
    bool create_test_checkpoint(const char* path) {
        brain_t test_brain = create_test_brain();
        if (!test_brain) return false;

        bool result = checkpoint_save(test_brain, path);
        brain_destroy(test_brain);
        return result;
    }
};

//=============================================================================
// Checkpoint-Recovery Integration Exception Tests
//=============================================================================

TEST_F(FaultToleranceExceptionIntegrationTest, CheckpointSaveLoadCycle) {
    // Create a brain
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Save checkpoint
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/test_cycle.ckpt", temp_dir);

    bool save_result = checkpoint_save(brain, ckpt_path);
    EXPECT_TRUE(save_result);

    // Destroy original brain
    brain_destroy(brain);
    brain = nullptr;

    // Load checkpoint
    bool load_result = checkpoint_load(&brain, ckpt_path);
    EXPECT_TRUE(load_result);
    EXPECT_NE(brain, nullptr);
}

TEST_F(FaultToleranceExceptionIntegrationTest, CheckpointSaveExWithOptions) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/test_options.ckpt", temp_dir);

    checkpoint_options_t opts = checkpoint_default_options();
    opts.enable_compression = false;
    opts.save_subsystems = true;

    bool result = checkpoint_save_ex(brain, ckpt_path, &opts);
    EXPECT_TRUE(result);

    // Validate the checkpoint
    EXPECT_TRUE(checkpoint_validate(ckpt_path));
}

TEST_F(FaultToleranceExceptionIntegrationTest, RecoveryAutoRestoreMultipleCheckpoints) {
    // Create multiple checkpoints
    for (int i = 0; i < 3; i++) {
        brain = create_test_brain();
        ASSERT_NE(brain, nullptr);

        char ckpt_path[512];
        snprintf(ckpt_path, sizeof(ckpt_path), "%s/brain_%d.ckpt", temp_dir, i);
        EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

        brain_destroy(brain);
        brain = nullptr;

        // Small delay to ensure different timestamps
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Auto restore should pick newest
    bool result = recovery_auto_restore(&brain, temp_dir);
    EXPECT_TRUE(result);
    EXPECT_NE(brain, nullptr);
}

TEST_F(FaultToleranceExceptionIntegrationTest, RecoveryRollbackIntegration) {
    // Create initial brain
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Save checkpoint
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/rollback.ckpt", temp_dir);
    EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

    // Rollback to checkpoint (essentially replaces state)
    bool result = recovery_rollback(brain, ckpt_path);
    EXPECT_TRUE(result);
}

TEST_F(FaultToleranceExceptionIntegrationTest, RecoveryPartialWithValidCheckpoint) {
    // Create a valid checkpoint
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/partial.ckpt", temp_dir);
    EXPECT_TRUE(create_test_checkpoint(ckpt_path));

    // Try partial recovery
    int recovery_level = 0;
    bool result = recovery_partial(&brain, ckpt_path, &recovery_level);
    EXPECT_TRUE(result);
    EXPECT_GT(recovery_level, 0);
    EXPECT_NE(brain, nullptr);
}

TEST_F(FaultToleranceExceptionIntegrationTest, CheckpointListAndCleanup) {
    // Create multiple checkpoints
    for (int i = 0; i < 5; i++) {
        brain = create_test_brain();
        ASSERT_NE(brain, nullptr);

        char ckpt_path[512];
        snprintf(ckpt_path, sizeof(ckpt_path), "%s/ckpt_%d.ckpt", temp_dir, i);
        EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

        brain_destroy(brain);
        brain = nullptr;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // List checkpoints
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;
    bool result = checkpoint_list(temp_dir, &list, &count);
    EXPECT_TRUE(result);
    EXPECT_EQ(count, 5u);

    if (list) {
        free(list);
        list = nullptr;
    }

    // Cleanup old checkpoints (keep 2)
    result = checkpoint_cleanup_old(temp_dir, 2);
    EXPECT_TRUE(result);

    // List again - should have 2
    result = checkpoint_list(temp_dir, &list, &count);
    EXPECT_TRUE(result);
    EXPECT_EQ(count, 2u);

    if (list) {
        free(list);
    }
}

//=============================================================================
// Health Monitor Integration Exception Tests
//=============================================================================

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorLifecycle) {
    monitor = health_monitor_create("integration_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Start monitoring
    EXPECT_TRUE(health_monitor_start(monitor));
    EXPECT_TRUE(health_monitor_is_running(monitor));

    // Record some metrics
    for (int i = 0; i < 10; i++) {
        health_monitor_record_operation(monitor, "test_op", 1000 + i * 100);
        health_monitor_record_memory(monitor, 1024 * (i + 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GE(status.score, 0.0f);
    EXPECT_LE(status.score, 100.0f);

    // Stop monitoring
    EXPECT_TRUE(health_monitor_stop(monitor));
    EXPECT_FALSE(health_monitor_is_running(monitor));
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorAnomalyDetection) {
    monitor = health_monitor_create("anomaly_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Establish baseline
    for (int i = 0; i < 20; i++) {
        health_monitor_record_memory(monitor, 10000);
        health_monitor_record_operation(monitor, "normal_op", 100);
    }
    EXPECT_TRUE(health_monitor_establish_baseline(monitor));

    // Detect anomalies (may or may not find any)
    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);
    EXPECT_GE(count, 0);
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorErrorTracking) {
    monitor = health_monitor_create("error_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Record various error types
    health_monitor_record_error(monitor, "NULL_PTR");
    health_monitor_record_error(monitor, "NULL_PTR");
    health_monitor_record_error(monitor, "OVERFLOW");
    health_monitor_record_error(monitor, "IO_ERROR");

    // Get status - should reflect errors
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorThreadMetrics) {
    monitor = health_monitor_create("thread_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Simulate thread contention events
    health_monitor_record_thread_event(monitor, false, 0);
    health_monitor_record_thread_event(monitor, false, 0);
    health_monitor_record_thread_event(monitor, true, 1000);  // Contentious
    health_monitor_record_thread_event(monitor, true, 2000);  // Contentious

    // Get status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorThroughputTracking) {
    monitor = health_monitor_create("throughput_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Record throughput samples
    for (int i = 0; i < 10; i++) {
        health_monitor_record_throughput(monitor, 100 + i * 10, 1000000);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GE(status.throughput_score, 0.0f);
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorCacheMetrics) {
    monitor = health_monitor_create("cache_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Record cache accesses
    for (int i = 0; i < 100; i++) {
        health_monitor_record_cache_access(monitor, (i % 3) != 0);  // ~67% hit rate
    }

    // Get status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GE(status.cache_score, 0.0f);
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorPredictFailure) {
    monitor = health_monitor_create("predict_test_brain");
    ASSERT_NE(monitor, nullptr);

    // Establish baseline
    for (int i = 0; i < 20; i++) {
        health_monitor_record_memory(monitor, 10000);
    }
    EXPECT_TRUE(health_monitor_establish_baseline(monitor));

    // Predict failure (may or may not predict based on metrics)
    uint32_t ttf = 0;
    bool predicted = health_monitor_predict_failure(monitor, &ttf);
    // Result depends on recorded metrics
    if (predicted) {
        EXPECT_GT(ttf, 0u);
    }
}

//=============================================================================
// Fast Recovery Integration Exception Tests
//=============================================================================

TEST_F(FaultToleranceExceptionIntegrationTest, FastRecoveryWithBrain) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Test recovery types that work with brain
    // Note: CLEAR_NAN, CLIP_GRADIENTS, etc. require brain internal access

    // Reset FPU works without brain
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_RESET_FPU, brain);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
}

TEST_F(FaultToleranceExceptionIntegrationTest, FastRecoveryStatistics) {
    fast_recovery_reset_stats();

    // Execute several recoveries
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    fast_recovery_execute(FAST_RECOVERY_RESET_COUNTER, nullptr);

    // Check statistics
    fast_recovery_stats_t stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 3u);
    EXPECT_EQ(stats.successful_recoveries, 3u);
    EXPECT_EQ(stats.reset_fpu_count, 1u);
    EXPECT_EQ(stats.flush_buffers_count, 1u);
    EXPECT_EQ(stats.reset_counter_count, 1u);
}

TEST_F(FaultToleranceExceptionIntegrationTest, FastRecoveryAttemptChain) {
    // Simulate error context
    fast_recovery_context_t ctx = {};
    ctx.signal = SIGFPE;
    ctx.is_numeric_error = false;

    // Attempt recovery
    fast_recovery_result_t result = fast_recovery_attempt(&ctx, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_EQ(result.type, FAST_RECOVERY_RESET_FPU);
}

TEST_F(FaultToleranceExceptionIntegrationTest, FastRecoveryPerformanceMetrics) {
    fast_recovery_reset_stats();

    // Execute multiple recoveries
    for (int i = 0; i < 10; i++) {
        fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    }

    // Check performance metrics
    uint32_t avg_latency = fast_recovery_get_avg_latency_us();
    EXPECT_GT(avg_latency, 0u);
    EXPECT_LT(avg_latency, 1000u);  // Should be sub-millisecond

    float hit_rate = fast_recovery_get_hit_rate();
    EXPECT_EQ(hit_rate, 100.0f);  // All successful

    float success_rate = fast_recovery_get_success_rate();
    EXPECT_EQ(success_rate, 100.0f);
}

//=============================================================================
// Combined Integration Tests
//=============================================================================

TEST_F(FaultToleranceExceptionIntegrationTest, MonitorDuringCheckpoint) {
    // Create monitor
    monitor = health_monitor_create("checkpoint_monitor_brain");
    ASSERT_NE(monitor, nullptr);

    // Create brain
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Record operation for checkpoint
    uint64_t start_time = health_monitor_get_timestamp_us();

    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/monitored.ckpt", temp_dir);
    bool result = checkpoint_save(brain, ckpt_path);

    uint64_t end_time = health_monitor_get_timestamp_us();

    // Record the checkpoint operation
    health_monitor_record_operation(monitor, "checkpoint_save", end_time - start_time);

    EXPECT_TRUE(result);

    // Get status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
}

TEST_F(FaultToleranceExceptionIntegrationTest, RecoveryAfterFastRecovery) {
    // Create and save brain
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/recovery_chain.ckpt", temp_dir);
    EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

    // Simulate fast recovery attempt
    fast_recovery_context_t ctx = {};
    ctx.signal = SIGFPE;
    fast_recovery_result_t fr_result = fast_recovery_attempt(&ctx, brain);
    EXPECT_EQ(fr_result.status, FAST_RECOVERY_SUCCESS);

    // If fast recovery wasn't enough, could rollback to checkpoint
    if (fr_result.fallback_needed) {
        EXPECT_TRUE(recovery_rollback(brain, ckpt_path));
    }
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorWithRecoveryMetrics) {
    monitor = health_monitor_create("recovery_metrics_brain");
    ASSERT_NE(monitor, nullptr);

    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Record fast recovery as an operation
    uint64_t start = health_monitor_get_timestamp_us();
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, brain);
    uint64_t duration = health_monitor_get_timestamp_us() - start;

    health_monitor_record_operation(monitor, "fast_recovery", duration);

    // Get stats
    operation_metric_t op_stats;
    bool found = health_monitor_get_operation_stats(monitor, "fast_recovery", &op_stats);
    EXPECT_TRUE(found);
    EXPECT_EQ(op_stats.count, 1u);
}

//=============================================================================
// Edge Cases and Error Scenarios
//=============================================================================

TEST_F(FaultToleranceExceptionIntegrationTest, MultipleMonitorsForSameBrain) {
    health_monitor_t monitor1 = health_monitor_create("same_brain");
    health_monitor_t monitor2 = health_monitor_create("same_brain");

    ASSERT_NE(monitor1, nullptr);
    ASSERT_NE(monitor2, nullptr);

    // Both should work independently
    health_monitor_record_memory(monitor1, 1000);
    health_monitor_record_memory(monitor2, 2000);

    health_status_snapshot_t status1, status2;
    EXPECT_TRUE(health_monitor_get_status(monitor1, &status1));
    EXPECT_TRUE(health_monitor_get_status(monitor2, &status2));

    health_monitor_destroy(monitor1);
    health_monitor_destroy(monitor2);
    monitor = nullptr;  // Prevent double free in TearDown
}

TEST_F(FaultToleranceExceptionIntegrationTest, RapidCheckpointOperations) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Rapid checkpoint operations
    for (int i = 0; i < 5; i++) {
        char ckpt_path[512];
        snprintf(ckpt_path, sizeof(ckpt_path), "%s/rapid_%d.ckpt", temp_dir, i);
        EXPECT_TRUE(checkpoint_save(brain, ckpt_path));
        EXPECT_TRUE(checkpoint_validate(ckpt_path));
    }
}

TEST_F(FaultToleranceExceptionIntegrationTest, HealthMonitorUnderLoad) {
    monitor = health_monitor_create("load_test_brain");
    ASSERT_NE(monitor, nullptr);

    EXPECT_TRUE(health_monitor_start(monitor));

    // Rapid metric recording
    for (int i = 0; i < 100; i++) {
        health_monitor_record_operation(monitor, "load_op", 100 + (i % 50));
        health_monitor_record_memory(monitor, 1024 * (1 + (i % 10)));
        health_monitor_record_cache_access(monitor, (i % 2) == 0);
    }

    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));

    EXPECT_TRUE(health_monitor_stop(monitor));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
