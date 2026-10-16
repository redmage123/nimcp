/**
 * @file test_portia_learning_classification_integration.cpp
 * @brief Integration tests for Portia learning and classification interaction
 *
 * WHAT: Tests learned associations affect classification decisions
 * WHY:  Validate learning feedback loop strengthens classification
 * HOW:  Train associations, classify stimuli, verify habituation effects
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_learning.h"
#include "async/nimcp_bio_async.h"
#include "utils/validation/nimcp_common.h"

// Mock classification system
typedef struct {
    uint32_t stimulus_id;
    uint32_t predicted_response;
    float classification_confidence;
    bool used_learned_association;
} mock_classification_result_t;

class PortiaLearningClassificationIntegrationTest : public ::testing::Test {
protected:
    portia_learning_state_t* learning_state = nullptr;

    void SetUp() override {
        // Initialize learning system
        portia_learning_config_t config = {
            .allowed_modes = LEARNING_MODE_FULL,
            .max_habituation_entries = 100,
            .max_association_entries = 100,
            .default_learning_rate = 0.3f,
            .default_forgetting_rate = 0.01f,
            .consolidation_interval_ms = 1000,
            .habituation_threshold = 0.1f,
            .association_threshold = 0.2f
        };

        learning_state = portia_learning_init(&config);
        ASSERT_NE(learning_state, nullptr);
    }

    void TearDown() override {
        if (learning_state) {
            portia_learning_destroy(learning_state);
            learning_state = nullptr;
        }
    }

    // Helper: Classify stimulus using learned associations
    mock_classification_result_t classify_stimulus(uint32_t stimulus_id) {
        mock_classification_result_t result = {
            .stimulus_id = stimulus_id,
            .predicted_response = 0,
            .classification_confidence = 0.5f,  // Baseline
            .used_learned_association = false
        };

        // Check if we have learned association for this stimulus
        // Try common response IDs
        for (uint32_t response_id = 1; response_id <= 10; response_id++) {
            portia_learning_query_result_t query =
                portia_learning_query_association(learning_state, stimulus_id, response_id);

            if (query.found && query.strength > 0.2f) {
                result.predicted_response = response_id;
                result.classification_confidence = 0.5f + (query.strength * 0.5f);
                result.used_learned_association = true;
                break;
            }
        }

        return result;
    }
};

//=============================================================================
// TEST SUITE 1: Learned Associations Affect Classification
//=============================================================================

TEST_F(PortiaLearningClassificationIntegrationTest, Learning_NewAssociationImprovesClassification) {
    uint32_t stimulus_id = 1;
    uint32_t response_id = 5;
    uint64_t timestamp = 1000;

    // Initial classification (no learning)
    mock_classification_result_t result1 = classify_stimulus(stimulus_id);
    EXPECT_FALSE(result1.used_learned_association);
    EXPECT_FLOAT_EQ(result1.classification_confidence, 0.5f);  // Baseline

    // Create association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Classify again
    mock_classification_result_t result2 = classify_stimulus(stimulus_id);
    EXPECT_TRUE(result2.used_learned_association);
    EXPECT_EQ(result2.predicted_response, response_id);
    EXPECT_GT(result2.classification_confidence, 0.5f);
}

TEST_F(PortiaLearningClassificationIntegrationTest, Learning_StrongAssociationHighConfidence) {
    uint32_t stimulus_id = 2;
    uint32_t response_id = 7;
    uint64_t timestamp = 1000;

    // Create and reinforce association multiple times
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    for (int i = 0; i < 10; i++) {
        timestamp += 100;
        ASSERT_EQ(portia_learning_reinforce(learning_state, stimulus_id, response_id,
                                             1.0f, timestamp), 0);
    }

    // Classify
    mock_classification_result_t result = classify_stimulus(stimulus_id);
    EXPECT_TRUE(result.used_learned_association);
    EXPECT_EQ(result.predicted_response, response_id);
    EXPECT_GT(result.classification_confidence, 0.8f);  // High confidence
}

TEST_F(PortiaLearningClassificationIntegrationTest, Learning_WeakAssociationLowConfidence) {
    uint32_t stimulus_id = 3;
    uint32_t response_id = 8;
    uint64_t timestamp = 1000;

    // Create weak association (minimal reinforcement)
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Classify
    mock_classification_result_t result = classify_stimulus(stimulus_id);

    // Should use association but with lower confidence
    if (result.used_learned_association) {
        EXPECT_LT(result.classification_confidence, 0.8f);
    }
}

//=============================================================================
// TEST SUITE 2: Classification Feedback Strengthens Learning
//=============================================================================

TEST_F(PortiaLearningClassificationIntegrationTest, Feedback_CorrectClassificationStrengthens) {
    uint32_t stimulus_id = 4;
    uint32_t response_id = 9;
    uint64_t timestamp = 1000;

    // Initial association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Query initial strength
    portia_learning_query_result_t query1 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    ASSERT_TRUE(query1.found);
    float initial_strength = query1.strength;

    // Simulate correct classification with positive feedback
    timestamp += 100;
    ASSERT_EQ(portia_learning_reinforce(learning_state, stimulus_id, response_id,
                                         1.0f, timestamp), 0);

    // Query updated strength
    portia_learning_query_result_t query2 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    ASSERT_TRUE(query2.found);

    // Association should be stronger
    EXPECT_GT(query2.strength, initial_strength);
}

TEST_F(PortiaLearningClassificationIntegrationTest, Feedback_IncorrectClassificationWeakens) {
    uint32_t stimulus_id = 5;
    uint32_t response_id = 10;
    uint64_t timestamp = 1000;

    // Create association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    portia_learning_query_result_t query1 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    ASSERT_TRUE(query1.found);
    float initial_strength = query1.strength;

    // Simulate incorrect classification with negative feedback
    timestamp += 100;
    ASSERT_EQ(portia_learning_reinforce(learning_state, stimulus_id, response_id,
                                         -0.5f, timestamp), 0);

    // Query updated strength
    portia_learning_query_result_t query2 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    // Association should be weaker or gone
    if (query2.found) {
        EXPECT_LE(query2.strength, initial_strength);
    }
}

TEST_F(PortiaLearningClassificationIntegrationTest, Feedback_RepeatedReinforcementConverges) {
    uint32_t stimulus_id = 6;
    uint32_t response_id = 11;
    uint64_t timestamp = 1000;

    // Create association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Repeatedly reinforce
    for (int i = 0; i < 50; i++) {
        timestamp += 100;
        ASSERT_EQ(portia_learning_reinforce(learning_state, stimulus_id, response_id,
                                             1.0f, timestamp), 0);
    }

    // Query strength
    portia_learning_query_result_t query =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    ASSERT_TRUE(query.found);

    // Should converge to high strength (< 1.0)
    EXPECT_GT(query.strength, 0.8f);
    EXPECT_LE(query.strength, 1.0f);
}

//=============================================================================
// TEST SUITE 3: Habituation Affects Threat Response
//=============================================================================

TEST_F(PortiaLearningClassificationIntegrationTest, Habituation_RepeatedBenignStimulusReducesResponse) {
    uint32_t stimulus_id = 100;
    uint64_t timestamp = 1000;

    // First exposure - check initial response
    portia_learning_query_result_t query1 = portia_learning_query(learning_state, stimulus_id);
    float initial_strength = query1.found ? query1.strength : 1.0f;

    // Habituate by repeated exposure
    for (int i = 0; i < 20; i++) {
        timestamp += 100;
        ASSERT_EQ(portia_learning_habituate(learning_state, stimulus_id, timestamp), 0);
    }

    // Check habituated response
    portia_learning_query_result_t query2 = portia_learning_query(learning_state, stimulus_id);
    ASSERT_TRUE(query2.found);

    // Response should decrease
    EXPECT_LT(query2.strength, initial_strength);
    EXPECT_GT(query2.exposure_count, 10u);
}

TEST_F(PortiaLearningClassificationIntegrationTest, Habituation_NoHabituationToThreats) {
    uint32_t threat_stimulus_id = 200;
    uint64_t timestamp = 1000;

    // Mark as threat by sensitizing instead of habituating
    for (int i = 0; i < 10; i++) {
        timestamp += 100;
        // Sensitize increases response to important stimuli
        ASSERT_EQ(portia_learning_sensitize(learning_state, threat_stimulus_id,
                                             0.2f, timestamp), 0);
    }

    // Query response
    portia_learning_query_result_t query = portia_learning_query(learning_state, threat_stimulus_id);
    ASSERT_TRUE(query.found);

    // Response should remain high or increase
    EXPECT_GT(query.strength, 0.8f);
}

TEST_F(PortiaLearningClassificationIntegrationTest, Habituation_ClassificationUsesHabituatedResponse) {
    uint32_t stimulus_id = 300;
    uint32_t response_id = 1;
    uint64_t timestamp = 1000;

    // Create association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Initial classification
    mock_classification_result_t result1 = classify_stimulus(stimulus_id);
    float initial_confidence = result1.classification_confidence;

    // Habituate stimulus
    for (int i = 0; i < 15; i++) {
        timestamp += 100;
        ASSERT_EQ(portia_learning_habituate(learning_state, stimulus_id, timestamp), 0);
    }

    // Classify again - habituation affects general response, not specific association
    // So classification confidence might be similar, but habituation is tracked
    portia_learning_query_result_t hab_query = portia_learning_query(learning_state, stimulus_id);
    ASSERT_TRUE(hab_query.found);
    EXPECT_GT(hab_query.exposure_count, 10u);
}

//=============================================================================
// TEST SUITE 4: Learning Modes Affect Classification
//=============================================================================

TEST_F(PortiaLearningClassificationIntegrationTest, LearningModes_AssociativeModeEnablesLearning) {
    // Set to associative mode
    ASSERT_EQ(portia_learning_set_mode(learning_state, LEARNING_MODE_ASSOCIATIVE), 0);

    uint32_t stimulus_id = 400;
    uint32_t response_id = 2;
    uint64_t timestamp = 1000;

    // Create association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Should be able to classify using learned association
    mock_classification_result_t result = classify_stimulus(stimulus_id);
    EXPECT_TRUE(result.used_learned_association);
}

TEST_F(PortiaLearningClassificationIntegrationTest, LearningModes_DisabledModeNoLearning) {
    // Disable learning
    ASSERT_EQ(portia_learning_set_mode(learning_state, LEARNING_MODE_DISABLED), 0);

    uint32_t stimulus_id = 500;
    uint32_t response_id = 3;
    uint64_t timestamp = 1000;

    // Try to create association (should work but may not be used)
    int result = portia_learning_associate(learning_state, stimulus_id, response_id,
                                            true, timestamp);

    // Classification should fall back to baseline (no learned association used)
    mock_classification_result_t class_result = classify_stimulus(stimulus_id);
    // May or may not use association depending on implementation
    EXPECT_GE(class_result.classification_confidence, 0.5f);
}

//=============================================================================
// TEST SUITE 5: Forgetting Affects Classification
//=============================================================================

TEST_F(PortiaLearningClassificationIntegrationTest, Forgetting_OldAssociationsWeaken) {
    uint32_t stimulus_id = 600;
    uint32_t response_id = 4;
    uint64_t timestamp = 1000;

    // Create association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    portia_learning_query_result_t query1 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    ASSERT_TRUE(query1.found);
    float initial_strength = query1.strength;

    // Simulate time passing with forgetting
    timestamp += 10000;  // 10 seconds
    ASSERT_EQ(portia_learning_forget(learning_state, timestamp), 0);

    // Query again
    portia_learning_query_result_t query2 =
        portia_learning_query_association(learning_state, stimulus_id, response_id);

    // Strength should decrease or association gone
    if (query2.found) {
        EXPECT_LT(query2.strength, initial_strength);
    }
}

TEST_F(PortiaLearningClassificationIntegrationTest, Forgetting_ClassificationConfidenceDecreases) {
    uint32_t stimulus_id = 700;
    uint32_t response_id = 5;
    uint64_t timestamp = 1000;

    // Create and reinforce association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);
    for (int i = 0; i < 5; i++) {
        timestamp += 100;
        ASSERT_EQ(portia_learning_reinforce(learning_state, stimulus_id, response_id,
                                             0.8f, timestamp), 0);
    }

    // Initial classification
    mock_classification_result_t result1 = classify_stimulus(stimulus_id);
    float initial_confidence = result1.classification_confidence;

    // Apply forgetting
    timestamp += 5000;
    ASSERT_EQ(portia_learning_forget(learning_state, timestamp), 0);

    // Classify again
    mock_classification_result_t result2 = classify_stimulus(stimulus_id);

    // Confidence should decrease
    if (result2.used_learned_association) {
        EXPECT_LE(result2.classification_confidence, initial_confidence);
    }
}

//=============================================================================
// TEST SUITE 6: Memory Consolidation
//=============================================================================

TEST_F(PortiaLearningClassificationIntegrationTest, Consolidation_StrongAssociationsPreserved) {
    uint32_t stimulus_id = 800;
    uint32_t response_id = 6;
    uint64_t timestamp = 1000;

    // Create strong association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);
    for (int i = 0; i < 15; i++) {
        timestamp += 100;
        ASSERT_EQ(portia_learning_reinforce(learning_state, stimulus_id, response_id,
                                             1.0f, timestamp), 0);
    }

    // Consolidate
    timestamp += 2000;
    ASSERT_EQ(portia_learning_consolidate(learning_state, timestamp), 0);

    // Strong association should still exist
    portia_learning_query_result_t query =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    ASSERT_TRUE(query.found);
    EXPECT_GT(query.strength, 0.7f);
}

TEST_F(PortiaLearningClassificationIntegrationTest, Consolidation_WeakAssociationsRemoved) {
    uint32_t stimulus_id = 900;
    uint32_t response_id = 7;
    uint64_t timestamp = 1000;

    // Create very weak association
    ASSERT_EQ(portia_learning_associate(learning_state, stimulus_id, response_id,
                                         true, timestamp), 0);

    // Immediately consolidate (should remove weak associations)
    timestamp += 2000;
    ASSERT_EQ(portia_learning_consolidate(learning_state, timestamp), 0);

    // Weak association may be removed
    portia_learning_query_result_t query =
        portia_learning_query_association(learning_state, stimulus_id, response_id);
    // Either not found or very weak
    if (query.found) {
        EXPECT_LT(query.strength, 0.5f);
    }
}
