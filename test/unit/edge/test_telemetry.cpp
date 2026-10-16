/**
 * @file test_telemetry.cpp
 * @brief GoogleTest unit tests for NIMCP edge telemetry subsystem
 *
 * Tests serialization/deserialization round-trips, analysis actions,
 * and edge context inference recording.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class TelemetryTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(TelemetryTest, SerializeDeserializeRoundTrip) {
    nimcp_device_telemetry_t original;
    memset(&original, 0, sizeof(original));
    original.device_id = 42;
    original.timestamp = 1234567890;
    original.avg_inference_ms = 5.5f;
    original.p99_inference_ms = 12.3f;
    original.avg_loss = 0.05f;
    original.loss_trend = -0.001f;
    original.avg_confidence = 0.95f;
    original.low_confidence_pct = 0.02f;
    original.anomaly_rate = 0.01f;
    original.ram_usage_pct = 65.0f;
    original.cpu_usage_pct = 45.0f;
    original.battery_pct = 80.0f;
    original.temperature_c = 42.5f;
    original.steps_since_sync = 500;
    original.local_accuracy = 0.92f;
    original.rollbacks_triggered = 1;
    original.offline_mode = NIMCP_OFFLINE_NORMAL;
    original.power_mode = NIMCP_POWER_FULL;

    uint8_t buffer[4096];
    uint32_t bytes_written = 0;
    int ret = nimcp_telemetry_serialize(&original, buffer, sizeof(buffer), &bytes_written);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(bytes_written, 0u);

    nimcp_device_telemetry_t restored;
    memset(&restored, 0, sizeof(restored));
    ret = nimcp_telemetry_deserialize(buffer, bytes_written, &restored);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(restored.device_id, original.device_id);
    EXPECT_EQ(restored.timestamp, original.timestamp);
    EXPECT_FLOAT_EQ(restored.avg_inference_ms, original.avg_inference_ms);
    EXPECT_FLOAT_EQ(restored.avg_loss, original.avg_loss);
    EXPECT_FLOAT_EQ(restored.battery_pct, original.battery_pct);
    EXPECT_EQ(restored.rollbacks_triggered, original.rollbacks_triggered);
}

TEST_F(TelemetryTest, AnalyzeHighLossTrendRedistill) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.avg_loss = 0.5f;
    // Threshold is loss_trend > 0.1f (strict greater than)
    t.loss_trend = 0.2f; // Loss increasing
    t.battery_pct = 80.0f;

    uint32_t actions = nimcp_telemetry_analyze(&t);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_REDISTILL);
}

TEST_F(TelemetryTest, AnalyzeHighAnomalyRateAlert) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    // Threshold is anomaly_rate > 20.0f (percentage, not fraction)
    t.anomaly_rate = 25.0f;
    t.battery_pct = 80.0f;

    uint32_t actions = nimcp_telemetry_analyze(&t);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_ALERT_ANOMALY);
}

TEST_F(TelemetryTest, AnalyzeLowBatteryPowerSave) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    // Threshold is battery_pct < 10.0f (strict less than)
    t.battery_pct = 5.0f;
    t.anomaly_rate = 0.0f;
    t.loss_trend = 0.0f;

    uint32_t actions = nimcp_telemetry_analyze(&t);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_POWER_SAVE);
}

TEST_F(TelemetryTest, AnalyzeManyRollbacksStopUpdates) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.rollbacks_triggered = 10; // Many rollbacks
    t.battery_pct = 80.0f;

    uint32_t actions = nimcp_telemetry_analyze(&t);
    EXPECT_TRUE(actions & NIMCP_TELEMETRY_ACTION_STOP_UPDATES);
}

TEST_F(TelemetryTest, AnalyzeNormalValuesNoAction) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.avg_loss = 0.05f;
    t.loss_trend = -0.001f;
    t.anomaly_rate = 0.01f;
    t.battery_pct = 90.0f;
    t.rollbacks_triggered = 0;
    t.avg_confidence = 0.95f;

    uint32_t actions = nimcp_telemetry_analyze(&t);
    EXPECT_EQ(actions, NIMCP_TELEMETRY_ACTION_NONE);
}

TEST_F(TelemetryTest, EdgeContextCreateDestroy) {
    nimcp_device_profile_t profile = nimcp_device_profile_default();
    nimcp_edge_ctx_t* ctx = nimcp_edge_ctx_create(&profile);
    ASSERT_NE(ctx, nullptr);

    nimcp_edge_ctx_destroy(ctx);
}

TEST_F(TelemetryTest, EdgeContextRecordInference) {
    nimcp_device_profile_t profile = nimcp_device_profile_default();
    nimcp_edge_ctx_t* ctx = nimcp_edge_ctx_create(&profile);
    ASSERT_NE(ctx, nullptr);

    nimcp_edge_record_inference(ctx, 5.0f, 0.1f);
    nimcp_edge_record_inference(ctx, 3.0f, 0.08f);
    nimcp_edge_record_inference(ctx, 7.0f, 0.12f);

    EXPECT_EQ(ctx->total_steps, 3u);

    nimcp_edge_ctx_destroy(ctx);
}

TEST_F(TelemetryTest, SerializeBufferTooSmall) {
    nimcp_device_telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.device_id = 1;

    uint8_t buffer[4]; // Too small
    uint32_t bytes_written = 0;
    int ret = nimcp_telemetry_serialize(&t, buffer, sizeof(buffer), &bytes_written);
    // Should fail or write 0 bytes
    EXPECT_TRUE(ret != 0 || bytes_written == 0);
}
