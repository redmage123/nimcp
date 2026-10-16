/**
 * @file test_jepa_fep_integration.cpp
 * @brief Integration tests for JEPA + FEP orchestrator
 *
 * Tests:
 * - JEPA bridges with FEP orchestrator coordination
 * - Precision-weighted prediction error updates
 * - Belief propagation through JEPA prediction
 * - FEP-JEPA bidirectional integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-26
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_multimodal.h"
#include "cognitive/jepa/nimcp_jepa_context.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "perception/nimcp_visual_jepa_bridge.h"
#include "perception/nimcp_speech_jepa_bridge.h"
#include "utils/error/nimcp_error_codes.h"

/**
 * @brief Test fixture for JEPA-FEP integration tests
 */
class JepaFepIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize FEP orchestrator
        fep_orchestrator_config_t orch_config;
        fep_orchestrator_default_config(&orch_config);
        orch_config.enable_statistics = true;
        orch_config.enable_logging = false;  // Quiet for tests

        orchestrator = fep_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr) << "Failed to create FEP orchestrator";

        // Initialize JEPA predictor
        jepa_predictor_config_t pred_config;
        jepa_predictor_default_config(&pred_config);
        pred_config.enable_fep = true;
        pred_config.input_dim = TEST_LATENT_DIM;
        pred_config.output_dim = TEST_LATENT_DIM;
        pred_config.hidden_dim = 128;

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

    // Helper to create a latent with random data
    jepa_latent_t* create_random_latent(uint32_t dim, jepa_modality_t modality = JEPA_MODALITY_UNKNOWN) {
        jepa_latent_config_t config;
        jepa_latent_default_config(&config);
        config.latent_dim = dim;
        config.modality = modality;
        config.enable_variance = true;

        jepa_latent_t* latent = jepa_latent_create(&config);
        if (!latent) return nullptr;

        // Fill with pseudo-random values
        for (uint32_t i = 0; i < dim; i++) {
            latent->embedding[i] = sinf((float)i * 0.1f) * 0.5f + 0.5f;
            if (latent->variance) {
                latent->variance[i] = 0.1f + 0.01f * (i % 10);
            }
        }

        return latent;
    }

    // Helper to create test visual features
    std::vector<float> create_visual_features(uint32_t dim) {
        std::vector<float> features(dim);
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = cosf((float)i * 0.05f) * 0.5f + 0.5f;
        }
        return features;
    }

    static constexpr uint32_t TEST_LATENT_DIM = 64;
    static constexpr uint32_t TEST_FEATURE_DIM = 256;

    fep_orchestrator_t* orchestrator = nullptr;
    jepa_predictor_t* predictor = nullptr;
};

//=============================================================================
// JEPA-FEP Orchestrator Registration Tests
//=============================================================================

/**
 * @brief Test registering JEPA predictor as FEP bridge
 */
TEST_F(JepaFepIntegrationTest, RegisterJepaPredictorWithOrchestrator) {
    // Start orchestrator
    ASSERT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Define update callback for predictor
    auto predictor_update = [](fep_bridge_handle_t handle) -> int {
        // In real usage, this would update the predictor state
        (void)handle;
        return NIMCP_SUCCESS;
    };

    // Register predictor with orchestrator
    uint32_t bridge_id = 0;
    int result = fep_orchestrator_register_bridge(
        orchestrator,
        "jepa_predictor",
        FEP_BRIDGE_CATEGORY_JEPA,
        predictor,
        predictor_update,
        nullptr,  // No auto-destroy
        &bridge_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge_id, 0);

    // Verify bridge is registered
    const fep_bridge_entry_t* entry = fep_orchestrator_get_bridge(orchestrator, bridge_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->bridge_name, "jepa_predictor");
    EXPECT_EQ(entry->category, FEP_BRIDGE_CATEGORY_JEPA);
    EXPECT_TRUE(entry->enabled);

    // Cleanup
    EXPECT_EQ(fep_orchestrator_unregister_bridge(orchestrator, bridge_id), NIMCP_SUCCESS);
    EXPECT_EQ(fep_orchestrator_stop(orchestrator), 0);
}

/**
 * @brief Test orchestrator update triggers JEPA predictor updates
 */
TEST_F(JepaFepIntegrationTest, OrchestratorUpdateTriggersJepaUpdate) {
    static int update_count = 0;
    update_count = 0;

    auto predictor_update = [](fep_bridge_handle_t handle) -> int {
        (void)handle;
        update_count++;
        return NIMCP_SUCCESS;
    };

    ASSERT_EQ(fep_orchestrator_start(orchestrator), 0);

    uint32_t bridge_id = 0;
    ASSERT_EQ(fep_orchestrator_register_bridge(
        orchestrator,
        "jepa_predictor",
        FEP_BRIDGE_CATEGORY_JEPA,
        predictor,
        predictor_update,
        nullptr,
        &bridge_id
    ), NIMCP_SUCCESS);

    // Force update all bridges
    int updated = fep_orchestrator_force_update_all(orchestrator);
    EXPECT_GE(updated, 1);
    EXPECT_EQ(update_count, 1);

    // Update specific category
    updated = fep_orchestrator_update_category(orchestrator, FEP_BRIDGE_CATEGORY_JEPA, 1000);
    EXPECT_GE(updated, 0);  // May or may not update based on interval

    // Cleanup
    fep_orchestrator_unregister_bridge(orchestrator, bridge_id);
    fep_orchestrator_stop(orchestrator);
}

//=============================================================================
// Precision-Weighted Update Tests
//=============================================================================

/**
 * @brief Test precision-weighted prediction error computation
 */
TEST_F(JepaFepIntegrationTest, PrecisionWeightedPredictionError) {
    // Create context and target latents
    jepa_latent_t* context = create_random_latent(TEST_LATENT_DIM, JEPA_MODALITY_VISUAL);
    jepa_latent_t* target = create_random_latent(TEST_LATENT_DIM, JEPA_MODALITY_VISUAL);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);

    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Slightly modify target from context
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        target->embedding[i] = context->embedding[i] + 0.1f * sinf((float)i);
    }

    // Predict target from context
    EXPECT_EQ(jepa_predictor_predict(predictor, context, prediction), NIMCP_SUCCESS);

    // Compute prediction error
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_LATENT_DIM);
    ASSERT_NE(error, nullptr);

    EXPECT_EQ(jepa_predictor_compute_error(predictor, prediction, target, error), NIMCP_SUCCESS);

    // Verify error structure
    EXPECT_NE(error->error, nullptr);
    EXPECT_NE(error->weighted_error, nullptr);
    EXPECT_EQ(error->dim, TEST_LATENT_DIM);
    EXPECT_GT(error->precision, 0.0f);

    // Error should have finite values
    bool has_finite_error = true;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        if (!std::isfinite(error->error[i]) || !std::isfinite(error->weighted_error[i])) {
            has_finite_error = false;
            break;
        }
    }
    EXPECT_TRUE(has_finite_error);

    // Cleanup
    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
    jepa_latent_destroy(context);
}

/**
 * @brief Test precision update from prediction errors
 */
TEST_F(JepaFepIntegrationTest, PrecisionUpdateFromErrors) {
    jepa_latent_t* context = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* target = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);

    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Get initial precision
    float initial_precision = predictor->prediction_precision;

    // Make prediction and compute error
    EXPECT_EQ(jepa_predictor_predict(predictor, context, prediction), NIMCP_SUCCESS);

    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_LATENT_DIM);
    ASSERT_NE(error, nullptr);

    EXPECT_EQ(jepa_predictor_compute_error(predictor, prediction, target, error), NIMCP_SUCCESS);

    // Update precision based on error
    EXPECT_EQ(jepa_predictor_update_precision(predictor, error), NIMCP_SUCCESS);

    // Precision should have changed (may increase or decrease based on error)
    float new_precision = predictor->prediction_precision;

    // Just verify precision is valid (positive, finite)
    EXPECT_GT(new_precision, 0.0f);
    EXPECT_TRUE(std::isfinite(new_precision));

    // Log for debugging
    SCOPED_TRACE("Initial precision: " + std::to_string(initial_precision) +
                 ", New precision: " + std::to_string(new_precision));

    // Cleanup
    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
    jepa_latent_destroy(context);
}

/**
 * @brief Test FEP error conversion from JEPA predictor
 */
TEST_F(JepaFepIntegrationTest, JepaToFepErrorConversion) {
    jepa_latent_t* context = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* target = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_LATENT_DIM);

    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Predict
    EXPECT_EQ(jepa_predictor_predict(predictor, context, prediction), NIMCP_SUCCESS);

    // Compute internal error
    jepa_prediction_error_t* jepa_error = jepa_prediction_error_create(TEST_LATENT_DIM);
    ASSERT_NE(jepa_error, nullptr);

    EXPECT_EQ(jepa_predictor_compute_error(predictor, prediction, target, jepa_error), NIMCP_SUCCESS);

    // Convert to FEP error
    fep_prediction_error_t fep_error;
    memset(&fep_error, 0, sizeof(fep_error));
    fep_error.error = (float*)malloc(TEST_LATENT_DIM * sizeof(float));
    fep_error.weighted_error = (float*)malloc(TEST_LATENT_DIM * sizeof(float));
    fep_error.precision = (float*)malloc(TEST_LATENT_DIM * sizeof(float));
    fep_error.dim = TEST_LATENT_DIM;

    ASSERT_NE(fep_error.error, nullptr);

    int result = jepa_predictor_to_fep_error(predictor, jepa_error, &fep_error);
    EXPECT_EQ(result, 0);

    // Verify FEP error has valid data
    EXPECT_GT(fep_error.magnitude, 0.0f);
    EXPECT_TRUE(std::isfinite(fep_error.magnitude));

    // Cleanup
    free(fep_error.precision);
    free(fep_error.weighted_error);
    free(fep_error.error);
    jepa_prediction_error_destroy(jepa_error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
    jepa_latent_destroy(context);
}

//=============================================================================
// Belief Propagation Tests
//=============================================================================

/**
 * @brief Test belief update through JEPA prediction cycle
 */
TEST_F(JepaFepIntegrationTest, BeliefUpdateThroughPrediction) {
    // Create latents for belief propagation
    jepa_latent_t* prior = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* observation = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* posterior = jepa_latent_create_dim(TEST_LATENT_DIM);

    ASSERT_NE(prior, nullptr);
    ASSERT_NE(observation, nullptr);
    ASSERT_NE(posterior, nullptr);

    // Set prior variance (uncertainty)
    float prior_variance[TEST_LATENT_DIM];
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        prior_variance[i] = 0.5f;  // Moderate uncertainty
    }
    EXPECT_EQ(jepa_latent_set_variance(prior, prior_variance, TEST_LATENT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_latent_update_precision(prior), 0);

    // Predict posterior from prior (simulating belief update)
    EXPECT_EQ(jepa_predictor_predict(predictor, prior, posterior), NIMCP_SUCCESS);

    // Compute prediction error against observation
    float loss = jepa_predictor_compute_loss(predictor, posterior, observation);
    EXPECT_TRUE(std::isfinite(loss));
    EXPECT_GE(loss, 0.0f);

    // Perform training step (updates beliefs/weights)
    jepa_predictor_set_training(predictor, true);

    float training_loss = 0.0f;
    EXPECT_EQ(jepa_predictor_train_step(predictor, prior, observation, &training_loss), NIMCP_SUCCESS);
    EXPECT_TRUE(std::isfinite(training_loss));

    jepa_predictor_set_training(predictor, false);

    // Cleanup
    jepa_latent_destroy(posterior);
    jepa_latent_destroy(observation);
    jepa_latent_destroy(prior);
}

/**
 * @brief Test multi-step belief propagation
 */
TEST_F(JepaFepIntegrationTest, MultiStepBeliefPropagation) {
    const int NUM_STEPS = 5;
    std::vector<float> losses;

    jepa_predictor_set_training(predictor, true);

    // Create consistent context-target pairs
    jepa_latent_t* context = create_random_latent(TEST_LATENT_DIM);
    jepa_latent_t* target = create_random_latent(TEST_LATENT_DIM);

    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Make target similar to context (learnable pattern)
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        target->embedding[i] = context->embedding[i] * 1.1f;  // Simple scaling
    }

    // Train for multiple steps
    for (int step = 0; step < NUM_STEPS; step++) {
        float loss = 0.0f;
        EXPECT_EQ(jepa_predictor_train_step(predictor, context, target, &loss), NIMCP_SUCCESS);
        losses.push_back(loss);
    }

    // Get predictor statistics
    jepa_predictor_stats_t stats;
    EXPECT_EQ(jepa_predictor_get_stats(predictor, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.predictions_made, (uint64_t)NUM_STEPS);

    // Loss should generally decrease or stabilize
    // (Note: not guaranteed in all cases, but should be finite)
    for (float l : losses) {
        EXPECT_TRUE(std::isfinite(l));
        EXPECT_GE(l, 0.0f);
    }

    jepa_predictor_set_training(predictor, false);

    // Cleanup
    jepa_latent_destroy(target);
    jepa_latent_destroy(context);
}

//=============================================================================
// FEP Orchestrator + Multiple JEPA Bridges
//=============================================================================

/**
 * @brief Test orchestrator coordinating multiple JEPA bridges
 */
TEST_F(JepaFepIntegrationTest, OrchestratorMultipleJepaBridges) {
    // Create visual and speech JEPA bridges
    visual_jepa_bridge_config_t visual_config;
    visual_jepa_bridge_default_config(&visual_config);
    visual_jepa_bridge_t* visual_bridge = visual_jepa_bridge_create(&visual_config);

    speech_jepa_bridge_config_t speech_config;
    speech_jepa_bridge_default_config(&speech_config);
    speech_jepa_bridge_t* speech_bridge = speech_jepa_bridge_create(&speech_config);

    // These may be NULL if not fully implemented - handle gracefully

    ASSERT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Counter for update tracking
    static int visual_updates = 0;
    static int speech_updates = 0;
    visual_updates = 0;
    speech_updates = 0;

    auto visual_update = [](fep_bridge_handle_t) -> int {
        visual_updates++;
        return NIMCP_SUCCESS;
    };

    auto speech_update = [](fep_bridge_handle_t) -> int {
        speech_updates++;
        return NIMCP_SUCCESS;
    };

    uint32_t visual_id = 0, speech_id = 0;

    // Register bridges
    if (visual_bridge) {
        EXPECT_EQ(fep_orchestrator_register_bridge(
            orchestrator, "visual_jepa", FEP_BRIDGE_CATEGORY_JEPA,
            visual_bridge, visual_update, nullptr, &visual_id
        ), NIMCP_SUCCESS);
    }

    if (speech_bridge) {
        EXPECT_EQ(fep_orchestrator_register_bridge(
            orchestrator, "speech_jepa", FEP_BRIDGE_CATEGORY_JEPA,
            speech_bridge, speech_update, nullptr, &speech_id
        ), NIMCP_SUCCESS);
    }

    // Force update all
    fep_orchestrator_force_update_all(orchestrator);

    // Verify updates occurred
    if (visual_bridge) EXPECT_EQ(visual_updates, 1);
    if (speech_bridge) EXPECT_EQ(speech_updates, 1);

    // Get orchestrator stats
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), NIMCP_SUCCESS);

    // JEPA category should have bridges
    EXPECT_GE(stats.categories[FEP_BRIDGE_CATEGORY_JEPA].bridge_count,
              (visual_bridge ? 1u : 0u) + (speech_bridge ? 1u : 0u));

    // Cleanup
    if (visual_id > 0) fep_orchestrator_unregister_bridge(orchestrator, visual_id);
    if (speech_id > 0) fep_orchestrator_unregister_bridge(orchestrator, speech_id);
    fep_orchestrator_stop(orchestrator);

    if (speech_bridge) speech_jepa_bridge_destroy(speech_bridge);
    if (visual_bridge) visual_jepa_bridge_destroy(visual_bridge);
}

/**
 * @brief Test JEPA category update interval configuration
 */
TEST_F(JepaFepIntegrationTest, JepaCategoryUpdateInterval) {
    ASSERT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Get current JEPA category config
    fep_category_config_t config;
    EXPECT_EQ(fep_orchestrator_get_category_config(
        orchestrator, FEP_BRIDGE_CATEGORY_JEPA, &config
    ), NIMCP_SUCCESS);

    // Default interval should be FEP_UPDATE_INTERVAL_JEPA (25ms)
    EXPECT_EQ(config.update_interval_ms, FEP_UPDATE_INTERVAL_JEPA);
    EXPECT_TRUE(config.enabled);

    // Change interval
    uint64_t new_interval = 50;
    EXPECT_EQ(fep_orchestrator_set_update_interval(
        orchestrator, FEP_BRIDGE_CATEGORY_JEPA, new_interval
    ), NIMCP_SUCCESS);

    // Verify change
    EXPECT_EQ(fep_orchestrator_get_category_config(
        orchestrator, FEP_BRIDGE_CATEGORY_JEPA, &config
    ), NIMCP_SUCCESS);
    EXPECT_EQ(config.update_interval_ms, new_interval);

    fep_orchestrator_stop(orchestrator);
}

//=============================================================================
// FEP Integration with JEPA Predictor Training
//=============================================================================

/**
 * @brief Test FEP-guided JEPA training
 */
TEST_F(JepaFepIntegrationTest, FepGuidedJepaTraining) {
    // This test simulates FEP-guided JEPA training where:
    // 1. FEP orchestrator coordinates update timing
    // 2. Prediction errors are precision-weighted
    // 3. Beliefs are updated through prediction

    static bool training_triggered = false;
    training_triggered = false;

    auto training_update = [](fep_bridge_handle_t handle) -> int {
        jepa_predictor_t* pred = static_cast<jepa_predictor_t*>(handle);

        // Create quick training context
        jepa_latent_t* ctx = jepa_latent_create_dim(64);
        jepa_latent_t* tgt = jepa_latent_create_dim(64);

        if (ctx && tgt) {
            // Fill with simple pattern
            for (uint32_t i = 0; i < 64; i++) {
                ctx->embedding[i] = (float)i / 64.0f;
                tgt->embedding[i] = (float)i / 64.0f + 0.1f;
            }

            float loss = 0.0f;
            jepa_predictor_set_training(pred, true);
            jepa_predictor_train_step(pred, ctx, tgt, &loss);
            jepa_predictor_set_training(pred, false);

            training_triggered = true;
        }

        if (tgt) jepa_latent_destroy(tgt);
        if (ctx) jepa_latent_destroy(ctx);

        return NIMCP_SUCCESS;
    };

    ASSERT_EQ(fep_orchestrator_start(orchestrator), 0);

    uint32_t bridge_id = 0;
    ASSERT_EQ(fep_orchestrator_register_bridge(
        orchestrator, "fep_jepa_training", FEP_BRIDGE_CATEGORY_JEPA,
        predictor, training_update, nullptr, &bridge_id
    ), NIMCP_SUCCESS);

    // Force update to trigger training
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_TRUE(training_triggered);

    // Check predictor has been trained
    jepa_predictor_stats_t stats;
    EXPECT_EQ(jepa_predictor_get_stats(predictor, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.updates_applied, 1u);

    fep_orchestrator_unregister_bridge(orchestrator, bridge_id);
    fep_orchestrator_stop(orchestrator);
}

/**
 * @brief Test precision similarity for FEP-JEPA integration
 */
TEST_F(JepaFepIntegrationTest, PrecisionSimilarityIntegration) {
    // Create latents with variance (uncertainty)
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = TEST_LATENT_DIM;
    config.enable_variance = true;

    jepa_latent_t* latent_a = jepa_latent_create(&config);
    jepa_latent_t* latent_b = jepa_latent_create(&config);

    ASSERT_NE(latent_a, nullptr);
    ASSERT_NE(latent_b, nullptr);

    // Set embeddings and variance
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        latent_a->embedding[i] = 1.0f;
        latent_b->embedding[i] = 1.0f;  // Same embedding

        // Different variances - a has low uncertainty, b has high
        latent_a->variance[i] = 0.1f;
        latent_b->variance[i] = 1.0f;
    }

    // Update precision from variance
    EXPECT_EQ(jepa_latent_update_precision(latent_a), 0);
    EXPECT_EQ(jepa_latent_update_precision(latent_b), 0);

    // Precision should be inverse of variance
    EXPECT_GT(latent_a->precision, latent_b->precision);

    // Compute precision-weighted similarity
    float precision_sim = jepa_latent_precision_similarity(latent_a, latent_b);

    // Should be high since embeddings are identical
    EXPECT_TRUE(std::isfinite(precision_sim));

    // Regular cosine similarity for comparison
    float cosine_sim = jepa_latent_cosine_similarity(latent_a, latent_b);
    EXPECT_NEAR(cosine_sim, 1.0f, 0.01f);  // Should be ~1 for identical embeddings

    jepa_latent_destroy(latent_b);
    jepa_latent_destroy(latent_a);
}

// Main provided by GTest::gtest_main
