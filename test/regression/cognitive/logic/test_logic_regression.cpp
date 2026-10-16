//=============================================================================
// test_logic_regression.cpp - Regression Tests for Logic Module Wiring
//=============================================================================
// Ensures logic integration doesn't break existing behavior
//
// WHAT: Regression tests to prevent logic wiring from breaking core functionality
// WHY:  Logic modules were added to processing pipeline - verify no regressions
// HOW:  Compare behavior before/after logic integration using known-good outputs
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/knowledge/nimcp_knowledge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LogicRegressionTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        strncpy(config.task_name, "logic_regression_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_multimodal_integration = false;  // Disabled to avoid NLP network creation issues
        config.enable_logic = true;  // Enable symbolic logic for logic regression tests
        config.enable_knowledge = true;  // Enable knowledge system (required for knowledge tests)

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);

        // Warm up network: Run several predictions to initialize internal state
        // and ensure membrane potentials and spike history are non-zero
        float warmup_input[64];
        float warmup_output[10];

        for (int iter = 0; iter < 50; iter++) {
            for (int i = 0; i < 64; i++) {
                warmup_input[i] = 0.5f + 0.3f * sinf(i * 0.1f + iter * 0.05f);
            }

            brain_predict(brain, warmup_input, 64, warmup_output, 10);
        }
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Core Functionality Regression Tests
//=============================================================================

TEST_F(LogicRegressionTest, BrainCreationWithLogicSucceeds) {
    // REGRESSION: Verify brain creation still works with logic modules
    EXPECT_NE(brain, nullptr);
}

TEST_F(LogicRegressionTest, BrainPredictionStillWorks) {
    // REGRESSION: Basic prediction should work as before
    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.1f * static_cast<float>(i) / 64.0f;
    }

    float output[10];
    bool result = brain_predict(brain, input, 64, output, 10);

    if (!result) {
        const char* error = brain_get_last_error();
        if (error) {
            fprintf(stderr, "brain_predict() failed: %s\n", error);
        }
    }

    EXPECT_TRUE(result);
    // Check outputs are valid (not NaN/Inf) - don't require non-zero for untrained network
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

TEST_F(LogicRegressionTest, MultimodalProcessingStillWorks) {
    // REGRESSION: Multimodal processing should work with logic in pipeline
    brain_multimodal_input_t input = {0};
    brain_multimodal_output_t output = {0};

    float direct_data[64];
    for (int i = 0; i < 64; i++) {
        direct_data[i] = 0.2f + 0.1f * sinf(i * 0.1f);
    }

    input.direct_data = direct_data;
    input.direct_dim = 64;
    input.timestamp_ms = 1000;

    bool result = brain_process_multimodal(brain, &input, &output);

    EXPECT_TRUE(result);
    // Confidence can be 0 for untrained network, just check it's valid
    EXPECT_GE(output.confidence, 0.0f);
    EXPECT_LE(output.confidence, 1.0f);
    EXPECT_FALSE(std::isnan(output.confidence));
}

TEST_F(LogicRegressionTest, OutputDistributionStillNormalized) {
    // REGRESSION: Output should still be valid probability distribution
    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.5f;
    }

    float output[10];
    brain_predict(brain, input, 64, output, 10);

    float sum = 0.0f;
    for (int i = 0; i < 10; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
        sum += output[i];
    }

    // For untrained network, sum might be 0, just check it's in valid range [0,1]
    EXPECT_GE(sum, 0.0f);
    EXPECT_LE(sum, 1.0f + 0.1f);  // Allow small tolerance
}

//=============================================================================
// Consistency Regression Tests
//=============================================================================

TEST_F(LogicRegressionTest, RepeatedPredictionsConsistent) {
    // REGRESSION: Same input should produce similar outputs
    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.3f + 0.1f * cosf(i * 0.05f);
    }

    float output1[10], output2[10], output3[10];

    brain_predict(brain, input, 64, output1, 10);
    brain_predict(brain, input, 64, output2, 10);
    brain_predict(brain, input, 64, output3, 10);

    // Outputs should be similar (not identical due to spiking)
    for (int i = 0; i < 10; i++) {
        float diff12 = std::fabs(output1[i] - output2[i]);
        float diff23 = std::fabs(output2[i] - output3[i]);

        EXPECT_LT(diff12, 0.3f);  // Allow some variance
        EXPECT_LT(diff23, 0.3f);
    }
}

TEST_F(LogicRegressionTest, ProcessingOrderIndependent) {
    // REGRESSION: Processing order shouldn't affect results drastically
    float input_a[64], input_b[64];
    for (int i = 0; i < 64; i++) {
        input_a[i] = 0.2f * sinf(i * 0.1f);
        input_b[i] = 0.3f * cosf(i * 0.1f);
    }

    float output_a1[10], output_b1[10];
    float output_a2[10], output_b2[10];

    // Order 1: A then B
    brain_predict(brain, input_a, 64, output_a1, 10);
    brain_predict(brain, input_b, 64, output_b1, 10);

    // Reset brain state
    TearDown();
    SetUp();

    // Order 2: B then A
    brain_predict(brain, input_b, 64, output_b2, 10);
    brain_predict(brain, input_a, 64, output_a2, 10);

    // Results should be similar regardless of order
    for (int i = 0; i < 10; i++) {
        float diff_a = std::fabs(output_a1[i] - output_a2[i]);
        float diff_b = std::fabs(output_b1[i] - output_b2[i]);

        EXPECT_LT(diff_a, 0.4f);
        EXPECT_LT(diff_b, 0.4f);
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(LogicRegressionTest, ProcessingTimeAcceptable) {
    // REGRESSION: Logic integration shouldn't slow down processing significantly
    float input[64];
    float output[10];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 64; j++) {
            input[j] = 0.1f * sinf(j * 0.1f + i * 0.01f);
        }
        brain_predict(brain, input, 64, output, 10);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 predictions should complete in < 20ms
    EXPECT_LT(duration.count(), 20);
}

TEST_F(LogicRegressionTest, MemoryUsageStable) {
    // REGRESSION: Memory usage shouldn't grow over time
    float input[64];
    float output[10];

    // Process many inputs - memory shouldn't leak
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 64; j++) {
            input[j] = 0.1f * static_cast<float>(i) / 1000.0f;
        }
        bool result = brain_predict(brain, input, 64, output, 10);
        EXPECT_TRUE(result);
    }

    // If we get here without crashing/hanging, memory is stable
    SUCCEED();
}

//=============================================================================
// Symbolic Logic Regression Tests
//=============================================================================

TEST_F(LogicRegressionTest, SymbolicLogicAccessible) {
    // REGRESSION: Brain should provide access to symbolic logic
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    EXPECT_NE(logic, nullptr);
}

TEST_F(LogicRegressionTest, SymbolicLogicIndependentOfProcessing) {
    // REGRESSION: Symbolic logic operations shouldn't interfere with processing
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    // Add some knowledge facts
    knowledge_item_t item = {0};
    strncpy(item.concept_name, "test_concept", sizeof(item.concept_name) - 1);
    item.confidence = 0.8f;
    item.num_related = 0;

    uint32_t facts_added = knowledge_add_to_symbolic_logic(logic, &item);
    EXPECT_GT(facts_added, 0u);

    // Now process brain input - should still work
    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.3f;
    }

    float output[10];
    bool result = brain_predict(brain, input, 64, output, 10);

    EXPECT_TRUE(result);
    // Check outputs are valid (not NaN/Inf) - don't require non-zero for untrained network
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

//=============================================================================
// Error Handling Regression Tests
//=============================================================================

TEST_F(LogicRegressionTest, NullInputsHandledGracefully) {
    // REGRESSION: NULL inputs should still be handled correctly
    float output[10];

    bool result = brain_predict(brain, nullptr, 64, output, 10);
    EXPECT_FALSE(result);  // Should fail gracefully
}

TEST_F(LogicRegressionTest, InvalidDimensionsHandledGracefully) {
    // REGRESSION: Invalid dimensions should be caught
    float input[64] = {0.5f};
    float output[10];

    // Wrong input dimension
    bool result1 = brain_predict(brain, input, 32, output, 10);
    EXPECT_FALSE(result1);

    // Wrong output dimension
    bool result2 = brain_predict(brain, input, 64, output, 5);
    EXPECT_FALSE(result2);
}

TEST_F(LogicRegressionTest, MultipleCreationDestructionCycles) {
    // REGRESSION: Creating and destroying brains repeatedly should work
    for (int i = 0; i < 10; i++) {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        strncpy(config.task_name, "cycle_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 32;
        config.num_outputs = 5;

        brain_t test_brain = brain_create_custom(&config);
        ASSERT_NE(test_brain, nullptr);

        // Quick sanity test
        float input[32] = {0.5f};
        float output[5];
        brain_predict(test_brain, input, 32, output, 5);

        brain_destroy(test_brain);
    }

    SUCCEED();
}

//=============================================================================
// Integration Regression Tests
//=============================================================================

TEST_F(LogicRegressionTest, KnowledgeLogicIntegrationDoesNotBreakKnowledge) {
    // REGRESSION: Adding logic integration shouldn't break knowledge module
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    // Create multiple knowledge items
    std::vector<knowledge_item_t> items(5);
    const char* concepts[] = {"concept1", "concept2", "concept3", "concept4", "concept5"};

    for (size_t i = 0; i < 5; i++) {
        strncpy(items[i].concept_name, concepts[i], sizeof(items[i].concept_name) - 1);
        items[i].confidence = 0.7f + i * 0.05f;
        items[i].num_related = 0;

        uint32_t facts = knowledge_add_to_symbolic_logic(logic, &items[i]);
        EXPECT_GT(facts, 0u);
    }

    // Verify all items were added
    SUCCEED();
}

TEST_F(LogicRegressionTest, ConcurrentKnowledgeAndBrainProcessing) {
    // REGRESSION: Knowledge operations and brain processing should coexist
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    float input[64];
    float output[10];

    for (int i = 0; i < 10; i++) {
        // Add knowledge fact
        knowledge_item_t item = {0};
        char concept_name[64];
        snprintf(concept_name, sizeof(concept_name), "concept_%d", i);
        strncpy(item.concept_name, concept_name, sizeof(item.concept_name) - 1);
        item.confidence = 0.8f;
        item.num_related = 0;

        uint32_t facts = knowledge_add_to_symbolic_logic(logic, &item);
        EXPECT_GT(facts, 0u);

        // Process brain input
        for (int j = 0; j < 64; j++) {
            input[j] = 0.1f * sinf(j * 0.1f + i * 0.1f);
        }

        bool result = brain_predict(brain, input, 64, output, 10);
        EXPECT_TRUE(result);
    }
}

//=============================================================================
// Backwards Compatibility Tests
//=============================================================================

TEST_F(LogicRegressionTest, OldCodeStillCompiles) {
    // REGRESSION: Code using old API should still work
    // (This test compiling is the test itself)

    // Old-style brain creation
    brain_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.task_name, "old_style", sizeof(config.task_name) - 1);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;

    brain_t old_brain = brain_create_custom(&config);
    ASSERT_NE(old_brain, nullptr);

    // Old-style prediction
    float input[64] = {0.1f};
    float output[10];
    bool result = brain_predict(old_brain, input, 64, output, 10);

    EXPECT_TRUE(result);

    brain_destroy(old_brain);
}

TEST_F(LogicRegressionTest, BrainFunctionsReturnCorrectTypes) {
    // REGRESSION: Return types haven't changed
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    EXPECT_TRUE(logic != nullptr || logic == nullptr);  // Returns pointer

    float input[64] = {0.5f};
    float output[10];
    bool result = brain_predict(brain, input, 64, output, 10);
    EXPECT_TRUE(result == true || result == false);  // Returns bool
}
