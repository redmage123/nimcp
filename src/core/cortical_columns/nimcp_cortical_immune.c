/**
 * @file nimcp_cortical_immune.c
 * @brief Cortical Immune Integration Implementation
 */

#include "core/cortical_columns/nimcp_cortical_immune.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_immune)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal cortical immune system state
 */
struct cortical_immune_system {
    cortical_immune_config_t config;

    /* Microglial surveillance */
    microglial_site_t* microglial_sites;
    size_t num_sites;
    size_t site_capacity;

    /* Column tracking */
    cortical_column_immune_t* column_states;
    size_t num_columns;
    size_t column_capacity;

    /* Layer tracking */
    layer_immune_state_t* layer_states;
    size_t num_layers;
    size_t layer_capacity;

    /* Brain immune integration */
    brain_immune_system_t* brain_immune;
    bool brain_immune_connected;

    /* Statistics */
    cortical_immune_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;

    /* State */
    uint64_t start_time;
    uint32_t next_site_id;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute Euclidean distance between cortical positions
 * WHY:  Determine if column is within microglial surveillance radius
 * HOW:  Standard distance formula
 */
static float compute_cortical_distance(
    float x1, float y1,
    float x2, float y2
) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

/**
 * WHAT: Find microglial site nearest to cortical position
 * WHY:  Assign columns to monitoring sites
 * HOW:  Linear search for minimum distance
 */
static uint32_t find_nearest_microglial_site(
    cortical_immune_system_t* system,
    float x, float y
) {
    if (!system || system->num_sites == 0) {
        return UINT32_MAX;
    }

    uint32_t nearest_id = UINT32_MAX;
    float min_distance = INFINITY;

    for (size_t i = 0; i < system->num_sites; i++) {
        microglial_site_t* site = &system->microglial_sites[i];
        float dist = compute_cortical_distance(x, y, site->cortical_x, site->cortical_y);

        if (dist < min_distance) {
            min_distance = dist;
            nearest_id = site->site_id;
        }
    }

    return nearest_id;
}

/**
 * WHAT: Compute abnormality score from activity statistics
 * WHY:  Quantify deviation from healthy baseline
 * HOW:  Weighted combination of deviation, variance, silence
 */
static float compute_abnormality_score(
    float baseline, float current, float variance,
    float hyper_threshold, float hypo_threshold
) {
    float deviation = fabsf(current - baseline);
    float normalized_deviation = deviation / (baseline + 1e-6f);

    /* Check for hyperexcitability */
    float hyper_score = 0.0f;
    if (current > baseline + hyper_threshold) {
        hyper_score = fminf(1.0f, normalized_deviation);
    }

    /* Check for hypoactivity */
    float hypo_score = 0.0f;
    if (current < baseline - hypo_threshold) {
        hypo_score = fminf(1.0f, normalized_deviation);
    }

    /* Combine scores */
    return fmaxf(hyper_score, hypo_score);
}

/**
 * WHAT: Create epitope from cortical activity pattern
 * WHY:  Convert abnormality to immune-recognizable signature
 * HOW:  Hash activity pattern into byte sequence
 */
static void create_cortical_epitope(
    cortical_abnormality_type_t type,
    uint32_t column_id,
    float abnormality_score,
    uint8_t* epitope,
    size_t* epitope_len
) {
    if (!epitope || !epitope_len) return;

    /* Simple epitope: type | column_id | quantized_score */
    size_t idx = 0;

    /* Abnormality type (1 byte) */
    epitope[idx++] = (uint8_t)type;

    /* Column ID (4 bytes) */
    epitope[idx++] = (column_id >> 24) & 0xFF;
    epitope[idx++] = (column_id >> 16) & 0xFF;
    epitope[idx++] = (column_id >> 8) & 0xFF;
    epitope[idx++] = column_id & 0xFF;

    /* Quantized score (1 byte) */
    epitope[idx++] = (uint8_t)(abnormality_score * 255.0f);

    *epitope_len = idx;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int cortical_immune_default_config(cortical_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_default_config: config is NULL");
        return -1;
    }

    config->max_microglial_sites = CORTICAL_IMMUNE_MAX_MICROGLIAL_SITES;
    config->microglial_density = 50.0f;  /* 50 sites/mm² */
    config->surveillance_radius = CORTICAL_IMMUNE_SURVEILLANCE_RADIUS;
    config->surveillance_interval_ms = 100;  /* 100ms surveillance */

    config->hyperexcitability_threshold = 0.5f;
    config->hypoactivity_threshold = 0.3f;
    config->synchronization_threshold = 0.7f;
    config->feature_loss_threshold = 0.4f;

    config->inflammation_gain_impact = 0.5f;  /* 50% gain reduction */
    config->cytokine_diffusion_rate = 0.1f;
    config->inflammation_decay_rate = 0.01f;

    config->enable_immune_integration = true;
    config->enable_antigen_presentation = true;
    config->enable_cytokine_modulation = true;
    config->enable_recovery_tracking = true;

    return 0;
}

cortical_immune_system_t* cortical_immune_create(
    const cortical_immune_config_t* config
) {
    cortical_immune_system_t* system = (cortical_immune_system_t*)
        nimcp_malloc(sizeof(cortical_immune_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_immune_create: system is NULL");
        return NULL;
    }

    memset(system, 0, sizeof(cortical_immune_system_t));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        cortical_immune_default_config(&system->config);
    }

    /* Allocate microglial sites */
    system->site_capacity = system->config.max_microglial_sites;
    system->microglial_sites = (microglial_site_t*)
        nimcp_malloc(sizeof(microglial_site_t) * system->site_capacity);
    if (!system->microglial_sites) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_immune_create: system->microglial_sites is NULL");
        goto cleanup;
    }
    memset(system->microglial_sites, 0, sizeof(microglial_site_t) * system->site_capacity);

    /* Allocate column states */
    system->column_capacity = CORTICAL_IMMUNE_MAX_COLUMNS;
    system->column_states = (cortical_column_immune_t*)
        nimcp_malloc(sizeof(cortical_column_immune_t) * system->column_capacity);
    if (!system->column_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_immune_create: system->column_states is NULL");
        goto cleanup;
    }
    memset(system->column_states, 0, sizeof(cortical_column_immune_t) * system->column_capacity);

    /* Allocate layer states */
    system->layer_capacity = CORTICAL_IMMUNE_MAX_LAYERS * 16;  /* 16 regions * 6 layers */
    system->layer_states = (layer_immune_state_t*)
        nimcp_malloc(sizeof(layer_immune_state_t) * system->layer_capacity);
    if (!system->layer_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_immune_create: system->layer_states is NULL");
        goto cleanup;
    }
    memset(system->layer_states, 0, sizeof(layer_immune_state_t) * system->layer_capacity);

    /* Create mutex */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_immune_create: mutex creation failed");
        goto cleanup;
    }

    /* Initialize state */
    system->start_time = nimcp_platform_time_monotonic_us();
    system->next_site_id = 1;
    system->brain_immune_connected = false;

    LOG_MODULE_INFO("cortical_immune",
                  "Created cortical immune system with %u max sites",
              system->config.max_microglial_sites);

    return system;

cleanup:
    nimcp_free(system->layer_states);
    nimcp_free(system->column_states);
    nimcp_free(system->microglial_sites);
    nimcp_free(system);
    return NULL;
}

void cortical_immune_destroy(cortical_immune_system_t* system) {
    if (!system) return;

    /* Free microglial sites */
    if (system->microglial_sites) {
        for (size_t i = 0; i < system->num_sites; i++) {
            if (system->microglial_sites[i].monitored_column_ids) {
                nimcp_free(system->microglial_sites[i].monitored_column_ids);
            }
        }
        nimcp_free(system->microglial_sites);
    }

    /* Free column states */
    if (system->column_states) {
        nimcp_free(system->column_states);
    }

    /* Free layer states */
    if (system->layer_states) {
        nimcp_free(system->layer_states);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int cortical_immune_connect_brain_immune(
    cortical_immune_system_t* system,
    brain_immune_system_t* brain_immune
) {
    if (!system || !brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_connect_brain_immune: required parameter is NULL (system, brain_immune)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->brain_immune = brain_immune;
    system->brain_immune_connected = true;
    nimcp_platform_mutex_unlock(system->mutex);

    LOG_MODULE_INFO("cortical_immune",
                  "Connected to brain immune system");

    return 0;
}

int cortical_immune_register_minicolumn(
    cortical_immune_system_t* system,
    minicolumn_t* column,
    uint32_t column_id
) {
    if (!system || !column) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_register_minicolumn: required parameter is NULL (system, column)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    if (system->num_columns >= system->column_capacity) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cortical_immune_register_minicolumn: capacity exceeded");
        return -1;
    }

    /* Initialize column immune state */
    cortical_column_immune_t* state = &system->column_states[system->num_columns];
    state->column_id = column_id;
    state->inflammation_level = 0.0f;
    state->inflammation_severity = INFLAMMATION_NONE;
    state->gain_modulation = 1.0f;
    state->inhibition_modulation = 1.0f;
    state->connectivity_modulation = 1.0f;
    state->selectivity_modulation = 1.0f;
    state->baseline_activation = 0.1f;  /* Will be calibrated */
    state->current_activation = 0.0f;
    state->activation_variance = 0.0f;
    state->linked_microglial_site = UINT32_MAX;
    state->immune_activations = 0;
    state->last_immune_event = 0;

    system->num_columns++;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int cortical_immune_register_hypercolumn(
    cortical_immune_system_t* system,
    hypercolumn_t* hcol,
    uint32_t hcol_id
) {
    if (!system || !hcol) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_register_hypercolumn: required parameter is NULL (system, hcol)");
        return -1;
    }

    /* For now, just log registration */
    LOG_MODULE_DEBUG("cortical_immune",
                  "Registered hypercolumn %u for monitoring", hcol_id);

    return 0;
}

int cortical_immune_register_laminar_structure(
    cortical_immune_system_t* system,
    laminar_structure_t* layers,
    uint32_t region_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_register_laminar_structure: system is NULL");
        return -1;
    }
    /* layers may be NULL - register with default layer states */

    nimcp_platform_mutex_lock(system->mutex);

    /* Initialize layer states for this region */
    for (int layer = 0; layer < CC_LAYER_COUNT; layer++) {
        if (system->num_layers >= system->layer_capacity) {
            break;
        }

        layer_immune_state_t* state = &system->layer_states[system->num_layers];
        state->layer = (cc_cortical_layer_t)layer;
        state->il1_concentration = 0.0f;
        state->il6_concentration = 0.0f;
        state->tnf_concentration = 0.0f;
        state->il10_concentration = 0.0f;
        state->feedforward_gain = 1.0f;
        state->feedback_gain = 1.0f;
        state->lateral_gain = 1.0f;
        state->excitability = 1.0f;
        state->mean_activation = 0.0f;
        state->activation_stability = 1.0f;
        state->is_dysfunctional = false;

        system->num_layers++;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    LOG_MODULE_DEBUG("cortical_immune",
                  "Registered laminar structure region %u", region_id);

    return 0;
}

int cortical_immune_register_orientation_hypercolumn(
    cortical_immune_system_t* system,
    orientation_hypercolumn_t* orient_hcol,
    uint32_t hcol_id
) {
    if (!system || !orient_hcol) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_register_orientation_hypercolumn: required parameter is NULL (system, orient_hcol)");
        return -1;
    }

    LOG_MODULE_DEBUG("cortical_immune",
                  "Registered orientation hypercolumn %u for selectivity monitoring",
              hcol_id);

    return 0;
}

/* ============================================================================
 * Microglial Surveillance Implementation
 * ============================================================================ */

int cortical_immune_create_microglial_site(
    cortical_immune_system_t* system,
    float cortical_x,
    float cortical_y,
    float radius,
    uint32_t* site_id
) {
    if (!system || !site_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_create_microglial_site: required parameter is NULL (system, site_id)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    if (system->num_sites >= system->site_capacity) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cortical_immune_create_microglial_site: capacity exceeded");
        return -1;
    }

    microglial_site_t* site = &system->microglial_sites[system->num_sites];
    site->site_id = system->next_site_id++;
    site->state = MICROGLIA_RESTING;
    site->cortical_x = cortical_x;
    site->cortical_y = cortical_y;
    site->surveillance_radius = radius;
    site->activation_level = 0.0f;
    site->cytokine_concentration = 0.0f;
    site->last_activation_time = 0;
    site->monitored_column_ids = NULL;
    site->num_monitored_columns = 0;
    site->abnormality_score = 0.0f;
    site->detected_type = ABNORMALITY_HYPEREXCITABILITY;
    site->detection_count = 0;

    *site_id = site->site_id;
    system->num_sites++;
    system->stats.total_microglial_sites++;
    system->stats.resting_microglia++;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

uint32_t cortical_immune_update_surveillance(
    cortical_immune_system_t* system,
    uint64_t delta_ms
) {
    if (!system) return 0;

    uint32_t new_abnormalities = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Check each column for abnormalities */
    for (size_t i = 0; i < system->num_columns; i++) {
        cortical_column_immune_t* col = &system->column_states[i];

        /* Detect hyperexcitability */
        float hyper = cortical_immune_detect_hyperexcitability(
            system, col->column_id, col->current_activation);

        if (hyper > system->config.hyperexcitability_threshold) {
            new_abnormalities++;
            system->stats.hyperexcitability_events++;
            system->stats.total_abnormalities_detected++;
        }

        /* Detect hypoactivity */
        float hypo = cortical_immune_detect_hypoactivity(
            system, col->column_id, col->current_activation);

        if (hypo > system->config.hypoactivity_threshold) {
            new_abnormalities++;
            system->stats.hypoactivity_events++;
            system->stats.total_abnormalities_detected++;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return new_abnormalities;
}

int cortical_immune_activate_microglia(
    cortical_immune_system_t* system,
    uint32_t site_id,
    cortical_abnormality_type_t abnormality_type
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_activate_microglia: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find site */
    microglial_site_t* site = NULL;
    for (size_t i = 0; i < system->num_sites; i++) {
        if (system->microglial_sites[i].site_id == site_id) {
            site = &system->microglial_sites[i];
            break;
        }
    }

    if (!site) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_activate_microglia: site is NULL");
        return -1;
    }

    /* Activate microglia */
    if (site->state == MICROGLIA_RESTING) {
        system->stats.resting_microglia--;
    }
    site->state = MICROGLIA_ACTIVATED;
    site->activation_level = 1.0f;
    site->detected_type = abnormality_type;
    site->detection_count++;
    site->last_activation_time = nimcp_platform_time_monotonic_us();
    system->stats.activated_microglia++;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Abnormality Detection Implementation
 * ============================================================================ */

float cortical_immune_detect_hyperexcitability(
    cortical_immune_system_t* system,
    uint32_t column_id,
    float activation
) {
    if (!system) return 0.0f;

    /* Find column state */
    cortical_column_immune_t* col = NULL;
    for (size_t i = 0; i < system->num_columns; i++) {
        if (system->column_states[i].column_id == column_id) {
            col = &system->column_states[i];
            break;
        }
    }

    if (!col) return 0.0f;

    return compute_abnormality_score(
        col->baseline_activation,
        activation,
        col->activation_variance,
        system->config.hyperexcitability_threshold,
        system->config.hypoactivity_threshold
    );
}

float cortical_immune_detect_hypoactivity(
    cortical_immune_system_t* system,
    uint32_t column_id,
    float activation
) {
    if (!system) return 0.0f;

    cortical_column_immune_t* col = NULL;
    for (size_t i = 0; i < system->num_columns; i++) {
        if (system->column_states[i].column_id == column_id) {
            col = &system->column_states[i];
            break;
        }
    }

    if (!col) return 0.0f;

    /* Hypoactivity score */
    if (activation < col->baseline_activation - system->config.hypoactivity_threshold) {
        float deficit = col->baseline_activation - activation;
        return fminf(1.0f, deficit / (col->baseline_activation + 1e-6f));
    }

    return 0.0f;
}

float cortical_immune_detect_synchronization(
    cortical_immune_system_t* system,
    const uint32_t* column_ids,
    const float* activations,
    uint32_t num_columns
) {
    if (!system || !column_ids || !activations || num_columns < 2) {
        return 0.0f;
    }

    /* Compute mean activation */
    float mean = 0.0f;
    for (uint32_t i = 0; i < num_columns; i++) {
        mean += activations[i];
    }
    mean /= num_columns;

    /* Compute variance */
    float variance = 0.0f;
    for (uint32_t i = 0; i < num_columns; i++) {
        float diff = activations[i] - mean;
        variance += diff * diff;
    }
    variance /= num_columns;

    /* Low variance = high synchronization */
    float sync_score = 1.0f - fminf(1.0f, variance / 0.1f);

    return sync_score;
}

float cortical_immune_detect_layer_dysfunction(
    cortical_immune_system_t* system,
    uint32_t region_id,
    cc_cortical_layer_t layer
) {
    if (!system) return 0.0f;

    /* Find layer state */
    layer_immune_state_t* state = NULL;
    for (size_t i = 0; i < system->num_layers; i++) {
        if (system->layer_states[i].layer == layer) {
            state = &system->layer_states[i];
            break;
        }
    }

    if (!state) return 0.0f;

    /* Compute dysfunction score from pathway gains */
    float ff_loss = 1.0f - state->feedforward_gain;
    float fb_loss = 1.0f - state->feedback_gain;
    float lateral_loss = 1.0f - state->lateral_gain;

    return (ff_loss + fb_loss + lateral_loss) / 3.0f;
}

float cortical_immune_detect_feature_loss(
    cortical_immune_system_t* system,
    uint32_t hcol_id,
    float current_osi
) {
    if (!system) return 0.0f;

    /* Healthy OSI should be > 0.5, degraded OSI < 0.3 */
    float baseline_osi = 0.7f;
    float loss = baseline_osi - current_osi;

    return fmaxf(0.0f, fminf(1.0f, loss / baseline_osi));
}

/* ============================================================================
 * Inflammation Implementation (Immune → Cortex)
 * ============================================================================ */

int cortical_immune_apply_inflammation(
    cortical_immune_system_t* system,
    uint32_t column_id,
    float inflammation_level
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_apply_inflammation: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find column, auto-register if not found */
    cortical_column_immune_t* col = NULL;
    for (size_t i = 0; i < system->num_columns; i++) {
        if (system->column_states[i].column_id == column_id) {
            col = &system->column_states[i];
            break;
        }
    }

    if (!col) {
        /* Auto-register column with default immune state */
        if (system->num_columns < system->column_capacity) {
            col = &system->column_states[system->num_columns];
            col->column_id = column_id;
            col->inflammation_level = 0.0f;
            col->inflammation_severity = INFLAMMATION_NONE;
            col->gain_modulation = 1.0f;
            col->inhibition_modulation = 1.0f;
            col->connectivity_modulation = 1.0f;
            col->selectivity_modulation = 1.0f;
            col->baseline_activation = 0.1f;
            col->current_activation = 0.0f;
            col->activation_variance = 0.0f;
            col->linked_microglial_site = UINT32_MAX;
            col->immune_activations = 0;
            col->last_immune_event = 0;
            system->num_columns++;
        } else {
            nimcp_platform_mutex_unlock(system->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cortical_immune_apply_inflammation: column capacity exceeded");
            return -1;
        }
    }

    /* Apply inflammation effects */
    col->inflammation_level = inflammation_level;

    /* Reduce gains based on inflammation */
    float impact = system->config.inflammation_gain_impact;
    col->gain_modulation = 1.0f - (inflammation_level * impact);
    col->inhibition_modulation = 1.0f - (inflammation_level * impact * 0.8f);
    col->connectivity_modulation = 1.0f - (inflammation_level * impact * 0.6f);
    col->selectivity_modulation = 1.0f - (inflammation_level * impact * 0.4f);

    /* Update severity category */
    if (inflammation_level < 0.2f) {
        col->inflammation_severity = INFLAMMATION_NONE;
    } else if (inflammation_level < 0.4f) {
        col->inflammation_severity = INFLAMMATION_LOCAL;
    } else if (inflammation_level < 0.6f) {
        col->inflammation_severity = INFLAMMATION_REGIONAL;
    } else if (inflammation_level < 0.8f) {
        col->inflammation_severity = INFLAMMATION_SYSTEMIC;
    } else {
        col->inflammation_severity = INFLAMMATION_STORM;
    }

    /* Update stats */
    if (inflammation_level > 0.1f) {
        system->stats.inflamed_columns++;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int cortical_immune_apply_cytokine(
    cortical_immune_system_t* system,
    uint32_t region_id,
    cc_cortical_layer_t layer,
    brain_cytokine_type_t cytokine_type,
    float concentration
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_apply_cytokine: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find layer state, auto-register if not found */
    layer_immune_state_t* state = NULL;
    for (size_t i = 0; i < system->num_layers; i++) {
        if (system->layer_states[i].layer == layer) {
            state = &system->layer_states[i];
            break;
        }
    }

    if (!state) {
        /* Auto-register layer with default immune state */
        if (system->num_layers < system->layer_capacity) {
            state = &system->layer_states[system->num_layers];
            state->layer = layer;
            state->il1_concentration = 0.0f;
            state->il6_concentration = 0.0f;
            state->tnf_concentration = 0.0f;
            state->il10_concentration = 0.0f;
            state->feedforward_gain = 1.0f;
            state->feedback_gain = 1.0f;
            state->lateral_gain = 1.0f;
            state->excitability = 1.0f;
            state->mean_activation = 0.0f;
            state->activation_stability = 1.0f;
            state->is_dysfunctional = false;
            system->num_layers++;
        } else {
            nimcp_platform_mutex_unlock(system->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cortical_immune_apply_cytokine: layer capacity exceeded");
            return -1;
        }
    }

    /* Apply cytokine-specific effects */
    switch (cytokine_type) {
        case CYTOKINE_IL1B:
            state->il1_concentration = concentration;
            state->feedforward_gain *= (1.0f - concentration * 0.2f);
            break;

        case CYTOKINE_IL6:
            state->il6_concentration = concentration;
            state->excitability *= (1.0f - concentration * 0.3f);
            break;

        case CYTOKINE_TNFA:
            state->tnf_concentration = concentration;
            state->feedforward_gain *= (1.0f - concentration * 0.5f);
            state->feedback_gain *= (1.0f - concentration * 0.5f);
            state->lateral_gain *= (1.0f - concentration * 0.5f);
            break;

        case CYTOKINE_IL10:
            /* Anti-inflammatory - restore function */
            state->il10_concentration = concentration;
            state->feedforward_gain = fminf(1.0f, state->feedforward_gain + concentration * 0.1f);
            state->feedback_gain = fminf(1.0f, state->feedback_gain + concentration * 0.1f);
            break;

        default:
            break;
    }

    /* Check for dysfunction */
    float avg_gain = (state->feedforward_gain + state->feedback_gain + state->lateral_gain) / 3.0f;
    state->is_dysfunctional = (avg_gain < 0.5f);

    if (state->is_dysfunctional) {
        system->stats.dysfunctional_layers++;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int cortical_immune_update_cytokine_diffusion(
    cortical_immune_system_t* system,
    uint64_t delta_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_update_cytokine_diffusion: system is NULL");
        return -1;
    }

    /* Simple diffusion: cytokines spread from microglial sites */
    float dt = delta_ms / 1000.0f;  /* Convert to seconds */
    float diffusion_rate = system->config.cytokine_diffusion_rate;

    nimcp_platform_mutex_lock(system->mutex);

    /* Diffuse from each active microglial site */
    for (size_t i = 0; i < system->num_sites; i++) {
        microglial_site_t* site = &system->microglial_sites[i];

        if (site->state != MICROGLIA_ACTIVATED && site->state != MICROGLIA_REACTIVE) {
            continue;
        }

        /* Increase local cytokine concentration */
        site->cytokine_concentration += diffusion_rate * dt;
        site->cytokine_concentration = fminf(1.0f, site->cytokine_concentration);
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int cortical_immune_resolve_inflammation(
    cortical_immune_system_t* system,
    uint32_t column_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_resolve_inflammation: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find column */
    cortical_column_immune_t* col = NULL;
    for (size_t i = 0; i < system->num_columns; i++) {
        if (system->column_states[i].column_id == column_id) {
            col = &system->column_states[i];
            break;
        }
    }

    if (!col) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_resolve_inflammation: col is NULL");
        return -1;
    }

    /* Begin resolution */
    float decay = system->config.inflammation_decay_rate;
    col->inflammation_level *= (1.0f - decay);

    /* Restore modulations */
    col->gain_modulation = fminf(1.0f, col->gain_modulation + decay);
    col->inhibition_modulation = fminf(1.0f, col->inhibition_modulation + decay);
    col->connectivity_modulation = fminf(1.0f, col->connectivity_modulation + decay);
    col->selectivity_modulation = fminf(1.0f, col->selectivity_modulation + decay);

    /* Update severity */
    if (col->inflammation_level < 0.1f) {
        col->inflammation_severity = INFLAMMATION_NONE;
        system->stats.resolutions_completed++;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Antigen Presentation Implementation (Cortex → Immune)
 * ============================================================================ */

int cortical_immune_present_abnormality(
    cortical_immune_system_t* system,
    uint32_t column_id,
    cortical_abnormality_type_t abnormality_type,
    float abnormality_score,
    uint32_t* antigen_id
) {
    if (!system || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_present_abnormality: required parameter is NULL (system, antigen_id)");
        return -1;
    }

    if (!system->brain_immune_connected || !system->brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_present_abnormality: required parameter is NULL (system->brain_immune_connected, system->brain_immune)");
        return -1;
    }

    /* Create epitope from abnormality */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t epitope_len = 0;
    create_cortical_epitope(abnormality_type, column_id, abnormality_score,
                           epitope, &epitope_len);

    /* Present to brain immune system */
    uint32_t severity = (uint32_t)(abnormality_score * 10.0f);
    int result = brain_immune_present_antigen(
        system->brain_immune,
        ANTIGEN_SOURCE_MANUAL,  /* Cortical source */
        epitope,
        epitope_len,
        severity,
        column_id,
        antigen_id
    );

    if (result == 0) {
        system->stats.antigens_presented++;

        LOG_MODULE_DEBUG("cortical_immune",
                  "Presented cortical abnormality (type=%d, column=%u, score=%.2f) as antigen %u",
                  abnormality_type, column_id, abnormality_score, *antigen_id);
    }

    return result;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int cortical_immune_get_column_status(
    cortical_immune_system_t* system,
    uint32_t column_id,
    cortical_column_immune_t* status
) {
    if (!system || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_get_column_status: required parameter is NULL (system, status)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->num_columns; i++) {
        if (system->column_states[i].column_id == column_id) {
            *status = system->column_states[i];
            nimcp_platform_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_immune_get_column_status: validation failed");
    return -1;
}

int cortical_immune_get_layer_state(
    cortical_immune_system_t* system,
    uint32_t region_id,
    cc_cortical_layer_t layer,
    layer_immune_state_t* state
) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_get_layer_state: required parameter is NULL (system, state)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->num_layers; i++) {
        if (system->layer_states[i].layer == layer) {
            *state = system->layer_states[i];
            nimcp_platform_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_immune_get_layer_state: validation failed");
    return -1;
}

int cortical_immune_get_microglial_site(
    cortical_immune_system_t* system,
    uint32_t site_id,
    microglial_site_t* site
) {
    if (!system || !site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_get_microglial_site: required parameter is NULL (system, site)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->num_sites; i++) {
        if (system->microglial_sites[i].site_id == site_id) {
            *site = system->microglial_sites[i];
            nimcp_platform_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_immune_get_microglial_site: validation failed");
    return -1;
}

int cortical_immune_get_stats(
    cortical_immune_system_t* system,
    cortical_immune_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    *stats = system->stats;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int cortical_immune_update(
    cortical_immune_system_t* system,
    uint64_t delta_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_immune_update: system is NULL");
        return -1;
    }

    /* Update surveillance */
    cortical_immune_update_surveillance(system, delta_ms);

    /* Update cytokine diffusion */
    cortical_immune_update_cytokine_diffusion(system, delta_ms);

    /* Update inflammation resolution */
    for (size_t i = 0; i < system->num_columns; i++) {
        cortical_column_immune_t* col = &system->column_states[i];
        if (col->inflammation_level > 0.0f) {
            cortical_immune_resolve_inflammation(system, col->column_id);
        }
    }

    return 0;
}

/* ============================================================================
 * String Utilities
 * ============================================================================ */

const char* cortical_immune_microglial_state_to_string(microglial_state_t state) {
    switch (state) {
        case MICROGLIA_RESTING:   return "RESTING";
        case MICROGLIA_ALERT:     return "ALERT";
        case MICROGLIA_ACTIVATED: return "ACTIVATED";
        case MICROGLIA_REACTIVE:  return "REACTIVE";
        case MICROGLIA_RESOLVING: return "RESOLVING";
        default:                  return "UNKNOWN";
    }
}

const char* cortical_immune_abnormality_to_string(cortical_abnormality_type_t type) {
    switch (type) {
        case ABNORMALITY_HYPEREXCITABILITY:  return "HYPEREXCITABILITY";
        case ABNORMALITY_HYPOACTIVITY:       return "HYPOACTIVITY";
        case ABNORMALITY_SYNCHRONIZATION:    return "SYNCHRONIZATION";
        case ABNORMALITY_LAYER_DYSFUNCTION:  return "LAYER_DYSFUNCTION";
        case ABNORMALITY_FEATURE_LOSS:       return "FEATURE_LOSS";
        case ABNORMALITY_COMPETITION_FAILURE: return "COMPETITION_FAILURE";
        default:                             return "UNKNOWN";
    }
}

const char* cortical_immune_effect_to_string(inflammation_effect_t effect) {
    switch (effect) {
        case INFLAMMATION_EFFECT_GAIN_REDUCTION:      return "GAIN_REDUCTION";
        case INFLAMMATION_EFFECT_INHIBITION_LOSS:     return "INHIBITION_LOSS";
        case INFLAMMATION_EFFECT_CONNECTIVITY_LOSS:   return "CONNECTIVITY_LOSS";
        case INFLAMMATION_EFFECT_SELECTIVITY_LOSS:    return "SELECTIVITY_LOSS";
        case INFLAMMATION_EFFECT_NOISE_INCREASE:      return "NOISE_INCREASE";
        default:                                       return "UNKNOWN";
    }
}
