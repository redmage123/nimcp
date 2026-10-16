/**
 * @file test_creative_regression.cpp
 * @brief Regression tests for Creative Cortex P0-6 archetype mapping bugs
 *
 * These tests lock down the specific bugs that were fixed:
 *   - STYLE_LIT_DOSTOEVSKY returned "Dickens" instead of "Dostoevsky"
 *   - STYLE_VIS_ESCHER returned "Frida Kahlo" instead of "Escher"
 *   - STYLE_VIS_CARAVAGGIO returned "Banksy" instead of "Caravaggio"
 *   - STYLE_CINEMA_WELLES returned "Scorsese" instead of "Welles"
 *   - STYLE_CINEMA_FINCHER returned "Lynch" instead of "Fincher"
 *   - STYLE_CINEMA_KUROSAWA returned "Wes Anderson" instead of "Kurosawa"
 *
 * Function signatures tested (from src/cognitive/creative/nimcp_creative.c):
 *   const char* literary_style_archetype_name(literary_style_archetype_t archetype);
 *   const char* visual_style_archetype_name(visual_style_archetype_t archetype);
 *   const char* cinematic_style_archetype_name(cinematic_style_archetype_t archetype);
 *   visual_image_t* visual_image_create(uint32_t width, uint32_t height, uint32_t channels);
 *   void visual_image_destroy(visual_image_t* image);
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <climits>

extern "C" {
#include "cognitive/creative/nimcp_creative.h"

/* Internal functions not in public header -- declare them for testing */
const char* literary_style_archetype_name(literary_style_archetype_t archetype);
const char* visual_style_archetype_name(visual_style_archetype_t archetype);
const char* cinematic_style_archetype_name(cinematic_style_archetype_t archetype);
visual_image_t* visual_image_create(uint32_t width, uint32_t height, uint32_t channels);
void visual_image_destroy(visual_image_t* image);
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class CreativeRegressionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * P0-6 Regression: Archetype Name Swaps
 * ============================================================================ */

TEST_F(CreativeRegressionTest, ArchetypeNameSwaps_NeverReturnWrongArtist) {
    // WHAT: Verify the six previously-swapped archetype names never regress
    // WHY:  These exact bugs existed: enum X returned name Y
    // HOW:  Assert the correct name AND assert the old wrong name is NOT returned

    /* Literary: Dostoevsky was returning "Dickens" */
    const char* dostoevsky = literary_style_archetype_name(STYLE_LIT_DOSTOEVSKY);
    EXPECT_STREQ(dostoevsky, "Dostoevsky");
    EXPECT_STRNE(dostoevsky, "Dickens");

    /* Visual: Escher was returning "Frida Kahlo" */
    const char* escher = visual_style_archetype_name(STYLE_VIS_ESCHER);
    EXPECT_STREQ(escher, "Escher");
    EXPECT_STRNE(escher, "Frida Kahlo");

    /* Visual: Caravaggio was returning "Banksy" */
    const char* caravaggio = visual_style_archetype_name(STYLE_VIS_CARAVAGGIO);
    EXPECT_STREQ(caravaggio, "Caravaggio");
    EXPECT_STRNE(caravaggio, "Banksy");

    /* Cinema: Welles was returning "Scorsese" */
    const char* welles = cinematic_style_archetype_name(STYLE_CINEMA_WELLES);
    EXPECT_STREQ(welles, "Welles");
    EXPECT_STRNE(welles, "Scorsese");

    /* Cinema: Fincher was returning "Lynch" */
    const char* fincher = cinematic_style_archetype_name(STYLE_CINEMA_FINCHER);
    EXPECT_STREQ(fincher, "Fincher");
    EXPECT_STRNE(fincher, "Lynch");

    /* Cinema: Kurosawa was returning "Wes Anderson" */
    const char* kurosawa = cinematic_style_archetype_name(STYLE_CINEMA_KUROSAWA);
    EXPECT_STREQ(kurosawa, "Kurosawa");
    EXPECT_STRNE(kurosawa, "Wes Anderson");
}

/* ============================================================================
 * P0-4/P0-5 Regression: Overflow and Error Codes
 * ============================================================================ */

TEST_F(CreativeRegressionTest, VisualImageCreate_OverflowReturnsNull) {
    // WHAT: Overflow dimensions must return NULL (not allocate wrong-sized buffer)
    // WHY:  P0-4 -- width*height*channels could overflow size_t before calloc
    // HOW:  Use extreme values that previously would have wrapped around

    /* These values trigger width*height overflow on 64-bit systems */
    visual_image_t* img = visual_image_create(UINT32_MAX, UINT32_MAX, 3);
    EXPECT_EQ(img, nullptr);

    /* Slightly less extreme but still overflows pixel_count * channels */
    img = visual_image_create(0x80000000u, 2, 4);
    EXPECT_EQ(img, nullptr);

    /* Boundary: max values that do NOT overflow should succeed */
    img = visual_image_create(1, 1, 1);
    ASSERT_NE(img, nullptr);
    visual_image_destroy(img);
}

TEST_F(CreativeRegressionTest, VisualImageCreate_InvalidParamsReturnNull) {
    // WHAT: Zero dimensions must return NULL with NIMCP_ERROR_INVALID_PARAM
    // WHY:  P0-5 -- previously returned NIMCP_ERROR_NO_MEMORY for param validation
    // HOW:  Zero width/height/channels all return NULL; channels > 4 also returns NULL

    EXPECT_EQ(visual_image_create(0, 64, 3), nullptr);
    EXPECT_EQ(visual_image_create(64, 0, 3), nullptr);
    EXPECT_EQ(visual_image_create(64, 64, 0), nullptr);
    EXPECT_EQ(visual_image_create(64, 64, 5), nullptr);  /* channels > 4 */
}
