/**
 * @file test_lnn_config.cpp
 * @brief Unit tests for LNN Configuration module
 *
 * TEST COVERAGE:
 * - Configuration lifecycle (create, default, validate, destroy)
 * - NCP configuration creation
 * - Layer configuration defaults and validation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_types.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LnnConfigTest : public ::testing::Test {
protected:
    lnn_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(lnn_config_t));
    }

    void TearDown() override {
        lnn_config_destroy(&config);
    }
};

//=============================================================================
// lnn_config_default Tests
//=============================================================================

TEST_F(LnnConfigTest, DefaultConfigSetsReasonableValues) {
    int result = lnn_config_default(&config);
    EXPECT_EQ(0, result);

    // ODE settings
    EXPECT_EQ(LNN_ODE_RK4, config.default_ode_method);
    EXPECT_FLOAT_EQ(1.0f, config.default_dt);
    EXPECT_GT(config.adaptive_error_tol, 0.0f);

    // Training
    EXPECT_GT(config.gradient_clip_norm, 0.0f);
    EXPECT_EQ(LNN_TRAIN_ADJOINT, config.train_mode);

    // Parallelization
    EXPECT_TRUE(config.enable_simd);
}

TEST_F(LnnConfigTest, DefaultConfigReturnsErrorOnNullPointer) {
    int result = lnn_config_default(nullptr);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, DefaultConfigSetsAdaptiveDtBounds) {
    lnn_config_default(&config);

    EXPECT_LT(config.adaptive_dt_min, config.adaptive_dt_max);
    EXPECT_GT(config.adaptive_dt_min, 0.0f);
}

//=============================================================================
// lnn_config_ncp Tests
//=============================================================================

TEST_F(LnnConfigTest, NcpConfigCreatesCorrectLayerCount) {
    int result = lnn_config_ncp(&config, 8, 16, 8, 4);
    EXPECT_EQ(0, result);

    // NCP creates 4 layers: sensory, inter, command, motor
    EXPECT_EQ(4u, config.n_layers);
    EXPECT_EQ(8u, config.n_inputs);
    EXPECT_EQ(4u, config.n_outputs);
}

TEST_F(LnnConfigTest, NcpConfigStoresNeuronCounts) {
    lnn_config_ncp(&config, 10, 20, 8, 5);

    EXPECT_EQ(10u, config.ncp_sensory);
    EXPECT_EQ(20u, config.ncp_inter);
    EXPECT_EQ(8u, config.ncp_command);
    EXPECT_EQ(5u, config.ncp_motor);
}

TEST_F(LnnConfigTest, NcpConfigAllocatesLayerConfigs) {
    lnn_config_ncp(&config, 4, 8, 4, 2);

    ASSERT_NE(nullptr, config.layer_configs);
    EXPECT_EQ(4u, config.layer_configs[0].n_neurons);  // sensory
    EXPECT_EQ(8u, config.layer_configs[1].n_neurons);  // inter
    EXPECT_EQ(4u, config.layer_configs[2].n_neurons);  // command
    EXPECT_EQ(2u, config.layer_configs[3].n_neurons);  // motor
}

TEST_F(LnnConfigTest, NcpConfigReturnsErrorOnNullPointer) {
    int result = lnn_config_ncp(nullptr, 4, 8, 4, 2);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, NcpConfigReturnsErrorOnZeroInputs) {
    int result = lnn_config_ncp(&config, 0, 8, 4, 2);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, NcpConfigReturnsErrorOnZeroOutputs) {
    int result = lnn_config_ncp(&config, 4, 8, 4, 0);
    EXPECT_NE(0, result);
}

//=============================================================================
// lnn_config_validate Tests
//=============================================================================

TEST_F(LnnConfigTest, ValidateAcceptsValidDefaultConfig) {
    // Default config alone isn't complete - use NCP to get a valid config
    lnn_config_ncp(&config, 4, 8, 4, 2);

    int result = lnn_config_validate(&config);
    EXPECT_EQ(0, result);
}

TEST_F(LnnConfigTest, ValidateAcceptsValidNcpConfig) {
    lnn_config_ncp(&config, 4, 8, 4, 2);

    int result = lnn_config_validate(&config);
    EXPECT_EQ(0, result);
}

TEST_F(LnnConfigTest, ValidateRejectsNullPointer) {
    int result = lnn_config_validate(nullptr);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, ValidateRejectsZeroInputs) {
    lnn_config_default(&config);
    config.n_inputs = 0;
    config.n_outputs = 2;

    int result = lnn_config_validate(&config);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, ValidateRejectsZeroOutputs) {
    lnn_config_default(&config);
    config.n_inputs = 4;
    config.n_outputs = 0;

    int result = lnn_config_validate(&config);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, ValidateRejectsNegativeDt) {
    lnn_config_ncp(&config, 4, 8, 4, 2);
    config.default_dt = -1.0f;

    int result = lnn_config_validate(&config);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, ValidateRejectsZeroDt) {
    lnn_config_ncp(&config, 4, 8, 4, 2);
    config.default_dt = 0.0f;

    int result = lnn_config_validate(&config);
    EXPECT_NE(0, result);
}

TEST_F(LnnConfigTest, ValidateRejectsNegativeGradientClip) {
    lnn_config_ncp(&config, 4, 8, 4, 2);
    config.gradient_clip_norm = -1.0f;

    int result = lnn_config_validate(&config);
    EXPECT_NE(0, result);
}

//=============================================================================
// lnn_config_destroy Tests
//=============================================================================

TEST_F(LnnConfigTest, DestroySafeOnNullPointer) {
    // Should not crash
    lnn_config_destroy(nullptr);
}

TEST_F(LnnConfigTest, DestroyFreesLayerConfigs) {
    lnn_config_ncp(&config, 4, 8, 4, 2);
    ASSERT_NE(nullptr, config.layer_configs);

    lnn_config_destroy(&config);

    // After destroy, layer_configs should be NULL
    EXPECT_EQ(nullptr, config.layer_configs);
}

TEST_F(LnnConfigTest, DestroyIsIdempotent) {
    lnn_config_ncp(&config, 4, 8, 4, 2);

    lnn_config_destroy(&config);
    // Should be safe to call again
    lnn_config_destroy(&config);
}

//=============================================================================
// Layer Configuration Tests
//=============================================================================

TEST_F(LnnConfigTest, LayerConfigHasCorrectDefaults) {
    lnn_config_ncp(&config, 4, 8, 4, 2);

    for (uint32_t i = 0; i < config.n_layers; i++) {
        lnn_layer_config_t* lc = &config.layer_configs[i];

        // Check tau bounds make sense
        EXPECT_LT(lc->tau_min, lc->tau_max);
        EXPECT_GT(lc->tau_base_init, 0.0f);
        EXPECT_LE(lc->tau_base_init, lc->tau_max);
        EXPECT_GE(lc->tau_base_init, lc->tau_min);

        // Check dt is positive
        EXPECT_GT(lc->dt, 0.0f);

        // Check sparsity is in valid range
        EXPECT_GE(lc->sparsity, 0.0f);
        EXPECT_LT(lc->sparsity, 1.0f);
    }
}

TEST_F(LnnConfigTest, NcpLayersHaveNcpWiring) {
    lnn_config_ncp(&config, 4, 8, 4, 2);

    for (uint32_t i = 0; i < config.n_layers; i++) {
        EXPECT_EQ(LNN_WIRING_NCP, config.layer_configs[i].wiring_type);
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(LnnConfigTest, LargeNetworkConfigValidates) {
    // Large but valid configuration
    lnn_config_ncp(&config, 128, 512, 128, 64);

    int result = lnn_config_validate(&config);
    EXPECT_EQ(0, result);
}

TEST_F(LnnConfigTest, MinimalNetworkConfigValidates) {
    // Minimal valid configuration
    lnn_config_ncp(&config, 1, 1, 1, 1);

    int result = lnn_config_validate(&config);
    EXPECT_EQ(0, result);
}
