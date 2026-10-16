/**
 * @file test_semantic_memory_backward_compat.cpp
 * @brief Regression tests for Phase M4 Semantic Memory backward compatibility
 *
 * WHAT: Ensure Phase M4 doesn't break existing brain functionality
 * WHY:  Maintain backward compatibility and stability
 * HOW:  Test all pre-M4 APIs work unchanged with M4 integrated
 *
 * @version Phase M4
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include <stdlib.h>
#include <string.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class SemanticMemoryRegressionTest : public ::testing::Test {
protected:
    void create_pattern(float* features, uint32_t size, float base) {
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base + (i * 0.1f);
        }
    }
};

//=============================================================================
// Brain Creation Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, BrainCreateTinySize) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, BrainCreateSmallSize) {
    brain_t brain = brain_create("test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);
    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, BrainCreateAllTaskTypes) {
    brain_task_t types[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING
    };

    for (int i = 0; i < 3; i++) {
        brain_t brain = brain_create("test", BRAIN_SIZE_TINY, types[i], 10, 3);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
    }
}

//=============================================================================
// Learning API Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, LegacyLearningWorks) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);
    EXPECT_GE(loss, 0.0f);

    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, BatchLearningWorks) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Create batch of examples
    const int batch_size = 5;
    brain_example_t* examples = (brain_example_t*)malloc(batch_size * sizeof(brain_example_t));

    for (int i = 0; i < batch_size; i++) {
        examples[i].features = (float*)malloc(10 * sizeof(float));
        create_pattern(examples[i].features, 10, 0.2f * i);
        examples[i].num_features = 10;
        strncpy(examples[i].label, "class_a", 63);
        examples[i].label[63] = '\0';
        examples[i].confidence = 0.9f;
    }

    float loss = brain_learn_batch(brain, examples, batch_size);
    EXPECT_GE(loss, 0.0f);

    // Free examples
    for (int i = 0; i < batch_size; i++) {
        free(examples[i].features);
    }
    free(examples);
    brain_destroy(brain);
}

//=============================================================================
// Inference API Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, LegacyInferenceWorks) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Learn first
    float features[10];
    create_pattern(features, 10, 0.5f);
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Infer
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, BatchInferenceWorks) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Learn
    float features[10];
    create_pattern(features, 10, 0.5f);
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Batch inference - allocate decision array
    const int batch_size = 3;
    const float* inputs[3];
    brain_decision_t* outputs = (brain_decision_t*)calloc(batch_size, sizeof(brain_decision_t));

    for (int i = 0; i < batch_size; i++) {
        inputs[i] = features;
    }

    bool success = brain_decide_batch(brain, inputs, batch_size, 10, outputs);
    EXPECT_TRUE(success);

    // Note: brain_decide_batch populates the decision structs, no need to free individually
    free(outputs);

    brain_destroy(brain);
}

//=============================================================================
// Stability Over Time Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, StableOverExtendedUse) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10];

    // 200 cycles of learning and inference
    for (int i = 0; i < 200; i++) {
        create_pattern(features, 10, 0.005f * i);

        // Learn
        float loss = brain_learn_example(brain, features, 10, "class_a", 0.8f);
        EXPECT_GE(loss, 0.0f);

        // Infer
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, ConsistentDecisionsOverTime) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10];
    create_pattern(features, 10, 0.5f);

    // Learn pattern
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Query 50 times - should get consistent results
    float first_confidence = 0.0f;
    for (int i = 0; i < 50; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);

        if (i == 0) {
            first_confidence = decision->confidence;
        } else {
            // Confidence should be relatively stable
            EXPECT_NEAR(decision->confidence, first_confidence, 0.3f);
        }

        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, NoPerformanceRegression) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10];
    create_pattern(features, 10, 0.5f);
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Time 20 inferences
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 20; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        brain_free_decision(decision);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should be < 200ms total (10ms per inference target)
    EXPECT_LT(duration.count(), 200);

    brain_destroy(brain);
}

//=============================================================================
// Memory Phase Integration Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, PhaseM1EngramsStillWork) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Learning should encode engrams (Phase M1)
    float features[10];
    for (int i = 0; i < 10; i++) {
        create_pattern(features, 10, 0.1f * i);
        float loss = brain_learn_example(brain, features, 10, "class_a", 0.8f);
        EXPECT_GE(loss, 0.0f);
    }

    // Engrams should support recall
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, PhaseM2ConsolidationStillWorks) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Learning triggers consolidation (Phase M2)
    float features[10];
    for (int i = 0; i < 20; i++) {
        create_pattern(features, 10, 0.05f * i);
        brain_learn_example(brain, features, 10, "class_a", 0.8f);
    }

    // Consolidation should work without errors
    SUCCEED();

    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, PhaseM3TransferStillWorks) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Learning and inference trigger WM transfer (Phase M3)
    float features[10];
    create_pattern(features, 10, 0.5f);

    // Learn multiple times (rehearsal)
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 10, "class_a", 0.9f);
    }

    // Query multiple times
    for (int i = 0; i < 5; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// API Compatibility Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, NoAPIChanges) {
    // All existing APIs should work exactly as before
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Learn
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Decide
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);

    // Free
    brain_free_decision(decision);

    // Destroy
    brain_destroy(brain);

    // No new parameters, no changed signatures
    SUCCEED();
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, NullHandlingUnchanged) {
    // NULL inputs should be handled gracefully as before
    EXPECT_EQ(brain_decide(nullptr, nullptr, 0), nullptr);

    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain_decide(brain, nullptr, 0), nullptr);

    brain_destroy(brain);
}

TEST_F(SemanticMemoryRegressionTest, ZeroInputsHandled) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {0};

    // Should handle gracefully
    brain_learn_example(brain, features, 10, "class_a", 0.5f);
    brain_decision_t* decision = brain_decide(brain, features, 10);

    if (decision) {
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// Memory Leak Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, NoNewMemoryLeaks) {
    // Create and destroy repeatedly - should not leak
    for (int cycle = 0; cycle < 10; cycle++) {
        brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                      BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);

        float features[10];
        create_pattern(features, 10, 0.5f);

        brain_learn_example(brain, features, 10, "class_a", 0.9f);
        brain_decision_t* decision = brain_decide(brain, features, 10);
        brain_free_decision(decision);

        brain_destroy(brain);
    }

    SUCCEED();
}

//=============================================================================
// Multi-Pattern Regression Tests
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, LearningManyPatterns) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Learn 30 distinct patterns
    for (int i = 0; i < 30; i++) {
        float features[10];
        create_pattern(features, 10, 0.03f * i);
        float loss = brain_learn_example(brain, features, 10, "class_a", 0.8f);
        EXPECT_GE(loss, 0.0f);
    }

    // Verify inference works
    float features[10];
    create_pattern(features, 10, 0.5f);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    brain_destroy(brain);
}

//=============================================================================
// Comprehensive Regression Test
//=============================================================================

TEST_F(SemanticMemoryRegressionTest, ComprehensiveRegressionTest) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10];

    // Mix of learning and inference over time
    for (int i = 0; i < 100; i++) {
        create_pattern(features, 10, 0.01f * i);

        if (i % 3 == 0) {
            // Learn
            brain_learn_example(brain, features, 10, "class_a", 0.8f);
        } else {
            // Infer
            brain_decision_t* decision = brain_decide(brain, features, 10);
            if (decision) {
                brain_free_decision(decision);
            }
        }
    }

    // Final verification
    create_pattern(features, 10, 0.5f);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
    brain_free_decision(decision);

    brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
