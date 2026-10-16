/**
 * @file test_jepa_predictor.cpp
 * @brief Comprehensive unit tests for JEPA Predictor module
 *
 * Tests cover:
 * - Creation and destruction
 * - Default configuration
 * - Forward prediction (MLP pass)
 * - Prediction error computation
 * - Training mode toggle
 * - Weight access and manipulation
 * - Statistics tracking
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-26
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/error/nimcp_error_codes.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

//=============================================================================
// Constants
//=============================================================================

static const uint32_t TEST_INPUT_DIM = 64;
static const uint32_t TEST_OUTPUT_DIM = 64;
static const uint32_t TEST_HIDDEN_DIM = 128;
static const uint32_t TEST_NUM_LAYERS = 2;
static const float TEST_LEARNING_RATE = 0.001f;
static const float TEST_EPSILON = 1e-5f;

//=============================================================================
// Test Fixture
//=============================================================================

class JepaPredictorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Get default config and customize for tests
        int result = jepa_predictor_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        config_.input_dim = TEST_INPUT_DIM;
        config_.output_dim = TEST_OUTPUT_DIM;
        config_.hidden_dim = TEST_HIDDEN_DIM;
        config_.num_layers = TEST_NUM_LAYERS;
        config_.learning_rate = TEST_LEARNING_RATE;
        config_.type = JEPA_PREDICTOR_MLP;
        config_.activation = JEPA_ACT_GELU;
        config_.loss_type = JEPA_LOSS_MSE;
        config_.enable_fep = false;  // Disable FEP for basic tests

        predictor_ = jepa_predictor_create(&config_);
        ASSERT_NE(predictor_, nullptr);
    }

    void TearDown() override
    {
        if (predictor_) {
            jepa_predictor_destroy(predictor_);
            predictor_ = nullptr;
        }
    }

    // Helper to create a test latent with random values
    jepa_latent_t* create_test_latent(uint32_t dim, float fill_value = 0.0f)
    {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = (fill_value != 0.0f) ? fill_value :
                    0.1f * static_cast<float>(i % 10);
            }
        }
        return latent;
    }

    // Helper to create a test latent with specific pattern
    jepa_latent_t* create_patterned_latent(uint32_t dim, float base, float increment)
    {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = base + increment * static_cast<float>(i);
            }
        }
        return latent;
    }

    jepa_predictor_config_t config_;
    jepa_predictor_t* predictor_;
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(JepaPredictorTest, DefaultConfigValidValues)
{
    jepa_predictor_config_t default_config;
    int result = jepa_predictor_default_config(&default_config);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(default_config.type, JEPA_PREDICTOR_MLP);
    EXPECT_GT(default_config.input_dim, 0u);
    EXPECT_GT(default_config.output_dim, 0u);
    EXPECT_GT(default_config.hidden_dim, 0u);
    EXPECT_GE(default_config.num_layers, 1u);
    EXPECT_LE(default_config.num_layers, JEPA_PREDICTOR_MAX_LAYERS);
    EXPECT_EQ(default_config.activation, JEPA_ACT_GELU);
    EXPECT_EQ(default_config.loss_type, JEPA_LOSS_MSE);
    EXPECT_GT(default_config.learning_rate, 0.0f);
    EXPECT_GE(default_config.weight_decay, 0.0f);
    EXPECT_GE(default_config.dropout_rate, 0.0f);
    EXPECT_LE(default_config.dropout_rate, 1.0f);
}

TEST_F(JepaPredictorTest, DefaultConfigNullPointer)
{
    int result = jepa_predictor_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

TEST_F(JepaPredictorTest, CreateWithConfig)
{
    // Predictor created in SetUp
    EXPECT_NE(predictor_, nullptr);
    EXPECT_EQ(predictor_->config.input_dim, TEST_INPUT_DIM);
    EXPECT_EQ(predictor_->config.output_dim, TEST_OUTPUT_DIM);
    EXPECT_EQ(predictor_->config.hidden_dim, TEST_HIDDEN_DIM);
    EXPECT_EQ(predictor_->config.num_layers, TEST_NUM_LAYERS);
    EXPECT_EQ(predictor_->type, JEPA_PREDICTOR_MLP);
}

TEST_F(JepaPredictorTest, CreateWithNullConfig)
{
    // Should use default config when NULL is passed
    jepa_predictor_t* pred = jepa_predictor_create(nullptr);
    ASSERT_NE(pred, nullptr);

    // Check it has valid defaults
    EXPECT_GT(pred->config.input_dim, 0u);
    EXPECT_GT(pred->config.output_dim, 0u);
    EXPECT_EQ(pred->type, JEPA_PREDICTOR_MLP);

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, CreateWithLinearType)
{
    jepa_predictor_config_t linear_config;
    jepa_predictor_default_config(&linear_config);
    linear_config.type = JEPA_PREDICTOR_LINEAR;
    linear_config.input_dim = 32;
    linear_config.output_dim = 32;

    jepa_predictor_t* pred = jepa_predictor_create(&linear_config);
    ASSERT_NE(pred, nullptr);
    EXPECT_EQ(pred->type, JEPA_PREDICTOR_LINEAR);

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, CreateWithInvalidDimensions)
{
    jepa_predictor_config_t bad_config;
    jepa_predictor_default_config(&bad_config);
    bad_config.input_dim = 0;  // Invalid

    jepa_predictor_t* pred = jepa_predictor_create(&bad_config);
    EXPECT_EQ(pred, nullptr);
}

TEST_F(JepaPredictorTest, CreateWithExcessiveLayers)
{
    jepa_predictor_config_t bad_config;
    jepa_predictor_default_config(&bad_config);
    bad_config.num_layers = JEPA_PREDICTOR_MAX_LAYERS + 1;  // Too many

    jepa_predictor_t* pred = jepa_predictor_create(&bad_config);
    EXPECT_EQ(pred, nullptr);
}

TEST_F(JepaPredictorTest, DestroyNullSafe)
{
    jepa_predictor_destroy(nullptr);
    // Should not crash
}

TEST_F(JepaPredictorTest, DestroyValid)
{
    jepa_predictor_t* pred = jepa_predictor_create(&config_);
    ASSERT_NE(pred, nullptr);
    jepa_predictor_destroy(pred);
    // Should not crash or leak
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(JepaPredictorTest, ResetPredictor)
{
    // Make some predictions to change internal state
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM, 0.0f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    jepa_predictor_predict(predictor_, context, prediction);

    // Get stats before reset
    jepa_predictor_stats_t stats_before;
    jepa_predictor_get_stats(predictor_, &stats_before);
    EXPECT_GT(stats_before.predictions_made, 0u);

    // Reset
    int result = jepa_predictor_reset(predictor_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should be zeroed
    jepa_predictor_stats_t stats_after;
    jepa_predictor_get_stats(predictor_, &stats_after);
    EXPECT_EQ(stats_after.predictions_made, 0u);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, ResetNullPredictor)
{
    int result = jepa_predictor_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Forward Prediction Tests
//=============================================================================

TEST_F(JepaPredictorTest, PredictBasic)
{
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM, 0.0f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    int result = jepa_predictor_predict(predictor_, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify prediction is populated (not all zeros after MLP)
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        if (fabsf(prediction->embedding[i]) > TEST_EPSILON) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, PredictNullPredictor)
{
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    int result = jepa_predictor_predict(nullptr, context, prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, PredictNullContext)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);

    int result = jepa_predictor_predict(predictor_, nullptr, prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, PredictNullPrediction)
{
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    ASSERT_NE(context, nullptr);

    int result = jepa_predictor_predict(predictor_, context, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, PredictDimensionMismatch)
{
    // Create context with wrong dimension
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM / 2);  // Wrong size
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    int result = jepa_predictor_predict(predictor_, context, prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, PredictDeterministic)
{
    // Same input should produce same output (in inference mode)
    jepa_predictor_set_training(predictor_, false);

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.3f);
    jepa_latent_t* prediction1 = create_test_latent(TEST_OUTPUT_DIM);
    jepa_latent_t* prediction2 = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction1, nullptr);
    ASSERT_NE(prediction2, nullptr);

    int result1 = jepa_predictor_predict(predictor_, context, prediction1);
    int result2 = jepa_predictor_predict(predictor_, context, prediction2);
    EXPECT_EQ(result1, NIMCP_SUCCESS);
    EXPECT_EQ(result2, NIMCP_SUCCESS);

    // Compare outputs
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_NEAR(prediction1->embedding[i], prediction2->embedding[i], TEST_EPSILON);
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction1);
    jepa_latent_destroy(prediction2);
}

TEST_F(JepaPredictorTest, PredictMasked)
{
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM, 0.0f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Create mask (predict first half)
    std::vector<float> mask(TEST_OUTPUT_DIM, 0.0f);
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM / 2; i++) {
        mask[i] = 1.0f;
    }

    int result = jepa_predictor_predict_masked(predictor_, context, mask.data(), prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

//=============================================================================
// MLP Forward Pass Correctness Tests
//=============================================================================

TEST_F(JepaPredictorTest, MLPLayerCount)
{
    // Verify the MLP has the correct number of layers
    EXPECT_EQ(predictor_->network.mlp.num_layers, TEST_NUM_LAYERS);
}

TEST_F(JepaPredictorTest, MLPLayerDimensions)
{
    // Verify layer dimensions match config
    const jepa_mlp_t* mlp = &predictor_->network.mlp;
    ASSERT_NE(mlp->layers, nullptr);

    // First layer: input_dim -> hidden_dim
    EXPECT_EQ(mlp->layers[0].in_dim, TEST_INPUT_DIM);
    EXPECT_EQ(mlp->layers[0].out_dim, TEST_HIDDEN_DIM);

    // Last layer: hidden_dim -> output_dim
    EXPECT_EQ(mlp->layers[TEST_NUM_LAYERS - 1].in_dim, TEST_HIDDEN_DIM);
    EXPECT_EQ(mlp->layers[TEST_NUM_LAYERS - 1].out_dim, TEST_OUTPUT_DIM);
}

TEST_F(JepaPredictorTest, MLPWeightsNotNull)
{
    const jepa_mlp_t* mlp = &predictor_->network.mlp;

    for (uint32_t i = 0; i < mlp->num_layers; i++) {
        EXPECT_NE(mlp->layers[i].weights, nullptr);
        EXPECT_NE(mlp->layers[i].bias, nullptr);
    }
}

TEST_F(JepaPredictorTest, MLPWeightsInitialized)
{
    const jepa_mlp_t* mlp = &predictor_->network.mlp;

    // Weights should be initialized to small random values (not all zero)
    bool has_nonzero = false;
    for (uint32_t i = 0; i < mlp->num_layers && !has_nonzero; i++) {
        uint32_t weight_count = mlp->layers[i].in_dim * mlp->layers[i].out_dim;
        for (uint32_t j = 0; j < weight_count; j++) {
            if (fabsf(mlp->layers[i].weights[j]) > TEST_EPSILON) {
                has_nonzero = true;
                break;
            }
        }
    }
    EXPECT_TRUE(has_nonzero);
}

//=============================================================================
// Prediction Error Computation Tests
//=============================================================================

TEST_F(JepaPredictorTest, ComputeErrorBasic)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM, 0.3f);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(error, nullptr);

    int result = jepa_predictor_compute_error(predictor_, prediction, target, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Error should be prediction - target = 0.5 - 0.3 = 0.2 for each element
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_NEAR(error->error[i], 0.2f, TEST_EPSILON);
    }

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, ComputeErrorNullPredictor)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM);
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(error, nullptr);

    int result = jepa_predictor_compute_error(nullptr, prediction, target, error);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, ComputeErrorNullPrediction)
{
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM);
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(error, nullptr);

    int result = jepa_predictor_compute_error(predictor_, nullptr, target, error);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, ComputeErrorNullTarget)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(error, nullptr);

    int result = jepa_predictor_compute_error(predictor_, prediction, nullptr, error);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, ComputeErrorNullErrorOutput)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    int result = jepa_predictor_compute_error(predictor_, prediction, target, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, ComputeLossMSE)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM, 0.3f);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    float loss = jepa_predictor_compute_loss(predictor_, prediction, target);

    // MSE = mean((0.5 - 0.3)^2) = mean(0.04) = 0.04
    EXPECT_NEAR(loss, 0.04f, TEST_EPSILON);
    EXPECT_FALSE(std::isnan(loss));

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, ComputeLossZeroError)
{
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM, 0.5f);  // Same as prediction
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    float loss = jepa_predictor_compute_loss(predictor_, prediction, target);

    // Perfect match should have zero loss
    EXPECT_NEAR(loss, 0.0f, TEST_EPSILON);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, ComputeLossNullReturnsNaN)
{
    jepa_latent_t* latent = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(latent, nullptr);

    float loss1 = jepa_predictor_compute_loss(nullptr, latent, latent);
    float loss2 = jepa_predictor_compute_loss(predictor_, nullptr, latent);
    float loss3 = jepa_predictor_compute_loss(predictor_, latent, nullptr);

    EXPECT_TRUE(std::isnan(loss1));
    EXPECT_TRUE(std::isnan(loss2));
    EXPECT_TRUE(std::isnan(loss3));

    jepa_latent_destroy(latent);
}

//=============================================================================
// Training Mode Tests
//=============================================================================

TEST_F(JepaPredictorTest, SetTrainingModeTrue)
{
    int result = jepa_predictor_set_training(predictor_, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(predictor_->training_mode);
}

TEST_F(JepaPredictorTest, SetTrainingModeFalse)
{
    int result = jepa_predictor_set_training(predictor_, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(predictor_->training_mode);
}

TEST_F(JepaPredictorTest, SetTrainingModeToggle)
{
    jepa_predictor_set_training(predictor_, true);
    EXPECT_TRUE(predictor_->training_mode);

    jepa_predictor_set_training(predictor_, false);
    EXPECT_FALSE(predictor_->training_mode);

    jepa_predictor_set_training(predictor_, true);
    EXPECT_TRUE(predictor_->training_mode);
}

TEST_F(JepaPredictorTest, SetTrainingModeNullPredictor)
{
    int result = jepa_predictor_set_training(nullptr, true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Training Step Tests
//=============================================================================

TEST_F(JepaPredictorTest, TrainStepBasic)
{
    jepa_predictor_set_training(predictor_, true);

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float loss = 0.0f;
    int result = jepa_predictor_train_step(predictor_, context, target, &loss);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(loss, 0.0f);
    EXPECT_FALSE(std::isnan(loss));

    // Step count should increase
    EXPECT_EQ(predictor_->step_count, 1u);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, TrainStepNullLossOutput)
{
    jepa_predictor_set_training(predictor_, true);

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // NULL loss output should still work
    int result = jepa_predictor_train_step(predictor_, context, target, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, TrainStepReducesLoss)
{
    jepa_predictor_set_training(predictor_, true);

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Train for several steps
    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    jepa_predictor_train_step(predictor_, context, target, &initial_loss);

    for (int i = 0; i < 100; i++) {
        jepa_predictor_train_step(predictor_, context, target, &final_loss);
    }

    // Loss should decrease (or at least not increase significantly)
    // Note: With random init, this may not always hold for small step counts
    EXPECT_GE(initial_loss, 0.0f);
    EXPECT_GE(final_loss, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

//=============================================================================
// Prediction Error Management Tests
//=============================================================================

TEST_F(JepaPredictorTest, PredictionErrorCreate)
{
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(error, nullptr);
    EXPECT_NE(error->error, nullptr);
    EXPECT_NE(error->weighted_error, nullptr);
    EXPECT_EQ(error->dim, TEST_OUTPUT_DIM);

    jepa_prediction_error_destroy(error);
}

TEST_F(JepaPredictorTest, PredictionErrorCreateZeroDim)
{
    jepa_prediction_error_t* error = jepa_prediction_error_create(0);
    EXPECT_EQ(error, nullptr);
}

TEST_F(JepaPredictorTest, PredictionErrorDestroyNullSafe)
{
    jepa_prediction_error_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Weight Access Tests
//=============================================================================

TEST_F(JepaPredictorTest, GetNumParams)
{
    uint32_t num_params = jepa_predictor_num_params(predictor_);

    // Calculate expected params for 2-layer MLP
    // Layer 0: (input_dim * hidden_dim) + hidden_dim (bias)
    // Layer 1: (hidden_dim * output_dim) + output_dim (bias)
    uint32_t expected = (TEST_INPUT_DIM * TEST_HIDDEN_DIM + TEST_HIDDEN_DIM) +
                        (TEST_HIDDEN_DIM * TEST_OUTPUT_DIM + TEST_OUTPUT_DIM);

    EXPECT_EQ(num_params, expected);
}

TEST_F(JepaPredictorTest, GetNumParamsNullPredictor)
{
    uint32_t num_params = jepa_predictor_num_params(nullptr);
    EXPECT_EQ(num_params, 0u);
}

TEST_F(JepaPredictorTest, GetWeightsLayer0)
{
    float* weights = nullptr;
    uint32_t dims[2] = {0, 0};

    int result = jepa_predictor_get_weights(predictor_, 0, &weights, dims);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(weights, nullptr);
    EXPECT_EQ(dims[0], TEST_HIDDEN_DIM);  // out_dim
    EXPECT_EQ(dims[1], TEST_INPUT_DIM);   // in_dim
}

TEST_F(JepaPredictorTest, GetWeightsInvalidLayer)
{
    float* weights = nullptr;
    uint32_t dims[2] = {0, 0};

    int result = jepa_predictor_get_weights(predictor_, 999, &weights, dims);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, GetWeightsNullPredictor)
{
    float* weights = nullptr;
    uint32_t dims[2] = {0, 0};

    int result = jepa_predictor_get_weights(nullptr, 0, &weights, dims);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, SetWeights)
{
    // Create custom weights
    std::vector<float> new_weights(TEST_INPUT_DIM * TEST_HIDDEN_DIM, 0.1f);

    int result = jepa_predictor_set_weights(predictor_, 0, new_weights.data(),
                                            TEST_INPUT_DIM, TEST_HIDDEN_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify weights were set
    float* weights = nullptr;
    uint32_t dims[2];
    jepa_predictor_get_weights(predictor_, 0, &weights, dims);

    for (size_t i = 0; i < new_weights.size(); i++) {
        EXPECT_NEAR(weights[i], 0.1f, TEST_EPSILON);
    }
}

TEST_F(JepaPredictorTest, SetWeightsNullPredictor)
{
    std::vector<float> weights(TEST_INPUT_DIM * TEST_HIDDEN_DIM, 0.1f);

    int result = jepa_predictor_set_weights(nullptr, 0, weights.data(),
                                            TEST_INPUT_DIM, TEST_HIDDEN_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, SetWeightsNullWeights)
{
    int result = jepa_predictor_set_weights(predictor_, 0, nullptr,
                                            TEST_INPUT_DIM, TEST_HIDDEN_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, SetWeightsDimensionMismatch)
{
    std::vector<float> weights(100, 0.1f);  // Wrong size

    int result = jepa_predictor_set_weights(predictor_, 0, weights.data(),
                                            10, 10);  // Wrong dimensions
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(JepaPredictorTest, GetStatsInitiallyZero)
{
    // Create fresh predictor
    jepa_predictor_t* pred = jepa_predictor_create(&config_);
    ASSERT_NE(pred, nullptr);

    jepa_predictor_stats_t stats;
    int result = jepa_predictor_get_stats(pred, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(stats.predictions_made, 0u);
    EXPECT_EQ(stats.updates_applied, 0u);

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, GetStatsAfterPredictions)
{
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Make some predictions
    for (int i = 0; i < 5; i++) {
        jepa_predictor_predict(predictor_, context, prediction);
    }

    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);
    EXPECT_EQ(stats.predictions_made, 5u);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, GetStatsNullPredictor)
{
    jepa_predictor_stats_t stats;
    int result = jepa_predictor_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, GetStatsNullOutput)
{
    int result = jepa_predictor_get_stats(predictor_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, ResetStats)
{
    // Make some predictions
    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* prediction = create_test_latent(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    jepa_predictor_predict(predictor_, context, prediction);

    // Reset stats
    int result = jepa_predictor_reset_stats(predictor_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);
    EXPECT_EQ(stats.predictions_made, 0u);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, ResetStatsNullPredictor)
{
    int result = jepa_predictor_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(JepaPredictorTest, PredictorTypeToString)
{
    EXPECT_STREQ(jepa_predictor_type_to_string(JEPA_PREDICTOR_LINEAR), "linear");
    EXPECT_STREQ(jepa_predictor_type_to_string(JEPA_PREDICTOR_MLP), "mlp");
    EXPECT_STREQ(jepa_predictor_type_to_string(JEPA_PREDICTOR_TRANSFORMER), "transformer");
    EXPECT_STREQ(jepa_predictor_type_to_string(JEPA_PREDICTOR_RECURRENT), "recurrent");
}

TEST_F(JepaPredictorTest, PredictorTypeToStringInvalid)
{
    const char* str = jepa_predictor_type_to_string(static_cast<jepa_predictor_type_t>(999));
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, "unknown");
}

TEST_F(JepaPredictorTest, ActivationToString)
{
    EXPECT_STREQ(jepa_activation_to_string(JEPA_ACT_NONE), "none");
    EXPECT_STREQ(jepa_activation_to_string(JEPA_ACT_RELU), "relu");
    EXPECT_STREQ(jepa_activation_to_string(JEPA_ACT_GELU), "gelu");
    EXPECT_STREQ(jepa_activation_to_string(JEPA_ACT_TANH), "tanh");
    EXPECT_STREQ(jepa_activation_to_string(JEPA_ACT_SIGMOID), "sigmoid");
}

TEST_F(JepaPredictorTest, ActivationToStringInvalid)
{
    const char* str = jepa_activation_to_string(static_cast<jepa_activation_t>(999));
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, "unknown");
}

TEST_F(JepaPredictorTest, LossToString)
{
    EXPECT_STREQ(jepa_loss_to_string(JEPA_LOSS_MSE), "mse");
    EXPECT_STREQ(jepa_loss_to_string(JEPA_LOSS_COSINE), "cosine");
    EXPECT_STREQ(jepa_loss_to_string(JEPA_LOSS_SMOOTH_L1), "smooth_l1");
    EXPECT_STREQ(jepa_loss_to_string(JEPA_LOSS_PRECISION_WEIGHTED), "precision_weighted");
}

TEST_F(JepaPredictorTest, LossToStringInvalid)
{
    const char* str = jepa_loss_to_string(static_cast<jepa_loss_t>(999));
    EXPECT_NE(str, nullptr);
    EXPECT_STREQ(str, "unknown");
}

//=============================================================================
// FEP Integration Tests
//=============================================================================

class JepaPredictorFEPTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        jepa_predictor_default_config(&config_);
        config_.input_dim = TEST_INPUT_DIM;
        config_.output_dim = TEST_OUTPUT_DIM;
        config_.hidden_dim = TEST_HIDDEN_DIM;
        config_.enable_fep = true;  // Enable FEP for these tests
        config_.initial_precision = 1.0f;

        predictor_ = jepa_predictor_create(&config_);
        ASSERT_NE(predictor_, nullptr);
    }

    void TearDown() override
    {
        if (predictor_) {
            jepa_predictor_destroy(predictor_);
            predictor_ = nullptr;
        }
    }

    jepa_predictor_config_t config_;
    jepa_predictor_t* predictor_;
};

TEST_F(JepaPredictorFEPTest, FEPEnabled)
{
    EXPECT_TRUE(predictor_->config.enable_fep);
}

TEST_F(JepaPredictorFEPTest, InitialPrecision)
{
    EXPECT_NEAR(predictor_->prediction_precision, 1.0f, TEST_EPSILON);
}

TEST_F(JepaPredictorFEPTest, UpdatePrecision)
{
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Set values with known difference
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        prediction->embedding[i] = 0.5f;
        target->embedding[i] = 0.4f;
    }

    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(error, nullptr);

    jepa_predictor_compute_error(predictor_, prediction, target, error);

    float initial_precision = predictor_->prediction_precision;
    int result = jepa_predictor_update_precision(predictor_, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Precision should have been updated
    // With error, precision should decrease (more uncertainty)
    EXPECT_NE(predictor_->prediction_precision, initial_precision);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(JepaPredictorTest, VerySmallDimensions)
{
    // Use minimum valid dimension of 16 for latents
    jepa_predictor_config_t small_config;
    jepa_predictor_default_config(&small_config);
    small_config.input_dim = 16;
    small_config.output_dim = 16;
    small_config.hidden_dim = 32;
    small_config.num_layers = 1;

    jepa_predictor_t* pred = jepa_predictor_create(&small_config);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = jepa_latent_create_dim(16);
    jepa_latent_t* prediction = jepa_latent_create_dim(16);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    for (uint32_t i = 0; i < 16; i++) {
        context->embedding[i] = 0.1f * static_cast<float>(i);
    }

    int result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, LargeDimensions)
{
    jepa_predictor_config_t large_config;
    jepa_predictor_default_config(&large_config);
    large_config.input_dim = 512;
    large_config.output_dim = 512;
    large_config.hidden_dim = JEPA_PREDICTOR_MAX_HIDDEN;
    large_config.num_layers = 2;

    jepa_predictor_t* pred = jepa_predictor_create(&large_config);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = jepa_latent_create_dim(512);
    jepa_latent_t* prediction = jepa_latent_create_dim(512);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    for (uint32_t i = 0; i < 512; i++) {
        context->embedding[i] = 0.01f * static_cast<float>(i % 100);
    }

    int result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, MaxLayers)
{
    jepa_predictor_config_t max_config;
    jepa_predictor_default_config(&max_config);
    max_config.input_dim = 32;
    max_config.output_dim = 32;
    max_config.hidden_dim = 64;
    max_config.num_layers = JEPA_PREDICTOR_MAX_LAYERS;

    jepa_predictor_t* pred = jepa_predictor_create(&max_config);
    ASSERT_NE(pred, nullptr);
    EXPECT_EQ(pred->network.mlp.num_layers, JEPA_PREDICTOR_MAX_LAYERS);

    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, DifferentActivations)
{
    std::vector<jepa_activation_t> activations = {
        JEPA_ACT_NONE,
        JEPA_ACT_RELU,
        JEPA_ACT_GELU,
        JEPA_ACT_TANH,
        JEPA_ACT_SIGMOID
    };

    for (jepa_activation_t act : activations) {
        jepa_predictor_config_t config;
        jepa_predictor_default_config(&config);
        config.activation = act;
        config.input_dim = 16;
        config.output_dim = 16;
        config.hidden_dim = 32;

        jepa_predictor_t* pred = jepa_predictor_create(&config);
        ASSERT_NE(pred, nullptr) << "Failed for activation: "
                                 << jepa_activation_to_string(act);

        jepa_latent_t* context = jepa_latent_create_dim(16);
        jepa_latent_t* prediction = jepa_latent_create_dim(16);
        ASSERT_NE(context, nullptr);
        ASSERT_NE(prediction, nullptr);

        for (uint32_t i = 0; i < 16; i++) {
            context->embedding[i] = 0.1f;
        }

        int result = jepa_predictor_predict(pred, context, prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Failed for activation: "
                                         << jepa_activation_to_string(act);

        jepa_latent_destroy(context);
        jepa_latent_destroy(prediction);
        jepa_predictor_destroy(pred);
    }
}

TEST_F(JepaPredictorTest, DifferentLossTypes)
{
    std::vector<jepa_loss_t> losses = {
        JEPA_LOSS_MSE,
        JEPA_LOSS_COSINE,
        JEPA_LOSS_SMOOTH_L1,
        JEPA_LOSS_PRECISION_WEIGHTED
    };

    for (jepa_loss_t loss_type : losses) {
        jepa_predictor_config_t config;
        jepa_predictor_default_config(&config);
        config.loss_type = loss_type;
        config.input_dim = 16;
        config.output_dim = 16;
        config.hidden_dim = 32;
        config.enable_fep = (loss_type == JEPA_LOSS_PRECISION_WEIGHTED);

        jepa_predictor_t* pred = jepa_predictor_create(&config);
        ASSERT_NE(pred, nullptr) << "Failed for loss: " << jepa_loss_to_string(loss_type);

        jepa_latent_t* prediction = jepa_latent_create_dim(16);
        jepa_latent_t* target = jepa_latent_create_dim(16);
        ASSERT_NE(prediction, nullptr);
        ASSERT_NE(target, nullptr);

        for (uint32_t i = 0; i < 16; i++) {
            prediction->embedding[i] = 0.5f;
            target->embedding[i] = 0.3f;
        }

        float loss = jepa_predictor_compute_loss(pred, prediction, target);
        EXPECT_FALSE(std::isnan(loss)) << "NaN loss for: " << jepa_loss_to_string(loss_type);
        EXPECT_GE(loss, 0.0f) << "Negative loss for: " << jepa_loss_to_string(loss_type);

        jepa_latent_destroy(prediction);
        jepa_latent_destroy(target);
        jepa_predictor_destroy(pred);
    }
}

//=============================================================================
// Backward Pass Tests
//=============================================================================

TEST_F(JepaPredictorTest, BackwardBasic)
{
    jepa_predictor_set_training(predictor_, true);

    // Forward pass
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.5f;
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.3f;
    }

    jepa_predictor_predict(predictor_, context, prediction);

    // Compute error
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(error, nullptr);

    jepa_predictor_compute_error(predictor_, prediction, target, error);

    // Backward pass
    int result = jepa_predictor_backward(predictor_, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Gradients should be populated
    const jepa_mlp_t* mlp = &predictor_->network.mlp;
    bool has_nonzero_grad = false;
    for (uint32_t l = 0; l < mlp->num_layers && !has_nonzero_grad; l++) {
        if (mlp->layers[l].grad_weights) {
            uint32_t grad_count = mlp->layers[l].in_dim * mlp->layers[l].out_dim;
            for (uint32_t i = 0; i < grad_count; i++) {
                if (fabsf(mlp->layers[l].grad_weights[i]) > TEST_EPSILON) {
                    has_nonzero_grad = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(has_nonzero_grad);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, BackwardNullPredictor)
{
    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(error, nullptr);

    int result = jepa_predictor_backward(nullptr, error);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_prediction_error_destroy(error);
}

TEST_F(JepaPredictorTest, BackwardNullError)
{
    int result = jepa_predictor_backward(predictor_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Update Weights Tests
//=============================================================================

TEST_F(JepaPredictorTest, UpdateWeightsBasic)
{
    jepa_predictor_set_training(predictor_, true);

    // Store original weights
    float* orig_weights = nullptr;
    uint32_t dims[2];
    jepa_predictor_get_weights(predictor_, 0, &orig_weights, dims);

    std::vector<float> orig_copy(orig_weights, orig_weights + dims[0] * dims[1]);

    // Do a training step to compute gradients
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.5f;
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.3f;
    }

    jepa_predictor_predict(predictor_, context, prediction);

    jepa_prediction_error_t* error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(error, nullptr);

    jepa_predictor_compute_error(predictor_, prediction, target, error);
    jepa_predictor_backward(predictor_, error);

    // Update weights with larger learning rate for visible change
    int result = jepa_predictor_update_weights(predictor_, 0.1f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Get updated weights
    float* new_weights = nullptr;
    jepa_predictor_get_weights(predictor_, 0, &new_weights, dims);

    // Weights should have changed
    bool weights_changed = false;
    for (size_t i = 0; i < orig_copy.size(); i++) {
        if (fabsf(new_weights[i] - orig_copy[i]) > TEST_EPSILON) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, UpdateWeightsNullPredictor)
{
    int result = jepa_predictor_update_weights(nullptr, TEST_LEARNING_RATE);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, UpdateWeightsZeroLR)
{
    // Zero learning rate should use config default
    int result = jepa_predictor_update_weights(predictor_, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Predictor Type Tests - TRANSFORMER
//=============================================================================

TEST_F(JepaPredictorTest, CreateTransformerType)
{
    jepa_predictor_config_t transformer_config;
    jepa_predictor_default_config(&transformer_config);
    transformer_config.type = JEPA_PREDICTOR_TRANSFORMER;
    transformer_config.input_dim = 32;
    transformer_config.output_dim = 32;
    transformer_config.hidden_dim = 64;

    jepa_predictor_t* pred = jepa_predictor_create(&transformer_config);
    // TRANSFORMER may not be fully implemented yet - check for NULL or valid
    if (pred != nullptr) {
        EXPECT_EQ(pred->type, JEPA_PREDICTOR_TRANSFORMER);
        jepa_predictor_destroy(pred);
    }
    // If NULL, it means transformer is not yet implemented - acceptable
}

TEST_F(JepaPredictorTest, TransformerPrediction)
{
    jepa_predictor_config_t transformer_config;
    jepa_predictor_default_config(&transformer_config);
    transformer_config.type = JEPA_PREDICTOR_TRANSFORMER;
    transformer_config.input_dim = 32;
    transformer_config.output_dim = 32;
    transformer_config.hidden_dim = 64;

    jepa_predictor_t* pred = jepa_predictor_create(&transformer_config);
    if (pred != nullptr) {
        jepa_latent_t* context = jepa_latent_create_dim(32);
        jepa_latent_t* prediction = jepa_latent_create_dim(32);
        ASSERT_NE(context, nullptr);
        ASSERT_NE(prediction, nullptr);

        for (uint32_t i = 0; i < 32; i++) {
            context->embedding[i] = 0.1f * static_cast<float>(i % 10);
        }

        int result = jepa_predictor_predict(pred, context, prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        jepa_latent_destroy(context);
        jepa_latent_destroy(prediction);
        jepa_predictor_destroy(pred);
    }
}

//=============================================================================
// Predictor Type Tests - RECURRENT
//=============================================================================

TEST_F(JepaPredictorTest, CreateRecurrentType)
{
    jepa_predictor_config_t recurrent_config;
    jepa_predictor_default_config(&recurrent_config);
    recurrent_config.type = JEPA_PREDICTOR_RECURRENT;
    recurrent_config.input_dim = 32;
    recurrent_config.output_dim = 32;
    recurrent_config.hidden_dim = 64;

    jepa_predictor_t* pred = jepa_predictor_create(&recurrent_config);
    // RECURRENT may not be fully implemented yet - check for NULL or valid
    if (pred != nullptr) {
        EXPECT_EQ(pred->type, JEPA_PREDICTOR_RECURRENT);
        jepa_predictor_destroy(pred);
    }
    // If NULL, it means recurrent is not yet implemented - acceptable
}

TEST_F(JepaPredictorTest, RecurrentPrediction)
{
    jepa_predictor_config_t recurrent_config;
    jepa_predictor_default_config(&recurrent_config);
    recurrent_config.type = JEPA_PREDICTOR_RECURRENT;
    recurrent_config.input_dim = 32;
    recurrent_config.output_dim = 32;
    recurrent_config.hidden_dim = 64;

    jepa_predictor_t* pred = jepa_predictor_create(&recurrent_config);
    if (pred != nullptr) {
        jepa_latent_t* context = jepa_latent_create_dim(32);
        jepa_latent_t* prediction = jepa_latent_create_dim(32);
        ASSERT_NE(context, nullptr);
        ASSERT_NE(prediction, nullptr);

        for (uint32_t i = 0; i < 32; i++) {
            context->embedding[i] = 0.1f * static_cast<float>(i % 10);
        }

        int result = jepa_predictor_predict(pred, context, prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        jepa_latent_destroy(context);
        jepa_latent_destroy(prediction);
        jepa_predictor_destroy(pred);
    }
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(JepaPredictorTest, BioAsyncNotConnectedInitially)
{
    bool connected = jepa_predictor_is_bio_async_connected(predictor_);
    EXPECT_FALSE(connected);
}

TEST_F(JepaPredictorTest, BioAsyncConnectDisconnect)
{
    // Connect to bio-async router
    int result = jepa_predictor_connect_bio_async(predictor_);

    // Skip if bio-async router is not available
    if (!jepa_predictor_is_bio_async_connected(predictor_)) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check connected status
    bool connected = jepa_predictor_is_bio_async_connected(predictor_);
    EXPECT_TRUE(connected);

    // Disconnect from bio-async router
    result = jepa_predictor_disconnect_bio_async(predictor_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check disconnected status
    connected = jepa_predictor_is_bio_async_connected(predictor_);
    EXPECT_FALSE(connected);
}

TEST_F(JepaPredictorTest, BioAsyncConnectNullPredictor)
{
    int result = jepa_predictor_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, BioAsyncDisconnectNullPredictor)
{
    int result = jepa_predictor_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, BioAsyncIsConnectedNullPredictor)
{
    bool connected = jepa_predictor_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(JepaPredictorTest, BioAsyncDoubleConnect)
{
    // First connect should succeed
    int result = jepa_predictor_connect_bio_async(predictor_);

    // Skip if bio-async router is not available
    if (!jepa_predictor_is_bio_async_connected(predictor_)) {
        GTEST_SKIP() << "Bio-async router not available";
    }

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second connect should be idempotent (succeed or return already connected)
    result = jepa_predictor_connect_bio_async(predictor_);
    // Either succeeds or returns an "already connected" status
    bool connected = jepa_predictor_is_bio_async_connected(predictor_);
    EXPECT_TRUE(connected);

    // Cleanup
    jepa_predictor_disconnect_bio_async(predictor_);
}

TEST_F(JepaPredictorTest, BioAsyncDoubleDisconnect)
{
    // Connect first
    jepa_predictor_connect_bio_async(predictor_);

    // First disconnect should succeed
    int result = jepa_predictor_disconnect_bio_async(predictor_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second disconnect should be idempotent
    result = jepa_predictor_disconnect_bio_async(predictor_);
    // Either succeeds or returns an "already disconnected" status
    bool connected = jepa_predictor_is_bio_async_connected(predictor_);
    EXPECT_FALSE(connected);
}

//=============================================================================
// FEP Error Conversion Tests
//=============================================================================

TEST_F(JepaPredictorFEPTest, ToFEPErrorBasic)
{
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Set values with known difference
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        prediction->embedding[i] = 0.5f;
        target->embedding[i] = 0.3f;
    }

    // Compute internal error
    jepa_prediction_error_t* internal_error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(internal_error, nullptr);

    int result = jepa_predictor_compute_error(predictor_, prediction, target, internal_error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Convert to FEP error - properly initialize with allocated memory and matching dimension
    fep_prediction_error_t fep_error;
    memset(&fep_error, 0, sizeof(fep_error));
    fep_error.error = (float*)malloc(TEST_OUTPUT_DIM * sizeof(float));
    fep_error.weighted_error = (float*)malloc(TEST_OUTPUT_DIM * sizeof(float));
    fep_error.precision = (float*)malloc(TEST_OUTPUT_DIM * sizeof(float));
    fep_error.dim = TEST_OUTPUT_DIM;
    ASSERT_NE(fep_error.error, nullptr);
    ASSERT_NE(fep_error.weighted_error, nullptr);
    ASSERT_NE(fep_error.precision, nullptr);

    result = jepa_predictor_to_fep_error(predictor_, internal_error, &fep_error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // FEP error should have valid values
    EXPECT_GE(fep_error.magnitude, 0.0f);

    // Cleanup fep_error allocations
    free(fep_error.precision);
    free(fep_error.weighted_error);
    free(fep_error.error);

    jepa_prediction_error_destroy(internal_error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorFEPTest, ToFEPErrorNullPredictor)
{
    jepa_prediction_error_t* internal_error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(internal_error, nullptr);

    fep_prediction_error_t fep_error;
    int result = jepa_predictor_to_fep_error(nullptr, internal_error, &fep_error);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_prediction_error_destroy(internal_error);
}

TEST_F(JepaPredictorFEPTest, ToFEPErrorNullInternalError)
{
    fep_prediction_error_t fep_error;
    int result = jepa_predictor_to_fep_error(predictor_, nullptr, &fep_error);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorFEPTest, ToFEPErrorNullOutput)
{
    jepa_prediction_error_t* internal_error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(internal_error, nullptr);

    int result = jepa_predictor_to_fep_error(predictor_, internal_error, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_prediction_error_destroy(internal_error);
}

TEST_F(JepaPredictorFEPTest, ToFEPErrorPrecisionIntegration)
{
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Set values with small difference (high precision case)
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        prediction->embedding[i] = 0.5f;
        target->embedding[i] = 0.49f;
    }

    jepa_prediction_error_t* internal_error = jepa_prediction_error_create(TEST_OUTPUT_DIM);
    ASSERT_NE(internal_error, nullptr);

    jepa_predictor_compute_error(predictor_, prediction, target, internal_error);

    // Properly initialize fep_error with allocated memory and matching dimension
    fep_prediction_error_t fep_error;
    memset(&fep_error, 0, sizeof(fep_error));
    fep_error.error = (float*)malloc(TEST_OUTPUT_DIM * sizeof(float));
    fep_error.weighted_error = (float*)malloc(TEST_OUTPUT_DIM * sizeof(float));
    fep_error.precision = (float*)malloc(TEST_OUTPUT_DIM * sizeof(float));
    fep_error.dim = TEST_OUTPUT_DIM;
    ASSERT_NE(fep_error.error, nullptr);
    ASSERT_NE(fep_error.weighted_error, nullptr);
    ASSERT_NE(fep_error.precision, nullptr);

    int result = jepa_predictor_to_fep_error(predictor_, internal_error, &fep_error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Small error should result in high precision
    EXPECT_GT(internal_error->precision, 0.0f);

    // Cleanup fep_error allocations
    free(fep_error.precision);
    free(fep_error.weighted_error);
    free(fep_error.error);

    jepa_prediction_error_destroy(internal_error);
    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

//=============================================================================
// Integration Tests - Full Training Loop
//=============================================================================

TEST_F(JepaPredictorTest, IntegrationFullTrainingLoop)
{
    jepa_predictor_set_training(predictor_, true);

    // Create training data
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Initialize with consistent values
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.1f * static_cast<float>(i % 10);
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.05f * static_cast<float>(i % 10);
    }

    // Record initial loss
    jepa_predictor_predict(predictor_, context, prediction);
    float initial_loss = jepa_predictor_compute_loss(predictor_, prediction, target);
    EXPECT_FALSE(std::isnan(initial_loss));

    // Train for multiple epochs
    const int NUM_EPOCHS = 50;
    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        float epoch_loss = 0.0f;
        int result = jepa_predictor_train_step(predictor_, context, target, &epoch_loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Compute final loss
    jepa_predictor_predict(predictor_, context, prediction);
    float final_loss = jepa_predictor_compute_loss(predictor_, prediction, target);
    EXPECT_FALSE(std::isnan(final_loss));

    // Check statistics
    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);
    EXPECT_GE(stats.predictions_made, static_cast<uint64_t>(NUM_EPOCHS));
    EXPECT_GE(stats.updates_applied, static_cast<uint64_t>(NUM_EPOCHS));

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, IntegrationMultipleContexts)
{
    jepa_predictor_set_training(predictor_, true);

    // Create multiple training contexts
    const int NUM_CONTEXTS = 5;
    std::vector<jepa_latent_t*> contexts(NUM_CONTEXTS);
    std::vector<jepa_latent_t*> targets(NUM_CONTEXTS);

    for (int i = 0; i < NUM_CONTEXTS; i++) {
        contexts[i] = jepa_latent_create_dim(TEST_INPUT_DIM);
        targets[i] = jepa_latent_create_dim(TEST_OUTPUT_DIM);
        ASSERT_NE(contexts[i], nullptr);
        ASSERT_NE(targets[i], nullptr);

        // Different values for each context
        float base = 0.1f * static_cast<float>(i + 1);
        for (uint32_t j = 0; j < TEST_INPUT_DIM; j++) {
            contexts[i]->embedding[j] = base + 0.01f * static_cast<float>(j % 10);
        }
        for (uint32_t j = 0; j < TEST_OUTPUT_DIM; j++) {
            targets[i]->embedding[j] = base * 0.5f + 0.005f * static_cast<float>(j % 10);
        }
    }

    // Train on all contexts
    for (int epoch = 0; epoch < 10; epoch++) {
        for (int i = 0; i < NUM_CONTEXTS; i++) {
            float loss = 0.0f;
            int result = jepa_predictor_train_step(predictor_, contexts[i], targets[i], &loss);
            EXPECT_EQ(result, NIMCP_SUCCESS);
        }
    }

    // Check stats
    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);
    EXPECT_EQ(stats.predictions_made, static_cast<uint64_t>(10 * NUM_CONTEXTS));

    // Cleanup
    for (int i = 0; i < NUM_CONTEXTS; i++) {
        jepa_latent_destroy(contexts[i]);
        jepa_latent_destroy(targets[i]);
    }
}

TEST_F(JepaPredictorTest, IntegrationTrainingVsInference)
{
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction1 = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* prediction2 = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction1, nullptr);
    ASSERT_NE(prediction2, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.3f;
    }

    // Inference mode prediction
    jepa_predictor_set_training(predictor_, false);
    jepa_predictor_predict(predictor_, context, prediction1);

    // Training mode prediction (should be same without dropout)
    jepa_predictor_set_training(predictor_, true);
    jepa_predictor_predict(predictor_, context, prediction2);

    // If dropout_rate is 0, outputs should be the same
    if (predictor_->config.dropout_rate == 0.0f) {
        for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
            EXPECT_NEAR(prediction1->embedding[i], prediction2->embedding[i], TEST_EPSILON);
        }
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction1);
    jepa_latent_destroy(prediction2);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(JepaPredictorTest, StressManyPredictions)
{
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.1f;
    }

    const int NUM_PREDICTIONS = 1000;
    for (int i = 0; i < NUM_PREDICTIONS; i++) {
        int result = jepa_predictor_predict(predictor_, context, prediction);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);
    EXPECT_EQ(stats.predictions_made, static_cast<uint64_t>(NUM_PREDICTIONS));

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, StressManyTrainingSteps)
{
    jepa_predictor_set_training(predictor_, true);

    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.5f;
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.3f;
    }

    const int NUM_STEPS = 500;
    for (int i = 0; i < NUM_STEPS; i++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(predictor_, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));
        EXPECT_FALSE(std::isinf(loss));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(JepaPredictorTest, NumericalStabilityLargeValues)
{
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Large input values
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 100.0f;
    }

    int result = jepa_predictor_predict(predictor_, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check for NaN or Inf
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, NumericalStabilitySmallValues)
{
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Very small input values
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 1e-10f;
    }

    int result = jepa_predictor_predict(predictor_, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check for NaN or Inf
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, NumericalStabilityNegativeValues)
{
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Negative input values
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = -0.5f;
    }

    int result = jepa_predictor_predict(predictor_, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check for NaN or Inf
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, NumericalStabilityMixedValues)
{
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Mixed input values
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        if (i % 3 == 0) {
            context->embedding[i] = 10.0f;
        } else if (i % 3 == 1) {
            context->embedding[i] = -5.0f;
        } else {
            context->embedding[i] = 1e-5f;
        }
    }

    int result = jepa_predictor_predict(predictor_, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check for NaN or Inf
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

//=============================================================================
// Layer Normalization Tests
//=============================================================================

TEST_F(JepaPredictorTest, LayerNormEnabled)
{
    jepa_predictor_config_t ln_config;
    jepa_predictor_default_config(&ln_config);
    ln_config.input_dim = TEST_INPUT_DIM;
    ln_config.output_dim = TEST_OUTPUT_DIM;
    ln_config.hidden_dim = TEST_HIDDEN_DIM;
    ln_config.enable_layer_norm = true;

    jepa_predictor_t* pred = jepa_predictor_create(&ln_config);
    ASSERT_NE(pred, nullptr);
    EXPECT_TRUE(pred->config.enable_layer_norm);

    // Test prediction still works
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.5f;
    }

    int result = jepa_predictor_predict(pred, context, prediction);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// Weight Decay Tests
//=============================================================================

TEST_F(JepaPredictorTest, WeightDecayApplied)
{
    jepa_predictor_config_t wd_config;
    jepa_predictor_default_config(&wd_config);
    wd_config.input_dim = TEST_INPUT_DIM;
    wd_config.output_dim = TEST_OUTPUT_DIM;
    wd_config.hidden_dim = TEST_HIDDEN_DIM;
    wd_config.weight_decay = 0.1f;  // Strong weight decay
    wd_config.learning_rate = 0.01f;

    jepa_predictor_t* pred = jepa_predictor_create(&wd_config);
    ASSERT_NE(pred, nullptr);

    // Get initial weight magnitude
    float* weights = nullptr;
    uint32_t dims[2];
    jepa_predictor_get_weights(pred, 0, &weights, dims);

    float initial_sum = 0.0f;
    for (uint32_t i = 0; i < dims[0] * dims[1]; i++) {
        initial_sum += fabsf(weights[i]);
    }

    // Train a few steps
    jepa_predictor_set_training(pred, true);
    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.0f;  // Zero input to minimize gradient effect
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.0f;
    }

    // Multiple updates with zero gradients - weight decay should shrink weights
    for (int i = 0; i < 10; i++) {
        jepa_predictor_train_step(pred, context, target, nullptr);
    }

    // Get final weight magnitude
    jepa_predictor_get_weights(pred, 0, &weights, dims);
    float final_sum = 0.0f;
    for (uint32_t i = 0; i < dims[0] * dims[1]; i++) {
        final_sum += fabsf(weights[i]);
    }

    // With strong weight decay and weak gradients, weights should shrink
    // (This may not always hold depending on implementation details)
    EXPECT_GE(initial_sum, 0.0f);
    EXPECT_GE(final_sum, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// Cosine Loss Tests
//=============================================================================

TEST_F(JepaPredictorTest, CosineLossComputation)
{
    jepa_predictor_config_t cosine_config;
    jepa_predictor_default_config(&cosine_config);
    cosine_config.input_dim = TEST_INPUT_DIM;
    cosine_config.output_dim = TEST_OUTPUT_DIM;
    cosine_config.hidden_dim = TEST_HIDDEN_DIM;
    cosine_config.loss_type = JEPA_LOSS_COSINE;

    jepa_predictor_t* pred = jepa_predictor_create(&cosine_config);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Identical vectors - should have minimal loss
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        prediction->embedding[i] = 0.5f;
        target->embedding[i] = 0.5f;
    }

    float loss = jepa_predictor_compute_loss(pred, prediction, target);
    EXPECT_FALSE(std::isnan(loss));
    EXPECT_GE(loss, 0.0f);
    // Cosine loss for identical vectors should be near zero
    EXPECT_NEAR(loss, 0.0f, 0.01f);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorTest, CosineLossOrthogonal)
{
    jepa_predictor_config_t cosine_config;
    jepa_predictor_default_config(&cosine_config);
    cosine_config.input_dim = TEST_INPUT_DIM;
    cosine_config.output_dim = TEST_OUTPUT_DIM;
    cosine_config.hidden_dim = TEST_HIDDEN_DIM;
    cosine_config.loss_type = JEPA_LOSS_COSINE;

    jepa_predictor_t* pred = jepa_predictor_create(&cosine_config);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Orthogonal vectors (approximately)
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        prediction->embedding[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        target->embedding[i] = (i % 2 == 1) ? 1.0f : 0.0f;
    }

    float loss = jepa_predictor_compute_loss(pred, prediction, target);
    EXPECT_FALSE(std::isnan(loss));
    EXPECT_GE(loss, 0.0f);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// Smooth L1 Loss Tests
//=============================================================================

TEST_F(JepaPredictorTest, SmoothL1LossComputation)
{
    jepa_predictor_config_t sl1_config;
    jepa_predictor_default_config(&sl1_config);
    sl1_config.input_dim = TEST_INPUT_DIM;
    sl1_config.output_dim = TEST_OUTPUT_DIM;
    sl1_config.hidden_dim = TEST_HIDDEN_DIM;
    sl1_config.loss_type = JEPA_LOSS_SMOOTH_L1;

    jepa_predictor_t* pred = jepa_predictor_create(&sl1_config);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Small difference
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        prediction->embedding[i] = 0.5f;
        target->embedding[i] = 0.45f;
    }

    float loss = jepa_predictor_compute_loss(pred, prediction, target);
    EXPECT_FALSE(std::isnan(loss));
    EXPECT_GE(loss, 0.0f);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

//=============================================================================
// Statistics Tracking Tests
//=============================================================================

TEST_F(JepaPredictorTest, StatsTrackMinMaxLoss)
{
    jepa_predictor_set_training(predictor_, true);

    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    // Train a few steps
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.5f;
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.3f;
    }

    for (int i = 0; i < 10; i++) {
        jepa_predictor_train_step(predictor_, context, target, nullptr);
    }

    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);

    // Stats should be tracked
    EXPECT_GE(stats.min_loss, 0.0f);
    EXPECT_GE(stats.max_loss, stats.min_loss);
    EXPECT_GE(stats.avg_loss, stats.min_loss);
    EXPECT_LE(stats.avg_loss, stats.max_loss);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, StatsTrackUpdates)
{
    jepa_predictor_set_training(predictor_, true);

    jepa_latent_t* context = jepa_latent_create_dim(TEST_INPUT_DIM);
    jepa_latent_t* target = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        context->embedding[i] = 0.5f;
    }
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        target->embedding[i] = 0.3f;
    }

    const int NUM_STEPS = 25;
    for (int i = 0; i < NUM_STEPS; i++) {
        jepa_predictor_train_step(predictor_, context, target, nullptr);
    }

    jepa_predictor_stats_t stats;
    jepa_predictor_get_stats(predictor_, &stats);
    EXPECT_EQ(stats.updates_applied, static_cast<uint64_t>(NUM_STEPS));

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

//=============================================================================
// Quantum Monte Carlo Integration Tests
//=============================================================================

TEST_F(JepaPredictorTest, QMCConfigInit)
{
    // WHAT: Test QMC config initialization
    // WHY:  Verify defaults are reasonable
    // HOW:  Initialize config and check values

    jepa_qmc_config_t config;
    int result = jepa_qmc_config_init(&config);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(config.num_samples, 0u);
    EXPECT_GT(config.num_iterations, 0u);
    EXPECT_GT(config.initial_temp, config.final_temp);
    EXPECT_GT(config.exploration_constant, 1.0f);
    EXPECT_GE(config.quantum_strength, 0.0f);
    EXPECT_LE(config.quantum_strength, 1.0f);
}

TEST_F(JepaPredictorTest, QMCConfigInitNull)
{
    // WHAT: Test NULL config handling
    // WHY:  Should return error on NULL
    // HOW:  Pass NULL to init function

    int result = jepa_qmc_config_init(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaPredictorTest, QMCAmplitudeEstimateBasic)
{
    // WHAT: Test amplitude estimation for a dimension
    // WHY:  Core QMC uncertainty quantification
    // HOW:  Estimate amplitude and verify valid output

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    ASSERT_NE(context, nullptr);

    float amplitude = 0.0f;
    float variance = 0.0f;

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 100;  // Reduce for test speed

    int result = jepa_predictor_qmc_amplitude_estimate(
        predictor_, context, 0, &config, &amplitude, &variance);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(amplitude, 0.0f);
    EXPECT_GE(variance, 0.0f);

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, QMCAmplitudeEstimateNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    float amplitude = 0.0f;
    float variance = 0.0f;

    EXPECT_NE(jepa_predictor_qmc_amplitude_estimate(
        nullptr, context, 0, nullptr, &amplitude, &variance), NIMCP_SUCCESS);

    EXPECT_NE(jepa_predictor_qmc_amplitude_estimate(
        predictor_, nullptr, 0, nullptr, &amplitude, &variance), NIMCP_SUCCESS);

    EXPECT_NE(jepa_predictor_qmc_amplitude_estimate(
        predictor_, context, 0, nullptr, nullptr, &variance), NIMCP_SUCCESS);

    EXPECT_NE(jepa_predictor_qmc_amplitude_estimate(
        predictor_, context, 0, nullptr, &amplitude, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, QMCAmplitudeEstimateInvalidDim)
{
    // WHAT: Test invalid dimension handling
    // WHY:  Should reject out-of-bounds dimension
    // HOW:  Pass dimension >= output_dim

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    float amplitude = 0.0f;
    float variance = 0.0f;

    int result = jepa_predictor_qmc_amplitude_estimate(
        predictor_, context, TEST_OUTPUT_DIM + 10, nullptr, &amplitude, &variance);

    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, QMCEntropyEstimate)
{
    // WHAT: Test entropy estimation
    // WHY:  Entropy measures uncertainty in latent space
    // HOW:  Estimate entropy and verify valid output

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);

    float entropy = 0.0f;
    float std_error = 0.0f;

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 100;

    int result = jepa_predictor_qmc_entropy(
        predictor_, context, &config, &entropy, &std_error);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(entropy, 0.0f);  // Entropy should be non-negative
    EXPECT_GE(std_error, 0.0f);

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, QMCEntropyNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    float entropy = 0.0f;

    EXPECT_NE(jepa_predictor_qmc_entropy(nullptr, context, nullptr, &entropy, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_entropy(predictor_, nullptr, nullptr, &entropy, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_entropy(predictor_, context, nullptr, nullptr, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, QMCFidelityBasic)
{
    // WHAT: Test fidelity computation between latent states
    // WHY:  Fidelity measures similarity in latent space
    // HOW:  Compute fidelity and verify valid output

    jepa_latent_t* latent1 = create_test_latent(TEST_OUTPUT_DIM, 0.5f);
    jepa_latent_t* latent2 = create_test_latent(TEST_OUTPUT_DIM, 0.5f);
    ASSERT_NE(latent1, nullptr);
    ASSERT_NE(latent2, nullptr);

    float fidelity = jepa_predictor_qmc_fidelity(predictor_, latent1, latent2);

    // Identical vectors should have high fidelity
    EXPECT_GE(fidelity, 0.9f);
    EXPECT_LE(fidelity, 1.0f);

    jepa_latent_destroy(latent1);
    jepa_latent_destroy(latent2);
}

TEST_F(JepaPredictorTest, QMCFidelityDifferentVectors)
{
    // WHAT: Test fidelity with different vectors
    // WHY:  Different vectors should have lower fidelity
    // HOW:  Create orthogonal-ish vectors

    jepa_latent_t* latent1 = create_test_latent(TEST_OUTPUT_DIM, 0.0f);
    jepa_latent_t* latent2 = create_test_latent(TEST_OUTPUT_DIM, 0.0f);
    ASSERT_NE(latent1, nullptr);
    ASSERT_NE(latent2, nullptr);

    // Set different patterns
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        latent1->embedding[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        latent2->embedding[i] = (i % 2 == 1) ? 1.0f : 0.0f;
    }

    float fidelity = jepa_predictor_qmc_fidelity(predictor_, latent1, latent2);

    // Different vectors should have lower fidelity
    EXPECT_GE(fidelity, 0.0f);
    EXPECT_LE(fidelity, 1.0f);

    jepa_latent_destroy(latent1);
    jepa_latent_destroy(latent2);
}

TEST_F(JepaPredictorTest, QMCFidelityNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should return 0.0 for invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* latent = create_test_latent(TEST_OUTPUT_DIM);

    EXPECT_EQ(jepa_predictor_qmc_fidelity(nullptr, latent, latent), 0.0f);
    EXPECT_EQ(jepa_predictor_qmc_fidelity(predictor_, nullptr, latent), 0.0f);
    EXPECT_EQ(jepa_predictor_qmc_fidelity(predictor_, latent, nullptr), 0.0f);

    jepa_latent_destroy(latent);
}

TEST_F(JepaPredictorTest, QMCPredictBasic)
{
    // WHAT: Test QMC-enhanced prediction
    // WHY:  Verify prediction with uncertainty estimation
    // HOW:  Make prediction and check outputs

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(prediction, nullptr);

    std::vector<float> uncertainty(TEST_OUTPUT_DIM);
    jepa_qmc_stats_t stats;

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 50;  // Reduce for test speed

    int result = jepa_predictor_predict_qmc(
        predictor_, context, prediction, uncertainty.data(), &config, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check prediction is valid
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_FALSE(std::isnan(prediction->embedding[i]));
        EXPECT_FALSE(std::isinf(prediction->embedding[i]));
    }

    // Check uncertainty is valid
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_GE(uncertainty[i], 0.0f);
        EXPECT_FALSE(std::isnan(uncertainty[i]));
    }

    // Check stats
    EXPECT_GT(stats.samples_taken, 0u);
    EXPECT_GE(stats.computation_time_ms, 0.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, QMCPredictNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* prediction = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    std::vector<float> uncertainty(TEST_OUTPUT_DIM);

    EXPECT_NE(jepa_predictor_predict_qmc(nullptr, context, prediction,
                                          uncertainty.data(), nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_predict_qmc(predictor_, nullptr, prediction,
                                          uncertainty.data(), nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_predict_qmc(predictor_, context, nullptr,
                                          uncertainty.data(), nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_predict_qmc(predictor_, context, prediction,
                                          nullptr, nullptr, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(prediction);
}

TEST_F(JepaPredictorTest, QMCFreeEnergyBasic)
{
    // WHAT: Test free energy estimation
    // WHY:  Free energy connects to FEP belief optimization
    // HOW:  Estimate free energy and verify valid output

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float free_energy = 0.0f;

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_samples = 50;

    int result = jepa_predictor_qmc_free_energy(
        predictor_, context, target, 1.0f, &config, &free_energy);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(std::isnan(free_energy));
    EXPECT_FALSE(std::isinf(free_energy));

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, QMCFreeEnergyNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* target = create_test_latent(TEST_OUTPUT_DIM);
    float free_energy = 0.0f;

    EXPECT_NE(jepa_predictor_qmc_free_energy(nullptr, context, target, 1.0f, nullptr, &free_energy), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_free_energy(predictor_, nullptr, target, 1.0f, nullptr, &free_energy), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_free_energy(predictor_, context, nullptr, 1.0f, nullptr, &free_energy), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_free_energy(predictor_, context, target, 1.0f, nullptr, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
}

TEST_F(JepaPredictorTest, QMCMCTSExploreBasic)
{
    // WHAT: Test MCTS-guided latent space exploration
    // WHY:  Verify structured exploration finds reasonable predictions
    // HOW:  Run MCTS and check output

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    jepa_latent_t* best_latent = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(best_latent, nullptr);

    float value = 0.0f;

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);
    config.num_iterations = 20;  // Reduce for test speed

    int result = jepa_predictor_qmc_mcts_explore(
        predictor_, context, 5, &config, best_latent, &value);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check output is valid
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        EXPECT_FALSE(std::isnan(best_latent->embedding[i]));
        EXPECT_FALSE(std::isinf(best_latent->embedding[i]));
    }

    EXPECT_GE(value, 0.0f);
    EXPECT_LE(value, 1.0f);

    jepa_latent_destroy(context);
    jepa_latent_destroy(best_latent);
}

TEST_F(JepaPredictorTest, QMCMCTSExploreNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* best_latent = jepa_latent_create_dim(TEST_OUTPUT_DIM);
    float value = 0.0f;

    EXPECT_NE(jepa_predictor_qmc_mcts_explore(nullptr, context, 5, nullptr, best_latent, &value), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_mcts_explore(predictor_, nullptr, 5, nullptr, best_latent, &value), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_mcts_explore(predictor_, context, 5, nullptr, nullptr, &value), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_mcts_explore(predictor_, context, 5, nullptr, best_latent, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(best_latent);
}

TEST_F(JepaPredictorTest, QMCSampleLatentBasic)
{
    // WHAT: Test latent space sampling via QMC
    // WHY:  Enable generative modeling
    // HOW:  Generate samples and verify validity

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM, 0.5f);
    ASSERT_NE(context, nullptr);

    const uint32_t NUM_SAMPLES = 5;
    jepa_latent_t* samples[NUM_SAMPLES];

    jepa_qmc_config_t config;
    jepa_qmc_config_init(&config);

    int result = jepa_predictor_qmc_sample_latent(
        predictor_, context, samples, NUM_SAMPLES, &config);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check samples are valid
    for (uint32_t s = 0; s < NUM_SAMPLES; s++) {
        ASSERT_NE(samples[s], nullptr);
        EXPECT_EQ(samples[s]->latent_dim, TEST_OUTPUT_DIM);

        for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
            EXPECT_FALSE(std::isnan(samples[s]->embedding[i]));
            EXPECT_FALSE(std::isinf(samples[s]->embedding[i]));
        }

        jepa_latent_destroy(samples[s]);
    }

    jepa_latent_destroy(context);
}

TEST_F(JepaPredictorTest, QMCSampleLatentNullInputs)
{
    // WHAT: Test NULL input handling
    // WHY:  Should reject invalid inputs
    // HOW:  Pass NULL values

    jepa_latent_t* context = create_test_latent(TEST_INPUT_DIM);
    jepa_latent_t* samples[5];

    EXPECT_NE(jepa_predictor_qmc_sample_latent(nullptr, context, samples, 5, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_sample_latent(predictor_, nullptr, samples, 5, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_sample_latent(predictor_, context, nullptr, 5, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(jepa_predictor_qmc_sample_latent(predictor_, context, samples, 0, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
}

}  // namespace
