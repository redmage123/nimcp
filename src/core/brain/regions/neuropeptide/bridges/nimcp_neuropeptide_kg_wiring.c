//=============================================================================
// nimcp_neuropeptide_kg_wiring.c - Neuropeptide KG Wiring (W4)
//=============================================================================

#include "core/brain/regions/neuropeptide/bridges/nimcp_neuropeptide_kg_wiring.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(neuropeptide_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

static int np_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t np_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc
) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_neuropeptide_kg_wiring_init(brain_t brain) {
    if (!np_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = np_ensure_node(
        kg, NEUROPEPTIDE_KG_ROOT_NAME, BRAIN_KG_NODE_COGNITIVE,
        "Neuropeptide system - slow-acting modulatory signalling peptides");

    struct { const char* name; const char* desc; } parts[] = {
        { "oxytocin",       "Oxytocin - social bonding, trust, maternal behaviour" },
        { "vasopressin",    "Vasopressin (AVP) - social memory, territorial behaviour" },
        { "crh",            "Corticotropin-releasing hormone - stress axis driver" },
        { "substance_p",    "Substance P - nociception, stress response" },
        { "orexin",         "Orexin/hypocretin - arousal, wakefulness, motivation" },
        { "opioid_peptide", "Endogenous opioids - analgesia, reward, euphoria" },
    };

    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = np_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_COGNITIVE, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains peptide", 1.0f);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    NIMCP_LOG_INFO(NEUROPEPTIDE_KG_MODULE_NAME,
        "Neuropeptide structural nodes wired into internal KG");
    neuropeptide_kg_wiring_heartbeat("wiring_init", 1.0f);
    return 0;
}

void neuropeptide_emit_release(
    brain_t brain,
    const char* peptide_name,
    float concentration
) {
    if (!np_is_usable(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;
    const char* pep = (peptide_name && *peptide_name) ? peptide_name : "unknown";
    uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;

    char name[160];
    snprintf(name, sizeof(name),
             "neuropeptide_event_release_%llu", (unsigned long long)ts_us);
    char desc[200];
    snprintf(desc, sizeof(desc),
             "neuropeptide release: peptide=%s conc=%.3f", pep, concentration);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, name,
        BRAIN_KG_NODE_COGNITIVE, desc);
    brain_kg_node_id_t root = brain_kg_find_node(kg, NEUROPEPTIDE_KG_ROOT_NAME);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "produced_by", concentration);
    }
    brain_kg_node_id_t pep_node = brain_kg_find_node(kg, pep);
    if (pep_node != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, pep_node, ev, BRAIN_KG_EDGE_PROVIDES_TO,
                          "released peptide", concentration);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

__attribute__((unused))
static void neuropeptide_kg_suppress_unused(void) {
    (void)neuropeptide_kg_wiring_set_health_agent;
    (void)neuropeptide_kg_wiring_heartbeat;
}
