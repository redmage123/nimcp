//=============================================================================
// test_brain_reasoning_api.cpp - Brain Reasoning API Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_rule_learning.h"
#include "core/brain/learning/nimcp_association_learning.h"
#include "core/brain/learning/nimcp_circuit_compilation.h"

class BrainReasoningAPITest : public ::testing::Test {
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

// Test 1: Rule learning API
TEST_F(BrainReasoningAPITest, RuleLearningAPI) {
    float features[] = {1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    rule_example_t example = {features, 10, "test", 1.0f};
    const char* label = "test";

    int result = brain_learn_rule_from_examples(brain, &example, &label, 1);
    EXPECT_GE(result, 0);
}

// Test 2: Association learning API
TEST_F(BrainReasoningAPITest, AssociationLearningAPI) {
    bool success = brain_learn_association(brain, "rain", "umbrella", 10);
    EXPECT_TRUE(success);

    float strength = get_association_strength(brain, "rain", "umbrella");
    EXPECT_GE(strength, 0.0f);
}

// Test 3: Circuit compilation API
TEST_F(BrainReasoningAPITest, CircuitCompilationAPI) {
    circuit_id_t id = compile_rule_to_circuit(brain, "IF A THEN B");
    EXPECT_NE(id, 0);
}

// Test 4: Rule confidence computation
TEST_F(BrainReasoningAPITest, RuleConfidenceComputation) {
    float conf = compute_rule_confidence(8, 10);
    EXPECT_GT(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

// Test 5: Association strength update
TEST_F(BrainReasoningAPITest, AssociationStrengthUpdate) {
    brain_learn_association(brain, "A", "B", 5);

    float strength1 = get_association_strength(brain, "A", "B");
    float strength2 = update_association_strength(brain, "A", "B", 1.0f);

    EXPECT_GT(strength2, strength1);
}

// Test 6: Association decay
TEST_F(BrainReasoningAPITest, AssociationDecay) {
    brain_learn_association(brain, "X", "Y", 10);
    float strength_before = get_association_strength(brain, "X", "Y");

    uint32_t decayed = decay_all_associations(brain, 0.9f);
    float strength_after = get_association_strength(brain, "X", "Y");

    EXPECT_GT(decayed, 0);
    EXPECT_LT(strength_after, strength_before);
}

// Test 7: Rule pattern extraction
TEST_F(BrainReasoningAPITest, RulePatternExtraction) {
    float f1[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float f2[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    rule_example_t examples[] = {
        {f1, 10, "A", 1.0f},
        {f2, 10, "A", 1.0f}
    };

    char rule[512];
    bool found = extract_rule_pattern(examples, 2, "A", rule, sizeof(rule));
    EXPECT_TRUE(found);
}

// Test 8: Circuit gate operations
TEST_F(BrainReasoningAPITest, CircuitGateOperations) {
    circuit_id_t id = compile_rule_to_circuit(brain, "IF A AND B THEN C");
    ASSERT_NE(id, 0);

    uint32_t gates = get_circuit_gate_count(brain, id);
    EXPECT_GT(gates, 0);
}

// Test 9: Batch rule learning
TEST_F(BrainReasoningAPITest, BatchRuleLearning) {
    float f1[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float f2[] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float f3[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    rule_example_t examples[] = {
        {f1, 10, "A", 1.0f},
        {f2, 10, "B", 1.0f},
        {f3, 10, "C", 1.0f}
    };

    const char* labels[] = {"A", "B", "C"};

    int rules = brain_learn_rule_from_examples(brain, examples, labels, 3);
    EXPECT_GE(rules, 0);
}

// Test 10: Association confidence
TEST_F(BrainReasoningAPITest, AssociationConfidence) {
    association_stats_t stats = {8, 10, 6, 20};
    float conf = compute_association_confidence("A", "B", &stats);

    EXPECT_GT(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

// Test 11: Circuit optimization
TEST_F(BrainReasoningAPITest, CircuitOptimizationAPI) {
    circuit_id_t id = compile_rule_to_circuit(brain, "IF A THEN B");
    ASSERT_NE(id, 0);

    bool optimized = optimize_circuit(brain, id);
    EXPECT_TRUE(optimized);
}

// Test 12: Null parameter handling
TEST_F(BrainReasoningAPITest, NullParameterHandling) {
    int r1 = brain_learn_rule_from_examples(nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(r1, -1);

    bool r2 = brain_learn_association(nullptr, nullptr, nullptr, 0);
    EXPECT_FALSE(r2);

    circuit_id_t r3 = compile_rule_to_circuit(nullptr, nullptr);
    EXPECT_EQ(r3, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
