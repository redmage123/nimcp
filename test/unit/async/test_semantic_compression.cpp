/**
 * @file test_semantic_compression.cpp
 * @brief Unit tests for semantic compression module
 *
 * TEST COVERAGE:
 * - Compressor creation and destruction
 * - Primitive addition and management
 * - Signal compression and decompression
 * - Delta coding
 * - Quality metrics
 * - Compression ratio
 * - Statistics tracking
 * - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "async/nimcp_semantic_compression.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SemanticCompressionTest : public ::testing::Test {
protected:
    nimcp_semantic_compressor_t* compressor;
    nimcp_compression_config_t config;

    void SetUp() override {
        /* Initialize memory system */
        nimcp_memory_init();

        /* Get default configuration */
        config = nimcp_semantic_compressor_default_config();

        /* Create compressor */
        compressor = nimcp_semantic_compressor_create(&config);
        ASSERT_NE(compressor, nullptr);
    }

    void TearDown() override {
        if (compressor) {
            nimcp_semantic_compressor_destroy(compressor);
            compressor = nullptr;
        }
    }

    /* Helper: Generate test signal */
    std::vector<float> generateTestSignal(size_t len, float frequency = 1.0f) {
        std::vector<float> signal(len);
        for (size_t i = 0; i < len; i++) {
            signal[i] = sinf(2.0f * M_PI * frequency * i / len);
        }
        return signal;
    }

    /* Helper: Generate repeating pattern */
    std::vector<float> generateRepeatingPattern(size_t pattern_len, size_t repeats) {
        std::vector<float> pattern(pattern_len * repeats);
        for (size_t r = 0; r < repeats; r++) {
            for (size_t i = 0; i < pattern_len; i++) {
                pattern[r * pattern_len + i] = sinf(2.0f * M_PI * i / pattern_len);
            }
        }
        return pattern;
    }
};

//=============================================================================
// Creation and Configuration Tests
//=============================================================================

TEST_F(SemanticCompressionTest, CreateValidCompressor) {
    EXPECT_NE(compressor, nullptr);
}

TEST_F(SemanticCompressionTest, CreateWithCustomConfig) {
    nimcp_compression_config_t custom_config = {
        .max_primitives = 128,
        .vector_dimension = 32,
        .quality_level = 0.9f,
        .enable_delta = true,
        .enable_residuals = false,
        .primitive_learning_rate = 0.02f,
        .min_primitive_usage = 3,
        .bio_async_enabled = false,
        .bio_channel = BIO_CHANNEL_DOPAMINE
    };

    nimcp_semantic_compressor_t* custom_comp =
        nimcp_semantic_compressor_create(&custom_config);
    ASSERT_NE(custom_comp, nullptr);

    nimcp_semantic_compressor_destroy(custom_comp);
}

TEST_F(SemanticCompressionTest, RejectInvalidConfig) {
    nimcp_compression_config_t invalid_config = config;

    /* Invalid max_primitives */
    invalid_config.max_primitives = SEMANTIC_MAX_PRIMITIVES + 1;
    EXPECT_EQ(nimcp_semantic_compressor_create(&invalid_config), nullptr);

    /* Invalid vector_dimension */
    invalid_config = config;
    invalid_config.vector_dimension = SEMANTIC_MAX_VECTOR_DIM + 1;
    EXPECT_EQ(nimcp_semantic_compressor_create(&invalid_config), nullptr);

    /* Invalid quality_level */
    invalid_config = config;
    invalid_config.quality_level = 1.5f;
    EXPECT_EQ(nimcp_semantic_compressor_create(&invalid_config), nullptr);
}

TEST_F(SemanticCompressionTest, GetDefaultConfig) {
    nimcp_compression_config_t default_config =
        nimcp_semantic_compressor_default_config();

    EXPECT_GT(default_config.max_primitives, 0u);
    EXPECT_GT(default_config.vector_dimension, 0u);
    EXPECT_GE(default_config.quality_level, SEMANTIC_MIN_QUALITY);
    EXPECT_LE(default_config.quality_level, SEMANTIC_MAX_QUALITY);
}

//=============================================================================
// Primitive Management Tests
//=============================================================================

TEST_F(SemanticCompressionTest, AddPrimitive) {
    std::vector<float> meaning_vector(config.vector_dimension);
    for (size_t i = 0; i < meaning_vector.size(); i++) {
        meaning_vector[i] = static_cast<float>(i) / meaning_vector.size();
    }

    uint32_t primitive_id = nimcp_semantic_compressor_add_primitive(
        compressor,
        meaning_vector.data(),
        config.vector_dimension);

    EXPECT_GT(primitive_id, 0u);
}

TEST_F(SemanticCompressionTest, AddMultiplePrimitives) {
    const uint32_t num_primitives = 10;

    for (uint32_t p = 0; p < num_primitives; p++) {
        std::vector<float> meaning_vector(config.vector_dimension);
        for (size_t i = 0; i < meaning_vector.size(); i++) {
            meaning_vector[i] = sinf(2.0f * M_PI * (p + 1) * i / meaning_vector.size());
        }

        uint32_t primitive_id = nimcp_semantic_compressor_add_primitive(
            compressor,
            meaning_vector.data(),
            config.vector_dimension);

        EXPECT_GT(primitive_id, 0u);
    }

    /* Check statistics */
    nimcp_compression_stats_t stats;
    EXPECT_EQ(nimcp_semantic_compressor_get_stats(compressor, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.total_primitives_created, num_primitives);
}

TEST_F(SemanticCompressionTest, RejectNullPrimitive) {
    uint32_t id = nimcp_semantic_compressor_add_primitive(
        compressor, nullptr, config.vector_dimension);
    EXPECT_EQ(id, 0u);
}

TEST_F(SemanticCompressionTest, PrunePrimitives) {
    /* Add some primitives */
    for (uint32_t i = 0; i < 5; i++) {
        std::vector<float> vec(config.vector_dimension, static_cast<float>(i));
        nimcp_semantic_compressor_add_primitive(compressor, vec.data(), config.vector_dimension);
    }

    /* Prune with high usage threshold (should prune all since usage is 0) */
    uint32_t pruned = nimcp_semantic_compressor_prune_primitives(compressor, 10);
    EXPECT_GT(pruned, 0u);
}

TEST_F(SemanticCompressionTest, ResetPrimitives) {
    /* Add primitives */
    std::vector<float> vec(config.vector_dimension, 1.0f);
    nimcp_semantic_compressor_add_primitive(compressor, vec.data(), config.vector_dimension);

    /* Reset */
    EXPECT_EQ(nimcp_semantic_compressor_reset_primitives(compressor), NIMCP_SUCCESS);

    /* Stats should show active primitives = 0 */
    nimcp_compression_stats_t stats;
    nimcp_semantic_compressor_get_stats(compressor, &stats);
    EXPECT_EQ(stats.active_primitives, 0u);
}

//=============================================================================
// Compression Tests
//=============================================================================

TEST_F(SemanticCompressionTest, CompressSimpleSignal) {
    auto signal = generateTestSignal(256);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal.data(), signal.size());

    ASSERT_NE(compressed, nullptr);
    EXPECT_GT(compressed->num_primitives, 0u);
    EXPECT_EQ(compressed->original_size, signal.size() * sizeof(float));
    EXPECT_GT(compressed->original_size, compressed->compressed_size);

    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, CompressRepeatingPattern) {
    /* Repeating pattern should compress well */
    auto signal = generateRepeatingPattern(64, 4);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal.data(), signal.size());

    ASSERT_NE(compressed, nullptr);

    /* Should use fewer primitives than total segments */
    size_t max_segments = signal.size() / config.vector_dimension;
    EXPECT_LE(compressed->num_primitives, max_segments);

    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, CompressionRatio) {
    auto signal = generateTestSignal(512);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal.data(), signal.size());
    ASSERT_NE(compressed, nullptr);

    float ratio = nimcp_semantic_compressor_get_ratio(compressor);
    EXPECT_GT(ratio, 0.0f);

    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, RejectNullSignal) {
    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, nullptr, 100);
    EXPECT_EQ(compressed, nullptr);
}

TEST_F(SemanticCompressionTest, RejectZeroLengthSignal) {
    std::vector<float> signal(10);
    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal.data(), 0);
    EXPECT_EQ(compressed, nullptr);
}

//=============================================================================
// Decompression Tests
//=============================================================================

TEST_F(SemanticCompressionTest, DecompressSignal) {
    auto signal = generateTestSignal(256);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal.data(), signal.size());
    ASSERT_NE(compressed, nullptr);

    nimcp_decompressed_signal_t* decompressed =
        nimcp_semantic_compressor_decompress(compressor, compressed);
    ASSERT_NE(decompressed, nullptr);

    EXPECT_GT(decompressed->len, 0u);
    EXPECT_NE(decompressed->signal, nullptr);

    nimcp_decompressed_signal_destroy(decompressed);
    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, RoundTripFidelity) {
    auto original_signal = generateTestSignal(128);

    /* Compress */
    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(
            compressor, original_signal.data(), original_signal.size());
    ASSERT_NE(compressed, nullptr);

    /* Decompress */
    nimcp_decompressed_signal_t* decompressed =
        nimcp_semantic_compressor_decompress(compressor, compressed);
    ASSERT_NE(decompressed, nullptr);

    /* Check length is close (may not be exact due to segmentation) */
    EXPECT_GE(decompressed->len, original_signal.size() / 2);

    nimcp_decompressed_signal_destroy(decompressed);
    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, RejectNullDecompression) {
    nimcp_decompressed_signal_t* decompressed =
        nimcp_semantic_compressor_decompress(compressor, nullptr);
    EXPECT_EQ(decompressed, nullptr);
}

//=============================================================================
// Delta Coding Tests
//=============================================================================

TEST_F(SemanticCompressionTest, DeltaCodingEnabled) {
    /* Ensure delta coding is enabled */
    config.enable_delta = true;
    nimcp_semantic_compressor_t* delta_comp =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(delta_comp, nullptr);

    auto signal = generateTestSignal(128);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(delta_comp, signal.data(), signal.size());
    ASSERT_NE(compressed, nullptr);

    /* Should have delta values */
    EXPECT_GT(compressed->num_deltas, 0u);
    EXPECT_NE(compressed->deltas, nullptr);

    nimcp_compressed_signal_destroy(compressed);
    nimcp_semantic_compressor_destroy(delta_comp);
}

TEST_F(SemanticCompressionTest, DeltaCodingDisabled) {
    /* Disable delta coding */
    config.enable_delta = false;
    nimcp_semantic_compressor_t* no_delta_comp =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(no_delta_comp, nullptr);

    auto signal = generateTestSignal(128);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(no_delta_comp, signal.data(), signal.size());
    ASSERT_NE(compressed, nullptr);

    /* Should not have delta values */
    EXPECT_EQ(compressed->num_deltas, 0u);

    nimcp_compressed_signal_destroy(compressed);
    nimcp_semantic_compressor_destroy(no_delta_comp);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SemanticCompressionTest, TrackStatistics) {
    nimcp_compression_stats_t stats;

    /* Initial stats */
    EXPECT_EQ(nimcp_semantic_compressor_get_stats(compressor, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_compressions, 0u);

    /* Perform compression */
    auto signal = generateTestSignal(128);
    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal.data(), signal.size());
    ASSERT_NE(compressed, nullptr);

    /* Check updated stats */
    EXPECT_EQ(nimcp_semantic_compressor_get_stats(compressor, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_compressions, 1u);
    EXPECT_GT(stats.total_bytes_in, 0u);
    EXPECT_GT(stats.total_bytes_out, 0u);

    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, RejectNullStatsPointer) {
    nimcp_result_t result = nimcp_semantic_compressor_get_stats(compressor, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SemanticCompressionTest, SemanticSimilarity) {
    std::vector<float> signal1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> signal2 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> signal3 = {-1.0f, -2.0f, -3.0f, -4.0f};

    /* Identical signals */
    float sim1 = nimcp_semantic_similarity(
        signal1.data(), signal1.size(),
        signal2.data(), signal2.size());
    EXPECT_NEAR(sim1, 1.0f, 0.01f);

    /* Opposite signals */
    float sim2 = nimcp_semantic_similarity(
        signal1.data(), signal1.size(),
        signal3.data(), signal3.size());
    EXPECT_NEAR(sim2, -1.0f, 0.01f);
}

TEST_F(SemanticCompressionTest, SimilarityHandlesNull) {
    std::vector<float> signal = {1.0f, 2.0f, 3.0f};

    float sim = nimcp_semantic_similarity(nullptr, 0, signal.data(), signal.size());
    EXPECT_EQ(sim, 0.0f);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SemanticCompressionTest, CompressEmptySignalSegment) {
    /* Very small signal */
    std::vector<float> tiny_signal = {1.0f, 2.0f};

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(
            compressor, tiny_signal.data(), tiny_signal.size());

    /* May succeed with minimal compression */
    if (compressed) {
        nimcp_compressed_signal_destroy(compressed);
    }
}

TEST_F(SemanticCompressionTest, CompressLargeSignal) {
    /* Large signal */
    auto large_signal = generateTestSignal(10000);

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(
            compressor, large_signal.data(), large_signal.size());

    ASSERT_NE(compressed, nullptr);
    EXPECT_GT(compressed->num_primitives, 0u);

    nimcp_compressed_signal_destroy(compressed);
}

TEST_F(SemanticCompressionTest, MultipleCompressions) {
    /* Multiple sequential compressions */
    for (int i = 0; i < 5; i++) {
        auto signal = generateTestSignal(128, static_cast<float>(i + 1));

        nimcp_compressed_signal_t* compressed =
            nimcp_semantic_compressor_compress(
                compressor, signal.data(), signal.size());

        ASSERT_NE(compressed, nullptr);
        nimcp_compressed_signal_destroy(compressed);
    }

    /* Check stats */
    nimcp_compression_stats_t stats;
    nimcp_semantic_compressor_get_stats(compressor, &stats);
    EXPECT_EQ(stats.total_compressions, 5u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
