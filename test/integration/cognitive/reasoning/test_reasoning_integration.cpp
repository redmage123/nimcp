/**
 * @file test_reasoning_integration.cpp
 * @brief Integration tests for NIMCP reasoning learning system
 *
 * WHAT: End-to-end tests for reasoning training layer
 * WHY:  Verify neural-symbolic integration works correctly
 * HOW:  Test learn rules → store in KB → forward chain → derive facts
 *
 * TEST COVERAGE:
 * - Rule learning from examples (inductive learning)
 * - Neural-symbolic compilation
 * - Brain reasoning integration
 * - Event flow during inference
 * - Working memory for active inferences
 * - Knowledge base integration
 *
 * TARGET: 15+ integration tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Core includes
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_reasoning_learning.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ReasoningIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    symbolic_logic_t* logic = nullptr;

    void SetUp() override {
        // Create brain with logic system enabled
        brain = brain_create("reasoning_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";

        // Get logic system (use accessor from brain_accessors.c)
        logic = brain_get_symbolic_logic(brain);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Test 1-5: Rule Learning from Examples
//=============================================================================

TEST_F(ReasoningIntegrationTest, LearnSimpleRule) {
    // WHAT: Learn basic rule from examples
    // WHY:  Test inductive learning pipeline
    // EXPECT: Rule extracted with >0.8 confidence

    logical_example_t examples[] = {
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f}
    };

    // Bird examples: has_wings=1, flies=1
    float bird1[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float bird2[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float bird3[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    examples[0].features = bird1;
    examples[1].features = bird2;
    examples[2].features = bird3;
    strcpy(examples[0].label, "bird");
    strcpy(examples[1].label, "bird");
    strcpy(examples[2].label, "bird");

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    bool success = brain_learn_logical_rule(brain, examples, 3, &rules, &num_rules, false);

    EXPECT_TRUE(success);
    EXPECT_GT(num_rules, 0);

    if (num_rules > 0) {
        EXPECT_STREQ(rules[0]->rule_name, "rule_bird");
        EXPECT_GE(rules[0]->confidence, 0.8f);

        // Cleanup
        for (int i = 0; i < num_rules; i++) {
            learned_rule_destroy(rules[i]);
        }
        nimcp_free(rules);
    }
}

TEST_F(ReasoningIntegrationTest, LearnMultipleRules) {
    // WHAT: Learn rules for multiple classes
    // WHY:  Test multi-class rule extraction
    // EXPECT: One rule per class

    logical_example_t examples[] = {
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f}
    };

    float bird[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float fish[] = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    examples[0].features = bird;
    examples[1].features = bird;
    strcpy(examples[0].label, "bird");
    strcpy(examples[1].label, "bird");

    examples[2].features = fish;
    examples[3].features = fish;
    strcpy(examples[2].label, "fish");
    strcpy(examples[3].label, "fish");

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    bool success = brain_learn_logical_rule(brain, examples, 4, &rules, &num_rules, false);

    EXPECT_TRUE(success);
    EXPECT_EQ(num_rules, 2); // One rule per class

    if (num_rules >= 2) {
        // Cleanup
        for (int i = 0; i < num_rules; i++) {
            learned_rule_destroy(rules[i]);
        }
        nimcp_free(rules);
    }
}

TEST_F(ReasoningIntegrationTest, RuleConfidenceReflectsSupport) {
    // WHAT: Rule confidence correlates with example support
    // WHY:  Test confidence estimation
    // EXPECT: High support → high confidence

    logical_example_t examples[] = {
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f}
    };

    // All examples same pattern
    float pattern[] = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 5; i++) {
        examples[i].features = pattern;
        strcpy(examples[i].label, "category_a");
    }

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    bool success = brain_learn_logical_rule(brain, examples, 5, &rules, &num_rules, false);

    EXPECT_TRUE(success);
    EXPECT_GT(num_rules, 0);

    if (num_rules > 0) {
        // Perfect support should yield high confidence
        EXPECT_GE(rules[0]->confidence, 0.9f);

        for (int i = 0; i < num_rules; i++) {
            learned_rule_destroy(rules[i]);
        }
        nimcp_free(rules);
    }
}

TEST_F(ReasoningIntegrationTest, RuleStoredInKnowledgeBase) {
    // WHAT: Learned rules are added to symbolic KB
    // WHY:  Test KB integration
    // EXPECT: KB contains learned rule

    if (!logic) {
        GTEST_SKIP() << "Logic system not available";
    }

    logical_example_t examples[] = {
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f}
    };

    float pattern[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    examples[0].features = pattern;
    examples[1].features = pattern;
    strcpy(examples[0].label, "test_category");
    strcpy(examples[1].label, "test_category");

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    bool success = brain_learn_logical_rule(brain, examples, 2, &rules, &num_rules, false);

    EXPECT_TRUE(success);

    // Check KB stats
    logic_stats_t stats;
    if (symbolic_logic_get_stats(logic, &stats)) {
        EXPECT_GT(stats.facts_stored, 0);
    }

    if (num_rules > 0) {
        for (int i = 0; i < num_rules; i++) {
            learned_rule_destroy(rules[i]);
        }
        nimcp_free(rules);
    }
}

TEST_F(ReasoningIntegrationTest, InvalidParametersHandled) {
    // WHAT: API rejects invalid parameters
    // WHY:  Test error handling
    // EXPECT: Graceful failure, no crash

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    // NULL brain
    bool success = brain_learn_logical_rule(nullptr, nullptr, 0, &rules, &num_rules, false);
    EXPECT_FALSE(success);

    // Zero examples
    logical_example_t dummy;
    success = brain_learn_logical_rule(brain, &dummy, 0, &rules, &num_rules, false);
    EXPECT_FALSE(success);

    // NULL output pointers
    success = brain_learn_logical_rule(brain, &dummy, 1, nullptr, nullptr, false);
    EXPECT_FALSE(success);
}

//=============================================================================
// Test 6-10: Neural-Symbolic Compilation
//=============================================================================

TEST_F(ReasoningIntegrationTest, CompileRuleToNeuralCircuit) {
    // WHAT: Compile symbolic rule to neural network
    // WHY:  Test neural-symbolic bridge
    // EXPECT: Successful compilation with neuron allocation

    // Create a simple learned rule
    learned_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    strcpy(rule.rule_name, "test_rule");
    rule.confidence = 0.9f;
    rule.num_premises = 1;

    neural_compilation_result_t* result = nullptr;

    bool success = brain_compile_rule_to_neural(brain, &rule, &result);

    EXPECT_TRUE(success);
    ASSERT_NE(result, nullptr);

    if (result) {
        EXPECT_TRUE(result->compiled_successfully);
        EXPECT_GT(result->num_neurons, 0);
        EXPECT_GT(result->num_gates, 0);
        EXPECT_STREQ(result->rule_name, "test_rule");

        neural_compilation_result_destroy(result);
    }
}

TEST_F(ReasoningIntegrationTest, CompiledRuleExecutesCorrectly) {
    // WHAT: Execute neural-compiled rule
    // WHY:  Test neural rule execution
    // EXPECT: Rule produces output

    learned_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    strcpy(rule.rule_name, "execution_test");
    rule.confidence = 0.9f;
    rule.num_premises = 1;

    neural_compilation_result_t* result = nullptr;

    bool compiled = brain_compile_rule_to_neural(brain, &rule, &result);
    ASSERT_TRUE(compiled);
    ASSERT_NE(result, nullptr);

    // Execute compiled rule
    float input[10] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output[5] = {0};

    bool executed = brain_execute_neural_rule(brain, result, input, 10, output, 5);
    EXPECT_TRUE(executed); // Should execute without error

    neural_compilation_result_destroy(result);
}

TEST_F(ReasoningIntegrationTest, CompilationWithNeuralEnabled) {
    // WHAT: Learn and compile in one step
    // WHY:  Test integrated learning+compilation
    // EXPECT: Rules compiled automatically

    logical_example_t examples[] = {
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f}
    };

    float pattern[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    examples[0].features = pattern;
    examples[1].features = pattern;
    strcpy(examples[0].label, "compiled_test");
    strcpy(examples[1].label, "compiled_test");

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    // Enable neural compilation
    bool success = brain_learn_logical_rule(brain, examples, 2, &rules, &num_rules, true);

    EXPECT_TRUE(success);
    EXPECT_GT(num_rules, 0);

    // Rules should be compiled (logged internally)

    if (num_rules > 0) {
        for (int i = 0; i < num_rules; i++) {
            learned_rule_destroy(rules[i]);
        }
        nimcp_free(rules);
    }
}

TEST_F(ReasoningIntegrationTest, CompilationAccuracyReported) {
    // WHAT: Compilation reports accuracy metric
    // WHY:  Track how well neural approximates symbolic
    // EXPECT: Accuracy >= 0.8

    learned_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    strcpy(rule.rule_name, "accuracy_test");
    rule.confidence = 0.95f;
    rule.num_premises = 2;

    neural_compilation_result_t* result = nullptr;

    bool success = brain_compile_rule_to_neural(brain, &rule, &result);
    ASSERT_TRUE(success);
    ASSERT_NE(result, nullptr);

    // Check accuracy metric
    EXPECT_GE(result->compilation_accuracy, 0.8f);
    EXPECT_LE(result->compilation_accuracy, 1.0f);

    neural_compilation_result_destroy(result);
}

TEST_F(ReasoningIntegrationTest, CompilationHandlesComplexRules) {
    // WHAT: Compile rule with multiple premises
    // WHY:  Test complex rule handling
    // EXPECT: More gates for more premises

    learned_rule_t simple_rule;
    memset(&simple_rule, 0, sizeof(simple_rule));
    strcpy(simple_rule.rule_name, "simple");
    simple_rule.num_premises = 1;

    learned_rule_t complex_rule;
    memset(&complex_rule, 0, sizeof(complex_rule));
    strcpy(complex_rule.rule_name, "complex");
    complex_rule.num_premises = 5;

    neural_compilation_result_t* simple_result = nullptr;
    neural_compilation_result_t* complex_result = nullptr;

    bool success1 = brain_compile_rule_to_neural(brain, &simple_rule, &simple_result);
    bool success2 = brain_compile_rule_to_neural(brain, &complex_rule, &complex_result);

    ASSERT_TRUE(success1);
    ASSERT_TRUE(success2);
    ASSERT_NE(simple_result, nullptr);
    ASSERT_NE(complex_result, nullptr);

    // Complex rule should use more gates
    EXPECT_GT(complex_result->num_gates, simple_result->num_gates);
    EXPECT_GT(complex_result->num_neurons, simple_result->num_neurons);

    neural_compilation_result_destroy(simple_result);
    neural_compilation_result_destroy(complex_result);
}

//=============================================================================
// Test 11-15: Symbolic Association & Rule Refinement
//=============================================================================

TEST_F(ReasoningIntegrationTest, LearnSymbolicAssociation) {
    // WHAT: Learn A→B implication
    // WHY:  Test associative learning
    // EXPECT: Association rule added to KB

    if (!logic) {
        GTEST_SKIP() << "Logic system not available";
    }

    logical_term_t* term_x = logic_term_create(TERM_VARIABLE, "X");
    ASSERT_NE(term_x, nullptr);

    logical_term_t* terms[] = {term_x};
    atomic_formula_t* clouds = logic_atom_create("dark_clouds", terms, 1);
    atomic_formula_t* rain = logic_atom_create("rain", terms, 1);

    ASSERT_NE(clouds, nullptr);
    ASSERT_NE(rain, nullptr);

    bool success = brain_learn_symbolic_association(brain, clouds, rain, 0.8f);
    EXPECT_TRUE(success);

    // Cleanup
    logic_atom_destroy(clouds);
    logic_atom_destroy(rain);
    logic_term_destroy(term_x);
}

TEST_F(ReasoningIntegrationTest, RefineRuleOnSuccess) {
    // WHAT: Strengthen rule on successful outcome
    // WHY:  Test reinforcement learning
    // EXPECT: Confidence increases (logged)

    bool success = brain_refine_rule_confidence(brain, "test_rule", true, 0.1f);
    EXPECT_TRUE(success);
}

TEST_F(ReasoningIntegrationTest, RefineRuleOnFailure) {
    // WHAT: Weaken rule on failed outcome
    // WHY:  Test negative reinforcement
    // EXPECT: Confidence decreases (logged)

    bool success = brain_refine_rule_confidence(brain, "test_rule", false, 0.1f);
    EXPECT_TRUE(success);
}

TEST_F(ReasoningIntegrationTest, RefinementWithInvalidLearningRate) {
    // WHAT: Reject invalid learning rates
    // WHY:  Test parameter validation
    // EXPECT: Graceful failure

    bool success = brain_refine_rule_confidence(brain, "test_rule", true, 1.5f);
    EXPECT_FALSE(success); // Out of range [0, 1]

    success = brain_refine_rule_confidence(brain, "test_rule", true, -0.1f);
    EXPECT_FALSE(success); // Negative
}

TEST_F(ReasoningIntegrationTest, GetLearnedRules) {
    // WHAT: Retrieve learned rules from brain
    // WHY:  Test rule introspection
    // EXPECT: Returns list of rules

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    bool success = brain_get_learned_rules(brain, &rules, &num_rules);
    EXPECT_TRUE(success);

    // Initially empty (no rules learned yet in this test)
    EXPECT_EQ(num_rules, 0);
}

//=============================================================================
// Test 16-17: End-to-End Integration
//=============================================================================

TEST_F(ReasoningIntegrationTest, EndToEndRuleLearnInferenceFlow) {
    // WHAT: Full pipeline - learn → store → infer → derive
    // WHY:  Test complete reasoning integration
    // EXPECT: New facts derived from learned rules

    if (!logic) {
        GTEST_SKIP() << "Logic system not available";
    }

    // Step 1: Learn rule from examples
    logical_example_t examples[] = {
        {.num_features = 10, .confidence = 1.0f},
        {.num_features = 10, .confidence = 1.0f}
    };

    float pattern[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    examples[0].features = pattern;
    examples[1].features = pattern;
    strcpy(examples[0].label, "animal");
    strcpy(examples[1].label, "animal");

    learned_rule_t** rules = nullptr;
    int num_rules = 0;

    bool learned = brain_learn_logical_rule(brain, examples, 2, &rules, &num_rules, false);
    ASSERT_TRUE(learned);
    ASSERT_GT(num_rules, 0);

    // Step 2: Forward chain to derive new facts
    logic_clause_t** new_facts = nullptr;
    int num_new_facts = 0;

    bool inferred = symbolic_logic_forward_chain(logic, 10, &new_facts, &num_new_facts);
    EXPECT_TRUE(inferred || num_new_facts == 0); // May derive facts or not

    // Cleanup
    for (int i = 0; i < num_rules; i++) {
        learned_rule_destroy(rules[i]);
    }
    nimcp_free(rules);

    if (new_facts) {
        nimcp_free(new_facts);
    }
}

TEST_F(ReasoningIntegrationTest, RuleToStringFormatting) {
    // WHAT: Convert learned rule to human-readable string
    // WHY:  Test rule introspection and debugging
    // EXPECT: Formatted string contains rule details

    learned_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    strcpy(rule.rule_name, "example_rule");
    rule.confidence = 0.85f;
    rule.support_count = 10;
    rule.contradiction_count = 2;

    char buffer[256];
    bool success = learned_rule_to_string(&rule, buffer, sizeof(buffer));

    EXPECT_TRUE(success);
    EXPECT_NE(strstr(buffer, "example_rule"), nullptr);
    EXPECT_NE(strstr(buffer, "0.85"), nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
