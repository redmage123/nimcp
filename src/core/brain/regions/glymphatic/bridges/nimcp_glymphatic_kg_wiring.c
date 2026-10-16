//=============================================================================
// nimcp_glymphatic_kg_wiring.c - Glymphatic KG Wiring (W4)
//=============================================================================

#include "core/brain/regions/glymphatic/bridges/nimcp_glymphatic_kg_wiring.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(glymphatic_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

static int gly_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t gly_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc
) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_glymphatic_kg_wiring_init(brain_t brain) {
    if (!gly_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = gly_ensure_node(
        kg, GLYMPHATIC_KG_ROOT_NAME, BRAIN_KG_NODE_COGNITIVE,
        "Glymphatic system - CSF-ISF waste clearance, sleep-coupled");

    struct { const char* name; const char* desc; } parts[] = {
        { "aquaporin_4",        "AQP4 water channel on astrocyte end-feet" },
        { "perivascular_space", "Perivascular CSF inflow pathway" },
        { "csf_isf_exchange",   "CSF-ISF bulk-flow exchange, sleep-enhanced" },
    };

    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = gly_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_COGNITIVE, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains glymphatic component", 1.0f);
        }
    }

    /* Pipeline: aquaporin_4 -> csf_isf_exchange <- perivascular_space */
    brain_kg_node_id_t aqp4  = brain_kg_find_node(kg, "aquaporin_4");
    brain_kg_node_id_t peri  = brain_kg_find_node(kg, "perivascular_space");
    brain_kg_node_id_t exch  = brain_kg_find_node(kg, "csf_isf_exchange");
    if (aqp4 != BRAIN_KG_INVALID_NODE && exch != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, aqp4, exch, BRAIN_KG_EDGE_PROVIDES_TO,
                          "AQP4 enables exchange", 0.9f);
    }
    if (peri != BRAIN_KG_INVALID_NODE && exch != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, peri, exch, BRAIN_KG_EDGE_SENDS_TO,
                          "perivascular feeds exchange", 0.9f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    NIMCP_LOG_INFO(GLYMPHATIC_KG_MODULE_NAME,
        "Glymphatic structural nodes wired into internal KG");
    glymphatic_kg_wiring_heartbeat("wiring_init", 1.0f);
    return 0;
}

void glymphatic_emit_clearance(brain_t brain, float volume) {
    if (!gly_is_usable(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;
    uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;

    char name[128];
    snprintf(name, sizeof(name),
             "glymphatic_event_clearance_%llu", (unsigned long long)ts_us);
    char desc[192];
    snprintf(desc, sizeof(desc),
             "glymphatic clearance: volume=%.3f", volume);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);
    brain_kg_node_id_t ev = brain_kg_add_node(kg, name,
        BRAIN_KG_NODE_COGNITIVE, desc);
    brain_kg_node_id_t root = brain_kg_find_node(kg, GLYMPHATIC_KG_ROOT_NAME);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "produced_by", volume);
    }
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

__attribute__((unused))
static void glymphatic_kg_suppress_unused(void) {
    (void)glymphatic_kg_wiring_set_health_agent;
    (void)glymphatic_kg_wiring_heartbeat;
}
