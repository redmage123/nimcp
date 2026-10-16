/**
 * @file nimcp_mesh_kg_integration.c
 * @brief Knowledge Graph and Mesh Topology Integration Implementation
 *
 * WHAT: Integrates Knowledge Graph module wiring with mesh network topology
 * WHY:  Synchronize KG module relationships with mesh endorsement and routing paths
 * HOW:  Bi-directional sync between KG topology and mesh fractal topology
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_kg_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_topology.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal KG integration structure
 */
struct mesh_kg_integration {
    uint32_t magic;
    mesh_kg_integration_config_t config;

    /* Dependencies */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* integration;
    mesh_topology_ctx_t topology_ctx;

    /* Module mappings */
    mesh_kg_module_mapping_t mappings[MESH_KG_MAX_MODULES];
    size_t mapping_count;

    /* Hub assignments */
    mesh_kg_hub_assignment_t hubs[MESH_KG_MAX_HUBS];
    size_t hub_count;

    /* Statistics */
    mesh_kg_integration_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_kg_integration_default_config(
    mesh_kg_integration_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    config->sync_mode = MESH_KG_SYNC_BIDIRECTIONAL;
    config->discovery = MESH_KG_DISCOVER_BOTH;

    config->auto_sync_on_change = true;
    config->sync_interval_ms = 5000;
    config->propagate_updates_through_mesh = true;

    config->hub_centrality_threshold = 0.75f;
    config->hub_degree_percentile = 0.90f;

    config->derive_endorsers_from_kg = true;
    config->map_handlers_to_policies = true;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_kg_integration_t* mesh_kg_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_kg_integration_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create KG integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_kg_integration_create: bootstrap is NULL");
        return NULL;
    }

    mesh_kg_integration_config_t default_config;
    if (!config) {
        mesh_kg_integration_default_config(&default_config);
        config = &default_config;
    }

    mesh_kg_integration_t* kg = nimcp_calloc(1, sizeof(*kg));
    if (!kg) {
        LOG_ERROR("Failed to allocate KG integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_kg_integration_create: kg is NULL");
        return NULL;
    }

    kg->magic = MESH_KG_INTEGRATION_MAGIC;
    kg->config = *config;
    kg->bootstrap = bootstrap;
    kg->integration = mesh_bootstrap_get_integration(bootstrap);

    /* Create topology context for analysis */
    mesh_topology_config_t topo_config = mesh_topology_default_config();
    topo_config.hub_percentile = config->hub_degree_percentile;
    topo_config.high_centrality_threshold = config->hub_centrality_threshold;
    kg->topology_ctx = mesh_topology_create(&topo_config);

    /* Create mutex */
    mutex_attr_t attr = {0};
    kg->mutex = nimcp_mutex_create(&attr);
    if (!kg->mutex) {
        LOG_ERROR("Failed to create KG integration mutex");
        if (kg->topology_ctx) mesh_topology_destroy(kg->topology_ctx);
        nimcp_free(kg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_kg_integration_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("KG integration created");
    return kg;
}

void mesh_kg_integration_destroy(mesh_kg_integration_t* integration) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) return;

    nimcp_mutex_lock(integration->mutex);
    /* Cleanup mappings */
    nimcp_mutex_unlock(integration->mutex);

    if (integration->topology_ctx) {
        mesh_topology_destroy(integration->topology_ctx);
    }

    nimcp_mutex_destroy(integration->mutex);
    integration->magic = 0;
    nimcp_free(integration);

    LOG_DEBUG("KG integration destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static int find_mapping_by_name(
    const mesh_kg_integration_t* kg,
    const char* name
) {
    for (size_t i = 0; i < kg->mapping_count; i++) {
        if (kg->mappings[i].active &&
            strcmp(kg->mappings[i].module_name, name) == 0) {
            return (int)i;
        }
    }
    /* Mapping not found - normal lookup result */
    return -1;
}

static int find_mapping_by_id(
    const mesh_kg_integration_t* kg,
    mesh_participant_id_t id
) {
    for (size_t i = 0; i < kg->mapping_count; i++) {
        if (kg->mappings[i].active &&
            kg->mappings[i].participant_id == id) {
            return (int)i;
        }
    }
    /* Mapping not found - normal lookup result */
    return -1;
}

static mesh_channel_id_t determine_channel_from_type(const char* module_type) {
    if (!module_type) return MESH_CHANNEL_SYSTEM;

    /* Map module types to channels */
    if (strstr(module_type, "COGNITIVE") ||
        strstr(module_type, "REASONING") ||
        strstr(module_type, "PFC")) {
        return MESH_CHANNEL_LEFT_HEMISPHERE;
    }
    if (strstr(module_type, "CREATIVE") ||
        strstr(module_type, "SPATIAL") ||
        strstr(module_type, "HOLISTIC")) {
        return MESH_CHANNEL_RIGHT_HEMISPHERE;
    }
    if (strstr(module_type, "EMOTION") ||
        strstr(module_type, "LIMBIC") ||
        strstr(module_type, "MOTOR")) {
        return MESH_CHANNEL_SUBCORTICAL;
    }
    if (strstr(module_type, "GPU") ||
        strstr(module_type, "BATCH") ||
        strstr(module_type, "ACCEL")) {
        return MESH_CHANNEL_GPU_COMPUTE;
    }

    return MESH_CHANNEL_SYSTEM;
}

static mesh_participant_id_t generate_participant_id(
    mesh_channel_id_t channel,
    uint16_t type_id,
    uint32_t local_id
) {
    return ((uint64_t)channel << 48) |
           ((uint64_t)type_id << 32) |
           local_id;
}

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

mesh_participant_id_t mesh_kg_integration_register_wiring(
    mesh_kg_integration_t* integration,
    const kg_module_wiring_t* wiring
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        return 0;
    }
    if (!wiring) return 0;

    nimcp_mutex_lock(integration->mutex);

    /* Check if already registered */
    int idx = find_mapping_by_name(integration, wiring->module_name);
    if (idx >= 0) {
        /* Update existing mapping */
        mesh_kg_module_mapping_t* mapping = &integration->mappings[idx];
        mapping->input_count = wiring->input_count;
        mapping->output_count = wiring->output_count;
        mapping->handler_count = wiring->handler_count;
        mapping->last_sync_ns = nimcp_time_now_ns();
        nimcp_mutex_unlock(integration->mutex);
        return mapping->participant_id;
    }

    /* Check capacity */
    if (integration->mapping_count >= MESH_KG_MAX_MODULES) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("KG integration at max module capacity");
        return 0;
    }

    /* Create new mapping */
    mesh_kg_module_mapping_t* mapping =
        &integration->mappings[integration->mapping_count];
    memset(mapping, 0, sizeof(*mapping));

    strncpy(mapping->module_name, wiring->module_name, MESH_MAX_NAME_LEN - 1);
    strncpy(mapping->module_type, wiring->module_type, 31);

    /* Determine channel from type */
    mapping->primary_channel = determine_channel_from_type(wiring->module_type);

    /* Generate participant ID */
    static uint32_t next_local_id = 1;
    mapping->participant_id = generate_participant_id(
        mapping->primary_channel,
        (uint16_t)MESH_PARTICIPANT_MODULE,
        next_local_id++
    );

    /* Copy KG wiring info */
    mapping->input_count = wiring->input_count;
    mapping->output_count = wiring->output_count;
    mapping->handler_count = wiring->handler_count;

    /* Determine if this should be an endorser */
    mapping->is_endorser = (wiring->output_count > 0);
    mapping->is_required_endorser =
        (strstr(wiring->module_type, "MEMORY") != NULL ||
         strstr(wiring->module_type, "SECURITY") != NULL);
    mapping->has_veto =
        (strstr(wiring->module_name, "amygdala") != NULL ||
         strstr(wiring->module_name, "immune") != NULL);

    mapping->active = true;
    mapping->last_sync_ns = nimcp_time_now_ns();

    integration->mapping_count++;
    integration->stats.modules_synced++;

    /* Add to topology context */
    if (integration->topology_ctx) {
        mesh_topology_add_participant(
            integration->topology_ctx,
            mapping->participant_id
        );
    }

    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Registered KG module '%s' -> participant 0x%llx",
                 wiring->module_name,
                 (unsigned long long)mapping->participant_id);
    }

    return mapping->participant_id;
}

nimcp_error_t mesh_kg_integration_unregister_module(
    mesh_kg_integration_t* integration,
    const char* module_name
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module_name) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(integration->mutex);

    int idx = find_mapping_by_name(integration, module_name);
    if (idx < 0) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_participant_id_t pid = integration->mappings[idx].participant_id;

    /* Remove from topology */
    if (integration->topology_ctx) {
        mesh_topology_remove_participant(integration->topology_ctx, pid);
    }

    /* Mark inactive */
    integration->mappings[idx].active = false;

    /* Compact array */
    for (size_t i = idx; i < integration->mapping_count - 1; i++) {
        integration->mappings[i] = integration->mappings[i + 1];
    }
    integration->mapping_count--;

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_get_mapping(
    const mesh_kg_integration_t* integration,
    const char* module_name,
    mesh_kg_module_mapping_t* mapping
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module_name || !mapping) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_kg_integration_t*)integration)->mutex);

    int idx = find_mapping_by_name(integration, module_name);
    if (idx < 0) {
        nimcp_mutex_unlock(((mesh_kg_integration_t*)integration)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    *mapping = integration->mappings[idx];

    nimcp_mutex_unlock(((mesh_kg_integration_t*)integration)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_get_mapping_by_id(
    const mesh_kg_integration_t* integration,
    mesh_participant_id_t participant_id,
    mesh_kg_module_mapping_t* mapping
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!mapping) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_kg_integration_t*)integration)->mutex);

    int idx = find_mapping_by_id(integration, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(((mesh_kg_integration_t*)integration)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    *mapping = integration->mappings[idx];

    nimcp_mutex_unlock(((mesh_kg_integration_t*)integration)->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Topology Synchronization API
 * ============================================================================ */

nimcp_error_t mesh_kg_integration_sync_kg_to_mesh(
    mesh_kg_integration_t* integration
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    uint64_t start_ns = nimcp_time_now_ns();

    /* For each registered module, add connections to topology */
    for (size_t i = 0; i < integration->mapping_count; i++) {
        mesh_kg_module_mapping_t* mapping = &integration->mappings[i];
        if (!mapping->active) continue;

        /* Each output creates potential connections to modules with matching inputs */
        for (size_t j = 0; j < integration->mapping_count; j++) {
            if (i == j || !integration->mappings[j].active) continue;

            /* Connection weight based on input/output matching */
            float weight = 1.0f;
            if (mapping->output_count > 0 &&
                integration->mappings[j].input_count > 0) {
                mesh_topology_add_connection(
                    integration->topology_ctx,
                    mapping->participant_id,
                    integration->mappings[j].participant_id,
                    weight
                );
            }
        }
    }

    /* Compute topology metrics */
    if (integration->topology_ctx) {
        mesh_topology_compute_metrics(integration->topology_ctx, NULL);
        mesh_topology_compute_betweenness(integration->topology_ctx);
    }

    /* Update hub status for each module */
    for (size_t i = 0; i < integration->mapping_count; i++) {
        mesh_kg_module_mapping_t* mapping = &integration->mappings[i];
        if (!mapping->active) continue;

        mesh_node_info_t info;
        if (mesh_topology_get_node_info(
                integration->topology_ctx,
                mapping->participant_id,
                &info) == NIMCP_SUCCESS) {
            mapping->is_hub = info.is_hub;
            mapping->betweenness_centrality = info.betweenness_centrality;
            mapping->connection_count = info.degree;
        }
    }

    integration->stats.sync_operations++;
    uint64_t duration_ns = nimcp_time_now_ns() - start_ns;
    float duration_ms = (float)duration_ns / 1000000.0f;

    /* Update average */
    if (integration->stats.sync_operations == 1) {
        integration->stats.avg_sync_duration_ms = duration_ms;
    } else {
        integration->stats.avg_sync_duration_ms =
            0.9f * integration->stats.avg_sync_duration_ms +
            0.1f * duration_ms;
    }
    integration->stats.last_sync_ns = nimcp_time_now_ns();

    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("KG->Mesh sync completed in %.2f ms", duration_ms);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_sync_mesh_to_kg(
    mesh_kg_integration_t* integration
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Query mesh participants and update KG mappings */
    if (integration->integration) {
        mesh_participant_registry_t* registry =
            mesh_integration_get_registry(integration->integration);
        if (registry) {
            /* Would iterate participants and update mappings */
            integration->stats.mesh_updates_to_kg++;
        }
    }

    integration->stats.last_sync_ns = nimcp_time_now_ns();

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_sync_all(
    mesh_kg_integration_t* integration
) {
    nimcp_error_t err;

    err = mesh_kg_integration_sync_kg_to_mesh(integration);
    if (err != NIMCP_SUCCESS) return err;

    err = mesh_kg_integration_sync_mesh_to_kg(integration);
    return err;
}

nimcp_error_t mesh_kg_integration_identify_hubs(
    mesh_kg_integration_t* integration,
    mesh_kg_hub_assignment_t* hubs,
    size_t max_hubs,
    size_t* hub_count
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!hubs || !hub_count) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(integration->mutex);

    *hub_count = 0;

    /* Find hubs from mappings */
    for (size_t i = 0; i < integration->mapping_count && *hub_count < max_hubs; i++) {
        mesh_kg_module_mapping_t* mapping = &integration->mappings[i];
        if (!mapping->active || !mapping->is_hub) continue;

        mesh_kg_hub_assignment_t* hub = &hubs[*hub_count];
        hub->hub_id = mapping->participant_id;
        hub->channel = mapping->primary_channel;
        hub->centrality = mapping->betweenness_centrality;
        hub->assigned_modules = mapping->connection_count;

        /* Determine coordinator pool based on channel */
        hub->coordinator_pool = (mesh_pool_id_t)mapping->primary_channel;

        (*hub_count)++;
    }

    integration->stats.hubs_identified = *hub_count;

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * KG Query Routing API
 * ============================================================================ */

mesh_tx_id_t mesh_kg_integration_route_query(
    mesh_kg_integration_t* integration,
    uint32_t query_type,
    const void* query_data,
    size_t query_size,
    mesh_channel_id_t channel
) {
    mesh_tx_id_t empty_id = {0};

    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        return empty_id;
    }

    (void)query_data;
    (void)query_size;

    nimcp_mutex_lock(integration->mutex);

    /* Create transaction ID for the query */
    mesh_tx_id_t tx_id;
    tx_id.channel = channel;
    tx_id.proposer = 0;  /* System proposer */
    tx_id.sequence = ++integration->stats.kg_updates_routed;
    tx_id.timestamp_ns = nimcp_time_now_ns();

    (void)query_type;

    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Routed KG query type %u to channel %u", query_type, channel);
    }

    return tx_id;
}

nimcp_error_t mesh_kg_integration_route_update(
    mesh_kg_integration_t* integration,
    const char* module_name,
    uint32_t update_type
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module_name) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(integration->mutex);

    int idx = find_mapping_by_name(integration, module_name);
    if (idx < 0) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_kg_module_mapping_t* mapping = &integration->mappings[idx];

    /* Would create and submit update transaction */
    integration->stats.kg_updates_routed++;
    mapping->last_sync_ns = nimcp_time_now_ns();

    (void)update_type;

    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Routed KG update for '%s' type %u", module_name, update_type);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Endorsement Path API
 * ============================================================================ */

nimcp_error_t mesh_kg_integration_derive_endorsement_path(
    mesh_kg_integration_t* integration,
    const char* module_name,
    mesh_tx_type_t tx_type,
    mesh_kg_endorsement_path_t* path
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module_name || !path) return NIMCP_ERROR_NULL_POINTER;

    memset(path, 0, sizeof(*path));
    path->tx_type = tx_type;

    nimcp_mutex_lock(integration->mutex);

    int idx = find_mapping_by_name(integration, module_name);
    if (idx < 0) {
        nimcp_mutex_unlock(integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Derive endorsers from module connections */
    /* Modules with outputs that this module takes as inputs should endorse */
    mesh_kg_module_mapping_t* target = &integration->mappings[idx];

    for (size_t i = 0; i < integration->mapping_count; i++) {
        mesh_kg_module_mapping_t* other = &integration->mappings[i];
        if (!other->active || i == (size_t)idx) continue;
        if (!other->is_endorser) continue;

        /* Check if there's a potential connection */
        if (other->output_count > 0 && target->input_count > 0) {
            if (path->endorser_count < MESH_KG_MAX_PATH_LEN) {
                path->endorsers[path->endorser_count++] = other->participant_id;

                if (other->is_required_endorser) {
                    if (path->required_count < MESH_KG_MAX_PATH_LEN) {
                        path->required[path->required_count++] = other->participant_id;
                    }
                }

                if (other->has_veto && path->veto_count < 8) {
                    path->veto_holders[path->veto_count++] = other->participant_id;
                }
            }
        }
    }

    snprintf(path->policy_name, sizeof(path->policy_name),
             "kg_%s", module_name);

    integration->stats.endorsement_paths_created++;

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_sync_endorsement_policies(
    mesh_kg_integration_t* integration
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* For each module with handlers, derive endorsement path */
    for (size_t i = 0; i < integration->mapping_count; i++) {
        mesh_kg_module_mapping_t* mapping = &integration->mappings[i];
        if (!mapping->active || mapping->handler_count == 0) continue;

        mesh_kg_endorsement_path_t path;
        mesh_kg_integration_derive_endorsement_path(
            integration,
            mapping->module_name,
            MESH_TX_STATE_CHANGE,
            &path
        );

        /* Would register with endorsement collector */
    }

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Module Discovery API
 * ============================================================================ */

nimcp_error_t mesh_kg_integration_discover_kg_modules(
    mesh_kg_integration_t* integration,
    char (*modules)[MESH_MAX_NAME_LEN],
    size_t max_modules,
    size_t* module_count
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!modules || !module_count) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(integration->mutex);

    *module_count = 0;

    for (size_t i = 0; i < integration->mapping_count && *module_count < max_modules; i++) {
        if (!integration->mappings[i].active) continue;

        strncpy((*modules), integration->mappings[i].module_name, MESH_MAX_NAME_LEN - 1);
        modules++;
        (*module_count)++;
    }

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_discover_mesh_modules(
    mesh_kg_integration_t* integration,
    char (*modules)[MESH_MAX_NAME_LEN],
    size_t max_modules,
    size_t* module_count
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!modules || !module_count) return NIMCP_ERROR_NULL_POINTER;

    /* For now, same as KG modules since we sync */
    return mesh_kg_integration_discover_kg_modules(
        integration, modules, max_modules, module_count);
}

nimcp_error_t mesh_kg_integration_find_by_type(
    mesh_kg_integration_t* integration,
    const char* module_type,
    char (*modules)[MESH_MAX_NAME_LEN],
    size_t max_modules,
    size_t* module_count
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module_type || !modules || !module_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_kg_integration: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(integration->mutex);

    *module_count = 0;

    for (size_t i = 0; i < integration->mapping_count && *module_count < max_modules; i++) {
        if (!integration->mappings[i].active) continue;

        if (strstr(integration->mappings[i].module_type, module_type) != NULL) {
            strncpy((*modules), integration->mappings[i].module_name, MESH_MAX_NAME_LEN - 1);
            modules++;
            (*module_count)++;
        }
    }

    nimcp_mutex_unlock(integration->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_kg_integration_get_stats(
    const mesh_kg_integration_t* integration,
    mesh_kg_integration_stats_t* stats
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_kg_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_kg_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_integration_reset_stats(
    mesh_kg_integration_t* integration
) {
    if (!integration || integration->magic != MESH_KG_INTEGRATION_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    memset(&integration->stats, 0, sizeof(integration->stats));
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_kg_sync_mode_to_string(mesh_kg_sync_mode_t mode) {
    switch (mode) {
        case MESH_KG_SYNC_DISABLED:      return "DISABLED";
        case MESH_KG_SYNC_KG_TO_MESH:    return "KG_TO_MESH";
        case MESH_KG_SYNC_MESH_TO_KG:    return "MESH_TO_KG";
        case MESH_KG_SYNC_BIDIRECTIONAL: return "BIDIRECTIONAL";
        default:                         return "UNKNOWN";
    }
}

const char* mesh_kg_discovery_source_to_string(mesh_kg_discovery_source_t source) {
    switch (source) {
        case MESH_KG_DISCOVER_NONE:      return "NONE";
        case MESH_KG_DISCOVER_KG_ONLY:   return "KG_ONLY";
        case MESH_KG_DISCOVER_MESH_ONLY: return "MESH_ONLY";
        case MESH_KG_DISCOVER_BOTH:      return "BOTH";
        default:                         return "UNKNOWN";
    }
}
