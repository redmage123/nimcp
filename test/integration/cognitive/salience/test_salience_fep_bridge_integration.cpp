/**
 * @file test_salience_fep_bridge_integration.cpp
 * @brief Integration tests for salience-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Integration tests for bidirectional FEP-salience coupling
 * WHY:  Verify that salience and FEP systems work together correctly
 * HOW:  Test realistic scenarios with actual FEP prediction errors and salience computation
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/salience/nimcp_salience_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/salience/nimcp_salience.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SalienceFEPBridgeIntegrationTest : public ::testing::Test {
protected:
    salience_fep_bridge_t* bridge;
    fep_system_t* fep;
    brain_t brain;
    salience_evaluator_t salience;

    void SetUp() override {
        bridge = nullptr;
        fep = nullptr;
        brain = nullptr;
        salience = nullptr;

        // Create FEP system
        fep_config_t fep_cfg;
        fep_default_config(&fep_cfg);

        fep = fep_create(&fep_cfg, 8, 4);  // observation_dim=8, action_dim=4
        ASSERT_NE(fep, nullptr);

        // Create brain for salience evaluator
        brain = brain_create("salience_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CUSTOM, 10, 3);
        ASSERT_NE(brain, nullptr);

        // Create salience evaluator
        salience_config_t sal_cfg = salience_default_config();
        sal_cfg.enable_novelty = true;
        sal_cfg.enable_surprise = true;
        sal_cfg.enable_urgency = true;

        salience = salience_evaluator_create(brain, &sal_cfg);
        ASSERT_NE(salience, nullptr);

        // Create bridge
        salience_fep_config_t bridge_cfg;
        salience_fep_bridge_default_config(&bridge_cfg);

        bridge = salience_fep_bridge_create(&bridge_cfg);
        ASSERT_NE(bridge, nullptr);

        // Connect components
        salience_fep_bridge_connect_fep(bridge, fep);
        salience_fep_bridge_connect_salience(bridge, salience);
    }

    void TearDown() override {
        if (bridge) {
            salience_fep_bridge_destroy(bridge);
        }
        if (salience) {
            salience_evaluator_destroy(salience);
        }
        if (brain) {
            brain_destroy(brain);
        }
        if (fep) {
            fep_destroy(fep);
        }
    }
};

/* ============================================================================
 * Integration Test: High Prediction Error → High Salience → Precision Boost
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, HighPredictionErrorBoostsPrecision) {
    // Scenario: FEP has high prediction error → Should compute high salience →
    // Should boost precision for salient stimuli

    // 1. Set high prediction errors in FEP system
    fep->levels[0].errors.magnitude = 8.0f;
    fep->levels[1].errors.magnitude = 6.0f;

    // Set reasonable precision values
    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 4.0f;
    }
    for (uint32_t i = 0; i < fep->levels[1].errors.dim; i++) {
        fep->levels[1].errors.precision[i] = 4.0f;
    }

    // 2. Compute salience from prediction error
    int result = salience_fep_compute_salience_from_pe(bridge);
    ASSERT_EQ(result, 0);

    // Verify high salience was computed
    salience_fep_state_t state;
    salience_fep_bridge_get_state(bridge, &state);
    EXPECT_GT(state.current_salience, 0.5f) << "High PE should produce high salience";

    // 3. Modulate precision by salience
    result = salience_fep_modulate_precision_by_salience(bridge);
    ASSERT_EQ(result, 0);

    // Verify precision boost was applied
    EXPECT_GT(state.current_precision_boost, 1.0f) << "High salience should boost precision";

    // 4. Verify statistics
    salience_fep_stats_t stats;
    salience_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_precision_boosts, 0);
    EXPECT_GT(stats.avg_salience, 0.0f);
}

/* ============================================================================
 * Integration Test: Low Prediction Error → Low Salience → No Boost
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, LowPredictionErrorNoBoost) {
    // Scenario: FEP has low prediction error → Low salience → No precision boost

    // 1. Set low prediction errors
    fep->levels[0].errors.magnitude = 0.5f;
    fep->levels[1].errors.magnitude = 0.3f;

    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 1.0f;
    }

    // 2. Compute salience from PE
    salience_fep_compute_salience_from_pe(bridge);

    // Verify low salience
    salience_fep_state_t state;
    salience_fep_bridge_get_state(bridge, &state);
    EXPECT_LT(state.current_salience, 0.3f) << "Low PE should produce low salience";

    // 3. Modulate precision
    salience_fep_modulate_precision_by_salience(bridge);

    // Verify minimal or no boost
    EXPECT_LE(state.current_precision_boost, 1.1f) << "Low salience should not boost precision";
}

/* ============================================================================
 * Integration Test: Surprise-Based Attention Shift
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, SurpriseTriggersAttentionShift) {
    // Scenario: Sudden increase in PE (surprise) → High surprise salience

    // 1. Establish baseline with low PE
    fep->levels[0].errors.magnitude = 1.0f;
    fep->levels[1].errors.magnitude = 0.8f;

    salience_fep_compute_salience_from_pe(bridge);

    salience_fep_state_t state_before;
    salience_fep_bridge_get_state(bridge, &state_before);
    float baseline_salience = state_before.current_salience;

    // 2. Introduce surprising event (large PE increase)
    fep->levels[0].errors.magnitude = 10.0f;
    fep->levels[1].errors.magnitude = 8.0f;

    salience_fep_compute_salience_from_pe(bridge);

    // 3. Verify surprise increased salience
    salience_fep_state_t state_after;
    salience_fep_bridge_get_state(bridge, &state_after);

    EXPECT_GT(state_after.current_salience, baseline_salience)
        << "Surprising events should increase salience";

    // 4. Verify high salience event was recorded
    EXPECT_GT(state_after.high_salience_events, state_before.high_salience_events)
        << "High salience event should be counted";
}

/* ============================================================================
 * Integration Test: Novelty Detection
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, NoveltyDetection) {
    // Scenario: PE deviates from running average → Novelty component increases

    // 1. Establish stable baseline over multiple updates
    for (int i = 0; i < 10; i++) {
        fep->levels[0].errors.magnitude = 2.0f;
        fep->levels[1].errors.magnitude = 1.5f;
        salience_fep_compute_salience_from_pe(bridge);
    }

    salience_fep_state_t state_baseline;
    salience_fep_bridge_get_state(bridge, &state_baseline);
    float expected_pe = state_baseline.avg_prediction_error;

    // 2. Introduce novel stimulus (much higher PE than average)
    fep->levels[0].errors.magnitude = expected_pe * 3.0f;
    fep->levels[1].errors.magnitude = expected_pe * 2.5f;

    salience_fep_compute_salience_from_pe(bridge);

    // 3. Verify novelty increased salience
    salience_fep_state_t state_novel;
    salience_fep_bridge_get_state(bridge, &state_novel);

    EXPECT_GT(state_novel.current_salience, state_baseline.current_salience)
        << "Novel stimuli should increase salience above baseline";
}

/* ============================================================================
 * Integration Test: Precision-Weighted Urgency
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, PrecisionWeightedUrgency) {
    // Scenario: High precision × High PE → High urgency → High salience

    // 1. Set high precision and high PE (confident prediction violated)
    fep->levels[0].errors.magnitude = 7.0f;
    fep->levels[1].errors.magnitude = 5.0f;

    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 10.0f;  // Very high precision
    }
    for (uint32_t i = 0; i < fep->levels[1].errors.dim; i++) {
        fep->levels[1].errors.precision[i] = 10.0f;
    }

    salience_fep_compute_salience_from_pe(bridge);

    salience_fep_state_t state_high_precision;
    salience_fep_bridge_get_state(bridge, &state_high_precision);

    // 2. Compare to low precision case
    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 0.1f;  // Very low precision
    }
    for (uint32_t i = 0; i < fep->levels[1].errors.dim; i++) {
        fep->levels[1].errors.precision[i] = 0.1f;
    }

    // Reset salience state for fair comparison
    bridge->state.current_salience = 0.0f;

    salience_fep_compute_salience_from_pe(bridge);

    salience_fep_state_t state_low_precision;
    salience_fep_bridge_get_state(bridge, &state_low_precision);

    // 3. High precision errors should create higher urgency/salience
    EXPECT_GT(state_high_precision.current_salience, state_low_precision.current_salience)
        << "High precision PE should create higher urgency than low precision PE";
}

/* ============================================================================
 * Integration Test: Salience Gating
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, SalienceGatingModulation) {
    // Scenario: Salience modulates gating of belief updates

    // 1. High salience case
    bridge->state.current_salience = 0.9f;
    salience_fep_gate_by_salience(bridge);

    salience_fep_state_t state_high;
    salience_fep_bridge_get_state(bridge, &state_high);
    float gating_high = bridge->effects.gating_factor;

    // 2. Low salience case
    bridge->state.current_salience = 0.1f;
    salience_fep_gate_by_salience(bridge);

    float gating_low = bridge->effects.gating_factor;

    // 3. Verify high salience produces stronger gating
    EXPECT_GT(gating_high, gating_low)
        << "High salience should produce stronger gating than low salience";

    // 4. Verify gating is in valid range
    EXPECT_GE(gating_high, 0.5f);
    EXPECT_LE(gating_high, 1.0f);
    EXPECT_GE(gating_low, 0.5f);
    EXPECT_LE(gating_low, 1.0f);
}

/* ============================================================================
 * Integration Test: Full Update Cycle
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, FullUpdateCycle) {
    // Scenario: Test complete bidirectional update cycle

    // 1. Set up FEP state
    fep->levels[0].errors.magnitude = 5.0f;
    fep->levels[1].errors.magnitude = 3.0f;

    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 3.0f;
    }

    // 2. Run full update cycle
    int result = salience_fep_bridge_update(bridge, 100);
    ASSERT_EQ(result, 0);

    // 3. Verify all effects were computed
    salience_fep_state_t state;
    salience_fep_bridge_get_state(bridge, &state);

    EXPECT_GT(state.current_salience, 0.0f) << "Salience should be computed";
    EXPECT_GT(bridge->effects.salience_from_pe, 0.0f) << "PE→salience effect should be computed";
    EXPECT_GT(bridge->effects.precision_boost, 0.0f) << "Precision boost should be computed";
    EXPECT_GT(bridge->effects.gating_factor, 0.0f) << "Gating factor should be computed";

    // 4. Verify statistics updated
    salience_fep_stats_t stats;
    salience_fep_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.avg_salience, 0.0f);
}

/* ============================================================================
 * Integration Test: Multiple Update Cycles (Temporal Dynamics)
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, TemporalDynamics) {
    // Scenario: Test that running average of PE stabilizes over time

    // Run multiple update cycles with varying PE
    for (int i = 0; i < 20; i++) {
        fep->levels[0].errors.magnitude = 3.0f + sinf(i * 0.5f) * 2.0f;
        fep->levels[1].errors.magnitude = 2.0f + cosf(i * 0.3f) * 1.5f;

        salience_fep_bridge_update(bridge, 100);
    }

    // Verify running average is in reasonable range
    salience_fep_state_t state;
    salience_fep_bridge_get_state(bridge, &state);

    EXPECT_GT(state.avg_prediction_error, 1.0f);
    EXPECT_LT(state.avg_prediction_error, 6.0f);

    // Verify statistics accumulated
    salience_fep_stats_t stats;
    salience_fep_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.total_precision_boosts, 0);
    EXPECT_GT(stats.total_salience_gates, 0);
    EXPECT_GT(stats.avg_salience, 0.0f);
}

/* ============================================================================
 * Integration Test: Aberrant Salience Scenario (Psychosis Model)
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, AberrantSalienceScenario) {
    // Scenario: Model aberrant salience in psychosis - inappropriate salience
    // attribution to irrelevant stimuli due to precision dysregulation

    // 1. Normal case: Low PE with appropriate precision
    fep->levels[0].errors.magnitude = 1.0f;
    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 1.0f;
    }

    salience_fep_compute_salience_from_pe(bridge);
    salience_fep_state_t state_normal;
    salience_fep_bridge_get_state(bridge, &state_normal);
    float salience_normal = state_normal.current_salience;

    // Reset state
    bridge->state.current_salience = 0.0f;

    // 2. Aberrant case: Same low PE but inappropriately high precision
    //    (dysregulated precision → aberrant salience)
    fep->levels[0].errors.magnitude = 1.0f;  // Same low PE
    for (uint32_t i = 0; i < fep->levels[0].errors.dim; i++) {
        fep->levels[0].errors.precision[i] = 20.0f;  // Inappropriately high
    }

    salience_fep_compute_salience_from_pe(bridge);
    salience_fep_state_t state_aberrant;
    salience_fep_bridge_get_state(bridge, &state_aberrant);
    float salience_aberrant = state_aberrant.current_salience;

    // 3. Verify aberrant salience is higher despite same PE
    EXPECT_GT(salience_aberrant, salience_normal)
        << "Inappropriately high precision should create aberrant salience";
}

/* ============================================================================
 * Integration Test: Bio-Async Integration
 * ============================================================================ */

TEST_F(SalienceFEPBridgeIntegrationTest, BioAsyncIntegration) {
    // Connect to bio-async (may or may not succeed depending on router)
    int result = salience_fep_bridge_connect_bio_async(bridge);

    // Should not crash regardless of router availability
    EXPECT_EQ(result, 0);

    // Verify connection state can be queried
    bool connected = salience_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;  // May be true or false depending on router

    // Disconnect should be safe
    result = salience_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
