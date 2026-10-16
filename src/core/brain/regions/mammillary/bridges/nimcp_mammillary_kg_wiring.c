//=============================================================================
// nimcp_mammillary_kg_wiring.c - Mammillary Bodies KG Wiring (W4)
//=============================================================================

#include "core/brain/regions/mammillary/bridges/nimcp_mammillary_kg_wiring.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(mammillary_kg_wiring, MESH_ADAPTER_CATEGORY_SUBCORTICAL)

static int mam_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t mam_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc
) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_mammillary_kg_wiring_init(brain_t brain) {
    if (!mam_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = mam_ensure_node(
        kg, MAMMILLARY_KG_ROOT_NAME, BRAIN_KG_NODE_SUBCORTICAL,
        "Mammillary bodies - Papez circuit relay (hippocampus -> anterior thalamus)");

    struct { const char* name; const char* desc; } parts[] = {
        { "medial_mammillary_nucleus",  "Medial mammillary nucleus - fornix input" },
        { "lateral_mammillary_nucleus", "Lateral mammillary nucleus - head direction" },
        { "mammillothalamic_tract",     "Mammillothalamic tract -> anterior thalamus" },
    };

    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = mam_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_SUBCORTICAL, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains mammillary component", 1.0f);
        }
    }

    /* Papez circuit linkage: hippocampus -> mammillary (via fornix),
     * mammillary -> anterior thalamus (via MTT). */
    brain_kg_node_id_t hippo = brain_kg_find_node(kg, "hippocampus");
    brain_kg_node_id_t mtt   = brain_kg_find_node(kg, "mammillothalamic_tract");
    brain_kg_node_id_t thal  = brain_kg_find_node(kg, "thalamus");
    if (hippo != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, hippo, root, BRAIN_KG_EDGE_SENDS_TO,
                          "hippocampus -> mammillary via fornix", 0.9f);
    }
    if (mtt != BRAIN_KG_INVALID_NODE && thal != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, mtt, thal, BRAIN_KG_EDGE_SENDS_TO,
                          "MTT -> anterior thalamus", 0.9f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    NIMCP_LOG_INFO(MAMMILLARY_KG_MODULE_NAME,
        "Mammillary structural nodes wired into internal KG");
    mammillary_kg_wiring_heartbeat("wiring_init", 1.0f);
    return 0;
}

void mammillary_emit_relay(brain_t brain, float signal_magnitude) {
    if (!mam_is_usable(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;
    uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;

    char name[128];
    snprintf(name, sizeof(name),
             "mammillary_event_relay_%llu", (unsigned long long)ts_us);
    char desc[192];
    snprintf(desc, sizeof(desc),
             "mammillary Papez relay: magnitude=%.3f", signal_magnitude);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);
    brain_kg_node_id_t ev = brain_kg_add_node(kg, name,
        BRAIN_KG_NODE_SUBCORTICAL, desc);
    brain_kg_node_id_t root = brain_kg_find_node(kg, MAMMILLARY_KG_ROOT_NAME);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "produced_by", signal_magnitude);
    }
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

__attribute__((unused))
static void mammillary_kg_suppress_unused(void) {
    (void)mammillary_kg_wiring_set_health_agent;
    (void)mammillary_kg_wiring_heartbeat;
}
