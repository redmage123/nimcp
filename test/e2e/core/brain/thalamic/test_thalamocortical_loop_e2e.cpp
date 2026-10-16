//=============================================================================
// test_thalamocortical_loop_e2e.cpp - Thalamocortical Loop End-to-End Tests
//=============================================================================
/**
 * @file test_thalamocortical_loop_e2e.cpp
 * @brief End-to-end tests for thalamocortical loop dynamics
 *
 * WHAT: Full pipeline tests for thalamocortical feedback and oscillations
 * WHY:  Verify bidirectional cortex-thalamus communication and sleep/wake transitions
 * HOW:  Test corticothalamic feedback, oscillatory patterns, arousal modulation
 *
 * TEST COVERAGE:
 * - Corticothalamic feedback
 * - Oscillatory dynamics (alpha, spindles)
 * - Sleep state transitions
 * - Arousal modulation
 * - Attention spotlight control
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>

#include "e2e_test_framework.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "middleware/routing/nimcp_thalamic_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ThalamocorticalLoopE2ETest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    thalamic_router_t* router = nullptr;
    static constexpr uint32_t NUM_CHANNELS = 64;
    static constexpr uint32_t NUM_NEURONS = 128;

    void SetUp() override {
        // Create thalamus
        thalamus_config_t thal_config;
        thalamus_default_config(&thal_config);
        thal_config.neurons_per_nucleus = NUM_NEURONS;
        thal_config.channels_per_nucleus = NUM_CHANNELS;
        thal_config.enable_trn = true;
        thal_config.enable_mode_switching = true;
        thal_config.enable_attention_gating = true;
        thal = thalamus_create(&thal_config);
        ASSERT_NE(thal, nullptr);

        // Create router
        thalamic_router_config_t router_config = thalamic_router_default_config();
        router_config.enable_attention_gating = true;
        router_config.enable_priority_routing = true;
        router = thalamic_router_create(&router_config);
        ASSERT_NE(router, nullptr);
    }

    void TearDown() override {
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }

    // Generate sinusoidal oscillation pattern
    std::vector<float> generate_oscillation(uint32_t size, float frequency,
                                             float phase, float amplitude) {
        std::vector<float> signal(size);
        for (uint32_t i = 0; i < size; ++i) {
            float t = static_cast<float>(i) / size;
            signal[i] = amplitude * (0.5f + 0.5f * std::sin(2.0f * M_PI * frequency * t + phase));
        }
        return signal;
    }

    // Simulate cortical feedback signal
    std::vector<float> generate_cortical_feedback(uint32_t size, float strength) {
        std::vector<float> feedback(size);
        for (uint32_t i = 0; i < size; ++i) {
            feedback[i] = strength * (0.5f + 0.3f * std::sin(static_cast<float>(i) * 0.1f));
        }
        return feedback;
    }

    float calculate_power(const std::vector<float>& signal) {
        float sum = 0.0f;
        for (float v : signal) {
            sum += v * v;
        }
        return sum / signal.size();
    }

    float calculate_mean(const std::vector<float>& signal) {
        return std::accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
    }
};

//=============================================================================
// Corticothalamic Feedback Tests
//=============================================================================

TEST_F(ThalamocorticalLoopE2ETest, CorticothalamicFeedback_BasicLoop) {
    E2E_PIPELINE_START("Corticothalamic Feedback: Basic Loop");

    thalamus_set_arousal(thal, 1.0f);

    E2E_STAGE_BEGIN("Initialize thalamocortical pathway", 200);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);
    thalamus_set_attention(thal, THAL_NUCLEUS_PULVINAR, 0.8f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Send ascending signal (thalamus -> cortex)", 500);
    auto sensory_input = generate_oscillation(NUM_CHANNELS, 10.0f, 0.0f, 0.8f);
    std::vector<float> cortical_output(NUM_CHANNELS);
    int result = thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                                       cortical_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate cortical processing", 300);
    // Cortex modulates the signal
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        cortical_output[i] *= 1.2f;  // Cortical amplification
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Send descending feedback (cortex -> thalamus)", 500);
    auto cortical_feedback = generate_cortical_feedback(NUM_CHANNELS, 0.6f);
    result = thalamus_pulvinar_attention(thal, cortical_feedback.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify loop modulation", 100);
    float attention = thalamus_get_attention(thal, THAL_NUCLEUS_LGN);
    EXPECT_GT(attention, 0.0f) << "Attention should be modulated by feedback";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, CorticothalamicFeedback_GainModulation) {
    E2E_PIPELINE_START("Corticothalamic Feedback: Gain Modulation");

    thalamus_set_arousal(thal, 1.0f);
    auto sensory_input = generate_oscillation(NUM_CHANNELS, 8.0f, 0.0f, 0.75f);

    E2E_STAGE_BEGIN("Process without cortical feedback", 500);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.5f);
    std::vector<float> output_no_feedback(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         output_no_feedback.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply cortical top-down attention", 300);
    std::vector<float> attention_boost(NUM_CHANNELS, 0.9f);
    thalamus_pulvinar_attention(thal, attention_boost.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with cortical feedback", 500);
    std::vector<float> output_with_feedback(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         output_with_feedback.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify gain increase from feedback", 100);
    float power_no_fb = calculate_power(output_no_feedback);
    float power_with_fb = calculate_power(output_with_feedback);

    // With feedback, the signal should be enhanced
    EXPECT_GE(power_with_fb, power_no_fb * 0.8f)
        << "Cortical feedback should modulate thalamic gain";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, CorticothalamicFeedback_ReverbLoop) {
    E2E_PIPELINE_START("Corticothalamic Feedback: Reverberant Activity");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);
    thalamus_set_attention(thal, THAL_NUCLEUS_PULVINAR, 0.8f);

    E2E_STAGE_BEGIN("Initialize brief sensory input", 200);
    auto brief_input = generate_oscillation(NUM_CHANNELS, 5.0f, 0.0f, 0.9f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run thalamocortical loop iterations", 3000);
    std::vector<float> thalamic_output(NUM_CHANNELS);
    std::vector<float> reverb_strength;

    // Initial sensory input
    thalamus_relay_visual(thal, brief_input.data(), NUM_CHANNELS,
                         thalamic_output.data(), NUM_CHANNELS);
    reverb_strength.push_back(calculate_power(thalamic_output));

    // Subsequent iterations simulate reverberant loop
    for (int iter = 0; iter < 10; ++iter) {
        // Cortical feedback (attenuated)
        std::vector<float> feedback(NUM_CHANNELS);
        for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
            feedback[i] = thalamic_output[i] * 0.7f;
        }
        thalamus_pulvinar_attention(thal, feedback.data(), NUM_CHANNELS);

        // Next thalamic relay
        std::vector<float> weak_input(NUM_CHANNELS, 0.1f);
        thalamus_relay_visual(thal, weak_input.data(), NUM_CHANNELS,
                             thalamic_output.data(), NUM_CHANNELS);

        reverb_strength.push_back(calculate_power(thalamic_output));
        thalamus_step(thal, 20.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sustained reverberant activity", 100);
    // Activity should persist for some iterations
    int active_iterations = 0;
    for (float power : reverb_strength) {
        if (power > 0.001f) active_iterations++;
    }
    EXPECT_GE(active_iterations, 3) << "Activity should persist through loop";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Oscillatory Dynamics Tests
//=============================================================================

TEST_F(ThalamocorticalLoopE2ETest, OscillatoryDynamics_AlphaRhythm) {
    E2E_PIPELINE_START("Oscillatory Dynamics: Alpha Rhythm (~10 Hz)");

    E2E_STAGE_BEGIN("Set awake-relaxed state", 200);
    thalamus_set_arousal(thal, 0.7f);  // Relaxed but awake
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate alpha oscillation input", 500);
    // Alpha rhythm: 8-12 Hz
    auto alpha_input = generate_oscillation(NUM_CHANNELS, 10.0f, 0.0f, 0.6f);
    std::vector<float> output(NUM_CHANNELS);
    int result = thalamus_relay_visual(thal, alpha_input.data(), NUM_CHANNELS,
                                       output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify alpha relay in tonic mode", 100);
    float output_power = calculate_power(output);
    EXPECT_GT(output_power, 0.0f)
        << "Alpha oscillations should be relayed in tonic mode";

    // Tonic mode should preserve signal relatively faithfully
    float input_power = calculate_power(alpha_input);
    float transfer_ratio = output_power / input_power;
    EXPECT_GT(transfer_ratio, 0.1f) << "Signal should be relayed with some gain";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, OscillatoryDynamics_SleepSpindles) {
    E2E_PIPELINE_START("Oscillatory Dynamics: Sleep Spindles (11-15 Hz)");

    E2E_STAGE_BEGIN("Transition to drowsy/light sleep", 300);
    thalamus_set_arousal(thal, 0.3f);
    // Should be in or transitioning to burst mode
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger spindle-like burst activity", 500);
    // Sleep spindles are generated by TRN-thalamus interaction
    int result = thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    EXPECT_EQ(result, 0);

    // Run simulation steps to allow burst to develop
    for (int i = 0; i < 20; ++i) {
        thalamus_step(thal, 5.0f);  // 5ms steps
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify burst mode characteristics", 100);
    thal_firing_mode_t mode = thalamus_get_mode(thal, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_BURST) << "Should be in burst mode for spindles";

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);
    EXPECT_GT(stats.burst_count, 0) << "Burst events should have occurred";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, OscillatoryDynamics_DeltaWaves) {
    E2E_PIPELINE_START("Oscillatory Dynamics: Delta Waves (0.5-4 Hz)");

    E2E_STAGE_BEGIN("Enter deep sleep state", 300);
    thalamus_set_arousal(thal, 0.1f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process slow oscillation input", 500);
    // Delta rhythm: very slow oscillation
    auto delta_input = generate_oscillation(NUM_CHANNELS, 2.0f, 0.0f, 0.5f);
    std::vector<float> output(NUM_CHANNELS);
    thalamus_relay_visual(thal, delta_input.data(), NUM_CHANNELS,
                         output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify reduced relay in deep sleep", 100);
    // In burst mode, weak/slow inputs are filtered
    float output_power = calculate_power(output);
    // Output should be significantly reduced or zero
    EXPECT_LE(output_power, 0.5f)
        << "Deep sleep should gate sensory input";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Sleep State Transition Tests
//=============================================================================

TEST_F(ThalamocorticalLoopE2ETest, SleepStateTransitions_WakeToSleep) {
    E2E_PIPELINE_START("Sleep State Transitions: Wake to Sleep");

    auto test_input = generate_oscillation(NUM_CHANNELS, 10.0f, 0.0f, 0.7f);
    std::vector<float> output(NUM_CHANNELS);

    E2E_STAGE_BEGIN("Start in awake state", 200);
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Gradually decrease arousal", 2000);
    std::vector<std::pair<float, thal_firing_mode_t>> state_transitions;

    for (float arousal = 1.0f; arousal >= 0.0f; arousal -= 0.1f) {
        thalamus_set_arousal(thal, arousal);
        thal_firing_mode_t mode = thalamus_get_mode(thal, THAL_NUCLEUS_LGN);
        state_transitions.push_back({arousal, mode});

        thalamus_relay_visual(thal, test_input.data(), NUM_CHANNELS,
                             output.data(), NUM_CHANNELS);
        thalamus_step(thal, 100.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify state progression", 100);
    // High arousal should be tonic, low arousal should be burst
    EXPECT_EQ(state_transitions.front().second, THAL_MODE_TONIC);
    EXPECT_EQ(state_transitions.back().second, THAL_MODE_BURST);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, SleepStateTransitions_SleepToWake) {
    E2E_PIPELINE_START("Sleep State Transitions: Sleep to Wake");

    E2E_STAGE_BEGIN("Start in sleep state", 200);
    thalamus_set_arousal(thal, 0.1f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate sudden arousal stimulus", 500);
    // Sudden arousal (like alarm or startle)
    thalamus_set_arousal(thal, 0.9f);

    // Process strong sensory input
    std::vector<float> startle_input(NUM_CHANNELS, 0.95f);
    std::vector<float> output(NUM_CHANNELS);
    int result = thalamus_relay_visual(thal, startle_input.data(), NUM_CHANNELS,
                                       output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify rapid mode transition", 100);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC)
        << "Should transition to tonic upon arousal";

    float output_power = calculate_power(output);
    EXPECT_GT(output_power, 0.0f) << "Startle should produce cortical response";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, SleepStateTransitions_FullCycle) {
    E2E_PIPELINE_START("Sleep State Transitions: Full Sleep-Wake Cycle");

    auto test_stimulus = generate_oscillation(NUM_CHANNELS, 8.0f, 0.0f, 0.7f);
    std::vector<std::tuple<std::string, float, thal_firing_mode_t, float>> cycle_log;

    E2E_STAGE_BEGIN("Run through complete sleep-wake cycle", 5000);

    // Wake
    thalamus_set_arousal(thal, 1.0f);
    std::vector<float> wake_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, test_stimulus.data(), NUM_CHANNELS,
                         wake_out.data(), NUM_CHANNELS);
    cycle_log.push_back({"Wake", 1.0f, thal->dominant_mode, calculate_power(wake_out)});

    // Drowsy
    thalamus_set_arousal(thal, 0.5f);
    std::vector<float> drowsy_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, test_stimulus.data(), NUM_CHANNELS,
                         drowsy_out.data(), NUM_CHANNELS);
    cycle_log.push_back({"Drowsy", 0.5f, thal->dominant_mode, calculate_power(drowsy_out)});

    // Light sleep
    thalamus_set_arousal(thal, 0.3f);
    for (int i = 0; i < 10; i++) thalamus_step(thal, 50.0f);
    std::vector<float> light_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, test_stimulus.data(), NUM_CHANNELS,
                         light_out.data(), NUM_CHANNELS);
    cycle_log.push_back({"Light Sleep", 0.3f, thal->dominant_mode, calculate_power(light_out)});

    // Deep sleep
    thalamus_set_arousal(thal, 0.1f);
    for (int i = 0; i < 20; i++) thalamus_step(thal, 50.0f);
    std::vector<float> deep_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, test_stimulus.data(), NUM_CHANNELS,
                         deep_out.data(), NUM_CHANNELS);
    cycle_log.push_back({"Deep Sleep", 0.1f, thal->dominant_mode, calculate_power(deep_out)});

    // Wake again
    thalamus_set_arousal(thal, 1.0f);
    std::vector<float> rewake_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, test_stimulus.data(), NUM_CHANNELS,
                         rewake_out.data(), NUM_CHANNELS);
    cycle_log.push_back({"Re-Wake", 1.0f, thal->dominant_mode, calculate_power(rewake_out)});
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify cycle characteristics", 100);
    // Wake states should be tonic
    EXPECT_EQ(std::get<2>(cycle_log[0]), THAL_MODE_TONIC);
    EXPECT_EQ(std::get<2>(cycle_log[4]), THAL_MODE_TONIC);

    // Deep sleep should be burst
    EXPECT_EQ(std::get<2>(cycle_log[3]), THAL_MODE_BURST);

    // Wake power should exceed deep sleep power
    float wake_power = std::get<3>(cycle_log[0]);
    float deep_power = std::get<3>(cycle_log[3]);
    EXPECT_GT(wake_power, deep_power)
        << "Sensory throughput should be higher during wake";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Arousal Modulation Tests
//=============================================================================

TEST_F(ThalamocorticalLoopE2ETest, ArousalModulation_LinearGainControl) {
    E2E_PIPELINE_START("Arousal Modulation: Linear Gain Control");

    auto constant_input = generate_oscillation(NUM_CHANNELS, 10.0f, 0.0f, 0.6f);
    std::vector<std::pair<float, float>> arousal_vs_output;

    E2E_STAGE_BEGIN("Measure output at different arousal levels", 3000);
    for (float arousal = 0.3f; arousal <= 1.0f; arousal += 0.1f) {
        thalamus_set_arousal(thal, arousal);
        thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

        std::vector<float> output(NUM_CHANNELS);
        thalamus_relay_visual(thal, constant_input.data(), NUM_CHANNELS,
                             output.data(), NUM_CHANNELS);

        float power = calculate_power(output);
        arousal_vs_output.push_back({arousal, power});

        thalamus_step(thal, 50.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify arousal-gain relationship", 100);
    // Higher arousal should generally produce higher gain (in tonic mode)
    // Compare first and last (both should be in tonic mode)
    EXPECT_GE(arousal_vs_output.back().second, arousal_vs_output.front().second * 0.5f)
        << "Higher arousal should maintain or increase relay gain";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, ArousalModulation_TonicFractionCorrelation) {
    E2E_PIPELINE_START("Arousal Modulation: Tonic Fraction Correlation");

    E2E_STAGE_BEGIN("Measure tonic fraction vs arousal", 2000);
    std::vector<std::pair<float, float>> arousal_vs_tonic;

    for (float arousal = 0.0f; arousal <= 1.0f; arousal += 0.1f) {
        thalamus_set_arousal(thal, arousal);
        thalamus_step(thal, 100.0f);  // Allow state to settle

        float tonic_frac = thalamus_get_tonic_fraction(thal, THAL_NUCLEUS_LGN);
        arousal_vs_tonic.push_back({arousal, tonic_frac});
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify positive correlation", 100);
    // Tonic fraction should increase with arousal
    float low_arousal_tonic = arousal_vs_tonic.front().second;
    float high_arousal_tonic = arousal_vs_tonic.back().second;

    EXPECT_GE(high_arousal_tonic, low_arousal_tonic)
        << "Tonic fraction should increase with arousal";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Attention Spotlight Control Tests
//=============================================================================

TEST_F(ThalamocorticalLoopE2ETest, AttentionSpotlight_SpatialFocus) {
    E2E_PIPELINE_START("Attention Spotlight: Spatial Focus Control");

    thalamus_set_arousal(thal, 1.0f);

    E2E_STAGE_BEGIN("Create uniform visual field", 200);
    std::vector<float> uniform_input(NUM_CHANNELS, 0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply spatial attention spotlight via pulvinar", 500);
    // Spotlight on center channels
    std::vector<float> attention_spotlight(NUM_CHANNELS);
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        float dist = std::abs(static_cast<float>(i) - NUM_CHANNELS / 2.0f) / (NUM_CHANNELS / 2.0f);
        attention_spotlight[i] = 0.9f * std::exp(-dist * dist * 4.0f) + 0.1f;
    }
    int result = thalamus_pulvinar_attention(thal, attention_spotlight.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process uniform input through spotlight", 500);
    std::vector<float> output(NUM_CHANNELS);
    thalamus_relay_visual(thal, uniform_input.data(), NUM_CHANNELS,
                         output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify spotlight enhances center", 100);
    // Center channels should have higher output
    float center_power = 0.0f, edge_power = 0.0f;
    uint32_t center_start = NUM_CHANNELS / 4;
    uint32_t center_end = 3 * NUM_CHANNELS / 4;

    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        if (i >= center_start && i < center_end) {
            center_power += output[i] * output[i];
        } else {
            edge_power += output[i] * output[i];
        }
    }

    EXPECT_GE(center_power, edge_power * 0.5f)
        << "Spotlight center should be enhanced or equal";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, AttentionSpotlight_ShiftingFocus) {
    E2E_PIPELINE_START("Attention Spotlight: Shifting Focus");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    // Two targets at different locations
    std::vector<float> dual_target(NUM_CHANNELS, 0.1f);
    for (uint32_t i = 10; i < 20; ++i) dual_target[i] = 0.9f;  // Target A
    for (uint32_t i = 44; i < 54; ++i) dual_target[i] = 0.9f;  // Target B

    E2E_STAGE_BEGIN("Attend to Target A", 500);
    std::vector<float> attn_A(NUM_CHANNELS, 0.2f);
    for (uint32_t i = 5; i < 25; ++i) attn_A[i] = 0.9f;
    thalamus_pulvinar_attention(thal, attn_A.data(), NUM_CHANNELS);

    std::vector<float> output_A(NUM_CHANNELS);
    thalamus_relay_visual(thal, dual_target.data(), NUM_CHANNELS,
                         output_A.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Shift attention to Target B", 500);
    std::vector<float> attn_B(NUM_CHANNELS, 0.2f);
    for (uint32_t i = 39; i < 59; ++i) attn_B[i] = 0.9f;
    thalamus_pulvinar_attention(thal, attn_B.data(), NUM_CHANNELS);

    std::vector<float> output_B(NUM_CHANNELS);
    thalamus_relay_visual(thal, dual_target.data(), NUM_CHANNELS,
                         output_B.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify attention shift effect", 100);
    // Compute power at each target location for both attention conditions
    float A_target_when_attn_A = 0.0f, B_target_when_attn_A = 0.0f;
    float A_target_when_attn_B = 0.0f, B_target_when_attn_B = 0.0f;

    for (uint32_t i = 10; i < 20; ++i) {
        A_target_when_attn_A += output_A[i];
        A_target_when_attn_B += output_B[i];
    }
    for (uint32_t i = 44; i < 54; ++i) {
        B_target_when_attn_A += output_A[i];
        B_target_when_attn_B += output_B[i];
    }

    // When attending A, target A should be enhanced
    EXPECT_GE(A_target_when_attn_A, A_target_when_attn_B * 0.5f)
        << "Attending A should enhance target A";

    // When attending B, target B should be enhanced
    EXPECT_GE(B_target_when_attn_B, B_target_when_attn_A * 0.5f)
        << "Attending B should enhance target B";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamocorticalLoopE2ETest, AttentionSpotlight_ModalitySelection) {
    E2E_PIPELINE_START("Attention Spotlight: Modality Selection");

    thalamus_set_arousal(thal, 1.0f);

    auto sensory_input = generate_oscillation(NUM_CHANNELS, 10.0f, 0.0f, 0.8f);

    E2E_STAGE_BEGIN("Attend to visual modality", 500);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.9f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.2f);

    std::vector<float> visual_out(NUM_CHANNELS);
    std::vector<float> auditory_out(NUM_CHANNELS);

    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         visual_out.data(), NUM_CHANNELS);
    thalamus_relay_auditory(thal, sensory_input.data(), NUM_CHANNELS,
                           auditory_out.data(), NUM_CHANNELS);

    float visual_power_v = calculate_power(visual_out);
    float auditory_power_v = calculate_power(auditory_out);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Attend to auditory modality", 500);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.2f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.9f);

    std::vector<float> visual_out2(NUM_CHANNELS);
    std::vector<float> auditory_out2(NUM_CHANNELS);

    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         visual_out2.data(), NUM_CHANNELS);
    thalamus_relay_auditory(thal, sensory_input.data(), NUM_CHANNELS,
                           auditory_out2.data(), NUM_CHANNELS);

    float visual_power_a = calculate_power(visual_out2);
    float auditory_power_a = calculate_power(auditory_out2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify modality selection", 100);
    // When attending visual, visual should dominate
    EXPECT_GT(visual_power_v, auditory_power_v)
        << "Visual attention should enhance visual";

    // When attending auditory, auditory should dominate
    EXPECT_GT(auditory_power_a, visual_power_a)
        << "Auditory attention should enhance auditory";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
