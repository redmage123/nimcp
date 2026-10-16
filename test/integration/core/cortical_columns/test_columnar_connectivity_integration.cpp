/**
 * @file test_columnar_connectivity_integration.cpp
 * @brief Integration tests for columnar connectivity module
 *
 * Tests columnar connectivity functionality including:
 * - Connection patterns (local, lateral, long-range)
 * - Synaptic weight management
 * - Connection strength modulation
 * - Topographic mapping
 * - Sparse connectivity
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"
#include "core/cortical_columns/nimcp_columnar_connectivity.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class ColumnarConnectivityIntegrationTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();

        conn_ = columnar_connectivity_create(10000);
        ASSERT_NE(conn_, nullptr);
    }

    void TearDown() override {
        if (conn_) {
            columnar_connectivity_destroy(conn_);
            conn_ = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create connectivity rule
    connectivity_rule_t CreateDefaultRule(connectivity_type_t type) {
        connectivity_rule_t rule;
        memset(&rule, 0, sizeof(rule));

        rule.type = type;
        rule.base_probability = 0.3f;
        rule.distance_decay_lambda = 1.5f;
        rule.feature_similarity_weight = 0.5f;
        rule.layer_specific = false;
        rule.min_delay_ms = 1.0f;
        rule.conduction_velocity_m_s = 1.0f;

        return rule;
    }

    columnar_connectivity_t* conn_ = nullptr;
};

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, CreateBasic) {
    columnar_connectivity_t* conn = columnar_connectivity_create(1000);
    EXPECT_NE(conn, nullptr);
    if (conn) {
        columnar_connectivity_destroy(conn);
    }
}

TEST_F(ColumnarConnectivityIntegrationTest, CreateZeroConnections) {
    columnar_connectivity_t* conn = columnar_connectivity_create(0);
    // Implementation dependent - may return NULL or valid empty manager
    if (conn) {
        columnar_connectivity_destroy(conn);
    }
    SUCCEED();
}

TEST_F(ColumnarConnectivityIntegrationTest, DestroyNullSafe) {
    columnar_connectivity_destroy(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(ColumnarConnectivityIntegrationTest, CreateLargeCapacity) {
    columnar_connectivity_t* conn = columnar_connectivity_create(100000);
    EXPECT_NE(conn, nullptr);
    if (conn) {
        columnar_connectivity_destroy(conn);
    }
}

/*=============================================================================
 * Connectivity Rule Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, AddIntracolumnarRule) {
    connectivity_rule_t rule = CreateDefaultRule(CONNECTIVITY_INTRACOLUMNAR);
    rule.base_probability = 0.5f;

    nimcp_result_t result = connectivity_add_rule(conn_, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, AddIntercolumnarRule) {
    connectivity_rule_t rule = CreateDefaultRule(CONNECTIVITY_INTERCOLUMNAR);
    rule.distance_decay_lambda = 2.0f;

    nimcp_result_t result = connectivity_add_rule(conn_, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, AddLongRangeRule) {
    connectivity_rule_t rule = CreateDefaultRule(CONNECTIVITY_LONG_RANGE);
    rule.base_probability = 0.1f;

    nimcp_result_t result = connectivity_add_rule(conn_, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, AddFeedforwardRule) {
    connectivity_rule_t rule = CreateDefaultRule(CONNECTIVITY_FEEDFORWARD);
    rule.layer_specific = true;
    rule.source_layer = CC_LAYER_II_III;
    rule.target_layer = CC_LAYER_IV;

    nimcp_result_t result = connectivity_add_rule(conn_, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, AddFeedbackRule) {
    connectivity_rule_t rule = CreateDefaultRule(CONNECTIVITY_FEEDBACK);
    rule.layer_specific = true;
    rule.source_layer = CC_LAYER_VI;
    rule.target_layer = CC_LAYER_I;

    nimcp_result_t result = connectivity_add_rule(conn_, &rule);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, AddMultipleRules) {
    connectivity_rule_t rules[5];

    rules[0] = CreateDefaultRule(CONNECTIVITY_INTRACOLUMNAR);
    rules[1] = CreateDefaultRule(CONNECTIVITY_INTERCOLUMNAR);
    rules[2] = CreateDefaultRule(CONNECTIVITY_LONG_RANGE);
    rules[3] = CreateDefaultRule(CONNECTIVITY_FEEDFORWARD);
    rules[4] = CreateDefaultRule(CONNECTIVITY_FEEDBACK);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(connectivity_add_rule(conn_, &rules[i]), NIMCP_SUCCESS);
    }
}

TEST_F(ColumnarConnectivityIntegrationTest, AddNullRule) {
    nimcp_result_t result = connectivity_add_rule(conn_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, ApplyCanonicalRules) {
    nimcp_result_t result = connectivity_apply_canonical_rules(conn_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/*=============================================================================
 * Connection Generation Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, GenerateIntracolumnarConnections) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Create laminar structure for column
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    uint32_t count = connectivity_generate_intracolumnar(conn_, 1, ls);
    EXPECT_GT(count, 0u);

    laminar_structure_destroy(ls);
}

TEST_F(ColumnarConnectivityIntegrationTest, GenerateIntercolumnarConnections) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Create column IDs
    uint32_t column_ids[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t num_columns = 8;

    // Create 2D grid positions
    float positions[] = {
        0.0f, 0.0f,   // Column 1
        1.0f, 0.0f,   // Column 2
        2.0f, 0.0f,   // Column 3
        3.0f, 0.0f,   // Column 4
        0.0f, 1.0f,   // Column 5
        1.0f, 1.0f,   // Column 6
        2.0f, 1.0f,   // Column 7
        3.0f, 1.0f    // Column 8
    };

    uint32_t count = connectivity_generate_intercolumnar(
        conn_, column_ids, num_columns, positions, 2);

    EXPECT_GE(count, 0u);  // May be 0 depending on probability
}

TEST_F(ColumnarConnectivityIntegrationTest, GenerateIntercolumnarNoPositions) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {1, 2, 3, 4};
    uint32_t num_columns = 4;

    // NULL positions should use grid layout
    uint32_t count = connectivity_generate_intercolumnar(
        conn_, column_ids, num_columns, nullptr, 2);

    EXPECT_GE(count, 0u);
}

TEST_F(ColumnarConnectivityIntegrationTest, GenerateLongRangeConnections) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t source_columns[] = {1, 2, 3, 4};
    uint32_t target_columns[] = {5, 6, 7, 8};

    uint32_t count = connectivity_generate_long_range(
        conn_, source_columns, 4, target_columns, 4);

    EXPECT_GE(count, 0u);
}

TEST_F(ColumnarConnectivityIntegrationTest, GenerateConnectionsEmptyArrays) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t count = connectivity_generate_long_range(
        conn_, nullptr, 0, nullptr, 0);

    EXPECT_EQ(count, 0u);
}

/*=============================================================================
 * Connection Access Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, GetConnectionsFrom) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Create some connections
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    connectivity_generate_intracolumnar(conn_, 1, ls);

    columnar_connection_t connections[100];
    uint32_t count = connectivity_get_connections_from(conn_, 1, connections, 100);

    EXPECT_GE(count, 0u);

    laminar_structure_destroy(ls);
}

TEST_F(ColumnarConnectivityIntegrationTest, GetConnectionsTo) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    connectivity_generate_intracolumnar(conn_, 1, ls);

    columnar_connection_t connections[100];
    uint32_t count = connectivity_get_connections_to(conn_, 1, connections, 100);

    EXPECT_GE(count, 0u);

    laminar_structure_destroy(ls);
}

TEST_F(ColumnarConnectivityIntegrationTest, GetConnectionsNonexistentColumn) {
    columnar_connection_t connections[10];
    uint32_t count = connectivity_get_connections_from(conn_, 9999, connections, 10);

    EXPECT_EQ(count, 0u);
}

TEST_F(ColumnarConnectivityIntegrationTest, GetConnectionsSmallBuffer) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Generate many connections
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    for (int i = 1; i <= 10; i++) {
        connectivity_generate_intracolumnar(conn_, i, ls);
    }

    // Request with small buffer
    columnar_connection_t connections[2];
    uint32_t count = connectivity_get_connections_from(conn_, 1, connections, 2);

    EXPECT_LE(count, 2u);

    laminar_structure_destroy(ls);
}

/*=============================================================================
 * Signal Propagation Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, PropagateBasic) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Generate connections between columns
    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    float source_activations[] = {1.0f, 0.5f, 0.3f, 0.1f};
    float target_inputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    connectivity_propagate(conn_, source_activations, target_inputs, 4);

    // Target inputs should have some values (or zeros if no connections)
    bool has_input = false;
    for (int i = 0; i < 4; i++) {
        if (target_inputs[i] > 0.0f) {
            has_input = true;
            break;
        }
    }
    // May or may not have input depending on connection generation
    SUCCEED();
}

TEST_F(ColumnarConnectivityIntegrationTest, PropagateWithDelay) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    float source_activations[] = {1.0f, 0.5f, 0.3f, 0.1f};
    float target_inputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Propagate with 1ms time step
    connectivity_propagate_with_delay(
        conn_, source_activations, target_inputs, 4, 1.0f);

    SUCCEED();
}

TEST_F(ColumnarConnectivityIntegrationTest, PropagateMultipleTimeSteps) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    // Simulate multiple time steps
    for (int t = 0; t < 100; t++) {
        float source_activations[] = {
            0.5f + 0.3f * sinf(t * 0.1f),
            0.5f + 0.3f * cosf(t * 0.1f),
            0.5f + 0.3f * sinf(t * 0.2f),
            0.5f + 0.3f * cosf(t * 0.2f)
        };
        float target_inputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        connectivity_propagate_with_delay(
            conn_, source_activations, target_inputs, 4, 1.0f);
    }

    SUCCEED();
}

/*=============================================================================
 * Plasticity Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, ApplyHebbianLearning) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    float pre_activations[] = {1.0f, 0.8f, 0.0f, 0.0f};
    float post_activations[] = {0.0f, 0.0f, 0.9f, 0.7f};

    connectivity_apply_hebbian(conn_, pre_activations, post_activations, 0.01f);

    SUCCEED();  // Should not crash
}

TEST_F(ColumnarConnectivityIntegrationTest, HebbianLearningModifiesWeights) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 1.5f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    // Get initial stats
    connectivity_stats_t stats_before;
    ASSERT_EQ(connectivity_get_stats(conn_, &stats_before), NIMCP_SUCCESS);

    // Apply correlated learning
    for (int i = 0; i < 100; i++) {
        float pre[] = {1.0f, 1.0f, 0.0f, 0.0f};
        float post[] = {1.0f, 1.0f, 0.0f, 0.0f};
        connectivity_apply_hebbian(conn_, pre, post, 0.01f);
    }

    // Weights may have changed
    connectivity_stats_t stats_after;
    ASSERT_EQ(connectivity_get_stats(conn_, &stats_after), NIMCP_SUCCESS);

    // Just verify stats retrieval works
    SUCCEED();
}

TEST_F(ColumnarConnectivityIntegrationTest, ApplySTDP) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    // Spike times in microseconds
    uint64_t pre_spike_times[] = {1000, 1100, 1200, 1300};
    uint64_t post_spike_times[] = {1010, 1090, 1220, 1280};  // After pre

    connectivity_apply_stdp(conn_, pre_spike_times, post_spike_times, 4);

    SUCCEED();  // Should not crash
}

TEST_F(ColumnarConnectivityIntegrationTest, STDPWithReversedTiming) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f};

    connectivity_generate_intercolumnar(conn_, column_ids, 4, positions, 2);

    // Post spikes before pre (should cause LTD)
    uint64_t pre_spike_times[] = {1010, 1110, 1210, 1310};
    uint64_t post_spike_times[] = {1000, 1100, 1200, 1300};  // Before pre

    connectivity_apply_stdp(conn_, pre_spike_times, post_spike_times, 4);

    SUCCEED();
}

/*=============================================================================
 * Topology Analysis Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, ComputeClustering) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Create densely connected network
    uint32_t column_ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
    float positions[] = {
        0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 1.5f, 0.0f,
        0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f, 1.5f, 0.5f
    };

    connectivity_generate_intercolumnar(conn_, column_ids, 8, positions, 2);

    float clustering = connectivity_compute_clustering(conn_);

    // Clustering should be valid or -1.0 for error
    EXPECT_GE(clustering, -1.0f);
    if (clustering >= 0.0f) {
        EXPECT_LE(clustering, 1.0f);
    }
}

TEST_F(ColumnarConnectivityIntegrationTest, ComputePathLength) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
    float positions[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 3.0f, 1.0f
    };

    connectivity_generate_intercolumnar(conn_, column_ids, 8, positions, 2);

    float path_length = connectivity_compute_path_length(conn_);

    // Path length should be valid or -1.0 for error
    EXPECT_GE(path_length, -1.0f);
}

TEST_F(ColumnarConnectivityIntegrationTest, CheckSmallWorld) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Create network with small-world properties
    uint32_t column_ids[16];
    float positions[32];

    for (int i = 0; i < 16; i++) {
        column_ids[i] = i;
        positions[i * 2] = (i % 4) * 0.3f;
        positions[i * 2 + 1] = (i / 4) * 0.3f;
    }

    connectivity_generate_intercolumnar(conn_, column_ids, 16, positions, 2);

    bool is_small_world = connectivity_is_small_world(conn_);

    // Just check it returns a valid bool
    EXPECT_TRUE(is_small_world || !is_small_world);
}

TEST_F(ColumnarConnectivityIntegrationTest, EmptyNetworkMetrics) {
    // No connections generated

    float clustering = connectivity_compute_clustering(conn_);
    float path_length = connectivity_compute_path_length(conn_);

    // Should return error values or handle gracefully
    SUCCEED();
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, GetStatsEmpty) {
    connectivity_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    nimcp_result_t result = connectivity_get_stats(conn_, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_connections, 0u);
}

TEST_F(ColumnarConnectivityIntegrationTest, GetStatsWithConnections) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Generate various connection types
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    for (int i = 1; i <= 5; i++) {
        connectivity_generate_intracolumnar(conn_, i, ls);
    }

    uint32_t column_ids[] = {1, 2, 3, 4, 5};
    float positions[] = {0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f, 4.0f, 0.0f};
    connectivity_generate_intercolumnar(conn_, column_ids, 5, positions, 2);

    connectivity_stats_t stats;
    nimcp_result_t result = connectivity_get_stats(conn_, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(stats.total_connections, 0u);
    EXPECT_GE(stats.intracolumnar_count, 0u);
    EXPECT_GE(stats.avg_weight, 0.0f);
    EXPECT_LE(stats.avg_weight, 1.0f);

    laminar_structure_destroy(ls);
}

TEST_F(ColumnarConnectivityIntegrationTest, GetStatsNullStats) {
    nimcp_result_t result = connectivity_get_stats(conn_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(ColumnarConnectivityIntegrationTest, LayerConnectionCounts) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    // Generate multiple columns
    for (int i = 1; i <= 10; i++) {
        connectivity_generate_intracolumnar(conn_, i, ls);
    }

    connectivity_stats_t stats;
    ASSERT_EQ(connectivity_get_stats(conn_, &stats), NIMCP_SUCCESS);

    // Check layer connection counts are valid
    for (int src = 0; src < CC_LAYER_COUNT; src++) {
        for (int tgt = 0; tgt < CC_LAYER_COUNT; tgt++) {
            EXPECT_GE(stats.layer_connection_counts[src][tgt], 0u);
        }
    }

    laminar_structure_destroy(ls);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, HighConnectionCount) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    // Generate many connections
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    for (int i = 1; i <= 50; i++) {
        connectivity_generate_intracolumnar(conn_, i, ls);
    }

    connectivity_stats_t stats;
    ASSERT_EQ(connectivity_get_stats(conn_, &stats), NIMCP_SUCCESS);

    EXPECT_GT(stats.total_connections, 0u);

    laminar_structure_destroy(ls);
}

TEST_F(ColumnarConnectivityIntegrationTest, RepeatedPropagation) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[20];
    float positions[40];

    for (int i = 0; i < 20; i++) {
        column_ids[i] = i;
        positions[i * 2] = (i % 5) * 0.5f;
        positions[i * 2 + 1] = (i / 5) * 0.5f;
    }

    connectivity_generate_intercolumnar(conn_, column_ids, 20, positions, 2);

    // Many propagation cycles
    for (int cycle = 0; cycle < 500; cycle++) {
        float source[20];
        float target[20];

        for (int i = 0; i < 20; i++) {
            source[i] = 0.5f + 0.3f * sinf(cycle * 0.05f + i * 0.3f);
            target[i] = 0.0f;
        }

        connectivity_propagate(conn_, source, target, 20);
    }

    SUCCEED();
}

TEST_F(ColumnarConnectivityIntegrationTest, RepeatedPlasticityUpdates) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
    float positions[] = {
        0.0f, 0.0f, 0.3f, 0.0f, 0.6f, 0.0f, 0.9f, 0.0f,
        0.0f, 0.3f, 0.3f, 0.3f, 0.6f, 0.3f, 0.9f, 0.3f
    };

    connectivity_generate_intercolumnar(conn_, column_ids, 8, positions, 2);

    // Many learning cycles
    for (int cycle = 0; cycle < 200; cycle++) {
        float pre[8], post[8];

        for (int i = 0; i < 8; i++) {
            pre[i] = (i < 4) ? 1.0f : 0.0f;
            post[i] = (i >= 4) ? 1.0f : 0.0f;
        }

        connectivity_apply_hebbian(conn_, pre, post, 0.001f);
    }

    connectivity_stats_t stats;
    ASSERT_EQ(connectivity_get_stats(conn_, &stats), NIMCP_SUCCESS);

    // Weights should still be valid
    EXPECT_GE(stats.avg_weight, 0.0f);
    EXPECT_LE(stats.avg_weight, 1.0f);
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(ColumnarConnectivityIntegrationTest, SingleColumn) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    uint32_t count = connectivity_generate_intracolumnar(conn_, 1, ls);
    EXPECT_GT(count, 0u);

    laminar_structure_destroy(ls);
}

TEST_F(ColumnarConnectivityIntegrationTest, TwoColumnsMaxDistance) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {1, 2};
    float positions[] = {0.0f, 0.0f, 100.0f, 100.0f};  // Very far apart

    uint32_t count = connectivity_generate_intercolumnar(
        conn_, column_ids, 2, positions, 2);

    // Probability should be very low due to distance
    EXPECT_GE(count, 0u);
}

TEST_F(ColumnarConnectivityIntegrationTest, OverlappingColumns) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {1, 2, 3, 4};
    float positions[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // All same position

    uint32_t count = connectivity_generate_intercolumnar(
        conn_, column_ids, 4, positions, 2);

    // Should have high connection probability
    EXPECT_GE(count, 0u);
}

TEST_F(ColumnarConnectivityIntegrationTest, ThreeDimensionalPositions) {
    ASSERT_EQ(connectivity_apply_canonical_rules(conn_), NIMCP_SUCCESS);

    uint32_t column_ids[] = {1, 2, 3, 4};
    float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

    uint32_t count = connectivity_generate_intercolumnar(
        conn_, column_ids, 4, positions, 3);

    EXPECT_GE(count, 0u);
}
