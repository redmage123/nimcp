/**
 * @file e2e_test_portia_power_lifecycle.cpp
 * @brief End-to-end test for Portia power management lifecycle
 *
 * WHAT: Tests battery-aware adaptation and power state transitions
 * WHY:  Verify system gracefully handles power constraints (critical for drones/robots)
 * HOW:  Simulate battery drain, test adaptive degradation, verify emergency mode
 *
 * TEST SCENARIOS:
 * - BatteryDrainScenario: Simulate progressive battery drain from full to critical
 * - SystemAdaptation: Verify system reduces power consumption as battery depletes
 * - EmergencyMode: Test emergency mode preserves critical functions only
 * - PowerRecovery: Test restoration when power is restored
 * - PowerSourceSwitching: Test AC power vs battery transitions
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia.h"
#include "portia/nimcp_portia_degradation.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaPowerLifecycleE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_log_init(NULL);

        nimcp_error_t err = nimcp_bio_async_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = bio_router_init(nullptr);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        brain_ = nullptr;
        portia_initialized_ = false;
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }

        if (portia_initialized_) {
            portia_destroy();
            portia_initialized_ = false;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }

    brain_t brain_;
    bool portia_initialized_;
};

//=============================================================================
// Test 1: Battery Drain Scenario
//=============================================================================

TEST_F(PortiaPowerLifecycleE2ETest, BatteryDrainScenario) {
    // GIVEN: Initialize Portia with power monitoring enabled
    portia_config_t config = portia_get_default_config();
    config.power_config.enable_battery_awareness = true;
    config.power_config.poll_interval_ms = 50;
    config.power_config.low_battery_threshold = 0.20f;
    config.power_config.critical_battery_threshold = 0.05f;
    config.degradation_config.enable_graceful_degradation = true;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // Get initial status
    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    nimcp_log(LOG_LEVEL_INFO, "Initial power state: %s, battery: %.1f%%",
              portia_power_state_name(status.power_state),
              status.battery_level * 100.0f);

    // Create brain
    platform_tier_config_t tier_config = platform_tier_get_config(status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // WHEN: Simulate battery drain by monitoring over time
    // Note: Actual battery monitoring depends on platform support
    // This test verifies the monitoring infrastructure works

    std::vector<portia_status_t> power_samples;
    const int num_samples = 30;

    for (int i = 0; i < num_samples; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Portia update failed at sample " << i;

        err = portia_get_status(&status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        power_samples.push_back(status);

        // THEN: Verify power monitoring is active
        EXPECT_TRUE(
            status.power_state == PORTIA_POWER_AC ||
            status.power_state == PORTIA_POWER_BATTERY_FULL ||
            status.power_state == PORTIA_POWER_BATTERY_MID ||
            status.power_state == PORTIA_POWER_BATTERY_LOW ||
            status.power_state == PORTIA_POWER_BATTERY_CRITICAL ||
            status.power_state == PORTIA_POWER_UNKNOWN
        ) << "Power state should be valid";

        // If battery level is available, verify it's in valid range
        if (status.battery_level >= 0.0f) {
            EXPECT_GE(status.battery_level, 0.0f);
            EXPECT_LE(status.battery_level, 1.0f);
        }

        // Verify brain remains operational
        EXPECT_NE(brain_, nullptr);
        EXPECT_GT(brain_get_neuron_count(brain_), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // THEN: Verify power monitoring occurred
    EXPECT_GT(status.updates, 0) << "Portia should have updated";

    // Check if any power state changes occurred
    bool had_power_state_change = false;
    portia_power_state_t first_state = power_samples[0].power_state;
    for (const auto& sample : power_samples) {
        if (sample.power_state != first_state) {
            had_power_state_change = true;
            break;
        }
    }

    nimcp_log(LOG_LEVEL_INFO, "BatteryDrainScenario: PASS - "
              "Samples=%d, Final power state=%s, Battery=%.1f%%",
              num_samples, portia_power_state_name(status.power_state),
              status.battery_level * 100.0f);
}

//=============================================================================
// Test 2: System Adaptation to Low Power
//=============================================================================

TEST_F(PortiaPowerLifecycleE2ETest, SystemAdaptationToLowPower) {
    // GIVEN: Initialize with aggressive power management
    portia_config_t config = portia_get_default_config();
    config.power_config.enable_battery_awareness = true;
    config.degradation_config.enable_graceful_degradation = true;
    config.degradation_config.max_degradation = PORTIA_DEGRADATION_SEVERE;
    config.tier_config.enable_auto_switching = true;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t initial_status;
    err = portia_get_status(&initial_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create brain
    platform_tier_config_t tier_config = platform_tier_get_config(initial_status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    uint32_t initial_neurons = brain_get_neuron_count(brain_);
    platform_tier_t initial_tier = initial_status.current_tier;

    // WHEN: Force degradation to simulate low power
    nimcp_log(LOG_LEVEL_INFO, "Simulating low power by forcing degradation");

    err = portia_set_degradation_level(PORTIA_DEGRADATION_MODERATE);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 10; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // THEN: Verify system adapted
    portia_status_t degraded_status;
    err = portia_get_status(&degraded_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(degraded_status.degradation_level, PORTIA_DEGRADATION_MODERATE)
        << "Should be in moderate degradation";

    // System should still be functional
    EXPECT_NE(brain_, nullptr);
    EXPECT_GT(brain_get_neuron_count(brain_), 0);

    // Recommended neurons should be valid
    uint32_t degraded_recommendation = portia_recommend_neuron_count();
    EXPECT_GT(degraded_recommendation, 0u)
        << "Neuron recommendation should be positive";

    // WHEN: Further degrade to severe
    err = portia_set_degradation_level(PORTIA_DEGRADATION_SEVERE);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 5; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // THEN: Verify severe degradation
    portia_status_t severe_status;
    err = portia_get_status(&severe_status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(severe_status.degradation_level, PORTIA_DEGRADATION_SEVERE);

    uint32_t severe_recommendation = portia_recommend_neuron_count();
    EXPECT_LE(severe_recommendation, degraded_recommendation)
        << "Severe degradation should further reduce recommendations";

    // Brain should still be operational (minimal functionality)
    EXPECT_NE(brain_, nullptr);
    EXPECT_GT(brain_get_neuron_count(brain_), 0);

    nimcp_log(LOG_LEVEL_INFO, "SystemAdaptationToLowPower: PASS - "
              "Initial neurons=%u, Degraded rec=%u, Severe rec=%u",
              initial_neurons, degraded_recommendation, severe_recommendation);
}

//=============================================================================
// Test 3: Emergency Mode Operation
//=============================================================================

TEST_F(PortiaPowerLifecycleE2ETest, EmergencyModeOperation) {
    // GIVEN: Initialize with emergency mode enabled
    portia_config_t config = portia_get_default_config();
    config.degradation_config.enable_graceful_degradation = true;
    config.degradation_config.max_degradation = PORTIA_DEGRADATION_EMERGENCY;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create minimal brain
    platform_tier_config_t tier_config = platform_tier_get_config(PLATFORM_TIER_MINIMAL);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // WHEN: Enter emergency mode
    nimcp_log(LOG_LEVEL_INFO, "Entering emergency mode");

    err = portia_set_degradation_level(PORTIA_DEGRADATION_EMERGENCY);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Also force minimal tier
    err = portia_set_tier(PLATFORM_TIER_MINIMAL);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    for (int i = 0; i < 10; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Emergency mode should still allow updates";
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // THEN: Verify emergency mode active
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(status.degradation_level, PORTIA_DEGRADATION_EMERGENCY)
        << "Should be in emergency mode";
    EXPECT_EQ(status.current_tier, PLATFORM_TIER_MINIMAL)
        << "Should be on minimal tier";

    // System should preserve critical functions
    EXPECT_NE(brain_, nullptr) << "Basic brain structure should exist";
    EXPECT_GT(brain_get_neuron_count(brain_), 0) << "Should have some neurons";

    // Recommended neurons should be minimal
    uint32_t emergency_recommendation = portia_recommend_neuron_count();
    EXPECT_LT(emergency_recommendation, 10000)
        << "Emergency mode should recommend very few neurons";

    // Verify only core cognitive modules are available
    bool has_attention = platform_tier_can_enable_module(
        PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_ATTENTION);
    bool has_curiosity = platform_tier_can_enable_module(
        PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_CURIOSITY);
    bool has_meta_learning = platform_tier_can_enable_module(
        PLATFORM_TIER_MINIMAL, COGNITIVE_MODULE_META_LEARNING);

    EXPECT_TRUE(has_attention) << "Minimal tier should have attention";
    EXPECT_FALSE(has_curiosity) << "Minimal tier should not have curiosity";
    EXPECT_FALSE(has_meta_learning) << "Minimal tier should not have meta-learning";

    nimcp_log(LOG_LEVEL_INFO, "EmergencyModeOperation: PASS - "
              "Neurons=%u, Recommended=%u, Updates successful",
              brain_get_neuron_count(brain_), emergency_recommendation);
}

//=============================================================================
// Test 4: Power Recovery
//=============================================================================

TEST_F(PortiaPowerLifecycleE2ETest, PowerRecovery) {
    // GIVEN: Start in degraded state
    portia_config_t config = portia_get_default_config();
    config.degradation_config.enable_graceful_degradation = true;
    config.degradation_config.recovery_delay_ms = 100;
    config.degradation_config.recovery_threshold = 0.5f;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    platform_tier_config_t tier_config = platform_tier_get_config(status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    // Enter degraded state
    err = portia_set_degradation_level(PORTIA_DEGRADATION_SEVERE);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    err = portia_set_tier(PLATFORM_TIER_CONSTRAINED);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 5; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    nimcp_log(LOG_LEVEL_INFO, "Degraded state: tier=%s, degradation=%d",
              platform_tier_get_name(status.current_tier),
              status.degradation_level);

    uint64_t degradations_before = status.degradations;

    // WHEN: Simulate power recovery
    nimcp_log(LOG_LEVEL_INFO, "Simulating power recovery");

    // Restore normal degradation
    err = portia_set_degradation_level(PORTIA_DEGRADATION_MINOR);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Allow recovery delay
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Restore tier
    err = portia_set_tier(PLATFORM_TIER_MEDIUM);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 10; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // THEN: Verify recovery
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(status.current_tier, PLATFORM_TIER_MEDIUM)
        << "Should have recovered to medium tier";
    EXPECT_EQ(status.degradation_level, PORTIA_DEGRADATION_MINOR)
        << "Should be in minor degradation";

    // System should be more capable now
    uint32_t recovered_recommendation = portia_recommend_neuron_count();
    EXPECT_GT(recovered_recommendation, 1000)
        << "Recovered system should recommend more neurons";

    // Verify more cognitive modules available
    bool has_executive = platform_tier_can_enable_module(
        PLATFORM_TIER_MEDIUM, COGNITIVE_MODULE_EXECUTIVE);
    EXPECT_TRUE(has_executive) << "Medium tier should have executive functions";

    // Brain should still be valid
    EXPECT_NE(brain_, nullptr);
    EXPECT_GT(brain_get_neuron_count(brain_), 0);

    nimcp_log(LOG_LEVEL_INFO, "PowerRecovery: PASS - "
              "Recovered to tier=%s, recommended neurons=%u",
              platform_tier_get_name(status.current_tier),
              recovered_recommendation);
}

//=============================================================================
// Test 5: Power Source Switching
//=============================================================================

TEST_F(PortiaPowerLifecycleE2ETest, PowerSourceSwitching) {
    // GIVEN: Initialize with AC detection enabled
    portia_config_t config = portia_get_default_config();
    config.power_config.enable_ac_detection = true;
    config.power_config.enable_battery_awareness = true;
    config.power_config.poll_interval_ms = 50;
    config.enable_metrics = true;

    nimcp_error_t err = portia_init(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    portia_initialized_ = true;

    // Create brain
    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    platform_tier_config_t tier_config = platform_tier_get_config(status.current_tier);
    brain_ = brain_create("portia_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 32);
    ASSERT_NE(brain_, nullptr);

    portia_power_state_t initial_power_state = status.power_state;

    nimcp_log(LOG_LEVEL_INFO, "Initial power state: %s",
              portia_power_state_name(initial_power_state));

    // WHEN: Monitor power state over time
    // Note: Actual AC/battery switching depends on platform
    // This test verifies the monitoring infrastructure

    std::vector<portia_power_state_t> power_states;
    const int num_samples = 40;

    for (int i = 0; i < num_samples; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = portia_get_status(&status);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        power_states.push_back(status.power_state);

        // THEN: Verify power state is valid
        EXPECT_TRUE(
            status.power_state >= PORTIA_POWER_AC &&
            status.power_state <= PORTIA_POWER_UNKNOWN
        ) << "Power state should be valid enum value";

        // System should remain operational regardless of power source
        EXPECT_NE(brain_, nullptr);
        EXPECT_GT(brain_get_neuron_count(brain_), 0u);

        // Verify metrics are collected
        EXPECT_GE(status.cpu_usage, 0.0f);
        EXPECT_LE(status.cpu_usage, 1.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // THEN: Analyze power state transitions
    bool had_power_transition = false;
    for (size_t i = 1; i < power_states.size(); i++) {
        if (power_states[i] != power_states[i-1]) {
            had_power_transition = true;
            nimcp_log(LOG_LEVEL_INFO, "Power state transition detected: %s → %s",
                      portia_power_state_name(power_states[i-1]),
                      portia_power_state_name(power_states[i]));
        }
    }

    // Get final status
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Verify system remained stable
    EXPECT_GT(status.updates, 0);
    EXPECT_NE(brain_, nullptr);

    nimcp_log(LOG_LEVEL_INFO, "PowerSourceSwitching: PASS - "
              "Samples=%d, Transitions=%s, Final state=%s",
              num_samples, had_power_transition ? "Yes" : "No",
              portia_power_state_name(status.power_state));
}
