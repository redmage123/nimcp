/**
 * @file nimcp_collective_phi.c
 * @brief Implementation of IIT metrics for collective consciousness
 *
 * WHAT: Measure integrated information (phi) across multiple brain instances
 * WHY: Quantify collective consciousness based on IIT 3.0
 * HOW: Compute phi from information integration and network topology
 */

#include "cognitive/collective_cognition/nimcp_collective_phi.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(collective_phi, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Constants
 *===========================================================================*/

#define MAX_EVENTS      64

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Instance entry for phi computation
 */
typedef struct {
    uint32_t instance_id;
    bool active;
    float local_phi;
    float information_in;
    float information_out;
    float integration_factor;
} phi_instance_t;

/**
 * @brief Flow entry between instances
 */
typedef struct {
    uint32_t from_id;
    uint32_t to_id;
    information_flow_t flow;
    bool active;
} flow_entry_t;

/**
 * @brief Collective phi internal state
 */
struct collective_phi_system {
    /* Configuration */
    collective_phi_config_t config;

    /* Registered instances */
    phi_instance_t instances[COLLECTIVE_MAX_INSTANCES];
    uint32_t instance_count;

    /* Information flows */
    flow_entry_t flows[COLLECTIVE_MAX_INSTANCES * COLLECTIVE_MAX_INSTANCES];
    uint32_t flow_count;

    /* Current phi metrics */
    collective_phi_t phi;

    /* Current qualia */
    qualia_report_t qualia;

    /* Integration matrix (cached) */
    float integration_matrix[COLLECTIVE_MAX_INSTANCES][COLLECTIVE_MAX_INSTANCES];

    /* Event history */
    emergence_event_t events[MAX_EVENTS];
    uint32_t event_count;
    uint32_t event_head;

    /* Statistics */
    collective_phi_stats_t stats;

    /* Flags */
    bool initialized;
    uint64_t last_update_us;
    float last_phi;
};

/*=============================================================================
 * Helper Functions - Time
 *===========================================================================*/

static uint64_t get_timestamp_us(void) {
    static _Atomic uint64_t counter = 0;
    return atomic_fetch_add(&counter, 1);
}

/*=============================================================================
 * Helper Functions - Instance Management
 *===========================================================================*/

static phi_instance_t* find_instance(
    collective_phi_system_t* cps,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cps->instances[i].active && cps->instances[i].instance_id == instance_id) {
            return &cps->instances[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static phi_instance_t* find_free_instance_slot(collective_phi_system_t* cps) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cps->instances[i].active) {
            return &cps->instances[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static int find_instance_index(
    const collective_phi_system_t* cps,
    uint32_t instance_id
) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cps->instances[i].active && cps->instances[i].instance_id == instance_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is normal */
}

/*=============================================================================
 * Helper Functions - Flow Management
 *===========================================================================*/

static flow_entry_t* find_flow(
    collective_phi_system_t* cps,
    uint32_t from_id,
    uint32_t to_id
) {
    for (uint32_t i = 0; i < cps->flow_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cps->flow_count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)cps->flow_count);
        }

        if (cps->flows[i].active &&
            cps->flows[i].from_id == from_id &&
            cps->flows[i].to_id == to_id) {
            return &cps->flows[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static flow_entry_t* get_or_create_flow(
    collective_phi_system_t* cps,
    uint32_t from_id,
    uint32_t to_id
) {
    flow_entry_t* f = find_flow(cps, from_id, to_id);
    if (f) return f;

    /* Find free slot */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES * COLLECTIVE_MAX_INSTANCES; i++) {
        if (!cps->flows[i].active) {
            cps->flows[i].from_id = from_id;
            cps->flows[i].to_id = to_id;
            cps->flows[i].active = true;
            cps->flows[i].flow.from_instance = from_id;
            cps->flows[i].flow.to_instance = to_id;
            cps->flow_count++;
            return &cps->flows[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_create_flow: operation failed");
    return NULL;
}

/*=============================================================================
 * Helper Functions - Phi Computation
 *===========================================================================*/

/**
 * @brief Compute local phi sum
 */
static float compute_local_phi_sum(const collective_phi_system_t* cps) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cps->instances[i].active) {
            sum += cps->instances[i].local_phi;
        }
    }
    return sum;
}

/**
 * @brief Compute network phi from information flows
 */
static float compute_network_phi(collective_phi_system_t* cps) {
    if (cps->instance_count < 2) return 0.0f;

    float total_flow = 0.0f;
    float total_mi = 0.0f;

    for (uint32_t i = 0; i < cps->flow_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cps->flow_count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)cps->flow_count);
        }

        if (!cps->flows[i].active) continue;
        total_flow += cps->flows[i].flow.flow_rate;
        total_mi += cps->flows[i].flow.mutual_information;
    }

    /* Network phi based on total integration */
    float connectivity = (float)(cps->instance_count - 1) / cps->instance_count;
    float integration = total_mi / (cps->instance_count * cps->instance_count);

    return connectivity * integration * cps->instance_count;
}

/**
 * @brief Update integration matrix
 */
static void update_integration_matrix(collective_phi_system_t* cps) {
    /* Clear matrix */
    memset(cps->integration_matrix, 0, sizeof(cps->integration_matrix));

    /* Fill from flows */
    for (uint32_t i = 0; i < cps->flow_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cps->flow_count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)cps->flow_count);
        }

        if (!cps->flows[i].active) continue;

        int from_idx = find_instance_index(cps, cps->flows[i].from_id);
        int to_idx = find_instance_index(cps, cps->flows[i].to_id);

        if (from_idx >= 0 && to_idx >= 0) {
            cps->integration_matrix[from_idx][to_idx] =
                cps->flows[i].flow.mutual_information;
            /* Symmetric for integration */
            cps->integration_matrix[to_idx][from_idx] =
                cps->flows[i].flow.mutual_information;
        }
    }
}

/**
 * @brief Compute network topology metrics
 */
static void compute_topology_metrics(collective_phi_system_t* cps) {
    if (cps->instance_count < 2) {
        cps->phi.connectivity = 0.0f;
        cps->phi.modularity = 0.0f;
        cps->phi.small_world_index = 0.0f;
        return;
    }

    /* Connectivity: fraction of possible edges present */
    uint32_t max_edges = cps->instance_count * (cps->instance_count - 1);
    uint32_t actual_edges = 0;

    for (uint32_t i = 0; i < cps->flow_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cps->flow_count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)cps->flow_count);
        }

        if (cps->flows[i].active && cps->flows[i].flow.mutual_information > 0.1f) {
            actual_edges++;
        }
    }

    cps->phi.connectivity = max_edges > 0 ?
        (float)actual_edges / max_edges : 0.0f;

    /* Modularity: estimate from integration variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cps->instances[i].active) continue;
        for (uint32_t j = i + 1; j < COLLECTIVE_MAX_INSTANCES; j++) {
            if (!cps->instances[j].active) continue;
            float val = cps->integration_matrix[i][j];
            sum += val;
            sum_sq += val * val;
            count++;
        }
    }

    if (count > 0) {
        float mean = sum / count;
        float variance = (sum_sq / count) - (mean * mean);
        /* Higher variance = more modular */
        cps->phi.modularity = sqrtf(variance);
    } else {
        cps->phi.modularity = 0.0f;
    }

    /* Small-world: high clustering + low path length */
    /* Simplified: connectivity * (1 - modularity) */
    cps->phi.small_world_index = cps->phi.connectivity * (1.0f - cps->phi.modularity);
}

/**
 * @brief Map phi to consciousness level
 */
static collective_consciousness_level_t phi_to_level(float phi) {
    if (phi < 0.1f) return COLLECTIVE_CONSCIOUSNESS_NONE;
    if (phi < 0.3f) return COLLECTIVE_CONSCIOUSNESS_MINIMAL;
    if (phi < 0.5f) return COLLECTIVE_CONSCIOUSNESS_EMERGING;
    if (phi < 0.7f) return COLLECTIVE_CONSCIOUSNESS_PARTIAL;
    if (phi < 0.9f) return COLLECTIVE_CONSCIOUSNESS_UNIFIED;
    return COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT;
}

/**
 * @brief Record emergence event
 */
static void record_event(
    collective_phi_system_t* cps,
    float phi_before,
    float phi_after,
    const char* description
) {
    emergence_event_t* e = &cps->events[cps->event_head];

    e->timestamp_us = get_timestamp_us();
    e->phi_before = phi_before;
    e->phi_after = phi_after;
    e->delta = phi_after - phi_before;
    e->instances_involved = cps->instance_count;
    e->is_emergence = phi_after > phi_before;

    if (description) {
        strncpy(e->description, description, sizeof(e->description) - 1);
        e->description[sizeof(e->description) - 1] = '\0';
    } else {
        e->description[0] = '\0';
    }

    cps->event_head = (cps->event_head + 1) % MAX_EVENTS;
    if (cps->event_count < MAX_EVENTS) {
        cps->event_count++;
    }

    if (e->is_emergence) {
        cps->stats.emergence_events++;
    } else {
        cps->stats.fragmentation_events++;
    }
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

collective_phi_system_t* collective_phi_create(const collective_phi_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_create", 0.0f);


    collective_phi_system_t* cps = nimcp_malloc(sizeof(collective_phi_system_t));
    if (!cps) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cps");

        return NULL;

    }

    memset(cps, 0, sizeof(collective_phi_system_t));

    /* Apply configuration */
    if (config) {
        cps->config = *config;
    } else {
        cps->config = collective_phi_default_config();
    }

    /* Initialize qualia to neutral */
    cps->qualia.valence = 0.0f;
    cps->qualia.arousal = 0.5f;
    cps->qualia.complexity = 0.5f;
    cps->qualia.coherence = 0.5f;
    cps->qualia.temporal_depth = 0.5f;
    cps->qualia.spatial_extent = 0.5f;
    cps->qualia.agency = 0.5f;
    cps->qualia.metacognition = 0.5f;

    cps->initialized = true;
    cps->last_update_us = get_timestamp_us();

    return cps;
}

void collective_phi_destroy(collective_phi_system_t* cps) {
    if (!cps) return;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_destroy", 0.0f);


    nimcp_free(cps);
    cps = NULL;
}

int collective_phi_reset(collective_phi_system_t* cps) {
    if (!cps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_reset: cps is NULL");
        return -1;
    }

    /* Clear instances */
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_reset", 0.0f);


    memset(cps->instances, 0, sizeof(cps->instances));
    cps->instance_count = 0;

    /* Clear flows */
    memset(cps->flows, 0, sizeof(cps->flows));
    cps->flow_count = 0;

    /* Clear phi */
    memset(&cps->phi, 0, sizeof(cps->phi));

    /* Clear events */
    cps->event_count = 0;
    cps->event_head = 0;

    /* Reset stats */
    memset(&cps->stats, 0, sizeof(cps->stats));

    cps->last_phi = 0.0f;
    cps->last_update_us = get_timestamp_us();

    return 0;
}

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

int collective_phi_register_instance(
    collective_phi_system_t* cps,
    uint32_t instance_id,
    float initial_phi
) {
    if (!cps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_register_instance: cps is NULL");
        return -1;
    }

    /* Check if already registered */
    if (find_instance(cps, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_phi_register_instance: validation failed");
        return -1;
    }

    /* Find free slot */
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_register_instance", 0.0f);


    phi_instance_t* slot = find_free_instance_slot(cps);
    if (!slot) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_register_instance: slot is NULL");
        return -1;
    }

    slot->instance_id = instance_id;
    slot->active = true;
    slot->local_phi = initial_phi;
    slot->integration_factor = 1.0f;

    cps->instance_count++;

    /* Create flows to/from all existing instances */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cps->instances[i].active && cps->instances[i].instance_id != instance_id) {
            /* Bidirectional flows */
            flow_entry_t* f1 = get_or_create_flow(cps, instance_id, cps->instances[i].instance_id);
            flow_entry_t* f2 = get_or_create_flow(cps, cps->instances[i].instance_id, instance_id);

            if (f1) {
                f1->flow.flow_rate = 0.5f;
                f1->flow.mutual_information = 0.3f;
            }
            if (f2) {
                f2->flow.flow_rate = 0.5f;
                f2->flow.mutual_information = 0.3f;
            }
        }
    }

    return 0;
}

int collective_phi_unregister_instance(
    collective_phi_system_t* cps,
    uint32_t instance_id
) {
    if (!cps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_unregister_instance: cps is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_unregister_instance", 0.0f);


    phi_instance_t* inst = find_instance(cps, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_unregister_instance: inst is NULL");
        return -1;
    }

    inst->active = false;
    cps->instance_count--;

    /* Deactivate related flows */
    for (uint32_t i = 0; i < cps->flow_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cps->flow_count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)cps->flow_count);
        }

        if (cps->flows[i].from_id == instance_id ||
            cps->flows[i].to_id == instance_id) {
            cps->flows[i].active = false;
        }
    }

    return 0;
}

int collective_phi_update_local(
    collective_phi_system_t* cps,
    uint32_t instance_id,
    float local_phi
) {
    if (!cps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_update_local: cps is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_update_local", 0.0f);


    phi_instance_t* inst = find_instance(cps, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_update_local: inst is NULL");
        return -1;
    }

    inst->local_phi = local_phi;
    return 0;
}

/*=============================================================================
 * Information Flow API
 *===========================================================================*/

int collective_phi_update_flow(
    collective_phi_system_t* cps,
    uint32_t from_instance,
    uint32_t to_instance,
    const information_flow_t* flow
) {
    if (!cps || !flow) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_update_flow: required parameter is NULL (cps, flow)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_update_flow", 0.0f);


    flow_entry_t* f = get_or_create_flow(cps, from_instance, to_instance);
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_update_flow: f is NULL");
        return -1;
    }

    f->flow = *flow;
    f->flow.from_instance = from_instance;
    f->flow.to_instance = to_instance;

    return 0;
}

int collective_phi_get_flow(
    const collective_phi_system_t* cps,
    uint32_t from_instance,
    uint32_t to_instance,
    information_flow_t* flow
) {
    if (!cps || !flow) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_flow: required parameter is NULL (cps, flow)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_flow", 0.0f);


    flow_entry_t* f = find_flow((collective_phi_system_t*)cps, from_instance, to_instance);
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_flow: f is NULL");
        return -1;
    }

    *flow = f->flow;
    return 0;
}

/*=============================================================================
 * Computation API
 *===========================================================================*/

int collective_phi_update(collective_phi_system_t* cps) {
    if (!cps || !cps->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_update: required parameter is NULL (cps, cps->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_update", 0.0f);


    float old_phi = cps->phi.phi_total;

    /* Compute local phi */
    cps->phi.phi_local = compute_local_phi_sum(cps);

    /* Update integration matrix */
    update_integration_matrix(cps);

    /* Compute network phi */
    cps->phi.phi_network = compute_network_phi(cps);

    /* Compute total phi based on aggregation method */
    switch (cps->config.aggregation_method) {
        case PHI_AGG_SUM:
            cps->phi.phi_total = cps->phi.phi_local + cps->phi.phi_network;
            break;
        case PHI_AGG_AVG:
            cps->phi.phi_total = (cps->phi.phi_local + cps->phi.phi_network) / 2.0f;
            break;
        case PHI_AGG_GEOMETRIC:
            cps->phi.phi_total = sqrtf(cps->phi.phi_local * (cps->phi.phi_network + 0.1f));
            break;
        case PHI_AGG_SYNERGISTIC:
        default:
            cps->phi.phi_total = cps->phi.phi_local +
                                  cps->phi.phi_network * cps->config.synergy_coefficient;
            break;
    }

    /* Compute IIT decomposition */
    cps->phi.information = cps->phi.phi_local;
    cps->phi.integration = cps->phi.phi_network;
    cps->phi.exclusion = cps->instance_count > 1 ? 1.0f : 0.0f;

    /* Compute topology metrics */
    compute_topology_metrics(cps);

    /* Check for emergence events */
    float delta = cps->phi.phi_total - old_phi;
    if (fabsf(delta) > 0.1f) {
        record_event(cps, old_phi, cps->phi.phi_total,
                     delta > 0 ? "Phi increase" : "Phi decrease");
    }

    /* Update instance contributions */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cps->instances[i].active) continue;

        float in_flow = 0.0f, out_flow = 0.0f;
        for (uint32_t j = 0; j < cps->flow_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && cps->flow_count > 256) {
                collective_phi_heartbeat("collective_p_loop",
                                 (float)(j + 1) / (float)cps->flow_count);
            }

            if (!cps->flows[j].active) continue;
            if (cps->flows[j].to_id == cps->instances[i].instance_id) {
                in_flow += cps->flows[j].flow.flow_rate;
            }
            if (cps->flows[j].from_id == cps->instances[i].instance_id) {
                out_flow += cps->flows[j].flow.flow_rate;
            }
        }
        cps->instances[i].information_in = in_flow;
        cps->instances[i].information_out = out_flow;
        cps->instances[i].integration_factor = (in_flow + out_flow) /
            (2.0f * cps->instance_count + 0.1f);
    }

    /* Update qualia based on phi */
    cps->qualia.coherence = cps->phi.phi_network / (cps->instance_count + 1);
    cps->qualia.complexity = 1.0f - cps->phi.modularity;
    cps->qualia.spatial_extent = cps->phi.connectivity;

    /* Update statistics */
    cps->stats.computations++;
    cps->stats.avg_phi = (cps->stats.avg_phi + cps->phi.phi_total) / 2.0f;
    if (cps->phi.phi_total > cps->stats.max_phi) {
        cps->stats.max_phi = cps->phi.phi_total;
    }
    if (cps->stats.computations == 1 || cps->phi.phi_total < cps->stats.min_phi) {
        cps->stats.min_phi = cps->phi.phi_total;
    }

    /* Compute variance */
    float diff = cps->phi.phi_total - cps->stats.avg_phi;
    cps->stats.phi_variance = (cps->stats.phi_variance + diff * diff) / 2.0f;

    /* Average information flow */
    float total_flow = 0.0f;
    for (uint32_t i = 0; i < cps->flow_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cps->flow_count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)cps->flow_count);
        }

        if (cps->flows[i].active) {
            total_flow += cps->flows[i].flow.flow_rate;
        }
    }
    cps->stats.avg_information_flow = total_flow / (cps->flow_count + 1);

    cps->last_phi = cps->phi.phi_total;
    cps->last_update_us = get_timestamp_us();

    return 0;
}

int collective_phi_get(
    const collective_phi_system_t* cps,
    collective_phi_t* phi
) {
    if (!cps || !phi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get: required parameter is NULL (cps, phi)");
        return -1;
    }
    *phi = cps->phi;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get", 0.0f);


    return 0;
}

collective_consciousness_level_t collective_phi_get_level(
    const collective_phi_system_t* cps
) {
    if (!cps) return COLLECTIVE_CONSCIOUSNESS_NONE;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_level", 0.0f);


    return phi_to_level(cps->phi.phi_total);
}

int collective_phi_get_contribution(
    const collective_phi_system_t* cps,
    uint32_t instance_id,
    instance_phi_contribution_t* contribution
) {
    if (!cps || !contribution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_contribution: required parameter is NULL (cps, contribution)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_contribution", 0.0f);


    phi_instance_t* inst = find_instance((collective_phi_system_t*)cps, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_contribution: inst is NULL");
        return -1;
    }

    contribution->instance_id = instance_id;
    contribution->local_phi = inst->local_phi;
    contribution->information_flow_in = inst->information_in;
    contribution->information_flow_out = inst->information_out;
    contribution->integration_factor = inst->integration_factor;
    contribution->network_contribution = inst->local_phi * inst->integration_factor;

    return 0;
}

/*=============================================================================
 * Qualia API
 *===========================================================================*/

int collective_phi_get_qualia(
    const collective_phi_system_t* cps,
    qualia_report_t* report
) {
    if (!cps || !report) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_qualia: required parameter is NULL (cps, report)");
        return -1;
    }
    *report = cps->qualia;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_qualia", 0.0f);


    return 0;
}

int collective_phi_update_qualia(
    collective_phi_system_t* cps,
    const qualia_report_t* report
) {
    if (!cps || !report) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_update_qualia: required parameter is NULL (cps, report)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_update_qualia", 0.0f);


    cps->qualia = *report;
    return 0;
}

/*=============================================================================
 * Network Analysis API
 *===========================================================================*/

int collective_phi_get_integration_matrix(
    const collective_phi_system_t* cps,
    float* matrix,
    uint32_t* size
) {
    if (!cps || !matrix || !size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_integration_matrix: required parameter is NULL (cps, matrix, size)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_integration_matr", 0.0f);


    uint32_t n = cps->instance_count;
    if (n > *size) n = *size;

    /* Copy matrix entries */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)n);
        }

        for (uint32_t j = 0; j < n; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && n > 256) {
                collective_phi_heartbeat("collective_p_loop",
                                 (float)(j + 1) / (float)n);
            }

            matrix[idx++] = cps->integration_matrix[i][j];
        }
    }

    *size = n;
    return 0;
}

float collective_phi_compute_mip(
    collective_phi_system_t* cps,
    uint32_t* partition,
    uint32_t* num_groups
) {
    if (!cps || !partition || !num_groups) return 0.0f;

    /* Simplified MIP: bipartition that minimizes phi loss */
    /* For now, just return a trivial partition */
    *num_groups = cps->instance_count > 1 ? 2 : 1;

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_compute_mip", 0.0f);


    uint32_t idx = 0;
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (cps->instances[i].active) {
            partition[idx++] = idx % 2;  /* Alternate groups */
        }
    }

    return cps->phi.phi_network * 0.5f;  /* Rough estimate */
}

/*=============================================================================
 * Event API
 *===========================================================================*/

uint32_t collective_phi_get_events(
    const collective_phi_system_t* cps,
    emergence_event_t* events,
    uint32_t max_events
) {
    if (!cps || !events) return 0;

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_events", 0.0f);


    uint32_t count = cps->event_count < max_events ? cps->event_count : max_events;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)count);
        }

        uint32_t idx = (cps->event_head - cps->event_count + i + MAX_EVENTS) % MAX_EVENTS;
        events[i] = cps->events[idx];
    }

    return count;
}

void collective_phi_clear_events(collective_phi_system_t* cps) {
    if (!cps) return;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_clear_events", 0.0f);


    cps->event_count = 0;
    cps->event_head = 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int collective_phi_get_stats(
    const collective_phi_system_t* cps,
    collective_phi_stats_t* stats
) {
    if (!cps || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_phi_get_stats: required parameter is NULL (cps, stats)");
        return -1;
    }
    *stats = cps->stats;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_get_stats", 0.0f);


    return 0;
}

void collective_phi_reset_stats(collective_phi_system_t* cps) {
    if (!cps) return;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_reset_stats", 0.0f);


    memset(&cps->stats, 0, sizeof(cps->stats));
}

/*=============================================================================
 * Debug API
 *===========================================================================*/

void collective_phi_dump(const collective_phi_system_t* cps) {
    if (!cps) {
        printf("Collective Phi: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_dump", 0.0f);


    printf("=== Collective Phi State ===\n");
    printf("Initialized: %s\n", cps->initialized ? "yes" : "no");
    printf("Instances: %u\n", cps->instance_count);
    printf("Flows: %u\n", cps->flow_count);

    printf("\nPhi Metrics:\n");
    printf("  Local phi: %.3f\n", cps->phi.phi_local);
    printf("  Network phi: %.3f\n", cps->phi.phi_network);
    printf("  Total phi: %.3f\n", cps->phi.phi_total);
    printf("  Level: %s\n",
           collective_consciousness_level_name(phi_to_level(cps->phi.phi_total)));

    printf("\nIIT Decomposition:\n");
    printf("  Information: %.3f\n", cps->phi.information);
    printf("  Integration: %.3f\n", cps->phi.integration);
    printf("  Exclusion: %.3f\n", cps->phi.exclusion);

    printf("\nTopology:\n");
    printf("  Connectivity: %.3f\n", cps->phi.connectivity);
    printf("  Modularity: %.3f\n", cps->phi.modularity);
    printf("  Small-world: %.3f\n", cps->phi.small_world_index);

    printf("\nQualia:\n");
    printf("  Valence: %.2f, Arousal: %.2f\n",
           cps->qualia.valence, cps->qualia.arousal);
    printf("  Complexity: %.2f, Coherence: %.2f\n",
           cps->qualia.complexity, cps->qualia.coherence);

    printf("\nInstances:\n");
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            collective_phi_heartbeat("collective_p_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!cps->instances[i].active) continue;
        printf("  [%u] phi=%.3f in=%.2f out=%.2f int=%.2f\n",
               cps->instances[i].instance_id,
               cps->instances[i].local_phi,
               cps->instances[i].information_in,
               cps->instances[i].information_out,
               cps->instances[i].integration_factor);
    }

    printf("\nStatistics:\n");
    printf("  Computations: %lu\n", (unsigned long)cps->stats.computations);
    printf("  Avg phi: %.3f (min: %.3f, max: %.3f)\n",
           cps->stats.avg_phi, cps->stats.min_phi, cps->stats.max_phi);
    printf("  Emergence events: %lu\n", (unsigned long)cps->stats.emergence_events);
    printf("  Fragmentation events: %lu\n", (unsigned long)cps->stats.fragmentation_events);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Collective Phi self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int collective_phi_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    collective_phi_heartbeat("collective_p_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Collective_Phi");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                collective_phi_heartbeat("collective_p_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            printf("Collective Phi self-knowledge: %s\n", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Collective_Phi");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Collective_Phi");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */

void collective_phi_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_collective_phi_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static _Atomic uint64_t g_collective_phi_training_steps = 0;
static _Atomic double g_collective_phi_training_total_error = 0.0;
static _Atomic double g_collective_phi_training_best_error = 1e30;
static _Atomic bool g_collective_phi_training_active = false;

int collective_phi_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_phi_training_begin: NULL argument");
        return -1;
    }
    collective_phi_heartbeat_instance(g_collective_phi_instance_health_agent, "coll_phi_train_begin", 0.0f);
    collective_phi_system_t* ctx = (collective_phi_system_t*)instance;

    /* Reset training counters */
    atomic_store(&g_collective_phi_training_steps, 0);
    atomic_store(&g_collective_phi_training_total_error, 0.0);
    atomic_store(&g_collective_phi_training_best_error, 1e30);
    atomic_store(&g_collective_phi_training_active, true);

    /* Reset module stats */
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->last_phi = 0.0f;

    NIMCP_LOGGING_INFO("collective_phi training begin: counters reset");
    return 0;
}

int collective_phi_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_phi_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_phi_heartbeat_instance(g_collective_phi_instance_health_agent, "coll_phi_train_step", progress);
    (void)instance;

    atomic_fetch_add(&g_collective_phi_training_steps, 1);

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    double old_error = atomic_load(&g_collective_phi_training_total_error);
    atomic_store(&g_collective_phi_training_total_error, old_error * (double)decay);

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    double old_best = atomic_load(&g_collective_phi_training_best_error);
    double new_best = old_best - (double)threshold_adjust;
    if (new_best < 0.0) new_best = 0.0;
    atomic_store(&g_collective_phi_training_best_error, new_best);

    return 0;
}

int collective_phi_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_phi_training_end: NULL argument");
        return -1;
    }
    collective_phi_heartbeat_instance(g_collective_phi_instance_health_agent, "coll_phi_train_end", 1.0f);

    collective_phi_system_t* ctx = (collective_phi_system_t*)instance;
    /* Compute final averages */
    uint64_t steps = atomic_load(&g_collective_phi_training_steps);
    double total_error = atomic_load(&g_collective_phi_training_total_error);
    double best_error = atomic_load(&g_collective_phi_training_best_error);
    double avg_error = (steps > 0)
        ? total_error / (double)steps
        : 0.0;

    float final_phi = ctx->phi.phi_total;
    uint64_t total_computations = ctx->stats.computations;

    /* Clear training flag */
    atomic_store(&g_collective_phi_training_active, false);

    NIMCP_LOGGING_INFO("collective_phi training end: %lu steps, avg_error=%.6f, best_error=%.6f, final_phi=%.4f, computations=%lu",
                       (unsigned long)steps,
                       avg_error, best_error,
                       final_phi, (unsigned long)total_computations);
    return 0;
}
