/**
 * @file test_lang_snn_decode.c
 * @brief Increment-1 SNN-as-generator readout: decode_spikes_cached + the
 *        produce_via_snn gl flag.
 *
 * Covers:
 *   1. snn_language_bridge_decode_spikes (the STUB) still returns 0 — it must
 *      stay stubbed so the on-training-path bigram-learning callers are
 *      unchanged.
 *   2. snn_language_bridge_decode_spikes_cached ranks words by summed Broca
 *      spike activity over each word's deterministic neuron ensemble: firing a
 *      target word's ensemble makes that word rank #1.
 *   3. decode_spikes_cached returns 0 results with no cached spikes (so produce
 *      falls back to the lexicon path).
 *   4. grounded_language_{set,get}_produce_via_snn: default OFF, round-trips,
 *      NULL-safe.
 *
 * The ensemble hash is re-derived locally (it is static in the bridge TU) so
 * the test can craft a spike_output that lights up exactly one word's ensemble.
 * It MUST match lang_ensemble_neuron in nimcp_snn_language_bridge.c.
 *
 * Compile (CMake wires this into lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_snn_decode.c \
 *       -L build/lib -lnimcp -lm -lpthread -Wl,-rpath,build/lib -o /tmp/test_lang_snn
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "language/nimcp_grounded_language.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; \
    } \
} while (0)

/* MUST match lang_ensemble_neuron() in nimcp_snn_language_bridge.c. */
static uint32_t ens(uint32_t pop_idx, uint32_t j, uint32_t n_neurons) {
    if (n_neurons == 0) return 0;
    /* Disjoint contiguous blocks (2026-06-09 collision fix, 53866c7c5). */
    return (uint32_t)(((uint64_t)pop_idx * SNN_LANG_NEURONS_PER_POP + j) % n_neurons);
}

#define NN 256u

static void test_decode_cached_ranks_fired_word(void) {
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.enable_snn_spike_routing = true;   /* so drain caches the spikes */
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "create"); if (!b) return;

    EXPECT(snn_language_bridge_attach_snn_pop(b, 0, NN, SNN_LANG_POP_ROLE_WORD) == 0,
           "attach word pop");
    snn_language_bridge_register_word(b, 0, "cat");
    snn_language_bridge_register_word(b, 1, "dog");
    snn_language_bridge_register_word(b, 2, "bird");

    /* Fire only word 1 ("dog")'s ensemble. */
    float spikes[NN];
    memset(spikes, 0, sizeof(spikes));
    for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) spikes[ens(1, j, NN)] = 1.0f;

    EXPECT(snn_language_bridge_drain_pop_spikes(b, 0, spikes, NN, 1.0f) == 0, "drain");

    float intent[8] = {0};   /* decode requires non-NULL concept_rates */
    snn_lang_word_result_t res[8];
    uint32_t nres = 0;
    int rc = snn_language_bridge_decode_spikes_cached(b, intent, 8, res, 8, &nres);
    EXPECT(rc == 0, "decode_cached rc=%d", rc);
    EXPECT(nres >= 1, "got %u results", nres);
    if (nres >= 1) {
        EXPECT(res[0].word_pop == 1, "top word_pop=%u (want 1=dog)", res[0].word_pop);
        EXPECT(res[0].word_form && strcmp(res[0].word_form, "dog") == 0,
               "top word='%s' (want dog)", res[0].word_form ? res[0].word_form : "(null)");
        EXPECT(res[0].activation > 0.0f, "top activation=%.2f", res[0].activation);
    }

    /* The STUB must still return 0 (default-path callers unchanged). */
    uint32_t stub_n = 99;
    EXPECT(snn_language_bridge_decode_spikes(b, intent, 8, res, 8, &stub_n) == 0, "stub rc");
    EXPECT(stub_n == 0, "stub returns 0 results, got %u", stub_n);

    snn_language_bridge_destroy(b);
}

static void test_decode_cached_no_signal(void) {
    snn_lang_config_t cfg = snn_lang_config_default();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "create2"); if (!b) return;
    snn_language_bridge_register_word(b, 0, "cat");

    float intent[8] = {0};
    snn_lang_word_result_t res[8];
    uint32_t nres = 99;
    /* No drain yet → no cache → 0 results (produce falls back to lexicon). */
    EXPECT(snn_language_bridge_decode_spikes_cached(b, intent, 8, res, 8, &nres) == 0,
           "decode no-cache rc");
    EXPECT(nres == 0, "no cache -> 0 results, got %u", nres);

    snn_language_bridge_destroy(b);
}

static void test_produce_via_snn_flag(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create gl"); if (!gl) return;

    EXPECT(grounded_language_get_produce_via_snn(gl) == false, "default OFF");
    grounded_language_set_produce_via_snn(gl, true);
    EXPECT(grounded_language_get_produce_via_snn(gl) == true, "round-trips ON");
    grounded_language_set_produce_via_snn(gl, false);
    EXPECT(grounded_language_get_produce_via_snn(gl) == false, "round-trips OFF");

    /* NULL-safe. */
    EXPECT(grounded_language_get_produce_via_snn(NULL) == false, "NULL getter -> false");
    grounded_language_set_produce_via_snn(NULL, true);  /* must not crash */

    grounded_language_destroy(gl);
}

static void test_metacog_floor_modulation(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create gl"); if (!gl) return;

    /* Flag default OFF, round-trips. */
    EXPECT(grounded_language_get_metacog_gates_produce(gl) == false, "metacog flag default OFF");
    grounded_language_set_metacog_gates_produce(gl, true);
    EXPECT(grounded_language_get_metacog_gates_produce(gl) == true, "metacog flag ON");

    /* Adjust default 0, round-trips, clamps to [-1,1], NaN-safe. */
    EXPECT(grounded_language_get_metacog_floor_adjust(gl) == 0.0f, "adjust default 0");
    grounded_language_set_metacog_floor_adjust(gl, 0.2f);
    EXPECT(grounded_language_get_metacog_floor_adjust(gl) == 0.2f, "adjust 0.2 round-trips");
    grounded_language_set_metacog_floor_adjust(gl, 5.0f);
    EXPECT(grounded_language_get_metacog_floor_adjust(gl) == 1.0f, "adjust clamps >1 -> 1");
    grounded_language_set_metacog_floor_adjust(gl, -5.0f);
    EXPECT(grounded_language_get_metacog_floor_adjust(gl) == -1.0f, "adjust clamps <-1 -> -1");

    /* NULL-safe. */
    EXPECT(grounded_language_get_metacog_gates_produce(NULL) == false, "NULL flag getter");
    EXPECT(grounded_language_get_metacog_floor_adjust(NULL) == 0.0f, "NULL adjust getter");
    grounded_language_set_metacog_gates_produce(NULL, true);  /* must not crash */
    grounded_language_set_metacog_floor_adjust(NULL, 0.5f);   /* must not crash */

    grounded_language_destroy(gl);
}

static void test_warmstart_collect_bindings(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create gl"); if (!gl) return;

    /* Empty lexicon → 0 bindings collected, no crash. */
    gl_warmstart_binding_t out[8];
    uint32_t n = grounded_language_collect_warmstart_bindings(gl, out, 8);
    EXPECT(n == 0, "empty lexicon -> 0 bindings, got %u", n);

    /* NULL-safe. */
    EXPECT(grounded_language_collect_warmstart_bindings(NULL, out, 8) == 0, "NULL gl -> 0");
    EXPECT(grounded_language_collect_warmstart_bindings(gl, NULL, 8) == 0, "NULL out -> 0");
    EXPECT(grounded_language_collect_warmstart_bindings(gl, out, 0) == 0, "max 0 -> 0");

    grounded_language_destroy(gl);
}

int main(void) {
    test_decode_cached_ranks_fired_word();
    test_decode_cached_no_signal();
    test_produce_via_snn_flag();
    test_metacog_floor_modulation();
    test_warmstart_collect_bindings();
    if (g_failures == 0) {
        printf("test_lang_snn_decode: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_snn_decode: %d FAILURE(S)\n", g_failures);
    return 1;
}
