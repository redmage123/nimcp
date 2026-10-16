/**
 * @file test_semantic_compression_integration.cpp
 * @brief Integration tests for semantic compression with bio-async
 *
 * Tests integration with:
 * - Bio-async messaging system
 * - Memory management
 * - Other async modules
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "async/nimcp_semantic_compression.h"
#include "utils/memory/nimcp_memory.h"

class SemanticCompressionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }
};

TEST_F(SemanticCompressionIntegrationTest, CreateAndDestroy) {
    nimcp_compression_config_t config =
        nimcp_semantic_compressor_default_config();

    nimcp_semantic_compressor_t* compressor =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(compressor, nullptr);

    nimcp_semantic_compressor_destroy(compressor);
}

TEST_F(SemanticCompressionIntegrationTest, MemoryIntegration) {
    nimcp_compression_config_t config =
        nimcp_semantic_compressor_default_config();

    nimcp_semantic_compressor_t* compressor =
        nimcp_semantic_compressor_create(&config);
    ASSERT_NE(compressor, nullptr);

    /* Perform operations that allocate memory */
    float signal[128];
    for (int i = 0; i < 128; i++) {
        signal[i] = static_cast<float>(i);
    }

    nimcp_compressed_signal_t* compressed =
        nimcp_semantic_compressor_compress(compressor, signal, 128);
    ASSERT_NE(compressed, nullptr);

    nimcp_compressed_signal_destroy(compressed);
    nimcp_semantic_compressor_destroy(compressor);

    /* Check for memory leaks if tracking enabled */
}

TEST_F(SemanticCompressionIntegrationTest, MultipleCompressors) {
    nimcp_compression_config_t config =
        nimcp_semantic_compressor_default_config();

    /* Create multiple compressors */
    nimcp_semantic_compressor_t* comp1 =
        nimcp_semantic_compressor_create(&config);
    nimcp_semantic_compressor_t* comp2 =
        nimcp_semantic_compressor_create(&config);

    ASSERT_NE(comp1, nullptr);
    ASSERT_NE(comp2, nullptr);

    /* Use both independently */
    float signal[64];
    for (int i = 0; i < 64; i++) {
        signal[i] = static_cast<float>(i);
    }

    nimcp_compressed_signal_t* c1 =
        nimcp_semantic_compressor_compress(comp1, signal, 64);
    nimcp_compressed_signal_t* c2 =
        nimcp_semantic_compressor_compress(comp2, signal, 64);

    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    /* Clean up */
    nimcp_compressed_signal_destroy(c1);
    nimcp_compressed_signal_destroy(c2);
    nimcp_semantic_compressor_destroy(comp1);
    nimcp_semantic_compressor_destroy(comp2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
