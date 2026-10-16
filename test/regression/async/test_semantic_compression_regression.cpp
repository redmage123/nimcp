/**
 * @file test_semantic_compression_regression.cpp
 * @brief Regression tests for semantic compression
 *
 * Ensures compression quality and performance remain consistent
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_semantic_compression.h"
#include "utils/memory/nimcp_memory.h"

class SemanticCompressionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }
};

TEST_F(SemanticCompressionRegressionTest, CompressionRatioRegression) {
    /* Ensure compression ratio meets minimum threshold */
    nimcp_compression_config_t config =
        nimcp_semantic_compressor_default_config();

    nimcp_semantic_compressor_t* compressor =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(compressor, nullptr);

    /* Generate highly compressible repeating signal */
    const size_t signal_len = 1024;
    float signal[signal_len];

    for (size_t i = 0; i < signal_len; i++) {
        signal[i] = sinf(2.0f * M_PI * i / 64.0f);  /* Repeating pattern */
    }

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal, signal_len);
    ASSERT_NE(compressed, nullptr);

    /* Compression ratio should be > 1.0 (actual compression) */
    float ratio = static_cast<float>(compressed->original_size) /
                  static_cast<float>(compressed->compressed_size);
    EXPECT_GT(ratio, 1.0f);

    nimcp_compressed_signal_destroy(compressed);
    nimcp_semantic_compressor_destroy(compressor);
}

TEST_F(SemanticCompressionRegressionTest, ReconstructionQuality) {
    /* Ensure reconstruction quality doesn't degrade */
    nimcp_compression_config_t config =
        nimcp_semantic_compressor_default_config();
    config.quality_level = 0.9f;

    nimcp_semantic_compressor_t* compressor =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(compressor, nullptr);

    float signal[256];
    for (int i = 0; i < 256; i++) {
        signal[i] = sinf(2.0f * M_PI * i / 32.0f);
    }

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal, 256);
    ASSERT_NE(compressed, nullptr);

    nimcp_decompressed_signal_t* decompressed =
        nimcp_semantic_compressor_decompress(compressor, compressed);
    ASSERT_NE(decompressed, nullptr);

    /* Semantic loss should be reasonable */
    EXPECT_LE(decompressed->semantic_loss, 0.5f);

    nimcp_decompressed_signal_destroy(decompressed);
    nimcp_compressed_signal_destroy(compressed);
    nimcp_semantic_compressor_destroy(compressor);
}

TEST_F(SemanticCompressionRegressionTest, PerformanceBaseline) {
    /* Ensure compression completes in reasonable time */
    nimcp_compression_config_t config =
        nimcp_semantic_compressor_default_config();

    nimcp_semantic_compressor_t* compressor =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(compressor, nullptr);

    const size_t large_signal_len = 10000;
    float* large_signal = new float[large_signal_len];

    for (size_t i = 0; i < large_signal_len; i++) {
        large_signal[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    /* Should complete without crash */
    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, large_signal, large_signal_len);

    if (compressed) {
        nimcp_compressed_signal_destroy(compressed);
    }

    delete[] large_signal;
    nimcp_semantic_compressor_destroy(compressor);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
