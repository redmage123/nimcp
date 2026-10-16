//=============================================================================
// test_subcortical_integration.cpp - Subcortical Integration Tests
//=============================================================================
/**
 * @file test_subcortical_integration.cpp
 * @brief Integration tests for subcortical brain structures
 *
 * WHAT: Test cross-component interactions within basal ganglia
 * WHY:  Verify biologically-realistic pathway dynamics
 * HOW:  Test direct, indirect, and hyperdirect pathway integration
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
// Direct Pathway Integration Tests
//=============================================================================

class DirectPathwayIntegrationTest : public ::testing::Test {
protected:
    striatum_t* striatum = nullptr;
    globus_pallidus_t* gpi = nullptr;
    substantia_nigra_t* snr = nullptr;

    void SetUp() override {
        striatum_config_t str_cfg;
        striatum_default_config(&str_cfg);
        str_cfg.num_actions = 4;
        striatum = striatum_create(&str_cfg);

        globus_pallidus_config_t gpi_cfg;
        globus_pallidus_default_config(&gpi_cfg, GP_SEGMENT_INTERNAL);
        gpi_cfg.num_actions = 4;
        gpi = globus_pallidus_create(&gpi_cfg);

        substantia_nigra_config_t snr_cfg;
        substantia_nigra_default_config(&snr_cfg, SN_PART_RETICULATA);
        snr_cfg.num_actions = 4;
        snr = substantia_nigra_create(&snr_cfg);
    }

    void TearDown() override {
        if (striatum) striatum_destroy(striatum);
        if (gpi) globus_pallidus_destroy(gpi);
        if (snr) substantia_nigra_destroy(snr);
    }
};

TEST_F(DirectPathwayIntegrationTest, D1PathwayInhibitsGPi) {
    // High cortical input with high dopamine should:
    // 1. Activate D1 MSNs strongly
    // 2. Inhibit GPi
    // 3. Disinhibit thalamus (allow action)

    float cortical_input[4] = {0.8f, 0.2f, 0.2f, 0.2f};
    float high_dopamine = 0.9f;

    // Process through striatum
    ASSERT_EQ(striatum_process_input(striatum, cortical_input, high_dopamine), 0);

    float d1_output[4];
    striatum_get_d1_output(striatum, d1_output);

    // D1 output should be high for action 0
    EXPECT_GT(d1_output[0], d1_output[1]);
    EXPECT_GT(d1_output[0], 0.5f);

    // Feed to GPi
    globus_pallidus_set_striatal_input(gpi, d1_output);
    globus_pallidus_process(gpi);

    float gpi_output[4];
    globus_pallidus_get_output(gpi, gpi_output);

    // GPi output should be lower for action 0 (more inhibited by D1)
    EXPECT_LT(gpi_output[0], gpi_output[1]);
}

TEST_F(DirectPathwayIntegrationTest, D1PathwayToSNr) {
    float cortical_input[4] = {0.9f, 0.1f, 0.1f, 0.1f};
    float dopamine = 0.8f;

    striatum_process_input(striatum, cortical_input, dopamine);

    float d1_output[4];
    striatum_get_d1_output(striatum, d1_output);

    // SNr receives D1 inhibition
    snr_set_striatal_input(snr, d1_output);
    snr_process(snr);

    float snr_output[4];
    snr_get_output(snr, snr_output);

    // SNr output should be low for action 0 (disinhibits thalamus)
    EXPECT_LT(snr_output[0], snr_output[1]);
}

TEST_F(DirectPathwayIntegrationTest, DopamineEnhancesDirect) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    // Low dopamine
    striatum_process_input(striatum, cortical_input, 0.2f);
    float d1_low_da[4];
    striatum_get_d1_output(striatum, d1_low_da);

    // High dopamine
    striatum_process_input(striatum, cortical_input, 0.9f);
    float d1_high_da[4];
    striatum_get_d1_output(striatum, d1_high_da);

    // D1 should be stronger with high dopamine
    for (int i = 0; i < 4; i++) {
        EXPECT_GT(d1_high_da[i], d1_low_da[i]);
    }
}

//=============================================================================
// Indirect Pathway Integration Tests
//=============================================================================

class IndirectPathwayIntegrationTest : public ::testing::Test {
protected:
    striatum_t* striatum = nullptr;
    globus_pallidus_t* gpe = nullptr;
    globus_pallidus_t* gpi = nullptr;
    subthalamic_nucleus_t* stn = nullptr;

    void SetUp() override {
        striatum_config_t str_cfg;
        striatum_default_config(&str_cfg);
        str_cfg.num_actions = 4;
        striatum = striatum_create(&str_cfg);

        globus_pallidus_config_t gpe_cfg;
        globus_pallidus_default_config(&gpe_cfg, GP_SEGMENT_EXTERNAL);
        gpe_cfg.num_actions = 4;
        gpe = globus_pallidus_create(&gpe_cfg);

        globus_pallidus_config_t gpi_cfg;
        globus_pallidus_default_config(&gpi_cfg, GP_SEGMENT_INTERNAL);
        gpi_cfg.num_actions = 4;
        gpi = globus_pallidus_create(&gpi_cfg);

        subthalamic_config_t stn_cfg;
        subthalamic_default_config(&stn_cfg);
        stn_cfg.num_actions = 4;
        stn = subthalamic_create(&stn_cfg);
    }

    void TearDown() override {
        if (striatum) striatum_destroy(striatum);
        if (gpe) globus_pallidus_destroy(gpe);
        if (gpi) globus_pallidus_destroy(gpi);
        if (stn) subthalamic_destroy(stn);
    }
};

TEST_F(IndirectPathwayIntegrationTest, D2InhibitsGPe) {
    // D2 pathway: D2 MSNs inhibit GPe, which disinhibits STN,
    // which excites GPi, inhibiting thalamus (NO-GO)

    float cortical_input[4] = {0.8f, 0.2f, 0.2f, 0.2f};
    float low_dopamine = 0.2f;  // Low DA increases D2

    striatum_process_input(striatum, cortical_input, low_dopamine);

    float d2_output[4];
    striatum_get_d2_output(striatum, d2_output);

    // D2 should be high for action 0
    EXPECT_GT(d2_output[0], 0.3f);

    // D2 inhibits GPe
    globus_pallidus_set_striatal_input(gpe, d2_output);
    globus_pallidus_process(gpe);

    float gpe_output[4];
    globus_pallidus_get_output(gpe, gpe_output);

    // GPe output should be lower for action 0
    EXPECT_LT(gpe_output[0], gpe_output[1]);
}

TEST_F(IndirectPathwayIntegrationTest, GPeToSTN) {
    // Low GPe output should disinhibit STN (increase STN activity)
    float low_gpe[4] = {0.1f, 0.5f, 0.5f, 0.5f};

    subthalamic_set_gpe_input(stn, low_gpe);
    subthalamic_process(stn);

    float stn_output[4];
    subthalamic_get_output(stn, stn_output);

    // STN should be more active for action 0 (disinhibited)
    EXPECT_GT(stn_output[0], stn_output[1]);
}

TEST_F(IndirectPathwayIntegrationTest, STNExcitesGPi) {
    float stn_input[4] = {0.8f, 0.2f, 0.2f, 0.2f};

    globus_pallidus_set_stn_input(gpi, stn_input);
    globus_pallidus_process(gpi);

    float gpi_output[4];
    globus_pallidus_get_output(gpi, gpi_output);

    // GPi should be more active for action 0 (excited by STN)
    EXPECT_GT(gpi_output[0], gpi_output[1]);
}

TEST_F(IndirectPathwayIntegrationTest, FullIndirectPathway) {
    // Full pathway: Cortex → D2 → GPe → STN → GPi
    float cortical_input[4] = {0.9f, 0.1f, 0.1f, 0.1f};
    float low_dopamine = 0.1f;

    // 1. Striatum D2
    striatum_process_input(striatum, cortical_input, low_dopamine);
    float d2_output[4];
    striatum_get_d2_output(striatum, d2_output);

    // 2. GPe inhibited by D2
    globus_pallidus_set_striatal_input(gpe, d2_output);
    globus_pallidus_process(gpe);
    float gpe_output[4];
    globus_pallidus_get_output(gpe, gpe_output);

    // 3. STN disinhibited by low GPe
    subthalamic_set_gpe_input(stn, gpe_output);
    subthalamic_process(stn);
    float stn_output[4];
    subthalamic_get_output(stn, stn_output);

    // 4. GPi excited by STN
    globus_pallidus_set_stn_input(gpi, stn_output);
    globus_pallidus_process(gpi);
    float gpi_output[4];
    globus_pallidus_get_output(gpi, gpi_output);

    // Result: GPi should be high for action 0 (blocking thalamus)
    // This is the NO-GO signal
    EXPECT_GT(gpi_output[0], 0.3f);
}

//=============================================================================
// Hyperdirect Pathway Integration Tests
//=============================================================================

class HyperdirectPathwayIntegrationTest : public ::testing::Test {
protected:
    subthalamic_nucleus_t* stn = nullptr;
    globus_pallidus_t* gpi = nullptr;
    substantia_nigra_t* snr = nullptr;

    void SetUp() override {
        subthalamic_config_t stn_cfg;
        subthalamic_default_config(&stn_cfg);
        stn_cfg.num_actions = 4;
        stn = subthalamic_create(&stn_cfg);

        globus_pallidus_config_t gpi_cfg;
        globus_pallidus_default_config(&gpi_cfg, GP_SEGMENT_INTERNAL);
        gpi_cfg.num_actions = 4;
        gpi = globus_pallidus_create(&gpi_cfg);

        substantia_nigra_config_t snr_cfg;
        substantia_nigra_default_config(&snr_cfg, SN_PART_RETICULATA);
        snr_cfg.num_actions = 4;
        snr = substantia_nigra_create(&snr_cfg);
    }

    void TearDown() override {
        if (stn) subthalamic_destroy(stn);
        if (gpi) globus_pallidus_destroy(gpi);
        if (snr) substantia_nigra_destroy(snr);
    }
};

TEST_F(HyperdirectPathwayIntegrationTest, CorticalToSTN) {
    // Hyperdirect: Cortex directly excites STN (fast global inhibition)
    float cortical_input[4] = {0.9f, 0.9f, 0.9f, 0.9f};

    subthalamic_set_cortical_input(stn, cortical_input, false);
    subthalamic_process(stn);

    // Should be in hyperdirect mode with elevated output
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_HYPERDIRECT);

    float global = subthalamic_get_global_output(stn);
    EXPECT_GT(global, 0.2f);  // Elevated above baseline
}

TEST_F(HyperdirectPathwayIntegrationTest, EmergencyStopPropagates) {
    // Emergency stop should propagate through GPi and SNr
    subthalamic_emergency_stop(stn, 1.0f);

    float stn_output[4];
    subthalamic_get_output(stn, stn_output);

    // All actions should have high STN output
    for (int i = 0; i < 4; i++) {
        EXPECT_GT(stn_output[i], 0.5f);
    }

    // Propagate to GPi
    globus_pallidus_set_stn_input(gpi, stn_output);
    globus_pallidus_process(gpi);

    float gpi_output[4];
    globus_pallidus_get_output(gpi, gpi_output);

    // GPi should have high output (blocking all actions)
    for (int i = 0; i < 4; i++) {
        EXPECT_GT(gpi_output[i], 0.4f);
    }
}

TEST_F(HyperdirectPathwayIntegrationTest, HyperdirectFasterThanDirect) {
    // Hyperdirect pathway should be faster (smaller delay) than direct
    // This is reflected in the delay configuration
    subthalamic_config_t cfg;
    subthalamic_default_config(&cfg);

    // Hyperdirect delay should be shorter than indirect
    EXPECT_LT(cfg.hyperdirect_delay_ms, cfg.indirect_delay_ms);
}

//=============================================================================
// Dopamine System Integration Tests
//=============================================================================

class DopamineIntegrationTest : public ::testing::Test {
protected:
    substantia_nigra_t* snc = nullptr;
    striatum_t* striatum = nullptr;
    basal_ganglia_t* bg = nullptr;

    void SetUp() override {
        substantia_nigra_config_t snc_cfg;
        substantia_nigra_default_config(&snc_cfg, SN_PART_COMPACTA);
        snc = substantia_nigra_create(&snc_cfg);

        striatum_config_t str_cfg;
        striatum_default_config(&str_cfg);
        str_cfg.num_actions = 4;
        striatum = striatum_create(&str_cfg);

        basal_ganglia_config_t bg_cfg;
        basal_ganglia_default_config(&bg_cfg);
        bg_cfg.num_actions = 4;
        bg = basal_ganglia_create(&bg_cfg);
    }

    void TearDown() override {
        if (snc) substantia_nigra_destroy(snc);
        if (striatum) striatum_destroy(striatum);
        if (bg) basal_ganglia_destroy(bg);
    }
};

TEST_F(DopamineIntegrationTest, PositiveRPEIncreasesDopamine) {
    float initial_da = snc_get_dopamine(snc);

    // Positive surprise: got reward when expected none
    snc_update_reward(snc, 1.0f, 0.0f);

    float new_da = snc_get_dopamine(snc);
    EXPECT_GT(new_da, initial_da);
    EXPECT_EQ(snc_get_state(snc), DA_STATE_BURST);
}

TEST_F(DopamineIntegrationTest, NegativeRPEDecreasesDopamine) {
    snc_set_prediction(snc, 0.8f);

    // Negative surprise: expected reward but got none
    snc_update_reward(snc, 0.0f, 0.8f);

    EXPECT_EQ(snc_get_state(snc), DA_STATE_PAUSE);
    EXPECT_LT(snc_get_rpe(snc), 0.0f);
}

TEST_F(DopamineIntegrationTest, DopamineModulatesActionSelection) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;

    // High dopamine - should be more likely to select actions
    basal_ganglia_set_dopamine(bg, 0.9f);
    basal_ganglia_select_action(bg, cortical_input, &selected);
    float direct_high = basal_ganglia_get_direct_activation(bg, selected);

    // Low dopamine - should be less likely to select actions
    basal_ganglia_set_dopamine(bg, 0.1f);
    basal_ganglia_select_action(bg, cortical_input, &selected);
    float direct_low = basal_ganglia_get_direct_activation(bg, selected);

    EXPECT_GT(direct_high, direct_low);
}

TEST_F(DopamineIntegrationTest, RewardLearningIntegration) {
    // Simulate learning: action followed by reward
    float cortical_input[4] = {0.8f, 0.2f, 0.2f, 0.2f};
    uint32_t selected;

    basal_ganglia_select_action(bg, cortical_input, &selected);
    EXPECT_EQ(selected, 0u);

    // Reward received - positive RPE
    basal_ganglia_update_dopamine(bg, 1.0f, 0.3f);
    float rpe = basal_ganglia_get_rpe(bg);
    EXPECT_GT(rpe, 0.0f);
}

//=============================================================================
// Full Basal Ganglia Integration Tests
//=============================================================================

class FullBGIntegrationTest : public ::testing::Test {
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

TEST_F(FullBGIntegrationTest, ActionSelectionWithCompetition) {
    // Multiple actions competing
    float cortical_input[4] = {0.4f, 0.8f, 0.3f, 0.5f};
    uint32_t selected;

    ASSERT_EQ(basal_ganglia_select_action(bg, cortical_input, &selected), 0);

    // Should select action with highest input
    EXPECT_EQ(selected, 1u);

    // Check conflict level
    float conflict = basal_ganglia_get_conflict(bg);
    EXPECT_GT(conflict, 0.0f);  // Some conflict exists
}

TEST_F(FullBGIntegrationTest, HabitFormation) {
    // Register and strengthen a habit
    uint32_t habit_id;
    ASSERT_EQ(basal_ganglia_register_habit(bg, 100, 0, &habit_id), 0);

    float initial_strength = basal_ganglia_get_habit_strength(bg, habit_id);

    // Repeatedly execute habit
    for (int i = 0; i < 50; i++) {
        basal_ganglia_strengthen_habit(bg, habit_id, true);
    }

    float final_strength = basal_ganglia_get_habit_strength(bg, habit_id);
    EXPECT_GT(final_strength, initial_strength);
}

TEST_F(FullBGIntegrationTest, ModeTransitions) {
    // Test transitions between modes
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_GOAL_DIRECTED);

    // Suppress action
    basal_ganglia_suppress_action(bg, 1.0f);
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_SUPPRESSED);

    // Reset
    basal_ganglia_reset(bg);
    EXPECT_EQ(basal_ganglia_get_mode(bg), BG_MODE_GOAL_DIRECTED);
}

TEST_F(FullBGIntegrationTest, PathwayBalance) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;

    // Balanced dopamine - D1 and D2 should be similar
    basal_ganglia_set_dopamine(bg, 0.5f);
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float direct = basal_ganglia_get_direct_activation(bg, 0);
    float indirect = basal_ganglia_get_indirect_activation(bg, 0);

    // Should be relatively balanced
    float ratio = (direct > 0.001f && indirect > 0.001f) ? direct / indirect : 1.0f;
    EXPECT_GT(ratio, 0.3f);
    EXPECT_LT(ratio, 3.0f);
}

TEST_F(FullBGIntegrationTest, ThalamicOutput) {
    float cortical_input[4] = {0.9f, 0.1f, 0.1f, 0.1f};
    uint32_t selected;

    basal_ganglia_select_action(bg, cortical_input, &selected);

    float thalamic[4];
    basal_ganglia_get_thalamic_output(bg, thalamic);

    // Thalamic output should be highest for selected action
    EXPECT_GT(thalamic[selected], thalamic[(selected + 1) % 4]);
}

TEST_F(FullBGIntegrationTest, StatisticsAccumulate) {
    float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;

    for (int i = 0; i < 10; i++) {
        basal_ganglia_select_action(bg, cortical_input, &selected);
    }

    basal_ganglia_stats_t stats;
    basal_ganglia_get_stats(bg, &stats);

    EXPECT_EQ(stats.total_selections, 10u);
}

//=============================================================================
// Cross-Component Communication Tests
//=============================================================================

class CrossComponentTest : public ::testing::Test {
protected:
    striatum_t* striatum = nullptr;
    globus_pallidus_t* gpe = nullptr;
    globus_pallidus_t* gpi = nullptr;
    subthalamic_nucleus_t* stn = nullptr;
    substantia_nigra_t* snc = nullptr;
    substantia_nigra_t* snr = nullptr;

    void SetUp() override {
        striatum_config_t str_cfg;
        striatum_default_config(&str_cfg);
        str_cfg.num_actions = 4;
        striatum = striatum_create(&str_cfg);

        globus_pallidus_config_t gpe_cfg;
        globus_pallidus_default_config(&gpe_cfg, GP_SEGMENT_EXTERNAL);
        gpe_cfg.num_actions = 4;
        gpe = globus_pallidus_create(&gpe_cfg);

        globus_pallidus_config_t gpi_cfg;
        globus_pallidus_default_config(&gpi_cfg, GP_SEGMENT_INTERNAL);
        gpi_cfg.num_actions = 4;
        gpi = globus_pallidus_create(&gpi_cfg);

        subthalamic_config_t stn_cfg;
        subthalamic_default_config(&stn_cfg);
        stn_cfg.num_actions = 4;
        stn = subthalamic_create(&stn_cfg);

        substantia_nigra_config_t snc_cfg;
        substantia_nigra_default_config(&snc_cfg, SN_PART_COMPACTA);
        snc = substantia_nigra_create(&snc_cfg);

        substantia_nigra_config_t snr_cfg;
        substantia_nigra_default_config(&snr_cfg, SN_PART_RETICULATA);
        snr_cfg.num_actions = 4;
        snr = substantia_nigra_create(&snr_cfg);
    }

    void TearDown() override {
        if (striatum) striatum_destroy(striatum);
        if (gpe) globus_pallidus_destroy(gpe);
        if (gpi) globus_pallidus_destroy(gpi);
        if (stn) subthalamic_destroy(stn);
        if (snc) substantia_nigra_destroy(snc);
        if (snr) substantia_nigra_destroy(snr);
    }
};

TEST_F(CrossComponentTest, FullCircuitProcessing) {
    // Simulate full basal ganglia circuit
    float cortical_input[4] = {0.8f, 0.3f, 0.2f, 0.1f};
    float dopamine = snc_get_dopamine(snc);

    // 1. Striatum processes cortical input with dopamine modulation
    striatum_process_input(striatum, cortical_input, dopamine);

    float d1_output[4], d2_output[4];
    striatum_get_d1_output(striatum, d1_output);
    striatum_get_d2_output(striatum, d2_output);

    // 2. D1 → GPi/SNr (direct pathway)
    globus_pallidus_set_striatal_input(gpi, d1_output);
    snr_set_striatal_input(snr, d1_output);

    // 3. D2 → GPe (indirect pathway)
    globus_pallidus_set_striatal_input(gpe, d2_output);
    globus_pallidus_process(gpe);

    float gpe_output[4];
    globus_pallidus_get_output(gpe, gpe_output);

    // 4. GPe → STN
    subthalamic_set_gpe_input(stn, gpe_output);
    subthalamic_set_cortical_input(stn, cortical_input, false);  // Hyperdirect
    subthalamic_process(stn);

    float stn_output[4];
    subthalamic_get_output(stn, stn_output);

    // 5. STN → GPi/SNr
    globus_pallidus_set_stn_input(gpi, stn_output);
    snr_set_stn_input(snr, stn_output);

    // 6. Process output nuclei
    globus_pallidus_process(gpi);
    snr_process(snr);

    float gpi_output[4], snr_output[4];
    globus_pallidus_get_output(gpi, gpi_output);
    snr_get_output(snr, snr_output);

    // 7. Compute thalamic disinhibition
    float thalamic[4];
    for (int i = 0; i < 4; i++) {
        float avg_inhibition = (gpi_output[i] + snr_output[i]) / 2.0f;
        thalamic[i] = 1.0f - avg_inhibition;
    }

    // Action 0 should win (highest cortical input)
    int winner = 0;
    for (int i = 1; i < 4; i++) {
        if (thalamic[i] > thalamic[winner]) winner = i;
    }
    EXPECT_EQ(winner, 0);
}

TEST_F(CrossComponentTest, ResetPropagates) {
    // Modify all components
    float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    striatum_process_input(striatum, input, 0.8f);
    globus_pallidus_set_striatal_input(gpe, input);
    globus_pallidus_set_striatal_input(gpi, input);
    subthalamic_emergency_stop(stn, 1.0f);
    snc_update_reward(snc, 1.0f, 0.0f);

    // Reset all
    striatum_reset(striatum);
    globus_pallidus_reset(gpe);
    globus_pallidus_reset(gpi);
    subthalamic_reset(stn);
    substantia_nigra_reset(snc);
    substantia_nigra_reset(snr);

    // All should be back to baseline
    EXPECT_EQ(subthalamic_get_mode(stn), STN_MODE_BASELINE);
    EXPECT_EQ(snc_get_state(snc), DA_STATE_TONIC);
}

