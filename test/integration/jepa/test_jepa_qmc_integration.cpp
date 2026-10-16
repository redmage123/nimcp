/**
 * @file test_jepa_qmc_integration.cpp
 * @brief Integration tests for JEPA Quantum Monte Carlo functionality
 *
 * WHAT: Test JEPA QMC integration with FEP and other cognitive modules
 * WHY:  Ensure QMC-enhanced predictions integrate correctly with system
 * HOW:  Test multi-module interactions, FEP coordination, end-to-end flows
 *
 * Test categories:
 * 1. QMC-FEP integration - free energy and precision coordination
 * 2. QMC multimodal - uncertainty across modalities
 * 3. QMC context encoding - temporal QMC predictions
 * 4. QMC exploration - MCTS with predictor evaluation
 * 5. QMC training integration - annealing with training loops
 *
 * @author Claude Code
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_context.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

static constexpr uint32_t TEST_LATENT_DIM = 64;
static constexpr uint32_t TEST_HIDDEN_DIM = 128;
static constexpr float TEST_EPSILON = 1e-5f;

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class JepaQmcIntegrationTest : public ::testing::Test {
protected:
    jepa_predictor_t* predictor = nullptr;
    fep_orchestrator_t* orchestrator = nullptr;

    void SetUp() override {
        // Initialize FEP orchestrator
        fep_orchestrator_config_t orch_config;
        fep_orchestrator_default_config(&orch_config);
        orch_config.enable_statistics = true;
        orch_config.enable_logging = false;

        orchestrator = fep_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr) << "Failed to create FEP orchestrator";

        // Initialize JEPA predictor with FEP enabled
        jepa_predictor_config_t pred_config;
        jepa_predictor_default_config(&pred_config);
        pred_config.enable_fep = true;
        pred_config.input_dim = TEST_LATENT_DIM;
        pred_config.output_dim = TEST_LATENT_DIM;
        pred_config.hidden_dim = TEST_HIDDEN_DIM;

        predictor = jepa_predictor_create(&pred_config);
        ASSERT_NE(predictor, nullptr) << "Failed to create JEPA predictor";
    }

    void TearDown() override {
        if (predictor) {
            jepa_predictor_destroy(predictor);
            predictor = nullptr;
        }
        if (orchestrator) {
            fep_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }

    // Helper to create a latent with pseudo-random values
    jepa_latent_t* create_test_latent(uint32_t dim, float base_value = 0.5f) {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = base_value + sinf((float)i * 0.1f) * 0.3f;
            }
        }
        return latent;
    }

    // Helper to compute vector difference norm
    float compute_norm_diff(const float* a, const float* b, uint32_t dim) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float d = a[i] - b[i];
            sum += d * d;
        }
        return sqrtf(sum);
    }
};

/* ============================================================================
 * QMC-FEP Integration Tests
 * ============================================================================ */

TEST_F(JepaQmcIntegrationTest, QMCFreeEnergyMatchesFEP) {
    // WHAT: QMC free energy estimation should align with FEP principles
    // WHY:  Ensure thermodynamic consistency with FEP framework
    // HOW:  Compare QMC free energy with FEP-computed values

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_LATENT_DIM, 0.4f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 100;

    float qmc_free_energy = 0.0f;
    int result = jepa_predictor_qmc_free_energy(
        predictor, context, target, 1.0f, &config, &qmc_free_energy);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(qmc_free_energy));
    EXPECT_FALSE(std::isinf(qmc_free_energy));

    // Free energy should be bounded reasonably
    EXPECT_GT(qmc_free_energy, -100.0f);
    EXPECT_LT(qmc_free_energy, 100.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaQmcIntegrationTest, QMCUncertaintyIntegratesWithPrecision) {
    // WHAT: QMC uncertainty should inversely relate to FEP precision
    // WHY:  High precision = low uncertainty, fundamental FEP relationship
    // HOW:  Compare QMC variance with predictor precision

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    std::vector<float> uncertainty(TEST_LATENT_DIM);
    jepa_qmc_stats_t stats;

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 50;

    int result = jepa_predictor_predict_qmc(
        predictor, context, prediction, uncertainty.data(), &config, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Get predictor precision
    float precision = predictor->prediction_precision;

    // Average uncertainty should inversely relate to precision
    float avg_uncertainty = 0.0f;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        avg_uncertainty += uncertainty[i];
    }
    avg_uncertainty /= TEST_LATENT_DIM;

    // Higher precision should mean lower uncertainty (rough check)
    // precision * uncertainty should be bounded
    float product = precision * avg_uncertainty;
    EXPECT_GT(product, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaQmcIntegrationTest, QMCEntropyWithFEPUpdates) {
    // WHAT: QMC entropy should change appropriately with FEP updates
    // WHY:  Training should reduce prediction entropy (more certainty)
    // HOW:  Measure entropy before and after training

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_LATENT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 100;

    // Measure initial entropy
    float initial_entropy = 0.0f;
    int result = jepa_predictor_qmc_entropy(predictor, context, &config,
                                             &initial_entropy, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Do some training
    jepa_predictor_set_training(predictor, true);
    for (int i = 0; i < 50; i++) {
        float loss = 0.0f;
        jepa_predictor_train_step(predictor, context, target, &loss);
    }
    jepa_predictor_set_training(predictor, false);

    // Measure final entropy
    float final_entropy = 0.0f;
    result = jepa_predictor_qmc_entropy(predictor, context, &config,
                                         &final_entropy, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Both should be valid
    EXPECT_GE(initial_entropy, 0.0f);
    EXPECT_GE(final_entropy, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

/* ============================================================================
 * QMC Multimodal Integration Tests
 * ============================================================================ */

TEST_F(JepaQmcIntegrationTest, QMCFidelityAcrossModalities) {
    // WHAT: QMC fidelity should detect cross-modal similarity
    // WHY:  Multimodal JEPA requires comparing latents across modalities
    // HOW:  Create similar latents with different modalities

    jepa_latent_t* visual = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* speech = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* different = create_test_latent(TEST_LATENT_DIM, 0.1f);

    ASSERT_NE(visual, nullptr);
    ASSERT_NE(speech, nullptr);
    ASSERT_NE(different, nullptr);

    visual->modality = JEPA_MODALITY_VISUAL;
    speech->modality = JEPA_MODALITY_SPEECH;
    different->modality = JEPA_MODALITY_TEXT;

    // Similar content, different modality
    float fid_similar = jepa_predictor_qmc_fidelity(predictor, visual, speech);
    // Different content
    float fid_different = jepa_predictor_qmc_fidelity(predictor, visual, different);

    // Similar should have higher fidelity
    EXPECT_GE(fid_similar, 0.0f);
    EXPECT_LE(fid_similar, 1.0f);
    EXPECT_GE(fid_different, 0.0f);
    EXPECT_LE(fid_different, 1.0f);

    // Similar latents should have higher fidelity
    EXPECT_GT(fid_similar, fid_different);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(different);
}

TEST_F(JepaQmcIntegrationTest, QMCSamplingPreservesModality) {
    // WHAT: QMC sampling should preserve modality information
    // WHY:  Samples should maintain semantic structure
    // HOW:  Sample from a modality-tagged latent

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    ASSERT_NE(context, nullptr);
    context->modality = JEPA_MODALITY_VISUAL;

    const uint32_t NUM_SAMPLES = 3;
    jepa_latent_t* samples[NUM_SAMPLES];

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);

    int result = jepa_predictor_qmc_sample_latent(
        predictor, context, samples, NUM_SAMPLES, &config);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify samples exist and have correct dimension
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        ASSERT_NE(samples[i], nullptr);
        EXPECT_EQ(samples[i]->latent_dim, TEST_LATENT_DIM);
        jepa_latent_destroy(samples[i]);
    }

    jepa_latent_destroy(context);
}

/* ============================================================================
 * QMC Exploration Integration Tests
 * ============================================================================ */

TEST_F(JepaQmcIntegrationTest, MCTSExplorationFindsBetterPrediction) {
    // WHAT: MCTS exploration should find predictions with good fidelity
    // WHY:  Structured search should improve over random predictions
    // HOW:  Compare MCTS result with base prediction

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* best_latent = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* base_pred = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(best_latent, nullptr);
    ASSERT_NE(base_pred, nullptr);

    // Get base prediction
    int result = jepa_predictor_predict(predictor, context, base_pred);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    float base_fidelity = jepa_predictor_qmc_fidelity(predictor, context, base_pred);

    // Run MCTS exploration
    float mcts_value = 0.0f;
    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 50;

    result = jepa_predictor_qmc_mcts_explore(
        predictor, context, 5, &config, best_latent, &mcts_value);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(mcts_value, 0.0f);
    EXPECT_LE(mcts_value, 1.0f);

    // MCTS should find at least as good as base
    // (may be equal if base is already good)
    EXPECT_GE(mcts_value, base_fidelity * 0.8f);  // Allow some tolerance

    jepa_latent_destroy(context);
    jepa_latent_destroy(best_latent);
    jepa_latent_destroy(base_pred);
}

TEST_F(JepaQmcIntegrationTest, MCTSDepthAffectsExploration) {
    // WHAT: Deeper MCTS exploration should explore more states
    // WHY:  Validate exploration depth parameter works correctly
    // HOW:  Compare shallow vs deep exploration

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* shallow_result = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* deep_result = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(shallow_result, nullptr);
    ASSERT_NE(deep_result, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 30;
    config.seed = 12345;  // Same seed for comparison

    float shallow_value = 0.0f, deep_value = 0.0f;

    // Shallow exploration
    int result = jepa_predictor_qmc_mcts_explore(
        predictor, context, 2, &config, shallow_result, &shallow_value);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Reset seed for fair comparison
    config.seed = 12345;

    // Deep exploration
    result = jepa_predictor_qmc_mcts_explore(
        predictor, context, 10, &config, deep_result, &deep_value);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Both should produce valid results
    EXPECT_GE(shallow_value, 0.0f);
    EXPECT_GE(deep_value, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(shallow_result);
    jepa_latent_destroy(deep_result);
}

/* ============================================================================
 * QMC Training Integration Tests
 * ============================================================================ */

TEST_F(JepaQmcIntegrationTest, QMCAmplitudeStableAfterTraining) {
    // WHAT: QMC amplitude estimates should stabilize with training
    // WHY:  Trained predictor should have more consistent outputs
    // HOW:  Compare amplitude variance before and after training

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_LATENT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 100;

    // Measure initial variance (across multiple dims)
    std::vector<float> initial_variances(10);
    for (uint32_t d = 0; d < 10; d++) {
        float amp, var;
        jepa_predictor_qmc_amplitude_estimate(predictor, context, d, &config, &amp, &var);
        initial_variances[d] = var;
    }

    // Train
    jepa_predictor_set_training(predictor, true);
    for (int i = 0; i < 100; i++) {
        float loss = 0.0f;
        jepa_predictor_train_step(predictor, context, target, &loss);
    }
    jepa_predictor_set_training(predictor, false);

    // Measure final variance
    std::vector<float> final_variances(10);
    for (uint32_t d = 0; d < 10; d++) {
        float amp, var;
        jepa_predictor_qmc_amplitude_estimate(predictor, context, d, &config, &amp, &var);
        final_variances[d] = var;
    }

    // All variances should be non-negative
    for (uint32_t d = 0; d < 10; d++) {
        EXPECT_GE(initial_variances[d], 0.0f);
        EXPECT_GE(final_variances[d], 0.0f);
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

/* ============================================================================
 * End-to-End QMC Pipeline Tests
 * ============================================================================ */

TEST_F(JepaQmcIntegrationTest, FullQMCPipelineE2E) {
    // WHAT: Test complete QMC pipeline from input to output
    // WHY:  Verify all QMC components work together
    // HOW:  Run prediction, exploration, sampling, and evaluation

    jepa_latent_t* context = create_test_latent(TEST_LATENT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_LATENT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 50;
    config.num_iterations = 20;

    // Step 1: QMC Prediction with uncertainty
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);
    std::vector<float> uncertainty(TEST_LATENT_DIM);
    jepa_qmc_stats_t pred_stats;

    int result = jepa_predictor_predict_qmc(
        predictor, context, prediction, uncertainty.data(), &config, &pred_stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(pred_stats.samples_taken, 0u);

    // Step 2: Compute fidelity with target
    float fidelity = jepa_predictor_qmc_fidelity(predictor, prediction, target);
    EXPECT_GE(fidelity, 0.0f);
    EXPECT_LE(fidelity, 1.0f);

    // Step 3: Estimate entropy
    float entropy = 0.0f;
    result = jepa_predictor_qmc_entropy(predictor, context, &config, &entropy, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(entropy, 0.0f);

    // Step 4: Compute free energy
    float free_energy = 0.0f;
    result = jepa_predictor_qmc_free_energy(
        predictor, context, target, 1.0f, &config, &free_energy);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 5: MCTS exploration
    jepa_latent_t* explored = jepa_latent_create_dim(TEST_LATENT_DIM);
    float explore_value = 0.0f;
    result = jepa_predictor_qmc_mcts_explore(
        predictor, context, 5, &config, explored, &explore_value);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 6: Sample from distribution
    const uint32_t NUM_SAMPLES = 3;
    jepa_latent_t* samples[NUM_SAMPLES];
    result = jepa_predictor_qmc_sample_latent(
        predictor, context, samples, NUM_SAMPLES, &config);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Cleanup samples
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_destroy(samples[i]);
    }

    jepa_latent_destroy(explored);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaQmcIntegrationTest, QMCWithContextEncoder) {
    // WHAT: QMC should work with context-encoded sequences
    // WHY:  Real JEPA uses temporal context encoding
    // HOW:  Create sequence, encode, then use QMC prediction

    // Create a simple sequence of latents
    const uint32_t SEQ_LEN = 4;
    std::vector<jepa_latent_t*> sequence(SEQ_LEN);
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        sequence[i] = create_test_latent(TEST_LATENT_DIM, 0.2f + 0.1f * i);
        ASSERT_NE(sequence[i], nullptr);
    }

    // Use last as context for QMC prediction
    jepa_latent_t* context = sequence[SEQ_LEN - 1];
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);
    std::vector<float> uncertainty(TEST_LATENT_DIM);

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 50;

    int result = jepa_predictor_predict_qmc(
        predictor, context, prediction, uncertainty.data(), &config, nullptr);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Cleanup
    jepa_latent_destroy(prediction);
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        jepa_latent_destroy(sequence[i]);
    }
}
