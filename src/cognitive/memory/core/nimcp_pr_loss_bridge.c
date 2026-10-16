/**
 * @file nimcp_pr_loss_bridge.c
 * @brief Prime Resonant Loss Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of memory-aware loss functions for Prime Resonant
 *       cognitive architecture training
 * WHY:  Enable training that respects memory geometry (quaternion manifold),
 *       semantic similarity (resonance), and consolidation state
 * HOW:  Implements geodesic, triplet, consolidation, entanglement, and
 *       tier-aware losses with proper gradient computation
 */

#include "cognitive/memory/core/nimcp_pr_loss_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <float.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_loss_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_loss_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_loss_bridge_mesh_registry = NULL;

nimcp_error_t pr_loss_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_loss_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_loss_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_loss_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_loss_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_loss_bridge_mesh_registry = registry;
    return err;
}

void pr_loss_bridge_mesh_unregister(void) {
    if (g_pr_loss_bridge_mesh_registry && g_pr_loss_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_loss_bridge_mesh_registry, g_pr_loss_bridge_mesh_id);
        g_pr_loss_bridge_mesh_id = 0;
        g_pr_loss_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_loss_bridge module (instance-level) */
static inline void pr_loss_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_loss_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_loss_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_loss_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_LOSS_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete state for loss bridge
 * WHY:  Encapsulate all bridge data for thread safety
 */
struct pr_loss_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_loss_config_t config;

    /* Statistics */
    pr_loss_stats_t stats;

    /* Resonance config for triplet loss */
    resonance_config_t resonance_config;

    /* Caches for batch operations */
    float* loss_cache;
    size_t loss_cache_capacity;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_loss_bridge, struct pr_loss_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Safe arc cosine with clamping
 */
static inline float safe_acos(float x) {
    return acosf(nimcp_myelin_clamp(x, -1.0f, 1.0f));
}

/**
 * @brief Get current time in nanoseconds (from microseconds)
 */
static inline uint64_t get_time_ns(void) {
    return nimcp_time_get_us() * 1000ULL;
}

/**
 * @brief Compute quaternion dot product
 */
static inline float quat_dot_inline(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    return q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;
}

/**
 * @brief Normalize quaternion inline
 */
static inline nimcp_quaternion_t quat_normalize_inline(nimcp_quaternion_t q) {
    float mag = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (mag < PR_LOSS_EPSILON) {
        return quat_identity();
    }
    float inv_mag = 1.0f / mag;
    return quat_create(q.w * inv_mag, q.x * inv_mag, q.y * inv_mag, q.z * inv_mag);
}

/**
 * @brief Get consolidation from memory node
 */
static inline float get_consolidation(const pr_memory_node_t* node) {
    if (!node) return 0.0f;
    return node->state.w;  /* w component is consolidation */
}

/**
 * @brief Get tier from memory node
 */
static inline pr_memory_tier_t get_tier(const pr_memory_node_t* node) {
    if (!node) return PR_MEMORY_TIER_Z0;
    return node->tier;
}

/**
 * @brief Ensure loss cache has sufficient capacity
 */
static int ensure_cache_capacity(pr_loss_bridge_t bridge, size_t needed) {
    if (bridge->loss_cache_capacity >= needed) {
        return 0;
    }

    size_t new_capacity = needed * 2;
    float* new_cache = nimcp_realloc(bridge->loss_cache, new_capacity * sizeof(float));
    if (!new_cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_cache");

        return -1;
    }

    bridge->loss_cache = new_cache;
    bridge->loss_cache_capacity = new_capacity;
    return 0;
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_loss_config_t pr_loss_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_config_defau", 0.0f);


    pr_loss_config_t config;
    memset(&config, 0, sizeof(config));

    config.geodesic_weight = PR_LOSS_DEFAULT_GEODESIC_WEIGHT;
    config.triplet_weight = PR_LOSS_DEFAULT_TRIPLET_WEIGHT;
    config.triplet_margin = PR_LOSS_DEFAULT_TRIPLET_MARGIN;
    config.consolidation_weight = PR_LOSS_DEFAULT_CONSOLIDATION_WEIGHT;
    config.consolidation_power = PR_LOSS_DEFAULT_CONSOLIDATION_POWER;
    config.entanglement_lambda = PR_LOSS_DEFAULT_ENTANGLEMENT_LAMBDA;

    config.tier_weights[PR_MEMORY_TIER_Z0] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z0;
    config.tier_weights[PR_MEMORY_TIER_Z1] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z1;
    config.tier_weights[PR_MEMORY_TIER_Z2] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z2;
    config.tier_weights[PR_MEMORY_TIER_Z3] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z3;

    return config;
}

pr_loss_config_t pr_loss_config_state_learning(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_config_state", 0.0f);


    pr_loss_config_t config = pr_loss_config_default();

    /* Emphasize geodesic loss for state learning */
    config.geodesic_weight = 0.6f;
    config.triplet_weight = 0.2f;
    config.consolidation_weight = 0.1f;
    config.entanglement_lambda = 0.05f;

    /* Lower consolidation protection for active learning */
    config.consolidation_power = 1.0f;

    return config;
}

pr_loss_config_t pr_loss_config_retrieval(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_config_retri", 0.0f);


    pr_loss_config_t config = pr_loss_config_default();

    /* Emphasize triplet loss for retrieval training */
    config.geodesic_weight = 0.2f;
    config.triplet_weight = 0.5f;
    config.triplet_margin = 0.3f;  /* Higher margin */
    config.consolidation_weight = 0.15f;
    config.entanglement_lambda = 0.15f;

    return config;
}

pr_loss_config_t pr_loss_config_fine_tuning(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_config_fine_", 0.0f);


    pr_loss_config_t config = pr_loss_config_default();

    /* Low overall weights with high consolidation protection */
    config.geodesic_weight = 0.15f;
    config.triplet_weight = 0.15f;
    config.consolidation_weight = 0.4f;
    config.consolidation_power = 3.0f;  /* Stronger protection */
    config.entanglement_lambda = 0.05f;

    /* Lower tier weights for protection */
    config.tier_weights[PR_MEMORY_TIER_Z1] = 0.5f;
    config.tier_weights[PR_MEMORY_TIER_Z2] = 0.2f;
    config.tier_weights[PR_MEMORY_TIER_Z3] = 0.05f;

    return config;
}

bool pr_loss_config_validate(const pr_loss_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_config_validate: config is NULL");
        return false;
    }

    /* Weight validation */
    if (config->geodesic_weight < 0.0f) {
        return false;
    }
    if (config->triplet_weight < 0.0f) {
        return false;
    }
    if (config->consolidation_weight < 0.0f) {
        return false;
    }
    if (config->entanglement_lambda < 0.0f) {
        return false;
    }

    /* At least one component should be active */
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_config_valid", 0.0f);


    float total_weight = config->geodesic_weight + config->triplet_weight +
                        config->consolidation_weight + config->entanglement_lambda;
    if (total_weight <= 0.0f) {
        return false;
    }

    /* Triplet margin must be positive */
    if (config->triplet_margin <= 0.0f) {
        return false;
    }

    /* Consolidation power must be positive */
    if (config->consolidation_power <= 0.0f) {
        return false;
    }

    /* Tier weight validation */
    for (int t = 0; t < PR_LOSS_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_LOSS_NUM_TIERS > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(t + 1) / (float)PR_LOSS_NUM_TIERS);
        }

        if (config->tier_weights[t] < 0.0f || config->tier_weights[t] > 1.0f) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

pr_loss_bridge_t pr_loss_bridge_create(const pr_loss_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_create", 0.0f);


    pr_loss_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_loss_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        if (!pr_loss_config_validate(config)) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_bridge_create: pr_loss_config_validate is NULL");
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = pr_loss_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_loss") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_loss_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize resonance config for triplet loss */
    bridge->resonance_config = resonance_config_default();

    /* Initialize loss cache */
    bridge->loss_cache_capacity = 256;
    bridge->loss_cache = nimcp_calloc(bridge->loss_cache_capacity, sizeof(float));
    if (!bridge->loss_cache) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_loss_bridge_create: bridge->loss_cache is NULL");
        return NULL;
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(pr_loss_stats_t));
    bridge->stats.min_combined_loss = FLT_MAX;

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_loss");
    return bridge;
}

void pr_loss_bridge_destroy(pr_loss_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_loss");

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_destroy", 0.0f);


    if (bridge->loss_cache) {
        nimcp_free(bridge->loss_cache);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_loss_bridge_reset(pr_loss_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pr_loss_stats_t));
    bridge->stats.min_combined_loss = FLT_MAX;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_loss_bridge_set_config(
    pr_loss_bridge_t bridge,
    const pr_loss_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, config, sizeof(*config));
    if (!pr_loss_config_validate(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_loss_bridge_set_config: pr_loss_config_validate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_set_config", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_loss_bridge_get_config(
    pr_loss_bridge_t bridge,
    pr_loss_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_bridge_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_get_config", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, config, sizeof(*config));

    nimcp_mutex_lock(bridge->base.mutex);
    *config = bridge->config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Quaternion Geodesic Loss Functions
//=============================================================================

float pr_loss_geodesic(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t q1,
    nimcp_quaternion_t q2)
{
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_geodesic", 0.0f);


    uint64_t start_time = 0;
    if (bridge) {
        start_time = get_time_ns();
    }

    /* Normalize quaternions */
    q1 = quat_normalize_inline(q1);
    q2 = quat_normalize_inline(q2);

    /* Compute dot product */
    float dot = quat_dot_inline(q1, q2);

    /* Handle antipodal quaternions (q and -q represent same state) */
    dot = fabsf(dot);

    /* Clamp for numerical stability */
    dot = nimcp_myelin_clamp(dot, 0.0f, 1.0f);

    /* Geodesic distance = arccos(|q1 . q2|) */
    float geodesic = safe_acos(dot);

    /* Normalize to [0, 1] */
    float loss = geodesic / (float)M_PI;

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.geodesic_computations++;
        bridge->stats.total_geodesic_loss += loss;
        bridge->stats.total_compute_time_ns += (get_time_ns() - start_time);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return loss;
}

float pr_loss_geodesic_batch(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* quats1,
    const nimcp_quaternion_t* quats2,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float* per_sample_loss)
{
    BRIDGE_BBB_VALIDATE(bridge, per_sample_loss, sizeof(*per_sample_loss));
    if (!quats1 || !quats2 || count == 0) return -1.0f;
    if (count > PR_LOSS_MAX_BATCH_SIZE) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_geodesic_bat", 0.0f);


    uint64_t start_time = 0;
    if (bridge) {
        start_time = get_time_ns();
    }

    float sum = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        /* Normalize */
        nimcp_quaternion_t q1 = quat_normalize_inline(quats1[i]);
        nimcp_quaternion_t q2 = quat_normalize_inline(quats2[i]);

        /* Compute geodesic */
        float dot = fabsf(quat_dot_inline(q1, q2));
        dot = nimcp_myelin_clamp(dot, 0.0f, 1.0f);
        float loss = safe_acos(dot) / (float)M_PI;

        if (per_sample_loss) {
            per_sample_loss[i] = loss;
        }

        sum += loss;
    }

    float result = 0.0f;
    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            result = sum / (float)count;
            break;
        case NIMCP_LOSS_REDUCE_SUM:
            result = sum;
            break;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            result = sum;  /* Return sum, per-sample in array */
            break;
    }

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.geodesic_computations += count;
        bridge->stats.total_geodesic_loss += sum;
        bridge->stats.total_compute_time_ns += (get_time_ns() - start_time);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int pr_loss_gradient_geodesic(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t q1,
    nimcp_quaternion_t q2,
    pr_loss_quat_gradient_t* grad)
{
    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_gradient_geodesic: grad is NULL");
        return -1;
    }

    /* Normalize */
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_gradient_geo", 0.0f);


    q1 = quat_normalize_inline(q1);
    q2 = quat_normalize_inline(q2);

    /* Compute dot product */
    float dot = quat_dot_inline(q1, q2);

    /* Handle sign for shortest path */
    float sign = (dot >= 0.0f) ? 1.0f : -1.0f;
    dot = fabsf(dot);

    /* Clamp for stability */
    dot = nimcp_myelin_clamp(dot, 0.0f, 1.0f - PR_LOSS_EPSILON);

    /* Compute derivative factor */
    /* d(arccos(|dot|))/d(q1) = -sign(dot) * q2 / sqrt(1 - dot^2) */
    float sin_theta = sqrtf(1.0f - dot * dot);
    if (sin_theta < PR_LOSS_EPSILON) {
        /* Nearly identical quaternions - zero gradient */
        grad->dw = 0.0f;
        grad->dx = 0.0f;
        grad->dy = 0.0f;
        grad->dz = 0.0f;
        return 0;
    }

    float factor = -sign / ((float)M_PI * sin_theta);

    /* Raw gradient (in embedding space) */
    float raw_dw = factor * q2.w;
    float raw_dx = factor * q2.x;
    float raw_dy = factor * q2.y;
    float raw_dz = factor * q2.z;

    /* Project to tangent space of hypersphere at q1 */
    /* grad_tangent = grad - (grad . q1) * q1 */
    float proj = raw_dw * q1.w + raw_dx * q1.x + raw_dy * q1.y + raw_dz * q1.z;

    grad->dw = raw_dw - proj * q1.w;
    grad->dx = raw_dx - proj * q1.x;
    grad->dy = raw_dy - proj * q1.y;
    grad->dz = raw_dz - proj * q1.z;

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.gradient_computations++;
        float norm = pr_loss_gradient_norm(grad);
        bridge->stats.avg_gradient_norm =
            (bridge->stats.avg_gradient_norm * (bridge->stats.gradient_computations - 1) + norm) /
            bridge->stats.gradient_computations;
        if (norm > bridge->stats.max_gradient_norm) {
            bridge->stats.max_gradient_norm = norm;
        }
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int pr_loss_gradient_geodesic_batch(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* quats1,
    const nimcp_quaternion_t* quats2,
    size_t count,
    pr_loss_quat_gradient_t* grads)
{
    if (!quats1 || !quats2 || !grads || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_gradient_geodesic_batch: required parameter is NULL (quats1, quats2, grads)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_gradient_geo", 0.0f);


    int computed = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        if (pr_loss_gradient_geodesic(bridge, quats1[i], quats2[i], &grads[i]) == 0) {
            computed++;
        }
    }

    return computed;
}

//=============================================================================
// Resonance-Triplet Loss Functions
//=============================================================================

float pr_loss_resonance_triplet(
    pr_loss_bridge_t bridge,
    const pr_memory_node_t* anchor,
    const pr_memory_node_t* positive,
    const pr_memory_node_t* negative)
{
    if (!anchor || !positive || !negative) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_resonance_tr", 0.0f);


    uint64_t start_time = 0;
    float margin = PR_LOSS_DEFAULT_TRIPLET_MARGIN;

    if (bridge) {
        start_time = get_time_ns();
        margin = bridge->config.triplet_margin;
    }

    /* Build resonance queries */
    resonance_query_t query;
    resonance_target_init((resonance_target_t*)&query);
    query.signature = (prime_signature_t*)pr_memory_node_get_signature(anchor);
    query.quaternion = pr_memory_node_get_state(anchor);
    query.phase = 0.0f;  /* Phase not used for static comparison */
    query.module_id = 0;

    /* Compute resonance to positive */
    resonance_target_t pos_target;
    resonance_target_init(&pos_target);
    pos_target.signature = (prime_signature_t*)pr_memory_node_get_signature(positive);
    pos_target.quaternion = pr_memory_node_get_state(positive);
    pos_target.phase = 0.0f;
    pos_target.module_id = 0;

    resonance_result_t pos_result;
    resonance_config_t res_config = resonance_config_default();
    if (!resonance_compute(&query, &pos_target, &res_config, NULL, &pos_result)) {
        return -1.0f;
    }
    float res_positive = pos_result.total;

    /* Compute resonance to negative */
    resonance_target_t neg_target;
    resonance_target_init(&neg_target);
    neg_target.signature = (prime_signature_t*)pr_memory_node_get_signature(negative);
    neg_target.quaternion = pr_memory_node_get_state(negative);
    neg_target.phase = 0.0f;
    neg_target.module_id = 0;

    resonance_result_t neg_result;
    if (!resonance_compute(&query, &neg_target, &res_config, NULL, &neg_result)) {
        return -1.0f;
    }
    float res_negative = neg_result.total;

    /* Triplet loss: max(0, res_neg - res_pos + margin) */
    /* We want res_positive > res_negative (similar should have higher resonance) */
    float loss = fmaxf(0.0f, res_negative - res_positive + margin);

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.triplet_computations++;
        bridge->stats.total_triplet_loss += loss;
        if (loss > 0.0f) {
            bridge->stats.active_triplets++;
            bridge->stats.avg_margin_violation =
                (bridge->stats.avg_margin_violation * (bridge->stats.active_triplets - 1) +
                 (res_negative - res_positive + margin)) / bridge->stats.active_triplets;
        } else {
            bridge->stats.inactive_triplets++;
        }
        bridge->stats.total_compute_time_ns += (get_time_ns() - start_time);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return loss;
}

float pr_loss_resonance_triplet_quat(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t anchor,
    nimcp_quaternion_t positive,
    nimcp_quaternion_t negative)
{
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_resonance_tr", 0.0f);


    float margin = PR_LOSS_DEFAULT_TRIPLET_MARGIN;
    if (bridge) {
        margin = bridge->config.triplet_margin;
    }

    /* Use quaternion similarity (1 - geodesic distance) as "resonance" */
    float sim_positive = 1.0f - pr_loss_geodesic(NULL, anchor, positive);
    float sim_negative = 1.0f - pr_loss_geodesic(NULL, anchor, negative);

    /* Triplet loss: max(0, sim_neg - sim_pos + margin) */
    float loss = fmaxf(0.0f, sim_negative - sim_positive + margin);

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.triplet_computations++;
        bridge->stats.total_triplet_loss += loss;
        if (loss > 0.0f) {
            bridge->stats.active_triplets++;
        } else {
            bridge->stats.inactive_triplets++;
        }
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return loss;
}

float pr_loss_resonance_triplet_batch(
    pr_loss_bridge_t bridge,
    const pr_memory_node_t* const* anchors,
    const pr_memory_node_t* const* positives,
    const pr_memory_node_t* const* negatives,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float* per_sample_loss)
{
    BRIDGE_BBB_VALIDATE(bridge, per_sample_loss, sizeof(*per_sample_loss));
    if (!anchors || !positives || !negatives || count == 0) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_resonance_tr", 0.0f);


    float sum = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        float loss = pr_loss_resonance_triplet(bridge, anchors[i], positives[i], negatives[i]);
        if (loss < 0.0f) {
            loss = 0.0f;  /* Skip invalid triplets */
        }
        if (per_sample_loss) {
            per_sample_loss[i] = loss;
        }
        sum += loss;
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        default:
            return sum;
    }
}

int pr_loss_gradient_triplet(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t anchor,
    nimcp_quaternion_t positive,
    nimcp_quaternion_t negative,
    pr_loss_triplet_gradient_t* grads)
{
    if (!grads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_gradient_triplet: grads is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_gradient_tri", 0.0f);


    float margin = PR_LOSS_DEFAULT_TRIPLET_MARGIN;
    if (bridge) {
        margin = bridge->config.triplet_margin;
    }

    /* Compute similarities */
    float geo_pos = pr_loss_geodesic(NULL, anchor, positive);
    float geo_neg = pr_loss_geodesic(NULL, anchor, negative);
    float sim_pos = 1.0f - geo_pos;
    float sim_neg = 1.0f - geo_neg;

    /* Check if triplet is active */
    float violation = sim_neg - sim_pos + margin;
    grads->margin_violation = violation;
    grads->is_active = (violation > 0.0f);

    if (!grads->is_active) {
        /* Zero gradients for inactive triplet */
        memset(&grads->anchor, 0, sizeof(pr_loss_quat_gradient_t));
        memset(&grads->positive, 0, sizeof(pr_loss_quat_gradient_t));
        memset(&grads->negative, 0, sizeof(pr_loss_quat_gradient_t));
        return 0;
    }

    /* Compute geodesic gradients */
    pr_loss_quat_gradient_t grad_pos, grad_neg;
    pr_loss_gradient_geodesic(NULL, anchor, positive, &grad_pos);
    pr_loss_gradient_geodesic(NULL, anchor, negative, &grad_neg);

    /* Triplet gradient:
     * L = max(0, sim_neg - sim_pos + margin)
     * dL/d_anchor = d(sim_neg)/d_anchor - d(sim_pos)/d_anchor
     *             = -d(geo_neg)/d_anchor + d(geo_pos)/d_anchor
     * dL/d_positive = -d(sim_pos)/d_positive = d(geo_pos)/d_positive
     * dL/d_negative = d(sim_neg)/d_negative = -d(geo_neg)/d_negative
     */

    /* Anchor gradient */
    grads->anchor.dw = grad_pos.dw - grad_neg.dw;
    grads->anchor.dx = grad_pos.dx - grad_neg.dx;
    grads->anchor.dy = grad_pos.dy - grad_neg.dy;
    grads->anchor.dz = grad_pos.dz - grad_neg.dz;

    /* Positive gradient - want to increase similarity (decrease geodesic) */
    pr_loss_gradient_geodesic(NULL, positive, anchor, &grads->positive);

    /* Negative gradient - want to decrease similarity (increase geodesic) */
    pr_loss_gradient_geodesic(NULL, negative, anchor, &grad_neg);
    grads->negative.dw = -grad_neg.dw;
    grads->negative.dx = -grad_neg.dx;
    grads->negative.dy = -grad_neg.dy;
    grads->negative.dz = -grad_neg.dz;

    return 0;
}

//=============================================================================
// Consolidation-Weighted Loss Functions
//=============================================================================

float pr_loss_consolidation_weighted(
    pr_loss_bridge_t bridge,
    const float* predictions,
    const float* targets,
    const pr_memory_node_t* const* nodes,
    size_t count)
{
    BRIDGE_BBB_VALIDATE(bridge, predictions, sizeof(*predictions));
    if (!predictions || !targets || !nodes || count == 0) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_consolidatio", 0.0f);


    uint64_t start_time = 0;
    float power = PR_LOSS_DEFAULT_CONSOLIDATION_POWER;

    if (bridge) {
        start_time = get_time_ns();
        power = bridge->config.consolidation_power;
    }

    float weighted_sum = 0.0f;
    float weight_sum = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        float consolidation = get_consolidation(nodes[i]);

        /* Weight = 1 - consolidation^power */
        /* High consolidation -> low weight -> protected */
        float weight = 1.0f - powf(nimcp_myelin_clamp(consolidation, 0.0f, 1.0f), power);
        weight = nimcp_myelin_clamp(weight, PR_LOSS_EPSILON, 1.0f);

        /* MSE for this sample */
        float diff = predictions[i] - targets[i];
        float mse = diff * diff;

        weighted_sum += weight * mse;
        weight_sum += weight;
    }

    /* Normalize by total weight */
    float loss = (weight_sum > PR_LOSS_EPSILON) ? weighted_sum / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f) : 0.0f;

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.consolidation_computations += count;
        bridge->stats.total_consolidation_loss += loss;
        bridge->stats.total_compute_time_ns += (get_time_ns() - start_time);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return loss;
}

float pr_loss_get_consolidation_weight(
    pr_loss_bridge_t bridge,
    const pr_memory_node_t* node)
{
    if (!node) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_get_consolid", 0.0f);


    float power = PR_LOSS_DEFAULT_CONSOLIDATION_POWER;
    if (bridge) {
        power = bridge->config.consolidation_power;
    }

    float consolidation = get_consolidation(node);
    float weight = 1.0f - powf(nimcp_myelin_clamp(consolidation, 0.0f, 1.0f), power);
    return nimcp_myelin_clamp(weight, PR_LOSS_EPSILON, 1.0f);
}

float pr_loss_consolidation_gradient_scale(
    pr_loss_bridge_t bridge,
    float consolidation)
{
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_consolidatio", 0.0f);


    float power = PR_LOSS_DEFAULT_CONSOLIDATION_POWER;
    if (bridge) {
        power = bridge->config.consolidation_power;
    }

    float weight = 1.0f - powf(nimcp_myelin_clamp(consolidation, 0.0f, 1.0f), power);
    return nimcp_myelin_clamp(weight, PR_LOSS_EPSILON, 1.0f);
}

//=============================================================================
// Entanglement Regularization Functions
//=============================================================================

float pr_loss_entanglement_reg(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph)
{
    if (!graph) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_entanglement", 0.0f);


    uint64_t start_time = 0;
    float lambda = PR_LOSS_DEFAULT_ENTANGLEMENT_LAMBDA;

    if (bridge) {
        start_time = get_time_ns();
        lambda = bridge->config.entanglement_lambda;
    }

    /* Get graph statistics */
    /* Note: In full implementation, would iterate over nodes */
    /* For now, compute simple density-based regularization */

    /* Placeholder: would call entangle_graph_get_stats */
    /* Using simple formula: reg = lambda * (1 - density) */

    /* Assume graph has some measure of connectivity */
    /* This would integrate with nimcp_entanglement.h */
    float density = 0.5f;  /* Placeholder - would query graph */

    float reg = lambda * (1.0f - nimcp_myelin_clamp(density, 0.0f, 1.0f));

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.entanglement_computations++;
        bridge->stats.total_entanglement_reg += reg;
        bridge->stats.total_compute_time_ns += (get_time_ns() - start_time);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return reg;
}

float pr_loss_entanglement_reg_nodes(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph,
    const uint64_t* node_ids,
    size_t count)
{
    BRIDGE_BBB_VALIDATE(bridge, node_ids, sizeof(*node_ids));
    if (!graph || !node_ids || count == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_entanglement", 0.0f);


    float lambda = PR_LOSS_DEFAULT_ENTANGLEMENT_LAMBDA;
    if (bridge) {
        lambda = bridge->config.entanglement_lambda;
    }

    float sum_isolation = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        float score = pr_loss_get_entanglement_score(bridge, graph, node_ids[i]);
        float isolation = 1.0f - score;
        sum_isolation += isolation;
    }

    return lambda * sum_isolation / (float)count;
}

float pr_loss_get_entanglement_score(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t node_id)
{
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_get_entangle", 0.0f);


    (void)bridge;

    if (!graph) return 0.0f;

    /* Would query entanglement graph for node connectivity */
    /* score = tanh(edge_count * avg_weight / normalization) */

    /* Placeholder implementation */
    (void)node_id;
    return 0.5f;  /* Would integrate with nimcp_entanglement.h */
}

int pr_loss_gradient_entanglement(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph,
    float* edge_gradients,
    size_t max_edges)
{
    BRIDGE_BBB_VALIDATE(bridge, edge_gradients, sizeof(*edge_gradients));
    if (!graph || !edge_gradients || max_edges == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_gradient_entanglement: required parameter is NULL (graph, edge_gradients)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_gradient_ent", 0.0f);


    float lambda = PR_LOSS_DEFAULT_ENTANGLEMENT_LAMBDA;
    if (bridge) {
        lambda = bridge->config.entanglement_lambda;
    }

    /* Would iterate over edges and compute gradients */
    /* grad = -lambda / num_edges (encouraging stronger edges) */

    /* Placeholder - would integrate with entanglement graph API */
    (void)lambda;
    memset(edge_gradients, 0, max_edges * sizeof(float));

    return 0;
}

//=============================================================================
// Tier-Aware Loss Functions
//=============================================================================

float pr_loss_tier_aware(
    pr_loss_bridge_t bridge,
    const float* predictions,
    const float* targets,
    const pr_memory_node_t* const* nodes,
    size_t count)
{
    BRIDGE_BBB_VALIDATE(bridge, predictions, sizeof(*predictions));
    if (!predictions || !targets || !nodes || count == 0) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_tier_aware", 0.0f);


    float tier_weights[PR_LOSS_NUM_TIERS];
    if (bridge) {
        for (int t = 0; t < PR_LOSS_NUM_TIERS; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && PR_LOSS_NUM_TIERS > 256) {
                pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                                 (float)(t + 1) / (float)PR_LOSS_NUM_TIERS);
            }

            tier_weights[t] = bridge->config.tier_weights[t];
        }
    } else {
        tier_weights[PR_MEMORY_TIER_Z0] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z0;
        tier_weights[PR_MEMORY_TIER_Z1] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z1;
        tier_weights[PR_MEMORY_TIER_Z2] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z2;
        tier_weights[PR_MEMORY_TIER_Z3] = PR_LOSS_DEFAULT_TIER_WEIGHT_Z3;
    }

    float weighted_sum = 0.0f;
    float weight_sum = 0.0f;
    uint64_t tier_counts[PR_LOSS_NUM_TIERS] = {0};

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        pr_memory_tier_t tier = get_tier(nodes[i]);
        if (tier >= PR_LOSS_NUM_TIERS) tier = PR_MEMORY_TIER_Z0;

        float weight = tier_weights[tier];
        float diff = predictions[i] - targets[i];
        float mse = diff * diff;

        weighted_sum += weight * mse;
        weight_sum += weight;
        tier_counts[tier]++;
    }

    float loss = (weight_sum > PR_LOSS_EPSILON) ? weighted_sum / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f) : 0.0f;

    /* Update statistics */
    if (bridge) {
        nimcp_mutex_lock(bridge->base.mutex);
        for (int t = 0; t < PR_LOSS_NUM_TIERS; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && PR_LOSS_NUM_TIERS > 256) {
                pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                                 (float)(t + 1) / (float)PR_LOSS_NUM_TIERS);
            }

            bridge->stats.samples_per_tier[t] += tier_counts[t];
        }
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return loss;
}

float pr_loss_get_tier_weight(
    pr_loss_bridge_t bridge,
    pr_memory_tier_t tier)
{
    if (tier >= PR_LOSS_NUM_TIERS) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_get_tier_wei", 0.0f);


    if (bridge) {
        return bridge->config.tier_weights[tier];
    }

    /* Default weights */
    static const float defaults[] = {
        PR_LOSS_DEFAULT_TIER_WEIGHT_Z0,
        PR_LOSS_DEFAULT_TIER_WEIGHT_Z1,
        PR_LOSS_DEFAULT_TIER_WEIGHT_Z2,
        PR_LOSS_DEFAULT_TIER_WEIGHT_Z3
    };
    return defaults[tier];
}

int pr_loss_set_tier_weight(
    pr_loss_bridge_t bridge,
    pr_memory_tier_t tier,
    float weight)
{
    if (!bridge || tier >= PR_LOSS_NUM_TIERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_loss_set_tier_weight: bridge is NULL");
        return -1;
    }
    if (weight < 0.0f || weight > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_loss_set_tier_weight: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_set_tier_wei", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.tier_weights[tier] = weight;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Combined Loss Functions
//=============================================================================

int pr_loss_combined(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* predictions,
    const nimcp_quaternion_t* targets,
    const pr_memory_node_t* const* nodes,
    entangle_graph_t graph,
    size_t count,
    pr_loss_result_t* result)
{
    if (!bridge || !predictions || !targets || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_combined: required parameter is NULL (bridge, predictions, targets, result)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_combined", 0.0f);


    if (count == 0) {
        pr_loss_result_init(result);
        return 0;
    }

    uint64_t start_time = get_time_ns();
    pr_loss_result_init(result);
    result->sample_count = (uint32_t)count;

    nimcp_mutex_lock(bridge->base.mutex);
    pr_loss_config_t config = bridge->config;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* 1. Geodesic loss */
    if (config.geodesic_weight > 0.0f) {
        float geo_loss = pr_loss_geodesic_batch(
            bridge, predictions, targets, count,
            NIMCP_LOSS_REDUCE_MEAN, NULL);
        result->geodesic_loss = config.geodesic_weight * geo_loss;
    }

    /* 2. Tier-weighted loss (if nodes provided) */
    if (nodes && config.tier_weights[0] > 0.0f) {
        /* Extract scalar predictions for tier weighting */
        /* Use first component (w) as representative */
        if (ensure_cache_capacity(bridge, count * 2) == 0) {
            float* pred_scalars = bridge->loss_cache;
            float* tgt_scalars = bridge->loss_cache + count;

            for (size_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                                     (float)(i + 1) / (float)count);
                }

                pred_scalars[i] = predictions[i].w;
                tgt_scalars[i] = targets[i].w;
            }

            float tier_loss = pr_loss_tier_aware(
                bridge, pred_scalars, tgt_scalars, nodes, count);
            result->tier_weighted_loss = tier_loss;
        }

        /* 3. Consolidation-weighted component */
        if (config.consolidation_weight > 0.0f) {
            float* pred_scalars = bridge->loss_cache;
            float* tgt_scalars = bridge->loss_cache + count;

            for (size_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                                     (float)(i + 1) / (float)count);
                }

                pred_scalars[i] = predictions[i].w;
                tgt_scalars[i] = targets[i].w;
            }

            float consol_loss = pr_loss_consolidation_weighted(
                bridge, pred_scalars, tgt_scalars, nodes, count);
            result->consolidation_loss = config.consolidation_weight * consol_loss;
        }
    }

    /* 4. Entanglement regularization */
    if (graph && config.entanglement_lambda > 0.0f) {
        result->entanglement_reg = pr_loss_entanglement_reg(bridge, graph);
    }

    /* Compute total loss */
    result->total_loss = result->geodesic_loss +
                         result->triplet_loss +
                         result->consolidation_loss +
                         result->entanglement_reg +
                         result->tier_weighted_loss;

    result->compute_time_ns = get_time_ns() - start_time;

    /* Update statistics */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.combined_computations++;

    /* Update running average */
    float n = (float)bridge->stats.combined_computations;
    bridge->stats.avg_combined_loss =
        (bridge->stats.avg_combined_loss * (n - 1.0f) + result->total_loss) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    if (result->total_loss < bridge->stats.min_combined_loss) {
        bridge->stats.min_combined_loss = result->total_loss;
    }
    if (result->total_loss > bridge->stats.max_combined_loss) {
        bridge->stats.max_combined_loss = result->total_loss;
    }

    bridge->stats.total_compute_time_ns += result->compute_time_ns;
    bridge->stats.avg_compute_time_us =
        (float)bridge->stats.total_compute_time_ns / (1000.0f * n);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_loss_combined_with_triplets(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* predictions,
    const nimcp_quaternion_t* targets,
    const pr_memory_node_t* const* nodes,
    entangle_graph_t graph,
    const size_t* triplet_anchors,
    const size_t* triplet_positives,
    const size_t* triplet_negatives,
    size_t sample_count,
    size_t triplet_count,
    pr_loss_result_t* result)
{
    /* First compute base combined loss */
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_combined_wit", 0.0f);


    int ret = pr_loss_combined(bridge, predictions, targets, nodes, graph,
                               sample_count, result);
    if (ret != 0) return ret;

    /* Add triplet loss if provided */
    if (triplet_anchors && triplet_positives && triplet_negatives && triplet_count > 0) {
        float triplet_sum = 0.0f;

        for (size_t i = 0; i < triplet_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && triplet_count > 256) {
                pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                                 (float)(i + 1) / (float)triplet_count);
            }

            size_t ai = triplet_anchors[i];
            size_t pi = triplet_positives[i];
            size_t ni = triplet_negatives[i];

            if (ai < sample_count && pi < sample_count && ni < sample_count) {
                float loss = pr_loss_resonance_triplet_quat(
                    bridge,
                    predictions[ai],
                    predictions[pi],
                    predictions[ni]);
                triplet_sum += loss;
            }
        }

        float triplet_weight = PR_LOSS_DEFAULT_TRIPLET_WEIGHT;
        if (bridge) {
            nimcp_mutex_lock(bridge->base.mutex);
            triplet_weight = bridge->config.triplet_weight;
            nimcp_mutex_unlock(bridge->base.mutex);
        }

        result->triplet_loss = triplet_weight * triplet_sum / (float)triplet_count;
        result->total_loss += result->triplet_loss;
    }

    return 0;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

int pr_loss_get_stats(
    pr_loss_bridge_t bridge,
    pr_loss_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_loss_reset_stats(pr_loss_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_loss_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(pr_loss_stats_t));
    bridge->stats.min_combined_loss = FLT_MAX;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

void pr_loss_print_stats(pr_loss_bridge_t bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_print_stats", 0.0f);


    pr_loss_stats_t stats;
    if (pr_loss_get_stats(bridge, &stats) != 0) return;

    printf("=== Prime Resonant Loss Bridge Statistics ===\n");
    printf("\nComputation Counts:\n");
    printf("  Geodesic:      %lu\n", (unsigned long)stats.geodesic_computations);
    printf("  Triplet:       %lu (active: %lu, inactive: %lu)\n",
           (unsigned long)stats.triplet_computations,
           (unsigned long)stats.active_triplets,
           (unsigned long)stats.inactive_triplets);
    printf("  Consolidation: %lu\n", (unsigned long)stats.consolidation_computations);
    printf("  Entanglement:  %lu\n", (unsigned long)stats.entanglement_computations);
    printf("  Combined:      %lu\n", (unsigned long)stats.combined_computations);

    printf("\nLoss Values:\n");
    printf("  Avg Combined:  %.6f\n", stats.avg_combined_loss);
    printf("  Min Combined:  %.6f\n", stats.min_combined_loss == FLT_MAX ? 0.0f : stats.min_combined_loss);
    printf("  Max Combined:  %.6f\n", stats.max_combined_loss);

    printf("\nGradient Statistics:\n");
    printf("  Computations:  %lu\n", (unsigned long)stats.gradient_computations);
    printf("  Avg Norm:      %.6f\n", stats.avg_gradient_norm);
    printf("  Max Norm:      %.6f\n", stats.max_gradient_norm);
    printf("  Clips:         %lu\n", (unsigned long)stats.gradient_clips);

    printf("\nTier Statistics:\n");
    for (int t = 0; t < PR_LOSS_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_LOSS_NUM_TIERS > 256) {
            pr_loss_bridge_heartbeat("pr_loss_brid_loop",
                             (float)(t + 1) / (float)PR_LOSS_NUM_TIERS);
        }

        printf("  Z%d: %lu samples, avg loss %.6f\n",
               t, (unsigned long)stats.samples_per_tier[t], stats.loss_per_tier[t]);
    }

    printf("\nTiming:\n");
    printf("  Total Time:    %.3f ms\n", stats.total_compute_time_ns / 1e6);
    printf("  Avg Time:      %.3f us/call\n", stats.avg_compute_time_us);
    printf("================================================\n");
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_loss_type_name(pr_loss_type_t type) {
    static const char* names[] = {
        "Geodesic",
        "Resonance-Triplet",
        "Consolidation",
        "Entanglement-Reg",
        "Tier-Weighted",
        "Combined",
        "Unknown"
    };

    if (type >= PR_LOSS_TYPE_COUNT) return names[PR_LOSS_TYPE_COUNT];
    return names[type];
}

void pr_loss_result_print(const pr_loss_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_result_print", 0.0f);


    printf("Loss Result (n=%u, %.3fus):\n", result->sample_count,
           result->compute_time_ns / 1000.0f);
    printf("  Total:        %.6f\n", result->total_loss);
    printf("  Geodesic:     %.6f\n", result->geodesic_loss);
    printf("  Triplet:      %.6f\n", result->triplet_loss);
    printf("  Consolidation:%.6f\n", result->consolidation_loss);
    printf("  Entanglement: %.6f\n", result->entanglement_reg);
    printf("  Tier-Weight:  %.6f\n", result->tier_weighted_loss);
}

nimcp_quaternion_t pr_loss_apply_gradient(
    nimcp_quaternion_t q,
    const pr_loss_quat_gradient_t* grad,
    float learning_rate)
{
    if (!grad) return q;

    /* Gradient descent step */
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_apply_gradie", 0.0f);


    nimcp_quaternion_t updated;
    updated.w = q.w - learning_rate * grad->dw;
    updated.x = q.x - learning_rate * grad->dx;
    updated.y = q.y - learning_rate * grad->dy;
    updated.z = q.z - learning_rate * grad->dz;

    /* Project back onto unit sphere */
    return quat_normalize_inline(updated);
}

float pr_loss_clip_gradient(
    pr_loss_quat_gradient_t* grad,
    float max_norm)
{
    if (!grad || max_norm <= 0.0f) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_clip_gradien", 0.0f);


    float norm = pr_loss_gradient_norm(grad);

    if (norm > max_norm) {
        float scale = max_norm / (fabsf(norm) > 1e-7f ? norm : 1e-7f);
        grad->dw *= scale;
        grad->dx *= scale;
        grad->dy *= scale;
        grad->dz *= scale;
    }

    return norm;
}

float pr_loss_gradient_norm(const pr_loss_quat_gradient_t* grad) {
    if (!grad) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_gradient_nor", 0.0f);


    return sqrtf(grad->dw * grad->dw +
                 grad->dx * grad->dx +
                 grad->dy * grad->dy +
                 grad->dz * grad->dz);
}

void pr_loss_result_init(pr_loss_result_t* result) {
    if (!result) return;
    /* Phase 8: Heartbeat at operation start */
    pr_loss_bridge_heartbeat("pr_loss_brid_pr_loss_result_init", 0.0f);


    memset(result, 0, sizeof(pr_loss_result_t));
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_loss_bridge_set_instance_health_agent(
    pr_loss_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_loss_bridge_training_begin(pr_loss_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_loss_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_loss_bridge_heartbeat_instance(bridge->health_agent, "pr_loss_bridge_training_begin", 0.0f);
    return 0;
}

int pr_loss_bridge_training_end(pr_loss_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_loss_bridge_training_end: NULL argument");
        return -1;
    }
    pr_loss_bridge_heartbeat_instance(bridge->health_agent, "pr_loss_bridge_training_end", 1.0f);
    return 0;
}

int pr_loss_bridge_training_step(pr_loss_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_loss_bridge_training_step: NULL argument");
        return -1;
    }
    pr_loss_bridge_heartbeat_instance(bridge->health_agent, "pr_loss_bridge_training_step", progress);
    return 0;
}
