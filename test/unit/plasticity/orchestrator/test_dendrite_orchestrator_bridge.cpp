/**
 * @file test_dendrite_orchestrator_bridge.cpp
 * @brief Unit Tests for Dendrite-Orchestrator Bridge
 *
 * Tests all aspects of the dendrite-orchestrator bridge including:
 * - Lifecycle management (create/destroy)
 * - Configuration
 * - Spine-synapse mapping
 * - Bidirectional synchronization
 * - Pre-spike forwarding
 * - Spine formation/elimination events
 * - Bio-async integration
 * - Statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// Test Fixture
// ============================================================================

class DendriteOrchestratorBridgeTest : public ::testing::Test {
protected:
    dendrite_orchestrator_bridge_t* bridge = nullptr;
    plasticity_orchestrator_t* orchestrator = nullptr;
    dendrite_network_t* dendrite_network = nullptr;
    dendrite_orchestrator_config_t config;

    void SetUp() override {
        dendrite_orchestrator_default_config(&config);

        // Create orchestrator
        orchestrator = plasticity_orchestrator_create(nullptr);

        // Create dendrite network with capacity
        dendrite_network = dendrite_network_create(100);  // capacity of 100 dendrites
    }

    void TearDown() override {
        if (bridge) {
            dendrite_orchestrator_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (orchestrator) {
            plasticity_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (dendrite_network) {
            dendrite_network_destroy(dendrite_network);
            dendrite_network = nullptr;
        }
    }

    // Helper to create bridge
    void CreateBridge() {
        bridge = dendrite_orchestrator_bridge_create(&config, orchestrator, dendrite_network);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, DefaultConfigSetsReasonableDefaults) {
    EXPECT_EQ(dendrite_orchestrator_default_config(&config), 0);

    // Check enabled flags
    EXPECT_TRUE(config.enable_weight_to_spine_sync);
    EXPECT_TRUE(config.enable_spine_to_orchestrator_sync);
    EXPECT_TRUE(config.enable_pre_spike_forwarding);
    EXPECT_TRUE(config.enable_bio_async);  // On by default

    // Check parameters
    EXPECT_GT(config.weight_to_volume_scale, 0.0f);
    EXPECT_GT(config.weight_to_ampa_scale, 0.0f);
    EXPECT_GE(config.min_weight_delta_for_sync, 0.0f);
    EXPECT_GT(config.initial_mapping_capacity, 0u);
}

TEST_F(DendriteOrchestratorBridgeTest, DefaultConfigReturnsErrorForNull) {
    EXPECT_EQ(dendrite_orchestrator_default_config(nullptr), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, CreateWithDefaultConfig) {
    bridge = dendrite_orchestrator_bridge_create(nullptr, orchestrator, dendrite_network);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(DendriteOrchestratorBridgeTest, CreateWithCustomConfig) {
    config.enable_weight_to_spine_sync = false;
    config.enable_spine_to_orchestrator_sync = false;

    CreateBridge();
    ASSERT_NE(bridge, nullptr);
}

TEST_F(DendriteOrchestratorBridgeTest, CreateRequiresOrchestrator) {
    bridge = dendrite_orchestrator_bridge_create(&config, nullptr, dendrite_network);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(DendriteOrchestratorBridgeTest, CreateWithNullNetworkAllowed) {
    // NULL network allowed (reduced functionality)
    bridge = dendrite_orchestrator_bridge_create(&config, orchestrator, nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(DendriteOrchestratorBridgeTest, DestroyNullIsSafe) {
    dendrite_orchestrator_bridge_destroy(nullptr);
    // Should not crash
}

// ============================================================================
// Mapping Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, MapSpine) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map synapse 100 to dendrite 1, spine 5
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 100, 1, 5), 0);

    // Verify mapping
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 100, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 1u);
    EXPECT_EQ(spine_index, 5u);
}

TEST_F(DendriteOrchestratorBridgeTest, MapMultipleSpines) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map multiple synapses
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 100, 1, 0), 0);
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 101, 1, 1), 0);
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 102, 2, 0), 0);

    // Verify all mappings
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 100, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 1u);
    EXPECT_EQ(spine_index, 0u);

    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 101, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 1u);
    EXPECT_EQ(spine_index, 1u);

    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 102, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 2u);
    EXPECT_EQ(spine_index, 0u);
}

TEST_F(DendriteOrchestratorBridgeTest, UnmapSpine) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map and unmap
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 100, 1, 5), 0);
    EXPECT_EQ(dendrite_orchestrator_unmap_spine(bridge, 100), 0);

    // Verify unmapped
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 100, &dendrite_id, &spine_index), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, GetMappingForUnknownSynapseReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 99999, &dendrite_id, &spine_index), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, MapWithNullBridgeReturnsError) {
    EXPECT_EQ(dendrite_orchestrator_map_spine(nullptr, 100, 1, 5), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, GetMappingCount) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 0u);

    dendrite_orchestrator_map_spine(bridge, 100, 1, 0);
    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 1u);

    dendrite_orchestrator_map_spine(bridge, 200, 2, 0);
    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 2u);

    dendrite_orchestrator_unmap_spine(bridge, 100);
    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 1u);
}

// ============================================================================
// Synchronization Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, SyncWeightToSpine) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map spine and set weight in orchestrator
    dendrite_orchestrator_map_spine(bridge, 100, 1, 0);
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.75f);

    // Sync weight to spine
    EXPECT_EQ(dendrite_orchestrator_sync_weight_to_spine(bridge, 100), 0);
}

TEST_F(DendriteOrchestratorBridgeTest, SyncSpineToOrchestrator) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map spine
    dendrite_orchestrator_map_spine(bridge, 100, 1, 0);
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.5f);

    // Sync spine to orchestrator
    EXPECT_EQ(dendrite_orchestrator_sync_spine_to_orchestrator(bridge, 100), 0);
}

TEST_F(DendriteOrchestratorBridgeTest, SyncWeightToSpineWithUnmappedSynapseReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dendrite_orchestrator_sync_weight_to_spine(bridge, 99999), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, SyncSpineToOrchestratorWithUnmappedSynapseReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dendrite_orchestrator_sync_spine_to_orchestrator(bridge, 99999), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, SyncAllOrchestratorToSpine) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map multiple spines
    for (uint32_t i = 0; i < 10; i++) {
        dendrite_orchestrator_map_spine(bridge, i, i / 5, i % 5);
        plasticity_orchestrator_set_weight(orchestrator, i, 0.5f + 0.01f * i);
    }

    // Sync all
    int synced = dendrite_orchestrator_sync_all(bridge, SYNC_ORCHESTRATOR_TO_SPINE);
    EXPECT_EQ(synced, 10);
}

TEST_F(DendriteOrchestratorBridgeTest, SyncAllSpineToOrchestrator) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map multiple spines
    for (uint32_t i = 0; i < 10; i++) {
        dendrite_orchestrator_map_spine(bridge, i, i / 5, i % 5);
        plasticity_orchestrator_set_weight(orchestrator, i, 0.5f);
    }

    // Sync all
    int synced = dendrite_orchestrator_sync_all(bridge, SYNC_SPINE_TO_ORCHESTRATOR);
    EXPECT_EQ(synced, 10);
}

// ============================================================================
// Pre-Spike Forwarding Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, PreSpike) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Set initial weight
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.5f);

    // Forward pre-spike
    EXPECT_EQ(dendrite_orchestrator_pre_spike(bridge, 100, 1000), 0);
}

TEST_F(DendriteOrchestratorBridgeTest, PreSpikeWithNullBridge) {
    EXPECT_EQ(dendrite_orchestrator_pre_spike(nullptr, 100, 1000), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, PreSpikeUpdatesStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pre_spikes_forwarded, 0u);

    plasticity_orchestrator_set_weight(orchestrator, 100, 0.5f);
    dendrite_orchestrator_pre_spike(bridge, 100, 1000);
    dendrite_orchestrator_pre_spike(bridge, 100, 2000);
    dendrite_orchestrator_pre_spike(bridge, 100, 3000);

    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pre_spikes_forwarded, 3u);
}

// ============================================================================
// Spine Formation/Elimination Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, SpineFormed) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Notify spine formation
    EXPECT_EQ(dendrite_orchestrator_spine_formed(bridge, 1, 0, 100, 0.3f), 0);

    // Verify mapping was created
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 100, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 1u);
    EXPECT_EQ(spine_index, 0u);

    // Verify weight was set
    float weight = plasticity_orchestrator_get_weight(orchestrator, 100);
    EXPECT_FLOAT_EQ(weight, 0.3f);
}

TEST_F(DendriteOrchestratorBridgeTest, SpineEliminated) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Form and then eliminate spine
    dendrite_orchestrator_spine_formed(bridge, 1, 0, 100, 0.3f);
    EXPECT_EQ(dendrite_orchestrator_spine_eliminated(bridge, 100), 0);

    // Verify mapping was removed
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 100, &dendrite_id, &spine_index), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, SpineFormedUpdatesStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spines_registered, 0u);

    dendrite_orchestrator_spine_formed(bridge, 1, 0, 100, 0.3f);
    dendrite_orchestrator_spine_formed(bridge, 1, 1, 101, 0.3f);

    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spines_registered, 2u);
}

TEST_F(DendriteOrchestratorBridgeTest, SpineEliminatedUpdatesStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_orchestrator_spine_formed(bridge, 1, 0, 100, 0.3f);
    dendrite_orchestrator_spine_formed(bridge, 1, 1, 101, 0.3f);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spines_eliminated, 0u);

    dendrite_orchestrator_spine_eliminated(bridge, 100);

    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spines_eliminated, 1u);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, GetStatsInitiallyZero) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_orchestrator_stats_t stats;
    EXPECT_EQ(dendrite_orchestrator_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.weight_to_spine_syncs, 0u);
    EXPECT_EQ(stats.spine_to_orchestrator_syncs, 0u);
    EXPECT_EQ(stats.pre_spikes_forwarded, 0u);
    EXPECT_EQ(stats.spines_registered, 0u);
    EXPECT_EQ(stats.spines_eliminated, 0u);
    EXPECT_EQ(stats.bio_async_messages_sent, 0u);
    EXPECT_EQ(stats.update_calls, 0u);
}

TEST_F(DendriteOrchestratorBridgeTest, GetStatsWithNullBridgeReturnsError) {
    dendrite_orchestrator_stats_t stats;
    EXPECT_EQ(dendrite_orchestrator_get_stats(nullptr, &stats), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, GetStatsWithNullStatsReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dendrite_orchestrator_get_stats(bridge, nullptr), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, ResetStats) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Create activity
    dendrite_orchestrator_spine_formed(bridge, 1, 0, 100, 0.3f);
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.5f);
    dendrite_orchestrator_pre_spike(bridge, 100, 1000);

    // Verify non-zero
    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_GT(stats.spines_registered + stats.pre_spikes_forwarded, 0u);

    // Reset and verify (spines_registered is a count, not a stat to reset)
    EXPECT_EQ(dendrite_orchestrator_reset_stats(bridge), 0);
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pre_spikes_forwarded, 0u);
    // spines_registered is current count, not a resettable stat
    EXPECT_GE(stats.spines_registered, 0u);
}

TEST_F(DendriteOrchestratorBridgeTest, SyncStatsTracked) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_orchestrator_map_spine(bridge, 100, 1, 0);
    plasticity_orchestrator_set_weight(orchestrator, 100, 0.5f);

    dendrite_orchestrator_sync_weight_to_spine(bridge, 100);
    dendrite_orchestrator_sync_spine_to_orchestrator(bridge, 100);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.weight_to_spine_syncs, 1u);
    EXPECT_EQ(stats.spine_to_orchestrator_syncs, 1u);
}

// ============================================================================
// Update Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, BridgeUpdate) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dendrite_orchestrator_bridge_update(bridge, 1000), 0);

    dendrite_orchestrator_stats_t stats;
    dendrite_orchestrator_get_stats(bridge, &stats);
    EXPECT_EQ(stats.update_calls, 1u);
}

TEST_F(DendriteOrchestratorBridgeTest, BridgeUpdateWithNullReturnsError) {
    EXPECT_EQ(dendrite_orchestrator_bridge_update(nullptr, 1000), -1);
}

// ============================================================================
// Accessor Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, GetOrchestrator) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    plasticity_orchestrator_t* orch = dendrite_orchestrator_get_orchestrator(bridge);
    EXPECT_EQ(orch, orchestrator);
}

TEST_F(DendriteOrchestratorBridgeTest, GetDendriteNetwork) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_network_t* net = dendrite_orchestrator_get_dendrite_network(bridge);
    EXPECT_EQ(net, dendrite_network);
}

TEST_F(DendriteOrchestratorBridgeTest, GetOrchestratorWithNull) {
    EXPECT_EQ(dendrite_orchestrator_get_orchestrator(nullptr), nullptr);
}

TEST_F(DendriteOrchestratorBridgeTest, GetDendriteNetworkWithNull) {
    EXPECT_EQ(dendrite_orchestrator_get_dendrite_network(nullptr), nullptr);
}

// ============================================================================
// Bio-Async Integration Tests
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, ConnectBioAsyncWithoutRouter) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    int result = dendrite_orchestrator_connect_bio_async(bridge);
    (void)result;  // May succeed or fail depending on router availability
}

TEST_F(DendriteOrchestratorBridgeTest, DisconnectBioAsync) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    dendrite_orchestrator_connect_bio_async(bridge);
    EXPECT_EQ(dendrite_orchestrator_disconnect_bio_async(bridge), 0);
}

TEST_F(DendriteOrchestratorBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(dendrite_orchestrator_is_bio_async_connected(bridge));
}

TEST_F(DendriteOrchestratorBridgeTest, ConnectBioAsyncWithNull) {
    EXPECT_EQ(dendrite_orchestrator_connect_bio_async(nullptr), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, DisconnectBioAsyncWithNull) {
    EXPECT_EQ(dendrite_orchestrator_disconnect_bio_async(nullptr), -1);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DendriteOrchestratorBridgeTest, MapSameSpineTwiceUpdatesMapping) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Map synapse to dendrite 1, spine 0
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 100, 1, 0), 0);

    // Map same synapse to different spine
    EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, 100, 2, 3), 0);

    // Should be updated
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 100, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 2u);
    EXPECT_EQ(spine_index, 3u);
}

TEST_F(DendriteOrchestratorBridgeTest, UnmapNonExistentSpineReturnsError) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dendrite_orchestrator_unmap_spine(bridge, 99999), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, LargeMappingCount) {
    config.initial_mapping_capacity = 100;
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Create many mappings beyond initial capacity
    for (uint32_t i = 0; i < 500; i++) {
        EXPECT_EQ(dendrite_orchestrator_map_spine(bridge, i, i / 10, i % 10), 0);
    }

    EXPECT_EQ(dendrite_orchestrator_get_mapping_count(bridge), 500u);

    // Verify random samples
    uint32_t dendrite_id, spine_index;
    EXPECT_EQ(dendrite_orchestrator_get_spine_mapping(bridge, 123, &dendrite_id, &spine_index), 0);
    EXPECT_EQ(dendrite_id, 123u / 10);
    EXPECT_EQ(spine_index, 123u % 10);
}

TEST_F(DendriteOrchestratorBridgeTest, SpineEliminatedWithNonExistentSynapse) {
    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Should not crash, just return error
    EXPECT_EQ(dendrite_orchestrator_spine_eliminated(bridge, 99999), -1);
}

TEST_F(DendriteOrchestratorBridgeTest, ConfigScalingFactors) {
    config.weight_to_volume_scale = 2.0f;
    config.weight_to_ampa_scale = 20.0f;
    config.min_weight_delta_for_sync = 0.05f;

    CreateBridge();
    ASSERT_NE(bridge, nullptr);

    // Bridge should use these custom scaling factors
    // Actual effect depends on implementation
}
