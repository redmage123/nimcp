//=============================================================================
// test_thalamus_regression.cpp - Thalamus Regression Tests
//=============================================================================
/**
 * @file test_thalamus_regression.cpp
 * @brief Regression tests for thalamic nuclei system
 *
 * WHAT: Tests for determinism, memory safety, numerical stability
 * WHY:  Ensure thalamus behavior is stable across versions
 * HOW:  GTest framework with determinism, bounds, and consistency checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_thalamus.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ThalamusRegressionTest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;

    void SetUp() override {
        thalamus_config_t config;
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
// Determinism Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, DeterministicRelay) {
    ASSERT_NE(thal, nullptr);

    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f + 0.01f * i;
    }

    float output1[16], output2[16];

    // First run
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output1, 16);

    // Reset and run again with same input
    thalamus_reset(thal);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output2, 16);

    // Outputs should be identical
    for (int i = 0; i < 16; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]) << "Mismatch at index " << i;
    }
}

TEST_F(ThalamusRegressionTest, DeterministicWithAttention) {
    ASSERT_NE(thal, nullptr);

    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.6f;
    }

    // Set specific attention
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    float output1[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output1, 16);

    // Reset and repeat
    thalamus_reset(thal);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);

    float output2[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output2, 16);

    for (int i = 0; i < 16; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]);
    }
}

TEST_F(ThalamusRegressionTest, DeterministicStep) {
    ASSERT_NE(thal, nullptr);

    // Setup initial state
    float input[16] = {0.5f};
    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    // Step and record state
    for (int i = 0; i < 10; i++) {
        thalamus_step(thal, 1.0f);
    }
    float rate1 = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);

    // Reset and repeat
    thalamus_reset(thal);
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
    for (int i = 0; i < 10; i++) {
        thalamus_step(thal, 1.0f);
    }
    float rate2 = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);

    EXPECT_FLOAT_EQ(rate1, rate2);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, CreateDestroyNoLeak) {
    for (int i = 0; i < 100; i++) {
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = 64;
        thalamus_t* t = thalamus_create(&config);
        ASSERT_NE(t, nullptr);
        thalamus_destroy(t);
    }
}

TEST_F(ThalamusRegressionTest, NullPointerSafety) {
    // These should not crash
    thalamus_destroy(nullptr);
    thalamus_reset(nullptr);
    thalamus_step(nullptr, 1.0f);
    thalamus_set_arousal(nullptr, 0.5f);
    thalamus_set_attention(nullptr, THAL_NUCLEUS_LGN, 0.5f);
    thalamus_get_attention(nullptr, THAL_NUCLEUS_LGN);
    thalamus_get_mode(nullptr, THAL_NUCLEUS_LGN);
    thalamus_get_firing_rate(nullptr, THAL_NUCLEUS_LGN);
    thalamus_is_bio_async_connected(nullptr);
    thalamus_get_nucleus(nullptr, THAL_NUCLEUS_LGN);

    float buffer[16];
    thalamus_relay(nullptr, THAL_NUCLEUS_LGN, buffer, 16, buffer, 16);
    thalamus_get_output(nullptr, THAL_NUCLEUS_LGN, buffer, 16);
}

TEST_F(ThalamusRegressionTest, BufferBoundarySafety) {
    ASSERT_NE(thal, nullptr);

    // Very small buffers
    float small_input[1] = {0.5f};
    float small_output[1] = {0};
    int result = thalamus_relay(thal, THAL_NUCLEUS_LGN, small_input, 1, small_output, 1);
    EXPECT_GE(result, 0);

    // Empty buffers
    float empty_input[1] = {0};
    float empty_output[1] = {0};
    result = thalamus_relay(thal, THAL_NUCLEUS_LGN, empty_input, 0, empty_output, 0);
    EXPECT_GE(result, 0);
}

TEST_F(ThalamusRegressionTest, NucleusLifecycleNoLeak) {
    for (int i = 0; i < 100; i++) {
        thal_nucleus_config_t config;
        thal_nucleus_default_config(&config, THAL_NUCLEUS_LGN);
        config.num_neurons = 64;
        config.num_channels = 32;
        thal_nucleus_t* n = thal_nucleus_create(&config);
        ASSERT_NE(n, nullptr);
        thal_nucleus_destroy(n);
    }
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, OutputBounds) {
    ASSERT_NE(thal, nullptr);

    // Test with various input ranges
    float test_inputs[] = {0.0f, 0.5f, 1.0f, 2.0f, -1.0f};

    for (float test_val : test_inputs) {
        float input[16];
        for (int i = 0; i < 16; i++) {
            input[i] = test_val;
        }

        float output[16];
        thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

        // Outputs should be bounded [0, 1]
        for (int i = 0; i < 16; i++) {
            EXPECT_GE(output[i], 0.0f) << "Output below 0 for input " << test_val;
            EXPECT_LE(output[i], 1.0f) << "Output above 1 for input " << test_val;
        }
    }
}

TEST_F(ThalamusRegressionTest, AttentionBounds) {
    ASSERT_NE(thal, nullptr);

    // Test extreme attention values
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, -1.0f);
    EXPECT_GE(thalamus_get_attention(thal, THAL_NUCLEUS_LGN), 0.0f);

    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 2.0f);
    EXPECT_LE(thalamus_get_attention(thal, THAL_NUCLEUS_LGN), 1.0f);
}

TEST_F(ThalamusRegressionTest, ArousalBounds) {
    ASSERT_NE(thal, nullptr);

    thalamus_set_arousal(thal, -0.5f);
    EXPECT_GE(thal->global_arousal, 0.0f);

    thalamus_set_arousal(thal, 1.5f);
    EXPECT_LE(thal->global_arousal, 1.0f);
}

TEST_F(ThalamusRegressionTest, TonicFractionBounds) {
    ASSERT_NE(thal, nullptr);

    thal_nucleus_type_t types[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VA, THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };

    for (auto type : types) {
        float fraction = thalamus_get_tonic_fraction(thal, type);
        EXPECT_GE(fraction, 0.0f);
        EXPECT_LE(fraction, 1.0f);
    }
}

TEST_F(ThalamusRegressionTest, FiringRateNonNegative) {
    ASSERT_NE(thal, nullptr);

    float input[16] = {0.5f};
    float output[16];
    thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);

    for (int i = 0; i < 100; i++) {
        thalamus_step(thal, 1.0f);
        float rate = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
        EXPECT_GE(rate, 0.0f) << "Negative firing rate at step " << i;
    }
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, ResetRestoresInitialState) {
    ASSERT_NE(thal, nullptr);

    // Record initial state
    float initial_arousal = thal->global_arousal;
    thal_firing_mode_t initial_mode = thal->dominant_mode;

    // Modify state
    thalamus_set_arousal(thal, 0.1f);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.2f);

    // Reset
    thalamus_reset(thal);

    // Verify restoration
    EXPECT_FLOAT_EQ(thal->global_arousal, initial_arousal);
    EXPECT_EQ(thal->dominant_mode, initial_mode);
    EXPECT_EQ(thal->stats.total_signals_relayed, 0);
}

TEST_F(ThalamusRegressionTest, ModeTransitionConsistency) {
    ASSERT_NE(thal, nullptr);

    // Tonic mode at high arousal
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);

    // Burst mode at low arousal
    thalamus_set_arousal(thal, 0.1f);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_BURST);

    // Back to tonic
    thalamus_set_arousal(thal, 1.0f);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);
}

TEST_F(ThalamusRegressionTest, NucleusIndependence) {
    ASSERT_NE(thal, nullptr);

    // Set different states for different nuclei
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.9f);
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.3f);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_VPL);

    // Verify independence
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_LGN), 0.9f);
    EXPECT_FLOAT_EQ(thalamus_get_attention(thal, THAL_NUCLEUS_MGN), 0.3f);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_VPL), THAL_MODE_BURST);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_TONIC);
}

TEST_F(ThalamusRegressionTest, StatsAccumulateCorrectly) {
    ASSERT_NE(thal, nullptr);

    float input[16] = {0.5f};
    float output[16];

    // Relay through multiple nuclei
    for (int i = 0; i < 10; i++) {
        thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
    }
    for (int i = 0; i < 5; i++) {
        thalamus_relay(thal, THAL_NUCLEUS_MGN, input, 16, output, 16);
    }

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);

    EXPECT_EQ(stats.total_signals_relayed, 15);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_LGN], 10);
    EXPECT_EQ(stats.signals_per_nucleus[THAL_NUCLEUS_MGN], 5);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, DefaultConfigStable) {
    thalamus_config_t config;
    thalamus_default_config(&config);

    // These values should remain stable across versions
    EXPECT_EQ(config.neurons_per_nucleus, THAL_DEFAULT_NEURONS);
    EXPECT_FLOAT_EQ(config.attention_baseline, THAL_ATTENTION_BASELINE);
    EXPECT_FLOAT_EQ(config.burst_threshold, THAL_BURST_THRESHOLD);
    EXPECT_TRUE(config.enable_trn);
    EXPECT_TRUE(config.enable_mode_switching);
    EXPECT_TRUE(config.enable_attention_gating);
}

TEST_F(ThalamusRegressionTest, NucleusTypeNamesStable) {
    // Names should be stable for logging/debugging
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_LGN), "LGN");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_MGN), "MGN");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VPL), "VPL");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VPM), "VPM");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VA), "VA");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_VL), "VL");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_PULVINAR), "Pulvinar");
    EXPECT_STREQ(thal_nucleus_name(THAL_NUCLEUS_MD), "MD");
}

TEST_F(ThalamusRegressionTest, ModeNamesStable) {
    EXPECT_STREQ(thal_mode_name(THAL_MODE_TONIC), "Tonic");
    EXPECT_STREQ(thal_mode_name(THAL_MODE_BURST), "Burst");
    EXPECT_STREQ(thal_mode_name(THAL_MODE_INHIBITED), "Inhibited");
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, RapidRelayNoCorruption) {
    ASSERT_NE(thal, nullptr);

    float input[16], output[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }

    // Rapid relay through all nuclei
    for (int iteration = 0; iteration < 1000; iteration++) {
        thal_nucleus_type_t types[] = {
            THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
            THAL_NUCLEUS_VA, THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
        };

        for (auto type : types) {
            int result = thalamus_relay(thal, type, input, 16, output, 16);
            EXPECT_GE(result, 0);

            // Verify output bounds
            for (int i = 0; i < 16; i++) {
                EXPECT_GE(output[i], 0.0f);
                EXPECT_LE(output[i], 1.0f);
            }
        }
    }
}

TEST_F(ThalamusRegressionTest, LongSimulationStable) {
    ASSERT_NE(thal, nullptr);

    float input[16] = {0.5f};
    float output[16];

    // Long simulation
    for (int i = 0; i < 10000; i++) {
        thalamus_relay(thal, THAL_NUCLEUS_LGN, input, 16, output, 16);
        thalamus_step(thal, 1.0f);

        // Check for NaN or Inf
        for (int j = 0; j < 16; j++) {
            EXPECT_FALSE(std::isnan(output[j])) << "NaN at step " << i;
            EXPECT_FALSE(std::isinf(output[j])) << "Inf at step " << i;
        }

        float rate = thalamus_get_firing_rate(thal, THAL_NUCLEUS_LGN);
        EXPECT_FALSE(std::isnan(rate));
        EXPECT_FALSE(std::isinf(rate));
    }
}

TEST_F(ThalamusRegressionTest, ModeTransitionStress) {
    ASSERT_NE(thal, nullptr);

    // Rapidly switch modes
    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0) {
            thalamus_set_arousal(thal, 1.0f);  // Tonic
        } else {
            thalamus_set_arousal(thal, 0.1f);  // Burst
        }

        // Should not crash or corrupt state
        float arousal = thal->global_arousal;
        EXPECT_GE(arousal, 0.0f);
        EXPECT_LE(arousal, 1.0f);
    }
}

//=============================================================================
// Error Handling Regression Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, InvalidNucleusType) {
    ASSERT_NE(thal, nullptr);

    float buffer[16] = {0};

    // Invalid nucleus type should fail gracefully
    thal_nucleus_t* n = thalamus_get_nucleus(thal, (thal_nucleus_type_t)99);
    EXPECT_EQ(n, nullptr);

    // Name should return "Unknown"
    EXPECT_STREQ(thal_nucleus_name((thal_nucleus_type_t)99), "Unknown");
    EXPECT_STREQ(thal_mode_name((thal_firing_mode_t)99), "Unknown");
}

TEST_F(ThalamusRegressionTest, InvalidChannelIndex) {
    ASSERT_NE(thal, nullptr);

    // Should fail for out-of-bounds channel
    int result = thalamus_set_channel_attention(thal, THAL_NUCLEUS_LGN, 9999, 0.5f);
    EXPECT_NE(result, 0);

    result = thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, 9999, 0.5f);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Relay Order Regression Tests
//=============================================================================

TEST_F(ThalamusRegressionTest, FirstOrderNucleiRelay) {
    ASSERT_NE(thal, nullptr);

    // First-order nuclei: LGN, MGN, VPL, VPM, VA, VL
    thal_nucleus_type_t first_order[] = {
        THAL_NUCLEUS_LGN, THAL_NUCLEUS_MGN, THAL_NUCLEUS_VPL,
        THAL_NUCLEUS_VPM, THAL_NUCLEUS_VA, THAL_NUCLEUS_VL
    };

    float input[16] = {0.7f};
    float output[16];

    for (auto type : first_order) {
        thal_nucleus_t* n = thalamus_get_nucleus(thal, type);
        ASSERT_NE(n, nullptr);
        EXPECT_EQ(n->order, THAL_ORDER_FIRST);

        int result = thalamus_relay(thal, type, input, 16, output, 16);
        EXPECT_GE(result, 0);
    }
}

TEST_F(ThalamusRegressionTest, HigherOrderNucleiRelay) {
    ASSERT_NE(thal, nullptr);

    // Higher-order nuclei: Pulvinar, MD
    thal_nucleus_type_t higher_order[] = {
        THAL_NUCLEUS_PULVINAR, THAL_NUCLEUS_MD
    };

    float input[16] = {0.7f};
    float output[16];

    for (auto type : higher_order) {
        thal_nucleus_t* n = thalamus_get_nucleus(thal, type);
        ASSERT_NE(n, nullptr);
        EXPECT_EQ(n->order, THAL_ORDER_HIGHER);

        int result = thalamus_relay(thal, type, input, 16, output, 16);
        EXPECT_GE(result, 0);
    }
}
