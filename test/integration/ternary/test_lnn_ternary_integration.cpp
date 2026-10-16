/**
 * @file test_lnn_ternary_integration.cpp
 * @brief Integration tests for LNN with ternary representation
 *
 * Tests:
 * - LNN with ternary recurrent weights
 * - ODE solver with ternary matrices
 * - Wiring patterns with ternary connectivity
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// LNN headers have their own extern "C" guards
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_wiring.h"
#include "lnn/nimcp_lnn_layer.h"

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

/**
 * @class LNNTernaryIntegrationTest
 * @brief Test fixture for LNN with ternary integration
 */
class LNNTernaryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize LNN library
        int result = lnn_init(2);  // 2 worker threads
        ASSERT_EQ(0, result) << "Failed to initialize LNN library";
    }

    void TearDown() override {
        lnn_shutdown();
    }

    /**
     * @brief Create input tensor with given dimensions
     */
    nimcp_tensor_t* createInputTensor(uint32_t size, float value = 0.5f) {
        uint32_t dims[1] = {size};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!tensor) return nullptr;

        float* data = (float*)nimcp_tensor_data(tensor);
        for (uint32_t i = 0; i < size; i++) {
            data[i] = value + 0.1f * (i % 5);
        }
        return tensor;
    }

    /**
     * @brief Create output tensor with given dimensions
     */
    nimcp_tensor_t* createOutputTensor(uint32_t size) {
        uint32_t dims[1] = {size};
        return nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    }

    /**
     * @brief Create ternary weight matrix from LNN wiring
     */
    trit_matrix_t* createTernaryWiringMatrix(lnn_wiring_t* wiring) {
        if (!wiring) return nullptr;

        trit_matrix_t* mat = trit_matrix_create(
            wiring->n_neurons, wiring->n_neurons, TERNARY_PACK_NONE);
        if (!mat) return nullptr;

        // Convert wiring to ternary connectivity matrix
        for (uint32_t from = 0; from < wiring->n_neurons; from++) {
            for (uint32_t to = 0; to < wiring->n_neurons; to++) {
                if (lnn_wiring_has_edge(wiring, from, to)) {
                    trit_matrix_set(mat, from, to, TRIT_POSITIVE);
                } else {
                    trit_matrix_set(mat, from, to, TRIT_UNKNOWN);
                }
            }
        }
        return mat;
    }
};

//=============================================================================
// Test: LNN with Ternary Recurrent Weights
//=============================================================================

/**
 * Test basic NCP network creation and forward pass
 */
TEST_F(LNNTernaryIntegrationTest, BasicNCPNetworkForwardPass) {
    const uint32_t n_inputs = 4;
    const uint32_t n_inter = 8;
    const uint32_t n_command = 4;
    const uint32_t n_outputs = 2;

    // Create NCP network
    lnn_network_t* network = lnn_network_create_ncp(
        n_inputs, n_inter, n_command, n_outputs);
    ASSERT_NE(nullptr, network) << "Failed to create NCP network";

    // Initialize weights
    int result = lnn_network_init_weights(network, 42);
    EXPECT_EQ(0, result) << "Failed to initialize weights";

    // Create input and output tensors
    nimcp_tensor_t* input = createInputTensor(n_inputs);
    nimcp_tensor_t* output = createOutputTensor(n_outputs);
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    // Forward step
    result = lnn_forward_step(network, input, output, 0.01f);
    EXPECT_EQ(0, result) << "Forward step failed";

    // Verify output is valid (not NaN or inf)
    float* out_data = (float*)nimcp_tensor_data(output);
    for (uint32_t i = 0; i < n_outputs; i++) {
        EXPECT_FALSE(std::isnan(out_data[i])) << "Output contains NaN at index " << i;
        EXPECT_FALSE(std::isinf(out_data[i])) << "Output contains inf at index " << i;
    }

    // Cleanup
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);
    lnn_network_destroy(network);
}

/**
 * Test ternary representation of recurrent weight connectivity
 */
TEST_F(LNNTernaryIntegrationTest, TernaryRecurrentWeightConnectivity) {
    const uint32_t n_neurons = 16;
    const float sparsity = 0.7f;

    // Create random sparse wiring
    lnn_wiring_t* wiring = lnn_wiring_create_random(n_neurons, sparsity, 12345);
    ASSERT_NE(nullptr, wiring) << "Failed to create random wiring";

    // Convert to ternary matrix
    trit_matrix_t* ternary_wiring = createTernaryWiringMatrix(wiring);
    ASSERT_NE(nullptr, ternary_wiring);

    // Verify ternary matrix matches wiring connectivity
    for (uint32_t from = 0; from < n_neurons; from++) {
        for (uint32_t to = 0; to < n_neurons; to++) {
            bool has_edge = lnn_wiring_has_edge(wiring, from, to);
            trit_t trit_val = trit_matrix_get(ternary_wiring, from, to);

            if (has_edge) {
                EXPECT_EQ(TRIT_POSITIVE, trit_val)
                    << "Edge at (" << from << "," << to << ") should be POSITIVE";
            } else {
                EXPECT_EQ(TRIT_UNKNOWN, trit_val)
                    << "No edge at (" << from << "," << to << ") should be UNKNOWN";
            }
        }
    }

    // Verify sparsity is approximately correct
    float ternary_sparsity = trit_matrix_sparsity(ternary_wiring);
    EXPECT_NEAR(sparsity, ternary_sparsity, 0.1f)
        << "Ternary matrix sparsity should approximate wiring sparsity";

    // Cleanup
    trit_matrix_destroy(ternary_wiring);
    lnn_wiring_destroy(wiring);
}

/**
 * Test float-to-ternary conversion for LNN weights
 */
TEST_F(LNNTernaryIntegrationTest, FloatToTernaryWeightConversion) {
    const size_t n_weights = 64;
    const float threshold = 0.3f;
    const float scale = 1.0f;

    // Create float weights simulating LNN weight values
    std::vector<float> float_weights(n_weights);
    for (size_t i = 0; i < n_weights; i++) {
        // Simulate weights: some negative, some zero-ish, some positive
        float_weights[i] = (float)(i % 7) / 3.0f - 1.0f;  // Range [-1, 1]
    }

    // Convert to ternary using threshold
    trit_vector_t* ternary_weights = trit_vector_create(n_weights, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, ternary_weights);

    for (size_t i = 0; i < n_weights; i++) {
        trit_t trit_val = trit_from_float_threshold(float_weights[i], threshold);
        trit_vector_set(ternary_weights, i, trit_val);
    }

    // Verify conversion correctness
    for (size_t i = 0; i < n_weights; i++) {
        trit_t trit_val = trit_vector_get(ternary_weights, i);

        if (float_weights[i] > threshold) {
            EXPECT_EQ(TRIT_POSITIVE, trit_val)
                << "Weight " << float_weights[i] << " should be POSITIVE";
        } else if (float_weights[i] < -threshold) {
            EXPECT_EQ(TRIT_NEGATIVE, trit_val)
                << "Weight " << float_weights[i] << " should be NEGATIVE";
        } else {
            EXPECT_EQ(TRIT_UNKNOWN, trit_val)
                << "Weight " << float_weights[i] << " should be UNKNOWN";
        }
    }

    // Test dequantization
    for (size_t i = 0; i < n_weights; i++) {
        trit_t trit_val = trit_vector_get(ternary_weights, i);
        float dequant = trit_dequantize_weight(trit_val, scale, -scale);

        EXPECT_TRUE(dequant >= -scale && dequant <= scale)
            << "Dequantized weight should be in [-scale, scale]";
    }

    trit_vector_destroy(ternary_weights);
}

//=============================================================================
// Test: ODE Solver with Ternary Matrices
//=============================================================================

/**
 * Test ternary matrix-vector product for ODE state computation
 */
TEST_F(LNNTernaryIntegrationTest, TernaryMatrixVectorODEComputation) {
    const size_t n_neurons = 8;

    // Create ternary connectivity matrix
    trit_matrix_t* connectivity = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, connectivity);

    // Set up sparse connectivity pattern
    // Excitatory connections on diagonal + few off-diagonal
    for (size_t i = 0; i < n_neurons; i++) {
        trit_matrix_set(connectivity, i, i, TRIT_POSITIVE);  // Self-loop
        if (i > 0) {
            trit_matrix_set(connectivity, i, i - 1, TRIT_POSITIVE);  // Forward
        }
        if (i < n_neurons - 1) {
            trit_matrix_set(connectivity, i + 1, i, TRIT_NEGATIVE);  // Inhibitory feedback
        }
    }

    // Create state vector
    trit_vector_t* state = trit_vector_create(n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, state);

    // Initialize state: alternating pattern
    for (size_t i = 0; i < n_neurons; i++) {
        trit_vector_set(state, i, (i % 3 == 0) ? TRIT_POSITIVE :
                                  (i % 3 == 1) ? TRIT_NEGATIVE : TRIT_UNKNOWN);
    }

    // Compute matrix-vector product (simulates one ODE step)
    trit_vector_t* result = trit_matrix_vector_mul(connectivity, state);
    ASSERT_NE(nullptr, result) << "Matrix-vector multiplication failed";

    // Verify result dimensions
    EXPECT_EQ(n_neurons, result->length);

    // Verify result values are valid trits
    for (size_t i = 0; i < n_neurons; i++) {
        trit_t val = trit_vector_get(result, i);
        EXPECT_TRUE(val == TRIT_NEGATIVE || val == TRIT_UNKNOWN || val == TRIT_POSITIVE)
            << "Result at " << i << " is invalid trit: " << (int)val;
    }

    trit_vector_destroy(result);
    trit_vector_destroy(state);
    trit_matrix_destroy(connectivity);
}

/**
 * Test ternary ODE solver consistency with integer accumulation
 */
TEST_F(LNNTernaryIntegrationTest, TernaryODEIntegerAccumulation) {
    const size_t n_neurons = 6;

    // Create weight matrix
    trit_matrix_t* weights = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, weights);

    // Set specific pattern for predictable output
    // Row 0: [+1, +1, +1, 0, 0, 0]
    trit_matrix_set(weights, 0, 0, TRIT_POSITIVE);
    trit_matrix_set(weights, 0, 1, TRIT_POSITIVE);
    trit_matrix_set(weights, 0, 2, TRIT_POSITIVE);

    // Row 1: [-1, +1, 0, -1, 0, 0]
    trit_matrix_set(weights, 1, 0, TRIT_NEGATIVE);
    trit_matrix_set(weights, 1, 1, TRIT_POSITIVE);
    trit_matrix_set(weights, 1, 3, TRIT_NEGATIVE);

    // Create input state: [+1, +1, -1, +1, 0, +1]
    trit_vector_t* input = trit_vector_create(n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, input);
    trit_vector_set(input, 0, TRIT_POSITIVE);
    trit_vector_set(input, 1, TRIT_POSITIVE);
    trit_vector_set(input, 2, TRIT_NEGATIVE);
    trit_vector_set(input, 3, TRIT_POSITIVE);
    trit_vector_set(input, 4, TRIT_UNKNOWN);
    trit_vector_set(input, 5, TRIT_POSITIVE);

    // Compute matrix-vector product with integer accumulation
    int result_int[6] = {0};
    ternary_error_t err = trit_matrix_vector_mul_int(weights, input, result_int);
    EXPECT_EQ(TERNARY_OK, err);

    // Verify row 0: (+1)*1 + (+1)*1 + (+1)*(-1) + 0 + 0 + 0 = 1
    EXPECT_EQ(1, result_int[0]) << "Row 0 accumulation incorrect";

    // Verify row 1: (-1)*1 + (+1)*1 + 0 + (-1)*1 + 0 + 0 = -1
    EXPECT_EQ(-1, result_int[1]) << "Row 1 accumulation incorrect";

    trit_vector_destroy(input);
    trit_matrix_destroy(weights);
}

/**
 * Test sequence processing with ternary state representation
 */
TEST_F(LNNTernaryIntegrationTest, TernarySequenceStateEvolution) {
    const size_t n_neurons = 4;
    const size_t seq_len = 5;

    // Create connectivity matrix
    trit_matrix_t* connectivity = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, connectivity);

    // Simple chain topology: 0 -> 1 -> 2 -> 3
    trit_matrix_set(connectivity, 1, 0, TRIT_POSITIVE);
    trit_matrix_set(connectivity, 2, 1, TRIT_POSITIVE);
    trit_matrix_set(connectivity, 3, 2, TRIT_POSITIVE);

    // Track state history
    std::vector<trit_vector_t*> state_history;

    // Initial state: only neuron 0 active
    trit_vector_t* state = trit_vector_create(n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, state);
    trit_vector_set(state, 0, TRIT_POSITIVE);

    state_history.push_back(trit_vector_clone(state));

    // Evolve state through sequence
    for (size_t t = 0; t < seq_len - 1; t++) {
        trit_vector_t* next_state = trit_matrix_vector_mul(connectivity, state);
        ASSERT_NE(nullptr, next_state) << "Failed at timestep " << t;

        // Store history
        state_history.push_back(trit_vector_clone(next_state));

        // Update state
        trit_vector_destroy(state);
        state = next_state;
    }

    // Verify activity propagates through chain
    // t=0: [+1, 0, 0, 0]
    EXPECT_EQ(TRIT_POSITIVE, trit_vector_get(state_history[0], 0));
    EXPECT_EQ(TRIT_UNKNOWN, trit_vector_get(state_history[0], 1));

    // t=1: [0, +1, 0, 0]
    EXPECT_EQ(TRIT_POSITIVE, trit_vector_get(state_history[1], 1));

    // Cleanup
    for (auto* s : state_history) {
        trit_vector_destroy(s);
    }
    trit_vector_destroy(state);
    trit_matrix_destroy(connectivity);
}

//=============================================================================
// Test: Wiring Patterns with Ternary Connectivity
//=============================================================================

/**
 * Test full wiring pattern with ternary matrix representation
 */
TEST_F(LNNTernaryIntegrationTest, FullWiringTernaryMatrix) {
    const uint32_t n_neurons = 8;

    // Create full wiring
    lnn_wiring_t* wiring = lnn_wiring_create_full(n_neurons);
    ASSERT_NE(nullptr, wiring);

    // Convert to ternary matrix
    trit_matrix_t* ternary_mat = createTernaryWiringMatrix(wiring);
    ASSERT_NE(nullptr, ternary_mat);

    // Verify all entries are POSITIVE (full connectivity)
    size_t n_positive, n_unknown, n_negative;
    trit_matrix_count(ternary_mat, &n_positive, &n_unknown, &n_negative);

    EXPECT_EQ(n_neurons * n_neurons, n_positive)
        << "Full wiring should have all positive entries";
    EXPECT_EQ(0u, n_unknown);
    EXPECT_EQ(0u, n_negative);

    // Verify sparsity is 0 (fully connected)
    float sparsity = trit_matrix_sparsity(ternary_mat);
    EXPECT_NEAR(0.0f, sparsity, 0.001f);

    trit_matrix_destroy(ternary_mat);
    lnn_wiring_destroy(wiring);
}

/**
 * Test small-world wiring with ternary matrix representation
 */
TEST_F(LNNTernaryIntegrationTest, SmallWorldWiringTernaryMatrix) {
    const uint32_t n_neurons = 16;
    const uint32_t k_neighbors = 4;
    const float rewire_prob = 0.1f;

    // Create small-world wiring
    lnn_wiring_t* wiring = lnn_wiring_create_small_world(
        n_neurons, k_neighbors, rewire_prob, 42);
    ASSERT_NE(nullptr, wiring);

    // Convert to ternary matrix
    trit_matrix_t* ternary_mat = createTernaryWiringMatrix(wiring);
    ASSERT_NE(nullptr, ternary_mat);

    // Verify edge count matches expected for small-world
    size_t n_positive, n_unknown, n_negative;
    trit_matrix_count(ternary_mat, &n_positive, &n_unknown, &n_negative);

    // Small-world should have approximately n_neurons * k_neighbors edges
    uint32_t expected_edges = wiring->n_edges;
    EXPECT_EQ(expected_edges, n_positive)
        << "Ternary matrix should have " << expected_edges << " positive entries";

    // Verify sparsity is high (sparse connectivity)
    float sparsity = trit_matrix_sparsity(ternary_mat);
    EXPECT_GT(sparsity, 0.5f) << "Small-world network should be sparse";

    trit_matrix_destroy(ternary_mat);
    lnn_wiring_destroy(wiring);
}

/**
 * Test scale-free wiring with ternary matrix representation
 */
TEST_F(LNNTernaryIntegrationTest, ScaleFreeWiringTernaryMatrix) {
    const uint32_t n_neurons = 20;
    const uint32_t m_edges = 3;

    // Create scale-free wiring
    lnn_wiring_t* wiring = lnn_wiring_create_scale_free(n_neurons, m_edges, 12345);
    ASSERT_NE(nullptr, wiring);

    // Convert to ternary matrix
    trit_matrix_t* ternary_mat = createTernaryWiringMatrix(wiring);
    ASSERT_NE(nullptr, ternary_mat);

    // Verify sparsity
    float sparsity = trit_matrix_sparsity(ternary_mat);
    EXPECT_GT(sparsity, 0.0f) << "Scale-free network should be sparse";
    EXPECT_LT(sparsity, 1.0f) << "Scale-free network should have some edges";

    // Verify degree distribution shows hub structure
    // In Barabasi-Albert model, early nodes (hubs) receive many incoming edges
    // from later nodes due to preferential attachment. Check in-degree for hubs.
    uint32_t max_in_degree = 0;
    uint32_t total_in_degree = 0;
    for (uint32_t i = 0; i < n_neurons; i++) {
        uint32_t in_degree = lnn_wiring_in_degree(wiring, i);
        total_in_degree += in_degree;
        if (in_degree > max_in_degree) {
            max_in_degree = in_degree;
        }
    }

    float avg_in_degree = (float)total_in_degree / n_neurons;
    EXPECT_GT(max_in_degree, avg_in_degree * 1.5f)
        << "Scale-free network should have hub neurons with high in-degree";

    trit_matrix_destroy(ternary_mat);
    lnn_wiring_destroy(wiring);
}

/**
 * Test NCP wiring with ternary matrix representation
 */
TEST_F(LNNTernaryIntegrationTest, NCPWiringTernaryMatrix) {
    const uint32_t n_sensory = 4;
    const uint32_t n_inter = 6;
    const uint32_t n_command = 3;
    const uint32_t n_motor = 2;
    const uint32_t n_total = n_sensory + n_inter + n_command + n_motor;

    // Create NCP wiring
    lnn_wiring_t* wiring = lnn_wiring_create_ncp(
        n_sensory, n_inter, n_command, n_motor);
    ASSERT_NE(nullptr, wiring);

    // Convert to ternary matrix
    trit_matrix_t* ternary_mat = createTernaryWiringMatrix(wiring);
    ASSERT_NE(nullptr, ternary_mat);

    // Verify dimensions
    EXPECT_EQ(n_total, ternary_mat->rows);
    EXPECT_EQ(n_total, ternary_mat->cols);

    // Verify hierarchical structure:
    // In CSR/ternary matrix, mat[from][to] = POSITIVE means edge FROM from TO to.
    // To check INCOMING connections to a neuron, we check all rows (sources) for that column (target).

    // Sensory neurons should NOT receive connections (check column for sensory neurons)
    size_t sensory_inputs = 0;
    for (uint32_t col = 0; col < n_sensory; col++) {
        for (uint32_t row = 0; row < n_total; row++) {
            if (trit_matrix_get(ternary_mat, row, col) == TRIT_POSITIVE) {
                sensory_inputs++;
            }
        }
    }
    // Sensory neurons should have NO incoming connections (they are input-only)
    EXPECT_EQ(sensory_inputs, 0u)
        << "Sensory neurons should have no incoming connections";

    // Motor neurons should have incoming connections from command neurons
    // Check columns for motor neurons
    size_t motor_inputs = 0;
    for (uint32_t col = n_sensory + n_inter + n_command; col < n_total; col++) {
        for (uint32_t row = 0; row < n_total; row++) {
            if (trit_matrix_get(ternary_mat, row, col) == TRIT_POSITIVE) {
                motor_inputs++;
            }
        }
    }
    EXPECT_GT(motor_inputs, 0u) << "Motor neurons should have incoming connections";

    trit_matrix_destroy(ternary_mat);
    lnn_wiring_destroy(wiring);
}

/**
 * Test ternary connectivity with mixed excitatory/inhibitory weights
 */
TEST_F(LNNTernaryIntegrationTest, MixedExcitatoryInhibitoryTernary) {
    const size_t n_neurons = 10;
    const float excitatory_fraction = 0.6f;

    // Create ternary weight matrix with mixed signs
    trit_matrix_t* weights = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, weights);

    // Assign excitatory and inhibitory weights based on fraction
    size_t excitatory_count = 0;
    size_t inhibitory_count = 0;

    for (size_t i = 0; i < n_neurons; i++) {
        for (size_t j = 0; j < n_neurons; j++) {
            float r = (float)((i * n_neurons + j) % 100) / 100.0f;
            if (r < excitatory_fraction * 0.7f) {  // 60% * 70% = 42% excitatory
                trit_matrix_set(weights, i, j, TRIT_POSITIVE);
                excitatory_count++;
            } else if (r < 0.7f) {  // 28% inhibitory
                trit_matrix_set(weights, i, j, TRIT_NEGATIVE);
                inhibitory_count++;
            }
            // Remaining 30% are UNKNOWN (zero/sparse)
        }
    }

    // Verify distribution
    size_t n_positive, n_unknown, n_negative;
    trit_matrix_count(weights, &n_positive, &n_unknown, &n_negative);

    EXPECT_EQ(excitatory_count, n_positive);
    EXPECT_EQ(inhibitory_count, n_negative);
    EXPECT_GT(n_unknown, 0u) << "Should have some sparse/zero weights";

    // Compute ratio
    float actual_excitatory_ratio = (float)n_positive / (n_positive + n_negative);
    EXPECT_GT(actual_excitatory_ratio, 0.5f)
        << "Excitatory connections should dominate";

    trit_matrix_destroy(weights);
}

/**
 * Test ternary wiring consistency with LNN layer operations
 */
TEST_F(LNNTernaryIntegrationTest, TernaryWiringLayerConsistency) {
    const uint32_t n_inputs = 4;
    const uint32_t n_inter = 8;
    const uint32_t n_command = 4;
    const uint32_t n_outputs = 2;

    // Create NCP network
    lnn_network_t* network = lnn_network_create_ncp(
        n_inputs, n_inter, n_command, n_outputs);
    ASSERT_NE(nullptr, network);
    ASSERT_GT(network->n_layers, 0u);

    // For each layer, verify wiring can be represented as ternary
    for (uint32_t l = 0; l < network->n_layers; l++) {
        lnn_layer_t* layer = network->layers[l];
        ASSERT_NE(nullptr, layer);

        if (layer->wiring) {
            trit_matrix_t* ternary_wiring = createTernaryWiringMatrix(layer->wiring);
            ASSERT_NE(nullptr, ternary_wiring)
                << "Failed to create ternary wiring for layer " << l;

            // Verify dimensions match layer neurons
            EXPECT_EQ(layer->n_neurons, ternary_wiring->rows);
            EXPECT_EQ(layer->n_neurons, ternary_wiring->cols);

            // Verify edge count consistency
            size_t n_positive, n_unknown, n_negative;
            trit_matrix_count(ternary_wiring, &n_positive, &n_unknown, &n_negative);
            EXPECT_EQ(layer->wiring->n_edges, n_positive)
                << "Edge count mismatch for layer " << l;

            trit_matrix_destroy(ternary_wiring);
        }
    }

    lnn_network_destroy(network);
}

/**
 * Test ternary state with different pack modes for efficiency
 */
TEST_F(LNNTernaryIntegrationTest, TernaryPackModeEfficiency) {
    const size_t n_neurons = 100;

    // Create matrices with different pack modes
    trit_matrix_t* unpacked = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_NONE);
    trit_matrix_t* packed_2bit = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_2BIT);
    trit_matrix_t* packed_base243 = trit_matrix_create(n_neurons, n_neurons, TERNARY_PACK_BASE243);

    ASSERT_NE(nullptr, unpacked);
    ASSERT_NE(nullptr, packed_2bit);
    ASSERT_NE(nullptr, packed_base243);

    // Set same values in all matrices
    for (size_t i = 0; i < n_neurons; i++) {
        for (size_t j = 0; j < n_neurons; j++) {
            trit_t val = (trit_t)((i + j) % 3 - 1);  // {-1, 0, 1}
            trit_matrix_set(unpacked, i, j, val);
            trit_matrix_set(packed_2bit, i, j, val);
            trit_matrix_set(packed_base243, i, j, val);
        }
    }

    // Verify values match across all modes
    for (size_t i = 0; i < n_neurons; i++) {
        for (size_t j = 0; j < n_neurons; j++) {
            trit_t v1 = trit_matrix_get(unpacked, i, j);
            trit_t v2 = trit_matrix_get(packed_2bit, i, j);
            trit_t v3 = trit_matrix_get(packed_base243, i, j);

            EXPECT_EQ(v1, v2) << "Mismatch at (" << i << "," << j << ") between unpacked and 2bit";
            EXPECT_EQ(v1, v3) << "Mismatch at (" << i << "," << j << ") between unpacked and base243";
        }
    }

    // Verify memory savings
    // Unpacked: 1 byte per trit = 10000 bytes
    // 2-bit: 4 trits per byte = 2500 bytes (4x savings)
    // Base243: 5 trits per byte = 2000 bytes (5x savings)
    EXPECT_LT(packed_2bit->packed_bytes, n_neurons * n_neurons);
    EXPECT_LT(packed_base243->packed_bytes, packed_2bit->packed_bytes);

    trit_matrix_destroy(unpacked);
    trit_matrix_destroy(packed_2bit);
    trit_matrix_destroy(packed_base243);
}

/**
 * Test ternary adjacency matrix transpose for bidirectional connectivity
 */
TEST_F(LNNTernaryIntegrationTest, TernaryAdjacencyTranspose) {
    const uint32_t n_neurons = 8;

    // Create asymmetric wiring (not all connections bidirectional)
    lnn_wiring_t* wiring = lnn_wiring_create_random(n_neurons, 0.6f, 54321);
    ASSERT_NE(nullptr, wiring);

    trit_matrix_t* adjacency = createTernaryWiringMatrix(wiring);
    ASSERT_NE(nullptr, adjacency);

    // Transpose to get reverse connectivity
    trit_matrix_t* transpose = trit_matrix_transpose(adjacency);
    ASSERT_NE(nullptr, transpose);

    // Verify transpose relationship: A[i,j] == A^T[j,i]
    for (size_t i = 0; i < n_neurons; i++) {
        for (size_t j = 0; j < n_neurons; j++) {
            trit_t a_ij = trit_matrix_get(adjacency, i, j);
            trit_t at_ji = trit_matrix_get(transpose, j, i);
            EXPECT_EQ(a_ij, at_ji) << "Transpose mismatch at (" << i << "," << j << ")";
        }
    }

    // Edge counts should be the same
    size_t n_pos_orig, n_unk_orig, n_neg_orig;
    size_t n_pos_trans, n_unk_trans, n_neg_trans;
    trit_matrix_count(adjacency, &n_pos_orig, &n_unk_orig, &n_neg_orig);
    trit_matrix_count(transpose, &n_pos_trans, &n_unk_trans, &n_neg_trans);

    EXPECT_EQ(n_pos_orig, n_pos_trans);
    EXPECT_EQ(n_unk_orig, n_unk_trans);
    EXPECT_EQ(n_neg_orig, n_neg_trans);

    trit_matrix_destroy(transpose);
    trit_matrix_destroy(adjacency);
    lnn_wiring_destroy(wiring);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
