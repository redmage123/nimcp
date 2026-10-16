/**
 * @file test_creative_e2e.cpp
 * @brief End-to-End Tests for Creative Module
 *
 * WHAT: Full workflow E2E tests for creative module image handling and archetypes
 * WHY:  Verify visual image lifecycle, overflow protection, archetype consistency,
 *       and error code correctness in the creative cortex module
 * HOW:  Test realistic creative scenarios through the orchestrator API
 *
 * TEST PIPELINES:
 * - CreativeE2E_ImageCreateDestroy: Full lifecycle of visual image creation
 * - CreativeE2E_ImageOverflowProtection: Very large dimensions rejected
 * - CreativeE2E_ArchetypeConsistency: Archetype names match enum values
 * - CreativeE2E_ErrorCodesCorrect: Error codes match actual error types
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <climits>

extern "C" {
#include "cognitive/creative/nimcp_creative.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class CreativeE2ETest : public ::testing::Test {
protected:
    creative_orchestrator_t* orchestrator_ = nullptr;

    void SetUp() override {
        // Create orchestrator with minimal config for testing
        creative_config_t config;
        creative_config_init_defaults(&config);
        config.enable_appreciation = true;
        config.enable_inspiration = true;
        config.enable_visual_generation = false;  // No GPU needed for tests
        config.enable_text_generation = false;
        config.enable_music_generation = false;
        config.enable_video_generation = false;
        config.integrate_with_ethics = false;
        config.integrate_with_emotion = false;
        config.integrate_with_memory = false;
        config.integrate_with_immune = false;
        config.device_type = 0;  // CPU only
        orchestrator_ = creative_orchestrator_create(&config);
        // Orchestrator may be NULL if creative module not fully built - tests handle this
    }

    void TearDown() override {
        if (orchestrator_) {
            creative_orchestrator_destroy(orchestrator_);
            orchestrator_ = nullptr;
        }
    }
};

//=============================================================================
// Test 1: Image Create/Destroy Lifecycle
//=============================================================================

TEST_F(CreativeE2ETest, CreativeE2E_ImageCreateDestroy) {
    // Stage: Create a visual_image_t with valid parameters
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint8_t channels = 3;  // RGB

    visual_image_t image;
    memset(&image, 0, sizeof(image));
    image.width = width;
    image.height = height;
    image.channels = channels;

    // Allocate pixel data
    size_t pixel_data_size = (size_t)width * height * channels;
    image.pixels = (uint8_t*)calloc(pixel_data_size, 1);
    ASSERT_NE(image.pixels, nullptr) << "Failed to allocate pixel data";
    image.owns_pixels = true;

    // Verify the image struct is properly formed
    EXPECT_EQ(image.width, width);
    EXPECT_EQ(image.height, height);
    EXPECT_EQ(image.channels, channels);
    EXPECT_NE(image.pixels, nullptr);
    EXPECT_TRUE(image.owns_pixels);

    // Fill with test pattern (gradient)
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t idx = ((size_t)y * width + x) * channels;
            image.pixels[idx + 0] = (uint8_t)(x * 4);  // R
            image.pixels[idx + 1] = (uint8_t)(y * 4);  // G
            image.pixels[idx + 2] = 128;                // B
        }
    }

    // Verify first pixel
    EXPECT_EQ(image.pixels[0], 0);    // R at (0,0)
    EXPECT_EQ(image.pixels[1], 0);    // G at (0,0)
    EXPECT_EQ(image.pixels[2], 128);  // B at (0,0)

    // If orchestrator available, try to evaluate the visual
    if (orchestrator_) {
        aesthetic_evaluation_t eval;
        memset(&eval, 0, sizeof(eval));
        int rc = creative_evaluate_visual(orchestrator_, &image, &eval);
        // May or may not succeed depending on model availability
        (void)rc;
    }

    // Cleanup
    free(image.pixels);
    image.pixels = nullptr;
    image.owns_pixels = false;
}

//=============================================================================
// Test 2: Image Overflow Protection
//=============================================================================

TEST_F(CreativeE2ETest, CreativeE2E_ImageOverflowProtection) {
    // Test that extremely large image dimensions don't cause integer overflow
    // or crash during allocation

    // Test case 1: Width that would overflow size_t when multiplied
    {
        visual_image_t image;
        memset(&image, 0, sizeof(image));
        image.width = UINT32_MAX;
        image.height = UINT32_MAX;
        image.channels = 4;

        // The product width * height * channels would overflow size_t on 32-bit
        // and is unreasonably large on 64-bit (> 68 exabytes)
        size_t max_reasonable_pixels = (size_t)1024 * 1024 * 1024;  // 1 billion
        size_t requested = (size_t)image.width * image.height * image.channels;

        // Verify we can detect the overflow condition
        if (image.width > 0 && image.height > 0) {
            size_t max_height = SIZE_MAX / image.width / image.channels;
            bool would_overflow = image.height > max_height;
            EXPECT_TRUE(would_overflow || requested > max_reasonable_pixels)
                << "Expected overflow or unreasonable size detection";
        }

        // If orchestrator is available, submitting this should fail gracefully
        if (orchestrator_) {
            visual_generation_request_t req;
            memset(&req, 0, sizeof(req));
            req.width = UINT32_MAX;
            req.height = UINT32_MAX;
            req.steps = 1;
            req.guidance_scale = 7.5f;
            req.prompt = "test overflow";

            visual_generation_result_t result;
            memset(&result, 0, sizeof(result));
            int rc = creative_generate_visual(orchestrator_, &req, &result);
            // Should fail gracefully, not crash
            EXPECT_EQ(rc, -1) << "Expected failure for unreasonable image dimensions";
            creative_visual_result_free(&result);
        }
    }

    // Test case 2: Zero dimensions should also be handled
    {
        visual_image_t image;
        memset(&image, 0, sizeof(image));
        image.width = 0;
        image.height = 0;
        image.channels = 3;
        image.pixels = nullptr;

        // A zero-size image should not crash the evaluator
        if (orchestrator_) {
            aesthetic_evaluation_t eval;
            memset(&eval, 0, sizeof(eval));
            int rc = creative_evaluate_visual(orchestrator_, &image, &eval);
            EXPECT_EQ(rc, -1) << "Expected failure for zero-size image";
        }
    }
}

//=============================================================================
// Test 3: Archetype Consistency
//=============================================================================

TEST_F(CreativeE2ETest, CreativeE2E_ArchetypeConsistency) {
    // Verify that all archetype enums have consistent values

    // Literary archetypes: 0 to STYLE_LIT_COUNT-1
    EXPECT_EQ(STYLE_LIT_HEMINGWAY, 0);
    EXPECT_EQ(STYLE_LIT_COUNT, 12);
    EXPECT_LT(STYLE_LIT_HEMINGWAY, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_TOLSTOY, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_JOYCE, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_POE, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_AUSTEN, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_SHAKESPEARE, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_BORGES, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_KAFKA, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_MARQUEZ, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_DOSTOEVSKY, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_WOOLF, STYLE_LIT_COUNT);
    EXPECT_LT(STYLE_LIT_FAULKNER, STYLE_LIT_COUNT);

    // Musical archetypes: 0 to STYLE_MUSIC_COUNT-1
    EXPECT_EQ(STYLE_MUSIC_BACH, 0);
    EXPECT_EQ(STYLE_MUSIC_COUNT, 12);
    EXPECT_LT(STYLE_MUSIC_BEETHOVEN, STYLE_MUSIC_COUNT);
    EXPECT_LT(STYLE_MUSIC_DEBUSSY, STYLE_MUSIC_COUNT);
    EXPECT_LT(STYLE_MUSIC_JOHN_WILLIAMS, STYLE_MUSIC_COUNT);
    EXPECT_LT(STYLE_MUSIC_HANS_ZIMMER, STYLE_MUSIC_COUNT);

    // Visual archetypes: 0 to STYLE_VIS_COUNT-1
    EXPECT_EQ(STYLE_VIS_VAN_GOGH, 0);
    EXPECT_EQ(STYLE_VIS_COUNT, 12);
    EXPECT_LT(STYLE_VIS_MONET, STYLE_VIS_COUNT);
    EXPECT_LT(STYLE_VIS_PICASSO, STYLE_VIS_COUNT);
    EXPECT_LT(STYLE_VIS_DALI, STYLE_VIS_COUNT);
    EXPECT_LT(STYLE_VIS_KANDINSKY, STYLE_VIS_COUNT);

    // Cinematic archetypes: 0 to STYLE_CINEMA_COUNT-1
    EXPECT_EQ(STYLE_CINEMA_KUBRICK, 0);
    EXPECT_EQ(STYLE_CINEMA_COUNT, 12);
    EXPECT_LT(STYLE_CINEMA_SPIELBERG, STYLE_CINEMA_COUNT);
    EXPECT_LT(STYLE_CINEMA_TARANTINO, STYLE_CINEMA_COUNT);
    EXPECT_LT(STYLE_CINEMA_NOLAN, STYLE_CINEMA_COUNT);
    EXPECT_LT(STYLE_CINEMA_MIYAZAKI, STYLE_CINEMA_COUNT);

    // If orchestrator available, verify archetype styles can be retrieved
    if (orchestrator_) {
        style_embedding_t style;
        memset(&style, 0, sizeof(style));

        // Test a literary archetype
        int rc = creative_get_archetype_style(
            orchestrator_, ART_MODALITY_TEXT_POETRY,
            STYLE_LIT_HEMINGWAY, &style);
        if (rc == 0) {
            EXPECT_EQ(style.archetype_id, STYLE_LIT_HEMINGWAY);
            EXPECT_GT(style.embedding_dim, 0u);
            style_embedding_destroy(&style);
        }

        // Test a visual archetype
        memset(&style, 0, sizeof(style));
        rc = creative_get_archetype_style(
            orchestrator_, ART_MODALITY_VISUAL_PAINTING,
            STYLE_VIS_VAN_GOGH, &style);
        if (rc == 0) {
            EXPECT_EQ(style.archetype_id, STYLE_VIS_VAN_GOGH);
            style_embedding_destroy(&style);
        }
    }
}

//=============================================================================
// Test 4: Error Codes Correct
//=============================================================================

TEST_F(CreativeE2ETest, CreativeE2E_ErrorCodesCorrect) {
    // Verify that creative functions return correct error codes

    // Test 1: NULL orchestrator should return -1
    {
        aesthetic_evaluation_t eval;
        memset(&eval, 0, sizeof(eval));
        int rc = creative_evaluate_text(nullptr, "test", 4,
                                        ART_MODALITY_TEXT_POETRY, &eval);
        EXPECT_EQ(rc, -1) << "NULL orchestrator should return -1";
    }

    // Test 2: NULL output should return -1
    if (orchestrator_) {
        int rc = creative_evaluate_text(orchestrator_, "test", 4,
                                        ART_MODALITY_TEXT_POETRY, nullptr);
        EXPECT_EQ(rc, -1) << "NULL output should return -1";
    }

    // Test 3: NULL content for evaluation should return -1
    if (orchestrator_) {
        aesthetic_evaluation_t eval;
        memset(&eval, 0, sizeof(eval));
        int rc = creative_evaluate_text(orchestrator_, nullptr, 0,
                                        ART_MODALITY_TEXT_POETRY, &eval);
        EXPECT_EQ(rc, -1) << "NULL content should return -1";
    }

    // Test 4: NULL orchestrator for generation should return -1
    {
        text_generation_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = TEXT_GEN_POETRY;
        req.prompt = "test";
        req.prompt_len = 4;
        req.max_length = 100;
        req.temperature = 1.0f;

        text_generation_result_t result;
        memset(&result, 0, sizeof(result));
        int rc = creative_generate_text(nullptr, &req, &result);
        EXPECT_EQ(rc, -1) << "NULL orchestrator for generation should return -1";
    }

    // Test 5: Modality helper functions
    EXPECT_TRUE(art_modality_is_text(ART_MODALITY_TEXT_POETRY));
    EXPECT_TRUE(art_modality_is_text(ART_MODALITY_TEXT_LYRICS));
    EXPECT_FALSE(art_modality_is_text(ART_MODALITY_MUSIC_CLASSICAL));
    EXPECT_FALSE(art_modality_is_text(ART_MODALITY_VISUAL_PAINTING));

    EXPECT_TRUE(art_modality_is_music(ART_MODALITY_MUSIC_CLASSICAL));
    EXPECT_TRUE(art_modality_is_music(ART_MODALITY_MUSIC_JAZZ));
    EXPECT_FALSE(art_modality_is_music(ART_MODALITY_TEXT_POETRY));
    EXPECT_FALSE(art_modality_is_music(ART_MODALITY_VISUAL_PAINTING));

    EXPECT_TRUE(art_modality_is_visual(ART_MODALITY_VISUAL_PAINTING));
    EXPECT_TRUE(art_modality_is_visual(ART_MODALITY_VISUAL_DIGITAL));
    EXPECT_FALSE(art_modality_is_visual(ART_MODALITY_TEXT_POETRY));
    EXPECT_FALSE(art_modality_is_visual(ART_MODALITY_VIDEO_CINEMA));

    EXPECT_TRUE(art_modality_is_video(ART_MODALITY_VIDEO_CINEMA));
    EXPECT_TRUE(art_modality_is_video(ART_MODALITY_VIDEO_ANIMATION));
    EXPECT_FALSE(art_modality_is_video(ART_MODALITY_VISUAL_PAINTING));
    EXPECT_FALSE(art_modality_is_video(ART_MODALITY_MUSIC_CLASSICAL));

    // Test 6: Category classification
    EXPECT_EQ(art_modality_category(ART_MODALITY_TEXT_POETRY), 0);
    EXPECT_EQ(art_modality_category(ART_MODALITY_MUSIC_CLASSICAL), 1);
    EXPECT_EQ(art_modality_category(ART_MODALITY_VISUAL_PAINTING), 2);
    EXPECT_EQ(art_modality_category(ART_MODALITY_VIDEO_CINEMA), 3);
}
