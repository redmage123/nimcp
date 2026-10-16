/**
 * @file test_snn_conduction_delay_integration.c
 * @brief Wave E FFI fix — Integration test for per-pop conduction delay.
 * @date 2026-04-27
 *
 * WHAT: Builds a 2-pop lightweight CSR network: src → dst with src's
 *       conduction_delay_steps = 3. Pre-populates src's spike-history
 *       ring with a known spike pattern at a slot chosen so a
 *       snn_network_step deposit pass reads "delay-3-old" → triggers
 *       dst.g_ampa > 0 immediately. Verifies that with a different
 *       slot (delay would point at zero history), dst.g_ampa stays 0.
 * WHY:  Pre-Wave-E the deposit kernel read src->spike_output for the
 *       SAME tick the spike was emitted, so deposit landed in the same
 *       step ⇒ effective conduction delay = 0. The Wave E fix introduces
 *       a per-pop spike-history ring and reads `delay`-step-old snapshots.
 *       This test checks the ring buffer is actually consulted during
 *       deposit and the index math is correct end-to-end.
 * HOW:  Check framework. Uses lightweight CSR pops + CB ON (so g_ampa is
 *       the receptor we observe — same path as the production hot loop).
 *       Pre-writes spike_history directly so we control exactly which
 *       past snapshot the deposit kernel sees.
 *
 *       NOTE: the lightweight step loop zeros spike_output at the START
 *       of each per-neuron iteration, so naive injection via writing
 *       spike_output before snn_network_step is overwritten. Pre-writing
 *       the spike-history ring is the deterministic alternative.
 *
 * See docs/claude/ffi-timing-audit-2026-04-27.md.
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_synapse.h"
#include "utils/tensor/nimcp_tensor.h"

/* SNN tunables (extern decls — no header). Mirrors the gap-junction test. */
extern void  snn_tune_set_conductance_enabled(float);
extern void  snn_tune_set_cb_weights_rescaled(float);
extern void  snn_tune_set_noise_rate_hz(float);
extern void  snn_tune_set_basket_enabled(float);
extern void  snn_tune_set_ahp_enabled(float);
extern void  snn_tune_set_pump_enabled(float);
extern void  snn_tune_set_substrate_enabled(float);

static snn_network_t* g_net = NULL;

static void reset_tunables(void) {
    snn_tune_set_noise_rate_hz(0.0f);
    snn_tune_set_basket_enabled(0.0f);
    snn_tune_set_ahp_enabled(0.0f);
    snn_tune_set_pump_enabled(0.0f);
    snn_tune_set_substrate_enabled(0.0f);
    snn_tune_set_conductance_enabled(1.0f);   /* CB ON: deposits into g_ampa */
    snn_tune_set_cb_weights_rescaled(1.0f);   /* skip rescale guard */
}

static snn_network_t* fresh_net(void) {
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;
    return snn_network_create(&cfg);
}

static void setup(void) {
    reset_tunables();
    g_net = fresh_net();
    ck_assert_ptr_nonnull(g_net);
}

static void teardown(void) {
    if (g_net) {
        snn_network_destroy(g_net);
        g_net = NULL;
    }
    snn_tune_set_conductance_enabled(0.0f);
}

/* Initialize a pop to "ready to receive deposits but not fire on its own"
 * — V at rest, no refractory (so the deposit kernel runs), no spikes,
 * all g_X cleared. The LIF integrator will run but with V starting at
 * v_rest and only weak deposits over a single step, V will not reach
 * threshold — no spurious spikes.
 *
 * NOTE: a `pin in refractory` shortcut would skip the deposit code path
 * entirely (the per-neuron loop has `if (ref > 0) continue;` at the top).
 * We therefore zero refractory and rely on physics. */
static void prep_pop_for_deposit(snn_population_t* pop) {
    float* v   = (float*)nimcp_tensor_data(pop->membrane_v);
    float* ref = (float*)nimcp_tensor_data(pop->refractory);
    float* sp  = (float*)nimcp_tensor_data(pop->spike_output);
    for (uint32_t i = 0; i < pop->n_neurons; i++) {
        v[i]   = -65.0f;
        ref[i] = 0.0f;       /* no refractory — deposit code path runs */
        sp[i]  = 0.0f;
    }
    if (pop->g_ampa) {
        for (uint32_t i = 0; i < pop->n_neurons; i++) pop->g_ampa[i] = 0.0f;
    }
    if (pop->g_nmda)   for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_nmda[i]   = 0.0f;
    if (pop->g_gaba_a) for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_gaba_a[i] = 0.0f;
    if (pop->g_gaba_b) for (uint32_t i=0;i<pop->n_neurons;i++) pop->g_gaba_b[i] = 0.0f;
}

/*
 * Test 1: with src->conduction_delay_steps = 3, a spike pattern stored in
 * the ring slot that matches "3 steps old at the upcoming deposit pass"
 * triggers dst->g_ampa > 0 at the next snn_network_step call.
 *
 * The deposit pass at step k reads src->spike_history at slot
 *    (head - 1 - delay) mod SLOTS
 * where head is src->spike_history_head AT THE TIME the deposit pass runs
 * (i.e. before the end-of-step write advances it).
 *
 * Strategy: set head to a chosen value H, write the spike pattern into
 * slot S = (H - 1 - 3) mod SLOTS, then call snn_network_step ONCE. The
 * deposit pass for dst will read slot S and see the spikes. After the
 * step we check dst->g_ampa[*] > 0.
 */
START_TEST(test_delayed_spike_arrives_at_correct_step)
{
    int src_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "src");
    int dst_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "dst");
    ck_assert_int_ge(src_id, 0);
    ck_assert_int_ge(dst_id, 0);

    /* Wire src → dst BEFORE finalize. Full connectivity, AMPA, +weight. */
    int nc = snn_network_connect_populations(
        g_net,
        (uint32_t)src_id,
        (uint32_t)dst_id,
        SNN_TOPO_FULL,
        1.0f,
        SYNAPSE_AMPA,
        1.0f,                /* weight mean */
        0.0f);               /* weight stddev */
    ck_assert_int_gt(nc, 0);

    snn_network_finalize_connections(g_net);

    const uint32_t DELAY = 3;
    int rc = snn_network_set_pop_conduction_delay(
        g_net, (uint32_t)src_id, DELAY);
    ck_assert_int_eq(rc, 0);

    snn_population_t* src = snn_network_get_population(g_net, (uint32_t)src_id);
    snn_population_t* dst = snn_network_get_population(g_net, (uint32_t)dst_id);
    ck_assert_ptr_nonnull(src);
    ck_assert_ptr_nonnull(dst);
    ck_assert_uint_eq(src->conduction_delay_steps, DELAY);
    ck_assert_ptr_nonnull(src->spike_history);
    ck_assert_ptr_nonnull(dst->g_ampa);

    prep_pop_for_deposit(src);
    prep_pop_for_deposit(dst);

    /* Choose head H = 4. Then slot read by deposit at delay=3 is
     *   (4 + SLOTS - 1 - 3) mod SLOTS = SLOTS mod SLOTS = 0
     * Write the spike pattern at slot 0. */
    src->spike_history_head = 4;
    const size_t row_off = 0;  /* slot 0 */
    for (uint32_t i = 0; i < src->n_neurons; i++) {
        src->spike_history[row_off + i] = 1.0f;
    }
    /* Also zero ALL other slots so we know any deposit comes from slot 0. */
    for (uint32_t s = 1; s < SNN_SPIKE_HISTORY_SLOTS; s++) {
        for (uint32_t i = 0; i < src->n_neurons; i++) {
            src->spike_history[(size_t)s * src->n_neurons + i] = 0.0f;
        }
    }

    /* Step once. Deposit pass for dst will read src->spike_history slot 0
     * (fresh-1 spikes ago, but we set head=4 so "delay 3 ago" lands on
     * slot 0). All 4 src "spikes" land on each of dst's 4 neurons via
     * full connectivity ⇒ each dst neuron sees 4 deposits, g_ampa rises. */
    int sr = snn_network_step(g_net, 1.0f);
    ck_assert_int_ge(sr, 0);

    for (uint32_t i = 0; i < 4; i++) {
        ck_assert_msg(dst->g_ampa[i] > 0.5f,
                      "dst.g_ampa[%u] should be substantially > 0 after delayed "
                      "spike arrival; got %f", i, dst->g_ampa[i]);
    }
}
END_TEST

/*
 * Test 2: when delay points at a ZERO slot (no past spike), no deposit
 * occurs even with a non-zero ring elsewhere. Pins the contract that
 * the ring is consulted at the CORRECT slot — not "any non-zero slot
 * triggers deposit" (which would be a flat-walking-off-the-end bug).
 */
START_TEST(test_delay_reads_correct_slot_only)
{
    int src_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "src2");
    int dst_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "dst2");
    ck_assert_int_ge(src_id, 0);
    ck_assert_int_ge(dst_id, 0);

    int nc = snn_network_connect_populations(
        g_net, (uint32_t)src_id, (uint32_t)dst_id,
        SNN_TOPO_FULL, 1.0f,
        SYNAPSE_AMPA, 1.0f, 0.0f);
    ck_assert_int_gt(nc, 0);
    snn_network_finalize_connections(g_net);

    const uint32_t DELAY = 3;
    ck_assert_int_eq(snn_network_set_pop_conduction_delay(
        g_net, (uint32_t)src_id, DELAY), 0);

    snn_population_t* src = snn_network_get_population(g_net, (uint32_t)src_id);
    snn_population_t* dst = snn_network_get_population(g_net, (uint32_t)dst_id);
    prep_pop_for_deposit(src);
    prep_pop_for_deposit(dst);

    /* Head = 4 ⇒ delay=3 reads slot 0. We deliberately put spikes in slot
     * 1 (which would be read by delay=2) and leave slot 0 zero. With
     * delay=3 the deposit pass should see no spikes. */
    src->spike_history_head = 4;
    for (uint32_t s = 0; s < SNN_SPIKE_HISTORY_SLOTS; s++) {
        for (uint32_t i = 0; i < src->n_neurons; i++) {
            src->spike_history[(size_t)s * src->n_neurons + i] = 0.0f;
        }
    }
    /* Plant spikes at slot 1 (read by delay=2, NOT by delay=3). */
    for (uint32_t i = 0; i < src->n_neurons; i++) {
        src->spike_history[(size_t)1 * src->n_neurons + i] = 1.0f;
    }

    int sr = snn_network_step(g_net, 1.0f);
    ck_assert_int_ge(sr, 0);

    for (uint32_t i = 0; i < 4; i++) {
        ck_assert_msg(dst->g_ampa[i] < 1e-3f,
                      "dst.g_ampa[%u] should be ~0 (delay=3 reads slot 0 which "
                      "is empty; spikes at slot 1 should NOT leak through); "
                      "got %f", i, dst->g_ampa[i]);
    }
}
END_TEST

/*
 * Test 3: with delay=0 (default), the deposit kernel reads the live
 * spike_output buffer same-tick (legacy pre-Wave-E semantics — bit-
 * identity contract). After step 0 fires src spikes via the LIF
 * integrator (driven by external_current), the SAME step's deposit pass
 * for dst sees src's just-fired spikes and lands g_ampa > 0 on step 0.
 *
 * This pins the bit-identity contract: delay=0 must behave EXACTLY like
 * pre-Wave-E (same-tick deposit) so existing checkpoints and tests don't
 * silently regress.
 */
START_TEST(test_default_delay_same_tick_propagation)
{
    int src_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "src3");
    int dst_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "dst3");
    ck_assert_int_ge(src_id, 0);
    ck_assert_int_ge(dst_id, 0);

    int nc = snn_network_connect_populations(
        g_net, (uint32_t)src_id, (uint32_t)dst_id,
        SNN_TOPO_FULL, 1.0f,
        SYNAPSE_AMPA, 1.0f, 0.0f);
    ck_assert_int_gt(nc, 0);
    snn_network_finalize_connections(g_net);

    snn_population_t* src = snn_network_get_population(g_net, (uint32_t)src_id);
    snn_population_t* dst = snn_network_get_population(g_net, (uint32_t)dst_id);
    /* Default delay = 0 (no setter call). */
    ck_assert_uint_eq(src->conduction_delay_steps, 0);

    prep_pop_for_deposit(dst);
    prep_pop_for_deposit(src);  /* zero refractory so LIF runs */

    /* Drive src with a strong external current so all 4 neurons cross
     * threshold within step 0. external_current is in mV-equivalent
     * scale; 100 mV is well above v_thresh - v_rest ~ 15 mV. */
    ck_assert_ptr_nonnull(src->external_current);
    for (uint32_t i = 0; i < 4; i++) {
        src->external_current[i] = 100.0f;
    }

    /* Step 0: with default delay=0, the deposit kernel reads src's LIVE
     * spike_output (NOT the spike-history ring, per the bit-identity
     * contract for delay=0). When src is processed BEFORE dst in the
     * pop loop (src has the smaller pop_id), src's fired spikes from
     * THIS step are visible to dst's deposit pass SAME-tick. dst.g_ampa
     * rises on step 0. This is the legacy pre-Wave-E behavior preserved. */
    int sr = snn_network_step(g_net, 1.0f);
    ck_assert_int_ge(sr, 0);

    /* Confirm src actually fired. spike_output should hold 1.0 for at
     * least one neuron (CB mode integrates the current; with strong drive
     * + tau_mem = 10 ms and dt=1 ms the membrane crosses thresh quickly). */
    float* src_spikes = (float*)nimcp_tensor_data(src->spike_output);
    int src_fire_count = 0;
    for (uint32_t i = 0; i < 4; i++) {
        if (src_spikes[i] > 0.5f) src_fire_count++;
    }
    ck_assert_msg(src_fire_count > 0,
                  "src failed to fire on step 0 with external_current=100 "
                  "(test setup invalid)");

    /* dst.g_ampa should be > 0 on step 0 — same-tick deposit through
     * the live spike_output read at delay=0. */
    int dst_received = 0;
    for (uint32_t i = 0; i < 4; i++) {
        if (dst->g_ampa[i] > 0.5f) dst_received++;
    }
    ck_assert_msg(dst_received > 0,
                  "dst.g_ampa stayed 0 on step 0 — default delay=0 should "
                  "produce SAME-TICK deposit (legacy bit-identity contract). "
                  "g_ampa=[%f,%f,%f,%f]",
                  dst->g_ampa[0], dst->g_ampa[1],
                  dst->g_ampa[2], dst->g_ampa[3]);
}
END_TEST

static Suite* conduction_delay_suite(void) {
    Suite* s = suite_create("SNN Conduction Delay (Wave E)");

    TCase* tc_timing = tcase_create("Delayed spike arrival timing");
    tcase_add_checked_fixture(tc_timing, setup, teardown);
    tcase_add_test(tc_timing, test_delayed_spike_arrives_at_correct_step);
    tcase_set_timeout(tc_timing, 60);
    suite_add_tcase(s, tc_timing);

    TCase* tc_slot = tcase_create("Delay reads correct slot only");
    tcase_add_checked_fixture(tc_slot, setup, teardown);
    tcase_add_test(tc_slot, test_delay_reads_correct_slot_only);
    tcase_set_timeout(tc_slot, 60);
    suite_add_tcase(s, tc_slot);

    TCase* tc_default = tcase_create("Default delay same-tick propagation");
    tcase_add_checked_fixture(tc_default, setup, teardown);
    tcase_add_test(tc_default, test_default_delay_same_tick_propagation);
    tcase_set_timeout(tc_default, 60);
    suite_add_tcase(s, tc_default);

    return s;
}

int main(void) {
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }
    Suite* s = conduction_delay_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    nimcp_shutdown();
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
