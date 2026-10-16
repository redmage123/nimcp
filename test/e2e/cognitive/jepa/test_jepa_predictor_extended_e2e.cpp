/**
 * @file test_jepa_predictor_extended_e2e.cpp
 * @brief End-to-end tests for JEPA TRANSFORMER and RECURRENT predictor types
 *
 * WHAT: Full pipeline tests for extended predictor types: create, train over
 *       multiple steps, verify loss reduction, and test weight save/restore
 *       producing identical predictions
 * WHY:  E2E tests verify the complete user-facing workflow works correctly,
 *       from initialization through training to weight persistence
 * HOW:  Run realistic training pipelines of 50 steps, verify loss < initial,
 *       and test that saving/restoring weights on a fresh predictor yields
 *       bit-identical predictions
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

static const uint32_t E2E_DIM = 32;
static const uint32_t E2E_HIDDEN_DIM = 64;
static const uint32_t E2E_NUM_LAYERS = 2;
static const int E2E_TRAIN_STEPS = 50;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class JepaPredictorExtendedE2ETest : public ::testing::Test {
protected:
    jepa_predictor_config_t config_;

    void SetUp() override {
        int result = jepa_predictor_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        config_.input_dim = E2E_DIM;
        config_.output_dim = E2E_DIM;
        config_.hidden_dim = E2E_HIDDEN_DIM;
        config_.num_layers = E2E_NUM_LAYERS;
        config_.learning_rate = 0.001f;
        config_.activation = JEPA_ACT_GELU;
        config_.loss_type = JEPA_LOSS_MSE;
        config_.enable_fep = false;
    }

    jepa_latent_t* create_test_latent(uint32_t dim, float base = 0.1f) {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = base + sinf(static_cast<float>(i) * 0.3f) * 0.2f;
            }
        }
        return latent;
    }

    jepa_predictor_t* create_typed_predictor(jepa_predictor_type_t type) {
        config_.type = type;
        return jepa_predictor_create(&config_);
    }
};

/* ============================================================================
 * Full TRANSFORMER Pipeline
 * ============================================================================ */

TEST_F(JepaPredictorExtendedE2ETest, FullTransformerPipeline) {
    // WHAT: Full TRANSFORMER pipeline: create -> train 50 steps -> verify loss < initial
    // WHY:  Verify the end-to-end experience works for TRANSFORMER predictor
    // HOW:  Create predictor, create context/target data, train for 50 steps,
    //       record initial and final loss, verify final < initial

    // Step 1: Create TRANSFORMER predictor
    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr) << "Failed to create TRANSFORMER predictor";
    EXPECT_EQ(pred->type, JEPA_PREDICTOR_TRANSFORMER);

    // Step 2: Create training data
    jepa_latent_t* context = create_test_latent(E2E_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(E2E_DIM, 0.4f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Step 3: Measure initial loss (before training)
    jepa_latent_t* pred_out = jepa_latent_create_dim(E2E_DIM);
    ASSERT_NE(pred_out, nullptr);

    int result = jepa_predictor_predict(pred, context, pred_out);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    float initial_loss = jepa_predictor_compute_loss(pred, pred_out, target);
    ASSERT_FALSE(std::isnan(initial_loss));
    ASSERT_GE(initial_loss, 0.0f);
    jepa_latent_destroy(pred_out);

    // Step 4: Train for 50 steps
    std::vector<float> loss_history;
    for (int step = 0; step < E2E_TRAIN_STEPS; step++) {
        float loss = 0.0f;
        result = jepa_predictor_train_step(pred, context, target, &loss);
        ASSERT_EQ(result, NIMCP_SUCCESS)
            << "Train step failed at step " << step;
        ASSERT_FALSE(std::isnan(loss))
            << "NaN loss at step " << step;
        ASSERT_FALSE(std::isinf(loss))
            << "Inf loss at step " << step;
        loss_history.push_back(loss);
    }

    // Step 5: Measure final loss
    pred_out = jepa_latent_create_dim(E2E_DIM);
    ASSERT_NE(pred_out, nullptr);

    result = jepa_predictor_predict(pred, context, pred_out);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    float final_loss = jepa_predictor_compute_loss(pred, pred_out, target);
    ASSERT_FALSE(std::isnan(final_loss));

    // Step 6: Verify loss decreased
    EXPECT_LT(final_loss, initial_loss)
        << "TRANSFORMER training should reduce loss. "
        << "Initial: " << initial_loss << ", Final: " << final_loss;

    // Step 7: Verify statistics
    jepa_predictor_stats_t stats;
    result = jepa_predictor_get_stats(pred, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(stats.predictions_made, static_cast<uint64_t>(E2E_TRAIN_STEPS));

    jepa_latent_destroy(pred_out);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * Full RECURRENT Pipeline
 * ============================================================================ */

TEST_F(JepaPredictorExtendedE2ETest, FullRecurrentPipeline) {
    // WHAT: Full RECURRENT pipeline: create -> train 50 steps -> verify loss < initial
    // WHY:  Verify the end-to-end experience works for RECURRENT predictor
    // HOW:  Same pipeline as TRANSFORMER, exercising RECURRENT backward path

    // Step 1: Create RECURRENT predictor
    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr) << "Failed to create RECURRENT predictor";
    EXPECT_EQ(pred->type, JEPA_PREDICTOR_RECURRENT);

    // Step 2: Create training data
    jepa_latent_t* context = create_test_latent(E2E_DIM, 0.15f);
    jepa_latent_t* target = create_test_latent(E2E_DIM, 0.45f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Step 3: Measure initial loss
    jepa_latent_t* pred_out = jepa_latent_create_dim(E2E_DIM);
    ASSERT_NE(pred_out, nullptr);

    int result = jepa_predictor_predict(pred, context, pred_out);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    float initial_loss = jepa_predictor_compute_loss(pred, pred_out, target);
    ASSERT_FALSE(std::isnan(initial_loss));
    ASSERT_GE(initial_loss, 0.0f);
    jepa_latent_destroy(pred_out);

    // Step 4: Train for 50 steps
    std::vector<float> loss_history;
    for (int step = 0; step < E2E_TRAIN_STEPS; step++) {
        float loss = 0.0f;
        result = jepa_predictor_train_step(pred, context, target, &loss);
        ASSERT_EQ(result, NIMCP_SUCCESS)
            << "Train step failed at step " << step;
        ASSERT_FALSE(std::isnan(loss))
            << "NaN loss at step " << step;
        ASSERT_FALSE(std::isinf(loss))
            << "Inf loss at step " << step;
        loss_history.push_back(loss);
    }

    // Step 5: Measure final loss
    pred_out = jepa_latent_create_dim(E2E_DIM);
    ASSERT_NE(pred_out, nullptr);

    result = jepa_predictor_predict(pred, context, pred_out);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    float final_loss = jepa_predictor_compute_loss(pred, pred_out, target);
    ASSERT_FALSE(std::isnan(final_loss));

    // Step 6: Verify loss decreased
    EXPECT_LT(final_loss, initial_loss)
        << "RECURRENT training should reduce loss. "
        << "Initial: " << initial_loss << ", Final: " << final_loss;

    // Step 7: Verify statistics
    jepa_predictor_stats_t stats;
    result = jepa_predictor_get_stats(pred, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(stats.predictions_made, static_cast<uint64_t>(E2E_TRAIN_STEPS));

    jepa_latent_destroy(pred_out);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * TRANSFORMER Weight Save/Restore Pipeline
 * ============================================================================ */

TEST_F(JepaPredictorExtendedE2ETest, TransformerSaveRestoreWeightsVerifyPredictions) {
    // WHAT: Train TRANSFORMER, save weights, create new predictor, set weights,
    //       verify same predictions
    // WHY:  Verify weight persistence produces identical behavior, essential for
    //       model checkpointing and deployment
    // HOW:  Train predictor, save all layer weights, create fresh predictor,
    //       restore weights, compare predictions on multiple inputs

    // Step 1: Create and train a TRANSFORMER predictor
    jepa_predictor_t* trained = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(trained, nullptr);

    jepa_latent_t* train_ctx = create_test_latent(E2E_DIM, 0.1f);
    jepa_latent_t* train_tgt = create_test_latent(E2E_DIM, 0.4f);
    ASSERT_NE(train_ctx, nullptr);
    ASSERT_NE(train_tgt, nullptr);

    for (int step = 0; step < E2E_TRAIN_STEPS; step++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(trained, train_ctx, train_tgt, &loss);
        ASSERT_EQ(result, NIMCP_SUCCESS);
    }

    // Step 2: Save all weights AND biases from trained predictor
    // Note: The public API (get/set_weights) only transfers weight matrices,
    // not bias vectors. For a full checkpoint/restore we must also copy biases
    // by accessing the internal MLP layer structures directly.
    uint32_t num_layers = trained->network.mlp.num_layers;
    std::vector<std::vector<float>> saved_weights(num_layers);
    std::vector<std::vector<float>> saved_biases(num_layers);
    std::vector<uint32_t> saved_in_dims(num_layers);
    std::vector<uint32_t> saved_out_dims(num_layers);

    for (uint32_t l = 0; l < num_layers; l++) {
        float* w_ptr = nullptr;
        uint32_t dims[2] = {0, 0};
        int result = jepa_predictor_get_weights(trained, l, &w_ptr, dims);
        ASSERT_EQ(result, NIMCP_SUCCESS);
        ASSERT_NE(w_ptr, nullptr);

        uint32_t count = dims[0] * dims[1];
        saved_weights[l].assign(w_ptr, w_ptr + count);
        saved_out_dims[l] = dims[0];
        saved_in_dims[l] = dims[1];

        // Save bias vector
        const jepa_mlp_layer_t* layer = &trained->network.mlp.layers[l];
        if (layer->bias) {
            saved_biases[l].assign(layer->bias, layer->bias + layer->out_dim);
        }
    }

    // Step 3: Create a fresh predictor and restore weights + biases
    jepa_predictor_t* restored = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(restored, nullptr);

    for (uint32_t l = 0; l < num_layers; l++) {
        int result = jepa_predictor_set_weights(restored, l,
                                                 saved_weights[l].data(),
                                                 saved_in_dims[l],
                                                 saved_out_dims[l]);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Restore bias vector
        jepa_mlp_layer_t* layer = &restored->network.mlp.layers[l];
        if (layer->bias && !saved_biases[l].empty()) {
            memcpy(layer->bias, saved_biases[l].data(),
                   saved_biases[l].size() * sizeof(float));
        }
    }

    // Step 4: Compare predictions on multiple test inputs
    float test_bases[] = {0.05f, 0.15f, 0.25f, 0.5f, 0.8f};
    for (int t = 0; t < 5; t++) {
        jepa_latent_t* test_input = create_test_latent(E2E_DIM, test_bases[t]);
        jepa_latent_t* output_trained = jepa_latent_create_dim(E2E_DIM);
        jepa_latent_t* output_restored = jepa_latent_create_dim(E2E_DIM);
        ASSERT_NE(test_input, nullptr);
        ASSERT_NE(output_trained, nullptr);
        ASSERT_NE(output_restored, nullptr);

        int result = jepa_predictor_predict(trained, test_input, output_trained);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        result = jepa_predictor_predict(restored, test_input, output_restored);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Predictions must match
        for (uint32_t i = 0; i < E2E_DIM; i++) {
            EXPECT_NEAR(output_trained->embedding[i],
                         output_restored->embedding[i], 1e-5f)
                << "Prediction mismatch at test " << t << " dim " << i
                << " (base=" << test_bases[t] << ")";
        }

        jepa_latent_destroy(test_input);
        jepa_latent_destroy(output_trained);
        jepa_latent_destroy(output_restored);
    }

    // Cleanup
    jepa_latent_destroy(train_ctx);
    jepa_latent_destroy(train_tgt);
    jepa_predictor_destroy(trained);
    jepa_predictor_destroy(restored);
}

} // namespace
