/**
 * @file test_snn_lang_generate_and_decode.c
 * @brief Prove the produce-path bridge call: connect_network + generate_and_decode.
 *
 * The produce path (gl_produce_candidates_via_snn) maps content_intent → concept
 * ids, then calls snn_language_bridge_generate_and_decode, which uses the network
 * + Wernicke/Broca pop ids stored by connect_network to seed → step → decode in
 * one call. This test validates that bridge call directly: wire a targeted
 * projection, teach cat/dog, connect_network, then generate_and_decode(cat) →
 * "cat", generate_and_decode(dog) → "dog". Also checks the not-connected guard.
 *
 * Forces CPU (deterministic; GPU cross-generation membrane issue tracked
 * separately).
 */

#include "nimcp.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_training.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); g_failures++; } } while (0)

#define NN 1024u

static float act_of(const snn_lang_word_result_t* res, uint32_t n, const char* w) {
    for (uint32_t i = 0; i < n; i++)
        if (res[i].word_form && strcmp(res[i].word_form, w) == 0) return res[i].activation;
    return 0.0f;
}

int main(void) {
    fprintf(stderr, "=== test_snn_lang_generate_and_decode ===\n");
    if (!getenv("NIMCP_TEST_ALLOW_GPU")) setenv("CUDA_VISIBLE_DEVICES", "", 1);
    nimcp_init();
    snn_tune_set_noise_rate_hz(0.0f);

    snn_config_t config; snn_config_default(&config);
    config.n_inputs = 16; config.n_outputs = 16; config.n_hidden = 0;
    snn_network_t* net = snn_network_create(&config);
    CHECK(net != NULL, "create net"); if (!net) return 1;

    int wid = snn_network_add_population_lightweight(net, NN, NEURON_GENERIC_LIF, "wernicke_substrate");
    int bid = snn_network_add_population_lightweight(net, NN, NEURON_GENERIC_LIF, "broca_substrate");
    CHECK(wid >= 0 && bid >= 0, "add pops");

    snn_lang_config_t lc = snn_lang_config_default();
    lc.enable_snn_spike_routing = true;
    snn_language_bridge_t* br = snn_language_bridge_create(&lc);
    CHECK(br != NULL, "bridge"); if (!br) return 1;
    CHECK(snn_language_bridge_attach_snn_pop(br, bid, NN, SNN_LANG_POP_ROLE_WORD) == 0, "attach broca");

    const uint32_t WP_CAT = 3u, WP_DOG = 7u;
    const uint64_t CID_CAT = 42ULL, CID_DOG = 99ULL;
    snn_language_bridge_register_word(br, WP_CAT, "cat");
    snn_language_bridge_register_word(br, WP_DOG, "dog");

    /* NOT-connected guard: generate_and_decode must fail cleanly before
     * connect_network (so the produce path falls back to the lexicon). */
    snn_lang_word_result_t r0[8]; uint32_t n0 = 123;
    uint64_t c0[1] = { CID_CAT };
    int rc0 = snn_language_bridge_generate_and_decode(br, c0, 1, 8, 100.0f, r0, 8, &n0);
    CHECK(rc0 == -1 && n0 == 0, "generate_and_decode returns -1 before connect_network");

    /* Targeted projection + teach, finalize, train weights. */
    CHECK(snn_language_bridge_wire_concept_word(br, net, wid, bid, CID_CAT, WP_CAT, 0.0f) == 64, "wire cat");
    CHECK(snn_language_bridge_wire_concept_word(br, net, wid, bid, CID_DOG, WP_DOG, 0.0f) == 64, "wire dog");
    snn_network_finalize_connections(net);
    CHECK(snn_language_bridge_learn_pair(br, net, wid, bid, CID_CAT, WP_CAT, 40.0f, 1.0f) > 0, "learn cat");
    CHECK(snn_language_bridge_learn_pair(br, net, wid, bid, CID_DOG, WP_DOG, 40.0f, 1.0f) > 0, "learn dog");

    /* Connect the bridge to the network, then generate via the stored handles. */
    CHECK(snn_language_bridge_connect_network(br, net, wid, bid) == 0, "connect_network");

    snn_lang_word_result_t res[16]; uint32_t nres = 0;
    uint64_t cc[1] = { CID_CAT };
    (void)snn_network_reset(net);
    int rc = snn_language_bridge_generate_and_decode(br, cc, 1, 60, 100.0f, res, 16, &nres);
    float catA = act_of(res, nres, "cat"), dogA = act_of(res, nres, "dog");
    fprintf(stderr, "  gen_and_decode(cat): rc=%d nres=%u cat=%.1f dog=%.1f\n", rc, nres, catA, dogA);
    CHECK(rc == 0, "gen_and_decode(cat) rc=0");
    CHECK(catA > 0.0f && catA > dogA, "seed cat → 'cat' wins via generate_and_decode");

    uint64_t cd[1] = { CID_DOG };
    (void)snn_network_reset(net);
    rc = snn_language_bridge_generate_and_decode(br, cd, 1, 60, 100.0f, res, 16, &nres);
    float catB = act_of(res, nres, "cat"), dogB = act_of(res, nres, "dog");
    fprintf(stderr, "  gen_and_decode(dog): rc=%d nres=%u cat=%.1f dog=%.1f\n", rc, nres, catB, dogB);
    CHECK(rc == 0, "gen_and_decode(dog) rc=0");
    CHECK(dogB > 0.0f && dogB > catB, "seed dog → 'dog' wins via generate_and_decode");

    snn_language_bridge_destroy(br);
    if (g_failures == 0) { printf("test_snn_lang_generate_and_decode: ALL PASS\n"); return 0; }
    fprintf(stderr, "test_snn_lang_generate_and_decode: %d FAILURE(S)\n", g_failures);
    return 1;
}
