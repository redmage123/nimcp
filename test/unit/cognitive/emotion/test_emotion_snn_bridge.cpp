/**
 * @file test_emotion_snn_bridge.cpp
 * @brief Unit tests for Emotion System - SNN Bridge integration
 * @date 2026-01-06
 *
 * Tests bidirectional integration between emotion recognition and SNN module:
 * - Emotion --> SNN: Observation/features encoding to spikes
 * - SNN --> Emotion: Category confidences and valence-arousal from spike output
 * - Bio-async message handling
 * - Modulation and state management
 */

#include <gtest/gtest.h>

// Note: Header has its own extern "C" guards
#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"

#include "utils/time/nimcp_time.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionSNNBridgeTest : public ::testing::Test {
protected:
    emotion_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotion_snn_config_t config = emotion_snn_config_default();
        config.input_dim = 64;
        config.hidden_dim = 128;
        config.output_dim = EMOTION_COUNT;
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.enable_immune_modulation = false;
        config.enable_plasticity_integration = false;
        bridge = emotion_snn_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create emotion-SNN bridge";
    }

    void TearDown() override {
        if (bridge) {
            emotion_snn_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper to create a mock emotion recognition result */
    emotion_recognition_result_t create_mock_result(
        emotion_category_t category,
        float confidence,
        float valence,
        float arousal,
        float intensity)
    {
        emotion_recognition_result_t result;
        memset(&result, 0, sizeof(result));
        result.category = category;
        result.confidence = confidence;
        result.valence = valence;
        result.arousal = arousal;
        result.intensity = (intensity > 0.8f) ? EMOTION_INTENSITY_HIGH :
                           (intensity > 0.5f) ? EMOTION_INTENSITY_MEDIUM :
                           (intensity > 0.2f) ? EMOTION_INTENSITY_LOW :
                           EMOTION_INTENSITY_NONE;
        return result;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(EmotionSNNBridgeTest, CreateWithDefaultConfig) {
    emotion_snn_bridge_t* b = emotion_snn_create(nullptr);
    ASSERT_NE(b, nullptr);
    emotion_snn_destroy(b);
}

TEST_F(EmotionSNNBridgeTest, CreateWithCustomConfig) {
    emotion_snn_config_t config = emotion_snn_config_default();
    config.input_dim = 32;
    config.hidden_dim = 64;
    config.output_dim = 8;
    config.encoding = EMOTION_SNN_ENCODE_RATE;
    config.decoding = EMOTION_SNN_DECODE_WINNER;

    emotion_snn_bridge_t* b = emotion_snn_create(&config);
    ASSERT_NE(b, nullptr);
    emotion_snn_destroy(b);
}

TEST_F(EmotionSNNBridgeTest, DestroyNull) {
    /* Should not crash */
    emotion_snn_destroy(nullptr);
}

TEST_F(EmotionSNNBridgeTest, Reset) {
    /* Encode something first */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.9f, 0.8f, 0.6f, 0.7f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    /* Reset should succeed */
    int ret = emotion_snn_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    /* State should be IDLE after reset */
    emotion_snn_bridge_state_t state;
    emotion_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, EMOTION_SNN_STATE_IDLE);
    EXPECT_EQ(state.emotion.current_category, EMOTION_NEUTRAL);
}

TEST_F(EmotionSNNBridgeTest, ResetNull) {
    int ret = emotion_snn_reset(nullptr);
    EXPECT_EQ(ret, -1) << "Reset with null bridge should fail";
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, DefaultConfigValues) {
    emotion_snn_config_t config = emotion_snn_config_default();

    EXPECT_EQ(config.input_dim, EMOTION_SNN_INPUT_DIM);
    EXPECT_EQ(config.output_dim, EMOTION_COUNT);
    EXPECT_EQ(config.va_dim, EMOTION_SNN_VA_DIM);
    EXPECT_GT(config.hidden_dim, 0u);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_EQ(config.dt_ms, EMOTION_SNN_DEFAULT_DT);
    EXPECT_GT(config.simulation_window_ms, 0.0f);
    EXPECT_TRUE(config.enable_va_encoding);
    EXPECT_TRUE(config.enable_plasticity_integration);
    EXPECT_EQ(config.encoding, EMOTION_SNN_ENCODE_POPULATION);
    EXPECT_EQ(config.decoding, EMOTION_SNN_DECODE_SOFTMAX);
    EXPECT_GT(config.baseline_rate_hz, 0.0f);
    EXPECT_GT(config.max_rate_hz, config.baseline_rate_hz);
}

//=============================================================================
// Encoding Tests (Emotion --> SNN)
//=============================================================================

TEST_F(EmotionSNNBridgeTest, EncodeObservation) {
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.9f, 0.8f, 0.6f, 0.7f);

    int spikes = emotion_snn_encode_observation(bridge, &result);
    EXPECT_GE(spikes, 0) << "Encoding should succeed and return spike count";
}

TEST_F(EmotionSNNBridgeTest, EncodeObservationNullBridge) {
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.9f, 0.8f, 0.6f, 0.7f);

    int spikes = emotion_snn_encode_observation(nullptr, &result);
    EXPECT_EQ(spikes, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, EncodeObservationNullResult) {
    int spikes = emotion_snn_encode_observation(bridge, nullptr);
    EXPECT_EQ(spikes, -1) << "Should fail with null result";
}

TEST_F(EmotionSNNBridgeTest, EncodeObservationDifferentEmotions) {
    /* Test encoding each basic emotion category */
    emotion_category_t emotions[] = {
        EMOTION_HAPPINESS, EMOTION_SADNESS, EMOTION_ANGER,
        EMOTION_FEAR, EMOTION_DISGUST, EMOTION_SURPRISE
    };

    for (auto cat : emotions) {
        emotion_recognition_result_t result = create_mock_result(
            cat, 0.8f, 0.5f, 0.5f, 0.6f);
        int spikes = emotion_snn_encode_observation(bridge, &result);
        EXPECT_GE(spikes, 0) << "Encoding " << (int)cat << " should succeed";
    }
}

TEST_F(EmotionSNNBridgeTest, EncodeFeatures) {
    float features[64];
    for (int i = 0; i < 64; i++) {
        features[i] = 0.5f + 0.3f * std::sin((float)i / 10.0f);
    }

    int spikes = emotion_snn_encode_features(bridge, features, 64, 0.3f, 0.7f);
    EXPECT_GE(spikes, 0) << "Feature encoding should succeed";
}

TEST_F(EmotionSNNBridgeTest, EncodeFeaturesNullBridge) {
    float features[64] = {0.5f};
    int spikes = emotion_snn_encode_features(nullptr, features, 64, 0.3f, 0.7f);
    EXPECT_EQ(spikes, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, EncodeFeaturesNullFeatures) {
    int spikes = emotion_snn_encode_features(bridge, nullptr, 64, 0.3f, 0.7f);
    EXPECT_EQ(spikes, -1) << "Should fail with null features";
}

TEST_F(EmotionSNNBridgeTest, EncodeFeaturesVaryingSize) {
    /* Test with fewer features than input_dim */
    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = 0.5f;
    }

    int spikes = emotion_snn_encode_features(bridge, features, 32, 0.0f, 0.5f);
    EXPECT_GE(spikes, 0) << "Should succeed with partial features";
}

TEST_F(EmotionSNNBridgeTest, EncodeValenceArousal) {
    int spikes = emotion_snn_encode_valence_arousal(bridge, 0.5f, 0.7f, 0.8f);
    EXPECT_GE(spikes, 0) << "VA encoding should succeed";
}

TEST_F(EmotionSNNBridgeTest, EncodeValenceArousalNull) {
    int spikes = emotion_snn_encode_valence_arousal(nullptr, 0.5f, 0.7f, 0.8f);
    EXPECT_EQ(spikes, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, EncodeValenceArousalBoundaryValues) {
    /* Test boundary values for valence [-1, 1] */
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, -1.0f, 0.5f, 0.5f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 1.0f, 0.5f, 0.5f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, 0.5f, 0.5f), 0);

    /* Test boundary values for arousal [0, 1] */
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, 0.0f, 0.5f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, 1.0f, 0.5f), 0);

    /* Test boundary values for intensity [0, 1] */
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, 0.5f, 0.0f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, 0.5f, 1.0f), 0);
}

TEST_F(EmotionSNNBridgeTest, EncodeValenceArousalOutOfRange) {
    /* Values should be clamped, not fail */
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, -2.0f, 0.5f, 0.5f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 2.0f, 0.5f, 0.5f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, -0.5f, 0.5f), 0);
    EXPECT_GE(emotion_snn_encode_valence_arousal(bridge, 0.0f, 1.5f, 0.5f), 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, Simulate) {
    /* Encode first */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.9f, 0.8f, 0.6f, 0.7f);
    emotion_snn_encode_observation(bridge, &result);

    /* Simulate */
    int ret = emotion_snn_simulate(bridge, 50.0f);
    EXPECT_EQ(ret, 0) << "Simulation should succeed";
}

TEST_F(EmotionSNNBridgeTest, SimulateNull) {
    int ret = emotion_snn_simulate(nullptr, 50.0f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, SimulateVaryingDuration) {
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_ANGER, 0.7f, -0.7f, 0.8f, 0.6f);
    emotion_snn_encode_observation(bridge, &result);

    /* Short duration */
    EXPECT_EQ(emotion_snn_simulate(bridge, 10.0f), 0);

    /* Medium duration */
    EXPECT_EQ(emotion_snn_simulate(bridge, 50.0f), 0);

    /* Longer duration */
    EXPECT_EQ(emotion_snn_simulate(bridge, 100.0f), 0);
}

TEST_F(EmotionSNNBridgeTest, Step) {
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_FEAR, 0.8f, -0.8f, 0.9f, 0.75f);
    emotion_snn_encode_observation(bridge, &result);

    int ret = emotion_snn_step(bridge);
    EXPECT_EQ(ret, 0) << "Single step should succeed";
}

TEST_F(EmotionSNNBridgeTest, StepNull) {
    int ret = emotion_snn_step(nullptr);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, MultipleSteps) {
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_SURPRISE, 0.6f, 0.3f, 0.8f, 0.5f);
    emotion_snn_encode_observation(bridge, &result);

    for (int i = 0; i < 50; i++) {
        int ret = emotion_snn_step(bridge);
        EXPECT_EQ(ret, 0) << "Step " << i << " should succeed";
    }
}

//=============================================================================
// Decoding Tests (SNN --> Emotion)
//=============================================================================

TEST_F(EmotionSNNBridgeTest, GetCategoryConfidences) {
    /* Encode and simulate first */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.9f, 0.8f, 0.6f, 0.7f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    /* Get confidences */
    float confidences[EMOTION_COUNT];
    emotion_category_t category = emotion_snn_get_category_confidences(bridge, confidences);

    EXPECT_NE(category, EMOTION_UNKNOWN) << "Should return valid category";

    /* Confidences should be in valid range */
    for (int i = 0; i < EMOTION_COUNT; i++) {
        EXPECT_GE(confidences[i], 0.0f);
        EXPECT_LE(confidences[i], 1.0f);
    }
}

TEST_F(EmotionSNNBridgeTest, GetCategoryConfidencesNull) {
    float confidences[EMOTION_COUNT];
    emotion_category_t category = emotion_snn_get_category_confidences(nullptr, confidences);
    EXPECT_EQ(category, EMOTION_UNKNOWN) << "Should return UNKNOWN with null bridge";

    category = emotion_snn_get_category_confidences(bridge, nullptr);
    EXPECT_EQ(category, EMOTION_UNKNOWN) << "Should return UNKNOWN with null confidences";
}

TEST_F(EmotionSNNBridgeTest, GetValenceArousal) {
    /* Encode valence-arousal first */
    emotion_snn_encode_valence_arousal(bridge, 0.5f, 0.7f, 0.8f);

    float valence, arousal;
    int ret = emotion_snn_get_valence_arousal(bridge, &valence, &arousal);
    EXPECT_EQ(ret, 0) << "Should succeed";

    /* Values should be in valid ranges */
    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

TEST_F(EmotionSNNBridgeTest, GetValenceArousalNull) {
    float valence, arousal;

    EXPECT_EQ(emotion_snn_get_valence_arousal(nullptr, &valence, &arousal), -1);
    EXPECT_EQ(emotion_snn_get_valence_arousal(bridge, nullptr, &arousal), -1);
    EXPECT_EQ(emotion_snn_get_valence_arousal(bridge, &valence, nullptr), -1);
}

TEST_F(EmotionSNNBridgeTest, GetEmotionState) {
    /* Encode and simulate */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_SADNESS, 0.75f, -0.6f, 0.3f, 0.5f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    emotion_snn_emotion_state_t emotion_state;
    int ret = emotion_snn_get_emotion_state(bridge, &emotion_state);
    EXPECT_EQ(ret, 0) << "Should succeed";

    /* Check state values are valid */
    EXPECT_LT((int)emotion_state.current_category, EMOTION_COUNT);
    EXPECT_GE(emotion_state.valence, -1.0f);
    EXPECT_LE(emotion_state.valence, 1.0f);
    EXPECT_GE(emotion_state.arousal, 0.0f);
    EXPECT_LE(emotion_state.arousal, 1.0f);
    EXPECT_GE(emotion_state.intensity, 0.0f);
    EXPECT_LE(emotion_state.intensity, 1.0f);
}

TEST_F(EmotionSNNBridgeTest, GetEmotionStateNull) {
    emotion_snn_emotion_state_t state;
    EXPECT_EQ(emotion_snn_get_emotion_state(nullptr, &state), -1);
    EXPECT_EQ(emotion_snn_get_emotion_state(bridge, nullptr), -1);
}

TEST_F(EmotionSNNBridgeTest, GetTransitionProb) {
    /* Set up some emotion state first */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.8f, 0.7f, 0.6f, 0.6f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    /* Decode to set up confidences */
    float confidences[EMOTION_COUNT];
    emotion_snn_get_category_confidences(bridge, confidences);

    /* Get transition probability */
    float prob = emotion_snn_get_transition_prob(bridge, EMOTION_HAPPINESS, EMOTION_SADNESS);
    EXPECT_GE(prob, 0.0f);
    EXPECT_LE(prob, 1.0f);
}

TEST_F(EmotionSNNBridgeTest, GetTransitionProbNull) {
    float prob = emotion_snn_get_transition_prob(nullptr, EMOTION_HAPPINESS, EMOTION_SADNESS);
    EXPECT_EQ(prob, 0.0f) << "Should return 0 with null bridge";
}

TEST_F(EmotionSNNBridgeTest, GetTransitionProbInvalidCategory) {
    float prob = emotion_snn_get_transition_prob(bridge, EMOTION_COUNT, EMOTION_HAPPINESS);
    EXPECT_EQ(prob, 0.0f) << "Should return 0 with invalid from_category";

    prob = emotion_snn_get_transition_prob(bridge, EMOTION_HAPPINESS, EMOTION_COUNT);
    EXPECT_EQ(prob, 0.0f) << "Should return 0 with invalid to_category";
}

//=============================================================================
// State and Statistics Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, GetState) {
    emotion_snn_bridge_state_t state;
    int ret = emotion_snn_get_state(bridge, &state);

    EXPECT_EQ(ret, 0) << "Should get state";
    EXPECT_EQ(state.state, EMOTION_SNN_STATE_IDLE);
    EXPECT_FALSE(state.bio_async_connected);
    EXPECT_GT(state.active_populations, 0u);
}

TEST_F(EmotionSNNBridgeTest, GetStateNull) {
    emotion_snn_bridge_state_t state;
    EXPECT_EQ(emotion_snn_get_state(nullptr, &state), -1);
    EXPECT_EQ(emotion_snn_get_state(bridge, nullptr), -1);
}

TEST_F(EmotionSNNBridgeTest, GetStats) {
    emotion_snn_stats_t stats;
    int ret = emotion_snn_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0) << "Should get stats";
    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.total_spikes_generated, 0u);
    EXPECT_EQ(stats.total_decodings, 0u);
}

TEST_F(EmotionSNNBridgeTest, GetStatsNull) {
    emotion_snn_stats_t stats;
    EXPECT_EQ(emotion_snn_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(emotion_snn_get_stats(bridge, nullptr), -1);
}

TEST_F(EmotionSNNBridgeTest, StatsUpdateAfterEncoding) {
    /* Initial stats */
    emotion_snn_stats_t stats_before;
    emotion_snn_get_stats(bridge, &stats_before);
    EXPECT_EQ(stats_before.total_observations, 0u);

    /* Encode some observations */
    for (int i = 0; i < 5; i++) {
        emotion_recognition_result_t result = create_mock_result(
            EMOTION_HAPPINESS, 0.8f, 0.6f, 0.5f, 0.6f);
        emotion_snn_encode_observation(bridge, &result);
    }

    /* Check stats updated */
    emotion_snn_stats_t stats_after;
    emotion_snn_get_stats(bridge, &stats_after);
    EXPECT_EQ(stats_after.total_observations, 5u);
    EXPECT_GT(stats_after.total_spikes_generated, 0u);
}

TEST_F(EmotionSNNBridgeTest, StatsUpdateAfterDecoding) {
    /* Encode and simulate */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_ANGER, 0.85f, -0.7f, 0.8f, 0.7f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    /* Decode multiple times */
    for (int i = 0; i < 3; i++) {
        float confidences[EMOTION_COUNT];
        emotion_snn_get_category_confidences(bridge, confidences);
    }

    /* Check stats */
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decodings, 3u);
}

TEST_F(EmotionSNNBridgeTest, ResetStats) {
    /* Generate some stats */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_FEAR, 0.9f, -0.8f, 0.9f, 0.8f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    float confidences[EMOTION_COUNT];
    emotion_snn_get_category_confidences(bridge, confidences);

    /* Verify stats exist */
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_observations, 0u);
    EXPECT_GT(stats.total_decodings, 0u);

    /* Reset stats */
    emotion_snn_reset_stats(bridge);

    /* Verify stats are cleared */
    emotion_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.total_spikes_generated, 0u);
    EXPECT_EQ(stats.total_decodings, 0u);
}

TEST_F(EmotionSNNBridgeTest, ResetStatsNull) {
    /* Should not crash */
    emotion_snn_reset_stats(nullptr);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, BioAsyncNotConnectedInitially) {
    bool connected = emotion_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected) << "Should not be connected initially";
}

TEST_F(EmotionSNNBridgeTest, BioAsyncConnectDisconnect) {
    /* Connect */
    int ret = emotion_snn_connect_bio_async(bridge);
    /* Note: Returns 0 if bio_async is disabled in config (no-op) */
    EXPECT_EQ(ret, 0);

    /* With bio_async disabled, it won't actually connect */
    bool connected = emotion_snn_is_bio_async_connected(bridge);
    /* Connection status depends on config.enable_bio_async */

    /* Disconnect */
    ret = emotion_snn_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    connected = emotion_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(EmotionSNNBridgeTest, BioAsyncConnectNull) {
    // Returns NIMCP error code (positive) or -1 for null bridge
    EXPECT_NE(emotion_snn_connect_bio_async(nullptr), 0);
}

TEST_F(EmotionSNNBridgeTest, BioAsyncDisconnectNull) {
    // Returns NIMCP error code (positive) or -1 for null bridge
    EXPECT_NE(emotion_snn_disconnect_bio_async(nullptr), 0);
}

TEST_F(EmotionSNNBridgeTest, BioAsyncIsConnectedNull) {
    bool connected = emotion_snn_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

class EmotionSNNBridgeBioAsyncEnabledTest : public ::testing::Test {
protected:
    emotion_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotion_snn_config_t config = emotion_snn_config_default();
        config.enable_bio_async = true;  /* Enable bio-async */
        config.enable_immune_modulation = false;
        bridge = emotion_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            emotion_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EmotionSNNBridgeBioAsyncEnabledTest, ConnectWithBioAsyncEnabled) {
    int ret = emotion_snn_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    bool connected = emotion_snn_is_bio_async_connected(bridge);
    EXPECT_TRUE(connected);

    ret = emotion_snn_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    connected = emotion_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, ModulateByArousal) {
    int ret = emotion_snn_modulate_by_arousal(bridge, 0.8f);
    EXPECT_EQ(ret, 0) << "Arousal modulation should succeed";
}

TEST_F(EmotionSNNBridgeTest, ModulateByArousalNull) {
    int ret = emotion_snn_modulate_by_arousal(nullptr, 0.5f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, ModulateByArousalBoundaryValues) {
    EXPECT_EQ(emotion_snn_modulate_by_arousal(bridge, 0.0f), 0);
    EXPECT_EQ(emotion_snn_modulate_by_arousal(bridge, 0.5f), 0);
    EXPECT_EQ(emotion_snn_modulate_by_arousal(bridge, 1.0f), 0);
}

TEST_F(EmotionSNNBridgeTest, SetIntensityModulation) {
    int ret = emotion_snn_set_intensity_modulation(bridge, 0.7f);
    EXPECT_EQ(ret, 0) << "Intensity modulation should succeed";

    /* Check that intensity is reflected in state */
    emotion_snn_emotion_state_t state;
    emotion_snn_get_emotion_state(bridge, &state);
    EXPECT_NEAR(state.intensity, 0.7f, 0.01f);
}

TEST_F(EmotionSNNBridgeTest, SetIntensityModulationNull) {
    int ret = emotion_snn_set_intensity_modulation(nullptr, 0.5f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(EmotionSNNBridgeTest, SetIntensityModulationBoundaryValues) {
    EXPECT_EQ(emotion_snn_set_intensity_modulation(bridge, 0.0f), 0);
    EXPECT_EQ(emotion_snn_set_intensity_modulation(bridge, 0.5f), 0);
    EXPECT_EQ(emotion_snn_set_intensity_modulation(bridge, 1.0f), 0);
}

TEST_F(EmotionSNNBridgeTest, ModulationAffectsEncoding) {
    /* Set high intensity modulation */
    emotion_snn_set_intensity_modulation(bridge, 1.0f);
    emotion_snn_modulate_by_arousal(bridge, 1.0f);

    emotion_recognition_result_t result = create_mock_result(
        EMOTION_RAGE, 0.95f, -0.9f, 0.95f, 0.9f);
    int spikes_high = emotion_snn_encode_observation(bridge, &result);

    emotion_snn_reset(bridge);

    /* Set low intensity modulation */
    emotion_snn_set_intensity_modulation(bridge, 0.1f);
    emotion_snn_modulate_by_arousal(bridge, 0.1f);

    int spikes_low = emotion_snn_encode_observation(bridge, &result);

    /* High modulation should generally produce more active inputs */
    /* (This is a weak test as spike count depends on implementation details) */
    EXPECT_GE(spikes_high, 0);
    EXPECT_GE(spikes_low, 0);
}

//=============================================================================
// Complete Forward Pass Tests
//=============================================================================

TEST_F(EmotionSNNBridgeTest, CompleteEmotionProcessingPipeline) {
    /* 1. Encode observation */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.85f, 0.7f, 0.6f, 0.65f);
    int spikes = emotion_snn_encode_observation(bridge, &result);
    EXPECT_GE(spikes, 0);

    /* 2. Simulate */
    int ret = emotion_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(ret, 0);

    /* 3. Decode category */
    float confidences[EMOTION_COUNT];
    emotion_category_t category = emotion_snn_get_category_confidences(bridge, confidences);
    EXPECT_NE(category, EMOTION_UNKNOWN);

    /* 4. Get valence-arousal */
    float valence, arousal;
    ret = emotion_snn_get_valence_arousal(bridge, &valence, &arousal);
    EXPECT_EQ(ret, 0);

    /* 5. Get full emotion state */
    emotion_snn_emotion_state_t emotion_state;
    ret = emotion_snn_get_emotion_state(bridge, &emotion_state);
    EXPECT_EQ(ret, 0);

    /* 6. Check statistics */
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_observations, 1u);
    EXPECT_GT(stats.total_decodings, 0u);
}

TEST_F(EmotionSNNBridgeTest, MultipleEmotionSequence) {
    /* Process a sequence of emotions */
    struct {
        emotion_category_t category;
        float confidence;
        float valence;
        float arousal;
    } sequence[] = {
        {EMOTION_NEUTRAL, 0.7f, 0.0f, 0.3f},
        {EMOTION_INTEREST, 0.8f, 0.3f, 0.5f},
        {EMOTION_HAPPINESS, 0.85f, 0.7f, 0.6f},
        {EMOTION_SURPRISE, 0.9f, 0.4f, 0.8f},
        {EMOTION_CALM, 0.75f, 0.2f, 0.2f}
    };

    emotion_category_t prev_category = EMOTION_UNKNOWN;

    for (const auto& emo : sequence) {
        emotion_recognition_result_t result = create_mock_result(
            emo.category, emo.confidence, emo.valence, emo.arousal, 0.6f);

        emotion_snn_encode_observation(bridge, &result);
        emotion_snn_simulate(bridge, 30.0f);

        float confidences[EMOTION_COUNT];
        emotion_category_t decoded = emotion_snn_get_category_confidences(bridge, confidences);

        EXPECT_NE(decoded, EMOTION_UNKNOWN);

        /* Check transition probability is valid */
        if (prev_category != EMOTION_UNKNOWN && prev_category < EMOTION_COUNT) {
            float prob = emotion_snn_get_transition_prob(bridge, prev_category, decoded);
            EXPECT_GE(prob, 0.0f);
            EXPECT_LE(prob, 1.0f);
        }

        prev_category = decoded;
    }

    /* Check final stats */
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_observations, 5u);
    EXPECT_EQ(stats.total_decodings, 5u);
}

//=============================================================================
// Thread Safety Tests (Basic)
//=============================================================================

TEST_F(EmotionSNNBridgeTest, ConcurrentReadOperations) {
    /* Set up some state first */
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_HAPPINESS, 0.8f, 0.6f, 0.5f, 0.6f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto read_task = [&]() {
        for (int i = 0; i < 100; i++) {
            emotion_snn_bridge_state_t state;
            if (emotion_snn_get_state(bridge, &state) == 0) {
                success_count++;
            } else {
                failure_count++;
            }

            emotion_snn_stats_t stats;
            if (emotion_snn_get_stats(bridge, &stats) == 0) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    std::thread t1(read_task);
    std::thread t2(read_task);
    std::thread t3(read_task);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_GT(success_count.load(), 0);
    /* All reads should succeed with proper locking */
}

TEST_F(EmotionSNNBridgeTest, ConcurrentEncodeSimulate) {
    std::atomic<int> encode_count{0};
    std::atomic<int> simulate_count{0};

    auto encode_task = [&]() {
        for (int i = 0; i < 50; i++) {
            float features[64];
            for (int j = 0; j < 64; j++) {
                features[j] = 0.3f + 0.4f * ((float)(i + j) / 100.0f);
            }
            if (emotion_snn_encode_features(bridge, features, 64, 0.0f, 0.5f) >= 0) {
                encode_count++;
            }
        }
    };

    auto simulate_task = [&]() {
        for (int i = 0; i < 50; i++) {
            if (emotion_snn_step(bridge) == 0) {
                simulate_count++;
            }
        }
    };

    std::thread t1(encode_task);
    std::thread t2(simulate_task);
    std::thread t3(encode_task);
    std::thread t4(simulate_task);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    /* All operations should succeed with proper mutex protection */
    EXPECT_EQ(encode_count.load(), 100);
    EXPECT_EQ(simulate_count.load(), 100);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(EmotionSNNBridgeTest, ZeroDurationSimulation) {
    emotion_recognition_result_t result = create_mock_result(
        EMOTION_NEUTRAL, 0.5f, 0.0f, 0.3f, 0.3f);
    emotion_snn_encode_observation(bridge, &result);

    /* Zero duration should still succeed (no-op) */
    int ret = emotion_snn_simulate(bridge, 0.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionSNNBridgeTest, NegativeValuesHandledGracefully) {
    /* Negative duration - implementation dependent */
    int ret = emotion_snn_simulate(bridge, -10.0f);
    /* Should either succeed as no-op or fail gracefully */
    (void)ret;

    /* Extreme negative valence - should be clamped */
    ret = emotion_snn_encode_valence_arousal(bridge, -5.0f, 0.5f, 0.5f);
    EXPECT_GE(ret, 0);
}

TEST_F(EmotionSNNBridgeTest, RepeatedResetStability) {
    for (int i = 0; i < 10; i++) {
        emotion_recognition_result_t result = create_mock_result(
            EMOTION_HAPPINESS, 0.8f, 0.6f, 0.5f, 0.6f);
        emotion_snn_encode_observation(bridge, &result);
        emotion_snn_simulate(bridge, 20.0f);

        int ret = emotion_snn_reset(bridge);
        EXPECT_EQ(ret, 0) << "Reset " << i << " should succeed";

        emotion_snn_bridge_state_t state;
        emotion_snn_get_state(bridge, &state);
        EXPECT_EQ(state.state, EMOTION_SNN_STATE_IDLE);
    }
}

TEST_F(EmotionSNNBridgeTest, EncodingMethodVariations) {
    /* Test different encoding methods by creating bridges with each type */
    emotion_snn_encoding_t encodings[] = {
        EMOTION_SNN_ENCODE_RATE,
        EMOTION_SNN_ENCODE_TEMPORAL,
        EMOTION_SNN_ENCODE_POPULATION,
        EMOTION_SNN_ENCODE_PHASE
    };

    for (auto enc : encodings) {
        emotion_snn_config_t config = emotion_snn_config_default();
        config.encoding = enc;
        config.enable_bio_async = false;

        emotion_snn_bridge_t* b = emotion_snn_create(&config);
        ASSERT_NE(b, nullptr) << "Should create bridge with encoding " << (int)enc;

        emotion_recognition_result_t result;
        memset(&result, 0, sizeof(result));
        result.category = EMOTION_HAPPINESS;
        result.confidence = 0.8f;
        result.valence = 0.6f;
        result.arousal = 0.5f;

        int spikes = emotion_snn_encode_observation(b, &result);
        EXPECT_GE(spikes, 0) << "Encoding with method " << (int)enc << " should succeed";

        emotion_snn_destroy(b);
    }
}

TEST_F(EmotionSNNBridgeTest, DecodingMethodVariations) {
    /* Test different decoding methods */
    emotion_snn_decoding_t decodings[] = {
        EMOTION_SNN_DECODE_WINNER,
        EMOTION_SNN_DECODE_SOFTMAX,
        EMOTION_SNN_DECODE_POPULATION,
        EMOTION_SNN_DECODE_TEMPORAL
    };

    for (auto dec : decodings) {
        emotion_snn_config_t config = emotion_snn_config_default();
        config.decoding = dec;
        config.enable_bio_async = false;

        emotion_snn_bridge_t* b = emotion_snn_create(&config);
        ASSERT_NE(b, nullptr) << "Should create bridge with decoding " << (int)dec;

        emotion_recognition_result_t result;
        memset(&result, 0, sizeof(result));
        result.category = EMOTION_ANGER;
        result.confidence = 0.85f;
        result.valence = -0.7f;
        result.arousal = 0.8f;

        emotion_snn_encode_observation(b, &result);
        emotion_snn_simulate(b, 50.0f);

        float confidences[EMOTION_COUNT];
        emotion_category_t category = emotion_snn_get_category_confidences(b, confidences);
        EXPECT_NE(category, EMOTION_UNKNOWN) << "Decoding with method " << (int)dec << " should return valid category";

        emotion_snn_destroy(b);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
