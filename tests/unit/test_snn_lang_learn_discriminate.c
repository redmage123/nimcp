/**
 * @file test_snn_lang_learn_discriminate.c
 * @brief MILESTONE — prove the SNN can LEARN and GENERATE-discriminate language.
 *
 * This is the go/no-go for the whole "SNN is the language generator, and it
 * learns language" directive. The existing test_snn_generate_step only proves
 * the inject→step→decode PLUMBING with ONE word and a full dense projection, so
 * any concept decodes to that one word — it never tests discrimination.
 *
 * Here we wire TWO concepts and TWO words on a tiny Wernicke+Broca net, teach
 * each concept↔word pair with the new training primitive
 * snn_language_bridge_learn_pair (the per-pair unit of "training the SNN on
 * language"), then seed each concept through generate_step and assert Broca
 * decodes back to that concept's OWN word — cat→"cat", dog→"dog". And that
 * BEFORE teaching (projection silent) seeding produces nothing. So it proves the
 * three things the directive rests on:
 *   1. CARRY      — the projection holds a concept→word association in synapses.
 *   2. LEARN      — learn_pair writes that association (untaught → no output).
 *   3. DISCRIMINATE — distinct concepts decode to distinct, correct words.
 *
 * Topology note: the test uses a FULL (density 1.0) Wernicke→Broca projection
 * at weight 0 so every concept→word synapse EXISTS for learn_pair to set. At
 * production scale full is infeasible (64K²); the scalable fix is a TARGETED
 * projection that wires only the registered pairs' ensembles. This test isolates
 * the learn+discriminate MECHANISM from that scaling concern.
 *
 * Compile (CMake wires this into lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_snn_lang_learn_discriminate.c \
 *       -L build/lib -lnimcp -lm -lpthread -Wl,-rpath,build/lib -o /tmp/t
 */

#include "nimcp.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_training.h"   /* snn_tune_set_noise_rate_hz */
#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); g_failures++; } } while (0)

/* Pop size: 8-neuron ensembles must fit DISJOINTLY or concept/word identities
 * alias (validated: 64 neurons → cat/dog ensembles collide, discrimination
 * scrambles). 1024 gives 128 disjoint ensemble slots — the 4 ensembles here are
 * disjoint (asserted below). Production pops are 64000 (8000 slots). */
#define NN 1024u
#define E  8u   /* SNN_LANG_NEURONS_PER_POP */

/* Mirror of lang_ensemble_neuron (static in the bridge TU) for the disjointness
 * diagnostic — must match the bridge's hash exactly. */
static uint32_t ens(uint32_t pop_idx, uint32_t j, uint32_t n) {
    if (n == 0) return 0;
    uint64_t z = (uint64_t)pop_idx * 0x9E3779B97F4A7C15ULL + 0xD1B54A32D192ED03ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z =  z ^ (z >> 31);
    uint32_t base   = (uint32_t)(z % n);
    uint32_t stride = (uint32_t)((z >> 17) % n) | 1u;
    return (uint32_t)(((uint64_t)base + (uint64_t)j * stride) % n);
}

/* #shared neurons between two ensembles (pop indices a,b) over an n-neuron pop. */
static int ens_overlap(uint32_t a, uint32_t b, uint32_t n) {
    uint32_t ea[E], eb[E]; int shared = 0;
    for (uint32_t j = 0; j < E; j++) { ea[j] = ens(a, j, n); eb[j] = ens(b, j, n); }
    for (uint32_t i = 0; i < E; i++)
        for (uint32_t k = 0; k < E; k++)
            if (ea[i] == eb[k]) shared++;
    return shared;
}

/* Sum the decode activation a given word_form received (0 if not decoded). */
static float act_of(const snn_lang_word_result_t* res, uint32_t n, const char* w) {
    for (uint32_t i = 0; i < n; i++) {
        if (res[i].word_form && strcmp(res[i].word_form, w) == 0) return res[i].activation;
    }
    return 0.0f;
}

/* Seed one concept through the generator, decode, return (cat_act, dog_act). */
static void seed_and_decode(snn_language_bridge_t* br, snn_network_t* net,
                            int wid, int bid, uint64_t concept_id,
                            float* cat_act, float* dog_act) {
    /* Reset membrane/refractory state so each seed starts from rest — otherwise
     * a previously-seeded ensemble bleeds into the next concept's settle. */
    (void)snn_network_reset(net);
    uint64_t cids[1] = { concept_id };
    /* High seed current so the Wernicke ensemble fires EVERY step (fast charge),
     * delivering sustained drive to the word ensemble — a single slow volley at
     * AMPA weight 2.0 leaks before Broca integrates to threshold. inject is the
     * concept-activation strength (from content_intent), not a synaptic weight,
     * so it's free to be large. */
    (void)snn_language_bridge_generate_step(br, net, wid, bid, cids, 1,
                                            60 /*steps*/, 100.0f /*inject*/);
    float intent[8] = {0};
    snn_lang_word_result_t res[16];
    uint32_t nres = 0;
    (void)snn_language_bridge_decode_spikes_cached(br, intent, 8, res, 16, &nres);
    fprintf(stderr, "    [decode cid=%llu] nres=%u:", (unsigned long long)concept_id, nres);
    for (uint32_t i = 0; i < nres; i++)
        fprintf(stderr, " '%s'@wp%u=%.1f", res[i].word_form ? res[i].word_form : "?",
                res[i].word_pop, res[i].activation);
    fprintf(stderr, "\n");
    *cat_act = act_of(res, nres, "cat");
    *dog_act = act_of(res, nres, "dog");
}

int main(void) {
    fprintf(stderr, "=== test_snn_lang_learn_discriminate ===\n");
    /* Force CPU: this test proves the language MECHANISM (learn→generate→
     * discriminate), which must be deterministic. The CPU integrator reads CSR
     * host weights directly, so learn_pair's changes are always visible. The GPU
     * fast path caches weights in a persistent device buffer that learn_pair's
     * post-finalize edits don't reliably reach (sync gap) — a real PRODUCTION
     * issue tracked separately, not what this mechanism test validates. Set
     * before nimcp_init so the auto GPU context sees no device. */
    if (!getenv("NIMCP_TEST_ALLOW_GPU")) setenv("CUDA_VISIBLE_DEVICES", "", 1);
    nimcp_init();
    snn_tune_set_noise_rate_hz(0.0f);  /* determinism — only the projection drives Broca */

    /* Tiny SNN: Wernicke (concept) + Broca (word). */
    snn_config_t config;
    snn_config_default(&config);
    config.n_inputs = 16; config.n_outputs = 16; config.n_hidden = 0;
    snn_network_t* net = snn_network_create(&config);
    CHECK(net != NULL, "snn_network_create");
    if (!net) return 1;

    int wid = snn_network_add_population_lightweight(net, NN, NEURON_GENERIC_LIF, "wernicke_substrate");
    int bid = snn_network_add_population_lightweight(net, NN, NEURON_GENERIC_LIF, "broca_substrate");
    CHECK(wid >= 0 && bid >= 0, "add wernicke+broca pops");

    /* FULL Wernicke→Broca projection at weight 0: all synapses exist (so
     * learn_pair has targets) but silent until trained. */
    snn_network_connect_populations(net, (uint32_t)wid, (uint32_t)bid,
        SNN_TOPO_RANDOM, 1.0f /*full*/, SYNAPSE_AMPA, 0.0f /*w_mean*/, 0.0f /*w_std*/);
    snn_network_finalize_connections(net);

    /* Bridge: register two words, attach Broca. */
    snn_lang_config_t lc = snn_lang_config_default();
    lc.enable_snn_spike_routing = true;
    snn_language_bridge_t* br = snn_language_bridge_create(&lc);
    CHECK(br != NULL, "bridge create"); if (!br) return 1;
    CHECK(snn_language_bridge_attach_snn_pop(br, bid, NN, SNN_LANG_POP_ROLE_WORD) == 0, "attach broca");

    const uint32_t WP_CAT = 3u, WP_DOG = 7u;
    const uint64_t CID_CAT = 42ULL, CID_DOG = 99ULL;   /* distinct concept_pops */
    snn_language_bridge_register_word(br, WP_CAT, "cat");
    snn_language_bridge_register_word(br, WP_DOG, "dog");

    /* Ensembles must be disjoint or identities alias. Concept ensembles live in
     * Wernicke (CID%SNN_LANG_MAX_CONCEPT_POPS), word ensembles in Broca. */
    uint32_t cpop_cat = (uint32_t)(CID_CAT % SNN_LANG_MAX_CONCEPT_POPS);
    uint32_t cpop_dog = (uint32_t)(CID_DOG % SNN_LANG_MAX_CONCEPT_POPS);
    int ov_concept = ens_overlap(cpop_cat, cpop_dog, NN);
    int ov_word    = ens_overlap(WP_CAT,   WP_DOG,   NN);
    fprintf(stderr, "  ensemble overlap: concepts=%d words=%d (want 0/0)\n",
            ov_concept, ov_word);
    fprintf(stderr, "  ens(cat-concept %u):", cpop_cat);
    for (uint32_t j=0;j<E;j++) fprintf(stderr," %u",ens(cpop_cat,j,NN));
    fprintf(stderr, "\n  ens(dog-concept %u):", cpop_dog);
    for (uint32_t j=0;j<E;j++) fprintf(stderr," %u",ens(cpop_dog,j,NN));
    fprintf(stderr, "\n  ens(cat-word %u):", WP_CAT);
    for (uint32_t j=0;j<E;j++) fprintf(stderr," %u",ens(WP_CAT,j,NN));
    fprintf(stderr, "\n  ens(dog-word %u):", WP_DOG);
    for (uint32_t j=0;j<E;j++) fprintf(stderr," %u",ens(WP_DOG,j,NN));
    fprintf(stderr, "\n");
    CHECK(ov_concept == 0, "cat/dog CONCEPT ensembles disjoint in Wernicke");
    CHECK(ov_word    == 0, "cat/dog WORD ensembles disjoint in Broca");

    /* (1) BEFORE teaching: projection is silent → seeding produces no output. */
    float c0 = 0.0f, d0 = 0.0f;
    seed_and_decode(br, net, wid, bid, CID_CAT, &c0, &d0);
    fprintf(stderr, "  pre-teach  seed=cat  cat_act=%.1f dog_act=%.1f\n", c0, d0);
    CHECK(c0 == 0.0f && d0 == 0.0f, "untaught projection produces no Broca output");

    /* (2) TEACH: cat-concept→"cat", dog-concept→"dog" (max AMPA weight). */
    int uc = snn_language_bridge_learn_pair(br, net, wid, bid, CID_CAT, WP_CAT, 40.0f, 1.0f);
    int ud = snn_language_bridge_learn_pair(br, net, wid, bid, CID_DOG, WP_DOG, 40.0f, 1.0f);
    fprintf(stderr, "  learn_pair updated: cat=%d dog=%d synapses\n", uc, ud);
    CHECK(uc > 0, "learn_pair(cat) set >0 synapses (targets exist)");
    CHECK(ud > 0, "learn_pair(dog) set >0 synapses (targets exist)");

    /* DEBUG: read back the weights learn_pair wrote. For the first cat-word
     * neuron, count incoming synapses whose source is the cat-concept ensemble
     * and print their weights — this tells us if the projection is actually set
     * supra-threshold where the CPU integrator reads it. */
    {
        snn_population_t* bpop = snn_network_get_population(net, (uint32_t)bid);
        snn_csr_storage_t* dcsr = bpop ? bpop->incoming_csr : NULL;
        uint32_t cens42[E];
        for (uint32_t j=0;j<E;j++) cens42[j]=ens(cpop_cat,j,NN);
        if (dcsr && dcsr->finalized) {
            uint32_t dst0 = ens(WP_CAT, 0, NN);
            uint32_t cnt=0;
            snn_csr_synapse_t* inc = snn_csr_get_incoming(dcsr, dst0, &cnt);
            int from_cat=0; float wsum=0;
            for (uint32_t e=0;e<cnt;e++) {
                if (inc[e].src_pop != (uint32_t)wid) continue;
                for (uint32_t j=0;j<E;j++) if (inc[e].src_neuron==cens42[j]) {
                    from_cat++; wsum+=inc[e].weight; break;
                }
            }
            fprintf(stderr, "  DEBUG cat-word neuron %u: %u incoming, %d from cat-concept, weightsum=%.1f (want 8 @ w_target=40 = 320, supra-threshold)\n",
                    dst0, cnt, from_cat, wsum);
        }
    }

    /* (3) DISCRIMINATE: seed cat → "cat" wins; seed dog → "dog" wins. */
    float cc = 0.0f, cd = 0.0f;
    seed_and_decode(br, net, wid, bid, CID_CAT, &cc, &cd);
    fprintf(stderr, "  seed=cat   cat_act=%.1f dog_act=%.1f\n", cc, cd);
    CHECK(cc > 0.0f, "seed cat → 'cat' Broca ensemble fired (SNN generated)");
    CHECK(cc > cd,   "seed cat → 'cat' outscores 'dog' (discrimination)");

    float dc = 0.0f, dd = 0.0f;
    seed_and_decode(br, net, wid, bid, CID_DOG, &dc, &dd);
    fprintf(stderr, "  seed=dog   cat_act=%.1f dog_act=%.1f\n", dc, dd);
    CHECK(dd > 0.0f, "seed dog → 'dog' Broca ensemble fired (SNN generated)");
    CHECK(dd > dc,   "seed dog → 'dog' outscores 'cat' (discrimination)");

    snn_language_bridge_destroy(br);

    if (g_failures == 0) { printf("test_snn_lang_learn_discriminate: ALL PASS\n"); return 0; }
    fprintf(stderr, "test_snn_lang_learn_discriminate: %d FAILURE(S)\n", g_failures);
    return 1;
}
