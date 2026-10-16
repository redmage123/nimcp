/**
 * @file test_snn_csr_reopen.c
 * @brief Prove snn_csr_reopen — the core of runtime synaptogenesis (#26).
 *
 * finalize() compacts entries[] in place from 16-byte COO to 12-byte CSR, so
 * naive post-finalize add_entry would corrupt the array. reopen() re-expands to
 * COO (recovering dst from row_ptr) so more synapses can be appended, then the
 * caller re-finalizes. This test builds a CSR, finalizes, reopens, appends
 * synapses to both EXISTING and NEW destination neurons, re-finalizes, and
 * verifies every old + new synapse is present with the correct weight — i.e. the
 * re-expansion preserved the existing topology and the new ones landed.
 *
 * Pure CSR (no GPU, no network) — deterministic.
 */

#include "snn/nimcp_snn_synapse.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); g_failures++; } } while (0)

/* weight of the synapse into `dst` from (src_pop,src_neuron), or -1 if absent. */
static float weight_of(snn_csr_storage_t* csr, uint32_t dst,
                       uint32_t src_pop, uint32_t src_neuron) {
    uint32_t cnt = 0;
    snn_csr_synapse_t* inc = snn_csr_get_incoming(csr, dst, &cnt);
    for (uint32_t i = 0; i < cnt; i++)
        if (inc[i].src_pop == src_pop && inc[i].src_neuron == src_neuron)
            return inc[i].weight;
    return -1.0f;
}

static uint32_t incoming_count(snn_csr_storage_t* csr, uint32_t dst) {
    uint32_t cnt = 0;
    (void)snn_csr_get_incoming(csr, dst, &cnt);
    return cnt;
}

int main(void) {
    fprintf(stderr, "=== test_snn_csr_reopen ===\n");

    snn_csr_storage_t* csr = snn_csr_create(10 /*n_neurons*/, 4 /*est*/);
    CHECK(csr != NULL, "create"); if (!csr) return 1;

    /* Round 1: 3 synapses, two into neuron 2, one into neuron 5. */
    CHECK(snn_csr_add_entry(csr, 2, 0, 5, 1.0f) == 0, "add (2<-5)");
    CHECK(snn_csr_add_entry(csr, 2, 0, 7, 2.0f) == 0, "add (2<-7)");
    CHECK(snn_csr_add_entry(csr, 5, 0, 3, 3.0f) == 0, "add (5<-3)");
    CHECK(snn_csr_finalize(csr) == 0, "finalize #1");

    CHECK(incoming_count(csr, 2) == 2, "neuron 2 has 2 incoming after finalize #1");
    CHECK(weight_of(csr, 2, 0, 5) == 1.0f, "(2<-5)=1.0");
    CHECK(weight_of(csr, 2, 0, 7) == 2.0f, "(2<-7)=2.0");
    CHECK(weight_of(csr, 5, 0, 3) == 3.0f, "(5<-3)=3.0");

    /* Re-open: must succeed and put the CSR back in build mode. */
    CHECK(snn_csr_reopen(csr) == 0, "reopen");

    /* Round 2: append to an EXISTING dst (2) and a NEW dst (8). */
    CHECK(snn_csr_add_entry(csr, 2, 0, 9, 4.0f) == 0, "add (2<-9) post-reopen");
    CHECK(snn_csr_add_entry(csr, 8, 0, 1, 5.0f) == 0, "add (8<-1) post-reopen");
    CHECK(snn_csr_finalize(csr) == 0, "finalize #2");

    /* All old synapses survived the re-expansion... */
    CHECK(weight_of(csr, 2, 0, 5) == 1.0f, "(2<-5) survived reopen");
    CHECK(weight_of(csr, 2, 0, 7) == 2.0f, "(2<-7) survived reopen");
    CHECK(weight_of(csr, 5, 0, 3) == 3.0f, "(5<-3) survived reopen");
    /* ...and the new ones landed on the right neurons. */
    CHECK(weight_of(csr, 2, 0, 9) == 4.0f, "(2<-9) added");
    CHECK(weight_of(csr, 8, 0, 1) == 5.0f, "(8<-1) added");
    /* Counts: neuron 2 now has 3, neuron 8 has 1, neuron 5 still 1. */
    CHECK(incoming_count(csr, 2) == 3, "neuron 2 has 3 incoming after reopen+grow");
    CHECK(incoming_count(csr, 8) == 1, "neuron 8 has 1 incoming (new)");
    CHECK(incoming_count(csr, 5) == 1, "neuron 5 still has 1 incoming");

    /* reopen on an empty/fresh CSR path: idempotent reopen-then-reopen. */
    CHECK(snn_csr_reopen(csr) == 0, "reopen #2");
    CHECK(snn_csr_reopen(csr) == 0, "reopen when already open is a no-op (0)");
    CHECK(snn_csr_finalize(csr) == 0, "finalize #3 (no new entries)");
    CHECK(incoming_count(csr, 2) == 3, "topology intact after reopen/refinalize cycle");

    snn_csr_destroy(csr);

    if (g_failures == 0) { printf("test_snn_csr_reopen: ALL PASS\n"); return 0; }
    fprintf(stderr, "test_snn_csr_reopen: %d FAILURE(S)\n", g_failures);
    return 1;
}
