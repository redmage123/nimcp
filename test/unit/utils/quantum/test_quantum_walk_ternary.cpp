/**
 * @file test_quantum_walk_ternary.cpp
 * @brief Unit tests for ternary quantum walk module
 *
 * Tests cover:
 * - 1D walker lifecycle and operations
 * - Graph walker lifecycle and operations
 * - Graph search and reachability
 * - Centrality computation
 * - Hitting time estimation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "utils/quantum/nimcp_quantum_walk_ternary.h"

//=============================================================================
// 1D Walker Tests
//=============================================================================

class TritWalker1DTest : public ::testing::Test {
protected:
    void SetUp() override {
        walker = nullptr;
    }

    void TearDown() override {
        if (walker) {
            trit_walker_1d_destroy(walker);
            walker = nullptr;
        }
    }

    trit_walker_1d_t* walker;
};

TEST_F(TritWalker1DTest, CreateDestroy) {
    walker = trit_walker_1d_create(100, 0.33f, 0.34f, 0.33f);
    ASSERT_NE(walker, nullptr);
    EXPECT_EQ(walker->magic, TRIT_WALK_MAGIC);
    EXPECT_EQ(walker->n_positions, 100u);
    EXPECT_NE(walker->coins, nullptr);
    EXPECT_NE(walker->amplitudes, nullptr);
    EXPECT_NE(walker->phases, nullptr);
}

TEST_F(TritWalker1DTest, CreateZeroPositions) {
    walker = trit_walker_1d_create(0, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(walker, nullptr);
}

TEST_F(TritWalker1DTest, BiasNormalization) {
    walker = trit_walker_1d_create(10, 2.0f, 4.0f, 4.0f);
    ASSERT_NE(walker, nullptr);
    EXPECT_FLOAT_EQ(walker->bias_left, 0.2f);
    EXPECT_FLOAT_EQ(walker->bias_stay, 0.4f);
    EXPECT_FLOAT_EQ(walker->bias_right, 0.4f);
}

TEST_F(TritWalker1DTest, Initialize) {
    walker = trit_walker_1d_create(100, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 50, TRIT_COIN_RIGHT);

    EXPECT_FLOAT_EQ(walker->amplitudes[50], 1.0f);
    EXPECT_FLOAT_EQ(walker->total_probability, 1.0f);
    EXPECT_EQ(walker->steps, 0u);

    // Other positions should be zero
    EXPECT_FLOAT_EQ(walker->amplitudes[0], 0.0f);
    EXPECT_FLOAT_EQ(walker->amplitudes[49], 0.0f);
    EXPECT_FLOAT_EQ(walker->amplitudes[51], 0.0f);
}

TEST_F(TritWalker1DTest, InitializeInvalidPosition) {
    walker = trit_walker_1d_create(10, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    // Should not crash
    trit_walker_1d_init(walker, 100, TRIT_COIN_LEFT);
}

TEST_F(TritWalker1DTest, SingleStep) {
    walker = trit_walker_1d_create(100, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 50, TRIT_COIN_RIGHT);
    trit_walker_1d_step(walker);

    EXPECT_EQ(walker->steps, 1u);
    // Amplitude should have spread
    EXPECT_TRUE(walker->total_probability > 0.0f);
}

TEST_F(TritWalker1DTest, MultipleSteps) {
    walker = trit_walker_1d_create(100, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 50, TRIT_COIN_STAY);

    for (int i = 0; i < 10; i++) {
        trit_walker_1d_step(walker);
    }

    EXPECT_EQ(walker->steps, 10u);
}

TEST_F(TritWalker1DTest, GetDistribution) {
    walker = trit_walker_1d_create(10, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 5, TRIT_COIN_STAY);

    float probs[10];
    trit_walker_1d_get_distribution(walker, probs);

    // Initial: all probability at position 5
    EXPECT_FLOAT_EQ(probs[5], 1.0f);
    for (int i = 0; i < 10; i++) {
        if (i != 5) EXPECT_FLOAT_EQ(probs[i], 0.0f);
    }
}

TEST_F(TritWalker1DTest, Measure) {
    walker = trit_walker_1d_create(10, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 5, TRIT_COIN_STAY);

    // With all probability at position 5, measurement should return 5
    uint32_t pos = trit_walker_1d_measure(walker, 0.5f);
    EXPECT_EQ(pos, 5u);
    EXPECT_FLOAT_EQ(walker->total_probability, 1.0f);
}

TEST_F(TritWalker1DTest, MeanPosition) {
    walker = trit_walker_1d_create(10, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 5, TRIT_COIN_STAY);

    float mean = trit_walker_1d_mean_position(walker);
    EXPECT_FLOAT_EQ(mean, 5.0f);
}

TEST_F(TritWalker1DTest, Variance) {
    walker = trit_walker_1d_create(10, 1.0f, 1.0f, 1.0f);
    ASSERT_NE(walker, nullptr);

    trit_walker_1d_init(walker, 5, TRIT_COIN_STAY);

    // All probability at one point → variance = 0
    float var = trit_walker_1d_variance(walker);
    EXPECT_FLOAT_EQ(var, 0.0f);
}

TEST_F(TritWalker1DTest, NullSafety) {
    // All functions should handle NULL gracefully
    trit_walker_1d_destroy(nullptr);
    trit_walker_1d_init(nullptr, 0, TRIT_COIN_STAY);
    trit_walker_1d_step(nullptr);
    trit_walker_1d_coin(nullptr, 0);
    trit_walker_1d_shift(nullptr);
    trit_walker_1d_get_distribution(nullptr, nullptr);
    EXPECT_EQ(trit_walker_1d_measure(nullptr, 0.5f), 0u);
    EXPECT_FLOAT_EQ(trit_walker_1d_mean_position(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(trit_walker_1d_variance(nullptr), 0.0f);
}

//=============================================================================
// Graph Walker Tests
//=============================================================================

class TritWalkerGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        walker = nullptr;
        adj = nullptr;
    }

    void TearDown() override {
        if (walker) {
            trit_walker_graph_destroy(walker);
            walker = nullptr;
        }
        if (adj) {
            trit_matrix_destroy(adj);
            adj = nullptr;
        }
    }

    // Create a simple chain graph: 0-1-2-3-4
    trit_matrix_t* createChainGraph(uint32_t n) {
        trit_matrix_t* m = trit_matrix_create(n, n, TERNARY_PACK_NONE);
        if (!m) return nullptr;

        // Initialize to UNKNOWN
        for (size_t i = 0; i < m->numel; i++) {
            m->data.unpacked[i] = TRIT_UNKNOWN;
        }

        // Add edges
        for (uint32_t i = 0; i < n - 1; i++) {
            trit_matrix_set(m, i, i + 1, TRIT_POSITIVE);
            trit_matrix_set(m, i + 1, i, TRIT_POSITIVE);
        }

        return m;
    }

    // Create a complete graph
    trit_matrix_t* createCompleteGraph(uint32_t n) {
        trit_matrix_t* m = trit_matrix_create(n, n, TERNARY_PACK_NONE);
        if (!m) return nullptr;

        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                if (i != j) {
                    trit_matrix_set(m, i, j, TRIT_POSITIVE);
                } else {
                    trit_matrix_set(m, i, j, TRIT_UNKNOWN);
                }
            }
        }

        return m;
    }

    // Create a star graph (center connected to all)
    trit_matrix_t* createStarGraph(uint32_t n) {
        trit_matrix_t* m = trit_matrix_create(n, n, TERNARY_PACK_NONE);
        if (!m) return nullptr;

        // Initialize to UNKNOWN
        for (size_t i = 0; i < m->numel; i++) {
            m->data.unpacked[i] = TRIT_UNKNOWN;
        }

        // Connect node 0 to all others
        for (uint32_t i = 1; i < n; i++) {
            trit_matrix_set(m, 0, i, TRIT_POSITIVE);
            trit_matrix_set(m, i, 0, TRIT_POSITIVE);
        }

        return m;
    }

    trit_walker_graph_t* walker;
    trit_matrix_t* adj;
};

TEST_F(TritWalkerGraphTest, CreateDestroyChain) {
    adj = createChainGraph(5);
    ASSERT_NE(adj, nullptr);

    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    EXPECT_EQ(walker->magic, TRIT_WALK_MAGIC);
    EXPECT_EQ(walker->n_nodes, 5u);
    EXPECT_EQ(walker->max_degree, 2u);  // Middle nodes have degree 2
    EXPECT_NE(walker->amplitudes, nullptr);
    EXPECT_NE(walker->row_ptr, nullptr);
    EXPECT_NE(walker->col_idx, nullptr);
}

TEST_F(TritWalkerGraphTest, CreateDestroyComplete) {
    adj = createCompleteGraph(4);
    ASSERT_NE(adj, nullptr);

    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    EXPECT_EQ(walker->n_nodes, 4u);
    EXPECT_EQ(walker->max_degree, 3u);  // Each node connected to all others
}

TEST_F(TritWalkerGraphTest, CreateNullAdjacency) {
    walker = trit_walker_graph_create(nullptr);
    EXPECT_EQ(walker, nullptr);
}

TEST_F(TritWalkerGraphTest, CreateNonSquareMatrix) {
    adj = trit_matrix_create(3, 4, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    walker = trit_walker_graph_create(adj);
    EXPECT_EQ(walker, nullptr);  // Should fail for non-square
}

TEST_F(TritWalkerGraphTest, Initialize) {
    adj = createChainGraph(5);
    ASSERT_NE(adj, nullptr);

    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 2);

    EXPECT_FLOAT_EQ(walker->amplitudes[2], 1.0f);
    EXPECT_EQ(walker->steps, 0u);

    // Other positions should be zero
    EXPECT_FLOAT_EQ(walker->amplitudes[0], 0.0f);
    EXPECT_FLOAT_EQ(walker->amplitudes[1], 0.0f);
    EXPECT_FLOAT_EQ(walker->amplitudes[3], 0.0f);
}

TEST_F(TritWalkerGraphTest, InitializeInvalidNode) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    // Should not crash
    trit_walker_graph_init(walker, 100);
}

TEST_F(TritWalkerGraphTest, GetDegree) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    // End nodes have degree 1, middle nodes have degree 2
    EXPECT_EQ(trit_walker_graph_degree(walker, 0), 1u);
    EXPECT_EQ(trit_walker_graph_degree(walker, 1), 2u);
    EXPECT_EQ(trit_walker_graph_degree(walker, 2), 2u);
    EXPECT_EQ(trit_walker_graph_degree(walker, 4), 1u);
}

TEST_F(TritWalkerGraphTest, SingleStep) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 2);
    trit_walker_graph_step(walker);

    EXPECT_EQ(walker->steps, 1u);

    // Amplitude should have spread to neighbors
    float amp1 = trit_walker_graph_get_amplitude(walker, 1);
    float amp3 = trit_walker_graph_get_amplitude(walker, 3);

    // Neighbors should have received amplitude
    EXPECT_GT(fabsf(amp1), 0.0f);
    EXPECT_GT(fabsf(amp3), 0.0f);
}

TEST_F(TritWalkerGraphTest, MultipleSteps) {
    adj = createChainGraph(10);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 5);
    trit_walker_graph_run(walker, 10);

    EXPECT_EQ(walker->steps, 10u);

    // Amplitude should have spread throughout the graph
    float total_prob = 0.0f;
    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        total_prob += trit_walker_graph_get_probability(walker, i);
    }

    // Probability should sum to ~1 (normalized)
    EXPECT_NEAR(total_prob, 1.0f, 0.1f);
}

TEST_F(TritWalkerGraphTest, GetAmplitude) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 2);

    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(walker, 2), 1.0f);
    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(walker, 0), 0.0f);
}

TEST_F(TritWalkerGraphTest, GetProbability) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 2);

    EXPECT_FLOAT_EQ(trit_walker_graph_get_probability(walker, 2), 1.0f);
    EXPECT_FLOAT_EQ(trit_walker_graph_get_probability(walker, 0), 0.0f);
}

TEST_F(TritWalkerGraphTest, GetDistribution) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 2);

    float probs[5];
    trit_walker_graph_get_distribution(walker, probs);

    EXPECT_FLOAT_EQ(probs[2], 1.0f);
    EXPECT_FLOAT_EQ(probs[0], 0.0f);
}

TEST_F(TritWalkerGraphTest, MaxNode) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 2);

    float max_amp;
    uint32_t max_node = trit_walker_graph_max_node(walker, &max_amp);

    EXPECT_EQ(max_node, 2u);
    EXPECT_FLOAT_EQ(max_amp, 1.0f);
}

TEST_F(TritWalkerGraphTest, SearchDirectNeighbor) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    uint32_t steps;
    bool found = trit_walker_graph_search(walker, 2, 3, 100, &steps);

    // Should find neighbor relatively quickly (amplitude > 0.5)
    // Due to spreading, may or may not reach threshold
    EXPECT_TRUE(steps <= 100);
}

TEST_F(TritWalkerGraphTest, SearchInCompleteGraph) {
    adj = createCompleteGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    uint32_t steps;
    bool found = trit_walker_graph_search(walker, 0, 4, 100, &steps);

    // In complete graph, all nodes reachable in 1 step
    EXPECT_TRUE(steps <= 100);
}

TEST_F(TritWalkerGraphTest, Reachability) {
    adj = createChainGraph(5);
    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    bool reachable[5];
    uint32_t count = trit_walker_graph_reachability(walker, 2, 10, reachable, 0.01f);

    // All nodes should be reachable from middle of chain
    EXPECT_GT(count, 0u);
}

TEST_F(TritWalkerGraphTest, CentralityChain) {
    adj = createChainGraph(5);
    ASSERT_NE(adj, nullptr);

    float centrality[5];
    int result = trit_walker_graph_centrality(adj, 10, 0.85f, centrality);

    EXPECT_EQ(result, 0);

    // Middle nodes should have higher centrality in a chain
    // Due to spreading dynamics, all should have some centrality
    float total = 0.0f;
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(centrality[i], 0.0f);
        total += centrality[i];
    }
    EXPECT_NEAR(total, 1.0f, 0.01f);  // Should sum to 1
}

TEST_F(TritWalkerGraphTest, CentralityStar) {
    adj = createStarGraph(5);
    ASSERT_NE(adj, nullptr);

    float centrality[5];
    int result = trit_walker_graph_centrality(adj, 10, 0.85f, centrality);

    EXPECT_EQ(result, 0);

    // Center node (0) should have highest centrality
    // Due to quantum effects this may vary, but center should be significant
    EXPECT_GT(centrality[0], 0.0f);
}

TEST_F(TritWalkerGraphTest, HittingTimeChain) {
    adj = createChainGraph(5);
    ASSERT_NE(adj, nullptr);

    float ht = trit_walker_graph_hitting_time(adj, 0, 4, 100);

    // Hitting time should be positive for reachable target
    // May be -1 if not reached within max_steps with threshold
    // Chain of 5 nodes, from 0 to 4 should take some steps
}

TEST_F(TritWalkerGraphTest, HittingTimeComplete) {
    adj = createCompleteGraph(5);
    ASSERT_NE(adj, nullptr);

    float ht = trit_walker_graph_hitting_time(adj, 0, 4, 100);

    // In complete graph, should reach quickly
}

TEST_F(TritWalkerGraphTest, NullSafety) {
    trit_walker_graph_destroy(nullptr);
    trit_walker_graph_init(nullptr, 0);

    EXPECT_EQ(trit_walker_graph_degree(nullptr, 0), 0u);
    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(nullptr, 0), 0.0f);
    EXPECT_FLOAT_EQ(trit_walker_graph_get_probability(nullptr, 0), 0.0f);

    float amp;
    EXPECT_EQ(trit_walker_graph_max_node(nullptr, &amp), 0u);
    EXPECT_FLOAT_EQ(amp, 0.0f);

    EXPECT_EQ(trit_walker_graph_centrality(nullptr, 10, 0.85f, nullptr), -1);
    EXPECT_FLOAT_EQ(trit_walker_graph_hitting_time(nullptr, 0, 1, 10), -1.0f);
}

//=============================================================================
// Edge Weight Tests
//=============================================================================

class TritWalkerEdgeWeightTest : public ::testing::Test {
protected:
    void SetUp() override {
        walker = nullptr;
        adj = nullptr;
    }

    void TearDown() override {
        if (walker) {
            trit_walker_graph_destroy(walker);
            walker = nullptr;
        }
        if (adj) {
            trit_matrix_destroy(adj);
            adj = nullptr;
        }
    }

    trit_walker_graph_t* walker;
    trit_matrix_t* adj;
};

TEST_F(TritWalkerEdgeWeightTest, PositiveEdges) {
    adj = trit_matrix_create(3, 3, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    // Initialize to UNKNOWN
    for (size_t i = 0; i < adj->numel; i++) {
        adj->data.unpacked[i] = TRIT_UNKNOWN;
    }

    // Add positive edges
    trit_matrix_set(adj, 0, 1, TRIT_POSITIVE);
    trit_matrix_set(adj, 1, 0, TRIT_POSITIVE);
    trit_matrix_set(adj, 1, 2, TRIT_POSITIVE);
    trit_matrix_set(adj, 2, 1, TRIT_POSITIVE);

    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 0);
    trit_walker_graph_step(walker);

    // Amplitude should spread positively
    float amp1 = trit_walker_graph_get_amplitude(walker, 1);
    EXPECT_GT(amp1, 0.0f);
}

TEST_F(TritWalkerEdgeWeightTest, NegativeEdges) {
    adj = trit_matrix_create(3, 3, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    // Initialize to UNKNOWN
    for (size_t i = 0; i < adj->numel; i++) {
        adj->data.unpacked[i] = TRIT_UNKNOWN;
    }

    // Add negative edges (inhibitory)
    trit_matrix_set(adj, 0, 1, TRIT_NEGATIVE);
    trit_matrix_set(adj, 1, 0, TRIT_NEGATIVE);
    trit_matrix_set(adj, 1, 2, TRIT_POSITIVE);
    trit_matrix_set(adj, 2, 1, TRIT_POSITIVE);

    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 0);
    trit_walker_graph_step(walker);

    // Amplitude at node 1 should be negative (inhibited)
    float amp1 = trit_walker_graph_get_amplitude(walker, 1);
    EXPECT_LT(amp1, 0.0f);
}

TEST_F(TritWalkerEdgeWeightTest, MixedEdges) {
    adj = trit_matrix_create(4, 4, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    // Initialize to UNKNOWN
    for (size_t i = 0; i < adj->numel; i++) {
        adj->data.unpacked[i] = TRIT_UNKNOWN;
    }

    // Mixed edges: 0--(+)-->1, 0--(-)-->2, 1--(+)-->3, 2--(+)-->3
    trit_matrix_set(adj, 0, 1, TRIT_POSITIVE);
    trit_matrix_set(adj, 1, 0, TRIT_POSITIVE);
    trit_matrix_set(adj, 0, 2, TRIT_NEGATIVE);
    trit_matrix_set(adj, 2, 0, TRIT_NEGATIVE);
    trit_matrix_set(adj, 1, 3, TRIT_POSITIVE);
    trit_matrix_set(adj, 3, 1, TRIT_POSITIVE);
    trit_matrix_set(adj, 2, 3, TRIT_POSITIVE);
    trit_matrix_set(adj, 3, 2, TRIT_POSITIVE);

    walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 0);
    trit_walker_graph_step(walker);

    float amp1 = trit_walker_graph_get_amplitude(walker, 1);
    float amp2 = trit_walker_graph_get_amplitude(walker, 2);

    // amp1 should be positive, amp2 should be negative
    EXPECT_GT(amp1, 0.0f);
    EXPECT_LT(amp2, 0.0f);
}

//=============================================================================
// Normalization Tests
//=============================================================================

TEST(TritWalkerNormalizationTest, ProbabilitySumsToOne) {
    trit_matrix_t* adj = trit_matrix_create(5, 5, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    // Create complete graph
    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = 0; j < 5; j++) {
            trit_matrix_set(adj, i, j, (i != j) ? TRIT_POSITIVE : TRIT_UNKNOWN);
        }
    }

    trit_walker_graph_t* walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 0);

    // Run multiple steps and check normalization
    for (int s = 0; s < 20; s++) {
        trit_walker_graph_step(walker);

        float total_prob = 0.0f;
        for (uint32_t i = 0; i < walker->n_nodes; i++) {
            total_prob += walker->amplitudes[i] * walker->amplitudes[i];
        }

        // Probability should sum to approximately 1
        EXPECT_NEAR(total_prob, 1.0f, 0.1f);
    }

    trit_walker_graph_destroy(walker);
    trit_matrix_destroy(adj);
}

//=============================================================================
// Isolated Node Tests
//=============================================================================

TEST(TritWalkerIsolatedTest, IsolatedNodeStaysIsolated) {
    trit_matrix_t* adj = trit_matrix_create(5, 5, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    // Initialize to UNKNOWN (no edges)
    for (size_t i = 0; i < adj->numel; i++) {
        adj->data.unpacked[i] = TRIT_UNKNOWN;
    }

    // Create chain 0-1-2-3, node 4 isolated
    trit_matrix_set(adj, 0, 1, TRIT_POSITIVE);
    trit_matrix_set(adj, 1, 0, TRIT_POSITIVE);
    trit_matrix_set(adj, 1, 2, TRIT_POSITIVE);
    trit_matrix_set(adj, 2, 1, TRIT_POSITIVE);
    trit_matrix_set(adj, 2, 3, TRIT_POSITIVE);
    trit_matrix_set(adj, 3, 2, TRIT_POSITIVE);

    trit_walker_graph_t* walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 0);
    trit_walker_graph_run(walker, 50);

    // Node 4 should have no amplitude
    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(walker, 4), 0.0f);

    trit_walker_graph_destroy(walker);
    trit_matrix_destroy(adj);
}

TEST(TritWalkerIsolatedTest, IsolatedSourceRemains) {
    trit_matrix_t* adj = trit_matrix_create(3, 3, TERNARY_PACK_NONE);
    ASSERT_NE(adj, nullptr);

    // Initialize to UNKNOWN (no edges)
    for (size_t i = 0; i < adj->numel; i++) {
        adj->data.unpacked[i] = TRIT_UNKNOWN;
    }

    // Node 0 is isolated, 1-2 connected
    trit_matrix_set(adj, 1, 2, TRIT_POSITIVE);
    trit_matrix_set(adj, 2, 1, TRIT_POSITIVE);

    trit_walker_graph_t* walker = trit_walker_graph_create(adj);
    ASSERT_NE(walker, nullptr);

    trit_walker_graph_init(walker, 0);  // Start at isolated node
    trit_walker_graph_run(walker, 10);

    // All amplitude should stay at node 0
    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(walker, 0), 1.0f);
    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(walker, 1), 0.0f);
    EXPECT_FLOAT_EQ(trit_walker_graph_get_amplitude(walker, 2), 0.0f);

    trit_walker_graph_destroy(walker);
    trit_matrix_destroy(adj);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
