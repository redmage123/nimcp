/**
 * @file test_cerebellum_synaptic_dynamics.cpp
 * @brief Unit tests for cerebellar synaptic dynamics (Phase 1)
 *
 * Tests:
 * - Vesicle pool dynamics
 * - Short-term plasticity (STP)
 * - Calcium dynamics
 * - Synaptic weight modulation
 *
 * @version Phase 1: Synaptic Dynamics
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Synaptic Configuration Tests
//=============================================================================

class SynapticConfigTest : public ::testing::Test {};

TEST_F(SynapticConfigTest, DefaultConfigValues) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // Vesicle pool
    EXPECT_EQ(config.rrp_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY);
    EXPECT_EQ(config.recycling_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RECYCLING_CAP);
    EXPECT_FLOAT_EQ(config.release_probability, CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB);
    EXPECT_FLOAT_EQ(config.refill_rate, CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE);

    // STP
    EXPECT_FLOAT_EQ(config.stp_U, CEREBELLUM_SYNAPSE_DEFAULT_STP_U);
    EXPECT_FLOAT_EQ(config.stp_tau_D, CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D);
    EXPECT_FLOAT_EQ(config.stp_tau_F, CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F);

    // Calcium
    EXPECT_FLOAT_EQ(config.ca_baseline, CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE);
    EXPECT_FLOAT_EQ(config.ca_decay_tau, CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU);
    EXPECT_FLOAT_EQ(config.ca_influx_per_spike, CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX);

    // Default disabled for backward compatibility
    EXPECT_FALSE(config.enable_vesicle_dynamics);
    EXPECT_FALSE(config.enable_stp);
    EXPECT_FALSE(config.enable_calcium_dynamics);
}

//=============================================================================
// Synaptic State Initialization Tests
//=============================================================================

class SynapticStateTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        config.enable_vesicle_dynamics = true;
        config.enable_stp = true;
        config.enable_calcium_dynamics = true;
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(SynapticStateTest, InitializationWithConfig) {
    EXPECT_EQ(state.rrp_available, config.rrp_capacity);
    EXPECT_EQ(state.rrp_capacity, config.rrp_capacity);
    EXPECT_FLOAT_EQ(state.vesicle_depletion, 0.0f);
    EXPECT_FLOAT_EQ(state.stp_x, 1.0f);
    EXPECT_FLOAT_EQ(state.stp_u, config.stp_U);
    EXPECT_FLOAT_EQ(state.calcium_concentration, config.ca_baseline);
}

TEST_F(SynapticStateTest, InitializationWithNull) {
    cerebellar_synapse_state_t null_state;
    cerebellar_synapse_init(&null_state, nullptr);

    // Should use defaults
    EXPECT_EQ(null_state.rrp_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY);
}

TEST_F(SynapticStateTest, InitializationNullState) {
    cerebellar_synapse_init(nullptr, &config);  // Should not crash
}

//=============================================================================
// Synaptic Update Tests
//=============================================================================

class SynapticUpdateTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        config.enable_vesicle_dynamics = true;
        config.enable_stp = true;
        config.enable_calcium_dynamics = true;
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(SynapticUpdateTest, UpdateWithActivity) {
    float initial_calcium = state.calcium_concentration;

    cerebellar_synapse_update(&state, 1.0f, 1.0f);  // 1ms, full activity

    // Calcium should increase with activity
    EXPECT_GT(state.calcium_concentration, initial_calcium);
}

TEST_F(SynapticUpdateTest, UpdateWithNoActivity) {
    // First spike to raise calcium
    cerebellar_synapse_update(&state, 1.0f, 1.0f);
    float peak_calcium = state.calcium_concentration;

    // Then no activity - calcium should decay
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.0f);
    }

    EXPECT_LT(state.calcium_concentration, peak_calcium);
}

TEST_F(SynapticUpdateTest, VesicleDepletion) {
    uint32_t initial_vesicles = state.rrp_available;

    // High activity should deplete vesicles
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }

    EXPECT_LT(state.rrp_available, initial_vesicles);
}

TEST_F(SynapticUpdateTest, VesicleRecovery) {
    // Deplete vesicles
    for (int i = 0; i < 50; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }
    uint32_t depleted_vesicles = state.rrp_available;

    // Allow recovery
    for (int i = 0; i < 1000; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.0f);
    }

    EXPECT_GT(state.rrp_available, depleted_vesicles);
}

TEST_F(SynapticUpdateTest, ShortTermDepression) {
    float initial_x = state.stp_x;

    // Repeated activity causes depression
    for (int i = 0; i < 10; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }

    EXPECT_LT(state.stp_x, initial_x);
}

TEST_F(SynapticUpdateTest, ShortTermFacilitation) {
    float initial_u = state.stp_u;

    // Activity can cause facilitation
    cerebellar_synapse_update(&state, 1.0f, 1.0f);

    // u should increase with facilitation
    EXPECT_GE(state.stp_u, initial_u);
}

TEST_F(SynapticUpdateTest, STDRecovery) {
    // Cause depression
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }
    float depressed_x = state.stp_x;

    // Allow recovery
    for (int i = 0; i < 500; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.0f);
    }

    EXPECT_GT(state.stp_x, depressed_x);
}

//=============================================================================
// Effective Weight Tests
//=============================================================================

class EffectiveWeightTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        config.enable_vesicle_dynamics = true;
        config.enable_stp = true;
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(EffectiveWeightTest, BaselineWeight) {
    float base_weight = 1.0f;
    float effective = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // At baseline, effective weight should be close to base
    EXPECT_NEAR(effective, base_weight, 0.5f);
}

TEST_F(EffectiveWeightTest, WeightAfterDepression) {
    float base_weight = 1.0f;

    // Cause depression
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }

    float effective = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Weight should be reduced after depression
    EXPECT_LT(effective, base_weight);
}

TEST_F(EffectiveWeightTest, WeightScalesWithBase) {
    float base_weight_1 = 1.0f;
    float base_weight_2 = 2.0f;

    float effective_1 = cerebellar_synapse_get_effective_weight(&state, base_weight_1);
    float effective_2 = cerebellar_synapse_get_effective_weight(&state, base_weight_2);

    // Effective weight should scale proportionally
    EXPECT_NEAR(effective_2 / effective_1, 2.0f, 0.1f);
}

//=============================================================================
// Cerebellum with Synaptic Dynamics Enabled
//=============================================================================

class CerebellumSynapticDynamicsTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_synaptic_dynamics = true;
        config.synapse_config = cerebellar_synapse_default_config();
        config.synapse_config.enable_vesicle_dynamics = true;
        config.synapse_config.enable_stp = true;
        config.synapse_config.enable_calcium_dynamics = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumSynapticDynamicsTest, EnabledInConfig) {
    cerebellum_config_t retrieved;
    cerebellum_get_config(adapter, &retrieved);
    EXPECT_TRUE(retrieved.enable_synaptic_dynamics);
}

TEST_F(CerebellumSynapticDynamicsTest, ProcessingWithDynamics) {
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    EXPECT_TRUE(cerebellum_process_mossy_input(adapter, &input));

    motor_coordination_result_t result;
    EXPECT_TRUE(cerebellum_process(adapter, &result));
}

TEST_F(CerebellumSynapticDynamicsTest, StatsTrackVesicleReleases) {
    // Process multiple inputs
    for (int i = 0; i < 100; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.vesicle_releases, 0);
}

TEST_F(CerebellumSynapticDynamicsTest, CalciumAffectsLearning) {
    // High-frequency input should raise calcium
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = 0;
        input.activity = 1.0f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.avg_calcium_concentration, 0.0f);
}

//=============================================================================
// Glomerular Divergence Tests
//=============================================================================

class GlomerularDivergenceTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_glomerular_divergence = true;
        config.granules_per_mossy_fiber = 75;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(GlomerularDivergenceTest, GlomerularLayerCreated) {
    glomerular_layer_t* glomerular = cerebellum_get_glomerular_layer(adapter);
    EXPECT_NE(glomerular, nullptr);
}

TEST_F(GlomerularDivergenceTest, DivergenceApplied) {
    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // Should have some divergence factor
    EXPECT_GT(stats.effective_divergence, 0.0f);
}

TEST_F(GlomerularDivergenceTest, MossyInputDiverges) {
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // Single mossy input should activate multiple granule cells
    EXPECT_GT(stats.granule_activations, 1);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
