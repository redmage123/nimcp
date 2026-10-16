/**
 * @file test_guardian_brain_integration.cpp
 * @brief Integration tests for Mental Health Guardian with Brain
 *
 * WHAT: Tests guardian integration with brain factory and lifecycle
 * WHY:  Verify guardian works correctly within brain infrastructure
 * HOW:  Test factory init, brain accessors, lifecycle coordination
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/mental_health/nimcp_mental_health_guardian.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"

// =============================================================================
// TEST FIXTURE
// =============================================================================

class GuardianBrainIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create brain with guardian enabled (default)
        brain = brain_create("guardian_integration_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Enable and initialize mental health subsystem (TINY brains have it disabled)
        brain->config.enable_mental_health_monitoring = true;
        ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain))
            << "Failed to initialize mental health subsystem";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// =============================================================================
// BRAIN FACTORY INTEGRATION TESTS
// =============================================================================

TEST_F(GuardianBrainIntegrationTest, BrainHasMentalHealthMonitor) {
    // Prerequisite: brain must have mental health monitor
    EXPECT_NE(brain->mental_health_monitor, nullptr);
}

TEST_F(GuardianBrainIntegrationTest, GuardianCreatedByFactory) {
    // Guardian should be created when enable_mental_health_guardian is true
    // Note: Guardian may or may not be auto-created depending on config
    // This test verifies the factory init function works correctly

    if (brain->config.enable_mental_health_guardian) {
        // If enabled, guardian should exist
        EXPECT_NE(brain->mental_health_guardian, nullptr);
    }
}

TEST_F(GuardianBrainIntegrationTest, ManualGuardianCreation) {
    // Create guardian manually if not auto-created
    if (brain->mental_health_guardian == nullptr) {
        mental_health_guardian_config_t config = mental_health_guardian_default_config();
        brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    }
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Verify it works
    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_STOPPED);
}

TEST_F(GuardianBrainIntegrationTest, BrainAccessorFunctions) {
    // Test accessor functions if guardian exists
    if (brain->mental_health_guardian == nullptr) {
        mental_health_guardian_config_t config = mental_health_guardian_default_config();
        brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    }

    // Get guardian via accessor
    mental_health_guardian_t* guardian = brain_get_mental_health_guardian(brain);
    EXPECT_EQ(guardian, brain->mental_health_guardian);

    // Start via accessor
    EXPECT_TRUE(brain_start_mental_health_guardian(brain));

    mental_health_guardian_status_t status;
    EXPECT_TRUE(brain_get_mental_health_guardian_status(brain, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);

    // Stop via accessor
    EXPECT_TRUE(brain_stop_mental_health_guardian(brain));
    EXPECT_TRUE(brain_get_mental_health_guardian_status(brain, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_STOPPED);
}

// =============================================================================
// MENTAL HEALTH MODULE INTEGRATION TESTS
// =============================================================================

TEST_F(GuardianBrainIntegrationTest, GuardianUsesCorrectMentalHealthMonitor) {
    if (brain->mental_health_guardian == nullptr) {
        mental_health_guardian_config_t config = mental_health_guardian_default_config();
        brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    }

    // Force a check and verify it uses the brain's mental health monitor
    guardian_intervention_level_t level =
        mental_health_guardian_force_check(brain->mental_health_guardian);

    // Should return a valid level (verifies it ran)
    EXPECT_GE(level, GUARDIAN_LEVEL_OBSERVE);
    EXPECT_LE(level, GUARDIAN_LEVEL_QUARANTINE);

    // Verify mental health module was used (check count should reflect)
    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
    EXPECT_GT(status.checks_performed, 0u);
}

TEST_F(GuardianBrainIntegrationTest, GuardianDetectsDisorders) {
    if (brain->mental_health_guardian == nullptr) {
        mental_health_guardian_config_t config = mental_health_guardian_default_config();
        brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    }

    // Force check and examine severity
    mental_health_guardian_force_check(brain->mental_health_guardian);

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));

    // Severity should be a valid float in [0, 1]
    EXPECT_GE(status.overall_severity, 0.0f);
    EXPECT_LE(status.overall_severity, 1.0f);
}

// =============================================================================
// LIFECYCLE COORDINATION TESTS
// =============================================================================

TEST_F(GuardianBrainIntegrationTest, GuardianDestroyedWithBrain) {
    // Create guardian
    if (brain->mental_health_guardian == nullptr) {
        mental_health_guardian_config_t config = mental_health_guardian_default_config();
        brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    }
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Start it
    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Destroy brain - should cleanly destroy guardian too
    brain_destroy(brain);
    brain = nullptr;  // Prevent double-free in TearDown

    // No crash means success
}

TEST_F(GuardianBrainIntegrationTest, MultipleStartStopCycles) {
    if (brain->mental_health_guardian == nullptr) {
        mental_health_guardian_config_t config = mental_health_guardian_default_config();
        config.monitoring_interval_ms = 20;
        brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    }

    // Multiple start/stop cycles should work correctly
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        mental_health_guardian_status_t status;
        EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
        EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);

        EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));
        EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
        EXPECT_EQ(status.state, GUARDIAN_STATE_STOPPED);
    }
}

// =============================================================================
// CONTINUOUS MONITORING TEST
// =============================================================================

TEST_F(GuardianBrainIntegrationTest, ContinuousMonitoring) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 10;  // Fast for testing
    config.auto_intervene = true;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Start monitoring
    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));

    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check that multiple checks happened
    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
    EXPECT_GT(status.checks_performed, 3u);  // Should have at least 3-4 checks in 100ms

    // Stop
    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));
}
