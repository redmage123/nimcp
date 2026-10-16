/**
 * @file test_snn_fno_recording.c
 * @brief Verify the SNN-FNO recording wiring (snn_fno_record_pair fires from
 *        inside snn_network_step).
 *
 * Pre-fix, snn_fno_record_pair was a phantom in src/ — declared, implemented,
 * tested in isolation, but with no production callers. The buffer never filled,
 * snn_fno_train returned -1 every learn step, train_mse stayed 0 forever.
 *
 * This test wires the chain end-to-end:
 *   1. Create a small SNN network with 2 populations.
 *   2. Call snn_network_init_fno — populates network->fno_populations and
 *      network->fno_v_prev_buf, sets fno_recording_enabled.
 *   3. Step the network ≥16 times (the train threshold).
 *   4. Assert each FNO's buffer_count >= 16.
 *   5. Call snn_fno_train — must return 0 (success), not -1.
 *   6. Assert train_mse > 0.0 (FNO actually computed a loss).
 *
 * Coverage maps to the production hot path: snn_network_step is called by
 * 12+ cognitive bridges and the trainer. With this wiring, every step
 * automatically fills the FNO buffer.
 */

#include "nimcp.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_fno.h"
#include "snn/nimcp_snn_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
    g_failures++; } } while (0)

static int g_failures = 0;

int main(void) {
    fprintf(stderr, "=== test_snn_fno_recording ===\n");

    nimcp_init();

    /* 1. Tiny SNN: 2 populations, lightweight CSR, no GPU. */
    snn_config_t config;
    snn_config_default(&config);
    config.n_inputs = 16;
    config.n_outputs = 16;
    config.n_hidden = 0;

    snn_network_t* net = snn_network_create(&config);
    CHECK(net != NULL, "snn_network_create");
    if (!net) return 1;

    int src_id = snn_network_add_population_lightweight(net, 16, NEURON_GENERIC_LIF, "src");
    int dst_id = snn_network_add_population_lightweight(net, 16, NEURON_GENERIC_LIF, "dst");
    CHECK(src_id >= 0 && dst_id >= 0, "add_population_lightweight");

    snn_network_connect_populations(net, src_id, dst_id,
        SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.8f, 0.2f);
    snn_network_finalize_connections(net);

    /* 2. Init FNO populations on the network. */
    snn_fno_config_t fno_cfg;
    snn_fno_config_default(&fno_cfg);
    int rc = snn_network_init_fno(net, &fno_cfg);
    CHECK(rc == 0, "snn_network_init_fno");
    CHECK(net->fno_populations != NULL, "fno_populations array allocated");
    CHECK(net->fno_count == net->n_populations, "fno_count matches n_populations");
    CHECK(net->fno_v_prev_buf != NULL, "v_prev scratch allocated");
    /* Default is OFF (opt-in) since the 2026-05-09 production wedge.
     * Flip it on for the test — the unit test runs on CPU-only SNN
     * so there's no GPU-sync issue. */
    CHECK(!net->fno_recording_enabled, "recording defaults OFF");
    snn_set_fno_recording_enabled(net, true);
    CHECK(net->fno_recording_enabled, "recording enabled after setter");

    /* 3. Drive spikes into the source population — set external_current
     * high enough to make src neurons fire every step. */
    snn_population_t* src_pop = net->populations[src_id];
    if (src_pop && src_pop->external_current) {
        for (uint32_t i = 0; i < src_pop->n_neurons; i++) {
            src_pop->external_current[i] = 5.0f;  /* well above threshold */
        }
    }

    /* 4. Step the network 32 times — FNO buffer threshold is 16. */
    for (int t = 0; t < 32; t++) {
        snn_network_step(net, 0.1f);
    }

    /* 5. Each FNO must have at least 16 buffered pairs. */
    for (uint32_t p = 0; p < net->fno_count; p++) {
        snn_fno_population_t* fno = (snn_fno_population_t*)net->fno_populations[p];
        if (!fno) continue;
        if (fno->buffer_count < 16) {
            fprintf(stderr, "FAIL: pop %u buffer_count=%u (expected >= 16)\n",
                    p, fno->buffer_count);
            g_failures++;
        } else {
            fprintf(stderr, "  pop %u buffer_count=%u ✓\n", p, fno->buffer_count);
        }
    }

    /* 6. Train must succeed (return 0) and produce a non-zero MSE. */
    for (uint32_t p = 0; p < net->fno_count; p++) {
        snn_fno_population_t* fno = (snn_fno_population_t*)net->fno_populations[p];
        if (!fno) continue;
        int trc = snn_fno_train(fno, 1);
        CHECK(trc == 0, "snn_fno_train returned 0");
        float mse = snn_fno_get_train_mse(fno);
        if (!(mse > 0.0f)) {
            fprintf(stderr, "FAIL: pop %u train_mse=%f (expected > 0)\n", p, mse);
            g_failures++;
        } else {
            fprintf(stderr, "  pop %u train_mse=%f ✓\n", p, mse);
        }
    }

    /* 7. Teardown — destroy_fno is called from snn_network_destroy. */
    snn_network_destroy(net);

    nimcp_shutdown();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
