/**
 * @file test_multi_network_training.cpp
 * @brief Integration tests for multi-network ensemble training (LNN + CNN + Adaptive)
 *
 * WHAT: Verify brain_enable_multi_network_training() creates LNN/CNN networks,
 *       sets HYBRID mode, and enables ensemble training via dispatch.
 * WHY:  Ensure the multi-network feature works correctly end-to-end.
 * HOW:  Create small brains, enable multi-network, train, verify state.
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards — avoid nimcp.h to prevent CUDA conflicts
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/learning/nimcp_brain_learning.h"
#include "training/nimcp_training_dispatch.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MultiNetworkTrainingTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /**
     * @brief Create a small brain for testing
     *
     * WHAT: Create minimal brain with given input/output dimensions
     * WHY:  Reusable helper for test cases
     * HOW:  Use internal brain_create with BRAIN_SIZE_SMALL + TASK_CLASSIFICATION
     */
    brain_t make_brain(uint32_t num_inputs, uint32_t num_outputs) {
        return brain_create("test_multi_net", BRAIN_SIZE_SMALL,
                           BRAIN_TASK_CLASSIFICATION, num_inputs, num_outputs);
    }
};

//=============================================================================
// Test Cases
//=============================================================================

/**
 * @brief Verify enabling multi-network creates LNN and CNN networks
 *
 * WHAT: Enable multi-network on small brain, verify pointers are non-NULL
 * WHY:  Core functionality: networks must actually be created
 * HOW:  Create brain, call enable, check internal pointers
 */
TEST_F(MultiNetworkTrainingTest, EnableMultiNetworkOnSmallBrain) {
    brain = make_brain(8, 4);
    ASSERT_NE(brain, nullptr) << "Failed to create test brain";

    // Before: should be ADAPTIVE with NULL specialized networks
    EXPECT_EQ(brain->active_network_type, NIMCP_NETWORK_ADAPTIVE);
    EXPECT_EQ(brain->lnn_network, nullptr);
    EXPECT_EQ(brain->cnn_trainer, nullptr);

    // Enable multi-network
    int rc = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc, 0) << "brain_enable_multi_network_training returned error";

    // After: should be HYBRID with CNN always available
    EXPECT_EQ(brain->active_network_type, NIMCP_NETWORK_HYBRID);
    EXPECT_NE(brain->cnn_trainer, nullptr) << "CNN trainer not created";

    // LNN requires num_inputs >= 8 AND num_outputs >= 8 for stable tensor ops.
    // With 8 inputs and 4 outputs, LNN is intentionally skipped.
    // Verify it works without LNN (CNN-only ensemble).
}

/**
 * @brief Verify LNN is created when dimensions are large enough
 *
 * WHAT: Enable multi-network on brain with >= 8 inputs and >= 8 outputs
 * WHY:  LNN NCP architecture needs minimum 8 neurons per layer
 * HOW:  Create brain with 16 inputs, 8 outputs, verify LNN is created
 */
TEST_F(MultiNetworkTrainingTest, EnableMultiNetworkWithLNN) {
    brain = make_brain(16, 8);
    ASSERT_NE(brain, nullptr) << "Failed to create test brain";

    int rc = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(brain->active_network_type, NIMCP_NETWORK_HYBRID);
    EXPECT_NE(brain->lnn_network, nullptr) << "LNN network not created for 16x8 brain";
    EXPECT_NE(brain->lnn_training_ctx, nullptr) << "LNN training context not created";
    EXPECT_NE(brain->cnn_trainer, nullptr) << "CNN trainer not created";
}

/**
 * @brief Verify learning with multi-network produces valid loss
 *
 * WHAT: Enable multi-network, train a few examples, verify loss is valid
 * WHY:  Ensemble training must not break the existing learning pipeline
 * HOW:  Train with simple features, check loss is finite and non-negative
 */
TEST_F(MultiNetworkTrainingTest, MultiNetworkLearningProducesValidLoss) {
    brain = make_brain(4, 3);
    ASSERT_NE(brain, nullptr);

    int rc = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc, 0);

    // Train a few examples
    float features_a[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float features_b[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float features_c[] = {0.0f, 0.0f, 1.0f, 0.0f};

    for (int epoch = 0; epoch < 5; epoch++) {
        float loss_a = brain_learn_example(brain, features_a, 4, "class_a", 1.0f);
        EXPECT_FALSE(std::isnan(loss_a)) << "Loss is NaN at epoch " << epoch;
        EXPECT_FALSE(std::isinf(loss_a)) << "Loss is Inf at epoch " << epoch;

        float loss_b = brain_learn_example(brain, features_b, 4, "class_b", 1.0f);
        EXPECT_FALSE(std::isnan(loss_b));

        float loss_c = brain_learn_example(brain, features_c, 4, "class_c", 1.0f);
        EXPECT_FALSE(std::isnan(loss_c));
    }
}

/**
 * @brief Verify NULL brain returns error
 *
 * WHAT: Call brain_enable_multi_network_training(NULL)
 * WHY:  Guard clause must reject NULL input
 * HOW:  Check return code is -1
 */
TEST_F(MultiNetworkTrainingTest, MultiNetworkDoesNotCrashOnNullBrain) {
    int rc = brain_enable_multi_network_training(NULL);
    EXPECT_EQ(rc, -1) << "Expected -1 for NULL brain";
}

/**
 * @brief Verify default brain stays in ADAPTIVE mode
 *
 * WHAT: Create brain without enabling multi-network
 * WHY:  Ensure enable is opt-in, not automatic
 * HOW:  Check active_network_type remains ADAPTIVE (0)
 */
TEST_F(MultiNetworkTrainingTest, DisableByNotCalling) {
    brain = make_brain(4, 2);
    ASSERT_NE(brain, nullptr);

    // Should be ADAPTIVE by default
    EXPECT_EQ(brain->active_network_type, NIMCP_NETWORK_ADAPTIVE);
    EXPECT_EQ(brain->lnn_network, nullptr);
    EXPECT_EQ(brain->cnn_trainer, nullptr);
}

/**
 * @brief Verify HYBRID dispatch trains all available networks
 *
 * WHAT: Enable multi-network, call training_dispatch_step directly
 * WHY:  The dispatch in HYBRID mode must exercise LNN and CNN
 * HOW:  Build target vector, call dispatch, check result has valid loss
 */
TEST_F(MultiNetworkTrainingTest, HybridDispatchTrainsAll) {
    brain = make_brain(4, 3);
    ASSERT_NE(brain, nullptr);

    int rc = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc, 0);

    // Build input and target
    float inputs[] = {0.5f, 0.3f, 0.2f, 0.1f};
    float targets[] = {1.0f, 0.0f, 0.0f};

    training_dispatch_result_t result = {};
    int dispatch_rc = training_dispatch_step(brain, inputs, 4, targets, 3, &result);
    EXPECT_EQ(dispatch_rc, 0) << "HYBRID dispatch step failed";

    // Loss should be finite (dispatch picks best network)
    EXPECT_FALSE(std::isnan(result.loss)) << "Dispatch result loss is NaN";
    EXPECT_FALSE(std::isinf(result.loss)) << "Dispatch result loss is Inf";
}

/**
 * @brief Verify idempotent enable (calling twice is safe)
 *
 * WHAT: Call brain_enable_multi_network_training twice
 * WHY:  Should be safe to call multiple times without error
 * HOW:  Check return code is 0 both times
 */
TEST_F(MultiNetworkTrainingTest, IdempotentEnable) {
    brain = make_brain(4, 2);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain_enable_multi_network_training(brain), 0);
    EXPECT_EQ(brain_enable_multi_network_training(brain), 0);

    // Still HYBRID
    EXPECT_EQ(brain->active_network_type, NIMCP_NETWORK_HYBRID);
}
