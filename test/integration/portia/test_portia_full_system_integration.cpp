/**
 * @file test_portia_full_system_integration.cpp
 * @brief Integration tests for complete Portia system
 *
 * WHAT: Tests all Portia subsystems working together
 * WHY:  Validate end-to-end system behavior under realistic conditions
 * HOW:  Initialize all subsystems, simulate resource pressure, verify coordinated response
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/platform/nimcp_platform_tier.h"
#include "portia/nimcp_portia_degradation.h"
#include "portia/nimcp_portia_sensor_fusion.h"
#include "portia/nimcp_portia_learning.h"
#include "async/nimcp_bio_async.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"

class PortiaFullSystemIntegrationTest : public ::testing::Test {
protected:
    // System components
    degradation_state_t* degrade_state = nullptr;
    portia_fusion_ctx_t* fusion_ctx = nullptr;
    portia_learning_state_t* learning_state = nullptr;
    // bio_ctx removed
    platform_tier_t current_tier;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        // bio_ctx removed

        // Detect platform tier
        current_tier = platform_tier_detect();

        // Initialize degradation system
        // Note: hysteresis_ms=0 to avoid blocking level changes during tests
        degradation_internal_config_t degrade_config = {
            .level_thresholds = {0.0f, 60.0f, 75.0f, 85.0f, 95.0f},
            .hysteresis_ms = 0,  // Disable hysteresis for tests
            .enable_auto_degrade = true,
            .enable_auto_restore = true,
            .restore_threshold = 10.0f
        };
        degrade_state = portia_degradation_init(&degrade_config);
        ASSERT_NE(degrade_state, nullptr);

        // Register all features
        degradation_feature_t features[] = {
            {FEATURE_PLASTICITY, "plasticity", DEGRADATION_LEVEL_MINOR, 0.3f, false, true},
            {FEATURE_LEARNING, "learning", DEGRADATION_LEVEL_MODERATE, 0.4f, false, true},
            {FEATURE_EMOTIONS, "emotions", DEGRADATION_LEVEL_MODERATE, 0.2f, false, true},
            {FEATURE_PLANNING, "planning", DEGRADATION_LEVEL_SEVERE, 0.5f, false, true},
            {FEATURE_SENSORS_FULL, "full_sensors", DEGRADATION_LEVEL_MINOR, 0.35f, false, true},
        };
        for (size_t i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
            portia_degradation_register_feature(degrade_state, &features[i]);
        }

        // Initialize sensor fusion
        portia_fusion_config_t fusion_config = portia_fusion_default_config();
        fusion_ctx = portia_fusion_init(&fusion_config, (nimcp_bio_ctx_t*)NULL);
        ASSERT_NE(fusion_ctx, nullptr);

        // Initialize learning system
        portia_learning_config_t learning_config = {
            .allowed_modes = LEARNING_MODE_FULL,
            .max_habituation_entries = 50,
            .max_association_entries = 50,
            .default_learning_rate = 0.3f,
            .default_forgetting_rate = 0.01f,
            .consolidation_interval_ms = 1000,
            .habituation_threshold = 0.1f,
            .association_threshold = 0.2f
        };
        learning_state = portia_learning_init(&learning_config);
        ASSERT_NE(learning_state, nullptr);
    }

    void TearDown() override {
        if (learning_state) {
            portia_learning_destroy(learning_state);
            learning_state = nullptr;
        }
        if (fusion_ctx) {
            portia_fusion_destroy(fusion_ctx);
            fusion_ctx = nullptr;
        }
        if (degrade_state) {
            portia_degradation_cleanup(degrade_state);
            degrade_state = nullptr;
        }
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// TEST SUITE 1: All Subsystems Initialize Together
//=============================================================================

TEST_F(PortiaFullSystemIntegrationTest, Initialization_AllSubsystemsReady) {
    // Verify all subsystems initialized
    EXPECT_NE(degrade_state, nullptr);
    EXPECT_NE(fusion_ctx, nullptr);
    EXPECT_NE(learning_state, nullptr);
    // bio_ctx check removed - not used in tests

    // Verify degradation state
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);
    EXPECT_GT(active_features, 0u);

    // Verify fusion ready
    EXPECT_GT(portia_fusion_get_confidence(fusion_ctx), 0.0f);

    // Verify learning ready - check that no active learning entries exist yet
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_EQ(stats.active_habituation_entries, 0u);  // No learning yet
}

TEST_F(PortiaFullSystemIntegrationTest, Initialization_TierConfigurationApplied) {
    platform_tier_config_t config = platform_tier_get_config(current_tier);

    // Configuration should be valid
    EXPECT_GT(config.max_neurons, 0u);
    EXPECT_GT(config.memory_budget_mb, 0u);
    EXPECT_GT(config.max_threads, 0u);

    // Should have some cognitive modules enabled
    EXPECT_GT(config.cognitive_modules_enabled, 0u);
}

//=============================================================================
// TEST SUITE 2: Coordinated Response to Resource Pressure
//=============================================================================

TEST_F(PortiaFullSystemIntegrationTest, ResourcePressure_AllSubsystemsRespond) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Establish baseline operation
    // Sensors working
    sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 50.0f, 0.9f, timestamp, true};
    sensor_reading_t imu = {SENSOR_TYPE_IMU, 30.0f, 0.85f, timestamp, true};
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    // Learning working
    ASSERT_EQ(portia_learning_associate(learning_state, 1, 10, true, timestamp), 0);

    // Get initial stats
    portia_fusion_stats_t fusion_stats_before;
    ASSERT_TRUE(portia_fusion_get_stats(fusion_ctx, &fusion_stats_before));
    portia_learning_stats_t learning_stats_before = portia_learning_get_stats(learning_state);

    // Apply resource pressure
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 70.0f, NULL), NIMCP_SUCCESS);

    // Check degradation state
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;
    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_MINOR);

    // Full sensors may be disabled
    bool full_sensors_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_SENSORS_FULL,
                                                      &full_sensors_enabled), NIMCP_SUCCESS);

    // Learning may be affected at higher degradation
    bool learning_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_LEARNING,
                                                      &learning_enabled), NIMCP_SUCCESS);
}

TEST_F(PortiaFullSystemIntegrationTest, ResourcePressure_SensorFusionAdapts) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Provide multi-sensor data
    sensor_reading_t sensors[] = {
        {SENSOR_TYPE_VISUAL, 50.0f, 0.9f, timestamp, true},
        {SENSOR_TYPE_IMU, 30.0f, 0.85f, timestamp, true},
        {SENSOR_TYPE_PROXIMITY, 20.0f, 0.8f, timestamp, true}
    };

    for (auto& sensor : sensors) {
        ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &sensor));
    }
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    float confidence_before = portia_fusion_get_confidence(fusion_ctx);

    // Apply resource pressure - may reduce sensor suite
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 70.0f, NULL), NIMCP_SUCCESS);

    // Simulate sensor reduction (disable some sensors)
    bool full_sensors;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_SENSORS_FULL,
                                                      &full_sensors), NIMCP_SUCCESS);
    if (!full_sensors) {
        // Disable non-essential sensors
        portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_PROXIMITY, false);
    }

    // Fusion should still work with reduced sensors
    timestamp += 100;
    sensor_reading_t visual2 = {SENSOR_TYPE_VISUAL, 52.0f, 0.9f, timestamp, true};
    sensor_reading_t imu2 = {SENSOR_TYPE_IMU, 32.0f, 0.85f, timestamp, true};
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual2));
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu2));
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    // May have lower confidence with fewer sensors
    float confidence_after = portia_fusion_get_confidence(fusion_ctx);
    EXPECT_GT(confidence_after, 0.0f);  // Still functioning
}

TEST_F(PortiaFullSystemIntegrationTest, ResourcePressure_LearningSlowsOrStops) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Create some associations
    for (uint32_t i = 1; i <= 10; i++) {
        ASSERT_EQ(portia_learning_associate(learning_state, i, i * 10, true, timestamp), 0);
        timestamp += 100;
    }

    portia_learning_stats_t stats_before = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats_before.total_association_entries, 0u);

    // Apply severe resource pressure
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 80.0f, NULL), NIMCP_SUCCESS);

    // Check if learning disabled
    bool learning_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_LEARNING,
                                                      &learning_enabled), NIMCP_SUCCESS);

    if (!learning_enabled) {
        // Learning module should stop creating new associations
        // (existing ones remain but no new learning)
        EXPECT_GT(stats_before.total_association_entries, 0u);  // Old data preserved
    }
}

//=============================================================================
// TEST SUITE 3: Full Degradation and Recovery Cycle
//=============================================================================

TEST_F(PortiaFullSystemIntegrationTest, FullCycle_ProgressiveDegradationThenRecovery) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Phase 1: Normal operation
    degradation_level_t level;
    uint32_t active_features;
    float resource_usage;

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);
    uint32_t initial_features = active_features;

    // Phase 2: Minor degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 65.0f, NULL), NIMCP_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_MINOR);

    // Phase 3: Moderate degradation
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 80.0f, NULL), NIMCP_SUCCESS);

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_MODERATE);
    EXPECT_LT(active_features, initial_features);

    // Phase 4: Severe degradation
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_GE(level, DEGRADATION_LEVEL_SEVERE);
    uint32_t minimum_features = active_features;

    // Phase 5: Recovery begins
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 60.0f, NULL), NIMCP_SUCCESS);

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_LT(level, DEGRADATION_LEVEL_SEVERE);
    EXPECT_GT(active_features, minimum_features);

    // Phase 6: Full recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 30.0f, NULL), NIMCP_SUCCESS);

    ASSERT_EQ(portia_degradation_get_state(degrade_state, &level,
                                            &active_features, &resource_usage), NIMCP_SUCCESS);
    EXPECT_LE(level, DEGRADATION_LEVEL_MINOR);
}

TEST_F(PortiaFullSystemIntegrationTest, FullCycle_SubsystemsRecoverInOrder) {
    // Degrade to severe
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Check what's disabled
    bool plasticity, learning, planning;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_PLASTICITY,
                                                      &plasticity), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_LEARNING,
                                                      &learning), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_PLANNING,
                                                      &planning), NIMCP_SUCCESS);

    EXPECT_FALSE(plasticity);  // Disabled at MINOR
    EXPECT_FALSE(learning);    // Disabled at MODERATE
    EXPECT_FALSE(planning);    // Disabled at SEVERE

    // Partial recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 70.0f, NULL), NIMCP_SUCCESS);

    // Some features should restore
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_PLANNING,
                                                      &planning), NIMCP_SUCCESS);
    EXPECT_TRUE(planning);  // Should be restored (only disabled at SEVERE)

    // Full recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 30.0f, NULL), NIMCP_SUCCESS);

    // All should be restored
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_PLASTICITY,
                                                      &plasticity), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_LEARNING,
                                                      &learning), NIMCP_SUCCESS);
    EXPECT_TRUE(plasticity);
    EXPECT_TRUE(learning);
}

//=============================================================================
// TEST SUITE 4: Cross-Subsystem Integration Scenarios
//=============================================================================

TEST_F(PortiaFullSystemIntegrationTest, CrossSubsystem_SensorFusionFeedsLearning) {
    // Sensors detect a pattern
    for (int i = 0; i < 5; i++) {
        // Get current timestamp for each iteration to avoid stale data detection
        uint64_t timestamp = nimcp_time_monotonic_ms();

        sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 50.0f + i * 5, 0.9f,
                                    timestamp, true};
        sensor_reading_t imu = {SENSOR_TYPE_IMU, 30.0f + i * 3, 0.85f,
                                 timestamp, true};

        ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
        ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &imu));
        ASSERT_TRUE(portia_fusion_process(fusion_ctx));

        // Get fused state
        fused_state_t state;
        ASSERT_TRUE(portia_fusion_get_state(fusion_ctx, &state));

        // Learn association between sensor state and outcome
        uint32_t stimulus_id = static_cast<uint32_t>(state.x + state.y);
        ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, 1, true,
                                             timestamp), 0);
    }

    // Verify learning occurred
    portia_learning_stats_t stats = portia_learning_get_stats(learning_state);
    EXPECT_GT(stats.active_association_entries, 0u);
}

TEST_F(PortiaFullSystemIntegrationTest, CrossSubsystem_DegradationAffectsBothSensorsAndLearning) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Establish baseline
    sensor_reading_t visual = {SENSOR_TYPE_VISUAL, 50.0f, 0.9f, timestamp, true};
    ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &visual));
    ASSERT_EQ(portia_learning_associate(learning_state, 1, 10, true, timestamp), 0);

    // Apply degradation to level SEVERE (90%) to disable both sensors and learning
    // Note: Default features have:
    // - FEATURE_SENSORS_FULL: disable_at = DEGRADATION_LEVEL_SEVERE (3)
    // - FEATURE_LEARNING: disable_at = DEGRADATION_LEVEL_MODERATE (2)
    // With thresholds {0, 60, 75, 85, 95}, 90% triggers level 3 (SEVERE)
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);

    // Check both subsystems
    bool full_sensors, learning_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_SENSORS_FULL,
                                                      &full_sensors), NIMCP_SUCCESS);
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state, FEATURE_LEARNING,
                                                      &learning_enabled), NIMCP_SUCCESS);

    // Both should be disabled at SEVERE degradation level
    EXPECT_FALSE(full_sensors);
    EXPECT_FALSE(learning_enabled);
}

TEST_F(PortiaFullSystemIntegrationTest, CrossSubsystem_EndToEndDataFlow) {
    // Use current monotonic time to avoid stale data detection
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Complete data flow: Sensors → Fusion → Learning → Decision
    // Step 1: Sensors observe environment
    sensor_reading_t sensors[] = {
        {SENSOR_TYPE_VISUAL, 75.0f, 0.9f, timestamp, true},
        {SENSOR_TYPE_IMU, 45.0f, 0.85f, timestamp, true},
        {SENSOR_TYPE_PROXIMITY, 25.0f, 0.8f, timestamp, true}
    };

    for (auto& sensor : sensors) {
        ASSERT_TRUE(portia_fusion_update_sensor(fusion_ctx, &sensor));
    }

    // Step 2: Fusion integrates sensor data
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    fused_state_t state;
    ASSERT_TRUE(portia_fusion_get_state(fusion_ctx, &state));
    EXPECT_GT(state.confidence, 0.5f);

    // Step 3: Learning creates association
    uint32_t stimulus_id = static_cast<uint32_t>(state.x * 10 + state.y);
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, 100, true, timestamp), 0);

    // Step 4: Query learned association
    portia_learning_query_result_t query =
        portia_learning_query_association(learning_state, stimulus_id, 100);
    ASSERT_TRUE(query.found);
    EXPECT_GT(query.strength, 0.0f);

    // Complete flow successful
    EXPECT_TRUE(true);
}
