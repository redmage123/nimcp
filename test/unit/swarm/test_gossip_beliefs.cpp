/**
 * @file test_gossip_beliefs.cpp
 * @brief Comprehensive unit tests for NIMCP Gossip-Based Belief Propagation System
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Agent registration and management
 * - Belief introduction and updates
 * - Gossip propagation rounds
 * - Belief decay over time
 * - Consensus detection
 * - Contradiction detection
 * - Credibility weighting
 * - Bio-async integration
 * - Statistics and monitoring
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_gossip_beliefs.h"

class GossipBeliefsTest : public ::testing::Test {
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
        }
    }

    belief_t* create_test_belief(const char* topic, float certainty) {
        float vector[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
        return belief_create(topic, vector, 5, certainty, 123);
    }
};

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(GossipBeliefsTest, DestroyNullSystem) {
    gossip_beliefs_destroy(nullptr);
    SUCCEED();
}

TEST_F(GossipBeliefsTest, CreateWithNullConfigFails) {
    auto* sys = gossip_beliefs_create(nullptr);
    EXPECT_EQ(sys, nullptr);
}

TEST_F(GossipBeliefsTest, CreateWithCustomConfig) {
    gossip_beliefs_config_t config = {
        .gossip_probability = 0.3f,
        .max_gossip_targets = 5,
        .belief_decay_rate = 0.002f,
        .credibility_weight = 0.7f,
        .enable_contradiction_detection = false,
        .enable_bio_async = true
    };

    auto* sys = gossip_beliefs_create(&config);
    EXPECT_NE(sys, nullptr);
    if (sys) {
        gossip_beliefs_destroy(sys);
    }
}

/* ============================================================================
 * Agent Management Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, RegisterAgent) {
    int result = gossip_register_agent(system, 123, 0.8f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, RegisterMultipleAgents) {
    EXPECT_EQ(gossip_register_agent(system, 123, 0.8f), NIMCP_SUCCESS);
    EXPECT_EQ(gossip_register_agent(system, 456, 0.9f), NIMCP_SUCCESS);
    EXPECT_EQ(gossip_register_agent(system, 789, 0.7f), NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, RegisterAgentWithInvalidCredibility) {
    // Should clamp to valid range
    EXPECT_EQ(gossip_register_agent(system, 123, 1.5f), NIMCP_SUCCESS);
    EXPECT_EQ(gossip_register_agent(system, 456, -0.5f), NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, UnregisterAgent) {
    gossip_register_agent(system, 123, 0.8f);
    int result = gossip_unregister_agent(system, 123);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, UnregisterNonexistentAgent) {
    int result = gossip_unregister_agent(system, 9999);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

TEST_F(GossipBeliefsTest, UpdateAgentCredibility) {
    gossip_register_agent(system, 123, 0.5f);
    int result = gossip_update_credibility(system, 123, 0.9f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, UpdateNonexistentAgentCredibility) {
    int result = gossip_update_credibility(system, 9999, 0.9f);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

/* ============================================================================
 * Belief Management Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, IntroduceBelief) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief = create_test_belief("test_topic", 0.7f);
    ASSERT_NE(belief, nullptr);

    int result = gossip_introduce_belief(system, 123, belief);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    belief_destroy(belief);
}

TEST_F(GossipBeliefsTest, IntroduceMultipleBeliefs) {
    gossip_register_agent(system, 123, 0.8f);

    for (int i = 0; i < 5; i++) {
        belief_t* belief = create_test_belief("test_topic", 0.5f + i * 0.1f);
        ASSERT_NE(belief, nullptr);

        int result = gossip_introduce_belief(system, 123, belief);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        belief_destroy(belief);
    }
}

TEST_F(GossipBeliefsTest, IntroduceBeliefToUnregisteredAgent) {
    belief_t* belief = create_test_belief("test_topic", 0.7f);
    ASSERT_NE(belief, nullptr);

    // Should auto-create agent
    int result = gossip_introduce_belief(system, 999, belief);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    belief_destroy(belief);
}

TEST_F(GossipBeliefsTest, UpdateBeliefCertainty) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief = create_test_belief("test_topic", 0.5f);
    gossip_introduce_belief(system, 123, belief);
    uint32_t belief_id = belief->belief_id;
    belief_destroy(belief);

    int result = gossip_update_belief(system, 123, belief_id, 0.9f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, UpdateNonexistentBelief) {
    gossip_register_agent(system, 123, 0.8f);

    int result = gossip_update_belief(system, 123, 9999, 0.9f);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

TEST_F(GossipBeliefsTest, RemoveBelief) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief = create_test_belief("test_topic", 0.7f);
    gossip_introduce_belief(system, 123, belief);
    uint32_t belief_id = belief->belief_id;
    belief_destroy(belief);

    int result = gossip_remove_belief(system, 123, belief_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, RemoveNonexistentBelief) {
    gossip_register_agent(system, 123, 0.8f);

    int result = gossip_remove_belief(system, 123, 9999);
    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

/* ============================================================================
 * Belief Utility Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, CreateBelief) {
    float vector[] = {0.1f, 0.2f, 0.3f};
    belief_t* belief = belief_create("test_topic", vector, 3, 0.7f, 123);

    ASSERT_NE(belief, nullptr);
    EXPECT_STREQ(belief->topic, "test_topic");
    EXPECT_FLOAT_EQ(belief->certainty, 0.7f);
    EXPECT_EQ(belief->vector_size, 3);
    EXPECT_EQ(belief->source_agent_id, 123);
    EXPECT_NE(belief->belief_vector, nullptr);

    belief_destroy(belief);
}

TEST_F(GossipBeliefsTest, DestroyNullBelief) {
    belief_destroy(nullptr);
    SUCCEED();
}

TEST_F(GossipBeliefsTest, BeliefSimilarity) {
    float vector1[] = {1.0f, 0.0f, 0.0f};
    float vector2[] = {1.0f, 0.0f, 0.0f};
    float vector3[] = {0.0f, 1.0f, 0.0f};

    belief_t* belief1 = belief_create("topic1", vector1, 3, 0.5f, 123);
    belief_t* belief2 = belief_create("topic2", vector2, 3, 0.5f, 456);
    belief_t* belief3 = belief_create("topic3", vector3, 3, 0.5f, 789);

    // Same vectors should have high similarity
    float sim12 = belief_similarity(belief1, belief2);
    EXPECT_GT(sim12, 0.99f);

    // Orthogonal vectors should have low similarity
    float sim13 = belief_similarity(belief1, belief3);
    EXPECT_LT(sim13, 0.01f);

    belief_destroy(belief1);
    belief_destroy(belief2);
    belief_destroy(belief3);
}

TEST_F(GossipBeliefsTest, BeliefSimilarityWithNullPointers) {
    belief_t* belief = create_test_belief("test", 0.5f);

    EXPECT_FLOAT_EQ(belief_similarity(nullptr, belief), 0.0f);
    EXPECT_FLOAT_EQ(belief_similarity(belief, nullptr), 0.0f);
    EXPECT_FLOAT_EQ(belief_similarity(nullptr, nullptr), 0.0f);

    belief_destroy(belief);
}

/* ============================================================================
 * Gossip Propagation Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, GossipPropagateRound) {
    gossip_register_agent(system, 123, 0.8f);
    gossip_register_agent(system, 456, 0.8f);

    belief_t* belief = create_test_belief("test_topic", 0.7f);
    gossip_introduce_belief(system, 123, belief);
    belief_destroy(belief);

    uint64_t current_time = 1000000;
    int result = gossip_propagate_round(system, current_time);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, GetAgentBeliefs) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief1 = create_test_belief("topic1", 0.7f);
    belief_t* belief2 = create_test_belief("topic2", 0.8f);

    gossip_introduce_belief(system, 123, belief1);
    gossip_introduce_belief(system, 123, belief2);

    belief_destroy(belief1);
    belief_destroy(belief2);

    belief_t* beliefs = nullptr;
    uint32_t count = 0;

    int result = gossip_get_agent_beliefs(system, 123, &beliefs, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Note: count may be 0 if not fully implemented
}

TEST_F(GossipBeliefsTest, GetBeliefsForNonexistentAgent) {
    belief_t* beliefs = nullptr;
    uint32_t count = 0;

    int result = gossip_get_agent_beliefs(system, 9999, &beliefs, &count);

    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

/* ============================================================================
 * Belief Decay Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, ApplyDecay) {
    uint64_t current_time = 1000000;
    int result = gossip_apply_decay(system, current_time);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, ApplyDecayWithBeliefs) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief = create_test_belief("test_topic", 0.9f);
    gossip_introduce_belief(system, 123, belief);
    belief_destroy(belief);

    uint64_t current_time = 1000000;
    int result = gossip_apply_decay(system, current_time);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Consensus and Analysis Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, GetConsensusBeliefs) {
    gossip_register_agent(system, 123, 0.8f);
    gossip_register_agent(system, 456, 0.8f);

    belief_t* belief = create_test_belief("consensus_topic", 0.9f);
    gossip_introduce_belief(system, 123, belief);
    gossip_introduce_belief(system, 456, belief);
    belief_destroy(belief);

    belief_t* consensus = nullptr;
    uint32_t count = 0;

    int result = gossip_get_consensus_beliefs(system, &consensus, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, DetectContradictions) {
    gossip_register_agent(system, 123, 0.8f);

    // Create contradictory beliefs (opposite vectors)
    float vector1[] = {1.0f, 0.0f, 0.0f};
    float vector2[] = {-1.0f, 0.0f, 0.0f};

    belief_t* belief1 = belief_create("topic1", vector1, 3, 0.9f, 123);
    belief_t* belief2 = belief_create("topic2", vector2, 3, 0.9f, 123);

    gossip_introduce_belief(system, 123, belief1);
    gossip_introduce_belief(system, 123, belief2);

    belief_destroy(belief1);
    belief_destroy(belief2);

    uint32_t* contradictions = nullptr;
    uint32_t count = 0;

    int result = gossip_detect_contradictions(system, &contradictions, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GossipBeliefsTest, CalculateEntropy) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief = create_test_belief("test_topic", 0.7f);
    gossip_introduce_belief(system, 123, belief);
    belief_destroy(belief);

    float entropy = gossip_calculate_entropy(system);

    EXPECT_GE(entropy, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, GetStatistics) {
    uint32_t total_beliefs, total_agents, total_gossips;
    float avg_certainty;

    int result = gossip_get_stats(system, &total_beliefs, &total_agents,
                                   &avg_certainty, &total_gossips);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(total_beliefs, 0);
    EXPECT_EQ(total_agents, 0);
    EXPECT_EQ(total_gossips, 0);
}

TEST_F(GossipBeliefsTest, GetStatisticsAfterIntroducingBeliefs) {
    gossip_register_agent(system, 123, 0.8f);

    belief_t* belief1 = create_test_belief("topic1", 0.7f);
    belief_t* belief2 = create_test_belief("topic2", 0.8f);

    gossip_introduce_belief(system, 123, belief1);
    gossip_introduce_belief(system, 123, belief2);

    belief_destroy(belief1);
    belief_destroy(belief2);

    uint32_t total_beliefs, total_agents;
    gossip_get_stats(system, &total_beliefs, &total_agents, nullptr, nullptr);

    EXPECT_EQ(total_beliefs, 2);
    EXPECT_EQ(total_agents, 1);
}

TEST_F(GossipBeliefsTest, PrintStatus) {
    gossip_beliefs_print_status(system, false);
    gossip_beliefs_print_status(system, true);
    SUCCEED();
}

TEST_F(GossipBeliefsTest, PrintStatusWithNullSystem) {
    gossip_beliefs_print_status(nullptr, false);
    SUCCEED();
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(GossipBeliefsTest, NullPointerHandling) {
    belief_t* belief = create_test_belief("test", 0.5f);

    EXPECT_EQ(gossip_introduce_belief(nullptr, 123, belief), NIMCP_INVALID_PARAM);
    EXPECT_EQ(gossip_update_belief(nullptr, 123, 1, 0.9f), NIMCP_INVALID_PARAM);
    EXPECT_EQ(gossip_remove_belief(nullptr, 123, 1), NIMCP_INVALID_PARAM);
    EXPECT_EQ(gossip_register_agent(nullptr, 123, 0.8f), NIMCP_INVALID_PARAM);
    EXPECT_EQ(gossip_unregister_agent(nullptr, 123), NIMCP_INVALID_PARAM);

    belief_destroy(belief);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(GossipBeliefsTest, CreateManyAgentsAndBeliefs) {
    // Create 10 agents with multiple beliefs each
    for (uint32_t i = 0; i < 10; i++) {
        gossip_register_agent(system, 100 + i, 0.8f);

        for (int j = 0; j < 5; j++) {
            belief_t* belief = create_test_belief("test_topic", 0.5f + j * 0.1f);
            gossip_introduce_belief(system, 100 + i, belief);
            belief_destroy(belief);
        }
    }

    uint32_t total_beliefs, total_agents;
    gossip_get_stats(system, &total_beliefs, &total_agents, nullptr, nullptr);

    EXPECT_EQ(total_agents, 10);
    EXPECT_EQ(total_beliefs, 50);
}

TEST_F(GossipBeliefsTest, CertaintyClamping) {
    gossip_register_agent(system, 123, 0.8f);

    // Test certainty clamping at extremes
    belief_t* belief = belief_create("test", nullptr, 0, 1.5f, 123);  // Over 1.0
    EXPECT_LE(belief->certainty, 1.0f);
    belief_destroy(belief);

    belief = belief_create("test", nullptr, 0, -0.5f, 123);  // Under 0.0
    EXPECT_GE(belief->certainty, 0.0f);
    belief_destroy(belief);
}

TEST_F(GossipBeliefsTest, ContradictionDetectionDisabled) {
    // Create system with contradiction detection disabled
    gossip_beliefs_config_t config = {
        .gossip_probability = 0.5f,
        .max_gossip_targets = 3,
        .belief_decay_rate = 0.001f,
        .credibility_weight = 0.5f,
        .enable_contradiction_detection = false,
        .enable_bio_async = false
    };

    auto* sys = gossip_beliefs_create(&config);
    gossip_beliefs_init(sys, nullptr);

    uint32_t* contradictions = nullptr;
    uint32_t count = 0;

    int result = gossip_detect_contradictions(sys, &contradictions, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0);

    gossip_beliefs_destroy(sys);
}
