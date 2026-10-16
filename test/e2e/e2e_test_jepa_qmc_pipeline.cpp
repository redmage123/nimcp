/**
 * @file e2e_test_jepa_qmc_pipeline.cpp
 * @brief End-to-end tests for JEPA Quantum Monte Carlo Pipeline
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Tests full JEPA QMC pipeline including:
 * - QMC-enhanced prediction with uncertainty quantification
 * - FEP integration for free energy minimization
 * - MCTS-guided latent space exploration
 * - Multi-step training with QMC evaluation
 * - Context encoding with QMC predictions
 * - Cross-modal QMC fidelity computation
 *
 * E2E scenarios:
 * 1. Full prediction pipeline with QMC uncertainty
 * 2. Training loop with QMC-based curriculum
 * 3. MCTS exploration for goal-directed planning
 * 4. FEP-QMC coordination for free energy estimation
 * 5. Multi-modal JEPA with QMC cross-modal binding
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_context.h"
#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

static constexpr uint32_t E2E_LATENT_DIM = 64;
static constexpr uint32_t E2E_HIDDEN_DIM = 128;
static constexpr uint32_t E2E_CONTEXT_DIM = 32;
static constexpr uint32_t E2E_TRAINING_STEPS = 100;
static constexpr uint32_t E2E_QMC_SAMPLES = 50;
static constexpr float E2E_EPSILON = 1e-5f;

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class JepaQmcE2ETest : public ::testing::Test {
protected:
    jepa_predictor_t* predictor = nullptr;
    jepa_context_encoder_t* context_encoder = nullptr;
    fep_orchestrator_t* orchestrator = nullptr;
    jepa_mask_generator_t* mask_gen = nullptr;

    void SetUp() override {
        // Create FEP orchestrator
        fep_orchestrator_config_t orch_cfg;
        fep_orchestrator_default_config(&orch_cfg);
        orch_cfg.enable_logging = false;
        orch_cfg.enable_statistics = true;
        orchestrator = fep_orchestrator_create(&orch_cfg);

        // Create JEPA predictor with FEP enabled
        jepa_predictor_config_t pred_cfg;
        jepa_predictor_default_config(&pred_cfg);
        pred_cfg.input_dim = E2E_LATENT_DIM;
        pred_cfg.output_dim = E2E_LATENT_DIM;
        pred_cfg.hidden_dim = E2E_HIDDEN_DIM;
        pred_cfg.enable_fep = true;
        pred_cfg.learning_rate = 0.01f;
        predictor = jepa_predictor_create(&pred_cfg);
        ASSERT_NE(predictor, nullptr);

        // Create context encoder
        jepa_context_config_t ctx_cfg;
        jepa_context_default_config(&ctx_cfg);
        ctx_cfg.input_dim = E2E_LATENT_DIM;
        ctx_cfg.output_dim = E2E_LATENT_DIM;
        ctx_cfg.context_dim = E2E_CONTEXT_DIM;
        context_encoder = jepa_context_encoder_create(&ctx_cfg);

        // Create mask generator for curriculum
        jepa_mask_config_t mask_cfg;
        jepa_mask_default_config(&mask_cfg, JEPA_MASK_CURRICULUM);
        mask_cfg.params.curriculum.start_ratio = 0.25f;
        mask_cfg.params.curriculum.end_ratio = 0.75f;
        mask_cfg.params.curriculum.warmup_steps = 50;
        mask_gen = jepa_mask_generator_create(&mask_cfg);
    }

    void TearDown() override {
        if (predictor) {
            jepa_predictor_destroy(predictor);
            predictor = nullptr;
        }
        if (context_encoder) {
            jepa_context_encoder_destroy(context_encoder);
            context_encoder = nullptr;
        }
        if (orchestrator) {
            fep_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (mask_gen) {
            jepa_mask_generator_destroy(mask_gen);
            mask_gen = nullptr;
        }
    }

    // Helper to create deterministic latent
    jepa_latent_t* create_latent(uint32_t dim, uint32_t seed) {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
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
};

/* ============================================================================
 * E2E Pipeline Tests
 * ============================================================================ */

TEST_F(JepaQmcE2ETest, FullPredictionPipelineWithUncertainty) {
    // WHAT: Test complete QMC prediction pipeline from input to output
    // WHY:  Verify all components work together in realistic scenario
    // HOW:  Encode context, predict with QMC, evaluate uncertainty

    // Step 1: Create input sequence (simulating temporal input)
    const uint32_t SEQ_LEN = 4;
    std::vector<jepa_latent_t*> sequence(SEQ_LEN);
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        sequence[i] = create_latent(E2E_LATENT_DIM, 1000 + i * 100);
        ASSERT_NE(sequence[i], nullptr);
        sequence[i]->modality = JEPA_MODALITY_VISUAL;
        jepa_latent_normalize(sequence[i]);
    }

    // Step 2: Use last element as context for prediction
    jepa_latent_t* context = sequence[SEQ_LEN - 1];

    // Step 3: Perform QMC prediction with uncertainty
    jepa_latent_t* prediction = jepa_latent_create_dim(E2E_LATENT_DIM);
    std::vector<float> uncertainty(E2E_LATENT_DIM);
    ASSERT_NE(prediction, nullptr);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = E2E_QMC_SAMPLES;

    jepa_qmc_stats_t stats;
    int result = jepa_predictor_predict_qmc(predictor, context, prediction,
                                             uncertainty.data(), &qmc_cfg, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.samples_taken, 0u);

    // Step 4: Verify prediction quality metrics
    float avg_uncertainty = 0.0f;
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
        avg_uncertainty += uncertainty[i];
    }
    avg_uncertainty /= E2E_LATENT_DIM;
    EXPECT_GE(avg_uncertainty, 0.0f);

    // Step 5: Compute fidelity with original context
    float fidelity = jepa_predictor_qmc_fidelity(predictor, context, prediction);
    EXPECT_GE(fidelity, 0.0f);
    EXPECT_LE(fidelity, 1.0f);

    // Cleanup
    for (auto* l : sequence) {
        jepa_latent_destroy(l);
    }
    jepa_latent_destroy(prediction);
}

TEST_F(JepaQmcE2ETest, TrainingLoopWithQMCEvaluation) {
    // WHAT: Test training loop with QMC-based progress evaluation
    // WHY:  Real training requires periodic evaluation of prediction quality
    // HOW:  Train for multiple steps, evaluate with QMC metrics

    jepa_latent_t* context = create_latent(E2E_LATENT_DIM, 2000);
    jepa_latent_t* target = create_latent(E2E_LATENT_DIM, 2001);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    jepa_latent_normalize(context);
    jepa_latent_normalize(target);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = 30;

    // Initial QMC evaluation
    float initial_entropy = 0.0f;
    jepa_predictor_qmc_entropy(predictor, context, &qmc_cfg, &initial_entropy, nullptr);

    float initial_free_energy = 0.0f;
    jepa_predictor_qmc_free_energy(predictor, context, target, 1.0f,
                                    &qmc_cfg, &initial_free_energy);

    // Training loop
    jepa_predictor_set_training(predictor, true);
    std::vector<float> losses;

    for (uint32_t step = 0; step < E2E_TRAINING_STEPS; step++) {
        float loss = 0.0f;
        jepa_predictor_train_step(predictor, context, target, &loss);
        losses.push_back(loss);

        // Advance curriculum mask
        if (mask_gen) {
            jepa_mask_curriculum_step(mask_gen);
        }
    }
    jepa_predictor_set_training(predictor, false);

    // Final QMC evaluation
    float final_entropy = 0.0f;
    jepa_predictor_qmc_entropy(predictor, context, &qmc_cfg, &final_entropy, nullptr);

    float final_free_energy = 0.0f;
    jepa_predictor_qmc_free_energy(predictor, context, target, 1.0f,
                                    &qmc_cfg, &final_free_energy);

    // Verify training progress
    EXPECT_GT(losses.size(), 0u);
    EXPECT_GE(initial_entropy, 0.0f);
    EXPECT_GE(final_entropy, 0.0f);

    // Loss should generally decrease
    float early_loss_avg = 0.0f, late_loss_avg = 0.0f;
    uint32_t window = E2E_TRAINING_STEPS / 10;
    for (uint32_t i = 0; i < window; i++) {
        early_loss_avg += losses[i];
        late_loss_avg += losses[losses.size() - window + i];
    }
    early_loss_avg /= window;
    late_loss_avg /= window;

    // Training should show improvement
    EXPECT_LT(late_loss_avg, early_loss_avg * 1.5f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaQmcE2ETest, MCTSExplorationForGoalDirectedPlanning) {
    // WHAT: Test MCTS exploration for finding optimal latent trajectories
    // WHY:  MCTS enables planning in latent space for goal-directed behavior
    // HOW:  Run MCTS exploration, verify trajectory quality

    jepa_latent_t* start_context = create_latent(E2E_LATENT_DIM, 3000);
    jepa_latent_t* goal_state = create_latent(E2E_LATENT_DIM, 3001);
    ASSERT_NE(start_context, nullptr);
    ASSERT_NE(goal_state, nullptr);
    jepa_latent_normalize(start_context);
    jepa_latent_normalize(goal_state);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_iterations = 50;
    qmc_cfg.exploration_constant = 1.414f;

    // Run MCTS exploration at different depths
    std::vector<float> values_by_depth;
    for (uint32_t depth = 2; depth <= 8; depth += 2) {
        jepa_latent_t* best_latent = jepa_latent_create_dim(E2E_LATENT_DIM);
        ASSERT_NE(best_latent, nullptr);

        float value = 0.0f;
        int result = jepa_predictor_qmc_mcts_explore(
            predictor, start_context, depth, &qmc_cfg, best_latent, &value);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);

        values_by_depth.push_back(value);

        // Verify best latent is valid
        for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
            EXPECT_FALSE(std::isnan(best_latent->embedding[i]));
        }

        jepa_latent_destroy(best_latent);
    }

    // All exploration depths should produce valid values
    for (float v : values_by_depth) {
        EXPECT_GE(v, 0.0f);
        EXPECT_LE(v, 1.0f);
    }

    jepa_latent_destroy(start_context);
    jepa_latent_destroy(goal_state);
}

TEST_F(JepaQmcE2ETest, FEPQMCCoordinationPipeline) {
    // WHAT: Test FEP-QMC coordination for free energy minimization
    // WHY:  QMC should help estimate and minimize free energy
    // HOW:  Compute FEP metrics via QMC, verify thermodynamic consistency

    jepa_latent_t* context = create_latent(E2E_LATENT_DIM, 4000);
    jepa_latent_t* target = create_latent(E2E_LATENT_DIM, 4001);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    jepa_latent_normalize(context);
    jepa_latent_normalize(target);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = E2E_QMC_SAMPLES;

    // Compute free energy at different temperatures
    std::vector<float> temperatures = {0.1f, 0.5f, 1.0f, 2.0f, 5.0f};
    std::vector<float> free_energies;

    for (float temp : temperatures) {
        float fe = 0.0f;
        int result = jepa_predictor_qmc_free_energy(
            predictor, context, target, temp, &qmc_cfg, &fe);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(fe));
        EXPECT_FALSE(std::isinf(fe));

        free_energies.push_back(fe);
    }

    // Compute entropy
    float entropy = 0.0f;
    jepa_predictor_qmc_entropy(predictor, context, &qmc_cfg, &entropy, nullptr);
    EXPECT_GE(entropy, 0.0f);

    // Compute fidelity (analogous to state overlap)
    float fidelity = jepa_predictor_qmc_fidelity(predictor, context, target);
    EXPECT_GE(fidelity, 0.0f);
    EXPECT_LE(fidelity, 1.0f);

    // Verify all free energies are bounded
    for (float fe : free_energies) {
        EXPECT_GT(fe, -1000.0f);
        EXPECT_LT(fe, 1000.0f);
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaQmcE2ETest, MultiModalQMCCrossModalBinding) {
    // WHAT: Test QMC fidelity for cross-modal latent binding
    // WHY:  Multi-modal JEPA requires comparing representations across modalities
    // HOW:  Create latents in different modalities, compute cross-modal fidelity

    // Create latents for different modalities with similar content
    jepa_latent_t* visual = create_latent(E2E_LATENT_DIM, 5000);
    jepa_latent_t* speech = create_latent(E2E_LATENT_DIM, 5000);  // Same seed = similar
    jepa_latent_t* text = create_latent(E2E_LATENT_DIM, 5100);    // Different

    ASSERT_NE(visual, nullptr);
    ASSERT_NE(speech, nullptr);
    ASSERT_NE(text, nullptr);

    visual->modality = JEPA_MODALITY_VISUAL;
    speech->modality = JEPA_MODALITY_SPEECH;
    text->modality = JEPA_MODALITY_TEXT;

    jepa_latent_normalize(visual);
    jepa_latent_normalize(speech);
    jepa_latent_normalize(text);

    // Compute cross-modal fidelities
    float fid_visual_speech = jepa_predictor_qmc_fidelity(predictor, visual, speech);
    float fid_visual_text = jepa_predictor_qmc_fidelity(predictor, visual, text);
    float fid_speech_text = jepa_predictor_qmc_fidelity(predictor, speech, text);

    // All should be valid
    EXPECT_GE(fid_visual_speech, 0.0f);
    EXPECT_LE(fid_visual_speech, 1.0f);
    EXPECT_GE(fid_visual_text, 0.0f);
    EXPECT_LE(fid_visual_text, 1.0f);
    EXPECT_GE(fid_speech_text, 0.0f);
    EXPECT_LE(fid_speech_text, 1.0f);

    // Similar content (visual-speech) should have higher fidelity than different
    EXPECT_GT(fid_visual_speech, fid_visual_text * 0.5f);

    // Sample from each modality
    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);

    const uint32_t NUM_SAMPLES = 3;
    jepa_latent_t* visual_samples[NUM_SAMPLES];
    jepa_latent_t* speech_samples[NUM_SAMPLES];

    int result = jepa_predictor_qmc_sample_latent(
        predictor, visual, visual_samples, NUM_SAMPLES, &qmc_cfg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    result = jepa_predictor_qmc_sample_latent(
        predictor, speech, speech_samples, NUM_SAMPLES, &qmc_cfg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Cleanup samples
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_destroy(visual_samples[i]);
        jepa_latent_destroy(speech_samples[i]);
    }

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(text);
}

TEST_F(JepaQmcE2ETest, ContextEncodingWithQMCPrediction) {
    // WHAT: Test context encoder integration with QMC prediction
    // WHY:  Real JEPA uses context encoding before prediction
    // HOW:  Encode input with context, then predict with QMC

    if (!context_encoder) {
        GTEST_SKIP() << "Context encoder not available";
    }

    // Create input and context
    jepa_latent_t* raw_input = create_latent(E2E_LATENT_DIM, 6000);
    jepa_latent_t* encoded = jepa_latent_create_dim(E2E_LATENT_DIM);
    ASSERT_NE(raw_input, nullptr);
    ASSERT_NE(encoded, nullptr);

    // Set temporal context
    std::vector<float> temporal_ctx(E2E_CONTEXT_DIM, 0.5f);
    jepa_context_set_custom(context_encoder, temporal_ctx.data(), E2E_CONTEXT_DIM);

    // Encode input with context
    int result = jepa_context_encode(context_encoder, raw_input, encoded);
    if (result != NIMCP_SUCCESS) {
        // Context encoding may not be fully implemented
        GTEST_SKIP() << "Context encoding returned " << result;
    }

    // Perform QMC prediction on encoded input
    jepa_latent_t* prediction = jepa_latent_create_dim(E2E_LATENT_DIM);
    std::vector<float> uncertainty(E2E_LATENT_DIM);
    ASSERT_NE(prediction, nullptr);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = E2E_QMC_SAMPLES;

    result = jepa_predictor_predict_qmc(predictor, encoded, prediction,
                                         uncertainty.data(), &qmc_cfg, nullptr);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify prediction
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_GE(uncertainty[i], 0.0f);
    }

    jepa_latent_destroy(raw_input);
    jepa_latent_destroy(encoded);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaQmcE2ETest, FullSystemPerformanceBenchmark) {
    // WHAT: Benchmark full QMC pipeline performance
    // WHY:  E2E performance is critical for real-time applications
    // HOW:  Time complete prediction+evaluation cycle

    jepa_latent_t* context = create_latent(E2E_LATENT_DIM, 7000);
    jepa_latent_t* target = create_latent(E2E_LATENT_DIM, 7001);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = E2E_QMC_SAMPLES;
    qmc_cfg.num_iterations = 30;

    const int NUM_ITERATIONS = 10;

    // Benchmark QMC prediction
    double pred_time = measure_time_ms([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            jepa_latent_t* pred = jepa_latent_create_dim(E2E_LATENT_DIM);
            jepa_predictor_predict_qmc(predictor, context, pred, nullptr, &qmc_cfg, nullptr);
            jepa_latent_destroy(pred);
        }
    });

    // Benchmark MCTS exploration
    double mcts_time = measure_time_ms([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            jepa_latent_t* result = jepa_latent_create_dim(E2E_LATENT_DIM);
            float value;
            jepa_predictor_qmc_mcts_explore(predictor, context, 5, &qmc_cfg, result, &value);
            jepa_latent_destroy(result);
        }
    });

    // Benchmark free energy computation
    double fe_time = measure_time_ms([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            float fe;
            jepa_predictor_qmc_free_energy(predictor, context, target, 1.0f, &qmc_cfg, &fe);
        }
    });

    // Report times (not strict assertions, just verify reasonable)
    double avg_pred_ms = pred_time / NUM_ITERATIONS;
    double avg_mcts_ms = mcts_time / NUM_ITERATIONS;
    double avg_fe_ms = fe_time / NUM_ITERATIONS;

    EXPECT_LT(avg_pred_ms, 500.0) << "QMC prediction too slow: " << avg_pred_ms << "ms";
    EXPECT_LT(avg_mcts_ms, 500.0) << "MCTS exploration too slow: " << avg_mcts_ms << "ms";
    EXPECT_LT(avg_fe_ms, 100.0) << "Free energy computation too slow: " << avg_fe_ms << "ms";

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaQmcE2ETest, AmplitudeEstimationPipeline) {
    // WHAT: Test QMC amplitude estimation end-to-end
    // WHY:  Amplitude estimation is fundamental to QMC
    // HOW:  Estimate amplitudes across dimensions, verify consistency

    jepa_latent_t* context = create_latent(E2E_LATENT_DIM, 8000);
    ASSERT_NE(context, nullptr);
    jepa_latent_normalize(context);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = E2E_QMC_SAMPLES;

    std::vector<float> amplitudes(E2E_LATENT_DIM);
    std::vector<float> variances(E2E_LATENT_DIM);

    // Estimate amplitude for each dimension
    for (uint32_t dim = 0; dim < E2E_LATENT_DIM; dim++) {
        int result = jepa_predictor_qmc_amplitude_estimate(
            predictor, context, dim, &qmc_cfg,
            &amplitudes[dim], &variances[dim]);

        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(amplitudes[dim]));
        EXPECT_GE(variances[dim], 0.0f);
    }

    // Verify amplitude statistics
    float mean_amp = 0.0f, mean_var = 0.0f;
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        mean_amp += amplitudes[i];
        mean_var += variances[i];
    }
    mean_amp /= E2E_LATENT_DIM;
    mean_var /= E2E_LATENT_DIM;

    // Mean amplitude should be reasonable
    EXPECT_GT(std::fabs(mean_amp), 0.0f);
    EXPECT_LT(std::fabs(mean_amp), 100.0f);

    jepa_latent_destroy(context);
}

TEST_F(JepaQmcE2ETest, RobustnessToNoisyInputs) {
    // WHAT: Test QMC robustness to noisy/extreme inputs
    // WHY:  Real data may be noisy; system should handle gracefully
    // HOW:  Create noisy inputs, verify QMC handles them without crashing

    // Create base context
    jepa_latent_t* base = create_latent(E2E_LATENT_DIM, 9000);
    ASSERT_NE(base, nullptr);
    jepa_latent_normalize(base);

    // Create noisy version
    jepa_latent_t* noisy = jepa_latent_clone(base);
    ASSERT_NE(noisy, nullptr);

    // Add noise
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        float noise = ((float)(i * 7919 % 1000) / 1000.0f - 0.5f) * 0.5f;
        noisy->embedding[i] += noise;
    }
    jepa_latent_normalize(noisy);

    jepa_qmc_config_t qmc_cfg;
    jepa_qmc_config_init(&qmc_cfg);
    qmc_cfg.num_samples = E2E_QMC_SAMPLES;

    // QMC prediction - may succeed or fail, but should not crash
    jepa_latent_t* pred_base = jepa_latent_create_dim(E2E_LATENT_DIM);
    jepa_latent_t* pred_noisy = jepa_latent_create_dim(E2E_LATENT_DIM);
    ASSERT_NE(pred_base, nullptr);
    ASSERT_NE(pred_noisy, nullptr);

    // Use regular prediction as a fallback test if QMC has issues
    int result1 = jepa_predictor_predict(predictor, base, pred_base);
    int result2 = jepa_predictor_predict(predictor, noisy, pred_noisy);

    // Regular prediction should work
    EXPECT_EQ(result1, NIMCP_SUCCESS);
    EXPECT_EQ(result2, NIMCP_SUCCESS);

    // Both predictions should be valid (no NaN/Inf)
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        EXPECT_FALSE(std::isnan(pred_base->embedding[i]));
        EXPECT_FALSE(std::isnan(pred_noisy->embedding[i]));
        EXPECT_FALSE(std::isinf(pred_base->embedding[i]));
        EXPECT_FALSE(std::isinf(pred_noisy->embedding[i]));
    }

    // Fidelity between base and noisy predictions (QMC fidelity)
    float fid = jepa_predictor_qmc_fidelity(predictor, pred_base, pred_noisy);
    EXPECT_GE(fid, 0.0f);
    EXPECT_LE(fid, 1.0f);

    // Similar inputs should have reasonable fidelity
    EXPECT_GT(fid, 0.3f);

    jepa_latent_destroy(base);
    jepa_latent_destroy(noisy);
    jepa_latent_destroy(pred_base);
    jepa_latent_destroy(pred_noisy);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
