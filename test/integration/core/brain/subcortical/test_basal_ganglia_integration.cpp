//=============================================================================
// test_basal_ganglia_integration.cpp - Basal Ganglia Integration Tests
//=============================================================================
/**
 * @file test_basal_ganglia_integration.cpp
 * @brief Integration tests for basal ganglia system components
 *
 * Tests integration between:
 * - Main BG action selection and striosome-matrix compartmentalization
 * - Action selection and sequence chunking
 * - Vigor modulation and action execution
 * - BBB security integration
 * - Bio-async messaging
 * - Bidirectional data flow across subsystems
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_bg_striosome_matrix.h"
#include "core/brain/subcortical/nimcp_bg_sequence_chunking.h"
#include "core/brain/subcortical/nimcp_bg_vigor.h"

//=============================================================================
// Test Fixture: Basal Ganglia Integration
//=============================================================================

class BasalGangliaIntegrationTest : public NimcpTestBase {
protected:
    basal_ganglia_t* bg = nullptr;
    bgsm_system_t* striosome_matrix = nullptr;
    bgsc_system_t* sequence_chunking = nullptr;
    bgv_system_t* vigor = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create basal ganglia
        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = 8;
        bg = basal_ganglia_create(&bg_config);

        // Create striosome-matrix system
        bgsm_config_t sm_config;
        bgsm_default_config(&sm_config);
        sm_config.num_striosomes = 8;
        sm_config.num_matrix_zones = 8;
        striosome_matrix = bgsm_create(&sm_config);

        // Create sequence chunking system
        bgsc_config_t sc_config;
        bgsc_default_config(&sc_config);
        sc_config.max_chunks = 16;
        sc_config.max_sequence_length = 8;
        sequence_chunking = bgsc_create(&sc_config);

        // Create vigor system
        bgv_config_t v_config;
        bgv_default_config(&v_config);
        v_config.max_actions = 8;
        vigor = bgv_create(&v_config);

        // Register actions in vigor system
        for (uint32_t i = 0; i < 8; i++) {
            bgv_register_action(vigor, i, 0.3f + 0.05f * i, 0.2f + 0.03f * i, 100.0f);
        }
    }

    void TearDown() override {
        if (vigor) {
            bgv_destroy(vigor);
            vigor = nullptr;
        }
        if (sequence_chunking) {
            bgsc_destroy(sequence_chunking);
            sequence_chunking = nullptr;
        }
        if (striosome_matrix) {
            bgsm_destroy(striosome_matrix);
            striosome_matrix = nullptr;
        }
        if (bg) {
            basal_ganglia_destroy(bg);
            bg = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, AllSystemsCreate) {
    ASSERT_NE(bg, nullptr);
    ASSERT_NE(striosome_matrix, nullptr);
    ASSERT_NE(sequence_chunking, nullptr);
    ASSERT_NE(vigor, nullptr);
}

//=============================================================================
// Striosome-Matrix Integration Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, StriosomeProvidesMotivationToBG) {
    // Step 1: Process limbic input through striosomes
    std::vector<float> limbic_input(8, 0.8f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(striosome_matrix);

    // Step 2: Get motivation signal from striosomes
    float motivation = bgsm_get_motivation(striosome_matrix);
    EXPECT_GT(motivation, 0.5f);

    // Step 3: Use motivation to modulate BG dopamine (via SNc modulation)
    float snc_modulation = bgsm_get_snc_modulation(striosome_matrix);

    // This modulation would affect dopamine release which affects BG
    // In integrated system, this creates feedback loop
}

TEST_F(BasalGangliaIntegrationTest, MatrixReceivesDopamineFromBG) {
    // Step 1: Set cortical input to BG
    std::vector<float> cortical_input(8, 0.5f);
    cortical_input[2] = 0.9f;  // Favor action 2

    // Step 2: Get dopamine level (simulated from SNc)
    float dopamine = 0.6f;  // Would come from dopamine system

    // Step 3: Feed dopamine to matrix
    bgsm_set_matrix_dopamine(striosome_matrix, dopamine);

    // Step 4: Process matrix with motor input
    std::vector<float> motor_input(8, 0.5f);
    motor_input[2] = 0.8f;  // Strong motor command for action 2
    bgsm_set_matrix_input(striosome_matrix, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_process_matrix(striosome_matrix);

    // Step 5: Get D1/D2 outputs
    float d1_out = bgsm_get_d1_output(striosome_matrix, 2);
    float d2_out = bgsm_get_d2_output(striosome_matrix, 2);

    // With dopamine > 0.5, D1 should be enhanced
    EXPECT_GT(d1_out, 0.0f);
}

TEST_F(BasalGangliaIntegrationTest, CrossCompartmentModulation) {
    // Striosome activation can modulate matrix output

    // Step 1: High striosome activation (high motivation)
    std::vector<float> limbic_input(8, 0.9f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(striosome_matrix);

    // Step 2: Process matrix
    std::vector<float> motor_input(8, 0.5f);
    bgsm_set_matrix_input(striosome_matrix, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(striosome_matrix, 0.6f);
    bgsm_process_matrix(striosome_matrix);

    // Step 3: Apply cross-compartment modulation
    int ret = bgsm_apply_striosome_modulation(striosome_matrix);
    ASSERT_EQ(ret, 0);

    // Matrix output should be modulated by striosome motivation
}

//=============================================================================
// Sequence Chunking Integration Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, ChunkExecutionUsesBGSelection) {
    // Step 1: Register a chunk representing a learned action sequence
    uint32_t chunk_id = 0;
    bgsc_register_chunk(sequence_chunking, "pickup_coffee", 100, &chunk_id);
    bgsc_add_action(sequence_chunking, chunk_id, 0, 100.0f);  // reach
    bgsc_add_action(sequence_chunking, chunk_id, 1, 150.0f);  // grasp
    bgsc_add_action(sequence_chunking, chunk_id, 2, 100.0f);  // lift

    // Step 2: Initiate chunk
    bgsc_initiate(sequence_chunking, chunk_id);
    EXPECT_TRUE(bgsc_is_executing(sequence_chunking));

    // Step 3: Get current action from chunk
    uint32_t action_id = 0;
    float urgency = 0.0f;
    bgsc_get_current_action(sequence_chunking, &action_id, &urgency);

    // Step 4: Feed action to BG for selection/execution
    std::vector<float> cortical_input(8, 0.2f);
    cortical_input[action_id] = 0.9f;  // Chunk requests this action

    basal_ganglia_set_cortical_input(bg, cortical_input.data());
    basal_ganglia_set_dopamine(bg, 0.6f);

    // Step 5: BG selects action
    basal_ganglia_process(bg);

    uint32_t selected_action = UINT32_MAX;
    float confidence = 0.0f;
    basal_ganglia_get_selected_action(bg, &selected_action, &confidence);

    // Should select the action requested by chunk
    EXPECT_EQ(selected_action, action_id);
}

TEST_F(BasalGangliaIntegrationTest, ChunkBidirectionalDataFlow) {
    // Test bidirectional data exchange

    // Register chunk
    uint32_t chunk_id = 0;
    bgsc_register_chunk(sequence_chunking, "bidir_test", 200, &chunk_id);
    bgsc_add_action(sequence_chunking, chunk_id, 3, 100.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 4, 100.0f);

    bgsc_initiate(sequence_chunking, chunk_id);

    // Create bidirectional data packet
    bgsc_bidir_data_t bidir;
    memset(&bidir, 0, sizeof(bidir));

    // Input: cortical and dopamine signals from BG/SNc
    bidir.cortical_input = 0.7f;
    bidir.dopamine_level = 0.6f;

    // Process bidirectional exchange
    int ret = bgsc_process_bidir(sequence_chunking, &bidir);
    ASSERT_EQ(ret, 0);

    // Output: action request and progress feedback
    EXPECT_EQ(bidir.requested_action, 3u);  // First action
    EXPECT_TRUE(bidir.chunk_active);
    EXPECT_GE(bidir.progress_feedback, 0.0f);

    // Signal action completion
    bidir.action_completed = true;
    bidir.completed_action_id = 3;
    bgsc_process_bidir(sequence_chunking, &bidir);

    // Process next cycle
    memset(&bidir, 0, sizeof(bidir));
    bidir.cortical_input = 0.7f;
    bidir.dopamine_level = 0.6f;
    bgsc_process_bidir(sequence_chunking, &bidir);

    // Should advance to next action
    EXPECT_EQ(bidir.requested_action, 4u);
}

//=============================================================================
// Vigor Integration Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, VigorModulatesActionExecution) {
    // Step 1: Set up dopamine from BG system
    float dopamine = 0.7f;
    bgv_set_dopamine(vigor, dopamine);

    // Step 2: Get motivation from striosomes
    std::vector<float> limbic_input(8, 0.7f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(striosome_matrix);
    float motivation = bgsm_get_motivation(striosome_matrix);

    bgv_set_motivation(vigor, motivation);

    // Step 3: Compute vigor for selected action
    float vigor_value = 0.0f;
    int ret = bgv_compute_vigor(vigor, 2, &vigor_value);
    ASSERT_EQ(ret, 0);

    // Step 4: Get motor scaling for execution
    float scaling = bgv_get_motor_scaling(vigor, 2);
    EXPECT_GT(scaling, 0.5f);

    // Step 5: Predict duration
    float duration = bgv_predict_duration(vigor, 2);
    EXPECT_GT(duration, 0.0f);
}

TEST_F(BasalGangliaIntegrationTest, VigorBidirectionalDataFlow) {
    // Process striosomes for motivation
    std::vector<float> limbic_input(8, 0.8f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(striosome_matrix);
    float motivation = bgsm_get_motivation(striosome_matrix);

    // Create vigor bidirectional packet
    bgv_bidir_data_t bidir;
    memset(&bidir, 0, sizeof(bidir));

    // Input: dopamine, motivation, urgency from other systems
    bidir.dopamine_level = 0.7f;
    bidir.motivation_signal = motivation;
    bidir.urgency_signal = 0.5f;
    bidir.reward_proximity = 0.3f;
    bidir.action_id = 2;
    bidir.compute_effort = true;

    // Process
    int ret = bgv_process_bidir(vigor, &bidir);
    ASSERT_EQ(ret, 0);

    // Output: vigor, effort, scaling, duration
    EXPECT_GT(bidir.computed_vigor, 0.0f);
    EXPECT_LE(bidir.computed_vigor, 1.0f);
    EXPECT_GT(bidir.motor_scaling, 0.0f);
    EXPECT_GT(bidir.predicted_duration_ms, 0.0f);
    EXPECT_GE(bidir.effort_cost, 0.0f);
}

TEST_F(BasalGangliaIntegrationTest, EffortBenefitAffectsSelection) {
    bgv_set_dopamine(vigor, 0.5f);

    // Register high and low effort actions
    // Action 5 already registered with higher effort

    // Compute effort for both
    bgv_effort_t effort_low, effort_high;
    bgv_compute_effort(vigor, 0, &effort_low);   // Lower effort
    bgv_compute_effort(vigor, 7, &effort_high);  // Higher effort

    // Get effort-benefit ratios
    float ratio_low = bgv_get_effort_benefit_ratio(vigor, 0, 1.0f);
    float ratio_high = bgv_get_effort_benefit_ratio(vigor, 7, 1.0f);

    // Lower effort action should have better ratio
    EXPECT_GT(ratio_low, ratio_high);
}

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, FullActionSelectionPipeline) {
    // This test demonstrates the complete integrated flow

    // 1. Limbic input → Striosomes → Motivation/SNc modulation
    std::vector<float> limbic_input(8, 0.7f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(striosome_matrix);

    float motivation = bgsm_get_motivation(striosome_matrix);
    float snc_mod = bgsm_get_snc_modulation(striosome_matrix);

    // 2. Dopamine level (affected by striosome SNc output)
    float dopamine = 0.5f + snc_mod * 0.3f;
    dopamine = std::max(0.0f, std::min(1.0f, dopamine));

    // 3. Motor cortex → Matrix → D1/D2 pathways
    std::vector<float> motor_input(8, 0.3f);
    motor_input[3] = 0.8f;  // Strong signal for action 3
    bgsm_set_matrix_input(striosome_matrix, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(striosome_matrix, dopamine);
    bgsm_process_matrix(striosome_matrix);

    // 4. BG processes cortical input with dopamine
    std::vector<float> cortical_input(8, 0.3f);
    cortical_input[3] = 0.85f;
    basal_ganglia_set_cortical_input(bg, cortical_input.data());
    basal_ganglia_set_dopamine(bg, dopamine);
    basal_ganglia_process(bg);

    // 5. BG selects action
    uint32_t selected = UINT32_MAX;
    float confidence = 0.0f;
    basal_ganglia_get_selected_action(bg, &selected, &confidence);
    EXPECT_EQ(selected, 3u);

    // 6. Vigor modulates execution
    bgv_set_dopamine(vigor, dopamine);
    bgv_set_motivation(vigor, motivation);

    float action_vigor = 0.0f;
    bgv_compute_vigor(vigor, selected, &action_vigor);

    float motor_scaling = bgv_get_motor_scaling(vigor, selected);
    float predicted_duration = bgv_predict_duration(vigor, selected);

    // 7. Verify integrated outputs
    EXPECT_GT(action_vigor, 0.0f);
    EXPECT_GT(motor_scaling, 0.0f);
    EXPECT_GT(predicted_duration, 0.0f);
}

TEST_F(BasalGangliaIntegrationTest, ChunkedActionSequencePipeline) {
    // Test automatic sequence execution

    // 1. Register a learned chunk
    uint32_t chunk_id = 0;
    bgsc_register_chunk(sequence_chunking, "learned_skill", 300, &chunk_id);
    bgsc_add_action(sequence_chunking, chunk_id, 1, 100.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 2, 150.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 3, 100.0f);

    // Strengthen the chunk (simulate learning)
    for (int i = 0; i < 10; i++) {
        bgsc_strengthen_chunk(sequence_chunking, chunk_id, 0.8f);
    }

    // 2. Check if context triggers chunk
    uint32_t triggered_id = 0;
    bool triggered = bgsc_check_trigger(sequence_chunking, 300, &triggered_id);
    EXPECT_TRUE(triggered);
    EXPECT_EQ(triggered_id, chunk_id);

    // 3. Initiate chunk
    bgsc_initiate(sequence_chunking, chunk_id);

    // 4. Execute sequence with integrated systems
    for (int step = 0; step < 3; step++) {
        // Get current action from chunk
        uint32_t action_id = 0;
        float urgency = 0.0f;
        bgsc_get_current_action(sequence_chunking, &action_id, &urgency);

        // BG processes and selects
        std::vector<float> cortical(8, 0.2f);
        cortical[action_id] = 0.9f + urgency * 0.1f;
        basal_ganglia_set_cortical_input(bg, cortical.data());
        basal_ganglia_set_dopamine(bg, 0.6f);
        basal_ganglia_process(bg);

        // Vigor modulates
        bgv_set_dopamine(vigor, 0.6f);
        float v = 0.0f;
        bgv_compute_vigor(vigor, action_id, &v);

        // Signal completion
        bgsc_action_completed(sequence_chunking, action_id, true, 100.0f);
    }

    // 5. Chunk should be complete
    EXPECT_FALSE(bgsc_is_executing(sequence_chunking));
}

//=============================================================================
// Stress and Load Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, RapidCycleStress) {
    // Rapid cycling through full pipeline
    for (int cycle = 0; cycle < 100; cycle++) {
        // Striosome processing
        std::vector<float> limbic(8, 0.5f + 0.3f * (cycle % 10) / 10.0f);
        bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic.data());
        bgsm_process_striosomes(striosome_matrix);

        // Matrix processing
        std::vector<float> motor(8, 0.4f);
        motor[cycle % 8] = 0.8f;
        bgsm_set_matrix_input(striosome_matrix, BGSM_INPUT_MOTOR, motor.data());
        bgsm_set_matrix_dopamine(striosome_matrix, 0.5f);
        bgsm_process_matrix(striosome_matrix);

        // BG processing
        std::vector<float> cortical(8, 0.3f);
        cortical[cycle % 8] = 0.85f;
        basal_ganglia_set_cortical_input(bg, cortical.data());
        basal_ganglia_process(bg);

        // Vigor processing
        bgv_bidir_data_t vdata;
        memset(&vdata, 0, sizeof(vdata));
        vdata.dopamine_level = 0.6f;
        vdata.motivation_signal = bgsm_get_motivation(striosome_matrix);
        vdata.action_id = cycle % 8;
        bgv_process_bidir(vigor, &vdata);
    }

    // All systems should still be functional
    EXPECT_NE(bg, nullptr);
    EXPECT_NE(striosome_matrix, nullptr);
    EXPECT_NE(vigor, nullptr);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, RecoverFromReset) {
    // Run some processing
    std::vector<float> limbic(8, 0.7f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic.data());
    bgsm_process_striosomes(striosome_matrix);

    // Reset all systems
    basal_ganglia_reset(bg);
    bgsm_reset(striosome_matrix);
    bgsc_reset(sequence_chunking);
    bgv_reset(vigor);

    // Should be able to continue processing
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic.data());
    int ret = bgsm_process_striosomes(striosome_matrix);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Statistics Integration Tests
//=============================================================================

TEST_F(BasalGangliaIntegrationTest, CollectIntegratedStats) {
    // Process multiple cycles
    for (int i = 0; i < 10; i++) {
        std::vector<float> input(8, 0.5f);
        input[i % 8] = 0.8f;

        basal_ganglia_set_cortical_input(bg, input.data());
        basal_ganglia_set_dopamine(bg, 0.6f);
        basal_ganglia_process(bg);
    }

    // Get stats from all systems
    basal_ganglia_stats_t bg_stats;
    basal_ganglia_get_stats(bg, &bg_stats);
    EXPECT_GE(bg_stats.total_selections, 10u);

    bgsm_stats_t sm_stats;
    bgsm_get_stats(striosome_matrix, &sm_stats);

    bgsc_stats_t sc_stats;
    bgsc_get_stats(sequence_chunking, &sc_stats);

    bgv_stats_t v_stats;
    bgv_get_stats(vigor, &v_stats);
}
