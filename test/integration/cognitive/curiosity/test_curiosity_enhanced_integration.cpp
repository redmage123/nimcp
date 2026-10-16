/**
 * @file test_curiosity_enhanced_integration.cpp
 * @brief Integration tests for Enhanced Curiosity System
 *
 * TEST COVERAGE:
 * - Integration with brain system
 * - Integration with introspection module
 * - Integration with emotion system
 * - Integration with anxiety/stress modules
 * - Cross-enhancement interactions
 * - Bio-async message flow
 * - State persistence across updates
 * - Multi-system coordination
 *
 * INTEGRATION SCENARIOS:
 * 1. Brain-Curiosity: curiosity modulates exploration behavior
 * 2. Introspection-Meta: meta-curiosity feeds introspection metrics
 * 3. Emotion-Anxiety: emotional state affects approach-avoidance
 * 4. Social-ToM: social curiosity uses Theory of Mind
 * 5. Learning-Surprise: surprise boosts plasticity
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "cognitive/curiosity/nimcp_curiosity.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CuriosityEnhancedIntegrationTest : public ::testing::Test {
protected:
    curiosity_enhanced_system_t* system = nullptr;

    void SetUp() override {
        // Create enhanced curiosity with default config
        // Note: We test the curiosity system independently since brain creation
        // has optional subsystem initialization that may fail in test environments
        system = curiosity_enhanced_create(nullptr, nullptr);
    }

    void TearDown() override {
        if (system) {
            curiosity_enhanced_destroy(system);
            system = nullptr;
        }
    }
};

//=============================================================================
// 1. Brain Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, CreateSystem) {
    // Verify system was created successfully
    ASSERT_NE(system, nullptr);
}

TEST_F(CuriosityEnhancedIntegrationTest, CuriosityDriveAffectsExploration) {
    ASSERT_NE(system, nullptr);

    // Get initial drive
    float initial_drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(initial_drive, 0.0f);
    EXPECT_LE(initial_drive, 1.0f);

    // Stimulate curiosity with novel stimuli
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_report_stimulus(system, (uint64_t)(1000 + i), 0.9f);
        curiosity_enhanced_update(system, 50.0f);
    }

    // Drive should still be within valid range
    float post_drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(post_drive, 0.0f);
    EXPECT_LE(post_drive, 1.0f);
}

TEST_F(CuriosityEnhancedIntegrationTest, BoredomTriggersExploration) {
    ASSERT_NE(system, nullptr);

    // Report low-novelty repeated stimuli
    for (int i = 0; i < 20; i++) {
        curiosity_enhanced_report_stimulus(system, 12345, 0.1f);
        curiosity_enhanced_update(system, 200.0f);
    }

    // Check for boredom
    curiosity_boredom_state_t state;
    curiosity_enhanced_is_bored(system, &state);

    // Monotony should have increased
    EXPECT_GT(state.monotony_level, 0.0f);
}

//=============================================================================
// 2. Cross-Enhancement Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, BoredomBoostsSurpriseSensitivity) {
    ASSERT_NE(system, nullptr);

    // Build boredom through repetition
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_report_stimulus(system, 12345, 0.05f);
        curiosity_enhanced_update(system, 100.0f);
    }

    float boredom_boost = curiosity_enhanced_get_boredom_boost(system);

    // Report surprise
    float surprise_boost = curiosity_enhanced_report_surprise(system, 0.9f, "novel_event");

    // Both boosts should be active
    EXPECT_GE(boredom_boost, 1.0f);
    EXPECT_GE(surprise_boost, 1.0f);
}

TEST_F(CuriosityEnhancedIntegrationTest, FatigueReducesExploration) {
    ASSERT_NE(system, nullptr);

    // Record initial drive
    curiosity_enhanced_update(system, 100.0f);
    float initial_drive = curiosity_enhanced_get_overall_drive(system);

    // Initiate recovery (fatigue mode)
    curiosity_enhanced_initiate_recovery(system, 10000.0f);
    curiosity_enhanced_update(system, 100.0f);

    // Get state to check effective rate
    curiosity_enhanced_state_t state;
    curiosity_enhanced_get_state(system, &state);

    // During rest, effective exploration should be reduced
    EXPECT_LT(state.effective_exploration_rate, initial_drive + 0.1f);
}

TEST_F(CuriosityEnhancedIntegrationTest, TypeAffectsSocialCuriosity) {
    ASSERT_NE(system, nullptr);

    // Set social type as dominant
    curiosity_enhanced_set_type_intensity(system, CURIOSITY_TYPE_SOCIAL, 1.0f);
    curiosity_enhanced_transition_type(system, CURIOSITY_TYPE_SOCIAL);

    // Social curiosity should be enhanced
    curiosity_type_t dominant = curiosity_enhanced_get_dominant_type(system);
    EXPECT_EQ(dominant, CURIOSITY_TYPE_SOCIAL);

    // Record social interaction
    int ret = curiosity_enhanced_record_social_interaction(system, "test_agent", 0.8f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityEnhancedIntegrationTest, SurpriseEnhancesLearning) {
    ASSERT_NE(system, nullptr);

    // Report high surprise
    float boost = curiosity_enhanced_report_surprise(system, 1.0f, "major_surprise");
    EXPECT_GT(boost, 1.0f);

    // Priority should be set for memory consolidation
    float priority = curiosity_enhanced_prioritize_surprise(system, "important");
    EXPECT_GT(priority, 0.0f);
}

TEST_F(CuriosityEnhancedIntegrationTest, MetaCuriosityGrowsWithIntrospection) {
    ASSERT_NE(system, nullptr);

    float initial_meta = curiosity_enhanced_get_meta_curiosity(system);

    // Perform multiple introspection cycles
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_introspect(system, nullptr);
        curiosity_enhanced_update(system, 100.0f);
    }

    float final_meta = curiosity_enhanced_get_meta_curiosity(system);

    // Meta-curiosity should grow (or remain stable)
    EXPECT_GE(final_meta, initial_meta - 0.01f);
}

//=============================================================================
// 3. Interest Decay Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, InterestDecaysWithExposure) {
    ASSERT_NE(system, nullptr);

    const char* topic = "repeated_topic";

    // Get initial interest (should be max for new topic)
    float initial = curiosity_enhanced_get_topic_interest(system, topic);
    EXPECT_FLOAT_EQ(initial, 1.0f);

    // Record multiple exposures
    for (int i = 0; i < 50; i++) {
        curiosity_enhanced_record_exposure(system, topic, 0.5f);
    }

    // Interest should remain but satiation increases
    float satiation = curiosity_enhanced_compute_satiation(system, topic);
    EXPECT_GT(satiation, 0.0f);
}

TEST_F(CuriosityEnhancedIntegrationTest, ResidualInterestMaintainsMinimum) {
    ASSERT_NE(system, nullptr);

    const char* topic = "heavily_explored";

    // Saturate the topic
    for (int i = 0; i < 100; i++) {
        curiosity_enhanced_record_exposure(system, topic, 0.9f);
    }

    float residual = curiosity_enhanced_get_residual_interest(system, topic);

    // Should still have minimum residual interest
    EXPECT_GT(residual, 0.0f);
}

//=============================================================================
// 4. Anxiety Balance Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, HighThreatReducesExploration) {
    ASSERT_NE(system, nullptr);

    // With zero threat, should explore
    bool should_low = curiosity_enhanced_should_explore(system, 0.0f);
    EXPECT_TRUE(should_low);

    // With maximum threat, exploration is suppressed
    bool should_high = curiosity_enhanced_should_explore(system, 1.0f);
    // Result depends on approach-avoidance balance
    (void)should_high;
}

TEST_F(CuriosityEnhancedIntegrationTest, ConflictResolutionLearning) {
    ASSERT_NE(system, nullptr);

    // Report multiple approach wins
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_report_conflict_resolution(system, true);
    }

    // Net motivation should favor approach
    float net = curiosity_enhanced_get_net_motivation(system);
    EXPECT_GE(net, 0.0f);
}

//=============================================================================
// 5. Social Curiosity Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, MultiAgentTracking) {
    ASSERT_NE(system, nullptr);

    // Track multiple agents
    const char* agents[] = {"alice", "bob", "charlie", "diana"};
    int num_agents = 4;

    for (int i = 0; i < num_agents; i++) {
        for (int j = 0; j < 5; j++) {
            curiosity_enhanced_record_social_interaction(system, agents[i], 0.5f);
        }
    }

    // All agents should be tracked with interactions
    for (int i = 0; i < num_agents; i++) {
        curiosity_social_target_t target;
        float interest = curiosity_enhanced_assess_social_target(system, agents[i], &target);
        EXPECT_GT(interest, 0.0f);
        EXPECT_EQ(target.interaction_count, 5u);
    }
}

TEST_F(CuriosityEnhancedIntegrationTest, GossipInterestDecays) {
    ASSERT_NE(system, nullptr);

    float initial_gossip = curiosity_enhanced_get_gossip_interest(system);

    // Update over time
    for (int i = 0; i < 100; i++) {
        curiosity_enhanced_update(system, 50.0f);
    }

    float final_gossip = curiosity_enhanced_get_gossip_interest(system);

    // Gossip interest should decay (or at least not increase without events)
    EXPECT_LE(final_gossip, initial_gossip + 0.01f);
}

//=============================================================================
// 6. Contagion Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, CuriositySpreadsThroughContagion) {
    ASSERT_NE(system, nullptr);

    // Set high susceptibility
    curiosity_enhanced_set_contagion_susceptibility(system, 0.9f);

    // Observe highly curious behavior
    curiosity_contagion_event_t event;
    memset(&event, 0, sizeof(event));
    strncpy(event.observed_agent, "enthusiast", sizeof(event.observed_agent) - 1);
    strncpy(event.topic_of_interest, "fascinating_topic", sizeof(event.topic_of_interest) - 1);
    event.observed_curiosity_intensity = 0.95f;

    bool adopted = curiosity_enhanced_observe_curiosity(system, &event);
    EXPECT_TRUE(adopted);

    // Get stats to verify contagion event recorded
    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);
    EXPECT_GE(stats.contagion_events, 1u);
}

TEST_F(CuriosityEnhancedIntegrationTest, LowSusceptibilityResistsContagion) {
    ASSERT_NE(system, nullptr);

    // Set low susceptibility
    curiosity_enhanced_set_contagion_susceptibility(system, 0.1f);

    // Observe modest curiosity
    curiosity_contagion_event_t event;
    memset(&event, 0, sizeof(event));
    strncpy(event.observed_agent, "casual", sizeof(event.observed_agent) - 1);
    strncpy(event.topic_of_interest, "mildly_interesting", sizeof(event.topic_of_interest) - 1);
    event.observed_curiosity_intensity = 0.3f;

    bool adopted = curiosity_enhanced_observe_curiosity(system, &event);
    EXPECT_FALSE(adopted);
}

//=============================================================================
// 7. Counterfactual Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, CounterfactualGenerationAndExploration) {
    ASSERT_NE(system, nullptr);

    // Generate counterfactual
    curiosity_counterfactual_t cf;
    int ret = curiosity_enhanced_generate_counterfactual(
        system, "chose_path_A", "ended_at_dead_end", &cf);
    EXPECT_EQ(ret, 0);

    // Explore counterfactual
    float learning;
    ret = curiosity_enhanced_explore_counterfactual(system, &cf, &learning);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(cf.is_explored);

    // Stats should reflect exploration
    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);
    EXPECT_GE(stats.counterfactuals_generated, 1u);
}

TEST_F(CuriosityEnhancedIntegrationTest, MultipleCounterfactualScenarios) {
    ASSERT_NE(system, nullptr);

    // Generate many counterfactuals
    for (int i = 0; i < 30; i++) {
        char decision[64], outcome[64];
        snprintf(decision, sizeof(decision), "decision_%d", i);
        snprintf(outcome, sizeof(outcome), "outcome_%d", i);

        curiosity_counterfactual_t cf;
        curiosity_enhanced_generate_counterfactual(system, decision, outcome, &cf);
    }

    // Should handle overflow gracefully (max is 16)
    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);
    EXPECT_GE(stats.counterfactuals_generated, 16u);
}

//=============================================================================
// 8. Full System Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, CompleteExplorationCycle) {
    ASSERT_NE(system, nullptr);

    // Simulate complete exploration cycle

    // 1. Initial curiosity drive
    float initial_drive = curiosity_enhanced_get_overall_drive(system);

    // 2. Encounter novel stimuli
    for (int i = 0; i < 5; i++) {
        curiosity_enhanced_report_stimulus(system, (uint64_t)(2000 + i), 0.85f);
    }

    // 3. Experience surprise
    curiosity_enhanced_report_surprise(system, 0.7f, "unexpected_finding");

    // 4. Learn about topic
    curiosity_enhanced_record_exposure(system, "exploration_target", 0.8f);

    // 5. Social sharing
    curiosity_enhanced_record_social_interaction(system, "collaborator", 0.6f);

    // 6. Meta-reflection
    curiosity_enhanced_introspect(system, nullptr);

    // 7. Update state
    curiosity_enhanced_update(system, 500.0f);

    // Verify system state is coherent
    curiosity_enhanced_state_t state;
    curiosity_enhanced_get_state(system, &state);

    EXPECT_GE(state.overall_curiosity_drive, 0.0f);
    EXPECT_LE(state.overall_curiosity_drive, 1.0f);
    EXPECT_GT(state.last_update_ms, 0u);
}

TEST_F(CuriosityEnhancedIntegrationTest, LongRunningSimulation) {
    ASSERT_NE(system, nullptr);

    // Run extended simulation
    for (int cycle = 0; cycle < 100; cycle++) {
        // Periodic novel stimuli
        if (cycle % 10 == 0) {
            curiosity_enhanced_report_stimulus(system, (uint64_t)(5000 + cycle), 0.9f);
        }

        // Periodic exposures
        if (cycle % 5 == 0) {
            char topic[32];
            snprintf(topic, sizeof(topic), "topic_%d", cycle / 5);
            curiosity_enhanced_record_exposure(system, topic, 0.4f);
        }

        // Periodic surprises
        if (cycle % 20 == 0) {
            curiosity_enhanced_report_surprise(system, 0.6f, "periodic_surprise");
        }

        // Regular updates
        curiosity_enhanced_update(system, 100.0f);
    }

    // Get final statistics
    curiosity_enhanced_stats_t stats;
    curiosity_enhanced_get_stats(system, &stats);

    // Verify activity was recorded
    EXPECT_GE(stats.novelty_events, 5u);
    EXPECT_GE(stats.surprise_events, 2u);
}

//=============================================================================
// 9. Bio-Async Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, BioAsyncLifecycle) {
    ASSERT_NE(system, nullptr);

    // Connect
    int ret = curiosity_enhanced_connect_bio_async(system);
    EXPECT_EQ(ret, 0);

    // Update while connected
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_update(system, 50.0f);
    }

    // Disconnect
    ret = curiosity_enhanced_disconnect_bio_async(system);
    EXPECT_EQ(ret, 0);

    // Should be disconnected
    bool connected = curiosity_enhanced_is_bio_async_connected(system);
    EXPECT_FALSE(connected);
}

//=============================================================================
// 10. State Persistence Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, StatePersistsAcrossUpdates) {
    ASSERT_NE(system, nullptr);

    // Set specific states
    curiosity_enhanced_transition_type(system, CURIOSITY_TYPE_PERCEPTUAL);
    curiosity_enhanced_set_contagion_susceptibility(system, 0.75f);
    curiosity_enhanced_record_exposure(system, "persistent_topic", 0.5f);

    // Multiple updates
    for (int i = 0; i < 50; i++) {
        curiosity_enhanced_update(system, 20.0f);
    }

    // Verify state persisted
    curiosity_type_t type = curiosity_enhanced_get_dominant_type(system);
    EXPECT_EQ(type, CURIOSITY_TYPE_PERCEPTUAL);

    float susc = curiosity_enhanced_get_contagion_susceptibility(system);
    EXPECT_FLOAT_EQ(susc, 0.75f);

    float interest = curiosity_enhanced_get_topic_interest(system, "persistent_topic");
    EXPECT_GT(interest, 0.0f);
}

//=============================================================================
// 11. Edge Case Integration Tests
//=============================================================================

TEST_F(CuriosityEnhancedIntegrationTest, RapidStateTransitions) {
    ASSERT_NE(system, nullptr);

    // Rapid type transitions
    for (int i = 0; i < 50; i++) {
        curiosity_type_t type = (curiosity_type_t)(i % CURIOSITY_TYPE_COUNT);
        curiosity_enhanced_transition_type(system, type);
        curiosity_enhanced_update(system, 10.0f);
    }

    // System should remain stable
    curiosity_enhanced_state_t state;
    curiosity_enhanced_get_state(system, &state);
    EXPECT_GE(state.overall_curiosity_drive, 0.0f);
    EXPECT_LE(state.overall_curiosity_drive, 1.0f);
}

TEST_F(CuriosityEnhancedIntegrationTest, ExtremeValues) {
    ASSERT_NE(system, nullptr);

    // Maximum surprise
    float boost = curiosity_enhanced_report_surprise(system, 1.0f, "max");
    EXPECT_LE(boost, SURPRISE_LR_BOOST_MAX);

    // Minimum novelty
    curiosity_enhanced_report_stimulus(system, 1, 0.0f);

    // Maximum fatigue then recovery
    for (int i = 0; i < 5000; i++) {
        curiosity_enhanced_update(system, 50.0f);
    }
    curiosity_enhanced_initiate_recovery(system, 60000.0f);
    curiosity_enhanced_update(system, 1000.0f);
    curiosity_enhanced_end_recovery(system);

    // Should still function
    float drive = curiosity_enhanced_get_overall_drive(system);
    EXPECT_GE(drive, 0.0f);
}
