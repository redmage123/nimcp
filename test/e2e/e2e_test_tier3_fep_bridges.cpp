/**
 * @file e2e_test_tier3_fep_bridges.cpp
 * @brief End-to-end tests for Tier 3 FEP bridges (FEP bridges for Tier 2 Hub bridges)
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: E2E tests for FEP integration of Tier 2 Hub bridges
 * WHY:  Validate that Hub bridges integrate properly with FEP orchestrator
 * HOW:  Test FEP registration, coordinated updates, free energy tracking
 *
 * TIER 3 FEP BRIDGES:
 * - Imagination-Reasoning FEP: Scenario generation and counterfactual reasoning
 * - Game Theory-Executive FEP: Strategic decision-making alignment
 * - Mirror-Empathy FEP: Action mirroring and emotional resonance
 * - Salience-Attention FEP: Salience detection and attention allocation
 * - Predictive-Attention FEP: Predictive coding and attention precision
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

// Tier 3 FEP Bridges (FEP bridges for Tier 2 Hub bridges)
#include "cognitive/integration/nimcp_imagination_reasoning_fep_bridge.h"
#include "cognitive/integration/nimcp_game_theory_executive_fep_bridge.h"
#include "cognitive/integration/nimcp_mirror_empathy_fep_bridge.h"
#include "cognitive/integration/nimcp_salience_attention_fep_bridge.h"
#include "cognitive/integration/nimcp_predictive_attention_fep_bridge.h"

// FEP Orchestrator
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int NUM_TIER3_BRIDGES = 5;
static constexpr int STRESS_ITERATIONS = 100;
static constexpr float MAX_FREE_ENERGY = 2.0f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @class Tier3FEPBridgesE2ETest
 * @brief E2E test fixture for Tier 3 FEP bridges
 */
class Tier3FEPBridgesE2ETest : public ::testing::Test {
protected:
    // FEP bridges
    imag_reason_fep_bridge_t* imag_reason_fep_bridge = nullptr;
    gt_exec_fep_bridge_t* gt_exec_fep_bridge = nullptr;
    me_fep_bridge_t* me_fep_bridge = nullptr;
    sa_fep_bridge_t* sa_fep_bridge = nullptr;
    pa_fep_bridge_t* pa_fep_bridge = nullptr;

    // Configs
    imag_reason_fep_config_t imag_reason_config;
    gt_exec_fep_config_t gt_config;
    me_fep_config_t me_config;
    sa_fep_config_t sa_config;
    pa_fep_config_t pa_config;

    void SetUp() override {
        // Initialize configs
        imag_reason_config = imag_reason_fep_config_default();
        gt_config = gt_exec_fep_config_default();
        me_config = me_fep_config_default();
        sa_config = sa_fep_config_default();
        pa_config = pa_fep_config_default();

        // Create FEP bridges
        imag_reason_fep_bridge = imag_reason_fep_bridge_create(&imag_reason_config);
        gt_exec_fep_bridge = gt_exec_fep_bridge_create(&gt_config);
        me_fep_bridge = me_fep_bridge_create(&me_config);
        sa_fep_bridge = sa_fep_bridge_create(&sa_config);
        pa_fep_bridge = pa_fep_bridge_create(&pa_config);
    }

    void TearDown() override {
        if (imag_reason_fep_bridge) imag_reason_fep_bridge_destroy(imag_reason_fep_bridge);
        if (gt_exec_fep_bridge) gt_exec_fep_bridge_destroy(gt_exec_fep_bridge);
        if (me_fep_bridge) me_fep_bridge_destroy(me_fep_bridge);
        if (sa_fep_bridge) sa_fep_bridge_destroy(sa_fep_bridge);
        if (pa_fep_bridge) pa_fep_bridge_destroy(pa_fep_bridge);
    }

    int CountBridgesCreated() {
        int count = 0;
        if (imag_reason_fep_bridge) count++;
        if (gt_exec_fep_bridge) count++;
        if (me_fep_bridge) count++;
        if (sa_fep_bridge) count++;
        if (pa_fep_bridge) count++;
        return count;
    }
};

//=============================================================================
// Bridge Creation Tests
//=============================================================================

/**
 * Test: AllBridgesCreate
 * All 5 Tier 3 FEP bridges should be created successfully
 */
TEST_F(Tier3FEPBridgesE2ETest, AllBridgesCreate) {
    EXPECT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES)
        << "All 5 Tier 3 FEP bridges should be created";

    EXPECT_NE(imag_reason_fep_bridge, nullptr) << "IR FEP bridge should be created";
    EXPECT_NE(gt_exec_fep_bridge, nullptr) << "GT-Exec FEP bridge should be created";
    EXPECT_NE(me_fep_bridge, nullptr) << "ME FEP bridge should be created";
    EXPECT_NE(sa_fep_bridge, nullptr) << "SA FEP bridge should be created";
    EXPECT_NE(pa_fep_bridge, nullptr) << "PA FEP bridge should be created";
}

/**
 * Test: InitialFreeEnergy
 * All bridges should start with reasonable initial free energy
 */
TEST_F(Tier3FEPBridgesE2ETest, InitialFreeEnergy) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    float imag_reason_fe = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
    float gt_fe = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
    float me_fe = me_fep_bridge_get_free_energy(me_fep_bridge);
    float sa_fe = sa_fep_bridge_get_free_energy(sa_fep_bridge);
    float pa_fe = pa_fep_bridge_get_free_energy(pa_fep_bridge);

    // All should be at baseline (low) free energy initially
    EXPECT_LT(imag_reason_fe, MAX_FREE_ENERGY) << "IR FE should be reasonable";
    EXPECT_LT(gt_fe, MAX_FREE_ENERGY) << "GT-Exec FE should be reasonable";
    EXPECT_LT(me_fe, MAX_FREE_ENERGY) << "ME FE should be reasonable";
    EXPECT_LT(sa_fe, MAX_FREE_ENERGY) << "SA FE should be reasonable";
    EXPECT_LT(pa_fe, MAX_FREE_ENERGY) << "PA FE should be reasonable";
}

//=============================================================================
// FEP Update Tests
//=============================================================================

/**
 * Test: ForcedUpdateCycle
 * All bridges should support forced update cycles
 */
TEST_F(Tier3FEPBridgesE2ETest, ForcedUpdateCycle) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    // Store initial FE values
    float imag_reason_initial = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
    float gt_initial = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
    float me_initial = me_fep_bridge_get_free_energy(me_fep_bridge);
    float sa_initial = sa_fep_bridge_get_free_energy(sa_fep_bridge);
    float pa_initial = pa_fep_bridge_get_free_energy(pa_fep_bridge);

    // Force updates
    imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
    gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
    me_fep_bridge_force_update(me_fep_bridge);
    sa_fep_bridge_force_update(sa_fep_bridge);
    pa_fep_bridge_force_update(pa_fep_bridge);

    // Verify updates occurred (stats should increment)
    imag_reason_fep_stats_t imag_reason_stats;
    imag_reason_fep_bridge_get_stats(imag_reason_fep_bridge, &imag_reason_stats);
    EXPECT_GE(imag_reason_stats.total_updates, 1u) << "IR should have updates";

    gt_exec_fep_stats_t gt_stats;
    gt_exec_fep_bridge_get_stats(gt_exec_fep_bridge, &gt_stats);
    EXPECT_GE(gt_stats.total_updates, 1u) << "GT-Exec should have updates";

    me_fep_stats_t me_stats;
    me_fep_bridge_get_stats(me_fep_bridge, &me_stats);
    EXPECT_GE(me_stats.total_updates, 1u) << "ME should have updates";

    sa_fep_stats_t sa_stats;
    sa_fep_bridge_get_stats(sa_fep_bridge, &sa_stats);
    EXPECT_GE(sa_stats.total_updates, 1u) << "SA should have updates";

    pa_fep_stats_t pa_stats;
    pa_fep_bridge_get_stats(pa_fep_bridge, &pa_stats);
    EXPECT_GE(pa_stats.total_updates, 1u) << "PA should have updates";
}

//=============================================================================
// Metrics Update Tests
//=============================================================================

/**
 * Test: MetricsAffectFreeEnergy
 * Changing metrics should affect free energy calculations
 */
TEST_F(Tier3FEPBridgesE2ETest, MetricsAffectFreeEnergy) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    // Imagination-Reasoning: Update scenario quality
    float imag_reason_initial = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
    imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, 0.3f);  // Poor quality
    imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
    float imag_reason_after = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
    EXPECT_GE(imag_reason_after, imag_reason_initial) << "Poor scenario quality should increase FE";

    // Game Theory-Executive: Update decision quality
    float gt_initial = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
    gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, 0.2f);  // Poor
    gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
    float gt_after = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
    EXPECT_GE(gt_after, gt_initial) << "Poor decision quality should increase FE";

    // Mirror-Empathy: Update mirroring error
    float me_initial = me_fep_bridge_get_free_energy(me_fep_bridge);
    me_fep_bridge_update_mirroring_error(me_fep_bridge, 0.9f);  // High error
    me_fep_bridge_force_update(me_fep_bridge);
    float me_after = me_fep_bridge_get_free_energy(me_fep_bridge);
    EXPECT_GE(me_after, me_initial) << "High mirroring error should increase FE";

    // Salience-Attention: Update salience accuracy
    float sa_initial = sa_fep_bridge_get_free_energy(sa_fep_bridge);
    sa_fep_bridge_update_salience_error(sa_fep_bridge, 0.2f);  // Poor
    sa_fep_bridge_force_update(sa_fep_bridge);
    float sa_after = sa_fep_bridge_get_free_energy(sa_fep_bridge);
    EXPECT_GE(sa_after, sa_initial) << "Poor salience accuracy should increase FE";

    // Predictive-Attention: Update prediction accuracy
    float pa_initial = pa_fep_bridge_get_free_energy(pa_fep_bridge);
    pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, 0.2f);  // Poor
    pa_fep_bridge_force_update(pa_fep_bridge);
    float pa_after = pa_fep_bridge_get_free_energy(pa_fep_bridge);
    EXPECT_GE(pa_after, pa_initial) << "Poor prediction accuracy should increase FE";
}

//=============================================================================
// Reset Tests
//=============================================================================

/**
 * Test: ResetFunctionality
 * All bridges should support reset to initial state
 */
TEST_F(Tier3FEPBridgesE2ETest, ResetFunctionality) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    // Store initial states
    float imag_reason_initial = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
    float gt_initial = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
    float me_initial = me_fep_bridge_get_free_energy(me_fep_bridge);
    float sa_initial = sa_fep_bridge_get_free_energy(sa_fep_bridge);
    float pa_initial = pa_fep_bridge_get_free_energy(pa_fep_bridge);

    // Perturb states
    imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, 0.1f);
    gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, 0.1f);
    me_fep_bridge_update_mirroring_error(me_fep_bridge, 0.9f);
    sa_fep_bridge_update_salience_error(sa_fep_bridge, 0.1f);
    pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, 0.1f);

    // Force updates
    imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
    gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
    me_fep_bridge_force_update(me_fep_bridge);
    sa_fep_bridge_force_update(sa_fep_bridge);
    pa_fep_bridge_force_update(pa_fep_bridge);

    // Reset all
    imag_reason_fep_bridge_reset(imag_reason_fep_bridge);
    gt_exec_fep_bridge_reset(gt_exec_fep_bridge);
    me_fep_bridge_reset(me_fep_bridge);
    sa_fep_bridge_reset(sa_fep_bridge);
    pa_fep_bridge_reset(pa_fep_bridge);

    // Verify FE returned to initial
    float imag_reason_after = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
    float gt_after = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
    float me_after = me_fep_bridge_get_free_energy(me_fep_bridge);
    float sa_after = sa_fep_bridge_get_free_energy(sa_fep_bridge);
    float pa_after = pa_fep_bridge_get_free_energy(pa_fep_bridge);

    EXPECT_NEAR(imag_reason_after, imag_reason_initial, 0.01f) << "IR FE should reset";
    EXPECT_NEAR(gt_after, gt_initial, 0.01f) << "GT FE should reset";
    EXPECT_NEAR(me_after, me_initial, 0.01f) << "ME FE should reset";
    EXPECT_NEAR(sa_after, sa_initial, 0.01f) << "SA FE should reset";
    EXPECT_NEAR(pa_after, pa_initial, 0.01f) << "PA FE should reset";
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * Test: StressMultipleUpdateCycles
 * All bridges should handle many update cycles without issues
 */
TEST_F(Tier3FEPBridgesE2ETest, StressMultipleUpdateCycles) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        // Update metrics with varying values
        float quality = 0.3f + 0.7f * (float)(i % 10) / 10.0f;

        imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, quality);
        gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, quality);
        me_fep_bridge_update_empathy_error(me_fep_bridge, 1.0f - quality);
        sa_fep_bridge_update_salience_error(sa_fep_bridge, quality);
        pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, quality);

        // Force updates
        imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
        gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
        me_fep_bridge_force_update(me_fep_bridge);
        sa_fep_bridge_force_update(sa_fep_bridge);
        pa_fep_bridge_force_update(pa_fep_bridge);

        // Validate FE values are reasonable
        float imag_reason_fe = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge);
        float gt_fe = gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge);
        float me_fe = me_fep_bridge_get_free_energy(me_fep_bridge);
        float sa_fe = sa_fep_bridge_get_free_energy(sa_fep_bridge);
        float pa_fe = pa_fep_bridge_get_free_energy(pa_fep_bridge);

        EXPECT_GE(imag_reason_fe, 0.0f);
        EXPECT_GE(gt_fe, 0.0f);
        EXPECT_GE(me_fe, 0.0f);
        EXPECT_GE(sa_fe, 0.0f);
        EXPECT_GE(pa_fe, 0.0f);
    }

    // Verify all updates were counted
    imag_reason_fep_stats_t imag_reason_stats;
    imag_reason_fep_bridge_get_stats(imag_reason_fep_bridge, &imag_reason_stats);
    EXPECT_GE(imag_reason_stats.total_updates, (uint64_t)STRESS_ITERATIONS);

    gt_exec_fep_stats_t gt_stats;
    gt_exec_fep_bridge_get_stats(gt_exec_fep_bridge, &gt_stats);
    EXPECT_GE(gt_stats.total_updates, (uint64_t)STRESS_ITERATIONS);

    me_fep_stats_t me_stats;
    me_fep_bridge_get_stats(me_fep_bridge, &me_stats);
    EXPECT_GE(me_stats.total_updates, (uint64_t)STRESS_ITERATIONS);

    sa_fep_stats_t sa_stats;
    sa_fep_bridge_get_stats(sa_fep_bridge, &sa_stats);
    EXPECT_GE(sa_stats.total_updates, (uint64_t)STRESS_ITERATIONS);

    pa_fep_stats_t pa_stats;
    pa_fep_bridge_get_stats(pa_fep_bridge, &pa_stats);
    EXPECT_GE(pa_stats.total_updates, (uint64_t)STRESS_ITERATIONS);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

/**
 * Test: ConcurrentUpdates
 * Multiple threads updating bridges simultaneously
 */
TEST_F(Tier3FEPBridgesE2ETest, ConcurrentUpdates) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    std::atomic<int> update_count{0};
    constexpr int NUM_THREADS = 4;
    constexpr int UPDATES_PER_THREAD = 25;

    auto update_fn = [this, &update_count](int thread_id) {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            float quality = 0.5f + 0.1f * (float)(i % 5);

            imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, quality);
            imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);

            gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, quality);
            gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);

            me_fep_bridge_update_empathy_error(me_fep_bridge, 1.0f - quality);
            me_fep_bridge_force_update(me_fep_bridge);

            sa_fep_bridge_update_salience_error(sa_fep_bridge, quality);
            sa_fep_bridge_force_update(sa_fep_bridge);

            pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, quality);
            pa_fep_bridge_force_update(pa_fep_bridge);

            update_count++;
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(update_fn, t);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(update_count.load(), NUM_THREADS * UPDATES_PER_THREAD)
        << "All concurrent updates should complete";

    // Bridges should still be in valid state
    EXPECT_GE(imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge), 0.0f);
    EXPECT_GE(gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge), 0.0f);
    EXPECT_GE(me_fep_bridge_get_free_energy(me_fep_bridge), 0.0f);
    EXPECT_GE(sa_fep_bridge_get_free_energy(sa_fep_bridge), 0.0f);
    EXPECT_GE(pa_fep_bridge_get_free_energy(pa_fep_bridge), 0.0f);
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

/**
 * Test: CognitiveProcessingScenario
 * Simulate a cognitive processing scenario with all bridges
 */
TEST_F(Tier3FEPBridgesE2ETest, CognitiveProcessingScenario) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    // Phase 1: Novel stimulus - high prediction errors
    imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, 0.3f);
    imag_reason_fep_bridge_update_reasoning_coherence(imag_reason_fep_bridge, 0.4f);
    gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, 0.3f);
    me_fep_bridge_update_mirroring_error(me_fep_bridge, 0.8f);
    me_fep_bridge_update_empathy_error(me_fep_bridge, 0.7f);
    sa_fep_bridge_update_salience_error(sa_fep_bridge, 0.4f);
    pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, 0.3f);

    imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
    gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
    me_fep_bridge_force_update(me_fep_bridge);
    sa_fep_bridge_force_update(sa_fep_bridge);
    pa_fep_bridge_force_update(pa_fep_bridge);

    float phase1_total_fe = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge) +
                            gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge) +
                            me_fep_bridge_get_free_energy(me_fep_bridge) +
                            sa_fep_bridge_get_free_energy(sa_fep_bridge) +
                            pa_fep_bridge_get_free_energy(pa_fep_bridge);

    // Phase 2: Learning - improving predictions
    imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, 0.6f);
    imag_reason_fep_bridge_update_reasoning_coherence(imag_reason_fep_bridge, 0.7f);
    gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, 0.6f);
    me_fep_bridge_update_mirroring_error(me_fep_bridge, 0.4f);
    me_fep_bridge_update_empathy_error(me_fep_bridge, 0.3f);
    sa_fep_bridge_update_salience_error(sa_fep_bridge, 0.7f);
    pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, 0.7f);

    imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
    gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
    me_fep_bridge_force_update(me_fep_bridge);
    sa_fep_bridge_force_update(sa_fep_bridge);
    pa_fep_bridge_force_update(pa_fep_bridge);

    float phase2_total_fe = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge) +
                            gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge) +
                            me_fep_bridge_get_free_energy(me_fep_bridge) +
                            sa_fep_bridge_get_free_energy(sa_fep_bridge) +
                            pa_fep_bridge_get_free_energy(pa_fep_bridge);

    EXPECT_LT(phase2_total_fe, phase1_total_fe)
        << "Learning should reduce total free energy";

    // Phase 3: Mastery - optimal predictions
    imag_reason_fep_bridge_update_scenario_quality(imag_reason_fep_bridge, 0.95f);
    imag_reason_fep_bridge_update_reasoning_coherence(imag_reason_fep_bridge, 0.95f);
    gt_exec_fep_bridge_update_decision_quality(gt_exec_fep_bridge, 0.95f);
    gt_exec_fep_bridge_update_executive_alignment(gt_exec_fep_bridge, 0.99f);
    me_fep_bridge_update_mirroring_error(me_fep_bridge, 0.05f);
    me_fep_bridge_update_empathy_error(me_fep_bridge, 0.05f);
    me_fep_bridge_update_resonance_deficit(me_fep_bridge, 0.05f);
    sa_fep_bridge_update_salience_error(sa_fep_bridge, 0.95f);
    sa_fep_bridge_update_attention_efficiency(sa_fep_bridge, 0.95f);
    pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, 0.95f);
    pa_fep_bridge_update_attention_precision(pa_fep_bridge, 0.95f);

    imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
    gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
    me_fep_bridge_force_update(me_fep_bridge);
    sa_fep_bridge_force_update(sa_fep_bridge);
    pa_fep_bridge_force_update(pa_fep_bridge);

    float phase3_total_fe = imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge) +
                            gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge) +
                            me_fep_bridge_get_free_energy(me_fep_bridge) +
                            sa_fep_bridge_get_free_energy(sa_fep_bridge) +
                            pa_fep_bridge_get_free_energy(pa_fep_bridge);

    EXPECT_LT(phase3_total_fe, phase2_total_fe)
        << "Mastery should have lowest free energy";

    // Verify optimal states
    EXPECT_TRUE(gt_exec_fep_bridge_is_exec_aligned(gt_exec_fep_bridge))
        << "GT-Exec should be aligned at mastery";
    EXPECT_TRUE(me_fep_bridge_is_high_resonance(me_fep_bridge))
        << "ME should have high resonance at mastery";
}

/**
 * Test: GracefulDegradation
 * System should continue operating with some bridges NULL
 */
TEST_F(Tier3FEPBridgesE2ETest, GracefulDegradation) {
    // Destroy some bridges
    imag_reason_fep_bridge_destroy(imag_reason_fep_bridge);
    imag_reason_fep_bridge = nullptr;

    gt_exec_fep_bridge_destroy(gt_exec_fep_bridge);
    gt_exec_fep_bridge = nullptr;

    // Remaining bridges should still work
    EXPECT_NE(me_fep_bridge, nullptr);
    EXPECT_NE(sa_fep_bridge, nullptr);
    EXPECT_NE(pa_fep_bridge, nullptr);

    // Update remaining bridges
    me_fep_bridge_update_mirroring_error(me_fep_bridge, 0.5f);
    me_fep_bridge_force_update(me_fep_bridge);

    sa_fep_bridge_update_salience_error(sa_fep_bridge, 0.7f);
    sa_fep_bridge_force_update(sa_fep_bridge);

    pa_fep_bridge_update_prediction_accuracy(pa_fep_bridge, 0.8f);
    pa_fep_bridge_force_update(pa_fep_bridge);

    // Verify they return valid FE
    EXPECT_GE(me_fep_bridge_get_free_energy(me_fep_bridge), 0.0f);
    EXPECT_GE(sa_fep_bridge_get_free_energy(sa_fep_bridge), 0.0f);
    EXPECT_GE(pa_fep_bridge_get_free_energy(pa_fep_bridge), 0.0f);
}

/**
 * Test: StatisticsAccumulation
 * Statistics should accumulate correctly across operations
 */
TEST_F(Tier3FEPBridgesE2ETest, StatisticsAccumulation) {
    ASSERT_EQ(CountBridgesCreated(), NUM_TIER3_BRIDGES);

    constexpr int NUM_UPDATES = 10;

    for (int i = 0; i < NUM_UPDATES; i++) {
        imag_reason_fep_bridge_force_update(imag_reason_fep_bridge);
        gt_exec_fep_bridge_force_update(gt_exec_fep_bridge);
        me_fep_bridge_force_update(me_fep_bridge);
        sa_fep_bridge_force_update(sa_fep_bridge);
        pa_fep_bridge_force_update(pa_fep_bridge);
    }

    imag_reason_fep_stats_t imag_reason_stats;
    imag_reason_fep_bridge_get_stats(imag_reason_fep_bridge, &imag_reason_stats);
    EXPECT_EQ(imag_reason_stats.total_updates, (uint64_t)NUM_UPDATES)
        << "IR stats should track updates";

    gt_exec_fep_stats_t gt_stats;
    gt_exec_fep_bridge_get_stats(gt_exec_fep_bridge, &gt_stats);
    EXPECT_EQ(gt_stats.total_updates, (uint64_t)NUM_UPDATES)
        << "GT stats should track updates";

    me_fep_stats_t me_stats;
    me_fep_bridge_get_stats(me_fep_bridge, &me_stats);
    EXPECT_EQ(me_stats.total_updates, (uint64_t)NUM_UPDATES)
        << "ME stats should track updates";

    sa_fep_stats_t sa_stats;
    sa_fep_bridge_get_stats(sa_fep_bridge, &sa_stats);
    EXPECT_EQ(sa_stats.total_updates, (uint64_t)NUM_UPDATES)
        << "SA stats should track updates";

    pa_fep_stats_t pa_stats;
    pa_fep_bridge_get_stats(pa_fep_bridge, &pa_stats);
    EXPECT_EQ(pa_stats.total_updates, (uint64_t)NUM_UPDATES)
        << "PA stats should track updates";
}
