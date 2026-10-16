/**
 * @file test_mental_health_guardian.cpp
 * @brief Unit tests for Mental Health Guardian (Phase 10.5.1)
 *
 * WHAT: Tests for the independent background monitoring agent
 * WHY:  Verify guardian correctly monitors, detects, and intervenes
 * HOW:  Test lifecycle, thread control, intervention levels, metrics
 *
 * TEST COVERAGE:
 * - Creation and destruction
 * - Configuration (default and custom)
 * - Thread start/stop/pause/resume
 * - Intervention level determination
 * - Status and metrics tracking
 * - Manual control API
 * - Thread safety
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/mental_health/nimcp_mental_health_guardian.h"

// =============================================================================
// TEST FIXTURE
// =============================================================================

class MentalHealthGuardianTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    mental_health_guardian_t* guardian = nullptr;

    void SetUp() override {
        // Create a brain with mental health monitoring enabled
        brain = brain_create("guardian_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Enable and initialize mental health subsystem (TINY brains have it disabled)
        brain->config.enable_mental_health_monitoring = true;
        ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain))
            << "Failed to initialize mental health subsystem";
        ASSERT_NE(brain->mental_health_monitor, nullptr)
            << "Brain should have mental health monitor";
    }

    void TearDown() override {
        if (guardian) {
            mental_health_guardian_destroy(guardian);
            guardian = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// =============================================================================
// CREATION AND DESTRUCTION TESTS
// =============================================================================

TEST_F(MentalHealthGuardianTest, DefaultConfigValues) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();

    EXPECT_EQ(config.monitoring_interval_ms, 100u);
    EXPECT_FLOAT_EQ(config.observe_threshold, 0.0f);
    EXPECT_FLOAT_EQ(config.adjust_threshold, 0.3f);
    EXPECT_FLOAT_EQ(config.regulate_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.quarantine_threshold, 0.8f);
    EXPECT_TRUE(config.auto_intervene);
    EXPECT_TRUE(config.immune_integration);
    EXPECT_TRUE(config.kg_integration);
    EXPECT_FLOAT_EQ(config.neuromod_adjust_strength, 0.1f);
    EXPECT_TRUE(config.enable_sleep_trigger);
    EXPECT_FALSE(config.verbose_logging);
}

TEST_F(MentalHealthGuardianTest, CreateWithDefaultConfig) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);
}

TEST_F(MentalHealthGuardianTest, CreateWithCustomConfig) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 50;
    config.auto_intervene = false;
    config.verbose_logging = true;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    // Verify config was applied
    mental_health_guardian_config_t retrieved_config;
    EXPECT_TRUE(mental_health_guardian_get_config(guardian, &retrieved_config));
    EXPECT_EQ(retrieved_config.monitoring_interval_ms, 50u);
    EXPECT_FALSE(retrieved_config.auto_intervene);
    EXPECT_TRUE(retrieved_config.verbose_logging);
}

TEST_F(MentalHealthGuardianTest, CreateWithNullBrain) {
    guardian = mental_health_guardian_create(nullptr, nullptr);
    EXPECT_EQ(guardian, nullptr);
}

TEST_F(MentalHealthGuardianTest, DestroyNull) {
    // Should not crash
    mental_health_guardian_destroy(nullptr);
}

TEST_F(MentalHealthGuardianTest, DestroyWhileRunning) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    // Start the guardian
    EXPECT_TRUE(mental_health_guardian_start(guardian));

    // Small delay to let thread start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Destroy should stop thread gracefully
    mental_health_guardian_destroy(guardian);
    guardian = nullptr;  // Prevent double-free in TearDown
}

// =============================================================================
// THREAD CONTROL TESTS
// =============================================================================

TEST_F(MentalHealthGuardianTest, StartStop) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status;

    // Initial state should be STOPPED
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_STOPPED);

    // Start
    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);

    // Stop
    EXPECT_TRUE(mental_health_guardian_stop(guardian));

    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_STOPPED);
}

TEST_F(MentalHealthGuardianTest, StartAlreadyRunning) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(guardian));

    // Starting again should return true (idempotent)
    EXPECT_TRUE(mental_health_guardian_start(guardian));

    mental_health_guardian_stop(guardian);
}

TEST_F(MentalHealthGuardianTest, StopAlreadyStopped) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    // Stopping when not running should return true (idempotent)
    EXPECT_TRUE(mental_health_guardian_stop(guardian));
}

TEST_F(MentalHealthGuardianTest, PauseResume) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    mental_health_guardian_status_t status;

    // Pause
    EXPECT_TRUE(mental_health_guardian_pause(guardian));
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_PAUSED);

    // Resume
    EXPECT_TRUE(mental_health_guardian_resume(guardian));
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);

    mental_health_guardian_stop(guardian);
}

// =============================================================================
// MONITORING AND METRICS TESTS
// =============================================================================

TEST_F(MentalHealthGuardianTest, PerformsHealthChecks) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 20;  // Fast for testing

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status_before;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status_before));
    EXPECT_EQ(status_before.checks_performed, 0u);

    // Start monitoring
    EXPECT_TRUE(mental_health_guardian_start(guardian));

    // Wait for a few checks to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mental_health_guardian_status_t status_after;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status_after));

    // Should have performed some checks
    EXPECT_GT(status_after.checks_performed, 0u);

    mental_health_guardian_stop(guardian);
}

TEST_F(MentalHealthGuardianTest, ForceCheck) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status_before;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status_before));
    uint64_t checks_before = status_before.checks_performed;

    // Force a check without starting the monitoring thread
    guardian_intervention_level_t level = mental_health_guardian_force_check(guardian);

    // Should be a valid level
    EXPECT_GE(level, GUARDIAN_LEVEL_OBSERVE);
    EXPECT_LE(level, GUARDIAN_LEVEL_QUARANTINE);

    // Check count should have increased
    mental_health_guardian_status_t status_after;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status_after));
    EXPECT_EQ(status_after.checks_performed, checks_before + 1);
}

TEST_F(MentalHealthGuardianTest, ResetStats) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 20;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    // Perform some checks
    mental_health_guardian_force_check(guardian);
    mental_health_guardian_force_check(guardian);

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.checks_performed, 2u);

    // Reset stats
    EXPECT_TRUE(mental_health_guardian_reset_stats(guardian));

    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.checks_performed, 0u);
}

// =============================================================================
// INTERVENTION LEVEL TESTS
// =============================================================================

TEST_F(MentalHealthGuardianTest, SetLevel) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status;

    // Set to ADJUST
    EXPECT_TRUE(mental_health_guardian_set_level(guardian, GUARDIAN_LEVEL_ADJUST));
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.level, GUARDIAN_LEVEL_ADJUST);

    // Set to QUARANTINE
    EXPECT_TRUE(mental_health_guardian_set_level(guardian, GUARDIAN_LEVEL_QUARANTINE));
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.level, GUARDIAN_LEVEL_QUARANTINE);
}

TEST_F(MentalHealthGuardianTest, LevelToString) {
    EXPECT_STREQ(guardian_level_to_string(GUARDIAN_LEVEL_OBSERVE), "OBSERVE");
    EXPECT_STREQ(guardian_level_to_string(GUARDIAN_LEVEL_ADJUST), "ADJUST");
    EXPECT_STREQ(guardian_level_to_string(GUARDIAN_LEVEL_REGULATE), "REGULATE");
    EXPECT_STREQ(guardian_level_to_string(GUARDIAN_LEVEL_QUARANTINE), "QUARANTINE");
}

TEST_F(MentalHealthGuardianTest, StateToString) {
    EXPECT_STREQ(guardian_state_to_string(GUARDIAN_STATE_STOPPED), "STOPPED");
    EXPECT_STREQ(guardian_state_to_string(GUARDIAN_STATE_RUNNING), "RUNNING");
    EXPECT_STREQ(guardian_state_to_string(GUARDIAN_STATE_PAUSED), "PAUSED");
    EXPECT_STREQ(guardian_state_to_string(GUARDIAN_STATE_ERROR), "ERROR");
}

// =============================================================================
// CONFIGURATION UPDATE TESTS
// =============================================================================

TEST_F(MentalHealthGuardianTest, UpdateConfig) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_config_t new_config = mental_health_guardian_default_config();
    new_config.monitoring_interval_ms = 200;
    new_config.auto_intervene = false;
    new_config.quarantine_threshold = 0.9f;

    EXPECT_TRUE(mental_health_guardian_update_config(guardian, &new_config));

    mental_health_guardian_config_t retrieved;
    EXPECT_TRUE(mental_health_guardian_get_config(guardian, &retrieved));
    EXPECT_EQ(retrieved.monitoring_interval_ms, 200u);
    EXPECT_FALSE(retrieved.auto_intervene);
    EXPECT_FLOAT_EQ(retrieved.quarantine_threshold, 0.9f);
}

TEST_F(MentalHealthGuardianTest, UpdateConfigWhileRunning) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 50;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Update config while running
    config.monitoring_interval_ms = 100;
    EXPECT_TRUE(mental_health_guardian_update_config(guardian, &config));

    mental_health_guardian_config_t retrieved;
    EXPECT_TRUE(mental_health_guardian_get_config(guardian, &retrieved));
    EXPECT_EQ(retrieved.monitoring_interval_ms, 100u);

    mental_health_guardian_stop(guardian);
}

// =============================================================================
// ERROR HANDLING TESTS
// =============================================================================

TEST_F(MentalHealthGuardianTest, NullPointerHandling) {
    // All functions should handle NULL gracefully
    EXPECT_FALSE(mental_health_guardian_start(nullptr));
    EXPECT_FALSE(mental_health_guardian_stop(nullptr));
    EXPECT_FALSE(mental_health_guardian_pause(nullptr));
    EXPECT_FALSE(mental_health_guardian_resume(nullptr));
    EXPECT_FALSE(mental_health_guardian_reset_stats(nullptr));
    EXPECT_EQ(mental_health_guardian_force_check(nullptr), GUARDIAN_LEVEL_OBSERVE);

    mental_health_guardian_status_t status;
    EXPECT_FALSE(mental_health_guardian_get_status(nullptr, &status));

    mental_health_guardian_config_t config;
    EXPECT_FALSE(mental_health_guardian_get_config(nullptr, &config));
    EXPECT_FALSE(mental_health_guardian_update_config(nullptr, &config));
}

TEST_F(MentalHealthGuardianTest, NullOutputHandling) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    EXPECT_FALSE(mental_health_guardian_get_status(guardian, nullptr));
    EXPECT_FALSE(mental_health_guardian_get_config(guardian, nullptr));
    EXPECT_FALSE(mental_health_guardian_update_config(guardian, nullptr));
}

// =============================================================================
// UPTIME TRACKING TEST
// =============================================================================

TEST_F(MentalHealthGuardianTest, UptimeTracking) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status;

    // Before start, uptime should be 0
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.uptime_ms, 0u);

    // Start and wait
    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Uptime should be > 0
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_GT(status.uptime_ms, 0u);
    EXPECT_GE(status.uptime_ms, 50u);  // At least 50ms

    mental_health_guardian_stop(guardian);
}
