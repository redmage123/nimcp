/**
 * @file e2e_test_portia_sensor_fusion_pipeline.cpp
 * @brief End-to-end test for Portia sensor fusion pipeline
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "e2e_test_framework.h"
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_sensor_fusion.h"
#include "async/nimcp_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"

class PortiaSensorFusionE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_log_init(NULL);
        nimcp_bio_async_init(nullptr);
        fusion_ctx_ = nullptr;
    }

    void TearDown() override {
        if (fusion_ctx_) {
            portia_fusion_destroy(fusion_ctx_);
        }
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }

    portia_fusion_ctx_t* fusion_ctx_;
};

TEST_F(PortiaSensorFusionE2ETest, MultiSensorDataFlow) {
    portia_fusion_config_t config = portia_fusion_default_config();
    fusion_ctx_ = portia_fusion_init(&config, nullptr);
    ASSERT_NE(fusion_ctx_, nullptr);

    // Send multiple sensor readings with current timestamps
    uint64_t now = nimcp_time_monotonic_ms();
    sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 1.0f, 0.9f, now, true};
    sensor_reading_t audio = {SENSOR_TYPE_AUDIO, 0.5f, 0.8f, now + 1, true};
    sensor_reading_t imu = {SENSOR_TYPE_IMU, 0.3f, 0.95f, now + 2, true};

    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx_, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx_, &audio));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx_, &imu));

    // Process fusion
    EXPECT_TRUE(portia_fusion_process(fusion_ctx_));

    // Get fused state
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx_, &state));
    EXPECT_GT(state.confidence, 0.0f);

    nimcp_log(LOG_LEVEL_INFO, "MultiSensorDataFlow: PASS - Confidence=%.2f", state.confidence);
}

TEST_F(PortiaSensorFusionE2ETest, SensorFailureHandling) {
    portia_fusion_config_t config = portia_fusion_default_config();
    config.enable_fallback = true;
    fusion_ctx_ = portia_fusion_init(&config, nullptr);
    ASSERT_NE(fusion_ctx_, nullptr);

    // Send one good sensor with current timestamp
    uint64_t now = nimcp_time_monotonic_ms();
    sensor_reading_t good = {SENSOR_TYPE_VISUAL, 1.0f, 0.9f, now, true};
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx_, &good));

    // Send failed sensor
    sensor_reading_t failed = {SENSOR_TYPE_AUDIO, 0.0f, 0.0f, now + 1, false};
    portia_fusion_update_sensor(fusion_ctx_, &failed);

    // Should still process successfully with fallback
    EXPECT_TRUE(portia_fusion_process(fusion_ctx_));

    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx_, &state));
    EXPECT_GT(state.confidence, 0.0f);

    nimcp_log(LOG_LEVEL_INFO, "SensorFailureHandling: PASS");
}
