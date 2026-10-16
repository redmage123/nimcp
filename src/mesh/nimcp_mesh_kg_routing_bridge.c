/**
 * @file nimcp_mesh_kg_routing_bridge.c
 * @brief Bridge between KG Module Wiring and Mesh Pattern Router
 *
 * WHAT: Integrates structural topology (KG) with pattern-based routing (Mesh)
 * WHY:  Combines crisp declarative routing with fuzzy learned routing
 * HOW:  KG provides priors, filtering, and validation for pattern matching
 */

#include "mesh/nimcp_mesh_kg_routing_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Registered module entry with both KG and pattern info
 */
typedef struct kg_bridge_module {
    mesh_participant_id_t id;
    kg_module_wiring_t wiring;
    mesh_receptive_field_t field;
    bool has_wiring;
    bool has_field;
    bool active;
} kg_bridge_module_t;

/**
 * @brief Topology cache entry
 */
typedef struct topology_cache_entry {
    mesh_participant_id_t source;
    mesh_participant_id_t neighbors[64];
    size_t neighbor_count;
    uint32_t max_hops;
    uint64_t timestamp;
    bool valid;
} topology_cache_entry_t;

/**
 * @brief Bridge structure
 */
struct mesh_kg_routing_bridge {
    bridge_base_t base;
    uint32_t magic;
    mesh_kg_bridge_config_t config;
    mesh_pattern_router_t* router;

    /* Registered modules */
    kg_bridge_module_t modules[MESH_KG_MAX_TOPOLOGY_CACHE];
    size_t module_count;

    /* Topology cache */
    topology_cache_entry_t topology_cache[64];
    size_t cache_count;

    /* Statistics */
    mesh_kg_bridge_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static kg_bridge_module_t* find_module(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t id
) {
    for (size_t i = 0; i < bridge->module_count; i++) {
        if (bridge->modules[i].id == id && bridge->modules[i].active) {
            return &bridge->modules[i];
        }
    }
    /* Not found is normal lookup result, not an error */
    return NULL;
}

static bool modules_connected_direct(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t from_id,
    mesh_participant_id_t to_id
) {
    kg_bridge_module_t* from_mod = find_module(bridge, from_id);
    kg_bridge_module_t* to_mod = find_module(bridge, to_id);

    if (!from_mod || !to_mod) {
        return false;
    }
    if (!from_mod->has_wiring || !to_mod->has_wiring) {
        return false;
    }

    /* Check if 'from' outputs to 'to' */
    for (uint32_t i = 0; i < from_mod->wiring.output_count; i++) {
        const char* out_type = from_mod->wiring.outputs[i].message_type;

        /* Check if 'to' handles this message type */
        for (uint32_t j = 0; j < to_mod->wiring.handler_count; j++) {
            if (strcmp(out_type, to_mod->wiring.handlers[j].message_type) == 0) {
                return true;
            }
        }

        /* Check if 'to' lists 'from' as input */
        for (uint32_t j = 0; j < to_mod->wiring.input_count; j++) {
            if (strcmp(from_mod->wiring.module_name,
                       to_mod->wiring.inputs[j].source_module) == 0) {
                return true;
            }
        }
    }

    /* Modules not directly connected - normal query result */
    return false;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

void mesh_kg_bridge_default_config(mesh_kg_bridge_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->mode = MESH_KG_ROUTE_HYBRID;
    config->structural_weight = MESH_KG_DEFAULT_STRUCTURAL_WEIGHT;
    config->pattern_weight = 1.0f - MESH_KG_DEFAULT_STRUCTURAL_WEIGHT;
    config->enable_topological_filter = true;
    config->max_hops = 2;
    config->enable_structural_validation = true;
    config->allow_novel_connections = true;
    config->learn_from_routing = true;
    config->learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config->enable_topology_cache = true;
    config->enable_logging = false;
}

mesh_kg_routing_bridge_t* mesh_kg_bridge_create(
    mesh_pattern_router_t* router,
    const mesh_kg_bridge_config_t* config
) {
    if (!router) {
        LOG_ERROR("Cannot create KG bridge without pattern router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_kg_bridge_create: router is NULL");
        return NULL;
    }

    mesh_kg_routing_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate KG routing bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_kg_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = MESH_KG_BRIDGE_MAGIC;
    bridge->router = router;

    if (config) {
        bridge->config = *config;
    } else {
        mesh_kg_bridge_default_config(&bridge->config);
    }

    /* Create recursive mutex (public functions call other public functions internally) */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        LOG_ERROR("Failed to create KG bridge mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_kg_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "mesh_kg_routing_bridge");

    LOG_INFO("Created KG-Mesh routing bridge (mode=%d)", bridge->config.mode);

    return bridge;
}

void mesh_kg_bridge_destroy(mesh_kg_routing_bridge_t* bridge) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) return;

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }

    bridge_base_cleanup(&bridge->base);
    bridge->magic = 0;
    nimcp_free(bridge);

    LOG_INFO("Destroyed KG-Mesh routing bridge");
}

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_register_module(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t module_id,
    const kg_module_wiring_t* wiring,
    const mesh_receptive_field_t* field
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Check if already registered */
    kg_bridge_module_t* existing = find_module(bridge, module_id);
    if (existing) {
        /* Update existing */
        if (wiring) {
            existing->wiring = *wiring;
            existing->has_wiring = true;
        }
        if (field) {
            existing->field = *field;
            existing->has_field = true;

            /* Also register with pattern router */
            mesh_pattern_router_register_receptive_field(
                bridge->router, module_id, field
            );
        }
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SUCCESS;
    }

    /* Add new */
    if (bridge->module_count >= MESH_KG_MAX_TOPOLOGY_CACHE) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_kg_routing_bridge: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    kg_bridge_module_t* mod = &bridge->modules[bridge->module_count++];
    mod->id = module_id;
    mod->active = true;

    if (wiring) {
        mod->wiring = *wiring;
        mod->has_wiring = true;
    }

    if (field) {
        mod->field = *field;
        mod->has_field = true;
        mesh_pattern_router_register_receptive_field(
            bridge->router, module_id, field
        );
    }

    bool enable_logging = bridge->config.enable_logging;
    nimcp_mutex_unlock(bridge->mutex);

    if (enable_logging) {
        LOG_DEBUG("Registered module 0x%llx with KG bridge (wiring=%d, field=%d)",
                  (unsigned long long)module_id, mod->has_wiring, mod->has_field);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_bridge_init_from_topology(
    mesh_kg_routing_bridge_t* bridge
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* For each module with wiring, bias pattern router based on connections */
    for (size_t i = 0; i < bridge->module_count; i++) {
        kg_bridge_module_t* mod = &bridge->modules[i];
        if (!mod->active || !mod->has_wiring || !mod->has_field) continue;

        /* For each input connection, find the source and use its pattern */
        for (uint32_t j = 0; j < mod->wiring.input_count; j++) {
            const char* source_name = mod->wiring.inputs[j].source_module;

            /* Find source module by name */
            for (size_t k = 0; k < bridge->module_count; k++) {
                kg_bridge_module_t* source = &bridge->modules[k];
                if (!source->active || !source->has_wiring || !source->has_field) continue;

                if (strcmp(source->wiring.module_name, source_name) == 0) {
                    /* Use source's preferred pattern to bias this module */
                    if (source->field.pattern_count > 0) {
                        mesh_pattern_router_update_receptive_field(
                            bridge->router,
                            mod->id,
                            &source->field.preferred[0],
                            bridge->config.learning_rate * bridge->config.structural_weight
                        );
                    }
                    break;
                }
            }
        }
    }

    size_t count = bridge->module_count;
    nimcp_mutex_unlock(bridge->mutex);

    LOG_INFO("Initialized pattern router from KG topology (%zu modules)", count);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Routing API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_route(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    mesh_activation_t* endorsers,
    size_t max_endorsers,
    size_t* count_out
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsers || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_kg_routing_bridge: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.total_routings++;

    switch (bridge->config.mode) {
    case MESH_KG_ROUTE_KG_ONLY:
        /* Fast path: only use KG handlers */
        /* TODO: Implement KG-only routing */
        bridge->stats.kg_fast_path++;
        break;

    case MESH_KG_ROUTE_PATTERN_ONLY:
        /* Pattern matching only */
        bridge->stats.pattern_only++;
        nimcp_mutex_unlock(bridge->mutex);
        return mesh_pattern_router_compute_activations(
            bridge->router, tx, endorsers, max_endorsers, count_out
        );

    case MESH_KG_ROUTE_KG_FILTER_PATTERN:
    case MESH_KG_ROUTE_HYBRID:
    case MESH_KG_ROUTE_PATTERN_VALIDATE_KG:
    default:
        bridge->stats.hybrid_routings++;
        break;
    }

    /* Hybrid routing: pattern match then validate */
    mesh_activation_t all_activations[128];
    size_t act_count = 0;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        bridge->router, tx, all_activations, 128, &act_count
    );
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->mutex);
        return err;
    }

    /* Filter by structural validity if enabled */
    if (bridge->config.enable_structural_validation) {
        err = mesh_kg_bridge_filter_by_structure(
            bridge, tx->proposer, all_activations, &act_count
        );
        if (err != NIMCP_SUCCESS) {
            nimcp_mutex_unlock(bridge->mutex);
            return err;
        }
    }

    /* Copy to output */
    size_t out_count = (act_count < max_endorsers) ? act_count : max_endorsers;
    memcpy(endorsers, all_activations, out_count * sizeof(mesh_activation_t));
    *count_out = out_count;

    /* Update stats */
    if (act_count > 0) {
        float total_sim = 0.0f;
        for (size_t i = 0; i < act_count; i++) {
            total_sim += all_activations[i].pattern_similarity;
        }
        bridge->stats.avg_pattern_similarity =
            (bridge->stats.avg_pattern_similarity + total_sim / act_count) / 2.0f;
    }

    bridge->stats.avg_endorsers_per_route =
        (bridge->stats.avg_endorsers_per_route + (float)out_count) / 2.0f;

    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_bridge_route_with_explanation(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    mesh_activation_t* endorsers,
    mesh_kg_routing_explanation_t* explanations,
    size_t max_endorsers,
    size_t* count_out
) {
    if (!bridge || !tx || !endorsers || !explanations || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* First do normal routing */
    nimcp_error_t err = mesh_kg_bridge_route(
        bridge, tx, endorsers, max_endorsers, count_out
    );
    if (err != NIMCP_SUCCESS) return err;

    /* Generate explanations */
    for (size_t i = 0; i < *count_out; i++) {
        err = mesh_kg_bridge_explain_routing(
            bridge, tx, endorsers[i].module_id, &explanations[i]
        );
        if (err != NIMCP_SUCCESS) {
            /* Fill with defaults on error */
            memset(&explanations[i], 0, sizeof(explanations[i]));
            explanations[i].module_id = endorsers[i].module_id;
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Topological Filtering API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_get_topological_neighbors(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t source_id,
    uint32_t max_hops,
    mesh_participant_id_t* neighbors,
    size_t max_neighbors,
    size_t* count_out
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!neighbors || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_kg_routing_bridge: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    nimcp_mutex_lock(bridge->mutex);

    /* Check cache first */
    if (bridge->config.enable_topology_cache) {
        for (size_t i = 0; i < bridge->cache_count; i++) {
            topology_cache_entry_t* entry = &bridge->topology_cache[i];
            if (entry->valid && entry->source == source_id &&
                entry->max_hops >= max_hops) {
                /* Cache hit */
                bridge->stats.topology_cache_hits++;
                size_t copy_count = entry->neighbor_count;
                if (copy_count > max_neighbors) copy_count = max_neighbors;
                memcpy(neighbors, entry->neighbors,
                       copy_count * sizeof(mesh_participant_id_t));
                *count_out = copy_count;
                nimcp_mutex_unlock(bridge->mutex);
                return NIMCP_SUCCESS;
            }
        }
        bridge->stats.topology_cache_misses++;
    }

    /* Compute neighbors (BFS) */
    bool visited[MESH_KG_MAX_TOPOLOGY_CACHE] = {false};
    mesh_participant_id_t queue[MESH_KG_MAX_TOPOLOGY_CACHE];
    uint32_t queue_hops[MESH_KG_MAX_TOPOLOGY_CACHE];
    size_t queue_head = 0, queue_tail = 0;

    /* Find source index */
    for (size_t i = 0; i < bridge->module_count; i++) {
        if (bridge->modules[i].id == source_id) {
            visited[i] = true;
            queue[queue_tail] = source_id;
            queue_hops[queue_tail] = 0;
            queue_tail++;
            break;
        }
    }

    while (queue_head < queue_tail && *count_out < max_neighbors) {
        mesh_participant_id_t current = queue[queue_head];
        uint32_t current_hops = queue_hops[queue_head];
        queue_head++;

        if (current_hops >= max_hops) continue;

        /* Find all direct connections */
        for (size_t i = 0; i < bridge->module_count; i++) {
            if (visited[i]) continue;
            if (!bridge->modules[i].active) continue;

            if (modules_connected_direct(bridge, current, bridge->modules[i].id)) {
                visited[i] = true;
                neighbors[*count_out] = bridge->modules[i].id;
                (*count_out)++;

                if (*count_out >= max_neighbors) break;

                queue[queue_tail] = bridge->modules[i].id;
                queue_hops[queue_tail] = current_hops + 1;
                queue_tail++;
            }
        }
    }

    /* Cache result */
    if (bridge->config.enable_topology_cache && bridge->cache_count < 64) {
        topology_cache_entry_t* entry = &bridge->topology_cache[bridge->cache_count++];
        entry->source = source_id;
        entry->max_hops = max_hops;
        entry->neighbor_count = *count_out;
        memcpy(entry->neighbors, neighbors,
               *count_out * sizeof(mesh_participant_id_t));
        entry->valid = true;
    }

    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

bool mesh_kg_bridge_has_connection(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t from_id,
    mesh_participant_id_t to_id
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        return false;
    }

    nimcp_mutex_lock(bridge->mutex);
    bool val = modules_connected_direct(bridge, from_id, to_id);
    nimcp_mutex_unlock(bridge->mutex);
    return val;
}

/* ============================================================================
 * Cross-Modal Discovery API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_find_convergence_points(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_participant_id_t* sources,
    size_t source_count,
    mesh_participant_id_t* convergence_points,
    size_t max_points,
    size_t* count_out
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!sources || !convergence_points || !count_out || source_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_kg_routing_bridge: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    nimcp_mutex_lock(bridge->mutex);

    /* For each module, check if it receives from ALL sources */
    for (size_t i = 0; i < bridge->module_count; i++) {
        kg_bridge_module_t* mod = &bridge->modules[i];
        if (!mod->active || !mod->has_wiring) continue;

        bool receives_from_all = true;

        for (size_t s = 0; s < source_count; s++) {
            bool receives_from_this = false;

            /* Check if any source connects to this module */
            for (size_t j = 0; j < bridge->module_count; j++) {
                if (bridge->modules[j].id == sources[s]) {
                    if (modules_connected_direct(bridge, sources[s], mod->id)) {
                        receives_from_this = true;
                        break;
                    }
                }
            }

            if (!receives_from_this) {
                receives_from_all = false;
                break;
            }
        }

        if (receives_from_all && *count_out < max_points) {
            convergence_points[*count_out] = mod->id;
            (*count_out)++;
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_bridge_suggest_multimodal_endorsers(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_t* patterns,
    const mesh_participant_id_t* pattern_sources,
    size_t pattern_count,
    mesh_activation_t* suggested,
    size_t max_suggested,
    size_t* count_out
) {
    if (!bridge || !patterns || !pattern_sources || !suggested || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *count_out = 0;

    nimcp_mutex_lock(bridge->mutex);

    /* Find convergence points */
    mesh_participant_id_t convergence[MESH_KG_MAX_CONVERGENCE_POINTS];
    size_t conv_count = 0;

    nimcp_error_t err = mesh_kg_bridge_find_convergence_points(
        bridge, pattern_sources, pattern_count,
        convergence, MESH_KG_MAX_CONVERGENCE_POINTS, &conv_count
    );
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->mutex);
        return err;
    }

    /* Blend patterns */
    mesh_pattern_t blended;
    float weights[16];
    for (size_t i = 0; i < pattern_count && i < 16; i++) {
        weights[i] = 1.0f;
    }
    mesh_pattern_blend(patterns, weights, pattern_count, &blended);

    /* Create activations for convergence points */
    for (size_t i = 0; i < conv_count && *count_out < max_suggested; i++) {
        kg_bridge_module_t* mod = find_module(bridge, convergence[i]);
        if (!mod || !mod->has_field) continue;

        /* Compute similarity to blended pattern */
        float sim = 0.0f;
        for (size_t p = 0; p < mod->field.pattern_count; p++) {
            float s = mesh_pattern_similarity(&blended, &mod->field.preferred[p]);
            if (s > sim) sim = s;
        }

        mesh_activation_t* act = &suggested[*count_out];
        act->module_id = convergence[i];
        act->activation_level = sim;
        act->confidence = 0.5f;  /* Medium confidence for suggestions */
        act->pattern_similarity = sim;
        act->should_endorse = true;
        (*count_out)++;
    }

    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Validation API
 * ============================================================================ */

bool mesh_kg_bridge_validate_activation(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t source_id,
    mesh_participant_id_t target_id,
    char* reason_out,
    size_t reason_size
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        if (reason_out && reason_size > 0) {
            snprintf(reason_out, reason_size, "Invalid bridge");
        }
        return false;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* If novel connections allowed, always valid */
    if (bridge->config.allow_novel_connections) {
        nimcp_mutex_unlock(bridge->mutex);
        return true;
    }

    /* Check for structural connection */
    if (modules_connected_direct(bridge, source_id, target_id)) {
        bridge->stats.validations_passed++;
        nimcp_mutex_unlock(bridge->mutex);
        return true;
    }

    /* Check within max_hops */
    mesh_participant_id_t neighbors[64];
    size_t neighbor_count = 0;
    mesh_kg_bridge_get_topological_neighbors(
        bridge, source_id, bridge->config.max_hops,
        neighbors, 64, &neighbor_count
    );

    for (size_t i = 0; i < neighbor_count; i++) {
        if (neighbors[i] == target_id) {
            bridge->stats.validations_passed++;
            nimcp_mutex_unlock(bridge->mutex);
            return true;
        }
    }

    /* Not found */
    bridge->stats.validations_failed++;
    nimcp_mutex_unlock(bridge->mutex);

    if (reason_out && reason_size > 0) {
        snprintf(reason_out, reason_size,
                 "No structural path from 0x%llx to 0x%llx within %u hops",
                 (unsigned long long)source_id,
                 (unsigned long long)target_id,
                 bridge->config.max_hops);
    }
    return false;
}

nimcp_error_t mesh_kg_bridge_filter_by_structure(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t source_id,
    mesh_activation_t* activations,
    size_t* count
) {
    if (!bridge || !activations || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    size_t write_idx = 0;
    for (size_t i = 0; i < *count; i++) {
        if (mesh_kg_bridge_validate_activation(
                bridge, source_id, activations[i].module_id, NULL, 0)) {
            if (write_idx != i) {
                activations[write_idx] = activations[i];
            }
            write_idx++;
        }
    }

    *count = write_idx;

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Learning API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_learn_outcome(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    const mesh_participant_id_t* endorsers,
    size_t endorser_count,
    bool success,
    float reward
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    if (!bridge->config.learn_from_routing) {
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_unlock(bridge->mutex);

    /* Delegate to pattern router */
    return mesh_pattern_router_learn_outcome(
        bridge->router, tx, endorsers, endorser_count, success, reward
    );
}

nimcp_error_t mesh_kg_bridge_strengthen_connection(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t from_id,
    mesh_participant_id_t to_id,
    float strength
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    kg_bridge_module_t* from_mod = find_module(bridge, from_id);
    kg_bridge_module_t* to_mod = find_module(bridge, to_id);

    if (!from_mod || !to_mod) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_routing_bridge: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!from_mod->has_field || !to_mod->has_field) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_kg_routing_bridge: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Update to_mod's receptive field to include from_mod's pattern */
    nimcp_error_t result = NIMCP_SUCCESS;
    if (from_mod->field.pattern_count > 0) {
        result = mesh_pattern_router_update_receptive_field(
            bridge->router,
            to_id,
            &from_mod->field.preferred[0],
            strength * bridge->config.learning_rate
        );
    }

    nimcp_mutex_unlock(bridge->mutex);
    return result;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_get_stats(
    mesh_kg_routing_bridge_t* bridge,
    mesh_kg_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_kg_bridge_reset_stats(mesh_kg_routing_bridge_t* bridge) {
    if (!bridge || bridge->magic != MESH_KG_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_bridge_reset_stats: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Introspection API
 * ============================================================================ */

nimcp_error_t mesh_kg_bridge_explain_routing(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    mesh_participant_id_t module_id,
    mesh_kg_routing_explanation_t* explanation
) {
    if (!bridge || !tx || !explanation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_kg_routing_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(explanation, 0, sizeof(*explanation));
    explanation->module_id = module_id;

    nimcp_mutex_lock(bridge->mutex);

    kg_bridge_module_t* mod = find_module(bridge, module_id);
    if (!mod) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_kg_routing_bridge: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Pattern-based explanation */
    if (mod->has_field && mod->field.pattern_count > 0) {
        explanation->pattern_similarity = mesh_pattern_similarity(
            &tx->content_pattern, &mod->field.preferred[0]
        );
        explanation->activation_level = explanation->pattern_similarity;
        explanation->selected_by_pattern =
            explanation->pattern_similarity >= mod->field.threshold;
    }

    /* Structural explanation */
    if (mod->has_wiring) {
        explanation->has_kg_connection =
            modules_connected_direct(bridge, tx->proposer, module_id);

        /* Check if handles any relevant message type */
        explanation->handles_message_type = mod->wiring.handler_count > 0;
        if (mod->wiring.handler_count > 0) {
            explanation->kg_handler_priority =
                mod->wiring.handlers[0].priority;
        }

        /* Build connection path description */
        kg_bridge_module_t* source = find_module(bridge, tx->proposer);
        if (source && source->has_wiring) {
            snprintf(explanation->connection_path,
                     sizeof(explanation->connection_path),
                     "%s -> %s",
                     source->wiring.module_name,
                     mod->wiring.module_name);
        }
    }

    /* Combined score */
    explanation->combined_score =
        bridge->config.pattern_weight * explanation->pattern_similarity +
        bridge->config.structural_weight * (explanation->has_kg_connection ? 1.0f : 0.0f);

    /* Validation */
    explanation->validated = mesh_kg_bridge_validate_activation(
        bridge, tx->proposer, module_id, NULL, 0
    );

    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

int mesh_kg_bridge_format_explanation(
    const mesh_kg_routing_explanation_t* explanation,
    char* buffer,
    size_t buffer_size
) {
    if (!explanation || !buffer || buffer_size == 0) {
        return 0;
    }

    return snprintf(buffer, buffer_size,
        "Module 0x%llx:\n"
        "  Pattern: similarity=%.3f, activation=%.3f, selected=%s\n"
        "  Structure: connection=%s, handles_type=%s, priority=%u\n"
        "  Path: %s\n"
        "  Combined: score=%.3f, validated=%s",
        (unsigned long long)explanation->module_id,
        explanation->pattern_similarity,
        explanation->activation_level,
        explanation->selected_by_pattern ? "yes" : "no",
        explanation->has_kg_connection ? "yes" : "no",
        explanation->handles_message_type ? "yes" : "no",
        explanation->kg_handler_priority,
        explanation->connection_path[0] ? explanation->connection_path : "(unknown)",
        explanation->combined_score,
        explanation->validated ? "yes" : "no"
    );
}
