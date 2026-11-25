/**
 * @file nimcp_language_prime_resonance_bridge.c
 * @brief Prime-signature content-addressable resonance over discourse turns.
 *
 * WHAT: Every new discourse turn vector gets a prime signature
 *       (prime_sig_from_floats) and is stored in a 32-slot ring together with
 *       a copy of the vector. The new turn is matched against the ring by
 *       Jaccard similarity; the best match is the "resonant" earlier turn.
 *       During production the resonant turn is recalled (pattern completion)
 *       and blended into content_intent with weight 0.10*resonance when
 *       resonance >= 0.5, and a familiar topic lowers the produce floor.
 * WHY:  Content-addressable recall of a related earlier utterance keeps a
 *       conversation on topic (hippocampal pattern completion; the module's
 *       documented "similarity retrieval / memory cue matching" role). The
 *       2026-10-21 version matched against rand() vectors.
 * HOW:  gl discourse turns (copied by the group under the gl mutate_lock).
 *       Signatures are computed only on new turns (~500ns + 50ns/float).
 *       State is rebuilt from conversation; not persisted.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <string.h>

#define LANG_PR_RING 32u

typedef struct {
    prime_signature_t* sig[LANG_PR_RING];
    float*             vec;            /* LANG_PR_RING * dim */
    uint32_t           dim;
    uint32_t           count, head;    /* head = next slot to write */
    int32_t            best;           /* ring slot of last best match, -1 none */
    float              resonance;      /* last best Jaccard */
} lang_pr_state_t;

static void* pr_create(struct brain_struct* brain) {
    if (!brain || !brain->grounded_lang) { return NULL; }
    uint32_t dim = brain->grounded_lang->semantic_dim;
    if (dim == 0 || dim > 65536u) { return NULL; }
    lang_pr_state_t* s = (lang_pr_state_t*)nimcp_calloc(1, sizeof(*s));
    if (!s) { return NULL; }
    s->vec = (float*)nimcp_calloc((size_t)LANG_PR_RING * dim, sizeof(float));
    if (!s->vec) { nimcp_free(s); return NULL; }
    s->dim  = dim;
    s->best = -1;
    return s;
}

static void pr_destroy(void* st) {
    lang_pr_state_t* s = (lang_pr_state_t*)st;
    if (!s) { return; }
    for (uint32_t i = 0; i < LANG_PR_RING; i++) {
        if (s->sig[i]) { prime_sig_destroy(s->sig[i]); }
    }
    nimcp_free(s->vec);
    nimcp_free(s);
}

static int pr_update(void* st, struct brain_struct* brain,
                     const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    lang_pr_state_t* s = (lang_pr_state_t*)st;
    if (!s || !brain || !in) { return -1; }

    if (in->new_turn && in->lang_vec && in->dim == s->dim) {
        for (uint32_t i = 0; i < s->dim; i++) {
            if (!isfinite(in->lang_vec[i])) { return -1; }   /* never index NaN */
        }
        prime_signature_t* q = prime_sig_from_floats(in->lang_vec, s->dim);
        if (!q) { return -1; }
        float best = 0.0f;
        int32_t best_i = -1;
        for (uint32_t i = 0; i < s->count; i++) {
            if (!s->sig[i]) { continue; }
            float j = lang_cog_finite_or(prime_sig_jaccard(q, s->sig[i]), 0.0f);
            if (j > best) { best = j; best_i = (int32_t)i; }
        }
        /* Insert the new turn (after matching, so it never matches itself). */
        uint32_t slot = s->head;
        if (s->sig[slot]) { prime_sig_destroy(s->sig[slot]); }
        s->sig[slot] = q;
        memcpy(s->vec + (size_t)slot * s->dim, in->lang_vec, (size_t)s->dim * sizeof(float));
        s->head = (s->head + 1u) % LANG_PR_RING;
        if (s->count < LANG_PR_RING) { s->count++; }
        /* The slot just overwritten can no longer be the recall target. */
        s->best      = (best_i == (int32_t)slot) ? -1 : best_i;
        s->resonance = lang_cog_clampf(best, 0.0f, 1.0f);
    }
    out->floor_adjust = -0.05f * s->resonance;
    out->primary   = s->resonance;
    out->secondary = (float)s->count;
    out->valid     = true;
    return 0;
}

static float pr_recall(void* st, float* out, uint32_t dim) {
    lang_pr_state_t* s = (lang_pr_state_t*)st;
    if (!s || !out || dim != s->dim || s->best < 0 || s->resonance < 0.5f) { return 0.0f; }
    memcpy(out, s->vec + (size_t)s->best * s->dim, (size_t)dim * sizeof(float));
    return 0.10f * s->resonance;
}

const lang_cog_bridge_vt_t lang_prime_resonance_bridge_vt = {
    .name = "prime_resonance", .gl_bit = GL_BRIDGE_PRIME_RESONANCE,
    .create = pr_create, .destroy = pr_destroy, .update = pr_update,
    .recall = pr_recall, .save = NULL, .load = NULL,
};
