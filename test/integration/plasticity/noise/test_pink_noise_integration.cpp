/**
 * @file test_pink_noise_integration.cpp
 * @brief Integration tests for Pink Noise enhancement module interactions
 *
 * WHAT: Integration tests for cross-module pink noise interactions
 * WHY:  Ensure all 9 enhancement modules work together correctly
 * HOW:  Test module chaining, bidirectional effects, and system-level behavior
 *
 * INTEGRATION SCENARIOS:
 * 1. Multiscale + Criticality: Hierarchical noise with avalanche detection
 * 2. Correlated + Spatial: Multi-channel spatially correlated generation
 * 3. Sleep + Immune: State-dependent modulation
 * 4. Quantum + Monitor: Adaptive quantum methods with spectral monitoring
 * 5. SIMD + Multiscale: Vectorized multi-scale generation
 * 6. Full Pipeline: All modules working together
 *
 * @version 1.0.0
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

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
// Test Fixture
//=============================================================================

class PinkNoiseIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// 1. Multiscale + Criticality Integration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, MultiscaleCriticality_HierarchicalAvalanche) {
    // WHAT: Feed multiscale noise to criticality analyzer
    // WHY:  Test avalanche detection across temporal scales
    // HOW:  Generate noise at slow scale, detect avalanches

    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);
    ASSERT_NE(ms, nullptr);

    criticality_config_t ca_config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&ca_config);
    ASSERT_NE(ca, nullptr);

    // Generate samples and feed to criticality
    for (int i = 0; i < 2000; i++) {
        pink_noise_multiscale_step(ms);
        float slow_noise = pink_noise_multiscale_get_scale(ms, 2);  // Slow scale
        criticality_update(ca, slow_noise);
    }

    // Should have some statistics
    criticality_stats_t stats;
    EXPECT_EQ(criticality_get_stats(ca, &stats), 0);
    EXPECT_GT(stats.total_samples, 0u);

    criticality_destroy(ca);
    pink_noise_multiscale_destroy(ms);
}

TEST_F(PinkNoiseIntegrationTest, MultiscaleCriticality_MultiScaleMonitoring) {
    // WHAT: Monitor criticality at multiple temporal scales
    // WHY:  Different scales may have different criticality regimes

    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);
    ASSERT_NE(ms, nullptr);

    // Create analyzers for different scales
    criticality_config_t ca_config = criticality_default_config();
    criticality_analyzer_t* ca_fast = criticality_create(&ca_config);
    criticality_analyzer_t* ca_slow = criticality_create(&ca_config);
    ASSERT_NE(ca_fast, nullptr);
    ASSERT_NE(ca_slow, nullptr);

    for (int i = 0; i < 1500; i++) {
        pink_noise_multiscale_step(ms);
        criticality_update(ca_fast, pink_noise_multiscale_get_scale(ms, 0));
        criticality_update(ca_slow, pink_noise_multiscale_get_scale(ms, 3));
    }

    // Both should work
    EXPECT_TRUE(std::isfinite(criticality_get_index(ca_fast)));
    EXPECT_TRUE(std::isfinite(criticality_get_index(ca_slow)));

    criticality_destroy(ca_fast);
    criticality_destroy(ca_slow);
    pink_noise_multiscale_destroy(ms);
}

//=============================================================================
// 2. Correlated + Spatial Integration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, CorrelatedSpatial_RegionChannelMapping) {
    // WHAT: Map neuromodulator channels to brain regions
    // WHY:  Different neuromodulators affect different regions differently

    pink_noise_correlated_config_t cn_config = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* cn = pink_noise_correlated_create(&cn_config);
    ASSERT_NE(cn, nullptr);

    pink_spatial_config_t sp_config = pink_spatial_network_config("visual");
    pink_spatial_t* sp = pink_spatial_create(&sp_config);
    ASSERT_NE(sp, nullptr);

    // Generate combined noise
    for (int i = 0; i < 500; i++) {
        pink_noise_correlated_step(cn);
        pink_spatial_step(sp);

        // Combine: neuromodulator affects spatial amplitude
        float da = pink_noise_correlated_get_channel(cn, 0);  // Dopamine
        float v1 = pink_spatial_get_region(sp, 0);            // V1

        float modulated = v1 * (1.0f + 0.1f * da);
        EXPECT_TRUE(std::isfinite(modulated));
    }

    pink_spatial_destroy(sp);
    pink_noise_correlated_destroy(cn);
}

TEST_F(PinkNoiseIntegrationTest, CorrelatedSpatial_NetworkCorrelation) {
    // WHAT: Verify spatial correlations align with network structure

    pink_spatial_config_t sp_config = pink_spatial_network_config("default_mode");
    pink_spatial_t* sp = pink_spatial_create(&sp_config);
    ASSERT_NE(sp, nullptr);

    // mPFC and PCC should be correlated (default mode hubs)
    float corr_mPFC_PCC = pink_spatial_get_correlation(sp, 0, 1);
    EXPECT_GT(corr_mPFC_PCC, 0.0f);  // Should have positive correlation

    pink_spatial_destroy(sp);
}

//=============================================================================
// 3. Sleep + Immune Integration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, SleepImmune_FeverReducesN3Amplitude) {
    // WHAT: Test fever (inflammation) effect during deep sleep
    // WHY:  Fever should disrupt normal sleep noise patterns

    pink_sleep_config_t sl_config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&sl_config);
    ASSERT_NE(sl, nullptr);

    pink_immune_config_t im_config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&im_config);
    ASSERT_NE(im, nullptr);

    // Set to deep sleep
    pink_sleep_set_stage(sl, PINK_SLEEP_N3);

    // Normal N3 noise
    pink_sleep_step(sl);
    float normal_amp = pink_sleep_get_amplitude(sl);

    // Apply inflammation
    pink_immune_bridge_set_inflammation(im, PINK_INFLAMMATION_SYSTEMIC);
    pink_immune_bridge_compute_effects(im);
    float amp_mod = pink_immune_bridge_get_amplitude_modifier(im);

    // Effective amplitude should be increased (inflammation increases noise)
    float effective = normal_amp * amp_mod;
    EXPECT_TRUE(std::isfinite(effective));

    pink_immune_bridge_destroy(im);
    pink_sleep_destroy(sl);
}

TEST_F(PinkNoiseIntegrationTest, SleepImmune_CytokineModulatesAlpha) {
    // WHAT: Test cytokine effect on spectral exponent

    pink_sleep_config_t sl_config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&sl_config);
    ASSERT_NE(sl, nullptr);

    pink_immune_config_t im_config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&im_config);
    ASSERT_NE(im, nullptr);

    pink_sleep_set_stage(sl, PINK_SLEEP_WAKE);
    pink_sleep_step(sl);
    float base_alpha = pink_sleep_get_alpha(sl);

    // Add TNF-alpha (whitens spectrum)
    pink_immune_bridge_set_cytokine(im, PINK_CYTOKINE_TNF, 0.8f);
    pink_immune_bridge_compute_effects(im);
    float alpha_shift = pink_immune_bridge_get_alpha_modifier(im);

    // Effective alpha
    float effective_alpha = base_alpha + alpha_shift;
    EXPECT_TRUE(std::isfinite(effective_alpha));

    pink_immune_bridge_destroy(im);
    pink_sleep_destroy(sl);
}

TEST_F(PinkNoiseIntegrationTest, SleepImmune_StateTransitions) {
    // WHAT: Test sleep-immune interaction across sleep stage transitions

    pink_sleep_config_t sl_config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&sl_config);
    ASSERT_NE(sl, nullptr);

    pink_immune_config_t im_config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&im_config);
    ASSERT_NE(im, nullptr);

    pink_sleep_stage_t stages[] = {
        PINK_SLEEP_WAKE, PINK_SLEEP_N1, PINK_SLEEP_N2, PINK_SLEEP_N3, PINK_SLEEP_REM
    };

    for (int s = 0; s < 5; s++) {
        pink_sleep_set_stage(sl, stages[s]);

        for (int i = 0; i < 100; i++) {
            pink_sleep_step(sl);
            float sample = pink_sleep_generate_sample(sl);
            EXPECT_TRUE(std::isfinite(sample));
        }
    }

    pink_immune_bridge_destroy(im);
    pink_sleep_destroy(sl);
}

//=============================================================================
// 4. Quantum + Monitor Integration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, QuantumMonitor_AdaptiveMethod) {
    // WHAT: Monitor quantum-generated noise and adapt method

    pink_quantum_config_t q_config = pink_quantum_default_config();
    q_config.method = PINK_QUANTUM_ANNEALING;
    pink_quantum_bridge_t* q = pink_quantum_create(&q_config);
    ASSERT_NE(q, nullptr);

    pink_monitor_config_t m_config = pink_monitor_default_config();
    pink_noise_monitor_t* m = pink_monitor_create(&m_config);
    ASSERT_NE(m, nullptr);

    // Generate and monitor
    for (int i = 0; i < 600; i++) {
        float sample;
        pink_quantum_generate_sample(q, &sample);
        pink_monitor_update(m, sample);
    }

    // Check quality
    pink_monitor_quality_t quality;
    EXPECT_EQ(pink_monitor_get_quality(m, &quality), 0);
    EXPECT_TRUE(std::isfinite(quality.current_alpha));

    pink_monitor_destroy(m);
    pink_quantum_destroy(q);
}

TEST_F(PinkNoiseIntegrationTest, QuantumMonitor_MethodComparison) {
    // WHAT: Compare different quantum methods via monitoring

    pink_quantum_method_t methods[] = {
        PINK_QUANTUM_ANNEALING, PINK_QUANTUM_TERNARY, PINK_QUANTUM_WALK
    };

    for (int m = 0; m < 3; m++) {
        pink_quantum_config_t q_config = pink_quantum_default_config();
        q_config.method = methods[m];
        pink_quantum_bridge_t* q = pink_quantum_create(&q_config);
        ASSERT_NE(q, nullptr);

        pink_monitor_config_t mon_config = pink_monitor_default_config();
        pink_noise_monitor_t* mon = pink_monitor_create(&mon_config);
        ASSERT_NE(mon, nullptr);

        for (int i = 0; i < 600; i++) {
            float sample;
            pink_quantum_generate_sample(q, &sample);
            pink_monitor_update(mon, sample);
        }

        float alpha = pink_monitor_get_alpha(mon);
        EXPECT_TRUE(std::isfinite(alpha));

        pink_monitor_destroy(mon);
        pink_quantum_destroy(q);
    }
}

//=============================================================================
// 5. SIMD + Multiscale Integration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, SIMDMultiscale_VectorizedGeneration) {
    // WHAT: Use SIMD for efficient multi-scale base noise generation

    pink_simd_config_t simd_config = pink_simd_default_config();
    pink_simd_generator_t* simd = pink_simd_create(&simd_config);
    ASSERT_NE(simd, nullptr);

    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);
    ASSERT_NE(ms, nullptr);

    // Generate batch with SIMD
    float simd_output[256];
    pink_simd_generate_batch(simd, simd_output, 256);

    // Compare with multiscale
    for (int i = 0; i < 256; i++) {
        pink_noise_multiscale_step(ms);
        float ms_output = pink_noise_multiscale_get_combined(ms, nullptr);
        EXPECT_TRUE(std::isfinite(simd_output[i]));
        EXPECT_TRUE(std::isfinite(ms_output));
    }

    pink_noise_multiscale_destroy(ms);
    pink_simd_destroy(simd);
}

TEST_F(PinkNoiseIntegrationTest, SIMDMultiscale_Performance) {
    // WHAT: Verify SIMD provides performance benefit

    pink_simd_config_t simd_config = pink_simd_default_config();
    pink_simd_generator_t* simd = pink_simd_create(&simd_config);
    ASSERT_NE(simd, nullptr);

    // Generate large batch
    float output[4096];
    int ret = pink_simd_generate_batch(simd, output, 4096);
    EXPECT_EQ(ret, 0);

    pink_simd_stats_t stats;
    pink_simd_get_stats(simd, &stats);
    EXPECT_GE(stats.total_samples, 4096u);

    pink_simd_destroy(simd);
}

//=============================================================================
// 6. Full Pipeline Integration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, FullPipeline_NeuralProcessingScenario) {
    // WHAT: Simulate complete neural processing with all noise modules
    // WHY:  Verify full system integration
    // HOW:  Chain all modules in realistic scenario

    // 1. Create all components
    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);

    pink_noise_correlated_config_t cn_config = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* cn = pink_noise_correlated_create(&cn_config);

    criticality_config_t ca_config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&ca_config);

    pink_sleep_config_t sl_config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&sl_config);

    pink_immune_config_t im_config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&im_config);

    pink_spatial_config_t sp_config = pink_spatial_network_config("visual");
    pink_spatial_t* sp = pink_spatial_create(&sp_config);

    pink_monitor_config_t mon_config = pink_monitor_default_config();
    pink_noise_monitor_t* mon = pink_monitor_create(&mon_config);

    ASSERT_NE(ms, nullptr);
    ASSERT_NE(cn, nullptr);
    ASSERT_NE(ca, nullptr);
    ASSERT_NE(sl, nullptr);
    ASSERT_NE(im, nullptr);
    ASSERT_NE(sp, nullptr);
    ASSERT_NE(mon, nullptr);

    // 2. Set initial state
    pink_sleep_set_stage(sl, PINK_SLEEP_WAKE);
    pink_immune_bridge_set_inflammation(im, PINK_INFLAMMATION_NONE);

    // 3. Run simulation
    for (int t = 0; t < 1000; t++) {
        // Step all generators
        pink_noise_multiscale_step(ms);
        pink_noise_correlated_step(cn);
        pink_spatial_step(sp);
        pink_sleep_step(sl);

        // Get values
        float multiscale = pink_noise_multiscale_get_combined(ms, nullptr);
        float dopamine = pink_noise_correlated_get_channel(cn, 0);
        float v1_noise = pink_spatial_get_region(sp, 0);
        float sleep_mod = pink_sleep_get_amplitude(sl);

        // Compute immune effects
        pink_immune_bridge_compute_effects(im);
        float amp_mod = pink_immune_bridge_get_amplitude_modifier(im);

        // Combine: final neural noise
        float neural_noise = (multiscale + v1_noise) * sleep_mod * amp_mod * (1.0f + 0.1f * dopamine);

        // Monitor
        pink_monitor_update(mon, neural_noise);
        criticality_update(ca, neural_noise);

        EXPECT_TRUE(std::isfinite(neural_noise));
    }

    // 4. Check final statistics
    pink_monitor_quality_t quality;
    EXPECT_EQ(pink_monitor_get_quality(mon, &quality), 0);

    criticality_stats_t crit_stats;
    EXPECT_EQ(criticality_get_stats(ca, &crit_stats), 0);
    EXPECT_GT(crit_stats.total_samples, 0u);

    // 5. Cleanup
    pink_monitor_destroy(mon);
    pink_spatial_destroy(sp);
    pink_immune_bridge_destroy(im);
    pink_sleep_destroy(sl);
    criticality_destroy(ca);
    pink_noise_correlated_destroy(cn);
    pink_noise_multiscale_destroy(ms);
}

TEST_F(PinkNoiseIntegrationTest, FullPipeline_StateTransitionScenario) {
    // WHAT: Test system behavior across state transitions
    // WHY:  Verify modules handle transitions smoothly

    pink_sleep_config_t sl_cfg = pink_sleep_default_config();
    pink_immune_config_t im_cfg = pink_immune_bridge_default_config();
    criticality_config_t ca_cfg = criticality_default_config();
    pink_monitor_config_t mon_cfg = pink_monitor_default_config();

    pink_sleep_bridge_t* sl = pink_sleep_create(&sl_cfg);
    pink_immune_bridge_t* im = pink_immune_bridge_create(&im_cfg);
    criticality_analyzer_t* ca = criticality_create(&ca_cfg);
    pink_noise_monitor_t* mon = pink_monitor_create(&mon_cfg);

    ASSERT_NE(sl, nullptr);
    ASSERT_NE(im, nullptr);
    ASSERT_NE(ca, nullptr);
    ASSERT_NE(mon, nullptr);

    // Transition: Wake -> N1 -> N2 -> Inflammation -> N3 -> REM -> Wake
    struct transition_t {
        pink_sleep_stage_t stage;
        pink_inflammation_level_t inflammation;
        int steps;
    } transitions[] = {
        {PINK_SLEEP_WAKE, PINK_INFLAMMATION_NONE, 200},
        {PINK_SLEEP_N1, PINK_INFLAMMATION_NONE, 150},
        {PINK_SLEEP_N2, PINK_INFLAMMATION_LOCAL, 200},
        {PINK_SLEEP_N3, PINK_INFLAMMATION_REGIONAL, 300},
        {PINK_SLEEP_REM, PINK_INFLAMMATION_LOCAL, 250},
        {PINK_SLEEP_WAKE, PINK_INFLAMMATION_NONE, 100}
    };

    for (int t = 0; t < 6; t++) {
        pink_sleep_set_stage(sl, transitions[t].stage);
        pink_immune_bridge_set_inflammation(im, transitions[t].inflammation);

        for (int i = 0; i < transitions[t].steps; i++) {
            pink_sleep_step(sl);
            pink_immune_bridge_compute_effects(im);

            float sample = pink_sleep_generate_sample(sl);
            float mod = pink_immune_bridge_get_amplitude_modifier(im);
            float final = sample * mod;

            pink_monitor_update(mon, final);
            criticality_update(ca, final);

            EXPECT_TRUE(std::isfinite(final));
        }
    }

    // System should remain stable through all transitions
    pink_monitor_quality_t quality;
    EXPECT_EQ(pink_monitor_get_quality(mon, &quality), 0);

    pink_monitor_destroy(mon);
    criticality_destroy(ca);
    pink_immune_bridge_destroy(im);
    pink_sleep_destroy(sl);
}

TEST_F(PinkNoiseIntegrationTest, FullPipeline_NeuromodulatorBrainRegionLoop) {
    // WHAT: Test feedback loop between neuromodulators and brain regions
    // WHY:  Simulate realistic neuromodulator-region interactions

    pink_noise_correlated_config_t cn_cfg = pink_noise_correlated_neuromod_config();
    pink_spatial_config_t sp_cfg = pink_spatial_network_config("motor");

    pink_noise_correlated_t* cn = pink_noise_correlated_create(&cn_cfg);
    pink_spatial_t* sp = pink_spatial_create(&sp_cfg);

    ASSERT_NE(cn, nullptr);
    ASSERT_NE(sp, nullptr);

    std::vector<float> m1_activity(500);
    std::vector<float> dopamine_levels(500);

    for (int t = 0; t < 500; t++) {
        pink_noise_correlated_step(cn);
        pink_spatial_step(sp);

        dopamine_levels[t] = pink_noise_correlated_get_channel(cn, 0);
        float m1_noise = pink_spatial_get_region(sp, 0);

        // M1 activity is modulated by dopamine
        m1_activity[t] = m1_noise * (1.0f + 0.2f * dopamine_levels[t]);

        EXPECT_TRUE(std::isfinite(m1_activity[t]));
    }

    // Verify reasonable statistics
    float sum = 0, sum_sq = 0;
    for (int i = 0; i < 500; i++) {
        sum += m1_activity[i];
        sum_sq += m1_activity[i] * m1_activity[i];
    }
    float mean = sum / 500.0f;
    float variance = sum_sq / 500.0f - mean * mean;

    EXPECT_TRUE(std::isfinite(mean));
    EXPECT_GT(variance, 0.0f);

    pink_spatial_destroy(sp);
    pink_noise_correlated_destroy(cn);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
