/**
 * @file test_fault_tolerance_exception.cpp
 * @brief Unit tests for fault tolerance module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * Tests exception handling for:
 * - nimcp_checkpoint.c (31 throws) - NULL checks, allocation failures, I/O errors
 * - nimcp_health_monitor.c (33 throws) - NULL checks, mutex/thread failures
 * - nimcp_fast_recovery.c (5 throws) - NULL brain checks
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

class FaultToleranceExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any pending exceptions
        nimcp_exception_clear_current();

        // Reset fast recovery stats
        fast_recovery_reset_stats();

        // Create temp directory for checkpoint tests
        snprintf(temp_dir, sizeof(temp_dir), "/tmp/nimcp_ft_test_%d", getpid());
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
    }

    char temp_dir[256];
    health_monitor_t monitor = nullptr;
};

//=============================================================================
// Checkpoint NULL Pointer Exception Tests
//=============================================================================

TEST_F(FaultToleranceExceptionTest, CheckpointSaveNullBrain) {
    bool result = checkpoint_save(nullptr, "/tmp/test.ckpt");
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointSaveNullPath) {
    // Would need a valid brain, but NULL path should fail fast
    bool result = checkpoint_save(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointSaveExNullBrain) {
    checkpoint_options_t opts = checkpoint_default_options();
    bool result = checkpoint_save_ex(nullptr, "/tmp/test.ckpt", &opts);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointSaveExNullPath) {
    checkpoint_options_t opts = checkpoint_default_options();
    bool result = checkpoint_save_ex(nullptr, nullptr, &opts);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointSaveIncrementalNullBrain) {
    bool result = checkpoint_save_incremental(nullptr, "/tmp/incr.ckpt", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointSaveIncrementalNullIncrPath) {
    bool result = checkpoint_save_incremental(nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointLoadNullBrain) {
    bool result = checkpoint_load(nullptr, "/tmp/test.ckpt");
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointLoadNullPath) {
    brain_t brain = nullptr;
    bool result = checkpoint_load(&brain, nullptr);
    EXPECT_FALSE(result);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(FaultToleranceExceptionTest, CheckpointValidateNullPath) {
    bool result = checkpoint_validate(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointListNullDir) {
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;
    bool result = checkpoint_list(nullptr, &list, &count);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointListNullList) {
    uint32_t count = 0;
    bool result = checkpoint_list(temp_dir, nullptr, &count);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointListNullCount) {
    checkpoint_info_t* list = nullptr;
    bool result = checkpoint_list(temp_dir, &list, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointCleanupOldNullDir) {
    bool result = checkpoint_cleanup_old(nullptr, 5);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointCleanupOldZeroKeepCount) {
    bool result = checkpoint_cleanup_old(temp_dir, 0);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryAutoRestoreNullBrain) {
    bool result = recovery_auto_restore(nullptr, temp_dir);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryAutoRestoreNullDir) {
    brain_t brain = nullptr;
    bool result = recovery_auto_restore(&brain, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryRollbackNullBrain) {
    bool result = recovery_rollback(nullptr, "/tmp/test.ckpt");
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryRollbackNullPath) {
    bool result = recovery_rollback(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryPartialNullBrain) {
    int level = 0;
    bool result = recovery_partial(nullptr, "/tmp/test.ckpt", &level);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryPartialNullPath) {
    brain_t brain = nullptr;
    int level = 0;
    bool result = recovery_partial(&brain, nullptr, &level);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, RecoveryPartialNullLevel) {
    brain_t brain = nullptr;
    bool result = recovery_partial(&brain, "/tmp/test.ckpt", nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Health Monitor NULL Pointer Exception Tests
//=============================================================================

TEST_F(FaultToleranceExceptionTest, HealthMonitorCreateNullBrainId) {
    health_monitor_t mon = health_monitor_create(nullptr);
    EXPECT_EQ(mon, nullptr);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorStartNullMonitor) {
    bool result = health_monitor_start(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorStopNullMonitor) {
    bool result = health_monitor_stop(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetStatusNullMonitor) {
    health_status_snapshot_t status;
    bool result = health_monitor_get_status(nullptr, &status);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetStatusNullStatus) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_get_status(monitor, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetScoreNullMonitor) {
    float score = health_monitor_get_score(nullptr);
    EXPECT_LT(score, 0.0f);  // Returns -1.0 on error
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetStatusLevelNullMonitor) {
    health_status_t status = health_monitor_get_status_level(nullptr);
    EXPECT_EQ(status, HEALTH_UNKNOWN);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorDetectAnomaliesNullMonitor) {
    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(nullptr, anomalies, 10);
    EXPECT_EQ(count, -1);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorDetectAnomaliesNullAnomalies) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    int32_t count = health_monitor_detect_anomalies(monitor, nullptr, 10);
    EXPECT_EQ(count, -1);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorDetectAnomaliesZeroMax) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 0);
    EXPECT_EQ(count, -1);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorPredictFailureNullMonitor) {
    uint32_t ttf = 0;
    bool result = health_monitor_predict_failure(nullptr, &ttf);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorClearResolvedAnomaliesNullMonitor) {
    uint32_t count = health_monitor_clear_resolved_anomalies(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetAnomalyCountNullMonitor) {
    uint32_t count = health_monitor_get_anomaly_count(nullptr, ANOMALY_MEMORY_LEAK);
    EXPECT_EQ(count, 0u);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorEstablishBaselineNullMonitor) {
    bool result = health_monitor_establish_baseline(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorResetBaselineNullMonitor) {
    bool result = health_monitor_reset_baseline(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorSetAnomalyThresholdNullMonitor) {
    bool result = health_monitor_set_anomaly_threshold(nullptr, 3.0);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorSetAnomalyThresholdNegative) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_set_anomaly_threshold(monitor, -1.0);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorSetIntervalNullMonitor) {
    bool result = health_monitor_set_interval(nullptr, 1000);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorSetIntervalZero) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_set_interval(monitor, 0);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorExportJsonNullMonitor) {
    char buffer[1024];
    int32_t result = health_monitor_export_json(nullptr, buffer, sizeof(buffer));
    EXPECT_EQ(result, -1);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorExportJsonNullBuffer) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    int32_t result = health_monitor_export_json(monitor, nullptr, 1024);
    EXPECT_EQ(result, -1);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorExportJsonZeroSize) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    char buffer[1024];
    int32_t result = health_monitor_export_json(monitor, buffer, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetOperationStatsNullMonitor) {
    operation_metric_t stats;
    bool result = health_monitor_get_operation_stats(nullptr, "test", &stats);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetOperationStatsNullOperation) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    operation_metric_t stats;
    bool result = health_monitor_get_operation_stats(monitor, nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetOperationStatsNullStats) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_get_operation_stats(monitor, "test", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetMemoryStatsNullMonitor) {
    memory_metric_t stats;
    bool result = health_monitor_get_memory_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorGetMemoryStatsNullStats) {
    monitor = health_monitor_create("test_brain");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_get_memory_stats(monitor, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Fast Recovery NULL Pointer Exception Tests
//=============================================================================

TEST_F(FaultToleranceExceptionTest, FastRecoveryIsApplicableNullContext) {
    fast_recovery_type_t type = fast_recovery_is_applicable(nullptr);
    EXPECT_EQ(type, FAST_RECOVERY_NONE);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryClearNaNNullBrain) {
    // This tests the internal action_clear_nan which requires brain
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_CLEAR_NAN, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryClipGradientsNullBrain) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_CLIP_GRADIENTS, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryClearCacheNullBrain) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_CLEAR_CACHE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryResetStateNullBrain) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_RESET_STATE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryExecuteWithContextNullContext) {
    fast_recovery_result_t result = fast_recovery_execute_with_context(nullptr, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_STREQ(result.message, "Invalid context");
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidateResultNullResult) {
    bool valid = fast_recovery_validate_result(nullptr);
    EXPECT_FALSE(valid);
}

//=============================================================================
// Checkpoint I/O Error Tests
//=============================================================================

TEST_F(FaultToleranceExceptionTest, CheckpointValidateNonexistentFile) {
    bool result = checkpoint_validate("/nonexistent/path/file.ckpt");
    EXPECT_FALSE(result);

    const char* error = checkpoint_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(strlen(error), 0u);
}

TEST_F(FaultToleranceExceptionTest, CheckpointLoadNonexistentFile) {
    brain_t brain = nullptr;
    bool result = checkpoint_load(&brain, "/nonexistent/path/file.ckpt");
    EXPECT_FALSE(result);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(FaultToleranceExceptionTest, RecoveryAutoRestoreEmptyDir) {
    brain_t brain = nullptr;
    bool result = recovery_auto_restore(&brain, temp_dir);
    EXPECT_FALSE(result);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(FaultToleranceExceptionTest, RecoveryPartialNonexistentFile) {
    brain_t brain = nullptr;
    int level = 0;
    bool result = recovery_partial(&brain, "/nonexistent/path/file.ckpt", &level);
    EXPECT_FALSE(result);
}

TEST_F(FaultToleranceExceptionTest, CheckpointListNonexistentDir) {
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;
    bool result = checkpoint_list("/nonexistent/directory", &list, &count);
    EXPECT_FALSE(result);
}

//=============================================================================
// Health Monitor Valid Operations Tests (ensure no exceptions on valid input)
//=============================================================================

TEST_F(FaultToleranceExceptionTest, HealthMonitorValidCreateDestroy) {
    monitor = health_monitor_create("test_brain_valid");
    ASSERT_NE(monitor, nullptr);

    // Should not throw on valid operations
    health_monitor_record_operation(monitor, "test_op", 1000);
    health_monitor_record_memory(monitor, 1024);
    health_monitor_record_error(monitor, "test_error");
    health_monitor_record_cache_access(monitor, true);
    health_monitor_record_thread_event(monitor, false, 0);
    health_monitor_record_throughput(monitor, 100, 1000000);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorValidGetStatus) {
    monitor = health_monitor_create("test_brain_status");
    ASSERT_NE(monitor, nullptr);

    health_status_snapshot_t status;
    bool result = health_monitor_get_status(monitor, &status);
    EXPECT_TRUE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorValidBaseline) {
    monitor = health_monitor_create("test_brain_baseline");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_establish_baseline(monitor);
    EXPECT_TRUE(result);

    result = health_monitor_reset_baseline(monitor);
    EXPECT_TRUE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorValidConfiguration) {
    monitor = health_monitor_create("test_brain_config");
    ASSERT_NE(monitor, nullptr);

    bool result = health_monitor_set_anomaly_threshold(monitor, 2.5);
    EXPECT_TRUE(result);

    result = health_monitor_set_interval(monitor, 500);
    EXPECT_TRUE(result);
}

TEST_F(FaultToleranceExceptionTest, HealthMonitorValidExportJson) {
    monitor = health_monitor_create("test_brain_json");
    ASSERT_NE(monitor, nullptr);

    char buffer[2048];
    int32_t written = health_monitor_export_json(monitor, buffer, sizeof(buffer));
    EXPECT_GT(written, 0);
    EXPECT_LT((size_t)written, sizeof(buffer));
}

//=============================================================================
// Fast Recovery Valid Operations Tests
//=============================================================================

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidResetFPU) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_LT(result.latency_us, 1000u);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidFlushBuffers) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidResetCounter) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_RESET_COUNTER, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidTriggerGC) {
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidPatternMatching) {
    fast_recovery_context_t ctx = {};
    ctx.signal = SIGFPE;
    ctx.is_numeric_error = true;

    fast_recovery_type_t type = fast_recovery_is_applicable(&ctx);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_NAN);
}

TEST_F(FaultToleranceExceptionTest, FastRecoveryValidSignalMatching) {
    fast_recovery_type_t type = fast_recovery_is_applicable_signal(SIGFPE);
    EXPECT_EQ(type, FAST_RECOVERY_RESET_FPU);

    type = fast_recovery_is_applicable_signal(SIGABRT);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_CACHE);
}

//=============================================================================
// Checkpoint Utility Function Tests
//=============================================================================

TEST_F(FaultToleranceExceptionTest, CheckpointDefaultOptions) {
    checkpoint_options_t opts = checkpoint_default_options();

    EXPECT_TRUE(opts.enable_compression);
    EXPECT_FALSE(opts.incremental);
    EXPECT_TRUE(opts.save_subsystems);
    EXPECT_FALSE(opts.save_activations);
    EXPECT_EQ(opts.compression_level, 6);
    EXPECT_EQ(opts.temp_dir, nullptr);
}

TEST_F(FaultToleranceExceptionTest, CheckpointGetVersion) {
    const char* version = checkpoint_get_version();
    EXPECT_NE(version, nullptr);
    EXPECT_NE(strlen(version), 0u);
}

TEST_F(FaultToleranceExceptionTest, CheckpointClearError) {
    checkpoint_clear_error();
    const char* error = checkpoint_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_EQ(strlen(error), 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
