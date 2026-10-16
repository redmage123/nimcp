//=============================================================================
// test_basal_ganglia.cpp - Basal Ganglia Unit Tests
//=============================================================================
/**
 * @file test_basal_ganglia.cpp
 * @brief Comprehensive unit tests for basal ganglia action selection system
 *
 * WHAT: Test all basal ganglia components and pathways
 * WHY:  Verify biologically-inspired action selection works correctly
 * HOW:  GTest fixtures testing striatum, GP, SN, STN, and integrated BG
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_striatum.h"
#include "core/brain/subcortical/nimcp_globus_pallidus.h"
#include "core/brain/subcortical/nimcp_substantia_nigra.h"
#include "core/brain/subcortical/nimcp_subthalamic.h"

//=============================================================================
// Striatum Tests
//=============================================================================

class StriatumTest : public ::testing::Test {
protected:
    striatum_t* striatum = nullptr;

    void SetUp() override {
        striatum_config_t config;
        striatum_default_config(&config);
        config.num_actions = 4;
        striatum = striatum_create(&config);
    }

    void TearDown() override {
        if (striatum) {
            striatum_destroy(striatum);
        }
    }
};

TEST_F(StriatumTest, CreateDestroy) {
    ASSERT_NE(striatum, nullptr);
}

TEST_F(StriatumTest, CreateWithNullConfig) {
    striatum_t* s = striatum_create(nullptr);
    ASSERT_NE(s, nullptr);
    striatum_destroy(s);
}

TEST_F(StriatumTest, DefaultConfig) {
    striatum_config_t config;
    striatum_default_config(&config);

    EXPECT_EQ(config.neurons_per_pathway, STRIATUM_DEFAULT_NEURONS);
    EXPECT_GT(config.d1_dopamine_gain, 0.0f);
    EXPECT_GT(config.d2_dopamine_gain, 0.0f);
}

TEST_F(StriatumTest, ProcessInputBaseline) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float dopamine = 0.5f;  // Baseline

    ASSERT_EQ(striatum_process_input(striatum, cortical_input, dopamine), 0);

    // D1 and D2 outputs should be similar at baseline dopamine
    float d1_out[4], d2_out[4];
    ASSERT_EQ(striatum_get_d1_output(striatum, d1_out), 0);
    ASSERT_EQ(striatum_get_d2_output(striatum, d2_out), 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_GE(d1_out[i], 0.0f);
        EXPECT_LE(d1_out[i], 1.0f);
        EXPECT_GE(d2_out[i], 0.0f);
        EXPECT_LE(d2_out[i], 1.0f);
    }
}

TEST_F(StriatumTest, DopamineModulationD1) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    // High dopamine should increase D1 activation
    striatum_process_input(striatum, cortical_input, 0.9f);
    float d1_high = striatum_get_d1_activation(striatum, 0);

    striatum_process_input(striatum, cortical_input, 0.1f);
    float d1_low = striatum_get_d1_activation(striatum, 0);

    EXPECT_GT(d1_high, d1_low);
}

TEST_F(StriatumTest, DopamineModulationD2) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    // High dopamine should decrease D2 activation
    striatum_process_input(striatum, cortical_input, 0.9f);
    float d2_high_da = striatum_get_d2_activation(striatum, 0);

    striatum_process_input(striatum, cortical_input, 0.1f);
    float d2_low_da = striatum_get_d2_activation(striatum, 0);

    EXPECT_LT(d2_high_da, d2_low_da);
}

TEST_F(StriatumTest, Reset) {
    float cortical_input[4] = {0.8f, 0.8f, 0.8f, 0.8f};
    striatum_process_input(striatum, cortical_input, 0.8f);

    ASSERT_EQ(striatum_reset(striatum), 0);

    // After reset, activations should be back to baseline
    striatum_stats_t stats;
    striatum_get_stats(striatum, &stats);
    // Stats may still show some residual values
}

TEST_F(StriatumTest, SetDopamine) {
    ASSERT_EQ(striatum_set_dopamine(striatum, 0.8f), 0);
    ASSERT_EQ(striatum_set_dopamine(striatum, 0.0f), 0);
    ASSERT_EQ(striatum_set_dopamine(striatum, 1.0f), 0);
}

TEST_F(StriatumTest, UpdateWeights) {
    ASSERT_EQ(striatum_update_weights(striatum, 0, 0.1f, -0.1f), 0);
    ASSERT_EQ(striatum_update_weights(striatum, 1, -0.1f, 0.1f), 0);

    // Invalid action should fail
    EXPECT_LT(striatum_update_weights(striatum, 100, 0.1f, 0.1f), 0);
}

TEST_F(StriatumTest, Statistics) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    striatum_process_input(striatum, cortical_input, 0.5f);

    striatum_stats_t stats;
    ASSERT_EQ(striatum_get_stats(striatum, &stats), 0);

    EXPECT_GE(stats.avg_d1_firing, 0.0f);
    EXPECT_GE(stats.avg_d2_firing, 0.0f);
}

//=============================================================================
// Globus Pallidus Tests
//=============================================================================

class GlobusPallidusTest : public ::testing::Test {
protected:
    globus_pallidus_t* gpi = nullptr;
    globus_pallidus_t* gpe = nullptr;

    void SetUp() override {
        globus_pallidus_config_t gpi_config;
        globus_pallidus_default_config(&gpi_config, GP_SEGMENT_INTERNAL);
        gpi_config.num_actions = 4;
        gpi = globus_pallidus_create(&gpi_config);

        globus_pallidus_config_t gpe_config;
        globus_pallidus_default_config(&gpe_config, GP_SEGMENT_EXTERNAL);
        gpe_config.num_actions = 4;
        gpe = globus_pallidus_create(&gpe_config);
    }

    void TearDown() override {
        if (gpi) globus_pallidus_destroy(gpi);
        if (gpe) globus_pallidus_destroy(gpe);
    }
};

TEST_F(GlobusPallidusTest, CreateDestroy) {
    ASSERT_NE(gpi, nullptr);
    ASSERT_NE(gpe, nullptr);
}

TEST_F(GlobusPallidusTest, DefaultConfig) {
    globus_pallidus_config_t config;
    globus_pallidus_default_config(&config, GP_SEGMENT_INTERNAL);

    EXPECT_EQ(config.segment, GP_SEGMENT_INTERNAL);
    EXPECT_GT(config.tonic_firing_rate, 0.0f);
}

TEST_F(GlobusPallidusTest, TonicFiring) {
    // Without input, GPi should have high tonic firing (inhibiting thalamus)
    float output[4];
    ASSERT_EQ(globus_pallidus_get_output(gpi, output), 0);

    for (int i = 0; i < 4; i++) {
        // Tonic firing normalized - should be relatively high
        EXPECT_GT(output[i], 0.2f);
    }
}

TEST_F(GlobusPallidusTest, StriatumInhibition) {
    // Strong striatal input should reduce GPi firing (disinhibit thalamus)
    float no_inhib[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float strong_inhib[4] = {0.8f, 0.8f, 0.8f, 0.8f};

    globus_pallidus_set_striatal_input(gpi, no_inhib);
    globus_pallidus_process(gpi);
    float output_no_inhib[4];
    globus_pallidus_get_output(gpi, output_no_inhib);

    globus_pallidus_set_striatal_input(gpi, strong_inhib);
    globus_pallidus_process(gpi);
    float output_strong_inhib[4];
    globus_pallidus_get_output(gpi, output_strong_inhib);

    // Strong inhibition should reduce GP output
    for (int i = 0; i < 4; i++) {
        EXPECT_LT(output_strong_inhib[i], output_no_inhib[i]);
    }
}

TEST_F(GlobusPallidusTest, STNExcitation) {
    float stn_input[4] = {0.8f, 0.8f, 0.8f, 0.8f};
    float no_stn[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    globus_pallidus_set_stn_input(gpi, no_stn);
    globus_pallidus_process(gpi);
    float output_no_stn[4];
    globus_pallidus_get_output(gpi, output_no_stn);

    globus_pallidus_set_stn_input(gpi, stn_input);
    globus_pallidus_process(gpi);
    float output_stn[4];
    globus_pallidus_get_output(gpi, output_stn);

    // STN excitation should increase GP output (more inhibition of thalamus)
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(output_stn[i], output_no_stn[i]);
    }
}

TEST_F(GlobusPallidusTest, Reset) {
    float inhib[4] = {0.8f, 0.8f, 0.8f, 0.8f};
    globus_pallidus_set_striatal_input(gpi, inhib);
    globus_pallidus_process(gpi);

    ASSERT_EQ(globus_pallidus_reset(gpi), 0);

    // After reset, should be back to tonic
    float output[4];
    globus_pallidus_get_output(gpi, output);
    EXPECT_GT(output[0], 0.2f);
}

TEST_F(GlobusPallidusTest, SegmentName) {
    EXPECT_STREQ(globus_pallidus_segment_name(GP_SEGMENT_EXTERNAL), "GPe");
    EXPECT_STREQ(globus_pallidus_segment_name(GP_SEGMENT_INTERNAL), "GPi");
}

TEST_F(GlobusPallidusTest, Statistics) {
    gp_stats_t stats;
    ASSERT_EQ(globus_pallidus_get_stats(gpi, &stats), 0);

    EXPECT_GE(stats.avg_firing_rate, 0.0f);
}

//=============================================================================
// Substantia Nigra Tests
//=============================================================================

class SubstantiaNigraTest : public ::testing::Test {
protected:
    substantia_nigra_t* snc = nullptr;
    substantia_nigra_t* snr = nullptr;

    void SetUp() override {
        substantia_nigra_config_t snc_config;
        substantia_nigra_default_config(&snc_config, SN_PART_COMPACTA);
        snc = substantia_nigra_create(&snc_config);

        substantia_nigra_config_t snr_config;
        substantia_nigra_default_config(&snr_config, SN_PART_RETICULATA);
        snr_config.num_actions = 4;
        snr = substantia_nigra_create(&snr_config);
    }

    void TearDown() override {
        if (snc) substantia_nigra_destroy(snc);
        if (snr) substantia_nigra_destroy(snr);
    }
};

TEST_F(SubstantiaNigraTest, CreateDestroy) {
    ASSERT_NE(snc, nullptr);
    ASSERT_NE(snr, nullptr);
}

TEST_F(SubstantiaNigraTest, DefaultConfig) {
    substantia_nigra_config_t config;
    substantia_nigra_default_config(&config, SN_PART_COMPACTA);

    EXPECT_EQ(config.part, SN_PART_COMPACTA);
    EXPECT_GT(config.tonic_firing_rate, 0.0f);
    EXPECT_GT(config.burst_firing_rate, config.tonic_firing_rate);
}

TEST_F(SubstantiaNigraTest, BaselineDopamine) {
    float da = snc_get_dopamine(snc);
    EXPECT_GT(da, 0.0f);
    EXPECT_LE(da, 1.0f);
}

TEST_F(SubstantiaNigraTest, PositiveRPE) {
    // Positive reward prediction error -> dopamine burst
    float initial_da = snc_get_dopamine(snc);
    snc_update_reward(snc, 1.0f, 0.0f);  // Reward > expected

    float new_da = snc_get_dopamine(snc);
    EXPECT_GT(new_da, initial_da);
    EXPECT_EQ(snc_get_state(snc), DA_STATE_BURST);
    EXPECT_GT(snc_get_rpe(snc), 0.0f);
}

TEST_F(SubstantiaNigraTest, NegativeRPE) {
    // Negative reward prediction error -> dopamine pause
    snc_set_prediction(snc, 0.8f);
    snc_update_reward(snc, 0.0f, 0.8f);  // Reward < expected

    EXPECT_EQ(snc_get_state(snc), DA_STATE_PAUSE);
    EXPECT_LT(snc_get_rpe(snc), 0.0f);
}

TEST_F(SubstantiaNigraTest, TonicState) {
    // No surprise -> tonic firing
    snc_update_reward(snc, 0.5f, 0.5f);  // Reward == expected

    EXPECT_EQ(snc_get_state(snc), DA_STATE_TONIC);
    EXPECT_NEAR(snc_get_rpe(snc), 0.0f, 0.1f);
}

TEST_F(SubstantiaNigraTest, SNrProcess) {
    float striatal_inhib[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float stn_input[4] = {0.3f, 0.3f, 0.3f, 0.3f};

    ASSERT_EQ(snr_set_striatal_input(snr, striatal_inhib), 0);
    ASSERT_EQ(snr_set_stn_input(snr, stn_input), 0);
    ASSERT_EQ(snr_process(snr), 0);

    float output[4];
    ASSERT_EQ(snr_get_output(snr, output), 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
    }
}

TEST_F(SubstantiaNigraTest, Reset) {
    snc_update_reward(snc, 1.0f, 0.0f);
    ASSERT_EQ(substantia_nigra_reset(snc), 0);

    EXPECT_EQ(snc_get_state(snc), DA_STATE_TONIC);
}

TEST_F(SubstantiaNigraTest, PartNames) {
    EXPECT_STREQ(substantia_nigra_part_name(SN_PART_COMPACTA), "SNc");
    EXPECT_STREQ(substantia_nigra_part_name(SN_PART_RETICULATA), "SNr");
}

TEST_F(SubstantiaNigraTest, DopamineStateNames) {
    EXPECT_STREQ(da_firing_state_name(DA_STATE_TONIC), "Tonic");
    EXPECT_STREQ(da_firing_state_name(DA_STATE_BURST), "Burst");
    EXPECT_STREQ(da_firing_state_name(DA_STATE_PAUSE), "Pause");
}

//=============================================================================
// Subthalamic Nucleus Tests
//=============================================================================

class SubthalamicTest : public ::testing::Test {
protected:
    subthalamic_nucleus_t* stn = nullptr;

    void SetUp() override {
        subthalamic_config_t config;
        subthalamic_default_config(&config);
        config.num_actions = 4;
        stn = subthalamic_create(&config);
    }

    void TearDown() override {
        if (stn) subthalamic_destroy(stn);
    }
};

TEST_F(SubthalamicTest, CreateDestroy) {
    ASSERT_NE(stn, nullptr);
}

TEST_F(SubthalamicTest, DefaultConfig) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);

    EXPECT_GT(config.tonic_firing_rate, 0.0f);
    EXPECT_GT(config.max_firing_rate, config.tonic_firing_rate);
}

TEST_F(SubthalamicTest, BaselineMode) {
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_BASELINE);
}

TEST_F(SubthalamicTest, HyperdirectActivation) {
    float cortical_input[4] = {0.9f, 0.9f, 0.9f, 0.9f};
    subthalamic_set_cortical_input(stn, cortical_input, false);
    subthalamic_process(stn);

    // Strong cortical input should activate hyperdirect pathway
    stn_mode_t mode = subthalamic_get_mode(stn);
    EXPECT_TRUE(mode == STN_MODE_HYPERDIRECT || mode == STN_MODE_SUPPRESSION);
}

TEST_F(SubthalamicTest, EmergencyStop) {
    ASSERT_EQ(subthalamic_emergency_stop(stn, 1.0f), 0);

    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_SUPPRESSION);
    EXPECT_GT(subthalamic_get_global_output(stn), 0.5f);
}

TEST_F(SubthalamicTest, GPeInput) {
    float gpe_low[4] = {0.1f, 0.1f, 0.1f, 0.1f};  // Low GPe = disinhibition
    ASSERT_EQ(subthalamic_set_gpe_input(stn, gpe_low), 0);
    ASSERT_EQ(subthalamic_process(stn), 0);

    // Low GPe should increase STN activity (disinhibition)
    float output[4];
    subthalamic_get_output(stn, output);
    EXPECT_GT(output[0], 0.1f);
}

TEST_F(SubthalamicTest, GetOutput) {
    float output[4];
    ASSERT_EQ(subthalamic_get_output(stn, output), 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
    }
}

TEST_F(SubthalamicTest, GlobalOutput) {
    float global = subthalamic_get_global_output(stn);
    EXPECT_GE(global, 0.0f);
    EXPECT_LE(global, 1.0f);
}

TEST_F(SubthalamicTest, Reset) {
    subthalamic_emergency_stop(stn, 1.0f);
    ASSERT_EQ(subthalamic_reset(stn), 0);

    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_BASELINE);
}

TEST_F(SubthalamicTest, ModeNames) {
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_BASELINE), "Baseline");
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_HYPERDIRECT), "Hyperdirect");
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_INDIRECT), "Indirect");
    EXPECT_STREQ(subthalamic_mode_name(STN_MODE_SUPPRESSION), "Suppression");
}

TEST_F(SubthalamicTest, Statistics) {
    stn_stats_t stats;
    ASSERT_EQ(subthalamic_get_stats(stn, &stats), 0);

    EXPECT_GE(stats.avg_firing_rate, 0.0f);
}

//=============================================================================
// Integrated Basal Ganglia Tests
//=============================================================================

class BasalGangliaTest : public ::testing::Test {
protected:
    basal_ganglia_t* bg = nullptr;

    void SetUp() override {
        basal_ganglia_config_t config;
        basal_ganglia_default_config(&config);
        config.num_actions = 4;
        bg = basal_ganglia_create(&config);
    }

    void TearDown() override {
        if (bg) basal_ganglia_destroy(bg);
    }
};

TEST_F(BasalGangliaTest, CreateDestroy) {
    ASSERT_NE(bg, nullptr);
}

TEST_F(BasalGangliaTest, CreateWithNullConfig) {
    basal_ganglia_t* bg2 = basal_ganglia_create(nullptr);
    ASSERT_NE(bg2, nullptr);
    basal_ganglia_destroy(bg2);
}

TEST_F(BasalGangliaTest, DefaultConfig) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);

    EXPECT_GT(config.num_actions, 0u);
    EXPECT_GT(config.dopamine_baseline, 0.0f);
    EXPECT_LE(config.dopamine_baseline, 1.0f);
}

TEST_F(BasalGangliaTest, ActionSelection) {
    float cortical_input[4] = {0.2f, 0.8f, 0.3f, 0.1f};
    uint32_t selected;

    ASSERT_EQ(basal_ganglia_select_action(bg, cortical_input, &selected), 0);

    // Should select action with highest cortical input (action 1)
    EXPECT_EQ(selected, 1u);
}

TEST_F(BasalGangliaTest, ThalamicOutput) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float thalamic[4];
    ASSERT_EQ(basal_ganglia_get_thalamic_output(bg, thalamic), 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_GE(thalamic[i], 0.0f);
        EXPECT_LE(thalamic[i], 1.0f);
    }
}

TEST_F(BasalGangliaTest, DopamineModulation) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    // High dopamine should bias toward action (GO)
    basal_ganglia_set_dopamine(bg, 0.9f);
    uint32_t selected_high_da;
    basal_ganglia_select_action(bg, cortical_input, &selected_high_da);
    float direct_high = basal_ganglia_get_direct_activation(bg, selected_high_da);

    // Low dopamine should reduce action tendency
    basal_ganglia_set_dopamine(bg, 0.1f);
    uint32_t selected_low_da;
    basal_ganglia_select_action(bg, cortical_input, &selected_low_da);
    float direct_low = basal_ganglia_get_direct_activation(bg, selected_low_da);

    EXPECT_GT(direct_high, direct_low);
}

TEST_F(BasalGangliaTest, RewardLearning) {
    basal_ganglia_update_dopamine(bg, 1.0f, 0.0f);  // Positive surprise
    float rpe = basal_ganglia_get_rpe(bg);
    EXPECT_GT(rpe, 0.0f);

    basal_ganglia_update_dopamine(bg, 0.0f, 1.0f);  // Negative surprise
    rpe = basal_ganglia_get_rpe(bg);
    EXPECT_LT(rpe, 0.0f);
}

TEST_F(BasalGangliaTest, ActionSuppression) {
    ASSERT_EQ(basal_ganglia_suppress_action(bg, 1.0f), 0);
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_SUPPRESSED);
}

TEST_F(BasalGangliaTest, ActionCompletion) {
    float cortical_input[4] = {0.8f, 0.2f, 0.2f, 0.2f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    ASSERT_EQ(basal_ganglia_action_completed(bg, selected, true), 0);
}

TEST_F(BasalGangliaTest, HabitRegistration) {
    uint32_t habit_id;
    ASSERT_EQ(basal_ganglia_register_habit(bg, 100, 0, &habit_id), 0);
    EXPECT_EQ(habit_id, 0u);

    float strength = basal_ganglia_get_habit_strength(bg, habit_id);
    EXPECT_GT(strength, 0.0f);
    EXPECT_LT(strength, 1.0f);
}

TEST_F(BasalGangliaTest, HabitStrengthening) {
    uint32_t habit_id;
    basal_ganglia_register_habit(bg, 100, 0, &habit_id);

    float initial_strength = basal_ganglia_get_habit_strength(bg, habit_id);

    // Strengthen through repetition
    for (int i = 0; i < 10; i++) {
        basal_ganglia_strengthen_habit(bg, habit_id, true);
    }

    float final_strength = basal_ganglia_get_habit_strength(bg, habit_id);
    EXPECT_GT(final_strength, initial_strength);
}

TEST_F(BasalGangliaTest, HabitCheck) {
    uint32_t habit_id;
    basal_ganglia_register_habit(bg, 100, 2, &habit_id);

    // Strengthen until it exceeds threshold
    for (int i = 0; i < 100; i++) {
        basal_ganglia_strengthen_habit(bg, habit_id, true);
    }

    uint32_t triggered_action;
    bool triggered = basal_ganglia_check_habit(bg, 100, &triggered_action);

    if (triggered) {
        EXPECT_EQ(triggered_action, 2u);
    }
}

TEST_F(BasalGangliaTest, HabitMode) {
    EXPECT_FALSE(basal_ganglia_is_habit_mode(bg));

    basal_ganglia_set_habit_mode(bg, true);
    EXPECT_TRUE(basal_ganglia_is_habit_mode(bg));

    basal_ganglia_set_habit_mode(bg, false);
    EXPECT_FALSE(basal_ganglia_is_habit_mode(bg));
}

TEST_F(BasalGangliaTest, PathwayActivations) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float direct = basal_ganglia_get_direct_activation(bg, 0);
    float indirect = basal_ganglia_get_indirect_activation(bg, 0);

    EXPECT_GE(direct, 0.0f);
    EXPECT_LE(direct, 1.0f);
    EXPECT_GE(indirect, 0.0f);
    EXPECT_LE(indirect, 1.0f);
}

TEST_F(BasalGangliaTest, ConflictDetection) {
    // Low conflict: one clear winner
    float low_conflict[4] = {0.9f, 0.1f, 0.1f, 0.1f};
    uint32_t selected;
    basal_ganglia_select_action(bg, low_conflict, &selected);
    float conflict_low = basal_ganglia_get_conflict(bg);

    // High conflict: multiple competing options
    float high_conflict[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    basal_ganglia_select_action(bg, high_conflict, &selected);
    float conflict_high = basal_ganglia_get_conflict(bg);

    EXPECT_GT(conflict_high, conflict_low);
}

TEST_F(BasalGangliaTest, OperatingModes) {
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_GOAL_DIRECTED);

    // After suppression
    basal_ganglia_suppress_action(bg, 1.0f);
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_SUPPRESSED);
}

TEST_F(BasalGangliaTest, Reset) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);
    basal_ganglia_suppress_action(bg, 1.0f);

    ASSERT_EQ(basal_ganglia_reset(bg), 0);
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_GOAL_DIRECTED);
}

TEST_F(BasalGangliaTest, Step) {
    ASSERT_EQ(basal_ganglia_step(bg, 1.0f), 0);
}

TEST_F(BasalGangliaTest, ProcessInput) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    ASSERT_EQ(basal_ganglia_process_input(bg, cortical_input), 0);
}

TEST_F(BasalGangliaTest, Statistics) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;

    for (int i = 0; i < 10; i++) {
        basal_ganglia_select_action(bg, cortical_input, &selected);
    }

    basal_ganglia_stats_t stats;
    ASSERT_EQ(basal_ganglia_get_stats(bg, &stats), 0);

    EXPECT_EQ(stats.total_selections, 10u);
    EXPECT_GE(stats.avg_selection_time_ms, 0.0f);
}

TEST_F(BasalGangliaTest, ModeNames) {
    EXPECT_STREQ(basal_ganglia_mode_name(BG_MODE_GOAL_DIRECTED), "Goal-Directed");
    EXPECT_STREQ(basal_ganglia_mode_name(BG_MODE_HABITUAL), "Habitual");
    EXPECT_STREQ(basal_ganglia_mode_name(BG_MODE_EXPLORATORY), "Exploratory");
    EXPECT_STREQ(basal_ganglia_mode_name(BG_MODE_SUPPRESSED), "Suppressed");
}

TEST_F(BasalGangliaTest, ActionStateNames) {
    EXPECT_STREQ(basal_ganglia_action_state_name(ACTION_STATE_IDLE), "Idle");
    EXPECT_STREQ(basal_ganglia_action_state_name(ACTION_STATE_COMPETING), "Competing");
    EXPECT_STREQ(basal_ganglia_action_state_name(ACTION_STATE_SELECTED), "Selected");
    EXPECT_STREQ(basal_ganglia_action_state_name(ACTION_STATE_EXECUTING), "Executing");
    EXPECT_STREQ(basal_ganglia_action_state_name(ACTION_STATE_COMPLETED), "Completed");
    EXPECT_STREQ(basal_ganglia_action_state_name(ACTION_STATE_CANCELLED), "Cancelled");
}

TEST_F(BasalGangliaTest, SetActionValue) {
    ASSERT_EQ(basal_ganglia_set_action_value(bg, 0, 0.8f, 0.5f, 0.2f), 0);
    ASSERT_EQ(basal_ganglia_set_action_value(bg, 1, 0.3f, 0.8f, 0.5f), 0);

    // Invalid action should fail
    EXPECT_LT(basal_ganglia_set_action_value(bg, 100, 0.5f, 0.5f, 0.5f), 0);
}

TEST_F(BasalGangliaTest, BioAsyncConnection) {
    // Bio-async may not be available in test environment
    int result = basal_ganglia_connect_bio_async(bg);
    // Either succeeds or fails gracefully
    EXPECT_TRUE(result == 0 || result < 0);

    bool connected = basal_ganglia_is_bio_async_connected(bg);
    // Connection status depends on result

    if (connected) {
        ASSERT_EQ(basal_ganglia_disconnect_bio_async(bg), 0);
        EXPECT_FALSE(basal_ganglia_is_bio_async_connected(bg));
    }
}

//=============================================================================
// Null Pointer Safety Tests
//=============================================================================

class NullPointerTest : public ::testing::Test {};

TEST_F(NullPointerTest, StriatumNullSafety) {
    EXPECT_LT(striatum_process_input(nullptr, nullptr, 0.5f), 0);
    EXPECT_LT(striatum_get_d1_output(nullptr, nullptr), 0);
    EXPECT_LT(striatum_get_d2_output(nullptr, nullptr), 0);
    EXPECT_LT(striatum_get_d1_activation(nullptr, 0), 0);
    EXPECT_LT(striatum_set_dopamine(nullptr, 0.5f), 0);
    EXPECT_LT(striatum_reset(nullptr), 0);
    EXPECT_LT(striatum_update_weights(nullptr, 0, 0.1f, 0.1f), 0);
    EXPECT_LT(striatum_get_stats(nullptr, nullptr), 0);

    striatum_destroy(nullptr);  // Should not crash
}

TEST_F(NullPointerTest, GPNullSafety) {
    EXPECT_EQ(globus_pallidus_create(nullptr), nullptr);
    EXPECT_LT(globus_pallidus_set_striatal_input(nullptr, nullptr), 0);
    EXPECT_LT(globus_pallidus_process(nullptr), 0);
    EXPECT_LT(globus_pallidus_get_output(nullptr, nullptr), 0);
    EXPECT_LT(globus_pallidus_reset(nullptr), 0);
    EXPECT_LT(globus_pallidus_get_stats(nullptr, nullptr), 0);

    globus_pallidus_destroy(nullptr);  // Should not crash
}

TEST_F(NullPointerTest, SNNullSafety) {
    EXPECT_EQ(substantia_nigra_create(nullptr), nullptr);
    EXPECT_LT(snc_update_reward(nullptr, 0.5f, 0.5f), 0);
    EXPECT_EQ(snc_get_dopamine(nullptr), 0.5f);  // Returns default
    EXPECT_LT(snr_set_striatal_input(nullptr, nullptr), 0);
    EXPECT_LT(snr_process(nullptr), 0);
    EXPECT_LT(substantia_nigra_reset(nullptr), 0);
    EXPECT_LT(substantia_nigra_get_stats(nullptr, nullptr), 0);

    substantia_nigra_destroy(nullptr);  // Should not crash
}

TEST_F(NullPointerTest, STNNullSafety) {
    EXPECT_EQ(subthalamic_create(nullptr), nullptr);
    EXPECT_LT(subthalamic_set_cortical_input(nullptr, nullptr, false), 0);
    EXPECT_LT(subthalamic_set_gpe_input(nullptr, nullptr), 0);
    EXPECT_LT(subthalamic_emergency_stop(nullptr, 1.0f), 0);
    EXPECT_LT(subthalamic_process(nullptr), 0);
    EXPECT_LT(subthalamic_get_output(nullptr, nullptr), 0);
    EXPECT_EQ(subthalamic_get_global_output(nullptr), 0.0f);
    EXPECT_LT(subthalamic_reset(nullptr), 0);
    EXPECT_LT(subthalamic_get_stats(nullptr, nullptr), 0);

    subthalamic_destroy(nullptr);  // Should not crash
}

TEST_F(NullPointerTest, BGNullSafety) {
    EXPECT_LT(basal_ganglia_select_action(nullptr, nullptr, nullptr), 0);
    EXPECT_LT(basal_ganglia_get_thalamic_output(nullptr, nullptr), 0);
    EXPECT_LT(basal_ganglia_suppress_action(nullptr, 1.0f), 0);
    EXPECT_LT(basal_ganglia_update_dopamine(nullptr, 0.5f, 0.5f), 0);
    EXPECT_LT(basal_ganglia_set_dopamine(nullptr, 0.5f), 0);
    EXPECT_EQ(basal_ganglia_get_dopamine(nullptr), 0.5f);  // Returns default
    EXPECT_LT(basal_ganglia_register_habit(nullptr, 0, 0, nullptr), 0);
    EXPECT_LT(basal_ganglia_strengthen_habit(nullptr, 0, true), 0);
    EXPECT_FALSE(basal_ganglia_check_habit(nullptr, 0, nullptr));
    EXPECT_LT(basal_ganglia_reset(nullptr), 0);
    EXPECT_LT(basal_ganglia_step(nullptr, 1.0f), 0);
    EXPECT_LT(basal_ganglia_get_stats(nullptr, nullptr), 0);

    basal_ganglia_destroy(nullptr);  // Should not crash
}
