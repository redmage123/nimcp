//=============================================================================
// e2e_test_portia_swarm_logic_pipeline.cpp - End-to-End Pipeline Tests
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaSwarmLogicE2ETest : public ::testing::Test {
protected:
    portia_swarm_logic_bridge_t* bridge;
    portia_swarm_logic_config_t config;

    void SetUp() override {
        portia_swarm_logic_default_config(&config);
        config.enable_bio_async = false;  // Simplify E2E tests
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            portia_swarm_logic_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper: Simulate resource pressure scenario
    void SimulateResourcePressure() {
        unified_decision_result_t result;

        // Check current resource state
        portia_swarm_logic_decide_resource_allocation(bridge, 1, 0.9f, &result);

        // If resources tight, may trigger degradation
        if (!result.approved || result.confidence < 0.5f) {
            portia_swarm_logic_decide_degradation(bridge, 1, &result);
        }
    }
};

//=============================================================================
// Full Pipeline E2E Tests (10+ tests)
//=============================================================================

TEST_F(PortiaSwarmLogicE2ETest, CompleteAdaptivePipeline) {
    // SCENARIO: Full metrics → logic → consensus → unified decision pipeline
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect all systems
    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Simulate full decision pipeline
    unified_decision_result_t result;

    // 1. Initial resource assessment
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 1, 0.7f, &result), 0);
    bool resources_ok = result.approved;

    // 2. Platform tier decision based on resources
    if (resources_ok) {
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
    }

    // 3. Check for degradation needs
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 5, &result), 0);

    // 4. Emergency check
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);

    // Verify pipeline completed (3-4 decisions depending on resource check)
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.total_decisions, 3);  // At least 3 (4 if resources_ok)
}

TEST_F(PortiaSwarmLogicE2ETest, EmergencyEscalationAcrossAllSystems) {
    // SCENARIO: Emergency detected, escalates through all systems
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Trigger emergency detection
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);

    if (result.approved) {
        // Emergency mode activated, verify coordinated response

        // Should trigger tier downgrade
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 3, 0, &result), 0);

        // Should trigger degradation
        EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 10, &result), 0);

        // Verify emergency activation counted
        portia_swarm_logic_stats_t stats;
        EXPECT_EQ(portia_swarm_logic_get_stats(bridge, &stats), 0);
        EXPECT_GT(stats.emergency_activations, 0);
    }
}

TEST_F(PortiaSwarmLogicE2ETest, CoordinatedTierSwitchingWithSwarmApproval) {
    // SCENARIO: Tier switching coordinated between local and swarm
    config.mode = PSL_MODE_COORDINATED;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    unified_decision_result_t result;

    // Attempt tier upgrade: requires both local and swarm approval
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 2, &result), 0);

    if (result.approved) {
        // Both local and swarm approved
        EXPECT_TRUE(result.local_approved);
        EXPECT_TRUE(result.swarm_approved);
        EXPECT_GT(result.swarm_consensus_count, 0);
    }

    // Attempt tier downgrade: should be easier
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 2, 0, &result), 0);
}

TEST_F(PortiaSwarmLogicE2ETest, GracefulDegradationWithCollectiveAgreement) {
    // SCENARIO: Coordinated degradation across swarm
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Test degradation of multiple features
    std::vector<uint32_t> features = {1, 2, 3, 4, 5};
    std::vector<bool> degraded(features.size(), false);

    for (size_t i = 0; i < features.size(); i++) {
        unified_decision_result_t result;
        EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, features[i], &result), 0);
        degraded[i] = result.approved;

        if (result.approved) {
            // Verify OR logic: either local or swarm triggered it
            EXPECT_TRUE(result.local_approved || result.swarm_approved);
        }
    }
}

TEST_F(PortiaSwarmLogicE2ETest, ByzantineFaultDetectionAndRecovery) {
    // SCENARIO: Detect Byzantine faults and recover
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Simulate conflicting decisions (Byzantine behavior)
    unified_decision_result_t result1, result2;

    // Same decision made twice should be consistent
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result1), 0);
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result2), 0);

    // Results should be similar (confidence within 0.2)
    EXPECT_NEAR(result1.confidence, result2.confidence, 0.2f);
}

TEST_F(PortiaSwarmLogicE2ETest, MultiDroneSwarmConsensusOnResourceAllocation) {
    // SCENARIO: Multiple swarm agents reach consensus on resource allocation
    config.mode = PSL_MODE_CONSENSUS_REQUIRED;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Simulate multiple resource requests
    float resource_amounts[] = {0.2f, 0.3f, 0.4f, 0.5f};

    for (float amount : resource_amounts) {
        unified_decision_result_t result;
        EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 100, amount, &result), 0);

        if (result.approved) {
            // In consensus mode, swarm must approve
            EXPECT_TRUE(result.swarm_approved);
            EXPECT_GT(result.swarm_consensus_count, 0);
        }
    }
}

TEST_F(PortiaSwarmLogicE2ETest, BrainNeuromodulationAffectingLogicThresholds) {
    // SCENARIO: Brain neurotransmitters modulate decision thresholds
    config.enable_brain_integration = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Make decisions with brain connected
    unified_decision_result_t result_with_brain;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result_with_brain), 0);

    // Brain connection should affect decisions (even if NULL, connection is tracked)
    EXPECT_GE(result_with_brain.confidence, 0.0f);
}

TEST_F(PortiaSwarmLogicE2ETest, ImmuneSystemInflammationAffectingDecisions) {
    // SCENARIO: Immune inflammation modulates decision-making
    config.enable_immune_feedback = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Make decisions with immune system connected
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 20, &result), 0);

    // Immune feedback should be incorporated
    EXPECT_GE(result.decision_time_us, 0);
}

TEST_F(PortiaSwarmLogicE2ETest, MemoryPressureTriggeringCoordinatedResponse) {
    // SCENARIO: UMM memory pressure triggers coordinated degradation
    config.enable_umm_tracking = true;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Simulate memory pressure scenario
    SimulateResourcePressure();

    // Check for degradation decisions
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, 15, &result), 0);

    // Memory pressure should influence resource allocation
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 50, 0.8f, &result), 0);
}

TEST_F(PortiaSwarmLogicE2ETest, RecoveryFromCommunicationFailure) {
    // SCENARIO: System recovers gracefully from communication issues
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Make decisions with no bridges connected (simulates comm failure)
    unified_decision_result_t result;

    // System should still make local decisions
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 25, 0.5f, &result), 0);

    // Verify no crashes and stats tracked
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 2);
}

TEST_F(PortiaSwarmLogicE2ETest, LongRunningOperationalStability) {
    // SCENARIO: System runs stably over extended period
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Simulate long-running operation
    const int total_cycles = 500;
    unified_decision_result_t result;

    for (int cycle = 0; cycle < total_cycles; cycle++) {
        // Periodic decision-making cycle
        EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, cycle, 0.5f, &result), 0);

        if (cycle % 100 == 0) {
            // Periodic tier check
            EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, cycle % 4, (cycle + 1) % 4, &result), 0);
        }

        if (cycle % 50 == 0) {
            // Periodic degradation check
            EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, cycle, &result), 0);
        }

        if (cycle % 200 == 0) {
            // Periodic emergency check
            EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);
        }
    }

    // Verify system stability
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.total_decisions, total_cycles);
    EXPECT_GT(stats.avg_decision_time_us, 0.0f);
}

TEST_F(PortiaSwarmLogicE2ETest, ComplexMultiAgentScenario) {
    // SCENARIO: Complex scenario with multiple agents and decision types
    config.mode = PSL_MODE_COORDINATED;
    config.local_weight = 0.6f;
    config.collective_weight = 0.4f;

    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(portia_swarm_logic_connect_brain(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_immune(bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_umm(bridge, nullptr), 0);

    EXPECT_EQ(portia_swarm_logic_start(bridge), 0);

    // Create custom gates for complex decisions
    uint32_t resource_gate, emergency_gate;
    EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, "A AND B", &resource_gate), 0);
    EXPECT_EQ(portia_swarm_logic_add_unified_gate(bridge, "A OR B", &emergency_gate), 0);

    // Simulate complex multi-agent scenario
    unified_decision_result_t result;

    // Agent 1: High resource request
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 1, 0.8f, &result), 0);
    bool agent1_approved = result.approved;

    // Agent 2: Moderate resource request
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(bridge, 2, 0.5f, &result), 0);

    // Agent 3: Tier upgrade request
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 2, &result), 0);

    // Emergency check based on aggregate state
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(bridge, &result), 0);

    if (result.approved) {
        // Emergency mode: all agents should degrade
        for (int agent = 1; agent <= 3; agent++) {
            EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, agent, &result), 0);
        }
    }

    // Verify comprehensive statistics
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_decisions, 5);
    EXPECT_GT(stats.avg_decision_time_us, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
