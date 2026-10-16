//=============================================================================
// nimcp_endocannabinoid_kg_wiring.c - Endocannabinoid KG Wiring (W4)
//=============================================================================
/**
 * Registers endocannabinoid structural nodes (ligands + CB1/CB2 receptors)
 * at brain init and provides a runtime event emitter for retrograde
 * signalling events. Admin-token self-elevation pattern — see
 * docs/claude/kg-node-naming-registry.md §7.
 */

#include "core/brain/regions/endocannabinoid/bridges/nimcp_endocannabinoid_kg_wiring.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(endocannabinoid_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

static int ecb_kg_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t ecb_kg_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc
) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

int nimcp_endocannabinoid_kg_wiring_init(brain_t brain) {
    if (!ecb_kg_is_usable(brain)) return 0;  /* null-tolerant */

    brain_kg_t* kg = brain->internal_kg;

    /* Elevate to ADMIN for structural writes (see kg-node-naming-registry §7). */
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = ecb_kg_ensure_node(
        kg, ENDOCANNABINOID_KG_ROOT_NAME, BRAIN_KG_NODE_COGNITIVE,
        "Endocannabinoid system - retrograde messenger, CB1/CB2 signalling");

    struct { const char* name; const char* desc; } parts[] = {
        { "anandamide",   "Anandamide (AEA) - arachidonoylethanolamine retrograde messenger" },
        { "2_ag",         "2-arachidonoylglycerol (2-AG) - primary retrograde eCB" },
        { "cb1_receptor", "CB1 - presynaptic cannabinoid receptor, dominant in CNS" },
        { "cb2_receptor", "CB2 - peripheral/microglia cannabinoid receptor" },
    };

    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = ecb_kg_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_COGNITIVE, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains eCB component", 1.0f);
        }
    }

    /* Ligand -> Receptor binding edges. */
    brain_kg_node_id_t aea   = brain_kg_find_node(kg, "anandamide");
    brain_kg_node_id_t ag    = brain_kg_find_node(kg, "2_ag");
    brain_kg_node_id_t cb1   = brain_kg_find_node(kg, "cb1_receptor");
    brain_kg_node_id_t cb2   = brain_kg_find_node(kg, "cb2_receptor");
    if (aea != BRAIN_KG_INVALID_NODE && cb1 != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, aea, cb1, BRAIN_KG_EDGE_MODULATES,
                          "anandamide binds CB1", 0.9f);
    }
    if (ag != BRAIN_KG_INVALID_NODE && cb1 != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ag, cb1, BRAIN_KG_EDGE_MODULATES,
                          "2-AG binds CB1", 0.95f);
    }
    if (ag != BRAIN_KG_INVALID_NODE && cb2 != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ag, cb2, BRAIN_KG_EDGE_MODULATES,
                          "2-AG binds CB2", 0.6f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    NIMCP_LOG_INFO(ENDOCANNABINOID_KG_MODULE_NAME,
        "eCB structural nodes wired into internal KG");
    endocannabinoid_kg_wiring_heartbeat("wiring_init", 1.0f);
    return 0;
}

void endocannabinoid_emit_retrograde_signal(
    brain_t brain,
    const char* ligand,
    float strength
) {
    if (!ecb_kg_is_usable(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;
    const char* lig_str = (ligand && *ligand) ? ligand : "eCB";

    uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;

    char name[128];
    snprintf(name, sizeof(name),
             "ecb_event_retrograde_signal_%llu", (unsigned long long)ts_us);

    char desc[192];
    snprintf(desc, sizeof(desc),
             "eCB retrograde signal: ligand=%s strength=%.3f", lig_str, strength);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, name,
        BRAIN_KG_NODE_COGNITIVE, desc);
    brain_kg_node_id_t root = brain_kg_find_node(kg, ENDOCANNABINOID_KG_ROOT_NAME);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "produced_by", strength);
    }
    /* Link to specific ligand node if present */
    brain_kg_node_id_t lig = brain_kg_find_node(kg, lig_str);
    if (lig != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, lig, ev, BRAIN_KG_EDGE_PROVIDES_TO,
                          "released ligand", strength);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Suppress unused warnings for bridge boilerplate health helpers */
__attribute__((unused))
static void endocannabinoid_kg_suppress_unused(void) {
    (void)endocannabinoid_kg_wiring_set_health_agent;
    (void)endocannabinoid_kg_wiring_heartbeat;
}
