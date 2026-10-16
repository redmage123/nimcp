/**
 * @file test_gossip_beliefs_memory.cpp
 * @brief Memory management tests for NIMCP Gossip-Based Belief Propagation System
 *
 * TEST COVERAGE:
 * - Memory leak prevention during destroy
 * - Belief vector memory management
 * - Agent and belief cleanup callbacks
 * - Repeated create/destroy cycles
 * - Memory stress testing
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_gossip_beliefs.h"
#include "utils/memory/nimcp_memory.h"

class GossipBeliefsMemoryTest : public ::testing::Test {
protected:
    gossip_beliefs_t* system;

    void SetUp() override {
        gossip_beliefs_config_t config = {
            .gossip_probability = 0.5f,
            .max_gossip_targets = 3,
            .belief_decay_rate = 0.001f,
            .credibility_weight = 0.5f,
            .enable_contradiction_detection = true,
            .enable_bio_async = false
        };

        system = gossip_beliefs_create(&config);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(gossip_beliefs_init(system, nullptr), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (system) {
            gossip_beliefs_destroy(system);
            system = nullptr;
        }
    }

    belief_t* create_test_belief(const char* topic, float certainty) {
        float vector[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
        return belief_create(topic, vector, 5, certainty, 123);
    }
};

/* ============================================================================
 * Cleanup Callback Tests
 * ============================================================================ */

TEST_F(GossipBeliefsMemoryTest, CleanupCallbackFreesBeliefVectors) {
    /* Register agents and add beliefs */
    gossip_beliefs_register_agent(system, 1, 1.0f);
    gossip_beliefs_register_agent(system, 2, 0.8f);
    gossip_beliefs_register_agent(system, 3, 0.6f);

    /* Add multiple beliefs per agent */
    for (int i = 0; i < 5; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), "test_topic_%d", i);
        belief_t* belief = create_test_belief(topic, 0.9f);
        ASSERT_NE(belief, nullptr);
        EXPECT_EQ(gossip_beliefs_introduce(system, 1, belief), NIMCP_SUCCESS);
    }

    /* Propagate to other agents */
    for (int round = 0; round < 10; round++) {
        gossip_beliefs_propagate(system);
    }

    /* Destroy should free all beliefs without leaks */
    gossip_beliefs_destroy(system);
    system = nullptr;

    SUCCEED();  /* If we get here without crash/leak, test passes */
}

TEST_F(GossipBeliefsMemoryTest, DestroyEmptySystemNoLeak) {
    /* Empty system should destroy cleanly */
    gossip_beliefs_destroy(system);
    system = nullptr;
    SUCCEED();
}

TEST_F(GossipBeliefsMemoryTest, DestroySystemWithAgentsNoBeliefs) {
    /* Register agents but add no beliefs */
    gossip_beliefs_register_agent(system, 1, 1.0f);
    gossip_beliefs_register_agent(system, 2, 0.8f);
    gossip_beliefs_register_agent(system, 3, 0.6f);

    gossip_beliefs_destroy(system);
    system = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Repeated Create/Destroy Cycle Tests
 * ============================================================================ */

TEST_F(GossipBeliefsMemoryTest, RepeatedCreateDestroyCycles) {
    /* First destroy the one from SetUp */
    gossip_beliefs_destroy(system);
    system = nullptr;

    gossip_beliefs_config_t config = {
        .gossip_probability = 0.5f,
        .max_gossip_targets = 3,
        .belief_decay_rate = 0.001f,
        .credibility_weight = 0.5f,
        .enable_contradiction_detection = true,
        .enable_bio_async = false
    };

    /* Repeatedly create, populate, and destroy */
    for (int cycle = 0; cycle < 10; cycle++) {
        gossip_beliefs_t* sys = gossip_beliefs_create(&config);
        ASSERT_NE(sys, nullptr);
        ASSERT_EQ(gossip_beliefs_init(sys, nullptr), NIMCP_SUCCESS);

        /* Add agents and beliefs */
        gossip_beliefs_register_agent(sys, 1, 1.0f);
        gossip_beliefs_register_agent(sys, 2, 0.8f);

        for (int i = 0; i < 3; i++) {
            char topic[64];
            snprintf(topic, sizeof(topic), "cycle_%d_topic_%d", cycle, i);
            belief_t* belief = create_test_belief(topic, 0.9f);
            ASSERT_NE(belief, nullptr);
            gossip_beliefs_introduce(sys, 1, belief);
        }

        gossip_beliefs_propagate(sys);
        gossip_beliefs_destroy(sys);
    }

    SUCCEED();
}

/* ============================================================================
 * Memory Stress Tests
 * ============================================================================ */

TEST_F(GossipBeliefsMemoryTest, ManyAgentsManyBeliefs) {
    /* Register many agents */
    for (uint32_t i = 0; i < 50; i++) {
        EXPECT_EQ(gossip_beliefs_register_agent(system, i, 1.0f - (i * 0.01f)),
                  NIMCP_SUCCESS);
    }

    /* Each agent introduces beliefs */
    for (uint32_t agent = 0; agent < 50; agent++) {
        for (int belief_idx = 0; belief_idx < 5; belief_idx++) {
            char topic[64];
            snprintf(topic, sizeof(topic), "agent_%u_belief_%d", agent, belief_idx);
            belief_t* belief = create_test_belief(topic, 0.8f);
            if (belief) {
                gossip_beliefs_introduce(system, agent, belief);
            }
        }
    }

    /* Multiple propagation rounds */
    for (int round = 0; round < 20; round++) {
        gossip_beliefs_propagate(system);
        gossip_beliefs_decay_beliefs(system, 100);  /* Simulate 100ms */
    }

    /* Get statistics to verify system state */
    gossip_beliefs_stats_t stats;
    gossip_beliefs_get_stats(system, &stats);
    EXPECT_GT(stats.total_beliefs, 0u);
    EXPECT_GT(stats.gossip_rounds, 0u);

    /* Destroy cleans up everything */
    gossip_beliefs_destroy(system);
    system = nullptr;
    SUCCEED();
}

TEST_F(GossipBeliefsMemoryTest, LargeBeliefVectors) {
    gossip_beliefs_register_agent(system, 1, 1.0f);

    /* Create beliefs with large vectors */
    for (int i = 0; i < 10; i++) {
        float* large_vector = (float*)nimcp_malloc(1000 * sizeof(float));
        ASSERT_NE(large_vector, nullptr);

        for (int j = 0; j < 1000; j++) {
            large_vector[j] = (float)j / 1000.0f;
        }

        char topic[64];
        snprintf(topic, sizeof(topic), "large_topic_%d", i);
        belief_t* belief = belief_create(topic, large_vector, 1000, 0.9f, 123);
        nimcp_free(large_vector);  /* belief_create copies the vector */

        if (belief) {
            gossip_beliefs_introduce(system, 1, belief);
        }
    }

    /* Destroy should free all large vectors */
    gossip_beliefs_destroy(system);
    system = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Edge Case Memory Tests
 * ============================================================================ */

TEST_F(GossipBeliefsMemoryTest, DestroyAfterAgentRemoval) {
    gossip_beliefs_register_agent(system, 1, 1.0f);
    gossip_beliefs_register_agent(system, 2, 0.8f);

    /* Add beliefs to agents */
    belief_t* belief1 = create_test_belief("topic1", 0.9f);
    belief_t* belief2 = create_test_belief("topic2", 0.8f);

    gossip_beliefs_introduce(system, 1, belief1);
    gossip_beliefs_introduce(system, 2, belief2);

    gossip_beliefs_propagate(system);

    /* Remove an agent */
    gossip_beliefs_unregister_agent(system, 1);

    /* Destroy should still work correctly */
    gossip_beliefs_destroy(system);
    system = nullptr;
    SUCCEED();
}

TEST_F(GossipBeliefsMemoryTest, DestroyAfterClear) {
    gossip_beliefs_register_agent(system, 1, 1.0f);

    for (int i = 0; i < 10; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), "topic_%d", i);
        belief_t* belief = create_test_belief(topic, 0.9f);
        gossip_beliefs_introduce(system, 1, belief);
    }

    /* Clear all beliefs */
    gossip_beliefs_clear(system);

    /* Destroy empty system */
    gossip_beliefs_destroy(system);
    system = nullptr;
    SUCCEED();
}

TEST_F(GossipBeliefsMemoryTest, DoubleDestroyGuard) {
    gossip_beliefs_destroy(system);
    system = nullptr;

    /* Second destroy of null should be safe */
    gossip_beliefs_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Belief Creation and Destruction Tests
 * ============================================================================ */

TEST_F(GossipBeliefsMemoryTest, BeliefCreateDestroy) {
    float vector[] = {0.1f, 0.2f, 0.3f};

    for (int i = 0; i < 100; i++) {
        belief_t* belief = belief_create("test", vector, 3, 0.9f, 123);
        ASSERT_NE(belief, nullptr);
        belief_destroy(belief);
    }

    SUCCEED();
}

TEST_F(GossipBeliefsMemoryTest, BeliefVectorOwnership) {
    /* Create belief - it should copy the vector */
    float original_vector[] = {1.0f, 2.0f, 3.0f, 4.0f};
    belief_t* belief = belief_create("ownership_test", original_vector, 4, 0.9f, 100);
    ASSERT_NE(belief, nullptr);

    /* Modify original - belief should be unaffected */
    original_vector[0] = 999.0f;

    /* Verify belief has its own copy */
    const float* belief_vector = belief->belief_vector;
    EXPECT_NE(belief_vector, nullptr);
    EXPECT_FLOAT_EQ(belief_vector[0], 1.0f);  /* Original value */

    belief_destroy(belief);
    SUCCEED();
}
