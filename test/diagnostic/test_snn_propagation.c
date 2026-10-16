/**
 * @file test_snn_propagation.c
 * @brief Isolated SNN propagation test — tune weights without the full brain.
 *
 * Creates a minimal 3-population SNN:
 *   src (input pop, non-lightweight) → mid (lightweight CSR) → dst (lightweight CSR)
 *
 * Manually fills src->spike_output, runs N SNN steps, measures:
 *   - Per-pop spike count per step
 *   - Per-pop V min/max/mean
 *   - I_syn per neuron
 *
 * Sweeps weight values to find the sweet spot for propagation
 * without saturation, independent of the full brain init overhead.
 *
 * Build: included in CMakeLists.txt as test_snn_propagation
 * Run:   ./build/test/diagnostic/test_snn_propagation [weight] [n_steps]
 *
 * Default: weight=0.5, n_steps=20
 */

#define _GNU_SOURCE
#include <pthread.h>
#include "nimcp.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Population sizes — small enough to iterate fast but large enough
 * to exhibit population-level dynamics. */
#define N_SRC   100
#define N_MID   500
#define N_DST   500

/* Connectivity (matches our reduced setup: 0.005 = 0.5%) */
#define CONN_DENSITY 0.005f

static void dump_pop(const char* label, snn_network_t* net, uint32_t pop_idx) {
    if (pop_idx >= net->n_populations) {
        printf("  %s: invalid pop_idx %u\n", label, pop_idx);
        return;
    }
    snn_population_t* pop = net->populations[pop_idx];
    if (!pop || !pop->spike_output || !pop->membrane_v) {
        printf("  %s: NULL pop or tensors\n", label);
        return;
    }
    const float* sp = (const float*)nimcp_tensor_data_const(pop->spike_output);
    const float* v  = (const float*)nimcp_tensor_data_const(pop->membrane_v);
    if (!sp || !v) { printf("  %s: tensor data NULL\n", label); return; }

    uint32_t n_spk = 0;
    float v_min = 1e9f, v_max = -1e9f, v_sum = 0;
    for (uint32_t i = 0; i < pop->n_neurons; i++) {
        if (sp[i] > 0.5f) n_spk++;
        v_sum += v[i];
        if (v[i] < v_min) v_min = v[i];
        if (v[i] > v_max) v_max = v[i];
    }
    float v_mean = v_sum / (float)pop->n_neurons;
    printf("  %s: n=%u spk=%u/%u V[%.2f..%.2f] μ=%.2f lightweight=%d\n",
           label, pop->n_neurons, n_spk, pop->n_neurons,
           v_min, v_max, v_mean, pop->lightweight ? 1 : 0);
}

int main(int argc, char** argv) {
    float weight = (argc > 1) ? atof(argv[1]) : 0.5f;
    int n_steps  = (argc > 2) ? atoi(argv[2]) : 20;

    printf("=== SNN propagation test ===\n");
    printf("  src=%d → mid=%d → dst=%d   conn=%.3f%%   weight=%.3f   steps=%d\n",
           N_SRC, N_MID, N_DST, CONN_DENSITY * 100, weight, n_steps);
    printf("  (LIF: v_thresh=-50  v_reset=-65  v_rest=-70  tau_mem=20ms  dt=0.1ms)\n\n");

    nimcp_init();

    snn_config_t config;
    snn_config_default(&config);
    config.n_inputs = N_SRC;
    config.n_outputs = N_DST;
    config.n_hidden = 0;  /* lightweight CSR for mid+dst */

    snn_network_t* net = snn_network_create(&config);
    if (!net) { fprintf(stderr, "snn_network_create failed\n"); return 1; }

    /* Add 3 populations: src (legacy/input), mid (lightweight), dst (lightweight) */
    int src_id = snn_network_add_population(net, N_SRC, NEURON_GENERIC_LIF, "src");
    int mid_id = snn_network_add_population_lightweight(net, N_MID, NEURON_GENERIC_LIF, "mid");
    int dst_id = snn_network_add_population_lightweight(net, N_DST, NEURON_GENERIC_LIF, "dst");
    if (src_id < 0 || mid_id < 0 || dst_id < 0) {
        fprintf(stderr, "add_population failed\n"); return 1;
    }
    printf("  Created populations: src=%d mid=%d dst=%d\n", src_id, mid_id, dst_id);

    /* Wire src → mid → dst */
    int n_sm = snn_network_connect_populations(net, src_id, mid_id,
        SNN_TOPO_RANDOM, CONN_DENSITY, SYNAPSE_AMPA, weight, weight * 0.3f);
    int n_md = snn_network_connect_populations(net, mid_id, dst_id,
        SNN_TOPO_RANDOM, CONN_DENSITY, SYNAPSE_AMPA, weight, weight * 0.3f);
    snn_network_finalize_connections(net);
    printf("  Connections: src→mid=%d  mid→dst=%d\n\n", n_sm, n_md);

    /* Manually drive src spikes: ALL src neurons spike every step */
    float* src_spikes = (float*)nimcp_tensor_data(net->populations[src_id]->spike_output);
    for (uint32_t i = 0; i < N_SRC; i++) src_spikes[i] = 1.0f;

    printf("Initial state (before any step):\n");
    dump_pop("src", net, src_id);
    dump_pop("mid", net, mid_id);
    dump_pop("dst", net, dst_id);
    printf("\n");

    /* Step the network n_steps times. Re-set src spikes each step to
     * simulate continuous input drive. */
    for (int step = 0; step < n_steps; step++) {
        for (uint32_t i = 0; i < N_SRC; i++) src_spikes[i] = 1.0f;
        snn_network_step(net, 0.1f);

        if (step < 5 || step == n_steps - 1) {
            printf("After step %d:\n", step + 1);
            dump_pop("src", net, src_id);
            dump_pop("mid", net, mid_id);
            dump_pop("dst", net, dst_id);
            printf("\n");
        }
    }

    snn_network_destroy(net);
    nimcp_shutdown();
    return 0;
}
