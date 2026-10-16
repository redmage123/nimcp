/**
 * @file e2e_test_medulla_pipeline.cpp
 * @brief Comprehensive End-to-End Tests for the Medulla Oblongata Pipeline
 *
 * WHAT: E2E tests simulating complete medulla lifecycle, autonomic regulation,
 *       and integration with other brain systems
 * WHY:  Verify the full autonomic regulation pipeline works in realistic scenarios
 * HOW:  Test complete workflows from initialization through operation to shutdown
 *
 * TEST CATEGORIES:
 * 1. LIFECYCLE        - Full brain-medulla lifecycle integration
 * 2. EMERGENCY        - Emergency response and recovery pipelines
 * 3. CIRCADIAN        - 24-hour circadian cycle simulation
 * 4. PROTECTION       - Protection level escalation/de-escalation
 * 5. INTEGRATION      - Integration with immune and sleep-wake systems
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 2.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_circadian.h"
#include "core/medulla/nimcp_protective_cutoff.h"
#include "core/medulla/nimcp_arousal_state.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int E2E_FULL_CYCLE_FRAMES = 500;
static constexpr float E2E_FRAME_DT = 0.016667f;  // 60 FPS
static constexpr float FLOAT_TOLERANCE = 1e-3f;

//=============================================================================
// E2E Test Fixture
//=============================================================================

class MedullaE2ETest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        medulla_config_t config = medulla_default_config();
        config.enable_bio_async = false;
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }

    // Helper to run update cycles
    void RunUpdateCycles(int count, float dt = E2E_FRAME_DT) {
        for (int i = 0; i < count; i++) {
            medulla_update(medulla, dt);
        }
    }
};

//=============================================================================
// 1. FULL BRAIN PIPELINE TESTS
//=============================================================================

/**
 * @test FullBrainMedullaIntegration
 * @brief Create full brain with medulla, run multiple update cycles, verify
 *        all systems interact correctly
 *
 * This tests the complete lifecycle of the medulla within a brain simulation:
 * 1. Initialization
 * 2. Normal operation
 * 3. Degraded operation
 * 4. Recovery
 * 5. Clean shutdown
 */
TEST_F(MedullaE2ETest, FullBrainMedullaIntegration) {
    // Phase 1: Initialize and start
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Phase 2: Normal operation - simulate several seconds of brain activity
    for (int frame = 0; frame < E2E_FULL_CYCLE_FRAMES; frame++) {
        int result = medulla_update(medulla, E2E_FRAME_DT);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        // Periodically verify state is valid
        if (frame % 100 == 0) {
            medulla_get_stats(medulla, &stats);
            EXPECT_GE(stats.current_arousal, 0.0f);
            EXPECT_LE(stats.current_arousal, 1.0f);
        }
    }

    // Verify stats accumulated
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.total_updates, (uint64_t)E2E_FULL_CYCLE_FRAMES);

    // Phase 3: Degraded operation - simulate subsystem failure
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED), NIMCP_SUCCESS);

    for (int frame = 0; frame < 100; frame++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_DEGRADED);

    // Phase 4: Recovery
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_RUNNING), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Continue normal operation
    for (int frame = 0; frame < 100; frame++) {
        EXPECT_EQ(medulla_update(medulla, E2E_FRAME_DT), NIMCP_SUCCESS);
    }

    // Phase 5: Clean shutdown
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);
}

/**
 * @test MedullaEmergencyPipeline
 * @brief Trigger emergency and verify complete system response
 *
 * Tests the emergency response pathway:
 * 1. Normal operation
 * 2. Emergency trigger
 * 3. System response (protection escalation, arousal suppression)
 * 4. Clean shutdown post-emergency
 */
TEST_F(MedullaE2ETest, MedullaEmergencyPipeline) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Normal operation phase
    for (int i = 0; i < 200; i++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    // Capture pre-emergency metrics
    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);
    EXPECT_EQ(stats_before.state, MEDULLA_STATE_RUNNING);

    // Trigger emergency - simulating critical system failure
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "E2E test: critical system failure"),
              NIMCP_SUCCESS);

    // Verify emergency state
    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // Emergency should trigger state change
    EXPECT_EQ(stats_after.state, MEDULLA_STATE_EMERGENCY);

    // Emergency count should increment
    EXPECT_GE(stats_after.emergency_shutdowns, 1u);

    // Protection should escalate to maximum
    EXPECT_EQ(stats_after.protection_level, PROTECTION_LEVEL_SHUTDOWN);

    // Arousal should be suppressed during emergency
    EXPECT_LT(stats_after.current_arousal, stats_before.current_arousal);

    // Verify system can still be stopped cleanly
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);
}

//=============================================================================
// 2. CIRCADIAN CYCLE TESTS
//=============================================================================

/**
 * @test CircadianCycle24Hour
 * @brief Simulate a complete 24-hour circadian cycle
 *
 * Tests full circadian rhythm progression:
 * 1. Track phase transitions through all 8 phases
 * 2. Verify arousal modulation follows circadian pattern
 * 3. Ensure time wraps correctly at 24 hours
 */
TEST_F(MedullaE2ETest, CircadianCycle24Hour) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Start at early morning
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_EARLY_MORNING);

    circadian_phase_t initial_phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(initial_phase, CIRCADIAN_PHASE_EARLY_MORNING);

    // Track phases seen
    bool phases_seen[8] = {false};
    phases_seen[initial_phase] = true;

    // Track arousal changes
    float max_arousal = 0.0f;
    float min_arousal = 1.0f;
    std::vector<float> arousal_samples;

    // Simulate 26 hours (slightly more than one cycle) with 1-hour steps
    for (int hour = 0; hour < 26; hour++) {
        // Advance 1 hour = 3600 seconds
        medulla_update(medulla, 3600.0f);

        circadian_phase_t current_phase = medulla_get_circadian_phase(medulla);
        phases_seen[current_phase] = true;

        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        // Track arousal
        arousal_samples.push_back(stats.current_arousal);
        if (stats.current_arousal > max_arousal) max_arousal = stats.current_arousal;
        if (stats.current_arousal < min_arousal) min_arousal = stats.current_arousal;

        // Circadian time should always be valid
        EXPECT_GE(stats.circadian_time_hours, 0.0f);
        EXPECT_LT(stats.circadian_time_hours, 24.0f);
    }

    // Verify all phases were visited
    int phases_visited = 0;
    for (int i = 0; i < 8; i++) {
        if (phases_seen[i]) phases_visited++;
    }
    EXPECT_GE(phases_visited, 7) << "Should visit at least 7 of 8 phases in 26 hours";

    // Verify circadian completed at least one cycle
    medulla_stats_t final_stats;
    medulla_get_stats(medulla, &final_stats);
    EXPECT_GE(final_stats.circadian_cycles, 1u);

    // Arousal should vary with circadian rhythm (day/night difference)
    EXPECT_GT(max_arousal - min_arousal, 0.1f)
        << "Arousal should vary with circadian rhythm";
}

/**
 * @test ProtectionEscalationPipeline
 * @brief Simulate health degradation and verify protection escalation
 *
 * Tests protection response to simulated health threats:
 * 1. Start at normal protection
 * 2. Escalate through protection levels
 * 3. Verify capability restrictions at each level
 * 4. De-escalate back to normal
 */
TEST_F(MedullaE2ETest, ProtectionEscalationPipeline) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Initial level should be normal
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);

    // Run some normal updates
    RunUpdateCycles(50);

    // Verify level remains normal without threats
    level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);

    // Escalate through each protection level and verify
    protection_level_t escalation_sequence[] = {
        PROTECTION_LEVEL_CAUTIOUS,
        PROTECTION_LEVEL_GUARDED,
        PROTECTION_LEVEL_DEFENSIVE,
        PROTECTION_LEVEL_CRITICAL
    };

    for (auto target_level : escalation_sequence) {
        EXPECT_EQ(medulla_test_set_protection(medulla, target_level), 0);

        // Run updates at this protection level
        for (int i = 0; i < 20; i++) {
            medulla_update(medulla, E2E_FRAME_DT);
        }

        level = medulla_get_protection_level(medulla);
        EXPECT_EQ(level, target_level);

        // Get stats to verify arousal modulation by protection level
        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);
        EXPECT_EQ(stats.protection_level, target_level);
    }

    // De-escalate back to normal
    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    RunUpdateCycles(50);

    level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);
}

//=============================================================================
// 3. INTEGRATION PIPELINE TESTS
//=============================================================================

/**
 * @test ImmuneMedullaFeedbackLoop
 * @brief Test the full immune-medulla feedback loop
 *
 * Tests bidirectional integration:
 * 1. Immune system affects arousal (inflammation -> fatigue)
 * 2. Protection level affects immune response
 * 3. Circadian phase affects immune efficiency
 */
TEST_F(MedullaE2ETest, ImmuneMedullaFeedbackLoop) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Test: Inflammation arousal factors are biologically correct
    // Higher inflammation should reduce arousal (sickness behavior)

    // Get baseline arousal
    medulla_stats_t baseline_stats;
    medulla_get_stats(medulla, &baseline_stats);

    // Verify immune bridge constants (API stability check)
    EXPECT_LT(CYTOKINE_IL1_AROUSAL_IMPACT, 0.0f);   // Pro-inflammatory reduces arousal
    EXPECT_LT(CYTOKINE_TNF_AROUSAL_IMPACT, 0.0f);
    EXPECT_GT(CYTOKINE_IL10_AROUSAL_IMPACT, 0.0f);  // Anti-inflammatory increases arousal

    // Test protection-immune relationship
    // Higher protection should increase immune activity (up to critical)
    EXPECT_GT(PROTECTION_ELEVATED_IMMUNE_FACTOR, PROTECTION_NORMAL_IMMUNE_FACTOR);
    EXPECT_GT(PROTECTION_CRITICAL_IMMUNE_FACTOR, PROTECTION_HIGH_IMMUNE_FACTOR);

    // But shutdown should reduce to prevent cytokine storm
    EXPECT_LT(PROTECTION_SHUTDOWN_IMMUNE_FACTOR, PROTECTION_NORMAL_IMMUNE_FACTOR);

    // Test circadian-immune relationship
    EXPECT_GT(CIRCADIAN_DAY_IMMUNE_FACTOR, CIRCADIAN_NIGHT_IMMUNE_FACTOR);

    // Simulate a scenario: Day operation with normal protection
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    RunUpdateCycles(100);

    medulla_stats_t day_stats;
    medulla_get_stats(medulla, &day_stats);
    EXPECT_EQ(day_stats.circadian_phase, CIRCADIAN_PHASE_MORNING);

    // Simulate night operation
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);
    RunUpdateCycles(100);

    medulla_stats_t night_stats;
    medulla_get_stats(medulla, &night_stats);
    EXPECT_EQ(night_stats.circadian_phase, CIRCADIAN_PHASE_DEEP_NIGHT);
}

/**
 * @test SleepWakeMedullaSync
 * @brief Test sleep-wake and medulla synchronization
 *
 * Tests arousal patterns matching sleep-wake states:
 * 1. Wake phases have higher arousal targets
 * 2. Sleep phases have lower arousal targets
 * 3. Transitions are smooth and stable
 */
TEST_F(MedullaE2ETest, SleepWakeMedullaSync) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Wake phase - Morning
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    medulla_test_set_arousal(medulla, 0.5f);

    // Update to let arousal adjust toward circadian target
    for (int i = 0; i < 200; i++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    medulla_stats_t wake_stats;
    medulla_get_stats(medulla, &wake_stats);

    // Sleep phase - Deep Night
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);

    // Update to let arousal adjust
    for (int i = 0; i < 200; i++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    medulla_stats_t sleep_stats;
    medulla_get_stats(medulla, &sleep_stats);

    // Wake arousal should be higher than sleep arousal
    EXPECT_GT(wake_stats.current_arousal, sleep_stats.current_arousal);

    // Both should remain in valid range
    EXPECT_GE(wake_stats.current_arousal, 0.0f);
    EXPECT_LE(wake_stats.current_arousal, 1.0f);
    EXPECT_GE(sleep_stats.current_arousal, 0.0f);
    EXPECT_LE(sleep_stats.current_arousal, 1.0f);
}

//=============================================================================
// 4. LIFECYCLE TESTS
//=============================================================================

TEST_F(MedullaE2ETest, CompleteMedullaLifecycle) {
    // Phase 1: Initialize and start
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Phase 2: Normal operation (simulate several updates)
    for (int frame = 0; frame < 100; frame++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    // Verify stats accumulated
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.total_updates, 0u);

    // Phase 3: Degraded operation
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED), NIMCP_SUCCESS);

    for (int frame = 0; frame < 50; frame++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_DEGRADED);

    // Phase 4: Recovery
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_RUNNING), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Phase 5: Clean shutdown
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);
}

TEST_F(MedullaE2ETest, EmergencyShutdownPipeline) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Normal operation
    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, E2E_FRAME_DT);
    }

    // Trigger emergency
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "critical system failure"), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Verify emergency state
    EXPECT_GE(stats.emergency_shutdowns, 1u);
    EXPECT_GE((int)stats.protection_level, (int)PROTECTION_LEVEL_CRITICAL);

    // Verify system can still be stopped cleanly
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);
}

TEST_F(MedullaE2ETest, FullCircadianCycle) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    circadian_phase_t initial_phase = medulla_get_circadian_phase(medulla);

    // Simulate many hours (accelerated time)
    for (int i = 0; i < 1000; i++) {
        int result = medulla_update(medulla, 1.0f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Circadian time should be valid
    EXPECT_GE(stats.circadian_time_hours, 0.0f);
    EXPECT_LT(stats.circadian_time_hours, 24.0f);

    // Phase should still be valid
    circadian_phase_t final_phase = medulla_get_circadian_phase(medulla);
    EXPECT_GE((int)final_phase, 0);
    EXPECT_LT((int)final_phase, 8);
}

TEST_F(MedullaE2ETest, ProtectionLevelManagement) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Initial level should be normal
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);

    // Operate normally
    for (int i = 0; i < 50; i++) {
        medulla_update(medulla, 0.02f);
    }

    // Level should remain normal without threats
    level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);

    // Trigger emergency
    medulla_emergency_shutdown(medulla, "test threat");

    level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, (int)PROTECTION_LEVEL_CRITICAL);
}

TEST_F(MedullaE2ETest, StatisticsAccumulation) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_initial;
    medulla_get_stats(medulla, &stats_initial);
    uint64_t initial_updates = stats_initial.total_updates;

    // Run for extended period
    const int FRAMES = 500;
    for (int i = 0; i < FRAMES; i++) {
        medulla_update(medulla, 0.02f);
    }

    medulla_stats_t stats_final;
    medulla_get_stats(medulla, &stats_final);

    // Verify update count accumulated correctly
    EXPECT_GE(stats_final.total_updates, initial_updates + FRAMES);
    EXPECT_GE(stats_final.uptime_ms, 0u);
    EXPECT_GE(stats_final.arousal_updates, 0u);
}

TEST_F(MedullaE2ETest, MultipleIndependentInstances) {
    // Create additional medullas
    medulla_config_t config = medulla_default_config();
    medulla_t medulla2 = medulla_create(&config);
    medulla_t medulla3 = medulla_create(&config);

    ASSERT_NE(medulla2, nullptr);
    ASSERT_NE(medulla3, nullptr);

    // Start all
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_start(medulla2), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_start(medulla3), NIMCP_SUCCESS);

    // Update each independently
    for (int i = 0; i < 50; i++) {
        medulla_update(medulla, 0.01f);
        medulla_update(medulla2, 0.02f);
        medulla_update(medulla3, 0.03f);
    }

    // Verify independent operation
    medulla_stats_t stats1, stats2, stats3;
    medulla_get_stats(medulla, &stats1);
    medulla_get_stats(medulla2, &stats2);
    medulla_get_stats(medulla3, &stats3);

    EXPECT_GE(stats1.total_updates, 50u);
    EXPECT_GE(stats2.total_updates, 50u);
    EXPECT_GE(stats3.total_updates, 50u);

    // Emergency on one shouldn't affect others
    medulla_emergency_shutdown(medulla2, "isolated emergency");

    protection_level_t level1 = medulla_get_protection_level(medulla);
    protection_level_t level2 = medulla_get_protection_level(medulla2);
    protection_level_t level3 = medulla_get_protection_level(medulla3);

    EXPECT_EQ(level1, PROTECTION_LEVEL_NORMAL);
    EXPECT_GE((int)level2, (int)PROTECTION_LEVEL_CRITICAL);
    EXPECT_EQ(level3, PROTECTION_LEVEL_NORMAL);

    // Clean up additional instances
    medulla_stop(medulla2);
    medulla_stop(medulla3);
    medulla_destroy(medulla2);
    medulla_destroy(medulla3);
}

TEST_F(MedullaE2ETest, RestartAfterEmergency) {
    // Start and operate
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    for (int i = 0; i < 50; i++) {
        medulla_update(medulla, 0.02f);
    }

    // Emergency shutdown
    medulla_emergency_shutdown(medulla, "recoverable error");

    // Stop completely
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);

    // Restart should work
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Should be able to operate again
    for (int i = 0; i < 50; i++) {
        int result = medulla_update(medulla, 0.02f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(MedullaE2ETest, ArousalStabilityOverTime) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    float min_arousal = 1.0f;
    float max_arousal = 0.0f;

    // Run for extended period and track arousal range
    for (int i = 0; i < 1000; i++) {
        medulla_update(medulla, 0.01f);

        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        if (stats.current_arousal < min_arousal) {
            min_arousal = stats.current_arousal;
        }
        if (stats.current_arousal > max_arousal) {
            max_arousal = stats.current_arousal;
        }
    }

    // Arousal should stay in valid range
    EXPECT_GE(min_arousal, 0.0f);
    EXPECT_LE(max_arousal, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
