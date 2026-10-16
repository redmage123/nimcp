/**
 * @file test_mesh_topology.cpp
 * @brief Unit Tests for Mesh Network Topology Management
 *
 * Tests: Configuration, node registration, connection management,
 *        topology analysis, hub detection, coordinator placement, clustering.
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 */

#include <gtest/gtest.h>

extern "C" {
#include "mesh/nimcp_mesh_topology.h"
#include "mesh/nimcp_mesh_types.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshTopologyTest : public ::testing::Test {
protected:
    mesh_topology_ctx_t ctx;

    void SetUp() override {
        ctx = mesh_topology_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        mesh_topology_destroy(ctx);
        ctx = nullptr;
    }

    /* Helper: Create participant ID */
    mesh_participant_id_t make_id(uint16_t channel, uint16_t type, uint32_t local) {
        return mesh_make_participant_id(channel, (mesh_participant_type_t)type, local);
    }

    /* Helper: Add participants */
    void add_participants(size_t count, uint16_t channel = 1) {
        for (size_t i = 0; i < count; i++) {
            mesh_participant_id_t id = make_id(channel, MESH_PARTICIPANT_MODULE, (uint32_t)i);
            ASSERT_EQ(mesh_topology_add_participant(ctx, id), NIMCP_SUCCESS);
        }
    }

    /* Helper: Create ring topology */
    void create_ring(size_t count, uint16_t channel = 1) {
        add_participants(count, channel);
        for (size_t i = 0; i < count; i++) {
            mesh_participant_id_t from = make_id(channel, MESH_PARTICIPANT_MODULE, (uint32_t)i);
            mesh_participant_id_t to = make_id(channel, MESH_PARTICIPANT_MODULE, (uint32_t)((i + 1) % count));
            ASSERT_EQ(mesh_topology_add_connection(ctx, from, to, 1.0f), NIMCP_SUCCESS);
        }
    }

    /* Helper: Create star topology (hub at center) */
    void create_star(size_t count, uint16_t channel = 1) {
        add_participants(count, channel);
        /* Node 0 is hub, connects to all others */
        mesh_participant_id_t hub = make_id(channel, MESH_PARTICIPANT_MODULE, 0);
        for (size_t i = 1; i < count; i++) {
            mesh_participant_id_t spoke = make_id(channel, MESH_PARTICIPANT_MODULE, (uint32_t)i);
            ASSERT_EQ(mesh_topology_add_connection(ctx, hub, spoke, 1.0f), NIMCP_SUCCESS);
            ASSERT_EQ(mesh_topology_add_connection(ctx, spoke, hub, 1.0f), NIMCP_SUCCESS);
        }
    }
};

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, DefaultConfigHasSensibleValues) {
    mesh_topology_config_t config = mesh_topology_default_config();

    EXPECT_FLOAT_EQ(config.hub_percentile, MESH_TOPOLOGY_HUB_PERCENTILE);
    EXPECT_FLOAT_EQ(config.high_centrality_threshold, MESH_TOPOLOGY_HIGH_CENTRALITY);
    EXPECT_TRUE(config.compute_clustering);
    EXPECT_TRUE(config.compute_path_length);
    EXPECT_TRUE(config.compute_small_world);
    EXPECT_TRUE(config.fit_power_law);
    EXPECT_GT(config.max_path_samples, 0u);
}

TEST_F(MeshTopologyTest, CreateWithCustomConfig) {
    mesh_topology_config_t config = mesh_topology_default_config();
    config.hub_percentile = 0.95f;
    config.compute_clustering = false;

    mesh_topology_ctx_t custom_ctx = mesh_topology_create(&config);
    ASSERT_NE(custom_ctx, nullptr);

    /* Should work with custom config */
    mesh_participant_id_t id = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    EXPECT_EQ(mesh_topology_add_participant(custom_ctx, id), NIMCP_SUCCESS);

    mesh_topology_destroy(custom_ctx);
}

/* ============================================================================
 * Node Registration Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, AddSingleParticipant) {
    mesh_participant_id_t id = make_id(1, MESH_PARTICIPANT_MODULE, 42);
    EXPECT_EQ(mesh_topology_add_participant(ctx, id), NIMCP_SUCCESS);

    uint32_t num_participants;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, nullptr, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 1u);
}

TEST_F(MeshTopologyTest, AddMultipleParticipants) {
    add_participants(100);

    uint32_t num_participants;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, nullptr, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 100u);
}

TEST_F(MeshTopologyTest, AddDuplicateParticipantIsIdempotent) {
    mesh_participant_id_t id = make_id(1, MESH_PARTICIPANT_MODULE, 42);

    EXPECT_EQ(mesh_topology_add_participant(ctx, id), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_topology_add_participant(ctx, id), NIMCP_SUCCESS);  /* Duplicate */

    uint32_t num_participants;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, nullptr, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 1u);  /* Should still be 1 */
}

TEST_F(MeshTopologyTest, RemoveParticipant) {
    add_participants(5);

    mesh_participant_id_t id = make_id(1, MESH_PARTICIPANT_MODULE, 2);
    EXPECT_EQ(mesh_topology_remove_participant(ctx, id), NIMCP_SUCCESS);

    uint32_t num_participants;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, nullptr, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 4u);
}

TEST_F(MeshTopologyTest, RemoveNonexistentParticipantFails) {
    add_participants(5);

    mesh_participant_id_t nonexistent = make_id(1, MESH_PARTICIPANT_MODULE, 999);
    EXPECT_EQ(mesh_topology_remove_participant(ctx, nonexistent), NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshTopologyTest, ClearRemovesAll) {
    add_participants(50);
    ASSERT_EQ(mesh_topology_clear(ctx), NIMCP_SUCCESS);

    uint32_t num_participants, num_connections;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, &num_connections, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 0u);
    EXPECT_EQ(num_connections, 0u);
}

/* ============================================================================
 * Connection Management Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, AddConnection) {
    add_participants(2);

    mesh_participant_id_t from = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    mesh_participant_id_t to = make_id(1, MESH_PARTICIPANT_MODULE, 1);

    EXPECT_EQ(mesh_topology_add_connection(ctx, from, to, 1.0f), NIMCP_SUCCESS);

    uint32_t num_connections;
    ASSERT_EQ(mesh_topology_get_stats(ctx, nullptr, &num_connections, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_connections, 1u);
}

TEST_F(MeshTopologyTest, AddConnectionToNonexistentFails) {
    add_participants(1);

    mesh_participant_id_t from = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    mesh_participant_id_t to = make_id(1, MESH_PARTICIPANT_MODULE, 999);  /* Doesn't exist */

    EXPECT_EQ(mesh_topology_add_connection(ctx, from, to, 1.0f), NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshTopologyTest, RingTopologyHasCorrectConnections) {
    create_ring(10);

    uint32_t num_participants, num_connections;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, &num_connections, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 10u);
    EXPECT_EQ(num_connections, 10u);
}

TEST_F(MeshTopologyTest, StarTopologyHasCorrectConnections) {
    create_star(10);

    uint32_t num_participants, num_connections;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, &num_connections, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(num_participants, 10u);
    EXPECT_EQ(num_connections, 18u);  /* 9 spokes * 2 directions */
}

/* ============================================================================
 * Topology Metrics Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, ComputeMetricsEmptyTopology) {
    mesh_topology_metrics_t metrics;
    EXPECT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_ERROR_INVALID_STATE);
}

TEST_F(MeshTopologyTest, ComputeMetricsRingTopology) {
    create_ring(10);

    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    EXPECT_EQ(metrics.num_participants, 10u);
    EXPECT_EQ(metrics.num_connections, 10u);
    EXPECT_FLOAT_EQ(metrics.avg_degree, 1.0f);  /* Each node has 1 outgoing edge */
}

TEST_F(MeshTopologyTest, ComputeMetricsStarTopology) {
    create_star(10);

    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    EXPECT_EQ(metrics.num_participants, 10u);
    EXPECT_GT(metrics.avg_degree, 1.0f);  /* Hub has high degree */
    EXPECT_GT(metrics.num_hubs, 0u);
}

TEST_F(MeshTopologyTest, GetNodeInfo) {
    create_star(5);

    mesh_participant_id_t hub = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    mesh_node_info_t info;

    ASSERT_EQ(mesh_topology_get_node_info(ctx, hub, &info), NIMCP_SUCCESS);
    EXPECT_EQ(info.participant_id, hub);
    EXPECT_GT(info.degree, 0u);  /* Hub should have connections */
}

TEST_F(MeshTopologyTest, GetNodeInfoNonexistent) {
    add_participants(5);

    mesh_participant_id_t nonexistent = make_id(1, MESH_PARTICIPANT_MODULE, 999);
    mesh_node_info_t info;

    EXPECT_EQ(mesh_topology_get_node_info(ctx, nonexistent, &info), NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Hub Detection Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, IdentifyHubsStarTopology) {
    create_star(10);

    /* Run metrics first to identify hubs */
    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    mesh_participant_id_t hubs[10];
    size_t num_hubs;

    ASSERT_EQ(mesh_topology_identify_hubs(ctx, hubs, 10, &num_hubs), NIMCP_SUCCESS);
    EXPECT_GE(num_hubs, 1u);

    /* Hub (node 0) should be identified */
    mesh_participant_id_t expected_hub = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    bool found = false;
    for (size_t i = 0; i < num_hubs; i++) {
        if (hubs[i] == expected_hub) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected hub node 0 to be identified";
}

TEST_F(MeshTopologyTest, IdentifyHubsRingTopology) {
    create_ring(10);

    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    /* Ring topology has uniform degree, so few/no hubs */
    mesh_participant_id_t hubs[10];
    size_t num_hubs;

    ASSERT_EQ(mesh_topology_identify_hubs(ctx, hubs, 10, &num_hubs), NIMCP_SUCCESS);
    /* At 90th percentile, top 10% should be hubs */
    EXPECT_LE(num_hubs, 2u);
}

/* ============================================================================
 * Betweenness Centrality Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, ComputeBetweennessStarTopology) {
    create_star(10);

    ASSERT_EQ(mesh_topology_compute_betweenness(ctx), NIMCP_SUCCESS);

    /* Hub should have highest betweenness */
    mesh_participant_id_t hub = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    mesh_node_info_t hub_info;
    ASSERT_EQ(mesh_topology_get_node_info(ctx, hub, &hub_info), NIMCP_SUCCESS);

    /* Check a spoke has lower betweenness */
    mesh_participant_id_t spoke = make_id(1, MESH_PARTICIPANT_MODULE, 1);
    mesh_node_info_t spoke_info;
    ASSERT_EQ(mesh_topology_get_node_info(ctx, spoke, &spoke_info), NIMCP_SUCCESS);

    EXPECT_GE(hub_info.betweenness_centrality, spoke_info.betweenness_centrality);
}

/* ============================================================================
 * Scale-Free and Small-World Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, IsScaleFreeRequiresEnoughNodes) {
    create_ring(5);  /* Too few nodes */

    float gamma, r2;
    EXPECT_FALSE(mesh_topology_is_scale_free(ctx, &gamma, &r2));
}

TEST_F(MeshTopologyTest, IsSmallWorldRequiresEnoughNodes) {
    create_ring(5);  /* Too few nodes */

    float sigma;
    EXPECT_FALSE(mesh_topology_is_small_world(ctx, &sigma));
}

/* ============================================================================
 * Coordinator Placement Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, OptimalPoolSizeSmall) {
    uint32_t size = mesh_topology_optimal_pool_size(4);
    EXPECT_EQ(size, MESH_TOPOLOGY_MIN_COORDINATORS);
}

TEST_F(MeshTopologyTest, OptimalPoolSizeMedium) {
    uint32_t size = mesh_topology_optimal_pool_size(64);  /* 2 * log2(64) = 12, capped at 8 */
    EXPECT_GE(size, MESH_TOPOLOGY_MIN_COORDINATORS);
    EXPECT_LE(size, MESH_TOPOLOGY_MAX_COORDINATORS);
}

TEST_F(MeshTopologyTest, OptimalPoolSizeLarge) {
    uint32_t size = mesh_topology_optimal_pool_size(1000);
    EXPECT_EQ(size, MESH_TOPOLOGY_MAX_COORDINATORS);
}

TEST_F(MeshTopologyTest, AssignCoordinatorHubsToLeader) {
    create_star(20);

    /* Run metrics to identify hub */
    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    /* Hub should be assigned to coordinator 0 (leader) */
    mesh_participant_id_t hub = make_id(1, MESH_PARTICIPANT_MODULE, 0);
    uint32_t assignment = mesh_topology_assign_coordinator(ctx, hub, 4);
    EXPECT_EQ(assignment, 0u);
}

TEST_F(MeshTopologyTest, AssignCoordinatorDistributes) {
    add_participants(100);

    uint32_t pool_size = 4;
    uint32_t counts[4] = {0, 0, 0, 0};

    for (uint32_t i = 0; i < 100; i++) {
        mesh_participant_id_t id = make_id(1, MESH_PARTICIPANT_MODULE, i);
        uint32_t assignment = mesh_topology_assign_coordinator(ctx, id, pool_size);
        ASSERT_LT(assignment, pool_size);
        counts[assignment]++;
    }

    /* Check roughly even distribution (each should have ~25) */
    for (int i = 0; i < 4; i++) {
        EXPECT_GT(counts[i], 10u) << "Coordinator " << i << " has too few assignments";
    }
}

TEST_F(MeshTopologyTest, RecommendPlacement) {
    create_star(50);

    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    mesh_coord_placement_t placement;
    ASSERT_EQ(mesh_topology_recommend_placement(ctx, &placement), NIMCP_SUCCESS);

    EXPECT_GE(placement.recommended_pool_size, MESH_TOPOLOGY_MIN_COORDINATORS);
    EXPECT_LE(placement.recommended_pool_size, MESH_TOPOLOGY_MAX_COORDINATORS);
    EXPECT_GT(placement.distribution_size, 0u);

    /* Load distribution should sum to ~1.0 */
    float total_load = 0.0f;
    for (size_t i = 0; i < placement.distribution_size; i++) {
        total_load += placement.load_distribution[i];
    }
    EXPECT_NEAR(total_load, 1.0f, 0.01f);

    mesh_coord_placement_free(&placement);
}

TEST_F(MeshTopologyTest, RebalanceAssignments) {
    add_participants(20);

    uint32_t assignments[20];
    size_t num_assignments;

    ASSERT_EQ(mesh_topology_rebalance_assignments(ctx, 4, assignments, &num_assignments), NIMCP_SUCCESS);
    EXPECT_EQ(num_assignments, 20u);

    /* All assignments should be valid */
    for (size_t i = 0; i < num_assignments; i++) {
        EXPECT_LT(assignments[i], 4u);
    }
}

/* ============================================================================
 * Cluster Detection Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, DetectClustersConnected) {
    create_ring(10);

    uint32_t num_clusters;
    ASSERT_EQ(mesh_topology_detect_clusters(ctx, 10, &num_clusters), NIMCP_SUCCESS);

    /* Ring is fully connected, should be 1 cluster */
    EXPECT_EQ(num_clusters, 1u);
}

TEST_F(MeshTopologyTest, DetectClustersDisconnected) {
    /* Create two separate groups */
    add_participants(10, 1);

    /* Group 1: nodes 0-4 */
    for (uint32_t i = 0; i < 4; i++) {
        mesh_participant_id_t from = make_id(1, MESH_PARTICIPANT_MODULE, i);
        mesh_participant_id_t to = make_id(1, MESH_PARTICIPANT_MODULE, i + 1);
        mesh_topology_add_connection(ctx, from, to, 1.0f);
    }

    /* Group 2: nodes 5-9 */
    for (uint32_t i = 5; i < 9; i++) {
        mesh_participant_id_t from = make_id(1, MESH_PARTICIPANT_MODULE, i);
        mesh_participant_id_t to = make_id(1, MESH_PARTICIPANT_MODULE, i + 1);
        mesh_topology_add_connection(ctx, from, to, 1.0f);
    }

    uint32_t num_clusters;
    ASSERT_EQ(mesh_topology_detect_clusters(ctx, 10, &num_clusters), NIMCP_SUCCESS);

    /* Should have 2 clusters */
    EXPECT_EQ(num_clusters, 2u);
}

TEST_F(MeshTopologyTest, GetClusterMembers) {
    create_ring(10);

    uint32_t num_clusters;
    ASSERT_EQ(mesh_topology_detect_clusters(ctx, 10, &num_clusters), NIMCP_SUCCESS);

    mesh_participant_id_t members[10];
    size_t count;

    ASSERT_EQ(mesh_topology_get_cluster_members(ctx, 0, members, 10, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 10u);  /* All in cluster 0 */
}

/* ============================================================================
 * Statistics and Debug Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, GetStats) {
    create_star(20);

    mesh_topology_metrics_t metrics;
    ASSERT_EQ(mesh_topology_compute_metrics(ctx, &metrics), NIMCP_SUCCESS);

    uint32_t num_participants, num_connections, num_hubs, num_clusters;
    ASSERT_EQ(mesh_topology_get_stats(ctx, &num_participants, &num_connections, &num_hubs, &num_clusters),
              NIMCP_SUCCESS);

    EXPECT_EQ(num_participants, 20u);
    EXPECT_GT(num_connections, 0u);
}

TEST_F(MeshTopologyTest, PrintDebugDoesNotCrash) {
    create_ring(5);
    mesh_topology_print_debug(ctx);  /* Should not crash */
    mesh_topology_print_debug(nullptr);  /* Should handle NULL */
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

TEST_F(MeshTopologyTest, NullContextHandling) {
    EXPECT_EQ(mesh_topology_add_participant(nullptr, 0), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_topology_add_connection(nullptr, 0, 0, 1.0f), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_topology_remove_participant(nullptr, 0), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_topology_clear(nullptr), NIMCP_ERROR_INVALID_PARAMETER);

    mesh_topology_metrics_t metrics;
    EXPECT_EQ(mesh_topology_compute_metrics(nullptr, &metrics), NIMCP_ERROR_INVALID_PARAMETER);

    mesh_node_info_t info;
    EXPECT_EQ(mesh_topology_get_node_info(nullptr, 0, &info), NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(MeshTopologyTest, NullOutputHandling) {
    add_participants(5);

    EXPECT_EQ(mesh_topology_compute_metrics(ctx, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_topology_get_node_info(ctx, make_id(1, MESH_PARTICIPANT_MODULE, 0), nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);

    size_t num_hubs;
    EXPECT_EQ(mesh_topology_identify_hubs(ctx, nullptr, 10, &num_hubs), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Placement Free Test
 * ============================================================================ */

TEST_F(MeshTopologyTest, PlacementFreeHandlesNull) {
    mesh_coord_placement_free(nullptr);  /* Should not crash */

    mesh_coord_placement_t placement = {0};
    mesh_coord_placement_free(&placement);  /* Should handle empty */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
