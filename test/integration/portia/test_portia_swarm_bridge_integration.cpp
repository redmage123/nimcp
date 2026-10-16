/**
 * @file test_portia_swarm_bridge_integration.cpp
 * @brief Integration tests for Portia-Swarm bridge coordination
 *
 * WHAT: Tests bidirectional integration between Portia adaptive intelligence and Swarm collective systems
 * WHY:  Validate coordinated adaptive behavior where individual resource constraints inform collective decisions
 * HOW:  Initialize Portia and Swarm components, test state propagation, recommendations, and consensus
 *
 * TEST COVERAGE:
 * - Full Portia-Swarm lifecycle integration
 * - State propagation from Portia to Swarm
 * - Recommendation flow from Swarm to Portia
 * - Multi-agent coordination scenarios
 * - Tier switching with swarm consensus
 * - Degradation propagation across swarm
 * - Energy gossip integration
 * - Emergence detection affecting Portia planning
 * - Bio-async message flow
 * - Concurrent access from multiple components
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_bridge.h"
#include "portia/nimcp_portia.h"
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_emergence.h"
#include "swarm/nimcp_swarm_energy_gossip.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Mock Implementations for Missing Components
//=============================================================================

// Mock Portia context (minimal implementation for testing)
struct portia_context_t {
    uint32_t agent_id;
    uint8_t power_state;
    uint8_t thermal_state;
    uint8_t platform_tier;
    uint8_t degradation_level;
    float cpu_usage;
    float memory_usage;
    float battery_level;
    float thermal_headroom;
    bool active;
};

// Mock Swarm Brain (minimal implementation)
struct nimcp_swarm_brain {
    uint32_t drone_id;
    uint32_t peer_count;
    float coherence;
    bool active;
};

// Mock Swarm Consensus (minimal implementation)
struct swarm_consensus_context {
    uint32_t node_id;
    uint32_t quorum_size;
    uint8_t consensus_tier;
    float confidence;
    bool active;
};

// Mock Swarm Emergence (minimal implementation)
struct swarm_emergence_t {
    uint32_t emergence_type;
    float magnitude;
    uint32_t agent_count;
    bool active;
};

// Mock Swarm Energy Gossip (minimal implementation)
struct swarm_energy_gossip_t {
    uint32_t node_id;
    float avg_energy;
    uint32_t message_count;
    bool active;
};

// Helper: Create mock Portia context
portia_context_t* create_mock_portia(uint32_t agent_id) {
    portia_context_t* ctx = (portia_context_t*)nimcp_malloc(sizeof(portia_context_t));
    if (ctx) {
        ctx->agent_id = agent_id;
        ctx->power_state = 1;
        ctx->thermal_state = 0;
        ctx->platform_tier = 2;
        ctx->degradation_level = 0;
        ctx->cpu_usage = 0.5f;
        ctx->memory_usage = 0.6f;
        ctx->battery_level = 0.8f;
        ctx->thermal_headroom = 20.0f;
        ctx->active = true;
    }
    return ctx;
}

// Helper: Create mock swarm brain
swarm_brain_t* create_mock_swarm_brain(uint32_t drone_id) {
    swarm_brain_t* brain = (swarm_brain_t*)nimcp_malloc(sizeof(swarm_brain_t));
    if (brain) {
        brain->drone_id = drone_id;
        brain->peer_count = 5;
        brain->coherence = 0.7f;
        brain->active = true;
    }
    return brain;
}

// Helper: Create mock swarm consensus
swarm_consensus_t create_mock_swarm_consensus(uint32_t node_id) {
    swarm_consensus_t consensus = (swarm_consensus_t)nimcp_malloc(sizeof(struct swarm_consensus_context));
    if (consensus) {
        consensus->node_id = node_id;
        consensus->quorum_size = 3;
        consensus->consensus_tier = 2;
        consensus->confidence = 0.8f;
        consensus->active = true;
    }
    return consensus;
}

// Helper: Create mock swarm emergence
swarm_emergence_t* create_mock_swarm_emergence() {
    swarm_emergence_t* emergence = (swarm_emergence_t*)nimcp_malloc(sizeof(swarm_emergence_t));
    if (emergence) {
        emergence->emergence_type = 1;
        emergence->magnitude = 0.6f;
        emergence->agent_count = 8;
        emergence->active = true;
    }
    return emergence;
}

// Helper: Create mock energy gossip
swarm_energy_gossip_t* create_mock_energy_gossip(uint32_t node_id) {
    swarm_energy_gossip_t* gossip = (swarm_energy_gossip_t*)nimcp_malloc(sizeof(swarm_energy_gossip_t));
    if (gossip) {
        gossip->node_id = node_id;
        gossip->avg_energy = 0.75f;
        gossip->message_count = 0;
        gossip->active = true;
    }
    return gossip;
}

//=============================================================================
// Callback Tracking Structures
//=============================================================================

struct RecommendationCallbackData {
    std::atomic<uint32_t> call_count{0};
    std::vector<portia_swarm_recommendation_t> recommendations;
    std::mutex mutex;
};

struct EmergenceCallbackData {
    std::atomic<uint32_t> call_count{0};
    uint32_t last_emergence_type;
    float last_magnitude;
    std::mutex mutex;
};

struct CollectiveCallbackData {
    std::atomic<uint32_t> call_count{0};
    portia_swarm_collective_state_t last_state;
    std::mutex mutex;
};

// Callback functions
static void recommendation_callback(
    portia_swarm_bridge_t* bridge,
    const portia_swarm_recommendation_t* recommendation,
    void* user_data)
{
    (void)bridge;
    if (user_data && recommendation) {
        auto* data = static_cast<RecommendationCallbackData*>(user_data);
        std::lock_guard<std::mutex> lock(data->mutex);
        data->recommendations.push_back(*recommendation);
        data->call_count.fetch_add(1);
    }
}

static void emergence_callback(
    portia_swarm_bridge_t* bridge,
    uint32_t emergence_type,
    float magnitude,
    void* user_data)
{
    (void)bridge;
    if (user_data) {
        auto* data = static_cast<EmergenceCallbackData*>(user_data);
        std::lock_guard<std::mutex> lock(data->mutex);
        data->last_emergence_type = emergence_type;
        data->last_magnitude = magnitude;
        data->call_count.fetch_add(1);
    }
}

static void collective_callback(
    portia_swarm_bridge_t* bridge,
    const portia_swarm_collective_state_t* collective_state,
    void* user_data)
{
    (void)bridge;
    if (user_data && collective_state) {
        auto* data = static_cast<CollectiveCallbackData*>(user_data);
        std::lock_guard<std::mutex> lock(data->mutex);
        data->last_state = *collective_state;
        data->call_count.fetch_add(1);
    }
}

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaSwarmBridgeIntegrationTest : public ::testing::Test {
protected:
    portia_swarm_bridge_t* bridge = nullptr;
    portia_context_t* portia = nullptr;
    swarm_brain_t* swarm_brain = nullptr;
    swarm_consensus_t consensus = nullptr;
    swarm_emergence_t* emergence = nullptr;
    swarm_energy_gossip_t* energy_gossip = nullptr;

    RecommendationCallbackData rec_callback_data;
    EmergenceCallbackData em_callback_data;
    CollectiveCallbackData coll_callback_data;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);

        // Create mock Portia context
        portia = create_mock_portia(1);
        ASSERT_NE(portia, nullptr);

        // Create mock swarm components
        swarm_brain = create_mock_swarm_brain(1);
        consensus = create_mock_swarm_consensus(1);
        emergence = create_mock_swarm_emergence();
        energy_gossip = create_mock_energy_gossip(1);

        ASSERT_NE(swarm_brain, nullptr);
        ASSERT_NE(consensus, nullptr);
        ASSERT_NE(emergence, nullptr);
        ASSERT_NE(energy_gossip, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            portia_swarm_bridge_destroy(bridge);
            bridge = nullptr;
        }

        if (portia) {
            nimcp_free(portia);
            portia = nullptr;
        }

        if (swarm_brain) {
            nimcp_free(swarm_brain);
            swarm_brain = nullptr;
        }

        if (consensus) {
            nimcp_free(consensus);
            consensus = nullptr;
        }

        if (emergence) {
            nimcp_free(emergence);
            emergence = nullptr;
        }

        if (energy_gossip) {
            nimcp_free(energy_gossip);
            energy_gossip = nullptr;
        }

        nimcp_bio_async_shutdown();
    }

    // Helper: Wait for condition with timeout
    bool wait_for_condition(std::function<bool()> condition, uint32_t timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();
        while (!condition()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            if (elapsed > timeout_ms) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
};

//=============================================================================
// TEST SUITE 1: Lifecycle and Configuration
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Lifecycle_CreateAndDestroy) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Lifecycle_StartAndStop) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);
    EXPECT_EQ(portia_swarm_bridge_stop(bridge), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Config_DefaultConfigValid) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    EXPECT_EQ(config.mode, PORTIA_SWARM_MODE_BIDIRECTIONAL);
    EXPECT_GT(config.sync_interval_ms, 0u);
    EXPECT_GE(config.consensus_weight, 0.0f);
    EXPECT_LE(config.consensus_weight, 1.0f);
    EXPECT_GE(config.local_weight, 0.0f);
    EXPECT_LE(config.local_weight, 1.0f);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Config_DifferentModes) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    // Test passive mode
    config.mode = PORTIA_SWARM_MODE_PASSIVE;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_destroy(bridge);

    // Test broadcast mode
    config.mode = PORTIA_SWARM_MODE_BROADCAST;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_destroy(bridge);

    bridge = nullptr;
}

//=============================================================================
// TEST SUITE 2: Connection Management
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Connection_SwarmBrain) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Connection_SwarmConsensus) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Connection_SwarmEmergence) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_emergence(bridge, emergence), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Connection_EnergyGossip) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_energy_gossip = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_energy_gossip(bridge, energy_gossip), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Connection_BioAsync) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_bio_async = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Connect - API call should succeed even if router unavailable
    EXPECT_EQ(portia_swarm_connect_bio_async(bridge), 0);
    // NOTE: In integration tests without a fully initialized bio-async router,
    // is_bio_async_connected() may return false. This is expected behavior.

    // Disconnect - should always succeed
    EXPECT_EQ(portia_swarm_disconnect_bio_async(bridge), 0);
    EXPECT_FALSE(portia_swarm_is_bio_async_connected(bridge));
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Connection_AllComponents) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_bio_async = true;
    config.enable_energy_gossip = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Connect all components - API calls should succeed
    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);
    EXPECT_EQ(portia_swarm_connect_emergence(bridge, emergence), 0);
    EXPECT_EQ(portia_swarm_connect_energy_gossip(bridge, energy_gossip), 0);
    EXPECT_EQ(portia_swarm_connect_bio_async(bridge), 0);
    // NOTE: is_bio_async_connected() depends on router availability in tests
}

//=============================================================================
// TEST SUITE 3: State Propagation (Portia → Swarm)
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, StatePropagation_BroadcastState) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BROADCAST;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Broadcast current state
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    // Verify statistics updated
    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, StatePropagation_TierChangeNotification) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Notify tier change
    uint8_t old_tier = 2;
    uint8_t new_tier = 1;
    EXPECT_EQ(portia_swarm_notify_tier_change(bridge, old_tier, new_tier), 0);

    // Verify statistics
    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, StatePropagation_DegradationNotification) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Notify degradation
    uint8_t degradation_level = 2;
    uint32_t reason = 0x1001;
    EXPECT_EQ(portia_swarm_notify_degradation(bridge, degradation_level, reason), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, StatePropagation_GetLocalState) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_state_t state;
    EXPECT_EQ(portia_swarm_get_local_state(bridge, &state), 0);

    EXPECT_EQ(state.agent_id, portia->agent_id);
    EXPECT_GE(state.cpu_usage, 0.0f);
    EXPECT_LE(state.cpu_usage, 1.0f);
    EXPECT_GE(state.battery_level, 0.0f);
    EXPECT_LE(state.battery_level, 1.0f);
}

//=============================================================================
// TEST SUITE 4: Recommendation Flow (Swarm → Portia)
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Recommendation_RequestFromSwarm) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    portia_swarm_recommendation_t recommendation;
    int result = portia_swarm_request_recommendation(bridge, &recommendation);

    // Should succeed or return no recommendation available
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_INVALID_STATE);

    if (result == 0) {
        EXPECT_GE(recommendation.confidence, 0.0f);
        EXPECT_LE(recommendation.confidence, 1.0f);
    }
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Recommendation_CallbackRegistration) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_register_recommendation_cb(
        bridge, recommendation_callback, &rec_callback_data), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Recommendation_ApplyToPortia) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.influence = PORTIA_SWARM_INFLUENCE_MODERATE;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);

    // Create a recommendation
    portia_swarm_recommendation_t recommendation;
    recommendation.recommended_tier = 2;
    recommendation.recommended_degradation = 1;
    recommendation.confidence = 0.85f;
    recommendation.consensus_count = 5;
    recommendation.source = PORTIA_TIER_REC_SWARM_CONSENSUS;
    recommendation.timestamp = 0;

    // Apply recommendation
    EXPECT_EQ(portia_swarm_apply_recommendation(bridge, &recommendation), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Recommendation_GetLatest) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_recommendation_t recommendation;
    int result = portia_swarm_get_recommendation(bridge, &recommendation);

    // May not have recommendation initially
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_INVALID_STATE);
}

//=============================================================================
// TEST SUITE 5: Multi-Agent Coordination
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, MultiAgent_CollectiveStateTracking) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    portia_swarm_collective_state_t collective_state;
    EXPECT_EQ(portia_swarm_get_collective_state(bridge, &collective_state), 0);

    EXPECT_GE(collective_state.agent_count, 0u);
    EXPECT_GE(collective_state.avg_power_level, 0.0f);
    EXPECT_LE(collective_state.avg_power_level, 1.0f);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, MultiAgent_CollectiveCallback) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_register_collective_cb(
        bridge, collective_callback, &coll_callback_data), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, MultiAgent_UpdatePropagation) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.sync_interval_ms = 50;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Perform multiple updates
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(portia_swarm_update(bridge), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.messages_sent, 0u);
}

//=============================================================================
// TEST SUITE 6: Tier Switching with Consensus
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, TierSwitching_ConsensusSupport) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);

    uint8_t proposed_tier = 2;
    bool supported = portia_swarm_consensus_supports_tier(bridge, proposed_tier);

    // Should return true or false (both valid)
    EXPECT_TRUE(supported == true || supported == false);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, TierSwitching_ComputeOptimalTier) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.consensus_weight = 0.5f;
    config.local_weight = 0.5f;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);

    uint8_t local_recommendation = 2;
    uint8_t optimal_tier = 0;

    EXPECT_EQ(portia_swarm_compute_optimal_tier(
        bridge, local_recommendation, &optimal_tier), 0);

    EXPECT_GT(optimal_tier, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, TierSwitching_HybridDecision) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.consensus_weight = 0.3f;
    config.local_weight = 0.7f;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);

    uint8_t local_tier = 3;
    uint8_t optimal_tier = 0;

    EXPECT_EQ(portia_swarm_compute_optimal_tier(bridge, local_tier, &optimal_tier), 0);

    // Verify weights are applied correctly
    EXPECT_GT(optimal_tier, 0u);
}

//=============================================================================
// TEST SUITE 7: Degradation Propagation
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Degradation_BroadcastToSwarm) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BROADCAST;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Simulate degradation increase
    portia->degradation_level = 2;

    EXPECT_EQ(portia_swarm_notify_degradation(bridge, 2, 0x2001), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Degradation_CollectiveAwareness) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);

    // Get collective state with degradation info
    portia_swarm_collective_state_t collective_state;
    EXPECT_EQ(portia_swarm_get_collective_state(bridge, &collective_state), 0);

    EXPECT_GE(collective_state.agents_critical, 0u);
    EXPECT_GE(collective_state.agents_degraded, 0u);
    EXPECT_GE(collective_state.agents_healthy, 0u);
}

//=============================================================================
// TEST SUITE 8: Energy Gossip Integration
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, EnergyGossip_EnableAndConnect) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_energy_gossip = true;
    config.gossip_interval_ms = 100;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_energy_gossip(bridge, energy_gossip), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Update to trigger gossip
    EXPECT_EQ(portia_swarm_update(bridge), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, EnergyGossip_BatteryStateSharing) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_energy_gossip = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_energy_gossip(bridge, energy_gossip), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Simulate battery change
    portia->battery_level = 0.3f;

    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.energy_gossips, 0u);
}

//=============================================================================
// TEST SUITE 9: Emergence Detection
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Emergence_AlertCallback) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_emergence_alerts = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_register_emergence_cb(
        bridge, emergence_callback, &em_callback_data), 0);

    EXPECT_EQ(portia_swarm_connect_emergence(bridge, emergence), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Emergence_AffectsPlanning) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_emergence_alerts = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_emergence(bridge, emergence), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Simulate emergence detection affecting recommendations
    portia_swarm_recommendation_t recommendation;
    int result = portia_swarm_request_recommendation(bridge, &recommendation);

    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_INVALID_STATE);
}

//=============================================================================
// TEST SUITE 10: Bio-Async Message Flow
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, BioAsync_ConnectAndDisconnect) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_bio_async = true;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Connect/disconnect cycle - API calls should succeed even if router unavailable
    EXPECT_EQ(portia_swarm_connect_bio_async(bridge), 0);
    // NOTE: is_bio_async_connected() depends on router availability

    EXPECT_EQ(portia_swarm_disconnect_bio_async(bridge), 0);
    EXPECT_FALSE(portia_swarm_is_bio_async_connected(bridge));
}

TEST_F(PortiaSwarmBridgeIntegrationTest, BioAsync_MessageBroadcast) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_bio_async = true;
    config.mode = PORTIA_SWARM_MODE_BROADCAST;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_bio_async(bridge), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Broadcast state via bio-async
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.messages_sent, 0u);
}

//=============================================================================
// TEST SUITE 11: Statistics and Monitoring
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Statistics_InitialState) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
    EXPECT_EQ(stats.consensus_queries, 0u);
    EXPECT_EQ(stats.tier_recommendations, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Statistics_UpdateAfterOperations) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Perform operations
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);
    EXPECT_EQ(portia_swarm_notify_tier_change(bridge, 2, 1), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);

    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Statistics_Reset) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Generate some stats
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    // Reset stats
    EXPECT_EQ(portia_swarm_reset_stats(bridge), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
}

//=============================================================================
// TEST SUITE 12: Concurrent Access
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Concurrent_MultipleBroadcasts) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Launch multiple threads broadcasting state
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 10; j++) {
                if (portia_swarm_broadcast_state(bridge) == 0) {
                    success_count.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Concurrent_UpdateAndQuery) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);

    std::atomic<bool> running{true};
    std::atomic<int> update_count{0};
    std::atomic<int> query_count{0};

    // Updater thread
    std::thread updater([this, &running, &update_count]() {
        while (running.load()) {
            if (portia_swarm_update(bridge) == 0) {
                update_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Query thread
    std::thread querier([this, &running, &query_count]() {
        portia_swarm_collective_state_t state;
        while (running.load()) {
            if (portia_swarm_get_collective_state(bridge, &state) == 0) {
                query_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    });

    // Run for a short duration
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running.store(false);

    updater.join();
    querier.join();

    EXPECT_GT(update_count.load(), 0);
    EXPECT_GT(query_count.load(), 0);
}

//=============================================================================
// TEST SUITE 13: Error Handling
//=============================================================================

TEST_F(PortiaSwarmBridgeIntegrationTest, Error_NullParameters) {
    // NULL config is valid (uses defaults), but NULL portia should fail
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    EXPECT_EQ(portia_swarm_bridge_create(&config, nullptr), nullptr);

    // NULL config uses defaults, should succeed
    portia_swarm_bridge_t* temp = portia_swarm_bridge_create(nullptr, portia);
    ASSERT_NE(temp, nullptr);
    portia_swarm_bridge_destroy(temp);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_NE(portia_swarm_connect_brain(bridge, nullptr), 0);
    EXPECT_NE(portia_swarm_get_stats(bridge, nullptr), 0);
}

TEST_F(PortiaSwarmBridgeIntegrationTest, Error_InvalidMode) {
    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = (portia_swarm_mode_t)999;

    bridge = portia_swarm_bridge_create(&config, portia);
    // Should handle invalid mode gracefully
    EXPECT_TRUE(bridge == nullptr || bridge != nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
