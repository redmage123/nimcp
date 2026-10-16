/**
 * @file test_ephaptic_regression.cpp
 * @brief Regression tests for ephaptic coupling module
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
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class EphapticRegressionTest : public ::testing::Test {
protected:
    nimcp_ephaptic_system_t system;
    nimcp_ephaptic_config_t config;

    void SetUp() override {
        config = nimcp_ephaptic_default_config();
        nimcp_error_t err = nimcp_ephaptic_init(&system, &config);
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_ephaptic_destroy(&system);
    }

    // Helper to add test neurons
    void AddTestNeurons(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            nimcp_ephaptic_neuron_t neuron = {};
            neuron.id = i;
            neuron.position[0] = static_cast<float>(i % 10) * 0.1f;
            neuron.position[1] = static_cast<float>((i / 10) % 10) * 0.1f;
            neuron.position[2] = static_cast<float>(i / 100) * 0.1f;
            neuron.membrane_potential = -65.0f + static_cast<float>(i % 20);
            neuron.phase = static_cast<float>(i) / count * 2.0f * M_PI;
            neuron.natural_frequency = 40.0f;  // 40Hz gamma
            neuron.field_susceptibility = 1.0f;
            neuron.is_spiking = false;

            nimcp_ephaptic_add_neuron(&system, &neuron);
        }
    }
};

//=============================================================================
// Test 1: Deterministic Output
//=============================================================================

TEST_F(EphapticRegressionTest, DeterministicOutput_SameNeuronsProduceSameField) {
    // Create two systems with identical neurons
    nimcp_ephaptic_system_t system2;
    nimcp_ephaptic_init(&system2, &config);

    // Add identical neurons to both
    const uint32_t NUM_NEURONS = 50;
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        nimcp_ephaptic_neuron_t neuron = {};
        neuron.id = i;
        neuron.position[0] = static_cast<float>(i % 10) * 0.1f;
        neuron.position[1] = static_cast<float>((i / 10) % 10) * 0.1f;
        neuron.position[2] = 0.0f;
        neuron.membrane_potential = -65.0f + static_cast<float>(i % 30);
        neuron.phase = static_cast<float>(i) / NUM_NEURONS * 2.0f * M_PI;
        neuron.natural_frequency = 40.0f;
        neuron.field_susceptibility = 1.0f;
        neuron.is_spiking = (i % 10 == 0);

        nimcp_ephaptic_add_neuron(&system, &neuron);
        nimcp_ephaptic_add_neuron(&system2, &neuron);
    }

    // Update fields
    nimcp_ephaptic_update_field(&system, 0.1f);
    nimcp_ephaptic_update_field(&system2, 0.1f);

    // Compare field values
    float field1[3], field2[3];
    nimcp_ephaptic_get_field(&system, field1);
    nimcp_ephaptic_get_field(&system2, field2);

    EXPECT_FLOAT_EQ(field1[0], field2[0]);
    EXPECT_FLOAT_EQ(field1[1], field2[1]);
    EXPECT_FLOAT_EQ(field1[2], field2[2]);

    nimcp_ephaptic_destroy(&system2);
}

TEST_F(EphapticRegressionTest, DeterministicOutput_PhaseCoherenceConsistent) {
    AddTestNeurons(100);

    // Run synchronization
    for (int i = 0; i < 10; i++) {
        nimcp_ephaptic_synchronize(&system, 0.1f);
    }

    float coherence1, coherence2;
    nimcp_ephaptic_get_phase_coherence(&system, &coherence1);
    nimcp_ephaptic_get_phase_coherence(&system, &coherence2);

    EXPECT_FLOAT_EQ(coherence1, coherence2);
}

//=============================================================================
// Test 2: Numerical Stability
//=============================================================================

TEST_F(EphapticRegressionTest, NumericalStability_NoNaNInfInFieldComputation) {
    AddTestNeurons(100);

    // Run many update cycles
    for (int i = 0; i < 1000; i++) {
        nimcp_error_t err = nimcp_ephaptic_update_field(&system, 0.1f);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        float field[3];
        nimcp_ephaptic_get_field(&system, field);

        EXPECT_FALSE(std::isnan(field[0])) << "NaN field[0] at step " << i;
        EXPECT_FALSE(std::isnan(field[1])) << "NaN field[1] at step " << i;
        EXPECT_FALSE(std::isnan(field[2])) << "NaN field[2] at step " << i;
        EXPECT_FALSE(std::isinf(field[0])) << "Inf field[0] at step " << i;
        EXPECT_FALSE(std::isinf(field[1])) << "Inf field[1] at step " << i;
        EXPECT_FALSE(std::isinf(field[2])) << "Inf field[2] at step " << i;
    }
}

TEST_F(EphapticRegressionTest, NumericalStability_LFPComputationStable) {
    AddTestNeurons(50);

    // Update field first
    nimcp_ephaptic_update_field(&system, 0.1f);

    // Compute LFP at various positions
    std::vector<std::array<float, 3>> positions = {
        {0.0f, 0.0f, 0.0f},
        {0.5f, 0.5f, 0.5f},
        {1.0f, 1.0f, 1.0f},
        {-0.5f, -0.5f, -0.5f}
    };

    for (const auto& pos : positions) {
        nimcp_lfp_result_t result;
        nimcp_error_t err = nimcp_ephaptic_compute_lfp(&system, pos.data(), &result);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        EXPECT_FALSE(std::isnan(result.amplitude));
        EXPECT_FALSE(std::isinf(result.amplitude));
        EXPECT_FALSE(std::isnan(result.dominant_frequency));
        EXPECT_FALSE(std::isnan(result.phase));
    }
}

TEST_F(EphapticRegressionTest, NumericalStability_SynchronizationStable) {
    AddTestNeurons(100);

    // Run many synchronization steps
    for (int i = 0; i < 1000; i++) {
        nimcp_error_t err = nimcp_ephaptic_synchronize(&system, 0.1f);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        float coherence;
        nimcp_ephaptic_get_phase_coherence(&system, &coherence);

        EXPECT_FALSE(std::isnan(coherence)) << "NaN coherence at step " << i;
        EXPECT_FALSE(std::isinf(coherence)) << "Inf coherence at step " << i;
    }
}

//=============================================================================
// Test 3: Physical Bounds
//=============================================================================

TEST_F(EphapticRegressionTest, PhysicalBounds_FieldStrengthWithinLimits) {
    AddTestNeurons(100);

    // Add some spiking neurons for strong field
    for (uint32_t i = 0; i < 20; i++) {
        nimcp_ephaptic_update_neuron(&system, i, 30.0f, 0.0f, true);
    }

    nimcp_ephaptic_update_field(&system, 0.1f);

    float field[3];
    nimcp_ephaptic_get_field(&system, field);

    // Field strength should not exceed physical maximum
    float magnitude = std::sqrt(field[0]*field[0] + field[1]*field[1] + field[2]*field[2]);
    EXPECT_LE(magnitude, EPHAPTIC_MAX_FIELD_STRENGTH)
        << "Field magnitude exceeds physical limit";
}

TEST_F(EphapticRegressionTest, PhysicalBounds_PhaseCoherenceBetweenZeroAndOne) {
    AddTestNeurons(100);

    // Test coherence at various stages
    for (int i = 0; i < 100; i++) {
        nimcp_ephaptic_synchronize(&system, 0.1f);

        float coherence;
        nimcp_ephaptic_get_phase_coherence(&system, &coherence);

        EXPECT_GE(coherence, 0.0f) << "Coherence < 0 at step " << i;
        EXPECT_LE(coherence, 1.0f) << "Coherence > 1 at step " << i;
    }
}

TEST_F(EphapticRegressionTest, PhysicalBounds_PolarizationReasonable) {
    AddTestNeurons(50);

    // Update field
    nimcp_ephaptic_update_field(&system, 0.1f);

    // Check polarization for each neuron
    for (uint32_t i = 0; i < 50; i++) {
        float polarization;
        nimcp_error_t err = nimcp_ephaptic_modulate_neuron(&system, i, &polarization);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Polarization should be small (few mV at most)
        EXPECT_GE(polarization, -10.0f) << "Polarization too negative for neuron " << i;
        EXPECT_LE(polarization, 10.0f) << "Polarization too positive for neuron " << i;
    }
}

//=============================================================================
// Test 4: Backward Compatibility (API Signatures)
//=============================================================================

TEST_F(EphapticRegressionTest, BackwardCompatibility_DefaultConfigReturnsValidStruct) {
    nimcp_ephaptic_config_t test_config = nimcp_ephaptic_default_config();

    // Verify default values are sensible
    EXPECT_FLOAT_EQ(test_config.coupling_strength, EPHAPTIC_DEFAULT_COUPLING_STRENGTH);
    EXPECT_FLOAT_EQ(test_config.field_decay_constant, EPHAPTIC_DEFAULT_FIELD_DECAY);
    EXPECT_FLOAT_EQ(test_config.sync_threshold, EPHAPTIC_DEFAULT_SYNC_THRESHOLD);
}

TEST_F(EphapticRegressionTest, BackwardCompatibility_InitDestroyPattern) {
    // Test standard init/destroy lifecycle
    nimcp_ephaptic_system_t test_system;
    nimcp_ephaptic_config_t test_config = nimcp_ephaptic_default_config();

    nimcp_error_t err = nimcp_ephaptic_init(&test_system, &test_config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(nimcp_ephaptic_is_initialized(&test_system));

    nimcp_ephaptic_destroy(&test_system);
}

TEST_F(EphapticRegressionTest, BackwardCompatibility_NullConfigUsesDefaults) {
    // Passing NULL config should use defaults
    nimcp_ephaptic_system_t test_system;
    nimcp_error_t err = nimcp_ephaptic_init(&test_system, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_ephaptic_destroy(&test_system);
}

TEST_F(EphapticRegressionTest, BackwardCompatibility_ResetRestoresInitialState) {
    AddTestNeurons(50);

    // Modify state
    nimcp_ephaptic_update_field(&system, 0.1f);
    nimcp_ephaptic_synchronize(&system, 0.1f);

    // Reset
    nimcp_error_t err = nimcp_ephaptic_reset(&system);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify field is reset
    float field[3];
    nimcp_ephaptic_get_field(&system, field);

    EXPECT_FLOAT_EQ(field[0], 0.0f);
    EXPECT_FLOAT_EQ(field[1], 0.0f);
    EXPECT_FLOAT_EQ(field[2], 0.0f);
}

//=============================================================================
// Test 5: Performance Regression
//=============================================================================

TEST_F(EphapticRegressionTest, PerformanceRegression_FieldUpdateLatency) {
    AddTestNeurons(100);

    const int NUM_ITERATIONS = 1000;
    std::vector<double> latencies;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_ephaptic_update_field(&system, 0.1f);

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

    // Performance targets (microseconds) for 100 neurons
    EXPECT_LT(mean, 100.0) << "Mean field update latency too high: " << mean << "us";
    EXPECT_LT(p95, 200.0) << "P95 field update latency too high: " << p95 << "us";
    EXPECT_LT(p99, 500.0) << "P99 field update latency too high: " << p99 << "us";
}

TEST_F(EphapticRegressionTest, PerformanceRegression_SynchronizationLatency) {
    AddTestNeurons(100);

    const int NUM_ITERATIONS = 1000;
    std::vector<double> latencies;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_ephaptic_synchronize(&system, 0.1f);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();

    std::sort(latencies.begin(), latencies.end());
    double p95 = latencies[static_cast<size_t>(0.95 * latencies.size())];

    // Synchronization is O(N^2) in naive implementation, allow more time
    EXPECT_LT(mean, 500.0) << "Mean sync latency too high: " << mean << "us";
    EXPECT_LT(p95, 1000.0) << "P95 sync latency too high: " << p95 << "us";
}

TEST_F(EphapticRegressionTest, PerformanceRegression_LFPComputationLatency) {
    AddTestNeurons(100);
    nimcp_ephaptic_update_field(&system, 0.1f);

    const int NUM_ITERATIONS = 1000;
    std::vector<double> latencies;
    float position[3] = {0.5f, 0.5f, 0.0f};

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_lfp_result_t result;

        auto start = std::chrono::high_resolution_clock::now();

        nimcp_ephaptic_compute_lfp(&system, position, &result);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();

    // LFP computation should be fast
    EXPECT_LT(mean, 50.0) << "Mean LFP latency too high: " << mean << "us";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
