/**
 * @file test_brain_api_returns.cpp
 * @brief Unit tests for brain API return code correctness
 *
 * WHAT: Verify that brain API functions return proper nimcp_status_t codes
 * WHY:  P0-7 fix - functions were returning raw -1 instead of enum codes
 * HOW:  Test failure paths to ensure proper status codes are returned
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <string.h>

/**
 * @brief Test fixture for brain API return code tests
 */
class BrainAPIReturnsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

/**
 * @brief P0-7: Verify learn failure returns proper nimcp_status_t, not raw -1
 *
 * WHAT: When brain_learn_example fails internally, the return must be a
 *       valid nimcp_status_t enum value (>= 1000), not -1
 * WHY:  Returning -1 violates the nimcp_status_t contract and causes
 *       incorrect error handling in callers checking against NIMCP_OK
 */
TEST_F(BrainAPIReturnsTest, BrainAPI_LearnFailureReturnsProperCode) {
    // Create a brain for testing
    nimcp_brain_t brain = nimcp_brain_create(
        "return_test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4,
        2
    );
    ASSERT_NE(brain, nullptr) << "Failed to create brain for test";

    // Test with NULL features - should return NIMCP_ERROR_NULL_ARG
    nimcp_status_t status = nimcp_brain_learn_example(
        brain, nullptr, 4, "test_label", 1.0f);

    // The return value must be a valid nimcp_status_t, not -1
    EXPECT_NE(status, (nimcp_status_t)-1)
        << "Return value must not be raw -1; must be a proper nimcp_status_t code";
    EXPECT_NE(status, NIMCP_OK)
        << "NULL features should not return success";
    EXPECT_GE((int)status, 1000)
        << "Error codes must be >= 1000 per NIMCP convention";

    // Test with NULL brain - should return NIMCP_ERROR_NULL_ARG
    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    status = nimcp_brain_learn_example(
        nullptr, features, 4, "test_label", 1.0f);

    EXPECT_NE(status, (nimcp_status_t)-1)
        << "Return value must not be raw -1";
    EXPECT_NE(status, NIMCP_OK)
        << "NULL brain should not return success";

    // Test with NULL label - should return NIMCP_ERROR_NULL_ARG
    status = nimcp_brain_learn_example(
        brain, features, 4, nullptr, 1.0f);

    EXPECT_NE(status, (nimcp_status_t)-1)
        << "Return value must not be raw -1";
    EXPECT_NE(status, NIMCP_OK)
        << "NULL label should not return success";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Verify success returns NIMCP_OK (0)
 *
 * WHAT: When learn_example succeeds, the return must be NIMCP_OK
 * WHY:  Ensure the success path uses the proper constant
 */
TEST_F(BrainAPIReturnsTest, BrainAPI_SuccessReturnsNIMCP_OK) {
    nimcp_brain_t brain = nimcp_brain_create(
        "success_test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        4,
        2
    );
    ASSERT_NE(brain, nullptr) << "Failed to create brain for test";

    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};

    nimcp_status_t status = nimcp_brain_learn_example(
        brain, features, 4, "class_a", 1.0f);

    // Success must return exactly NIMCP_OK (0)
    EXPECT_EQ(status, NIMCP_OK)
        << "Successful learn_example must return NIMCP_OK (0)";
    EXPECT_EQ(status, NIMCP_SUCCESS)
        << "NIMCP_OK and NIMCP_SUCCESS must be equivalent";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Verify predict failure returns proper code, not -1
 */
TEST_F(BrainAPIReturnsTest, BrainAPI_PredictFailureReturnsProperCode) {
    // Test with NULL brain
    char label[64];
    float confidence;
    nimcp_status_t status = nimcp_brain_predict(
        nullptr, nullptr, 0, label, &confidence);

    EXPECT_NE(status, (nimcp_status_t)-1)
        << "Predict must not return raw -1";
    EXPECT_NE(status, NIMCP_OK)
        << "NULL brain predict should fail";
}

/**
 * @brief Verify save failure returns proper IO error code, not -1
 */
TEST_F(BrainAPIReturnsTest, BrainAPI_SaveFailureReturnsProperCode) {
    // Test with NULL brain
    nimcp_status_t status = nimcp_brain_save(nullptr, "/tmp/test.brain");

    EXPECT_NE(status, (nimcp_status_t)-1)
        << "Save must not return raw -1";
    EXPECT_NE(status, NIMCP_OK)
        << "NULL brain save should fail";
}
