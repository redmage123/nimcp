/**
 * @file test_security_collective_regression.cpp
 * @brief Regression tests for Security-Collective Cognition Bridge
 *
 * WHAT: Regression tests for security-collective cognition bidirectional bridge
 * WHY:  Verify correctness, consistency, and expected behavior over time
 * HOW:  Test invariants, boundary conditions, state consistency, and
 *       known edge cases that have caused issues in similar systems
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

extern "C" {
#include "security/collective/nimcp_security_collective_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityCollectiveRegressionTest : public ::testing::Test {
protected:
    security_collective_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = nullptr;
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
};

// ============================================================================
// State Consistency Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, StateConsistencyAfterCreate) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_state_t state = {};
    int ret = security_collective_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.total_agents, 0u);
    EXPECT_EQ(state.quarantined_count, 0u);
    EXPECT_EQ(state.suspected_byzantine_count, 0u);
    EXPECT_EQ(state.confirmed_byzantine_count, 0u);
    EXPECT_GE(state.swarm_health, 0.0f);
    EXPECT_LE(state.swarm_health, 1.0f);
}

TEST_F(SecurityCollectiveRegressionTest, StatsConsistencyAfterCreate) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_stats_t stats = {};
    int ret = security_collective_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_byzantine_checks, 0u);
    EXPECT_EQ(stats.byzantine_detections, 0u);
    EXPECT_EQ(stats.agents_quarantined, 0u);
    EXPECT_EQ(stats.consensus_verifications, 0u);
    EXPECT_EQ(stats.swarm_monitoring_updates, 0u);
    EXPECT_EQ(stats.bridge_updates, 0u);
}

TEST_F(SecurityCollectiveRegressionTest, AgentCountConsistency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Register agents
    RegisterAgents(10);

    security_collective_state_t state1 = {};
    security_collective_bridge_get_state(bridge, &state1);
    EXPECT_EQ(state1.total_agents, 10u);

    // Unregister half
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_unregister_agent(bridge, i);
    }

    security_collective_state_t state2 = {};
    security_collective_bridge_get_state(bridge, &state2);
    EXPECT_EQ(state2.total_agents, 5u);
}

TEST_F(SecurityCollectiveRegressionTest, QuarantineCountConsistency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Quarantine agents one by one
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_quarantine_agent(bridge, i, "test");

        security_collective_state_t state = {};
        security_collective_bridge_get_state(bridge, &state);
        EXPECT_EQ(state.quarantined_count, i - 99);  // 1, 2, 3, 4, 5
    }

    // Release agents one by one
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_release_agent(bridge, i);

        security_collective_state_t state = {};
        security_collective_bridge_get_state(bridge, &state);
        EXPECT_EQ(state.quarantined_count, 104 - i);  // 4, 3, 2, 1, 0
    }
}

// ============================================================================
// Trust Score Invariant Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, TrustScoreBoundedZeroOne) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Test with extreme positive actions
    for (int i = 0; i < 1000; i++) {
        security_collective_report_action(bridge, 100, true, 1.0f);
    }

    agent_trust_result_t result_high = {};
    security_collective_score_agent(bridge, 100, &result_high);
    EXPECT_LE(result_high.trust_score, 1.0f);
    EXPECT_GE(result_high.trust_score, 0.0f);

    // Test with extreme negative actions
    for (int i = 0; i < 2000; i++) {
        security_collective_report_action(bridge, 100, false, 1.0f);
    }

    agent_trust_result_t result_low = {};
    security_collective_score_agent(bridge, 100, &result_low);
    EXPECT_LE(result_low.trust_score, 1.0f);
    EXPECT_GE(result_low.trust_score, 0.0f);
}

TEST_F(SecurityCollectiveRegressionTest, TrustLevelMatchesScore) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Test at various trust levels
    for (int round = 0; round < 50; round++) {
        bool positive = (round % 3 != 0);  // 2/3 positive
        security_collective_report_action(bridge, 100, positive, 0.5f);

        agent_trust_result_t result = {};
        security_collective_score_agent(bridge, 100, &result);

        // Verify level matches score
        float score = result.trust_score;
        trust_level_t level = result.level;

        if (score < 0.1f) {
            EXPECT_EQ(level, TRUST_LEVEL_UNTRUSTED);
        } else if (score < 0.3f) {
            EXPECT_EQ(level, TRUST_LEVEL_MINIMAL);
        } else if (score < 0.5f) {
            EXPECT_EQ(level, TRUST_LEVEL_LOW);
        } else if (score < 0.7f) {
            EXPECT_EQ(level, TRUST_LEVEL_MODERATE);
        } else if (score < 0.9f) {
            EXPECT_EQ(level, TRUST_LEVEL_HIGH);
        } else {
            EXPECT_EQ(level, TRUST_LEVEL_VERIFIED);
        }
    }
}

TEST_F(SecurityCollectiveRegressionTest, ActionCountsConsistency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    uint32_t positive_count = 0;
    uint32_t negative_count = 0;

    for (int i = 0; i < 100; i++) {
        bool positive = (i % 2 == 0);
        security_collective_report_action(bridge, 100, positive, 0.5f);

        if (positive) {
            positive_count++;
        } else {
            negative_count++;
        }
    }

    agent_trust_result_t result = {};
    security_collective_score_agent(bridge, 100, &result);

    EXPECT_EQ(result.positive_actions, positive_count);
    EXPECT_EQ(result.negative_actions, negative_count);
}

// ============================================================================
// Byzantine Detection Invariant Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, ByzantineConflictCountIncreases) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    uint32_t expected_conflicts = 0;

    for (int i = 0; i < 10; i++) {
        security_collective_report_action(bridge, 100, false, 1.0f);
        expected_conflicts++;

        byzantine_detection_result_t result = {};
        security_collective_detect_byzantine(bridge, 100, &result);

        EXPECT_EQ(result.conflict_count, expected_conflicts);
    }
}

TEST_F(SecurityCollectiveRegressionTest, QuarantineEffectsReflected) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(5);

    // Quarantine agent
    security_collective_quarantine_agent(bridge, 100, "test");

    // Verify in Byzantine status
    byzantine_detection_result_t byz = {};
    security_collective_detect_byzantine(bridge, 100, &byz);
    EXPECT_TRUE(byz.is_quarantined);
    EXPECT_EQ(byz.status, BYZANTINE_STATUS_QUARANTINED);

    // Verify in effects
    security_to_collective_effects_t effects = {};
    security_collective_get_security_effects(bridge, &effects);
    EXPECT_EQ(effects.quarantined_agent_count, 1u);

    // Verify in quarantine list
    uint32_t quarantined[10] = {};
    uint32_t count = 0;
    security_collective_get_quarantined_agents(bridge, quarantined, 10, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(quarantined[0], 100u);
}

// ============================================================================
// Consensus Verification Invariant Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, ConsensusValidVotesNotExceedParticipants) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    uint32_t participants[10];
    for (uint32_t i = 0; i < 10; i++) {
        participants[i] = 100 + i;
    }

    consensus_verification_result_t result = {};
    security_collective_verify_consensus(bridge, 1, participants, 10, &result);

    EXPECT_LE(result.valid_votes + result.invalid_votes, result.participant_count);
}

TEST_F(SecurityCollectiveRegressionTest, ConsensusQuorumRatioBounded) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    for (uint32_t num_participants = 2; num_participants <= 10; num_participants++) {
        uint32_t participants[10];
        for (uint32_t i = 0; i < num_participants; i++) {
            participants[i] = 100 + i;
        }

        consensus_verification_result_t result = {};
        security_collective_verify_consensus(bridge, 1, participants, num_participants, &result);

        EXPECT_GE(result.quorum_ratio, 0.0f);
        EXPECT_LE(result.quorum_ratio, 1.0f);
    }
}

// ============================================================================
// Swarm Monitoring Invariant Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, SwarmMetricsBounded) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Various scenarios
    for (int scenario = 0; scenario < 5; scenario++) {
        // Vary quarantine level
        for (uint32_t i = 100; i < 100 + scenario * 4; i++) {
            security_collective_quarantine_agent(bridge, i, "test");
        }

        swarm_monitoring_result_t result = {};
        security_collective_monitor_swarm(bridge, &result);

        EXPECT_GE(result.synchronization_level, 0.0f);
        EXPECT_LE(result.synchronization_level, 1.0f);
        EXPECT_GE(result.coherence_level, 0.0f);
        EXPECT_LE(result.coherence_level, 1.0f);
        EXPECT_GE(result.fragmentation_index, 0.0f);
        EXPECT_LE(result.fragmentation_index, 1.0f);
        EXPECT_GE(result.anomaly_score, 0.0f);
        EXPECT_LE(result.anomaly_score, 1.0f);

        // Reset for next scenario
        for (uint32_t i = 100; i < 100 + scenario * 4; i++) {
            security_collective_release_agent(bridge, i);
        }
    }
}

TEST_F(SecurityCollectiveRegressionTest, ActiveAgentsMatchExpected) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Quarantine 5 agents
    for (uint32_t i = 100; i < 105; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    swarm_monitoring_result_t result = {};
    security_collective_monitor_swarm(bridge, &result);

    EXPECT_EQ(result.active_agents, 15u);  // 20 - 5
}

// ============================================================================
// Emergent Pattern Invariant Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, PatternAuthenticityBounded) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    for (int scenario = 0; scenario < 10; scenario++) {
        emergent_pattern_result_t result = {};
        security_collective_validate_emergent(bridge, static_cast<uint32_t>(scenario), &result);

        EXPECT_GE(result.authenticity_score, 0.0f);
        EXPECT_LE(result.authenticity_score, 1.0f);
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
    }
}

TEST_F(SecurityCollectiveRegressionTest, PatternContributorsMatchNonQuarantined) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(20);

    // Quarantine 7 agents
    for (uint32_t i = 100; i < 107; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    emergent_pattern_result_t result = {};
    security_collective_validate_emergent(bridge, 1, &result);

    EXPECT_EQ(result.contributing_agents, 13u);  // 20 - 7
}

// ============================================================================
// Statistics Consistency Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, ByzantineStatsMonotonic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(5);

    uint64_t prev_checks = 0;

    for (int i = 0; i < 10; i++) {
        byzantine_detection_result_t result = {};
        security_collective_detect_byzantine(bridge, 100, &result);

        security_collective_stats_t stats = {};
        security_collective_bridge_get_stats(bridge, &stats);

        EXPECT_GE(stats.total_byzantine_checks, prev_checks);
        prev_checks = stats.total_byzantine_checks;
    }
}

TEST_F(SecurityCollectiveRegressionTest, ConsensusStatsMonotonic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(5);

    uint64_t prev_verifications = 0;

    for (int i = 0; i < 10; i++) {
        consensus_verification_result_t result = {};
        security_collective_verify_consensus(bridge, static_cast<uint32_t>(i), nullptr, 5, &result);

        security_collective_stats_t stats = {};
        security_collective_bridge_get_stats(bridge, &stats);

        EXPECT_GE(stats.consensus_verifications, prev_verifications);
        prev_verifications = stats.consensus_verifications;
    }
}

TEST_F(SecurityCollectiveRegressionTest, UpdateStatsMonotonic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint64_t prev_updates = 0;

    for (int i = 0; i < 10; i++) {
        security_collective_bridge_update(bridge, 100);

        security_collective_stats_t stats = {};
        security_collective_bridge_get_stats(bridge, &stats);

        EXPECT_GE(stats.bridge_updates, prev_updates);
        prev_updates = stats.bridge_updates;
    }
}

// ============================================================================
// Reset Behavior Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, ResetStatsZerosAllCounters) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(5);

    // Generate statistics
    byzantine_detection_result_t byz = {};
    security_collective_detect_byzantine(bridge, 100, &byz);

    consensus_verification_result_t cons = {};
    security_collective_verify_consensus(bridge, 1, nullptr, 5, &cons);

    swarm_monitoring_result_t swarm = {};
    security_collective_monitor_swarm(bridge, &swarm);

    security_collective_bridge_update(bridge, 100);

    // Reset
    security_collective_bridge_reset_stats(bridge);

    // Verify all zero
    security_collective_stats_t stats = {};
    security_collective_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_byzantine_checks, 0u);
    EXPECT_EQ(stats.byzantine_detections, 0u);
    EXPECT_EQ(stats.agents_quarantined, 0u);
    EXPECT_EQ(stats.consensus_verifications, 0u);
    EXPECT_EQ(stats.swarm_monitoring_updates, 0u);
    EXPECT_EQ(stats.bridge_updates, 0u);
    EXPECT_EQ(stats.patterns_validated, 0u);
    EXPECT_EQ(stats.trust_updates, 0u);
}

// ============================================================================
// Double Operation Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, DoubleRegisterNoError) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret1 = security_collective_register_agent(bridge, 100);
    EXPECT_EQ(ret1, 0);

    int ret2 = security_collective_register_agent(bridge, 100);
    EXPECT_EQ(ret2, 0);

    // Count should still be 1
    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);
    EXPECT_EQ(state.total_agents, 1u);
}

TEST_F(SecurityCollectiveRegressionTest, DoubleQuarantineNoError) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    int ret1 = security_collective_quarantine_agent(bridge, 100, "first");
    EXPECT_EQ(ret1, 0);

    int ret2 = security_collective_quarantine_agent(bridge, 100, "second");
    EXPECT_EQ(ret2, 0);

    // Count should still be 1
    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);
    EXPECT_EQ(state.quarantined_count, 1u);
}

TEST_F(SecurityCollectiveRegressionTest, DoubleReleaseNoError) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);
    security_collective_quarantine_agent(bridge, 100, "test");

    int ret1 = security_collective_release_agent(bridge, 100);
    EXPECT_EQ(ret1, 0);

    int ret2 = security_collective_release_agent(bridge, 100);
    EXPECT_EQ(ret2, 0);

    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);
    EXPECT_EQ(state.quarantined_count, 0u);
}

// ============================================================================
// Order Independence Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, RegistrationOrderIndependence) {
    // Create first bridge with forward registration
    security_collective_config_t config;
    security_collective_default_config(&config);

    security_collective_bridge_t* bridge1 = security_collective_bridge_create(&config);
    if (!bridge1) GTEST_SKIP();

    for (uint32_t i = 100; i < 110; i++) {
        security_collective_register_agent(bridge1, i);
    }

    security_collective_state_t state1 = {};
    security_collective_bridge_get_state(bridge1, &state1);

    security_collective_bridge_destroy(bridge1);

    // Create second bridge with reverse registration
    security_collective_bridge_t* bridge2 = security_collective_bridge_create(&config);
    if (!bridge2) GTEST_SKIP();

    for (uint32_t i = 109; i >= 100 && i <= 109; i--) {
        security_collective_register_agent(bridge2, i);
    }

    security_collective_state_t state2 = {};
    security_collective_bridge_get_state(bridge2, &state2);

    security_collective_bridge_destroy(bridge2);

    // States should be equivalent
    EXPECT_EQ(state1.total_agents, state2.total_agents);
}

// ============================================================================
// Effects Consistency Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, SecurityEffectsMatchState) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    // Quarantine some
    for (uint32_t i = 100; i < 104; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    security_collective_apply_security_effects(bridge);

    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    security_to_collective_effects_t effects = {};
    security_collective_get_security_effects(bridge, &effects);

    EXPECT_EQ(effects.quarantined_agent_count, state.quarantined_count);
}

TEST_F(SecurityCollectiveRegressionTest, CollectiveEffectsMatchState) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterAgents(10);

    security_collective_bridge_update(bridge, 100);
    security_collective_apply_collective_effects(bridge);

    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    collective_to_security_effects_t effects = {};
    security_collective_get_collective_effects(bridge, &effects);

    uint32_t expected_active = state.total_agents - state.quarantined_count;
    EXPECT_EQ(effects.active_agent_count, expected_active);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, EmptyBridgeOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // All operations should work on empty bridge
    byzantine_detection_result_t byz = {};
    EXPECT_EQ(security_collective_detect_byzantine(bridge, 999, &byz), 0);

    consensus_verification_result_t cons = {};
    EXPECT_EQ(security_collective_verify_consensus(bridge, 1, nullptr, 0, &cons), 0);

    swarm_monitoring_result_t swarm = {};
    EXPECT_EQ(security_collective_monitor_swarm(bridge, &swarm), 0);

    emergent_pattern_result_t pattern = {};
    EXPECT_EQ(security_collective_validate_emergent(bridge, 1, &pattern), 0);

    EXPECT_EQ(security_collective_bridge_update(bridge, 100), 0);
    EXPECT_EQ(security_collective_apply_security_effects(bridge), 0);
    EXPECT_EQ(security_collective_apply_collective_effects(bridge), 0);
}

TEST_F(SecurityCollectiveRegressionTest, SingleAgentOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // All operations should work with single agent
    agent_trust_result_t trust = {};
    EXPECT_EQ(security_collective_score_agent(bridge, 100, &trust), 0);

    byzantine_detection_result_t byz = {};
    EXPECT_EQ(security_collective_detect_byzantine(bridge, 100, &byz), 0);

    EXPECT_EQ(security_collective_quarantine_agent(bridge, 100, "test"), 0);
    EXPECT_EQ(security_collective_release_agent(bridge, 100), 0);

    swarm_monitoring_result_t swarm = {};
    EXPECT_EQ(security_collective_monitor_swarm(bridge, &swarm), 0);
    EXPECT_EQ(swarm.active_agents, 1u);
}

TEST_F(SecurityCollectiveRegressionTest, UnregisterQuarantinedAgent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);
    security_collective_quarantine_agent(bridge, 100, "test");

    security_collective_state_t state1 = {};
    security_collective_bridge_get_state(bridge, &state1);
    EXPECT_EQ(state1.quarantined_count, 1u);

    // Unregister quarantined agent
    security_collective_unregister_agent(bridge, 100);

    security_collective_state_t state2 = {};
    security_collective_bridge_get_state(bridge, &state2);

    // Both counts should decrease
    EXPECT_EQ(state2.total_agents, 0u);
    EXPECT_EQ(state2.quarantined_count, 0u);
}

// ============================================================================
// Configuration Validation Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, ConfigSensitivityBounds) {
    security_collective_config_t config;
    security_collective_default_config(&config);

    EXPECT_GE(config.security_sensitivity, 0.5f);
    EXPECT_LE(config.security_sensitivity, 2.0f);
    EXPECT_GE(config.collective_sensitivity, 0.5f);
    EXPECT_LE(config.collective_sensitivity, 2.0f);
}

TEST_F(SecurityCollectiveRegressionTest, ConfigThresholdBounds) {
    security_collective_config_t config;
    security_collective_default_config(&config);

    EXPECT_GE(config.byzantine_threshold, 0.0f);
    EXPECT_LE(config.byzantine_threshold, 1.0f);
    EXPECT_GE(config.min_quorum_ratio, 0.0f);
    EXPECT_LE(config.min_quorum_ratio, 1.0f);
    EXPECT_GE(config.anomaly_threshold, 0.0f);
    EXPECT_LE(config.anomaly_threshold, 1.0f);
    EXPECT_GE(config.pattern_confidence_threshold, 0.0f);
    EXPECT_LE(config.pattern_confidence_threshold, 1.0f);
    EXPECT_GE(config.initial_trust_score, 0.0f);
    EXPECT_LE(config.initial_trust_score, 1.0f);
}

// ============================================================================
// Memory Safety Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, NullParameterSafety) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // All NULL output parameters should return error, not crash
    EXPECT_EQ(security_collective_bridge_get_state(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_bridge_get_stats(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_get_security_effects(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_get_collective_effects(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_detect_byzantine(bridge, 1, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_verify_consensus(bridge, 1, nullptr, 5, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_monitor_swarm(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_validate_emergent(bridge, 1, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_score_agent(bridge, 1, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveRegressionTest, NullBridgeSafety) {
    // All operations with NULL bridge should return error, not crash
    security_collective_state_t state = {};
    security_collective_stats_t stats = {};
    security_to_collective_effects_t sec_effects = {};
    collective_to_security_effects_t col_effects = {};
    byzantine_detection_result_t byz = {};
    consensus_verification_result_t cons = {};
    swarm_monitoring_result_t swarm = {};
    emergent_pattern_result_t pattern = {};
    agent_trust_result_t trust = {};
    uint32_t agents[10] = {};
    uint32_t count = 0;

    EXPECT_EQ(security_collective_bridge_get_state(nullptr, &state),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_bridge_get_stats(nullptr, &stats),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_get_security_effects(nullptr, &sec_effects),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_get_collective_effects(nullptr, &col_effects),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_detect_byzantine(nullptr, 1, &byz),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_verify_consensus(nullptr, 1, nullptr, 5, &cons),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_monitor_swarm(nullptr, &swarm),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_validate_emergent(nullptr, 1, &pattern),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_score_agent(nullptr, 1, &trust),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_register_agent(nullptr, 1),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_unregister_agent(nullptr, 1),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_quarantine_agent(nullptr, 1, "test"),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_release_agent(nullptr, 1),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_report_action(nullptr, 1, true, 1.0f),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_bridge_update(nullptr, 100),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_apply_security_effects(nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_apply_collective_effects(nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_bridge_reset_stats(nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_get_quarantined_agents(nullptr, agents, 10, &count),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_get_agents_by_trust(nullptr, TRUST_LEVEL_HIGH, agents, 10, &count),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_bridge_connect_bio_async(nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_collective_bridge_disconnect_bio_async(nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(security_collective_bridge_is_bio_async_connected(nullptr));
    EXPECT_FALSE(security_collective_bridge_is_connected(nullptr));
}

// ============================================================================
// Destroy Safety Tests
// ============================================================================

TEST_F(SecurityCollectiveRegressionTest, DestroyNullSafe) {
    security_collective_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(SecurityCollectiveRegressionTest, DestroyAfterOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Perform many operations
    RegisterAgents(20);

    for (int i = 0; i < 10; i++) {
        byzantine_detection_result_t byz = {};
        security_collective_detect_byzantine(bridge, 100, &byz);

        consensus_verification_result_t cons = {};
        security_collective_verify_consensus(bridge, static_cast<uint32_t>(i), nullptr, 5, &cons);

        security_collective_bridge_update(bridge, 100);
    }

    for (uint32_t i = 100; i < 110; i++) {
        security_collective_quarantine_agent(bridge, i, "test");
    }

    // Destroy should succeed without issues
    security_collective_bridge_destroy(bridge);
    bridge = nullptr;  // Prevent double-free in TearDown
}
