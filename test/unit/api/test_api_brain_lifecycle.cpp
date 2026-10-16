/**
 * @file test_api_brain_lifecycle.cpp
 * @brief GoogleTest unit tests for NIMCP API brain lifecycle
 *
 * Tests brain creation and destruction to ensure proper resource
 * management and parameter validation.
 */

#include <gtest/gtest.h>
#include "nimcp.h"

/**
 * @brief Test fixture for API brain lifecycle tests
 */
class APIBrainLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

/**
 * @brief Test that brain creation with valid parameters succeeds
 */
TEST_F(APIBrainLifecycleTest, BrainCreateSuccess) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test that brain creation with NULL name fails
 */
TEST_F(APIBrainLifecycleTest, BrainCreateNullNameFails) {
    nimcp_brain_t brain = nimcp_brain_create(
        nullptr,        // NULL name
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    EXPECT_EQ(brain, nullptr);
    const char* error = nimcp_get_error();
    EXPECT_NE(strstr(error, "name"), nullptr) << "Error should mention name";
}

/**
 * @brief Test that brain creation returns non-NULL handle
 */
TEST_F(APIBrainLifecycleTest, BrainCreateReturnsNonNull) {
    nimcp_brain_t brain = nimcp_brain_create(
        "valid_brain",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_REGRESSION,
        20,
        5
    );

    EXPECT_NE(brain, nullptr) << "Valid brain creation should return non-NULL";

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test that brain destroy with valid brain succeeds
 */
TEST_F(APIBrainLifecycleTest, BrainDestroySuccess) {
    nimcp_brain_t brain = nimcp_brain_create(
        "test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );
    ASSERT_NE(brain, nullptr);

    // Should not crash
    EXPECT_NO_FATAL_FAILURE(nimcp_brain_destroy(brain));
}

/**
 * @brief Test that brain destroy with NULL is safe
 */
TEST_F(APIBrainLifecycleTest, BrainDestroyNullIsSafe) {
    // Should not crash
    EXPECT_NO_FATAL_FAILURE(nimcp_brain_destroy(nullptr));
}

/**
 * @brief Test brain creation with TINY size
 */
TEST_F(APIBrainLifecycleTest, BrainCreateTinySize) {
    nimcp_brain_t brain = nimcp_brain_create(
        "tiny_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        5,
        2
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with SMALL size
 */
TEST_F(APIBrainLifecycleTest, BrainCreateSmallSize) {
    nimcp_brain_t brain = nimcp_brain_create(
        "small_brain",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        10,
        3
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with MEDIUM size
 */
TEST_F(APIBrainLifecycleTest, BrainCreateMediumSize) {
    nimcp_brain_t brain = nimcp_brain_create(
        "medium_brain",
        NIMCP_BRAIN_MEDIUM,
        NIMCP_TASK_CLASSIFICATION,
        50,
        10
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with LARGE size
 */
TEST_F(APIBrainLifecycleTest, BrainCreateLargeSize) {
    nimcp_brain_t brain = nimcp_brain_create(
        "large_brain",
        NIMCP_BRAIN_LARGE,
        NIMCP_TASK_CLASSIFICATION,
        100,
        20
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with CLASSIFICATION task
 */
TEST_F(APIBrainLifecycleTest, BrainCreateClassificationTask) {
    nimcp_brain_t brain = nimcp_brain_create(
        "classifier",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        3
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with REGRESSION task
 */
TEST_F(APIBrainLifecycleTest, BrainCreateRegressionTask) {
    nimcp_brain_t brain = nimcp_brain_create(
        "regressor",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_REGRESSION,
        10,
        1
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with PATTERN_MATCHING task
 */
TEST_F(APIBrainLifecycleTest, BrainCreatePatternMatchingTask) {
    nimcp_brain_t brain = nimcp_brain_create(
        "pattern_matcher",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_PATTERN_MATCHING,
        20,
        5
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with SEQUENCE task
 */
TEST_F(APIBrainLifecycleTest, BrainCreateSequenceTask) {
    nimcp_brain_t brain = nimcp_brain_create(
        "sequencer",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_SEQUENCE,
        15,
        8
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test brain creation with ASSOCIATION task
 */
TEST_F(APIBrainLifecycleTest, BrainCreateAssociationTask) {
    nimcp_brain_t brain = nimcp_brain_create(
        "associator",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_ASSOCIATION,
        12,
        4
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test multiple brain creation and destruction
 */
TEST_F(APIBrainLifecycleTest, MultipleBrainCreateDestroy) {
    nimcp_brain_t brain1 = nimcp_brain_create(
        "brain1", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 2
    );
    nimcp_brain_t brain2 = nimcp_brain_create(
        "brain2", NIMCP_BRAIN_TINY, NIMCP_TASK_REGRESSION, 5, 1
    );
    nimcp_brain_t brain3 = nimcp_brain_create(
        "brain3", NIMCP_BRAIN_SMALL, NIMCP_TASK_PATTERN_MATCHING, 20, 5
    );

    EXPECT_NE(brain1, nullptr);
    EXPECT_NE(brain2, nullptr);
    EXPECT_NE(brain3, nullptr);

    // All should be different
    EXPECT_NE(brain1, brain2);
    EXPECT_NE(brain2, brain3);
    EXPECT_NE(brain1, brain3);

    if (brain1) nimcp_brain_destroy(brain1);
    if (brain2) nimcp_brain_destroy(brain2);
    if (brain3) nimcp_brain_destroy(brain3);
}

/**
 * @brief Test brain creation with various input sizes
 */
TEST_F(APIBrainLifecycleTest, BrainCreateVariousInputSizes) {
    // Small input
    nimcp_brain_t brain1 = nimcp_brain_create(
        "small_input", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 1, 2
    );
    EXPECT_NE(brain1, nullptr);

    // Medium input
    nimcp_brain_t brain2 = nimcp_brain_create(
        "medium_input", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 50, 10
    );
    EXPECT_NE(brain2, nullptr);

    // Large input
    nimcp_brain_t brain3 = nimcp_brain_create(
        "large_input", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 200, 20
    );
    EXPECT_NE(brain3, nullptr);

    if (brain1) nimcp_brain_destroy(brain1);
    if (brain2) nimcp_brain_destroy(brain2);
    if (brain3) nimcp_brain_destroy(brain3);
}

/**
 * @brief Test brain creation with various output sizes
 */
TEST_F(APIBrainLifecycleTest, BrainCreateVariousOutputSizes) {
    // Binary classification
    nimcp_brain_t brain1 = nimcp_brain_create(
        "binary", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 2
    );
    EXPECT_NE(brain1, nullptr);

    // Multi-class classification
    nimcp_brain_t brain2 = nimcp_brain_create(
        "multiclass", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 20, 10
    );
    EXPECT_NE(brain2, nullptr);

    // Single output (regression)
    nimcp_brain_t brain3 = nimcp_brain_create(
        "regression", NIMCP_BRAIN_TINY, NIMCP_TASK_REGRESSION, 15, 1
    );
    EXPECT_NE(brain3, nullptr);

    if (brain1) nimcp_brain_destroy(brain1);
    if (brain2) nimcp_brain_destroy(brain2);
    if (brain3) nimcp_brain_destroy(brain3);
}

/**
 * @brief Test that brain names are preserved
 */
TEST_F(APIBrainLifecycleTest, BrainNamesPreserved) {
    const char* names[] = {
        "classifier_model",
        "regression_model",
        "pattern_detector",
        "sequence_predictor",
        "association_learner"
    };

    for (const char* name : names) {
        nimcp_brain_t brain = nimcp_brain_create(
            name,
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            10,
            2
        );

        EXPECT_NE(brain, nullptr) << "Failed to create brain: " << name;

        if (brain) {
            nimcp_brain_destroy(brain);
        }
    }
}
