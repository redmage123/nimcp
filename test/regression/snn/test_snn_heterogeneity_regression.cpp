//=============================================================================
// test_snn_heterogeneity_regression.cpp
//=============================================================================
/**
 * @file test_snn_heterogeneity_regression.cpp
 * @brief Wave G regression — σ=0 (default) must produce IDENTICAL behaviour
 *        to a network where the per-neuron arrays are explicitly populated
 *        with the pop-wide value.
 *
 * WHAT: Two contracts pinned by this file:
 *       1. Default (no setter call, arrays NULL) vs explicit per-neuron
 *          arrays populated with pop-wide values produces identical spike
 *          trains over 50 steps. This pins that the LIF lookup helper
 *          falls back to pop-wide bit-identically when arrays are NULL.
 *       2. σ=0 → σ>0 → σ=0 round-trip restores the default state (arrays
 *          NULL), preserving the "off-mode is bit-identical" contract.
 *
 * WHY:  Wave G adds new branches in the SNN hot loop. The default-OFF
 *       contract requires that with σ=0 the path is observably
 *       indistinguishable from pre-Wave-G — otherwise checkpoints / training
 *       trajectories silently change the day this code lands.
 *
 * HOW:  Google Test. Fixture builds a 32-neuron lightweight pop, drives it
 *       with sustained current for 50 steps, records spike counts.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
}

extern "C" {
    void  snn_tune_set_conductance_enabled(float);
    void  snn_tune_set_cb_weights_rescaled(float);
    void  snn_tune_set_noise_rate_hz(float);
    void  snn_tune_set_basket_enabled(float);
    void  snn_tune_set_ahp_enabled(float);
    void  snn_tune_set_pump_enabled(float);
    void  snn_tune_set_substrate_enabled(float);
}

namespace {

class SnnHeterogeneityRegression : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }
    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        snn_tune_set_noise_rate_hz(0.0f);
        snn_tune_set_basket_enabled(0.0f);
        snn_tune_set_ahp_enabled(0.0f);
        snn_tune_set_pump_enabled(0.0f);
        snn_tune_set_substrate_enabled(0.0f);
        snn_tune_set_conductance_enabled(0.0f);
        snn_tune_set_cb_weights_rescaled(1.0f);
    }

    void TearDown() override {}

    /* Build a network + 32-neuron lightweight pop. Caller owns destroy. */
    snn_network_t* build_with_pop(int* out_pop_id) {
        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = 1;
        cfg.n_outputs = 1;
        cfg.n_hidden  = 0;
        cfg.dt        = 1.0f;
        snn_network_t* net = snn_network_create(&cfg);
        if (!net) return nullptr;

        int pop_id = snn_network_add_population_lightweight(
            net, 32, NEURON_GENERIC_LIF, "reg_pop");
        if (pop_id < 0) {
            snn_network_destroy(net);
            return nullptr;
        }
        snn_network_finalize_connections(net);
        if (out_pop_id) *out_pop_id = pop_id;
        return net;
    }

    /* Drive a pop with sustained current for n_steps. Returns per-neuron
     * spike count vector. */
    std::vector<uint32_t> run_collect(snn_network_t* net,
                                      int pop_id,
                                      float drive,
                                      int n_steps) {
        snn_population_t* pop = snn_network_get_population(net, (uint32_t)pop_id);
        EXPECT_NE(pop, nullptr);
        float* v   = (float*)nimcp_tensor_data(pop->membrane_v);
        float* ref = (float*)nimcp_tensor_data(pop->refractory);
        float* spk = (float*)nimcp_tensor_data(pop->spike_output);

        snn_lif_params_t base = snn_pop_lif_params(pop, &net->config);
        std::vector<uint32_t> counts(pop->n_neurons, 0);

        for (uint32_t i = 0; i < pop->n_neurons; i++) {
            v[i]   = base.v_rest;
            ref[i] = 0.0f;
            spk[i] = 0.0f;
        }

        for (int s = 0; s < n_steps; s++) {
            for (uint32_t i = 0; i < pop->n_neurons; i++) {
                pop->external_current[i] = drive;
            }
            EXPECT_GE(snn_network_step(net, 1.0f), 0);
            for (uint32_t i = 0; i < pop->n_neurons; i++) {
                if (spk[i] > 0.5f) counts[i] += 1;
            }
        }
        return counts;
    }
};

//-----------------------------------------------------------------------------
// 1. Default (arrays NULL) vs explicit (arrays populated with pop-wide value):
//    spike counts must be identical. Pins the bit-identity contract.
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityRegression, NullArraysMatchExplicitPopWideValues) {
    int pop_id_a = -1, pop_id_b = -1;
    snn_network_t* net_default = build_with_pop(&pop_id_a);
    snn_network_t* net_explicit = build_with_pop(&pop_id_b);
    ASSERT_NE(net_default, nullptr);
    ASSERT_NE(net_explicit, nullptr);

    /* Manually allocate per-neuron arrays in net_explicit and fill them
     * with the pop-wide value (uniform variance == 0). The LIF lookup
     * helper should return the SAME values as the NULL-fallback path. */
    snn_population_t* pop_e = snn_network_get_population(
        net_explicit, (uint32_t)pop_id_b);
    ASSERT_NE(pop_e, nullptr);
    snn_lif_params_t base = snn_pop_lif_params(pop_e, &net_explicit->config);

    pop_e->tau_mem_per_neuron =
        (float*)nimcp_calloc(pop_e->n_neurons, sizeof(float));
    pop_e->v_thresh_per_neuron =
        (float*)nimcp_calloc(pop_e->n_neurons, sizeof(float));
    ASSERT_NE(pop_e->tau_mem_per_neuron,  nullptr);
    ASSERT_NE(pop_e->v_thresh_per_neuron, nullptr);

    for (uint32_t i = 0; i < pop_e->n_neurons; i++) {
        pop_e->tau_mem_per_neuron[i]  = base.tau_mem;
        pop_e->v_thresh_per_neuron[i] = base.v_thresh;
    }
    /* heterogeneity_sigma stays at 0 (we did NOT call the setter), so the
     * bypass behaviour is governed solely by array-NULL-ness. */

    const float drive = 20.0f;
    const int n_steps = 50;

    auto cnt_default  = run_collect(net_default,  pop_id_a, drive, n_steps);
    auto cnt_explicit = run_collect(net_explicit, pop_id_b, drive, n_steps);

    ASSERT_EQ(cnt_default.size(), cnt_explicit.size());
    for (size_t i = 0; i < cnt_default.size(); i++) {
        EXPECT_EQ(cnt_default[i], cnt_explicit[i])
            << "n=" << i
            << ": default(NULL arrays) spike count=" << cnt_default[i]
            << " vs explicit(pop-wide arrays) spike count=" << cnt_explicit[i]
            << " — bit-identity contract violated";
    }

    snn_network_destroy(net_default);
    snn_network_destroy(net_explicit);
}

//-----------------------------------------------------------------------------
// 2. Round-trip σ=0 → σ>0 → σ=0 restores arrays to NULL.
//    Pins the symmetric off-switch contract.
//-----------------------------------------------------------------------------
TEST_F(SnnHeterogeneityRegression, RoundTripRestoresDefault) {
    int pop_id = -1;
    snn_network_t* net = build_with_pop(&pop_id);
    ASSERT_NE(net, nullptr);

    snn_population_t* pop = snn_network_get_population(net, (uint32_t)pop_id);
    ASSERT_NE(pop, nullptr);
    EXPECT_EQ(pop->tau_mem_per_neuron,  nullptr);
    EXPECT_EQ(pop->v_thresh_per_neuron, nullptr);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.0f);

    ASSERT_EQ(snn_network_set_pop_heterogeneity(net, (uint32_t)pop_id, 0.15f), 0);
    EXPECT_NE(pop->tau_mem_per_neuron,  nullptr);
    EXPECT_NE(pop->v_thresh_per_neuron, nullptr);
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.15f);

    ASSERT_EQ(snn_network_set_pop_heterogeneity(net, (uint32_t)pop_id, 0.0f), 0);
    EXPECT_EQ(pop->tau_mem_per_neuron,  nullptr)
        << "round-trip σ=0 must free per-neuron τ array";
    EXPECT_EQ(pop->v_thresh_per_neuron, nullptr)
        << "round-trip σ=0 must free per-neuron v_thresh array";
    EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, 0.0f);

    snn_network_destroy(net);
}

}  // namespace
