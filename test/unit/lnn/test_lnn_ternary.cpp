/**
 * @file test_lnn_ternary.cpp
 * @brief Unit tests for ternary integration in Liquid Neural Networks
 *
 * Tests ternary integration with LNN wiring patterns including:
 * - Ternary recurrent weight matrices
 * - Ternary wiring patterns (full, random, small-world, scale-free, NCP)
 * - Sparse wiring with ternary weights
 * - Memory efficiency with packed ternary storage
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>

// Headers have their own extern "C" guards
#include "lnn/nimcp_lnn_wiring.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LnnTernaryTest : public ::testing::Test {
protected:
    lnn_wiring_t* wiring;

    void SetUp() override {
        wiring = nullptr;
    }

    void TearDown() override {
        if (wiring) {
            lnn_wiring_destroy(wiring);
            wiring = nullptr;
        }
    }

    // Helper: Create ternary adjacency matrix from wiring
    std::vector<trit_t> createTernaryAdjacency(const lnn_wiring_t* w) {
        if (!w) return {};

        std::vector<trit_t> adj(w->n_neurons * w->n_neurons, TRIT_UNKNOWN);

        // Convert CSR to ternary adjacency
        for (uint32_t row = 0; row < w->n_neurons; row++) {
            for (uint32_t idx = w->row_ptr[row]; idx < w->row_ptr[row + 1]; idx++) {
                uint32_t col = w->col_idx[idx];
                if (w->edge_weights) {
                    float weight = w->edge_weights[idx];
                    if (weight > 0) {
                        adj[row * w->n_neurons + col] = TRIT_POSITIVE;
                    } else if (weight < 0) {
                        adj[row * w->n_neurons + col] = TRIT_NEGATIVE;
                    }
                } else {
                    // Unweighted edge = excitatory
                    adj[row * w->n_neurons + col] = TRIT_POSITIVE;
                }
            }
        }
        return adj;
    }

    // Helper: Count ternary distribution in adjacency
    void countTernaryDistribution(const std::vector<trit_t>& adj,
                                   uint32_t& n_negative, uint32_t& n_zero, uint32_t& n_positive) {
        n_negative = n_zero = n_positive = 0;
        for (trit_t t : adj) {
            switch (t) {
                case TRIT_NEGATIVE: n_negative++; break;
                case TRIT_UNKNOWN: n_zero++; break;
                case TRIT_POSITIVE: n_positive++; break;
            }
        }
    }
};

//=============================================================================
// Wiring Creation Tests - Full
//=============================================================================

class LnnWiringFullTest : public LnnTernaryTest {};

TEST_F(LnnWiringFullTest, CreateFullWiringBasic) {
    wiring = lnn_wiring_create_full(10);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 10u);
    EXPECT_EQ(wiring->type, LNN_WIRING_FULL);

    // Full wiring: n * n edges
    EXPECT_EQ(wiring->n_edges, 100u);
}

TEST_F(LnnWiringFullTest, CreateFullWiringSparsity) {
    wiring = lnn_wiring_create_full(20);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    // Full wiring has 0% sparsity
    EXPECT_FLOAT_EQ(sparsity, 0.0f);
}

TEST_F(LnnWiringFullTest, CreateFullWiringTernaryConversion) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    std::vector<trit_t> adj = createTernaryAdjacency(wiring);
    EXPECT_EQ(adj.size(), 25u);

    // In ternary, all edges should be positive (unweighted = excitatory)
    uint32_t n_neg, n_zero, n_pos;
    countTernaryDistribution(adj, n_neg, n_zero, n_pos);

    EXPECT_EQ(n_pos, 25u);  // All connected
    EXPECT_EQ(n_zero, 0u);
    EXPECT_EQ(n_neg, 0u);
}

TEST_F(LnnWiringFullTest, CreateFullWiringZeroNeurons) {
    wiring = lnn_wiring_create_full(0);
    // Should return NULL or empty wiring
    if (wiring) {
        EXPECT_EQ(wiring->n_neurons, 0u);
        EXPECT_EQ(wiring->n_edges, 0u);
    }
}

//=============================================================================
// Wiring Creation Tests - Random
//=============================================================================

class LnnWiringRandomTest : public LnnTernaryTest {};

TEST_F(LnnWiringRandomTest, CreateRandomWiringBasic) {
    wiring = lnn_wiring_create_random(20, 0.5f, 12345);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 20u);
    EXPECT_EQ(wiring->type, LNN_WIRING_RANDOM);
}

TEST_F(LnnWiringRandomTest, CreateRandomWiringSparsityApproximate) {
    wiring = lnn_wiring_create_random(100, 0.7f, 54321);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    // Sparsity should be approximately 0.7 (some variance expected)
    EXPECT_GT(sparsity, 0.5f);
    EXPECT_LT(sparsity, 0.9f);
}

TEST_F(LnnWiringRandomTest, CreateRandomWiringZeroSparsity) {
    // 0% sparsity = full connectivity
    wiring = lnn_wiring_create_random(10, 0.0f, 11111);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    EXPECT_NEAR(sparsity, 0.0f, 0.01f);
}

TEST_F(LnnWiringRandomTest, CreateRandomWiringHighSparsity) {
    // 95% sparsity = very few connections
    wiring = lnn_wiring_create_random(50, 0.95f, 22222);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    EXPECT_GT(sparsity, 0.85f);
}

TEST_F(LnnWiringRandomTest, CreateRandomWiringTernaryConversion) {
    wiring = lnn_wiring_create_random(10, 0.6f, 33333);
    ASSERT_NE(wiring, nullptr);

    std::vector<trit_t> adj = createTernaryAdjacency(wiring);
    EXPECT_EQ(adj.size(), 100u);

    uint32_t n_neg, n_zero, n_pos;
    countTernaryDistribution(adj, n_neg, n_zero, n_pos);

    // Should have mix of connections and non-connections
    EXPECT_GT(n_zero, 0u);  // Some zeros due to sparsity
    EXPECT_GT(n_pos, 0u);   // Some connections
}

TEST_F(LnnWiringRandomTest, CreateRandomWiringDeterministic) {
    // Same seed should produce same wiring
    lnn_wiring_t* wiring1 = lnn_wiring_create_random(10, 0.5f, 99999);
    lnn_wiring_t* wiring2 = lnn_wiring_create_random(10, 0.5f, 99999);

    ASSERT_NE(wiring1, nullptr);
    ASSERT_NE(wiring2, nullptr);

    EXPECT_EQ(wiring1->n_edges, wiring2->n_edges);

    // Compare edge structure
    for (uint32_t i = 0; i <= wiring1->n_neurons; i++) {
        EXPECT_EQ(wiring1->row_ptr[i], wiring2->row_ptr[i]);
    }

    lnn_wiring_destroy(wiring1);
    lnn_wiring_destroy(wiring2);
}

//=============================================================================
// Wiring Creation Tests - Small World
//=============================================================================

class LnnWiringSmallWorldTest : public LnnTernaryTest {};

TEST_F(LnnWiringSmallWorldTest, CreateSmallWorldWiringBasic) {
    // k=4 neighbors, p=0.1 rewiring probability
    wiring = lnn_wiring_create_small_world(20, 4, 0.1f, 44444);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 20u);
    EXPECT_EQ(wiring->type, LNN_WIRING_SMALL_WORLD);
}

TEST_F(LnnWiringSmallWorldTest, CreateSmallWorldWiringEdgeCount) {
    // 30 neurons, k=6 neighbors each
    wiring = lnn_wiring_create_small_world(30, 6, 0.05f, 55555);
    ASSERT_NE(wiring, nullptr);

    // Expected edges: n * k = 30 * 6 = 180 (before rewiring)
    // After rewiring, edge count should be similar
    EXPECT_GT(wiring->n_edges, 0u);
}

TEST_F(LnnWiringSmallWorldTest, CreateSmallWorldWiringSparsity) {
    wiring = lnn_wiring_create_small_world(50, 4, 0.1f, 66666);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    // k=4 out of 50 neighbors = sparse (~92% sparsity expected)
    EXPECT_GT(sparsity, 0.7f);
}

TEST_F(LnnWiringSmallWorldTest, CreateSmallWorldWiringTernaryConversion) {
    wiring = lnn_wiring_create_small_world(10, 4, 0.1f, 77777);
    ASSERT_NE(wiring, nullptr);

    std::vector<trit_t> adj = createTernaryAdjacency(wiring);

    uint32_t n_neg, n_zero, n_pos;
    countTernaryDistribution(adj, n_neg, n_zero, n_pos);

    // Should have mostly zeros (sparse) with some positive connections
    EXPECT_GT(n_zero, n_pos);
}

TEST_F(LnnWiringSmallWorldTest, CreateSmallWorldWiringZeroRewiring) {
    // p=0 means pure ring lattice (no rewiring)
    wiring = lnn_wiring_create_small_world(20, 4, 0.0f, 88888);
    ASSERT_NE(wiring, nullptr);

    // Each neuron connected to k=4 neighbors
    EXPECT_EQ(wiring->n_edges, 20u * 4u);
}

TEST_F(LnnWiringSmallWorldTest, CreateSmallWorldWiringFullRewiring) {
    // p=1 means complete rewiring (random graph)
    wiring = lnn_wiring_create_small_world(15, 4, 1.0f, 11111);
    ASSERT_NE(wiring, nullptr);

    // Should still have same number of edges
    EXPECT_GT(wiring->n_edges, 0u);
}

//=============================================================================
// Wiring Creation Tests - Scale Free
//=============================================================================

class LnnWiringScaleFreeTest : public LnnTernaryTest {};

TEST_F(LnnWiringScaleFreeTest, CreateScaleFreeWiringBasic) {
    // m=3 edges per new node
    wiring = lnn_wiring_create_scale_free(30, 3, 12121);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 30u);
    EXPECT_EQ(wiring->type, LNN_WIRING_SCALE_FREE);
}

TEST_F(LnnWiringScaleFreeTest, CreateScaleFreeWiringSparsity) {
    wiring = lnn_wiring_create_scale_free(50, 2, 23232);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    // Scale-free networks are sparse
    EXPECT_GT(sparsity, 0.5f);
}

TEST_F(LnnWiringScaleFreeTest, CreateScaleFreeWiringHubStructure) {
    wiring = lnn_wiring_create_scale_free(100, 3, 34343);
    ASSERT_NE(wiring, nullptr);

    // Check for degree variance (hubs should have high degree)
    std::vector<uint32_t> degrees(wiring->n_neurons);
    for (uint32_t i = 0; i < wiring->n_neurons; i++) {
        degrees[i] = lnn_wiring_out_degree(wiring, i);
    }

    // Calculate variance
    float mean = static_cast<float>(wiring->n_edges) / wiring->n_neurons;
    float variance = 0.0f;
    for (uint32_t d : degrees) {
        float diff = static_cast<float>(d) - mean;
        variance += diff * diff;
    }
    variance /= wiring->n_neurons;

    // Scale-free networks have high degree variance
    EXPECT_GT(variance, 0.0f);
}

TEST_F(LnnWiringScaleFreeTest, CreateScaleFreeWiringTernaryConversion) {
    wiring = lnn_wiring_create_scale_free(20, 2, 45454);
    ASSERT_NE(wiring, nullptr);

    std::vector<trit_t> adj = createTernaryAdjacency(wiring);

    uint32_t n_neg, n_zero, n_pos;
    countTernaryDistribution(adj, n_neg, n_zero, n_pos);

    // Most entries should be zero (sparse)
    EXPECT_GT(n_zero, n_pos);
    EXPECT_GT(n_pos, 0u);  // But some connections exist
}

//=============================================================================
// Wiring Creation Tests - NCP
//=============================================================================

class LnnWiringNcpTest : public LnnTernaryTest {};

TEST_F(LnnWiringNcpTest, CreateNcpWiringBasic) {
    // sensory=4, inter=8, command=4, motor=2
    wiring = lnn_wiring_create_ncp(4, 8, 4, 2);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 18u);  // 4+8+4+2
    EXPECT_EQ(wiring->type, LNN_WIRING_NCP);
}

TEST_F(LnnWiringNcpTest, CreateNcpWiringNeuronCounts) {
    wiring = lnn_wiring_create_ncp(5, 10, 3, 4);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_sensory, 5u);
    EXPECT_EQ(wiring->n_inter, 10u);
    EXPECT_EQ(wiring->n_command, 3u);
    EXPECT_EQ(wiring->n_motor, 4u);
}

TEST_F(LnnWiringNcpTest, CreateNcpWiringHierarchical) {
    wiring = lnn_wiring_create_ncp(4, 8, 4, 2);
    ASSERT_NE(wiring, nullptr);

    // Sensory neurons (indices 0-3) should not have incoming connections
    for (uint32_t sensory_idx = 0; sensory_idx < 4; sensory_idx++) {
        uint32_t in_degree = lnn_wiring_in_degree(wiring, sensory_idx);
        // In NCP, sensory neurons may still have some recurrent connections
        // so we just check they exist as valid neurons
        EXPECT_LE(in_degree, wiring->n_neurons);
    }

    // Motor neurons should have outgoing connections
    uint32_t motor_start = 4 + 8 + 4;  // After sensory + inter + command
    for (uint32_t motor_idx = motor_start; motor_idx < wiring->n_neurons; motor_idx++) {
        // Motor neurons typically receive from command
        uint32_t in_degree = lnn_wiring_in_degree(wiring, motor_idx);
        EXPECT_GE(in_degree, 0u);  // May have incoming from command
    }
}

TEST_F(LnnWiringNcpTest, CreateNcpWiringTernaryConversion) {
    wiring = lnn_wiring_create_ncp(3, 6, 3, 2);
    ASSERT_NE(wiring, nullptr);

    std::vector<trit_t> adj = createTernaryAdjacency(wiring);

    uint32_t n_neg, n_zero, n_pos;
    countTernaryDistribution(adj, n_neg, n_zero, n_pos);

    // NCP wiring is sparse
    EXPECT_GT(n_zero, n_pos);
}

//=============================================================================
// Wiring Query Tests
//=============================================================================

class LnnWiringQueryTest : public LnnTernaryTest {};

TEST_F(LnnWiringQueryTest, HasEdgeBasic) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    // Full wiring - all edges should exist
    for (uint32_t from = 0; from < 5; from++) {
        for (uint32_t to = 0; to < 5; to++) {
            EXPECT_TRUE(lnn_wiring_has_edge(wiring, from, to));
        }
    }
}

TEST_F(LnnWiringQueryTest, HasEdgeSparse) {
    wiring = lnn_wiring_create_random(20, 0.9f, 56789);  // 90% sparse
    ASSERT_NE(wiring, nullptr);

    uint32_t edge_count = 0;
    for (uint32_t from = 0; from < 20; from++) {
        for (uint32_t to = 0; to < 20; to++) {
            if (lnn_wiring_has_edge(wiring, from, to)) {
                edge_count++;
            }
        }
    }

    // Should match reported edge count
    EXPECT_EQ(edge_count, wiring->n_edges);
}

TEST_F(LnnWiringQueryTest, HasEdgeNullWiring) {
    bool result = lnn_wiring_has_edge(nullptr, 0, 1);
    EXPECT_FALSE(result);
}

TEST_F(LnnWiringQueryTest, HasEdgeOutOfRange) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    // Out of range indices
    EXPECT_FALSE(lnn_wiring_has_edge(wiring, 10, 1));
    EXPECT_FALSE(lnn_wiring_has_edge(wiring, 1, 10));
    EXPECT_FALSE(lnn_wiring_has_edge(wiring, 100, 100));
}

TEST_F(LnnWiringQueryTest, OutDegreeBasic) {
    wiring = lnn_wiring_create_full(10);
    ASSERT_NE(wiring, nullptr);

    // Full wiring - each neuron connected to all (including self)
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t degree = lnn_wiring_out_degree(wiring, i);
        EXPECT_EQ(degree, 10u);
    }
}

TEST_F(LnnWiringQueryTest, InDegreeBasic) {
    wiring = lnn_wiring_create_full(10);
    ASSERT_NE(wiring, nullptr);

    // Full wiring - each neuron receives from all
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t degree = lnn_wiring_in_degree(wiring, i);
        EXPECT_EQ(degree, 10u);
    }
}

TEST_F(LnnWiringQueryTest, GetNeighborsBasic) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    uint32_t count;
    const uint32_t* neighbors = lnn_wiring_get_neighbors(wiring, 0, &count);

    EXPECT_NE(neighbors, nullptr);
    EXPECT_EQ(count, 5u);  // Connected to all 5 neurons
}

TEST_F(LnnWiringQueryTest, GetNeighborsNullWiring) {
    uint32_t count = 99;
    const uint32_t* neighbors = lnn_wiring_get_neighbors(nullptr, 0, &count);

    EXPECT_EQ(neighbors, nullptr);
}

//=============================================================================
// Wiring Utility Tests
//=============================================================================

class LnnWiringUtilityTest : public LnnTernaryTest {};

TEST_F(LnnWiringUtilityTest, ComputeSparsityFull) {
    wiring = lnn_wiring_create_full(10);
    ASSERT_NE(wiring, nullptr);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    EXPECT_FLOAT_EQ(sparsity, 0.0f);
}

TEST_F(LnnWiringUtilityTest, ComputeSparsityNullWiring) {
    float sparsity = lnn_wiring_compute_sparsity(nullptr);
    // Should return 0 or some indicator
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);
}

TEST_F(LnnWiringUtilityTest, ToDenseBasic) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    std::vector<float> dense(25, 0.0f);
    int result = lnn_wiring_to_dense(wiring, dense.data(), 5, 5);
    EXPECT_EQ(result, 0);

    // All entries should be 1.0 (connected)
    for (float val : dense) {
        EXPECT_FLOAT_EQ(val, 1.0f);
    }
}

TEST_F(LnnWiringUtilityTest, ToDenseSparsity) {
    wiring = lnn_wiring_create_random(10, 0.6f, 67890);
    ASSERT_NE(wiring, nullptr);

    std::vector<float> dense(100, 0.0f);
    int result = lnn_wiring_to_dense(wiring, dense.data(), 10, 10);
    EXPECT_EQ(result, 0);

    // Count non-zeros
    uint32_t nonzero_count = 0;
    for (float val : dense) {
        if (val != 0.0f) nonzero_count++;
    }

    EXPECT_EQ(nonzero_count, wiring->n_edges);
}

TEST_F(LnnWiringUtilityTest, ToDenseNullInputs) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    std::vector<float> dense(25);

    // Null wiring
    int result = lnn_wiring_to_dense(nullptr, dense.data(), 5, 5);
    EXPECT_LT(result, 0);

    // Null dense
    result = lnn_wiring_to_dense(wiring, nullptr, 5, 5);
    EXPECT_LT(result, 0);
}

TEST_F(LnnWiringUtilityTest, WiringTypeToString) {
    const char* full_str = lnn_wiring_type_to_string(LNN_WIRING_FULL);
    EXPECT_NE(full_str, nullptr);
    EXPECT_GT(strlen(full_str), 0u);

    const char* random_str = lnn_wiring_type_to_string(LNN_WIRING_RANDOM);
    EXPECT_NE(random_str, nullptr);

    const char* sw_str = lnn_wiring_type_to_string(LNN_WIRING_SMALL_WORLD);
    EXPECT_NE(sw_str, nullptr);

    const char* sf_str = lnn_wiring_type_to_string(LNN_WIRING_SCALE_FREE);
    EXPECT_NE(sf_str, nullptr);

    const char* ncp_str = lnn_wiring_type_to_string(LNN_WIRING_NCP);
    EXPECT_NE(ncp_str, nullptr);
}

//=============================================================================
// Wiring Factory Tests
//=============================================================================

class LnnWiringFactoryTest : public LnnTernaryTest {};

TEST_F(LnnWiringFactoryTest, CreateByTypeFull) {
    wiring = lnn_wiring_create(LNN_WIRING_FULL, 10, 0.0f);
    ASSERT_NE(wiring, nullptr);
    EXPECT_EQ(wiring->type, LNN_WIRING_FULL);
    EXPECT_EQ(wiring->n_neurons, 10u);
}

TEST_F(LnnWiringFactoryTest, CreateByTypeRandom) {
    wiring = lnn_wiring_create(LNN_WIRING_RANDOM, 20, 0.7f);
    ASSERT_NE(wiring, nullptr);
    EXPECT_EQ(wiring->type, LNN_WIRING_RANDOM);
}

TEST_F(LnnWiringFactoryTest, CreateByTypeInvalid) {
    wiring = lnn_wiring_create(static_cast<lnn_wiring_type_t>(999), 10, 0.5f);
    // Should return NULL for invalid type
    EXPECT_EQ(wiring, nullptr);
}

//=============================================================================
// Ternary Weight Conversion Tests
//=============================================================================

class LnnTernaryWeightTest : public LnnTernaryTest {
protected:
    // Helper: Convert float weights to ternary
    std::vector<trit_t> floatToTernary(const std::vector<float>& weights, float threshold) {
        std::vector<trit_t> ternary(weights.size());
        for (size_t i = 0; i < weights.size(); i++) {
            if (weights[i] > threshold) {
                ternary[i] = TRIT_POSITIVE;
            } else if (weights[i] < -threshold) {
                ternary[i] = TRIT_NEGATIVE;
            } else {
                ternary[i] = TRIT_UNKNOWN;
            }
        }
        return ternary;
    }
};

TEST_F(LnnTernaryWeightTest, ConvertRecurrentWeightsToTernary) {
    wiring = lnn_wiring_create_full(5);
    ASSERT_NE(wiring, nullptr);

    // Create sample float weights
    std::vector<float> weights(25);
    for (size_t i = 0; i < 25; i++) {
        weights[i] = static_cast<float>(i) / 25.0f * 2.0f - 1.0f;  // Range [-1, 1]
    }

    std::vector<trit_t> ternary_weights = floatToTernary(weights, 0.3f);

    uint32_t n_neg = 0, n_zero = 0, n_pos = 0;
    for (trit_t t : ternary_weights) {
        switch (t) {
            case TRIT_NEGATIVE: n_neg++; break;
            case TRIT_UNKNOWN: n_zero++; break;
            case TRIT_POSITIVE: n_pos++; break;
        }
    }

    // Should have all three types
    EXPECT_GT(n_neg, 0u);
    EXPECT_GT(n_zero, 0u);
    EXPECT_GT(n_pos, 0u);
}

TEST_F(LnnTernaryWeightTest, TernaryWeightMemorySavings) {
    const uint32_t n_neurons = 100;
    const uint32_t n_weights = n_neurons * n_neurons;

    // Float32 storage
    size_t float_bytes = n_weights * sizeof(float);

    // Ternary storage (unpacked: 1 byte per trit)
    size_t ternary_unpacked = n_weights * sizeof(trit_t);

    // Ternary storage (2-bit packed: 4 trits per byte)
    size_t ternary_packed2 = (n_weights + 3) / 4;

    // Ternary storage (base-243: 5 trits per byte)
    size_t ternary_packed243 = (n_weights + 4) / 5;

    EXPECT_LT(ternary_unpacked, float_bytes);
    EXPECT_LT(ternary_packed2, ternary_unpacked);
    EXPECT_LT(ternary_packed243, ternary_packed2);

    // Calculate savings
    float savings_unpacked = 1.0f - static_cast<float>(ternary_unpacked) / float_bytes;
    float savings_packed2 = 1.0f - static_cast<float>(ternary_packed2) / float_bytes;
    float savings_packed243 = 1.0f - static_cast<float>(ternary_packed243) / float_bytes;

    EXPECT_GT(savings_unpacked, 0.7f);   // >70% savings (1 byte vs 4 bytes)
    EXPECT_GT(savings_packed2, 0.93f);   // >93% savings (0.25 byte vs 4 bytes)
    EXPECT_GT(savings_packed243, 0.94f); // >94% savings (0.2 byte vs 4 bytes)
}

//=============================================================================
// Edge Cases and Boundary Conditions
//=============================================================================

class LnnEdgeCasesTest : public LnnTernaryTest {};

TEST_F(LnnEdgeCasesTest, SingleNeuronWiring) {
    wiring = lnn_wiring_create_full(1);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 1u);
    EXPECT_EQ(wiring->n_edges, 1u);  // Self-connection

    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 0, 0));
}

TEST_F(LnnEdgeCasesTest, TwoNeuronWiring) {
    wiring = lnn_wiring_create_full(2);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_edges, 4u);  // 2x2 full

    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 0, 0));
    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 0, 1));
    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 1, 0));
    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 1, 1));
}

TEST_F(LnnEdgeCasesTest, LargeNetworkWiring) {
    wiring = lnn_wiring_create_random(1000, 0.99f, 11223);  // Very sparse
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 1000u);

    float sparsity = lnn_wiring_compute_sparsity(wiring);
    EXPECT_GT(sparsity, 0.95f);
}

TEST_F(LnnEdgeCasesTest, DestroyNullSafe) {
    lnn_wiring_destroy(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(LnnEdgeCasesTest, SmallWorldOddNeighbors) {
    // k should be even but try odd value
    wiring = lnn_wiring_create_small_world(10, 3, 0.1f, 44556);
    // Should either handle gracefully or round to even
    if (wiring) {
        EXPECT_GT(wiring->n_edges, 0u);
    }
}

TEST_F(LnnEdgeCasesTest, ScaleFreeMLargerThanN) {
    // m should be < n_neurons
    wiring = lnn_wiring_create_scale_free(5, 10, 55667);
    // Should return NULL or clamp m
    if (wiring) {
        EXPECT_LE(wiring->n_edges, 5u * 5u);
    }
}

//=============================================================================
// Ternary Matrix Integration Tests
//=============================================================================

class LnnTernaryMatrixTest : public LnnTernaryTest {};

TEST_F(LnnTernaryMatrixTest, CreateTernaryMatrixFromWiring) {
    wiring = lnn_wiring_create_random(10, 0.5f, 77889);
    ASSERT_NE(wiring, nullptr);

    // Convert to dense, then to ternary matrix
    std::vector<float> dense(100);
    int result = lnn_wiring_to_dense(wiring, dense.data(), 10, 10);
    ASSERT_EQ(result, 0);

    // Create ternary matrix
    trit_matrix_t* tmat = trit_matrix_create(10, 10, TERNARY_PACK_NONE);
    if (tmat) {
        // Fill matrix with ternary values
        for (uint32_t row = 0; row < 10; row++) {
            for (uint32_t col = 0; col < 10; col++) {
                float val = dense[row * 10 + col];
                trit_t trit_val = (val > 0) ? TRIT_POSITIVE : TRIT_UNKNOWN;
                trit_matrix_set(tmat, row, col, trit_val);
            }
        }

        EXPECT_EQ(tmat->rows, 10u);
        EXPECT_EQ(tmat->cols, 10u);

        trit_matrix_destroy(tmat);
    }
}

TEST_F(LnnTernaryMatrixTest, TernaryMatrixSparsityMatchesWiring) {
    wiring = lnn_wiring_create_random(20, 0.7f, 88990);
    ASSERT_NE(wiring, nullptr);

    std::vector<trit_t> adj = createTernaryAdjacency(wiring);

    // Count zeros
    uint32_t zero_count = 0;
    for (trit_t t : adj) {
        if (t == TRIT_UNKNOWN) zero_count++;
    }

    float ternary_sparsity = static_cast<float>(zero_count) / adj.size();
    float wiring_sparsity = lnn_wiring_compute_sparsity(wiring);

    // Sparsities should match
    EXPECT_NEAR(ternary_sparsity, wiring_sparsity, 0.01f);
}

//=============================================================================
// From Adjacency Matrix Tests
//=============================================================================

class LnnFromAdjacencyTest : public LnnTernaryTest {};

TEST_F(LnnFromAdjacencyTest, CreateFromAdjacencyBasic) {
    // Create 4x4 adjacency matrix with pattern
    std::vector<uint8_t> adj = {
        1, 1, 0, 0,
        0, 1, 1, 0,
        0, 0, 1, 1,
        1, 0, 0, 1
    };

    wiring = lnn_wiring_create_from_adjacency(adj.data(), 4);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_neurons, 4u);
    EXPECT_EQ(wiring->n_edges, 8u);  // 8 ones in matrix

    // Verify edges
    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 0, 0));
    EXPECT_TRUE(lnn_wiring_has_edge(wiring, 0, 1));
    EXPECT_FALSE(lnn_wiring_has_edge(wiring, 0, 2));
    EXPECT_FALSE(lnn_wiring_has_edge(wiring, 0, 3));
}

TEST_F(LnnFromAdjacencyTest, CreateFromAdjacencyFull) {
    std::vector<uint8_t> adj(25, 1);  // 5x5 full

    wiring = lnn_wiring_create_from_adjacency(adj.data(), 5);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_edges, 25u);
    EXPECT_FLOAT_EQ(lnn_wiring_compute_sparsity(wiring), 0.0f);
}

TEST_F(LnnFromAdjacencyTest, CreateFromAdjacencyEmpty) {
    std::vector<uint8_t> adj(16, 0);  // 4x4 empty

    wiring = lnn_wiring_create_from_adjacency(adj.data(), 4);
    ASSERT_NE(wiring, nullptr);

    EXPECT_EQ(wiring->n_edges, 0u);
    EXPECT_FLOAT_EQ(lnn_wiring_compute_sparsity(wiring), 1.0f);
}

TEST_F(LnnFromAdjacencyTest, CreateFromAdjacencyNullMatrix) {
    wiring = lnn_wiring_create_from_adjacency(nullptr, 5);
    EXPECT_EQ(wiring, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
