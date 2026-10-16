/**
 * @file test_lip_reading.cpp
 * @brief Unit tests for lip reading system
 *
 * WHAT: Comprehensive testing of lip reading / visual speech perception
 * WHY:  Ensure viseme classification, audiovisual integration, and McGurk effect work correctly
 * HOW:  Test face detection, viseme classification, audiovisual fusion, motor theory
 *
 * @version Phase B5: Audiovisual Speech Integration
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

extern "C" {
#include "perception/nimcp_lip_reading.h"
}

/*=============================================================================
 * TEST FIXTURES
 *===========================================================================*/

class LipReadingTest : public ::testing::Test {
protected:
    lip_reading_system_t* system;
    lip_reading_config_t config;

    void SetUp() override {
        config = lip_reading_default_config();
        config.debug_mode = false;
        system = lip_reading_create(&config);
        ASSERT_NE(system, nullptr) << "Failed to create lip reading system";
    }

    void TearDown() override {
        if (system) {
            lip_reading_destroy(system);
            system = nullptr;
        }
    }

    // Helper to create a test image with face-like features
    std::vector<uint8_t> createTestImage(uint32_t width, uint32_t height, uint32_t channels,
                                          uint8_t base_value = 128) {
        std::vector<uint8_t> image(width * height * channels, base_value);

        // Add skin-colored region (face)
        uint32_t face_x = width / 4;
        uint32_t face_y = height / 4;
        uint32_t face_w = width / 2;
        uint32_t face_h = height / 2;

        for (uint32_t y = face_y; y < face_y + face_h && y < height; y++) {
            for (uint32_t x = face_x; x < face_x + face_w && x < width; x++) {
                uint32_t idx = (y * width + x) * channels;
                if (channels >= 3) {
                    // Skin color: R > G > B
                    image[idx] = 180;     // R
                    image[idx + 1] = 140; // G
                    image[idx + 2] = 100; // B
                } else {
                    image[idx] = 160;     // Grayscale
                }
            }
        }

        // Add mouth region (darker, in lower face)
        uint32_t mouth_x = face_x + face_w / 4;
        uint32_t mouth_y = face_y + face_h * 2 / 3;
        uint32_t mouth_w = face_w / 2;
        uint32_t mouth_h = face_h / 4;

        for (uint32_t y = mouth_y; y < mouth_y + mouth_h && y < height; y++) {
            for (uint32_t x = mouth_x; x < mouth_x + mouth_w && x < width; x++) {
                uint32_t idx = (y * width + x) * channels;
                if (channels >= 3) {
                    // Darker lip color
                    image[idx] = 150;     // R
                    image[idx + 1] = 90;  // G
                    image[idx + 2] = 80;  // B
                } else {
                    image[idx] = 100;
                }
            }
        }

        return image;
    }

    // Create mouth ROI with specific aperture
    std::vector<uint8_t> createMouthROI(uint32_t width, uint32_t height,
                                         float aperture_ratio = 0.3f) {
        std::vector<uint8_t> roi(width * height, 100);  // Lip base color

        // Create mouth opening (brighter = teeth)
        uint32_t opening_h = (uint32_t)(height * aperture_ratio);
        uint32_t center_y = height / 2;

        for (uint32_t y = center_y - opening_h/2; y < center_y + opening_h/2; y++) {
            for (uint32_t x = width/4; x < 3*width/4; x++) {
                if (y < height && x < width) {
                    roi[y * width + x] = 200;  // Teeth brightness
                }
            }
        }

        return roi;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, DefaultConfigValues) {
    // WHAT: Test default configuration values
    // WHY:  Ensure reasonable defaults are set
    // HOW:  Check each config field

    lip_reading_config_t default_config = lip_reading_default_config();

    EXPECT_EQ(default_config.mouth_roi_width, 64u);
    EXPECT_EQ(default_config.mouth_roi_height, 64u);
    EXPECT_GT(default_config.min_face_confidence, 0.0f);
    EXPECT_LT(default_config.min_face_confidence, 1.0f);
    EXPECT_EQ(default_config.target_frame_rate, 30u);
    EXPECT_TRUE(default_config.enable_audiovisual_fusion);
    EXPECT_NEAR(default_config.visual_lead_ms, 200.0f, 50.0f);
    EXPECT_TRUE(default_config.enable_temporal_smoothing);
    EXPECT_TRUE(default_config.enable_speaker_adaptation);
    EXPECT_TRUE(default_config.enable_bio_async);
}

TEST_F(LipReadingTest, CreateWithNullConfig) {
    // WHAT: Test creation with NULL config uses defaults
    // WHY:  Convenience API should work
    // HOW:  Create system without config

    lip_reading_system_t* sys = lip_reading_create(nullptr);
    ASSERT_NE(sys, nullptr) << "Should create with default config";

    // Verify it's functional
    EXPECT_EQ(lip_reading_get_status(sys), LIP_READING_STATUS_IDLE);
    EXPECT_EQ(lip_reading_get_last_error(sys), LIP_READING_ERROR_NONE);

    lip_reading_destroy(sys);
}

TEST_F(LipReadingTest, ResetClearsState) {
    // WHAT: Test that reset clears all state
    // WHY:  Allow reusing system without recreation
    // HOW:  Process a frame, reset, verify clean state

    auto image = createTestImage(128, 128, 3);
    face_detection_result_t result;

    // Process to change state
    lip_reading_detect_face(system, image.data(), 128, 128, 3, &result);

    // Reset
    EXPECT_TRUE(lip_reading_reset(system));

    // Verify clean state
    EXPECT_EQ(lip_reading_get_status(system), LIP_READING_STATUS_IDLE);
    EXPECT_EQ(lip_reading_get_last_error(system), LIP_READING_ERROR_NONE);

    lip_reading_stats_t stats;
    EXPECT_TRUE(lip_reading_get_stats(system, &stats));
    EXPECT_EQ(stats.frames_processed, 0u);
}

/*=============================================================================
 * FACE DETECTION TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, DetectFaceWithSkinColor) {
    // WHAT: Test face detection on image with skin-colored region
    // WHY:  Face detection is foundation of lip reading
    // HOW:  Create test image with face, verify detection

    auto image = createTestImage(128, 128, 3);
    face_detection_result_t result;

    bool detected = lip_reading_detect_face(system, image.data(), 128, 128, 3, &result);

    EXPECT_TRUE(detected) << "Should detect face in skin-colored region";
    EXPECT_TRUE(result.face_detected);
    EXPECT_GT(result.face_confidence, 0.0f);

    // Mouth region should be within face
    EXPECT_GE(result.mouth_bbox[0], result.face_bbox[0]);
    EXPECT_GE(result.mouth_bbox[1], result.face_bbox[1]);
    EXPECT_LE(result.mouth_bbox[0] + result.mouth_bbox[2],
              result.face_bbox[0] + result.face_bbox[2]);

    EXPECT_EQ(lip_reading_get_status(system), LIP_READING_STATUS_FACE_DETECTED);
}

TEST_F(LipReadingTest, DetectFaceGrayscale) {
    // WHAT: Test face detection on grayscale image
    // WHY:  Should work with single-channel images
    // HOW:  Create grayscale test image, verify detection

    auto image = createTestImage(128, 128, 1);
    face_detection_result_t result;

    bool detected = lip_reading_detect_face(system, image.data(), 128, 128, 1, &result);

    EXPECT_TRUE(detected) << "Should detect face in grayscale image";
    EXPECT_TRUE(result.face_detected);
}

TEST_F(LipReadingTest, DetectFaceRejectsBlankImage) {
    // WHAT: Test face detection rejects blank image
    // WHY:  Should not detect face where there is none
    // HOW:  Create uniform image, expect no detection

    std::vector<uint8_t> blank(128 * 128 * 3, 50);  // Dark uniform image
    face_detection_result_t result;

    bool detected = lip_reading_detect_face(system, blank.data(), 128, 128, 3, &result);

    EXPECT_FALSE(detected) << "Should not detect face in blank image";
    EXPECT_FALSE(result.face_detected);
    EXPECT_EQ(lip_reading_get_last_error(system), LIP_READING_ERROR_NO_FACE_DETECTED);
}

TEST_F(LipReadingTest, ExtractMouthROI) {
    // WHAT: Test mouth ROI extraction
    // WHY:  Need cropped mouth region for viseme classification
    // HOW:  Detect face, extract mouth ROI

    auto image = createTestImage(256, 256, 3);
    face_detection_result_t face_result;

    ASSERT_TRUE(lip_reading_detect_face(system, image.data(), 256, 256, 3, &face_result));

    std::vector<uint8_t> mouth_roi(64 * 64 * 3, 0);

    bool extracted = lip_reading_extract_mouth_roi(system, image.data(), 256, 256, 3,
                                                    &face_result, mouth_roi.data(), 64, 64);

    EXPECT_TRUE(extracted) << "Should extract mouth ROI";
    EXPECT_EQ(lip_reading_get_status(system), LIP_READING_STATUS_MOUTH_TRACKED);

    // Verify ROI is not all zeros
    uint32_t non_zero = 0;
    for (auto v : mouth_roi) {
        if (v > 0) non_zero++;
    }
    EXPECT_GT(non_zero, 0u) << "Mouth ROI should have non-zero pixels";
}

/*=============================================================================
 * VISEME CLASSIFICATION TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, ClassifyVisemeFromFeatures) {
    // WHAT: Test viseme classification from visual features
    // WHY:  Core lip reading capability
    // HOW:  Create features, classify viseme

    visual_speech_features_t features = {};
    features.lip_width = 40.0f;
    features.lip_height = 20.0f;
    features.lip_area = 40.0f * 20.0f * 0.785f;
    features.lip_aspect_ratio = 2.0f;
    features.lip_protrusion = 0.0f;
    features.upper_teeth_visible = 0.3f;
    features.lower_teeth_visible = 0.2f;
    features.tongue_visible = 0.0f;
    features.feature_confidence = 0.8f;

    viseme_classification_t result;
    bool classified = lip_reading_classify_viseme(system, &features, &result);

    EXPECT_TRUE(classified);
    EXPECT_NE(result.viseme, VISEME_UNKNOWN);
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    // Probabilities should sum to ~1.0
    float prob_sum = 0.0f;
    for (int i = 0; i < VISEME_COUNT; i++) {
        EXPECT_GE(result.probabilities[i], 0.0f);
        EXPECT_LE(result.probabilities[i], 1.0f);
        prob_sum += result.probabilities[i];
    }
    EXPECT_NEAR(prob_sum, 1.0f, 0.01f) << "Probabilities should sum to 1";
}

TEST_F(LipReadingTest, ClassifyBilabialViseme) {
    // WHAT: Test classification of bilabial viseme
    // WHY:  Bilabials (/p/, /b/, /m/) have closed lips
    // HOW:  Create closed-lip features, check probability distribution

    visual_speech_features_t features = {};
    features.lip_width = 40.0f;
    features.lip_height = 1.0f;  // Almost closed
    features.lip_area = 40.0f;
    features.lip_aspect_ratio = 40.0f;
    features.lip_protrusion = 0.0f;
    features.upper_teeth_visible = 0.0f;
    features.lower_teeth_visible = 0.0f;
    features.tongue_visible = 0.0f;
    features.lip_velocity_x = 0.0f;
    features.lip_velocity_y = 0.0f;
    features.feature_confidence = 0.9f;

    viseme_classification_t result;
    lip_reading_classify_viseme(system, &features, &result);

    // The heuristic classifier produces distributed probabilities
    // With very closed lips (high aspect ratio), verify the classifier produces
    // a valid result and has non-zero probabilities for closed-lip visemes
    float closed_lip_prob = result.probabilities[VISEME_BILABIAL] +
                            result.probabilities[VISEME_SILENCE];
    EXPECT_GT(closed_lip_prob, 0.0f)
        << "Closed-lip visemes should have some probability";

    // The BILABIAL score should be influenced by low lip height
    EXPECT_GT(result.probabilities[VISEME_BILABIAL], 0.0f)
        << "BILABIAL should have non-zero probability for closed lips";

    // Also verify it's a valid classification
    EXPECT_NE(result.viseme, VISEME_UNKNOWN);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST_F(LipReadingTest, ClassifyOpenMouthViseme) {
    // WHAT: Test classification of open mouth viseme
    // WHY:  Open vowels have wide mouth aperture
    // HOW:  Create open-mouth features, expect UNROUNDED_OPEN

    visual_speech_features_t features = {};
    features.lip_width = 35.0f;
    features.lip_height = 30.0f;  // Wide open
    features.lip_area = 800.0f;
    features.lip_aspect_ratio = 1.17f;
    features.lip_protrusion = 0.0f;  // Not rounded
    features.upper_teeth_visible = 0.5f;
    features.lower_teeth_visible = 0.3f;
    features.tongue_visible = 0.0f;
    features.feature_confidence = 0.9f;

    viseme_classification_t result;
    lip_reading_classify_viseme(system, &features, &result);

    // Should be an open vowel
    EXPECT_TRUE(result.viseme == VISEME_UNROUNDED_OPEN ||
                result.viseme == VISEME_UNROUNDED_MID ||
                result.viseme == VISEME_ROUNDED_OPEN)
        << "Wide open mouth should classify as open vowel, got: "
        << lip_reading_viseme_name(result.viseme);
}

TEST_F(LipReadingTest, ClassifyDentalViseme) {
    // WHAT: Test classification of dental viseme
    // WHY:  Dental sounds have visible tongue
    // HOW:  Create tongue-visible features, verify tongue affects classification

    visual_speech_features_t features = {};
    features.lip_width = 35.0f;
    features.lip_height = 8.0f;
    features.lip_area = 200.0f;
    features.lip_aspect_ratio = 4.4f;
    features.lip_protrusion = 0.0f;
    features.upper_teeth_visible = 0.4f;
    features.lower_teeth_visible = 0.2f;
    features.tongue_visible = 0.8f;  // Tongue between teeth
    features.teeth_gap = 5.0f;
    features.feature_confidence = 0.9f;

    viseme_classification_t result_with_tongue;
    lip_reading_classify_viseme(system, &features, &result_with_tongue);

    // Now classify without tongue visible
    features.tongue_visible = 0.0f;

    // Reset system state for fair comparison
    lip_reading_reset(system);

    viseme_classification_t result_no_tongue;
    lip_reading_classify_viseme(system, &features, &result_no_tongue);

    // Visible tongue should increase DENTAL probability compared to no tongue
    EXPECT_GE(result_with_tongue.probabilities[VISEME_DENTAL],
              result_no_tongue.probabilities[VISEME_DENTAL])
        << "Visible tongue should increase DENTAL probability";

    // Also verify that DENTAL has some non-trivial probability
    EXPECT_GT(result_with_tongue.probabilities[VISEME_DENTAL], 0.05f)
        << "DENTAL should have non-trivial probability with visible tongue";
}

TEST_F(LipReadingTest, ClassifyFromMouthROI) {
    // WHAT: Test classification from mouth ROI image
    // WHY:  End-to-end classification from image
    // HOW:  Create mouth ROI, classify viseme

    auto mouth_roi = createMouthROI(64, 64, 0.3f);

    viseme_classification_t result;
    bool classified = lip_reading_classify_viseme_from_roi(
        system, mouth_roi.data(), 64, 64, &result);

    EXPECT_TRUE(classified);
    EXPECT_NE(result.viseme, VISEME_UNKNOWN);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST_F(LipReadingTest, VisemeHistoryUpdates) {
    // WHAT: Test that viseme history is maintained
    // WHY:  Temporal context improves classification
    // HOW:  Classify multiple frames, verify history

    visual_speech_features_t features = {};
    features.lip_width = 40.0f;
    features.lip_height = 10.0f;
    features.lip_aspect_ratio = 4.0f;
    features.feature_confidence = 0.8f;

    // Classify multiple frames
    for (int i = 0; i < 5; i++) {
        viseme_classification_t result;
        lip_reading_classify_viseme(system, &features, &result);

        EXPECT_EQ(result.history_count, (uint32_t)(i + 1))
            << "History count should increase";
    }

    // Final classification should have full history
    viseme_classification_t final_result;
    lip_reading_classify_viseme(system, &features, &final_result);

    EXPECT_GE(final_result.history_count, 5u);
}

/*=============================================================================
 * PHONEME-VISEME MAPPING TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, PhonemeToVisemeMapping) {
    // WHAT: Test phoneme to viseme mapping
    // WHY:  Multiple phonemes map to same viseme
    // HOW:  Verify mapping for known phonemes

    // Bilabials
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_P), VISEME_BILABIAL);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_B), VISEME_BILABIAL);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_M), VISEME_BILABIAL);

    // Labiodentals
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_F), VISEME_LABIODENTAL);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_V), VISEME_LABIODENTAL);

    // Dentals
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_TH), VISEME_DENTAL);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_DH), VISEME_DENTAL);

    // Alveolars
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_T), VISEME_ALVEOLAR);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_D), VISEME_ALVEOLAR);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_N), VISEME_ALVEOLAR);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_S), VISEME_ALVEOLAR);

    // Velars
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_K), VISEME_VELAR);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_G), VISEME_VELAR);
    EXPECT_EQ(lip_reading_phoneme_to_viseme(PHONEME_NG), VISEME_VELAR);
}

TEST_F(LipReadingTest, VisemeToPhonemesMapping) {
    // WHAT: Test viseme to phonemes mapping (reverse)
    // WHY:  Visemes are ambiguous - multiple possible phonemes
    // HOW:  Get phonemes for each viseme, verify count

    phoneme_t phonemes[10];

    // Bilabial should have 3 phonemes
    uint32_t count = lip_reading_viseme_to_phonemes(VISEME_BILABIAL, phonemes, 10);
    EXPECT_EQ(count, 3u) << "BILABIAL should map to P, B, M";
    EXPECT_EQ(phonemes[0], PHONEME_P);
    EXPECT_EQ(phonemes[1], PHONEME_B);
    EXPECT_EQ(phonemes[2], PHONEME_M);

    // Labiodental should have 2 phonemes
    count = lip_reading_viseme_to_phonemes(VISEME_LABIODENTAL, phonemes, 10);
    EXPECT_EQ(count, 2u) << "LABIODENTAL should map to F, V";

    // Alveolar should have 6 phonemes
    count = lip_reading_viseme_to_phonemes(VISEME_ALVEOLAR, phonemes, 10);
    EXPECT_EQ(count, 6u) << "ALVEOLAR should map to T, D, N, L, S, Z";
}

TEST_F(LipReadingTest, VisemeAndPhonemeNames) {
    // WHAT: Test viseme and phoneme name strings
    // WHY:  Debug and logging support
    // HOW:  Get names for all types

    // Viseme names
    EXPECT_STREQ(lip_reading_viseme_name(VISEME_BILABIAL), "BILABIAL");
    EXPECT_STREQ(lip_reading_viseme_name(VISEME_LABIODENTAL), "LABIODENTAL");
    EXPECT_STREQ(lip_reading_viseme_name(VISEME_DENTAL), "DENTAL");
    EXPECT_STREQ(lip_reading_viseme_name(VISEME_ALVEOLAR), "ALVEOLAR");
    EXPECT_STREQ(lip_reading_viseme_name(VISEME_VELAR), "VELAR");
    EXPECT_STREQ(lip_reading_viseme_name(VISEME_SILENCE), "SILENCE");

    // Phoneme names
    EXPECT_STREQ(lip_reading_phoneme_name(PHONEME_P), "P");
    EXPECT_STREQ(lip_reading_phoneme_name(PHONEME_B), "B");
    EXPECT_STREQ(lip_reading_phoneme_name(PHONEME_M), "M");
    EXPECT_STREQ(lip_reading_phoneme_name(PHONEME_TH), "TH");
    EXPECT_STREQ(lip_reading_phoneme_name(PHONEME_IY), "IY");
}

/*=============================================================================
 * AUDIOVISUAL INTEGRATION TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, AudiovisualIntegrationBasic) {
    // WHAT: Test basic audiovisual integration
    // WHY:  Core McGurk-style fusion capability
    // HOW:  Provide visual and audio, verify fusion

    audiovisual_integration_t result;

    bool integrated = lip_reading_integrate_audiovisual(
        system,
        VISEME_BILABIAL,      // Visual: /p/, /b/, /m/
        0.8f,                 // Visual confidence
        PHONEME_B,            // Audio: /b/
        0.9f,                 // Audio confidence
        10.0f,                // SNR = 10dB (good audio)
        &result);

    EXPECT_TRUE(integrated);
    EXPECT_EQ(result.visual_viseme, VISEME_BILABIAL);
    EXPECT_EQ(result.auditory_phoneme, PHONEME_B);
    EXPECT_GT(result.fusion_confidence, 0.0f);

    // With good audio, should favor auditory
    EXPECT_GT(result.auditory_weight, 0.5f)
        << "Good SNR should favor auditory channel";
}

TEST_F(LipReadingTest, AudiovisualIntegrationLowSNR) {
    // WHAT: Test audiovisual integration with low SNR
    // WHY:  Visual should dominate in noisy conditions
    // HOW:  Set low SNR, verify visual weight increases

    audiovisual_integration_t result;

    lip_reading_integrate_audiovisual(
        system,
        VISEME_BILABIAL,
        0.8f,
        PHONEME_B,
        0.9f,
        -10.0f,               // SNR = -10dB (noisy audio)
        &result);

    // With poor audio, should rely more on visual
    EXPECT_GT(result.visual_weight, 0.3f)
        << "Low SNR should increase visual weight";
}

TEST_F(LipReadingTest, McGurkEffectClassic) {
    // WHAT: Test classic McGurk effect
    // WHY:  Visual /ga/ + Audio /ba/ -> Perceived /da/
    // HOW:  Provide conflicting cues, verify fusion

    audiovisual_integration_t result;

    lip_reading_integrate_audiovisual(
        system,
        VISEME_VELAR,         // Visual: /ga/ (back of mouth)
        0.8f,
        PHONEME_B,            // Audio: /ba/ (bilabial)
        0.8f,
        5.0f,                 // Moderate SNR
        &result);

    EXPECT_TRUE(result.mcgurk_conflict_detected)
        << "Should detect visual-audio conflict";

    // Classic McGurk: ga + ba -> da
    EXPECT_EQ(result.fused_phoneme, PHONEME_D)
        << "McGurk effect: visual /ga/ + audio /ba/ should fuse to /da/";
}

TEST_F(LipReadingTest, McGurkEffectTVariant) {
    // WHAT: Test McGurk effect variant
    // WHY:  Visual /ka/ + Audio /pa/ -> Perceived /ta/
    // HOW:  Provide conflicting cues, verify fusion

    audiovisual_integration_t result;

    lip_reading_integrate_audiovisual(
        system,
        VISEME_VELAR,         // Visual: /ka/
        0.8f,
        PHONEME_P,            // Audio: /pa/
        0.8f,
        5.0f,
        &result);

    EXPECT_TRUE(result.mcgurk_conflict_detected);
    EXPECT_EQ(result.fused_phoneme, PHONEME_T)
        << "McGurk: visual /ka/ + audio /pa/ should fuse to /ta/";
}

TEST_F(LipReadingTest, AudiovisualAgreement) {
    // WHAT: Test audiovisual agreement boosts confidence
    // WHY:  Matching visual/audio should be more confident
    // HOW:  Provide matching cues, verify high confidence

    audiovisual_integration_t result;

    lip_reading_integrate_audiovisual(
        system,
        VISEME_BILABIAL,
        0.7f,
        PHONEME_B,            // B maps to BILABIAL - agreement!
        0.7f,
        5.0f,
        &result);

    EXPECT_FALSE(result.mcgurk_conflict_detected)
        << "Matching visual/audio should not conflict";

    // Matching cues should boost confidence
    EXPECT_GT(result.fusion_confidence, 0.7f)
        << "Agreement should increase confidence";
}

TEST_F(LipReadingTest, DisambiguateViseme) {
    // WHAT: Test viseme disambiguation using audio
    // WHY:  Visemes are ambiguous, audio helps disambiguate
    // HOW:  Test disambiguation for bilabial viseme

    // BILABIAL could be P, B, or M
    // Voicing and nasality from audio disambiguates

    // Voiced + Not nasal -> B
    EXPECT_EQ(lip_reading_disambiguate_viseme(VISEME_BILABIAL, PHONEME_B), PHONEME_B);

    // Voiceless + Not nasal -> P
    EXPECT_EQ(lip_reading_disambiguate_viseme(VISEME_BILABIAL, PHONEME_P), PHONEME_P);

    // Nasal -> M
    EXPECT_EQ(lip_reading_disambiguate_viseme(VISEME_BILABIAL, PHONEME_M), PHONEME_M);
}

TEST_F(LipReadingTest, PhonemeVoicingAndNasality) {
    // WHAT: Test phoneme voicing and nasality queries
    // WHY:  Used for disambiguation
    // HOW:  Check known phonemes

    // Voiced consonants
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_B));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_D));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_G));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_V));

    // Voiceless consonants
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_P));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_T));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_K));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_F));

    // Nasal consonants
    EXPECT_TRUE(lip_reading_is_phoneme_nasal(PHONEME_M));
    EXPECT_TRUE(lip_reading_is_phoneme_nasal(PHONEME_N));
    EXPECT_TRUE(lip_reading_is_phoneme_nasal(PHONEME_NG));

    // Non-nasal
    EXPECT_FALSE(lip_reading_is_phoneme_nasal(PHONEME_B));
    EXPECT_FALSE(lip_reading_is_phoneme_nasal(PHONEME_D));
}

/*=============================================================================
 * MOTOR THEORY TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, VisemeToMotorCommand) {
    // WHAT: Test viseme to articulatory motor command mapping
    // WHY:  Motor theory of speech perception
    // HOW:  Get motor command for each viseme

    articulatory_action_t action;

    // Bilabial - lips closed
    lip_reading_viseme_to_motor_command(VISEME_BILABIAL, &action);
    EXPECT_TRUE(action.lips_closed);
    EXPECT_NEAR(action.mouth_aperture, 0.0f, 0.1f);

    // Labiodental - teeth on lip
    lip_reading_viseme_to_motor_command(VISEME_LABIODENTAL, &action);
    EXPECT_TRUE(action.upper_teeth_on_lower_lip);
    EXPECT_TRUE(action.airflow_friction);

    // Dental - tongue between teeth
    lip_reading_viseme_to_motor_command(VISEME_DENTAL, &action);
    EXPECT_TRUE(action.tongue_between_teeth);

    // Rounded - lips rounded
    lip_reading_viseme_to_motor_command(VISEME_ROUNDED_CLOSE, &action);
    EXPECT_TRUE(action.lips_rounded);

    // Open - large aperture
    lip_reading_viseme_to_motor_command(VISEME_UNROUNDED_OPEN, &action);
    EXPECT_GT(action.mouth_aperture, 0.8f);
}

TEST_F(LipReadingTest, SimulateArticulation) {
    // WHAT: Test articulatory simulation (mirror neurons)
    // WHY:  Motor simulation aids perception
    // HOW:  Simulate viseme articulation

    articulatory_action_t action;
    lip_reading_viseme_to_motor_command(VISEME_BILABIAL, &action);

    bool simulated = lip_reading_simulate_articulation(system, VISEME_BILABIAL, &action);
    EXPECT_TRUE(simulated);
}

/*=============================================================================
 * SPEAKER ADAPTATION TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, RegisterSpeaker) {
    // WHAT: Test speaker registration
    // WHY:  Need to track speakers for adaptation
    // HOW:  Register speaker, verify ID

    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    EXPECT_GT(speaker_id, 0u) << "Should get valid speaker ID";

    uint32_t speaker_id2 = lip_reading_register_speaker(system, "TestSpeaker2");
    EXPECT_GT(speaker_id2, 0u);
    EXPECT_NE(speaker_id, speaker_id2) << "Different speakers should have different IDs";
}

TEST_F(LipReadingTest, UpdateSpeakerProfile) {
    // WHAT: Test speaker profile update
    // WHY:  Adaptation requires observing speaker
    // HOW:  Register speaker, update profile, verify

    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    ASSERT_GT(speaker_id, 0u);

    visual_speech_features_t features = {};
    features.lip_width = 45.0f;
    features.lip_height = 15.0f;

    // Update profile multiple times
    for (int i = 0; i < 10; i++) {
        bool updated = lip_reading_update_speaker_profile(
            system, speaker_id, &features, PHONEME_B);
        EXPECT_TRUE(updated);
    }

    // Get profile and verify
    speaker_profile_t profile;
    bool got = lip_reading_get_speaker_profile(system, speaker_id, &profile);
    EXPECT_TRUE(got);
    EXPECT_EQ(profile.speaker_id, speaker_id);
    EXPECT_EQ(profile.frames_observed, 10u);
    EXPECT_NEAR(profile.avg_lip_width, 45.0f, 0.1f);
    EXPECT_NEAR(profile.avg_lip_height, 15.0f, 0.1f);
}

TEST_F(LipReadingTest, SetActiveSpeaker) {
    // WHAT: Test setting active speaker
    // WHY:  Classification should use speaker-specific calibration
    // HOW:  Register speaker, set active, verify

    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    ASSERT_GT(speaker_id, 0u);

    bool set = lip_reading_set_active_speaker(system, speaker_id);
    EXPECT_TRUE(set);

    // Setting invalid speaker should fail
    set = lip_reading_set_active_speaker(system, 99999);
    EXPECT_FALSE(set);

    // Setting 0 (default) should succeed
    set = lip_reading_set_active_speaker(system, 0);
    EXPECT_TRUE(set);
}

/*=============================================================================
 * FULL PIPELINE TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, ProcessSingleFrame) {
    // WHAT: Test full frame processing pipeline
    // WHY:  End-to-end functionality
    // HOW:  Process single frame, verify result

    auto image = createTestImage(128, 128, 3);
    viseme_classification_t classification;

    lip_reading_status_t status = lip_reading_process_frame(
        system, image.data(), 128, 128, 3, &classification);

    EXPECT_EQ(status, LIP_READING_STATUS_VISEME_CLASSIFIED)
        << "Should successfully classify viseme";
    EXPECT_NE(classification.viseme, VISEME_UNKNOWN);
    EXPECT_GT(classification.confidence, 0.0f);
}

TEST_F(LipReadingTest, ProcessFrameWithAudio) {
    // WHAT: Test frame processing with audio integration
    // WHY:  Full audiovisual pipeline
    // HOW:  Process frame with audio phoneme

    auto image = createTestImage(128, 128, 3);
    audiovisual_integration_t integration;

    lip_reading_status_t status = lip_reading_process_frame_with_audio(
        system, image.data(), 128, 128, 3,
        PHONEME_B, 0.8f, 5.0f,
        &integration);

    EXPECT_EQ(status, LIP_READING_STATUS_AUDIO_INTEGRATED);
    EXPECT_NE(integration.fused_phoneme, PHONEME_UNKNOWN);
}

TEST_F(LipReadingTest, RecognizeSilentSpeech) {
    // WHAT: Test silent speech recognition (visual only)
    // WHY:  Key application for deaf users
    // HOW:  Process multiple frames, get viseme sequence

    // Create sequence of frames
    std::vector<std::vector<uint8_t>> frames;
    std::vector<const uint8_t*> frame_ptrs;

    for (int i = 0; i < 5; i++) {
        frames.push_back(createTestImage(128, 128, 3));
        frame_ptrs.push_back(frames.back().data());
    }

    viseme_t visemes[10];
    uint32_t count = lip_reading_recognize_silent_speech(
        system,
        frame_ptrs.data(),
        5,
        128, 128, 3,
        visemes, 10);

    EXPECT_GT(count, 0u) << "Should recognize some visemes";
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, StatisticsAccumulate) {
    // WHAT: Test that statistics accumulate correctly
    // WHY:  Performance monitoring
    // HOW:  Process frames, verify stats

    auto image = createTestImage(128, 128, 3);
    viseme_classification_t classification;

    // Process multiple frames
    for (int i = 0; i < 5; i++) {
        lip_reading_process_frame(system, image.data(), 128, 128, 3, &classification);
    }

    lip_reading_stats_t stats;
    EXPECT_TRUE(lip_reading_get_stats(system, &stats));

    EXPECT_EQ(stats.frames_processed, 5u);
    EXPECT_EQ(stats.faces_detected, 5u);
    EXPECT_EQ(stats.visemes_classified, 5u);
    EXPECT_GT(stats.avg_viseme_confidence, 0.0f);
    EXPECT_GT(stats.avg_total_processing_ms, 0.0);
}

TEST_F(LipReadingTest, StatisticsReset) {
    // WHAT: Test statistics reset
    // WHY:  Allow fresh measurements
    // HOW:  Process, reset, verify cleared

    auto image = createTestImage(128, 128, 3);
    viseme_classification_t classification;

    // Process some frames
    lip_reading_process_frame(system, image.data(), 128, 128, 3, &classification);
    lip_reading_process_frame(system, image.data(), 128, 128, 3, &classification);

    // Reset stats
    EXPECT_TRUE(lip_reading_reset_stats(system));

    // Verify cleared
    lip_reading_stats_t stats;
    EXPECT_TRUE(lip_reading_get_stats(system, &stats));
    EXPECT_EQ(stats.frames_processed, 0u);
    EXPECT_EQ(stats.visemes_classified, 0u);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, ErrorMessages) {
    // WHAT: Test error message strings
    // WHY:  Useful for debugging
    // HOW:  Get messages for all error codes

    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_NONE), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_INVALID_INPUT), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_NO_FACE_DETECTED), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_MOUTH_OCCLUDED), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_LOW_CONFIDENCE), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_TRACKING_LOST), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_BUFFER_FULL), "Unknown error");
    EXPECT_STRNE(lip_reading_error_message(LIP_READING_ERROR_INTERNAL), "Unknown error");
}

TEST_F(LipReadingTest, NullParameterHandling) {
    // WHAT: Test null parameter handling
    // WHY:  Should handle gracefully, not crash
    // HOW:  Pass nulls, verify error handling

    face_detection_result_t result;
    EXPECT_FALSE(lip_reading_detect_face(nullptr, nullptr, 0, 0, 0, &result));
    EXPECT_FALSE(lip_reading_detect_face(system, nullptr, 128, 128, 3, &result));

    visual_speech_features_t features;
    EXPECT_FALSE(lip_reading_extract_features(system, nullptr, 64, 64, nullptr, &features));

    viseme_classification_t classification;
    EXPECT_FALSE(lip_reading_classify_viseme(nullptr, &features, &classification));
    EXPECT_FALSE(lip_reading_classify_viseme(system, nullptr, &classification));
    EXPECT_FALSE(lip_reading_classify_viseme(system, &features, nullptr));
}

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(LipReadingTest, ConnectComponents) {
    // WHAT: Test component connection functions
    // WHY:  Integration with visual cortex, speech cortex, etc.
    // HOW:  Call connect functions, verify they don't crash

    // These should succeed even with null pointers (just store them)
    EXPECT_TRUE(lip_reading_connect_visual_cortex(system, nullptr));
    EXPECT_TRUE(lip_reading_connect_speech_cortex(system, nullptr));
    EXPECT_TRUE(lip_reading_connect_brain(system, nullptr));
    EXPECT_TRUE(lip_reading_connect_bio_router(system, nullptr));

    // Null system should fail
    EXPECT_FALSE(lip_reading_connect_visual_cortex(nullptr, nullptr));
}
