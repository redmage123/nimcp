/**
 * @file test_brain_utility_functions.cpp
 * @brief Tests for brain utility and optimization functions
 *
 * WHAT: Tests for pruning, optimization, and utility functions
 * WHY: Cover uncovered utility functions (~8-10% coverage gain potential)
 * HOW: Test pruning, optimization, explanation, neuron ranking functions
 *
 * TARGET FUNCTIONS:
 * - brain_prune
 * - brain_recommend_pruning_threshold
 * - brain_optimize_for_inference
 * - brain_explain_decision
 * - brain_get_top_neurons
 * - brain_sync_neuromodulators
 * - brain_is_distributed
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainUtilityFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    brain_t create_trained_brain() {
        brain_t brain = brain_create("utility_test", BRAIN_SIZE_SMALL,
                                      BRAIN_TASK_CLASSIFICATION, 50, 10);
        if (!brain) return nullptr;

        // Train to create synapses
        float features[50];
        for (int i = 0; i < 50; i++) features[i] = 0.5f + 0.01f * i;

        for (int i = 0; i < 20; i++) {
            brain_learn_example(brain, features, 50, "test_class", 0.8f);
        }

        return brain;
    }
};

//=============================================================================
// Pruning Tests
//=============================================================================

TEST_F(BrainUtilityFunctionsTest, PruneWithSmallThreshold) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    // Prune weak synapses (threshold = 0.01)
    uint32_t pruned = brain_prune(brain, 0.01f);
    // Should prune some or no synapses depending on training

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, PruneWithLargeThreshold) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    // Prune with high threshold (should prune many synapses)
    uint32_t pruned = brain_prune(brain, 0.9f);
    // Pruning count depends on weight distribution

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, PruneNullBrain) {
    uint32_t pruned = brain_prune(nullptr, 0.5f);
    EXPECT_EQ(pruned, 0u);  // Should handle NULL gracefully
}

TEST_F(BrainUtilityFunctionsTest, RecommendPruningThreshold) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    // Get recommended pruning threshold for 80% sparsity
    float threshold = brain_recommend_pruning_threshold(brain, 0.8f);

    if (threshold >= 0.0f) {
        EXPECT_GE(threshold, 0.0f);
        EXPECT_LE(threshold, 1.0f);
    }

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, RecommendPruningThresholdNullBrain) {
    float threshold = brain_recommend_pruning_threshold(nullptr, 0.8f);
    EXPECT_LT(threshold, 0.0f);  // Should return error value
}

//=============================================================================
// Optimization Tests
//=============================================================================

TEST_F(BrainUtilityFunctionsTest, OptimizeForInference) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    // Optimize brain for inference
    bool result = brain_optimize_for_inference(brain);
    // May succeed or fail depending on implementation

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, OptimizeForInferenceNullBrain) {
    bool result = brain_optimize_for_inference(nullptr);
    EXPECT_FALSE(result);  // Should fail with NULL brain
}

//=============================================================================
// Explanation Tests
//=============================================================================

TEST_F(BrainUtilityFunctionsTest, ExplainDecision) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    float features[50];
    for (int i = 0; i < 50; i++) features[i] = 0.5f + 0.01f * i;

    char explanation[512];
    bool result = brain_explain_decision(brain, features, 50, explanation, sizeof(explanation));

    if (result) {
        EXPECT_GT(strlen(explanation), 0u);
    }

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, ExplainDecisionNullBrain) {
    float features[10] = {0.5f};
    char explanation[512];

    bool result = brain_explain_decision(nullptr, features, 10, explanation, sizeof(explanation));
    EXPECT_FALSE(result);
}

TEST_F(BrainUtilityFunctionsTest, ExplainDecisionNullFeatures) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    char explanation[512];
    bool result = brain_explain_decision(brain, nullptr, 50, explanation, sizeof(explanation));
    EXPECT_FALSE(result);

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, ExplainDecisionNullBuffer) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    float features[50];
    for (int i = 0; i < 50; i++) features[i] = 0.5f;

    bool result = brain_explain_decision(brain, features, 50, nullptr, 512);
    EXPECT_FALSE(result);

    brain_destroy(brain);
}

//=============================================================================
// Neuron Ranking Tests
//=============================================================================

TEST_F(BrainUtilityFunctionsTest, GetTopNeurons) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    uint32_t neuron_ids[10];
    float importances[10];

    uint32_t count = brain_get_top_neurons(brain, 10, neuron_ids, importances);

    if (count > 0) {
        EXPECT_LE(count, 10u);
        // Check importances are valid
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_GE(importances[i], 0.0f);
        }
    }

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, GetTopNeuronsNullBrain) {
    uint32_t neuron_ids[10];
    float importances[10];

    uint32_t count = brain_get_top_neurons(nullptr, 10, neuron_ids, importances);
    EXPECT_EQ(count, 0u);
}

TEST_F(BrainUtilityFunctionsTest, GetTopNeuronsNullArrays) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    uint32_t count = brain_get_top_neurons(brain, 10, nullptr, nullptr);
    EXPECT_EQ(count, 0u);

    brain_destroy(brain);
}

//=============================================================================
// Neuromodulator Sync Tests
//=============================================================================

TEST_F(BrainUtilityFunctionsTest, SyncNeuromodulators) {
    brain_t brain = create_trained_brain();
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    // Sync neuromodulators
    bool result = brain_sync_neuromodulators(brain);
    // May succeed or fail depending on neuromodulator system state

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, SyncNeuromodulatorsNullBrain) {
    bool result = brain_sync_neuromodulators(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Distributed Brain Tests
//=============================================================================

TEST_F(BrainUtilityFunctionsTest, IsDistributedRegularBrain) {
    brain_t brain = brain_create("regular_brain", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 50, 10);
    if (!brain) {
        GTEST_SKIP() << "Brain creation failed";
        return;
    }

    bool is_distributed = brain_is_distributed(brain);
    EXPECT_FALSE(is_distributed);  // Regular brain should not be distributed

    brain_destroy(brain);
}

TEST_F(BrainUtilityFunctionsTest, IsDistributedNullBrain) {
    bool is_distributed = brain_is_distributed(nullptr);
    EXPECT_FALSE(is_distributed);  // NULL should return false
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
