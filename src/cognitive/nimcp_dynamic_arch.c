/**
 * @file nimcp_dynamic_arch.c
 * @brief Dynamic Neural Architecture Search — utilisation monitoring
 *
 * Tracks per-region neuron activation statistics and produces advisory
 * recommendations (grow / shrink / none) based on configurable thresholds.
 *
 * This module is purely observational — it does NOT modify the brain.
 */

#include "cognitive/nimcp_dynamic_arch.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "dynamic_arch"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

typedef struct {
    char     name[64];
    uint32_t neuron_start;
    uint32_t neuron_count;
    double   activation_sum;    /* sum of |activation| values recorded */
    uint64_t activation_count;  /* number of record_activation calls   */
    float    avg_utilization;   /* computed by analyze()                */
} nimcp_region_stat_t;

struct nimcp_dynamic_arch {
    nimcp_dynamic_arch_config_t config;

    nimcp_region_stat_t  region_stats[NIMCP_DYNARCH_MAX_REGIONS];
    uint32_t             region_count;

    nimcp_arch_recommendation_t recommendations[NIMCP_DYNARCH_MAX_REGIONS];
    uint32_t                    recommendation_count;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static int find_region(const nimcp_dynamic_arch_t* h, const char* name)
{
    if (!name) return -1;
    for (uint32_t i = 0; i < h->region_count; i++) {
        if (strcmp(h->region_stats[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_dynamic_arch_config_t nimcp_dynamic_arch_config_default(void)
{
    nimcp_dynamic_arch_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.monitor_interval    = 1000;
    cfg.utilization_window  = 500;
    cfg.grow_threshold      = 0.9f;
    cfg.shrink_threshold    = 0.1f;
    cfg.max_recommendations = 5;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_dynamic_arch_t* nimcp_dynamic_arch_create(
    const nimcp_dynamic_arch_config_t* config)
{
    nimcp_dynamic_arch_t* h = (nimcp_dynamic_arch_t*)nimcp_calloc(
        1, sizeof(nimcp_dynamic_arch_t));
    if (!h) {
        return NULL;
    }

    if (config) {
        h->config = *config;
    } else {
        h->config = nimcp_dynamic_arch_config_default();
    }

    if (h->config.max_recommendations > NIMCP_DYNARCH_MAX_REGIONS) {
        h->config.max_recommendations = NIMCP_DYNARCH_MAX_REGIONS;
    }

    return h;
}

void nimcp_dynamic_arch_destroy(nimcp_dynamic_arch_t* handle)
{
    if (handle) {
        nimcp_free(handle);
    }
}

/* ============================================================================
 * Region Registration
 * ============================================================================ */

int nimcp_dynamic_arch_register_region(
    nimcp_dynamic_arch_t* handle,
    const char* name,
    uint32_t neuron_start,
    uint32_t neuron_count)
{
    if (!handle || !name) {
        return -1;
    }
    if (handle->region_count >= NIMCP_DYNARCH_MAX_REGIONS) {
        return -1;
    }

    /* Reject duplicates */
    if (find_region(handle, name) >= 0) {
        return -1;
    }

    nimcp_region_stat_t* rs = &handle->region_stats[handle->region_count];
    memset(rs, 0, sizeof(*rs));
    strncpy(rs->name, name, sizeof(rs->name) - 1);
    rs->name[sizeof(rs->name) - 1] = '\0';
    rs->neuron_start  = neuron_start;
    rs->neuron_count  = neuron_count;

    handle->region_count++;
    return 0;
}

/* ============================================================================
 * Activation Recording
 * ============================================================================ */

int nimcp_dynamic_arch_record_activation(
    nimcp_dynamic_arch_t* handle,
    const char* region_name,
    uint32_t neuron_idx,
    float activation)
{
    if (!handle || !region_name) {
        return -1;
    }

    int idx = find_region(handle, region_name);
    if (idx < 0) {
        return -1;
    }

    (void)neuron_idx;  /* tracked for future per-neuron analysis */

    nimcp_region_stat_t* rs = &handle->region_stats[idx];
    rs->activation_sum += (double)fabsf(activation);
    rs->activation_count++;

    return 0;
}

/* ============================================================================
 * Analysis — compute utilisation and generate recommendations
 * ============================================================================ */

int nimcp_dynamic_arch_analyze(nimcp_dynamic_arch_t* handle)
{
    if (!handle) {
        return -1;
    }

    handle->recommendation_count = 0;

    for (uint32_t i = 0; i < handle->region_count; i++) {
        nimcp_region_stat_t* rs = &handle->region_stats[i];

        /* Compute average utilisation.
         * Utilisation = (activation_sum / activation_count) clamped to [0, 1].
         * If no activations recorded, utilisation is 0 (idle region). */
        if (rs->activation_count > 0 && rs->neuron_count > 0) {
            float mean_activation = (float)(rs->activation_sum / (double)rs->activation_count);
            /* Normalise: we treat mean_activation as a proxy for utilisation.
             * A fully active region has mean ~1.0; a dormant one ~0.0. */
            rs->avg_utilization = mean_activation;
            if (rs->avg_utilization > 1.0f) rs->avg_utilization = 1.0f;
            if (rs->avg_utilization < 0.0f) rs->avg_utilization = 0.0f;
        } else {
            rs->avg_utilization = 0.0f;
        }

        /* Generate recommendation if thresholds are crossed */
        if (handle->recommendation_count >= handle->config.max_recommendations) {
            continue;
        }

        nimcp_arch_action_t action = NIMCP_ARCH_NONE;
        int32_t delta = 0;

        if (rs->avg_utilization > handle->config.grow_threshold) {
            action = NIMCP_ARCH_GROW;
            /* Suggest growing by 20% of current neuron count */
            delta = (int32_t)(rs->neuron_count * 0.2f);
            if (delta < 1) delta = 1;
        } else if (rs->avg_utilization < handle->config.shrink_threshold) {
            action = NIMCP_ARCH_SHRINK;
            /* Suggest shrinking by 10% of current neuron count */
            delta = -(int32_t)(rs->neuron_count * 0.1f);
            if (delta > -1) delta = -1;
        }

        if (action != NIMCP_ARCH_NONE) {
            nimcp_arch_recommendation_t* rec =
                &handle->recommendations[handle->recommendation_count];
            strncpy(rec->region_name, rs->name, sizeof(rec->region_name) - 1);
            rec->region_name[sizeof(rec->region_name) - 1] = '\0';
            rec->action          = action;
            rec->suggested_delta = delta;
            rec->utilization     = rs->avg_utilization;
            handle->recommendation_count++;
        }

        /* Reset accumulators for next analysis window */
        rs->activation_sum   = 0.0;
        rs->activation_count = 0;
    }

    return (int)handle->recommendation_count;
}

/* ============================================================================
 * Query
 * ============================================================================ */

int nimcp_dynamic_arch_get_recommendation(
    const nimcp_dynamic_arch_t* handle,
    uint32_t idx,
    nimcp_arch_recommendation_t* recommendation_out)
{
    if (!handle || !recommendation_out) {
        return -1;
    }
    if (idx >= handle->recommendation_count) {
        return -1;
    }

    *recommendation_out = handle->recommendations[idx];
    return 0;
}

float nimcp_dynamic_arch_get_utilization(
    const nimcp_dynamic_arch_t* handle,
    const char* region_name)
{
    if (!handle || !region_name) {
        return -1.0f;
    }

    int idx = find_region(handle, region_name);
    if (idx < 0) {
        return -1.0f;
    }

    return handle->region_stats[idx].avg_utilization;
}
