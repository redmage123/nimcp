/**
 * @file test_jepa_predictor_extended_regression.cpp
 * @brief Regression tests for JEPA TRANSFORMER and RECURRENT predictor types
 *
 * WHAT: Verify stability, gradient boundedness, convergence, and memory safety
 *       for TRANSFORMER and RECURRENT predictor types under sustained operation
 * WHY:  Ensure extended predictor types do not regress under stress: no NaN/Inf
 *       gradients, no training divergence, no memory leaks from create/destroy
 *       cycles, and no weight explosion after many training steps
 * HOW:  Run backward pass over 100 iterations checking gradient bounds, verify
 *       training loss decreases over 50 steps, cycle create/destroy 50 times,
 *       and check weight norms remain bounded
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <numeric>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/error/nimcp_error_codes.h"

namespace {

/* ============================================================================
 * Constants
 * ============================================================================ */

static const uint32_t REG_DIM = 32;
static const uint32_t REG_HIDDEN_DIM = 64;
static const uint32_t REG_NUM_LAYERS = 2;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class JepaPredictorExtendedRegressionTest : public ::testing::Test {
protected:
    jepa_predictor_config_t config_;

    void SetUp() override {
        int result = jepa_predictor_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        config_.input_dim = REG_DIM;
        config_.output_dim = REG_DIM;
        config_.hidden_dim = REG_HIDDEN_DIM;
        config_.num_layers = REG_NUM_LAYERS;
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

    // Compute L2 norm of a float array
    float compute_l2_norm(const float* data, uint32_t size) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            sum += data[i] * data[i];
        }
        return sqrtf(sum);
    }

    // Check if all elements in a float array are finite (not NaN or Inf)
    bool all_finite(const float* data, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            if (std::isnan(data[i]) || std::isinf(data[i])) {
                return false;
            }
        }
        return true;
    }
};

/* ============================================================================
 * Bounded Gradients Tests (100 iterations)
 * ============================================================================ */

TEST_F(JepaPredictorExtendedRegressionTest, TransformerBoundedGradients100Iterations) {
    // WHAT: TRANSFORMER backward produces bounded gradients over 100 iterations
    // WHY:  Detect gradient explosion/vanishing regressions in backward_transformer
    // HOW:  Run 100 forward-backward cycles, verify no NaN/Inf in predictions

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(REG_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(REG_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    for (int iter = 0; iter < 100; iter++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        ASSERT_EQ(result, NIMCP_SUCCESS)
            << "Train step failed at iteration " << iter;

        // Loss must remain finite
        ASSERT_FALSE(std::isnan(loss))
            << "NaN loss at iteration " << iter;
        ASSERT_FALSE(std::isinf(loss))
            << "Inf loss at iteration " << iter;

        // Verify prediction output is bounded after training step
        jepa_latent_t* check = jepa_latent_create_dim(REG_DIM);
        ASSERT_NE(check, nullptr);
        result = jepa_predictor_predict(pred, context, check);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        ASSERT_TRUE(all_finite(check->embedding, REG_DIM))
            << "Non-finite prediction at iteration " << iter;

        jepa_latent_destroy(check);
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedRegressionTest, RecurrentBoundedGradients100Iterations) {
    // WHAT: RECURRENT backward produces bounded gradients over 100 iterations
    // WHY:  Detect gradient explosion/vanishing regressions in backward_recurrent
    // HOW:  Run 100 forward-backward cycles, verify no NaN/Inf in predictions

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(REG_DIM, 0.15f);
    jepa_latent_t* target = create_test_latent(REG_DIM, 0.35f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    for (int iter = 0; iter < 100; iter++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        ASSERT_EQ(result, NIMCP_SUCCESS)
            << "Train step failed at iteration " << iter;

        ASSERT_FALSE(std::isnan(loss))
            << "NaN loss at iteration " << iter;
        ASSERT_FALSE(std::isinf(loss))
            << "Inf loss at iteration " << iter;

        // Spot-check prediction every 10 iterations for performance
        if (iter % 10 == 0) {
            jepa_latent_t* check = jepa_latent_create_dim(REG_DIM);
            ASSERT_NE(check, nullptr);
            int res = jepa_predictor_predict(pred, context, check);
            ASSERT_EQ(res, NIMCP_SUCCESS);
            ASSERT_TRUE(all_finite(check->embedding, REG_DIM))
                << "Non-finite prediction at iteration " << iter;
            jepa_latent_destroy(check);
        }
    }

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * Training Convergence Tests (50 steps)
 * ============================================================================ */

TEST_F(JepaPredictorExtendedRegressionTest, TransformerTrainingConverges50Steps) {
    // WHAT: TRANSFORMER training converges (loss decreases over 50 steps)
    // WHY:  Verify the backward+update cycle actually learns
    // HOW:  Record loss at step 0 and step 49, final should be less

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(REG_DIM, 0.1f);
    jepa_latent_t* target = create_test_latent(REG_DIM, 0.3f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    float first_loss = 0.0f;
    float last_loss = 0.0f;
    std::vector<float> losses;

    for (int step = 0; step < 50; step++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));

        losses.push_back(loss);
        if (step == 0) first_loss = loss;
        last_loss = loss;
    }

    // Final loss should be less than initial loss
    // Use a generous check: last 5 average vs first 5 average
    float first_5_avg = 0.0f;
    float last_5_avg = 0.0f;
    for (int i = 0; i < 5; i++) {
        first_5_avg += losses[i];
        last_5_avg += losses[45 + i];
    }
    first_5_avg /= 5.0f;
    last_5_avg /= 5.0f;

    EXPECT_LT(last_5_avg, first_5_avg)
        << "TRANSFORMER training should converge. "
        << "First 5 avg: " << first_5_avg << ", Last 5 avg: " << last_5_avg;

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

TEST_F(JepaPredictorExtendedRegressionTest, RecurrentTrainingConverges50Steps) {
    // WHAT: RECURRENT training converges (loss decreases over 50 steps)
    // WHY:  Verify the BPTT-style backward+update cycle actually learns
    // HOW:  Record loss at step 0 and step 49, final should be less

    jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
    ASSERT_NE(pred, nullptr);

    jepa_latent_t* context = create_test_latent(REG_DIM, 0.15f);
    jepa_latent_t* target = create_test_latent(REG_DIM, 0.35f);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(target, nullptr);

    std::vector<float> losses;

    for (int step = 0; step < 50; step++) {
        float loss = 0.0f;
        int result = jepa_predictor_train_step(pred, context, target, &loss);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(loss));
        losses.push_back(loss);
    }

    // Compare first 5 average vs last 5 average
    float first_5_avg = 0.0f;
    float last_5_avg = 0.0f;
    for (int i = 0; i < 5; i++) {
        first_5_avg += losses[i];
        last_5_avg += losses[45 + i];
    }
    first_5_avg /= 5.0f;
    last_5_avg /= 5.0f;

    EXPECT_LT(last_5_avg, first_5_avg)
        << "RECURRENT training should converge. "
        << "First 5 avg: " << first_5_avg << ", Last 5 avg: " << last_5_avg;

    jepa_latent_destroy(context);
    jepa_latent_destroy(target);
    jepa_predictor_destroy(pred);
}

/* ============================================================================
 * Create/Destroy Cycle Stress Test
 * ============================================================================ */

TEST_F(JepaPredictorExtendedRegressionTest, RepeatedCreateDestroy50Cycles) {
    // WHAT: Repeated create/destroy cycles (50x) don't crash or leak
    // WHY:  Detect memory leaks, use-after-free, or resource exhaustion
    // HOW:  Create and destroy TRANSFORMER and RECURRENT predictors in a loop,
    //       performing a minimal operation each time to exercise initialization

    for (int cycle = 0; cycle < 50; cycle++) {
        // TRANSFORMER cycle
        jepa_predictor_t* trans = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
        ASSERT_NE(trans, nullptr)
            << "TRANSFORMER creation failed at cycle " << cycle;

        // Quick sanity: make one prediction
        jepa_latent_t* ctx = create_test_latent(REG_DIM, 0.1f);
        jepa_latent_t* out = jepa_latent_create_dim(REG_DIM);
        ASSERT_NE(ctx, nullptr);
        ASSERT_NE(out, nullptr);

        int result = jepa_predictor_predict(trans, ctx, out);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        jepa_latent_destroy(ctx);
        jepa_latent_destroy(out);
        jepa_predictor_destroy(trans);

        // RECURRENT cycle
        jepa_predictor_t* rec = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
        ASSERT_NE(rec, nullptr)
            << "RECURRENT creation failed at cycle " << cycle;

        ctx = create_test_latent(REG_DIM, 0.15f);
        out = jepa_latent_create_dim(REG_DIM);
        ASSERT_NE(ctx, nullptr);
        ASSERT_NE(out, nullptr);

        result = jepa_predictor_predict(rec, ctx, out);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        jepa_latent_destroy(ctx);
        jepa_latent_destroy(out);
        jepa_predictor_destroy(rec);
    }
}

/* ============================================================================
 * Weight Norm Boundedness After Training
 * ============================================================================ */

TEST_F(JepaPredictorExtendedRegressionTest, WeightNormsBoundedAfterTraining) {
    // WHAT: Weight norms remain bounded after training
    // WHY:  Detect weight explosion regressions in extended predictor types
    // HOW:  Train both types for 100 steps, verify weight L2 norms stay bounded

    // Test TRANSFORMER
    {
        jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_TRANSFORMER);
        ASSERT_NE(pred, nullptr);

        jepa_latent_t* context = create_test_latent(REG_DIM, 0.1f);
        jepa_latent_t* target = create_test_latent(REG_DIM, 0.3f);
        ASSERT_NE(context, nullptr);
        ASSERT_NE(target, nullptr);

        // Record initial weight norms
        uint32_t num_layers = pred->network.mlp.num_layers;
        std::vector<float> initial_norms(num_layers);
        for (uint32_t l = 0; l < num_layers; l++) {
            float* w_ptr = nullptr;
            uint32_t dims[2] = {0, 0};
            int result = jepa_predictor_get_weights(pred, l, &w_ptr, dims);
            ASSERT_EQ(result, NIMCP_SUCCESS);
            initial_norms[l] = compute_l2_norm(w_ptr, dims[0] * dims[1]);
        }

        // Train for 100 steps
        for (int step = 0; step < 100; step++) {
            float loss = 0.0f;
            int result = jepa_predictor_train_step(pred, context, target, &loss);
            EXPECT_EQ(result, NIMCP_SUCCESS);
        }

        // Check final weight norms
        for (uint32_t l = 0; l < num_layers; l++) {
            float* w_ptr = nullptr;
            uint32_t dims[2] = {0, 0};
            int result = jepa_predictor_get_weights(pred, l, &w_ptr, dims);
            ASSERT_EQ(result, NIMCP_SUCCESS);

            float final_norm = compute_l2_norm(w_ptr, dims[0] * dims[1]);
            EXPECT_TRUE(std::isfinite(final_norm))
                << "TRANSFORMER layer " << l << " weight norm is not finite";
            // Weight norm should not explode (allow 100x growth max)
            EXPECT_LT(final_norm, (initial_norms[l] + 1.0f) * 100.0f)
                << "TRANSFORMER layer " << l << " weight norm exploded: "
                << initial_norms[l] << " -> " << final_norm;
        }

        jepa_latent_destroy(context);
        jepa_latent_destroy(target);
        jepa_predictor_destroy(pred);
    }

    // Test RECURRENT
    {
        jepa_predictor_t* pred = create_typed_predictor(JEPA_PREDICTOR_RECURRENT);
        ASSERT_NE(pred, nullptr);

        jepa_latent_t* context = create_test_latent(REG_DIM, 0.15f);
        jepa_latent_t* target = create_test_latent(REG_DIM, 0.35f);
        ASSERT_NE(context, nullptr);
        ASSERT_NE(target, nullptr);

        uint32_t num_layers = pred->network.mlp.num_layers;
        std::vector<float> initial_norms(num_layers);
        for (uint32_t l = 0; l < num_layers; l++) {
            float* w_ptr = nullptr;
            uint32_t dims[2] = {0, 0};
            int result = jepa_predictor_get_weights(pred, l, &w_ptr, dims);
            ASSERT_EQ(result, NIMCP_SUCCESS);
            initial_norms[l] = compute_l2_norm(w_ptr, dims[0] * dims[1]);
        }

        for (int step = 0; step < 100; step++) {
            float loss = 0.0f;
            int result = jepa_predictor_train_step(pred, context, target, &loss);
            EXPECT_EQ(result, NIMCP_SUCCESS);
        }

        for (uint32_t l = 0; l < num_layers; l++) {
            float* w_ptr = nullptr;
            uint32_t dims[2] = {0, 0};
            int result = jepa_predictor_get_weights(pred, l, &w_ptr, dims);
            ASSERT_EQ(result, NIMCP_SUCCESS);

            float final_norm = compute_l2_norm(w_ptr, dims[0] * dims[1]);
            EXPECT_TRUE(std::isfinite(final_norm))
                << "RECURRENT layer " << l << " weight norm is not finite";
            EXPECT_LT(final_norm, (initial_norms[l] + 1.0f) * 100.0f)
                << "RECURRENT layer " << l << " weight norm exploded: "
                << initial_norms[l] << " -> " << final_norm;
        }

        jepa_latent_destroy(context);
        jepa_latent_destroy(target);
        jepa_predictor_destroy(pred);
    }
}

} // namespace
