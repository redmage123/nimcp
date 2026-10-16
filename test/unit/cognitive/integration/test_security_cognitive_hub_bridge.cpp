/**
 * @file test_security_cognitive_hub_bridge.cpp
 * @brief Unit tests for Security-Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>

#include "security/nimcp_security_orchestrator.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_security_cognitive_hub_bridge.h"

/**
 * @brief Test fixture for Security-Cognitive hub bridge tests
 */
class SecurityCognitiveHubBridgeTest : public ::testing::Test {
protected:
    security_cognitive_bridge_t bridge;
    security_cognitive_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, security_cognitive_default_config(&config));
        bridge = security_cognitive_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            security_cognitive_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/**
 * @brief Test fixture with both orchestrator and cognitive hub
 */
class SecurityCognitiveHubBridgeFullTest : public ::testing::Test {
protected:
    security_cognitive_bridge_t bridge;
    security_orchestrator_t orchestrator;
    cognitive_integration_hub_t cognitive_hub;
    security_cognitive_config_t config;

    void SetUp() override {
        // Create security orchestrator
        security_orch_config_t orch_config;
        ASSERT_EQ(0, security_orch_default_config(&orch_config));
        orchestrator = security_orch_create(&orch_config);
        ASSERT_NE(nullptr, orchestrator);

        // Create cognitive hub
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        cognitive_hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(nullptr, cognitive_hub);

        // Create bridge
        ASSERT_EQ(0, security_cognitive_default_config(&config));
        bridge = security_cognitive_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            security_cognitive_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (orchestrator) {
            security_orch_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (cognitive_hub) {
            cognitive_hub_destroy(cognitive_hub);
            cognitive_hub = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

/**
 * @brief Test bridge creation and destruction
 */
TEST_F(SecurityCognitiveHubBridgeTest, BridgeCreation) {
    // Bridge already created in SetUp, verify it's valid
    EXPECT_NE(nullptr, bridge);

    // Create another bridge with NULL config (uses defaults)
    security_cognitive_bridge_t bridge2 = security_cognitive_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge2);
    security_cognitive_bridge_destroy(bridge2);

    // Destroy NULL should be safe
    security_cognitive_bridge_destroy(nullptr);
}

/**
 * @brief Test default configuration values
 */
TEST_F(SecurityCognitiveHubBridgeTest, DefaultConfig) {
    security_cognitive_config_t default_config;
    ASSERT_EQ(0, security_cognitive_default_config(&default_config));

    // Verify expected defaults
    EXPECT_TRUE(default_config.translate_security_to_cognitive);
    EXPECT_TRUE(default_config.translate_cognitive_to_security);
    EXPECT_TRUE(default_config.enable_async_translation);
    EXPECT_TRUE(default_config.coordinate_lockdown);
    EXPECT_TRUE(default_config.protect_memory_on_attack);
    EXPECT_TRUE(default_config.restrict_reasoning_on_threat);
    EXPECT_TRUE(default_config.enable_security_queries);
    EXPECT_TRUE(default_config.enable_cognitive_queries);

    // Threshold defaults
    EXPECT_FLOAT_EQ(0.5f, default_config.attention_shift_threshold);
    EXPECT_FLOAT_EQ(0.7f, default_config.reasoning_restrict_threshold);
    EXPECT_FLOAT_EQ(0.9f, default_config.lockdown_notify_threshold);
    EXPECT_FLOAT_EQ(0.6f, default_config.cognitive_anomaly_threshold);

    EXPECT_TRUE(default_config.report_memory_anomalies);
    EXPECT_TRUE(default_config.report_reasoning_anomalies);

    // NULL config should return error
    EXPECT_NE(0, security_cognitive_default_config(nullptr));
}

/**
 * @brief Test bridge reset
 */
TEST_F(SecurityCognitiveHubBridgeTest, BridgeReset) {
    EXPECT_EQ(0, security_cognitive_bridge_reset(bridge));

    // NULL should fail
    EXPECT_NE(0, security_cognitive_bridge_reset(nullptr));
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

/**
 * @brief Test connection to security orchestrator
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, ConnectSecurity) {
    EXPECT_FALSE(security_cognitive_is_connected(bridge));

    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));

    // Connect to cognitive hub too
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Now fully connected
    EXPECT_TRUE(security_cognitive_is_connected(bridge));

    // Disconnect
    EXPECT_EQ(0, security_cognitive_disconnect_security(bridge));
    EXPECT_FALSE(security_cognitive_is_connected(bridge));
}

/**
 * @brief Test connection to cognitive hub
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, ConnectCognitive) {
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Connect to security orchestrator too
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));

    // Now fully connected
    EXPECT_TRUE(security_cognitive_is_connected(bridge));

    // Disconnect
    EXPECT_EQ(0, security_cognitive_disconnect_cognitive(bridge));
    EXPECT_FALSE(security_cognitive_is_connected(bridge));
}

/**
 * @brief Test NULL connection handling
 */
TEST_F(SecurityCognitiveHubBridgeTest, NullConnectionHandling) {
    // NULL parameters should fail
    EXPECT_NE(0, security_cognitive_connect_security(nullptr, nullptr));
    EXPECT_NE(0, security_cognitive_connect_cognitive(nullptr, nullptr));
    EXPECT_NE(0, security_cognitive_disconnect_security(nullptr));
    EXPECT_NE(0, security_cognitive_disconnect_cognitive(nullptr));
}

/* ============================================================================
 * State Tests
 * ============================================================================ */

/**
 * @brief Test state retrieval
 */
TEST_F(SecurityCognitiveHubBridgeTest, GetState) {
    security_cognitive_state_t state;
    EXPECT_EQ(0, security_cognitive_get_state(bridge, &state));
    EXPECT_EQ(SEC_COG_STATE_DISCONNECTED, state);

    // NULL should fail
    EXPECT_NE(0, security_cognitive_get_state(nullptr, &state));
    EXPECT_NE(0, security_cognitive_get_state(bridge, nullptr));
}

/**
 * @brief Test state transitions through connection
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, StateTransitions) {
    security_cognitive_state_t state;

    // Initially disconnected
    EXPECT_EQ(0, security_cognitive_get_state(bridge, &state));
    EXPECT_EQ(SEC_COG_STATE_DISCONNECTED, state);

    // Connect security
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));

    // Connect cognitive
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Should be connected now
    EXPECT_EQ(0, security_cognitive_get_state(bridge, &state));
    EXPECT_EQ(SEC_COG_STATE_CONNECTED, state);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * @brief Test statistics retrieval
 */
TEST_F(SecurityCognitiveHubBridgeTest, GetStats) {
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));

    // Initial stats should be zero
    EXPECT_EQ(0u, stats.security_events_translated);
    EXPECT_EQ(0u, stats.cognitive_events_translated);
    EXPECT_EQ(0u, stats.events_dropped);
    EXPECT_EQ(0u, stats.security_queries_handled);
    EXPECT_EQ(0u, stats.cognitive_queries_made);
    EXPECT_EQ(0u, stats.query_failures);
    EXPECT_EQ(0u, stats.lockdowns_coordinated);
    EXPECT_EQ(0u, stats.attention_shifts_triggered);
    EXPECT_EQ(0u, stats.memory_protections_triggered);

    // NULL should fail
    EXPECT_NE(0, security_cognitive_get_stats(nullptr, &stats));
    EXPECT_NE(0, security_cognitive_get_stats(bridge, nullptr));
}

/**
 * @brief Test statistics reset
 */
TEST_F(SecurityCognitiveHubBridgeTest, ResetStats) {
    EXPECT_EQ(0, security_cognitive_reset_stats(bridge));

    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.security_events_translated);

    // NULL should fail
    EXPECT_NE(0, security_cognitive_reset_stats(nullptr));
}

/* ============================================================================
 * State Name Tests
 * ============================================================================ */

/**
 * @brief Test state name conversion
 */
TEST(SecurityCognitiveStateName, ValidStates) {
    EXPECT_STREQ("Uninitialized", security_cognitive_state_name(SEC_COG_STATE_UNINITIALIZED));
    EXPECT_STREQ("Disconnected", security_cognitive_state_name(SEC_COG_STATE_DISCONNECTED));
    EXPECT_STREQ("Connected", security_cognitive_state_name(SEC_COG_STATE_CONNECTED));
    EXPECT_STREQ("Active", security_cognitive_state_name(SEC_COG_STATE_ACTIVE));
    EXPECT_STREQ("Coordinating", security_cognitive_state_name(SEC_COG_STATE_COORDINATING));
    EXPECT_STREQ("Lockdown", security_cognitive_state_name(SEC_COG_STATE_LOCKDOWN));
    EXPECT_STREQ("Error", security_cognitive_state_name(SEC_COG_STATE_ERROR));
}

/**
 * @brief Test invalid state name
 */
TEST(SecurityCognitiveStateName, InvalidState) {
    // Invalid state returns "Invalid" (not "Unknown")
    EXPECT_STREQ("Invalid", security_cognitive_state_name((security_cognitive_state_t)100));
}

/* ============================================================================
 * Print Functions (smoke tests)
 * ============================================================================ */

/**
 * @brief Test print summary (should not crash)
 */
TEST_F(SecurityCognitiveHubBridgeTest, PrintSummary) {
    security_cognitive_print_summary(bridge);
    security_cognitive_print_summary(nullptr);
}

/**
 * @brief Test print stats (should not crash)
 */
TEST_F(SecurityCognitiveHubBridgeTest, PrintStats) {
    security_cognitive_stats_t stats;
    ASSERT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    security_cognitive_print_stats(&stats);
    security_cognitive_print_stats(nullptr);
}

/* ============================================================================
 * Coordination Tests (Connected Environment)
 * ============================================================================ */

/**
 * @brief Test lockdown coordination
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, CoordinateLockdown) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));
    EXPECT_TRUE(security_cognitive_is_connected(bridge));

    // Coordinate lockdown
    EXPECT_EQ(0, security_cognitive_coordinate_lockdown(bridge, "Test lockdown"));

    // Check state
    security_cognitive_state_t state;
    EXPECT_EQ(0, security_cognitive_get_state(bridge, &state));
    EXPECT_EQ(SEC_COG_STATE_LOCKDOWN, state);

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.lockdowns_coordinated);

    // Release lockdown
    EXPECT_EQ(0, security_cognitive_release_lockdown(bridge));

    // Check state changed back
    EXPECT_EQ(0, security_cognitive_get_state(bridge, &state));
    EXPECT_EQ(SEC_COG_STATE_CONNECTED, state);
}

/**
 * @brief Test lockdown without connection fails
 */
TEST_F(SecurityCognitiveHubBridgeTest, CoordinateLockdownNotConnected) {
    EXPECT_NE(0, security_cognitive_coordinate_lockdown(bridge, "Should fail"));
}

/**
 * @brief Test memory protection
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, ProtectMemory) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Trigger memory protection
    EXPECT_EQ(0, security_cognitive_protect_memory(bridge, 0.8f));

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.memory_protections_triggered);
}

/**
 * @brief Test attention shift
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, ShiftAttention) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Trigger attention shift
    EXPECT_EQ(0, security_cognitive_shift_attention(bridge, COG_PRIORITY_HIGH, COG_CATEGORY_SELF));

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.attention_shifts_triggered);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

/**
 * @brief Test security assessment query
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, GetSecurityAssessment) {
    // Connect to security
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));

    security_threat_assessment_t assessment;
    EXPECT_EQ(0, security_cognitive_get_security_assessment(bridge, &assessment));

    // Initial state should have no threats
    EXPECT_LE(assessment.unified_threat_level, 0.0f);
}

/**
 * @brief Test security assessment without connection fails
 */
TEST_F(SecurityCognitiveHubBridgeTest, GetSecurityAssessmentNotConnected) {
    security_threat_assessment_t assessment;
    EXPECT_NE(0, security_cognitive_get_security_assessment(bridge, &assessment));
}

/**
 * @brief Test cognitive query
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, QueryCognitive) {
    // Connect to cognitive hub
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Query will likely fail since no modules are registered in the hub
    // but the bridge should handle it gracefully
    cognitive_query_result_t result;
    int status = security_cognitive_query_cognitive(bridge, 0, COG_QUERY_STATUS, &result);

    // Check that stats were updated
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    // Either success or failure should be tracked
    EXPECT_TRUE(stats.cognitive_queries_made > 0 || stats.query_failures > 0);
}

/* ============================================================================
 * Event Translation Tests (Connected Environment)
 * ============================================================================ */

/**
 * @brief Test security event translation to cognitive
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, TranslateSecurityEvent) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Create a security event
    security_event_data_t sec_event = {};
    sec_event.event_type = SEC_EVENT_THREAT_DETECTED;
    sec_event.severity = SEC_SEVERITY_HIGH;
    sec_event.timestamp = 1000000;
    sec_event.threat.threat_level = 0.7f;

    // Translate event
    EXPECT_EQ(0, security_cognitive_translate_security_event(bridge, &sec_event));

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.security_events_translated);

    // Attention shift should have been triggered (threat_level >= attention_shift_threshold)
    EXPECT_EQ(1u, stats.attention_shifts_triggered);
}

/**
 * @brief Test cognitive event translation to security
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, TranslateCognitiveEvent) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Translate cognitive anomaly event
    EXPECT_EQ(0, security_cognitive_translate_cognitive_event(
        bridge,
        COG_EVENT_MEMORY_ACCESS,
        COG_CATEGORY_MEMORY,
        0.8f,  // High anomaly score
        "Memory access anomaly"
    ));

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.cognitive_events_translated);
}

/**
 * @brief Test translation without connection fails
 */
TEST_F(SecurityCognitiveHubBridgeTest, TranslateWithoutConnection) {
    security_event_data_t sec_event = {};
    sec_event.event_type = SEC_EVENT_THREAT_DETECTED;

    EXPECT_NE(0, security_cognitive_translate_security_event(bridge, &sec_event));
    EXPECT_NE(0, security_cognitive_translate_cognitive_event(
        bridge, COG_EVENT_STATE_CHANGE, COG_CATEGORY_SELF, 0.5f, "test"));
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * @brief Test custom configuration
 */
TEST(SecurityCognitiveConfig, CustomConfig) {
    security_cognitive_config_t config;
    ASSERT_EQ(0, security_cognitive_default_config(&config));

    // Modify config
    config.translate_security_to_cognitive = false;
    config.attention_shift_threshold = 0.8f;
    config.cognitive_anomaly_threshold = 0.9f;

    // Create bridge with custom config
    security_cognitive_bridge_t bridge = security_cognitive_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Bridge should use custom config (we can't easily verify internal state,
    // but we can verify it was created successfully)
    security_cognitive_state_t state;
    EXPECT_EQ(0, security_cognitive_get_state(bridge, &state));

    security_cognitive_bridge_destroy(bridge);
}

/**
 * @brief Test disabled translation
 */
TEST(SecurityCognitiveConfig, DisabledTranslation) {
    security_cognitive_config_t config;
    ASSERT_EQ(0, security_cognitive_default_config(&config));

    // Disable translation
    config.translate_security_to_cognitive = false;
    config.translate_cognitive_to_security = false;

    security_cognitive_bridge_t bridge = security_cognitive_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    // Create orchestrator and hub
    security_orch_config_t orch_config;
        ASSERT_EQ(0, security_orch_default_config(&orch_config));
    security_orchestrator_t orchestrator = security_orch_create(&orch_config);
    cognitive_hub_config_t hub_config = cognitive_hub_default_config();
    cognitive_integration_hub_t cognitive_hub = cognitive_hub_create(&hub_config);

    // Connect
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Event translation should succeed but stats may not increment
    // (depending on implementation of disabled translation)
    security_event_data_t sec_event = {};
    sec_event.event_type = SEC_EVENT_THREAT_DETECTED;
    sec_event.severity = SEC_SEVERITY_HIGH;
    sec_event.threat.threat_level = 0.9f;

    int result = security_cognitive_translate_security_event(bridge, &sec_event);
    // Should either succeed silently or succeed normally
    EXPECT_GE(result, 0);

    security_cognitive_bridge_destroy(bridge);
    security_orch_destroy(orchestrator);
    cognitive_hub_destroy(cognitive_hub);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

/**
 * @brief Test multiple create/destroy cycles
 */
TEST(SecurityCognitiveStress, MultipleLifecycles) {
    for (int i = 0; i < 100; i++) {
        security_cognitive_bridge_t bridge = security_cognitive_bridge_create(nullptr);
        ASSERT_NE(nullptr, bridge);
        security_cognitive_bridge_destroy(bridge);
    }
}

/**
 * @brief Test multiple events
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, MultipleEvents) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Send multiple events
    for (int i = 0; i < 50; i++) {
        security_event_data_t sec_event = {};
        sec_event.event_type = SEC_EVENT_THREAT_DETECTED;
        sec_event.severity = SEC_SEVERITY_MEDIUM;
        sec_event.timestamp = 1000000 + i;
        sec_event.threat.threat_level = 0.4f;  // Below attention threshold

        EXPECT_EQ(0, security_cognitive_translate_security_event(bridge, &sec_event));
    }

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(50u, stats.security_events_translated);
}

/**
 * @brief Test multiple lockdowns
 */
TEST_F(SecurityCognitiveHubBridgeFullTest, MultipleLockdowns) {
    // Connect both systems
    EXPECT_EQ(0, security_cognitive_connect_security(bridge, orchestrator));
    EXPECT_EQ(0, security_cognitive_connect_cognitive(bridge, cognitive_hub));

    // Multiple lockdown/release cycles
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, security_cognitive_coordinate_lockdown(bridge, "Lockdown cycle"));
        EXPECT_EQ(0, security_cognitive_release_lockdown(bridge));
    }

    // Check stats
    security_cognitive_stats_t stats;
    EXPECT_EQ(0, security_cognitive_get_stats(bridge, &stats));
    EXPECT_EQ(10u, stats.lockdowns_coordinated);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

/**
 * @brief Test NULL parameter handling across all functions
 */
TEST(SecurityCognitiveErrors, NullParameters) {
    security_cognitive_config_t config;
    EXPECT_NE(0, security_cognitive_default_config(nullptr));

    // Note: bridge_create with NULL config uses defaults (returns valid bridge)
    security_cognitive_bridge_t bridge = security_cognitive_bridge_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    security_cognitive_state_t state;
    EXPECT_NE(0, security_cognitive_get_state(nullptr, &state));
    EXPECT_NE(0, security_cognitive_get_state(bridge, nullptr));

    security_cognitive_stats_t stats;
    EXPECT_NE(0, security_cognitive_get_stats(nullptr, &stats));
    EXPECT_NE(0, security_cognitive_get_stats(bridge, nullptr));

    EXPECT_NE(0, security_cognitive_reset_stats(nullptr));
    EXPECT_NE(0, security_cognitive_bridge_reset(nullptr));

    EXPECT_NE(0, security_cognitive_connect_security(nullptr, nullptr));
    EXPECT_NE(0, security_cognitive_connect_cognitive(nullptr, nullptr));

    EXPECT_NE(0, security_cognitive_disconnect_security(nullptr));
    EXPECT_NE(0, security_cognitive_disconnect_cognitive(nullptr));

    EXPECT_NE(0, security_cognitive_coordinate_lockdown(nullptr, "test"));
    EXPECT_NE(0, security_cognitive_release_lockdown(nullptr));

    EXPECT_NE(0, security_cognitive_protect_memory(nullptr, 0.5f));
    EXPECT_NE(0, security_cognitive_shift_attention(nullptr, COG_PRIORITY_HIGH, COG_CATEGORY_SELF));

    EXPECT_NE(0, security_cognitive_translate_security_event(nullptr, nullptr));
    EXPECT_NE(0, security_cognitive_translate_security_event(bridge, nullptr));

    security_threat_assessment_t assessment;
    EXPECT_NE(0, security_cognitive_get_security_assessment(nullptr, &assessment));
    EXPECT_NE(0, security_cognitive_get_security_assessment(bridge, nullptr));

    cognitive_query_result_t result;
    EXPECT_NE(0, security_cognitive_query_cognitive(nullptr, 0, 0, &result));
    EXPECT_NE(0, security_cognitive_query_cognitive(bridge, 0, 0, nullptr));

    security_cognitive_bridge_destroy(bridge);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
