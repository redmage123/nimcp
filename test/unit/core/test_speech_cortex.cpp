/**
 * @file test_speech_cortex.cpp
 * @brief Unit tests for Speech Cortex bidirectional functions (Phase 10.11.3)
 *
 * WHAT: Tests for speech-audio bidirectional feedback
 * WHY:  Validate speech cortex bidirectional API
 * HOW:  Test phoneme confidence and frequency boost requests
 *
 * @author NIMCP Phase 10.11.3
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cmath>

    #include "perception/nimcp_speech_cortex.h"

// =============================================================================
// TEST FIXTURES
// =============================================================================

class SpeechCortexBidirectionalTest : public ::testing::Test {
protected:
    speech_cortex_t* cortex;

    void SetUp() override {
        speech_cortex_config_t config = speech_cortex_default_config();

        cortex = speech_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed to create speech cortex";
    }

    void TearDown() override {
        if (cortex) {
            speech_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    // Helper: Simulate phoneme processing to update stats
    void simulate_phoneme_processing(uint32_t num_frames, uint32_t num_phonemes) {
        // Process frames to update internal statistics
        std::vector<float> audio(512, 0.0f);
        // Use config feature_dim (64) to match expected buffer size
        std::vector<float> features(64, 0.0f);

        for (uint32_t i = 0; i < num_frames; i++) {
            speech_cortex_process(cortex, audio.data(), audio.size(), features.data());
        }

        // Note: In real implementation, phoneme detection would update phonemes_detected
        // For now, this is a placeholder to test the API
    }
};

// =============================================================================
// PHONEME CONFIDENCE TESTS
// =============================================================================

TEST_F(SpeechCortexBidirectionalTest, GetPhonemeConfidenceNullCortex) {
    float confidence = speech_cortex_get_phoneme_confidence(nullptr);
    EXPECT_EQ(confidence, 0.0f) << "Null cortex should return 0 confidence";
}

TEST_F(SpeechCortexBidirectionalTest, GetPhonemeConfidenceInitial) {
    // With no processing, should return neutral confidence (0.5)
    float confidence = speech_cortex_get_phoneme_confidence(cortex);

    EXPECT_GE(confidence, 0.0f) << "Confidence should be >= 0";
    EXPECT_LE(confidence, 1.0f) << "Confidence should be <= 1";
    EXPECT_EQ(confidence, 0.5f) << "Initial confidence should be neutral (0.5)";
}

TEST_F(SpeechCortexBidirectionalTest, GetPhonemeConfidenceAfterProcessing) {
    // Simulate some processing
    simulate_phoneme_processing(10, 5);

    float confidence = speech_cortex_get_phoneme_confidence(cortex);

    EXPECT_GE(confidence, 0.0f) << "Confidence should be >= 0";
    EXPECT_LE(confidence, 1.0f) << "Confidence should be <= 1";
    // After processing, confidence should be influenced by detection rate
}

TEST_F(SpeechCortexBidirectionalTest, GetPhonemeConfidenceRange) {
    // Test confidence stays in valid range across multiple calls
    for (int i = 0; i < 10; i++) {
        simulate_phoneme_processing(5, 2);
        float confidence = speech_cortex_get_phoneme_confidence(cortex);

        EXPECT_GE(confidence, 0.0f) << "Confidence should be >= 0 at iteration " << i;
        EXPECT_LE(confidence, 1.0f) << "Confidence should be <= 1 at iteration " << i;
    }
}

TEST_F(SpeechCortexBidirectionalTest, GetPhonemeConfidenceMinimumThreshold) {
    // Even with poor detection, confidence should have minimum threshold (0.3)
    float confidence = speech_cortex_get_phoneme_confidence(cortex);
    EXPECT_GE(confidence, 0.3f) << "Confidence should have minimum threshold of 0.3";
}

// =============================================================================
// FREQUENCY BOOST REQUEST TESTS
// =============================================================================

TEST_F(SpeechCortexBidirectionalTest, RequestFrequencyBoostNullCortex) {
    float target_freq = 0.0f;
    float bandwidth = 0.0f;
    bool result = speech_cortex_request_frequency_boost(nullptr, &target_freq, &bandwidth);
    EXPECT_FALSE(result) << "Null cortex should return false";
}

TEST_F(SpeechCortexBidirectionalTest, RequestFrequencyBoostNullOutputs) {
    bool result1 = speech_cortex_request_frequency_boost(cortex, nullptr, nullptr);
    EXPECT_FALSE(result1) << "Null outputs should return false";

    float target_freq = 0.0f;
    bool result2 = speech_cortex_request_frequency_boost(cortex, &target_freq, nullptr);
    EXPECT_FALSE(result2) << "Null bandwidth should return false";

    float bandwidth = 0.0f;
    bool result3 = speech_cortex_request_frequency_boost(cortex, nullptr, &bandwidth);
    EXPECT_FALSE(result3) << "Null target_freq should return false";
}

TEST_F(SpeechCortexBidirectionalTest, RequestFrequencyBoostHighConfidence) {
    // Simulate high confidence scenario (no boost needed)
    // Note: Initial confidence is 0.5, which is below 0.7 threshold
    // So boost will be requested initially

    float target_freq = 0.0f;
    float bandwidth = 0.0f;
    bool result = speech_cortex_request_frequency_boost(cortex, &target_freq, &bandwidth);

    // With initial neutral confidence (0.5 < 0.7), boost should be requested
    EXPECT_TRUE(result) << "Low confidence should request boost";
}

TEST_F(SpeechCortexBidirectionalTest, RequestFrequencyBoostOutputValues) {
    float target_freq = 0.0f;
    float bandwidth = 0.0f;
    bool result = speech_cortex_request_frequency_boost(cortex, &target_freq, &bandwidth);

    if (result) {
        // Boost requested - check output values are reasonable
        EXPECT_GT(target_freq, 0.0f) << "Target frequency should be positive";
        EXPECT_GT(bandwidth, 0.0f) << "Bandwidth should be positive";

        // F2 formant range (most important for vowels): 800-2500 Hz
        // Center: ~1650 Hz, Bandwidth: ~850 Hz
        EXPECT_GE(target_freq, 800.0f) << "Target frequency should be in speech range";
        EXPECT_LE(target_freq, 2500.0f) << "Target frequency should be in speech range";

        EXPECT_GE(bandwidth, 500.0f) << "Bandwidth should be reasonable";
        EXPECT_LE(bandwidth, 2000.0f) << "Bandwidth should be reasonable";
    }
}

TEST_F(SpeechCortexBidirectionalTest, RequestFrequencyBoostF2FormantDefault) {
    float target_freq = 0.0f;
    float bandwidth = 0.0f;
    bool result = speech_cortex_request_frequency_boost(cortex, &target_freq, &bandwidth);

    if (result) {
        // Default should target F2 formant band (1650 Hz ± 850 Hz)
        EXPECT_FLOAT_EQ(target_freq, 1650.0f) << "Default should target F2 center";
        EXPECT_FLOAT_EQ(bandwidth, 850.0f) << "Default should use F2 bandwidth";
    }
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

TEST_F(SpeechCortexBidirectionalTest, LowConfidenceTriggersBoost) {
    // Workflow: Low confidence → Request frequency boost

    float confidence = speech_cortex_get_phoneme_confidence(cortex);

    if (confidence < 0.7f) {
        float target_freq = 0.0f;
        float bandwidth = 0.0f;
        bool boost_requested = speech_cortex_request_frequency_boost(cortex, &target_freq, &bandwidth);

        EXPECT_TRUE(boost_requested) << "Low confidence should trigger boost request";
        EXPECT_GT(target_freq, 0.0f) << "Boost should specify target frequency";
        EXPECT_GT(bandwidth, 0.0f) << "Boost should specify bandwidth";
    }
}

TEST_F(SpeechCortexBidirectionalTest, SpeechToAudioFeedbackLoop) {
    // Simulate speech cortex → audio cortex feedback

    // 1. Get phoneme confidence
    float confidence = speech_cortex_get_phoneme_confidence(cortex);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // 2. If low confidence, request frequency boost
    if (confidence < 0.7f) {
        float target_freq = 0.0f;
        float bandwidth = 0.0f;
        bool boost_needed = speech_cortex_request_frequency_boost(
            cortex, &target_freq, &bandwidth
        );

        EXPECT_TRUE(boost_needed);

        // 3. Audio cortex would use target_freq and bandwidth to adjust processing
        // (In real integration, audio cortex would adjust mel filterbank)
        SUCCEED() << "Feedback loop: Speech requests boost at "
                  << target_freq << " Hz ± " << bandwidth << " Hz";
    }
}

// =============================================================================
// CONSISTENCY TESTS
// =============================================================================

TEST_F(SpeechCortexBidirectionalTest, ConfidenceConsistency) {
    // Multiple calls should return consistent values
    float conf1 = speech_cortex_get_phoneme_confidence(cortex);
    float conf2 = speech_cortex_get_phoneme_confidence(cortex);

    EXPECT_FLOAT_EQ(conf1, conf2) << "Confidence should be consistent without state changes";
}

TEST_F(SpeechCortexBidirectionalTest, FrequencyBoostConsistency) {
    // Multiple calls should return consistent values
    float freq1 = 0.0f, bw1 = 0.0f;
    float freq2 = 0.0f, bw2 = 0.0f;

    bool result1 = speech_cortex_request_frequency_boost(cortex, &freq1, &bw1);
    bool result2 = speech_cortex_request_frequency_boost(cortex, &freq2, &bw2);

    EXPECT_EQ(result1, result2) << "Boost request result should be consistent";
    if (result1 && result2) {
        EXPECT_FLOAT_EQ(freq1, freq2) << "Target frequency should be consistent";
        EXPECT_FLOAT_EQ(bw1, bw2) << "Bandwidth should be consistent";
    }
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

TEST_F(SpeechCortexBidirectionalTest, PerformanceConfidenceQuery) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        speech_cortex_get_phoneme_confidence(cortex);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be very fast (< 0.1ms per call on average)
    float avg_us = duration.count() / 10000.0f;
    EXPECT_LT(avg_us, 100.0f) << "Confidence query should be < 0.1ms per call";
}

TEST_F(SpeechCortexBidirectionalTest, PerformanceFrequencyBoostRequest) {
    float target_freq = 0.0f;
    float bandwidth = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        speech_cortex_request_frequency_boost(cortex, &target_freq, &bandwidth);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be very fast (< 0.1ms per call on average)
    float avg_us = duration.count() / 10000.0f;
    EXPECT_LT(avg_us, 100.0f) << "Frequency boost request should be < 0.1ms per call";
}

// Note: main() provided by GTest::Main in CMake configuration
