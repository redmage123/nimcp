//=============================================================================
// nimcp_parahippocampal_kg_wiring.c - Parahippocampal KG Wiring (W3)
//=============================================================================
/**
 * Registers parahippocampal cortex (posterior/anterior PHC) nodes + runtime
 * context-event emitter. Admin-token self-elevation (kg-node-naming-registry §7).
 */

#include "core/brain/regions/parahippocampal/bridges/nimcp_parahippocampal_kg_wiring.h"

#include <stdio.h>
#include <string.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(parahippocampal_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

static int phc_kg_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t phc_kg_ensure_node(brain_kg_t* kg, const char* name,
                                             brain_kg_node_type_t type,
                                             const char* desc) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_parahippocampal_kg_wiring_init(brain_t brain) {
    if (!phc_kg_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = phc_kg_ensure_node(
        kg, PARAHIPPOCAMPAL_KG_ROOT_NAME, BRAIN_KG_NODE_CORTICAL,
        "Parahippocampal cortex - contextual + spatial memory gateway, MTL");

    struct { const char* name; const char* desc; } parts[] = {
        { "parahippocampal_posterior", "Posterior parahippocampal - scene recognition, spatial context" },
        { "parahippocampal_anterior",  "Anterior parahippocampal - non-spatial context, associations" },
    };
    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = phc_kg_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_CORTICAL, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains PHC sub-region", 1.0f);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
    NIMCP_LOG_INFO(PARAHIPPOCAMPAL_KG_MODULE_NAME,
                   "parahippocampal KG structural wiring registered");
    return 0;
}

void nimcp_parahippocampal_kg_emit_event(brain_t brain, const char* kind,
                                         float intensity, uint64_t ts_us) {
    if (!phc_kg_is_usable(brain) || !kind) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[256];
    snprintf(ev_name, sizeof(ev_name),
             "parahippocampal_event_%s_%llu", kind, (unsigned long long)ts_us);

    char desc[256];
    snprintf(desc, sizeof(desc),
             "Parahippocampal context event: kind=%s intensity=%.3f",
             kind, intensity);

    brain_kg_node_id_t ev = brain_kg_add_node(
        kg, ev_name, BRAIN_KG_NODE_CORTICAL, desc);

    brain_kg_node_id_t owner = brain_kg_find_node(kg, PARAHIPPOCAMPAL_KG_ROOT_NAME);
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "context_event", intensity);
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

void parahippocampal_event_context(brain_t brain, const char* kind,
                                   float intensity, uint64_t ts_us) {
    nimcp_parahippocampal_kg_emit_event(brain, kind, intensity, ts_us);
}
