//=============================================================================
// test_physics_quantum_integration.cpp - Physics QMC Integration Tests
//=============================================================================
/**
 * @file test_physics_quantum_integration.cpp
 * @brief Integration tests for physics layer Quantum Monte Carlo bridges
 *
 * Tests QMC integration with HH (parameter optimization, entropy),
 * Thermodynamics (partition functions), and Ephaptic (coherence optimization).
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/bridges/nimcp_hh_quantum_bridge.h"
#include "physics/bridges/nimcp_thermo_quantum_bridge.h"
#include "physics/bridges/nimcp_ephaptic_quantum_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsQuantumIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_hh_config_t hh_config = {
            .g_Na = 120.0f, .g_K = 36.0f, .g_L = 0.3f,
            .g_Ca_L = 0.0f, .g_Ca_T = 0.0f, .g_K_Ca = 0.0f,
            .g_K_A = 0.0f, .g_H = 0.0f,
            .E_Na = 50.0f, .E_K = -77.0f, .E_L = -54.4f,
            .E_Ca = 120.0f, .E_H = -30.0f,
            .C_m = 1.0f, .V_rest = -65.0f,
            .temperature = 37.0f, .surface_area = 0.01f,
            .length = 100.0f, .diameter = 10.0f,
            .enable_calcium = false, .enable_adaptation = false,
            .enable_h_current = false,
            .dt_max = 0.1f, .adaptive_dt = false
        };
        hh_initialized_ = (nimcp_hh_population_create(&hh_pop_, 10, &hh_config) == NIMCP_SUCCESS);

        nimcp_thermo_config_t thermo_config = nimcp_thermo_default_config();
        thermo_initialized_ = (nimcp_thermo_init(&thermo_state_, &thermo_config) == NIMCP_SUCCESS);

        nimcp_ephaptic_config_t eph_config = nimcp_ephaptic_default_config();
        ephaptic_initialized_ = (nimcp_ephaptic_init(&ephaptic_, &eph_config) == NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (hh_initialized_) nimcp_hh_population_destroy(&hh_pop_);
        if (thermo_initialized_) nimcp_thermo_destroy(&thermo_state_);
        if (ephaptic_initialized_) nimcp_ephaptic_destroy(&ephaptic_);
    }

    nimcp_hh_population_t hh_pop_;
    nimcp_thermodynamic_state_t thermo_state_;
    nimcp_ephaptic_system_t ephaptic_;
    bool hh_initialized_ = false;
    bool thermo_initialized_ = false;
    bool ephaptic_initialized_ = false;
};

//=============================================================================
// HH QMC Tests
//=============================================================================

TEST_F(PhysicsQuantumIntegrationTest, HHQMCDefaultConfig) {
    hh_qmc_config_t config;
    EXPECT_EQ(hh_qmc_default_config(&config), 0);
    EXPECT_GT(config.num_iterations, 0U);
    EXPECT_GT(config.initial_temp, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, HHQMCDefaultTarget) {
    hh_qmc_target_t target;
    EXPECT_EQ(hh_qmc_default_target(&target), 0);
    EXPECT_GT(target.target_firing_rate, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, HHEntropyDefaultConfig) {
    hh_entropy_config_t config;
    EXPECT_EQ(hh_entropy_default_config(&config), 0);
    EXPECT_GT(config.num_samples, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, HHStochasticDefaultConfig) {
    hh_stochastic_config_t config;
    EXPECT_EQ(hh_stochastic_default_config(&config), 0);
    EXPECT_GT(config.num_channels, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, HHQMCPopulationCoherence) {
    if (!hh_initialized_) GTEST_SKIP() << "HH not initialized";

    float coherence = 0.0f;
    float entropy = 0.0f;

    int result = hh_qmc_population_coherence(&hh_pop_, &coherence, &entropy);
    EXPECT_EQ(result, 0);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
    EXPECT_GE(entropy, 0.0f);
}

//=============================================================================
// Thermo QMC Tests
//=============================================================================

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCPartitionDefaultConfig) {
    thermo_partition_config_t config;
    EXPECT_EQ(thermo_qmc_partition_default_config(&config), 0);
    EXPECT_GT(config.num_samples, 0U);
    EXPECT_GT(config.temperature, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCLandauerDefaultConfig) {
    thermo_landauer_config_t config;
    EXPECT_EQ(thermo_qmc_landauer_default_config(&config), 0);
    EXPECT_GT(config.num_iterations, 0U);
    EXPECT_GT(config.initial_temp, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCATPDefaultConfig) {
    thermo_atp_config_t config;
    EXPECT_EQ(thermo_qmc_atp_default_config(&config), 0);
    EXPECT_GT(config.num_iterations, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCEntropyDefaultConfig) {
    thermo_entropy_config_t config;
    EXPECT_EQ(thermo_qmc_entropy_default_config(&config), 0);
    EXPECT_GT(config.num_samples, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCEstimatePartition) {
    if (!thermo_initialized_) GTEST_SKIP() << "Thermo not initialized";

    thermo_partition_config_t config;
    thermo_qmc_partition_default_config(&config);
    config.num_samples = 100;  // Reduced for test speed

    thermo_partition_result_t result;
    int ret = thermo_qmc_estimate_partition(&thermo_state_, &config, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(result.log_Z));
    EXPECT_TRUE(std::isfinite(result.free_energy));
}

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCLandauerLimit) {
    float energy_per_bit = 0.0f;
    int result = thermo_qmc_landauer_limit(310.15f, &energy_per_bit);
    EXPECT_EQ(result, 0);
    EXPECT_GT(energy_per_bit, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, ThermoQMCOptimizeLandauer) {
    if (!thermo_initialized_) GTEST_SKIP() << "Thermo not initialized";

    thermo_landauer_config_t config;
    thermo_qmc_landauer_default_config(&config);
    config.num_iterations = 50;  // Reduced for test

    thermo_landauer_result_t result;
    int ret = thermo_qmc_optimize_landauer(&thermo_state_, &config, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(result.optimal_temperature, 0.0f);
    EXPECT_GE(result.landauer_efficiency, 0.0f);
    EXPECT_LE(result.landauer_efficiency, 1.0f);
}

//=============================================================================
// Ephaptic QMC Tests
//=============================================================================

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCWalkDefaultConfig) {
    ephaptic_walk_config_t config;
    EXPECT_EQ(ephaptic_qmc_walk_default_config(&config), 0);
    EXPECT_GT(config.max_steps, 0U);
    EXPECT_GT(config.mcts_iterations, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCCoherenceDefaultConfig) {
    ephaptic_coherence_config_t config;
    EXPECT_EQ(ephaptic_qmc_coherence_default_config(&config), 0);
    EXPECT_GT(config.num_iterations, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCCoherenceDefaultTarget) {
    ephaptic_coherence_target_t target;
    EXPECT_EQ(ephaptic_qmc_coherence_default_target(&target), 0);
    EXPECT_GT(target.target_coherence, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCEntropyDefaultConfig) {
    ephaptic_entropy_config_t config;
    EXPECT_EQ(ephaptic_qmc_entropy_default_config(&config), 0);
    EXPECT_GT(config.num_samples, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCPatternDefaultConfig) {
    ephaptic_pattern_config_t config;
    EXPECT_EQ(ephaptic_qmc_pattern_default_config(&config), 0);
    EXPECT_GT(config.mcts_iterations, 0U);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCCurrentCoherence) {
    if (!ephaptic_initialized_) GTEST_SKIP() << "Ephaptic not initialized";

    float coherence = 0.0f;
    int result = ephaptic_qmc_current_coherence(&ephaptic_, &coherence);
    EXPECT_EQ(result, 0);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCPropagationSpeed) {
    if (!ephaptic_initialized_) GTEST_SKIP() << "Ephaptic not initialized";

    float speed = 0.0f;
    int result = ephaptic_qmc_propagation_speed(&ephaptic_, &speed);
    EXPECT_EQ(result, 0);
    EXPECT_GE(speed, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCCorrelationLength) {
    if (!ephaptic_initialized_) GTEST_SKIP() << "Ephaptic not initialized";

    float corr_length = 0.0f;
    int result = ephaptic_qmc_correlation_length(&ephaptic_, &corr_length);
    EXPECT_EQ(result, 0);
    EXPECT_GE(corr_length, 0.0f);
}

TEST_F(PhysicsQuantumIntegrationTest, EphapticQMCFieldCapacity) {
    if (!ephaptic_initialized_) GTEST_SKIP() << "Ephaptic not initialized";

    float capacity = 0.0f;
    int result = ephaptic_qmc_field_capacity(&ephaptic_, &capacity);
    EXPECT_EQ(result, 0);
    EXPECT_GE(capacity, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
