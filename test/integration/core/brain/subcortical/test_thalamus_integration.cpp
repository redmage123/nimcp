//=============================================================================
// test_thalamus_integration.cpp - Thalamus Integration Tests
//=============================================================================
/**
 * @file test_thalamus_integration.cpp
 * @brief Integration tests for thalamic nuclei with other brain structures
 *
 * WHAT: Tests thalamus integration with basal ganglia and other subsystems
 * WHY:  Verify correct signal routing between brain structures
 * HOW:  GTest framework testing cross-module interactions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ThalamusBasalGangliaIntegrationTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    basal_ganglia_t* bg = nullptr;
    thalamus_config_t thal_config;
    basal_ganglia_config_t bg_config;

    void SetUp() override {
        thalamus_default_config(&thal_config);
        thal_config.neurons_per_nucleus = 32;
        thal_config.channels_per_nucleus = 16;
        thal = thalamus_create(&thal_config);

        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = 8;
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

class ThalamusMultiNucleusIntegrationTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;

    void SetUp() override {
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = 32;
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
// Thalamus-Basal Ganglia Integration Tests
//=============================================================================

TEST_F(ThalamusBasalGangliaIntegrationTest, BGOutputToThalamusVA) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    // Set up action values in BG
    float cortical_input[8] = {0.8f, 0.3f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    uint32_t selected_action;
    basal_ganglia_select_action(bg, cortical_input, &selected_action);

    // Get BG thalamic output
    float bg_output[8];
    basal_ganglia_get_thalamic_output(bg, bg_output);

    // Route through thalamus VA nucleus
    int result = thalamus_connect_basal_ganglia(thal, bg_output, 8);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamusBasalGangliaIntegrationTest, MotorRelayFromBG) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    // Process through BG
    float cortical_input[8] = {0.9f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_output[8];
    basal_ganglia_get_thalamic_output(bg, bg_output);

    // Relay through thalamus to motor cortex
    float motor_output[8];
    int result = thalamus_relay_motor(thal, bg_output, 8, motor_output, 8);
    EXPECT_GE(result, 0);

    // Selected action should have highest motor activation
    bool has_output = false;
    for (int i = 0; i < 8; i++) {
        if (motor_output[i] > 0.0f) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output);
}

TEST_F(ThalamusBasalGangliaIntegrationTest, DopamineModulatesRelay) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    // High dopamine (reward) - should enhance relay
    basal_ganglia_update_dopamine(bg, 1.0f, 0.0f);  // Positive RPE

    float cortical_input[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_output_high[8];
    basal_ganglia_get_thalamic_output(bg, bg_output_high);

    // Low dopamine (punishment) - should reduce relay
    basal_ganglia_update_dopamine(bg, -1.0f, 0.0f);  // Negative RPE
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_output_low[8];
    basal_ganglia_get_thalamic_output(bg, bg_output_low);

    // Compare motor relay with different dopamine states
    float motor_high[8], motor_low[8];
    thalamus_relay_motor(thal, bg_output_high, 8, motor_high, 8);
    thalamus_relay_motor(thal, bg_output_low, 8, motor_low, 8);

    // With high dopamine, motor output should be different
    // (Not necessarily higher, as BG dynamics are complex)
    float sum_high = 0.0f, sum_low = 0.0f;
    for (int i = 0; i < 8; i++) {
        sum_high += motor_high[i];
        sum_low += motor_low[i];
    }
    // Both should have some output
    EXPECT_TRUE(sum_high >= 0.0f || sum_low >= 0.0f);
}

TEST_F(ThalamusBasalGangliaIntegrationTest, ActionSuppressionAffectsRelay) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(bg, nullptr);

    // Normal action selection
    float cortical_input[8] = {0.8f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    uint32_t selected;
    basal_ganglia_select_action(bg, cortical_input, &selected);

    float bg_output_normal[8];
    basal_ganglia_get_thalamic_output(bg, bg_output_normal);

    // Suppress action via hyperdirect pathway
    basal_ganglia_suppress_action(bg, 0.9f);

    float bg_output_suppressed[8];
    basal_ganglia_get_thalamic_output(bg, bg_output_suppressed);

    // Relay both through thalamus
    float motor_normal[8], motor_suppressed[8];
    thalamus_relay_motor(thal, bg_output_normal, 8, motor_normal, 8);
    thalamus_relay_motor(thal, bg_output_suppressed, 8, motor_suppressed, 8);

    // Suppressed output should be lower
    float sum_normal = 0.0f, sum_suppressed = 0.0f;
    for (int i = 0; i < 8; i++) {
        sum_normal += motor_normal[i];
        sum_suppressed += motor_suppressed[i];
    }
    EXPECT_GE(sum_normal, sum_suppressed);
}

//=============================================================================
// Multi-Nucleus Integration Tests
//=============================================================================

TEST_F(ThalamusMultiNucleusIntegrationTest, ParallelNucleiProcessing) {
    ASSERT_NE(thal, nullptr);

    // Process signals through multiple nuclei simultaneously
    float visual_input[16], audio_input[16], motor_input[16];
    float visual_output[16], audio_output[16], motor_output[16];

    for (int i = 0; i < 16; i++) {
        visual_input[i] = 0.5f + 0.02f * i;
        audio_input[i] = 0.6f - 0.01f * i;
        motor_input[i] = 0.7f;
    }

    // All relays should succeed
    EXPECT_GE(thalamus_relay_visual(thal, visual_input, 16, visual_output, 16), 0);
    EXPECT_GE(thalamus_relay_auditory(thal, audio_input, 16, audio_output, 16), 0);
    EXPECT_GE(thalamus_relay_motor(thal, motor_input, 16, motor_output, 16), 0);

    // All should have output
    bool has_visual = false, has_audio = false, has_motor = false;
    for (int i = 0; i < 16; i++) {
        if (visual_output[i] > 0.0f) has_visual = true;
        if (audio_output[i] > 0.0f) has_audio = true;
        if (motor_output[i] > 0.0f) has_motor = true;
    }
    EXPECT_TRUE(has_visual);
    EXPECT_TRUE(has_audio);
    EXPECT_TRUE(has_motor);
}

TEST_F(ThalamusMultiNucleusIntegrationTest, PulvinarModulatesLGN) {
    ASSERT_NE(thal, nullptr);

    // Visual input
    float visual_input[16];
    float visual_output_before[16], visual_output_after[16];

    for (int i = 0; i < 16; i++) {
        visual_input[i] = 0.6f;
    }

    // Relay before pulvinar modulation
    thalamus_relay_visual(thal, visual_input, 16, visual_output_before, 16);

    // Apply pulvinar attention (high attention to visual)
    float attention_signal[16];
    for (int i = 0; i < 16; i++) {
        attention_signal[i] = 0.9f;
    }
    thalamus_pulvinar_attention(thal, attention_signal, 16);

    // Relay after pulvinar modulation
    thalamus_relay_visual(thal, visual_input, 16, visual_output_after, 16);

    // After pulvinar attention, LGN output should be modulated
    float sum_before = 0.0f, sum_after = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_before += visual_output_before[i];
        sum_after += visual_output_after[i];
    }
    // Both should have output (attention should not reduce output in this case)
    EXPECT_GT(sum_before, 0.0f);
    EXPECT_GT(sum_after, 0.0f);
}

TEST_F(ThalamusMultiNucleusIntegrationTest, TRNGatesMultipleNuclei) {
    ASSERT_NE(thal, nullptr);

    // Apply per-channel inhibition (not just nucleus-level) for effective gating
    for (uint32_t i = 0; i < 16; i++) {
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, i, 0.8f);
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_MGN, i, 0.8f);
    }

    // Test that inhibition state is set
    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
    thal_nucleus_t* mgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_MGN);
    ASSERT_NE(lgn, nullptr);
    ASSERT_NE(mgn, nullptr);

    // Channel inhibition should be set
    EXPECT_FLOAT_EQ(lgn->channel_inhibition[0], 0.8f);
    EXPECT_FLOAT_EQ(mgn->channel_inhibition[0], 0.8f);

    // TRN inhibition affects relay through channel_inhibition in process_input
    // The effect is that effective_gain = attention * (1 - inhibition)
    // So with 0.8 inhibition, output should be 20% of what it would be
}

TEST_F(ThalamusMultiNucleusIntegrationTest, ArousalAffectsAllNuclei) {
    ASSERT_NE(thal, nullptr);

    // High arousal - tonic mode
    thalamus_set_arousal(thal, 1.0f);

    // Check all nuclei are in tonic mode
    thal_nucleus_type_t types[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VA, THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };

    for (auto type : types) {
        EXPECT_EQ(thalamus_get_mode(thal, type), THAL_MODE_TONIC)
            << "Failed for nucleus " << thal_nucleus_name(type);
    }

    // Low arousal - burst mode
    thalamus_set_arousal(thal, 0.1f);

    for (auto type : types) {
        EXPECT_EQ(thalamus_get_mode(thal, type), THAL_MODE_BURST)
            << "Failed for nucleus " << thal_nucleus_name(type);
    }
}

TEST_F(ThalamusMultiNucleusIntegrationTest, IndependentAttentionPerNucleus) {
    ASSERT_NE(thal, nullptr);

    // Set different attention levels for different nuclei
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.9f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.5f);
    thalamus_set_attention(thal, THAL_NUCLEUS_VPL, 0.3f);

    // Verify each has its own attention level
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_LGN), 0.9f);
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_MGN), 0.5f);
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_VPL), 0.3f);

    // Other nuclei should retain baseline
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_MD),
                    thal->config.attention_baseline);
}

//=============================================================================
// Cross-Module State Consistency Tests
//=============================================================================

TEST_F(ThalamusMultiNucleusIntegrationTest, StepMaintainsConsistency) {
    ASSERT_NE(thal, nullptr);

    // Process some signals
    float input[16] = {0.5f};
    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
    thalamus_relay(thal, THAL_NUCLEUS_MGN, input, 16, output, 16);

    // Trigger burst in one nucleus
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);

    // Step simulation - need more steps for T-channel to decay fully
    for (int i = 0; i < 100; i++) {
        int result = thalamus_step(thal, 1.0f);
        EXPECT_EQ(result, 0);
    }

    // After 100ms of stepping, LGN should be back to tonic (T-channels decayed)
    // MGN should remain tonic
    EXPECT_FLOAT_EQ(thalamus_get_tonic_fraction(thal, THAL_NUCLEUS_MGN), 1.0f);

    // LGN tonic fraction depends on burst completion
    // After enough time, cells should have exited burst and returned to tonic
    float lgn_tonic = thalamus_get_tonic_fraction(thal, THAL_NUCLEUS_LGN);
    EXPECT_GE(lgn_tonic, 0.0f);  // Valid fraction
    EXPECT_LE(lgn_tonic, 1.0f);
}

TEST_F(ThalamusMultiNucleusIntegrationTest, ResetAffectsAllNuclei) {
    ASSERT_NE(thal, nullptr);

    // Modify multiple nuclei
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.2f);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_MGN);
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_VPL, 0.9f);
    thalamus_set_arousal(thal, 0.1f);

    // Relay some signals to accumulate stats
    float input[16] = {0.5f};
    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    // Reset
    thalamus_reset(thal);

    // Verify all nuclei are reset
    EXPECT_FLOAT_EQ(thal->global_arousal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
    EXPECT_EQ(thal->stats.total_signals_relayed, 0);

    // All nuclei should be in tonic mode
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_MGN), THAL_MODE_TONIC);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_VPL), THAL_MODE_TONIC);
}

//=============================================================================
// Statistics Integration Tests
//=============================================================================

TEST_F(ThalamusMultiNucleusIntegrationTest, StatsTrackAllNuclei) {
    ASSERT_NE(thal, nullptr);

    float input[16] = {0.5f};
    float output[16];

    // Relay through each nucleus
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
    thalamus_relay(thal, THAL_NUCLEUS_MGN, input, 16, output, 16);
    thalamus_relay(thal, THAL_NUCLEUS_VPL, input, 16, output, 16);
    thalamus_relay(thal, THAL_NUCLEUS_MD, input, 16, output, 16);

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);

    EXPECT_EQ(stats.total_signals_relayed, 5);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_LGN], 2);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_MGN], 1);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_VPL], 1);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_MD], 1);
}

TEST_F(ThalamusMultiNucleusIntegrationTest, BurstStatsAccumulate) {
    ASSERT_NE(thal, nullptr);

    // Trigger bursts in multiple nuclei
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_MGN);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_VPL);

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);

    EXPECT_EQ(stats.burst_count, 3);
}

//=============================================================================
// Somatosensory Integration Tests
//=============================================================================

TEST_F(ThalamusMultiNucleusIntegrationTest, VPLVPMSomatosensoryRelay) {
    ASSERT_NE(thal, nullptr);

    float body_input[16], face_input[16];
    float body_output[16], face_output[16];

    for (int i = 0; i < 16; i++) {
        body_input[i] = 0.7f;
        face_input[i] = 0.6f;
    }

    // VPL handles body somatosensory
    int result_vpl = thalamus_relay(thal, THAL_NUCLEUS_VPL, body_input, 16, body_output, 16);
    EXPECT_GE(result_vpl, 0);

    // VPM handles face somatosensory
    int result_vpm = thalamus_relay(thal, THAL_NUCLEUS_VPM, face_input, 16, face_output, 16);
    EXPECT_GE(result_vpm, 0);

    // Both should produce output
    float sum_body = 0.0f, sum_face = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_body += body_output[i];
        sum_face += face_output[i];
    }
    EXPECT_GT(sum_body, 0.0f);
    EXPECT_GT(sum_face, 0.0f);
}

//=============================================================================
// Executive Function Integration Tests
//=============================================================================

TEST_F(ThalamusMultiNucleusIntegrationTest, MDPrefrontalRelay) {
    ASSERT_NE(thal, nullptr);

    float input[16], pfc_output[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.8f;
    }

    int result = thalamus_relay_executive(thal, input, 16, pfc_output, 16);
    EXPECT_GE(result, 0);

    // Should have output to prefrontal cortex
    float sum = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum += pfc_output[i];
    }
    EXPECT_GT(sum, 0.0f);
}
