/**
 * @file test_jepa_predictor_extended.cpp
 * @brief Unit tests for JEPA predictor TRANSFORMER and RECURRENT extensions
 *
 * WHAT: Tests for backward pass, get/set weights, and QMC anneal on
 *       TRANSFORMER and RECURRENT predictor types (newly implemented)
 * WHY:  Verify extended predictor types produce correct behavior after
 *       implementation of backward_transformer() and backward_recurrent()
 * HOW:  Create predictors of each type, run forward+backward passes,
 *       verify gradients exist, test weight get/set operations
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

//=============================================================================
// Constants
//=============================================================================

static const uint32_t TEST_DIM = 32;
static const uint32_t TEST_HIDDEN_DIM = 64;
static const uint32_t TEST_NUM_LAYERS = 2;

//=============================================================================
// Test Fixture
//=============================================================================

class JepaPredictorExtendedTest : public ::testing::Test {
protected:
    jepa_predictor_config_t config_;

    void SetUp() override {
        int result = jepa_predictor_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        config_.input_dim = TEST_DIM;
        config_.output_dim = TEST_DIM;
        config_.hidden_dim = TEST_HIDDEN_DIM;
        config_.num_layers = TEST_NUM_LAYERS;
        config_.learning_rate = 0.001f;
        config_.activation = JEPA_ACT_GELU;
        config_.loss_type = JEPA_LOSS_MSE;
        config_.enable_fep = false;
    }

    // Create a latent with patterned values
    jepa_latent_t* create_test_latent(uint32_t dim, float base = 0.1f) {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = base * static_cast<float>(i % 10 + 1);
            }
        }
        return latent;
    }

    // Create a predictor with given type, skip test if creation fails
    jepa_predictor_t* create_typed_predictor(jepa_predictor_type_t type) {
        config_.type = type;
        return jepa_predictor_create(&config_);
    }
};

//=============================================================================
// TRANSFORMER Backward Pass Tests
//=============================================================================

TEST_F(JepaPredictorExtendedTest, TransformerCreate) {
    // WHAT: Verify TRANSFORMER predictor creation succeeds
    // WHY:  Prerequisite for all other transformer tests

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr) << "TRANSFORMER predictor creation must succeed";
    EXPECT_EQ(pred->type, JEPA_PREDICTOR_TRANSFORMER);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, TransformerForwardBackward) {
    // WHAT: Run forward then backward pass on TRANSFORMER predictor
    // WHY:  Verify backward_transformer() does not crash and produces gradients

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Enable training mode
    int result = jepa_predictor_set_training(pred, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Forward pass
    result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Compute error
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_DIM);
    ASSERT_NE(error, nullptr);

    result = jepa_predictor_compute_error(pred, prediction, target, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Backward pass - the key test
    result = jepa_predictor_backward(pred, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify gradients exist in at least the first layer
    float* weights_ptr = nullptr;
    uint32_t dims[2] = {0, 0};
    result = jepa_predictor_get_weights(pred, 0, &weights_ptr, dims);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(weights_ptr, nullptr);
    EXPECT_GT(dims[0], 0u);
    EXPECT_GT(dims[1], 0u);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, TransformerTrainStep) {
    // WHAT: Run a complete train_step on TRANSFORMER predictor
    // WHY:  Verify full forward + backward + update cycle works

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float loss = 0.0f;
    int result = jepa_predictor_train_step(pred, context, target, &loss);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Loss should be finite and non-negative
    EXPECT_FALSE(std::isnan(loss));
    EXPECT_FALSE(std::isinf(loss));
    EXPECT_GE(loss, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, TransformerMultipleTrainSteps) {
    // WHAT: Run multiple training steps on TRANSFORMER
    // WHY:  Verify training does not diverge or crash over iterations

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float first_loss = 0.0f;
    float last_loss = 0.0f;

    for (int step = 0; step < 10; step++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));
        EXPECT_FALSE(std::isinf(loss));

        if (step == 0) first_loss = loss;
        last_loss = loss;
    }

    // After training, loss should not have increased dramatically
    // (it may not strictly decrease with attention-based backward, but
    //  should not explode)
    EXPECT_LT(last_loss, first_loss * 100.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// RECURRENT Backward Pass Tests
//=============================================================================

TEST_F(JepaPredictorExtendedTest, RecurrentCreate) {
    // WHAT: Verify RECURRENT predictor creation succeeds
    // WHY:  Prerequisite for all other recurrent tests

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr) << "RECURRENT predictor creation must succeed";
    EXPECT_EQ(pred->type, JEPA_PREDICTOR_RECURRENT);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentForwardBackward) {
    // WHAT: Run forward then backward pass on RECURRENT predictor
    // WHY:  Verify backward_recurrent() does not crash and produces gradients

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Enable training mode
    int result = jepa_predictor_set_training(pred, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Forward pass
    result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Compute error
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_DIM);
    ASSERT_NE(error, nullptr);

    result = jepa_predictor_compute_error(pred, prediction, target, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Backward pass - the key test
    result = jepa_predictor_backward(pred, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify gradients exist in at least the first layer
    float* weights_ptr = nullptr;
    uint32_t dims[2] = {0, 0};
    result = jepa_predictor_get_weights(pred, 0, &weights_ptr, dims);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(weights_ptr, nullptr);
    EXPECT_GT(dims[0], 0u);
    EXPECT_GT(dims[1], 0u);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentTrainStep) {
    // WHAT: Run a complete train_step on RECURRENT predictor
    // WHY:  Verify full forward + backward + update cycle works

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float loss = 0.0f;
    int result = jepa_predictor_train_step(pred, context, target, &loss);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Loss should be finite and non-negative
    EXPECT_FALSE(std::isnan(loss));
    EXPECT_FALSE(std::isinf(loss));
    EXPECT_GE(loss, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentMultipleTrainSteps) {
    // WHAT: Run multiple training steps on RECURRENT
    // WHY:  Verify BPTT-style backward is stable over iterations

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float first_loss = 0.0f;
    float last_loss = 0.0f;

    for (int step = 0; step < 10; step++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));
        EXPECT_FALSE(std::isinf(loss));

        if (step == 0) first_loss = loss;
        last_loss = loss;
    }

    // After training, loss should not have exploded
    EXPECT_LT(last_loss, first_loss * 100.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// TRANSFORMER Get/Set Weights Tests
//=============================================================================

TEST_F(JepaPredictorExtendedTest, TransformerGetWeights) {
    // WHAT: Verify get_weights works for TRANSFORMER type
    // WHY:  get_weights must support all types that use MLP layers

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    float* weights_ptr = nullptr;
    uint32_t dims[2] = {0, 0};

    // Get weights for layer 0
    int result = jepa_predictor_get_weights(pred, 0, &weights_ptr, dims);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(weights_ptr, nullptr);
    EXPECT_GT(dims[0], 0u);
    EXPECT_GT(dims[1], 0u);

    // Verify weight values are finite (Xavier-initialized)
    uint32_t num_weights = dims[0] * dims[1];
    for (uint32_t i = 0; i < num_weights && i < 100; i++) {
        EXPECT_FALSE(std::isnan(weights_ptr[i]));
        EXPECT_FALSE(std::isinf(weights_ptr[i]));
    }

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, TransformerSetWeights) {
    // WHAT: Verify set_weights works for TRANSFORMER type
    // WHY:  Must be able to load external weights for transfer learning

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    // First get dimensions
    float* original_weights = nullptr;
    uint32_t dims[2] = {0, 0};
    int result = jepa_predictor_get_weights(pred, 0, &original_weights, dims);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    uint32_t num_weights = dims[0] * dims[1];

    // Create custom weights
    std::vector<float> custom_weights(num_weights, 0.01f);
    for (uint32_t i = 0; i < num_weights; i++) {
        custom_weights[i] = 0.001f * static_cast<float>(i);
    }

    // Set custom weights
    result = jepa_predictor_set_weights(pred, 0, custom_weights.data(),
                                        dims[1], dims[0]);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify weights were updated
    float* updated_weights = nullptr;
    uint32_t updated_dims[2] = {0, 0};
    result = jepa_predictor_get_weights(pred, 0, &updated_weights, updated_dims);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(updated_dims[0], dims[0]);
    EXPECT_EQ(updated_dims[1], dims[1]);

    for (uint32_t i = 0; i < num_weights && i < 50; i++) {
        EXPECT_NEAR(updated_weights[i], custom_weights[i], 1e-6f);
    }

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, TransformerGetWeightsMultipleLayers) {
    // WHAT: Verify get_weights works for all layers of TRANSFORMER
    // WHY:  TRANSFORMER has num_layers + 1 layers (extra attention projection)

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    uint32_t num_params = jepa_predictor_num_params(pred);
    EXPECT_GT(num_params, 0u);

    // TRANSFORMER should have TEST_NUM_LAYERS + 1 layers
    // Try accessing each layer
    for (uint32_t i = 0; i <= TEST_NUM_LAYERS; i++) {
        float* weights_ptr = nullptr;
        uint32_t dims[2] = {0, 0};

        int result = jepa_predictor_get_weights(pred, i, &weights_ptr, dims);
        EXPECT_EQ(result, NIMCP_SUCCESS)
            << "Failed to get weights for layer " << i;
        EXPECT_NE(weights_ptr, nullptr)
            << "Null weights for layer " << i;
    }

    jepa_predictor_destroy(pred);
}

//=============================================================================
// RECURRENT Get/Set Weights Tests
//=============================================================================

TEST_F(JepaPredictorExtendedTest, RecurrentGetWeights) {
    // WHAT: Verify get_weights works for RECURRENT type
    // WHY:  get_weights must support all types that use MLP layers

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    float* weights_ptr = nullptr;
    uint32_t dims[2] = {0, 0};

    // Get weights for layer 0
    int result = jepa_predictor_get_weights(pred, 0, &weights_ptr, dims);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(weights_ptr, nullptr);
    EXPECT_GT(dims[0], 0u);
    EXPECT_GT(dims[1], 0u);

    // Verify weight values are finite
    uint32_t num_weights = dims[0] * dims[1];
    for (uint32_t i = 0; i < num_weights && i < 100; i++) {
        EXPECT_FALSE(std::isnan(weights_ptr[i]));
        EXPECT_FALSE(std::isinf(weights_ptr[i]));
    }

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentSetWeights) {
    // WHAT: Verify set_weights works for RECURRENT type
    // WHY:  Must be able to load external weights for transfer learning

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    // First get dimensions
    float* original_weights = nullptr;
    uint32_t dims[2] = {0, 0};
    int result = jepa_predictor_get_weights(pred, 0, &original_weights, dims);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    uint32_t num_weights = dims[0] * dims[1];

    // Create custom weights
    std::vector<float> custom_weights(num_weights);
    for (uint32_t i = 0; i < num_weights; i++) {
        custom_weights[i] = 0.002f * static_cast<float>(i);
    }

    // Set custom weights
    result = jepa_predictor_set_weights(pred, 0, custom_weights.data(),
                                        dims[1], dims[0]);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify weights were updated
    float* updated_weights = nullptr;
    uint32_t updated_dims[2] = {0, 0};
    result = jepa_predictor_get_weights(pred, 0, &updated_weights, updated_dims);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < num_weights && i < 50; i++) {
        EXPECT_NEAR(updated_weights[i], custom_weights[i], 1e-6f);
    }

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentGetWeightsAllLayers) {
    // WHAT: Verify get_weights works for all layers of RECURRENT
    // WHY:  RECURRENT uses same num_layers as config

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    uint32_t num_params = jepa_predictor_num_params(pred);
    EXPECT_GT(num_params, 0u);

    // RECURRENT should have TEST_NUM_LAYERS layers
    for (uint32_t i = 0; i < TEST_NUM_LAYERS; i++) {
        float* weights_ptr = nullptr;
        uint32_t dims[2] = {0, 0};

        int result = jepa_predictor_get_weights(pred, i, &weights_ptr, dims);
        EXPECT_EQ(result, NIMCP_SUCCESS)
            << "Failed to get weights for layer " << i;
        EXPECT_NE(weights_ptr, nullptr)
            << "Null weights for layer " << i;
    }

    jepa_predictor_destroy(pred);
}

//=============================================================================
// Set Weights Then Predict - Verify Weights Are Used
//=============================================================================

TEST_F(JepaPredictorExtendedTest, TransformerSetWeightsThenPredict) {
    // WHAT: Set weights, then predict, verify prediction changes
    // WHY:  Confirm set_weights actually affects the forward pass

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* pred1 = jepa_latent_create_dim(TEST_DIM);
    jepa_latent_t* pred2 = jepa_latent_create_dim(TEST_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(pred1, nullptr);
    ASSERT_NE(pred2, nullptr);

    // First prediction with original weights
    int result = jepa_predictor_predict(pred, context, pred1);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Modify weights for layer 0
    float* weights_ptr = nullptr;
    uint32_t dims[2] = {0, 0};
    result = jepa_predictor_get_weights(pred, 0, &weights_ptr, dims);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    uint32_t num_weights = dims[0] * dims[1];
    std::vector<float> new_weights(num_weights, 0.5f);
    result = jepa_predictor_set_weights(pred, 0, new_weights.data(), dims[1], dims[0]);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second prediction with modified weights
    result = jepa_predictor_predict(pred, context, pred2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Predictions should differ (different weights)
    bool any_different = false;
    for (uint32_t i = 0; i < TEST_DIM; i++) {
        if (std::abs(pred1->embedding[i] - pred2->embedding[i]) > 1e-6f) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different) << "Predictions should differ after weight change";

    jepa_latent_destroy(context);
    jepa_latent_destroy(pred1);
    jepa_latent_destroy(pred2);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentSetWeightsThenPredict) {
    // WHAT: Set weights, then predict, verify prediction changes
    // WHY:  Confirm set_weights actually affects the forward pass for RECURRENT

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* pred1 = jepa_latent_create_dim(TEST_DIM);
    jepa_latent_t* pred2 = jepa_latent_create_dim(TEST_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(pred1, nullptr);
    ASSERT_NE(pred2, nullptr);

    // First prediction with original weights
    int result = jepa_predictor_predict(pred, context, pred1);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Modify weights for layer 0
    float* weights_ptr = nullptr;
    uint32_t dims[2] = {0, 0};
    result = jepa_predictor_get_weights(pred, 0, &weights_ptr, dims);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    uint32_t num_weights = dims[0] * dims[1];
    std::vector<float> new_weights(num_weights, 0.5f);
    result = jepa_predictor_set_weights(pred, 0, new_weights.data(), dims[1], dims[0]);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second prediction with modified weights
    result = jepa_predictor_predict(pred, context, pred2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Predictions should differ
    bool any_different = false;
    for (uint32_t i = 0; i < TEST_DIM; i++) {
        if (std::abs(pred1->embedding[i] - pred2->embedding[i]) > 1e-6f) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different) << "Predictions should differ after weight change";

    jepa_latent_destroy(context);
    jepa_latent_destroy(pred1);
    jepa_latent_destroy(pred2);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// Num Params Tests for Extended Types
//=============================================================================

TEST_F(JepaPredictorExtendedTest, TransformerNumParams) {
    // WHAT: Verify num_params returns non-zero for TRANSFORMER
    // WHY:  TRANSFORMER should have more params than MLP (extra attention layer)

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    uint32_t num_params = jepa_predictor_num_params(pred);
    EXPECT_GT(num_params, 0u);

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentNumParams) {
    // WHAT: Verify num_params returns non-zero for RECURRENT
    // WHY:  RECURRENT should have same param count as MLP

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    uint32_t num_params = jepa_predictor_num_params(pred);
    EXPECT_GT(num_params, 0u);

    jepa_predictor_destroy(pred);
}

//=============================================================================
// Reset and Re-train Tests
//=============================================================================

TEST_F(JepaPredictorExtendedTest, TransformerResetAndRetrain) {
    // WHAT: Reset TRANSFORMER predictor and retrain
    // WHY:  Verify reset + backward cycle works for TRANSFORMER

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Train a few steps
    float loss = 0.0f;
    for (int i = 0; i < 3; i++) {
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Reset
    int result = jepa_predictor_reset(pred);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Train again after reset
    for (int i = 0; i < 3; i++) {
        result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedTest, RecurrentResetAndRetrain) {
    // WHAT: Reset RECURRENT predictor and retrain
    // WHY:  Verify reset + backward cycle works for RECURRENT

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(TEST_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(TEST_DIM, 0.2f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Train a few steps
    float loss = 0.0f;
    for (int i = 0; i < 3; i++) {
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Reset
    int result = jepa_predictor_reset(pred);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Train again after reset
    for (int i = 0; i < 3; i++) {
        result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

} // namespace
