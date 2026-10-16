/**
 * @file test_phase1_bridges_regression.cpp
 * @brief Regression tests for Phase 1 bridges stability
 *
 * WHAT: Regression tests for all 6 Phase 1 bridge modules
 * WHY:  Ensure numerical stability, parameter bounds, and no regressions
 * HOW:  Extended simulations, stress tests, boundary conditions
 *
 * TEST CATEGORIES:
 * - Numerical Stability: Long-running without NaN/Inf
 * - Bounds: All values within expected ranges
 * - Memory: No leaks after create/destroy cycles
 * - Reproducibility: Consistent outputs for same inputs
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/bridges/nimcp_dendrite_plasticity_bridge.h"
#include "plasticity/bridges/nimcp_synapse_plasticity_bridge.h"
#include "plasticity/bridges/nimcp_axon_plasticity_bridge.h"
#include "glial/immune/nimcp_astrocytes_immune_bridge.h"
#include "glial/immune/nimcp_oligodendrocytes_immune_bridge.h"
#include "glial/immune/nimcp_myelin_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class Phase1BridgesRegressionTest : public ::testing::Test {
protected:
    // Plasticity bridges
    dendrite_plasticity_bridge_t* dendrite_bridge = nullptr;
    synapse_plasticity_bridge_t* synapse_bridge = nullptr;
    axon_plasticity_bridge_t* axon_bridge = nullptr;

    // Glial-immune bridges
    astro_immune_bridge_t* astro_bridge = nullptr;
    oligo_immune_bridge_t* oligo_bridge = nullptr;
    myelin_immune_bridge_t* myelin_bridge = nullptr;

    void SetUp() override {
        dendrite_bridge = dendrite_plasticity_create(nullptr, nullptr, nullptr);
        synapse_bridge = synapse_plasticity_create(nullptr, nullptr, nullptr);
        axon_bridge = axon_plasticity_create(nullptr, nullptr, nullptr);
        astro_bridge = astro_cell_create(nullptr, nullptr, nullptr);
        oligo_bridge = oligo_immune_create(nullptr, nullptr, nullptr);
        myelin_bridge = myelin_immune_create(nullptr, nullptr, nullptr);

        ASSERT_NE(dendrite_bridge, nullptr);
        ASSERT_NE(synapse_bridge, nullptr);
        ASSERT_NE(axon_bridge, nullptr);
        ASSERT_NE(astro_bridge, nullptr);
        ASSERT_NE(oligo_bridge, nullptr);
        ASSERT_NE(myelin_bridge, nullptr);
    }

    void TearDown() override {
        if (dendrite_bridge) dendrite_plasticity_destroy(dendrite_bridge);
        if (synapse_bridge) synapse_plasticity_destroy(synapse_bridge);
        if (axon_bridge) axon_plasticity_destroy(axon_bridge);
        if (astro_bridge) astro_cell_destroy(astro_bridge);
        if (oligo_bridge) oligo_immune_destroy(oligo_bridge);
        if (myelin_bridge) myelin_immune_destroy(myelin_bridge);
    }
};

//=============================================================================
// Numerical Stability Tests - Extended Simulations
//=============================================================================

TEST_F(Phase1BridgesRegressionTest, DendritePlasticityNumericalStability10000Updates) {
    float dt_ms = 1.0f;
    for (int i = 0; i < 10000; i++) {
        dendrite_plasticity_update_calcium(dendrite_bridge, i % 100, 0.5f);
        dendrite_plasticity_update(dendrite_bridge, dt_ms);
    }

    dendrite_plasticity_stats_t stats;
    dendrite_plasticity_get_stats(dendrite_bridge, &stats);

    EXPECT_FALSE(std::isnan(stats.avg_calcium_level));
    EXPECT_FALSE(std::isinf(stats.avg_calcium_level));
    EXPECT_TRUE(dendrite_bridge->initialized);
}

TEST_F(Phase1BridgesRegressionTest, SynapsePlasticityNumericalStability10000Updates) {
    for (int i = 0; i < 10000; i++) {
        synapse_plasticity_on_pre_spike(synapse_bridge, i);
        if (i % 2 == 0) {
            synapse_plasticity_on_post_spike(synapse_bridge, i + 5);
        }
        synapse_plasticity_update(synapse_bridge, 1.0f);
    }

    synapse_plasticity_stats_t stats;
    synapse_plasticity_get_stats(synapse_bridge, &stats);

    EXPECT_FALSE(std::isnan((float)stats.net_weight_change));
    EXPECT_FALSE(std::isinf((float)stats.net_weight_change));
    EXPECT_TRUE(synapse_bridge->initialized);
}

TEST_F(Phase1BridgesRegressionTest, AxonPlasticityNumericalStability10000Updates) {
    for (int i = 0; i < 10000; i++) {
        axon_plasticity_on_spike(axon_bridge, i % 64, i);
        axon_plasticity_update(axon_bridge, 1.0f);
        if (i % 100 == 0) {
            axon_plasticity_update_myelination(axon_bridge);
        }
    }

    axon_plasticity_stats_t stats;
    axon_plasticity_get_stats(axon_bridge, &stats);

    EXPECT_FALSE(std::isnan((float)stats.avg_conduction_velocity));
    EXPECT_FALSE(std::isinf((float)stats.avg_conduction_velocity));
    EXPECT_TRUE(axon_bridge->initialized);
}

TEST_F(Phase1BridgesRegressionTest, AstrocytesImmuneNumericalStability10000Updates) {
    float dt_ms = 10.0f;
    for (int i = 0; i < 10000; i++) {
        // Oscillate between A1 and A2 drive
        astro_bridge->cytokine_effects.a1_drive = (i % 200) < 100 ? 0.8f : 0.2f;
        astro_bridge->cytokine_effects.a2_drive = (i % 200) < 100 ? 0.2f : 0.8f;
        astro_cell_update(astro_bridge, dt_ms);
    }

    float clearance = astro_cell_get_glutamate_clearance(astro_bridge);

    EXPECT_FALSE(std::isnan(clearance));
    EXPECT_FALSE(std::isinf(clearance));
    EXPECT_GT(clearance, 0.0f);
    EXPECT_TRUE(astro_bridge->initialized);
}

TEST_F(Phase1BridgesRegressionTest, OligodendrocytesImmuneNumericalStability10000Updates) {
    float dt_ms = 10.0f;
    for (int i = 0; i < 10000; i++) {
        // Alternating damage and protection
        if ((i % 100) < 50) {
            oligo_bridge->cytokine_effects.net_damage_signal = 0.3f;
            oligo_bridge->cytokine_effects.net_protection_signal = 0.1f;
        } else {
            oligo_bridge->cytokine_effects.net_damage_signal = 0.1f;
            oligo_bridge->cytokine_effects.net_protection_signal = 0.5f;
        }
        oligo_immune_update(oligo_bridge, dt_ms);
    }

    EXPECT_FALSE(std::isnan(oligo_bridge->damage_level));
    EXPECT_FALSE(std::isinf(oligo_bridge->damage_level));
    EXPECT_GE(oligo_bridge->damage_level, 0.0f);
    EXPECT_LE(oligo_bridge->damage_level, OLIGO_IMMUNE_MAX_DAMAGE);
    EXPECT_TRUE(oligo_bridge->initialized);
}

TEST_F(Phase1BridgesRegressionTest, MyelinImmuneNumericalStability10000Updates) {
    float dt_ms = 10.0f;
    for (int i = 0; i < 10000; i++) {
        // Alternating damage and repair
        if ((i % 100) < 50) {
            myelin_bridge->cytokine_effects.net_damage = 0.3f;
            myelin_immune_apply_damage(myelin_bridge, dt_ms);
        } else {
            myelin_immune_apply_repair(myelin_bridge, dt_ms);
        }
    }

    float integrity = myelin_immune_get_integrity(myelin_bridge);
    EXPECT_FALSE(std::isnan(integrity));
    EXPECT_FALSE(std::isinf(integrity));
    EXPECT_GE(integrity, 0.0f);
    EXPECT_LE(integrity, 1.0f);
    EXPECT_TRUE(myelin_bridge->initialized);
}

//=============================================================================
// Parameter Bounds Tests
//=============================================================================

TEST_F(Phase1BridgesRegressionTest, DendritePlasticityConfigBounds) {
    dendrite_plasticity_config_t config;
    dendrite_plasticity_default_config(&config);

    // All parameters should be in valid ranges
    EXPECT_GE(config.calcium_ltp_threshold, 0.0f);
    EXPECT_LE(config.calcium_ltp_threshold, 1.0f);
    EXPECT_GE(config.calcium_ltd_threshold, 0.0f);
    EXPECT_GT(config.calcium_decay_tau_ms, 0.0f);
    EXPECT_GE(config.stdp_gain, 0.0f);
}

TEST_F(Phase1BridgesRegressionTest, SynapsePlasticityConfigBounds) {
    synapse_plasticity_config_t config;
    synapse_plasticity_default_config(&config);

    EXPECT_GE(config.weight_min, 0.0f);
    EXPECT_LE(config.weight_max, 100.0f);
    EXPECT_LE(config.weight_min, config.weight_max);
    EXPECT_GT(config.integration_dt_ms, 0.0f);
}

TEST_F(Phase1BridgesRegressionTest, AxonPlasticityConfigBounds) {
    axon_plasticity_config_t config;
    axon_plasticity_default_config(&config);

    EXPECT_GE(config.base_conduction_velocity, 0.0f);
    EXPECT_GT(config.max_conduction_velocity, config.base_conduction_velocity);
    EXPECT_GT(config.excitability_max, 0.0f);
}

TEST_F(Phase1BridgesRegressionTest, AstrocytesImmuneConfigBounds) {
    astro_immune_config_t config;
    astro_cell_default_config(&config);

    EXPECT_GE(config.il1_a1_induction, 0.0f);
    EXPECT_LE(config.il1_a1_induction, 1.0f);
    EXPECT_GE(config.il10_a2_promotion, 0.0f);
    EXPECT_LE(config.il10_a2_promotion, 1.0f);
    EXPECT_GT(config.glutamate_clearance_base, 0.0f);
}

TEST_F(Phase1BridgesRegressionTest, OligodendrocytesImmuneConfigBounds) {
    oligo_immune_config_t config;
    oligo_immune_default_config(&config);

    EXPECT_GE(config.il1_myelination_reduction, 0.0f);
    EXPECT_LE(config.il1_myelination_reduction, 1.0f);
    EXPECT_GT(config.death_threshold, 0.0f);
    EXPECT_LE(config.death_threshold, 1.0f);
    EXPECT_GT(config.damage_accumulation_rate, 0.0f);
}

TEST_F(Phase1BridgesRegressionTest, MyelinImmuneConfigBounds) {
    myelin_immune_config_t config;
    myelin_immune_default_config(&config);

    EXPECT_GT(config.il1_damage_rate, 0.0f);
    EXPECT_GT(config.tnf_damage_rate, 0.0f);
    EXPECT_GT(config.il10_repair_rate, 0.0f);
    EXPECT_GT(config.integrity_repair_rate, 0.0f);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(Phase1BridgesRegressionTest, RepeatedCreateDestroyDendritePlasticity) {
    for (int i = 0; i < 100; i++) {
        dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        dendrite_plasticity_update(b, 10.0f);
        dendrite_plasticity_destroy(b);
    }
    // If we get here without crashing, test passes
    SUCCEED();
}

TEST_F(Phase1BridgesRegressionTest, RepeatedCreateDestroySynapsePlasticity) {
    for (int i = 0; i < 100; i++) {
        synapse_plasticity_bridge_t* b = synapse_plasticity_create(nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        synapse_plasticity_update(b, 10.0f);
        synapse_plasticity_destroy(b);
    }
    SUCCEED();
}

TEST_F(Phase1BridgesRegressionTest, RepeatedCreateDestroyAxonPlasticity) {
    for (int i = 0; i < 100; i++) {
        axon_plasticity_bridge_t* b = axon_plasticity_create(nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        axon_plasticity_update(b, 10.0f);
        axon_plasticity_destroy(b);
    }
    SUCCEED();
}

TEST_F(Phase1BridgesRegressionTest, RepeatedCreateDestroyAstrocytesImmune) {
    for (int i = 0; i < 100; i++) {
        astro_immune_bridge_t* b = astro_cell_create(nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        astro_cell_update(b, 10.0f);
        astro_cell_destroy(b);
    }
    SUCCEED();
}

TEST_F(Phase1BridgesRegressionTest, RepeatedCreateDestroyOligodendrocytesImmune) {
    for (int i = 0; i < 100; i++) {
        oligo_immune_bridge_t* b = oligo_immune_create(nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        oligo_immune_update(b, 10.0f);
        oligo_immune_destroy(b);
    }
    SUCCEED();
}

TEST_F(Phase1BridgesRegressionTest, RepeatedCreateDestroyMyelinImmune) {
    for (int i = 0; i < 100; i++) {
        myelin_immune_bridge_t* b = myelin_immune_create(nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        myelin_immune_update(b, 10.0f);
        myelin_immune_destroy(b);
    }
    SUCCEED();
}

//=============================================================================
// Reproducibility Tests
//=============================================================================

TEST_F(Phase1BridgesRegressionTest, DendritePlasticityReproducibleResults) {
    // Run same sequence twice
    float result1, result2;

    {
        dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(nullptr, nullptr, nullptr);
        for (int i = 0; i < 100; i++) {
            dendrite_plasticity_update_calcium(b, 0, 0.5f);
            dendrite_plasticity_update(b, 1.0f);
        }
        dendrite_plasticity_stats_t stats;
        dendrite_plasticity_get_stats(b, &stats);
        result1 = stats.avg_calcium_level;
        dendrite_plasticity_destroy(b);
    }

    {
        dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(nullptr, nullptr, nullptr);
        for (int i = 0; i < 100; i++) {
            dendrite_plasticity_update_calcium(b, 0, 0.5f);
            dendrite_plasticity_update(b, 1.0f);
        }
        dendrite_plasticity_stats_t stats;
        dendrite_plasticity_get_stats(b, &stats);
        result2 = stats.avg_calcium_level;
        dendrite_plasticity_destroy(b);
    }

    EXPECT_FLOAT_EQ(result1, result2);
}

TEST_F(Phase1BridgesRegressionTest, SynapsePlasticityReproducibleResults) {
    uint32_t result1, result2;

    {
        synapse_plasticity_bridge_t* b = synapse_plasticity_create(nullptr, nullptr, nullptr);
        for (int i = 0; i < 100; i++) {
            synapse_plasticity_on_pre_spike(b, i * 10);
            synapse_plasticity_on_post_spike(b, i * 10 + 5);
        }
        synapse_plasticity_stats_t stats;
        synapse_plasticity_get_stats(b, &stats);
        result1 = stats.pre_spike_count;
        synapse_plasticity_destroy(b);
    }

    {
        synapse_plasticity_bridge_t* b = synapse_plasticity_create(nullptr, nullptr, nullptr);
        for (int i = 0; i < 100; i++) {
            synapse_plasticity_on_pre_spike(b, i * 10);
            synapse_plasticity_on_post_spike(b, i * 10 + 5);
        }
        synapse_plasticity_stats_t stats;
        synapse_plasticity_get_stats(b, &stats);
        result2 = stats.pre_spike_count;
        synapse_plasticity_destroy(b);
    }

    EXPECT_EQ(result1, result2);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(Phase1BridgesRegressionTest, ExtremeDtValuesDoNotCrash) {
    // Very small dt
    EXPECT_EQ(dendrite_plasticity_update(dendrite_bridge, 0.001f), 0);
    EXPECT_EQ(synapse_plasticity_update(synapse_bridge, 0.001f), 0);
    EXPECT_EQ(axon_plasticity_update(axon_bridge, 0.001f), 0);

    // Very large dt
    EXPECT_EQ(dendrite_plasticity_update(dendrite_bridge, 1000.0f), 0);
    EXPECT_EQ(synapse_plasticity_update(synapse_bridge, 1000.0f), 0);
    EXPECT_EQ(axon_plasticity_update(axon_bridge, 1000.0f), 0);

    // Zero dt
    EXPECT_EQ(dendrite_plasticity_update(dendrite_bridge, 0.0f), 0);
    EXPECT_EQ(synapse_plasticity_update(synapse_bridge, 0.0f), 0);
    EXPECT_EQ(axon_plasticity_update(axon_bridge, 0.0f), 0);
}

TEST_F(Phase1BridgesRegressionTest, ExtremeCalciumValuesDoNotCrash) {
    EXPECT_EQ(dendrite_plasticity_update_calcium(dendrite_bridge, 0, 0.0f), 0);
    EXPECT_EQ(dendrite_plasticity_update_calcium(dendrite_bridge, 0, 1.0f), 0);
    EXPECT_EQ(dendrite_plasticity_update_calcium(dendrite_bridge, 0, -1.0f), 0);
    EXPECT_EQ(dendrite_plasticity_update_calcium(dendrite_bridge, 0, 100.0f), 0);
}

TEST_F(Phase1BridgesRegressionTest, ExtremeCytokineValuesDoNotCrash) {
    // Max values
    astro_bridge->cytokine_effects.a1_drive = 1.0f;
    astro_bridge->cytokine_effects.a2_drive = 1.0f;
    EXPECT_EQ(astro_cell_update(astro_bridge, 10.0f), 0);

    // Zero values
    astro_bridge->cytokine_effects.a1_drive = 0.0f;
    astro_bridge->cytokine_effects.a2_drive = 0.0f;
    EXPECT_EQ(astro_cell_update(astro_bridge, 10.0f), 0);

    // Negative values (edge case)
    astro_bridge->cytokine_effects.a1_drive = -1.0f;
    EXPECT_EQ(astro_cell_update(astro_bridge, 10.0f), 0);
}

TEST_F(Phase1BridgesRegressionTest, MaxDamageIsClamped) {
    oligo_bridge->cytokine_effects.net_damage_signal = 1.0f;
    for (int i = 0; i < 10000; i++) {
        oligo_immune_accumulate_damage(oligo_bridge, 1000.0f);
    }

    EXPECT_LE(oligo_bridge->damage_level, OLIGO_IMMUNE_MAX_DAMAGE);
}

TEST_F(Phase1BridgesRegressionTest, MyelinIntegrityClamped) {
    // Try to damage below 0
    myelin_bridge->cytokine_effects.net_damage = 1.0f;
    for (int i = 0; i < 10000; i++) {
        myelin_immune_apply_damage(myelin_bridge, 1000.0f);
    }

    float integrity = myelin_immune_get_integrity(myelin_bridge);
    EXPECT_GE(integrity, 0.0f);

    // Try to repair above 1
    for (int i = 0; i < 10000; i++) {
        myelin_immune_apply_repair(myelin_bridge, 1000.0f);
    }

    integrity = myelin_immune_get_integrity(myelin_bridge);
    EXPECT_LE(integrity, 1.0f);
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(Phase1BridgesRegressionTest, BridgesRemainValidAfterStress) {
    // Heavy workload on all bridges
    for (int i = 0; i < 1000; i++) {
        // Plasticity bridges
        dendrite_plasticity_update_calcium(dendrite_bridge, i % 100, 0.5f);
        dendrite_plasticity_update(dendrite_bridge, 1.0f);

        synapse_plasticity_on_pre_spike(synapse_bridge, i);
        synapse_plasticity_on_post_spike(synapse_bridge, i + 5);
        synapse_plasticity_update(synapse_bridge, 1.0f);

        axon_plasticity_on_spike(axon_bridge, i % 64, i);
        axon_plasticity_update(axon_bridge, 1.0f);

        // Immune bridges
        astro_bridge->cytokine_effects.a1_drive = 0.5f;
        astro_cell_update(astro_bridge, 10.0f);

        oligo_bridge->cytokine_effects.net_damage_signal = 0.2f;
        oligo_immune_update(oligo_bridge, 10.0f);

        myelin_bridge->cytokine_effects.net_damage = 0.1f;
        myelin_immune_update(myelin_bridge, 10.0f);
    }

    // All should remain valid
    EXPECT_TRUE(dendrite_bridge->initialized);
    EXPECT_TRUE(synapse_bridge->initialized);
    EXPECT_TRUE(axon_bridge->initialized);
    EXPECT_TRUE(astro_bridge->initialized);
    EXPECT_TRUE(oligo_bridge->initialized);
    EXPECT_TRUE(myelin_bridge->initialized);
}

TEST_F(Phase1BridgesRegressionTest, ResetRestoresInitialState) {
    // Generate activity
    dendrite_plasticity_update_calcium(dendrite_bridge, 0, 0.5f);
    synapse_plasticity_on_pre_spike(synapse_bridge, 100);
    astro_bridge->cytokine_effects.a1_drive = 0.8f;
    astro_cell_update_reactivity(astro_bridge, 10.0f);

    // Reset stats
    dendrite_plasticity_reset_stats(dendrite_bridge);
    synapse_plasticity_reset_stats(synapse_bridge);
    astro_cell_reset_stats(astro_bridge);

    // Verify stats are cleared
    dendrite_plasticity_stats_t d_stats;
    synapse_plasticity_stats_t s_stats;
    astro_immune_stats_t a_stats;

    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);
    astro_cell_get_stats(astro_bridge, &a_stats);

    EXPECT_EQ(d_stats.calcium_events, 0u);
    EXPECT_EQ(s_stats.pre_spike_count, 0u);
    EXPECT_EQ(a_stats.reactivity_changes, 0u);
}
