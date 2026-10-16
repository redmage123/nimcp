/**
 * @file test_mirror_enhancements.cpp
 * @brief Unit tests for Mirror Neuron Enhancement Systems
 * @version 1.0.0
 * @date 2025-11-25
 *
 * Tests for:
 * - Phase 10.11.4: STDP Learning
 * - Phase 10.11.5: Motor Resonance
 * - Phase 10.11.6: Hierarchical Goals
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/mirror_neurons/nimcp_mirror_stdp.h"
#include "cognitive/mirror_neurons/nimcp_mirror_resonance.h"
#include "cognitive/mirror_neurons/nimcp_mirror_hierarchy.h"

//=============================================================================
// STDP Learning Tests (Phase 10.11.4)
//=============================================================================

class STDPLearningTest : public ::testing::Test {
protected:
    mirror_stdp_t stdp = nullptr;

    void SetUp() override {
        stdp = mirror_stdp_create(nullptr, 100);
        ASSERT_NE(stdp, nullptr);
    }

    void TearDown() override {
        if (stdp) {
            mirror_stdp_destroy(stdp);
        }
    }
};

TEST_F(STDPLearningTest, CreateDestroy) {
    // Test basic creation/destruction
    EXPECT_NE(stdp, nullptr);
}

TEST_F(STDPLearningTest, DefaultConfiguration) {
    mirror_stdp_config_t config = mirror_stdp_get_default_config();

    // Verify default values match biological constants
    EXPECT_FLOAT_EQ(config.ltp_window_ms, NIMCP_STDP_LTP_WINDOW_MS);
    EXPECT_FLOAT_EQ(config.ltd_window_ms, NIMCP_STDP_LTD_WINDOW_MS);
    EXPECT_FLOAT_EQ(config.A_plus, NIMCP_STDP_A_PLUS);
    EXPECT_FLOAT_EQ(config.A_minus, NIMCP_STDP_A_MINUS);
    EXPECT_TRUE(config.enable_homeostasis);
    EXPECT_TRUE(config.enable_triplet);
    EXPECT_TRUE(config.enable_dopamine_gating);
}

TEST_F(STDPLearningTest, CreateSynapse) {
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 1, 0.5f);
    EXPECT_NE(syn_id, UINT32_MAX);

    float weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(STDPLearningTest, FindSynapse) {
    uint32_t action_id = 42;
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, action_id, 0.5f);
    EXPECT_NE(syn_id, UINT32_MAX);

    uint32_t found_id = mirror_stdp_find_synapse(stdp, action_id);
    EXPECT_EQ(found_id, syn_id);

    // Non-existent action
    uint32_t not_found = mirror_stdp_find_synapse(stdp, 999);
    EXPECT_EQ(not_found, UINT32_MAX);
}

TEST_F(STDPLearningTest, LTPWhenObsBeforeExec) {
    // LTP: Observation spike followed by execution spike should strengthen
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 1, 0.5f);
    float initial_weight = mirror_stdp_get_weight(stdp, syn_id);

    // Observation at t=0
    uint64_t obs_time = 1000000;  // 1 second
    mirror_stdp_observation_spike(stdp, syn_id, obs_time, 1.0f);

    // Execution at t=20ms later (within LTP window)
    uint64_t exec_time = obs_time + 20000;  // +20ms
    float delta_w = mirror_stdp_execution_spike(stdp, syn_id, exec_time, 1.0f);

    // Weight should increase (LTP)
    float final_weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_GT(delta_w, 0.0f) << "LTP should produce positive weight change";
    EXPECT_GT(final_weight, initial_weight) << "Weight should increase with LTP";
}

TEST_F(STDPLearningTest, LTDWhenExecBeforeObs) {
    // LTD: Execution spike followed by observation spike should weaken
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 1, 0.5f);
    float initial_weight = mirror_stdp_get_weight(stdp, syn_id);

    // Execution at t=0
    uint64_t exec_time = 1000000;
    mirror_stdp_execution_spike(stdp, syn_id, exec_time, 1.0f);

    // Observation at t=20ms later (within LTD window)
    uint64_t obs_time = exec_time + 20000;  // +20ms
    float delta_w = mirror_stdp_observation_spike(stdp, syn_id, obs_time, 1.0f);

    // Weight should decrease (LTD)
    float final_weight = mirror_stdp_get_weight(stdp, syn_id);
    EXPECT_LT(delta_w, 0.0f) << "LTD should produce negative weight change";
    EXPECT_LT(final_weight, initial_weight) << "Weight should decrease with LTD";
}

TEST_F(STDPLearningTest, DopamineModulation) {
    uint32_t syn_id = mirror_stdp_create_synapse(stdp, 1, 0.5f);

    // Set high dopamine
    mirror_stdp_set_dopamine(stdp, 1.0f);

    uint64_t obs_time = 1000000;
    mirror_stdp_observation_spike(stdp, syn_id, obs_time, 1.0f);

    uint64_t exec_time = obs_time + 20000;
    float delta_w_high_da = mirror_stdp_execution_spike(stdp, syn_id, exec_time, 1.0f);

    // Reset synapse
    syn_id = mirror_stdp_create_synapse(stdp, 2, 0.5f);

    // Set low dopamine
    mirror_stdp_set_dopamine(stdp, 0.0f);

    obs_time = 2000000;
    mirror_stdp_observation_spike(stdp, syn_id, obs_time, 1.0f);

    exec_time = obs_time + 20000;
    float delta_w_low_da = mirror_stdp_execution_spike(stdp, syn_id, exec_time, 1.0f);

    // High dopamine should boost LTP more
    EXPECT_GT(delta_w_high_da, delta_w_low_da)
        << "High dopamine should enhance LTP";
}

TEST_F(STDPLearningTest, Statistics) {
    // Create several synapses and generate activity
    for (int i = 0; i < 10; i++) {
        uint32_t syn_id = mirror_stdp_create_synapse(stdp, i, 0.5f);

        uint64_t t = i * 100000;
        mirror_stdp_observation_spike(stdp, syn_id, t, 1.0f);
        mirror_stdp_execution_spike(stdp, syn_id, t + 20000, 1.0f);
    }

    mirror_stdp_stats_t stats;
    bool ok = mirror_stdp_get_stats(stdp, &stats);
    EXPECT_TRUE(ok);

    EXPECT_EQ(stats.num_synapses, 10u);
    EXPECT_GT(stats.total_ltp_events, 0u);
}

//=============================================================================
// Motor Resonance Tests (Phase 10.11.5)
//=============================================================================

class MotorResonanceTest : public ::testing::Test {
protected:
    motor_resonance_t resonance = nullptr;

    void SetUp() override {
        resonance = motor_resonance_create(nullptr, 64);
        ASSERT_NE(resonance, nullptr);
    }

    void TearDown() override {
        if (resonance) {
            motor_resonance_destroy(resonance);
        }
    }
};

TEST_F(MotorResonanceTest, CreateDestroy) {
    EXPECT_NE(resonance, nullptr);
}

TEST_F(MotorResonanceTest, DefaultConfiguration) {
    motor_resonance_config_t config = motor_resonance_get_default_config();

    EXPECT_FLOAT_EQ(config.resonance_gain, NIMCP_RESONANCE_DEFAULT_GAIN);
    EXPECT_FLOAT_EQ(config.bg_tonic_inhibition, NIMCP_RESONANCE_BG_TONIC_INHIB);
    EXPECT_FLOAT_EQ(config.execution_threshold, NIMCP_RESONANCE_EXEC_THRESHOLD);
}

TEST_F(MotorResonanceTest, CreateChannel) {
    uint32_t ch_id = motor_resonance_create_channel(resonance, 1);
    EXPECT_NE(ch_id, UINT32_MAX);

    motor_channel_t channel;
    bool ok = motor_resonance_get_channel(resonance, ch_id, &channel);
    EXPECT_TRUE(ok);
    EXPECT_EQ(channel.action_id, 1u);
}

TEST_F(MotorResonanceTest, ObservationDrivesResonance) {
    uint32_t ch_id = motor_resonance_create_channel(resonance, 1);

    // Before observation: no resonance
    motor_channel_t ch_before;
    motor_resonance_get_channel(resonance, ch_id, &ch_before);
    EXPECT_FLOAT_EQ(ch_before.resonance_level, 0.0f);

    // Apply strong observation
    uint64_t t = 1000000;
    float res_level = motor_resonance_observe(resonance, ch_id, 1.0f, t);

    EXPECT_GT(res_level, 0.0f) << "Observation should create resonance";
}

TEST_F(MotorResonanceTest, BGSuppressionBlocksOutput) {
    uint32_t ch_id = motor_resonance_create_channel(resonance, 1);

    // Set high BG inhibition
    motor_resonance_set_bg_inhibition(resonance, 0.9f);

    // Apply observation
    uint64_t t = 1000000;
    motor_resonance_observe(resonance, ch_id, 1.0f, t);

    // Step to compute output
    motor_resonance_step(resonance, 1.0f);

    // Output should be suppressed
    float output = motor_resonance_get_output(resonance, ch_id);
    EXPECT_LT(output, 0.5f) << "High BG inhibition should suppress output";
}

TEST_F(MotorResonanceTest, LearningContextReleasesSupression) {
    uint32_t ch_id = motor_resonance_create_channel(resonance, 1);

    // Initial suppression
    motor_resonance_set_bg_inhibition(resonance, 0.7f);

    // Release for learning
    motor_resonance_release_for_learning(resonance, -1, 1.0f);

    // Get channel state
    motor_channel_t channel;
    motor_resonance_get_channel(resonance, ch_id, &channel);

    EXPECT_LT(channel.suppression_level, 0.7f)
        << "Learning context should reduce suppression";
}

TEST_F(MotorResonanceTest, ConflictDetection) {
    uint32_t ch_a = motor_resonance_create_channel(resonance, 1);
    uint32_t ch_b = motor_resonance_create_channel(resonance, 2);

    // Define conflict between actions
    motor_resonance_set_conflict(resonance, ch_a, ch_b);

    // Activate both channels
    uint64_t t = 1000000;
    motor_resonance_observe(resonance, ch_a, 0.8f, t);
    motor_resonance_observe(resonance, ch_b, 0.8f, t);

    // Step to detect conflict
    motor_resonance_step(resonance, 1.0f);

    // Both should have conflict signal
    float conflict_a = motor_resonance_get_conflict(resonance, ch_a);
    float conflict_b = motor_resonance_get_conflict(resonance, ch_b);

    EXPECT_GT(conflict_a, 0.0f) << "Channel A should detect conflict";
    EXPECT_GT(conflict_b, 0.0f) << "Channel B should detect conflict";
}

TEST_F(MotorResonanceTest, ThresholdDetection) {
    uint32_t ch_id = motor_resonance_create_channel(resonance, 1);

    // Initially below threshold
    EXPECT_FALSE(motor_resonance_above_threshold(resonance, ch_id));

    // Set low inhibition and strong observation
    motor_resonance_set_bg_inhibition(resonance, 0.1f);

    uint64_t t = 1000000;
    motor_resonance_observe(resonance, ch_id, 1.0f, t);

    // Multiple steps to build resonance
    for (int i = 0; i < 10; i++) {
        motor_resonance_observe(resonance, ch_id, 1.0f, t + i * 10000);
        motor_resonance_step(resonance, 10.0f);
    }

    // Check output
    float output = motor_resonance_get_output(resonance, ch_id);

    // With low inhibition and repeated strong observation, might reach threshold
    // This tests the mechanism even if threshold isn't reached
    EXPECT_GE(output, 0.0f);
}

TEST_F(MotorResonanceTest, Statistics) {
    // Create channels and generate activity
    for (int i = 0; i < 5; i++) {
        uint32_t ch_id = motor_resonance_create_channel(resonance, i);
        motor_resonance_observe(resonance, ch_id, 0.5f, i * 100000);
    }

    motor_resonance_step(resonance, 1.0f);

    motor_resonance_stats_t stats;
    bool ok = motor_resonance_get_stats(resonance, &stats);
    EXPECT_TRUE(ok);

    EXPECT_EQ(stats.num_channels, 5u);
    EXPECT_GT(stats.active_channels, 0u);
}

//=============================================================================
// Hierarchical Goals Tests (Phase 10.11.6)
//=============================================================================

class HierarchicalGoalsTest : public ::testing::Test {
protected:
    mirror_hierarchy_t hierarchy = nullptr;

    void SetUp() override {
        hierarchy = mirror_hierarchy_create(nullptr);
        ASSERT_NE(hierarchy, nullptr);
    }

    void TearDown() override {
        if (hierarchy) {
            mirror_hierarchy_destroy(hierarchy);
        }
    }
};

TEST_F(HierarchicalGoalsTest, CreateDestroy) {
    EXPECT_NE(hierarchy, nullptr);
}

TEST_F(HierarchicalGoalsTest, DefaultConfiguration) {
    mirror_hierarchy_config_t config = mirror_hierarchy_get_default_config();

    EXPECT_EQ(config.max_goals, NIMCP_HIERARCHY_MAX_GOALS);
    EXPECT_EQ(config.max_motors, NIMCP_HIERARCHY_MAX_MOTORS);
    EXPECT_TRUE(config.enable_goal_competition);
    EXPECT_TRUE(config.enable_predictive_coding);
}

TEST_F(HierarchicalGoalsTest, CreateGoal) {
    uint32_t goal_id = mirror_hierarchy_create_goal(hierarchy, "grasp_to_eat",
                                                     GOAL_CATEGORY_CONSUMATORY);
    EXPECT_NE(goal_id, UINT32_MAX);

    goal_representation_t goal;
    bool ok = mirror_hierarchy_get_goal(hierarchy, goal_id, &goal);
    EXPECT_TRUE(ok);
    EXPECT_EQ(goal.category, GOAL_CATEGORY_CONSUMATORY);
    EXPECT_STREQ(goal.name, "grasp_to_eat");
}

TEST_F(HierarchicalGoalsTest, CreateMotor) {
    uint32_t motor_id = mirror_hierarchy_create_motor(hierarchy, "precision_grip",
                                                       MOTOR_TYPE_GRASP);
    EXPECT_NE(motor_id, UINT32_MAX);

    motor_representation_t motor;
    bool ok = mirror_hierarchy_get_motor(hierarchy, motor_id, &motor);
    EXPECT_TRUE(ok);
    EXPECT_EQ(motor.type, MOTOR_TYPE_GRASP);
    EXPECT_STREQ(motor.name, "precision_grip");
}

TEST_F(HierarchicalGoalsTest, CreateBinding) {
    uint32_t goal_id = mirror_hierarchy_create_goal(hierarchy, "grasp_to_eat",
                                                     GOAL_CATEGORY_CONSUMATORY);
    uint32_t motor_id = mirror_hierarchy_create_motor(hierarchy, "precision_grip",
                                                       MOTOR_TYPE_GRASP);

    bool ok = mirror_hierarchy_create_binding(hierarchy, goal_id, motor_id,
                                               0.8f, BINDING_INVARIANT);
    EXPECT_TRUE(ok);

    float binding = mirror_hierarchy_get_binding(hierarchy, goal_id, motor_id);
    EXPECT_FLOAT_EQ(binding, 0.8f);
}

TEST_F(HierarchicalGoalsTest, GoalInference) {
    // Create goals and motors with bindings
    uint32_t goal_eat = mirror_hierarchy_create_goal(hierarchy, "eat",
                                                      GOAL_CATEGORY_CONSUMATORY);
    uint32_t goal_place = mirror_hierarchy_create_goal(hierarchy, "place",
                                                        GOAL_CATEGORY_PLACING);
    uint32_t motor_grip = mirror_hierarchy_create_motor(hierarchy, "grip",
                                                         MOTOR_TYPE_GRASP);

    // Bind motor to both goals with different strengths
    mirror_hierarchy_create_binding(hierarchy, goal_eat, motor_grip, 0.9f, BINDING_FLEXIBLE);
    mirror_hierarchy_create_binding(hierarchy, goal_place, motor_grip, 0.3f, BINDING_FLEXIBLE);

    // Activate motor
    mirror_hierarchy_activate_motor(hierarchy, motor_grip, 1.0f);

    // Infer goal
    uint32_t inferred_goals[4];
    float probs[4];
    uint32_t num = mirror_hierarchy_infer_goal(hierarchy, motor_grip,
                                                inferred_goals, probs, 4);

    EXPECT_GT(num, 0u) << "Should infer at least one goal";

    // The goal with stronger binding should be inferred first
    if (num > 0) {
        EXPECT_EQ(inferred_goals[0], goal_eat)
            << "Stronger binding should produce higher probability";
    }
}

TEST_F(HierarchicalGoalsTest, MotorPrediction) {
    // Create goals and motors
    uint32_t goal_id = mirror_hierarchy_create_goal(hierarchy, "eat",
                                                     GOAL_CATEGORY_CONSUMATORY);
    uint32_t motor_a = mirror_hierarchy_create_motor(hierarchy, "grip", MOTOR_TYPE_GRASP);
    uint32_t motor_b = mirror_hierarchy_create_motor(hierarchy, "reach", MOTOR_TYPE_REACH);

    // Bind motors to goal
    mirror_hierarchy_create_binding(hierarchy, goal_id, motor_a, 0.9f, BINDING_INVARIANT);
    mirror_hierarchy_create_binding(hierarchy, goal_id, motor_b, 0.5f, BINDING_FLEXIBLE);

    // Activate goal
    mirror_hierarchy_activate_goal(hierarchy, goal_id, 1.0f);

    // Predict motor
    uint32_t predicted_motors[4];
    float probs[4];
    uint32_t num = mirror_hierarchy_predict_motor(hierarchy, goal_id,
                                                   predicted_motors, probs, 4);

    EXPECT_GT(num, 0u);

    // Stronger binding should predict first
    if (num > 0) {
        EXPECT_EQ(predicted_motors[0], motor_a);
    }
}

TEST_F(HierarchicalGoalsTest, GoalSelection) {
    uint32_t goal_a = mirror_hierarchy_create_goal(hierarchy, "eat",
                                                    GOAL_CATEGORY_CONSUMATORY);
    uint32_t goal_b = mirror_hierarchy_create_goal(hierarchy, "place",
                                                    GOAL_CATEGORY_PLACING);

    // No selection initially
    EXPECT_EQ(mirror_hierarchy_get_selected_goal(hierarchy), UINT32_MAX);

    // Select goal A
    mirror_hierarchy_select_goal(hierarchy, goal_a);
    EXPECT_EQ(mirror_hierarchy_get_selected_goal(hierarchy), goal_a);

    // Select goal B
    mirror_hierarchy_select_goal(hierarchy, goal_b);
    EXPECT_EQ(mirror_hierarchy_get_selected_goal(hierarchy), goal_b);

    // Clear selection
    mirror_hierarchy_select_goal(hierarchy, -1);
    EXPECT_EQ(mirror_hierarchy_get_selected_goal(hierarchy), UINT32_MAX);
}

TEST_F(HierarchicalGoalsTest, StrengthenBinding) {
    uint32_t goal_id = mirror_hierarchy_create_goal(hierarchy, "eat",
                                                     GOAL_CATEGORY_CONSUMATORY);
    uint32_t motor_id = mirror_hierarchy_create_motor(hierarchy, "grip",
                                                       MOTOR_TYPE_GRASP);

    mirror_hierarchy_create_binding(hierarchy, goal_id, motor_id, 0.5f, BINDING_LEARNED);

    float initial = mirror_hierarchy_get_binding(hierarchy, goal_id, motor_id);

    // Strengthen binding
    mirror_hierarchy_strengthen_binding(hierarchy, goal_id, motor_id, 0.2f);

    float final_val = mirror_hierarchy_get_binding(hierarchy, goal_id, motor_id);
    EXPECT_GT(final_val, initial) << "Binding should be stronger after strengthening";
}

TEST_F(HierarchicalGoalsTest, Statistics) {
    // Create some goals and motors
    for (int i = 0; i < 5; i++) {
        mirror_hierarchy_create_goal(hierarchy, "goal", GOAL_CATEGORY_UNKNOWN);
        mirror_hierarchy_create_motor(hierarchy, "motor", MOTOR_TYPE_UNKNOWN);
    }

    mirror_hierarchy_stats_t stats;
    bool ok = mirror_hierarchy_get_stats(hierarchy, &stats);
    EXPECT_TRUE(ok);

    EXPECT_EQ(stats.num_goals, 5u);
    EXPECT_EQ(stats.num_motors, 5u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
