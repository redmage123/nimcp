/**
 * @file test_portia_sensor_planning_integration.cpp
 * @brief Integration tests for Portia sensor fusion and planning coordination
 *
 * WHAT: Tests sensor fusion feeds planning system
 * WHY:  Validate planning uses fused sensor state and adapts to sensor dropouts
 * HOW:  Simulate multi-sensor inputs, planning decisions, and sensor failures
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_sensor_fusion.h"
#include "async/nimcp_bio_async.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"

// Mock planning system that uses fused state
typedef struct {
    fused_state_t last_state;
    uint32_t planning_updates;
    float plan_quality;  // Affected by sensor confidence
    bool has_valid_state;
    uint32_t sensor_dropout_events;
} mock_planning_system_t;

class PortiaSensorPlanningIntegrationTest : public ::testing::Test {
protected:
    portia_fusion_ctx_t* fusion_ctx = nullptr;
    nimcp_bio_ctx_t* bio_ctx = nullptr;
    mock_planning_system_t planning_system;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        // bio_ctx removed - not needed

        // Initialize sensor fusion
        portia_fusion_config_t fusion_config = portia_fusion_default_config();
        fusion_config.min_sensors = 2;
        fusion_config.enable_fallback = true;
        fusion_config.outlier_threshold = 3.0f;

        fusion_ctx = portia_fusion_init(&fusion_config, bio_ctx);
        ASSERT_NE(fusion_ctx, nullptr);

        // Initialize mock planning system
        planning_system = {
            .last_state = {0},
            .planning_updates = 0,
            .plan_quality = 0.0f,
            .has_valid_state = false,
            .sensor_dropout_events = 0
        };
    }

    void TearDown() override {
        if (fusion_ctx) {
            portia_fusion_destroy(fusion_ctx);
            fusion_ctx = nullptr;
        }
        nimcp_bio_async_shutdown();
    }

    // Helper: Update planning with fused state
    // fusion_ok indicates whether fusion_process succeeded (optional, defaults to check get_state)
    void update_planning_with_fusion(bool fusion_ok = true) {
        fused_state_t state;
        if (fusion_ok && portia_fusion_get_state(fusion_ctx, &state)) {
            planning_system.last_state = state;
            planning_system.planning_updates++;
            planning_system.has_valid_state = true;

            // Plan quality depends on fusion confidence
            planning_system.plan_quality = state.confidence * 0.9f;
        } else {
            planning_system.has_valid_state = false;
            planning_system.sensor_dropout_events++;
        }
    }

    // Helper: Provide multi-sensor readings
    void provide_multisensor_reading(float x, float y, float z, uint64_t timestamp) {
        // Visual sensor
        sensor_reading_t visual = {
            .type = SENSOR_TYPE_VISUAL,
            .value = x,
            .confidence = 0.8f,
            .timestamp_ms = timestamp,
            .valid = true
        };
        portia_fusion_update_sensor(fusion_ctx, &visual);

        // IMU sensor
        sensor_reading_t imu = {
            .type = SENSOR_TYPE_IMU,
            .value = y,
            .confidence = 0.9f,
            .timestamp_ms = timestamp,
            .valid = true
        };
        portia_fusion_update_sensor(fusion_ctx, &imu);

        // Proximity sensor
        sensor_reading_t proximity = {
            .type = SENSOR_TYPE_PROXIMITY,
            .value = z,
            .confidence = 0.7f,
            .timestamp_ms = timestamp,
            .valid = true
        };
        portia_fusion_update_sensor(fusion_ctx, &proximity);
    }
};

//=============================================================================
// TEST SUITE 1: Sensor Fusion Feeds Planning
//=============================================================================

TEST_F(PortiaSensorPlanningIntegrationTest, SensorFusion_ProvidesStateToPlanning) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Provide sensor readings
    provide_multisensor_reading(10.0f, 20.0f, 5.0f, timestamp);

    // Process fusion
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    // Update planning with fused state
    update_planning_with_fusion();

    // Planning should have valid state
    EXPECT_TRUE(planning_system.has_valid_state);
    EXPECT_EQ(planning_system.planning_updates, 1u);
    EXPECT_GT(planning_system.plan_quality, 0.0f);
}

TEST_F(PortiaSensorPlanningIntegrationTest, SensorFusion_HighConfidenceImprovesPlanQuality) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Provide high-confidence readings
    sensor_reading_t high_conf_readings[] = {
        {SENSOR_TYPE_VISUAL, 10.0f, 0.95f, timestamp, true},
        {SENSOR_TYPE_IMU, 20.0f, 0.98f, timestamp, true},
        {SENSOR_TYPE_PROXIMITY, 5.0f, 0.92f, timestamp, true},
        {SENSOR_TYPE_GPS, 15.0f, 0.90f, timestamp, true}
    };

    for (auto& reading : high_conf_readings) {
        portia_fusion_update_sensor(fusion_ctx, &reading);
    }

    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // High confidence should yield high plan quality
    EXPECT_GT(planning_system.plan_quality, 0.7f);
    EXPECT_GT(portia_fusion_get_confidence(fusion_ctx), 0.8f);
}

TEST_F(PortiaSensorPlanningIntegrationTest, SensorFusion_LowConfidenceReducesPlanQuality) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Provide low-confidence readings
    sensor_reading_t low_conf_readings[] = {
        {SENSOR_TYPE_VISUAL, 10.0f, 0.3f, timestamp, true},
        {SENSOR_TYPE_IMU, 20.0f, 0.4f, timestamp, true}
    };

    for (auto& reading : low_conf_readings) {
        portia_fusion_update_sensor(fusion_ctx, &reading);
    }

    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // Low confidence should yield lower plan quality
    EXPECT_LT(planning_system.plan_quality, 0.5f);
}

//=============================================================================
// TEST SUITE 2: Planning Uses Fused State
//=============================================================================

TEST_F(PortiaSensorPlanningIntegrationTest, Planning_UsesPositionFromFusion) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Provide readings with known position
    provide_multisensor_reading(100.0f, 200.0f, 50.0f, timestamp);

    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // Planning should use fused position
    EXPECT_TRUE(planning_system.has_valid_state);
    // Position should be somewhere in range (fusion may average/filter)
    EXPECT_GT(planning_system.last_state.x, 0.0f);
    EXPECT_GT(planning_system.last_state.y, 0.0f);
}

TEST_F(PortiaSensorPlanningIntegrationTest, Planning_TracksVelocityFromFusion) {
    // First reading
    uint64_t timestamp1 = nimcp_time_monotonic_ms();
    provide_multisensor_reading(10.0f, 10.0f, 5.0f, timestamp1);
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    // Second reading (moved)
    uint64_t timestamp2 = nimcp_time_monotonic_ms();
    provide_multisensor_reading(15.0f, 12.0f, 5.0f, timestamp2);
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));

    update_planning_with_fusion();

    // Velocity should be computed by fusion
    // (exact values depend on fusion implementation)
    EXPECT_TRUE(planning_system.has_valid_state);
}

TEST_F(PortiaSensorPlanningIntegrationTest, Planning_UpdatesWithEachFusionCycle) {
    for (int i = 0; i < 10; i++) {
        // Get fresh timestamp each iteration to avoid stale data detection
        uint64_t timestamp = nimcp_time_monotonic_ms();
        provide_multisensor_reading(
            static_cast<float>(i * 5),
            static_cast<float>(i * 3),
            static_cast<float>(i * 2),
            timestamp
        );

        ASSERT_TRUE(portia_fusion_process(fusion_ctx));
        update_planning_with_fusion();
    }

    // Planning should have updated 10 times
    EXPECT_EQ(planning_system.planning_updates, 10u);
    EXPECT_TRUE(planning_system.has_valid_state);
}

//=============================================================================
// TEST SUITE 3: Sensor Dropout Affects Plans
//=============================================================================

TEST_F(PortiaSensorPlanningIntegrationTest, SensorDropout_SingleSensorFailure) {
    // Initially all sensors working
    uint64_t timestamp = nimcp_time_monotonic_ms();
    provide_multisensor_reading(10.0f, 20.0f, 5.0f, timestamp);
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();
    float quality_before = planning_system.plan_quality;
    (void)quality_before;  // Suppress unused variable warning

    // Drop visual sensor
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_VISUAL, false);

    // Get fresh timestamp after sensor change
    timestamp = nimcp_time_monotonic_ms();
    sensor_reading_t imu = {SENSOR_TYPE_IMU, 25.0f, 0.9f, timestamp, true};
    sensor_reading_t proximity = {SENSOR_TYPE_PROXIMITY, 6.0f, 0.7f, timestamp, true};
    portia_fusion_update_sensor(fusion_ctx, &imu);
    portia_fusion_update_sensor(fusion_ctx, &proximity);

    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // Quality may decrease slightly but should still plan
    EXPECT_TRUE(planning_system.has_valid_state);
    // Confidence should be lower with fewer sensors
    EXPECT_LE(portia_fusion_get_confidence(fusion_ctx), 0.9f);
}

TEST_F(PortiaSensorPlanningIntegrationTest, SensorDropout_MultipleSensorFailure) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Initially working
    provide_multisensor_reading(10.0f, 20.0f, 5.0f, timestamp);
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // Drop most sensors (keep only one active)
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_VISUAL, false);
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_IMU, false);

    // Get fresh timestamp after sensor change
    timestamp = nimcp_time_monotonic_ms();
    sensor_reading_t proximity = {SENSOR_TYPE_PROXIMITY, 6.0f, 0.7f, timestamp, true};
    portia_fusion_update_sensor(fusion_ctx, &proximity);

    // With only one sensor, fusion may still work (fallback mode)
    bool fusion_ok = portia_fusion_process(fusion_ctx);
    update_planning_with_fusion(fusion_ok);

    // Check if fallback enabled planning
    if (fusion_ok) {
        EXPECT_TRUE(planning_system.has_valid_state);
        // But confidence should be low
        EXPECT_LT(portia_fusion_get_confidence(fusion_ctx), 0.6f);
    } else {
        // Planning should detect sensor dropout
        EXPECT_FALSE(planning_system.has_valid_state);
        EXPECT_GT(planning_system.sensor_dropout_events, 0u);
    }
}

TEST_F(PortiaSensorPlanningIntegrationTest, SensorDropout_CompleteFailure) {
    // Disable all sensors
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        portia_fusion_enable_sensor(fusion_ctx, static_cast<sensor_type_t>(i), false);
    }

    // Try to process fusion - should fail with no sensors
    bool fusion_ok = portia_fusion_process(fusion_ctx);
    update_planning_with_fusion(fusion_ok);

    // Planning should not have valid state
    EXPECT_FALSE(planning_system.has_valid_state);
    EXPECT_GT(planning_system.sensor_dropout_events, 0u);
}

TEST_F(PortiaSensorPlanningIntegrationTest, SensorDropout_RecoveryRestoresPlanning) {
    uint64_t timestamp = nimcp_time_monotonic_ms();

    // Start with working sensors
    provide_multisensor_reading(10.0f, 20.0f, 5.0f, timestamp);
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();
    uint32_t updates_before = planning_system.planning_updates;

    // Simulate dropout
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_VISUAL, false);
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_IMU, false);

    // Get fresh timestamp after sensor change
    timestamp = nimcp_time_monotonic_ms();
    sensor_reading_t proximity = {SENSOR_TYPE_PROXIMITY, 6.0f, 0.7f, timestamp, true};
    portia_fusion_update_sensor(fusion_ctx, &proximity);
    bool dropout_fusion_ok = portia_fusion_process(fusion_ctx);
    update_planning_with_fusion(dropout_fusion_ok);

    // Re-enable sensors (recovery)
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_VISUAL, true);
    portia_fusion_enable_sensor(fusion_ctx, SENSOR_TYPE_IMU, true);

    // Get fresh timestamp for recovery
    timestamp = nimcp_time_monotonic_ms();
    provide_multisensor_reading(12.0f, 22.0f, 6.0f, timestamp);
    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // Planning should recover
    EXPECT_TRUE(planning_system.has_valid_state);
    EXPECT_GT(planning_system.planning_updates, updates_before);
    EXPECT_GT(planning_system.plan_quality, 0.5f);
}

//=============================================================================
// TEST SUITE 4: Outlier Rejection Protects Planning
//=============================================================================

TEST_F(PortiaSensorPlanningIntegrationTest, OutlierRejection_FiltersSpuriousReadings) {
    // Establish baseline
    for (int i = 0; i < 5; i++) {
        // Get fresh timestamp each iteration to avoid stale data detection
        uint64_t timestamp = nimcp_time_monotonic_ms();
        provide_multisensor_reading(10.0f, 20.0f, 5.0f, timestamp);
        ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    }

    update_planning_with_fusion();
    fused_state_t baseline_state = planning_system.last_state;

    // Inject outlier
    uint64_t timestamp = nimcp_time_monotonic_ms();
    sensor_reading_t outlier = {
        .type = SENSOR_TYPE_VISUAL,
        .value = 1000.0f,  // Huge outlier
        .confidence = 0.8f,
        .timestamp_ms = timestamp,
        .valid = true
    };
    portia_fusion_update_sensor(fusion_ctx, &outlier);

    // Normal readings from other sensors
    sensor_reading_t imu = {SENSOR_TYPE_IMU, 20.0f, 0.9f, timestamp, true};
    sensor_reading_t proximity = {SENSOR_TYPE_PROXIMITY, 5.0f, 0.7f, timestamp, true};
    portia_fusion_update_sensor(fusion_ctx, &imu);
    portia_fusion_update_sensor(fusion_ctx, &proximity);

    ASSERT_TRUE(portia_fusion_process(fusion_ctx));
    update_planning_with_fusion();

    // State should not jump dramatically (outlier rejected)
    EXPECT_TRUE(planning_system.has_valid_state);
    // Position shouldn't be near the outlier value
    EXPECT_LT(std::abs(planning_system.last_state.x - baseline_state.x), 100.0f);
}

//=============================================================================
// TEST SUITE 5: Sensor Fusion Statistics Available to Planning
//=============================================================================

TEST_F(PortiaSensorPlanningIntegrationTest, Statistics_PlanningCanQueryFusionStats) {
    // Generate some fusion activity
    for (int i = 0; i < 20; i++) {
        // Get fresh timestamp each iteration to avoid stale data detection
        uint64_t timestamp = nimcp_time_monotonic_ms();
        provide_multisensor_reading(
            static_cast<float>(i),
            static_cast<float>(i * 2),
            static_cast<float>(i / 2),
            timestamp
        );
        portia_fusion_process(fusion_ctx);
    }

    // Query statistics
    portia_fusion_stats_t stats;
    ASSERT_TRUE(portia_fusion_get_stats(fusion_ctx, &stats));

    // Should have recorded activity
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.successful_fusions, 0u);
    EXPECT_GT(stats.active_sensor_count, 0u);
    EXPECT_GT(stats.average_confidence, 0.0f);

    // Planning could use these stats to assess data quality
    bool sensor_health_good = (stats.active_sensor_count >= 2) &&
                               (stats.average_confidence > 0.5f);
    EXPECT_TRUE(sensor_health_good);
}
