//=============================================================================
// nimcp_perirhinal_kg_wiring.c - Perirhinal KG Wiring (W3)
//=============================================================================
/**
 * Registers perirhinal cortex (Brodmann 35/36) nodes + runtime familiarity
 * event emitter. Admin-token self-elevation (kg-node-naming-registry §7).
 */

#include "core/brain/regions/perirhinal/bridges/nimcp_perirhinal_kg_wiring.h"

#include <stdio.h>
#include <string.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(perirhinal_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

static int prc_kg_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t prc_kg_ensure_node(brain_kg_t* kg, const char* name,
                                             brain_kg_node_type_t type,
                                             const char* desc) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_perirhinal_kg_wiring_init(brain_t brain) {
    if (!prc_kg_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = prc_kg_ensure_node(
        kg, PERIRHINAL_KG_ROOT_NAME, BRAIN_KG_NODE_CORTICAL,
        "Perirhinal cortex - item familiarity, object/item recognition memory");

    struct { const char* name; const char* desc; } parts[] = {
        { "perirhinal_area35", "PRC area 35 - item familiarity signaling" },
        { "perirhinal_area36", "PRC area 36 - object-context associations" },
    };
    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = prc_kg_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_CORTICAL, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains PRC sub-area", 1.0f);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
    NIMCP_LOG_INFO(PERIRHINAL_KG_MODULE_NAME,
                   "perirhinal KG structural wiring registered");
    return 0;
}

void nimcp_perirhinal_kg_emit_event(brain_t brain, const char* kind,
                                    float intensity, uint64_t ts_us) {
    if (!prc_kg_is_usable(brain) || !kind) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[256];
    snprintf(ev_name, sizeof(ev_name),
             "perirhinal_event_%s_%llu", kind, (unsigned long long)ts_us);

    char desc[256];
    snprintf(desc, sizeof(desc),
             "Perirhinal familiarity event: kind=%s intensity=%.3f",
             kind, intensity);

    brain_kg_node_id_t ev = brain_kg_add_node(
        kg, ev_name, BRAIN_KG_NODE_CORTICAL, desc);

    brain_kg_node_id_t owner = brain_kg_find_node(kg, PERIRHINAL_KG_ROOT_NAME);
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "familiarity", intensity);
    }

    if (ev != BRAIN_KG_INVALID_NODE) {
        char ibuf[32];
        snprintf(ibuf, sizeof(ibuf), "%.4f", intensity);
        brain_kg_add_metadata(kg, ev, "intensity", ibuf);
        snprintf(ibuf, sizeof(ibuf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", ibuf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

void perirhinal_event_familiarity(brain_t brain, const char* kind,
                                  float intensity, uint64_t ts_us) {
    nimcp_perirhinal_kg_emit_event(brain, kind, intensity, ts_us);
}
