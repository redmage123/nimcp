/**
 * @file test_security_collective_integration.cpp
 * @brief Integration tests for Security-Collective Cognition Bridge
 *
 * WHAT: Integration tests for security-collective cognition bidirectional bridge
 * WHY:  Verify security and collective cognition systems integrate correctly
 *       with proper data flow, state synchronization, and bidirectional effects
 * HOW:  Test multi-agent scenarios, consensus workflows, Byzantine detection,
 *       pattern validation, and system-wide behavior monitoring
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

extern "C" {
#include "security/collective/nimcp_security_collective_bridge.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "security/nimcp_policy_engine.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityCollectiveIntegrationTest : public ::testing::Test {
protected:
    security_collective_bridge_t* bridge = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        bridge = nullptr;
        rng.seed(42);  // Fixed seed for reproducibility
    }

    void TearDown() override {
        if (bridge) {
            security_collective_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        security_collective_config_t config;
        security_collective_default_config(&config);
        bridge = security_collective_bridge_create(&config);
    }

    void CreateBridgeWithConfig(const security_collective_config_t* config) {
        bridge = security_collective_bridge_create(config);
    }

    void RegisterAgents(uint32_t count, uint32_t base_id = 100) {
        for (uint32_t i = 0; i < count; i++) {
            security_collective_register_agent(bridge, base_id + i);
        }
    }

    void SimulatePositiveActions(uint32_t agent_id, int count) {
        for (int i = 0; i < count; i++) {
            security_collective_report_action(bridge, agent_id, true, 0.5f);
        }
    }

    void SimulateNegativeActions(uint32_t agent_id, int count) {
        for (int i = 0; i < count; i++) {
            security_collective_report_action(bridge, agent_id, false, 0.5f);
        }
    }

    float RandomFloat(float min_val, float max_val) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        return dist(rng);
    }
};

// ============================================================================
// Multi-Agent Scenario Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, LargeSwarmRegistration) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Register a large swarm of agents
    uint32_t agent_count = 50;
    RegisterAgents(agent_count);

    // Verify state
    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    EXPECT_EQ(state.total_agents, agent_count);
    EXPECT_GE(state.swarm_health, 0.0f);
}

TEST_F(SecurityCollectiveIntegrationTest, SwarmWithMixedTrustLevels) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Simulate different trust levels
    // Group 1: High trust (positive actions)
    for (uint32_t i = 100; i < 105; i++) {
        SimulatePositiveActions(i, 20);
    }

    // Group 2: Low trust (negative actions)
    for (uint32_t i = 105; i < 110; i++) {
        SimulateNegativeActions(i, 10);
    }

    // Group 3: Mixed (alternating actions)
    for (uint32_t i = 110; i < 115; i++) {
        SimulatePositiveActions(i, 5);
        SimulateNegativeActions(i, 3);
    }

    // Group 4: Neutral (no actions beyond registration)
    // Agents 115-119 remain at initial trust

    // Update bridge
    security_collective_bridge_update(bridge, 100);
    security_collective_apply_collective_effects(bridge);

    // Verify trust distribution
    collective_to_security_effects_t effects = {};
    security_collective_get_collective_effects(bridge, &effects);

    EXPECT_GT(effects.trust_variation, 0.0f);  // Should have variation
}

TEST_F(SecurityCollectiveIntegrationTest, AgentJoinLeaveWorkflow) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Start with initial agents
    RegisterAgents(10);

    security_collective_state_t state1 = {};
    security_collective_bridge_get_state(bridge, &state1);
    EXPECT_EQ(state1.total_agents, 10u);

    // Add more agents
    RegisterAgents(5, 200);

    security_collective_state_t state2 = {};
    security_collective_bridge_get_state(bridge, &state2);
    EXPECT_EQ(state2.total_agents, 15u);

    // Remove some agents
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_unregister_agent(bridge, i);
    }

    security_collective_state_t state3 = {};
    security_collective_bridge_get_state(bridge, &state3);
    EXPECT_EQ(state3.total_agents, 10u);
}

// ============================================================================
// Byzantine Detection Workflow Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, ByzantineDetectionWorkflow) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.byzantine_threshold = 0.3f;
    config.min_conflicts_for_byzantine = 3;
    config.enable_automatic_quarantine = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Make one agent Byzantine
    uint32_t byzantine_agent = 100;
    SimulateNegativeActions(byzantine_agent, 10);

    // Detect Byzantine
    byzantine_detection_result_t result = {};
    security_collective_detect_byzantine(bridge, byzantine_agent, &result);

    // Verify detection
    EXPECT_GE(result.conflict_count, 3u);

    // Check state
    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    if (result.status == BYZANTINE_STATUS_QUARANTINED) {
        EXPECT_EQ(state.quarantined_count, 1u);
    }
}

TEST_F(SecurityCollectiveIntegrationTest, MultipleByzantineAgents) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.byzantine_threshold = 0.3f;
    config.min_conflicts_for_byzantine = 3;
    config.enable_automatic_quarantine = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Make several agents Byzantine
    std::vector<uint32_t> byzantine_agents = {100, 105, 110, 115};
    for (uint32_t agent : byzantine_agents) {
        SimulateNegativeActions(agent, 15);
    }

    // Detect all Byzantine agents
    uint32_t detected_count = 0;
    for (uint32_t agent : byzantine_agents) {
        byzantine_detection_result_t result = {};
        security_collective_detect_byzantine(bridge, agent, &result);
        if (result.status >= BYZANTINE_STATUS_SUSPECTED) {
            detected_count++;
        }
    }

    EXPECT_GE(detected_count, 1u);

    // Verify swarm health degraded
    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    EXPECT_LT(state.swarm_health, 1.0f);
}

TEST_F(SecurityCollectiveIntegrationTest, ByzantineRecovery) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.enable_automatic_quarantine = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Quarantine an agent
    security_collective_quarantine_agent(bridge, 100, "Suspected Byzantine");

    security_collective_state_t state1 = {};
    security_collective_bridge_get_state(bridge, &state1);
    EXPECT_EQ(state1.quarantined_count, 1u);

    // Release agent
    security_collective_release_agent(bridge, 100);

    security_collective_state_t state2 = {};
    security_collective_bridge_get_state(bridge, &state2);
    EXPECT_EQ(state2.quarantined_count, 0u);

    // Verify trust is minimal after release
    agent_trust_result_t trust = {};
    security_collective_score_agent(bridge, 100, &trust);
    EXPECT_LT(trust.trust_score, 0.5f);
}

// ============================================================================
// Consensus Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, ConsensusWithTrustedAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Boost all agents' trust
    for (uint32_t i = 100; i < 110; i++) {
        SimulatePositiveActions(i, 10);
    }

    // Verify consensus
    uint32_t participants[10];
    for (uint32_t i = 0; i < 10; i++) {
        participants[i] = 100 + i;
    }

    consensus_verification_result_t result = {};
    security_collective_verify_consensus(bridge, 1, participants, 10, &result);

    EXPECT_EQ(result.validity, CONSENSUS_VALID);
    EXPECT_EQ(result.valid_votes, 10u);
}

TEST_F(SecurityCollectiveIntegrationTest, ConsensusWithQuarantinedAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Quarantine half the agents
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    // Verify consensus
    uint32_t participants[10];
    for (uint32_t i = 0; i < 10; i++) {
        participants[i] = 100 + i;
    }

    consensus_verification_result_t result = {};
    security_collective_verify_consensus(bridge, 1, participants, 10, &result);

    // Half should be invalid
    EXPECT_EQ(result.invalid_votes, 5u);
}

TEST_F(SecurityCollectiveIntegrationTest, ConsensusQuorumThreshold) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.min_quorum_ratio = 0.75f;  // Require 75% quorum

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Quarantine enough to fail quorum
    for (uint32_t i = 100; i < 110; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    // Verify consensus - should fail quorum
    uint32_t participants[20];
    for (uint32_t i = 0; i < 20; i++) {
        participants[i] = 100 + i;
    }

    consensus_verification_result_t result = {};
    security_collective_verify_consensus(bridge, 1, participants, 20, &result);

    // Only 10/20 = 50% valid, less than 75% required
    EXPECT_EQ(result.validity, CONSENSUS_INVALID_QUORUM);
}

// ============================================================================
// Swarm Monitoring Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, SwarmHealthOverTime) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Initial health check
    swarm_monitoring_result_t initial = {};
    security_collective_monitor_swarm(bridge, &initial);
    EXPECT_EQ(initial.active_agents, 20u);

    // Simulate time passing with positive actions
    for (int t = 0; t < 10; t++) {
        for (uint32_t i = 100; i < 120; i++) {
            security_collective_report_action(bridge, i, true, 0.3f);
        }
        security_collective_bridge_update(bridge, 100);
    }

    // Health should remain good
    swarm_monitoring_result_t after = {};
    security_collective_monitor_swarm(bridge, &after);
    EXPECT_GE(after.synchronization_level, initial.synchronization_level);
}

TEST_F(SecurityCollectiveIntegrationTest, SwarmFragmentation) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.anomaly_threshold = 0.3f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Quarantine many agents to cause fragmentation
    for (uint32_t i = 100; i < 112; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    swarm_monitoring_result_t result = {};
    security_collective_monitor_swarm(bridge, &result);

    EXPECT_GT(result.fragmentation_index, 0.5f);  // 12/20 = 60% fragmentation
    EXPECT_TRUE(result.anomaly_detected);
}

TEST_F(SecurityCollectiveIntegrationTest, SwarmSynchronization) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Create highly synchronized swarm (all high trust)
    for (uint32_t i = 100; i < 110; i++) {
        SimulatePositiveActions(i, 20);
    }

    swarm_monitoring_result_t result = {};
    security_collective_monitor_swarm(bridge, &result);

    EXPECT_GT(result.synchronization_level, 0.5f);
    EXPECT_GT(result.coherence_level, 0.5f);
}

// ============================================================================
// Emergent Pattern Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, ValidEmergentPattern) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // High trust agents contribute to valid pattern
    for (uint32_t i = 100; i < 110; i++) {
        SimulatePositiveActions(i, 15);
    }

    emergent_pattern_result_t result = {};
    security_collective_validate_emergent(bridge, 1, &result);

    EXPECT_EQ(result.status, EMERGENT_PATTERN_VALID);
    EXPECT_GT(result.authenticity_score, 0.5f);
    EXPECT_TRUE(result.is_stable);
}

TEST_F(SecurityCollectiveIntegrationTest, SuspiciousEmergentPattern) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.pattern_confidence_threshold = 0.7f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Mixed trust creates suspicious pattern
    for (uint32_t i = 100; i < 105; i++) {
        SimulatePositiveActions(i, 5);
    }
    for (uint32_t i = 105; i < 110; i++) {
        SimulateNegativeActions(i, 5);
    }

    emergent_pattern_result_t result = {};
    security_collective_validate_emergent(bridge, 1, &result);

    // Should be suspicious or manipulated due to low overall trust
    EXPECT_NE(result.status, EMERGENT_PATTERN_VALID);
}

TEST_F(SecurityCollectiveIntegrationTest, ManipulatedEmergentPattern) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Quarantine most agents - pattern from few agents is suspicious
    for (uint32_t i = 100; i < 108; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    emergent_pattern_result_t result = {};
    security_collective_validate_emergent(bridge, 1, &result);

    // Very low authenticity due to quarantined agents
    EXPECT_LT(result.authenticity_score, 0.5f);
    EXPECT_EQ(result.contributing_agents, 2u);  // Only 2 non-quarantined
}

// ============================================================================
// Trust Scoring Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, TrustProgression) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Track trust progression
    std::vector<float> trust_history;

    agent_trust_result_t result = {};
    security_collective_score_agent(bridge, 100, &result);
    trust_history.push_back(result.trust_score);

    // Positive actions increase trust
    for (int round = 0; round < 5; round++) {
        SimulatePositiveActions(100, 5);
        security_collective_score_agent(bridge, 100, &result);
        trust_history.push_back(result.trust_score);
    }

    // Verify monotonic increase
    for (size_t i = 1; i < trust_history.size(); i++) {
        EXPECT_GE(trust_history[i], trust_history[i-1]);
    }
}

TEST_F(SecurityCollectiveIntegrationTest, TrustDecayWithUpdates) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Build initial trust
    SimulatePositiveActions(100, 10);

    agent_trust_result_t before = {};
    security_collective_score_agent(bridge, 100, &before);

    // Let time pass with no actions (trust should decay)
    for (int i = 0; i < 50; i++) {
        security_collective_bridge_update(bridge, 1000);  // 1 second each
    }

    agent_trust_result_t after = {};
    security_collective_score_agent(bridge, 100, &after);

    EXPECT_LT(after.trust_score, before.trust_score);
}

TEST_F(SecurityCollectiveIntegrationTest, TrustImpactOnQuarantine) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Get initial trust
    agent_trust_result_t before = {};
    security_collective_score_agent(bridge, 100, &before);

    // Quarantine destroys trust
    security_collective_quarantine_agent(bridge, 100, "test");

    agent_trust_result_t after_quarantine = {};
    security_collective_score_agent(bridge, 100, &after_quarantine);
    EXPECT_LT(after_quarantine.trust_score, before.trust_score * 0.2f);

    // Release restores minimal trust
    security_collective_release_agent(bridge, 100);

    agent_trust_result_t after_release = {};
    security_collective_score_agent(bridge, 100, &after_release);
    EXPECT_EQ(after_release.level, TRUST_LEVEL_MINIMAL);
}

// ============================================================================
// Bidirectional Effects Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, SecurityEffectsOnCollective) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Quarantine some agents
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    security_collective_apply_security_effects(bridge);

    security_to_collective_effects_t effects = {};
    security_collective_get_security_effects(bridge, &effects);

    EXPECT_EQ(effects.quarantined_agent_count, 5u);
    EXPECT_GT(effects.untrusted_agent_count, 0u);
}

TEST_F(SecurityCollectiveIntegrationTest, CollectiveEffectsOnSecurity) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Create diverse trust levels
    for (uint32_t i = 100; i < 110; i++) {
        SimulatePositiveActions(i, 15);
    }
    for (uint32_t i = 110; i < 115; i++) {
        SimulateNegativeActions(i, 10);
    }

    security_collective_apply_collective_effects(bridge);

    collective_to_security_effects_t effects = {};
    security_collective_get_collective_effects(bridge, &effects);

    EXPECT_EQ(effects.active_agent_count, 20u);
    EXPECT_GT(effects.trust_variation, 0.0f);
}

TEST_F(SecurityCollectiveIntegrationTest, BidirectionalUpdateCycle) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(15);

    // Simulate multiple update cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Agent actions
        for (uint32_t i = 100; i < 115; i++) {
            bool positive = RandomFloat(0, 1) > 0.3f;
            security_collective_report_action(bridge, i, positive, 0.3f);
        }

        // Update
        security_collective_bridge_update(bridge, 100);
        security_collective_apply_security_effects(bridge);
        security_collective_apply_collective_effects(bridge);
    }

    // Verify statistics accumulated
    security_collective_stats_t stats = {};
    security_collective_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.bridge_updates, 10u);
    EXPECT_GT(stats.trust_updates, 0u);
}

// ============================================================================
// Performance Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, LargeScaleOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Register many agents
    uint32_t agent_count = 50;
    RegisterAgents(agent_count);

    // Perform many operations
    for (int round = 0; round < 20; round++) {
        // Random actions
        for (uint32_t i = 100; i < 100 + agent_count; i++) {
            bool positive = RandomFloat(0, 1) > 0.4f;
            security_collective_report_action(bridge, i, positive, RandomFloat(0.1f, 0.9f));
        }

        // Byzantine checks
        for (uint32_t i = 100; i < 100 + agent_count; i += 5) {
            byzantine_detection_result_t result = {};
            security_collective_detect_byzantine(bridge, i, &result);
        }

        // Swarm monitoring
        swarm_monitoring_result_t swarm = {};
        security_collective_monitor_swarm(bridge, &swarm);

        // Update
        security_collective_bridge_update(bridge, 50);
    }

    // Verify system still functional
    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    EXPECT_GT(state.total_agents, 0u);
}

TEST_F(SecurityCollectiveIntegrationTest, StatisticsAccumulation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Perform various operations
    for (int i = 0; i < 5; i++) {
        byzantine_detection_result_t byz = {};
        security_collective_detect_byzantine(bridge, 100, &byz);

        consensus_verification_result_t cons = {};
        security_collective_verify_consensus(bridge, static_cast<uint32_t>(i), nullptr, 5, &cons);

        swarm_monitoring_result_t swarm = {};
        security_collective_monitor_swarm(bridge, &swarm);

        emergent_pattern_result_t pattern = {};
        security_collective_validate_emergent(bridge, static_cast<uint32_t>(i), &pattern);

        security_collective_bridge_update(bridge, 100);
    }

    security_collective_stats_t stats = {};
    security_collective_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_byzantine_checks, 5u);
    EXPECT_EQ(stats.consensus_verifications, 5u);
    EXPECT_EQ(stats.swarm_monitoring_updates, 5u);
    EXPECT_EQ(stats.patterns_validated, 5u);
    EXPECT_EQ(stats.bridge_updates, 5u);
}

// ============================================================================
// Bio-Async Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, BioAsyncConnectionWorkflow) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Initial state - not connected
    EXPECT_FALSE(security_collective_bridge_is_bio_async_connected(bridge));

    // Try to connect
    int connect_ret = security_collective_bridge_connect_bio_async(bridge);

    if (connect_ret != 0) {
        GTEST_SKIP() << "Bio-async connection failed";
    }

    // Check if actually connected (router may return 0 but not be available)
    if (!security_collective_bridge_is_bio_async_connected(bridge)) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    // If connected, verify and disconnect
    EXPECT_TRUE(security_collective_bridge_is_bio_async_connected(bridge));

    int disconnect_ret = security_collective_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(disconnect_ret, 0);

    EXPECT_FALSE(security_collective_bridge_is_bio_async_connected(bridge));
}

// ============================================================================
// Error Recovery Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveIntegrationTest, RecoveryAfterMassQuarantine) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Quarantine most agents
    for (uint32_t i = 100; i < 118; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    security_collective_state_t state1 = {};
    security_collective_bridge_get_state(bridge, &state1);
    EXPECT_EQ(state1.quarantined_count, 18u);
    EXPECT_LT(state1.swarm_health, 0.5f);

    // Release all quarantined agents
    for (uint32_t i = 100; i < 118; i++) {
        security_collective_release_agent(bridge, i);
    }

    // Rebuild trust
    for (int round = 0; round < 10; round++) {
        for (uint32_t i = 100; i < 120; i++) {
            SimulatePositiveActions(i, 3);
        }
        security_collective_bridge_update(bridge, 100);
    }

    security_collective_state_t state2 = {};
    security_collective_bridge_get_state(bridge, &state2);
    EXPECT_EQ(state2.quarantined_count, 0u);
    EXPECT_GT(state2.swarm_health, state1.swarm_health);
}

TEST_F(SecurityCollectiveIntegrationTest, HandlingAgentChurn) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Simulate agent churn
    for (int round = 0; round < 10; round++) {
        // Remove some agents
        for (uint32_t i = 100; i < 103; i++) {
            security_collective_unregister_agent(bridge, i);
        }

        // Add new agents
        RegisterAgents(3, 200 + round * 10);

        security_collective_bridge_update(bridge, 100);
    }

    // System should still be functional
    swarm_monitoring_result_t result = {};
    security_collective_monitor_swarm(bridge, &result);

    EXPECT_GT(result.active_agents, 0u);
}
