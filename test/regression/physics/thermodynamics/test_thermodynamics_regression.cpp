/**
 * @file test_thermodynamics_regression.cpp
 * @brief Regression tests for non-equilibrium thermodynamics module
 *
 * Tests for:
 * 1. Deterministic output - Same input produces same output
 * 2. Numerical stability - No NaN/Inf values
 * 3. Physical bounds - Values stay within realistic ranges
 * 4. Backward compatibility - API signatures unchanged
 * 5. Performance regression - Execution time bounds
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>

extern "C" {
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ThermodynamicsRegressionTest : public ::testing::Test {
protected:
    nimcp_thermodynamic_state_t state;
    nimcp_thermo_config_t config;

    void SetUp() override {
        config = nimcp_thermo_default_config();
        nimcp_error_t err = nimcp_thermo_init(&state, &config);
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_thermo_destroy(&state);
    }
};

//=============================================================================
// Test 1: Deterministic Output
//=============================================================================

TEST_F(ThermodynamicsRegressionTest, DeterministicOutput_SameUpdateProducesSameState) {
    // Run identical updates and verify same results
    nimcp_thermodynamic_state_t state1, state2;

    nimcp_thermo_init(&state1, &config);
    nimcp_thermo_init(&state2, &config);

    // Apply identical updates
    const double DT = 0.001;        // 1ms timestep
    const double POWER = 1e-9;      // 1nW power consumption
    const uint64_t BITS = 1000;     // bits erased

    for (int i = 0; i < 100; i++) {
        nimcp_thermo_update(&state1, DT, POWER, BITS);
        nimcp_thermo_update(&state2, DT, POWER, BITS);
    }

    // Verify identical states
    EXPECT_DOUBLE_EQ(state1.total_energy_consumed, state2.total_energy_consumed);
    EXPECT_DOUBLE_EQ(state1.total_entropy_produced, state2.total_entropy_produced);
    EXPECT_DOUBLE_EQ(state1.atp_available, state2.atp_available);

    nimcp_thermo_destroy(&state1);
    nimcp_thermo_destroy(&state2);
}

TEST_F(ThermodynamicsRegressionTest, DeterministicOutput_LandauerCostConsistent) {
    // Landauer cost computation should be deterministic
    nimcp_landauer_cost_t cost1, cost2;

    nimcp_error_t err1 = nimcp_thermo_compute_landauer_cost(310.0, 1000, &cost1);
    nimcp_error_t err2 = nimcp_thermo_compute_landauer_cost(310.0, 1000, &cost2);

    ASSERT_EQ(err1, NIMCP_SUCCESS);
    ASSERT_EQ(err2, NIMCP_SUCCESS);

    EXPECT_DOUBLE_EQ(cost1.minimum_cost, cost2.minimum_cost);
    EXPECT_EQ(cost1.bits_erased, cost2.bits_erased);
}

//=============================================================================
// Test 2: Numerical Stability
//=============================================================================

TEST_F(ThermodynamicsRegressionTest, NumericalStability_LongSimulationNoOverflow) {
    // Run many updates without numerical issues
    const double DT = 0.001;
    const double POWER = 1e-9;
    const uint64_t BITS = 100;
    const int NUM_STEPS = 100000;

    for (int i = 0; i < NUM_STEPS; i++) {
        nimcp_error_t err = nimcp_thermo_update(&state, DT, POWER, BITS);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_FALSE(std::isnan(state.total_energy_consumed))
            << "NaN energy at step " << i;
        EXPECT_FALSE(std::isinf(state.total_energy_consumed))
            << "Inf energy at step " << i;
        EXPECT_FALSE(std::isnan(state.entropy_production_rate))
            << "NaN entropy rate at step " << i;
    }
}

TEST_F(ThermodynamicsRegressionTest, NumericalStability_ExtremeTemperatures) {
    // Test Landauer cost at extreme temperatures
    std::vector<double> temperatures = {1.0, 100.0, 310.0, 1000.0, 10000.0};

    for (double T : temperatures) {
        nimcp_landauer_cost_t cost;
        nimcp_error_t err = nimcp_thermo_compute_landauer_cost(T, 1000, &cost);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_FALSE(std::isnan(cost.minimum_cost))
            << "NaN Landauer cost at T=" << T;
        EXPECT_FALSE(std::isinf(cost.minimum_cost))
            << "Inf Landauer cost at T=" << T;
        EXPECT_GT(cost.minimum_cost, 0.0)
            << "Non-positive Landauer cost at T=" << T;
    }
}

TEST_F(ThermodynamicsRegressionTest, NumericalStability_EntropyRateComputation) {
    // Ensure entropy rate computation is stable
    nimcp_thermo_update(&state, 0.001, 1e-6, 1000000);  // High power, many bits

    double entropy_rate;
    nimcp_error_t err = nimcp_thermo_compute_entropy_rate(&state, 310.0, &entropy_rate);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(std::isnan(entropy_rate));
    EXPECT_FALSE(std::isinf(entropy_rate));
}

//=============================================================================
// Test 3: Physical Bounds
//=============================================================================

TEST_F(ThermodynamicsRegressionTest, PhysicalBounds_EnergyMonotonicallyIncreases) {
    // Total energy consumed should only increase
    double prev_energy = state.total_energy_consumed;

    for (int i = 0; i < 100; i++) {
        nimcp_thermo_update(&state, 0.001, 1e-9, 100);
        EXPECT_GE(state.total_energy_consumed, prev_energy)
            << "Energy decreased at step " << i;
        prev_energy = state.total_energy_consumed;
    }
}

TEST_F(ThermodynamicsRegressionTest, PhysicalBounds_ATPDepletion) {
    // ATP should deplete during consumption
    double initial_atp = state.atp_available;

    // Consume significant power
    for (int i = 0; i < 1000; i++) {
        nimcp_thermo_update(&state, 0.001, 1e-6, 1000);
    }

    EXPECT_LT(state.atp_available, initial_atp)
        << "ATP did not deplete during high consumption";
    EXPECT_GE(state.atp_available, 0.0)
        << "ATP went negative";
}

TEST_F(ThermodynamicsRegressionTest, PhysicalBounds_EfficiencyBetweenZeroAndOne) {
    // Run some computation to generate efficiency metrics
    for (int i = 0; i < 100; i++) {
        nimcp_thermo_update(&state, 0.001, 1e-9, 100);
    }

    double comp_eff, thermo_eff, landauer_eff;
    nimcp_error_t err = nimcp_thermo_get_efficiency(&state, &comp_eff, &thermo_eff, &landauer_eff);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Efficiency should be in [0, 1]
    EXPECT_GE(comp_eff, 0.0);
    EXPECT_LE(comp_eff, 1.0);
    EXPECT_GE(thermo_eff, 0.0);
    EXPECT_LE(thermo_eff, 1.0);
    EXPECT_GE(landauer_eff, 0.0);
    EXPECT_LE(landauer_eff, 1.0);
}

TEST_F(ThermodynamicsRegressionTest, PhysicalBounds_LandauerLimitRespected) {
    // Verify Landauer limit formula: E = kT * ln(2) * n_bits
    const double T = 310.0;  // Body temperature
    const uint64_t BITS = 1000000;

    nimcp_landauer_cost_t cost;
    nimcp_error_t err = nimcp_thermo_compute_landauer_cost(T, BITS, &cost);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Calculate expected Landauer limit
    double expected = NIMCP_THERMO_KB * T * std::log(2.0) * BITS;

    // Allow 1% tolerance for floating point
    EXPECT_NEAR(cost.minimum_cost, expected, expected * 0.01);
}

//=============================================================================
// Test 4: Backward Compatibility (API Signatures)
//=============================================================================

TEST_F(ThermodynamicsRegressionTest, BackwardCompatibility_DefaultConfigReturnsValidStruct) {
    nimcp_thermo_config_t test_config = nimcp_thermo_default_config();

    // Verify default values are sensible
    EXPECT_DOUBLE_EQ(test_config.temperature_k, NIMCP_THERMO_DEFAULT_TEMP_K);
    EXPECT_GT(test_config.atp_pool_size, 0.0);
    EXPECT_GT(test_config.metabolic_rate, 0.0);
}

TEST_F(ThermodynamicsRegressionTest, BackwardCompatibility_InitDestroyPattern) {
    // Test standard init/destroy lifecycle
    nimcp_thermodynamic_state_t test_state;
    nimcp_thermo_config_t test_config = nimcp_thermo_default_config();

    nimcp_error_t err = nimcp_thermo_init(&test_state, &test_config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(test_state.initialized);

    nimcp_thermo_destroy(&test_state);
}

TEST_F(ThermodynamicsRegressionTest, BackwardCompatibility_NullConfigUsesDefaults) {
    // Passing NULL config should use defaults
    nimcp_thermodynamic_state_t test_state;
    nimcp_error_t err = nimcp_thermo_init(&test_state, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_thermo_destroy(&test_state);
}

TEST_F(ThermodynamicsRegressionTest, BackwardCompatibility_ATPConversionFunctions) {
    // Test ATP/energy conversion functions
    double atp = 1e-12;  // 1 picomole
    double energy = nimcp_thermo_atp_to_energy(atp);

    EXPECT_GT(energy, 0.0);
    EXPECT_FALSE(std::isnan(energy));

    double atp_back = nimcp_thermo_energy_to_atp(energy);
    EXPECT_NEAR(atp, atp_back, atp * 0.001);  // Round-trip accuracy
}

//=============================================================================
// Test 5: Performance Regression
//=============================================================================

TEST_F(ThermodynamicsRegressionTest, PerformanceRegression_UpdateLatency) {
    const int NUM_ITERATIONS = 10000;

    std::vector<double> latencies;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_thermo_update(&state, 0.001, 1e-9, 100);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    // Calculate statistics
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();

    std::sort(latencies.begin(), latencies.end());
    double p95 = latencies[static_cast<size_t>(0.95 * latencies.size())];
    double p99 = latencies[static_cast<size_t>(0.99 * latencies.size())];

    // Performance targets (microseconds)
    EXPECT_LT(mean, 5.0) << "Mean latency too high: " << mean << "us";
    EXPECT_LT(p95, 10.0) << "P95 latency too high: " << p95 << "us";
    EXPECT_LT(p99, 20.0) << "P99 latency too high: " << p99 << "us";
}

TEST_F(ThermodynamicsRegressionTest, PerformanceRegression_LandauerComputationLatency) {
    const int NUM_ITERATIONS = 10000;

    std::vector<double> latencies;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_landauer_cost_t cost;

        auto start = std::chrono::high_resolution_clock::now();

        nimcp_thermo_compute_landauer_cost(310.0, 1000, &cost);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();

    // Landauer computation should be very fast
    EXPECT_LT(mean, 2.0) << "Mean Landauer latency too high: " << mean << "us";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
