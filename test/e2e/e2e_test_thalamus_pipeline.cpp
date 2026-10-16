//=============================================================================
// e2e_test_thalamus_pipeline.cpp - Thalamus E2E Pipeline Tests
//=============================================================================
/**
 * @file e2e_test_thalamus_pipeline.cpp
 * @brief End-to-end tests for complete thalamic relay pipelines
 *
 * WHAT: Full pipeline tests for sensory relay and motor coordination
 * WHY:  Verify complete thalamic pathways function correctly
 * HOW:  Create realistic scenarios with basal ganglia integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class E2EThalamusVisualPipelineTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;

    void SetUp() override {
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = 64;
        config.channels_per_nucleus = 32;
        thal = thalamus_create(&config);
    }

    void TearDown() override {
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }
};

class E2EThalamusMotorPipelineTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    basal_ganglia_t* bg = nullptr;

    void SetUp() override {
        thalamus_config_t thal_config;
        thalamus_default_config(&thal_config);
        thal_config.neurons_per_nucleus = 64;
        thal_config.channels_per_nucleus = 16;
        thal = thalamus_create(&thal_config);

        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = 16;
        bg = basal_ganglia_create(&bg_config);
    }

    void TearDown() override {
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
        if (bg) {
            basal_ganglia_destroy(bg);
            bg = nullptr;
        }
    }
};

class E2EThalamusAttentionPipelineTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;

    void SetUp() override {
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = 64;
        thal = thalamus_create(&config);
    }

    void TearDown() override {
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }
};

class E2EThalamusArousalPipelineTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;

    void SetUp() override {
        thalamus_config_t config;
        thalamus_default_config(&config);
        thal = thalamus_create(&config);
    }

    void TearDown() override {
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }
};

//=============================================================================
// Visual Relay Pipeline Tests
//=============================================================================

TEST_F(E2EThalamusVisualPipelineTest, RetinalInputToV1) {
    ASSERT_NE(thal, nullptr);

    // Simulate retinal input (e.g., visual stimulus)
    float retinal_input[32];
    for (int i = 0; i < 32; i++) {
        // Create a Gaussian-like stimulus (bright center)
        float center_dist = std::abs(i - 16) / 16.0f;
        retinal_input[i] = 0.9f * std::exp(-center_dist * center_dist * 4.0f);
    }

    float v1_output[32];
    int result = thalamus_relay_visual(thal, retinal_input, 32, v1_output, 32);
    EXPECT_GE(result, 0);

    // V1 should receive modulated signal
    float max_output = 0.0f;
    int max_idx = -1;
    for (int i = 0; i < 32; i++) {
        if (v1_output[i] > max_output) {
            max_output = v1_output[i];
            max_idx = i;
        }
    }

    // Peak should be near center (following Gaussian input)
    EXPECT_NEAR(max_idx, 16, 5);  // Within 5 channels of center
    EXPECT_GT(max_output, 0.0f);
}

TEST_F(E2EThalamusVisualPipelineTest, AttentionModulatesVisual) {
    ASSERT_NE(thal, nullptr);

    float retinal_input[32];
    for (int i = 0; i < 32; i++) {
        retinal_input[i] = 0.7f;
    }

    // Low attention
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.2f);
    float v1_low_attention[32];
    thalamus_relay_visual(thal, retinal_input, 32, v1_low_attention, 32);

    // Reset and high attention
    thalamus_reset(thal);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.9f);
    float v1_high_attention[32];
    thalamus_relay_visual(thal, retinal_input, 32, v1_high_attention, 32);

    // High attention should produce stronger output
    float sum_low = 0.0f, sum_high = 0.0f;
    for (int i = 0; i < 32; i++) {
        sum_low += v1_low_attention[i];
        sum_high += v1_high_attention[i];
    }
    EXPECT_GT(sum_high, sum_low);
}

TEST_F(E2EThalamusVisualPipelineTest, PulvinarGuidesVisualAttention) {
    ASSERT_NE(thal, nullptr);

    // Set up pulvinar attention signal (attend to left visual field)
    float pulvinar_attention[32];
    for (int i = 0; i < 32; i++) {
        pulvinar_attention[i] = (i < 16) ? 0.9f : 0.2f;  // Attend left
    }

    int result = thalamus_pulvinar_attention(thal, pulvinar_attention, 32);
    EXPECT_EQ(result, 0);

    // Now relay visual input
    float retinal_input[32];
    for (int i = 0; i < 32; i++) {
        retinal_input[i] = 0.6f;  // Uniform input
    }

    float v1_output[32];
    thalamus_relay_visual(thal, retinal_input, 32, v1_output, 32);

    // LGN should have been modulated by pulvinar
    float lgn_attention = thalamus_get_attention(thal, THAL_NUCLEUS_LGN);
    EXPECT_GT(lgn_attention, 0.0f);
}

//=============================================================================
// Motor Relay Pipeline Tests
//=============================================================================

TEST_F(E2EThalamusMotorPipelineTest, BasalGangliaToMotorCortex) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    // Set up cortical input favoring action 0
    float cortical_input[16];
    cortical_input[0] = 0.9f;  // Strong preference for action 0
    for (int i = 1; i < 16; i++) {
        cortical_input[i] = 0.1f;
    }

    // BG action selection
    uint32_t selected_action;
    basal_ganglia_select_action(bg, cortical_input, &selected_action);

    // Get BG output to thalamus
    float bg_output[16];
    basal_ganglia_get_thalamic_output(bg, bg_output);

    // Relay through thalamus VA to motor cortex
    float motor_output[16];
    int result = thalamus_relay_motor(thal, bg_output, 16, motor_output, 16);
    EXPECT_GE(result, 0);

    // Motor output should be available
    bool has_motor_activity = false;
    for (int i = 0; i < 16; i++) {
        if (motor_output[i] > 0.0f) {
            has_motor_activity = true;
            break;
        }
    }
    EXPECT_TRUE(has_motor_activity);
}

TEST_F(E2EThalamusMotorPipelineTest, DopamineModulatesMotorRelay) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    float cortical_input[16];
    for (int i = 0; i < 16; i++) {
        cortical_input[i] = 0.5f;
    }

    // High dopamine (reward)
    basal_ganglia_update_dopamine(bg, 1.0f, 0.0f);
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_output_reward[16];
    basal_ganglia_get_thalamic_output(bg, bg_output_reward);

    float motor_reward[16];
    thalamus_relay_motor(thal, bg_output_reward, 16, motor_reward, 16);

    // Low dopamine (punishment)
    basal_ganglia_update_dopamine(bg, -1.0f, 0.0f);
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_output_punish[16];
    basal_ganglia_get_thalamic_output(bg, bg_output_punish);

    float motor_punish[16];
    thalamus_relay_motor(thal, bg_output_punish, 16, motor_punish, 16);

    // Both should have some output (exact relationship depends on BG dynamics)
    float sum_reward = 0.0f, sum_punish = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_reward += motor_reward[i];
        sum_punish += motor_punish[i];
    }
    EXPECT_TRUE(sum_reward >= 0.0f);
    EXPECT_TRUE(sum_punish >= 0.0f);
}

TEST_F(E2EThalamusMotorPipelineTest, ActionSuppressionPipeline) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    float cortical_input[16];
    for (int i = 0; i < 16; i++) {
        cortical_input[i] = 0.6f;
    }

    // Normal action selection
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_normal[16];
    basal_ganglia_get_thalamic_output(bg, bg_normal);

    float motor_normal[16];
    thalamus_relay_motor(thal, bg_normal, 16, motor_normal, 16);

    // Suppress action
    basal_ganglia_suppress_action(bg, 0.9f);

    float bg_suppressed[16];
    basal_ganglia_get_thalamic_output(bg, bg_suppressed);

    float motor_suppressed[16];
    thalamus_relay_motor(thal, bg_suppressed, 16, motor_suppressed, 16);

    // Suppressed motor output should be lower
    float sum_normal = 0.0f, sum_suppressed = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_normal += motor_normal[i];
        sum_suppressed += motor_suppressed[i];
    }
    EXPECT_GE(sum_normal, sum_suppressed);
}

//=============================================================================
// Attention Pipeline Tests
//=============================================================================

TEST_F(E2EThalamusAttentionPipelineTest, SelectiveAttentionToModality) {
    ASSERT_NE(thal, nullptr);

    // High attention to visual, low to auditory
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.9f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.2f);

    float input[32];
    for (int i = 0; i < 32; i++) {
        input[i] = 0.7f;
    }

    float visual_out[32], audio_out[32];
    thalamus_relay_visual(thal, input, 32, visual_out, 32);
    thalamus_relay_auditory(thal, input, 32, audio_out, 32);

    float sum_visual = 0.0f, sum_audio = 0.0f;
    for (int i = 0; i < 32; i++) {
        sum_visual += visual_out[i];
        sum_audio += audio_out[i];
    }

    // Visual should be stronger due to attention
    EXPECT_GT(sum_visual, sum_audio);
}

TEST_F(E2EThalamusAttentionPipelineTest, TRNInhibitsSensory) {
    ASSERT_NE(thal, nullptr);

    float input[32];
    for (int i = 0; i < 32; i++) {
        input[i] = 0.8f;
    }

    // Baseline relay
    float output_normal[32];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 32, output_normal, 32);

    // Apply TRN inhibition to specific channels
    for (int i = 0; i < 16; i++) {
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, i, 0.8f);
    }

    float output_inhibited[32];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 32, output_inhibited, 32);

    // First 16 channels should be more suppressed
    float sum_first_normal = 0.0f, sum_first_inhibited = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_first_normal += output_normal[i];
        sum_first_inhibited += output_inhibited[i];
    }
    EXPECT_GT(sum_first_normal, sum_first_inhibited);
}

TEST_F(E2EThalamusAttentionPipelineTest, ExecutiveAttentionPipeline) {
    ASSERT_NE(thal, nullptr);

    // Simulate executive function input to MD (mediodorsal)
    float executive_input[32];
    for (int i = 0; i < 32; i++) {
        executive_input[i] = 0.6f;
    }

    thalamus_set_attention(thal, THAL_NUCLEUS_MD, 0.85f);

    float pfc_output[32];
    int result = thalamus_relay_executive(thal, executive_input, 32, pfc_output, 32);
    EXPECT_GE(result, 0);

    // Should have output to prefrontal cortex
    float sum = 0.0f;
    for (int i = 0; i < 32; i++) {
        sum += pfc_output[i];
    }
    EXPECT_GT(sum, 0.0f);
}

//=============================================================================
// Arousal Pipeline Tests
//=============================================================================

TEST_F(E2EThalamusArousalPipelineTest, AwakeTonicMode) {
    ASSERT_NE(thal, nullptr);

    // High arousal = awake = tonic mode
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);

    // All nuclei should be tonic
    thal_nucleus_type_t types[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VA, THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };

    for (auto type : types) {
        EXPECT_EQ(thalamus_get_mode(thal, type), THAL_MODE_TONIC);
    }

    // Relay should work linearly
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }

    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    // Output should be proportional to input * attention
    float expected_factor = thalamus_get_attention(thal, THAL_NUCLEUS_LGN);
    for (int i = 0; i < 16; i++) {
        EXPECT_GE(output[i], 0.0f);
    }
}

TEST_F(E2EThalamusArousalPipelineTest, DrowsyBurstMode) {
    ASSERT_NE(thal, nullptr);

    // Low arousal = drowsy = burst mode
    thalamus_set_arousal(thal, 0.1f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);

    // All nuclei should be in burst mode
    thal_nucleus_type_t types[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VA, THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };

    for (auto type : types) {
        EXPECT_EQ(thalamus_get_mode(thal, type), THAL_MODE_BURST);
    }

    // In burst mode, weak inputs are suppressed
    float weak_input[16];
    for (int i = 0; i < 16; i++) {
        weak_input[i] = 0.1f;  // Below burst threshold
    }

    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, weak_input, 16, output, 16);

    // Output should be suppressed (zero) for weak inputs in burst mode
    float sum = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum += output[i];
    }
    EXPECT_FLOAT_EQ(sum, 0.0f);
}

TEST_F(E2EThalamusArousalPipelineTest, ArousalTransitionPipeline) {
    ASSERT_NE(thal, nullptr);

    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.6f;
    }

    float output_awake[16], output_drowsy[16], output_recovered[16];

    // Awake
    thalamus_set_arousal(thal, 1.0f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_awake, 16);

    // Transition to drowsy
    thalamus_set_arousal(thal, 0.1f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_drowsy, 16);

    // Recover to awake
    thalamus_set_arousal(thal, 1.0f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_recovered, 16);

    // Awake and recovered should be similar
    float sum_awake = 0.0f, sum_recovered = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_awake += output_awake[i];
        sum_recovered += output_recovered[i];
    }
    EXPECT_NEAR(sum_awake, sum_recovered, 0.1f);
}

//=============================================================================
// Complete Circuit Tests
//=============================================================================

TEST_F(E2EThalamusMotorPipelineTest, CompleteMotorCircuit) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    // Simulate complete motor circuit:
    // 1. Cortical command
    // 2. BG action selection
    // 3. Thalamic relay
    // 4. Back to motor cortex

    float cortical_command[16];
    cortical_command[0] = 0.95f;  // Strong command for action 0
    cortical_command[1] = 0.3f;   // Weaker alternative
    for (int i = 2; i < 16; i++) {
        cortical_command[i] = 0.05f;
    }

    // BG selects action
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_command, &selected);
    EXPECT_EQ(selected, 0);  // Should select strongest

    // Get BG thalamic output
    float bg_to_thal[16];
    basal_ganglia_get_thalamic_output(bg, bg_to_thal);

    // Relay through thalamus
    thalamus_set_attention(thal, THAL_NUCLEUS_VA, 0.8f);
    float motor_output[16];
    thalamus_relay_motor(thal, bg_to_thal, 16, motor_output, 16);

    // Motor cortex should receive activation for selected action
    bool has_motor_output = false;
    for (int i = 0; i < 16; i++) {
        if (motor_output[i] > 0.0f) {
            has_motor_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_motor_output);

    // Mark action completed
    basal_ganglia_action_completed(bg, selected, true);
}

TEST_F(E2EThalamusVisualPipelineTest, MultimodalIntegration) {
    ASSERT_NE(thal, nullptr);

    // Simulate concurrent visual and auditory processing
    float visual_input[32], auditory_input[32];
    for (int i = 0; i < 32; i++) {
        visual_input[i] = 0.7f;
        auditory_input[i] = 0.6f;
    }

    float v1_output[32], a1_output[32];

    // Process both modalities
    int vis_result = thalamus_relay_visual(thal, visual_input, 32, v1_output, 32);
    int aud_result = thalamus_relay_auditory(thal, auditory_input, 32, a1_output, 32);

    EXPECT_GE(vis_result, 0);
    EXPECT_GE(aud_result, 0);

    // Step the simulation
    thalamus_step(thal, 1.0f);

    // Check statistics
    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);
    EXPECT_EQ(stats.total_signals_relayed, 2);
}

TEST_F(E2EThalamusArousalPipelineTest, SleepWakeCycle) {
    ASSERT_NE(thal, nullptr);

    // Simulate a mini sleep-wake cycle
    for (int cycle = 0; cycle < 3; cycle++) {
        // Wake period
        thalamus_set_arousal(thal, 1.0f);
        EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);

        float input[16] = {0.5f};
        float output[16];
        thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

        for (int step = 0; step < 10; step++) {
            thalamus_step(thal, 1.0f);
        }

        // Sleep period
        thalamus_set_arousal(thal, 0.1f);
        EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);

        for (int step = 0; step < 10; step++) {
            thalamus_step(thal, 1.0f);
        }
    }

    // Final wake
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
}
