/**
 * @file test_dialect_regression.cpp
 * @brief Regression tests for dialect learning system
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_dialect_learning.h"

TEST(DialectLearningRegression, LearningConvergence) {
    // Ensure learning converges consistently
    dialect_learner_t dl = dialect_learner_create(nullptr);
    ASSERT_NE(dl, nullptr);

    const uint32_t DIM = 8;
    float* src[5];
    float* tgt[5];

    for (int i = 0; i < 5; i++) {
        src[i] = new float[DIM];
        tgt[i] = new float[DIM];
        for (uint32_t j = 0; j < DIM; j++) {
            src[i][j] = 0.5f;
            tgt[i][j] = 0.5f;
        }
    }

    dialect_learn_from_pairs(dl, 1, 2, (const float**)src, (const float**)tgt, 5, DIM);

    float compat = dialect_get_compatibility(dl, 1, 2);
    EXPECT_GT(compat, 0.9f);  // Should learn identity mapping well

    for (int i = 0; i < 5; i++) {
        delete[] src[i];
        delete[] tgt[i];
    }

    dialect_learner_destroy(dl);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
