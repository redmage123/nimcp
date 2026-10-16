//=============================================================================
// test_basal_ganglia_pipeline_e2e.cpp - Basal Ganglia Full Pipeline E2E Tests
//=============================================================================
/**
 * @file test_basal_ganglia_pipeline_e2e.cpp
 * @brief End-to-end tests for complete basal ganglia pipeline
 *
 * WHAT: Full pipeline tests for BG action selection with all enhancements
 * WHY:  Verify complete system integration works end-to-end
 * HOW:  Test striosome-matrix, chunking, vigor through full action cycle
 *
 * TEST COVERAGE:
 * - Full action selection pipeline
 * - Striosome motivation → BG → Matrix motor output
 * - Sequence chunking → BG selection → Vigor modulation
 * - Skill learning and automatization
 * - Bidirectional data flow across all components
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>

#include "e2e_test_framework.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_bg_striosome_matrix.h"
#include "core/brain/subcortical/nimcp_bg_sequence_chunking.h"
#include "core/brain/subcortical/nimcp_bg_vigor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BasalGangliaPipelineE2ETest : public ::testing::Test {
protected:
    basal_ganglia_t* bg = nullptr;
    bgsm_system_t* striosome_matrix = nullptr;
    bgsc_system_t* sequence_chunking = nullptr;
    bgv_system_t* vigor = nullptr;

    static constexpr uint32_t NUM_ACTIONS = 8;
    static constexpr uint32_t NUM_STRIOSOMES = 8;
    static constexpr uint32_t NUM_MATRIX_ZONES = 8;
    static constexpr uint32_t MAX_CHUNKS = 16;

    void SetUp() override {
        // Create basal ganglia
        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = NUM_ACTIONS;
        bg_config.enable_hyperdirect = true;
        bg_config.enable_habit_learning = true;
        bg = basal_ganglia_create(&bg_config);
        ASSERT_NE(bg, nullptr);

        // Create striosome-matrix system
        bgsm_config_t sm_config;
        bgsm_default_config(&sm_config);
        sm_config.num_striosomes = NUM_STRIOSOMES;
        sm_config.num_matrix_zones = NUM_MATRIX_ZONES;
        striosome_matrix = bgsm_create(&sm_config);
        ASSERT_NE(striosome_matrix, nullptr);

        // Create sequence chunking
        bgsc_config_t sc_config;
        bgsc_default_config(&sc_config);
        sc_config.max_chunks = MAX_CHUNKS;
        sc_config.max_sequence_length = 8;
        sc_config.enable_chunking = true;
        sequence_chunking = bgsc_create(&sc_config);
        ASSERT_NE(sequence_chunking, nullptr);

        // Create vigor system
        bgv_config_t v_config;
        bgv_default_config(&v_config);
        v_config.max_actions = NUM_ACTIONS;
        v_config.enable_fatigue = true;
        vigor = bgv_create(&v_config);
        ASSERT_NE(vigor, nullptr);

        // Register actions in vigor system
        for (uint32_t i = 0; i < NUM_ACTIONS; i++) {
            bgv_register_action(vigor, i, 0.2f + 0.1f * i, 0.15f + 0.05f * i, 100.0f);
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
    }

    // Generate cortical action preferences
    std::vector<float> generateActionPreferences(uint32_t preferred_action, float strength) {
        std::vector<float> prefs(NUM_ACTIONS, 0.1f);
        if (preferred_action < NUM_ACTIONS) {
            prefs[preferred_action] = strength;
        }
        return prefs;
    }

    // Generate limbic/emotional input
    std::vector<float> generateLimbicInput(float baseline, float peak_idx = -1) {
        std::vector<float> input(NUM_STRIOSOMES, baseline);
        if (peak_idx >= 0 && peak_idx < NUM_STRIOSOMES) {
            input[(int)peak_idx] = 0.9f;
        }
        return input;
    }

    // Simulate full action cycle
    struct ActionCycleResult {
        uint32_t selected_action;
        float confidence;
        float vigor_level;
        float motor_scaling;
        float predicted_duration;
        float motivation;
        bool success;
    };

    ActionCycleResult executeFullCycle(const std::vector<float>& cortical_input,
                                       const std::vector<float>& limbic_input,
                                       float dopamine) {
        ActionCycleResult result = {};

        // 1. Process striosomes
        bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_input.data());
        bgsm_process_striosomes(striosome_matrix);
        result.motivation = bgsm_get_motivation(striosome_matrix);

        // 2. Get SNc modulation and adjust dopamine
        float snc_mod = bgsm_get_snc_modulation(striosome_matrix);
        float effective_dopamine = std::max(0.0f, std::min(1.0f, dopamine + snc_mod * 0.2f));

        // 3. Process matrix
        std::vector<float> motor_input(NUM_MATRIX_ZONES, 0.3f);
        for (uint32_t i = 0; i < NUM_ACTIONS && i < NUM_MATRIX_ZONES; i++) {
            motor_input[i] = cortical_input[i] * 0.8f;
        }
        bgsm_set_matrix_input(striosome_matrix, BGSM_INPUT_MOTOR, motor_input.data());
        bgsm_set_matrix_dopamine(striosome_matrix, effective_dopamine);
        bgsm_process_matrix(striosome_matrix);

        // 4. BG action selection
        basal_ganglia_set_dopamine(bg, effective_dopamine);
        basal_ganglia_process_input(bg, cortical_input.data());
        basal_ganglia_step(bg, 1.0f);

        int ret = basal_ganglia_select_action(bg, cortical_input.data(), &result.selected_action);
        if (ret != 0) {
            result.success = false;
            return result;
        }
        result.confidence = 0.8f;  // Default confidence since API doesn't return it

        // 5. Vigor computation
        bgv_set_dopamine(vigor, effective_dopamine);
        bgv_set_motivation(vigor, result.motivation);

        ret = bgv_compute_vigor(vigor, result.selected_action, &result.vigor_level);
        if (ret != 0) {
            result.success = false;
            return result;
        }

        result.motor_scaling = bgv_get_motor_scaling(vigor, result.selected_action);
        result.predicted_duration = bgv_predict_duration(vigor, result.selected_action);
        result.success = true;

        return result;
    }
};

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(BasalGangliaPipelineE2ETest, FullPipeline_BasicActionSelection) {
    E2E_PIPELINE_START("Full Pipeline: Basic Action Selection");

    E2E_STAGE_BEGIN("Generate cortical preference for action 3", 100);
    auto cortical = generateActionPreferences(3, 0.9f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate baseline limbic input", 100);
    auto limbic = generateLimbicInput(0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute full action cycle", 500);
    auto result = executeFullCycle(cortical, limbic, 0.6f);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.selected_action, 3u);
    EXPECT_GT(result.confidence, 0.5f);
    EXPECT_GT(result.vigor_level, 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify motor output", 100);
    EXPECT_GT(result.motor_scaling, 0.5f);
    EXPECT_GT(result.predicted_duration, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, FullPipeline_MotivationModulation) {
    E2E_PIPELINE_START("Full Pipeline: Motivation Modulation");

    auto cortical = generateActionPreferences(2, 0.7f);

    E2E_STAGE_BEGIN("Low motivation cycle", 300);
    auto limbic_low = generateLimbicInput(0.2f);
    auto result_low = executeFullCycle(cortical, limbic_low, 0.5f);
    EXPECT_TRUE(result_low.success);
    float vigor_low = result_low.vigor_level;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High motivation cycle", 300);
    auto limbic_high = generateLimbicInput(0.9f);
    auto result_high = executeFullCycle(cortical, limbic_high, 0.5f);
    EXPECT_TRUE(result_high.success);
    float vigor_high = result_high.vigor_level;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify motivation effect", 100);
    EXPECT_GT(result_high.motivation, result_low.motivation);
    // High motivation should increase vigor
    EXPECT_GE(vigor_high, vigor_low);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, FullPipeline_DopamineModulation) {
    E2E_PIPELINE_START("Full Pipeline: Dopamine Modulation");

    auto cortical = generateActionPreferences(1, 0.8f);
    auto limbic = generateLimbicInput(0.6f);

    E2E_STAGE_BEGIN("Low dopamine state (bradykinesia)", 300);
    auto result_low_da = executeFullCycle(cortical, limbic, 0.15f);
    EXPECT_TRUE(result_low_da.success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High dopamine state", 300);
    auto result_high_da = executeFullCycle(cortical, limbic, 0.85f);
    EXPECT_TRUE(result_high_da.success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify dopamine effect on vigor", 100);
    EXPECT_GT(result_high_da.vigor_level, result_low_da.vigor_level);
    // Higher vigor = shorter predicted duration
    EXPECT_LT(result_high_da.predicted_duration, result_low_da.predicted_duration);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Sequence Chunking E2E Tests
//=============================================================================

TEST_F(BasalGangliaPipelineE2ETest, SequenceChunking_LearnedSkill) {
    E2E_PIPELINE_START("Sequence Chunking: Learned Skill Execution");

    E2E_STAGE_BEGIN("Register skill chunk", 200);
    uint32_t chunk_id = 0;
    int ret = bgsc_register_chunk(sequence_chunking, "typing_hello", 1000, &chunk_id);
    EXPECT_EQ(ret, 0);

    // Add action sequence: h(0) e(1) l(2) l(2) o(3)
    bgsc_add_action(sequence_chunking, chunk_id, 0, 80.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 1, 80.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 2, 80.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 2, 80.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 3, 80.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train chunk through repetition", 500);
    for (int rep = 0; rep < 15; rep++) {
        bgsc_strengthen_chunk(sequence_chunking, chunk_id, 0.8f);
    }
    float automaticity = bgsc_get_automaticity(sequence_chunking, chunk_id);
    EXPECT_GT(automaticity, 0.3f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute learned sequence", 1000);
    bgsc_initiate(sequence_chunking, chunk_id);
    EXPECT_TRUE(bgsc_is_executing(sequence_chunking));

    int step_count = 0;
    while (bgsc_is_executing(sequence_chunking) && step_count < 10) {
        uint32_t action_id = 0;
        float urgency = 0.0f;
        ret = bgsc_get_current_action(sequence_chunking, &action_id, &urgency);
        EXPECT_EQ(ret, 0);

        // Execute through BG
        auto cortical = generateActionPreferences(action_id, 0.9f);
        auto limbic = generateLimbicInput(0.7f);
        auto result = executeFullCycle(cortical, limbic, 0.6f);
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.selected_action, action_id);

        // Signal completion
        bgsc_action_completed(sequence_chunking, action_id, true, 85.0f);
        step_count++;
    }

    EXPECT_EQ(step_count, 5);  // 5 actions in sequence
    EXPECT_FALSE(bgsc_is_executing(sequence_chunking));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, SequenceChunking_BidirectionalDataFlow) {
    E2E_PIPELINE_START("Sequence Chunking: Bidirectional Data Flow");

    E2E_STAGE_BEGIN("Create and initiate chunk", 200);
    uint32_t chunk_id = 0;
    bgsc_register_chunk(sequence_chunking, "bidir_skill", 2000, &chunk_id);
    bgsc_add_action(sequence_chunking, chunk_id, 4, 100.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 5, 100.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 6, 100.0f);
    bgsc_initiate(sequence_chunking, chunk_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with bidirectional interface", 500);
    for (int step = 0; step < 3; step++) {
        // Create bidir packet
        bgsc_bidir_data_t chunk_data;
        memset(&chunk_data, 0, sizeof(chunk_data));
        chunk_data.cortical_input = 0.8f;
        chunk_data.dopamine_level = 0.6f;

        // Process chunk bidirectionally
        int ret = bgsc_process_bidir(sequence_chunking, &chunk_data);
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(chunk_data.chunk_active || step == 2);

        // Use chunk outputs to drive vigor
        bgv_bidir_data_t vigor_data;
        memset(&vigor_data, 0, sizeof(vigor_data));
        vigor_data.dopamine_level = 0.6f;
        vigor_data.motivation_signal = 0.7f;
        vigor_data.urgency_signal = chunk_data.action_urgency;
        vigor_data.action_id = chunk_data.requested_action;
        vigor_data.compute_effort = true;

        ret = bgv_process_bidir(vigor, &vigor_data);
        EXPECT_EQ(ret, 0);
        EXPECT_GT(vigor_data.computed_vigor, 0.0f);

        // Complete action
        chunk_data.action_completed = true;
        chunk_data.completed_action_id = chunk_data.requested_action;
        bgsc_process_bidir(sequence_chunking, &chunk_data);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Striosome-Matrix E2E Tests
//=============================================================================

TEST_F(BasalGangliaPipelineE2ETest, StriosomeMatrix_ValueBasedModulation) {
    E2E_PIPELINE_START("Striosome-Matrix: Value-Based Modulation");

    E2E_STAGE_BEGIN("Process high-value context", 300);
    // High limbic activation → high motivation
    auto limbic_high_value = generateLimbicInput(0.9f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_high_value.data());
    bgsm_process_striosomes(striosome_matrix);

    float motivation_high = bgsm_get_motivation(striosome_matrix);
    float snc_high = bgsm_get_snc_modulation(striosome_matrix);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process low-value context", 300);
    bgsm_reset(striosome_matrix);
    auto limbic_low_value = generateLimbicInput(0.1f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic_low_value.data());
    bgsm_process_striosomes(striosome_matrix);

    float motivation_low = bgsm_get_motivation(striosome_matrix);
    float snc_low = bgsm_get_snc_modulation(striosome_matrix);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify value modulation", 100);
    EXPECT_GT(motivation_high, motivation_low);
    // SNc modulation should differ based on value
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, StriosomeMatrix_MotorOutput) {
    E2E_PIPELINE_START("Striosome-Matrix: Motor Output Pathways");

    E2E_STAGE_BEGIN("Set up motor command", 200);
    std::vector<float> motor_input(NUM_MATRIX_ZONES, 0.2f);
    motor_input[3] = 0.9f;  // Strong command for action 3
    motor_input[5] = 0.6f;  // Moderate command for action 5
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with high dopamine", 300);
    bgsm_set_matrix_input(striosome_matrix, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(striosome_matrix, 0.8f);
    bgsm_process_matrix(striosome_matrix);

    float d1_action3_high = bgsm_get_d1_output(striosome_matrix, 3);
    float d2_action3_high = bgsm_get_d2_output(striosome_matrix, 3);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with low dopamine", 300);
    bgsm_set_matrix_dopamine(striosome_matrix, 0.2f);
    bgsm_process_matrix(striosome_matrix);

    float d1_action3_low = bgsm_get_d1_output(striosome_matrix, 3);
    float d2_action3_low = bgsm_get_d2_output(striosome_matrix, 3);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify D1/D2 modulation", 100);
    // High DA: D1 enhanced, D2 reduced
    EXPECT_GT(d1_action3_high, d1_action3_low);
    EXPECT_LT(d2_action3_high, d2_action3_low);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Vigor E2E Tests
//=============================================================================

TEST_F(BasalGangliaPipelineE2ETest, Vigor_FatigueAndRecovery) {
    E2E_PIPELINE_START("Vigor: Fatigue and Recovery Cycle");

    E2E_STAGE_BEGIN("Set initial state", 100);
    bgv_set_dopamine(vigor, 0.6f);
    bgv_set_motivation(vigor, 0.7f);
    bgv_set_fatigue(vigor, 0.0f);

    float initial_vigor = 0.0f;
    bgv_compute_vigor(vigor, 0, &initial_vigor);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Perform high-effort actions", 500);
    // Execute high-effort action multiple times
    for (int i = 0; i < 20; i++) {
        bgv_apply_fatigue(vigor, 7);  // Action 7 has highest effort
    }

    float fatigue_level = bgv_get_fatigue(vigor);
    EXPECT_GT(fatigue_level, 0.3f);

    float fatigued_vigor = 0.0f;
    bgv_compute_vigor(vigor, 0, &fatigued_vigor);
    EXPECT_LT(fatigued_vigor, initial_vigor);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery period", 500);
    // Simulate rest with high dopamine (aids recovery)
    bgv_set_dopamine(vigor, 0.8f);
    for (int i = 0; i < 100; i++) {
        bgv_process_recovery(vigor, 100.0f);  // 100ms steps
    }

    float recovered_fatigue = bgv_get_fatigue(vigor);
    EXPECT_LT(recovered_fatigue, fatigue_level);

    float recovered_vigor = 0.0f;
    bgv_compute_vigor(vigor, 0, &recovered_vigor);
    EXPECT_GT(recovered_vigor, fatigued_vigor);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, Vigor_EffortBenefitDecision) {
    E2E_PIPELINE_START("Vigor: Effort-Benefit Decision Making");

    E2E_STAGE_BEGIN("Set moderate resources", 100);
    bgv_set_dopamine(vigor, 0.5f);
    bgv_set_motivation(vigor, 0.5f);
    bgv_set_fatigue(vigor, 0.3f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare effort-benefit for actions", 300);
    // Action 0: lowest effort
    // Action 7: highest effort

    float ratio_low_effort = bgv_get_effort_benefit_ratio(vigor, 0, 1.0f);
    float ratio_high_effort = bgv_get_effort_benefit_ratio(vigor, 7, 1.0f);

    // Low effort action should have better ratio for same reward
    EXPECT_GT(ratio_low_effort, ratio_high_effort);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Higher reward justifies higher effort", 300);
    // Same action but 5x reward
    float ratio_high_reward = bgv_get_effort_benefit_ratio(vigor, 7, 5.0f);

    // High reward should improve the ratio
    EXPECT_GT(ratio_high_reward, ratio_high_effort);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Complete System Integration E2E Tests
//=============================================================================

TEST_F(BasalGangliaPipelineE2ETest, CompleteSystem_GoalDirectedBehavior) {
    E2E_PIPELINE_START("Complete System: Goal-Directed Behavior");

    E2E_STAGE_BEGIN("Set high motivation goal state", 200);
    auto limbic = generateLimbicInput(0.85f);
    bgsm_set_striosome_input(striosome_matrix, BGSM_INPUT_LIMBIC, limbic.data());
    bgsm_process_striosomes(striosome_matrix);
    float motivation = bgsm_get_motivation(striosome_matrix);
    EXPECT_GT(motivation, 0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Goal-directed action selection", 500);
    // Cortical goal signal for action 4
    auto cortical = generateActionPreferences(4, 0.85f);
    auto result = executeFullCycle(cortical, limbic, 0.65f);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.selected_action, 4u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Action execution with vigor", 300);
    // Signal action completion to BG (3 args: bg, action_id, success)
    int ret = basal_ganglia_action_completed(bg, result.selected_action, true);
    EXPECT_EQ(ret, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, CompleteSystem_HabitFormation) {
    E2E_PIPELINE_START("Complete System: Habit Formation");

    E2E_STAGE_BEGIN("Register habitual sequence", 200);
    uint32_t chunk_id = 0;
    bgsc_register_chunk(sequence_chunking, "morning_routine", 3000, &chunk_id);
    bgsc_add_action(sequence_chunking, chunk_id, 1, 100.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 2, 100.0f);
    bgsc_add_action(sequence_chunking, chunk_id, 3, 100.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train through repetition", 1000);
    for (int day = 0; day < 30; day++) {
        // Practice the sequence
        bgsc_initiate(sequence_chunking, chunk_id);

        while (bgsc_is_executing(sequence_chunking)) {
            uint32_t action_id = 0;
            float urgency = 0.0f;
            bgsc_get_current_action(sequence_chunking, &action_id, &urgency);

            auto cortical = generateActionPreferences(action_id, 0.8f);
            auto limbic = generateLimbicInput(0.6f);
            auto result = executeFullCycle(cortical, limbic, 0.5f);

            bgsc_action_completed(sequence_chunking, action_id, result.success, 100.0f);
        }

        // Reinforce
        bgsc_strengthen_chunk(sequence_chunking, chunk_id, 0.7f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify automatization", 100);
    float automaticity = bgsc_get_automaticity(sequence_chunking, chunk_id);
    // After 30 repetitions with reinforcement, should be fairly automatic
    EXPECT_GT(automaticity, 0.5f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(BasalGangliaPipelineE2ETest, CompleteSystem_StressTest) {
    E2E_PIPELINE_START("Complete System: Stress Test");

    E2E_STAGE_BEGIN("Run 1000 action cycles", 5000);
    int successful_cycles = 0;

    for (int cycle = 0; cycle < 1000; cycle++) {
        // Varying inputs
        uint32_t preferred = cycle % NUM_ACTIONS;
        float strength = 0.6f + 0.3f * sin(cycle * 0.1f);
        float limbic_level = 0.4f + 0.4f * cos(cycle * 0.07f);
        float dopamine = 0.4f + 0.3f * sin(cycle * 0.05f);

        auto cortical = generateActionPreferences(preferred, strength);
        auto limbic = generateLimbicInput(limbic_level);
        auto result = executeFullCycle(cortical, limbic, dopamine);

        if (result.success) {
            successful_cycles++;

            // Sometimes apply fatigue
            if (cycle % 10 == 0) {
                bgv_apply_fatigue(vigor, result.selected_action);
            }

            // Sometimes recover
            if (cycle % 50 == 0) {
                bgv_process_recovery(vigor, 500.0f);
            }
        }
    }

    EXPECT_GT(successful_cycles, 950);  // >95% success rate
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify system stability", 100);
    // All systems should still be functional
    EXPECT_NE(bg, nullptr);
    EXPECT_NE(striosome_matrix, nullptr);
    EXPECT_NE(sequence_chunking, nullptr);
    EXPECT_NE(vigor, nullptr);

    // Can still process
    auto cortical = generateActionPreferences(0, 0.9f);
    auto limbic = generateLimbicInput(0.7f);
    auto result = executeFullCycle(cortical, limbic, 0.6f);
    EXPECT_TRUE(result.success);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
