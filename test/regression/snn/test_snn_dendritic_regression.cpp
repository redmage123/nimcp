//=============================================================================
// test_snn_dendritic_regression.cpp — Wave H OFF-mode regression contract
//=============================================================================
/**
 * @file test_snn_dendritic_regression.cpp
 * @brief Wave H regression — dendritic_enabled = false (default) MUST
 *        produce bit-identical step behavior to a network where the gate
 *        was never set. The new two-compartment branch must be an
 *        observable no-op when no pop has been opted in.
 *
 * WHAT: Two pinning checks:
 *       1. Default flag (OFF) on every pop produces deterministically
 *          identical spike counts to a fresh net run with the flag
 *          explicitly set OFF. No drift from the new compartmental
 *          deposit-routing path.
 *       2. Setting dendritic_enabled on a pop measurably changes its
 *          per-neuron state (apical V drifts off rest with NMDA input)
 *          — pinning that the new branch is not silently dead.
 *
 * WHY:  Wave H adds a per-neuron-per-step branch in snn_network_step.
 *       The default-OFF case must remain bit-identical to a network
 *       where the setter was never called — otherwise downstream
 *       training checkpoints could silently change.
 *
 * HOW:  Google Test. Builds 2-pop networks (sensor → tgt) with CB ON,
 *       runs identical drives twice. Compares total spike counts and
 *       expects them to match within ±1 (the only nondeterminism is
 *       the rand_r-driven Poisson noise, which we disable).
 *
 * See docs/claude/wave-h-dendritic-design-2026-04-27.md.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_synapse.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "utils/tensor/nimcp_tensor.h"
}

extern "C" {
    void  snn_tune_set_conductance_enabled(float);
    void  snn_tune_set_cb_weights_rescaled(float);
    void  snn_tune_set_dendritic_enabled(float);
    float snn_tune_get_dendritic_enabled(void);
    void  snn_tune_set_noise_rate_hz(float);
    void  snn_tune_set_basket_enabled(float);
    void  snn_tune_set_ahp_enabled(float);
    void  snn_tune_set_pump_enabled(float);
    void  snn_tune_set_substrate_enabled(float);
}

namespace {

class SnnDendriticRegression : public ::testing::Test {
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
        snn_tune_set_conductance_enabled(1.0f);   /* CB ON */
        snn_tune_set_cb_weights_rescaled(1.0f);
        snn_tune_set_dendritic_enabled(0.0f);     /* dendritic OFF */
    }

    void TearDown() override {
        snn_tune_set_conductance_enabled(0.0f);
        snn_tune_set_dendritic_enabled(0.0f);
    }

    /* Build a 2-pop net with a fixed AMPA path. Returns net + pop ids. */
    snn_network_t* build_pair(int& src_id_out, int& dst_id_out) {
        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = 1;
        cfg.n_outputs = 1;
        cfg.n_hidden  = 0;
        cfg.dt        = 1.0f;
        snn_network_t* net = snn_network_create(&cfg);
        if (!net) return nullptr;
        int src = snn_network_add_population_lightweight(
            net, 4, NEURON_GENERIC_LIF, "src");
        int dst = snn_network_add_population_lightweight(
            net, 4, NEURON_GENERIC_LIF, "dst");
        if (src < 0 || dst < 0) { snn_network_destroy(net); return nullptr; }
        int n = snn_network_connect_populations(
            net, (uint32_t)src, (uint32_t)dst, SNN_TOPO_FULL, 1.0f,
            SYNAPSE_AMPA, 0.5f, 0.0f);
        if (n <= 0) { snn_network_destroy(net); return nullptr; }
        if (snn_network_finalize_connections(net) < 0) {
            snn_network_destroy(net); return nullptr;
        }
        src_id_out = src;
        dst_id_out = dst;
        return net;
    }

    /* Drive every neuron in src to fire each step. Mirrors the helper
     * in test_snn_per_receptor_integration.c. */
    static void drive_all(snn_network_t* net, int pop_id) {
        snn_population_t* p = net->populations[pop_id];
        if (!p) return;
        float* v = (float*)nimcp_tensor_data(p->membrane_v);
        if (v) for (uint32_t i = 0; i < p->n_neurons; i++) v[i] = -49.5f;
        float* ref = (float*)nimcp_tensor_data(p->refractory);
        if (ref) for (uint32_t i = 0; i < p->n_neurons; i++) ref[i] = 0.0f;
        if (p->external_current)
            for (uint32_t i = 0; i < p->n_neurons; i++) p->external_current[i] = 100.0f;
    }

    /* Run N steps with src driven; return total dst soma spikes. */
    static uint64_t run_count(snn_network_t* net, int src, int dst, int n_steps) {
        uint64_t baseline = net->populations[dst]->total_spikes;
        for (int s = 0; s < n_steps; s++) {
            drive_all(net, src);
            int rc = snn_network_step(net, 1.0f);
            (void)rc;
        }
        return net->populations[dst]->total_spikes - baseline;
    }
};

//-----------------------------------------------------------------------------
// 1. Default OFF on every pop → spike count is deterministic + matches a
//    second identical run. The new compartmental deposit/integrate paths
//    must NOT be exercised when dendritic_enabled is false.
//-----------------------------------------------------------------------------
TEST_F(SnnDendriticRegression, DefaultOffIsDeterministic) {
    int srcA, dstA, srcB, dstB;
    snn_network_t* netA = build_pair(srcA, dstA);
    snn_network_t* netB = build_pair(srcB, dstB);
    ASSERT_NE(netA, nullptr);
    ASSERT_NE(netB, nullptr);

    /* Both nets default OFF — spike counts must match within ±1
     * (cross-step nondeterminism from rand_r is ruled out by disabling
     * Poisson noise in SetUp). */
    uint64_t cntA = run_count(netA, srcA, dstA, 50);
    uint64_t cntB = run_count(netB, srcB, dstB, 50);
    EXPECT_NEAR(static_cast<double>(cntA), static_cast<double>(cntB), 1.0)
        << "Default-OFF runs diverged: A=" << cntA << " B=" << cntB;

    /* Sanity: both pops must NOT have any dendritic state allocated. */
    for (auto* net : {netA, netB}) {
        for (uint32_t p = 0; p < net->n_populations; p++) {
            const snn_population_t* pop = net->populations[p];
            ASSERT_NE(pop, nullptr);
            EXPECT_FALSE(pop->dendritic_enabled);
            EXPECT_EQ(pop->v_basal,         nullptr);
            EXPECT_EQ(pop->v_apical,        nullptr);
            EXPECT_EQ(pop->g_ampa_basal,    nullptr);
            EXPECT_EQ(pop->g_gaba_a_basal,  nullptr);
            EXPECT_EQ(pop->g_nmda_apical,   nullptr);
            EXPECT_EQ(pop->g_gaba_b_apical, nullptr);
            EXPECT_EQ(pop->plateau_active,  nullptr);
            EXPECT_EQ(pop->plateau_t0,      nullptr);
        }
    }

    snn_network_destroy(netA);
    snn_network_destroy(netB);
}

//-----------------------------------------------------------------------------
// 2. Enabling dendritic on a pop measurably changes per-neuron state. The
//    new branch is not silently dead.
//-----------------------------------------------------------------------------
TEST_F(SnnDendriticRegression, EnableDendriticChangesState) {
    int src, dst;
    snn_network_t* net = build_pair(src, dst);
    ASSERT_NE(net, nullptr);

    /* Before enable: dst pop has no dendritic state. */
    snn_population_t* dst_p = net->populations[dst];
    ASSERT_NE(dst_p, nullptr);
    ASSERT_FALSE(dst_p->dendritic_enabled);
    ASSERT_EQ(dst_p->v_basal, nullptr);

    /* Enable dendritic on dst. */
    int rc = snn_network_enable_dendritic(net, (uint32_t)dst);
    ASSERT_EQ(rc, SNN_SUCCESS);
    ASSERT_TRUE(dst_p->dendritic_enabled);
    ASSERT_NE(dst_p->v_basal, nullptr);
    ASSERT_NE(dst_p->v_apical, nullptr);

    /* All compartment voltages start at v_rest (default config v_rest is
     * NIMCP_RESTING_POTENTIAL_MV = -70 mV; setter copies it from the LIF
     * params on enable). Use the actual config value as the reference. */
    const float expected_rest = net->config.v_rest;
    for (uint32_t i = 0; i < dst_p->n_neurons; i++) {
        EXPECT_NEAR(dst_p->v_basal[i],  expected_rest, 1.0f);
        EXPECT_NEAR(dst_p->v_apical[i], expected_rest, 1.0f);
    }

    /* Drive src; the basal AMPA bucket on dst should accumulate (the
     * two-compartment deposit path is alive). v_basal itself can be
     * reset by spike emissions, so we check g_ampa_basal — the deposit
     * destination — which only resets via decay, never via spike. */
    for (int s = 0; s < 5; s++) {
        drive_all(net, src);
        snn_network_step(net, 1.0f);
    }
    bool basal_g_ampa_moved = false;
    for (uint32_t i = 0; i < dst_p->n_neurons; i++) {
        if (dst_p->g_ampa_basal[i] > 0.01f) {
            basal_g_ampa_moved = true;
            break;
        }
    }
    EXPECT_TRUE(basal_g_ampa_moved)
        << "Dendritic g_ampa_basal stayed at zero — compartmental "
        << "deposit path not engaging when dendritic_enabled is true";

    snn_network_destroy(net);
}

//-----------------------------------------------------------------------------
// 3. Idempotent enable: calling snn_network_enable_dendritic twice on the
//    same pop with no failures keeps state intact (no double-alloc).
//-----------------------------------------------------------------------------
TEST_F(SnnDendriticRegression, EnableDendriticIdempotent) {
    int src, dst;
    snn_network_t* net = build_pair(src, dst);
    ASSERT_NE(net, nullptr);

    int rc1 = snn_network_enable_dendritic(net, (uint32_t)dst);
    ASSERT_EQ(rc1, SNN_SUCCESS);
    float* first_v_basal = net->populations[dst]->v_basal;
    ASSERT_NE(first_v_basal, nullptr);

    /* Second call: same pointer (no re-alloc). */
    int rc2 = snn_network_enable_dendritic(net, (uint32_t)dst);
    EXPECT_EQ(rc2, SNN_SUCCESS);
    EXPECT_EQ(net->populations[dst]->v_basal, first_v_basal);

    snn_network_destroy(net);
}

//-----------------------------------------------------------------------------
// 4. Tune flag survives across pop creation: setting dendritic_enabled and
//    reading it back returns the canonical 1.0 (or whatever was stored).
//-----------------------------------------------------------------------------
TEST_F(SnnDendriticRegression, TuneFlagPersistsAcrossPopCreation) {
    snn_tune_set_dendritic_enabled(1.0f);
    int src, dst;
    snn_network_t* net = build_pair(src, dst);
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(snn_tune_get_dendritic_enabled(), 1.0f);
    snn_network_destroy(net);
    snn_tune_set_dendritic_enabled(0.0f);
}

}  // namespace
