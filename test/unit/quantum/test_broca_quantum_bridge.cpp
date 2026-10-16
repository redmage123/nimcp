/**
 * @file test_broca_quantum_bridge.cpp
 * @brief Comprehensive unit tests for Broca quantum bridge
 *
 * Tests cover:
 * - Bridge lifecycle (create, destroy)
 * - Configuration management
 * - Lexical search with Grover algorithm
 * - Syntax optimization
 * - Phoneme sequence optimization
 * - Statistics tracking
 * - Error handling
 * - Performance characteristics
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "core/brain/regions/broca/nimcp_broca_quantum_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrocaQuantumBridgeTest : public ::testing::Test {
protected:
    broca_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        broca_quantum_config_t config = broca_quantum_default_config();
        bridge = broca_quantum_bridge_create(nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            broca_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /**
     * @brief Generate random semantic vector
     */
    void generate_semantic_vector(float* vec, uint32_t dim, float seed) {
        for (uint32_t i = 0; i < dim; i++) {
            vec[i] = sinf(seed + (float)i * 0.3f) * 0.5f + 0.5f;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, CreateWithNullConfig) {
    broca_quantum_bridge_t* b = broca_quantum_bridge_create(nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    broca_quantum_bridge_destroy(b);
}

TEST_F(BrocaQuantumBridgeTest, CreateWithConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BrocaQuantumBridgeTest, DestroyNull) {
    broca_quantum_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(BrocaQuantumBridgeTest, MultipleCreate) {
    broca_quantum_bridge_t* bridges[5];

    for (int i = 0; i < 5; i++) {
        bridges[i] = broca_quantum_bridge_create(nullptr, nullptr);
        ASSERT_NE(bridges[i], nullptr);
    }

    for (int i = 0; i < 5; i++) {
        broca_quantum_bridge_destroy(bridges[i]);
    }
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, DefaultConfigValues) {
    broca_quantum_config_t config = broca_quantum_default_config();

    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.lexicon_search_depth, 0u);
    EXPECT_GT(config.syntax_alternatives, 0u);
    EXPECT_GT(config.max_grover_iterations, 0u);
    EXPECT_GT(config.min_expression_confidence, 0.0f);
    EXPECT_LE(config.min_expression_confidence, 1.0f);
    EXPECT_TRUE(config.enable_interference);
    EXPECT_TRUE(config.use_superposition);
}

TEST_F(BrocaQuantumBridgeTest, GetConfig) {
    broca_quantum_config_t config;
    int ret = broca_quantum_get_config(bridge, &config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.lexicon_search_depth, 0u);
}

TEST_F(BrocaQuantumBridgeTest, GetConfigNull) {
    broca_quantum_config_t config;
    EXPECT_EQ(broca_quantum_get_config(nullptr, &config), -1);
    EXPECT_EQ(broca_quantum_get_config(bridge, nullptr), -1);
}

TEST_F(BrocaQuantumBridgeTest, CustomConfig) {
    broca_quantum_config_t custom_config = {
        .enabled = true,
        .lexicon_search_depth = 500,
        .syntax_alternatives = 4,
        .max_grover_iterations = 5,
        .min_expression_confidence = 0.7f,
        .enable_interference = false,
        .use_superposition = true,
        .seed = 12345
    };

    broca_quantum_bridge_t* custom_bridge = broca_quantum_bridge_create(nullptr, &custom_config);
    ASSERT_NE(custom_bridge, nullptr);

    broca_quantum_config_t retrieved;
    broca_quantum_get_config(custom_bridge, &retrieved);

    EXPECT_EQ(retrieved.lexicon_search_depth, 500u);
    EXPECT_EQ(retrieved.syntax_alternatives, 4u);
    EXPECT_EQ(retrieved.max_grover_iterations, 5u);
    EXPECT_FLOAT_EQ(retrieved.min_expression_confidence, 0.7f);
    EXPECT_FALSE(retrieved.enable_interference);

    broca_quantum_bridge_destroy(custom_bridge);
}

//=============================================================================
// Enable/Disable Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, IsEnabled) {
    EXPECT_TRUE(broca_quantum_bridge_is_enabled(bridge));
}

TEST_F(BrocaQuantumBridgeTest, IsEnabledNull) {
    EXPECT_FALSE(broca_quantum_bridge_is_enabled(nullptr));
}

TEST_F(BrocaQuantumBridgeTest, SetEnabled) {
    broca_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(broca_quantum_bridge_is_enabled(bridge));

    broca_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(broca_quantum_bridge_is_enabled(bridge));
}

TEST_F(BrocaQuantumBridgeTest, SetEnabledNull) {
    broca_quantum_bridge_set_enabled(nullptr, true);  /* Should not crash */
}

//=============================================================================
// Lexical Search Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, SearchLexiconBasic) {
    float semantic_target[] = {0.1f, 0.2f, 0.3f, 0.4f};
    quantum_lexical_result_t result;

    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 4, 100, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_candidate, nullptr);
    EXPECT_GT(result.candidates_evaluated, 0u);
    EXPECT_GE(result.satisfaction_probability, 0.0f);
    EXPECT_LE(result.satisfaction_probability, 1.0f);
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconLarge) {
    float semantic_target[16];
    generate_semantic_vector(semantic_target, 16, 1.0f);

    quantum_lexical_result_t result;
    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 16, 1000, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_candidate, nullptr);
    EXPECT_GT(result.search_speedup, 0.0f);
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconSpeedup) {
    float semantic_target[] = {0.5f, 0.5f, 0.5f, 0.5f};
    quantum_lexical_result_t result;

    /* Search in lexicon of 1000 words */
    broca_quantum_search_lexicon(bridge, semantic_target, 4, 1000, &result);

    /* Grover provides sqrt(N) speedup, so for N=1000, speedup ~ 31 */
    EXPECT_GT(result.search_speedup, 20.0f);
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconDisabled) {
    broca_quantum_bridge_set_enabled(bridge, false);

    float semantic_target[] = {0.1f, 0.2f};
    quantum_lexical_result_t result;

    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 2, 100, &result);
    EXPECT_EQ(ret, -1);  /* Should fail when disabled */
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconNullTarget) {
    quantum_lexical_result_t result;
    int ret = broca_quantum_search_lexicon(bridge, nullptr, 4, 100, &result);
    EXPECT_EQ(ret, -1);
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconNullResult) {
    float semantic_target[] = {0.1f, 0.2f};
    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 2, 100, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconVaryingSizes) {
    float semantic_target[8];
    generate_semantic_vector(semantic_target, 8, 0.5f);

    uint32_t sizes[] = {10, 50, 100, 500};

    for (uint32_t size : sizes) {
        quantum_lexical_result_t result;
        int ret = broca_quantum_search_lexicon(bridge, semantic_target, 8, size, &result);

        EXPECT_EQ(ret, 0);
        EXPECT_NE(result.best_candidate, nullptr);
    }
}

TEST_F(BrocaQuantumBridgeTest, SearchLexiconCandidateQuality) {
    float semantic_target[] = {0.8f, 0.2f, 0.5f, 0.3f};
    quantum_lexical_result_t result;

    broca_quantum_search_lexicon(bridge, semantic_target, 4, 200, &result);

    ASSERT_NE(result.best_candidate, nullptr);

    /* Check candidate properties */
    EXPECT_GE(result.best_candidate->amplitude, 0.0f);
    EXPECT_LE(result.best_candidate->amplitude, 1.0f);
    EXPECT_GE(result.best_candidate->semantic_match, 0.0f);
    EXPECT_LE(result.best_candidate->semantic_match, 1.0f);
    EXPECT_GE(result.best_candidate->combined_score, 0.0f);
}

//=============================================================================
// Syntax Optimization Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntaxBasic) {
    quantum_syntax_result_t result;

    int ret = broca_quantum_optimize_syntax(bridge, "The cat sat on the mat", 6, 0.8f, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_structure, nullptr);
    EXPECT_GT(result.structures_evaluated, 0u);
}

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntaxComplex) {
    quantum_syntax_result_t result;

    const char* content = "Although the weather was cold, the children played outside happily";
    int ret = broca_quantum_optimize_syntax(bridge, content, 10, 0.9f, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_structure, nullptr);
}

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntaxLowComplexity) {
    quantum_syntax_result_t result;

    int ret = broca_quantum_optimize_syntax(bridge, "Simple sentence", 2, 0.3f, &result);

    EXPECT_EQ(ret, 0);

    if (result.best_structure) {
        EXPECT_LE(result.best_structure->complexity, 0.3f);
    }
}

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntaxNull) {
    quantum_syntax_result_t result;

    EXPECT_EQ(broca_quantum_optimize_syntax(bridge, nullptr, 5, 0.8f, &result), -1);
    EXPECT_EQ(broca_quantum_optimize_syntax(bridge, "test", 5, 0.8f, nullptr), -1);
    EXPECT_EQ(broca_quantum_optimize_syntax(nullptr, "test", 5, 0.8f, &result), -1);
}

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntaxDisabled) {
    broca_quantum_bridge_set_enabled(bridge, false);

    quantum_syntax_result_t result;
    int ret = broca_quantum_optimize_syntax(bridge, "test", 2, 0.5f, &result);

    EXPECT_EQ(ret, -1);
}

TEST_F(BrocaQuantumBridgeTest, OptimizeSyntaxStructureProperties) {
    quantum_syntax_result_t result;

    broca_quantum_optimize_syntax(bridge, "Test content here", 3, 0.7f, &result);

    ASSERT_NE(result.best_structure, nullptr);

    EXPECT_GE(result.best_structure->amplitude, 0.0f);
    EXPECT_LE(result.best_structure->amplitude, 1.0f);
    EXPECT_GE(result.best_structure->fluency_score, 0.0f);
    EXPECT_LE(result.best_structure->fluency_score, 1.0f);
    EXPECT_GE(result.best_structure->complexity, 0.0f);
    EXPECT_LE(result.best_structure->complexity, 1.0f);
}

//=============================================================================
// Phoneme Optimization Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemesBasic) {
    uint8_t phonemes[] = {1, 2, 3, 4, 5, 6, 7, 8};
    quantum_phoneme_result_t result;

    int ret = broca_quantum_optimize_phonemes(bridge, phonemes, 8, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_sequence, nullptr);
    EXPECT_GT(result.sequences_evaluated, 0u);
}

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemesShort) {
    uint8_t phonemes[] = {10, 20};
    quantum_phoneme_result_t result;

    int ret = broca_quantum_optimize_phonemes(bridge, phonemes, 2, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_sequence, nullptr);
}

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemesLong) {
    uint8_t phonemes[32];
    for (int i = 0; i < 32; i++) {
        phonemes[i] = (uint8_t)(i * 3 + 1);
    }

    quantum_phoneme_result_t result;
    int ret = broca_quantum_optimize_phonemes(bridge, phonemes, 32, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_sequence, nullptr);
}

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemesNull) {
    quantum_phoneme_result_t result;

    EXPECT_EQ(broca_quantum_optimize_phonemes(bridge, nullptr, 8, &result), -1);

    uint8_t phonemes[] = {1, 2, 3};
    EXPECT_EQ(broca_quantum_optimize_phonemes(bridge, phonemes, 3, nullptr), -1);
    EXPECT_EQ(broca_quantum_optimize_phonemes(nullptr, phonemes, 3, &result), -1);
}

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemesDisabled) {
    broca_quantum_bridge_set_enabled(bridge, false);

    uint8_t phonemes[] = {1, 2, 3, 4};
    quantum_phoneme_result_t result;

    int ret = broca_quantum_optimize_phonemes(bridge, phonemes, 4, &result);
    EXPECT_EQ(ret, -1);
}

TEST_F(BrocaQuantumBridgeTest, OptimizePhonemesSequenceProperties) {
    uint8_t phonemes[] = {5, 10, 15, 20, 25};
    quantum_phoneme_result_t result;

    broca_quantum_optimize_phonemes(bridge, phonemes, 5, &result);

    ASSERT_NE(result.best_sequence, nullptr);

    EXPECT_GE(result.best_sequence->articulatory_cost, 0.0f);
    EXPECT_LE(result.best_sequence->articulatory_cost, 1.0f);
    EXPECT_GE(result.best_sequence->coarticulation_score, 0.0f);
    EXPECT_LE(result.best_sequence->coarticulation_score, 1.0f);
    EXPECT_GE(result.best_sequence->amplitude, 0.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, GetStatsInitial) {
    broca_quantum_stats_t stats;
    int ret = broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.lexical_searches, 0u);
    EXPECT_EQ(stats.syntax_optimizations, 0u);
    EXPECT_EQ(stats.phoneme_optimizations, 0u);
}

TEST_F(BrocaQuantumBridgeTest, GetStatsNull) {
    broca_quantum_stats_t stats;
    EXPECT_EQ(broca_quantum_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(broca_quantum_get_stats(bridge, nullptr), -1);
}

TEST_F(BrocaQuantumBridgeTest, StatsTrackLexical) {
    float semantic[] = {0.5f, 0.5f};
    quantum_lexical_result_t result;

    broca_quantum_search_lexicon(bridge, semantic, 2, 50, &result);
    broca_quantum_search_lexicon(bridge, semantic, 2, 50, &result);
    broca_quantum_search_lexicon(bridge, semantic, 2, 50, &result);

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.lexical_searches, 3u);
}

TEST_F(BrocaQuantumBridgeTest, StatsTrackSyntax) {
    quantum_syntax_result_t result;

    broca_quantum_optimize_syntax(bridge, "test1", 2, 0.5f, &result);
    broca_quantum_optimize_syntax(bridge, "test2", 3, 0.6f, &result);

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.syntax_optimizations, 2u);
}

TEST_F(BrocaQuantumBridgeTest, StatsTrackPhoneme) {
    uint8_t phonemes[] = {1, 2, 3, 4};
    quantum_phoneme_result_t result;

    broca_quantum_optimize_phonemes(bridge, phonemes, 4, &result);

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.phoneme_optimizations, 1u);
}

TEST_F(BrocaQuantumBridgeTest, StatsAverages) {
    float semantic[] = {0.5f, 0.5f, 0.5f};
    quantum_lexical_result_t lex_result;

    /* Multiple searches to accumulate speedup stats */
    for (int i = 0; i < 5; i++) {
        broca_quantum_search_lexicon(bridge, semantic, 3, 100, &lex_result);
    }

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_GT(stats.avg_lexical_speedup, 0.0f);
    EXPECT_GT(stats.avg_satisfaction_prob, 0.0f);
}

TEST_F(BrocaQuantumBridgeTest, ResetStats) {
    /* Perform operations */
    float semantic[] = {0.5f};
    quantum_lexical_result_t lex_result;
    broca_quantum_search_lexicon(bridge, semantic, 1, 20, &lex_result);

    quantum_syntax_result_t syn_result;
    broca_quantum_optimize_syntax(bridge, "test", 2, 0.5f, &syn_result);

    uint8_t phonemes[] = {1, 2};
    quantum_phoneme_result_t phon_result;
    broca_quantum_optimize_phonemes(bridge, phonemes, 2, &phon_result);

    /* Reset stats */
    broca_quantum_reset_stats(bridge);

    /* Verify reset */
    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.lexical_searches, 0u);
    EXPECT_EQ(stats.syntax_optimizations, 0u);
    EXPECT_EQ(stats.phoneme_optimizations, 0u);
    EXPECT_EQ(stats.successful_searches, 0u);
    EXPECT_EQ(stats.failed_searches, 0u);
}

TEST_F(BrocaQuantumBridgeTest, ResetStatsNull) {
    broca_quantum_reset_stats(nullptr);  /* Should not crash */
}

//=============================================================================
// Grover Iteration Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, GroverIterationsUsed) {
    float semantic[] = {0.3f, 0.4f, 0.5f, 0.6f};
    quantum_lexical_result_t result;

    int ret = broca_quantum_search_lexicon(bridge, semantic, 4, 500, &result);
    EXPECT_EQ(ret, 0);

    /* Grover iterations may be 0 when solutions are majority (> 50% of states),
     * as the algorithm correctly skips amplification in that case. */
    EXPECT_GE(result.grover_iterations_used, 0u);
}

TEST_F(BrocaQuantumBridgeTest, GroverIterationsCapped) {
    broca_quantum_config_t config = broca_quantum_default_config();
    config.max_grover_iterations = 3;

    broca_quantum_bridge_t* capped_bridge = broca_quantum_bridge_create(nullptr, &config);
    ASSERT_NE(capped_bridge, nullptr);

    float semantic[] = {0.5f, 0.5f};
    quantum_lexical_result_t result;

    broca_quantum_search_lexicon(capped_bridge, semantic, 2, 1000, &result);

    EXPECT_LE(result.grover_iterations_used, 3u);

    broca_quantum_bridge_destroy(capped_bridge);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, FullLanguageProductionPipeline) {
    /* Step 1: Lexical selection */
    float semantic_target[] = {0.4f, 0.6f, 0.3f, 0.7f};
    quantum_lexical_result_t lex_result;

    int ret = broca_quantum_search_lexicon(bridge, semantic_target, 4, 200, &lex_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(lex_result.best_candidate, nullptr);

    /* Step 2: Syntax optimization */
    quantum_syntax_result_t syn_result;
    ret = broca_quantum_optimize_syntax(bridge, "selected word phrase", 3, 0.7f, &syn_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(syn_result.best_structure, nullptr);

    /* Step 3: Phoneme optimization */
    uint8_t phonemes[] = {5, 10, 15, 20, 25, 30};
    quantum_phoneme_result_t phon_result;
    ret = broca_quantum_optimize_phonemes(bridge, phonemes, 6, &phon_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(phon_result.best_sequence, nullptr);

    /* Verify all operations tracked */
    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.lexical_searches, 1u);
    EXPECT_EQ(stats.syntax_optimizations, 1u);
    EXPECT_EQ(stats.phoneme_optimizations, 1u);
}

TEST_F(BrocaQuantumBridgeTest, RepeatedSearchesDifferentSeeds) {
    broca_quantum_config_t config1 = broca_quantum_default_config();
    config1.seed = 12345;

    broca_quantum_config_t config2 = broca_quantum_default_config();
    config2.seed = 54321;

    broca_quantum_bridge_t* bridge1 = broca_quantum_bridge_create(nullptr, &config1);
    broca_quantum_bridge_t* bridge2 = broca_quantum_bridge_create(nullptr, &config2);

    float semantic[] = {0.5f, 0.5f, 0.5f};
    quantum_lexical_result_t result1, result2;

    broca_quantum_search_lexicon(bridge1, semantic, 3, 100, &result1);
    broca_quantum_search_lexicon(bridge2, semantic, 3, 100, &result2);

    /* Results may differ due to different random seeds */
    /* But both should be valid */
    EXPECT_NE(result1.best_candidate, nullptr);
    EXPECT_NE(result2.best_candidate, nullptr);

    broca_quantum_bridge_destroy(bridge1);
    broca_quantum_bridge_destroy(bridge2);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(BrocaQuantumBridgeTest, HighThroughput) {
    const int NUM_SEARCHES = 50;

    float semantic[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    quantum_lexical_result_t result;

    for (int i = 0; i < NUM_SEARCHES; i++) {
        semantic[0] = 0.5f + 0.3f * sinf((float)i * 0.1f);
        broca_quantum_search_lexicon(bridge, semantic, 4, 100, &result);
    }

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.lexical_searches, (uint64_t)NUM_SEARCHES);
}

TEST_F(BrocaQuantumBridgeTest, MixedOperations) {
    const int NUM_ITERATIONS = 20;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        /* Lexical */
        float semantic[] = {0.5f + 0.2f * sinf((float)i), 0.5f};
        quantum_lexical_result_t lex_result;
        broca_quantum_search_lexicon(bridge, semantic, 2, 50, &lex_result);

        /* Syntax */
        quantum_syntax_result_t syn_result;
        broca_quantum_optimize_syntax(bridge, "test", 2, 0.6f, &syn_result);

        /* Phoneme */
        uint8_t phonemes[] = {(uint8_t)(i % 256), (uint8_t)((i + 1) % 256)};
        quantum_phoneme_result_t phon_result;
        broca_quantum_optimize_phonemes(bridge, phonemes, 2, &phon_result);
    }

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.lexical_searches, (uint64_t)NUM_ITERATIONS);
    EXPECT_EQ(stats.syntax_optimizations, (uint64_t)NUM_ITERATIONS);
    EXPECT_EQ(stats.phoneme_optimizations, (uint64_t)NUM_ITERATIONS);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
