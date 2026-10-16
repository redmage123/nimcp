//=============================================================================
// test_snn_conduction_delay_regression.cpp
//=============================================================================
/**
 * @file test_snn_conduction_delay_regression.cpp
 * @brief Wave E regression — conduction_delay_steps = 0 (default) MUST
 *        produce bit-identical deposit behavior to a network where the
 *        setter was never called. This is the OFF-mode regression
 *        contract: the new ring buffer machinery must be an observable
 *        no-op when every pop has delay 0.
 *
 * WHAT: Two ways to validate the OFF-mode contract:
 *       1. Default delay (0) on both pops produces the SAME g_ampa
 *          trajectory as explicit-zero on both pops over multiple steps.
 *       2. Setting one pop's delay > 0 measurably changes the deposit
 *          timing — pinning that the new branch is not silently dead.
 *
 * WHY:  Wave E adds a deposit-time read indirection. We must pin that
 *       the default-OFF case (delay=0 for every pop) behaves the same
 *       as if the ring were not present — otherwise downstream training
 *       checkpoints could silently change.
 *
 * HOW:  Google Test. Builds 2-pop networks with src→dst AMPA weight 1.0.
 *       Drives src with strong external current so it fires within step 0
 *       (LIF integrates → spike → end-of-step writes ring); checks that
 *       dst's g_ampa trajectory after subsequent steps matches the
 *       expected behavior for each delay setting.
 *
 * See docs/claude/ffi-timing-audit-2026-04-27.md.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_synapse.h"
#include "utils/tensor/nimcp_tensor.h"
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

class SnnConductionDelayRegression : public ::testing::Test {
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
        snn_tune_set_conductance_enabled(1.0f);   /* CB ON — observe g_ampa */
        snn_tune_set_cb_weights_rescaled(1.0f);
    }

    void TearDown() override {
        snn_tune_set_conductance_enabled(0.0f);
    }

    /* Build a 2-pop net (src → dst, full topology, AMPA weight 1.0) and
     * return src/dst ids via out params. Caller must destroy net. */
    snn_network_t* build_pair(int& src_id_out, int& dst_id_out) {
        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = 1;
        cfg.n_outputs = 1;
        cfg.n_hidden  = 0;
        cfg.dt        = 1.0f;
        snn_network_t* net = snn_network_create(&cfg);
        if (!net) return nullptr;

        int src_id = snn_network_add_population_lightweight(
            net, 4, NEURON_GENERIC_LIF, "src");
        int dst_id = snn_network_add_population_lightweight(
            net, 4, NEURON_GENERIC_LIF, "dst");
        if (src_id < 0 || dst_id < 0) {
            snn_network_destroy(net);
            return nullptr;
        }

        int nc = snn_network_connect_populations(
            net, (uint32_t)src_id, (uint32_t)dst_id,
            SNN_TOPO_FULL, 1.0f,
            SYNAPSE_AMPA, 1.0f, 0.0f);
        if (nc <= 0) {
            snn_network_destroy(net);
            return nullptr;
        }
        snn_network_finalize_connections(net);

        src_id_out = src_id;
        dst_id_out = dst_id;
        return net;
    }

    /* Initialize a pop to "ready to receive deposits but not fire on its
     * own" — V at rest, no refractory (so the deposit kernel runs), no
     * spikes, all g_X cleared. NOTE: a `pin in refractory` shortcut would
     * skip the deposit code path entirely (the per-neuron loop has
     * `if (ref > 0) continue;` at the top). We zero refractory and rely
     * on physics: V starts at -65, single-step weak deposit won't reach
     * threshold, so no spurious spikes. */
    void prep_pop_for_deposit(snn_population_t* pop) {
        float* v   = (float*)nimcp_tensor_data(pop->membrane_v);
        float* ref = (float*)nimcp_tensor_data(pop->refractory);
        float* sp  = (float*)nimcp_tensor_data(pop->spike_output);
        for (uint32_t i = 0; i < pop->n_neurons; i++) {
            v[i]   = -65.0f;
            ref[i] = 0.0f;
            sp[i]  = 0.0f;
        }
        if (pop->g_ampa)   for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_ampa[i] = 0.0f;
        if (pop->g_nmda)   for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_nmda[i] = 0.0f;
        if (pop->g_gaba_a) for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_gaba_a[i] = 0.0f;
        if (pop->g_gaba_b) for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_gaba_b[i] = 0.0f;
    }

    /* Reset src to fire once on step 0, then go quiet. We drive with
     * external_current and keep src out of refractory only at start of
     * step 0; after the test trace is captured, the LIF integrator
     * naturally puts src into its post-spike refractory. */
    void prime_src_to_fire(snn_population_t* src) {
        float* v   = (float*)nimcp_tensor_data(src->membrane_v);
        float* ref = (float*)nimcp_tensor_data(src->refractory);
        for (uint32_t i = 0; i < src->n_neurons; i++) {
            v[i]   = -65.0f;
            ref[i] = 0.0f;
        }
        for (uint32_t i = 0; i < src->n_neurons; i++) {
            src->external_current[i] = 100.0f;
        }
    }
};

//-----------------------------------------------------------------------------
// 1. Default delay (0) on both pops: bit-identity contract — at delay=0
//    the deposit kernel reads src's LIVE spike_output (same-tick),
//    matching pre-Wave-E behavior. A spike emitted on step 0 by src is
//    visible to dst's deposit pass the SAME step (since src has smaller
//    pop_id and is processed first in the population loop).
//
//    The spike-history ring buffer is bypassed at delay=0 — deposit
//    reads spike_output directly. This is the OFF-mode regression
//    contract: existing checkpoints / training tests must not regress.
//-----------------------------------------------------------------------------
TEST_F(SnnConductionDelayRegression, DefaultDelayProducesSameTickDeposit) {
    int src_id = -1, dst_id = -1;
    snn_network_t* net = build_pair(src_id, dst_id);
    ASSERT_NE(net, nullptr);

    snn_population_t* src = snn_network_get_population(net, (uint32_t)src_id);
    snn_population_t* dst = snn_network_get_population(net, (uint32_t)dst_id);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);

    /* Default delay must be 0 for both pops. */
    EXPECT_EQ(src->conduction_delay_steps, 0);
    EXPECT_EQ(dst->conduction_delay_steps, 0);

    prep_pop_for_deposit(dst);
    prime_src_to_fire(src);

    /* Step 0: src fires AND dst's deposit pass reads src.spike_output
     * same-tick (delay=0 bypasses the ring per the bit-identity contract).
     * dst.g_ampa rises immediately — pre-Wave-E semantics. */
    ASSERT_GE(snn_network_step(net, 1.0f), 0);
    EXPECT_GT(dst->g_ampa[0], 0.0f)
        << "Default delay=0: deposit should fire SAME-TICK on step 0 "
           "(legacy semantics). Got g_ampa[0]=" << dst->g_ampa[0];

    snn_network_destroy(net);
}

//-----------------------------------------------------------------------------
// 2. Sanity / not-dead test: setting delay > 0 actually shifts arrival.
//    With src delay = 2, the deposit fires on step 3 instead of step 1.
//-----------------------------------------------------------------------------
TEST_F(SnnConductionDelayRegression, NonzeroDelayShiftsArrival) {
    int src_id = -1, dst_id = -1;
    snn_network_t* net = build_pair(src_id, dst_id);
    ASSERT_NE(net, nullptr);

    EXPECT_EQ(snn_network_set_pop_conduction_delay(
        net, (uint32_t)src_id, 2), 0);

    snn_population_t* src = snn_network_get_population(net, (uint32_t)src_id);
    snn_population_t* dst = snn_network_get_population(net, (uint32_t)dst_id);
    prep_pop_for_deposit(dst);
    prime_src_to_fire(src);

    /* Step 0: src fires; deposit pass reads slot (0-1-2) mod SLOTS =
     * SLOTS - 3 (zero-initialized) ⇒ no deposit. End-of-step writes
     * spike_output → slot 0, head→1. */
    ASSERT_GE(snn_network_step(net, 1.0f), 0);
    EXPECT_FLOAT_EQ(dst->g_ampa[0], 0.0f) << "step 0: empty ring";

    /* Drop src drive + pin in refractory so it doesn't fire again. */
    for (uint32_t i = 0; i < 4; i++) src->external_current[i] = 0.0f;
    {
        float* sref = (float*)nimcp_tensor_data(src->refractory);
        for (uint32_t i = 0; i < 4; i++) sref[i] = 1000.0f;
    }

    /* Step 1: re-pin dst (its g_ampa decays naturally so we measure
     * fresh deposit only). slot = (1-1-2) mod SLOTS = SLOTS - 2 → empty. */
    prep_pop_for_deposit(dst);
    ASSERT_GE(snn_network_step(net, 1.0f), 0);
    EXPECT_FLOAT_EQ(dst->g_ampa[0], 0.0f) << "step 1: delay=2 not yet elapsed";

    /* Step 2: slot = (2-1-2) mod SLOTS = SLOTS - 1 → empty. */
    prep_pop_for_deposit(dst);
    ASSERT_GE(snn_network_step(net, 1.0f), 0);
    EXPECT_FLOAT_EQ(dst->g_ampa[0], 0.0f) << "step 2: delay=2 not yet elapsed";

    /* Step 3: slot = (3-1-2) mod SLOTS = 0 → holds the step-0 snapshot.
     * Deposit fires. */
    prep_pop_for_deposit(dst);
    ASSERT_GE(snn_network_step(net, 1.0f), 0);
    EXPECT_GT(dst->g_ampa[0], 0.0f)
        << "step 3: delay=2 elapsed, deposit should fire; g_ampa[0]="
        << dst->g_ampa[0];

    snn_network_destroy(net);
}

//-----------------------------------------------------------------------------
// 3. Bit-identity contract: with both pops at default delay 0, the deposit
//    behavior is identical to a network where the setter was explicitly
//    called with 0. Pins the OFF-mode regression contract.
//-----------------------------------------------------------------------------
TEST_F(SnnConductionDelayRegression, ExplicitZeroMatchesDefault) {
    /* Network A: never set delay. */
    int src_a = -1, dst_a = -1;
    snn_network_t* net_a = build_pair(src_a, dst_a);
    ASSERT_NE(net_a, nullptr);

    /* Network B: explicitly set delay = 0 on both pops. */
    int src_b = -1, dst_b = -1;
    snn_network_t* net_b = build_pair(src_b, dst_b);
    ASSERT_NE(net_b, nullptr);
    EXPECT_EQ(snn_network_set_pop_conduction_delay(
        net_b, (uint32_t)src_b, 0), 0);
    EXPECT_EQ(snn_network_set_pop_conduction_delay(
        net_b, (uint32_t)dst_b, 0), 0);

    snn_population_t* src_pa = snn_network_get_population(net_a, (uint32_t)src_a);
    snn_population_t* dst_pa = snn_network_get_population(net_a, (uint32_t)dst_a);
    snn_population_t* src_pb = snn_network_get_population(net_b, (uint32_t)src_b);
    snn_population_t* dst_pb = snn_network_get_population(net_b, (uint32_t)dst_b);

    /* Drive both networks identically with strong currents and observe
     * dst.g_ampa trajectory across 4 steps. Must be bit-identical. */
    prep_pop_for_deposit(dst_pa);
    prep_pop_for_deposit(dst_pb);
    prime_src_to_fire(src_pa);
    prime_src_to_fire(src_pb);

    ASSERT_GE(snn_network_step(net_a, 1.0f), 0);
    ASSERT_GE(snn_network_step(net_b, 1.0f), 0);
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(dst_pa->g_ampa[i], dst_pb->g_ampa[i])
            << "step 0 neuron " << i;
    }

    /* Drop drives + pin everything for clean comparison. */
    for (uint32_t i = 0; i < 4; i++) {
        src_pa->external_current[i] = 0.0f;
        src_pb->external_current[i] = 0.0f;
    }
    {
        float* sra = (float*)nimcp_tensor_data(src_pa->refractory);
        float* srb = (float*)nimcp_tensor_data(src_pb->refractory);
        for (uint32_t i = 0; i < 4; i++) {
            sra[i] = 1000.0f;
            srb[i] = 1000.0f;
        }
    }

    for (int step = 1; step < 4; step++) {
        prep_pop_for_deposit(dst_pa);
        prep_pop_for_deposit(dst_pb);
        ASSERT_GE(snn_network_step(net_a, 1.0f), 0);
        ASSERT_GE(snn_network_step(net_b, 1.0f), 0);
        for (uint32_t i = 0; i < 4; i++) {
            EXPECT_FLOAT_EQ(dst_pa->g_ampa[i], dst_pb->g_ampa[i])
                << "step=" << step << " neuron=" << i
                << " (default vs explicit-zero must be bit-identical)";
        }
    }

    snn_network_destroy(net_a);
    snn_network_destroy(net_b);
}

}  // namespace
