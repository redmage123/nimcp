/**
 * @file test_emotional_contagion_regression.cpp
 * @brief Regression tests for emotional contagion
 *
 * Ensures contagion dynamics remain consistent
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_emotional_contagion.h"
#include "utils/memory/nimcp_memory.h"

class EmotionalContagionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }
};

TEST_F(EmotionalContagionRegressionTest, PropagationSpeedBaseline) {
    /* Ensure emotions propagate at expected rate */
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);
    config.contagion_rate = 0.5f;

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    /* Create chain of 10 agents */
    for (uint32_t i = 0; i < 10; i++) {
        emotional_contagion_register_agent(ec, i, 0.8f);
    }

    for (uint32_t i = 0; i < 9; i++) {
        emotional_contagion_add_connection(ec, i, i + 1, 0.9f, 1.0f);
    }

    /* Set initial emotion */
    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 1.0f);

    /* Propagate for several steps */
    for (int step = 0; step < 5; step++) {
        emotional_contagion_propagate(ec, 100);
    }

    /* Check that emotion has spread */
    agent_emotional_state_t state;
    emotional_contagion_get_emotional_state(ec, 5, &state);

    /* Should have at least reached middle of chain */
    EXPECT_TRUE(state.emotion == EMOTION_JOY || state.intensity > 0.0f);

    emotional_contagion_destroy(ec);
}

TEST_F(EmotionalContagionRegressionTest, DecayRateConsistency) {
    /* Ensure decay behaves consistently */
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);
    config.decay_rate = 0.1f;

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    emotional_contagion_register_agent(ec, 0, 0.5f);
    emotional_contagion_set_emotion(ec, 0, EMOTION_FEAR, 1.0f);

    /* Apply decay */
    for (int i = 0; i < 10; i++) {
        emotional_contagion_apply_decay(ec, 1000);  /* 1 second */
    }

    /* Emotion should have decayed significantly */
    agent_emotional_state_t state;
    emotional_contagion_get_emotional_state(ec, 0, &state);

    EXPECT_LT(state.intensity, 0.5f);  /* Should have decayed */

    emotional_contagion_destroy(ec);
}

TEST_F(EmotionalContagionRegressionTest, LargeNetworkStability) {
    /* Ensure large networks remain stable */
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);
    config.max_agents = 200;

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    /* Register 100 agents */
    for (uint32_t i = 0; i < 100; i++) {
        EXPECT_EQ(emotional_contagion_register_agent(ec, i, 0.5f), NIMCP_SUCCESS);
    }

    /* Random connections */
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t target = (i + 1 + rand() % 10) % 100;
        emotional_contagion_add_connection(ec, i, target, 0.7f, 1.0f);
    }

    /* Set multiple emotion sources */
    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);
    emotional_contagion_set_emotion(ec, 50, EMOTION_FEAR, 0.8f);

    /* Propagate many times */
    for (int step = 0; step < 20; step++) {
        EXPECT_EQ(emotional_contagion_propagate(ec, 100), NIMCP_SUCCESS);
    }

    /* System should remain stable */
    collective_emotion_state_t collective;
    EXPECT_EQ(emotional_contagion_get_collective_state(ec, &collective),
              NIMCP_SUCCESS);

    emotional_contagion_destroy(ec);
}

TEST_F(EmotionalContagionRegressionTest, StatisticsAccuracy) {
    /* Ensure statistics are tracked accurately */
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    /* Create simple network */
    emotional_contagion_register_agent(ec, 0, 0.8f);
    emotional_contagion_register_agent(ec, 1, 0.8f);
    emotional_contagion_add_connection(ec, 0, 1, 0.9f, 1.0f);

    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);

    /* Propagate */
    emotional_contagion_propagate(ec, 100);

    /* Check statistics */
    emotional_contagion_stats_t stats;
    EXPECT_EQ(emotional_contagion_get_stats(ec, &stats), NIMCP_SUCCESS);

    EXPECT_GT(stats.total_propagations, 0u);

    emotional_contagion_destroy(ec);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
