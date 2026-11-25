/**
 * @file nimcp_language_cognitive_wired.h
 * @brief Language <-> cognitive-module bridges (wired group BRAIN_WIRED_LANGUAGE).
 *
 * WHAT: Fourteen small bridges between the grounded-language system and the
 *       brain's cognitive modules (ACC, amygdala, basal ganglia, global
 *       workspace, episodic replay, Hamiltonian coherence, hypothalamus,
 *       imagination, introspection, JEPA, mirror neurons, prime-signature
 *       resonance, theory of mind, world model).
 * WHY:  The 2026-10-21 bridge files returned hard-coded constants / rand(),
 *       cast the cascade state to a struct whose first field is NOT
 *       content_intent, and their entry points duplicated the no-op
 *       grounded_language_integrate_* symbols already exported by
 *       nimcp_language_integration_stubs.c. They were never compiled.
 * HOW:  Each module file exports one vtable (lang_<mod>_bridge_vt). The
 *       group (src/core/brain/wired/nimcp_brain_wired_language.c) builds a
 *       language input snapshot every N training ticks (newest + previous
 *       discourse turn vectors copied under the gl mutate_lock, comprehension
 *       confidence, external reward), lets every bridge read its REAL module
 *       state and turn it into a bounded signal:
 *         - floor_adjust: contribution to the metacognitive produce-floor
 *           (the integration the cascade documents as correct for modulatory
 *           modules that carry no semantic content vector);
 *         - intent_gain:  multiplicative gain on content_intent (salience);
 *         - recall():     optional semantic recall vector (pattern completion)
 *           blended into content_intent.
 *       The cascade consumes the aggregate through
 *       brain_wired_language_floor_adjust() and
 *       brain_wired_language_apply_cascade().
 *
 * Every value a bridge emits is finite and bounded; bridges whose subsystem
 * is NULL on this brain are not created.
 */
#ifndef NIMCP_LANGUAGE_COGNITIVE_WIRED_H
#define NIMCP_LANGUAGE_COGNITIVE_WIRED_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/** Language-side snapshot handed to every bridge on an update. */
typedef struct lang_cog_input {
    const float* lang_vec;        /**< newest discourse turn vector, or NULL */
    const float* lang_prev;       /**< previous discourse turn vector, or NULL */
    uint32_t     dim;             /**< length of both vectors (gl semantic_dim) */
    bool         new_turn;        /**< lang_vec changed since the last update */
    float        semantic_surprise; /**< (1 - cos(prev, cur)) / 2 in [0,1]; 0 if n/a */
    float        comprehension_conf; /**< gl running comprehension confidence [0,1] */
    float        external_reward; /**< brain->last_external_reward (caregiver critic) */
    uint64_t     external_reward_us; /**< timestamp of that reward (0 = never) */
    uint64_t     update_index;    /**< monotonically increasing update counter */
} lang_cog_input_t;

/** Signal a bridge returns. Neutral = {0, 1, ...}. */
typedef struct lang_cog_signal {
    float floor_adjust;  /**< produce-floor contribution; >0 = more cautious */
    float intent_gain;   /**< content_intent gain; 1 = neutral */
    float primary;       /**< headline module reading (status only) */
    float secondary;     /**< second module reading (status only) */
    bool  valid;         /**< false = module unreadable this update (ignored) */
} lang_cog_signal_t;

typedef struct lang_cog_bridge_vt {
    const char* name;        /**< short name, <= 15 chars (status + sidecar key) */
    uint32_t    gl_bit;      /**< GL_BRIDGE_* bit (runtime enable via gl flags) */
    /** Create state; NULL when the bridged subsystem is absent. */
    void* (*create)(struct brain_struct* brain);
    void  (*destroy)(void* st);
    /** Read module + language input, fill *out. Must not take the group lock. */
    int   (*update)(void* st, struct brain_struct* brain,
                    const lang_cog_input_t* in, lang_cog_signal_t* out);
    /** Optional: copy a recall vector (dim floats) into out; return blend
     *  weight in [0, 0.2], 0 = nothing to recall. Called under the group lock. */
    float (*recall)(void* st, float* out, uint32_t dim);
    /** Optional learned-state persistence (small fixed blobs). */
    int   (*save)(const void* st, FILE* f);
    int   (*load)(void* st, FILE* f);
} lang_cog_bridge_vt_t;

extern const lang_cog_bridge_vt_t lang_acc_bridge_vt;
extern const lang_cog_bridge_vt_t lang_amygdala_bridge_vt;
extern const lang_cog_bridge_vt_t lang_basal_ganglia_bridge_vt;
extern const lang_cog_bridge_vt_t lang_consciousness_bridge_vt;
extern const lang_cog_bridge_vt_t lang_episodic_replay_bridge_vt;
extern const lang_cog_bridge_vt_t lang_hamiltonian_bridge_vt;
extern const lang_cog_bridge_vt_t lang_hypothalamus_bridge_vt;
extern const lang_cog_bridge_vt_t lang_imagination_bridge_vt;
extern const lang_cog_bridge_vt_t lang_introspection_bridge_vt;
extern const lang_cog_bridge_vt_t lang_jepa_bridge_vt;
extern const lang_cog_bridge_vt_t lang_mirror_bridge_vt;
extern const lang_cog_bridge_vt_t lang_prime_resonance_bridge_vt;
extern const lang_cog_bridge_vt_t lang_tom_bridge_vt;
extern const lang_cog_bridge_vt_t lang_world_model_bridge_vt;

/* ---- helpers shared by the bridge files (static inline, no symbols) ---- */
static inline float lang_cog_clampf(float v, float lo, float hi) {
    if (!(v == v)) return lo;           /* NaN -> lo */
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float lang_cog_finite_or(float v, float fallback) {
    return (v == v && v <= 3.0e38f && v >= -3.0e38f) ? v : fallback;
}
static inline void lang_cog_signal_neutral(lang_cog_signal_t* s) {
    s->floor_adjust = 0.0f; s->intent_gain = 1.0f;
    s->primary = 0.0f; s->secondary = 0.0f; s->valid = false;
}
/** Fixed-size float blob persistence helpers (count-prefixed). */
static inline int lang_cog_write_floats(FILE* f, const float* v, uint32_t n) {
    return (fwrite(&n, sizeof(n), 1, f) == 1 &&
            fwrite(v, sizeof(float), n, f) == n) ? 0 : -1;
}
static inline int lang_cog_read_floats(FILE* f, float* v, uint32_t n) {
    uint32_t got = 0;
    if (fread(&got, sizeof(got), 1, f) != 1 || got != n) return -1;
    if (fread(v, sizeof(float), n, f) != n) return -1;
    for (uint32_t i = 0; i < n; i++) v[i] = lang_cog_finite_or(v[i], 0.0f);
    return 0;
}

/** brain->imagination for TUs that cannot include nimcp_brain_internal.h
 *  (imagination header typedef clash). NULL-safe. */
void* lang_cog_brain_imagination(struct brain_struct* brain);

/* ---- group-level cascade entry points (nimcp_brain_wired_language.c) ---- */

/** Summed, clamped [-0.3, 0.3] produce-floor contribution of all live
 *  bridges (0 when the group is absent). Thread-safe. Intended to be added
 *  to the cascade's metacognitive `adjust` before
 *  grounded_language_set_metacog_floor_adjust(). */
float brain_wired_language_floor_adjust(struct brain_struct* brain);

/** Apply the aggregate intent gain (clamped [0.8, 1.2]) and any semantic
 *  recall vector to a production_cascade_state_t's content_intent.
 *  Thread-safe; no-op (returns 0) when the group is absent.
 *  @return 1 if content_intent was modified, 0 if not, -1 on bad args. */
int brain_wired_language_apply_cascade(struct brain_struct* brain, void* production_cascade_state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_COGNITIVE_WIRED_H */
