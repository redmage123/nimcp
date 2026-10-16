/**
 * @file nimcp_lgss_autonomic_gate.c
 * @brief LGSS Autonomic Output Gate implementation
 *
 * WHAT: Implements gated autonomic control for hormones and vital signs.
 * WHY:  Ensures internal state changes are validated to maintain system stability.
 * HOW:  Hormone release limiting, vital signs monitoring, homeostatic regulation.
 */

#include "security/lgss/gates/nimcp_lgss_autonomic_gate.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_autonomic_gate, MESH_ADAPTER_CATEGORY_SECURITY)


#endif

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

/**
 * @brief Autonomic gate internal structure
 */
struct autonomic_gate {
    uint32_t magic;                         /**< Magic number for validation */
    bool enabled;                           /**< Whether gate is enabled */
    autonomic_gate_config_t config;         /**< Gate configuration */
    autonomic_gate_stats_t stats;           /**< Operational statistics */

    /* Hormone state */
    autonomic_hormone_limits_t hormone_limits[NIMCP_AUTONOMIC_HORMONE_COUNT];
    float hormone_levels[NIMCP_AUTONOMIC_HORMONE_COUNT];
    float hormone_rates[NIMCP_AUTONOMIC_HORMONE_COUNT];
    uint64_t hormone_last_update[NIMCP_AUTONOMIC_HORMONE_COUNT];
    bool hormone_locked[NIMCP_AUTONOMIC_HORMONE_COUNT];

    /* Vital signs state */
    autonomic_vital_bounds_t vital_bounds[VITAL_COUNT];
    float vital_values[VITAL_COUNT];
    uint64_t vitals_last_update;

    uint32_t next_sequence_id;              /**< Next sequence ID */
};

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return 0;
#endif
}

/**
 * @brief Validate gate structure
 */
static bool validate_gate(const autonomic_gate_t* gate) {
    return gate != NULL && gate->magic == NIMCP_AUTONOMIC_GATE_MAGIC;
}

/**
 * @brief Validate hormone enum
 */
static bool validate_hormone(autonomic_hormone_t hormone) {
    return hormone >= 0 && hormone < NIMCP_AUTONOMIC_HORMONE_COUNT;
}

/**
 * @brief Validate vital sign enum
 */
static bool validate_vital(autonomic_vital_t vital) {
    return vital >= 0 && vital < VITAL_COUNT;
}

/**
 * @brief Get vital sign status based on bounds
 */
static autonomic_vital_status_t get_vital_status(
    float value,
    const autonomic_vital_bounds_t* bounds
) {
    if (value <= bounds->critical_min) {
        return VITAL_STATUS_CRITICAL_LOW;
    }
    if (value >= bounds->critical_max) {
        return VITAL_STATUS_CRITICAL_HIGH;
    }
    if (value < bounds->normal_min) {
        return VITAL_STATUS_LOW;
    }
    if (value > bounds->normal_max) {
        return VITAL_STATUS_HIGH;
    }
    return VITAL_STATUS_NORMAL;
}

/**
 * @brief Initialize default hormone limits
 */
static void init_default_hormone_limits(autonomic_hormone_limits_t* limits) {
    limits->min_level = NIMCP_AUTONOMIC_DEFAULT_MIN_LEVEL;
    limits->max_level = NIMCP_AUTONOMIC_DEFAULT_MAX_LEVEL;
    limits->max_release_rate = NIMCP_AUTONOMIC_DEFAULT_RELEASE_RATE;
    limits->max_absorption_rate = NIMCP_AUTONOMIC_DEFAULT_RELEASE_RATE;
    limits->target_baseline = 0.5f;  /* Middle of range */
    limits->enabled = true;
    limits->locked = false;
}

/**
 * @brief Initialize default vital bounds
 */
static void init_default_vital_bounds(autonomic_vital_bounds_t* bounds, autonomic_vital_t vital) {
    switch (vital) {
        case VITAL_HEART_RATE:
            bounds->normal_min = 60.0f;
            bounds->normal_max = 100.0f;
            bounds->critical_min = NIMCP_AUTONOMIC_DEFAULT_HR_MIN;
            bounds->critical_max = NIMCP_AUTONOMIC_DEFAULT_HR_MAX;
            bounds->max_rate_of_change = 20.0f;  /* BPM per second */
            break;

        case VITAL_TEMPERATURE:
            bounds->normal_min = 36.0f;
            bounds->normal_max = 38.0f;
            bounds->critical_min = 34.0f;
            bounds->critical_max = 42.0f;
            bounds->max_rate_of_change = 0.5f;  /* Degrees per second */
            break;

        case VITAL_RESPIRATION:
            bounds->normal_min = 12.0f;
            bounds->normal_max = 20.0f;
            bounds->critical_min = 8.0f;
            bounds->critical_max = 30.0f;
            bounds->max_rate_of_change = 5.0f;
            break;

        case VITAL_BLOOD_PRESSURE_SYS:
            bounds->normal_min = 90.0f;
            bounds->normal_max = 140.0f;
            bounds->critical_min = 70.0f;
            bounds->critical_max = 180.0f;
            bounds->max_rate_of_change = 10.0f;
            break;

        case VITAL_BLOOD_PRESSURE_DIA:
            bounds->normal_min = 60.0f;
            bounds->normal_max = 90.0f;
            bounds->critical_min = 40.0f;
            bounds->critical_max = 120.0f;
            bounds->max_rate_of_change = 8.0f;
            break;

        case VITAL_STRESS_LEVEL:
            bounds->normal_min = 0.0f;
            bounds->normal_max = 0.5f;
            bounds->critical_min = 0.0f;  /* Can't go below 0 */
            bounds->critical_max = 0.9f;
            bounds->max_rate_of_change = 0.1f;
            break;

        case VITAL_ENERGY_LEVEL:
            bounds->normal_min = 0.3f;
            bounds->normal_max = 1.0f;
            bounds->critical_min = 0.1f;
            bounds->critical_max = 1.0f;  /* Can't go above 1 */
            bounds->max_rate_of_change = 0.05f;
            break;

        default:
            bounds->normal_min = 0.0f;
            bounds->normal_max = 1.0f;
            bounds->critical_min = 0.0f;
            bounds->critical_max = 1.0f;
            bounds->max_rate_of_change = NIMCP_AUTONOMIC_DEFAULT_VITAL_RATE;
            break;
    }
    bounds->monitoring_enabled = true;
}

/**
 * @brief Fill release details
 */
static void fill_release_details(
    autonomic_release_details_t* details,
    autonomic_release_result_t result,
    autonomic_hormone_t hormone,
    float requested,
    float actual,
    float new_level,
    const char* description
) {
    if (details == NULL) {
        return;
    }
    details->result = result;
    details->hormone = hormone;
    details->requested_amount = requested;
    details->actual_amount = actual;
    details->new_level = new_level;
    details->timestamp = get_timestamp_ns();
    if (description != NULL) {
        snprintf(details->description, sizeof(details->description), "%s", description);
    } else {
        details->description[0] = '\0';
    }
}

/* =============================================================================
 * Public Functions
 * ============================================================================= */

autonomic_gate_t* autonomic_gate_create(const autonomic_gate_config_t* config) {
    autonomic_gate_t* gate = (autonomic_gate_t*)nimcp_calloc(1, sizeof(autonomic_gate_t));
    if (gate == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gate is NULL");

        return NULL;
    }

    gate->magic = NIMCP_AUTONOMIC_GATE_MAGIC;
    gate->enabled = true;
    gate->next_sequence_id = 1;

    /* Initialize hormone limits and levels */
    for (int i = 0; i < NIMCP_AUTONOMIC_HORMONE_COUNT; i++) {
        init_default_hormone_limits(&gate->hormone_limits[i]);
        gate->hormone_levels[i] = gate->hormone_limits[i].target_baseline;
        gate->hormone_rates[i] = 0.0f;
        gate->hormone_last_update[i] = get_timestamp_ns();
        gate->hormone_locked[i] = false;
    }

    /* Set hormone-specific baselines */
    gate->hormone_limits[HORMONE_DOPAMINE].target_baseline = 0.5f;
    gate->hormone_limits[HORMONE_SEROTONIN].target_baseline = 0.6f;
    gate->hormone_limits[HORMONE_NOREPINEPHRINE].target_baseline = 0.3f;
    gate->hormone_limits[HORMONE_CORTISOL].target_baseline = 0.2f;
    gate->hormone_limits[HORMONE_OXYTOCIN].target_baseline = 0.4f;
    gate->hormone_limits[HORMONE_MELATONIN].target_baseline = 0.1f;  /* Low during "day" */
    gate->hormone_limits[HORMONE_ACETYLCHOLINE].target_baseline = 0.5f;
    gate->hormone_limits[HORMONE_GABA].target_baseline = 0.4f;

    /* Update initial levels to baselines */
    for (int i = 0; i < NIMCP_AUTONOMIC_HORMONE_COUNT; i++) {
        gate->hormone_levels[i] = gate->hormone_limits[i].target_baseline;
    }

    /* Initialize vital sign bounds and values */
    for (int i = 0; i < VITAL_COUNT; i++) {
        init_default_vital_bounds(&gate->vital_bounds[i], (autonomic_vital_t)i);
    }

    /* Set initial vital values to middle of normal range */
    gate->vital_values[VITAL_HEART_RATE] = 75.0f;
    gate->vital_values[VITAL_TEMPERATURE] = 37.0f;
    gate->vital_values[VITAL_RESPIRATION] = 16.0f;
    gate->vital_values[VITAL_BLOOD_PRESSURE_SYS] = 120.0f;
    gate->vital_values[VITAL_BLOOD_PRESSURE_DIA] = 80.0f;
    gate->vital_values[VITAL_STRESS_LEVEL] = 0.2f;
    gate->vital_values[VITAL_ENERGY_LEVEL] = 0.8f;

    gate->vitals_last_update = get_timestamp_ns();

    /* Apply configuration if provided */
    if (config != NULL) {
        memcpy(&gate->config, config, sizeof(autonomic_gate_config_t));
    } else {
        /* Default configuration */
        gate->config.enable_homeostasis = true;
        gate->config.homeostasis_strength = 0.1f;
        gate->config.enable_vital_monitoring = true;
        gate->config.vital_update_interval_ms = 100.0f;
        gate->config.vital_callback = NULL;
        gate->config.homeostasis_callback = NULL;
        gate->config.callback_user_data = NULL;
        gate->config.log_all_releases = false;
        gate->config.strict_mode = false;
    }

    /* Initialize statistics */
    memset(&gate->stats, 0, sizeof(autonomic_gate_stats_t));

    return gate;
}

void autonomic_gate_destroy(autonomic_gate_t* gate) {
    if (gate == NULL) {
        return;
    }
    if (gate->magic != NIMCP_AUTONOMIC_GATE_MAGIC) {
        return;  /* Invalid gate */
    }

    gate->magic = 0;  /* Invalidate before free */
    nimcp_free(gate);
}

autonomic_release_result_t autonomic_gate_release_hormone(
    autonomic_gate_t* gate,
    const autonomic_release_request_t* request,
    autonomic_release_details_t* details
) {
    /* Validate inputs */
    if (!validate_gate(gate)) {
        fill_release_details(details, AUTONOMIC_RELEASE_ERROR, 0, 0, 0, 0,
                            "Invalid gate");
        return AUTONOMIC_RELEASE_ERROR;
    }

    if (request == NULL) {
        fill_release_details(details, AUTONOMIC_RELEASE_INVALID, 0, 0, 0, 0,
                            "NULL request");
        gate->stats.release_requests++;
        gate->stats.releases_blocked++;
        return AUTONOMIC_RELEASE_INVALID;
    }

    gate->stats.release_requests++;

    /* Check if gate is enabled */
    if (!gate->enabled) {
        fill_release_details(details, AUTONOMIC_RELEASE_GATE_DISABLED,
                            request->hormone, request->amount, 0, 0,
                            "Autonomic gate is disabled");
        gate->stats.releases_blocked++;
        return AUTONOMIC_RELEASE_GATE_DISABLED;
    }

    /* Validate hormone type */
    if (!validate_hormone(request->hormone)) {
        fill_release_details(details, AUTONOMIC_RELEASE_INVALID,
                            request->hormone, request->amount, 0, 0,
                            "Invalid hormone type");
        gate->stats.releases_blocked++;
        return AUTONOMIC_RELEASE_INVALID;
    }

    /* Check hormone lock */
    if (gate->hormone_locked[request->hormone]) {
        fill_release_details(details, AUTONOMIC_RELEASE_HORMONE_LOCKED,
                            request->hormone, request->amount, 0,
                            gate->hormone_levels[request->hormone],
                            "Hormone is locked");
        gate->stats.releases_blocked++;
        return AUTONOMIC_RELEASE_HORMONE_LOCKED;
    }

    /* Check hormone enabled */
    const autonomic_hormone_limits_t* limits = &gate->hormone_limits[request->hormone];
    if (!limits->enabled) {
        fill_release_details(details, AUTONOMIC_RELEASE_HORMONE_LOCKED,
                            request->hormone, request->amount, 0,
                            gate->hormone_levels[request->hormone],
                            "Hormone is disabled");
        gate->stats.releases_blocked++;
        return AUTONOMIC_RELEASE_HORMONE_LOCKED;
    }

    /* Calculate actual release amount */
    float requested_amount = request->amount;
    float actual_amount = requested_amount;
    float current_level = gate->hormone_levels[request->hormone];
    autonomic_release_result_t result = AUTONOMIC_RELEASE_SUCCESS;

    /* Rate limit the release */
    float max_rate = (requested_amount > 0) ?
                     limits->max_release_rate : limits->max_absorption_rate;

    if (fabsf(requested_amount) > max_rate) {
        actual_amount = (requested_amount > 0) ? max_rate : -max_rate;
        result = AUTONOMIC_RELEASE_RATE_LIMITED;
        gate->stats.releases_clamped++;
    }

    /* Calculate new level and clamp to bounds */
    float new_level = current_level + actual_amount;

    /* Strict mode: block instead of clamping if would exceed bounds */
    if (gate->config.strict_mode) {
        if (new_level < limits->min_level || new_level > limits->max_level) {
            fill_release_details(details, AUTONOMIC_RELEASE_BLOCKED,
                                request->hormone, requested_amount, 0, current_level,
                                "Would exceed bounds (strict mode)");
            gate->stats.releases_blocked++;
            return AUTONOMIC_RELEASE_BLOCKED;
        }
    }

    /* Clamp to limits */
    float clamped_level = nimcp_clampf(new_level, limits->min_level, limits->max_level);
    if (clamped_level != new_level) {
        actual_amount = clamped_level - current_level;
        if (result == AUTONOMIC_RELEASE_SUCCESS) {
            result = AUTONOMIC_RELEASE_CLAMPED;
        }
        gate->stats.releases_clamped++;
    }

    /* Apply the release */
    gate->hormone_levels[request->hormone] = clamped_level;
    gate->hormone_rates[request->hormone] = actual_amount;
    gate->hormone_last_update[request->hormone] = get_timestamp_ns();

    /* Update statistics */
    if (result == AUTONOMIC_RELEASE_SUCCESS) {
        gate->stats.releases_approved++;
    }

    /* Calculate running averages */
    gate->stats.avg_dopamine_level =
        gate->stats.avg_dopamine_level * 0.95f +
        gate->hormone_levels[HORMONE_DOPAMINE] * 0.05f;

    gate->stats.avg_stress_level =
        gate->stats.avg_stress_level * 0.95f +
        gate->vital_values[VITAL_STRESS_LEVEL] * 0.05f;

    /* Fill result details */
    char desc[NIMCP_ERROR_BUFFER_SIZE];
    if (result == AUTONOMIC_RELEASE_SUCCESS) {
        snprintf(desc, sizeof(desc), "Released %.3f %s",
                actual_amount, autonomic_hormone_name(request->hormone));
    } else if (result == AUTONOMIC_RELEASE_CLAMPED) {
        snprintf(desc, sizeof(desc), "Clamped from %.3f to %.3f",
                requested_amount, actual_amount);
    } else {
        snprintf(desc, sizeof(desc), "Rate limited from %.3f to %.3f",
                requested_amount, actual_amount);
    }

    fill_release_details(details, result, request->hormone,
                        requested_amount, actual_amount, clamped_level, desc);

    return result;
}

nimcp_result_t autonomic_gate_set_hormone_limits(
    autonomic_gate_t* gate,
    autonomic_hormone_t hormone,
    const autonomic_hormone_limits_t* limits
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_hormone(hormone), NIMCP_ERROR_INVALID_PARAM, "invalid hormone type");
    NIMCP_CHECK_THROW(limits, NIMCP_ERROR_NULL_POINTER, "limits is NULL");

    /* Validate limit values */
    NIMCP_CHECK_THROW(limits->min_level <= limits->max_level &&
                      limits->max_release_rate >= 0 &&
                      limits->max_absorption_rate >= 0, NIMCP_ERROR_INVALID_PARAM, "invalid limit values");

    memcpy(&gate->hormone_limits[hormone], limits, sizeof(autonomic_hormone_limits_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_get_hormone_limits(
    const autonomic_gate_t* gate,
    autonomic_hormone_t hormone,
    autonomic_hormone_limits_t* limits
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_hormone(hormone), NIMCP_ERROR_INVALID_PARAM, "invalid hormone type");
    NIMCP_CHECK_THROW(limits, NIMCP_ERROR_NULL_POINTER, "limits is NULL");

    memcpy(limits, &gate->hormone_limits[hormone], sizeof(autonomic_hormone_limits_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_set_vital_bounds(
    autonomic_gate_t* gate,
    autonomic_vital_t vital,
    const autonomic_vital_bounds_t* bounds
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_vital(vital), NIMCP_ERROR_INVALID_PARAM, "invalid vital type");
    NIMCP_CHECK_THROW(bounds, NIMCP_ERROR_NULL_POINTER, "bounds is NULL");

    /* Validate bound values */
    NIMCP_CHECK_THROW(bounds->critical_min <= bounds->normal_min &&
                      bounds->normal_min <= bounds->normal_max &&
                      bounds->normal_max <= bounds->critical_max, NIMCP_ERROR_INVALID_PARAM, "invalid bound ordering");

    memcpy(&gate->vital_bounds[vital], bounds, sizeof(autonomic_vital_bounds_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_get_vital_bounds(
    const autonomic_gate_t* gate,
    autonomic_vital_t vital,
    autonomic_vital_bounds_t* bounds
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_vital(vital), NIMCP_ERROR_INVALID_PARAM, "invalid vital type");
    NIMCP_CHECK_THROW(bounds, NIMCP_ERROR_NULL_POINTER, "bounds is NULL");

    memcpy(bounds, &gate->vital_bounds[vital], sizeof(autonomic_vital_bounds_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_get_hormone_state(
    const autonomic_gate_t* gate,
    autonomic_hormone_state_t* state
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    memcpy(state->levels, gate->hormone_levels, sizeof(state->levels));
    memcpy(state->rates, gate->hormone_rates, sizeof(state->rates));
    memcpy(state->last_update, gate->hormone_last_update, sizeof(state->last_update));
    memcpy(state->locked, gate->hormone_locked, sizeof(state->locked));

    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_read_vitals(
    autonomic_gate_t* gate,
    autonomic_vitals_reading_t* reading
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(reading, NIMCP_ERROR_NULL_POINTER, "reading is NULL");

    gate->stats.vital_readings++;

    /* Copy current values */
    memcpy(reading->values, gate->vital_values, sizeof(reading->values));

    /* Calculate status for each vital */
    reading->any_critical = false;
    reading->critical_count = 0;

    for (int i = 0; i < VITAL_COUNT; i++) {
        reading->status[i] = get_vital_status(gate->vital_values[i],
                                               &gate->vital_bounds[i]);
        if (reading->status[i] == VITAL_STATUS_CRITICAL_LOW ||
            reading->status[i] == VITAL_STATUS_CRITICAL_HIGH) {
            reading->any_critical = true;
            reading->critical_count++;

            /* Trigger callback if configured */
            if (gate->config.vital_callback != NULL) {
                gate->config.vital_callback((autonomic_vital_t)i,
                                             reading->status[i],
                                             gate->vital_values[i],
                                             gate->config.callback_user_data);
            }

            gate->stats.vital_alerts++;
            gate->stats.critical_events++;
        }
    }

    reading->timestamp = get_timestamp_ns();
    gate->vitals_last_update = reading->timestamp;

    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_update_vital(
    autonomic_gate_t* gate,
    autonomic_vital_t vital,
    float value
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_vital(vital), NIMCP_ERROR_INVALID_PARAM, "invalid vital type");

    const autonomic_vital_bounds_t* bounds = &gate->vital_bounds[vital];
    if (!bounds->monitoring_enabled) {
        return NIMCP_SUCCESS;  /* Silently ignore if monitoring disabled */
    }

    /* Rate limit the change */
    float current = gate->vital_values[vital];
    float delta = value - current;
    float max_delta = bounds->max_rate_of_change;

    if (fabsf(delta) > max_delta) {
        delta = (delta > 0) ? max_delta : -max_delta;
        value = current + delta;
    }

    gate->vital_values[vital] = value;

    /* Check for alerts */
    autonomic_vital_status_t status = get_vital_status(value, bounds);
    if (status == VITAL_STATUS_CRITICAL_LOW || status == VITAL_STATUS_CRITICAL_HIGH) {
        gate->stats.vital_alerts++;
        gate->stats.critical_events++;

        if (gate->config.vital_callback != NULL) {
            gate->config.vital_callback(vital, status, value,
                                         gate->config.callback_user_data);
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_lock_hormone(
    autonomic_gate_t* gate,
    autonomic_hormone_t hormone
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_hormone(hormone), NIMCP_ERROR_INVALID_PARAM, "invalid hormone type");

    gate->hormone_locked[hormone] = true;
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_unlock_hormone(
    autonomic_gate_t* gate,
    autonomic_hormone_t hormone
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(validate_hormone(hormone), NIMCP_ERROR_INVALID_PARAM, "invalid hormone type");

    gate->hormone_locked[hormone] = false;
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_trigger_homeostasis(autonomic_gate_t* gate) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");

    if (!gate->config.enable_homeostasis) {
        return NIMCP_SUCCESS;  /* Homeostasis not enabled */
    }

    float strength = gate->config.homeostasis_strength;

    for (int i = 0; i < NIMCP_AUTONOMIC_HORMONE_COUNT; i++) {
        if (gate->hormone_locked[i]) {
            continue;  /* Skip locked hormones */
        }

        const autonomic_hormone_limits_t* limits = &gate->hormone_limits[i];
        if (!limits->enabled) {
            continue;
        }

        float current = gate->hormone_levels[i];
        float baseline = limits->target_baseline;
        float deviation = current - baseline;

        if (fabsf(deviation) > 0.01f) {  /* Only correct if significant deviation */
            float correction = -deviation * strength;

            /* Clamp correction to max rate */
            float max_rate = (correction > 0) ?
                            limits->max_release_rate : limits->max_absorption_rate;
            if (fabsf(correction) > max_rate) {
                correction = (correction > 0) ? max_rate : -max_rate;
            }

            float old_level = current;
            float new_level = nimcp_clampf(current + correction,
                                          limits->min_level, limits->max_level);

            gate->hormone_levels[i] = new_level;
            gate->stats.homeostasis_corrections++;

            /* Notify via callback if configured */
            if (gate->config.homeostasis_callback != NULL) {
                gate->config.homeostasis_callback((autonomic_hormone_t)i,
                                                   old_level, new_level,
                                                   gate->config.callback_user_data);
            }
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_reset_to_baseline(autonomic_gate_t* gate) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");

    for (int i = 0; i < NIMCP_AUTONOMIC_HORMONE_COUNT; i++) {
        float old_level = gate->hormone_levels[i];
        float baseline = gate->hormone_limits[i].target_baseline;

        gate->hormone_levels[i] = baseline;
        gate->hormone_rates[i] = 0.0f;
        gate->hormone_last_update[i] = get_timestamp_ns();

        /* Notify via callback if configured */
        if (gate->config.homeostasis_callback != NULL && old_level != baseline) {
            gate->config.homeostasis_callback((autonomic_hormone_t)i,
                                               old_level, baseline,
                                               gate->config.callback_user_data);
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_get_stats(
    const autonomic_gate_t* gate,
    autonomic_gate_stats_t* stats
) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    memcpy(stats, &gate->stats, sizeof(autonomic_gate_stats_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t autonomic_gate_reset_stats(autonomic_gate_t* gate) {
    NIMCP_CHECK_THROW(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "invalid autonomic gate");

    memset(&gate->stats, 0, sizeof(autonomic_gate_stats_t));
    return NIMCP_SUCCESS;
}

const char* autonomic_hormone_name(autonomic_hormone_t hormone) {
    switch (hormone) {
        case HORMONE_DOPAMINE:       return "DOPAMINE";
        case HORMONE_SEROTONIN:      return "SEROTONIN";
        case HORMONE_NOREPINEPHRINE: return "NOREPINEPHRINE";
        case HORMONE_CORTISOL:       return "CORTISOL";
        case HORMONE_OXYTOCIN:       return "OXYTOCIN";
        case HORMONE_MELATONIN:      return "MELATONIN";
        case HORMONE_ACETYLCHOLINE:  return "ACETYLCHOLINE";
        case HORMONE_GABA:           return "GABA";
        default:                     return "UNKNOWN";
    }
}

const char* autonomic_vital_name(autonomic_vital_t vital) {
    switch (vital) {
        case VITAL_HEART_RATE:         return "HEART_RATE";
        case VITAL_TEMPERATURE:        return "TEMPERATURE";
        case VITAL_RESPIRATION:        return "RESPIRATION";
        case VITAL_BLOOD_PRESSURE_SYS: return "BLOOD_PRESSURE_SYS";
        case VITAL_BLOOD_PRESSURE_DIA: return "BLOOD_PRESSURE_DIA";
        case VITAL_STRESS_LEVEL:       return "STRESS_LEVEL";
        case VITAL_ENERGY_LEVEL:       return "ENERGY_LEVEL";
        default:                       return "UNKNOWN";
    }
}

const char* autonomic_release_result_name(autonomic_release_result_t result) {
    switch (result) {
        case AUTONOMIC_RELEASE_SUCCESS:        return "SUCCESS";
        case AUTONOMIC_RELEASE_CLAMPED:        return "CLAMPED";
        case AUTONOMIC_RELEASE_RATE_LIMITED:   return "RATE_LIMITED";
        case AUTONOMIC_RELEASE_BLOCKED:        return "BLOCKED";
        case AUTONOMIC_RELEASE_HORMONE_LOCKED: return "HORMONE_LOCKED";
        case AUTONOMIC_RELEASE_GATE_DISABLED:  return "GATE_DISABLED";
        case AUTONOMIC_RELEASE_INVALID:        return "INVALID";
        case AUTONOMIC_RELEASE_ERROR:          return "ERROR";
        default:                               return "UNKNOWN";
    }
}

const char* autonomic_vital_status_name(autonomic_vital_status_t status) {
    switch (status) {
        case VITAL_STATUS_NORMAL:        return "NORMAL";
        case VITAL_STATUS_LOW:           return "LOW";
        case VITAL_STATUS_HIGH:          return "HIGH";
        case VITAL_STATUS_CRITICAL_LOW:  return "CRITICAL_LOW";
        case VITAL_STATUS_CRITICAL_HIGH: return "CRITICAL_HIGH";
        default:                         return "UNKNOWN";
    }
}
