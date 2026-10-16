//=============================================================================
// test_sensory_relay_e2e.cpp - Sensory Relay End-to-End Tests
//=============================================================================
/**
 * @file test_sensory_relay_e2e.cpp
 * @brief End-to-end tests for thalamic sensory relay pathways
 *
 * WHAT: Full pipeline tests for sensory relay through thalamic nuclei
 * WHY:  Verify complete sensory pathways (visual, auditory, somatosensory)
 * HOW:  Test realistic sensory processing scenarios with attention and TRN gating
 *
 * TEST COVERAGE:
 * - Full visual pathway (retina -> LGN -> V1)
 * - Auditory pathway (cochlea -> MGN -> A1)
 * - Somatosensory pathway (receptors -> VPL -> S1)
 * - Attentional modulation of relay
 * - TRN gating of sensory flow
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

//=============================================================================
// Test Fixture
//=============================================================================

class SensoryRelayE2ETest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    static constexpr uint32_t NUM_CHANNELS = 64;
    static constexpr uint32_t NUM_NEURONS = 128;

    void SetUp() override {
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = NUM_NEURONS;
        config.channels_per_nucleus = NUM_CHANNELS;
        config.enable_trn = true;
        config.enable_mode_switching = true;
        config.enable_attention_gating = true;
        thal = thalamus_create(&config);
        ASSERT_NE(thal, nullptr) << "Failed to create thalamus";
    }

    void TearDown() override {
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }

    // Helper: Generate Gaussian stimulus centered at position
    std::vector<float> generate_gaussian_stimulus(uint32_t size, float center,
                                                   float sigma, float amplitude) {
        std::vector<float> stimulus(size);
        for (uint32_t i = 0; i < size; ++i) {
            float x = static_cast<float>(i) / size;
            float dist = (x - center) / sigma;
            stimulus[i] = amplitude * std::exp(-0.5f * dist * dist);
        }
        return stimulus;
    }

    // Helper: Generate edge stimulus
    std::vector<float> generate_edge_stimulus(uint32_t size, float edge_pos,
                                               float contrast) {
        std::vector<float> stimulus(size);
        for (uint32_t i = 0; i < size; ++i) {
            float x = static_cast<float>(i) / size;
            stimulus[i] = (x < edge_pos) ? contrast : 0.0f;
        }
        return stimulus;
    }

    // Helper: Calculate signal power
    float calculate_power(const std::vector<float>& signal) {
        float sum = 0.0f;
        for (float v : signal) {
            sum += v * v;
        }
        return sum / signal.size();
    }

    // Helper: Find peak position
    uint32_t find_peak(const std::vector<float>& signal) {
        auto it = std::max_element(signal.begin(), signal.end());
        return static_cast<uint32_t>(std::distance(signal.begin(), it));
    }
};

//=============================================================================
// Visual Pathway Tests (Retina -> LGN -> V1)
//=============================================================================

TEST_F(SensoryRelayE2ETest, FullVisualPathway_RetinaToV1) {
    E2E_PIPELINE_START("Visual Pathway: Retina -> LGN -> V1");

    E2E_STAGE_BEGIN("Generate retinal input", 100);
    auto retinal_input = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f, 0.9f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Set high arousal for tonic mode", 100);
    int result = thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Relay through LGN", 500);
    std::vector<float> v1_output(NUM_CHANNELS);
    result = thalamus_relay_visual(thal, retinal_input.data(), NUM_CHANNELS,
                                   v1_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify output characteristics", 100);
    // Output should have signal at center
    uint32_t input_peak = find_peak(retinal_input);
    uint32_t output_peak = find_peak(v1_output);
    EXPECT_NEAR(input_peak, output_peak, 5) << "Peak position should be preserved";

    // Output should have non-zero power
    float output_power = calculate_power(v1_output);
    EXPECT_GT(output_power, 0.0f) << "V1 should receive signal";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, VisualPathway_ContrastPreservation) {
    E2E_PIPELINE_START("Visual Pathway: Contrast Preservation");

    E2E_STAGE_BEGIN("Set awake state", 100);
    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process high contrast stimulus", 500);
    auto high_contrast = generate_edge_stimulus(NUM_CHANNELS, 0.5f, 0.9f);
    std::vector<float> high_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, high_contrast.data(), NUM_CHANNELS,
                         high_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process low contrast stimulus", 500);
    auto low_contrast = generate_edge_stimulus(NUM_CHANNELS, 0.5f, 0.3f);
    std::vector<float> low_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, low_contrast.data(), NUM_CHANNELS,
                         low_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify contrast difference preserved", 100);
    float high_power = calculate_power(high_output);
    float low_power = calculate_power(low_output);
    EXPECT_GT(high_power, low_power) << "High contrast should produce stronger output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, VisualPathway_MultipleStimuli) {
    E2E_PIPELINE_START("Visual Pathway: Multiple Stimuli Processing");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    E2E_STAGE_BEGIN("Process sequence of visual stimuli", 2000);
    std::vector<std::vector<float>> outputs;

    for (int i = 0; i < 5; ++i) {
        float center = 0.2f + 0.15f * i;  // Moving stimulus
        auto stimulus = generate_gaussian_stimulus(NUM_CHANNELS, center, 0.08f, 0.8f);
        std::vector<float> output(NUM_CHANNELS);

        int result = thalamus_relay_visual(thal, stimulus.data(), NUM_CHANNELS,
                                          output.data(), NUM_CHANNELS);
        EXPECT_EQ(result, 0);
        outputs.push_back(output);

        thalamus_step(thal, 10.0f);  // 10ms between stimuli
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all stimuli processed", 100);
    for (size_t i = 0; i < outputs.size(); ++i) {
        float power = calculate_power(outputs[i]);
        EXPECT_GT(power, 0.0f) << "Stimulus " << i << " should produce output";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Auditory Pathway Tests (Cochlea -> MGN -> A1)
//=============================================================================

TEST_F(SensoryRelayE2ETest, FullAuditoryPathway_CochleaToA1) {
    E2E_PIPELINE_START("Auditory Pathway: Cochlea -> MGN -> A1");

    E2E_STAGE_BEGIN("Generate cochlear input (frequency-specific)", 100);
    // Simulate tonotopic input (specific frequency activation)
    auto cochlear_input = generate_gaussian_stimulus(NUM_CHANNELS, 0.3f, 0.05f, 0.85f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Configure MGN for auditory processing", 200);
    thalamus_set_arousal(thal, 0.9f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.75f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Relay through MGN", 500);
    std::vector<float> a1_output(NUM_CHANNELS);
    int result = thalamus_relay_auditory(thal, cochlear_input.data(), NUM_CHANNELS,
                                         a1_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify tonotopic preservation", 100);
    uint32_t input_peak = find_peak(cochlear_input);
    uint32_t output_peak = find_peak(a1_output);
    EXPECT_NEAR(input_peak, output_peak, 3) << "Tonotopic mapping preserved";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, AuditoryPathway_TemporalPattern) {
    E2E_PIPELINE_START("Auditory Pathway: Temporal Pattern Processing");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.8f);

    E2E_STAGE_BEGIN("Process rhythmic auditory pattern", 3000);
    std::vector<float> total_output(NUM_CHANNELS, 0.0f);

    // Simulate rhythmic input (like speech syllables)
    for (int beat = 0; beat < 8; ++beat) {
        auto pulse = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f,
                                                (beat % 2 == 0) ? 0.9f : 0.5f);
        std::vector<float> output(NUM_CHANNELS);

        thalamus_relay_auditory(thal, pulse.data(), NUM_CHANNELS,
                               output.data(), NUM_CHANNELS);

        for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
            total_output[i] += output[i];
        }

        thalamus_step(thal, 50.0f);  // 50ms between beats (20Hz rhythm)
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify temporal integration", 100);
    float avg_output = std::accumulate(total_output.begin(), total_output.end(), 0.0f)
                       / total_output.size();
    EXPECT_GT(avg_output, 0.0f) << "Temporal pattern should be integrated";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Somatosensory Pathway Tests (Receptors -> VPL -> S1)
//=============================================================================

TEST_F(SensoryRelayE2ETest, FullSomatosensoryPathway_ReceptorsToS1) {
    E2E_PIPELINE_START("Somatosensory Pathway: Receptors -> VPL -> S1");

    E2E_STAGE_BEGIN("Generate somatosensory receptor input", 100);
    // Simulate tactile input on finger (localized activation)
    auto receptor_input = generate_gaussian_stimulus(NUM_CHANNELS, 0.7f, 0.03f, 0.95f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Configure VPL for somatosensory relay", 200);
    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_VPL, 0.9f);  // High attention to touch
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Relay through VPL to S1", 500);
    std::vector<float> s1_output(NUM_CHANNELS);
    int result = thalamus_relay(thal, THAL_NUCLEUS_VPL, receptor_input.data(),
                                NUM_CHANNELS, s1_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify somatotopic mapping", 100);
    uint32_t input_peak = find_peak(receptor_input);
    uint32_t output_peak = find_peak(s1_output);
    EXPECT_NEAR(input_peak, output_peak, 4) << "Somatotopic mapping preserved";

    float output_power = calculate_power(s1_output);
    EXPECT_GT(output_power, 0.0f) << "S1 should receive tactile signal";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, SomatosensoryPathway_PainVsTouch) {
    E2E_PIPELINE_START("Somatosensory Pathway: Pain vs Touch Discrimination");

    thalamus_set_arousal(thal, 1.0f);

    E2E_STAGE_BEGIN("Process light touch", 500);
    auto light_touch = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f, 0.4f);
    std::vector<float> touch_output(NUM_CHANNELS);
    thalamus_set_attention(thal, THAL_NUCLEUS_VPL, 0.5f);
    thalamus_relay(thal, THAL_NUCLEUS_VPL, light_touch.data(), NUM_CHANNELS,
                  touch_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process painful stimulus (higher intensity)", 500);
    auto pain = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.15f, 0.95f);
    std::vector<float> pain_output(NUM_CHANNELS);
    thalamus_set_attention(thal, THAL_NUCLEUS_VPL, 0.95f);  // Pain grabs attention
    thalamus_relay(thal, THAL_NUCLEUS_VPL, pain.data(), NUM_CHANNELS,
                  pain_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify intensity discrimination", 100);
    float touch_power = calculate_power(touch_output);
    float pain_power = calculate_power(pain_output);
    EXPECT_GT(pain_power, touch_power) << "Pain should produce stronger cortical response";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Attentional Modulation Tests
//=============================================================================

TEST_F(SensoryRelayE2ETest, AttentionalModulation_VisualEnhancement) {
    E2E_PIPELINE_START("Attentional Modulation: Visual Enhancement");

    auto visual_input = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f, 0.7f);
    thalamus_set_arousal(thal, 1.0f);

    E2E_STAGE_BEGIN("Process with low attention", 500);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.2f);
    std::vector<float> low_attn_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                         low_attn_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Reset and process with high attention", 500);
    thalamus_reset(thal);
    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.95f);
    std::vector<float> high_attn_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                         high_attn_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify attention enhances signal", 100);
    float low_power = calculate_power(low_attn_output);
    float high_power = calculate_power(high_attn_output);
    EXPECT_GT(high_power, low_power)
        << "High attention should enhance visual signal (high=" << high_power
        << ", low=" << low_power << ")";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, AttentionalModulation_CrossModalInhibition) {
    E2E_PIPELINE_START("Attentional Modulation: Cross-Modal Inhibition");

    thalamus_set_arousal(thal, 1.0f);
    auto input = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f, 0.8f);

    E2E_STAGE_BEGIN("High visual attention, low auditory", 500);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.9f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.2f);

    std::vector<float> visual_output(NUM_CHANNELS);
    std::vector<float> auditory_output(NUM_CHANNELS);

    thalamus_relay_visual(thal, input.data(), NUM_CHANNELS,
                         visual_output.data(), NUM_CHANNELS);
    thalamus_relay_auditory(thal, input.data(), NUM_CHANNELS,
                           auditory_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify cross-modal attention effect", 100);
    float visual_power = calculate_power(visual_output);
    float auditory_power = calculate_power(auditory_output);

    EXPECT_GT(visual_power, auditory_power)
        << "Visual (attended) should be stronger than auditory (unattended)";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, AttentionalModulation_SpatialAttentionSpotlight) {
    E2E_PIPELINE_START("Attentional Modulation: Spatial Attention Spotlight");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    E2E_STAGE_BEGIN("Set up spatial attention via pulvinar", 300);
    // Attend to left visual field (channels 0-31)
    std::vector<float> attention_map(NUM_CHANNELS);
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        attention_map[i] = (i < NUM_CHANNELS / 2) ? 0.9f : 0.2f;
    }
    int result = thalamus_pulvinar_attention(thal, attention_map.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process bilateral visual stimuli", 500);
    // Two stimuli: one in attended field, one in unattended
    std::vector<float> visual_input(NUM_CHANNELS, 0.0f);
    auto left_stim = generate_gaussian_stimulus(NUM_CHANNELS / 2, 0.5f, 0.1f, 0.8f);
    auto right_stim = generate_gaussian_stimulus(NUM_CHANNELS / 2, 0.5f, 0.1f, 0.8f);

    for (uint32_t i = 0; i < NUM_CHANNELS / 2; ++i) {
        visual_input[i] = left_stim[i];
        visual_input[i + NUM_CHANNELS / 2] = right_stim[i];
    }

    std::vector<float> output(NUM_CHANNELS);
    thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                         output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify spatial attention spotlight", 100);
    // Left field (attended) should have stronger response
    float left_power = 0.0f, right_power = 0.0f;
    for (uint32_t i = 0; i < NUM_CHANNELS / 2; ++i) {
        left_power += output[i] * output[i];
        right_power += output[i + NUM_CHANNELS / 2] * output[i + NUM_CHANNELS / 2];
    }

    EXPECT_GE(left_power, right_power * 0.8f)
        << "Attended field should have stronger response";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TRN Gating Tests
//=============================================================================

TEST_F(SensoryRelayE2ETest, TRNGating_SensoryFlowControl) {
    E2E_PIPELINE_START("TRN Gating: Sensory Flow Control");

    auto visual_input = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f, 0.85f);
    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);

    E2E_STAGE_BEGIN("Process without TRN inhibition", 500);
    std::vector<float> uninhibited_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                         uninhibited_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply strong TRN inhibition", 300);
    int result = thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.9f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process with TRN inhibition", 500);
    std::vector<float> inhibited_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                         inhibited_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify TRN suppresses sensory flow", 100);
    float uninhibited_power = calculate_power(uninhibited_output);
    float inhibited_power = calculate_power(inhibited_output);

    EXPECT_GT(uninhibited_power, inhibited_power)
        << "TRN inhibition should reduce sensory throughput";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, TRNGating_ChannelSpecificInhibition) {
    E2E_PIPELINE_START("TRN Gating: Channel-Specific Inhibition");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);

    // Uniform input across all channels
    std::vector<float> uniform_input(NUM_CHANNELS, 0.7f);

    E2E_STAGE_BEGIN("Apply per-channel TRN inhibition", 500);
    // Inhibit first half of channels
    for (uint32_t i = 0; i < NUM_CHANNELS / 2; ++i) {
        int result = thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, i, 0.85f);
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process visual input", 500);
    std::vector<float> output(NUM_CHANNELS);
    thalamus_relay_visual(thal, uniform_input.data(), NUM_CHANNELS,
                         output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify selective channel suppression", 100);
    float first_half_power = 0.0f, second_half_power = 0.0f;
    for (uint32_t i = 0; i < NUM_CHANNELS / 2; ++i) {
        first_half_power += output[i] * output[i];
        second_half_power += output[i + NUM_CHANNELS / 2] * output[i + NUM_CHANNELS / 2];
    }

    EXPECT_LT(first_half_power, second_half_power)
        << "Inhibited channels should have weaker output";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, TRNGating_DistractorSuppression) {
    E2E_PIPELINE_START("TRN Gating: Distractor Suppression");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    E2E_STAGE_BEGIN("Create target and distractor stimuli", 100);
    // Target in center
    auto target = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.05f, 0.9f);
    // Distractor on the side
    auto distractor = generate_gaussian_stimulus(NUM_CHANNELS, 0.8f, 0.05f, 0.9f);

    std::vector<float> combined(NUM_CHANNELS);
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        combined[i] = std::max(target[i], distractor[i]);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply TRN inhibition to distractor location", 300);
    // Inhibit channels around distractor (channel ~51 for 0.8 position)
    uint32_t distractor_center = static_cast<uint32_t>(0.8f * NUM_CHANNELS);
    for (uint32_t i = distractor_center - 5; i <= distractor_center + 5; ++i) {
        if (i < NUM_CHANNELS) {
            thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, i, 0.8f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process combined stimulus", 500);
    std::vector<float> output(NUM_CHANNELS);
    thalamus_relay_visual(thal, combined.data(), NUM_CHANNELS,
                         output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify target enhanced relative to distractor", 100);
    uint32_t target_center = NUM_CHANNELS / 2;
    float target_response = 0.0f, distractor_response = 0.0f;

    for (int j = -3; j <= 3; ++j) {
        if (target_center + j < NUM_CHANNELS) {
            target_response += output[target_center + j];
        }
        if (distractor_center + j < NUM_CHANNELS) {
            distractor_response += output[distractor_center + j];
        }
    }

    EXPECT_GT(target_response, distractor_response)
        << "Target should be enhanced, distractor suppressed";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SensoryRelayE2ETest, MultimodalSensoryIntegration) {
    E2E_PIPELINE_START("Multimodal Sensory Integration");

    thalamus_set_arousal(thal, 1.0f);

    E2E_STAGE_BEGIN("Configure attention for audiovisual binding", 200);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.8f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process concurrent audiovisual stimuli", 1000);
    // Visual: flash at position 0.5
    auto visual_flash = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.08f, 0.9f);
    // Auditory: tone at corresponding frequency
    auto auditory_tone = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.08f, 0.9f);

    std::vector<float> v1_out(NUM_CHANNELS);
    std::vector<float> a1_out(NUM_CHANNELS);

    // Process both modalities in quick succession
    thalamus_relay_visual(thal, visual_flash.data(), NUM_CHANNELS,
                         v1_out.data(), NUM_CHANNELS);
    thalamus_step(thal, 5.0f);
    thalamus_relay_auditory(thal, auditory_tone.data(), NUM_CHANNELS,
                           a1_out.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify both modalities processed", 100);
    float v1_power = calculate_power(v1_out);
    float a1_power = calculate_power(a1_out);

    EXPECT_GT(v1_power, 0.0f) << "Visual should be processed";
    EXPECT_GT(a1_power, 0.0f) << "Auditory should be processed";

    // Peak positions should align (same spatial location)
    uint32_t v1_peak = find_peak(v1_out);
    uint32_t a1_peak = find_peak(a1_out);
    EXPECT_NEAR(v1_peak, a1_peak, 5) << "Spatial alignment for binding";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check thalamus statistics", 100);
    thalamus_stats_t stats;
    int result = thalamus_get_stats(thal, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_signals_relayed, 2) << "Should have relayed both signals";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(SensoryRelayE2ETest, SensoryAdaptation_RepeatedStimuli) {
    E2E_PIPELINE_START("Sensory Adaptation: Repeated Stimuli");

    thalamus_set_arousal(thal, 1.0f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    auto constant_stimulus = generate_gaussian_stimulus(NUM_CHANNELS, 0.5f, 0.1f, 0.8f);
    std::vector<float> response_powers;

    E2E_STAGE_BEGIN("Present repeated stimuli", 5000);
    for (int trial = 0; trial < 10; ++trial) {
        std::vector<float> output(NUM_CHANNELS);
        thalamus_relay_visual(thal, constant_stimulus.data(), NUM_CHANNELS,
                             output.data(), NUM_CHANNELS);

        float power = calculate_power(output);
        response_powers.push_back(power);

        thalamus_step(thal, 100.0f);  // 100ms between presentations
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sensory responses maintained", 100);
    // All responses should be non-zero
    for (size_t i = 0; i < response_powers.size(); ++i) {
        EXPECT_GT(response_powers[i], 0.0f)
            << "Response " << i << " should be non-zero";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
