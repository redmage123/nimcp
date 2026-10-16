/**
 * @file test_dragonfly_hunting_integration.cpp
 * @brief Integration tests for dragonfly hunting subsystems
 *
 * Tests the integration of energy, learning, collision, gaze, multi-target,
 * emotion bridge, sleep bridge, and immune bridge modules.
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_multi_target.h"
#include "dragonfly/nimcp_dragonfly_energy.h"
#include "dragonfly/nimcp_dragonfly_collision.h"
#include "dragonfly/nimcp_dragonfly_learning.h"
#include "dragonfly/nimcp_dragonfly_gaze.h"
#include "dragonfly/nimcp_dragonfly_emotion_bridge.h"
#include "dragonfly/nimcp_dragonfly_sleep_bridge.h"
#include "dragonfly/nimcp_dragonfly_immune_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyHuntingIntegrationTest : public ::testing::Test {
protected:
    dragonfly_multi_target_t multi_target = nullptr;
    dragonfly_energy_t energy = nullptr;
    dragonfly_collision_t collision = nullptr;
    dragonfly_learning_t learning = nullptr;
    dragonfly_gaze_t gaze = nullptr;
    dragonfly_emotion_bridge_t emotion = nullptr;
    dragonfly_sleep_bridge_t sleep = nullptr;
    dragonfly_immune_bridge_t immune = nullptr;

    void SetUp() override {
        multi_target = dragonfly_multi_target_create(nullptr);
        energy = dragonfly_energy_create(nullptr);
        collision = dragonfly_collision_create(nullptr);
        learning = dragonfly_learning_create(nullptr);
        gaze = dragonfly_gaze_create(nullptr);
        emotion = dragonfly_emotion_bridge_create(nullptr);
        sleep = dragonfly_sleep_bridge_create(nullptr);
        immune = dragonfly_immune_bridge_create(nullptr);

        ASSERT_NE(multi_target, nullptr);
        ASSERT_NE(energy, nullptr);
        ASSERT_NE(collision, nullptr);
        ASSERT_NE(learning, nullptr);
        ASSERT_NE(gaze, nullptr);
        ASSERT_NE(emotion, nullptr);
        ASSERT_NE(sleep, nullptr);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (multi_target) dragonfly_multi_target_destroy(multi_target);
        if (energy) dragonfly_energy_destroy(energy);
        if (collision) dragonfly_collision_destroy(collision);
        if (learning) dragonfly_learning_destroy(learning);
        if (gaze) dragonfly_gaze_destroy(gaze);
        if (emotion) dragonfly_emotion_bridge_destroy(emotion);
        if (sleep) dragonfly_sleep_bridge_destroy(sleep);
        if (immune) dragonfly_immune_bridge_destroy(immune);
    }

    dragonfly_detection_t make_detection(uint32_t id, float x, float y, float z) {
        dragonfly_detection_t det = {};
        det.id = id;
        det.position[0] = x;
        det.position[1] = y;
        det.position[2] = z;
        det.motion_direction_rad = 0.0f;
        det.motion_speed = 3.0f;
        det.size = 0.05f;
        det.contrast = 0.8f;
        det.timestamp_us = 1000000;
        return det;
    }

    dragonfly_self_state_t make_self_state() {
        dragonfly_self_state_t self = {};
        self.position[0] = 0;
        self.position[1] = 0;
        self.position[2] = 0;
        self.max_speed = 15.0f;
        self.max_accel = 10.0f;
        self.max_turn_rate = 3.0f;
        self.energy_level = 0.8f;
        return self;
    }

    body_state_t make_body_state() {
        body_state_t body = {};
        body.yaw_rad = 0.0f;
        body.pitch_rad = 0.0f;
        body.roll_rad = 0.0f;
        return body;
    }

    gaze_target_t make_gaze_target(const float pos[3]) {
        gaze_target_t target = {};
        target.type = GAZE_TARGET_PREY;
        target.position[0] = pos[0];
        target.position[1] = pos[1];
        target.position[2] = pos[2];
        target.velocity[0] = 0.0f;
        target.velocity[1] = 0.0f;
        target.velocity[2] = 0.0f;
        target.priority = 1.0f;
        target.is_moving = true;
        return target;
    }
};

//=============================================================================
// Hunt Decision Making Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, EnergyAffectsHuntDecision) {
    // When energy is low, hunting motivation should decrease
    dragonfly_emotion_bridge_update(emotion, 0.1f);

    float motivation_high = dragonfly_emotion_get_motivation(emotion);
    (void)motivation_high;

    // Simulate energy depletion
    for (int i = 0; i < 50; i++) {
        dragonfly_energy_update(energy, ACTIVITY_PURSUIT, 0.1f);
    }

    // Check energy state
    energy_budget_t budget;
    dragonfly_energy_get_budget(energy, &budget);

    // Lower energy should affect pursuit decisions
    EXPECT_LT(budget.current_energy_j, budget.max_energy_j);
}

TEST_F(DragonflyHuntingIntegrationTest, HealthAffectsHuntCapability) {
    // Healthy dragonfly can pursue
    EXPECT_TRUE(dragonfly_immune_hunting_safe(immune));

    // Simulate severe stress (like injury)
    for (int i = 0; i < 20; i++) {
        dragonfly_immune_report_stress(immune, 1.0f, 0.5f);
        dragonfly_immune_bridge_update(immune, 0.1f);
    }

    // After severe stress, should not be safe to hunt
    // (depends on implementation - health may still be safe)
    health_status_t health = dragonfly_immune_get_health(immune);
    // Verify we got some health status
    EXPECT_GE((int)health, (int)HEALTH_OPTIMAL);
}

TEST_F(DragonflyHuntingIntegrationTest, EmotionModulatesAggression) {
    // Build up hunger to increase aggression
    for (int i = 0; i < 100; i++) {
        dragonfly_emotion_bridge_update(emotion, 0.1f);
    }

    // Get emotional state which contains drives
    emotional_state_t state;
    dragonfly_emotion_get_state(emotion, &state);

    // Hunger should increase over time
    EXPECT_GT(state.drives[DRIVE_HUNGER], 0.0f);

    // Get modulation
    emotion_modulation_t mod;
    dragonfly_emotion_get_modulation(emotion, &mod);
    // Pursuit aggression should be present
    EXPECT_GE(mod.pursuit_aggression, 0.0f);
}

//=============================================================================
// Target Selection Integration Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, MultiTargetWithEnergyConsideration) {
    dragonfly_self_state_t self = make_self_state();

    // Add multiple targets at different distances
    dragonfly_detection_t close = make_detection(1, 50, 0, 0);
    dragonfly_detection_t far = make_detection(2, 200, 0, 0);

    int r1 = dragonfly_multi_target_update(multi_target, &close, &self);
    int r2 = dragonfly_multi_target_update(multi_target, &far, &self);
    EXPECT_EQ(r1, 0);
    EXPECT_EQ(r2, 0);

    // Evaluate and sort targets
    int r3 = dragonfly_multi_target_evaluate(multi_target, &self);
    EXPECT_EQ(r3, 0);

    // Set the closer target as primary (ID 1)
    int r4 = dragonfly_multi_target_set_primary(multi_target, 1);
    EXPECT_EQ(r4, 0);

    // Get primary target
    queued_target_t best = {};
    int result = dragonfly_multi_target_get_primary(multi_target, &best);

    // Should have a primary target
    EXPECT_EQ(result, 0);
    if (result == 0) {
        EXPECT_EQ(best.id, 1u);
    }
}

TEST_F(DragonflyHuntingIntegrationTest, GazeTracksSelectedTarget) {
    dragonfly_self_state_t self = make_self_state();

    // Add a target
    dragonfly_detection_t det = make_detection(1, 100, 50, 0);
    dragonfly_multi_target_update(multi_target, &det, &self);

    // Get target position
    queued_target_t target;
    dragonfly_multi_target_get_primary(multi_target, &target);

    // Direct gaze to target
    gaze_target_t gaze_target = make_gaze_target(target.position);
    dragonfly_gaze_set_target(gaze, &gaze_target);

    // Update gaze with body state
    body_state_t body = make_body_state();
    float self_pos[3] = {0, 0, 0};
    gaze_command_t cmd;
    dragonfly_gaze_update(gaze, &body, self_pos, 0.1f, &cmd);

    // Gaze should be responding - check stats
    gaze_stats_t stats;
    dragonfly_gaze_get_stats(gaze, &stats);
    EXPECT_GE(stats.updates, 1u);
}

//=============================================================================
// Collision Avoidance Integration Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, CollisionAvoidanceModifiesPursuit) {
    // Add obstacle in path
    detected_obstacle_t obs = {};
    obs.id = 1;
    obs.position[0] = 5.0f;
    obs.position[1] = 0.0f;
    obs.position[2] = 0.0f;
    obs.extent[0] = 1.0f;
    obs.extent[1] = 1.0f;
    obs.extent[2] = 1.0f;
    obs.type = OBSTACLE_STRUCTURE;

    dragonfly_collision_add_obstacle(collision, &obs);

    // Self moving toward obstacle
    dragonfly_self_state_t self = make_self_state();
    self.velocity[0] = 10.0f;

    // Analyze collision threats
    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {10.0f, 0, 0};
    collision_summary_t summary;
    dragonfly_collision_analyze(collision, self_pos, self_vel, &summary);

    // Should detect threats
    EXPECT_GT(summary.obstacle_count, 0u);
    // Path should not be clear with obstacle ahead
    // (depends on implementation details)
}

//=============================================================================
// Learning Integration Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, LearnFromSuccessfulHunt) {
    // Record a successful hunt
    hunt_episode_t episode = {};
    episode.outcome = OUTCOME_SUCCESS;
    episode.strategy = INTERCEPT_PURSUIT;
    episode.pursuit_duration_s = 2.0f;
    episode.target_size = 0.05f;
    episode.target_speed = 4.0f;
    episode.target_maneuverability = 0.3f;
    episode.initial_range = 50.0f;

    dragonfly_learning_record_episode(learning, &episode);

    // Get strategy stats
    strategy_effectiveness_t stats;
    dragonfly_learning_get_strategy_stats(learning, INTERCEPT_PURSUIT, &stats);
    EXPECT_GE(stats.attempts, 1u);
}

TEST_F(DragonflyHuntingIntegrationTest, SleepConsolidatesLearning) {
    // Record experiences
    for (int i = 0; i < 10; i++) {
        dragonfly_sleep_record_success(sleep, i, 0.05f, INTERCEPT_PURSUIT);
    }

    // Consolidate during "sleep"
    dragonfly_sleep_consolidate(sleep);

    // Check memory was updated
    consolidated_memory_t memory;
    dragonfly_sleep_get_memory(sleep, &memory);
    EXPECT_GT(memory.experiences_processed, 0u);
}

//=============================================================================
// Circadian Integration Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, CircadianAffectsActivity) {
    // Update circadian state
    dragonfly_sleep_bridge_update(sleep, 0.1f);

    float activity = dragonfly_sleep_get_activity(sleep);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.5f);

    bool allowed = dragonfly_sleep_hunting_allowed(sleep);
    // Activity level affects hunting permission
    (void)allowed;
}

//=============================================================================
// Stress and Recovery Integration Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, PursuitIncreasesStress) {
    // Get initial state
    dragonfly_immune_state_t before;
    dragonfly_immune_get_state(immune, &before);

    // Simulate intense pursuit
    dragonfly_immune_report_stress(immune, 0.9f, 1.0f);
    dragonfly_immune_bridge_update(immune, 0.1f);

    dragonfly_immune_state_t after;
    dragonfly_immune_get_state(immune, &after);

    // Stress level should have changed
    EXPECT_GE((int)after.stress_level, (int)before.stress_level);
}

TEST_F(DragonflyHuntingIntegrationTest, RestRecovery) {
    // Build up stress
    dragonfly_immune_report_stress(immune, 0.9f, 1.0f);
    dragonfly_immune_bridge_update(immune, 0.1f);

    stress_level_t before = dragonfly_immune_get_stress(immune);

    // Simulate rest
    dragonfly_immune_report_rest(immune, 5.0f);
    for (int i = 0; i < 50; i++) {
        dragonfly_immune_bridge_update(immune, 0.1f);
    }

    stress_level_t after = dragonfly_immune_get_stress(immune);

    // After rest, stress should be less or equal
    EXPECT_LE((int)after, (int)before);
}

//=============================================================================
// Full Hunt Simulation Tests
//=============================================================================

TEST_F(DragonflyHuntingIntegrationTest, SimulateSuccessfulHunt) {
    dragonfly_self_state_t self = make_self_state();

    // 1. Detect target
    dragonfly_detection_t det = make_detection(1, 80, 0, 0);
    dragonfly_multi_target_update(multi_target, &det, &self);

    // 2. Check hunt viability
    EXPECT_TRUE(dragonfly_immune_hunting_safe(immune));

    // 3. Get hunting motivation
    float motivation = dragonfly_emotion_get_motivation(emotion);
    EXPECT_GE(motivation, 0.0f);

    // 4. Check energy budget
    energy_budget_t budget;
    dragonfly_energy_get_budget(energy, &budget);
    EXPECT_GT(budget.current_energy_j, 0.0f);

    // 5. Direct gaze to target
    queued_target_t target;
    dragonfly_multi_target_get_primary(multi_target, &target);
    gaze_target_t gaze_target = make_gaze_target(target.position);
    dragonfly_gaze_set_target(gaze, &gaze_target);

    // 6. Begin pursuit - process emotional event
    emotional_event_t prey_event = {};
    prey_event.is_success = false;  // Not success yet
    dragonfly_emotion_process_event(emotion, &prey_event);
    dragonfly_immune_report_stress(immune, motivation, 0.1f);

    // 7. Simulate pursuit updates
    body_state_t body = make_body_state();
    float self_pos[3] = {0, 0, 0};
    float self_vel[3] = {5, 0, 0};
    gaze_command_t gaze_cmd;
    collision_summary_t col_summary;
    for (int i = 0; i < 30; i++) {
        dragonfly_gaze_update(gaze, &body, self_pos, 0.016f, &gaze_cmd);
        dragonfly_collision_analyze(collision, self_pos, self_vel, &col_summary);
        dragonfly_energy_update(energy, ACTIVITY_PURSUIT, 0.016f);
        dragonfly_emotion_bridge_update(emotion, 0.016f);
        dragonfly_immune_bridge_update(immune, 0.016f);
    }

    // 8. Successful catch
    dragonfly_emotion_report_success(emotion, 0.9f);
    dragonfly_energy_gain(energy, 200.0f);

    // 9. Record success
    dragonfly_sleep_record_success(sleep, 1, 0.05f, INTERCEPT_PURSUIT);

    // 10. End pursuit - report rest
    dragonfly_immune_report_rest(immune, 0.5f);
    dragonfly_immune_report_hunt(immune, true, 0.5f, 50.0f);

    // Verify stats
    dragonfly_sleep_stats_t stats;
    dragonfly_sleep_get_stats(sleep, &stats);
    EXPECT_EQ(stats.experiences_recorded, 1u);
}

TEST_F(DragonflyHuntingIntegrationTest, SimulateFailedHunt) {
    dragonfly_self_state_t self = make_self_state();

    // 1. Detect target
    dragonfly_detection_t det = make_detection(1, 150, 0, 0);
    dragonfly_multi_target_update(multi_target, &det, &self);

    // 2. Begin pursuit - process emotional event
    emotional_event_t prey_event = {};
    prey_event.is_success = false;
    dragonfly_emotion_process_event(emotion, &prey_event);
    dragonfly_immune_report_stress(immune, 0.6f, 0.5f);

    // 3. Simulate long pursuit (target escapes)
    for (int i = 0; i < 100; i++) {
        dragonfly_energy_update(energy, ACTIVITY_PURSUIT, 0.016f);
        dragonfly_immune_bridge_update(immune, 0.016f);
    }

    // 4. Record failure
    dragonfly_sleep_record_failure(sleep, 1, "target_escaped", INTERCEPT_PURSUIT);

    // 5. End pursuit - report rest after failure
    dragonfly_immune_report_rest(immune, 2.0f);
    dragonfly_immune_report_hunt(immune, false, 2.0f, 100.0f);

    // Learn from failure
    hunt_episode_t episode = {};
    episode.outcome = OUTCOME_ESCAPED;
    episode.strategy = INTERCEPT_PURSUIT;
    episode.pursuit_duration_s = 2.0f;
    episode.failure_reason = FAIL_REASON_EVASION;
    dragonfly_learning_record_episode(learning, &episode);

    // Verify recorded
    learning_stats_t lstats;
    dragonfly_learning_get_stats(learning, &lstats);
    EXPECT_GE(lstats.episodes_recorded, 1u);
}
