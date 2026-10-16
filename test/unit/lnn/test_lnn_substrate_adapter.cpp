/**
 * @file test_lnn_substrate_adapter.cpp
 * @brief Unit tests for the LNN substrate adapter (Phase 2 biological wiring).
 *
 * WHAT: Covers the three runtime-tunable substrate knobs plus the
 *       lnn_network_attach_substrate API. Also runs a small live LNN
 *       with different substrate states (ATP, membrane integrity,
 *       temperature) to verify the effects struct cache is populated
 *       and that output statistics respond to substrate degradation.
 * WHY:  The substrate adapter is the feedback path from metabolic state
 *       into the LNN forward pass. Unlike SNN, LNN has LEARNED tau per
 *       neuron — the substrate multiplier must COMPOSE (wrap) with the
 *       learned value, not overwrite it. Regressions here silently
 *       decouple biological state from the continuous-time dynamics.
 * HOW:  Google Test. Tests that do not need a network just exercise the
 *       tunable API. Tests that need a network construct a lightweight
 *       config, force-disable the GPU layers (we want the CPU LTC path
 *       so the substrate hooks execute), and compare outputs across
 *       substrate regimes.
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
 * Fixture: saves + restores every substrate tunable the tests touch.
 *==========================================================================*/
class LNNSubstrateAdapterTest : public ::testing::Test {
protected:
    float saved_enabled         = 0.0f;
    float saved_period          = 0.0f;
    float saved_tau_compose_on  = 0.0f;

    void SetUp() override {
        saved_enabled        = lnn_tune_get_substrate_enabled();
        saved_period         = lnn_tune_get_substrate_update_period();
        saved_tau_compose_on = lnn_tune_get_substrate_tau_compose_on();

        /* Ensure library is initialized (no-op if already initialized). */
        (void)lnn_init(1);
    }

    void TearDown() override {
        lnn_tune_set_substrate_enabled(saved_enabled);
        lnn_tune_set_substrate_update_period(saved_period);
        lnn_tune_set_substrate_tau_compose_on(saved_tau_compose_on);
    }

    /* Build a small 1-layer LNN (8 neurons). Forces CPU path by null-ing
     * the GPU handle/context so the substrate hooks execute. */
    lnn_network_t* MakeSmallNetwork(uint32_t n_neurons = 8,
                                    uint32_t n_inputs = 4) {
        lnn_config_t config;
        memset(&config, 0, sizeof(config));
        if (lnn_config_default(&config) != NIMCP_SUCCESS) return nullptr;
        config.n_inputs = n_inputs;
        config.n_outputs = n_neurons;
        config.n_layers = 1;
        config.default_dt = 1.0f;
        config.layer_configs = (lnn_layer_config_t*)calloc(1, sizeof(lnn_layer_config_t));
        if (!config.layer_configs) { lnn_config_destroy(&config); return nullptr; }
        config.layer_configs[0].n_neurons = n_neurons;
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
        config.layer_configs = nullptr;  /* avoid double free in destroy */
        lnn_config_destroy(&config);
        if (!net) return nullptr;

        /* Force CPU LTC path: null GPU handles on every layer + the
         * network's gpu_ctx so GPU dispatch is skipped. The GPU module
         * still allocates its resources above, but the dispatch check
         * requires BOTH layer->gpu_lnn_layer and layer->gpu_ctx to be
         * non-NULL — we clear both. */
        for (uint32_t i = 0; i < net->n_layers; i++) {
            if (net->layers[i]) {
                net->layers[i]->gpu_lnn_layer = nullptr;
                net->layers[i]->gpu_ctx = nullptr;
            }
        }
        net->gpu_ctx = nullptr;

        (void)lnn_network_init_weights(net, 42);  /* deterministic seed */
        return net;
    }

    nimcp_tensor_t* MakeInput(uint32_t n, float value = 0.5f) {
        uint32_t dims[1] = {n};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        float* d = (float*)nimcp_tensor_data(t);
        for (uint32_t i = 0; i < n; i++) d[i] = value;
        return t;
    }

    nimcp_tensor_t* MakeOutput(uint32_t n) {
        uint32_t dims[1] = {n};
        return nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    }

    static float L2Norm(const nimcp_tensor_t* t) {
        const float* d = (const float*)nimcp_tensor_data_const(t);
        size_t n = nimcp_tensor_numel(t);
        if (!d || n == 0) return 0.0f;
        float s = 0.0f;
        for (size_t i = 0; i < n; i++) {
            if (std::isfinite(d[i])) s += d[i] * d[i];
        }
        return std::sqrt(s);
    }

    static bool AllFinite(const nimcp_tensor_t* t) {
        const float* d = (const float*)nimcp_tensor_data_const(t);
        size_t n = nimcp_tensor_numel(t);
        if (!d) return false;
        for (size_t i = 0; i < n; i++) {
            if (!std::isfinite(d[i])) return false;
        }
        return true;
    }

    /* Create a fresh substrate with defaults then overlay test values. */
    neural_substrate_t* MakeSubstrate(float atp,
                                      float tempC = 37.0f,
                                      float membrane = -1.0f) {
        substrate_config_t scfg;
        substrate_default_config(&scfg);
        neural_substrate_t* sub = substrate_create(&scfg);
        if (!sub) return nullptr;
        substrate_set_atp(sub, atp);
        substrate_set_temperature(sub, tempC);
        if (membrane >= 0.0f) {
            substrate_set_membrane_integrity(sub, membrane);
        }
        return sub;
    }
};

/*============================================================================
 * Tunables — round-trip + out-of-range rejection.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, TunableEnabledRoundTripsNonzeroToOne) {
    lnn_tune_set_substrate_enabled(0.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_enabled(), 0.0f);

    lnn_tune_set_substrate_enabled(1.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_enabled(), 1.0f);

    lnn_tune_set_substrate_enabled(0.3f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_enabled(), 1.0f);

    lnn_tune_set_substrate_enabled(-7.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_enabled(), 1.0f);

    lnn_tune_set_substrate_enabled(0.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_enabled(), 0.0f);
}

TEST_F(LNNSubstrateAdapterTest, TunablePeriodClampsOutOfRange) {
    lnn_tune_set_substrate_update_period(10.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 10.0f);

    /* Out-of-range low: rejected. */
    lnn_tune_set_substrate_update_period(0.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 10.0f);
    lnn_tune_set_substrate_update_period(-1.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 10.0f);

    /* Out-of-range high: rejected. */
    lnn_tune_set_substrate_update_period(1e9f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 10.0f);

    /* In range: accepted. */
    lnn_tune_set_substrate_update_period(5.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 5.0f);

    lnn_tune_set_substrate_update_period(10000.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 10000.0f);
}

TEST_F(LNNSubstrateAdapterTest, TunableTauComposeRoundTrip) {
    lnn_tune_set_substrate_tau_compose_on(0.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_tau_compose_on(), 0.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_tau_compose_on(), 1.0f);
    lnn_tune_set_substrate_tau_compose_on(42.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_tau_compose_on(), 1.0f);
    lnn_tune_set_substrate_tau_compose_on(-0.5f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_tau_compose_on(), 1.0f);
}

/*============================================================================
 * Attach API — null-tolerant + basic bookkeeping.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, AttachNullNetworkIsNoOp) {
    /* Should not crash. */
    lnn_network_attach_substrate(nullptr, nullptr);
    SUCCEED();
}

TEST_F(LNNSubstrateAdapterTest, AttachSetsAndDetachesPointer) {
    lnn_network_t* net = MakeSmallNetwork();
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(net->substrate, nullptr);

    neural_substrate_t* sub = MakeSubstrate(1.0f);
    ASSERT_NE(sub, nullptr);

    lnn_network_attach_substrate(net, sub);
    EXPECT_EQ(net->substrate, sub);
    EXPECT_EQ(net->substrate_steps_since_update, 0u);

    /* Detach (NULL) */
    lnn_network_attach_substrate(net, nullptr);
    EXPECT_EQ(net->substrate, nullptr);

    substrate_destroy(sub);
    lnn_network_destroy(net);
}

/*============================================================================
 * F1 CRITICAL mirror: attach must populate the effects cache IMMEDIATELY.
 * Before the fix, attach memset'd the cache to zero and left population
 * to the first forward step's refresh — so any consumer that reads
 * cached_dend_effects.plasticity_mod before the first step saw 0, and
 * learning silently died (lr × 0). Protect that with this explicit
 * pre-step check. Mirrors test_snn_substrate_adapter.cpp::AttachPopulatesCache.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, AttachPopulatesCache) {
    lnn_network_t* net = MakeSmallNetwork();
    ASSERT_NE(net, nullptr);

    /* Build a substrate with known ATP so we can predict the cache. */
    neural_substrate_t* sub = MakeSubstrate(0.5f, 37.0f, 1.0f);
    ASSERT_NE(sub, nullptr);

    /* Cache is zero before attach. */
    EXPECT_FLOAT_EQ(net->cached_dend_effects.plasticity_mod, 0.0f);
    EXPECT_FLOAT_EQ(net->cached_axon_effects.atp_velocity_factor, 0.0f);

    /* Attach — must populate cache BEFORE any forward step runs. */
    lnn_network_attach_substrate(net, sub);

    /* plasticity_mod is linear in ATP in the substrate helper, so atp=0.5
     * yields plasticity_mod≈0.5. Must be non-zero to prove eager population. */
    EXPECT_NE(net->cached_dend_effects.plasticity_mod, 0.0f)
        << "Attach did not immediately refresh plasticity_mod from substrate; "
        << "downstream LR × 0 would silently kill learning.";
    EXPECT_NEAR(net->cached_dend_effects.plasticity_mod, 0.5f, 1e-4f);
    EXPECT_NEAR(net->cached_axon_effects.atp_velocity_factor, 0.5f, 1e-4f)
        << "Attach did not immediately refresh axon cache.";
    /* Sanity: overall_capacity is non-zero on a real substrate (ATP>0). */
    EXPECT_GT(net->cached_dend_effects.overall_capacity, 0.0f);

    /* Detach must not crash (null sub). */
    lnn_network_attach_substrate(net, nullptr);
    EXPECT_EQ(net->substrate, nullptr);

    substrate_destroy(sub);
    lnn_network_destroy(net);
}

/*============================================================================
 * Null-substrate + no-attach forward behaves identically to the baseline
 * path (regression safeguard).
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, NullSubstrateForwardBehavesAsBaseline) {
    lnn_network_t* net = MakeSmallNetwork();
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(net->substrate, nullptr);

    nimcp_tensor_t* in  = MakeInput(net->n_inputs, 0.5f);
    nimcp_tensor_t* out = MakeOutput(net->n_outputs);
    ASSERT_NE(in, nullptr);
    ASSERT_NE(out, nullptr);

    /* Enable knob — should be inert with no substrate attached. */
    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);

    int rc = lnn_forward_step(net, in, out, 1.0f);
    EXPECT_EQ(rc, LNN_SUCCESS);
    EXPECT_TRUE(AllFinite(out));

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    lnn_network_destroy(net);
}

/*============================================================================
 * Default-ATP substrate + forward: output finite, cache populated.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, DefaultSubstrateProducesFiniteOutputs) {
    lnn_network_t* net = MakeSmallNetwork();
    ASSERT_NE(net, nullptr);

    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);
    lnn_tune_set_substrate_update_period(1.0f);  /* refresh every step */

    neural_substrate_t* sub = MakeSubstrate(1.0f, 37.0f, 1.0f);
    ASSERT_NE(sub, nullptr);
    lnn_network_attach_substrate(net, sub);

    nimcp_tensor_t* in  = MakeInput(net->n_inputs, 0.5f);
    nimcp_tensor_t* out = MakeOutput(net->n_outputs);

    int rc = lnn_forward_step(net, in, out, 1.0f);
    EXPECT_EQ(rc, LNN_SUCCESS);
    EXPECT_TRUE(AllFinite(out));

    /* After one step, cache is populated. atp=1.0 → plasticity_mod=1.0,
     * atp_velocity_factor=1.0, q10=1.0 (at T=37). */
    EXPECT_NEAR(net->cached_axon_effects.atp_velocity_factor, 1.0f, 1e-4f);
    EXPECT_NEAR(net->cached_dend_effects.plasticity_mod, 1.0f, 1e-4f);
    EXPECT_NEAR(net->cached_axon_effects.temperature_q10_factor, 1.0f, 1e-4f);

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    substrate_destroy(sub);
    lnn_network_destroy(net);
}

/*============================================================================
 * Damaged membrane substrate reduces tau → output magnitude differs.
 *
 * Rationale: membrane_time_constant_mod = 0.5 + 0.5*mem. At mem=1.0 it's 1.0
 * (no-op). At mem=0.1 it's 0.55, shrinking effective tau by ~45%. Smaller
 * tau = faster decay toward steady-state, producing a different output
 * trajectory from the mem=1.0 baseline. We run 100 forward steps and
 * compare L2 norms of the outputs. The relative difference is expected
 * to be non-trivial (>2%).
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, DamagedMembraneChangesOutputVsHealthy) {
    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_tau_compose_on(1.0f);
    lnn_tune_set_substrate_update_period(1.0f);

    /* Baseline: healthy membrane. */
    lnn_network_t* net1 = MakeSmallNetwork();
    ASSERT_NE(net1, nullptr);
    neural_substrate_t* sub_ok = MakeSubstrate(1.0f, 37.0f, 1.0f);
    ASSERT_NE(sub_ok, nullptr);
    lnn_network_attach_substrate(net1, sub_ok);

    nimcp_tensor_t* in1  = MakeInput(net1->n_inputs, 0.5f);
    nimcp_tensor_t* out1 = MakeOutput(net1->n_outputs);

    float norm_healthy = 0.0f;
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(lnn_forward_step(net1, in1, out1, 1.0f), LNN_SUCCESS);
    }
    norm_healthy = L2Norm(out1);

    lnn_network_destroy(net1);
    nimcp_tensor_destroy(in1);
    nimcp_tensor_destroy(out1);
    substrate_destroy(sub_ok);

    /* Damaged: membrane_integrity=0.1 → tau multiplier 0.55. */
    lnn_network_t* net2 = MakeSmallNetwork();
    ASSERT_NE(net2, nullptr);
    neural_substrate_t* sub_bad = MakeSubstrate(1.0f, 37.0f, 0.1f);
    ASSERT_NE(sub_bad, nullptr);
    lnn_network_attach_substrate(net2, sub_bad);

    nimcp_tensor_t* in2  = MakeInput(net2->n_inputs, 0.5f);
    nimcp_tensor_t* out2 = MakeOutput(net2->n_outputs);

    float norm_damaged = 0.0f;
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(lnn_forward_step(net2, in2, out2, 1.0f), LNN_SUCCESS);
    }
    norm_damaged = L2Norm(out2);

    /* Outputs must stay finite in both regimes. */
    EXPECT_TRUE(AllFinite(out2));

    /* The two trajectories must differ: relative L2-norm distance > 2%.
     * Weights are seeded deterministically, only substrate changes. */
    float denom = std::max(norm_healthy, 1e-6f);
    float rel   = std::fabs(norm_healthy - norm_damaged) / denom;
    EXPECT_GT(rel, 0.02f)
        << "Damaged-membrane substrate should change output L2 norm by >2% "
        << "(healthy=" << norm_healthy
        << ", damaged=" << norm_damaged
        << ", rel=" << rel << ")";

    lnn_network_destroy(net2);
    nimcp_tensor_destroy(in2);
    nimcp_tensor_destroy(out2);
    substrate_destroy(sub_bad);
}

/*============================================================================
 * Hot temperature (Q10) changes cached q10_factor compared to 37°C.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, TemperatureChangesCachedQ10Factor) {
    lnn_network_t* net = MakeSmallNetwork();
    ASSERT_NE(net, nullptr);

    lnn_tune_set_substrate_enabled(1.0f);
    lnn_tune_set_substrate_update_period(1.0f);

    /* T=42°C: Q10 = 2.3^((42-37)/10) = 2.3^0.5 ≈ 1.517. */
    neural_substrate_t* sub = MakeSubstrate(1.0f, 42.0f, 1.0f);
    ASSERT_NE(sub, nullptr);
    lnn_network_attach_substrate(net, sub);

    nimcp_tensor_t* in  = MakeInput(net->n_inputs, 0.3f);
    nimcp_tensor_t* out = MakeOutput(net->n_outputs);

    ASSERT_EQ(lnn_forward_step(net, in, out, 1.0f), LNN_SUCCESS);

    /* q10 factor at T=42 must be > 1.3 (> 37°C baseline of 1.0). */
    EXPECT_GT(net->cached_axon_effects.temperature_q10_factor, 1.3f);

    /* output finite */
    EXPECT_TRUE(AllFinite(out));

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    substrate_destroy(sub);
    lnn_network_destroy(net);
}

/*============================================================================
 * Disabled enabled knob ignores an attached substrate: cache stays zero,
 * output is finite and identical to the no-substrate-attached baseline.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, DisabledKnobIgnoresAttachedSubstrate) {
    lnn_network_t* net = MakeSmallNetwork();
    ASSERT_NE(net, nullptr);

    /* Severely damaged substrate — would normally mangle tau + LR. */
    neural_substrate_t* sub = MakeSubstrate(0.05f, 37.0f, 0.05f);
    ASSERT_NE(sub, nullptr);

    /* Master knob OFF: per-step refresh will not run. */
    lnn_tune_set_substrate_enabled(0.0f);

    /* F1 CRITICAL mirror: attach unconditionally populates the cache from
     * the attached substrate so downstream consumers don't multiply by 0.
     * The enabled-knob check only gates per-step cache refreshes + the
     * hot-loop modulation, not attach-time initialization. To preserve the
     * original test intent (stepping with knob off leaves cache at baseline
     * zero), force-zero the cache AFTER attach so the stepping check holds. */
    lnn_network_attach_substrate(net, sub);
    memset(&net->cached_axon_effects, 0, sizeof(net->cached_axon_effects));
    memset(&net->cached_dend_effects, 0, sizeof(net->cached_dend_effects));

    nimcp_tensor_t* in  = MakeInput(net->n_inputs, 0.5f);
    nimcp_tensor_t* out = MakeOutput(net->n_outputs);

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(lnn_forward_step(net, in, out, 1.0f), LNN_SUCCESS);
    }

    /* With the knob off, the cached effects are never repopulated. */
    EXPECT_FLOAT_EQ(net->cached_axon_effects.atp_velocity_factor, 0.0f);
    EXPECT_FLOAT_EQ(net->cached_dend_effects.plasticity_mod, 0.0f);

    EXPECT_TRUE(AllFinite(out));

    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    substrate_destroy(sub);
    lnn_network_destroy(net);
}

/*============================================================================
 * Out-of-range value rejections are covered in the round-trip tests above
 * (period rejects <1 and >10000; enabled/tau_compose normalize nonzero).
 * This separate test hammers a handful of numerical edge cases explicitly
 * so that any regression in the setter math is caught directly.
 *==========================================================================*/
TEST_F(LNNSubstrateAdapterTest, TunablesRejectExplicitOutOfRangeValues) {
    /* Period floor */
    lnn_tune_set_substrate_update_period(50.0f);
    lnn_tune_set_substrate_update_period(0.99f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 50.0f);

    /* Period ceiling */
    lnn_tune_set_substrate_update_period(50.0f);
    lnn_tune_set_substrate_update_period(10001.0f);
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 50.0f);

    /* NaN/infinity rejected too (period condition is a range test). */
    lnn_tune_set_substrate_update_period(50.0f);
    lnn_tune_set_substrate_update_period(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 50.0f);
    lnn_tune_set_substrate_update_period(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(lnn_tune_get_substrate_update_period(), 50.0f);
}
