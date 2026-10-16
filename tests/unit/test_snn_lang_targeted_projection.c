/**
 * @file test_snn_lang_targeted_projection.c
 * @brief Prove the TARGETED concept→word projection (the scalable topology).
 *
 * test_snn_lang_learn_discriminate proved learn+generate+discriminate using a
 * FULL (density 1.0) Wernicke→Broca projection so every synapse existed for
 * learn_pair to set — fine for the mechanism, but 64K² synapses at production
 * scale. This test proves the SCALABLE alternative: snn_language_bridge_wire_
 * concept_word adds ONLY each registered pair's 8×8 ensemble cross-product
 * (pre-finalize), so two pairs cost 128 synapses, not a million — yet generation
 * still discriminates cat→"cat", dog→"dog".
 *
 * This is the production topology: at vocab scale you wire ~vocab×64 synapses
 * with EXACT coverage, instead of a 0.15% random sweep that connects a concept
 * ensemble to its word ensemble with ~0.1 expected synapses (the reason
 * warmstart/learn_pair were no-ops on the random projection).
 *
 * Forces CPU (deterministic; the GPU cross-generation membrane issue is tracked
 * separately and is not what this topology test validates).
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

static void seed_and_decode(snn_language_bridge_t* br, snn_network_t* net,
                            int wid, int bid, uint64_t concept_id,
                            float* cat_act, float* dog_act) {
    (void)snn_network_reset(net);
    uint64_t cids[1] = { concept_id };
    (void)snn_language_bridge_generate_step(br, net, wid, bid, cids, 1, 60, 100.0f);
    snn_lang_word_result_t res[16]; uint32_t nres = 0; float intent[8] = {0};
    (void)snn_language_bridge_decode_spikes_cached(br, intent, 8, res, 16, &nres);
    fprintf(stderr, "    [decode cid=%llu] nres=%u:", (unsigned long long)concept_id, nres);
    for (uint32_t i = 0; i < nres; i++)
        fprintf(stderr, " '%s'=%.1f", res[i].word_form ? res[i].word_form : "?", res[i].activation);
    fprintf(stderr, "\n");
    *cat_act = act_of(res, nres, "cat");
    *dog_act = act_of(res, nres, "dog");
}

int main(void) {
    fprintf(stderr, "=== test_snn_lang_targeted_projection ===\n");
    if (!getenv("NIMCP_TEST_ALLOW_GPU")) setenv("CUDA_VISIBLE_DEVICES", "", 1);
    nimcp_init();
    snn_tune_set_noise_rate_hz(0.0f);

    snn_config_t config;
    snn_config_default(&config);
    config.n_inputs = 16; config.n_outputs = 16; config.n_hidden = 0;
    snn_network_t* net = snn_network_create(&config);
    CHECK(net != NULL, "snn_network_create"); if (!net) return 1;

    int wid = snn_network_add_population_lightweight(net, NN, NEURON_GENERIC_LIF, "wernicke_substrate");
    int bid = snn_network_add_population_lightweight(net, NN, NEURON_GENERIC_LIF, "broca_substrate");
    CHECK(wid >= 0 && bid >= 0, "add pops");

    snn_lang_config_t lc = snn_lang_config_default();
    lc.enable_snn_spike_routing = true;
    snn_language_bridge_t* br = snn_language_bridge_create(&lc);
    CHECK(br != NULL, "bridge create"); if (!br) return 1;
    CHECK(snn_language_bridge_attach_snn_pop(br, bid, NN, SNN_LANG_POP_ROLE_WORD) == 0, "attach broca");

    const uint32_t WP_CAT = 3u, WP_DOG = 7u;
    const uint64_t CID_CAT = 42ULL, CID_DOG = 99ULL;
    snn_language_bridge_register_word(br, WP_CAT, "cat");
    snn_language_bridge_register_word(br, WP_DOG, "dog");

    /* TARGETED wiring (pre-finalize): only the two pairs' ensembles. */
    int wc = snn_language_bridge_wire_concept_word(br, net, wid, bid, CID_CAT, WP_CAT, 0.0f);
    int wd = snn_language_bridge_wire_concept_word(br, net, wid, bid, CID_DOG, WP_DOG, 0.0f);
    fprintf(stderr, "  wire_concept_word added: cat=%d dog=%d (want 64 each)\n", wc, wd);
    CHECK(wc == 64, "cat pair wired 8x8=64 synapses");
    CHECK(wd == 64, "dog pair wired 8x8=64 synapses");

    snn_network_finalize_connections(net);

    /* Confirm the projection is TINY — only the targeted synapses exist. */
    snn_population_t* bpop = snn_network_get_population(net, (uint32_t)bid);
    uint32_t total_syn = (bpop && bpop->incoming_csr) ? bpop->incoming_csr->n_synapses : 0;
    fprintf(stderr, "  total Broca incoming synapses = %u (targeted; full would be %u)\n",
            total_syn, NN * NN);
    CHECK(total_syn == 128u, "only 128 targeted synapses (not the 1M full projection)");

    /* Train the weights supra-threshold, then generate + discriminate. */
    CHECK(snn_language_bridge_learn_pair(br, net, wid, bid, CID_CAT, WP_CAT, 40.0f, 1.0f) > 0, "learn cat");
    CHECK(snn_language_bridge_learn_pair(br, net, wid, bid, CID_DOG, WP_DOG, 40.0f, 1.0f) > 0, "learn dog");

    float cc=0, cd=0, dc=0, dd=0;
    seed_and_decode(br, net, wid, bid, CID_CAT, &cc, &cd);
    fprintf(stderr, "  seed=cat cat=%.1f dog=%.1f\n", cc, cd);
    CHECK(cc > 0.0f && cc > cd, "seed cat → 'cat' wins (targeted projection generates)");

    seed_and_decode(br, net, wid, bid, CID_DOG, &dc, &dd);
    fprintf(stderr, "  seed=dog cat=%.1f dog=%.1f\n", dc, dd);
    CHECK(dd > 0.0f && dd > dc, "seed dog → 'dog' wins (discrimination on targeted projection)");

    snn_language_bridge_destroy(br);
    if (g_failures == 0) { printf("test_snn_lang_targeted_projection: ALL PASS\n"); return 0; }
    fprintf(stderr, "test_snn_lang_targeted_projection: %d FAILURE(S)\n", g_failures);
    return 1;
}
