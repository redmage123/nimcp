/**
 * @file test_emotional_contagion_integration.cpp
 * @brief Integration tests for emotional contagion with bio-async
 *
 * Tests integration with:
 * - Bio-async messaging
 * - Swarm coordination
 * - Multi-agent systems
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_emotional_contagion.h"
#include "utils/memory/nimcp_memory.h"

class EmotionalContagionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }
};

TEST_F(EmotionalContagionIntegrationTest, CreateAndDestroy) {
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    emotional_contagion_destroy(ec);
}

TEST_F(EmotionalContagionIntegrationTest, LargeScaleNetwork) {
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);
    config.max_agents = 100;

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    /* Register many agents */
    for (uint32_t i = 0; i < 50; i++) {
        EXPECT_EQ(emotional_contagion_register_agent(ec, i, 0.5f), NIMCP_SUCCESS);
    }

    /* Create connections */
    for (uint32_t i = 0; i < 49; i++) {
        emotional_contagion_add_connection(ec, i, i + 1, 0.8f, 1.0f);
    }

    /* Set initial emotion */
    emotional_contagion_set_emotion(ec, 0, EMOTION_JOY, 0.9f);

    /* Propagate multiple times */
    for (int step = 0; step < 10; step++) {
        emotional_contagion_propagate(ec, 100);
    }

    /* Check collective state */
    collective_emotion_state_t collective;
    EXPECT_EQ(emotional_contagion_get_collective_state(ec, &collective),
              NIMCP_SUCCESS);

    emotional_contagion_destroy(ec);
}

TEST_F(EmotionalContagionIntegrationTest, ComplexTopology) {
    emotional_contagion_config_t config;
    emotional_contagion_get_default_config(&config);

    emotional_contagion_t* ec = emotional_contagion_create(&config);
    ASSERT_NE(ec, nullptr);

    /* Create fully connected network */
    const uint32_t num_agents = 5;
    for (uint32_t i = 0; i < num_agents; i++) {
        emotional_contagion_register_agent(ec, i, 0.6f);
    }

    /* Fully connect */
    for (uint32_t i = 0; i < num_agents; i++) {
        for (uint32_t j = 0; j < num_agents; j++) {
            if (i != j) {
                emotional_contagion_add_connection(ec, i, j, 0.7f, 1.0f);
            }
        }
    }

    /* Trigger emotion */
    emotional_contagion_set_emotion(ec, 0, EMOTION_FEAR, 0.8f);

    /* Single propagation should spread to all */
    emotional_contagion_propagate(ec, 100);

    /* Check spread */
    emotional_contagion_stats_t stats;
    emotional_contagion_get_stats(ec, &stats);
    EXPECT_GT(stats.total_propagations, 0u);

    emotional_contagion_destroy(ec);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
