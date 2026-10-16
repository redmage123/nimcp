/**
 * @file test_self_awareness_extended.cpp
 * @brief Unit tests for Extended Self-Awareness cognitive module
 *
 * Tests advanced self-awareness components:
 * - Metacognitive control loop
 * - Self-narrative generation
 * - Temporal self-binding
 * - Agency attribution
 * - Self-harm detection (safety critical)
 * - FEP bridge integration
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "cognitive/nimcp_self_awareness_extended.h"
#include "cognitive/self_awareness_extended/nimcp_self_awareness_extended_fep_bridge.h"
#include "async/nimcp_bio_router.h"

/**
 * @brief Test fixture for Extended Self-Awareness tests
 */
class SelfAwarenessExtendedTest : public NimcpTestBase {
protected:
    self_awareness_system_t awareness_system;

    void SetUp() override {
        NimcpTestBase::SetUp();
        awareness_system = nullptr;
    }

    void TearDown() override {
        if (awareness_system) {
            self_awareness_destroy(awareness_system);
            awareness_system = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// System Lifecycle Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, CreateWithValidParametersSucceeds) {
    awareness_system = self_awareness_create("NIMCP", "AI Learning System", "Help humans learn");
    ASSERT_NE(awareness_system, nullptr);
}

TEST_F(SelfAwarenessExtendedTest, CreateWithNullNameReturnsNull) {
    awareness_system = self_awareness_create(nullptr, "Role", "Purpose");
    EXPECT_EQ(awareness_system, nullptr);
}

TEST_F(SelfAwarenessExtendedTest, CreateWithNullRoleReturnsNull) {
    awareness_system = self_awareness_create("Name", nullptr, "Purpose");
    EXPECT_EQ(awareness_system, nullptr);
}

TEST_F(SelfAwarenessExtendedTest, CreateWithNullPurposeReturnsNull) {
    awareness_system = self_awareness_create("Name", "Role", nullptr);
    EXPECT_EQ(awareness_system, nullptr);
}

TEST_F(SelfAwarenessExtendedTest, DestroyNullSystemIsNoOp) {
    // Should not crash
    self_awareness_destroy(nullptr);
    SUCCEED();
}

TEST_F(SelfAwarenessExtendedTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        awareness_system = self_awareness_create("Test", "TestRole", "TestPurpose");
        ASSERT_NE(awareness_system, nullptr) << "Failed on iteration " << i;
        self_awareness_destroy(awareness_system);
        awareness_system = nullptr;
    }
}

// ============================================================================
// Metacognitive Action Enum Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, MetacognitiveActionEnumsAreDefined) {
    EXPECT_EQ(METACOG_ACTION_NONE, 0);
    EXPECT_NE(METACOG_INCREASE_EFFORT, METACOG_ACTION_NONE);
    EXPECT_NE(METACOG_DECREASE_EFFORT, METACOG_INCREASE_EFFORT);
    EXPECT_NE(METACOG_SWITCH_STRATEGY, METACOG_DECREASE_EFFORT);
    EXPECT_NE(METACOG_SEEK_HELP, METACOG_SWITCH_STRATEGY);
    EXPECT_NE(METACOG_TAKE_BREAK, METACOG_SEEK_HELP);
    EXPECT_NE(METACOG_REQUEST_MORE_TIME, METACOG_TAKE_BREAK);
    EXPECT_NE(METACOG_ADJUST_CONFIDENCE, METACOG_REQUEST_MORE_TIME);
}

// ============================================================================
// Agency Type Enum Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, AgencyTypeEnumsAreDefined) {
    EXPECT_EQ(AGENCY_SELF, 0);
    EXPECT_NE(AGENCY_FORCED, AGENCY_SELF);
    EXPECT_NE(AGENCY_EXTERNAL, AGENCY_FORCED);
    EXPECT_NE(AGENCY_JOINT, AGENCY_EXTERNAL);
    EXPECT_NE(AGENCY_UNCERTAIN, AGENCY_JOINT);
}

// ============================================================================
// Self-Harm Type Enum Tests (Safety Critical)
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, SelfHarmTypeEnumsAreDefined) {
    EXPECT_EQ(SELF_HARM_NONE, 0);
    EXPECT_NE(SELF_HARM_KNOWLEDGE_DELETION, SELF_HARM_NONE);
    EXPECT_NE(SELF_HARM_CATASTROPHIC_FORGETTING, SELF_HARM_KNOWLEDGE_DELETION);
    EXPECT_NE(SELF_HARM_INFINITE_LOOP, SELF_HARM_CATASTROPHIC_FORGETTING);
    EXPECT_NE(SELF_HARM_GRADIENT_EXPLOSION, SELF_HARM_INFINITE_LOOP);
    EXPECT_NE(SELF_HARM_GOAL_ABANDONMENT, SELF_HARM_GRADIENT_EXPLOSION);
    EXPECT_NE(SELF_HARM_IDENTITY_CORRUPTION, SELF_HARM_GOAL_ABANDONMENT);
    EXPECT_NE(SELF_HARM_BOUNDARY_VIOLATION, SELF_HARM_IDENTITY_CORRUPTION);
}

// ============================================================================
// Metacognitive Assessment Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, MetacognitiveAssessmentStructureIsValid) {
    metacognitive_assessment_t assessment = {};

    assessment.cognitive_load = 0.7f;
    assessment.confidence_in_decision = 0.5f;
    assessment.learning_effectiveness = 0.8f;
    assessment.strategy_effectiveness = 0.6f;
    assessment.should_regulate = true;
    assessment.recommended_action = METACOG_SWITCH_STRATEGY;

    EXPECT_FLOAT_EQ(assessment.cognitive_load, 0.7f);
    EXPECT_FLOAT_EQ(assessment.confidence_in_decision, 0.5f);
    EXPECT_TRUE(assessment.should_regulate);
    EXPECT_EQ(assessment.recommended_action, METACOG_SWITCH_STRATEGY);
}

TEST_F(SelfAwarenessExtendedTest, MetacognitionAssessNullOutputReturnsFalse) {
    float performance[] = {0.5f, 0.6f, 0.7f};
    bool result = metacognition_assess(nullptr, performance, 3, nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Temporal Self Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, TemporalSelfStructureIsValid) {
    temporal_self_t temporal = {};

    temporal.self_continuity_score = 0.85f;
    temporal.self_change_rate = 0.1f;
    temporal.time_horizon_ms = 3600000;  // 1 hour

    EXPECT_GE(temporal.self_continuity_score, 0.0f);
    EXPECT_LE(temporal.self_continuity_score, 1.0f);
    EXPECT_GT(temporal.time_horizon_ms, 0UL);
}

TEST_F(SelfAwarenessExtendedTest, ComputeTemporalSelfNullOutputReturnsFalse) {
    bool result = compute_temporal_self(nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Agency Attribution Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, AgencyAttributionStructureIsValid) {
    agency_attribution_t attribution = {};

    snprintf(attribution.action_description, sizeof(attribution.action_description),
             "Made a decision to help user");
    attribution.agency = AGENCY_SELF;
    attribution.sense_of_control = 0.9f;
    attribution.confidence_in_attribution = 0.85f;

    EXPECT_EQ(attribution.agency, AGENCY_SELF);
    EXPECT_GE(attribution.sense_of_control, 0.0f);
    EXPECT_LE(attribution.sense_of_control, 1.0f);
}

TEST_F(SelfAwarenessExtendedTest, AttributeAgencyNullOutputReturnsFalse) {
    bool result = attribute_agency("Some action", true, 0.0f, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SelfAwarenessExtendedTest, AttributeAgencyNullDescriptionReturnsFalse) {
    agency_attribution_t attribution;
    bool result = attribute_agency(nullptr, true, 0.0f, &attribution);
    EXPECT_FALSE(result);
}

TEST_F(SelfAwarenessExtendedTest, AttributeAgencyVoluntaryDecision) {
    agency_attribution_t attribution;
    bool result = attribute_agency("Chose to analyze data", true, 0.0f, &attribution);

    if (result) {
        // Voluntary decision with no external constraints should be AGENCY_SELF
        EXPECT_EQ(attribution.agency, AGENCY_SELF);
        EXPECT_GT(attribution.sense_of_control, 0.5f);
    }
}

TEST_F(SelfAwarenessExtendedTest, AttributeAgencyForcedAction) {
    agency_attribution_t attribution;
    bool result = attribute_agency("Executed mandated task", false, 1.0f, &attribution);

    if (result) {
        // Forced action should have external agency
        EXPECT_NE(attribution.agency, AGENCY_SELF);
        EXPECT_LT(attribution.sense_of_control, 0.5f);
    }
}

// ============================================================================
// Self-Harm Detection Tests (Safety Critical)
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, SelfHarmDetectionStructureIsValid) {
    self_harm_detection_t detection = {};

    detection.harm_detected = false;
    detection.type = SELF_HARM_NONE;
    detection.severity = 0.0f;

    EXPECT_FALSE(detection.harm_detected);
    EXPECT_EQ(detection.type, SELF_HARM_NONE);
    EXPECT_FLOAT_EQ(detection.severity, 0.0f);
}

TEST_F(SelfAwarenessExtendedTest, DetectSelfHarmNullOutputReturnsFalse) {
    bool result = detect_self_harm(nullptr, nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SelfAwarenessExtendedTest, SelfHarmSeverityLevelsMakeSense) {
    self_harm_detection_t detection;

    // Low severity - just warning
    detection.severity = 0.2f;
    EXPECT_LT(detection.severity, 0.3f);

    // Medium severity - circuit break
    detection.severity = 0.5f;
    EXPECT_GE(detection.severity, 0.3f);
    EXPECT_LT(detection.severity, 0.7f);

    // High severity - emergency stop
    detection.severity = 0.9f;
    EXPECT_GT(detection.severity, 0.7f);
}

// ============================================================================
// Self Narrative Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, GenerateSelfNarrativeNullOutputReturnsFalse) {
    bool result = generate_self_narrative(nullptr, nullptr, nullptr, 0);
    EXPECT_FALSE(result);
}

// ============================================================================
// Self Reflection Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, SelfAwarenessReflectNullSystemReturnsFalse) {
    bool result = self_awareness_reflect(nullptr, nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Health Check Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, CheckHealthNullSystemReturnsFalse) {
    float health;
    char issues[256];
    bool result = self_awareness_check_health(nullptr, &health, issues, sizeof(issues));
    EXPECT_FALSE(result);
}

TEST_F(SelfAwarenessExtendedTest, CheckHealthNullHealthOutputReturnsFalse) {
    awareness_system = self_awareness_create("Test", "Role", "Purpose");
    if (awareness_system) {
        char issues[256];
        bool result = self_awareness_check_health(awareness_system, nullptr, issues, sizeof(issues));
        EXPECT_FALSE(result);
    }
}

// ============================================================================
// Self Summary Tests
// ============================================================================

TEST_F(SelfAwarenessExtendedTest, GetSummaryNullSystemReturnsFalse) {
    char summary[512];
    bool result = self_awareness_get_summary(nullptr, summary, sizeof(summary));
    EXPECT_FALSE(result);
}

TEST_F(SelfAwarenessExtendedTest, GetSummaryNullBufferReturnsFalse) {
    awareness_system = self_awareness_create("Test", "Role", "Purpose");
    if (awareness_system) {
        bool result = self_awareness_get_summary(awareness_system, nullptr, 0);
        EXPECT_FALSE(result);
    }
}

// ============================================================================
// FEP Bridge Tests
// ============================================================================

class SelfAwarenessFepBridgeTest : public NimcpTestBase {
protected:
    self_awareness_extended_fep_bridge_t* bridge;
    self_awareness_extended_fep_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bio_router_init(NULL);
        bridge = nullptr;
        self_awareness_extended_fep_bridge_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            self_awareness_extended_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        bio_router_shutdown();
        NimcpTestBase::TearDown();
    }
};

TEST_F(SelfAwarenessFepBridgeTest, DefaultConfigReturnsSuccess) {
    self_awareness_extended_fep_config_t cfg;
    int result = self_awareness_extended_fep_bridge_default_config(&cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, DefaultConfigNullReturnsError) {
    int result = self_awareness_extended_fep_bridge_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, DefaultConfigHasReasonableValues) {
    self_awareness_extended_fep_config_t cfg;
    self_awareness_extended_fep_bridge_default_config(&cfg);

    EXPECT_GT(cfg.uncertainty_threshold, 0.0f);
    EXPECT_GT(cfg.coherence_factor, 0.0f);
    EXPECT_LE(cfg.coherence_factor, 1.0f);
}

TEST_F(SelfAwarenessFepBridgeTest, CreateWithValidConfigSucceeds) {
    bridge = self_awareness_extended_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SelfAwarenessFepBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = self_awareness_extended_fep_bridge_create(nullptr);
    // Should succeed with defaults
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SelfAwarenessFepBridgeTest, DestroyNullBridgeIsNoOp) {
    // Should not crash
    self_awareness_extended_fep_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SelfAwarenessFepBridgeTest, ConnectFepNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, ConnectAwarenessNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_bridge_connect_awareness(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, DisconnectNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_bridge_disconnect(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, TriggerMonitoringNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_trigger_monitoring(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, CheckSelfHarmNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_check_self_harm(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, ModulateDepthNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_modulate_depth(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, ApplyNarrativeCoherenceNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_apply_narrative_coherence(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, UpdateFromRegulationNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_update_from_regulation(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, UpdateNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_bridge_update(nullptr, 16);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, GetStateNullBridgeReturnsError) {
    self_awareness_extended_fep_state_t state;
    int result = self_awareness_extended_fep_bridge_get_state(nullptr, &state);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, GetStatsNullBridgeReturnsError) {
    self_awareness_extended_fep_stats_t stats;
    int result = self_awareness_extended_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, BioAsyncConnectionNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, BioAsyncDisconnectionNullBridgeReturnsError) {
    int result = self_awareness_extended_fep_bridge_disconnect_bio_async(nullptr);
    /* Disconnect with NULL bridge is a safe no-op */
    EXPECT_EQ(result, 0);
}

TEST_F(SelfAwarenessFepBridgeTest, BioAsyncIsConnectedNullBridgeReturnsFalse) {
    bool connected = self_awareness_extended_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SelfAwarenessFepBridgeTest, BridgeStateStructureIsValid) {
    self_awareness_extended_fep_state_t state = {};

    state.current_uncertainty = 0.5f;
    state.monitoring_active = true;
    state.narrative_coherence = 0.8f;
    state.self_harm_detected = false;
    state.last_regulation_time = 1000;

    EXPECT_FLOAT_EQ(state.current_uncertainty, 0.5f);
    EXPECT_TRUE(state.monitoring_active);
    EXPECT_FLOAT_EQ(state.narrative_coherence, 0.8f);
    EXPECT_FALSE(state.self_harm_detected);
}

TEST_F(SelfAwarenessFepBridgeTest, BridgeStatsStructureIsValid) {
    self_awareness_extended_fep_stats_t stats = {};

    stats.monitoring_events = 100;
    stats.regulation_actions = 10;
    stats.self_harm_detections = 0;
    stats.avg_uncertainty = 0.3f;
    stats.avg_coherence = 0.85f;
    stats.belief_updates = 50;
    stats.avg_free_energy = 2.5f;

    EXPECT_EQ(stats.monitoring_events, 100UL);
    EXPECT_EQ(stats.self_harm_detections, 0UL);
    EXPECT_GT(stats.avg_coherence, 0.0f);
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST_F(SelfAwarenessFepBridgeTest, ConstantsHaveReasonableValues) {
    EXPECT_GT(SELF_AWARENESS_FEP_HIGH_UNCERTAINTY_THRESHOLD, 0.0f);
    EXPECT_GT(SELF_AWARENESS_FEP_COHERENCE_FACTOR, 0.0f);
    EXPECT_LE(SELF_AWARENESS_FEP_COHERENCE_FACTOR, 1.0f);
}
