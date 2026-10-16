/**
 * @file test_lang_phantom_wiring_audit.c
 * @brief Tier 3 phantom-wiring audit — verify arcuate relay pop attach.
 *
 * WHAT: Stand-in unit test for the activation done in
 *       attach_lang_adapters_to_substrate() — the production path attaches
 *       Broca (WORD), Wernicke (CONCEPT), and Arcuate (CONCEPT) to the
 *       SNN-language bridge during init. Prior to this commit, only Broca
 *       and Wernicke were attached; the arcuate pop was created and wired
 *       into the SNN's internal CSR (Wernicke ↔ Arcuate ↔ Broca) but its
 *       spike traffic never reached the bridge / STDP path.
 *
 * WHY:  attach_lang_adapters_to_substrate() is a static helper, and a
 *       full brain init pulls in 80+ subsystems (~14s in FAST mode and ~11
 *       minutes in FULL mode for the 2M-neuron build). Both are too
 *       heavyweight for a unit test harness. Instead we directly exercise
 *       the bridge attach contract that the helper relies on, asserting:
 *
 *         - 3 pops can coexist in the attach table (Broca + Wernicke +
 *           Arcuate; capacity SNN_LANG_MAX_ATTACHED_POPS is 8 — plenty of
 *           room).
 *         - The 2 CONCEPT pops are visible via the iterator with the
 *           correct role, and the 1 WORD pop is visible with the correct
 *           role. (Wernicke + Arcuate are both CONCEPT; the test asserts
 *           the iterator sees ≥2 CONCEPT-role pops, which is the load-
 *           bearing invariant the production wiring is meant to satisfy.)
 *         - Re-running the attach is idempotent (re-attaching a known
 *           pop_id updates the role rather than consuming a fresh slot —
 *           important for the resume path which calls
 *           attach_lang_adapters_to_substrate twice if the load partial-
 *           overlaps).
 *
 * INTEGRATION DOC:
 *       The "real" integration test belongs in the integration tier — it
 *       should run a brain factory init far enough to populate
 *       brain->snn_lang_bridge, then call
 *       nimcp_brain_factory_init_language_pops(brain) and walk the
 *       bridge's attach table via snn_language_bridge_get_attached_pop.
 *       Expected after init: ≥2 attached pops, ≥1 with role CONCEPT
 *       (Wernicke OR Arcuate or both — Arcuate may be skipped if the
 *       snn_network capacity is exhausted, in which case its pop_id will
 *       be -1). See nimcp_brain_init_language_pops.c::attach_lang_adapters_to_substrate
 *       for the contract.
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Match the IDs the production code allocates — these are arbitrary in the
 * test (the bridge stores them opaquely), but using meaningful constants
 * makes the test read like the production attach order. */
#define TEST_BROCA_POP_ID     11
#define TEST_WERNICKE_POP_ID  22
#define TEST_ARCUATE_POP_ID   33

#define TEST_BROCA_N      64000u
#define TEST_WERNICKE_N   64000u
#define TEST_ARCUATE_N    32000u

static void test_arcuate_attach_concept_role(void)
{
    /* Bridge cap chosen large enough that none of the production pop sizes
     * trigger the "n_neurons > cap" warning path — we want this test to
     * focus on attachment, not on the collision-aliasing path. */
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = 1024;
    cfg.max_word_pops    = 1024;
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "create bridge");
    if (!b) return;

    /* Replicate the production attach order in
     * attach_lang_adapters_to_substrate: Broca (WORD), Wernicke (CONCEPT),
     * Arcuate (CONCEPT). */
    EXPECT(snn_language_bridge_attach_snn_pop(b, TEST_BROCA_POP_ID,
                                                TEST_BROCA_N,
                                                SNN_LANG_POP_ROLE_WORD) == 0,
            "attach Broca as WORD");
    EXPECT(snn_language_bridge_attach_snn_pop(b, TEST_WERNICKE_POP_ID,
                                                TEST_WERNICKE_N,
                                                SNN_LANG_POP_ROLE_CONCEPT) == 0,
            "attach Wernicke as CONCEPT");
    EXPECT(snn_language_bridge_attach_snn_pop(b, TEST_ARCUATE_POP_ID,
                                                TEST_ARCUATE_N,
                                                SNN_LANG_POP_ROLE_CONCEPT) == 0,
            "attach Arcuate as CONCEPT (Tier 3 phantom-wiring activation)");

    /* Walk the attach table and count by role. */
    uint32_t n_word = 0;
    uint32_t n_concept = 0;
    bool seen_arcuate = false;
    bool seen_wernicke = false;
    bool seen_broca = false;
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        int pid = -1;
        uint32_t nn = 0;
        snn_lang_pop_role_t r = SNN_LANG_POP_ROLE_CONCEPT;
        if (snn_language_bridge_get_attached_pop(b, i, &pid, &nn, &r) != 0) {
            continue;
        }
        if (pid < 0) continue;
        if (r == SNN_LANG_POP_ROLE_WORD) n_word++;
        if (r == SNN_LANG_POP_ROLE_CONCEPT) n_concept++;
        if (pid == TEST_ARCUATE_POP_ID && r == SNN_LANG_POP_ROLE_CONCEPT) {
            seen_arcuate = true;
        }
        if (pid == TEST_WERNICKE_POP_ID && r == SNN_LANG_POP_ROLE_CONCEPT) {
            seen_wernicke = true;
        }
        if (pid == TEST_BROCA_POP_ID && r == SNN_LANG_POP_ROLE_WORD) {
            seen_broca = true;
        }
    }

    /* Load-bearing invariant: at least 2 CONCEPT pops are attached
     * (Wernicke + Arcuate). Before this commit, only Wernicke was. */
    EXPECT(n_concept >= 2,
            "expected ≥2 CONCEPT pops (Wernicke + Arcuate); got %u",
            n_concept);
    EXPECT(n_word >= 1, "expected ≥1 WORD pop (Broca); got %u", n_word);
    EXPECT(seen_arcuate,
            "Arcuate pop must be attached with role CONCEPT — this is the "
            "Tier 3 activation under test");
    EXPECT(seen_wernicke, "Wernicke must remain attached as CONCEPT");
    EXPECT(seen_broca, "Broca must remain attached as WORD");

    /* Re-attaching the arcuate pop_id with a different role must NOT
     * consume a new slot — the resume path calls
     * attach_lang_adapters_to_substrate again, and the second pass must
     * not double-allocate. */
    EXPECT(snn_language_bridge_attach_snn_pop(b, TEST_ARCUATE_POP_ID,
                                                TEST_ARCUATE_N,
                                                SNN_LANG_POP_ROLE_CONCEPT) == 0,
            "re-attach arcuate with same role (idempotent)");
    uint32_t n_concept_after = 0;
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        int pid = -1;
        uint32_t nn = 0;
        snn_lang_pop_role_t r = SNN_LANG_POP_ROLE_CONCEPT;
        if (snn_language_bridge_get_attached_pop(b, i, &pid, &nn, &r) != 0) {
            continue;
        }
        if (pid < 0) continue;
        if (r == SNN_LANG_POP_ROLE_CONCEPT) n_concept_after++;
    }
    EXPECT(n_concept_after == n_concept,
            "re-attach must not change the CONCEPT count (was %u, now %u)",
            n_concept, n_concept_after);

    snn_language_bridge_destroy(b);
}

int main(void)
{
    fprintf(stderr, "[Tier 3 audit] test_lang_phantom_wiring_audit\n");
    test_arcuate_attach_concept_role();

    if (g_failures == 0) {
        fprintf(stderr, "OK — all tests passed\n");
        return 0;
    } else {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
}
