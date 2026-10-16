/**
 * @file test_api_error_handling.cpp
 * @brief GoogleTest unit tests for NIMCP API error handling
 *
 * Tests error reporting mechanisms to ensure consistent and safe
 * error handling across all API functions.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <string.h>

/**
 * @brief Test fixture for API error handling tests
 */
class APIErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

/**
 * @brief Test that nimcp_get_error() returns "No error" initially
 */
TEST_F(APIErrorHandlingTest, InitialErrorStateIsNoError) {
    const char* error = nimcp_get_error();
    EXPECT_STREQ(error, "No error");
}

/**
 * @brief Test that nimcp_get_error() returns correct error after NULL brain
 */
TEST_F(APIErrorHandlingTest, ErrorAfterNullBrainDestroy) {
    // Destroying NULL brain should be safe but might not set error
    nimcp_brain_destroy(nullptr);

    // No error should be set (it's a safe no-op)
    // This is intentionally a no-op operation
    SUCCEED();
}

/**
 * @brief Test error message for NULL brain in learn_example
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullBrainLearn) {
    float features[10] = {0.0f};
    nimcp_status_t status = nimcp_brain_learn_example(
        nullptr,        // NULL brain
        features,
        10,
        "test_label",
        1.0f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_STRNE(error, "No error");
    EXPECT_NE(strstr(error, "NULL"), nullptr) << "Error should mention NULL";
}

/**
 * @brief Test error message for NULL features in learn_example
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullFeaturesLearn) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

    nimcp_status_t status = nimcp_brain_learn_example(
        brain,
        nullptr,        // NULL features
        10,
        "test_label",
        1.0f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "Features"), nullptr) << "Error should mention Features";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error message for NULL label in learn_example
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullLabelLearn) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.0f};
    nimcp_status_t status = nimcp_brain_learn_example(
        brain,
        features,
        10,
        nullptr,        // NULL label
        1.0f
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "Label"), nullptr) << "Error should mention Label";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error message for NULL brain in predict
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullBrainPredict) {
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
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "NULL"), nullptr) << "Error should mention NULL";
}

/**
 * @brief Test error message for NULL features in predict
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullFeaturesPredict) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

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
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "Features"), nullptr) << "Error should mention Features";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error message for NULL out_label in predict
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullOutputLabelPredict) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

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
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "label"), nullptr) << "Error should mention label";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error message for NULL out_confidence in predict
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullOutputConfidencePredict) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

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
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "confidence"), nullptr) << "Error should mention confidence";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error message for NULL brain in infer
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullBrainInfer) {
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
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "NULL"), nullptr) << "Error should mention NULL";
}

/**
 * @brief Test error message for NULL features in infer
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullFeaturesInfer) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

    float outputs[2];

    nimcp_status_t status = nimcp_brain_infer(
        brain,
        nullptr,        // NULL features
        10,
        outputs,
        2
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "Features"), nullptr) << "Error should mention Features";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error message for NULL outputs in infer
 */
TEST_F(APIErrorHandlingTest, ErrorOnNullOutputsInfer) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.0f};

    nimcp_status_t status = nimcp_brain_infer(
        brain,
        features,
        10,
        nullptr,        // NULL outputs
        2
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "Outputs"), nullptr) << "Error should mention Outputs";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test error buffer safety (no overflow)
 */
TEST_F(APIErrorHandlingTest, ErrorBufferNoOverflow) {
    // Trigger multiple errors
    for (int i = 0; i < 100; i++) {
        nimcp_brain_learn_example(nullptr, nullptr, 0, nullptr, 0.0f);
    }

    // Error string should still be valid and null-terminated
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);

    // Should be able to safely compute length
    size_t len = strlen(error);
    EXPECT_LT(len, 256) << "Error message should fit in buffer (256 bytes)";
}

/**
 * @brief Test that error is cleared after successful operation
 */
TEST_F(APIErrorHandlingTest, ErrorClearedAfterSuccess) {
    // Trigger an error
    nimcp_brain_learn_example(nullptr, nullptr, 0, nullptr, 0.0f);
    const char* error1 = nimcp_get_error();
    EXPECT_STRNE(error1, "No error");

    // Successful operation should clear error
    nimcp_brain_t brain = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10, 2
    );
    ASSERT_NE(brain, nullptr);

    const char* error2 = nimcp_get_error();
    EXPECT_STREQ(error2, "No error");

    nimcp_brain_destroy(brain);
}
