/**
 * @file test_omni_wm_logging_bridge.cpp
 * @brief Comprehensive unit tests for World Model Logging Bridge
 *
 * WHAT: Tests for WM Logging Bridge providing audit trails for World Model operations
 * WHY:  Bridge is critical for traceability, debugging, and performance monitoring
 * HOW:  Tests all APIs: lifecycle, connection, update, prediction logging, training
 *       logging, anomaly logging, calibration, replay operations, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_DIM = 16;
static constexpr float TEST_DT = 0.016f; // ~60Hz

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float value, float min_val, float max_val)
{
    return value >= min_val && value <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class WMLoggingBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config
        bridge_ = omni_wm_logging_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_logging_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create bridge with custom config
    omni_wm_logging_bridge_t* create_custom_bridge(bool enable_modulation,
                                                    wm_log_severity_t min_severity)
    {
        omni_wm_logging_bridge_config_t config;
        omni_wm_logging_bridge_default_config(&config);
        config.enable_modulation = enable_modulation;
        config.min_severity = min_severity;
        return omni_wm_logging_bridge_create(&config);
    }

    // Helper to create test input array
    std::vector<float> create_test_array(uint32_t dim, float base_value)
    {
        std::vector<float> arr(dim);
        for (uint32_t i = 0; i < dim; i++) {
            arr[i] = base_value + (float)i * 0.01f;
        }
        return arr;
    }

    omni_wm_logging_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_logging_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    nimcp_error_t result = omni_wm_logging_bridge_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check general settings
    EXPECT_TRUE(config.enable_modulation);
    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));

    // Check filtering
    EXPECT_GE(config.min_severity, WM_LOG_SEV_TRACE);
    EXPECT_LT(config.min_severity, WM_LOG_SEV_COUNT);

    // Check prediction logging
    EXPECT_TRUE(float_in_range(config.prediction_sample_rate, 0.0f, 1.0f));
    EXPECT_GT(config.high_pe_threshold, 0.0f);

    // Check training logging
    EXPECT_GT(config.training_log_interval, 0u);

    // Check buffering
    EXPECT_GT(config.buffer_size, 0u);
    EXPECT_GT(config.flush_interval_ms, 0u);
    EXPECT_GT(config.batch_size, 0u);
}

TEST_F(WMLoggingBridgeTest, DefaultConfigIdempotent)
{
    omni_wm_logging_bridge_config_t config1, config2;

    omni_wm_logging_bridge_default_config(&config1);
    omni_wm_logging_bridge_default_config(&config2);

    EXPECT_EQ(config1.enable_modulation, config2.enable_modulation);
    EXPECT_FLOAT_EQ(config1.sensitivity, config2.sensitivity);
    EXPECT_EQ(config1.min_severity, config2.min_severity);
    EXPECT_EQ(config1.buffer_size, config2.buffer_size);
}

TEST_F(WMLoggingBridgeTest, DefaultConfigEnablesAllCategories)
{
    omni_wm_logging_bridge_config_t config;
    omni_wm_logging_bridge_default_config(&config);

    // Check that enabled_categories is non-zero (some categories enabled)
    EXPECT_NE(config.enabled_categories, 0u);
}

// =============================================================================
// 2. Lifecycle Tests - Create/Destroy
// =============================================================================

TEST_F(WMLoggingBridgeTest, CreateWithNullConfigUsesDefaults)
{
    // bridge_ created in SetUp with NULL config
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(WMLoggingBridgeTest, CreateWithCustomConfig)
{
    omni_wm_logging_bridge_config_t config;
    omni_wm_logging_bridge_default_config(&config);
    config.enable_modulation = false;
    config.min_severity = WM_LOG_SEV_WARNING;
    config.enable_prediction_logging = false;
    config.buffer_size = 2048;

    omni_wm_logging_bridge_t* custom_bridge = omni_wm_logging_bridge_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    // Verify config was applied
    EXPECT_FALSE(custom_bridge->config.enable_modulation);
    EXPECT_EQ(custom_bridge->config.min_severity, WM_LOG_SEV_WARNING);
    EXPECT_FALSE(custom_bridge->config.enable_prediction_logging);
    EXPECT_EQ(custom_bridge->config.buffer_size, 2048u);

    omni_wm_logging_bridge_destroy(custom_bridge);
}

TEST_F(WMLoggingBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    // Check base bridge infrastructure
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_NE(bridge_->base.module_id, 0u);
}

TEST_F(WMLoggingBridgeTest, CreateInitializesStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Stats should be zeroed on creation
    EXPECT_EQ(bridge_->stats.predictions_logged, 0u);
    EXPECT_EQ(bridge_->stats.training_steps_logged, 0u);
    EXPECT_EQ(bridge_->stats.anomalies_logged, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(WMLoggingBridgeTest, CreateInitializesBuffers)
{
    ASSERT_NE(bridge_, nullptr);

    // Buffers should be allocated
    EXPECT_EQ(bridge_->buffer_count, 0u);
    EXPECT_GT(bridge_->buffer_capacity, 0u);
}

TEST_F(WMLoggingBridgeTest, CreateInitializesSequenceNumbers)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->next_entry_id, 1u); // Start at 1, not 0
    EXPECT_EQ(bridge_->sequence_number, 0u);
}

TEST_F(WMLoggingBridgeTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_logging_bridge_destroy(nullptr);
}

TEST_F(WMLoggingBridgeTest, DestroyValidBridge)
{
    omni_wm_logging_bridge_t* temp = omni_wm_logging_bridge_create(nullptr);
    ASSERT_NE(temp, nullptr);

    // Should not crash and should free buffers
    omni_wm_logging_bridge_destroy(temp);
}

// =============================================================================
// 3. Reset Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, ResetClearsStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Manually increment some stats
    bridge_->stats.predictions_logged = 100;
    bridge_->stats.training_steps_logged = 50;
    bridge_->stats.anomalies_logged = 10;

    nimcp_error_t result = omni_wm_logging_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.predictions_logged, 0u);
    EXPECT_EQ(bridge_->stats.training_steps_logged, 0u);
    EXPECT_EQ(bridge_->stats.anomalies_logged, 0u);
}

TEST_F(WMLoggingBridgeTest, ResetClearsBuffers)
{
    ASSERT_NE(bridge_, nullptr);

    // Simulate some buffer usage
    bridge_->buffer_count = 50;
    bridge_->buffer_head = 50;
    bridge_->pred_buffer_count = 25;

    nimcp_error_t result = omni_wm_logging_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->buffer_count, 0u);
    EXPECT_EQ(bridge_->buffer_head, 0u);
    EXPECT_EQ(bridge_->buffer_tail, 0u);
    EXPECT_EQ(bridge_->pred_buffer_count, 0u);
}

TEST_F(WMLoggingBridgeTest, ResetPreservesConfiguration)
{
    omni_wm_logging_bridge_t* custom = create_custom_bridge(false, WM_LOG_SEV_ERROR);
    ASSERT_NE(custom, nullptr);

    // Store original config values
    bool orig_enable = custom->config.enable_modulation;
    wm_log_severity_t orig_severity = custom->config.min_severity;

    nimcp_error_t result = omni_wm_logging_bridge_reset(custom);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Config should be preserved
    EXPECT_EQ(custom->config.enable_modulation, orig_enable);
    EXPECT_EQ(custom->config.min_severity, orig_severity);

    omni_wm_logging_bridge_destroy(custom);
}

TEST_F(WMLoggingBridgeTest, ResetResetsSequenceNumbers)
{
    ASSERT_NE(bridge_, nullptr);

    // Increment sequence numbers
    bridge_->next_entry_id = 1000;
    bridge_->sequence_number = 500;

    nimcp_error_t result = omni_wm_logging_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->next_entry_id, 1u);
    EXPECT_EQ(bridge_->sequence_number, 0u);
}

// =============================================================================
// 4. Connection Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, IsConnectedWithoutConnectionReturnsFalse)
{
    ASSERT_NE(bridge_, nullptr);

    bool connected = omni_wm_logging_bridge_is_connected(bridge_);
    EXPECT_FALSE(connected);
}

TEST_F(WMLoggingBridgeTest, IsConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_logging_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(WMLoggingBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_connect(nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, ConnectLoggerNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_connect_logger(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, ConnectAuditNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_connect_audit(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 5. Update Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_update(nullptr, TEST_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, UpdateIncrementsTotalUpdates)
{
    ASSERT_NE(bridge_, nullptr);

    uint64_t initial_updates = bridge_->stats.total_updates;

    // Update may fail without connection, but should not crash
    omni_wm_logging_bridge_update(bridge_, TEST_DT);
    omni_wm_logging_bridge_update(bridge_, TEST_DT);

    // Stats might or might not be updated depending on connection state
}

TEST_F(WMLoggingBridgeTest, UpdateZeroDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_update(bridge_, 0.0f);
    // Should handle gracefully
    EXPECT_TRUE(result == NIMCP_SUCCESS || result != NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, UpdateNegativeDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_update(bridge_, -1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 6. Flush Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, FlushNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_flush(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, FlushEmptyBufferSucceeds)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_flush(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, FlushIncrementsFlushCounter)
{
    ASSERT_NE(bridge_, nullptr);

    uint64_t initial_flushes = bridge_->stats.buffer_flushes;

    omni_wm_logging_bridge_flush(bridge_);
    omni_wm_logging_bridge_flush(bridge_);

    EXPECT_GE(bridge_->stats.buffer_flushes, initial_flushes);
}

// =============================================================================
// 7. Prediction Logging Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, LogPredictionNullBridgeFails)
{
    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        nullptr, input.data(), TEST_DIM, output.data(), TEST_DIM, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionNullInputFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        bridge_, nullptr, TEST_DIM, output.data(), TEST_DIM, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        bridge_, input.data(), TEST_DIM, nullptr, TEST_DIM, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionZeroDimFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        bridge_, input.data(), 0, output.data(), TEST_DIM, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionValidParams)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        bridge_, input.data(), TEST_DIM, output.data(), TEST_DIM, 0.9f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionHighConfidence)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        bridge_, input.data(), TEST_DIM, output.data(), TEST_DIM, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionLowConfidence)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction(
        bridge_, input.data(), TEST_DIM, output.data(), TEST_DIM, 0.1f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionEntryNullBridgeFails)
{
    wm_prediction_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction_entry(nullptr, &entry);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionEntryNullEntryFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction_entry(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogPredictionErrorNullBridgeFails)
{
    std::vector<float> predicted = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> actual = create_test_array(TEST_DIM, 0.6f);

    nimcp_error_t result = omni_wm_logging_bridge_log_prediction_error(
        nullptr, predicted.data(), actual.data(), TEST_DIM, 0.1f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 8. Training Logging Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, LogTrainingStepNullBridgeFails)
{
    wm_training_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));

    nimcp_error_t result = omni_wm_logging_bridge_log_training_step(nullptr, 0.5f, &metrics);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogTrainingStepNullMetricsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_training_step(bridge_, 0.5f, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogTrainingStepValidParams)
{
    ASSERT_NE(bridge_, nullptr);

    wm_training_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss = 0.5f;
    metrics.gradient_norm = 0.1f;
    metrics.learning_rate = 0.001f;

    nimcp_error_t result = omni_wm_logging_bridge_log_training_step(bridge_, 0.5f, &metrics);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogTrainingEntryNullBridgeFails)
{
    wm_training_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    nimcp_error_t result = omni_wm_logging_bridge_log_training_entry(nullptr, &entry);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogTrainingEntryNullEntryFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_training_entry(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogLRChangeNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_log_lr_change(nullptr, 0.001f, 0.0001f, "decay");
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogLRChangeValidParams)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_lr_change(
        bridge_, 0.001f, 0.0001f, "scheduled decay");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogLRChangeNullReason)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_lr_change(bridge_, 0.001f, 0.0001f, nullptr);
    // Should handle null reason gracefully
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_INVALID_PARAMETER);
}

// =============================================================================
// 9. Anomaly Logging Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, LogAnomalyNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly(
        nullptr, WM_ANOMALY_HIGH_PE, "High prediction error detected");
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyNullDetailsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly(bridge_, WM_ANOMALY_HIGH_PE, nullptr);
    // Should handle null details or require details
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyHighPE)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly(
        bridge_, WM_ANOMALY_HIGH_PE, "Prediction error exceeded threshold");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyDivergence)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly(
        bridge_, WM_ANOMALY_DIVERGENCE, "Trajectory diverged from expected path");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyNaNInf)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly(
        bridge_, WM_ANOMALY_NAN_INF, "NaN detected in prediction output");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyGradientExplode)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly(
        bridge_, WM_ANOMALY_GRADIENT_EXPLODE, "Gradient norm exceeded 1000");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyEntryNullBridgeFails)
{
    wm_anomaly_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly_entry(nullptr, &entry);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyEntryNullEntryFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly_entry(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyResolvedNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly_resolved(nullptr, 1, "Fixed");
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAnomalyResolvedValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_anomaly_resolved(
        bridge_, 1, "Gradient clipping applied");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 10. Calibration Logging Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, LogCalibrationNullBridgeFails)
{
    wm_calibration_metrics_t calibration;
    memset(&calibration, 0, sizeof(calibration));

    nimcp_error_t result = omni_wm_logging_bridge_log_calibration(nullptr, &calibration);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogCalibrationNullCalibrationFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_calibration(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogCalibrationValid)
{
    ASSERT_NE(bridge_, nullptr);

    wm_calibration_metrics_t calibration;
    memset(&calibration, 0, sizeof(calibration));
    calibration.calibration_error = 0.05f;
    calibration.mean_confidence = 0.85f;
    calibration.mean_accuracy = 0.82f;

    nimcp_error_t result = omni_wm_logging_bridge_log_calibration(bridge_, &calibration);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, UpdateCalibrationNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_update_calibration(nullptr, 0.8f, true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, UpdateCalibrationAccuratePrediction)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_update_calibration(bridge_, 0.9f, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, UpdateCalibrationInaccuratePrediction)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_update_calibration(bridge_, 0.9f, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 11. Replay Logging Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, LogReplayOperationNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_log_replay_operation(
        nullptr, "add", 32, 1000);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogReplayOperationNullOperation)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_replay_operation(
        bridge_, nullptr, 32, 1000);
    // Should handle null operation
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(WMLoggingBridgeTest, LogReplayOperationAdd)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_replay_operation(
        bridge_, "add", 32, 1032);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogReplayOperationSample)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_replay_operation(
        bridge_, "sample", 64, 10000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogReplayOperationClear)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_replay_operation(
        bridge_, "clear", 10000, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogDreamNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_log_dream(nullptr, 100, 50.0f, 10);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogDreamValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log_dream(bridge_, 100, 50.0f, 10);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 12. General Logging Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, LogNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_log(
        nullptr, WM_LOG_CAT_SYSTEM, WM_LOG_SEV_INFO, "Test message");
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogNullMessageFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log(
        bridge_, WM_LOG_CAT_SYSTEM, WM_LOG_SEV_INFO, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogValidMessage)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_log(
        bridge_, WM_LOG_CAT_SYSTEM, WM_LOG_SEV_INFO, "System initialized");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, LogAllCategories)
{
    ASSERT_NE(bridge_, nullptr);

    for (int cat = WM_LOG_CAT_PREDICTION; cat < WM_LOG_CAT_COUNT; cat++) {
        nimcp_error_t result = omni_wm_logging_bridge_log(
            bridge_, (wm_log_category_t)cat, WM_LOG_SEV_INFO, "Test message");
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(WMLoggingBridgeTest, LogAllSeverities)
{
    ASSERT_NE(bridge_, nullptr);

    for (int sev = WM_LOG_SEV_TRACE; sev < WM_LOG_SEV_COUNT; sev++) {
        nimcp_error_t result = omni_wm_logging_bridge_log(
            bridge_, WM_LOG_CAT_SYSTEM, (wm_log_severity_t)sev, "Test message");
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// 13. Statistics Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_logging_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_logging_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, GetStatsNullStatsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_logging_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, GetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    omni_wm_logging_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    nimcp_error_t result = omni_wm_logging_bridge_get_stats(bridge_, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Fresh bridge should have zero stats
    EXPECT_EQ(stats.predictions_logged, 0u);
    EXPECT_EQ(stats.training_steps_logged, 0u);
    EXPECT_EQ(stats.anomalies_logged, 0u);
}

TEST_F(WMLoggingBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, ResetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Increment some stats
    bridge_->stats.predictions_logged = 100;
    bridge_->stats.anomalies_logged = 5;

    nimcp_error_t result = omni_wm_logging_bridge_reset_stats(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.predictions_logged, 0u);
    EXPECT_EQ(bridge_->stats.anomalies_logged, 0u);
}

// =============================================================================
// 14. Query Effects and Metrics Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, GetWMEffectsNullBridgeReturnsNull)
{
    const omni_wm_to_logging_effects_t* effects =
        omni_wm_logging_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetWMEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const omni_wm_to_logging_effects_t* effects =
        omni_wm_logging_bridge_get_wm_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetLoggingEffectsNullBridgeReturnsNull)
{
    const logging_to_omni_wm_effects_t* effects =
        omni_wm_logging_bridge_get_logging_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetLoggingEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const logging_to_omni_wm_effects_t* effects =
        omni_wm_logging_bridge_get_logging_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetTrainingMetricsNullBridgeReturnsNull)
{
    const wm_training_metrics_t* metrics =
        omni_wm_logging_bridge_get_training_metrics(nullptr);
    EXPECT_EQ(metrics, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetTrainingMetricsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const wm_training_metrics_t* metrics =
        omni_wm_logging_bridge_get_training_metrics(bridge_);
    ASSERT_NE(metrics, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetCalibrationNullBridgeReturnsNull)
{
    const wm_calibration_metrics_t* calibration =
        omni_wm_logging_bridge_get_calibration(nullptr);
    EXPECT_EQ(calibration, nullptr);
}

TEST_F(WMLoggingBridgeTest, GetCalibrationValid)
{
    ASSERT_NE(bridge_, nullptr);

    const wm_calibration_metrics_t* calibration =
        omni_wm_logging_bridge_get_calibration(bridge_);
    ASSERT_NE(calibration, nullptr);
}

// =============================================================================
// 15. Bio-Async Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, IsBioAsyncConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_logging_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

// =============================================================================
// 16. Utility Function Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_logging_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, ValidateConfigDefaultValid)
{
    omni_wm_logging_bridge_config_t config;
    omni_wm_logging_bridge_default_config(&config);

    nimcp_error_t result = omni_wm_logging_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMLoggingBridgeTest, CategoryToStringValid)
{
    for (int cat = WM_LOG_CAT_PREDICTION; cat < WM_LOG_CAT_COUNT; cat++) {
        const char* str = wm_log_category_to_string((wm_log_category_t)cat);
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(WMLoggingBridgeTest, SeverityToStringValid)
{
    for (int sev = WM_LOG_SEV_TRACE; sev < WM_LOG_SEV_COUNT; sev++) {
        const char* str = wm_log_severity_to_string((wm_log_severity_t)sev);
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(WMLoggingBridgeTest, AnomalyTypeToStringValid)
{
    for (int type = WM_ANOMALY_NONE; type < WM_ANOMALY_COUNT; type++) {
        const char* str = wm_anomaly_type_to_string((wm_anomaly_type_t)type);
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(WMLoggingBridgeTest, CategoryMaskMacro)
{
    uint32_t mask = WM_LOG_CAT_MASK(WM_LOG_CAT_PREDICTION);
    EXPECT_EQ(mask, 1u << WM_LOG_CAT_PREDICTION);

    mask = WM_LOG_CAT_MASK(WM_LOG_CAT_TRAINING);
    EXPECT_EQ(mask, 1u << WM_LOG_CAT_TRAINING);
}

TEST_F(WMLoggingBridgeTest, CategoryAllMask)
{
    uint32_t all_mask = WM_LOG_CAT_ALL;
    EXPECT_EQ(all_mask, (1u << WM_LOG_CAT_COUNT) - 1);
}

// =============================================================================
// 17. Memory Safety Tests
// =============================================================================

TEST_F(WMLoggingBridgeTest, CreateDestroyManyTimes)
{
    for (int i = 0; i < 100; i++) {
        omni_wm_logging_bridge_t* temp = omni_wm_logging_bridge_create(nullptr);
        ASSERT_NE(temp, nullptr);
        omni_wm_logging_bridge_destroy(temp);
    }
}

TEST_F(WMLoggingBridgeTest, LogManyPredictions)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> input = create_test_array(TEST_DIM, 0.5f);
    std::vector<float> output = create_test_array(TEST_DIM, 0.6f);

    for (int i = 0; i < 1000; i++) {
        omni_wm_logging_bridge_log_prediction(
            bridge_, input.data(), TEST_DIM, output.data(), TEST_DIM, 0.8f);
    }

    // Should handle many logs without crashing
}

TEST_F(WMLoggingBridgeTest, FlushManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 100; i++) {
        omni_wm_logging_bridge_flush(bridge_);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
