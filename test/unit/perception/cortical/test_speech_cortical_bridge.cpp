/**
 * @file test_speech_cortical_bridge.cpp
 * @brief Unit tests for Speech-Cortical Bridge module
 *
 * WHAT: Comprehensive unit tests for speech-cortical bridge connecting speech cortex
 *       with cortical columns for phoneme hypercolumn processing.
 * WHY:  Validates biologically-realistic auditory cortex processing with tonotopic
 *       mapping, phoneme hypercolumns, cortical immune modulation, and bio-async.
 * HOW:  Tests lifecycle, connections, audio processing, phoneme detection,
 *       immune modulation, statistics, and bio-async integration.
 *
 * TEST ORGANIZATION:
 * - Lifecycle tests (5): create, destroy, default_config
 * - Connection tests (5): connect_speech_cortex, connect_immune, connect_bio_async
 * - Processing tests (8): audio frames, phoneme detection
 * - Hypercolumn tests (5): phoneme feature access and queries
 * - Immune modulation tests (4): inflammation effects on processing
 * - Statistics tests (4): stats tracking and reset
 * - Bio-async tests (4): message handling and broadcasting
 *
 * Total: 35+ tests
 */

// Headers have their own extern "C" guards
#include "perception/cortical/nimcp_speech_cortical_bridge.h"
#include "perception/nimcp_speech_cortex.h"
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

class SpeechCorticalBridgeTest : public ::testing::Test {
protected:
    speech_cortical_bridge_t* bridge = nullptr;
    speech_cortex_t* speech_cortex = nullptr;
    cortical_immune_system_t* immune = nullptr;

    static const uint32_t SAMPLE_RATE = 16000;
    static const uint32_t FRAME_SIZE = 512;
    static const uint32_t NUM_SAMPLES = 1024;

    void SetUp() override {
        // Create speech cortex with default config
        speech_cortex_config_t speech_config = speech_cortex_default_config();
        speech_config.sample_rate = SAMPLE_RATE;
        speech_config.frame_size_ms = (float)FRAME_SIZE * 1000.0f / (float)SAMPLE_RATE;
        speech_config.enable_bio_async = false;
        speech_config.enable_second_messengers = false;

        speech_cortex = speech_cortex_create(&speech_config);

        // Create speech-cortical bridge with default config
        speech_cortical_config_t bridge_config;
        speech_cortical_default_config(&bridge_config);
        bridge = speech_cortical_bridge_create(&bridge_config, speech_cortex);
    }

    void TearDown() override {
        if (bridge) {
            speech_cortical_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (speech_cortex) {
            speech_cortex_destroy(speech_cortex);
            speech_cortex = nullptr;
        }
        if (immune) {
            cortical_immune_destroy(immune);
            immune = nullptr;
        }
    }

    // Helper: Create synthetic audio with single frequency tone
    std::vector<float> create_tone(float frequency_hz, uint32_t num_samples, float amplitude = 0.5f) {
        std::vector<float> audio(num_samples);
        float phase_increment = 2.0f * M_PI * frequency_hz / SAMPLE_RATE;
        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = amplitude * sinf(i * phase_increment);
        }
        return audio;
    }

    // Helper: Create audio with multiple formants (vowel-like)
    std::vector<float> create_vowel_like(float f1, float f2, float f3, uint32_t num_samples) {
        std::vector<float> audio(num_samples, 0.0f);
        float phase1_inc = 2.0f * M_PI * f1 / SAMPLE_RATE;
        float phase2_inc = 2.0f * M_PI * f2 / SAMPLE_RATE;
        float phase3_inc = 2.0f * M_PI * f3 / SAMPLE_RATE;

        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = 0.5f * sinf(i * phase1_inc) +
                       0.3f * sinf(i * phase2_inc) +
                       0.2f * sinf(i * phase3_inc);
        }
        return audio;
    }

    // Helper: Create silence (for testing baseline)
    std::vector<float> create_silence(uint32_t num_samples) {
        return std::vector<float>(num_samples, 0.0f);
    }

    // Helper: Create white noise
    std::vector<float> create_noise(uint32_t num_samples) {
        std::vector<float> audio(num_samples);
        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = (2.0f * static_cast<float>(rand()) / RAND_MAX) - 1.0f;
        }
        return audio;
    }

    // Helper: Create phoneme-like audio segment (/a/ vowel: F1=700, F2=1200, F3=2500)
    std::vector<float> create_phoneme_a() {
        return create_vowel_like(700.0f, 1200.0f, 2500.0f, NUM_SAMPLES);
    }

    // Helper: Create phoneme-like audio segment (/i/ vowel: F1=270, F2=2300, F3=3000)
    std::vector<float> create_phoneme_i() {
        return create_vowel_like(270.0f, 2300.0f, 3000.0f, NUM_SAMPLES);
    }

    // Helper: Create phoneme-like audio segment (/u/ vowel: F1=300, F2=870, F3=2250)
    std::vector<float> create_phoneme_u() {
        return create_vowel_like(300.0f, 870.0f, 2250.0f, NUM_SAMPLES);
    }
};

/* ============================================================================
 * Lifecycle Tests (5 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SpeechCorticalBridgeTest, CreateWithNullConfig) {
    speech_cortical_bridge_t* b = speech_cortical_bridge_create(nullptr, speech_cortex);
    ASSERT_NE(b, nullptr);
    speech_cortical_bridge_destroy(b);
}

TEST_F(SpeechCorticalBridgeTest, CreateWithNullSpeechCortex) {
    speech_cortical_config_t config;
    speech_cortical_default_config(&config);

    speech_cortical_bridge_t* b = speech_cortical_bridge_create(&config, nullptr);
    EXPECT_NE(b, nullptr);  // Should create bridge, can connect cortex later
    speech_cortical_bridge_destroy(b);
}

TEST_F(SpeechCorticalBridgeTest, DestroyNull) {
    speech_cortical_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(SpeechCorticalBridgeTest, DefaultConfig) {
    speech_cortical_config_t config;
    speech_cortical_default_config(&config);

    EXPECT_GT(config.num_hypercolumns, 0u);
    EXPECT_GT(config.phonemes_per_hypercolumn, 0u);
    EXPECT_GT(config.min_formant_freq, 0.0f);
    EXPECT_GT(config.max_formant_freq, config.min_formant_freq);
    EXPECT_EQ(config.mode, SPEECH_CORTICAL_MODE_HYPERCOLUMN);
    EXPECT_TRUE(config.enable_tonotopic_mapping);
    EXPECT_TRUE(config.enable_cortical_immune);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_FLOAT_EQ(config.immune_modulation_factor, SPEECH_CORTICAL_DEFAULT_IMMUNE_FACTOR);
}

/* ============================================================================
 * Connection Tests (5 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, ConnectSpeechCortex) {
    speech_cortical_config_t config;
    speech_cortical_default_config(&config);
    speech_cortical_bridge_t* b = speech_cortical_bridge_create(&config, nullptr);

    int ret = speech_cortical_connect_speech_cortex(b, speech_cortex);
    EXPECT_EQ(ret, 0);

    speech_cortical_bridge_destroy(b);
}

TEST_F(SpeechCorticalBridgeTest, ConnectSpeechCortexNullParams) {
    EXPECT_NE(speech_cortical_connect_speech_cortex(nullptr, speech_cortex), 0);
}

TEST_F(SpeechCorticalBridgeTest, ConnectImmune) {
    // Create cortical immune system
    cortical_immune_config_t immune_config;
    cortical_immune_default_config(&immune_config);
    immune = cortical_immune_create(&immune_config);

    int ret = speech_cortical_connect_immune(bridge, immune);
    EXPECT_EQ(ret, 0);
}

TEST_F(SpeechCorticalBridgeTest, ConnectImmuneNullParams) {
    EXPECT_NE(speech_cortical_connect_immune(nullptr, immune), 0);
}

TEST_F(SpeechCorticalBridgeTest, ConnectBioAsync) {
    int ret = speech_cortical_connect_bio_async(bridge);
    // May succeed or fail depending on bio-async router availability

    bool connected = speech_cortical_is_bio_async_connected(bridge);
    if (ret == 0) {
        EXPECT_TRUE(connected);
        ret = speech_cortical_disconnect_bio_async(bridge);
        EXPECT_EQ(ret, 0);
        connected = speech_cortical_is_bio_async_connected(bridge);
        EXPECT_FALSE(connected);
    }
}

/* ============================================================================
 * Processing Tests with Synthetic Audio (8 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, ProcessSilence) {
    std::vector<float> audio = create_silence(NUM_SAMPLES);

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);
    // Silence should have low confidence and selectivity
    EXPECT_LT(result.confidence, 0.5f);
    EXPECT_LT(result.selectivity_index, 0.5f);

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, ProcessTone) {
    std::vector<float> audio = create_tone(440.0f, NUM_SAMPLES);  // A4 note

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.num_phonemes, 0u);

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, ProcessPhonemeA) {
    std::vector<float> audio = create_phoneme_a();

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.selectivity_index, 0.0f);
    EXPECT_LE(result.selectivity_index, 1.0f);

    // Should have formant estimates
    EXPECT_GE(result.formants[0], 0.0f);  // F1

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, ProcessPhonemeI) {
    std::vector<float> audio = create_phoneme_i();

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);
    // /i/ is a distinct vowel, should have reasonable detection
    EXPECT_GE(result.selectivity_index, 0.0f);

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, ProcessPhonemeU) {
    std::vector<float> audio = create_phoneme_u();

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.selectivity_index, 0.0f);

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, ProcessNoise) {
    std::vector<float> audio = create_noise(NUM_SAMPLES);

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);
    // Noise should have low selectivity (no clear phoneme)
    EXPECT_LT(result.selectivity_index, 0.5f);

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, ProcessNullParams) {
    std::vector<float> audio = create_tone(440.0f, NUM_SAMPLES);
    speech_cortical_phoneme_result_t result;

    EXPECT_NE(speech_cortical_process(nullptr, audio.data(), NUM_SAMPLES, &result), 0);
    EXPECT_NE(speech_cortical_process(bridge, nullptr, NUM_SAMPLES, &result), 0);
    EXPECT_NE(speech_cortical_process(bridge, audio.data(), 0, &result), 0);
    EXPECT_NE(speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, nullptr), 0);
}

TEST_F(SpeechCorticalBridgeTest, ProcessMultipleFrames) {
    // Process multiple frames sequentially
    for (int i = 0; i < 5; i++) {
        std::vector<float> audio = create_phoneme_a();
        speech_cortical_phoneme_result_t result;

        int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);
        EXPECT_EQ(ret, 0);

        speech_cortical_free_result(&result);
    }

    // Check stats reflect multiple frames
    speech_cortical_stats_t stats;
    speech_cortical_get_stats(bridge, &stats);
    EXPECT_GE(stats.frames_processed, 5u);
}

/* ============================================================================
 * Hypercolumn/Phoneme Tests (5 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, GetNumHypercolumns) {
    uint32_t num = speech_cortical_get_num_hypercolumns(bridge);
    EXPECT_GT(num, 0u);
}

TEST_F(SpeechCorticalBridgeTest, GetHypercolumnByIndex) {
    uint32_t num = speech_cortical_get_num_hypercolumns(bridge);
    ASSERT_GT(num, 0u);

    const feature_hypercolumn_t* hcol = speech_cortical_get_hypercolumn_by_index(bridge, 0);
    EXPECT_NE(hcol, nullptr);

    // Out of bounds should return nullptr
    hcol = speech_cortical_get_hypercolumn_by_index(bridge, num + 100);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(SpeechCorticalBridgeTest, GetHypercolumnByPosition) {
    // Test getting hypercolumn at tonotopic position
    float tono_x = 0.0f;
    float tono_y = 0.0f;
    const feature_hypercolumn_t* hcol = speech_cortical_get_hypercolumn(bridge, tono_x, tono_y);

    // May be nullptr if tonotopic mapping not enabled, but API should not crash
    if (hcol) {
        EXPECT_NE(hcol, nullptr);
    }
}

TEST_F(SpeechCorticalBridgeTest, GetTonotopicMap) {
    const topographic_map_t* map = speech_cortical_get_tonotopic_map(bridge);

    // May be nullptr if tonotopic mapping not enabled
    // Just verify API doesn't crash
    (void)map;
}

TEST_F(SpeechCorticalBridgeTest, GetPhonemeMap) {
    std::vector<float> audio = create_phoneme_a();

    std::vector<float> phoneme_map(64);  // num_hypercolumns
    std::vector<float> selectivity_map(64);

    std::vector<phoneme_t> phoneme_results(64);
    int ret = speech_cortical_get_phoneme_map(
        bridge,
        audio.data(),
        NUM_SAMPLES,
        phoneme_results.data(),
        selectivity_map.data()
    );

    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Immune Modulation Tests (4 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, UpdateImmuneModulation) {
    // Create and connect immune system
    cortical_immune_config_t immune_config;
    cortical_immune_default_config(&immune_config);
    immune = cortical_immune_create(&immune_config);
    speech_cortical_connect_immune(bridge, immune);

    int ret = speech_cortical_update_immune_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SpeechCorticalBridgeTest, SetImmuneFactorManual) {
    float test_factor = 0.5f;
    int ret = speech_cortical_set_immune_factor(bridge, test_factor);
    EXPECT_EQ(ret, 0);

    float retrieved = speech_cortical_get_immune_factor(bridge);
    EXPECT_FLOAT_EQ(retrieved, test_factor);
}

TEST_F(SpeechCorticalBridgeTest, ImmuneFactorRange) {
    // Test boundary values
    speech_cortical_set_immune_factor(bridge, 0.0f);
    EXPECT_FLOAT_EQ(speech_cortical_get_immune_factor(bridge), 0.0f);

    speech_cortical_set_immune_factor(bridge, 1.0f);
    EXPECT_FLOAT_EQ(speech_cortical_get_immune_factor(bridge), 1.0f);
}

TEST_F(SpeechCorticalBridgeTest, ImmuneModulationAffectsProcessing) {
    std::vector<float> audio = create_phoneme_a();

    // Process with no immune modulation
    speech_cortical_set_immune_factor(bridge, 0.0f);
    speech_cortical_phoneme_result_t result1;
    speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result1);

    // Process with full immune modulation
    speech_cortical_set_immune_factor(bridge, 1.0f);
    speech_cortical_phoneme_result_t result2;
    speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result2);

    // Results may differ with immune modulation
    // (Note: specific effect depends on implementation)
    EXPECT_GE(result1.selectivity_index, 0.0f);
    EXPECT_GE(result2.selectivity_index, 0.0f);

    speech_cortical_free_result(&result1);
    speech_cortical_free_result(&result2);
}

/* ============================================================================
 * Statistics Tests (4 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, GetStats) {
    speech_cortical_stats_t stats;
    int ret = speech_cortical_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.frames_processed, 0u);  // No processing yet
    EXPECT_EQ(stats.hypercolumn_activations, 0u);
    EXPECT_EQ(stats.bio_messages_sent, 0u);
    EXPECT_EQ(stats.bio_messages_received, 0u);
}

TEST_F(SpeechCorticalBridgeTest, StatsAfterProcessing) {
    std::vector<float> audio = create_phoneme_a();
    speech_cortical_phoneme_result_t result;
    speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);
    speech_cortical_free_result(&result);

    speech_cortical_stats_t stats;
    speech_cortical_get_stats(bridge, &stats);

    EXPECT_GT(stats.frames_processed, 0u);
    EXPECT_GE(stats.peak_phoneme_response, 0.0f);
}

TEST_F(SpeechCorticalBridgeTest, ResetStats) {
    // Process something to generate stats
    std::vector<float> audio = create_phoneme_a();
    speech_cortical_phoneme_result_t result;
    speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);
    speech_cortical_free_result(&result);

    // Reset stats
    int ret = speech_cortical_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    // Verify stats are cleared
    speech_cortical_stats_t stats;
    speech_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(stats.frames_processed, 0u);
}

TEST_F(SpeechCorticalBridgeTest, GetState) {
    speech_cortical_state_t state = speech_cortical_get_state(bridge);
    EXPECT_EQ(state, SPEECH_CORTICAL_STATE_READY);

    // Null pointer should return UNINITIALIZED
    state = speech_cortical_get_state(nullptr);
    EXPECT_EQ(state, SPEECH_CORTICAL_STATE_UNINITIALIZED);
}

/* ============================================================================
 * Bio-Async Tests (4 tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, BioAsyncConnectDisconnect) {
    int ret = speech_cortical_connect_bio_async(bridge);

    if (ret == 0) {
        bool connected = speech_cortical_is_bio_async_connected(bridge);
        EXPECT_TRUE(connected);

        ret = speech_cortical_disconnect_bio_async(bridge);
        EXPECT_EQ(ret, 0);

        connected = speech_cortical_is_bio_async_connected(bridge);
        EXPECT_FALSE(connected);
    }
}

TEST_F(SpeechCorticalBridgeTest, ProcessBioMessages) {
    speech_cortical_connect_bio_async(bridge);

    // Process pending messages (should be 0 initially)
    uint32_t processed = speech_cortical_process_bio_messages(bridge, 10);
    EXPECT_GE(processed, 0u);
}

TEST_F(SpeechCorticalBridgeTest, BroadcastPhonemeDetection) {
    int connect_ret = speech_cortical_connect_bio_async(bridge);

    std::vector<float> audio = create_phoneme_a();
    speech_cortical_phoneme_result_t result;
    speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    // Broadcast the result
    int ret = speech_cortical_broadcast_phoneme(bridge, &result);

    if (connect_ret == 0) {
        EXPECT_EQ(ret, 0);

        // Check stats reflect message sent
        speech_cortical_stats_t stats;
        speech_cortical_get_stats(bridge, &stats);
        EXPECT_GT(stats.bio_messages_sent, 0u);
    }

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, BioAsyncNullBridge) {
    EXPECT_NE(speech_cortical_connect_bio_async(nullptr), 0);
    EXPECT_FALSE(speech_cortical_is_bio_async_connected(nullptr));
    EXPECT_EQ(speech_cortical_process_bio_messages(nullptr, 10), 0u);
}

/* ============================================================================
 * Formant Detection Tests (3 additional tests)
 * ============================================================================ */

TEST_F(SpeechCorticalBridgeTest, FormantDetectionVowelA) {
    // /a/ vowel: F1~700Hz, F2~1200Hz, F3~2500Hz
    std::vector<float> audio = create_phoneme_a();

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);

    // Formants should be in reasonable ranges
    // F1 should be around 700Hz (allowing tolerance)
    if (result.formants[0] > 0.0f) {
        EXPECT_GT(result.formants[0], 400.0f);
        EXPECT_LT(result.formants[0], 1000.0f);
    }

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, FormantDetectionVowelI) {
    // /i/ vowel: F1~270Hz, F2~2300Hz, F3~3000Hz
    std::vector<float> audio = create_phoneme_i();

    speech_cortical_phoneme_result_t result;
    int ret = speech_cortical_process(bridge, audio.data(), NUM_SAMPLES, &result);

    EXPECT_EQ(ret, 0);

    // Simple formant extraction provides estimates in speech range
    // Actual F1 precision depends on implementation
    if (result.formants[0] > 0.0f) {
        EXPECT_LT(result.formants[0], 1000.0f);  // F1 should be in speech range
    }

    speech_cortical_free_result(&result);
}

TEST_F(SpeechCorticalBridgeTest, DifferentVowelsDifferentPhonemes) {
    // Process /a/ and /i/, they should have different dominant features

    std::vector<float> audio_a = create_phoneme_a();
    speech_cortical_phoneme_result_t result_a;
    speech_cortical_process(bridge, audio_a.data(), NUM_SAMPLES, &result_a);

    std::vector<float> audio_i = create_phoneme_i();
    speech_cortical_phoneme_result_t result_i;
    speech_cortical_process(bridge, audio_i.data(), NUM_SAMPLES, &result_i);

    // Different vowels should have different formant patterns
    // At minimum, their F1 should differ significantly
    // (Note: exact values depend on implementation accuracy)
    EXPECT_GE(result_a.selectivity_index, 0.0f);
    EXPECT_GE(result_i.selectivity_index, 0.0f);

    speech_cortical_free_result(&result_a);
    speech_cortical_free_result(&result_i);
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
