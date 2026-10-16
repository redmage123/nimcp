//=============================================================================
// test_portia_swarm_logic_bridge.cpp - Unit Tests for Unified Bridge
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaSwarmLogicBridgeTest : public ::testing::Test {
protected:
    portia_swarm_logic_bridge_t* bridge;
    portia_swarm_logic_config_t config;

    void SetUp() override {
        portia_swarm_logic_default_config(&config);
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            portia_swarm_logic_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, DefaultConfigValid) {
    portia_swarm_logic_config_t cfg;
    portia_swarm_logic_default_config(&cfg);

    EXPECT_EQ(cfg.mode, PSL_MODE_COORDINATED);
    EXPECT_FLOAT_EQ(cfg.local_weight, 0.5f);
    EXPECT_FLOAT_EQ(cfg.collective_weight, 0.5f);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_FALSE(cfg.enable_brain_integration);
    EXPECT_FALSE(cfg.enable_immune_feedback);
    EXPECT_FALSE(cfg.enable_umm_tracking);
    EXPECT_EQ(cfg.consensus_timeout_ms, PSL_DEFAULT_CONSENSUS_TIMEOUT_MS);
    EXPECT_FLOAT_EQ(cfg.confidence_threshold, PSL_DEFAULT_CONFIDENCE_THRESHOLD);
}

TEST_F(PortiaSwarmLogicBridgeTest, DefaultConfigNullSafe) {
    portia_swarm_logic_default_config(nullptr);
    // Should not crash
}

TEST_F(PortiaSwarmLogicBridgeTest, CustomConfigWeights) {
    config.local_weight = 0.7f;
    config.collective_weight = 0.3f;
    config.confidence_threshold = 0.8f;

    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Verify by making a decision
    unified_decision_result_t result;
    int ret = portia_swarm_logic_start(bridge);
    EXPECT_EQ(ret, 0);

    ret = portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, AllOperationModes) {
    portia_swarm_logic_mode_t modes[] = {
        PSL_MODE_DISABLED,
        PSL_MODE_PORTIA_ONLY,
        PSL_MODE_SWARM_ONLY,
        PSL_MODE_COORDINATED,
        PSL_MODE_CONSENSUS_REQUIRED
    };

    for (auto mode : modes) {
        config.mode = mode;
        bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        portia_swarm_logic_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(PortiaSwarmLogicBridgeTest, ConfigWeightValidation) {
    // Valid: weights sum to 1.0
    config.local_weight = 0.6f;
    config.collective_weight = 0.4f;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    EXPECT_NE(bridge, nullptr);
}

//=============================================================================
// Lifecycle Tests (10 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, CreateDestroy) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_destroy(bridge);
    bridge = nullptr;  // Prevent double-free
}

TEST_F(PortiaSwarmLogicBridgeTest, CreateWithNullConfig) {
    bridge = portia_swarm_logic_create(nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmLogicBridgeTest, CreateWithAllBridges) {
    // Create with all bridge pointers as NULL (valid)
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(PortiaSwarmLogicBridgeTest, DestroyNullSafe) {
    portia_swarm_logic_destroy(nullptr);
    // Should not crash
}

TEST_F(PortiaSwarmLogicBridgeTest, StartStop) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = portia_swarm_logic_start(bridge);
    EXPECT_EQ(ret, 0);

    ret = portia_swarm_logic_stop(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, StartTwice) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = portia_swarm_logic_start(bridge);
    EXPECT_EQ(ret, 0);

    ret = portia_swarm_logic_start(bridge);
    EXPECT_EQ(ret, 0);  // Should succeed (idempotent)
}

TEST_F(PortiaSwarmLogicBridgeTest, StopTwice) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    int ret = portia_swarm_logic_stop(bridge);
    EXPECT_EQ(ret, 0);

    ret = portia_swarm_logic_stop(bridge);
    EXPECT_EQ(ret, 0);  // Should succeed (idempotent)
}

TEST_F(PortiaSwarmLogicBridgeTest, StartNullBridge) {
    int ret = portia_swarm_logic_start(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, StopNullBridge) {
    int ret = portia_swarm_logic_stop(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, StartStopCycle) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(portia_swarm_logic_start(bridge), 0);
        EXPECT_EQ(portia_swarm_logic_stop(bridge), 0);
    }
}

//=============================================================================
// Integration Tests (8 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, ConnectBrain) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect with NULL brain (valid for testing)
    int ret = portia_swarm_logic_connect_brain(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConnectBrainNull) {
    int ret = portia_swarm_logic_connect_brain(nullptr, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConnectImmune) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = portia_swarm_logic_connect_immune(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConnectImmuneNull) {
    int ret = portia_swarm_logic_connect_immune(nullptr, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConnectUMM) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = portia_swarm_logic_connect_umm(bridge, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConnectUMMNull) {
    int ret = portia_swarm_logic_connect_umm(nullptr, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConnectAllIntegrations) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, MultipleConnectCalls) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect multiple times (should be idempotent)
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
        EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
        EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);
    }
}

//=============================================================================
// Decision API Tests (12 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, DecideTierChangeBasic) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    int ret = portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GT(result.decision_time_us, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideTierChangeNotStarted) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    unified_decision_result_t result;
    int ret = portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_STATE);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideTierChangeNull) {
    int ret = portia_swarm_logic_decide_tier_change(nullptr, 0, 1, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideDegradationBasic) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    int ret = portia_swarm_logic_decide_degradation(bridge, 42, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideDegradationNull) {
    int ret = portia_swarm_logic_decide_degradation(nullptr, 42, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideResourceAllocationBasic) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    int ret = portia_swarm_logic_decide_resource_allocation(bridge, 100, 0.5f, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideResourceAllocationInvalidAmount) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    int ret = portia_swarm_logic_decide_resource_allocation(bridge, 100, 1.5f, &result);
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideResourceAllocationNull) {
    int ret = portia_swarm_logic_decide_resource_allocation(nullptr, 100, 0.5f, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideEmergencyModeBasic) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    int ret = portia_swarm_logic_decide_emergency_mode(bridge, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(PortiaSwarmLogicBridgeTest, DecideEmergencyModeNull) {
    int ret = portia_swarm_logic_decide_emergency_mode(nullptr, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, MultipleDecisions) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
        EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, i, &result), 0);
        EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, i, 0.3f, &result), 0);
        EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);
    }
}

TEST_F(PortiaSwarmLogicBridgeTest, DecisionModes) {
    portia_swarm_logic_mode_t modes[] = {
        PSL_MODE_PORTIA_ONLY,
        PSL_MODE_SWARM_ONLY,
        PSL_MODE_COORDINATED,
        PSL_MODE_CONSENSUS_REQUIRED
    };

    for (auto mode : modes) {
        config.mode = mode;
        bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        portia_swarm_logic_start(bridge);

        unified_decision_result_t result;
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);

        portia_swarm_logic_destroy(bridge);
        bridge = nullptr;
    }
}

//=============================================================================
// Custom Gate Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, AddUnifiedGateAND) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t gate_id;
    int ret = portia_swarm_logic_add_unified_gate(bridge, "A AND B", &gate_id);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(gate_id, UINT32_MAX);
}

TEST_F(PortiaSwarmLogicBridgeTest, AddUnifiedGateOR) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t gate_id;
    int ret = portia_swarm_logic_add_unified_gate(bridge, "A OR B", &gate_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, AddUnifiedGateInvalid) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t gate_id;
    int ret = portia_swarm_logic_add_unified_gate(bridge, "INVALID", &gate_id);
    EXPECT_NE(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, EvaluateUnifiedGate) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t gate_id;
    int ret = portia_swarm_logic_add_unified_gate(bridge, "A AND B", &gate_id);
    ASSERT_EQ(ret, 0);

    bool result = portia_swarm_logic_evaluate_unified_gate(bridge, gate_id);
    // Result can be true or false, just check it doesn't crash
}

TEST_F(PortiaSwarmLogicBridgeTest, AddMultipleGates) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    const char* expressions[] = {
        "A AND B",
        "A OR B",
        "A XOR B",
        "A IMPLIES B"
    };

    for (const char* expr : expressions) {
        uint32_t gate_id;
        int ret = portia_swarm_logic_add_unified_gate(bridge, expr, &gate_id);
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// Bio-Async Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, BioAsyncConnect) {
    config.enable_bio_async = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Bio-async connection is attempted in start()
    portia_swarm_logic_start(bridge);

    // Check connection state (may or may not succeed depending on router availability)
    // Just verify it doesn't crash
}

TEST_F(PortiaSwarmLogicBridgeTest, BioAsyncDisconnect) {
    config.enable_bio_async = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);
    int ret = portia_swarm_logic_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, BioAsyncIsConnected) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool connected = portia_swarm_logic_is_bio_async_connected(bridge);
    // Just check it doesn't crash
}

TEST_F(PortiaSwarmLogicBridgeTest, BioAsyncProcessInbox) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    int count = portia_swarm_logic_process_inbox(bridge);
    EXPECT_GE(count, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, BioAsyncNullPointer) {
    EXPECT_EQ(portia_swarm_logic_connect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(portia_swarm_logic_disconnect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(portia_swarm_logic_is_bio_async_connected(nullptr));
    EXPECT_EQ(portia_swarm_logic_process_inbox(nullptr), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Statistics Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, GetStatsInitial) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_stats_t stats;
    int ret = portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.total_decisions, 0);
    EXPECT_EQ(stats.local_decisions, 0);
    EXPECT_EQ(stats.collective_decisions, 0);
    EXPECT_EQ(stats.consensus_achieved, 0);
    EXPECT_EQ(stats.consensus_failed, 0);
    EXPECT_EQ(stats.emergency_activations, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, GetStatsAfterDecisions) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    portia_swarm_logic_decide_emergency_mode(bridge, &result);

    portia_swarm_logic_stats_t stats;
    int ret = portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(stats.total_decisions, 2);
    EXPECT_GT(stats.avg_decision_time_us, 0.0f);
}

TEST_F(PortiaSwarmLogicBridgeTest, ResetStats) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Make some decisions
    unified_decision_result_t result;
    portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);

    // Reset stats
    int ret = portia_swarm_logic_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    // Verify reset
    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 0);
}

TEST_F(PortiaSwarmLogicBridgeTest, StatsNullPointer) {
    EXPECT_EQ(portia_swarm_logic_get_stats(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(portia_swarm_logic_reset_stats(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PortiaSwarmLogicBridgeTest, StatsAccuracy) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Make exactly 5 decisions
    unified_decision_result_t result;
    for (int i = 0; i < 5; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    }

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 5);
}

//=============================================================================
// Thread Safety Tests (3 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicBridgeTest, ConcurrentDecisions) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    const int num_threads = 4;
    const int decisions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, decisions_per_thread]() {
            unified_decision_result_t result;
            for (int i = 0; i < decisions_per_thread; i++) {
                portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, num_threads * decisions_per_thread);
}

TEST_F(PortiaSwarmLogicBridgeTest, ConcurrentIntegrationConnections) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    const int num_threads = 3;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this]() {
            portia_swarm_logic_connect_brain(bridge, nullptr);
            portia_swarm_logic_connect_immune(bridge, nullptr);
            portia_swarm_logic_connect_umm(bridge, nullptr);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(PortiaSwarmLogicBridgeTest, ConcurrentStatsAccess) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    std::thread writer([this]() {
        unified_decision_result_t result;
        for (int i = 0; i < 50; i++) {
            portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
        }
    });

    std::thread reader([this]() {
        portia_swarm_logic_stats_t stats;
        for (int i = 0; i < 50; i++) {
            portia_swarm_logic_get_stats(bridge, &stats);
        }
    });

    writer.join();
    reader.join();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
