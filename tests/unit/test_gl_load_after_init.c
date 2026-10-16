/**
 * @file test_gl_load_after_init.c
 * @brief Verify the deferred sidecar load (.gl_lang) runs at the right point.
 *
 * Bug context (2026-05-06):
 *   The grounded-language sidecar load used to live inside brain_load(),
 *   gated on `if (brain->grounded_lang)`. But grounded_lang is created by
 *   nimcp_brain_eager_init_cognitive() — which runs AFTER brain_load()
 *   returns. So the gate was always false and the trained .gl_lang sidecar
 *   was silently dropped on every daemon resume.
 *
 *   Fix: brain_load_auto() now stashes the source filepath in
 *   brain->loaded_from_path, and brain_load_post_init_sidecars() — invoked
 *   from nimcp_brain_eager_init_cognitive() — replays the .immune / .kg /
 *   .gl_lang loads once those subsystems exist.
 *
 * This test exercises brain_load_post_init_sidecars() directly:
 *   1. Create a synthetic .gl_lang sidecar via gl_persistence_save() with
 *      a recognizable word ("gavagai") fast-mapped into the lexicon.
 *   2. Calloc a minimal brain_struct, attach a fresh grounded_language_t
 *      (which only contains seeded function words, no "gavagai"), and set
 *      brain->loaded_from_path to the synthetic checkpoint stem.
 *   3. Call brain_load_post_init_sidecars(brain).
 *   4. Verify "gavagai" is now visible via grounded_language_lookup().
 *
 * Also verifies the no-op contract: empty loaded_from_path → no load
 * attempt, no log spam, lexicon untouched.
 *
 * Standalone harness (no GTest dep). Compile:
 *   gcc -I include tests/unit/test_gl_load_after_init.c \
 *       -L build/lib -lnimcp \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_gl_load_after_init
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"
#include "core/brain/nimcp_brain_internal.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Helper from src/core/brain/persistence/nimcp_brain_persistence.c.
 * Not declared in any public header; the test pulls it via extern. */
extern void brain_load_post_init_sidecars(brain_t brain);

#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
    exit(1); } } while (0)

#define SEMANTIC_DIM 128u

/* Build a .gl_lang sidecar at <stem>.gl_lang containing one known word
 * via gl_persistence_save(). Returns the full sidecar path (caller-owned
 * static buffer is fine — single test process). */
static const char* write_synthetic_sidecar(const char* stem,
                                           const char* known_word) {
    static char path[512];
    snprintf(path, sizeof(path), "%s.gl_lang", stem);

    grounded_language_t* sender = grounded_language_create(SEMANTIC_DIM, NULL);
    CHECK(sender != NULL, "create sender gl");

    /* Synthetic feature vector for "gavagai" (any non-zero pattern works —
     * persistence round-trips raw bytes regardless of semantic content). */
    float features[SEMANTIC_DIM];
    for (uint32_t i = 0; i < SEMANTIC_DIM; i++) {
        features[i] = 0.1f * (float)((i % 7) + 1);
    }

    uint64_t cid = grounded_language_fast_map(sender, known_word, features,
                                              SEMANTIC_DIM, /*category=*/1u);
    CHECK(cid != 0, "fast_map seeded the known word");

    /* Confirm it landed in the sender lexicon before we save. */
    const gl_lexicon_entry_t* entry = grounded_language_lookup(sender, known_word);
    CHECK(entry != NULL, "sender lexicon contains known word pre-save");

    int rc = gl_persistence_save(sender, path);
    CHECK(rc == 0, "gl_persistence_save synthetic sidecar");

    grounded_language_destroy(sender);
    return path;
}

/* Allocate a zero-initialized brain_struct and attach a fresh grounded_lang.
 * We never run any brain logic against this struct — it's a vehicle for
 * exercising brain_load_post_init_sidecars(). */
static brain_t make_minimal_brain_with_gl(void) {
    brain_t b = (brain_t)calloc(1, sizeof(*b));
    CHECK(b != NULL, "calloc brain_struct");

    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, NULL);
    CHECK(gl != NULL, "create brain.grounded_lang");
    b->grounded_lang = (struct grounded_language*)gl;

    return b;
}

static void destroy_minimal_brain(brain_t b) {
    if (!b) return;
    if (b->grounded_lang) {
        grounded_language_destroy((grounded_language_t*)b->grounded_lang);
        b->grounded_lang = NULL;
    }
    free(b);
}

/* ----------------------------------------------------- happy path */
static void test_loads_when_path_set_and_gl_present(void) {
    const char* stem = "/tmp/nimcp_gl_load_after_init_a";
    const char* known = "gavagai";

    const char* sidecar_path = write_synthetic_sidecar(stem, known);

    brain_t b = make_minimal_brain_with_gl();
    snprintf(b->loaded_from_path, sizeof(b->loaded_from_path), "%s", stem);

    /* Pre-condition: known word is NOT in seeded lexicon. */
    const gl_lexicon_entry_t* pre = grounded_language_lookup(
        (grounded_language_t*)b->grounded_lang, known);
    CHECK(pre == NULL, "known word absent before deferred load");

    /* The fix in action. */
    brain_load_post_init_sidecars(b);

    /* Post-condition: word is now visible — sidecar was loaded. */
    const gl_lexicon_entry_t* post = grounded_language_lookup(
        (grounded_language_t*)b->grounded_lang, known);
    CHECK(post != NULL, "known word present after deferred load");

    destroy_minimal_brain(b);
    unlink(sidecar_path);
    printf("PASS test_loads_when_path_set_and_gl_present\n");
}

/* ----------------------------------------------------- no-op: empty path */
static void test_noop_when_loaded_from_path_empty(void) {
    brain_t b = make_minimal_brain_with_gl();
    /* Do NOT set loaded_from_path — calloc leaves it as "". */

    brain_load_post_init_sidecars(b);  /* must NOT crash, must NOT load */

    /* Lexicon should still be the seed-only baseline. */
    const gl_lexicon_entry_t* probe = grounded_language_lookup(
        (grounded_language_t*)b->grounded_lang, "gavagai");
    CHECK(probe == NULL, "no spurious load when loaded_from_path is empty");

    destroy_minimal_brain(b);
    printf("PASS test_noop_when_loaded_from_path_empty\n");
}

/* ----------------------------------------------------- no-op: NULL gl */
static void test_noop_when_gl_null(void) {
    /* Path is set, but grounded_lang is NULL — helper must skip the
     * gl_lang block (and not crash) regardless. Mirrors the production
     * pre-eager_init_cognitive state. */
    brain_t b = (brain_t)calloc(1, sizeof(*b));
    CHECK(b != NULL, "calloc brain_struct");
    snprintf(b->loaded_from_path, sizeof(b->loaded_from_path),
             "%s", "/tmp/nimcp_gl_load_after_init_nonexistent_stem");

    brain_load_post_init_sidecars(b);  /* must NOT crash */

    free(b);
    printf("PASS test_noop_when_gl_null\n");
}

int main(void) {
    test_loads_when_path_set_and_gl_present();
    test_noop_when_loaded_from_path_empty();
    test_noop_when_gl_null();
    printf("ALL PASS\n");
    return 0;
}
