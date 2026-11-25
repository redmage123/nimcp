/**
 * @file nimcp_language_hamiltonian_bridge.c
 * @brief Hamiltonian (energy-conservation) coherence of the language trajectory.
 *
 * WHAT: Treats the discourse semantic state q_k (newest turn vector) as a
 *       position and p_k = q_k - q_{k-1} as momentum. The kinetic energy
 *       T_k = 1/2 ||p_k||^2 of each new turn is compared with its running
 *       mean; a turn whose energy departs strongly from the conserved level
 *       is a topic jump (low coherence). When the brain's HNN layer is
 *       active its relative energy deviation is folded in as well.
 * WHY:  Original intent (2026-10-21): "H(semantic_state) = constant -> topic
 *       coherence preserved". That version called a non-existent brain->hnn
 *       and an hnn_forward on content_intent; this keeps the intent with
 *       quantities that exist.
 * HOW:  coherence = 1/(1 + drift) * 1/(1 + hnn_dev);
 *       floor_adjust = 0.10*(0.5 - coherence) clamped [-0.05, 0.05];
 *       intent_gain  = 0.9 + 0.2*coherence. Running energy mean persists.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>

typedef struct {
    float h_mean;     /* running mean of turn kinetic energy */
    float coherence;  /* last coherence */
    float n_turns;    /* turns observed */
} lang_ham_state_t;

static void* ham_create(struct brain_struct* brain) {
    if (!brain || !brain->grounded_lang) { return NULL; }
    lang_ham_state_t* s = (lang_ham_state_t*)nimcp_calloc(1, sizeof(*s));
    if (s) { s->coherence = 1.0f; }
    return s;
}

static void ham_destroy(void* st) { if (st) { nimcp_free(st); } }

static int ham_update(void* st, struct brain_struct* brain,
                      const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    lang_ham_state_t* s = (lang_ham_state_t*)st;
    if (!s || !brain || !in) { return -1; }

    if (in->new_turn && in->lang_vec && in->lang_prev && in->dim > 0) {
        double t = 0.0;
        for (uint32_t i = 0; i < in->dim; i++) {
            double p = (double)in->lang_vec[i] - (double)in->lang_prev[i];
            t += p * p;
        }
        float T = lang_cog_finite_or((float)(0.5 * t), 0.0f);
        if (s->n_turns < 1.0f) {
            s->h_mean = T;
        }
        float drift = fabsf(T - s->h_mean) / (s->h_mean + 1e-3f);
        drift = lang_cog_clampf(drift, 0.0f, 4.0f);
        s->h_mean = lang_cog_finite_or(0.95f * s->h_mean + 0.05f * T, s->h_mean);
        s->n_turns += 1.0f;

        float hnn_dev = 0.0f;
        if (brain->network_metrics.hnn_active) {
            hnn_dev = lang_cog_clampf(lang_cog_finite_or(
                          brain->network_metrics.hnn_energy_deviation, 0.0f), 0.0f, 1.0f);
        }
        s->coherence = (1.0f / (1.0f + drift)) * (1.0f / (1.0f + hnn_dev));
    }
    if (s->n_turns < 2.0f) {
        out->valid = true;            /* not enough trajectory yet: neutral */
        out->primary = s->coherence;
        return 0;
    }
    out->floor_adjust = lang_cog_clampf(0.10f * (0.5f - s->coherence), -0.05f, 0.05f);
    out->intent_gain  = 0.9f + 0.2f * lang_cog_clampf(s->coherence, 0.0f, 1.0f);
    out->primary   = s->coherence;
    out->secondary = s->h_mean;
    out->valid     = true;
    return 0;
}

static int ham_save(const void* st, FILE* f) {
    const lang_ham_state_t* s = (const lang_ham_state_t*)st;
    float blob[3] = { s->h_mean, s->coherence, s->n_turns };
    return lang_cog_write_floats(f, blob, 3);
}

static int ham_load(void* st, FILE* f) {
    lang_ham_state_t* s = (lang_ham_state_t*)st;
    float blob[3];
    if (lang_cog_read_floats(f, blob, 3) != 0) { return -1; }
    s->h_mean    = fmaxf(0.0f, blob[0]);
    s->coherence = lang_cog_clampf(blob[1], 0.0f, 1.0f);
    s->n_turns   = fmaxf(0.0f, blob[2]);
    return 0;
}

const lang_cog_bridge_vt_t lang_hamiltonian_bridge_vt = {
    .name = "hamiltonian", .gl_bit = GL_BRIDGE_HAMILTONIAN,
    .create = ham_create, .destroy = ham_destroy, .update = ham_update,
    .recall = NULL, .save = ham_save, .load = ham_load,
};
