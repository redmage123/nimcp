/**
 * @file nimcp_vae_fep_bridge.c
 * @brief VAE-FEP Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements bidirectional integration between VAE latent space and FEP beliefs.
 *
 * BIO_MODULE: 0x1F10
 */

#include "cognitive/vae/bridges/nimcp_vae_fep_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
/* TODO: Fix immune path #include "immune/nimcp_immune.h" */
#include "utils/fault_tolerance/nimcp_health_agent.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_FEP_MODULE_ID           BIO_MODULE_VAE_FEP_BRIDGE
#define VAE_FEP_EMA_ALPHA           0.95f
#define VAE_FEP_HEALTH_DECAY        0.01f
#define VAE_FEP_MIN_HEALTH          0.1f
#define VAE_FEP_MAX_SYNC_FAILURES   5

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Update health metrics
 */
static void update_health(vae_fep_bridge_t* bridge, bool success)
{
    if (!bridge) return;

    if (success) {
        bridge->health.consecutive_failures = 0;
        bridge->health.bridge_health = fminf(1.0f, bridge->health.bridge_health + 0.02f);
        bridge->health.sync_reliability = fminf(1.0f, bridge->health.sync_reliability + 0.01f);
    } else {
        bridge->health.consecutive_failures++;
        bridge->health.bridge_health = fmaxf(VAE_FEP_MIN_HEALTH,
                                              bridge->health.bridge_health - 0.1f);
        bridge->health.sync_reliability = fmaxf(VAE_FEP_MIN_HEALTH,
                                                 bridge->health.sync_reliability - 0.05f);
    }

    bridge->health.is_healthy = (bridge->health.bridge_health > 0.5f) &&
                                 (bridge->health.consecutive_failures < VAE_FEP_MAX_SYNC_FAILURES);
}

/**
 * @brief Update EMA statistics
 */
static void update_ema_stats(vae_fep_bridge_t* bridge, float free_energy,
                             float inaccuracy, float complexity)
{
    if (!bridge) return;

    float alpha = VAE_FEP_EMA_ALPHA;

    if (bridge->stats.total_syncs == 0) {
        bridge->stats.avg_free_energy = free_energy;
        bridge->stats.avg_inaccuracy = inaccuracy;
        bridge->stats.avg_complexity = complexity;
        bridge->stats.min_free_energy = free_energy;
        bridge->stats.max_free_energy = free_energy;
    } else {
        if (isfinite(free_energy)) {
            bridge->stats.avg_free_energy = alpha * bridge->stats.avg_free_energy +
                                            (1.0f - alpha) * free_energy;
        }
        if (isfinite(inaccuracy)) {
            bridge->stats.avg_inaccuracy = alpha * bridge->stats.avg_inaccuracy +
                                           (1.0f - alpha) * inaccuracy;
        }
        if (isfinite(complexity)) {
            bridge->stats.avg_complexity = alpha * bridge->stats.avg_complexity +
                                           (1.0f - alpha) * complexity;
        }

        if (free_energy < bridge->stats.min_free_energy) {
            bridge->stats.min_free_energy = free_energy;
        }
        if (free_energy > bridge->stats.max_free_energy) {
            bridge->stats.max_free_energy = free_energy;
        }
    }
}

/* Forward declaration for cleanup on init failure */
static void free_mapping_state(vae_fep_mapping_state_t* mapping);

/**
 * @brief Initialize mapping state
 */
static int init_mapping_state(vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_mapping_state: bridge is NULL");
        return -1;
    }

    vae_fep_mapping_state_t* mapping = &bridge->mapping;

    /* Get dimensions from connected systems */
    mapping->latent_dim = 0;
    mapping->belief_dim = 0;

    if (bridge->vae) {
        mapping->latent_dim = vae_get_latent_dim(bridge->vae);
    }

    if (bridge->fep && bridge->fep->num_levels > 0) {
        uint32_t target_level = bridge->config.target_hierarchy_level;
        if (target_level < bridge->fep->num_levels) {
            mapping->belief_dim = bridge->fep->levels[target_level].beliefs.dim;
        }
    }

    /* Allocate mapping buffers if needed */
    uint32_t map_dim = (mapping->latent_dim > 0) ? mapping->latent_dim : 32;

    if (!mapping->mapped_mean) {
        mapping->mapped_mean = nimcp_calloc(map_dim, sizeof(float));
        if (!mapping->mapped_mean) goto alloc_fail;
    }
    if (!mapping->mapped_precision) {
        mapping->mapped_precision = nimcp_calloc(map_dim, sizeof(float));
        if (!mapping->mapped_precision) goto alloc_fail;
        /* Initialize precision to 1.0 */
        for (uint32_t i = 0; i < map_dim; i++) {
            mapping->mapped_precision[i] = 1.0f;
        }
    }

    /* For linear mapping mode, allocate transformation matrices */
    if (bridge->config.mapping_mode == VAE_FEP_MAP_LINEAR) {
        if (mapping->latent_dim > 0 && mapping->belief_dim > 0) {
            if (!mapping->latent_to_belief_matrix) {
                mapping->latent_to_belief_matrix = nimcp_calloc(
                    mapping->belief_dim * mapping->latent_dim, sizeof(float));
                if (!mapping->latent_to_belief_matrix) goto alloc_fail;
                /* Initialize to identity-like mapping */
                uint32_t min_dim = (mapping->latent_dim < mapping->belief_dim) ?
                                    mapping->latent_dim : mapping->belief_dim;
                for (uint32_t i = 0; i < min_dim; i++) {
                    mapping->latent_to_belief_matrix[i * mapping->latent_dim + i] = 1.0f;
                }
            }
            if (!mapping->belief_to_latent_matrix) {
                mapping->belief_to_latent_matrix = nimcp_calloc(
                    mapping->latent_dim * mapping->belief_dim, sizeof(float));
                if (!mapping->belief_to_latent_matrix) goto alloc_fail;
                uint32_t min_dim = (mapping->latent_dim < mapping->belief_dim) ?
                                    mapping->latent_dim : mapping->belief_dim;
                for (uint32_t i = 0; i < min_dim; i++) {
                    mapping->belief_to_latent_matrix[i * mapping->belief_dim + i] = 1.0f;
                }
            }
        }
    }

    mapping->shared_free_energy = 0.0f;
    mapping->vae_free_energy = 0.0f;
    mapping->fep_free_energy = 0.0f;
    mapping->inaccuracy = 0.0f;
    mapping->complexity = 0.0f;

    return 0;

alloc_fail:
    free_mapping_state(mapping);
    return -1;
}

/**
 * @brief Free mapping state resources
 */
static void free_mapping_state(vae_fep_mapping_state_t* mapping)
{
    if (!mapping) return;

    if (mapping->latent_to_belief_matrix) {
        nimcp_free(mapping->latent_to_belief_matrix);
        mapping->latent_to_belief_matrix = NULL;
    }
    if (mapping->belief_to_latent_matrix) {
        nimcp_free(mapping->belief_to_latent_matrix);
        mapping->belief_to_latent_matrix = NULL;
    }
    if (mapping->mapped_mean) {
        nimcp_free(mapping->mapped_mean);
        mapping->mapped_mean = NULL;
    }
    if (mapping->mapped_precision) {
        nimcp_free(mapping->mapped_precision);
        mapping->mapped_precision = NULL;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_fep_bridge_default_config(vae_fep_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "NULL config in vae_fep_bridge_default_config");
        return -1;
    }

    memset(config, 0, sizeof(vae_fep_bridge_config_t));

    config->sync_direction = VAE_FEP_SYNC_BIDIRECTIONAL;
    config->sync_interval_ms = VAE_FEP_DEFAULT_SYNC_INTERVAL;
    config->auto_sync_enabled = false;

    config->mapping_mode = VAE_FEP_MAP_DIRECT;
    config->target_hierarchy_level = 0;

    config->share_precision = true;
    config->precision_scale = 1.0f;
    config->min_precision = VAE_FEP_MIN_PRECISION;
    config->max_precision = VAE_FEP_MAX_PRECISION;

    config->share_free_energy = true;
    config->free_energy_weight_vae = 0.5f;
    config->free_energy_weight_fep = 0.5f;

    config->enable_bio_async = false;
    config->enable_immune_reporting = true;
    config->enable_logging = false;

    config->message_buffer_size = VAE_FEP_DEFAULT_MSG_BUFFER;

    return 0;
}

vae_fep_bridge_t* vae_fep_bridge_create(const vae_fep_bridge_config_t* config)
{
    vae_fep_bridge_config_t default_config;

    if (!config) {
        if (vae_fep_bridge_default_config(&default_config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_create: validation failed");
            return NULL;
        }
        config = &default_config;
    }

    vae_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate VAE-FEP bridge");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_FEP_STATE_DISCONNECTED;
    bridge->vae = NULL;
    bridge->fep = NULL;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        NIMCP_LOG_ERROR("VAE-FEP Bridge: Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_fep_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    /* Initialize health */
    bridge->health.bridge_health = 1.0f;
    bridge->health.sync_reliability = 1.0f;
    bridge->health.latency_health = 1.0f;
    bridge->health.is_healthy = true;
    bridge->health.consecutive_failures = 0;
    bridge->health.last_error_code = 0;

    /* Initialize timing */
    bridge->creation_time_us = get_timestamp_us();
    bridge->last_sync_time_us = 0;

    bridge->is_initialized = true;

    NIMCP_LOG_INFO("VAE-FEP Bridge: Created (mapping_mode=%d, share_precision=%d)",
                   config->mapping_mode, config->share_precision);

    return bridge;
}

void vae_fep_bridge_destroy(vae_fep_bridge_t* bridge)
{
    if (!bridge) return;

    NIMCP_LOG_INFO("VAE-FEP Bridge: Destroying");

    /* Disconnect systems */
    vae_fep_bridge_disconnect(bridge);

    /* Free mapping state */
    free_mapping_state(&bridge->mapping);

    /* Free mutex */
    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_fep_bridge_reset(vae_fep_bridge_t* bridge)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "Invalid bridge in reset");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(vae_fep_bridge_stats_t));

    /* Reset health */
    bridge->health.bridge_health = 1.0f;
    bridge->health.sync_reliability = 1.0f;
    bridge->health.latency_health = 1.0f;
    bridge->health.is_healthy = true;
    bridge->health.consecutive_failures = 0;

    /* Reset mapping state */
    bridge->mapping.shared_free_energy = 0.0f;
    bridge->mapping.vae_free_energy = 0.0f;
    bridge->mapping.fep_free_energy = 0.0f;
    bridge->mapping.inaccuracy = 0.0f;
    bridge->mapping.complexity = 0.0f;

    bridge->last_sync_time_us = 0;

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_DEBUG("VAE-FEP Bridge: Reset");

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int vae_fep_bridge_connect_vae(vae_fep_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "Invalid bridge in connect_vae");
        return -1;
    }

    if (!vae) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NO_VAE,
                             "NULL VAE system in connect_vae");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->vae = vae;
    bridge->mapping.latent_dim = vae_get_latent_dim(vae);

    /* Re-initialize mapping if both systems connected */
    if (bridge->fep) {
        init_mapping_state(bridge);
        bridge->state = VAE_FEP_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-FEP Bridge: VAE connected (latent_dim=%u)",
                   bridge->mapping.latent_dim);

    return 0;
}

int vae_fep_bridge_connect_fep(vae_fep_bridge_t* bridge, fep_system_t* fep)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "Invalid bridge in connect_fep");
        return -1;
    }

    if (!fep) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NO_FEP,
                             "NULL FEP system in connect_fep");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->fep = fep;

    /* Get belief dimension from target level */
    if (fep->num_levels > 0) {
        uint32_t target_level = bridge->config.target_hierarchy_level;
        if (target_level >= fep->num_levels) {
            target_level = 0;
        }
        bridge->mapping.belief_dim = fep->levels[target_level].beliefs.dim;
    }

    /* Re-initialize mapping if both systems connected */
    if (bridge->vae) {
        init_mapping_state(bridge);
        bridge->state = VAE_FEP_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-FEP Bridge: FEP connected (belief_dim=%u)",
                   bridge->mapping.belief_dim);

    return 0;
}

int vae_fep_bridge_disconnect(vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->vae = NULL;
    bridge->fep = NULL;
    bridge->state = VAE_FEP_STATE_DISCONNECTED;

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-FEP Bridge: Disconnected");

    return 0;
}

bool vae_fep_bridge_is_connected(const vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return (bridge->vae != NULL) && (bridge->fep != NULL);
}

vae_fep_bridge_state_t vae_fep_bridge_get_state(const vae_fep_bridge_t* bridge)
{
    if (!bridge) return VAE_FEP_STATE_DISCONNECTED;
    return bridge->state;
}

/* ============================================================================
 * Synchronization API
 * ============================================================================ */

int vae_fep_sync_latent_to_belief(vae_fep_bridge_t* bridge)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "Invalid bridge in sync_latent_to_belief");
        return -1;
    }

    if (!bridge->vae || !bridge->fep) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NOT_CONNECTED,
                             "Bridge not fully connected for sync");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->state = VAE_FEP_STATE_SYNCING;

    uint64_t start_time = get_timestamp_us();
    int result = 0;

    /* Get VAE latent state */
    vae_latent_state_t vae_latent;
    uint32_t latent_dim = bridge->mapping.latent_dim;

    if (latent_dim == 0) {
        latent_dim = vae_get_latent_dim(bridge->vae);
        bridge->mapping.latent_dim = latent_dim;
    }

    /* Allocate temporary latent state */
    vae_latent.mu = nimcp_calloc(latent_dim, sizeof(float));
    vae_latent.log_var = nimcp_calloc(latent_dim, sizeof(float));
    vae_latent.z = nimcp_calloc(latent_dim, sizeof(float));
    vae_latent.precision = nimcp_calloc(latent_dim, sizeof(float));
    vae_latent.latent_dim = latent_dim;

    if (!vae_latent.mu || !vae_latent.log_var || !vae_latent.z || !vae_latent.precision) {
        result = -1;
        goto cleanup;
    }

    /* Get current latent state from VAE */
    if (vae_get_latent_state(bridge->vae, &vae_latent) != 0) {
        NIMCP_LOG_WARN("VAE-FEP Bridge: Failed to get VAE latent state");
        result = -1;
        goto cleanup;
    }

    /* Get target FEP level */
    uint32_t target_level = bridge->config.target_hierarchy_level;
    if (target_level >= bridge->fep->num_levels) {
        target_level = 0;
    }

    fep_hierarchy_level_t* fep_level = &bridge->fep->levels[target_level];
    uint32_t belief_dim = fep_level->beliefs.dim;

    /* Transfer based on mapping mode */
    if (bridge->config.mapping_mode == VAE_FEP_MAP_DIRECT) {
        /* Direct 1:1 mapping */
        uint32_t copy_dim = (latent_dim < belief_dim) ? latent_dim : belief_dim;

        if (fep_level->beliefs.mean) {
            memcpy(fep_level->beliefs.mean, vae_latent.mu, copy_dim * sizeof(float));
        }

        if (bridge->config.share_precision && fep_level->beliefs.precision) {
            /* Convert log_var to precision */
            for (uint32_t i = 0; i < copy_dim; i++) {
                float var = expf(vae_latent.log_var[i]);
                float prec = 1.0f / fmaxf(var, 1e-6f);
                prec = nimcp_clampf(prec * bridge->config.precision_scale,
                             bridge->config.min_precision,
                             bridge->config.max_precision);
                fep_level->beliefs.precision[i] = prec;
            }
        }
    } else if (bridge->config.mapping_mode == VAE_FEP_MAP_LINEAR) {
        /* Linear transformation: belief = W * latent */
        if (bridge->mapping.latent_to_belief_matrix && fep_level->beliefs.mean) {
            for (uint32_t i = 0; i < belief_dim; i++) {
                float sum = 0.0f;
                for (uint32_t j = 0; j < latent_dim; j++) {
                    sum += bridge->mapping.latent_to_belief_matrix[i * latent_dim + j] *
                           vae_latent.mu[j];
                }
                fep_level->beliefs.mean[i] = sum;
            }
        }
    }

    /* Store mapped values */
    if (bridge->mapping.mapped_mean) {
        uint32_t copy_dim = (latent_dim < bridge->mapping.latent_dim) ?
                            latent_dim : bridge->mapping.latent_dim;
        memcpy(bridge->mapping.mapped_mean, vae_latent.mu, copy_dim * sizeof(float));
    }

    /* Update statistics */
    bridge->stats.latent_to_belief_syncs++;
    bridge->stats.total_syncs++;
    bridge->mapping.last_vae_to_fep_us = get_timestamp_us();

    uint64_t elapsed = get_timestamp_us() - start_time;
    float alpha = VAE_FEP_EMA_ALPHA;
    float elapsed_f = (float)elapsed;
    if (isfinite(elapsed_f)) {
        bridge->stats.avg_sync_latency_us = alpha * bridge->stats.avg_sync_latency_us +
                                            (1.0f - alpha) * elapsed_f;
    }

    update_health(bridge, true);

cleanup:
    if (vae_latent.mu) nimcp_free(vae_latent.mu);
    if (vae_latent.log_var) nimcp_free(vae_latent.log_var);
    if (vae_latent.z) nimcp_free(vae_latent.z);
    if (vae_latent.precision) nimcp_free(vae_latent.precision);

    bridge->state = (result == 0) ? VAE_FEP_STATE_CONNECTED : VAE_FEP_STATE_ERROR;
    bridge->last_sync_time_us = get_timestamp_us();

    nimcp_mutex_unlock(bridge->mutex);

    if (result != 0) {
        bridge->stats.sync_failures++;
        update_health(bridge, false);
    }

    return result;
}

int vae_fep_sync_belief_to_latent(vae_fep_bridge_t* bridge)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "Invalid bridge in sync_belief_to_latent");
        return -1;
    }

    if (!bridge->vae || !bridge->fep) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NOT_CONNECTED,
                             "Bridge not fully connected for sync");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->state = VAE_FEP_STATE_SYNCING;

    uint64_t start_time = get_timestamp_us();
    int result = 0;

    /* Get target FEP level */
    uint32_t target_level = bridge->config.target_hierarchy_level;
    if (target_level >= bridge->fep->num_levels) {
        target_level = 0;
    }

    fep_hierarchy_level_t* fep_level = &bridge->fep->levels[target_level];
    uint32_t belief_dim = fep_level->beliefs.dim;
    uint32_t latent_dim = bridge->mapping.latent_dim;

    /* For now, we store the belief values in the mapping state
     * The VAE itself may use these for guided generation or training
     */
    if (bridge->config.mapping_mode == VAE_FEP_MAP_DIRECT) {
        uint32_t copy_dim = (belief_dim < latent_dim) ? belief_dim : latent_dim;

        if (bridge->mapping.mapped_mean && fep_level->beliefs.mean) {
            memcpy(bridge->mapping.mapped_mean, fep_level->beliefs.mean,
                   copy_dim * sizeof(float));
        }

        if (bridge->config.share_precision && bridge->mapping.mapped_precision &&
            fep_level->beliefs.precision) {
            memcpy(bridge->mapping.mapped_precision, fep_level->beliefs.precision,
                   copy_dim * sizeof(float));
        }
    } else if (bridge->config.mapping_mode == VAE_FEP_MAP_LINEAR) {
        /* Linear transformation: latent = W' * belief */
        if (bridge->mapping.belief_to_latent_matrix && bridge->mapping.mapped_mean) {
            for (uint32_t i = 0; i < latent_dim; i++) {
                float sum = 0.0f;
                for (uint32_t j = 0; j < belief_dim; j++) {
                    sum += bridge->mapping.belief_to_latent_matrix[i * belief_dim + j] *
                           fep_level->beliefs.mean[j];
                }
                bridge->mapping.mapped_mean[i] = sum;
            }
        }
    }

    /* Update statistics */
    bridge->stats.belief_to_latent_syncs++;
    bridge->stats.belief_updates_received++;
    bridge->stats.total_syncs++;
    bridge->mapping.last_fep_to_vae_us = get_timestamp_us();

    uint64_t elapsed = get_timestamp_us() - start_time;
    float alpha = VAE_FEP_EMA_ALPHA;
    float elapsed_f2 = (float)elapsed;
    if (isfinite(elapsed_f2)) {
        bridge->stats.avg_sync_latency_us = alpha * bridge->stats.avg_sync_latency_us +
                                            (1.0f - alpha) * elapsed_f2;
    }

    update_health(bridge, true);

    bridge->state = VAE_FEP_STATE_CONNECTED;
    bridge->last_sync_time_us = get_timestamp_us();

    nimcp_mutex_unlock(bridge->mutex);

    return result;
}

int vae_fep_bridge_sync(vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_sync: bridge is NULL");
        return -1;
    }

    int result = 0;

    /* Sync VAE to FEP */
    result = vae_fep_sync_latent_to_belief(bridge);
    if (result != 0) return result;

    /* Sync FEP to VAE */
    result = vae_fep_sync_belief_to_latent(bridge);
    if (result != 0) return result;

    /* Sync free energy */
    if (bridge->config.share_free_energy) {
        float fe = 0.0f;
        vae_fep_compute_free_energy(bridge, &fe);
    }

    bridge->stats.bidirectional_syncs++;

    return 0;
}

int vae_fep_bridge_sync_direction(vae_fep_bridge_t* bridge,
                                   vae_fep_sync_direction_t direction)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_sync_direction: bridge is NULL");
        return -1;
    }

    switch (direction) {
        case VAE_FEP_SYNC_VAE_TO_FEP:
            return vae_fep_sync_latent_to_belief(bridge);
        case VAE_FEP_SYNC_FEP_TO_VAE:
            return vae_fep_sync_belief_to_latent(bridge);
        case VAE_FEP_SYNC_BIDIRECTIONAL:
            return vae_fep_bridge_sync(bridge);
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_fep_bridge_sync_direction: operation failed");
            return -1;
    }
}

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

int vae_fep_compute_free_energy(vae_fep_bridge_t* bridge, float* free_energy)
{
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NULL_BRIDGE,
                             "Invalid arguments in compute_free_energy");
        return -1;
    }

    if (!vae_fep_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_FEP_MODULE_ID, NIMCP_ERROR_VAE_FEP_NOT_CONNECTED,
                             "Bridge not connected for free energy");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Get VAE free energy */
    float vae_fe = vae_get_free_energy(bridge->vae);
    if (isnan(vae_fe)) vae_fe = 0.0f;

    /* Get FEP free energy */
    float fep_fe = bridge->fep->free_energy.total;
    if (isnan(fep_fe)) fep_fe = 0.0f;

    /* Combine with weights */
    float combined = bridge->config.free_energy_weight_vae * vae_fe +
                     bridge->config.free_energy_weight_fep * fep_fe;

    /* Store values */
    bridge->mapping.vae_free_energy = vae_fe;
    bridge->mapping.fep_free_energy = fep_fe;
    bridge->mapping.shared_free_energy = combined;
    bridge->mapping.last_free_energy_us = get_timestamp_us();

    /* Extract inaccuracy and complexity from FEP */
    bridge->mapping.inaccuracy = bridge->fep->free_energy.inaccuracy;
    bridge->mapping.complexity = bridge->fep->free_energy.complexity;

    /* Update statistics */
    update_ema_stats(bridge, combined, bridge->mapping.inaccuracy,
                     bridge->mapping.complexity);

    *free_energy = combined;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_fep_get_free_energy_decomposition(vae_fep_bridge_t* bridge,
                                           float* total,
                                           float* inaccuracy,
                                           float* complexity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_get_free_energy_decomposition: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    if (total) *total = bridge->mapping.shared_free_energy;
    if (inaccuracy) *inaccuracy = bridge->mapping.inaccuracy;
    if (complexity) *complexity = bridge->mapping.complexity;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_fep_update_free_energy_from_vae(vae_fep_bridge_t* bridge,
                                         const vae_loss_t* vae_loss)
{
    if (!bridge || !vae_loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_update_free_energy_from_vae: required parameter is NULL (bridge, vae_loss)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Update VAE contribution */
    bridge->mapping.vae_free_energy = vae_loss->free_energy;
    bridge->mapping.inaccuracy = vae_loss->inaccuracy;
    bridge->mapping.complexity = vae_loss->complexity;

    /* Recompute combined free energy */
    float fep_fe = bridge->fep ? bridge->fep->free_energy.total : 0.0f;
    bridge->mapping.shared_free_energy =
        bridge->config.free_energy_weight_vae * vae_loss->free_energy +
        bridge->config.free_energy_weight_fep * fep_fe;

    bridge->mapping.last_free_energy_us = get_timestamp_us();

    update_ema_stats(bridge, bridge->mapping.shared_free_energy,
                     bridge->mapping.inaccuracy, bridge->mapping.complexity);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Precision API
 * ============================================================================ */

int vae_fep_get_precision(vae_fep_bridge_t* bridge, float* precision, uint32_t dim)
{
    if (!bridge || !precision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_get_precision: required parameter is NULL (bridge, precision)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    uint32_t copy_dim = (dim < bridge->mapping.latent_dim) ?
                        dim : bridge->mapping.latent_dim;

    if (bridge->mapping.mapped_precision) {
        memcpy(precision, bridge->mapping.mapped_precision, copy_dim * sizeof(float));
    } else {
        /* Default to 1.0 */
        for (uint32_t i = 0; i < copy_dim; i++) {
            precision[i] = 1.0f;
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_fep_sync_precision(vae_fep_bridge_t* bridge)
{
    if (!bridge || !vae_fep_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_sync_precision: required parameter is NULL (bridge, vae_fep_bridge_is_connected)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    uint32_t latent_dim = bridge->mapping.latent_dim;
    float* precision = nimcp_calloc(latent_dim, sizeof(float));

    if (!precision) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_fep_sync_precision: precision is NULL");
        return -1;
    }

    /* Get precision from VAE */
    if (vae_get_precision(bridge->vae, precision, latent_dim) == 0) {
        /* Apply to FEP beliefs */
        uint32_t target_level = bridge->config.target_hierarchy_level;
        if (target_level < bridge->fep->num_levels) {
            fep_hierarchy_level_t* level = &bridge->fep->levels[target_level];
            uint32_t copy_dim = (latent_dim < level->beliefs.dim) ?
                                latent_dim : level->beliefs.dim;

            for (uint32_t i = 0; i < copy_dim; i++) {
                float p = precision[i] * bridge->config.precision_scale;
                p = nimcp_clampf(p, bridge->config.min_precision, bridge->config.max_precision);
                if (level->beliefs.precision) {
                    level->beliefs.precision[i] = p;
                }
            }
        }

        /* Store in mapping */
        if (bridge->mapping.mapped_precision) {
            memcpy(bridge->mapping.mapped_precision, precision,
                   latent_dim * sizeof(float));
        }

        bridge->stats.precision_updates++;
    }

    nimcp_free(precision);
    precision = NULL;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

float vae_fep_get_avg_precision(const vae_fep_bridge_t* bridge)
{
    if (!bridge || !bridge->mapping.mapped_precision) return NAN;
    if (bridge->mapping.latent_dim == 0) return NAN;

    float sum = 0.0f;
    for (uint32_t i = 0; i < bridge->mapping.latent_dim; i++) {
        sum += bridge->mapping.mapped_precision[i];
    }

    return sum / (float)bridge->mapping.latent_dim;
}

/* ============================================================================
 * Prediction Error API
 * ============================================================================ */

int vae_fep_report_prediction_error(vae_fep_bridge_t* bridge, float error_magnitude)
{
    if (!bridge || !vae_fep_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_report_prediction_error: required parameter is NULL (bridge, vae_fep_bridge_is_connected)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Report to FEP system */
    uint32_t target_level = bridge->config.target_hierarchy_level;
    if (target_level < bridge->fep->num_levels) {
        fep_hierarchy_level_t* level = &bridge->fep->levels[target_level];
        level->errors.magnitude = error_magnitude;
    }

    bridge->stats.prediction_errors_sent++;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_fep_report_prediction_error_tensor(vae_fep_bridge_t* bridge,
                                            const nimcp_tensor_t* error)
{
    if (!bridge || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_report_prediction_error_tensor: required parameter is NULL (bridge, error)");
        return -1;
    }

    /* Compute magnitude */
    uint32_t total = nimcp_tensor_numel(error);
    const float* data = (const float*)error->data;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        sum_sq += data[i] * data[i];
    }

    float magnitude = sqrtf(sum_sq / (float)total);

    return vae_fep_report_prediction_error(bridge, magnitude);
}

int vae_fep_get_fep_prediction_error(vae_fep_bridge_t* bridge,
                                      nimcp_tensor_t* error)
{
    if (!bridge || !error || !vae_fep_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_get_fep_prediction_error: required parameter is NULL (bridge, error, vae_fep_bridge_is_connected)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    uint32_t target_level = bridge->config.target_hierarchy_level;
    if (target_level < bridge->fep->num_levels) {
        fep_hierarchy_level_t* level = &bridge->fep->levels[target_level];

        uint32_t copy_dim = level->errors.dim;
        if (copy_dim > nimcp_tensor_numel(error)) {
            copy_dim = nimcp_tensor_numel(error);
        }

        if (level->errors.error) {
            memcpy(error->data, level->errors.error, copy_dim * sizeof(float));
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Active Inference API
 * ============================================================================ */

int vae_fep_compute_expected_free_energy(vae_fep_bridge_t* bridge,
                                          const nimcp_tensor_t* action_latent,
                                          float* expected_fe)
{
    if (!bridge || !action_latent || !expected_fe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_compute_expected_free_energy: required parameter is NULL (bridge, action_latent, expected_fe)");
        return -1;
    }
    if (!vae_fep_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_fep_compute_expected_free_energy: vae_fep_bridge_is_connected is NULL");
        return -1;
    }

    /* For now, return current free energy as estimate
     * Full implementation would use VAE decoder to predict outcomes */
    *expected_fe = bridge->mapping.shared_free_energy;

    return 0;
}

int vae_fep_sample_action_latent(vae_fep_bridge_t* bridge,
                                  nimcp_tensor_t* action_latent)
{
    if (!bridge || !action_latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_sample_action_latent: required parameter is NULL (bridge, action_latent)");
        return -1;
    }
    if (!vae_fep_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_fep_sample_action_latent: vae_fep_bridge_is_connected is NULL");
        return -1;
    }

    /* Sample from prior as default action
     * Full implementation would use FEP policy selection */
    uint32_t latent_dim = bridge->mapping.latent_dim;
    return vae_latent_sample_prior(1, latent_dim, action_latent);
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int vae_fep_bridge_update(vae_fep_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_update: required parameter is NULL (bridge, bridge->is_initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Auto-sync if enabled */
    if (bridge->config.auto_sync_enabled && vae_fep_bridge_is_connected(bridge)) {
        uint64_t now = get_timestamp_us();
        uint64_t elapsed_ms = (now - bridge->last_sync_time_us) / 1000;

        if (elapsed_ms >= bridge->config.sync_interval_ms) {
            nimcp_mutex_unlock(bridge->mutex);
            vae_fep_bridge_sync_direction(bridge, bridge->config.sync_direction);
            nimcp_mutex_lock(bridge->mutex);
        }
    }

    /* Update statistics timing */
    bridge->stats.last_update_us = get_timestamp_us();
    bridge->stats.uptime_us = get_timestamp_us() - bridge->creation_time_us;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_fep_bridge_process_messages(vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_process_messages: bridge is NULL");
        return -1;
    }

    /* Bio-async message processing placeholder */
    /* Full implementation would process queued messages */

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int vae_fep_bridge_get_stats(const vae_fep_bridge_t* bridge,
                              vae_fep_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int vae_fep_bridge_get_mapping(const vae_fep_bridge_t* bridge,
                                vae_fep_mapping_state_t* mapping)
{
    if (!bridge || !mapping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_get_mapping: required parameter is NULL (bridge, mapping)");
        return -1;
    }

    /* Copy scalar values only, not pointers */
    mapping->latent_dim = bridge->mapping.latent_dim;
    mapping->belief_dim = bridge->mapping.belief_dim;
    mapping->shared_free_energy = bridge->mapping.shared_free_energy;
    mapping->vae_free_energy = bridge->mapping.vae_free_energy;
    mapping->fep_free_energy = bridge->mapping.fep_free_energy;
    mapping->inaccuracy = bridge->mapping.inaccuracy;
    mapping->complexity = bridge->mapping.complexity;
    mapping->last_vae_to_fep_us = bridge->mapping.last_vae_to_fep_us;
    mapping->last_fep_to_vae_us = bridge->mapping.last_fep_to_vae_us;
    mapping->last_free_energy_us = bridge->mapping.last_free_energy_us;

    return 0;
}

int vae_fep_bridge_get_health(const vae_fep_bridge_t* bridge,
                               vae_fep_bridge_health_t* health)
{
    if (!bridge || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_get_health: required parameter is NULL (bridge, health)");
        return -1;
    }
    *health = bridge->health;
    return 0;
}

int vae_fep_bridge_get_config(const vae_fep_bridge_t* bridge,
                               vae_fep_bridge_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

/* ============================================================================
 * Dimension Query
 * ============================================================================ */

uint32_t vae_fep_bridge_get_latent_dim(const vae_fep_bridge_t* bridge)
{
    if (!bridge) return 0;
    return bridge->mapping.latent_dim;
}

uint32_t vae_fep_bridge_get_belief_dim(const vae_fep_bridge_t* bridge)
{
    if (!bridge) return 0;
    return bridge->mapping.belief_dim;
}

bool vae_fep_bridge_dims_compatible(const vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_dims_compatible: bridge is NULL");
        return false;
    }

    if (bridge->config.mapping_mode == VAE_FEP_MAP_DIRECT) {
        /* For direct mapping, dimensions should match */
        return bridge->mapping.latent_dim == bridge->mapping.belief_dim;
    }

    /* For other modes, any dimensions are compatible */
    return (bridge->mapping.latent_dim > 0 && bridge->mapping.belief_dim > 0);
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int vae_fep_bridge_connect_bio_async(vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Bio-async connection placeholder */
    bridge->config.enable_bio_async = true;

    NIMCP_LOG_DEBUG("VAE-FEP Bridge: Bio-async connected");

    return 0;
}

int vae_fep_bridge_disconnect_bio_async(vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    bridge->config.enable_bio_async = false;

    NIMCP_LOG_DEBUG("VAE-FEP Bridge: Bio-async disconnected");

    return 0;
}

bool vae_fep_bridge_is_bio_async_connected(const vae_fep_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->config.enable_bio_async;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

void vae_fep_bridge_set_health_agent(vae_fep_bridge_t* bridge,
                                      nimcp_health_agent_t* agent)
{
    if (!bridge) return;

    /* Health agent integration placeholder */

    NIMCP_LOG_DEBUG("VAE-FEP Bridge: Health agent set");
}
