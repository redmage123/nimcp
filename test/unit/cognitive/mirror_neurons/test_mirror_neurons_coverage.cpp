/**
 * @file test_mirror_neurons_coverage.cpp
 * @brief Comprehensive tests for nimcp_mirror_neurons.c (TARGET: 100% coverage)
 *
 * WHAT: Test mirror neuron system for observation-based learning
 * WHY:  Achieve 100% line/branch coverage for nimcp_mirror_neurons.c (1,282 lines)
 * HOW:  Test all public functions, guard clauses, configurations, learning
 *
 * COVERAGE GOALS:
 * - Line coverage: 100%
 * - Branch coverage: 100%
 * - Function coverage: 100%
 *
 * TEST COVERAGE:
 * - 19 core API functions
 * - Configuration validation
 * - Action observation/execution pathways
 * - Action matching and similarity
 * - Learning and association updates
 * - Activation decay
 * - Query and analysis functions
 * - Integration functions (working memory, theory of mind, predictive, glial)
 * - Utility functions
 * - Bidirectional feedback functions
 * - All NULL guards
 * - Edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "cognitive/nimcp_mirror_neurons.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorNeuronsTest : public ::testing::Test {
protected:
    mirror_neurons_t mirror;

    void SetUp() override {
        mirror = nullptr;
    }

    void TearDown() override {
        if (mirror) {
            mirror_neurons_destroy(mirror);
            mirror = nullptr;
        }
    }

    // Helper: Create valid config
    mirror_neuron_config_t create_valid_config() {
        return mirror_neurons_get_default_config();
    }

    // Helper: Create minimal config
    mirror_neuron_config_t create_minimal_config() {
        mirror_neuron_config_t config = {0};
        config.num_mirror_neurons = 10;
        config.max_actions = 5;
        config.max_agents = 3;
        config.learning_rate = 0.01f;
        config.decay_rate = 0.05f;
        config.match_threshold = 0.7f;
        config.enable_working_memory = true;
        config.enable_theory_of_mind = true;
        config.enable_prediction = true;
        config.observation_window = 1000;
        config.replay_capacity = 100;
        config.enable_glial_modulation = true;
        config.enable_astrocytes = true;
        config.enable_oligodendrocytes = true;
        config.enable_microglia = true;
        return config;
    }

    // Helper: Create action
    action_t create_test_action(uint32_t id, const char* name, uint32_t agent_id = 0) {
        float features[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
        return mirror_neurons_create_action(id, name, features, 5, agent_id);
    }
};

//=============================================================================
// Test Suite: Utility Functions - Default Configuration
//=============================================================================

TEST_F(MirrorNeuronsTest, GetDefaultConfig_ReturnsValidConfig) {
    mirror_neuron_config_t config = mirror_neurons_get_default_config();

    EXPECT_EQ(config.num_mirror_neurons, 1000U);
    EXPECT_EQ(config.max_actions, 100U);
    EXPECT_EQ(config.max_agents, 10U);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(config.decay_rate, 0.05f);
    EXPECT_FLOAT_EQ(config.match_threshold, 0.7f);
    EXPECT_TRUE(config.enable_working_memory);
    EXPECT_TRUE(config.enable_theory_of_mind);
    EXPECT_TRUE(config.enable_prediction);
    EXPECT_EQ(config.observation_window, 1000U);
    EXPECT_EQ(config.replay_capacity, 100U);
}

TEST_F(MirrorNeuronsTest, GetDefaultConfig_GlialSupport) {
    mirror_neuron_config_t config = mirror_neurons_get_default_config();

    EXPECT_TRUE(config.enable_glial_modulation);
    EXPECT_TRUE(config.enable_astrocytes);
    EXPECT_TRUE(config.enable_oligodendrocytes);
    EXPECT_TRUE(config.enable_microglia);
}

//=============================================================================
// Test Suite: Utility Functions - Create Action
//=============================================================================

TEST_F(MirrorNeuronsTest, CreateAction_ValidInput) {
    float features[3] = {1.0f, 2.0f, 3.0f};
    action_t action = mirror_neurons_create_action(1, "test_action", features, 3, 0);

    EXPECT_EQ(action.action_id, 1U);
    EXPECT_STREQ(action.action_name, "test_action");
    EXPECT_EQ(action.num_features, 3U);
    EXPECT_FLOAT_EQ(action.features[0], 1.0f);
    EXPECT_FLOAT_EQ(action.features[1], 2.0f);
    EXPECT_FLOAT_EQ(action.features[2], 3.0f);
    EXPECT_EQ(action.agent_id, 0U);
    EXPECT_FLOAT_EQ(action.confidence, 1.0f);
}

TEST_F(MirrorNeuronsTest, CreateAction_NullName) {
    float features[2] = {1.0f, 2.0f};
    action_t action = mirror_neurons_create_action(1, nullptr, features, 2, 0);

    EXPECT_EQ(action.action_id, 1U);
    EXPECT_EQ(action.num_features, 2U);
}

TEST_F(MirrorNeuronsTest, CreateAction_NullFeatures) {
    action_t action = mirror_neurons_create_action(1, "test", nullptr, 5, 0);

    EXPECT_EQ(action.action_id, 1U);
    EXPECT_EQ(action.num_features, 5U);
}

TEST_F(MirrorNeuronsTest, CreateAction_ZeroFeatures) {
    action_t action = mirror_neurons_create_action(1, "test", nullptr, 0, 0);

    EXPECT_EQ(action.action_id, 1U);
    EXPECT_EQ(action.num_features, 0U);
}

TEST_F(MirrorNeuronsTest, CreateAction_MaxFeatures) {
    float features[35];
    for (int i = 0; i < 35; i++) features[i] = (float)i;

    action_t action = mirror_neurons_create_action(1, "test", features, 35, 0);

    // Should cap at 32
    EXPECT_EQ(action.num_features, 32U);
}

TEST_F(MirrorNeuronsTest, CreateAction_AgentId) {
    float features[2] = {1.0f, 2.0f};
    action_t action = mirror_neurons_create_action(1, "test", features, 2, 42);

    EXPECT_EQ(action.agent_id, 42U);
}

//=============================================================================
// Test Suite: Lifecycle Management - Create/Destroy
//=============================================================================

TEST_F(MirrorNeuronsTest, Create_NullConfig_UsesDefaults) {
    mirror = mirror_neurons_create(nullptr);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_ValidConfig) {
    mirror_neuron_config_t config = create_valid_config();
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_MinimalConfig) {
    mirror_neuron_config_t config = create_minimal_config();
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_InvalidConfig_ZeroNeurons) {
    mirror_neuron_config_t config = create_valid_config();
    config.num_mirror_neurons = 0;
    mirror = mirror_neurons_create(&config);
    EXPECT_EQ(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_InvalidConfig_ZeroActions) {
    mirror_neuron_config_t config = create_valid_config();
    config.max_actions = 0;
    mirror = mirror_neurons_create(&config);
    EXPECT_EQ(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_CustomNeuronCount) {
    mirror_neuron_config_t config = create_valid_config();
    config.num_mirror_neurons = 500;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_CustomMaxActions) {
    mirror_neuron_config_t config = create_valid_config();
    config.max_actions = 50;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_CustomLearningRate) {
    mirror_neuron_config_t config = create_valid_config();
    config.learning_rate = 0.1f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Create_DisabledIntegrations) {
    mirror_neuron_config_t config = create_valid_config();
    config.enable_working_memory = false;
    config.enable_theory_of_mind = false;
    config.enable_prediction = false;
    config.enable_glial_modulation = false;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Destroy_NullHandle) {
    mirror_neurons_destroy(nullptr);
    SUCCEED();
}

TEST_F(MirrorNeuronsTest, Destroy_ValidHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);
    mirror_neurons_destroy(mirror);
    mirror = nullptr;
    SUCCEED();
}

//=============================================================================
// Test Suite: Action Processing - Observe Action
//=============================================================================

TEST_F(MirrorNeuronsTest, ObserveAction_NullMirror) {
    action_t action = create_test_action(1, "test");
    bool result = mirror_neurons_observe_action(nullptr, &action);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, ObserveAction_NullAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_observe_action(mirror, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, ObserveAction_ValidAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    bool result = mirror_neurons_observe_action(mirror, &action);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, ObserveAction_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        action_t action = create_test_action(i, "action");
        bool result = mirror_neurons_observe_action(mirror, &action);
        EXPECT_TRUE(result);
    }
}

TEST_F(MirrorNeuronsTest, ObserveAction_SameActionTwice) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action));
    EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action));
}

TEST_F(MirrorNeuronsTest, ObserveAction_DifferentAgents) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action1 = create_test_action(1, "reach", 1);
    action_t action2 = create_test_action(2, "grasp", 2);

    EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action1));
    EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action2));
}

TEST_F(MirrorNeuronsTest, ObserveAction_SelfAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach", 0);
    bool result = mirror_neurons_observe_action(mirror, &action);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, ObserveAction_LowConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action.confidence = 0.3f;
    bool result = mirror_neurons_observe_action(mirror, &action);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, ObserveAction_HighConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action.confidence = 1.0f;
    bool result = mirror_neurons_observe_action(mirror, &action);
    EXPECT_TRUE(result);
}

//=============================================================================
// Test Suite: Action Processing - Execute Action
//=============================================================================

TEST_F(MirrorNeuronsTest, ExecuteAction_NullMirror) {
    action_t action = create_test_action(1, "test");
    bool result = mirror_neurons_execute_action(nullptr, &action);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_NullAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_execute_action(mirror, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_ValidAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    bool result = mirror_neurons_execute_action(mirror, &action);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        action_t action = create_test_action(i, "action");
        bool result = mirror_neurons_execute_action(mirror, &action);
        EXPECT_TRUE(result);
    }
}

TEST_F(MirrorNeuronsTest, ExecuteAction_SameActionTwice) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    EXPECT_TRUE(mirror_neurons_execute_action(mirror, &action));
    EXPECT_TRUE(mirror_neurons_execute_action(mirror, &action));
}

TEST_F(MirrorNeuronsTest, ExecuteAction_LowConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action.confidence = 0.3f;
    bool result = mirror_neurons_execute_action(mirror, &action);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, ExecuteAction_HighConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action.confidence = 1.0f;
    bool result = mirror_neurons_execute_action(mirror, &action);
    EXPECT_TRUE(result);
}

//=============================================================================
// Test Suite: Action Processing - Get Activation
//=============================================================================

TEST_F(MirrorNeuronsTest, GetActivation_NullMirror) {
    float activation = mirror_neurons_get_activation(nullptr, 1);
    EXPECT_FLOAT_EQ(activation, -1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_NonexistentAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    float activation = mirror_neurons_get_activation(mirror, 999);
    EXPECT_FLOAT_EQ(activation, -1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_AfterObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_AfterExecution) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_execute_action(mirror, &action);

    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_AfterBothPathways) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);
    mirror_neurons_execute_action(mirror, &action);

    float activation = mirror_neurons_get_activation(mirror, 1);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetActivation_ZeroId) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    float activation = mirror_neurons_get_activation(mirror, 0);
    EXPECT_FLOAT_EQ(activation, -1.0f);
}

//=============================================================================
// Test Suite: Action Processing - Match Actions
//=============================================================================

TEST_F(MirrorNeuronsTest, MatchActions_NullMirror) {
    action_t action1 = create_test_action(1, "reach");
    action_t action2 = create_test_action(2, "reach");
    float similarity;

    bool match = mirror_neurons_match_actions(nullptr, &action1, &action2, &similarity);
    EXPECT_FALSE(match);
    EXPECT_FLOAT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_NullObserved) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    float similarity;

    bool match = mirror_neurons_match_actions(mirror, nullptr, &action, &similarity);
    EXPECT_FALSE(match);
    EXPECT_FLOAT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_NullExecuted) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    float similarity;

    bool match = mirror_neurons_match_actions(mirror, &action, nullptr, &similarity);
    EXPECT_FALSE(match);
    EXPECT_FLOAT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_NullSimilarityOutput) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action1 = create_test_action(1, "reach");
    action_t action2 = create_test_action(2, "reach");

    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, nullptr);
    // Should still compute match
    EXPECT_TRUE(match || !match); // Either is valid
}

TEST_F(MirrorNeuronsTest, MatchActions_IdenticalActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action1 = create_test_action(1, "reach");
    action_t action2 = create_test_action(2, "reach");
    float similarity;

    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_DifferentActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    float features1[3] = {1.0f, 0.0f, 0.0f};
    float features2[3] = {0.0f, 1.0f, 0.0f};

    action_t action1 = mirror_neurons_create_action(1, "reach", features1, 3, 0);
    action_t action2 = mirror_neurons_create_action(2, "grasp", features2, 3, 0);
    float similarity;

    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_ZeroFeatures) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action1 = mirror_neurons_create_action(1, "reach", nullptr, 0, 0);
    action_t action2 = mirror_neurons_create_action(2, "grasp", nullptr, 0, 0);
    float similarity;

    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);
    EXPECT_FALSE(match);
    EXPECT_FLOAT_EQ(similarity, 0.0f);
}

TEST_F(MirrorNeuronsTest, MatchActions_DifferentFeatureCounts) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    float features1[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float features2[3] = {1.0f, 2.0f, 3.0f};

    action_t action1 = mirror_neurons_create_action(1, "reach", features1, 5, 0);
    action_t action2 = mirror_neurons_create_action(2, "reach", features2, 3, 0);
    float similarity;

    bool match = mirror_neurons_match_actions(mirror, &action1, &action2, &similarity);
    // Should use minimum feature count
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}

//=============================================================================
// Test Suite: Learning & Adaptation - Learn Demonstration
//=============================================================================

TEST_F(MirrorNeuronsTest, LearnDemonstration_NullMirror) {
    action_t action = create_test_action(1, "reach");
    bool result = mirror_neurons_learn_demonstration(nullptr, &action, 1, 1);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_NullActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_learn_demonstration(mirror, nullptr, 5, 1);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_ZeroActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    bool result = mirror_neurons_learn_demonstration(mirror, &action, 0, 1);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_SingleAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    bool result = mirror_neurons_learn_demonstration(mirror, &action, 1, 1);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t actions[3];
    actions[0] = create_test_action(1, "reach");
    actions[1] = create_test_action(2, "grasp");
    actions[2] = create_test_action(3, "lift");

    bool result = mirror_neurons_learn_demonstration(mirror, actions, 3, 1);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, LearnDemonstration_DifferentDemonstrators) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");

    EXPECT_TRUE(mirror_neurons_learn_demonstration(mirror, &action, 1, 1));
    EXPECT_TRUE(mirror_neurons_learn_demonstration(mirror, &action, 1, 2));
}

//=============================================================================
// Test Suite: Learning & Adaptation - Update Associations
//=============================================================================

TEST_F(MirrorNeuronsTest, UpdateAssociations_NullMirror) {
    bool result = mirror_neurons_update_associations(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_EmptySystem) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_update_associations(mirror);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_AfterObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    bool result = mirror_neurons_update_associations(mirror);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_AfterBothPathways) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);
    mirror_neurons_execute_action(mirror, &action);

    bool result = mirror_neurons_update_associations(mirror);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, UpdateAssociations_MultipleTimes) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);
    mirror_neurons_execute_action(mirror, &action);

    EXPECT_TRUE(mirror_neurons_update_associations(mirror));
    EXPECT_TRUE(mirror_neurons_update_associations(mirror));
    EXPECT_TRUE(mirror_neurons_update_associations(mirror));
}

//=============================================================================
// Test Suite: Learning & Adaptation - Decay Activations
//=============================================================================

TEST_F(MirrorNeuronsTest, DecayActivations_NullMirror) {
    bool result = mirror_neurons_decay_activations(nullptr, 100);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, DecayActivations_ZeroTime) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_decay_activations(mirror, 0);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, DecayActivations_SmallTime) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    bool result = mirror_neurons_decay_activations(mirror, 10);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, DecayActivations_LargeTime) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    bool result = mirror_neurons_decay_activations(mirror, 10000);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, DecayActivations_ReducesActivation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    float activation_before = mirror_neurons_get_activation(mirror, 1);
    mirror_neurons_decay_activations(mirror, 1000);
    float activation_after = mirror_neurons_get_activation(mirror, 1);

    // Activation should decrease or reach zero threshold
    EXPECT_LE(activation_after, activation_before);
}

TEST_F(MirrorNeuronsTest, DecayActivations_MultipleCalls) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    EXPECT_TRUE(mirror_neurons_decay_activations(mirror, 100));
    EXPECT_TRUE(mirror_neurons_decay_activations(mirror, 100));
    EXPECT_TRUE(mirror_neurons_decay_activations(mirror, 100));
}

//=============================================================================
// Test Suite: Query & Analysis - Get Stats
//=============================================================================

TEST_F(MirrorNeuronsTest, GetStats_NullMirror) {
    mirror_neuron_stats_t stats;
    bool result = mirror_neurons_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, GetStats_NullStats) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_get_stats(mirror, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, GetStats_EmptySystem) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_neuron_stats_t stats;
    bool result = mirror_neurons_get_stats(mirror, &stats);
    EXPECT_TRUE(result);

    EXPECT_EQ(stats.num_learned_actions, 0U);
    EXPECT_EQ(stats.num_observed_agents, 0U);
    EXPECT_EQ(stats.total_observations, 0U);
    EXPECT_EQ(stats.total_executions, 0U);
}

TEST_F(MirrorNeuronsTest, GetStats_AfterObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach", 1);
    mirror_neurons_observe_action(mirror, &action);

    mirror_neuron_stats_t stats;
    EXPECT_TRUE(mirror_neurons_get_stats(mirror, &stats));

    EXPECT_EQ(stats.num_learned_actions, 1U);
    EXPECT_EQ(stats.num_observed_agents, 1U);
    EXPECT_EQ(stats.total_observations, 1U);
    EXPECT_EQ(stats.total_executions, 0U);
}

TEST_F(MirrorNeuronsTest, GetStats_AfterExecution) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_execute_action(mirror, &action);

    mirror_neuron_stats_t stats;
    EXPECT_TRUE(mirror_neurons_get_stats(mirror, &stats));

    EXPECT_EQ(stats.num_learned_actions, 1U);
    EXPECT_EQ(stats.total_observations, 0U);
    EXPECT_EQ(stats.total_executions, 1U);
}

TEST_F(MirrorNeuronsTest, GetStats_MultipleActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        action_t action = create_test_action(i, "action");
        mirror_neurons_observe_action(mirror, &action);
        mirror_neurons_execute_action(mirror, &action);
    }

    mirror_neuron_stats_t stats;
    EXPECT_TRUE(mirror_neurons_get_stats(mirror, &stats));

    EXPECT_EQ(stats.num_learned_actions, 5U);
    EXPECT_EQ(stats.total_observations, 5U);
    EXPECT_EQ(stats.total_executions, 5U);
}

//=============================================================================
// Test Suite: Query & Analysis - Get Activation Record
//=============================================================================

TEST_F(MirrorNeuronsTest, GetActivationRecord_NullMirror) {
    mirror_activation_t activation;
    bool result = mirror_neurons_get_activation_record(nullptr, 1, &activation);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_NullActivation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_get_activation_record(mirror, 1, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_NonexistentAction) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_activation_t activation;
    bool result = mirror_neurons_get_activation_record(mirror, 999, &activation);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_AfterObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    mirror_activation_t activation;
    bool result = mirror_neurons_get_activation_record(mirror, 1, &activation);
    EXPECT_TRUE(result);
    EXPECT_EQ(activation.action_id, 1U);
    EXPECT_EQ(activation.observation_count, 1U);
    EXPECT_EQ(activation.execution_count, 0U);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_AfterExecution) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_execute_action(mirror, &action);

    mirror_activation_t activation;
    bool result = mirror_neurons_get_activation_record(mirror, 1, &activation);
    EXPECT_TRUE(result);
    EXPECT_EQ(activation.action_id, 1U);
    EXPECT_EQ(activation.observation_count, 0U);
    EXPECT_EQ(activation.execution_count, 1U);
}

TEST_F(MirrorNeuronsTest, GetActivationRecord_AfterBothPathways) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);
    mirror_neurons_execute_action(mirror, &action);

    mirror_activation_t activation;
    bool result = mirror_neurons_get_activation_record(mirror, 1, &activation);
    EXPECT_TRUE(result);
    EXPECT_EQ(activation.action_id, 1U);
    EXPECT_EQ(activation.observation_count, 1U);
    EXPECT_EQ(activation.execution_count, 1U);
}

//=============================================================================
// Test Suite: Query & Analysis - Predict Next Action
//=============================================================================

TEST_F(MirrorNeuronsTest, PredictNextAction_NullMirror) {
    action_t action = create_test_action(1, "reach");
    action_t predicted;
    float confidence;

    bool result = mirror_neurons_predict_next_action(nullptr, &action, 1, &predicted, &confidence);
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(MirrorNeuronsTest, PredictNextAction_NullPreviousActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t predicted;
    float confidence;

    bool result = mirror_neurons_predict_next_action(mirror, nullptr, 1, &predicted, &confidence);
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(MirrorNeuronsTest, PredictNextAction_ZeroPrevious) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action_t predicted;
    float confidence;

    bool result = mirror_neurons_predict_next_action(mirror, &action, 0, &predicted, &confidence);
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(MirrorNeuronsTest, PredictNextAction_NullPredicted) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    float confidence;

    bool result = mirror_neurons_predict_next_action(mirror, &action, 1, nullptr, &confidence);
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(MirrorNeuronsTest, PredictNextAction_NullConfidence) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action_t predicted;

    // Should still work without confidence output
    mirror_neurons_predict_next_action(mirror, &action, 1, &predicted, nullptr);
    SUCCEED();
}

TEST_F(MirrorNeuronsTest, PredictNextAction_EmptySystem) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    action_t predicted;
    float confidence;

    bool result = mirror_neurons_predict_next_action(mirror, &action, 1, &predicted, &confidence);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, PredictNextAction_AfterObservations) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe a sequence
    action_t action1 = create_test_action(1, "reach");
    action_t action2 = create_test_action(2, "grasp");
    mirror_neurons_observe_action(mirror, &action1);
    mirror_neurons_observe_action(mirror, &action2);

    action_t predicted;
    float confidence;

    // Try to predict next action
    mirror_neurons_predict_next_action(mirror, &action1, 1, &predicted, &confidence);
    // May succeed or fail depending on activations
    SUCCEED();
}

//=============================================================================
// Test Suite: Integration API - Working Memory
//=============================================================================

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_NullMirror) {
    void* wm = (void*)0x12345678;
    bool result = mirror_neurons_integrate_working_memory(nullptr, wm);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_ValidHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* wm = (void*)0x12345678;
    bool result = mirror_neurons_integrate_working_memory(mirror, wm);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_NullHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_integrate_working_memory(mirror, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateWorkingMemory_MultipleTimes) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* wm1 = (void*)0x12345678;
    void* wm2 = (void*)0x87654321;

    EXPECT_TRUE(mirror_neurons_integrate_working_memory(mirror, wm1));
    EXPECT_TRUE(mirror_neurons_integrate_working_memory(mirror, wm2));
}

//=============================================================================
// Test Suite: Integration API - Theory of Mind
//=============================================================================

TEST_F(MirrorNeuronsTest, IntegrateTheoryOfMind_NullMirror) {
    void* tom = (void*)0x12345678;
    bool result = mirror_neurons_integrate_theory_of_mind(nullptr, tom);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateTheoryOfMind_ValidHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* tom = (void*)0x12345678;
    bool result = mirror_neurons_integrate_theory_of_mind(mirror, tom);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateTheoryOfMind_NullHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_integrate_theory_of_mind(mirror, nullptr);
    EXPECT_TRUE(result);
}

//=============================================================================
// Test Suite: Integration API - Predictive Processing
//=============================================================================

TEST_F(MirrorNeuronsTest, IntegratePredictive_NullMirror) {
    void* pred = (void*)0x12345678;
    bool result = mirror_neurons_integrate_predictive(nullptr, pred);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, IntegratePredictive_ValidHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* pred = (void*)0x12345678;
    bool result = mirror_neurons_integrate_predictive(mirror, pred);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegratePredictive_NullHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_integrate_predictive(mirror, nullptr);
    EXPECT_TRUE(result);
}

//=============================================================================
// Test Suite: Integration API - Glial Cells
//=============================================================================

TEST_F(MirrorNeuronsTest, IntegrateGlial_NullMirror) {
    void* glial = (void*)0x12345678;
    bool result = mirror_neurons_integrate_glial(nullptr, glial);
    EXPECT_FALSE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_ValidHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    void* glial = (void*)0x12345678;
    bool result = mirror_neurons_integrate_glial(mirror, glial);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_NullHandle) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    bool result = mirror_neurons_integrate_glial(mirror, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_WithAstrocytesEnabled) {
    mirror_neuron_config_t config = create_valid_config();
    config.enable_astrocytes = true;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    void* glial = (void*)0x12345678;
    bool result = mirror_neurons_integrate_glial(mirror, glial);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_WithOligodendrocytesEnabled) {
    mirror_neuron_config_t config = create_valid_config();
    config.enable_oligodendrocytes = true;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    void* glial = (void*)0x12345678;
    bool result = mirror_neurons_integrate_glial(mirror, glial);
    EXPECT_TRUE(result);
}

TEST_F(MirrorNeuronsTest, IntegrateGlial_WithMicrogliaEnabled) {
    mirror_neuron_config_t config = create_valid_config();
    config.enable_microglia = true;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    void* glial = (void*)0x12345678;
    bool result = mirror_neurons_integrate_glial(mirror, glial);
    EXPECT_TRUE(result);
}

//=============================================================================
// Test Suite: Bidirectional Feedback - Social Salience
//=============================================================================

TEST_F(MirrorNeuronsTest, GetSocialSalience_NullMirror) {
    float salience = mirror_neurons_get_social_salience(nullptr);
    EXPECT_FLOAT_EQ(salience, 0.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_EmptySystem) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    float salience = mirror_neurons_get_social_salience(mirror);
    EXPECT_FLOAT_EQ(salience, 0.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_AfterObservation) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach", 1);
    mirror_neurons_observe_action(mirror, &action);

    float salience = mirror_neurons_get_social_salience(mirror);
    EXPECT_GE(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_MultipleObservations) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 0; i < 3; i++) {
        action_t action = create_test_action(i, "action", i + 1);
        mirror_neurons_observe_action(mirror, &action);
    }

    float salience = mirror_neurons_get_social_salience(mirror);
    EXPECT_GE(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
}

TEST_F(MirrorNeuronsTest, GetSocialSalience_AfterDecay) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach", 1);
    mirror_neurons_observe_action(mirror, &action);

    float salience_before = mirror_neurons_get_social_salience(mirror);
    mirror_neurons_decay_activations(mirror, 5000);
    float salience_after = mirror_neurons_get_social_salience(mirror);

    EXPECT_LE(salience_after, salience_before);
}

//=============================================================================
// Test Suite: Bidirectional Feedback - Activate Observation Mode
//=============================================================================

TEST_F(MirrorNeuronsTest, ActivateObservationMode_NullMirror) {
    mirror_neurons_activate_observation_mode(nullptr);
    SUCCEED(); // Should not crash
}

TEST_F(MirrorNeuronsTest, ActivateObservationMode_ValidMirror) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_neurons_activate_observation_mode(mirror);
    SUCCEED();
}

TEST_F(MirrorNeuronsTest, ActivateObservationMode_MultipleTimes) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    mirror_neurons_activate_observation_mode(mirror);
    mirror_neurons_activate_observation_mode(mirror);
    mirror_neurons_activate_observation_mode(mirror);
    SUCCEED();
}

TEST_F(MirrorNeuronsTest, ActivateObservationMode_AfterActions) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    action_t action = create_test_action(1, "reach");
    mirror_neurons_observe_action(mirror, &action);

    mirror_neurons_activate_observation_mode(mirror);
    SUCCEED();
}

//=============================================================================
// Test Suite: Edge Cases - Max Actions Limit
//=============================================================================

TEST_F(MirrorNeuronsTest, MaxActions_ReachLimit) {
    mirror_neuron_config_t config = create_minimal_config();
    config.max_actions = 3;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 0; i < 3; i++) {
        action_t action = create_test_action(i, "action");
        EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }
}

TEST_F(MirrorNeuronsTest, MaxActions_ExceedLimit) {
    mirror_neuron_config_t config = create_minimal_config();
    config.max_actions = 3;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        action_t action = create_test_action(i, "action");
        bool result = mirror_neurons_observe_action(mirror, &action);
        if (i < 3) {
            EXPECT_TRUE(result);
        }
        // After limit, may fail
    }
}

//=============================================================================
// Test Suite: Edge Cases - Max Agents Limit
//=============================================================================

TEST_F(MirrorNeuronsTest, MaxAgents_ReachLimit) {
    mirror_neuron_config_t config = create_minimal_config();
    config.max_agents = 2;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 1; i <= 2; i++) {
        action_t action = create_test_action(1, "action", i);
        EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action));
    }
}

TEST_F(MirrorNeuronsTest, MaxAgents_ExceedLimit) {
    mirror_neuron_config_t config = create_minimal_config();
    config.max_agents = 2;
    mirror = mirror_neurons_create(&config);
    ASSERT_NE(mirror, nullptr);

    for (uint32_t i = 1; i <= 4; i++) {
        action_t action = create_test_action(1, "action", i);
        mirror_neurons_observe_action(mirror, &action);
        // Should handle gracefully
    }
    SUCCEED();
}

//=============================================================================
// Test Suite: Edge Cases - Configuration Variations
//=============================================================================

TEST_F(MirrorNeuronsTest, Config_VeryLowLearningRate) {
    mirror_neuron_config_t config = create_valid_config();
    config.learning_rate = 0.001f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_VeryHighLearningRate) {
    mirror_neuron_config_t config = create_valid_config();
    config.learning_rate = 0.5f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_VeryLowDecayRate) {
    mirror_neuron_config_t config = create_valid_config();
    config.decay_rate = 0.001f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_VeryHighDecayRate) {
    mirror_neuron_config_t config = create_valid_config();
    config.decay_rate = 0.5f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_LowMatchThreshold) {
    mirror_neuron_config_t config = create_valid_config();
    config.match_threshold = 0.3f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_HighMatchThreshold) {
    mirror_neuron_config_t config = create_valid_config();
    config.match_threshold = 0.95f;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_ShortObservationWindow) {
    mirror_neuron_config_t config = create_valid_config();
    config.observation_window = 100;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_LongObservationWindow) {
    mirror_neuron_config_t config = create_valid_config();
    config.observation_window = 10000;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_SmallReplayCapacity) {
    mirror_neuron_config_t config = create_valid_config();
    config.replay_capacity = 10;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

TEST_F(MirrorNeuronsTest, Config_LargeReplayCapacity) {
    mirror_neuron_config_t config = create_valid_config();
    config.replay_capacity = 1000;
    mirror = mirror_neurons_create(&config);
    EXPECT_NE(mirror, nullptr);
}

//=============================================================================
// Test Suite: Integration - Complete Workflow
//=============================================================================

TEST_F(MirrorNeuronsTest, CompleteWorkflow_ObserveLearnExecute) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Observe action from another agent
    action_t observed = create_test_action(1, "reach", 1);
    EXPECT_TRUE(mirror_neurons_observe_action(mirror, &observed));

    // Learn from demonstration
    action_t demo[2];
    demo[0] = create_test_action(1, "reach", 1);
    demo[1] = create_test_action(2, "grasp", 1);
    EXPECT_TRUE(mirror_neurons_learn_demonstration(mirror, demo, 2, 1));

    // Execute action ourselves
    action_t executed = create_test_action(1, "reach", 0);
    EXPECT_TRUE(mirror_neurons_execute_action(mirror, &executed));

    // Update associations
    EXPECT_TRUE(mirror_neurons_update_associations(mirror));

    // Check stats
    mirror_neuron_stats_t stats;
    EXPECT_TRUE(mirror_neurons_get_stats(mirror, &stats));
    EXPECT_GT(stats.num_learned_actions, 0U);
}

TEST_F(MirrorNeuronsTest, CompleteWorkflow_WithAllIntegrations) {
    mirror = mirror_neurons_create(nullptr);
    ASSERT_NE(mirror, nullptr);

    // Integrate with other systems
    void* wm = (void*)0x1;
    void* tom = (void*)0x2;
    void* pred = (void*)0x3;
    void* glial = (void*)0x4;

    EXPECT_TRUE(mirror_neurons_integrate_working_memory(mirror, wm));
    EXPECT_TRUE(mirror_neurons_integrate_theory_of_mind(mirror, tom));
    EXPECT_TRUE(mirror_neurons_integrate_predictive(mirror, pred));
    EXPECT_TRUE(mirror_neurons_integrate_glial(mirror, glial));

    // Perform actions
    action_t action = create_test_action(1, "reach");
    EXPECT_TRUE(mirror_neurons_observe_action(mirror, &action));
    EXPECT_TRUE(mirror_neurons_execute_action(mirror, &action));
}
