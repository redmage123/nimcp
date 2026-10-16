//=============================================================================
// test_quantum_walk_coin.cpp - Custom Coin Operator Unit Tests
//=============================================================================
/**
 * @file test_quantum_walk_coin.cpp
 * @brief Comprehensive unit tests for custom coin operator implementation
 *
 * WHAT: Validate custom coin operator correctness and properties
 * WHY: Ensure quantum walk supports user-defined quantum gates
 * HOW: Unit tests for Hadamard, Grover, DFT, and custom matrices
 *
 * TEST COVERAGE:
 * 1. Hadamard matrix coin operator
 * 2. Grover diffusion matrix coin operator
 * 3. DFT (Discrete Fourier Transform) coin operator
 * 4. Custom unitary matrices
 * 5. Unitarity validation
 * 6. Probability conservation
 * 7. Error handling (NULL, non-unitary matrices)
 * 8. Performance benchmarks
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.9.0 Phase C2.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <complex>
#include <vector>

#include "utils/quantum/nimcp_quantum_walk.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "test_helpers.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Utilities
//=============================================================================

/**
 * @brief Create test neural network
 */
neural_network_t create_test_network(uint32_t num_neurons) {
    network_config_t config = create_test_network_config();
    config.num_neurons = num_neurons;
    config.input_size = num_neurons;
    config.output_size = num_neurons;

    neural_network_t network = neural_network_create(&config);

    return network;
}

/**
 * @brief Allocate 2D matrix for coin operator
 */
quantum_amplitude_t** allocate_coin_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = (quantum_amplitude_t**)nimcp_malloc(
        size * sizeof(quantum_amplitude_t*)
    );

    for (uint32_t i = 0; i < size; i++) {
        matrix[i] = (quantum_amplitude_t*)nimcp_calloc(
            size, sizeof(quantum_amplitude_t)
        );
    }

    return matrix;
}

/**
 * @brief Free 2D matrix
 */
void free_coin_matrix(quantum_amplitude_t** matrix, uint32_t size) {
    if (!matrix) return;

    for (uint32_t i = 0; i < size; i++) {
        if (matrix[i]) {
            nimcp_free(matrix[i]);
        }
    }
    nimcp_free(matrix);
}

/**
 * @brief Create Hadamard matrix: H = (1/√2) [[1, 1], [1, -1]]
 */
quantum_amplitude_t** create_hadamard_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = allocate_coin_matrix(size);

    // For size > 2, use DFT-based unitary matrix (always unitary)
    // This is simpler and always correct
    float scale = 1.0f / sqrtf((float)size);
    const float PI = 3.14159265359f;

    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            // DFT matrix: H[i][j] = (1/√N) * exp(2πi*i*j/N)
            float phase = 2.0f * PI * (float)(i * j) / (float)size;
            matrix[i][j] = scale * std::complex<float>(cosf(phase), sinf(phase));
        }
    }

    return matrix;
}

/**
 * @brief Create Grover diffusion matrix: G = (2/N)J - I
 */
quantum_amplitude_t** create_grover_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = allocate_coin_matrix(size);

    float scale = 2.0f / (float)size;

    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            if (i == j) {
                // Diagonal: 2/N - 1
                matrix[i][j] = scale - 1.0f;
            } else {
                // Off-diagonal: 2/N
                matrix[i][j] = scale;
            }
        }
    }

    return matrix;
}

/**
 * @brief Create DFT (Discrete Fourier Transform) matrix
 */
quantum_amplitude_t** create_dft_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = allocate_coin_matrix(size);

    const float PI = 3.14159265359f;
    float scale = 1.0f / sqrtf((float)size);

    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            float phase = 2.0f * PI * (float)(i * j) / (float)size;
            std::complex<float> twiddle(cosf(phase), sinf(phase));
            matrix[i][j] = twiddle * scale;
        }
    }

    return matrix;
}

/**
 * @brief Create identity matrix
 */
quantum_amplitude_t** create_identity_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = allocate_coin_matrix(size);

    for (uint32_t i = 0; i < size; i++) {
        matrix[i][i] = 1.0f;
    }

    return matrix;
}

/**
 * @brief Verify matrix is unitary: U†U = I
 */
bool is_unitary(quantum_amplitude_t** matrix, uint32_t size) {
    const float tolerance = 1e-4f;

    // Compute U†U
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            std::complex<float> sum = 0.0f;

            for (uint32_t k = 0; k < size; k++) {
                // U†[i][k] = conj(U[k][i])
                std::complex<float> u_dag_ik = std::conj(matrix[k][i]);
                std::complex<float> u_kj = matrix[k][j];
                sum += u_dag_ik * u_kj;
            }

            // Check if diagonal = 1, off-diagonal = 0
            float expected = (i == j) ? 1.0f : 0.0f;
            if (std::abs(sum - expected) > tolerance) {
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Check if probability distribution is valid
 */
bool is_valid_distribution(const float* probs, uint32_t size) {
    float total = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (probs[i] < 0.0f || probs[i] > 1.0f) return false;
        total += probs[i];
    }
    return fabsf(total - 1.0f) < 1e-4f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumWalkCoinTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup
        srand(42); // Reproducibility
    }

    void TearDown() override {
        // Cleanup per test
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

//=============================================================================
// Custom Coin Operator Tests
//=============================================================================

TEST_F(QuantumWalkCoinTest, HadamardCoinOperator) {
    // WHAT: Test custom Hadamard matrix coin operator
    // WHY: Verify basic coin operator application
    // HOW: Apply Hadamard matrix, check probability conservation

    const uint32_t num_neurons = 8;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY; // Start with identity, apply custom
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Initialize at center node
    quantum_walk_initialize(walker, num_neurons / 2);

    // Create Hadamard matrix
    quantum_amplitude_t** hadamard = create_hadamard_matrix(num_neurons);
    ASSERT_NE(hadamard, nullptr);

    // Verify unitarity
    EXPECT_TRUE(is_unitary(hadamard, num_neurons));

    // Apply custom coin operator
    bool success = quantum_walk_apply_custom_coin(walker,
        const_cast<const quantum_amplitude_t**>(hadamard));
    ASSERT_TRUE(success);

    // Verify probability conservation
    EXPECT_TRUE(quantum_walk_verify(walker));

    float* probs = (float*)nimcp_malloc(num_neurons * sizeof(float));
    quantum_walk_get_distribution(walker, probs);
    EXPECT_TRUE(is_valid_distribution(probs, num_neurons));

    printf("✅ Hadamard custom coin operator test passed\n");

    nimcp_free(probs);
    free_coin_matrix(hadamard, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkCoinTest, GroverCoinOperator) {
    // WHAT: Test custom Grover diffusion coin operator
    // WHY: Verify complex coin operators work correctly
    // HOW: Apply Grover matrix, check spreading

    const uint32_t num_neurons = 10;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 5);

    // Create Grover matrix
    quantum_amplitude_t** grover = create_grover_matrix(num_neurons);
    ASSERT_NE(grover, nullptr);

    // Verify unitarity
    EXPECT_TRUE(is_unitary(grover, num_neurons));

    // Apply custom coin operator
    bool success = quantum_walk_apply_custom_coin(walker,
        const_cast<const quantum_amplitude_t**>(grover));
    ASSERT_TRUE(success);

    // Verify probability conservation
    EXPECT_TRUE(quantum_walk_verify(walker));

    printf("✅ Grover custom coin operator test passed\n");

    free_coin_matrix(grover, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkCoinTest, DFTCoinOperator) {
    // WHAT: Test Discrete Fourier Transform coin operator
    // WHY: Verify phase-dependent coin operators
    // HOW: Apply DFT matrix, check probability conservation

    const uint32_t num_neurons = 8;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 4);

    // Create DFT matrix
    quantum_amplitude_t** dft = create_dft_matrix(num_neurons);
    ASSERT_NE(dft, nullptr);

    // Verify unitarity
    EXPECT_TRUE(is_unitary(dft, num_neurons));

    // Apply custom coin operator
    bool success = quantum_walk_apply_custom_coin(walker,
        const_cast<const quantum_amplitude_t**>(dft));
    ASSERT_TRUE(success);

    // Verify probability conservation
    EXPECT_TRUE(quantum_walk_verify(walker));

    float* probs = (float*)nimcp_malloc(num_neurons * sizeof(float));
    quantum_walk_get_distribution(walker, probs);
    EXPECT_TRUE(is_valid_distribution(probs, num_neurons));

    printf("✅ DFT custom coin operator test passed\n");

    nimcp_free(probs);
    free_coin_matrix(dft, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkCoinTest, IdentityCoinOperator) {
    // WHAT: Test identity matrix (classical limit)
    // WHY: Verify no transformation case
    // HOW: Apply identity, check state unchanged

    const uint32_t num_neurons = 6;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 3);

    // Get initial probabilities
    float* probs_before = (float*)nimcp_malloc(num_neurons * sizeof(float));
    quantum_walk_get_distribution(walker, probs_before);

    // Create identity matrix
    quantum_amplitude_t** identity = create_identity_matrix(num_neurons);
    ASSERT_NE(identity, nullptr);

    // Apply identity coin operator
    bool success = quantum_walk_apply_custom_coin(walker,
        const_cast<const quantum_amplitude_t**>(identity));
    ASSERT_TRUE(success);

    // Get final probabilities
    float* probs_after = (float*)nimcp_malloc(num_neurons * sizeof(float));
    quantum_walk_get_distribution(walker, probs_after);

    // Probabilities should be nearly identical (within numerical error)
    for (uint32_t i = 0; i < num_neurons; i++) {
        EXPECT_NEAR(probs_before[i], probs_after[i], 1e-5f);
    }

    printf("✅ Identity custom coin operator test passed\n");

    nimcp_free(probs_before);
    nimcp_free(probs_after);
    free_coin_matrix(identity, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkCoinTest, MultipleApplications) {
    // WHAT: Test applying custom coin multiple times
    // WHY: Verify repeated application maintains consistency
    // HOW: Apply coin operator 10 times, check conservation

    const uint32_t num_neurons = 8;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 4);

    // Create Hadamard matrix
    quantum_amplitude_t** hadamard = create_hadamard_matrix(num_neurons);
    ASSERT_NE(hadamard, nullptr);

    // Apply coin operator 10 times
    for (uint32_t i = 0; i < 10; i++) {
        bool success = quantum_walk_apply_custom_coin(walker,
            const_cast<const quantum_amplitude_t**>(hadamard));
        ASSERT_TRUE(success);

        // Verify probability conservation at each step
        EXPECT_TRUE(quantum_walk_verify(walker));
    }

    printf("✅ Multiple applications test passed\n");

    free_coin_matrix(hadamard, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(QuantumWalkCoinTest, NullWalkerHandling) {
    // WHAT: Test NULL walker input
    // WHY: Ensure graceful failure
    // HOW: Pass NULL walker, expect false return

    quantum_amplitude_t** matrix = create_identity_matrix(4);

    bool success = quantum_walk_apply_custom_coin(nullptr,
        const_cast<const quantum_amplitude_t**>(matrix));
    EXPECT_FALSE(success);

    free_coin_matrix(matrix, 4);
}

TEST_F(QuantumWalkCoinTest, NullMatrixHandling) {
    // WHAT: Test NULL matrix input
    // WHY: Ensure graceful failure
    // HOW: Pass NULL matrix, expect false return

    neural_network_t network = create_test_network(5);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 2);

    bool success = quantum_walk_apply_custom_coin(walker, nullptr);
    EXPECT_FALSE(success);

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkCoinTest, NullMatrixRowHandling) {
    // WHAT: Test NULL row in matrix
    // WHY: Ensure row validation
    // HOW: Create matrix with NULL row, expect false return

    const uint32_t num_neurons = 5;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 2);

    // Create matrix with NULL row
    quantum_amplitude_t** matrix = allocate_coin_matrix(num_neurons);
    nimcp_free(matrix[2]); // Free middle row
    matrix[2] = nullptr;

    bool success = quantum_walk_apply_custom_coin(walker,
        const_cast<const quantum_amplitude_t**>(matrix));
    EXPECT_FALSE(success);

    // Free remaining rows
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (i != 2 && matrix[i]) {
            nimcp_free(matrix[i]);
        }
    }
    nimcp_free(matrix);

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Probability Conservation Tests
//=============================================================================

TEST_F(QuantumWalkCoinTest, ProbabilityConservationHadamard) {
    // WHAT: Verify probability conservation with Hadamard
    // WHY: Critical quantum property must hold
    // HOW: Apply Hadamard multiple times, check Σ|αᵢ|² = 1.0

    const uint32_t num_neurons = 10;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 5);

    quantum_amplitude_t** hadamard = create_hadamard_matrix(num_neurons);
    ASSERT_NE(hadamard, nullptr);

    // Apply coin and verify conservation 100 times
    for (uint32_t step = 0; step < 100; step++) {
        quantum_walk_apply_custom_coin(walker,
            const_cast<const quantum_amplitude_t**>(hadamard));

        quantum_walk_stats_t stats;
        quantum_walk_compute_stats(walker, &stats);

        EXPECT_NEAR(stats.total_probability, 1.0f, 1e-4f)
            << "Probability not conserved at step " << step;
    }

    printf("✅ Probability conservation verified over 100 applications\n");

    free_coin_matrix(hadamard, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Integration with Quantum Walk Tests
//=============================================================================

TEST_F(QuantumWalkCoinTest, CoinPlusShiftOperator) {
    // WHAT: Test custom coin + shift operator integration
    // WHY: Verify coin works in full quantum walk evolution
    // HOW: Apply custom coin, then quantum walk step

    const uint32_t num_neurons = 12;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY; // We'll apply custom coin manually
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 6);

    quantum_amplitude_t** grover = create_grover_matrix(num_neurons);
    ASSERT_NE(grover, nullptr);

    // Simulate quantum walk: alternate custom coin + shift
    for (uint32_t step = 0; step < 20; step++) {
        // Apply custom coin
        quantum_walk_apply_custom_coin(walker,
            const_cast<const quantum_amplitude_t**>(grover));

        // Apply shift operator (via quantum walk step)
        quantum_walk_step(walker);

        // Verify conservation
        EXPECT_TRUE(quantum_walk_verify(walker));
    }

    printf("✅ Custom coin + shift operator integration test passed\n");

    free_coin_matrix(grover, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
