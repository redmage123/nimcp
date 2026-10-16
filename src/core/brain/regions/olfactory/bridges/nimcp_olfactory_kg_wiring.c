//=============================================================================
// nimcp_olfactory_kg_wiring.c - Olfactory KG Wiring (W3)
//=============================================================================
/**
 * Registers olfactory bulb + piriform cortex nodes and runtime odor-detection
 * event emitter. Admin-token self-elevation (kg-node-naming-registry §7).
 */

#include "core/brain/regions/olfactory/bridges/nimcp_olfactory_kg_wiring.h"

#include <stdio.h>
#include <string.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(olfactory_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

static int olf_kg_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t olf_kg_ensure_node(brain_kg_t* kg, const char* name,
                                             brain_kg_node_type_t type,
                                             const char* desc) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_olfactory_kg_wiring_init(brain_t brain) {
    if (!olf_kg_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = olf_kg_ensure_node(
        kg, OLFACTORY_KG_ROOT_NAME, BRAIN_KG_NODE_SUBCORTICAL,
        "Olfactory system - odor detection, hedonic odor processing");

    struct { const char* name; const char* desc; } parts[] = {
        { "olfactory_bulb",    "Olfactory bulb - glomerular odor map, mitral/tufted output" },
        { "olfactory_piriform","Piriform cortex - odor identification, pattern completion" },
    };
    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = olf_kg_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_PERCEPTION, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains olfactory stage", 1.0f);
        }
    }

    brain_kg_node_id_t ob = brain_kg_find_node(kg, "olfactory_bulb");
    brain_kg_node_id_t pc = brain_kg_find_node(kg, "olfactory_piriform");
    if (ob != BRAIN_KG_INVALID_NODE && pc != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ob, pc, BRAIN_KG_EDGE_SENDS_TO,
                          "bulb -> piriform", 0.9f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
    NIMCP_LOG_INFO(OLFACTORY_KG_MODULE_NAME,
                   "olfactory KG structural wiring registered");
    return 0;
}

void nimcp_olfactory_kg_emit_event(brain_t brain, const char* kind,
                                   float intensity, uint64_t ts_us) {
    if (!olf_kg_is_usable(brain) || !kind) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[256];
    snprintf(ev_name, sizeof(ev_name),
             "olfactory_event_%s_%llu", kind, (unsigned long long)ts_us);

    char desc[256];
    snprintf(desc, sizeof(desc),
             "Olfactory odor-detection event: kind=%s intensity=%.3f",
             kind, intensity);

    brain_kg_node_id_t ev = brain_kg_add_node(
        kg, ev_name, BRAIN_KG_NODE_PERCEPTION, desc);

    brain_kg_node_id_t owner = brain_kg_find_node(kg, OLFACTORY_KG_ROOT_NAME);
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "odor_detected", intensity);
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

void olfactory_event_odor_detected(brain_t brain, const char* kind,
                                   float intensity, uint64_t ts_us) {
    nimcp_olfactory_kg_emit_event(brain, kind, intensity, ts_us);
}
