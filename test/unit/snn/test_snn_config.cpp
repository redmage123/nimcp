/**
 * @file test_snn_config.cpp
 * @brief Unit tests for SNN Configuration module
 *
 * TEST COVERAGE:
 * - Configuration lifecycle (default, create, validate, destroy)
 * - Preset configurations (feedforward, reservoir, cortical column)
 * - Encoder configuration
 * - Training configuration
 * - Integration configuration
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SnnConfigTest : public ::testing::Test {
protected:
    snn_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(snn_config_t));
    }

    void TearDown() override {
        snn_config_destroy(&config);
    }
};

//=============================================================================
// snn_config_default Tests
//=============================================================================

TEST_F(SnnConfigTest, DefaultConfigSetsReasonableValues) {
    int result = snn_config_default(&config);
    EXPECT_EQ(SNN_SUCCESS, result);

    // Timing parameters
    EXPECT_FLOAT_EQ(SNN_DT_DEFAULT, config.dt);
    EXPECT_FLOAT_EQ(SNN_REFRACTORY_DEFAULT, config.t_ref);
    EXPECT_GT(config.tau_mem, 0.0f);
    EXPECT_GT(config.tau_syn, 0.0f);

    // Voltage parameters
    EXPECT_LT(config.v_reset, config.v_thresh);
    EXPECT_LE(config.v_rest, config.v_thresh);

    // Training defaults
    EXPECT_EQ(SNN_TRAIN_STDP, config.train_mode);
    EXPECT_TRUE(config.enable_stdp);
    EXPECT_GT(config.learning_rate, 0.0f);

    // Encoder defaults
    EXPECT_EQ(SNN_ENCODE_POISSON, config.encoder.method);
    EXPECT_GT(config.encoder.max_rate, 0.0f);
    EXPECT_GT(config.encoder.time_window, 0.0f);

    // Decoder defaults
    EXPECT_EQ(SNN_DECODE_RATE, config.decoder.method);
    EXPECT_GT(config.decoder.time_window, 0.0f);
}

TEST_F(SnnConfigTest, DefaultConfigReturnsErrorOnNullPointer) {
    int result = snn_config_default(nullptr);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, DefaultConfigSetsAdaptiveDtBounds) {
    snn_config_default(&config);

    EXPECT_GE(config.dt, SNN_DT_MIN);
    EXPECT_LE(config.dt, SNN_DT_MAX);
}

//=============================================================================
// snn_config_feedforward Tests
//=============================================================================

TEST_F(SnnConfigTest, FeedforwardConfigCreatesCorrectDimensions) {
    int result = snn_config_feedforward(&config, 784, 256, 10);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(784u, config.n_inputs);
    EXPECT_EQ(10u, config.n_outputs);
    EXPECT_EQ(3u, config.n_populations);  // input, hidden, output
}

TEST_F(SnnConfigTest, FeedforwardConfigWithNoHiddenLayer) {
    int result = snn_config_feedforward(&config, 100, 0, 10);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(100u, config.n_inputs);
    EXPECT_EQ(10u, config.n_outputs);
    EXPECT_EQ(2u, config.n_populations);  // input, output only
}

TEST_F(SnnConfigTest, FeedforwardConfigReturnsErrorOnNullPointer) {
    int result = snn_config_feedforward(nullptr, 100, 50, 10);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, FeedforwardConfigReturnsErrorOnZeroInputs) {
    int result = snn_config_feedforward(&config, 0, 50, 10);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, FeedforwardConfigReturnsErrorOnZeroOutputs) {
    int result = snn_config_feedforward(&config, 100, 50, 0);
    EXPECT_NE(SNN_SUCCESS, result);
}

//=============================================================================
// snn_config_multilayer Tests
//=============================================================================

TEST_F(SnnConfigTest, MultilayerConfigCreatesCorrectDimensions) {
    uint32_t layers[] = {100, 64, 32, 10};
    int result = snn_config_multilayer(&config, layers, 4);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(100u, config.n_inputs);
    EXPECT_EQ(10u, config.n_outputs);
    EXPECT_EQ(4u, config.n_populations);
}

TEST_F(SnnConfigTest, MultilayerConfigReturnsErrorOnNullPointer) {
    uint32_t layers[] = {100, 50, 10};
    int result = snn_config_multilayer(nullptr, layers, 3);
    EXPECT_NE(SNN_SUCCESS, result);

    result = snn_config_multilayer(&config, nullptr, 3);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, MultilayerConfigReturnsErrorOnTooFewLayers) {
    uint32_t layers[] = {100};
    int result = snn_config_multilayer(&config, layers, 1);
    EXPECT_NE(SNN_SUCCESS, result);
}

//=============================================================================
// snn_config_reservoir Tests
//=============================================================================

TEST_F(SnnConfigTest, ReservoirConfigCreatesCorrectDimensions) {
    int result = snn_config_reservoir(&config, 100, 500, 10, 0.1f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(100u, config.n_inputs);
    EXPECT_EQ(10u, config.n_outputs);
    EXPECT_EQ(3u, config.n_populations);  // input, reservoir, output
}

TEST_F(SnnConfigTest, ReservoirConfigEnablesSTDP) {
    snn_config_reservoir(&config, 100, 500, 10, 0.1f);
    EXPECT_TRUE(config.enable_stdp);
}

TEST_F(SnnConfigTest, ReservoirConfigReturnsErrorOnInvalidConnectivity) {
    int result = snn_config_reservoir(&config, 100, 500, 10, 1.5f);
    EXPECT_NE(SNN_SUCCESS, result);

    result = snn_config_reservoir(&config, 100, 500, 10, -0.1f);
    EXPECT_NE(SNN_SUCCESS, result);
}

//=============================================================================
// snn_config_cortical_column Tests
//=============================================================================

TEST_F(SnnConfigTest, CorticalColumnConfigCreates6Populations) {
    int result = snn_config_cortical_column(&config, 10, 20);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(6u, config.n_populations);
}

TEST_F(SnnConfigTest, CorticalColumnConfigEnablesBiologicalFeatures) {
    snn_config_cortical_column(&config, 10, 20);

    EXPECT_TRUE(config.use_axon_delays);
    EXPECT_TRUE(config.use_dendritic_integration);
    EXPECT_TRUE(config.enable_stdp);
}

//=============================================================================
// snn_config_validate Tests
//=============================================================================

TEST_F(SnnConfigTest, ValidateAcceptsValidFeedforwardConfig) {
    snn_config_feedforward(&config, 100, 50, 10);

    int result = snn_config_validate(&config);
    EXPECT_EQ(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, ValidateRejectsNullPointer) {
    int result = snn_config_validate(nullptr);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, ValidateRejectsZeroInputs) {
    snn_config_default(&config);
    config.n_inputs = 0;
    config.n_outputs = 10;

    int result = snn_config_validate(&config);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, ValidateRejectsZeroOutputs) {
    snn_config_default(&config);
    config.n_inputs = 10;
    config.n_outputs = 0;

    int result = snn_config_validate(&config);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, ValidateRejectsInvalidDt) {
    snn_config_feedforward(&config, 10, 5, 2);
    config.dt = SNN_DT_MIN / 2.0f;

    int result = snn_config_validate(&config);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, ValidateRejectsNegativeTauMem) {
    snn_config_feedforward(&config, 10, 5, 2);
    config.tau_mem = -1.0f;

    int result = snn_config_validate(&config);
    EXPECT_NE(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, ValidateRejectsInvalidThresholds) {
    snn_config_feedforward(&config, 10, 5, 2);
    config.v_thresh = -80.0f;
    config.v_reset = -60.0f;  // v_thresh < v_reset is invalid

    int result = snn_config_validate(&config);
    EXPECT_NE(SNN_SUCCESS, result);
}

//=============================================================================
// snn_config_destroy Tests
//=============================================================================

TEST_F(SnnConfigTest, DestroySafeOnNullPointer) {
    // Should not crash
    snn_config_destroy(nullptr);
}

TEST_F(SnnConfigTest, DestroyIsIdempotent) {
    snn_config_feedforward(&config, 10, 5, 2);

    snn_config_destroy(&config);
    // Should be safe to call again
    snn_config_destroy(&config);
}

//=============================================================================
// Encoder Configuration Tests
//=============================================================================

TEST_F(SnnConfigTest, EncoderRateSetsCorrectValues) {
    snn_config_default(&config);
    int result = snn_config_encoder_rate(&config, 200.0f, 50.0f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_ENCODE_RATE, config.encoder.method);
    EXPECT_FLOAT_EQ(200.0f, config.encoder.max_rate);
    EXPECT_FLOAT_EQ(50.0f, config.encoder.time_window);
}

TEST_F(SnnConfigTest, EncoderPopulationSetsCorrectValues) {
    snn_config_default(&config);
    int result = snn_config_encoder_population(&config, 10, 0.2f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_ENCODE_POPULATION, config.encoder.method);
    EXPECT_EQ(10u, config.encoder.population_size);
    EXPECT_FLOAT_EQ(0.2f, config.encoder.sigma);
}

TEST_F(SnnConfigTest, EncoderLatencySetsCorrectValues) {
    snn_config_default(&config);
    int result = snn_config_encoder_latency(&config, 20.0f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_ENCODE_LATENCY, config.encoder.method);
    EXPECT_FLOAT_EQ(20.0f, config.encoder.time_window);
}

TEST_F(SnnConfigTest, EncoderRateRejectsInvalidParams) {
    snn_config_default(&config);

    int result = snn_config_encoder_rate(&config, -100.0f, 50.0f);
    EXPECT_NE(SNN_SUCCESS, result);

    result = snn_config_encoder_rate(&config, 100.0f, -50.0f);
    EXPECT_NE(SNN_SUCCESS, result);
}

//=============================================================================
// Training Configuration Tests
//=============================================================================

TEST_F(SnnConfigTest, TrainStdpSetsCorrectMode) {
    snn_config_default(&config);
    int result = snn_config_train_stdp(&config, 0.01f, 20.0f, 0.005f, 0.00525f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_TRAIN_STDP, config.train_mode);
    EXPECT_TRUE(config.enable_stdp);
    EXPECT_FLOAT_EQ(0.01f, config.learning_rate);
}

TEST_F(SnnConfigTest, TrainRstdpSetsCorrectMode) {
    snn_config_default(&config);
    int result = snn_config_train_rstdp(&config, 0.01f, 0.99f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_TRAIN_R_STDP, config.train_mode);
    EXPECT_TRUE(config.enable_stdp);
    EXPECT_TRUE(config.enable_reward_modulation);
}

TEST_F(SnnConfigTest, TrainSurrogateSetsCorrectMode) {
    snn_config_default(&config);
    int result = snn_config_train_surrogate(&config, SNN_SURROGATE_FAST_SIGMOID, 10.0f, 0.001f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_TRAIN_SURROGATE, config.train_mode);
    EXPECT_EQ(SNN_SURROGATE_FAST_SIGMOID, config.surrogate);
    EXPECT_FLOAT_EQ(10.0f, config.surrogate_beta);
    EXPECT_FLOAT_EQ(0.001f, config.learning_rate);
}

TEST_F(SnnConfigTest, TrainEpropSetsCorrectMode) {
    snn_config_default(&config);
    int result = snn_config_train_eprop(&config, 0.01f, 0.95f);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(SNN_TRAIN_EPROP, config.train_mode);
}

TEST_F(SnnConfigTest, TrainSurrogateRejectsInvalidBeta) {
    snn_config_default(&config);
    int result = snn_config_train_surrogate(&config, SNN_SURROGATE_SIGMOID, 0.0f, 0.01f);
    EXPECT_NE(SNN_SUCCESS, result);

    result = snn_config_train_surrogate(&config, SNN_SURROGATE_SIGMOID, -1.0f, 0.01f);
    EXPECT_NE(SNN_SUCCESS, result);
}

//=============================================================================
// Integration Configuration Tests
//=============================================================================

TEST_F(SnnConfigTest, EnableBioAsyncSetsFlag) {
    snn_config_default(&config);

    snn_config_enable_bio_async(&config, true);
    EXPECT_TRUE(config.enable_bio_async);

    snn_config_enable_bio_async(&config, false);
    EXPECT_FALSE(config.enable_bio_async);
}

TEST_F(SnnConfigTest, EnableImmuneSetsFlag) {
    snn_config_default(&config);

    snn_config_enable_immune(&config, true);
    EXPECT_TRUE(config.enable_immune);

    snn_config_enable_immune(&config, false);
    EXPECT_FALSE(config.enable_immune);
}

TEST_F(SnnConfigTest, EnableAxonDelaysSetsFlag) {
    snn_config_default(&config);

    snn_config_enable_axon_delays(&config, true);
    EXPECT_TRUE(config.use_axon_delays);

    snn_config_enable_axon_delays(&config, false);
    EXPECT_FALSE(config.use_axon_delays);
}

TEST_F(SnnConfigTest, EnableDendritesSetsFlag) {
    snn_config_default(&config);

    snn_config_enable_dendrites(&config, true);
    EXPECT_TRUE(config.use_dendritic_integration);

    snn_config_enable_dendrites(&config, false);
    EXPECT_FALSE(config.use_dendritic_integration);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SnnConfigTest, CloneCreatesIdenticalCopy) {
    snn_config_feedforward(&config, 100, 50, 10);
    config.enable_stdp = true;
    config.learning_rate = 0.05f;

    snn_config_t clone;
    int result = snn_config_clone(&config, &clone);
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(config.n_inputs, clone.n_inputs);
    EXPECT_EQ(config.n_outputs, clone.n_outputs);
    EXPECT_EQ(config.n_populations, clone.n_populations);
    EXPECT_EQ(config.enable_stdp, clone.enable_stdp);
    EXPECT_FLOAT_EQ(config.learning_rate, clone.learning_rate);
}

TEST_F(SnnConfigTest, PrintProducesOutput) {
    snn_config_feedforward(&config, 100, 50, 10);

    char buffer[1024];
    int len = snn_config_print(&config, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_NE(nullptr, strstr(buffer, "SNN Configuration"));
}

TEST_F(SnnConfigTest, PrintHandlesSmallBuffer) {
    snn_config_feedforward(&config, 100, 50, 10);

    char buffer[10];  // Too small
    int len = snn_config_print(&config, buffer, sizeof(buffer));

    // Should return needed size or truncated length
    EXPECT_GT(len, 0);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SnnConfigTest, LargeNetworkConfigValidates) {
    // Large but valid configuration
    snn_config_feedforward(&config, 10000, 1000, 100);

    int result = snn_config_validate(&config);
    EXPECT_EQ(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, MinimalNetworkConfigValidates) {
    // Minimal valid configuration
    snn_config_feedforward(&config, 1, 0, 1);

    int result = snn_config_validate(&config);
    EXPECT_EQ(SNN_SUCCESS, result);
}

TEST_F(SnnConfigTest, AllSurrogateTypesValidate) {
    for (int s = 0; s < SNN_SURROGATE_COUNT; s++) {
        snn_config_default(&config);
        int result = snn_config_train_surrogate(&config, (snn_surrogate_t)s, 10.0f, 0.01f);
        EXPECT_EQ(SNN_SUCCESS, result) << "Surrogate type " << s << " failed";
    }
}

TEST_F(SnnConfigTest, AllEncodingMethodsSupported) {
    // Test that all encoding methods are properly defined
    EXPECT_EQ(0, SNN_ENCODE_RATE);
    EXPECT_EQ(7, SNN_ENCODE_COUNT);
}

TEST_F(SnnConfigTest, AllDecodingMethodsSupported) {
    // Test that all decoding methods are properly defined
    EXPECT_EQ(0, SNN_DECODE_RATE);
    EXPECT_EQ(4, SNN_DECODE_COUNT);
}
