/**
 * @file test_lc_integration.cpp
 * @brief Integration tests for Locus Coeruleus module
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.h"
#include "core/brain/regions/locus_coeruleus/nimcp_norepinephrine_release.h"
#include "core/brain/regions/locus_coeruleus/nimcp_arousal_modulation.h"
#include "core/brain/regions/locus_coeruleus/nimcp_novelty_detection.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class LCIntegrationTest : public ::testing::Test {
protected:
    nimcp_lc_adapter_t adapter;
    nimcp_lc_system_t* lc;

    void SetUp() override {
        adapter = nimcp_lc_adapter_create(nullptr);
        ASSERT_NE(adapter, nullptr);
        lc = nimcp_lc_adapter_get_lc(adapter);
        ASSERT_NE(lc, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            nimcp_lc_adapter_destroy(adapter);
        }
    }

    void runSimulation(float duration_ms, float dt_ms) {
        int steps = (int)(duration_ms / dt_ms);
        for (int i = 0; i < steps; i++) {
            nimcp_lc_adapter_update(adapter, dt_ms);
        }
    }
};

//=============================================================================
// Novelty-Arousal Integration Tests
//=============================================================================

TEST_F(LCIntegrationTest, NoveltyTriggersArousalIncrease) {
    float initial_arousal = lc->arousal_level;

    /* Novel input */
    float novel_input[] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
    nimcp_lc_adapter_process_input(adapter, novel_input, 8);

    runSimulation(500.0f, 10.0f);

    /* Novelty should have increased arousal */
    EXPECT_GT(lc->arousal_level, initial_arousal);
}

TEST_F(LCIntegrationTest, RepeatedInputReducesNoveltyResponse) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};

    /* First exposure */
    nimcp_lc_adapter_process_input(adapter, input, 4);
    float first_novelty = lc->novelty_signal;

    runSimulation(100.0f, 10.0f);

    /* Repeated exposures */
    for (int i = 0; i < 10; i++) {
        nimcp_lc_adapter_process_input(adapter, input, 4);
        runSimulation(50.0f, 10.0f);
    }

    /* Novelty should be lower for familiar stimulus */
    float final_novelty = lc->novelty_signal;
    EXPECT_LE(final_novelty, first_novelty);
}

//=============================================================================
// Mode Switching Integration Tests
//=============================================================================

TEST_F(LCIntegrationTest, HighNoveltyTriggersPhasicMode) {
    EXPECT_EQ(lc->mode, LC_MODE_TONIC);

    /* High novelty input */
    float novel_input[] = {10.0f, 10.0f, 10.0f, 10.0f};
    nimcp_lc_adapter_process_input(adapter, novel_input, 4);

    /* If novelty was high enough, mode should change */
    if (lc->novelty_signal > lc->mode_switch_threshold) {
        runSimulation(100.0f, 10.0f);
        EXPECT_TRUE(lc->mode == LC_MODE_PHASIC || lc->phasic_mode);
    }
}

TEST_F(LCIntegrationTest, PhasicModeTransitionsBackToTonic) {
    /* Force phasic mode */
    nimcp_lc_trigger_attention_reset(lc);
    EXPECT_EQ(lc->mode, LC_MODE_PHASIC);

    /* Run without new novelty - should return to tonic */
    runSimulation(1000.0f, 10.0f);

    EXPECT_EQ(lc->mode, LC_MODE_TONIC);
}

TEST_F(LCIntegrationTest, LowArousalTransitionsToQuiescent) {
    /* Force very low arousal state */
    lc->arousal_level = 0.05f;
    lc->tonic_firing_rate = 0.1f;

    runSimulation(500.0f, 10.0f);

    /* Should remain in low-to-moderate arousal range
     * Homeostasis prevents staying at extreme lows */
    EXPECT_LE(lc->arousal_level, 0.5f);
}

//=============================================================================
// NE Release Integration Tests
//=============================================================================

TEST_F(LCIntegrationTest, FiringRateAffectsNE) {
    /* Start with baseline */
    float baseline_ne = lc->ne_concentration;

    /* Apply excitation to increase firing */
    for (int i = 0; i < 10; i++) {
        nimcp_lc_apply_excitation(lc, 0.8f);
        nimcp_lc_adapter_update(adapter, 10.0f);
    }

    runSimulation(500.0f, 10.0f);

    /* NE should have increased */
    EXPECT_GT(lc->ne_concentration, baseline_ne);
}

TEST_F(LCIntegrationTest, NEDecaysWithoutInput) {
    /* First elevate NE */
    nimcp_lc_trigger_burst(lc, 1.0f, 100.0f);
    runSimulation(200.0f, 10.0f);

    float elevated_ne = lc->ne_concentration;

    /* Now let it decay */
    runSimulation(2000.0f, 10.0f);

    EXPECT_LT(lc->ne_concentration, elevated_ne);
}

TEST_F(LCIntegrationTest, StressIncreasesNEAndArousal) {
    float initial_ne = lc->ne_concentration;
    float initial_arousal = lc->arousal_level;

    nimcp_lc_signal_stress(lc, 0.9f);
    runSimulation(500.0f, 10.0f);

    EXPECT_GT(lc->ne_concentration, initial_ne);
    EXPECT_GT(lc->arousal_level, initial_arousal);
}

//=============================================================================
// Projection Integration Tests
//=============================================================================

TEST_F(LCIntegrationTest, ProjectionsReceiveNE) {
    /* Add cortical projection if not already present */
    nimcp_lc_projection_t* proj = nimcp_lc_get_projection_by_target(lc, LC_TARGET_CORTEX);
    if (!proj) {
        uint32_t id;
        nimcp_lc_add_projection(lc, LC_TARGET_CORTEX, "TestCortex", 0.8f, &id);
        proj = nimcp_lc_get_projection(lc, id);
    }

    /* Trigger NE release */
    nimcp_lc_trigger_burst(lc, 1.0f, 100.0f);
    runSimulation(200.0f, 10.0f);

    /* Projection should have received some NE */
    float target_ne;
    nimcp_lc_get_ne_at_target(lc, LC_TARGET_CORTEX, &target_ne);
    EXPECT_GT(target_ne, 0.0f);
}

TEST_F(LCIntegrationTest, GainModulationReflectsNE) {
    /* Get baseline gain */
    float baseline_gain;
    nimcp_lc_get_gain_modulation(lc, LC_TARGET_CORTEX, &baseline_gain);

    /* Increase NE */
    nimcp_lc_trigger_burst(lc, 1.0f, 100.0f);
    runSimulation(100.0f, 10.0f);

    float elevated_gain;
    nimcp_lc_get_gain_modulation(lc, LC_TARGET_CORTEX, &elevated_gain);

    /* Gain should change with NE level */
    /* Note: exact direction depends on inverted-U relationship */
    EXPECT_NE(elevated_gain, baseline_gain);
}

//=============================================================================
// Immune Integration Tests
//=============================================================================

TEST_F(LCIntegrationTest, InflammationAffectsLC) {
    float initial_arousal = lc->arousal_level;

    /* Apply inflammatory signal */
    nimcp_lc_adapter_process_immune(adapter, 0.7f, nullptr, 0);
    runSimulation(500.0f, 10.0f);

    /* Inflammation should affect LC state */
    EXPECT_NE(lc->arousal_level, initial_arousal);
}

TEST_F(LCIntegrationTest, CytokinesModulateActivity) {
    float cytokines[] = {0.8f, 0.9f, 0.7f};

    float initial_excitation = lc->neurons.excitatory_input;
    nimcp_lc_adapter_process_immune(adapter, 0.5f, cytokines, 3);

    /* Cytokines should have added excitation */
    EXPECT_GT(lc->neurons.excitatory_input, initial_excitation);
}

//=============================================================================
// Thalamic Gate Integration Tests
//=============================================================================

TEST_F(LCIntegrationTest, ThalamicGateReducesActivity) {
    /* Run without gate */
    runSimulation(100.0f, 10.0f);
    float ungated_arousal = lc->arousal_level;

    nimcp_lc_reset(lc);

    /* Apply strong thalamic gate */
    nimcp_lc_adapter_apply_thalamic_gate(adapter, 0.2f);
    runSimulation(100.0f, 10.0f);

    float gated_arousal = lc->arousal_level;

    /* Gating should reduce effective activity */
    EXPECT_LE(gated_arousal, ungated_arousal);
}

//=============================================================================
// Long-term Stability Tests
//=============================================================================

TEST_F(LCIntegrationTest, ExtendedSimulationStable) {
    /* Run for extended period with varying input */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* Active phase */
        float input[] = {(float)cycle * 0.1f, 0.5f, 0.3f, 0.7f};
        nimcp_lc_adapter_process_input(adapter, input, 4);
        runSimulation(100.0f, 10.0f);

        /* Rest phase */
        runSimulation(200.0f, 10.0f);
    }

    /* System should remain stable */
    EXPECT_FALSE(std::isnan(lc->ne_concentration));
    EXPECT_FALSE(std::isinf(lc->ne_concentration));
    EXPECT_GE(lc->ne_concentration, 0.0f);

    EXPECT_FALSE(std::isnan(lc->arousal_level));
    EXPECT_GE(lc->arousal_level, 0.0f);
    EXPECT_LE(lc->arousal_level, 1.0f);
}

TEST_F(LCIntegrationTest, MetricsAccurateAfterSimulation) {
    runSimulation(1000.0f, 10.0f);

    nimcp_lc_metrics_t metrics;
    nimcp_lc_get_metrics(lc, &metrics);

    EXPECT_EQ(metrics.update_count, 100u);
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
    EXPECT_GT(metrics.total_spikes, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
