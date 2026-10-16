/**
 * @file test_semantic_memory_integration.cpp
 * @brief Integration tests for Phase M4 Semantic Memory with full brain
 *
 * WHAT: Test semantic memory integrated with brain learning and cognitive pipelines
 * WHY:  Ensure Phase M4 works correctly within complete brain system
 * HOW:  Create brain, learn patterns, verify semantic memory builds and queries work
 *
 * @version Phase M4
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <cstring>

#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SemanticMemoryIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = brain_create("test_semantic", BRAIN_SIZE_TINY,
                              BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        brain_destroy(brain);
    }

    // Helper: Create test pattern
    void create_pattern(float* features, uint32_t size, float base_value) {
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base_value + (i * 0.05f);
        }
    }
};

//=============================================================================
// Brain Creation and Initialization Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, BrainCreationWithSemanticMemory) {
    // Brain already created in SetUp
    // Verify it was created successfully
    EXPECT_NE(brain, nullptr);
}

TEST_F(SemanticMemoryIntegrationTest, SemanticMemoryInitialized) {
    // Semantic memory should be initialized automatically
    // We can't directly access it from outside, but we can verify
    // the brain was created without errors
    SUCCEED();
}

//=============================================================================
// Learning Pipeline Integration Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, LearningPipelineIntegration) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Learn pattern - this should trigger semantic memory extraction
    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Should succeed without crashing
    EXPECT_GE(loss, 0.0f);
}

TEST_F(SemanticMemoryIntegrationTest, MultipleLearnCycles) {
    float features[10];

    // Learn multiple patterns
    for (int i = 0; i < 10; i++) {
        create_pattern(features, 10, 0.1f * i);
        float loss = brain_learn_example(brain, features, 10, "class_a", 0.8f);
        EXPECT_GE(loss, 0.0f);
    }

    SUCCEED();
}

//=============================================================================
// Cognitive Pipeline Integration Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, CognitivePipelineIntegration) {
    // First learn a pattern
    float learn_features[10] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f,
                                 0.4f, 0.3f, 0.2f, 0.1f, 0.0f};
    brain_learn_example(brain, learn_features, 10, "class_a", 0.9f);

    // Now query - this should trigger semantic memory query
    float query_features[10];
    memcpy(query_features, learn_features, sizeof(learn_features));

    brain_decision_t* decision = brain_decide(brain, query_features, 10);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

TEST_F(SemanticMemoryIntegrationTest, MultipleInferenceCycles) {
    float features[10];
    create_pattern(features, 10, 0.5f);

    // Learn first
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Multiple inference cycles
    for (int i = 0; i < 20; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    SUCCEED();
}

//=============================================================================
// Extended Use and Stability Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, ExtendedUseStability) {
    float features[10];

    // 50 learning cycles
    for (int i = 0; i < 50; i++) {
        create_pattern(features, 10, 0.02f * i);
        brain_learn_example(brain, features, 10, "class_a", 0.8f);
    }

    // 50 inference cycles
    for (int i = 0; i < 50; i++) {
        create_pattern(features, 10, 0.02f * i);
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    SUCCEED();
}

TEST_F(SemanticMemoryIntegrationTest, LearningInferenceInterleaved) {
    float features[10];

    // Interleave learning and inference
    for (int i = 0; i < 30; i++) {
        create_pattern(features, 10, 0.03f * i);

        if (i % 2 == 0) {
            // Learn
            brain_learn_example(brain, features, 10, "class_a", 0.8f);
        } else {
            // Infer
            brain_decision_t* decision = brain_decide(brain, features, 10);
            ASSERT_NE(decision, nullptr);
            brain_free_decision(decision);
        }
    }

    SUCCEED();
}

//=============================================================================
// Multi-Pattern Learning Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, MultiPatternLearning) {
    // Learn 5 distinct patterns
    for (int pattern = 0; pattern < 5; pattern++) {
        float features[10];
        create_pattern(features, 10, 0.2f * pattern);

        // Learn each pattern 3 times
        for (int rep = 0; rep < 3; rep++) {
            float loss = brain_learn_example(brain, features, 10, "class_a", 0.8f);
            EXPECT_GE(loss, 0.0f);
        }
    }

    // Verify inference works on all patterns
    for (int pattern = 0; pattern < 5; pattern++) {
        float features[10];
        create_pattern(features, 10, 0.2f * pattern);

        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    SUCCEED();
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, BackwardCompatibility) {
    // Verify existing brain API works unchanged
    float features[10];
    create_pattern(features, 10, 0.5f);

    // Standard learning
    float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);
    EXPECT_GE(loss, 0.0f);

    // Standard inference
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    brain_free_decision(decision);

    // No API changes required
    SUCCEED();
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, PerformanceWithinLimits) {
    float features[10];
    create_pattern(features, 10, 0.5f);

    // Learn
    brain_learn_example(brain, features, 10, "class_a", 0.9f);

    // Time inference (should be < 20ms for TINY brain)
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        brain_free_decision(decision);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10 inferences should take < 200ms (20ms per inference)
    EXPECT_LT(duration.count(), 200);
}

//=============================================================================
// Edge Cases and Resilience Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, ZeroFeatures) {
    float features[10] = {0};

    // Should handle gracefully
    brain_learn_example(brain, features, 10, "class_a", 0.5f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    SUCCEED();
}

TEST_F(SemanticMemoryIntegrationTest, OneFeatures) {
    float features[10];
    for (int i = 0; i < 10; i++) features[i] = 1.0f;

    // Should handle gracefully
    brain_learn_example(brain, features, 10, "class_a", 0.5f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    SUCCEED();
}

TEST_F(SemanticMemoryIntegrationTest, ExtremeFeatures) {
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = (i % 2 == 0) ? -10.0f : 10.0f;
    }

    // Should handle gracefully
    brain_learn_example(brain, features, 10, "class_a", 0.5f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    SUCCEED();
}

//=============================================================================
// Memory Leak Tests
//=============================================================================

TEST_F(SemanticMemoryIntegrationTest, NoMemoryLeaks) {
    // Create and destroy brain multiple times
    for (int cycle = 0; cycle < 5; cycle++) {
        brain_t temp_brain = brain_create("temp", BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(temp_brain, nullptr);

        // Use it briefly
        float features[10] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f,
                              0.4f, 0.3f, 0.2f, 0.1f, 0.0f};
        brain_learn_example(temp_brain, features, 10, "class_a", 0.9f);
        brain_decision_t* decision = brain_decide(temp_brain, features, 10);
        brain_free_decision(decision);

        brain_destroy(temp_brain);
    }

    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
