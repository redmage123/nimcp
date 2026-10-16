/**
 * @file nimcp_structural_plasticity.c
 * @brief Structural Plasticity Implementation
 */

#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Forward declaration - defined in nimcp_brain_strategy.c */
extern void set_error(const char* format, ...);

#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(structural_plasticity)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

struct structural_plasticity_system {
    /* Configuration */
    structural_plasticity_config_t config;

    /* Spine tracking */
    synapse_structural_state_t* spines;
    uint32_t num_spines;
    uint32_t max_spines;
    uint32_t next_synapse_id;

    /* Callbacks */
    structural_change_callback_t callback;
    void* callback_user_data;

    /* Statistics */
    uint64_t total_formations;
    uint64_t total_eliminations;
    uint64_t total_stabilizations;
    uint64_t total_potentiations;
    uint64_t total_pruning_starts;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Forward declaration for find_eliminated_slot_locked used by structural_plasticity_form_synapse */
static uint32_t find_eliminated_slot_locked(structural_plasticity_system_t* system);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Find spine by synapse ID
 * WHY:  Need to locate spine in array
 * HOW:  Linear search (could optimize with hash table)
 */
static synapse_structural_state_t* find_spine(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system || !system->spines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_spine: required parameter is NULL (system, system->spines)");
        return NULL;
    }

    for (uint32_t i = 0; i < system->num_spines; i++) {
        if (system->spines[i].synapse_id == synapse_id &&
            system->spines[i].state != SYNAPSE_STATE_ELIMINATED) {
            return &system->spines[i];
        }
    }
    set_error("find_spine: synapse_id %u not found", synapse_id);
    return NULL;
}

/**
 * @brief Find spine by ID including eliminated spines (for state queries)
 */
static synapse_structural_state_t* find_spine_any_state(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system || !system->spines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_spine_any_state: required parameter is NULL (system, system->spines)");
        return NULL;
    }

    for (uint32_t i = 0; i < system->num_spines; i++) {
        if (system->spines[i].synapse_id == synapse_id) {
            return &system->spines[i];
        }
    }
    set_error("find_spine_any_state: synapse_id %u not found", synapse_id);
    return NULL;
}

/* Forward declaration for slot reclamation helper (defined at end of file) */
static uint32_t find_eliminated_slot_locked(structural_plasticity_system_t* system);

/**
 * WHAT: Initialize spine morphology for nascent state
 * WHY:  New spines start as thin, unstable structures
 * HOW:  Set small volume, PSD, high motility
 */
static void init_nascent_morphology(spine_morphology_t* morph) {
    if (!morph) return;

    morph->spine_volume = STRUCTURAL_VOLUME_NASCENT_MIN +
        (STRUCTURAL_VOLUME_NASCENT_MAX - STRUCTURAL_VOLUME_NASCENT_MIN) * 0.5f;
    morph->psd_size = STRUCTURAL_PSD_NASCENT_MIN +
        (STRUCTURAL_PSD_NASCENT_MAX - STRUCTURAL_PSD_NASCENT_MIN) * 0.5f;
    morph->actin_dynamics = STRUCTURAL_ACTIN_GROWTH_RATE;
    morph->spine_stability = STRUCTURAL_STABILITY_NASCENT;
    morph->spine_motility = STRUCTURAL_MOTILITY_NASCENT;
    morph->camkii_concentration = STRUCTURAL_CAMKII_NASCENT;
    morph->ampar_count = STRUCTURAL_AMPAR_NASCENT;
    morph->nmdar_count = STRUCTURAL_NMDAR_NASCENT;
}

/**
 * WHAT: Update morphology to stable state
 * WHY:  Stabilized spines are larger, more stable
 * HOW:  Increase volume, PSD, stability
 */
static void update_to_stable_morphology(spine_morphology_t* morph) {
    if (!morph) return;

    morph->spine_volume = STRUCTURAL_VOLUME_STABLE_MIN +
        (STRUCTURAL_VOLUME_STABLE_MAX - STRUCTURAL_VOLUME_STABLE_MIN) * 0.7f;
    morph->psd_size = STRUCTURAL_PSD_STABLE_MIN +
        (STRUCTURAL_PSD_STABLE_MAX - STRUCTURAL_PSD_STABLE_MIN) * 0.7f;
    morph->actin_dynamics = STRUCTURAL_ACTIN_STABLE;
    morph->spine_stability = STRUCTURAL_STABILITY_STABLE;
    morph->spine_motility = STRUCTURAL_MOTILITY_STABLE;
    morph->camkii_concentration = STRUCTURAL_CAMKII_STABLE;
    morph->ampar_count = STRUCTURAL_AMPAR_STABLE;
    morph->nmdar_count = STRUCTURAL_NMDAR_STABLE;
}

/**
 * WHAT: Update morphology to potentiated state
 * WHY:  LTP enlarges spines
 * HOW:  Maximize volume, PSD, receptors
 */
static void update_to_potentiated_morphology(spine_morphology_t* morph) {
    if (!morph) return;

    morph->spine_volume = STRUCTURAL_VOLUME_POTENTIATED_MIN +
        (STRUCTURAL_VOLUME_POTENTIATED_MAX - STRUCTURAL_VOLUME_POTENTIATED_MIN) * 0.8f;
    morph->psd_size = STRUCTURAL_PSD_POTENTIATED_MIN +
        (STRUCTURAL_PSD_POTENTIATED_MAX - STRUCTURAL_PSD_POTENTIATED_MIN) * 0.8f;
    morph->actin_dynamics = STRUCTURAL_ACTIN_POTENTIATED;
    morph->spine_stability = STRUCTURAL_STABILITY_POTENTIATED;
    morph->spine_motility = STRUCTURAL_MOTILITY_POTENTIATED;
    morph->camkii_concentration = STRUCTURAL_CAMKII_POTENTIATED;
    morph->ampar_count = STRUCTURAL_AMPAR_POTENTIATED;
    morph->nmdar_count = STRUCTURAL_NMDAR_POTENTIATED;
}

/**
 * WHAT: Invoke structural change callback
 * WHY:  Notify listeners of state transitions
 * HOW:  Call registered callback if present
 */
static void invoke_callback(
    structural_plasticity_system_t* system,
    structural_event_t event,
    uint32_t synapse_id,
    synapse_state_t old_state,
    synapse_state_t new_state
) {
    if (!system || !system->callback) {
        return;
    }

    system->callback(event, synapse_id, old_state, new_state,
                    system->callback_user_data);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int structural_plasticity_default_config(structural_plasticity_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "Structural plasticity config is NULL");

    config->formation_threshold_hz = STRUCTURAL_FORMATION_THRESHOLD_MAX;
    config->formation_rate = 0.01f;
    config->maturation_time_sec = STRUCTURAL_MATURATION_TIME_TYPICAL;
    config->stabilization_threshold = 10.0f;
    config->require_sleep_consolidation = true;
    config->pruning_threshold_hz = 0.5f;  /* Lower threshold to avoid premature pruning */
    config->pruning_rate = 0.005f;
    config->inactivity_timeout_sec = 3600.0f * 24.0f;  /* 24 hours */
    config->ltp_potentiation_threshold = 10.0f;
    config->potentiation_decay_rate = 0.001f;
    config->enable_immune_pruning = true;
    config->complement_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->enable_sleep_consolidation = true;
    config->sleep_consolidation_boost = 2.0f;
    config->max_spines = STRUCTURAL_MAX_SPINES;
    config->spine_density_limit = 1000.0f;

    return 0;
}

structural_plasticity_system_t* structural_plasticity_create(
    const structural_plasticity_config_t* config
) {
    structural_plasticity_system_t* system =
        (structural_plasticity_system_t*)nimcp_malloc(sizeof(*system));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_plasticity_create: system allocation failed");
        return NULL;
    }

    memset(system, 0, sizeof(*system));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        structural_plasticity_default_config(&system->config);
    }

    /* Allocate spine array */
    system->max_spines = system->config.max_spines;
    system->spines = (synapse_structural_state_t*)nimcp_malloc(
        sizeof(synapse_structural_state_t) * system->max_spines);
    if (!system->spines) {
        LOG_ERROR("Structural plasticity spine array allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_plasticity_create: spines allocation failed");
        goto cleanup;
    }

    memset(system->spines, 0,
           sizeof(synapse_structural_state_t) * system->max_spines);

    /* Create mutex */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        LOG_ERROR("Structural plasticity mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "structural_plasticity_create: mutex creation failed");
        goto cleanup;
    }

    system->next_synapse_id = 1;

    NIMCP_LOGGING_INFO("Structural plasticity system created");
    return system;

cleanup:
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
    }
    nimcp_free(system->spines);
    nimcp_free(system);
    return NULL;
}

void structural_plasticity_destroy(structural_plasticity_system_t* system) {
    if (!system) return;

    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    if (system->spines) {
        nimcp_free(system->spines);
    }

    nimcp_free(system);
    NIMCP_LOGGING_INFO("Structural plasticity system destroyed");
}

/* ============================================================================
 * Formation and Elimination Implementation
 * ============================================================================ */

int structural_plasticity_form_synapse(
    structural_plasticity_system_t* system,
    uint32_t pre_neuron_id,
    uint32_t post_neuron_id,
    float activity_hz,
    uint32_t* synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_form_synapse: system is NULL");
        return -1;
    }
    if (!synapse_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_form_synapse: synapse_id is NULL");
        return -1;
    }

    /* Validate activity_hz to prevent NaN propagation */
    if (isnan(activity_hz) || isinf(activity_hz)) {
        activity_hz = 0.0f;
    }
    if (activity_hz < 0.0f) activity_hz = 0.0f;

    nimcp_platform_mutex_lock(system->mutex);

    /* BOUNDS VALIDATION: Check spine array limits
     * WHAT: Verify we have room in the spine array
     * WHY:  Prevent array overflow and memory corruption
     * HOW:  Check both num_spines < max_spines and spines array exists
     */
    if (!system->spines) {
        NIMCP_LOGGING_ERROR("Spine array not initialized");
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_form_synapse: system->spines is NULL");
        return -1;
    }

    /* Try to reuse an eliminated slot first to avoid unbounded growth */
    uint32_t slot = find_eliminated_slot_locked(system);
    bool reusing_slot = (slot != (uint32_t)-1);

    if (!reusing_slot) {
        /* No eliminated slot available, use next free slot */
        if (system->num_spines >= system->max_spines) {
            NIMCP_LOGGING_WARN("Spine limit reached, no eliminated slots available");
            nimcp_platform_mutex_unlock(system->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "structural_plasticity_form_synapse: capacity exceeded");
            return -1;
        }
        slot = system->num_spines;
    }

    /* Bounds check (defensive - should not be reachable) */
    if (slot >= system->max_spines) {
        NIMCP_LOGGING_ERROR("Slot index exceeds max_spines");
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "structural_plasticity_form_synapse: capacity exceeded");
        return -1;
    }

    synapse_structural_state_t* spine = &system->spines[slot];

    /* Initialize spine */
    memset(spine, 0, sizeof(*spine));
    spine->synapse_id = system->next_synapse_id++;
    spine->pre_neuron_id = pre_neuron_id;
    spine->post_neuron_id = post_neuron_id;
    spine->state = SYNAPSE_STATE_NASCENT;
    init_nascent_morphology(&spine->morphology);
    spine->recent_activity_hz = activity_hz;
    spine->formation_time = 0;  /* Will be set by caller */
    spine->maturation_progress = 0.0f;
    spine->consolidation_tagged = false;
    spine->complement_tagged = false;
    spine->pruning_urgency = 0.0f;
    spine->formation_events = 1;

    /* Only increment num_spines if using a new slot, not reusing eliminated */
    if (!reusing_slot) {
        system->num_spines++;
    }
    system->total_formations++;
    *synapse_id = spine->synapse_id;

    /* Copy callback data before releasing mutex */
    structural_change_callback_t callback = system->callback;
    void* callback_user_data = system->callback_user_data;
    uint32_t cb_synapse_id = spine->synapse_id;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Notify callback after releasing mutex to avoid deadlock */
    if (callback) {
        callback(STRUCTURAL_EVENT_FORMATION, cb_synapse_id,
                SYNAPSE_STATE_ELIMINATED, SYNAPSE_STATE_NASCENT,
                callback_user_data);
    }

    NIMCP_LOGGING_DEBUG("Formed synapse %u (pre=%u, post=%u)",
                       spine->synapse_id, pre_neuron_id, post_neuron_id);
    return 0;
}

int structural_plasticity_eliminate_synapse(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_eliminate_synapse: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        NIMCP_LOGGING_WARN("Synapse %u not found", synapse_id);
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_eliminate_synapse: spine is NULL");
        return -1;
    }

    synapse_state_t old_state = spine->state;
    spine->state = SYNAPSE_STATE_ELIMINATED;
    system->total_eliminations++;

    /* Copy callback data before releasing mutex */
    structural_change_callback_t callback = system->callback;
    void* callback_user_data = system->callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Notify callback after releasing mutex to avoid deadlock */
    if (callback) {
        callback(STRUCTURAL_EVENT_ELIMINATION, synapse_id,
                old_state, SYNAPSE_STATE_ELIMINATED, callback_user_data);
    }

    NIMCP_LOGGING_DEBUG("Eliminated synapse %u", synapse_id);
    return 0;
}

bool structural_plasticity_should_form(
    const structural_plasticity_system_t* system,
    float activity_hz
) {
    if (!system) {
        return false;
    }
    return activity_hz >= system->config.formation_threshold_hz;
}

bool structural_plasticity_should_prune(
    const structural_plasticity_system_t* system,
    const synapse_structural_state_t* synapse
) {
    if (!system) {
        return false;
    }
    if (!synapse) {
        return false;
    }

    /* Complement-tagged synapses should be pruned */
    if (system->config.enable_immune_pruning && synapse->complement_tagged) {
        return true;
    }

    /* Low activity triggers pruning */
    if (synapse->recent_activity_hz < system->config.pruning_threshold_hz) {
        return true;
    }

    return false;  /* No pruning criteria met - normal behavior */
}

/* ============================================================================
 * Stabilization and Potentiation Implementation
 * ============================================================================ */

int structural_plasticity_stabilize_synapse(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_stabilize_synapse: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_stabilize_synapse: spine is NULL");
        return -1;
    }

    if (spine->state != SYNAPSE_STATE_NASCENT) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "structural_plasticity_stabilize_synapse: validation failed");
        return -1;  /* Can only stabilize nascent spines */
    }

    synapse_state_t old_state = spine->state;
    spine->state = SYNAPSE_STATE_STABLE;
    update_to_stable_morphology(&spine->morphology);
    spine->maturation_progress = 1.0f;
    system->total_stabilizations++;

    /* Copy callback data before releasing mutex */
    structural_change_callback_t callback = system->callback;
    void* callback_user_data = system->callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Notify callback after releasing mutex to avoid deadlock */
    if (callback) {
        callback(STRUCTURAL_EVENT_STABILIZATION, synapse_id,
                old_state, SYNAPSE_STATE_STABLE, callback_user_data);
    }

    NIMCP_LOGGING_DEBUG("Stabilized synapse %u", synapse_id);
    return 0;
}

int structural_plasticity_potentiate_synapse(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_potentiate_synapse: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_potentiate_synapse: spine is NULL");
        return -1;
    }

    if (spine->state != SYNAPSE_STATE_STABLE) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "structural_plasticity_potentiate_synapse: validation failed");
        return -1;  /* Can only potentiate stable spines */
    }

    synapse_state_t old_state = spine->state;
    spine->state = SYNAPSE_STATE_POTENTIATED;
    update_to_potentiated_morphology(&spine->morphology);
    spine->potentiation_events++;
    system->total_potentiations++;

    /* Copy callback data before releasing mutex */
    structural_change_callback_t callback = system->callback;
    void* callback_user_data = system->callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Notify callback after releasing mutex to avoid deadlock */
    if (callback) {
        callback(STRUCTURAL_EVENT_POTENTIATION, synapse_id,
                old_state, SYNAPSE_STATE_POTENTIATED, callback_user_data);
    }

    NIMCP_LOGGING_DEBUG("Potentiated synapse %u", synapse_id);
    return 0;
}

int structural_plasticity_tag_for_consolidation(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_tag_for_consolidation: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_tag_for_consolidation: spine is NULL");
        return -1;
    }

    spine->consolidation_tagged = true;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Activity Tracking Implementation
 * ============================================================================ */

int structural_plasticity_update_activity(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    uint64_t current_time
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_update_activity: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_update_activity: spine is NULL");
        return -1;
    }

    /* Update activity with exponential moving average */
    float alpha = 0.1f;  /* Smoothing factor */
    if (spine->last_active_time > 0) {
        float dt_sec = (current_time - spine->last_active_time) / 1000.0f;

        /* Guard against division by very small dt (cap at 1ms = 1000Hz max) */
        const float MIN_DT_SEC = 0.001f;  /* 1 millisecond */
        if (dt_sec >= MIN_DT_SEC) {
            float instant_rate = 1.0f / dt_sec;

            /* Cap maximum instantaneous rate to prevent overflow (max 10kHz) */
            const float MAX_RATE_HZ = 10000.0f;
            if (instant_rate > MAX_RATE_HZ) {
                instant_rate = MAX_RATE_HZ;
            }

            spine->recent_activity_hz =
                alpha * instant_rate + (1.0f - alpha) * spine->recent_activity_hz;
        }
    }

    spine->last_active_time = current_time;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int structural_plasticity_record_ltp(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    float ltp_magnitude
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_record_ltp: system is NULL");
        return -1;
    }

    /* PARAMETER VALIDATION: Validate LTP magnitude
     * WHAT: Ensure ltp_magnitude is finite and non-negative
     * WHY:  Invalid values cause accumulator corruption
     * HOW:  Check for NaN/Inf and clamp to reasonable range
     */
    if (isnan(ltp_magnitude) || isinf(ltp_magnitude)) {
        return 0;  /* Skip invalid LTP event */
    }
    if (ltp_magnitude < 0.0f) ltp_magnitude = 0.0f;
    if (ltp_magnitude > 100.0f) ltp_magnitude = 100.0f;  /* Cap at reasonable max */

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_record_ltp: spine is NULL");
        return -1;
    }

    /* Accumulate with overflow protection */
    spine->ltp_accumulator += ltp_magnitude;
    if (spine->ltp_accumulator > 10000.0f) {
        spine->ltp_accumulator = 10000.0f;  /* Cap accumulator */
    }
    if (isnan(spine->ltp_accumulator)) {
        spine->ltp_accumulator = 0.0f;  /* Reset on NaN */
    }

    /* Check potentiation threshold */
    bool should_notify = false;
    synapse_state_t old_state = spine->state;
    if (spine->state == SYNAPSE_STATE_STABLE &&
        spine->ltp_accumulator >= system->config.ltp_potentiation_threshold) {
        /* Auto-potentiate */
        spine->state = SYNAPSE_STATE_POTENTIATED;
        update_to_potentiated_morphology(&spine->morphology);
        spine->potentiation_events++;
        system->total_potentiations++;
        should_notify = true;
    }

    /* Copy callback data before releasing mutex */
    structural_change_callback_t callback = system->callback;
    void* callback_user_data = system->callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Notify callback after releasing mutex to avoid deadlock */
    if (should_notify && callback) {
        callback(STRUCTURAL_EVENT_POTENTIATION, synapse_id,
                old_state, SYNAPSE_STATE_POTENTIATED, callback_user_data);
    }

    return 0;
}

int structural_plasticity_record_ltd(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    float ltd_magnitude
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_record_ltd: system is NULL");
        return -1;
    }

    /* PARAMETER VALIDATION: Validate LTD magnitude
     * WHAT: Ensure ltd_magnitude is finite and non-negative
     * WHY:  Invalid values cause accumulator corruption
     * HOW:  Check for NaN/Inf and clamp to reasonable range
     */
    if (isnan(ltd_magnitude) || isinf(ltd_magnitude)) {
        return 0;  /* Skip invalid LTD event */
    }
    if (ltd_magnitude < 0.0f) ltd_magnitude = 0.0f;
    if (ltd_magnitude > 100.0f) ltd_magnitude = 100.0f;  /* Cap at reasonable max */

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_record_ltd: spine is NULL");
        return -1;
    }

    /* Accumulate with overflow protection */
    spine->ltd_accumulator += ltd_magnitude;
    if (spine->ltd_accumulator > 10000.0f) {
        spine->ltd_accumulator = 10000.0f;  /* Cap accumulator */
    }
    if (isnan(spine->ltd_accumulator)) {
        spine->ltd_accumulator = 0.0f;  /* Reset on NaN */
    }

    /* LTD increases pruning urgency */
    float urgency_increase = ltd_magnitude * 0.1f;
    if (!isnan(urgency_increase) && urgency_increase > 0.0f) {
        spine->pruning_urgency += urgency_increase;
    }
    if (spine->pruning_urgency > 1.0f) {
        spine->pruning_urgency = 1.0f;
    }
    if (spine->pruning_urgency < 0.0f) {
        spine->pruning_urgency = 0.0f;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/* Max deferred callbacks during update */
#define MAX_DEFERRED_CALLBACKS 64

typedef struct {
    structural_event_t event;
    uint32_t synapse_id;
    synapse_state_t old_state;
    synapse_state_t new_state;
} deferred_callback_t;

int structural_plasticity_update(
    structural_plasticity_system_t* system,
    float delta_sec
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_update: system is NULL");
        return -1;
    }

    /* Validate delta_sec to prevent numerical issues */
    if (isnan(delta_sec) || isinf(delta_sec) || delta_sec < 0.0f) {
        return 0;  /* Skip update with invalid time step */
    }
    /* Cap delta_sec to prevent extreme values */
    if (delta_sec > 3600.0f) {  /* 1 hour max */
        delta_sec = 3600.0f;
    }

    /* Deferred callbacks to invoke after releasing mutex */
    deferred_callback_t deferred[MAX_DEFERRED_CALLBACKS];
    uint32_t num_deferred = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* BOUNDS VALIDATION: Verify spine array before iteration */
    if (!system->spines || system->num_spines > system->max_spines) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_update: system->spines is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at start of structural plasticity update */
    structural_plasticity_heartbeat("structural_update", 0.0f);

    for (uint32_t i = 0; i < system->num_spines; i++) {
        /* Defensive bounds check per iteration */
        if (i >= system->max_spines) break;
        synapse_structural_state_t* spine = &system->spines[i];

        if (spine->state == SYNAPSE_STATE_ELIMINATED) {
            continue;
        }

        /* Decay activity rate during inactivity (exponential decay with time constant ~20s) */
        float decay_rate = 0.05f;  /* 1/tau where tau = 20 seconds (accelerated for simulations) */
        float decay_factor = expf(-decay_rate * delta_sec);
        float old_activity = spine->recent_activity_hz;
        (void)old_activity;  /* Suppress unused warning */
        spine->recent_activity_hz *= decay_factor;

        /* Update maturation progress for nascent spines */
        if (spine->state == SYNAPSE_STATE_NASCENT) {
            float maturation_rate = 1.0f / system->config.maturation_time_sec;
            spine->maturation_progress += delta_sec * maturation_rate;

            /* Check for auto-stabilization */
            if (spine->maturation_progress >= 1.0f &&
                spine->recent_activity_hz >= system->config.stabilization_threshold) {

                /* Require sleep consolidation if enabled */
                bool can_stabilize = !system->config.require_sleep_consolidation ||
                                    spine->consolidation_tagged;

                if (can_stabilize) {
                    synapse_state_t old_state = spine->state;
                    spine->state = SYNAPSE_STATE_STABLE;
                    update_to_stable_morphology(&spine->morphology);
                    system->total_stabilizations++;

                    /* Defer callback */
                    if (num_deferred < MAX_DEFERRED_CALLBACKS) {
                        deferred[num_deferred].event = STRUCTURAL_EVENT_STABILIZATION;
                        deferred[num_deferred].synapse_id = spine->synapse_id;
                        deferred[num_deferred].old_state = old_state;
                        deferred[num_deferred].new_state = SYNAPSE_STATE_STABLE;
                        num_deferred++;
                    }
                }
            }
        }

        /* Phase 8: Periodic heartbeat for large spine counts */
        if ((i & 0x7F) == 0 && system->num_spines > 128) {
            structural_plasticity_heartbeat("structural_update",
                                           (float)(i + 1) / (float)system->num_spines);
        }

        /* Check for pruning */
        if (structural_plasticity_should_prune(system, spine) &&
            spine->state != SYNAPSE_STATE_PRUNING) {

            synapse_state_t old_state = spine->state;
            spine->state = SYNAPSE_STATE_PRUNING;
            spine->pruning_start_time = 0;  /* Set by caller with real time */
            system->total_pruning_starts++;

            /* Defer callback */
            if (num_deferred < MAX_DEFERRED_CALLBACKS) {
                deferred[num_deferred].event = STRUCTURAL_EVENT_PRUNING_START;
                deferred[num_deferred].synapse_id = spine->synapse_id;
                deferred[num_deferred].old_state = old_state;
                deferred[num_deferred].new_state = SYNAPSE_STATE_PRUNING;
                num_deferred++;
            }
        }

        /* Update pruning spines */
        if (spine->state == SYNAPSE_STATE_PRUNING) {
            spine->pruning_urgency += delta_sec * system->config.pruning_rate;

            /* Shrink spine */
            spine->morphology.spine_volume -=
                delta_sec * STRUCTURAL_ACTIN_SHRINK_RATE;
            if (spine->morphology.spine_volume < 0.0f) {
                spine->morphology.spine_volume = 0.0f;
            }

            /* Eliminate if urgency threshold reached */
            if (spine->pruning_urgency >= 1.0f ||
                spine->morphology.spine_volume <= 0.01f) {

                synapse_state_t old_state = spine->state;
                spine->state = SYNAPSE_STATE_ELIMINATED;
                system->total_eliminations++;

                /* Defer callback */
                if (num_deferred < MAX_DEFERRED_CALLBACKS) {
                    deferred[num_deferred].event = STRUCTURAL_EVENT_ELIMINATION;
                    deferred[num_deferred].synapse_id = spine->synapse_id;
                    deferred[num_deferred].old_state = old_state;
                    deferred[num_deferred].new_state = SYNAPSE_STATE_ELIMINATED;
                    num_deferred++;
                }
            }
        }

        /* Decay potentiation back to stable */
        if (spine->state == SYNAPSE_STATE_POTENTIATED) {
            spine->ltp_accumulator -=
                delta_sec * system->config.potentiation_decay_rate;

            if (spine->ltp_accumulator < system->config.ltp_potentiation_threshold * 0.5f) {
                /* Decay back to stable */
                synapse_state_t old_state = spine->state;
                spine->state = SYNAPSE_STATE_STABLE;
                update_to_stable_morphology(&spine->morphology);

                /* Defer callback */
                if (num_deferred < MAX_DEFERRED_CALLBACKS) {
                    deferred[num_deferred].event = STRUCTURAL_EVENT_STABILIZATION;
                    deferred[num_deferred].synapse_id = spine->synapse_id;
                    deferred[num_deferred].old_state = old_state;
                    deferred[num_deferred].new_state = SYNAPSE_STATE_STABLE;
                    num_deferred++;
                }
            }
        }
    }

    /* Copy callback data before releasing mutex */
    structural_change_callback_t callback = system->callback;
    void* callback_user_data = system->callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Invoke deferred callbacks after releasing mutex to avoid deadlock */
    if (callback) {
        for (uint32_t i = 0; i < num_deferred; i++) {
            callback(deferred[i].event, deferred[i].synapse_id,
                    deferred[i].old_state, deferred[i].new_state,
                    callback_user_data);
        }
    }

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int structural_plasticity_get_synapse_state(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    synapse_structural_state_t* state
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_synapse_state: system is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_synapse_state: state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Use find_spine_any_state to allow querying eliminated spines */
    synapse_structural_state_t* spine =
        find_spine_any_state((structural_plasticity_system_t*)system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_synapse_state: spine is NULL");
        return -1;
    }

    *state = *spine;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int structural_plasticity_get_morphology(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    spine_morphology_t* morphology
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_morphology: system is NULL");
        return -1;
    }
    if (!morphology) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_morphology: morphology is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Use find_spine_any_state to allow querying eliminated spines */
    synapse_structural_state_t* spine =
        find_spine_any_state((structural_plasticity_system_t*)system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_morphology: spine is NULL");
        return -1;
    }

    *morphology = spine->morphology;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

uint32_t structural_plasticity_get_spine_count(
    structural_plasticity_system_t* system,
    synapse_state_t state
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_spine_count: system is NULL");
        return 0;
    }

    nimcp_platform_mutex_lock(system->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < system->num_spines; i++) {
        if (system->spines[i].state == state) {
            count++;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return count;
}

uint32_t structural_plasticity_get_total_spines(
    structural_plasticity_system_t* system
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_total_spines: system is NULL");
        return 0;
    }

    nimcp_platform_mutex_lock(system->mutex);
    uint32_t count = 0;
    for (uint32_t i = 0; i < system->num_spines; i++) {
        if (system->spines[i].state != SYNAPSE_STATE_ELIMINATED) {
            count++;
        }
    }
    nimcp_platform_mutex_unlock(system->mutex);
    return count;
}

/* ============================================================================
 * Callback Implementation
 * ============================================================================ */

int structural_plasticity_register_callback(
    structural_plasticity_system_t* system,
    structural_change_callback_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_register_callback: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->callback = callback;
    system->callback_user_data = user_data;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Immune Integration Implementation
 * ============================================================================ */

int structural_plasticity_tag_complement(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    const uint8_t* tag,
    size_t tag_len
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_tag_complement: system is NULL");
        return -1;
    }
    if (!tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_tag_complement: tag is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine = find_spine(system, synapse_id);
    if (!spine) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_tag_complement: spine is NULL");
        return -1;
    }

    spine->complement_tagged = true;

    /* Copy tag (truncate if necessary) */
    size_t copy_len = tag_len < STRUCTURAL_EPITOPE_SIZE ?
                     tag_len : STRUCTURAL_EPITOPE_SIZE;
    memcpy(spine->complement_tag, tag, copy_len);
    if (copy_len < STRUCTURAL_EPITOPE_SIZE) {
        memset(spine->complement_tag + copy_len, 0,
              STRUCTURAL_EPITOPE_SIZE - copy_len);
    }

    /* Increase pruning urgency */
    spine->pruning_urgency += 0.5f * system->config.complement_sensitivity;
    if (spine->pruning_urgency > 1.0f) {
        spine->pruning_urgency = 1.0f;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Tagged synapse %u with complement", synapse_id);
    return 0;
}

bool structural_plasticity_is_complement_tagged(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
) {
    if (!system) {
        return false;
    }

    nimcp_platform_mutex_lock(system->mutex);

    synapse_structural_state_t* spine =
        find_spine((structural_plasticity_system_t*)system, synapse_id);
    bool tagged = spine ? spine->complement_tagged : false;

    nimcp_platform_mutex_unlock(system->mutex);
    return tagged;
}

int structural_plasticity_get_complement_tagged(
    structural_plasticity_system_t* system,
    uint32_t* synapse_ids,
    uint32_t max_count,
    uint32_t* count
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_complement_tagged: system is NULL");
        return -1;
    }
    if (!synapse_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_complement_tagged: synapse_ids is NULL");
        return -1;
    }
    if (!count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_get_complement_tagged: count is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    *count = 0;
    for (uint32_t i = 0; i < system->num_spines && *count < max_count; i++) {
        if (system->spines[i].complement_tagged &&
            system->spines[i].state != SYNAPSE_STATE_ELIMINATED) {
            synapse_ids[(*count)++] = system->spines[i].synapse_id;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Slot Reclamation Implementation
 * ============================================================================ */

/**
 * WHAT: Compact spine array by removing eliminated slots
 * WHY:  Eliminated spines consume memory and slow down iteration
 * HOW:  Shift non-eliminated spines to fill gaps left by eliminated ones
 *
 * THREAD SAFETY: Caller must hold system mutex
 * COMPLEXITY: O(n) where n = num_spines
 *
 * @param system Structural plasticity system
 * @return Number of slots reclaimed
 */
static uint32_t compact_eliminated_spines_locked(structural_plasticity_system_t* system) {
    if (!system || !system->spines) return 0;

    uint32_t write_idx = 0;
    uint32_t reclaimed = 0;

    for (uint32_t read_idx = 0; read_idx < system->num_spines; read_idx++) {
        if (system->spines[read_idx].state != SYNAPSE_STATE_ELIMINATED) {
            /* Keep this spine */
            if (write_idx != read_idx) {
                system->spines[write_idx] = system->spines[read_idx];
            }
            write_idx++;
        } else {
            /* Reclaim this slot */
            reclaimed++;
        }
    }

    system->num_spines = write_idx;
    return reclaimed;
}

/**
 * WHAT: Find a reusable eliminated slot in the spine array
 * WHY:  Avoid compaction overhead by reusing eliminated slots for new spines
 * HOW:  Linear scan for first ELIMINATED slot
 *
 * THREAD SAFETY: Caller must hold system mutex
 *
 * @param system Structural plasticity system
 * @return Index of eliminated slot, or (uint32_t)-1 if none found
 */
static uint32_t find_eliminated_slot_locked(structural_plasticity_system_t* system) {
    if (!system || !system->spines) return (uint32_t)-1;

    for (uint32_t i = 0; i < system->num_spines; i++) {
        if (system->spines[i].state == SYNAPSE_STATE_ELIMINATED) {
            return i;
        }
    }
    return (uint32_t)-1;
}

int structural_plasticity_compact(structural_plasticity_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_compact: system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    uint32_t reclaimed = compact_eliminated_spines_locked(system);
    nimcp_platform_mutex_unlock(system->mutex);

    if (reclaimed > 0) {
        NIMCP_LOGGING_INFO("Compacted spine array: reclaimed %u slots", reclaimed);
    }

    return (int)reclaimed;
}

uint32_t structural_plasticity_count_eliminated(
    const structural_plasticity_system_t* system
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "structural_plasticity_count_eliminated: system is NULL");
        return 0;
    }

    nimcp_platform_mutex_lock(((structural_plasticity_system_t*)system)->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < system->num_spines; i++) {
        if (system->spines[i].state == SYNAPSE_STATE_ELIMINATED) {
            count++;
        }
    }

    nimcp_platform_mutex_unlock(((structural_plasticity_system_t*)system)->mutex);
    return count;
}
