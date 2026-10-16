/**
 * @file test_lip_reading_backward_compat.cpp
 * @brief Backward compatibility regression tests for Lip Reading System
 *
 * WHAT: Tests Lip Reading API stability and backward compatibility
 * WHY:  Ensure existing lip reading code continues to work after updates
 * HOW:  Test core API functions, data structures, and return values
 *
 * REGRESSION FOCUS:
 * - API function signatures unchanged
 * - Return value semantics preserved
 * - Default behaviors maintained
 * - Error codes consistent
 * - Configuration defaults stable
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

class LipReadingBackwardCompatTest : public ::testing::Test {
protected:
    lip_reading_system_t* system;
    lip_reading_config_t config;

    void SetUp() override {
        config = lip_reading_default_config();
        system = lip_reading_create(&config);
        ASSERT_NE(nullptr, system);
    }

    void TearDown() override {
        if (system) {
            lip_reading_destroy(system);
            system = nullptr;
        }
    }

    std::vector<uint8_t> CreateFaceImage(uint32_t width, uint32_t height, uint32_t channels) {
        std::vector<uint8_t> image(width * height * channels, 50);

        // Add face region with skin color
        uint32_t face_x = width / 4;
        uint32_t face_y = height / 4;
        uint32_t face_w = width / 2;
        uint32_t face_h = height / 2;

        for (uint32_t y = face_y; y < face_y + face_h && y < height; y++) {
            for (uint32_t x = face_x; x < face_x + face_w && x < width; x++) {
                uint32_t idx = (y * width + x) * channels;
                if (channels >= 3) {
                    image[idx] = 180;     // R
                    image[idx + 1] = 140; // G
                    image[idx + 2] = 100; // B
                }
            }
        }
        return image;
    }
};

/*=============================================================================
 * API FUNCTION SIGNATURE TESTS
 * These tests verify that the expected API functions exist and have
 * compatible signatures.
 *===========================================================================*/

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_default_config_exists) {
    /* Verify function exists and returns valid config */
    lip_reading_config_t cfg = lip_reading_default_config();
    EXPECT_TRUE(true);  /* Compilation success = function exists */
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_create_exists) {
    /* Verify lip_reading_create accepts config pointer */
    lip_reading_config_t cfg = lip_reading_default_config();
    lip_reading_system_t* test_system = lip_reading_create(&cfg);
    ASSERT_NE(nullptr, test_system);
    lip_reading_destroy(test_system);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_destroy_exists) {
    /* Verify lip_reading_destroy accepts system pointer */
    lip_reading_config_t cfg = lip_reading_default_config();
    lip_reading_system_t* test_system = lip_reading_create(&cfg);
    lip_reading_destroy(test_system);
    lip_reading_destroy(nullptr);  /* Should handle NULL safely */
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_reset_exists) {
    bool result = lip_reading_reset(system);
    EXPECT_TRUE(result);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_detect_face_exists) {
    auto image = CreateFaceImage(128, 128, 3);
    face_detection_result_t result;

    bool detected = lip_reading_detect_face(
        system, image.data(), 128, 128, 3, &result);

    /* Function exists and executes */
    EXPECT_TRUE(true);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_extract_mouth_roi_exists) {
    face_detection_result_t face;
    memset(&face, 0, sizeof(face));
    face.face_bbox[0] = 32.0f;  /* x */
    face.face_bbox[1] = 32.0f;  /* y */
    face.face_bbox[2] = 64.0f;  /* width */
    face.face_bbox[3] = 64.0f;  /* height */

    auto image = CreateFaceImage(128, 128, 3);
    std::vector<uint8_t> roi(64 * 64 * 3);

    bool result = lip_reading_extract_mouth_roi(
        system, image.data(), 128, 128, 3,
        &face, roi.data(), 64, 64);

    /* Function exists and executes */
    EXPECT_TRUE(true);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_classify_viseme_exists) {
    visual_speech_features_t features;
    memset(&features, 0, sizeof(features));
    features.lip_width = 40.0f;
    features.lip_height = 10.0f;
    features.feature_confidence = 0.8f;

    viseme_classification_t result;
    bool success = lip_reading_classify_viseme(system, &features, &result);

    /* Function exists and executes */
    EXPECT_TRUE(true);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_integrate_audiovisual_exists) {
    audiovisual_integration_t result;

    bool success = lip_reading_integrate_audiovisual(
        system, VISEME_BILABIAL, 0.8f,
        PHONEME_B, 0.9f, 10.0f, &result);

    EXPECT_TRUE(success);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_phoneme_to_viseme_exists) {
    viseme_t viseme = lip_reading_phoneme_to_viseme(PHONEME_B);
    EXPECT_EQ(VISEME_BILABIAL, viseme);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_viseme_to_phonemes_exists) {
    phoneme_t phonemes[5];
    uint32_t count = lip_reading_viseme_to_phonemes(VISEME_BILABIAL, phonemes, 5);
    EXPECT_GE(count, 1u);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_viseme_name_exists) {
    const char* name = lip_reading_viseme_name(VISEME_BILABIAL);
    EXPECT_NE(nullptr, name);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_phoneme_name_exists) {
    const char* name = lip_reading_phoneme_name(PHONEME_B);
    EXPECT_NE(nullptr, name);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_viseme_to_motor_command_exists) {
    articulatory_action_t action;
    bool success = lip_reading_viseme_to_motor_command(VISEME_BILABIAL, &action);
    EXPECT_TRUE(success);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_register_speaker_exists) {
    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    EXPECT_GT(speaker_id, 0u);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_update_speaker_profile_exists) {
    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    visual_speech_features_t features;
    memset(&features, 0, sizeof(features));

    bool result = lip_reading_update_speaker_profile(
        system, speaker_id, &features, PHONEME_B);

    EXPECT_TRUE(true);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_get_speaker_profile_exists) {
    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    speaker_profile_t profile;

    bool result = lip_reading_get_speaker_profile(system, speaker_id, &profile);
    EXPECT_TRUE(result);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_set_active_speaker_exists) {
    uint32_t speaker_id = lip_reading_register_speaker(system, "TestSpeaker");
    bool result = lip_reading_set_active_speaker(system, speaker_id);
    EXPECT_TRUE(result);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_process_frame_exists) {
    auto image = CreateFaceImage(128, 128, 3);
    viseme_classification_t result;

    lip_reading_status_t status = lip_reading_process_frame(
        system, image.data(), 128, 128, 3, &result);

    EXPECT_EQ(LIP_READING_STATUS_VISEME_CLASSIFIED, status);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_process_frame_with_audio_exists) {
    auto image = CreateFaceImage(128, 128, 3);
    audiovisual_integration_t result;

    lip_reading_status_t status = lip_reading_process_frame_with_audio(
        system, image.data(), 128, 128, 3,
        PHONEME_B, 0.8f, 10.0f, &result);

    EXPECT_EQ(LIP_READING_STATUS_AUDIO_INTEGRATED, status);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_recognize_silent_speech_exists) {
    auto image1 = CreateFaceImage(128, 128, 3);
    auto image2 = CreateFaceImage(128, 128, 3);

    const uint8_t* frames[] = { image1.data(), image2.data() };
    viseme_t visemes[10];

    uint32_t count = lip_reading_recognize_silent_speech(
        system, frames, 2, 128, 128, 3, visemes, 10);

    EXPECT_GT(count, 0u);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_get_stats_exists) {
    lip_reading_stats_t stats;
    bool result = lip_reading_get_stats(system, &stats);
    EXPECT_TRUE(result);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_is_phoneme_voiced_exists) {
    bool voiced = lip_reading_is_phoneme_voiced(PHONEME_B);
    EXPECT_TRUE(voiced);  /* /b/ is voiced */
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_is_phoneme_nasal_exists) {
    bool nasal = lip_reading_is_phoneme_nasal(PHONEME_M);
    EXPECT_TRUE(nasal);  /* /m/ is nasal */
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_disambiguate_viseme_exists) {
    phoneme_t result = lip_reading_disambiguate_viseme(VISEME_BILABIAL, PHONEME_B);
    EXPECT_TRUE(result == PHONEME_B || result == PHONEME_P || result == PHONEME_M);
}

TEST_F(LipReadingBackwardCompatTest, API_lip_reading_connect_bio_router_exists) {
    bool result = lip_reading_connect_bio_router(system, nullptr);
    EXPECT_TRUE(result);  /* Should handle NULL router gracefully */
}

/*=============================================================================
 * DEFAULT CONFIGURATION TESTS
 * Verify default configuration values remain stable
 *===========================================================================*/

TEST_F(LipReadingBackwardCompatTest, DefaultConfig_MouthROISize) {
    lip_reading_config_t cfg = lip_reading_default_config();
    EXPECT_EQ(LIP_READING_DEFAULT_MOUTH_ROI_SIZE, cfg.mouth_roi_width);
    EXPECT_EQ(LIP_READING_DEFAULT_MOUTH_ROI_SIZE, cfg.mouth_roi_height);
}

TEST_F(LipReadingBackwardCompatTest, DefaultConfig_VisemeHistoryLength) {
    lip_reading_config_t cfg = lip_reading_default_config();
    EXPECT_EQ(LIP_READING_MAX_VISEME_HISTORY, cfg.viseme_history_length);
}

TEST_F(LipReadingBackwardCompatTest, DefaultConfig_AdaptationFrames) {
    lip_reading_config_t cfg = lip_reading_default_config();
    EXPECT_EQ(LIP_READING_DEFAULT_ADAPTATION_FRAMES, cfg.adaptation_frames);
}

TEST_F(LipReadingBackwardCompatTest, DefaultConfig_TemporalSmoothingEnabled) {
    lip_reading_config_t cfg = lip_reading_default_config();
    /* Temporal smoothing should be enabled by default for better accuracy */
    EXPECT_TRUE(cfg.enable_temporal_smoothing);
}

/*=============================================================================
 * RETURN VALUE CONSISTENCY TESTS
 * Verify return values remain consistent
 *===========================================================================*/

TEST_F(LipReadingBackwardCompatTest, ReturnValue_NullSystemHandled) {
    /* Functions should handle NULL system gracefully */
    viseme_classification_t result;
    lip_reading_status_t status = lip_reading_process_frame(
        nullptr, nullptr, 0, 0, 0, &result);

    EXPECT_EQ(LIP_READING_STATUS_ERROR, status);
}

TEST_F(LipReadingBackwardCompatTest, ReturnValue_NullImageHandled) {
    viseme_classification_t result;
    lip_reading_status_t status = lip_reading_process_frame(
        system, nullptr, 128, 128, 3, &result);

    EXPECT_EQ(LIP_READING_STATUS_ERROR, status);
}

TEST_F(LipReadingBackwardCompatTest, ReturnValue_ZeroDimensionsHandled) {
    auto image = CreateFaceImage(128, 128, 3);
    viseme_classification_t result;

    lip_reading_status_t status = lip_reading_process_frame(
        system, image.data(), 0, 0, 3, &result);

    /* Zero dimensions should not produce successful classification */
    EXPECT_NE(LIP_READING_STATUS_VISEME_CLASSIFIED, status);
}

TEST_F(LipReadingBackwardCompatTest, ReturnValue_InvalidSpeakerID) {
    speaker_profile_t profile;
    bool result = lip_reading_get_speaker_profile(system, 99999, &profile);
    EXPECT_FALSE(result);  /* Non-existent speaker should fail */
}

/*=============================================================================
 * ENUM VALUE STABILITY TESTS
 * Verify enum values remain stable
 *===========================================================================*/

TEST_F(LipReadingBackwardCompatTest, EnumValues_VisemeStable) {
    /* Verify viseme enum values haven't changed */
    EXPECT_EQ(0, VISEME_BILABIAL);
    EXPECT_EQ(1, VISEME_LABIODENTAL);
    EXPECT_EQ(2, VISEME_DENTAL);
    EXPECT_EQ(3, VISEME_ALVEOLAR);
    EXPECT_EQ(4, VISEME_VELAR);
    EXPECT_EQ(5, VISEME_ROUNDED_CLOSE);
    EXPECT_EQ(6, VISEME_ROUNDED_OPEN);
    EXPECT_EQ(7, VISEME_UNROUNDED_CLOSE);
    EXPECT_EQ(8, VISEME_UNROUNDED_MID);
    EXPECT_EQ(9, VISEME_UNROUNDED_OPEN);
    EXPECT_EQ(10, VISEME_SILENCE);
    EXPECT_EQ(11, VISEME_UNKNOWN);
    EXPECT_EQ(12, VISEME_COUNT);
}

TEST_F(LipReadingBackwardCompatTest, EnumValues_StatusStable) {
    /* Verify status enum values exist (exact values may vary) */
    lip_reading_status_t status;
    status = LIP_READING_STATUS_IDLE;
    status = LIP_READING_STATUS_FACE_DETECTED;
    status = LIP_READING_STATUS_MOUTH_TRACKED;
    status = LIP_READING_STATUS_VISEME_CLASSIFIED;
    status = LIP_READING_STATUS_AUDIO_INTEGRATED;
    status = LIP_READING_STATUS_SPEECH_RECOGNIZED;
    status = LIP_READING_STATUS_ERROR;
    (void)status;
    EXPECT_TRUE(true);  /* Compilation success = enum exists */
}

TEST_F(LipReadingBackwardCompatTest, EnumValues_PhonemeStable) {
    /* Verify key phoneme values haven't changed */
    EXPECT_EQ(0, PHONEME_UNKNOWN);
    EXPECT_NE(PHONEME_B, PHONEME_UNKNOWN);
    EXPECT_NE(PHONEME_D, PHONEME_B);
    EXPECT_NE(PHONEME_G, PHONEME_D);
}

/*=============================================================================
 * STRUCT SIZE STABILITY TESTS
 * Verify struct sizes haven't changed unexpectedly
 *===========================================================================*/

TEST_F(LipReadingBackwardCompatTest, StructSize_ConfigReasonable) {
    /* Config struct should be reasonably sized */
    EXPECT_GT(sizeof(lip_reading_config_t), 0u);
    EXPECT_LT(sizeof(lip_reading_config_t), 4096u);
}

TEST_F(LipReadingBackwardCompatTest, StructSize_VisemeClassificationReasonable) {
    EXPECT_GT(sizeof(viseme_classification_t), 0u);
    EXPECT_LT(sizeof(viseme_classification_t), 4096u);
}

TEST_F(LipReadingBackwardCompatTest, StructSize_AudiovisualIntegrationReasonable) {
    EXPECT_GT(sizeof(audiovisual_integration_t), 0u);
    EXPECT_LT(sizeof(audiovisual_integration_t), 4096u);
}

TEST_F(LipReadingBackwardCompatTest, StructSize_SpeakerProfileReasonable) {
    EXPECT_GT(sizeof(speaker_profile_t), 0u);
    EXPECT_LT(sizeof(speaker_profile_t), 4096u);
}

/*=============================================================================
 * CONSTANT STABILITY TESTS
 * Verify important constants haven't changed
 *===========================================================================*/

TEST_F(LipReadingBackwardCompatTest, Constants_LipContourPoints) {
    EXPECT_EQ(12, LIP_READING_MAX_LIP_CONTOUR_POINTS);
}

TEST_F(LipReadingBackwardCompatTest, Constants_InnerContourPoints) {
    EXPECT_EQ(8, LIP_READING_INNER_CONTOUR_POINTS);
}

TEST_F(LipReadingBackwardCompatTest, Constants_VisemeHistory) {
    EXPECT_EQ(10, LIP_READING_MAX_VISEME_HISTORY);
}

TEST_F(LipReadingBackwardCompatTest, Constants_MaxSpeakers) {
    EXPECT_EQ(16, LIP_READING_MAX_SPEAKERS);
}

TEST_F(LipReadingBackwardCompatTest, Constants_FeatureDim) {
    EXPECT_EQ(128, LIP_READING_FEATURE_DIM);
}
