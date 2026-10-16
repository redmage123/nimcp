/**
 * @file test_concept_registry_binding.c
 * @brief Slice B regression — Cross-modal population binding registry.
 *
 * Coverage:
 *   1. Intern "leaves" twice → same pop_id (text idempotence).
 *   2. Intern visual features → pop_b. Bind(text_id, pop_b). After:
 *      canonical(text_id) == canonical(pop_b).
 *   3. Intern audio features → pop_c. Bind(text_id, pop_c). All three
 *      canonicalize to the same root.
 *   4. After 1000 random interns + 100 random binds, every observed id
 *      resolves to a valid root in the [1, total_referents] range.
 *   5. Save + load round-trip preserves canonical identity.
 *
 * Compile (matches the other standalone-C tests in tests/unit/):
 *   gcc -I include tests/unit/test_concept_registry_binding.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,$(pwd)/build/lib \
 *       -o /tmp/test_concept_registry_binding
 *
 * Run:
 *   /tmp/test_concept_registry_binding
 *
 * Exit code: 0 on PASS, non-zero on FAIL (ctest reads exit code).
 */

#include "language/nimcp_concept_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) do {                                  \
    if (cond) {                                                      \
        printf("  [PASS] %s\n", msg);                                \
        g_passed++;                                                  \
    } else {                                                         \
        printf("  [FAIL] %s\n", msg);                                \
        g_failed++;                                                  \
    }                                                                \
} while (0)

/*=============================================================================
 * Test 1: Text interning idempotence
 *===========================================================================*/

static void test_text_idempotence(void)
{
    printf("\n[1] test_text_idempotence\n");

    concept_registry_t* reg = concept_registry_create(0);
    TEST_ASSERT(reg != NULL, "registry created");
    if (!reg) return;

    concept_pop_id_t a = concept_registry_intern_text(reg, "leaves");
    concept_pop_id_t b = concept_registry_intern_text(reg, "leaves");
    TEST_ASSERT(a != CONCEPT_POP_ID_INVALID, "first intern returns a valid id");
    TEST_ASSERT(a == b, "second intern of same text returns same id");

    /* Trim + lowercase normalization */
    concept_pop_id_t c = concept_registry_intern_text(reg, "  LEAVES ");
    TEST_ASSERT(c == a, "intern normalizes whitespace + case");

    /* Different text → different id */
    concept_pop_id_t d = concept_registry_intern_text(reg, "ball");
    TEST_ASSERT(d != CONCEPT_POP_ID_INVALID, "second word interned");
    TEST_ASSERT(d != a, "different text → different id");

    /* Empty / NULL → invalid */
    concept_pop_id_t e = concept_registry_intern_text(reg, "");
    concept_pop_id_t f = concept_registry_intern_text(reg, NULL);
    TEST_ASSERT(e == CONCEPT_POP_ID_INVALID, "empty text → INVALID");
    TEST_ASSERT(f == CONCEPT_POP_ID_INVALID, "NULL text → INVALID");

    concept_registry_destroy(reg);
}

/*=============================================================================
 * Test 2: Visual binding coalesces with text
 *===========================================================================*/

static void test_visual_text_binding(void)
{
    printf("\n[2] test_visual_text_binding\n");

    concept_registry_t* reg = concept_registry_create(0);
    if (!reg) return;

    concept_pop_id_t text_id = concept_registry_intern_text(reg, "leaves");
    TEST_ASSERT(text_id != CONCEPT_POP_ID_INVALID, "text interned");

    float visual_features[16];
    for (int i = 0; i < 16; i++) visual_features[i] = (float)i * 0.1f;
    concept_pop_id_t visual_id =
        concept_registry_intern_visual(reg, visual_features, 16);
    TEST_ASSERT(visual_id != CONCEPT_POP_ID_INVALID, "visual interned");
    TEST_ASSERT(visual_id != text_id,
                "visual id differs from text id BEFORE binding");

    int rc = concept_registry_bind_modalities(reg, text_id, visual_id);
    TEST_ASSERT(rc == 0, "bind_modalities succeeds");

    concept_pop_id_t can_t = concept_registry_canonical(reg, text_id);
    concept_pop_id_t can_v = concept_registry_canonical(reg, visual_id);
    TEST_ASSERT(can_t == can_v,
                "canonical(text) == canonical(visual) AFTER binding");

    /* Re-interning either modality returns the canonical root. */
    concept_pop_id_t reintern_t = concept_registry_intern_text(reg, "leaves");
    concept_pop_id_t reintern_v =
        concept_registry_intern_visual(reg, visual_features, 16);
    TEST_ASSERT(reintern_t == can_t, "re-intern text → canonical root");
    TEST_ASSERT(reintern_v == can_v, "re-intern visual → canonical root");

    concept_registry_destroy(reg);
}

/*=============================================================================
 * Test 3: Three-modality binding (text + visual + audio)
 *===========================================================================*/

static void test_three_modality_binding(void)
{
    printf("\n[3] test_three_modality_binding\n");

    concept_registry_t* reg = concept_registry_create(0);
    if (!reg) return;

    concept_pop_id_t t = concept_registry_intern_text(reg, "drum");

    float visual[16];
    for (int i = 0; i < 16; i++) visual[i] = (float)i * 0.2f;
    concept_pop_id_t v = concept_registry_intern_visual(reg, visual, 16);

    float audio[32];
    for (int i = 0; i < 32; i++) audio[i] = sinf((float)i * 0.5f);
    concept_pop_id_t a = concept_registry_intern_audio(reg, audio, 32);

    TEST_ASSERT(t != v && v != a && t != a,
                "three distinct ids before any binding");

    concept_registry_bind_modalities(reg, t, v);
    concept_registry_bind_modalities(reg, t, a);

    concept_pop_id_t ct = concept_registry_canonical(reg, t);
    concept_pop_id_t cv = concept_registry_canonical(reg, v);
    concept_pop_id_t ca = concept_registry_canonical(reg, a);
    TEST_ASSERT(ct == cv && cv == ca,
                "all three modalities canonicalize to same root");
}

/*=============================================================================
 * Test 4: Bulk integrity after random ops
 *===========================================================================*/

static void test_random_bulk_integrity(void)
{
    printf("\n[4] test_random_bulk_integrity (1000 interns + 100 binds)\n");

    concept_registry_t* reg = concept_registry_create(256);
    if (!reg) return;

    /* Use a fixed seed for reproducibility — the test must be
     * deterministic across CI runs. */
    srand(42);

    concept_pop_id_t observed[2048];
    int n_observed = 0;

    /* 1000 random text interns. Some duplicates → fewer roots than calls. */
    for (int i = 0; i < 1000; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "word_%d", rand() % 600);
        concept_pop_id_t id = concept_registry_intern_text(reg, buf);
        if (id != CONCEPT_POP_ID_INVALID && n_observed < 2048) {
            observed[n_observed++] = id;
        }
    }

    /* 100 random binds — choose pairs uniformly from observed. */
    int merges = 0;
    for (int i = 0; i < 100; i++) {
        if (n_observed < 2) break;
        int ia = rand() % n_observed;
        int ib = rand() % n_observed;
        if (ia == ib) continue;
        concept_pop_id_t pre_a = concept_registry_canonical(reg, observed[ia]);
        concept_pop_id_t pre_b = concept_registry_canonical(reg, observed[ib]);
        if (concept_registry_bind_modalities(reg, observed[ia], observed[ib]) == 0
            && pre_a != pre_b) {
            merges++;
        }
    }

    /* Integrity: every observed id resolves to a valid root in the
     * allocated id space. The canonical of canonical equals canonical
     * (idempotence). */
    size_t total = concept_registry_total_referents(reg);
    int bad = 0;
    for (int i = 0; i < n_observed; i++) {
        concept_pop_id_t r1 = concept_registry_canonical(reg, observed[i]);
        if (r1 == CONCEPT_POP_ID_INVALID) { bad++; continue; }
        if (r1 == CONCEPT_POP_ID_NONE)    { bad++; continue; }
        if ((size_t)r1 > total)            { bad++; continue; }
        concept_pop_id_t r2 = concept_registry_canonical(reg, r1);
        if (r1 != r2) { bad++; }
    }
    TEST_ASSERT(bad == 0, "every observed id resolves to a valid stable root");

    /* total_modality_bindings counter equals the number of merges that
     * actually changed a root (not the number of bind calls). */
    size_t reported = concept_registry_total_modality_bindings(reg);
    TEST_ASSERT(reported == (size_t)merges,
                "modality_bindings counter matches actual merges");

    concept_registry_destroy(reg);
}

/*=============================================================================
 * Test 5: Save + load round-trip
 *===========================================================================*/

static void test_save_load_roundtrip(void)
{
    printf("\n[5] test_save_load_roundtrip\n");

    concept_registry_t* reg1 = concept_registry_create(0);
    if (!reg1) return;

    concept_pop_id_t t1 = concept_registry_intern_text(reg1, "leaves");
    concept_pop_id_t t2 = concept_registry_intern_text(reg1, "ball");
    concept_pop_id_t t3 = concept_registry_intern_text(reg1, "cup");

    float v[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    concept_pop_id_t vid = concept_registry_intern_visual(reg1, v, 8);

    float a[4] = {0.9f, -0.5f, 0.1f, 0.3f};
    concept_pop_id_t aid = concept_registry_intern_audio(reg1, a, 4);

    /* Bind text "leaves" ↔ visual + audio. Leave "ball" + "cup" alone. */
    concept_registry_bind_modalities(reg1, t1, vid);
    concept_registry_bind_modalities(reg1, t1, aid);

    concept_pop_id_t pre_can_t1  = concept_registry_canonical(reg1, t1);
    concept_pop_id_t pre_can_vid = concept_registry_canonical(reg1, vid);
    concept_pop_id_t pre_can_aid = concept_registry_canonical(reg1, aid);
    concept_pop_id_t pre_can_t2  = concept_registry_canonical(reg1, t2);
    concept_pop_id_t pre_can_t3  = concept_registry_canonical(reg1, t3);
    size_t pre_total = concept_registry_total_referents(reg1);
    size_t pre_merges = concept_registry_total_modality_bindings(reg1);

    /* Save to tmp file. */
    const char* path = "/tmp/test_concept_registry_roundtrip.bin";
    FILE* fp = fopen(path, "wb");
    TEST_ASSERT(fp != NULL, "opened tmp save file");
    if (!fp) { concept_registry_destroy(reg1); return; }
    int rc = concept_registry_save(reg1, fp);
    fclose(fp);
    TEST_ASSERT(rc == 0, "save returned 0");

    /* Load into a fresh registry. */
    concept_registry_t* reg2 = concept_registry_create(0);
    fp = fopen(path, "rb");
    TEST_ASSERT(fp != NULL, "opened tmp load file");
    if (!fp) { concept_registry_destroy(reg1); concept_registry_destroy(reg2); return; }
    rc = concept_registry_load(reg2, fp);
    fclose(fp);
    TEST_ASSERT(rc == 0, "load returned 0");

    /* Stats match. */
    TEST_ASSERT(concept_registry_total_referents(reg2) == pre_total,
                "total_referents preserved");
    TEST_ASSERT(concept_registry_total_modality_bindings(reg2) == pre_merges,
                "total_modality_bindings preserved");

    /* Re-interning the same text/feature should return the same canonical
     * root as before (the lookup tables survive). */
    concept_pop_id_t post_t1  = concept_registry_intern_text(reg2, "leaves");
    concept_pop_id_t post_t2  = concept_registry_intern_text(reg2, "ball");
    concept_pop_id_t post_t3  = concept_registry_intern_text(reg2, "cup");
    concept_pop_id_t post_vid = concept_registry_intern_visual(reg2, v, 8);
    concept_pop_id_t post_aid = concept_registry_intern_audio(reg2, a, 4);

    TEST_ASSERT(concept_registry_canonical(reg2, post_t1)  == pre_can_t1,
                "leaves canonical preserved");
    TEST_ASSERT(concept_registry_canonical(reg2, post_t2)  == pre_can_t2,
                "ball canonical preserved");
    TEST_ASSERT(concept_registry_canonical(reg2, post_t3)  == pre_can_t3,
                "cup canonical preserved");
    TEST_ASSERT(concept_registry_canonical(reg2, post_vid) == pre_can_vid,
                "visual canonical preserved");
    TEST_ASSERT(concept_registry_canonical(reg2, post_aid) == pre_can_aid,
                "audio canonical preserved");
    /* The bound trio shares one root after the trip. */
    TEST_ASSERT(concept_registry_canonical(reg2, post_t1) ==
                concept_registry_canonical(reg2, post_vid),
                "post-load leaves ↔ visual still merged");
    TEST_ASSERT(concept_registry_canonical(reg2, post_t1) ==
                concept_registry_canonical(reg2, post_aid),
                "post-load leaves ↔ audio still merged");
    /* The un-bound entries are separate. */
    TEST_ASSERT(concept_registry_canonical(reg2, post_t2) !=
                concept_registry_canonical(reg2, post_t1),
                "post-load ball not merged with leaves");

    concept_registry_destroy(reg1);
    concept_registry_destroy(reg2);
    remove(path);
}

/*=============================================================================
 * Driver
 *===========================================================================*/

int main(void)
{
    printf("===========================================================\n");
    printf("Slice B regression — concept_registry_binding\n");
    printf("===========================================================\n");

    test_text_idempotence();
    test_visual_text_binding();
    test_three_modality_binding();
    test_random_bulk_integrity();
    test_save_load_roundtrip();

    printf("\n===========================================================\n");
    printf("RESULT: %d passed, %d failed\n", g_passed, g_failed);
    printf("===========================================================\n");
    return g_failed == 0 ? 0 : 1;
}
