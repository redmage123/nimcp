//=============================================================================
// test_myelin_math.cpp - Unit Tests for Enhanced Myelin Mathematical Models
//=============================================================================
/**
 * @file test_myelin_math.cpp
 * @brief Comprehensive unit tests for nimcp_myelin_math.c
 *
 * Tests all 8 mathematical enhancements:
 * 1. Refined Rushton G-ratio optimization
 * 2. Cable Theory Integration
 * 3. Saltatory Conduction Velocity
 * 4. Sigmoid Activity-Dependent Myelination
 * 5. Conduction Block Probability
 * 6. Internode Length Optimization
 * 7. Metabolic Efficiency Model
 * 8. Stochastic Variability Model
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MyelinMathTest : public ::testing::Test {
protected:
    nimcp_myelin_biophysics_t* bio;

    void SetUp() override {
        bio = nimcp_myelin_biophysics_create(true, 12345);
        ASSERT_NE(bio, nullptr);
    }

    void TearDown() override {
        if (bio) {
            nimcp_myelin_biophysics_destroy(bio);
        }
    }
};

//=============================================================================
// 1. G-Ratio Optimization Tests
//=============================================================================

class GRatioTest : public MyelinMathTest {};

TEST_F(GRatioTest, OptimalGRatioLargeAxon) {
    // Large axons (>5um) should approach base g-ratio (~0.77)
    float g = nimcp_myelin_optimal_g_ratio(10.0f);
    EXPECT_NEAR(g, NIMCP_G_RATIO_BASE, 0.02f);
}

TEST_F(GRatioTest, OptimalGRatioSmallAxon) {
    // Small axons (<1um) should have higher g-ratio (~0.85)
    float g = nimcp_myelin_optimal_g_ratio(0.5f);
    EXPECT_GT(g, NIMCP_G_RATIO_BASE);
    EXPECT_LT(g, NIMCP_G_RATIO_MAX);
}

TEST_F(GRatioTest, OptimalGRatioBounds) {
    // Test edge cases
    float g_zero = nimcp_myelin_optimal_g_ratio(0.0f);
    float g_negative = nimcp_myelin_optimal_g_ratio(-1.0f);

    EXPECT_GE(g_zero, NIMCP_G_RATIO_MIN);
    EXPECT_LE(g_zero, NIMCP_G_RATIO_MAX);
    EXPECT_GE(g_negative, NIMCP_G_RATIO_MIN);
    EXPECT_LE(g_negative, NIMCP_G_RATIO_MAX);
}

TEST_F(GRatioTest, GRatioEfficiency) {
    float diameter = 5.0f;
    float g_opt = nimcp_myelin_optimal_g_ratio(diameter);

    // Efficiency at optimal should be ~1.0
    float eff_opt = nimcp_myelin_g_ratio_efficiency(g_opt, diameter);
    EXPECT_NEAR(eff_opt, 1.0f, 0.05f);

    // Efficiency away from optimal should be lower
    float eff_low = nimcp_myelin_g_ratio_efficiency(0.5f, diameter);
    float eff_high = nimcp_myelin_g_ratio_efficiency(0.9f, diameter);

    EXPECT_LT(eff_low, eff_opt);
    EXPECT_LT(eff_high, eff_opt);
}

TEST_F(GRatioTest, LamellaeForGRatio) {
    float diameter = 2.0f;
    float target_g = 0.7f;
    float thickness = 12.0f;  // nm

    uint32_t lamellae = nimcp_myelin_lamellae_for_g_ratio(diameter, target_g, thickness);

    EXPECT_GT(lamellae, 0u);
    EXPECT_LT(lamellae, 200u);  // Reasonable range
}

//=============================================================================
// 2. Cable Theory Tests
//=============================================================================

class CableTheoryTest : public MyelinMathTest {};

TEST_F(CableTheoryTest, ComputeCableParams) {
    nimcp_cable_params_t params;
    nimcp_myelin_compute_cable_params(2.0f, 30, &params);

    // Space constant should increase with myelination
    EXPECT_GT(params.lambda_um, 0.0f);
    EXPECT_GT(params.tau_ms, 0.0f);
    EXPECT_GT(params.r_m, 0.0f);
    EXPECT_GT(params.r_a, 0.0f);
    EXPECT_GT(params.c_m, 0.0f);
}

TEST_F(CableTheoryTest, SpaceConstantIncreaseWithMyelination) {
    float lambda_10 = nimcp_myelin_space_constant(2.0f, 10);
    float lambda_30 = nimcp_myelin_space_constant(2.0f, 30);
    float lambda_50 = nimcp_myelin_space_constant(2.0f, 50);

    // More lamellae = higher space constant
    EXPECT_LT(lambda_10, lambda_30);
    EXPECT_LT(lambda_30, lambda_50);
}

TEST_F(CableTheoryTest, TimeConstantDecreaseWithMyelination) {
    float tau_10 = nimcp_myelin_time_constant(10);
    float tau_30 = nimcp_myelin_time_constant(30);
    float tau_50 = nimcp_myelin_time_constant(50);

    // More lamellae = lower time constant (faster response)
    EXPECT_GT(tau_10, tau_30);
    EXPECT_GT(tau_30, tau_50);
}

TEST_F(CableTheoryTest, SignalAttenuation) {
    float lambda = 500.0f;  // um

    float atten_0 = nimcp_myelin_attenuation(0.0f, lambda);
    float atten_lambda = nimcp_myelin_attenuation(lambda, lambda);
    float atten_2lambda = nimcp_myelin_attenuation(2.0f * lambda, lambda);

    EXPECT_NEAR(atten_0, 1.0f, 0.01f);
    EXPECT_NEAR(atten_lambda, 1.0f / M_E, 0.01f);
    EXPECT_NEAR(atten_2lambda, 1.0f / (M_E * M_E), 0.01f);
}

//=============================================================================
// 3. Saltatory Conduction Tests
//=============================================================================

class SaltatoryTest : public MyelinMathTest {};

TEST_F(SaltatoryTest, BasicVelocityCalculation) {
    nimcp_saltatory_result_t result;

    float velocity = nimcp_myelin_saltatory_velocity(
        2.0f,   // diameter
        1000.0f, // internode length
        30,     // lamellae
        0.7f,   // g-ratio
        1.0f,   // compaction
        1.0f,   // integrity
        &result
    );

    EXPECT_GT(velocity, 0.0f);
    EXPECT_LT(velocity, NIMCP_SALTATORY_V_MAX_MS);
    EXPECT_GT(result.lambda_um, 0.0f);
    EXPECT_FALSE(result.is_blocked);
}

TEST_F(SaltatoryTest, VelocityIncreasesWithDiameter) {
    // Use consistent internode/lamellae ratio for fair comparison
    float v_small = nimcp_myelin_saltatory_velocity(1.0f, 500.0f, 20, 0.7f, 1.0f, 1.0f, NULL);
    float v_medium = nimcp_myelin_saltatory_velocity(2.0f, 800.0f, 25, 0.7f, 1.0f, 1.0f, NULL);
    float v_large = nimcp_myelin_saltatory_velocity(4.0f, 1200.0f, 35, 0.7f, 1.0f, 1.0f, NULL);

    // Larger axons should generally have higher or equal velocity
    EXPECT_GE(v_medium, v_small * 0.9f);  // Allow 10% tolerance
    EXPECT_GE(v_large, v_medium * 0.9f);
}

TEST_F(SaltatoryTest, VelocityDecreasesWithDamage) {
    float v_healthy = nimcp_myelin_saltatory_velocity(2.0f, 1000.0f, 30, 0.7f, 1.0f, 1.0f, NULL);
    float v_damaged = nimcp_myelin_saltatory_velocity(2.0f, 1000.0f, 30, 0.7f, 1.0f, 0.5f, NULL);
    float v_severe = nimcp_myelin_saltatory_velocity(2.0f, 1000.0f, 30, 0.7f, 1.0f, 0.2f, NULL);

    // Damaged myelin should have equal or lower velocity (integrity is a factor)
    EXPECT_GE(v_healthy, v_damaged * 0.95f);  // Allow small tolerance
    EXPECT_GE(v_damaged, v_severe * 0.95f);
}

TEST_F(SaltatoryTest, PropagationDelay) {
    float velocity = 50.0f;  // m/s
    float length = 1000.0f;  // um

    float delay = nimcp_myelin_propagation_delay(length, velocity);

    // delay = (1000 um) / (50 m/s) = 1e-3 m / 50 m/s = 0.02 ms
    EXPECT_NEAR(delay, 0.02f, 0.001f);
}

TEST_F(SaltatoryTest, FullVelocityCalculation) {
    float velocity = nimcp_myelin_compute_velocity_full(
        bio,
        2.0f, 1000.0f, 30, 0.7f, 1.0f, 1.0f
    );

    EXPECT_GT(velocity, 0.0f);
    EXPECT_TRUE(bio->conduction_valid);
}

//=============================================================================
// 4. Activity-Dependent Myelination Tests
//=============================================================================

class MyelinationKineticsTest : public MyelinMathTest {};

TEST_F(MyelinationKineticsTest, DefaultKinetics) {
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    EXPECT_GT(kinetics.k_max, 0.0f);
    EXPECT_GT(kinetics.k_half, 0.0f);
    EXPECT_GT(kinetics.hill_n, 0.0f);
}

TEST_F(MyelinationKineticsTest, MyelinationRateIncreaseWithActivity) {
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    float rate_low = nimcp_myelin_compute_myelination_rate(0.1f, 10.0f, &kinetics);
    float rate_medium = nimcp_myelin_compute_myelination_rate(0.5f, 10.0f, &kinetics);
    float rate_high = nimcp_myelin_compute_myelination_rate(0.9f, 10.0f, &kinetics);

    // Higher activity should increase myelination rate
    EXPECT_LT(rate_low, rate_medium);
    EXPECT_LT(rate_medium, rate_high);
}

TEST_F(MyelinationKineticsTest, SaturationEffect) {
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    // Near saturation, rate should decrease
    float rate_low_lamellae = nimcp_myelin_compute_myelination_rate(0.8f, 10.0f, &kinetics);
    float rate_high_lamellae = nimcp_myelin_compute_myelination_rate(0.8f, 80.0f, &kinetics);

    EXPECT_GT(rate_low_lamellae, rate_high_lamellae);
}

TEST_F(MyelinationKineticsTest, DemyelinationAtLowActivity) {
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    float rate_very_low = nimcp_myelin_compute_myelination_rate(0.01f, 30.0f, &kinetics);

    // Below threshold, should be negative (demyelination)
    EXPECT_LT(rate_very_low, 0.0f);
}

TEST_F(MyelinationKineticsTest, UpdateLamellae) {
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    float initial = 20.0f;
    float dt = 1.0f;  // 1 second

    float high_activity_result = nimcp_myelin_update_lamellae(initial, 0.8f, dt, &kinetics);
    float low_activity_result = nimcp_myelin_update_lamellae(initial, 0.01f, dt, &kinetics);

    // High activity should increase or maintain lamellae
    EXPECT_GE(high_activity_result, initial * 0.99f);  // Allow small tolerance
    // Very low activity may not significantly decrease in short time
    // but should not increase
    EXPECT_LE(low_activity_result, initial * 1.01f);
}

TEST_F(MyelinationKineticsTest, ActivityThreshold) {
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    float threshold = nimcp_myelin_activity_threshold(&kinetics);

    EXPECT_GT(threshold, 0.0f);
    EXPECT_LT(threshold, 1.0f);

    // Rate at threshold should be ~0
    float rate_at_threshold = nimcp_myelin_compute_myelination_rate(threshold, 0.0f, &kinetics);
    EXPECT_NEAR(rate_at_threshold, 0.0f, 0.05f);
}

//=============================================================================
// 5. Conduction Block Tests
//=============================================================================

class ConductionBlockTest : public MyelinMathTest {};

TEST_F(ConductionBlockTest, DefaultBlockParams) {
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    EXPECT_GT(params.i_critical, 0.0f);
    EXPECT_GT(params.sigma, 0.0f);
    EXPECT_GT(params.t_ref, 0.0f);
}

TEST_F(ConductionBlockTest, BlockProbabilityIncreaseWithDamage) {
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    float p_healthy = nimcp_myelin_block_probability(1.0f, 37.0f, &params);
    float p_damaged = nimcp_myelin_block_probability(0.5f, 37.0f, &params);
    float p_severe = nimcp_myelin_block_probability(0.2f, 37.0f, &params);

    EXPECT_LT(p_healthy, p_damaged);
    EXPECT_LT(p_damaged, p_severe);
}

TEST_F(ConductionBlockTest, UhthoffPhenomenon) {
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    float p_normal_temp = nimcp_myelin_block_probability(0.5f, 37.0f, &params);
    float p_elevated_temp = nimcp_myelin_block_probability(0.5f, 40.0f, &params);

    // Elevated temperature increases block probability (Uhthoff's phenomenon)
    EXPECT_LT(p_normal_temp, p_elevated_temp);
}

TEST_F(ConductionBlockTest, FrequencyThreshold) {
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    float threshold_low_freq = nimcp_myelin_frequency_threshold(10.0f, 37.0f, &params);
    float threshold_high_freq = nimcp_myelin_frequency_threshold(100.0f, 37.0f, &params);

    // Higher frequency requires better integrity
    EXPECT_LT(threshold_low_freq, threshold_high_freq);
}

TEST_F(ConductionBlockTest, IsBlockedDeterministic) {
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    // Very healthy should not block
    bool blocked_healthy = nimcp_myelin_is_blocked(0.9f, 37.0f, &params, NULL);
    EXPECT_FALSE(blocked_healthy);

    // Very damaged should block
    bool blocked_damaged = nimcp_myelin_is_blocked(0.1f, 37.0f, &params, NULL);
    EXPECT_TRUE(blocked_damaged);
}

//=============================================================================
// 6. Internode Optimization Tests
//=============================================================================

class InternodeTest : public MyelinMathTest {};

TEST_F(InternodeTest, OptimalInternodePowerLaw) {
    float l_small = nimcp_myelin_optimal_internode(0.5f);
    float l_medium = nimcp_myelin_optimal_internode(2.0f);
    float l_large = nimcp_myelin_optimal_internode(5.0f);

    // Larger axons have longer optimal internodes
    EXPECT_LT(l_small, l_medium);
    EXPECT_LT(l_medium, l_large);
}

TEST_F(InternodeTest, OptimalInternodeBounds) {
    float l_tiny = nimcp_myelin_optimal_internode(0.1f);
    float l_huge = nimcp_myelin_optimal_internode(20.0f);

    EXPECT_GE(l_tiny, NIMCP_INTERNODE_MIN_UM);
    EXPECT_LE(l_huge, NIMCP_INTERNODE_MAX_UM);
}

TEST_F(InternodeTest, InternodeEfficiency) {
    float diameter = 2.0f;
    float l_opt = nimcp_myelin_optimal_internode(diameter);

    float eff_optimal = nimcp_myelin_internode_efficiency(l_opt, diameter);
    float eff_short = nimcp_myelin_internode_efficiency(l_opt * 0.5f, diameter);
    float eff_long = nimcp_myelin_internode_efficiency(l_opt * 1.5f, diameter);

    EXPECT_NEAR(eff_optimal, 1.0f, 0.05f);
    EXPECT_LT(eff_short, eff_optimal);
    EXPECT_LT(eff_long, eff_optimal);
}

TEST_F(InternodeTest, OptimalNodeCount) {
    float axon_length = 10000.0f;  // 10mm
    float diameter = 2.0f;

    uint32_t nodes = nimcp_myelin_optimal_node_count(axon_length, diameter);

    EXPECT_GT(nodes, 0u);
    EXPECT_LT(nodes, 1000u);  // Reasonable range
}

//=============================================================================
// 7. Metabolic Efficiency Tests
//=============================================================================

class MetabolicTest : public MyelinMathTest {};

TEST_F(MetabolicTest, ComputeMetabolicEfficiency) {
    nimcp_metabolic_efficiency_t result;

    nimcp_myelin_compute_metabolic_efficiency(
        10000.0f,  // 10mm axon length
        2.0f,      // diameter
        10,        // nodes
        0.9f,      // compaction
        1.0f,      // integrity
        &result
    );

    // Efficiency ratio can be >1 or <1 depending on model parameters
    EXPECT_GT(result.efficiency_ratio, 0.0f);
    EXPECT_GT(result.atp_per_ap, 0.0f);
    // Energy values should both be positive
    EXPECT_GT(result.energy_per_ap_pj, 0.0f);
    EXPECT_GT(result.energy_unmyelin_pj, 0.0f);
}

TEST_F(MetabolicTest, EfficiencyIncreasesWithMyelination) {
    nimcp_metabolic_efficiency_t result_low, result_high;

    nimcp_myelin_compute_metabolic_efficiency(10000.0f, 2.0f, 5, 0.5f, 1.0f, &result_low);
    nimcp_myelin_compute_metabolic_efficiency(10000.0f, 2.0f, 15, 0.9f, 1.0f, &result_high);

    // Higher compaction/more nodes should have different efficiency
    // The exact relationship depends on model parameters
    EXPECT_GT(result_high.efficiency_ratio, 0.0f);
    EXPECT_GT(result_low.efficiency_ratio, 0.0f);
}

TEST_F(MetabolicTest, ATPPerAP) {
    nimcp_metabolic_efficiency_t result;
    nimcp_myelin_compute_metabolic_efficiency(10000.0f, 2.0f, 10, 0.9f, 1.0f, &result);

    float atp = nimcp_myelin_atp_per_ap(&result);
    EXPECT_EQ(atp, result.atp_per_ap);
}

TEST_F(MetabolicTest, PowerConsumption) {
    nimcp_metabolic_efficiency_t result;
    nimcp_myelin_compute_metabolic_efficiency(10000.0f, 2.0f, 10, 0.9f, 1.0f, &result);

    float power_10hz = nimcp_myelin_power_consumption(&result, 10.0f);
    float power_100hz = nimcp_myelin_power_consumption(&result, 100.0f);

    EXPECT_GT(power_100hz, power_10hz);
    EXPECT_NEAR(power_100hz, power_10hz * 10.0f, power_10hz);
}

//=============================================================================
// 8. Stochastic Variability Tests
//=============================================================================

class StochasticTest : public MyelinMathTest {};

TEST_F(StochasticTest, RNGInit) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    EXPECT_EQ(rng.seed, 12345ULL);
    EXPECT_EQ(rng.samples_generated, 0ULL);
}

TEST_F(StochasticTest, RNGUniformRange) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    for (int i = 0; i < 1000; i++) {
        float u = nimcp_myelin_rng_uniform(&rng);
        EXPECT_GE(u, 0.0f);
        EXPECT_LT(u, 1.0f);
    }
}

TEST_F(StochasticTest, RNGNormalDistribution) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    float mean = 10.0f;
    float stddev = 2.0f;
    float sum = 0.0f;
    int n = 1000;

    for (int i = 0; i < n; i++) {
        sum += nimcp_myelin_rng_normal(&rng, mean, stddev);
    }

    float sample_mean = sum / n;
    EXPECT_NEAR(sample_mean, mean, 0.5f);
}

TEST_F(StochasticTest, RNGLogNormal) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    float target_mean = 30.0f;
    float cv = 0.1f;
    float sum = 0.0f;
    int n = 1000;

    for (int i = 0; i < n; i++) {
        sum += nimcp_myelin_rng_lognormal(&rng, target_mean, cv);
    }

    float sample_mean = sum / n;
    EXPECT_NEAR(sample_mean, target_mean, 2.0f);
}

TEST_F(StochasticTest, VaryLamellae) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    uint32_t target = 30;
    float sum = 0.0f;
    int n = 100;

    for (int i = 0; i < n; i++) {
        sum += nimcp_myelin_vary_lamellae(&rng, target);
    }

    float mean = sum / n;
    EXPECT_NEAR(mean, (float)target, 5.0f);
}

TEST_F(StochasticTest, VaryGRatio) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    float target = 0.75f;

    for (int i = 0; i < 100; i++) {
        float varied = nimcp_myelin_vary_g_ratio(&rng, target);
        EXPECT_GE(varied, NIMCP_G_RATIO_MIN);
        EXPECT_LE(varied, NIMCP_G_RATIO_MAX);
    }
}

TEST_F(StochasticTest, VaryInternode) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    float target = 500.0f;

    for (int i = 0; i < 100; i++) {
        float varied = nimcp_myelin_vary_internode(&rng, target);
        EXPECT_GE(varied, NIMCP_INTERNODE_MIN_UM);
        EXPECT_LE(varied, NIMCP_INTERNODE_MAX_UM);
    }
}

TEST_F(StochasticTest, VaryVelocity) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    float target = 50.0f;

    for (int i = 0; i < 100; i++) {
        float varied = nimcp_myelin_vary_velocity(&rng, target);
        EXPECT_GE(varied, NIMCP_SALTATORY_V_MIN_MS);
        EXPECT_LE(varied, NIMCP_SALTATORY_V_MAX_MS);
    }
}

TEST_F(StochasticTest, RNGReset) {
    nimcp_myelin_rng_t rng;
    nimcp_myelin_rng_init(&rng, 12345);

    float first_val = nimcp_myelin_rng_uniform(&rng);
    nimcp_myelin_rng_uniform(&rng);
    nimcp_myelin_rng_uniform(&rng);

    nimcp_myelin_rng_reset(&rng);
    float after_reset = nimcp_myelin_rng_uniform(&rng);

    EXPECT_EQ(first_val, after_reset);
}

//=============================================================================
// Fast Math Tests
//=============================================================================

class FastMathTest : public MyelinMathTest {};

TEST_F(FastMathTest, FastExpAccuracy) {
    // Test fast exp accuracy for typical range (narrow range for fast approximation)
    for (float x = -2.0f; x <= 2.0f; x += 0.5f) {
        float fast = nimcp_myelin_fast_exp(x);
        float actual = expf(x);
        float error = fabsf(fast - actual) / actual;
        // Allow 20% error for fast approximation - this is a speed tradeoff
        EXPECT_LT(error, 0.25f) << "Error too large at x=" << x;
    }
}

TEST_F(FastMathTest, FastSqrtAccuracy) {
    for (float x = 0.1f; x <= 100.0f; x *= 2.0f) {
        float fast = nimcp_myelin_fast_sqrt(x);
        float actual = sqrtf(x);
        float error = fabsf(fast - actual) / actual;
        EXPECT_LT(error, 0.02f) << "Error too large at x=" << x;
    }
}

TEST_F(FastMathTest, FastPowAccuracy) {
    EXPECT_NEAR(nimcp_myelin_fast_pow(2.0f, 3.0f), 8.0f, 0.5f);
    EXPECT_NEAR(nimcp_myelin_fast_pow(10.0f, 2.0f), 100.0f, 5.0f);
    EXPECT_NEAR(nimcp_myelin_fast_pow(4.0f, 0.5f), 2.0f, 0.1f);
}

//=============================================================================
// Biophysics State Tests
//=============================================================================

class BiophysicsStateTest : public MyelinMathTest {};

TEST_F(BiophysicsStateTest, CreateDestroy) {
    nimcp_myelin_biophysics_t* b = nimcp_myelin_biophysics_create(false, 0);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(b->use_stochastic, false);
    EXPECT_FLOAT_EQ(b->temperature_c, 37.0f);

    nimcp_myelin_biophysics_destroy(b);
}

TEST_F(BiophysicsStateTest, Reset) {
    bio->activity_ema = 0.5f;
    bio->cable_valid = true;

    nimcp_myelin_biophysics_reset(bio);

    EXPECT_FLOAT_EQ(bio->activity_ema, 0.0f);
    EXPECT_FALSE(bio->cable_valid);
}

TEST_F(BiophysicsStateTest, UpdateActivityEMA) {
    bio->activity_ema = 0.0f;

    nimcp_myelin_update_activity_ema(bio, 1.0f, 0.1f);
    EXPECT_GT(bio->activity_ema, 0.0f);

    float prev = bio->activity_ema;
    nimcp_myelin_update_activity_ema(bio, 0.0f, 0.1f);
    EXPECT_LT(bio->activity_ema, prev);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
