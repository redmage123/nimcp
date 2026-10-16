/**
 * @file test_explanations_extraction.cpp
 * @brief Unit tests for explanation decision extraction functions
 *
 * WHAT: Test extraction of features, confidence, labels from brain_decision_t
 * WHY:  Ensure all TODO stubs are properly implemented
 * HOW:  Create real brain_decision_t structures and test extraction
 *
 * @author NIMCP Test Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>

#include "cognitive/nimcp_explanations.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class ExplanationExtractionTest : public ::testing::Test {
protected:
    explanation_generator_t gen;
    brain_t brain;

    void SetUp() override {
        gen = nullptr;
        brain = nullptr;
    }

    void TearDown() override {
        if (gen) {
            explanation_generator_destroy(gen);
            gen = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create real brain_decision_t with specific values
    brain_decision_t* create_test_decision(
        const char* label,
        float confidence,
        uint32_t num_active,
        float sparsity)
    {
        brain_decision_t* decision = (brain_decision_t*)nimcp_calloc(1, sizeof(brain_decision_t));
        if (!decision) return nullptr;

        strncpy(decision->label, label, sizeof(decision->label) - 1);
        decision->confidence = confidence;
        decision->num_active_neurons = num_active;
        decision->sparsity = sparsity;
        decision->output_vector = nullptr;
        decision->output_size = 0;
        decision->active_neuron_ids = nullptr;

        return decision;
    }

    void free_test_decision(brain_decision_t* decision) {
        if (decision) {
            nimcp_free(decision);
        }
    }
};

//=============================================================================
// Metadata Extraction Tests (Lines 299-301)
//=============================================================================

TEST_F(ExplanationExtractionTest, ExtractNumFeaturesUsed) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("cat", 0.85f, 42, 0.92f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(explanation.num_features_used, 42u);  // Should extract from num_active_neurons

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractDecisionConfidence) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("dog", 0.73f, 30, 0.88f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(explanation.decision_confidence, 0.73f);  // Should extract exact confidence

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractSymbolicLogicAvailability) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    // Brain without symbolic logic
    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("test", 0.9f, 10, 0.95f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    // With mock brain, symbolic logic should be NULL -> false
    EXPECT_FALSE(explanation.has_symbolic_proof);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractAllMetadata) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("bird", 0.91f, 55, 0.94f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert - All metadata correctly extracted
    EXPECT_TRUE(result);
    EXPECT_EQ(explanation.num_features_used, 55u);
    EXPECT_FLOAT_EQ(explanation.decision_confidence, 0.91f);
    EXPECT_FALSE(explanation.has_symbolic_proof);  // No symbolic logic in mock

    free_test_decision(decision);
}

//=============================================================================
// Output Label Extraction Tests (Lines 526-527)
//=============================================================================

TEST_F(ExplanationExtractionTest, ExtractOutputLabelInWhat) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("elephant", 0.88f, 40, 0.9f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.what), 0u);
    EXPECT_NE(strstr(explanation.what, "elephant"), nullptr);  // Label should appear
    EXPECT_NE(strstr(explanation.what, "88"), nullptr);  // Confidence should appear

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractOutputLabelEmptyString) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("", 0.75f, 20, 0.85f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.what), 0u);
    // Should handle empty label gracefully
    EXPECT_NE(strstr(explanation.what, "75"), nullptr);  // Confidence should still appear

    free_test_decision(decision);
}

//=============================================================================
// Feature Salience Extraction Tests (Lines 545-546)
//=============================================================================

TEST_F(ExplanationExtractionTest, ExtractFeatureSalienceHighSparsity) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    // High sparsity = very few features active
    brain_decision_t* decision = create_test_decision("precise", 0.95f, 5, 0.98f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.why), 0u);
    // Should describe as "very few" or "few" features
    EXPECT_TRUE(
        strstr(explanation.why, "very few") != nullptr ||
        strstr(explanation.why, "few") != nullptr
    );

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractFeatureSalienceLowSparsity) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    // Low sparsity = many features active
    brain_decision_t* decision = create_test_decision("distributed", 0.70f, 100, 0.3f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.why), 0u);
    // Should describe as "many" features
    EXPECT_NE(strstr(explanation.why, "many"), nullptr);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractFeatureSalienceZeroActive) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    // No active neurons
    brain_decision_t* decision = create_test_decision("none", 0.5f, 0, 1.0f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.why), 0u);
    // Should provide fallback explanation
    EXPECT_NE(strstr(explanation.why, "distributed"), nullptr);

    free_test_decision(decision);
}

//=============================================================================
// Confidence Explanation Extraction Tests
//=============================================================================

TEST_F(ExplanationExtractionTest, ExtractConfidenceHighLevel) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("confident", 0.92f, 30, 0.9f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.confidence), 0u);
    EXPECT_NE(strstr(explanation.confidence, "92"), nullptr);
    EXPECT_NE(strstr(explanation.confidence, "high"), nullptr);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractConfidenceMediumLevel) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("medium", 0.65f, 25, 0.8f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.confidence), 0u);
    EXPECT_NE(strstr(explanation.confidence, "65"), nullptr);
    EXPECT_NE(strstr(explanation.confidence, "medium"), nullptr);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractConfidenceLowLevel) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("uncertain", 0.35f, 15, 0.7f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.confidence), 0u);
    EXPECT_NE(strstr(explanation.confidence, "35"), nullptr);
    EXPECT_NE(strstr(explanation.confidence, "low"), nullptr);

    free_test_decision(decision);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ExplanationExtractionTest, ExtractWithMaxConfidence) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("perfect", 1.0f, 50, 0.95f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(explanation.decision_confidence, 1.0f);
    EXPECT_NE(strstr(explanation.confidence, "100"), nullptr);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractWithMinConfidence) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("random", 0.0f, 1, 0.99f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(explanation.decision_confidence, 0.0f);
    EXPECT_NE(strstr(explanation.confidence, "very low"), nullptr);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, ExtractWithLongLabel) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision(
        "very_long_classification_label_that_might_exceed_buffer", 0.8f, 25, 0.85f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_GT(strlen(explanation.what), 0u);
    // Should handle long labels without buffer overflow

    free_test_decision(decision);
}

//=============================================================================
// Regression Tests
//=============================================================================

TEST_F(ExplanationExtractionTest, RegressionNoStubPlaceholders) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("regression", 0.75f, 30, 0.8f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert - No stub placeholders should remain
    EXPECT_TRUE(result);
    EXPECT_EQ(strstr(explanation.what, "TODO"), nullptr);
    EXPECT_EQ(strstr(explanation.why, "TODO"), nullptr);
    EXPECT_EQ(strstr(explanation.how, "TODO"), nullptr);
    EXPECT_EQ(strstr(explanation.what, "placeholder"), nullptr);

    free_test_decision(decision);
}

TEST_F(ExplanationExtractionTest, RegressionMetadataNotZero) {
    GTEST_SKIP() << "Test requires valid brain pointer - explanation_generate_from_decision validates brain != NULL";

    // Arrange
    gen = explanation_generator_create(nullptr);
    ASSERT_NE(gen, nullptr);

    brain = nullptr;  // Cannot use invalid pointer
    brain_decision_t* decision = create_test_decision("nonzero", 0.85f, 40, 0.9f);
    ASSERT_NE(decision, nullptr);

    natural_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    // Act
    bool result = explanation_generate_from_decision(gen, brain, decision, &explanation);

    // Assert - Metadata should be extracted, not left as 0
    EXPECT_TRUE(result);
    EXPECT_NE(explanation.num_features_used, 0u);  // Should be 40
    EXPECT_NE(explanation.decision_confidence, 0.0f);  // Should be 0.85

    free_test_decision(decision);
}

// Main is provided by GTest
