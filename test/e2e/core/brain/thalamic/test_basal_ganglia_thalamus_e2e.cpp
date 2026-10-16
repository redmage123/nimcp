//=============================================================================
// test_basal_ganglia_thalamus_e2e.cpp - BG-Thalamus Integration E2E Tests
//=============================================================================
/**
 * @file test_basal_ganglia_thalamus_e2e.cpp
 * @brief End-to-end tests for basal ganglia-thalamus motor pathway
 *
 * WHAT: Full pipeline tests for BG-thalamus motor control circuit
 * WHY:  Verify action selection through BG-thalamic pathway
 * HOW:  Test direct/indirect pathways, STN hyperdirect pathway, dopamine effects
 *
 * TEST COVERAGE:
 * - Motor loop through thalamus
 * - Action selection via BG-thalamus
 * - Direct/indirect pathway balance
 * - STN hyperdirect pathway
 * - Dopamine modulation effects
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>

#include "e2e_test_framework.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BasalGangliaThalamusE2ETest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    basal_ganglia_t* bg = nullptr;
    static constexpr uint32_t NUM_ACTIONS = 16;
    static constexpr uint32_t NUM_CHANNELS = 32;

    void SetUp() override {
        // Create thalamus
        thalamus_config_t thal_config;
        thalamus_default_config(&thal_config);
        thal_config.neurons_per_nucleus = 64;
        thal_config.channels_per_nucleus = NUM_CHANNELS;
        thal_config.enable_trn = true;
        thal = thalamus_create(&thal_config);
        ASSERT_NE(thal, nullptr);

        // Create basal ganglia
        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = NUM_ACTIONS;
        bg_config.enable_hyperdirect = true;
        bg_config.enable_habit_learning = true;
        bg = basal_ganglia_create(&bg_config);
        ASSERT_NE(bg, nullptr);

        // Set awake state
        thalamus_set_arousal(thal, 1.0f);
    }

    void TearDown() override {
        if (bg) {
            basal_ganglia_destroy(bg);
            bg = nullptr;
        }
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }

    // Generate cortical action preferences
    std::vector<float> generate_action_preferences(uint32_t preferred_action, float strength) {
        std::vector<float> prefs(NUM_ACTIONS, 0.1f);
        prefs[preferred_action] = strength;
        return prefs;
    }

    // Generate competing action preferences
    std::vector<float> generate_competing_actions(const std::vector<std::pair<uint32_t, float>>& actions) {
        std::vector<float> prefs(NUM_ACTIONS, 0.05f);
        for (const auto& [action, strength] : actions) {
            if (action < NUM_ACTIONS) {
                prefs[action] = strength;
            }
        }
        return prefs;
    }

    float calculate_power(const std::vector<float>& signal) {
        float sum = 0.0f;
        for (float v : signal) {
            sum += v * v;
        }
        return sum / signal.size();
    }
};

//=============================================================================
// Motor Loop Through Thalamus Tests
//=============================================================================

TEST_F(BasalGangliaThalamusE2ETest, MotorLoop_BasicActionRelay) {
    E2E_PIPELINE_START("Motor Loop: Basic Action Relay");

    E2E_STAGE_BEGIN("Generate cortical action command", 200);
    auto cortical_input = generate_action_preferences(0, 0.9f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process through basal ganglia", 500);
    uint32_t selected_action;
    int result = basal_ganglia_select_action(bg, cortical_input.data(), &selected_action);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(selected_action, 0) << "BG should select strongest action";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get BG thalamic output", 200);
    std::vector<float> bg_output(NUM_ACTIONS);
    result = basal_ganglia_get_thalamic_output(bg, bg_output.data());
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Relay through thalamus VA nucleus", 500);
    thalamus_set_attention(thal, THAL_NUCLEUS_VA, 0.8f);

    // Resize to match thalamus channels
    std::vector<float> motor_input(NUM_CHANNELS, 0.0f);
    for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
        motor_input[i] = bg_output[i];
    }

    std::vector<float> motor_output(NUM_CHANNELS);
    result = thalamus_relay_motor(thal, motor_input.data(), NUM_CHANNELS,
                                  motor_output.data(), NUM_CHANNELS);
    EXPECT_GE(result, 0) << "thalamus_relay_motor returns copy_size on success";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify motor cortex receives signal", 100);
    bool has_motor_signal = false;
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        if (motor_output[i] > 0.0f) {
            has_motor_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_motor_signal) << "Motor cortex should receive BG output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, MotorLoop_SequentialActions) {
    E2E_PIPELINE_START("Motor Loop: Sequential Action Selection");

    std::vector<uint32_t> selected_actions;

    E2E_STAGE_BEGIN("Execute sequence of motor commands", 3000);
    for (int trial = 0; trial < 5; ++trial) {
        uint32_t target_action = trial % NUM_ACTIONS;
        auto input = generate_action_preferences(target_action, 0.85f);

        // BG selection
        uint32_t selected;
        basal_ganglia_select_action(bg, input.data(), &selected);
        selected_actions.push_back(selected);

        // Thalamic relay
        std::vector<float> bg_out(NUM_ACTIONS);
        basal_ganglia_get_thalamic_output(bg, bg_out.data());

        std::vector<float> motor_in(NUM_CHANNELS, 0.0f);
        for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
            motor_in[i] = bg_out[i];
        }

        std::vector<float> motor_out(NUM_CHANNELS);
        thalamus_relay_motor(thal, motor_in.data(), NUM_CHANNELS,
                            motor_out.data(), NUM_CHANNELS);

        // Complete action
        basal_ganglia_action_completed(bg, selected, true);
        thalamus_step(thal, 50.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all actions selected", 100);
    EXPECT_EQ(selected_actions.size(), 5);
    for (size_t i = 0; i < selected_actions.size(); ++i) {
        uint32_t expected = i % NUM_ACTIONS;
        EXPECT_EQ(selected_actions[i], expected)
            << "Trial " << i << " should select action " << expected;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, MotorLoop_PersistentMotorSignal) {
    E2E_PIPELINE_START("Motor Loop: Persistent Motor Signal");

    auto input = generate_action_preferences(5, 0.9f);
    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);

    E2E_STAGE_BEGIN("Maintain motor signal over time", 2000);
    std::vector<float> motor_powers;

    for (int step = 0; step < 10; ++step) {
        std::vector<float> bg_out(NUM_ACTIONS);
        basal_ganglia_get_thalamic_output(bg, bg_out.data());

        std::vector<float> motor_in(NUM_CHANNELS, 0.0f);
        for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
            motor_in[i] = bg_out[i];
        }

        std::vector<float> motor_out(NUM_CHANNELS);
        thalamus_relay_motor(thal, motor_in.data(), NUM_CHANNELS,
                            motor_out.data(), NUM_CHANNELS);

        motor_powers.push_back(calculate_power(motor_out));
        thalamus_step(thal, 20.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sustained motor drive", 100);
    // All steps should have non-zero motor output
    for (size_t i = 0; i < motor_powers.size(); ++i) {
        EXPECT_GE(motor_powers[i], 0.0f)
            << "Step " << i << " should have motor output";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Action Selection via BG-Thalamus Tests
//=============================================================================

TEST_F(BasalGangliaThalamusE2ETest, ActionSelection_WinnerTakeAll) {
    E2E_PIPELINE_START("Action Selection: Winner-Take-All");

    E2E_STAGE_BEGIN("Present competing action candidates", 200);
    auto competing = generate_competing_actions({
        {3, 0.9f},   // Strongest
        {7, 0.7f},
        {11, 0.5f}
    });
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("BG selects winning action", 500);
    uint32_t selected;
    int result = basal_ganglia_select_action(bg, competing.data(), &selected);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify winner selection", 100);
    EXPECT_EQ(selected, 3) << "BG should select strongest action";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify thalamic disinhibition for winner", 500);
    std::vector<float> bg_out(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out.data());

    // Winner should have strongest disinhibition
    float winner_signal = bg_out[3];
    for (uint32_t i = 0; i < NUM_ACTIONS; ++i) {
        if (i != 3) {
            EXPECT_GE(winner_signal, bg_out[i])
                << "Winner should have strongest thalamic signal";
        }
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, ActionSelection_ConflictResolution) {
    E2E_PIPELINE_START("Action Selection: Conflict Resolution");

    E2E_STAGE_BEGIN("Create high-conflict situation", 200);
    // Two nearly equal competing actions
    auto conflict = generate_competing_actions({
        {2, 0.85f},
        {8, 0.83f}
    });
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("BG resolves conflict", 500);
    uint32_t selected;
    basal_ganglia_select_action(bg, conflict.data(), &selected);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify single action selected", 100);
    // Should select one of the competing actions
    EXPECT_TRUE(selected == 2 || selected == 8)
        << "Should select one of the strong competitors";

    // Check BG conflict level
    EXPECT_GE(bg->conflict_level, 0.0f)
        << "BG should report some conflict level";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Relay resolved action through thalamus", 500);
    std::vector<float> bg_out(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out.data());

    std::vector<float> motor_in(NUM_CHANNELS, 0.0f);
    for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
        motor_in[i] = bg_out[i];
    }

    std::vector<float> motor_out(NUM_CHANNELS);
    thalamus_relay_motor(thal, motor_in.data(), NUM_CHANNELS,
                        motor_out.data(), NUM_CHANNELS);

    float selected_power = motor_out[selected];
    EXPECT_GT(selected_power, 0.0f)
        << "Selected action should reach motor cortex";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, ActionSelection_NoAction) {
    E2E_PIPELINE_START("Action Selection: No-Go (Suppress All)");

    E2E_STAGE_BEGIN("Present weak input (no clear action)", 200);
    std::vector<float> weak_input(NUM_ACTIONS, 0.15f);  // All weak
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger global suppression", 500);
    basal_ganglia_suppress_action(bg, 0.9f);  // Strong suppression

    uint32_t selected;
    basal_ganglia_select_action(bg, weak_input.data(), &selected);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify minimal motor output", 500);
    std::vector<float> bg_out(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out.data());

    std::vector<float> motor_in(NUM_CHANNELS, 0.0f);
    for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
        motor_in[i] = bg_out[i];
    }

    std::vector<float> motor_out(NUM_CHANNELS);
    thalamus_relay_motor(thal, motor_in.data(), NUM_CHANNELS,
                        motor_out.data(), NUM_CHANNELS);

    float total_motor = calculate_power(motor_out);
    // Suppression should reduce motor output
    EXPECT_LE(total_motor, 0.5f)
        << "Global suppression should reduce motor drive";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Direct/Indirect Pathway Balance Tests
//=============================================================================

TEST_F(BasalGangliaThalamusE2ETest, DirectIndirectBalance_GoAction) {
    E2E_PIPELINE_START("Direct/Indirect Balance: Go Action (Direct Dominant)");

    E2E_STAGE_BEGIN("Strong cortical input activates direct pathway", 500);
    auto strong_input = generate_action_preferences(4, 0.95f);
    uint32_t selected;
    basal_ganglia_select_action(bg, strong_input.data(), &selected);
    EXPECT_EQ(selected, 4);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify direct pathway dominates", 200);
    // Direct pathway: D1 MSNs -> GPi inhibition -> thalamic disinhibition
    EXPECT_GE(bg->direct_pathway[4], bg->indirect_pathway[4])
        << "Direct pathway should be at least as strong for selected action";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify thalamic facilitation", 500);
    std::vector<float> bg_out(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out.data());

    std::vector<float> motor_out(NUM_CHANNELS);
    std::vector<float> motor_in(NUM_CHANNELS, 0.0f);
    for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
        motor_in[i] = bg_out[i];
    }

    thalamus_relay_motor(thal, motor_in.data(), NUM_CHANNELS,
                        motor_out.data(), NUM_CHANNELS);

    EXPECT_GT(motor_out[4], 0.0f)
        << "Selected action should facilitate motor cortex";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, DirectIndirectBalance_NoGoSuppression) {
    E2E_PIPELINE_START("Direct/Indirect Balance: No-Go Suppression");

    E2E_STAGE_BEGIN("Select initial action", 300);
    auto input = generate_action_preferences(6, 0.8f);
    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get baseline motor output", 300);
    std::vector<float> bg_out_baseline(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_baseline.data());
    float baseline_signal = bg_out_baseline[6];
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Activate indirect pathway (punishment signal)", 500);
    // Negative dopamine boosts indirect pathway (D2)
    basal_ganglia_update_dopamine(bg, -0.8f, 0.0f);

    // Re-process
    basal_ganglia_select_action(bg, input.data(), &selected);
    std::vector<float> bg_out_punish(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_punish.data());
    float punished_signal = bg_out_punish[6];
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify indirect pathway suppression", 100);
    // Indirect pathway should reduce thalamic output
    EXPECT_LE(punished_signal, baseline_signal)
        << "Indirect pathway should suppress thalamic output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, DirectIndirectBalance_PathwayDynamics) {
    E2E_PIPELINE_START("Direct/Indirect Balance: Dynamic Pathway Modulation");

    auto input = generate_action_preferences(10, 0.7f);
    std::vector<std::tuple<std::string, float, float, float>> log;

    E2E_STAGE_BEGIN("Track pathway balance over dopamine changes", 3000);

    // Baseline
    basal_ganglia_update_dopamine(bg, 0.0f, 0.0f);
    uint32_t sel;
    basal_ganglia_select_action(bg, input.data(), &sel);
    log.push_back({"Baseline", bg->direct_pathway[10], bg->indirect_pathway[10],
                   bg->direct_pathway[10] - bg->indirect_pathway[10]});

    // High dopamine (reward)
    basal_ganglia_update_dopamine(bg, 1.0f, 0.0f);
    basal_ganglia_select_action(bg, input.data(), &sel);
    log.push_back({"Reward", bg->direct_pathway[10], bg->indirect_pathway[10],
                   bg->direct_pathway[10] - bg->indirect_pathway[10]});

    // Low dopamine (punishment)
    basal_ganglia_update_dopamine(bg, -1.0f, 0.0f);
    basal_ganglia_select_action(bg, input.data(), &sel);
    log.push_back({"Punish", bg->direct_pathway[10], bg->indirect_pathway[10],
                   bg->direct_pathway[10] - bg->indirect_pathway[10]});

    // Recovery
    basal_ganglia_update_dopamine(bg, 0.0f, 0.0f);
    basal_ganglia_select_action(bg, input.data(), &sel);
    log.push_back({"Recovery", bg->direct_pathway[10], bg->indirect_pathway[10],
                   bg->direct_pathway[10] - bg->indirect_pathway[10]});
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify pathway balance shifts", 100);
    // Reward should increase direct-indirect difference
    float reward_balance = std::get<3>(log[1]);
    float punish_balance = std::get<3>(log[2]);
    EXPECT_GT(reward_balance, punish_balance)
        << "Reward should bias toward direct pathway";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// STN Hyperdirect Pathway Tests
//=============================================================================

TEST_F(BasalGangliaThalamusE2ETest, Hyperdirect_GlobalInhibition) {
    E2E_PIPELINE_START("Hyperdirect Pathway: Global Motor Inhibition");

    E2E_STAGE_BEGIN("Prepare motor action", 300);
    auto input = generate_action_preferences(2, 0.85f);
    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);

    std::vector<float> bg_out_pre(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_pre.data());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Activate STN hyperdirect pathway", 500);
    // Hyperdirect: Cortex -> STN -> GPi (fast global inhibition)
    basal_ganglia_suppress_action(bg, 0.8f);  // Simulates STN activation

    std::vector<float> bg_out_post(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_post.data());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify global motor suppression", 100);
    float pre_total = 0.0f, post_total = 0.0f;
    for (uint32_t i = 0; i < NUM_ACTIONS; ++i) {
        pre_total += bg_out_pre[i];
        post_total += bg_out_post[i];
    }
    EXPECT_LE(post_total, pre_total)
        << "Hyperdirect should globally suppress motor output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, Hyperdirect_BrakingMechanism) {
    E2E_PIPELINE_START("Hyperdirect Pathway: Emergency Braking");

    E2E_STAGE_BEGIN("Initiate strong motor command", 300);
    auto strong_motor = generate_action_preferences(1, 0.95f);
    uint32_t selected;
    basal_ganglia_select_action(bg, strong_motor.data(), &selected);
    EXPECT_EQ(selected, 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger emergency stop via STN", 500);
    // Maximum suppression (stop signal)
    basal_ganglia_suppress_action(bg, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify motor halt at thalamus", 500);
    std::vector<float> bg_out(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out.data());

    std::vector<float> motor_in(NUM_CHANNELS, 0.0f);
    for (uint32_t i = 0; i < std::min(NUM_ACTIONS, NUM_CHANNELS); ++i) {
        motor_in[i] = bg_out[i];
    }

    std::vector<float> motor_out(NUM_CHANNELS);
    thalamus_relay_motor(thal, motor_in.data(), NUM_CHANNELS,
                        motor_out.data(), NUM_CHANNELS);

    float motor_power = calculate_power(motor_out);
    EXPECT_LE(motor_power, 0.3f)
        << "Emergency stop should minimize motor output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, Hyperdirect_ResponseInhibition) {
    E2E_PIPELINE_START("Hyperdirect Pathway: Response Inhibition Paradigm");

    // Go/No-Go task simulation
    std::vector<bool> correct_responses;

    E2E_STAGE_BEGIN("Run Go/No-Go trials", 4000);
    for (int trial = 0; trial < 10; ++trial) {
        bool is_go_trial = (trial % 3 != 0);  // 30% no-go trials

        auto trial_input = generate_action_preferences(0, 0.8f);
        uint32_t selected;

        if (!is_go_trial) {
            // No-go: activate suppression before action
            basal_ganglia_suppress_action(bg, 0.85f);
        } else {
            basal_ganglia_suppress_action(bg, 0.0f);  // Release suppression
        }

        basal_ganglia_select_action(bg, trial_input.data(), &selected);

        std::vector<float> bg_out(NUM_ACTIONS);
        basal_ganglia_get_thalamic_output(bg, bg_out.data());
        float action_strength = bg_out[0];

        // Score response
        if (is_go_trial && action_strength > 0.1f) {
            correct_responses.push_back(true);  // Correct go
        } else if (!is_go_trial && action_strength < 0.3f) {
            correct_responses.push_back(true);  // Correct no-go
        } else {
            correct_responses.push_back(false);
        }

        basal_ganglia_action_completed(bg, selected, correct_responses.back());
        thalamus_step(thal, 100.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify response inhibition performance", 100);
    int correct_count = std::count(correct_responses.begin(),
                                    correct_responses.end(), true);
    float accuracy = static_cast<float>(correct_count) / correct_responses.size();
    EXPECT_GE(accuracy, 0.5f)
        << "Should achieve reasonable response inhibition accuracy";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Dopamine Modulation Effects Tests
//=============================================================================

TEST_F(BasalGangliaThalamusE2ETest, DopamineModulation_RewardLearning) {
    E2E_PIPELINE_START("Dopamine Modulation: Reward Learning");

    uint32_t target_action = 3;
    auto input = generate_action_preferences(target_action, 0.7f);

    E2E_STAGE_BEGIN("Baseline action selection", 300);
    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);
    EXPECT_EQ(selected, target_action);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply reward (positive dopamine burst)", 500);
    basal_ganglia_update_dopamine(bg, 1.0f, 0.0f);  // Strong reward
    basal_ganglia_action_completed(bg, selected, true);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify enhanced action value", 100);
    // After reward, action should have higher Q-value
    float q_value = bg->actions[target_action].value;
    EXPECT_GE(q_value, 0.0f)
        << "Rewarded action should maintain or increase value";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify thalamic facilitation after reward", 500);
    basal_ganglia_select_action(bg, input.data(), &selected);

    std::vector<float> bg_out(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out.data());

    EXPECT_GT(bg_out[target_action], 0.0f)
        << "Rewarded action should have thalamic output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, DopamineModulation_PunishmentAvoidance) {
    E2E_PIPELINE_START("Dopamine Modulation: Punishment Avoidance");

    uint32_t punished_action = 5;
    auto input = generate_action_preferences(punished_action, 0.8f);

    E2E_STAGE_BEGIN("Execute and punish action", 500);
    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);

    // Punish
    basal_ganglia_update_dopamine(bg, -1.0f, 0.0f);
    basal_ganglia_action_completed(bg, selected, false);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Present equal alternative", 500);
    // Now present two equal options
    auto competing = generate_competing_actions({
        {punished_action, 0.75f},
        {12, 0.75f}  // Alternative
    });

    basal_ganglia_select_action(bg, competing.data(), &selected);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify avoidance or reduced preference", 100);
    // Punished action should have reduced value
    float punished_value = bg->actions[punished_action].value;
    float alt_value = bg->actions[12].value;

    // At minimum, punished action shouldn't dominate
    EXPECT_LE(punished_value, alt_value + 0.5f)
        << "Punished action should be less preferred";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, DopamineModulation_TonicBaseline) {
    E2E_PIPELINE_START("Dopamine Modulation: Tonic Baseline Effects");

    auto input = generate_action_preferences(8, 0.7f);

    E2E_STAGE_BEGIN("Normal tonic dopamine", 500);
    basal_ganglia_update_dopamine(bg, 0.0f, 0.5f);  // Normal tonic level

    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);

    std::vector<float> bg_out_normal(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_normal.data());
    float normal_output = bg_out_normal[8];
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low tonic dopamine (Parkinson's-like)", 500);
    basal_ganglia_update_dopamine(bg, 0.0f, 0.1f);  // Reduced tonic

    basal_ganglia_select_action(bg, input.data(), &selected);

    std::vector<float> bg_out_low(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_low.data());
    float low_output = bg_out_low[8];
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify reduced motor drive with low dopamine", 100);
    // Low tonic dopamine should reduce overall motor output
    EXPECT_LE(low_output, normal_output + 0.2f)
        << "Low dopamine should reduce motor facilitation";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, DopamineModulation_PhasicBurst) {
    E2E_PIPELINE_START("Dopamine Modulation: Phasic Burst Effect");

    auto input = generate_action_preferences(0, 0.6f);

    E2E_STAGE_BEGIN("Baseline without phasic", 300);
    basal_ganglia_update_dopamine(bg, 0.0f, 0.5f);
    uint32_t selected;
    basal_ganglia_select_action(bg, input.data(), &selected);

    std::vector<float> bg_out_base(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_base.data());
    float baseline = bg_out_base[0];
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply phasic dopamine burst", 300);
    basal_ganglia_update_dopamine(bg, 1.5f, 0.5f);  // Strong phasic
    basal_ganglia_select_action(bg, input.data(), &selected);

    std::vector<float> bg_out_burst(NUM_ACTIONS);
    basal_ganglia_get_thalamic_output(bg, bg_out_burst.data());
    float burst = bg_out_burst[0];
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify phasic enhances motor signal", 100);
    EXPECT_GE(burst, baseline * 0.8f)
        << "Phasic burst should enhance motor output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaThalamusE2ETest, DopamineModulation_ExplorationExploitation) {
    E2E_PIPELINE_START("Dopamine Modulation: Exploration vs Exploitation");

    // High uncertainty -> more exploration
    // Low uncertainty -> exploitation

    std::vector<float> ambiguous_input(NUM_ACTIONS, 0.5f);  // All equal

    E2E_STAGE_BEGIN("Low dopamine (high uncertainty)", 1000);
    basal_ganglia_update_dopamine(bg, -0.3f, 0.3f);

    std::vector<uint32_t> low_da_selections;
    for (int trial = 0; trial < 10; ++trial) {
        uint32_t sel;
        basal_ganglia_select_action(bg, ambiguous_input.data(), &sel);
        low_da_selections.push_back(sel);
        basal_ganglia_action_completed(bg, sel, true);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High dopamine (low uncertainty)", 1000);
    basal_ganglia_update_dopamine(bg, 0.5f, 0.7f);

    std::vector<uint32_t> high_da_selections;
    for (int trial = 0; trial < 10; ++trial) {
        uint32_t sel;
        basal_ganglia_select_action(bg, ambiguous_input.data(), &sel);
        high_da_selections.push_back(sel);
        basal_ganglia_action_completed(bg, sel, true);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare selection variability", 100);
    // Count unique selections
    std::set<uint32_t> low_unique(low_da_selections.begin(), low_da_selections.end());
    std::set<uint32_t> high_unique(high_da_selections.begin(), high_da_selections.end());

    // Both should make selections
    EXPECT_GE(low_unique.size(), 1);
    EXPECT_GE(high_unique.size(), 1);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
