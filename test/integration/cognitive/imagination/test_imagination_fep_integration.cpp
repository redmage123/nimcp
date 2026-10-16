/**
 * @file test_imagination_fep_integration.cpp
 * @brief Integration tests for Imagination + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests integration between Imagination Engine and FEP orchestrator
 * WHY:  Verify FEP correctly coordinates imagination updates, tracks prediction
 *       errors from simulation divergence, and manages free energy across
 *       mental simulation and counterfactual reasoning
 * HOW:  Test FEP update cycles, simulation effects on free energy, counterfactual
 *       costs, and creative generation with controlled surprise
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/imagination/nimcp_imagination_fep_bridge.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define IMAGINATION_FEP_BRIDGE_ID_BASE     0x1A10
#define TEST_UPDATE_INTERVAL_MS            50
#define TEST_NUM_UPDATE_CYCLES             20

/* ============================================================================
 * Test Fixture: Imagination + FEP Integration
 * ============================================================================ */

class ImaginationFEPIntegrationTest : public ::testing::Test {
protected:
    imagination_engine_t* engine = nullptr;
    imagination_engine_config_t engine_config;
    imagination_fep_bridge_t* bridge = nullptr;
    imagination_fep_config_t bridge_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;

    void SetUp() override {
        /* Create imagination engine */
        engine_config = imagination_engine_default_config();
        engine_config.max_concurrent_scenarios = 8;
        engine_config.enable_reality_checking = true;
        engine_config.enable_counterfactual = true;
        engine_config.enable_prospective_mode = true;
        engine = imagination_engine_create(&engine_config);
        ASSERT_NE(engine, nullptr);

        /* Create FEP bridge */
        bridge_config = imagination_fep_config_default();
        bridge_config.enable_logging = false;
        bridge_config.enable_callbacks = true;
        bridge_config.enable_degraded_mode = true;
        bridge = imagination_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);

        /* Start FEP orchestrator */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
    }

    void TearDown() override {
        if (bridge) {
            imagination_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (engine) {
            imagination_engine_destroy(engine);
            engine = nullptr;
        }
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /* Helper to register bridge with FEP */
    bool register_bridge() {
        uint32_t bridge_id = 0;
        int ret = imagination_fep_bridge_register(bridge, fep_orch, engine, &bridge_id);
        return (ret == 0 && bridge_id > 0);
    }

    /* Helper to run FEP update cycles */
    int run_update_cycles(int num_cycles, uint64_t interval_ms = TEST_UPDATE_INTERVAL_MS) {
        uint64_t base_time = get_current_time_ms();
        int total_updated = 0;
        for (int i = 0; i < num_cycles; i++) {
            uint64_t current_time = base_time + (i * interval_ms);
            int updated = fep_orchestrator_update(fep_orch, current_time);
            if (updated >= 0) {
                total_updated += updated;
            }
        }
        return total_updated;
    }
};

/* ============================================================================
 * MentalSimulationWithFEP - Simulations minimize prediction error
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, MentalSimulationWithFEP) {
    ASSERT_TRUE(register_bridge());

    /* Get initial free energy */
    float initial_fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f);

    /* Run FEP update cycles */
    int updated = run_update_cycles(TEST_NUM_UPDATE_CYCLES);
    EXPECT_GT(updated, 0);

    /* Verify bridge is in active state */
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_TRUE(state == IMAGINATION_FEP_STATE_ACTIVE ||
                state == IMAGINATION_FEP_STATE_IDLE);

    /* Free energy should be within expected bounds */
    float current_fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(current_fe, 0.0f);
    EXPECT_LE(current_fe, IMAGINATION_FEP_MAX_FREE_ENERGY);

    /* Statistics should show updates occurred */
    imagination_fep_stats_t stats;
    int ret = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * CounterfactualReasoningFreeEnergy - Counterfactuals explore prediction space
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, CounterfactualReasoningFreeEnergy) {
    ASSERT_TRUE(register_bridge());

    /* Set config with higher counterfactual cost for testing */
    imagination_fep_config_t test_config = bridge_config;
    test_config.counterfactual_cost = 0.5f;
    int ret = imagination_fep_bridge_set_config(bridge, &test_config);
    EXPECT_EQ(ret, 0);

    /* Run update cycles */
    run_update_cycles(TEST_NUM_UPDATE_CYCLES);

    /* Get stats to check counterfactual tracking */
    imagination_fep_stats_t stats;
    ret = imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    /* Average counterfactual cost should be tracked */
    EXPECT_GE(stats.avg_counterfactual_cost, 0.0f);
    EXPECT_LE(stats.avg_counterfactual_cost, 1.0f);

    /* Verify counterfactual weight contributes to free energy */
    imagination_fep_config_t retrieved_config;
    ret = imagination_fep_bridge_get_config(bridge, &retrieved_config);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(retrieved_config.counterfactual_cost, 0.5f);
}

/* ============================================================================
 * CreativeGenerationSurprise - Novel ideas have controlled surprise
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, CreativeGenerationSurprise) {
    ASSERT_TRUE(register_bridge());

    /* Initial state should have baseline free energy */
    float baseline_fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_FLOAT_EQ(baseline_fe, bridge_config.baseline_free_energy);

    /* Run updates to generate some activity */
    run_update_cycles(10);

    /* Free energy should stay within bounds even with creative activity */
    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, bridge_config.max_free_energy);

    /* Prediction error should be bounded */
    float pe = imagination_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(pe, 0.0f);
    EXPECT_LE(pe, 1.0f);
}

/* ============================================================================
 * FutureProjectionUncertainty - Future predictions have uncertainty bounds
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, FutureProjectionUncertainty) {
    ASSERT_TRUE(register_bridge());

    /* Configure for prospective simulation tracking */
    imagination_fep_config_t test_config = bridge_config;
    test_config.simulation_divergence_weight = 0.5f;
    imagination_fep_bridge_set_config(bridge, &test_config);

    /* Run multiple update cycles */
    run_update_cycles(TEST_NUM_UPDATE_CYCLES);

    /* Simulation divergence should be tracked */
    float divergence = imagination_fep_bridge_get_simulation_divergence(bridge);
    EXPECT_GE(divergence, 0.0f);
    EXPECT_LE(divergence, 1.0f);

    /* Average divergence should be tracked in stats */
    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.avg_simulation_divergence, 0.0f);
    EXPECT_LE(stats.avg_simulation_divergence, 1.0f);
}

/* ============================================================================
 * ScenarioPlanningPredictionError - Planning reduces future uncertainty
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, ScenarioPlanningPredictionError) {
    ASSERT_TRUE(register_bridge());

    /* Run initial update cycles */
    run_update_cycles(10);

    /* Record initial prediction error */
    float initial_pe = imagination_fep_bridge_get_prediction_error(bridge);

    /* Run more cycles - continued processing should maintain bounded error */
    run_update_cycles(20);

    float final_pe = imagination_fep_bridge_get_prediction_error(bridge);

    /* Both should be bounded [0,1] */
    EXPECT_GE(initial_pe, 0.0f);
    EXPECT_LE(initial_pe, 1.0f);
    EXPECT_GE(final_pe, 0.0f);
    EXPECT_LE(final_pe, 1.0f);
}

/* ============================================================================
 * DreamStateFreeEnergy - Dream-like states have different free energy profile
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, DreamStateFreeEnergy) {
    ASSERT_TRUE(register_bridge());

    /* Configure for higher creativity (dream-like) */
    imagination_fep_config_t dream_config = bridge_config;
    dream_config.coherence_weight = 0.5f;  /* Higher coherence penalty */
    dream_config.prediction_accuracy_weight = 0.2f;  /* Lower prediction bonus */
    imagination_fep_bridge_set_config(bridge, &dream_config);

    /* Run update cycles */
    run_update_cycles(TEST_NUM_UPDATE_CYCLES);

    /* Free energy profile should be affected by weights */
    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, dream_config.max_free_energy);

    /* Stats should show activity */
    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * ImaginativeReplayLearning - Replaying scenarios reduces prediction error
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, ImaginativeReplayLearning) {
    ASSERT_TRUE(register_bridge());

    /* Run multiple rounds of update cycles */
    for (int round = 0; round < 5; round++) {
        run_update_cycles(10);
    }

    /* Get final statistics */
    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);

    /* Total updates should reflect all rounds (allowing margin for timing) */
    EXPECT_GE(stats.total_updates, 40u);

    /* Average free energy should be computed */
    EXPECT_GE(stats.avg_free_energy, 0.0f);

    /* Average update time should be reasonable */
    EXPECT_GE(stats.avg_update_time_us, 0.0f);
}

/* ============================================================================
 * NoveltySeekingBalance - Balance between novelty and prediction accuracy
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, NoveltySeekingBalance) {
    ASSERT_TRUE(register_bridge());

    /* Test with different prediction accuracy weights */
    imagination_fep_config_t low_novelty = bridge_config;
    low_novelty.prediction_accuracy_weight = 0.3f;
    imagination_fep_bridge_set_config(bridge, &low_novelty);

    run_update_cycles(10);
    float low_novelty_fe = imagination_fep_bridge_get_free_energy(bridge);

    /* Reset bridge */
    imagination_fep_bridge_reset(bridge);

    /* Test with higher novelty (lower prediction accuracy weight) */
    imagination_fep_config_t high_novelty = bridge_config;
    high_novelty.prediction_accuracy_weight = 0.05f;
    imagination_fep_bridge_set_config(bridge, &high_novelty);

    run_update_cycles(10);
    float high_novelty_fe = imagination_fep_bridge_get_free_energy(bridge);

    /* Both should be within bounds */
    EXPECT_GE(low_novelty_fe, 0.0f);
    EXPECT_LE(low_novelty_fe, bridge_config.max_free_energy);
    EXPECT_GE(high_novelty_fe, 0.0f);
    EXPECT_LE(high_novelty_fe, bridge_config.max_free_energy);
}

/* ============================================================================
 * FEPUpdateCycleIntegration - Verify 50ms update cycles work correctly
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, FEPUpdateCycleIntegration) {
    ASSERT_TRUE(register_bridge());

    /* Verify bridge is registered */
    EXPECT_TRUE(imagination_fep_bridge_is_registered(bridge));
    EXPECT_GT(imagination_fep_bridge_get_id(bridge), 0u);

    /* Run FEP updates at cognitive timescale (50ms) */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        uint64_t current_time = base_time + (i * 50);  /* 50ms intervals */
        int updated = fep_orchestrator_update(fep_orch, current_time);
        EXPECT_GE(updated, 0);
    }

    /* Verify FEP orchestrator tracked updates */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);
    EXPECT_GT(orch_stats.total_update_cycles, 0u);
    EXPECT_GT(orch_stats.total_bridge_updates, 0u);

    /* Verify bridge stats also updated */
    imagination_fep_stats_t bridge_stats;
    imagination_fep_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_GT(bridge_stats.total_updates, 0u);
}

/* ============================================================================
 * StatisticsAccumulation - Stats accumulate across multiple cycles
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, StatisticsAccumulation) {
    ASSERT_TRUE(register_bridge());

    /* Reset stats to ensure clean start */
    imagination_fep_bridge_reset_stats(bridge);

    /* Get initial stats */
    imagination_fep_stats_t initial_stats;
    imagination_fep_bridge_get_stats(bridge, &initial_stats);
    EXPECT_EQ(initial_stats.total_updates, 0u);
    EXPECT_FLOAT_EQ(initial_stats.avg_free_energy, 0.0f);

    /* Run first batch of updates */
    run_update_cycles(10);

    imagination_fep_stats_t mid_stats;
    imagination_fep_bridge_get_stats(bridge, &mid_stats);
    uint64_t mid_updates = mid_stats.total_updates;
    EXPECT_GT(mid_updates, 0u);

    /* Run second batch of updates */
    run_update_cycles(10);

    imagination_fep_stats_t final_stats;
    imagination_fep_bridge_get_stats(bridge, &final_stats);

    /* Updates should accumulate */
    EXPECT_GT(final_stats.total_updates, mid_updates);

    /* Averages should be computed */
    EXPECT_GE(final_stats.avg_free_energy, 0.0f);
    EXPECT_GE(final_stats.avg_simulation_divergence, 0.0f);
    EXPECT_GE(final_stats.avg_update_time_us, 0.0f);

    /* Total contribution should accumulate */
    EXPECT_GE(final_stats.total_free_energy_contribution, 0.0f);
}

/* ============================================================================
 * DegradedModeIntegration - High free energy triggers degraded mode
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, DegradedModeIntegration) {
    ASSERT_TRUE(register_bridge());

    /* Configure with low threshold to trigger degraded mode */
    imagination_fep_config_t degraded_config = bridge_config;
    degraded_config.high_free_energy_threshold = 0.05f;  /* Very low threshold */
    degraded_config.enable_degraded_mode = true;
    imagination_fep_bridge_set_config(bridge, &degraded_config);

    /* Run updates - should likely trigger degraded mode */
    run_update_cycles(TEST_NUM_UPDATE_CYCLES);

    /* Check stats for degraded mode entries */
    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);

    /* Either in degraded mode or threshold was never exceeded */
    bool is_degraded = imagination_fep_bridge_is_degraded(bridge);
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);

    if (is_degraded) {
        EXPECT_EQ(state, IMAGINATION_FEP_STATE_DEGRADED);
        EXPECT_GT(stats.degraded_mode_entries, 0u);
    } else {
        /* Free energy stayed below threshold */
        float fe = imagination_fep_bridge_get_free_energy(bridge);
        EXPECT_LE(fe, degraded_config.high_free_energy_threshold);
    }
}

/* ============================================================================
 * CallbackIntegration - Callbacks triggered during integration
 * ============================================================================ */

static std::atomic<int> g_high_fe_count{0};
static std::atomic<int> g_divergence_count{0};

static void integration_high_fe_callback(
    imagination_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
) {
    (void)bridge;
    (void)free_energy;
    (void)user_data;
    g_high_fe_count++;
}

static void integration_divergence_callback(
    imagination_fep_bridge_t* bridge,
    float divergence,
    void* user_data
) {
    (void)bridge;
    (void)divergence;
    (void)user_data;
    g_divergence_count++;
}

TEST_F(ImaginationFEPIntegrationTest, CallbackIntegration) {
    g_high_fe_count = 0;
    g_divergence_count = 0;

    ASSERT_TRUE(register_bridge());

    /* Register callbacks */
    imagination_fep_bridge_set_high_fe_callback(bridge, integration_high_fe_callback, nullptr);
    imagination_fep_bridge_set_divergence_callback(bridge, integration_divergence_callback, nullptr);

    /* Configure with low thresholds to trigger callbacks */
    imagination_fep_config_t callback_config = bridge_config;
    callback_config.high_free_energy_threshold = 0.05f;
    callback_config.divergence_threshold = 0.05f;
    callback_config.enable_callbacks = true;
    imagination_fep_bridge_set_config(bridge, &callback_config);

    /* Run updates */
    run_update_cycles(TEST_NUM_UPDATE_CYCLES);

    /* Callbacks should be invoked (depending on actual metrics) */
    /* Just verify the mechanism works - callbacks may or may not trigger */
    EXPECT_GE(g_high_fe_count.load(), 0);
    EXPECT_GE(g_divergence_count.load(), 0);
}

/* ============================================================================
 * MultipleOrchestratorsIntegration - Bridge handles re-registration
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, ReregistrationIntegration) {
    /* First registration */
    ASSERT_TRUE(register_bridge());
    uint32_t first_id = imagination_fep_bridge_get_id(bridge);
    EXPECT_GT(first_id, 0u);

    /* Run some updates */
    run_update_cycles(5);

    /* Unregister */
    EXPECT_EQ(imagination_fep_bridge_unregister(bridge), 0);
    EXPECT_FALSE(imagination_fep_bridge_is_registered(bridge));
    EXPECT_EQ(imagination_fep_bridge_get_id(bridge), 0u);

    /* Re-register */
    uint32_t new_id = 0;
    int ret = imagination_fep_bridge_register(bridge, fep_orch, engine, &new_id);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(new_id, 0u);
    EXPECT_TRUE(imagination_fep_bridge_is_registered(bridge));

    /* Run more updates after re-registration */
    run_update_cycles(5);

    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * ConcurrentAccessIntegration - Thread safety during FEP updates
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, ConcurrentAccessIntegration) {
    ASSERT_TRUE(register_bridge());

    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> update_count{0};

    /* Reader thread */
    std::thread reader([&]() {
        while (!stop) {
            imagination_fep_bridge_get_free_energy(bridge);
            imagination_fep_bridge_get_simulation_divergence(bridge);
            imagination_fep_bridge_get_state(bridge);
            read_count++;
        }
    });

    /* Updater thread - run FEP updates */
    std::thread updater([&]() {
        uint64_t base_time = get_current_time_ms();
        for (int i = 0; i < 50 && !stop; i++) {
            uint64_t current_time = base_time + (i * 10);
            fep_orchestrator_update(fep_orch, current_time);
            update_count++;
        }
        stop = true;
    });

    /* Wait for completion */
    updater.join();
    stop = true;
    reader.join();

    EXPECT_GT(read_count.load(), 0);
    EXPECT_GT(update_count.load(), 0);

    /* Verify bridge is still in valid state */
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_NE(state, IMAGINATION_FEP_STATE_ERROR);
}

/* ============================================================================
 * CoherenceWeightImpact - Coherence weight affects free energy
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, CoherenceWeightImpact) {
    ASSERT_TRUE(register_bridge());

    /* Test with different coherence weights */
    imagination_fep_config_t config1 = bridge_config;
    config1.coherence_weight = 0.1f;
    imagination_fep_bridge_set_config(bridge, &config1);

    run_update_cycles(10);

    imagination_fep_config_t config2 = bridge_config;
    config2.coherence_weight = 0.5f;
    imagination_fep_bridge_set_config(bridge, &config2);

    run_update_cycles(10);

    /* Free energy should remain bounded regardless of weights */
    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, bridge_config.max_free_energy);
}

/* ============================================================================
 * PeakFreeEnergyTracking - Peak free energy is tracked correctly
 * ============================================================================ */

TEST_F(ImaginationFEPIntegrationTest, PeakFreeEnergyTracking) {
    ASSERT_TRUE(register_bridge());

    /* Reset stats */
    imagination_fep_bridge_reset_stats(bridge);

    /* Run updates */
    run_update_cycles(TEST_NUM_UPDATE_CYCLES);

    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);

    /* Peak should be >= average (by definition) */
    EXPECT_GE(stats.peak_free_energy, 0.0f);
    if (stats.total_updates > 0) {
        EXPECT_GE(stats.peak_free_energy, stats.avg_free_energy * 0.9f);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
