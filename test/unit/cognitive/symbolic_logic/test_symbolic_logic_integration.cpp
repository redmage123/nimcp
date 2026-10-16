/**
 * @file test_symbolic_logic_integration.cpp
 * @brief Unit tests for symbolic logic cognitive layer integration
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests for symbolic logic FEP orchestrator and cognitive hub integration
 * WHY:  Verify symbolic logic integrates properly with the full cognitive layer
 * HOW:  Test hub bridge, FEP registration, and inter-module communication
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/symbolic_logic/nimcp_symbolic_logic_hub_bridge.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_fep_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

//=============================================================================
// Hub Bridge Tests
//=============================================================================

class SymbolicLogicHubBridgeTest : public ::testing::Test {
protected:
    symbolic_logic_hub_bridge_t* bridge = nullptr;
    symbolic_logic_hub_config_t config;

    void SetUp() override {
        symbolic_logic_hub_bridge_default_config(&config);
        bridge = symbolic_logic_hub_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            symbolic_logic_hub_bridge_destroy(bridge);
        }
    }
};

TEST_F(SymbolicLogicHubBridgeTest, DefaultConfigInitialization) {
    symbolic_logic_hub_config_t cfg;
    int result = symbolic_logic_hub_bridge_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.subscribe_memory_access);
    EXPECT_TRUE(cfg.subscribe_attention_shift);
    EXPECT_TRUE(cfg.publish_inference_results);
    EXPECT_FLOAT_EQ(cfg.inference_priority, 0.7f);
}

TEST_F(SymbolicLogicHubBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SymbolicLogicHubBridgeTest, BridgeCreationWithNullConfig) {
    symbolic_logic_hub_bridge_t* b = symbolic_logic_hub_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    symbolic_logic_hub_bridge_destroy(b);
}

TEST_F(SymbolicLogicHubBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    symbolic_logic_hub_state_t state;
    int result = symbolic_logic_hub_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.is_registered);
    EXPECT_FALSE(state.is_active);
}

TEST_F(SymbolicLogicHubBridgeTest, GetStats) {
    ASSERT_NE(bridge, nullptr);

    symbolic_logic_hub_stats_t stats;
    int result = symbolic_logic_hub_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.events_received, 0u);
    EXPECT_EQ(stats.events_published, 0u);
}

TEST_F(SymbolicLogicHubBridgeTest, ResetStats) {
    ASSERT_NE(bridge, nullptr);

    int result = symbolic_logic_hub_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SymbolicLogicHubBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    int result = symbolic_logic_hub_bridge_force_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SymbolicLogicHubBridgeTest, NullSafety) {
    EXPECT_NE(symbolic_logic_hub_bridge_default_config(nullptr), 0);
    EXPECT_EQ(symbolic_logic_hub_bridge_get_state(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(symbolic_logic_hub_bridge_get_stats(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(symbolic_logic_hub_bridge_force_update(nullptr), NIMCP_ERROR_NULL_POINTER);

    // Destroy should be safe with NULL
    symbolic_logic_hub_bridge_destroy(nullptr);
}

//=============================================================================
// FEP Bridge Tests
//=============================================================================

class SymbolicLogicFEPBridgeTest : public ::testing::Test {
protected:
    symbolic_logic_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = symbolic_logic_fep_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) {
            symbolic_logic_fep_bridge_destroy(bridge);
        }
    }
};

TEST_F(SymbolicLogicFEPBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SymbolicLogicFEPBridgeTest, DefaultConfig) {
    symbolic_logic_fep_config_t config;
    int result = symbolic_logic_fep_bridge_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(config.pe_exploration_threshold, LOGIC_FEP_HIGH_PE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.proof_precision_factor, LOGIC_FEP_PROOF_PRECISION_FACTOR);
    EXPECT_TRUE(config.enable_pe_exploration);
    EXPECT_TRUE(config.enable_proof_validation);
}

TEST_F(SymbolicLogicFEPBridgeTest, TriggerExploration) {
    ASSERT_NE(bridge, nullptr);

    // Below threshold - should not trigger
    int result = symbolic_logic_fep_trigger_exploration(bridge, 2.0f);
    EXPECT_EQ(result, 0);

    // Above threshold - should trigger
    result = symbolic_logic_fep_trigger_exploration(bridge, 5.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(SymbolicLogicFEPBridgeTest, ValidateBeliefsByProof) {
    ASSERT_NE(bridge, nullptr);

    // Need FEP system connected for this
    int result = symbolic_logic_fep_validate_beliefs_by_proof(bridge);
    // Should fail without FEP connection
    EXPECT_NE(result, 0);
}

TEST_F(SymbolicLogicFEPBridgeTest, GetState) {
    ASSERT_NE(bridge, nullptr);

    symbolic_logic_fep_state_t state;
    int result = symbolic_logic_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(state.exploration_active);
}

TEST_F(SymbolicLogicFEPBridgeTest, GetStats) {
    ASSERT_NE(bridge, nullptr);

    symbolic_logic_fep_stats_t stats;
    int result = symbolic_logic_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.exploration_events, 0u);
}

TEST_F(SymbolicLogicFEPBridgeTest, UpdateWrapper) {
    ASSERT_NE(bridge, nullptr);

    int result = symbolic_logic_fep_bridge_update_wrapper(bridge);
    EXPECT_EQ(result, 0);

    result = symbolic_logic_fep_bridge_update_wrapper(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SymbolicLogicFEPBridgeTest, NullSafety) {
    EXPECT_NE(symbolic_logic_fep_bridge_default_config(nullptr), 0);
    /* FEP bridge functions return -1 for errors, not NIMCP error codes */
    EXPECT_EQ(symbolic_logic_fep_bridge_get_state(nullptr, nullptr), -1);
    EXPECT_EQ(symbolic_logic_fep_bridge_get_stats(nullptr, nullptr), -1);

    // Destroy should be safe with NULL
    symbolic_logic_fep_bridge_destroy(nullptr);
}

//=============================================================================
// Integration Tests
//=============================================================================

class SymbolicLogicIntegrationTest : public ::testing::Test {
protected:
    symbolic_logic_hub_bridge_t* hub_bridge = nullptr;
    symbolic_logic_fep_bridge_t* fep_bridge = nullptr;
    cognitive_integration_hub_t hub = nullptr;
    fep_orchestrator_t* orchestrator = nullptr;

    void SetUp() override {
        // Create hub bridge
        hub_bridge = symbolic_logic_hub_bridge_create(nullptr);

        // Create FEP bridge
        fep_bridge = symbolic_logic_fep_bridge_create(nullptr);

        // Create cognitive hub
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);

        // Create FEP orchestrator
        fep_orchestrator_config_t orch_config;
        fep_orchestrator_default_config(&orch_config);
        orchestrator = fep_orchestrator_create(&orch_config);
    }

    void TearDown() override {
        if (hub_bridge) symbolic_logic_hub_bridge_destroy(hub_bridge);
        if (fep_bridge) symbolic_logic_fep_bridge_destroy(fep_bridge);
        if (hub) cognitive_hub_destroy(hub);
        if (orchestrator) fep_orchestrator_destroy(orchestrator);
    }
};

TEST_F(SymbolicLogicIntegrationTest, HubBridgeConnectToHub) {
    ASSERT_NE(hub_bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = symbolic_logic_hub_bridge_connect_hub(hub_bridge, hub);
    EXPECT_EQ(result, 0);

    symbolic_logic_hub_state_t state;
    symbolic_logic_hub_bridge_get_state(hub_bridge, &state);
    EXPECT_TRUE(state.is_registered);
    EXPECT_TRUE(state.is_active);
}

TEST_F(SymbolicLogicIntegrationTest, FEPBridgeRegisterWithOrchestrator) {
    ASSERT_NE(fep_bridge, nullptr);
    ASSERT_NE(orchestrator, nullptr);

    uint32_t bridge_id = 0;
    int result = symbolic_logic_fep_bridge_register_with_orchestrator(
        fep_bridge, orchestrator, &bridge_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge_id, 0u);
}

TEST_F(SymbolicLogicIntegrationTest, BothBridgesActive) {
    ASSERT_NE(hub_bridge, nullptr);
    ASSERT_NE(fep_bridge, nullptr);
    ASSERT_NE(hub, nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Connect hub bridge
    int result = symbolic_logic_hub_bridge_connect_hub(hub_bridge, hub);
    EXPECT_EQ(result, 0);

    // Register FEP bridge
    uint32_t bridge_id = 0;
    result = symbolic_logic_fep_bridge_register_with_orchestrator(
        fep_bridge, orchestrator, &bridge_id);
    EXPECT_EQ(result, 0);

    // Both should be operational
    symbolic_logic_hub_state_t hub_state;
    symbolic_logic_hub_bridge_get_state(hub_bridge, &hub_state);
    EXPECT_TRUE(hub_state.is_active);

    symbolic_logic_fep_state_t fep_state;
    symbolic_logic_fep_bridge_get_state(fep_bridge, &fep_state);
    // FEP state should be valid (exploration not active by default)
    EXPECT_FALSE(fep_state.exploration_active);
}

TEST_F(SymbolicLogicIntegrationTest, ExplorationTriggerUpdatesStats) {
    ASSERT_NE(fep_bridge, nullptr);

    // Trigger exploration above threshold
    symbolic_logic_fep_trigger_exploration(fep_bridge, 5.5f);

    symbolic_logic_fep_stats_t stats;
    symbolic_logic_fep_bridge_get_stats(fep_bridge, &stats);
    EXPECT_GT(stats.exploration_events, 0u);
}
