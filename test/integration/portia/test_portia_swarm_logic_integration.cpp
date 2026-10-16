//=============================================================================
// test_portia_swarm_logic_integration.cpp - Integration Tests
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

class PortiaSwarmLogicIntegrationTest : public ::testing::Test {
protected:
    portia_swarm_logic_bridge_t* bridge;
    portia_swarm_logic_config_t config;

    void SetUp() override {
        portia_swarm_logic_default_config(&config);
        config.enable_bio_async = false;  // Disable for controlled integration tests
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
// Full Pipeline Integration Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicIntegrationTest, FullDecisionPipeline) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect all integrations
    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    // Start bridge
    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Make all types of decisions
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 10, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 20, 0.5f, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);

    // Verify statistics
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 4);
}

TEST_F(PortiaSwarmLogicIntegrationTest, ModeTransitions) {
    portia_swarm_logic_mode_t modes[] = {
        PSL_MODE_PORTIA_ONLY,
        PSL_MODE_COORDINATED,
        PSL_MODE_SWARM_ONLY,
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

TEST_F(PortiaSwarmLogicIntegrationTest, WeightedDecisionBalancing) {
    // Test different weight configurations
    float weight_pairs[][2] = {
        {1.0f, 0.0f},  // Local only
        {0.0f, 1.0f},  // Collective only
        {0.5f, 0.5f},  // Balanced
        {0.7f, 0.3f},  // Local-biased
        {0.3f, 0.7f}   // Collective-biased
    };

    for (auto& weights : weight_pairs) {
        config.local_weight = weights[0];
        config.collective_weight = weights[1];

        bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        portia_swarm_logic_start(bridge);

        unified_decision_result_t result;
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);

        portia_swarm_logic_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, ConfidenceThresholdEnforcement) {
    config.confidence_threshold = 0.9f;  // High threshold
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);

    // With high threshold, decisions may be denied due to low confidence
    if (!result.approved) {
        EXPECT_LT(result.confidence, config.confidence_threshold);
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, ConsensusTimeoutHandling) {
    config.consensus_timeout_ms = 100;  // Short timeout
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);

    // Should complete within reasonable time even with short timeout
    EXPECT_LT(result.decision_time_us, 1000000);  // < 1 second
}

//=============================================================================
// Cross-Module Coordination Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicIntegrationTest, TierChangeCoordination) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Test coordinated tier changes
    unified_decision_result_t result;
    uint8_t current_tier = 0;
    for (uint8_t proposed = 1; proposed <= 3; proposed++) {
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, current_tier, proposed, &result), 0);

        if (result.approved) {
            current_tier = proposed;
        }
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, DegradationCoordination) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Test coordinated degradation across multiple features
    for (uint32_t feature_id = 0; feature_id < 5; feature_id++) {
        unified_decision_result_t result;
        EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, feature_id, &result), 0);

        // OR logic: Either local or swarm can trigger degradation
        if (result.local_approved || result.swarm_approved) {
            EXPECT_TRUE(result.approved);
        }
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, ResourceAllocationCoordination) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Test resource allocation with varying amounts
    float amounts[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    for (float amount : amounts) {
        unified_decision_result_t result;
        EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 100, amount, &result), 0);

        // AND logic: Both local and swarm must approve in coordinated mode
        if (config.mode == PSL_MODE_COORDINATED) {
            if (result.approved) {
                EXPECT_TRUE(result.local_approved);
                EXPECT_TRUE(result.swarm_approved);
            }
        }
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, EmergencyEscalationPath) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Test emergency mode detection and escalation
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);

    if (result.approved) {
        EXPECT_GT(stats.emergency_activations, 0);
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, MultiDecisionSequence) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Simulate realistic decision sequence
    unified_decision_result_t result;

    // 1. Check resources
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 1, 0.6f, &result), 0);

    // 2. Potentially change tier
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);

    // 3. Check for degradation needs
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 5, &result), 0);

    // 4. Check emergency status
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 4);
}

//=============================================================================
// Logic Gate Integration Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicIntegrationTest, CustomGateCreationAndEvaluation) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create multiple custom gates
    uint32_t and_gate, or_gate, implies_gate;
    EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, "A AND B", &and_gate), 0);
    EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, "A OR B", &or_gate), 0);
    EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, "A IMPLIES B", &implies_gate), 0);

    // Evaluate gates
    bool and_result = portia_swarm_logic_evaluate_unified_gate(bridge, and_gate);
    bool or_result = portia_swarm_logic_evaluate_unified_gate(bridge, or_gate);
    bool implies_result = portia_swarm_logic_evaluate_unified_gate(bridge, implies_gate);

    // Results should be deterministic (all true with default inputs)
    EXPECT_TRUE(and_result);
    EXPECT_TRUE(or_result);
    EXPECT_TRUE(implies_result);
}

TEST_F(PortiaSwarmLogicIntegrationTest, GateBasedDecisionLogic) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    portia_swarm_logic_start(bridge);

    // Create gate for resource decision: local_ok AND swarm_ok
    uint32_t resource_gate;
    EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, "A AND B", &resource_gate), 0);

    // Make resource decision
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 50, 0.4f, &result), 0);

    // In coordinated mode, both must approve (AND logic)
    if (config.mode == PSL_MODE_COORDINATED && result.approved) {
        EXPECT_TRUE(result.local_approved && result.swarm_approved);
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, MultipleGateEvaluations) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create several gates
    uint32_t gates[5];
    const char* expressions[] = {
        "A AND B",
        "A OR B",
        "A XOR B",
        "A IMPLIES B",
        "NOT A"
    };

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, expressions[i], &gates[i]), 0);
    }

    // Evaluate all gates multiple times
    for (int iter = 0; iter < 10; iter++) {
        for (int i = 0; i < 5; i++) {
            bool result = portia_swarm_logic_evaluate_unified_gate(bridge, gates[i]);
            // Just verify it doesn't crash
        }
    }
}

TEST_F(PortiaSwarmLogicIntegrationTest, GatePerformanceUnderLoad) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create many gates
    const int num_gates = 50;
    uint32_t gates[num_gates];

    for (int i = 0; i < num_gates; i++) {
        const char* expr = (i % 2 == 0) ? "A AND B" : "A OR B";
        EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, expr, &gates[i]), 0);
    }

    // Evaluate all gates
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_gates; i++) {
        portia_swarm_logic_evaluate_unified_gate(bridge, gates[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in reasonable time (< 100ms for 50 gates)
    EXPECT_LT(duration.count(), 100000);
}

TEST_F(PortiaSwarmLogicIntegrationTest, GateErrorHandling) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Try invalid expressions
    uint32_t gate_id;
    EXPECT_NE(portia_swarm_logic_add_unified_gate(bridge, "INVALID EXPR", &gate_id), 0);
    EXPECT_NE(portia_swarm_logic_add_unified_gate(bridge, "", &gate_id), 0);

    // Try evaluating non-existent gate
    bool result = portia_swarm_logic_evaluate_unified_gate(bridge, UINT32_MAX);
    EXPECT_FALSE(result);
}

//=============================================================================
// Integration with External Systems Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicIntegrationTest, BrainNeuromodulationIntegration) {
    config.enable_brain_integration = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect brain (NULL for testing)
    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);

    portia_swarm_logic_start(bridge);

    // Make decision with brain connected
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
}

TEST_F(PortiaSwarmLogicIntegrationTest, ImmuneFeedbackIntegration) {
    config.enable_immune_feedback = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect immune system (NULL for testing)
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);

    portia_swarm_logic_start(bridge);

    // Make decision with immune connected
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 15, &result), 0);
}

TEST_F(PortiaSwarmLogicIntegrationTest, UMMMemoryTrackingIntegration) {
    config.enable_umm_tracking = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect UMM (NULL for testing)
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    portia_swarm_logic_start(bridge);

    // Make decision with UMM connected
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 75, 0.8f, &result), 0);
}

TEST_F(PortiaSwarmLogicIntegrationTest, AllSystemsIntegration) {
    config.enable_brain_integration = true;
    config.enable_immune_feedback = true;
    config.enable_umm_tracking = true;

    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect all systems
    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    portia_swarm_logic_start(bridge);

    // Make all decision types
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 20, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 30, 0.6f, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);
}

TEST_F(PortiaSwarmLogicIntegrationTest, SystemDisconnectionAndReconnection) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect systems
    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    portia_swarm_logic_start(bridge);

    // Make decision
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);

    // Reconnect systems (should be idempotent)
    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    // Make another decision
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 1, 2, &result), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
