/**
 * @file nimcp_temporal_discounting.c
 * @brief Temporal discounting system implementation
 * @date 2026-01-11
 *
 * Implements hyperbolic discounting:
 *   V = A / (1 + k * D)
 *
 * Where:
 *   V = subjective present value
 *   A = actual future reward amount
 *   k = discount rate (lower = more patient)
 *   D = delay to reward
 */

#include "core/brain/regions/raphe/nimcp_temporal_discounting.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(temporal_discounting, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * @brief Hyperbolic discounting function
 * V = A / (1 + k * D)
 */
static float hyperbolic_discount(float amount, float delay, float k) {
    if (delay <= 0.0f) return amount;
    return amount / (1.0f + k * delay);
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_temporal_config_t nimcp_temporal_default_config(void) {
    nimcp_temporal_config_t config = {
        .baseline_k = TEMPORAL_DEFAULT_K,
        .baseline_orientation = TEMPORAL_DEFAULT_ORIENTATION,
        .ht_discount_gain = TEMPORAL_5HT_DISCOUNT_GAIN,
        .ht_orientation_gain = 0.4f,
        .min_k = 0.01f,   /* Very patient */
        .max_k = 1.0f     /* Very impulsive */
    };
    return config;
}

int nimcp_temporal_init(nimcp_temporal_system_t* system,
                        const nimcp_temporal_config_t* config) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    memset(system, 0, sizeof(nimcp_temporal_system_t));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_temporal_default_config();
    }

    /* Initialize state */
    system->discount_rate = system->config.baseline_k;
    system->future_orientation = system->config.baseline_orientation;
    system->delay_tolerance = 0.5f;

    /* Initialize 5-HT state */
    system->current_5ht = 20.0f;  /* nM baseline */
    system->baseline_5ht = 20.0f;

    /* Initialize statistics */
    system->choices_made = 0;
    system->delayed_chosen = 0;
    system->immediate_chosen = 0;
    system->avg_k = system->config.baseline_k;

    system->initialized = true;
    return 0;
}

int nimcp_temporal_shutdown(nimcp_temporal_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    system->initialized = false;
    return 0;
}

int nimcp_temporal_reset(nimcp_temporal_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_temporal_config_t config = system->config;
    return nimcp_temporal_init(system, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_temporal_update(nimcp_temporal_system_t* system, float ht_level, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float dt_sec = dt / 1000.0f;
    system->current_5ht = ht_level;

    /* Compute 5-HT ratio */
    float ht_ratio = ht_level / system->baseline_5ht;

    /* 1. Update discount rate (k) based on 5-HT */
    /* Higher 5-HT -> lower k -> more patient (less discounting) */
    /* Lower 5-HT -> higher k -> more impulsive (more discounting) */
    float k_adjustment = (1.0f - ht_ratio) * system->config.ht_discount_gain;
    float k_target = system->config.baseline_k + k_adjustment;
    k_target = nimcp_clampf(k_target, system->config.min_k, system->config.max_k);

    /* Smooth transition */
    float alpha = 1.0f - expf(-dt_sec * 0.2f);
    system->discount_rate = lerp(system->discount_rate, k_target, alpha);

    /* 2. Update future orientation */
    /* Higher 5-HT -> more future-oriented */
    float orientation_target = system->config.baseline_orientation +
                               (ht_ratio - 1.0f) * system->config.ht_orientation_gain;
    orientation_target = nimcp_clampf(orientation_target, 0.1f, 0.9f);
    system->future_orientation = lerp(system->future_orientation,
                                       orientation_target, alpha);

    /* 3. Update delay tolerance */
    /* Based on both 5-HT and future orientation */
    float tolerance_target = (ht_ratio + system->future_orientation) / 2.0f;
    tolerance_target = nimcp_clampf(tolerance_target, 0.1f, 0.9f);
    system->delay_tolerance = lerp(system->delay_tolerance, tolerance_target, alpha);

    /* 4. Update running average k */
    system->avg_k = lerp(system->avg_k, system->discount_rate, 0.01f);

    return 0;
}

/*=============================================================================
 * Discounting API
 *===========================================================================*/

int nimcp_temporal_discount_value(nimcp_temporal_system_t* system,
                                  float value,
                                  float delay,
                                  float* discounted_value) {
    if (!system || !system->initialized || !discounted_value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_update: required parameter is NULL (system, system->initialized, discounted_value)");
        return -1;
    }
    if (value < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_temporal_update: validation failed");
        return -1;
    }

    *discounted_value = hyperbolic_discount(value, delay, system->discount_rate);
    return 0;
}

int nimcp_temporal_get_current_k(nimcp_temporal_system_t* system, float* k) {
    if (!system || !system->initialized || !k) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_get_current_k: required parameter is NULL (system, system->initialized, k)");
        return -1;
    }

    *k = system->discount_rate;
    return 0;
}

int nimcp_temporal_evaluate_choice(nimcp_temporal_system_t* system,
                                   float immediate_value,
                                   float delayed_value,
                                   float delay,
                                   nimcp_temporal_choice_t* result) {
    if (!system || !system->initialized || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_get_current_k: required parameter is NULL (system, system->initialized, result)");
        return -1;
    }

    /* Compute discounted value of delayed reward */
    float discounted = hyperbolic_discount(delayed_value, delay, system->discount_rate);

    /* Store results */
    result->discounted_value = discounted;
    result->effective_k = system->discount_rate;

    /* Make choice */
    result->prefer_delayed = (discounted > immediate_value);

    /* Compute indifference delay */
    /* At indifference: immediate = delayed / (1 + k*D) */
    /* Solving: D = (delayed/immediate - 1) / k */
    if (delayed_value > immediate_value && system->discount_rate > 0.0f) {
        result->indifference_delay = (delayed_value / immediate_value - 1.0f) /
                                     system->discount_rate;
    } else {
        result->indifference_delay = 0.0f;
    }

    /* Update statistics */
    system->choices_made++;
    if (result->prefer_delayed) {
        system->delayed_chosen++;
    } else {
        system->immediate_chosen++;
    }

    return 0;
}

int nimcp_temporal_find_indifference(nimcp_temporal_system_t* system,
                                     float immediate_value,
                                     float delayed_value,
                                     float* indifference_delay) {
    if (!system || !system->initialized || !indifference_delay) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_get_current_k: required parameter is NULL (system, system->initialized, indifference_delay)");
        return -1;
    }
    if (immediate_value <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_temporal_get_current_k: validation failed");
        return -1;
    }

    /* At indifference point: immediate = delayed / (1 + k*D) */
    /* Solving for D: D = (delayed/immediate - 1) / k */

    if (delayed_value <= immediate_value) {
        /* Delayed is worth less or equal, no positive delay would help */
        *indifference_delay = 0.0f;
        return 0;
    }

    if (system->discount_rate <= 0.001f) {
        /* Near-zero discounting, effectively no indifference */
        *indifference_delay = 1000000.0f;  /* Very large */
        return 0;
    }

    float ratio = delayed_value / immediate_value;
    *indifference_delay = (ratio - 1.0f) / system->discount_rate;

    return 0;
}

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_temporal_get_discount_rate(nimcp_temporal_system_t* system, float* rate) {
    if (!system || !system->initialized || !rate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_get_discount_rate: required parameter is NULL (system, system->initialized, rate)");
        return -1;
    }

    *rate = system->discount_rate;
    return 0;
}

int nimcp_temporal_get_future_orientation(nimcp_temporal_system_t* system,
                                          float* orientation) {
    if (!system || !system->initialized || !orientation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_get_discount_rate: required parameter is NULL (system, system->initialized, orientation)");
        return -1;
    }

    *orientation = system->future_orientation;
    return 0;
}

int nimcp_temporal_get_delay_tolerance(nimcp_temporal_system_t* system,
                                       float* tolerance) {
    if (!system || !system->initialized || !tolerance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_temporal_get_discount_rate: required parameter is NULL (system, system->initialized, tolerance)");
        return -1;
    }

    *tolerance = system->delay_tolerance;
    return 0;
}
