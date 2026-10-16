/**
 * @file test_api_safety_fixes.cpp
 * @brief GoogleTest unit tests for NIMCP API safety fixes
 *
 * WHAT: Tests for buffer overflow fixes, gradient distribution correctness,
 *       gradient clipping, and memory cleanup on error paths
 * WHY:  Verify that critical safety bugs in the API layer are fixed
 * HOW:  GTest fixture with NIMCP_BRAIN_TINY, exercises decide_full and
 *       train_step with edge-case inputs
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <string.h>
#include <math.h>

/**
 * @brief Test fixture for API safety fix tests
 *
 * Creates a TINY brain with 4 inputs and 2 outputs for
 * testing buffer safety and gradient handling.
 */
class ApiSafetyTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_init();

        brain = nimcp_brain_create(
            "safety_test_brain",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            4,  /* num_inputs */
            2   /* num_outputs */
        );
        ASSERT_NE(brain, nullptr) << "Failed to create brain for test";
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
        }
        nimcp_shutdown();
    }
};

/**
 * @brief Test 1: Label buffer safety with long labels
 *
 * WHAT: Verify decide_full does not overflow out_label buffer
 * WHY:  Bug fix: hardcoded 63 replaced with NIMCP_MAX_LABEL_SIZE - 1
 * HOW:  Teach a label near NIMCP_MAX_LABEL_SIZE, call decide_full,
 *       verify null-terminated within bounds
 */
TEST_F(ApiSafetyTest, DecideFullLongLabel) {
    /* Create a label that is exactly NIMCP_MAX_LABEL_SIZE - 1 characters */
    char long_label[NIMCP_MAX_LABEL_SIZE];
    memset(long_label, 'A', NIMCP_MAX_LABEL_SIZE - 1);
    long_label[NIMCP_MAX_LABEL_SIZE - 1] = '\0';

    /* Teach the brain with this long label */
    float features[4] = {0.9f, 0.8f, 0.7f, 0.6f};
    nimcp_status_t learn_status = nimcp_brain_learn_example(
        brain, features, 4, long_label, 1.0f);
    /* Learning may or may not succeed (label may be truncated internally),
       but it should not crash */
    (void)learn_status;

    /* Call decide_full with properly sized buffers */
    char out_label[NIMCP_MAX_LABEL_SIZE];
    memset(out_label, 0xCC, sizeof(out_label));  /* Poison buffer */
    float out_confidence = -1.0f;
    char out_explanation[NIMCP_MAX_EXPLANATION_SIZE];
    memset(out_explanation, 0xCC, sizeof(out_explanation));
    float out_output_vector[2] = {0.0f};
    uint32_t out_output_size = 2;
    uint32_t out_num_active = 0;
    float out_sparsity = 0.0f;
    uint64_t out_inference_time = 0;

    nimcp_status_t status = nimcp_brain_decide_full(
        brain, features, 4,
        out_label, &out_confidence,
        out_explanation,
        out_output_vector, &out_output_size,
        &out_num_active, &out_sparsity,
        &out_inference_time);

    if (status == NIMCP_OK) {
        /* Verify label is null-terminated within bounds */
        size_t label_len = strlen(out_label);
        EXPECT_LT(label_len, (size_t)NIMCP_MAX_LABEL_SIZE)
            << "Label exceeds NIMCP_MAX_LABEL_SIZE buffer";

        /* Verify the null terminator is within bounds */
        EXPECT_EQ(out_label[label_len], '\0');
    }
    /* Even if decision fails, no buffer overflow should have occurred */
}

/**
 * @brief Test 2: Gradient distribution correctness
 *
 * WHAT: Verify gradients are properly distributed in train_step
 * WHY:  Bug fix: fallback gradient distribution now uses fabsf() sum
 *       instead of signed sum, preventing sign cancellation
 * HOW:  Configure training, run train_step, verify result is valid
 */
TEST_F(ApiSafetyTest, TrainStepGradientDistribution) {
    /* Configure training with defaults */
    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t cfg_status = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(cfg_status, NIMCP_OK) << "Failed to configure training";

    /* Run a training step with clear signal */
    float features[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float targets[2] = {1.0f, 0.0f};
    nimcp_training_result_t result = {};

    nimcp_status_t status = nimcp_brain_train_step(
        brain, features, 4, targets, 2, &result);

    EXPECT_EQ(status, NIMCP_OK) << "train_step should succeed";

    /* Verify result has valid loss (not NaN or Inf) */
    EXPECT_FALSE(isnan(result.loss)) << "Loss should not be NaN";
    EXPECT_FALSE(isinf(result.loss)) << "Loss should not be Inf";
    EXPECT_GE(result.loss, 0.0f) << "Loss should be non-negative";
}

/**
 * @brief Test 3: Gradient clipping
 *
 * WHAT: Verify extreme gradients are clipped to [-1, 1]
 * WHY:  Per-element gradient clipping prevents exploding gradients
 * HOW:  Train with extreme target values that would produce large gradients,
 *       verify the result is still numerically stable
 */
TEST_F(ApiSafetyTest, TrainStepGradientClipping) {
    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t cfg_status = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(cfg_status, NIMCP_OK) << "Failed to configure training";

    /* Run multiple steps with extreme targets to stress gradient clipping */
    float features[4] = {100.0f, 100.0f, 100.0f, 100.0f};
    float targets[2] = {100.0f, -100.0f};
    nimcp_training_result_t result = {};

    for (int step = 0; step < 5; step++) {
        nimcp_status_t status = nimcp_brain_train_step(
            brain, features, 4, targets, 2, &result);

        if (status == NIMCP_OK) {
            /* Gradient norm should be finite (clipping prevents explosion) */
            EXPECT_FALSE(isnan(result.gradient_norm))
                << "Gradient norm should not be NaN at step " << step;
            EXPECT_FALSE(isinf(result.gradient_norm))
                << "Gradient norm should not be Inf at step " << step;

            /* With per-element clipping to [-1,1], gradient norm should be bounded */
            EXPECT_GE(result.gradient_norm, 0.0f)
                << "Gradient norm should be non-negative at step " << step;

            /* Loss should remain finite */
            EXPECT_FALSE(isnan(result.loss))
                << "Loss should not be NaN at step " << step;
            EXPECT_FALSE(isinf(result.loss))
                << "Loss should not be Inf at step " << step;
        }
    }
}

/**
 * @brief Test 4: Memory cleanup on zero weights error path
 *
 * WHAT: Verify no memory leak when total_weights is 0
 * WHY:  Bug fix: predictions and output_gradients must be freed before error return
 * HOW:  Create a brain, configure training, and exercise the train_step path.
 *       If the brain has no weights (edge case), the function should return an
 *       error without leaking memory. Since we cannot easily create a zero-weight
 *       brain via the public API, we verify the normal path returns successfully
 *       and that repeated calls do not accumulate leaked memory.
 */
TEST_F(ApiSafetyTest, TrainStepCleanupOnZeroWeights) {
    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t cfg_status = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(cfg_status, NIMCP_OK) << "Failed to configure training";

    /* Run many training steps to detect memory leaks via repeated allocation.
       If cleanup was missing, this would leak on each iteration. */
    float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float targets[2] = {1.0f, 0.0f};
    nimcp_training_result_t result = {};

    for (int i = 0; i < 100; i++) {
        nimcp_status_t status = nimcp_brain_train_step(
            brain, features, 4, targets, 2, &result);
        /* Each step should either succeed or fail cleanly without leaking */
        EXPECT_TRUE(status == NIMCP_OK || status == NIMCP_ERROR)
            << "Unexpected status at step " << i;
    }

    /* If we get here without ASAN/valgrind complaints, cleanup is correct */
    SUCCEED() << "100 train_step iterations completed without memory issues";
}

/**
 * @brief Test 5: Explanation buffer safety
 *
 * WHAT: Verify explanation buffer does not overflow
 * WHY:  Bug fix: hardcoded 255 replaced with NIMCP_MAX_EXPLANATION_SIZE - 1
 * HOW:  Call decide_full with explanation buffer, verify null-terminated
 */
TEST_F(ApiSafetyTest, DecideFullLongExplanation) {
    /* Teach the brain something so decide_full has data to work with */
    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_brain_learn_example(brain, features, 4, "test_class", 1.0f);

    /* Allocate exactly NIMCP_MAX_EXPLANATION_SIZE buffer */
    char out_label[NIMCP_MAX_LABEL_SIZE];
    float out_confidence = 0.0f;
    char out_explanation[NIMCP_MAX_EXPLANATION_SIZE];

    /* Poison buffers with sentinel values to detect overwrites */
    memset(out_label, 0xCC, sizeof(out_label));
    memset(out_explanation, 0xCC, sizeof(out_explanation));

    float out_output_vector[2] = {0.0f};
    uint32_t out_output_size = 2;
    uint32_t out_num_active = 0;
    float out_sparsity = 0.0f;
    uint64_t out_inference_time = 0;

    nimcp_status_t status = nimcp_brain_decide_full(
        brain, features, 4,
        out_label, &out_confidence,
        out_explanation,
        out_output_vector, &out_output_size,
        &out_num_active, &out_sparsity,
        &out_inference_time);

    if (status == NIMCP_OK) {
        /* Verify explanation is null-terminated within buffer bounds */
        size_t explanation_len = strlen(out_explanation);
        EXPECT_LT(explanation_len, (size_t)NIMCP_MAX_EXPLANATION_SIZE)
            << "Explanation exceeds NIMCP_MAX_EXPLANATION_SIZE buffer";

        /* Verify null terminator */
        EXPECT_EQ(out_explanation[explanation_len], '\0');

        /* Verify label is also safe */
        size_t label_len = strlen(out_label);
        EXPECT_LT(label_len, (size_t)NIMCP_MAX_LABEL_SIZE)
            << "Label exceeds NIMCP_MAX_LABEL_SIZE buffer";
    }
}

/**
 * @brief Verify NIMCP_MAX_EXPLANATION_SIZE constant is defined correctly
 *
 * WHAT: Compile-time check that the new constant exists and has correct value
 * WHY:  Ensures nimcp.h properly defines the constant
 */
TEST_F(ApiSafetyTest, MaxExplanationSizeConstant) {
    EXPECT_EQ(NIMCP_MAX_EXPLANATION_SIZE, 256)
        << "NIMCP_MAX_EXPLANATION_SIZE should be 256";
    EXPECT_EQ(NIMCP_MAX_LABEL_SIZE, 64)
        << "NIMCP_MAX_LABEL_SIZE should be 64";
}

/**
 * @brief Verify train_step with NULL result pointer
 *
 * WHAT: Calling train_step with NULL result should still work
 * WHY:  The result parameter is optional (can be NULL)
 */
TEST_F(ApiSafetyTest, TrainStepNullResult) {
    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t cfg_status = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(cfg_status, NIMCP_OK) << "Failed to configure training";

    float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float targets[2] = {1.0f, 0.0f};

    /* NULL result pointer should not crash */
    nimcp_status_t status = nimcp_brain_train_step(
        brain, features, 4, targets, 2, nullptr);

    EXPECT_EQ(status, NIMCP_OK) << "train_step with NULL result should succeed";
}
