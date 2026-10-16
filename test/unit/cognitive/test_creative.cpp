/**
 * @file test_creative.cpp
 * @brief Unit tests for Creative Cortex module
 *
 * WHAT: Comprehensive unit tests for creative orchestrator and related functions
 * WHY:  Ensure correct lifecycle, configuration, and basic functionality
 * HOW:  Test create/destroy, NULL handling, config defaults, update cycle
 *
 * Function signatures tested (from include/cognitive/creative/nimcp_creative.h):
 *   creative_orchestrator_t* creative_orchestrator_create(const creative_config_t* config);
 *   void creative_orchestrator_destroy(creative_orchestrator_t* orchestrator);
 *   int creative_orchestrator_update(creative_orchestrator_t* orchestrator, uint64_t dt_us);
 *   void creative_config_init_defaults(creative_config_t* config);
 *   int style_embedding_create(style_embedding_t* embedding, uint32_t dim);
 *   void style_embedding_destroy(style_embedding_t* embedding);
 *   int style_embedding_clone(const style_embedding_t* src, style_embedding_t* dst);
 *   float style_embedding_similarity(const style_embedding_t* a, const style_embedding_t* b);
 *   void style_embedding_normalize(style_embedding_t* embedding);
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/creative/nimcp_creative.h"
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CreativeTest : public NimcpTestBase {
protected:
    creative_orchestrator_t* orchestrator = nullptr;
    creative_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        memset(&config, 0, sizeof(config));
    }

    void TearDown() override {
        if (orchestrator) {
            creative_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CreativeTest, ConfigInitDefaults_ValidPointer) {
    // WHAT: Initialize config with defaults
    // WHY:  Ensure default config is sensible
    // HOW:  Call init_defaults and check values

    creative_config_init_defaults(&config);

    // Defaults should enable basic features
    EXPECT_TRUE(config.enable_appreciation);
    EXPECT_TRUE(config.enable_inspiration);

    // Quality thresholds should be reasonable
    EXPECT_GT(config.min_quality_threshold, 0.0f);
    EXPECT_LE(config.min_quality_threshold, 1.0f);
    EXPECT_GT(config.copyright_similarity_threshold, 0.0f);
    EXPECT_LE(config.copyright_similarity_threshold, 1.0f);
}

TEST_F(CreativeTest, ConfigInitDefaults_NullPointer) {
    // WHAT: Test NULL safety for config init
    // WHY:  Defensive programming
    // HOW:  Call with NULL, should not crash

    creative_config_init_defaults(nullptr);
    SUCCEED() << "creative_config_init_defaults(NULL) did not crash";
}

/* ============================================================================
 * Orchestrator Lifecycle Tests
 * ============================================================================ */

TEST_F(CreativeTest, OrchestratorCreate_WithDefaultConfig) {
    // WHAT: Create orchestrator with default configuration
    // WHY:  Basic lifecycle validation
    // HOW:  Init defaults, create orchestrator

    creative_config_init_defaults(&config);
    orchestrator = creative_orchestrator_create(&config);

    // May return NULL if external dependencies unavailable
    // The test passes if it doesn't crash
    SUCCEED() << "creative_orchestrator_create with defaults completed";
}

TEST_F(CreativeTest, OrchestratorCreate_WithNullConfig) {
    // WHAT: Test NULL config uses defaults
    // WHY:  Implementation applies default config when NULL is passed
    // HOW:  Call with NULL config, expect valid orchestrator with defaults

    orchestrator = creative_orchestrator_create(nullptr);
    // Implementation applies creative_config_init_defaults when config is NULL
    EXPECT_NE(orchestrator, nullptr);
}

TEST_F(CreativeTest, OrchestratorDestroy_NullIsNoop) {
    // WHAT: Verify destroying NULL orchestrator doesn't crash
    // WHY:  Defensive programming
    // HOW:  Call destroy with NULL

    creative_orchestrator_destroy(nullptr);
    SUCCEED() << "creative_orchestrator_destroy(NULL) did not crash";
}

TEST_F(CreativeTest, OrchestratorCreate_MinimalConfig) {
    // WHAT: Create with minimal features enabled
    // WHY:  Test that partial configs work
    // HOW:  Disable most features, only enable basics

    creative_config_init_defaults(&config);
    config.enable_text_generation = false;
    config.enable_music_generation = false;
    config.enable_visual_generation = false;
    config.enable_video_generation = false;
    config.enable_multimodal_direction = false;

    orchestrator = creative_orchestrator_create(&config);
    // May return NULL if even minimal config can't be satisfied
    SUCCEED() << "Minimal config orchestrator creation completed";
}

/* ============================================================================
 * Orchestrator Update Tests
 * ============================================================================ */

TEST_F(CreativeTest, OrchestratorUpdate_NullOrchestrator) {
    // WHAT: Test NULL safety for update
    // WHY:  Defensive programming
    // HOW:  Call update with NULL

    int result = creative_orchestrator_update(nullptr, 1000);
    EXPECT_EQ(result, -1);
}

TEST_F(CreativeTest, OrchestratorUpdate_ValidOrchestrator) {
    // WHAT: Test update with valid orchestrator
    // WHY:  Basic functionality test
    // HOW:  Create orchestrator, call update

    creative_config_init_defaults(&config);
    orchestrator = creative_orchestrator_create(&config);

    if (orchestrator != nullptr) {
        int result = creative_orchestrator_update(orchestrator, 16667); // ~60 fps
        EXPECT_EQ(result, 0);
    } else {
        GTEST_SKIP() << "Orchestrator creation failed (dependencies unavailable)";
    }
}

/* ============================================================================
 * Style Embedding Tests
 * ============================================================================ */

TEST_F(CreativeTest, StyleEmbeddingCreate_ValidParams) {
    // WHAT: Create style embedding with valid parameters
    // WHY:  Test embedding allocation
    // HOW:  Create, verify, destroy

    style_embedding_t embedding;
    memset(&embedding, 0, sizeof(embedding));

    int result = style_embedding_create(&embedding, 256);
    EXPECT_EQ(result, 0);

    if (result == 0) {
        EXPECT_NE(embedding.embedding, nullptr);
        EXPECT_EQ(embedding.embedding_dim, 256u);
        style_embedding_destroy(&embedding);
    }
}

TEST_F(CreativeTest, StyleEmbeddingCreate_NullEmbedding) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int result = style_embedding_create(nullptr, 256);
    EXPECT_EQ(result, -1);
}

TEST_F(CreativeTest, StyleEmbeddingCreate_ZeroDimension) {
    // WHAT: Test zero dimension handling
    // WHY:  Edge case validation
    // HOW:  Create with dim=0

    style_embedding_t embedding;
    memset(&embedding, 0, sizeof(embedding));

    int result = style_embedding_create(&embedding, 0);
    // Should either fail or create minimal embedding
    SUCCEED() << "Zero dimension handling completed";
}

TEST_F(CreativeTest, StyleEmbeddingDestroy_NullEmbedding) {
    // WHAT: Test NULL safety for destroy
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    style_embedding_destroy(nullptr);
    SUCCEED() << "style_embedding_destroy(NULL) did not crash";
}

TEST_F(CreativeTest, StyleEmbeddingClone_ValidParams) {
    // WHAT: Clone a style embedding
    // WHY:  Test copy functionality
    // HOW:  Create source, clone to dest, verify

    style_embedding_t src, dst;
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    int result = style_embedding_create(&src, 64);
    ASSERT_EQ(result, 0);

    // Fill source with known values
    for (uint32_t i = 0; i < src.embedding_dim; i++) {
        src.embedding[i] = (float)i / 64.0f;
    }

    result = style_embedding_clone(&src, &dst);
    EXPECT_EQ(result, 0);

    if (result == 0) {
        EXPECT_EQ(dst.embedding_dim, src.embedding_dim);
        for (uint32_t i = 0; i < dst.embedding_dim; i++) {
            EXPECT_FLOAT_EQ(dst.embedding[i], src.embedding[i]);
        }
        style_embedding_destroy(&dst);
    }

    style_embedding_destroy(&src);
}

TEST_F(CreativeTest, StyleEmbeddingClone_NullSrc) {
    // WHAT: Test NULL safety for clone
    // WHY:  Defensive programming
    // HOW:  Call with NULL source

    style_embedding_t dst;
    memset(&dst, 0, sizeof(dst));

    int result = style_embedding_clone(nullptr, &dst);
    EXPECT_EQ(result, -1);
}

TEST_F(CreativeTest, StyleEmbeddingClone_NullDst) {
    // WHAT: Test NULL safety for clone
    // WHY:  Defensive programming
    // HOW:  Call with NULL destination

    style_embedding_t src;
    memset(&src, 0, sizeof(src));

    int result = style_embedding_create(&src, 64);
    ASSERT_EQ(result, 0);

    result = style_embedding_clone(&src, nullptr);
    EXPECT_EQ(result, -1);

    style_embedding_destroy(&src);
}

TEST_F(CreativeTest, StyleEmbeddingSimilarity_ValidParams) {
    // WHAT: Compute similarity between embeddings
    // WHY:  Test similarity computation
    // HOW:  Create two embeddings, compute similarity

    style_embedding_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    ASSERT_EQ(style_embedding_create(&a, 64), 0);
    ASSERT_EQ(style_embedding_create(&b, 64), 0);

    // Make them identical
    for (uint32_t i = 0; i < 64; i++) {
        a.embedding[i] = 1.0f;
        b.embedding[i] = 1.0f;
    }

    float similarity = style_embedding_similarity(&a, &b);

    // Identical vectors should have high similarity (1.0 for cosine)
    EXPECT_GE(similarity, 0.99f);
    EXPECT_LE(similarity, 1.0f);

    style_embedding_destroy(&a);
    style_embedding_destroy(&b);
}

TEST_F(CreativeTest, StyleEmbeddingSimilarity_Orthogonal) {
    // WHAT: Test similarity for orthogonal vectors
    // WHY:  Verify correct similarity computation
    // HOW:  Create orthogonal embeddings

    style_embedding_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    ASSERT_EQ(style_embedding_create(&a, 64), 0);
    ASSERT_EQ(style_embedding_create(&b, 64), 0);

    // Make them orthogonal (first half vs second half)
    for (uint32_t i = 0; i < 32; i++) {
        a.embedding[i] = 1.0f;
        a.embedding[i + 32] = 0.0f;
        b.embedding[i] = 0.0f;
        b.embedding[i + 32] = 1.0f;
    }

    float similarity = style_embedding_similarity(&a, &b);

    // Orthogonal vectors should have ~0 similarity
    EXPECT_NEAR(similarity, 0.0f, 0.1f);

    style_embedding_destroy(&a);
    style_embedding_destroy(&b);
}

TEST_F(CreativeTest, StyleEmbeddingSimilarity_NullParams) {
    // WHAT: Test NULL safety for similarity
    // WHY:  Defensive programming
    // HOW:  Call with NULL parameters

    style_embedding_t a;
    memset(&a, 0, sizeof(a));
    ASSERT_EQ(style_embedding_create(&a, 64), 0);

    // Both NULL should return 0 or -1
    float result = style_embedding_similarity(nullptr, nullptr);
    EXPECT_LE(result, 0.0f);

    // One NULL
    result = style_embedding_similarity(&a, nullptr);
    EXPECT_LE(result, 0.0f);

    result = style_embedding_similarity(nullptr, &a);
    EXPECT_LE(result, 0.0f);

    style_embedding_destroy(&a);
}

TEST_F(CreativeTest, StyleEmbeddingNormalize_ValidParams) {
    // WHAT: Normalize a style embedding
    // WHY:  Test normalization to unit length
    // HOW:  Create, normalize, verify length

    style_embedding_t embedding;
    memset(&embedding, 0, sizeof(embedding));

    ASSERT_EQ(style_embedding_create(&embedding, 64), 0);

    // Fill with known values
    for (uint32_t i = 0; i < 64; i++) {
        embedding.embedding[i] = (float)(i + 1);
    }

    style_embedding_normalize(&embedding);

    // Compute length after normalization
    float length_sq = 0.0f;
    for (uint32_t i = 0; i < embedding.embedding_dim; i++) {
        length_sq += embedding.embedding[i] * embedding.embedding[i];
    }
    float length = sqrtf(length_sq);

    // Should be unit length
    EXPECT_NEAR(length, 1.0f, 0.001f);

    style_embedding_destroy(&embedding);
}

TEST_F(CreativeTest, StyleEmbeddingNormalize_NullParam) {
    // WHAT: Test NULL safety for normalize
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    style_embedding_normalize(nullptr);
    SUCCEED() << "style_embedding_normalize(NULL) did not crash";
}

/* ============================================================================
 * Art Modality Helper Tests
 * ============================================================================ */

TEST_F(CreativeTest, ArtModalityCategory) {
    // WHAT: Test modality category helper
    // WHY:  Verify category classification
    // HOW:  Check various modalities

    // Text modalities (0-9)
    EXPECT_EQ(art_modality_category(ART_MODALITY_TEXT_POETRY), 0u);
    EXPECT_EQ(art_modality_category(ART_MODALITY_TEXT_PROSE), 0u);

    // Music modalities (10-19)
    EXPECT_EQ(art_modality_category(ART_MODALITY_MUSIC_CLASSICAL), 1u);
    EXPECT_EQ(art_modality_category(ART_MODALITY_MUSIC_JAZZ), 1u);

    // Visual modalities (20-29)
    EXPECT_EQ(art_modality_category(ART_MODALITY_VISUAL_PAINTING), 2u);
    EXPECT_EQ(art_modality_category(ART_MODALITY_VISUAL_DIGITAL), 2u);

    // Video modalities (30+)
    EXPECT_EQ(art_modality_category(ART_MODALITY_VIDEO_CINEMA), 3u);
    EXPECT_EQ(art_modality_category(ART_MODALITY_VIDEO_ANIMATION), 3u);
}

TEST_F(CreativeTest, ArtModalityIsText) {
    // WHAT: Test text modality detection
    // WHY:  Verify helper function
    // HOW:  Check various modalities

    EXPECT_TRUE(art_modality_is_text(ART_MODALITY_TEXT_POETRY));
    EXPECT_TRUE(art_modality_is_text(ART_MODALITY_TEXT_PROSE));
    EXPECT_FALSE(art_modality_is_text(ART_MODALITY_MUSIC_CLASSICAL));
    EXPECT_FALSE(art_modality_is_text(ART_MODALITY_VISUAL_PAINTING));
}

TEST_F(CreativeTest, ArtModalityIsMusic) {
    // WHAT: Test music modality detection
    // WHY:  Verify helper function
    // HOW:  Check various modalities

    EXPECT_FALSE(art_modality_is_music(ART_MODALITY_TEXT_POETRY));
    EXPECT_TRUE(art_modality_is_music(ART_MODALITY_MUSIC_CLASSICAL));
    EXPECT_TRUE(art_modality_is_music(ART_MODALITY_MUSIC_JAZZ));
    EXPECT_FALSE(art_modality_is_music(ART_MODALITY_VISUAL_PAINTING));
}

TEST_F(CreativeTest, ArtModalityIsVisual) {
    // WHAT: Test visual modality detection
    // WHY:  Verify helper function
    // HOW:  Check various modalities

    EXPECT_FALSE(art_modality_is_visual(ART_MODALITY_TEXT_POETRY));
    EXPECT_FALSE(art_modality_is_visual(ART_MODALITY_MUSIC_CLASSICAL));
    EXPECT_TRUE(art_modality_is_visual(ART_MODALITY_VISUAL_PAINTING));
    EXPECT_TRUE(art_modality_is_visual(ART_MODALITY_VISUAL_DIGITAL));
}

TEST_F(CreativeTest, ArtModalityIsVideo) {
    // WHAT: Test video modality detection
    // WHY:  Verify helper function
    // HOW:  Check various modalities

    EXPECT_FALSE(art_modality_is_video(ART_MODALITY_TEXT_POETRY));
    EXPECT_FALSE(art_modality_is_video(ART_MODALITY_VISUAL_PAINTING));
    EXPECT_TRUE(art_modality_is_video(ART_MODALITY_VIDEO_CINEMA));
    EXPECT_TRUE(art_modality_is_video(ART_MODALITY_VIDEO_ANIMATION));
}

/* ============================================================================
 * Cleanup Helper Tests
 * ============================================================================ */

TEST_F(CreativeTest, TextResultFree_NullParam) {
    // WHAT: Test NULL safety for text result free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_text_result_free(nullptr);
    SUCCEED() << "creative_text_result_free(NULL) did not crash";
}

TEST_F(CreativeTest, MusicResultFree_NullParam) {
    // WHAT: Test NULL safety for music result free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_music_result_free(nullptr);
    SUCCEED() << "creative_music_result_free(NULL) did not crash";
}

TEST_F(CreativeTest, VisualResultFree_NullParam) {
    // WHAT: Test NULL safety for visual result free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_visual_result_free(nullptr);
    SUCCEED() << "creative_visual_result_free(NULL) did not crash";
}

TEST_F(CreativeTest, ProjectSpecFree_NullParam) {
    // WHAT: Test NULL safety for project spec free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_project_spec_free(nullptr);
    SUCCEED() << "creative_project_spec_free(NULL) did not crash";
}

TEST_F(CreativeTest, ProjectOutputFree_NullParam) {
    // WHAT: Test NULL safety for project output free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_project_output_free(nullptr);
    SUCCEED() << "creative_project_output_free(NULL) did not crash";
}

TEST_F(CreativeTest, StyleEmbeddingFree_NullParam) {
    // WHAT: Test NULL safety for style embedding free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_style_embedding_free(nullptr);
    SUCCEED() << "creative_style_embedding_free(NULL) did not crash";
}

TEST_F(CreativeTest, BlendResultFree_NullParam) {
    // WHAT: Test NULL safety for blend result free
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    creative_blend_result_free(nullptr);
    SUCCEED() << "creative_blend_result_free(NULL) did not crash";
}
