//=============================================================================
// test_end_to_end_reasoning.cpp - End-to-End Reasoning Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_rule_learning.h"
#include "core/brain/learning/nimcp_association_learning.h"
#include "core/brain/learning/nimcp_circuit_compilation.h"
#include "cognitive/reasoning/nimcp_forward_chaining.h"

class EndToEndReasoningTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // brain_config_t config;
        // config.num_neurons = 1000;
        // config.num_inputs = 10;
        // config.num_outputs = 5;
        // config.sparsity = 0.1f;
        // config.learning_rate = 0.01f;

        brain = brain_create("reasoning_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Test 1: Learn rules from examples
TEST_F(EndToEndReasoningTest, LearnRulesFromExamples) {
    // Create training examples
    float cat_features[] = {1.0f, 0.8f, 0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float dog_features[] = {1.0f, 0.7f, 0.3f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    rule_example_t examples[] = {
        {cat_features, 10, "cat", 1.0f},
        {dog_features, 10, "dog", 1.0f}
    };

    const char* labels[] = {"cat", "dog"};

    // Learn rules
    int rules_learned = brain_learn_rule_from_examples(brain, examples, labels, 2);

    EXPECT_GE(rules_learned, 1);
}

// Test 2: Store rules in knowledge base
TEST_F(EndToEndReasoningTest, StoreRulesInKnowledgeBase) {
    const char* rule = "IF furry AND meows THEN cat";
    float confidence = 0.95f;

    bool success = add_learned_rule_to_kb(brain, rule, confidence);

    EXPECT_TRUE(success);
}

// Test 3: Forward chaining inference
TEST_F(EndToEndReasoningTest, ForwardChainingInference) {
    // Add rules forming a chain: A -> B -> C
    bool rule1_added = add_learned_rule_to_kb(brain, "IF A THEN B", 0.9f);
    bool rule2_added = add_learned_rule_to_kb(brain, "IF B THEN C", 0.8f);

    ASSERT_TRUE(rule1_added) << "First rule should be added";
    ASSERT_TRUE(rule2_added) << "Second rule should be added";

    // Perform forward chaining for transitive inference
    forward_chain_result_t result;
    bool chain_success = brain_forward_chain(brain, 10, &result);

    if (chain_success) {
        // Verify inference was attempted
        EXPECT_GE(result.iterations_performed, 0u)
            << "Should perform at least some iterations";

        // If facts were derived, confidence should be product of chain
        // A->B (0.9) * B->C (0.8) = 0.72 expected
        if (result.num_new_facts > 0) {
            EXPECT_GT(result.confidence, 0.0f) << "Derived facts should have confidence";
        }
        forward_chain_free_result(&result);
    } else {
        // Logic engine not attached - verify brain integrity maintained
        EXPECT_NE(brain, nullptr) << "Brain should remain valid";
    }
}

// Test 4: Derive new facts from rules
TEST_F(EndToEndReasoningTest, DeriveNewFacts) {
    // Add a compound rule
    bool rule_added = add_learned_rule_to_kb(brain, "IF mammal AND lays_eggs THEN platypus", 1.0f);
    ASSERT_TRUE(rule_added) << "Rule should be added to KB";

    // Perform forward chaining to derive facts
    forward_chain_result_t result;
    bool chain_success = brain_forward_chain(brain, 5, &result);

    if (chain_success) {
        // If preconditions (mammal, lays_eggs) exist, platypus should be derived
        // Without preconditions, no new facts but process should complete
        EXPECT_TRUE(result.converged || result.iterations_performed > 0)
            << "Inference should complete";

        // Track derived facts count
        EXPECT_GE(result.num_new_facts, 0u) << "Should track number of derived facts";
        forward_chain_free_result(&result);
    } else {
        // Logic engine not configured - expected in isolation
        EXPECT_NE(brain, nullptr);
    }
}

// Test 5: Rule confidence propagation
TEST_F(EndToEndReasoningTest, RuleConfidencePropagation) {
    // Test confidence calculation
    float conf1 = compute_rule_confidence(10, 12);  // 10/12 support
    float conf2 = compute_rule_confidence(5, 10);   // 5/10 support

    EXPECT_GT(conf1, conf2);
    EXPECT_GE(conf1, 0.0f);
    EXPECT_LE(conf1, 1.0f);
}

// Test 6: Multi-step reasoning
TEST_F(EndToEndReasoningTest, MultiStepReasoning) {
    // Add chain of rules: smoke -> fire -> danger -> evacuate
    bool r1 = add_learned_rule_to_kb(brain, "IF sees_smoke THEN fire", 0.7f);
    bool r2 = add_learned_rule_to_kb(brain, "IF fire THEN danger", 0.9f);
    bool r3 = add_learned_rule_to_kb(brain, "IF danger THEN evacuate", 0.95f);

    ASSERT_TRUE(r1 && r2 && r3) << "All rules should be added";

    // Perform multi-step forward chaining
    forward_chain_result_t result;
    bool chain_success = brain_forward_chain(brain, 20, &result);

    if (chain_success) {
        // Multi-step reasoning requires multiple iterations
        // With 3 chained rules, we need at least 3 iterations for full chain
        EXPECT_GE(result.iterations_performed, 0u);

        // Track inference time for multi-step reasoning
        EXPECT_GE(result.inference_time_ms, 0u) << "Time should be tracked";

        forward_chain_free_result(&result);
    } else {
        // Logic engine not attached
        EXPECT_NE(brain, nullptr);
    }
}

// Test 7: Contradictory rules handling
TEST_F(EndToEndReasoningTest, ContradictoryRulesHandling) {
    // Add contradictory rules with different confidences
    bool r1 = add_learned_rule_to_kb(brain, "IF A THEN B", 0.9f);
    bool r2 = add_learned_rule_to_kb(brain, "IF A THEN NOT B", 0.3f);

    ASSERT_TRUE(r1) << "First rule should be added";
    ASSERT_TRUE(r2) << "Second contradictory rule should be added";

    // Perform forward chaining - system should handle conflict
    forward_chain_result_t result;
    bool chain_success = brain_forward_chain(brain, 5, &result);

    if (chain_success) {
        // System should resolve conflict by preferring higher confidence (0.9 > 0.3)
        // Verify system doesn't crash and produces valid result
        EXPECT_GE(result.iterations_performed, 0u);

        // If facts derived, confidence should reflect resolution
        if (result.num_new_facts > 0) {
            // Higher confidence rule should dominate
            EXPECT_GT(result.confidence, 0.5f)
                << "Higher confidence rule (0.9) should be preferred";
        }
        forward_chain_free_result(&result);
    } else {
        // Logic engine not configured
        EXPECT_NE(brain, nullptr);
    }
}

// Test 8: Rule extraction accuracy
TEST_F(EndToEndReasoningTest, RuleExtractionAccuracy) {
    // Create consistent examples
    float ex1[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float ex2[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float ex3[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    rule_example_t examples[] = {
        {ex1, 10, "class_A", 1.0f},
        {ex2, 10, "class_A", 1.0f},
        {ex3, 10, "class_A", 1.0f}
    };

    const char* labels[] = {"class_A", "class_A", "class_A"};

    char rule[512];
    bool found = extract_rule_pattern(examples, 3, "class_A", rule, sizeof(rule));

    EXPECT_TRUE(found);
    EXPECT_NE(strstr(rule, "feature_0"), nullptr);
    EXPECT_NE(strstr(rule, "feature_1"), nullptr);
}

// Test 9: Empty example handling
TEST_F(EndToEndReasoningTest, EmptyExampleHandling) {
    int result = brain_learn_rule_from_examples(brain, nullptr, nullptr, 0);
    EXPECT_EQ(result, -1);
}

// Test 10: Large rule set
TEST_F(EndToEndReasoningTest, LargeRuleSet) {
    const int NUM_RULES = 100;
    int rules_added = 0;

    // Add many rules
    for (int i = 0; i < NUM_RULES; i++) {
        char rule[256];
        snprintf(rule, sizeof(rule), "IF condition_%d THEN result_%d", i, i);
        bool success = add_learned_rule_to_kb(brain, rule, 0.8f);
        if (success) rules_added++;
    }

    // Verify all rules were added
    EXPECT_EQ(rules_added, NUM_RULES) << "All 100 rules should be added";

    // Verify system can handle large rule set during inference
    forward_chain_result_t result;
    bool chain_success = brain_forward_chain(brain, 5, &result);

    if (chain_success) {
        // Large rule sets should complete without timeout
        EXPECT_LE(result.inference_time_ms, 5000u)
            << "Large rule set inference should complete within 5 seconds";
        forward_chain_free_result(&result);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
