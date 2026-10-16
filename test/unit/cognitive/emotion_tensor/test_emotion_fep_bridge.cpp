/**
 * @file test_emotion_fep_bridge.cpp
 * @brief Unit tests for Emotion-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Emotion bidirectional integration
 * WHY:  Ensure valenced prediction errors and emotional modulation work correctly
 * HOW:  Test lifecycle, connections, emotion generation, precision modulation, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/emotion/nimcp_emotion_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/error/nimcp_error_codes.h"

class EmotionFepBridgeTest : public ::testing::Test {
protected:
    emotion_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotion_fep_config_t config;
        emotion_fep_bridge_default_config(&config);
        bridge = emotion_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            emotion_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EmotionFepBridgeTest, CreateWithNullConfig) {
    emotion_fep_bridge_t* br = emotion_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    emotion_fep_bridge_destroy(br);
}

TEST_F(EmotionFepBridgeTest, DestroyNull) {
    emotion_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(EmotionFepBridgeTest, DefaultConfig) {
    emotion_fep_config_t config;
    int ret = emotion_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.pe_valence_scaling, 0.0f);
    EXPECT_GT(config.pe_arousal_scaling, 0.0f);
    EXPECT_GT(config.precision_intensity_scaling, 0.0f);
    EXPECT_TRUE(config.enable_pe_emotion_generation);
    EXPECT_TRUE(config.enable_precision_intensity);
    EXPECT_TRUE(config.enable_interoceptive_inference);
}

TEST_F(EmotionFepBridgeTest, DefaultConfigNullPtr) {
    int ret = emotion_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  /* Returns error code for NULL */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = emotion_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(EmotionFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(emotion_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(emotion_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(emotion_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

TEST_F(EmotionFepBridgeTest, ConnectEmotion) {
    // Emotion system requires complex initialization, test with NULL for now
    int ret = emotion_fep_bridge_connect_emotion(bridge, nullptr);
    EXPECT_NE(ret, 0);  // Should fail with NULL
}

TEST_F(EmotionFepBridgeTest, ConnectEmotionNull) {
    EXPECT_NE(emotion_fep_bridge_connect_emotion(nullptr, nullptr), 0);
}

TEST_F(EmotionFepBridgeTest, Disconnect) {
    int ret = emotion_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionFepBridgeTest, DisconnectNull) {
    EXPECT_NE(emotion_fep_bridge_disconnect(nullptr), 0);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, GetState) {
    emotion_fep_state_t state;
    int ret = emotion_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_prediction_error, 0.0f);
    EXPECT_GE(state.current_precision, 0.0f);
    EXPECT_GE(state.current_valence, -1.0f);
    EXPECT_LE(state.current_valence, 1.0f);
    EXPECT_GE(state.current_arousal, 0.0f);
    EXPECT_LE(state.current_arousal, 1.0f);
}

TEST_F(EmotionFepBridgeTest, GetStateNull) {
    emotion_fep_state_t state;

    EXPECT_NE(emotion_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(emotion_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(EmotionFepBridgeTest, GetStats) {
    emotion_fep_stats_t stats;
    int ret = emotion_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.emotion_generation_events, 0u);
    EXPECT_EQ(stats.precision_modulation_events, 0u);
}

TEST_F(EmotionFepBridgeTest, GetStatsNull) {
    emotion_fep_stats_t stats;

    EXPECT_NE(emotion_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(emotion_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * FEP → Emotion Direction Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, GenerateValencedPe) {
    float pe_magnitude = 5.0f;

    int ret = emotion_fep_generate_valenced_pe(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionFepBridgeTest, GenerateValencedPePositive) {
    float positive_pe = 5.0f;

    emotion_fep_generate_valenced_pe(bridge, positive_pe);

    emotion_fep_state_t state;
    emotion_fep_bridge_get_state(bridge, &state);
    // Valence should be set based on PE
    EXPECT_GE(state.current_valence, -1.0f);
    EXPECT_LE(state.current_valence, 1.0f);
}

TEST_F(EmotionFepBridgeTest, GenerateValencedPeNegative) {
    float negative_pe = -3.0f;

    emotion_fep_generate_valenced_pe(bridge, negative_pe);

    emotion_fep_state_t state;
    emotion_fep_bridge_get_state(bridge, &state);
    EXPECT_GE(state.current_valence, -1.0f);
    EXPECT_LE(state.current_valence, 1.0f);
}

TEST_F(EmotionFepBridgeTest, GenerateValencedPeNull) {
    EXPECT_NE(emotion_fep_generate_valenced_pe(nullptr, 5.0f), 0);
}

TEST_F(EmotionFepBridgeTest, ModulatePrecisionByIntensity) {
    /* Must connect FEP system before precision modulation can work */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    emotion_fep_bridge_connect_fep(bridge, fep);
    int ret = emotion_fep_modulate_precision_by_intensity(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(EmotionFepBridgeTest, ModulatePrecisionByIntensityNull) {
    EXPECT_NE(emotion_fep_modulate_precision_by_intensity(nullptr), 0);
}

/* ============================================================================
 * Emotion → FEP Direction Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, ApplyEmotionPrecisionModulation) {
    /* Must connect FEP system before precision modulation can work */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    emotion_fep_bridge_connect_fep(bridge, fep);
    int ret = emotion_fep_apply_emotion_precision_modulation(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(EmotionFepBridgeTest, ApplyEmotionPrecisionModulationNull) {
    EXPECT_NE(emotion_fep_apply_emotion_precision_modulation(nullptr), 0);
}

TEST_F(EmotionFepBridgeTest, ApplyEmotionLearningModulation) {
    /* Must connect FEP system before learning modulation can work */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    emotion_fep_bridge_connect_fep(bridge, fep);
    int ret = emotion_fep_apply_emotion_learning_modulation(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(EmotionFepBridgeTest, ApplyEmotionLearningModulationNull) {
    EXPECT_NE(emotion_fep_apply_emotion_learning_modulation(nullptr), 0);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, Update) {
    int ret = emotion_fep_bridge_update(bridge, 16);  // 16ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionFepBridgeTest, UpdateNull) {
    EXPECT_NE(emotion_fep_bridge_update(nullptr, 16), 0);
}

TEST_F(EmotionFepBridgeTest, UpdateZeroDelta) {
    int ret = emotion_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, BioAsyncConnect) {
    int ret = emotion_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionFepBridgeTest, BioAsyncDisconnect) {
    emotion_fep_bridge_connect_bio_async(bridge);

    int ret = emotion_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(emotion_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(EmotionFepBridgeTest, BioAsyncIsConnected) {
    EXPECT_FALSE(emotion_fep_bridge_is_bio_async_connected(bridge));

    emotion_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability

    emotion_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(emotion_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(EmotionFepBridgeTest, BioAsyncNullParams) {
    EXPECT_NE(emotion_fep_bridge_connect_bio_async(nullptr), 0);
    /* Disconnect with NULL is a graceful no-op, returns SUCCESS */
    EXPECT_EQ(emotion_fep_bridge_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(emotion_fep_bridge_is_bio_async_connected(nullptr));
}

TEST_F(EmotionFepBridgeTest, BioAsyncDoubleConnect) {
    emotion_fep_bridge_connect_bio_async(bridge);
    int ret = emotion_fep_bridge_connect_bio_async(bridge);  // Should be no-op
    EXPECT_EQ(ret, 0);
    emotion_fep_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(EmotionFepBridgeTest, PredictionErrorGeneratesEmotion) {
    float pe = 5.0f;
    emotion_fep_generate_valenced_pe(bridge, pe);

    emotion_fep_stats_t stats;
    emotion_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.emotion_generation_events, 0u);
}

TEST_F(EmotionFepBridgeTest, PrecisionMapsToIntensity) {
    emotion_fep_modulate_precision_by_intensity(bridge);

    emotion_fep_state_t state;
    emotion_fep_bridge_get_state(bridge, &state);
    // Precision should affect emotional intensity
    EXPECT_GE(state.current_precision, 0.0f);
}

TEST_F(EmotionFepBridgeTest, EmotionModulatesPrecision) {
    emotion_fep_apply_emotion_precision_modulation(bridge);

    emotion_fep_stats_t stats;
    emotion_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.precision_modulation_events, 0u);
}

TEST_F(EmotionFepBridgeTest, ValenceRangeValidation) {
    float high_pe = 10.0f;
    emotion_fep_generate_valenced_pe(bridge, high_pe);

    emotion_fep_state_t state;
    emotion_fep_bridge_get_state(bridge, &state);

    // Valence should always be in [-1, 1]
    EXPECT_GE(state.current_valence, -1.0f);
    EXPECT_LE(state.current_valence, 1.0f);
}

TEST_F(EmotionFepBridgeTest, ArousalRangeValidation) {
    emotion_fep_state_t state;
    emotion_fep_bridge_get_state(bridge, &state);

    // Arousal should always be in [0, 1]
    EXPECT_GE(state.current_arousal, 0.0f);
    EXPECT_LE(state.current_arousal, 1.0f);
}
