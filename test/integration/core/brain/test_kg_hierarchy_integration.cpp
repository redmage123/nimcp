/**
 * @file test_kg_hierarchy_integration.cpp
 * @brief Integration tests for KG Hierarchy edge metadata functionality
 *
 * Tests edge metadata with real KG operations:
 * - Add nodes, add edges, then set/get metadata on those edges
 * - Metadata interaction with hierarchy rebuild/sync
 * - Metadata persistence across state changes and traversal
 * - Multi-edge metadata with realistic brain topologies
 * - Metadata on edges across hemisphere boundaries
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <set>

#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"

// ============================================================================
// Test Fixture
// ============================================================================

class KGHierarchyEdgeMetadataIntegration : public ::testing::Test {
protected:
    brain_kg_t* kg;
    kg_hierarchy_t* hier;
    kg_hierarchy_config_t config;

    void SetUp() override {
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg_config.enable_integrity_checks = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        kg_hierarchy_default_config(&config);
        hier = nullptr;
    }

    void TearDown() override {
        if (hier) {
            kg_hierarchy_destroy(hier);
            hier = nullptr;
        }
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    brain_kg_node_id_t add_module(const char* name, brain_kg_node_type_t type) {
        return brain_kg_add_node(kg, name, type, "Test module");
    }

    /**
     * Create a small but realistic brain topology with edges
     * for testing metadata on real graph structures.
     *
     * Topology:
     *   core_init --> memory_mgr --> hippocampus
     *             \-> event_bus  --> visual_cortex --> prefrontal
     *                            \-> auditory      --> prefrontal
     *   plasticity (isolated)
     */
    struct TestTopology {
        brain_kg_node_id_t core_init;
        brain_kg_node_id_t memory_mgr;
        brain_kg_node_id_t event_bus;
        brain_kg_node_id_t visual_cortex;
        brain_kg_node_id_t auditory;
        brain_kg_node_id_t hippocampus;
        brain_kg_node_id_t prefrontal;
        brain_kg_node_id_t plasticity;

        brain_kg_edge_id_t edge_core_mem;
        brain_kg_edge_id_t edge_core_bus;
        brain_kg_edge_id_t edge_bus_visual;
        brain_kg_edge_id_t edge_bus_audio;
        brain_kg_edge_id_t edge_mem_hippo;
        brain_kg_edge_id_t edge_visual_pfc;
        brain_kg_edge_id_t edge_audio_pfc;
    };

    TestTopology create_test_topology() {
        TestTopology t;

        // Add nodes
        t.core_init     = add_module("core_init", BRAIN_KG_NODE_CORE);
        t.memory_mgr    = add_module("memory_mgr", BRAIN_KG_NODE_CORE);
        t.event_bus     = add_module("event_bus", BRAIN_KG_NODE_CORE);
        t.visual_cortex = add_module("visual_cortex", BRAIN_KG_NODE_PERCEPTION);
        t.auditory      = add_module("auditory_cortex", BRAIN_KG_NODE_PERCEPTION);
        t.hippocampus   = add_module("hippocampus", BRAIN_KG_NODE_COGNITIVE);
        t.prefrontal    = add_module("prefrontal", BRAIN_KG_NODE_COGNITIVE);
        t.plasticity    = add_module("plasticity_mgr", BRAIN_KG_NODE_PLASTICITY);

        EXPECT_NE(t.core_init, BRAIN_KG_INVALID_NODE);
        EXPECT_NE(t.plasticity, BRAIN_KG_INVALID_NODE);

        // Add edges (dependencies)
        t.edge_core_mem   = brain_kg_add_edge(kg, t.core_init, t.memory_mgr,
                                               BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
        t.edge_core_bus   = brain_kg_add_edge(kg, t.core_init, t.event_bus,
                                               BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
        t.edge_bus_visual = brain_kg_add_edge(kg, t.event_bus, t.visual_cortex,
                                               BRAIN_KG_EDGE_SENDS_TO, "events", 0.8f);
        t.edge_bus_audio  = brain_kg_add_edge(kg, t.event_bus, t.auditory,
                                               BRAIN_KG_EDGE_SENDS_TO, "events", 0.8f);
        t.edge_mem_hippo  = brain_kg_add_edge(kg, t.memory_mgr, t.hippocampus,
                                               BRAIN_KG_EDGE_PROVIDES_TO, "memory", 0.9f);
        t.edge_visual_pfc = brain_kg_add_edge(kg, t.visual_cortex, t.prefrontal,
                                               BRAIN_KG_EDGE_SENDS_TO, "visual", 0.7f);
        t.edge_audio_pfc  = brain_kg_add_edge(kg, t.auditory, t.prefrontal,
                                               BRAIN_KG_EDGE_SENDS_TO, "audio", 0.7f);

        return t;
    }
};

// ============================================================================
// Integration: Metadata on Real Graph Edges
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataIntegration, SetMetadataOnRealEdge) {
    // Build a real graph, create hierarchy, then add metadata on actual edges
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set metadata on the core_init -> memory_mgr edge
    int result = kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "latency_us", 150);
    EXPECT_EQ(result, 0);

    int32_t value = 0;
    result = kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "latency_us", &value);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(value, 150);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataOnMultipleEdgesInPipeline) {
    // Set metadata on each edge in a processing pipeline and verify all persist
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Annotate each edge with priority metadata
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "priority", 1), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.event_bus, "priority", 2), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.event_bus, t.visual_cortex, "priority", 3), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.event_bus, t.auditory, "priority", 4), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.memory_mgr, t.hippocampus, "priority", 5), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.visual_cortex, t.prefrontal, "priority", 6), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.auditory, t.prefrontal, "priority", 7), 0);

    // Verify all 7 edges have correct metadata
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "priority", &value), 0);
    EXPECT_EQ(value, 1);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.event_bus, "priority", &value), 0);
    EXPECT_EQ(value, 2);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.event_bus, t.visual_cortex, "priority", &value), 0);
    EXPECT_EQ(value, 3);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.event_bus, t.auditory, "priority", &value), 0);
    EXPECT_EQ(value, 4);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.memory_mgr, t.hippocampus, "priority", &value), 0);
    EXPECT_EQ(value, 5);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.visual_cortex, t.prefrontal, "priority", &value), 0);
    EXPECT_EQ(value, 6);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.auditory, t.prefrontal, "priority", &value), 0);
    EXPECT_EQ(value, 7);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataMultipleKeysPerEdge) {
    // Set multiple different metadata keys on the same edge
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Annotate core_init -> event_bus with several metadata properties
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.event_bus, "latency_us", 250), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.event_bus, "bandwidth_mbps", 1000), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.event_bus, "reliability_pct", 99), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.event_bus, "hop_count", 1), 0);

    // Read back all values
    int32_t latency = 0, bw = 0, rel = 0, hops = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.event_bus, "latency_us", &latency), 0);
    EXPECT_EQ(latency, 250);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.event_bus, "bandwidth_mbps", &bw), 0);
    EXPECT_EQ(bw, 1000);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.event_bus, "reliability_pct", &rel), 0);
    EXPECT_EQ(rel, 99);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.event_bus, "hop_count", &hops), 0);
    EXPECT_EQ(hops, 1);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataDirectionality) {
    // Verify that edge metadata is directional: (A->B) != (B->A)
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set metadata on forward direction
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "direction_flag", 100), 0);

    // Set different metadata on reverse direction
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.memory_mgr, t.core_init, "direction_flag", 200), 0);

    // Verify they are independent
    int32_t fwd = 0, rev = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "direction_flag", &fwd), 0);
    EXPECT_EQ(fwd, 100);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.memory_mgr, t.core_init, "direction_flag", &rev), 0);
    EXPECT_EQ(rev, 200);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataSurvivesStateChangeReporting) {
    // Set metadata, then report state changes on modules - metadata should persist
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set metadata on an edge
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.visual_cortex, t.prefrontal, "signal_strength", 42), 0);

    // Report state changes on both endpoints
    kg_hierarchy_report_state_change(hier, t.visual_cortex,
        KG_MODULE_STATE_RUNNING, "started processing");
    kg_hierarchy_report_state_change(hier, t.prefrontal,
        KG_MODULE_STATE_RUNNING, "started processing");

    // Report health changes
    kg_hierarchy_report_health_change(hier, t.visual_cortex,
        BIO_MODULE_HEALTH_HEALTHY);
    kg_hierarchy_report_health_change(hier, t.prefrontal,
        BIO_MODULE_HEALTH_DEGRADED);

    // Report message stats
    kg_hierarchy_report_message_stats(hier, t.visual_cortex, 100, 50);
    kg_hierarchy_report_message_stats(hier, t.prefrontal, 200, 150);

    // Metadata should still be intact
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.visual_cortex, t.prefrontal, "signal_strength", &value), 0);
    EXPECT_EQ(value, 42);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataUpdateAfterMultipleReads) {
    // Set metadata, read it multiple times, update, read again
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.event_bus, t.auditory, "version", 1), 0);

    // Read 5 times - should always return same value
    for (int i = 0; i < 5; i++) {
        int32_t value = -1;
        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, t.event_bus, t.auditory, "version", &value), 0);
        EXPECT_EQ(value, 1);
    }

    // Update the value
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.event_bus, t.auditory, "version", 2), 0);

    // Confirm update
    int32_t updated = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.event_bus, t.auditory, "version", &updated), 0);
    EXPECT_EQ(updated, 2);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataWithHierarchyRebuild) {
    // Set metadata, rebuild hierarchy, verify metadata survives
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set metadata
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "rebuild_test", 777), 0);

    // Rebuild hierarchy from KG
    int rebuild_result = kg_hierarchy_rebuild(hier);
    EXPECT_EQ(rebuild_result, 0);

    // Metadata is in the hierarchy layer, not the KG layer, so it should
    // still be accessible after rebuild (implementation stores it separately)
    int32_t value = 0;
    int get_result = kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.memory_mgr, "rebuild_test", &value);
    // Note: whether metadata survives rebuild depends on implementation.
    // The current implementation stores metadata in the hierarchy struct
    // separate from KG data, so it should survive.
    EXPECT_EQ(get_result, 0);
    EXPECT_EQ(value, 777);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataOnConvergingEdges) {
    // Two edges converging on prefrontal: visual->pfc and audio->pfc
    // Each should have independent metadata
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.visual_cortex, t.prefrontal, "modality", 1), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.auditory, t.prefrontal, "modality", 2), 0);

    int32_t vis_mod = 0, aud_mod = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.visual_cortex, t.prefrontal, "modality", &vis_mod), 0);
    EXPECT_EQ(vis_mod, 1);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.auditory, t.prefrontal, "modality", &aud_mod), 0);
    EXPECT_EQ(aud_mod, 2);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataOnDivergingEdges) {
    // Two edges diverging from event_bus: bus->visual and bus->audio
    // Each should have independent metadata
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.event_bus, t.visual_cortex, "channel", 10), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.event_bus, t.auditory, "channel", 20), 0);

    int32_t ch_vis = 0, ch_aud = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.event_bus, t.visual_cortex, "channel", &ch_vis), 0);
    EXPECT_EQ(ch_vis, 10);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.event_bus, t.auditory, "channel", &ch_aud), 0);
    EXPECT_EQ(ch_aud, 20);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataOnIsolatedNodeEdge) {
    // Set metadata on an edge involving the isolated node (plasticity_mgr)
    // Even though no KG edge exists, the metadata API works on any (from, to) pair
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Plasticity is isolated - no KG edges, but metadata API should still work
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.plasticity, t.core_init, "virtual_link", 999), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.plasticity, t.core_init, "virtual_link", &value), 0);
    EXPECT_EQ(value, 999);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataWithAnomalyReporting) {
    // Set metadata, report anomaly on endpoint, verify metadata integrity
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.memory_mgr, t.hippocampus, "coherence_score", 95), 0);

    // Report anomaly on hippocampus
    kg_hierarchy_report_anomaly(hier, t.hippocampus, true);

    // Metadata should persist through anomaly reporting
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.memory_mgr, t.hippocampus, "coherence_score", &value), 0);
    EXPECT_EQ(value, 95);

    // Clear anomaly
    kg_hierarchy_report_anomaly(hier, t.hippocampus, false);

    // Still intact
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.memory_mgr, t.hippocampus, "coherence_score", &value), 0);
    EXPECT_EQ(value, 95);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataWithBrainStatsQuery) {
    // Verify that querying brain stats does not disturb edge metadata
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, t.core_init, t.event_bus, "query_test", 42), 0);

    // Query brain stats
    kg_brain_stats_t stats;
    kg_hierarchy_get_brain_stats(hier, &stats);
    EXPECT_GT(stats.total_modules, 0u);

    // Query hemispheres
    kg_hemisphere_info_t hemispheres[KG_HEMISPHERE_COUNT];
    kg_hierarchy_get_hemispheres(hier, hemispheres);

    // Query layers
    kg_layer_info_t layers[KG_LAYER_COUNT];
    kg_hierarchy_get_layers(hier, layers);

    // Metadata should be undisturbed
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, t.core_init, t.event_bus, "query_test", &value), 0);
    EXPECT_EQ(value, 42);
}

TEST_F(KGHierarchyEdgeMetadataIntegration, MetadataBatchSetAndVerify) {
    // Batch-set metadata on all edges, then batch-verify
    TestTopology t = create_test_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    struct EdgeMeta {
        brain_kg_node_id_t from;
        brain_kg_node_id_t to;
        const char* key;
        int32_t value;
    };

    EdgeMeta batch[] = {
        {t.core_init, t.memory_mgr, "batch_id", 100},
        {t.core_init, t.event_bus, "batch_id", 101},
        {t.event_bus, t.visual_cortex, "batch_id", 102},
        {t.event_bus, t.auditory, "batch_id", 103},
        {t.memory_mgr, t.hippocampus, "batch_id", 104},
        {t.visual_cortex, t.prefrontal, "batch_id", 105},
        {t.auditory, t.prefrontal, "batch_id", 106},
    };
    const size_t batch_count = sizeof(batch) / sizeof(batch[0]);

    // Set all
    for (size_t i = 0; i < batch_count; i++) {
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, batch[i].from, batch[i].to, batch[i].key, batch[i].value), 0)
            << "Failed to set batch metadata at index " << i;
    }

    // Verify all
    for (size_t i = 0; i < batch_count; i++) {
        int32_t value = 0;
        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, batch[i].from, batch[i].to, batch[i].key, &value), 0)
            << "Failed to get batch metadata at index " << i;
        EXPECT_EQ(value, batch[i].value)
            << "Value mismatch at index " << i;
    }
}
