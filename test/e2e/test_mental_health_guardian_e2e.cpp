/**
 * @file test_mental_health_guardian_e2e.cpp
 * @brief End-to-end tests for Mental Health Guardian
 *
 * WHAT: Full lifecycle tests for guardian in complete brain context
 * WHY:  Verify guardian works correctly in production-like scenarios
 * HOW:  Test full brain lifecycle with guardian monitoring
 *
 * E2E SCENARIOS:
 * - Full brain lifecycle with guardian
 * - Extended monitoring session
 * - Guardian resilience under load
 * - Multi-cycle monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/mental_health/nimcp_mental_health_guardian.h"

// =============================================================================
// E2E TEST FIXTURE
// =============================================================================

class GuardianE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing - each test creates its own brain
    }

    void TearDown() override {
        // Nothing - each test cleans up its own brain
    }
};

// =============================================================================
// FULL LIFECYCLE E2E TESTS
// =============================================================================

TEST_F(GuardianE2ETest, FullBrainLifecycleWithGuardian) {
    // Create brain
    brain_t brain = brain_create("e2e_guardian_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem (TINY brains have it disabled)
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));
    ASSERT_NE(brain->mental_health_monitor, nullptr);

    // Create and configure guardian
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 25;
    config.auto_intervene = true;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Start guardian monitoring
    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));

    // Simulate brain activity (make some decisions)
    for (int i = 0; i < 10; i++) {
        float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        brain_decision_t* decision = brain_decide(brain, input, 10);
        (void)decision;  // Suppress unused warning

        // Small delay between decisions
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Let guardian run for a bit more
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check guardian status
    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
    EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);
    EXPECT_GT(status.checks_performed, 0u);

    // Stop guardian
    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));

    // Destroy brain (should clean up guardian)
    brain_destroy(brain);
}

TEST_F(GuardianE2ETest, ExtendedMonitoringSession) {
    brain_t brain = brain_create("e2e_extended_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 20;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Start monitoring
    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));

    // Run for an extended period with periodic status checks
    for (int cycle = 0; cycle < 5; cycle++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        mental_health_guardian_status_t status;
        EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
        EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);

        // Checks should keep increasing
        EXPECT_GT(status.checks_performed, (uint64_t)cycle);
    }

    // Stop and verify final state
    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));

    mental_health_guardian_status_t final_status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &final_status));
    EXPECT_EQ(final_status.state, GUARDIAN_STATE_STOPPED);
    EXPECT_GT(final_status.checks_performed, 5u);

    brain_destroy(brain);
}

TEST_F(GuardianE2ETest, GuardianResilienceUnderLoad) {
    brain_t brain = brain_create("e2e_resilience_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 10;  // Fast monitoring

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));

    // Stress test: rapid decisions while guardian monitors
    for (int burst = 0; burst < 3; burst++) {
        for (int i = 0; i < 20; i++) {
            float input[10] = {0};
            input[i % 10] = 1.0f;

            brain_decision_t* decision = brain_decide(brain, input, 10);
            (void)decision;
        }

        // Brief pause between bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Guardian should still be running
        mental_health_guardian_status_t status;
        EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
        EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);
    }

    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));
    brain_destroy(brain);
}

TEST_F(GuardianE2ETest, MultiCycleMonitoring) {
    brain_t brain = brain_create("e2e_multicycle_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 15;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Multiple start/stop cycles
    for (int cycle = 0; cycle < 3; cycle++) {
        EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));

        // Run for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Do some brain activity
        float input[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        brain_decision_t* decision = brain_decide(brain, input, 10);
        (void)decision;

        // Stop
        EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));

        mental_health_guardian_status_t status;
        EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
        EXPECT_EQ(status.state, GUARDIAN_STATE_STOPPED);
    }

    brain_destroy(brain);
}

TEST_F(GuardianE2ETest, PauseResumeDuringActivity) {
    brain_t brain = brain_create("e2e_pause_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 20;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    mental_health_guardian_status_t status_before_pause;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status_before_pause));
    uint64_t checks_before_pause = status_before_pause.checks_performed;

    // Pause
    EXPECT_TRUE(mental_health_guardian_pause(brain->mental_health_guardian));

    mental_health_guardian_status_t pause_status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &pause_status));
    EXPECT_EQ(pause_status.state, GUARDIAN_STATE_PAUSED);

    // Do brain activity while paused
    for (int i = 0; i < 5; i++) {
        float input[10] = {0.1f * i};
        brain_decision_t* decision = brain_decide(brain, input, 10);
        (void)decision;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check count shouldn't have increased much while paused
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &pause_status));
    EXPECT_LE(pause_status.checks_performed, checks_before_pause + 1);

    // Resume
    EXPECT_TRUE(mental_health_guardian_resume(brain->mental_health_guardian));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now checks should have increased
    mental_health_guardian_status_t status_after_resume;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status_after_resume));
    EXPECT_EQ(status_after_resume.state, GUARDIAN_STATE_RUNNING);
    EXPECT_GT(status_after_resume.checks_performed, checks_before_pause);

    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));
    brain_destroy(brain);
}

TEST_F(GuardianE2ETest, ConfigUpdateDuringOperation) {
    brain_t brain = brain_create("e2e_config_update_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 50;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mental_health_guardian_status_t status_slow;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status_slow));
    uint64_t checks_at_slow = status_slow.checks_performed;

    // Update to faster interval
    config.monitoring_interval_ms = 10;
    EXPECT_TRUE(mental_health_guardian_update_config(brain->mental_health_guardian, &config));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mental_health_guardian_status_t status_fast;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status_fast));

    // Should have more checks with faster interval
    uint64_t checks_after_speedup = status_fast.checks_performed - checks_at_slow;
    EXPECT_GT(checks_after_speedup, 5u);  // Should be ~10 at 10ms interval

    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));
    brain_destroy(brain);
}

TEST_F(GuardianE2ETest, ForceCheckIntegration) {
    brain_t brain = brain_create("e2e_force_check_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));

    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 1000;  // Long interval

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Don't start the thread - just use force_check

    // Do some brain activity
    for (int i = 0; i < 5; i++) {
        float input[10] = {0.1f * i};
        brain_decision_t* decision = brain_decide(brain, input, 10);
        (void)decision;

        // Force immediate check
        guardian_intervention_level_t level =
            mental_health_guardian_force_check(brain->mental_health_guardian);

        EXPECT_GE(level, GUARDIAN_LEVEL_OBSERVE);
        EXPECT_LE(level, GUARDIAN_LEVEL_QUARANTINE);
    }

    mental_health_guardian_status_t status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
    EXPECT_EQ(status.checks_performed, 5u);

    brain_destroy(brain);
}

// =============================================================================
// COMPLETE SYSTEM E2E TEST
// =============================================================================

TEST_F(GuardianE2ETest, FullSystemEndToEnd) {
    // Create and fully configure brain
    brain_t brain = brain_create("e2e_full_system", BRAIN_SIZE_SMALL,
                                 BRAIN_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);

    // Enable and initialize mental health subsystem
    brain->config.enable_mental_health_monitoring = true;
    ASSERT_TRUE(nimcp_brain_factory_init_mental_health_subsystem(brain));
    ASSERT_NE(brain->mental_health_monitor, nullptr);

    // Configure guardian
    mental_health_guardian_config_t config = mental_health_guardian_default_config();
    config.monitoring_interval_ms = 25;
    config.auto_intervene = true;
    config.verbose_logging = false;

    if (brain->mental_health_guardian) {
        mental_health_guardian_destroy(brain->mental_health_guardian);
    }
    brain->mental_health_guardian = mental_health_guardian_create(brain, &config);
    ASSERT_NE(brain->mental_health_guardian, nullptr);

    // Start guardian
    EXPECT_TRUE(mental_health_guardian_start(brain->mental_health_guardian));

    // Simulate realistic brain usage pattern
    for (int epoch = 0; epoch < 3; epoch++) {
        // Training-like activity
        for (int batch = 0; batch < 5; batch++) {
            float input[20];
            for (int j = 0; j < 20; j++) {
                input[j] = (float)(batch * 20 + j) / 100.0f;
            }

            brain_decision_t* decision = brain_decide(brain, input, 20);
            (void)decision;
        }

        // Periodic status check
        mental_health_guardian_status_t status;
        EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &status));
        EXPECT_EQ(status.state, GUARDIAN_STATE_RUNNING);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // Final status check
    mental_health_guardian_status_t final_status;
    EXPECT_TRUE(mental_health_guardian_get_status(brain->mental_health_guardian, &final_status));

    // Report results
    printf("\n=== E2E Full System Test Results ===\n");
    printf("State: %s\n", guardian_state_to_string(final_status.state));
    printf("Level: %s\n", guardian_level_to_string(final_status.level));
    printf("Checks performed: %lu\n", final_status.checks_performed);
    printf("Interventions applied: %lu\n", final_status.interventions_applied);
    printf("Overall severity: %.3f\n", final_status.overall_severity);
    printf("Uptime: %lu ms\n", final_status.uptime_ms);
    printf("====================================\n\n");

    // Cleanup
    EXPECT_TRUE(mental_health_guardian_stop(brain->mental_health_guardian));
    brain_destroy(brain);
}
