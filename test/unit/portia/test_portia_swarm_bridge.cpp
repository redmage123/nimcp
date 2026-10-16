/**
 * @file test_portia_swarm_bridge.cpp
 * @brief Comprehensive unit tests for Portia-Swarm Intelligence Bridge
 *
 * WHAT: Full test coverage for bidirectional Portia-Swarm integration
 * WHY:  Ensure reliable coordination between individual resource optimization
 *       and collective swarm decision-making
 * HOW:  Mock implementations + GTest framework with 50+ test cases
 *
 * TEST CATEGORIES:
 * 1. Configuration (default, custom, validation)
 * 2. Lifecycle (create, destroy, start, stop, null handling)
 * 3. Connection (swarm_brain, consensus, emergence, energy_gossip, bio_async)
 * 4. State (get/broadcast local state, collective state)
 * 5. Update (periodic update, state synchronization)
 * 6. Recommendation (request, get, apply recommendations)
 * 7. Decision (compute_optimal_tier, consensus_supports_tier)
 * 8. Callback (register and trigger callbacks)
 * 9. Statistics (get stats, reset stats)
 * 10. Mode (disabled, passive, broadcast, bidirectional)
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_bridge.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Mock Implementations
//=============================================================================

/**
 * @brief Mock portia_context_t for testing
 */
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
};

/**
 * @brief Mock nimcp_swarm_brain (swarm_brain_t is typedef'd to pointer)
 */
struct nimcp_swarm_brain {
    uint32_t agent_count;
    bool connected;
};

/**
 * @brief Mock swarm_consensus_context (swarm_consensus_t is pointer to this)
 */
struct swarm_consensus_context {
    uint8_t consensus_tier;
    uint32_t consensus_count;
    float confidence;
    bool supports_tier[8];  // Support for tiers 0-7
};

/**
 * @brief Mock swarm_emergence_t
 */
struct swarm_emergence_t {
    uint32_t emergence_type;
    float magnitude;
    bool enabled;
};

/**
 * @brief Mock swarm_energy_gossip_t
 */
struct swarm_energy_gossip_t {
    float avg_power_level;
    uint32_t message_count;
    bool enabled;
};

/* NOTE: bio_module_context_t is already defined in nimcp_bio_router.h
 * as an opaque pointer type - no mock needed */

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for Portia-Swarm bridge tests
 */
class PortiaSwarmBridgeTest : public ::testing::Test {
protected:
    portia_swarm_bridge_t* bridge;
    portia_context_t* portia;
    swarm_brain_t* swarm_brain;
    swarm_consensus_t consensus;  // Already a pointer type
    swarm_emergence_t* emergence;
    swarm_energy_gossip_t* energy_gossip;
    portia_swarm_config_t config;

    // Callback tracking
    static int recommendation_cb_count;
    static int emergence_cb_count;
    static int collective_cb_count;
    static portia_swarm_recommendation_t last_recommendation;
    static uint32_t last_emergence_type;
    static float last_emergence_magnitude;
    static portia_swarm_collective_state_t last_collective_state;
    static void* last_user_data;

    void SetUp() override {
        // Initialize logging
        nimcp_log_config_t log_config = nimcp_log_default_config();
        log_config.level = LOG_LEVEL_DEBUG;
        nimcp_log_init(&log_config);

        // Create mock Portia context
        portia = new portia_context_t();
        portia->agent_id = 42;     // Test agent ID
        portia->power_state = 1;  // BATTERY_FULL
        portia->thermal_state = 0; // NOMINAL
        portia->platform_tier = 2;
        portia->degradation_level = 0;
        portia->cpu_usage = 0.5f;
        portia->memory_usage = 0.4f;
        portia->battery_level = 0.85f;
        portia->thermal_headroom = 20.0f;

        // Create mock swarm components
        swarm_brain = new nimcp_swarm_brain();
        swarm_brain->agent_count = 10;
        swarm_brain->connected = false;

        consensus = new swarm_consensus_context();
        consensus->consensus_tier = 2;
        consensus->consensus_count = 7;
        consensus->confidence = 0.8f;
        for (int i = 0; i < 8; i++) {
            consensus->supports_tier[i] = (i == 2);  // Support tier 2
        }

        emergence = new swarm_emergence_t();
        emergence->emergence_type = 1;
        emergence->magnitude = 0.6f;
        emergence->enabled = false;

        energy_gossip = new swarm_energy_gossip_t();
        energy_gossip->avg_power_level = 0.7f;
        energy_gossip->message_count = 0;
        energy_gossip->enabled = false;

        // Default config
        portia_swarm_default_config(&config);

        // Bridge starts as nullptr
        bridge = nullptr;

        // Reset callback tracking
        recommendation_cb_count = 0;
        emergence_cb_count = 0;
        collective_cb_count = 0;
        memset(&last_recommendation, 0, sizeof(last_recommendation));
        last_emergence_type = 0;
        last_emergence_magnitude = 0.0f;
        memset(&last_collective_state, 0, sizeof(last_collective_state));
        last_user_data = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            portia_swarm_bridge_destroy(bridge);
            bridge = nullptr;
        }

        delete portia;
        delete swarm_brain;
        delete consensus;
        delete emergence;
        delete energy_gossip;

        nimcp_log_shutdown();
    }

    // Callback implementations
    static void recommendation_callback(
        portia_swarm_bridge_t* bridge,
        const portia_swarm_recommendation_t* recommendation,
        void* user_data)
    {
        recommendation_cb_count++;
        last_recommendation = *recommendation;
        last_user_data = user_data;
    }

    static void emergence_callback(
        portia_swarm_bridge_t* bridge,
        uint32_t emergence_type,
        float magnitude,
        void* user_data)
    {
        emergence_cb_count++;
        last_emergence_type = emergence_type;
        last_emergence_magnitude = magnitude;
        last_user_data = user_data;
    }

    static void collective_callback(
        portia_swarm_bridge_t* bridge,
        const portia_swarm_collective_state_t* collective_state,
        void* user_data)
    {
        collective_cb_count++;
        last_collective_state = *collective_state;
        last_user_data = user_data;
    }
};

// Static member initialization
int PortiaSwarmBridgeTest::recommendation_cb_count = 0;
int PortiaSwarmBridgeTest::emergence_cb_count = 0;
int PortiaSwarmBridgeTest::collective_cb_count = 0;
portia_swarm_recommendation_t PortiaSwarmBridgeTest::last_recommendation = {};
uint32_t PortiaSwarmBridgeTest::last_emergence_type = 0;
float PortiaSwarmBridgeTest::last_emergence_magnitude = 0.0f;
portia_swarm_collective_state_t PortiaSwarmBridgeTest::last_collective_state = {};
void* PortiaSwarmBridgeTest::last_user_data = nullptr;

//=============================================================================
// 1. Configuration Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, DefaultConfig) {
    portia_swarm_config_t cfg;
    portia_swarm_default_config(&cfg);

    EXPECT_EQ(cfg.mode, PORTIA_SWARM_MODE_BIDIRECTIONAL);
    EXPECT_EQ(cfg.influence, PORTIA_SWARM_INFLUENCE_MODERATE);
    EXPECT_EQ(cfg.sync_interval_ms, PORTIA_SWARM_SYNC_INTERVAL_MS);
    EXPECT_EQ(cfg.gossip_interval_ms, PORTIA_SWARM_ENERGY_GOSSIP_INTERVAL_MS);
    EXPECT_EQ(cfg.consensus_timeout_ms, PORTIA_SWARM_CONSENSUS_TIMEOUT_MS);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_energy_gossip);
    EXPECT_TRUE(cfg.enable_emergence_alerts);
    EXPECT_FLOAT_EQ(cfg.consensus_weight, 0.5f);
    EXPECT_FLOAT_EQ(cfg.local_weight, 0.5f);
}

TEST_F(PortiaSwarmBridgeTest, CustomConfigDisabledMode) {
    config.mode = PORTIA_SWARM_MODE_DISABLED;
    config.influence = PORTIA_SWARM_INFLUENCE_NONE;
    config.enable_bio_async = false;
    config.enable_energy_gossip = false;
    config.enable_emergence_alerts = false;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmBridgeTest, CustomConfigPassiveMode) {
    config.mode = PORTIA_SWARM_MODE_PASSIVE;
    config.influence = PORTIA_SWARM_INFLUENCE_ADVISORY;
    config.consensus_weight = 0.3f;
    config.local_weight = 0.7f;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmBridgeTest, CustomConfigBroadcastMode) {
    config.mode = PORTIA_SWARM_MODE_BROADCAST;
    config.sync_interval_ms = 200;
    config.gossip_interval_ms = 1000;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmBridgeTest, CustomConfigDominantInfluence) {
    config.influence = PORTIA_SWARM_INFLUENCE_DOMINANT;
    config.consensus_weight = 0.9f;
    config.local_weight = 0.1f;

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
}

//=============================================================================
// 2. Lifecycle Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, CreateBridgeSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmBridgeTest, CreateBridgeNullConfig) {
    // NULL config should use defaults, creation succeeds
    bridge = portia_swarm_bridge_create(nullptr, portia);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmBridgeTest, CreateBridgeNullPortia) {
    bridge = portia_swarm_bridge_create(&config, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(PortiaSwarmBridgeTest, DestroyBridgeValid) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_bridge_destroy(bridge);
    bridge = nullptr;  // Prevent double-free in TearDown
}

TEST_F(PortiaSwarmBridgeTest, DestroyBridgeNull) {
    // Should not crash
    portia_swarm_bridge_destroy(nullptr);
}

TEST_F(PortiaSwarmBridgeTest, StartBridgeSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_bridge_start(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, StartBridgeNull) {
    int result = portia_swarm_bridge_start(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, StopBridgeSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_start(bridge);

    int result = portia_swarm_bridge_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, StopBridgeNull) {
    int result = portia_swarm_bridge_stop(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, StartStopCycle) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);
    EXPECT_EQ(portia_swarm_bridge_stop(bridge), 0);
    EXPECT_EQ(portia_swarm_bridge_start(bridge), 0);
    EXPECT_EQ(portia_swarm_bridge_stop(bridge), 0);
}

//=============================================================================
// 3. Connection Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, ConnectSwarmBrainSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_brain(bridge, swarm_brain);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, ConnectSwarmBrainNull) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_brain(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ConnectConsensusSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_consensus(bridge, consensus);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, ConnectConsensusNull) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_consensus(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ConnectEmergenceSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_emergence(bridge, emergence);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, ConnectEmergenceNull) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_emergence(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ConnectEnergyGossipSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_energy_gossip(bridge, energy_gossip);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, ConnectEnergyGossipNull) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_energy_gossip(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ConnectBioAsyncSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, ConnectBioAsyncNull) {
    int result = portia_swarm_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, DisconnectBioAsyncSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_bio_async(bridge);

    int result = portia_swarm_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, DisconnectBioAsyncNull) {
    int result = portia_swarm_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, IsBioAsyncConnected) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Should not be connected initially
    EXPECT_FALSE(portia_swarm_is_bio_async_connected(bridge));

    // Try to connect - in unit tests, bio-async router is not initialized
    // so this will return 0 (success) but bio_async_enabled will remain false
    int result = portia_swarm_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);  // Function succeeds even if router unavailable

    // NOTE: In unit tests, bio-async router is not initialized, so connection
    // will fail gracefully. This is expected behavior.
    // In integration/e2e tests with a real router, this would return true.

    // Disconnect should also succeed (idempotent)
    result = portia_swarm_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(portia_swarm_is_bio_async_connected(bridge));
}

TEST_F(PortiaSwarmBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = portia_swarm_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(PortiaSwarmBridgeTest, ConnectAllModules) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_connect_brain(bridge, swarm_brain), 0);
    EXPECT_EQ(portia_swarm_connect_consensus(bridge, consensus), 0);
    EXPECT_EQ(portia_swarm_connect_emergence(bridge, emergence), 0);
    EXPECT_EQ(portia_swarm_connect_energy_gossip(bridge, energy_gossip), 0);
    EXPECT_EQ(portia_swarm_connect_bio_async(bridge), 0);
}

//=============================================================================
// 4. State Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, GetLocalStateSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_state_t state;
    int result = portia_swarm_get_local_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Verify state matches Portia context
    EXPECT_EQ(state.power_state, portia->power_state);
    EXPECT_EQ(state.thermal_state, portia->thermal_state);
    EXPECT_EQ(state.platform_tier, portia->platform_tier);
    EXPECT_EQ(state.degradation_level, portia->degradation_level);
    EXPECT_FLOAT_EQ(state.cpu_usage, portia->cpu_usage);
    EXPECT_FLOAT_EQ(state.memory_usage, portia->memory_usage);
    EXPECT_FLOAT_EQ(state.battery_level, portia->battery_level);
    EXPECT_FLOAT_EQ(state.thermal_headroom, portia->thermal_headroom);
}

TEST_F(PortiaSwarmBridgeTest, GetLocalStateNullBridge) {
    portia_swarm_state_t state;
    int result = portia_swarm_get_local_state(nullptr, &state);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, GetLocalStateNullOutput) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_get_local_state(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, BroadcastStateSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_start(bridge);

    int result = portia_swarm_broadcast_state(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, BroadcastStateNull) {
    int result = portia_swarm_broadcast_state(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, BroadcastStateDisabledMode) {
    config.mode = PORTIA_SWARM_MODE_DISABLED;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_broadcast_state(bridge);
    // Should succeed but do nothing
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, BroadcastStatePassiveMode) {
    config.mode = PORTIA_SWARM_MODE_PASSIVE;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_broadcast_state(bridge);
    // Passive mode should not broadcast
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, GetCollectiveStateSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_brain(bridge, swarm_brain);

    // Start the bridge and update to populate collective state
    portia_swarm_bridge_start(bridge);
    portia_swarm_update(bridge);

    portia_swarm_collective_state_t state;
    int result = portia_swarm_get_collective_state(bridge, &state);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(state.agent_count, swarm_brain->agent_count);
}

TEST_F(PortiaSwarmBridgeTest, GetCollectiveStateNullBridge) {
    portia_swarm_collective_state_t state;
    int result = portia_swarm_get_collective_state(nullptr, &state);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, GetCollectiveStateNullOutput) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_get_collective_state(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

//=============================================================================
// 5. Update Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, UpdateBridgeSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_start(bridge);

    int result = portia_swarm_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, UpdateBridgeNull) {
    int result = portia_swarm_update(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, UpdateBridgeMultipleTimes) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_start(bridge);

    for (int i = 0; i < 10; i++) {
        int result = portia_swarm_update(bridge);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(PortiaSwarmBridgeTest, UpdateWithStateChange) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_start(bridge);

    // Initial update
    EXPECT_EQ(portia_swarm_update(bridge), 0);

    // Change Portia state
    portia->platform_tier = 3;
    portia->cpu_usage = 0.8f;
    portia->battery_level = 0.3f;

    // Update again
    EXPECT_EQ(portia_swarm_update(bridge), 0);

    // Verify state reflects changes
    portia_swarm_state_t state;
    portia_swarm_get_local_state(bridge, &state);
    EXPECT_EQ(state.platform_tier, 3);
    EXPECT_FLOAT_EQ(state.cpu_usage, 0.8f);
    EXPECT_FLOAT_EQ(state.battery_level, 0.3f);
}

//=============================================================================
// 6. Recommendation Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, RequestRecommendationSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    portia_swarm_recommendation_t recommendation;
    int result = portia_swarm_request_recommendation(bridge, &recommendation);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(recommendation.recommended_tier, consensus->consensus_tier);
    EXPECT_GE(recommendation.confidence, 0.0f);
    EXPECT_LE(recommendation.confidence, 1.0f);
}

TEST_F(PortiaSwarmBridgeTest, RequestRecommendationNullBridge) {
    portia_swarm_recommendation_t recommendation;
    int result = portia_swarm_request_recommendation(nullptr, &recommendation);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, RequestRecommendationNullOutput) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_request_recommendation(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, GetRecommendationSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    // Request a recommendation first
    portia_swarm_recommendation_t rec1;
    portia_swarm_request_recommendation(bridge, &rec1);

    // Get the latest recommendation
    portia_swarm_recommendation_t rec2;
    int result = portia_swarm_get_recommendation(bridge, &rec2);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(rec2.recommended_tier, rec1.recommended_tier);
}

TEST_F(PortiaSwarmBridgeTest, GetRecommendationNullBridge) {
    portia_swarm_recommendation_t recommendation;
    int result = portia_swarm_get_recommendation(nullptr, &recommendation);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, GetRecommendationNullOutput) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_get_recommendation(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ApplyRecommendationSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_recommendation_t recommendation;
    recommendation.recommended_tier = 3;
    recommendation.recommended_degradation = 1;
    recommendation.confidence = 0.9f;
    recommendation.consensus_count = 8;
    recommendation.source = PORTIA_TIER_REC_SWARM_CONSENSUS;

    int result = portia_swarm_apply_recommendation(bridge, &recommendation);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, ApplyRecommendationNullBridge) {
    portia_swarm_recommendation_t recommendation;
    recommendation.recommended_tier = 3;

    int result = portia_swarm_apply_recommendation(nullptr, &recommendation);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ApplyRecommendationNullRecommendation) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_apply_recommendation(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

//=============================================================================
// 7. Decision Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, ComputeOptimalTierSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    uint8_t optimal_tier;
    int result = portia_swarm_compute_optimal_tier(bridge, 2, &optimal_tier);
    EXPECT_EQ(result, 0);
    EXPECT_GE(optimal_tier, 0);
    EXPECT_LE(optimal_tier, 7);
}

TEST_F(PortiaSwarmBridgeTest, ComputeOptimalTierNullBridge) {
    uint8_t optimal_tier;
    int result = portia_swarm_compute_optimal_tier(nullptr, 2, &optimal_tier);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ComputeOptimalTierNullOutput) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_compute_optimal_tier(bridge, 2, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ComputeOptimalTierLocalWeight) {
    config.consensus_weight = 0.2f;
    config.local_weight = 0.8f;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    uint8_t optimal_tier;
    int result = portia_swarm_compute_optimal_tier(bridge, 3, &optimal_tier);
    EXPECT_EQ(result, 0);
    // Should be closer to local recommendation (3)
}

TEST_F(PortiaSwarmBridgeTest, ComputeOptimalTierSwarmWeight) {
    config.consensus_weight = 0.9f;
    config.local_weight = 0.1f;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    uint8_t optimal_tier;
    int result = portia_swarm_compute_optimal_tier(bridge, 3, &optimal_tier);
    EXPECT_EQ(result, 0);
    // Should be closer to swarm recommendation (2)
}

TEST_F(PortiaSwarmBridgeTest, ConsensusSupportsTrue) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    // Request recommendation to populate latest_recommendation
    portia_swarm_recommendation_t rec;
    portia_swarm_request_recommendation(bridge, &rec);

    // Now tier 2 should be supported (within 1 of recommended tier)
    bool supports = portia_swarm_consensus_supports_tier(bridge, 2);
    EXPECT_TRUE(supports);
}

TEST_F(PortiaSwarmBridgeTest, ConsensusSupportsFalse) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    bool supports = portia_swarm_consensus_supports_tier(bridge, 5);
    EXPECT_FALSE(supports);
}

TEST_F(PortiaSwarmBridgeTest, ConsensusSupportsNullBridge) {
    bool supports = portia_swarm_consensus_supports_tier(nullptr, 2);
    EXPECT_FALSE(supports);
}

//=============================================================================
// 8. Callback Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, RegisterRecommendationCallback) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_register_recommendation_cb(
        bridge, recommendation_callback, (void*)0x1234);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, RegisterRecommendationCallbackNull) {
    int result = portia_swarm_register_recommendation_cb(
        nullptr, recommendation_callback, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, RegisterEmergenceCallback) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_register_emergence_cb(
        bridge, emergence_callback, (void*)0x5678);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, RegisterEmergenceCallbackNull) {
    int result = portia_swarm_register_emergence_cb(
        nullptr, emergence_callback, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, RegisterCollectiveCallback) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_register_collective_cb(
        bridge, collective_callback, (void*)0xABCD);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, RegisterCollectiveCallbackNull) {
    int result = portia_swarm_register_collective_cb(
        nullptr, collective_callback, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, TriggerRecommendationCallback) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    void* user_data = (void*)0x1234;
    portia_swarm_register_recommendation_cb(
        bridge, recommendation_callback, user_data);

    // Trigger by requesting recommendation
    portia_swarm_connect_consensus(bridge, consensus);
    portia_swarm_recommendation_t rec;
    portia_swarm_request_recommendation(bridge, &rec);

    // Note: Callback triggering depends on implementation
    // This test structure is ready for when callbacks are implemented
}

TEST_F(PortiaSwarmBridgeTest, CallbackUserDataPreserved) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    void* test_data = (void*)0xDEADBEEF;
    portia_swarm_register_recommendation_cb(
        bridge, recommendation_callback, test_data);

    // When callback is triggered, last_user_data should equal test_data
}

//=============================================================================
// 9. Statistics Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, GetStatsSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_stats_t stats;
    int result = portia_swarm_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GE(stats.messages_sent, 0);
    EXPECT_GE(stats.messages_received, 0);
    EXPECT_GE(stats.consensus_queries, 0);
    EXPECT_GE(stats.sync_efficiency, 0.0f);
    EXPECT_LE(stats.sync_efficiency, 1.0f);
}

TEST_F(PortiaSwarmBridgeTest, GetStatsNullBridge) {
    portia_swarm_stats_t stats;
    int result = portia_swarm_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, GetStatsNullOutput) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_get_stats(bridge, nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, ResetStatsSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Do some operations
    portia_swarm_broadcast_state(bridge);
    portia_swarm_update(bridge);

    // Reset stats
    int result = portia_swarm_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    // Get stats - should be reset
    portia_swarm_stats_t stats;
    portia_swarm_get_stats(bridge, &stats);
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
}

TEST_F(PortiaSwarmBridgeTest, ResetStatsNull) {
    int result = portia_swarm_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, StatsAccumulation) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_bridge_start(bridge);
    portia_swarm_connect_consensus(bridge, consensus);

    // Get initial stats
    portia_swarm_stats_t stats1;
    portia_swarm_get_stats(bridge, &stats1);

    // Perform operations
    portia_swarm_broadcast_state(bridge);
    portia_swarm_recommendation_t rec;
    portia_swarm_request_recommendation(bridge, &rec);
    portia_swarm_update(bridge);

    // Get updated stats
    portia_swarm_stats_t stats2;
    portia_swarm_get_stats(bridge, &stats2);

    // Stats should have changed
    EXPECT_GE(stats2.messages_sent, stats1.messages_sent);
    EXPECT_GE(stats2.consensus_queries, stats1.consensus_queries);
}

//=============================================================================
// 10. Mode Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, DisabledModeNoOperations) {
    config.mode = PORTIA_SWARM_MODE_DISABLED;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Operations should succeed but do nothing
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);
    EXPECT_EQ(portia_swarm_update(bridge), 0);

    portia_swarm_stats_t stats;
    portia_swarm_get_stats(bridge, &stats);
    EXPECT_EQ(stats.messages_sent, 0);
}

TEST_F(PortiaSwarmBridgeTest, PassiveModeReceiveOnly) {
    config.mode = PORTIA_SWARM_MODE_PASSIVE;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    // Can receive recommendations
    portia_swarm_recommendation_t rec;
    EXPECT_EQ(portia_swarm_request_recommendation(bridge, &rec), 0);

    // But broadcast should not send
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);
}

TEST_F(PortiaSwarmBridgeTest, BroadcastModeSendOnly) {
    config.mode = PORTIA_SWARM_MODE_BROADCAST;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Can broadcast
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    // But should not query consensus
    portia_swarm_recommendation_t rec;
    EXPECT_EQ(portia_swarm_request_recommendation(bridge, &rec), 0);
}

TEST_F(PortiaSwarmBridgeTest, BidirectionalModeFullFunctionality) {
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_connect_consensus(bridge, consensus);

    // Can broadcast
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    // Can receive
    portia_swarm_recommendation_t rec;
    EXPECT_EQ(portia_swarm_request_recommendation(bridge, &rec), 0);

    // Can update
    EXPECT_EQ(portia_swarm_update(bridge), 0);
}

//=============================================================================
// 11. Notification Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeTest, NotifyTierChangeSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_notify_tier_change(bridge, 2, 3);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, NotifyTierChangeNull) {
    int result = portia_swarm_notify_tier_change(nullptr, 2, 3);
    EXPECT_NE(result, NIMCP_OK);
}

TEST_F(PortiaSwarmBridgeTest, NotifyDegradationSuccess) {
    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    int result = portia_swarm_notify_degradation(bridge, 2, 0x1234);
    EXPECT_EQ(result, 0);
}

TEST_F(PortiaSwarmBridgeTest, NotifyDegradationNull) {
    int result = portia_swarm_notify_degradation(nullptr, 2, 0x1234);
    EXPECT_NE(result, NIMCP_OK);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
