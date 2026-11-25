/**
 * @file nimcp_language_consciousness_bridge.c
 * @brief Global workspace (conscious access) <-> language.
 *
 * WHAT: Global-workspace broadcast strength scales the reportability of the
 *       content about to be verbalised.
 * WHY:  Content becomes reportable when it ignites the global workspace
 *       (Dehaene & Changeux 2011): strong broadcast -> confident report,
 *       weak broadcast -> hedged report.
 * HOW:  brain->global_workspace: global_workspace_get_broadcast_strength
 *       ([0,1], O(1), thread-safe). intent_gain = 0.9 + 0.2*s;
 *       floor_adjust = -0.05*(2s - 1).
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_consciousness.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

typedef struct { int unused; } lang_gw_state_t;

static void* gw_create(struct brain_struct* brain) {
    if (!brain || !brain->global_workspace) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_gw_state_t));
}

static void gw_destroy(void* st) { if (st) { nimcp_free(st); } }

static int gw_update(void* st, struct brain_struct* brain,
                     const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->global_workspace || !in) { return -1; }
    float s = lang_cog_clampf(lang_cog_finite_or(
                  global_workspace_get_broadcast_strength(brain->global_workspace), 0.0f),
                  0.0f, 1.0f);
    out->intent_gain  = 0.9f + 0.2f * s;
    out->floor_adjust = -0.05f * (2.0f * s - 1.0f);
    out->primary = s;
    out->valid   = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_consciousness_bridge_vt = {
    .name = "consciousness", .gl_bit = GL_BRIDGE_CONSCIOUSNESS,
    .create = gw_create, .destroy = gw_destroy, .update = gw_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
