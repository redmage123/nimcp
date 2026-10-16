//=============================================================================
// nimcp_sensory_integration_kg_wiring.c - Sensory Integration KG Wiring (W4)
//=============================================================================

#include "core/brain/regions/sensory_integration/bridges/nimcp_sensory_integration_kg_wiring.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(sensory_integration_kg_wiring,
                             MESH_ADAPTER_CATEGORY_PERCEPTION)

static int si_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t si_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc
) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

int nimcp_sensory_integration_kg_wiring_init(brain_t brain) {
    if (!si_is_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = si_ensure_node(
        kg, SENSORY_INTEGRATION_KG_ROOT_NAME, BRAIN_KG_NODE_PERCEPTION,
        "Sensory integration - cross-modal binding, multisensory convergence");

    struct { const char* name; const char* desc; } parts[] = {
        { "crossmodal_binding",        "Cross-modal binding - linking concurrent modalities" },
        { "multisensory_convergence",  "Multisensory convergence - superadditive response" },
        { "temporal_binding",          "Temporal binding - coincidence detection" },
    };

    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = si_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_PERCEPTION, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains sensory integration component", 1.0f);
        }
    }

    /* Binding pipeline: temporal -> crossmodal -> convergence */
    brain_kg_node_id_t temp  = brain_kg_find_node(kg, "temporal_binding");
    brain_kg_node_id_t cross = brain_kg_find_node(kg, "crossmodal_binding");
    brain_kg_node_id_t conv  = brain_kg_find_node(kg, "multisensory_convergence");
    if (temp != BRAIN_KG_INVALID_NODE && cross != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, temp, cross, BRAIN_KG_EDGE_PROVIDES_TO,
                          "temporal coincidence enables crossmodal binding", 0.85f);
    }
    if (cross != BRAIN_KG_INVALID_NODE && conv != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, cross, conv, BRAIN_KG_EDGE_SENDS_TO,
                          "binding drives convergent response", 0.85f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    NIMCP_LOG_INFO(SENSORY_INTEGRATION_KG_MODULE_NAME,
        "Sensory integration structural nodes wired into internal KG");
    sensory_integration_kg_wiring_heartbeat("wiring_init", 1.0f);
    return 0;
}

void sensory_integration_emit_crossmodal_bind(
    brain_t brain,
    const char* modality_a,
    const char* modality_b,
    float strength
) {
    if (!si_is_usable(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;
    const char* ma = (modality_a && *modality_a) ? modality_a : "?";
    const char* mb = (modality_b && *modality_b) ? modality_b : "?";
    uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;

    char name[160];
    snprintf(name, sizeof(name),
             "sensory_integration_event_crossmodal_bind_%llu",
             (unsigned long long)ts_us);
    char desc[200];
    snprintf(desc, sizeof(desc),
             "crossmodal bind: %s <-> %s strength=%.3f", ma, mb, strength);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, name,
        BRAIN_KG_NODE_PERCEPTION, desc);
    brain_kg_node_id_t root = brain_kg_find_node(kg,
        SENSORY_INTEGRATION_KG_ROOT_NAME);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "produced_by", strength);
    }
    brain_kg_node_id_t anchor = brain_kg_find_node(kg, "crossmodal_binding");
    if (anchor != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, anchor, ev, BRAIN_KG_EDGE_PROVIDES_TO,
                          "bound event instance", strength);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

__attribute__((unused))
static void sensory_integration_kg_suppress_unused(void) {
    (void)sensory_integration_kg_wiring_set_health_agent;
    (void)sensory_integration_kg_wiring_heartbeat;
}
