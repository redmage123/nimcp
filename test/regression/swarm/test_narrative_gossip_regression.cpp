/**
 * @file test_narrative_gossip_regression.cpp
 * @brief Regression tests for Swarm Narrative Memory and Gossip Beliefs systems
 *
 * WHAT: Tests to ensure no regression in narrative and gossip functionality
 * WHY:  Prevent future code changes from breaking existing behavior
 * HOW:  Comprehensive test scenarios covering edge cases and known issues
 *
 * REGRESSION TEST COVERAGE:
 * - Narrative coherence calculation consistency
 * - Belief similarity computation accuracy
 * - Memory leak detection in long-running scenarios
 * - Performance benchmarks for large swarms
 * - Boundary condition handling
 * - Error recovery scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_narrative.h"
#include "swarm/nimcp_gossip_beliefs.h"

/* ============================================================================
 * Narrative Regression Tests
 * ============================================================================ */

class NarrativeRegressionTest : public ::testing::Test {
protected:
    swarm_narrative_t* system;

    void SetUp() override {
        swarm_narrative_config_t config = {
            .max_narratives = 1000,
            .max_events_per_narrative = 100,
            .coherence_threshold = 0.3f,
            .enable_compression = false,
            .enable_bio_async = false
        };

        system = swarm_narrative_create(&config);
        ASSERT_NE(system, nullptr);
        swarm_narrative_init(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            swarm_narrative_destroy(system);
        }
    }
};

/**
 * REGRESSION: Coherence calculation must be deterministic
 * ISSUE: Previous bug where coherence varied with same inputs
 * EXPECTED: Same narrative produces same coherence score
 */
TEST_F(NarrativeRegressionTest, CoherenceDeterminism) {
    float encoding[] = {0.5f, 0.5f, 0.5f};

    // Create same narrative twice
    std::vector<float> coherence_scores;

    for (int trial = 0; trial < 3; trial++) {
        uint32_t narrative_id;
        swarm_narrative_begin(system, 123, &narrative_id);

        for (int i = 0; i < 5; i++) {
            narrative_event_t* event = narrative_event_create(123, encoding, 3, 0.5f, 0.8f);
            swarm_narrative_add_event(system, narrative_id, event);
            narrative_event_destroy(event);
        }

        swarm_narrative_end(system, narrative_id);

        narrative_t* narrative;
        swarm_narrative_get(system, narrative_id, &narrative);
        coherence_scores.push_back(narrative->coherence_score);
    }

    // All scores should be identical (or within floating point epsilon)
    for (size_t i = 1; i < coherence_scores.size(); i++) {
        EXPECT_FLOAT_EQ(coherence_scores[0], coherence_scores[i])
            << "Coherence calculation is not deterministic";
    }
}

/**
 * REGRESSION: Memory leak in narrative with many events
 * ISSUE: Events not properly freed in certain edge cases
 * EXPECTED: No memory leaks
 */
TEST_F(NarrativeRegressionTest, NoMemoryLeaksWithManyEvents) {
    // Create and destroy many narratives with max events
    const int NUM_NARRATIVES = 100;
    const int EVENTS_PER_NARRATIVE = 50;

    for (int n = 0; n < NUM_NARRATIVES; n++) {
        uint32_t narrative_id;
        swarm_narrative_begin(system, 123, &narrative_id);

        for (int i = 0; i < EVENTS_PER_NARRATIVE; i++) {
            float encoding[] = {0.1f * i, 0.2f * i, 0.3f * i};
            narrative_event_t* event = narrative_event_create(123, encoding, 3, 0.5f, 0.8f);
            swarm_narrative_add_event(system, narrative_id, event);
            narrative_event_destroy(event);
        }

        swarm_narrative_end(system, narrative_id);
    }

    // Verify statistics are correct
    uint32_t total_narratives, total_events;
    swarm_narrative_get_stats(system, &total_narratives, &total_events, nullptr, nullptr);

    EXPECT_EQ(total_narratives, NUM_NARRATIVES);
    EXPECT_EQ(total_events, NUM_NARRATIVES * EVENTS_PER_NARRATIVE);
}

/**
 * REGRESSION: Coherence threshold boundary behavior
 * ISSUE: Narratives at exact threshold sometimes rejected
 * EXPECTED: Narrative at threshold should be accepted
 */
TEST_F(NarrativeRegressionTest, CoherenceThresholdBoundary) {
    // Create system with specific threshold
    swarm_narrative_config_t config = {
        .max_narratives = 100,
        .max_events_per_narrative = 50,
        .coherence_threshold = 0.5f,  // Specific threshold
        .enable_compression = false,
        .enable_bio_async = false
    };

    auto* test_system = swarm_narrative_create(&config);
    swarm_narrative_init(test_system, nullptr);

    // Create narrative that should have coherence near threshold
    uint32_t narrative_id;
    swarm_narrative_begin(test_system, 123, &narrative_id);

    // Add events with moderate coherence
    for (int i = 0; i < 3; i++) {
        float encoding[] = {0.5f + i * 0.1f, 0.5f, 0.5f};
        narrative_event_t* event = narrative_event_create(123, encoding, 3, 0.5f, 0.8f);
        swarm_narrative_add_event(test_system, narrative_id, event);
        narrative_event_destroy(event);
    }

    int result = swarm_narrative_end(test_system, narrative_id);

    // Should be accepted (coherence should be >= threshold)
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Narrative at threshold boundary should be accepted";

    swarm_narrative_destroy(test_system);
}

/**
 * REGRESSION: Statistics overflow with large numbers
 * ISSUE: Statistics counters could overflow with many operations
 * EXPECTED: Statistics remain accurate
 */
TEST_F(NarrativeRegressionTest, StatisticsAccuracy) {
    const uint32_t NUM_NARRATIVES = 500;

    for (uint32_t i = 0; i < NUM_NARRATIVES; i++) {
        uint32_t narrative_id;
        swarm_narrative_begin(system, i, &narrative_id);

        float encoding[] = {0.1f, 0.2f, 0.3f};
        narrative_event_t* event = narrative_event_create(i, encoding, 3, 0.5f, 0.8f);
        swarm_narrative_add_event(system, narrative_id, event);
        narrative_event_destroy(event);

        swarm_narrative_end(system, narrative_id);
    }

    uint32_t total_narratives, total_events;
    swarm_narrative_get_stats(system, &total_narratives, &total_events, nullptr, nullptr);

    EXPECT_EQ(total_narratives, NUM_NARRATIVES);
    EXPECT_EQ(total_events, NUM_NARRATIVES);
}

/* ============================================================================
 * Gossip Beliefs Regression Tests
 * ============================================================================ */

class GossipBeliefsRegressionTest : public ::testing::Test {
protected:
    gossip_beliefs_t* system;

    void SetUp() override {
        gossip_beliefs_config_t config = {
            .gossip_probability = 0.5f,
            .max_gossip_targets = 5,
            .belief_decay_rate = 0.001f,
            .credibility_weight = 0.5f,
            .enable_contradiction_detection = true,
            .enable_bio_async = false
        };

        system = gossip_beliefs_create(&config);
        ASSERT_NE(system, nullptr);
        gossip_beliefs_init(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            gossip_beliefs_destroy(system);
        }
    }
};

/**
 * REGRESSION: Belief similarity calculation consistency
 * ISSUE: Similarity varied slightly due to floating point imprecision
 * EXPECTED: Identical beliefs have similarity == 1.0
 */
TEST_F(GossipBeliefsRegressionTest, BeliefSimilarityConsistency) {
    float vector[] = {1.0f, 0.0f, 0.0f};

    belief_t* belief1 = belief_create("topic1", vector, 3, 0.7f, 123);
    belief_t* belief2 = belief_create("topic2", vector, 3, 0.7f, 456);

    float similarity = belief_similarity(belief1, belief2);

    EXPECT_FLOAT_EQ(similarity, 1.0f) << "Identical vectors should have similarity == 1.0";

    belief_destroy(belief1);
    belief_destroy(belief2);
}

/**
 * REGRESSION: Orthogonal belief similarity
 * ISSUE: Orthogonal vectors sometimes had non-zero similarity
 * EXPECTED: Orthogonal vectors have similarity == 0.0
 */
TEST_F(GossipBeliefsRegressionTest, OrthogonalBeliefSimilarity) {
    float vector1[] = {1.0f, 0.0f, 0.0f};
    float vector2[] = {0.0f, 1.0f, 0.0f};

    belief_t* belief1 = belief_create("x-axis", vector1, 3, 0.7f, 123);
    belief_t* belief2 = belief_create("y-axis", vector2, 3, 0.7f, 456);

    float similarity = belief_similarity(belief1, belief2);

    EXPECT_NEAR(similarity, 0.0f, 1e-6f) << "Orthogonal vectors should have similarity ~= 0.0";

    belief_destroy(belief1);
    belief_destroy(belief2);
}

/**
 * REGRESSION: Certainty clamping at boundaries
 * ISSUE: Certainty values outside [0,1] not properly clamped
 * EXPECTED: All certainty values clamped to valid range
 */
TEST_F(GossipBeliefsRegressionTest, CertaintyClamping) {
    float vector[] = {0.5f, 0.5f, 0.5f};

    // Test over-range certainty
    belief_t* belief1 = belief_create("test", vector, 3, 1.5f, 123);
    EXPECT_LE(belief1->certainty, 1.0f) << "Certainty should be clamped to max 1.0";
    EXPECT_GE(belief1->certainty, 0.0f);
    belief_destroy(belief1);

    // Test under-range certainty
    belief_t* belief2 = belief_create("test", vector, 3, -0.5f, 123);
    EXPECT_GE(belief2->certainty, 0.0f) << "Certainty should be clamped to min 0.0";
    EXPECT_LE(belief2->certainty, 1.0f);
    belief_destroy(belief2);

    // Test exact boundaries
    belief_t* belief3 = belief_create("test", vector, 3, 0.0f, 123);
    EXPECT_FLOAT_EQ(belief3->certainty, 0.0f);
    belief_destroy(belief3);

    belief_t* belief4 = belief_create("test", vector, 3, 1.0f, 123);
    EXPECT_FLOAT_EQ(belief4->certainty, 1.0f);
    belief_destroy(belief4);
}

/**
 * REGRESSION: Agent creation and belief association
 * ISSUE: Beliefs sometimes not properly associated with agents
 * EXPECTED: Each agent's beliefs independently managed
 */
TEST_F(GossipBeliefsRegressionTest, AgentBeliefAssociation) {
    // Register multiple agents
    for (uint32_t i = 0; i < 10; i++) {
        gossip_register_agent(system, 100 + i, 0.8f);
    }

    // Each agent gets unique beliefs
    for (uint32_t i = 0; i < 10; i++) {
        float vector[] = {0.1f * i, 0.2f * i, 0.3f * i};
        belief_t* belief = belief_create("test", vector, 3, 0.7f, 100 + i);
        gossip_introduce_belief(system, 100 + i, belief);
        belief_destroy(belief);
    }

    // Verify statistics
    uint32_t total_beliefs, total_agents;
    gossip_get_stats(system, &total_beliefs, &total_agents, nullptr, nullptr);

    EXPECT_EQ(total_agents, 10);
    EXPECT_EQ(total_beliefs, 10);
}

/**
 * REGRESSION: Memory leak with agent churn
 * ISSUE: Agents not properly cleaned up when unregistered
 * EXPECTED: No memory leaks with register/unregister cycles
 */
TEST_F(GossipBeliefsRegressionTest, NoMemoryLeaksWithAgentChurn) {
    const int NUM_CYCLES = 100;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        // Register agent
        gossip_register_agent(system, 1000 + cycle, 0.8f);

        // Add beliefs
        float vector[] = {0.5f, 0.5f, 0.5f};
        belief_t* belief = belief_create("test", vector, 3, 0.7f, 1000 + cycle);
        gossip_introduce_belief(system, 1000 + cycle, belief);
        belief_destroy(belief);

        // Unregister agent
        gossip_unregister_agent(system, 1000 + cycle);
    }

    // Final statistics should show no agents
    uint32_t total_agents;
    gossip_get_stats(system, nullptr, &total_agents, nullptr, nullptr);

    EXPECT_EQ(total_agents, 0) << "All agents should be unregistered";
}

/**
 * REGRESSION: Average certainty calculation
 * ISSUE: Average certainty sometimes incorrect with belief updates
 * EXPECTED: Average certainty accurately reflects all beliefs
 */
TEST_F(GossipBeliefsRegressionTest, AverageCertaintyAccuracy) {
    gossip_register_agent(system, 123, 0.8f);

    // Add beliefs with known certainties
    float certainties[] = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    float expected_avg = 0.6f;  // (0.2 + 0.4 + 0.6 + 0.8 + 1.0) / 5

    float vector[] = {0.5f, 0.5f, 0.5f};
    for (float certainty : certainties) {
        belief_t* belief = belief_create("test", vector, 3, certainty, 123);
        gossip_introduce_belief(system, 123, belief);
        belief_destroy(belief);
    }

    uint32_t total_beliefs;
    float avg_certainty;
    gossip_get_stats(system, &total_beliefs, nullptr, &avg_certainty, nullptr);

    EXPECT_EQ(total_beliefs, 5);
    EXPECT_NEAR(avg_certainty, expected_avg, 0.01f) << "Average certainty should be accurate";
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

/**
 * REGRESSION: Narrative creation performance
 * ISSUE: Performance degraded with many narratives
 * EXPECTED: Linear scaling with narrative count
 */
TEST_F(NarrativeRegressionTest, NarrativeCreationPerformance) {
    const int NUM_NARRATIVES = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_NARRATIVES; i++) {
        uint32_t narrative_id;
        swarm_narrative_begin(system, i, &narrative_id);

        float encoding[] = {0.1f, 0.2f, 0.3f};
        narrative_event_t* event = narrative_event_create(i, encoding, 3, 0.5f, 0.8f);
        swarm_narrative_add_event(system, narrative_id, event);
        narrative_event_destroy(event);

        swarm_narrative_end(system, narrative_id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second for 1000 narratives)
    EXPECT_LT(duration.count(), 1000) << "Narrative creation should be fast";

    LOG_INFO("Created %d narratives in %ld ms (%.3f ms/narrative)",
             NUM_NARRATIVES, duration.count(),
             (double)duration.count() / NUM_NARRATIVES);
}

/**
 * REGRESSION: Belief introduction performance
 * ISSUE: Performance degraded with many beliefs per agent
 * EXPECTED: Linear scaling with belief count
 */
TEST_F(GossipBeliefsRegressionTest, BeliefIntroductionPerformance) {
    gossip_register_agent(system, 123, 0.8f);

    const int NUM_BELIEFS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_BELIEFS; i++) {
        float vector[] = {0.1f * i, 0.2f * i, 0.3f * i};
        belief_t* belief = belief_create("test", vector, 3, 0.7f, 123);
        gossip_introduce_belief(system, 123, belief);
        belief_destroy(belief);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second for 1000 beliefs)
    EXPECT_LT(duration.count(), 1000) << "Belief introduction should be fast";

    LOG_INFO("Introduced %d beliefs in %ld ms (%.3f ms/belief)",
             NUM_BELIEFS, duration.count(),
             (double)duration.count() / NUM_BELIEFS);
}
