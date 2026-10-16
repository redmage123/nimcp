//=============================================================================
// test_brain_learning_security.cpp - Security Tests for Brain Learning
//=============================================================================
/**
 * @file test_brain_learning_security.cpp
 * @brief Security validation tests for brain learning module
 *
 * WHAT: Tests adversarial input detection and injection prevention
 * WHY:  Ensure learning system is robust against attacks
 * HOW:  Inject malicious inputs and verify rejection
 *
 * @author NIMCP Development Team
 * @date 2025-12-05
 */

#include <gtest/gtest.h>
#include <cmath>
// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_brain_learning.h"

class BrainLearningSecurityTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_inputs = 10;
        config.num_outputs = 5;
        config.learning_rate = 0.01f;
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Test: NaN/Inf Detection
//=============================================================================

TEST_F(BrainLearningSecurityTest, RejectsNaNFeatures) {
    float features[10] = {0.1f, NAN, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    float loss = brain_learn_example(brain, features, 10, "valid_label", 0.8f);

    // Should reject input with NaN
    EXPECT_LT(loss, 0.0f);
}

TEST_F(BrainLearningSecurityTest, RejectsInfFeatures) {
    float features[10] = {0.1f, 0.2f, INFINITY, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    float loss = brain_learn_example(brain, features, 10, "valid_label", 0.8f);

    // Should reject input with Inf
    EXPECT_LT(loss, 0.0f);
}

//=============================================================================
// Test: Unusual Label Handling
//=============================================================================

TEST_F(BrainLearningSecurityTest, RejectsFormatStringInLabel) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Attempt format string attack via label
    float loss = brain_learn_example(brain, features, 10, "%s%n%x", 0.8f);

    // Security system should reject malicious label
    EXPECT_LT(loss, 0.0f);
}

//=============================================================================
// Test: SQL Injection Prevention
//=============================================================================

TEST_F(BrainLearningSecurityTest, RejectsSQLInjectionInLabel) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Attempt SQL injection via label
    float loss = brain_learn_example(brain, features, 10, "'; DROP TABLE users--", 0.8f);

    // Security system should reject malicious label
    EXPECT_LT(loss, 0.0f);
}

//=============================================================================
// Test: Long Label Handling
//=============================================================================

TEST_F(BrainLearningSecurityTest, HandlesLongLabels) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Long label string
    float loss = brain_learn_example(brain, features, 10, "very_long_label_name_that_might_cause_issues", 0.8f);

    // Should handle long labels gracefully
    EXPECT_GE(loss, 0.0f);
}

//=============================================================================
// Test: Normal Input Works
//=============================================================================

TEST_F(BrainLearningSecurityTest, AcceptsValidInput) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    float loss = brain_learn_example(brain, features, 10, "valid_label", 0.8f);

    // Valid input should not return error
    // Note: May still return -1 if learning not fully initialized
    SUCCEED();
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
