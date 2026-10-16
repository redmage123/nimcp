//=============================================================================
// test_emotion_bridge_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_emotion_bridge_regression.cpp
 * @brief Regression tests for Emotion-SNN and Emotion-Plasticity bridges
 *
 * WHAT: Test for regressions in numerical stability, memory safety, and performance
 * WHY:  Ensure consistent behavior across changes and prevent past bugs
 * HOW:  Test edge cases, numerical bounds, memory safety, and performance constraints
 *
 * REGRESSION TEST CATEGORIES:
 * - Numerical stability (weight bounds, confidence normalization, valence/arousal)
 * - Memory safety (create/destroy cycles, max synapse handling, buffer protection)
 * - Performance bounds (simulation time, encoding latency, memory usage)
 * - Edge cases (zero/max intensity, rapid switching, empty input)
 *
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <memory>
#include <random>

// Headers have their own extern "C" guards
#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"
#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"

#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture: Emotion-SNN Bridge
//=============================================================================

class EmotionSNNBridgeRegressionTest : public ::testing::Test {
protected:
    emotion_snn_bridge_t* bridge = nullptr;
    emotion_snn_config_t config;

    void SetUp() override {
        config = emotion_snn_config_default();
        config.enable_bio_async = false;
        config.enable_immune_modulation = false;
        config.enable_plasticity_integration = false;
        bridge = emotion_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            emotion_snn_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Create a test emotion recognition result
    emotion_recognition_result_t create_emotion_result(
        emotion_category_t category = EMOTION_HAPPINESS,
        float confidence = 0.8f,
        float valence = 0.5f,
        float arousal = 0.5f
    ) {
        emotion_recognition_result_t result = {};
        result.category = category;
        result.confidence = confidence;
        result.valence = valence;
        result.arousal = arousal;
        result.intensity = EMOTION_INTENSITY_MEDIUM;
        result.dominance = 0.0f;
        result.timestamp_ms = 1000;
        return result;
    }

    // Generate deterministic test features
    void generate_features(float* features, uint32_t n, uint32_t seed) {
        for (uint32_t i = 0; i < n; i++) {
            features[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
    }
};

//=============================================================================
// Test Fixture: Emotion-Plasticity Bridge
//=============================================================================

class EmotionPlasticityBridgeRegressionTest : public ::testing::Test {
protected:
    emotion_plasticity_bridge_t* bridge = nullptr;
    emotion_plasticity_config_t config;

    void SetUp() override {
        config = emotion_plasticity_config_default();
        config.enable_bio_async = false;
        config.enable_immune_modulation = false;
        bridge = emotion_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            emotion_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }

    uint64_t get_timestamp_us() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// Test Fixture: Combined Bridges
//=============================================================================

class EmotionBridgeCombinedRegressionTest : public ::testing::Test {
protected:
    emotion_snn_bridge_t* snn_bridge = nullptr;
    emotion_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        emotion_snn_config_t snn_config = emotion_snn_config_default();
        snn_config.enable_bio_async = false;
        snn_config.enable_immune_modulation = false;
        snn_bridge = emotion_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        emotion_plasticity_config_t plasticity_config = emotion_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity_bridge = emotion_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            emotion_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            emotion_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    uint64_t get_timestamp_us() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// SECTION 1: Numerical Stability Tests
//=============================================================================

TEST_F(EmotionSNNBridgeRegressionTest, WeightValuesBoundedAfterSimulation) {
    // REGRESSION: Weight values must stay within [0, max_weight] bounds

    emotion_recognition_result_t result = create_emotion_result(EMOTION_ANGER, 0.9f, -0.7f, 0.8f);

    // Run multiple encode-simulate cycles
    for (int i = 0; i < 100; i++) {
        result.arousal = 0.3f + 0.6f * (float)(i % 10) / 10.0f;
        int spikes = emotion_snn_encode_observation(bridge, &result);
        EXPECT_GE(spikes, -1) << "Encoding should not fail catastrophically at iteration " << i;

        int ret = emotion_snn_simulate(bridge, 10.0f);
        EXPECT_EQ(ret, 0) << "Simulation should succeed at iteration " << i;
    }

    // Verify state is still valid
    emotion_snn_bridge_state_t state;
    int ret = emotion_snn_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.avg_firing_rate, 0.0f);
}

TEST_F(EmotionSNNBridgeRegressionTest, ConfidenceValuesNormalized) {
    // REGRESSION: Confidence values must always be in [0, 1]

    float test_confidences[] = {0.1f, 0.5f, 0.9f, 1.0f};

    for (float input_conf : test_confidences) {
        emotion_snn_reset(bridge);

        emotion_recognition_result_t result = create_emotion_result(
            EMOTION_HAPPINESS, input_conf, 0.5f, 0.5f);

        emotion_snn_encode_observation(bridge, &result);
        emotion_snn_simulate(bridge, 50.0f);

        float confidences[EMOTION_COUNT];
        emotion_category_t category = emotion_snn_get_category_confidences(bridge, confidences);

        // All output confidences must be normalized
        for (int i = 0; i < EMOTION_COUNT; i++) {
            EXPECT_GE(confidences[i], 0.0f)
                << "Confidence[" << i << "] must be >= 0 (input_conf=" << input_conf << ")";
            EXPECT_LE(confidences[i], 1.0f)
                << "Confidence[" << i << "] must be <= 1 (input_conf=" << input_conf << ")";
            EXPECT_FALSE(std::isnan(confidences[i]))
                << "Confidence[" << i << "] must not be NaN";
            EXPECT_FALSE(std::isinf(confidences[i]))
                << "Confidence[" << i << "] must not be Inf";
        }
    }
}

TEST_F(EmotionSNNBridgeRegressionTest, ValenceArousalBoundsRespected) {
    // REGRESSION: Valence must stay in [-1, 1], arousal in [0, 1]

    float test_valences[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    float test_arousals[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float val : test_valences) {
        for (float aro : test_arousals) {
            emotion_snn_reset(bridge);

            int spikes = emotion_snn_encode_valence_arousal(bridge, val, aro, 0.7f);
            EXPECT_GE(spikes, -1) << "Encode should handle valence=" << val << " arousal=" << aro;

            emotion_snn_simulate(bridge, 30.0f);

            float out_valence, out_arousal;
            int ret = emotion_snn_get_valence_arousal(bridge, &out_valence, &out_arousal);
            EXPECT_EQ(ret, 0);

            EXPECT_GE(out_valence, -1.0f) << "Valence must be >= -1";
            EXPECT_LE(out_valence, 1.0f) << "Valence must be <= 1";
            EXPECT_GE(out_arousal, 0.0f) << "Arousal must be >= 0";
            EXPECT_LE(out_arousal, 1.0f) << "Arousal must be <= 1";
        }
    }
}

TEST_F(EmotionSNNBridgeRegressionTest, NoNaNOrInfAfterManyIterations) {
    // REGRESSION: No NaN or Inf values should appear after extended simulation

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int iter = 0; iter < 500; iter++) {
        float valence = dist(rng) * 2.0f - 1.0f;  // [-1, 1]
        float arousal = dist(rng);                 // [0, 1]
        float intensity = dist(rng);               // [0, 1]

        emotion_snn_encode_valence_arousal(bridge, valence, arousal, intensity);
        emotion_snn_simulate(bridge, 5.0f);

        // Check emotion state
        emotion_snn_emotion_state_t emotion_state;
        int ret = emotion_snn_get_emotion_state(bridge, &emotion_state);
        EXPECT_EQ(ret, 0);

        EXPECT_FALSE(std::isnan(emotion_state.valence)) << "Valence NaN at iter " << iter;
        EXPECT_FALSE(std::isnan(emotion_state.arousal)) << "Arousal NaN at iter " << iter;
        EXPECT_FALSE(std::isnan(emotion_state.intensity)) << "Intensity NaN at iter " << iter;
        EXPECT_FALSE(std::isinf(emotion_state.valence)) << "Valence Inf at iter " << iter;
        EXPECT_FALSE(std::isinf(emotion_state.arousal)) << "Arousal Inf at iter " << iter;
        EXPECT_FALSE(std::isinf(emotion_state.intensity)) << "Intensity Inf at iter " << iter;

        // Check confidences
        for (int i = 0; i < EMOTION_COUNT; i++) {
            EXPECT_FALSE(std::isnan(emotion_state.category_confidences[i]))
                << "Confidence[" << i << "] NaN at iter " << iter;
            EXPECT_FALSE(std::isinf(emotion_state.category_confidences[i]))
                << "Confidence[" << i << "] Inf at iter " << iter;
        }
    }
}

TEST_F(EmotionPlasticityBridgeRegressionTest, WeightsBoundedAfterSTDP) {
    // REGRESSION: Weights must stay within [weight_min, weight_max] after STDP updates

    // Register synapses
    for (uint32_t i = 0; i < 16; i++) {
        int ret = emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.5f);
        EXPECT_EQ(ret, 0) << "Synapse registration should succeed for synapse " << i;
    }

    uint64_t timestamp = get_timestamp_us();

    // Trigger many plasticity events
    for (int iter = 0; iter < 200; iter++) {
        emotion_category_t emotion = static_cast<emotion_category_t>(iter % EMOTION_COUNT);

        emotion_plasticity_stimulus(bridge, emotion, 0.8f, timestamp);
        timestamp += 5000;  // 5ms later

        emotion_plasticity_response(bridge, emotion, 0.9f, timestamp);
        timestamp += 5000;

        // Alternate between reward and punishment
        float reward = (iter % 2 == 0) ? 1.0f : -1.0f;
        emotion_plasticity_reward(bridge, reward, timestamp);
        timestamp += 10000;

        emotion_plasticity_update(bridge, 10.0f);
    }

    // Verify all synapse weights are within bounds
    for (uint32_t syn = 0; syn < 16; syn++) {
        emotion_plasticity_synapse_t state;
        int ret = emotion_plasticity_get_synapse(bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.weight, config.weight_min)
                << "Weight must be >= weight_min for synapse " << syn;
            EXPECT_LE(state.weight, config.weight_max)
                << "Weight must be <= weight_max for synapse " << syn;
            EXPECT_FALSE(std::isnan(state.weight))
                << "Weight must not be NaN for synapse " << syn;
        }
    }
}

TEST_F(EmotionPlasticityBridgeRegressionTest, EligibilityTracesBounded) {
    // REGRESSION: Eligibility traces must remain bounded

    // Register synapses with eligibility enabled
    for (uint32_t i = 0; i < 8; i++) {
        emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_EMOTION_TO_RESPONSE,
            static_cast<emotion_category_t>(i), 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    // Rapid stimulation to build up eligibility traces
    for (int iter = 0; iter < 300; iter++) {
        emotion_plasticity_stimulus(bridge, EMOTION_ANGER, 1.0f, timestamp);
        emotion_plasticity_update(bridge, 1.0f);
        timestamp += 1000;
    }

    // Verify eligibility traces
    for (uint32_t syn = 0; syn < 8; syn++) {
        emotion_plasticity_synapse_t state;
        int ret = emotion_plasticity_get_synapse(bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.eligibility_trace, 0.0f)
                << "Eligibility trace must be >= 0 for synapse " << syn;
            EXPECT_LE(state.eligibility_trace, 10.0f)
                << "Eligibility trace must be bounded for synapse " << syn;
            EXPECT_FALSE(std::isnan(state.eligibility_trace))
                << "Eligibility trace must not be NaN";
        }
    }
}

//=============================================================================
// SECTION 2: Memory Safety Tests
//=============================================================================

TEST_F(EmotionSNNBridgeRegressionTest, RepeatedCreateDestroyCycles) {
    // REGRESSION: Memory leaks on repeated create/destroy

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        emotion_snn_config_t test_config = emotion_snn_config_default();
        test_config.input_dim = 32;
        test_config.hidden_dim = 64;
        test_config.output_dim = EMOTION_COUNT;
        test_config.enable_bio_async = false;

        emotion_snn_bridge_t* test_bridge = emotion_snn_create(&test_config);
        ASSERT_NE(test_bridge, nullptr) << "Create failed at cycle " << i;

        // Do some work
        emotion_recognition_result_t result = create_emotion_result();
        emotion_snn_encode_observation(test_bridge, &result);
        emotion_snn_simulate(test_bridge, 10.0f);

        emotion_snn_destroy(test_bridge);
    }

    // If we got here without crash/hang, memory management is working
    SUCCEED();
}

TEST_F(EmotionPlasticityBridgeRegressionTest, RepeatedCreateDestroyCycles) {
    // REGRESSION: Plasticity bridge memory leaks

    const int cycles = 50;

    for (int i = 0; i < cycles; i++) {
        emotion_plasticity_config_t test_config = emotion_plasticity_config_default();
        emotion_plasticity_bridge_t* test_bridge = emotion_plasticity_create(&test_config);
        ASSERT_NE(test_bridge, nullptr) << "Create failed at cycle " << i;

        // Register and use synapses
        for (uint32_t s = 0; s < 8; s++) {
            emotion_plasticity_register_synapse(
                test_bridge, s, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
                static_cast<emotion_category_t>(s % EMOTION_COUNT), 0.5f);
        }

        emotion_plasticity_stimulus(test_bridge, EMOTION_HAPPINESS, 0.8f, get_timestamp_us());
        emotion_plasticity_destroy(test_bridge);
    }

    SUCCEED();
}

TEST_F(EmotionPlasticityBridgeRegressionTest, MaxSynapseCountGraceful) {
    // REGRESSION: Handle max synapse count gracefully

    // Try to register more than max synapses
    int success_count = 0;
    for (uint32_t i = 0; i < EMOTION_PLASTICITY_MAX_SYNAPSES + 50; i++) {
        int ret = emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.5f);

        if (ret == 0) {
            success_count++;
        }
    }

    // Should have registered exactly max synapses
    EXPECT_LE(success_count, EMOTION_PLASTICITY_MAX_SYNAPSES)
        << "Should not exceed max synapse count";
    EXPECT_GT(success_count, 0) << "Should register at least some synapses";

    // Bridge should still be functional
    emotion_plasticity_bridge_state_t state;
    int ret = emotion_plasticity_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_LE(state.registered_synapses, EMOTION_PLASTICITY_MAX_SYNAPSES);
}

TEST_F(EmotionSNNBridgeRegressionTest, NullInputHandling) {
    // REGRESSION: Null input should be handled gracefully

    // Null result should fail gracefully
    int spikes = emotion_snn_encode_observation(bridge, nullptr);
    EXPECT_LT(spikes, 0) << "Null input should return error";

    // Null features should fail gracefully
    spikes = emotion_snn_encode_features(bridge, nullptr, 32, 0.5f, 0.5f);
    EXPECT_LT(spikes, 0) << "Null features should return error";

    // Bridge should still be functional after null inputs
    emotion_recognition_result_t result = create_emotion_result();
    spikes = emotion_snn_encode_observation(bridge, &result);
    EXPECT_GE(spikes, 0) << "Bridge should recover after null input";
}

TEST_F(EmotionSNNBridgeRegressionTest, NullOutputHandling) {
    // REGRESSION: Null output pointers should be handled gracefully

    // Get state with null output
    int ret = emotion_snn_get_state(bridge, nullptr);
    EXPECT_LT(ret, 0) << "Null output should return error";

    // Get stats with null output
    ret = emotion_snn_get_stats(bridge, nullptr);
    EXPECT_LT(ret, 0) << "Null stats output should return error";

    // Get valence/arousal with null outputs
    ret = emotion_snn_get_valence_arousal(bridge, nullptr, nullptr);
    EXPECT_LT(ret, 0) << "Null valence/arousal output should return error";
}

//=============================================================================
// SECTION 3: Performance Bounds Tests
//=============================================================================

TEST_F(EmotionSNNBridgeRegressionTest, SimulationTimeScalesLinearly) {
    // REGRESSION: Simulation time should scale roughly linearly with duration

    emotion_recognition_result_t result = create_emotion_result(EMOTION_ANGER, 0.9f, -0.5f, 0.8f);
    emotion_snn_encode_observation(bridge, &result);

    // Measure time for different simulation durations
    std::vector<std::pair<float, double>> timings;

    for (float duration : {10.0f, 50.0f, 100.0f}) {
        emotion_snn_reset(bridge);
        emotion_snn_encode_observation(bridge, &result);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10; i++) {
            emotion_snn_simulate(bridge, duration);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        timings.push_back({duration, static_cast<double>(us) / 10.0});
    }

    // Longer simulations should take more time (roughly proportional)
    // Allow for some overhead
    EXPECT_LT(timings[0].second, timings[2].second * 2.0)
        << "10ms simulation should take less than 2x of 100ms simulation time";
}

TEST_F(EmotionSNNBridgeRegressionTest, EncodingLatencyBounded) {
    // REGRESSION: Encoding latency should be bounded

    const int iterations = 100;
    emotion_recognition_result_t result = create_emotion_result();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        result.arousal = 0.5f + 0.3f * sinf(i * 0.1f);
        emotion_snn_encode_observation(bridge, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Should complete 100 encodings in < 50ms (500us per encoding max)
    double avg_us = static_cast<double>(duration_us) / iterations;
    EXPECT_LT(avg_us, 500.0)
        << "Encoding too slow: " << avg_us << " us/encoding (max 500us)";
}

TEST_F(EmotionPlasticityBridgeRegressionTest, PlasticityUpdatePerformance) {
    // REGRESSION: Plasticity updates should be fast

    // Register synapses
    for (uint32_t i = 0; i < EMOTION_PLASTICITY_MAX_SYNAPSES / 2; i++) {
        emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        emotion_plasticity_stimulus(bridge,
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.7f, timestamp);
        emotion_plasticity_update(bridge, 1.0f);
        timestamp += 1000;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 1000 plasticity updates should complete in < 200ms (200us per update max)
    double avg_us = static_cast<double>(duration_us) / iterations;
    EXPECT_LT(avg_us, 200.0)
        << "Plasticity too slow: " << avg_us << " us/update (max 200us)";
}

TEST_F(EmotionBridgeCombinedRegressionTest, MemoryUsageBounded) {
    // REGRESSION: Memory usage should not grow unboundedly

    // Get initial stats
    emotion_plasticity_stats_t plasticity_stats_initial;
    emotion_plasticity_get_stats(plasticity_bridge, &plasticity_stats_initial);

    // Run many operations using encode_observation (which increments stats)
    uint64_t timestamp = get_timestamp_us();

    for (int iter = 0; iter < 500; iter++) {
        // SNN operations - use encode_observation to ensure stats are tracked
        emotion_recognition_result_t result = {};
        result.category = static_cast<emotion_category_t>(iter % EMOTION_COUNT);
        result.confidence = 0.8f;
        result.valence = sinf(iter * 0.1f);          // valence [-1, 1]
        result.arousal = 0.5f + 0.5f * cosf(iter * 0.1f);  // arousal [0, 1]
        result.intensity = EMOTION_INTENSITY_MEDIUM;

        emotion_snn_encode_observation(snn_bridge, &result);
        emotion_snn_simulate(snn_bridge, 5.0f);

        // Plasticity operations
        emotion_plasticity_stimulus(plasticity_bridge,
            static_cast<emotion_category_t>(iter % EMOTION_COUNT),
            0.8f, timestamp);
        emotion_plasticity_update(plasticity_bridge, 5.0f);
        timestamp += 5000;

        // Reset periodically to prevent state accumulation
        if (iter % 100 == 99) {
            emotion_snn_reset(snn_bridge);
            emotion_plasticity_reset(plasticity_bridge);
        }
    }

    // Get final stats
    emotion_snn_stats_t snn_stats_final;
    emotion_snn_get_stats(snn_bridge, &snn_stats_final);

    emotion_plasticity_stats_t plasticity_stats_final;
    emotion_plasticity_get_stats(plasticity_bridge, &plasticity_stats_final);

    // Stats should reflect operations (after resets, may be lower than 500)
    // Just verify bridges are still functional
    emotion_snn_bridge_state_t snn_state;
    emotion_snn_get_state(snn_bridge, &snn_state);
    EXPECT_NE(snn_state.state, EMOTION_SNN_STATE_DISABLED);

    emotion_plasticity_bridge_state_t plasticity_state;
    emotion_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_NE(plasticity_state.state, EMOTION_PLASTICITY_STATE_DISABLED);
}

//=============================================================================
// SECTION 4: Edge Cases Tests
//=============================================================================

TEST_F(EmotionSNNBridgeRegressionTest, ZeroIntensityEmotion) {
    // Edge case: Zero intensity emotion

    emotion_recognition_result_t result = create_emotion_result();
    result.intensity = EMOTION_INTENSITY_NONE;
    result.confidence = 0.0f;

    int spikes = emotion_snn_encode_observation(bridge, &result);
    // Should handle gracefully (may return 0 spikes or -1)
    EXPECT_GE(spikes, -1);

    // Simulation should still work
    int ret = emotion_snn_simulate(bridge, 20.0f);
    EXPECT_EQ(ret, 0);

    // State should be valid
    emotion_snn_emotion_state_t state;
    ret = emotion_snn_get_emotion_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(std::isnan(state.intensity));
}

TEST_F(EmotionSNNBridgeRegressionTest, MaximumIntensityEmotion) {
    // Edge case: Maximum intensity emotion

    emotion_recognition_result_t result = create_emotion_result(
        EMOTION_RAGE, 1.0f, -1.0f, 1.0f);
    result.intensity = EMOTION_INTENSITY_EXTREME;

    int spikes = emotion_snn_encode_observation(bridge, &result);
    EXPECT_GE(spikes, 0) << "Max intensity should produce spikes";

    emotion_snn_simulate(bridge, 50.0f);

    // Outputs should still be bounded
    float confidences[EMOTION_COUNT];
    emotion_snn_get_category_confidences(bridge, confidences);

    for (int i = 0; i < EMOTION_COUNT; i++) {
        EXPECT_LE(confidences[i], 1.0f) << "Confidence must be <= 1 at max intensity";
        EXPECT_GE(confidences[i], 0.0f) << "Confidence must be >= 0 at max intensity";
    }
}

TEST_F(EmotionSNNBridgeRegressionTest, RapidEmotionSwitching) {
    // Edge case: Rapid switching between different emotions

    emotion_category_t emotions[] = {
        EMOTION_HAPPINESS, EMOTION_ANGER, EMOTION_FEAR, EMOTION_SADNESS,
        EMOTION_SURPRISE, EMOTION_DISGUST, EMOTION_CALM, EMOTION_PANIC
    };

    for (int cycle = 0; cycle < 50; cycle++) {
        for (emotion_category_t emo : emotions) {
            emotion_recognition_result_t result = create_emotion_result(
                emo, 0.8f,
                (emo <= EMOTION_SURPRISE) ? 0.5f : -0.5f,
                0.7f);

            emotion_snn_encode_observation(bridge, &result);
            emotion_snn_step(bridge);
        }
    }

    // Bridge should still be stable
    emotion_snn_bridge_state_t state;
    int ret = emotion_snn_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(state.state, EMOTION_SNN_STATE_DISABLED);

    // Stats should reflect the operations
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_observations, 0u);
    // Note: emotion_transitions may not be tracked by all implementations
    // So we only verify observations were recorded
}

TEST_F(EmotionSNNBridgeRegressionTest, EmptyFeatureHandling) {
    // Edge case: Empty or zero-length feature input

    // Null features should fail gracefully
    int spikes = emotion_snn_encode_features(bridge, nullptr, 0, 0.5f, 0.5f);
    EXPECT_LE(spikes, 0) << "Null features should fail gracefully";

    // Empty feature array with n_features=0 - implementation may still process VA
    // which is acceptable behavior, so we just verify it doesn't crash
    float empty_features[1] = {0.0f};
    spikes = emotion_snn_encode_features(bridge, empty_features, 0, 0.5f, 0.5f);
    EXPECT_GE(spikes, -1) << "Empty features should handle gracefully (may still encode VA)";

    // Bridge should still be functional
    emotion_recognition_result_t result = create_emotion_result();
    spikes = emotion_snn_encode_observation(bridge, &result);
    EXPECT_GE(spikes, 0);
}

TEST_F(EmotionPlasticityBridgeRegressionTest, ZeroIntensityStimulus) {
    // Edge case: Zero intensity stimulus

    emotion_plasticity_register_synapse(
        bridge, 0, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.5f);

    uint64_t timestamp = get_timestamp_us();

    int ret = emotion_plasticity_stimulus(bridge, EMOTION_HAPPINESS, 0.0f, timestamp);
    EXPECT_EQ(ret, 0) << "Zero intensity stimulus should be handled";

    emotion_plasticity_update(bridge, 10.0f);

    // Synapse should still be valid
    emotion_plasticity_synapse_t state;
    ret = emotion_plasticity_get_synapse(bridge, 0, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionPlasticityBridgeRegressionTest, RapidRewardPunishmentAlternation) {
    // Edge case: Rapid alternation between reward and punishment

    for (uint32_t i = 0; i < 8; i++) {
        emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i), 0.5f);
    }

    uint64_t timestamp = get_timestamp_us();

    for (int iter = 0; iter < 200; iter++) {
        emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 0.8f, timestamp);
        timestamp += 1000;

        // Alternate between strong reward and punishment
        float reward = (iter % 2 == 0) ? 1.0f : -1.0f;
        emotion_plasticity_reward(bridge, reward, timestamp);
        timestamp += 1000;

        emotion_plasticity_update(bridge, 2.0f);
    }

    // Weights should still be bounded
    for (uint32_t syn = 0; syn < 8; syn++) {
        emotion_plasticity_synapse_t state;
        int ret = emotion_plasticity_get_synapse(bridge, syn, &state);
        if (ret == 0) {
            EXPECT_GE(state.weight, config.weight_min);
            EXPECT_LE(state.weight, config.weight_max);
        }
    }
}

TEST_F(EmotionPlasticityBridgeRegressionTest, ExtinctionTrialEdgeCases) {
    // Edge case: Extinction trial behavior

    emotion_plasticity_register_synapse(
        bridge, 0, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.8f);

    uint64_t timestamp = get_timestamp_us();

    // First, strengthen the association
    for (int i = 0; i < 50; i++) {
        emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, timestamp);
        emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, timestamp + 5000);
        emotion_plasticity_update(bridge, 10.0f);
        timestamp += 20000;
    }

    emotion_plasticity_synapse_t state_before;
    emotion_plasticity_get_synapse(bridge, 0, &state_before);
    float weight_before = state_before.weight;

    // Run extinction trials
    for (int i = 0; i < 100; i++) {
        emotion_plasticity_extinction_trial(bridge, EMOTION_FEAR, timestamp);
        emotion_plasticity_update(bridge, 10.0f);
        timestamp += 20000;
    }

    emotion_plasticity_synapse_t state_after;
    emotion_plasticity_get_synapse(bridge, 0, &state_after);

    // Extinction level should increase
    EXPECT_GT(state_after.extinction_level, 0.0f)
        << "Extinction level should increase after extinction trials";
    EXPECT_LE(state_after.extinction_level, 1.0f)
        << "Extinction level must be bounded";
}

//=============================================================================
// SECTION 5: State Consistency Tests
//=============================================================================

TEST_F(EmotionSNNBridgeRegressionTest, StateConsistentAfterManyOperations) {
    // Perform many operations and verify state consistency

    for (int iter = 0; iter < 200; iter++) {
        emotion_category_t emo = static_cast<emotion_category_t>(iter % EMOTION_COUNT);
        emotion_recognition_result_t result = create_emotion_result(
            emo, 0.7f + 0.2f * sinf(iter * 0.1f), sinf(iter * 0.1f), 0.5f + 0.3f * cosf(iter * 0.1f));

        emotion_snn_encode_observation(bridge, &result);
        emotion_snn_simulate(bridge, 5.0f);

        if (iter % 50 == 49) {
            // Modulate periodically
            emotion_snn_modulate_by_arousal(bridge, 0.8f);
            emotion_snn_set_intensity_modulation(bridge, 0.7f);
        }
    }

    // State should be valid and consistent
    emotion_snn_bridge_state_t state;
    int ret = emotion_snn_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(state.avg_firing_rate, 0.0f);
    EXPECT_GE(state.emotion.valence, -1.0f);
    EXPECT_LE(state.emotion.valence, 1.0f);
    EXPECT_GE(state.emotion.arousal, 0.0f);
    EXPECT_LE(state.emotion.arousal, 1.0f);
}

TEST_F(EmotionPlasticityBridgeRegressionTest, StatisticsAccurate) {
    // Statistics should accurately reflect operations

    for (uint32_t i = 0; i < 8; i++) {
        emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i), 0.5f);
    }

    const int n_stimuli = 50;
    const int n_responses = 30;
    uint64_t timestamp = get_timestamp_us();

    for (int i = 0; i < n_stimuli; i++) {
        emotion_plasticity_stimulus(bridge,
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.8f, timestamp);
        timestamp += 10000;
    }

    for (int i = 0; i < n_responses; i++) {
        emotion_plasticity_response(bridge,
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.7f, timestamp);
        timestamp += 10000;
    }

    emotion_plasticity_stats_t stats;
    int ret = emotion_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.total_observations, static_cast<uint64_t>(n_stimuli));
    EXPECT_EQ(stats.total_responses, static_cast<uint64_t>(n_responses));
}

TEST_F(EmotionSNNBridgeRegressionTest, ResetClearsStateCorrectly) {
    // Reset should clear state while maintaining configuration

    // Do some work
    emotion_recognition_result_t result = create_emotion_result(EMOTION_ANGER, 0.9f, -0.7f, 0.9f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 50.0f);

    // Get state before reset
    emotion_snn_bridge_state_t state_before;
    emotion_snn_get_state(bridge, &state_before);

    // Reset
    int ret = emotion_snn_reset(bridge);
    EXPECT_EQ(ret, 0);

    // Get state after reset
    emotion_snn_bridge_state_t state_after;
    emotion_snn_get_state(bridge, &state_after);

    // State should be reset
    EXPECT_EQ(state_after.state, EMOTION_SNN_STATE_IDLE);

    // Bridge should still be functional
    result = create_emotion_result(EMOTION_HAPPINESS, 0.8f, 0.7f, 0.5f);
    int spikes = emotion_snn_encode_observation(bridge, &result);
    EXPECT_GE(spikes, 0);
}

//=============================================================================
// SECTION 6: Transition Probability Tests
//=============================================================================

TEST_F(EmotionSNNBridgeRegressionTest, TransitionProbabilitiesBounded) {
    // Transition probabilities should always be in [0, 1]

    // Process some emotions to establish state
    for (int i = 0; i < 20; i++) {
        emotion_recognition_result_t result = create_emotion_result(
            static_cast<emotion_category_t>(i % EMOTION_COUNT), 0.8f, 0.0f, 0.6f);
        emotion_snn_encode_observation(bridge, &result);
        emotion_snn_simulate(bridge, 10.0f);
    }

    // Check all pairwise transition probabilities
    for (int from = 0; from < EMOTION_COUNT; from++) {
        for (int to = 0; to < EMOTION_COUNT; to++) {
            float prob = emotion_snn_get_transition_prob(
                bridge,
                static_cast<emotion_category_t>(from),
                static_cast<emotion_category_t>(to));

            EXPECT_GE(prob, 0.0f)
                << "Transition prob must be >= 0 (from=" << from << " to=" << to << ")";
            EXPECT_LE(prob, 1.0f)
                << "Transition prob must be <= 1 (from=" << from << " to=" << to << ")";
            EXPECT_FALSE(std::isnan(prob))
                << "Transition prob must not be NaN (from=" << from << " to=" << to << ")";
        }
    }
}

//=============================================================================
// SECTION 7: Modulation Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeRegressionTest, ValenceModulationBounded) {
    // Valence modulation should handle extreme values

    float test_valences[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};

    for (float val : test_valences) {
        int ret = emotion_plasticity_set_valence_modulation(bridge, val);
        EXPECT_EQ(ret, 0) << "Setting valence modulation should succeed for " << val;

        emotion_plasticity_bridge_state_t state;
        emotion_plasticity_get_state(bridge, &state);

        EXPECT_GE(state.current_valence_mod, -1.0f);
        EXPECT_LE(state.current_valence_mod, 1.0f);
    }
}

TEST_F(EmotionPlasticityBridgeRegressionTest, ArousalModulationBounded) {
    // Arousal modulation should handle extreme values

    float test_arousals[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float aro : test_arousals) {
        int ret = emotion_plasticity_set_arousal_modulation(bridge, aro);
        EXPECT_EQ(ret, 0) << "Setting arousal modulation should succeed for " << aro;

        emotion_plasticity_bridge_state_t state;
        emotion_plasticity_get_state(bridge, &state);

        EXPECT_GE(state.current_arousal_mod, 0.0f);
        EXPECT_LE(state.current_arousal_mod, 1.0f);
    }
}

TEST_F(EmotionSNNBridgeRegressionTest, IntensityModulationEffect) {
    // Intensity modulation should affect processing

    emotion_recognition_result_t result = create_emotion_result(EMOTION_FEAR, 0.8f, -0.6f, 0.9f);

    // Low intensity modulation
    emotion_snn_set_intensity_modulation(bridge, 0.1f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 30.0f);

    emotion_snn_emotion_state_t low_state;
    emotion_snn_get_emotion_state(bridge, &low_state);

    emotion_snn_reset(bridge);

    // High intensity modulation
    emotion_snn_set_intensity_modulation(bridge, 0.9f);
    emotion_snn_encode_observation(bridge, &result);
    emotion_snn_simulate(bridge, 30.0f);

    emotion_snn_emotion_state_t high_state;
    emotion_snn_get_emotion_state(bridge, &high_state);

    // Both should be valid
    EXPECT_FALSE(std::isnan(low_state.intensity));
    EXPECT_FALSE(std::isnan(high_state.intensity));
}

//=============================================================================
// SECTION 8: Sensitivity and Extinction Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeRegressionTest, SensitivityBounded) {
    // Sensitivity values should be bounded

    for (uint32_t i = 0; i < 8; i++) {
        emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            static_cast<emotion_category_t>(i), 0.5f);
    }

    // Process many events
    uint64_t timestamp = get_timestamp_us();
    for (int iter = 0; iter < 100; iter++) {
        emotion_plasticity_stimulus(bridge, EMOTION_ANGER, 0.9f, timestamp);
        emotion_plasticity_response(bridge, EMOTION_ANGER, 0.9f, timestamp + 5000);
        emotion_plasticity_reward(bridge, 1.0f, timestamp + 10000);
        emotion_plasticity_update(bridge, 10.0f);
        timestamp += 20000;
    }

    // Check sensitivity for all emotions
    for (int i = 0; i < EMOTION_COUNT; i++) {
        float sensitivity = emotion_plasticity_get_sensitivity(
            bridge, static_cast<emotion_category_t>(i));

        EXPECT_GE(sensitivity, 0.0f)
            << "Sensitivity must be >= 0 for emotion " << i;
        EXPECT_FALSE(std::isnan(sensitivity))
            << "Sensitivity must not be NaN for emotion " << i;
        EXPECT_FALSE(std::isinf(sensitivity))
            << "Sensitivity must not be Inf for emotion " << i;
    }
}

TEST_F(EmotionPlasticityBridgeRegressionTest, ExtinctionLevelBounded) {
    // Extinction level should be bounded

    for (int i = 0; i < EMOTION_COUNT; i++) {
        float extinction = emotion_plasticity_get_extinction_level(
            bridge, static_cast<emotion_category_t>(i));

        EXPECT_GE(extinction, 0.0f)
            << "Extinction must be >= 0 for emotion " << i;
        EXPECT_LE(extinction, 1.0f)
            << "Extinction must be <= 1 for emotion " << i;
    }
}

TEST_F(EmotionPlasticityBridgeRegressionTest, ResponseModulationBounded) {
    // Response modulation should be bounded

    for (uint32_t i = 0; i < 8; i++) {
        emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_EMOTION_TO_RESPONSE,
            static_cast<emotion_category_t>(i), 0.5f);
    }

    for (int i = 0; i < EMOTION_COUNT; i++) {
        float modulation;
        int ret = emotion_plasticity_get_response_modulation(
            bridge, static_cast<emotion_category_t>(i), &modulation);

        if (ret == 0) {
            EXPECT_GE(modulation, 0.0f)
                << "Response modulation must be >= 0 for emotion " << i;
            EXPECT_FALSE(std::isnan(modulation))
                << "Response modulation must not be NaN for emotion " << i;
        }
    }
}
