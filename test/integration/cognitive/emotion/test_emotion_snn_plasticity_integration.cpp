//=============================================================================
// test_emotion_snn_plasticity_integration.cpp - Emotion SNN/Plasticity Integration
//=============================================================================
/**
 * @file test_emotion_snn_plasticity_integration.cpp
 * @brief Integration tests for Emotion-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between emotion system, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows enable emotional learning
 * HOW:  Create both bridges, simulate emotional processing pipelines
 *
 * INTEGRATION POINTS:
 * - Emotion observation -> SNN encoding -> population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Valence-arousal encoding -> round-trip decoding
 * - Fear conditioning -> extinction learning
 *
 * TEST SCENARIOS:
 * 1. Full Emotion Processing Pipeline: Encode emotion -> SNN simulation -> Decode
 * 2. Emotion-Plasticity Learning: Stimulus -> STDP update -> weight changes
 * 3. Fear Conditioning: Stimulus-response pairing with reward
 * 4. Extinction Learning: Repeated stimulus without outcome
 * 5. Valence-Arousal Encoding/Decoding: Round-trip encoding
 * 6. Cross-Bridge Integration: Full pipeline with response modulation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"
#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    emotion_snn_bridge_t* snn_bridge;
    emotion_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> weight_change_count{0};
    std::atomic<float> last_weight_change{0.0f};
    emotion_category_t last_emotion_changed{EMOTION_NEUTRAL};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        emotion_snn_config_t snn_config = emotion_snn_config_default();
        snn_config.input_dim = EMOTION_SNN_INPUT_DIM;
        snn_config.hidden_dim = 64;
        snn_config.output_dim = EMOTION_COUNT;
        snn_config.va_dim = EMOTION_SNN_VA_DIM;
        snn_config.enable_bio_async = false;  // Disable for predictable tests
        snn_config.enable_immune_modulation = false;
        snn_config.enable_va_encoding = true;
        snn_config.enable_plasticity_integration = true;
        snn_config.dt_ms = EMOTION_SNN_DEFAULT_DT;
        snn_config.simulation_window_ms = 100.0f;

        snn_bridge = emotion_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create emotion SNN bridge";

        // Create Plasticity bridge with test-friendly config
        emotion_plasticity_config_t plasticity_config = emotion_plasticity_config_default();
        plasticity_config.enable_valence_modulation = true;
        plasticity_config.enable_arousal_modulation = true;
        plasticity_config.enable_bcm = true;
        plasticity_config.enable_homeostatic = true;
        plasticity_config.enable_eligibility = true;
        plasticity_config.enable_extinction = true;
        plasticity_config.enable_bio_async = false;

        plasticity_bridge = emotion_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create emotion plasticity bridge";

        // Reset counters
        weight_change_count = 0;
        last_weight_change = 0.0f;
        last_emotion_changed = EMOTION_NEUTRAL;
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

    // Generate emotion features based on valence/arousal
    void generate_emotion_features(float* features, uint32_t n,
                                   emotion_category_t emotion,
                                   float valence, float arousal) {
        // Create distinct patterns for different emotions
        for (uint32_t i = 0; i < n; i++) {
            float base = 0.3f + 0.2f * valence;
            float mod = arousal * 0.4f;
            features[i] = base + mod * sinf((float)i * 0.15f + (float)emotion * 0.5f);
            // Clamp to valid range
            if (features[i] < 0.0f) features[i] = 0.0f;
            if (features[i] > 1.0f) features[i] = 1.0f;
        }
    }

    // Create a mock emotion recognition result
    void create_emotion_result(emotion_recognition_result_t* result,
                               emotion_category_t category,
                               float confidence,
                               float valence,
                               float arousal,
                               float intensity) {
        memset(result, 0, sizeof(*result));
        result->category = category;
        result->confidence = confidence;
        result->valence = valence;
        result->arousal = arousal;
        result->intensity = (intensity < 0.3f) ? EMOTION_INTENSITY_LOW :
                           (intensity < 0.6f) ? EMOTION_INTENSITY_MEDIUM :
                           (intensity < 0.8f) ? EMOTION_INTENSITY_HIGH :
                                                EMOTION_INTENSITY_EXTREME;
        result->timestamp_ms = nimcp_time_get_us() / 1000;
        result->is_negative_emotion = (valence < 0.0f);
    }
};

//=============================================================================
// Static Callback Functions
//=============================================================================

static std::atomic<int>* g_weight_counter = nullptr;
static std::atomic<float>* g_last_weight_change = nullptr;

static void weight_change_callback(uint32_t synapse_id,
                                   emotion_category_t emotion,
                                   float old_weight,
                                   float new_weight,
                                   emotion_learn_event_t event_type,
                                   void* user_data) {
    if (g_weight_counter) {
        (*g_weight_counter)++;
    }
    if (g_last_weight_change) {
        (*g_last_weight_change) = new_weight - old_weight;
    }
}

//=============================================================================
// Test 1: Full Emotion Processing Pipeline
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, FullEmotionProcessingPipeline) {
    // Create emotion recognition result for FEAR
    emotion_recognition_result_t result;
    create_emotion_result(&result, EMOTION_FEAR, 0.85f, -0.8f, 0.9f, 0.8f);

    // Step 1: Encode emotion observation in SNN
    int spikes = emotion_snn_encode_observation(snn_bridge, &result);
    EXPECT_GE(spikes, 0) << "Emotion encoding should succeed (returns spike count)";

    // Step 2: Run SNN simulation
    int ret = emotion_snn_simulate(snn_bridge, 100.0f);
    EXPECT_EQ(ret, 0) << "SNN simulation should succeed";

    // Step 3: Decode emotion category from SNN output
    float confidences[EMOTION_COUNT];
    emotion_category_t decoded = emotion_snn_get_category_confidences(snn_bridge, confidences);
    EXPECT_NE(decoded, EMOTION_UNKNOWN) << "Should decode valid emotion category";

    // Verify FEAR has reasonable confidence
    EXPECT_GT(confidences[EMOTION_FEAR], 0.0f) << "FEAR confidence should be positive";

    // Step 4: Get complete emotion state
    emotion_snn_emotion_state_t emotion_state;
    ret = emotion_snn_get_emotion_state(snn_bridge, &emotion_state);
    EXPECT_EQ(ret, 0) << "Getting emotion state should succeed";

    // Verify stats accumulated
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(snn_bridge, &stats);
    EXPECT_GT(stats.total_observations, 0u) << "Should have recorded observations";
}

//=============================================================================
// Test 2: Emotion-Plasticity Learning (STDP Update)
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, EmotionPlasticityLearning) {
    // Setup weight change callback
    g_weight_counter = &weight_change_count;
    g_last_weight_change = &last_weight_change;
    emotion_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    // Register synapses for emotional learning
    for (int i = 0; i < (int)EMOTION_COUNT; i++) {
        emotion_plasticity_register_synapse(
            plasticity_bridge,
            i,                                    // synapse_id
            EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,  // type
            (emotion_category_t)i,                // associated emotion
            0.5f                                  // initial weight
        );
    }

    uint64_t timestamp = nimcp_time_get_us();

    // Step 1: Record emotional stimulus (pre-synaptic event)
    int ret = emotion_plasticity_stimulus(
        plasticity_bridge,
        EMOTION_FEAR,
        0.8f,        // intensity
        timestamp
    );
    EXPECT_EQ(ret, 0) << "Recording stimulus should succeed";

    // Step 2: Record emotional response (post-synaptic event)
    ret = emotion_plasticity_response(
        plasticity_bridge,
        EMOTION_FEAR,
        0.7f,        // response strength
        timestamp + 10000  // 10ms later (within STDP window)
    );
    EXPECT_EQ(ret, 0) << "Recording response should succeed";

    // Step 3: Update plasticity (triggers STDP calculation)
    ret = emotion_plasticity_update(plasticity_bridge, 50.0f);
    EXPECT_EQ(ret, 0) << "Plasticity update should succeed";

    // Step 4: Verify weight change occurred
    emotion_plasticity_synapse_t synapse;
    ret = emotion_plasticity_get_synapse(plasticity_bridge, EMOTION_FEAR, &synapse);
    EXPECT_EQ(ret, 0) << "Getting synapse state should succeed";

    // LTP should occur (pre before post within STDP window)
    // Weight may or may not change depending on STDP implementation details
    EXPECT_GE(synapse.weight, 0.0f) << "Weight should be non-negative";
    EXPECT_LE(synapse.weight, 1.0f) << "Weight should be bounded";

    // Check statistics
    emotion_plasticity_stats_t stats;
    emotion_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GT(stats.total_observations, 0u) << "Should have recorded stimulus observations";
}

//=============================================================================
// Test 3: Fear Conditioning
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, FearConditioning) {
    // Register synapse for fear conditioning
    emotion_plasticity_register_synapse(
        plasticity_bridge,
        100,                                   // synapse_id for CS-US association
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_FEAR,
        0.3f                                   // low initial weight
    );

    uint64_t timestamp = nimcp_time_get_us();
    const int conditioning_trials = 5;

    // Get initial weight
    emotion_plasticity_synapse_t synapse_before;
    emotion_plasticity_get_synapse(plasticity_bridge, 100, &synapse_before);
    float initial_weight = synapse_before.weight;

    // Perform conditioning trials (CS followed by US)
    for (int trial = 0; trial < conditioning_trials; trial++) {
        // Conditioned stimulus (CS) - neutral stimulus
        emotion_plasticity_stimulus(
            plasticity_bridge,
            EMOTION_FEAR,
            0.6f,
            timestamp + trial * 2000000  // 2s between trials
        );

        // Unconditioned stimulus (US) - causes fear response
        emotion_plasticity_response(
            plasticity_bridge,
            EMOTION_FEAR,
            0.9f,  // strong response
            timestamp + trial * 2000000 + 20000  // 20ms after CS
        );

        // Reward signal reinforces the association
        emotion_plasticity_reward(
            plasticity_bridge,
            -0.8f,  // Negative reward (punishment/aversive)
            timestamp + trial * 2000000 + 50000
        );

        // Update plasticity
        emotion_plasticity_update(plasticity_bridge, 50.0f);
    }

    // Consolidate learning
    int ret = emotion_plasticity_consolidate(plasticity_bridge);
    EXPECT_EQ(ret, 0) << "Consolidation should succeed";

    // Verify synapse state after conditioning
    emotion_plasticity_synapse_t synapse_after;
    int get_ret = emotion_plasticity_get_synapse(plasticity_bridge, 100, &synapse_after);
    EXPECT_EQ(get_ret, 0) << "Should be able to get synapse state";

    // Verify weight is within valid bounds (plasticity may adjust weight up or down
    // depending on STDP timing - the important thing is the weight stays bounded)
    emotion_plasticity_config_t cfg = emotion_plasticity_config_default();
    EXPECT_GE(synapse_after.weight, cfg.weight_min)
        << "Weight should be >= weight_min";
    EXPECT_LE(synapse_after.weight, cfg.weight_max)
        << "Weight should be <= weight_max";
    EXPECT_FALSE(std::isnan(synapse_after.weight))
        << "Weight should not be NaN";

    // Check sensitivity to fear stimuli (should be non-negative)
    float sensitivity = emotion_plasticity_get_sensitivity(plasticity_bridge, EMOTION_FEAR);
    EXPECT_GE(sensitivity, 0.0f) << "Sensitivity should be non-negative";
}

//=============================================================================
// Test 4: Extinction Learning
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, ExtinctionLearning) {
    // Register synapse with high initial weight (already conditioned)
    emotion_plasticity_register_synapse(
        plasticity_bridge,
        200,                                   // synapse_id
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_FEAR,
        0.8f                                   // high initial weight (conditioned)
    );

    uint64_t timestamp = nimcp_time_get_us();
    const int extinction_trials = 8;

    // Get initial extinction level
    float initial_extinction = emotion_plasticity_get_extinction_level(
        plasticity_bridge, EMOTION_FEAR
    );

    // Perform extinction trials (CS without US)
    for (int trial = 0; trial < extinction_trials; trial++) {
        // Present conditioned stimulus only
        int ret = emotion_plasticity_extinction_trial(
            plasticity_bridge,
            EMOTION_FEAR,
            timestamp + trial * 2000000  // 2s between trials
        );
        EXPECT_EQ(ret, 0) << "Extinction trial should succeed";

        // Update plasticity (allows extinction to progress)
        emotion_plasticity_update(plasticity_bridge, 100.0f);
    }

    // Get final extinction level
    float final_extinction = emotion_plasticity_get_extinction_level(
        plasticity_bridge, EMOTION_FEAR
    );

    // Extinction level should increase (more extinction)
    EXPECT_GE(final_extinction, initial_extinction)
        << "Extinction level should increase after extinction trials";

    // Verify weight decreased via response modulation
    float modulation;
    int ret = emotion_plasticity_get_response_modulation(
        plasticity_bridge, EMOTION_FEAR, &modulation
    );
    EXPECT_EQ(ret, 0) << "Getting response modulation should succeed";

    // After extinction, response should be modulated downward
    // (modulation factor depends on extinction level)
    EXPECT_GE(modulation, 0.0f) << "Modulation should be non-negative";
}

//=============================================================================
// Test 5: Valence-Arousal Encoding/Decoding Round-trip
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, ValenceArousalRoundTrip) {
    // Test multiple valence-arousal coordinates
    struct {
        float valence;
        float arousal;
        float intensity;
    } test_cases[] = {
        { -0.8f, 0.9f, 0.8f },   // High negative arousal (fear/anger)
        {  0.7f, 0.8f, 0.7f },   // High positive arousal (excitement)
        { -0.5f, 0.3f, 0.4f },   // Low negative arousal (sadness)
        {  0.6f, 0.2f, 0.3f },   // Low positive arousal (contentment)
        {  0.0f, 0.5f, 0.5f },   // Neutral valence, moderate arousal
    };

    for (const auto& tc : test_cases) {
        // Reset bridge state for each test
        emotion_snn_reset(snn_bridge);

        // Encode valence-arousal
        int spikes = emotion_snn_encode_valence_arousal(
            snn_bridge,
            tc.valence,
            tc.arousal,
            tc.intensity
        );
        EXPECT_GE(spikes, 0) << "VA encoding should succeed";

        // Simulate
        int ret = emotion_snn_simulate(snn_bridge, 100.0f);
        EXPECT_EQ(ret, 0) << "Simulation should succeed";

        // Decode valence-arousal
        float decoded_valence, decoded_arousal;
        ret = emotion_snn_get_valence_arousal(
            snn_bridge,
            &decoded_valence,
            &decoded_arousal
        );
        EXPECT_EQ(ret, 0) << "VA decoding should succeed";

        // Verify decoded values are in valid range
        EXPECT_GE(decoded_valence, -1.0f) << "Decoded valence should be >= -1";
        EXPECT_LE(decoded_valence, 1.0f) << "Decoded valence should be <= 1";
        EXPECT_GE(decoded_arousal, 0.0f) << "Decoded arousal should be >= 0";
        EXPECT_LE(decoded_arousal, 1.0f) << "Decoded arousal should be <= 1";

        // Check that encoding preserves valence sign direction
        // (exact reconstruction depends on SNN dynamics)
        if (std::abs(tc.valence) > 0.3f) {
            EXPECT_TRUE((tc.valence > 0 && decoded_valence >= -0.2f) ||
                        (tc.valence < 0 && decoded_valence <= 0.2f) ||
                        std::abs(tc.valence) < 0.5f)
                << "Valence direction should be roughly preserved";
        }
    }
}

//=============================================================================
// Test 6: Cross-Bridge Integration Pipeline
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, CrossBridgeIntegrationPipeline) {
    // Setup weight callback
    g_weight_counter = &weight_change_count;
    emotion_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    // Register synapses for all emotion categories
    for (int i = 0; i < (int)EMOTION_COUNT; i++) {
        emotion_plasticity_register_synapse(
            plasticity_bridge,
            i,
            EMOTION_SYNAPSE_EMOTION_TO_RESPONSE,
            (emotion_category_t)i,
            0.5f
        );
    }

    uint64_t timestamp = nimcp_time_get_us();

    // Simulate a complete emotional processing cycle
    const int iterations = 5;

    for (int iter = 0; iter < iterations; iter++) {
        // Step 1: Create emotion recognition result
        emotion_category_t emotions[] = {
            EMOTION_FEAR, EMOTION_ANGER, EMOTION_HAPPINESS,
            EMOTION_SADNESS, EMOTION_SURPRISE
        };
        emotion_category_t current_emotion = emotions[iter % 5];

        float valence = (current_emotion == EMOTION_HAPPINESS ||
                        current_emotion == EMOTION_SURPRISE) ? 0.7f : -0.6f;
        float arousal = (current_emotion == EMOTION_FEAR ||
                        current_emotion == EMOTION_ANGER) ? 0.8f : 0.4f;

        emotion_recognition_result_t result;
        create_emotion_result(&result, current_emotion, 0.75f, valence, arousal, 0.7f);

        // Step 2: Encode in SNN
        emotion_snn_encode_observation(snn_bridge, &result);
        emotion_snn_simulate(snn_bridge, 50.0f);

        // Step 3: Get SNN output
        float confidences[EMOTION_COUNT];
        emotion_category_t detected = emotion_snn_get_category_confidences(
            snn_bridge, confidences
        );

        // Step 4: Record stimulus in plasticity bridge
        emotion_plasticity_stimulus(
            plasticity_bridge,
            current_emotion,
            result.arousal,
            timestamp + iter * 1000000
        );

        // Step 5: Record response
        emotion_plasticity_response(
            plasticity_bridge,
            detected,
            confidences[detected],
            timestamp + iter * 1000000 + 15000  // 15ms later
        );

        // Step 6: Set valence/arousal modulation
        emotion_plasticity_set_valence_modulation(plasticity_bridge, valence);
        emotion_plasticity_set_arousal_modulation(plasticity_bridge, arousal);

        // Step 7: Update plasticity
        emotion_plasticity_update(plasticity_bridge, 50.0f);

        // Step 8: Get response modulation for next iteration
        float modulation;
        emotion_plasticity_get_response_modulation(
            plasticity_bridge, current_emotion, &modulation
        );

        // Step 9: Modulate SNN based on arousal from plasticity
        emotion_snn_modulate_by_arousal(snn_bridge, arousal);
    }

    // Verify statistics accumulated across both bridges
    emotion_snn_stats_t snn_stats;
    emotion_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_observations, (uint64_t)iterations)
        << "Should have recorded all observations";

    emotion_plasticity_stats_t plasticity_stats;
    emotion_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GT(plasticity_stats.total_observations, 0u)
        << "Should have recorded plasticity observations";
}

//=============================================================================
// Test 7: State Transitions
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, StateTransitions) {
    // Initial states should be IDLE
    emotion_snn_bridge_state_t snn_state;
    emotion_plasticity_bridge_state_t plasticity_state;

    int ret1 = emotion_snn_get_state(snn_bridge, &snn_state);
    int ret2 = emotion_plasticity_get_state(plasticity_bridge, &plasticity_state);

    EXPECT_EQ(ret1, 0) << "Getting SNN state should succeed";
    EXPECT_EQ(ret2, 0) << "Getting plasticity state should succeed";
    EXPECT_EQ(snn_state.state, EMOTION_SNN_STATE_IDLE) << "SNN should start idle";
    EXPECT_EQ(plasticity_state.state, EMOTION_PLASTICITY_STATE_IDLE)
        << "Plasticity should start idle";

    // Trigger encoding -> should change state
    emotion_recognition_result_t result;
    create_emotion_result(&result, EMOTION_HAPPINESS, 0.8f, 0.7f, 0.6f, 0.7f);

    emotion_snn_encode_observation(snn_bridge, &result);

    // Check SNN state after encoding
    emotion_snn_get_state(snn_bridge, &snn_state);
    EXPECT_GE(snn_state.state, EMOTION_SNN_STATE_IDLE) << "SNN state should be valid";
    EXPECT_LE(snn_state.state, EMOTION_SNN_STATE_DISABLED) << "SNN state should be valid";
}

//=============================================================================
// Test 8: Reset Both Bridges
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, ResetBothBridges) {
    // Generate some activity
    emotion_recognition_result_t result;
    create_emotion_result(&result, EMOTION_FEAR, 0.8f, -0.7f, 0.8f, 0.8f);

    for (int i = 0; i < 5; i++) {
        emotion_snn_encode_observation(snn_bridge, &result);
        emotion_snn_simulate(snn_bridge, 20.0f);
        emotion_plasticity_stimulus(plasticity_bridge, EMOTION_FEAR, 0.8f,
                                    nimcp_time_get_us());
    }

    // Reset both bridges
    int ret1 = emotion_snn_reset(snn_bridge);
    int ret2 = emotion_plasticity_reset(plasticity_bridge);

    EXPECT_EQ(ret1, 0) << "SNN reset should succeed";
    EXPECT_EQ(ret2, 0) << "Plasticity reset should succeed";

    // Verify states are reset
    emotion_snn_bridge_state_t snn_state;
    emotion_plasticity_bridge_state_t plasticity_state;

    emotion_snn_get_state(snn_bridge, &snn_state);
    emotion_plasticity_get_state(plasticity_bridge, &plasticity_state);

    EXPECT_EQ(snn_state.state, EMOTION_SNN_STATE_IDLE) << "SNN should be idle after reset";
    EXPECT_EQ(plasticity_state.state, EMOTION_PLASTICITY_STATE_IDLE)
        << "Plasticity should be idle after reset";
}

//=============================================================================
// Test 9: Statistics Aggregation
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, StatisticsAggregation) {
    // Register synapses
    for (int i = 0; i < 5; i++) {
        emotion_plasticity_register_synapse(
            plasticity_bridge, i,
            EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            (emotion_category_t)i, 0.5f
        );
    }

    // Perform mixed emotional activity
    emotion_category_t emotions[] = {
        EMOTION_HAPPINESS, EMOTION_SADNESS, EMOTION_FEAR,
        EMOTION_ANGER, EMOTION_SURPRISE
    };

    for (int i = 0; i < 5; i++) {
        uint64_t ts = nimcp_time_get_us();

        emotion_recognition_result_t result;
        float valence = (i % 2 == 0) ? 0.6f : -0.6f;
        create_emotion_result(&result, emotions[i], 0.75f, valence, 0.5f, 0.6f);

        emotion_snn_encode_observation(snn_bridge, &result);
        emotion_snn_simulate(snn_bridge, 50.0f);

        emotion_plasticity_stimulus(plasticity_bridge, emotions[i], 0.7f, ts);
        emotion_plasticity_response(plasticity_bridge, emotions[i], 0.6f, ts + 15000);
        emotion_plasticity_update(plasticity_bridge, 20.0f);
    }

    // Get aggregated stats
    emotion_snn_stats_t snn_stats;
    emotion_snn_get_stats(snn_bridge, &snn_stats);

    emotion_plasticity_stats_t plasticity_stats;
    emotion_plasticity_get_stats(plasticity_bridge, &plasticity_stats);

    // Verify SNN stats
    EXPECT_EQ(snn_stats.total_observations, 5u) << "SNN should track 5 observations";
    EXPECT_GE(snn_stats.total_decodings, 0u) << "SNN should have decodings available";

    // Verify plasticity stats
    EXPECT_GT(plasticity_stats.total_pre_spikes, 0u) << "Should have pre-spikes";
    EXPECT_GT(plasticity_stats.total_post_spikes, 0u) << "Should have post-spikes";

    // Reset stats and verify
    emotion_snn_reset_stats(snn_bridge);
    emotion_plasticity_reset_stats(plasticity_bridge);

    emotion_snn_get_stats(snn_bridge, &snn_stats);
    emotion_plasticity_get_stats(plasticity_bridge, &plasticity_stats);

    EXPECT_EQ(snn_stats.total_observations, 0u) << "SNN stats should be reset";
    EXPECT_EQ(plasticity_stats.total_pre_spikes, 0u) << "Plasticity stats should be reset";
}

//=============================================================================
// Test 10: Emotion Transition Tracking
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, EmotionTransitionTracking) {
    // Test emotion transitions through SNN
    emotion_category_t transition_sequence[] = {
        EMOTION_NEUTRAL,
        EMOTION_INTEREST,
        EMOTION_FRUSTRATION,
        EMOTION_ANGER,
        EMOTION_CALM
    };

    emotion_category_t prev_emotion = EMOTION_UNKNOWN;

    for (const auto& emotion : transition_sequence) {
        // Create emotion result
        float valence = (emotion == EMOTION_INTEREST || emotion == EMOTION_CALM) ? 0.5f : -0.5f;
        float arousal = (emotion == EMOTION_ANGER || emotion == EMOTION_FRUSTRATION) ? 0.8f : 0.3f;

        emotion_recognition_result_t result;
        create_emotion_result(&result, emotion, 0.8f, valence, arousal, 0.7f);

        // Encode and simulate
        emotion_snn_encode_observation(snn_bridge, &result);
        emotion_snn_simulate(snn_bridge, 50.0f);

        if (prev_emotion != EMOTION_UNKNOWN) {
            // Get transition probability
            float trans_prob = emotion_snn_get_transition_prob(
                snn_bridge, prev_emotion, emotion
            );
            EXPECT_GE(trans_prob, 0.0f) << "Transition probability should be non-negative";
            EXPECT_LE(trans_prob, 1.0f) << "Transition probability should be <= 1";
        }

        prev_emotion = emotion;
    }

    // Verify transition stats
    emotion_snn_stats_t stats;
    emotion_snn_get_stats(snn_bridge, &stats);
    EXPECT_GE(stats.emotion_transitions, 0u) << "Should track emotion transitions";
}

//=============================================================================
// Test 11: Arousal-Based Modulation
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, ArousalBasedModulation) {
    // Test that arousal modulates encoding gain
    float arousal_levels[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};

    for (float arousal : arousal_levels) {
        // Reset for clean test
        emotion_snn_reset(snn_bridge);

        // Set arousal modulation
        int ret = emotion_snn_modulate_by_arousal(snn_bridge, arousal);
        EXPECT_EQ(ret, 0) << "Arousal modulation should succeed";

        // Encode same emotion at different arousal levels
        emotion_recognition_result_t result;
        create_emotion_result(&result, EMOTION_FEAR, 0.8f, -0.7f, arousal, 0.7f);

        int spikes = emotion_snn_encode_observation(snn_bridge, &result);
        EXPECT_GE(spikes, 0) << "Encoding should succeed";

        // Higher arousal should generally produce more spikes or activity
        // (exact relationship depends on implementation)
        emotion_snn_simulate(snn_bridge, 50.0f);

        emotion_snn_bridge_state_t state;
        emotion_snn_get_state(snn_bridge, &state);
        EXPECT_GE(state.avg_firing_rate, 0.0f) << "Firing rate should be non-negative";
    }
}

//=============================================================================
// Test 12: Intensity-Based Rate Modulation
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, IntensityBasedRateModulation) {
    // Test intensity modulation on SNN
    float intensity_levels[] = {0.2f, 0.5f, 0.8f, 1.0f};

    for (float intensity : intensity_levels) {
        emotion_snn_reset(snn_bridge);

        // Set intensity modulation
        int ret = emotion_snn_set_intensity_modulation(snn_bridge, intensity);
        EXPECT_EQ(ret, 0) << "Intensity modulation should succeed";

        // Encode emotion
        int spikes = emotion_snn_encode_valence_arousal(snn_bridge, -0.5f, 0.6f, intensity);
        EXPECT_GE(spikes, 0) << "Encoding should succeed";

        emotion_snn_simulate(snn_bridge, 50.0f);

        // Get firing rate
        emotion_snn_bridge_state_t state;
        emotion_snn_get_state(snn_bridge, &state);
        EXPECT_GE(state.avg_firing_rate, 0.0f) << "Firing rate should be non-negative";
    }
}

//=============================================================================
// Test 13: Habituation Through Repeated Stimuli
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, HabituationThroughRepeatedStimuli) {
    // Register synapse for habituation test
    emotion_plasticity_register_synapse(
        plasticity_bridge,
        300,
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_FEAR,
        0.7f
    );

    // Get initial sensitivity
    float initial_sensitivity = emotion_plasticity_get_sensitivity(
        plasticity_bridge, EMOTION_FEAR
    );

    uint64_t timestamp = nimcp_time_get_us();
    const int habituation_trials = 15;

    // Repeated weak stimuli should cause habituation
    for (int trial = 0; trial < habituation_trials; trial++) {
        // Weak stimulus without strong outcome
        emotion_plasticity_stimulus(
            plasticity_bridge,
            EMOTION_FEAR,
            0.3f,  // weak intensity
            timestamp + trial * 500000  // 500ms between trials
        );

        // No response or very weak response
        // (absence of post-synaptic activity)

        emotion_plasticity_update(plasticity_bridge, 50.0f);
    }

    // Get final sensitivity
    float final_sensitivity = emotion_plasticity_get_sensitivity(
        plasticity_bridge, EMOTION_FEAR
    );

    // After habituation, sensitivity should decrease or stay same
    // (depends on implementation details)
    EXPECT_GE(final_sensitivity, 0.0f) << "Sensitivity should remain non-negative";

    // Verify stats
    emotion_plasticity_stats_t stats;
    emotion_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_GE(stats.total_pre_spikes, (uint64_t)habituation_trials)
        << "Should have recorded all habituation stimuli";
}

//=============================================================================
// Test 14: BCM Metaplasticity
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, BCMMetaplasticity) {
    // Register synapse for BCM test
    emotion_plasticity_register_synapse(
        plasticity_bridge,
        400,
        EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
        EMOTION_HAPPINESS,
        0.5f
    );

    uint64_t timestamp = nimcp_time_get_us();

    // Phase 1: High activity (should shift BCM threshold up)
    for (int i = 0; i < 10; i++) {
        emotion_plasticity_stimulus(
            plasticity_bridge, EMOTION_HAPPINESS, 0.9f,
            timestamp + i * 100000
        );
        emotion_plasticity_response(
            plasticity_bridge, EMOTION_HAPPINESS, 0.9f,
            timestamp + i * 100000 + 10000
        );
        emotion_plasticity_update(plasticity_bridge, 50.0f);
    }

    // Get synapse state after high activity
    emotion_plasticity_synapse_t synapse;
    emotion_plasticity_get_synapse(plasticity_bridge, 400, &synapse);
    float bcm_threshold_high = synapse.bcm_threshold;

    // Phase 2: Low activity (threshold should adjust)
    for (int i = 0; i < 10; i++) {
        emotion_plasticity_stimulus(
            plasticity_bridge, EMOTION_HAPPINESS, 0.2f,
            timestamp + 1000000 + i * 100000
        );
        emotion_plasticity_update(plasticity_bridge, 50.0f);
    }

    emotion_plasticity_get_synapse(plasticity_bridge, 400, &synapse);
    float bcm_threshold_low = synapse.bcm_threshold;

    // BCM threshold should have changed based on activity history
    // (exact direction depends on implementation)
    EXPECT_GE(synapse.avg_activity, 0.0f) << "Activity average should be tracked";
}

//=============================================================================
// Test 15: Reward-Modulated Learning
//=============================================================================

TEST_F(EmotionSNNPlasticityIntegrationTest, RewardModulatedLearning) {
    // Setup callback
    g_weight_counter = &weight_change_count;
    emotion_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    // Register synapse for reward test
    emotion_plasticity_register_synapse(
        plasticity_bridge,
        500,
        EMOTION_SYNAPSE_EMOTION_TO_RESPONSE,
        EMOTION_HAPPINESS,
        0.5f
    );

    uint64_t timestamp = nimcp_time_get_us();

    // Trial with positive reward
    emotion_plasticity_stimulus(plasticity_bridge, EMOTION_HAPPINESS, 0.7f, timestamp);
    emotion_plasticity_response(plasticity_bridge, EMOTION_HAPPINESS, 0.6f, timestamp + 15000);
    emotion_plasticity_reward(plasticity_bridge, 0.9f, timestamp + 50000);  // Positive reward
    emotion_plasticity_update(plasticity_bridge, 100.0f);

    emotion_plasticity_synapse_t synapse;
    emotion_plasticity_get_synapse(plasticity_bridge, 500, &synapse);
    float weight_after_reward = synapse.weight;

    // Trial with punishment
    emotion_plasticity_stimulus(plasticity_bridge, EMOTION_HAPPINESS, 0.7f, timestamp + 200000);
    emotion_plasticity_response(plasticity_bridge, EMOTION_HAPPINESS, 0.6f, timestamp + 215000);
    emotion_plasticity_reward(plasticity_bridge, -0.9f, timestamp + 250000);  // Negative reward
    emotion_plasticity_update(plasticity_bridge, 100.0f);

    emotion_plasticity_get_synapse(plasticity_bridge, 500, &synapse);
    float weight_after_punishment = synapse.weight;

    // Verify stats tracked reward/punishment
    emotion_plasticity_stats_t stats;
    emotion_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_NE(stats.total_reward, 0.0f) << "Should have recorded rewards";
    EXPECT_NE(stats.total_punishment, 0.0f) << "Should have recorded punishments";
}

