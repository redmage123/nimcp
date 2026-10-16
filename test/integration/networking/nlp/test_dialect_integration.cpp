/**
 * @file test_dialect_integration.cpp
 * @brief Integration tests for dialect learning with NLP system
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_dialect_learning.h"

class DialectIntegrationTest : public ::testing::Test {
protected:
    dialect_learner_t dl;

    void SetUp() override {
        dl = dialect_learner_create(nullptr);
        ASSERT_NE(dl, nullptr);
    }

    void TearDown() override {
        dialect_learner_destroy(dl);
    }
};

TEST_F(DialectIntegrationTest, MultiRegionCommunication) {
    // Test dialect learning between multiple brain regions
    const uint32_t DIM = 8;
    float* src[3];
    float* tgt[3];

    for (int i = 0; i < 3; i++) {
        src[i] = new float[DIM];
        tgt[i] = new float[DIM];
        for (uint32_t j = 0; j < DIM; j++) {
            src[i][j] = (float)rand() / RAND_MAX;
            tgt[i][j] = (float)rand() / RAND_MAX;
        }
    }

    EXPECT_EQ(dialect_learn_from_pairs(dl, 0x0700, 0x0703,
                                      (const float**)src, (const float**)tgt, 3, DIM), 0);

    float signal[DIM], translated[DIM];
    uint32_t size;
    EXPECT_EQ(dialect_translate(dl, 0x0700, 0x0703, src[0], DIM, translated, &size), 0);

    for (int i = 0; i < 3; i++) {
        delete[] src[i];
        delete[] tgt[i];
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
