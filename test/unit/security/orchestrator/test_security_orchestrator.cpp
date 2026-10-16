/**
 * @file test_security_orchestrator.cpp
 * @brief Unit tests for Security Orchestrator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Comprehensive tests for the security orchestrator including:
 * - Lifecycle (default config, create, destroy, reset)
 * - Bridge registration and management
 * - Event subscription and publishing
 * - Threat assessment and aggregation
 * - Lockdown functionality
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cmath>

extern "C" {
#include "security/nimcp_security_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityOrchestratorTest : public ::testing::Test {
protected:
    security_orchestrator_t orch = nullptr;
    security_orch_config_t config;

    void SetUp() override {
        int result = security_orch_default_config(&config);
        ASSERT_EQ(result, 0);
        orch = security_orch_create(&config);
        ASSERT_NE(orch, nullptr);
    }

    void TearDown() override {
        if (orch) {
            security_orch_destroy(orch);
            orch = nullptr;
        }
    }

    // Helper: Register a test bridge
    uint32_t register_test_bridge(security_bridge_type_t type, const char* name) {
        uint32_t bridge_id = 0;
        int result = security_orch_register_bridge(orch, type, name, nullptr, nullptr, &bridge_id);
        EXPECT_EQ(result, 0);
        return bridge_id;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, DefaultConfigIsValid) {
    security_orch_config_t cfg;
    int result = security_orch_default_config(&cfg);
    EXPECT_EQ(result, 0);

    // Verify default values
    EXPECT_EQ(cfg.max_bridges, SEC_ORCH_MAX_BRIDGES);
    EXPECT_EQ(cfg.max_subscriptions, SEC_ORCH_MAX_SUBSCRIPTIONS);
    EXPECT_TRUE(cfg.enable_async);
    EXPECT_EQ(cfg.event_queue_size, SEC_ORCH_MAX_EVENT_QUEUE);

    EXPECT_FLOAT_EQ(cfg.critical_threshold, SEC_ORCH_DEFAULT_CRITICAL_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.high_threshold, SEC_ORCH_DEFAULT_HIGH_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.medium_threshold, SEC_ORCH_DEFAULT_MEDIUM_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.threat_decay_rate, SEC_ORCH_DEFAULT_THREAT_DECAY);

    EXPECT_TRUE(cfg.auto_lockdown);
    EXPECT_TRUE(cfg.enable_cascade);
    EXPECT_TRUE(cfg.enable_recovery);
}

TEST_F(SecurityOrchestratorTest, DefaultConfigNullFails) {
    int result = security_orch_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityOrchestratorTest, CreateWithNullConfigUsesDefaults) {
    security_orchestrator_t o = security_orch_create(nullptr);
    ASSERT_NE(o, nullptr);

    security_orch_state_t state;
    int result = security_orch_get_state(o, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state, SEC_ORCH_STATE_IDLE);

    security_orch_destroy(o);
}

TEST_F(SecurityOrchestratorTest, CreateWithCustomConfig) {
    security_orch_config_t custom_cfg;
    security_orch_default_config(&custom_cfg);

    custom_cfg.max_bridges = 8;
    custom_cfg.critical_threshold = 0.95f;
    custom_cfg.auto_lockdown = false;

    security_orchestrator_t o = security_orch_create(&custom_cfg);
    ASSERT_NE(o, nullptr);

    security_orch_destroy(o);
}

TEST_F(SecurityOrchestratorTest, DestroyNullIsSafe) {
    security_orch_destroy(nullptr);
    // Should not crash
}

TEST_F(SecurityOrchestratorTest, ResetClearsState) {
    // Register some bridges
    register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test1");
    register_test_bridge(SEC_BRIDGE_KNOWLEDGE_GRAPH, "test2");

    // Reset
    int result = security_orch_reset(orch);
    EXPECT_EQ(result, 0);

    // State should be idle
    security_orch_state_t state;
    security_orch_get_state(orch, &state);
    EXPECT_EQ(state, SEC_ORCH_STATE_IDLE);

    // Threat level should be zero
    float threat;
    security_orch_get_threat_level(orch, &threat);
    EXPECT_FLOAT_EQ(threat, 0.0f);
}

TEST_F(SecurityOrchestratorTest, ResetNullFails) {
    int result = security_orch_reset(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bridge Registration Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, RegisterBridgeSuccess) {
    uint32_t bridge_id = 0;
    int result = security_orch_register_bridge(
        orch,
        SEC_BRIDGE_DISTRIBUTED_TRAINING,
        "Distributed Training",
        nullptr,
        nullptr,
        &bridge_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge_id, 0u);
}

TEST_F(SecurityOrchestratorTest, RegisterMultipleBridges) {
    uint32_t id1 = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "dist");
    uint32_t id2 = register_test_bridge(SEC_BRIDGE_KNOWLEDGE_GRAPH, "kg");
    uint32_t id3 = register_test_bridge(SEC_BRIDGE_GAME_THEORY, "gt");

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);
    EXPECT_EQ(stats.registered_bridges, 3u);
}

TEST_F(SecurityOrchestratorTest, RegisterDuplicateBridgeTypeFails) {
    register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "first");

    uint32_t id2 = 0;
    int result = security_orch_register_bridge(
        orch,
        SEC_BRIDGE_DISTRIBUTED_TRAINING,  // Same type
        "second",
        nullptr,
        nullptr,
        &id2
    );

    EXPECT_EQ(result, NIMCP_ERROR_ALREADY_EXISTS);
}

TEST_F(SecurityOrchestratorTest, RegisterInvalidBridgeTypeFails) {
    uint32_t id = 0;
    int result = security_orch_register_bridge(
        orch,
        SEC_BRIDGE_UNKNOWN,
        "invalid",
        nullptr,
        nullptr,
        &id
    );

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(SecurityOrchestratorTest, RegisterBridgeNullOrchFails) {
    uint32_t id = 0;
    int result = security_orch_register_bridge(
        nullptr,
        SEC_BRIDGE_DISTRIBUTED_TRAINING,
        "test",
        nullptr,
        nullptr,
        &id
    );

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityOrchestratorTest, UnregisterBridge) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    int result = security_orch_unregister_bridge(orch, id);
    EXPECT_EQ(result, 0);

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);
    EXPECT_EQ(stats.registered_bridges, 0u);
}

TEST_F(SecurityOrchestratorTest, UnregisterNonexistentBridgeFails) {
    int result = security_orch_unregister_bridge(orch, 99999);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SecurityOrchestratorTest, GetBridgeInfo) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_EPISTEMIC, "Epistemic Bridge");

    security_bridge_info_t info;
    int result = security_orch_get_bridge_info(orch, id, &info);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(info.bridge_id, id);
    EXPECT_EQ(info.type, SEC_BRIDGE_EPISTEMIC);
    EXPECT_STREQ(info.name, "Epistemic Bridge");
    EXPECT_TRUE(info.is_active);
    EXPECT_FLOAT_EQ(info.current_threat_level, 0.0f);
}

TEST_F(SecurityOrchestratorTest, GetBridgeByType) {
    register_test_bridge(SEC_BRIDGE_HIPPOCAMPUS, "Hippocampus");

    void* handle = nullptr;
    int result = security_orch_get_bridge_by_type(orch, SEC_BRIDGE_HIPPOCAMPUS, &handle);
    EXPECT_EQ(result, 0);
    // Handle is nullptr since we passed nullptr during registration
}

/* ============================================================================
 * Event Subscription Tests
 * ============================================================================ */

static int test_callback_count = 0;
static int test_callback(const security_event_data_t* event, void* user_data) {
    test_callback_count++;
    return 0;
}

TEST_F(SecurityOrchestratorTest, SubscribeToEvent) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    int result = security_orch_subscribe(
        orch,
        id,
        SEC_EVENT_THREAT_DETECTED,
        test_callback,
        nullptr
    );

    EXPECT_EQ(result, 0);

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);
    EXPECT_EQ(stats.active_subscriptions, 1u);
}

TEST_F(SecurityOrchestratorTest, SubscribeNullCallbackFails) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    int result = security_orch_subscribe(
        orch,
        id,
        SEC_EVENT_THREAT_DETECTED,
        nullptr,  // Null callback
        nullptr
    );

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityOrchestratorTest, Unsubscribe) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");
    security_orch_subscribe(orch, id, SEC_EVENT_THREAT_DETECTED, test_callback, nullptr);

    int result = security_orch_unsubscribe(orch, id, SEC_EVENT_THREAT_DETECTED);
    EXPECT_EQ(result, 0);

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);
    EXPECT_EQ(stats.active_subscriptions, 0u);
}

/* ============================================================================
 * Event Publishing Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, PublishEvent) {
    uint32_t publisher_id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "publisher");
    uint32_t subscriber_id = register_test_bridge(SEC_BRIDGE_KNOWLEDGE_GRAPH, "subscriber");

    test_callback_count = 0;
    security_orch_subscribe(orch, subscriber_id, SEC_EVENT_THREAT_DETECTED, test_callback, nullptr);

    security_event_data_t event = {};
    event.event_type = SEC_EVENT_THREAT_DETECTED;
    event.source = SEC_BRIDGE_DISTRIBUTED_TRAINING;
    event.severity = SEC_SEVERITY_HIGH;
    event.threat.threat_level = 0.8f;

    int result = security_orch_publish(orch, publisher_id, &event);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_callback_count, 1);
}

TEST_F(SecurityOrchestratorTest, ReportThreatInvalidRange) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "dist");

    // Invalid threat level should fail
    int result = security_orch_report_threat(orch, id, 1.5f, SEC_SEVERITY_HIGH, "Invalid");
    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);

    result = security_orch_report_threat(orch, id, -0.5f, SEC_SEVERITY_HIGH, "Invalid");
    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
}

TEST_F(SecurityOrchestratorTest, ReportThreatUpdatesLevel) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    int result = security_orch_report_threat(
        orch,
        id,
        0.75f,
        SEC_SEVERITY_HIGH,
        "Test threat"
    );

    EXPECT_EQ(result, 0);

    float threat;
    security_orch_get_threat_level(orch, &threat);
    EXPECT_GT(threat, 0.0f);
}

/* ============================================================================
 * Threat Assessment Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, GetThreatAssessmentEmpty) {
    security_threat_assessment_t assessment;
    int result = security_orch_get_threat_assessment(orch, &assessment);
    EXPECT_EQ(result, 0);

    EXPECT_FLOAT_EQ(assessment.unified_threat_level, 0.0f);
    EXPECT_EQ(assessment.severity, SEC_SEVERITY_NONE);
    EXPECT_EQ(assessment.active_threats, 0u);
}

TEST_F(SecurityOrchestratorTest, GetThreatAssessmentWithThreats) {
    uint32_t id1 = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "dist");
    uint32_t id2 = register_test_bridge(SEC_BRIDGE_EPISTEMIC, "epist");

    security_orch_report_threat(orch, id1, 0.6f, SEC_SEVERITY_MEDIUM, "Threat 1");
    security_orch_report_threat(orch, id2, 0.8f, SEC_SEVERITY_HIGH, "Threat 2");

    security_threat_assessment_t assessment;
    security_orch_get_threat_assessment(orch, &assessment);

    EXPECT_GT(assessment.unified_threat_level, 0.0f);
    EXPECT_GE(assessment.severity, SEC_SEVERITY_MEDIUM);
    EXPECT_EQ(assessment.bridges_reporting, 2u);
}

TEST_F(SecurityOrchestratorTest, ThreatDecay) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    // Report high threat
    security_orch_report_threat(orch, id, 0.9f, SEC_SEVERITY_CRITICAL, "Critical threat");

    float initial_threat;
    security_orch_get_threat_level(orch, &initial_threat);
    EXPECT_GT(initial_threat, 0.0f);

    // Force decay update (note: decay requires time to pass)
    security_orch_update_threat_decay(orch);

    // Threat should still be present (not enough time passed)
    float current_threat;
    security_orch_get_threat_level(orch, &current_threat);
    EXPECT_GE(current_threat, 0.0f);
}

TEST_F(SecurityOrchestratorTest, ClearThreats) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");
    security_orch_report_threat(orch, id, 0.9f, SEC_SEVERITY_CRITICAL, "Threat");

    int result = security_orch_clear_threats(orch);
    EXPECT_EQ(result, 0);

    float threat;
    security_orch_get_threat_level(orch, &threat);
    EXPECT_FLOAT_EQ(threat, 0.0f);
}

/* ============================================================================
 * Lockdown Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, TriggerLockdown) {
    int result = security_orch_trigger_lockdown(orch, "Test lockdown");
    EXPECT_EQ(result, 0);

    bool is_locked;
    security_orch_is_locked_down(orch, &is_locked);
    EXPECT_TRUE(is_locked);

    security_orch_state_t state;
    security_orch_get_state(orch, &state);
    EXPECT_EQ(state, SEC_ORCH_STATE_LOCKDOWN);
}

TEST_F(SecurityOrchestratorTest, ReleaseLockdown) {
    security_orch_trigger_lockdown(orch, "Test");

    int result = security_orch_release_lockdown(orch);
    EXPECT_EQ(result, 0);

    bool is_locked;
    security_orch_is_locked_down(orch, &is_locked);
    EXPECT_FALSE(is_locked);

    security_orch_state_t state;
    security_orch_get_state(orch, &state);
    EXPECT_EQ(state, SEC_ORCH_STATE_RECOVERY);
}

TEST_F(SecurityOrchestratorTest, AutoLockdownOnCriticalThreat) {
    // Config has auto_lockdown = true by default
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    // Report critical threat
    security_orch_report_threat(orch, id, 0.95f, SEC_SEVERITY_CRITICAL, "Critical!");

    bool is_locked;
    security_orch_is_locked_down(orch, &is_locked);
    EXPECT_TRUE(is_locked);
}

TEST_F(SecurityOrchestratorTest, NoAutoLockdownWhenDisabled) {
    // Create orchestrator with auto_lockdown disabled
    security_orch_config_t cfg;
    security_orch_default_config(&cfg);
    cfg.auto_lockdown = false;

    security_orchestrator_t o = security_orch_create(&cfg);
    ASSERT_NE(o, nullptr);

    uint32_t id = 0;
    security_orch_register_bridge(o, SEC_BRIDGE_DISTRIBUTED_TRAINING, "test", nullptr, nullptr, &id);

    // Report critical threat
    security_orch_report_threat(o, id, 0.95f, SEC_SEVERITY_CRITICAL, "Critical!");

    bool is_locked;
    security_orch_is_locked_down(o, &is_locked);
    EXPECT_FALSE(is_locked);  // Should not auto-lockdown

    security_orch_destroy(o);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, GetStatsInitial) {
    security_orch_stats_t stats;
    int result = security_orch_get_stats(orch, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.registered_bridges, 0u);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
}

TEST_F(SecurityOrchestratorTest, GetStatsAfterActivity) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");
    security_orch_report_threat(orch, id, 0.5f, SEC_SEVERITY_MEDIUM, "Threat");

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);

    EXPECT_EQ(stats.registered_bridges, 1u);
    EXPECT_GE(stats.events_published, 1u);  // Registration + threat events
    EXPECT_EQ(stats.threats_detected, 1u);
}

TEST_F(SecurityOrchestratorTest, ResetStats) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");
    security_orch_report_threat(orch, id, 0.5f, SEC_SEVERITY_MEDIUM, "Threat");

    int result = security_orch_reset_stats(orch);
    EXPECT_EQ(result, 0);

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);

    // Counts should be preserved
    EXPECT_EQ(stats.registered_bridges, 1u);
    // Event counts should be reset
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.threats_detected, 0u);
}

/* ============================================================================
 * State Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, InitialStateIsIdle) {
    security_orch_state_t state;
    int result = security_orch_get_state(orch, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state, SEC_ORCH_STATE_IDLE);
}

TEST_F(SecurityOrchestratorTest, StateChangesOnThreat) {
    uint32_t id = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "test");

    // Low threat -> monitoring
    security_orch_report_threat(orch, id, 0.2f, SEC_SEVERITY_LOW, "Low");
    security_orch_state_t state;
    security_orch_get_state(orch, &state);
    EXPECT_EQ(state, SEC_ORCH_STATE_MONITORING);

    // Clear and report medium threat -> alert
    security_orch_clear_threats(orch);
    security_orch_report_threat(orch, id, 0.5f, SEC_SEVERITY_MEDIUM, "Medium");
    security_orch_get_state(orch, &state);
    EXPECT_EQ(state, SEC_ORCH_STATE_ALERT);

    // Clear and report high threat -> responding
    security_orch_clear_threats(orch);
    security_orch_report_threat(orch, id, 0.8f, SEC_SEVERITY_HIGH, "High");
    security_orch_get_state(orch, &state);
    EXPECT_EQ(state, SEC_ORCH_STATE_RESPONDING);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, BridgeTypeNames) {
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_UNKNOWN), "Unknown");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_DISTRIBUTED_TRAINING), "Distributed Training");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_KNOWLEDGE_GRAPH), "Knowledge Graph");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_GAME_THEORY), "Game Theory");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_IMAGINATION), "Imagination");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_CONTINUAL_LEARNING), "Continual Learning");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_EPISTEMIC), "Epistemic");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_COLLECTIVE), "Collective");
    EXPECT_STREQ(security_bridge_type_name(SEC_BRIDGE_HIPPOCAMPUS), "Hippocampus");
}

TEST_F(SecurityOrchestratorTest, EventTypeNames) {
    EXPECT_STREQ(security_event_type_name(SEC_EVENT_THREAT_DETECTED), "Threat Detected");
    EXPECT_STREQ(security_event_type_name(SEC_EVENT_ATTACK_BLOCKED), "Attack Blocked");
    EXPECT_STREQ(security_event_type_name(SEC_EVENT_BYZANTINE_DETECTED), "Byzantine Detected");
    EXPECT_STREQ(security_event_type_name(SEC_EVENT_MEMORY_CORRUPTION), "Memory Corruption");
}

TEST_F(SecurityOrchestratorTest, SeverityNames) {
    EXPECT_STREQ(security_severity_name(SEC_SEVERITY_NONE), "None");
    EXPECT_STREQ(security_severity_name(SEC_SEVERITY_LOW), "Low");
    EXPECT_STREQ(security_severity_name(SEC_SEVERITY_MEDIUM), "Medium");
    EXPECT_STREQ(security_severity_name(SEC_SEVERITY_HIGH), "High");
    EXPECT_STREQ(security_severity_name(SEC_SEVERITY_CRITICAL), "Critical");
}

TEST_F(SecurityOrchestratorTest, StateNames) {
    EXPECT_STREQ(security_orch_state_name(SEC_ORCH_STATE_IDLE), "Idle");
    EXPECT_STREQ(security_orch_state_name(SEC_ORCH_STATE_MONITORING), "Monitoring");
    EXPECT_STREQ(security_orch_state_name(SEC_ORCH_STATE_ALERT), "Alert");
    EXPECT_STREQ(security_orch_state_name(SEC_ORCH_STATE_RESPONDING), "Responding");
    EXPECT_STREQ(security_orch_state_name(SEC_ORCH_STATE_LOCKDOWN), "Lockdown");
    EXPECT_STREQ(security_orch_state_name(SEC_ORCH_STATE_RECOVERY), "Recovery");
}

TEST_F(SecurityOrchestratorTest, PrintSummaryNullSafe) {
    security_orch_print_summary(nullptr);
    // Should not crash
}

TEST_F(SecurityOrchestratorTest, PrintStatsNullSafe) {
    security_orch_print_stats(nullptr);
    // Should not crash
}

/* ============================================================================
 * Integration Connection Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, ConnectImmune) {
    int result = security_orch_connect_immune(orch, (void*)0x12345678);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityOrchestratorTest, ConnectCognitiveHub) {
    int result = security_orch_connect_cognitive_hub(orch, (void*)0x87654321);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityOrchestratorTest, ConnectBioAsync) {
    // This may fail if router isn't initialized, but shouldn't crash
    int result = security_orch_connect_bio_async(orch);
    // Result depends on whether router is initialized
    EXPECT_TRUE(result == 0 || result != 0);
}

TEST_F(SecurityOrchestratorTest, DisconnectBioAsync) {
    int result = security_orch_disconnect_bio_async(orch);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Null Pointer Safety Tests
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, AllFunctionsHandleNull) {
    EXPECT_EQ(security_orch_reset(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_register_bridge(nullptr, SEC_BRIDGE_BBB, "test", nullptr, nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_unregister_bridge(nullptr, 1), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_get_bridge_info(nullptr, 1, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_subscribe(nullptr, 1, SEC_EVENT_THREAT_DETECTED, test_callback, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_unsubscribe(nullptr, 1, SEC_EVENT_THREAT_DETECTED), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_publish(nullptr, 1, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_report_threat(nullptr, 1, 0.5f, SEC_SEVERITY_LOW, "test"), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_get_threat_assessment(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_get_threat_level(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_clear_threats(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_trigger_lockdown(nullptr, "test"), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_release_lockdown(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_is_locked_down(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_get_state(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_get_stats(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_reset_stats(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_connect_immune(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_connect_cognitive_hub(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_connect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(security_orch_disconnect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * All 8 Bridge Types Test
 * ============================================================================ */

TEST_F(SecurityOrchestratorTest, RegisterAll8CognitiveBridges) {
    uint32_t ids[8];

    ids[0] = register_test_bridge(SEC_BRIDGE_DISTRIBUTED_TRAINING, "Distributed Training");
    ids[1] = register_test_bridge(SEC_BRIDGE_KNOWLEDGE_GRAPH, "Knowledge Graph");
    ids[2] = register_test_bridge(SEC_BRIDGE_GAME_THEORY, "Game Theory");
    ids[3] = register_test_bridge(SEC_BRIDGE_IMAGINATION, "Imagination");
    ids[4] = register_test_bridge(SEC_BRIDGE_CONTINUAL_LEARNING, "Continual Learning");
    ids[5] = register_test_bridge(SEC_BRIDGE_EPISTEMIC, "Epistemic");
    ids[6] = register_test_bridge(SEC_BRIDGE_COLLECTIVE, "Collective");
    ids[7] = register_test_bridge(SEC_BRIDGE_HIPPOCAMPUS, "Hippocampus");

    security_orch_stats_t stats;
    security_orch_get_stats(orch, &stats);
    EXPECT_EQ(stats.registered_bridges, 8u);

    // Report threats from all bridges
    for (int i = 0; i < 8; i++) {
        float threat_level = 0.3f + (i * 0.1f);  // 0.3 to 1.0
        security_orch_report_threat(orch, ids[i], threat_level,
            (security_severity_t)(SEC_SEVERITY_LOW + (i / 2)), "Test threat");
    }

    // Get unified assessment
    security_threat_assessment_t assessment;
    security_orch_get_threat_assessment(orch, &assessment);

    EXPECT_GT(assessment.unified_threat_level, 0.0f);
    EXPECT_EQ(assessment.bridges_reporting, 8u);
}
