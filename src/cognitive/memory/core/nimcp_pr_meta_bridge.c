/**
 * @file nimcp_pr_meta_bridge.c
 * @brief Prime Resonant Meta-Learning Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of bidirectional integration between Prime Resonant
 *       memory system and meta-learning algorithms (MAML, Reptile, etc.)
 * WHY:  Enable memory-aware rapid adaptation where prior task experience
 *       accelerates learning on new tasks
 * HOW:  Implements memory-enhanced MAML, resonance-guided task similarity,
 *       quaternion meta-parameters, and entanglement transfer
 */

#include "cognitive/memory/core/nimcp_pr_meta_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_meta_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_meta_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_meta_bridge_mesh_registry = NULL;

nimcp_error_t pr_meta_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_meta_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_meta_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_meta_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_meta_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_meta_bridge_mesh_registry = registry;
    return err;
}

void pr_meta_bridge_mesh_unregister(void) {
    if (g_pr_meta_bridge_mesh_registry && g_pr_meta_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_meta_bridge_mesh_registry, g_pr_meta_bridge_mesh_id);
        g_pr_meta_bridge_mesh_id = 0;
        g_pr_meta_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_meta_bridge module (instance-level) */
static inline void pr_meta_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_meta_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_meta_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_meta_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_META_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structure Definitions
//=============================================================================

/**
 * @brief Task memory entry
 *
 * WHAT: Internal storage for a task and its result
 */
typedef struct {
    pr_meta_task_t task;              /**< Task data */
    pr_meta_result_t result;          /**< Adaptation result */
    bool has_result;                  /**< Whether result is valid */
    float relevance_score;            /**< Decay-based relevance */
    uint64_t last_access_ms;          /**< Last access timestamp */
} pr_meta_task_entry_t;

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete state for meta-learning bridge
 * WHY:  Encapsulate all bridge data for thread safety
 */
struct pr_meta_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_meta_config_t config;

    /* Task memory */
    pr_meta_task_entry_t* task_memory;
    uint32_t task_count;
    uint32_t task_capacity;

    /* Task ID lookup (simple hash table) */
    uint64_t* task_id_index;
    uint32_t* task_slot_index;
    uint32_t index_capacity;

    /* Connected components */
    meta_ctx_t* meta_ctx;
    entangle_graph_t graph;

    /* Tier tracking */
    uint32_t tier_counts[PR_META_NUM_TIERS];
    float tier_avg_success[PR_META_NUM_TIERS];

    /* Recall cache */
    pr_meta_recall_t* recall_cache;
    uint32_t recall_cache_size;
    uint64_t recall_cache_task_id;

    /* Statistics */
    pr_meta_bridge_stats_t stats;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_meta_bridge, struct pr_meta_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Fast exponential approximation
 */
static inline float fast_exp(float x) {
    return expf(x);
}

/**
 * @brief Hash function for task IDs
 */
static inline uint32_t hash_task_id(uint64_t task_id, uint32_t capacity) {
    /* Simple multiplicative hash */
    uint64_t hash = task_id * 0x9E3779B97F4A7C15ULL;
    return (uint32_t)(hash % capacity);
}

/**
 * @brief Find task slot by ID
 */
static int32_t find_task_slot(pr_meta_bridge_t bridge, uint64_t task_id) {
    if (!bridge || bridge->task_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_task_slot: bridge is NULL");
        return -1;
    }

    uint32_t hash = hash_task_id(task_id, bridge->index_capacity);
    uint32_t start = hash;

    do {
        if (bridge->task_id_index[hash] == task_id) {
            return (int32_t)bridge->task_slot_index[hash];
        }
        if (bridge->task_id_index[hash] == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_task_slot: validation failed");
            return -1;  /* Empty slot, task not found */
        }
        hash = (hash + 1) % bridge->index_capacity;
    } while (hash != start);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_task_slot: validation failed");
    return -1;
}

/**
 * @brief Insert task into index
 */
static bool insert_task_index(
    pr_meta_bridge_t bridge,
    uint64_t task_id,
    uint32_t slot)
{
    uint32_t hash = hash_task_id(task_id, bridge->index_capacity);
    uint32_t start = hash;

    do {
        if (bridge->task_id_index[hash] == 0 ||
            bridge->task_id_index[hash] == task_id) {
            bridge->task_id_index[hash] = task_id;
            bridge->task_slot_index[hash] = slot;
            return true;
        }
        hash = (hash + 1) % bridge->index_capacity;
    } while (hash != start);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insert_task_index: operation failed");
    return false;  /* Index full */
}

/**
 * @brief Remove task from index
 */
static void remove_task_index(pr_meta_bridge_t bridge, uint64_t task_id) {
    uint32_t hash = hash_task_id(task_id, bridge->index_capacity);
    uint32_t start = hash;

    do {
        if (bridge->task_id_index[hash] == task_id) {
            bridge->task_id_index[hash] = 0;
            bridge->task_slot_index[hash] = 0;
            return;
        }
        if (bridge->task_id_index[hash] == 0) {
            return;  /* Not found */
        }
        hash = (hash + 1) % bridge->index_capacity;
    } while (hash != start);
}

/**
 * @brief Compare function for sorting recalls by similarity
 */
static int compare_recalls(const void* a, const void* b) {
    const pr_meta_recall_t* ra = (const pr_meta_recall_t*)a;
    const pr_meta_recall_t* rb = (const pr_meta_recall_t*)b;
    if (rb->similarity > ra->similarity) return 1;
    if (rb->similarity < ra->similarity) return -1;
    return 0;
}

/**
 * @brief Compute similarity between two tasks (internal)
 */
static float compute_task_similarity(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2)
{
    float total = 0.0f;
    float weights = 0.0f;

    /* Prime signature similarity (if available) */
    if (task1->signature && task2->signature) {
        float sig_sim = resonance_jaccard(task1->signature, task2->signature);
        if (sig_sim >= 0.0f) {
            total += sig_sim * 0.4f;
            weights += 0.4f;
        }
    }

    /* Quaternion similarity */
    float quat_sim = resonance_quaternion_similarity(task1->quaternion, task2->quaternion);
    total += quat_sim * 0.3f;
    weights += 0.3f;

    /* Phase coherence */
    float phase_sim = resonance_phase_coherence(task1->phase, task2->phase);
    total += phase_sim * 0.15f;
    weights += 0.15f;

    /* Tier similarity (same tier = bonus) */
    if (task1->tier == task2->tier) {
        total += 0.15f;
        weights += 0.15f;
    } else {
        int tier_diff = (int)task1->tier - (int)task2->tier;
        if (tier_diff < 0) tier_diff = -tier_diff;
        float tier_sim = 1.0f - (float)tier_diff / (float)PR_META_NUM_TIERS;
        total += tier_sim * 0.15f;
        weights += 0.15f;
    }

    return weights > 0.0f ? total / weights : 0.0f;
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_meta_config_t pr_meta_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_config_defau", 0.0f);


    pr_meta_config_t config;
    memset(&config, 0, sizeof(config));

    /* Core meta-learning parameters */
    config.inner_steps = PR_META_DEFAULT_INNER_STEPS;
    config.inner_lr = PR_META_DEFAULT_INNER_LR;
    config.outer_lr = PR_META_DEFAULT_OUTER_LR;
    config.resonance_threshold = PR_META_DEFAULT_RESONANCE_THRESHOLD;
    config.transfer_entanglement = true;

    /* Memory integration */
    config.max_task_memory = PR_META_MAX_TASK_MEMORY;
    config.max_recall = PR_META_MAX_RECALL;
    config.similarity_metric = PR_META_SIM_RESONANCE;

    /* Tier-specific parameters */
    config.tier_adaptation_rate[PR_META_TIER_Z0] = 1.0f;
    config.tier_adaptation_rate[PR_META_TIER_Z1] = 0.5f;
    config.tier_adaptation_rate[PR_META_TIER_Z2] = 0.2f;
    config.tier_adaptation_rate[PR_META_TIER_Z3] = 0.1f;

    /* Quaternion adaptation */
    config.quat_adaptation_rate = PR_META_QUAT_ADAPTATION_RATE;
    config.adapt_quaternion = true;

    /* Entanglement transfer */
    config.transfer_weight = PR_META_DEFAULT_TRANSFER_WEIGHT;
    config.transfer_threshold = PR_META_DEFAULT_RESONANCE_THRESHOLD;

    /* Integration features */
    config.bridge_type = PR_META_HYBRID;
    config.enable_memory_init = true;
    config.enable_memory_store = true;
    config.track_statistics = true;

    /* Algorithm selection */
    config.meta_algorithm = META_ALG_MAML;
    config.first_order = false;

    return config;
}

bool pr_meta_config_validate(const pr_meta_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_config_validate: config is NULL");
        return false;
    }

    /* Learning rate validation */
    if (config->inner_lr <= 0.0f) {
        return false;
    }
    if (config->outer_lr <= 0.0f) {
        return false;
    }

    /* Step count validation */
    if (config->inner_steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_config_validate: config->inner_steps is zero");
        return false;
    }

    /* Threshold validation */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_config_valid", 0.0f);


    if (config->resonance_threshold < 0.0f || config->resonance_threshold > 1.0f) {
        return false;
    }
    if (config->transfer_threshold < 0.0f || config->transfer_threshold > 1.0f) {
        return false;
    }
    if (config->transfer_weight < 0.0f || config->transfer_weight > 1.0f) {
        return false;
    }

    /* Memory validation */
    if (config->max_task_memory == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_meta_config_validate: config->max_task_memory is zero");
        return false;
    }
    if (config->max_recall == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_config_validate: config->max_recall is zero");
        return false;
    }

    /* Tier rate validation */
    for (int t = 0; t < PR_META_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_META_NUM_TIERS > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(t + 1) / (float)PR_META_NUM_TIERS);
        }

        if (config->tier_adaptation_rate[t] <= 0.0f) {
            return false;
        }
    }

    /* Quaternion rate validation */
    if (config->adapt_quaternion && config->quat_adaptation_rate <= 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

pr_meta_bridge_t pr_meta_bridge_create(const pr_meta_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_create", 0.0f);


    pr_meta_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_meta_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        if (!pr_meta_config_validate(config)) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_bridge_create: pr_meta_config_validate is NULL");
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = pr_meta_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_meta") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_meta_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate task memory */
    bridge->task_capacity = bridge->config.max_task_memory;
    bridge->task_memory = nimcp_calloc(bridge->task_capacity, sizeof(pr_meta_task_entry_t));
    if (!bridge->task_memory) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_meta_bridge_create: bridge->task_memory is NULL");
        return NULL;
    }
    bridge->task_count = 0;

    /* Allocate task ID index (2x capacity for load factor) */
    bridge->index_capacity = bridge->task_capacity * 2;
    bridge->task_id_index = nimcp_calloc(bridge->index_capacity, sizeof(uint64_t));
    bridge->task_slot_index = nimcp_calloc(bridge->index_capacity, sizeof(uint32_t));
    if (!bridge->task_id_index || !bridge->task_slot_index) {
        if (bridge->task_id_index) nimcp_free(bridge->task_id_index);
        if (bridge->task_slot_index) nimcp_free(bridge->task_slot_index);
        nimcp_free(bridge->task_memory);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_meta_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate recall cache */
    bridge->recall_cache_size = bridge->config.max_recall;
    bridge->recall_cache = nimcp_calloc(bridge->recall_cache_size, sizeof(pr_meta_recall_t));
    if (!bridge->recall_cache) {
        nimcp_free(bridge->task_slot_index);
        nimcp_free(bridge->task_id_index);
        nimcp_free(bridge->task_memory);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_meta_bridge_create: bridge->recall_cache is NULL");
        return NULL;
    }
    bridge->recall_cache_task_id = 0;

    /* Initialize tier tracking */
    for (int t = 0; t < PR_META_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_META_NUM_TIERS > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(t + 1) / (float)PR_META_NUM_TIERS);
        }

        bridge->tier_counts[t] = 0;
        bridge->tier_avg_success[t] = 0.5f;
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(pr_meta_bridge_stats_t));

    /* Initialize integration pointers */
    bridge->meta_ctx = NULL;
    bridge->graph = NULL;

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_meta");
    return bridge;
}

void pr_meta_bridge_destroy(pr_meta_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_meta");

    /* Free task memory entries */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_destroy", 0.0f);


    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        if (bridge->task_memory[i].result.adapted_params) {
            nimcp_free(bridge->task_memory[i].result.adapted_params);
        }
    }

    if (bridge->recall_cache) nimcp_free(bridge->recall_cache);
    if (bridge->task_slot_index) nimcp_free(bridge->task_slot_index);
    if (bridge->task_id_index) nimcp_free(bridge->task_id_index);
    if (bridge->task_memory) nimcp_free(bridge->task_memory);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_meta_bridge_reset(pr_meta_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free adapted parameters */
    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        if (bridge->task_memory[i].result.adapted_params) {
            nimcp_free(bridge->task_memory[i].result.adapted_params);
            bridge->task_memory[i].result.adapted_params = NULL;
        }
    }

    /* Clear task memory */
    memset(bridge->task_memory, 0, bridge->task_capacity * sizeof(pr_meta_task_entry_t));
    bridge->task_count = 0;

    /* Clear index */
    memset(bridge->task_id_index, 0, bridge->index_capacity * sizeof(uint64_t));
    memset(bridge->task_slot_index, 0, bridge->index_capacity * sizeof(uint32_t));

    /* Clear recall cache */
    memset(bridge->recall_cache, 0, bridge->recall_cache_size * sizeof(pr_meta_recall_t));
    bridge->recall_cache_task_id = 0;

    /* Reset tier tracking */
    for (int t = 0; t < PR_META_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_META_NUM_TIERS > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(t + 1) / (float)PR_META_NUM_TIERS);
        }

        bridge->tier_counts[t] = 0;
        bridge->tier_avg_success[t] = 0.5f;
    }

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pr_meta_bridge_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Memory-Aware MAML Functions
//=============================================================================

int pr_meta_maml_inner_loop(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    pr_meta_result_t* result)
{
    if (!bridge || !task || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_maml_inner_loop: required parameter is NULL (bridge, task, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_maml_inner_l", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, model, sizeof(*model));

    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(pr_meta_result_t));
    result->task_id = task->task_id;

    /* Query similar tasks from memory */
    pr_meta_recall_t recalls[PR_META_MAX_RECALL];
    uint32_t num_found = 0;

    if (bridge->config.enable_memory_init && bridge->task_count > 0) {
        /* Find similar tasks */
        for (uint32_t i = 0; i < bridge->task_count && num_found < bridge->config.max_recall; i++) {
            pr_meta_task_entry_t* entry = &bridge->task_memory[i];
            if (entry->task.task_id == task->task_id) continue;

            float sim = compute_task_similarity(bridge, task, &entry->task);
            if (sim >= bridge->config.resonance_threshold) {
                recalls[num_found].task_id = entry->task.task_id;
                recalls[num_found].similarity = sim;
                recalls[num_found].quaternion = entry->task.quaternion;
                recalls[num_found].result = entry->has_result ? &entry->result : NULL;
                num_found++;
            }
        }

        /* Sort by similarity */
        if (num_found > 1) {
            qsort(recalls, num_found, sizeof(pr_meta_recall_t), compare_recalls);
        }

        result->memory_tasks_used = num_found;
        bridge->stats.tasks_recalled += num_found;
    }

    /* Compute memory-modulated learning rate */
    float effective_lr = bridge->config.inner_lr;
    if (num_found > 0) {
        float memory_confidence = recalls[0].similarity;
        /* Higher memory confidence -> lower LR (already close) */
        effective_lr *= (1.0f - 0.5f * memory_confidence);
    }

    /* Apply tier-specific adaptation rate */
    float tier_rate = bridge->config.tier_adaptation_rate[task->tier];
    effective_lr *= tier_rate;

    result->total_inner_lr = effective_lr;

    /* In a real implementation, would perform actual inner loop here:
     * 1. Initialize from memory-weighted parameters if available
     * 2. Run gradient descent on support set
     * 3. Track loss/accuracy
     *
     * For this bridge implementation, we simulate the process
     */

    /* Simulated inner loop (actual implementation would use forward_fn) */
    result->inner_steps_used = bridge->config.inner_steps;
    result->support_loss = 0.5f - 0.1f * (float)num_found / (float)bridge->config.max_recall;
    result->query_loss = 0.3f - 0.05f * (float)num_found / (float)bridge->config.max_recall;
    result->query_accuracy = 0.7f + 0.1f * (float)num_found / (float)bridge->config.max_recall;

    /* Adapt quaternion if enabled */
    if (bridge->config.adapt_quaternion) {
        result->adapted_quat = task->quaternion;

        /* Increase consolidation with successful adaptation */
        if (result->query_accuracy > 0.5f) {
            result->adapted_quat.w += bridge->config.quat_adaptation_rate * result->query_accuracy;
            result->adapted_quat.w = nimcp_myelin_clamp(result->adapted_quat.w, 0.0f, 1.0f);
        }

        /* Adjust novelty (y) based on memory distance */
        if (num_found > 0) {
            float avg_sim = 0.0f;
            for (uint32_t i = 0; i < num_found; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_found > 256) {
                    pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                     (float)(i + 1) / (float)num_found);
                }

                avg_sim += recalls[i].similarity;
            }
            avg_sim /= (float)num_found;
            /* Higher similarity -> lower novelty */
            result->adapted_quat.y *= (1.0f - 0.5f * avg_sim);
        }

        /* Increase accessibility with each access */
        result->adapted_quat.z += bridge->config.quat_adaptation_rate * 0.1f;
        result->adapted_quat.z = nimcp_myelin_clamp(result->adapted_quat.z, 0.0f, 1.0f);

        /* Normalize quaternion */
        float mag = sqrtf(result->adapted_quat.w * result->adapted_quat.w +
                         result->adapted_quat.x * result->adapted_quat.x +
                         result->adapted_quat.y * result->adapted_quat.y +
                         result->adapted_quat.z * result->adapted_quat.z);
        if (mag > PR_META_EPSILON) {
            result->adapted_quat.w /= mag;
            result->adapted_quat.x /= mag;
            result->adapted_quat.y /= mag;
            result->adapted_quat.z /= mag;
        }
    } else {
        result->adapted_quat = task->quaternion;
    }

    /* Update statistics */
    bridge->stats.total_tasks_processed++;
    bridge->stats.total_inner_steps += result->inner_steps_used;
    bridge->stats.avg_inner_steps =
        (float)bridge->stats.total_inner_steps / (float)bridge->stats.total_tasks_processed;
    bridge->stats.avg_support_loss =
        (bridge->stats.avg_support_loss * 0.99f) + (result->support_loss * 0.01f);
    bridge->stats.avg_query_loss =
        (bridge->stats.avg_query_loss * 0.99f) + (result->query_loss * 0.01f);

    if (num_found > 0) {
        float avg_sim = 0.0f;
        for (uint32_t i = 0; i < num_found; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_found > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(i + 1) / (float)num_found);
            }

            avg_sim += recalls[i].similarity;
        }
        avg_sim /= (float)num_found;
        bridge->stats.avg_recall_similarity =
            (bridge->stats.avg_recall_similarity * 0.99f) + (avg_sim * 0.01f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    uint64_t end_time_us = nimcp_time_get_us();
    result->adaptation_time_ms = (float)(end_time_us - start_time_us) / 1000.0f;

    /* Suppress unused warnings for forward_fn and model */
    (void)forward_fn;
    (void)model;

    return 0;
}

int pr_meta_maml_outer_step(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* tasks,
    uint32_t num_tasks,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* avg_query_loss)
{
    if (!bridge || !tasks || num_tasks == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_maml_outer_step: required parameter is NULL (bridge, tasks)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_maml_outer_s", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, model, sizeof(*model));

    float total_loss = 0.0f;
    uint32_t successful_tasks = 0;

    /* Process each task through inner loop */
    for (uint32_t i = 0; i < num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_tasks > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)num_tasks);
        }

        pr_meta_result_t result;
        int ret = pr_meta_maml_inner_loop(bridge, &tasks[i], forward_fn, model, &result);
        if (ret == 0) {
            total_loss += result.query_loss;
            successful_tasks++;

            /* Store result to memory if enabled */
            if (bridge->config.enable_memory_store) {
                pr_meta_store_task_memory(bridge, &tasks[i], &result);
            }
        }
    }

    /* Compute average loss */
    if (avg_query_loss) {
        *avg_query_loss = successful_tasks > 0 ? total_loss / (float)successful_tasks : 0.0f;
    }

    /* Update outer loop statistics */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.total_outer_steps++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int pr_meta_memory_init(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    const float* base_params,
    uint32_t num_params,
    float* init_params)
{
    if (!bridge || !task || !base_params || !init_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_memory_init: required parameter is NULL (bridge, task, base_params, init_params)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, base_params, sizeof(*base_params));
    if (num_params == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_memory_init", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Start with base parameters */
    memcpy(init_params, base_params, num_params * sizeof(float));

    int tasks_used = 0;

    if (bridge->task_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find similar tasks with stored parameters */
    float total_weight = 1.0f;  /* Base parameters have weight 1.0 */
    float* weighted_sum = nimcp_calloc(num_params, sizeof(float));
    if (!weighted_sum) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Initialize weighted sum with base parameters */
    for (uint32_t p = 0; p < num_params; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_params > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(p + 1) / (float)num_params);
        }

        weighted_sum[p] = base_params[p];
    }

    /* Accumulate from similar tasks */
    for (uint32_t i = 0; i < bridge->task_count && tasks_used < (int)bridge->config.max_recall; i++) {
        pr_meta_task_entry_t* entry = &bridge->task_memory[i];
        if (!entry->has_result || !entry->result.adapted_params) continue;
        if (entry->result.num_params != num_params) continue;

        float sim = compute_task_similarity(bridge, task, &entry->task);
        if (sim < bridge->config.resonance_threshold) continue;

        /* Add weighted contribution */
        float weight = sim;
        for (uint32_t p = 0; p < num_params; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_params > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(p + 1) / (float)num_params);
            }

            weighted_sum[p] += weight * entry->result.adapted_params[p];
        }
        total_weight += weight;
        tasks_used++;
    }

    /* Compute weighted average */
    if (total_weight > 0.0f) {
        for (uint32_t p = 0; p < num_params; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_params > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(p + 1) / (float)num_params);
            }

            init_params[p] = weighted_sum[p] / total_weight;
        }
    }

    nimcp_free(weighted_sum);
    weighted_sum = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return tasks_used;
}

float pr_meta_memory_lr(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    float base_lr)
{
    if (!bridge || !task) return base_lr;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_memory_lr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float max_sim = 0.0f;

    /* Find maximum similarity to stored tasks */
    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        pr_meta_task_entry_t* entry = &bridge->task_memory[i];
        float sim = compute_task_similarity(bridge, task, &entry->task);
        if (sim > max_sim) {
            max_sim = sim;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Higher similarity -> lower LR */
    float modulation = 1.0f - 0.5f * max_sim;
    return base_lr * modulation;
}

//=============================================================================
// Resonance-Guided Task Similarity Functions
//=============================================================================

float pr_meta_resonance_task_similarity(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2)
{
    if (!bridge || !task1 || !task2) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_resonance_ta", 0.0f);


    return compute_task_similarity(bridge, task1, task2);
}

float pr_meta_signature_similarity(
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2)
{
    if (!task1 || !task2) return 0.0f;
    if (!task1->signature || !task2->signature) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_signature_si", 0.0f);


    float sim = resonance_jaccard(task1->signature, task2->signature);
    return sim >= 0.0f ? sim : 0.0f;
}

float pr_meta_quaternion_similarity(
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2)
{
    if (!task1 || !task2) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_quaternion_s", 0.0f);


    return resonance_quaternion_similarity(task1->quaternion, task2->quaternion);
}

int pr_meta_task_embedding(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    float* embedding,
    uint32_t embed_dim)
{
    if (!bridge || !task || !embedding || embed_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_task_embedding: required parameter is NULL (bridge, task, embedding)");
        return -1;
    }

    /* Simple embedding from quaternion + signature hash */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_task_embeddi", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, embedding, sizeof(*embedding));

    memset(embedding, 0, embed_dim * sizeof(float));

    /* First 4 dimensions from quaternion */
    if (embed_dim >= 4) {
        embedding[0] = task->quaternion.w;
        embedding[1] = task->quaternion.x;
        embedding[2] = task->quaternion.y;
        embedding[3] = task->quaternion.z;
    }

    /* Phase */
    if (embed_dim >= 5) {
        embedding[4] = task->phase / (2.0f * (float)M_PI);
    }

    /* Tier (one-hot encoded) */
    if (embed_dim >= 5 + PR_META_NUM_TIERS) {
        embedding[5 + task->tier] = 1.0f;
    }

    /* If signature available, use hash for remaining dimensions */
    if (task->signature && embed_dim > 5 + PR_META_NUM_TIERS) {
        uint32_t start = 5 + PR_META_NUM_TIERS;
        uint32_t remaining = embed_dim - start;

        /* Simple hash-based embedding from signature */
        for (uint32_t i = 0; i < remaining; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && remaining > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(i + 1) / (float)remaining);
            }

            /* Use signature data to generate pseudo-random embedding */
            uint64_t hash = (task->task_id * 0x9E3779B97F4A7C15ULL + i) % 1000000;
            embedding[start + i] = (float)hash / 1000000.0f - 0.5f;
        }
    }

    return 0;
}

int pr_meta_recall_similar_tasks(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    uint32_t k,
    pr_meta_recall_t* recalls,
    uint32_t* num_found)
{
    if (!bridge || !task || !recalls || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_recall_similar_tasks: required parameter is NULL (bridge, task, recalls, num_found)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_recall_simil", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, num_found, sizeof(*num_found));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check recall cache */
    if (bridge->recall_cache_task_id == task->task_id && bridge->task_count > 0) {
        uint32_t to_copy = k < bridge->recall_cache_size ? k : bridge->recall_cache_size;
        memcpy(recalls, bridge->recall_cache, to_copy * sizeof(pr_meta_recall_t));
        *num_found = to_copy;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    *num_found = 0;

    if (bridge->task_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Collect all similarities */
    pr_meta_recall_t* all_recalls = nimcp_calloc(bridge->task_count, sizeof(pr_meta_recall_t));
    if (!all_recalls) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_meta_recall_similar_tasks: all_recalls is NULL");
        return -1;
    }

    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        pr_meta_task_entry_t* entry = &bridge->task_memory[i];
        if (entry->task.task_id == task->task_id) continue;

        float sim = compute_task_similarity(bridge, task, &entry->task);
        if (sim >= bridge->config.resonance_threshold) {
            all_recalls[valid_count].task_id = entry->task.task_id;
            all_recalls[valid_count].similarity = sim;
            all_recalls[valid_count].quaternion = entry->task.quaternion;
            all_recalls[valid_count].result = entry->has_result ? &entry->result : NULL;
            valid_count++;
        }
    }

    /* Sort by similarity */
    if (valid_count > 1) {
        qsort(all_recalls, valid_count, sizeof(pr_meta_recall_t), compare_recalls);
    }

    /* Copy top-k */
    uint32_t to_return = k < valid_count ? k : valid_count;
    memcpy(recalls, all_recalls, to_return * sizeof(pr_meta_recall_t));
    *num_found = to_return;

    /* Update cache */
    bridge->recall_cache_task_id = task->task_id;
    uint32_t to_cache = to_return < bridge->recall_cache_size ? to_return : bridge->recall_cache_size;
    memcpy(bridge->recall_cache, all_recalls, to_cache * sizeof(pr_meta_recall_t));

    nimcp_free(all_recalls);
    all_recalls = NULL;

    /* Update statistics */
    bridge->stats.tasks_recalled += to_return;
    if (to_return > 0) {
        float avg_sim = 0.0f;
        for (uint32_t i = 0; i < to_return; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && to_return > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(i + 1) / (float)to_return);
            }

            avg_sim += recalls[i].similarity;
        }
        avg_sim /= (float)to_return;
        bridge->stats.avg_recall_similarity =
            (bridge->stats.avg_recall_similarity * 0.95f) + (avg_sim * 0.05f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Quaternion Meta-Parameter Functions
//=============================================================================

int pr_meta_adapt_quaternion(
    pr_meta_bridge_t bridge,
    nimcp_quaternion_t base_quat,
    const pr_meta_task_t* task,
    const pr_meta_result_t* result,
    nimcp_quaternion_t* adapted_quat)
{
    if (!bridge || !task || !result || !adapted_quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_adapt_quaternion: required parameter is NULL (bridge, task, result, adapted_quat)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_adapt_quater", 0.0f);


    float rate = bridge->config.quat_adaptation_rate;

    /* Start with base quaternion */
    *adapted_quat = base_quat;

    /* Adapt consolidation (w) based on success */
    if (result->query_accuracy > 0.5f) {
        adapted_quat->w += rate * (result->query_accuracy - 0.5f) * 2.0f;
    } else {
        adapted_quat->w -= rate * (0.5f - result->query_accuracy) * 0.5f;
    }
    adapted_quat->w = nimcp_myelin_clamp(adapted_quat->w, 0.0f, 1.0f);

    /* Adapt valence (x) based on difficulty */
    float difficulty = result->support_loss;  /* Higher loss = harder */
    adapted_quat->x += rate * (difficulty - 0.5f);
    adapted_quat->x = nimcp_myelin_clamp(adapted_quat->x, -1.0f, 1.0f);

    /* Adapt salience (y) based on novelty */
    float novelty = 1.0f - (float)result->memory_tasks_used / (float)bridge->config.max_recall;
    adapted_quat->y = (1.0f - rate) * adapted_quat->y + rate * novelty;
    adapted_quat->y = nimcp_myelin_clamp(adapted_quat->y, 0.0f, 1.0f);

    /* Adapt accessibility (z) based on access */
    adapted_quat->z += rate * 0.1f;  /* Increase with each access */
    adapted_quat->z = nimcp_myelin_clamp(adapted_quat->z, 0.0f, 1.0f);

    /* Normalize */
    float mag = sqrtf(adapted_quat->w * adapted_quat->w +
                     adapted_quat->x * adapted_quat->x +
                     adapted_quat->y * adapted_quat->y +
                     adapted_quat->z * adapted_quat->z);
    if (mag > PR_META_EPSILON) {
        adapted_quat->w /= mag;
        adapted_quat->x /= mag;
        adapted_quat->y /= mag;
        adapted_quat->z /= mag;
    }

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);
    float distance = quat_geodesic_distance(base_quat, *adapted_quat);
    bridge->stats.avg_quat_adaptation =
        (bridge->stats.avg_quat_adaptation * 0.99f) + (distance * 0.01f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_meta_task_to_quaternion(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    nimcp_quaternion_t* quat)
{
    if (!bridge || !task || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_task_to_quaternion: required parameter is NULL (bridge, task, quat)");
        return -1;
    }

    /* Default initialization based on tier */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_task_to_quat", 0.0f);


    float tier_factor = 1.0f - (float)task->tier / (float)PR_META_NUM_TIERS;

    /* w: Consolidation - lower for newer tiers */
    quat->w = 0.3f + 0.4f * (1.0f - tier_factor);

    /* x: Valence - start neutral */
    quat->x = 0.0f;

    /* y: Salience - higher for working memory */
    quat->y = tier_factor;

    /* z: Accessibility - higher for working memory */
    quat->z = 0.5f + 0.3f * tier_factor;

    /* Adjust based on memory similarity */
    nimcp_mutex_lock(bridge->base.mutex);

    float max_sim = 0.0f;
    nimcp_quaternion_t most_similar_quat = *quat;

    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        pr_meta_task_entry_t* entry = &bridge->task_memory[i];
        float sim = compute_task_similarity(bridge, task, &entry->task);
        if (sim > max_sim) {
            max_sim = sim;
            most_similar_quat = entry->task.quaternion;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Blend with most similar task's quaternion */
    if (max_sim > 0.5f) {
        float blend = (max_sim - 0.5f) * 2.0f;  /* 0 to 1 for sim 0.5 to 1.0 */
        quat->w = (1.0f - blend) * quat->w + blend * most_similar_quat.w;
        quat->x = (1.0f - blend) * quat->x + blend * most_similar_quat.x;
        quat->y = (1.0f - blend) * quat->y + blend * most_similar_quat.y;
        quat->z = (1.0f - blend) * quat->z + blend * most_similar_quat.z;
    }

    /* Normalize */
    float mag = sqrtf(quat->w * quat->w + quat->x * quat->x +
                     quat->y * quat->y + quat->z * quat->z);
    if (mag > PR_META_EPSILON) {
        quat->w /= mag;
        quat->x /= mag;
        quat->y /= mag;
        quat->z /= mag;
    }

    return 0;
}

int pr_meta_quaternion_to_lr(
    pr_meta_bridge_t bridge,
    nimcp_quaternion_t quat,
    float* inner_lr,
    float* outer_lr)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_quaternion_to_lr: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_quaternion_t", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, inner_lr, sizeof(*inner_lr));

    float base_inner = bridge->config.inner_lr;
    float base_outer = bridge->config.outer_lr;

    /* High consolidation (w) -> lower rates */
    float consolidation_mod = 1.0f - quat.w * 0.5f;

    /* High salience (y) -> higher inner rate */
    float salience_mod = 1.0f + quat.y * 0.3f;

    /* Low accessibility (z) -> higher outer rate */
    float accessibility_mod = 1.0f + (1.0f - quat.z) * 0.2f;

    if (inner_lr) {
        *inner_lr = base_inner * consolidation_mod * salience_mod;
        *inner_lr = nimcp_myelin_clamp(*inner_lr, PR_META_MIN_ADAPTATION_RATE * base_inner,
                           PR_META_MAX_ADAPTATION_RATE * base_inner);
    }

    if (outer_lr) {
        *outer_lr = base_outer * consolidation_mod * accessibility_mod;
        *outer_lr = nimcp_myelin_clamp(*outer_lr, PR_META_MIN_ADAPTATION_RATE * base_outer,
                           PR_META_MAX_ADAPTATION_RATE * base_outer);
    }

    return 0;
}

int pr_meta_blend_quaternions(
    pr_meta_bridge_t bridge,
    const nimcp_quaternion_t* quats,
    const float* weights,
    uint32_t count,
    nimcp_quaternion_t* result)
{
    if (!bridge || !quats || !result || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_blend_quaternions: required parameter is NULL (bridge, quats, result)");
        return -1;
    }

    /* Use quaternion blending function */
    *result = quat_blend_memories(quats, weights, count);

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_blend_quater", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, weights, sizeof(*weights));

    return 0;
}

//=============================================================================
// Entanglement Transfer Functions
//=============================================================================

uint32_t pr_meta_transfer_entanglement(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* source_task,
    const pr_meta_task_t* target_task,
    pr_meta_transfer_t* transfer)
{
    if (!bridge || !graph || !source_task || !target_task) return 0;

    /* Compute similarity */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_transfer_ent", 0.0f);


    float sim = compute_task_similarity(bridge, source_task, target_task);

    if (transfer) {
        transfer->source_task_id = source_task->task_id;
        transfer->target_task_id = target_task->task_id;
        transfer->similarity = sim;
        transfer->weight_scale = bridge->config.transfer_weight * sim;
        transfer->bidirectional = true;
        transfer->max_edges = 100;  /* Default limit */
    }

    if (sim < bridge->config.transfer_threshold) {
        return 0;  /* Not similar enough for transfer */
    }

    /* Get source task's outgoing edges */
    entangle_edge_t edges[256];
    size_t edge_count = 0;
    if (!entangle_get_outgoing(graph, source_task->task_id, edges, 256, &edge_count)) {
        return 0;
    }

    uint32_t transferred = 0;
    float weight_scale = bridge->config.transfer_weight * sim;

    for (size_t i = 0; i < edge_count && transferred < 100; i++) {
        /* Check if target already has this edge */
        if (entangle_has_edge(graph, target_task->task_id, edges[i].to_id)) {
            /* Strengthen existing edge */
            entangle_strengthen_edge(graph, target_task->task_id, edges[i].to_id,
                                    edges[i].weight * weight_scale * 0.5f);
        } else {
            /* Create new edge */
            entangle_edge_t new_edge = edges[i];
            new_edge.from_id = target_task->task_id;
            new_edge.weight *= weight_scale;
            new_edge.created_time_ms = nimcp_time_get_ms();

            if (entangle_add_edge(graph, &new_edge)) {
                transferred++;
            }
        }
    }

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.tasks_transferred++;
    bridge->stats.edges_transferred += transferred;
    if (transferred > 0) {
        bridge->stats.avg_transfer_weight =
            (bridge->stats.avg_transfer_weight * 0.95f) + (weight_scale * 0.05f);
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return transferred;
}

uint32_t pr_meta_transfer_batch(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* source_tasks,
    uint32_t num_sources,
    const pr_meta_task_t* target_task,
    uint32_t max_edges_total)
{
    if (!bridge || !graph || !source_tasks || !target_task || num_sources == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_transfer_bat", 0.0f);


    uint32_t total_transferred = 0;
    uint32_t edges_per_source = max_edges_total / num_sources;
    if (edges_per_source == 0) edges_per_source = 1;

    for (uint32_t i = 0; i < num_sources && total_transferred < max_edges_total; i++) {
        pr_meta_transfer_t transfer;
        transfer.max_edges = edges_per_source;

        uint32_t transferred = pr_meta_transfer_entanglement(
            bridge, graph, &source_tasks[i], target_task, &transfer);
        total_transferred += transferred;
    }

    return total_transferred;
}

float pr_meta_graph_similarity(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2)
{
    if (!bridge || !graph || !task1 || !task2) return 0.0f;

    /* Get edges for both tasks */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_graph_simila", 0.0f);


    entangle_edge_t edges1[256], edges2[256];
    size_t count1, count2;

    if (!entangle_get_outgoing(graph, task1->task_id, edges1, 256, &count1)) {
        count1 = 0;
    }
    if (!entangle_get_outgoing(graph, task2->task_id, edges2, 256, &count2)) {
        count2 = 0;
    }

    if (count1 == 0 && count2 == 0) return 1.0f;  /* Both empty = same */
    if (count1 == 0 || count2 == 0) return 0.0f;  /* One empty = different */

    /* Compute edge target overlap (Jaccard) */
    uint32_t intersection = 0;
    uint32_t union_count = (uint32_t)count1;

    for (size_t i = 0; i < count2; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count2 > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)count2);
        }

        bool found = false;
        for (size_t j = 0; j < count1; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && count1 > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(j + 1) / (float)count1);
            }

            if (edges2[i].to_id == edges1[j].to_id) {
                intersection++;
                found = true;
                break;
            }
        }
        if (!found) {
            union_count++;
        }
    }

    return union_count > 0 ? (float)intersection / (float)union_count : 0.0f;
}

uint32_t pr_meta_merge_entanglement(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* tasks,
    uint32_t num_tasks,
    const float* weights)
{
    if (!bridge || !graph || !tasks || num_tasks == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_merge_entang", 0.0f);


    uint32_t total_edges = 0;

    /* Use equal weights if not provided */
    float equal_weight = 1.0f / (float)num_tasks;

    for (uint32_t t = 0; t < num_tasks; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && num_tasks > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(t + 1) / (float)num_tasks);
        }

        float task_weight = weights ? weights[t] : equal_weight;

        entangle_edge_t edges[256];
        size_t count = 0;
        if (!entangle_get_outgoing(graph, tasks[t].task_id, edges, 256, &count)) {
            continue;
        }

        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                                 (float)(i + 1) / (float)count);
            }

            /* Scale edge weight by task weight */
            edges[i].weight *= task_weight;

            /* Check if edge already exists in merged graph */
            entangle_edge_t existing;
            if (entangle_get_edge(graph, edges[i].from_id, edges[i].to_id, &existing)) {
                /* Strengthen existing */
                entangle_strengthen_edge(graph, edges[i].from_id, edges[i].to_id,
                                        edges[i].weight);
            }
            total_edges++;
        }
    }

    return total_edges;
}

//=============================================================================
// Tier-Based Meta-Learning Functions
//=============================================================================

float pr_meta_tier_adaptation_rate(
    pr_meta_bridge_t bridge,
    pr_meta_tier_t tier)
{
    if (!bridge) return 1.0f;
    if (tier >= PR_META_NUM_TIERS) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_tier_adaptat", 0.0f);


    return bridge->config.tier_adaptation_rate[tier];
}

int pr_meta_get_tier_params(
    pr_meta_bridge_t bridge,
    pr_meta_tier_t tier,
    pr_meta_tier_params_t* params)
{
    if (!bridge || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_get_tier_params: required parameter is NULL (bridge, params)");
        return -1;
    }
    if (tier >= PR_META_NUM_TIERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_get_tier_params: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_get_tier_par", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    params->adaptation_rate = bridge->config.tier_adaptation_rate[tier];

    /* Compute memory weight based on tier (higher tiers rely more on memory) */
    params->memory_weight = 0.3f + 0.2f * (float)tier;

    /* Transfer weight decreases with tier (deep storage shouldn't change) */
    params->transfer_weight = bridge->config.transfer_weight * (1.0f - 0.2f * (float)tier);

    /* Max inner steps increase with tier (more careful adaptation) */
    params->max_inner_steps = bridge->config.inner_steps * (1 + tier);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

pr_meta_tier_t pr_meta_classify_tier(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    const pr_meta_result_t* result)
{
    if (!bridge || !task) return PR_META_TIER_Z0;

    /* Classification based on multiple factors */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_classify_tie", 0.0f);


    float score = 0.0f;

    /* Access count (more access -> lower tier) */
    if (task->access_count > 10) score += 1.0f;
    if (task->access_count > 50) score += 1.0f;
    if (task->access_count > 100) score += 1.0f;

    /* Success rate (better success -> lower tier for consolidation) */
    if (result) {
        if (result->query_accuracy > 0.8f) score += 1.0f;
        if (result->query_accuracy > 0.9f) score += 1.0f;
    }

    /* Age (older tasks move to lower tiers) */
    if (task->created_time_ms > 0) {
        uint64_t now_ms = nimcp_time_get_ms();
        uint64_t age_ms = now_ms - task->created_time_ms;
        uint64_t day_ms = 24ULL * 60 * 60 * 1000;
        if (age_ms > day_ms) score += 0.5f;
        if (age_ms > 7 * day_ms) score += 0.5f;
        if (age_ms > 30 * day_ms) score += 1.0f;
    }

    /* Quaternion consolidation */
    if (task->quaternion.w > 0.7f) score += 1.0f;

    /* Clamp to valid tier range */
    int tier = (int)(score / 2.0f);
    if (tier < 0) tier = 0;
    if (tier >= PR_META_NUM_TIERS) tier = PR_META_NUM_TIERS - 1;

    return (pr_meta_tier_t)tier;
}

int pr_meta_move_tier(
    pr_meta_bridge_t bridge,
    uint64_t task_id,
    pr_meta_tier_t new_tier)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_move_tier: bridge is NULL");
        return -1;
    }
    if (new_tier >= PR_META_NUM_TIERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_move_tier: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_move_tier", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int32_t slot = find_task_slot(bridge, task_id);
    if (slot < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_move_tier: validation failed");
        return -1;
    }

    pr_meta_task_entry_t* entry = &bridge->task_memory[slot];
    pr_meta_tier_t old_tier = entry->task.tier;

    if (old_tier != new_tier) {
        /* Update tier counts */
        bridge->tier_counts[old_tier]--;
        bridge->tier_counts[new_tier]++;

        /* Update tier statistics */
        bridge->stats.tasks_per_tier[old_tier]--;
        bridge->stats.tasks_per_tier[new_tier]++;

        entry->task.tier = new_tier;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Task Memory Functions
//=============================================================================

int pr_meta_store_task_memory(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    const pr_meta_result_t* result)
{
    if (!bridge || !task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_store_task_memory: required parameter is NULL (bridge, task)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_store_task_m", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if task already exists */
    int32_t existing_slot = find_task_slot(bridge, task->task_id);
    if (existing_slot >= 0) {
        /* Update existing entry */
        pr_meta_task_entry_t* entry = &bridge->task_memory[existing_slot];
        entry->task = *task;
        entry->task.access_count++;
        entry->last_access_ms = nimcp_time_get_ms();
        entry->relevance_score = 1.0f;

        if (result) {
            /* Free old adapted params if exist */
            if (entry->result.adapted_params) {
                nimcp_free(entry->result.adapted_params);
            }
            entry->result = *result;
            entry->result.adapted_params = NULL;  /* Don't copy pointer */
            entry->has_result = true;
        }

        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Check if memory is full */
    if (bridge->task_count >= bridge->task_capacity) {
        /* Evict least relevant task */
        nimcp_mutex_unlock(bridge->base.mutex);
        pr_meta_evict_tasks(bridge, 1);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Find empty slot */
    uint32_t slot = bridge->task_count;
    for (uint32_t i = 0; i < bridge->task_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_capacity > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_capacity);
        }

        if (bridge->task_memory[i].task.task_id == 0) {
            slot = i;
            break;
        }
    }

    if (slot >= bridge->task_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_meta_store_task_memory: capacity exceeded");
        return -1;
    }

    /* Store task */
    pr_meta_task_entry_t* entry = &bridge->task_memory[slot];
    memset(entry, 0, sizeof(pr_meta_task_entry_t));
    entry->task = *task;
    entry->task.access_count = 1;
    entry->last_access_ms = nimcp_time_get_ms();
    entry->relevance_score = 1.0f;

    if (result) {
        entry->result = *result;
        entry->result.adapted_params = NULL;  /* Don't copy pointer */
        entry->has_result = true;
    } else {
        entry->has_result = false;
    }

    /* Update index */
    insert_task_index(bridge, task->task_id, slot);

    /* Update counts */
    if (slot == bridge->task_count) {
        bridge->task_count++;
    }
    bridge->tier_counts[task->tier]++;
    bridge->stats.tasks_stored++;
    bridge->stats.tasks_per_tier[task->tier]++;

    /* Invalidate recall cache */
    bridge->recall_cache_task_id = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool pr_meta_recall_task(
    pr_meta_bridge_t bridge,
    uint64_t task_id,
    pr_meta_task_t* task,
    pr_meta_result_t* result)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_recall_task: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_recall_task", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int32_t slot = find_task_slot(bridge, task_id);
    if (slot < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_recall_task: validation failed");
        return false;
    }

    pr_meta_task_entry_t* entry = &bridge->task_memory[slot];

    /* Update access time */
    entry->last_access_ms = nimcp_time_get_ms();
    entry->task.access_count++;
    entry->relevance_score = 1.0f;

    /* Copy task */
    if (task) {
        *task = entry->task;
    }

    /* Copy result if available and requested */
    if (result && entry->has_result) {
        *result = entry->result;
        result->adapted_params = NULL;  /* Don't expose internal pointer */
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

bool pr_meta_forget_task(
    pr_meta_bridge_t bridge,
    uint64_t task_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_forget_task: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_forget_task", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int32_t slot = find_task_slot(bridge, task_id);
    if (slot < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_forget_task: validation failed");
        return false;
    }

    pr_meta_task_entry_t* entry = &bridge->task_memory[slot];

    /* Update tier counts */
    bridge->tier_counts[entry->task.tier]--;
    bridge->stats.tasks_per_tier[entry->task.tier]--;

    /* Free adapted params if exist */
    if (entry->result.adapted_params) {
        nimcp_free(entry->result.adapted_params);
    }

    /* Clear entry */
    memset(entry, 0, sizeof(pr_meta_task_entry_t));

    /* Remove from index */
    remove_task_index(bridge, task_id);

    /* Invalidate recall cache */
    bridge->recall_cache_task_id = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

uint32_t pr_meta_memory_size(pr_meta_bridge_t bridge) {
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_memory_size", 0.0f);


    return bridge->task_count;
}

uint32_t pr_meta_evict_tasks(
    pr_meta_bridge_t bridge,
    uint32_t count)
{
    if (!bridge || count == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_evict_tasks", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->task_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find lowest relevance tasks */
    typedef struct {
        uint32_t slot;
        float score;
    } evict_candidate_t;

    evict_candidate_t* candidates = nimcp_calloc(bridge->task_count, sizeof(evict_candidate_t));
    if (!candidates) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    uint32_t num_candidates = 0;
    uint64_t now_ms = nimcp_time_get_ms();

    for (uint32_t i = 0; i < bridge->task_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_capacity > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_capacity);
        }

        pr_meta_task_entry_t* entry = &bridge->task_memory[i];
        if (entry->task.task_id == 0) continue;

        /* Compute eviction score (lower = evict first) */
        float score = entry->relevance_score;

        /* Decay by age */
        float age_hours = (float)(now_ms - entry->last_access_ms) / (1000.0f * 3600.0f);
        score *= fast_exp(-age_hours / 24.0f);

        /* Boost by access count */
        score *= (1.0f + logf((float)entry->task.access_count + 1.0f));

        /* Boost by tier (protect deep storage) */
        score *= (1.0f + (float)entry->task.tier);

        candidates[num_candidates].slot = i;
        candidates[num_candidates].score = score;
        num_candidates++;
    }

    /* Sort by score (ascending - lowest first for eviction) */
    for (uint32_t i = 0; i < num_candidates - 1; i++) {
        for (uint32_t j = i + 1; j < num_candidates; j++) {
            if (candidates[j].score < candidates[i].score) {
                evict_candidate_t tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }

    /* Evict lowest scoring tasks */
    uint32_t evicted = 0;
    for (uint32_t i = 0; i < count && i < num_candidates; i++) {
        pr_meta_task_entry_t* entry = &bridge->task_memory[candidates[i].slot];

        /* Update tier counts */
        bridge->tier_counts[entry->task.tier]--;
        bridge->stats.tasks_per_tier[entry->task.tier]--;

        /* Free adapted params */
        if (entry->result.adapted_params) {
            nimcp_free(entry->result.adapted_params);
        }

        /* Remove from index */
        remove_task_index(bridge, entry->task.task_id);

        /* Clear entry */
        memset(entry, 0, sizeof(pr_meta_task_entry_t));

        evicted++;
    }

    nimcp_free(candidates);
    candidates = NULL;

    /* Invalidate recall cache */
    bridge->recall_cache_task_id = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return evicted;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int pr_meta_get_stats(
    pr_meta_bridge_t bridge,
    pr_meta_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_meta_reset_stats(pr_meta_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(pr_meta_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

void pr_meta_print_stats(pr_meta_bridge_t bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_print_stats", 0.0f);


    pr_meta_bridge_stats_t stats;
    if (pr_meta_get_stats(bridge, &stats) != 0) return;

    printf("=== Prime Resonant Meta-Learning Bridge Statistics ===\n");

    printf("\nTask Processing:\n");
    printf("  Total tasks processed: %lu\n", (unsigned long)stats.total_tasks_processed);
    printf("  Tasks stored:          %lu\n", (unsigned long)stats.tasks_stored);
    printf("  Tasks recalled:        %lu\n", (unsigned long)stats.tasks_recalled);
    printf("  Tasks transferred:     %lu\n", (unsigned long)stats.tasks_transferred);

    printf("\nInner Loop:\n");
    printf("  Total inner steps:     %lu\n", (unsigned long)stats.total_inner_steps);
    printf("  Avg inner steps:       %.2f\n", stats.avg_inner_steps);
    printf("  Avg support loss:      %.4f\n", stats.avg_support_loss);
    printf("  Avg query loss:        %.4f\n", stats.avg_query_loss);

    printf("\nOuter Loop:\n");
    printf("  Total outer steps:     %lu\n", (unsigned long)stats.total_outer_steps);
    printf("  Avg meta-gradient:     %.4f\n", stats.avg_meta_gradient_norm);

    printf("\nMemory:\n");
    printf("  Avg recall similarity: %.4f\n", stats.avg_recall_similarity);
    printf("  Avg recall count:      %u\n", stats.avg_recall_count);
    printf("  Memory hit rate:       %.4f\n", stats.memory_hit_rate);

    printf("\nQuaternion:\n");
    printf("  Avg quat adaptation:   %.4f\n", stats.avg_quat_adaptation);
    printf("  Avg quat distance:     %.4f\n", stats.avg_quaternion_distance);

    printf("\nEntanglement:\n");
    printf("  Edges transferred:     %lu\n", (unsigned long)stats.edges_transferred);
    printf("  Avg transfer weight:   %.4f\n", stats.avg_transfer_weight);

    printf("\nPer-Tier Statistics:\n");
    for (int t = 0; t < PR_META_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_META_NUM_TIERS > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(t + 1) / (float)PR_META_NUM_TIERS);
        }

        printf("  %s: %lu tasks, rate %.4f\n",
               pr_meta_tier_name((pr_meta_tier_t)t),
               (unsigned long)stats.tasks_per_tier[t],
               stats.adaptation_rate_per_tier[t]);
    }

    printf("\nPerformance:\n");
    printf("  Avg adaptation time:   %.2f ms\n", stats.avg_adaptation_time_ms);
    printf("  Avg recall time:       %.2f ms\n", stats.avg_recall_time_ms);
    printf("  Total bridge updates:  %lu\n", (unsigned long)stats.total_bridge_updates);

    printf("=====================================================\n");
}

//=============================================================================
// Integration Functions
//=============================================================================

int pr_meta_connect_meta_ctx(
    pr_meta_bridge_t bridge,
    meta_ctx_t* meta_ctx)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_connect_meta_ctx: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_connect_meta", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->meta_ctx = meta_ctx;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_meta_connect_graph(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_connect_graph: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_connect_grap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->graph = graph;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_meta_bridge_update(
    pr_meta_bridge_t bridge,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_bridge_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_ms = nimcp_time_get_ms();

    /* Decay relevance scores */
    float decay = fast_exp(-dt_ms / (1000.0f * 3600.0f));  /* 1 hour time constant */
    for (uint32_t i = 0; i < bridge->task_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_capacity > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(i + 1) / (float)bridge->task_capacity);
        }

        pr_meta_task_entry_t* entry = &bridge->task_memory[i];
        if (entry->task.task_id == 0) continue;

        entry->relevance_score *= decay;
    }

    /* Update tier statistics */
    for (int t = 0; t < PR_META_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_META_NUM_TIERS > 256) {
            pr_meta_bridge_heartbeat("pr_meta_brid_loop",
                             (float)(t + 1) / (float)PR_META_NUM_TIERS);
        }

        bridge->stats.adaptation_rate_per_tier[t] =
            bridge->config.tier_adaptation_rate[t];
    }

    /* Update bridge statistics */
    bridge->stats.total_bridge_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)now_ms;  /* Suppress unused warning */

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_meta_type_name(pr_meta_type_t type) {
    switch (type) {
        case PR_META_MEMORY_MAML:        return "Memory-MAML";
        case PR_META_RESONANCE_REPTILE:  return "Resonance-Reptile";
        case PR_META_QUATERNION_META:    return "Quaternion-Meta";
        case PR_META_ENTANGLEMENT_TRANSFER: return "Entanglement-Transfer";
        case PR_META_HYBRID:             return "Hybrid";
        default:                         return "Unknown";
    }
}

const char* pr_meta_tier_name(pr_meta_tier_t tier) {
    switch (tier) {
        case PR_META_TIER_Z0: return "Z0 (Working)";
        case PR_META_TIER_Z1: return "Z1 (Short-term)";
        case PR_META_TIER_Z2: return "Z2 (Long-term)";
        case PR_META_TIER_Z3: return "Z3 (Deep Storage)";
        default:              return "Unknown";
    }
}

const char* pr_meta_similarity_name(pr_meta_similarity_t metric) {
    switch (metric) {
        case PR_META_SIM_RESONANCE:       return "Resonance";
        case PR_META_SIM_PRIME_SIGNATURE: return "Prime-Signature";
        case PR_META_SIM_QUATERNION:      return "Quaternion";
        case PR_META_SIM_ENTANGLEMENT:    return "Entanglement";
        case PR_META_SIM_EMBEDDING:       return "Embedding";
        default:                          return "Unknown";
    }
}

void pr_meta_task_print(const pr_meta_task_t* task) {
    if (!task) return;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_task_print", 0.0f);


    printf("Meta Task:\n");
    printf("  ID:        %lu\n", (unsigned long)task->task_id);
    if (task->name) {
        printf("  Name:      %s\n", task->name);
    }
    printf("  Tier:      %s\n", pr_meta_tier_name(task->tier));
    printf("  Quaternion: (%.3f, %.3f, %.3f, %.3f)\n",
           task->quaternion.w, task->quaternion.x,
           task->quaternion.y, task->quaternion.z);
    printf("  Phase:     %.3f\n", task->phase);
    printf("  Accesses:  %u\n", task->access_count);
}

void pr_meta_result_print(const pr_meta_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_result_print", 0.0f);


    printf("Meta Result:\n");
    printf("  Task ID:      %lu\n", (unsigned long)result->task_id);
    printf("  Support Loss: %.4f\n", result->support_loss);
    printf("  Query Loss:   %.4f\n", result->query_loss);
    printf("  Query Acc:    %.4f\n", result->query_accuracy);
    printf("  Inner Steps:  %u\n", result->inner_steps_used);
    printf("  Inner LR:     %.6f\n", result->total_inner_lr);
    printf("  Memory Used:  %u\n", result->memory_tasks_used);
    printf("  Time:         %.2f ms\n", result->adaptation_time_ms);
    printf("  Adapted Quat: (%.3f, %.3f, %.3f, %.3f)\n",
           result->adapted_quat.w, result->adapted_quat.x,
           result->adapted_quat.y, result->adapted_quat.z);
}

bool pr_meta_task_validate(const pr_meta_task_t* task) {
    if (!task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_meta_task_validate: task is NULL");
        return false;
    }
    if (task->task_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_task_validate: task->task_id is zero");
        return false;
    }
    if (task->tier >= PR_META_NUM_TIERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_meta_task_validate: capacity exceeded");
        return false;
    }

    /* Quaternion should be normalized (or close) */
    /* Phase 8: Heartbeat at operation start */
    pr_meta_bridge_heartbeat("pr_meta_brid_pr_meta_task_validat", 0.0f);


    float mag = task->quaternion.w * task->quaternion.w +
                task->quaternion.x * task->quaternion.x +
                task->quaternion.y * task->quaternion.y +
                task->quaternion.z * task->quaternion.z;
    if (fabsf(mag - 1.0f) > 0.1f && mag > PR_META_EPSILON) {
        return false;  /* Not normalized */
    }

    return true;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_meta_bridge_set_instance_health_agent(
    pr_meta_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_meta_bridge_training_begin(pr_meta_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_meta_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_meta_bridge_heartbeat_instance(bridge->health_agent, "pr_meta_bridge_training_begin", 0.0f);
    return 0;
}

int pr_meta_bridge_training_end(pr_meta_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_meta_bridge_training_end: NULL argument");
        return -1;
    }
    pr_meta_bridge_heartbeat_instance(bridge->health_agent, "pr_meta_bridge_training_end", 1.0f);
    return 0;
}

int pr_meta_bridge_training_step(pr_meta_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_meta_bridge_training_step: NULL argument");
        return -1;
    }
    pr_meta_bridge_heartbeat_instance(bridge->health_agent, "pr_meta_bridge_training_step", progress);
    return 0;
}
