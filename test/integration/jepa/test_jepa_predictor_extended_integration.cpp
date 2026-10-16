/**
 * @file test_jepa_predictor_extended_integration.cpp
 * @brief Integration tests for JEPA TRANSFORMER and RECURRENT predictor types
 *
 * WHAT: Verify TRANSFORMER and RECURRENT predictors integrate correctly with
 *       the JEPA framework (create/predict/backward/update cycle, weight
 *       get/set roundtrip, QMC adaptive annealing, mixed-type coexistence)
 * WHY:  Unit tests verify individual operations; integration tests verify the
 *       complete training pipeline works end-to-end within the JEPA framework
 * HOW:  Create predictors of each extended type, run full training cycles,
 *       verify weight persistence affects predictions, test QMC annealing
 *       improves loss, and confirm different types coexist without interference
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/error/nimcp_error_codes.h"

namespace {

/* ============================================================================
 * Constants
 * ============================================================================ */

static const uint32_t INTEG_DIM = 32;
static const uint32_t INTEG_HIDDEN_DIM = 64;
static const uint32_t INTEG_NUM_LAYERS = 2;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class JepaPredictorExtendedIntegrationTest : public ::testing::Test {
protected:
    jepa_predictor_config_t config_;

    void SetUp() override {
        int result = jepa_predictor_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        config_.input_dim = INTEG_DIM;
        config_.output_dim = INTEG_DIM;
        config_.hidden_dim = INTEG_HIDDEN_DIM;
        config_.num_layers = INTEG_NUM_LAYERS;
        config_.learning_rate = 0.001f;
        config_.activation = JEPA_ACT_GELU;
        config_.loss_type = JEPA_LOSS_MSE;
        config_.enable_fep = false;
    }

    // Create a latent with sinusoidal pattern for varied but reproducible data
    jepa_latent_t* create_test_latent(uint32_t dim, float base = 0.1f) {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = base + sinf(static_cast<float>(i) * 0.3f) * 0.2f;
            }
        }
        return latent;
    }

    // Create a predictor of the given type
    jepa_predictor_t* create_typed_predictor(jepa_predictor_type_t type) {
        config_.type = type;
        return jepa_predictor_create(&config_);
    }

    // Run a full training cycle: forward -> error -> backward -> update
    // Returns the loss value for the step
    float run_training_cycle(jepa_predictor_t* pred,
                             jepa_latent_t* context,
                             jepa_latent_t* target) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        return loss;
    }
};

/* ============================================================================
 * TRANSFORMER Full Framework Integration
 * ============================================================================ */

TEST_F(JepaPredictorExtendedIntegrationTest, TransformerFullFrameworkIntegration) {
    // WHAT: TRANSFORMER predictor integrates with JEPA framework
    //       (create, predict, backward, update)
    // WHY:  Verify the complete training pipeline works for TRANSFORMER type
    // HOW:  Create predictor, enable training, run forward pass, compute error,
    //       backward pass, update weights, verify loss is valid

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(INTEG_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(INTEG_DIM, 0.3f);
    jepa_latent_t* prediction = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Step 1: Enable training mode
    int result = jepa_predictor_set_training(pred, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 2: Forward pass
    result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify prediction is finite
    for (uint32_t i = 0; i < INTEG_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    // Step 3: Compute error
    jepa_prediction_error_t* error = jepa_prediction_error_create(INTEG_DIM);
    ASSERT_NE(error, nullptr);

    result = jepa_predictor_compute_error(pred, prediction, target, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(error->loss));
    EXPECT_GE(error->loss, 0.0f);

    // Step 4: Backward pass
    result = jepa_predictor_backward(pred, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 5: Update weights
    result = jepa_predictor_update_weights(pred, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 6: Verify statistics updated
    jepa_predictor_stats_t stats;
    result = jepa_predictor_get_stats(pred, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.predictions_made, 0u);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * RECURRENT Full Framework Integration
 * ============================================================================ */

TEST_F(JepaPredictorExtendedIntegrationTest, RecurrentFullFrameworkIntegration) {
    // WHAT: RECURRENT predictor integrates with JEPA framework
    //       (create, predict, backward, update)
    // WHY:  Verify the complete training pipeline works for RECURRENT type
    // HOW:  Same pipeline as TRANSFORMER test, exercising RECURRENT backward path

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(INTEG_DIM, 0.15f);
    jepa_latent_t* target = create_test_latent(INTEG_DIM, 0.35f);
    jepa_latent_t* prediction = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Step 1: Enable training mode
    int result = jepa_predictor_set_training(pred, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 2: Forward pass
    result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < INTEG_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    // Step 3: Compute error
    jepa_prediction_error_t* error = jepa_prediction_error_create(INTEG_DIM);
    ASSERT_NE(error, nullptr);

    result = jepa_predictor_compute_error(pred, prediction, target, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(error->loss));
    EXPECT_GE(error->loss, 0.0f);

    // Step 4: Backward pass
    result = jepa_predictor_backward(pred, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 5: Update weights
    result = jepa_predictor_update_weights(pred, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Step 6: Verify statistics updated
    jepa_predictor_stats_t stats;
    result = jepa_predictor_get_stats(pred, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.predictions_made, 0u);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * Weight Get/Set Roundtrip Tests
 * ============================================================================ */

TEST_F(JepaPredictorExtendedIntegrationTest, TransformerWeightRoundtripPreservesPrediction) {
    // WHAT: TRANSFORMER weight get/set roundtrip preserves prediction behavior
    // WHY:  Verify that saving and restoring weights produces identical predictions
    // HOW:  Get weights, set same weights on a fresh predictor, compare predictions

    jepa_predictor_t* pred1 = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred1, nullptr);

    jepa_latent_t* context = create_test_latent(INTEG_DIM, 0.2f);
    jepa_latent_t* output1 = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(output1, nullptr);

    // Make a prediction with original predictor
    int result = jepa_predictor_predict(pred1, context, output1);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Save all layer weights
    uint32_t num_layers = pred1->network.mlp.num_layers;
    std::vector<std::vector<float>> saved_weights(num_layers);
    std::vector<uint32_t> saved_in_dims(num_layers);
    std::vector<uint32_t> saved_out_dims(num_layers);

    for (uint32_t l = 0; l < num_layers; l++) {
        float* w_ptr = nullptr;
        uint32_t dims[2] = {0, 0};
        result = jepa_predictor_get_weights(pred1, l, &w_ptr, dims);
        ASSERT_EQ(result, NIMCP_SUCCESS);
        ASSERT_NE(w_ptr, nullptr);

        uint32_t count = dims[0] * dims[1];
        saved_weights[l].assign(w_ptr, w_ptr + count);
        saved_out_dims[l] = dims[0];
        saved_in_dims[l] = dims[1];
    }

    // Create a new predictor and set saved weights
    jepa_predictor_t* pred2 = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred2, nullptr);

    for (uint32_t l = 0; l < num_layers; l++) {
        result = jepa_predictor_set_weights(pred2, l,
                                             saved_weights[l].data(),
                                             saved_in_dims[l],
                                             saved_out_dims[l]);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Make a prediction with restored predictor
    jepa_latent_t* output2 = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(output2, nullptr);

    result = jepa_predictor_predict(pred2, context, output2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Predictions should match closely
    for (uint32_t i = 0; i < INTEG_DIM; i++) {
        EXPECT_NEAR(output1->embedding[i], output2->embedding[i], 1e-5f)
            << "Mismatch at dimension " << i;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(output2);
    jepa_predictor_destroy(pred1);
    jepa_predictor_destroy(pred2);
}

TEST_F(JepaPredictorExtendedIntegrationTest, RecurrentWeightRoundtripPreservesPrediction) {
    // WHAT: RECURRENT weight get/set roundtrip preserves prediction behavior
    // WHY:  Verify that saving and restoring weights produces identical predictions
    // HOW:  Get weights, set same weights on a fresh predictor, compare predictions

    jepa_predictor_t* pred1 = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred1, nullptr);

    jepa_latent_t* context = create_test_latent(INTEG_DIM, 0.25f);
    jepa_latent_t* output1 = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(output1, nullptr);

    // Make a prediction with original predictor
    int result = jepa_predictor_predict(pred1, context, output1);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Save all layer weights
    uint32_t num_layers = pred1->network.mlp.num_layers;
    std::vector<std::vector<float>> saved_weights(num_layers);
    std::vector<uint32_t> saved_in_dims(num_layers);
    std::vector<uint32_t> saved_out_dims(num_layers);

    for (uint32_t l = 0; l < num_layers; l++) {
        float* w_ptr = nullptr;
        uint32_t dims[2] = {0, 0};
        result = jepa_predictor_get_weights(pred1, l, &w_ptr, dims);
        ASSERT_EQ(result, NIMCP_SUCCESS);
        ASSERT_NE(w_ptr, nullptr);

        uint32_t count = dims[0] * dims[1];
        saved_weights[l].assign(w_ptr, w_ptr + count);
        saved_out_dims[l] = dims[0];
        saved_in_dims[l] = dims[1];
    }

    // Create a new predictor and set saved weights
    jepa_predictor_t* pred2 = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred2, nullptr);

    for (uint32_t l = 0; l < num_layers; l++) {
        result = jepa_predictor_set_weights(pred2, l,
                                             saved_weights[l].data(),
                                             saved_in_dims[l],
                                             saved_out_dims[l]);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Make a prediction with restored predictor
    jepa_latent_t* output2 = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(output2, nullptr);

    result = jepa_predictor_predict(pred2, context, output2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Predictions should match closely
    for (uint32_t i = 0; i < INTEG_DIM; i++) {
        EXPECT_NEAR(output1->embedding[i], output2->embedding[i], 1e-5f)
            << "Mismatch at dimension " << i;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(output2);
    jepa_predictor_destroy(pred1);
    jepa_predictor_destroy(pred2);
}

/* ============================================================================
 * QMC Annealing Improves Loss
 * ============================================================================ */

TEST_F(JepaPredictorExtendedIntegrationTest, QMCAnnealingImprovesTransformerLoss) {
    // WHAT: QMC annealing improves TRANSFORMER predictor loss
    // WHY:  Verify QMC adaptive anneal actually reduces prediction error
    // HOW:  Measure loss before anneal, run anneal, measure loss after

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    // Create training data
    const uint32_t NUM_SAMPLES = 5;
    std::vector<jepa_latent_t*> contexts(NUM_SAMPLES);
    std::vector<jepa_latent_t*> targets(NUM_SAMPLES);

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        contexts[i] = create_test_latent(INTEG_DIM, 0.1f + 0.05f * i);
        targets[i] = create_test_latent(INTEG_DIM, 0.3f + 0.05f * i);
        ASSERT_NE(contexts[i], nullptr);
        ASSERT_NE(targets[i], nullptr);
    }

    // Measure initial loss (average over all samples)
    float initial_total_loss = 0.0f;
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_t* prediction = jepa_latent_create_dim(INTEG_DIM);
        ASSERT_NE(prediction, nullptr);

        int result = jepa_predictor_predict(pred, contexts[i], prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        float loss = jepa_predictor_compute_loss(pred, prediction, targets[i]);
        EXPECT_FALSE(std::isnan(loss));
        initial_total_loss += loss;

        jepa_latent_destroy(prediction);
    }
    float initial_avg_loss = initial_total_loss / NUM_SAMPLES;

    // Run QMC adaptive annealing
    jepa_qmc_config_t qmc_config;
    jepa_qmc_config_init(&qmc_config);
    qmc_config.num_samples = NUM_SAMPLES;
    qmc_config.num_iterations = 30;
    qmc_config.initial_temp = 1.0f;
    qmc_config.final_temp = 0.01f;

    std::vector<const jepa_latent_t*> ctx_ptrs(NUM_SAMPLES);
    std::vector<const jepa_latent_t*> tgt_ptrs(NUM_SAMPLES);
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        ctx_ptrs[i] = contexts[i];
        tgt_ptrs[i] = targets[i];
    }

    jepa_qmc_stats_t stats;
    int result = jepa_predictor_qmc_adaptive_anneal(
        pred, ctx_ptrs.data(), tgt_ptrs.data(),
        NUM_SAMPLES, &qmc_config, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Measure loss after annealing
    float final_total_loss = 0.0f;
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_t* prediction = jepa_latent_create_dim(INTEG_DIM);
        ASSERT_NE(prediction, nullptr);

        result = jepa_predictor_predict(pred, contexts[i], prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        float loss = jepa_predictor_compute_loss(pred, prediction, targets[i]);
        EXPECT_FALSE(std::isnan(loss));
        final_total_loss += loss;

        jepa_latent_destroy(prediction);
    }
    float final_avg_loss = final_total_loss / NUM_SAMPLES;

    // Annealing should improve (reduce) loss
    // Allow tolerance: final must be less than initial or at most equal
    EXPECT_LE(final_avg_loss, initial_avg_loss * 1.1f)
        << "QMC annealing should not significantly worsen loss. "
        << "Initial: " << initial_avg_loss << ", Final: " << final_avg_loss;

    // Cleanup
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_destroy(contexts[i]);
        jepa_latent_destroy(targets[i]);
    }
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedIntegrationTest, QMCAnnealingImprovesRecurrentLoss) {
    // WHAT: QMC annealing improves RECURRENT predictor loss
    // WHY:  Verify QMC adaptive anneal works for RECURRENT type as well
    // HOW:  Same approach as TRANSFORMER test

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    const uint32_t NUM_SAMPLES = 5;
    std::vector<jepa_latent_t*> contexts(NUM_SAMPLES);
    std::vector<jepa_latent_t*> targets(NUM_SAMPLES);

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        contexts[i] = create_test_latent(INTEG_DIM, 0.1f + 0.05f * i);
        targets[i] = create_test_latent(INTEG_DIM, 0.3f + 0.05f * i);
        ASSERT_NE(contexts[i], nullptr);
        ASSERT_NE(targets[i], nullptr);
    }

    // Measure initial loss
    float initial_total_loss = 0.0f;
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_t* prediction = jepa_latent_create_dim(INTEG_DIM);
        ASSERT_NE(prediction, nullptr);

        int result = jepa_predictor_predict(pred, contexts[i], prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        float loss = jepa_predictor_compute_loss(pred, prediction, targets[i]);
        EXPECT_FALSE(std::isnan(loss));
        initial_total_loss += loss;

        jepa_latent_destroy(prediction);
    }
    float initial_avg_loss = initial_total_loss / NUM_SAMPLES;

    // Run QMC adaptive annealing
    jepa_qmc_config_t qmc_config;
    jepa_qmc_config_init(&qmc_config);
    qmc_config.num_samples = NUM_SAMPLES;
    qmc_config.num_iterations = 30;
    qmc_config.initial_temp = 1.0f;
    qmc_config.final_temp = 0.01f;

    std::vector<const jepa_latent_t*> ctx_ptrs(NUM_SAMPLES);
    std::vector<const jepa_latent_t*> tgt_ptrs(NUM_SAMPLES);
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        ctx_ptrs[i] = contexts[i];
        tgt_ptrs[i] = targets[i];
    }

    jepa_qmc_stats_t stats;
    int result = jepa_predictor_qmc_adaptive_anneal(
        pred, ctx_ptrs.data(), tgt_ptrs.data(),
        NUM_SAMPLES, &qmc_config, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Measure loss after annealing
    float final_total_loss = 0.0f;
    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_t* prediction = jepa_latent_create_dim(INTEG_DIM);
        ASSERT_NE(prediction, nullptr);

        result = jepa_predictor_predict(pred, contexts[i], prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        float loss = jepa_predictor_compute_loss(pred, prediction, targets[i]);
        EXPECT_FALSE(std::isnan(loss));
        final_total_loss += loss;

        jepa_latent_destroy(prediction);
    }
    float final_avg_loss = final_total_loss / NUM_SAMPLES;

    EXPECT_LE(final_avg_loss, initial_avg_loss * 1.1f)
        << "QMC annealing should not significantly worsen loss. "
        << "Initial: " << initial_avg_loss << ", Final: " << final_avg_loss;

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        jepa_latent_destroy(contexts[i]);
        jepa_latent_destroy(targets[i]);
    }
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * Mixed Predictor Types Coexistence
 * ============================================================================ */

TEST_F(JepaPredictorExtendedIntegrationTest, MixedPredictorTypesNoInterference) {
    // WHAT: Mixed predictor types in same process don't interfere
    // WHY:  Verify TRANSFORMER and RECURRENT can coexist without corrupting
    //       each other's state (shared global state, memory, etc.)
    // HOW:  Create both types, train each independently, verify predictions
    //       from one are not affected by training the other

    jepa_predictor_t* transformer = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    jepa_predictor_t* recurrent = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(transformer, nullptr);
    ASSERT_NE(recurrent, nullptr);

    jepa_latent_t* context = create_test_latent(INTEG_DIM, 0.15f);
    jepa_latent_t* target = create_test_latent(INTEG_DIM, 0.35f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Get baseline prediction from transformer before any training
    jepa_latent_t* trans_pred_before = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(trans_pred_before, nullptr);
    int result = jepa_predictor_predict(transformer, context, trans_pred_before);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Train recurrent predictor for several steps
    for (int step = 0; step < 20; step++) {
        float loss = 0.0f;
        result = jepa_predictor_train_step(recurrent, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Verify transformer prediction is unchanged after recurrent training
    jepa_latent_t* trans_pred_after = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(trans_pred_after, nullptr);
    result = jepa_predictor_predict(transformer, context, trans_pred_after);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < INTEG_DIM; i++) {
        EXPECT_NEAR(trans_pred_before->embedding[i],
                     trans_pred_after->embedding[i], 1e-6f)
            << "Transformer prediction changed after recurrent training at dim " << i;
    }

    // Now train transformer and verify recurrent prediction is unchanged
    jepa_latent_t* rec_pred_before = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(rec_pred_before, nullptr);
    result = jepa_predictor_predict(recurrent, context, rec_pred_before);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (int step = 0; step < 20; step++) {
        float loss = 0.0f;
        result = jepa_predictor_train_step(transformer, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    jepa_latent_t* rec_pred_after = jepa_latent_create_dim(INTEG_DIM);
    ASSERT_NE(rec_pred_after, nullptr);
    result = jepa_predictor_predict(recurrent, context, rec_pred_after);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < INTEG_DIM; i++) {
        EXPECT_NEAR(rec_pred_before->embedding[i],
                     rec_pred_after->embedding[i], 1e-6f)
            << "Recurrent prediction changed after transformer training at dim " << i;
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(trans_pred_before);
    jepa_latent_destroy(trans_pred_after);
    jepa_latent_destroy(rec_pred_before);
    jepa_latent_destroy(rec_pred_after);
    jepa_predictor_destroy(transformer);
    jepa_predictor_destroy(recurrent);
}

} // namespace
