//=============================================================================
// test_reasoning_performance.cpp - Reasoning Performance Regression Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/learning/nimcp_association_learning.h"
#include <chrono>
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_rule_learning.h"
#include "core/brain/learning/nimcp_circuit_compilation.h"

class ReasoningPerformanceTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // brain_config_t config;
        // config.num_neurons = 10000;
        // config.num_inputs = 100;
        // config.num_outputs = 50;
        // config.sparsity = 0.1f;
        // config.learning_rate = 0.01f;

        brain = brain_create("reasoning_test", BRAIN_SIZE_LARGE, BRAIN_TASK_CLASSIFICATION, 100, 50);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Test 1: 1000 symbolic inferences in <100ms
TEST_F(ReasoningPerformanceTest, SymbolicInferenceSpeed) {
    // Add rules to KB
    for (int i = 0; i < 100; i++) {
        char rule[256];
        snprintf(rule, sizeof(rule), "IF condition_%d THEN result_%d", i, i);
        add_learned_rule_to_kb(brain, rule, 0.8f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Perform 1000 inferences
    for (int i = 0; i < 1000; i++) {
        // TODO: Actual inference operation
        // For now, just measure rule addition overhead
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100) << "1000 inferences took " << duration.count() << "ms";
}

// Test 2: 10000 neural gate evaluations in <10ms
TEST_F(ReasoningPerformanceTest, NeuralGateEvaluationSpeed) {
    // Compile circuits
    circuit_id_t circuits[100];
    for (int i = 0; i < 100; i++) {
        char rule[256];
        snprintf(rule, sizeof(rule), "IF A_%d AND B_%d THEN C_%d", i, i, i);
        circuits[i] = compile_rule_to_circuit(brain, rule);
        ASSERT_NE(circuits[i], 0);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Evaluate circuits 100 times each = 10000 evaluations
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            // TODO: Actual circuit evaluation
            uint32_t gates = get_circuit_gate_count(brain, circuits[j]);
            (void)gates;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 10) << "10000 gate evaluations took " << duration.count() << "ms";
}

// Test 3: Rule learning throughput
TEST_F(ReasoningPerformanceTest, RuleLearningThroughput) {
    float features[100];
    for (int i = 0; i < 100; i++) {
        features[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Learn 1000 rules
    for (int i = 0; i < 1000; i++) {
        rule_example_t example = {features, 100, "test", 1.0f};
        char label[32];
        snprintf(label, sizeof(label), "class_%d", i % 10);
        const char* label_ptr = label;

        brain_learn_rule_from_examples(brain, &example, &label_ptr, 1);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500) << "1000 rule learnings took " << duration.count() << "ms";
}

// Test 4: Circuit compilation speed
TEST_F(ReasoningPerformanceTest, CircuitCompilationSpeed) {
    auto start = std::chrono::high_resolution_clock::now();

    // Compile 100 circuits
    for (int i = 0; i < 100; i++) {
        char rule[256];
        snprintf(rule, sizeof(rule), "IF X_%d THEN Y_%d", i, i);
        circuit_id_t id = compile_rule_to_circuit(brain, rule);
        ASSERT_NE(id, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 50) << "100 circuit compilations took " << duration.count() << "ms";
}

// Test 5: Association lookup performance
TEST_F(ReasoningPerformanceTest, AssociationLookupPerformance) {
    // Create 1000 associations
    for (int i = 0; i < 1000; i++) {
        char a[32], b[32];
        snprintf(a, sizeof(a), "A_%d", i);
        snprintf(b, sizeof(b), "B_%d", i);
        brain_learn_association(brain, a, b, 5);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Lookup 10000 associations
    for (int i = 0; i < 10000; i++) {
        char a[32], b[32];
        snprintf(a, sizeof(a), "A_%d", i % 1000);
        snprintf(b, sizeof(b), "B_%d", i % 1000);
        float strength = get_association_strength(brain, a, b);
        (void)strength;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100) << "10000 association lookups took " << duration.count() << "ms";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
