/**
 * @file nimcp_pr_continual_bridge.c
 * @brief Continual Learning Bridge Implementation for Prime Resonant Memory
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of continual learning bridge with EWC + quaternion
 *       consolidation protection
 * WHY:  Prevent catastrophic forgetting by protecting important parameters
 *       based on Fisher information weighted by memory consolidation state
 * HOW:  Implements Fisher computation, EWC loss/gradients, experience replay,
 *       and tier-based protection with entanglement awareness
 */

#include "cognitive/memory/core/nimcp_pr_continual_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_continual_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_continual_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_continual_bridge_mesh_registry = NULL;

nimcp_error_t pr_continual_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_continual_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_continual_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_continual_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_continual_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_continual_bridge_mesh_registry = registry;
    return err;
}

void pr_continual_bridge_mesh_unregister(void) {
    if (g_pr_continual_bridge_mesh_registry && g_pr_continual_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_continual_bridge_mesh_registry, g_pr_continual_bridge_mesh_id);
        g_pr_continual_bridge_mesh_id = 0;
        g_pr_continual_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_continual_bridge module (instance-level) */
static inline void pr_continual_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_continual_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_continual_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_continual_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_CONTINUAL_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete state for continual learning bridge
 * WHY:  Encapsulate all data for thread-safe operations
 */
struct pr_continual_bridge_struct {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_continual_config_t config;

    /* Fisher information storage */
    float* fisher_diag;                 /**< Fisher diagonal (importance per param) */
    size_t fisher_size;                 /**< Number of parameters */
    size_t fisher_capacity;             /**< Allocated capacity */

    /* Reference parameters (theta*) */
    float* old_params;                  /**< Parameters after previous task */
    bool params_stored;                 /**< Whether params have been stored */

    /* Per-task Fisher (for task-specific importance) */
    float** task_fisher;                /**< Fisher per task (if enabled) */
    uint32_t num_task_fisher;           /**< Number of task-specific Fisher */

    /* Task history */
    pr_continual_task_info_t* task_history;
    uint32_t task_history_count;
    uint32_t task_history_capacity;
    uint32_t current_task_id;

    /* Statistics */
    pr_continual_stats_t stats;

    /* Online Fisher accumulation */
    float* fisher_accum;                /**< Accumulator for online Fisher */
    uint64_t fisher_accum_count;        /**< Samples in accumulator */

    /* Importance cache */
    pr_continual_node_importance_t* importance_cache;
    uint32_t importance_cache_count;
    uint32_t importance_cache_capacity;

    /* Timing */
    uint64_t session_start_ms;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_continual_bridge, struct pr_continual_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Square a float
 */
static inline float sq(float x) {
    return x * x;
}

/**
 * @brief Get decay rate for tier
 */
static float tier_to_decay(pr_continual_tier_t tier) {
    switch (tier) {
        case PR_CONTINUAL_TIER_Z0: return 1.0f;    /* Fastest decay in importance */
        case PR_CONTINUAL_TIER_Z1: return 0.9f;
        case PR_CONTINUAL_TIER_Z2: return 0.7f;
        case PR_CONTINUAL_TIER_Z3: return 0.3f;    /* Slowest decay */
        default: return 1.0f;
    }
}

/**
 * @brief Ensure Fisher arrays are allocated for num_params
 */
static int ensure_fisher_capacity(pr_continual_bridge_t bridge, size_t num_params) {
    if (num_params <= bridge->fisher_capacity) {
        bridge->fisher_size = num_params;
        return 0;
    }

    /* Pre-allocate all arrays to temp pointers before committing */
    float* new_fisher = nimcp_realloc(bridge->fisher_diag,
                                       num_params * sizeof(float));
    if (!new_fisher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_fisher_capacity: new_fisher is NULL");
        return -1;
    }

    float* new_params = nimcp_realloc(bridge->old_params,
                                       num_params * sizeof(float));
    if (!new_params) {
        /* fisher_diag was reallocated but old_params was not - safe because
         * fisher_diag is still valid at the larger size, we just don't update capacity */
        bridge->fisher_diag = new_fisher;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_fisher_capacity: new_params is NULL");
        return -1;
    }

    float* new_accum = NULL;
    if (bridge->config.enable_online_fisher) {
        new_accum = nimcp_realloc(bridge->fisher_accum,
                                          num_params * sizeof(float));
        if (!new_accum) {
            bridge->fisher_diag = new_fisher;
            bridge->old_params = new_params;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_fisher_capacity: new_accum is NULL");
            return -1;
        }
    }

    /* All allocations succeeded - commit */
    bridge->fisher_diag = new_fisher;
    bridge->old_params = new_params;

    /* Initialize new entries to zero */
    for (size_t i = bridge->fisher_capacity; i < num_params; i++) {
        bridge->fisher_diag[i] = 0.0f;
        bridge->old_params[i] = 0.0f;
    }

    if (bridge->config.enable_online_fisher && new_accum) {
        bridge->fisher_accum = new_accum;
        for (size_t i = bridge->fisher_capacity; i < num_params; i++) {
            bridge->fisher_accum[i] = 0.0f;
        }
    }

    bridge->fisher_capacity = num_params;
    bridge->fisher_size = num_params;

    return 0;
}

/**
 * @brief Add task to history
 */
static int add_task_to_history(pr_continual_bridge_t bridge,
                               const pr_continual_task_info_t* info) {
    if (bridge->task_history_count >= bridge->task_history_capacity) {
        /* Expand capacity */
        uint32_t new_cap = bridge->task_history_capacity * 2;
        if (new_cap > PR_CONTINUAL_MAX_TASKS) {
            new_cap = PR_CONTINUAL_MAX_TASKS;
        }
        if (bridge->task_history_count >= new_cap) {
            /* Shift out oldest */
            memmove(bridge->task_history, bridge->task_history + 1,
                    (bridge->task_history_count - 1) * sizeof(pr_continual_task_info_t));
            bridge->task_history_count--;
        } else {
            pr_continual_task_info_t* new_hist = nimcp_realloc(
                bridge->task_history,
                new_cap * sizeof(pr_continual_task_info_t));
            if (!new_hist) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_fisher_capacity: new_hist is NULL");
                return -1;
            }
            bridge->task_history = new_hist;
            bridge->task_history_capacity = new_cap;
        }
    }

    bridge->task_history[bridge->task_history_count] = *info;
    bridge->task_history_count++;

    return 0;
}

/**
 * @brief Compute L2 norm of float array
 */
static float compute_l2_norm(const float* arr, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        sum += arr[i] * arr[i];
    }
    return sqrtf(sum);
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_continual_config_t pr_continual_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_config_", 0.0f);


    pr_continual_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = PR_CONTINUAL_COMBINED;
    config.ewc_lambda = PR_CONTINUAL_DEFAULT_LAMBDA;
    config.consolidation_weight = PR_CONTINUAL_DEFAULT_CONSOL_WEIGHT;
    config.replay_ratio = PR_CONTINUAL_DEFAULT_REPLAY_RATIO;
    config.replay_batch_size = PR_CONTINUAL_DEFAULT_REPLAY_BATCH;

    config.protection_threshold[PR_CONTINUAL_TIER_Z0] = PR_CONTINUAL_TIER_PROTECT_Z0;
    config.protection_threshold[PR_CONTINUAL_TIER_Z1] = PR_CONTINUAL_TIER_PROTECT_Z1;
    config.protection_threshold[PR_CONTINUAL_TIER_Z2] = PR_CONTINUAL_TIER_PROTECT_Z2;
    config.protection_threshold[PR_CONTINUAL_TIER_Z3] = PR_CONTINUAL_TIER_PROTECT_Z3;

    config.entangle_threshold = PR_CONTINUAL_ENTANGLE_THRESHOLD;
    config.fisher_samples = PR_CONTINUAL_DEFAULT_FISHER_SAMPLES;

    config.enable_online_fisher = true;
    config.enable_task_specific = false;
    config.enable_importance_decay = true;
    config.importance_decay_rate = 0.1f;

    config.enable_sparse_fisher = true;
    config.sparse_threshold = PR_CONTINUAL_FISHER_EPSILON;

    return config;
}

pr_continual_config_t pr_continual_config_ewc_only(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_config_", 0.0f);


    pr_continual_config_t config = pr_continual_config_default();

    config.type = PR_CONTINUAL_EWC;
    config.replay_ratio = 0.0f;
    config.consolidation_weight = 0.0f;

    return config;
}

pr_continual_config_t pr_continual_config_replay_focused(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_config_", 0.0f);


    pr_continual_config_t config = pr_continual_config_default();

    config.type = PR_CONTINUAL_RESONANCE_REPLAY;
    config.ewc_lambda = 100.0f;  /* Weaker EWC */
    config.replay_ratio = 0.8f;  /* Strong replay */
    config.replay_batch_size = NIMCP_MEDIUM_BATCH_SIZE;

    return config;
}

pr_continual_config_t pr_continual_config_max_protection(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_config_", 0.0f);


    pr_continual_config_t config = pr_continual_config_default();

    config.type = PR_CONTINUAL_COMBINED;
    config.ewc_lambda = 10000.0f;
    config.consolidation_weight = 4.0f;
    config.replay_ratio = 0.6f;

    /* Maximum tier protection */
    config.protection_threshold[PR_CONTINUAL_TIER_Z0] = 0.3f;
    config.protection_threshold[PR_CONTINUAL_TIER_Z1] = 0.6f;
    config.protection_threshold[PR_CONTINUAL_TIER_Z2] = 0.9f;
    config.protection_threshold[PR_CONTINUAL_TIER_Z3] = 1.0f;

    config.enable_importance_decay = false;

    return config;
}

bool pr_continual_config_validate(const pr_continual_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_config_validate: config is NULL");
        return false;
    }

    if (config->ewc_lambda < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_config_validate: validation failed");
        return false;
    }
    if (config->consolidation_weight < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_config_validate: validation failed");
        return false;
    }
    if (config->replay_ratio < 0.0f || config->replay_ratio > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_config_validate: validation failed");
        return false;
    }
    if (config->replay_batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_config_validate: config->replay_batch_size is zero");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_config_", 0.0f);


    for (int i = 0; i < PR_CONTINUAL_NUM_TIERS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_CONTINUAL_NUM_TIERS > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)PR_CONTINUAL_NUM_TIERS);
        }

        if (config->protection_threshold[i] < 0.0f ||
            config->protection_threshold[i] > 1.0f) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_config_validate: validation failed");
            return false;
        }
    }

    if (config->enable_importance_decay &&
        (config->importance_decay_rate < 0.0f || config->importance_decay_rate > 1.0f)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_config_validate: operation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

pr_continual_bridge_t pr_continual_bridge_create(
    const pr_continual_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_create", 0.0f);


    pr_continual_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_continual_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        if (!pr_continual_config_validate(config)) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_bridge_create: pr_continual_config_validate is NULL");
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = pr_continual_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_continual") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_continual_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize Fisher storage with initial capacity */
    bridge->fisher_capacity = 1024;  /* Initial capacity */
    bridge->fisher_size = 0;

    bridge->fisher_diag = nimcp_calloc(bridge->fisher_capacity, sizeof(float));
    if (!bridge->fisher_diag) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_continual_bridge_create: bridge->fisher_diag is NULL");
        return NULL;
    }

    bridge->old_params = nimcp_calloc(bridge->fisher_capacity, sizeof(float));
    if (!bridge->old_params) {
        nimcp_free(bridge->fisher_diag);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_continual_bridge_create: bridge->old_params is NULL");
        return NULL;
    }
    bridge->params_stored = false;

    /* Online Fisher accumulator */
    if (bridge->config.enable_online_fisher) {
        bridge->fisher_accum = nimcp_calloc(bridge->fisher_capacity, sizeof(float));
        if (!bridge->fisher_accum) {
            nimcp_free(bridge->old_params);
            nimcp_free(bridge->fisher_diag);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_bridge_create: bridge->fisher_accum is NULL");
            return NULL;
        }
        bridge->fisher_accum_count = 0;
    }

    /* Task history */
    bridge->task_history_capacity = 16;
    bridge->task_history = nimcp_calloc(bridge->task_history_capacity,
                                         sizeof(pr_continual_task_info_t));
    if (!bridge->task_history) {
        if (bridge->fisher_accum) nimcp_free(bridge->fisher_accum);
        nimcp_free(bridge->old_params);
        nimcp_free(bridge->fisher_diag);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_bridge_create: validation failed");
        return NULL;
    }
    bridge->task_history_count = 0;
    bridge->current_task_id = 0;

    /* Importance cache */
    bridge->importance_cache_capacity = 256;
    bridge->importance_cache = nimcp_calloc(bridge->importance_cache_capacity,
                                             sizeof(pr_continual_node_importance_t));
    if (!bridge->importance_cache) {
        nimcp_free(bridge->task_history);
        if (bridge->fisher_accum) nimcp_free(bridge->fisher_accum);
        nimcp_free(bridge->old_params);
        nimcp_free(bridge->fisher_diag);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_bridge_create: validation failed");
        return NULL;
    }
    bridge->importance_cache_count = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(pr_continual_stats_t));

    /* Initialize timing */
    bridge->session_start_ms = nimcp_time_get_ms();

    return bridge;
}

void pr_continual_bridge_destroy(pr_continual_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_continual");

    /* Free Fisher storage */
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_destroy", 0.0f);


    if (bridge->fisher_diag) nimcp_free(bridge->fisher_diag);
    if (bridge->old_params) nimcp_free(bridge->old_params);
    if (bridge->fisher_accum) nimcp_free(bridge->fisher_accum);

    /* Free task-specific Fisher */
    if (bridge->task_fisher) {
        for (uint32_t t = 0; t < bridge->num_task_fisher; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && bridge->num_task_fisher > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(t + 1) / (float)bridge->num_task_fisher);
            }

            if (bridge->task_fisher[t]) {
                nimcp_free(bridge->task_fisher[t]);
            }
        }
        nimcp_free(bridge->task_fisher);
    }

    /* Free task history */
    if (bridge->task_history) nimcp_free(bridge->task_history);

    /* Free importance cache */
    if (bridge->importance_cache) nimcp_free(bridge->importance_cache);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_continual_bridge_reset(pr_continual_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset Fisher */
    if (bridge->fisher_diag) {
        memset(bridge->fisher_diag, 0, bridge->fisher_capacity * sizeof(float));
    }
    bridge->fisher_size = 0;

    /* Reset old params */
    if (bridge->old_params) {
        memset(bridge->old_params, 0, bridge->fisher_capacity * sizeof(float));
    }
    bridge->params_stored = false;

    /* Reset accumulator */
    if (bridge->fisher_accum) {
        memset(bridge->fisher_accum, 0, bridge->fisher_capacity * sizeof(float));
    }
    bridge->fisher_accum_count = 0;

    /* Reset task history */
    bridge->task_history_count = 0;
    bridge->current_task_id = 0;

    /* Reset importance cache */
    bridge->importance_cache_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pr_continual_stats_t));

    bridge->session_start_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Fisher Information Functions
//=============================================================================

int pr_continual_compute_fisher(
    pr_continual_bridge_t bridge,
    const float** gradients,
    size_t num_samples,
    size_t num_params)
{
    if (!bridge || !gradients || num_samples == 0 || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_compute_fisher: required parameter is NULL (bridge, gradients)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_compute", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure capacity */
    if (ensure_fisher_capacity(bridge, num_params) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_compute_fisher: validation failed");
        return -1;
    }

    uint64_t start_time_us = nimcp_time_get_us();

    /* Zero out Fisher diagonal */
    memset(bridge->fisher_diag, 0, num_params * sizeof(float));

    /* Compute empirical Fisher: F_i = (1/N) * sum_n (grad[n][i])^2 */
    for (size_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        if (!gradients[s]) continue;

        for (size_t i = 0; i < num_params; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_params > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(i + 1) / (float)num_params);
            }

            float g = gradients[s][i];
            bridge->fisher_diag[i] += g * g;
        }
    }

    /* Normalize by sample count */
    float inv_n = 1.0f / (float)num_samples;
    for (size_t i = 0; i < num_params; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_params > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)num_params);
        }

        bridge->fisher_diag[i] *= inv_n;

        /* Clamp to valid range */
        bridge->fisher_diag[i] = nimcp_myelin_clamp(bridge->fisher_diag[i],
                                          0.0f, PR_CONTINUAL_FISHER_MAX);

        /* Apply sparsification if enabled */
        if (bridge->config.enable_sparse_fisher &&
            bridge->fisher_diag[i] < bridge->config.sparse_threshold) {
            bridge->fisher_diag[i] = 0.0f;
        }
    }

    /* Update statistics */
    bridge->stats.fisher_computations++;

    float fisher_sum = 0.0f;
    size_t fisher_nonzero = 0;
    for (size_t i = 0; i < num_params; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_params > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)num_params);
        }

        fisher_sum += bridge->fisher_diag[i];
        if (bridge->fisher_diag[i] > PR_CONTINUAL_FISHER_EPSILON) {
            fisher_nonzero++;
        }
    }
    bridge->stats.avg_fisher_value = fisher_sum / (float)num_params;
    bridge->stats.fisher_sparsity = 1.0f - (float)fisher_nonzero / (float)num_params;

    uint64_t end_time_us = nimcp_time_get_us();
    float elapsed_ms = (float)(end_time_us - start_time_us) / 1000.0f;
    bridge->stats.avg_fisher_time_ms =
        (bridge->stats.avg_fisher_time_ms * 0.9f) + (elapsed_ms * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_compute_fisher_weighted(
    pr_continual_bridge_t bridge,
    const float** gradients,
    const nimcp_quaternion_t* quaternions,
    size_t num_samples,
    size_t num_params)
{
    if (!bridge || !gradients || !quaternions ||
        num_samples == 0 || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_compute_fisher_weighted: operation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_compute", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure capacity */
    if (ensure_fisher_capacity(bridge, num_params) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_compute_fisher_weighted: validation failed");
        return -1;
    }

    /* Zero out Fisher diagonal */
    memset(bridge->fisher_diag, 0, num_params * sizeof(float));

    float alpha = bridge->config.consolidation_weight;
    float total_weight = 0.0f;

    /* Compute weighted Fisher: F_i = sum_n ((grad[n][i])^2 * (1 + alpha * w^2)) */
    for (size_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        if (!gradients[s]) continue;

        /* Compute consolidation weight */
        float w = quaternions[s].w;
        float weight = 1.0f + alpha * w * w;
        total_weight += weight;

        for (size_t i = 0; i < num_params; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_params > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(i + 1) / (float)num_params);
            }

            float g = gradients[s][i];
            bridge->fisher_diag[i] += g * g * weight;
        }
    }

    /* Normalize */
    if (total_weight > PR_CONTINUAL_EPSILON) {
        float inv_w = 1.0f / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
        for (size_t i = 0; i < num_params; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_params > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(i + 1) / (float)num_params);
            }

            bridge->fisher_diag[i] *= inv_w;
            bridge->fisher_diag[i] = nimcp_myelin_clamp(bridge->fisher_diag[i],
                                              0.0f, PR_CONTINUAL_FISHER_MAX);
        }
    }

    bridge->stats.fisher_computations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float pr_continual_get_fisher(
    pr_continual_bridge_t bridge,
    size_t param_idx)
{
    if (!bridge) return -1.0f;
    if (param_idx >= bridge->fisher_size) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_get_fis", 0.0f);


    return bridge->fisher_diag[param_idx];
}

int pr_continual_set_fisher(
    pr_continual_bridge_t bridge,
    size_t param_idx,
    float value)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_set_fisher: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_set_fis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (param_idx >= bridge->fisher_capacity) {
        if (ensure_fisher_capacity(bridge, param_idx + 1) != 0) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_set_fisher: validation failed");
            return -1;
        }
    }

    if (param_idx >= bridge->fisher_size) {
        bridge->fisher_size = param_idx + 1;
    }

    bridge->fisher_diag[param_idx] = nimcp_myelin_clamp(value, 0.0f, PR_CONTINUAL_FISHER_MAX);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_accumulate_fisher(
    pr_continual_bridge_t bridge,
    const float* new_fisher,
    size_t num_params,
    bool decay)
{
    if (!bridge || !new_fisher || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_accumulate_fisher: required parameter is NULL (bridge, new_fisher)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_accumul", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, new_fisher, sizeof(*new_fisher));

    nimcp_mutex_lock(bridge->base.mutex);

    if (ensure_fisher_capacity(bridge, num_params) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_accumulate_fisher: validation failed");
        return -1;
    }

    /* Apply decay to old Fisher if requested */
    if (decay && bridge->config.enable_importance_decay) {
        float decay_factor = 1.0f - bridge->config.importance_decay_rate;
        for (size_t i = 0; i < bridge->fisher_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->fisher_size > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(i + 1) / (float)bridge->fisher_size);
            }

            bridge->fisher_diag[i] *= decay_factor;
        }
    }

    /* Accumulate new Fisher */
    for (size_t i = 0; i < num_params; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_params > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)num_params);
        }

        bridge->fisher_diag[i] += new_fisher[i];
        bridge->fisher_diag[i] = nimcp_myelin_clamp(bridge->fisher_diag[i],
                                          0.0f, PR_CONTINUAL_FISHER_MAX);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// EWC Loss Functions
//=============================================================================

float pr_continual_ewc_loss(
    pr_continual_bridge_t bridge,
    const float* current_params,
    size_t num_params)
{
    if (!bridge || !current_params || num_params == 0) return -1.0f;
    if (!bridge->params_stored) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_ewc_los", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, current_params, sizeof(*current_params));

    nimcp_mutex_lock(bridge->base.mutex);

    size_t n = (num_params < bridge->fisher_size) ? num_params : bridge->fisher_size;
    float lambda = bridge->config.ewc_lambda;
    float loss = 0.0f;

    /* L_ewc = (lambda/2) * sum_i (F[i] * (theta[i] - theta*[i])^2) */
    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        float diff = current_params[i] - bridge->old_params[i];
        loss += bridge->fisher_diag[i] * diff * diff;
    }

    loss *= lambda * 0.5f;

    /* Update statistics */
    bridge->stats.ewc_loss_current = loss;
    bridge->stats.ewc_loss_avg = bridge->stats.ewc_loss_avg * 0.95f + loss * 0.05f;
    if (loss > bridge->stats.ewc_loss_max) {
        bridge->stats.ewc_loss_max = loss;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return loss;
}

float pr_continual_ewc_loss_weighted(
    pr_continual_bridge_t bridge,
    const float* current_params,
    const nimcp_quaternion_t* quaternions,
    size_t num_params)
{
    if (!bridge || !current_params || num_params == 0) return -1.0f;
    if (!bridge->params_stored) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_ewc_los", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, current_params, sizeof(*current_params));

    nimcp_mutex_lock(bridge->base.mutex);

    size_t n = (num_params < bridge->fisher_size) ? num_params : bridge->fisher_size;
    float lambda = bridge->config.ewc_lambda;
    float alpha = bridge->config.consolidation_weight;
    float loss = 0.0f;

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        float diff = current_params[i] - bridge->old_params[i];

        /* Scale by quaternion consolidation if provided */
        float weight = 1.0f;
        if (quaternions) {
            float w = quaternions[i].w;
            weight = 1.0f + alpha * w * w;
        }

        loss += bridge->fisher_diag[i] * weight * diff * diff;
    }

    loss *= lambda * 0.5f;

    bridge->stats.ewc_loss_current = loss;

    nimcp_mutex_unlock(bridge->base.mutex);

    return loss;
}

int pr_continual_ewc_gradient(
    pr_continual_bridge_t bridge,
    const float* current_params,
    size_t num_params,
    float* ewc_gradients)
{
    if (!bridge || !current_params || !ewc_gradients || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_ewc_gradient: required parameter is NULL (bridge, current_params, ewc_gradients)");
        return -1;
        BRIDGE_BBB_VALIDATE(bridge, current_params, sizeof(*current_params));
    }
    if (!bridge->params_stored) {
        memset(ewc_gradients, 0, num_params * sizeof(float));
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_ewc_gra", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    size_t n = (num_params < bridge->fisher_size) ? num_params : bridge->fisher_size;
    float lambda = bridge->config.ewc_lambda;

    /* grad_ewc[i] = lambda * F[i] * (theta[i] - theta*[i]) */
    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        float diff = current_params[i] - bridge->old_params[i];
        ewc_gradients[i] = lambda * bridge->fisher_diag[i] * diff;
    }

    /* Zero remaining */
    for (size_t i = n; i < num_params; i++) {
        ewc_gradients[i] = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Importance Functions
//=============================================================================

float pr_continual_consolidation_importance(
    pr_continual_bridge_t bridge,
    const pr_memory_node_t* node)
{
    if (!bridge || !node) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_consoli", 0.0f);


    nimcp_quaternion_t quat = pr_memory_node_get_state(node);
    return pr_continual_quat_importance(bridge, quat);
}

float pr_continual_quat_importance(
    pr_continual_bridge_t bridge,
    nimcp_quaternion_t quat)
{
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_quat_im", 0.0f);


    float alpha = bridge->config.consolidation_weight;
    float w = quat.w;

    /* importance = 1 + alpha * w^2 */
    return 1.0f + alpha * w * w;
}

float pr_continual_tier_protection_mask(
    pr_continual_bridge_t bridge,
    pr_continual_tier_t tier)
{
    if (!bridge) return 0.0f;
    if (tier >= PR_CONTINUAL_NUM_TIERS) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_tier_pr", 0.0f);


    return bridge->config.protection_threshold[tier];
}

float pr_continual_entanglement_protection(
    pr_continual_bridge_t bridge,
    const pr_memory_node_t* node)
{
    if (!bridge || !node) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_entangl", 0.0f);


    uint32_t entangle_count = pr_memory_node_get_entanglement_count(node);
    uint32_t threshold = bridge->config.entangle_threshold;

    if (threshold == 0) return 0.0f;

    float protection = (float)entangle_count / (float)threshold;
    return nimcp_myelin_clamp(protection, 0.0f, 1.0f);
}

float pr_continual_combined_importance(
    pr_continual_bridge_t bridge,
    const pr_memory_node_t* node,
    float fisher_contribution)
{
    if (!bridge || !node) return 0.0f;

    /* Get tier protection */
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_combine", 0.0f);


    pr_memory_tier_t tier = pr_memory_node_get_tier(node);
    float tier_prot = pr_continual_tier_protection_mask(bridge, (pr_continual_tier_t)tier);

    /* Get entanglement protection */
    float entangle_prot = pr_continual_entanglement_protection(bridge, node);

    /* Get quaternion importance */
    float quat_imp = pr_continual_consolidation_importance(bridge, node);

    /* Combine: max of structural protections, scaled by consolidation */
    float structural_prot = (tier_prot > entangle_prot) ? tier_prot : entangle_prot;
    float importance = structural_prot * quat_imp;

    /* Add Fisher contribution */
    if (fisher_contribution > 0.0f) {
        importance *= (1.0f + fisher_contribution);
    }

    return importance;
}

//=============================================================================
// Experience Replay Functions
//=============================================================================

int pr_continual_replay_sample(
    pr_continual_bridge_t bridge,
    z_ladder_t ladder,
    int tier,
    size_t batch_size,
    pr_continual_replay_sample_t* samples,
    size_t* samples_returned)
{
    if (!bridge || !ladder || !samples || !samples_returned) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_replay_sample: required parameter is NULL (bridge, ladder, samples, samples_returned)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_replay_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *samples_returned = 0;

    /* Determine which tiers to sample from */
    int start_tier = (tier < 0) ? 0 : tier;
    int end_tier = (tier < 0) ? PR_CONTINUAL_NUM_TIERS : (tier + 1);

    /* Get distribution if sampling all tiers */
    size_t tier_counts[PR_CONTINUAL_NUM_TIERS] = {0};
    if (tier < 0) {
        pr_continual_replay_tier_distribution(bridge, batch_size, tier_counts);
    } else {
        tier_counts[tier] = batch_size;
    }

    size_t total_sampled = 0;

    for (int t = start_tier; t < end_tier && total_sampled < batch_size; t++) {
        size_t to_sample = tier_counts[t];
        if (to_sample == 0) continue;

        /* Get memories from tier */
        pr_memory_tier_t z_tier = (pr_memory_tier_t)t;
        size_t tier_size = z_ladder_get_count(ladder, z_tier);
        if (tier_size == 0) continue;

        /* Get all nodes from tier for sampling */
        pr_memory_node_t** tier_nodes = (pr_memory_node_t**)nimcp_calloc(tier_size, sizeof(pr_memory_node_t*));
        if (!tier_nodes) continue;

        size_t actual_count = 0;
        if (z_ladder_get_nodes(ladder, z_tier, tier_nodes, tier_size, &actual_count) != Z_LADDER_SUCCESS) {
            nimcp_free(tier_nodes);
            tier_nodes = NULL;
            continue;
        }

        /* Sample randomly from tier (simplified - full impl would use importance) */
        for (size_t s = 0; s < to_sample && total_sampled < batch_size && actual_count > 0; s++) {
            /* Random index in tier */
            size_t idx = (size_t)(nimcp_tl_rand() % (int)actual_count);

            /* Get node from tier */
            pr_memory_node_t* node = tier_nodes[idx];
            if (!node) continue;

            /* Fill sample */
            pr_continual_replay_sample_t* sample = &samples[total_sampled];
            sample->node_id = pr_memory_node_get_id(node);
            sample->tier = (pr_continual_tier_t)t;
            sample->importance = pr_continual_combined_importance(bridge, node, 0.0f);
            sample->resonance = 0.5f;  /* Would compute with context in full impl */
            sample->state = pr_memory_node_get_state(node);
            sample->data = pr_memory_node_read((pr_memory_node_t*)node);
            sample->data_size = pr_memory_node_get_data_size(node);

            total_sampled++;

            /* Update statistics */
            bridge->stats.replay_samples_total++;
            bridge->stats.replay_per_tier[t]++;
        }

        nimcp_free(tier_nodes);
        tier_nodes = NULL;
    }

    *samples_returned = total_sampled;

    /* Update average importance */
    if (total_sampled > 0) {
        float total_imp = 0.0f;
        for (size_t i = 0; i < total_sampled; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && total_sampled > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(i + 1) / (float)total_sampled);
            }

            total_imp += samples[i].importance;
        }
        bridge->stats.avg_replay_importance =
            bridge->stats.avg_replay_importance * 0.9f +
            (total_imp / (float)total_sampled) * 0.1f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_replay_sample_resonant(
    pr_continual_bridge_t bridge,
    z_ladder_t ladder,
    pr_continual_tier_t tier,
    const resonance_query_t* query,
    float min_resonance,
    size_t batch_size,
    pr_continual_replay_sample_t* samples,
    size_t* samples_returned)
{
    if (!bridge || !ladder || !query || !samples || !samples_returned) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_replay_sample_resonant: required parameter is NULL (bridge, ladder, query, samples, samples_returned)");
        return -1;
    }

    /* For now, delegate to basic sampling */
    /* Full implementation would compute resonance and filter */
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_replay_", 0.0f);


    (void)query;
    (void)min_resonance;

    return pr_continual_replay_sample(bridge, ladder, (int)tier, batch_size,
                                       samples, samples_returned);
}

int pr_continual_replay_tier_distribution(
    pr_continual_bridge_t bridge,
    size_t total_batch,
    size_t tier_counts[PR_CONTINUAL_NUM_TIERS])
{
    if (!bridge || !tier_counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_replay_tier_distribution: required parameter is NULL (bridge, tier_counts)");
        return -1;
    }

    /* Default distribution: Z0=40%, Z1=30%, Z2=20%, Z3=10% */
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_replay_", 0.0f);


    float dist[PR_CONTINUAL_NUM_TIERS] = {0.4f, 0.3f, 0.2f, 0.1f};

    size_t assigned = 0;
    for (int t = 0; t < PR_CONTINUAL_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CONTINUAL_NUM_TIERS > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(t + 1) / (float)PR_CONTINUAL_NUM_TIERS);
        }

        tier_counts[t] = (size_t)(dist[t] * (float)total_batch);
        assigned += tier_counts[t];
    }

    /* Assign remainder to Z0 */
    tier_counts[0] += total_batch - assigned;

    return 0;
}

//=============================================================================
// Gradient Protection Functions
//=============================================================================

int pr_continual_apply_protection(
    pr_continual_bridge_t bridge,
    float* gradients,
    size_t num_params,
    pr_continual_grad_result_t* result)
{
    if (!bridge || !gradients || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_apply_protection: required parameter is NULL (bridge, gradients)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_apply_p", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, gradients, sizeof(*gradients));

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_time_us = nimcp_time_get_us();

    /* Compute gradient norm before */
    float norm_before = compute_l2_norm(gradients, num_params);

    size_t n = (num_params < bridge->fisher_size) ? num_params : bridge->fisher_size;
    size_t modified = 0;
    float total_protection = 0.0f;
    float max_protection = 0.0f;

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        float fisher = bridge->fisher_diag[i];
        if (fisher < PR_CONTINUAL_FISHER_EPSILON) continue;

        /* Protection factor based on Fisher importance */
        float protection = fisher * bridge->config.ewc_lambda;
        float scale = 1.0f / (1.0f + protection);

        /* Apply protection */
        gradients[i] *= scale;

        modified++;
        total_protection += protection;
        if (protection > max_protection) {
            max_protection = protection;
        }
    }

    /* Compute gradient norm after */
    float norm_after = compute_l2_norm(gradients, num_params);

    /* Fill result if provided */
    if (result) {
        result->num_parameters = num_params;
        result->num_modified = modified;
        result->total_protection = total_protection;
        result->avg_protection = (modified > 0) ? total_protection / (float)modified : 0.0f;
        result->max_protection = max_protection;
        result->gradient_norm_before = norm_before;
        result->gradient_norm_after = norm_after;
        result->protection_loss = bridge->stats.ewc_loss_current;
    }

    /* Update statistics */
    bridge->stats.total_protections++;
    bridge->stats.avg_protection_strength =
        bridge->stats.avg_protection_strength * 0.99f +
        (modified > 0 ? total_protection / (float)modified : 0.0f) * 0.01f;
    if (max_protection > bridge->stats.max_protection_applied) {
        bridge->stats.max_protection_applied = max_protection;
    }

    uint64_t end_time_us = nimcp_time_get_us();
    bridge->stats.avg_protection_time_us =
        bridge->stats.avg_protection_time_us * 0.99f +
        (float)(end_time_us - start_time_us) * 0.01f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_apply_protection_nodes(
    pr_continual_bridge_t bridge,
    float* gradients,
    const pr_memory_node_t** nodes,
    size_t num_params,
    pr_continual_grad_result_t* result)
{
    if (!bridge || !gradients || !nodes || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_apply_protection_nodes: required parameter is NULL (bridge, gradients, nodes)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_apply_p", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, gradients, sizeof(*gradients));

    nimcp_mutex_lock(bridge->base.mutex);

    float norm_before = compute_l2_norm(gradients, num_params);

    size_t n = (num_params < bridge->fisher_size) ? num_params : bridge->fisher_size;
    size_t modified = 0;
    float total_protection = 0.0f;
    float max_protection = 0.0f;

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        float fisher = bridge->fisher_diag[i];

        /* Add node-specific importance if node provided */
        float node_importance = 1.0f;
        if (nodes[i]) {
            node_importance = pr_continual_combined_importance(bridge, nodes[i], fisher);
        }

        float protection = fisher * node_importance * bridge->config.ewc_lambda;
        if (protection < PR_CONTINUAL_EPSILON) continue;

        float scale = 1.0f / (1.0f + protection);
        gradients[i] *= scale;

        modified++;
        total_protection += protection;
        if (protection > max_protection) {
            max_protection = protection;
        }
    }

    float norm_after = compute_l2_norm(gradients, num_params);

    if (result) {
        result->num_parameters = num_params;
        result->num_modified = modified;
        result->total_protection = total_protection;
        result->avg_protection = (modified > 0) ? total_protection / (float)modified : 0.0f;
        result->max_protection = max_protection;
        result->gradient_norm_before = norm_before;
        result->gradient_norm_after = norm_after;
        result->protection_loss = bridge->stats.ewc_loss_current;
    }

    bridge->stats.total_protections++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_preview_protection(
    pr_continual_bridge_t bridge,
    const float* gradients,
    size_t num_params,
    float* protection_factors)
{
    if (!bridge || !gradients || !protection_factors || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_preview_protection: required parameter is NULL (bridge, gradients, protection_factors)");
        return -1;
        BRIDGE_BBB_VALIDATE(bridge, gradients, sizeof(*gradients));
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_preview", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    size_t n = (num_params < bridge->fisher_size) ? num_params : bridge->fisher_size;

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)n);
        }

        float fisher = bridge->fisher_diag[i];
        float protection = fisher * bridge->config.ewc_lambda;
        protection_factors[i] = 1.0f / (1.0f + protection);
    }

    /* Set remaining to 1.0 (no protection) */
    for (size_t i = n; i < num_params; i++) {
        protection_factors[i] = 1.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Task Boundary Functions
//=============================================================================

int pr_continual_task_boundary(
    pr_continual_bridge_t bridge,
    uint32_t task_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_task_boundary: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_task_bo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Create task info record */
    pr_continual_task_info_t info;
    memset(&info, 0, sizeof(info));

    info.task_id = task_id;
    info.end_time_ms = nimcp_time_get_ms();

    /* Get start time from stats or use session start */
    if (bridge->stats.total_tasks > 0 && bridge->task_history_count > 0) {
        info.start_time_ms = bridge->task_history[bridge->task_history_count - 1].end_time_ms;
    } else {
        info.start_time_ms = bridge->session_start_ms;
    }

    info.fisher_computed = (bridge->fisher_size > 0);
    info.final_loss = bridge->stats.ewc_loss_current;

    /* Add to history */
    add_task_to_history(bridge, &info);

    /* Update current task */
    bridge->current_task_id = task_id + 1;
    bridge->stats.total_tasks++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_store_params(
    pr_continual_bridge_t bridge,
    const float* params,
    size_t num_params)
{
    if (!bridge || !params || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_store_params: required parameter is NULL (bridge, params)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_store_p", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, params, sizeof(*params));

    nimcp_mutex_lock(bridge->base.mutex);

    if (ensure_fisher_capacity(bridge, num_params) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_store_params: validation failed");
        return -1;
    }

    memcpy(bridge->old_params, params, num_params * sizeof(float));
    bridge->params_stored = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float pr_continual_get_stored_param(
    pr_continual_bridge_t bridge,
    size_t param_idx)
{
    if (!bridge) return NAN;
    if (!bridge->params_stored) return NAN;
    if (param_idx >= bridge->fisher_size) return NAN;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_get_sto", 0.0f);


    return bridge->old_params[param_idx];
}

int pr_continual_consolidate_task(
    pr_continual_bridge_t bridge,
    z_ladder_t ladder,
    entangle_graph_t graph,
    uint32_t task_id)
{
    if (!bridge || !ladder || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_consolidate_task: required parameter is NULL (bridge, ladder, graph)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_consoli", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t consolidated = 0;
    uint64_t pruned = 0;

    /* Iterate through tiers Z0-Z2 looking for promotion candidates */
    for (int t = 0; t < PR_CONTINUAL_NUM_TIERS - 1; t++) {
        pr_memory_tier_t z_tier = (pr_memory_tier_t)t;
        size_t tier_size = z_ladder_get_count(ladder, z_tier);
        if (tier_size == 0) continue;

        /* Get all nodes from tier */
        pr_memory_node_t** tier_nodes = (pr_memory_node_t**)nimcp_calloc(tier_size, sizeof(pr_memory_node_t*));
        if (!tier_nodes) continue;

        size_t actual_count = 0;
        if (z_ladder_get_nodes(ladder, z_tier, tier_nodes, tier_size, &actual_count) != Z_LADDER_SUCCESS) {
            nimcp_free(tier_nodes);
            tier_nodes = NULL;
            continue;
        }

        for (size_t idx = 0; idx < actual_count; idx++) {
            /* Phase 8: Loop progress heartbeat */
            if ((idx & 0xFF) == 0 && actual_count > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(idx + 1) / (float)actual_count);
            }

            pr_memory_node_t* node = tier_nodes[idx];
            if (!node) continue;

            /* Compute importance */
            float importance = pr_continual_combined_importance(bridge, node, 0.0f);

            /* Check for promotion eligibility */
            float promotion_thresh = bridge->config.protection_threshold[t + 1];
            if (importance >= promotion_thresh) {
                /* Promote to next tier */
                uint64_t node_id = pr_memory_node_get_id(node);
                if (z_ladder_promote(ladder, node_id) == 0) {
                    consolidated++;
                }
            }
        }

        nimcp_free(tier_nodes);
        tier_nodes = NULL;
    }

    /* Prune low-importance edges from entanglement graph */
    /* Simplified: would iterate through edges in full implementation */

    bridge->stats.memories_consolidated += consolidated;
    bridge->stats.memories_pruned += pruned;

    /* Update task info */
    if (bridge->task_history_count > 0) {
        bridge->task_history[bridge->task_history_count - 1].memories_consolidated =
            (uint32_t)consolidated;
    }

    (void)task_id;  /* Used for tagging in full implementation */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Statistics and Information
//=============================================================================

int pr_continual_get_stats(
    pr_continual_bridge_t bridge,
    pr_continual_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_get_sta", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_continual_get_task_info(
    pr_continual_bridge_t bridge,
    uint32_t task_id,
    pr_continual_task_info_t* info)
{
    if (!bridge || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_get_task_info: required parameter is NULL (bridge, info)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_get_tas", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Search for task */
    bool found = false;
    for (uint32_t i = 0; i < bridge->task_history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_history_count > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)bridge->task_history_count);
        }

        if (bridge->task_history[i].task_id == task_id) {
            *info = bridge->task_history[i];
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return found ? 0 : -1;
}

uint32_t pr_continual_get_current_task(pr_continual_bridge_t bridge) {
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_get_cur", 0.0f);


    return bridge->current_task_id;
}

size_t pr_continual_get_num_params(pr_continual_bridge_t bridge) {
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_get_num", 0.0f);


    return bridge->fisher_size;
}

void pr_continual_reset_stats(pr_continual_bridge_t bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_reset_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(pr_continual_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_continual_type_name(pr_continual_type_t type) {
    switch (type) {
        case PR_CONTINUAL_EWC:                return "EWC";
        case PR_CONTINUAL_RESONANCE_REPLAY:   return "Resonance Replay";
        case PR_CONTINUAL_QUATERNION_PROTECTED: return "Quaternion Protected";
        case PR_CONTINUAL_ENTANGLEMENT_AWARE: return "Entanglement Aware";
        case PR_CONTINUAL_COMBINED:           return "Combined";
        default:                              return "Unknown";
    }
}

const char* pr_continual_tier_name(pr_continual_tier_t tier) {
    switch (tier) {
        case PR_CONTINUAL_TIER_Z0: return "Z0 (Working)";
        case PR_CONTINUAL_TIER_Z1: return "Z1 (Short-term)";
        case PR_CONTINUAL_TIER_Z2: return "Z2 (Long-term)";
        case PR_CONTINUAL_TIER_Z3: return "Z3 (Permanent)";
        default:                   return "Unknown";
    }
}

void pr_continual_print_stats(pr_continual_bridge_t bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_print_s", 0.0f);


    pr_continual_stats_t stats;
    if (pr_continual_get_stats(bridge, &stats) != 0) return;

    printf("=== Prime Resonant Continual Learning Bridge Statistics ===\n");
    printf("\nStrategy: %s\n", pr_continual_type_name(bridge->config.type));

    printf("\nTask Statistics:\n");
    printf("  Total tasks:          %u\n", stats.total_tasks);
    printf("  Current task ID:      %u\n", stats.current_task_id);

    printf("\nProtection Statistics:\n");
    printf("  Total protections:    %lu\n", (unsigned long)stats.total_protections);
    printf("  Avg protection str:   %.4f\n", stats.avg_protection_strength);
    printf("  Max protection:       %.4f\n", stats.max_protection_applied);

    printf("\nFisher Statistics:\n");
    printf("  Fisher computations:  %lu\n", (unsigned long)stats.fisher_computations);
    printf("  Avg Fisher value:     %.6f\n", stats.avg_fisher_value);
    printf("  Fisher sparsity:      %.2f%%\n", stats.fisher_sparsity * 100.0f);
    printf("  Avg Fisher time:      %.2f ms\n", stats.avg_fisher_time_ms);

    printf("\nReplay Statistics:\n");
    printf("  Total replay samples: %lu\n", (unsigned long)stats.replay_samples_total);
    printf("  Avg replay importance: %.4f\n", stats.avg_replay_importance);
    printf("  Samples per tier:\n");
    for (int t = 0; t < PR_CONTINUAL_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CONTINUAL_NUM_TIERS > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(t + 1) / (float)PR_CONTINUAL_NUM_TIERS);
        }

        printf("    %s: %lu\n", pr_continual_tier_name((pr_continual_tier_t)t),
               (unsigned long)stats.replay_per_tier[t]);
    }

    printf("\nConsolidation Statistics:\n");
    printf("  Memories consolidated: %lu\n", (unsigned long)stats.memories_consolidated);
    printf("  Memories pruned:       %lu\n", (unsigned long)stats.memories_pruned);

    printf("\nEWC Loss:\n");
    printf("  Current:              %.6f\n", stats.ewc_loss_current);
    printf("  Average:              %.6f\n", stats.ewc_loss_avg);
    printf("  Maximum:              %.6f\n", stats.ewc_loss_max);

    printf("\nTiming:\n");
    printf("  Avg protection time:  %.2f us\n", stats.avg_protection_time_us);

    printf("============================================================\n");
}

void pr_continual_print_task_info(const pr_continual_task_info_t* info) {
    if (!info) return;

    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_print_t", 0.0f);


    printf("Task %u Information:\n", info->task_id);
    printf("  Duration:           %lu ms\n",
           (unsigned long)(info->end_time_ms - info->start_time_ms));
    printf("  Samples processed:  %u\n", info->num_samples);
    printf("  Final loss:         %.6f\n", info->final_loss);
    printf("  Avg resonance:      %.4f\n", info->avg_resonance);
    printf("  Memories consolidated: %u\n", info->memories_consolidated);
    printf("  Fisher computed:    %s\n", info->fisher_computed ? "Yes" : "No");
}

bool pr_continual_validate(pr_continual_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_continual_validate: bridge is NULL");
        return false;
    }

    /* Check Fisher array consistency */
    if (bridge->fisher_size > bridge->fisher_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_validate: validation failed");
        return false;
    }

    /* Check Fisher values are valid */
    /* Phase 8: Heartbeat at operation start */
    pr_continual_bridge_heartbeat("pr_continual_pr_continual_validat", 0.0f);


    for (size_t i = 0; i < bridge->fisher_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->fisher_size > 256) {
            pr_continual_bridge_heartbeat("pr_continual_loop",
                             (float)(i + 1) / (float)bridge->fisher_size);
        }

        if (isnan(bridge->fisher_diag[i]) || isinf(bridge->fisher_diag[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_validate: validation failed");
            return false;
        }
        if (bridge->fisher_diag[i] < 0.0f) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_validate: validation failed");
            return false;
        }
    }

    /* Check old params if stored */
    if (bridge->params_stored) {
        for (size_t i = 0; i < bridge->fisher_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->fisher_size > 256) {
                pr_continual_bridge_heartbeat("pr_continual_loop",
                                 (float)(i + 1) / (float)bridge->fisher_size);
            }

            if (isnan(bridge->old_params[i]) || isinf(bridge->old_params[i])) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_validate: validation failed");
                return false;
            }
        }
    }

    /* Check task history */
    if (bridge->task_history_count > bridge->task_history_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_continual_validate: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_continual_bridge_set_instance_health_agent(
    pr_continual_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_continual_bridge_training_begin(pr_continual_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_continual_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_continual_bridge_heartbeat_instance(bridge->health_agent, "pr_continual_bridge_training_begin", 0.0f);
    return 0;
}

int pr_continual_bridge_training_end(pr_continual_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_continual_bridge_training_end: NULL argument");
        return -1;
    }
    pr_continual_bridge_heartbeat_instance(bridge->health_agent, "pr_continual_bridge_training_end", 1.0f);
    return 0;
}

int pr_continual_bridge_training_step(pr_continual_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_continual_bridge_training_step: NULL argument");
        return -1;
    }
    pr_continual_bridge_heartbeat_instance(bridge->health_agent, "pr_continual_bridge_training_step", progress);
    return 0;
}
