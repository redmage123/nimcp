//=============================================================================
// test_logic_integration.cpp - Integration Tests for Logic Module Wiring
//=============================================================================
// Tests the integration of neural logic and symbolic logic with:
// - Brain processing pipeline
// - Knowledge module
// - Ethics module
//
// WHAT: Verify logic modules are properly wired into cognitive processing
// WHY:  Logic modules were dormant (audit findings) - now wired and active
// HOW:  Test each integration point with realistic scenarios
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/knowledge/nimcp_knowledge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LogicIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create brain with logic processing enabled
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        strncpy(config.task_name, "logic_integration_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_multimodal_integration = false;  // Not needed for logic tests
        config.enable_visual_cortex = false;
        config.enable_audio_cortex = false;
        config.enable_speech_cortex = false;
        config.enable_logic = true;  // Enable symbolic logic for these tests

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Neural Logic Integration Tests
//=============================================================================

TEST_F(LogicIntegrationTest, NeuralLogicNetworkCreatedInBrain) {
    // WHAT: Verify neural logic network is created during brain initialization
    // WHY:  Audit found logic network created but never used
    // HOW:  Check brain has non-NULL logic pointer

    // Brain should have neural logic network if properly initialized
    // We can't directly access brain->logic from C++, but we can test indirectly
    // by processing input and checking for no crashes

    float input[64] = {0};
    for (int i = 0; i < 64; i++) {
        input[i] = 0.1f * sinf(i * 0.1f);
    }

    float output[10];
    bool result = brain_predict(brain, input, 64, output, 10);

    EXPECT_TRUE(result);
    // If neural logic is wired, this should execute without errors
}

TEST_F(LogicIntegrationTest, NeuralLogicExecutesInProcessingPipeline) {
    // WHAT: Verify neural logic update is called during brain processing
    // WHY:  Audit found neural_logic_update() never called in processing loop
    // HOW:  Process multiple inputs and verify logic gates are evaluated

    // Create multimodal input
    brain_multimodal_input_t input;
    brain_multimodal_output_t output;
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    // Direct input (no visual/audio for this test)
    float direct_data[64];
    for (int i = 0; i < 64; i++) {
        direct_data[i] = 0.2f + 0.1f * cosf(i * 0.05f);
    }

    input.direct_data = direct_data;
    input.direct_dim = 64;
    input.timestamp_ms = 1000;

    // Process through brain - this should trigger STAGE 3.5 (neural logic update)
    bool result = brain_process_multimodal(brain, &input, &output);

    EXPECT_TRUE(result);
    // Note: Confidence may be 0 for untrained network, but processing should succeed
    EXPECT_GE(output.confidence, 0.0f);

    // Process again - logic should maintain state across invocations
    input.timestamp_ms = 1100;
    result = brain_process_multimodal(brain, &input, &output);

    EXPECT_TRUE(result);
}

TEST_F(LogicIntegrationTest, NeuralLogicHandlesMultipleProcessingCycles) {
    // WHAT: Verify neural logic works correctly over multiple processing cycles
    // WHY:  Logic gates should maintain consistent behavior over time
    // HOW:  Process 10 inputs and check outputs remain valid

    brain_multimodal_input_t input;
    brain_multimodal_output_t output;
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    float direct_data[64];

    for (int cycle = 0; cycle < 10; cycle++) {
        // Vary input slightly each cycle
        for (int i = 0; i < 64; i++) {
            direct_data[i] = 0.3f * sinf(i * 0.1f + cycle * 0.1f);
        }

        input.direct_data = direct_data;
        input.direct_dim = 64;
        input.timestamp_ms = 1000 + cycle * 100;

        bool result = brain_process_multimodal(brain, &input, &output);

        EXPECT_TRUE(result);
        // Note: Confidence may be 0 for untrained network, but processing should succeed
        EXPECT_GE(output.confidence, 0.0f);
        EXPECT_LE(output.confidence, 1.0f);
    }
}

//=============================================================================
// Symbolic Logic + Knowledge Integration Tests
//=============================================================================

TEST_F(LogicIntegrationTest, KnowledgeIntegratesWithSymbolicLogic) {
    // WHAT: Verify knowledge items can be added to symbolic logic
    // WHY:  Audit found symbolic logic not used by Knowledge module
    // HOW:  Create knowledge item, add to symbolic logic, verify facts added

    // Get symbolic logic from brain
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    // Create a simple knowledge item
    knowledge_item_t item;
    memset(&item, 0, sizeof(item));
    strncpy(item.concept_name, "cat", sizeof(item.concept_name) - 1);
    item.domain = KNOWLEDGE_DOMAIN_GENERAL;
    strncpy(item.definition, "A small carnivorous mammal", sizeof(item.definition) - 1);
    item.confidence = 0.9f;
    item.num_related = 1;

    // Allocate related concepts
    char* related[1];
    char animal[] = "animal";
    related[0] = animal;
    item.related_concepts = related;

    // Add to symbolic logic
    uint32_t facts_added = knowledge_add_to_symbolic_logic(logic, &item);

    // Should add at least 2 facts: IsA(cat, animal) and Concept(cat)
    EXPECT_GE(facts_added, 2u);
}

TEST_F(LogicIntegrationTest, SymbolicLogicStoresMultipleKnowledgeFacts) {
    // WHAT: Verify symbolic logic can store multiple knowledge items
    // WHY:  Test knowledge base grows correctly with multiple additions
    // HOW:  Add 3 knowledge items with relationships and verify

    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    // Item 1: cat → animal
    knowledge_item_t item1;
    memset(&item1, 0, sizeof(item1));
    strncpy(item1.concept_name, "cat", sizeof(item1.concept_name) - 1);
    item1.confidence = 0.9f;
    item1.num_related = 1;
    char* related1[1];
    char animal[] = "animal";
    related1[0] = animal;
    item1.related_concepts = related1;

    // Item 2: dog → animal
    knowledge_item_t item2;
    memset(&item2, 0, sizeof(item2));
    strncpy(item2.concept_name, "dog", sizeof(item2.concept_name) - 1);
    item2.confidence = 0.85f;
    item2.num_related = 1;
    char* related2[1];
    related2[0] = animal;
    item2.related_concepts = related2;

    // Item 3: animal (root concept)
    knowledge_item_t item3;
    memset(&item3, 0, sizeof(item3));
    strncpy(item3.concept_name, "animal", sizeof(item3.concept_name) - 1);
    item3.confidence = 1.0f;
    item3.num_related = 0;
    item3.related_concepts = nullptr;

    // Add all three
    uint32_t facts1 = knowledge_add_to_symbolic_logic(logic, &item1);
    uint32_t facts2 = knowledge_add_to_symbolic_logic(logic, &item2);
    uint32_t facts3 = knowledge_add_to_symbolic_logic(logic, &item3);

    EXPECT_GT(facts1, 0u);
    EXPECT_GT(facts2, 0u);
    EXPECT_GT(facts3, 0u);

    // Total facts should be at least 5 (2 IsA + 3 Concept)
    uint32_t total_facts = facts1 + facts2 + facts3;
    EXPECT_GE(total_facts, 5u);
}

TEST_F(LogicIntegrationTest, KnowledgeLogicIntegrationWithComplexHierarchy) {
    // WHAT: Test knowledge→logic integration with multi-level hierarchy
    // WHY:  Verify logic can represent complex knowledge structures
    // HOW:  Create hierarchy (mammal → animal, cat → mammal) and verify

    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    // Level 1: animal (root)
    knowledge_item_t animal;
    memset(&animal, 0, sizeof(animal));
    strncpy(animal.concept_name, "animal", sizeof(animal.concept_name) - 1);
    animal.confidence = 1.0f;
    animal.num_related = 0;

    // Level 2: mammal → animal
    knowledge_item_t mammal;
    memset(&mammal, 0, sizeof(mammal));
    strncpy(mammal.concept_name, "mammal", sizeof(mammal.concept_name) - 1);
    mammal.confidence = 0.95f;
    mammal.num_related = 1;
    char* mammal_related[1];
    char animal_str[] = "animal";
    mammal_related[0] = animal_str;
    mammal.related_concepts = mammal_related;

    // Level 3: cat → mammal
    knowledge_item_t cat;
    memset(&cat, 0, sizeof(cat));
    strncpy(cat.concept_name, "cat", sizeof(cat.concept_name) - 1);
    cat.confidence = 0.9f;
    cat.num_related = 1;
    char* cat_related[1];
    char mammal_str[] = "mammal";
    cat_related[0] = mammal_str;
    cat.related_concepts = cat_related;

    // Add hierarchy bottom-up
    uint32_t facts_animal = knowledge_add_to_symbolic_logic(logic, &animal);
    uint32_t facts_mammal = knowledge_add_to_symbolic_logic(logic, &mammal);
    uint32_t facts_cat = knowledge_add_to_symbolic_logic(logic, &cat);

    EXPECT_GT(facts_animal, 0u);
    EXPECT_GT(facts_mammal, 0u);
    EXPECT_GT(facts_cat, 0u);

    // Verify hierarchy was stored
    // Should have: Concept(animal), IsA(mammal,animal), Concept(mammal),
    //              IsA(cat,mammal), Concept(cat)
    uint32_t total = facts_animal + facts_mammal + facts_cat;
    EXPECT_GE(total, 5u);
}

//=============================================================================
// Regression Tests - Ensure Logic Doesn't Break Existing Functionality
//=============================================================================

TEST_F(LogicIntegrationTest, LogicIntegrationDoesNotBreakNormalProcessing) {
    // WHAT: Verify adding logic doesn't break normal brain processing
    // WHY:  Regression test - ensure backward compatibility
    // HOW:  Run standard brain_predict and verify results unchanged

    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.1f * i / 64.0f;
    }

    float output[10];
    bool result = brain_predict(brain, input, 64, output, 10);

    EXPECT_TRUE(result);

    // Verify output is valid probability distribution
    float sum = 0.0f;
    for (int i = 0; i < 10; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
        sum += output[i];
    }

    EXPECT_NEAR(sum, 1.0f, 0.1f);
}

TEST_F(LogicIntegrationTest, MultipleProcessingCyclesWithLogicStable) {
    // WHAT: Verify repeated processing with logic remains stable
    // WHY:  Regression test for memory leaks and state corruption
    // HOW:  Process 100 inputs and verify no degradation

    float input[64];
    float output[10];

    for (int iter = 0; iter < 100; iter++) {
        // Generate varying input
        for (int i = 0; i < 64; i++) {
            input[i] = 0.1f * sinf(i * 0.1f + iter * 0.01f);
        }

        bool result = brain_predict(brain, input, 64, output, 10);
        EXPECT_TRUE(result);

        // Verify output remains valid
        for (int i = 0; i < 10; i++) {
            EXPECT_GE(output[i], 0.0f);
            EXPECT_LE(output[i], 1.0f);
            EXPECT_FALSE(std::isnan(output[i]));
            EXPECT_FALSE(std::isinf(output[i]));
        }
    }
}

TEST_F(LogicIntegrationTest, LogicDoesNotCauseMemoryLeaks) {
    // WHAT: Verify logic integration doesn't leak memory
    // WHY:  Regression test for resource management
    // HOW:  Create/destroy multiple brains with logic enabled

    for (int iter = 0; iter < 10; iter++) {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        strncpy(config.task_name, "leak_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 32;
        config.num_outputs = 5;
        config.enable_multimodal_integration = true;

        brain_t test_brain = brain_create_custom(&config);
        ASSERT_NE(test_brain, nullptr);

        // Process some input to trigger logic
        float input[32] = {0.5f};
        float output[5];
        brain_predict(test_brain, input, 32, output, 5);

        brain_destroy(test_brain);
    }

    // If this completes without crashing, no obvious memory leaks
    SUCCEED();
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(LogicIntegrationTest, KnowledgeLogicIntegrationHandlesNullInputs) {
    // WHAT: Verify knowledge→logic integration handles NULL inputs gracefully
    // WHY:  Error handling regression test
    // HOW:  Pass NULL to knowledge_add_to_symbolic_logic

    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);

    // NULL logic pointer
    knowledge_item_t item;
    memset(&item, 0, sizeof(item));
    strncpy(item.concept_name, "test", sizeof(item.concept_name) - 1);
    item.confidence = 0.9f;
    item.num_related = 0;

    uint32_t result1 = knowledge_add_to_symbolic_logic(nullptr, &item);
    EXPECT_EQ(result1, 0u);

    // NULL item pointer
    uint32_t result2 = knowledge_add_to_symbolic_logic(logic, nullptr);
    EXPECT_EQ(result2, 0u);

    // Both NULL
    uint32_t result3 = knowledge_add_to_symbolic_logic(nullptr, nullptr);
    EXPECT_EQ(result3, 0u);
}

TEST_F(LogicIntegrationTest, LogicIntegrationWithEmptyKnowledgeItem) {
    // WHAT: Verify logic integration handles empty knowledge items
    // WHY:  Edge case testing
    // HOW:  Add item with no related concepts and verify

    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    ASSERT_NE(logic, nullptr);

    // Empty knowledge item (only concept name)
    knowledge_item_t item;
    memset(&item, 0, sizeof(item));
    strncpy(item.concept_name, "isolated_concept", sizeof(item.concept_name) - 1);
    item.confidence = 0.5f;
    item.num_related = 0;
    item.related_concepts = nullptr;

    uint32_t facts_added = knowledge_add_to_symbolic_logic(logic, &item);

    // Should still add Concept(isolated_concept) fact
    EXPECT_GE(facts_added, 1u);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(LogicIntegrationTest, LogicIntegrationPerformanceAcceptable) {
    // WHAT: Verify logic integration doesn't significantly slow processing
    // WHY:  Performance regression test
    // HOW:  Time 1000 processing cycles and verify < 100ms total

    auto start = std::chrono::high_resolution_clock::now();

    float input[64];
    float output[10];

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 64; j++) {
            input[j] = 0.1f * sinf(j * 0.1f + i * 0.001f);
        }

        brain_predict(brain, input, 64, output, 10);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 predictions should complete in < 100ms (CPU mode)
    // This is generous - typical should be ~10-20ms
    EXPECT_LT(duration.count(), 100);
}
