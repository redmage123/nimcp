/**
 * @file test_lnn_substrate_integration.cpp
 * @brief Integration tests for the LNN substrate adapter in realistic
 *        multi-step forward runs.
 *
 * WHAT: Exercises lnn_network_attach_substrate + lnn_forward_step over
 *       extended trajectories, verifying (a) a substrate with ramping
 *       ATP + declining membrane integrity produces a materially
 *       different output trajectory than the no-substrate baseline,
 *       and (b) with every substrate knob OFF the forward path is
 *       bit-identical to the no-attach baseline (regression safety net).
 * WHY:  Unit tests isolate the effects cache + tunables; integration is
 *       where composition of substrate modulation with the learned LNN
 *       dynamics is verified over many steps. The adapter must modulate
 *       observable outputs when "on" and be a no-op when "off".
 * HOW:  Google Test. Constructs a small LNN, forces the CPU LTC path,
 *       and compares trajectories across substrate regimes.
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_layer.h"
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_training.h"
#include "utils/tensor/nimcp_tensor.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/substrate/nimcp_substrate_effects.h"
}

/*============================================================================
 * Fixture. Saves/restores all substrate tunables and helper builders.
 *==========================================================================*/
class LNNSubstrateIntegrationTest : public ::testing::Test {
protected:
    float saved_enabled         = 0.0f;
    float saved_period          = 0.0f;
    float saved_tau_compose_on  = 0.0f;

    void SetUp() override {
        saved_enabled        = lnn_tune_get_substrate_enabled();
        saved_period         = lnn_tune_get_substrate_update_period();
        saved_tau_compose_on = lnn_tune_get_substrate_tau_compose_on();

        /* Init library once (idempotent). */
        (void)lnn_init(1);
    }

    void TearDown() override {
        lnn_tune_set_substrate_enabled(saved_enabled);
        lnn_tune_set_substrate_update_period(saved_period);
        lnn_tune_set_substrate_tau_compose_on(saved_tau_compose_on);
    }

    /* Build a 1-layer LNN identical to the unit-test builder: tanh 8
     * neurons, FULL wiring, EULER ODE, seeded for determinism.
     * Forces CPU LTC path by clearing GPU pointers. */
    lnn_network_t* MakeNetwork() {
        lnn_config_t config;
        memset(&config, 0, sizeof(config));
        if (lnn_config_default(&config) != NIMCP_SUCCESS) return nullptr;
        config.n_inputs = 4;
        config.n_outputs = 8;
        config.n_layers = 1;
        config.default_dt = 1.0f;
        config.layer_configs = (lnn_layer_config_t*)calloc(1, sizeof(lnn_layer_config_t));
        if (!config.layer_configs) { lnn_config_destroy(&config); return nullptr; }
        config.layer_configs[0].n_neurons = 8;
        config.layer_configs[0].activation = LNN_ACTIVATION_TANH;
        config.layer_configs[0].tau_base_init = 10.0f;
        config.layer_configs[0].tau_min = 0.1f;
        config.layer_configs[0].tau_max = 1000.0f;
        config.layer_configs[0].learn_tau = true;
        config.layer_configs[0].weight_init_std = 0.1f;
        config.layer_configs[0].wiring_type = LNN_WIRING_FULL;
        config.layer_configs[0].sparsity = 0.0f;
        config.layer_configs[0].ode_method = LNN_ODE_EULER;
        config.layer_configs[0].dt = 1.0f;
        config.layer_configs[0].use_layer_norm = false;
        config.layer_configs[0].layer_norm_eps = 1e-5f;

        lnn_network_t* net = lnn_network_create(&config);
        free(config.layer_configs);
        config.layer_configs = nullptr;
        lnn_config_destroy(&config);
        if (!net) return nullptr;

        /* Force CPU path. */
        for (uint32_t i = 0; i < net->n_layers; i++) {
            if (net->layers[i]) {
                net->layers[i]->gpu_lnn_layer = nullptr;
                net->layers[i]->gpu_ctx = nullptr;
            }
        }
        net->gpu_ctx = nullptr;

        /* Deterministic weights for cross-run comparison. */
        (void)lnn_network_init_weights(net, 42);
        return net;
    }

    nimcp_tensor_t* MakeInput(float value) {
        uint32_t dims[1] = {4};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        float* d = (float*)nimcp_tensor_data(t);
        for (uint32_t i = 0; i < 4; i++) d[i] = value;
        return t;
    }

    nimcp_tensor_t* MakeOutput() {
        uint32_t dims[1] = {8};
        return nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    }

    /* Copy 8-element tensor into a std::vector for easy comparisons. */
    static std::vector<float> Vec(const nimcp_tensor_t* t) {
        std::vector<float> v(8, 0.0f);
        const float* d = (const float*)nimcp_tensor_data_const(t);
        if (d) { for (size_t i = 0; i < 8; i++) v[i] = d[i]; }
        return v;
    }

    /* L2 norm of (a - b). */
    static float L2Diff(const std::vector<float>& a,
                        const std::vector<float>& b) {
        float s = 0.0f;
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; i++) {
            float d = a[i] - b[i];
            s += d * d;
        }
        return std::sqrt(s);
    }

    static float L2Norm(const std::vector<float>& a) {
        float s = 0.0f;
        for (float v : a) s += v * v;
        return std::sqrt(s);
    }

    static bool AllFinite(const std::vector<float>& a) {
        for (float v : a) if (!std::isfinite(v)) return false;
        return true;
    }
};

/*============================================================================
 * Test 1: ATP + membrane ramp-down over 100 steps materially changes the
 * output trajectory compared to the substrate-NULL baseline.
 *
 * Rationale:
 *   - Substrate path: ATP ramps from 1.0 → 0.3 over 100 steps (linear),
 *     membrane ramps from 1.0 → 0.1 (more severe). Dendrite
 *     membrane_time_constant_mod = 0.5 + 0.5*mem goes 1.0 → 0.55,
 *     shrinking effective tau.
 *   - Baseline: no substrate attached.
 *   - Final-output L2-norm-of-difference / baseline-L2-norm must exceed 2%.
 *==========================================================================*/
TEST_F(LNNSubstrateIntegrationTest, RampDownDifferFromBaselineBy2Percent) {
    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);
    lnn_tune_set_substrate_update_period(1.0f);  /* recompute every step */

    const int n_steps = 100;

    /* Baseline trajectory: no substrate attached. */
    lnn_network_t* net_base = MakeNetwork();
    ASSERT_NE(net_base, nullptr);
    nimcp_tensor_t* in_base  = MakeInput(0.5f);
    nimcp_tensor_t* out_base = MakeOutput();
    ASSERT_NE(in_base, nullptr);
    ASSERT_NE(out_base, nullptr);

    for (int i = 0; i < n_steps; i++) {
        ASSERT_EQ(lnn_forward_step(net_base, in_base, out_base, 1.0f),
                  LNN_SUCCESS) << "baseline step " << i;
    }
    std::vector<float> base_out = Vec(out_base);
    EXPECT_TRUE(AllFinite(base_out));

    nimcp_tensor_destroy(in_base);
    nimcp_tensor_destroy(out_base);
    lnn_network_destroy(net_base);

    /* Substrate trajectory: ramping ATP + membrane integrity. */
    lnn_network_t* net_sub = MakeNetwork();
    ASSERT_NE(net_sub, nullptr);
    substrate_config_t scfg;
    substrate_default_config(&scfg);
    neural_substrate_t* sub = substrate_create(&scfg);
    ASSERT_NE(sub, nullptr);
    lnn_network_attach_substrate(net_sub, sub);

    nimcp_tensor_t* in_sub  = MakeInput(0.5f);
    nimcp_tensor_t* out_sub = MakeOutput();

    for (int i = 0; i < n_steps; i++) {
        /* Ramp ATP 1.0 -> 0.3 and membrane 1.0 -> 0.1. */
        float frac = (float)i / (float)(n_steps - 1);
        float atp  = 1.0f - 0.7f * frac;
        float mem  = 1.0f - 0.9f * frac;
        substrate_set_atp(sub, atp);
        substrate_set_membrane_integrity(sub, mem);
        ASSERT_EQ(lnn_forward_step(net_sub, in_sub, out_sub, 1.0f),
                  LNN_SUCCESS) << "substrate step " << i;
    }
    std::vector<float> sub_out = Vec(out_sub);
    EXPECT_TRUE(AllFinite(sub_out));

    /* Quantify divergence. */
    float diff_l2 = L2Diff(base_out, sub_out);
    float base_l2 = std::max(L2Norm(base_out), 1e-6f);
    float rel = diff_l2 / base_l2;

    EXPECT_GT(rel, 0.02f)
        << "Substrate ramp-down must change the trajectory L2 by >2% "
        << "(base_l2=" << base_l2
        << ", diff_l2=" << diff_l2
        << ", rel=" << rel << ")";

    nimcp_tensor_destroy(in_sub);
    nimcp_tensor_destroy(out_sub);
    substrate_destroy(sub);
    lnn_network_destroy(net_sub);
}

/*============================================================================
 * Test 2: All substrate knobs off -> bit-identical to the substrate-NULL
 * baseline. This is the core no-op guarantee: attaching a substrate with
 * the master enable knob off must NOT perturb the forward path in any way.
 *==========================================================================*/
TEST_F(LNNSubstrateIntegrationTest, AllOffBitIdenticalToNullBaseline) {
    /* Baseline run: no substrate attached, knob value irrelevant. */
    lnn_network_t* net_base = MakeNetwork();
    ASSERT_NE(net_base, nullptr);

    nimcp_tensor_t* in_base  = MakeInput(0.4f);
    nimcp_tensor_t* out_base = MakeOutput();

    const int n_steps = 50;
    for (int i = 0; i < n_steps; i++) {
        ASSERT_EQ(lnn_forward_step(net_base, in_base, out_base, 1.0f),
                  LNN_SUCCESS);
    }
    std::vector<float> base_out = Vec(out_base);

    nimcp_tensor_destroy(in_base);
    nimcp_tensor_destroy(out_base);
    lnn_network_destroy(net_base);

    /* "Off" run: attach a damaged substrate but with the knob OFF. */
    lnn_network_t* net_off = MakeNetwork();
    ASSERT_NE(net_off, nullptr);

    substrate_config_t scfg;
    substrate_default_config(&scfg);
    neural_substrate_t* sub = substrate_create(&scfg);
    ASSERT_NE(sub, nullptr);
    /* Damage the substrate so differences would be obvious IF the knob
     * mattered. It doesn't, because the knob is off. */
    substrate_set_atp(sub, 0.1f);
    substrate_set_membrane_integrity(sub, 0.1f);
    lnn_network_attach_substrate(net_off, sub);

    lnn_tune_set_substrate_enabled(0.0f);          /* master OFF */
    lnn_tune_set_substrate_tau_compose_on(0.0f);   /* tau compose OFF */

    nimcp_tensor_t* in_off  = MakeInput(0.4f);
    nimcp_tensor_t* out_off = MakeOutput();

    for (int i = 0; i < n_steps; i++) {
        ASSERT_EQ(lnn_forward_step(net_off, in_off, out_off, 1.0f),
                  LNN_SUCCESS);
    }
    std::vector<float> off_out = Vec(out_off);

    /* Must match bit-for-bit (same seeded weights, same inputs, no
     * substrate effects applied when knob is off). */
    ASSERT_EQ(base_out.size(), off_out.size());
    for (size_t i = 0; i < base_out.size(); i++) {
        EXPECT_FLOAT_EQ(base_out[i], off_out[i])
            << "Substrate off knob path must be bit-identical to baseline "
            << "at index " << i;
    }

    nimcp_tensor_destroy(in_off);
    nimcp_tensor_destroy(out_off);
    substrate_destroy(sub);
    lnn_network_destroy(net_off);
}

/*============================================================================
 * Test 3: update_period rate-limits the cache refresh. With period=5, the
 * cache is recomputed every 5 steps; intermediate steps must leave
 * substrate_steps_since_update incrementing monotonically until the reset.
 *==========================================================================*/
TEST_F(LNNSubstrateIntegrationTest, UpdatePeriodRateLimitsCacheRefresh) {
    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);
    lnn_tune_set_substrate_update_period(5.0f);

    lnn_network_t* net = MakeNetwork();
    ASSERT_NE(net, nullptr);

    substrate_config_t scfg;
    substrate_default_config(&scfg);
    neural_substrate_t* sub = substrate_create(&scfg);
    ASSERT_NE(sub, nullptr);
    lnn_network_attach_substrate(net, sub);

    nimcp_tensor_t* in  = MakeInput(0.3f);
    nimcp_tensor_t* out = MakeOutput();

    /* Step 1: counter increments 0 -> 1. */
    ASSERT_EQ(lnn_forward_step(net, in, out, 1.0f), LNN_SUCCESS);
    EXPECT_EQ(net->substrate_steps_since_update, 1u);

    /* Steps 2, 3, 4: counter goes 1 -> 2 -> 3 -> 4. */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(lnn_forward_step(net, in, out, 1.0f), LNN_SUCCESS);
    }
    EXPECT_EQ(net->substrate_steps_since_update, 4u);

    /* Step 5: counter becomes 5, then resets to 0 at the end of the
     * update since period=5. */
    ASSERT_EQ(lnn_forward_step(net, in, out, 1.0f), LNN_SUCCESS);
    EXPECT_EQ(net->substrate_steps_since_update, 0u);

    /* Step 6: cache recomputes (counter was 0), counter goes to 1. */
    ASSERT_EQ(lnn_forward_step(net, in, out, 1.0f), LNN_SUCCESS);
    EXPECT_EQ(net->substrate_steps_since_update, 1u);

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    substrate_destroy(sub);
    lnn_network_destroy(net);
}
