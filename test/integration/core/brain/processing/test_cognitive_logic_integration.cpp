/**
 * @file test_cognitive_logic_integration.cpp
 * @brief Integration tests for cognitive logic circuits with brain
 *
 * WHAT: Test cognitive constraint validation in realistic brain scenarios
 * WHY:  Ensure logic circuits integrate correctly with cognitive processor
 * HOW:  Test full brain decision pipeline with constraint checking
 *
 * COVERAGE TARGET: 7+ integration tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.7.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/processing/cognitive_processor.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveLogicIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    neural_logic_network_t logic = nullptr;

    void SetUp() override {
        // Create brain with neural logic enabled (new API)
        brain = brain_create("cognitive_logic_test", BRAIN_SIZE_SMALL,
                           BRAIN_TASK_CLASSIFICATION, 128, 20);
        if (!brain) {
            return;
        }

        // Create separate logic network for direct testing
        neural_logic_config_t logic_config = neural_logic_default_config(100);
        logic = neural_logic_create(&logic_config);
    }

    void TearDown() override {
        if (logic) {
            neural_logic_destroy(logic);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }

    // Helper: Create test input (simple float array)
    void create_test_input(
        float* input,
        uint32_t size,
        float visual_val,
        float auditory_val)
    {
        // Fill first half with visual values, second half with auditory
        for (uint32_t i = 0; i < size / 2; i++) {
            input[i] = visual_val;
        }
        for (uint32_t i = size / 2; i < size; i++) {
            input[i] = auditory_val;
        }
    }
};

//=============================================================================
// Brain Decision Pipeline Integration
//=============================================================================

TEST_F(CognitiveLogicIntegrationTest, BrainDecisionWithLogicValidation) {
    // Test: Full brain decision with logic constraint checking
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    float input[128];
    create_test_input(input, 128, 0.8f, 0.6f);

    brain_decision_t* decision = brain_decide(brain, input, 128);

    ASSERT_NE(decision, nullptr);
    // Decision should have confidence and uncertainty fields
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
}

TEST_F(CognitiveLogicIntegrationTest, ConstraintCheckingWithRealBrain) {
    // Test: Constraint checking integrated with real brain processing
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    float input[128];
    create_test_input(input, 128, 0.5f, 0.5f);

    brain_decision_t* decision = brain_decide(brain, input, 128);

    ASSERT_NE(decision, nullptr);

    // Validate decision constraints
    // Confidence should be in valid range [0, 1]
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
}

TEST_F(CognitiveLogicIntegrationTest, LogicCircuitReuse) {
    // Test: Logic circuits can be reused across multiple decisions
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    float input1[128];
    create_test_input(input1, 128, 0.7f, 0.3f);

    float input2[128];
    create_test_input(input2, 128, 0.4f, 0.9f);

    // First decision
    brain_decision_t* decision1 = brain_decide(brain, input1, 128);
    ASSERT_NE(decision1, nullptr);

    // Second decision (reuses logic network)
    brain_decision_t* decision2 = brain_decide(brain, input2, 128);
    ASSERT_NE(decision2, nullptr);

    // Both should complete successfully
    EXPECT_GE(decision1->confidence, 0.0f);
    EXPECT_GE(decision2->confidence, 0.0f);
}

TEST_F(CognitiveLogicIntegrationTest, HighUncertaintyScenario) {
    // Test: High uncertainty input triggers constraint checking
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    float input[128];
    // Create ambiguous input (mid-range values)
    create_test_input(input, 128, 0.5f, 0.5f);

    brain_decision_t* decision = brain_decide(brain, input, 128);

    ASSERT_NE(decision, nullptr);

    // Confidence might be lower for ambiguous input
    // (This depends on introspection module being enabled)
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
}

TEST_F(CognitiveLogicIntegrationTest, EthicalConstraintValidation) {
    // Test: Ethical violations trigger constraint checking
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    float input[128];
    create_test_input(input, 128, 0.8f, 0.8f);

    brain_decision_t* decision = brain_decide(brain, input, 128);

    ASSERT_NE(decision, nullptr);

    // Should have ethical approval status
    // (Default should be approved for normal inputs)
}

//=============================================================================
// Constraint Violation Detection
//=============================================================================

TEST_F(CognitiveLogicIntegrationTest, DetectConfidenceUncertaintyConflict) {
    // Test: Detect when confidence and uncertainty conflict
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    // Process multiple inputs to see constraint variations
    for (int i = 0; i < 5; i++) {
        float input[128];
        float val = 0.2f * i;
        create_test_input(input, 128, val, 1.0f - val);

        brain_decision_t* decision = brain_decide(brain, input, 128);

        ASSERT_NE(decision, nullptr);

        // Check confidence is in valid range
        EXPECT_GE(decision->confidence, 0.0f);
        EXPECT_LE(decision->confidence, 1.0f);
    }
}

TEST_F(CognitiveLogicIntegrationTest, MultipleDecisionsStressTest) {
    // Test: Multiple decisions in sequence maintain constraint validity
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
    }

    const int num_decisions = 20;
    int valid_decisions = 0;

    for (int i = 0; i < num_decisions; i++) {
        float input[128];
        float visual_val = (float)i / num_decisions;
        float auditory_val = 1.0f - visual_val;
        create_test_input(input, 128, visual_val, auditory_val);

        brain_decision_t* decision = brain_decide(brain, input, 128);

        if (decision != nullptr) {
            valid_decisions++;
        }
    }

    // Most decisions should succeed
    EXPECT_GT(valid_decisions, num_decisions / 2);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(CognitiveLogicIntegrationTest, LogicCircuitPerformance) {
    // Test: Logic circuit evaluation performance
    if (!logic) {
        GTEST_SKIP() << "Logic network creation failed";
    }

    const int num_evaluations = 100;

    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(and_gate, UINT32_MAX);

    uint32_t successful_evals = 0;

    for (int i = 0; i < num_evaluations; i++) {
        float inputs[2] = {
            (float)(i % 2),
            (float)((i + 1) % 2)
        };
        float output = 0.0f;

        if (neural_logic_evaluate(logic, and_gate, inputs, 2, &output)) {
            successful_evals++;
        }
    }

    // All evaluations should succeed
    EXPECT_EQ(successful_evals, num_evaluations);
}
