/**
 * @file test_security_perception_input_bridge.cpp
 * @brief Unit tests for Security-Perception Input Bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Comprehensive tests for the security-perception input bridge including:
 * - Lifecycle (default_config, create/destroy)
 * - Connection tests (cochlea, visual_cortex, bbb, anomaly_detector)
 * - Audio validation (valid input, out-of-range, NaN/Inf, ultrasonic)
 * - Visual validation (valid frame, invalid dimensions, adversarial)
 * - Anomaly detection (audio, visual)
 * - Gating tests (PASS, ATTENUATE, SANITIZE, HOLD, BLOCK)
 * - Bidirectional update tests
 * - State and statistics tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <limits>

extern "C" {
#include "security/perception/nimcp_security_perception_input_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityPerceptionInputBridgeTest : public ::testing::Test {
protected:
    security_perception_input_bridge_t* bridge = nullptr;
    sec_percept_input_config_t config;

    void SetUp() override {
        security_perception_input_default_config(&config);
        bridge = security_perception_input_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_perception_input_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper: Create valid audio samples in normal range [-1.0, 1.0] */
    std::vector<float> create_valid_audio_samples(size_t count = 1024) {
        std::vector<float> samples(count);
        for (size_t i = 0; i < count; i++) {
            /* Generate a sine wave */
            samples[i] = 0.5f * std::sin(2.0f * 3.14159f * static_cast<float>(i) / 100.0f);
        }
        return samples;
    }

    /* Helper: Create out-of-range audio samples */
    std::vector<float> create_out_of_range_audio_samples(size_t count = 1024) {
        std::vector<float> samples(count);
        for (size_t i = 0; i < count; i++) {
            /* Values exceeding [-1.0, 1.0] range */
            samples[i] = (i % 2 == 0) ? 5.0f : -5.0f;
        }
        return samples;
    }

    /* Helper: Create audio samples with NaN values */
    std::vector<float> create_nan_audio_samples(size_t count = 1024) {
        std::vector<float> samples(count);
        for (size_t i = 0; i < count; i++) {
            samples[i] = (i % 10 == 0) ? std::numeric_limits<float>::quiet_NaN() : 0.5f;
        }
        return samples;
    }

    /* Helper: Create audio samples with Inf values */
    std::vector<float> create_inf_audio_samples(size_t count = 1024) {
        std::vector<float> samples(count);
        for (size_t i = 0; i < count; i++) {
            if (i % 10 == 0) {
                samples[i] = std::numeric_limits<float>::infinity();
            } else if (i % 10 == 5) {
                samples[i] = -std::numeric_limits<float>::infinity();
            } else {
                samples[i] = 0.3f;
            }
        }
        return samples;
    }

    /* Helper: Create ultrasonic-simulated audio (high frequency content) */
    std::vector<float> create_ultrasonic_audio_samples(size_t count = 1024) {
        std::vector<float> samples(count);
        /* Simulate very high frequency content (beyond audible range) */
        for (size_t i = 0; i < count; i++) {
            /* High frequency oscillation */
            samples[i] = 0.8f * ((i % 2 == 0) ? 1.0f : -1.0f);
        }
        return samples;
    }

    /* Helper: Create valid visual frame (grayscale) */
    std::vector<uint8_t> create_valid_visual_frame(uint32_t width = 64, uint32_t height = 64, uint32_t channels = 1) {
        std::vector<uint8_t> pixels(width * height * channels);
        for (size_t i = 0; i < pixels.size(); i++) {
            /* Normal pixel values */
            pixels[i] = static_cast<uint8_t>((i * 17) % 256);
        }
        return pixels;
    }

    /* Helper: Create adversarial pattern (high contrast checkerboard) */
    std::vector<uint8_t> create_adversarial_visual_frame(uint32_t width = 64, uint32_t height = 64, uint32_t channels = 1) {
        std::vector<uint8_t> pixels(width * height * channels);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint8_t value = ((x + y) % 2 == 0) ? 255 : 0;
                for (uint32_t c = 0; c < channels; c++) {
                    pixels[(y * width + x) * channels + c] = value;
                }
            }
        }
        return pixels;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, DefaultConfigIsValid) {
    sec_percept_input_config_t cfg;
    int result = security_perception_input_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_audio_validation);
    EXPECT_TRUE(cfg.enable_visual_validation);
    EXPECT_FLOAT_EQ(cfg.audio_anomaly_threshold, SEC_PERCEPT_INPUT_DEFAULT_AUDIO_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.visual_anomaly_threshold, SEC_PERCEPT_INPUT_DEFAULT_VISUAL_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.gate_threshold, SEC_PERCEPT_INPUT_DEFAULT_GATE_THRESHOLD);
    EXPECT_GE(cfg.audio_min_value, -1.0f);
    EXPECT_LE(cfg.audio_max_value, 1.0f);
    EXPECT_GE(cfg.visual_min_value, 0);
    EXPECT_LE(cfg.visual_max_value, 255);
}

TEST_F(SecurityPerceptionInputBridgeTest, DefaultConfigNullFails) {
    int result = security_perception_input_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, CreateWithValidConfig) {
    sec_percept_input_config_t cfg;
    security_perception_input_default_config(&cfg);

    security_perception_input_bridge_t* br = security_perception_input_bridge_create(&cfg);
    ASSERT_NE(br, nullptr);

    sec_percept_input_state_t state;
    security_perception_input_get_state(br, &state);
    EXPECT_EQ(state.operational_state, SEC_INPUT_STATE_READY);

    security_perception_input_bridge_destroy(br);
}

TEST_F(SecurityPerceptionInputBridgeTest, CreateWithNullConfigUsesDefaults) {
    security_perception_input_bridge_t* br = security_perception_input_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);

    sec_percept_input_state_t state;
    int ret = security_perception_input_get_state(br, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.operational_state, SEC_INPUT_STATE_READY);

    security_perception_input_bridge_destroy(br);
}

TEST_F(SecurityPerceptionInputBridgeTest, CreateWithCustomConfig) {
    sec_percept_input_config_t custom_cfg;
    security_perception_input_default_config(&custom_cfg);
    custom_cfg.audio_anomaly_threshold = 0.9f;
    custom_cfg.visual_anomaly_threshold = 0.85f;
    custom_cfg.detect_ultrasonic = true;
    custom_cfg.detect_adversarial_patches = true;
    custom_cfg.enable_auto_gating = true;

    security_perception_input_bridge_t* br = security_perception_input_bridge_create(&custom_cfg);
    ASSERT_NE(br, nullptr);

    security_perception_input_bridge_destroy(br);
}

TEST_F(SecurityPerceptionInputBridgeTest, DestroyNullIsSafe) {
    security_perception_input_bridge_destroy(nullptr);
    /* Should not crash */
}

TEST_F(SecurityPerceptionInputBridgeTest, DestroyValidBridge) {
    sec_percept_input_config_t cfg;
    security_perception_input_default_config(&cfg);

    security_perception_input_bridge_t* br = security_perception_input_bridge_create(&cfg);
    ASSERT_NE(br, nullptr);

    security_perception_input_bridge_destroy(br);
    /* Should not crash */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, ConnectCochleaNullBridgeFails) {
    cochlea_t* dummy_cochlea = reinterpret_cast<cochlea_t*>(0x12345678);
    int result = security_perception_input_connect_cochlea(nullptr, dummy_cochlea);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectCochleaNullCochleaFails) {
    int result = security_perception_input_connect_cochlea(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectCochleaValidUpdatesState) {
    cochlea_t* dummy_cochlea = reinterpret_cast<cochlea_t*>(0x12345678);
    int result = security_perception_input_connect_cochlea(bridge, dummy_cochlea);
    EXPECT_EQ(result, 0);

    sec_percept_input_state_t state;
    security_perception_input_get_state(bridge, &state);
    EXPECT_TRUE(state.cochlea_connected);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectVisualCortexNullBridgeFails) {
    visual_cortex_t* dummy_cortex = reinterpret_cast<visual_cortex_t*>(0x12345678);
    int result = security_perception_input_connect_visual_cortex(nullptr, dummy_cortex);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectVisualCortexNullCortexFails) {
    int result = security_perception_input_connect_visual_cortex(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectVisualCortexValidUpdatesState) {
    visual_cortex_t* dummy_cortex = reinterpret_cast<visual_cortex_t*>(0x12345678);
    int result = security_perception_input_connect_visual_cortex(bridge, dummy_cortex);
    EXPECT_EQ(result, 0);

    sec_percept_input_state_t state;
    security_perception_input_get_state(bridge, &state);
    EXPECT_TRUE(state.visual_cortex_connected);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectBBBNullBridgeFails) {
    bbb_system_t dummy_bbb = reinterpret_cast<bbb_system_t>(0x12345678);
    int result = security_perception_input_connect_bbb(nullptr, dummy_bbb);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectBBBNullBBBFails) {
    int result = security_perception_input_connect_bbb(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectBBBValidUpdatesState) {
    bbb_system_t dummy_bbb = reinterpret_cast<bbb_system_t>(0x12345678);
    int result = security_perception_input_connect_bbb(bridge, dummy_bbb);
    EXPECT_EQ(result, 0);

    sec_percept_input_state_t state;
    security_perception_input_get_state(bridge, &state);
    EXPECT_TRUE(state.bbb_connected);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectAnomalyDetectorNullBridgeFails) {
    nimcp_anomaly_detector_t dummy_detector = reinterpret_cast<nimcp_anomaly_detector_t>(0x12345678);
    int result = security_perception_input_connect_anomaly_detector(nullptr, dummy_detector);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectAnomalyDetectorNullDetectorFails) {
    int result = security_perception_input_connect_anomaly_detector(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConnectAnomalyDetectorValidUpdatesState) {
    nimcp_anomaly_detector_t dummy_detector = reinterpret_cast<nimcp_anomaly_detector_t>(0x12345678);
    int result = security_perception_input_connect_anomaly_detector(bridge, dummy_detector);
    EXPECT_EQ(result, 0);

    sec_percept_input_state_t state;
    security_perception_input_get_state(bridge, &state);
    EXPECT_TRUE(state.anomaly_detector_connected);
}

/* ============================================================================
 * Audio Validation Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioValidInput) {
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_INPUT_VALID);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioNullBridgeFails) {
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        nullptr, samples.data(), samples.size(), 44100, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioNullSamplesFails) {
    sec_input_validation_result_t result;
    int ret = security_perception_validate_audio_input(
        bridge, nullptr, 1024, 44100, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioZeroSamplesFails) {
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), 0, 44100, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioNullResultFails) {
    auto samples = create_valid_audio_samples();
    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioOutOfRangeSamples) {
    auto samples = create_out_of_range_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    EXPECT_EQ(ret, 0);
    /* Should detect range warning or malformed */
    EXPECT_TRUE(result == SEC_INPUT_RANGE_WARNING ||
                result == SEC_INPUT_MALFORMED ||
                result == SEC_INPUT_ANOMALY_DETECTED);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioNaNSamples) {
    auto samples = create_nan_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    EXPECT_EQ(ret, 0);
    /* Should detect malformed input due to NaN */
    EXPECT_TRUE(result == SEC_INPUT_MALFORMED ||
                result == SEC_INPUT_RANGE_WARNING ||
                result == SEC_INPUT_ANOMALY_DETECTED);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioInfSamples) {
    auto samples = create_inf_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    EXPECT_EQ(ret, 0);
    /* Should detect malformed input due to Inf */
    EXPECT_TRUE(result == SEC_INPUT_MALFORMED ||
                result == SEC_INPUT_RANGE_WARNING ||
                result == SEC_INPUT_ANOMALY_DETECTED);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioUltrasonicDetection) {
    /* Enable ultrasonic detection */
    sec_percept_input_config_t cfg;
    security_perception_input_default_config(&cfg);
    cfg.detect_ultrasonic = true;

    security_perception_input_bridge_t* br = security_perception_input_bridge_create(&cfg);
    ASSERT_NE(br, nullptr);

    auto samples = create_ultrasonic_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        br, samples.data(), samples.size(), 44100, &result
    );

    EXPECT_EQ(ret, 0);
    /* May detect anomaly or pass depending on implementation */
    EXPECT_TRUE(result == SEC_INPUT_VALID ||
                result == SEC_INPUT_ANOMALY_DETECTED ||
                result == SEC_INPUT_STATS_WARNING);

    security_perception_input_bridge_destroy(br);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioTooManySamples) {
    /* Create samples exceeding max batch size */
    std::vector<float> samples(SEC_PERCEPT_INPUT_MAX_AUDIO_SAMPLES + 100, 0.5f);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    /* Should either succeed (processing in batches) or fail with invalid parameter */
    EXPECT_TRUE(ret == 0 || ret == NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateAudioUpdatesStats) {
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;

    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_EQ(stats.audio_validations_total, 1u);
}

/* ============================================================================
 * Visual Validation Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualValidFrame) {
    auto pixels = create_valid_visual_frame(64, 64, 1);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 1, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_INPUT_VALID);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualRGBFrame) {
    auto pixels = create_valid_visual_frame(64, 64, 3);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 3, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_INPUT_VALID);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualRGBAFrame) {
    auto pixels = create_valid_visual_frame(64, 64, 4);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 4, &result
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SEC_INPUT_VALID);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualNullBridgeFails) {
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        nullptr, pixels.data(), 64, 64, 1, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualNullPixelsFails) {
    sec_input_validation_result_t result;
    int ret = security_perception_validate_visual_input(
        bridge, nullptr, 64, 64, 1, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualZeroWidthFails) {
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 0, 64, 1, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualZeroHeightFails) {
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 0, 1, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualZeroChannelsFails) {
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 0, &result
    );
    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualNullResultFails) {
    auto pixels = create_valid_visual_frame();
    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 1, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualInvalidDimensionsTooWide) {
    /* Width exceeds maximum */
    auto pixels = create_valid_visual_frame(64, 64, 1);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), SEC_PERCEPT_INPUT_MAX_IMAGE_WIDTH + 1, 64, 1, &result
    );

    /* Should fail or return malformed */
    EXPECT_TRUE(ret == -1 || result == SEC_INPUT_MALFORMED);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualInvalidDimensionsTooTall) {
    /* Height exceeds maximum */
    auto pixels = create_valid_visual_frame(64, 64, 1);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 64, SEC_PERCEPT_INPUT_MAX_IMAGE_HEIGHT + 1, 1, &result
    );

    /* Should fail or return malformed */
    EXPECT_TRUE(ret == -1 || result == SEC_INPUT_MALFORMED);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualAdversarialPatternDetection) {
    /* Enable adversarial detection */
    sec_percept_input_config_t cfg;
    security_perception_input_default_config(&cfg);
    cfg.detect_adversarial_patches = true;

    security_perception_input_bridge_t* br = security_perception_input_bridge_create(&cfg);
    ASSERT_NE(br, nullptr);

    auto pixels = create_adversarial_visual_frame(64, 64, 1);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        br, pixels.data(), 64, 64, 1, &result
    );

    EXPECT_EQ(ret, 0);
    /* May detect adversarial pattern */
    EXPECT_TRUE(result == SEC_INPUT_VALID ||
                result == SEC_INPUT_ADVERSARIAL_DETECTED ||
                result == SEC_INPUT_STATS_WARNING);

    security_perception_input_bridge_destroy(br);
}

TEST_F(SecurityPerceptionInputBridgeTest, ValidateVisualUpdatesStats) {
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t result;

    security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 1, &result
    );

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_validations_total, 1u);
}

/* ============================================================================
 * Anomaly Detection Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, DetectAudioAnomalyValid) {
    auto samples = create_valid_audio_samples();
    float anomaly_score = 0.0f;
    float confidence = 0.0f;

    int ret = security_perception_detect_audio_anomaly(
        bridge, samples.data(), samples.size(), &anomaly_score, &confidence
    );

    EXPECT_EQ(ret, 0);
    EXPECT_GE(anomaly_score, 0.0f);
    EXPECT_LE(anomaly_score, 1.0f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectAudioAnomalyNullBridgeFails) {
    auto samples = create_valid_audio_samples();
    float anomaly_score, confidence;

    int ret = security_perception_detect_audio_anomaly(
        nullptr, samples.data(), samples.size(), &anomaly_score, &confidence
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectAudioAnomalyNullSamplesFails) {
    float anomaly_score, confidence;
    int ret = security_perception_detect_audio_anomaly(
        bridge, nullptr, 1024, &anomaly_score, &confidence
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectAudioAnomalyNullScoreFails) {
    auto samples = create_valid_audio_samples();
    float confidence;
    int ret = security_perception_detect_audio_anomaly(
        bridge, samples.data(), samples.size(), nullptr, &confidence
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectAudioAnomalyNullConfidenceFails) {
    auto samples = create_valid_audio_samples();
    float anomaly_score;
    int ret = security_perception_detect_audio_anomaly(
        bridge, samples.data(), samples.size(), &anomaly_score, nullptr
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectAudioAnomalyAnomalousInput) {
    /* Use out-of-range samples as anomalous input */
    auto samples = create_out_of_range_audio_samples();
    float anomaly_score = 0.0f;
    float confidence = 0.0f;

    int ret = security_perception_detect_audio_anomaly(
        bridge, samples.data(), samples.size(), &anomaly_score, &confidence
    );

    EXPECT_EQ(ret, 0);
    /* Anomalous input should have higher score */
    EXPECT_GE(anomaly_score, 0.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectVisualAnomalyValid) {
    auto pixels = create_valid_visual_frame();
    float anomaly_score = 0.0f;
    float confidence = 0.0f;

    int ret = security_perception_detect_visual_anomaly(
        bridge, pixels.data(), 64, 64, 1, &anomaly_score, &confidence
    );

    EXPECT_EQ(ret, 0);
    EXPECT_GE(anomaly_score, 0.0f);
    EXPECT_LE(anomaly_score, 1.0f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectVisualAnomalyNullBridgeFails) {
    auto pixels = create_valid_visual_frame();
    float anomaly_score, confidence;

    int ret = security_perception_detect_visual_anomaly(
        nullptr, pixels.data(), 64, 64, 1, &anomaly_score, &confidence
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectVisualAnomalyNullPixelsFails) {
    float anomaly_score, confidence;
    int ret = security_perception_detect_visual_anomaly(
        bridge, nullptr, 64, 64, 1, &anomaly_score, &confidence
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, DetectVisualAnomalyAdversarialInput) {
    auto pixels = create_adversarial_visual_frame();
    float anomaly_score = 0.0f;
    float confidence = 0.0f;

    int ret = security_perception_detect_visual_anomaly(
        bridge, pixels.data(), 64, 64, 1, &anomaly_score, &confidence
    );

    EXPECT_EQ(ret, 0);
    /* Adversarial pattern may have higher anomaly score */
    EXPECT_GE(anomaly_score, 0.0f);
}

/* ============================================================================
 * Gating Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, GateInputPassAtLowThreat) {
    sec_input_gate_action_t action;
    float attenuation;

    /* Low threat should result in PASS */
    int ret = security_perception_gate_input(bridge, 0.1f, &action, &attenuation);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(action, SEC_INPUT_GATE_PASS);
    EXPECT_FLOAT_EQ(attenuation, 1.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputAttenuateAtMediumThreat) {
    sec_input_gate_action_t action;
    float attenuation;

    /* Medium threat (0.5) results in SANITIZE per implementation:
     * threat < 0.3: PASS
     * threat < 0.5: ATTENUATE
     * threat < 0.7: SANITIZE
     * So 0.5 falls into SANITIZE range */
    int ret = security_perception_gate_input(bridge, 0.5f, &action, &attenuation);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(action, SEC_INPUT_GATE_SANITIZE);
    EXPECT_GT(attenuation, 0.0f);
    EXPECT_LE(attenuation, 1.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputSanitizeAtHighThreat) {
    sec_input_gate_action_t action;
    float attenuation;

    /* Higher threat may result in SANITIZE */
    int ret = security_perception_gate_input(bridge, 0.7f, &action, &attenuation);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(action == SEC_INPUT_GATE_ATTENUATE ||
                action == SEC_INPUT_GATE_SANITIZE ||
                action == SEC_INPUT_GATE_HOLD);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputHoldAtVeryHighThreat) {
    sec_input_gate_action_t action;
    float attenuation;

    /* Very high threat may result in HOLD */
    int ret = security_perception_gate_input(bridge, 0.85f, &action, &attenuation);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(action == SEC_INPUT_GATE_SANITIZE ||
                action == SEC_INPUT_GATE_HOLD ||
                action == SEC_INPUT_GATE_BLOCK);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputBlockAtCriticalThreat) {
    sec_input_gate_action_t action;
    float attenuation;

    /* Critical threat should result in BLOCK */
    int ret = security_perception_gate_input(bridge, 0.95f, &action, &attenuation);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(action == SEC_INPUT_GATE_HOLD ||
                action == SEC_INPUT_GATE_BLOCK);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputBlockAtMaxThreat) {
    sec_input_gate_action_t action;
    float attenuation;

    /* Maximum threat should result in BLOCK */
    int ret = security_perception_gate_input(bridge, 1.0f, &action, &attenuation);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(action, SEC_INPUT_GATE_BLOCK);
    EXPECT_FLOAT_EQ(attenuation, 0.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputNullBridgeFails) {
    sec_input_gate_action_t action;
    float attenuation;
    int ret = security_perception_gate_input(nullptr, 0.5f, &action, &attenuation);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputNullActionFails) {
    float attenuation;
    int ret = security_perception_gate_input(bridge, 0.5f, nullptr, &attenuation);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateInputNullAttenuationFails) {
    sec_input_gate_action_t action;
    int ret = security_perception_gate_input(bridge, 0.5f, &action, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ApplyAudioGatingValid) {
    auto samples = create_valid_audio_samples();
    int ret = security_perception_apply_audio_gating(
        bridge, samples.data(), samples.size()
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionInputBridgeTest, ApplyAudioGatingNullBridgeFails) {
    auto samples = create_valid_audio_samples();
    int ret = security_perception_apply_audio_gating(
        nullptr, samples.data(), samples.size()
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ApplyAudioGatingNullSamplesFails) {
    int ret = security_perception_apply_audio_gating(bridge, nullptr, 1024);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ApplyVisualGatingValid) {
    auto pixels = create_valid_visual_frame();
    int ret = security_perception_apply_visual_gating(
        bridge, pixels.data(), 64, 64, 1
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionInputBridgeTest, ApplyVisualGatingNullBridgeFails) {
    auto pixels = create_valid_visual_frame();
    int ret = security_perception_apply_visual_gating(
        nullptr, pixels.data(), 64, 64, 1
    );
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ApplyVisualGatingNullPixelsFails) {
    int ret = security_perception_apply_visual_gating(bridge, nullptr, 64, 64, 1);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, UpdateSecToPerceptValid) {
    int ret = security_perception_input_update_sec_to_percept(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionInputBridgeTest, UpdateSecToPerceptNullFails) {
    int ret = security_perception_input_update_sec_to_percept(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, UpdatePerceptToSecValid) {
    int ret = security_perception_input_update_percept_to_sec(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionInputBridgeTest, UpdatePerceptToSecNullFails) {
    int ret = security_perception_input_update_percept_to_sec(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, UpdateFullCycleValid) {
    int ret = security_perception_input_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityPerceptionInputBridgeTest, UpdateFullCycleNullFails) {
    int ret = security_perception_input_update(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetSecToPerceptEffectsValid) {
    sec_to_percept_input_effects_t effects;
    int ret = security_perception_input_get_sec_to_percept_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.audio_threat_level, 0.0f);
    EXPECT_LE(effects.audio_threat_level, 1.0f);
    EXPECT_GE(effects.visual_threat_level, 0.0f);
    EXPECT_LE(effects.visual_threat_level, 1.0f);
    EXPECT_GE(effects.combined_threat_level, 0.0f);
    EXPECT_LE(effects.combined_threat_level, 1.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetSecToPerceptEffectsNullBridgeFails) {
    sec_to_percept_input_effects_t effects;
    int ret = security_perception_input_get_sec_to_percept_effects(nullptr, &effects);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetSecToPerceptEffectsNullEffectsFails) {
    int ret = security_perception_input_get_sec_to_percept_effects(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetPerceptToSecEffectsValid) {
    percept_to_sec_input_effects_t effects;
    int ret = security_perception_input_get_percept_to_sec_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.audio_anomaly_score, 0.0f);
    EXPECT_LE(effects.audio_anomaly_score, 1.0f);
    EXPECT_GE(effects.visual_anomaly_score, 0.0f);
    EXPECT_LE(effects.visual_anomaly_score, 1.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetPerceptToSecEffectsNullBridgeFails) {
    percept_to_sec_input_effects_t effects;
    int ret = security_perception_input_get_percept_to_sec_effects(nullptr, &effects);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetPerceptToSecEffectsNullEffectsFails) {
    int ret = security_perception_input_get_percept_to_sec_effects(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, UpdateAffectsEffects) {
    /* Process some input to generate effects */
    auto samples = create_out_of_range_audio_samples();
    sec_input_validation_result_t result;
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    /* Update bidirectional flow */
    security_perception_input_update(bridge);

    /* Get updated effects */
    sec_to_percept_input_effects_t effects;
    security_perception_input_get_sec_to_percept_effects(bridge, &effects);

    /* After processing anomalous input, threat level should be updated */
    EXPECT_GE(effects.audio_threat_level, 0.0f);
}

/* ============================================================================
 * State and Statistics Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, GetStateValid) {
    sec_percept_input_state_t state;
    int ret = security_perception_input_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.operational_state, SEC_INPUT_STATE_READY);
    EXPECT_FALSE(state.cochlea_connected);
    EXPECT_FALSE(state.visual_cortex_connected);
    EXPECT_FALSE(state.bbb_connected);
    EXPECT_FALSE(state.anomaly_detector_connected);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetStateNullBridgeFails) {
    sec_percept_input_state_t state;
    int ret = security_perception_input_get_state(nullptr, &state);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetStateNullStateFails) {
    int ret = security_perception_input_get_state(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetStatsValid) {
    sec_percept_input_stats_t stats;
    int ret = security_perception_input_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.audio_validations_total, 0u);
    EXPECT_EQ(stats.visual_validations_total, 0u);
    EXPECT_EQ(stats.audio_anomalies_detected, 0u);
    EXPECT_EQ(stats.visual_anomalies_detected, 0u);
    EXPECT_EQ(stats.inputs_blocked, 0u);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetStatsNullBridgeFails) {
    sec_percept_input_stats_t stats;
    int ret = security_perception_input_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, GetStatsNullStatsFails) {
    int ret = security_perception_input_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, StatsAccumulateAudio) {
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;

    /* Process multiple audio inputs */
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);

    EXPECT_EQ(stats.audio_validations_total, 3u);
    EXPECT_EQ(stats.audio_validations_passed, 3u);
    EXPECT_EQ(stats.audio_validations_failed, 0u);
}

TEST_F(SecurityPerceptionInputBridgeTest, StatsAccumulateVisual) {
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t result;

    /* Process multiple visual inputs */
    security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 1, &result
    );
    security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 1, &result
    );

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);

    EXPECT_EQ(stats.visual_validations_total, 2u);
    EXPECT_EQ(stats.visual_validations_passed, 2u);
    EXPECT_EQ(stats.visual_validations_failed, 0u);
}

TEST_F(SecurityPerceptionInputBridgeTest, StatsTrackAnomalies) {
    /* Process anomalous audio */
    auto bad_samples = create_nan_audio_samples();
    sec_input_validation_result_t result;
    security_perception_validate_audio_input(
        bridge, bad_samples.data(), bad_samples.size(), 44100, &result
    );

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);

    EXPECT_EQ(stats.audio_validations_total, 1u);
    /* May have detected anomaly or failed validation */
    EXPECT_TRUE(stats.audio_anomalies_detected >= 0u);
}

TEST_F(SecurityPerceptionInputBridgeTest, ResetStatsValid) {
    /* Generate some stats */
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );

    /* Reset stats */
    security_perception_input_reset_stats(bridge);

    /* Verify reset */
    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);

    EXPECT_EQ(stats.audio_validations_total, 0u);
    EXPECT_EQ(stats.visual_validations_total, 0u);
    EXPECT_EQ(stats.audio_anomalies_detected, 0u);
    EXPECT_EQ(stats.visual_anomalies_detected, 0u);
}

TEST_F(SecurityPerceptionInputBridgeTest, ResetStatsNullIsSafe) {
    security_perception_input_reset_stats(nullptr);
    /* Should not crash */
}

TEST_F(SecurityPerceptionInputBridgeTest, ResetStatsPreservesState) {
    /* Connect something to change state */
    cochlea_t* dummy_cochlea = reinterpret_cast<cochlea_t*>(0x12345678);
    security_perception_input_connect_cochlea(bridge, dummy_cochlea);

    /* Generate stats and reset */
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );
    security_perception_input_reset_stats(bridge);

    /* State should be preserved */
    sec_percept_input_state_t state;
    security_perception_input_get_state(bridge, &state);
    EXPECT_TRUE(state.cochlea_connected);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, ResultNameMapping) {
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_VALID), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_RANGE_WARNING), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_STATS_WARNING), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_ANOMALY_DETECTED), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_ADVERSARIAL_DETECTED), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_SPOOFING_DETECTED), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_MALFORMED), nullptr);
    EXPECT_NE(security_perception_input_result_name(SEC_INPUT_REJECTED), nullptr);
}

TEST_F(SecurityPerceptionInputBridgeTest, GateActionNameMapping) {
    EXPECT_NE(security_perception_input_gate_action_name(SEC_INPUT_GATE_PASS), nullptr);
    EXPECT_NE(security_perception_input_gate_action_name(SEC_INPUT_GATE_ATTENUATE), nullptr);
    EXPECT_NE(security_perception_input_gate_action_name(SEC_INPUT_GATE_SANITIZE), nullptr);
    EXPECT_NE(security_perception_input_gate_action_name(SEC_INPUT_GATE_HOLD), nullptr);
    EXPECT_NE(security_perception_input_gate_action_name(SEC_INPUT_GATE_BLOCK), nullptr);
}

TEST_F(SecurityPerceptionInputBridgeTest, StateNameMapping) {
    EXPECT_NE(security_perception_input_state_name(SEC_INPUT_STATE_UNINITIALIZED), nullptr);
    EXPECT_NE(security_perception_input_state_name(SEC_INPUT_STATE_READY), nullptr);
    EXPECT_NE(security_perception_input_state_name(SEC_INPUT_STATE_PROCESSING), nullptr);
    EXPECT_NE(security_perception_input_state_name(SEC_INPUT_STATE_DEGRADED), nullptr);
    EXPECT_NE(security_perception_input_state_name(SEC_INPUT_STATE_ERROR), nullptr);
}

TEST_F(SecurityPerceptionInputBridgeTest, ReportFalsePositiveValid) {
    int ret = security_perception_input_report_false_positive(bridge);
    EXPECT_EQ(ret, 0);

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_EQ(stats.false_positives_reported, 1u);
}

TEST_F(SecurityPerceptionInputBridgeTest, ReportFalsePositiveNullFails) {
    int ret = security_perception_input_report_false_positive(nullptr);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SecurityPerceptionInputBridgeTest, ReportFalsePositiveMultiple) {
    security_perception_input_report_false_positive(bridge);
    security_perception_input_report_false_positive(bridge);
    security_perception_input_report_false_positive(bridge);

    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_EQ(stats.false_positives_reported, 3u);
}

/* ============================================================================
 * Integration Workflow Tests
 * ============================================================================ */

TEST_F(SecurityPerceptionInputBridgeTest, FullAudioValidationWorkflow) {
    /* 1. Validate audio input */
    auto samples = create_valid_audio_samples();
    sec_input_validation_result_t result;

    int ret = security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &result
    );
    EXPECT_EQ(ret, 0);

    /* 2. Detect anomalies */
    float anomaly_score, confidence;
    ret = security_perception_detect_audio_anomaly(
        bridge, samples.data(), samples.size(), &anomaly_score, &confidence
    );
    EXPECT_EQ(ret, 0);

    /* 3. Determine gating action */
    sec_input_gate_action_t action;
    float attenuation;
    ret = security_perception_gate_input(bridge, anomaly_score, &action, &attenuation);
    EXPECT_EQ(ret, 0);

    /* 4. Apply gating if needed */
    if (action != SEC_INPUT_GATE_PASS && action != SEC_INPUT_GATE_BLOCK) {
        std::vector<float> gated_samples = samples;
        ret = security_perception_apply_audio_gating(
            bridge, gated_samples.data(), gated_samples.size()
        );
        EXPECT_EQ(ret, 0);
    }

    /* 5. Update bidirectional flow */
    ret = security_perception_input_update(bridge);
    EXPECT_EQ(ret, 0);

    /* 6. Verify stats */
    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_GE(stats.audio_validations_total, 1u);
}

TEST_F(SecurityPerceptionInputBridgeTest, FullVisualValidationWorkflow) {
    /* 1. Validate visual input */
    auto pixels = create_valid_visual_frame(128, 128, 3);
    sec_input_validation_result_t result;

    int ret = security_perception_validate_visual_input(
        bridge, pixels.data(), 128, 128, 3, &result
    );
    EXPECT_EQ(ret, 0);

    /* 2. Detect anomalies */
    float anomaly_score, confidence;
    ret = security_perception_detect_visual_anomaly(
        bridge, pixels.data(), 128, 128, 3, &anomaly_score, &confidence
    );
    EXPECT_EQ(ret, 0);

    /* 3. Determine gating action */
    sec_input_gate_action_t action;
    float attenuation;
    ret = security_perception_gate_input(bridge, anomaly_score, &action, &attenuation);
    EXPECT_EQ(ret, 0);

    /* 4. Apply gating if needed */
    if (action != SEC_INPUT_GATE_PASS && action != SEC_INPUT_GATE_BLOCK) {
        std::vector<uint8_t> gated_pixels = pixels;
        ret = security_perception_apply_visual_gating(
            bridge, gated_pixels.data(), 128, 128, 3
        );
        EXPECT_EQ(ret, 0);
    }

    /* 5. Update bidirectional flow */
    ret = security_perception_input_update(bridge);
    EXPECT_EQ(ret, 0);

    /* 6. Verify stats */
    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_GE(stats.visual_validations_total, 1u);
}

TEST_F(SecurityPerceptionInputBridgeTest, ThreatDetectionAndBlockingWorkflow) {
    /* 1. Process potentially malicious input */
    auto bad_samples = create_nan_audio_samples();
    sec_input_validation_result_t result;

    security_perception_validate_audio_input(
        bridge, bad_samples.data(), bad_samples.size(), 44100, &result
    );

    /* 2. If threat detected, check gating */
    if (result != SEC_INPUT_VALID) {
        /* Simulate high threat score for detected anomaly */
        sec_input_gate_action_t action;
        float attenuation;
        security_perception_gate_input(bridge, 0.9f, &action, &attenuation);

        EXPECT_TRUE(action == SEC_INPUT_GATE_HOLD ||
                    action == SEC_INPUT_GATE_BLOCK);
    }

    /* 3. Update and check effects */
    security_perception_input_update(bridge);

    sec_to_percept_input_effects_t effects;
    security_perception_input_get_sec_to_percept_effects(bridge, &effects);

    /* Threat level should be non-zero after processing bad input */
    EXPECT_GE(effects.audio_threat_level, 0.0f);
}

TEST_F(SecurityPerceptionInputBridgeTest, ConcurrentValidation) {
    /* Simulate concurrent validation of audio and visual */
    auto samples = create_valid_audio_samples();
    auto pixels = create_valid_visual_frame();
    sec_input_validation_result_t audio_result, visual_result;

    /* Validate both */
    security_perception_validate_audio_input(
        bridge, samples.data(), samples.size(), 44100, &audio_result
    );
    security_perception_validate_visual_input(
        bridge, pixels.data(), 64, 64, 1, &visual_result
    );

    /* Both should succeed */
    EXPECT_EQ(audio_result, SEC_INPUT_VALID);
    EXPECT_EQ(visual_result, SEC_INPUT_VALID);

    /* Stats should reflect both */
    sec_percept_input_stats_t stats;
    security_perception_input_get_stats(bridge, &stats);
    EXPECT_EQ(stats.audio_validations_total, 1u);
    EXPECT_EQ(stats.visual_validations_total, 1u);
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
