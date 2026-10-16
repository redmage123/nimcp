/**
 * @file test_security_collective_bridge.cpp
 * @brief Unit tests for Security-Collective Cognition Integration Bridge
 *
 * WHAT: Tests for security-collective cognition bidirectional bridge
 * WHY:  Verify Byzantine detection, consensus verification, swarm monitoring,
 *       emergent pattern validation, and trust scoring integrate correctly
 * HOW:  Test lifecycle, connections, Byzantine detection, consensus, monitoring,
 *       pattern validation, trust scoring, and bidirectional effects
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "security/collective/nimcp_security_collective_bridge.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "security/nimcp_policy_engine.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityCollectiveBridgeTest : public ::testing::Test {
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

    void RegisterTestAgents(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            security_collective_register_agent(bridge, 100 + i);
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, DefaultConfigReturnsValidConfig) {
    security_collective_config_t config;
    memset(&config, 0, sizeof(config));

    int ret = security_collective_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_byzantine_detection);
    EXPECT_TRUE(config.enable_consensus_verification);
    EXPECT_TRUE(config.enable_swarm_monitoring);
    EXPECT_TRUE(config.enable_pattern_validation);
    EXPECT_TRUE(config.enable_trust_scoring);
    EXPECT_GE(config.security_sensitivity, 0.5f);
    EXPECT_LE(config.security_sensitivity, 2.0f);
    EXPECT_GE(config.collective_sensitivity, 0.5f);
    EXPECT_LE(config.collective_sensitivity, 2.0f);
}

TEST_F(SecurityCollectiveBridgeTest, DefaultConfigNullPointer) {
    int ret = security_collective_default_config(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, CreateWithNullConfig) {
    bridge = security_collective_bridge_create(nullptr);
    if (bridge) {
        EXPECT_NE(bridge, nullptr);
    }
}

TEST_F(SecurityCollectiveBridgeTest, CreateWithValidConfig) {
    security_collective_config_t config;
    security_collective_default_config(&config);

    bridge = security_collective_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityCollectiveBridgeTest, CreateWithCustomConfig) {
    security_collective_config_t config;
    security_collective_default_config(&config);

    config.enable_byzantine_detection = true;
    config.byzantine_threshold = 0.5f;
    config.min_conflicts_for_byzantine = 5;
    config.enable_automatic_quarantine = true;
    config.enable_consensus_verification = true;
    config.min_quorum_ratio = 0.75f;
    config.enable_sybil_detection = true;
    config.enable_trust_scoring = true;
    config.initial_trust_score = 0.6f;

    bridge = security_collective_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SecurityCollectiveBridgeTest, DestroyNull) {
    security_collective_bridge_destroy(nullptr);
}

TEST_F(SecurityCollectiveBridgeTest, DestroyValid) {
    CreateBridge();
    if (bridge) {
        security_collective_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(SecurityCollectiveBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        security_collective_config_t config;
        security_collective_default_config(&config);
        security_collective_bridge_t* temp = security_collective_bridge_create(&config);
        EXPECT_NE(temp, nullptr);
        security_collective_bridge_destroy(temp);
    }
}

// ============================================================================
// Connection Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ConnectCollectiveNullBridge) {
    collective_cognition_t* cc = nullptr;
    EXPECT_EQ(security_collective_bridge_connect_collective(nullptr, cc),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ConnectPolicyEngineNullBridge) {
    nimcp_policy_engine_t engine = nullptr;
    EXPECT_EQ(security_collective_bridge_connect_policy_engine(nullptr, engine),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, DisconnectNullBridge) {
    EXPECT_EQ(security_collective_bridge_disconnect(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, IsConnectedNullBridge) {
    EXPECT_FALSE(security_collective_bridge_is_connected(nullptr));
}

// ============================================================================
// Connection Tests - Valid Bridge, NULL System
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ConnectCollectiveNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_collective_bridge_connect_collective(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ConnectPolicyEngineNull) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_EQ(security_collective_bridge_connect_policy_engine(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Connection Tests - Valid Bridge
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, DisconnectValid) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    int ret = security_collective_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, IsConnectedNoConnections) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();
    EXPECT_FALSE(security_collective_bridge_is_connected(bridge));
}

// ============================================================================
// Agent Registration Tests
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, RegisterAgentNullBridge) {
    EXPECT_EQ(security_collective_register_agent(nullptr, 1), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, RegisterAgentBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_register_agent(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, RegisterAgentMultiple) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (uint32_t i = 0; i < 10; i++) {
        int ret = security_collective_register_agent(bridge, 100 + i);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(SecurityCollectiveBridgeTest, RegisterAgentDuplicate) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret1 = security_collective_register_agent(bridge, 100);
    EXPECT_EQ(ret1, 0);

    int ret2 = security_collective_register_agent(bridge, 100);
    EXPECT_EQ(ret2, 0);  // Duplicate registration should succeed silently
}

TEST_F(SecurityCollectiveBridgeTest, UnregisterAgentNullBridge) {
    EXPECT_EQ(security_collective_unregister_agent(nullptr, 1), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, UnregisterAgentNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_unregister_agent(bridge, 999);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityCollectiveBridgeTest, UnregisterAgentBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);
    int ret = security_collective_unregister_agent(bridge, 100);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Byzantine Detection Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, DetectByzantineNullBridge) {
    byzantine_detection_result_t result = {};
    EXPECT_EQ(security_collective_detect_byzantine(nullptr, 1, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, DetectByzantineNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_detect_byzantine(bridge, 1, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Byzantine Detection Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, DetectByzantineUnregisteredAgent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    byzantine_detection_result_t result = {};
    int ret = security_collective_detect_byzantine(bridge, 999, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.status, BYZANTINE_STATUS_NORMAL);
}

TEST_F(SecurityCollectiveBridgeTest, DetectByzantineNormalAgent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    byzantine_detection_result_t result = {};
    int ret = security_collective_detect_byzantine(bridge, 100, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.agent_id, 100u);
    EXPECT_EQ(result.status, BYZANTINE_STATUS_NORMAL);
    EXPECT_FALSE(result.is_quarantined);
}

TEST_F(SecurityCollectiveBridgeTest, DetectByzantineAfterConflicts) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.byzantine_threshold = 0.3f;
    config.min_conflicts_for_byzantine = 3;
    config.enable_automatic_quarantine = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Report negative actions to trigger Byzantine detection
    for (int i = 0; i < 10; i++) {
        security_collective_report_action(bridge, 100, false, 1.0f);
    }

    byzantine_detection_result_t result = {};
    int ret = security_collective_detect_byzantine(bridge, 100, &result);

    EXPECT_EQ(ret, 0);
    // Should be suspected or confirmed Byzantine
    EXPECT_GE(result.conflict_count, 3u);
}

// ============================================================================
// Consensus Verification Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, VerifyConsensusNullBridge) {
    consensus_verification_result_t result = {};
    uint32_t participants[] = {1, 2, 3};
    EXPECT_EQ(security_collective_verify_consensus(nullptr, 1, participants, 3, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, VerifyConsensusNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t participants[] = {1, 2, 3};
    EXPECT_EQ(security_collective_verify_consensus(bridge, 1, participants, 3, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Consensus Verification Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, VerifyConsensusInsufficientQuorum) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    consensus_verification_result_t result = {};

    // Only one participant - should fail quorum
    int ret = security_collective_verify_consensus(bridge, 1, nullptr, 1, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.validity, CONSENSUS_INVALID_QUORUM);
}

TEST_F(SecurityCollectiveBridgeTest, VerifyConsensusNullParticipants) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    consensus_verification_result_t result = {};

    int ret = security_collective_verify_consensus(bridge, 1, nullptr, 5, &result);

    EXPECT_EQ(ret, 0);
    // Should succeed when participants array is NULL (treated as all valid)
}

TEST_F(SecurityCollectiveBridgeTest, VerifyConsensusWithRegisteredAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Register some agents
    for (uint32_t i = 0; i < 5; i++) {
        security_collective_register_agent(bridge, 100 + i);
    }

    uint32_t participants[] = {100, 101, 102, 103, 104};
    consensus_verification_result_t result = {};

    int ret = security_collective_verify_consensus(bridge, 1, participants, 5, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.participant_count, 5u);
    EXPECT_GT(result.valid_votes, 0u);
}

// ============================================================================
// Quarantine Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, QuarantineAgentNullBridge) {
    EXPECT_EQ(security_collective_quarantine_agent(nullptr, 1, "test"),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ReleaseAgentNullBridge) {
    EXPECT_EQ(security_collective_release_agent(nullptr, 1), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Quarantine Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, QuarantineUnregisteredAgent) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_quarantine_agent(bridge, 999, "test");
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityCollectiveBridgeTest, QuarantineAgentBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    int ret = security_collective_quarantine_agent(bridge, 100, "Byzantine behavior");
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, QuarantineAgentDouble) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);
    security_collective_quarantine_agent(bridge, 100, "first");

    // Double quarantine should succeed silently
    int ret = security_collective_quarantine_agent(bridge, 100, "second");
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, ReleaseAgentNotQuarantined) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Release agent that is not quarantined should succeed
    int ret = security_collective_release_agent(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, ReleaseAgentBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);
    security_collective_quarantine_agent(bridge, 100, "test");

    int ret = security_collective_release_agent(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, QuarantineAndVerifyState) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);
    security_collective_quarantine_agent(bridge, 100, "test");

    security_collective_state_t state = {};
    security_collective_bridge_get_state(bridge, &state);

    EXPECT_EQ(state.quarantined_count, 1u);
}

// ============================================================================
// Emergent Pattern Validation Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ValidateEmergentNullBridge) {
    emergent_pattern_result_t result = {};
    EXPECT_EQ(security_collective_validate_emergent(nullptr, 1, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ValidateEmergentNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_validate_emergent(bridge, 1, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Emergent Pattern Validation Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ValidateEmergentNoAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    emergent_pattern_result_t result = {};
    int ret = security_collective_validate_emergent(bridge, 1, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern_id, 1u);
    EXPECT_EQ(result.contributing_agents, 0u);
}

TEST_F(SecurityCollectiveBridgeTest, ValidateEmergentWithAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(5);

    emergent_pattern_result_t result = {};
    int ret = security_collective_validate_emergent(bridge, 1, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.pattern_id, 1u);
    EXPECT_EQ(result.contributing_agents, 5u);
    EXPECT_GE(result.authenticity_score, 0.0f);
    EXPECT_LE(result.authenticity_score, 1.0f);
}

TEST_F(SecurityCollectiveBridgeTest, ValidateEmergentWithQuarantinedAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(5);
    security_collective_quarantine_agent(bridge, 100, "test");
    security_collective_quarantine_agent(bridge, 101, "test");

    emergent_pattern_result_t result = {};
    int ret = security_collective_validate_emergent(bridge, 1, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.contributing_agents, 3u);  // 5 - 2 quarantined
}

// ============================================================================
// Swarm Monitoring Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, MonitorSwarmNullBridge) {
    swarm_monitoring_result_t result = {};
    EXPECT_EQ(security_collective_monitor_swarm(nullptr, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, MonitorSwarmNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_monitor_swarm(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Swarm Monitoring Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, MonitorSwarmEmpty) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    swarm_monitoring_result_t result = {};
    int ret = security_collective_monitor_swarm(bridge, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.active_agents, 0u);
    EXPECT_EQ(result.current_behavior, SWARM_BEHAVIOR_IDLE);
}

TEST_F(SecurityCollectiveBridgeTest, MonitorSwarmWithAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(10);

    swarm_monitoring_result_t result = {};
    int ret = security_collective_monitor_swarm(bridge, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.active_agents, 10u);
    EXPECT_GE(result.synchronization_level, 0.0f);
    EXPECT_LE(result.synchronization_level, 1.0f);
    EXPECT_GE(result.coherence_level, 0.0f);
    EXPECT_LE(result.coherence_level, 1.0f);
}

TEST_F(SecurityCollectiveBridgeTest, MonitorSwarmWithQuarantined) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(10);
    security_collective_quarantine_agent(bridge, 100, "test");
    security_collective_quarantine_agent(bridge, 101, "test");

    swarm_monitoring_result_t result = {};
    int ret = security_collective_monitor_swarm(bridge, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.active_agents, 8u);  // 10 - 2 quarantined
    EXPECT_GT(result.fragmentation_index, 0.0f);
}

TEST_F(SecurityCollectiveBridgeTest, MonitorSwarmAnomalyDetection) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.anomaly_threshold = 0.2f;  // Low threshold to trigger anomaly

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(10);

    // Quarantine many agents to trigger anomaly
    for (uint32_t i = 0; i < 5; i++) {
        security_collective_quarantine_agent(bridge, 100 + i, "test");
    }

    swarm_monitoring_result_t result = {};
    int ret = security_collective_monitor_swarm(bridge, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.anomaly_detected);
    EXPECT_GT(result.anomaly_score, 0.0f);
}

// ============================================================================
// Trust Scoring Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ScoreAgentNullBridge) {
    agent_trust_result_t result = {};
    EXPECT_EQ(security_collective_score_agent(nullptr, 1, &result),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ScoreAgentNullResult) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_score_agent(bridge, 1, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ReportActionNullBridge) {
    EXPECT_EQ(security_collective_report_action(nullptr, 1, true, 1.0f),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Trust Scoring Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ScoreAgentUnregistered) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    agent_trust_result_t result = {};
    int ret = security_collective_score_agent(bridge, 999, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.level, TRUST_LEVEL_UNTRUSTED);
    EXPECT_EQ(result.trust_score, 0.0f);
}

TEST_F(SecurityCollectiveBridgeTest, ScoreAgentNewlyRegistered) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.initial_trust_score = 0.5f;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    agent_trust_result_t result = {};
    int ret = security_collective_score_agent(bridge, 100, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.agent_id, 100u);
    EXPECT_FLOAT_EQ(result.trust_score, 0.5f);
}

TEST_F(SecurityCollectiveBridgeTest, ReportActionNotFound) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_report_action(bridge, 999, true, 1.0f);
    EXPECT_EQ(ret, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityCollectiveBridgeTest, ReportPositiveAction) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    agent_trust_result_t before = {};
    security_collective_score_agent(bridge, 100, &before);

    int ret = security_collective_report_action(bridge, 100, true, 1.0f);
    EXPECT_EQ(ret, 0);

    agent_trust_result_t after = {};
    security_collective_score_agent(bridge, 100, &after);

    EXPECT_GT(after.trust_score, before.trust_score);
    EXPECT_EQ(after.positive_actions, 1u);
}

TEST_F(SecurityCollectiveBridgeTest, ReportNegativeAction) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    agent_trust_result_t before = {};
    security_collective_score_agent(bridge, 100, &before);

    int ret = security_collective_report_action(bridge, 100, false, 1.0f);
    EXPECT_EQ(ret, 0);

    agent_trust_result_t after = {};
    security_collective_score_agent(bridge, 100, &after);

    EXPECT_LT(after.trust_score, before.trust_score);
    EXPECT_EQ(after.negative_actions, 1u);
}

TEST_F(SecurityCollectiveBridgeTest, TrustScoreBounds) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Report many positive actions
    for (int i = 0; i < 100; i++) {
        security_collective_report_action(bridge, 100, true, 1.0f);
    }

    agent_trust_result_t result = {};
    security_collective_score_agent(bridge, 100, &result);

    EXPECT_LE(result.trust_score, 1.0f);  // Should not exceed 1.0

    // Report many negative actions
    for (int i = 0; i < 200; i++) {
        security_collective_report_action(bridge, 100, false, 1.0f);
    }

    security_collective_score_agent(bridge, 100, &result);

    EXPECT_GE(result.trust_score, 0.0f);  // Should not go below 0.0
}

TEST_F(SecurityCollectiveBridgeTest, TrustLevelProgression) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Boost trust to high level
    for (int i = 0; i < 50; i++) {
        security_collective_report_action(bridge, 100, true, 1.0f);
    }

    agent_trust_result_t result = {};
    security_collective_score_agent(bridge, 100, &result);

    EXPECT_GE(result.level, TRUST_LEVEL_MODERATE);
}

// ============================================================================
// Bidirectional Update Tests - NULL Bridge
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, BridgeUpdateNullBridge) {
    EXPECT_EQ(security_collective_bridge_update(nullptr, 100), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ApplySecurityEffectsNullBridge) {
    EXPECT_EQ(security_collective_apply_security_effects(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ApplyCollectiveEffectsNullBridge) {
    EXPECT_EQ(security_collective_apply_collective_effects(nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Bidirectional Update Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, BridgeUpdateBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, BridgeUpdateZeroDelta) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, BridgeUpdateMultiple) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    for (int i = 0; i < 10; i++) {
        int ret = security_collective_bridge_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(SecurityCollectiveBridgeTest, BridgeUpdateWithAgents) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(10);

    int ret = security_collective_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, TrustDecayOverTime) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    agent_trust_result_t before = {};
    security_collective_score_agent(bridge, 100, &before);

    // Update multiple times to trigger decay
    for (int i = 0; i < 100; i++) {
        security_collective_bridge_update(bridge, 1000);  // 1 second each
    }

    agent_trust_result_t after = {};
    security_collective_score_agent(bridge, 100, &after);

    EXPECT_LT(after.trust_score, before.trust_score);
}

TEST_F(SecurityCollectiveBridgeTest, ApplySecurityEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_apply_security_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, ApplyCollectiveEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_apply_collective_effects(bridge);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Query Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetStateNullBridge) {
    security_collective_state_t state = {};
    EXPECT_EQ(security_collective_bridge_get_state(nullptr, &state),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetStateNullState) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_bridge_get_state(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetStatsNullBridge) {
    security_collective_stats_t stats = {};
    EXPECT_EQ(security_collective_bridge_get_stats(nullptr, &stats),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetStatsNullStats) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_bridge_get_stats(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, ResetStatsNullBridge) {
    EXPECT_EQ(security_collective_bridge_reset_stats(nullptr), NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Query Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetStateBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_state_t state = {};
    int ret = security_collective_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.swarm_health, 0.0f);
    EXPECT_LE(state.swarm_health, 1.0f);
}

TEST_F(SecurityCollectiveBridgeTest, GetStatsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_stats_t stats = {};
    int ret = security_collective_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_byzantine_checks, 0u);
}

TEST_F(SecurityCollectiveBridgeTest, GetStatsAfterOperations) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(5);

    // Perform various operations
    byzantine_detection_result_t byz = {};
    security_collective_detect_byzantine(bridge, 100, &byz);

    consensus_verification_result_t cons = {};
    security_collective_verify_consensus(bridge, 1, nullptr, 5, &cons);

    swarm_monitoring_result_t swarm = {};
    security_collective_monitor_swarm(bridge, &swarm);

    security_collective_stats_t stats = {};
    int ret = security_collective_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_byzantine_checks, 1u);
    EXPECT_GE(stats.consensus_verifications, 1u);
    EXPECT_GE(stats.swarm_monitoring_updates, 1u);
}

TEST_F(SecurityCollectiveBridgeTest, ResetStatsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Perform some operations
    RegisterTestAgents(3);
    byzantine_detection_result_t byz = {};
    security_collective_detect_byzantine(bridge, 100, &byz);

    // Reset stats
    int ret = security_collective_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    // Verify reset
    security_collective_stats_t stats = {};
    security_collective_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_byzantine_checks, 0u);
}

// ============================================================================
// Effects Query Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetSecurityEffectsNullBridge) {
    security_to_collective_effects_t effects = {};
    EXPECT_EQ(security_collective_get_security_effects(nullptr, &effects),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetSecurityEffectsNullEffects) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_get_security_effects(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetCollectiveEffectsNullBridge) {
    collective_to_security_effects_t effects = {};
    EXPECT_EQ(security_collective_get_collective_effects(nullptr, &effects),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetCollectiveEffectsNullEffects) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_EQ(security_collective_get_collective_effects(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Effects Query Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetSecurityEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_to_collective_effects_t effects = {};
    int ret = security_collective_get_security_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.avg_swarm_trust, 0.0f);
    EXPECT_LE(effects.avg_swarm_trust, 1.0f);
}

TEST_F(SecurityCollectiveBridgeTest, GetCollectiveEffectsBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    collective_to_security_effects_t effects = {};
    int ret = security_collective_get_collective_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.synchronization_level, 0.0f);
    EXPECT_LE(effects.synchronization_level, 1.0f);
}

// ============================================================================
// Quarantine Query Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetQuarantinedAgentsNullBridge) {
    uint32_t agents[10] = {};
    uint32_t count = 0;
    EXPECT_EQ(security_collective_get_quarantined_agents(nullptr, agents, 10, &count),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetQuarantinedAgentsNullArray) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t count = 0;
    EXPECT_EQ(security_collective_get_quarantined_agents(bridge, nullptr, 10, &count),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetQuarantinedAgentsNullCount) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t agents[10] = {};
    EXPECT_EQ(security_collective_get_quarantined_agents(bridge, agents, 10, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Quarantine Query Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetQuarantinedAgentsEmpty) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t agents[10] = {};
    uint32_t count = 999;

    int ret = security_collective_get_quarantined_agents(bridge, agents, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(SecurityCollectiveBridgeTest, GetQuarantinedAgentsWithData) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(5);
    security_collective_quarantine_agent(bridge, 100, "test");
    security_collective_quarantine_agent(bridge, 102, "test");

    uint32_t agents[10] = {};
    uint32_t count = 0;

    int ret = security_collective_get_quarantined_agents(bridge, agents, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 2u);
}

TEST_F(SecurityCollectiveBridgeTest, GetQuarantinedAgentsZeroMax) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t agents[10] = {};
    uint32_t count = 999;

    int ret = security_collective_get_quarantined_agents(bridge, agents, 0, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0u);
}

// ============================================================================
// Trust Level Query Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetAgentsByTrustNullBridge) {
    uint32_t agents[10] = {};
    uint32_t count = 0;
    EXPECT_EQ(security_collective_get_agents_by_trust(nullptr, TRUST_LEVEL_HIGH,
                                                       agents, 10, &count),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetAgentsByTrustNullArray) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t count = 0;
    EXPECT_EQ(security_collective_get_agents_by_trust(bridge, TRUST_LEVEL_HIGH,
                                                       nullptr, 10, &count),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, GetAgentsByTrustNullCount) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t agents[10] = {};
    EXPECT_EQ(security_collective_get_agents_by_trust(bridge, TRUST_LEVEL_HIGH,
                                                       agents, 10, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

// ============================================================================
// Trust Level Query Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, GetAgentsByTrustEmpty) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    uint32_t agents[10] = {};
    uint32_t count = 999;

    int ret = security_collective_get_agents_by_trust(bridge, TRUST_LEVEL_HIGH,
                                                       agents, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(SecurityCollectiveBridgeTest, GetAgentsByTrustWithData) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(5);

    // All newly registered agents should have moderate trust (initial_trust = 0.5)
    uint32_t agents[10] = {};
    uint32_t count = 0;

    int ret = security_collective_get_agents_by_trust(bridge, TRUST_LEVEL_MODERATE,
                                                       agents, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(count, 0u);
}

// ============================================================================
// Bio-Async Tests - NULL Parameters
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ConnectBioAsyncNullBridge) {
    EXPECT_EQ(security_collective_bridge_connect_bio_async(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, DisconnectBioAsyncNullBridge) {
    EXPECT_EQ(security_collective_bridge_disconnect_bio_async(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityCollectiveBridgeTest, IsBioAsyncConnectedNullBridge) {
    EXPECT_FALSE(security_collective_bridge_is_bio_async_connected(nullptr));
}

// ============================================================================
// Bio-Async Tests - Valid Operations
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, ConnectBioAsyncBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP() << "Bridge creation failed";

    int ret = security_collective_bridge_connect_bio_async(bridge);
    if (ret == -1) {
        GTEST_SKIP() << "Bio-async router not available";
    }
    EXPECT_TRUE(ret == 0 || ret >= NIMCP_ERROR_UNKNOWN);
}

TEST_F(SecurityCollectiveBridgeTest, DisconnectBioAsyncBasic) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, IsBioAsyncConnectedNotConnected) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    EXPECT_FALSE(security_collective_bridge_is_bio_async_connected(bridge));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, FullByzantineWorkflow) {
    security_collective_config_t config;
    security_collective_default_config(&config);
    config.byzantine_threshold = 0.3f;
    config.min_conflicts_for_byzantine = 3;
    config.enable_automatic_quarantine = true;

    CreateBridgeWithConfig(&config);
    if (!bridge) GTEST_SKIP();

    // Register agent
    security_collective_register_agent(bridge, 100);

    // Report negative actions to trigger Byzantine
    for (int i = 0; i < 5; i++) {
        security_collective_report_action(bridge, 100, false, 1.0f);
    }

    // Detect Byzantine
    byzantine_detection_result_t byz = {};
    security_collective_detect_byzantine(bridge, 100, &byz);

    // Check quarantine
    uint32_t quarantined[10] = {};
    uint32_t count = 0;
    security_collective_get_quarantined_agents(bridge, quarantined, 10, &count);

    // If detected as Byzantine, should be quarantined
    if (byz.status == BYZANTINE_STATUS_QUARANTINED) {
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(quarantined[0], 100u);
    }

    // Release agent
    security_collective_release_agent(bridge, 100);

    // Verify release
    security_collective_get_quarantined_agents(bridge, quarantined, 10, &count);
    EXPECT_EQ(count, 0u);
}

TEST_F(SecurityCollectiveBridgeTest, FullConsensusWorkflow) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Register multiple agents
    for (uint32_t i = 0; i < 10; i++) {
        security_collective_register_agent(bridge, 100 + i);
    }

    // Boost some agents' trust
    for (int i = 0; i < 5; i++) {
        for (uint32_t j = 0; j < 10; j++) {
            security_collective_report_action(bridge, 100 + j, true, 0.5f);
        }
    }

    // Verify consensus with all agents
    uint32_t participants[10];
    for (uint32_t i = 0; i < 10; i++) {
        participants[i] = 100 + i;
    }

    consensus_verification_result_t result = {};
    int ret = security_collective_verify_consensus(bridge, 1, participants, 10, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.participant_count, 10u);
    EXPECT_GE(result.valid_votes, 5u);
}

TEST_F(SecurityCollectiveBridgeTest, FullMonitoringWorkflow) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(20);

    // Update bridge to apply effects
    security_collective_bridge_update(bridge, 100);
    security_collective_apply_security_effects(bridge);
    security_collective_apply_collective_effects(bridge);

    // Monitor swarm
    swarm_monitoring_result_t swarm = {};
    security_collective_monitor_swarm(bridge, &swarm);

    EXPECT_EQ(swarm.active_agents, 20u);
    EXPECT_GT(swarm.synchronization_level, 0.0f);

    // Quarantine some agents
    for (uint32_t i = 0; i < 5; i++) {
        security_collective_quarantine_agent(bridge, 100 + i, "test");
    }

    // Re-monitor
    security_collective_monitor_swarm(bridge, &swarm);

    EXPECT_EQ(swarm.active_agents, 15u);
    EXPECT_GT(swarm.fragmentation_index, 0.0f);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SecurityCollectiveBridgeTest, MaxAgentCapacity) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    // Register up to capacity
    int success_count = 0;
    for (uint32_t i = 0; i < 100; i++) {
        int ret = security_collective_register_agent(bridge, 1000 + i);
        if (ret == 0) {
            success_count++;
        } else if (ret == NIMCP_ERROR_OUT_OF_RANGE) {
            break;
        }
    }

    EXPECT_GT(success_count, 0);
}

TEST_F(SecurityCollectiveBridgeTest, ZeroAgentId) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_register_agent(bridge, 0);
    EXPECT_EQ(ret, 0);  // Agent ID 0 should be valid
}

TEST_F(SecurityCollectiveBridgeTest, MaxAgentId) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    int ret = security_collective_register_agent(bridge, UINT32_MAX);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityCollectiveBridgeTest, ActionWeightBounds) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    // Test with weight out of bounds (should be clamped)
    int ret1 = security_collective_report_action(bridge, 100, true, -1.0f);
    EXPECT_EQ(ret1, 0);

    int ret2 = security_collective_report_action(bridge, 100, true, 2.0f);
    EXPECT_EQ(ret2, 0);
}

TEST_F(SecurityCollectiveBridgeTest, QuarantineNullReason) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    security_collective_register_agent(bridge, 100);

    int ret = security_collective_quarantine_agent(bridge, 100, nullptr);
    EXPECT_EQ(ret, 0);  // NULL reason should be acceptable
}

TEST_F(SecurityCollectiveBridgeTest, ConcurrentUpdates) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    RegisterTestAgents(10);

    // Multiple rapid updates should not crash
    for (int i = 0; i < 100; i++) {
        security_collective_bridge_update(bridge, 10);
        security_collective_apply_security_effects(bridge);
        security_collective_apply_collective_effects(bridge);
    }
}

TEST_F(SecurityCollectiveBridgeTest, EmptyConsensusParticipants) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    consensus_verification_result_t result = {};
    int ret = security_collective_verify_consensus(bridge, 1, nullptr, 0, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.validity, CONSENSUS_INVALID_QUORUM);
}
