/**
 * @file test_tom_social_bridge.cpp
 * @brief Unit tests for Theory of Mind - Social Cognition Bridge
 *
 * Tests bidirectional integration between Theory of Mind and Social systems.
 * ToM informs social responses; social cues trigger ToM inference processes.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/integration/nimcp_tom_social_bridge.h"

class TomSocialBridgeTest : public ::testing::Test {
protected:
    tom_social_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = tom_social_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            tom_social_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper to register an agent with the bridge
    void RegisterAgent(uint32_t agent_id) {
        tom_social_belief_update_t update;
        memset(&update, 0, sizeof(update));
        update.belief_type = 0;
        update.belief_value = 0.5f;
        update.confidence = 0.8f;
        update.source = 0;
        tom_social_update_agent_model(bridge, agent_id, &update);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, BridgeCreation) {
    // Bridge was created in SetUp - verify it's not null
    EXPECT_NE(bridge, nullptr);

    // Create and destroy a separate bridge
    tom_social_bridge_t* test_bridge = tom_social_bridge_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    tom_social_bridge_destroy(test_bridge);
}

TEST_F(TomSocialBridgeTest, CreateWithConfig) {
    tom_social_config_t config;
    int ret = tom_social_default_config(&config);
    EXPECT_EQ(ret, 0);

    config.inference_depth = 4;
    config.social_weight = 0.8f;

    tom_social_bridge_t* custom = tom_social_bridge_create(&config);
    ASSERT_NE(custom, nullptr);
    tom_social_bridge_destroy(custom);
}

TEST_F(TomSocialBridgeTest, DestroyNullSafe) {
    // Should not crash
    tom_social_bridge_destroy(nullptr);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, DefaultConfig) {
    tom_social_config_t config;
    memset(&config, 0, sizeof(config));

    int ret = tom_social_default_config(&config);
    EXPECT_EQ(ret, 0);

    // Default values: inference_depth=2, social_weight=0.7, agent_capacity=32
    EXPECT_EQ(config.inference_depth, 2u);
    EXPECT_FLOAT_EQ(config.social_weight, 0.7f);
    EXPECT_EQ(config.agent_capacity, 32u);
}

TEST_F(TomSocialBridgeTest, DefaultConfigNullPointer) {
    int ret = tom_social_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, DefaultConfigAdditionalFields) {
    tom_social_config_t config;
    int ret = tom_social_default_config(&config);
    EXPECT_EQ(ret, 0);

    // Check additional fields have sensible defaults
    EXPECT_GE(config.inference_confidence_threshold, 0.0f);
    EXPECT_LE(config.inference_confidence_threshold, 1.0f);
}

//=============================================================================
// Mental State Inference Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, InferForResponse) {
    uint32_t agent_id = 101;

    // First register the agent
    RegisterAgent(agent_id);

    tom_social_mental_state_t mental_state;
    memset(&mental_state, 0, sizeof(mental_state));

    int ret = tom_social_infer_for_response(bridge, agent_id, &mental_state);
    EXPECT_EQ(ret, 0);

    // Verify mental state fields are populated
    EXPECT_EQ(mental_state.agent_id, agent_id);
    EXPECT_GE(mental_state.confidence, 0.0f);
    EXPECT_LE(mental_state.confidence, 1.0f);
    EXPECT_GE(mental_state.emotional_valence, -1.0f);
    EXPECT_LE(mental_state.emotional_valence, 1.0f);
    EXPECT_GE(mental_state.emotional_arousal, 0.0f);
    EXPECT_LE(mental_state.emotional_arousal, 1.0f);
}

TEST_F(TomSocialBridgeTest, InferForResponseUnknown) {
    uint32_t unknown_agent_id = 99999;

    tom_social_mental_state_t mental_state;
    memset(&mental_state, 0, sizeof(mental_state));

    // Unknown agent should return -1
    int ret = tom_social_infer_for_response(bridge, unknown_agent_id, &mental_state);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, InferForResponseNullBridge) {
    tom_social_mental_state_t mental_state;
    int ret = tom_social_infer_for_response(nullptr, 1, &mental_state);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, InferForResponseNullOutput) {
    RegisterAgent(1);
    int ret = tom_social_infer_for_response(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Social Cue Processing Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, OnSocialCue) {
    // Process a facial expression cue
    float cue_data = 0.8f;  // Example cue data
    int ret = tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_FACIAL_EXPRESSION, &cue_data);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomSocialBridgeTest, OnSocialCueAllTypes) {
    float cue_data = 0.5f;

    // Test all cue types
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_FACIAL_EXPRESSION, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_BODY_LANGUAGE, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_VOCAL_TONE, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_GAZE_DIRECTION, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_PROXIMITY, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_GESTURE, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_VERBAL_CONTENT, &cue_data), 0);
    EXPECT_EQ(tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_CONTEXT, &cue_data), 0);
}

TEST_F(TomSocialBridgeTest, OnSocialCueNullBridge) {
    float cue_data = 0.5f;
    int ret = tom_social_on_social_cue(nullptr, TOM_SOCIAL_CUE_FACIAL_EXPRESSION, &cue_data);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, OnSocialCueNullData) {
    // Note: Implementation allows null data (gracefully handles it)
    int ret = tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_FACIAL_EXPRESSION, nullptr);
    EXPECT_GE(ret, 0);  // Null data is allowed, not an error
}

//=============================================================================
// Agent Model Update Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, UpdateAgentModel) {
    uint32_t agent_id = 201;

    tom_social_belief_update_t update;
    memset(&update, 0, sizeof(update));
    update.belief_type = 1;
    update.belief_value = 0.7f;
    update.confidence = 0.9f;
    update.source = 0;  // ToM source

    int ret = tom_social_update_agent_model(bridge, agent_id, &update);
    EXPECT_EQ(ret, 0);
}

TEST_F(TomSocialBridgeTest, UpdateAgentModelMultipleTimes) {
    uint32_t agent_id = 202;

    for (int i = 0; i < 5; i++) {
        tom_social_belief_update_t update;
        memset(&update, 0, sizeof(update));
        update.belief_type = (uint32_t)i;
        update.belief_value = (float)i * 0.2f;
        update.confidence = 0.8f;
        update.source = (uint32_t)(i % 3);

        int ret = tom_social_update_agent_model(bridge, agent_id, &update);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(TomSocialBridgeTest, UpdateAgentModelNullBridge) {
    tom_social_belief_update_t update = {0};
    int ret = tom_social_update_agent_model(nullptr, 1, &update);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, UpdateAgentModelNullUpdate) {
    int ret = tom_social_update_agent_model(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Agent State Query Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, GetAgentState) {
    uint32_t agent_id = 301;

    // First register the agent
    RegisterAgent(agent_id);

    tom_social_agent_state_t state;
    memset(&state, 0, sizeof(state));

    int ret = tom_social_get_agent_state(bridge, agent_id, &state);
    EXPECT_EQ(ret, 0);

    // Verify state is valid
    EXPECT_TRUE(state.is_valid);
    EXPECT_EQ(state.mental_state.agent_id, agent_id);
    EXPECT_GE(state.model_stability, 0.0f);
    EXPECT_LE(state.model_stability, 1.0f);
}

TEST_F(TomSocialBridgeTest, GetAgentStateUnknown) {
    uint32_t unknown_agent_id = 88888;

    tom_social_agent_state_t state;
    memset(&state, 0, sizeof(state));

    int ret = tom_social_get_agent_state(bridge, unknown_agent_id, &state);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, GetAgentStateNullBridge) {
    tom_social_agent_state_t state;
    int ret = tom_social_get_agent_state(nullptr, 1, &state);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, GetAgentStateNullOutput) {
    RegisterAgent(1);
    int ret = tom_social_get_agent_state(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, StatsTracking) {
    tom_social_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Perform some operations
    // 1. Process social cues
    for (int i = 0; i < 3; i++) {
        float cue_data = 0.5f;
        tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_FACIAL_EXPRESSION, &cue_data);
    }

    // 2. Update agent models
    for (uint32_t i = 0; i < 4; i++) {
        tom_social_belief_update_t update = {0, 0.5f, 0.8f, 0};
        tom_social_update_agent_model(bridge, i + 1, &update);
    }

    // 3. Make inferences
    for (uint32_t i = 0; i < 2; i++) {
        tom_social_mental_state_t mental_state;
        tom_social_infer_for_response(bridge, i + 1, &mental_state);
    }

    // Get stats
    int ret = tom_social_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Verify stats are tracked
    EXPECT_GE(stats.inferences_made, 2u);
    EXPECT_GE(stats.social_cues_processed, 3u);
    EXPECT_GE(stats.agent_models_updated, 4u);
    EXPECT_GE(stats.active_agents, 0u);
    EXPECT_GE(stats.avg_inference_confidence, 0.0f);
    EXPECT_LE(stats.avg_inference_confidence, 1.0f);
}

TEST_F(TomSocialBridgeTest, StatsNullBridge) {
    tom_social_stats_t stats;
    int ret = tom_social_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, StatsNullOutput) {
    int ret = tom_social_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(TomSocialBridgeTest, StatsInitialValues) {
    tom_social_stats_t stats;
    int ret = tom_social_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.inferences_made, 0u);
    EXPECT_EQ(stats.social_cues_processed, 0u);
    EXPECT_EQ(stats.agent_models_updated, 0u);
    EXPECT_EQ(stats.inference_failures, 0u);
    EXPECT_EQ(stats.cue_failures, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(TomSocialBridgeTest, FullSocialInteractionPipeline) {
    uint32_t agent_id = 401;

    // 1. Observe social cue from agent
    float cue_data = 0.6f;
    int ret = tom_social_on_social_cue(bridge, TOM_SOCIAL_CUE_FACIAL_EXPRESSION, &cue_data);
    EXPECT_EQ(ret, 0);

    // 2. Update agent model based on cue
    tom_social_belief_update_t update;
    memset(&update, 0, sizeof(update));
    update.belief_type = 1;  // Emotion belief
    update.belief_value = 0.7f;  // Positive emotion
    update.confidence = 0.85f;
    update.source = 1;  // Social source

    ret = tom_social_update_agent_model(bridge, agent_id, &update);
    EXPECT_EQ(ret, 0);

    // 3. Infer mental state for response
    tom_social_mental_state_t mental_state;
    ret = tom_social_infer_for_response(bridge, agent_id, &mental_state);
    EXPECT_EQ(ret, 0);

    // 4. Get full agent state
    tom_social_agent_state_t state;
    ret = tom_social_get_agent_state(bridge, agent_id, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(state.is_valid);

    // 5. Verify stats
    tom_social_stats_t stats;
    ret = tom_social_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.social_cues_processed, 1u);
    EXPECT_GE(stats.agent_models_updated, 1u);
    EXPECT_GE(stats.inferences_made, 1u);
}

TEST_F(TomSocialBridgeTest, MultipleAgentTracking) {
    const uint32_t num_agents = 10;

    // Register multiple agents
    for (uint32_t i = 0; i < num_agents; i++) {
        tom_social_belief_update_t update = {0, (float)i * 0.1f, 0.8f, 0};
        int ret = tom_social_update_agent_model(bridge, i + 1, &update);
        EXPECT_EQ(ret, 0);
    }

    // Infer for each agent
    for (uint32_t i = 0; i < num_agents; i++) {
        tom_social_mental_state_t mental_state;
        int ret = tom_social_infer_for_response(bridge, i + 1, &mental_state);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(mental_state.agent_id, i + 1);
    }

    // Verify stats
    tom_social_stats_t stats;
    tom_social_get_stats(bridge, &stats);
    EXPECT_EQ(stats.agent_models_updated, num_agents);
    EXPECT_EQ(stats.inferences_made, num_agents);
    EXPECT_GE(stats.active_agents, 0u);
}

TEST_F(TomSocialBridgeTest, AgentCapacityLimit) {
    tom_social_config_t config;
    tom_social_default_config(&config);
    config.agent_capacity = 5;  // Small capacity

    tom_social_bridge_t* limited_bridge = tom_social_bridge_create(&config);
    ASSERT_NE(limited_bridge, nullptr);

    // Register agents up to capacity
    for (uint32_t i = 0; i < 5; i++) {
        tom_social_belief_update_t update = {0, 0.5f, 0.8f, 0};
        int ret = tom_social_update_agent_model(limited_bridge, i + 1, &update);
        EXPECT_EQ(ret, 0);
    }

    tom_social_bridge_destroy(limited_bridge);
}
