/**
 * @file test_fault_tolerance_exception_regression.cpp
 * @brief Regression tests for fault tolerance module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * Tests API contract stability and error code consistency for:
 * - nimcp_checkpoint.c
 * - nimcp_health_monitor.c
 * - nimcp_fast_recovery.c
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

class FaultToleranceExceptionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_clear_current();
        fast_recovery_reset_stats();

        snprintf(temp_dir, sizeof(temp_dir), "/tmp/nimcp_ft_reg_test_%d", getpid());
        mkdir(temp_dir, 0755);
    }

    void TearDown() override {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
        (void)system(cmd);

        if (monitor) {
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
// Checkpoint API Contract Regression Tests
//=============================================================================

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointSaveContractNullBrain) {
    // Contract: checkpoint_save returns false for NULL brain
    bool result = checkpoint_save(nullptr, "/tmp/test.ckpt");
    EXPECT_FALSE(result) << "API Contract: checkpoint_save must return false for NULL brain";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointSaveContractNullPath) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Contract: checkpoint_save returns false for NULL path
    bool result = checkpoint_save(brain, nullptr);
    EXPECT_FALSE(result) << "API Contract: checkpoint_save must return false for NULL path";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointLoadContractNullOutputParam) {
    // Contract: checkpoint_load returns false for NULL output parameter
    bool result = checkpoint_load(nullptr, "/tmp/test.ckpt");
    EXPECT_FALSE(result) << "API Contract: checkpoint_load must return false for NULL brain output";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointLoadContractNullPath) {
    brain_t loaded_brain = nullptr;

    // Contract: checkpoint_load returns false for NULL path
    bool result = checkpoint_load(&loaded_brain, nullptr);
    EXPECT_FALSE(result) << "API Contract: checkpoint_load must return false for NULL path";
    EXPECT_EQ(loaded_brain, nullptr) << "API Contract: brain output must remain NULL on failure";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointValidateContractNullPath) {
    // Contract: checkpoint_validate returns false for NULL path
    bool result = checkpoint_validate(nullptr);
    EXPECT_FALSE(result) << "API Contract: checkpoint_validate must return false for NULL path";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointListContractNullParams) {
    checkpoint_info_t* list = nullptr;
    uint32_t count = 0;

    // Contract: checkpoint_list returns false for NULL dir
    EXPECT_FALSE(checkpoint_list(nullptr, &list, &count))
        << "API Contract: checkpoint_list must return false for NULL dir";

    // Contract: checkpoint_list returns false for NULL list output
    EXPECT_FALSE(checkpoint_list(temp_dir, nullptr, &count))
        << "API Contract: checkpoint_list must return false for NULL list output";

    // Contract: checkpoint_list returns false for NULL count output
    EXPECT_FALSE(checkpoint_list(temp_dir, &list, nullptr))
        << "API Contract: checkpoint_list must return false for NULL count output";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointCleanupContractInvalidParams) {
    // Contract: checkpoint_cleanup_old returns false for NULL dir
    EXPECT_FALSE(checkpoint_cleanup_old(nullptr, 5))
        << "API Contract: checkpoint_cleanup_old must return false for NULL dir";

    // Contract: checkpoint_cleanup_old returns false for keep_count == 0
    EXPECT_FALSE(checkpoint_cleanup_old(temp_dir, 0))
        << "API Contract: checkpoint_cleanup_old must return false for keep_count == 0";
}

TEST_F(FaultToleranceExceptionRegressionTest, RecoveryAutoRestoreContractNullParams) {
    brain_t restored = nullptr;

    // Contract: recovery_auto_restore returns false for NULL brain output
    EXPECT_FALSE(recovery_auto_restore(nullptr, temp_dir))
        << "API Contract: recovery_auto_restore must return false for NULL brain output";

    // Contract: recovery_auto_restore returns false for NULL dir
    EXPECT_FALSE(recovery_auto_restore(&restored, nullptr))
        << "API Contract: recovery_auto_restore must return false for NULL dir";
}

TEST_F(FaultToleranceExceptionRegressionTest, RecoveryRollbackContractNullParams) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Contract: recovery_rollback returns false for NULL brain
    EXPECT_FALSE(recovery_rollback(nullptr, "/tmp/test.ckpt"))
        << "API Contract: recovery_rollback must return false for NULL brain";

    // Contract: recovery_rollback returns false for NULL path
    EXPECT_FALSE(recovery_rollback(brain, nullptr))
        << "API Contract: recovery_rollback must return false for NULL path";
}

TEST_F(FaultToleranceExceptionRegressionTest, RecoveryPartialContractNullParams) {
    brain_t recovered = nullptr;
    int level = 0;

    // Contract: recovery_partial returns false for NULL brain output
    EXPECT_FALSE(recovery_partial(nullptr, "/tmp/test.ckpt", &level))
        << "API Contract: recovery_partial must return false for NULL brain output";

    // Contract: recovery_partial returns false for NULL path
    EXPECT_FALSE(recovery_partial(&recovered, nullptr, &level))
        << "API Contract: recovery_partial must return false for NULL path";

    // Contract: recovery_partial returns false for NULL level output
    EXPECT_FALSE(recovery_partial(&recovered, "/tmp/test.ckpt", nullptr))
        << "API Contract: recovery_partial must return false for NULL level output";
}

//=============================================================================
// Health Monitor API Contract Regression Tests
//=============================================================================

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorCreateContractNullBrainId) {
    // Contract: health_monitor_create returns NULL for NULL brain_id
    health_monitor_t mon = health_monitor_create(nullptr);
    EXPECT_EQ(mon, nullptr) << "API Contract: health_monitor_create must return NULL for NULL brain_id";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorStartStopContractNullMonitor) {
    // Contract: health_monitor_start returns false for NULL monitor
    EXPECT_FALSE(health_monitor_start(nullptr))
        << "API Contract: health_monitor_start must return false for NULL monitor";

    // Contract: health_monitor_stop returns false for NULL monitor
    EXPECT_FALSE(health_monitor_stop(nullptr))
        << "API Contract: health_monitor_stop must return false for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorGetStatusContractNullParams) {
    health_status_snapshot_t status;

    // Contract: health_monitor_get_status returns false for NULL monitor
    EXPECT_FALSE(health_monitor_get_status(nullptr, &status))
        << "API Contract: health_monitor_get_status must return false for NULL monitor";

    monitor = health_monitor_create("regression_test");
    ASSERT_NE(monitor, nullptr);

    // Contract: health_monitor_get_status returns false for NULL status output
    EXPECT_FALSE(health_monitor_get_status(monitor, nullptr))
        << "API Contract: health_monitor_get_status must return false for NULL status output";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorGetScoreContractNullMonitor) {
    // Contract: health_monitor_get_score returns negative value for NULL monitor
    float score = health_monitor_get_score(nullptr);
    EXPECT_LT(score, 0.0f)
        << "API Contract: health_monitor_get_score must return negative for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorGetStatusLevelContractNullMonitor) {
    // Contract: health_monitor_get_status_level returns HEALTH_UNKNOWN for NULL monitor
    health_status_t status = health_monitor_get_status_level(nullptr);
    EXPECT_EQ(status, HEALTH_UNKNOWN)
        << "API Contract: health_monitor_get_status_level must return HEALTH_UNKNOWN for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorDetectAnomaliesContractNullParams) {
    anomaly_t anomalies[10];

    // Contract: health_monitor_detect_anomalies returns -1 for NULL monitor
    EXPECT_EQ(health_monitor_detect_anomalies(nullptr, anomalies, 10), -1)
        << "API Contract: health_monitor_detect_anomalies must return -1 for NULL monitor";

    monitor = health_monitor_create("anomaly_test");
    ASSERT_NE(monitor, nullptr);

    // Contract: health_monitor_detect_anomalies returns -1 for NULL anomalies output
    EXPECT_EQ(health_monitor_detect_anomalies(monitor, nullptr, 10), -1)
        << "API Contract: health_monitor_detect_anomalies must return -1 for NULL anomalies output";

    // Contract: health_monitor_detect_anomalies returns -1 for zero max_anomalies
    EXPECT_EQ(health_monitor_detect_anomalies(monitor, anomalies, 0), -1)
        << "API Contract: health_monitor_detect_anomalies must return -1 for zero max_anomalies";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorPredictFailureContractNullMonitor) {
    uint32_t ttf = 0;

    // Contract: health_monitor_predict_failure returns false for NULL monitor
    EXPECT_FALSE(health_monitor_predict_failure(nullptr, &ttf))
        << "API Contract: health_monitor_predict_failure must return false for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorClearResolvedContractNullMonitor) {
    // Contract: health_monitor_clear_resolved_anomalies returns 0 for NULL monitor
    EXPECT_EQ(health_monitor_clear_resolved_anomalies(nullptr), 0u)
        << "API Contract: health_monitor_clear_resolved_anomalies must return 0 for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorGetAnomalyCountContractNullMonitor) {
    // Contract: health_monitor_get_anomaly_count returns 0 for NULL monitor
    EXPECT_EQ(health_monitor_get_anomaly_count(nullptr, ANOMALY_MEMORY_LEAK), 0u)
        << "API Contract: health_monitor_get_anomaly_count must return 0 for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorBaselineContractNullMonitor) {
    // Contract: health_monitor_establish_baseline returns false for NULL monitor
    EXPECT_FALSE(health_monitor_establish_baseline(nullptr))
        << "API Contract: health_monitor_establish_baseline must return false for NULL monitor";

    // Contract: health_monitor_reset_baseline returns false for NULL monitor
    EXPECT_FALSE(health_monitor_reset_baseline(nullptr))
        << "API Contract: health_monitor_reset_baseline must return false for NULL monitor";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorSetThresholdContractInvalidParams) {
    // Contract: health_monitor_set_anomaly_threshold returns false for NULL monitor
    EXPECT_FALSE(health_monitor_set_anomaly_threshold(nullptr, 3.0))
        << "API Contract: health_monitor_set_anomaly_threshold must return false for NULL monitor";

    monitor = health_monitor_create("threshold_test");
    ASSERT_NE(monitor, nullptr);

    // Contract: health_monitor_set_anomaly_threshold returns false for negative threshold
    EXPECT_FALSE(health_monitor_set_anomaly_threshold(monitor, -1.0))
        << "API Contract: health_monitor_set_anomaly_threshold must return false for negative threshold";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorSetIntervalContractInvalidParams) {
    // Contract: health_monitor_set_interval returns false for NULL monitor
    EXPECT_FALSE(health_monitor_set_interval(nullptr, 1000))
        << "API Contract: health_monitor_set_interval must return false for NULL monitor";

    monitor = health_monitor_create("interval_test");
    ASSERT_NE(monitor, nullptr);

    // Contract: health_monitor_set_interval returns false for zero interval
    EXPECT_FALSE(health_monitor_set_interval(monitor, 0))
        << "API Contract: health_monitor_set_interval must return false for zero interval";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorExportJsonContractInvalidParams) {
    char buffer[1024];

    // Contract: health_monitor_export_json returns -1 for NULL monitor
    EXPECT_EQ(health_monitor_export_json(nullptr, buffer, sizeof(buffer)), -1)
        << "API Contract: health_monitor_export_json must return -1 for NULL monitor";

    monitor = health_monitor_create("json_test");
    ASSERT_NE(monitor, nullptr);

    // Contract: health_monitor_export_json returns -1 for NULL buffer
    EXPECT_EQ(health_monitor_export_json(monitor, nullptr, sizeof(buffer)), -1)
        << "API Contract: health_monitor_export_json must return -1 for NULL buffer";

    // Contract: health_monitor_export_json returns -1 for zero buffer size
    EXPECT_EQ(health_monitor_export_json(monitor, buffer, 0), -1)
        << "API Contract: health_monitor_export_json must return -1 for zero buffer size";
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthMonitorGetStatsContractNullParams) {
    operation_metric_t op_stats;
    memory_metric_t mem_stats;

    // Contract: health_monitor_get_operation_stats returns false for NULL monitor
    EXPECT_FALSE(health_monitor_get_operation_stats(nullptr, "test", &op_stats))
        << "API Contract: health_monitor_get_operation_stats must return false for NULL monitor";

    // Contract: health_monitor_get_memory_stats returns false for NULL monitor
    EXPECT_FALSE(health_monitor_get_memory_stats(nullptr, &mem_stats))
        << "API Contract: health_monitor_get_memory_stats must return false for NULL monitor";

    monitor = health_monitor_create("stats_test");
    ASSERT_NE(monitor, nullptr);

    // Contract: health_monitor_get_operation_stats returns false for NULL operation
    EXPECT_FALSE(health_monitor_get_operation_stats(monitor, nullptr, &op_stats))
        << "API Contract: health_monitor_get_operation_stats must return false for NULL operation";

    // Contract: health_monitor_get_operation_stats returns false for NULL stats output
    EXPECT_FALSE(health_monitor_get_operation_stats(monitor, "test", nullptr))
        << "API Contract: health_monitor_get_operation_stats must return false for NULL stats output";

    // Contract: health_monitor_get_memory_stats returns false for NULL stats output
    EXPECT_FALSE(health_monitor_get_memory_stats(monitor, nullptr))
        << "API Contract: health_monitor_get_memory_stats must return false for NULL stats output";
}

//=============================================================================
// Fast Recovery API Contract Regression Tests
//=============================================================================

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryIsApplicableContractNullContext) {
    // Contract: fast_recovery_is_applicable returns FAST_RECOVERY_NONE for NULL context
    fast_recovery_type_t type = fast_recovery_is_applicable(nullptr);
    EXPECT_EQ(type, FAST_RECOVERY_NONE)
        << "API Contract: fast_recovery_is_applicable must return FAST_RECOVERY_NONE for NULL context";
}

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryExecuteContractNullBrain) {
    // Contract: Brain-dependent recovery types return NOT_APPLICABLE for NULL brain

    // CLEAR_NAN requires brain
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_CLEAR_NAN, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE)
        << "API Contract: CLEAR_NAN must return NOT_APPLICABLE for NULL brain";

    // CLIP_GRADIENTS requires brain
    result = fast_recovery_execute(FAST_RECOVERY_CLIP_GRADIENTS, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE)
        << "API Contract: CLIP_GRADIENTS must return NOT_APPLICABLE for NULL brain";

    // CLEAR_CACHE requires brain
    result = fast_recovery_execute(FAST_RECOVERY_CLEAR_CACHE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE)
        << "API Contract: CLEAR_CACHE must return NOT_APPLICABLE for NULL brain";

    // RESET_STATE requires brain
    result = fast_recovery_execute(FAST_RECOVERY_RESET_STATE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE)
        << "API Contract: RESET_STATE must return NOT_APPLICABLE for NULL brain";
}

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryExecuteContractBrainIndependent) {
    // Contract: Brain-independent recovery types succeed without brain

    // RESET_FPU does not require brain
    fast_recovery_result_t result = fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS)
        << "API Contract: RESET_FPU must succeed without brain";

    // FLUSH_BUFFERS does not require brain
    result = fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS)
        << "API Contract: FLUSH_BUFFERS must succeed without brain";

    // RESET_COUNTER does not require brain
    result = fast_recovery_execute(FAST_RECOVERY_RESET_COUNTER, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS)
        << "API Contract: RESET_COUNTER must succeed without brain";

    // TRIGGER_GC does not require brain
    result = fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS)
        << "API Contract: TRIGGER_GC must succeed without brain";
}

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryExecuteWithContextContractNullContext) {
    // Contract: fast_recovery_execute_with_context returns NOT_APPLICABLE for NULL context
    fast_recovery_result_t result = fast_recovery_execute_with_context(nullptr, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE)
        << "API Contract: fast_recovery_execute_with_context must return NOT_APPLICABLE for NULL context";
    EXPECT_STREQ(result.message, "Invalid context")
        << "API Contract: Message must indicate invalid context";
}

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryValidateResultContractNullResult) {
    // Contract: fast_recovery_validate_result returns false for NULL result
    EXPECT_FALSE(fast_recovery_validate_result(nullptr))
        << "API Contract: fast_recovery_validate_result must return false for NULL result";
}

//=============================================================================
// Error Code Consistency Tests
//=============================================================================

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointErrorMessagesNonEmpty) {
    // Trigger an error
    checkpoint_clear_error();
    (void)checkpoint_validate("/nonexistent/path/file.ckpt");

    // Contract: Error messages should be non-empty after failure
    const char* error = checkpoint_get_error();
    EXPECT_NE(error, nullptr)
        << "API Contract: checkpoint_get_error must return non-NULL after error";
    EXPECT_GT(strlen(error), 0u)
        << "API Contract: Error message must be non-empty after failure";
}

TEST_F(FaultToleranceExceptionRegressionTest, CheckpointClearErrorContract) {
    // Trigger an error first
    (void)checkpoint_validate("/nonexistent/path/file.ckpt");

    // Clear error
    checkpoint_clear_error();

    // Contract: Error should be empty after clear
    const char* error = checkpoint_get_error();
    EXPECT_NE(error, nullptr)
        << "API Contract: checkpoint_get_error must return non-NULL";
    EXPECT_EQ(strlen(error), 0u)
        << "API Contract: Error message must be empty after clear";
}

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryTypeNamesConsistent) {
    // Contract: Type names must be consistent strings
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_CLEAR_NAN), "CLEAR_NAN");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_CLIP_GRADIENTS), "CLIP_GRADIENTS");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_RESET_FPU), "RESET_FPU");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_CLEAR_CACHE), "CLEAR_CACHE");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_FLUSH_BUFFERS), "FLUSH_BUFFERS");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_RESET_STATE), "RESET_STATE");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_RESET_COUNTER), "RESET_COUNTER");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_TRIGGER_GC), "TRIGGER_GC");
}

TEST_F(FaultToleranceExceptionRegressionTest, FastRecoveryStatusNamesConsistent) {
    // Contract: Status names must be consistent strings
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_SUCCESS), "SUCCESS");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_PARTIAL), "PARTIAL");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_NOT_APPLICABLE), "NOT_APPLICABLE");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_FAILED), "FAILED");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_TIMEOUT), "TIMEOUT");
}

TEST_F(FaultToleranceExceptionRegressionTest, HealthStatusNamesConsistent) {
    // Contract: Health status names must be consistent strings
    EXPECT_STREQ(health_status_to_string(HEALTH_EXCELLENT), "EXCELLENT");
    EXPECT_STREQ(health_status_to_string(HEALTH_GOOD), "GOOD");
    EXPECT_STREQ(health_status_to_string(HEALTH_FAIR), "FAIR");
    EXPECT_STREQ(health_status_to_string(HEALTH_POOR), "POOR");
    EXPECT_STREQ(health_status_to_string(HEALTH_CRITICAL), "CRITICAL");
    EXPECT_STREQ(health_status_to_string(HEALTH_UNKNOWN), "UNKNOWN");
}

TEST_F(FaultToleranceExceptionRegressionTest, AnomalyTypeNamesConsistent) {
    // Contract: Anomaly type names must be consistent strings
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_NONE), "NONE");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_MEMORY_LEAK), "MEMORY_LEAK");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_PERFORMANCE_DEGRADATION), "PERFORMANCE_DEGRADATION");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_ERROR_SPIKE), "ERROR_SPIKE");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_THROUGHPUT_DROP), "THROUGHPUT_DROP");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_CACHE_THRASHING), "CACHE_THRASHING");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_RESOURCE_EXHAUSTION), "RESOURCE_EXHAUSTION");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_NUMERICAL_INSTABILITY), "NUMERICAL_INSTABILITY");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_THREAD_CONTENTION), "THREAD_CONTENTION");
}

TEST_F(FaultToleranceExceptionRegressionTest, AnomalySeverityNamesConsistent) {
    // Contract: Anomaly severity names must be consistent strings
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_CRITICAL), "CRITICAL");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
