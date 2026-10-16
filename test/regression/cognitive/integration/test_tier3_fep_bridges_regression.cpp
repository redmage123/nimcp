/**
 * @file test_tier3_fep_bridges_regression.cpp
 * @brief Regression tests for Tier 3 FEP bridges stability
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests ensuring Tier 3 FEP bridges maintain consistent behavior
 * WHY:  Prevent regressions in FEP bridge behavior across code changes
 * HOW:  Test expected values, state transitions, and edge cases
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/integration/nimcp_imagination_reasoning_fep_bridge.h"
#include "cognitive/integration/nimcp_game_theory_executive_fep_bridge.h"
#include "cognitive/integration/nimcp_mirror_empathy_fep_bridge.h"
#include "cognitive/integration/nimcp_salience_attention_fep_bridge.h"
#include "cognitive/integration/nimcp_predictive_attention_fep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class Tier3FEPBridgesRegressionTest : public ::testing::Test {
protected:
    imag_reason_fep_bridge_t* ir_bridge = nullptr;
    gt_exec_fep_bridge_t* gt_bridge = nullptr;
    me_fep_bridge_t* me_bridge = nullptr;
    sa_fep_bridge_t* sa_bridge = nullptr;
    pa_fep_bridge_t* pa_bridge = nullptr;

    void SetUp() override {
        ir_bridge = imag_reason_fep_bridge_create(nullptr);
        gt_bridge = gt_exec_fep_bridge_create(nullptr);
        me_bridge = me_fep_bridge_create(nullptr);
        sa_bridge = sa_fep_bridge_create(nullptr);
        pa_bridge = pa_fep_bridge_create(nullptr);
    }

    void TearDown() override {
        if (ir_bridge) imag_reason_fep_bridge_destroy(ir_bridge);
        if (gt_bridge) gt_exec_fep_bridge_destroy(gt_bridge);
        if (me_bridge) me_fep_bridge_destroy(me_bridge);
        if (sa_bridge) sa_fep_bridge_destroy(sa_bridge);
        if (pa_bridge) pa_fep_bridge_destroy(pa_bridge);
    }
};

//=============================================================================
// Baseline Free Energy Regression Tests
//=============================================================================

/**
 * Test: BaselineFreeEnergyValues
 * Initial free energy should match expected baseline
 */
TEST_F(Tier3FEPBridgesRegressionTest, BaselineFreeEnergyValues) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    // Expected baseline values (from default configs)
    float ir_fe = imag_reason_fep_bridge_get_free_energy(ir_bridge);
    float gt_fe = gt_exec_fep_bridge_get_free_energy(gt_bridge);
    float me_fe = me_fep_bridge_get_free_energy(me_bridge);
    float sa_fe = sa_fep_bridge_get_free_energy(sa_bridge);
    float pa_fe = pa_fep_bridge_get_free_energy(pa_bridge);

    // All should be at low baseline (< 0.5 typically)
    EXPECT_LT(ir_fe, 0.5f) << "IR initial FE should be low";
    EXPECT_LT(gt_fe, 0.5f) << "GT initial FE should be low";
    EXPECT_LT(me_fe, 0.5f) << "ME initial FE should be low";
    EXPECT_LT(sa_fe, 0.5f) << "SA initial FE should be low";
    EXPECT_LT(pa_fe, 0.5f) << "PA initial FE should be low";
}

/**
 * Test: FreeEnergyMonotonicity
 * FE should increase monotonically with increasing errors
 */
TEST_F(Tier3FEPBridgesRegressionTest, FreeEnergyMonotonicity) {
    ASSERT_NE(ir_bridge, nullptr);

    std::vector<float> fe_values;

    for (float quality = 1.0f; quality >= 0.1f; quality -= 0.1f) {
        imag_reason_fep_bridge_update_scenario_quality(ir_bridge, quality);
        imag_reason_fep_bridge_force_update(ir_bridge);
        fe_values.push_back(imag_reason_fep_bridge_get_free_energy(ir_bridge));
    }

    // FE should generally increase as quality decreases
    float first_fe = fe_values.front();
    float last_fe = fe_values.back();
    EXPECT_LT(first_fe, last_fe) << "FE should increase with decreasing quality";
}

//=============================================================================
// State Transition Regression Tests
//=============================================================================

/**
 * Test: ExecutiveAlignmentTransition
 * GT-Exec bridge should transition to aligned state correctly
 */
TEST_F(Tier3FEPBridgesRegressionTest, ExecutiveAlignmentTransition) {
    ASSERT_NE(gt_bridge, nullptr);

    // Should not be aligned initially (metrics at default 1.0, but verify)
    gt_exec_fep_bridge_reset(gt_bridge);

    // Set low alignment
    gt_exec_fep_bridge_update_executive_alignment(gt_bridge, 0.5f);
    gt_exec_fep_bridge_force_update(gt_bridge);
    EXPECT_FALSE(gt_exec_fep_bridge_is_exec_aligned(gt_bridge))
        << "Should not be aligned at 0.5";

    // Set high alignment
    gt_exec_fep_bridge_update_executive_alignment(gt_bridge, 0.99f);
    gt_exec_fep_bridge_force_update(gt_bridge);
    EXPECT_TRUE(gt_exec_fep_bridge_is_exec_aligned(gt_bridge))
        << "Should be aligned at 0.99";
}

/**
 * Test: HighResonanceTransition
 * ME bridge should transition to high resonance state correctly
 */
TEST_F(Tier3FEPBridgesRegressionTest, HighResonanceTransition) {
    ASSERT_NE(me_bridge, nullptr);

    // Set high resonance deficit (low resonance)
    me_fep_bridge_update_resonance_deficit(me_bridge, 0.5f);
    me_fep_bridge_force_update(me_bridge);
    EXPECT_FALSE(me_fep_bridge_is_high_resonance(me_bridge))
        << "Should not be high resonance with 0.5 deficit";

    // Set low resonance deficit (high resonance)
    me_fep_bridge_update_resonance_deficit(me_bridge, 0.05f);
    me_fep_bridge_force_update(me_bridge);
    EXPECT_TRUE(me_fep_bridge_is_high_resonance(me_bridge))
        << "Should be high resonance with 0.05 deficit";
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

/**
 * Test: ZeroMetricsBehavior
 * Bridges should handle zero metrics correctly
 */
TEST_F(Tier3FEPBridgesRegressionTest, ZeroMetricsBehavior) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    // Set all metrics to zero (best quality/no error)
    imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 1.0f);
    imag_reason_fep_bridge_update_reasoning_coherence(ir_bridge, 1.0f);
    imag_reason_fep_bridge_force_update(ir_bridge);

    gt_exec_fep_bridge_update_decision_quality(gt_bridge, 1.0f);
    gt_exec_fep_bridge_update_executive_alignment(gt_bridge, 1.0f);
    gt_exec_fep_bridge_force_update(gt_bridge);

    me_fep_bridge_update_mirroring_error(me_bridge, 0.0f);
    me_fep_bridge_update_empathy_error(me_bridge, 0.0f);
    me_fep_bridge_update_resonance_deficit(me_bridge, 0.0f);
    me_fep_bridge_force_update(me_bridge);

    sa_fep_bridge_update_salience_error(sa_bridge, 0.0f);
    sa_fep_bridge_update_attention_efficiency(sa_bridge, 1.0f);
    sa_fep_bridge_force_update(sa_bridge);

    pa_fep_bridge_update_prediction_accuracy(pa_bridge, 1.0f);
    pa_fep_bridge_update_attention_precision(pa_bridge, 1.0f);
    pa_fep_bridge_force_update(pa_bridge);

    // All FE values should be at or near baseline
    EXPECT_LT(imag_reason_fep_bridge_get_free_energy(ir_bridge), 0.5f);
    EXPECT_LT(gt_exec_fep_bridge_get_free_energy(gt_bridge), 0.5f);
    EXPECT_LT(me_fep_bridge_get_free_energy(me_bridge), 0.5f);
    EXPECT_LT(sa_fep_bridge_get_free_energy(sa_bridge), 0.5f);
    EXPECT_LT(pa_fep_bridge_get_free_energy(pa_bridge), 0.5f);
}

/**
 * Test: MaxMetricsBehavior
 * Bridges should handle maximum error metrics correctly
 */
TEST_F(Tier3FEPBridgesRegressionTest, MaxMetricsBehavior) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);

    // Set maximum error metrics
    imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 0.0f);
    imag_reason_fep_bridge_update_reasoning_coherence(ir_bridge, 0.0f);
    imag_reason_fep_bridge_force_update(ir_bridge);

    me_fep_bridge_update_mirroring_error(me_bridge, 1.0f);
    me_fep_bridge_update_empathy_error(me_bridge, 1.0f);
    me_fep_bridge_update_resonance_deficit(me_bridge, 1.0f);
    me_fep_bridge_force_update(me_bridge);

    // FE should be elevated
    float ir_fe = imag_reason_fep_bridge_get_free_energy(ir_bridge);
    float me_fe = me_fep_bridge_get_free_energy(me_bridge);

    EXPECT_GT(ir_fe, 0.5f) << "IR FE should be elevated with max errors";
    EXPECT_GT(me_fe, 1.0f) << "ME FE should be elevated with max errors";
}

//=============================================================================
// Reset Stability Regression Tests
//=============================================================================

/**
 * Test: MultipleResetStability
 * Multiple resets should produce consistent results
 */
TEST_F(Tier3FEPBridgesRegressionTest, MultipleResetStability) {
    ASSERT_NE(ir_bridge, nullptr);

    float original_fe = imag_reason_fep_bridge_get_free_energy(ir_bridge);

    for (int i = 0; i < 10; i++) {
        // Perturb
        imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 0.2f);
        imag_reason_fep_bridge_force_update(ir_bridge);

        // Reset
        imag_reason_fep_bridge_reset(ir_bridge);

        // Verify
        float reset_fe = imag_reason_fep_bridge_get_free_energy(ir_bridge);
        EXPECT_NEAR(reset_fe, original_fe, 0.01f)
            << "Reset " << i << " should restore original FE";
    }
}

//=============================================================================
// Statistics Consistency Regression Tests
//=============================================================================

/**
 * Test: UpdateCountConsistency
 * Update counts should match number of force_update calls
 */
TEST_F(Tier3FEPBridgesRegressionTest, UpdateCountConsistency) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    constexpr int EXPECTED_UPDATES = 25;

    for (int i = 0; i < EXPECTED_UPDATES; i++) {
        imag_reason_fep_bridge_force_update(ir_bridge);
        gt_exec_fep_bridge_force_update(gt_bridge);
        me_fep_bridge_force_update(me_bridge);
        sa_fep_bridge_force_update(sa_bridge);
        pa_fep_bridge_force_update(pa_bridge);
    }

    imag_reason_fep_stats_t ir_stats;
    gt_exec_fep_stats_t gt_stats;
    me_fep_stats_t me_stats;
    sa_fep_stats_t sa_stats;
    pa_fep_stats_t pa_stats;

    imag_reason_fep_bridge_get_stats(ir_bridge, &ir_stats);
    gt_exec_fep_bridge_get_stats(gt_bridge, &gt_stats);
    me_fep_bridge_get_stats(me_bridge, &me_stats);
    sa_fep_bridge_get_stats(sa_bridge, &sa_stats);
    pa_fep_bridge_get_stats(pa_bridge, &pa_stats);

    EXPECT_EQ(ir_stats.total_updates, (uint64_t)EXPECTED_UPDATES);
    EXPECT_EQ(gt_stats.total_updates, (uint64_t)EXPECTED_UPDATES);
    EXPECT_EQ(me_stats.total_updates, (uint64_t)EXPECTED_UPDATES);
    EXPECT_EQ(sa_stats.total_updates, (uint64_t)EXPECTED_UPDATES);
    EXPECT_EQ(pa_stats.total_updates, (uint64_t)EXPECTED_UPDATES);
}

//=============================================================================
// Null Safety Regression Tests
//=============================================================================

/**
 * Test: NullBridgeSafety
 * Functions should handle NULL bridge safely
 */
TEST_F(Tier3FEPBridgesRegressionTest, NullBridgeSafety) {
    // These should not crash
    imag_reason_fep_bridge_destroy(nullptr);
    gt_exec_fep_bridge_destroy(nullptr);
    me_fep_bridge_destroy(nullptr);
    sa_fep_bridge_destroy(nullptr);
    pa_fep_bridge_destroy(nullptr);

    // Get operations should return error indicator (-1) for NULL
    float ir_fe = imag_reason_fep_bridge_get_free_energy(nullptr);
    float gt_fe = gt_exec_fep_bridge_get_free_energy(nullptr);
    float me_fe = me_fep_bridge_get_free_energy(nullptr);
    float sa_fe = sa_fep_bridge_get_free_energy(nullptr);
    float pa_fe = pa_fep_bridge_get_free_energy(nullptr);

    EXPECT_LT(ir_fe, 0.0f) << "Should return negative error indicator";
    EXPECT_LT(gt_fe, 0.0f) << "Should return negative error indicator";
    EXPECT_LT(me_fe, 0.0f) << "Should return negative error indicator";
    EXPECT_LT(sa_fe, 0.0f) << "Should return negative error indicator";
    EXPECT_LT(pa_fe, 0.0f) << "Should return negative error indicator";

    // State queries should return safe defaults
    EXPECT_FALSE(gt_exec_fep_bridge_is_exec_aligned(nullptr));
    EXPECT_FALSE(me_fep_bridge_is_high_resonance(nullptr));
    EXPECT_FALSE(gt_exec_fep_bridge_is_degraded(nullptr));
    EXPECT_FALSE(me_fep_bridge_is_degraded(nullptr));
}

/**
 * Test: NullStatsOutput
 * Stats functions should handle NULL output safely
 */
TEST_F(Tier3FEPBridgesRegressionTest, NullStatsOutput) {
    ASSERT_NE(ir_bridge, nullptr);

    // Should not crash with NULL stats output
    int result = imag_reason_fep_bridge_get_stats(ir_bridge, nullptr);
    EXPECT_NE(result, 0) << "Should return error for NULL output";

    result = gt_exec_fep_bridge_get_stats(gt_bridge, nullptr);
    EXPECT_NE(result, 0) << "Should return error for NULL output";
}
