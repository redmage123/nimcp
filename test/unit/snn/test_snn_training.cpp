/**
 * @file test_snn_training.cpp
 * @brief Unit tests for SNN Training Module
 *
 * WHAT: Comprehensive tests for SNN training algorithms
 * WHY:  Verify correctness of STDP, R-STDP, surrogate gradients, eProp
 * HOW:  GoogleTest framework with 25+ test cases
 *
 * @author NIMCP Team
 * @date 2024
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_network.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SNNTrainingTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize STDP config */
        snn_stdp_config_default(&stdp_config);

        /* Initialize R-STDP config */
        snn_rstdp_config_default(&rstdp_config);

        /* Initialize surrogate config */
        snn_surrogate_config_default(&surrogate_config);

        /* Initialize eProp config */
        snn_eprop_config_default(&eprop_config);

        /* Initialize homeostatic config */
        snn_homeostatic_config_default(&homeostatic_config);
    }

    void TearDown() override {
        /* Cleanup handled in individual tests */
    }

    snn_stdp_config_t stdp_config;
    snn_rstdp_config_t rstdp_config;
    snn_surrogate_config_t surrogate_config;
    snn_eprop_config_t eprop_config;
    snn_homeostatic_config_t homeostatic_config;
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SNNTrainingTest, STDPConfigDefaultSetsReasonableValues) {
    /* WHAT: Verify STDP defaults are biologically plausible */
    EXPECT_NEAR(stdp_config.a_plus, 0.01f, 0.001f);
    EXPECT_NEAR(stdp_config.a_minus, 0.0105f, 0.001f);
    EXPECT_NEAR(stdp_config.tau_plus, 20.0f, 1.0f);
    EXPECT_NEAR(stdp_config.tau_minus, 20.0f, 1.0f);
    EXPECT_NEAR(stdp_config.w_min, 0.0f, 0.01f);
    EXPECT_NEAR(stdp_config.w_max, 1.0f, 0.01f);
    EXPECT_TRUE(stdp_config.soft_bounds);
    EXPECT_FALSE(stdp_config.symmetric);
}

TEST_F(SNNTrainingTest, RSTDPConfigDefaultSetsReasonableValues) {
    /* WHAT: Verify R-STDP defaults include base STDP plus traces */
    EXPECT_NEAR(rstdp_config.stdp.a_plus, 0.01f, 0.001f);
    EXPECT_NEAR(rstdp_config.eligibility_tau, 100.0f, 10.0f);
    EXPECT_NEAR(rstdp_config.reward_tau, 50.0f, 10.0f);
    EXPECT_NEAR(rstdp_config.baseline_reward, 0.0f, 0.01f);
    EXPECT_FALSE(rstdp_config.use_td_error);
}

TEST_F(SNNTrainingTest, SurrogateConfigDefaultSetsFastSigmoid) {
    /* WHAT: Verify surrogate defaults use fast sigmoid */
    EXPECT_EQ(surrogate_config.type, SNN_SURROGATE_FAST_SIGMOID);
    EXPECT_NEAR(surrogate_config.beta, 10.0f, 1.0f);
    EXPECT_NEAR(surrogate_config.threshold, 1.0f, 0.1f);
    EXPECT_NEAR(surrogate_config.learning_rate, 1e-3f, 1e-4f);
    EXPECT_NEAR(surrogate_config.momentum, 0.9f, 0.01f);
    EXPECT_NEAR(surrogate_config.weight_decay, 1e-5f, 1e-6f);
}

TEST_F(SNNTrainingTest, EpropConfigDefaultUsesAdam) {
    /* WHAT: Verify eProp defaults use Adam optimizer */
    EXPECT_NEAR(eprop_config.learning_rate, 1e-3f, 1e-4f);
    EXPECT_NEAR(eprop_config.eligibility_tau, 100.0f, 10.0f);
    EXPECT_NEAR(eprop_config.kappa, 0.1f, 0.01f);
    EXPECT_TRUE(eprop_config.use_adam);
    EXPECT_NEAR(eprop_config.adam_beta1, 0.9f, 0.01f);
    EXPECT_NEAR(eprop_config.adam_beta2, 0.999f, 0.001f);
}

TEST_F(SNNTrainingTest, HomeostaticConfigDefaultTargetsCortical) {
    /* WHAT: Verify homeostatic defaults target cortical rates */
    EXPECT_NEAR(homeostatic_config.target_rate, 5.0f, 1.0f);  /* 5 Hz */
    EXPECT_NEAR(homeostatic_config.rate_tau, 1000.0f, 100.0f);  /* 1 second */
    EXPECT_NEAR(homeostatic_config.adaptation_rate, 0.01f, 0.005f);
    EXPECT_TRUE(homeostatic_config.adjust_threshold);
    EXPECT_FALSE(homeostatic_config.adjust_weights);
}

TEST_F(SNNTrainingTest, DefaultConfigHandlesNullPointer) {
    /* WHAT: Verify config functions handle NULL safely */
    snn_stdp_config_default(nullptr);
    snn_rstdp_config_default(nullptr);
    snn_surrogate_config_default(nullptr);
    snn_eprop_config_default(nullptr);
    snn_homeostatic_config_default(nullptr);
    /* Should not crash */
    SUCCEED();
}

//=============================================================================
// STDP Context Creation Tests
//=============================================================================

TEST_F(SNNTrainingTest, CreateSTDPContextWithValidConfig) {
    /* WHAT: Create STDP context from valid config */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->mode, SNN_TRAIN_STDP);
    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, CreateSTDPContextReturnsNullForNullConfig) {
    /* WHAT: Verify NULL config is rejected */
    snn_training_ctx_t* ctx = snn_training_create_stdp(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(SNNTrainingTest, CreateRSTDPContextWithValidConfig) {
    /* WHAT: Create R-STDP context with eligibility tensor */
    snn_training_ctx_t* ctx = snn_training_create_rstdp(&rstdp_config, 10, 5);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->mode, SNN_TRAIN_R_STDP);
    EXPECT_NE(ctx->eligibility, nullptr);
    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, CreateRSTDPContextRejectsZeroDimensions) {
    /* WHAT: Verify zero dimensions rejected */
    snn_training_ctx_t* ctx1 = snn_training_create_rstdp(&rstdp_config, 0, 5);
    EXPECT_EQ(ctx1, nullptr);

    snn_training_ctx_t* ctx2 = snn_training_create_rstdp(&rstdp_config, 10, 0);
    EXPECT_EQ(ctx2, nullptr);
}

TEST_F(SNNTrainingTest, CreateSurrogateContextWithValidConfig) {
    /* WHAT: Create surrogate gradient context */
    snn_training_ctx_t* ctx = snn_training_create_surrogate(&surrogate_config, 10, 5);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->mode, SNN_TRAIN_SURROGATE);
    EXPECT_NE(ctx->grad_weights, nullptr);
    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, CreateEpropContextWithValidConfig) {
    /* WHAT: Create eProp context with eligibility and gradient tensors */
    snn_training_ctx_t* ctx = snn_training_create_eprop(&eprop_config, 8, 4);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->mode, SNN_TRAIN_EPROP);
    EXPECT_NE(ctx->eligibility, nullptr);
    EXPECT_NE(ctx->grad_weights, nullptr);
    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, DestroyHandlesNullContext) {
    /* WHAT: Verify destroy handles NULL safely */
    snn_training_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// STDP Weight Change Tests
//=============================================================================

TEST_F(SNNTrainingTest, STDPDeltaWPositiveForPreBeforePost) {
    /* WHAT: Verify LTP when pre fires before post */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);

    /* Pre fires 10ms before post: LTP */
    float delta_w = snn_stdp_compute_delta_w(ctx, 10.0f, 0.5f);
    EXPECT_GT(delta_w, 0.0f);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, STDPDeltaWNegativeForPostBeforePre) {
    /* WHAT: Verify LTD when post fires before pre */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);

    /* Post fires 10ms before pre: LTD (negative dt) */
    float delta_w = snn_stdp_compute_delta_w(ctx, -10.0f, 0.5f);
    EXPECT_LT(delta_w, 0.0f);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, STDPDeltaWDecaysWithTime) {
    /* WHAT: Verify delta_w decreases with larger time differences */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);

    float delta_near = snn_stdp_compute_delta_w(ctx, 5.0f, 0.5f);
    float delta_far = snn_stdp_compute_delta_w(ctx, 50.0f, 0.5f);

    EXPECT_GT(std::abs(delta_near), std::abs(delta_far));

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, STDPDeltaWRespectsSoftBounds) {
    /* WHAT: Verify soft bounds limit weight changes */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);

    /* High weight: LTP should be smaller */
    float delta_high = snn_stdp_compute_delta_w(ctx, 10.0f, 0.95f);
    float delta_low = snn_stdp_compute_delta_w(ctx, 10.0f, 0.05f);

    EXPECT_LT(delta_high, delta_low);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, STDPDeltaWHandlesNullContext) {
    /* WHAT: Verify NULL context returns zero */
    float delta_w = snn_stdp_compute_delta_w(nullptr, 10.0f, 0.5f);
    EXPECT_EQ(delta_w, 0.0f);
}

//=============================================================================
// Surrogate Gradient Tests
//=============================================================================

TEST_F(SNNTrainingTest, SurrogateGradientNonZeroNearThreshold) {
    /* WHAT: Verify gradient is non-zero near threshold */
    snn_training_ctx_t* ctx = snn_training_create_surrogate(&surrogate_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    /* At threshold (1.0), gradient should be maximum */
    float grad = snn_training_surrogate_gradient(ctx, 1.0f);
    EXPECT_GT(grad, 0.0f);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, SurrogateGradientDecaysAwayFromThreshold) {
    /* WHAT: Verify gradient decreases away from threshold */
    snn_training_ctx_t* ctx = snn_training_create_surrogate(&surrogate_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    float grad_near = snn_training_surrogate_gradient(ctx, 1.0f);   /* At threshold */
    float grad_below = snn_training_surrogate_gradient(ctx, 0.5f);  /* Below threshold */
    float grad_above = snn_training_surrogate_gradient(ctx, 1.5f);  /* Above threshold */

    EXPECT_GE(grad_near, grad_below);
    EXPECT_GE(grad_near, grad_above);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, SurrogateGradientHandlesNullContext) {
    /* WHAT: Verify NULL context returns zero */
    float grad = snn_training_surrogate_gradient(nullptr, 1.0f);
    EXPECT_EQ(grad, 0.0f);
}

TEST_F(SNNTrainingTest, SurrogateBackwardComputesInputGrad) {
    /* WHAT: Verify backward pass computes input gradients */
    snn_training_ctx_t* ctx = snn_training_create_surrogate(&surrogate_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    float output_grad[] = {1.0f, 0.5f, -0.5f};
    float membrane_v[] = {0.8f, 1.0f, 1.2f};
    float input_grad[3] = {0};

    int result = snn_surrogate_backward(ctx, output_grad, membrane_v, 3, input_grad);
    EXPECT_EQ(result, SNN_SUCCESS);

    /* Input grad should be non-zero for non-zero output grad */
    EXPECT_NE(input_grad[0], 0.0f);
    EXPECT_NE(input_grad[1], 0.0f);
    EXPECT_NE(input_grad[2], 0.0f);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, SurrogateBackwardRejectsNullPointers) {
    /* WHAT: Verify backward rejects NULL pointers */
    snn_training_ctx_t* ctx = snn_training_create_surrogate(&surrogate_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    float output_grad[] = {1.0f};
    float membrane_v[] = {1.0f};
    float input_grad[1];

    EXPECT_EQ(snn_surrogate_backward(nullptr, output_grad, membrane_v, 1, input_grad),
              SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_surrogate_backward(ctx, nullptr, membrane_v, 1, input_grad),
              SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_surrogate_backward(ctx, output_grad, nullptr, 1, input_grad),
              SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_surrogate_backward(ctx, output_grad, membrane_v, 1, nullptr),
              SNN_ERROR_NULL_POINTER);

    snn_training_destroy(ctx);
}

//=============================================================================
// R-STDP and eProp Update Tests
//=============================================================================

TEST_F(SNNTrainingTest, RSTDPSetRewardStoresValue) {
    /* WHAT: Verify reward value is stored */
    snn_training_ctx_t* ctx = snn_training_create_rstdp(&rstdp_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    snn_rstdp_set_reward(ctx, 1.5f);
    EXPECT_NEAR(ctx->reward, 1.5f, 0.001f);

    snn_rstdp_set_reward(ctx, -0.5f);
    EXPECT_NEAR(ctx->reward, -0.5f, 0.001f);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, RSTDPSetRewardHandlesNullContext) {
    /* WHAT: Verify NULL context handled safely */
    snn_rstdp_set_reward(nullptr, 1.0f);
    SUCCEED();
}

TEST_F(SNNTrainingTest, RSTDPUpdateEligibilityHandlesNullContext) {
    /* WHAT: Verify NULL context handled safely */
    snn_rstdp_update_eligibility(nullptr, 1.0f);
    SUCCEED();
}

TEST_F(SNNTrainingTest, EpropUpdateEligibilityHandlesNullContext) {
    /* WHAT: Verify NULL context handled safely */
    uint8_t pre[4] = {1, 0, 1, 0};
    uint8_t post[4] = {0, 1, 0, 1};
    snn_eprop_update_eligibility(nullptr, pre, post, 1.0f);
    SUCCEED();
}

//=============================================================================
// Training Reset Tests
//=============================================================================

TEST_F(SNNTrainingTest, TrainingResetZerosState) {
    /* WHAT: Verify reset clears all state */
    snn_training_ctx_t* ctx = snn_training_create_rstdp(&rstdp_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    /* Set some state */
    ctx->reward = 1.5f;
    ctx->current_loss = 0.5f;
    ctx->smoothed_loss = 0.4f;

    /* Reset */
    snn_training_reset(ctx);

    /* Verify state cleared */
    EXPECT_EQ(ctx->reward, 0.0f);
    EXPECT_EQ(ctx->current_loss, 0.0f);
    EXPECT_EQ(ctx->smoothed_loss, 0.0f);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, TrainingResetHandlesNullContext) {
    /* WHAT: Verify NULL context handled safely */
    snn_training_reset(nullptr);
    SUCCEED();
}

TEST_F(SNNTrainingTest, TrainingResetStatsHandlesNullContext) {
    /* WHAT: Verify NULL context handled safely */
    snn_training_reset_stats(nullptr);
    SUCCEED();
}

//=============================================================================
// Training Statistics Tests
//=============================================================================

TEST_F(SNNTrainingTest, GetStatsHandlesNullContext) {
    /* WHAT: Verify NULL context handled safely */
    uint64_t updates, steps;
    float delta;
    snn_training_get_stats(nullptr, &updates, &steps, &delta);
    SUCCEED();
}

TEST_F(SNNTrainingTest, GetStatsHandlesNullOutputs) {
    /* WHAT: Verify NULL outputs handled safely */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);

    snn_training_get_stats(ctx, nullptr, nullptr, nullptr);
    SUCCEED();

    snn_training_destroy(ctx);
}

//=============================================================================
// Integration with Network Tests
//=============================================================================

TEST_F(SNNTrainingTest, STDPApplyNetworkReturnsZeroForNull) {
    /* WHAT: Verify NULL pointers return 0 */
    snn_training_ctx_t* ctx = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx, nullptr);

    uint32_t count = snn_stdp_apply_network(ctx, nullptr, 0.0f);
    EXPECT_EQ(count, 0u);

    count = snn_stdp_apply_network(nullptr, nullptr, 0.0f);
    EXPECT_EQ(count, 0u);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, RSTDPApplyReturnsZeroForNull) {
    /* WHAT: Verify NULL pointers return 0 */
    snn_training_ctx_t* ctx = snn_training_create_rstdp(&rstdp_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    uint32_t count = snn_rstdp_apply(ctx, nullptr);
    EXPECT_EQ(count, 0u);

    count = snn_rstdp_apply(nullptr, nullptr);
    EXPECT_EQ(count, 0u);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, EpropApplyReturnsZeroForNull) {
    /* WHAT: Verify NULL pointers return 0 */
    snn_training_ctx_t* ctx = snn_training_create_eprop(&eprop_config, 10, 5);
    ASSERT_NE(ctx, nullptr);

    uint32_t count = snn_eprop_apply(ctx, nullptr, 1.0f);
    EXPECT_EQ(count, 0u);

    count = snn_eprop_apply(nullptr, nullptr, 1.0f);
    EXPECT_EQ(count, 0u);

    snn_training_destroy(ctx);
}

TEST_F(SNNTrainingTest, HomeostaticApplyReturnsZeroForNull) {
    /* WHAT: Verify NULL pointers return 0 */
    uint32_t count = snn_homeostatic_apply(nullptr, nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(SNNTrainingTest, HomeostaticUpdateRatesHandlesNull) {
    /* WHAT: Verify NULL pointers handled safely */
    uint8_t spikes[10] = {0};
    snn_homeostatic_update_rates(nullptr, spikes, 10, 1.0f);
    SUCCEED();
}
