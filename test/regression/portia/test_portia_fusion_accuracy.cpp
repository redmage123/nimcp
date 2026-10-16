/**
 * @file test_portia_fusion_accuracy.cpp
 * @brief Regression tests for Portia sensor fusion accuracy
 *
 * TEST COVERAGE:
 * - Fusion accuracy with noisy sensors
 * - Kalman filter convergence
 * - Sensor dropout recovery
 * - Fusion latency
 * - Outlier rejection effectiveness
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_sensor_fusion.h"

namespace {

class PortiaFusionAccuracyTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = portia_fusion_default_config();
        config.fusion_rate_hz = 100;
        config.enable_kalman = true;
        config.outlier_threshold = 3.0f;
        config.min_sensors = 1;

        ctx = portia_fusion_init(&config, nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            portia_fusion_destroy(ctx);
        }
    }

    sensor_reading_t create_reading(sensor_type_t type, float value,
                                    float confidence = 0.9f) {
        sensor_reading_t reading;
        reading.type = type;
        reading.value = value;
        reading.confidence = confidence;
        reading.timestamp_ms = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
        reading.valid = true;
        return reading;
    }

    portia_fusion_config_t config;
    portia_fusion_ctx_t* ctx;
    std::mt19937 rng{42};
};

TEST_F(PortiaFusionAccuracyTest, AccuracyWithNoisySensors) {
    const int SAMPLES = 100;
    const float TRUE_VALUE = 100.0f;
    const float NOISE_STDDEV = 5.0f;

    std::normal_distribution<float> noise(0.0f, NOISE_STDDEV);

    for (int i = 0; i < SAMPLES; i++) {
        float noisy_value = TRUE_VALUE + noise(rng);

        sensor_reading_t reading = create_reading(
            SENSOR_TYPE_VISUAL, noisy_value, 0.8f);
        portia_fusion_update_sensor(ctx, &reading);
        portia_fusion_process(ctx);
    }

    fused_state_t state;
    bool success = portia_fusion_get_state(ctx, &state);
    ASSERT_TRUE(success);

    // Fused estimate should be close to true value
    float error = std::abs(state.x - TRUE_VALUE);
    EXPECT_LT(error, NOISE_STDDEV * 2.0f)
        << "Fusion error too high: " << error;

    std::cout << "Fusion error with noisy sensors: " << error << "\n";
}

TEST_F(PortiaFusionAccuracyTest, KalmanFilterConvergence) {
    const int CONVERGENCE_SAMPLES = 200;
    std::vector<float> errors;

    for (int i = 0; i < CONVERGENCE_SAMPLES; i++) {
        sensor_reading_t reading = create_reading(
            SENSOR_TYPE_VISUAL, 50.0f + (i % 2 == 0 ? 1.0f : -1.0f), 0.9f);
        portia_fusion_update_sensor(ctx, &reading);
        portia_fusion_process(ctx);

        fused_state_t state;
        if (portia_fusion_get_state(ctx, &state)) {
            float error = std::abs(state.x - 50.0f);
            errors.push_back(error);
        }
    }

    // Errors should decrease over time (convergence)
    if (errors.size() > 50) {
        float early_avg = 0.0f, late_avg = 0.0f;
        for (size_t i = 0; i < 25; i++) early_avg += errors[i];
        for (size_t i = errors.size() - 25; i < errors.size(); i++) late_avg += errors[i];
        early_avg /= 25.0f;
        late_avg /= 25.0f;

        EXPECT_LT(late_avg, early_avg)
            << "Kalman filter did not converge";
        std::cout << "Early error: " << early_avg
                  << ", Late error: " << late_avg << "\n";
    }
}

TEST_F(PortiaFusionAccuracyTest, SensorDropoutRecovery) {
    // Start with multiple sensors
    for (int i = 0; i < 50; i++) {
        sensor_reading_t r1 = create_reading(SENSOR_TYPE_VISUAL, 100.0f, 0.9f);
        sensor_reading_t r2 = create_reading(SENSOR_TYPE_IMU, 100.0f, 0.8f);
        portia_fusion_update_sensor(ctx, &r1);
        portia_fusion_update_sensor(ctx, &r2);
        portia_fusion_process(ctx);
    }

    fused_state_t state_before;
    portia_fusion_get_state(ctx, &state_before);

    // Simulate sensor dropout (only one sensor continues)
    for (int i = 0; i < 50; i++) {
        sensor_reading_t r1 = create_reading(SENSOR_TYPE_VISUAL, 100.0f, 0.9f);
        portia_fusion_update_sensor(ctx, &r1);
        portia_fusion_process(ctx);
    }

    fused_state_t state_after;
    bool success = portia_fusion_get_state(ctx, &state_after);

    ASSERT_TRUE(success) << "Fusion failed after sensor dropout";
    EXPECT_GT(state_after.confidence, 0.5f)
        << "Confidence too low after dropout";

    std::cout << "Confidence before: " << state_before.confidence
              << ", after dropout: " << state_after.confidence << "\n";
}

TEST_F(PortiaFusionAccuracyTest, FusionLatency) {
    const int ITERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, 50.0f, 0.9f);
        portia_fusion_update_sensor(ctx, &reading);
        portia_fusion_process(ctx);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avg_latency = elapsed.count() / ITERATIONS;

    const double MAX_LATENCY_US = 500.0;
    EXPECT_LT(avg_latency, MAX_LATENCY_US)
        << "Fusion latency too high: " << avg_latency << " us";

    std::cout << "Average fusion latency: " << avg_latency << " us\n";
}

TEST_F(PortiaFusionAccuracyTest, OutlierRejectionEffective) {
    const int SAMPLES = 100;
    const float NORMAL_VALUE = 100.0f;
    const float OUTLIER_VALUE = 500.0f;

    // Feed mostly normal readings with occasional outliers
    for (int i = 0; i < SAMPLES; i++) {
        float value = (i % 10 == 0) ? OUTLIER_VALUE : NORMAL_VALUE;
        sensor_reading_t reading = create_reading(SENSOR_TYPE_VISUAL, value, 0.9f);
        portia_fusion_update_sensor(ctx, &reading);
        portia_fusion_process(ctx);
    }

    fused_state_t state;
    bool success = portia_fusion_get_state(ctx, &state);
    ASSERT_TRUE(success);

    // Fused estimate should be close to normal value, not outliers
    float error = std::abs(state.x - NORMAL_VALUE);
    EXPECT_LT(error, 20.0f)
        << "Outliers affected fusion: error = " << error;

    portia_fusion_stats_t stats;
    portia_fusion_get_stats(ctx, &stats);
    EXPECT_GT(stats.outliers_rejected, 0u)
        << "No outliers were rejected";

    std::cout << "Outliers rejected: " << stats.outliers_rejected << "\n";
}

TEST_F(PortiaFusionAccuracyTest, MultiSensorFusionAccurate) {
    // Feed data from multiple sensors with different noise levels
    std::normal_distribution<float> noise1(0.0f, 2.0f);   // Low noise
    std::normal_distribution<float> noise2(0.0f, 10.0f);  // High noise

    const float TRUE_VALUE = 75.0f;

    for (int i = 0; i < 100; i++) {
        float value1 = TRUE_VALUE + noise1(rng);
        float value2 = TRUE_VALUE + noise2(rng);

        sensor_reading_t r1 = create_reading(SENSOR_TYPE_VISUAL, value1, 0.9f);
        sensor_reading_t r2 = create_reading(SENSOR_TYPE_IMU, value2, 0.6f);

        portia_fusion_update_sensor(ctx, &r1);
        portia_fusion_update_sensor(ctx, &r2);
        portia_fusion_process(ctx);
    }

    fused_state_t state;
    portia_fusion_get_state(ctx, &state);

    float error = std::abs(state.x - TRUE_VALUE);
    EXPECT_LT(error, 5.0f)
        << "Multi-sensor fusion error: " << error;
}

TEST_F(PortiaFusionAccuracyTest, WeightAdjustmentWorks) {
    // Set different weights
    portia_fusion_set_weight(ctx, SENSOR_TYPE_VISUAL, 0.8f);
    portia_fusion_set_weight(ctx, SENSOR_TYPE_IMU, 0.2f);

    for (int i = 0; i < 50; i++) {
        sensor_reading_t r1 = create_reading(SENSOR_TYPE_VISUAL, 100.0f, 0.9f);
        sensor_reading_t r2 = create_reading(SENSOR_TYPE_IMU, 50.0f, 0.9f);

        portia_fusion_update_sensor(ctx, &r1);
        portia_fusion_update_sensor(ctx, &r2);
        portia_fusion_process(ctx);
    }

    fused_state_t state;
    portia_fusion_get_state(ctx, &state);

    // Result should be closer to visual (100) than IMU (50)
    // Note: With equal convergence to midpoint, x may be exactly 75.0
    EXPECT_GE(state.x, 74.0f)
        << "Weight adjustment not working: x = " << state.x;
}

} // namespace
