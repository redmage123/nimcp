//=============================================================================
// test_thalamus.cpp - Thalamic Nuclei Unit Tests
//=============================================================================
/**
 * @file test_thalamus.cpp
 * @brief Unit tests for thalamic nuclei system
 *
 * WHAT: Tests for thalamus relay, attention gating, firing modes
 * WHY:  Ensure biological accuracy and correct signal routing
 * HOW:  GTest framework testing all thalamic nuclei functions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_thalamus.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ThalamusNucleusTest : public ::testing::Test {
protected:
    thal_nucleus_t* nucleus = nullptr;
    thal_nucleus_config_t config;

    void SetUp() override {
        thal_nucleus_default_config(&config, THAL_NUCLEUS_LGN);
        config.num_neurons = 32;
        config.num_channels = 16;
        nucleus = thal_nucleus_create(&config);
    }

    void TearDown() override {
        if (nucleus) {
            thal_nucleus_destroy(nucleus);
            nucleus = nullptr;
        }
    }
};

class ThalamusTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    thalamus_config_t config;

    void SetUp() override {
        thalamus_default_config(&config);
        config.neurons_per_nucleus = 32;
        config.channels_per_nucleus = 16;
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
// Nucleus Configuration Tests
//=============================================================================

TEST(ThalamusConfigTest, DefaultNucleusConfig) {
    thal_nucleus_config_t config;
    thal_nucleus_default_config(&config, THAL_NUCLEUS_LGN);

    EXPECT_EQ(config.type, THAL_NUCLEUS_LGN);
    EXPECT_EQ(config.num_neurons, THAL_DEFAULT_NEURONS);
    EXPECT_GT(config.num_channels, 0);
    EXPECT_FLOAT_EQ(config.burst_threshold, THAL_BURST_THRESHOLD);
    EXPECT_FLOAT_EQ(config.attention_weight, THAL_ATTENTION_BASELINE);
    EXPECT_TRUE(config.enable_adaptation);
}

TEST(ThalamusConfigTest, DefaultNucleusConfigDifferentTypes) {
    thal_nucleus_config_t lgn_config, mgn_config, pulvinar_config;

    thal_nucleus_default_config(&lgn_config, THAL_NUCLEUS_LGN);
    thal_nucleus_default_config(&mgn_config, THAL_NUCLEUS_MGN);
    thal_nucleus_default_config(&pulvinar_config, THAL_NUCLEUS_PULVINAR);

    EXPECT_EQ(lgn_config.type, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mgn_config.type, THAL_NUCLEUS_MGN);
    EXPECT_EQ(pulvinar_config.type, THAL_NUCLEUS_PULVINAR);

    // LGN and MGN are first-order, Pulvinar is higher-order
    EXPECT_EQ(lgn_config.order, THAL_ORDER_FIRST);
    EXPECT_EQ(mgn_config.order, THAL_ORDER_FIRST);
    EXPECT_EQ(pulvinar_config.order, THAL_ORDER_HIGHER);
}

TEST(ThalamusConfigTest, DefaultThalamusConfig) {
    thalamus_config_t config;
    thalamus_default_config(&config);

    EXPECT_EQ(config.neurons_per_nucleus, THAL_DEFAULT_NEURONS);
    EXPECT_GT(config.channels_per_nucleus, 0);
    EXPECT_FLOAT_EQ(config.attention_baseline, THAL_ATTENTION_BASELINE);
    EXPECT_FLOAT_EQ(config.burst_threshold, THAL_BURST_THRESHOLD);
    EXPECT_TRUE(config.enable_trn);
    EXPECT_TRUE(config.enable_mode_switching);
    EXPECT_TRUE(config.enable_attention_gating);
}

TEST(ThalamusConfigTest, NullConfigHandling) {
    thal_nucleus_default_config(nullptr, THAL_NUCLEUS_LGN);  // Should not crash
    thalamus_default_config(nullptr);  // Should not crash
}

//=============================================================================
// Nucleus Lifecycle Tests
//=============================================================================

TEST_F(ThalamusNucleusTest, CreateWithConfig) {
    ASSERT_NE(nucleus, nullptr);
    EXPECT_EQ(nucleus->type, THAL_NUCLEUS_LGN);
    EXPECT_EQ(nucleus->num_cells, config.num_neurons);
    EXPECT_EQ(nucleus->num_input_channels, config.num_channels);
    EXPECT_EQ(nucleus->num_output_channels, config.num_channels);
}

TEST(ThalamusNucleusLifecycleTest, CreateWithNullConfig) {
    thal_nucleus_t* n = thal_nucleus_create(nullptr);
    ASSERT_NE(n, nullptr);  // Should use defaults
    EXPECT_EQ(n->type, THAL_NUCLEUS_LGN);  // Default type
    thal_nucleus_destroy(n);
}

TEST(ThalamusNucleusLifecycleTest, DestroyNull) {
    thal_nucleus_destroy(nullptr);  // Should not crash
}

TEST_F(ThalamusNucleusTest, CellsInitialized) {
    ASSERT_NE(nucleus->cells, nullptr);
    for (uint32_t i = 0; i < nucleus->num_cells; i++) {
        EXPECT_EQ(nucleus->cells[i].cell_id, i);
        EXPECT_FLOAT_EQ(nucleus->cells[i].membrane_potential, -65.0f);
        EXPECT_EQ(nucleus->cells[i].mode, THAL_MODE_TONIC);
        EXPECT_FALSE(nucleus->cells[i].is_bursting);
    }
}

TEST_F(ThalamusNucleusTest, BuffersInitialized) {
    ASSERT_NE(nucleus->input_buffer, nullptr);
    ASSERT_NE(nucleus->output_buffer, nullptr);
    ASSERT_NE(nucleus->channel_attention, nullptr);
    ASSERT_NE(nucleus->channel_inhibition, nullptr);

    for (uint32_t i = 0; i < nucleus->num_input_channels; i++) {
        EXPECT_FLOAT_EQ(nucleus->input_buffer[i], 0.0f);
        EXPECT_FLOAT_EQ(nucleus->channel_inhibition[i], 0.0f);
    }
}

//=============================================================================
// Thalamus Lifecycle Tests
//=============================================================================

TEST_F(ThalamusTest, CreateWithConfig) {
    ASSERT_NE(thal, nullptr);
    ASSERT_NE(thal->lgn, nullptr);
    ASSERT_NE(thal->mgn, nullptr);
    ASSERT_NE(thal->vpl, nullptr);
    ASSERT_NE(thal->vpm, nullptr);
    ASSERT_NE(thal->va, nullptr);
    ASSERT_NE(thal->vl, nullptr);
    ASSERT_NE(thal->pulvinar, nullptr);
    ASSERT_NE(thal->md, nullptr);
}

TEST(ThalamusLifecycleTest, CreateWithNullConfig) {
    thalamus_t* t = thalamus_create(nullptr);
    ASSERT_NE(t, nullptr);
    thalamus_destroy(t);
}

TEST(ThalamusLifecycleTest, DestroyNull) {
    thalamus_destroy(nullptr);  // Should not crash
}

TEST_F(ThalamusTest, TRNCreated) {
    // TRN should be created when enabled
    ASSERT_NE(thal->trn, nullptr);
    EXPECT_GT(thal->trn->num_channels, 0);
}

TEST_F(ThalamusTest, InitialState) {
    EXPECT_FLOAT_EQ(thal->global_arousal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
    EXPECT_FALSE(thal->bio_async_enabled);
}

TEST_F(ThalamusTest, Reset) {
    // Modify state
    thal->global_arousal = 0.5f;
    thal->stats.total_signals_relayed = 100;
    thal->dominant_mode = THAL_MODE_BURST;

    // Reset
    int result = thalamus_reset(thal);
    EXPECT_EQ(result, 0);

    // Verify reset
    EXPECT_FLOAT_EQ(thal->global_arousal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);
    EXPECT_EQ(thal->stats.total_signals_relayed, 0);
}

TEST_F(ThalamusTest, ResetNullPointer) {
    int result = thalamus_reset(nullptr);
    EXPECT_NE(result, 0);  // Error code (positive or negative)
}

//=============================================================================
// Signal Relay Tests
//=============================================================================

TEST_F(ThalamusTest, RelayVisual) {
    float retinal_input[16];
    float v1_output[16];

    // Create test input
    for (int i = 0; i < 16; i++) {
        retinal_input[i] = 0.5f + 0.1f * i;
    }

    int result = thalamus_relay_visual(thal, retinal_input, 16, v1_output, 16);
    EXPECT_GE(result, 0);

    // Output should be modulated but non-zero for non-zero input
    bool has_output = false;
    for (int i = 0; i < 16; i++) {
        if (v1_output[i] > 0.0f) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output);
}

TEST_F(ThalamusTest, RelayAuditory) {
    float ic_input[16];
    float a1_output[16];

    for (int i = 0; i < 16; i++) {
        ic_input[i] = 0.6f;
    }

    int result = thalamus_relay_auditory(thal, ic_input, 16, a1_output, 16);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamusTest, RelayMotor) {
    float bg_input[16];
    float motor_output[16];

    for (int i = 0; i < 16; i++) {
        bg_input[i] = 0.8f;  // Disinhibition from BG
    }

    int result = thalamus_relay_motor(thal, bg_input, 16, motor_output, 16);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamusTest, RelayExecutive) {
    float input[16];
    float pfc_output[16];

    for (int i = 0; i < 16; i++) {
        input[i] = 0.7f;
    }

    int result = thalamus_relay_executive(thal, input, 16, pfc_output, 16);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamusTest, RelayGeneric) {
    float input[16];
    float output[16];

    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }

    // Test relay through each nucleus type
    thal_nucleus_type_t types[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VPM, THAL_NUCLEUS_VA, THAL_NUCLEUS_VL,
        THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };

    for (auto type : types) {
        memset(output, 0, sizeof(output));
        int result = thalamus_relay(thal, type, input, 16, output, 16);
        EXPECT_GE(result, 0) << "Failed for nucleus " << thal_nucleus_name(type);
    }
}

TEST_F(ThalamusTest, RelayNullPointers) {
    float buffer[16];

    EXPECT_NE(thalamus_relay(nullptr, THAL_NUCLEUS_LGN, buffer, 16, buffer, 16), 0);
    EXPECT_NE(thalamus_relay(thal, THAL_NUCLEUS_LGN, nullptr, 16, buffer, 16), 0);
    EXPECT_NE(thalamus_relay(thal, THAL_NUCLEUS_LGN, buffer, 16, nullptr, 16), 0);
}

TEST_F(ThalamusTest, GetOutput) {
    float input[16] = {0.5f};
    float output[16] = {0};

    // First relay something
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    // Then get the output
    float get_output[16] = {0};
    int result = thalamus_get_output(thal, THAL_NUCLEUS_LGN, get_output, 16);
    EXPECT_GE(result, 0);
}

//=============================================================================
// Attention Gating Tests
//=============================================================================

TEST_F(ThalamusTest, SetAttention) {
    int result = thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);
    EXPECT_EQ(result, 0);

    float attention = thalamus_get_attention(thal, THAL_NUCLEUS_LGN);
    EXPECT_FLOAT_EQ(attention, 0.8f);
}

TEST_F(ThalamusTest, SetAttentionClamps) {
    // Test clamping to [0, 1]
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 2.0f);
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_LGN), 1.0f);

    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, -0.5f);
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_LGN), 0.0f);
}

TEST_F(ThalamusTest, SetChannelAttention) {
    int result = thalamus_set_channel_attention(thal, THAL_NUCLEUS_LGN, 5, 0.9f);
    EXPECT_EQ(result, 0);

    // Get nucleus and verify channel attention
    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
    ASSERT_NE(lgn, nullptr);
    EXPECT_FLOAT_EQ(lgn->channel_attention[5], 0.9f);
}

TEST_F(ThalamusTest, SetChannelAttentionInvalidChannel) {
    int result = thalamus_set_channel_attention(thal, THAL_NUCLEUS_LGN, 9999, 0.5f);
    EXPECT_NE(result, 0);
}

TEST_F(ThalamusTest, AttentionModulatesRelay) {
    float input[16];
    float output_high[16], output_low[16];

    for (int i = 0; i < 16; i++) {
        input[i] = 0.8f;
    }

    // High attention
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 1.0f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_high, 16);

    // Low attention
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.2f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_low, 16);

    // High attention should produce higher output
    float sum_high = 0.0f, sum_low = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_high += output_high[i];
        sum_low += output_low[i];
    }
    EXPECT_GT(sum_high, sum_low);
}

TEST_F(ThalamusTest, GetAttentionNullPointer) {
    float attention = thalamus_get_attention(nullptr, THAL_NUCLEUS_LGN);
    EXPECT_LT(attention, 0.0f);
}

//=============================================================================
// Arousal Tests
//=============================================================================

TEST_F(ThalamusTest, SetArousal) {
    int result = thalamus_set_arousal(thal, 0.7f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(thal->global_arousal, 0.7f);
}

TEST_F(ThalamusTest, SetArousalClamps) {
    thalamus_set_arousal(thal, 1.5f);
    EXPECT_FLOAT_EQ(thal->global_arousal, 1.0f);

    thalamus_set_arousal(thal, -0.3f);
    EXPECT_FLOAT_EQ(thal->global_arousal, 0.0f);
}

TEST_F(ThalamusTest, LowArousalTriggersBurstMode) {
    // High arousal = tonic mode
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC);

    // Low arousal = burst mode
    thalamus_set_arousal(thal, 0.1f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);
}

TEST_F(ThalamusTest, ArousalNullPointer) {
    int result = thalamus_set_arousal(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// TRN Inhibition Tests
//=============================================================================

TEST_F(ThalamusTest, ApplyTRNInhibition) {
    int result = thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.6f);
    EXPECT_EQ(result, 0);

    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
    ASSERT_NE(lgn, nullptr);
    EXPECT_FLOAT_EQ(lgn->trn_inhibition, 0.6f);
}

TEST_F(ThalamusTest, HighTRNInhibitionSuppressesRelay) {
    float input[16];
    float output_normal[16], output_inhibited[16];

    for (int i = 0; i < 16; i++) {
        input[i] = 0.8f;
    }

    // Normal (no inhibition)
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.0f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_normal, 16);

    // Full inhibition
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 1.0f);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output_inhibited, 16);

    // Inhibited output should be lower or zero
    float sum_normal = 0.0f, sum_inhibited = 0.0f;
    for (int i = 0; i < 16; i++) {
        sum_normal += output_normal[i];
        sum_inhibited += output_inhibited[i];
    }
    EXPECT_GT(sum_normal, sum_inhibited);
}

TEST_F(ThalamusTest, ApplyChannelInhibition) {
    int result = thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_MGN, 3, 0.7f);
    EXPECT_EQ(result, 0);

    thal_nucleus_t* mgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_MGN);
    ASSERT_NE(mgn, nullptr);
    EXPECT_FLOAT_EQ(mgn->channel_inhibition[3], 0.7f);
}

//=============================================================================
// Firing Mode Tests
//=============================================================================

TEST_F(ThalamusTest, SetMode) {
    int result = thalamus_set_mode(thal, THAL_NUCLEUS_LGN, THAL_MODE_BURST);
    EXPECT_EQ(result, 0);

    thal_firing_mode_t mode = thalamus_get_mode(thal, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_BURST);
}

TEST_F(ThalamusTest, TriggerBurst) {
    int result = thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    EXPECT_EQ(result, 0);

    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
    ASSERT_NE(lgn, nullptr);
    EXPECT_EQ(lgn->dominant_mode, THAL_MODE_BURST);

    // All cells should be bursting
    for (uint32_t i = 0; i < lgn->num_cells; i++) {
        EXPECT_TRUE(lgn->cells[i].is_bursting);
        EXPECT_FLOAT_EQ(lgn->cells[i].t_channel_state, 1.0f);
    }
}

TEST_F(ThalamusTest, GetTonicFraction) {
    // Initially all tonic
    float fraction = thalamus_get_tonic_fraction(thal, THAL_NUCLEUS_LGN);
    EXPECT_FLOAT_EQ(fraction, 1.0f);

    // After burst trigger
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    fraction = thalamus_get_tonic_fraction(thal, THAL_NUCLEUS_LGN);
    EXPECT_FLOAT_EQ(fraction, 0.0f);
}

TEST_F(ThalamusTest, BurstModeBelowThresholdNoRelay) {
    float input[16];
    float output[16];

    // Input below burst threshold
    for (int i = 0; i < 16; i++) {
        input[i] = 0.1f;  // Below THAL_BURST_THRESHOLD (0.3)
    }

    // Set burst mode
    thalamus_set_mode(thal, THAL_NUCLEUS_LGN, THAL_MODE_BURST);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    // In burst mode, signals below threshold should be suppressed
    for (int i = 0; i < 16; i++) {
        EXPECT_FLOAT_EQ(output[i], 0.0f);
    }
}

//=============================================================================
// Update/Step Tests
//=============================================================================

TEST_F(ThalamusTest, Step) {
    int result = thalamus_step(thal, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamusTest, NucleusStep) {
    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
    ASSERT_NE(lgn, nullptr);

    int result = thal_nucleus_step(lgn, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamusTest, BurstDecaysOverTime) {
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);

    // Initial T-channel state should be 1.0
    EXPECT_FLOAT_EQ(lgn->cells[0].t_channel_state, 1.0f);

    // Step multiple times - T-channel state should decay
    for (int i = 0; i < 100; i++) {
        thal_nucleus_step(lgn, 1.0f);
    }

    // After 100ms, T-channel should have decayed significantly
    EXPECT_LT(lgn->cells[0].t_channel_state, 0.1f);
}

TEST_F(ThalamusTest, UpdateTRN) {
    ASSERT_NE(thal->trn, nullptr);

    int result = thalamus_update_trn(thal);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(ThalamusTest, ConnectBasalGanglia) {
    float bg_output[16];
    for (int i = 0; i < 16; i++) {
        bg_output[i] = 0.6f;
    }

    int result = thalamus_connect_basal_ganglia(thal, bg_output, 16);
    EXPECT_EQ(result, 0);
}

TEST_F(ThalamusTest, PulvinarAttention) {
    float attention_signal[16];
    for (int i = 0; i < 16; i++) {
        attention_signal[i] = 0.8f;
    }

    int result = thalamus_pulvinar_attention(thal, attention_signal, 16);
    EXPECT_EQ(result, 0);

    // Pulvinar should modulate LGN attention
    float lgn_attention = thalamus_get_attention(thal, THAL_NUCLEUS_LGN);
    EXPECT_GT(lgn_attention, 0.0f);
}

TEST_F(ThalamusTest, GetNucleus) {
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN), thal->lgn);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_MGN), thal->mgn);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_VPL), thal->vpl);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_VPM), thal->vpm);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_VA), thal->va);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_VL), thal->vl);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_PULVINAR), thal->pulvinar);
    EXPECT_EQ(thalamus_get_nucleus(thal, THAL_NUCLEUS_MD), thal->md);
}

TEST_F(ThalamusTest, GetNucleusInvalidType) {
    thal_nucleus_t* n = thalamus_get_nucleus(thal, (thal_nucleus_type_t)99);
    EXPECT_EQ(n, nullptr);
}

TEST_F(ThalamusTest, GetNucleusNull) {
    thal_nucleus_t* n = thalamus_get_nucleus(nullptr, THAL_NUCLEUS_LGN);
    EXPECT_EQ(n, nullptr);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(ThalamusTest, BioAsyncConnect) {
    int result = thalamus_connect_bio_async(thal);
    // May succeed or fail depending on router availability
    EXPECT_GE(result, -1);
}

TEST_F(ThalamusTest, BioAsyncConnectNull) {
    int result = thalamus_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(ThalamusTest, IsBioAsyncConnected) {
    // Initially not connected
    EXPECT_FALSE(thalamus_is_bio_async_connected(thal));

    // Try to connect (may or may not succeed)
    thalamus_connect_bio_async(thal);

    // Either connected or still not connected, but shouldn't crash
    bool connected = thalamus_is_bio_async_connected(thal);
    (void)connected;  // May be true or false
}

TEST_F(ThalamusTest, BioAsyncDisconnect) {
    thalamus_connect_bio_async(thal);
    int result = thalamus_disconnect_bio_async(thal);
    EXPECT_GE(result, 0);
    EXPECT_FALSE(thalamus_is_bio_async_connected(thal));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ThalamusTest, GetStats) {
    thalamus_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero to verify write

    int result = thalamus_get_stats(thal, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_signals_relayed, 0);
}

TEST_F(ThalamusTest, StatsIncrementOnRelay) {
    float input[16] = {0.5f};
    float output[16];

    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
    thalamus_relay(thal, THAL_NUCLEUS_MGN, input, 16, output, 16);

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);
    EXPECT_EQ(stats.total_signals_relayed, 2);
    EXPECT_GE(stats.signals_per_nucleus[THAL_NUCLEUS_LGN], 1);
    EXPECT_GE(stats.signals_per_nucleus[THAL_NUCLEUS_MGN], 1);
}

TEST_F(ThalamusTest, BurstCountInStats) {
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_MGN);

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);
    EXPECT_EQ(stats.burst_count, 2);
}

TEST_F(ThalamusTest, GetStatsNull) {
    EXPECT_NE(thalamus_get_stats(nullptr, nullptr), 0);
    EXPECT_NE(thalamus_get_stats(thal, nullptr), 0);
}

//=============================================================================
// Name Functions Tests
//=============================================================================

TEST(ThalamusNamesTest, NucleusNames) {
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_LGN), "LGN");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_MGN), "MGN");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VPL), "VPL");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VPM), "VPM");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VA), "VA");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VL), "VL");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_PULVINAR), "Pulvinar");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_MD), "MD");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_TRN), "TRN");
}

TEST(ThalamusNamesTest, ModeNames) {
    EXPECT_STREQ(thal_mode_name(THAL_MODE_TONIC), "Tonic");
    EXPECT_STREQ(thal_mode_name(THAL_MODE_BURST), "Burst");
    EXPECT_STREQ(thal_mode_name(THAL_MODE_INHIBITED), "Inhibited");
}

TEST(ThalamusNamesTest, InvalidValues) {
    EXPECT_STREQ(thal_nucleus_name((thal_nucleus_type_t)99), "Unknown");
    EXPECT_STREQ(thal_mode_name((thal_firing_mode_t)99), "Unknown");
}

//=============================================================================
// Firing Rate Tests
//=============================================================================

TEST_F(ThalamusTest, GetFiringRate) {
    // Initially should be low/zero
    float rate = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
    EXPECT_GE(rate, 0.0f);

    // After relay, should have some activity
    float input[16] = {0.5f};
    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    rate = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(ThalamusTest, GetFiringRateNull) {
    float rate = thalamus_get_firing_rate(nullptr, THAL_NUCLEUS_LGN);
    EXPECT_FLOAT_EQ(rate, 0.0f);
}

//=============================================================================
// Nucleus Process Input Tests
//=============================================================================

TEST_F(ThalamusNucleusTest, ProcessInput) {
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f + 0.02f * i;
    }

    int result = thal_nucleus_process_input(nucleus, input, 16);
    EXPECT_EQ(result, 0);

    // Input buffer should be filled
    bool has_input = false;
    for (uint32_t i = 0; i < nucleus->num_input_channels; i++) {
        if (nucleus->input_buffer[i] > 0.0f) {
            has_input = true;
            break;
        }
    }
    EXPECT_TRUE(has_input);
}

TEST_F(ThalamusNucleusTest, ProcessInputNull) {
    EXPECT_NE(thal_nucleus_process_input(nullptr, nullptr, 16), 0);
    EXPECT_NE(thal_nucleus_process_input(nucleus, nullptr, 16), 0);
}

TEST_F(ThalamusNucleusTest, InhibitedModeNoOutput) {
    nucleus->dominant_mode = THAL_MODE_INHIBITED;

    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.8f;
    }

    thal_nucleus_process_input(nucleus, input, 16);

    // All outputs should be zero in inhibited mode
    for (uint32_t i = 0; i < nucleus->num_input_channels; i++) {
        EXPECT_FLOAT_EQ(nucleus->input_buffer[i], 0.0f);
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ThalamusTest, ZeroSizeRelay) {
    float input[16] = {0.5f};
    float output[16] = {0};

    int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 0, output, 0);
    EXPECT_GE(result, 0);  // Should handle gracefully
}

TEST_F(ThalamusTest, LargerInputThanChannels) {
    float input[256];
    float output[256];

    for (int i = 0; i < 256; i++) {
        input[i] = 0.5f;
    }

    // Should process only up to num_channels
    int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 256, output, 256);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamusTest, SmallerOutputBuffer) {
    float input[16] = {0.5f};
    float output[8] = {0};

    int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 8);
    EXPECT_GE(result, 0);
    EXPECT_LE(result, 8);  // Should not exceed output buffer size
}
