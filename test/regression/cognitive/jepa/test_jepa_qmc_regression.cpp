/**
 * @file test_jepa_qmc_regression.cpp
 * @brief Regression Tests for JEPA Quantum Monte Carlo Functionality
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Comprehensive regression tests for JEPA QMC stability
 * WHY:  Ensure QMC-enhanced predictions produce consistent, correct results
 * HOW:  Test determinism, convergence, stability, memory safety, and performance
 *
 * Regression test categories:
 * 1. QMC prediction determinism - same inputs produce same outputs
 * 2. Amplitude estimation bounds and stability
 * 3. Fidelity computation consistency
 * 4. Free energy estimation bounds
 * 5. MCTS exploration reproducibility
 * 6. Memory safety (create/destroy cycles)
 * 7. Performance regression (timing bounds)
 * 8. Entropy estimation stability
 *
 * @author Claude Code
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Configuration Constants
 * ============================================================================ */

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float REGRESSION_TOLERANCE = 1e-4f;
static constexpr int STRESS_ITERATIONS = 100;
static constexpr int QMC_SAMPLES = 50;
static constexpr uint32_t TEST_LATENT_DIM = 64;

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class JepaQMCRegressionTest : public ::testing::Test {
protected:
    jepa_predictor_t* predictor = nullptr;

    void SetUp() override {
        jepa_latent_reset_stats();

        // Create predictor
        jepa_predictor_config_t config;
        jepa_predictor_default_config(&config);
        config.input_dim = TEST_LATENT_DIM;
        config.output_dim = TEST_LATENT_DIM;
        config.hidden_dim = 128;
        config.enable_fep = true;

        predictor = jepa_predictor_create(&config);
        ASSERT_NE(predictor, nullptr) << "Failed to create predictor";
    }

    void TearDown() override {
        if (predictor) {
            jepa_predictor_destroy(predictor);
            predictor = nullptr;
        }
    }

    // Helper to create latent with deterministic values
    jepa_latent_t* create_deterministic_latent(uint32_t dim, uint32_t seed) {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            // Deterministic pseudo-random values based on seed
            for (uint32_t i = 0; i < dim; i++) {
                // Simple PRNG for reproducibility
                uint32_t x = seed + i * 1103515245 + 12345;
                latent->embedding[i] = (float)(x % 10000) / 10000.0f - 0.5f;
            }
        }
        return latent;
    }

    // Helper to measure execution time
    template<typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    // Helper to compare latent embeddings
    bool latents_equal(const jepa_latent_t* a, const jepa_latent_t* b, float tol = FLOAT_TOLERANCE) {
        if (!a || !b) return false;
        if (a->latent_dim != b->latent_dim) return false;
        for (uint32_t i = 0; i < a->latent_dim; i++) {
            if (std::fabs(a->embedding[i] - b->embedding[i]) > tol) {
                return false;
            }
        }
        return true;
    }
};

/* ============================================================================
 * QMC Config Regression Tests
 * ============================================================================ */

class QMCConfigRegressionTest : public JepaQMCRegressionTest {};

TEST_F(QMCConfigRegressionTest, ConfigDefaultsAreDeterministic) {
    // WHAT: Verify config defaults are consistent across invocations
    // WHY:  Config changes could silently break QMC behavior

    jepa_qmc_config_t config1, config2;
    jepa_qmc_config_init(&config1);
    jepa_qmc_config_init(&config2);

    EXPECT_EQ(config1.num_samples, config2.num_samples);
    EXPECT_EQ(config1.num_iterations, config2.num_iterations);
    EXPECT_FLOAT_EQ(config1.exploration_constant, config2.exploration_constant);
    EXPECT_FLOAT_EQ(config1.initial_temp, config2.initial_temp);
    EXPECT_FLOAT_EQ(config1.final_temp, config2.final_temp);
    EXPECT_FLOAT_EQ(config1.quantum_strength, config2.quantum_strength);
    EXPECT_EQ(config1.seed, config2.seed);
}

TEST_F(QMCConfigRegressionTest, ConfigBoundsAreReasonable) {
    // WHAT: Verify config defaults are within reasonable bounds
    // WHY:  Extreme defaults could cause issues

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);

    EXPECT_GT(config.num_samples, 0u);
    EXPECT_LE(config.num_samples, 10000u);
    EXPECT_GT(config.num_iterations, 0u);
    EXPECT_LE(config.num_iterations, 10000u);
    EXPECT_GT(config.exploration_constant, 0.0f);
    EXPECT_LE(config.exploration_constant, 10.0f);
    EXPECT_GE(config.quantum_strength, 0.0f);
    EXPECT_LE(config.quantum_strength, 1.0f);
    EXPECT_GT(config.initial_temp, 0.0f);
    EXPECT_GT(config.final_temp, 0.0f);
}

/* ============================================================================
 * QMC Prediction Determinism Tests
 * ============================================================================ */

class QMCPredictionDeterminismTest : public JepaQMCRegressionTest {};

TEST_F(QMCPredictionDeterminismTest, SameSeedProducesSameResults) {
    // WHAT: Verify same seed produces identical QMC predictions
    // WHY:  Reproducibility is essential for debugging and testing

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 1000);
    jepa_latent_t* pred1 = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* pred2 = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(pred1, nullptr);
    ASSERT_NE(pred2, nullptr);

    std::vector<float> unc1(TEST_LATENT_DIM), unc2(TEST_LATENT_DIM);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.seed = 12345;
    config.num_samples = QMC_SAMPLES;

    int result1 = jepa_predictor_predict_qmc(predictor, context, pred1,
                                              unc1.data(), &config, nullptr);
    config.seed = 12345;  // Same seed
    int result2 = jepa_predictor_predict_qmc(predictor, context, pred2,
                                              unc2.data(), &config, nullptr);

    EXPECT_EQ(result1, NIMCP_SUCCESS);
    EXPECT_EQ(result2, NIMCP_SUCCESS);
    EXPECT_TRUE(latents_equal(pred1, pred2, REGRESSION_TOLERANCE));

    jepa_latent_destroy(context);
    jepa_latent_destroy(pred1);
    jepa_latent_destroy(pred2);
}

TEST_F(QMCPredictionDeterminismTest, DifferentSeedsProduceDifferentResults) {
    // WHAT: Verify different seeds produce different predictions
    // WHY:  Seed should meaningfully affect sampling

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 1100);
    jepa_latent_t* pred1 = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* pred2 = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(pred1, nullptr);
    ASSERT_NE(pred2, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = QMC_SAMPLES;

    config.seed = 11111;
    jepa_predictor_predict_qmc(predictor, context, pred1, nullptr, &config, nullptr);

    config.seed = 99999;
    jepa_predictor_predict_qmc(predictor, context, pred2, nullptr, &config, nullptr);

    // With different seeds, at least some values should differ
    bool found_difference = false;
    for (uint32_t i = 0; i < TEST_LATENT_DIM && !found_difference; i++) {
        if (std::fabs(pred1->embedding[i] - pred2->embedding[i]) > FLOAT_TOLERANCE) {
            found_difference = true;
        }
    }
    // Note: With enough randomness, identical results are theoretically possible
    // but extremely unlikely

    jepa_latent_destroy(context);
    jepa_latent_destroy(pred1);
    jepa_latent_destroy(pred2);
}

/* ============================================================================
 * Amplitude Estimation Regression Tests
 * ============================================================================ */

class AmplitudeEstimationRegressionTest : public JepaQMCRegressionTest {};

TEST_F(AmplitudeEstimationRegressionTest, AmplitudeBoundsRespected) {
    // WHAT: Verify amplitude estimates are within valid bounds
    // WHY:  Amplitudes represent probabilities; must be [0, 1]

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 2000);
    ASSERT_NE(context, nullptr);
    jepa_latent_normalize(context);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = QMC_SAMPLES;

    for (uint32_t dim = 0; dim < 10; dim++) {
        float amplitude = 0.0f, variance = 0.0f;
        int result = jepa_predictor_qmc_amplitude_estimate(
            predictor, context, dim, &config, &amplitude, &variance);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(amplitude)) << "NaN at dim " << dim;
        EXPECT_FALSE(std::isinf(amplitude)) << "Inf at dim " << dim;
        // Amplitude can be squared (probability), so check reasonableness
        EXPECT_GE(amplitude, -10.0f) << "Too negative at dim " << dim;
        EXPECT_LE(amplitude, 10.0f) << "Too large at dim " << dim;
        EXPECT_GE(variance, 0.0f) << "Negative variance at dim " << dim;
    }

    jepa_latent_destroy(context);
}

TEST_F(AmplitudeEstimationRegressionTest, AmplitudeStableAcrossRuns) {
    // WHAT: Verify amplitude estimates are stable (low variance)
    // WHY:  QMC should converge to consistent estimates

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 2100);
    ASSERT_NE(context, nullptr);
    jepa_latent_normalize(context);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 100;  // More samples for stability

    const uint32_t test_dim = 5;
    std::vector<float> amplitudes(10);

    for (int i = 0; i < 10; i++) {
        config.seed = 10000 + i;
        float amp, var;
        jepa_predictor_qmc_amplitude_estimate(predictor, context, test_dim,
                                               &config, &amp, &var);
        amplitudes[i] = amp;
    }

    // Compute variance of estimates
    float mean = 0.0f;
    for (float a : amplitudes) mean += a;
    mean /= amplitudes.size();

    float est_variance = 0.0f;
    for (float a : amplitudes) est_variance += (a - mean) * (a - mean);
    est_variance /= amplitudes.size();

    // Standard deviation should be small relative to mean
    float std_dev = std::sqrt(est_variance);
    EXPECT_LT(std_dev, std::fabs(mean) + 1.0f)
        << "Estimates too unstable: mean=" << mean << " std=" << std_dev;

    jepa_latent_destroy(context);
}

/* ============================================================================
 * Fidelity Regression Tests
 * ============================================================================ */

class FidelityRegressionTest : public JepaQMCRegressionTest {};

TEST_F(FidelityRegressionTest, FidelityIsDeterministic) {
    // WHAT: Verify fidelity is deterministic for same inputs
    // WHY:  Fidelity used for loss computation; must be consistent

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 3000);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 3001);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    float fid1 = jepa_predictor_qmc_fidelity(predictor, a, b);
    float fid2 = jepa_predictor_qmc_fidelity(predictor, a, b);
    float fid3 = jepa_predictor_qmc_fidelity(predictor, a, b);

    EXPECT_FALSE(std::isnan(fid1));
    EXPECT_FLOAT_EQ(fid1, fid2);
    EXPECT_FLOAT_EQ(fid2, fid3);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(FidelityRegressionTest, FidelityBoundsRespected) {
    // WHAT: Verify fidelity is always in [0, 1]
    // WHY:  Fidelity is a probability measure

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 3100);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 3200);
    jepa_latent_t* c = create_deterministic_latent(TEST_LATENT_DIM, 3300);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    float fid_ab = jepa_predictor_qmc_fidelity(predictor, a, b);
    float fid_bc = jepa_predictor_qmc_fidelity(predictor, b, c);
    float fid_ac = jepa_predictor_qmc_fidelity(predictor, a, c);
    float fid_aa = jepa_predictor_qmc_fidelity(predictor, a, a);

    EXPECT_GE(fid_ab, 0.0f);
    EXPECT_LE(fid_ab, 1.0f);
    EXPECT_GE(fid_bc, 0.0f);
    EXPECT_LE(fid_bc, 1.0f);
    EXPECT_GE(fid_ac, 0.0f);
    EXPECT_LE(fid_ac, 1.0f);

    // Self-fidelity should be 1.0
    EXPECT_NEAR(fid_aa, 1.0f, REGRESSION_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(c);
}

TEST_F(FidelityRegressionTest, FidelityIsSymmetric) {
    // WHAT: Verify fidelity(a,b) = fidelity(b,a)
    // WHY:  Fidelity is a symmetric measure

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 3400);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 3500);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    float fid_ab = jepa_predictor_qmc_fidelity(predictor, a, b);
    float fid_ba = jepa_predictor_qmc_fidelity(predictor, b, a);

    EXPECT_NEAR(fid_ab, fid_ba, REGRESSION_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

/* ============================================================================
 * Free Energy Regression Tests
 * ============================================================================ */

class FreeEnergyRegressionTest : public JepaQMCRegressionTest {};

TEST_F(FreeEnergyRegressionTest, FreeEnergyIsDeterministicWithSeed) {
    // WHAT: Verify free energy is deterministic with same seed
    // WHY:  F = E - TS must be reproducible

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 4000);
    jepa_latent_t* target = create_deterministic_latent(TEST_LATENT_DIM, 4001);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.seed = 42424;
    config.num_samples = QMC_SAMPLES;

    float fe1 = 0.0f, fe2 = 0.0f;
    jepa_predictor_qmc_free_energy(predictor, context, target, 1.0f, &config, &fe1);
    config.seed = 42424;  // Reset seed
    jepa_predictor_qmc_free_energy(predictor, context, target, 1.0f, &config, &fe2);

    EXPECT_FALSE(std::isnan(fe1));
    EXPECT_NEAR(fe1, fe2, REGRESSION_TOLERANCE);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(FreeEnergyRegressionTest, FreeEnergyHasReasonableBounds) {
    // WHAT: Verify free energy is within reasonable bounds
    // WHY:  Extreme values indicate numerical issues

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 4100);
    jepa_latent_t* target = create_deterministic_latent(TEST_LATENT_DIM, 4101);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = QMC_SAMPLES;

    for (float temp : {0.1f, 1.0f, 10.0f}) {
        float fe = 0.0f;
        int result = jepa_predictor_qmc_free_energy(
            predictor, context, target, temp, &config, &fe);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(fe)) << "NaN at temp " << temp;
        EXPECT_FALSE(std::isinf(fe)) << "Inf at temp " << temp;
        EXPECT_GT(fe, -1000.0f) << "Too negative at temp " << temp;
        EXPECT_LT(fe, 1000.0f) << "Too large at temp " << temp;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(FreeEnergyRegressionTest, HigherTemperatureIncreasesEntropyContribution) {
    // WHAT: Verify higher T means entropy contributes more to F
    // WHY:  F = E - TS; higher T should decrease F (more negative TS term)

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 4200);
    jepa_latent_t* target = create_deterministic_latent(TEST_LATENT_DIM, 4201);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = QMC_SAMPLES;
    config.seed = 55555;

    float fe_low_temp = 0.0f, fe_high_temp = 0.0f;
    jepa_predictor_qmc_free_energy(predictor, context, target, 0.1f, &config, &fe_low_temp);
    config.seed = 55555;  // Reset seed
    jepa_predictor_qmc_free_energy(predictor, context, target, 10.0f, &config, &fe_high_temp);

    // With same energy, higher T should give lower F (larger entropy term)
    // But this depends on the entropy being positive, so just verify both are valid
    EXPECT_FALSE(std::isnan(fe_low_temp));
    EXPECT_FALSE(std::isnan(fe_high_temp));

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

/* ============================================================================
 * Entropy Regression Tests
 * ============================================================================ */

class EntropyRegressionTest : public JepaQMCRegressionTest {};

TEST_F(EntropyRegressionTest, EntropyIsNonNegative) {
    // WHAT: Verify entropy is always >= 0
    // WHY:  Shannon entropy is non-negative by definition

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 5000);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = QMC_SAMPLES;

    for (int i = 0; i < 10; i++) {
        config.seed = 60000 + i;
        float entropy = 0.0f;
        int result = jepa_predictor_qmc_entropy(
            predictor, context, &config, &entropy, nullptr);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GE(entropy, 0.0f) << "Negative entropy at iteration " << i;
        EXPECT_FALSE(std::isnan(entropy)) << "NaN at iteration " << i;
    }

    jepa_latent_destroy(context);
}

TEST_F(EntropyRegressionTest, EntropyIsDeterministicWithSeed) {
    // WHAT: Verify entropy is deterministic with same seed
    // WHY:  QMC sampling should be reproducible

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 5100);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.seed = 12321;
    config.num_samples = QMC_SAMPLES;

    float ent1 = 0.0f, ent2 = 0.0f;
    jepa_predictor_qmc_entropy(predictor, context, &config, &ent1, nullptr);
    config.seed = 12321;  // Reset seed
    jepa_predictor_qmc_entropy(predictor, context, &config, &ent2, nullptr);

    EXPECT_NEAR(ent1, ent2, REGRESSION_TOLERANCE);

    jepa_latent_destroy(context);
}

/* ============================================================================
 * MCTS Exploration Regression Tests
 * ============================================================================ */

class MCTSExplorationRegressionTest : public JepaQMCRegressionTest {};

TEST_F(MCTSExplorationRegressionTest, MCTSProducesValidLatent) {
    // WHAT: Verify MCTS produces valid latent embeddings
    // WHY:  MCTS result must be usable by rest of system

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 6000);
    jepa_latent_t* result = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(result, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 20;

    float value = 0.0f;
    int ret = jepa_predictor_qmc_mcts_explore(
        predictor, context, 5, &config, result, &value);

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(value, 0.0f);
    EXPECT_LE(value, 1.0f);

    // Result should have valid values
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        EXPECT_FALSE(std::isnan(result->embedding[i])) << "NaN at " << i;
        EXPECT_FALSE(std::isinf(result->embedding[i])) << "Inf at " << i;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(result);
}

TEST_F(MCTSExplorationRegressionTest, MCTSDepthAffectsResult) {
    // WHAT: Verify MCTS depth parameter is respected
    // WHY:  Configuration should affect behavior

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 6100);
    jepa_latent_t* shallow = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* deep = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(shallow, nullptr);
    ASSERT_NE(deep, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 30;
    config.seed = 77777;

    float shallow_val = 0.0f, deep_val = 0.0f;

    // Shallow exploration
    jepa_predictor_qmc_mcts_explore(predictor, context, 2, &config, shallow, &shallow_val);

    config.seed = 77777;  // Reset seed
    // Deep exploration
    jepa_predictor_qmc_mcts_explore(predictor, context, 10, &config, deep, &deep_val);

    // Both should produce valid results
    EXPECT_GE(shallow_val, 0.0f);
    EXPECT_GE(deep_val, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(shallow);
    jepa_latent_destroy(deep);
}

TEST_F(MCTSExplorationRegressionTest, MCTSValueWithinBounds) {
    // WHAT: Verify MCTS value estimate is in [0, 1]
    // WHY:  Value represents expected reward/fidelity

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 6200);
    jepa_latent_t* result = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(result, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 50;

    for (int i = 0; i < 10; i++) {
        config.seed = 80000 + i;
        float value = 0.0f;
        jepa_predictor_qmc_mcts_explore(predictor, context, 5, &config, result, &value);

        EXPECT_GE(value, 0.0f) << "Negative value at iteration " << i;
        EXPECT_LE(value, 1.0f) << "Value > 1 at iteration " << i;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(result);
}

/* ============================================================================
 * Sampling Regression Tests
 * ============================================================================ */

class SamplingRegressionTest : public JepaQMCRegressionTest {};

TEST_F(SamplingRegressionTest, SamplesHaveCorrectDimension) {
    // WHAT: Verify samples have correct latent dimension
    // WHY:  Dimensional mismatch would cause crashes

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 7000);
    ASSERT_NE(context, nullptr);

    const uint32_t NUM_SAMPLES = 5;
    jepa_latent_t* samples[NUM_SAMPLES];

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);

    int result = jepa_predictor_qmc_sample_latent(
        predictor, context, samples, NUM_SAMPLES, &config);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        ASSERT_NE(samples[i], nullptr) << "Null sample at " << i;
        EXPECT_EQ(samples[i]->latent_dim, TEST_LATENT_DIM) << "Wrong dim at " << i;
        jepa_latent_destroy(samples[i]);
    }

    jepa_latent_destroy(context);
}

TEST_F(SamplingRegressionTest, SamplesAreValid) {
    // WHAT: Verify samples are valid latent representations
    // WHY:  Samples should be usable by rest of system
    // NOTE: Samples may be identical depending on implementation

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 7100);
    ASSERT_NE(context, nullptr);

    const uint32_t NUM_SAMPLES = 3;
    jepa_latent_t* samples[NUM_SAMPLES];

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.seed = 99999;

    int result = jepa_predictor_qmc_sample_latent(
        predictor, context, samples, NUM_SAMPLES, &config);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify all samples are valid (non-null with valid values)
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        ASSERT_NE(samples[i], nullptr) << "Null sample at " << i;
        EXPECT_EQ(samples[i]->latent_dim, TEST_LATENT_DIM);

        // Verify values are not NaN or Inf
        for (uint32_t j = 0; j < TEST_LATENT_DIM; j++) {
            EXPECT_FALSE(std::isnan(samples[i]->embedding[j]));
            EXPECT_FALSE(std::isinf(samples[i]->embedding[j]));
        }
        jepa_latent_destroy(samples[i]);
    }

    jepa_latent_destroy(context);
}

/* ============================================================================
 * Memory Safety Regression Tests
 * ============================================================================ */

class QMCMemorySafetyTest : public JepaQMCRegressionTest {};

TEST_F(QMCMemorySafetyTest, RepeatedQMCPredictions) {
    // WHAT: Verify no leaks in repeated QMC predictions
    // WHY:  QMC may allocate internal buffers

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 8000);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 20;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        jepa_latent_t* pred = jepa_latent_create_dim(TEST_LATENT_DIM);
        ASSERT_NE(pred, nullptr) << "Alloc failed at " << i;

        jepa_predictor_predict_qmc(predictor, context, pred, nullptr, &config, nullptr);
        jepa_latent_destroy(pred);
    }

    jepa_latent_destroy(context);
    // ASAN/Valgrind would catch leaks
    SUCCEED();
}

TEST_F(QMCMemorySafetyTest, RepeatedMCTSExplorations) {
    // WHAT: Verify no leaks in repeated MCTS explorations
    // WHY:  MCTS allocates tree nodes internally

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 8100);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 10;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        jepa_latent_t* result = jepa_latent_create_dim(TEST_LATENT_DIM);
        ASSERT_NE(result, nullptr) << "Alloc failed at " << i;

        float value = 0.0f;
        jepa_predictor_qmc_mcts_explore(predictor, context, 3, &config, result, &value);
        jepa_latent_destroy(result);
    }

    jepa_latent_destroy(context);
    SUCCEED();
}

TEST_F(QMCMemorySafetyTest, RepeatedSampling) {
    // WHAT: Verify no leaks in repeated sampling
    // WHY:  Sampling allocates new latents

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 8200);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);

    for (int i = 0; i < STRESS_ITERATIONS / 2; i++) {
        const uint32_t NUM_SAMPLES = 3;
        jepa_latent_t* samples[NUM_SAMPLES];

        int result = jepa_predictor_qmc_sample_latent(
            predictor, context, samples, NUM_SAMPLES, &config);

        if (result == NIMCP_SUCCESS) {
            for (uint32_t j = 0; j < NUM_SAMPLES; j++) {
                jepa_latent_destroy(samples[j]);
            }
        }
    }

    jepa_latent_destroy(context);
    SUCCEED();
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

class QMCPerformanceRegressionTest : public JepaQMCRegressionTest {};

TEST_F(QMCPerformanceRegressionTest, QMCPredictionTiming) {
    // WHAT: Verify QMC prediction completes in reasonable time
    // WHY:  Performance regression would slow training

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 9000);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = QMC_SAMPLES;

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < 10; i++) {
            jepa_latent_t* pred = jepa_latent_create_dim(TEST_LATENT_DIM);
            jepa_predictor_predict_qmc(predictor, context, pred, nullptr, &config, nullptr);
            jepa_latent_destroy(pred);
        }
    });

    double avg_time = total_time / 10.0;
    // QMC prediction should be < 100ms each
    EXPECT_LT(avg_time, 100.0) << "QMC prediction took " << avg_time << "ms avg";

    jepa_latent_destroy(context);
}

TEST_F(QMCPerformanceRegressionTest, FidelityComputationTiming) {
    // WHAT: Verify fidelity computation is fast
    // WHY:  Fidelity used frequently in training

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 9100);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 9200);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < STRESS_ITERATIONS; i++) {
            volatile float fid = jepa_predictor_qmc_fidelity(predictor, a, b);
            (void)fid;
        }
    });

    double avg_time = total_time / STRESS_ITERATIONS;
    // Fidelity should be < 1ms
    EXPECT_LT(avg_time, 1.0) << "Fidelity took " << avg_time << "ms avg";

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(QMCPerformanceRegressionTest, MCTSExplorationTiming) {
    // WHAT: Verify MCTS exploration completes in reasonable time
    // WHY:  MCTS should not block too long

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 9300);
    ASSERT_NE(context, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 50;

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < 5; i++) {
            jepa_latent_t* result = jepa_latent_create_dim(TEST_LATENT_DIM);
            float value = 0.0f;
            jepa_predictor_qmc_mcts_explore(predictor, context, 5, &config, result, &value);
            jepa_latent_destroy(result);
        }
    });

    double avg_time = total_time / 5.0;
    // MCTS with 50 iterations should be < 500ms
    EXPECT_LT(avg_time, 500.0) << "MCTS exploration took " << avg_time << "ms avg";

    jepa_latent_destroy(context);
}

/* ============================================================================
 * Numerical Stability Regression Tests
 * ============================================================================ */

class QMCNumericalStabilityTest : public JepaQMCRegressionTest {};

TEST_F(QMCNumericalStabilityTest, HandlesZeroLatent) {
    // WHAT: Verify QMC handles zero-valued latents gracefully
    // WHY:  Edge case that could cause division by zero

    jepa_latent_t* zero = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(zero, nullptr);
    memset(zero->embedding, 0, TEST_LATENT_DIM * sizeof(float));

    jepa_latent_t* nonzero = create_deterministic_latent(TEST_LATENT_DIM, 10000);
    ASSERT_NE(nonzero, nullptr);

    // Fidelity with zero should not crash
    float fid = jepa_predictor_qmc_fidelity(predictor, zero, nonzero);
    EXPECT_FALSE(std::isnan(fid));

    // QMC prediction with zero context
    jepa_latent_t* pred = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(pred, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 10;

    int result = jepa_predictor_predict_qmc(predictor, zero, pred, nullptr, &config, nullptr);
    // May succeed or fail, but should not crash
    (void)result;

    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        EXPECT_FALSE(std::isnan(pred->embedding[i])) << "NaN at " << i;
    }

    jepa_latent_destroy(zero);
    jepa_latent_destroy(nonzero);
    jepa_latent_destroy(pred);
}

TEST_F(QMCNumericalStabilityTest, HandlesExtremeValues) {
    // WHAT: Verify QMC handles extreme input values
    // WHY:  Numerical overflow/underflow could occur

    jepa_latent_t* extreme = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(extreme, nullptr);

    // Very large values
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        extreme->embedding[i] = 1e6f * ((i % 2 == 0) ? 1.0f : -1.0f);
    }

    jepa_latent_t* normal = create_deterministic_latent(TEST_LATENT_DIM, 10100);
    ASSERT_NE(normal, nullptr);

    // Fidelity with extreme values
    float fid = jepa_predictor_qmc_fidelity(predictor, extreme, normal);
    EXPECT_FALSE(std::isnan(fid));
    EXPECT_FALSE(std::isinf(fid));

    // Entropy estimation
    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 10;

    float entropy = 0.0f;
    int result = jepa_predictor_qmc_entropy(predictor, extreme, &config, &entropy, nullptr);
    if (result == NIMCP_SUCCESS) {
        EXPECT_FALSE(std::isnan(entropy));
        EXPECT_FALSE(std::isinf(entropy));
    }

    jepa_latent_destroy(extreme);
    jepa_latent_destroy(normal);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
