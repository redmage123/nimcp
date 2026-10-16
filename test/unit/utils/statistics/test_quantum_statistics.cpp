//=============================================================================
// test_quantum_statistics.cpp - Unit Tests for Quantum Statistics Module
//=============================================================================
/**
 * @file test_quantum_statistics.cpp
 * @brief Comprehensive unit tests for quantum-enhanced statistics functions
 *
 * Tests cover:
 * - Quantum state creation and management
 * - Born rule probability extraction
 * - Von Neumann entropy and variants
 * - Quantum relative entropy and mutual information
 * - Fidelity and trace distance
 * - Entanglement measures
 * - Quantum Fisher information
 * - Quantum walk integration
 * - Quantum annealing integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_quantum_statistics.h"
#include <cmath>
#include <vector>
#include <limits>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize config
        config = qstats_default_config();
    }

    void TearDown() override {
        // Cleanup handled by individual tests
    }

    qstats_config_t config;
};

//=============================================================================
// State Creation Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, PureState_Create) {
    qstats_pure_state_t* state = qstats_pure_state_create(4);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->dim, 4u);
    EXPECT_TRUE(state->normalized);

    // Default is |0> state
    EXPECT_NEAR(state->amplitudes[0].real, 1.0f, TOLERANCE);
    EXPECT_NEAR(state->amplitudes[0].imag, 0.0f, TOLERANCE);

    qstats_pure_state_destroy(state);
}

TEST_F(QuantumStatisticsTest, PureState_CreateZeroDim) {
    qstats_pure_state_t* state = qstats_pure_state_create(0);
    EXPECT_EQ(state, nullptr);
}

TEST_F(QuantumStatisticsTest, DensityMatrix_Create) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);
    EXPECT_EQ(dm->dim, 4u);

    // Default is |0><0|
    EXPECT_NEAR(dm->elements[0].real, 1.0f, TOLERANCE);
    EXPECT_NEAR(qstats_trace(dm), 1.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, DensityMatrix_FromPure) {
    qstats_pure_state_t* state = qstats_pure_state_create(2);
    ASSERT_NE(state, nullptr);

    // Create superposition |+> = (|0> + |1>)/sqrt(2)
    state->amplitudes[0].real = 1.0f / std::sqrt(2.0f);
    state->amplitudes[1].real = 1.0f / std::sqrt(2.0f);

    qstats_density_matrix_t* dm = qstats_density_matrix_from_pure(state);
    ASSERT_NE(dm, nullptr);

    // ρ = |+><+| should have all elements = 0.5
    EXPECT_NEAR(dm->elements[0].real, 0.5f, TOLERANCE);
    EXPECT_NEAR(dm->elements[1].real, 0.5f, TOLERANCE);
    EXPECT_NEAR(dm->elements[2].real, 0.5f, TOLERANCE);
    EXPECT_NEAR(dm->elements[3].real, 0.5f, TOLERANCE);

    EXPECT_NEAR(qstats_trace(dm), 1.0f, TOLERANCE);

    qstats_pure_state_destroy(state);
    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, DensityMatrix_MaximallyMixed) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    // I/4: diagonal = 0.25, off-diagonal = 0
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(dm->elements[i * 4 + i].real, 0.25f, TOLERANCE);
        for (uint32_t j = 0; j < 4; j++) {
            if (i != j) {
                EXPECT_NEAR(dm->elements[i * 4 + j].real, 0.0f, TOLERANCE);
            }
        }
    }

    EXPECT_NEAR(qstats_trace(dm), 1.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, DensityMatrix_IsValid) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(2);
    ASSERT_NE(dm, nullptr);
    EXPECT_TRUE(qstats_density_matrix_is_valid(dm));
    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Born Rule Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, Born_Probabilities) {
    qstats_pure_state_t* state = qstats_pure_state_create(4);
    ASSERT_NE(state, nullptr);

    // Set up state with known amplitudes
    state->amplitudes[0].real = 0.5f;
    state->amplitudes[1].real = 0.5f;
    state->amplitudes[2].real = 0.5f;
    state->amplitudes[3].real = 0.5f;

    float probs[4];
    qstats_result_t res = qstats_born_probabilities(state, probs);
    EXPECT_EQ(res, QSTATS_OK);

    // Each probability should be 0.25
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(probs[i], 0.25f, TOLERANCE);
    }

    qstats_pure_state_destroy(state);
}

TEST_F(QuantumStatisticsTest, AmplitudeEncode) {
    float probs[] = {0.25f, 0.25f, 0.25f, 0.25f};
    qstats_pure_state_t* state = qstats_amplitude_encode(probs, 4);
    ASSERT_NE(state, nullptr);

    // Check that Born rule gives back the probabilities
    float recovered[4];
    qstats_born_probabilities(state, recovered);

    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(recovered[i], probs[i], TOLERANCE);
    }

    qstats_pure_state_destroy(state);
}

//=============================================================================
// Measurement Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, Measure_Statistical) {
    qstats_pure_state_t* state = qstats_pure_state_create(2);
    ASSERT_NE(state, nullptr);

    // |+> state: equal superposition
    state->amplitudes[0].real = 1.0f / std::sqrt(2.0f);
    state->amplitudes[1].real = 1.0f / std::sqrt(2.0f);

    // Measure many times and check statistics
    uint32_t seed = 12345;
    uint32_t counts[2] = {0, 0};
    const int num_samples = 10000;

    for (int i = 0; i < num_samples; i++) {
        qstats_measurement_t result;
        qstats_measure(state, &result, &seed);
        counts[result.outcome]++;
    }

    // Should be roughly 50-50
    float ratio = (float)counts[0] / (float)num_samples;
    EXPECT_NEAR(ratio, 0.5f, 0.05f);

    qstats_pure_state_destroy(state);
}

TEST_F(QuantumStatisticsTest, Measure_FiniteShots) {
    qstats_pure_state_t* state = qstats_pure_state_create(4);
    ASSERT_NE(state, nullptr);

    // Set unequal probabilities
    state->amplitudes[0].real = 0.5f;
    state->amplitudes[1].real = 0.5f;
    state->amplitudes[2].real = 0.5f;
    state->amplitudes[3].real = 0.5f;

    uint32_t counts[4];
    uint32_t seed = 42;
    const uint32_t num_shots = 1000;

    qstats_result_t res = qstats_measure_finite_shots(state, num_shots, counts, &seed);
    EXPECT_EQ(res, QSTATS_OK);

    // Total counts should equal num_shots
    uint32_t total = counts[0] + counts[1] + counts[2] + counts[3];
    EXPECT_EQ(total, num_shots);

    qstats_pure_state_destroy(state);
}

//=============================================================================
// Von Neumann Entropy Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, VonNeumann_PureState) {
    // Pure state should have zero entropy
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    // |0><0| is pure
    float entropy = qstats_von_neumann_entropy(dm);
    EXPECT_NEAR(entropy, 0.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, VonNeumann_MaximallyMixed) {
    // Maximally mixed state has maximum entropy = log(d)
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float entropy = qstats_von_neumann_entropy(dm);
    float expected = std::log2(4.0f);  // 2 bits
    EXPECT_NEAR(entropy, expected, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, VonNeumann_MixedState) {
    // Create a mixed state: (|0><0| + |1><1|)/2
    qstats_density_matrix_t* dm = qstats_density_matrix_create(2);
    ASSERT_NE(dm, nullptr);

    dm->elements[0].real = 0.5f;  // |0><0|
    dm->elements[3].real = 0.5f;  // |1><1|

    float entropy = qstats_von_neumann_entropy(dm);
    EXPECT_NEAR(entropy, 1.0f, LOOSE_TOLERANCE);  // 1 bit

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, Entropy_All) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    qstats_entropy_result_t result;
    qstats_result_t res = qstats_entropy_all(dm, &result);
    EXPECT_EQ(res, QSTATS_OK);

    EXPECT_NEAR(result.von_neumann, 2.0f, LOOSE_TOLERANCE);
    EXPECT_NEAR(result.purity, 0.25f, TOLERANCE);  // 1/4 for d=4
    EXPECT_NEAR(result.max_entropy, 2.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Purity and Linear Entropy Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, Purity_PureState) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    float purity = qstats_purity(dm);
    EXPECT_NEAR(purity, 1.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, Purity_MaximallyMixed) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float purity = qstats_purity(dm);
    EXPECT_NEAR(purity, 0.25f, TOLERANCE);  // 1/d = 1/4

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, LinearEntropy_PureState) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    float S_L = qstats_linear_entropy(dm);
    EXPECT_NEAR(S_L, 0.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, LinearEntropy_MaximallyMixed) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float S_L = qstats_linear_entropy(dm);
    // S_L = (d/(d-1))(1 - 1/d) = (d/(d-1))((d-1)/d) = 1
    EXPECT_NEAR(S_L, 1.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Renyi Entropy Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, RenyiEntropy_Order2) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float H2 = qstats_renyi_entropy(dm, 2.0f);
    // H_2 = -log(Tr(ρ²)) = -log(1/4) = 2 bits
    EXPECT_NEAR(H2, 2.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, MinEntropy) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float H_min = qstats_min_entropy(dm);
    // H_min = -log(λ_max) = -log(0.25) = 2 bits
    EXPECT_NEAR(H_min, 2.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Fidelity and Distance Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, Fidelity_Identical) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float F = qstats_fidelity(dm, dm);
    EXPECT_NEAR(F, 1.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, Fidelity_Orthogonal) {
    qstats_density_matrix_t* dm1 = qstats_density_matrix_create(2);
    qstats_density_matrix_t* dm2 = qstats_density_matrix_create(2);
    ASSERT_NE(dm1, nullptr);
    ASSERT_NE(dm2, nullptr);

    // |0><0| and |1><1| are orthogonal
    dm2->elements[0].real = 0.0f;
    dm2->elements[3].real = 1.0f;

    float F = qstats_fidelity(dm1, dm2);
    EXPECT_NEAR(F, 0.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm1);
    qstats_density_matrix_destroy(dm2);
}

TEST_F(QuantumStatisticsTest, Fidelity_PureStates) {
    qstats_pure_state_t* psi = qstats_pure_state_create(2);
    qstats_pure_state_t* phi = qstats_pure_state_create(2);
    ASSERT_NE(psi, nullptr);
    ASSERT_NE(phi, nullptr);

    // |psi> = |0>, |phi> = |0> → F = 1
    float F1 = qstats_fidelity_pure(psi, phi);
    EXPECT_NEAR(F1, 1.0f, TOLERANCE);

    // |phi> = |1> → F = 0
    phi->amplitudes[0].real = 0.0f;
    phi->amplitudes[1].real = 1.0f;
    float F2 = qstats_fidelity_pure(psi, phi);
    EXPECT_NEAR(F2, 0.0f, TOLERANCE);

    qstats_pure_state_destroy(psi);
    qstats_pure_state_destroy(phi);
}

TEST_F(QuantumStatisticsTest, TraceDistance_Identical) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float D = qstats_trace_distance(dm, dm);
    EXPECT_NEAR(D, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, TraceDistance_Orthogonal) {
    qstats_density_matrix_t* dm1 = qstats_density_matrix_create(2);
    qstats_density_matrix_t* dm2 = qstats_density_matrix_create(2);
    ASSERT_NE(dm1, nullptr);
    ASSERT_NE(dm2, nullptr);

    // |0><0| and |1><1|
    dm2->elements[0].real = 0.0f;
    dm2->elements[3].real = 1.0f;

    float D = qstats_trace_distance(dm1, dm2);
    EXPECT_NEAR(D, 1.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm1);
    qstats_density_matrix_destroy(dm2);
}

TEST_F(QuantumStatisticsTest, BuresDistance) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    // Distance to self = 0
    float d = qstats_bures_distance(dm, dm);
    EXPECT_NEAR(d, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, CorrelationAll) {
    qstats_density_matrix_t* dm1 = qstats_density_matrix_maximally_mixed(4);
    qstats_density_matrix_t* dm2 = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm1, nullptr);
    ASSERT_NE(dm2, nullptr);

    qstats_correlation_result_t result;
    qstats_result_t res = qstats_correlation_all(dm1, dm2, &result);
    EXPECT_EQ(res, QSTATS_OK);

    EXPECT_NEAR(result.fidelity, 1.0f, LOOSE_TOLERANCE);
    EXPECT_NEAR(result.trace_distance, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm1);
    qstats_density_matrix_destroy(dm2);
}

//=============================================================================
// Quantum Relative Entropy Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, QuantumRelativeEntropy_SameState) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float S_rel = qstats_quantum_relative_entropy(dm, dm);
    EXPECT_NEAR(S_rel, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, QuantumRelativeEntropy_NonNegative) {
    qstats_density_matrix_t* dm1 = qstats_density_matrix_maximally_mixed(4);
    qstats_density_matrix_t* dm2 = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm1, nullptr);
    ASSERT_NE(dm2, nullptr);

    // Modify dm2 slightly
    dm2->elements[0].real = 0.4f;
    dm2->elements[5].real = 0.2f;
    dm2->elements[10].real = 0.2f;
    dm2->elements[15].real = 0.2f;

    float S_rel = qstats_quantum_relative_entropy(dm1, dm2);
    EXPECT_GE(S_rel, -TOLERANCE);  // Should be non-negative

    qstats_density_matrix_destroy(dm1);
    qstats_density_matrix_destroy(dm2);
}

//=============================================================================
// Entanglement Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, EntanglementEntropy_Product) {
    // Product state |00><00| should have zero entanglement
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);  // 2x2 system
    ASSERT_NE(dm, nullptr);

    float E = qstats_entanglement_entropy(dm, 2, 2);
    EXPECT_NEAR(E, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, EntanglementEntropy_Bell) {
    // Bell state |Φ+> = (|00> + |11>)/√2 should have E = 1 bit
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    // |Φ+><Φ+| = (|00><00| + |00><11| + |11><00| + |11><11|)/2
    dm->elements[0].real = 0.5f;   // |00><00|
    dm->elements[3].real = 0.5f;   // |00><11|
    dm->elements[12].real = 0.5f;  // |11><00|
    dm->elements[15].real = 0.5f;  // |11><11|

    float E = qstats_entanglement_entropy(dm, 2, 2);
    EXPECT_NEAR(E, 1.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, Negativity_Product) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    float N = qstats_negativity(dm, 2, 2);
    EXPECT_NEAR(N, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Partial Trace Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, PartialTrace_B) {
    // Create |00><00|
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    qstats_density_matrix_t* rho_a = qstats_partial_trace_b(dm, 2, 2);
    ASSERT_NE(rho_a, nullptr);

    // Should be |0><0| on subsystem A
    EXPECT_NEAR(rho_a->elements[0].real, 1.0f, TOLERANCE);
    EXPECT_NEAR(qstats_trace(rho_a), 1.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
    qstats_density_matrix_destroy(rho_a);
}

TEST_F(QuantumStatisticsTest, PartialTrace_A) {
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    qstats_density_matrix_t* rho_b = qstats_partial_trace_a(dm, 2, 2);
    ASSERT_NE(rho_b, nullptr);

    // Should be |0><0| on subsystem B
    EXPECT_NEAR(rho_b->elements[0].real, 1.0f, TOLERANCE);
    EXPECT_NEAR(qstats_trace(rho_b), 1.0f, TOLERANCE);

    qstats_density_matrix_destroy(dm);
    qstats_density_matrix_destroy(rho_b);
}

//=============================================================================
// Quantum Walk Integration Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, QuantumWalk_Entropy) {
    const uint32_t n = 8;
    std::vector<float> real(n), imag(n);

    // Localized state
    real[0] = 1.0f;
    float H_loc = qstats_quantum_walk_entropy(real.data(), imag.data(), n);
    EXPECT_NEAR(H_loc, 0.0f, TOLERANCE);

    // Uniform superposition
    float amp = 1.0f / std::sqrt((float)n);
    for (uint32_t i = 0; i < n; i++) {
        real[i] = amp;
    }
    float H_uniform = qstats_quantum_walk_entropy(real.data(), imag.data(), n);
    EXPECT_NEAR(H_uniform, std::log2((float)n), LOOSE_TOLERANCE);
}

TEST_F(QuantumStatisticsTest, QuantumWalk_Localization) {
    const uint32_t n = 8;
    std::vector<float> real(n, 0.0f), imag(n, 0.0f);

    // Localized at one site
    real[0] = 1.0f;
    float loc = qstats_quantum_walk_localization(real.data(), imag.data(), n);
    EXPECT_NEAR(loc, 1.0f, TOLERANCE);  // Effective sites = 1

    // Uniform over all sites
    float amp = 1.0f / std::sqrt((float)n);
    for (uint32_t i = 0; i < n; i++) {
        real[i] = amp;
    }
    loc = qstats_quantum_walk_localization(real.data(), imag.data(), n);
    EXPECT_NEAR(loc, (float)n, TOLERANCE);  // Effective sites = n
}

TEST_F(QuantumStatisticsTest, QuantumWalk_FromAmplitudes) {
    const uint32_t n = 4;
    std::vector<float> real = {0.5f, 0.5f, 0.5f, 0.5f};
    std::vector<float> imag = {0.0f, 0.0f, 0.0f, 0.0f};

    qstats_pure_state_t* state = qstats_from_quantum_walk(
        real.data(), imag.data(), n
    );
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->dim, n);
    EXPECT_TRUE(state->normalized);

    qstats_pure_state_destroy(state);
}

//=============================================================================
// Quantum Annealing Integration Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, Boltzmann_Distribution) {
    float energies[] = {0.0f, 1.0f, 2.0f, 3.0f};
    float probs[4];
    float T = 1.0f;

    qstats_result_t res = qstats_boltzmann_distribution(energies, 4, T, probs);
    EXPECT_EQ(res, QSTATS_OK);

    // Check normalization
    float sum = probs[0] + probs[1] + probs[2] + probs[3];
    EXPECT_NEAR(sum, 1.0f, TOLERANCE);

    // Lower energy should have higher probability
    EXPECT_GT(probs[0], probs[1]);
    EXPECT_GT(probs[1], probs[2]);
    EXPECT_GT(probs[2], probs[3]);
}

TEST_F(QuantumStatisticsTest, Boltzmann_HighTemperature) {
    float energies[] = {0.0f, 1.0f, 2.0f, 3.0f};
    float probs[4];
    float T = 1000.0f;  // Very high temperature

    qstats_boltzmann_distribution(energies, 4, T, probs);

    // At high T, all probabilities should be nearly equal
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(probs[i], 0.25f, 0.01f);
    }
}

TEST_F(QuantumStatisticsTest, FreeEnergy) {
    float energies[] = {0.0f, 1.0f};
    float T = 1.0f;

    float F = qstats_free_energy(energies, 2, T);

    // F = -T log(Z) = -T log(exp(0) + exp(-1)) = -T log(1 + e^(-1))
    float expected = -T * std::log(1.0f + std::exp(-1.0f));
    EXPECT_NEAR(F, expected, TOLERANCE);
}

TEST_F(QuantumStatisticsTest, PartitionFunction) {
    float energies[] = {0.0f, 1.0f};
    float T = 1.0f;

    float Z = qstats_partition_function(energies, 2, T);

    // Z = exp(0) + exp(-1) = 1 + e^(-1)
    float expected = 1.0f + std::exp(-1.0f);
    EXPECT_NEAR(Z, expected, TOLERANCE);
}

TEST_F(QuantumStatisticsTest, ThermodynamicEntropy) {
    float energies[] = {0.0f, 0.0f};  // Degenerate ground state
    float T = 1.0f;

    float S = qstats_thermodynamic_entropy(energies, 2, T);

    // S = log(2) for two-fold degeneracy
    EXPECT_NEAR(S, std::log(2.0f), LOOSE_TOLERANCE);
}

//=============================================================================
// Complex Number Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, Complex_Operations) {
    qstats_complex_t a = {3.0f, 4.0f};
    qstats_complex_t b = {1.0f, 2.0f};

    // Addition
    qstats_complex_t sum = qstats_complex_add(a, b);
    EXPECT_NEAR(sum.real, 4.0f, TOLERANCE);
    EXPECT_NEAR(sum.imag, 6.0f, TOLERANCE);

    // Subtraction
    qstats_complex_t diff = qstats_complex_sub(a, b);
    EXPECT_NEAR(diff.real, 2.0f, TOLERANCE);
    EXPECT_NEAR(diff.imag, 2.0f, TOLERANCE);

    // Multiplication
    qstats_complex_t prod = qstats_complex_mul(a, b);
    // (3+4i)(1+2i) = 3 + 6i + 4i + 8i² = 3 + 10i - 8 = -5 + 10i
    EXPECT_NEAR(prod.real, -5.0f, TOLERANCE);
    EXPECT_NEAR(prod.imag, 10.0f, TOLERANCE);

    // Conjugate
    qstats_complex_t conj = qstats_complex_conj(a);
    EXPECT_NEAR(conj.real, 3.0f, TOLERANCE);
    EXPECT_NEAR(conj.imag, -4.0f, TOLERANCE);

    // Absolute value
    float abs = qstats_complex_abs(a);
    EXPECT_NEAR(abs, 5.0f, TOLERANCE);

    // Absolute squared
    float abs_sq = qstats_complex_abs_squared(a);
    EXPECT_NEAR(abs_sq, 25.0f, TOLERANCE);
}

//=============================================================================
// Inner Product Test
//=============================================================================

TEST_F(QuantumStatisticsTest, InnerProduct) {
    qstats_pure_state_t* psi = qstats_pure_state_create(2);
    qstats_pure_state_t* phi = qstats_pure_state_create(2);
    ASSERT_NE(psi, nullptr);
    ASSERT_NE(phi, nullptr);

    // <0|0> = 1
    qstats_complex_t ip1 = qstats_inner_product(psi, phi);
    EXPECT_NEAR(ip1.real, 1.0f, TOLERANCE);
    EXPECT_NEAR(ip1.imag, 0.0f, TOLERANCE);

    // <0|1> = 0
    phi->amplitudes[0].real = 0.0f;
    phi->amplitudes[1].real = 1.0f;
    qstats_complex_t ip2 = qstats_inner_product(psi, phi);
    EXPECT_NEAR(ip2.real, 0.0f, TOLERANCE);
    EXPECT_NEAR(ip2.imag, 0.0f, TOLERANCE);

    qstats_pure_state_destroy(psi);
    qstats_pure_state_destroy(phi);
}

//=============================================================================
// Quantum Mutual Information Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, QuantumMutualInformation_Product) {
    // Product state: I(A:B) = 0
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    float I = qstats_quantum_mutual_information(dm, 2, 2);
    EXPECT_NEAR(I, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, QuantumMutualInformation_MaximallyEntangled) {
    // Bell state: I(A:B) = 2 (maximal for 2 qubits)
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    // |Φ+><Φ+|
    dm->elements[0].real = 0.5f;
    dm->elements[3].real = 0.5f;
    dm->elements[12].real = 0.5f;
    dm->elements[15].real = 0.5f;

    float I = qstats_quantum_mutual_information(dm, 2, 2);
    EXPECT_NEAR(I, 2.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Quantum Conditional Entropy Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, QuantumConditionalEntropy_Product) {
    // Product state |00>: S(A|B) = 0
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    float S_AgivenB = qstats_quantum_conditional_entropy(dm, 2, 2);
    EXPECT_NEAR(S_AgivenB, 0.0f, LOOSE_TOLERANCE);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, QuantumConditionalEntropy_CanBeNegative) {
    // For entangled states, S(A|B) can be negative (quantum feature!)
    qstats_density_matrix_t* dm = qstats_density_matrix_create(4);
    ASSERT_NE(dm, nullptr);

    // Bell state
    dm->elements[0].real = 0.5f;
    dm->elements[3].real = 0.5f;
    dm->elements[12].real = 0.5f;
    dm->elements[15].real = 0.5f;

    float S_AgivenB = qstats_quantum_conditional_entropy(dm, 2, 2);
    // For Bell state: S(AB) = 0, S(B) = 1, so S(A|B) = 0 - 1 = -1
    EXPECT_LT(S_AgivenB, 0.0f);

    qstats_density_matrix_destroy(dm);
}

//=============================================================================
// Monte Carlo Tests
//=============================================================================

TEST_F(QuantumStatisticsTest, VonNeumann_MC) {
    qstats_density_matrix_t* dm = qstats_density_matrix_maximally_mixed(4);
    ASSERT_NE(dm, nullptr);

    float variance;
    float H_mc = qstats_von_neumann_entropy_mc(dm, 10000, &variance);

    EXPECT_NEAR(H_mc, 2.0f, 0.1f);  // Allow more tolerance for MC
    EXPECT_GE(variance, 0.0f);

    qstats_density_matrix_destroy(dm);
}

TEST_F(QuantumStatisticsTest, PartitionFunction_MC) {
    float energies[] = {0.0f, 1.0f, 2.0f, 3.0f};
    float T = 1.0f;

    float variance;
    float Z_mc = qstats_partition_function_mc(energies, 4, T, 10000, &variance);
    float Z_exact = qstats_partition_function(energies, 4, T);

    EXPECT_NEAR(Z_mc, Z_exact, 0.5f);  // Allow tolerance for MC noise

    qstats_density_matrix_destroy(nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
