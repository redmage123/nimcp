/**
 * @file nimcp_lgss_vta_guard.c
 * @brief LGSS Component A9: VTA Guard Implementation
 * @date 2026-01-16
 *
 * Implementation of VTA dopamine emission control and safety gating.
 */

#include "security/lgss/reward/nimcp_lgss_vta_guard.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_vta_guard, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

#define VTA_GUARD_RATE_WINDOW_SIZE 32

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    static uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Calculate DA emission rate from window
 */
static float calculate_da_rate(const vta_guard_t* guard) {
    if (!guard) return 0.0f;

    float total_da = 0.0f;
    for (int i = 0; i < VTA_GUARD_RATE_WINDOW_SIZE; i++) {
        total_da += guard->da_rate_window[i];
    }

    /* Assume window represents 1 second of activity */
    return total_da;
}

/**
 * @brief Update DA rate window
 */
static void update_rate_window(vta_guard_t* guard, float da_amount) {
    if (!guard) return;

    guard->da_rate_window[guard->rate_window_idx] = da_amount;
    guard->rate_window_idx = (guard->rate_window_idx + 1) % VTA_GUARD_RATE_WINDOW_SIZE;
}

/**
 * @brief Compute pathway hash
 */
static uint32_t compute_pathway_hash(const vta_guard_t* guard, da_pathway_t pathway) {
    if (!guard) return 0;

    /* Simple hash of pathway configuration */
    uint32_t hash = 0xDEADBEEF;
    hash ^= (uint32_t)pathway;
    hash = (hash << 5) | (hash >> 27);
    hash ^= (uint32_t)(guard->config.max_da_rate * 1000.0f);
    hash ^= (uint32_t)(guard->config.max_burst_frequency * 1000.0f);

    return hash;
}

/*=============================================================================
 * LIFECYCLE API IMPLEMENTATION
 *===========================================================================*/

vta_guard_config_t vta_guard_default_config(void) {
    vta_guard_config_t config = {
        /* DA limits */
        .max_da_rate = VTA_GUARD_MAX_DA_RATE,
        .max_da_concentration = VTA_GUARD_DA_CEILING,
        .da_decay_rate = 0.1f,

        /* Burst limits */
        .max_burst_frequency = VTA_GUARD_MAX_BURST_FREQ,
        .min_burst_interval_ms = VTA_GUARD_MIN_BURST_INTERVAL_MS,
        .max_burst_spikes = 10,

        /* RPE limits */
        .max_rpe_positive = VTA_GUARD_MAX_RPE_MAGNITUDE,
        .max_rpe_negative = VTA_GUARD_MAX_RPE_MAGNITUDE,

        /* Receptor limits */
        .min_d1_d2_ratio = VTA_GUARD_D1_D2_MIN_RATIO,
        .max_d1_d2_ratio = VTA_GUARD_D1_D2_MAX_RATIO,

        /* Pathway protection */
        .protect_mesolimbic = true,
        .protect_mesocortical = true,
        .allow_pathway_modification = false
    };
    return config;
}

vta_guard_t* vta_guard_create(const vta_guard_config_t* config) {
    vta_guard_t* guard = nimcp_calloc(1, sizeof(vta_guard_t));
    if (!guard) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vta_guard_create: allocation failed");

        return NULL;
    }

    guard->magic = VTA_GUARD_MAGIC;

    /* Apply configuration */
    if (config) {
        guard->config = *config;
    } else {
        guard->config = vta_guard_default_config();
    }

    /* Initialize state */
    guard->current_da = 0.0f;
    memset(guard->da_rate_window, 0, sizeof(guard->da_rate_window));
    guard->rate_window_idx = 0;
    guard->last_burst_time = 0;
    guard->burst_count_window = 0;

    /* Initialize pathway protection */
    for (int i = 0; i < DA_PATHWAY_COUNT; i++) {
        guard->pathways_protected[i] = false;
        guard->pathway_hashes[i] = 0;
    }

    /* Apply default protections */
    if (guard->config.protect_mesolimbic) {
        guard->pathways_protected[DA_PATHWAY_MESOLIMBIC] = true;
        guard->pathway_hashes[DA_PATHWAY_MESOLIMBIC] =
            compute_pathway_hash(guard, DA_PATHWAY_MESOLIMBIC);
    }
    if (guard->config.protect_mesocortical) {
        guard->pathways_protected[DA_PATHWAY_MESOCORTICAL] = true;
        guard->pathway_hashes[DA_PATHWAY_MESOCORTICAL] =
            compute_pathway_hash(guard, DA_PATHWAY_MESOCORTICAL);
    }

    /* Initialize D1/D2 */
    guard->d1_activation = 0.5f;
    guard->d2_activation = 0.5f;

    /* Initialize statistics */
    memset(&guard->stats, 0, sizeof(vta_guard_stats_t));

    guard->initialized = true;
    return guard;
}

void vta_guard_destroy(vta_guard_t* guard) {
    if (!guard) return;
    if (guard->magic != VTA_GUARD_MAGIC) return;

    guard->magic = 0;
    guard->initialized = false;
    nimcp_free(guard);
}

int vta_guard_reset(vta_guard_t* guard) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Reset state */
    guard->current_da = 0.0f;
    memset(guard->da_rate_window, 0, sizeof(guard->da_rate_window));
    guard->rate_window_idx = 0;
    guard->last_burst_time = 0;
    guard->burst_count_window = 0;
    guard->d1_activation = 0.5f;
    guard->d2_activation = 0.5f;

    /* Keep configuration and pathway protection */
    return 0;
}

/*=============================================================================
 * CORE DA EMISSION API IMPLEMENTATION
 *===========================================================================*/

vta_guard_status_t vta_guard_emit_dopamine(
    vta_guard_t* guard,
    const da_emission_request_t* request,
    da_emission_result_t* result)
{
    if (!guard || !request || !result) {
        if (result) {
            result->status = VTA_GUARD_ERROR;
            snprintf(result->message, sizeof(result->message), "Invalid parameters");
        }
        return VTA_GUARD_ERROR;
    }
    if (guard->magic != VTA_GUARD_MAGIC) {
        result->status = VTA_GUARD_ERROR;
        snprintf(result->message, sizeof(result->message), "Invalid guard handle");
        return VTA_GUARD_ERROR;
    }

    uint64_t now = get_timestamp_us();
    guard->stats.total_emissions++;

    /* Initialize result */
    result->status = VTA_GUARD_OK;
    result->da_emitted = request->da_amount;
    result->limitation_factor = 1.0f;
    result->alert = VTA_ALERT_NONE;
    result->message[0] = '\0';

    float requested_da = request->da_amount;

    /* Check 1: Pathway protection */
    if (request->pathway < DA_PATHWAY_COUNT) {
        if (guard->pathways_protected[request->pathway]) {
            uint32_t current_hash = compute_pathway_hash(guard, request->pathway);
            if (current_hash != guard->pathway_hashes[request->pathway]) {
                result->status = VTA_GUARD_BLOCKED;
                result->da_emitted = 0.0f;
                result->alert = VTA_ALERT_PATHWAY_BYPASS;
                snprintf(result->message, sizeof(result->message),
                        "Pathway bypass attempt blocked");
                guard->stats.blocked++;
                guard->stats.alerts_triggered++;
                guard->stats.last_alert = VTA_ALERT_PATHWAY_BYPASS;
                guard->stats.last_alert_time = now;

                if (guard->alert_callback) {
                    guard->alert_callback(VTA_ALERT_PATHWAY_BYPASS,
                                         guard->callback_user_data);
                }
                return VTA_GUARD_BLOCKED;
            }
        }
    }

    /* Check 2: DA ceiling */
    if (guard->current_da + requested_da > guard->config.max_da_concentration) {
        float allowed = guard->config.max_da_concentration - guard->current_da;
        if (allowed < 0.0f) allowed = 0.0f;

        result->da_emitted = allowed;
        result->limitation_factor = (requested_da > 0.0f)
                                   ? allowed / requested_da
                                   : 0.0f;
        result->status = VTA_GUARD_RATE_LIMITED;
        result->alert = VTA_ALERT_DA_CEILING;
        snprintf(result->message, sizeof(result->message),
                "DA ceiling reached (%.1f nM)", guard->config.max_da_concentration);

        guard->stats.rate_limited++;
        guard->stats.alerts_triggered++;
        guard->stats.last_alert = VTA_ALERT_DA_CEILING;
        guard->stats.last_alert_time = now;

        if (guard->alert_callback) {
            guard->alert_callback(VTA_ALERT_DA_CEILING, guard->callback_user_data);
        }
    }

    /* Check 3: Rate limiting */
    float current_rate = calculate_da_rate(guard);
    if (current_rate + requested_da > guard->config.max_da_rate) {
        float allowed = guard->config.max_da_rate - current_rate;
        if (allowed < 0.0f) allowed = 0.0f;

        if (allowed < result->da_emitted) {
            result->da_emitted = allowed;
            result->limitation_factor = (requested_da > 0.0f)
                                       ? allowed / requested_da
                                       : 0.0f;
        }

        if (result->status == VTA_GUARD_OK) {
            result->status = VTA_GUARD_RATE_LIMITED;
        }
        result->alert = VTA_ALERT_HIGH_DA_RATE;
        snprintf(result->message, sizeof(result->message),
                "DA rate limited (%.1f/%.1f nM/s)",
                current_rate, guard->config.max_da_rate);

        guard->stats.rate_limited++;
        guard->stats.alerts_triggered++;
        guard->stats.last_alert = VTA_ALERT_HIGH_DA_RATE;
        guard->stats.last_alert_time = now;

        if (guard->alert_callback) {
            guard->alert_callback(VTA_ALERT_HIGH_DA_RATE, guard->callback_user_data);
        }
    }

    /* Check 4: Burst frequency (for phasic emissions) */
    if (request->is_phasic) {
        float time_since_burst = (float)(now - guard->last_burst_time);

        if (time_since_burst < guard->config.min_burst_interval_ms * 1000.0f) {
            result->status = VTA_GUARD_RATE_LIMITED;
            result->da_emitted *= 0.5f;  /* Reduce by half */
            result->limitation_factor *= 0.5f;
            result->alert = VTA_ALERT_BURST_FREQUENCY;
            snprintf(result->message, sizeof(result->message),
                    "Burst frequency limited");

            guard->stats.bursts_limited++;
            guard->stats.alerts_triggered++;
            guard->stats.last_alert = VTA_ALERT_BURST_FREQUENCY;
            guard->stats.last_alert_time = now;

            if (guard->alert_callback) {
                guard->alert_callback(VTA_ALERT_BURST_FREQUENCY,
                                     guard->callback_user_data);
            }
        } else {
            guard->last_burst_time = now;
            guard->burst_count_window++;
            guard->stats.burst_count++;
        }
    }

    /* Check 5: RPE magnitude (if provided) */
    if (fabsf(request->rpe_value) > 0.001f) {
        float max_rpe = (request->rpe_value > 0.0f)
                       ? guard->config.max_rpe_positive
                       : guard->config.max_rpe_negative;

        if (fabsf(request->rpe_value) > max_rpe) {
            result->alert = VTA_ALERT_RPE_MAGNITUDE;
            guard->stats.rpe_limited++;
            guard->stats.alerts_triggered++;
            guard->stats.last_alert = VTA_ALERT_RPE_MAGNITUDE;
            guard->stats.last_alert_time = now;
        }

        guard->stats.rpe_emissions++;
        if (fabsf(request->rpe_value) > guard->stats.max_rpe_seen) {
            guard->stats.max_rpe_seen = fabsf(request->rpe_value);
        }
    }

    /* Apply emission */
    if (result->da_emitted > 0.0f) {
        guard->current_da += result->da_emitted;
        update_rate_window(guard, result->da_emitted);
        guard->stats.total_da_emitted += result->da_emitted;
    }

    /* Update statistics */
    guard->stats.current_da_rate = calculate_da_rate(guard);
    if (guard->stats.current_da_rate > guard->stats.peak_da_rate) {
        guard->stats.peak_da_rate = guard->stats.current_da_rate;
    }

    if (result->message[0] == '\0') {
        snprintf(result->message, sizeof(result->message),
                "Emitted %.2f nM DA", result->da_emitted);
    }

    return result->status;
}

vta_guard_status_t vta_guard_emit_rpe(
    vta_guard_t* guard,
    float rpe,
    float* da_emitted)
{
    if (!guard || !da_emitted) {
        return VTA_GUARD_ERROR;
    }
    if (guard->magic != VTA_GUARD_MAGIC) {
        return VTA_GUARD_ERROR;
    }

    /* Clamp RPE to limits */
    float clamped_rpe = rpe;
    if (rpe > 0.0f && rpe > guard->config.max_rpe_positive) {
        clamped_rpe = guard->config.max_rpe_positive;
    } else if (rpe < 0.0f && fabsf(rpe) > guard->config.max_rpe_negative) {
        clamped_rpe = -guard->config.max_rpe_negative;
    }

    /* Convert RPE to DA amount (simplified model) */
    /* Positive RPE -> DA burst, Negative RPE -> DA pause (reduction) */
    float da_amount;
    if (clamped_rpe > 0.0f) {
        /* Positive RPE: phasic burst */
        da_amount = clamped_rpe * 20.0f;  /* Scale factor */
    } else {
        /* Negative RPE: pause (we don't emit negative DA) */
        da_amount = 0.0f;
    }

    /* Create emission request */
    da_emission_request_t request;
    da_emission_request_init(&request, da_amount, DA_PATHWAY_MESOLIMBIC,
                             clamped_rpe > 0.0f);
    request.rpe_value = clamped_rpe;

    da_emission_result_t result;
    vta_guard_status_t status = vta_guard_emit_dopamine(guard, &request, &result);

    *da_emitted = result.da_emitted;
    return status;
}

vta_guard_status_t vta_guard_trigger_burst(
    vta_guard_t* guard,
    float intensity,
    float duration_ms,
    da_emission_result_t* result)
{
    if (!guard || !result) {
        return VTA_GUARD_ERROR;
    }
    if (guard->magic != VTA_GUARD_MAGIC) {
        return VTA_GUARD_ERROR;
    }

    /* Clamp intensity */
    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);

    /* Calculate DA based on intensity and duration */
    float da_amount = intensity * duration_ms * 0.1f;  /* Scale factor */

    da_emission_request_t request;
    da_emission_request_init(&request, da_amount, DA_PATHWAY_MESOLIMBIC, true);

    return vta_guard_emit_dopamine(guard, &request, result);
}

/*=============================================================================
 * RATE CONTROL API IMPLEMENTATION
 *===========================================================================*/

int vta_guard_get_da_rate(const vta_guard_t* guard, float* rate) {
    if (!guard || !rate) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    *rate = calculate_da_rate(guard);
    return 0;
}

bool vta_guard_emission_allowed(const vta_guard_t* guard) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return false;
    }

    float current_rate = calculate_da_rate(guard);
    if (current_rate >= guard->config.max_da_rate) {
        return false;
    }

    if (guard->current_da >= guard->config.max_da_concentration) {
        return false;
    }

    return true;
}

float vta_guard_get_rate_limit_factor(const vta_guard_t* guard) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return 0.0f;
    }

    float current_rate = calculate_da_rate(guard);
    if (current_rate >= guard->config.max_da_rate) {
        return 0.0f;
    }

    float available = guard->config.max_da_rate - current_rate;
    return nimcp_clampf(available / guard->config.max_da_rate, 0.0f, 1.0f);
}

/*=============================================================================
 * PATHWAY PROTECTION API IMPLEMENTATION
 *===========================================================================*/

int vta_guard_protect_pathway(vta_guard_t* guard, da_pathway_t pathway) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (pathway >= DA_PATHWAY_COUNT) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    guard->pathways_protected[pathway] = true;
    guard->pathway_hashes[pathway] = compute_pathway_hash(guard, pathway);
    return 0;
}

bool vta_guard_verify_pathway(const vta_guard_t* guard, da_pathway_t pathway) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return false;
    }
    if (pathway >= DA_PATHWAY_COUNT) {
        return false;
    }
    if (!guard->pathways_protected[pathway]) {
        return true;  /* Not protected, nothing to verify */
    }

    uint32_t current_hash = compute_pathway_hash(guard, pathway);
    return current_hash == guard->pathway_hashes[pathway];
}

bool vta_guard_pathway_protected(const vta_guard_t* guard, da_pathway_t pathway) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return false;
    }
    if (pathway >= DA_PATHWAY_COUNT) {
        return false;
    }
    return guard->pathways_protected[pathway];
}

/*=============================================================================
 * RECEPTOR MONITORING API IMPLEMENTATION
 *===========================================================================*/

int vta_guard_update_receptor_activation(
    vta_guard_t* guard,
    float d1_activation,
    float d2_activation)
{
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    guard->d1_activation = nimcp_clampf(d1_activation, 0.0f, 1.0f);
    guard->d2_activation = nimcp_clampf(d2_activation, 0.0f, 1.0f);

    /* Check D1/D2 balance */
    float ratio = (d2_activation > 0.001f)
                 ? d1_activation / d2_activation
                 : d1_activation * 100.0f;

    if (ratio < guard->config.min_d1_d2_ratio ||
        ratio > guard->config.max_d1_d2_ratio) {
        guard->stats.alerts_triggered++;
        guard->stats.last_alert = VTA_ALERT_RECEPTOR_IMBALANCE;
        guard->stats.last_alert_time = get_timestamp_us();

        if (guard->alert_callback) {
            guard->alert_callback(VTA_ALERT_RECEPTOR_IMBALANCE,
                                 guard->callback_user_data);
        }
        return -1;  /* Imbalanced */
    }

    return 0;
}

int vta_guard_get_d1_d2_ratio(const vta_guard_t* guard, float* ratio) {
    if (!guard || !ratio || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (guard->d2_activation > 0.001f) {
        *ratio = guard->d1_activation / guard->d2_activation;
    } else {
        *ratio = guard->d1_activation * 100.0f;  /* Very high if D2 is near zero */
    }
    return 0;
}

bool vta_guard_receptor_balance_ok(const vta_guard_t* guard) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return false;
    }

    float ratio;
    if (vta_guard_get_d1_d2_ratio(guard, &ratio) != 0) {
        return false;
    }

    return (ratio >= guard->config.min_d1_d2_ratio &&
            ratio <= guard->config.max_d1_d2_ratio);
}

/*=============================================================================
 * INTEGRATION API IMPLEMENTATION
 *===========================================================================*/

int vta_guard_set_vta(vta_guard_t* guard, void* vta) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    guard->vta = vta;
    return 0;
}

int vta_guard_set_reward_monitor(
    vta_guard_t* guard,
    reward_alignment_monitor_t* monitor)
{
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    guard->reward_monitor = monitor;
    return 0;
}

/*=============================================================================
 * STATISTICS API IMPLEMENTATION
 *===========================================================================*/

int vta_guard_get_stats(const vta_guard_t* guard, vta_guard_stats_t* stats) {
    if (!guard || !stats || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    *stats = guard->stats;
    return 0;
}

int vta_guard_reset_stats(vta_guard_t* guard) {
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    memset(&guard->stats, 0, sizeof(vta_guard_stats_t));
    return 0;
}

/*=============================================================================
 * CALLBACK API IMPLEMENTATION
 *===========================================================================*/

int vta_guard_set_alert_callback(
    vta_guard_t* guard,
    void (*callback)(vta_alert_type_t type, void* user_data),
    void* user_data)
{
    if (!guard || guard->magic != VTA_GUARD_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    guard->alert_callback = callback;
    guard->callback_user_data = user_data;
    return 0;
}

/*=============================================================================
 * UTILITY API IMPLEMENTATION
 *===========================================================================*/

const char* vta_guard_status_string(vta_guard_status_t status) {
    switch (status) {
        case VTA_GUARD_OK:
            return "OK";
        case VTA_GUARD_RATE_LIMITED:
            return "RATE_LIMITED";
        case VTA_GUARD_BLOCKED:
            return "BLOCKED";
        case VTA_GUARD_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

const char* vta_alert_type_string(vta_alert_type_t type) {
    switch (type) {
        case VTA_ALERT_NONE:
            return "NONE";
        case VTA_ALERT_HIGH_DA_RATE:
            return "HIGH_DA_RATE";
        case VTA_ALERT_BURST_FREQUENCY:
            return "BURST_FREQUENCY";
        case VTA_ALERT_DA_CEILING:
            return "DA_CEILING";
        case VTA_ALERT_RPE_MAGNITUDE:
            return "RPE_MAGNITUDE";
        case VTA_ALERT_RECEPTOR_IMBALANCE:
            return "RECEPTOR_IMBALANCE";
        case VTA_ALERT_PATHWAY_BYPASS:
            return "PATHWAY_BYPASS";
        default:
            return "UNKNOWN";
    }
}

const char* da_pathway_string(da_pathway_t pathway) {
    switch (pathway) {
        case DA_PATHWAY_MESOLIMBIC:
            return "MESOLIMBIC";
        case DA_PATHWAY_MESOCORTICAL:
            return "MESOCORTICAL";
        case DA_PATHWAY_NIGROSTRIATAL:
            return "NIGROSTRIATAL";
        case DA_PATHWAY_TUBEROINFUNDIBULAR:
            return "TUBEROINFUNDIBULAR";
        default:
            return "UNKNOWN";
    }
}

void da_emission_request_init(
    da_emission_request_t* request,
    float da_amount,
    da_pathway_t pathway,
    bool is_phasic)
{
    if (!request) return;

    memset(request, 0, sizeof(da_emission_request_t));
    request->da_amount = da_amount;
    request->pathway = pathway;
    request->target_region = 0;
    request->is_phasic = is_phasic;
    request->rpe_value = 0.0f;
    request->timestamp_us = get_timestamp_us();
}
