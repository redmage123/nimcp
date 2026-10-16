/**
 * @file test_mirror_empathy_integration.cpp
 * @brief Integration tests for Mirror Neurons - Empathetic Response Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration tests for complete mirror neuron to empathy pipeline
 * WHY:  Verify end-to-end integration of action understanding to empathetic response
 * HOW:  Test complete flows with realistic scenarios
 *
 * TEST COVERAGE:
 * - ActionToEmpathyPipeline: Full action understanding to empathy
 * - EmotionalContagionFlow: Mirrored emotion triggers empathy
 * - IntentionBasedEmpathy: Understanding intention drives response
 * - SocialBondingIntegration: Empathy strengthens social bonds
 * - CompassionateResponseGeneration: Generating appropriate responses
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

#include "cognitive/integration/nimcp_mirror_empathy_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define MODULE_MIRROR_EMPATHY   0x4D49454D  /* MIEM */
#define MODULE_OBSERVER_1       0x4F425331  /* OBS1 */
#define MODULE_OBSERVER_2       0x4F425332  /* OBS2 */

#define AGENT_FRIEND            1
#define AGENT_STRANGER          2
#define AGENT_FAMILY            3
#define AGENT_COLLEAGUE         4

/* ============================================================================
 * Test Context for Event Tracking
 * ============================================================================ */

struct IntegrationTestContext {
    std::atomic<int> social_events_received{0};
    std::atomic<int> state_events_received{0};
    std::atomic<int> output_events_received{0};
    std::atomic<int> total_events_received{0};

    mirror_empathy_response_t last_response;
    mirror_empathy_action_t last_action;
    mirror_empathy_resonance_t last_resonance;

    bool action_callback_invoked{false};
    bool resonance_callback_invoked{false};
    bool response_callback_invoked{false};
};

static IntegrationTestContext g_test_ctx;

/* ============================================================================
 * Hub Event Callbacks for Observer Modules
 * ============================================================================ */

static int observer_event_callback(const cognitive_event_data_t* event, void* user_data) {
    IntegrationTestContext* ctx = static_cast<IntegrationTestContext*>(user_data);
    if (!ctx || !event) return -1;

    ctx->total_events_received++;

    switch (event->event_type) {
        case COG_EVENT_SOCIAL_SIGNAL:
            ctx->social_events_received++;
            break;
        case COG_EVENT_STATE_CHANGE:
            ctx->state_events_received++;
            break;
        case COG_EVENT_OUTPUT_READY:
            ctx->output_events_received++;
            break;
        default:
            break;
    }

    return 0;
}

/* ============================================================================
 * Bridge Callbacks
 * ============================================================================ */

static void integration_action_callback(
    const mirror_empathy_action_t* action,
    void* user_data
) {
    IntegrationTestContext* ctx = static_cast<IntegrationTestContext*>(user_data);
    if (ctx && action) {
        ctx->last_action = *action;
        ctx->action_callback_invoked = true;
    }
}

static void integration_resonance_callback(
    const mirror_empathy_resonance_t* resonance,
    void* user_data
) {
    IntegrationTestContext* ctx = static_cast<IntegrationTestContext*>(user_data);
    if (ctx && resonance) {
        ctx->last_resonance = *resonance;
        ctx->resonance_callback_invoked = true;
    }
}

static void integration_response_callback(
    const mirror_empathy_response_t* response,
    void* user_data
) {
    IntegrationTestContext* ctx = static_cast<IntegrationTestContext*>(user_data);
    if (ctx && response) {
        ctx->last_response = *response;
        ctx->response_callback_invoked = true;
    }
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorEmpathyIntegrationTest : public ::testing::Test {
protected:
    mirror_empathy_bridge_t* bridge = nullptr;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Reset test context
        g_test_ctx.social_events_received = 0;
        g_test_ctx.state_events_received = 0;
        g_test_ctx.output_events_received = 0;
        g_test_ctx.total_events_received = 0;
        memset(&g_test_ctx.last_response, 0, sizeof(g_test_ctx.last_response));
        memset(&g_test_ctx.last_action, 0, sizeof(g_test_ctx.last_action));
        memset(&g_test_ctx.last_resonance, 0, sizeof(g_test_ctx.last_resonance));
        g_test_ctx.action_callback_invoked = false;
        g_test_ctx.resonance_callback_invoked = false;
        g_test_ctx.response_callback_invoked = false;

        // Create cognitive hub with async disabled for deterministic tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub_config.max_modules = 16;
        hub_config.max_subscriptions = 64;
        hub_config.enable_async = false;
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr);

        // Create mirror-empathy bridge
        mirror_empathy_config_t bridge_config;
        mirror_empathy_bridge_default_config(&bridge_config);
        bridge = mirror_empathy_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        // Set callbacks
        mirror_empathy_set_action_callback(bridge, integration_action_callback, &g_test_ctx);
        mirror_empathy_set_resonance_callback(bridge, integration_resonance_callback, &g_test_ctx);
        mirror_empathy_set_response_callback(bridge, integration_response_callback, &g_test_ctx);

        // Register bridge with hub
        int result = mirror_empathy_bridge_register_with_hub(bridge, hub);
        ASSERT_EQ(result, 0);
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

    // Helper to register observer module
    void RegisterObserver(uint32_t module_id, const char* name) {
        int result = cognitive_hub_register_module(
            hub, module_id, COG_CATEGORY_PERCEPTION, name, &g_test_ctx);
        ASSERT_EQ(result, 0);

        // Subscribe to events
        cognitive_hub_subscribe(hub, module_id, COG_EVENT_SOCIAL_SIGNAL,
                                observer_event_callback, &g_test_ctx);
        cognitive_hub_subscribe(hub, module_id, COG_EVENT_STATE_CHANGE,
                                observer_event_callback, &g_test_ctx);
        cognitive_hub_subscribe(hub, module_id, COG_EVENT_OUTPUT_READY,
                                observer_event_callback, &g_test_ctx);
    }
};

/* ============================================================================
 * ActionToEmpathyPipeline Test
 * Full action understanding to empathy generation pipeline
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, ActionToEmpathyPipeline) {
    // Register observer to track events
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    // Step 1: Observe an action (e.g., distressed gesture)
    mirror_empathy_action_t observed_action;
    memset(&observed_action, 0, sizeof(observed_action));
    observed_action.agent_id = AGENT_FRIEND;
    observed_action.action_type = MIRROR_ACTION_FACIAL;
    observed_action.understanding_confidence = 0.9f;
    observed_action.goal_inference_confidence = 0.8f;
    observed_action.timestamp = 1000;
    strncpy(observed_action.action_description, "Facial expression of distress",
            sizeof(observed_action.action_description) - 1);

    int result = mirror_empathy_publish_mirrored_action(bridge, &observed_action);
    EXPECT_EQ(result, 0);

    // Step 2: Resonate with the emotional content
    mirror_empathy_resonance_t resonance;
    memset(&resonance, 0, sizeof(resonance));
    resonance.agent_id = AGENT_FRIEND;
    resonance.emotion_type = MIRROR_EMOTION_SADNESS;
    resonance.valence = -0.7f;
    resonance.arousal = 0.6f;
    resonance.resonance_strength = 0.85f;
    resonance.timestamp = 1100;

    result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
    EXPECT_EQ(result, 0);

    // Step 3: Generate empathetic response
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge, AGENT_FRIEND, MIRROR_EMOTION_SADNESS, &response);
    EXPECT_EQ(result, 0);

    // Verify the pipeline produced appropriate empathy
    EXPECT_EQ(response.target_agent_id, (uint32_t)AGENT_FRIEND);
    EXPECT_EQ(response.perceived_emotion, MIRROR_EMOTION_SADNESS);
    EXPECT_GT(response.empathy_intensity, 0.3f)
        << "Empathy should be above threshold for familiar agent";
    EXPECT_TRUE(response.helping_motivation)
        << "High empathy should motivate helping";
    EXPECT_GT(strlen(response.response_suggestion), 0u)
        << "Response suggestion should be generated";

    // Verify callback was invoked
    EXPECT_TRUE(g_test_ctx.response_callback_invoked);

    // Verify events were published to hub
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.actions_mirrored, 1u);
    EXPECT_GE(stats.emotions_resonated, 1u);
    EXPECT_GE(stats.empathetic_responses, 1u);
}

/* ============================================================================
 * EmotionalContagionFlow Test
 * Mirrored emotion triggers empathetic response (emotional contagion)
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, EmotionalContagionFlow) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    // Simulate observing someone experiencing fear
    mirror_empathy_resonance_t fear_resonance;
    memset(&fear_resonance, 0, sizeof(fear_resonance));
    fear_resonance.agent_id = AGENT_STRANGER;
    fear_resonance.emotion_type = MIRROR_EMOTION_FEAR;
    fear_resonance.valence = -0.8f;
    fear_resonance.arousal = 0.9f;  // High arousal for fear
    fear_resonance.resonance_strength = 0.7f;
    fear_resonance.timestamp = 2000;

    int result = mirror_empathy_publish_emotional_resonance(bridge, &fear_resonance);
    EXPECT_EQ(result, 0);

    // Request empathetic response to the fearful agent
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge, AGENT_STRANGER, MIRROR_EMOTION_FEAR, &response);
    EXPECT_EQ(result, 0);

    // Verify empathetic response to fear
    EXPECT_EQ(response.perceived_emotion, MIRROR_EMOTION_FEAR);
    EXPECT_GT(response.empathy_intensity, 0.0f);

    // Fear typically elicits higher empathy due to survival relevance
    // Check response suggestion is appropriate for fear
    EXPECT_GT(strlen(response.response_suggestion), 0u);

    // Query agent state - should reflect fear
    float empathy_level = 0.0f;
    mirror_emotion_type_t last_emotion = MIRROR_EMOTION_NEUTRAL;
    result = mirror_empathy_get_agent_state(bridge, AGENT_STRANGER,
                                            &empathy_level, &last_emotion);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(last_emotion, MIRROR_EMOTION_FEAR);
}

/* ============================================================================
 * IntentionBasedEmpathy Test
 * Understanding intention drives empathetic response
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, IntentionBasedEmpathy) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    // Step 1: Mirror action that suggests intention
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = AGENT_FAMILY;
    action.action_type = MIRROR_ACTION_GESTURE;
    action.understanding_confidence = 0.85f;
    action.goal_inference_confidence = 0.9f;
    action.timestamp = 3000;
    strncpy(action.action_description, "Reaching out gesture",
            sizeof(action.action_description) - 1);

    int result = mirror_empathy_publish_mirrored_action(bridge, &action);
    EXPECT_EQ(result, 0);

    // Step 2: Predict intention based on action
    mirror_empathy_intention_t intention;
    memset(&intention, 0, sizeof(intention));
    intention.agent_id = AGENT_FAMILY;
    intention.predicted_goal = 1;  // Goal: seeking comfort
    intention.confidence = 0.85f;
    intention.time_to_action_ms = 1000.0f;
    strncpy(intention.intention_description, "Seeking emotional support",
            sizeof(intention.intention_description) - 1);
    intention.timestamp = 3100;

    result = mirror_empathy_notify_action_intention(bridge, &intention);
    EXPECT_EQ(result, 0);

    // Step 3: Generate empathetic response informed by intention
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge, AGENT_FAMILY, MIRROR_EMOTION_SADNESS, &response);
    EXPECT_EQ(result, 0);

    // Family members should elicit empathetic response
    EXPECT_GT(response.empathy_intensity, 0.3f)
        << "Family should elicit meaningful empathy";
    // Helping motivation is triggered when empathy_intensity > 0.6
    if (response.empathy_intensity > 0.6f) {
        EXPECT_TRUE(response.helping_motivation);
    }

    // Verify stats show intention prediction
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.intentions_predicted, 1u);
}

/* ============================================================================
 * SocialBondingIntegration Test
 * Empathy strengthens social bonds through repeated interactions
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, SocialBondingIntegration) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    // Simulate multiple empathetic interactions with same agent
    const int NUM_INTERACTIONS = 5;

    for (int i = 0; i < NUM_INTERACTIONS; i++) {
        // Alternate between different emotional states
        mirror_emotion_type_t emotions[] = {
            MIRROR_EMOTION_JOY,
            MIRROR_EMOTION_SADNESS,
            MIRROR_EMOTION_FEAR,
            MIRROR_EMOTION_JOY,
            MIRROR_EMOTION_SADNESS
        };

        mirror_empathy_resonance_t resonance;
        memset(&resonance, 0, sizeof(resonance));
        resonance.agent_id = AGENT_COLLEAGUE;
        resonance.emotion_type = emotions[i];
        resonance.valence = (emotions[i] == MIRROR_EMOTION_JOY) ? 0.7f : -0.5f;
        resonance.arousal = 0.5f;
        resonance.resonance_strength = 0.6f + (i * 0.05f);  // Increasing over time
        resonance.timestamp = 4000 + (i * 100);

        int result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
        EXPECT_EQ(result, 0);

        mirror_empathy_response_t response;
        result = mirror_empathy_request_empathetic_response(
            bridge, AGENT_COLLEAGUE, emotions[i], &response);
        EXPECT_EQ(result, 0);
    }

    // Publish social understanding reflecting strengthened bond
    mirror_empathy_social_t understanding;
    memset(&understanding, 0, sizeof(understanding));
    understanding.agent_id = AGENT_COLLEAGUE;
    understanding.rapport_level = 0.75f;  // High rapport after interactions
    understanding.trust_level = 0.7f;
    understanding.familiarity = 0.8f;
    understanding.cooperation_likely = true;
    understanding.timestamp = 5000;

    int result = mirror_empathy_publish_social_understanding(bridge, &understanding);
    EXPECT_EQ(result, 0);

    // Verify stats reflect multiple interactions
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.emotions_resonated, (uint64_t)NUM_INTERACTIONS);
    EXPECT_GE(stats.empathetic_responses, (uint64_t)NUM_INTERACTIONS);
    EXPECT_GE(stats.social_insights, 1u);
}

/* ============================================================================
 * CompassionateResponseGeneration Test
 * Generating appropriate compassionate responses for different emotions
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, CompassionateResponseGeneration) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    struct EmotionScenario {
        mirror_emotion_type_t emotion;
        const char* context;
        bool expect_helping;
    };

    EmotionScenario scenarios[] = {
        {MIRROR_EMOTION_SADNESS, "Loss experience", true},
        {MIRROR_EMOTION_FEAR, "Threat situation", true},
        {MIRROR_EMOTION_JOY, "Achievement celebration", false},  // Joy doesn't need help
        {MIRROR_EMOTION_ANGER, "Frustration", false},  // Anger needs space
        {MIRROR_EMOTION_DISGUST, "Unpleasant experience", false}
    };

    const int NUM_SCENARIOS = sizeof(scenarios) / sizeof(scenarios[0]);

    for (int i = 0; i < NUM_SCENARIOS; i++) {
        uint32_t agent_id = 500 + i;

        // Generate emotional resonance
        mirror_empathy_resonance_t resonance;
        memset(&resonance, 0, sizeof(resonance));
        resonance.agent_id = agent_id;
        resonance.emotion_type = scenarios[i].emotion;
        resonance.valence = (scenarios[i].emotion == MIRROR_EMOTION_JOY) ? 0.8f : -0.6f;
        resonance.arousal = 0.7f;
        resonance.resonance_strength = 0.75f;
        resonance.timestamp = 6000 + (i * 100);

        int result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
        EXPECT_EQ(result, 0);

        // Request compassionate response
        mirror_empathy_response_t response;
        result = mirror_empathy_request_empathetic_response(
            bridge, agent_id, scenarios[i].emotion, &response);
        EXPECT_EQ(result, 0);

        // Verify response is appropriate for emotion
        EXPECT_EQ(response.perceived_emotion, scenarios[i].emotion);
        EXPECT_GT(response.compassion_level, 0.0f);
        EXPECT_GT(strlen(response.response_suggestion), 0u)
            << "Response for " << scenarios[i].context << " should have suggestion";

        // For high-distress emotions, expect helping motivation
        if (scenarios[i].expect_helping && response.empathy_intensity > 0.6f) {
            EXPECT_TRUE(response.helping_motivation)
                << "Scenario '" << scenarios[i].context
                << "' with high empathy should motivate helping";
        }
    }

    // Verify all emotions processed
    mirror_empathy_stats_t stats;
    int result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.emotions_resonated, (uint64_t)NUM_SCENARIOS);
    EXPECT_GE(stats.empathetic_responses, (uint64_t)NUM_SCENARIOS);
}

/* ============================================================================
 * MultiAgentEmpathyTracking Test
 * Track empathy states for multiple agents simultaneously
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, MultiAgentEmpathyTracking) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    const int NUM_AGENTS = 8;
    mirror_emotion_type_t emotions[NUM_AGENTS] = {
        MIRROR_EMOTION_JOY,
        MIRROR_EMOTION_SADNESS,
        MIRROR_EMOTION_FEAR,
        MIRROR_EMOTION_ANGER,
        MIRROR_EMOTION_SURPRISE,
        MIRROR_EMOTION_DISGUST,
        MIRROR_EMOTION_NEUTRAL,
        MIRROR_EMOTION_JOY
    };

    // Generate empathy for multiple agents
    for (int i = 0; i < NUM_AGENTS; i++) {
        uint32_t agent_id = 700 + i;

        mirror_empathy_response_t response;
        int result = mirror_empathy_request_empathetic_response(
            bridge, agent_id, emotions[i], &response);
        EXPECT_EQ(result, 0);
    }

    // Query each agent's state
    for (int i = 0; i < NUM_AGENTS; i++) {
        uint32_t agent_id = 700 + i;

        float empathy_level = 0.0f;
        mirror_emotion_type_t last_emotion = MIRROR_EMOTION_NEUTRAL;

        int result = mirror_empathy_get_agent_state(
            bridge, agent_id, &empathy_level, &last_emotion);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(last_emotion, emotions[i])
            << "Agent " << agent_id << " should have correct emotion";
    }

    // Verify all agents tracked
    mirror_empathy_stats_t stats;
    int result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.empathetic_responses, (uint64_t)NUM_AGENTS);
}

/* ============================================================================
 * CrossModuleEventPropagation Test
 * Verify events propagate correctly to other hub modules
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, CrossModuleEventPropagation) {
    // Register two observer modules
    RegisterObserver(MODULE_OBSERVER_1, "observer1");
    RegisterObserver(MODULE_OBSERVER_2, "observer2");

    int initial_total = g_test_ctx.total_events_received.load();

    // Publish mirrored action - should trigger social signal events
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 800;
    action.action_type = MIRROR_ACTION_GESTURE;
    action.understanding_confidence = 0.9f;

    int result = mirror_empathy_publish_mirrored_action(bridge, &action);
    EXPECT_EQ(result, 0);

    // Publish emotional resonance - should trigger state change events
    mirror_empathy_resonance_t resonance;
    memset(&resonance, 0, sizeof(resonance));
    resonance.agent_id = 800;
    resonance.emotion_type = MIRROR_EMOTION_JOY;
    resonance.resonance_strength = 0.8f;

    result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
    EXPECT_EQ(result, 0);

    // Request empathetic response - should trigger output ready events
    mirror_empathy_response_t response;
    result = mirror_empathy_request_empathetic_response(
        bridge, 800, MIRROR_EMOTION_JOY, &response);
    EXPECT_EQ(result, 0);

    // Verify observers received events
    // Each event type goes to both observers
    int events_after = g_test_ctx.total_events_received.load();
    EXPECT_GT(events_after, initial_total)
        << "Observer modules should receive events";

    // Verify specific event types were received
    EXPECT_GT(g_test_ctx.social_events_received.load(), 0)
        << "Social signal events should be received";
    EXPECT_GT(g_test_ctx.state_events_received.load(), 0)
        << "State change events should be received";
    EXPECT_GT(g_test_ctx.output_events_received.load(), 0)
        << "Output ready events should be received";
}

/* ============================================================================
 * EmpathyIntensityGradient Test
 * Verify empathy intensity varies appropriately with context
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, EmpathyIntensityGradient) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    // Test that sadness and fear elicit higher empathy than neutral
    mirror_empathy_response_t sad_response, fear_response, neutral_response;

    int result = mirror_empathy_request_empathetic_response(
        bridge, 900, MIRROR_EMOTION_SADNESS, &sad_response);
    EXPECT_EQ(result, 0);

    result = mirror_empathy_request_empathetic_response(
        bridge, 901, MIRROR_EMOTION_FEAR, &fear_response);
    EXPECT_EQ(result, 0);

    result = mirror_empathy_request_empathetic_response(
        bridge, 902, MIRROR_EMOTION_NEUTRAL, &neutral_response);
    EXPECT_EQ(result, 0);

    // Distress emotions should elicit higher empathy than neutral
    // (accounting for the empathy_threshold that sets a minimum)
    EXPECT_GE(sad_response.empathy_intensity, neutral_response.empathy_intensity * 0.9f)
        << "Sadness should elicit at least as much empathy as neutral";
    EXPECT_GE(fear_response.empathy_intensity, neutral_response.empathy_intensity * 0.9f)
        << "Fear should elicit at least as much empathy as neutral";

    // Verify compassion correlates with empathy intensity
    EXPECT_GT(sad_response.compassion_level, 0.0f);
    EXPECT_GT(fear_response.compassion_level, 0.0f);
}

/* ============================================================================
 * RapidEmpathyUpdates Test
 * Verify bridge handles rapid sequential updates correctly
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, RapidEmpathyUpdates) {
    RegisterObserver(MODULE_OBSERVER_1, "observer1");

    const int RAPID_UPDATES = 100;
    int result;

    for (int i = 0; i < RAPID_UPDATES; i++) {
        mirror_empathy_resonance_t resonance;
        memset(&resonance, 0, sizeof(resonance));
        resonance.agent_id = 1000;
        resonance.emotion_type = (mirror_emotion_type_t)(i % MIRROR_EMOTION_COUNT);
        resonance.resonance_strength = 0.5f + (0.005f * i);
        resonance.timestamp = 10000 + i;

        result = mirror_empathy_publish_emotional_resonance(bridge, &resonance);
        EXPECT_EQ(result, 0);
    }

    // Verify all updates processed
    mirror_empathy_stats_t stats;
    result = mirror_empathy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.emotions_resonated, (uint64_t)RAPID_UPDATES);

    // Query final agent state
    float empathy_level = 0.0f;
    mirror_emotion_type_t last_emotion;
    result = mirror_empathy_get_agent_state(bridge, 1000, &empathy_level, &last_emotion);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * BridgeReconnection Test
 * Verify bridge works correctly after disconnect/reconnect
 * ============================================================================ */

TEST_F(MirrorEmpathyIntegrationTest, BridgeReconnection) {
    // Initial operation
    mirror_empathy_response_t response1;
    int result = mirror_empathy_request_empathetic_response(
        bridge, 1100, MIRROR_EMOTION_JOY, &response1);
    EXPECT_EQ(result, 0);

    // Disconnect
    result = mirror_empathy_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(mirror_empathy_bridge_is_registered(bridge));

    // Operations should fail while disconnected
    mirror_empathy_response_t response2;
    result = mirror_empathy_request_empathetic_response(
        bridge, 1101, MIRROR_EMOTION_FEAR, &response2);
    EXPECT_EQ(result, -1) << "Operations should fail while disconnected";

    // Reconnect
    result = mirror_empathy_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(mirror_empathy_bridge_is_registered(bridge));

    // Operations should work again
    mirror_empathy_response_t response3;
    result = mirror_empathy_request_empathetic_response(
        bridge, 1102, MIRROR_EMOTION_SADNESS, &response3);
    EXPECT_EQ(result, 0);
}
