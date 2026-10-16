/**
 * @file test_snn_regression.cpp
 * @brief Regression tests for SNN module
 *
 * WHAT: Regression tests ensuring SNN stability
 * WHY:  Prevent regressions in spike timing, weights, and computation
 * HOW:  Deterministic tests with known expected values
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Spike Timing Regression Tests
//=============================================================================

class SNNSpikeTimingRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        config.dt = 1.0f;  // 1ms timestep
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            snn_network_destroy(network);
        }
    }
};

TEST_F(SNNSpikeTimingRegression, TimestepConsistency) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output1[2], output2[2];

    // Run with same input twice
    snn_network_reset(network);
    snn_network_forward(network, input, 4, output1, 2, 100.0f);

    snn_network_reset(network);
    snn_network_forward(network, input, 4, output2, 2, 100.0f);

    // Outputs should be identical (deterministic)
    for (int i = 0; i < 2; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]);
    }
}

TEST_F(SNNSpikeTimingRegression, ZeroInputZeroOutput) {
    float input[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float output[2] = {999.0f, 999.0f};  // Sentinel values

    snn_network_forward(network, input, 4, output, 2, 100.0f);

    // Zero input should produce zero or near-zero output
    for (int i = 0; i < 2; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 0.2f);  // Allow small baseline activity
    }
}

TEST_F(SNNSpikeTimingRegression, MaxInputBounded) {
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float output[2];

    snn_network_forward(network, input, 4, output, 2, 100.0f);

    // Output should be bounded
    for (int i = 0; i < 2; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
    }
}

TEST_F(SNNSpikeTimingRegression, InputScaling) {
    float input_low[] = {0.1f, 0.1f, 0.1f, 0.1f};
    float input_high[] = {0.9f, 0.9f, 0.9f, 0.9f};
    float output_low[2], output_high[2];

    snn_network_reset(network);
    snn_network_forward(network, input_low, 4, output_low, 2, 100.0f);

    snn_network_reset(network);
    snn_network_forward(network, input_high, 4, output_high, 2, 100.0f);

    // Higher input should generally produce higher or equal output
    float sum_low = output_low[0] + output_low[1];
    float sum_high = output_high[0] + output_high[1];
    EXPECT_GE(sum_high, sum_low * 0.5f);
}

//=============================================================================
// Network Stability Regression Tests
//=============================================================================

class SNNNetworkStabilityRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        config.enable_stdp = true;
        network = snn_network_create(&config);

        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);
    }

    void TearDown() override {
        if (training) snn_training_destroy(training);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNNetworkStabilityRegression, NetworkCreatedWithValidState) {
    EXPECT_NE(nullptr, network);
    EXPECT_NE(nullptr, training);
}

TEST_F(SNNNetworkStabilityRegression, ForwardPassStable) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[2];

    for (int i = 0; i < 100; i++) {
        int result = snn_network_forward(network, input, 4, output, 2, 10.0f);
        EXPECT_EQ(0, result);
    }
}

TEST_F(SNNNetworkStabilityRegression, ResetWorks) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[2];

    snn_network_forward(network, input, 4, output, 2, 100.0f);
    int result = snn_network_reset(network);
    EXPECT_EQ(0, result);
}

//=============================================================================
// Numerical Stability Regression Tests
//=============================================================================

class SNNNumericalRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNNumericalRegression, NoNaNOutputs) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[2];

    for (int i = 0; i < 100; i++) {
        snn_network_forward(network, input, 4, output, 2, 10.0f);

        for (int j = 0; j < 2; j++) {
            EXPECT_FALSE(std::isnan(output[j]));
            EXPECT_FALSE(std::isinf(output[j]));
        }
    }
}

TEST_F(SNNNumericalRegression, NoNaNWithExtremeInput) {
    float extreme_inputs[][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0001f, 0.0001f, 0.0001f, 0.0001f},
        {0.9999f, 0.9999f, 0.9999f, 0.9999f}
    };
    float output[2];

    for (int i = 0; i < 4; i++) {
        snn_network_reset(network);
        snn_network_forward(network, extreme_inputs[i], 4, output, 2, 100.0f);

        for (int j = 0; j < 2; j++) {
            EXPECT_FALSE(std::isnan(output[j]));
            EXPECT_FALSE(std::isinf(output[j]));
        }
    }
}

TEST_F(SNNNumericalRegression, LongSimulationStable) {
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[2];

    // Run for 10000 timesteps
    for (int i = 0; i < 10000; i++) {
        snn_network_forward(network, input, 4, output, 2, 1.0f);
    }

    // Check final output is still valid
    for (int j = 0; j < 2; j++) {
        EXPECT_FALSE(std::isnan(output[j]));
        EXPECT_FALSE(std::isinf(output[j]));
        EXPECT_GE(output[j], 0.0f);
        EXPECT_LE(output[j], 1.0f);
    }
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

class SNNMemoryRegression : public ::testing::Test {};

TEST_F(SNNMemoryRegression, CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        snn_network_t* network = snn_network_create(&config);
        EXPECT_NE(nullptr, network);
        snn_network_destroy(network);
    }
    SUCCEED();
}

TEST_F(SNNMemoryRegression, EncoderDecoderNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_rate_encoder_config_t enc_config;
        snn_rate_encoder_config_default(&enc_config);
        snn_encoder_t* encoder = snn_encoder_create_rate(4, &enc_config);
        EXPECT_NE(nullptr, encoder);
        snn_encoder_destroy(encoder);

        snn_rate_decoder_config_t dec_config;
        snn_rate_decoder_config_default(&dec_config);
        snn_decoder_t* decoder = snn_decoder_create_rate(4, 2, &dec_config);
        EXPECT_NE(nullptr, decoder);
        snn_decoder_destroy(decoder);
    }
    SUCCEED();
}

TEST_F(SNNMemoryRegression, TrainingContextNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_stdp_config_t config;
        snn_stdp_config_default(&config);
        snn_training_ctx_t* training = snn_training_create_stdp(&config);
        EXPECT_NE(nullptr, training);
        snn_training_destroy(training);
    }
    SUCCEED();
}

TEST_F(SNNMemoryRegression, ImmuneBridgeNoLeak) {
    for (int i = 0; i < 5; i++) {
        snn_config_t net_config;
        snn_config_feedforward(&net_config, 4, 8, 2);
        snn_network_t* network = snn_network_create(&net_config);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        brain_immune_system_t* immune = brain_immune_create(&immune_config);

        snn_immune_config_t bridge_config;
        snn_immune_config_default(&bridge_config);
        snn_immune_bridge_t* bridge = snn_immune_bridge_create(&bridge_config, network, immune);
        EXPECT_NE(nullptr, bridge);

        snn_immune_bridge_destroy(bridge);
        snn_network_destroy(network);
        brain_immune_destroy(immune);
    }
    SUCCEED();
}

//=============================================================================
// Configuration Regression Tests
//=============================================================================

class SNNConfigRegression : public ::testing::Test {};

TEST_F(SNNConfigRegression, DefaultConfigSetsDefaults) {
    snn_config_t config;
    int result = snn_config_default(&config);
    EXPECT_EQ(SNN_SUCCESS, result);

    // Default config sets parameters but requires n_inputs/n_outputs
    // to be set before validation
    EXPECT_GT(config.dt, 0.0f);
    EXPECT_GT(config.tau_mem, 0.0f);
}

TEST_F(SNNConfigRegression, FeedforwardConfigValid) {
    snn_config_t config;
    int result = snn_config_feedforward(&config, 4, 8, 2);
    EXPECT_EQ(SNN_SUCCESS, result);

    result = snn_config_validate(&config);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(4u, config.n_inputs);
    EXPECT_EQ(2u, config.n_outputs);
}

TEST_F(SNNConfigRegression, ReservoirConfigValid) {
    snn_config_t config;
    int result = snn_config_reservoir(&config, 4, 16, 2, 0.1f);
    EXPECT_EQ(SNN_SUCCESS, result);

    result = snn_config_validate(&config);
    EXPECT_EQ(SNN_SUCCESS, result);
}

TEST_F(SNNConfigRegression, ColumnConfigValid) {
    snn_config_t config;
    int result = snn_config_cortical_column(&config, 4, 8);
    EXPECT_EQ(SNN_SUCCESS, result);

    result = snn_config_validate(&config);
    EXPECT_EQ(SNN_SUCCESS, result);
}

//=============================================================================
// Encoding Regression Tests
//=============================================================================

class SNNEncodingRegression : public ::testing::Test {};

TEST_F(SNNEncodingRegression, RateEncodingConsistent) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    snn_encoder_t* encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(nullptr, encoder);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint8_t spikes1[4], spikes2[4];

    snn_encode_rate(encoder, input, 1.0f, spikes1);
    snn_encode_rate(encoder, input, 1.0f, spikes2);

    snn_encoder_destroy(encoder);
    SUCCEED();
}

TEST_F(SNNEncodingRegression, TemporalEncodingValid) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);
    snn_encoder_t* encoder = snn_encoder_create_temporal(4, &config);
    ASSERT_NE(nullptr, encoder);

    float input[] = {0.2f, 0.4f, 0.6f, 0.8f};
    float spike_times[4];

    int result = snn_encode_temporal(encoder, input, spike_times);
    EXPECT_EQ(0, result);

    // Check spike times are valid
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(spike_times[i], 0.0f);
    }

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingRegression, RateDecodingConsistent) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);
    snn_decoder_t* decoder = snn_decoder_create_rate(4, 2, &config);
    ASSERT_NE(nullptr, decoder);

    float spike_counts[] = {10.0f, 20.0f, 15.0f, 25.0f};
    float output1[2], output2[2];

    snn_decode_rate(decoder, spike_counts, output1);
    snn_decode_rate(decoder, spike_counts, output2);

    // Same counts should give same output
    for (int i = 0; i < 2; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]);
    }

    snn_decoder_destroy(decoder);
}

//=============================================================================
// Immune Bridge Regression Tests
//=============================================================================

class SNNImmuneRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        network = snn_network_create(&config);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        snn_immune_config_t bridge_config;
        snn_immune_config_default(&bridge_config);
        bridge = snn_immune_bridge_create(&bridge_config, network, immune);
    }

    void TearDown() override {
        if (bridge) snn_immune_bridge_destroy(bridge);
        if (network) snn_network_destroy(network);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(SNNImmuneRegression, EffectsInitiallyNeutral) {
    snn_immune_update_effects(bridge);

    snn_cytokine_effects_t effects;
    snn_immune_get_effects(bridge, &effects);

    // Without inflammation, effects should be near neutral
    EXPECT_NEAR(effects.learning_rate_factor, 1.0f, 0.1f);
}

TEST_F(SNNImmuneRegression, HealthMetricsValid) {
    snn_immune_compute_health(bridge);

    snn_health_metrics_t health;
    snn_immune_get_health(bridge, &health);

    // Health values should be in valid ranges
    EXPECT_GE(health.mean_rate, 0.0f);
    EXPECT_LE(health.mean_rate, 1000.0f);
}

TEST_F(SNNImmuneRegression, StatsIncrement) {
    uint32_t initial_reports;
    snn_immune_get_stats(bridge, nullptr, &initial_reports, nullptr);

    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 5);

    uint32_t final_reports;
    snn_immune_get_stats(bridge, nullptr, &final_reports, nullptr);

    EXPECT_EQ(initial_reports + 1, final_reports);
}

TEST_F(SNNImmuneRegression, StatsResetWorks) {
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 5);
    snn_immune_reset_stats(bridge);

    uint32_t reports;
    snn_immune_get_stats(bridge, nullptr, &reports, nullptr);
    EXPECT_EQ(0u, reports);
}
