/**
 * @file test_portia_deception_sensor_integration.cpp
 * @brief Integration tests for Portia deception and sensor interaction
 *
 * WHAT: Tests stealth mode affects sensor outputs and mimicry
 * WHY:  Validate Portia-inspired deceptive hunting behaviors work with sensors
 * HOW:  Enable stealth/mimicry, verify sensor outputs modified, measure effectiveness
 *
 * BIOLOGICAL INSPIRATION:
 * Portia spiders use sophisticated deception:
 * - Stealth mode: Slow, deliberate movements to avoid prey detection
 * - Mimicry: Imitate debris or other spider species
 * - Signal manipulation: Generate vibrations to deceive prey
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_sensor_fusion.h"
#include "async/nimcp_bio_async.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"

// Mock deception system
typedef struct {
    bool stealth_mode_active;
    bool mimicry_active;
    uint32_t mimicry_target_type;  // What we're imitating
    float deception_effectiveness;  // 0.0-1.0
    uint32_t successful_deceptions;
} mock_deception_state_t;

// Sensor output modification modes
typedef enum {
    SENSOR_NORMAL,
    SENSOR_STEALTHED,    // Reduced signature
    SENSOR_MIMICKED      // Altered signature
} sensor_output_mode_t;

class PortiaDeceptionSensorIntegrationTest : public ::testing::Test {
protected:
    portia_fusion_ctx_t* fusion_ctx = nullptr;
    nimcp_bio_ctx_t* bio_ctx = nullptr;
    mock_deception_state_t deception_state;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        // bio_ctx removed

        // Initialize sensor fusion
        portia_fusion_config_t fusion_config = portia_fusion_default_config();
        fusion_ctx = portia_fusion_init(&fusion_config, NULL);
        ASSERT_NE(fusion_ctx, nullptr);

        // Initialize deception state
        deception_state = {
            .stealth_mode_active = false,
            .mimicry_active = false,
            .mimicry_target_type = 0,
            .deception_effectiveness = 0.0f,
            .successful_deceptions = 0
        };
    }

    void TearDown() override {
        if (fusion_ctx) {
            portia_fusion_destroy(fusion_ctx);
            fusion_ctx = nullptr;
        }
        nimcp_bio_async_shutdown();
    }

    // Helper: Apply stealth modification to sensor reading
    sensor_reading_t apply_stealth(const sensor_reading_t& original) {
        sensor_reading_t modified = original;

        if (deception_state.stealth_mode_active) {
            // Reduce sensor signature (movement, thermal, acoustic)
            modified.value *= 0.3f;  // 70% reduction
            modified.confidence *= 0.8f;  // Slightly reduce confidence
        }

        return modified;
    }

    // Helper: Apply mimicry modification to sensor reading
    sensor_reading_t apply_mimicry(const sensor_reading_t& original) {
        sensor_reading_t modified = original;

        if (deception_state.mimicry_active) {
            // Alter signature to match target type
            switch (deception_state.mimicry_target_type) {
                case 1:  // Debris
                    modified.value *= 0.1f;  // Minimal movement
                    break;
                case 2:  // Another spider
                    modified.value *= 0.6f;  // Different movement pattern
                    break;
                default:
                    break;
            }
        }

        return modified;
    }

    // Helper: Measure deception effectiveness
    float measure_deception_effectiveness(float original_value, float modified_value) {
        float change_ratio = std::abs(original_value - modified_value) /
                             std::max(original_value, 0.01f);
        return std::min(change_ratio, 1.0f);
    }
};

//=============================================================================
// TEST SUITE 1: Stealth Mode Affects Sensor Outputs
//=============================================================================

TEST_F(PortiaDeceptionSensorIntegrationTest, Stealth_ReducesMovementSignature) {
    uint64_t timestamp = 1000;

    // Normal movement
    sensor_reading_t normal_visual = {
        SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true
    };

    // Enable stealth
    deception_state.stealth_mode_active = true;

    // Apply stealth
    sensor_reading_t stealthed = apply_stealth(normal_visual);

    // Signature should be reduced
    EXPECT_LT(stealthed.value, normal_visual.value);
    EXPECT_LT(stealthed.value, normal_visual.value * 0.5f);  // At least 50% reduction
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Stealth_AffectsMultipleSensorTypes) {
    uint64_t timestamp = 1000;
    deception_state.stealth_mode_active = true;

    // Visual sensor
    sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 80.0f, 0.9f, timestamp, true};
    sensor_reading_t stealthed_visual = apply_stealth(visual);
    EXPECT_LT(stealthed_visual.value, visual.value);

    // Acoustic/vibration sensor
    sensor_reading_t vibration = {SENSOR_TYPE_VIBRATION, 50.0f, 0.8f, timestamp, true};
    sensor_reading_t stealthed_vibration = apply_stealth(vibration);
    EXPECT_LT(stealthed_vibration.value, vibration.value);

    // Thermal sensor
    sensor_reading_t thermal = {SENSOR_TYPE_THERMAL, 60.0f, 0.7f, timestamp, true};
    sensor_reading_t stealthed_thermal = apply_stealth(thermal);
    EXPECT_LT(stealthed_thermal.value, thermal.value);
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Stealth_DisablingRestoresNormalOutput) {
    uint64_t timestamp = 1000;
    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true};

    // Enable then disable stealth
    deception_state.stealth_mode_active = true;
    sensor_reading_t stealthed = apply_stealth(original);
    EXPECT_LT(stealthed.value, original.value);

    deception_state.stealth_mode_active = false;
    sensor_reading_t restored = apply_stealth(original);
    EXPECT_FLOAT_EQ(restored.value, original.value);
}

//=============================================================================
// TEST SUITE 2: Mimicry Generates Expected Signals
//=============================================================================

TEST_F(PortiaDeceptionSensorIntegrationTest, Mimicry_DebrisModeMinimalSignature) {
    uint64_t timestamp = 1000;
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 1;  // Debris

    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true};
    sensor_reading_t mimicked = apply_mimicry(original);

    // Debris should have very low signature
    EXPECT_LT(mimicked.value, original.value * 0.2f);
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Mimicry_SpiderModeAlteredSignature) {
    uint64_t timestamp = 1000;
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 2;  // Another spider

    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true};
    sensor_reading_t mimicked = apply_mimicry(original);

    // Spider mimicry should alter but not eliminate signature
    EXPECT_LT(mimicked.value, original.value);
    EXPECT_GT(mimicked.value, original.value * 0.4f);  // Not too low
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Mimicry_DifferentTargetsDifferentOutputs) {
    uint64_t timestamp = 1000;
    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true};

    // Debris mimicry
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 1;
    sensor_reading_t debris_output = apply_mimicry(original);

    // Spider mimicry
    deception_state.mimicry_target_type = 2;
    sensor_reading_t spider_output = apply_mimicry(original);

    // Should produce different outputs
    EXPECT_NE(debris_output.value, spider_output.value);
    EXPECT_LT(debris_output.value, spider_output.value);  // Debris more subtle
}

//=============================================================================
// TEST SUITE 3: Deception Effectiveness Measurable
//=============================================================================

TEST_F(PortiaDeceptionSensorIntegrationTest, Effectiveness_HighForStealthMode) {
    deception_state.stealth_mode_active = true;

    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, 1000, true};
    sensor_reading_t modified = apply_stealth(original);

    float effectiveness = measure_deception_effectiveness(original.value, modified.value);

    // Should be highly effective (large change)
    EXPECT_GT(effectiveness, 0.6f);
    deception_state.deception_effectiveness = effectiveness;
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Effectiveness_VariesByMimicryType) {
    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, 1000, true};

    // Debris mimicry
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 1;
    sensor_reading_t debris = apply_mimicry(original);
    float debris_effectiveness = measure_deception_effectiveness(original.value, debris.value);

    // Spider mimicry
    deception_state.mimicry_target_type = 2;
    sensor_reading_t spider = apply_mimicry(original);
    float spider_effectiveness = measure_deception_effectiveness(original.value, spider.value);

    // Debris should be more effective (more dramatic change)
    EXPECT_GT(debris_effectiveness, spider_effectiveness);
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Effectiveness_CombiningStealthAndMimicry) {
    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, 1000, true};

    // Stealth only
    deception_state.stealth_mode_active = true;
    sensor_reading_t stealthed = apply_stealth(original);
    float stealth_effectiveness = measure_deception_effectiveness(original.value, stealthed.value);

    // Mimicry only
    deception_state.stealth_mode_active = false;
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 1;
    sensor_reading_t mimicked = apply_mimicry(original);
    float mimicry_effectiveness = measure_deception_effectiveness(original.value, mimicked.value);

    // Both
    deception_state.stealth_mode_active = true;
    sensor_reading_t both = apply_mimicry(apply_stealth(original));
    float combined_effectiveness = measure_deception_effectiveness(original.value, both.value);

    // Combined should be most effective
    EXPECT_GE(combined_effectiveness, std::max(stealth_effectiveness, mimicry_effectiveness));
}

//=============================================================================
// TEST SUITE 4: Sensor Fusion Handles Deceptive Inputs
//=============================================================================

TEST_F(PortiaDeceptionSensorIntegrationTest, SensorFusion_ProcessesStealthedReadings) {
    uint64_t timestamp = nimcp_time_monotonic_ms();  // Use current time to avoid stale data rejection
    deception_state.stealth_mode_active = true;

    // Provide stealthed sensor readings
    sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true};
    sensor_reading_t stealthed_visual = apply_stealth(visual);
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &stealthed_visual));

    sensor_reading_t imu = {SENSOR_TYPE_IMU, 50.0f, 0.85f, timestamp, true};
    sensor_reading_t stealthed_imu = apply_stealth(imu);
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &stealthed_imu));

    // Fusion should process stealthed inputs
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    fused_state_t state;
    ASSERT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Confidence may be affected
    EXPECT_GT(state.confidence, 0.0f);
}

TEST_F(PortiaDeceptionSensorIntegrationTest, SensorFusion_DetectsInconsistentMimicry) {
    uint64_t timestamp = nimcp_time_monotonic_ms();  // Use current time to avoid stale data rejection
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 1;  // Debris

    // Some sensors mimicked, some not (inconsistent)
    sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, timestamp, true};
    sensor_reading_t mimicked_visual = apply_mimicry(visual);
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &mimicked_visual));

    sensor_reading_t imu = {SENSOR_TYPE_IMU, 80.0f, 0.9f, timestamp, true};  // Not mimicked
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));

    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    fused_state_t state;
    ASSERT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

    // Inconsistency may reduce confidence
    EXPECT_LT(state.confidence, 0.95f);
}

//=============================================================================
// TEST SUITE 5: Deception Success Tracking
//=============================================================================

TEST_F(PortiaDeceptionSensorIntegrationTest, Success_TrackingDeceptionAttempts) {
    // Simulate multiple deception attempts
    for (int i = 0; i < 10; i++) {
        deception_state.stealth_mode_active = true;

        sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f,
                                       static_cast<uint64_t>(1000 + i * 100), true};
        sensor_reading_t stealthed = apply_stealth(original);

        float effectiveness = measure_deception_effectiveness(original.value, stealthed.value);

        // Count as successful if effectiveness > 0.5
        if (effectiveness > 0.5f) {
            deception_state.successful_deceptions++;
        }
    }

    // Should have some successful deceptions
    EXPECT_GT(deception_state.successful_deceptions, 0u);
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Success_EffectivenessMetricsAccurate) {
    deception_state.stealth_mode_active = true;
    deception_state.mimicry_active = true;
    deception_state.mimicry_target_type = 1;

    std::vector<float> effectiveness_values;

    for (int i = 0; i < 20; i++) {
        sensor_reading_t original = {SENSOR_TYPE_VISUAL,
                                       static_cast<float>(50 + i * 5),
                                       0.9f,
                                       static_cast<uint64_t>(1000 + i * 100),
                                       true};

        sensor_reading_t modified = apply_mimicry(apply_stealth(original));
        float eff = measure_deception_effectiveness(original.value, modified.value);
        effectiveness_values.push_back(eff);
    }

    // Calculate average effectiveness
    float avg_effectiveness = 0.0f;
    for (float eff : effectiveness_values) {
        avg_effectiveness += eff;
    }
    avg_effectiveness /= effectiveness_values.size();

    // Should be reasonably effective on average
    EXPECT_GT(avg_effectiveness, 0.4f);
    deception_state.deception_effectiveness = avg_effectiveness;
}

//=============================================================================
// TEST SUITE 6: Dynamic Deception Control
//=============================================================================

TEST_F(PortiaDeceptionSensorIntegrationTest, Dynamic_TogglingStealthMode) {
    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, 1000, true};

    // Toggle stealth on/off
    for (int i = 0; i < 5; i++) {
        deception_state.stealth_mode_active = (i % 2 == 0);
        sensor_reading_t modified = apply_stealth(original);

        if (deception_state.stealth_mode_active) {
            EXPECT_LT(modified.value, original.value);
        } else {
            EXPECT_FLOAT_EQ(modified.value, original.value);
        }
    }
}

TEST_F(PortiaDeceptionSensorIntegrationTest, Dynamic_SwitchingMimicryTargets) {
    sensor_reading_t original = {SENSOR_TYPE_VISUAL, 100.0f, 0.9f, 1000, true};
    deception_state.mimicry_active = true;

    // Try different mimicry targets
    for (uint32_t target = 1; target <= 2; target++) {
        deception_state.mimicry_target_type = target;
        sensor_reading_t modified = apply_mimicry(original);

        // Each target should produce different output
        EXPECT_NE(modified.value, original.value);
    }
}
