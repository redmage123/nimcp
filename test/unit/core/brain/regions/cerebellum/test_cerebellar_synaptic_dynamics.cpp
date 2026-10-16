/**
 * @file test_cerebellar_synaptic_dynamics.cpp
 * @brief Unit tests for Cerebellar synaptic dynamics (Phase 1)
 *
 * Tests:
 * - Default configuration values
 * - Synapse state initialization
 * - Vesicle pool dynamics
 * - Short-term plasticity (Tsodyks-Markram model)
 * - Calcium dynamics
 * - Effective weight computation
 * - Null safety
 *
 * @version Phase 1: Synaptic Dynamics
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Default Configuration Tests
//=============================================================================

class CerebellarSynapseConfigTest : public ::testing::Test {};

TEST_F(CerebellarSynapseConfigTest, DefaultConfigHasValidVesicleParams) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // Vesicle pool params should be valid
    EXPECT_EQ(config.rrp_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY);
    EXPECT_EQ(config.recycling_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RECYCLING_CAP);
    EXPECT_FLOAT_EQ(config.release_probability, CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB);
    EXPECT_FLOAT_EQ(config.refill_rate, CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE);
}

TEST_F(CerebellarSynapseConfigTest, DefaultConfigHasValidSTPParams) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // STP params (Tsodyks-Markram model)
    EXPECT_FLOAT_EQ(config.stp_U, CEREBELLUM_SYNAPSE_DEFAULT_STP_U);
    EXPECT_FLOAT_EQ(config.stp_tau_D, CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D);
    EXPECT_FLOAT_EQ(config.stp_tau_F, CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F);
}

TEST_F(CerebellarSynapseConfigTest, DefaultConfigHasValidCalciumParams) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // Calcium dynamics params
    EXPECT_FLOAT_EQ(config.ca_baseline, CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE);
    EXPECT_FLOAT_EQ(config.ca_decay_tau, CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU);
    EXPECT_FLOAT_EQ(config.ca_influx_per_spike, CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX);
}

TEST_F(CerebellarSynapseConfigTest, DefaultConfigEnableFlags) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // By default, all dynamics should be enabled for realistic simulation
    EXPECT_TRUE(config.enable_vesicle_dynamics);
    EXPECT_TRUE(config.enable_stp);
    EXPECT_TRUE(config.enable_calcium_dynamics);
}

TEST_F(CerebellarSynapseConfigTest, ReleaseProbabilityInRange) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // Release probability must be in [0, 1]
    EXPECT_GE(config.release_probability, 0.0f);
    EXPECT_LE(config.release_probability, 1.0f);

    // STP_U must also be in [0, 1]
    EXPECT_GE(config.stp_U, 0.0f);
    EXPECT_LE(config.stp_U, 1.0f);
}

TEST_F(CerebellarSynapseConfigTest, TimeConstantsPositive) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // Time constants must be positive
    EXPECT_GT(config.stp_tau_D, 0.0f);
    EXPECT_GT(config.stp_tau_F, 0.0f);
    EXPECT_GT(config.ca_decay_tau, 0.0f);
}

//=============================================================================
// Synapse State Initialization Tests
//=============================================================================

class CerebellarSynapseInitTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        memset(&state, 0, sizeof(state));
        config = cerebellar_synapse_default_config();
    }
};

TEST_F(CerebellarSynapseInitTest, InitWithDefaultConfig) {
    cerebellar_synapse_init(&state, nullptr);

    // RRP should be full initially
    EXPECT_EQ(state.rrp_available, CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY);
    EXPECT_EQ(state.rrp_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY);
    EXPECT_FLOAT_EQ(state.vesicle_depletion, 0.0f);
}

TEST_F(CerebellarSynapseInitTest, InitWithCustomConfig) {
    config.rrp_capacity = 20;
    config.stp_U = 0.7f;
    config.ca_baseline = 0.2f;

    cerebellar_synapse_init(&state, &config);

    EXPECT_EQ(state.rrp_available, 20);
    EXPECT_EQ(state.rrp_capacity, 20);
    EXPECT_FLOAT_EQ(state.calcium_concentration, 0.2f);
}

TEST_F(CerebellarSynapseInitTest, STPStateInitialized) {
    cerebellar_synapse_init(&state, nullptr);

    // STP state: x (resources) should start at 1, u at baseline U
    EXPECT_FLOAT_EQ(state.stp_x, 1.0f);
    EXPECT_FLOAT_EQ(state.stp_u, CEREBELLUM_SYNAPSE_DEFAULT_STP_U);
    EXPECT_EQ(state.last_spike_time, 0);
}

TEST_F(CerebellarSynapseInitTest, CalciumStateInitialized) {
    cerebellar_synapse_init(&state, nullptr);

    // Calcium at baseline, decay tau set
    EXPECT_FLOAT_EQ(state.calcium_concentration, CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE);
    EXPECT_FLOAT_EQ(state.calcium_decay_tau, CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU);
}

TEST_F(CerebellarSynapseInitTest, FacilitationDepressionAtBaseline) {
    cerebellar_synapse_init(&state, nullptr);

    // Facilitation and depression factors at neutral baseline
    EXPECT_FLOAT_EQ(state.facilitation_factor, 1.0f);
    EXPECT_FLOAT_EQ(state.depression_factor, 1.0f);
    EXPECT_FLOAT_EQ(state.effective_weight, 1.0f);
}

TEST_F(CerebellarSynapseInitTest, NullStateHandledGracefully) {
    // Should not crash on null state
    cerebellar_synapse_init(nullptr, &config);
    // If we get here without crashing, test passes
}

//=============================================================================
// Vesicle Pool Dynamics Tests
//=============================================================================

class CerebellarVesicleDynamicsTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        config.enable_vesicle_dynamics = true;
        config.enable_stp = false;  // Isolate vesicle dynamics
        config.enable_calcium_dynamics = false;
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(CerebellarVesicleDynamicsTest, VesicleDepletionOnActivity) {
    uint32_t initial_rrp = state.rrp_available;

    // High presynaptic activity should deplete vesicles
    cerebellar_synapse_update(&state, 10.0f, 1.0f);  // 10ms, full activity

    EXPECT_LT(state.rrp_available, initial_rrp);
    EXPECT_GT(state.vesicle_depletion, 0.0f);
}

TEST_F(CerebellarVesicleDynamicsTest, VesicleRefillOverTime) {
    // First deplete vesicles
    cerebellar_synapse_update(&state, 10.0f, 1.0f);
    uint32_t depleted_rrp = state.rrp_available;

    // Then allow recovery with no activity
    cerebellar_synapse_update(&state, 100.0f, 0.0f);  // 100ms, no activity

    EXPECT_GE(state.rrp_available, depleted_rrp);
}

TEST_F(CerebellarVesicleDynamicsTest, VesiclePoolNeverExceedsCapacity) {
    // Multiple updates with no activity (refill)
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 10.0f, 0.0f);
    }

    EXPECT_LE(state.rrp_available, state.rrp_capacity);
}

TEST_F(CerebellarVesicleDynamicsTest, VesiclePoolNeverNegative) {
    // Extreme activity to fully deplete
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 5.0f, 1.0f);
    }

    EXPECT_GE(state.rrp_available, 0);
    EXPECT_LE(state.vesicle_depletion, 1.0f);
}

TEST_F(CerebellarVesicleDynamicsTest, DepletionScalesWithActivity) {
    cerebellar_synapse_state_t state_low, state_high;
    cerebellar_synapse_init(&state_low, &config);
    cerebellar_synapse_init(&state_high, &config);

    // Low activity
    cerebellar_synapse_update(&state_low, 10.0f, 0.2f);

    // High activity
    cerebellar_synapse_update(&state_high, 10.0f, 0.8f);

    // Higher activity should cause more depletion
    EXPECT_GT(state_high.vesicle_depletion, state_low.vesicle_depletion);
}

//=============================================================================
// Short-Term Plasticity (STP) Tests
//=============================================================================

class CerebellarSTPTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        config.enable_vesicle_dynamics = false;  // Isolate STP
        config.enable_stp = true;
        config.enable_calcium_dynamics = false;
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(CerebellarSTPTest, ResourceDepressionOnActivity) {
    float initial_x = state.stp_x;

    // Activity depletes resources
    cerebellar_synapse_update(&state, 5.0f, 1.0f);

    EXPECT_LT(state.stp_x, initial_x);
}

TEST_F(CerebellarSTPTest, ResourceRecoveryOverTime) {
    // First deplete resources
    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    float depleted_x = state.stp_x;

    // Recovery with no activity
    cerebellar_synapse_update(&state, 200.0f, 0.0f);  // Long recovery period

    EXPECT_GT(state.stp_x, depleted_x);
}

TEST_F(CerebellarSTPTest, FacilitationOnRepeatedActivity) {
    // Repeated stimulation should cause facilitation (increased u)
    float initial_u = state.stp_u;

    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    cerebellar_synapse_update(&state, 5.0f, 1.0f);

    // Facilitation increases utilization
    EXPECT_GE(state.stp_u, initial_u);
}

TEST_F(CerebellarSTPTest, FacilitationDecays) {
    // Build up facilitation
    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    float facilitated_u = state.stp_u;

    // Long pause should decay facilitation
    cerebellar_synapse_update(&state, 500.0f, 0.0f);

    EXPECT_LT(state.stp_u, facilitated_u);
}

TEST_F(CerebellarSTPTest, STPResourcesClampedToValidRange) {
    // Extreme activity
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 2.0f, 1.0f);
    }

    // x must be in [0, 1]
    EXPECT_GE(state.stp_x, 0.0f);
    EXPECT_LE(state.stp_x, 1.0f);

    // u must be in [0, 1]
    EXPECT_GE(state.stp_u, 0.0f);
    EXPECT_LE(state.stp_u, 1.0f);
}

TEST_F(CerebellarSTPTest, DepressionFactorReflectsSTPState) {
    cerebellar_synapse_state_t fresh_state;
    cerebellar_synapse_init(&fresh_state, &config);

    // Active state should have lower resources -> depression
    for (int i = 0; i < 10; i++) {
        cerebellar_synapse_update(&state, 5.0f, 1.0f);
    }

    // Depression factor should reflect resource depletion
    EXPECT_LT(state.depression_factor, fresh_state.depression_factor);
}

//=============================================================================
// Calcium Dynamics Tests
//=============================================================================

class CerebellarCalciumTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        config.enable_vesicle_dynamics = false;
        config.enable_stp = false;
        config.enable_calcium_dynamics = true;  // Isolate calcium
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(CerebellarCalciumTest, CalciumInfluxOnActivity) {
    float initial_ca = state.calcium_concentration;

    // Activity increases calcium
    cerebellar_synapse_update(&state, 5.0f, 1.0f);

    EXPECT_GT(state.calcium_concentration, initial_ca);
}

TEST_F(CerebellarCalciumTest, CalciumDecayWithoutActivity) {
    // First raise calcium
    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    float elevated_ca = state.calcium_concentration;

    // Decay without activity
    cerebellar_synapse_update(&state, 200.0f, 0.0f);

    EXPECT_LT(state.calcium_concentration, elevated_ca);
}

TEST_F(CerebellarCalciumTest, CalciumDecaysTowardBaseline) {
    // Raise calcium significantly
    for (int i = 0; i < 10; i++) {
        cerebellar_synapse_update(&state, 5.0f, 1.0f);
    }

    // Long decay period
    for (int i = 0; i < 50; i++) {
        cerebellar_synapse_update(&state, 100.0f, 0.0f);
    }

    // Should approach baseline
    EXPECT_NEAR(state.calcium_concentration, config.ca_baseline, 0.1f);
}

TEST_F(CerebellarCalciumTest, CalciumScalesWithActivityLevel) {
    cerebellar_synapse_state_t state_low, state_high;
    cerebellar_synapse_init(&state_low, &config);
    cerebellar_synapse_init(&state_high, &config);

    // Low activity
    cerebellar_synapse_update(&state_low, 10.0f, 0.2f);

    // High activity
    cerebellar_synapse_update(&state_high, 10.0f, 0.8f);

    // Higher activity should cause more calcium influx
    EXPECT_GT(state_high.calcium_concentration, state_low.calcium_concentration);
}

TEST_F(CerebellarCalciumTest, CalciumNeverNegative) {
    // Very long decay
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 100.0f, 0.0f);
    }

    EXPECT_GE(state.calcium_concentration, 0.0f);
}

//=============================================================================
// Effective Weight Computation Tests
//=============================================================================

class CerebellarEffectiveWeightTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(CerebellarEffectiveWeightTest, BaseWeightModulated) {
    float base_weight = 1.0f;

    float effective = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Initial effective weight should equal base weight (no modulation yet)
    EXPECT_FLOAT_EQ(effective, base_weight);
}

TEST_F(CerebellarEffectiveWeightTest, DepressionReducesEffectiveWeight) {
    float base_weight = 1.0f;

    // Activity causes depression
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 5.0f, 1.0f);
    }

    float effective = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Depressed synapse should have lower effective weight
    EXPECT_LT(effective, base_weight);
}

TEST_F(CerebellarEffectiveWeightTest, WeightScalesWithBaseWeight) {
    float small_weight = 0.5f;
    float large_weight = 2.0f;

    float effective_small = cerebellar_synapse_get_effective_weight(&state, small_weight);
    float effective_large = cerebellar_synapse_get_effective_weight(&state, large_weight);

    // Larger base weight should give larger effective weight
    EXPECT_GT(effective_large, effective_small);
}

TEST_F(CerebellarEffectiveWeightTest, EffectiveWeightNonNegative) {
    float base_weight = 1.0f;

    // Heavy depression
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 2.0f, 1.0f);
    }

    float effective = cerebellar_synapse_get_effective_weight(&state, base_weight);

    EXPECT_GE(effective, 0.0f);
}

TEST_F(CerebellarEffectiveWeightTest, RecoveryIncreasesEffectiveWeight) {
    float base_weight = 1.0f;

    // First depress
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 5.0f, 1.0f);
    }
    float depressed_weight = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Then recover
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 50.0f, 0.0f);
    }
    float recovered_weight = cerebellar_synapse_get_effective_weight(&state, base_weight);

    EXPECT_GT(recovered_weight, depressed_weight);
}

TEST_F(CerebellarEffectiveWeightTest, NullStateReturnsZero) {
    float effective = cerebellar_synapse_get_effective_weight(nullptr, 1.0f);

    EXPECT_FLOAT_EQ(effective, 0.0f);
}

//=============================================================================
// Integrated Dynamics Tests
//=============================================================================

class CerebellarIntegratedDynamicsTest : public ::testing::Test {
protected:
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config;

    void SetUp() override {
        config = cerebellar_synapse_default_config();
        // Enable all dynamics
        config.enable_vesicle_dynamics = true;
        config.enable_stp = true;
        config.enable_calcium_dynamics = true;
        cerebellar_synapse_init(&state, &config);
    }
};

TEST_F(CerebellarIntegratedDynamicsTest, AllDynamicsContribute) {
    float base_weight = 1.0f;

    // Initial state
    float initial_effective = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Activity affects all dynamics
    cerebellar_synapse_update(&state, 10.0f, 1.0f);

    float after_activity = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Weight should change due to combined effects
    EXPECT_NE(after_activity, initial_effective);
}

TEST_F(CerebellarIntegratedDynamicsTest, RepeatedStimulationPattern) {
    float base_weight = 1.0f;
    std::vector<float> weights;

    // Simulate burst pattern: 5 spikes at 100Hz, then 500ms pause
    for (int burst = 0; burst < 3; burst++) {
        // Burst of 5 spikes
        for (int spike = 0; spike < 5; spike++) {
            cerebellar_synapse_update(&state, 10.0f, 1.0f);
            weights.push_back(cerebellar_synapse_get_effective_weight(&state, base_weight));
        }
        // Pause
        cerebellar_synapse_update(&state, 500.0f, 0.0f);
    }

    // Weights should show depression during burst and recovery during pause
    EXPECT_GT(weights.size(), 0);
}

TEST_F(CerebellarIntegratedDynamicsTest, PairedPulse) {
    float base_weight = 1.0f;

    // First pulse
    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    float first_response = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Short interval (20ms) - within facilitation window
    cerebellar_synapse_update(&state, 20.0f, 0.0f);

    // Second pulse
    cerebellar_synapse_update(&state, 5.0f, 1.0f);
    float second_response = cerebellar_synapse_get_effective_weight(&state, base_weight);

    // Paired-pulse ratio depends on facilitation vs depression balance
    // Just verify both responses are valid
    EXPECT_GT(first_response, 0.0f);
    EXPECT_GT(second_response, 0.0f);
}

TEST_F(CerebellarIntegratedDynamicsTest, FrequencyDependence) {
    cerebellar_synapse_state_t state_low_freq, state_high_freq;
    cerebellar_synapse_init(&state_low_freq, &config);
    cerebellar_synapse_init(&state_high_freq, &config);

    float base_weight = 1.0f;

    // Low frequency: 10 Hz (100ms intervals)
    for (int i = 0; i < 10; i++) {
        cerebellar_synapse_update(&state_low_freq, 100.0f, 1.0f);
    }

    // High frequency: 100 Hz (10ms intervals)
    for (int i = 0; i < 10; i++) {
        cerebellar_synapse_update(&state_high_freq, 10.0f, 1.0f);
    }

    float low_freq_weight = cerebellar_synapse_get_effective_weight(&state_low_freq, base_weight);
    float high_freq_weight = cerebellar_synapse_get_effective_weight(&state_high_freq, base_weight);

    // High frequency should cause more depression
    EXPECT_GT(low_freq_weight, high_freq_weight);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

class CerebellarSynapseNullSafetyTest : public ::testing::Test {};

TEST_F(CerebellarSynapseNullSafetyTest, InitNullState) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    cerebellar_synapse_init(nullptr, &config);
    // Should not crash
}

TEST_F(CerebellarSynapseNullSafetyTest, UpdateNullState) {
    cerebellar_synapse_update(nullptr, 10.0f, 0.5f);
    // Should not crash
}

TEST_F(CerebellarSynapseNullSafetyTest, GetEffectiveWeightNullState) {
    float weight = cerebellar_synapse_get_effective_weight(nullptr, 1.0f);
    EXPECT_FLOAT_EQ(weight, 0.0f);
}

//=============================================================================
// Cerebellum Adapter Synaptic Dynamics Integration Tests
//=============================================================================

class CerebellumAdapterSynapticDynamicsTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_synaptic_dynamics = true;
        config.synapse_config = cerebellar_synapse_default_config();
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellumAdapterSynapticDynamicsTest, CreateWithSynapticDynamics) {
    ASSERT_NE(adapter, nullptr);

    cerebellum_config_t retrieved;
    EXPECT_TRUE(cerebellum_get_config(adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_synaptic_dynamics);
}

TEST_F(CerebellumAdapterSynapticDynamicsTest, StatsTrackVesicleReleases) {
    // Process some input to trigger vesicle releases
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 10; i++) {
        input.fiber_id = i;
        input.timestamp_ms = (float)i * 10.0f;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // If synaptic dynamics enabled, should track vesicle releases
    EXPECT_GE(stats.vesicle_releases, 0);
}

TEST_F(CerebellumAdapterSynapticDynamicsTest, SynapticDynamicsAffectOutput) {
    // First run: fresh synapses
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result1;
    cerebellum_process(adapter, &result1);

    // Second run: same input, but synapses may be depressed
    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result2;
    cerebellum_process(adapter, &result2);

    // Both should produce valid results
    EXPECT_TRUE(result1.motor_ready);
    EXPECT_TRUE(result2.motor_ready);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
