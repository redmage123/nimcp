//=============================================================================
// test_basal_ganglia_regression.cpp - Basal Ganglia Regression Tests
//=============================================================================
/**
 * @file test_basal_ganglia_regression.cpp
 * @brief Regression tests for basal ganglia bug fixes and enhancements
 *
 * WHAT: Tests for bug fixes and behavioral stability
 * WHY:  Ensure fixed bugs stay fixed, new features don't break old behavior
 * HOW:  Test specific bug scenarios and determinism of new modules
 *
 * BUG FIXES TESTED:
 * - Deadlock in basal_ganglia_action_completed() (Task #1)
 * - Stats conflict_ratio calculation bug (Task #2)
 *
 * ENHANCEMENTS TESTED:
 * - Striosome-matrix compartmentalization determinism
 * - Sequence chunking determinism
 * - Vigor/effort modulation determinism
 * - Bidirectional data flow stability
 *
 * @author NIMCP Development Team
 * @date 2026-01-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_bg_striosome_matrix.h"
#include "core/brain/subcortical/nimcp_bg_sequence_chunking.h"
#include "core/brain/subcortical/nimcp_bg_vigor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BasalGangliaRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper to check float near equality
    bool FloatNear(float a, float b, float epsilon = 0.0001f) {
        return std::fabs(a - b) < epsilon;
    }
};

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

/**
 * REGRESSION: Deadlock fix in basal_ganglia_action_completed()
 *
 * Original bug: basal_ganglia_action_completed() called
 * basal_ganglia_strengthen_habit() while holding mutex, causing deadlock.
 * Fix: Created internal _unlocked() helper function.
 *
 * This test verifies the function completes without deadlock.
 */
TEST_F(BasalGangliaRegressionTest, ActionCompletedNoDeadlock) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 8;
    config.enable_habits = true;

    basal_ganglia_t* bg = basal_ganglia_create(&config);
    ASSERT_NE(bg, nullptr);

    // Set up cortical input and process
    std::vector<float> input(8, 0.3f);
    input[2] = 0.9f;

    basal_ganglia_set_cortical_input(bg, input.data());
    basal_ganglia_set_dopamine(bg, 0.6f);
    basal_ganglia_process(bg);

    // Get selected action
    uint32_t selected = UINT32_MAX;
    float confidence = 0.0f;
    basal_ganglia_get_selected_action(bg, &selected, &confidence);

    // This call previously caused deadlock
    // It should complete within reasonable time (test timeout will catch if not)
    int result = basal_ganglia_action_completed(bg, selected, true, 100.0f);
    EXPECT_EQ(result, 0);

    // Run multiple cycles to ensure no accumulated deadlock
    for (int i = 0; i < 100; i++) {
        input[i % 8] = 0.9f;
        input[(i + 1) % 8] = 0.3f;
        basal_ganglia_set_cortical_input(bg, input.data());
        basal_ganglia_process(bg);
        basal_ganglia_get_selected_action(bg, &selected, &confidence);
        basal_ganglia_action_completed(bg, selected, true, 100.0f);
    }

    basal_ganglia_destroy(bg);
}

/**
 * REGRESSION: Stats conflict_ratio calculation fix
 *
 * Original bug: bg->stats.conflict_ratio++ (incrementing) instead of
 * calculating actual ratio.
 * Fix: Changed to proper calculation.
 *
 * This test verifies conflict_ratio is a valid ratio in [0, 1].
 */
TEST_F(BasalGangliaRegressionTest, ConflictRatioCalculation) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 8;

    basal_ganglia_t* bg = basal_ganglia_create(&config);
    ASSERT_NE(bg, nullptr);

    // Run many selections with varying conflict levels
    for (int i = 0; i < 50; i++) {
        std::vector<float> input(8, 0.4f);  // Some baseline conflict

        // Varying dominance
        if (i % 3 == 0) {
            // Clear winner (low conflict)
            input[i % 8] = 0.95f;
        } else if (i % 3 == 1) {
            // Two strong competitors (high conflict)
            input[i % 8] = 0.7f;
            input[(i + 1) % 8] = 0.68f;
        } else {
            // Medium conflict
            input[i % 8] = 0.8f;
            input[(i + 2) % 8] = 0.5f;
        }

        basal_ganglia_set_cortical_input(bg, input.data());
        basal_ganglia_set_dopamine(bg, 0.5f);
        basal_ganglia_process(bg);
    }

    // Get stats and verify conflict_ratio is valid
    basal_ganglia_stats_t stats;
    basal_ganglia_get_stats(bg, &stats);

    // Conflict ratio must be a proper ratio, not an incremented counter
    EXPECT_GE(stats.conflict_ratio, 0.0f);
    EXPECT_LE(stats.conflict_ratio, 1.0f);

    // With 50 selections, if it was incrementing, it would be much > 1
    // This also implicitly tests that it's not being incremented

    basal_ganglia_destroy(bg);
}

//=============================================================================
// Striosome-Matrix Regression Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, StriosomeMatrixDeterminism) {
    bgsm_config_t config;
    bgsm_default_config(&config);
    config.num_striosomes = 8;
    config.num_matrix_zones = 8;

    bgsm_system_t* sm1 = bgsm_create(&config);
    bgsm_system_t* sm2 = bgsm_create(&config);
    ASSERT_NE(sm1, nullptr);
    ASSERT_NE(sm2, nullptr);

    // Apply identical inputs
    std::vector<float> limbic(8, 0.6f);
    std::vector<float> motor(8, 0.5f);

    bgsm_set_striosome_input(sm1, BGSM_INPUT_LIMBIC, limbic.data());
    bgsm_set_striosome_input(sm2, BGSM_INPUT_LIMBIC, limbic.data());
    bgsm_process_striosomes(sm1);
    bgsm_process_striosomes(sm2);

    bgsm_set_matrix_input(sm1, BGSM_INPUT_MOTOR, motor.data());
    bgsm_set_matrix_input(sm2, BGSM_INPUT_MOTOR, motor.data());
    bgsm_set_matrix_dopamine(sm1, 0.6f);
    bgsm_set_matrix_dopamine(sm2, 0.6f);
    bgsm_process_matrix(sm1);
    bgsm_process_matrix(sm2);

    // Outputs must be identical
    EXPECT_FLOAT_EQ(bgsm_get_snc_modulation(sm1), bgsm_get_snc_modulation(sm2));
    EXPECT_FLOAT_EQ(bgsm_get_motivation(sm1), bgsm_get_motivation(sm2));

    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_FLOAT_EQ(bgsm_get_striosome_activation(sm1, i),
                        bgsm_get_striosome_activation(sm2, i));
        EXPECT_FLOAT_EQ(bgsm_get_d1_output(sm1, i), bgsm_get_d1_output(sm2, i));
        EXPECT_FLOAT_EQ(bgsm_get_d2_output(sm1, i), bgsm_get_d2_output(sm2, i));
    }

    bgsm_destroy(sm1);
    bgsm_destroy(sm2);
}

TEST_F(BasalGangliaRegressionTest, StriosomeMatrixOutputRange) {
    bgsm_config_t config;
    bgsm_default_config(&config);
    bgsm_system_t* sm = bgsm_create(&config);
    ASSERT_NE(sm, nullptr);

    // Test with extreme inputs
    std::vector<float> extreme_high(config.num_striosomes, 1.0f);
    std::vector<float> extreme_low(config.num_striosomes, 0.0f);

    bgsm_set_striosome_input(sm, BGSM_INPUT_LIMBIC, extreme_high.data());
    bgsm_process_striosomes(sm);

    // SNc modulation should be in valid range
    float snc = bgsm_get_snc_modulation(sm);
    EXPECT_GE(snc, -1.0f);
    EXPECT_LE(snc, 1.0f);

    // Motivation should be in [0, 1]
    float motivation = bgsm_get_motivation(sm);
    EXPECT_GE(motivation, 0.0f);
    EXPECT_LE(motivation, 1.0f);

    bgsm_destroy(sm);
}

//=============================================================================
// Sequence Chunking Regression Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, SequenceChunkingDeterminism) {
    bgsc_config_t config;
    bgsc_default_config(&config);
    config.max_chunks = 16;
    config.max_sequence_length = 8;

    bgsc_system_t* sc1 = bgsc_create(&config);
    bgsc_system_t* sc2 = bgsc_create(&config);
    ASSERT_NE(sc1, nullptr);
    ASSERT_NE(sc2, nullptr);

    // Register identical chunks
    uint32_t id1, id2;
    bgsc_register_chunk(sc1, "test_chunk", 100, &id1);
    bgsc_register_chunk(sc2, "test_chunk", 100, &id2);

    bgsc_add_action(sc1, id1, 1, 100.0f);
    bgsc_add_action(sc1, id1, 2, 100.0f);
    bgsc_add_action(sc2, id2, 1, 100.0f);
    bgsc_add_action(sc2, id2, 2, 100.0f);

    // Initiate both
    bgsc_initiate(sc1, id1);
    bgsc_initiate(sc2, id2);

    // Process with identical inputs
    bgsc_bidir_data_t data1, data2;
    memset(&data1, 0, sizeof(data1));
    memset(&data2, 0, sizeof(data2));
    data1.cortical_input = 0.7f;
    data1.dopamine_level = 0.6f;
    data2.cortical_input = 0.7f;
    data2.dopamine_level = 0.6f;

    bgsc_process_bidir(sc1, &data1);
    bgsc_process_bidir(sc2, &data2);

    // Outputs must be identical
    EXPECT_EQ(data1.requested_action, data2.requested_action);
    EXPECT_FLOAT_EQ(data1.action_urgency, data2.action_urgency);
    EXPECT_FLOAT_EQ(data1.progress_feedback, data2.progress_feedback);

    bgsc_destroy(sc1);
    bgsc_destroy(sc2);
}

TEST_F(BasalGangliaRegressionTest, SequenceChunkingAutomaticityRange) {
    bgsc_config_t config;
    bgsc_default_config(&config);
    bgsc_system_t* sc = bgsc_create(&config);
    ASSERT_NE(sc, nullptr);

    uint32_t chunk_id;
    bgsc_register_chunk(sc, "range_test", 100, &chunk_id);
    bgsc_add_action(sc, chunk_id, 1, 100.0f);

    // Initial automaticity
    float auto_initial = bgsc_get_automaticity(sc, chunk_id);
    EXPECT_GE(auto_initial, 0.0f);
    EXPECT_LE(auto_initial, 1.0f);

    // Strengthen many times
    for (int i = 0; i < 100; i++) {
        bgsc_strengthen_chunk(sc, chunk_id, 1.0f);
    }

    // Should still be in valid range
    float auto_final = bgsc_get_automaticity(sc, chunk_id);
    EXPECT_GE(auto_final, 0.0f);
    EXPECT_LE(auto_final, 1.0f);

    bgsc_destroy(sc);
}

//=============================================================================
// Vigor/Effort Regression Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, VigorDeterminism) {
    bgv_config_t config;
    bgv_default_config(&config);

    bgv_system_t* v1 = bgv_create(&config);
    bgv_system_t* v2 = bgv_create(&config);
    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);

    // Register identical actions
    bgv_register_action(v1, 1, 0.5f, 0.3f, 100.0f);
    bgv_register_action(v2, 1, 0.5f, 0.3f, 100.0f);

    // Set identical state
    bgv_set_dopamine(v1, 0.6f);
    bgv_set_dopamine(v2, 0.6f);
    bgv_set_motivation(v1, 0.7f);
    bgv_set_motivation(v2, 0.7f);
    bgv_set_fatigue(v1, 0.2f);
    bgv_set_fatigue(v2, 0.2f);

    // Compute vigor
    float vigor1, vigor2;
    bgv_compute_vigor(v1, 1, &vigor1);
    bgv_compute_vigor(v2, 1, &vigor2);

    EXPECT_FLOAT_EQ(vigor1, vigor2);

    // Test bidirectional determinism
    bgv_bidir_data_t data1, data2;
    memset(&data1, 0, sizeof(data1));
    memset(&data2, 0, sizeof(data2));

    data1.dopamine_level = 0.7f;
    data1.motivation_signal = 0.6f;
    data1.urgency_signal = 0.3f;
    data1.action_id = 1;
    data1.compute_effort = true;

    data2 = data1;  // Identical input

    bgv_process_bidir(v1, &data1);
    bgv_process_bidir(v2, &data2);

    EXPECT_FLOAT_EQ(data1.computed_vigor, data2.computed_vigor);
    EXPECT_FLOAT_EQ(data1.effort_cost, data2.effort_cost);
    EXPECT_FLOAT_EQ(data1.motor_scaling, data2.motor_scaling);

    bgv_destroy(v1);
    bgv_destroy(v2);
}

TEST_F(BasalGangliaRegressionTest, VigorOutputRange) {
    bgv_config_t config;
    bgv_default_config(&config);
    bgv_system_t* v = bgv_create(&config);
    ASSERT_NE(v, nullptr);

    bgv_register_action(v, 1, 0.5f, 0.3f, 100.0f);

    // Test extreme conditions

    // Extreme high
    bgv_set_dopamine(v, 1.0f);
    bgv_set_motivation(v, 1.0f);
    bgv_set_urgency(v, 1.0f);
    bgv_set_fatigue(v, 0.0f);

    float vigor_high;
    bgv_compute_vigor(v, 1, &vigor_high);
    EXPECT_GE(vigor_high, BGV_MIN_VIGOR);
    EXPECT_LE(vigor_high, BGV_MAX_VIGOR);

    // Extreme low
    bgv_set_dopamine(v, 0.0f);
    bgv_set_motivation(v, 0.0f);
    bgv_set_urgency(v, 0.0f);
    bgv_set_fatigue(v, 1.0f);

    float vigor_low;
    bgv_compute_vigor(v, 1, &vigor_low);
    EXPECT_GE(vigor_low, BGV_MIN_VIGOR);
    EXPECT_LE(vigor_low, BGV_MAX_VIGOR);

    bgv_destroy(v);
}

TEST_F(BasalGangliaRegressionTest, VigorFatigueClamp) {
    bgv_config_t config;
    bgv_default_config(&config);
    config.enable_fatigue = true;
    bgv_system_t* v = bgv_create(&config);
    ASSERT_NE(v, nullptr);

    // Register high-effort action
    bgv_register_action(v, 1, 0.9f, 0.8f, 100.0f);

    // Apply fatigue many times
    for (int i = 0; i < 100; i++) {
        bgv_apply_fatigue(v, 1);
    }

    // Fatigue should be clamped to 1.0
    float fatigue = bgv_get_fatigue(v);
    EXPECT_LE(fatigue, 1.0f);
    EXPECT_GE(fatigue, 0.0f);

    bgv_destroy(v);
}

//=============================================================================
// Bidirectional Data Flow Stability Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, BidirDataFlowStability) {
    // Create all systems
    bgsm_config_t sm_config;
    bgsm_default_config(&sm_config);
    bgsm_system_t* sm = bgsm_create(&sm_config);

    bgsc_config_t sc_config;
    bgsc_default_config(&sc_config);
    bgsc_system_t* sc = bgsc_create(&sc_config);

    bgv_config_t v_config;
    bgv_default_config(&v_config);
    bgv_system_t* v = bgv_create(&v_config);

    ASSERT_NE(sm, nullptr);
    ASSERT_NE(sc, nullptr);
    ASSERT_NE(v, nullptr);

    bgv_register_action(v, 0, 0.5f, 0.3f, 100.0f);

    // Run many cycles with bidirectional data
    for (int cycle = 0; cycle < 1000; cycle++) {
        // SM bidirectional
        std::vector<float> limbic(sm_config.num_striosomes, 0.5f + 0.3f * sin(cycle * 0.1f));
        bgsm_set_striosome_input(sm, BGSM_INPUT_LIMBIC, limbic.data());
        bgsm_process(sm);

        float motivation = bgsm_get_motivation(sm);
        ASSERT_GE(motivation, 0.0f);
        ASSERT_LE(motivation, 1.0f);

        // Vigor bidirectional
        bgv_bidir_data_t vdata;
        memset(&vdata, 0, sizeof(vdata));
        vdata.dopamine_level = 0.5f + 0.3f * cos(cycle * 0.1f);
        vdata.motivation_signal = motivation;
        vdata.action_id = 0;
        bgv_process_bidir(v, &vdata);

        ASSERT_GE(vdata.computed_vigor, 0.0f);
        ASSERT_LE(vdata.computed_vigor, 1.0f);
    }

    bgsm_destroy(sm);
    bgsc_destroy(sc);
    bgv_destroy(v);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, StriosomeMatrixMemorySafety) {
    // Create and destroy many times
    for (int i = 0; i < 100; i++) {
        bgsm_config_t config;
        bgsm_default_config(&config);
        bgsm_system_t* sm = bgsm_create(&config);
        ASSERT_NE(sm, nullptr);

        // Use the system
        std::vector<float> input(config.num_striosomes, 0.5f);
        bgsm_set_striosome_input(sm, BGSM_INPUT_LIMBIC, input.data());
        bgsm_process(sm);

        bgsm_destroy(sm);
    }
}

TEST_F(BasalGangliaRegressionTest, SequenceChunkingMemorySafety) {
    for (int i = 0; i < 100; i++) {
        bgsc_config_t config;
        bgsc_default_config(&config);
        bgsc_system_t* sc = bgsc_create(&config);
        ASSERT_NE(sc, nullptr);

        uint32_t chunk_id;
        bgsc_register_chunk(sc, "mem_test", 100, &chunk_id);
        bgsc_add_action(sc, chunk_id, 1, 100.0f);

        bgsc_destroy(sc);
    }
}

TEST_F(BasalGangliaRegressionTest, VigorMemorySafety) {
    for (int i = 0; i < 100; i++) {
        bgv_config_t config;
        bgv_default_config(&config);
        bgv_system_t* v = bgv_create(&config);
        ASSERT_NE(v, nullptr);

        bgv_register_action(v, 1, 0.5f, 0.3f, 100.0f);
        float vigor;
        bgv_compute_vigor(v, 1, &vigor);

        bgv_destroy(v);
    }
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, ResetRestoresInitialState) {
    bgsm_config_t sm_config;
    bgsm_default_config(&sm_config);
    bgsm_system_t* sm = bgsm_create(&sm_config);
    ASSERT_NE(sm, nullptr);

    // Get initial state
    float initial_snc = bgsm_get_snc_modulation(sm);
    float initial_motivation = bgsm_get_motivation(sm);

    // Modify state heavily
    std::vector<float> high_input(sm_config.num_striosomes, 0.9f);
    for (int i = 0; i < 100; i++) {
        bgsm_set_striosome_input(sm, BGSM_INPUT_LIMBIC, high_input.data());
        bgsm_process(sm);
    }

    // Reset
    bgsm_reset(sm);

    // State should be back to initial
    float reset_snc = bgsm_get_snc_modulation(sm);
    EXPECT_NEAR(reset_snc, initial_snc, 0.1f);

    bgsm_destroy(sm);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(BasalGangliaRegressionTest, BGAPIBackwardCompatible) {
    // Verify old API still works
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 4;

    basal_ganglia_t* bg = basal_ganglia_create(&config);
    ASSERT_NE(bg, nullptr);

    // These are the "classic" BG operations that must continue to work
    float input[4] = {0.3f, 0.9f, 0.2f, 0.4f};
    basal_ganglia_set_cortical_input(bg, input);
    basal_ganglia_set_dopamine(bg, 0.6f);
    basal_ganglia_process(bg);

    uint32_t selected;
    float confidence;
    int result = basal_ganglia_get_selected_action(bg, &selected, &confidence);
    EXPECT_EQ(result, 0);
    EXPECT_LT(selected, 4u);
    EXPECT_GE(confidence, 0.0f);

    basal_ganglia_destroy(bg);
}
