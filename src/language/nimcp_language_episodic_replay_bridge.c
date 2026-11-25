/**
 * @file nimcp_language_episodic_replay_bridge.c
 * @brief Episodic replay buffer <-> language (telemetry only).
 *
 * WHAT: Reports the episodic replay buffer occupancy (pending consolidation
 *       pressure) next to the language state.
 * WHY:  The 2026-10-21 version blended constant "replay activity" numbers
 *       into content_intent. The replay module exposes no per-episode read
 *       API (only record / consolidate / buffer size), and its replay path
 *       feeds stored features straight into brain_learn_vector, so writing
 *       language vectors into it would inject wrong-dimension training
 *       examples. With no honest language-facing signal available, this
 *       bridge is deliberately neutral: it emits telemetry, no modulation.
 * HOW:  brain->episodic_replay: nimcp_episodic_replay_get_buffer_size /
 *       EPISODIC_REPLAY_DEFAULT_CAPACITY.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/memory/nimcp_episodic_replay.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

typedef struct { int unused; } lang_er_state_t;

static void* er_create(struct brain_struct* brain) {
    if (!brain || !brain->episodic_replay) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_er_state_t));
}

static void er_destroy(void* st) { if (st) { nimcp_free(st); } }

static int er_update(void* st, struct brain_struct* brain,
                     const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->episodic_replay || !in) { return -1; }
    uint32_t n = nimcp_episodic_replay_get_buffer_size(
                     (const nimcp_episodic_replay_t*)brain->episodic_replay);
    out->primary   = lang_cog_clampf((float)n / (float)EPISODIC_REPLAY_DEFAULT_CAPACITY, 0.0f, 1.0f);
    out->secondary = (float)n;
    out->valid     = true;   /* neutral floor/gain: telemetry only */
    return 0;
}

const lang_cog_bridge_vt_t lang_episodic_replay_bridge_vt = {
    .name = "episodic_replay", .gl_bit = GL_BRIDGE_EPISODIC_REPLAY,
    .create = er_create, .destroy = er_destroy, .update = er_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
