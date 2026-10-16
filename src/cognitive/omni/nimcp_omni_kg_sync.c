/**
 * @file nimcp_omni_kg_sync.c
 * @brief Implementation of Omnidirectional Inference KG Synchronization
 */

#include "cognitive/omni/nimcp_omni_kg_sync.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_temporal_replay.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(omni_kg_sync, MESH_ADAPTER_CATEGORY_COGNITIVE)

void omni_kg_sync_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_omni_kg_sync_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_kg_sync_training_begin(void* ctx) {
    /* ctx is optional; NULL means no instance state — not an error */
    omni_kg_sync_heartbeat_instance(g_omni_kg_sync_health_agent, "training_begin", 0.0f);
    (void)ctx;
    return 0;
}

int omni_kg_sync_training_step(void* ctx, float progress) {
    /* ctx is optional; NULL means no instance state — not an error */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_kg_sync_heartbeat_instance(g_omni_kg_sync_health_agent, "training_step", progress);
    (void)ctx;
    return 0;
}

int omni_kg_sync_training_end(void* ctx) {
    /* ctx is optional; NULL means no instance state — not an error */
    omni_kg_sync_heartbeat_instance(g_omni_kg_sync_health_agent, "training_end", 1.0f);
    (void)ctx;
    return 0;
}

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static brain_kg_node_type_t map_omni_to_kg_type(omni_kg_module_type_t type) {
    switch (type) {
        case OMNI_KG_TYPE_PREDICTOR:
            return BRAIN_KG_NODE_COGNITIVE;
        case OMNI_KG_TYPE_MEMORY:
            return BRAIN_KG_NODE_COGNITIVE;
        case OMNI_KG_TYPE_HIERARCHY_LEVEL:
            return BRAIN_KG_NODE_COGNITIVE;
        case OMNI_KG_TYPE_REPLAY:
            return BRAIN_KG_NODE_COGNITIVE;
        case OMNI_KG_TYPE_BRIDGE:
            return BRAIN_KG_NODE_INTEGRATION;
        case OMNI_KG_TYPE_SENSORY:
            return BRAIN_KG_NODE_PERCEPTION;
        case OMNI_KG_TYPE_CORTICAL:
            return BRAIN_KG_NODE_CORTICAL;
        case OMNI_KG_TYPE_LANGUAGE:
            return BRAIN_KG_NODE_COGNITIVE;
        default:
            return BRAIN_KG_NODE_CUSTOM;
    }
}

static brain_kg_edge_type_t map_omni_edge_to_kg_edge(omni_kg_edge_type_t edge) {
    switch (edge) {
        case OMNI_KG_EDGE_PREDICTS_FORWARD:
        case OMNI_KG_EDGE_PREDICTS_DOWN:
            return BRAIN_KG_EDGE_SENDS_TO;
        case OMNI_KG_EDGE_PREDICTS_BACKWARD:
        case OMNI_KG_EDGE_PREDICTS_UP:
            return BRAIN_KG_EDGE_RECEIVES_FROM;
        case OMNI_KG_EDGE_PREDICTS_LATERAL:
        case OMNI_KG_EDGE_BINDS_WITH:
            return BRAIN_KG_EDGE_INTEGRATES_WITH;
        case OMNI_KG_EDGE_MODULATES_PRECISION:
            return BRAIN_KG_EDGE_MODULATES;
        case OMNI_KG_EDGE_REPLAYS_TO:
            return BRAIN_KG_EDGE_SENDS_TO;
        default:
            return BRAIN_KG_EDGE_CONNECTS_TO;
    }
}

static int ensure_capacity(omni_kg_sync_t* sync) {
    if (sync->module_count < sync->module_capacity) {
        return 0;
    }

    uint32_t new_capacity = sync->module_capacity == 0 ? 16 :
                            sync->module_capacity * 2;

    omni_kg_module_info_t* new_modules = nimcp_realloc(
        sync->modules, new_capacity * sizeof(omni_kg_module_info_t));
    if (!new_modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_capacity: new_modules is NULL");
        return -1;
    }
    sync->modules = new_modules;

    brain_kg_node_id_t* new_ids = nimcp_realloc(
        sync->node_ids, new_capacity * sizeof(brain_kg_node_id_t));
    if (!new_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_capacity: new_ids is NULL");
        return -1;
    }
    sync->node_ids = new_ids;

    sync->module_capacity = new_capacity;
    return 0;
}

static int find_module_index(const omni_kg_sync_t* sync, const char* name) {
    for (uint32_t i = 0; i < sync->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sync->module_count > 256) {
            omni_kg_sync_heartbeat("omni_kg_sync_loop",
                             (float)(i + 1) / (float)sync->module_count);
        }

        if (strcmp(sync->modules[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;  /* Not found is a normal lookup result, not an error */
}

static int find_module_index_by_node(const omni_kg_sync_t* sync,
                                      brain_kg_node_id_t node_id) {
    for (uint32_t i = 0; i < sync->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sync->module_count > 256) {
            omni_kg_sync_heartbeat("omni_kg_sync_loop",
                             (float)(i + 1) / (float)sync->module_count);
        }

        if (sync->node_ids[i] == node_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is a normal lookup result, not an error */
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_kg_sync_default_config(omni_kg_sync_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_kg_sync_config_t));

    config->create_nodes = true;
    config->create_edges = true;
    config->update_precision = true;
    config->sync_capabilities = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_kg_sync_t* omni_kg_sync_create(brain_kg_t* kg,
                                     const omni_kg_sync_config_t* config) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_create", 0.0f);


    omni_kg_sync_t* sync = nimcp_calloc(1, sizeof(omni_kg_sync_t));
    if (!sync) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sync");

        return NULL;

    }

    sync->kg = kg;

    if (config) {
        memcpy(&sync->config, config, sizeof(omni_kg_sync_config_t));
    } else {
        omni_kg_sync_default_config(&sync->config);
    }

    sync->mutex = nimcp_mutex_create(NULL);
    if (!sync->mutex) {
        nimcp_free(sync);
        sync = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_kg_sync_default_config: sync->mutex is NULL");
        return NULL;
    }

    /* Initialize registry */
    sync->module_capacity = 16;
    sync->modules = nimcp_calloc(sync->module_capacity,
                                  sizeof(omni_kg_module_info_t));
    sync->node_ids = nimcp_calloc(sync->module_capacity,
                                   sizeof(brain_kg_node_id_t));

    if (!sync->modules || !sync->node_ids) {
        if (sync->modules) nimcp_free(sync->modules);
        if (sync->node_ids) nimcp_free(sync->node_ids);
        nimcp_mutex_destroy(sync->mutex);
        nimcp_free(sync);
        sync = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_kg_sync_default_config: validation failed");
        return NULL;
    }

    memset(&sync->stats, 0, sizeof(omni_kg_sync_stats_t));

    return sync;
}

void omni_kg_sync_destroy(omni_kg_sync_t* sync) {
    if (!sync) return;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_destroy", 0.0f);


    if (sync->modules) nimcp_free(sync->modules);
    if (sync->node_ids) nimcp_free(sync->node_ids);

    if (sync->mutex) {
        nimcp_mutex_destroy(sync->mutex);
    }

    nimcp_free(sync);
    sync = NULL;
}

/* ============================================================================
 * Registration API
 * ============================================================================ */

brain_kg_node_id_t omni_kg_register_module(omni_kg_sync_t* sync,
                                            const omni_kg_module_info_t* info) {
    if (!sync || !info) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_register_mod", 0.0f);


    nimcp_mutex_lock(sync->mutex);

    /* Check if already registered */
    int existing = find_module_index(sync, info->name);
    if (existing >= 0) {
        brain_kg_node_id_t node_id = sync->node_ids[existing];
        nimcp_mutex_unlock(sync->mutex);
        return node_id;
    }

    /* Ensure capacity */
    if (ensure_capacity(sync) != 0) {
        nimcp_mutex_unlock(sync->mutex);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Create description */
    char description[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(description, sizeof(description),
             "Omnidirectional %s module with capabilities: 0x%02X",
             omni_kg_module_type_to_string(info->type),
             info->capabilities);

    /* Add node to brain KG */
    brain_kg_node_id_t node_id = brain_kg_add_node(
        sync->kg,
        info->name,
        map_omni_to_kg_type(info->type),
        description
    );

    if (node_id == BRAIN_KG_INVALID_NODE) {
        nimcp_mutex_unlock(sync->mutex);
        return BRAIN_KG_INVALID_NODE;
    }

    /* Store in registry */
    uint32_t index = sync->module_count;
    memcpy(&sync->modules[index], info, sizeof(omni_kg_module_info_t));
    sync->node_ids[index] = node_id;
    sync->module_count++;

    /* Add metadata for capabilities */
    char cap_str[32];
    snprintf(cap_str, sizeof(cap_str), "0x%02X", info->capabilities);
    brain_kg_add_metadata(sync->kg, node_id, "omni_capabilities", cap_str);

    char prec_str[32];
    snprintf(prec_str, sizeof(prec_str), "%.3f", info->default_precision);
    brain_kg_add_metadata(sync->kg, node_id, "default_precision", prec_str);

    char type_str[32];
    snprintf(type_str, sizeof(type_str), "%d", (int)info->type);
    brain_kg_add_metadata(sync->kg, node_id, "omni_type", type_str);

    sync->stats.nodes_created++;

    nimcp_mutex_unlock(sync->mutex);
    return node_id;
}

brain_kg_node_id_t omni_kg_register_jepa(omni_kg_sync_t* sync,
                                          const char* name,
                                          jepa_bidirectional_t* jepa) {
    if (!sync || !name) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_register_jep", 0.0f);


    omni_kg_module_info_t info = {0};
    strncpy(info.name, name, sizeof(info.name) - 1);
    info.type = OMNI_KG_TYPE_PREDICTOR;
    info.bio_module_id = 0x0E10;  /* BIO_MODULE_JEPA_BIDIRECTIONAL */
    info.capabilities = OMNI_KG_CAP_FORWARD | OMNI_KG_CAP_BACKWARD |
                        OMNI_KG_CAP_LATERAL | OMNI_KG_CAP_MASKED;
    info.default_precision = 1.0f;
    info.module_ptr = jepa;

    return omni_kg_register_module(sync, &info);
}

brain_kg_node_id_t omni_kg_register_hopfield(omni_kg_sync_t* sync,
                                              const char* name,
                                              hopfield_memory_t* hopfield) {
    if (!sync || !name) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_register_hop", 0.0f);


    omni_kg_module_info_t info = {0};
    strncpy(info.name, name, sizeof(info.name) - 1);
    info.type = OMNI_KG_TYPE_MEMORY;
    info.bio_module_id = 0x0E20;  /* BIO_MODULE_HOPFIELD_MEMORY */
    info.capabilities = OMNI_KG_CAP_ASSOCIATIVE | OMNI_KG_CAP_LATERAL;
    info.default_precision = 1.0f;
    info.module_ptr = hopfield;

    return omni_kg_register_module(sync, &info);
}

brain_kg_node_id_t omni_kg_register_pred_level(omni_kg_sync_t* sync,
                                                const char* name,
                                                uint32_t level_index,
                                                predictive_hierarchy_t* hierarchy) {
    if (!sync || !name) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_register_pre", 0.0f);


    omni_kg_module_info_t info = {0};
    snprintf(info.name, sizeof(info.name), "%s_L%u", name, level_index);
    info.type = OMNI_KG_TYPE_HIERARCHY_LEVEL;
    info.bio_module_id = 0x0E30 + (uint16_t)level_index;
    info.capabilities = OMNI_KG_CAP_FORWARD | OMNI_KG_CAP_BACKWARD |
                        OMNI_KG_CAP_HIERARCHICAL;
    info.default_precision = 1.0f;
    info.module_ptr = hierarchy;

    return omni_kg_register_module(sync, &info);
}

brain_kg_node_id_t omni_kg_register_replay(omni_kg_sync_t* sync,
                                            const char* name,
                                            temporal_replay_t* replay) {
    if (!sync || !name) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_register_rep", 0.0f);


    omni_kg_module_info_t info = {0};
    strncpy(info.name, name, sizeof(info.name) - 1);
    info.type = OMNI_KG_TYPE_REPLAY;
    info.bio_module_id = 0x0E40;  /* BIO_MODULE_TEMPORAL_REPLAY */
    info.capabilities = OMNI_KG_CAP_FORWARD | OMNI_KG_CAP_BACKWARD;
    info.default_precision = 1.0f;
    info.module_ptr = replay;

    return omni_kg_register_module(sync, &info);
}

brain_kg_node_id_t omni_kg_register_bridge(omni_kg_sync_t* sync,
                                            const char* name,
                                            omni_kg_capability_t capabilities,
                                            void* bridge) {
    if (!sync || !name) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_register_bri", 0.0f);


    omni_kg_module_info_t info = {0};
    strncpy(info.name, name, sizeof(info.name) - 1);
    info.type = OMNI_KG_TYPE_BRIDGE;
    info.bio_module_id = 0x0E50;  /* Generic bridge ID */
    info.capabilities = capabilities;
    info.default_precision = 1.0f;
    info.module_ptr = bridge;

    return omni_kg_register_module(sync, &info);
}

/* ============================================================================
 * Edge API
 * ============================================================================ */

int omni_kg_add_prediction_edge(omni_kg_sync_t* sync,
                                 brain_kg_node_id_t from_node,
                                 brain_kg_node_id_t to_node,
                                 omni_kg_edge_type_t edge_type,
                                 float precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_add_predicti", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    nimcp_mutex_lock(sync->mutex);

    /* Create description with precision */
    char description[NIMCP_ERROR_BUFFER_MEDIUM];
    snprintf(description, sizeof(description),
             "%s edge (precision=%.3f)",
             omni_kg_edge_type_to_string(edge_type),
             precision);

    /* Add edge to brain KG */
    brain_kg_edge_id_t edge_id = brain_kg_add_edge(
        sync->kg,
        from_node,
        to_node,
        map_omni_edge_to_kg_edge(edge_type),
        description,
        precision
    );

    if (edge_id == BRAIN_KG_INVALID_NODE) {
        nimcp_mutex_unlock(sync->mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    sync->stats.edges_created++;

    nimcp_mutex_unlock(sync->mutex);
    return NIMCP_SUCCESS;
}

int omni_kg_add_bidirectional_edge(omni_kg_sync_t* sync,
                                    brain_kg_node_id_t node_a,
                                    brain_kg_node_id_t node_b,
                                    float precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_add_bidirect", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    int result = omni_kg_add_prediction_edge(sync, node_a, node_b,
                                              OMNI_KG_EDGE_PREDICTS_FORWARD,
                                              precision);
    if (result != NIMCP_SUCCESS) return result;

    return omni_kg_add_prediction_edge(sync, node_b, node_a,
                                        OMNI_KG_EDGE_PREDICTS_BACKWARD,
                                        precision);
}

int omni_kg_add_hierarchical_edges(omni_kg_sync_t* sync,
                                    brain_kg_node_id_t lower_node,
                                    brain_kg_node_id_t upper_node,
                                    float precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_add_hierarch", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    /* Bottom-up (lower → upper) */
    int result = omni_kg_add_prediction_edge(sync, lower_node, upper_node,
                                              OMNI_KG_EDGE_PREDICTS_UP,
                                              precision);
    if (result != NIMCP_SUCCESS) return result;

    /* Top-down (upper → lower) */
    return omni_kg_add_prediction_edge(sync, upper_node, lower_node,
                                        OMNI_KG_EDGE_PREDICTS_DOWN,
                                        precision);
}

int omni_kg_update_edge_precision(omni_kg_sync_t* sync,
                                   brain_kg_node_id_t from_node,
                                   brain_kg_node_id_t to_node,
                                   omni_kg_edge_type_t edge_type,
                                   float new_precision) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_update_edge_", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    nimcp_mutex_lock(sync->mutex);

    /* Find the edge first (edge_type is used for semantics, find_edge uses from/to only) */
    (void)edge_type; /* edge_type used for identifying edge type in multi-edge scenarios */
    brain_kg_edge_id_t edge_id = brain_kg_find_edge(sync->kg, from_node, to_node);
    if (edge_id == BRAIN_KG_INVALID_NODE) {
        nimcp_mutex_unlock(sync->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Update edge weight in brain KG */
    int result = brain_kg_update_edge(sync->kg, edge_id, new_precision, NULL);

    if (result == 0) {
        sync->stats.edges_updated++;
    }

    nimcp_mutex_unlock(sync->mutex);
    return result == 0 ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_kg_get_modules_with_capability(const omni_kg_sync_t* sync,
                                         omni_kg_capability_t capability,
                                         brain_kg_node_id_t* nodes_out,
                                         uint32_t max_nodes) {
    if (!sync || !nodes_out || max_nodes == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_get_modules_", 0.0f);


    nimcp_mutex_lock(((omni_kg_sync_t*)sync)->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < sync->module_count && count < max_nodes; i++) {
        if (sync->modules[i].capabilities & capability) {
            nodes_out[count++] = sync->node_ids[i];
        }
    }

    nimcp_mutex_unlock(((omni_kg_sync_t*)sync)->mutex);
    return (int)count;
}

int omni_kg_get_prediction_path(const omni_kg_sync_t* sync,
                                 brain_kg_node_id_t from_node,
                                 brain_kg_node_id_t to_node,
                                 omni_kg_edge_type_t direction,
                                 brain_kg_node_id_t* path_out,
                                 uint32_t max_path_len) {
    if (!sync || !path_out || max_path_len == 0) return 0;

    /* Use brain_kg path finding */
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_get_predicti", 0.0f);


    brain_kg_path_t* path = brain_kg_find_path(sync->kg, from_node, to_node);
    if (!path) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < path->length && count < max_path_len; i++) {
        path_out[count++] = path->nodes[i];
    }

    brain_kg_path_destroy(path);
    return (int)count;
}

int omni_kg_get_predictors_for(const omni_kg_sync_t* sync,
                                brain_kg_node_id_t target_node,
                                omni_kg_edge_type_t direction,
                                brain_kg_node_id_t* predictors_out,
                                uint32_t max_predictors) {
    if (!sync || !predictors_out || max_predictors == 0) return 0;

    /* Get incoming edges */
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_get_predicto", 0.0f);


    brain_kg_edge_list_t* edges = brain_kg_get_incoming(sync->kg, target_node);
    if (!edges) return 0;

    brain_kg_edge_type_t target_type = map_omni_edge_to_kg_edge(direction);
    uint32_t count = 0;

    for (uint32_t i = 0; i < edges->count && count < max_predictors; i++) {
        if (edges->edges[i]->type == target_type) {
            predictors_out[count++] = edges->edges[i]->from;
        }
    }

    brain_kg_edge_list_destroy(edges);
    return (int)count;
}

brain_kg_node_id_t omni_kg_get_node_by_name(omni_kg_sync_t* sync,
                                             const char* name) {
    if (!sync || !name) return BRAIN_KG_INVALID_NODE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_get_node_by_", 0.0f);


    nimcp_mutex_lock(((omni_kg_sync_t*)sync)->mutex);

    int index = find_module_index(sync, name);
    brain_kg_node_id_t result = (index >= 0) ? sync->node_ids[index] :
                                                BRAIN_KG_INVALID_NODE;

    nimcp_mutex_unlock(((omni_kg_sync_t*)sync)->mutex);
    return result;
}

omni_kg_capability_t omni_kg_get_capabilities(omni_kg_sync_t* sync,
                                               brain_kg_node_id_t node) {
    if (!sync) return OMNI_KG_CAP_NONE;

    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_get_capabili", 0.0f);


    nimcp_mutex_lock(((omni_kg_sync_t*)sync)->mutex);

    int index = find_module_index_by_node(sync, node);
    omni_kg_capability_t result = (index >= 0) ?
        sync->modules[index].capabilities : OMNI_KG_CAP_NONE;

    nimcp_mutex_unlock(((omni_kg_sync_t*)sync)->mutex);
    return result;
}

/* ============================================================================
 * Sync API
 * ============================================================================ */

int omni_kg_sync_all(omni_kg_sync_t* sync) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_all", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    nimcp_mutex_lock(sync->mutex);

    /* All modules are already registered, just update timestamp */
    sync->stats.total_syncs++;
    sync->stats.last_sync_time_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(sync->mutex);
    return NIMCP_SUCCESS;
}

int omni_kg_sync_precision(omni_kg_sync_t* sync) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_precision", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    nimcp_mutex_lock(sync->mutex);

    /* Update precision metadata for all modules */
    for (uint32_t i = 0; i < sync->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sync->module_count > 256) {
            omni_kg_sync_heartbeat("omni_kg_sync_loop",
                             (float)(i + 1) / (float)sync->module_count);
        }

        char prec_str[32];
        snprintf(prec_str, sizeof(prec_str), "%.3f",
                 sync->modules[i].default_precision);
        brain_kg_add_metadata(sync->kg, sync->node_ids[i],
                               "current_precision", prec_str);
        sync->stats.nodes_updated++;
    }

    sync->stats.total_syncs++;
    sync->stats.last_sync_time_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(sync->mutex);
    return NIMCP_SUCCESS;
}

int omni_kg_sync_capabilities(omni_kg_sync_t* sync) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_capabilities", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    nimcp_mutex_lock(sync->mutex);

    /* Update capability metadata for all modules */
    for (uint32_t i = 0; i < sync->module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sync->module_count > 256) {
            omni_kg_sync_heartbeat("omni_kg_sync_loop",
                             (float)(i + 1) / (float)sync->module_count);
        }

        char cap_str[32];
        snprintf(cap_str, sizeof(cap_str), "0x%02X",
                 sync->modules[i].capabilities);
        brain_kg_add_metadata(sync->kg, sync->node_ids[i],
                               "omni_capabilities", cap_str);
        sync->stats.nodes_updated++;
    }

    sync->stats.total_syncs++;
    sync->stats.last_sync_time_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(sync->mutex);
    return NIMCP_SUCCESS;
}

int omni_kg_get_sync_stats(omni_kg_sync_t* sync,
                            omni_kg_sync_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_get_sync_sta", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    nimcp_mutex_lock(((omni_kg_sync_t*)sync)->mutex);
    memcpy(stats, &sync->stats, sizeof(omni_kg_sync_stats_t));
    nimcp_mutex_unlock(((omni_kg_sync_t*)sync)->mutex);
    return NIMCP_SUCCESS;
}

int omni_kg_reset_sync_stats(omni_kg_sync_t* sync) {
    /* Phase 8: Heartbeat at operation start */
    omni_kg_sync_heartbeat("omni_kg_sync_omni_kg_reset_sync_s", 0.0f);


    NIMCP_CHECK_THROW(sync, NIMCP_ERROR_INVALID_PARAM, "sync is NULL");

    nimcp_mutex_lock(sync->mutex);
    memset(&sync->stats, 0, sizeof(omni_kg_sync_stats_t));
    nimcp_mutex_unlock(sync->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_kg_module_type_to_string(omni_kg_module_type_t type) {
    switch (type) {
        case OMNI_KG_TYPE_PREDICTOR: return "PREDICTOR";
        case OMNI_KG_TYPE_MEMORY: return "MEMORY";
        case OMNI_KG_TYPE_HIERARCHY_LEVEL: return "HIERARCHY_LEVEL";
        case OMNI_KG_TYPE_REPLAY: return "REPLAY";
        case OMNI_KG_TYPE_BRIDGE: return "BRIDGE";
        case OMNI_KG_TYPE_SENSORY: return "SENSORY";
        case OMNI_KG_TYPE_CORTICAL: return "CORTICAL";
        case OMNI_KG_TYPE_LANGUAGE: return "LANGUAGE";
        default: return "UNKNOWN";
    }
}

const char* omni_kg_edge_type_to_string(omni_kg_edge_type_t edge) {
    switch (edge) {
        case OMNI_KG_EDGE_PREDICTS_FORWARD: return "PREDICTS_FORWARD";
        case OMNI_KG_EDGE_PREDICTS_BACKWARD: return "PREDICTS_BACKWARD";
        case OMNI_KG_EDGE_PREDICTS_LATERAL: return "PREDICTS_LATERAL";
        case OMNI_KG_EDGE_PREDICTS_UP: return "PREDICTS_UP";
        case OMNI_KG_EDGE_PREDICTS_DOWN: return "PREDICTS_DOWN";
        case OMNI_KG_EDGE_MODULATES_PRECISION: return "MODULATES_PRECISION";
        case OMNI_KG_EDGE_BINDS_WITH: return "BINDS_WITH";
        case OMNI_KG_EDGE_REPLAYS_TO: return "REPLAYS_TO";
        default: return "UNKNOWN";
    }
}

const char* omni_kg_capability_to_string(omni_kg_capability_t cap) {
    switch (cap) {
        case OMNI_KG_CAP_NONE: return "NONE";
        case OMNI_KG_CAP_FORWARD: return "FORWARD";
        case OMNI_KG_CAP_BACKWARD: return "BACKWARD";
        case OMNI_KG_CAP_LATERAL: return "LATERAL";
        case OMNI_KG_CAP_HIERARCHICAL: return "HIERARCHICAL";
        case OMNI_KG_CAP_ASSOCIATIVE: return "ASSOCIATIVE";
        case OMNI_KG_CAP_MASKED: return "MASKED";
        case OMNI_KG_CAP_ALL: return "ALL";
        default: return "MIXED";
    }
}
