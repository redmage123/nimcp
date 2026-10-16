//=============================================================================
// test_thalamus_regression.cpp - Comprehensive Thalamus Regression Tests
//=============================================================================
/**
 * @file test_thalamus_regression.cpp
 * @brief Comprehensive regression tests for the thalamic nuclei system
 *
 * WHAT: Tests for performance, determinism, state consistency, TRN gating,
 *       mode transitions, null safety, and backward compatibility
 * WHY:  Ensure thalamus behavior is stable, deterministic, and performant
 * HOW:  GTest framework with benchmarks, determinism, and stability checks
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_thalamus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ThalamusRegressionTest : public NimcpTestBase {
protected:
    thalamus_t* thal = nullptr;
    static constexpr uint32_t TEST_CHANNELS = 32;
    static constexpr uint32_t TEST_NEURONS = 64;

    void SetUp() override {
        NimcpTestBase::SetUp();

        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = TEST_NEURONS;
        config.channels_per_nucleus = TEST_CHANNELS;
        config.enable_trn = true;
        config.enable_mode_switching = true;
        config.enable_attention_gating = true;
        thal = thalamus_create(&config);
    }

    void TearDown() override {
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to generate test input
    void fillTestInput(float* buffer, uint32_t size, float base_value, float increment = 0.0f) {
        for (uint32_t i = 0; i < size; i++) {
            buffer[i] = base_value + increment * i;
        }
    }
};

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, NucleusLookupPerformance) {
    ASSERT_NE(thal, nullptr);

    const int NUM_ITERATIONS = 10000;
    thal_nucleus_type_t types[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VPM, THAL_NUCLEUS_VA, THAL_NUCLEUS_VL,
        THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };
    const int NUM_TYPES = sizeof(types) / sizeof(types[0]);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        for (int t = 0; t < NUM_TYPES; t++) {
            thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, types[t]);
            ASSERT_NE(nucleus, nullptr);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Nucleus lookup should be fast (< 10us per lookup on average)
    double avg_us = (double)duration.count() / (NUM_ITERATIONS * NUM_TYPES);
    EXPECT_LT(avg_us, 10.0) << "Nucleus lookup too slow: " << avg_us << " us";
}

TEST_F(ThalamusRegressionTest, RelayProcessingPerformance) {
    ASSERT_NE(thal, nullptr);

    const int NUM_ITERATIONS = 1000;
    std::vector<float> input(TEST_CHANNELS, 0.5f);
    std::vector<float> output(TEST_CHANNELS);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                                    output.data(), TEST_CHANNELS);
        EXPECT_GE(result, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Relay processing should be < 100us per call
    double avg_us = (double)duration.count() / NUM_ITERATIONS;
    EXPECT_LT(avg_us, 100.0) << "Relay processing too slow: " << avg_us << " us";
}

TEST_F(ThalamusRegressionTest, StepPerformance) {
    ASSERT_NE(thal, nullptr);

    const int NUM_STEPS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_STEPS; i++) {
        int result = thalamus_step(thal, 1.0f);
        EXPECT_GE(result, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Step should be < 500us per call
    double avg_us = (double)duration.count() / NUM_STEPS;
    EXPECT_LT(avg_us, 500.0) << "Thalamus step too slow: " << avg_us << " us";
}

//=============================================================================
// Determinism Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, RelayDeterminism_SameInputSameOutput) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS);
    fillTestInput(input.data(), TEST_CHANNELS, 0.3f, 0.02f);

    std::vector<float> output1(TEST_CHANNELS), output2(TEST_CHANNELS);

    // First run
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output1.data(), TEST_CHANNELS);

    // Reset and run again
    thalamus_reset(thal);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output2.data(), TEST_CHANNELS);

    // Outputs must be identical
    for (uint32_t i = 0; i < TEST_CHANNELS; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]) << "Non-deterministic at index " << i;
    }
}

TEST_F(ThalamusRegressionTest, RelayDeterminism_MultipleNuclei) {
    ASSERT_NE(thal, nullptr);

    thal_nucleus_type_t nuclei[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };
    const int NUM_NUCLEI = sizeof(nuclei) / sizeof(nuclei[0]);

    std::vector<float> input(TEST_CHANNELS, 0.6f);
    std::vector<std::vector<float>> outputs1(NUM_NUCLEI, std::vector<float>(TEST_CHANNELS));
    std::vector<std::vector<float>> outputs2(NUM_NUCLEI, std::vector<float>(TEST_CHANNELS));

    // First pass
    for (int n = 0; n < NUM_NUCLEI; n++) {
        thalamus_relay(thal, nuclei[n], input.data(), TEST_CHANNELS,
                       outputs1[n].data(), TEST_CHANNELS);
    }

    // Reset and second pass
    thalamus_reset(thal);
    for (int n = 0; n < NUM_NUCLEI; n++) {
        thalamus_relay(thal, nuclei[n], input.data(), TEST_CHANNELS,
                       outputs2[n].data(), TEST_CHANNELS);
    }

    // Compare all outputs
    for (int n = 0; n < NUM_NUCLEI; n++) {
        for (uint32_t i = 0; i < TEST_CHANNELS; i++) {
            EXPECT_FLOAT_EQ(outputs1[n][i], outputs2[n][i])
                << "Non-deterministic for nucleus " << thal_nucleus_name(nuclei[n])
                << " at index " << i;
        }
    }
}

TEST_F(ThalamusRegressionTest, AttentionModulationDeterminism) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.5f);
    std::vector<float> output1(TEST_CHANNELS), output2(TEST_CHANNELS);

    // Set attention and relay
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);
    for (uint32_t i = 0; i < TEST_CHANNELS; i++) {
        thalamus_set_channel_attention(thal, THAL_NUCLEUS_LGN, i, 0.7f + 0.01f * i);
    }
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output1.data(), TEST_CHANNELS);

    // Reset and repeat exactly
    thalamus_reset(thal);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.8f);
    for (uint32_t i = 0; i < TEST_CHANNELS; i++) {
        thalamus_set_channel_attention(thal, THAL_NUCLEUS_LGN, i, 0.7f + 0.01f * i);
    }
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output2.data(), TEST_CHANNELS);

    for (uint32_t i = 0; i < TEST_CHANNELS; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]) << "Attention modulation non-deterministic at " << i;
    }
}

TEST_F(ThalamusRegressionTest, StepSequenceDeterminism) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.4f);
    std::vector<float> output(TEST_CHANNELS);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS, output.data(), TEST_CHANNELS);

    // Run 50 steps and record states
    std::vector<float> rates1(50), rates2(50);
    for (int i = 0; i < 50; i++) {
        thalamus_step(thal, 1.0f);
        rates1[i] = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
    }

    // Reset and repeat
    thalamus_reset(thal);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS, output.data(), TEST_CHANNELS);
    for (int i = 0; i < 50; i++) {
        thalamus_step(thal, 1.0f);
        rates2[i] = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
    }

    for (int i = 0; i < 50; i++) {
        EXPECT_FLOAT_EQ(rates1[i], rates2[i]) << "Step sequence non-deterministic at step " << i;
    }
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, StateConsistency_AfterRelay) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.7f);
    std::vector<float> output(TEST_CHANNELS);

    // Relay multiple times
    for (int i = 0; i < 100; i++) {
        int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                                    output.data(), TEST_CHANNELS);
        EXPECT_GE(result, 0);

        // Check state consistency
        EXPECT_GE(thal->global_arousal, 0.0f);
        EXPECT_LE(thal->global_arousal, 1.0f);
        EXPECT_GE(thal->global_attention, 0.0f);
        EXPECT_LE(thal->global_attention, 1.0f);

        // Nucleus should be valid
        thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
        ASSERT_NE(lgn, nullptr);
        EXPECT_GE(lgn->attention_level, 0.0f);
        EXPECT_LE(lgn->attention_level, 1.0f);
    }
}

TEST_F(ThalamusRegressionTest, StateConsistency_AfterModeSwitch) {
    ASSERT_NE(thal, nullptr);

    // Alternate between tonic and burst modes
    for (int i = 0; i < 50; i++) {
        if (i % 2 == 0) {
            thalamus_set_arousal(thal, 1.0f);  // Tonic
        } else {
            thalamus_set_arousal(thal, 0.1f);  // Burst
        }

        // Verify state bounds
        EXPECT_GE(thal->global_arousal, 0.0f);
        EXPECT_LE(thal->global_arousal, 1.0f);

        // All nuclei should have valid modes
        thal_nucleus_type_t types[] = {THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_MD};
        for (auto type : types) {
            thal_firing_mode_t mode = thalamus_get_mode(thal, type);
            EXPECT_TRUE(mode == THAL_MODE_TONIC || mode == THAL_MODE_BURST || mode == THAL_MODE_INHIBITED);
        }
    }
}

TEST_F(ThalamusRegressionTest, StateConsistency_Statistics) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.5f);
    std::vector<float> output(TEST_CHANNELS);

    // Process through different nuclei
    for (int i = 0; i < 20; i++) {
        thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS, output.data(), TEST_CHANNELS);
    }
    for (int i = 0; i < 15; i++) {
        thalamus_relay(thal, THAL_NUCLEUS_MGN, input.data(), TEST_CHANNELS, output.data(), TEST_CHANNELS);
    }
    for (int i = 0; i < 10; i++) {
        thalamus_relay(thal, THAL_NUCLEUS_PULVINAR, input.data(), TEST_CHANNELS, output.data(), TEST_CHANNELS);
    }

    thalamus_stats_t stats;
    int result = thalamus_get_stats(thal, &stats);
    EXPECT_GE(result, 0);

    // Verify statistics consistency
    EXPECT_EQ(stats.total_signals_relayed, 45);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_LGN], 20);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_MGN], 15);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_PULVINAR], 10);
    EXPECT_GE(stats.avg_relay_latency_ms, 0.0f);
}

//=============================================================================
// TRN Gating Accuracy Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, TRNGating_InhibitionEffect) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 1.0f);  // Strong input
    std::vector<float> output_no_inhibit(TEST_CHANNELS);
    std::vector<float> output_inhibited(TEST_CHANNELS);

    // Relay without inhibition
    int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output_no_inhibit.data(), TEST_CHANNELS);
    EXPECT_GE(result, 0);

    // Apply strong TRN inhibition
    thalamus_reset(thal);
    int inhibit_result = thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.9f);
    EXPECT_GE(inhibit_result, 0);

    // Step to allow inhibition to take effect
    for (int i = 0; i < 10; i++) {
        thalamus_step(thal, 1.0f);
        thalamus_update_trn(thal);
    }

    result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output_inhibited.data(), TEST_CHANNELS);
    EXPECT_GE(result, 0);

    // Verify outputs are valid (bounded)
    float sum_no_inhibit = std::accumulate(output_no_inhibit.begin(), output_no_inhibit.end(), 0.0f);
    float sum_inhibited = std::accumulate(output_inhibited.begin(), output_inhibited.end(), 0.0f);

    // TRN inhibition should reduce or maintain output (allow equal as implementation detail)
    EXPECT_LE(sum_inhibited, sum_no_inhibit + 0.001f) << "TRN inhibition should not increase output";

    // Verify nucleus state reflects inhibition
    thal_nucleus_t* lgn = thalamus_get_nucleus(thal, THAL_NUCLEUS_LGN);
    EXPECT_GT(lgn->trn_inhibition, 0.0f);
}

TEST_F(ThalamusRegressionTest, TRNGating_ChannelSpecific) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.8f);
    std::vector<float> output(TEST_CHANNELS);

    // Apply inhibition to specific channels
    for (uint32_t i = 0; i < TEST_CHANNELS / 2; i++) {
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, i, 0.9f);
    }

    thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                   output.data(), TEST_CHANNELS);

    // First half should be more inhibited than second half
    float sum_first_half = 0.0f, sum_second_half = 0.0f;
    for (uint32_t i = 0; i < TEST_CHANNELS / 2; i++) {
        sum_first_half += output[i];
    }
    for (uint32_t i = TEST_CHANNELS / 2; i < TEST_CHANNELS; i++) {
        sum_second_half += output[i];
    }

    EXPECT_LT(sum_first_half, sum_second_half) << "Channel-specific inhibition failed";
}

TEST_F(ThalamusRegressionTest, TRNGating_UpdateConsistency) {
    ASSERT_NE(thal, nullptr);

    // Update TRN multiple times
    for (int i = 0; i < 100; i++) {
        int result = thalamus_update_trn(thal);
        EXPECT_GE(result, 0);

        // TRN state should remain valid
        if (thal->trn) {
            EXPECT_GE(thal->trn->global_inhibition, 0.0f);
            EXPECT_LE(thal->trn->global_inhibition, 1.0f);
        }
    }
}

//=============================================================================
// Burst/Tonic Mode Transition Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, ModeTransition_ArousalDriven) {
    ASSERT_NE(thal, nullptr);

    // High arousal should produce tonic mode
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_MGN), THAL_MODE_TONIC);

    // Low arousal should produce burst mode
    thalamus_set_arousal(thal, 0.1f);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_BURST);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_MGN), THAL_MODE_BURST);
}

TEST_F(ThalamusRegressionTest, ModeTransition_ManualTrigger) {
    ASSERT_NE(thal, nullptr);

    // Trigger burst manually
    int result = thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    EXPECT_GE(result, 0);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_BURST);

    // Other nuclei should be unaffected
    EXPECT_NE(thalamus_get_mode(thal, THAL_NUCLEUS_MGN), THAL_MODE_BURST);
}

TEST_F(ThalamusRegressionTest, ModeTransition_RapidSwitching) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.5f);
    std::vector<float> output(TEST_CHANNELS);

    // Rapidly switch modes during processing
    for (int i = 0; i < 100; i++) {
        float arousal = (i % 2 == 0) ? 1.0f : 0.1f;
        thalamus_set_arousal(thal, arousal);

        // Should not crash or corrupt
        int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                                    output.data(), TEST_CHANNELS);
        EXPECT_GE(result, 0);

        // Output bounds check
        for (uint32_t j = 0; j < TEST_CHANNELS; j++) {
            EXPECT_GE(output[j], 0.0f);
            EXPECT_LE(output[j], 1.0f);
            EXPECT_FALSE(std::isnan(output[j]));
        }
    }
}

TEST_F(ThalamusRegressionTest, ModeTransition_TonicFractionBounds) {
    ASSERT_NE(thal, nullptr);

    float arousal_levels[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float arousal : arousal_levels) {
        thalamus_set_arousal(thal, arousal);

        thal_nucleus_type_t types[] = {THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_MD};
        for (auto type : types) {
            float fraction = thalamus_get_tonic_fraction(thal, type);
            EXPECT_GE(fraction, 0.0f) << "Tonic fraction < 0 for arousal " << arousal;
            EXPECT_LE(fraction, 1.0f) << "Tonic fraction > 1 for arousal " << arousal;
        }
    }
}

//=============================================================================
// Null Pointer Safety Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, NullSafety_ThalamusOperations) {
    // All these should not crash
    thalamus_destroy(nullptr);
    EXPECT_NE(thalamus_reset(nullptr), 0);
    EXPECT_NE(thalamus_step(nullptr, 1.0f), 0);
    EXPECT_NE(thalamus_set_arousal(nullptr, 0.5f), 0);
    EXPECT_NE(thalamus_set_attention(nullptr, THAL_NUCLEUS_LGN, 0.5f), 0);
    EXPECT_LT(thalamus_get_attention(nullptr, THAL_NUCLEUS_LGN), 0.0f);
    EXPECT_EQ(thalamus_get_nucleus(nullptr, THAL_NUCLEUS_LGN), nullptr);
    EXPECT_EQ(thalamus_get_nucleus_const(nullptr, THAL_NUCLEUS_LGN), nullptr);
    EXPECT_FALSE(thalamus_is_bio_async_connected(nullptr));
}

TEST_F(ThalamusRegressionTest, NullSafety_RelayOperations) {
    ASSERT_NE(thal, nullptr);

    float buffer[TEST_CHANNELS] = {0};

    // Null thalamus
    EXPECT_NE(thalamus_relay(nullptr, THAL_NUCLEUS_LGN, buffer, TEST_CHANNELS, buffer, TEST_CHANNELS), 0);

    // Null input buffer
    EXPECT_NE(thalamus_relay(thal, THAL_NUCLEUS_LGN, nullptr, TEST_CHANNELS, buffer, TEST_CHANNELS), 0);

    // Null output buffer
    EXPECT_NE(thalamus_relay(thal, THAL_NUCLEUS_LGN, buffer, TEST_CHANNELS, nullptr, TEST_CHANNELS), 0);
}

TEST_F(ThalamusRegressionTest, NullSafety_StatisticsOperations) {
    thalamus_stats_t stats;

    EXPECT_NE(thalamus_get_stats(nullptr, &stats), 0);
    EXPECT_NE(thalamus_get_stats(thal, nullptr), 0);
}

TEST_F(ThalamusRegressionTest, NullSafety_NucleusOperations) {
    thal_nucleus_destroy(nullptr);

    thal_nucleus_config_t config;
    thal_nucleus_default_config(&config, THAL_NUCLEUS_LGN);
    thal_nucleus_default_config(nullptr, THAL_NUCLEUS_LGN);  // Should not crash

    EXPECT_NE(thal_nucleus_step(nullptr, 1.0f), 0);
    EXPECT_NE(thal_nucleus_process_input(nullptr, nullptr, 0), 0);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, BackwardCompat_DefaultConfigValues) {
    thalamus_config_t config;
    thalamus_default_config(&config);

    // These values must remain stable for backward compatibility
    EXPECT_EQ(config.neurons_per_nucleus, THAL_DEFAULT_NEURONS);
    EXPECT_FLOAT_EQ(config.attention_baseline, THAL_ATTENTION_BASELINE);
    EXPECT_FLOAT_EQ(config.burst_threshold, THAL_BURST_THRESHOLD);
    EXPECT_FLOAT_EQ(config.trn_strength, THAL_TRN_INHIBITION_STRENGTH);
    EXPECT_TRUE(config.enable_trn);
    EXPECT_TRUE(config.enable_mode_switching);
    EXPECT_TRUE(config.enable_attention_gating);
}

TEST_F(ThalamusRegressionTest, BackwardCompat_NucleusConfigValues) {
    thal_nucleus_config_t configs[THAL_NUCLEUS_COUNT];

    for (int i = 0; i < THAL_NUCLEUS_COUNT; i++) {
        thal_nucleus_default_config(&configs[i], (thal_nucleus_type_t)i);
        EXPECT_EQ(configs[i].type, (thal_nucleus_type_t)i);
        EXPECT_GT(configs[i].num_neurons, 0);
        EXPECT_GT(configs[i].num_channels, 0);
        EXPECT_GE(configs[i].burst_threshold, 0.0f);
        EXPECT_LE(configs[i].burst_threshold, 1.0f);
    }
}

TEST_F(ThalamusRegressionTest, BackwardCompat_NucleusNames) {
    // String names must be stable for logging compatibility
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_LGN), "LGN");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_MGN), "MGN");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VPL), "VPL");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VPM), "VPM");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VA), "VA");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VL), "VL");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_PULVINAR), "Pulvinar");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_MD), "MD");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_ANTERIOR), "Anterior");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_TRN), "TRN");
}

TEST_F(ThalamusRegressionTest, BackwardCompat_ModeNames) {
    EXPECT_STREQ(thal_mode_name(THAL_MODE_TONIC), "Tonic");
    EXPECT_STREQ(thal_mode_name(THAL_MODE_BURST), "Burst");
    EXPECT_STREQ(thal_mode_name(THAL_MODE_INHIBITED), "Inhibited");
}

TEST_F(ThalamusRegressionTest, BackwardCompat_CreateWithNullConfig) {
    // Creating with NULL config should use defaults
    thalamus_t* default_thal = thalamus_create(nullptr);
    ASSERT_NE(default_thal, nullptr);

    // Should have default values
    EXPECT_FLOAT_EQ(default_thal->config.attention_baseline, THAL_ATTENTION_BASELINE);
    EXPECT_FLOAT_EQ(default_thal->config.burst_threshold, THAL_BURST_THRESHOLD);

    thalamus_destroy(default_thal);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, Stress_CreateDestroyLoop) {
    for (int i = 0; i < 100; i++) {
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = 32 + (i % 64);
        config.channels_per_nucleus = 16 + (i % 32);

        thalamus_t* t = thalamus_create(&config);
        ASSERT_NE(t, nullptr) << "Create failed at iteration " << i;

        // Do some work
        std::vector<float> input(config.channels_per_nucleus, 0.5f);
        std::vector<float> output(config.channels_per_nucleus);
        thalamus_relay(t, THAL_NUCLEUS_LGN, input.data(), config.channels_per_nucleus,
                       output.data(), config.channels_per_nucleus);

        thalamus_destroy(t);
    }
}

TEST_F(ThalamusRegressionTest, Stress_HighVolumeMixedOperations) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS);
    std::vector<float> output(TEST_CHANNELS);

    thal_nucleus_type_t nuclei[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VA, THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };
    const int NUM_NUCLEI = sizeof(nuclei) / sizeof(nuclei[0]);

    for (int i = 0; i < 5000; i++) {
        fillTestInput(input.data(), TEST_CHANNELS, 0.1f + 0.01f * (i % 80), 0.001f);

        // Select nucleus
        thal_nucleus_type_t nucleus = nuclei[i % NUM_NUCLEI];

        // Vary arousal
        if (i % 10 == 0) {
            thalamus_set_arousal(thal, 0.2f + 0.6f * (i % 100) / 100.0f);
        }

        // Occasionally trigger burst
        if (i % 50 == 0) {
            thalamus_trigger_burst(thal, nucleus);
        }

        // Relay
        int result = thalamus_relay(thal, nucleus, input.data(), TEST_CHANNELS,
                                    output.data(), TEST_CHANNELS);
        EXPECT_GE(result, 0);

        // Step simulation
        if (i % 5 == 0) {
            thalamus_step(thal, 1.0f);
        }

        // Verify output sanity every 100 iterations
        if (i % 100 == 0) {
            for (uint32_t j = 0; j < TEST_CHANNELS; j++) {
                EXPECT_FALSE(std::isnan(output[j])) << "NaN at iteration " << i;
                EXPECT_FALSE(std::isinf(output[j])) << "Inf at iteration " << i;
                EXPECT_GE(output[j], 0.0f);
                EXPECT_LE(output[j], 1.0f);
            }
        }
    }
}

TEST_F(ThalamusRegressionTest, Stress_LongSimulation) {
    ASSERT_NE(thal, nullptr);

    std::vector<float> input(TEST_CHANNELS, 0.5f);
    std::vector<float> output(TEST_CHANNELS);

    for (int i = 0; i < 50000; i++) {
        // Relay every 10 steps
        if (i % 10 == 0) {
            input[i % TEST_CHANNELS] = 0.3f + 0.4f * (i % 100) / 100.0f;
            thalamus_relay(thal, THAL_NUCLEUS_LGN, input.data(), TEST_CHANNELS,
                           output.data(), TEST_CHANNELS);
        }

        // Step
        thalamus_step(thal, 0.1f);

        // Periodic sanity checks
        if (i % 1000 == 0) {
            float rate = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
            EXPECT_FALSE(std::isnan(rate));
            EXPECT_FALSE(std::isinf(rate));
            EXPECT_GE(rate, 0.0f);
        }
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
