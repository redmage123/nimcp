/**
 * @file test_mirror_empathy_bridge.cpp
 * @brief Unit tests for Mirror Neurons - Empathetic Response Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for Mirror-Empathy Hub Bridge
 * WHY:  Ensure mirror neuron action understanding drives empathetic responses correctly
 * HOW:  Test lifecycle, registration, events, callbacks, and statistics
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Configuration Validation
 * - Hub Registration/Unregistration
 * - Mirrored Action Publishing
 * - Emotional Resonance Flow
 * - Empathetic Response Request
 * - Intention Prediction
 * - Social Understanding
 * - Statistics Tracking
 * - Thread Safety
 * - Null Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_mirror_empathy_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_action_callback_count{0};
static std::atomic<int> g_resonance_callback_count{0};
static std::atomic<int> g_response_callback_count{0};
static mirror_empathy_action_t g_last_action;
static mirror_empathy_resonance_t g_last_resonance;
static mirror_empathy_response_t g_last_response;

/**
 * Test callback for action events
 */
static void test_action_callback(
    const mirror_empathy_action_t* action,
    void* user_data
) {
    (void)user_data;
    g_action_callback_count++;
    if (action) {
        g_last_action = *action;
    }
}

/**
 * Test callback for resonance events
 */
static void test_resonance_callback(
    const mirror_empathy_resonance_t* resonance,
    void* user_data
) {
    (void)user_data;
    g_resonance_callback_count++;
    if (resonance) {
        g_last_resonance = *resonance;
    }
}

/**
 * Test callback for response events
 */
static void test_response_callback(
    const mirror_empathy_response_t* response,
    void* user_data
) {
    (void)user_data;
    g_response_callback_count++;
    if (response) {
        g_last_response = *response;
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorEmpathyBridgeTest : public ::testing::Test {
protected:
    mirror_empathy_bridge_t* bridge = nullptr;
    mirror_empathy_config_t config;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Reset global state
        g_action_callback_count = 0;
        g_resonance_callback_count = 0;
        g_response_callback_count = 0;
        memset(&g_last_action, 0, sizeof(g_last_action));
        memset(&g_last_resonance, 0, sizeof(g_last_resonance));
        memset(&g_last_response, 0, sizeof(g_last_response));

        // Get default config
        int result = mirror_empathy_bridge_default_config(&config);
        ASSERT_EQ(result, 0) << "Default config should succeed";

        // Create bridge
        bridge = mirror_empathy_bridge_create(&config);

        // Create cognitive hub for registration tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            mirror_empathy_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Lifecycle Tests
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created and destroyed successfully
 */
TEST_F(MirrorEmpathyBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify not registered initially
    EXPECT_FALSE(mirror_empathy_bridge_is_registered(bridge))
        << "Bridge should not be registered initially";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(MirrorEmpathyBridgeTest, BridgeCreationNullConfig) {
    mirror_empathy_bridge_t* br = mirror_empathy_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";
    mirror_empathy_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(MirrorEmpathyBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    mirror_empathy_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    mirror_empathy_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(MirrorEmpathyBridgeTest, DefaultConfig) {
    mirror_empathy_config_t default_config;
    int result = mirror_empathy_bridge_default_config(&default_config);
    EXPECT_EQ(result, 0) << "Default config should succeed";

    // Verify module ID
    EXPECT_EQ(default_config.module_id, MIRROR_EMPATHY_DEFAULT_MODULE_ID);

    // Verify weights are in valid range
    EXPECT_GE(default_config.action_understanding_weight, 0.0f);
    EXPECT_LE(default_config.action_understanding_weight, 1.0f);
    EXPECT_GE(default_config.emotional_resonance_weight, 0.0f);
    EXPECT_LE(default_config.emotional_resonance_weight, 1.0f);
    EXPECT_GE(default_config.empathy_generation_weight, 0.0f);
    EXPECT_LE(default_config.empathy_generation_weight, 1.0f);

    // Verify auto-subscribe options
    EXPECT_TRUE(default_config.auto_subscribe_social);
    EXPECT_TRUE(default_config.auto_subscribe_state);
    EXPECT_TRUE(default_config.auto_subscribe_input);

    // Verify other options
    EXPECT_TRUE(default_config.publish_predictions);
    EXPECT_GT(default_config.event_buffer_size, 0u);
    EXPECT_GT(default_config.agent_capacity, 0u);
}

/**
 * Test: ConfigValidation
 * Verify configuration with custom values works
 */
TEST_F(MirrorEmpathyBridgeTest, ConfigValidation) {
    mirror_empathy_config_t custom_config;
    mirror_empathy_bridge_default_config(&custom_config);

    custom_config.action_understanding_weight = 0.9f;
    custom_config.emotional_resonance_weight = 0.5f;
    custom_config.empathy_generation_weight = 0.6f;
    custom_config.enable_logging = true;
    custom_config.empathy_threshold = 0.5f;

    mirror_empathy_bridge_t* custom_bridge = mirror_empathy_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr) << "Bridge with custom config should be created";

    mirror_empathy_bridge_destroy(custom_bridge);
}

/**
 * Test: DefaultConfigNull
 * Verify default_config handles NULL gracefully
 */
TEST_F(MirrorEmpathyBridgeTest, DefaultConfigNull) {
    int result = mirror_empathy_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/* ============================================================================
 * Hub Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithHub
 * Verify bridge can register with cognitive hub
 */
TEST_F(MirrorEmpathyBridgeTest, RegisterWithHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Registration should succeed";

    EXPECT_TRUE(mirror_empathy_bridge_is_registered(bridge))
        << "Bridge should be registered after register_with_hub()";
}

/**
 * Test: RegisterWithHubNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(MirrorEmpathyBridgeTest, RegisterWithHubNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // NULL bridge
    int result = mirror_empathy_bridge_register_with_hub(nullptr, hub);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL hub
    result = mirror_empathy_bridge_register_with_hub(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL hub should fail";
}

/**
 * Test: RegisterWithHubDuplicate
 * Verify registering when already registered is handled
 */
TEST_F(MirrorEmpathyBridgeTest, RegisterWithHubDuplicate) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // First registration
    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "First registration should succeed";

    // Second registration - should fail
    result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, -1) << "Duplicate registration should fail";
}

/**
 * Test: UnregisterFromHub
 * Verify bridge can unregister cleanly
 */
TEST_F(MirrorEmpathyBridgeTest, UnregisterFromHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for unregister test";

    // Unregister
    result = mirror_empathy_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0) << "Unregister should succeed";

    EXPECT_FALSE(mirror_empathy_bridge_is_registered(bridge))
        << "Bridge should not be registered after unregister";
}

/**
 * Test: UnregisterFromHubNull
 * Verify unregister handles NULL gracefully
 */
TEST_F(MirrorEmpathyBridgeTest, UnregisterFromHubNull) {
    int result = mirror_empathy_bridge_unregister_from_hub(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/**
 * Test: UnregisterFromHubNotRegistered
 * Verify unregistering when not registered is handled
 */
TEST_F(MirrorEmpathyBridgeTest, UnregisterFromHubNotRegistered) {
    ASSERT_NE(bridge, nullptr);

    // Unregister without registering first
    int result = mirror_empathy_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, -1) << "Unregister when not registered should fail";
}

/**
 * Test: IsRegisteredNull
 * Verify is_registered handles NULL gracefully
 */
TEST_F(MirrorEmpathyBridgeTest, IsRegisteredNull) {
    bool registered = mirror_empathy_bridge_is_registered(nullptr);
    EXPECT_FALSE(registered) << "NULL bridge should return false";
}

/* ============================================================================
 * Mirrored Action Publishing Tests
 * ============================================================================ */

/**
 * Test: MirroredActionPublishing
 * Verify mirrored action events can be published
 */
TEST_F(MirrorEmpathyBridgeTest, MirroredActionPublishing) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for publishing test";

    // Create action data
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 1;
    action.action_type = MIRROR_ACTION_GESTURE;
    action.understanding_confidence = 0.85f;
    action.goal_inference_confidence = 0.7f;
    action.timestamp = 1000;
    strncpy(action.action_description, "Waving gesture", sizeof(action.action_description) - 1);

    // Publish action
    result = mirror_empathy_publish_mirrored_action(bridge, &action);
    EXPECT_EQ(result, 0) << "Publish mirrored action should succeed";

    // Verify stats updated
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.actions_mirrored, 1u) << "Actions mirrored should be counted";
}

/**
 * Test: MirroredActionPublishingNotRegistered
 * Verify publishing when not registered fails
 */
TEST_F(MirrorEmpathyBridgeTest, MirroredActionPublishingNotRegistered) {
    ASSERT_NE(bridge, nullptr);

    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 1;
    action.action_type = MIRROR_ACTION_FACIAL;

    // Try to publish without registering
    int result = mirror_empathy_publish_mirrored_action(bridge, &action);
    EXPECT_EQ(result, -1) << "Publishing when not registered should fail";
}

/**
 * Test: MirroredActionPublishingNullParams
 * Verify action publishing handles NULL parameters
 */
TEST_F(MirrorEmpathyBridgeTest, MirroredActionPublishingNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // NULL bridge
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    result = mirror_empathy_publish_mirrored_action(nullptr, &action);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL action
    result = mirror_empathy_publish_mirrored_action(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL action should fail";
}

/* ============================================================================
 * Emotional Resonance Tests
 * ============================================================================ */

/**
 * Test: EmotionalResonanceFlow
 * Verify emotional resonance events can be published
 */
TEST_F(MirrorEmpathyBridgeTest, EmotionalResonanceFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create resonance data
    mirror_empathy_resonance_t resonance;
    memset(&resonance, 0, sizeof(resonance));
    resonance.agent_id = 2;
    resonance.emotion_type = MIRROR_EMOTION_JOY;
    resonance.valence = 0.8f;
    resonance.arousal = 0.6f;
    resonance.resonance_strength = 0.75f;
    resonance.timestamp = 2000;

    // Publish resonance
    result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
    EXPECT_EQ(result, 0) << "Publish emotional resonance should succeed";

    // Verify stats updated
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.emotions_resonated, 1u) << "Emotions resonated should be counted";
}

/**
 * Test: EmotionalResonanceAllEmotions
 * Verify all emotion types can be published
 */
TEST_F(MirrorEmpathyBridgeTest, EmotionalResonanceAllEmotions) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    for (int i = 0; i < MIRROR_EMOTION_COUNT; i++) {
        mirror_empathy_resonance_t resonance;
        memset(&resonance, 0, sizeof(resonance));
        resonance.agent_id = (uint32_t)(100 + i);
        resonance.emotion_type = (mirror_emotion_type_t)i;
        resonance.valence = (i % 2 == 0) ? 0.5f : -0.5f;
        resonance.arousal = 0.5f;
        resonance.resonance_strength = 0.5f;
        resonance.timestamp = 3000 + i;

        result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
        EXPECT_EQ(result, 0) << "Emotion type " << i << " should publish";
    }
}

/* ============================================================================
 * Empathetic Response Tests
 * ============================================================================ */

/**
 * Test: EmpatheticResponseRequest
 * Verify empathetic response generation works
 */
TEST_F(MirrorEmpathyBridgeTest, EmpatheticResponseRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Request empathetic response
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge,
        5,  // target_agent_id
        MIRROR_EMOTION_SADNESS,
        &response
    );
    EXPECT_EQ(result, 0) << "Empathetic response request should succeed";

    // Verify response fields
    EXPECT_EQ(response.target_agent_id, 5u);
    EXPECT_EQ(response.perceived_emotion, MIRROR_EMOTION_SADNESS);
    EXPECT_GE(response.empathy_intensity, 0.0f);
    EXPECT_LE(response.empathy_intensity, 1.0f);
    EXPECT_GT(strlen(response.response_suggestion), 0u)
        << "Response suggestion should be generated";

    // Verify stats updated
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.empathetic_responses, 1u);
}

/**
 * Test: EmpatheticResponseWithCallback
 * Verify response callback is invoked
 */
TEST_F(MirrorEmpathyBridgeTest, EmpatheticResponseWithCallback) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Set callback before registering
    int result = mirror_empathy_set_response_callback(
        bridge, test_response_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Request empathetic response
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge,
        10,
        MIRROR_EMOTION_FEAR,
        &response
    );
    EXPECT_EQ(result, 0);

    // Verify callback was invoked
    EXPECT_EQ(g_response_callback_count.load(), 1)
        << "Response callback should be invoked";
    EXPECT_EQ(g_last_response.target_agent_id, 10u);
    EXPECT_EQ(g_last_response.perceived_emotion, MIRROR_EMOTION_FEAR);
}

/**
 * Test: EmpatheticResponseAllEmotions
 * Verify empathetic response for all emotion types
 */
TEST_F(MirrorEmpathyBridgeTest, EmpatheticResponseAllEmotions) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    for (int i = 0; i < MIRROR_EMOTION_COUNT; i++) {
        mirror_empathy_response_t response;
        result = mirror_empathy_request_empathetic_response(
            bridge,
            (uint32_t)(200 + i),
            (mirror_emotion_type_t)i,
            &response
        );
        EXPECT_EQ(result, 0) << "Emotion type " << i << " should generate response";
        EXPECT_EQ(response.perceived_emotion, (mirror_emotion_type_t)i);
    }
}

/* ============================================================================
 * Intention Prediction Tests
 * ============================================================================ */

/**
 * Test: IntentionPrediction
 * Verify intention predictions can be published
 */
TEST_F(MirrorEmpathyBridgeTest, IntentionPrediction) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create intention prediction
    mirror_empathy_intention_t intention;
    memset(&intention, 0, sizeof(intention));
    intention.agent_id = 15;
    intention.predicted_goal = 100;
    intention.confidence = 0.8f;
    intention.time_to_action_ms = 500.0f;
    strncpy(intention.intention_description, "Approaching to greet",
            sizeof(intention.intention_description) - 1);
    intention.timestamp = 5000;

    // Publish intention
    result = mirror_empathy_notify_action_intention(bridge, &intention);
    EXPECT_EQ(result, 0) << "Intention notification should succeed";

    // Verify stats
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.intentions_predicted, 1u);
}

/**
 * Test: IntentionPredictionDisabled
 * Verify predictions can be disabled via config
 */
TEST_F(MirrorEmpathyBridgeTest, IntentionPredictionDisabled) {
    // Create bridge with predictions disabled
    mirror_empathy_config_t no_pred_config;
    mirror_empathy_bridge_default_config(&no_pred_config);
    no_pred_config.publish_predictions = false;

    mirror_empathy_bridge_t* no_pred_bridge = mirror_empathy_bridge_create(&no_pred_config);
    ASSERT_NE(no_pred_bridge, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(no_pred_bridge, hub);
    ASSERT_EQ(result, 0);

    mirror_empathy_intention_t intention;
    memset(&intention, 0, sizeof(intention));
    intention.agent_id = 20;
    intention.predicted_goal = 200;

    // Should succeed but not publish
    result = mirror_empathy_notify_action_intention(no_pred_bridge, &intention);
    EXPECT_EQ(result, 0) << "Should succeed even with predictions disabled";

    mirror_empathy_bridge_destroy(no_pred_bridge);
}

/* ============================================================================
 * Social Understanding Tests
 * ============================================================================ */

/**
 * Test: SocialUnderstanding
 * Verify social understanding events can be published
 */
TEST_F(MirrorEmpathyBridgeTest, SocialUnderstanding) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create social understanding
    mirror_empathy_social_t understanding;
    memset(&understanding, 0, sizeof(understanding));
    understanding.agent_id = 25;
    understanding.rapport_level = 0.7f;
    understanding.trust_level = 0.6f;
    understanding.familiarity = 0.8f;
    understanding.cooperation_likely = true;
    understanding.timestamp = 6000;

    // Publish understanding
    result = mirror_empathy_publish_social_understanding(bridge, &understanding);
    EXPECT_EQ(result, 0) << "Social understanding should publish";

    // Verify stats
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.social_insights, 1u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are tracked correctly
 */
TEST_F(MirrorEmpathyBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Perform various operations
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 1;
    mirror_empathy_publish_mirrored_action(bridge, &action);

    mirror_empathy_resonance_t resonance;
    memset(&resonance, 0, sizeof(resonance));
    resonance.agent_id = 2;
    mirror_empathy_publish_emotional_resonance(bridge, &resonance);

    mirror_empathy_response_t response;
    mirror_empathy_request_empathetic_response(bridge, 3, MIRROR_EMOTION_JOY, &response);

    // Get stats
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GE(stats.actions_mirrored, 1u);
    EXPECT_GE(stats.emotions_resonated, 1u);
    EXPECT_GE(stats.empathetic_responses, 1u);
    EXPECT_GE(stats.events_published, 3u);
}

/**
 * Test: StatisticsTrackingNull
 * Verify get_stats handles NULL parameters
 */
TEST_F(MirrorEmpathyBridgeTest, StatisticsTrackingNull) {
    mirror_empathy_stats_t stats;

    int result = mirror_empathy_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = mirror_empathy_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL stats output should fail";
}

/**
 * Test: StatisticsReset
 * Verify statistics can be reset
 */
TEST_F(MirrorEmpathyBridgeTest, StatisticsReset) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Perform operation
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 1;
    mirror_empathy_publish_mirrored_action(bridge, &action);

    // Reset stats
    result = mirror_empathy_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    // Get stats after reset
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.actions_mirrored, 0u);
    EXPECT_EQ(stats.emotions_resonated, 0u);
    EXPECT_EQ(stats.empathetic_responses, 0u);
    EXPECT_EQ(stats.events_published, 0u);
}

/**
 * Test: StatisticsResetNull
 * Verify reset_stats handles NULL gracefully
 */
TEST_F(MirrorEmpathyBridgeTest, StatisticsResetNull) {
    int result = mirror_empathy_bridge_reset_stats(nullptr);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Callback Registration Tests
 * ============================================================================ */

/**
 * Test: CallbackRegistration
 * Verify callbacks can be registered
 */
TEST_F(MirrorEmpathyBridgeTest, CallbackRegistration) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set action callback
    int result = mirror_empathy_set_action_callback(
        bridge, test_action_callback, &user_data);
    EXPECT_EQ(result, 0);

    // Set resonance callback
    result = mirror_empathy_set_resonance_callback(
        bridge, test_resonance_callback, &user_data);
    EXPECT_EQ(result, 0);

    // Set response callback
    result = mirror_empathy_set_response_callback(
        bridge, test_response_callback, &user_data);
    EXPECT_EQ(result, 0);
}

/**
 * Test: CallbackClear
 * Verify callbacks can be cleared
 */
TEST_F(MirrorEmpathyBridgeTest, CallbackClear) {
    ASSERT_NE(bridge, nullptr);

    // Set then clear callbacks
    int result = mirror_empathy_set_action_callback(
        bridge, test_action_callback, nullptr);
    EXPECT_EQ(result, 0);
    result = mirror_empathy_set_action_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0);

    result = mirror_empathy_set_resonance_callback(
        bridge, test_resonance_callback, nullptr);
    EXPECT_EQ(result, 0);
    result = mirror_empathy_set_resonance_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0);

    result = mirror_empathy_set_response_callback(
        bridge, test_response_callback, nullptr);
    EXPECT_EQ(result, 0);
    result = mirror_empathy_set_response_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0);
}

/**
 * Test: CallbackRegistrationNullBridge
 * Verify callback registration handles NULL bridge
 */
TEST_F(MirrorEmpathyBridgeTest, CallbackRegistrationNullBridge) {
    int result = mirror_empathy_set_action_callback(
        nullptr, test_action_callback, nullptr);
    EXPECT_EQ(result, -1);

    result = mirror_empathy_set_resonance_callback(
        nullptr, test_resonance_callback, nullptr);
    EXPECT_EQ(result, -1);

    result = mirror_empathy_set_response_callback(
        nullptr, test_response_callback, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Agent State Query Tests
 * ============================================================================ */

/**
 * Test: AgentStateQuery
 * Verify agent state can be queried
 */
TEST_F(MirrorEmpathyBridgeTest, AgentStateQuery) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    // Generate some empathy for an agent
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge, 50, MIRROR_EMOTION_SADNESS, &response);
    ASSERT_EQ(result, 0);

    // Query agent state
    float empathy_level = 0.0f;
    mirror_emotion_type_t last_emotion = MIRROR_EMOTION_NEUTRAL;
    result = mirror_empathy_get_agent_state(bridge, 50, &empathy_level, &last_emotion);
    EXPECT_EQ(result, 0);

    EXPECT_GE(empathy_level, 0.0f);
    EXPECT_EQ(last_emotion, MIRROR_EMOTION_SADNESS);
}

/**
 * Test: AgentStateQueryUnknownAgent
 * Verify query for unknown agent returns defaults
 */
TEST_F(MirrorEmpathyBridgeTest, AgentStateQueryUnknownAgent) {
    ASSERT_NE(bridge, nullptr);

    float empathy_level = 1.0f;  // Set non-default value
    mirror_emotion_type_t last_emotion = MIRROR_EMOTION_ANGER;

    int result = mirror_empathy_get_agent_state(
        bridge, 9999, &empathy_level, &last_emotion);
    EXPECT_EQ(result, 0);

    // Should return defaults for unknown agent
    EXPECT_EQ(empathy_level, 0.0f);
    EXPECT_EQ(last_emotion, MIRROR_EMOTION_NEUTRAL);
}

/**
 * Test: AgentStateQueryNull
 * Verify agent state query handles NULL parameters
 */
TEST_F(MirrorEmpathyBridgeTest, AgentStateQueryNull) {
    float empathy_level;
    mirror_emotion_type_t last_emotion;

    int result = mirror_empathy_get_agent_state(nullptr, 1, &empathy_level, &last_emotion);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Basic test for concurrent access to bridge
 */
TEST_F(MirrorEmpathyBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, t, ITERATIONS]() {
            for (int i = 0; i < ITERATIONS; i++) {
                // Read operations
                mirror_empathy_bridge_is_registered(bridge);

                float empathy_level = 0.0f;
                mirror_emotion_type_t last_emotion;
                mirror_empathy_get_agent_state(bridge, (uint32_t)t,
                    &empathy_level, &last_emotion);

                mirror_empathy_stats_t stats;
                mirror_empathy_bridge_get_stats(bridge, &stats);

                // Write operations
                mirror_empathy_action_t action;
                memset(&action, 0, sizeof(action));
                action.agent_id = (uint32_t)(t * 1000 + i);
                action.action_type = (mirror_action_type_t)(i % MIRROR_ACTION_COUNT);
                mirror_empathy_publish_mirrored_action(bridge, &action);
            }
            completed++;
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete successfully";

    // Verify stats
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.actions_mirrored, (uint64_t)(NUM_THREADS * ITERATIONS));
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling
 */
TEST_F(MirrorEmpathyBridgeTest, NullHandling) {
    // Lifecycle
    mirror_empathy_bridge_destroy(nullptr);
    EXPECT_EQ(mirror_empathy_bridge_default_config(nullptr), -1);

    // Registration
    EXPECT_EQ(mirror_empathy_bridge_register_with_hub(nullptr, hub), -1);
    EXPECT_EQ(mirror_empathy_bridge_register_with_hub(bridge, nullptr), -1);
    EXPECT_EQ(mirror_empathy_bridge_unregister_from_hub(nullptr), -1);
    EXPECT_FALSE(mirror_empathy_bridge_is_registered(nullptr));

    // Callbacks
    EXPECT_EQ(mirror_empathy_set_action_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(mirror_empathy_set_resonance_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(mirror_empathy_set_response_callback(nullptr, nullptr, nullptr), -1);

    // Publication
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    EXPECT_EQ(mirror_empathy_publish_mirrored_action(nullptr, &action), -1);
    EXPECT_EQ(mirror_empathy_publish_mirrored_action(bridge, nullptr), -1);

    mirror_empathy_resonance_t resonance;
    memset(&resonance, 0, sizeof(resonance));
    EXPECT_EQ(mirror_empathy_publish_emotional_resonance(nullptr, &resonance), -1);
    EXPECT_EQ(mirror_empathy_publish_emotional_resonance(bridge, nullptr), -1);

    mirror_empathy_response_t response;
    EXPECT_EQ(mirror_empathy_request_empathetic_response(nullptr, 1, MIRROR_EMOTION_JOY, &response), -1);
    EXPECT_EQ(mirror_empathy_request_empathetic_response(bridge, 1, MIRROR_EMOTION_JOY, nullptr), -1);

    mirror_empathy_intention_t intention;
    memset(&intention, 0, sizeof(intention));
    EXPECT_EQ(mirror_empathy_notify_action_intention(nullptr, &intention), -1);
    EXPECT_EQ(mirror_empathy_notify_action_intention(bridge, nullptr), -1);

    mirror_empathy_social_t understanding;
    memset(&understanding, 0, sizeof(understanding));
    EXPECT_EQ(mirror_empathy_publish_social_understanding(nullptr, &understanding), -1);
    EXPECT_EQ(mirror_empathy_publish_social_understanding(bridge, nullptr), -1);

    // Query
    float empathy_level;
    mirror_emotion_type_t last_emotion;
    EXPECT_EQ(mirror_empathy_get_agent_state(nullptr, 1, &empathy_level, &last_emotion), -1);

    // Stats
    mirror_empathy_stats_t stats;
    EXPECT_EQ(mirror_empathy_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(mirror_empathy_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(mirror_empathy_bridge_reset_stats(nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

/**
 * Test: ActionTypeToString
 * Verify action type string conversion
 */
TEST_F(MirrorEmpathyBridgeTest, ActionTypeToString) {
    EXPECT_STREQ(mirror_empathy_action_type_to_string(MIRROR_ACTION_GRASP), "GRASP");
    EXPECT_STREQ(mirror_empathy_action_type_to_string(MIRROR_ACTION_FACIAL), "FACIAL");
    EXPECT_STREQ(mirror_empathy_action_type_to_string(MIRROR_ACTION_GESTURE), "GESTURE");
    EXPECT_STREQ(mirror_empathy_action_type_to_string(MIRROR_ACTION_POSTURAL), "POSTURAL");
    EXPECT_STREQ(mirror_empathy_action_type_to_string(MIRROR_ACTION_VOCAL), "VOCAL");
    EXPECT_STREQ(mirror_empathy_action_type_to_string((mirror_action_type_t)99), "UNKNOWN");
}

/**
 * Test: EmotionTypeToString
 * Verify emotion type string conversion
 */
TEST_F(MirrorEmpathyBridgeTest, EmotionTypeToString) {
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_JOY), "JOY");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_SADNESS), "SADNESS");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_FEAR), "FEAR");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_ANGER), "ANGER");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_SURPRISE), "SURPRISE");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_DISGUST), "DISGUST");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string(MIRROR_EMOTION_NEUTRAL), "NEUTRAL");
    EXPECT_STREQ(mirror_empathy_emotion_type_to_string((mirror_emotion_type_t)99), "UNKNOWN");
}

/**
 * Test: EventTypeToString
 * Verify event type string conversion
 */
TEST_F(MirrorEmpathyBridgeTest, EventTypeToString) {
    EXPECT_STREQ(mirror_empathy_event_type_to_string(MIRROR_EMPATHY_EVENT_ACTION_MIRRORED),
                 "ACTION_MIRRORED");
    EXPECT_STREQ(mirror_empathy_event_type_to_string(MIRROR_EMPATHY_EVENT_EMOTION_RESONATED),
                 "EMOTION_RESONATED");
    EXPECT_STREQ(mirror_empathy_event_type_to_string(MIRROR_EMPATHY_EVENT_EMPATHY_GENERATED),
                 "EMPATHY_GENERATED");
    EXPECT_STREQ(mirror_empathy_event_type_to_string((mirror_empathy_event_type_t)99),
                 "UNKNOWN");
}

/* ============================================================================
 * Integration Flow Tests
 * ============================================================================ */

/**
 * Test: FullIntegrationFlow
 * Test complete flow: register, publish, query, unregister
 */
TEST_F(MirrorEmpathyBridgeTest, FullIntegrationFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(mirror_empathy_bridge_is_registered(bridge));

    // Publish mirrored action
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 100;
    action.action_type = MIRROR_ACTION_FACIAL;
    action.understanding_confidence = 0.9f;
    result = mirror_empathy_publish_mirrored_action(bridge, &action);
    EXPECT_EQ(result, 0);

    // Publish emotional resonance
    mirror_empathy_resonance_t resonance;
    memset(&resonance, 0, sizeof(resonance));
    resonance.agent_id = 100;
    resonance.emotion_type = MIRROR_EMOTION_SADNESS;
    resonance.resonance_strength = 0.8f;
    result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
    EXPECT_EQ(result, 0);

    // Request empathetic response
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge, 100, MIRROR_EMOTION_SADNESS, &response);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response.empathy_intensity, 0.0f);

    // Query agent state
    float empathy_level = 0.0f;
    mirror_emotion_type_t last_emotion = MIRROR_EMOTION_NEUTRAL;
    result = mirror_empathy_get_agent_state(bridge, 100, &empathy_level, &last_emotion);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(last_emotion, MIRROR_EMOTION_SADNESS);

    // Check stats
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.actions_mirrored, 1u);
    EXPECT_GE(stats.emotions_resonated, 1u);
    EXPECT_GE(stats.empathetic_responses, 1u);

    // Unregister
    result = mirror_empathy_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(mirror_empathy_bridge_is_registered(bridge));
}

/**
 * Test: ReregisterAfterUnregister
 * Verify bridge can re-register after unregistering
 */
TEST_F(MirrorEmpathyBridgeTest, ReregisterAfterUnregister) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register
    int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0);

    // Unregister
    result = mirror_empathy_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0);

    // Re-register
    result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(mirror_empathy_bridge_is_registered(bridge));
}

/* ============================================================================
 * Configuration Variants Tests
 * ============================================================================ */

/**
 * Test: ConfigVariants
 * Test bridge creation with various config options
 */
TEST_F(MirrorEmpathyBridgeTest, ConfigVariants) {
    // Config with all auto-subscribe disabled
    mirror_empathy_config_t config1;
    mirror_empathy_bridge_default_config(&config1);
    config1.auto_subscribe_social = false;
    config1.auto_subscribe_state = false;
    config1.auto_subscribe_input = false;

    mirror_empathy_bridge_t* br1 = mirror_empathy_bridge_create(&config1);
    ASSERT_NE(br1, nullptr);
    mirror_empathy_bridge_destroy(br1);

    // Config with logging enabled
    mirror_empathy_config_t config2;
    mirror_empathy_bridge_default_config(&config2);
    config2.enable_logging = true;

    mirror_empathy_bridge_t* br2 = mirror_empathy_bridge_create(&config2);
    ASSERT_NE(br2, nullptr);
    mirror_empathy_bridge_destroy(br2);

    // Config with custom module ID
    mirror_empathy_config_t config3;
    mirror_empathy_bridge_default_config(&config3);
    config3.module_id = 0x12345678;

    mirror_empathy_bridge_t* br3 = mirror_empathy_bridge_create(&config3);
    ASSERT_NE(br3, nullptr);
    mirror_empathy_bridge_destroy(br3);

    // Config with high empathy threshold
    mirror_empathy_config_t config4;
    mirror_empathy_bridge_default_config(&config4);
    config4.empathy_threshold = 0.9f;

    mirror_empathy_bridge_t* br4 = mirror_empathy_bridge_create(&config4);
    ASSERT_NE(br4, nullptr);
    mirror_empathy_bridge_destroy(br4);

    // Config with small agent capacity
    mirror_empathy_config_t config5;
    mirror_empathy_bridge_default_config(&config5);
    config5.agent_capacity = 4;

    mirror_empathy_bridge_t* br5 = mirror_empathy_bridge_create(&config5);
    ASSERT_NE(br5, nullptr);
    mirror_empathy_bridge_destroy(br5);

    SUCCEED() << "All config variants should work";
}
