/**
 * @file test_creative_validation.cpp
 * @brief Unit tests for Creative Cortex validation: archetype names, image creation,
 *        overflow protection, and error-code correctness.
 *
 * Function signatures tested (from src/cognitive/creative/nimcp_creative.c):
 *   const char* literary_style_archetype_name(literary_style_archetype_t archetype);
 *   const char* visual_style_archetype_name(visual_style_archetype_t archetype);
 *   const char* cinematic_style_archetype_name(cinematic_style_archetype_t archetype);
 *   const char* musical_style_archetype_name(musical_style_archetype_t archetype);
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
const char* musical_style_archetype_name(musical_style_archetype_t archetype);
const char* visual_style_archetype_name(visual_style_archetype_t archetype);
const char* cinematic_style_archetype_name(cinematic_style_archetype_t archetype);
visual_image_t* visual_image_create(uint32_t width, uint32_t height, uint32_t channels);
void visual_image_destroy(visual_image_t* image);
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CreativeValidationTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Archetype Name Tests
 * ============================================================================ */

TEST_F(CreativeValidationTest, LiteraryArchetypeNames_AllCorrect) {
    // WHAT: Verify all literary archetype names map correctly
    // WHY:  P0-6 fix -- STYLE_LIT_DOSTOEVSKY was returning "Dickens"
    // HOW:  Check every enum value returns the correct string

    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_HEMINGWAY),   "Hemingway");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_TOLSTOY),     "Tolstoy");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_JOYCE),       "Joyce");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_POE),         "Poe");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_AUSTEN),      "Austen");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_SHAKESPEARE), "Shakespeare");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_BORGES),      "Borges");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_KAFKA),       "Kafka");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_MARQUEZ),     "Marquez");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_DOSTOEVSKY),  "Dostoevsky");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_WOOLF),       "Woolf");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_FAULKNER),    "Faulkner");
    EXPECT_STREQ(literary_style_archetype_name(STYLE_LIT_COUNT),       "Unknown");
}

TEST_F(CreativeValidationTest, VisualArchetypeNames_AllCorrect) {
    // WHAT: Verify all visual archetype names map correctly
    // WHY:  P0-6 fix -- ESCHER returned "Frida Kahlo", CARAVAGGIO returned "Banksy"
    // HOW:  Check every enum value returns the correct string

    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_VAN_GOGH),   "Van Gogh");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_MONET),      "Monet");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_PICASSO),    "Picasso");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_DALI),       "Dali");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_REMBRANDT),  "Rembrandt");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_WARHOL),     "Warhol");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_KLIMT),      "Klimt");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_HOKUSAI),    "Hokusai");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_KANDINSKY),  "Kandinsky");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_ESCHER),     "Escher");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_BASQUIAT),   "Basquiat");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_CARAVAGGIO), "Caravaggio");
    EXPECT_STREQ(visual_style_archetype_name(STYLE_VIS_COUNT),      "Unknown");
}

TEST_F(CreativeValidationTest, CinematicArchetypeNames_AllCorrect) {
    // WHAT: Verify all cinematic archetype names map correctly
    // WHY:  P0-6 fix -- WELLES/"Scorsese", FINCHER/"Lynch", KUROSAWA/"Wes Anderson"
    // HOW:  Check every enum value returns the correct string

    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_KUBRICK),    "Kubrick");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_SPIELBERG),  "Spielberg");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_TARANTINO),  "Tarantino");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_NOLAN),      "Nolan");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_TARKOVSKY),  "Tarkovsky");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_MIYAZAKI),   "Miyazaki");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_HITCHCOCK),  "Hitchcock");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_WELLES),     "Welles");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_KUROSAWA),   "Kurosawa");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_FINCHER),    "Fincher");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_VILLENEUVE), "Denis Villeneuve");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_COPPOLA),    "Coppola");
    EXPECT_STREQ(cinematic_style_archetype_name(STYLE_CINEMA_COUNT),      "Unknown");
}

TEST_F(CreativeValidationTest, MusicalArchetypeNames_AllCorrect) {
    // WHAT: Verify all musical archetype names map correctly
    // WHY:  Ensure no regressions in musical archetypes (known-good baseline)
    // HOW:  Check every enum value returns the correct string

    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_BACH),            "Bach");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_BEETHOVEN),       "Beethoven");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_DEBUSSY),         "Debussy");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_JOHN_WILLIAMS),   "John Williams");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_MILES_DAVIS),     "Miles Davis");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_HANS_ZIMMER),     "Hans Zimmer");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_STRAVINSKY),      "Stravinsky");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_ENNIO_MORRICONE), "Ennio Morricone");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_SAKAMOTO),        "Sakamoto");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_GLASS),           "Philip Glass");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_COPLAND),         "Copland");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_RAVEL),           "Ravel");
    EXPECT_STREQ(musical_style_archetype_name(STYLE_MUSIC_COUNT),           "Unknown");
}

/* ============================================================================
 * Visual Image Creation Tests
 * ============================================================================ */

TEST_F(CreativeValidationTest, VisualImageCreate_ValidParams) {
    // WHAT: Create a valid image and verify fields
    // WHY:  Ensure normal usage works correctly
    // HOW:  Create 64x64 RGB image, check fields, destroy

    visual_image_t* img = visual_image_create(64, 64, 3);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->width, 64u);
    EXPECT_EQ(img->height, 64u);
    EXPECT_EQ(img->channels, 3u);
    EXPECT_NE(img->pixels, nullptr);
    EXPECT_TRUE(img->owns_pixels);
    visual_image_destroy(img);
}

TEST_F(CreativeValidationTest, VisualImageCreate_ZeroWidth) {
    // WHAT: Reject zero width
    // WHY:  P0-4/P0-5 -- invalid params must return NULL with correct error code
    // HOW:  Pass width=0, expect NULL return

    visual_image_t* img = visual_image_create(0, 64, 3);
    EXPECT_EQ(img, nullptr);
}

TEST_F(CreativeValidationTest, VisualImageCreate_ZeroHeight) {
    // WHAT: Reject zero height
    // WHY:  Defensive validation
    // HOW:  Pass height=0, expect NULL return

    visual_image_t* img = visual_image_create(64, 0, 3);
    EXPECT_EQ(img, nullptr);
}

TEST_F(CreativeValidationTest, VisualImageCreate_ZeroChannels) {
    // WHAT: Reject zero channels
    // WHY:  Defensive validation
    // HOW:  Pass channels=0, expect NULL return

    visual_image_t* img = visual_image_create(64, 64, 0);
    EXPECT_EQ(img, nullptr);
}

TEST_F(CreativeValidationTest, VisualImageCreate_OverflowProtection) {
    // WHAT: Reject dimensions that would overflow size_t
    // WHY:  P0-4 -- integer overflow before calloc could allocate wrong size
    // HOW:  Pass extreme dimensions that overflow width*height

    /* UINT32_MAX * UINT32_MAX would overflow size_t on 64-bit */
    visual_image_t* img = visual_image_create(UINT32_MAX, UINT32_MAX, 3);
    EXPECT_EQ(img, nullptr);

    /* Large but not max -- still overflows */
    img = visual_image_create(0x80000000u, 2, 4);
    EXPECT_EQ(img, nullptr);
}
