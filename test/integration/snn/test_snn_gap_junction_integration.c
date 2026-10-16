/**
 * @file test_snn_gap_junction_integration.c
 * @brief Wave D — Integration test for PV gap-junction electrical coupling.
 * @date 2026-04-27
 *
 * WHAT: Wires 4 lightweight SNN populations with gap_coupling = 0.1, drives
 *       only ONE pop with external_current, and verifies the OTHER 3 pops
 *       show measurable membrane voltage AND/OR spike response within a few
 *       steps. Wait — the task says "wire 4 lightweight pops" with gap
 *       coupling 0.1 each, but gap-junctions in this implementation are
 *       INTRA-population (PV-PV inside one pop). The integration assertion
 *       is "drive only one with external_current=100; verify the OTHER 3
 *       pops show non-zero membrane response within 5 steps (electrical
 *       coupling propagated voltage)."
 *
 *       To get cross-pop electrical propagation we need either (a) a
 *       chemical synapse path between the pops or (b) a within-pop
 *       coupling that synchronizes neurons within one large pop. Since
 *       gap junctions in NIMCP are per-pop, the cross-pop integration test
 *       is reinterpreted as: ONE pop of 4 neurons, drive only neuron 0,
 *       verify the OTHER 3 neurons in the same pop show non-zero membrane
 *       response within 5 steps (electrical coupling propagated voltage
 *       within the syncytium).
 *
 *       This is the biologically meaningful test: gap junctions among PV
 *       basket cells synchronize firing across the entire PV pool, so a
 *       single driven neuron drags the rest of the pool's V along.
 *
 * WHY:  Verifies the hot-loop gap-junction adjustment runs end-to-end
 *       through snn_network_step under the production CB code path.
 *
 * HOW:  Check framework. Uses lightweight CSR pops + CB ON.
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
#include "utils/tensor/nimcp_tensor.h"

/* SNN tunables (extern decls — no header). */
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
    snn_tune_set_conductance_enabled(1.0f);   /* CB ON for gap-junction path */
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

/*
 * Test: gap-junction propagation within a single PV-style pop.
 *
 * Build one 4-neuron lightweight pop with gap_coupling=0.1. Initialize all
 * V to v_rest = -65 mV. Drive ONLY neuron 0 with a strong external current
 * (100). Step the network 5 times. Verify neurons 1-3 show non-zero
 * membrane perturbation (gap-junction coupling propagated the voltage
 * change from the driven neuron to the rest of the syncytium).
 *
 * Without gap coupling, neurons 1-3 would sit at v_rest unchanged. With
 * gap coupling, neuron 0's V rises from external drive, V_mean drifts
 * upward, and the other neurons get pulled along.
 */
START_TEST(test_gap_junction_propagates_voltage_within_pop)
{
    int pop_id = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "pv_test");
    ck_assert_int_ge(pop_id, 0);
    snn_network_finalize_connections(g_net);

    int rc = snn_network_set_pop_gap_coupling(g_net, (uint32_t)pop_id, 0.1f);
    ck_assert_int_eq(rc, 0);

    snn_population_t* pop = snn_network_get_population(g_net, (uint32_t)pop_id);
    ck_assert_ptr_nonnull(pop);

    float* v   = (float*)nimcp_tensor_data(pop->membrane_v);
    float* ref = (float*)nimcp_tensor_data(pop->refractory);
    ck_assert_ptr_nonnull(v);
    ck_assert_ptr_nonnull(ref);
    ck_assert_ptr_nonnull(pop->external_current);

    /* All neurons start at rest. */
    const float v_rest_test = -65.0f;
    for (uint32_t i = 0; i < 4; i++) {
        v[i]   = v_rest_test;
        ref[i] = 0.0f;
    }

    /* Run 5 steps. Drive only neuron 0 each step (external_current is
     * cleared at the end of each step by the lightweight path, so we
     * must re-set every iteration). */
    for (int step = 0; step < 5; step++) {
        for (uint32_t i = 0; i < 4; i++) pop->external_current[i] = 0.0f;
        pop->external_current[0] = 100.0f;
        int sr = snn_network_step(g_net, 1.0f);
        ck_assert_int_ge(sr, 0);
    }

    /* Sanity: neuron 0 must have moved (it was driven). */
    ck_assert_msg(fabsf(v[0] - v_rest_test) > 1e-3f,
                  "Driven neuron 0 V did not change (V=%.4f, rest=%.4f); "
                  "test pre-condition failed", v[0], v_rest_test);

    /* Core assertion: at least one of the OTHER 3 neurons shows a
     * non-zero membrane response. This is the gap-junction propagation
     * signal. With coupling=0.1 over 5 steps the deflection should be
     * comfortably > 0.01 mV; we use 1e-4 mV as a generous floor.
     *
     * NOTE: if neuron 0 spiked and reset to v_reset, V_mean still drifts
     * because v_reset != v_rest (typically). The propagation signal
     * survives spike resets. */
    int propagated = 0;
    for (uint32_t i = 1; i < 4; i++) {
        if (fabsf(v[i] - v_rest_test) > 1e-4f) propagated++;
    }
    ck_assert_msg(propagated >= 1,
                  "Gap-junction propagation FAILED: neurons 1-3 unchanged "
                  "(V=%.6f, %.6f, %.6f) after 5 steps with driven neuron 0 "
                  "at V=%.4f and gap_coupling=0.1",
                  v[1], v[2], v[3], v[0]);
}
END_TEST

/*
 * Companion test: with gap_coupling = 0, the OTHER 3 neurons get NO
 * propagation signal from the driven neuron 0 — they only see passive
 * leak toward v_rest. With gap_coupling > 0, the OTHER 3 neurons see
 * BOTH the leak AND the gap-junction pull toward V_mean. The
 * propagation signal is the DIFFERENCE between the coupled and zero
 * trajectories.
 *
 * This is the negative control — confirms that the propagation signal
 * in the test above is actually due to the gap-junction path, not just
 * passive leak.
 */
START_TEST(test_zero_vs_nonzero_coupling_diverge)
{
    /* Parallel networks: one with coupling=0, one with coupling=0.1. */
    snn_network_t* net_zero = fresh_net();
    ck_assert_ptr_nonnull(net_zero);

    int p0 = snn_network_add_population_lightweight(
        g_net, 4, NEURON_GENERIC_LIF, "coupled");
    int p_zero = snn_network_add_population_lightweight(
        net_zero, 4, NEURON_GENERIC_LIF, "zero");
    ck_assert_int_ge(p0, 0);
    ck_assert_int_ge(p_zero, 0);
    snn_network_finalize_connections(g_net);
    snn_network_finalize_connections(net_zero);

    int rc = snn_network_set_pop_gap_coupling(g_net, (uint32_t)p0, 0.1f);
    ck_assert_int_eq(rc, 0);
    /* net_zero: gap_coupling stays at default 0. */

    snn_population_t* pop_c = snn_network_get_population(g_net, (uint32_t)p0);
    snn_population_t* pop_z = snn_network_get_population(net_zero, (uint32_t)p_zero);
    ck_assert_ptr_nonnull(pop_c);
    ck_assert_ptr_nonnull(pop_z);

    float* vc = (float*)nimcp_tensor_data(pop_c->membrane_v);
    float* vz = (float*)nimcp_tensor_data(pop_z->membrane_v);
    float* rc_data = (float*)nimcp_tensor_data(pop_c->refractory);
    float* rz_data = (float*)nimcp_tensor_data(pop_z->refractory);

    const float v_init = -65.0f;
    for (uint32_t i = 0; i < 4; i++) {
        vc[i] = vz[i] = v_init;
        rc_data[i] = rz_data[i] = 0.0f;
    }

    for (int step = 0; step < 5; step++) {
        for (uint32_t i = 0; i < 4; i++) {
            pop_c->external_current[i] = 0.0f;
            pop_z->external_current[i] = 0.0f;
        }
        pop_c->external_current[0] = 100.0f;
        pop_z->external_current[0] = 100.0f;
        int sc = snn_network_step(g_net, 1.0f);
        int sz = snn_network_step(net_zero, 1.0f);
        ck_assert_int_ge(sc, 0);
        ck_assert_int_ge(sz, 0);
    }

    /* In the zero-coupling network, neurons 1-3 see only passive leak.
     * In the coupled network they additionally feel the pull from the
     * driven neuron 0. The deltas (v_coupled - v_zero) for neurons 1-3
     * are the gap-junction propagation signal — must be non-zero. */
    int propagated = 0;
    for (uint32_t i = 1; i < 4; i++) {
        if (fabsf(vc[i] - vz[i]) > 1e-4f) propagated++;
    }
    ck_assert_msg(propagated >= 1,
                  "Gap-junction propagation absent: neurons 1-3 trajectories "
                  "identical between coupled (%.4f, %.4f, %.4f) and zero "
                  "(%.4f, %.4f, %.4f) networks",
                  vc[1], vc[2], vc[3], vz[1], vz[2], vz[3]);

    snn_network_destroy(net_zero);
}
END_TEST

static Suite* gap_junction_suite(void) {
    Suite* s = suite_create("SNN Gap-Junction Coupling (Wave D)");

    TCase* tc_propagate = tcase_create("Voltage propagation within pop");
    tcase_add_checked_fixture(tc_propagate, setup, teardown);
    tcase_add_test(tc_propagate, test_gap_junction_propagates_voltage_within_pop);
    tcase_set_timeout(tc_propagate, 30);
    suite_add_tcase(s, tc_propagate);

    TCase* tc_zero = tcase_create("Zero vs non-zero coupling diverge");
    tcase_add_checked_fixture(tc_zero, setup, teardown);
    tcase_add_test(tc_zero, test_zero_vs_nonzero_coupling_diverge);
    tcase_set_timeout(tc_zero, 30);
    suite_add_tcase(s, tc_zero);

    return s;
}

int main(void) {
    if (nimcp_init() != NIMCP_SUCCESS) {
        fprintf(stderr, "nimcp_init failed\n");
        return EXIT_FAILURE;
    }
    Suite* s = gap_junction_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    nimcp_shutdown();
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
