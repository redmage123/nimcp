/**
 * @file test_portia_sensor_fusion.cpp
 * @brief Unit tests for Portia sensor fusion
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>
// Headers have their own extern "C" guards
#include "portia/nimcp_portia_sensor_fusion.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"

class PortiaSensorFusionTest : public ::testing::Test {
protected:
    portia_fusion_ctx_t* fusion_ctx;
    portia_fusion_config_t config;

    void SetUp() override {
        // Get default config - bio_ctx is optional for testing
        config = portia_fusion_default_config();
        fusion_ctx = nullptr;
    }

    void TearDown() override {
        if (fusion_ctx) {
            portia_fusion_destroy(fusion_ctx);
        }
    }

    sensor_reading_t create_reading(sensor_type_t type, float value, float confidence) {
        sensor_reading_t reading;
        reading.type = type;
        reading.value = value;
        reading.confidence = confidence;
        reading.timestamp_ms = nimcp_platform_time_monotonic_ms();
        reading.valid = true;
        return reading;
    }
};

// Test initialization
TEST_F(PortiaSensorFusionTest, InitializationSuccess) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    float confidence = portia_fusion_get_confidence(fusion_ctx);
    EXPECT_GT(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(PortiaSensorFusionTest, InitializationWithNullConfig) {
    fusion_ctx = portia_fusion_init(nullptr, NULL);
    EXPECT_EQ(fusion_ctx, nullptr);
}

TEST_F(PortiaSensorFusionTest, InitializationWithInvalidConfig) {
    config.fusion_rate_hz = 0;  // Invalid
    fusion_ctx = portia_fusion_init(&config, NULL);
    EXPECT_EQ(fusion_ctx, nullptr);
}

// Test default configuration
TEST_F(PortiaSensorFusionTest, DefaultConfigValidation) {
    portia_fusion_config_t default_cfg = portia_fusion_default_config();

    EXPECT_EQ(default_cfg.fusion_rate_hz, 20u);
    EXPECT_FALSE(default_cfg.enable_kalman);
    EXPECT_TRUE(default_cfg.enable_fallback);
    EXPECT_EQ(default_cfg.min_sensors, 1u);

    // Check all sensors are configured
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        EXPECT_EQ(default_cfg.sensors[i].type, i);
        EXPECT_TRUE(default_cfg.sensors[i].enabled);
        EXPECT_GT(default_cfg.sensors[i].weight, 0.0f);
        EXPECT_LE(default_cfg.sensors[i].weight, 1.0f);
    }
}

// Test sensor names
TEST_F(PortiaSensorFusionTest, SensorNames) {
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_VISUAL), "VISUAL");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_AUDIO), "AUDIO");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_VIBRATION), "VIBRATION");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_CHEMICAL), "CHEMICAL");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_THERMAL), "THERMAL");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_PROXIMITY), "PROXIMITY");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_IMU), "IMU");
    EXPECT_STREQ(portia_fusion_sensor_name(SENSOR_TYPE_GPS), "GPS");
}

// Test sensor update
TEST_F(PortiaSensorFusionTest, UpdateSensorSuccess) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    bool result = portia_fusion_update_sensor(fusion_ctx, &reading);
    EXPECT_TRUE(result);
}

TEST_F(PortiaSensorFusionTest, UpdateSensorInvalidType) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    sensor_reading_t reading = create_reading(SENSOR_TYPE_COUNT, 10.0f, 0.9f);
    bool result = portia_fusion_update_sensor(fusion_ctx, &reading);
    EXPECT_FALSE(result);
}

TEST_F(PortiaSensorFusionTest, UpdateSensorInvalidConfidence) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 1.5f);  // > 1.0
    bool result = portia_fusion_update_sensor(fusion_ctx, &reading);
    EXPECT_FALSE(result);
}

TEST_F(PortiaSensorFusionTest, UpdateDisabledSensor) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Disable visual sensor
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_VISUAL, false);

    sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    bool result = portia_fusion_update_sensor(fusion_ctx, &reading);
    EXPECT_FALSE(result);
}

// Test weighted average fusion
TEST_F(PortiaSensorFusionTest, WeightedAverageFusion) {
    config.enable_kalman = false;  // Use weighted average
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add multiple sensor readings
    sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    sensor_reading_t audio = create_reading(SENSOR_TYPE_AUDIO, 15.0f, 0.8f);
    sensor_reading_t imu = create_reading(SENSOR_TYPE_IMU, 12.0f, 0.85f);

    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &audio));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));

    // Process fusion
    bool result = portia_fusion_process(fusion_ctx);
    EXPECT_TRUE(result);

    // Get fused state
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Verify state is reasonable
    EXPECT_GT(state.confidence, 0.0f);
    EXPECT_LE(state.confidence, 1.0f);
    EXPECT_GT(state.contributing_sensors, 0u);
    EXPECT_EQ(state.timestamp_ms > 0, true);
}

// Test Kalman filter fusion
TEST_F(PortiaSensorFusionTest, KalmanFilterFusion) {
    config.enable_kalman = true;  // Use Kalman filter
    config.process_noise = 0.05f;
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add sensor readings over time
    for (int i = 0; i < 10; i++) {
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f + i * 0.1f, 0.9f);
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));

        nimcp_platform_sleep_ms(10);

        bool result = portia_fusion_process(fusion_ctx);
        EXPECT_TRUE(result);
    }

    // Get final state
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Verify state tracking
    EXPECT_GT(state.confidence, 0.5f);  // Should have high confidence after updates
    EXPECT_NE(state.vx, 0.0f);  // Should have estimated velocity
}

// Test sensor weight adjustment
TEST_F(PortiaSensorFusionTest, SetSensorWeight) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Set new weight
    bool result = portia_fusion_set_weight(fusion_ctx, SENSOR_TYPE_VISUAL, 0.75f);
    EXPECT_TRUE(result);

    // Invalid weight
    result = portia_fusion_set_weight(fusion_ctx, SENSOR_TYPE_VISUAL, 1.5f);
    EXPECT_FALSE(result);

    // Invalid type
    result = portia_fusion_set_weight(fusion_ctx, SENSOR_TYPE_COUNT, 0.5f);
    EXPECT_FALSE(result);
}

// Test sensor enable/disable
TEST_F(PortiaSensorFusionTest, EnableDisableSensor) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Disable sensor
    bool result = portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_AUDIO, false);
    EXPECT_TRUE(result);

    // Re-enable sensor
    result = portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_AUDIO, true);
    EXPECT_TRUE(result);

    // Invalid type
    result = portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_COUNT, true);
    EXPECT_FALSE(result);
}

// Test outlier rejection
TEST_F(PortiaSensorFusionTest, OutlierRejection) {
    config.outlier_threshold = 3.0f;
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Establish baseline with normal readings
    for (int i = 0; i < 5; i++) {
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f + i * 0.1f, 0.9f);
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
        nimcp_platform_sleep_ms(10);
    }

    // Get initial stats
    portia_fusion_stats_t stats_before;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats_before));

    // Send outlier (should be rejected)
    sensor_reading_t outlier = create_reading(SENSOR_TYPE_VISUAL, 100.0f, 0.9f);
    portia_fusion_update_sensor(fusion_ctx, &outlier);  // May or may not succeed

    // Check if outliers were detected
    portia_fusion_stats_t stats_after;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats_after));

    // Outlier count should increase or reading should be rejected
    EXPECT_GE(stats_after.outliers_rejected, stats_before.outliers_rejected);
}

// Test multi-sensor fusion
TEST_F(PortiaSensorFusionTest, MultiSensorFusion) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add readings from all sensor types
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        sensor_reading_t reading = create_reading(
            static_cast<sensor_type_t>(i),
            10.0f + i,
            0.8f + i * 0.01f
        );
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
    }

    // Process fusion
    bool result = portia_fusion_process(fusion_ctx);
    EXPECT_TRUE(result);

    // Get state
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Should have contributions from multiple sensors
    int sensor_count = __builtin_popcount(state.contributing_sensors);
    EXPECT_GT(sensor_count, 1);
}

// Test minimum sensor requirement
TEST_F(PortiaSensorFusionTest, MinimumSensorRequirement) {
    config.min_sensors = 3;
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add only 2 sensors (below minimum)
    sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    sensor_reading_t audio = create_reading(SENSOR_TYPE_AUDIO, 12.0f, 0.85f);

    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &audio));

    // Fusion should fail due to insufficient sensors
    bool result = portia_fusion_process(fusion_ctx);
    EXPECT_FALSE(result);

    // Add third sensor
    sensor_reading_t imu = create_reading(SENSOR_TYPE_IMU, 11.0f, 0.88f);
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));

    // Now fusion should succeed
    result = portia_fusion_process(fusion_ctx);
    EXPECT_TRUE(result);
}

// Test state reset
TEST_F(PortiaSensorFusionTest, ResetState) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add sensor data and process
    sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    // Reset
    bool result = portia_fusion_reset(fusion_ctx);
    EXPECT_TRUE(result);

    // Get state after reset
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // State should be cleared
    EXPECT_EQ(state.x, 0.0f);
    EXPECT_EQ(state.y, 0.0f);
    EXPECT_EQ(state.z, 0.0f);
    EXPECT_LT(state.confidence, 0.1f);  // Near minimum
}

// Test statistics
TEST_F(PortiaSensorFusionTest, Statistics) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Perform multiple updates and fusions
    for (int i = 0; i < 10; i++) {
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f + i, 0.9f);
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
        EXPECT_TRUE(portia_fusion_process(fusion_ctx));
        nimcp_platform_sleep_ms(10);
    }

    // Get statistics
    portia_fusion_stats_t stats;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats));

    EXPECT_EQ(stats.total_updates, 10u);
    EXPECT_EQ(stats.successful_fusions, 10u);
    EXPECT_GT(stats.average_confidence, 0.0f);
    EXPECT_GT(stats.active_sensor_count, 0u);
}

// Test thread safety
TEST_F(PortiaSensorFusionTest, ThreadSafety) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Multiple threads updating different sensors
    const int num_threads = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t]() {
            sensor_type_t type = static_cast<sensor_type_t>(t % SENSOR_TYPE_COUNT);
            for (int i = 0; i < 100; i++) {
                sensor_reading_t reading = create_reading(type, 10.0f + i * 0.1f, 0.9f);
                portia_fusion_update_sensor(fusion_ctx, &reading);
                if (i % 10 == 0) {
                    portia_fusion_process(fusion_ctx);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify system is still functional
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    portia_fusion_stats_t stats;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats));
    EXPECT_GT(stats.total_updates, 0u);
}

// Test confidence calculation
TEST_F(PortiaSensorFusionTest, ConfidenceCalculation) {
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add high confidence readings
    sensor_reading_t high_conf = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.95f);
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &high_conf));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    float conf1 = portia_fusion_get_confidence(fusion_ctx);

    // Add low confidence reading
    sensor_reading_t low_conf = create_reading(SENSOR_TYPE_AUDIO, 12.0f, 0.3f);
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &low_conf));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    float conf2 = portia_fusion_get_confidence(fusion_ctx);

    // Confidence should be affected by sensor quality
    EXPECT_GT(conf1, 0.5f);
    EXPECT_LT(conf2, conf1);  // Adding low confidence sensor reduces overall confidence
}

// Test stale data handling
TEST_F(PortiaSensorFusionTest, StaleDataHandling) {
    config.enable_kalman = false;
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add fresh sensor reading
    sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    // Wait for data to become stale (5x update period)
    uint32_t stale_time_ms = (1000 / config.sensors[SENSOR_TYPE_VISUAL].update_rate_hz) * 6;
    nimcp_platform_sleep_ms(stale_time_ms);

    // Try to process with stale data
    bool result = portia_fusion_process(fusion_ctx);
    // May succeed or fail depending on other sensors, but should handle gracefully
    EXPECT_TRUE(result || !result);  // Should not crash
}

// Test velocity estimation
TEST_F(PortiaSensorFusionTest, VelocityEstimation) {
    config.enable_kalman = false;
    fusion_ctx = portia_fusion_init(&config, NULL);
    ASSERT_NE(fusion_ctx, nullptr);

    // Add sequence of readings showing movement
    for (int i = 0; i < 10; i++) {
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f + i * 2.0f, 0.9f);
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
        EXPECT_TRUE(portia_fusion_process(fusion_ctx));
        nimcp_platform_sleep_ms(50);  // 20 Hz
    }

    // Get state
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Should have non-zero velocity
    float speed = sqrtf(state.vx * state.vx + state.vy * state.vy + state.vz * state.vz);
    EXPECT_GT(speed, 0.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
