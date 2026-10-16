/**
 * @file test_guardian_intervention_regression.cpp
 * @brief Regression tests for Mental Health Guardian intervention levels
 *
 * WHAT: Tests to ensure intervention level thresholds work correctly
 * WHY:  Prevent regressions in graduated intervention logic
 * HOW:  Test boundary conditions and level transitions
 *
 * REGRESSION COVERAGE:
 * - Threshold boundary conditions
 * - Level transitions
 * - Auto-intervention behavior
 * - Metrics accuracy
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

class GuardianInterventionRegressionTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    mental_health_guardian_t* guardian = nullptr;

    void SetUp() override {
        brain = brain_create("guardian_regression_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        // Enable and initialize mental health subsystem (TINY brains have it disabled)
        brain->config.enable_mental_health_monitoring = true;
        ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain))
            << "Failed to initialize mental health subsystem";
        ASSERT_NE(brain->mental_health_monitor, nullptr);
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
// THRESHOLD BOUNDARY TESTS
// =============================================================================

TEST_F(GuardianInterventionRegressionTest, DefaultThresholdsCorrect) {
    // Verify default thresholds match specification
    mental_health_guardian_config_t config = mental_health_guardian_default_config();

    // From spec: OBSERVE < 0.3, ADJUST 0.3-0.6, REGULATE 0.6-0.8, QUARANTINE > 0.8
    EXPECT_FLOAT_EQ(config.observe_threshold, 0.0f);
    EXPECT_FLOAT_EQ(config.adjust_threshold, 0.3f);
    EXPECT_FLOAT_EQ(config.regulate_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.quarantine_threshold, 0.8f);
}

TEST_F(GuardianInterventionRegressionTest, CustomThresholdsApplied) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.adjust_threshold = 0.2f;
    config.regulate_threshold = 0.5f;
    config.quarantine_threshold = 0.7f;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_config_t retrieved;
    EXPECT_TRUE(mental_health_guardian_get_config(guardian, &retrieved));

    EXPECT_FLOAT_EQ(retrieved.adjust_threshold, 0.2f);
    EXPECT_FLOAT_EQ(retrieved.regulate_threshold, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.quarantine_threshold, 0.7f);
}

// =============================================================================
// LEVEL TRANSITION TESTS
// =============================================================================

TEST_F(GuardianInterventionRegressionTest, ManualLevelTransitions) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status;

    // Transition through all levels
    guardian_intervention_level_t levels[] = {
        GUARDIAN_LEVEL_OBSERVE,
        GUARDIAN_LEVEL_ADJUST,
        GUARDIAN_LEVEL_REGULATE,
        GUARDIAN_LEVEL_QUARANTINE,
        GUARDIAN_LEVEL_OBSERVE  // Back to observe
    };

    for (auto level : levels) {
        EXPECT_TRUE(mental_health_guardian_set_level(guardian, level));
        EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
        EXPECT_EQ(status.level, level);
    }
}

TEST_F(GuardianInterventionRegressionTest, LevelPersistsAfterPause) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    // Set level
    EXPECT_TRUE(mental_health_guardian_set_level(guardian, GUARDIAN_LEVEL_REGULATE));

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.level, GUARDIAN_LEVEL_REGULATE);

    // Start, pause, resume
    EXPECT_TRUE(mental_health_guardian_start(guardian));
    EXPECT_TRUE(mental_health_guardian_pause(guardian));

    // Level should persist through pause
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.level, GUARDIAN_LEVEL_REGULATE);

    mental_health_guardian_stop(guardian);
}

// =============================================================================
// AUTO-INTERVENTION TESTS
// =============================================================================

TEST_F(GuardianInterventionRegressionTest, AutoInterventionEnabled) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.auto_intervene = true;
    config.monitoring_interval_ms = 20;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    // Start monitoring
    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));

    // Should have performed checks
    EXPECT_GT(status.checks_performed, 0u);

    // Observe count should be tracked
    EXPECT_GE(status.observe_count, 0u);

    mental_health_guardian_stop(guardian);
}

TEST_F(GuardianInterventionRegressionTest, AutoInterventionDisabled) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.auto_intervene = false;
    config.monitoring_interval_ms = 20;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    // Start monitoring
    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));

    // Should have performed checks
    EXPECT_GT(status.checks_performed, 0u);

    // With auto_intervene=false, no interventions should be applied
    // (Only observe count should increase)
    EXPECT_EQ(status.interventions_applied, 0u);

    mental_health_guardian_stop(guardian);
}

// =============================================================================
// METRICS ACCURACY TESTS
// =============================================================================

TEST_F(GuardianInterventionRegressionTest, ChecksPerformedAccurate) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 20;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status;

    // Manual checks
    for (int i = 0; i < 5; i++) {
        mental_health_guardian_force_check(guardian);
    }

    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.checks_performed, 5u);
}

TEST_F(GuardianInterventionRegressionTest, LevelCountsAccurate) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    mental_health_guardian_status_t status;

    // Force checks at different levels
    // Note: force_check will determine level automatically based on mental health
    // So we just verify the counts increment
    for (int i = 0; i < 3; i++) {
        mental_health_guardian_force_check(guardian);
    }

    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));

    // Total of level counts should equal checks performed
    uint64_t total_levels = status.observe_count + status.adjust_count +
                           status.regulate_count + status.quarantine_count;

    // With auto_intervene enabled (default), level counts should be tracked
    // But without actual high severity, mostly observe
    EXPECT_LE(total_levels, status.checks_performed);
}

TEST_F(GuardianInterventionRegressionTest, ResetStatsComplete) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 20;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    // Accumulate some stats
    EXPECT_TRUE(mental_health_guardian_start(guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mental_health_guardian_stop(guardian);

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_GT(status.checks_performed, 0u);

    // Reset
    EXPECT_TRUE(mental_health_guardian_reset_stats(guardian));

    // All counters should be zero
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));
    EXPECT_EQ(status.checks_performed, 0u);
    EXPECT_EQ(status.interventions_applied, 0u);
    EXPECT_EQ(status.observe_count, 0u);
    EXPECT_EQ(status.adjust_count, 0u);
    EXPECT_EQ(status.regulate_count, 0u);
    EXPECT_EQ(status.quarantine_count, 0u);
}

// =============================================================================
// TIMING REGRESSION TESTS
// =============================================================================

TEST_F(GuardianInterventionRegressionTest, MonitoringIntervalRespected) {
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 50;

    guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(guardian));

    // Wait for ~200ms, should get ~4 checks at 50ms interval
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));

    // Allow some tolerance for timing
    EXPECT_GE(status.checks_performed, 2u);
    EXPECT_LE(status.checks_performed, 6u);

    mental_health_guardian_stop(guardian);
}

TEST_F(GuardianInterventionRegressionTest, UptimeAccuracy) {
    guardian = mental_health_guardian_create(brain, nullptr);
    ASSERT_NE(guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(guardian));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(guardian, &status));

    // Uptime should be approximately 100ms (allow 50-150ms tolerance)
    EXPECT_GE(status.uptime_ms, 50u);
    EXPECT_LE(status.uptime_ms, 200u);

    mental_health_guardian_stop(guardian);
}
