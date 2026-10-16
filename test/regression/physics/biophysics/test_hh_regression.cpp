/**
 * @file test_hh_regression.cpp
 * @brief Regression tests for Hodgkin-Huxley biophysics module
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
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class HHRegressionTest : public ::testing::Test {
protected:
    nimcp_hh_neuron_t neuron;
    nimcp_hh_config_t config;

    void SetUp() override {
        nimcp_error_t err = nimcp_hh_config_default(&config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        err = nimcp_hh_neuron_init(&neuron, &config);
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_hh_neuron_destroy(&neuron);
    }
};

//=============================================================================
// Test 1: Deterministic Output
//=============================================================================

TEST_F(HHRegressionTest, DeterministicOutput_SameInputProducesSameVoltage) {
    // Run simulation with specific input and record voltages
    const int NUM_STEPS = 100;
    const float I_EXT = 10.0f;  // Suprathreshold current
    const float DT = 0.01f;     // 10us timestep

    std::vector<float> voltages_run1;
    std::vector<float> voltages_run2;

    // First run
    nimcp_hh_neuron_reset(&neuron);
    for (int i = 0; i < NUM_STEPS; i++) {
        nimcp_hh_neuron_update(&neuron, I_EXT, DT);
        voltages_run1.push_back(nimcp_hh_neuron_get_voltage(&neuron));
    }

    // Second run with identical parameters
    nimcp_hh_neuron_reset(&neuron);
    for (int i = 0; i < NUM_STEPS; i++) {
        nimcp_hh_neuron_update(&neuron, I_EXT, DT);
        voltages_run2.push_back(nimcp_hh_neuron_get_voltage(&neuron));
    }

    // Verify exact match (deterministic)
    for (size_t i = 0; i < voltages_run1.size(); i++) {
        EXPECT_FLOAT_EQ(voltages_run1[i], voltages_run2[i])
            << "Voltage mismatch at step " << i;
    }
}

TEST_F(HHRegressionTest, DeterministicOutput_GatingVariablesConsistent) {
    // Verify gating variable computations are deterministic
    const float TEST_VOLTAGE = -40.0f;

    float m_inf1 = nimcp_hh_m_inf(TEST_VOLTAGE);
    float m_inf2 = nimcp_hh_m_inf(TEST_VOLTAGE);
    EXPECT_FLOAT_EQ(m_inf1, m_inf2);

    float h_inf1 = nimcp_hh_h_inf(TEST_VOLTAGE);
    float h_inf2 = nimcp_hh_h_inf(TEST_VOLTAGE);
    EXPECT_FLOAT_EQ(h_inf1, h_inf2);

    float n_inf1 = nimcp_hh_n_inf(TEST_VOLTAGE);
    float n_inf2 = nimcp_hh_n_inf(TEST_VOLTAGE);
    EXPECT_FLOAT_EQ(n_inf1, n_inf2);
}

//=============================================================================
// Test 2: Numerical Stability
//=============================================================================

TEST_F(HHRegressionTest, NumericalStability_NoNaNInfDuringSpike) {
    // Drive neuron to spike and verify no NaN/Inf
    const float I_EXT = 15.0f;  // Strong current to elicit spike
    const float DT = 0.01f;
    const int NUM_STEPS = 500;  // 5ms simulation

    for (int i = 0; i < NUM_STEPS; i++) {
        nimcp_error_t err = nimcp_hh_neuron_update(&neuron, I_EXT, DT);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        float V = nimcp_hh_neuron_get_voltage(&neuron);
        EXPECT_FALSE(std::isnan(V)) << "NaN voltage at step " << i;
        EXPECT_FALSE(std::isinf(V)) << "Inf voltage at step " << i;
    }
}

TEST_F(HHRegressionTest, NumericalStability_GatingVariablesAtExtremeVoltages) {
    // Test gating variable computations at extreme voltages
    std::vector<float> test_voltages = {-200.0f, -100.0f, -65.0f, 0.0f, 50.0f, 100.0f, 200.0f};

    for (float V : test_voltages) {
        float m_inf = nimcp_hh_m_inf(V);
        float h_inf = nimcp_hh_h_inf(V);
        float n_inf = nimcp_hh_n_inf(V);
        float m_tau = nimcp_hh_m_tau(V);
        float h_tau = nimcp_hh_h_tau(V);
        float n_tau = nimcp_hh_n_tau(V);

        EXPECT_FALSE(std::isnan(m_inf)) << "NaN m_inf at V=" << V;
        EXPECT_FALSE(std::isnan(h_inf)) << "NaN h_inf at V=" << V;
        EXPECT_FALSE(std::isnan(n_inf)) << "NaN n_inf at V=" << V;
        EXPECT_FALSE(std::isnan(m_tau)) << "NaN m_tau at V=" << V;
        EXPECT_FALSE(std::isnan(h_tau)) << "NaN h_tau at V=" << V;
        EXPECT_FALSE(std::isnan(n_tau)) << "NaN n_tau at V=" << V;

        EXPECT_FALSE(std::isinf(m_inf)) << "Inf m_inf at V=" << V;
        EXPECT_FALSE(std::isinf(h_inf)) << "Inf h_inf at V=" << V;
        EXPECT_FALSE(std::isinf(n_inf)) << "Inf n_inf at V=" << V;
    }
}

//=============================================================================
// Test 3: Physical Bounds
//=============================================================================

TEST_F(HHRegressionTest, PhysicalBounds_VoltageWithinRealisticRange) {
    // Voltage should stay within biological limits (-100mV to +60mV)
    const float I_EXT = 20.0f;  // Strong current
    const float DT = 0.01f;
    const int NUM_STEPS = 1000;  // 10ms simulation

    for (int i = 0; i < NUM_STEPS; i++) {
        nimcp_hh_neuron_update(&neuron, I_EXT, DT);
        float V = nimcp_hh_neuron_get_voltage(&neuron);

        EXPECT_GE(V, -100.0f) << "Voltage below -100mV at step " << i;
        EXPECT_LE(V, 60.0f) << "Voltage above +60mV at step " << i;
    }
}

TEST_F(HHRegressionTest, PhysicalBounds_GatingVariablesBetweenZeroAndOne) {
    // All gating variables must be in [0, 1]
    std::vector<float> test_voltages = {-80.0f, -65.0f, -40.0f, -20.0f, 0.0f, 20.0f, 40.0f};

    for (float V : test_voltages) {
        float m_inf = nimcp_hh_m_inf(V);
        float h_inf = nimcp_hh_h_inf(V);
        float n_inf = nimcp_hh_n_inf(V);

        EXPECT_GE(m_inf, 0.0f) << "m_inf < 0 at V=" << V;
        EXPECT_LE(m_inf, 1.0f) << "m_inf > 1 at V=" << V;
        EXPECT_GE(h_inf, 0.0f) << "h_inf < 0 at V=" << V;
        EXPECT_LE(h_inf, 1.0f) << "h_inf > 1 at V=" << V;
        EXPECT_GE(n_inf, 0.0f) << "n_inf < 0 at V=" << V;
        EXPECT_LE(n_inf, 1.0f) << "n_inf > 1 at V=" << V;
    }
}

TEST_F(HHRegressionTest, PhysicalBounds_TimeConstantsPositive) {
    // Time constants must be positive
    std::vector<float> test_voltages = {-80.0f, -65.0f, -40.0f, 0.0f, 40.0f};

    for (float V : test_voltages) {
        float m_tau = nimcp_hh_m_tau(V);
        float h_tau = nimcp_hh_h_tau(V);
        float n_tau = nimcp_hh_n_tau(V);

        EXPECT_GT(m_tau, 0.0f) << "m_tau <= 0 at V=" << V;
        EXPECT_GT(h_tau, 0.0f) << "h_tau <= 0 at V=" << V;
        EXPECT_GT(n_tau, 0.0f) << "n_tau <= 0 at V=" << V;
    }
}

//=============================================================================
// Test 4: Backward Compatibility (API Signatures)
//=============================================================================

TEST_F(HHRegressionTest, BackwardCompatibility_ConfigDefaultReturnsSuccess) {
    nimcp_hh_config_t test_config;
    nimcp_error_t err = nimcp_hh_config_default(&test_config);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify default values are sensible
    EXPECT_FLOAT_EQ(test_config.g_Na, NIMCP_HH_DEFAULT_G_NA);
    EXPECT_FLOAT_EQ(test_config.g_K, NIMCP_HH_DEFAULT_G_K);
    EXPECT_FLOAT_EQ(test_config.g_L, NIMCP_HH_DEFAULT_G_L);
}

TEST_F(HHRegressionTest, BackwardCompatibility_NeuronInitDestroyPattern) {
    // Test standard init/destroy lifecycle
    nimcp_hh_neuron_t test_neuron;
    nimcp_hh_config_t test_config;

    nimcp_error_t err = nimcp_hh_config_default(&test_config);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_hh_neuron_init(&test_neuron, &test_config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(test_neuron.initialized);

    nimcp_hh_neuron_destroy(&test_neuron);
}

TEST_F(HHRegressionTest, BackwardCompatibility_UpdateAPISignature) {
    // Verify update function accepts expected parameters
    nimcp_error_t err = nimcp_hh_neuron_update(&neuron, 0.0f, 0.01f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Test 5: Performance Regression
//=============================================================================

TEST_F(HHRegressionTest, PerformanceRegression_SingleStepLatency) {
    const int NUM_ITERATIONS = 10000;
    const float I_EXT = 10.0f;
    const float DT = 0.01f;

    std::vector<double> latencies;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_hh_neuron_update(&neuron, I_EXT, DT);

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

    // Performance targets (microseconds) - HH update should be fast
    EXPECT_LT(mean, 10.0) << "Mean latency too high: " << mean << "us";
    EXPECT_LT(p95, 20.0) << "P95 latency too high: " << p95 << "us";
    EXPECT_LT(p99, 50.0) << "P99 latency too high: " << p99 << "us";
}

TEST_F(HHRegressionTest, PerformanceRegression_PopulationUpdateLatency) {
    const uint32_t POPULATION_SIZE = 100;
    nimcp_hh_population_t population;

    nimcp_error_t err = nimcp_hh_population_create(&population, POPULATION_SIZE, &config);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> currents(POPULATION_SIZE, 10.0f);
    const int NUM_ITERATIONS = 1000;

    std::vector<double> latencies;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_hh_population_update(&population, currents.data(), 0.01f);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();

    // Population of 100 should update in reasonable time
    EXPECT_LT(mean, 500.0) << "Mean population latency too high: " << mean << "us";

    nimcp_hh_population_destroy(&population);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
