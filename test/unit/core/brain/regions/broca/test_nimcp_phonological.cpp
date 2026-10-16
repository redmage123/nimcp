/**
 * @file test_nimcp_phonological.cpp
 * @brief Unit tests for nimcp_phonological.c
 *
 * WHAT: Comprehensive unit tests for the phonological processing module
 * WHY:  Ensure correct phoneme buffering, syllabification, stress application,
 *       prosody generation, and coarticulation planning.
 * HOW:  Use Google Test framework to test lifecycle, phoneme/syllable management,
 *       prosody, and coarticulation with various inputs and edge cases.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h> // For memset, strcmp
#include <stdlib.h> // For NULL
#include <math.h>   // For fabs

#include "core/brain/regions/broca/nimcp_phonological.h"

// Test Fixture for Phonological Processor
class PhonologicalProcessorTest : public ::testing::Test {
protected:
    phonological_processor_t* processor;
    phonological_config_t config;

    void SetUp() override {
        config = phonological_default_config();
        config.max_phonemes = 10;   // Small buffer for testing limits
        config.max_syllables = 5;   // Small buffer for testing limits
        processor = phonological_create(&config);
        ASSERT_NE(nullptr, processor) << "Failed to create phonological processor";
    }

    void TearDown() override {
        phonological_destroy(processor);
        processor = nullptr;
    }

    void ExpectFloatNear(float actual, float expected, float epsilon = 1e-5f, const char* msg = "") {
        EXPECT_NEAR(actual, expected, epsilon) << msg;
    }

    // Helper to create a phoneme (simplified)
    phoneme_t create_phoneme(uint8_t symbol, uint8_t category, float duration, float voicing, bool stressed = false) {
        phoneme_t p;
        p.symbol = symbol;
        p.category = category;
        p.duration_ms = duration;
        p.voicing = voicing;
        p.is_stressed = stressed;
        return p;
    }
};

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(PhonologicalProcessorTest, CreateAndDestroy) {
    // Processor created and destroyed in SetUp/TearDown
    ASSERT_NE(nullptr, processor);
    SUCCEED();
}

TEST_F(PhonologicalProcessorTest, CreateWithNullConfig) {
    phonological_processor_t* null_config_processor = phonological_create(nullptr);
    ASSERT_NE(nullptr, null_config_processor) << "Should create with default config";
    phonological_destroy(null_config_processor);
}

// =============================================================================
// Phoneme Operations Tests
// =============================================================================

TEST_F(PhonologicalProcessorTest, AddPhoneme) {
    EXPECT_TRUE(phonological_add_phoneme(processor, 'a'));
    EXPECT_EQ(1, phonological_get_phoneme_count(processor));
}

TEST_F(PhonologicalProcessorTest, AddPhonemeDetailed) {
    EXPECT_TRUE(phonological_add_phoneme_detailed(processor, 'p', PHONEME_CATEGORY_CONSONANT, 50.0f, 0.0f));
    EXPECT_EQ(1, phonological_get_phoneme_count(processor));
}

TEST_F(PhonologicalProcessorTest, AddPhonemeLimit) {
    for (uint32_t i = 0; i < config.max_phonemes; ++i) {
        ASSERT_TRUE(phonological_add_phoneme(processor, 'a')) << "Failed to add phoneme " << i;
    }
    EXPECT_EQ(config.max_phonemes, phonological_get_phoneme_count(processor));
    EXPECT_FALSE(phonological_add_phoneme(processor, 'b')) << "Should not add beyond capacity";
}

TEST_F(PhonologicalProcessorTest, AddPhonemeNullInputs) {
    EXPECT_FALSE(phonological_add_phoneme(nullptr, 'a'));
    EXPECT_FALSE(phonological_add_phoneme_detailed(nullptr, 'a', 0, 0, 0));
}

TEST_F(PhonologicalProcessorTest, ClearPhonemes) {
    phonological_add_phoneme(processor, 'a');
    phonological_add_phoneme(processor, 'b');
    ASSERT_EQ(2, phonological_get_phoneme_count(processor));

    EXPECT_TRUE(phonological_clear_phonemes(processor));
    EXPECT_EQ(0, phonological_get_phoneme_count(processor));
    EXPECT_EQ(PHONOLOGICAL_STATUS_IDLE, phonological_get_status(processor));
}

TEST_F(PhonologicalProcessorTest, ClearPhonemesNullInput) {
    EXPECT_FALSE(phonological_clear_phonemes(nullptr));
}

TEST_F(PhonologicalProcessorTest, ResetProcessor) {
    phonological_add_phoneme(processor, 'a');
    phonological_add_phoneme(processor, 't');
    phonological_generate_syllables(processor);
    ASSERT_EQ(2, phonological_get_phoneme_count(processor));
    ASSERT_EQ(1, phonological_get_syllable_count(processor));

    EXPECT_TRUE(phonological_reset(processor));
    EXPECT_EQ(0, phonological_get_phoneme_count(processor));
    EXPECT_EQ(0, phonological_get_syllable_count(processor));
    EXPECT_EQ(PHONOLOGICAL_STATUS_IDLE, phonological_get_status(processor));
}

TEST_F(PhonologicalProcessorTest, ResetNullProcessor) {
    EXPECT_FALSE(phonological_reset(nullptr));
}

// =============================================================================
// Syllable Operations Tests
// =============================================================================

TEST_F(PhonologicalProcessorTest, GenerateSyllables_CVC) {
    // /k/ /æ/ /t/ -> CVC syllable
    phonological_add_phoneme_detailed(processor, 'k', PHONEME_CATEGORY_CONSONANT, 50, 0);
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);

    EXPECT_TRUE(phonological_generate_syllables(processor));
    EXPECT_EQ(1, phonological_get_syllable_count(processor));

    syllable_t syll;
    ASSERT_TRUE(phonological_get_syllable(processor, 0, &syll));
    EXPECT_EQ(SYLLABLE_TYPE_CLOSED, syll.type);
    EXPECT_EQ(1, syll.onset_count);
    EXPECT_EQ('k', syll.onset[0].symbol);
    EXPECT_EQ(1, syll.nucleus_count);
    EXPECT_EQ('a', syll.nucleus[0].symbol);
    EXPECT_EQ(1, syll.coda_count);
    EXPECT_EQ('t', syll.coda[0].symbol);
    ExpectFloatNear(200.0f, syll.duration_ms); // 50+100+50
}

TEST_F(PhonologicalProcessorTest, GenerateSyllables_V) {
    // /a/ -> V syllable (no onset, no coda)
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);

    EXPECT_TRUE(phonological_generate_syllables(processor));
    EXPECT_EQ(1, phonological_get_syllable_count(processor));

    syllable_t syll;
    ASSERT_TRUE(phonological_get_syllable(processor, 0, &syll));
    EXPECT_EQ(SYLLABLE_TYPE_OPEN, syll.type);
    EXPECT_EQ(0, syll.onset_count);
    EXPECT_EQ(1, syll.nucleus_count);
    EXPECT_EQ('a', syll.nucleus[0].symbol);
    EXPECT_EQ(0, syll.coda_count);
    ExpectFloatNear(100.0f, syll.duration_ms);
}

TEST_F(PhonologicalProcessorTest, GenerateSyllables_TwoSyllables_CV_CV) {
    // /d/ /o/ /g/ /i/ -> "doggie"
    phonological_add_phoneme_detailed(processor, 'd', PHONEME_CATEGORY_CONSONANT, 40, 1);
    phonological_add_phoneme_detailed(processor, 'o', PHONEME_CATEGORY_VOWEL, 80, 1);
    phonological_add_phoneme_detailed(processor, 'g', PHONEME_CATEGORY_CONSONANT, 40, 1);
    phonological_add_phoneme_detailed(processor, 'i', PHONEME_CATEGORY_VOWEL, 80, 1);

    EXPECT_TRUE(phonological_generate_syllables(processor));
    EXPECT_EQ(2, phonological_get_syllable_count(processor));

    syllable_t syll1, syll2;
    ASSERT_TRUE(phonological_get_syllable(processor, 0, &syll1));
    ASSERT_TRUE(phonological_get_syllable(processor, 1, &syll2));

    EXPECT_EQ(SYLLABLE_TYPE_OPEN, syll1.type); // do
    EXPECT_EQ(1, syll1.onset_count); EXPECT_EQ('d', syll1.onset[0].symbol);
    EXPECT_EQ(1, syll1.nucleus_count); EXPECT_EQ('o', syll1.nucleus[0].symbol);
    EXPECT_EQ(0, syll1.coda_count);

    EXPECT_EQ(SYLLABLE_TYPE_OPEN, syll2.type); // gie
    EXPECT_EQ(1, syll2.onset_count); EXPECT_EQ('g', syll2.onset[0].symbol);
    EXPECT_EQ(1, syll2.nucleus_count); EXPECT_EQ('i', syll2.nucleus[0].symbol);
    EXPECT_EQ(0, syll2.coda_count);

    EXPECT_TRUE(syll1.is_initial);
    EXPECT_FALSE(syll1.is_final);
    EXPECT_FALSE(syll2.is_initial);
    EXPECT_TRUE(syll2.is_final);
}

TEST_F(PhonologicalProcessorTest, GenerateSyllables_NoPhonemes) {
    EXPECT_FALSE(phonological_generate_syllables(processor));
    EXPECT_EQ(0, phonological_get_syllable_count(processor));
}

TEST_F(PhonologicalProcessorTest, GenerateSyllables_ConsonantOnly) {
    // Should result in a consonant-only "syllable" if no vowel can be found
    phonological_add_phoneme_detailed(processor, 'p', PHONEME_CATEGORY_CONSONANT, 50, 0);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);

    EXPECT_TRUE(phonological_generate_syllables(processor));
    EXPECT_EQ(2, phonological_get_syllable_count(processor)); // Each consonant treated as a syllable

    syllable_t syll1, syll2;
    ASSERT_TRUE(phonological_get_syllable(processor, 0, &syll1));
    ASSERT_TRUE(phonological_get_syllable(processor, 1, &syll2));

    EXPECT_EQ(SYLLABLE_TYPE_ONSET_ONLY, syll1.type);
    EXPECT_EQ(1, syll1.onset_count);
    EXPECT_EQ('p', syll1.onset[0].symbol);
    EXPECT_EQ(0, syll1.nucleus_count);
    EXPECT_EQ(0, syll1.coda_count);

    EXPECT_EQ(SYLLABLE_TYPE_ONSET_ONLY, syll2.type);
    EXPECT_EQ(1, syll2.onset_count);
    EXPECT_EQ('t', syll2.onset[0].symbol);
    EXPECT_EQ(0, syll2.nucleus_count);
    EXPECT_EQ(0, syll2.coda_count);
}

TEST_F(PhonologicalProcessorTest, ApplyStress) {
    // /k/ /a/ /t/
    phonological_add_phoneme_detailed(processor, 'k', PHONEME_CATEGORY_CONSONANT, 50, 0);
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);
    phonological_generate_syllables(processor);
    ASSERT_EQ(1, phonological_get_syllable_count(processor));

    syllable_t syll_before;
    phonological_get_syllable(processor, 0, &syll_before);
    float original_vowel_duration = syll_before.nucleus[0].duration_ms;

    EXPECT_TRUE(phonological_apply_stress(processor, 0, 0.8f)); // Apply high stress
    syllable_t syll_after;
    phonological_get_syllable(processor, 0, &syll_after);

    EXPECT_GE(syll_after.stress_level, 0.8f); // Should be approximately 0.8
    EXPECT_TRUE(syll_after.nucleus[0].is_stressed);

    // Stressed syllables are longer: 100 * (1 + 0.8 * 0.7) = 100 * (1 + 0.56) = 156
    ExpectFloatNear(original_vowel_duration * (1.0f + (0.8f * config.stress_weight)), syll_after.nucleus[0].duration_ms);
    ExpectFloatNear(50 + original_vowel_duration * (1.0f + (0.8f * config.stress_weight)) + 50, syll_after.duration_ms);
}

TEST_F(PhonologicalProcessorTest, ApplyStressNullInputs) {
    EXPECT_FALSE(phonological_apply_stress(nullptr, 0, 0.5f));
    EXPECT_FALSE(phonological_apply_stress(processor, 99, 0.5f)); // Invalid index
}

// =============================================================================
// Prosody Operations Tests
// =============================================================================

TEST_F(PhonologicalProcessorTest, GenerateProsodyFlat) {
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);
    ASSERT_EQ(2, phonological_get_phoneme_count(processor));

    EXPECT_TRUE(phonological_generate_prosody(processor, INTONATION_PATTERN_FLAT));

    prosody_curve_t curve;
    EXPECT_TRUE(phonological_get_prosody(processor, &curve));
    EXPECT_EQ(INTONATION_PATTERN_FLAT, curve.pattern);
    EXPECT_EQ(2, curve.num_points);
    ExpectFloatNear(config.default_f0, curve.f0_values[0]);
    ExpectFloatNear(config.default_f0, curve.f0_values[1]);
}

TEST_F(PhonologicalProcessorTest, GenerateProsodyRising) {
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);
    ASSERT_EQ(2, phonological_get_phoneme_count(processor));

    EXPECT_TRUE(phonological_generate_prosody(processor, INTONATION_PATTERN_RISING));

    prosody_curve_t curve;
    EXPECT_TRUE(phonological_get_prosody(processor, &curve));
    EXPECT_EQ(INTONATION_PATTERN_RISING, curve.pattern);
    EXPECT_EQ(2, curve.num_points);
    EXPECT_LT(curve.f0_values[0], curve.f0_values[1]);
    ExpectFloatNear(config.default_f0, curve.f0_values[0]);
    ExpectFloatNear(config.default_f0 + curve.f0_range, curve.f0_values[1]);
}

TEST_F(PhonologicalProcessorTest, GenerateProsodyDisabled) {
    config.enable_prosody = false;
    phonological_destroy(processor);
    processor = phonological_create(&config);
    ASSERT_NE(nullptr, processor);

    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    EXPECT_FALSE(phonological_generate_prosody(processor, INTONATION_PATTERN_FLAT));
}

TEST_F(PhonologicalProcessorTest, GenerateProsodyNullInputs) {
    EXPECT_FALSE(phonological_generate_prosody(nullptr, INTONATION_PATTERN_FLAT));
}

TEST_F(PhonologicalProcessorTest, SetBaselineF0) {
    EXPECT_TRUE(phonological_set_baseline_f0(processor, 150.0f));
    prosody_curve_t curve;
    phonological_add_phoneme(processor, 'a');
    phonological_generate_prosody(processor, INTONATION_PATTERN_FLAT);
    phonological_get_prosody(processor, &curve);
    ExpectFloatNear(150.0f, curve.baseline_f0);
    ExpectFloatNear(150.0f, curve.f0_values[0]);
}

TEST_F(PhonologicalProcessorTest, SetBaselineF0Clamping) {
    EXPECT_TRUE(phonological_set_baseline_f0(processor, PROSODY_F0_MIN - 10.0f));
    prosody_curve_t curve;
    phonological_add_phoneme(processor, 'a');
    phonological_generate_prosody(processor, INTONATION_PATTERN_FLAT);
    phonological_get_prosody(processor, &curve);
    ExpectFloatNear(PROSODY_F0_MIN, curve.baseline_f0);

    EXPECT_TRUE(phonological_set_baseline_f0(processor, PROSODY_F0_MAX + 10.0f));
    phonological_generate_prosody(processor, INTONATION_PATTERN_FLAT);
    phonological_get_prosody(processor, &curve);
    ExpectFloatNear(PROSODY_F0_MAX, curve.baseline_f0);
}

TEST_F(PhonologicalProcessorTest, GetProsodyNullInputs) {
    prosody_curve_t curve;
    EXPECT_FALSE(phonological_get_prosody(nullptr, &curve));
    EXPECT_FALSE(phonological_get_prosody(processor, nullptr));
}

// =============================================================================
// Coarticulation Planning Tests
// =============================================================================

TEST_F(PhonologicalProcessorTest, PlanCoarticulation_VC) {
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);

    EXPECT_TRUE(phonological_plan_coarticulation(processor));
}

TEST_F(PhonologicalProcessorTest, PlanCoarticulation_CV) {
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    EXPECT_TRUE(phonological_plan_coarticulation(processor));
}

TEST_F(PhonologicalProcessorTest, PlanCoarticulation_CC_VoicingAssimilation) {
    phonological_add_phoneme_detailed(processor, 'p', PHONEME_CATEGORY_CONSONANT, 50, 0.0f); // Voiceless
    phonological_add_phoneme_detailed(processor, 'b', PHONEME_CATEGORY_CONSONANT, 50, 1.0f); // Voiced
    EXPECT_TRUE(phonological_plan_coarticulation(processor));
}

TEST_F(PhonologicalProcessorTest, PlanCoarticulationDisabled) {
    config.enable_coarticulation = false;
    phonological_destroy(processor);
    processor = phonological_create(&config);
    ASSERT_NE(nullptr, processor);
    phonological_add_phoneme_detailed(processor, 'a', PHONEME_CATEGORY_VOWEL, 100, 1);
    phonological_add_phoneme_detailed(processor, 't', PHONEME_CATEGORY_CONSONANT, 50, 0);
    EXPECT_TRUE(phonological_plan_coarticulation(processor));
}

TEST_F(PhonologicalProcessorTest, PlanCoarticulationNullInputs) {
    EXPECT_FALSE(phonological_plan_coarticulation(nullptr));
}

// =============================================================================
// Status and Query Tests
// =============================================================================

TEST_F(PhonologicalProcessorTest, GetStatus) {
    EXPECT_EQ(PHONOLOGICAL_STATUS_IDLE, phonological_get_status(processor));

    phonological_add_phoneme(processor, 'a');
    EXPECT_EQ(PHONOLOGICAL_STATUS_BUFFERING, phonological_get_status(processor));

    phonological_generate_syllables(processor);
    EXPECT_EQ(PHONOLOGICAL_STATUS_SYLLABIFYING, phonological_get_status(processor)); // Status changes before apply stress
}

TEST_F(PhonologicalProcessorTest, IsReady) {
    // Not ready initially
    EXPECT_FALSE(phonological_is_ready(processor));

    // Add phonemes, generate syllables, generate prosody
    phonological_add_phoneme(processor, 'a');
    phonological_add_phoneme(processor, 't');
    phonological_generate_syllables(processor);
    phonological_generate_prosody(processor, INTONATION_PATTERN_FLAT);

    EXPECT_TRUE(phonological_is_ready(processor));
}

TEST_F(PhonologicalProcessorTest, IsReadyNoProsodyEnabled) {
    config.enable_prosody = false;
    phonological_destroy(processor);
    processor = phonological_create(&config);
    ASSERT_NE(nullptr, processor);

    // Add phonemes, generate syllables (prosody not needed)
    phonological_add_phoneme(processor, 'a');
    phonological_add_phoneme(processor, 't');
    phonological_generate_syllables(processor);

    EXPECT_TRUE(phonological_is_ready(processor));
}

TEST_F(PhonologicalProcessorTest, GetConfig) {
    phonological_config_t retrieved_config;
    EXPECT_TRUE(phonological_get_config(processor, &retrieved_config));
    EXPECT_EQ(config.max_phonemes, retrieved_config.max_phonemes);
    EXPECT_EQ(config.max_syllables, retrieved_config.max_syllables);
    ExpectFloatNear(config.stress_weight, retrieved_config.stress_weight);
    EXPECT_EQ(config.enable_prosody, retrieved_config.enable_prosody);
    EXPECT_EQ(config.enable_coarticulation, retrieved_config.enable_coarticulation);
}

TEST_F(PhonologicalProcessorTest, GetConfigNullInputs) {
    phonological_config_t retrieved_config;
    EXPECT_FALSE(phonological_get_config(nullptr, &retrieved_config));
    EXPECT_FALSE(phonological_get_config(processor, nullptr));
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}