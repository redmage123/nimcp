/**
 * @file test_api_brain_operations.cpp
 * @brief GoogleTest unit tests for NIMCP API brain operations
 *
 * Tests learning, prediction, and inference operations to ensure
 * correct behavior and parameter validation.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <string.h>

/**
 * @brief Test fixture for API brain operations tests
 */
class APIBrainOperationsTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_init();

        brain = nimcp_brain_create(
            "test_brain",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            10,
            2
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
 * @brief Test that learn_example with valid data succeeds
 */
TEST_F(APIBrainOperationsTest, LearnExampleValidDataSucceeds) {
    float features[10] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f
    };

    nimcp_status_t status = nimcp_brain_learn_example(
        brain,
        features,
        10,
        "class_a",
        1.0f
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);
}

/**
 * @brief Test that learn_example with NULL brain fails
 */
TEST_F(APIBrainOperationsTest, LearnExampleNullBrainFails) {
    float features[10] = {0.0f};

    nimcp_status_t status = nimcp_brain_learn_example(
        nullptr,        // NULL brain
        features,
        10,
        "class_a",
        1.0f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that learn_example with NULL features fails
 */
TEST_F(APIBrainOperationsTest, LearnExampleNullFeaturesFails) {
    nimcp_status_t status = nimcp_brain_learn_example(
        brain,
        nullptr,        // NULL features
        10,
        "class_a",
        1.0f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that learn_example with NULL label fails
 */
TEST_F(APIBrainOperationsTest, LearnExampleNullLabelFails) {
    float features[10] = {0.0f};

    nimcp_status_t status = nimcp_brain_learn_example(
        brain,
        features,
        10,
        nullptr,        // NULL label
        1.0f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that predict with valid data succeeds
 */
TEST_F(APIBrainOperationsTest, PredictValidDataSucceeds) {
    // First, train the brain
    float features_train[10] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f
    };
    nimcp_brain_learn_example(brain, features_train, 10, "class_a", 1.0f);

    // Now predict
    float features_predict[10] = {
        0.15f, 0.25f, 0.35f, 0.45f, 0.55f,
        0.65f, 0.75f, 0.85f, 0.95f, 1.05f
    };
    char label[64];
    float confidence;

    nimcp_status_t status = nimcp_brain_predict(
        brain,
        features_predict,
        10,
        label,
        &confidence
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);
}

/**
 * @brief Test that predict with NULL brain fails
 */
TEST_F(APIBrainOperationsTest, PredictNullBrainFails) {
    float features[10] = {0.0f};
    char label[64];
    float confidence;

    nimcp_status_t status = nimcp_brain_predict(
        nullptr,        // NULL brain
        features,
        10,
        label,
        &confidence
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that predict with NULL features fails
 */
TEST_F(APIBrainOperationsTest, PredictNullFeaturesFails) {
    char label[64];
    float confidence;

    nimcp_status_t status = nimcp_brain_predict(
        brain,
        nullptr,        // NULL features
        10,
        label,
        &confidence
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that predict with NULL out_label fails
 */
TEST_F(APIBrainOperationsTest, PredictNullOutLabelFails) {
    float features[10] = {0.0f};
    float confidence;

    nimcp_status_t status = nimcp_brain_predict(
        brain,
        features,
        10,
        nullptr,        // NULL out_label
        &confidence
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that predict with NULL out_confidence fails
 */
TEST_F(APIBrainOperationsTest, PredictNullOutConfidenceFails) {
    float features[10] = {0.0f};
    char label[64];

    nimcp_status_t status = nimcp_brain_predict(
        brain,
        features,
        10,
        label,
        nullptr        // NULL out_confidence
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that infer with valid data succeeds
 */
TEST_F(APIBrainOperationsTest, InferValidDataSucceeds) {
    float features[10] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f
    };
    float outputs[2];

    nimcp_status_t status = nimcp_brain_infer(
        brain,
        features,
        10,
        outputs,
        2
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);
}

/**
 * @brief Test that infer with NULL brain fails
 */
TEST_F(APIBrainOperationsTest, InferNullBrainFails) {
    float features[10] = {0.0f};
    float outputs[2];

    nimcp_status_t status = nimcp_brain_infer(
        nullptr,        // NULL brain
        features,
        10,
        outputs,
        2
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that infer with NULL features fails
 */
TEST_F(APIBrainOperationsTest, InferNullFeaturesFails) {
    float outputs[2];

    nimcp_status_t status = nimcp_brain_infer(
        brain,
        nullptr,        // NULL features
        10,
        outputs,
        2
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that infer with NULL outputs fails
 */
TEST_F(APIBrainOperationsTest, InferNullOutputsFails) {
    float features[10] = {0.0f};

    nimcp_status_t status = nimcp_brain_infer(
        brain,
        features,
        10,
        nullptr,        // NULL outputs
        2
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

/**
 * @brief Test that infer fills output array correctly
 */
TEST_F(APIBrainOperationsTest, InferFillsOutputArrayCorrectly) {
    float features[10] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        0.6f, 0.7f, 0.8f, 0.9f, 1.0f
    };
    float outputs[2] = {-999.0f, -999.0f};  // Sentinel values

    nimcp_status_t status = nimcp_brain_infer(
        brain,
        features,
        10,
        outputs,
        2
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Outputs should have been modified (not sentinel values)
    // We can't know exact values, but they should be in valid range
    EXPECT_NE(outputs[0], -999.0f) << "Output[0] was not modified";
    EXPECT_NE(outputs[1], -999.0f) << "Output[1] was not modified";
}

/**
 * @brief Test learning and prediction integration
 */
TEST_F(APIBrainOperationsTest, LearnAndPredictIntegration) {
    // Learn multiple examples
    float features1[10] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    float features2[10] = {1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};

    nimcp_status_t status1 = nimcp_brain_learn_example(
        brain, features1, 10, "class_a", 1.0f
    );
    EXPECT_EQ(status1, NIMCP_SUCCESS);

    nimcp_status_t status2 = nimcp_brain_learn_example(
        brain, features2, 10, "class_b", 1.0f
    );
    EXPECT_EQ(status2, NIMCP_SUCCESS);

    // Now predict
    char label[64];
    float confidence;

    nimcp_status_t status3 = nimcp_brain_predict(
        brain, features1, 10, label, &confidence
    );
    EXPECT_EQ(status3, NIMCP_SUCCESS);

    // Label should be valid (not empty)
    EXPECT_GT(strlen(label), 0) << "Predicted label should not be empty";

    // Confidence should be in valid range [0.0, 1.0]
    EXPECT_GE(confidence, 0.0f) << "Confidence should be >= 0.0";
    EXPECT_LE(confidence, 1.0f) << "Confidence should be <= 1.0";
}

/**
 * @brief Test multiple learning examples with same label
 */
TEST_F(APIBrainOperationsTest, MultipleLearningSameLabel) {
    float features1[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float features2[10] = {0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f, 0.95f, 1.05f};
    float features3[10] = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f};

    nimcp_status_t status1 = nimcp_brain_learn_example(brain, features1, 10, "positive", 1.0f);
    nimcp_status_t status2 = nimcp_brain_learn_example(brain, features2, 10, "positive", 1.0f);
    nimcp_status_t status3 = nimcp_brain_learn_example(brain, features3, 10, "positive", 1.0f);

    EXPECT_EQ(status1, NIMCP_SUCCESS);
    EXPECT_EQ(status2, NIMCP_SUCCESS);
    EXPECT_EQ(status3, NIMCP_SUCCESS);
}

/**
 * @brief Test learning with different confidence values
 */
TEST_F(APIBrainOperationsTest, LearningWithDifferentConfidences) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // High confidence
    nimcp_status_t status1 = nimcp_brain_learn_example(brain, features, 10, "label1", 1.0f);
    EXPECT_EQ(status1, NIMCP_SUCCESS);

    // Medium confidence
    nimcp_status_t status2 = nimcp_brain_learn_example(brain, features, 10, "label2", 0.5f);
    EXPECT_EQ(status2, NIMCP_SUCCESS);

    // Low confidence
    nimcp_status_t status3 = nimcp_brain_learn_example(brain, features, 10, "label3", 0.1f);
    EXPECT_EQ(status3, NIMCP_SUCCESS);

    // Zero confidence (edge case)
    nimcp_status_t status4 = nimcp_brain_learn_example(brain, features, 10, "label4", 0.0f);
    EXPECT_EQ(status4, NIMCP_SUCCESS);
}

/**
 * @brief Test infer with different output sizes
 */
TEST_F(APIBrainOperationsTest, InferWithDifferentOutputSizes) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Exact size
    float outputs1[2];
    nimcp_status_t status1 = nimcp_brain_infer(brain, features, 10, outputs1, 2);
    EXPECT_EQ(status1, NIMCP_SUCCESS);

    // Smaller than brain output (should work, truncated)
    float outputs2[1];
    nimcp_status_t status2 = nimcp_brain_infer(brain, features, 10, outputs2, 1);
    EXPECT_EQ(status2, NIMCP_SUCCESS);

    // Larger than brain output (should work, zero-padded)
    float outputs3[5] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    nimcp_status_t status3 = nimcp_brain_infer(brain, features, 10, outputs3, 5);
    EXPECT_EQ(status3, NIMCP_SUCCESS);

    // Extra outputs should be zero (not sentinel)
    EXPECT_EQ(outputs3[2], 0.0f);
    EXPECT_EQ(outputs3[3], 0.0f);
    EXPECT_EQ(outputs3[4], 0.0f);
}

/**
 * @brief Test predict returns valid label buffer
 */
TEST_F(APIBrainOperationsTest, PredictReturnsValidLabel) {
    // Train
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    nimcp_brain_learn_example(brain, features, 10, "test_label", 1.0f);

    // Predict
    char label[64];
    memset(label, 0xFF, sizeof(label));  // Fill with garbage
    float confidence;

    nimcp_status_t status = nimcp_brain_predict(brain, features, 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Label should be null-terminated
    EXPECT_NE(label[63], 0xFF) << "Label buffer was not written to";

    // Should be able to safely compute length
    size_t len = strlen(label);
    EXPECT_LT(len, 64) << "Label should be null-terminated within buffer";
}

/**
 * @brief Test batch learning and prediction
 */
TEST_F(APIBrainOperationsTest, BatchLearningAndPrediction) {
    const int num_examples = 10;
    const int num_features = 10;

    // Learn multiple examples
    for (int i = 0; i < num_examples; i++) {
        float features[10];
        for (int j = 0; j < num_features; j++) {
            features[j] = (i * num_features + j) / 100.0f;
        }

        char label[32];
        snprintf(label, sizeof(label), "class_%d", i % 3);

        nimcp_status_t status = nimcp_brain_learn_example(
            brain, features, num_features, label, 1.0f
        );
        EXPECT_EQ(status, NIMCP_SUCCESS) << "Learning failed at example " << i;
    }

    // Predict on all examples
    for (int i = 0; i < num_examples; i++) {
        float features[10];
        for (int j = 0; j < num_features; j++) {
            features[j] = (i * num_features + j) / 100.0f;
        }

        char predicted_label[64];
        float confidence;

        nimcp_status_t status = nimcp_brain_predict(
            brain, features, num_features, predicted_label, &confidence
        );
        EXPECT_EQ(status, NIMCP_SUCCESS) << "Prediction failed at example " << i;
    }
}
