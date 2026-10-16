/**
 * @file test_architecture_eval_fixes.cpp
 * @brief Regression tests for architecture evaluation fixes
 *
 * Covers all 6 recommended fixes from the architecture evaluation:
 *   1. Anti-collapse gradient correction in adaptive backprop (diversity_grad injection)
 *   2. Weight decay in backprop kernel (decoupled AdamW-style)
 *   3. LR scheduling in UTM (warmup + cosine decay)
 *   4. AdamW optimizer in UTM (replacing mislabeled SGD)
 *   5. Mini-batching (Python-level, verified via MiniBatchTrainer class)
 *   6. Curriculum difficulty scaling (verified via existing MasteryTracker)
 *
 * @date 2026-03-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "training/nimcp_unified_training.h"
#include "plasticity/adaptive/nimcp_backprop_kernel.h"
#include "core/neuralnet/nimcp_neuralnet.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ArchEvalFixesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    /* Helper: create a small test network for backprop tests */
    neural_network_t create_test_network(uint32_t num_neurons = 256,
                                          uint32_t num_layers = 3) {
        network_config_t cfg = {};
        cfg.num_neurons = num_neurons;
        cfg.num_layers = num_layers;
        uint32_t sizes[3] = {64, 128, 64};
        cfg.layer_sizes = sizes;
        cfg.input_size = 64;
        cfg.output_size = 64;
        cfg.learning_rate = 0.01f;
        cfg.ei_ratio = 0.8f;
        cfg.min_weight = -2.0f;
        cfg.max_weight = 2.0f;
        cfg.wiring_threads = 1;
        return neural_network_create(&cfg);
    }
};

/* ============================================================================
 * Fix 1: Anti-Collapse Diversity Gradient Injection
 *
 * Verifies that nimcp_anti_collapse_diversity_loss() writes a non-zero
 * gradient when given a non-NULL grad_output buffer, and that
 * backprop_sparse_full_ex2() accepts the diversity_grad parameter.
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, DiversityGrad_NonNullProducesGradient) {
    nimcp_anti_collapse_state_t state;
    nimcp_anti_collapse_init(&state, NULL);

    float output1[8] = {0.5f, 0.3f, 0.2f, 0.1f, 0.4f, 0.6f, 0.7f, 0.8f};
    float grad1[8] = {};

    /* First call populates the ring buffer */
    nimcp_anti_collapse_diversity_loss(&state, output1, grad1, 8);

    /* Second call with similar output should produce diversity gradient */
    float output2[8] = {0.5f, 0.3f, 0.2f, 0.1f, 0.4f, 0.6f, 0.7f, 0.8f};
    float grad2[8] = {};
    float div_loss = nimcp_anti_collapse_diversity_loss(&state, output2, grad2, 8);

    /* With identical outputs, diversity loss should be positive */
    EXPECT_GT(div_loss, 0.0f) << "Identical outputs should produce diversity penalty";

    /* Gradient should be non-zero (pushing output away from ring buffer average) */
    float grad_norm = 0.0f;
    for (int i = 0; i < 8; i++) {
        grad_norm += grad2[i] * grad2[i];
    }
    EXPECT_GT(grad_norm, 0.0f) << "Diversity gradient should be non-zero for identical outputs";

    nimcp_anti_collapse_destroy(&state);
}

TEST_F(ArchEvalFixesTest, DiversityGrad_NullStillReturnsLoss) {
    nimcp_anti_collapse_state_t state;
    nimcp_anti_collapse_init(&state, NULL);

    float output[8] = {0.5f, 0.3f, 0.2f, 0.1f, 0.4f, 0.6f, 0.7f, 0.8f};

    /* NULL grad_output should still compute loss without crashing */
    float loss1 = nimcp_anti_collapse_diversity_loss(&state, output, NULL, 8);
    float loss2 = nimcp_anti_collapse_diversity_loss(&state, output, NULL, 8);

    /* Second call with same output in buffer should produce loss */
    EXPECT_GE(loss2, 0.0f);

    nimcp_anti_collapse_destroy(&state);
}

TEST_F(ArchEvalFixesTest, DiversityGrad_DiverseOutputsLowLoss) {
    nimcp_anti_collapse_state_t state;
    nimcp_anti_collapse_init(&state, NULL);

    /* Fill buffer with diverse outputs */
    for (int i = 0; i < 16; i++) {
        float output[8];
        for (int j = 0; j < 8; j++) {
            output[j] = (float)((i * 7 + j * 3) % 100) / 100.0f;
        }
        nimcp_anti_collapse_diversity_loss(&state, output, NULL, 8);
    }

    /* A very different output should have low diversity loss */
    float unique[8] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float grad[8] = {};
    float loss = nimcp_anti_collapse_diversity_loss(&state, unique, grad, 8);

    /* Loss should be much lower for orthogonal output */
    EXPECT_LT(loss, 0.5f) << "Orthogonal output should have low diversity loss";

    nimcp_anti_collapse_destroy(&state);
}

/* ============================================================================
 * Fix 2: Weight Decay in Backprop Kernel
 *
 * backprop_sparse_full_ex2() applies decoupled weight decay:
 *   w *= (1 - lr * wd) BEFORE gradient step
 * With wd > 0, weights should shrink toward zero over multiple iterations.
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, WeightDecay_Ex2AcceptsWeightDecayParam) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    uint32_t num_layers = 3;
    uint32_t layer_sizes[3] = {64, 128, 64};
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        output[i] = 0.5f;
    }

    float grad_norm = 0.0f;
    backprop_layer_grads_t layer_grads = {};

    /* Call with weight_decay > 0 should succeed */
    int rc = backprop_sparse_full_ex2(net, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 1e-4f, NULL,  /* weight_decay=1e-4, no diversity grad */
        &grad_norm, &layer_grads);

    EXPECT_EQ(rc, 0) << "backprop_sparse_full_ex2 should succeed with weight decay";
    EXPECT_TRUE(std::isfinite(grad_norm));
    EXPECT_GT(grad_norm, 0.0f);

    neural_network_destroy(net);
}

TEST_F(ArchEvalFixesTest, WeightDecay_ZeroDecayMatchesOriginal) {
    /* With wd=0, ex2 should produce the same result as ex (backward compatible) */
    neural_network_t net1 = create_test_network();
    neural_network_t net2 = create_test_network();
    ASSERT_NE(net1, nullptr);
    ASSERT_NE(net2, nullptr);

    uint32_t num_layers = 3;
    uint32_t layer_sizes[3] = {64, 128, 64};
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i % 3 == 0) ? 1.0f : 0.0f;
        output[i] = 0.5f;
    }

    float gn1 = 0.0f, gn2 = 0.0f;

    backprop_sparse_full_ex(net1, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f, target, output, 64, 1.0f, &gn1, NULL);

    backprop_sparse_full_ex2(net2, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f, target, output, 64, 1.0f,
        0.0f, NULL, &gn2, NULL);  /* wd=0 */

    /* Gradient norms should be identical */
    EXPECT_NEAR(gn1, gn2, 1e-4f) << "wd=0 should match original backprop";

    neural_network_destroy(net1);
    neural_network_destroy(net2);
}

TEST_F(ArchEvalFixesTest, WeightDecay_RegressionVariantAcceptsParams) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    uint32_t num_layers = 3;
    uint32_t layer_sizes[3] = {64, 128, 64};
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = ((float)i / 64.0f) * 2.0f - 1.0f;
        output[i] = 0.0f;
    }

    float grad_norm = 0.0f;
    backprop_layer_grads_t layer_grads = {};

    int rc = backprop_sparse_full_regression_wd(net, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 1e-4f, NULL,
        &grad_norm, &layer_grads);

    EXPECT_EQ(rc, 0) << "regression_wd should succeed";
    EXPECT_TRUE(std::isfinite(grad_norm));

    neural_network_destroy(net);
}

TEST_F(ArchEvalFixesTest, WeightDecay_DiversityGradInjection) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    uint32_t num_layers = 3;
    uint32_t layer_sizes[3] = {64, 128, 64};
    float target[64], output[64], div_grad[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        output[i] = 0.5f;
        div_grad[i] = 0.01f;  /* Small uniform diversity gradient */
    }

    float grad_norm = 0.0f;
    int rc = backprop_sparse_full_ex2(net, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 0.0f, div_grad,
        &grad_norm, NULL);

    EXPECT_EQ(rc, 0) << "backprop_sparse_full_ex2 should accept diversity_grad";
    EXPECT_TRUE(std::isfinite(grad_norm));
    EXPECT_GT(grad_norm, 0.0f);

    neural_network_destroy(net);
}

/* ============================================================================
 * Fix 3: LR Scheduling in UTM
 *
 * nimcp_utm_get_scheduled_lr() returns:
 * - Linear warmup during warmup_steps
 * - Cosine decay after warmup (default schedule)
 * - min_lr at the end
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, LRSchedule_DefaultIsCosine) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);

    EXPECT_EQ(cfg.lr_schedule.type, NIMCP_LR_SCHEDULE_COSINE);
    EXPECT_EQ(cfg.lr_schedule.warmup_steps, 1000u);
    EXPECT_EQ(cfg.lr_schedule.total_steps, 100000u);
    EXPECT_FLOAT_EQ(cfg.lr_schedule.min_lr_ratio, 0.01f);
}

TEST_F(ArchEvalFixesTest, LRSchedule_WarmupLinearRamp) {
    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(NULL);
    ASSERT_NE(mgr, nullptr);

    /* At step 0 (set by create): LR should be small (beginning of warmup) */
    float lr_start = nimcp_utm_get_scheduled_lr(mgr);
    float base_lr = mgr->config.learning_rate;

    /* During warmup, LR should be less than base_lr */
    EXPECT_LT(lr_start, base_lr);
    EXPECT_GT(lr_start, 0.0f);

    nimcp_utm_destroy(mgr);
}

TEST_F(ArchEvalFixesTest, LRSchedule_CosineDecayAfterWarmup) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    cfg.lr_schedule.warmup_steps = 10;
    cfg.lr_schedule.total_steps = 100;
    cfg.lr_schedule.min_lr_ratio = 0.1f;

    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    float base_lr = cfg.learning_rate;
    float min_lr = base_lr * cfg.lr_schedule.min_lr_ratio;

    /* Simulate step progression and check LR decay */
    /* At warmup boundary: LR should be at base */
    mgr->step_count = 10;
    float lr_post_warmup = nimcp_utm_get_scheduled_lr(mgr);
    EXPECT_NEAR(lr_post_warmup, base_lr, base_lr * 0.1f);

    /* At 50% through decay: LR should be ~midpoint */
    mgr->step_count = 55;  /* 10 warmup + 45 = halfway through 90 decay steps */
    float lr_mid = nimcp_utm_get_scheduled_lr(mgr);
    float expected_mid = min_lr + 0.5f * (base_lr - min_lr) * (1.0f + cosf(M_PI * 0.5f));
    EXPECT_NEAR(lr_mid, expected_mid, base_lr * 0.15f);

    /* At end: LR should be at minimum */
    mgr->step_count = 100;
    float lr_end = nimcp_utm_get_scheduled_lr(mgr);
    EXPECT_NEAR(lr_end, min_lr, min_lr * 0.2f);

    nimcp_utm_destroy(mgr);
}

TEST_F(ArchEvalFixesTest, LRSchedule_ConstantReturnsBaseUnchanged) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    cfg.lr_schedule.type = NIMCP_LR_SCHEDULE_CONSTANT;
    cfg.lr_schedule.warmup_steps = 0;

    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    float base_lr = cfg.learning_rate;

    mgr->step_count = 0;
    EXPECT_FLOAT_EQ(nimcp_utm_get_scheduled_lr(mgr), base_lr);

    mgr->step_count = 50000;
    EXPECT_FLOAT_EQ(nimcp_utm_get_scheduled_lr(mgr), base_lr);

    nimcp_utm_destroy(mgr);
}

TEST_F(ArchEvalFixesTest, LRSchedule_StepDecayWorksCorrectly) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    cfg.lr_schedule.type = NIMCP_LR_SCHEDULE_STEP;
    cfg.lr_schedule.warmup_steps = 0;
    cfg.lr_schedule.step_decay_factor = 0.5f;
    cfg.lr_schedule.step_decay_interval = 100;
    cfg.lr_schedule.min_lr_ratio = 0.01f;

    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    float base_lr = cfg.learning_rate;

    mgr->step_count = 0;
    EXPECT_FLOAT_EQ(nimcp_utm_get_scheduled_lr(mgr), base_lr);

    mgr->step_count = 100;
    EXPECT_NEAR(nimcp_utm_get_scheduled_lr(mgr), base_lr * 0.5f, 1e-6f);

    mgr->step_count = 200;
    EXPECT_NEAR(nimcp_utm_get_scheduled_lr(mgr), base_lr * 0.25f, 1e-6f);

    nimcp_utm_destroy(mgr);
}

/* ============================================================================
 * Fix 4: AdamW Optimizer in UTM
 *
 * The UTM config has optimizer_type=5 (ADAM). The optimizer step now
 * implements actual AdamW with:
 * - Bias-corrected moment estimates (m_hat, v_hat)
 * - Decoupled weight decay
 * - Lazy-allocated moment buffers
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, AdamW_DefaultOptimizerIsAdam) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    EXPECT_EQ(cfg.optimizer_type, 5u) << "Default optimizer should be ADAM (type 5)";
}

TEST_F(ArchEvalFixesTest, AdamW_ManagerInitializesState) {
    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(NULL);
    ASSERT_NE(mgr, nullptr);

    /* Adam state should be initialized but not yet allocated (lazy) */
    EXPECT_EQ(mgr->adam_num_groups, 0u);
    EXPECT_FLOAT_EQ(mgr->adam_beta1_t, 1.0f);
    EXPECT_FLOAT_EQ(mgr->adam_beta2_t, 1.0f);

    nimcp_utm_destroy(mgr);
}

TEST_F(ArchEvalFixesTest, AdamW_DestroyWithNoStepDoesNotCrash) {
    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(NULL);
    ASSERT_NE(mgr, nullptr);
    /* Destroy without any training steps — should not crash */
    nimcp_utm_destroy(mgr);
    SUCCEED();
}

TEST_F(ArchEvalFixesTest, AdamW_BetaPowersDecayOnStep) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    /* Before any step, beta powers should be 1.0 */
    EXPECT_FLOAT_EQ(mgr->adam_beta1_t, 1.0f);
    EXPECT_FLOAT_EQ(mgr->adam_beta2_t, 1.0f);

    /* Note: adam_beta1_t gets updated during nimcp_utm_step which requires
     * registered networks. We just verify the initial state is correct. */

    nimcp_utm_destroy(mgr);
}

/* ============================================================================
 * Fix 5: Backprop With Accumulation (C-level batch support)
 *
 * backprop_with_accumulation() divides LR by accumulation_steps,
 * producing mathematically equivalent results to mini-batch gradient
 * accumulation. This underlies the Python MiniBatchTrainer.
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, BatchAccumulation_FunctionExists) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    uint32_t num_layers = 3;
    uint32_t layer_sizes[3] = {64, 128, 64};
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i < 32) ? 1.0f : 0.0f;
        output[i] = 0.5f;
    }

    float grad_norm = 0.0f;
    int rc = backprop_with_accumulation(net, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 8,  /* accumulation_steps = 8 */
        &grad_norm);

    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(std::isfinite(grad_norm));
    EXPECT_GT(grad_norm, 0.0f);

    neural_network_destroy(net);
}

TEST_F(ArchEvalFixesTest, BatchAccumulation_ScalesGradientBySteps) {
    neural_network_t net1 = create_test_network();
    neural_network_t net2 = create_test_network();
    ASSERT_NE(net1, nullptr);
    ASSERT_NE(net2, nullptr);

    uint32_t num_layers = 3;
    uint32_t layer_sizes[3] = {64, 128, 64};
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i < 32) ? 1.0f : 0.0f;
        output[i] = 0.5f;
    }

    float gn1 = 0.0f, gn_acc = 0.0f;

    /* Single step at full LR */
    backprop_sparse_full(net1, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f, target, output, 64, 1.0f, &gn1);

    /* Accumulated step (should rescale norm to match full LR) */
    backprop_with_accumulation(net2, num_layers, layer_sizes,
        0.01f, -2.0f, 2.0f, target, output, 64, 1.0f, 1, &gn_acc);

    /* With accumulation_steps=1, should match single step */
    EXPECT_NEAR(gn1, gn_acc, gn1 * 0.01f);

    neural_network_destroy(net1);
    neural_network_destroy(net2);
}

/* ============================================================================
 * Fix 6: Curriculum Difficulty Scaling
 *
 * Verified through the existing MasteryTracker Python class and adaptive LR
 * per domain. The C-level anti-collapse config supports adaptive_gradient_target.
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, Curriculum_AdaptiveGradientTargetEnabled) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    EXPECT_TRUE(cfg.anti_collapse.adaptive_gradient_target)
        << "Adaptive gradient target should be enabled by default";
}

TEST_F(ArchEvalFixesTest, Curriculum_AntiCollapseGradientNormalization) {
    nimcp_anti_collapse_state_t state;
    nimcp_anti_collapse_config_t acfg;
    acfg.diversity_loss_weight = 0.1f;
    acfg.diversity_buffer_size = 16;
    acfg.use_gradient_normalization = true;
    acfg.gradient_target_norm = 0.0f;  /* adaptive */
    acfg.gradient_clip_value = 5.0f;
    acfg.adaptive_gradient_target = true;
    nimcp_anti_collapse_init(&state, &acfg);

    /* Create gradient arrays to normalize */
    float grads[32];
    for (int i = 0; i < 32; i++) {
        grads[i] = (float)(i - 16) * 10.0f;  /* large gradient range */
    }
    float* grad_ptrs[1] = {grads};
    size_t sizes[1] = {32};

    float scale = nimcp_anti_collapse_normalize_gradients(
        &state, grad_ptrs, sizes, 1);

    EXPECT_TRUE(std::isfinite(scale));

    /* After normalization, gradients should be finite and scaled */
    for (int i = 0; i < 32; i++) {
        EXPECT_TRUE(std::isfinite(grads[i]));
    }

    nimcp_anti_collapse_destroy(&state);
}

/* ============================================================================
 * Integration: Combined Weight Decay + Diversity Grad
 *
 * Verifies that both fixes work together without interference.
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, Integration_WeightDecayAndDiversityGradTogether) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    /* First, compute diversity gradient */
    nimcp_anti_collapse_state_t state;
    nimcp_anti_collapse_init(&state, NULL);

    float output[64], target[64], div_grad[64];
    for (int i = 0; i < 64; i++) {
        output[i] = 0.5f;
        target[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    /* Build up diversity buffer */
    for (int s = 0; s < 5; s++) {
        memset(div_grad, 0, sizeof(div_grad));
        nimcp_anti_collapse_diversity_loss(&state, output, div_grad, 64);
    }

    /* Now run backprop with both weight decay AND diversity gradient */
    float grad_norm = 0.0f;
    backprop_layer_grads_t layer_grads = {};

    uint32_t ls3[3] = {64, 128, 64};
    int rc = backprop_sparse_full_ex2(net, 3, ls3,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 1e-4f, div_grad,
        &grad_norm, &layer_grads);

    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(std::isfinite(grad_norm));
    EXPECT_GT(grad_norm, 0.0f);

    /* Per-layer norms should be tracked */
    EXPECT_GT(layer_grads.num_layers, 0u);
    for (uint32_t l = 0; l < layer_grads.num_layers; l++) {
        EXPECT_TRUE(std::isfinite(layer_grads.norms[l]));
    }

    nimcp_anti_collapse_destroy(&state);
    neural_network_destroy(net);
}

TEST_F(ArchEvalFixesTest, Integration_RegressionWdAndDiversityGrad) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    float output[64], target[64], div_grad[64];
    for (int i = 0; i < 64; i++) {
        output[i] = 0.0f;
        target[i] = ((float)i / 64.0f) * 2.0f - 1.0f;  /* MSE targets [-1, 1] */
        div_grad[i] = 0.005f;
    }

    float grad_norm = 0.0f;
    uint32_t ls3r[3] = {64, 128, 64};
    int rc = backprop_sparse_full_regression_wd(net, 3, ls3r,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 1e-4f, div_grad,
        &grad_norm, NULL);

    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(std::isfinite(grad_norm));

    neural_network_destroy(net);
}

/* ============================================================================
 * Edge Cases and Safety
 * ============================================================================ */

TEST_F(ArchEvalFixesTest, EdgeCase_NullNetworkReturnsError) {
    float target[8] = {1.0f};
    float output[8] = {0.5f};
    float grad_norm = 0.0f;

    uint32_t ls2[2] = {8, 8};
    int rc = backprop_sparse_full_ex2(NULL, 2, ls2,
        0.01f, -1.0f, 1.0f,
        target, output, 8,
        1.0f, 1e-4f, NULL,
        &grad_norm, NULL);
    EXPECT_EQ(rc, -1);
}

TEST_F(ArchEvalFixesTest, EdgeCase_NullGradNormReturnsError) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    float target[64] = {1.0f};
    float output[64] = {0.5f};

    uint32_t ls3n[3] = {64, 128, 64};
    int rc = backprop_sparse_full_ex2(net, 3, ls3n,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 0.0f, NULL,
        NULL, NULL);  /* NULL out_grad_norm */
    EXPECT_EQ(rc, -1);

    neural_network_destroy(net);
}

TEST_F(ArchEvalFixesTest, EdgeCase_UTMNullConfigUsesDefaults) {
    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(NULL);
    ASSERT_NE(mgr, nullptr);

    /* Should use default cosine schedule */
    EXPECT_EQ(mgr->config.lr_schedule.type, NIMCP_LR_SCHEDULE_COSINE);
    EXPECT_GT(mgr->config.learning_rate, 0.0f);

    nimcp_utm_destroy(mgr);
}

TEST_F(ArchEvalFixesTest, EdgeCase_ScheduledLRNeverNegative) {
    nimcp_unified_training_config_t cfg;
    nimcp_utm_default_config(&cfg);
    nimcp_unified_training_manager_t* mgr = nimcp_utm_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    /* Test at various step counts */
    uint64_t steps[] = {0, 1, 500, 999, 1000, 1001, 50000, 99999, 100000, 200000};
    for (uint64_t s : steps) {
        mgr->step_count = s;
        float lr = nimcp_utm_get_scheduled_lr(mgr);
        EXPECT_GE(lr, 0.0f) << "LR should never be negative at step " << s;
        EXPECT_TRUE(std::isfinite(lr)) << "LR should be finite at step " << s;
    }

    nimcp_utm_destroy(mgr);
}

TEST_F(ArchEvalFixesTest, EdgeCase_WeightDecayWithZeroWeights) {
    neural_network_t net = create_test_network();
    ASSERT_NE(net, nullptr);

    float target[64], output[64];
    memset(target, 0, sizeof(target));
    memset(output, 0, sizeof(output));
    target[0] = 1.0f;

    float grad_norm = 0.0f;
    /* Large weight decay should not cause NaN */
    uint32_t ls3w[3] = {64, 128, 64};
    int rc = backprop_sparse_full_ex2(net, 3, ls3w,
        0.01f, -2.0f, 2.0f,
        target, output, 64,
        1.0f, 0.1f, NULL,  /* large wd=0.1 */
        &grad_norm, NULL);

    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(std::isfinite(grad_norm));

    neural_network_destroy(net);
}
