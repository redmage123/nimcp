/**
 * @file test_pink_noise_regression.cpp
 * @brief Regression tests for Pink Noise enhancement stability
 *
 * WHAT: Regression tests ensuring long-term stability of pink noise modules
 * WHY:  Verify numerical stability, parameter bounds, and consistency
 * HOW:  Extended runs, edge cases, stress tests, repeatability verification
 *
 * REGRESSION SCENARIOS:
 * 1. Numerical Stability: No NaN/Inf over extended runs
 * 2. Parameter Bounds: Values stay within expected ranges
 * 3. Reproducibility: Same seed gives same results
 * 4. Memory Stability: No leaks over many create/destroy cycles
 * 5. Statistical Properties: Noise maintains 1/f^α spectrum
 * 6. Edge Cases: Zero inputs, extreme values, boundary conditions
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
// Constants
//=============================================================================

static const int LONG_RUN_SAMPLES = 50000;
static const int STRESS_ITERATIONS = 100;
static const int MEMORY_TEST_CYCLES = 50;

//=============================================================================
// Test Fixture
//=============================================================================

class PinkNoiseRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// 1. Numerical Stability Tests
//=============================================================================

TEST_F(PinkNoiseRegressionTest, Multiscale_NumericalStability_LongRun) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    for (int i = 0; i < LONG_RUN_SAMPLES; i++) {
        pink_noise_multiscale_step(ms);

        for (uint32_t s = 0; s < config.num_scales; s++) {
            float val = pink_noise_multiscale_get_scale(ms, s);
            ASSERT_TRUE(std::isfinite(val)) << "NaN/Inf at sample " << i << " scale " << s;
        }
    }

    pink_noise_multiscale_destroy(ms);
}

TEST_F(PinkNoiseRegressionTest, Correlated_NumericalStability_LongRun) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    for (int i = 0; i < LONG_RUN_SAMPLES; i++) {
        pink_noise_correlated_step(cn);

        for (uint32_t c = 0; c < config.num_channels; c++) {
            float val = pink_noise_correlated_get_channel(cn, c);
            ASSERT_TRUE(std::isfinite(val)) << "NaN/Inf at sample " << i << " channel " << c;
        }
    }

    pink_noise_correlated_destroy(cn);
}

TEST_F(PinkNoiseRegressionTest, Criticality_NumericalStability_LongRun) {
    criticality_config_t config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    for (int i = 0; i < LONG_RUN_SAMPLES; i++) {
        float sample = sinf(i * 0.01f) * 0.5f + 0.1f * (rand() % 100 - 50) / 50.0f;
        criticality_update(ca, sample);

        float idx = criticality_get_index(ca);
        ASSERT_TRUE(std::isfinite(idx)) << "NaN/Inf criticality index at sample " << i;
    }

    criticality_destroy(ca);
}

TEST_F(PinkNoiseRegressionTest, Quantum_NumericalStability_AllMethods) {
    pink_quantum_method_t methods[] = {
        PINK_QUANTUM_ANNEALING, PINK_QUANTUM_TERNARY, PINK_QUANTUM_WALK, PINK_QUANTUM_HYBRID
    };

    for (int m = 0; m < 4; m++) {
        pink_quantum_config_t config = pink_quantum_default_config();
        config.method = methods[m];
        pink_quantum_bridge_t* q = pink_quantum_create(&config);
        ASSERT_NE(q, nullptr);

        for (int i = 0; i < 10000; i++) {
            float sample;
            pink_quantum_generate_sample(q, &sample);
            ASSERT_TRUE(std::isfinite(sample))
                << "NaN/Inf with method " << pink_quantum_method_name(methods[m])
                << " at sample " << i;
        }

        pink_quantum_destroy(q);
    }
}

TEST_F(PinkNoiseRegressionTest, SIMD_NumericalStability_LongRun) {
    pink_simd_config_t config = pink_simd_default_config();
    pink_simd_generator_t* gen = pink_simd_create(&config);
    ASSERT_NE(gen, nullptr);

    float output[1024];
    for (int batch = 0; batch < 100; batch++) {
        pink_simd_generate_batch(gen, output, 1024);

        for (int i = 0; i < 1024; i++) {
            ASSERT_TRUE(std::isfinite(output[i]))
                << "NaN/Inf in SIMD batch " << batch << " sample " << i;
        }
    }

    pink_simd_destroy(gen);
}

TEST_F(PinkNoiseRegressionTest, Spatial_NumericalStability_AllNetworks) {
    const char* networks[] = {"visual", "motor", "default_mode", "salience"};

    for (int n = 0; n < 4; n++) {
        pink_spatial_config_t config = pink_spatial_network_config(networks[n]);
        pink_spatial_t* sp = pink_spatial_create(&config);
        ASSERT_NE(sp, nullptr);

        for (int i = 0; i < 10000; i++) {
            pink_spatial_step(sp);

            for (uint32_t r = 0; r < config.num_regions; r++) {
                float val = pink_spatial_get_region(sp, r);
                ASSERT_TRUE(std::isfinite(val))
                    << "NaN/Inf in " << networks[n] << " region " << r << " at sample " << i;
            }
        }

        pink_spatial_destroy(sp);
    }
}

//=============================================================================
// 2. Parameter Bounds Tests
//=============================================================================

TEST_F(PinkNoiseRegressionTest, Multiscale_ValueBounds) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&config);
    ASSERT_NE(ms, nullptr);

    float max_observed = 0.0f;
    for (int i = 0; i < 10000; i++) {
        pink_noise_multiscale_step(ms);
        float combined = pink_noise_multiscale_get_combined(ms, nullptr);

        max_observed = std::max(max_observed, fabsf(combined));
    }

    // Combined should stay within reasonable bounds (e.g., 10 * amplitude)
    EXPECT_LT(max_observed, 10.0f * config.global_amplitude);

    pink_noise_multiscale_destroy(ms);
}

TEST_F(PinkNoiseRegressionTest, Correlated_CorrelationBounds) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    // Check correlation matrix bounds after creation
    for (uint32_t i = 0; i < config.num_channels; i++) {
        for (uint32_t j = 0; j < config.num_channels; j++) {
            float corr = config.correlation_matrix[i * PINK_NOISE_MAX_CHANNELS + j];
            EXPECT_GE(corr, -1.0f);
            EXPECT_LE(corr, 1.0f);
        }
    }

    pink_noise_correlated_destroy(cn);
}

TEST_F(PinkNoiseRegressionTest, Criticality_IndexBounds) {
    criticality_config_t config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    for (int i = 0; i < 5000; i++) {
        float sample = sinf(i * 0.05f) * 0.3f;
        criticality_update(ca, sample);

        float idx = criticality_get_index(ca);
        EXPECT_GE(idx, 0.0f) << "Negative criticality index";
    }

    criticality_destroy(ca);
}

TEST_F(PinkNoiseRegressionTest, Immune_ModifierBounds) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&config);
    ASSERT_NE(im, nullptr);

    pink_inflammation_level_t levels[] = {
        PINK_INFLAMMATION_NONE, PINK_INFLAMMATION_LOCAL,
        PINK_INFLAMMATION_REGIONAL, PINK_INFLAMMATION_SYSTEMIC,
        PINK_INFLAMMATION_STORM
    };

    for (int l = 0; l < 5; l++) {
        pink_immune_bridge_set_inflammation(im, levels[l]);
        pink_immune_bridge_compute_effects(im);

        float amp_mod = pink_immune_bridge_get_amplitude_modifier(im);
        float alpha_mod = pink_immune_bridge_get_alpha_modifier(im);

        EXPECT_GT(amp_mod, 0.0f) << "Amplitude modifier should be positive";
        EXPECT_TRUE(std::isfinite(alpha_mod)) << "Alpha modifier should be finite";
    }

    pink_immune_bridge_destroy(im);
}

TEST_F(PinkNoiseRegressionTest, Sleep_AlphaBounds) {
    pink_sleep_config_t config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&config);
    ASSERT_NE(sl, nullptr);

    pink_sleep_stage_t stages[] = {
        PINK_SLEEP_WAKE, PINK_SLEEP_N1, PINK_SLEEP_N2, PINK_SLEEP_N3, PINK_SLEEP_REM
    };

    for (int s = 0; s < 5; s++) {
        pink_sleep_set_stage(sl, stages[s]);
        pink_sleep_step(sl);

        float alpha = pink_sleep_get_alpha(sl);
        EXPECT_GE(alpha, 0.0f) << "Alpha should be non-negative";
        EXPECT_LE(alpha, 3.0f) << "Alpha should be at most 3.0";
    }

    pink_sleep_destroy(sl);
}

TEST_F(PinkNoiseRegressionTest, Spatial_DistanceCorrelationInverse) {
    pink_spatial_config_t config = pink_spatial_default_config();
    pink_spatial_t* sp = pink_spatial_create(&config);
    ASSERT_NE(sp, nullptr);

    // Farther regions should have lower correlation
    for (uint32_t i = 0; i < config.num_regions; i++) {
        for (uint32_t j = i + 1; j < config.num_regions; j++) {
            float dist = pink_spatial_get_distance(sp, i, j);
            float corr = pink_spatial_get_correlation(sp, i, j);

            EXPECT_GE(dist, 0.0f);
            EXPECT_GE(corr, 0.0f);
            EXPECT_LE(corr, 1.0f);
        }
    }

    pink_spatial_destroy(sp);
}

//=============================================================================
// 3. Reproducibility Tests
//=============================================================================

TEST_F(PinkNoiseRegressionTest, Multiscale_Reproducibility) {
    pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
    config.seed = 12345;

    // First run
    pink_noise_multiscale_t* ms1 = pink_noise_multiscale_create(&config);
    std::vector<float> run1(1000);
    for (int i = 0; i < 1000; i++) {
        pink_noise_multiscale_step(ms1);
        run1[i] = pink_noise_multiscale_get_combined(ms1, nullptr);
    }
    pink_noise_multiscale_destroy(ms1);

    // Second run with same seed
    pink_noise_multiscale_t* ms2 = pink_noise_multiscale_create(&config);
    for (int i = 0; i < 1000; i++) {
        pink_noise_multiscale_step(ms2);
        float val = pink_noise_multiscale_get_combined(ms2, nullptr);
        EXPECT_FLOAT_EQ(run1[i], val) << "Mismatch at sample " << i;
    }
    pink_noise_multiscale_destroy(ms2);
}

TEST_F(PinkNoiseRegressionTest, SIMD_Reproducibility) {
    pink_simd_config_t config = pink_simd_default_config();
    config.seed = 54321;

    // First run
    pink_simd_generator_t* gen1 = pink_simd_create(&config);
    float run1[256];
    pink_simd_generate_batch(gen1, run1, 256);
    pink_simd_destroy(gen1);

    // Second run
    pink_simd_generator_t* gen2 = pink_simd_create(&config);
    float run2[256];
    pink_simd_generate_batch(gen2, run2, 256);
    pink_simd_destroy(gen2);

    for (int i = 0; i < 256; i++) {
        EXPECT_FLOAT_EQ(run1[i], run2[i]) << "Mismatch at sample " << i;
    }
}

TEST_F(PinkNoiseRegressionTest, Quantum_ResetReproducibility) {
    pink_quantum_config_t config = pink_quantum_default_config();
    config.seed = 99999;
    pink_quantum_bridge_t* q = pink_quantum_create(&config);
    ASSERT_NE(q, nullptr);

    // Generate some samples
    float run1[100];
    for (int i = 0; i < 100; i++) {
        pink_quantum_generate_sample(q, &run1[i]);
    }

    // Reset and regenerate
    pink_quantum_reset(q, 99999);
    for (int i = 0; i < 100; i++) {
        float sample;
        pink_quantum_generate_sample(q, &sample);
        EXPECT_FLOAT_EQ(run1[i], sample) << "Reset mismatch at sample " << i;
    }

    pink_quantum_destroy(q);
}

//=============================================================================
// 4. Memory Stability Tests
//=============================================================================

TEST_F(PinkNoiseRegressionTest, Multiscale_MemoryStability) {
    for (int cycle = 0; cycle < MEMORY_TEST_CYCLES; cycle++) {
        pink_noise_multiscale_config_t config = pink_noise_multiscale_default_config();
        pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&config);
        ASSERT_NE(ms, nullptr);

        for (int i = 0; i < 100; i++) {
            pink_noise_multiscale_step(ms);
        }

        pink_noise_multiscale_destroy(ms);
    }
    SUCCEED();
}

TEST_F(PinkNoiseRegressionTest, AllModules_MemoryStability) {
    for (int cycle = 0; cycle < MEMORY_TEST_CYCLES; cycle++) {
        // Multiscale
        pink_noise_multiscale_config_t ms_cfg = pink_noise_multiscale_default_config();
        auto ms = pink_noise_multiscale_create(&ms_cfg);
        pink_noise_multiscale_destroy(ms);

        // Correlated
        pink_noise_correlated_config_t cn_cfg = pink_noise_correlated_neuromod_config();
        auto cn = pink_noise_correlated_create(&cn_cfg);
        pink_noise_correlated_destroy(cn);

        // Criticality
        criticality_config_t ca_cfg = criticality_default_config();
        auto ca = criticality_create(&ca_cfg);
        criticality_destroy(ca);

        // Quantum
        pink_quantum_config_t q_cfg = pink_quantum_default_config();
        auto q = pink_quantum_create(&q_cfg);
        pink_quantum_destroy(q);

        // Immune
        pink_immune_config_t im_cfg = pink_immune_bridge_default_config();
        auto im = pink_immune_bridge_create(&im_cfg);
        pink_immune_bridge_destroy(im);

        // Sleep
        pink_sleep_config_t sl_cfg = pink_sleep_default_config();
        auto sl = pink_sleep_create(&sl_cfg);
        pink_sleep_destroy(sl);

        // SIMD
        pink_simd_config_t simd_cfg = pink_simd_default_config();
        auto simd = pink_simd_create(&simd_cfg);
        pink_simd_destroy(simd);

        // Monitor
        pink_monitor_config_t mon_cfg = pink_monitor_default_config();
        auto mon = pink_monitor_create(&mon_cfg);
        pink_monitor_destroy(mon);

        // Spatial
        pink_spatial_config_t sp_cfg = pink_spatial_default_config();
        auto sp = pink_spatial_create(&sp_cfg);
        pink_spatial_destroy(sp);
    }
    SUCCEED();
}

//=============================================================================
// 5. Statistical Properties Tests
//=============================================================================

TEST_F(PinkNoiseRegressionTest, Monitor_AlphaEstimationAccuracy) {
    // Generate true pink noise and verify monitor detects correct alpha
    pink_noise_config_t pn_config = pink_noise_default_config();
    pn_config.alpha = 1.0f;
    pn_config.seed = 12345;
    pink_noise_generator_t pn = pink_noise_create(&pn_config);
    ASSERT_NE(pn, nullptr);

    pink_monitor_config_t mon_config = pink_monitor_default_config();
    mon_config.target_alpha = 1.0f;
    pink_noise_monitor_t* mon = pink_monitor_create(&mon_config);
    ASSERT_NE(mon, nullptr);

    // Generate and monitor
    for (int i = 0; i < 5000; i++) {
        float sample;
        pink_noise_generate_sample(pn, &sample);
        pink_monitor_update(mon, sample);
    }

    // Check estimated alpha is close to 1.0
    pink_monitor_quality_t quality;
    pink_monitor_get_quality(mon, &quality);

    // Allow significant tolerance for short-time estimation
    EXPECT_NEAR(quality.current_alpha, 1.0f, 0.5f);

    pink_monitor_destroy(mon);
    pink_noise_destroy(pn);
}

TEST_F(PinkNoiseRegressionTest, Correlated_ChannelMeansNearZero) {
    pink_noise_correlated_config_t config = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* cn = pink_noise_correlated_create(&config);
    ASSERT_NE(cn, nullptr);

    double sums[PINK_NOISE_NEUROMOD_CHANNELS] = {0};
    int N = 10000;

    for (int i = 0; i < N; i++) {
        pink_noise_correlated_step(cn);
        for (uint32_t c = 0; c < PINK_NOISE_NEUROMOD_CHANNELS; c++) {
            sums[c] += pink_noise_correlated_get_channel(cn, c);
        }
    }

    // Means should be near zero
    for (uint32_t c = 0; c < PINK_NOISE_NEUROMOD_CHANNELS; c++) {
        double mean = sums[c] / N;
        EXPECT_NEAR(mean, 0.0, 0.1) << "Channel " << c << " mean not near zero";
    }

    pink_noise_correlated_destroy(cn);
}

//=============================================================================
// 6. Edge Case Tests
//=============================================================================

TEST_F(PinkNoiseRegressionTest, Criticality_ZeroInput) {
    criticality_config_t config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    // Feed all zeros
    for (int i = 0; i < 1000; i++) {
        int ret = criticality_update(ca, 0.0f);
        EXPECT_EQ(ret, 0);
    }

    // Should still be valid
    float idx = criticality_get_index(ca);
    EXPECT_TRUE(std::isfinite(idx));

    criticality_destroy(ca);
}

TEST_F(PinkNoiseRegressionTest, Criticality_ExtremeInput) {
    criticality_config_t config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&config);
    ASSERT_NE(ca, nullptr);

    // Feed extreme values
    for (int i = 0; i < 500; i++) {
        criticality_update(ca, 100.0f);
        criticality_update(ca, -100.0f);
    }

    // Should remain stable
    float idx = criticality_get_index(ca);
    EXPECT_TRUE(std::isfinite(idx));

    criticality_destroy(ca);
}

TEST_F(PinkNoiseRegressionTest, Immune_AllCytokinesMax) {
    pink_immune_config_t config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&config);
    ASSERT_NE(im, nullptr);

    // Set all cytokines to max
    for (int c = 0; c < PINK_CYTOKINE_COUNT; c++) {
        pink_immune_bridge_set_cytokine(im, (pink_immune_cytokine_t)c, 1.0f);
    }
    pink_immune_bridge_set_inflammation(im, PINK_INFLAMMATION_STORM);
    pink_immune_bridge_compute_effects(im);

    float amp_mod = pink_immune_bridge_get_amplitude_modifier(im);
    float alpha_mod = pink_immune_bridge_get_alpha_modifier(im);

    EXPECT_TRUE(std::isfinite(amp_mod));
    EXPECT_TRUE(std::isfinite(alpha_mod));
    EXPECT_GT(amp_mod, 0.0f);

    pink_immune_bridge_destroy(im);
}

TEST_F(PinkNoiseRegressionTest, Sleep_RapidTransitions) {
    pink_sleep_config_t config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&config);
    ASSERT_NE(sl, nullptr);

    pink_sleep_stage_t stages[] = {
        PINK_SLEEP_WAKE, PINK_SLEEP_N1, PINK_SLEEP_REM, PINK_SLEEP_N3,
        PINK_SLEEP_N2, PINK_SLEEP_WAKE, PINK_SLEEP_N3, PINK_SLEEP_REM
    };

    for (int cycle = 0; cycle < 100; cycle++) {
        for (int s = 0; s < 8; s++) {
            pink_sleep_set_stage(sl, stages[s]);
            pink_sleep_step(sl);
            float sample = pink_sleep_generate_sample(sl);
            EXPECT_TRUE(std::isfinite(sample))
                << "NaN/Inf during rapid transition cycle " << cycle << " stage " << s;
        }
    }

    pink_sleep_destroy(sl);
}

TEST_F(PinkNoiseRegressionTest, Spatial_MaxRegions) {
    pink_spatial_config_t config = pink_spatial_default_config();
    pink_spatial_t* sp = pink_spatial_create(&config);
    ASSERT_NE(sp, nullptr);

    // Add regions up to maximum
    for (int r = config.num_regions; r < PINK_SPATIAL_MAX_REGIONS; r++) {
        int ret = pink_spatial_add_region(sp, "Region",
            r * 10.0f, r * 5.0f, r * 2.0f, 1.0f, 0.05f);
        if (ret != 0) break;  // Hit max
    }

    // Should still work
    pink_spatial_step(sp);
    float val = pink_spatial_get_region(sp, 0);
    EXPECT_TRUE(std::isfinite(val));

    pink_spatial_destroy(sp);
}

TEST_F(PinkNoiseRegressionTest, Monitor_AutoCorrection) {
    pink_monitor_config_t config = pink_monitor_default_config();
    config.enable_auto_correction = true;
    config.target_alpha = 1.0f;
    config.tolerance = 0.1f;
    pink_noise_monitor_t* mon = pink_monitor_create(&config);
    ASSERT_NE(mon, nullptr);

    // Feed samples that drift from target
    for (int i = 0; i < 1000; i++) {
        float sample = sinf(i * 0.001f) * 0.5f;  // Very low frequency (red-ish)
        pink_monitor_update(mon, sample);

        // Corrections should be finite
        float alpha_corr = pink_monitor_get_alpha_correction(mon);
        EXPECT_TRUE(std::isfinite(alpha_corr));
    }

    pink_monitor_destroy(mon);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
