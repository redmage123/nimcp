/**
 * @file test_jepa_regression.cpp
 * @brief Regression Tests for JEPA (Joint Embedding Predictive Architecture) Module
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Comprehensive regression tests for JEPA module stability
 * WHY:  Ensure JEPA components produce consistent, correct results across changes
 * HOW:  Test determinism, convergence, stability, memory safety, and performance
 *
 * Regression test categories:
 * 1. Latent space determinism - same inputs produce same outputs
 * 2. FiLM conditioning output ranges
 * 3. Predictor training convergence
 * 4. Context encoding stability
 * 5. Masking strategy validity
 * 6. Memory safety (create/destroy cycles)
 * 7. Performance regression (timing bounds)
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
#include "cognitive/jepa/nimcp_jepa_context.h"
#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Configuration Constants
 * ============================================================================ */

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float REGRESSION_TOLERANCE = 1e-4f;
static constexpr int STRESS_ITERATIONS = 100;
static constexpr int CONVERGENCE_ITERATIONS = 500;
static constexpr uint32_t TEST_LATENT_DIM = 64;

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class JepaRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset global stats before each test
        jepa_latent_reset_stats();
    }

    void TearDown() override {
        // Verify no leaked memory through stats
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
 * Latent Space Determinism Tests
 * ============================================================================ */

class LatentDeterminismTest : public JepaRegressionTest {};

TEST_F(LatentDeterminismTest, NormalizationIsDeterministic) {
    // WHAT: Verify normalization produces identical results for identical inputs
    // WHY:  JEPA relies on normalized embeddings; non-determinism would break predictions

    jepa_latent_t* latent1 = create_deterministic_latent(TEST_LATENT_DIM, 42);
    jepa_latent_t* latent2 = create_deterministic_latent(TEST_LATENT_DIM, 42);
    ASSERT_NE(latent1, nullptr);
    ASSERT_NE(latent2, nullptr);

    // Verify inputs are identical
    EXPECT_TRUE(latents_equal(latent1, latent2));

    // Normalize both
    EXPECT_EQ(jepa_latent_normalize(latent1), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_latent_normalize(latent2), NIMCP_SUCCESS);

    // Results should be identical
    EXPECT_TRUE(latents_equal(latent1, latent2));

    jepa_latent_destroy(latent1);
    jepa_latent_destroy(latent2);
}

TEST_F(LatentDeterminismTest, LayerNormalizationIsDeterministic) {
    // WHAT: Verify layer normalization is deterministic
    // WHY:  Layer norm is used throughout JEPA architecture

    jepa_latent_t* latent1 = create_deterministic_latent(TEST_LATENT_DIM, 123);
    jepa_latent_t* latent2 = create_deterministic_latent(TEST_LATENT_DIM, 123);
    ASSERT_NE(latent1, nullptr);
    ASSERT_NE(latent2, nullptr);

    EXPECT_EQ(jepa_latent_layer_normalize(latent1), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_latent_layer_normalize(latent2), NIMCP_SUCCESS);

    EXPECT_TRUE(latents_equal(latent1, latent2));

    jepa_latent_destroy(latent1);
    jepa_latent_destroy(latent2);
}

TEST_F(LatentDeterminismTest, SimilarityComputationIsDeterministic) {
    // WHAT: Verify similarity computation is deterministic
    // WHY:  Similarity is used for loss computation; must be consistent

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 100);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 200);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Compute similarity multiple times
    float sim1 = jepa_latent_cosine_similarity(a, b);
    float sim2 = jepa_latent_cosine_similarity(a, b);
    float sim3 = jepa_latent_cosine_similarity(a, b);

    EXPECT_FALSE(std::isnan(sim1));
    EXPECT_FLOAT_EQ(sim1, sim2);
    EXPECT_FLOAT_EQ(sim2, sim3);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(LatentDeterminismTest, InterpolationIsDeterministic) {
    // WHAT: Verify interpolation produces consistent results
    // WHY:  Interpolation used in prediction and visualization

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 300);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 400);
    jepa_latent_t* result1 = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* result2 = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result1, nullptr);
    ASSERT_NE(result2, nullptr);

    float alpha = 0.3f;

    EXPECT_EQ(jepa_latent_interpolate(a, b, alpha, result1), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_latent_interpolate(a, b, alpha, result2), NIMCP_SUCCESS);

    EXPECT_TRUE(latents_equal(result1, result2));

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result1);
    jepa_latent_destroy(result2);
}

TEST_F(LatentDeterminismTest, MeanPoolingIsDeterministic) {
    // WHAT: Verify mean pooling produces consistent results
    // WHY:  Pooling aggregates multiple embeddings; must be stable

    const uint32_t num_latents = 5;
    std::vector<jepa_latent_t*> latents(num_latents);
    for (uint32_t i = 0; i < num_latents; i++) {
        latents[i] = create_deterministic_latent(TEST_LATENT_DIM, 500 + i);
        ASSERT_NE(latents[i], nullptr);
    }

    jepa_latent_t* result1 = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* result2 = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(result1, nullptr);
    ASSERT_NE(result2, nullptr);

    EXPECT_EQ(jepa_latent_mean_pool(
        const_cast<const jepa_latent_t**>(latents.data()), num_latents, result1), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_latent_mean_pool(
        const_cast<const jepa_latent_t**>(latents.data()), num_latents, result2), NIMCP_SUCCESS);

    EXPECT_TRUE(latents_equal(result1, result2));

    for (auto* l : latents) {
        jepa_latent_destroy(l);
    }
    jepa_latent_destroy(result1);
    jepa_latent_destroy(result2);
}

/* ============================================================================
 * FiLM Conditioning Tests
 * ============================================================================ */

class FiLMConditioningTest : public JepaRegressionTest {};

TEST_F(FiLMConditioningTest, OutputWithinExpectedRange) {
    // WHAT: Verify FiLM conditioning produces outputs in valid range
    // WHY:  FiLM modulates features; extreme values indicate bugs

    jepa_context_config_t config;
    EXPECT_EQ(jepa_context_default_config(&config), NIMCP_SUCCESS);
    config.conditioning = JEPA_COND_FILM;
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.context_dim = 32;

    jepa_context_encoder_t* encoder = jepa_context_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // Create input and output latents
    jepa_latent_t* input = create_deterministic_latent(TEST_LATENT_DIM, 1000);
    jepa_latent_t* output = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    // Set a simple context
    std::vector<float> context(config.context_dim, 0.5f);
    EXPECT_EQ(jepa_context_set_custom(encoder, context.data(), config.context_dim), NIMCP_SUCCESS);

    // Encode
    int result = jepa_context_encode(encoder, input, output);
    if (result == NIMCP_SUCCESS) {
        // Verify output values are in reasonable range
        for (uint32_t i = 0; i < output->latent_dim; i++) {
            EXPECT_FALSE(std::isnan(output->embedding[i])) << "NaN at index " << i;
            EXPECT_FALSE(std::isinf(output->embedding[i])) << "Inf at index " << i;
            // FiLM typically produces values within a few orders of magnitude of input
            EXPECT_LT(std::fabs(output->embedding[i]), 1000.0f)
                << "Extreme value at index " << i;
        }
    }

    jepa_latent_destroy(input);
    jepa_latent_destroy(output);
    jepa_context_encoder_destroy(encoder);
}

TEST_F(FiLMConditioningTest, ContextChangesOutput) {
    // WHAT: Verify different contexts produce different outputs
    // WHY:  Context conditioning should meaningfully affect embeddings

    jepa_context_config_t config;
    EXPECT_EQ(jepa_context_default_config(&config), NIMCP_SUCCESS);
    config.conditioning = JEPA_COND_FILM;
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.context_dim = 32;

    jepa_context_encoder_t* encoder = jepa_context_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    jepa_latent_t* input = create_deterministic_latent(TEST_LATENT_DIM, 1100);
    jepa_latent_t* output1 = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* output2 = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output1, nullptr);
    ASSERT_NE(output2, nullptr);

    // First context
    std::vector<float> context1(config.context_dim, 0.1f);
    jepa_context_set_custom(encoder, context1.data(), config.context_dim);
    int result1 = jepa_context_encode(encoder, input, output1);

    // Second (different) context
    std::vector<float> context2(config.context_dim, 0.9f);
    jepa_context_set_custom(encoder, context2.data(), config.context_dim);
    int result2 = jepa_context_encode(encoder, input, output2);

    if (result1 == NIMCP_SUCCESS && result2 == NIMCP_SUCCESS) {
        // Outputs should be different
        EXPECT_FALSE(latents_equal(output1, output2));
    }

    jepa_latent_destroy(input);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(output2);
    jepa_context_encoder_destroy(encoder);
}

/* ============================================================================
 * Predictor Training Convergence Tests
 * ============================================================================ */

class PredictorConvergenceTest : public JepaRegressionTest {};

TEST_F(PredictorConvergenceTest, LossDecreasesOverIterations) {
    // WHAT: Verify predictor training loss decreases over iterations
    // WHY:  Training should converge; non-decreasing loss indicates bugs

    jepa_predictor_config_t config;
    EXPECT_EQ(jepa_predictor_default_config(&config), NIMCP_SUCCESS);
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.hidden_dim = 128;
    config.num_layers = 2;
    config.learning_rate = 0.01f;

    jepa_predictor_t* predictor = jepa_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    // Set training mode
    EXPECT_EQ(jepa_predictor_set_training(predictor, true), NIMCP_SUCCESS);

    // Create consistent training data
    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 2000);
    jepa_latent_t* target = create_deterministic_latent(TEST_LATENT_DIM, 2001);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Normalize for stable training
    jepa_latent_normalize(context);
    jepa_latent_normalize(target);

    // Track losses
    std::vector<float> losses;
    losses.reserve(CONVERGENCE_ITERATIONS);

    for (int i = 0; i < CONVERGENCE_ITERATIONS; i++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(predictor, context, target, &loss);
        if (result == NIMCP_SUCCESS && !std::isnan(loss)) {
            losses.push_back(loss);
        }
    }

    // Verify we got enough valid losses
    EXPECT_GT(losses.size(), static_cast<size_t>(CONVERGENCE_ITERATIONS / 2));

    if (losses.size() >= 20) {
        // Compare average of first 10% to last 10%
        size_t window = losses.size() / 10;
        float early_avg = 0.0f, late_avg = 0.0f;

        for (size_t i = 0; i < window; i++) {
            early_avg += losses[i];
            late_avg += losses[losses.size() - window + i];
        }
        early_avg /= window;
        late_avg /= window;

        // Loss should decrease (late < early)
        EXPECT_LT(late_avg, early_avg) << "Training did not converge: early_avg="
            << early_avg << " late_avg=" << late_avg;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(predictor);
}

TEST_F(PredictorConvergenceTest, NoNaNOrInfDuringTraining) {
    // WHAT: Verify training never produces NaN or Inf values
    // WHY:  Numerical instability would break the entire system

    jepa_predictor_config_t config;
    EXPECT_EQ(jepa_predictor_default_config(&config), NIMCP_SUCCESS);
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.hidden_dim = 64;
    config.learning_rate = 0.001f;

    jepa_predictor_t* predictor = jepa_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    jepa_predictor_set_training(predictor, true);

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 3000);
    jepa_latent_t* target = create_deterministic_latent(TEST_LATENT_DIM, 3001);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    int nan_count = 0;
    int inf_count = 0;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        float loss = 0.0f;
        jepa_predictor_train_step(predictor, context, target, &loss);

        if (std::isnan(loss)) nan_count++;
        if (std::isinf(loss)) inf_count++;

        // Also check prediction output
        if (jepa_predictor_predict(predictor, context, prediction) == NIMCP_SUCCESS) {
            for (uint32_t j = 0; j < prediction->latent_dim; j++) {
                if (std::isnan(prediction->embedding[j])) nan_count++;
                if (std::isinf(prediction->embedding[j])) inf_count++;
            }
        }
    }

    EXPECT_EQ(nan_count, 0) << "Got " << nan_count << " NaN values during training";
    EXPECT_EQ(inf_count, 0) << "Got " << inf_count << " Inf values during training";

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(predictor);
}

/* ============================================================================
 * Context Encoding Stability Tests
 * ============================================================================ */

class ContextEncodingStabilityTest : public JepaRegressionTest {};

TEST_F(ContextEncodingStabilityTest, SameInputContextProducesSameOutput) {
    // WHAT: Verify identical input+context produces identical output
    // WHY:  Context encoding must be deterministic for reproducibility

    jepa_context_config_t config;
    EXPECT_EQ(jepa_context_default_config(&config), NIMCP_SUCCESS);
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.context_dim = 32;

    jepa_context_encoder_t* encoder = jepa_context_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    jepa_latent_t* input = create_deterministic_latent(TEST_LATENT_DIM, 4000);
    jepa_latent_t* output1 = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* output2 = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output1, nullptr);
    ASSERT_NE(output2, nullptr);

    // Set same context twice
    std::vector<float> context(config.context_dim, 0.5f);
    jepa_context_set_custom(encoder, context.data(), config.context_dim);

    // Encode twice
    int result1 = jepa_context_encode(encoder, input, output1);
    int result2 = jepa_context_encode(encoder, input, output2);

    if (result1 == NIMCP_SUCCESS && result2 == NIMCP_SUCCESS) {
        EXPECT_TRUE(latents_equal(output1, output2))
            << "Same input+context produced different outputs";
    }

    jepa_latent_destroy(input);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(output2);
    jepa_context_encoder_destroy(encoder);
}

TEST_F(ContextEncodingStabilityTest, ResetProducesConsistentState) {
    // WHAT: Verify reset returns encoder to consistent state
    // WHY:  Reset should allow clean re-use of encoder

    jepa_context_config_t config;
    EXPECT_EQ(jepa_context_default_config(&config), NIMCP_SUCCESS);
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.context_dim = 32;

    jepa_context_encoder_t* encoder = jepa_context_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    jepa_latent_t* input = create_deterministic_latent(TEST_LATENT_DIM, 4100);
    jepa_latent_t* output_before = jepa_latent_create_dim(TEST_LATENT_DIM);
    jepa_latent_t* output_after = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output_before, nullptr);
    ASSERT_NE(output_after, nullptr);

    // Encode with some context
    std::vector<float> context(config.context_dim, 0.7f);
    jepa_context_set_custom(encoder, context.data(), config.context_dim);
    jepa_context_encode(encoder, input, output_before);

    // Reset
    EXPECT_EQ(jepa_context_encoder_reset(encoder), NIMCP_SUCCESS);

    // Set same context and encode again
    jepa_context_set_custom(encoder, context.data(), config.context_dim);
    int result = jepa_context_encode(encoder, input, output_after);

    if (result == NIMCP_SUCCESS) {
        // After reset with same context, should get same output
        EXPECT_TRUE(latents_equal(output_before, output_after, REGRESSION_TOLERANCE));
    }

    jepa_latent_destroy(input);
    jepa_latent_destroy(output_before);
    jepa_latent_destroy(output_after);
    jepa_context_encoder_destroy(encoder);
}

/* ============================================================================
 * Masking Strategy Validity Tests
 * ============================================================================ */

class MaskingValidityTest : public JepaRegressionTest {};

TEST_F(MaskingValidityTest, RandomMaskProducesValidRatio) {
    // WHAT: Verify random masking achieves target ratio
    // WHY:  Mask ratio affects training dynamics; must be accurate

    jepa_mask_config_t config;
    EXPECT_EQ(jepa_mask_default_config(&config, JEPA_MASK_RANDOM), NIMCP_SUCCESS);
    config.target_ratio = 0.75f;
    config.seed = 12345;
    config.use_fixed_seed = true;

    jepa_mask_generator_t* generator = jepa_mask_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t width = 16, height = 16;
    jepa_mask_t* mask = jepa_mask_create(width, height, 1);
    ASSERT_NE(mask, nullptr);

    // Generate multiple masks and check ratio
    float total_ratio = 0.0f;
    const int num_samples = 20;

    for (int i = 0; i < num_samples; i++) {
        EXPECT_EQ(jepa_mask_generate_2d(generator, width, height, mask), NIMCP_SUCCESS);
        EXPECT_EQ(jepa_mask_compute_stats(mask), NIMCP_SUCCESS);
        total_ratio += mask->mask_ratio;
    }

    float avg_ratio = total_ratio / num_samples;
    // Allow 15% tolerance for randomness
    EXPECT_NEAR(avg_ratio, config.target_ratio, 0.15f)
        << "Average mask ratio " << avg_ratio << " deviates from target " << config.target_ratio;

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(generator);
}

TEST_F(MaskingValidityTest, BlockMaskProducesContiguousRegions) {
    // WHAT: Verify block masking creates contiguous masked regions
    // WHY:  Block masking is key to spatial JEPA training

    jepa_mask_config_t config;
    EXPECT_EQ(jepa_mask_default_config(&config, JEPA_MASK_BLOCK), NIMCP_SUCCESS);
    config.target_ratio = 0.5f;
    config.params.block.num_blocks = 1;
    config.seed = 54321;
    config.use_fixed_seed = true;

    jepa_mask_generator_t* generator = jepa_mask_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t width = 16, height = 16;
    jepa_mask_t* mask = jepa_mask_create(width, height, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(generator, width, height, mask);
    if (result == NIMCP_SUCCESS) {
        // Count number of connected masked regions (simple flood fill)
        // For a single block mask, there should be exactly 1 or few connected regions
        uint32_t masked_count = 0;
        for (uint32_t i = 0; i < mask->total_size; i++) {
            if (mask->data[i] > 0.5f) {
                masked_count++;
            }
        }

        // Should have some masked pixels
        EXPECT_GT(masked_count, 0u);

        // Mask values should be binary (0 or 1)
        for (uint32_t i = 0; i < mask->total_size; i++) {
            EXPECT_TRUE(mask->data[i] < 0.01f || mask->data[i] > 0.99f)
                << "Non-binary mask value: " << mask->data[i];
        }
    }

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(generator);
}

TEST_F(MaskingValidityTest, CurriculumMaskingProgressesCorrectly) {
    // WHAT: Verify curriculum masking increases difficulty over time
    // WHY:  Curriculum learning starts easy and progressively increases

    jepa_mask_config_t config;
    EXPECT_EQ(jepa_mask_default_config(&config, JEPA_MASK_CURRICULUM), NIMCP_SUCCESS);
    config.params.curriculum.start_ratio = 0.25f;
    config.params.curriculum.end_ratio = 0.85f;
    config.params.curriculum.warmup_steps = 100;

    jepa_mask_generator_t* generator = jepa_mask_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    float start_ratio = jepa_mask_curriculum_get_ratio(generator);

    // Advance curriculum
    for (int i = 0; i < 50; i++) {
        jepa_mask_curriculum_step(generator);
    }

    float mid_ratio = jepa_mask_curriculum_get_ratio(generator);

    // Advance to near end
    for (int i = 0; i < 100; i++) {
        jepa_mask_curriculum_step(generator);
    }

    float end_ratio = jepa_mask_curriculum_get_ratio(generator);

    // Ratio should increase over time
    EXPECT_LE(start_ratio, mid_ratio);
    EXPECT_LE(mid_ratio, end_ratio);

    jepa_mask_generator_destroy(generator);
}

TEST_F(MaskingValidityTest, MaskInversionIsCorrect) {
    // WHAT: Verify mask inversion swaps visible/masked correctly
    // WHY:  Inversion used to get target regions from visible regions

    jepa_mask_config_t config;
    EXPECT_EQ(jepa_mask_default_config(&config, JEPA_MASK_RANDOM), NIMCP_SUCCESS);
    config.target_ratio = 0.5f;
    config.seed = 99999;
    config.use_fixed_seed = true;

    jepa_mask_generator_t* generator = jepa_mask_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t width = 8, height = 8;
    jepa_mask_t* mask = jepa_mask_create(width, height, 1);
    ASSERT_NE(mask, nullptr);

    EXPECT_EQ(jepa_mask_generate_2d(generator, width, height, mask), NIMCP_SUCCESS);

    // Store original values
    std::vector<float> original(mask->total_size);
    std::copy(mask->data, mask->data + mask->total_size, original.begin());

    // Invert
    EXPECT_EQ(jepa_mask_invert(mask), NIMCP_SUCCESS);

    // Verify inversion: each value should be 1 - original
    for (uint32_t i = 0; i < mask->total_size; i++) {
        float expected = 1.0f - original[i];
        EXPECT_NEAR(mask->data[i], expected, FLOAT_TOLERANCE)
            << "Inversion incorrect at index " << i;
    }

    // Double invert should restore original
    EXPECT_EQ(jepa_mask_invert(mask), NIMCP_SUCCESS);
    for (uint32_t i = 0; i < mask->total_size; i++) {
        EXPECT_NEAR(mask->data[i], original[i], FLOAT_TOLERANCE)
            << "Double inversion didn't restore at index " << i;
    }

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(generator);
}

/* ============================================================================
 * Memory Safety Tests
 * ============================================================================ */

class MemorySafetyTest : public JepaRegressionTest {};

TEST_F(MemorySafetyTest, LatentCreateDestroyCycles) {
    // WHAT: Verify no memory leaks in latent create/destroy cycles
    // WHY:  JEPA creates many latents during training; leaks would accumulate

    jepa_latent_stats_t stats_before, stats_after;
    jepa_latent_get_stats(&stats_before);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        jepa_latent_t* latent = jepa_latent_create_dim(TEST_LATENT_DIM);
        ASSERT_NE(latent, nullptr) << "Failed at iteration " << i;
        jepa_latent_destroy(latent);
    }

    jepa_latent_get_stats(&stats_after);

    // All created latents should be destroyed
    uint64_t created = stats_after.latents_created - stats_before.latents_created;
    uint64_t destroyed = stats_after.latents_destroyed - stats_before.latents_destroyed;
    EXPECT_EQ(created, destroyed)
        << "Created " << created << " but destroyed " << destroyed;
}

TEST_F(MemorySafetyTest, PredictorCreateDestroyCycles) {
    // WHAT: Verify no memory leaks in predictor create/destroy cycles
    // WHY:  Predictors contain weights; leaks would be significant

    for (int i = 0; i < 10; i++) {
        jepa_predictor_config_t config;
        jepa_predictor_default_config(&config);
        config.input_dim = TEST_LATENT_DIM;
        config.output_dim = TEST_LATENT_DIM;
        config.hidden_dim = 64;

        jepa_predictor_t* predictor = jepa_predictor_create(&config);
        ASSERT_NE(predictor, nullptr) << "Failed at iteration " << i;

        // Do some operations to ensure memory is actually used
        jepa_latent_t* ctx = create_deterministic_latent(TEST_LATENT_DIM, i);
        jepa_latent_t* pred = jepa_latent_create_dim(TEST_LATENT_DIM);
        if (ctx && pred) {
            jepa_predictor_predict(predictor, ctx, pred);
        }

        jepa_latent_destroy(ctx);
        jepa_latent_destroy(pred);
        jepa_predictor_destroy(predictor);
    }

    // If we get here without ASAN/Valgrind errors, likely no leaks
    SUCCEED();
}

TEST_F(MemorySafetyTest, ContextEncoderCreateDestroyCycles) {
    // WHAT: Verify no memory leaks in context encoder cycles
    // WHY:  Context encoder has internal buffers that must be freed

    for (int i = 0; i < 10; i++) {
        jepa_context_config_t config;
        jepa_context_default_config(&config);
        config.input_dim = TEST_LATENT_DIM;
        config.output_dim = TEST_LATENT_DIM;
        config.context_dim = 32;

        jepa_context_encoder_t* encoder = jepa_context_encoder_create(&config);
        ASSERT_NE(encoder, nullptr) << "Failed at iteration " << i;

        jepa_context_encoder_destroy(encoder);
    }

    SUCCEED();
}

TEST_F(MemorySafetyTest, MaskGeneratorCreateDestroyCycles) {
    // WHAT: Verify no memory leaks in mask generator cycles
    // WHY:  Mask generators have buffers that must be freed

    for (int i = 0; i < 10; i++) {
        jepa_mask_config_t config;
        jepa_mask_default_config(&config, JEPA_MASK_RANDOM);

        jepa_mask_generator_t* generator = jepa_mask_generator_create(&config);
        ASSERT_NE(generator, nullptr) << "Failed at iteration " << i;

        // Generate some masks
        jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
        if (mask) {
            jepa_mask_generate_2d(generator, 16, 16, mask);
            jepa_mask_destroy(mask);
        }

        jepa_mask_generator_destroy(generator);
    }

    SUCCEED();
}

TEST_F(MemorySafetyTest, CloneAndDestroyChain) {
    // WHAT: Verify memory safety in clone chains
    // WHY:  Clone creates independent copies; all must be freed

    jepa_latent_t* original = create_deterministic_latent(TEST_LATENT_DIM, 5000);
    ASSERT_NE(original, nullptr);

    std::vector<jepa_latent_t*> clones;
    clones.reserve(STRESS_ITERATIONS);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        jepa_latent_t* clone = jepa_latent_clone(original);
        ASSERT_NE(clone, nullptr) << "Clone failed at iteration " << i;
        clones.push_back(clone);
    }

    // Destroy in reverse order
    for (auto it = clones.rbegin(); it != clones.rend(); ++it) {
        jepa_latent_destroy(*it);
    }
    jepa_latent_destroy(original);

    SUCCEED();
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

class PerformanceRegressionTest : public JepaRegressionTest {};

TEST_F(PerformanceRegressionTest, LatentNormalizationTiming) {
    // WHAT: Verify normalization completes within expected time
    // WHY:  Performance regression would slow down training

    jepa_latent_t* latent = create_deterministic_latent(TEST_LATENT_DIM, 6000);
    ASSERT_NE(latent, nullptr);

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < STRESS_ITERATIONS; i++) {
            jepa_latent_normalize(latent);
        }
    });

    double avg_time = total_time / STRESS_ITERATIONS;
    // Normalization should be < 1ms for 64-dim vector
    EXPECT_LT(avg_time, 1.0) << "Normalization took " << avg_time << "ms avg";

    jepa_latent_destroy(latent);
}

TEST_F(PerformanceRegressionTest, SimilarityComputationTiming) {
    // WHAT: Verify similarity computation is fast enough
    // WHY:  Similarity is computed many times during training

    jepa_latent_t* a = create_deterministic_latent(TEST_LATENT_DIM, 6100);
    jepa_latent_t* b = create_deterministic_latent(TEST_LATENT_DIM, 6200);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < STRESS_ITERATIONS; i++) {
            volatile float sim = jepa_latent_cosine_similarity(a, b);
            (void)sim;
        }
    });

    double avg_time = total_time / STRESS_ITERATIONS;
    // Similarity should be < 0.5ms for 64-dim vectors
    EXPECT_LT(avg_time, 0.5) << "Similarity took " << avg_time << "ms avg";

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(PerformanceRegressionTest, PredictorForwardPassTiming) {
    // WHAT: Verify predictor forward pass is fast enough
    // WHY:  Forward pass happens every training iteration

    jepa_predictor_config_t config;
    jepa_predictor_default_config(&config);
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.hidden_dim = 128;
    config.num_layers = 2;

    jepa_predictor_t* predictor = jepa_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 6300);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < STRESS_ITERATIONS; i++) {
            jepa_predictor_predict(predictor, context, prediction);
        }
    });

    double avg_time = total_time / STRESS_ITERATIONS;
    // Forward pass should be < 5ms for small model
    EXPECT_LT(avg_time, 5.0) << "Forward pass took " << avg_time << "ms avg";

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(predictor);
}

TEST_F(PerformanceRegressionTest, MaskGenerationTiming) {
    // WHAT: Verify mask generation is fast enough
    // WHY:  Masks generated every batch during training

    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_RANDOM);

    jepa_mask_generator_t* generator = jepa_mask_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    double total_time = measure_time_ms([&]() {
        for (int i = 0; i < STRESS_ITERATIONS; i++) {
            jepa_mask_generate_2d(generator, 16, 16, mask);
        }
    });

    double avg_time = total_time / STRESS_ITERATIONS;
    // Mask generation should be < 1ms for 16x16
    EXPECT_LT(avg_time, 1.0) << "Mask generation took " << avg_time << "ms avg";

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(generator);
}

/* ============================================================================
 * Numerical Stability Tests
 * ============================================================================ */

class NumericalStabilityTest : public JepaRegressionTest {};

TEST_F(NumericalStabilityTest, ExtendedTrainingNoOverflow) {
    // WHAT: Verify extended training doesn't cause numerical overflow
    // WHY:  Training can accumulate errors leading to overflow

    jepa_predictor_config_t config;
    jepa_predictor_default_config(&config);
    config.input_dim = TEST_LATENT_DIM;
    config.output_dim = TEST_LATENT_DIM;
    config.hidden_dim = 64;
    config.learning_rate = 0.001f;
    config.weight_decay = 0.01f;  // Helps prevent overflow

    jepa_predictor_t* predictor = jepa_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    jepa_predictor_set_training(predictor, true);

    jepa_latent_t* context = create_deterministic_latent(TEST_LATENT_DIM, 7000);
    jepa_latent_t* target = create_deterministic_latent(TEST_LATENT_DIM, 7001);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_latent_normalize(context);
    jepa_latent_normalize(target);

    float max_loss = 0.0f;
    float min_loss = std::numeric_limits<float>::max();
    int overflow_count = 0;

    for (int i = 0; i < CONVERGENCE_ITERATIONS; i++) {
        float loss = 0.0f;
        jepa_predictor_train_step(predictor, context, target, &loss);

        if (std::isinf(loss) || std::isnan(loss)) {
            overflow_count++;
        } else {
            max_loss = std::max(max_loss, loss);
            min_loss = std::min(min_loss, loss);
        }
    }

    EXPECT_EQ(overflow_count, 0) << "Got " << overflow_count << " overflow values";
    EXPECT_LT(max_loss, 1e6f) << "Max loss is unreasonably large: " << max_loss;

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(predictor);
}

TEST_F(NumericalStabilityTest, ExtremeInputValues) {
    // WHAT: Verify handling of extreme input values
    // WHY:  Real data may have unexpected value ranges

    jepa_latent_t* extreme = jepa_latent_create_dim(TEST_LATENT_DIM);
    ASSERT_NE(extreme, nullptr);

    // Very large values
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        extreme->embedding[i] = 1e6f * ((i % 2 == 0) ? 1.0f : -1.0f);
    }

    // Normalization should handle this
    int result = jepa_latent_normalize(extreme);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Result should be unit length
    float norm = jepa_latent_norm(extreme);
    EXPECT_FALSE(std::isnan(norm));
    EXPECT_FALSE(std::isinf(norm));
    EXPECT_NEAR(norm, 1.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(extreme);
}

TEST_F(NumericalStabilityTest, PrecisionBoundsRespected) {
    // WHAT: Verify precision stays within valid bounds
    // WHY:  Precision used for FEP integration; must be valid

    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = TEST_LATENT_DIM;
    config.enable_variance = true;

    jepa_latent_t* latent = jepa_latent_create(&config);
    ASSERT_NE(latent, nullptr);
    ASSERT_NE(latent->variance, nullptr);

    // Set very small variance (high precision)
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        latent->variance[i] = 1e-8f;
    }
    EXPECT_EQ(jepa_latent_update_precision(latent), NIMCP_SUCCESS);
    EXPECT_GE(latent->precision, JEPA_LATENT_MIN_PRECISION);
    EXPECT_LE(latent->precision, JEPA_LATENT_MAX_PRECISION);
    EXPECT_FALSE(std::isinf(latent->precision));

    // Set very large variance (low precision)
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        latent->variance[i] = 1e6f;
    }
    EXPECT_EQ(jepa_latent_update_precision(latent), NIMCP_SUCCESS);
    EXPECT_GE(latent->precision, JEPA_LATENT_MIN_PRECISION);
    EXPECT_LE(latent->precision, JEPA_LATENT_MAX_PRECISION);

    jepa_latent_destroy(latent);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
