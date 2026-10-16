/**
 * @file test_dragonfly_audio_bridge.cpp
 * @brief Unit tests for Dragonfly Audio Bridge module
 *
 * Tests sound localization, multi-modal fusion, and attention cueing.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_audio_bridge.h"
#include "dragonfly/nimcp_dragonfly.h"

class AudioBridgeTest : public ::testing::Test {
protected:
    dragonfly_audio_bridge_t* bridge = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    void SetUp() override {
        dragonfly = dragonfly_system_create(nullptr);
        bridge = dragonfly_audio_bridge_create(dragonfly, nullptr, nullptr);
    }

    void TearDown() override {
        dragonfly_audio_bridge_destroy(bridge);
        dragonfly_system_destroy(dragonfly);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(AudioBridgeTest, DefaultConfig) {
    audio_bridge_config_t config = audio_bridge_default_config();

    EXPECT_GT(config.min_intensity_db, 0);
    EXPECT_GT(config.min_frequency_hz, 0);
    EXPECT_GT(config.max_frequency_hz, config.min_frequency_hz);
    EXPECT_GT(config.ear_separation_m, 0);
    EXPECT_GT(config.speed_of_sound_mps, 300.0f);
    EXPECT_LE(config.speed_of_sound_mps, 400.0f);
}

TEST_F(AudioBridgeTest, ValidateConfig) {
    audio_bridge_config_t config = audio_bridge_default_config();
    EXPECT_TRUE(audio_bridge_validate_config(&config));

    // Invalid intensity
    config.min_intensity_db = -10.0f;
    EXPECT_FALSE(audio_bridge_validate_config(&config));
    config = audio_bridge_default_config();

    // Invalid frequency range
    config.max_frequency_hz = config.min_frequency_hz - 1.0f;
    EXPECT_FALSE(audio_bridge_validate_config(&config));
    config = audio_bridge_default_config();

    // Invalid detection threshold
    config.detection_threshold = 1.5f;
    EXPECT_FALSE(audio_bridge_validate_config(&config));
}

TEST_F(AudioBridgeTest, ValidateNullConfig) {
    EXPECT_FALSE(audio_bridge_validate_config(nullptr));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AudioBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    // Destroy is handled in TearDown
}

TEST_F(AudioBridgeTest, CreateWithNullDragonfly) {
    dragonfly_audio_bridge_t* b = dragonfly_audio_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);  // Should still work, just won't cue
    dragonfly_audio_bridge_destroy(b);
}

TEST_F(AudioBridgeTest, CreateWithCustomConfig) {
    audio_bridge_config_t config = audio_bridge_default_config();
    config.min_intensity_db = 50.0f;
    config.enable_attention_cue = false;

    dragonfly_audio_bridge_t* b = dragonfly_audio_bridge_create(dragonfly, nullptr, &config);
    ASSERT_NE(b, nullptr);

    audio_bridge_config_t retrieved;
    EXPECT_EQ(dragonfly_audio_bridge_get_config(b, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.min_intensity_db, 50.0f);
    EXPECT_FALSE(retrieved.enable_attention_cue);

    dragonfly_audio_bridge_destroy(b);
}

TEST_F(AudioBridgeTest, CreateWithInvalidConfig) {
    audio_bridge_config_t config = audio_bridge_default_config();
    config.min_intensity_db = -100.0f;  // Invalid

    dragonfly_audio_bridge_t* b = dragonfly_audio_bridge_create(dragonfly, nullptr, &config);
    EXPECT_EQ(b, nullptr);
}

TEST_F(AudioBridgeTest, Reset) {
    // Inject a source first
    audio_source_t source = {0};
    source.azimuth = 0.5f;
    source.elevation = 0.1f;
    source.intensity_db = 60.0f;
    source.confidence = 0.8f;
    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);

    // Verify it was added
    audio_detection_result_t result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    EXPECT_EQ(result.num_sources, 1u);

    // Reset
    EXPECT_EQ(dragonfly_audio_bridge_reset(bridge), 0);

    // Verify cleared
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    EXPECT_EQ(result.num_sources, 0u);
}

//=============================================================================
// Source Injection Tests
//=============================================================================

TEST_F(AudioBridgeTest, InjectSingleSource) {
    audio_source_t source = {0};
    source.azimuth = 0.3f;
    source.elevation = 0.0f;
    source.intensity_db = 65.0f;
    source.confidence = 0.9f;
    source.frequency_hz = 1000.0f;
    source.bandwidth_hz = 200.0f;

    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);

    audio_detection_result_t result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    EXPECT_EQ(result.num_sources, 1u);
    EXPECT_NEAR(result.sources[0].azimuth, 0.3f, 0.01f);
    EXPECT_FLOAT_EQ(result.sources[0].intensity_db, 65.0f);
}

TEST_F(AudioBridgeTest, InjectMultipleSources) {
    for (int i = 0; i < 5; i++) {
        audio_source_t source = {0};
        source.azimuth = -1.0f + 0.5f * i;  // Spread across directions
        source.elevation = 0.0f;
        source.intensity_db = 50.0f + i * 5.0f;
        source.confidence = 0.7f;
        source.frequency_hz = 500.0f + i * 200.0f;

        EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);
    }

    audio_detection_result_t result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    EXPECT_EQ(result.num_sources, 5u);
}

TEST_F(AudioBridgeTest, InjectSourceUpdatesExisting) {
    // Inject at same location twice
    audio_source_t source = {0};
    source.azimuth = 0.4f;
    source.elevation = 0.0f;
    source.intensity_db = 60.0f;
    source.confidence = 0.8f;

    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);

    source.intensity_db = 70.0f;  // Update intensity
    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);

    audio_detection_result_t result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    EXPECT_EQ(result.num_sources, 1u);  // Should be same source
}

//=============================================================================
// Audio Frame Processing Tests
//=============================================================================

TEST_F(AudioBridgeTest, ProcessStereoFrame) {
    // Generate simple stereo test signal
    const uint32_t sample_rate = 44100;
    const uint32_t num_samples = 1024;
    std::vector<float> samples(num_samples * 2);

    // Simulate sound from the right (louder in right channel)
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float freq = 1000.0f;  // 1kHz tone
        float wave = sinf(2.0f * M_PI * freq * t);
        samples[i * 2] = wave * 0.3f;      // Left channel quieter
        samples[i * 2 + 1] = wave * 0.7f;  // Right channel louder
    }

    EXPECT_EQ(dragonfly_audio_bridge_process_frame(
        bridge, samples.data(), num_samples, 2, sample_rate), 0);

    audio_detection_result_t result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    // Should detect a source biased to the right
    if (result.num_sources > 0) {
        EXPECT_LT(result.sources[0].azimuth, 0);  // Negative = right
    }
}

TEST_F(AudioBridgeTest, ProcessSilentFrame) {
    const uint32_t num_samples = 1024;
    std::vector<float> samples(num_samples * 2, 0.0f);  // Silence

    EXPECT_EQ(dragonfly_audio_bridge_process_frame(
        bridge, samples.data(), num_samples, 2, 44100), 0);

    audio_detection_result_t result;
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, &result), 0);
    // May or may not detect anything depending on threshold
}

TEST_F(AudioBridgeTest, ProcessInvalidFrame) {
    EXPECT_EQ(dragonfly_audio_bridge_process_frame(bridge, nullptr, 1024, 2, 44100), -1);
    EXPECT_EQ(dragonfly_audio_bridge_process_frame(bridge, nullptr, 0, 2, 44100), -1);
}

//=============================================================================
// Attention Cueing Tests
//=============================================================================

TEST_F(AudioBridgeTest, GetAttentionCueWhenEmpty) {
    float direction[2];
    float priority;

    int result = dragonfly_audio_bridge_get_attention_cue(bridge, direction, &priority);
    EXPECT_EQ(result, 1);  // No cue available
}

TEST_F(AudioBridgeTest, GetAttentionCueWithSource) {
    audio_source_t source = {0};
    source.azimuth = 0.5f;
    source.elevation = 0.1f;
    source.intensity_db = 70.0f;
    source.confidence = 0.9f;

    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);

    float direction[2];
    float priority;
    int result = dragonfly_audio_bridge_get_attention_cue(bridge, direction, &priority);
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(direction[0], 0.5f, 0.1f);
    EXPECT_NEAR(direction[1], 0.1f, 0.1f);
    EXPECT_GT(priority, 0);
}

TEST_F(AudioBridgeTest, AttentionCuePriorityBoost) {
    audio_bridge_config_t config = audio_bridge_default_config();
    config.cue_priority_boost = 0.5f;
    dragonfly_audio_bridge_set_config(bridge, &config);

    audio_source_t source = {0};
    source.azimuth = 0.0f;
    source.intensity_db = 60.0f;
    source.confidence = 0.5f;
    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, &source), 0);

    float direction[2];
    float priority;
    EXPECT_EQ(dragonfly_audio_bridge_get_attention_cue(bridge, direction, &priority), 0);
    EXPECT_GE(priority, config.cue_priority_boost);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(AudioBridgeTest, AnglesToVector) {
    float direction[3];

    // Forward (azimuth=0, elevation=0)
    dragonfly_audio_bridge_angles_to_vector(0, 0, direction);
    EXPECT_NEAR(direction[0], 0.0f, 0.01f);  // x
    EXPECT_NEAR(direction[1], 0.0f, 0.01f);  // y
    EXPECT_NEAR(direction[2], 1.0f, 0.01f);  // z (forward)

    // Right (azimuth=pi/2)
    dragonfly_audio_bridge_angles_to_vector(M_PI / 2, 0, direction);
    EXPECT_NEAR(direction[0], 1.0f, 0.01f);  // x (right)
    EXPECT_NEAR(direction[1], 0.0f, 0.01f);
    EXPECT_NEAR(direction[2], 0.0f, 0.01f);

    // Up (elevation=pi/2)
    dragonfly_audio_bridge_angles_to_vector(0, M_PI / 2, direction);
    EXPECT_NEAR(direction[0], 0.0f, 0.01f);
    EXPECT_NEAR(direction[1], 1.0f, 0.01f);  // y (up)
    EXPECT_NEAR(direction[2], 0.0f, 0.01f);
}

TEST_F(AudioBridgeTest, EstimateDistance) {
    // At reference level, distance should be ~1m
    float dist = dragonfly_audio_bridge_estimate_distance(bridge, 70.0f);  // Default ref
    EXPECT_NEAR(dist, 1.0f, 0.5f);

    // Quieter = farther
    float dist2 = dragonfly_audio_bridge_estimate_distance(bridge, 50.0f);
    EXPECT_GT(dist2, dist);
}

TEST_F(AudioBridgeTest, AzimuthToITD) {
    // Forward (azimuth=0) should have ITD=0
    float itd = dragonfly_audio_bridge_azimuth_to_itd(bridge, 0.0f);
    EXPECT_NEAR(itd, 0.0f, 1e-6f);

    // 90 degrees should have max ITD
    float itd_right = dragonfly_audio_bridge_azimuth_to_itd(bridge, M_PI / 2);
    EXPECT_GT(itd_right, 0);

    // Left should be negative
    float itd_left = dragonfly_audio_bridge_azimuth_to_itd(bridge, -M_PI / 2);
    EXPECT_LT(itd_left, 0);
}

TEST_F(AudioBridgeTest, LocalizationModeName) {
    EXPECT_STREQ(dragonfly_audio_localization_mode_name(AUDIO_LOC_ITD), "ITD");
    EXPECT_STREQ(dragonfly_audio_localization_mode_name(AUDIO_LOC_ILD), "ILD");
    EXPECT_STREQ(dragonfly_audio_localization_mode_name(AUDIO_LOC_COMBINED), "Combined");
    EXPECT_STREQ(dragonfly_audio_localization_mode_name(AUDIO_LOC_SPECTRAL), "Spectral");
    EXPECT_STREQ(dragonfly_audio_localization_mode_name((audio_localization_mode_t)99), "Unknown");
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AudioBridgeTest, GetStats) {
    audio_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_audio_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.frames_processed, 0u);
    EXPECT_EQ(stats.sources_detected, 0u);
}

TEST_F(AudioBridgeTest, StatsTrackProcessing) {
    // Inject some sources
    for (int i = 0; i < 5; i++) {
        audio_source_t source = {0};
        source.azimuth = 0.1f * i;
        source.intensity_db = 60.0f;
        source.confidence = 0.8f;
        dragonfly_audio_bridge_inject_source(bridge, &source);
    }

    audio_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_audio_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.sources_detected, 5u);
}

TEST_F(AudioBridgeTest, ResetStats) {
    // Generate some stats
    audio_source_t source = {0};
    source.intensity_db = 60.0f;
    dragonfly_audio_bridge_inject_source(bridge, &source);

    EXPECT_EQ(dragonfly_audio_bridge_reset_stats(bridge), 0);

    audio_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_audio_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.sources_detected, 0u);
    EXPECT_EQ(stats.frames_processed, 0u);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST_F(AudioBridgeTest, SetConfig) {
    audio_bridge_config_t config = audio_bridge_default_config();
    config.min_intensity_db = 45.0f;
    config.loc_mode = AUDIO_LOC_ILD;

    EXPECT_EQ(dragonfly_audio_bridge_set_config(bridge, &config), 0);

    audio_bridge_config_t retrieved;
    EXPECT_EQ(dragonfly_audio_bridge_get_config(bridge, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.min_intensity_db, 45.0f);
    EXPECT_EQ(retrieved.loc_mode, AUDIO_LOC_ILD);
}

TEST_F(AudioBridgeTest, SetInvalidConfig) {
    audio_bridge_config_t config = audio_bridge_default_config();
    config.speed_of_sound_mps = 10.0f;  // Invalid

    EXPECT_EQ(dragonfly_audio_bridge_set_config(bridge, &config), -1);
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(AudioBridgeTest, NullPointerHandling) {
    audio_detection_result_t result;
    audio_bridge_stats_t stats;
    audio_bridge_config_t config;
    float direction[2];
    float priority;

    EXPECT_EQ(dragonfly_audio_bridge_reset(nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_inject_source(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_inject_source(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_result(nullptr, &result), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_result(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_reset_stats(nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_audio_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_config(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_attention_cue(nullptr, direction, &priority), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_attention_cue(bridge, nullptr, &priority), -1);
    EXPECT_EQ(dragonfly_audio_bridge_get_attention_cue(bridge, direction, nullptr), -1);
}
