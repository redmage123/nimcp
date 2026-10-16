/**
 * @file test_dialect_learning.cpp
 * @brief Unit tests for dialect learning system
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_dialect_learning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DialectLearningTest : public ::testing::Test {
protected:
    dialect_learner_t dl;
    const uint32_t DIM = 8;
    const uint32_t V1_ID = 0x0700;
    const uint32_t MT_ID = 0x0703;

    void SetUp() override {
        dialect_learner_config_t config = {
            .max_dialects = 50,
            .translation_dim = DIM,
            .learning_rate = 0.01f,
            .enable_bidirectional = true,
            .enable_bio_async = false
        };
        dl = dialect_learner_create(&config);
        ASSERT_NE(dl, nullptr);
    }

    void TearDown() override {
        if (dl) {
            dialect_learner_destroy(dl);
            dl = nullptr;
        }
    }

    // Helper to generate random signal
    void generate_signal(float* signal, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            signal[i] = (float)rand() / (float)RAND_MAX;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(DialectLearningTest, CreateWithDefaultConfig) {
    dialect_learner_destroy(dl);
    dl = dialect_learner_create(nullptr);
    ASSERT_NE(dl, nullptr);
}

TEST_F(DialectLearningTest, DestroyNullHandle) {
    dialect_learner_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Dialect Learning Tests
//=============================================================================

TEST_F(DialectLearningTest, LearnFromPairs) {
    const uint32_t NUM_PAIRS = 10;

    // Create training pairs
    std::vector<float*> source_signals;
    std::vector<float*> target_signals;

    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        float* src = new float[DIM];
        float* tgt = new float[DIM];
        generate_signal(src, DIM);
        generate_signal(tgt, DIM);
        source_signals.push_back(src);
        target_signals.push_back(tgt);
    }

    // Learn dialect
    int result = dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                                         (const float**)source_signals.data(),
                                         (const float**)target_signals.data(),
                                         NUM_PAIRS, DIM);

    EXPECT_EQ(result, 0);

    // Verify dialect exists
    EXPECT_TRUE(dialect_exists(dl, V1_ID, MT_ID));

    // Cleanup
    for (auto ptr : source_signals) delete[] ptr;
    for (auto ptr : target_signals) delete[] ptr;
}

TEST_F(DialectLearningTest, LearnBidirectional) {
    const uint32_t NUM_PAIRS = 5;

    std::vector<float*> source_signals;
    std::vector<float*> target_signals;

    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        float* src = new float[DIM];
        float* tgt = new float[DIM];
        generate_signal(src, DIM);
        generate_signal(tgt, DIM);
        source_signals.push_back(src);
        target_signals.push_back(tgt);
    }

    int result = dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                                         (const float**)source_signals.data(),
                                         (const float**)target_signals.data(),
                                         NUM_PAIRS, DIM);

    EXPECT_EQ(result, 0);

    // Both forward and reverse dialects should exist
    EXPECT_TRUE(dialect_exists(dl, V1_ID, MT_ID));
    EXPECT_TRUE(dialect_exists(dl, MT_ID, V1_ID));

    // Cleanup
    for (auto ptr : source_signals) delete[] ptr;
    for (auto ptr : target_signals) delete[] ptr;
}

TEST_F(DialectLearningTest, LearnFromPairsInvalidParams) {
    float* signals[5] = {nullptr};

    EXPECT_NE(dialect_learn_from_pairs(nullptr, V1_ID, MT_ID,
                                      (const float**)signals,
                                      (const float**)signals, 5, DIM), 0);
    EXPECT_NE(dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                                      nullptr, (const float**)signals, 5, DIM), 0);
    EXPECT_NE(dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                                      (const float**)signals, nullptr, 5, DIM), 0);
    EXPECT_NE(dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                                      (const float**)signals,
                                      (const float**)signals, 0, DIM), 0);
}

//=============================================================================
// Online Learning Tests
//=============================================================================

TEST_F(DialectLearningTest, UpdateOnline) {
    // First create a dialect
    const uint32_t NUM_PAIRS = 3;
    std::vector<float*> source_signals;
    std::vector<float*> target_signals;

    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        float* src = new float[DIM];
        float* tgt = new float[DIM];
        generate_signal(src, DIM);
        generate_signal(tgt, DIM);
        source_signals.push_back(src);
        target_signals.push_back(tgt);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)source_signals.data(),
                            (const float**)target_signals.data(),
                            NUM_PAIRS, DIM);

    // Now do online update
    float src[DIM], tgt[DIM];
    generate_signal(src, DIM);
    generate_signal(tgt, DIM);

    int result = dialect_update_online(dl, V1_ID, MT_ID, src, tgt, DIM);
    EXPECT_EQ(result, 0);

    // Cleanup
    for (auto ptr : source_signals) delete[] ptr;
    for (auto ptr : target_signals) delete[] ptr;
}

TEST_F(DialectLearningTest, UpdateOnlineNonExistentDialect) {
    float src[DIM], tgt[DIM];
    generate_signal(src, DIM);
    generate_signal(tgt, DIM);

    int result = dialect_update_online(dl, 999, 888, src, tgt, DIM);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Translation Tests
//=============================================================================

TEST_F(DialectLearningTest, TranslateSignal) {
    // Learn a simple identity-like mapping
    const uint32_t NUM_PAIRS = 5;
    std::vector<float*> source_signals;
    std::vector<float*> target_signals;

    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        float* src = new float[DIM];
        float* tgt = new float[DIM];
        generate_signal(src, DIM);
        // Target is similar to source (with noise)
        for (uint32_t j = 0; j < DIM; j++) {
            tgt[j] = src[j] + ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        source_signals.push_back(src);
        target_signals.push_back(tgt);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)source_signals.data(),
                            (const float**)target_signals.data(),
                            NUM_PAIRS, DIM);

    // Translate a signal
    float signal[DIM];
    generate_signal(signal, DIM);

    float translated[DIM];
    uint32_t translated_size;

    int result = dialect_translate(dl, V1_ID, MT_ID, signal, DIM,
                                   translated, &translated_size);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(translated_size, DIM);

    // Cleanup
    for (auto ptr : source_signals) delete[] ptr;
    for (auto ptr : target_signals) delete[] ptr;
}

TEST_F(DialectLearningTest, TranslateNonExistentDialect) {
    float signal[DIM];
    float translated[DIM];
    uint32_t size;

    int result = dialect_translate(dl, 999, 888, signal, DIM, translated, &size);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Dialect Query Tests
//=============================================================================

TEST_F(DialectLearningTest, GetDialect) {
    // Learn a dialect first
    const uint32_t NUM_PAIRS = 3;
    std::vector<float*> source_signals;
    std::vector<float*> target_signals;

    for (uint32_t i = 0; i < NUM_PAIRS; i++) {
        float* src = new float[DIM];
        float* tgt = new float[DIM];
        generate_signal(src, DIM);
        generate_signal(tgt, DIM);
        source_signals.push_back(src);
        target_signals.push_back(tgt);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)source_signals.data(),
                            (const float**)target_signals.data(),
                            NUM_PAIRS, DIM);

    // Get the dialect
    neural_dialect_t dialect;
    int result = dialect_get(dl, V1_ID, MT_ID, &dialect);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(dialect.source_region, V1_ID);
    EXPECT_EQ(dialect.target_region, MT_ID);
    EXPECT_NE(dialect.translation_matrix, nullptr);

    // Cleanup
    for (auto ptr : source_signals) delete[] ptr;
    for (auto ptr : target_signals) delete[] ptr;
}

TEST_F(DialectLearningTest, DialectExists) {
    EXPECT_FALSE(dialect_exists(dl, V1_ID, MT_ID));

    // Create a dialect
    float* signals[1] = {new float[DIM]};
    generate_signal(signals[0], DIM);

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 1, DIM);

    EXPECT_TRUE(dialect_exists(dl, V1_ID, MT_ID));

    delete[] signals[0];
}

TEST_F(DialectLearningTest, GetCompatibility) {
    // Initially no dialect
    float compat = dialect_get_compatibility(dl, V1_ID, MT_ID);
    EXPECT_LT(compat, 0.0f);

    // Learn a dialect
    float* signals[3];
    for (int i = 0; i < 3; i++) {
        signals[i] = new float[DIM];
        generate_signal(signals[i], DIM);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 3, DIM);

    // Should now have a compatibility score
    compat = dialect_get_compatibility(dl, V1_ID, MT_ID);
    EXPECT_GE(compat, 0.0f);
    EXPECT_LE(compat, 1.0f);

    for (int i = 0; i < 3; i++) delete[] signals[i];
}

//=============================================================================
// Dialect Management Tests
//=============================================================================

TEST_F(DialectLearningTest, GetAllDialects) {
    // Create a few dialects
    float* signals[2];
    for (int i = 0; i < 2; i++) {
        signals[i] = new float[DIM];
        generate_signal(signals[i], DIM);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 2, DIM);
    dialect_learn_from_pairs(dl, MT_ID, 0x0704,
                            (const float**)signals, (const float**)signals, 2, DIM);

    // Get all
    neural_dialect_t dialects[10];
    uint32_t count;
    int result = dialect_get_all(dl, dialects, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 2);  // At least 2 (may have bidirectional)

    for (int i = 0; i < 2; i++) delete[] signals[i];
}

TEST_F(DialectLearningTest, RemoveDialect) {
    // Create a dialect
    float* signals[1] = {new float[DIM]};
    generate_signal(signals[0], DIM);

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 1, DIM);

    EXPECT_TRUE(dialect_exists(dl, V1_ID, MT_ID));

    // Remove it
    int result = dialect_remove(dl, V1_ID, MT_ID);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(dialect_exists(dl, V1_ID, MT_ID));

    delete[] signals[0];
}

TEST_F(DialectLearningTest, ClearAllDialects) {
    // Create multiple dialects
    float* signals[2];
    for (int i = 0; i < 2; i++) {
        signals[i] = new float[DIM];
        generate_signal(signals[i], DIM);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 2, DIM);
    dialect_learn_from_pairs(dl, MT_ID, 0x0704,
                            (const float**)signals, (const float**)signals, 2, DIM);

    // Clear all
    int result = dialect_clear_all(dl);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(dialect_exists(dl, V1_ID, MT_ID));
    EXPECT_FALSE(dialect_exists(dl, MT_ID, 0x0704));

    for (int i = 0; i < 2; i++) delete[] signals[i];
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DialectLearningTest, GetStatistics) {
    // Perform some operations
    float* signals[3];
    for (int i = 0; i < 3; i++) {
        signals[i] = new float[DIM];
        generate_signal(signals[i], DIM);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 3, DIM);

    float signal[DIM], translated[DIM];
    uint32_t size;
    generate_signal(signal, DIM);
    dialect_translate(dl, V1_ID, MT_ID, signal, DIM, translated, &size);

    // Get stats
    dialect_learner_stats_t stats;
    int result = dialect_get_stats(dl, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.total_dialects_learned, 0);
    EXPECT_GT(stats.total_translations, 0);
    EXPECT_GT(stats.active_dialects, 0);

    for (int i = 0; i < 3; i++) delete[] signals[i];
}

TEST_F(DialectLearningTest, ResetStatistics) {
    // Generate some stats
    float* signals[2];
    for (int i = 0; i < 2; i++) {
        signals[i] = new float[DIM];
        generate_signal(signals[i], DIM);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 2, DIM);

    // Reset
    int result = dialect_reset_stats(dl);
    EXPECT_EQ(result, 0);

    // Verify stats are cleared
    dialect_learner_stats_t stats;
    dialect_get_stats(dl, &stats);
    EXPECT_EQ(stats.total_dialects_learned, 0);
    EXPECT_EQ(stats.total_translations, 0);

    for (int i = 0; i < 2; i++) delete[] signals[i];
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(DialectLearningTest, CloneDialect) {
    // Create a dialect
    float* signals[2];
    for (int i = 0; i < 2; i++) {
        signals[i] = new float[DIM];
        generate_signal(signals[i], DIM);
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals, (const float**)signals, 2, DIM);

    neural_dialect_t original, clone;
    dialect_get(dl, V1_ID, MT_ID, &original);

    int result = dialect_clone(&original, &clone);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(clone.source_region, original.source_region);
    EXPECT_EQ(clone.target_region, original.target_region);
    EXPECT_NE(clone.translation_matrix, original.translation_matrix);  // Different pointers

    // Verify matrix content is copied
    bool same = true;
    for (uint32_t i = 0; i < DIM * DIM; i++) {
        if (fabsf(clone.translation_matrix[i] - original.translation_matrix[i]) > 0.001f) {
            same = false;
            break;
        }
    }
    EXPECT_TRUE(same);

    dialect_free(&clone);

    for (int i = 0; i < 2; i++) delete[] signals[i];
}

TEST_F(DialectLearningTest, ComputeSimilarity) {
    // Create two similar dialects
    float* signals1[2];
    float* signals2[2];

    for (int i = 0; i < 2; i++) {
        signals1[i] = new float[DIM];
        signals2[i] = new float[DIM];
        generate_signal(signals1[i], DIM);
        // Make signals2 similar to signals1
        for (uint32_t j = 0; j < DIM; j++) {
            signals2[i][j] = signals1[i][j] + ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
        }
    }

    dialect_learn_from_pairs(dl, V1_ID, MT_ID,
                            (const float**)signals1, (const float**)signals1, 2, DIM);
    dialect_learn_from_pairs(dl, 100, 200,
                            (const float**)signals2, (const float**)signals2, 2, DIM);

    neural_dialect_t d1, d2;
    dialect_get(dl, V1_ID, MT_ID, &d1);
    dialect_get(dl, 100, 200, &d2);

    float similarity = dialect_compute_similarity(&d1, &d2);
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);

    // Similarity with itself should be 1.0
    similarity = dialect_compute_similarity(&d1, &d1);
    EXPECT_NEAR(similarity, 1.0f, 0.001f);

    for (int i = 0; i < 2; i++) {
        delete[] signals1[i];
        delete[] signals2[i];
    }
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(DialectLearningTest, MaxDialectsLimit) {
    dialect_learner_config_t config = {
        .max_dialects = 2,
        .translation_dim = DIM,
        .learning_rate = 0.01f,
        .enable_bidirectional = false,
        .enable_bio_async = false
    };

    dialect_learner_destroy(dl);
    dl = dialect_learner_create(&config);

    float* signals[1] = {new float[DIM]};
    generate_signal(signals[0], DIM);

    // First two should succeed
    EXPECT_EQ(dialect_learn_from_pairs(dl, 1, 2, (const float**)signals,
                                      (const float**)signals, 1, DIM), 0);
    EXPECT_EQ(dialect_learn_from_pairs(dl, 3, 4, (const float**)signals,
                                      (const float**)signals, 1, DIM), 0);

    // Third should fail
    EXPECT_NE(dialect_learn_from_pairs(dl, 5, 6, (const float**)signals,
                                      (const float**)signals, 1, DIM), 0);

    delete[] signals[0];
}

//=============================================================================
// Main Function
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
