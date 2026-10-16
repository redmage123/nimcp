/**
 * @file test_lip_reading_signal_regression.cpp
 * @brief Signal processing regression tests for Lip Reading System
 *
 * WHAT: Tests Lip Reading signal flow consistency and numerical stability
 * WHY:  Ensure lip reading signals propagate correctly after code changes
 * HOW:  Test known input/output pairs, timing, and signal characteristics
 *
 * REGRESSION FOCUS:
 * - Viseme classification determinism
 * - Audiovisual fusion consistency
 * - Phoneme mapping accuracy
 * - Speaker adaptation behavior
 * - Temporal smoothing stability
 *
 * @version Phase B5: Audiovisual Speech Integration
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "perception/nimcp_lip_reading.h"
#include "utils/memory/nimcp_memory.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class LipReadingSignalRegressionTest : public ::testing::Test {
protected:
    lip_reading_system_t* system;
    lip_reading_config_t config;

    void SetUp() override {
        config = lip_reading_default_config();
        config.enable_temporal_smoothing = true;
        config.enable_speaker_adaptation = true;
        system = lip_reading_create(&config);
        ASSERT_NE(nullptr, system);
    }

    void TearDown() override {
        if (system) {
            lip_reading_destroy(system);
            system = nullptr;
        }
    }

    /* Create standardized face image with specified mouth aperture */
    std::vector<uint8_t> CreateFaceImage(
        uint32_t width, uint32_t height, uint32_t channels,
        float mouth_aperture = 0.3f)
    {
        std::vector<uint8_t> image(width * height * channels, 50);

        uint32_t face_x = width / 4;
        uint32_t face_y = height / 4;
        uint32_t face_w = width / 2;
        uint32_t face_h = height / 2;

        for (uint32_t y = face_y; y < face_y + face_h && y < height; y++) {
            for (uint32_t x = face_x; x < face_x + face_w && x < width; x++) {
                uint32_t idx = (y * width + x) * channels;
                if (channels >= 3) {
                    image[idx] = 180;
                    image[idx + 1] = 140;
                    image[idx + 2] = 100;
                }
            }
        }

        uint32_t mouth_x = face_x + face_w / 4;
        uint32_t mouth_y = face_y + face_h * 2 / 3;
        uint32_t mouth_w = face_w / 2;
        uint32_t mouth_h = (uint32_t)(face_h / 4 * mouth_aperture);

        for (uint32_t y = mouth_y; y < mouth_y + mouth_h && y < height; y++) {
            for (uint32_t x = mouth_x; x < mouth_x + mouth_w && x < width; x++) {
                uint32_t idx = (y * width + x) * channels;
                if (channels >= 3) {
                    uint8_t brightness = (uint8_t)(150 + 80 * mouth_aperture);
                    image[idx] = brightness;
                    image[idx + 1] = brightness;
                    image[idx + 2] = brightness;
                }
            }
        }

        return image;
    }
};

/*=============================================================================
 * VISEME CLASSIFICATION DETERMINISM TESTS
 * Verify consistent classification outputs for same inputs
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, ClassificationDeterminism_SameInputSameOutput) {
    /* Same input should produce same classification */
    auto image = CreateFaceImage(128, 128, 3, 0.5f);

    viseme_classification_t result1;
    viseme_classification_t result2;

    lip_reading_process_frame(system, image.data(), 128, 128, 3, &result1);

    lip_reading_reset(system);

    lip_reading_process_frame(system, image.data(), 128, 128, 3, &result2);

    /* Primary viseme should be identical for same input */
    EXPECT_EQ(result1.viseme, result2.viseme);
    /* Confidence should be very close */
    EXPECT_NEAR(result1.confidence, result2.confidence, 0.01f);
}

TEST_F(LipReadingSignalRegressionTest, ClassificationDeterminism_MultipleFrames) {
    /* Same sequence should produce same results */
    auto frame1 = CreateFaceImage(128, 128, 3, 0.3f);
    auto frame2 = CreateFaceImage(128, 128, 3, 0.6f);
    auto frame3 = CreateFaceImage(128, 128, 3, 0.4f);

    std::vector<viseme_t> visemes1, visemes2;

    /* First pass */
    viseme_classification_t result;
    lip_reading_process_frame(system, frame1.data(), 128, 128, 3, &result);
    visemes1.push_back(result.viseme);
    lip_reading_process_frame(system, frame2.data(), 128, 128, 3, &result);
    visemes1.push_back(result.viseme);
    lip_reading_process_frame(system, frame3.data(), 128, 128, 3, &result);
    visemes1.push_back(result.viseme);

    lip_reading_reset(system);

    /* Second pass */
    lip_reading_process_frame(system, frame1.data(), 128, 128, 3, &result);
    visemes2.push_back(result.viseme);
    lip_reading_process_frame(system, frame2.data(), 128, 128, 3, &result);
    visemes2.push_back(result.viseme);
    lip_reading_process_frame(system, frame3.data(), 128, 128, 3, &result);
    visemes2.push_back(result.viseme);

    EXPECT_EQ(visemes1, visemes2) << "Same sequence should produce same visemes";
}

/*=============================================================================
 * PHONEME MAPPING CONSISTENCY TESTS
 * Verify phoneme-to-viseme mapping remains stable
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, PhonemeMapping_BilabialGroup) {
    /* Bilabial phonemes should map to VISEME_BILABIAL */
    EXPECT_EQ(VISEME_BILABIAL, lip_reading_phoneme_to_viseme(PHONEME_P));
    EXPECT_EQ(VISEME_BILABIAL, lip_reading_phoneme_to_viseme(PHONEME_B));
    EXPECT_EQ(VISEME_BILABIAL, lip_reading_phoneme_to_viseme(PHONEME_M));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeMapping_LabioddentalGroup) {
    /* Labiodental phonemes should map to VISEME_LABIODENTAL */
    EXPECT_EQ(VISEME_LABIODENTAL, lip_reading_phoneme_to_viseme(PHONEME_F));
    EXPECT_EQ(VISEME_LABIODENTAL, lip_reading_phoneme_to_viseme(PHONEME_V));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeMapping_DentalGroup) {
    /* Dental phonemes should map to VISEME_DENTAL */
    EXPECT_EQ(VISEME_DENTAL, lip_reading_phoneme_to_viseme(PHONEME_TH));
    EXPECT_EQ(VISEME_DENTAL, lip_reading_phoneme_to_viseme(PHONEME_DH));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeMapping_AlveolarGroup) {
    /* Alveolar phonemes should map to VISEME_ALVEOLAR */
    EXPECT_EQ(VISEME_ALVEOLAR, lip_reading_phoneme_to_viseme(PHONEME_T));
    EXPECT_EQ(VISEME_ALVEOLAR, lip_reading_phoneme_to_viseme(PHONEME_D));
    EXPECT_EQ(VISEME_ALVEOLAR, lip_reading_phoneme_to_viseme(PHONEME_N));
    EXPECT_EQ(VISEME_ALVEOLAR, lip_reading_phoneme_to_viseme(PHONEME_L));
    EXPECT_EQ(VISEME_ALVEOLAR, lip_reading_phoneme_to_viseme(PHONEME_S));
    EXPECT_EQ(VISEME_ALVEOLAR, lip_reading_phoneme_to_viseme(PHONEME_Z));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeMapping_VelarGroup) {
    /* Velar phonemes should map to VISEME_VELAR */
    EXPECT_EQ(VISEME_VELAR, lip_reading_phoneme_to_viseme(PHONEME_K));
    EXPECT_EQ(VISEME_VELAR, lip_reading_phoneme_to_viseme(PHONEME_G));
    EXPECT_EQ(VISEME_VELAR, lip_reading_phoneme_to_viseme(PHONEME_NG));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeMapping_Reversible) {
    /* Viseme to phonemes should include the original phoneme */
    phoneme_t phonemes[10];

    /* Test bilabial */
    uint32_t count = lip_reading_viseme_to_phonemes(VISEME_BILABIAL, phonemes, 10);
    EXPECT_GE(count, 1u);

    bool found_b = false;
    for (uint32_t i = 0; i < count; i++) {
        if (phonemes[i] == PHONEME_B) found_b = true;
    }
    EXPECT_TRUE(found_b) << "VISEME_BILABIAL should include PHONEME_B";

    /* Test labiodental */
    count = lip_reading_viseme_to_phonemes(VISEME_LABIODENTAL, phonemes, 10);
    bool found_f = false;
    for (uint32_t i = 0; i < count; i++) {
        if (phonemes[i] == PHONEME_F) found_f = true;
    }
    EXPECT_TRUE(found_f) << "VISEME_LABIODENTAL should include PHONEME_F";
}

/*=============================================================================
 * AUDIOVISUAL FUSION CONSISTENCY TESTS
 * Verify audiovisual integration produces consistent results
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, AudiovisualFusion_CongruentInputs) {
    /* When visual and audio agree, fusion should maintain agreement */
    audiovisual_integration_t result;

    bool success = lip_reading_integrate_audiovisual(
        system,
        VISEME_BILABIAL, 0.9f,   /* Visual: bilabial */
        PHONEME_B, 0.9f,         /* Audio: bilabial /b/ */
        20.0f,                    /* High SNR */
        &result
    );

    EXPECT_TRUE(success);
    EXPECT_EQ(VISEME_BILABIAL, result.visual_viseme);
    EXPECT_EQ(PHONEME_B, result.auditory_phoneme);

    /* Fused result should maintain agreement */
    viseme_t fused_viseme = lip_reading_phoneme_to_viseme(result.fused_phoneme);
    EXPECT_EQ(VISEME_BILABIAL, fused_viseme);

    /* No McGurk conflict expected */
    EXPECT_FALSE(result.mcgurk_conflict_detected);
}

TEST_F(LipReadingSignalRegressionTest, AudiovisualFusion_McGurk_Classic) {
    /* Classic McGurk: Visual /ga/ + Audio /ba/ -> Perceived /da/ */
    audiovisual_integration_t result;

    bool success = lip_reading_integrate_audiovisual(
        system,
        VISEME_VELAR, 0.8f,      /* Visual: velar (like /ga/) */
        PHONEME_B, 0.8f,         /* Audio: bilabial /b/ */
        10.0f,                    /* Moderate SNR */
        &result
    );

    EXPECT_TRUE(success);

    /* Should detect McGurk conflict */
    EXPECT_TRUE(result.mcgurk_conflict_detected);

    /* Fused phoneme should be alveolar /d/ (classic McGurk result) */
    EXPECT_EQ(PHONEME_D, result.fused_phoneme);
}

TEST_F(LipReadingSignalRegressionTest, AudiovisualFusion_HighSNR_AudioDominates) {
    /* With high SNR, audio should dominate */
    audiovisual_integration_t result;

    lip_reading_integrate_audiovisual(
        system,
        VISEME_VELAR, 0.6f,      /* Visual: velar */
        PHONEME_B, 0.9f,         /* Audio: bilabial */
        30.0f,                    /* Very high SNR */
        &result
    );

    /* With high SNR, audio weight should be higher */
    EXPECT_GT(result.auditory_weight, result.visual_weight);
}

TEST_F(LipReadingSignalRegressionTest, AudiovisualFusion_LowSNR_VisualDominates) {
    /* With low SNR, visual should gain weight */
    audiovisual_integration_t result;

    lip_reading_integrate_audiovisual(
        system,
        VISEME_BILABIAL, 0.8f,   /* Visual: bilabial */
        PHONEME_B, 0.5f,         /* Audio: bilabial (low confidence) */
        -15.0f,                   /* Very low SNR */
        &result
    );

    /* With low SNR, visual weight should increase */
    EXPECT_GT(result.visual_weight, 0.3f);
}

/*=============================================================================
 * SPEAKER ADAPTATION CONSISTENCY TESTS
 * Verify speaker adaptation produces consistent improvements
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, SpeakerAdaptation_ProfileUpdates) {
    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    ASSERT_GT(speaker_id, 0u);

    /* Update profile with consistent features */
    visual_speech_features_t features;
    memset(&features, 0, sizeof(features));
    features.lip_width = 42.0f;
    features.lip_height = 14.0f;
    features.feature_confidence = 0.9f;

    for (int i = 0; i < 50; i++) {
        lip_reading_update_speaker_profile(system, speaker_id, &features, PHONEME_B);
    }

    speaker_profile_t profile;
    ASSERT_TRUE(lip_reading_get_speaker_profile(system, speaker_id, &profile));

    EXPECT_EQ(50u, profile.frames_observed);
    EXPECT_NEAR(42.0f, profile.avg_lip_width, 0.1f);
    EXPECT_NEAR(14.0f, profile.avg_lip_height, 0.1f);
}

TEST_F(LipReadingSignalRegressionTest, SpeakerAdaptation_UniqueProfiles) {
    /* Different speakers should have independent profiles */
    uint32_t speaker1 = lip_reading_register_speaker(system, "Speaker1");
    uint32_t speaker2 = lip_reading_register_speaker(system, "Speaker2");

    /* Update speaker1 with wide lips */
    visual_speech_features_t features1 = {};
    features1.lip_width = 50.0f;
    features1.lip_height = 20.0f;
    lip_reading_update_speaker_profile(system, speaker1, &features1, PHONEME_B);

    /* Update speaker2 with narrow lips */
    visual_speech_features_t features2 = {};
    features2.lip_width = 35.0f;
    features2.lip_height = 10.0f;
    lip_reading_update_speaker_profile(system, speaker2, &features2, PHONEME_B);

    speaker_profile_t profile1, profile2;
    lip_reading_get_speaker_profile(system, speaker1, &profile1);
    lip_reading_get_speaker_profile(system, speaker2, &profile2);

    /* Profiles should be different */
    EXPECT_NEAR(50.0f, profile1.avg_lip_width, 0.1f);
    EXPECT_NEAR(35.0f, profile2.avg_lip_width, 0.1f);
}

/*=============================================================================
 * PHONEME PROPERTY CONSISTENCY TESTS
 * Verify phoneme properties remain consistent
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, PhonemeProperties_VoicedPhonemes) {
    /* Verify voiced phonemes */
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_B));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_D));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_G));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_V));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_Z));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_M));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_N));
    EXPECT_TRUE(lip_reading_is_phoneme_voiced(PHONEME_L));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeProperties_VoicelessPhonemes) {
    /* Verify voiceless phonemes */
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_P));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_T));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_K));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_F));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_S));
    EXPECT_FALSE(lip_reading_is_phoneme_voiced(PHONEME_TH));
}

TEST_F(LipReadingSignalRegressionTest, PhonemeProperties_NasalPhonemes) {
    /* Verify nasal phonemes */
    EXPECT_TRUE(lip_reading_is_phoneme_nasal(PHONEME_M));
    EXPECT_TRUE(lip_reading_is_phoneme_nasal(PHONEME_N));
    EXPECT_TRUE(lip_reading_is_phoneme_nasal(PHONEME_NG));

    /* Non-nasal phonemes */
    EXPECT_FALSE(lip_reading_is_phoneme_nasal(PHONEME_B));
    EXPECT_FALSE(lip_reading_is_phoneme_nasal(PHONEME_D));
    EXPECT_FALSE(lip_reading_is_phoneme_nasal(PHONEME_G));
}

/*=============================================================================
 * MOTOR COMMAND CONSISTENCY TESTS
 * Verify motor commands for articulation remain consistent
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, MotorCommand_BilabialArticulation) {
    articulatory_action_t action;
    EXPECT_TRUE(lip_reading_viseme_to_motor_command(VISEME_BILABIAL, &action));

    /* Bilabial should have lips closed */
    EXPECT_TRUE(action.lips_closed);
}

TEST_F(LipReadingSignalRegressionTest, MotorCommand_DentalArticulation) {
    articulatory_action_t action;
    EXPECT_TRUE(lip_reading_viseme_to_motor_command(VISEME_DENTAL, &action));

    /* Dental should have tongue between teeth */
    EXPECT_TRUE(action.tongue_between_teeth);
}

TEST_F(LipReadingSignalRegressionTest, MotorCommand_RoundedArticulation) {
    articulatory_action_t action;
    EXPECT_TRUE(lip_reading_viseme_to_motor_command(VISEME_ROUNDED_CLOSE, &action));

    /* Rounded should have lips rounded */
    EXPECT_TRUE(action.lips_rounded);
}

/*=============================================================================
 * VISEME NAME CONSISTENCY TESTS
 * Verify viseme names remain stable
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, VisemeName_AllNames) {
    /* All viseme names should be non-null */
    for (int i = 0; i < VISEME_COUNT; i++) {
        const char* name = lip_reading_viseme_name(static_cast<viseme_t>(i));
        EXPECT_NE(nullptr, name) << "Viseme " << i << " has no name";
        EXPECT_GT(strlen(name), 0u) << "Viseme " << i << " has empty name";
    }
}

TEST_F(LipReadingSignalRegressionTest, VisemeName_SpecificNames) {
    /* Verify specific viseme names haven't changed */
    EXPECT_STREQ("BILABIAL", lip_reading_viseme_name(VISEME_BILABIAL));
    EXPECT_STREQ("LABIODENTAL", lip_reading_viseme_name(VISEME_LABIODENTAL));
    EXPECT_STREQ("DENTAL", lip_reading_viseme_name(VISEME_DENTAL));
    EXPECT_STREQ("ALVEOLAR", lip_reading_viseme_name(VISEME_ALVEOLAR));
    EXPECT_STREQ("VELAR", lip_reading_viseme_name(VISEME_VELAR));
    EXPECT_STREQ("SILENCE", lip_reading_viseme_name(VISEME_SILENCE));
    EXPECT_STREQ("UNKNOWN", lip_reading_viseme_name(VISEME_UNKNOWN));
}

/*=============================================================================
 * STATISTICS CONSISTENCY TESTS
 * Verify statistics remain accurate
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, Statistics_FramesCounted) {
    auto image = CreateFaceImage(128, 128, 3);

    for (int i = 0; i < 10; i++) {
        viseme_classification_t result;
        lip_reading_process_frame(system, image.data(), 128, 128, 3, &result);
    }

    lip_reading_stats_t stats;
    lip_reading_get_stats(system, &stats);

    EXPECT_EQ(10u, stats.frames_processed);
    EXPECT_EQ(10u, stats.faces_detected);
    EXPECT_EQ(10u, stats.visemes_classified);
}

TEST_F(LipReadingSignalRegressionTest, Statistics_ResetClears) {
    auto image = CreateFaceImage(128, 128, 3);

    for (int i = 0; i < 10; i++) {
        viseme_classification_t result;
        lip_reading_process_frame(system, image.data(), 128, 128, 3, &result);
    }

    lip_reading_reset(system);

    lip_reading_stats_t stats;
    lip_reading_get_stats(system, &stats);

    EXPECT_EQ(0u, stats.frames_processed);
    EXPECT_EQ(0u, stats.faces_detected);
}

/*=============================================================================
 * TEMPORAL SMOOTHING CONSISTENCY TESTS
 * Verify temporal smoothing behavior remains consistent
 *===========================================================================*/

TEST_F(LipReadingSignalRegressionTest, TemporalSmoothing_HistoryMaintained) {
    /* Process multiple frames and verify history is maintained */
    for (int i = 0; i < 15; i++) {
        auto image = CreateFaceImage(128, 128, 3, 0.3f + 0.02f * i);
        viseme_classification_t result;
        lip_reading_process_frame(system, image.data(), 128, 128, 3, &result);

        if (i >= LIP_READING_MAX_VISEME_HISTORY) {
            EXPECT_EQ(LIP_READING_MAX_VISEME_HISTORY, result.history_count)
                << "History should be capped at max";
        }
    }
}

TEST_F(LipReadingSignalRegressionTest, TemporalSmoothing_HistoryClearedOnReset) {
    /* Process frames, reset, verify history cleared */
    auto image = CreateFaceImage(128, 128, 3, 0.5f);
    viseme_classification_t result;

    for (int i = 0; i < 5; i++) {
        lip_reading_process_frame(system, image.data(), 128, 128, 3, &result);
    }

    EXPECT_GT(result.history_count, 0u);

    lip_reading_reset(system);

    lip_reading_process_frame(system, image.data(), 128, 128, 3, &result);

    /* First frame after reset should have history count of 1 */
    EXPECT_LE(result.history_count, 1u);
}
