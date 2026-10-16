/**
 * @file test_explanations_real.cpp
 * @brief Real tests for explanations module
 *
 * Tests only functions that ACTUALLY EXIST in nimcp_explanations.h
 */

#include <gtest/gtest.h>

#include "cognitive/nimcp_explanations.h"
#include "core/brain/nimcp_brain.h"

class ExplanationsRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    explanation_generator_t gen = nullptr;

    void SetUp() override {
        brain = brain_create("test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        // Create generator with default config
        gen = explanation_generator_create(nullptr);
    }

    void TearDown() override {
        if (gen) explanation_generator_destroy(gen);
        if (brain) brain_destroy(brain);
    }
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

TEST_F(ExplanationsRealTest, Create_WithNullConfig_ReturnsNonNull) {
    explanation_generator_t test_gen = explanation_generator_create(nullptr);
    EXPECT_NE(test_gen, nullptr);
    explanation_generator_destroy(test_gen);
}

TEST_F(ExplanationsRealTest, Create_WithCustomConfig_ReturnsNonNull) {
    explanation_config_t config = explanation_default_config();

    explanation_generator_t test_gen = explanation_generator_create(&config);
    EXPECT_NE(test_gen, nullptr);
    explanation_generator_destroy(test_gen);
}

TEST_F(ExplanationsRealTest, Destroy_NullGenerator_DoesNotCrash) {
    // Should not crash
    explanation_generator_destroy(nullptr);
}

TEST_F(ExplanationsRealTest, Destroy_ValidGenerator_DoesNotCrash) {
    explanation_generator_t test_gen = explanation_generator_create(nullptr);
    ASSERT_NE(test_gen, nullptr);

    // Should not crash
    explanation_generator_destroy(test_gen);
}

//=============================================================================
// Default Configuration
//=============================================================================

TEST_F(ExplanationsRealTest, DefaultConfig_AllFlagsEnabled) {
    explanation_config_t config = explanation_default_config();

    EXPECT_TRUE(config.generate_what);
    EXPECT_TRUE(config.generate_why);
    EXPECT_TRUE(config.generate_how);
    EXPECT_TRUE(config.generate_confidence);
    EXPECT_TRUE(config.generate_alternatives);
    EXPECT_TRUE(config.generate_counterfactuals);
}

TEST_F(ExplanationsRealTest, DefaultConfig_ReasonableValues) {
    explanation_config_t config = explanation_default_config();

    EXPECT_GT(config.max_alternatives, 0);
    EXPECT_GE(config.min_alternative_prob, 0.0f);
    EXPECT_LE(config.min_alternative_prob, 1.0f);
}

//=============================================================================
// Generate Explanation from Decision
//=============================================================================

TEST_F(ExplanationsRealTest, GenerateFromDecision_NullGenerator_ReturnsFalse) {
    natural_explanation_t explanation = {};

    bool result = explanation_generate_from_decision(
        nullptr, brain, nullptr, &explanation
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, GenerateFromDecision_NullBrain_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    natural_explanation_t explanation = {};

    bool result = explanation_generate_from_decision(
        gen, nullptr, nullptr, &explanation
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, GenerateFromDecision_NullOutput_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);

    bool result = explanation_generate_from_decision(
        gen, brain, nullptr, nullptr
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, GenerateFromDecision_ValidInputs_MaySucceed) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);

    natural_explanation_t explanation = {};

    // May succeed or fail depending on brain state
    bool result = explanation_generate_from_decision(
        gen, brain, nullptr, &explanation
    );

    // Just ensure no crash
}

//=============================================================================
// Generate Explanation from Multimodal
//=============================================================================

TEST_F(ExplanationsRealTest, GenerateFromMultimodal_NullGenerator_ReturnsFalse) {
    natural_explanation_t explanation = {};

    bool result = explanation_generate_from_multimodal(
        nullptr, brain, nullptr, &explanation
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, GenerateFromMultimodal_NullBrain_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    natural_explanation_t explanation = {};

    bool result = explanation_generate_from_multimodal(
        gen, nullptr, nullptr, &explanation
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, GenerateFromMultimodal_NullOutput_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);

    bool result = explanation_generate_from_multimodal(
        gen, brain, nullptr, nullptr
    );

    EXPECT_FALSE(result);
}

//=============================================================================
// Symbolic Logic Proof
//=============================================================================

TEST_F(ExplanationsRealTest, SymbolicLogic_NullGenerator_ReturnsFalse) {
    char proof[512] = {0};

    bool result = explain_with_symbolic_logic(
        nullptr, brain, nullptr, proof, sizeof(proof)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, SymbolicLogic_NullBrain_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    char proof[512] = {0};

    bool result = explain_with_symbolic_logic(
        gen, nullptr, nullptr, proof, sizeof(proof)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, SymbolicLogic_NullBuffer_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);

    bool result = explain_with_symbolic_logic(
        gen, brain, nullptr, nullptr, 512
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, SymbolicLogic_ZeroBufferSize_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char proof[512] = {0};

    bool result = explain_with_symbolic_logic(
        gen, brain, nullptr, proof, 0
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, SymbolicLogic_ValidInputs_MaySucceed) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char proof[512] = {0};

    // May succeed or fail depending on symbolic logic availability
    bool result = explain_with_symbolic_logic(
        gen, brain, nullptr, proof, sizeof(proof)
    );

    // Just ensure no crash
}

//=============================================================================
// Causal Chain
//=============================================================================

TEST_F(ExplanationsRealTest, CausalChain_NullGenerator_ReturnsFalse) {
    char chain[512] = {0};

    bool result = generate_causal_chain(
        nullptr, brain, "input", "output", chain, sizeof(chain)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, CausalChain_NullBrain_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    char chain[512] = {0};

    bool result = generate_causal_chain(
        gen, nullptr, "input", "output", chain, sizeof(chain)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, CausalChain_NullInputDescription_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char chain[512] = {0};

    bool result = generate_causal_chain(
        gen, brain, nullptr, "output", chain, sizeof(chain)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, CausalChain_NullOutputLabel_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char chain[512] = {0};

    bool result = generate_causal_chain(
        gen, brain, "input", nullptr, chain, sizeof(chain)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, CausalChain_NullBuffer_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);

    bool result = generate_causal_chain(
        gen, brain, "input", "output", nullptr, 512
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, CausalChain_ZeroBufferSize_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char chain[512] = {0};

    bool result = generate_causal_chain(
        gen, brain, "input", "output", chain, 0
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, CausalChain_ValidInputs_MaySucceed) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char chain[512] = {0};

    // May succeed or fail depending on implementation
    bool result = generate_causal_chain(
        gen, brain, "test input", "test output", chain, sizeof(chain)
    );

    // Just ensure no crash
}

//=============================================================================
// Counterfactual Explanation
//=============================================================================

TEST_F(ExplanationsRealTest, Counterfactual_NullGenerator_ReturnsFalse) {
    char counterfactual[256] = {0};

    bool result = generate_counterfactual(
        nullptr, brain, nullptr, "feature", "change",
        counterfactual, sizeof(counterfactual)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, Counterfactual_NullBrain_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    char counterfactual[256] = {0};

    bool result = generate_counterfactual(
        gen, nullptr, nullptr, "feature", "change",
        counterfactual, sizeof(counterfactual)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, Counterfactual_NullFeature_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char counterfactual[256] = {0};

    bool result = generate_counterfactual(
        gen, brain, nullptr, nullptr, "change",
        counterfactual, sizeof(counterfactual)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, Counterfactual_NullModification_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char counterfactual[256] = {0};

    bool result = generate_counterfactual(
        gen, brain, nullptr, "feature", nullptr,
        counterfactual, sizeof(counterfactual)
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, Counterfactual_NullBuffer_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);

    bool result = generate_counterfactual(
        gen, brain, nullptr, "feature", "change", nullptr, 256
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, Counterfactual_ZeroBufferSize_ReturnsFalse) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char counterfactual[256] = {0};

    bool result = generate_counterfactual(
        gen, brain, nullptr, "feature", "change",
        counterfactual, 0
    );

    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, Counterfactual_ValidInputs_MaySucceed) {
    ASSERT_NE(gen, nullptr);
    ASSERT_NE(brain, nullptr);
    char counterfactual[256] = {0};

    // May succeed or fail depending on implementation
    bool result = generate_counterfactual(
        gen, brain, nullptr, "test_feature", "test_change",
        counterfactual, sizeof(counterfactual)
    );

    // Just ensure no crash
}

//=============================================================================
// Utility Functions
//=============================================================================

TEST_F(ExplanationsRealTest, Print_NullExplanation_DoesNotCrash) {
    // Should not crash with NULL
    explanation_print(nullptr);
}

TEST_F(ExplanationsRealTest, Print_ValidExplanation_DoesNotCrash) {
    natural_explanation_t explanation = {};
    strncpy(explanation.what, "test what", sizeof(explanation.what) - 1);
    strncpy(explanation.why, "test why", sizeof(explanation.why) - 1);
    strncpy(explanation.how, "test how", sizeof(explanation.how) - 1);

    // Should not crash
    explanation_print(&explanation);
}

TEST_F(ExplanationsRealTest, ToJson_NullExplanation_ReturnsFalse) {
    char json[2048] = {0};

    bool result = explanation_to_json(nullptr, json, sizeof(json));
    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, ToJson_NullBuffer_ReturnsFalse) {
    natural_explanation_t explanation = {};

    bool result = explanation_to_json(&explanation, nullptr, 2048);
    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, ToJson_ZeroBufferSize_ReturnsFalse) {
    natural_explanation_t explanation = {};
    char json[2048] = {0};

    bool result = explanation_to_json(&explanation, json, 0);
    EXPECT_FALSE(result);
}

TEST_F(ExplanationsRealTest, ToJson_TinyBuffer_HandledGracefully) {
    natural_explanation_t explanation = {};
    char json[10] = {0};

    // Implementation may truncate or return false
    bool result = explanation_to_json(&explanation, json, sizeof(json));
    // Just ensure it doesn't crash - may succeed or fail
}

TEST_F(ExplanationsRealTest, ToJson_ValidInputs_MaySucceed) {
    natural_explanation_t explanation = {};
    strncpy(explanation.what, "test classification", sizeof(explanation.what) - 1);
    strncpy(explanation.why, "because features detected", sizeof(explanation.why) - 1);
    strncpy(explanation.how, "through processing pipeline", sizeof(explanation.how) - 1);
    strncpy(explanation.confidence, "90% certain", sizeof(explanation.confidence) - 1);
    explanation.decision_confidence = 0.9f;
    explanation.num_features_used = 5;

    char json[2048] = {0};

    bool result = explanation_to_json(&explanation, json, sizeof(json));

    if (result) {
        // Should produce valid JSON
        EXPECT_GT(strlen(json), 0);
        EXPECT_NE(strstr(json, "what"), nullptr);
    }
}

//=============================================================================
// Explanation Structure
//=============================================================================

TEST_F(ExplanationsRealTest, ExplanationStructure_HasAllFields) {
    natural_explanation_t explanation = {};

    // Ensure all fields are accessible
    EXPECT_EQ(explanation.what[0], '\0');
    EXPECT_EQ(explanation.why[0], '\0');
    EXPECT_EQ(explanation.how[0], '\0');
    EXPECT_EQ(explanation.confidence[0], '\0');
    EXPECT_EQ(explanation.alternatives[0], '\0');
    EXPECT_EQ(explanation.counterfactual[0], '\0');
    EXPECT_EQ(explanation.num_features_used, 0);
    EXPECT_EQ(explanation.decision_confidence, 0.0f);
    EXPECT_FALSE(explanation.has_symbolic_proof);
}

TEST_F(ExplanationsRealTest, ExplanationStructure_StringFieldsAreFixedSize) {
    natural_explanation_t explanation = {};

    // Ensure buffers are properly sized
    EXPECT_GE(sizeof(explanation.what), 256);
    EXPECT_GE(sizeof(explanation.why), 512);
    EXPECT_GE(sizeof(explanation.how), 512);
    EXPECT_GE(sizeof(explanation.confidence), 128);
    EXPECT_GE(sizeof(explanation.alternatives), 256);
    EXPECT_GE(sizeof(explanation.counterfactual), 256);
}
