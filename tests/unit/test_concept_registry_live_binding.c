/**
 * @file test_concept_registry_live_binding.c
 * @brief Walkthrough-2 regression — concept_registry is interned via the
 *        live grounded_language path, not just the standalone registry API.
 *
 * Slice B (commit c94b13375) shipped the concept_registry as a standalone
 * data structure with its own unit test (test_concept_registry_binding).
 * What walkthrough-1 surfaced is that *no live caller* invoked
 * intern_text / bind_modalities outside that test, so the registry was
 * created at brain init and never populated during actual training.
 *
 * Walkthrough-2 wires the registry into grounded_language via
 * grounded_language_set_concept_registry(), and mirror_binding_to_bridge()
 * now calls concept_registry_intern_text() whenever a lexicon entry is
 * mirrored. This test verifies that path actually fires: a sequence of
 * grounding events produces a non-zero referent count in the registry.
 *
 * Coverage:
 *   1. Without registry wired: ground() works but registry stays empty.
 *   2. With registry wired: ground() interns the text form, referent
 *      count rises.
 *   3. Re-grounding the same word does NOT inflate referent count
 *      (idempotence preserved through the wiring).
 *   4. Two different words produce two distinct referent ids.
 *
 * Run via ctest; exit code 0 on PASS.
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_concept_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) do {                                  \
    if (cond) { printf("  [PASS] %s\n", msg); g_passed++; }          \
    else      { printf("  [FAIL] %s\n", msg); g_failed++; }          \
} while (0)

static void make_event(gl_grounding_event_t* ev, const char* word,
                       const float* features, uint32_t dim) {
    memset(ev, 0, sizeof(*ev));
    ev->word = word;
    ev->modality = GL_MODALITY_VISUAL;
    ev->sensory_features = features;
    ev->feature_dim = dim;
    ev->emotional_valence = 0.0f;
    ev->emotional_arousal = 0.5f;
    ev->attention = 1.0f;
    ev->negative = false;
}

int main(void) {
    printf("\n=== Walkthrough-2: live concept_registry binding via grounded_language ===\n\n");

    /* Test 1: registry not wired → ground() works, registry empty. */
    {
        printf("Test 1: ground() without registry wired\n");
        grounded_language_t* gl = grounded_language_create(0, NULL);
        TEST_ASSERT(gl != NULL, "grounded_language_create");

        concept_registry_t* reg = concept_registry_create(64);
        TEST_ASSERT(reg != NULL, "concept_registry_create");
        size_t initial = concept_registry_total_referents(reg);

        float feats[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        gl_grounding_event_t ev;
        make_event(&ev, "leaves", feats, 8);
        int rc = grounded_language_ground(gl, &ev);
        TEST_ASSERT(rc == 0, "ground succeeds without registry");
        TEST_ASSERT(concept_registry_total_referents(reg) == initial,
                    "registry untouched when not wired");

        concept_registry_destroy(reg);
        grounded_language_destroy(gl);
    }

    /* Test 2: wire registry, ground "leaves" → registry grows by 1. */
    {
        printf("\nTest 2: ground() with registry wired interns the text form\n");
        grounded_language_t* gl = grounded_language_create(0, NULL);
        concept_registry_t* reg = concept_registry_create(64);
        grounded_language_set_concept_registry(gl, reg);

        size_t before = concept_registry_total_referents(reg);

        float feats[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        gl_grounding_event_t ev;
        make_event(&ev, "leaves", feats, 8);
        int rc = grounded_language_ground(gl, &ev);
        TEST_ASSERT(rc == 0, "ground succeeds");

        size_t after = concept_registry_total_referents(reg);
        TEST_ASSERT(after > before, "registry referent count grew");

        /* Direct probe: the text should resolve to the same id when
         * re-interned (idempotence via the registry's hash table). */
        concept_pop_id_t id_a = concept_registry_intern_text(reg, "leaves");
        concept_pop_id_t id_b = concept_registry_intern_text(reg, "leaves");
        TEST_ASSERT(id_a == id_b && id_a != CONCEPT_POP_ID_INVALID,
                    "re-interning 'leaves' returns same id");

        concept_registry_destroy(reg);
        grounded_language_destroy(gl);
    }

    /* Test 3: re-grounding the same word twice → still 1 unique referent
     * (the mirror_binding path may fire on every ground call, but the
     * registry's intern_text dedupes). */
    {
        printf("\nTest 3: re-grounding 'fire' twice does not duplicate referents\n");
        grounded_language_t* gl = grounded_language_create(0, NULL);
        concept_registry_t* reg = concept_registry_create(64);
        grounded_language_set_concept_registry(gl, reg);

        float feats[8] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
        gl_grounding_event_t ev;
        make_event(&ev, "fire", feats, 8);

        size_t baseline = concept_registry_total_referents(reg);
        grounded_language_ground(gl, &ev);
        size_t after_first = concept_registry_total_referents(reg);
        grounded_language_ground(gl, &ev);
        size_t after_second = concept_registry_total_referents(reg);

        TEST_ASSERT(after_first > baseline, "first ground bumps referent count");
        TEST_ASSERT(after_second == after_first,
                    "second ground does NOT bump referent count (idempotent)");

        concept_registry_destroy(reg);
        grounded_language_destroy(gl);
    }

    /* Test 4: grounding two different words produces two distinct referent
     * roots. */
    {
        printf("\nTest 4: two different words → two distinct registry roots\n");
        grounded_language_t* gl = grounded_language_create(0, NULL);
        concept_registry_t* reg = concept_registry_create(64);
        grounded_language_set_concept_registry(gl, reg);

        float feats_a[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        float feats_b[8] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
        gl_grounding_event_t ev_a, ev_b;
        make_event(&ev_a, "water", feats_a, 8);
        make_event(&ev_b, "stone", feats_b, 8);

        grounded_language_ground(gl, &ev_a);
        grounded_language_ground(gl, &ev_b);

        concept_pop_id_t water_id = concept_registry_intern_text(reg, "water");
        concept_pop_id_t stone_id = concept_registry_intern_text(reg, "stone");
        TEST_ASSERT(water_id != stone_id, "distinct words get distinct ids");
        TEST_ASSERT(concept_registry_canonical(reg, water_id)
                    != concept_registry_canonical(reg, stone_id),
                    "distinct words have distinct canonical roots");

        concept_registry_destroy(reg);
        grounded_language_destroy(gl);
    }

    printf("\n=== Summary: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
