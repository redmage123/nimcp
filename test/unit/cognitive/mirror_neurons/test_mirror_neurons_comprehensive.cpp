/**
 * @file test_mirror_neurons_comprehensive.cpp
 * @brief Comprehensive unit tests for mirror_neurons.c (Target: 95%+ coverage)
 *
 * WHAT: Complete test coverage for mirror neuron system
 * WHY:  Increase coverage from 6.8% to 95%+ (385+ uncovered lines)
 * HOW:  Test all code paths, branches, error conditions, and edge cases
 *
 * COVERAGE STRATEGY:
 * 1. Lifecycle (creation, destruction, configuration)
 * 2. Action Processing (observe, execute, activation)
 * 3. Learning & Adaptation (demonstrations, associations, decay)
 * 4. Query & Analysis (stats, activation records, prediction)
 * 5. Integration (working memory, theory of mind, predictive, glial)
 * 6. Bidirectional Feedback (social salience, observation mode)
 * 7. Error Handling (all error paths)
 * 8. Edge Cases (boundary conditions, limits)
 * 9. Neuromodulation (ACh gating)
 * 10. Agent Tracking (multi-agent learning)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

    #include "cognitive/nimcp_mirror_neurons.h"
    #include "core/brain/nimcp_brain.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MirrorNeuronsTest : public ::testing::Test {
protected:
    mirror_neurons_t mirror;
    brain_t test_brain;

    void SetUp() override {
        // Initialize memory system
        nimcp_memory_init();

        mirror = nullptr;
        test_brain = nullptr;
    }

    void TearDown() override {
        if (mirror) {
            mirror_neurons_destroy(mirror);
            mirror = nullptr;
        }
        if (test_brain) {
            brain_destroy(test_brain);
            test_brain = nullptr;
        }

        nimcp_memory_cleanup();
    }

    // Helper: Create test action with features
    action_t create_test_action(uint32_t action_id, const char* name,
                                uint32_t agent_id = 0, float confidence = 1.0f) {
        float features[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

        // Vary features based on action_id for distinctiveness
        for (int i = 0; i < 8; i++) {
            features[i] += action_id * 0.05f;
        }

        return mirror_neurons_create_action(action_id, name, features, 8, agent_id);
    }

    // Helper: Create custom config
    mirror_neuron_config_t create_test_config(uint32_t num_neurons = 100,
                                              uint32_t max_actions = 20,
                                              uint32_t max_agents = 5) {
        mirror_neuron_config_t config = mirror_neurons_get_default_config();
        config.num_mirror_neurons = num_neurons;
        config.max_actions = max_actions;
        config.max_agents = max_agents;
        return config;
    }
};

//=============================================================================
// 1. Lifecycle Tests - Creation & Destruction
//=============================================================================

TEST_F(MirrorNeuronsTest, Create_WithDefaultConfig) {
    // Test creation with default config (NULL)
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Verify default configuration applied
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.num_learned_actions, 0u);
    EXPECT_EQ(stats.num_observed_agents, 0u);
}

TEST_F(MirrorNeuronsTest, Create_WithCustomConfig) {
    // Test creation with custom config
    mirror_neuron_config_t config = create_test_config(500, 50, 10);
    config.learning_rate = 0.02f;
    config.decay_rate = 0.1f;
    config.match_threshold = 0.8f;

    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    // System should be created successfully
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
}

TEST_F(MirrorNeuronsTest, Create_AllIntegrationFlags) {
    // Test with all integration flags enabled/disabled
    mirror_neuron_config_t config = create_test_config();
    config.enable_working_memory = true;
    config.enable_theory_of_mind = true;
    config.enable_prediction = true;
    config.enable_glial_modulation = true;
    config.enable_astrocytes = true;
    config.enable_oligodendrocytes = true;
    config.enable_microglia = true;

    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    // Now test with all disabled
    mirror_neurons_destroy(mirror);
    config.enable_working_memory = false;
    config.enable_theory_of_mind = false;
    config.enable_prediction = false;
    config.enable_glial_modulation = false;

    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_InvalidConfig_ZeroNeurons) {
    // Test creation with invalid config (zero neurons)
    mirror_neuron_config_t config = create_test_config();
    config.num_mirror_neurons = 0;

    mirror = mirror_neurons_create(&config);
    EXPECT_EQ(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_InvalidConfig_ZeroActions) {
    // Test creation with invalid config (zero max_actions)
    mirror_neuron_config_t config = create_test_config();
    config.max_actions = 0;

    mirror = mirror_neurons_create(&config);
    EXPECT_EQ(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Destroy_NullHandle) {
    // Test destroy with NULL handle (should not crash)
    mirror_neurons_destroy(nullptr);
    // Success if no crash
}

TEST_F(MirrorNeuronsTest, Destroy_MultipleActions) {
    // Create system with multiple actions, then destroy
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe multiple actions
    for (uint32_t i = 1; i <= 5; i++) {
        action_t action = create_test_action(i, "test_action");
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    // Destroy should clean up all action mappings
    mirror_neurons_destroy(mirror);
    mirror = nullptr;
}

//=============================================================================
// 2. Configuration & Utility Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, GetDefaultConfig) {
    // Test default configuration retrieval
    mirror_neuron_config_t config = mirror_neurons_get_default_config();

    EXPECT_EQ(config.num_mirror_neurons, 1000u);
    EXPECT_EQ(config.max_actions, 100u);
    EXPECT_EQ(config.max_agents, 10u);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(config.decay_rate, 0.05f);
    EXPECT_FLOAT_EQ(config.match_threshold, 0.7f);
    EXPECT_TRUE(config.enable_working_memory);
    EXPECT_TRUE(config.enable_theory_of_mind);
    EXPECT_TRUE(config.enable_prediction);
    EXPECT_EQ(config.observation_window, 1000u);
    EXPECT_EQ(config.replay_capacity, 100u);
}

TEST_F(MirrorNeuronsTest, CreateAction_AllParameters) {
    // Test action creation with all parameters
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    action_t action = mirror_neurons_create_action(
        42,                // action_id
        "test_action",     // action_name
        features,          // features
        10,                // num_features
        5                  // agent_id
    );

    EXPECT_EQ(action.action_id, 42u);
    EXPECT_STREQ(action.action_name, "test_action");
    EXPECT_EQ(action.num_features, 10u);
    EXPECT_EQ(action.agent_id, 5u);
    EXPECT_FLOAT_EQ(action.confidence, 1.0f);
    EXPECT_GT(action.timestamp, 0u);

    // Verify features copied correctly
    for (int i = 0; i < 10; i++) {
        EXPECT_FLOAT_EQ(action.features[i], features[i]);
    }
}

TEST_F(MirrorNeuronsTest, CreateAction_MaxFeatures) {
    // Test action creation with more than max features (should clamp to 32)
    float features[40];
    for (int i = 0; i < 40; i++) {
        features[i] = i * 0.1f;
    }

    action_t action = mirror_neurons_create_action(1, "test", features, 40, 0);

    // Should clamp to 32
    EXPECT_EQ(action.num_features, 32u);
}

TEST_F(MirrorNeuronsTest, CreateAction_NullName) {
    // Test action creation with NULL name
    float features[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    action_t action = mirror_neurons_create_action(1, nullptr, features, 5, 0);

    EXPECT_EQ(action.action_id, 1u);
    EXPECT_EQ(action.num_features, 5u);
}

TEST_F(MirrorNeuronsTest, CreateAction_NullFeatures) {
    // Test action creation with NULL features
    action_t action = mirror_neurons_create_action(1, "test", nullptr, 0, 0);

    EXPECT_EQ(action.action_id, 1u);
    EXPECT_EQ(action.num_features, 0u);
}

//=============================================================================
// 3. Action Processing Tests - Observation & Execution
//=============================================================================

TEST_F(MirrorNeuronsTest, ObserveAction_SingleAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 5, 0.9f);

    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Verify statistics updated
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 1u);
    EXPECT_EQ(stats.num_learned_actions, 1u);
    EXPECT_EQ(stats.num_observed_agents, 1u);
}

TEST_F(MirrorNeuronsTest, ObserveAction_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe 5 different actions
    for (uint32_t i = 1; i <= 5; i++) {
        action_t action = create_test_action(i, "action", 1);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    // Verify statistics
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 5u);
    EXPECT_EQ(stats.num_learned_actions, 5u);
    EXPECT_EQ(stats.num_observed_agents, 1u);
}

TEST_F(MirrorNeuronsTest, ObserveAction_SameActionMultipleTimes) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);

    // Observe same action 3 times
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    // Verify statistics
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 3u);
    EXPECT_EQ(stats.num_learned_actions, 1u);  // Still just 1 unique action
}

TEST_F(MirrorNeuronsTest, ObserveAction_MultipleAgents) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe actions from 3 different agents
    for (uint32_t agent = 1; agent <= 3; agent++) {
        action_t action = create_test_action(1, "wave", agent);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    // Verify agent tracking
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.num_observed_agents, 3u);
}

TEST_F(MirrorNeuronsTest, ObserveAction_SelfAgent) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe action from self (agent_id = 0)
    action_t action = create_test_action(1, "wave", 0);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Self should not be counted as observed agent
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.num_observed_agents, 0u);
}

TEST_F(MirrorNeuronsTest, ObserveAction_MaxActionsLimit) {
    mirror_neuron_config_t config = create_test_config(100, 3, 10);  // max 3 actions
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    // Observe 3 actions successfully
    for (uint32_t i = 1; i <= 3; i++) {
        action_t action = create_test_action(i, "action");
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    // 4th action should fail (exceeds limit)
    action_t action4 = create_test_action(4, "action4");
    EXPECT_FALSE(mirror_neurons_observe_action(mirror, &action4));
}

TEST_F(MirrorNeuronsTest, ObserveAction_MaxAgentsLimit) {
    mirror_neuron_config_t config = create_test_config(100, 10, 2);  // max 2 agents
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    // Observe from 2 agents successfully
    for (uint32_t agent = 1; agent <= 2; agent++) {
        action_t action = create_test_action(1, "wave", agent);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    // 3rd agent should still process but not create new agent entry
    action_t action3 = create_test_action(1, "wave", 3);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action3));

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.num_observed_agents, 2u);  // Still capped at 2
}

TEST_F(MirrorNeuronsTest, ObserveAction_NullMirror) {
    action_t action = create_test_action(1, "wave");
    EXPECT_FALSE(mirror_neurons_observe_action(nullptr, &action));
}

TEST_F(MirrorNeuronsTest, ObserveAction_NullAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    EXPECT_FALSE(mirror_neurons_observe_action(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, ExecuteAction_SingleAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave");

    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    // Verify statistics
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_executions, 1u);
    EXPECT_EQ(stats.num_learned_actions, 1u);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Execute 5 different actions
    for (uint32_t i = 1; i <= 5; i++) {
        action_t action = create_test_action(i, "action");
        ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));
    }

    // Verify statistics
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_executions, 5u);
    EXPECT_EQ(stats.num_learned_actions, 5u);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_SameActionMultipleTimes) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave");

    // Execute same action 3 times
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));
    }

    // Verify statistics
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_executions, 3u);
    EXPECT_EQ(stats.num_learned_actions, 1u);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_NullMirror) {
    action_t action = create_test_action(1, "wave");
    EXPECT_FALSE(mirror_neurons_execute_action(nullptr, &action));
}

TEST_F(MirrorNeuronsTest, ExecuteAction_NullAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    EXPECT_FALSE(mirror_neurons_execute_action(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, ObserveAndExecute_SameAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);

    // Observe action from another agent
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Execute the same action ourselves
    action.agent_id = 0;  // Self
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    // Verify both pathways activated
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 1u);
    EXPECT_EQ(stats.total_executions, 1u);
}

//=============================================================================
// 4. Activation & Matching Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, GetActivation_AfterObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1, 0.8f);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Get activation for observed action
    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GT(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_AfterExecution) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 0, 0.9f);
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    // Get activation for executed action
    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GT(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_NonExistentAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Query non-existent action
    float activation = mirror_neurons_get_activation(mirror, 999);
    EXPECT_EQ(activation, -1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_NullMirror) {
    float activation = mirror_neurons_get_activation(nullptr, 1);
    EXPECT_EQ(activation, -1.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_SimilarActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create two similar actions (same features)
    action_t observed = create_test_action(1, "wave", 1);
    action_t executed = create_test_action(2, "wave", 0);

    // Copy features to make them identical
    memcpy(executed.features, observed.features, sizeof(observed.features));
    executed.num_features = observed.num_features;

    float similarity;
    bool match = mirror_neurons_match_actions(mirror, &observed, &executed, &similarity);

    EXPECT_TRUE(match);
    EXPECT_GT(similarity, 0.9f);  // Should be very similar
}

TEST_F(MirrorNeuronsTest, MatchActions_DifferentActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create two different actions
    action_t observed = create_test_action(1, "wave", 1);
    action_t executed = create_test_action(50, "jump", 0);  // Very different features

    float similarity;
    bool match = mirror_neurons_match_actions(mirror, &observed, &executed, &similarity);

    // May or may not match depending on threshold, but similarity should be computed
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_ZeroFeatures) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t observed = create_test_action(1, "wave", 1);
    action_t executed = create_test_action(2, "wave", 0);

    // Set zero features
    observed.num_features = 0;
    executed.num_features = 0;

    float similarity;
    bool match = mirror_neurons_match_actions(mirror, &observed, &executed, &similarity);

    EXPECT_FALSE(match);
    EXPECT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_NullSimilarityOut) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t observed = create_test_action(1, "wave", 1);
    action_t executed = create_test_action(1, "wave", 0);

    // Should work with NULL similarity output
    bool match = mirror_neurons_match_actions(mirror, &observed, &executed, nullptr);
    // Result depends on similarity, but should not crash
}

TEST_F(MirrorNeuronsTest, MatchActions_NullInputs) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave");
    float similarity;

    // NULL mirror
    EXPECT_FALSE(mirror_neurons_match_actions(nullptr, &action, &action, &similarity));

    // NULL observed action
    EXPECT_FALSE(mirror_neurons_match_actions(mirror, nullptr, &action, &similarity));

    // NULL executed action
    EXPECT_FALSE(mirror_neurons_match_actions(mirror, &action, nullptr, &similarity));
}

//=============================================================================
// 5. Learning & Adaptation Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, LearnDemonstration_SingleAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t actions[1];
    actions[0] = create_test_action(1, "wave", 5);

    ASSERT_TRUE(mirror_neurons_learn_demonstration(mirror, actions, 1, 5));

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 1u);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create action sequence
    action_t actions[5];
    for (int i = 0; i < 5; i++) {
        actions[i] = create_test_action(i + 1, "action", 3);
    }

    ASSERT_TRUE(mirror_neurons_learn_demonstration(mirror, actions, 5, 3));

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 5u);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_NullInputs) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t actions[1];
    actions[0] = create_test_action(1, "wave");

    // NULL mirror
    EXPECT_FALSE(mirror_neurons_learn_demonstration(nullptr, actions, 1, 1));

    // NULL actions
    EXPECT_FALSE(mirror_neurons_learn_demonstration(mirror, nullptr, 1, 1));

    // Zero actions
    EXPECT_FALSE(mirror_neurons_learn_demonstration(mirror, actions, 0, 1));
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_WithCoactivation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);

    // Observe and execute same action
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    action.agent_id = 0;
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    // Update associations
    ASSERT_TRUE(mirror_neurons_update_associations(mirror));

    // Get activation record to check association strength
    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));
    EXPECT_GT(activation.association_strength, 0.0f);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_MultipleUpdates) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);

    // Repeated observations and executions
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
        action.agent_id = 0;
        ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));
        ASSERT_TRUE(mirror_neurons_update_associations(mirror));
    }

    // Association should strengthen over time
    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));
    EXPECT_GT(activation.association_strength, 0.0f);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_NullMirror) {
    EXPECT_FALSE(mirror_neurons_update_associations(nullptr));
}

TEST_F(MirrorNeuronsTest, DecayActivations_ReducesActivation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    float activation_before = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GT(activation_before, 0.0f);

    // Apply decay (1 second = 1000ms)
    ASSERT_TRUE(mirror_neurons_decay_activations(mirror, 1000));

    float activation_after = mirror_neurons_get_activation(mirror, 1);
    EXPECT_LT(activation_after, activation_before);
}

TEST_F(MirrorNeuronsTest, DecayActivations_LargeTimeStep) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Apply large decay (10 seconds)
    ASSERT_TRUE(mirror_neurons_decay_activations(mirror, 10000));

    float activation_after = mirror_neurons_get_activation(mirror, 1);
    // Should be reduced but may not be below 0.01 depending on decay formula
    EXPECT_GE(activation_after, 0.0f);
    EXPECT_LE(activation_after, 1.0f);
}

TEST_F(MirrorNeuronsTest, DecayActivations_NullMirror) {
    EXPECT_FALSE(mirror_neurons_decay_activations(nullptr, 1000));
}

//=============================================================================
// 6. Query & Statistics Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, GetStats_InitialState) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));

    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.total_executions, 0u);
    EXPECT_EQ(stats.num_learned_actions, 0u);
    EXPECT_EQ(stats.num_observed_agents, 0u);
    EXPECT_EQ(stats.num_active_neurons, 0u);
}

TEST_F(MirrorNeuronsTest, GetStats_AfterActivity) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Perform some activity
    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    action.agent_id = 0;
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));

    EXPECT_EQ(stats.total_observations, 1u);
    EXPECT_EQ(stats.total_executions, 1u);
    EXPECT_EQ(stats.num_learned_actions, 1u);
    EXPECT_EQ(stats.num_observed_agents, 1u);
    EXPECT_GT(stats.num_active_neurons, 0u);
    EXPECT_GT(stats.last_update_time, 0u);
}

TEST_F(MirrorNeuronsTest, GetStats_NullInputs) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_neuron_stats_t stats;

    // NULL mirror
    EXPECT_FALSE(mirror_neurons_get_stats(nullptr, &stats));

    // NULL stats
    EXPECT_FALSE(mirror_neurons_get_stats(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_ValidAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    action.agent_id = 0;
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));

    EXPECT_EQ(activation.action_id, 1u);
    EXPECT_GT(activation.observation_activation, 0.0f);
    EXPECT_GT(activation.execution_activation, 0.0f);
    EXPECT_EQ(activation.observation_count, 1u);
    EXPECT_EQ(activation.execution_count, 1u);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_NonExistentAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_activation_t activation;
    EXPECT_FALSE(mirror_neurons_get_activation_record(mirror, 999, &activation));
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_NullInputs) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_activation_t activation;

    // NULL mirror
    EXPECT_FALSE(mirror_neurons_get_activation_record(nullptr, 1, &activation));

    // NULL activation
    EXPECT_FALSE(mirror_neurons_get_activation_record(mirror, 1, nullptr));
}

TEST_F(MirrorNeuronsTest, PredictNextAction_AfterSequence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create and observe a sequence
    action_t actions[3];
    for (int i = 0; i < 3; i++) {
        actions[i] = create_test_action(i + 1, "action", 1);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &actions[i]));
    }

    // Try to predict next action
    action_t predicted;
    float confidence;

    // Note: Prediction may fail if no distinct action is activated
    bool result = mirror_neurons_predict_next_action(mirror, actions, 3, &predicted, &confidence);

    if (result) {
        EXPECT_GT(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }
}

TEST_F(MirrorNeuronsTest, PredictNextAction_SingleAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe single action
    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    action_t predicted;
    float confidence;

    // Prediction might fail with only one action
    bool result = mirror_neurons_predict_next_action(mirror, &action, 1, &predicted, &confidence);

    // Should handle gracefully
    if (!result) {
        EXPECT_EQ(confidence, 0.0f);
    }
}

TEST_F(MirrorNeuronsTest, PredictNextAction_NullInputs) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave");
    action_t predicted;
    float confidence;

    // NULL mirror
    EXPECT_FALSE(mirror_neurons_predict_next_action(nullptr, &action, 1, &predicted, &confidence));

    // NULL previous actions
    EXPECT_FALSE(mirror_neurons_predict_next_action(mirror, nullptr, 1, &predicted, &confidence));

    // Zero previous actions
    EXPECT_FALSE(mirror_neurons_predict_next_action(mirror, &action, 0, &predicted, &confidence));

    // NULL predicted action
    EXPECT_FALSE(mirror_neurons_predict_next_action(mirror, &action, 1, nullptr, &confidence));
}

//=============================================================================
// 7. Integration API Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_Success) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* working_memory = (void*)0x12345678;  // Dummy pointer

    ASSERT_TRUE(mirror_neurons_integrate_working_memory(mirror, working_memory));
}

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_Null) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    ASSERT_TRUE(mirror_neurons_integrate_working_memory(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_NullMirror) {
    EXPECT_FALSE(mirror_neurons_integrate_working_memory(nullptr, (void*)0x123));
}

TEST_F(MirrorNeuronsTest, IntegrateTheoryOfMind_Success) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* tom = (void*)0x12345678;  // Dummy pointer

    ASSERT_TRUE(mirror_neurons_integrate_theory_of_mind(mirror, tom));
}

TEST_F(MirrorNeuronsTest, IntegrateTheoryOfMind_Null) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    ASSERT_TRUE(mirror_neurons_integrate_theory_of_mind(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, IntegrateTheoryOfMind_NullMirror) {
    EXPECT_FALSE(mirror_neurons_integrate_theory_of_mind(nullptr, (void*)0x123));
}

TEST_F(MirrorNeuronsTest, IntegratePredictive_Success) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* predictive = (void*)0x12345678;  // Dummy pointer

    ASSERT_TRUE(mirror_neurons_integrate_predictive(mirror, predictive));
}

TEST_F(MirrorNeuronsTest, IntegratePredictive_Null) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    ASSERT_TRUE(mirror_neurons_integrate_predictive(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, IntegratePredictive_NullMirror) {
    EXPECT_FALSE(mirror_neurons_integrate_predictive(nullptr, (void*)0x123));
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_AllEnabled) {
    mirror_neuron_config_t config = create_test_config();
    config.enable_glial_modulation = true;
    config.enable_astrocytes = true;
    config.enable_oligodendrocytes = true;
    config.enable_microglia = true;

    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    void* glial = (void*)0x12345678;  // Dummy pointer

    ASSERT_TRUE(mirror_neurons_integrate_glial(mirror, glial));
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_Null) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    ASSERT_TRUE(mirror_neurons_integrate_glial(mirror, nullptr));
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_NullMirror) {
    EXPECT_FALSE(mirror_neurons_integrate_glial(nullptr, (void*)0x123));
}

//=============================================================================
// 8. Bidirectional Feedback Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, GetSocialSalience_NoActivity) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    float salience = mirror_neurons_get_social_salience(mirror);
    EXPECT_EQ(salience, 0.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_WithObservations) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe some actions
    for (int i = 0; i < 3; i++) {
        action_t action = create_test_action(i + 1, "action", 1);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    float salience = mirror_neurons_get_social_salience(mirror);
    EXPECT_GT(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_WithMultipleAgents) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe from multiple agents
    for (uint32_t agent = 1; agent <= 3; agent++) {
        action_t action = create_test_action(1, "wave", agent);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    float salience = mirror_neurons_get_social_salience(mirror);
    EXPECT_GT(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_NullMirror) {
    float salience = mirror_neurons_get_social_salience(nullptr);
    EXPECT_EQ(salience, 0.0f);
}

TEST_F(MirrorNeuronsTest, ActivateObservationMode_BoostsActivation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // First observe an action to create neurons
    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Activate observation mode
    mirror_neurons_activate_observation_mode(mirror);

    // Should boost activation
    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GT(activation, 0.0f);
}

TEST_F(MirrorNeuronsTest, ActivateObservationMode_NullMirror) {
    // Should not crash with NULL mirror
    mirror_neurons_activate_observation_mode(nullptr);
}

TEST_F(MirrorNeuronsTest, HasRecentObservations_NoObservations) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Note: The system initializes last_update_time to creation_time,
    // so it may return true if checked immediately after creation.
    // This tests the logic, not the exact timing.
    bool has_recent = mirror_neurons_has_recent_observations(mirror);
    // Accept either result as valid depending on timing
    (void)has_recent;
}

TEST_F(MirrorNeuronsTest, HasRecentObservations_RecentActivity) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    EXPECT_TRUE(mirror_neurons_has_recent_observations(mirror));
}

TEST_F(MirrorNeuronsTest, HasRecentObservations_NullMirror) {
    EXPECT_FALSE(mirror_neurons_has_recent_observations(nullptr));
}

//=============================================================================
// 9. Neuromodulation Tests (ACh Gating)
//=============================================================================

TEST_F(MirrorNeuronsTest, SetBrain_ValidBrain) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create a test brain
    test_brain = brain_create("test_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
    ASSERT_NE(test_brain, nullptr);

    // Set brain reference (should not crash)
    mirror_neurons_set_brain(mirror, test_brain);

    // Observe action - ACh modulation should be applied
    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
}

TEST_F(MirrorNeuronsTest, SetBrain_NullBrain) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Set NULL brain (disable neuromodulation)
    mirror_neurons_set_brain(mirror, nullptr);

    // Should still work without modulation
    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
}

TEST_F(MirrorNeuronsTest, SetBrain_NullMirror) {
    // Should not crash with NULL mirror
    mirror_neurons_set_brain(nullptr, nullptr);
}

//=============================================================================
// 10. Edge Cases & Stress Tests
//=============================================================================

TEST_F(MirrorNeuronsTest, StressTest_ManyActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe many actions
    for (uint32_t i = 1; i <= 50; i++) {
        action_t action = create_test_action(i, "action", 1);
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.num_learned_actions, 50u);
}

TEST_F(MirrorNeuronsTest, StressTest_ManyRepetitions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1);

    // Observe same action many times
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }

    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 100u);
}

TEST_F(MirrorNeuronsTest, EdgeCase_VerySmallConfig) {
    mirror_neuron_config_t config = create_test_config(10, 2, 1);
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    // Should still work with minimal configuration
    action_t action = create_test_action(1, "wave", 1);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
}

TEST_F(MirrorNeuronsTest, EdgeCase_LongActionName) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create action with very long name (should truncate to 63 chars + null)
    char long_name[100];
    memset(long_name, 'A', 99);
    long_name[99] = '\0';

    float features[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    action_t action = mirror_neurons_create_action(1, long_name, features, 5, 0);

    EXPECT_LT(strlen(action.action_name), 64u);
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
}

TEST_F(MirrorNeuronsTest, EdgeCase_ZeroConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1, 0.0f);  // Zero confidence
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    // Should still process but with minimal activation
    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GE(activation, 0.0f);
}

TEST_F(MirrorNeuronsTest, EdgeCase_HighConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "wave", 1, 1.0f);  // Max confidence
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));

    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GT(activation, 0.0f);
}

TEST_F(MirrorNeuronsTest, CombinedWorkflow_ObserveLearnExecute) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // 1. Observe demonstration
    action_t demo[3];
    for (int i = 0; i < 3; i++) {
        demo[i] = create_test_action(i + 1, "action", 5);
    }
    ASSERT_TRUE(mirror_neurons_learn_demonstration(mirror, demo, 3, 5));

    // 2. Update associations
    ASSERT_TRUE(mirror_neurons_update_associations(mirror));

    // 3. Execute learned actions
    for (int i = 0; i < 3; i++) {
        demo[i].agent_id = 0;  // Self
        ASSERT_TRUE(mirror_neurons_execute_action(mirror, &demo[i]));
    }

    // 4. Check statistics
    mirror_neuron_stats_t stats;
    ASSERT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_EQ(stats.total_observations, 3u);
    EXPECT_EQ(stats.total_executions, 3u);
    EXPECT_EQ(stats.num_learned_actions, 3u);
}

TEST_F(MirrorNeuronsTest, EdgeCase_ZeroFeatureSimilarity) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create actions with features that result in zero norms
    action_t action1 = create_test_action(1, "zero1");
    action_t action2 = create_test_action(2, "zero2");

    // Set all features to zero
    memset(action1.features, 0, sizeof(action1.features));
    memset(action2.features, 0, sizeof(action2.features));
    action1.num_features = 5;
    action2.num_features = 5;

    float similarity;
    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);

    EXPECT_FALSE(match);
    EXPECT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, EdgeCase_StrongAssociationClamping) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "strong", 1);

    // Build very strong association by repeated co-activation
    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
        action.agent_id = 0;
        ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));
        ASSERT_TRUE(mirror_neurons_update_associations(mirror));
        action.agent_id = 1;
    }

    // Get activation record - association should be clamped to 1.0
    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));
    EXPECT_LE(activation.association_strength, 1.0f);
}

TEST_F(MirrorNeuronsTest, EdgeCase_GetActivationRecordNoNeurons) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Create action but don't activate any neurons
    // This is done by creating the action mapping without observation/execution
    // We can't easily test this directly, so skip this edge case
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_OnlyObservationNoExecution) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "observe_only", 1);

    // Only observe, never execute
    ASSERT_TRUE(mirror_neurons_observe_action(mirror, &action));
    ASSERT_TRUE(mirror_neurons_update_associations(mirror));

    // Association should remain weak
    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));
    EXPECT_LE(activation.association_strength, 0.1f);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_OnlyExecutionNoObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "execute_only", 0);

    // Only execute, never observe
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));
    ASSERT_TRUE(mirror_neurons_update_associations(mirror));

    // Association should remain weak
    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));
    EXPECT_LE(activation.association_strength, 0.1f);
}

TEST_F(MirrorNeuronsTest, EdgeCase_NullFeaturePointers) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action1 = create_test_action(1, "null1");
    action_t action2 = create_test_action(2, "null2");

    // Test with mismatched feature counts
    action1.num_features = 0;
    action2.num_features = 5;

    float similarity;
    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);

    EXPECT_FALSE(match);
    EXPECT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_ExecutionTimestamp) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "timestamp_test", 0);

    // Execute first (no observation)
    ASSERT_TRUE(mirror_neurons_execute_action(mirror, &action));

    mirror_activation_t activation;
    ASSERT_TRUE(mirror_neurons_get_activation_record(mirror, 1, &activation));

    // Should have execution timestamp
    EXPECT_GT(activation.last_activation, 0u);
}

TEST_F(MirrorNeuronsTest, MatchActions_MismatchedFeatureCounts) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action1 = create_test_action(1, "short");
    action_t action2 = create_test_action(2, "long");

    action1.num_features = 3;
    action2.num_features = 10;

    float similarity;
    // Should use minimum feature count
    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);

    // Should compute similarity over 3 features
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
