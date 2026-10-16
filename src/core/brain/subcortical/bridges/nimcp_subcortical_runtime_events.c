//=============================================================================
// nimcp_subcortical_runtime_events.c - Per-Subcortical-Module Runtime Events
//=============================================================================
/**
 * @file nimcp_subcortical_runtime_events.c
 * @brief Implementation of 8 subcortical runtime event emitters (W4).
 *
 * All functions are null-safe, self-elevate the KG access level using
 * `brain->internal_kg_admin_token`, write one event node + a back-edge to
 * the structural parent, then restore READ access.
 */

#include "core/brain/subcortical/bridges/nimcp_subcortical_runtime_events.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

BRIDGE_BOILERPLATE_MESH_ONLY(subcortical_runtime_events, MESH_ADAPTER_CATEGORY_SUBCORTICAL)

/* ------------------------------------------------------------------------- */
/* Common helper: emit one event node + back-edge to a named structural      */
/* parent. Null/enabled/token guards handled here so the 8 emit functions    */
/* stay small.                                                               */
/* ------------------------------------------------------------------------- */

static void emit_event_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float edge_weight
) {
    if (!brain) return;
    if (!brain->internal_kg_enabled) return;
    if (!brain->internal_kg) return;
    if (!event_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    /* Self-elevate for the writes */
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, event_name,
        BRAIN_KG_NODE_COGNITIVE,
        description ? description : "subcortical runtime event");

    if (ev != BRAIN_KG_INVALID_NODE && parent_name && *parent_name) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            /* produced_by: parent -> event, matches registry §5 */
            brain_kg_add_edge(kg, parent, ev,
                BRAIN_KG_EDGE_SENDS_TO, "produced_by", edge_weight);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Common timestamp in microseconds (resolution-limited by time()). */
static uint64_t ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/* ------------------------------------------------------------------------- */
/* 1. basal_ganglia / striatum: action selected                              */
/* ------------------------------------------------------------------------- */

void subcortical_emit_action_selected(
    struct brain_struct* brain,
    uint32_t action_id,
    float confidence
) {
    char name[160];
    snprintf(name, sizeof(name),
             "striatum_event_action_%u_%llu",
             action_id, (unsigned long long)ts_us());
    char desc[200];
    snprintf(desc, sizeof(desc),
             "BG action selected: id=%u confidence=%.3f",
             action_id, confidence);
    emit_event_linked(brain, name, desc, "basal_ganglia", confidence);
}

/* ------------------------------------------------------------------------- */
/* 2. globus_pallidus: inhibition event                                      */
/* ------------------------------------------------------------------------- */

void subcortical_emit_inhibition(
    struct brain_struct* brain,
    float strength
) {
    char name[128];
    snprintf(name, sizeof(name),
             "gp_event_inhibit_%llu", (unsigned long long)ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "globus pallidus inhibition: strength=%.3f", strength);
    /* Parent preference: globus_pallidus_internal (GPi) if present, else basal_ganglia */
    const char* parent = "globus_pallidus_internal";
    if (brain && brain->internal_kg) {
        brain_kg_node_id_t gp = brain_kg_find_node(brain->internal_kg,
            "globus_pallidus_internal");
        if (gp == BRAIN_KG_INVALID_NODE) parent = "basal_ganglia";
    }
    emit_event_linked(brain, name, desc, parent, strength);
}

/* ------------------------------------------------------------------------- */
/* 3. STN: stop signal                                                       */
/* ------------------------------------------------------------------------- */

void subcortical_emit_stop_signal(struct brain_struct* brain) {
    char name[128];
    snprintf(name, sizeof(name),
             "stn_event_stop_%llu", (unsigned long long)ts_us());
    const char* desc = "STN hyperdirect stop signal (global action suppression)";
    const char* parent = "subthalamic_nucleus";
    if (brain && brain->internal_kg) {
        if (brain_kg_find_node(brain->internal_kg, parent) == BRAIN_KG_INVALID_NODE) {
            parent = "basal_ganglia";
        }
    }
    emit_event_linked(brain, name, desc, parent, 1.0f);
}

/* ------------------------------------------------------------------------- */
/* 4. substantia_nigra: dopamine release (distinct from VTA, W2)             */
/* ------------------------------------------------------------------------- */

void subcortical_emit_dopamine_release(
    struct brain_struct* brain,
    float amount
) {
    char name[128];
    snprintf(name, sizeof(name),
             "sn_event_da_%llu", (unsigned long long)ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "substantia nigra dopamine release: amount=%.3f", amount);
    const char* parent = "substantia_nigra_compacta";
    if (brain && brain->internal_kg) {
        if (brain_kg_find_node(brain->internal_kg, parent) == BRAIN_KG_INVALID_NODE) {
            parent = "basal_ganglia";
        }
    }
    emit_event_linked(brain, name, desc, parent, amount);
}

/* ------------------------------------------------------------------------- */
/* 5. NAcc: reward prediction error                                          */
/* ------------------------------------------------------------------------- */

void subcortical_emit_reward_prediction(
    struct brain_struct* brain,
    float rpe
) {
    char name[128];
    snprintf(name, sizeof(name),
             "nacc_event_rpe_%llu", (unsigned long long)ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "NAcc reward prediction error: rpe=%.3f", rpe);
    emit_event_linked(brain, name, desc, "nucleus_accumbens", rpe);
}

/* ------------------------------------------------------------------------- */
/* 6. superior_colliculus: orienting response                                */
/* ------------------------------------------------------------------------- */

void subcortical_emit_orienting_response(
    struct brain_struct* brain,
    float x,
    float y
) {
    char name[128];
    snprintf(name, sizeof(name),
             "sc_event_orient_%llu", (unsigned long long)ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "SC orient response: x=%.3f y=%.3f", x, y);
    /* SC is not in the subcortical umbrella's structural set; link to the
     * brainstem root if present, else to basal_ganglia as a coarse parent. */
    const char* parent = "brainstem";
    if (brain && brain->internal_kg) {
        if (brain_kg_find_node(brain->internal_kg, parent) == BRAIN_KG_INVALID_NODE) {
            parent = "basal_ganglia";
        }
    }
    emit_event_linked(brain, name, desc, parent, 1.0f);
}

/* ------------------------------------------------------------------------- */
/* 7. inferior_colliculus: auditory localization                             */
/* ------------------------------------------------------------------------- */

void subcortical_emit_auditory_localization(
    struct brain_struct* brain,
    float azimuth,
    float elevation
) {
    char name[128];
    snprintf(name, sizeof(name),
             "ic_event_loc_%llu", (unsigned long long)ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "IC auditory loc: az=%.3f el=%.3f", azimuth, elevation);
    /* IC feeds MGN, whose structural peer is `mgn` via the subcortical
     * umbrella. Fall back to brainstem if MGN missing. */
    const char* parent = "mgn";
    if (brain && brain->internal_kg) {
        if (brain_kg_find_node(brain->internal_kg, parent) == BRAIN_KG_INVALID_NODE) {
            parent = "brainstem";
            if (brain_kg_find_node(brain->internal_kg, parent) == BRAIN_KG_INVALID_NODE) {
                parent = "thalamus";
            }
        }
    }
    emit_event_linked(brain, name, desc, parent, 1.0f);
}

/* ------------------------------------------------------------------------- */
/* 8. thalamus (subcortical, distinct from thalamic_router): routing event   */
/* ------------------------------------------------------------------------- */

void subcortical_emit_routing_decision(
    struct brain_struct* brain,
    uint32_t src_idx,
    uint32_t dst_idx
) {
    char name[160];
    snprintf(name, sizeof(name),
             "thalamus_event_route_%llu", (unsigned long long)ts_us());
    char desc[200];
    snprintf(desc, sizeof(desc),
             "thalamic routing: src=%u dst=%u", src_idx, dst_idx);
    emit_event_linked(brain, name, desc, "thalamus", 1.0f);
}

/* Suppress unused health helpers warning (BRIDGE_BOILERPLATE emits them). */
__attribute__((unused))
static void subcortical_runtime_events_suppress_unused(void) {
    (void)subcortical_runtime_events_set_health_agent;
    (void)subcortical_runtime_events_heartbeat;
}
