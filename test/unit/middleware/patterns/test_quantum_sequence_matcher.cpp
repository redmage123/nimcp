//=============================================================================
// test_quantum_sequence_matcher.cpp - Unit Tests for Quantum Sequence Matcher
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/patterns/nimcp_quantum_sequence_matcher.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class QSeqMatcherLifecycleTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(QSeqMatcherLifecycleTest, CreateWithDefaultConfig) {
    ctx = qseq_matcher_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QSeqMatcherLifecycleTest, CreateWithCustomConfig) {
    qseq_matcher_config_t config = qseq_matcher_default_config();
    config.max_templates = 64;
    config.amplitude_dim = 32;
    config.temporal_tolerance = 100.0f;

    ctx = qseq_matcher_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QSeqMatcherLifecycleTest, CreateInvalidTemplates) {
    qseq_matcher_config_t config = qseq_matcher_default_config();
    config.max_templates = 0;

    ctx = qseq_matcher_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QSeqMatcherLifecycleTest, CreateInvalidAmplitudeDim) {
    qseq_matcher_config_t config = qseq_matcher_default_config();
    config.amplitude_dim = 0;

    ctx = qseq_matcher_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QSeqMatcherLifecycleTest, DestroyNull) {
    qseq_matcher_destroy(nullptr);  // Should not crash
}

TEST_F(QSeqMatcherLifecycleTest, GetConfig) {
    qseq_matcher_config_t config = qseq_matcher_default_config();
    config.max_templates = 100;
    config.min_similarity = 0.7f;

    ctx = qseq_matcher_create(&config);
    ASSERT_NE(ctx, nullptr);

    qseq_matcher_config_t retrieved;
    int result = qseq_matcher_get_config(ctx, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.max_templates, 100);
    EXPECT_NEAR(retrieved.min_similarity, 0.7f, 1e-5);
}

//=============================================================================
// Template Management Tests
//=============================================================================

class QSeqMatcherTemplateTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void SetUp() override {
        ctx = qseq_matcher_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
        }
    }

    qseq_pattern_t createPattern(const std::vector<uint32_t>& symbols) {
        std::vector<float> timestamps(symbols.size());
        for (size_t i = 0; i < symbols.size(); i++) {
            timestamps[i] = (float)i * 10.0f;
        }
        return qseq_create_pattern(symbols.data(), timestamps.data(), symbols.size());
    }
};

TEST_F(QSeqMatcherTemplateTest, AddTemplate) {
    qseq_pattern_t pattern = createPattern({1, 2, 3, 4, 5});
    uint32_t pattern_id;

    int result = qseq_matcher_add_template(ctx, &pattern, &pattern_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(pattern_id, 0);
    EXPECT_EQ(qseq_matcher_template_count(ctx), 1);
}

TEST_F(QSeqMatcherTemplateTest, AddMultipleTemplates) {
    for (uint32_t i = 0; i < 10; i++) {
        qseq_pattern_t pattern = createPattern({i, i+1, i+2});
        uint32_t pattern_id;

        int result = qseq_matcher_add_template(ctx, &pattern, &pattern_id);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(pattern_id, i);
    }
    EXPECT_EQ(qseq_matcher_template_count(ctx), 10);
}

TEST_F(QSeqMatcherTemplateTest, RemoveTemplate) {
    qseq_pattern_t pattern = createPattern({1, 2, 3});
    uint32_t pattern_id;
    qseq_matcher_add_template(ctx, &pattern, &pattern_id);

    EXPECT_EQ(qseq_matcher_template_count(ctx), 1);

    int result = qseq_matcher_remove_template(ctx, pattern_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(qseq_matcher_template_count(ctx), 0);
}

TEST_F(QSeqMatcherTemplateTest, RemoveInvalidTemplate) {
    int result = qseq_matcher_remove_template(ctx, 999);
    EXPECT_EQ(result, -2);
}

TEST_F(QSeqMatcherTemplateTest, ClearTemplates) {
    for (int i = 0; i < 5; i++) {
        qseq_pattern_t pattern = createPattern({(uint32_t)i, (uint32_t)(i+1)});
        qseq_matcher_add_template(ctx, &pattern, nullptr);
    }
    EXPECT_EQ(qseq_matcher_template_count(ctx), 5);

    qseq_matcher_clear_templates(ctx);
    EXPECT_EQ(qseq_matcher_template_count(ctx), 0);
}

TEST_F(QSeqMatcherTemplateTest, AddTemplateNullContext) {
    qseq_pattern_t pattern = createPattern({1, 2, 3});
    int result = qseq_matcher_add_template(nullptr, &pattern, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Pattern Creation Tests
//=============================================================================

class QSeqPatternTest : public ::testing::Test {};

TEST_F(QSeqPatternTest, CreatePatternWithTimestamps) {
    uint32_t symbols[] = {10, 20, 30, 40};
    float timestamps[] = {0.0f, 5.0f, 15.0f, 25.0f};

    qseq_pattern_t pattern = qseq_create_pattern(symbols, timestamps, 4);

    EXPECT_EQ(pattern.length, 4);
    EXPECT_EQ(pattern.elements[0].symbol, 10);
    EXPECT_EQ(pattern.elements[3].symbol, 40);
    EXPECT_NEAR(pattern.elements[2].timestamp, 15.0f, 1e-5);
    EXPECT_NEAR(pattern.duration, 25.0f, 1e-5);
}

TEST_F(QSeqPatternTest, CreatePatternWithoutTimestamps) {
    uint32_t symbols[] = {1, 2, 3};

    qseq_pattern_t pattern = qseq_create_pattern(symbols, nullptr, 3);

    EXPECT_EQ(pattern.length, 3);
    EXPECT_NEAR(pattern.elements[0].timestamp, 0.0f, 1e-5);
    EXPECT_NEAR(pattern.elements[1].timestamp, 10.0f, 1e-5);
    EXPECT_NEAR(pattern.elements[2].timestamp, 20.0f, 1e-5);
}

TEST_F(QSeqPatternTest, PatternLengthLimit) {
    std::vector<uint32_t> long_symbols(100);
    for (size_t i = 0; i < 100; i++) {
        long_symbols[i] = i;
    }

    qseq_pattern_t pattern = qseq_create_pattern(long_symbols.data(), nullptr, 100);

    EXPECT_LE(pattern.length, QSEQ_MAX_PATTERN_LENGTH);
}

//=============================================================================
// Matching Tests
//=============================================================================

class QSeqMatcherMatchTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void SetUp() override {
        qseq_matcher_config_t config = qseq_matcher_default_config();
        config.grover_iterations = 3;  // Fixed for determinism
        ctx = qseq_matcher_create(&config);

        // Add some template patterns
        addTemplate({1, 2, 3, 4, 5});
        addTemplate({10, 20, 30, 40, 50});
        addTemplate({100, 200, 300});
    }

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
        }
    }

    void addTemplate(const std::vector<uint32_t>& symbols) {
        std::vector<float> timestamps(symbols.size());
        for (size_t i = 0; i < symbols.size(); i++) {
            timestamps[i] = (float)i * 10.0f;
        }
        qseq_pattern_t pattern = qseq_create_pattern(symbols.data(), timestamps.data(), symbols.size());
        qseq_matcher_add_template(ctx, &pattern, nullptr);
    }
};

TEST_F(QSeqMatcherMatchTest, ExactMatch) {
    uint32_t symbols[] = {1, 2, 3, 4, 5};
    float timestamps[] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f};
    qseq_pattern_t query = qseq_create_pattern(symbols, timestamps, 5);

    qseq_match_result_t result;
    int status = qseq_matcher_match(ctx, &query, &result);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(result.pattern_id, 0);  // First template
    EXPECT_GT(result.similarity, 0.5f);
}

TEST_F(QSeqMatcherMatchTest, NoMatch) {
    uint32_t symbols[] = {999, 998, 997};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 3);

    qseq_match_result_t result;
    int status = qseq_matcher_match(ctx, &query, &result);

    EXPECT_EQ(status, 0);
    // Should still return something, but low similarity
    EXPECT_LT(result.similarity, 0.8f);
}

TEST_F(QSeqMatcherMatchTest, PartialMatch) {
    uint32_t symbols[] = {1, 2, 3};  // Subset of first template
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 3);

    qseq_match_result_t result;
    qseq_matcher_match(ctx, &query, &result);

    // Should have some match
    EXPECT_GT(result.similarity, 0.0f);
}

TEST_F(QSeqMatcherMatchTest, MatchWithNoTemplates) {
    qseq_matcher_clear_templates(ctx);

    uint32_t symbols[] = {1, 2, 3};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 3);

    qseq_match_result_t result;
    int status = qseq_matcher_match(ctx, &query, &result);

    EXPECT_EQ(status, -2);  // No templates
}

TEST_F(QSeqMatcherMatchTest, MatchNullArgs) {
    uint32_t symbols[] = {1, 2, 3};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 3);
    qseq_match_result_t result;

    EXPECT_EQ(qseq_matcher_match(nullptr, &query, &result), -1);
    EXPECT_EQ(qseq_matcher_match(ctx, nullptr, &result), -1);
    EXPECT_EQ(qseq_matcher_match(ctx, &query, nullptr), -1);
}

//=============================================================================
// Find All Tests
//=============================================================================

class QSeqMatcherFindAllTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void SetUp() override {
        ctx = qseq_matcher_create(nullptr);

        // Add similar patterns
        addTemplate({1, 2, 3, 4, 5});
        addTemplate({1, 2, 3, 6, 7});  // Similar to first
        addTemplate({10, 20, 30});
    }

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
        }
    }

    void addTemplate(const std::vector<uint32_t>& symbols) {
        qseq_pattern_t pattern = qseq_create_pattern(symbols.data(), nullptr, symbols.size());
        qseq_matcher_add_template(ctx, &pattern, nullptr);
    }
};

TEST_F(QSeqMatcherFindAllTest, FindMultipleMatches) {
    uint32_t symbols[] = {1, 2, 3, 4, 5};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 5);

    qseq_match_result_t results[10];
    uint32_t n_results;

    int status = qseq_matcher_find_all(ctx, &query, 0.1f, results, 10, &n_results);

    EXPECT_EQ(status, 0);
    EXPECT_GT(n_results, 0);
}

TEST_F(QSeqMatcherFindAllTest, FindAllWithHighThreshold) {
    uint32_t symbols[] = {1, 2, 3};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 3);

    qseq_match_result_t results[10];
    uint32_t n_results;

    int status = qseq_matcher_find_all(ctx, &query, 0.99f, results, 10, &n_results);

    EXPECT_EQ(status, 0);
    // Very high threshold might find nothing
    for (uint32_t i = 0; i < n_results; i++) {
        EXPECT_GE(results[i].similarity, 0.99f);
    }
}

TEST_F(QSeqMatcherFindAllTest, FindAllNullArgs) {
    uint32_t symbols[] = {1, 2, 3};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 3);
    qseq_match_result_t results[10];
    uint32_t n_results;

    EXPECT_EQ(qseq_matcher_find_all(nullptr, &query, 0.5f, results, 10, &n_results), -1);
    EXPECT_EQ(qseq_matcher_find_all(ctx, nullptr, 0.5f, results, 10, &n_results), -1);
    EXPECT_EQ(qseq_matcher_find_all(ctx, &query, 0.5f, nullptr, 10, &n_results), -1);
    EXPECT_EQ(qseq_matcher_find_all(ctx, &query, 0.5f, results, 10, nullptr), -1);
}

//=============================================================================
// Grover Search Tests
//=============================================================================

class QSeqGroverTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void SetUp() override {
        qseq_matcher_config_t config = qseq_matcher_default_config();
        config.max_templates = 32;
        config.grover_iterations = 5;
        ctx = qseq_matcher_create(&config);

        // Add many templates
        for (uint32_t i = 0; i < 20; i++) {
            std::vector<uint32_t> symbols = {i, i+1, i+2, i+3};
            qseq_pattern_t pattern = qseq_create_pattern(symbols.data(), nullptr, 4);
            qseq_matcher_add_template(ctx, &pattern, nullptr);
        }
    }

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
        }
    }
};

TEST_F(QSeqGroverTest, GroverIterationsUsed) {
    uint32_t symbols[] = {5, 6, 7, 8};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 4);

    qseq_match_result_t result;
    qseq_matcher_match(ctx, &query, &result);

    EXPECT_EQ(result.grover_iterations, 5);  // From config
}

TEST_F(QSeqGroverTest, GroverFindsMatch) {
    // Query matches template 5 exactly
    uint32_t symbols[] = {5, 6, 7, 8};
    qseq_pattern_t query = qseq_create_pattern(symbols, nullptr, 4);

    qseq_match_result_t result;
    qseq_matcher_match(ctx, &query, &result);

    // Should find a good match
    EXPECT_GT(result.similarity, 0.3f);
}

//=============================================================================
// Distance Matrix Tests
//=============================================================================

class QSeqDistanceTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void SetUp() override {
        ctx = qseq_matcher_create(nullptr);

        // Add 3 templates
        addTemplate({1, 2, 3});
        addTemplate({1, 2, 3});  // Identical to first
        addTemplate({100, 200, 300});  // Very different
    }

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
        }
    }

    void addTemplate(const std::vector<uint32_t>& symbols) {
        qseq_pattern_t pattern = qseq_create_pattern(symbols.data(), nullptr, symbols.size());
        qseq_matcher_add_template(ctx, &pattern, nullptr);
    }
};

TEST_F(QSeqDistanceTest, DistanceMatrixSymmetric) {
    float distances[9];  // 3x3
    uint32_t n;

    int status = qseq_matcher_distance_matrix(ctx, distances, &n);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(n, 3);

    // Check symmetry
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            EXPECT_NEAR(distances[i * n + j], distances[j * n + i], 1e-5);
        }
    }
}

TEST_F(QSeqDistanceTest, DistanceMatrixDiagonal) {
    float distances[9];
    uint32_t n;

    qseq_matcher_distance_matrix(ctx, distances, &n);

    // Diagonal should be 0 (self-distance)
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_NEAR(distances[i * n + i], 0.0f, 1e-5);
    }
}

TEST_F(QSeqDistanceTest, IdenticalPatternsCloseDistance) {
    float distances[9];
    uint32_t n;

    qseq_matcher_distance_matrix(ctx, distances, &n);

    // Patterns 0 and 1 are identical, so distance should be 0
    EXPECT_NEAR(distances[0 * n + 1], 0.0f, 1e-5);
    EXPECT_NEAR(distances[1 * n + 0], 0.0f, 1e-5);
}

TEST_F(QSeqDistanceTest, DifferentPatternsLargerDistance) {
    float distances[9];
    uint32_t n;

    qseq_matcher_distance_matrix(ctx, distances, &n);

    // Pattern 0 and 2 are very different
    float dist_01 = distances[0 * n + 1];  // Identical
    float dist_02 = distances[0 * n + 2];  // Very different

    EXPECT_GT(dist_02, dist_01);
}

//=============================================================================
// Statistics Tests
//=============================================================================

class QSeqStatsTest : public ::testing::Test {
protected:
    qseq_matcher_t ctx = nullptr;

    void SetUp() override {
        ctx = qseq_matcher_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qseq_matcher_destroy(ctx);
        }
    }
};

TEST_F(QSeqStatsTest, InitialStats) {
    qseq_matcher_stats_t stats;
    int result = qseq_matcher_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.matches_performed, 0);
    EXPECT_EQ(stats.grover_total_iterations, 0);
    EXPECT_EQ(stats.stored_templates, 0);
}

TEST_F(QSeqStatsTest, StatsAfterOperations) {
    // Add template
    uint32_t symbols[] = {1, 2, 3};
    qseq_pattern_t pattern = qseq_create_pattern(symbols, nullptr, 3);
    qseq_matcher_add_template(ctx, &pattern, nullptr);

    // Perform match
    qseq_match_result_t result;
    qseq_matcher_match(ctx, &pattern, &result);

    qseq_matcher_stats_t stats;
    qseq_matcher_get_stats(ctx, &stats);

    EXPECT_EQ(stats.matches_performed, 1);
    EXPECT_GT(stats.grover_total_iterations, 0);
    EXPECT_EQ(stats.stored_templates, 1);
}

TEST_F(QSeqStatsTest, GetStatsNull) {
    EXPECT_EQ(qseq_matcher_get_stats(nullptr, nullptr), -1);
}

//=============================================================================
// Amplitude Encoding Tests
//=============================================================================

class QSeqAmplitudeTest : public ::testing::Test {};

TEST_F(QSeqAmplitudeTest, EncodedPatternNormalized) {
    uint32_t symbols[] = {10, 20, 30, 40};
    qseq_pattern_t pattern = qseq_create_pattern(symbols, nullptr, 4);
    pattern.elements[0].weight = 1.0f;
    pattern.elements[1].weight = 1.0f;
    pattern.elements[2].weight = 1.0f;
    pattern.elements[3].weight = 1.0f;

    float amplitude[64] = {0};
    qseq_encode_amplitude(&pattern, amplitude, 64);

    // Check normalization
    float norm = 0.0f;
    for (int i = 0; i < 64; i++) {
        norm += amplitude[i] * amplitude[i];
    }
    EXPECT_NEAR(norm, 1.0f, 1e-4);
}

TEST_F(QSeqAmplitudeTest, FidelitySelf) {
    float amplitude[64];
    for (int i = 0; i < 64; i++) {
        amplitude[i] = (float)i / 64.0f;
    }
    // Normalize
    float norm = 0.0f;
    for (int i = 0; i < 64; i++) norm += amplitude[i] * amplitude[i];
    norm = sqrtf(norm);
    for (int i = 0; i < 64; i++) amplitude[i] /= norm;

    float fidelity = qseq_fidelity(amplitude, amplitude, 64);
    EXPECT_NEAR(fidelity, 1.0f, 1e-5);
}

TEST_F(QSeqAmplitudeTest, FidelityOrthogonal) {
    float a[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float b[4] = {0.0f, 1.0f, 0.0f, 0.0f};

    float fidelity = qseq_fidelity(a, b, 4);
    EXPECT_NEAR(fidelity, 0.0f, 1e-5);
}

//=============================================================================
// Edge Cases
//=============================================================================

class QSeqEdgeCasesTest : public ::testing::Test {};

TEST_F(QSeqEdgeCasesTest, EmptyPattern) {
    auto ctx = qseq_matcher_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    qseq_pattern_t empty = {0};
    empty.length = 0;

    uint32_t id;
    int result = qseq_matcher_add_template(ctx, &empty, &id);
    EXPECT_EQ(result, 0);  // Should succeed but pattern has 0 elements

    qseq_match_result_t match_result;
    qseq_matcher_match(ctx, &empty, &match_result);
    // Should not crash

    qseq_matcher_destroy(ctx);
}

TEST_F(QSeqEdgeCasesTest, SingleElementPattern) {
    auto ctx = qseq_matcher_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint32_t symbol = 42;
    qseq_pattern_t pattern = qseq_create_pattern(&symbol, nullptr, 1);

    uint32_t id;
    int result = qseq_matcher_add_template(ctx, &pattern, &id);
    EXPECT_EQ(result, 0);

    qseq_match_result_t match_result;
    qseq_matcher_match(ctx, &pattern, &match_result);
    EXPECT_GT(match_result.similarity, 0.0f);

    qseq_matcher_destroy(ctx);
}

TEST_F(QSeqEdgeCasesTest, MaxLengthPattern) {
    auto ctx = qseq_matcher_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    std::vector<uint32_t> symbols(QSEQ_MAX_PATTERN_LENGTH);
    for (size_t i = 0; i < QSEQ_MAX_PATTERN_LENGTH; i++) {
        symbols[i] = i;
    }

    qseq_pattern_t pattern = qseq_create_pattern(symbols.data(), nullptr, QSEQ_MAX_PATTERN_LENGTH);

    uint32_t id;
    int result = qseq_matcher_add_template(ctx, &pattern, &id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(pattern.length, QSEQ_MAX_PATTERN_LENGTH);

    qseq_matcher_destroy(ctx);
}

TEST_F(QSeqEdgeCasesTest, TemplateCapacityFull) {
    qseq_matcher_config_t config = qseq_matcher_default_config();
    config.max_templates = 3;

    auto ctx = qseq_matcher_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Fill to capacity
    for (int i = 0; i < 3; i++) {
        uint32_t symbol = i;
        qseq_pattern_t pattern = qseq_create_pattern(&symbol, nullptr, 1);
        int result = qseq_matcher_add_template(ctx, &pattern, nullptr);
        EXPECT_EQ(result, 0);
    }

    // Try to add one more
    uint32_t symbol = 999;
    qseq_pattern_t pattern = qseq_create_pattern(&symbol, nullptr, 1);
    int result = qseq_matcher_add_template(ctx, &pattern, nullptr);
    EXPECT_EQ(result, -2);  // Full

    qseq_matcher_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
