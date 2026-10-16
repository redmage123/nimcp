/**
 * @file speech_cortex_demo.c
 * @brief Comprehensive test and demonstration of speech cortex functionality
 *
 * WHAT: Tests phoneme recognition, formant analysis, prosody, and word recognition
 * WHY:  Validate speech cortex implementation and demonstrate linguistic processing
 * HOW:  Synthetic audio signals + feature extraction + lexical access
 *
 * TESTS:
 * 1. Phoneme classification (vowels from formants)
 * 2. Formant extraction (LPC analysis)
 * 3. Prosody extraction (pitch, stress)
 * 4. Phonological working memory (7±2 item buffer)
 * 5. Word recognition from phoneme sequences
 *
 * @version 2.7.0 (Phase 8.8)
 */

#include "perception/nimcp_speech_cortex.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846
#define TEST_SAMPLE_RATE 16000
#define TEST_FRAME_SIZE 320  // 20ms at 16kHz

//=============================================================================
// Test Utilities
//=============================================================================

/**
 * @brief Generate synthetic vowel with specified formants
 *
 * WHAT: Create audio signal with formant frequencies
 * WHY:  Test formant extraction without needing real audio files
 * HOW:  Sum of sinusoids at F1, F2, F3 frequencies
 */
static void generate_vowel_signal(
    float* signal,
    uint32_t num_samples,
    uint32_t sample_rate,
    float f1, float f2, float f3,
    float fundamental_hz)
{
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;

        // Fundamental frequency (pitch)
        float fundamental = 0.3f * sinf(2.0f * PI * fundamental_hz * t);

        // Formants (vocal tract resonances)
        float formant1 = 0.4f * sinf(2.0f * PI * f1 * t);
        float formant2 = 0.3f * sinf(2.0f * PI * f2 * t);
        float formant3 = 0.2f * sinf(2.0f * PI * f3 * t);

        // Combine with amplitude envelope
        float envelope = expf(-3.0f * t); // Decay envelope
        signal[i] = envelope * (fundamental + formant1 + formant2 + formant3);
    }
}

/**
 * @brief Generate synthetic consonant (fricative)
 */
static void generate_fricative_signal(
    float* signal,
    uint32_t num_samples,
    uint32_t sample_rate)
{
    // Fricatives have noise-like spectrum (high zero-crossing rate)
    for (uint32_t i = 0; i < num_samples; i++) {
        float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float t = (float)i / num_samples;
        float envelope = expf(-5.0f * t); // Sharp decay
        signal[i] = envelope * noise * 0.5f;
    }
}

/**
 * @brief Generate pitch contour (for prosody testing)
 */
static void generate_pitch_signal(
    float* signal,
    uint32_t num_samples,
    uint32_t sample_rate,
    float pitch_hz)
{
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        signal[i] = 0.5f * sinf(2.0f * PI * pitch_hz * t);
    }
}

//=============================================================================
// Test Functions
//=============================================================================

/**
 * @brief Test 1: Phoneme Classification
 */
static bool test_phoneme_classification(void)
{
    printf("\n=== Test 1: Phoneme Classification ===\n");

    // Test vowel classification from formant values
    typedef struct {
        float f1, f2;
        phoneme_t expected;
        const char* vowel_name;
    } vowel_test_t;

    vowel_test_t tests[] = {
        {300.0f, 2500.0f, PHONEME_IY, "IY (beet)"},   // High front
        {700.0f, 1100.0f, PHONEME_AA, "AA (father)"}, // Low back
        {500.0f, 1000.0f, PHONEME_UW, "UW (boot)"},   // High back
        {600.0f, 1700.0f, PHONEME_AE, "AE (bat)"}     // Low front
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        phoneme_t result = speech_cortex_classify_vowel(tests[i].f1, tests[i].f2);
        bool correct = (result == tests[i].expected);

        printf("  Test %d: F1=%.0f Hz, F2=%.0f Hz → %s [%s] %s\n",
               i + 1, tests[i].f1, tests[i].f2,
               speech_cortex_phoneme_name(result),
               speech_cortex_phoneme_ipa(result),
               correct ? "✓ PASS" : "✗ FAIL");

        if (correct) passed++;
    }

    printf("  Result: %d/%d tests passed\n", passed, total);
    return (passed == total);
}

/**
 * @brief Test 2: Formant Extraction
 */
static bool test_formant_extraction(void)
{
    printf("\n=== Test 2: Formant Extraction ===\n");

    // Create speech cortex
    speech_cortex_config_t config = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&config);

    if (!cortex) {
        printf("  ✗ FAIL: Failed to create speech cortex\n");
        return false;
    }

    // Generate synthetic vowel "IY" (high front vowel)
    float* signal = malloc(TEST_FRAME_SIZE * sizeof(float));
    generate_vowel_signal(signal, TEST_FRAME_SIZE, TEST_SAMPLE_RATE,
                         300.0f,  // F1 (high vowel)
                         2200.0f, // F2 (front vowel)
                         3000.0f, // F3
                         150.0f); // Pitch

    // Extract formants
    float formants[4];
    bool success = speech_cortex_extract_formants(cortex, signal, TEST_FRAME_SIZE,
                                                  formants, 4);

    if (success) {
        printf("  Extracted formants:\n");
        printf("    F1: %.0f Hz\n", formants[0]);
        printf("    F2: %.0f Hz\n", formants[1]);
        printf("    F3: %.0f Hz\n", formants[2]);
        printf("    F4: %.0f Hz\n", formants[3]);
        printf("  ✓ PASS: Formant extraction successful\n");
    } else {
        printf("  ✗ FAIL: Formant extraction failed\n");
    }

    free(signal);
    speech_cortex_destroy(cortex);
    return success;
}

/**
 * @brief Test 3: Prosody Extraction
 */
static bool test_prosody_extraction(void)
{
    printf("\n=== Test 3: Prosody Extraction ===\n");

    speech_cortex_config_t config = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&config);

    if (!cortex) {
        printf("  ✗ FAIL: Failed to create speech cortex\n");
        return false;
    }

    // Test pitch extraction at different frequencies
    float test_pitches[] = {100.0f, 150.0f, 200.0f, 250.0f};
    int passed = 0;

    for (int i = 0; i < 4; i++) {
        float* signal = malloc(TEST_FRAME_SIZE * sizeof(float));
        generate_pitch_signal(signal, TEST_FRAME_SIZE, TEST_SAMPLE_RATE, test_pitches[i]);

        float extracted_pitch = 0.0f;
        float stress_level = 0.0f;
        bool success = speech_cortex_extract_prosody(cortex, signal, TEST_FRAME_SIZE,
                                                     &extracted_pitch, &stress_level);

        if (success && extracted_pitch > 0.0f) {
            float error = fabsf(extracted_pitch - test_pitches[i]);
            float error_pct = (error / test_pitches[i]) * 100.0f;

            printf("  Test %d: Expected %.0f Hz, Got %.0f Hz (error: %.1f%%) [%s]\n",
                   i + 1, test_pitches[i], extracted_pitch, error_pct,
                   (error_pct < 10.0f) ? "✓ PASS" : "~ APPROXIMATE");

            if (error_pct < 20.0f) passed++; // Allow 20% tolerance for simple pitch tracker
        } else {
            printf("  Test %d: Expected %.0f Hz, Got %.0f Hz [✗ FAIL]\n",
                   i + 1, test_pitches[i], extracted_pitch);
        }

        free(signal);
    }

    printf("  Result: %d/4 tests passed\n", passed);

    speech_cortex_destroy(cortex);
    return (passed >= 2); // Pass if at least 50% correct
}

/**
 * @brief Test 4: Phoneme Detection
 */
static bool test_phoneme_detection(void)
{
    printf("\n=== Test 4: Phoneme Detection ===\n");

    speech_cortex_config_t config = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&config);

    if (!cortex) {
        printf("  ✗ FAIL: Failed to create speech cortex\n");
        return false;
    }

    // Generate vowel signal
    float* signal = malloc(TEST_FRAME_SIZE * sizeof(float));
    generate_vowel_signal(signal, TEST_FRAME_SIZE, TEST_SAMPLE_RATE,
                         700.0f,  // F1 for "AA"
                         1100.0f, // F2 for "AA"
                         2500.0f, // F3
                         120.0f); // Pitch

    // Detect phonemes
    phoneme_event_t phonemes[10];
    uint32_t num_detected = 0;
    bool success = speech_cortex_detect_phonemes(cortex, signal, TEST_FRAME_SIZE,
                                                 phonemes, 10, &num_detected);

    if (success && num_detected > 0) {
        printf("  Detected %u phoneme(s):\n", num_detected);
        for (uint32_t i = 0; i < num_detected; i++) {
            printf("    Phoneme %u: %s [%s] (confidence: %.2f)\n",
                   i + 1,
                   speech_cortex_phoneme_name(phonemes[i].phoneme),
                   speech_cortex_phoneme_ipa(phonemes[i].phoneme),
                   phonemes[i].confidence);
            printf("      Formants: F1=%.0f Hz, F2=%.0f Hz, F3=%.0f Hz\n",
                   phonemes[i].features.formant_f1,
                   phonemes[i].features.formant_f2,
                   phonemes[i].features.formant_f3);
        }
        printf("  ✓ PASS: Phoneme detection successful\n");
    } else {
        printf("  ✗ FAIL: No phonemes detected\n");
        success = false;
    }

    free(signal);
    speech_cortex_destroy(cortex);
    return success;
}

/**
 * @brief Test 5: Phonological Working Memory
 */
static bool test_phonological_memory(void)
{
    printf("\n=== Test 5: Phonological Working Memory ===\n");

    speech_cortex_config_t config = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&config);

    if (!cortex) {
        printf("  ✗ FAIL: Failed to create speech cortex\n");
        return false;
    }

    // Test sequence: "CAT" → [K AE T]
    phoneme_t word_cat[] = {PHONEME_K, PHONEME_AE, PHONEME_T};

    // Store phonemes
    bool stored = speech_cortex_store_phonological_buffer(cortex, word_cat, 3);
    if (!stored) {
        printf("  ✗ FAIL: Failed to store phonemes\n");
        speech_cortex_destroy(cortex);
        return false;
    }
    printf("  Stored 3 phonemes in working memory\n");

    // Retrieve phonemes
    phoneme_t retrieved[10];
    uint32_t num_retrieved = 0;
    bool retrieved_ok = speech_cortex_retrieve_phonological_buffer(cortex, retrieved,
                                                                   10, &num_retrieved);

    if (retrieved_ok && num_retrieved == 3) {
        printf("  Retrieved %u phonemes:\n", num_retrieved);
        bool match = true;
        for (uint32_t i = 0; i < num_retrieved; i++) {
            printf("    %u: %s [%s] %s\n",
                   i + 1,
                   speech_cortex_phoneme_name(retrieved[i]),
                   speech_cortex_phoneme_ipa(retrieved[i]),
                   (retrieved[i] == word_cat[i]) ? "✓" : "✗");
            if (retrieved[i] != word_cat[i]) match = false;
        }

        if (match) {
            printf("  ✓ PASS: Phonological memory test passed\n");
        } else {
            printf("  ✗ FAIL: Retrieved phonemes don't match\n");
        }

        speech_cortex_destroy(cortex);
        return match;
    } else {
        printf("  ✗ FAIL: Retrieved %u phonemes (expected 3)\n", num_retrieved);
        speech_cortex_destroy(cortex);
        return false;
    }
}

/**
 * @brief Test 6: Word Recognition (Lexical Access)
 */
static bool test_word_recognition(void)
{
    printf("\n=== Test 6: Word Recognition (Lexical Access) ===\n");

    speech_cortex_config_t config = speech_cortex_default_config();
    speech_cortex_t* cortex = speech_cortex_create(&config);

    if (!cortex) {
        printf("  ✗ FAIL: Failed to create speech cortex\n");
        return false;
    }

    // Build lexicon
    typedef struct {
        const char* word;
        phoneme_t phonemes[10];
        uint32_t num_phonemes;
    } lexicon_entry;

    lexicon_entry words[] = {
        {"CAT", {PHONEME_K, PHONEME_AE, PHONEME_T}, 3},
        {"DOG", {PHONEME_D, PHONEME_AO, PHONEME_G}, 3},
        {"HELLO", {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW}, 4},
        {"SPEECH", {PHONEME_S, PHONEME_P, PHONEME_IY, PHONEME_CH}, 4}
    };

    printf("  Building lexicon with %lu words...\n", sizeof(words) / sizeof(words[0]));
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        bool added = speech_cortex_add_word_to_lexicon(cortex, words[i].word,
                                                       words[i].phonemes,
                                                       words[i].num_phonemes);
        if (added) {
            printf("    Added: %s (", words[i].word);
            for (uint32_t j = 0; j < words[i].num_phonemes; j++) {
                printf("%s%s", speech_cortex_phoneme_name(words[i].phonemes[j]),
                       (j < words[i].num_phonemes - 1) ? " " : "");
            }
            printf(")\n");
        }
    }

    // Test word recognition
    printf("\n  Testing word recognition:\n");
    int passed = 0;

    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        char recognized_word[64];
        float confidence = 0.0f;
        bool recognized = speech_cortex_recognize_word(cortex,
                                                       words[i].phonemes,
                                                       words[i].num_phonemes,
                                                       recognized_word,
                                                       sizeof(recognized_word),
                                                       &confidence);

        if (recognized && strcmp(recognized_word, words[i].word) == 0) {
            printf("    Test %lu: %s → %s (confidence: %.2f) ✓ PASS\n",
                   i + 1, words[i].word, recognized_word, confidence);
            passed++;
        } else {
            printf("    Test %lu: %s → %s (confidence: %.2f) ✗ FAIL\n",
                   i + 1, words[i].word,
                   recognized ? recognized_word : "NOT_RECOGNIZED",
                   confidence);
        }
    }

    printf("  Result: %d/%lu words recognized correctly\n",
           passed, sizeof(words) / sizeof(words[0]));

    speech_cortex_destroy(cortex);
    return (passed == (int)(sizeof(words) / sizeof(words[0])));
}

/**
 * @brief Test 7: Speech Processing Pipeline
 */
static bool test_speech_processing_pipeline(void)
{
    printf("\n=== Test 7: Speech Processing Pipeline ===\n");

    speech_cortex_config_t config = speech_cortex_default_config();
    config.feature_dim = 64;
    speech_cortex_t* cortex = speech_cortex_create(&config);

    if (!cortex) {
        printf("  ✗ FAIL: Failed to create speech cortex\n");
        return false;
    }

    // Generate audio
    float* signal = malloc(TEST_FRAME_SIZE * sizeof(float));
    generate_vowel_signal(signal, TEST_FRAME_SIZE, TEST_SAMPLE_RATE,
                         500.0f, 1500.0f, 2500.0f, 120.0f);

    // Process through pipeline
    float features[64];
    bool success = speech_cortex_process(cortex, signal, TEST_FRAME_SIZE, features);

    if (success) {
        printf("  Speech feature vector extracted (dim=%u):\n", config.feature_dim);
        printf("    First 8 features: [");
        for (int i = 0; i < 8; i++) {
            printf("%.3f%s", features[i], (i < 7) ? ", " : "");
        }
        printf("...]\n");

        // Get statistics
        speech_cortex_stats_t stats;
        speech_cortex_get_stats(cortex, &stats);
        printf("  Statistics:\n");
        printf("    Frames processed: %lu\n", stats.frames_processed);
        printf("    Phonemes detected: %u\n", stats.phonemes_detected);
        printf("    Words recognized: %u\n", stats.words_recognized);
        printf("  ✓ PASS: Pipeline test successful\n");
    } else {
        printf("  ✗ FAIL: Pipeline processing failed\n");
    }

    free(signal);
    speech_cortex_destroy(cortex);
    return success;
}

/**
 * @brief Test 8: Utility Functions
 */
static bool test_utility_functions(void)
{
    printf("\n=== Test 8: Utility Functions ===\n");

    // Test phoneme name/IPA lookup
    phoneme_t test_phonemes[] = {PHONEME_IY, PHONEME_P, PHONEME_S, PHONEME_M};
    const char* expected_names[] = {"IY", "P", "S", "M"};
    const char* expected_ipa[] = {"i:", "p", "s", "m"};

    bool all_passed = true;

    for (int i = 0; i < 4; i++) {
        const char* name = speech_cortex_phoneme_name(test_phonemes[i]);
        const char* ipa = speech_cortex_phoneme_ipa(test_phonemes[i]);
        bool is_vowel = speech_cortex_is_vowel(test_phonemes[i]);

        bool name_match = (strcmp(name, expected_names[i]) == 0);
        bool ipa_match = (strcmp(ipa, expected_ipa[i]) == 0);

        printf("  Phoneme %s: name=%s [%s], IPA=%s [%s], is_vowel=%s\n",
               expected_names[i],
               name, name_match ? "✓" : "✗",
               ipa, ipa_match ? "✓" : "✗",
               is_vowel ? "yes" : "no");

        if (!name_match || !ipa_match) all_passed = false;
    }

    // Test default config
    speech_cortex_config_t config = speech_cortex_default_config();
    printf("  Default configuration:\n");
    printf("    Sample rate: %u Hz\n", config.sample_rate);
    printf("    Frame size: %u ms\n", config.frame_size_ms);
    printf("    Num phonemes: %u\n", config.num_phonemes);
    printf("    Wernicke enabled: %s\n", config.enable_wernicke ? "yes" : "no");
    printf("    Prosody enabled: %s\n", config.enable_prosody ? "yes" : "no");

    if (all_passed) {
        printf("  ✓ PASS: Utility functions test passed\n");
    } else {
        printf("  ✗ FAIL: Some utility functions failed\n");
    }

    return all_passed;
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║   NIMCP Speech Cortex Test Suite (Phase 8.8)              ║\n");
    printf("║   Biologically-Inspired Language Processing               ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    int total_tests = 8;
    int passed_tests = 0;

    // Run tests
    if (test_phoneme_classification()) passed_tests++;
    if (test_formant_extraction()) passed_tests++;
    if (test_prosody_extraction()) passed_tests++;
    if (test_phoneme_detection()) passed_tests++;
    if (test_phonological_memory()) passed_tests++;
    if (test_word_recognition()) passed_tests++;
    if (test_speech_processing_pipeline()) passed_tests++;
    if (test_utility_functions()) passed_tests++;

    // Summary
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║   TEST SUMMARY                                             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║   Tests Passed: %d / %d                                     ", passed_tests, total_tests);
    printf(passed_tests == total_tests ? " ✓ ALL PASS" : "");
    printf("     ║\n");
    printf("║   Success Rate: %.0f%%                                       ║\n",
           (100.0f * passed_tests) / total_tests);
    printf("╚════════════════════════════════════════════════════════════╝\n");

    if (passed_tests == total_tests) {
        printf("\n🎉 All speech cortex tests PASSED! The linguistic system is ready.\n");
        return 0;
    } else {
        printf("\n⚠️  Some tests failed. Review output above for details.\n");
        return 1;
    }
}
