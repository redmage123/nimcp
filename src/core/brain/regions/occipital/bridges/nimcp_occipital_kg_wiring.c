//=============================================================================
// nimcp_occipital_kg_wiring.c - Occipital KG Wiring (W3)
//=============================================================================
/**
 * Registers occipital visual-hierarchy structural nodes (V1/V2/V4/IT) at
 * brain init and provides a runtime event emitter for feature-detection
 * events (edges, textures, objects, faces). Admin-token self-elevation
 * pattern — see docs/claude/kg-node-naming-registry.md §7.
 */

#include "core/brain/regions/occipital/bridges/nimcp_occipital_kg_wiring.h"

#include <stdio.h>
#include <string.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

static int occ_kg_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t occ_kg_ensure_node(brain_kg_t* kg, const char* name,
                                             brain_kg_node_type_t type,
                                             const char* desc) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

int nimcp_occipital_kg_wiring_init(brain_t brain) {
    if (!occ_kg_is_usable(brain)) return 0;  /* null-tolerant */

    brain_kg_t* kg = brain->internal_kg;

    /* Elevate to ADMIN for structural writes (see kg-node-naming-registry §7). */
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = occ_kg_ensure_node(
        kg, OCCIPITAL_KG_ROOT_NAME, BRAIN_KG_NODE_CORTICAL,
        "Occipital cortex - primary visual processing, object recognition");

    struct { const char* name; const char* desc; } parts[] = {
        { "occipital_v1", "V1 primary visual cortex - orientation, edges, retinotopy" },
        { "occipital_v2", "V2 secondary visual cortex - contours, illusory boundaries" },
        { "occipital_v4", "V4 extrastriate cortex - color, shape, intermediate features" },
        { "occipital_it", "Inferior temporal / IT cortex - object + face recognition" },
    };

    const size_t n = sizeof(parts) / sizeof(parts[0]);
    for (size_t i = 0; i < n; ++i) {
        brain_kg_node_id_t child = occ_kg_ensure_node(
            kg, parts[i].name, BRAIN_KG_NODE_PERCEPTION, parts[i].desc);
        if (root != BRAIN_KG_INVALID_NODE && child != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, child, BRAIN_KG_EDGE_CONNECTS_TO,
                              "contains visual sub-region", 1.0f);
        }
    }

    /* Intra-hierarchy forward edges V1 -> V2 -> V4 -> IT. */
    const char* chain[] = { "occipital_v1", "occipital_v2",
                            "occipital_v4", "occipital_it" };
    for (size_t i = 0; i + 1 < sizeof(chain) / sizeof(chain[0]); ++i) {
        brain_kg_node_id_t a = brain_kg_find_node(kg, chain[i]);
        brain_kg_node_id_t b = brain_kg_find_node(kg, chain[i + 1]);
        if (a != BRAIN_KG_INVALID_NODE && b != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_SENDS_TO,
                              "feedforward ventral stream", 0.9f);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
    NIMCP_LOG_INFO(OCCIPITAL_KG_MODULE_NAME, "occipital KG structural wiring registered");
    return 0;
}

void nimcp_occipital_kg_emit_event(brain_t brain, const char* kind,
                                   float intensity, uint64_t ts_us) {
    if (!occ_kg_is_usable(brain) || !kind) return;

    brain_kg_t* kg = brain->internal_kg;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[256];
    snprintf(ev_name, sizeof(ev_name),
             "occipital_event_%s_%llu", kind, (unsigned long long)ts_us);

    char desc[256];
    snprintf(desc, sizeof(desc),
             "Occipital feature-detection event: kind=%s intensity=%.3f",
             kind, intensity);

    brain_kg_node_id_t ev = brain_kg_add_node(
        kg, ev_name, BRAIN_KG_NODE_PERCEPTION, desc);

    brain_kg_node_id_t owner = brain_kg_find_node(kg, OCCIPITAL_KG_ROOT_NAME);
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
                          "feature_detected", intensity);
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

/* Back-compat convenience alias in the shape the spec asked for. */
void occipital_event_feature_detected(brain_t brain, const char* kind,
                                      float intensity, uint64_t ts_us) {
    nimcp_occipital_kg_emit_event(brain, kind, intensity, ts_us);
}
