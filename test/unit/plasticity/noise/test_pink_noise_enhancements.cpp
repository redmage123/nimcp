/**
 * @file test_pink_noise_enhancements.cpp
 * @brief Comprehensive unit tests for Pink Noise Enhancement modules
 *
 * WHAT: Unit tests covering all 9 pink noise enhancement modules
 * WHY:  Ensure 100% code coverage and correctness of implementation
 * HOW:  Test all functions, methods, edge cases, and error paths
 *
 * MODULES TESTED:
 * 1. Multi-Scale Hierarchical Pink Noise
 * 2. Correlated Multi-Channel Pink Noise
 * 3. Criticality/Avalanche Integration
 * 4. Quantum-Inspired Pink Noise Bridge
 * 5. Pink Noise Immune Bridge
 * 6. Sleep/Wake Pink Noise Integration
 * 7. SIMD Vectorized Generation
 * 8. Real-Time Spectral Monitoring
 * 9. Spatial Correlation Pink Noise
 *
 * @version 1.0.0
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/noise/nimcp_pink_noise.h"
#include "plasticity/noise/nimcp_pink_noise_multiscale.h"
#include "plasticity/noise/nimcp_pink_noise_correlated.h"
#include "plasticity/noise/nimcp_pink_noise_criticality.h"
#include "plasticity/noise/nimcp_pink_noise_quantum_bridge.h"
#include "plasticity/noise/nimcp_pink_noise_immune_bridge.h"
#include "plasticity/noise/nimcp_pink_noise_sleep.h"
#include "plasticity/noise/nimcp_pink_noise_simd.h"
#include "plasticity/noise/nimcp_pink_noise_monitor.h"
#include "plasticity/noise/nimcp_pink_noise_spatial.h"

//=============================================================================
// Test Fixture Base
//=============================================================================

class PinkNoiseEnhancementsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// 1. Multi-Scale Hierarchical Pink Noise Tests
//=============================================================================

class MultiscaleTest : public PinkNoiseEnhancementsTest {
protected:
    pink_noise_multiscale_t* ms = nullptr;

    void TearDown() override {
        if (ms) {
            pink_noise_multiscale_destroy(ms);
            ms = nullptr;
        }
    }
};

TEST_F(MultiscaleTest, DefaultConfig_HasValidScales) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    EXPECT_EQ(config.num_scales, PINK_NOISE_DEFAULT_SCALES);
    EXPECT_GT(config.scales[0].timescale_ms, 0.0f);
    EXPECT_GT(config.global_amplitude, 0.0f);
}

TEST_F(MultiscaleTest, Create_NullConfig) {
    ms = pink_noise_multiscale_create(nullptr);
    EXPECT_EQ(ms, nullptr);
}

TEST_F(MultiscaleTest, Create_ValidConfig) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);
}

TEST_F(MultiscaleTest, Destroy_NullSafe) {
    pink_noise_multiscale_destroy(nullptr);
    SUCCEED();
}

TEST_F(MultiscaleTest, Step_GeneratesValues) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    int ret = pink_noise_multiscale_step(ms);
    EXPECT_EQ(ret, 0);

    // Check values are generated
    for (uint32_t i = 0; i < config.num_scales; i++) {
        float val = pink_noise_multiscale_get_scale(ms, i);
        EXPECT_TRUE(std::isfinite(val));
    }
}

TEST_F(MultiscaleTest, GetScale_OutOfBounds) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    float val = pink_noise_multiscale_get_scale(ms, 100);
    EXPECT_EQ(val, 0.0f);
}

TEST_F(MultiscaleTest, GetCombined_EqualWeights) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    pink_noise_multiscale_step(ms);
    float combined = pink_noise_multiscale_get_combined(ms, nullptr);
    EXPECT_TRUE(std::isfinite(combined));
}

TEST_F(MultiscaleTest, GetCombined_CustomWeights) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    pink_noise_multiscale_step(ms);
    float weights[PINK_NOISE_MAX_SCALES] = {1.0f, 0.5f, 0.25f, 0.125f};
    float combined = pink_noise_multiscale_get_combined(ms, weights);
    EXPECT_TRUE(std::isfinite(combined));
}

TEST_F(MultiscaleTest, SetCoupling_Valid) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    int ret = pink_noise_multiscale_set_coupling(ms, 1, 0.5f, 0.3f);
    EXPECT_EQ(ret, 0);
}

TEST_F(MultiscaleTest, SetCoupling_InvalidScale) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    int ret = pink_noise_multiscale_set_coupling(ms, 100, 0.5f, 0.3f);
    EXPECT_LT(ret, 0);
}

TEST_F(MultiscaleTest, SetAmplitude_Valid) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    int ret = pink_noise_multiscale_set_amplitude(ms, 0, 0.1f);
    EXPECT_EQ(ret, 0);
}

TEST_F(MultiscaleTest, GetStats_Valid) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    // Generate some samples
    for (int i = 0; i < 100; i++) {
        pink_noise_multiscale_step(ms);
    }

    pink_noise_multiscale_stats_t stats;
    int ret = pink_noise_multiscale_get_stats(ms, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.total_samples, 0u);
}

TEST_F(MultiscaleTest, Reset_Valid) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    pink_noise_multiscale_step(ms);
    int ret = pink_noise_multiscale_reset(ms, 12345);
    EXPECT_EQ(ret, 0);
}

TEST_F(MultiscaleTest, GenerateBatch_Valid) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    float* outputs[PINK_NOISE_MAX_SCALES];
    for (uint32_t i = 0; i < config.num_scales; i++) {
        outputs[i] = new float[100];
    }

    int ret = pink_noise_multiscale_generate_batch(ms, outputs, 100);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < config.num_scales; i++) {
        for (int j = 0; j < 100; j++) {
            EXPECT_TRUE(std::isfinite(outputs[i][j]));
        }
        delete[] outputs[i];
    }
}

//=============================================================================
// 2. Correlated Multi-Channel Pink Noise Tests
//=============================================================================

class CorrelatedTest : public PinkNoiseEnhancementsTest {
protected:
    pink_noise_correlated_t* cn = nullptr;

    void TearDown() override {
        if (cn) {
            pink_noise_correlated_destroy(cn);
            cn = nullptr;
        }
    }
};

TEST_F(CorrelatedTest, NeuromodConfig_HasValidChannels) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    EXPECT_EQ(config.num_channels, PINK_NOISE_NEUROMOD_CHANNELS);
    EXPECT_EQ(config.correlation_type, PINK_CORR_NEUROMODULATORS);
}

TEST_F(CorrelatedTest, IndependentConfig_Identity) {
    pink_noise_correlated_config_t config = pink_noise_correlated_independent_config(4);
    EXPECT_EQ(config.num_channels, 4u);
    EXPECT_EQ(config.correlation_type, PINK_CORR_INDEPENDENT);
}

TEST_F(CorrelatedTest, Create_NullConfig) {
    cn = pink_noise_correlated_create(nullptr);
    EXPECT_EQ(cn, nullptr);
}

TEST_F(CorrelatedTest, Create_NeuromodConfig) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);
}

TEST_F(CorrelatedTest, Step_GeneratesValues) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    int ret = pink_noise_correlated_step(cn);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < config.num_channels; i++) {
        float val = pink_noise_correlated_get_channel(cn, i);
        EXPECT_TRUE(std::isfinite(val));
    }
}

TEST_F(CorrelatedTest, GetChannel_OutOfBounds) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    float val = pink_noise_correlated_get_channel(cn, 100);
    EXPECT_EQ(val, 0.0f);
}

TEST_F(CorrelatedTest, GetAll_Valid) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    pink_noise_correlated_step(cn);

    float values[PINK_NOISE_MAX_CHANNELS];
    int ret = pink_noise_correlated_get_all(cn, values);
    EXPECT_EQ(ret, 0);
}

TEST_F(CorrelatedTest, GetNamed_ValidName) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    pink_noise_correlated_step(cn);
    float val = pink_noise_correlated_get_named(cn, "dopamine");
    EXPECT_TRUE(std::isfinite(val));
}

TEST_F(CorrelatedTest, SetCorrelation_Valid) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    int ret = pink_noise_correlated_set_correlation(cn, 0, 1, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(CorrelatedTest, GetStats_Valid) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    for (int i = 0; i < 100; i++) {
        pink_noise_correlated_step(cn);
    }

    pink_noise_correlated_stats_t stats;
    int ret = pink_noise_correlated_get_stats(cn, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(CorrelatedTest, Reset_Valid) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    int ret = pink_noise_correlated_reset(cn, 42);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// 3. Criticality/Avalanche Integration Tests
//=============================================================================

class CriticalityTest : public PinkNoiseEnhancementsTest {
protected:
    criticality_analyzer_t* ca = nullptr;

    void TearDown() override {
        if (ca) {
            criticality_destroy(ca);
            ca = nullptr;
        }
    }
};

TEST_F(CriticalityTest, DefaultConfig_Valid) {
    criticality_config_t config = criticality_default_config();
    EXPECT_GT(config.threshold_high, 0.0f);
    EXPECT_EQ(config.target_alpha, 1.0f);
    EXPECT_EQ(config.target_tau, 1.5f);
}

TEST_F(CriticalityTest, Create_NullConfig) {
    ca = criticality_create(nullptr);
    EXPECT_EQ(ca, nullptr);
}

TEST_F(CriticalityTest, Create_ValidConfig) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);
}

TEST_F(CriticalityTest, Destroy_NullSafe) {
    criticality_destroy(nullptr);
    SUCCEED();
}

TEST_F(CriticalityTest, Update_WithSamples) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    // Feed samples
    for (int i = 0; i < 500; i++) {
        float sample = sinf(i * 0.1f) * 0.5f + (rand() % 100) * 0.01f;
        int ret = criticality_update(ca, sample);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(CriticalityTest, GetRegime_Initially) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    criticality_regime_t regime = criticality_get_regime(ca);
    EXPECT_EQ(regime, CRITICALITY_UNKNOWN);
}

TEST_F(CriticalityTest, GetIndex_Valid) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    float idx = criticality_get_index(ca);
    EXPECT_TRUE(std::isfinite(idx));
}

TEST_F(CriticalityTest, InAvalanche_InitiallyFalse) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    EXPECT_FALSE(criticality_in_avalanche(ca));
}

TEST_F(CriticalityTest, GetAmplitudeCorrection_Valid) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    float corr = criticality_get_amplitude_correction(ca);
    EXPECT_TRUE(std::isfinite(corr));
}

TEST_F(CriticalityTest, GenerateAvalanche_Valid) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    float output[100];
    uint32_t num_generated = 0;
    int ret = criticality_generate_avalanche(ca, output, 100, &num_generated);
    EXPECT_EQ(ret, 0);
}

TEST_F(CriticalityTest, GetAvalanches_Valid) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    avalanche_event_t avalanches[10];
    uint32_t count = 0;
    int ret = criticality_get_avalanches(ca, avalanches, 10, &count);
    EXPECT_EQ(ret, 0);
}

TEST_F(CriticalityTest, GetStats_Valid) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    criticality_stats_t stats;
    int ret = criticality_get_stats(ca, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(CriticalityTest, Reset_Valid) {
    criticality_config_t config = criticality_default_config();
    ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    int ret = criticality_reset(ca);
    EXPECT_EQ(ret, 0);
}

TEST_F(CriticalityTest, RegimeName_AllTypes) {
    EXPECT_STREQ(criticality_regime_name(CRITICALITY_SUBCRITICAL), "subcritical");
    EXPECT_STREQ(criticality_regime_name(CRITICALITY_CRITICAL), "critical");
    EXPECT_STREQ(criticality_regime_name(CRITICALITY_SUPERCRITICAL), "supercritical");
    EXPECT_STREQ(criticality_regime_name(CRITICALITY_UNKNOWN), "unknown");
}

//=============================================================================
// 4. Quantum-Inspired Pink Noise Bridge Tests
//=============================================================================

class QuantumTest : public PinkNoiseEnhancementsTest {
protected:
    pink_quantum_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            pink_quantum_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(QuantumTest, DefaultConfig_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    EXPECT_EQ(config.target_alpha, 1.0f);
    EXPECT_GT(config.amplitude, 0.0f);
}

TEST_F(QuantumTest, Create_NullConfig) {
    bridge = pink_quantum_create(nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(QuantumTest, Create_ValidConfig) {
    pink_quantum_config_t config = pink_quantum_default_config();
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(QuantumTest, Destroy_NullSafe) {
    pink_quantum_destroy(nullptr);
    SUCCEED();
}

TEST_F(QuantumTest, GenerateSample_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    float sample;
    int ret = pink_quantum_generate_sample(bridge, &sample);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(QuantumTest, GenerateBatch_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    float samples[100];
    int ret = pink_quantum_generate_batch(bridge, samples, 100);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(std::isfinite(samples[i]));
    }
}

TEST_F(QuantumTest, AnnealStep_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    config.method = PINK_QUANTUM_ANNEALING;
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    float energy = pink_quantum_anneal_step(bridge, 1.0f);
    EXPECT_TRUE(std::isfinite(energy));
}

TEST_F(QuantumTest, TernaryFilter_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    config.method = PINK_QUANTUM_TERNARY;
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    float output = pink_quantum_ternary_filter(bridge, 0.5f);
    EXPECT_TRUE(std::isfinite(output));
}

TEST_F(QuantumTest, WalkStep_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    config.method = PINK_QUANTUM_WALK;
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    float val = pink_quantum_walk_step(bridge);
    EXPECT_TRUE(std::isfinite(val));
}

TEST_F(QuantumTest, SetMethod_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = pink_quantum_set_method(bridge, PINK_QUANTUM_TERNARY);
    EXPECT_EQ(ret, 0);
}

TEST_F(QuantumTest, SetEnabled_Toggle) {
    pink_quantum_config_t config = pink_quantum_default_config();
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = pink_quantum_set_enabled(bridge, false);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(pink_quantum_is_enabled(bridge));

    ret = pink_quantum_set_enabled(bridge, true);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(pink_quantum_is_enabled(bridge));
}

TEST_F(QuantumTest, GetStats_Valid) {
    pink_quantum_config_t config = pink_quantum_default_config();
    bridge = pink_quantum_create(&config);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        float sample;
        pink_quantum_generate_sample(bridge, &sample);
    }

    pink_quantum_stats_t stats;
    int ret = pink_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.quantum_operations, 0u);
}

TEST_F(QuantumTest, MethodName_AllTypes) {
    EXPECT_STREQ(pink_quantum_method_name(PINK_QUANTUM_ANNEALING), "annealing");
    EXPECT_STREQ(pink_quantum_method_name(PINK_QUANTUM_TERNARY), "ternary");
    EXPECT_STREQ(pink_quantum_method_name(PINK_QUANTUM_WALK), "walk");
    EXPECT_STREQ(pink_quantum_method_name(PINK_QUANTUM_HYBRID), "hybrid");
}

//=============================================================================
// 5. Pink Noise Immune Bridge Tests
//=============================================================================

class ImmuneBridgeTest : public PinkNoiseEnhancementsTest {
protected:
    pink_immune_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            pink_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(ImmuneBridgeTest, DefaultConfig_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    EXPECT_GT(config.base_amplitude, 0.0f);
    EXPECT_EQ(config.base_alpha, 1.0f);
}

TEST_F(ImmuneBridgeTest, Create_NullConfig) {
    bridge = pink_immune_bridge_create(nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ImmuneBridgeTest, Create_ValidConfig) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ImmuneBridgeTest, Destroy_NullSafe) {
    pink_immune_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(ImmuneBridgeTest, SetCytokine_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = pink_immune_bridge_set_cytokine(bridge, PINK_CYTOKINE_IL1, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(ImmuneBridgeTest, SetInflammation_AllLevels) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(pink_immune_bridge_set_inflammation(bridge, PINK_INFLAMMATION_NONE), 0);
    EXPECT_EQ(pink_immune_bridge_set_inflammation(bridge, PINK_INFLAMMATION_LOCAL), 0);
    EXPECT_EQ(pink_immune_bridge_set_inflammation(bridge, PINK_INFLAMMATION_REGIONAL), 0);
    EXPECT_EQ(pink_immune_bridge_set_inflammation(bridge, PINK_INFLAMMATION_SYSTEMIC), 0);
    EXPECT_EQ(pink_immune_bridge_set_inflammation(bridge, PINK_INFLAMMATION_STORM), 0);
}

TEST_F(ImmuneBridgeTest, ComputeEffects_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_immune_bridge_set_cytokine(bridge, PINK_CYTOKINE_TNF, 0.8f);
    int ret = pink_immune_bridge_compute_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ImmuneBridgeTest, GetAmplitudeModifier_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    float mod = pink_immune_bridge_get_amplitude_modifier(bridge);
    EXPECT_TRUE(std::isfinite(mod));
}

TEST_F(ImmuneBridgeTest, GetAlphaModifier_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    float mod = pink_immune_bridge_get_alpha_modifier(bridge);
    EXPECT_TRUE(std::isfinite(mod));
}

TEST_F(ImmuneBridgeTest, GetEffectiveAmplitude_WithInflammation) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_immune_bridge_set_inflammation(bridge, PINK_INFLAMMATION_SYSTEMIC);
    pink_immune_bridge_compute_effects(bridge);

    float amp = pink_immune_bridge_get_effective_amplitude(bridge);
    EXPECT_GT(amp, 0.0f);
}

TEST_F(ImmuneBridgeTest, GetFeedback_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_immune_bridge_compute_feedback(bridge, 0.8f, 0.1f);

    pink_immune_feedback_t feedback;
    int ret = pink_immune_bridge_get_feedback(bridge, &feedback);
    EXPECT_EQ(ret, 0);
}

TEST_F(ImmuneBridgeTest, GetStats_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_immune_stats_t stats;
    int ret = pink_immune_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(ImmuneBridgeTest, Reset_Valid) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    bridge = pink_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = pink_immune_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// 6. Sleep/Wake Pink Noise Integration Tests
//=============================================================================

class SleepTest : public PinkNoiseEnhancementsTest {
protected:
    pink_sleep_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            pink_sleep_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(SleepTest, DefaultConfig_Valid) {
    pink_sleep_config_t config = pink_sleep_default_config();
    EXPECT_GT(config.sample_rate, 0.0f);
}

TEST_F(SleepTest, Create_NullConfig) {
    bridge = pink_sleep_create(nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(SleepTest, Create_ValidConfig) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SleepTest, Destroy_NullSafe) {
    pink_sleep_destroy(nullptr);
    SUCCEED();
}

TEST_F(SleepTest, SetStage_AllStages) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(pink_sleep_set_stage(bridge, PINK_SLEEP_WAKE), 0);
    EXPECT_EQ(pink_sleep_set_stage(bridge, PINK_SLEEP_DROWSY), 0);
    EXPECT_EQ(pink_sleep_set_stage(bridge, PINK_SLEEP_N1), 0);
    EXPECT_EQ(pink_sleep_set_stage(bridge, PINK_SLEEP_N2), 0);
    EXPECT_EQ(pink_sleep_set_stage(bridge, PINK_SLEEP_N3), 0);
    EXPECT_EQ(pink_sleep_set_stage(bridge, PINK_SLEEP_REM), 0);
}

TEST_F(SleepTest, SetArousal_Valid) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(pink_sleep_set_arousal(bridge, 0.5f), 0);
}

TEST_F(SleepTest, Step_Valid) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = pink_sleep_step(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SleepTest, GetAmplitude_Valid) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_sleep_step(bridge);
    float amp = pink_sleep_get_amplitude(bridge);
    EXPECT_GT(amp, 0.0f);
}

TEST_F(SleepTest, GetAlpha_Valid) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_sleep_step(bridge);
    float alpha = pink_sleep_get_alpha(bridge);
    EXPECT_GT(alpha, 0.0f);
}

TEST_F(SleepTest, GenerateSample_Valid) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    float sample = pink_sleep_generate_sample(bridge);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(SleepTest, StageName_AllStages) {
    EXPECT_STREQ(pink_sleep_stage_name(PINK_SLEEP_WAKE), "wake");
    EXPECT_STREQ(pink_sleep_stage_name(PINK_SLEEP_DROWSY), "drowsy");
    EXPECT_STREQ(pink_sleep_stage_name(PINK_SLEEP_N1), "N1");
    EXPECT_STREQ(pink_sleep_stage_name(PINK_SLEEP_N2), "N2");
    EXPECT_STREQ(pink_sleep_stage_name(PINK_SLEEP_N3), "N3");
    EXPECT_STREQ(pink_sleep_stage_name(PINK_SLEEP_REM), "REM");
}

TEST_F(SleepTest, N2_MayGenerateSpindles) {
    pink_sleep_config_t config = pink_sleep_default_config();
    bridge = pink_sleep_create(&config);
    ASSERT_NE(bridge, nullptr);

    pink_sleep_set_stage(bridge, PINK_SLEEP_N2);

    // Generate many samples; spindles should occur occasionally
    for (int i = 0; i < 1000; i++) {
        float sample = pink_sleep_generate_sample(bridge);
        EXPECT_TRUE(std::isfinite(sample));
    }
}

//=============================================================================
// 7. SIMD Vectorized Generation Tests
//=============================================================================

class SIMDTest : public PinkNoiseEnhancementsTest {
protected:
    pink_simd_generator_t* gen = nullptr;

    void TearDown() override {
        if (gen) {
            pink_simd_destroy(gen);
            gen = nullptr;
        }
    }
};

TEST_F(SIMDTest, Detect_ReturnsValidType) {
    pink_simd_type_t type = pink_simd_detect();
    EXPECT_GE((int)type, (int)PINK_SIMD_NONE);
    EXPECT_LE((int)type, (int)PINK_SIMD_NEON);
}

TEST_F(SIMDTest, TypeName_AllTypes) {
    EXPECT_STREQ(pink_simd_type_name(PINK_SIMD_NONE), "scalar");
    EXPECT_STREQ(pink_simd_type_name(PINK_SIMD_SSE4), "SSE4");
    EXPECT_STREQ(pink_simd_type_name(PINK_SIMD_AVX2), "AVX2");
    EXPECT_STREQ(pink_simd_type_name(PINK_SIMD_AVX512), "AVX512");
    EXPECT_STREQ(pink_simd_type_name(PINK_SIMD_NEON), "NEON");
}

TEST_F(SIMDTest, DefaultConfig_Valid) {
    pink_simd_config_t config = pink_simd_default_config();
    EXPECT_EQ(config.alpha, 1.0f);
    EXPECT_GT(config.amplitude, 0.0f);
}

TEST_F(SIMDTest, Create_NullConfig) {
    gen = pink_simd_create(nullptr);
    EXPECT_EQ(gen, nullptr);
}

TEST_F(SIMDTest, Create_ValidConfig) {
    pink_simd_config_t config = pink_simd_default_config();
    gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);
}

TEST_F(SIMDTest, Destroy_NullSafe) {
    pink_simd_destroy(nullptr);
    SUCCEED();
}

TEST_F(SIMDTest, GenerateBatch_Valid) {
    pink_simd_config_t config = pink_simd_default_config();
    gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);

    float output[256];
    int ret = pink_simd_generate_batch(gen, output, 256);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 256; i++) {
        EXPECT_TRUE(std::isfinite(output[i]));
    }
}

TEST_F(SIMDTest, GenerateAligned_Valid) {
    pink_simd_config_t config = pink_simd_default_config();
    gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);

    const float* buf = pink_simd_generate_aligned(gen, 64);
    ASSERT_NE(buf, nullptr);

    for (int i = 0; i < 64; i++) {
        EXPECT_TRUE(std::isfinite(buf[i]));
    }
}

TEST_F(SIMDTest, GenerateSample_Valid) {
    pink_simd_config_t config = pink_simd_default_config();
    gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);

    float sample = pink_simd_generate_sample(gen);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(SIMDTest, Reset_Valid) {
    pink_simd_config_t config = pink_simd_default_config();
    gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);

    int ret = pink_simd_reset(gen, 42);
    EXPECT_EQ(ret, 0);
}

TEST_F(SIMDTest, GetStats_Valid) {
    pink_simd_config_t config = pink_simd_default_config();
    gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);

    // Generate some samples
    float output[100];
    pink_simd_generate_batch(gen, output, 100);

    pink_simd_stats_t stats;
    int ret = pink_simd_get_stats(gen, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.total_samples, 0u);
}

//=============================================================================
// 8. Real-Time Spectral Monitoring Tests
//=============================================================================

class MonitorTest : public PinkNoiseEnhancementsTest {
protected:
    pink_noise_monitor_t* monitor = nullptr;

    void TearDown() override {
        if (monitor) {
            pink_monitor_destroy(monitor);
            monitor = nullptr;
        }
    }
};

TEST_F(MonitorTest, DefaultConfig_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    EXPECT_EQ(config.target_alpha, 1.0f);
    EXPECT_GT(config.tolerance, 0.0f);
}

TEST_F(MonitorTest, Create_NullConfig) {
    monitor = pink_monitor_create(nullptr);
    EXPECT_EQ(monitor, nullptr);
}

TEST_F(MonitorTest, Create_ValidConfig) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);
}

TEST_F(MonitorTest, Destroy_NullSafe) {
    pink_monitor_destroy(nullptr);
    SUCCEED();
}

TEST_F(MonitorTest, Update_WithSamples) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    for (int i = 0; i < 600; i++) {
        float sample = sinf(i * 0.05f) * 0.3f;
        int ret = pink_monitor_update(monitor, sample);
        EXPECT_GE(ret, 0);
    }
}

TEST_F(MonitorTest, GetAlpha_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    float alpha = pink_monitor_get_alpha(monitor);
    EXPECT_TRUE(std::isfinite(alpha));
}

TEST_F(MonitorTest, GetAmplitudeCorrection_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    float corr = pink_monitor_get_amplitude_correction(monitor);
    EXPECT_TRUE(std::isfinite(corr));
}

TEST_F(MonitorTest, GetAlphaCorrection_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    float corr = pink_monitor_get_alpha_correction(monitor);
    EXPECT_TRUE(std::isfinite(corr));
}

TEST_F(MonitorTest, GetQuality_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    // Fill buffer
    for (int i = 0; i < 600; i++) {
        pink_monitor_update(monitor, 0.1f * sinf(i * 0.1f));
    }

    pink_monitor_quality_t quality;
    int ret = pink_monitor_get_quality(monitor, &quality);
    EXPECT_EQ(ret, 0);
}

TEST_F(MonitorTest, Recalculate_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    // Fill buffer first
    for (int i = 0; i < 600; i++) {
        pink_monitor_update(monitor, 0.1f * sinf(i * 0.1f));
    }

    int ret = pink_monitor_recalculate(monitor);
    EXPECT_EQ(ret, 0);
}

TEST_F(MonitorTest, Reset_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    int ret = pink_monitor_reset(monitor);
    EXPECT_EQ(ret, 0);
}

static bool callback_called = false;
static void test_alert_callback(void* user_data, float measured, float target, const char* msg) {
    callback_called = true;
    (void)user_data;
    (void)measured;
    (void)target;
    (void)msg;
}

TEST_F(MonitorTest, SetCallback_Valid) {
    pink_monitor_config_t config = pink_monitor_default_config();
    monitor = pink_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);

    int ret = pink_monitor_set_callback(monitor, test_alert_callback, nullptr);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// 9. Spatial Correlation Pink Noise Tests
//=============================================================================

class SpatialTest : public PinkNoiseEnhancementsTest {
protected:
    pink_spatial_t* spatial = nullptr;

    void TearDown() override {
        if (spatial) {
            pink_spatial_destroy(spatial);
            spatial = nullptr;
        }
    }
};

TEST_F(SpatialTest, DefaultConfig_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    EXPECT_EQ(config.num_regions, 4u);
    EXPECT_GT(config.length_constant, 0.0f);
}

TEST_F(SpatialTest, NetworkConfig_Visual) {
    pink_spatial_config_t config = pink_spatial_network_config("visual");
    EXPECT_EQ(config.num_regions, 5u);
}

TEST_F(SpatialTest, NetworkConfig_Motor) {
    pink_spatial_config_t config = pink_spatial_network_config("motor");
    EXPECT_EQ(config.num_regions, 4u);
}

TEST_F(SpatialTest, NetworkConfig_DefaultMode) {
    pink_spatial_config_t config = pink_spatial_network_config("default_mode");
    EXPECT_EQ(config.num_regions, 5u);
}

TEST_F(SpatialTest, NetworkConfig_Salience) {
    pink_spatial_config_t config = pink_spatial_network_config("salience");
    EXPECT_EQ(config.num_regions, 4u);
}

TEST_F(SpatialTest, Create_NullConfig) {
    spatial = pink_spatial_create(nullptr);
    EXPECT_EQ(spatial, nullptr);
}

TEST_F(SpatialTest, Create_ValidConfig) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);
}

TEST_F(SpatialTest, Destroy_NullSafe) {
    pink_spatial_destroy(nullptr);
    SUCCEED();
}

TEST_F(SpatialTest, Step_GeneratesValues) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    int ret = pink_spatial_step(spatial);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < config.num_regions; i++) {
        float val = pink_spatial_get_region(spatial, i);
        EXPECT_TRUE(std::isfinite(val));
    }
}

TEST_F(SpatialTest, GetRegion_OutOfBounds) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    float val = pink_spatial_get_region(spatial, 100);
    EXPECT_EQ(val, 0.0f);
}

TEST_F(SpatialTest, GetNamed_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    pink_spatial_step(spatial);
    float val = pink_spatial_get_named(spatial, "V1");
    EXPECT_TRUE(std::isfinite(val));
}

TEST_F(SpatialTest, GetAll_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    pink_spatial_step(spatial);

    float values[PINK_SPATIAL_MAX_REGIONS];
    int ret = pink_spatial_get_all(spatial, values);
    EXPECT_EQ(ret, 0);
}

TEST_F(SpatialTest, GetCorrelation_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    float corr = pink_spatial_get_correlation(spatial, 0, 1);
    EXPECT_TRUE(std::isfinite(corr));
    EXPECT_GE(corr, 0.0f);
    EXPECT_LE(corr, 1.0f);
}

TEST_F(SpatialTest, GetCorrelation_Diagonal) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    float corr = pink_spatial_get_correlation(spatial, 0, 0);
    EXPECT_EQ(corr, 1.0f);
}

TEST_F(SpatialTest, GetDistance_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    float dist = pink_spatial_get_distance(spatial, 0, 1);
    EXPECT_GT(dist, 0.0f);
}

TEST_F(SpatialTest, GetDistance_Same) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    float dist = pink_spatial_get_distance(spatial, 0, 0);
    EXPECT_EQ(dist, 0.0f);
}

TEST_F(SpatialTest, AddRegion_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    int ret = pink_spatial_add_region(spatial, "NewRegion", 10.0f, 20.0f, 30.0f, 1.0f, 0.05f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SpatialTest, ComputeCorrelations_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    int ret = pink_spatial_compute_correlations(spatial);
    EXPECT_EQ(ret, 0);
}

TEST_F(SpatialTest, Reset_Valid) {
    pink_spatial_config_t config = pink_spatial_default_config();
    spatial = pink_spatial_create(&config);
    ASSERT_NE(spatial, nullptr);

    int ret = pink_spatial_reset(spatial, 42);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
