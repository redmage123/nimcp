/**
 * @file test_emotional_contagion.cpp
 * @brief Unit tests for emotional contagion module
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Agent registration and management
 * - Emotion setting and getting
 * - Connection management
 * - Emotion propagation
 * - Decay mechanics
 * - Susceptibility and resistance
 * - Collective mood tracking
 * - Statistics
 * - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "swarm/nimcp_emotional_contagion.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionalContagionTest : public ::testing::Test {
protected:
    emotional_contagion_t* ec;
    emotional_contagion_config_t config;

    void SetUp() override {
        /* Initialize memory system */
        nimcp_memory_init();

        /* Get default configuration */
        emotional_contagion_get_default_config(&config);

        /* Create contagion system */
        ec = emotional_contagion_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            emotional_contagion_destroy(ec);
            ec = nullptr;
        }
    }

    /* Helper: Register test agents */
    void registerTestAgents(uint32_t count, float susceptibility = 0.5f) {
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_EQ(emotional_contagion_register_agent(ec, i, susceptibility),
                      NIMCP_SUCCESS);
        }
    }

    /* Helper: Create simple network */
    void createSimpleNetwork() {
        /* Register 3 agents */
        registerTestAgents(3);

        /* Connect: 0 -> 1 -> 2 */
        EXPECT_EQ(emotional_contagion_add_connection(ec, 0, 1, 0.8f, 1.0f),
                  NIMCP_SUCCESS);
        EXPECT_EQ(emotional_contagion_add_connection(ec, 1, 2, 0.8f, 1.0f),
                  NIMCP_SUCCESS);
    }
};

//=============================================================================
// Creation and Configuration Tests
//=============================================================================

TEST_F(EmotionalContagionTest, CreateValidSystem) {
    EXPECT_NE(ec, nullptr);
}

TEST_F(EmotionalContagionTest, CreateWithCustomConfig) {
    emotional_contagion_config_t custom_config;
    emotional_contagion_get_default_config(&custom_config);

    custom_config.contagion_rate = 0.5f;
    custom_config.decay_rate = 0.2f;
    custom_config.max_agents = 500;

    emotional_contagion_t* custom_ec = emotional_contagion_create(&custom_config);
    ASSERT_NE(custom_ec, nullptr);

    emotional_contagion_destroy(custom_ec);
}

TEST_F(EmotionalContagionTest, ValidateConfiguration) {
    emotional_contagion_config_t valid_config;
    emotional_contagion_get_default_config(&valid_config);

    EXPECT_EQ(emotional_contagion_validate_config(&valid_config), NIMCP_SUCCESS);

    /* Invalid contagion_rate */
    valid_config.contagion_rate = 1.5f;
    EXPECT_NE(emotional_contagion_validate_config(&valid_config), NIMCP_SUCCESS);

    /* Reset and test invalid max_agents */
    emotional_contagion_get_default_config(&valid_config);
    valid_config.max_agents = 0;
    EXPECT_NE(emotional_contagion_validate_config(&valid_config), NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, GetDefaultConfig) {
    emotional_contagion_config_t default_config;
    emotional_contagion_get_default_config(&default_config);

    EXPECT_GT(default_config.contagion_rate, 0.0f);
    EXPECT_LE(default_config.contagion_rate, 1.0f);
    EXPECT_GT(default_config.max_agents, 0u);
}

TEST_F(EmotionalContagionTest, ResetSystem) {
    /* Register agents and set emotions */
    registerTestAgents(3);
    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.8f);
    emotional_contagion_set_emotion(ec, 1, EMOTION_FEAR, 0.6f);

    /* Reset */
    EXPECT_EQ(emotional_contagion_reset(ec, true), NIMCP_SUCCESS);

    /* Check agents are neutral */
    agent_emotional_state_t state;
    EXPECT_EQ(emotional_contagion_get_emotional_state(ec, 0, &state), NIMCP_SUCCESS);
    EXPECT_EQ(state.emotion, EMOTION_NEUTRAL);
    EXPECT_EQ(state.intensity, 0.0f);
}

//=============================================================================
// Agent Management Tests
//=============================================================================

TEST_F(EmotionalContagionTest, RegisterAgent) {
    uint32_t agent_id = 100;
    float susceptibility = 0.7f;

    EXPECT_EQ(emotional_contagion_register_agent(ec, agent_id, susceptibility),
              NIMCP_SUCCESS);

    /* Verify agent state */
    agent_emotional_state_t state;
    EXPECT_EQ(emotional_contagion_get_emotional_state(ec, agent_id, &state),
              NIMCP_SUCCESS);
    EXPECT_EQ(state.agent_id, agent_id);
    EXPECT_EQ(state.emotion, EMOTION_NEUTRAL);
    EXPECT_EQ(state.susceptibility, susceptibility);
}

TEST_F(EmotionalContagionTest, RegisterMultipleAgents) {
    const uint32_t count = 10;
    registerTestAgents(count);

    /* All should be registered */
    for (uint32_t i = 0; i < count; i++) {
        agent_emotional_state_t state;
        EXPECT_EQ(emotional_contagion_get_emotional_state(ec, i, &state),
                  NIMCP_SUCCESS);
    }
}

TEST_F(EmotionalContagionTest, RejectDuplicateAgent) {
    uint32_t agent_id = 1;

    EXPECT_EQ(emotional_contagion_register_agent(ec, agent_id, 0.5f), NIMCP_SUCCESS);

    /* Try to register again */
    EXPECT_EQ(emotional_contagion_register_agent(ec, agent_id, 0.5f),
              NIMCP_ALREADY_EXISTS);
}

TEST_F(EmotionalContagionTest, RejectInvalidSusceptibility) {
    EXPECT_NE(emotional_contagion_register_agent(ec, 1, -0.1f), NIMCP_SUCCESS);
    EXPECT_NE(emotional_contagion_register_agent(ec, 2, 1.5f), NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, UnregisterAgent) {
    uint32_t agent_id = 1;

    emotional_contagion_register_agent(ec, agent_id, 0.5f);
    EXPECT_EQ(emotional_contagion_unregister_agent(ec, agent_id), NIMCP_SUCCESS);

    /* Should not be found */
    agent_emotional_state_t state;
    EXPECT_EQ(emotional_contagion_get_emotional_state(ec, agent_id, &state),
              NIMCP_ERROR_NOT_FOUND);
}

TEST_F(EmotionalContagionTest, UnregisterNonexistentAgent) {
    EXPECT_EQ(emotional_contagion_unregister_agent(ec, 999), NIMCP_ERROR_NOT_FOUND);
}

//=============================================================================
// Emotion Setting and Getting Tests
//=============================================================================

TEST_F(EmotionalContagionTest, SetEmotion) {
    registerTestAgents(1);

    EXPECT_EQ(emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.8f),
              NIMCP_SUCCESS);

    agent_emotional_state_t state;
    EXPECT_EQ(emotional_contagion_get_emotional_state(ec, 0, &state), NIMCP_SUCCESS);
    EXPECT_EQ(state.emotion, EMOTION_JOY);
    EXPECT_FLOAT_EQ(state.intensity, 0.8f);
}

TEST_F(EmotionalContagionTest, SetMultipleEmotions) {
    registerTestAgents(3);

    EXPECT_EQ(emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f), NIMCP_SUCCESS);
    EXPECT_EQ(emotional_contagion_set_emotion(ec, 1, EMOTION_FEAR, 0.7f), NIMCP_SUCCESS);
    EXPECT_EQ(emotional_contagion_set_emotion(ec, 2, EMOTION_ANGER, 0.5f), NIMCP_SUCCESS);

    /* Verify each */
    agent_emotional_state_t state;

    emotional_contagion_get_emotional_state(ec, 0, &state);
    EXPECT_EQ(state.emotion, EMOTION_JOY);

    emotional_contagion_get_emotional_state(ec, 1, &state);
    EXPECT_EQ(state.emotion, EMOTION_FEAR);

    emotional_contagion_get_emotional_state(ec, 2, &state);
    EXPECT_EQ(state.emotion, EMOTION_ANGER);
}

TEST_F(EmotionalContagionTest, RejectInvalidEmotion) {
    registerTestAgents(1);

    emotion_type_t invalid_emotion = (emotion_type_t)999;
    EXPECT_NE(emotional_contagion_set_emotion(ec, 0, invalid_emotion, 0.5f),
              NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, RejectInvalidIntensity) {
    registerTestAgents(1);

    EXPECT_NE(emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, -0.1f), NIMCP_SUCCESS);
    EXPECT_NE(emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 1.5f), NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, UpdateSusceptibility) {
    registerTestAgents(1);

    float new_susceptibility = 0.9f;
    EXPECT_EQ(emotional_contagion_set_susceptibility(ec, 0, new_susceptibility),
              NIMCP_SUCCESS);

    agent_emotional_state_t state;
    emotional_contagion_get_emotional_state(ec, 0, &state);
    EXPECT_FLOAT_EQ(state.susceptibility, new_susceptibility);
}

//=============================================================================
// Connection Management Tests
//=============================================================================

TEST_F(EmotionalContagionTest, AddConnection) {
    registerTestAgents(2);

    EXPECT_EQ(emotional_contagion_add_connection(ec, 0, 1, 0.8f, 1.0f),
              NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, AddMultipleConnections) {
    registerTestAgents(4);

    /* Create star topology */
    EXPECT_EQ(emotional_contagion_add_connection(ec, 0, 1, 0.8f, 1.0f), NIMCP_SUCCESS);
    EXPECT_EQ(emotional_contagion_add_connection(ec, 0, 2, 0.8f, 1.0f), NIMCP_SUCCESS);
    EXPECT_EQ(emotional_contagion_add_connection(ec, 0, 3, 0.8f, 1.0f), NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, RemoveConnection) {
    registerTestAgents(2);

    emotional_contagion_add_connection(ec, 0, 1, 0.8f, 1.0f);
    EXPECT_EQ(emotional_contagion_remove_connection(ec, 0, 1), NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, UpdateConnection) {
    registerTestAgents(2);

    emotional_contagion_add_connection(ec, 0, 1, 0.5f, 0.5f);

    EXPECT_EQ(emotional_contagion_update_connection(ec, 0, 1, 0.9f, 1.0f),
              NIMCP_SUCCESS);
}

TEST_F(EmotionalContagionTest, RejectConnectionToNonexistentAgent) {
    registerTestAgents(1);

    EXPECT_NE(emotional_contagion_add_connection(ec, 0, 999, 0.8f, 1.0f),
              NIMCP_SUCCESS);
}

//=============================================================================
// Propagation Tests
//=============================================================================

TEST_F(EmotionalContagionTest, PropagateEmotion) {
    createSimpleNetwork();

    /* Set emotion on agent 0 */
    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);

    /* Propagate */
    EXPECT_EQ(emotional_contagion_propagate(ec, 100), NIMCP_SUCCESS);

    /* Agent 1 should have received some joy */
    agent_emotional_state_t state;
    emotional_contagion_get_emotional_state(ec, 1, &state);

    /* Due to contagion, should have some emotion (though might be weak) */
    EXPECT_TRUE(state.emotion == EMOTION_JOY || state.intensity > 0.0f);
}

TEST_F(EmotionalContagionTest, PropagateMultipleSteps) {
    createSimpleNetwork();

    emotional_contagion_set_emotion(ec, 0, EMOTION_FEAR, 0.8f);

    /* Multiple propagation steps */
    for (int i = 0; i < 5; i++) {
        emotional_contagion_propagate(ec, 100);
    }

    /* Emotion should have spread down the chain */
    agent_emotional_state_t state1, state2;
    emotional_contagion_get_emotional_state(ec, 1, &state1);
    emotional_contagion_get_emotional_state(ec, 2, &state2);

    /* At least one should have fear */
    EXPECT_TRUE(state1.emotion == EMOTION_FEAR || state2.emotion == EMOTION_FEAR);
}

TEST_F(EmotionalContagionTest, BlockBySusceptibilityThreshold) {
    registerTestAgents(2, 0.1f);  /* Very low susceptibility */

    emotional_contagion_add_connection(ec, 0, 1, 0.8f, 1.0f);
    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);

    /* Propagate */
    emotional_contagion_propagate(ec, 100);

    /* Agent 1 should block due to low susceptibility */
    emotional_contagion_stats_t stats;
    emotional_contagion_get_stats(ec, &stats);
    EXPECT_GT(stats.blocked_by_susceptibility, 0u);
}

//=============================================================================
// Decay Tests
//=============================================================================

TEST_F(EmotionalContagionTest, EmotionDecay) {
    registerTestAgents(1);

    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);

    /* Apply decay over time */
    for (int i = 0; i < 10; i++) {
        emotional_contagion_apply_decay(ec, 500);  /* 500ms steps */
    }

    /* Emotion should have decayed */
    agent_emotional_state_t state;
    emotional_contagion_get_emotional_state(ec, 0, &state);

    EXPECT_LT(state.intensity, 0.9f);  /* Should have decreased */
}

TEST_F(EmotionalContagionTest, DecayToNeutral) {
    registerTestAgents(1);

    emotional_contagion_set_emotion(ec, 0, EMOTION_SADNESS, 0.5f);

    /* Apply heavy decay - need ~200 iterations for 0.5 * exp(-0.1*200) < 1e-6 EPSILON */
    for (int i = 0; i < 200; i++) {
        emotional_contagion_apply_decay(ec, 1000);
    }

    /* Should return to neutral */
    agent_emotional_state_t state;
    emotional_contagion_get_emotional_state(ec, 0, &state);

    EXPECT_EQ(state.emotion, EMOTION_NEUTRAL);
    EXPECT_FLOAT_EQ(state.intensity, 0.0f);
}

//=============================================================================
// Collective State Tests
//=============================================================================

TEST_F(EmotionalContagionTest, GetDominantEmotion) {
    registerTestAgents(5);

    /* Set most agents to joy */
    for (uint32_t i = 0; i < 4; i++) {
        emotional_contagion_set_emotion(ec, i, EMOTION_JOY, 0.7f);
    }
    emotional_contagion_set_emotion(ec, 4, EMOTION_FEAR, 0.8f);

    /* Get dominant */
    emotion_type_t dominant;
    float avg_intensity;
    EXPECT_EQ(emotional_contagion_get_dominant_emotion(ec, &dominant, &avg_intensity),
              NIMCP_SUCCESS);

    EXPECT_EQ(dominant, EMOTION_JOY);
}

TEST_F(EmotionalContagionTest, GetCollectiveState) {
    registerTestAgents(3);

    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.8f);
    emotional_contagion_set_emotion(ec, 1, EMOTION_JOY, 0.7f);
    emotional_contagion_set_emotion(ec, 2, EMOTION_FEAR, 0.6f);

    collective_emotion_state_t collective;
    EXPECT_EQ(emotional_contagion_get_collective_state(ec, &collective),
              NIMCP_SUCCESS);

    EXPECT_EQ(collective.dominant_emotion, EMOTION_JOY);
    EXPECT_GT(collective.avg_intensity, 0.0f);
    EXPECT_GE(collective.emotional_diversity, 0.0f);
    EXPECT_LE(collective.emotional_diversity, 1.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EmotionalContagionTest, TrackStatistics) {
    createSimpleNetwork();

    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);
    emotional_contagion_propagate(ec, 100);

    emotional_contagion_stats_t stats;
    EXPECT_EQ(emotional_contagion_get_stats(ec, &stats), NIMCP_SUCCESS);

    EXPECT_GT(stats.total_propagations, 0u);
}

TEST_F(EmotionalContagionTest, ResetStatistics) {
    createSimpleNetwork();

    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);
    emotional_contagion_propagate(ec, 100);

    EXPECT_EQ(emotional_contagion_reset_stats(ec), NIMCP_SUCCESS);

    emotional_contagion_stats_t stats;
    emotional_contagion_get_stats(ec, &stats);

    EXPECT_EQ(stats.total_propagations, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(EmotionalContagionTest, EmotionNameMapping) {
    const char* name = emotional_contagion_emotion_name(EMOTION_JOY);
    EXPECT_STREQ(name, "joy");

    emotion_type_t emotion = emotional_contagion_emotion_from_name("fear");
    EXPECT_EQ(emotion, EMOTION_FEAR);

    /* Case insensitive */
    emotion = emotional_contagion_emotion_from_name("ANGER");
    EXPECT_EQ(emotion, EMOTION_ANGER);

    /* Unknown emotion */
    emotion = emotional_contagion_emotion_from_name("unknown_emotion");
    EXPECT_EQ(emotion, EMOTION_NEUTRAL);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(EmotionalContagionTest, HandleNullPointers) {
    EXPECT_EQ(emotional_contagion_create(nullptr), nullptr);

    agent_emotional_state_t state;
    EXPECT_EQ(emotional_contagion_get_emotional_state(nullptr, 0, &state),
              NIMCP_ERROR_NULL_POINTER);

    emotional_contagion_stats_t stats;
    EXPECT_EQ(emotional_contagion_get_stats(nullptr, &stats),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(EmotionalContagionTest, DestroyNullSystem) {
    /* Should not crash */
    emotional_contagion_destroy(nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
