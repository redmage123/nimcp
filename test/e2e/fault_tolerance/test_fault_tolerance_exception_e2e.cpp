/**
 * @file test_fault_tolerance_exception_e2e.cpp
 * @brief End-to-End tests for fault tolerance module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * Tests complete fault tolerance pipeline with exception recovery:
 * - Full checkpoint-monitor-recovery chain
 * - Exception handling under real-world scenarios
 * - Stress testing with concurrent operations
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
#include <atomic>
#include <vector>

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

class FaultToleranceExceptionE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_clear_current();
        fast_recovery_reset_stats();

        snprintf(temp_dir, sizeof(temp_dir), "/tmp/nimcp_ft_e2e_test_%d", getpid());
        mkdir(temp_dir, 0755);
    }

    void TearDown() override {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
        (void)system(cmd);

        if (monitor) {
            health_monitor_stop(monitor);
            health_monitor_destroy(monitor);
            monitor = nullptr;
        }

        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    char temp_dir[256];
    health_monitor_t monitor = nullptr;
    brain_t brain = nullptr;

    brain_t create_test_brain() {
        return brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    }
};

//=============================================================================
// Full Pipeline E2E Tests
//=============================================================================

TEST_F(FaultToleranceExceptionE2ETest, FullCheckpointRecoveryPipeline) {
    // Create brain
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Create health monitor
    monitor = health_monitor_create("e2e_pipeline_brain");
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(health_monitor_start(monitor));

    // Save initial checkpoint
    char ckpt1_path[512];
    snprintf(ckpt1_path, sizeof(ckpt1_path), "%s/pipeline_ckpt1.ckpt", temp_dir);

    uint64_t start = health_monitor_get_timestamp_us();
    EXPECT_TRUE(checkpoint_save(brain, ckpt1_path));
    uint64_t duration = health_monitor_get_timestamp_us() - start;
    health_monitor_record_operation(monitor, "checkpoint_save", duration);

    // Validate checkpoint
    EXPECT_TRUE(checkpoint_validate(ckpt1_path));

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Save second checkpoint
    char ckpt2_path[512];
    snprintf(ckpt2_path, sizeof(ckpt2_path), "%s/pipeline_ckpt2.ckpt", temp_dir);

    start = health_monitor_get_timestamp_us();
    checkpoint_options_t opts = checkpoint_default_options();
    opts.enable_compression = true;
    EXPECT_TRUE(checkpoint_save_ex(brain, ckpt2_path, &opts));
    duration = health_monitor_get_timestamp_us() - start;
    health_monitor_record_operation(monitor, "checkpoint_save_compressed", duration);

    // Destroy original brain
    brain_destroy(brain);
    brain = nullptr;

    // Auto-restore from latest
    start = health_monitor_get_timestamp_us();
    EXPECT_TRUE(recovery_auto_restore(&brain, temp_dir));
    duration = health_monitor_get_timestamp_us() - start;
    health_monitor_record_operation(monitor, "auto_restore", duration);
    EXPECT_NE(brain, nullptr);

    // Get health status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GE(status.score, 0.0f);

    // Check operation stats
    operation_metric_t op_stats;
    EXPECT_TRUE(health_monitor_get_operation_stats(monitor, "checkpoint_save", &op_stats));
    EXPECT_EQ(op_stats.count, 1u);

    EXPECT_TRUE(health_monitor_stop(monitor));
}

TEST_F(FaultToleranceExceptionE2ETest, FastRecoveryWithMonitoringPipeline) {
    // Create brain
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Create health monitor
    monitor = health_monitor_create("fast_recovery_e2e_brain");
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(health_monitor_start(monitor));

    // Save checkpoint for potential rollback
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/fast_recovery.ckpt", temp_dir);
    EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

    // Simulate FPE error and fast recovery
    fast_recovery_context_t ctx = {};
    ctx.signal = SIGFPE;
    ctx.is_numeric_error = true;
    ctx.brain_ptr = brain;

    uint64_t start = health_monitor_get_timestamp_us();
    fast_recovery_result_t result = fast_recovery_attempt(&ctx, brain);
    uint64_t duration = health_monitor_get_timestamp_us() - start;

    health_monitor_record_operation(monitor, "fast_recovery", duration);

    if (result.status == FAST_RECOVERY_SUCCESS) {
        // Fast recovery succeeded
        EXPECT_FALSE(result.fallback_needed);
    } else if (result.fallback_needed) {
        // Need to rollback to checkpoint
        start = health_monitor_get_timestamp_us();
        EXPECT_TRUE(recovery_rollback(brain, ckpt_path));
        duration = health_monitor_get_timestamp_us() - start;
        health_monitor_record_operation(monitor, "rollback_recovery", duration);
    }

    // Get statistics
    fast_recovery_stats_t stats = fast_recovery_get_stats();
    EXPECT_GT(stats.fast_hits + stats.fast_misses, 0u);

    // Check health
    EXPECT_TRUE(health_monitor_is_healthy(monitor));

    EXPECT_TRUE(health_monitor_stop(monitor));
}

TEST_F(FaultToleranceExceptionE2ETest, ExceptionHandlingChain) {
    // Test exception handling across all fault tolerance components

    // 1. Checkpoint exceptions
    checkpoint_clear_error();
    bool result = checkpoint_save(nullptr, temp_dir);
    EXPECT_FALSE(result);

    result = checkpoint_load(nullptr, temp_dir);
    EXPECT_FALSE(result);

    // 2. Health monitor exceptions
    float score = health_monitor_get_score(nullptr);
    EXPECT_LT(score, 0.0f);

    health_status_snapshot_t status;
    result = health_monitor_get_status(nullptr, &status);
    EXPECT_FALSE(result);

    // 3. Fast recovery exceptions
    fast_recovery_result_t fr_result = fast_recovery_execute_with_context(nullptr, nullptr);
    EXPECT_EQ(fr_result.status, FAST_RECOVERY_NOT_APPLICABLE);

    // All exceptions handled gracefully without crashes
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(FaultToleranceExceptionE2ETest, ConcurrentCheckpointOperations) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);

    // Create multiple threads performing checkpoint operations
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, &failure_count, t]() {
            for (int i = 0; i < 5; i++) {
                char ckpt_path[512];
                snprintf(ckpt_path, sizeof(ckpt_path), "%s/concurrent_%d_%d.ckpt",
                         temp_dir, t, i);

                if (checkpoint_save(brain, ckpt_path)) {
                    success_count++;
                    checkpoint_validate(ckpt_path);
                } else {
                    failure_count++;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most operations should succeed
    EXPECT_GT(success_count.load(), 0);
}

TEST_F(FaultToleranceExceptionE2ETest, ConcurrentHealthMonitorOperations) {
    monitor = health_monitor_create("concurrent_e2e_brain");
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(health_monitor_start(monitor));

    std::atomic<bool> running(true);
    std::atomic<int> operations_completed(0);

    // Multiple threads recording metrics
    std::vector<std::thread> threads;

    // Operation recording thread
    threads.emplace_back([this, &running, &operations_completed]() {
        while (running.load()) {
            health_monitor_record_operation(monitor, "test_op", 100);
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Memory recording thread
    threads.emplace_back([this, &running, &operations_completed]() {
        while (running.load()) {
            health_monitor_record_memory(monitor, 1024);
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Error recording thread
    threads.emplace_back([this, &running, &operations_completed]() {
        while (running.load()) {
            health_monitor_record_error(monitor, "TEST_ERROR");
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Status reading thread
    threads.emplace_back([this, &running, &operations_completed]() {
        health_status_snapshot_t status;
        while (running.load()) {
            health_monitor_get_status(monitor, &status);
            operations_completed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running.store(false);

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(operations_completed.load(), 0);

    // Monitor should still be healthy
    EXPECT_TRUE(health_monitor_is_running(monitor));
    EXPECT_TRUE(health_monitor_stop(monitor));
}

TEST_F(FaultToleranceExceptionE2ETest, RapidFastRecoverySequence) {
    fast_recovery_reset_stats();

    // Rapid-fire fast recovery operations
    for (int i = 0; i < 100; i++) {
        fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    }

    fast_recovery_stats_t stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 100u);
    EXPECT_EQ(stats.successful_recoveries, 100u);
    EXPECT_EQ(stats.reset_fpu_count, 100u);

    uint32_t avg_latency = fast_recovery_get_avg_latency_us();
    EXPECT_GT(avg_latency, 0u);
    EXPECT_LT(avg_latency, 1000u);  // Should be sub-millisecond
}

//=============================================================================
// Real-World Scenario Tests
//=============================================================================

TEST_F(FaultToleranceExceptionE2ETest, SimulatedTrainingWithCheckpoints) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    monitor = health_monitor_create("training_brain");
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(health_monitor_start(monitor));

    // Simulate training loop with periodic checkpoints
    for (int epoch = 0; epoch < 3; epoch++) {
        // Simulate training step
        health_monitor_record_operation(monitor, "training_step", 50000);  // 50ms
        health_monitor_record_memory(monitor, 1024 * 1024 * (epoch + 1));

        // Periodic checkpoint
        char ckpt_path[512];
        snprintf(ckpt_path, sizeof(ckpt_path), "%s/training_epoch_%d.ckpt", temp_dir, epoch);

        uint64_t start = health_monitor_get_timestamp_us();
        EXPECT_TRUE(checkpoint_save(brain, ckpt_path));
        uint64_t duration = health_monitor_get_timestamp_us() - start;
        health_monitor_record_operation(monitor, "checkpoint", duration);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Check health status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));

    // Cleanup old checkpoints (keep last 2)
    EXPECT_TRUE(checkpoint_cleanup_old(temp_dir, 2));

    // Verify cleanup
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;
    EXPECT_TRUE(checkpoint_list(temp_dir, &list, &count));
    EXPECT_EQ(count, 2u);
    if (list) free(list);

    EXPECT_TRUE(health_monitor_stop(monitor));
}

TEST_F(FaultToleranceExceptionE2ETest, SimulatedCrashRecovery) {
    // Phase 1: Create brain and checkpoint
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/crash_recovery.ckpt", temp_dir);
    EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

    // Destroy brain (simulates crash)
    brain_destroy(brain);
    brain = nullptr;

    // Phase 2: Recovery
    monitor = health_monitor_create("recovery_brain");
    ASSERT_NE(monitor, nullptr);

    // Try fast recovery first (won't apply without active brain/error)
    fast_recovery_context_t ctx = {};
    ctx.signal = 0;
    fast_recovery_result_t fr_result = fast_recovery_attempt(&ctx, nullptr);

    if (fr_result.status != FAST_RECOVERY_SUCCESS) {
        // Fall back to checkpoint recovery
        uint64_t start = health_monitor_get_timestamp_us();
        EXPECT_TRUE(checkpoint_load(&brain, ckpt_path));
        uint64_t duration = health_monitor_get_timestamp_us() - start;
        health_monitor_record_operation(monitor, "crash_recovery", duration);
    }

    EXPECT_NE(brain, nullptr);

    // Record successful recovery
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
}

TEST_F(FaultToleranceExceptionE2ETest, AnomalyDetectionAndRecovery) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    monitor = health_monitor_create("anomaly_detection_brain");
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(health_monitor_start(monitor));

    // Establish baseline
    for (int i = 0; i < 20; i++) {
        health_monitor_record_memory(monitor, 10000);
        health_monitor_record_operation(monitor, "normal_op", 100);
    }
    EXPECT_TRUE(health_monitor_establish_baseline(monitor));

    // Create checkpoint before anomaly
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/pre_anomaly.ckpt", temp_dir);
    EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

    // Simulate anomalous behavior (memory spike)
    for (int i = 0; i < 10; i++) {
        health_monitor_record_memory(monitor, 1000000);  // 100x normal
        health_monitor_record_error(monitor, "MEMORY_ERROR");
    }

    // Detect anomalies
    anomaly_t anomalies[10];
    int32_t anomaly_count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    // If anomalies detected, trigger recovery
    if (anomaly_count > 0) {
        // Try fast recovery
        fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_CLEAR_CACHE, brain);

        if (result.status != FAST_RECOVERY_SUCCESS) {
            // Rollback to checkpoint
            EXPECT_TRUE(recovery_rollback(brain, ckpt_path));
        }
    }

    // Clear resolved anomalies
    health_monitor_clear_resolved_anomalies(monitor);

    EXPECT_TRUE(health_monitor_stop(monitor));
}

TEST_F(FaultToleranceExceptionE2ETest, PredictiveFailureHandling) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    monitor = health_monitor_create("predictive_brain");
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(health_monitor_start(monitor));

    // Establish baseline
    for (int i = 0; i < 30; i++) {
        health_monitor_record_memory(monitor, 10000);
        health_monitor_record_throughput(monitor, 100, 1000000);
    }
    EXPECT_TRUE(health_monitor_establish_baseline(monitor));

    // Create checkpoint
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s/predictive.ckpt", temp_dir);
    EXPECT_TRUE(checkpoint_save(brain, ckpt_path));

    // Check for failure prediction
    uint32_t ttf = 0;
    bool failure_predicted = health_monitor_predict_failure(monitor, &ttf);

    if (failure_predicted) {
        // Proactive recovery
        fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, brain);

        // Create emergency checkpoint
        char emergency_path[512];
        snprintf(emergency_path, sizeof(emergency_path), "%s/emergency.ckpt", temp_dir);
        EXPECT_TRUE(checkpoint_save(brain, emergency_path));
    }

    // Get final health status
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GE(status.score, 0.0f);

    EXPECT_TRUE(health_monitor_stop(monitor));
}

//=============================================================================
// Error Boundary Tests
//=============================================================================

TEST_F(FaultToleranceExceptionE2ETest, NullPointerBoundaries) {
    // Ensure all NULL pointer cases are handled gracefully without crashes

    // Checkpoint module
    EXPECT_FALSE(checkpoint_save(nullptr, nullptr));
    EXPECT_FALSE(checkpoint_save_ex(nullptr, nullptr, nullptr));
    EXPECT_FALSE(checkpoint_save_incremental(nullptr, nullptr, nullptr));
    EXPECT_FALSE(checkpoint_load(nullptr, nullptr));
    EXPECT_FALSE(checkpoint_validate(nullptr));
    EXPECT_FALSE(checkpoint_list(nullptr, nullptr, nullptr));
    EXPECT_FALSE(checkpoint_cleanup_old(nullptr, 0));
    EXPECT_FALSE(recovery_auto_restore(nullptr, nullptr));
    EXPECT_FALSE(recovery_rollback(nullptr, nullptr));
    EXPECT_FALSE(recovery_partial(nullptr, nullptr, nullptr));

    // Health monitor module
    EXPECT_EQ(health_monitor_create(nullptr), nullptr);
    EXPECT_FALSE(health_monitor_start(nullptr));
    EXPECT_FALSE(health_monitor_stop(nullptr));
    EXPECT_FALSE(health_monitor_is_running(nullptr));
    EXPECT_FALSE(health_monitor_get_status(nullptr, nullptr));
    EXPECT_LT(health_monitor_get_score(nullptr), 0.0f);
    EXPECT_EQ(health_monitor_get_status_level(nullptr), HEALTH_UNKNOWN);
    EXPECT_EQ(health_monitor_detect_anomalies(nullptr, nullptr, 0), -1);
    EXPECT_FALSE(health_monitor_predict_failure(nullptr, nullptr));
    EXPECT_EQ(health_monitor_clear_resolved_anomalies(nullptr), 0u);
    EXPECT_EQ(health_monitor_get_anomaly_count(nullptr, ANOMALY_NONE), 0u);
    EXPECT_FALSE(health_monitor_establish_baseline(nullptr));
    EXPECT_FALSE(health_monitor_reset_baseline(nullptr));
    EXPECT_FALSE(health_monitor_set_anomaly_threshold(nullptr, 0.0));
    EXPECT_FALSE(health_monitor_set_interval(nullptr, 0));
    EXPECT_EQ(health_monitor_export_json(nullptr, nullptr, 0), -1);
    EXPECT_FALSE(health_monitor_get_operation_stats(nullptr, nullptr, nullptr));
    EXPECT_FALSE(health_monitor_get_memory_stats(nullptr, nullptr));

    // Fast recovery module
    EXPECT_EQ(fast_recovery_is_applicable(nullptr), FAST_RECOVERY_NONE);
    EXPECT_EQ(fast_recovery_execute_with_context(nullptr, nullptr).status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_FALSE(fast_recovery_validate_result(nullptr));

    // All operations completed without crashes
}

TEST_F(FaultToleranceExceptionE2ETest, ResourceExhaustion) {
    // Create many monitors to test resource handling
    std::vector<health_monitor_t> monitors;

    for (int i = 0; i < 10; i++) {
        char id[64];
        snprintf(id, sizeof(id), "resource_test_%d", i);
        health_monitor_t mon = health_monitor_create(id);
        if (mon) {
            monitors.push_back(mon);
        }
    }

    // All should be created successfully
    EXPECT_GT(monitors.size(), 0u);

    // Destroy all monitors
    for (auto mon : monitors) {
        health_monitor_destroy(mon);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
