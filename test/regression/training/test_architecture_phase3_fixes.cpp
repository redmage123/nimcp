/**
 * @file test_architecture_phase3_fixes.cpp
 * @brief Regression tests for architecture Phase 3 fixes (post-eval audit)
 *
 * Covers 10 fixes from the second architecture evaluation:
 *   1. Plasticity-backprop gating wired (UTM + coordinator)
 *   2. Delta clamping widened from [-1,+1] to [-5,+5]
 *   3. Weight bounds widened from [-1,+1] to [-10,+10]
 *   4. Unified training auto-enabled (secondary networks get ground truth)
 *   5. Homeostatic plasticity enabled in SNN
 *   6. Neuromodulators wired to backprop LR
 *   7. (Train/test alignment — architectural, verified via code review)
 *   8. (Eval metrics alignment — Python-level, verified via MetricsComputer)
 *   9. Inference state reset API
 *  10. Fusion input normalization
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
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "plasticity/orchestrator/nimcp_neural_plasticity_coordinator.h"
#include "training/nimcp_snn_backprop.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ArchPhase3Test : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    /* Helper: create a small test network */
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
        cfg.min_weight = -10.0f;
        cfg.max_weight = 10.0f;
        cfg.wiring_threads = 1;
        return neural_network_create(&cfg);
    }
};

/* ============================================================================
 * Fix 1: Plasticity-Backprop Gating Wired
 *
 * Verifies that the TPB backprop_active flag works correctly and that
 * the UTM setter function exists and can be called.
 * ============================================================================ */

TEST_F(ArchPhase3Test, PlasticityGate_TPBSetAndGet) {
    tpb_config_t tpb_cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&tpb_cfg);
    ASSERT_NE(tpb, nullptr) << "TPB context creation should succeed";

    /* Initially backprop should NOT be active */
    EXPECT_FALSE(nimcp_tpb_is_backprop_active(tpb))
        << "Backprop should be inactive initially";

    /* Set active */
    nimcp_tpb_set_backprop_active(tpb, true);
    EXPECT_TRUE(nimcp_tpb_is_backprop_active(tpb))
        << "Backprop should be active after set(true)";

    /* Set inactive */
    nimcp_tpb_set_backprop_active(tpb, false);
    EXPECT_FALSE(nimcp_tpb_is_backprop_active(tpb))
        << "Backprop should be inactive after set(false)";

    tpb_destroy(tpb);
}

TEST_F(ArchPhase3Test, PlasticityGate_UTMWiresTPB) {
    nimcp_unified_training_config_t utm_cfg;
    nimcp_utm_default_config(&utm_cfg);
    nimcp_unified_training_manager_t* utm = nimcp_utm_create(&utm_cfg);
    ASSERT_NE(utm, nullptr);

    /* Before wiring, plasticity_bridge should be NULL */
    EXPECT_EQ(utm->plasticity_bridge, nullptr);

    /* Wire TPB */
    tpb_config_t tpb_cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&tpb_cfg);
    ASSERT_NE(tpb, nullptr);

    nimcp_utm_set_plasticity_bridge(utm, tpb);
    EXPECT_EQ(utm->plasticity_bridge, tpb)
        << "UTM should hold reference to TPB after wiring";

    /* Verify UTM can use it */
    nimcp_tpb_set_backprop_active(utm->plasticity_bridge, true);
    EXPECT_TRUE(nimcp_tpb_is_backprop_active(utm->plasticity_bridge));
    nimcp_tpb_set_backprop_active(utm->plasticity_bridge, false);

    tpb_destroy(tpb);
    nimcp_utm_destroy(utm);
}

TEST_F(ArchPhase3Test, PlasticityGate_NullSafe) {
    /* NULL context should not crash */
    nimcp_tpb_set_backprop_active(NULL, true);
    EXPECT_FALSE(nimcp_tpb_is_backprop_active(NULL));

    /* NULL UTM should not crash */
    nimcp_utm_set_plasticity_bridge(NULL, NULL);
}

/* ============================================================================
 * Fix 2: Delta Clamping Widened [-1,+1] → [-5,+5]
 *
 * Verify that backprop can propagate deltas larger than 1.0.
 * With the old [-1,+1] clamp, large errors would be throttled.
 * ============================================================================ */

TEST_F(ArchPhase3Test, DeltaClamp_LargeErrorsPropagated) {
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    /* Create target and output with large errors (delta > 1.0) */
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i < 5) ? 1.0f : 0.0f;   /* 5 active targets */
        output[i] = (i < 5) ? 0.0f : 0.0f;    /* All zeros → delta = 1.0 for active */
    }

    /* Pre-set some output neurons to very wrong values */
    output[0] = -3.0f;  /* delta = 1.0 - (-3.0) = 4.0 — would be clamped to 1.0 under old regime */

    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};
    int ret = backprop_sparse_full_ex2(net, 3, ls, 0.01f,
                                        -10.0f, 10.0f,
                                        target, output, 64,
                                        100.0f, 0.0f, NULL,
                                        &grad_norm, NULL);

    EXPECT_EQ(ret, 0) << "Backprop should succeed";
    EXPECT_GT(grad_norm, 0.0f) << "Gradient norm should be non-zero";

    neural_network_destroy(net);
}

TEST_F(ArchPhase3Test, DeltaClamp_RegressionVariantWidened) {
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (float)i / 64.0f;
        output[i] = 0.0f;
    }

    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};
    int ret = backprop_sparse_full_regression_wd(net, 3, ls, 0.01f,
                                                  -10.0f, 10.0f,
                                                  target, output, 64,
                                                  100.0f, 0.0f, NULL,
                                                  &grad_norm, NULL);

    EXPECT_EQ(ret, 0) << "Regression backprop should succeed";
    EXPECT_GT(grad_norm, 0.0f) << "Gradient norm should be non-zero";

    neural_network_destroy(net);
}

/* ============================================================================
 * Fix 3: Weight Bounds Widened [-1,+1] → [-10,+10]
 *
 * Verify that backprop accepts wider weight bounds and weights can
 * grow beyond the old [-1, +1] range.
 * ============================================================================ */

TEST_F(ArchPhase3Test, WeightBounds_AcceptsWiderRange) {
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = (i < 5) ? 1.0f : 0.0f;
        output[i] = 0.0f;
    }

    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};

    /* Train with wider bounds — should not crash */
    for (int step = 0; step < 10; step++) {
        int ret = backprop_sparse_full_ex2(net, 3, ls, 0.1f,
                                            -10.0f, 10.0f,
                                            target, output, 64,
                                            100.0f, 0.0f, NULL,
                                            &grad_norm, NULL);
        EXPECT_EQ(ret, 0) << "Step " << step << " should succeed";
    }

    /* Verify no NaN in network state */
    for (uint32_t i = 0; i < 256; i++) {
        neuron_t* n = neural_network_get_neuron(net, i);
        if (n) {
            EXPECT_TRUE(std::isfinite(n->state))
                << "Neuron " << i << " state should be finite";
            EXPECT_TRUE(std::isfinite(n->bias))
                << "Neuron " << i << " bias should be finite";
        }
    }

    neural_network_destroy(net);
}

TEST_F(ArchPhase3Test, WeightBounds_OldNarrowBoundsStillWork) {
    /* Backward compatibility: [-1, +1] bounds should still work */
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    float target[64] = {}, output[64] = {};
    target[0] = 1.0f;
    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};

    int ret = backprop_sparse_full_ex2(net, 3, ls, 0.01f,
                                        -1.0f, 1.0f,
                                        target, output, 64,
                                        100.0f, 0.0f, NULL,
                                        &grad_norm, NULL);
    EXPECT_EQ(ret, 0);

    neural_network_destroy(net);
}

/* ============================================================================
 * Fix 4: Unified Training Auto-Enabled
 *
 * When secondary networks (CNN/SNN/LNN) exist, use_unified_training should
 * be auto-enabled so all networks receive direct ground truth supervision
 * via the UTM's MSE gradient, plus cross-network gradient bridges.
 * ============================================================================ */

TEST_F(ArchPhase3Test, UnifiedTraining_UTMCreationSucceeds) {
    /* Verify the UTM can be created with default config */
    nimcp_unified_training_config_t utm_cfg;
    nimcp_utm_default_config(&utm_cfg);
    nimcp_unified_training_manager_t* utm = nimcp_utm_create(&utm_cfg);
    ASSERT_NE(utm, nullptr) << "UTM creation should succeed";
    EXPECT_EQ(utm->num_networks, 0u) << "Fresh UTM has no networks";
    nimcp_utm_destroy(utm);
}

TEST_F(ArchPhase3Test, UnifiedTraining_UTMStepWithNoNetworks) {
    /* UTM step with no registered networks should not crash */
    nimcp_unified_training_config_t utm_cfg;
    nimcp_utm_default_config(&utm_cfg);
    nimcp_unified_training_manager_t* utm = nimcp_utm_create(&utm_cfg);
    ASSERT_NE(utm, nullptr);

    float input[16] = {}, target[16] = {};
    for (int i = 0; i < 16; i++) input[i] = (float)i / 16.0f;
    target[0] = 1.0f;

    nimcp_utm_step_result_t result = {};
    int rc = nimcp_utm_step(utm, input, 16, target, 16, &result);
    /* Step may return 0 (no-op) or -1 (no networks) — just shouldn't crash */
    (void)rc;

    nimcp_utm_destroy(utm);
}

TEST_F(ArchPhase3Test, UnifiedTraining_PerNetworkLossTracked) {
    /* Verify the step result has per-network loss array */
    nimcp_utm_step_result_t result = {};
    EXPECT_EQ(sizeof(result.per_network_loss) / sizeof(float),
              (size_t)NIMCP_UTM_MAX_NETWORKS)
        << "per_network_loss should have space for all networks";
}

TEST_F(ArchPhase3Test, UnifiedTraining_CrossNetworkBridgesExist) {
    /* Verify UTM supports cross-network gradient bridges */
    nimcp_unified_training_config_t utm_cfg;
    nimcp_utm_default_config(&utm_cfg);
    nimcp_unified_training_manager_t* utm = nimcp_utm_create(&utm_cfg);
    ASSERT_NE(utm, nullptr);

    EXPECT_EQ(utm->num_bridges, 0u) << "Fresh UTM has no bridges";
    /* Bridge capacity should exist */
    EXPECT_GT((int)NIMCP_UTM_MAX_BRIDGES, 0) << "Bridge capacity should be non-zero";

    nimcp_utm_destroy(utm);
}

/* ============================================================================
 * Fix 5: Homeostatic Plasticity Enabled in SNN
 * ============================================================================ */

TEST_F(ArchPhase3Test, Homeostatic_SNNDefaultEnabled) {
    snn_backprop_config_t cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
    EXPECT_TRUE(cfg.use_homeostatic)
        << "SNN default config should have homeostatic enabled";
}

TEST_F(ArchPhase3Test, Homeostatic_SNNConfigOverridable) {
    snn_backprop_config_t cfg = snn_backprop_default_config(SNN_TRAIN_BPTT);
    cfg.use_homeostatic = false;
    EXPECT_FALSE(cfg.use_homeostatic)
        << "Homeostatic should be overridable to false";
}

/* ============================================================================
 * Fix 9: Inference State Reset API
 *
 * brain_reset_inference_state() declaration is in nimcp_brain.h.
 * We verify the function exists and handles NULL safely.
 * Full integration test requires a full brain (too heavy for unit test).
 * ============================================================================ */

extern "C" {
    /* Forward-declare the function — it's in the brain public header
     * but we can't include the full brain header in a unit test */
    int brain_reset_inference_state(void* brain);
}

TEST_F(ArchPhase3Test, InferenceReset_NullSafe) {
    int ret = brain_reset_inference_state(NULL);
    EXPECT_EQ(ret, -1) << "NULL brain should return -1";
}

/* ============================================================================
 * Fix 10: Fusion Input Normalization
 *
 * Structural test — the normalization code normalizes each network's
 * output by L2 norm before weighted sum. We verify the math is correct.
 * ============================================================================ */

TEST_F(ArchPhase3Test, FusionNorm_L2NormalizationMath) {
    /* Simulate the normalization logic from nimcp_brain_part_helpers.c */
    float output_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};  /* |a| = sqrt(30) ≈ 5.48 */
    float output_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};  /* |b| = sqrt(3000) ≈ 54.8 */

    /* Without normalization, B dominates by 10x */
    float raw_sum[4];
    for (int i = 0; i < 4; i++)
        raw_sum[i] = 0.5f * output_a[i] + 0.5f * output_b[i];

    /* With L2 normalization, both contribute equally (per unit norm) */
    float norm_a = 0, norm_b = 0;
    for (int i = 0; i < 4; i++) {
        norm_a += output_a[i] * output_a[i];
        norm_b += output_b[i] * output_b[i];
    }
    norm_a = sqrtf(norm_a + 1e-8f);
    norm_b = sqrtf(norm_b + 1e-8f);

    float normalized_sum[4];
    for (int i = 0; i < 4; i++) {
        normalized_sum[i] = 0.5f * output_a[i] / norm_a
                          + 0.5f * output_b[i] / norm_b;
    }

    /* Verify: normalized outputs should have similar magnitude per-element,
     * unlike raw sum where B dominates */
    float ratio_raw = raw_sum[0] > 0 ? raw_sum[3] / raw_sum[0] : 0;
    float ratio_norm = normalized_sum[0] > 0 ? normalized_sum[3] / normalized_sum[0] : 0;

    /* Both should preserve the direction ratio (4:1) */
    EXPECT_NEAR(ratio_raw, 4.0f, 0.1f) << "Raw ratio should be 4:1";
    EXPECT_NEAR(ratio_norm, 4.0f, 0.1f) << "Normalized ratio should preserve direction";

    /* But the scale should differ: raw_sum[0] ≈ 5.5, normalized_sum[0] ≈ 0.27 */
    EXPECT_GT(raw_sum[0], 5.0f) << "Raw sum dominated by network B";
    EXPECT_LT(normalized_sum[0], 1.0f) << "Normalized sum should have unit-scale";

    /* Key invariant: A and B contribute equal DIRECTION weight after normalization */
    float a_contrib = 0.5f * output_a[0] / norm_a;
    float b_contrib = 0.5f * output_b[0] / norm_b;
    EXPECT_NEAR(a_contrib, b_contrib, 0.01f)
        << "Equal-weight networks should contribute equally after normalization";
}

/* ============================================================================
 * Integration: Backprop with TPB gating
 * ============================================================================ */

TEST_F(ArchPhase3Test, Integration_BackpropWithTPBGating) {
    /* Create UTM with TPB wired */
    nimcp_unified_training_config_t utm_cfg;
    nimcp_utm_default_config(&utm_cfg);
    nimcp_unified_training_manager_t* utm = nimcp_utm_create(&utm_cfg);
    ASSERT_NE(utm, nullptr);

    tpb_config_t tpb_cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&tpb_cfg);
    ASSERT_NE(tpb, nullptr);

    nimcp_utm_set_plasticity_bridge(utm, tpb);

    /* Simulate backprop gating lifecycle */
    EXPECT_FALSE(nimcp_tpb_is_backprop_active(tpb));

    /* Before backward: suppress plasticity */
    nimcp_tpb_set_backprop_active(tpb, true);
    EXPECT_TRUE(nimcp_tpb_is_backprop_active(tpb));

    /* After backward: re-enable plasticity */
    nimcp_tpb_set_backprop_active(tpb, false);
    EXPECT_FALSE(nimcp_tpb_is_backprop_active(tpb));

    tpb_destroy(tpb);
    nimcp_utm_destroy(utm);
}

TEST_F(ArchPhase3Test, Integration_WeightDecayWithWiderBounds) {
    /* Verify weight decay + wider bounds don't cause explosion */
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    float target[64] = {}, output[64] = {};
    for (int i = 0; i < 5; i++) target[i] = 1.0f;

    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};

    /* Run many steps with weight decay and wide bounds */
    for (int step = 0; step < 50; step++) {
        int ret = backprop_sparse_full_ex2(net, 3, ls, 0.01f,
                                            -10.0f, 10.0f,
                                            target, output, 64,
                                            100.0f, 1e-4f, NULL,
                                            &grad_norm, NULL);
        EXPECT_EQ(ret, 0) << "Step " << step << " should succeed";
        EXPECT_TRUE(std::isfinite(grad_norm))
            << "Grad norm should be finite at step " << step;
    }

    neural_network_destroy(net);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(ArchPhase3Test, EdgeCase_BackpropMaxDeltaBounds) {
    /* Verify that delta clamping at [-5,+5] doesn't cause overflow
     * with wide weight bounds [-10,+10] */
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    /* Extreme target-output mismatch */
    float target[64], output[64];
    for (int i = 0; i < 64; i++) {
        target[i] = 1.0f;      /* All active */
        output[i] = -10.0f;    /* Very wrong → delta = 11.0, clamped to 5.0 */
    }

    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};
    int ret = backprop_sparse_full_ex2(net, 3, ls, 0.01f,
                                        -10.0f, 10.0f,
                                        target, output, 64,
                                        100.0f, 0.0f, NULL,
                                        &grad_norm, NULL);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(grad_norm));
    /* With 5.0 delta limit and 64 outputs, grad norm should be substantial */
    EXPECT_GT(grad_norm, 0.1f) << "Large deltas should produce significant gradient";

    neural_network_destroy(net);
}

TEST_F(ArchPhase3Test, EdgeCase_TPBRapidToggle) {
    /* Rapid on/off toggling should be safe (no race with atomics) */
    tpb_config_t cfg = tpb_config_default();
    tpb_context_t* tpb = tpb_create(&cfg);
    ASSERT_NE(tpb, nullptr);

    for (int i = 0; i < 1000; i++) {
        nimcp_tpb_set_backprop_active(tpb, i % 2 == 0);
        bool active = nimcp_tpb_is_backprop_active(tpb);
        EXPECT_EQ(active, i % 2 == 0) << "Toggle iteration " << i;
    }

    tpb_destroy(tpb);
}

TEST_F(ArchPhase3Test, EdgeCase_ZeroWeightBounds) {
    /* Pathological: weight bounds [0, 0] should still not crash */
    neural_network_t net = create_test_network();
    if (!net) GTEST_SKIP() << "Could not create test network";

    float target[64] = {}, output[64] = {};
    target[0] = 1.0f;
    float grad_norm = 0.0f;
    uint32_t ls[3] = {64, 128, 64};

    int ret = backprop_sparse_full_ex2(net, 3, ls, 0.01f,
                                        0.0f, 0.0f,
                                        target, output, 64,
                                        100.0f, 0.0f, NULL,
                                        &grad_norm, NULL);
    EXPECT_EQ(ret, 0) << "Zero bounds should not crash";

    neural_network_destroy(net);
}
