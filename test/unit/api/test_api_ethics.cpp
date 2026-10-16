/**
 * @file test_api_ethics.cpp
 * @brief Unit tests for NIMCP API - Ethics functions
 *
 * Tests the ethics API:
 * - nimcp_ethics_create()
 * - nimcp_ethics_destroy()
 * - nimcp_ethics_check()
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cmath>

class EthicsAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// nimcp_ethics_create() tests
//=============================================================================

TEST_F(EthicsAPITest, EthicsCreateSucceeds) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    EXPECT_NE(ethics, nullptr);

    if (ethics) {
        nimcp_ethics_destroy(ethics);
    }
}

TEST_F(EthicsAPITest, EthicsCreateReturnsValidHandle) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Verify handle is valid by using it
    float situation[10] = {0.5f};
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 10, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCreateMultipleInstances) {
    nimcp_ethics_t ethics1 = nimcp_ethics_create();
    nimcp_ethics_t ethics2 = nimcp_ethics_create();

    EXPECT_NE(ethics1, nullptr);
    EXPECT_NE(ethics2, nullptr);
    EXPECT_NE(ethics1, ethics2);

    if (ethics1) nimcp_ethics_destroy(ethics1);
    if (ethics2) nimcp_ethics_destroy(ethics2);
}

//=============================================================================
// nimcp_ethics_destroy() tests
//=============================================================================

TEST_F(EthicsAPITest, EthicsDestroySucceeds) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Should not crash
    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsDestroyWithNullIsSafe) {
    // Should not crash with NULL
    nimcp_ethics_destroy(nullptr);
}

//=============================================================================
// nimcp_ethics_check() tests
//=============================================================================

TEST_F(EthicsAPITest, EthicsCheckSucceeds) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.5f;
    }
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckNullEthicsFails) {
    float situation[10] = {0.5f};
    float score;

    nimcp_status_t status = nimcp_ethics_check(nullptr, situation, 10, &score);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(EthicsAPITest, EthicsCheckNullSituationFails) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, nullptr, 10, &score);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckNullOutScoreFails) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[10] = {0.5f};

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 10, nullptr);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckScoreInValidRange) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.5f;
    }
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
    ASSERT_EQ(status, NIMCP_SUCCESS);

    // Score should be in range [-1.0, 1.0] according to API docs
    EXPECT_GE(score, -1.0f);
    EXPECT_LE(score, 1.0f);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckWithDifferentFeatureCounts) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    uint32_t feature_counts[] = {1, 5, 10, 32, 64, 128};

    for (uint32_t count : feature_counts) {
        float* situation = new float[count];
        for (uint32_t i = 0; i < count; i++) {
            situation[i] = 0.5f;
        }
        float score;

        nimcp_status_t status = nimcp_ethics_check(ethics, situation, count, &score);
        EXPECT_EQ(status, NIMCP_SUCCESS);

        delete[] situation;
    }

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckWithZeroFeatures) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[1] = {0.0f};
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 0, &score);
    // May fail or succeed depending on implementation

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckWithPositiveSituation) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Create situation that should be positive (all positive features)
    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 1.0f;
    }
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckWithNegativeSituation) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Create situation that should be negative (all negative features)
    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = -1.0f;
    }
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckWithNeutralSituation) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Create neutral situation (all zeros)
    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.0f;
    }
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsCheckMultipleTimes) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.5f;
    }

    // Check multiple times
    for (int i = 0; i < 10; i++) {
        float score;
        nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
        EXPECT_EQ(status, NIMCP_SUCCESS);
        EXPECT_GE(score, -1.0f);
        EXPECT_LE(score, 1.0f);
    }

    nimcp_ethics_destroy(ethics);
}

//=============================================================================
// Ethics evaluation workflow tests
//=============================================================================

TEST_F(EthicsAPITest, EthicsEvaluationWorkflow) {
    // Create ethics module
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Test multiple situations
    struct TestCase {
        const char* description;
        float features[32];
        uint32_t num_features;
    };

    TestCase cases[] = {
        {"Positive action", {1.0f, 0.8f, 0.6f}, 3},
        {"Negative action", {-1.0f, -0.8f, -0.6f}, 3},
        {"Neutral action", {0.0f, 0.0f, 0.0f}, 3},
        {"Mixed action", {0.5f, -0.3f, 0.1f}, 3}
    };

    for (const auto& test_case : cases) {
        float score;
        nimcp_status_t status = nimcp_ethics_check(
            ethics, test_case.features, test_case.num_features, &score
        );

        EXPECT_EQ(status, NIMCP_SUCCESS) << "Failed for: " << test_case.description;
        EXPECT_GE(score, -1.0f) << "Score out of range for: " << test_case.description;
        EXPECT_LE(score, 1.0f) << "Score out of range for: " << test_case.description;
    }

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, EthicsConsistentEvaluations) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.7f;
    }

    float score1, score2;

    nimcp_ethics_check(ethics, situation, 32, &score1);
    nimcp_ethics_check(ethics, situation, 32, &score2);

    // Same input should give same output (deterministic)
    EXPECT_FLOAT_EQ(score1, score2);

    nimcp_ethics_destroy(ethics);
}

TEST_F(EthicsAPITest, MultipleEthicsInstancesIndependent) {
    nimcp_ethics_t ethics1 = nimcp_ethics_create();
    nimcp_ethics_t ethics2 = nimcp_ethics_create();

    ASSERT_NE(ethics1, nullptr);
    ASSERT_NE(ethics2, nullptr);

    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.5f;
    }

    float score1, score2;

    nimcp_status_t status1 = nimcp_ethics_check(ethics1, situation, 32, &score1);
    nimcp_status_t status2 = nimcp_ethics_check(ethics2, situation, 32, &score2);

    EXPECT_EQ(status1, NIMCP_SUCCESS);
    EXPECT_EQ(status2, NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics1);
    nimcp_ethics_destroy(ethics2);
}

TEST_F(EthicsAPITest, EthicsScoreNotNaN) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[32];
    for (int i = 0; i < 32; i++) {
        situation[i] = 0.5f;
    }
    float score;

    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 32, &score);
    ASSERT_EQ(status, NIMCP_SUCCESS);

    EXPECT_FALSE(std::isnan(score)) << "Score should not be NaN";
    EXPECT_FALSE(std::isinf(score)) << "Score should not be infinite";

    nimcp_ethics_destroy(ethics);
}
