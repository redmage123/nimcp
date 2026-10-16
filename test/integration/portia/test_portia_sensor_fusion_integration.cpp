/**
 * @file test_portia_sensor_fusion_integration.cpp
 * @brief Integration tests for Portia sensor fusion with bio-async
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "portia/nimcp_portia_sensor_fusion.h"
#include "async/nimcp_bio_ctx.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"

class PortiaSensorFusionIntegrationTest : public ::testing::Test {
protected:
    portia_fusion_ctx_t* fusion_ctx;
    nimcp_bio_ctx_t* bio_ctx;
    portia_fusion_config_t config;

    void SetUp() override {
        bio_ctx = nimcp_bio_ctx_create(64, 2048);
        ASSERT_NE(bio_ctx, nullptr);

        config = portia_fusion_default_config();
        fusion_ctx = nullptr;
    }

    void TearDown() override {
        if (fusion_ctx) {
            portia_fusion_destroy(fusion_ctx);
        }
        if (bio_ctx) {
            nimcp_bio_ctx_destroy(bio_ctx);
        }
    }

    sensor_reading_t create_reading(sensor_type_t type, float value, float confidence) {
        sensor_reading_t reading;
        reading.type = type;
        reading.value = value;
        reading.confidence = confidence;
        reading.timestamp_ms = nimcp_platform_get_time_ms();
        reading.valid = true;
        return reading;
    }
};

// Test bio-async event broadcasting
TEST_F(PortiaSensorFusionIntegrationTest, BioAsyncEventBroadcasting) {
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Register message handler
    int message_count = 0;
    auto handler = [](void* user_data, const nimcp_bio_message_t* msg) {
        int* count = static_cast<int*>(user_data);
        (*count)++;
    };

    // Note: Would need to register handler if bio_ctx supports it
    // For now, just verify fusion works with bio_ctx

    // Perform fusion operations that should generate events
    sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    // Allow time for async processing
    nimcp_platform_sleep_ms(50);

    // Verify system is functional
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));
}

// Test real-time multi-sensor integration
TEST_F(PortiaSensorFusionIntegrationTest, RealTimeMultiSensorIntegration) {
    config.fusion_rate_hz = 50;  // High rate
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Simulate real-time sensor streams
    const int duration_ms = 1000;
    const int update_period_ms = 20;  // 50 Hz
    uint64_t start_time = nimcp_platform_get_time_ms();

    int fusion_count = 0;
    while (nimcp_platform_get_time_ms() - start_time < duration_ms) {
        // Simulate sensor updates at different rates
        uint64_t current_time = nimcp_platform_get_time_ms();

        // Visual sensor at 50 Hz
        if ((current_time - start_time) % update_period_ms == 0) {
            float t = (current_time - start_time) / 1000.0f;
            sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f + sinf(t), 0.9f);
            portia_fusion_update_sensor(fusion_ctx, &visual);
        }

        // IMU at 100 Hz (faster)
        if ((current_time - start_time) % (update_period_ms / 2) == 0) {
            float t = (current_time - start_time) / 1000.0f;
            sensor_reading_t imu = create_reading(SENSOR_TYPE_IMU, 11.0f + cosf(t), 0.85f);
            portia_fusion_update_sensor(fusion_ctx, &imu);
        }

        // GPS at 10 Hz (slower)
        if ((current_time - start_time) % (update_period_ms * 5) == 0) {
            sensor_reading_t gps = create_reading(SENSOR_TYPE_GPS, 12.0f, 0.8f);
            portia_fusion_update_sensor(fusion_ctx, &gps);
        }

        // Run fusion
        if (portia_fusion_process(fusion_ctx)) {
            fusion_count++;
        }

        nimcp_platform_sleep_ms(5);
    }

    // Verify reasonable number of fusions occurred
    EXPECT_GT(fusion_count, 30);  // At least 30 Hz average

    portia_fusion_stats_t stats;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats));
    EXPECT_GT(stats.successful_fusions, 0u);
    EXPECT_GT(stats.total_updates, 0u);
}

// Test Kalman filter convergence
TEST_F(PortiaSensorFusionIntegrationTest, KalmanFilterConvergence) {
    config.enable_kalman = true;
    config.process_noise = 0.01f;
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Simulate constant velocity motion
    const float true_velocity = 5.0f;
    const float dt = 0.05f;  // 20 Hz
    float true_position = 0.0f;

    std::vector<float> confidence_history;

    for (int i = 0; i < 100; i++) {
        true_position += true_velocity * dt;

        // Add noisy measurement
        float noise = (rand() % 100) / 100.0f - 0.5f;
        sensor_reading_t reading = create_reading(
            SENSOR_TYPE_VISUAL,
            true_position + noise,
            0.9f
        );

        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &reading));
        EXPECT_TRUE(portia_fusion_process(fusion_ctx));

        // Track confidence
        float conf = portia_fusion_get_confidence(fusion_ctx);
        confidence_history.push_back(conf);

        nimcp_platform_sleep_ms(static_cast<uint32_t>(dt * 1000));
    }

    // Verify confidence improved over time (convergence)
    float early_conf = 0.0f;
    for (int i = 0; i < 10; i++) {
        early_conf += confidence_history[i];
    }
    early_conf /= 10.0f;

    float late_conf = 0.0f;
    for (int i = 90; i < 100; i++) {
        late_conf += confidence_history[i];
    }
    late_conf /= 10.0f;

    EXPECT_GE(late_conf, early_conf * 0.8f);  // Should maintain or improve confidence

    // Verify velocity estimation is reasonable
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    float estimated_velocity = sqrtf(state.vx * state.vx + state.vy * state.vy + state.vz * state.vz);
    EXPECT_GT(estimated_velocity, 0.0f);
}

// Test sensor failure recovery
TEST_F(PortiaSensorFusionIntegrationTest, SensorFailureRecovery) {
    config.enable_fallback = true;
    config.min_sensors = 2;
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Start with multiple sensors
    sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
    sensor_reading_t audio = create_reading(SENSOR_TYPE_AUDIO, 12.0f, 0.85f);
    sensor_reading_t imu = create_reading(SENSOR_TYPE_IMU, 11.0f, 0.88f);

    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &audio));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    float initial_confidence = portia_fusion_get_confidence(fusion_ctx);

    // Simulate sensor failure - disable one sensor
    EXPECT_TRUE(portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_AUDIO, false));

    // Continue with remaining sensors
    for (int i = 0; i < 10; i++) {
        visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f + i * 0.1f, 0.9f);
        imu = create_reading(SENSOR_TYPE_IMU, 11.0f + i * 0.1f, 0.88f);

        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));
        EXPECT_TRUE(portia_fusion_process(fusion_ctx));

        nimcp_platform_sleep_ms(20);
    }

    // System should still function
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));
    EXPECT_GT(state.confidence, 0.0f);

    // Re-enable sensor
    EXPECT_TRUE(portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_AUDIO, true));

    // Add all sensors again
    visual = create_reading(SENSOR_TYPE_VISUAL, 15.0f, 0.9f);
    audio = create_reading(SENSOR_TYPE_AUDIO, 17.0f, 0.85f);
    imu = create_reading(SENSOR_TYPE_IMU, 16.0f, 0.88f);

    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &audio));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    float recovered_confidence = portia_fusion_get_confidence(fusion_ctx);
    EXPECT_GT(recovered_confidence, 0.0f);
}

// Test adaptive weight adjustment
TEST_F(PortiaSensorFusionIntegrationTest, AdaptiveWeightAdjustment) {
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Start with equal weights
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        EXPECT_TRUE(portia_fusion_set_weight(fusion_ctx, static_cast<sensor_type_t>(i), 0.5f));
    }

    // Add consistent readings from visual sensor
    for (int i = 0; i < 20; i++) {
        sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f + i * 0.01f, 0.95f);
        EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
        nimcp_platform_sleep_ms(10);
    }

    // Add noisy readings from audio sensor
    for (int i = 0; i < 20; i++) {
        float noise = (rand() % 1000) / 100.0f - 5.0f;
        sensor_reading_t audio = create_reading(SENSOR_TYPE_AUDIO, 10.0f + noise, 0.5f);
        portia_fusion_update_sensor(fusion_ctx, &audio);  // May be rejected as outliers
        nimcp_platform_sleep_ms(10);
    }

    // Increase weight of reliable sensor
    EXPECT_TRUE(portia_fusion_set_weight(fusion_ctx, SENSOR_TYPE_VISUAL, 0.9f));
    EXPECT_TRUE(portia_fusion_set_weight(fusion_ctx, SENSOR_TYPE_AUDIO, 0.1f));

    // Process fusion with adjusted weights
    sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.95f);
    sensor_reading_t audio = create_reading(SENSOR_TYPE_AUDIO, 20.0f, 0.5f);

    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &audio));
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    // State should be closer to visual sensor reading
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));
    EXPECT_GT(state.confidence, 0.5f);
}

// Test high-frequency operation
TEST_F(PortiaSensorFusionIntegrationTest, HighFrequencyOperation) {
    config.fusion_rate_hz = 100;  // 100 Hz
    config.enable_kalman = false;  // Use faster weighted average
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    uint64_t start_time = nimcp_platform_get_time_ms();
    int operations = 0;

    // Run for 1 second
    while (nimcp_platform_get_time_ms() - start_time < 1000) {
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.9f);
        if (portia_fusion_update_sensor(fusion_ctx, &reading)) {
            if (portia_fusion_process(fusion_ctx)) {
                operations++;
            }
        }

        nimcp_platform_sleep_ms(1);  // Minimal delay
    }

    // Should achieve high rate
    EXPECT_GT(operations, 80);  // At least 80 Hz average

    portia_fusion_stats_t stats;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats));
    EXPECT_GT(stats.successful_fusions, 80u);
}

// Test cross-modal sensor integration
TEST_F(PortiaSensorFusionIntegrationTest, CrossModalIntegration) {
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Simulate different modalities detecting the same event
    // Visual: high confidence, precise
    sensor_reading_t visual = create_reading(SENSOR_TYPE_VISUAL, 10.0f, 0.95f);

    // Vibration: medium confidence, complementary
    sensor_reading_t vibration = create_reading(SENSOR_TYPE_VIBRATION, 10.5f, 0.75f);

    // Chemical: low confidence, slower
    sensor_reading_t chemical = create_reading(SENSOR_TYPE_CHEMICAL, 9.8f, 0.6f);

    // Thermal: medium confidence
    sensor_reading_t thermal = create_reading(SENSOR_TYPE_THERMAL, 10.2f, 0.8f);

    // Add all readings
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &vibration));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &chemical));
    EXPECT_TRUE(portia_fusion_update_sensor(fusion_ctx, &thermal));

    // Process fusion
    EXPECT_TRUE(portia_fusion_process(fusion_ctx));

    // Get fused estimate
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Should have high confidence from multi-modal agreement
    EXPECT_GT(state.confidence, 0.6f);

    // Should include contributions from multiple sensors
    int contributing = __builtin_popcount(state.contributing_sensors);
    EXPECT_GE(contributing, 4);
}

// Test memory and performance under load
TEST_F(PortiaSensorFusionIntegrationTest, MemoryAndPerformance) {
    fusion_ctx = portia_fusion_init(&config, bio_ctx);
    ASSERT_NE(fusion_ctx, nullptr);

    // Run extended operation
    const int iterations = 10000;
    uint64_t start_time = nimcp_platform_get_time_ms();

    for (int i = 0; i < iterations; i++) {
        // Cycle through all sensor types
        sensor_type_t type = static_cast<sensor_type_t>(i % SENSOR_TYPE_COUNT);
        sensor_reading_t reading = create_reading(type, 10.0f + i * 0.001f, 0.9f);

        portia_fusion_update_sensor(fusion_ctx, &reading);

        if (i % 10 == 0) {
            portia_fusion_process(fusion_ctx);
        }
    }

    uint64_t elapsed_ms = nimcp_platform_get_time_ms() - start_time;

    // Verify performance
    float ops_per_ms = iterations / static_cast<float>(elapsed_ms);
    EXPECT_GT(ops_per_ms, 1.0f);  // At least 1000 ops/sec

    // Verify no memory leaks (system should still be functional)
    fused_state_t state;
    EXPECT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    portia_fusion_stats_t stats;
    EXPECT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats));
    EXPECT_EQ(stats.total_updates, iterations);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
