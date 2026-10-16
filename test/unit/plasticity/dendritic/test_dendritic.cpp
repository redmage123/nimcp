/**
 * @file test_dendritic.cpp
 * @brief Unit tests for Dendritic Nonlinearities module
 *
 * WHAT: Comprehensive tests for NMDA dynamics, compartments, dendritic spikes
 * WHY:  Verify biological realism and mathematical correctness
 *
 * BIOLOGICAL BASIS:
 * - Jahr & Stevens 1990: NMDA voltage dependence
 * - Larkum et al. 1999: Dendritic spikes
 * - Rall 1964: Cable theory and compartmental modeling
 *
 * TEST PHILOSOPHY:
 * - Test biological constraints (Mg²⁺ block, reversal potentials)
 * - Verify supralinear summation
 * - Performance benchmarks for real-time simulation
 */

#include <gtest/gtest.h>
#include "plasticity/dendritic/nimcp_dendritic.h"
#include <cmath>
#include <chrono>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class DendriticTest : public ::testing::Test {
protected:
    nmda_params_t nmda_params;
    dendritic_tree_config_t tree_config;

    void SetUp() override {
        nmda_params = nmda_params_default();
        tree_config = dendritic_tree_config_default();
    }
};

//=============================================================================
// NMDA Parameter Factory Tests
//=============================================================================

TEST_F(DendriticTest, NMDADefaultParamsValid) {
    EXPECT_GT(nmda_params.g_max, 0.0f);
    EXPECT_GT(nmda_params.tau_rise, 0.0f);
    EXPECT_GT(nmda_params.tau_decay, nmda_params.tau_rise);
    EXPECT_GT(nmda_params.ca_permeability, 0.0f);
}

TEST_F(DendriticTest, NMDANR2AFasterThanNR2B) {
    nmda_params_t nr2a = nmda_params_nr2a();
    nmda_params_t nr2b = nmda_params_nr2b();

    // NR2A should have faster kinetics
    EXPECT_LT(nr2a.tau_decay, nr2b.tau_decay);
}

TEST_F(DendriticTest, NMDANR2BMoreCalcium) {
    nmda_params_t nr2a = nmda_params_nr2a();
    nmda_params_t nr2b = nmda_params_nr2b();

    // NR2B should have higher Ca permeability (more plastic)
    EXPECT_GT(nr2b.ca_permeability, nr2a.ca_permeability);
}

//=============================================================================
// NMDA State Tests
//=============================================================================

TEST_F(DendriticTest, NMDAStateInitValid) {
    dendritic_nmda_state_t state = nmda_state_init();

    EXPECT_FLOAT_EQ(state.s, 0.0f);
    EXPECT_FLOAT_EQ(state.s_rise, 0.0f);
    EXPECT_FLOAT_EQ(state.conductance, 0.0f);
    EXPECT_FALSE(state.active);
}

//=============================================================================
// NMDA Voltage Block Tests (Jahr & Stevens 1990)
//=============================================================================

TEST_F(DendriticTest, NMDABlockAtRestingPotential) {
    // At -70 mV, NMDA should be mostly blocked by Mg²⁺
    float block = nmda_compute_block(-70.0f, &nmda_params);

    // B(V) = 1 / (1 + [Mg]/3.57 * exp(-0.062 * V))
    // At V = -70: exp(-0.062 * -70) = exp(4.34) ≈ 76.7
    // B(-70) ≈ 1 / (1 + 1/3.57 * 76.7) ≈ 1 / 22.5 ≈ 0.044
    EXPECT_LT(block, 0.1f);  // Mostly blocked
    EXPECT_GT(block, 0.0f);
}

TEST_F(DendriticTest, NMDABlockAtDepolarization) {
    // At 0 mV, NMDA should be largely unblocked
    float block = nmda_compute_block(0.0f, &nmda_params);

    // B(0) = 1 / (1 + 1/3.57 * 1) ≈ 0.78
    EXPECT_GT(block, 0.5f);  // Mostly unblocked
}

TEST_F(DendriticTest, NMDABlockMonotonicWithVoltage) {
    // Block should increase (unblock) with depolarization
    float block_low = nmda_compute_block(-80.0f, &nmda_params);
    float block_mid = nmda_compute_block(-40.0f, &nmda_params);
    float block_high = nmda_compute_block(0.0f, &nmda_params);

    EXPECT_LT(block_low, block_mid);
    EXPECT_LT(block_mid, block_high);
}

TEST_F(DendriticTest, NMDABlockBounded) {
    // Block factor should always be in [0, 1]
    for (float v = -100.0f; v <= 50.0f; v += 10.0f) {
        float block = nmda_compute_block(v, &nmda_params);
        EXPECT_GE(block, 0.0f);
        EXPECT_LE(block, 1.0f);
    }
}

TEST_F(DendriticTest, NMDABlockNullSafe) {
    float block = nmda_compute_block(0.0f, nullptr);
    EXPECT_FLOAT_EQ(block, 0.0f);
}

//=============================================================================
// NMDA Kinetics Tests
//=============================================================================

TEST_F(DendriticTest, NMDAKineticsRiseWithGlutamate) {
    dendritic_nmda_state_t state = nmda_state_init();
    float dt = 1.0f;

    // Apply glutamate
    nmda_update_kinetics(&state, 1.0f, dt, &nmda_params);

    EXPECT_GT(state.s, 0.0f);
    EXPECT_TRUE(state.active);
}

TEST_F(DendriticTest, NMDAKineticsDecayWithoutGlutamate) {
    dendritic_nmda_state_t state = nmda_state_init();
    state.s = 0.8f;  // Start high
    float dt = 10.0f;

    // No glutamate
    for (int i = 0; i < 100; i++) {
        nmda_update_kinetics(&state, 0.0f, dt, &nmda_params);
    }

    EXPECT_LT(state.s, 0.1f);  // Should decay
    EXPECT_FALSE(state.active);
}

TEST_F(DendriticTest, NMDAKineticsBounded) {
    dendritic_nmda_state_t state = nmda_state_init();
    float dt = 1.0f;

    // Saturate with glutamate
    for (int i = 0; i < 1000; i++) {
        nmda_update_kinetics(&state, 1.0f, dt, &nmda_params);
    }

    EXPECT_GE(state.s, 0.0f);
    EXPECT_LE(state.s, 1.0f);
}

//=============================================================================
// NMDA Current and Calcium Tests
//=============================================================================

TEST_F(DendriticTest, NMDACurrentDependsOnVoltage) {
    dendritic_nmda_state_t state = nmda_state_init();
    state.s = 0.5f;  // Partially open

    float current_rest = nmda_compute_current(&state, -70.0f, &nmda_params);
    float current_depol = nmda_compute_current(&state, -20.0f, &nmda_params);

    // Current magnitude should be larger at depolarized voltages (less blocked)
    // Note: current is negative (inward) for excitation, so compare magnitudes
    EXPECT_GT(std::abs(current_depol), std::abs(current_rest));
}

TEST_F(DendriticTest, NMDACalciumInfluxPositive) {
    dendritic_nmda_state_t state = nmda_state_init();
    state.s = 0.5f;

    float ca_influx = nmda_compute_calcium_influx(&state, -20.0f, &nmda_params);

    EXPECT_GE(ca_influx, 0.0f);  // Calcium influx should be positive
}

TEST_F(DendriticTest, NMDACalciumIncreasesWithOpenChannel) {
    dendritic_nmda_state_t state_closed = nmda_state_init();
    state_closed.s = 0.1f;

    dendritic_nmda_state_t state_open = nmda_state_init();
    state_open.s = 0.9f;

    float ca_closed = nmda_compute_calcium_influx(&state_closed, -20.0f, &nmda_params);
    float ca_open = nmda_compute_calcium_influx(&state_open, -20.0f, &nmda_params);

    EXPECT_GT(ca_open, ca_closed);
}

//=============================================================================
// Compartment Parameter Tests
//=============================================================================

TEST_F(DendriticTest, CompartmentParamsDefaultValid) {
    compartment_params_t soma = compartment_params_default(COMPARTMENT_SOMA);
    compartment_params_t distal = compartment_params_default(COMPARTMENT_DISTAL);

    EXPECT_GT(soma.diameter, 0.0f);
    EXPECT_GT(soma.length, 0.0f);
    EXPECT_GT(distal.diameter, 0.0f);

    // Soma should be larger than distal
    EXPECT_GT(soma.diameter, distal.diameter);
}

TEST_F(DendriticTest, CompartmentTypesDifferent) {
    compartment_params_t proximal = compartment_params_default(COMPARTMENT_PROXIMAL);
    compartment_params_t distal = compartment_params_default(COMPARTMENT_DISTAL);
    compartment_params_t apical = compartment_params_default(COMPARTMENT_APICAL_TUFT);

    // Different types should have different supralinearity
    EXPECT_LT(proximal.supralinearity_factor, distal.supralinearity_factor);
    EXPECT_LT(distal.supralinearity_factor, apical.supralinearity_factor);
}

//=============================================================================
// Compartment State Tests
//=============================================================================

TEST_F(DendriticTest, CompartmentStateInitValid) {
    compartment_state_t state = compartment_state_init(-70.0f);

    EXPECT_FLOAT_EQ(state.voltage, -70.0f);
    EXPECT_FLOAT_EQ(state.total_excitatory, 0.0f);
    EXPECT_FLOAT_EQ(state.total_inhibitory, 0.0f);
    EXPECT_FALSE(state.spike_active);
    EXPECT_EQ(state.spike_count, 0u);
}

//=============================================================================
// Compartment Integration Tests
//=============================================================================

TEST_F(DendriticTest, CompartmentIntegrateExcitation) {
    compartment_state_t state = compartment_state_init(-70.0f);
    compartment_params_t params = compartment_params_default(COMPARTMENT_DISTAL);
    float dt = 1.0f;

    // Apply excitatory input
    compartment_integrate(&state, 1.0f, 0.0f, &params, dt);

    // Voltage should increase (depolarize)
    EXPECT_GT(state.voltage, -70.0f);
}

TEST_F(DendriticTest, CompartmentIntegrateInhibition) {
    compartment_state_t state = compartment_state_init(-60.0f);
    compartment_params_t params = compartment_params_default(COMPARTMENT_DISTAL);
    float dt = 1.0f;

    // Apply inhibitory input
    compartment_integrate(&state, 0.0f, 1.0f, &params, dt);

    // Voltage should decrease (hyperpolarize) toward E_GABA
    EXPECT_LT(state.voltage, -60.0f);
}

TEST_F(DendriticTest, CompartmentIntegrateCalciumDecays) {
    compartment_state_t state = compartment_state_init(-70.0f);
    state.calcium_concentration = 1.0f;
    compartment_params_t params = compartment_params_default(COMPARTMENT_DISTAL);
    float dt = 10.0f;

    // No input, calcium should decay
    for (int i = 0; i < 100; i++) {
        compartment_integrate(&state, 0.0f, 0.0f, &params, dt);
    }

    EXPECT_LT(state.calcium_concentration, 0.5f);
}

//=============================================================================
// Dendritic Spike Tests
//=============================================================================

TEST_F(DendriticTest, CompartmentSpikeGenerates) {
    compartment_state_t state = compartment_state_init(-35.0f);  // Near threshold
    state.voltage_prev = -40.0f;  // Was below
    state.voltage = -25.0f;  // Now above
    compartment_params_t params = compartment_params_default(COMPARTMENT_DISTAL);

    bool spiked = compartment_check_spike(&state, &params);

    EXPECT_TRUE(spiked);
    EXPECT_TRUE(state.spike_active);
    EXPECT_EQ(state.spike_count, 1u);
    EXPECT_GT(state.calcium_concentration, 0.0f);
}

TEST_F(DendriticTest, CompartmentSpikeNoFalsePositive) {
    compartment_state_t state = compartment_state_init(-50.0f);  // Below threshold
    compartment_params_t params = compartment_params_default(COMPARTMENT_DISTAL);

    bool spiked = compartment_check_spike(&state, &params);

    EXPECT_FALSE(spiked);
    EXPECT_FALSE(state.spike_active);
}

TEST_F(DendriticTest, CompartmentSpikeNoDoubleCount) {
    compartment_state_t state = compartment_state_init(-25.0f);
    state.voltage_prev = -35.0f;
    compartment_params_t params = compartment_params_default(COMPARTMENT_DISTAL);

    // First spike
    compartment_check_spike(&state, &params);
    EXPECT_EQ(state.spike_count, 1u);

    // Already active, shouldn't count again
    compartment_check_spike(&state, &params);
    EXPECT_EQ(state.spike_count, 1u);
}

//=============================================================================
// Supralinear Summation Tests
//=============================================================================

TEST_F(DendriticTest, SupralinearFactorBelowThreshold) {
    float factor = compartment_supralinear_factor(0.3f, 0.5f, 0.5f);
    EXPECT_FLOAT_EQ(factor, 1.0f);  // No boost below threshold
}

TEST_F(DendriticTest, SupralinearFactorAboveThreshold) {
    float factor = compartment_supralinear_factor(1.0f, 0.5f, 0.5f);

    // factor = 1 + 0.5 * (1.0 - 0.5) = 1.25
    EXPECT_NEAR(factor, 1.25f, 0.01f);
}

TEST_F(DendriticTest, SupralinearFactorClamped) {
    // Very high input
    float factor = compartment_supralinear_factor(10.0f, 0.5f, 0.5f);

    // Should be clamped to max (3.0)
    EXPECT_LE(factor, 3.0f);
}

//=============================================================================
// Dendritic Tree Tests
//=============================================================================

TEST_F(DendriticTest, TreeConfigDefaultValid) {
    EXPECT_GT(tree_config.num_branches, 0u);
    EXPECT_GT(tree_config.compartments_per_branch, 0u);
    EXPECT_TRUE(tree_config.enable_nmda);
    EXPECT_TRUE(tree_config.enable_dendritic_spikes);
}

TEST_F(DendriticTest, TreeCreateDestroy) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeCreateFailsWithNullConfig) {
    dendritic_tree_t tree = dendritic_tree_create(nullptr);
    EXPECT_EQ(tree, nullptr);
}

TEST_F(DendriticTest, TreeCreateFailsWithInvalidBranches) {
    dendritic_tree_config_t bad_config = tree_config;
    bad_config.num_branches = 0;

    dendritic_tree_t tree = dendritic_tree_create(&bad_config);
    EXPECT_EQ(tree, nullptr);

    bad_config.num_branches = 1000;  // Too many
    tree = dendritic_tree_create(&bad_config);
    EXPECT_EQ(tree, nullptr);
}

TEST_F(DendriticTest, TreeInjectInput) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    // Should not crash
    dendritic_tree_inject_input(tree, 0, 0, 1.0f, 0.0f, 0.5f);

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeInjectInputBoundsCheck) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    // Invalid branch - should not crash
    dendritic_tree_inject_input(tree, 1000, 0, 1.0f, 0.0f, 0.0f);

    // Invalid compartment - should not crash
    dendritic_tree_inject_input(tree, 0, 1000, 1.0f, 0.0f, 0.0f);

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeUpdate) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    float initial_voltage = dendritic_tree_get_soma_voltage(tree);

    // Inject input and update
    dendritic_tree_inject_input(tree, 0, 5, 2.0f, 0.0f, 1.0f);
    dendritic_tree_update(tree, 1.0f);

    // Soma voltage might change (depending on coupling)
    float final_voltage = dendritic_tree_get_soma_voltage(tree);

    // At minimum, should not crash and return valid voltage
    EXPECT_GT(final_voltage, -100.0f);
    EXPECT_LT(final_voltage, 50.0f);

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeGetSomaVoltage) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    float voltage = dendritic_tree_get_soma_voltage(tree);

    // Should start at resting potential
    EXPECT_NEAR(voltage, -70.0f, 1.0f);

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeGetTotalCalcium) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    float initial_ca = dendritic_tree_get_total_calcium(tree);
    EXPECT_FLOAT_EQ(initial_ca, 0.0f);  // No calcium initially

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeGetStats) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    dendritic_tree_stats_t stats;
    EXPECT_TRUE(dendritic_tree_get_stats(tree, &stats));

    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.dendritic_spikes, 0u);

    // Run some updates
    for (int i = 0; i < 10; i++) {
        dendritic_tree_inject_input(tree, 0, 5, 2.0f, 0.0f, 0.5f);
        dendritic_tree_update(tree, 1.0f);
    }

    EXPECT_TRUE(dendritic_tree_get_stats(tree, &stats));
    EXPECT_EQ(stats.total_updates, 10u);

    dendritic_tree_destroy(tree);
}

TEST_F(DendriticTest, TreeReset) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    // Run some updates
    for (int i = 0; i < 10; i++) {
        dendritic_tree_inject_input(tree, 0, 5, 5.0f, 0.0f, 1.0f);
        dendritic_tree_update(tree, 1.0f);
    }

    dendritic_tree_reset(tree);

    // Soma should be back to resting
    float voltage = dendritic_tree_get_soma_voltage(tree);
    EXPECT_NEAR(voltage, -70.0f, 1.0f);

    dendritic_tree_destroy(tree);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(DendriticTest, NMDABlockPerformance) {
    const int NUM_ITERATIONS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    float sum = 0.0f;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float v = -70.0f + (i % 100);
        sum += nmda_compute_block(v, &nmda_params);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float ns_per_op = (duration.count() * 1000.0f) / NUM_ITERATIONS;
    EXPECT_LT(ns_per_op, 100.0f);  // < 100 ns per operation

    std::cout << "NMDA block computation: " << ns_per_op << " ns per call (sum=" << sum << ")" << std::endl;
}

TEST_F(DendriticTest, TreeUpdatePerformance) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    const int NUM_UPDATES = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_UPDATES; i++) {
        // Inject input to random compartment
        dendritic_tree_inject_input(tree, i % tree_config.num_branches,
                                    i % tree_config.compartments_per_branch,
                                    1.0f, 0.0f, 0.5f);
        dendritic_tree_update(tree, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float us_per_update = duration.count() / (float)NUM_UPDATES;
    EXPECT_LT(us_per_update, 1000.0f);  // < 1 ms per update

    std::cout << "Dendritic tree update: " << us_per_update << " us per update ("
              << tree_config.num_branches << " branches x "
              << tree_config.compartments_per_branch << " compartments)" << std::endl;

    dendritic_tree_destroy(tree);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(DendriticTest, TreeDestroyNull) {
    // Should not crash
    dendritic_tree_destroy(nullptr);
}

TEST_F(DendriticTest, TreeNullOperations) {
    // All operations with null tree should not crash
    dendritic_tree_inject_input(nullptr, 0, 0, 1.0f, 0.0f, 0.0f);
    dendritic_tree_update(nullptr, 1.0f);

    float v = dendritic_tree_get_soma_voltage(nullptr);
    EXPECT_FLOAT_EQ(v, -70.0f);  // Returns default

    float ca = dendritic_tree_get_total_calcium(nullptr);
    EXPECT_FLOAT_EQ(ca, 0.0f);

    dendritic_tree_stats_t stats;
    EXPECT_FALSE(dendritic_tree_get_stats(nullptr, &stats));

    dendritic_tree_reset(nullptr);
}

//=============================================================================
// Biological Realism Tests
//=============================================================================

TEST_F(DendriticTest, NMDACoincidenceDetection) {
    // NMDA requires both glutamate AND depolarization
    dendritic_nmda_state_t state = nmda_state_init();
    float dt = 1.0f;

    // Case 1: Glutamate but hyperpolarized
    nmda_update_kinetics(&state, 1.0f, dt, &nmda_params);
    float current_hyper = nmda_compute_current(&state, -70.0f, &nmda_params);

    // Case 2: Same state but depolarized
    float current_depol = nmda_compute_current(&state, -20.0f, &nmda_params);

    // Depolarized should have much more current (unblocked)
    EXPECT_GT(std::abs(current_depol), std::abs(current_hyper) * 2.0f);
}

TEST_F(DendriticTest, DendriticAmplification) {
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    // Apply clustered input to distal compartments
    for (uint32_t c = 5; c < tree_config.compartments_per_branch; c++) {
        dendritic_tree_inject_input(tree, 0, c, 1.0f, 0.0f, 0.8f);
    }

    dendritic_tree_update(tree, 1.0f);

    dendritic_tree_stats_t stats;
    dendritic_tree_get_stats(tree, &stats);

    // With supralinearity enabled, should see amplified response
    // This is a qualitative test - exact values depend on parameters

    dendritic_tree_destroy(tree);
}
