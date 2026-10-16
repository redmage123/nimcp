/**
 * @file nimcp_plasticity_coordinator.c
 * @brief Plasticity Coordinator Implementation
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "plasticity/nimcp_plasticity_coordinator.h"
#include <stddef.h>  /* for NULL */
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"

/* Forward declaration of the one tpb API we need here, to avoid pulling in
 * nimcp_training_plasticity_bridge.h (which transitively includes a large
 * chunk of cognitive/training headers). tpb_context_t is already forward-
 * declared in nimcp_plasticity_coordinator.h. */
extern bool nimcp_tpb_is_backprop_active(tpb_context_t* ctx);

/* ============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(plasticity)

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Platform-independent millisecond timer
 * WHY:  Need consistent timing across platforms
 * HOW:  Use clock_gettime or fallback
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

/**
 * @brief Apply state configuration to mechanisms
 *
 * WHAT: Enable/disable mechanisms based on state
 * WHY:  Different states have different mechanism priorities
 * HOW:  Iterate mechanisms, set enabled based on state config
 */
static int apply_state_configuration(
    plasticity_coordinator_t* coordinator,
    plasticity_coordinator_state_t state
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_state_configuration: coordinator is NULL");
        return -1;
    }

    const plasticity_state_config_t* state_config =
        &coordinator->config.state_configs[state];

    for (uint32_t i = 0; i < coordinator->mechanism_count; i++) {
        plasticity_mechanism_entry_t* entry = &coordinator->mechanisms[i];

        switch (entry->type) {
            case PLASTICITY_TYPE_STDP:
                entry->enabled = state_config->enable_stdp;
                entry->priority = state_config->stdp_priority;
                break;
            case PLASTICITY_TYPE_BCM:
                entry->enabled = state_config->enable_bcm;
                entry->priority = state_config->bcm_priority;
                break;
            case PLASTICITY_TYPE_HOMEOSTATIC:
                entry->enabled = state_config->enable_homeostatic;
                entry->priority = state_config->homeostatic_priority;
                break;
            case PLASTICITY_TYPE_ELIGIBILITY:
                entry->enabled = state_config->enable_eligibility;
                entry->priority = state_config->eligibility_priority;
                break;
            case PLASTICITY_TYPE_DENDRITIC:
                entry->enabled = state_config->enable_dendritic;
                entry->priority = state_config->dendritic_priority;
                break;
            case PLASTICITY_TYPE_STP:
                entry->enabled = state_config->enable_stp;
                entry->priority = state_config->stp_priority;
                break;
            case PLASTICITY_TYPE_ADAPTIVE:
                entry->enabled = state_config->enable_adaptive;
                entry->priority = state_config->adaptive_priority;
                break;
            case PLASTICITY_TYPE_PREDICTIVE:
                entry->enabled = state_config->enable_predictive;
                entry->priority = state_config->predictive_priority;
                break;
            default:
                break;
        }
    }

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int plasticity_coordinator_default_config(plasticity_coordinator_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Config is NULL");
        NIMCP_LOGGING_ERROR("Config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(plasticity_coordinator_config_t));

    /* ACQUISITION state: New learning (STDP+BCM dominant) */
    config->state_configs[PLASTICITY_STATE_ACQUISITION] = (plasticity_state_config_t){
        .enable_stdp = true,
        .enable_bcm = true,
        .enable_homeostatic = false,
        .enable_eligibility = true,
        .enable_dendritic = true,
        .enable_stp = true,
        .enable_adaptive = true,
        .enable_predictive = true,
        .stdp_priority = 0.9f,
        .bcm_priority = 0.8f,
        .homeostatic_priority = 0.2f,
        .eligibility_priority = 0.7f,
        .dendritic_priority = 0.6f,
        .stp_priority = 0.5f,
        .adaptive_priority = 0.5f,
        .predictive_priority = 0.8f
    };

    /* CONSOLIDATION state: Memory consolidation (eligibility→weight) */
    config->state_configs[PLASTICITY_STATE_CONSOLIDATION] = (plasticity_state_config_t){
        .enable_stdp = false,
        .enable_bcm = false,
        .enable_homeostatic = true,
        .enable_eligibility = true,
        .enable_dendritic = false,
        .enable_stp = false,
        .enable_adaptive = false,
        .enable_predictive = false,
        .stdp_priority = 0.3f,
        .bcm_priority = 0.3f,
        .homeostatic_priority = 0.9f,
        .eligibility_priority = 1.0f,
        .dendritic_priority = 0.2f,
        .stp_priority = 0.1f,
        .adaptive_priority = 0.2f,
        .predictive_priority = 0.3f
    };

    /* MAINTENANCE state: Stable state (minimal plasticity) */
    config->state_configs[PLASTICITY_STATE_MAINTENANCE] = (plasticity_state_config_t){
        .enable_stdp = false,
        .enable_bcm = false,
        .enable_homeostatic = true,
        .enable_eligibility = false,
        .enable_dendritic = false,
        .enable_stp = true,
        .enable_adaptive = false,
        .enable_predictive = false,
        .stdp_priority = 0.1f,
        .bcm_priority = 0.1f,
        .homeostatic_priority = 0.7f,
        .eligibility_priority = 0.1f,
        .dendritic_priority = 0.1f,
        .stp_priority = 0.8f,
        .adaptive_priority = 0.1f,
        .predictive_priority = 0.2f
    };

    /* STABILIZING state: Preventing runaway (homeostatic dominant) */
    config->state_configs[PLASTICITY_STATE_STABILIZING] = (plasticity_state_config_t){
        .enable_stdp = false,
        .enable_bcm = true,
        .enable_homeostatic = true,
        .enable_eligibility = false,
        .enable_dendritic = false,
        .enable_stp = false,
        .enable_adaptive = true,
        .enable_predictive = false,
        .stdp_priority = 0.2f,
        .bcm_priority = 0.6f,
        .homeostatic_priority = 1.0f,
        .eligibility_priority = 0.1f,
        .dendritic_priority = 0.1f,
        .stp_priority = 0.3f,
        .adaptive_priority = 0.7f,
        .predictive_priority = 0.2f
    };

    /* Global settings */
    config->initial_state = PLASTICITY_STATE_ACQUISITION;
    config->conflict_strategy = CONFLICT_RESOLUTION_WEIGHTED_AVERAGE;
    config->max_mechanisms = PLASTICITY_COORDINATOR_MAX_MECHANISMS;

    /* Energy budget */
    config->enable_energy_tracking = true;
    config->energy_budget_per_second = 100.0f;
    config->low_energy_threshold = 80.0f;

    /* Integration */
    config->enable_bio_async = true;
    config->enable_brain_immune = true;
    config->enable_statistics = true;
    config->enable_logging = true;

    /* Advanced */
    config->auto_state_transitions = false;
    config->consolidation_trigger_interval_ms = 60000;  /* 1 minute */
    config->conflict_threshold = 0.001f;

    return 0;
}

plasticity_coordinator_t* plasticity_coordinator_create(
    const plasticity_coordinator_config_t* config
) {
    /* Allocate coordinator */
    plasticity_coordinator_t* coordinator = (plasticity_coordinator_t*)
        nimcp_malloc(sizeof(plasticity_coordinator_t));
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate coordinator");
        NIMCP_LOGGING_ERROR("Failed to allocate coordinator");
        return NULL;
    }

    memset(coordinator, 0, sizeof(plasticity_coordinator_t));

    /* Apply configuration */
    if (config) {
        coordinator->config = *config;
    } else {
        if (plasticity_coordinator_default_config(&coordinator->config) != 0) {
            nimcp_free(coordinator);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_coordinator_create: validation failed");
            return NULL;
        }
    }

    /* Allocate mechanism registry */
    coordinator->mechanism_capacity = coordinator->config.max_mechanisms;
    coordinator->mechanisms = (plasticity_mechanism_entry_t*)
        nimcp_malloc(sizeof(plasticity_mechanism_entry_t) * coordinator->mechanism_capacity);
    if (!coordinator->mechanisms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mechanism registry");
        NIMCP_LOGGING_ERROR("Failed to allocate mechanism registry");
        nimcp_free(coordinator);
        return NULL;
    }

    memset(coordinator->mechanisms, 0,
           sizeof(plasticity_mechanism_entry_t) * coordinator->mechanism_capacity);

    /* Create mutex */
    coordinator->mutex = nimcp_platform_mutex_create();
    if (!coordinator->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(coordinator->mechanisms);
        nimcp_free(coordinator);
        return NULL;
    }

    /* Initialize state */
    coordinator->state = coordinator->config.initial_state;
    coordinator->start_time = get_current_time_ms();
    coordinator->last_update_time = coordinator->start_time;
    coordinator->last_consolidation_time = coordinator->start_time;
    coordinator->energy_tracking_start = coordinator->start_time;

    /* Initialize statistics */
    memset(&coordinator->stats, 0, sizeof(plasticity_coordinator_stats_t));
    coordinator->stats.current_state = coordinator->state;

    /* Initialize mechanism ID counter to 1 (0 is reserved for "invalid") */
    coordinator->next_mechanism_id = 1;

    if (coordinator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Plasticity coordinator created");
    }

    return coordinator;
}

void plasticity_coordinator_destroy(plasticity_coordinator_t* coordinator) {
    /* Guard clause */
    if (!coordinator) return;

    /* Disconnect integrations */
    if (coordinator->bio_async_connected) {
        plasticity_coordinator_disconnect_bio_async(coordinator);
    }
    if (coordinator->immune_connected) {
        plasticity_coordinator_disconnect_brain_immune(coordinator);
    }

    /* Destroy mutex */
    if (coordinator->mutex) {
        nimcp_platform_mutex_destroy(coordinator->mutex);
        nimcp_free(coordinator->mutex);
        coordinator->mutex = NULL;
    }

    /* Free registry */
    if (coordinator->mechanisms) {
        nimcp_free(coordinator->mechanisms);
    }

    /* Free coordinator */
    nimcp_free(coordinator);

    NIMCP_LOGGING_INFO("Plasticity coordinator destroyed");
}

/* ============================================================================
 * Mechanism Registration API Implementation
 * ============================================================================ */

int plasticity_coordinator_register_mechanism(
    plasticity_coordinator_t* coordinator,
    const char* name,
    plasticity_mechanism_type_t type,
    plasticity_mechanism_handle_t handle,
    plasticity_mechanism_update_fn_t update_fn,
    plasticity_mechanism_get_weight_change_fn_t get_weight_change_fn,
    float priority,
    float energy_cost,
    uint64_t update_interval_ms,
    uint32_t* mechanism_id_out
) {
    /* Guard clauses — handle and update_fn may be NULL for declaration-only
     * registrations where the coordinator tracks metadata/scheduling only */
    if (!coordinator || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid parameters for mechanism registration");
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return -1;
    }

    nimcp_platform_mutex_lock(coordinator->mutex);

    /* Check capacity */
    if (coordinator->mechanism_count >= coordinator->mechanism_capacity) {
        nimcp_platform_mutex_unlock(coordinator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Mechanism registry full");
        NIMCP_LOGGING_ERROR("Mechanism registry full");
        return -1;
    }

    /* Create entry */
    plasticity_mechanism_entry_t* entry =
        &coordinator->mechanisms[coordinator->mechanism_count];

    entry->mechanism_id = coordinator->next_mechanism_id++;
    entry->mechanism_name = name;
    entry->type = type;
    entry->handle = handle;
    entry->update_fn = update_fn;
    entry->get_weight_change_fn = get_weight_change_fn;
    entry->update_interval_ms = update_interval_ms;
    entry->last_update_time = get_current_time_ms();
    entry->priority = priority;
    entry->energy_cost = energy_cost;
    entry->enabled = true;
    entry->update_count = 0;
    entry->total_energy_consumed = 0.0f;

    coordinator->mechanism_count++;
    coordinator->stats.total_mechanisms++;

    if (mechanism_id_out) {
        *mechanism_id_out = entry->mechanism_id;
    }

    nimcp_platform_mutex_unlock(coordinator->mutex);

    if (coordinator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered mechanism: %s (ID: %u, Type: %s)",
            name, entry->mechanism_id, plasticity_mechanism_type_to_string(type));
    }

    return 0;
}

int plasticity_coordinator_unregister_mechanism(
    plasticity_coordinator_t* coordinator,
    uint32_t mechanism_id
) {
    /* Guard clause */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(coordinator->mutex);

    /* Find mechanism */
    int32_t found_idx = -1;
    for (uint32_t i = 0; i < coordinator->mechanism_count; i++) {
        if (coordinator->mechanisms[i].mechanism_id == mechanism_id) {
            found_idx = (int32_t)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_platform_mutex_unlock(coordinator->mutex);
        NIMCP_LOGGING_WARN("Mechanism %u not found", mechanism_id);
        return -1;
    }

    /* Remove by shifting array */
    for (uint32_t i = (uint32_t)found_idx; i < coordinator->mechanism_count - 1; i++) {
        coordinator->mechanisms[i] = coordinator->mechanisms[i + 1];
    }

    coordinator->mechanism_count--;
    coordinator->stats.total_mechanisms--;

    nimcp_platform_mutex_unlock(coordinator->mutex);

    if (coordinator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Unregistered mechanism: %u", mechanism_id);
    }

    return 0;
}

int plasticity_coordinator_set_mechanism_enabled(
    plasticity_coordinator_t* coordinator,
    uint32_t mechanism_id,
    bool enabled
) {
    /* Guard clause */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(coordinator->mutex);

    /* Find mechanism */
    for (uint32_t i = 0; i < coordinator->mechanism_count; i++) {
        if (coordinator->mechanisms[i].mechanism_id == mechanism_id) {
            coordinator->mechanisms[i].enabled = enabled;
            nimcp_platform_mutex_unlock(coordinator->mutex);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(coordinator->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_coordinator_set_mechanism_enabled: validation failed");
    return -1;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

int plasticity_coordinator_update(
    plasticity_coordinator_t* coordinator,
    uint64_t current_time_ms,
    float dt
) {
    /* Guard clauses */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in update");
        return -1;
    }
    if (dt <= 0.0f) return 0;

    /* Backprop gate: skip all plasticity updates while gradient-based
     * training is running. Prevents STDP/BCM/homeostatic from racing
     * the optimizer's weight writes.
     *
     * The pointer read uses __atomic_load_n(ACQUIRE) to pair with the
     * __atomic_store_n(RELEASE) in the setter — so whichever thread
     * reads sees either the old or the new pointer, never a torn value,
     * and any writes the setter made prior to storing the pointer are
     * visible to the reader. The skip counter uses __atomic_fetch_add
     * to avoid a lost-update race under contention. */
    tpb_context_t* bridge = __atomic_load_n(&coordinator->plasticity_bridge,
                                            __ATOMIC_ACQUIRE);
    if (bridge && nimcp_tpb_is_backprop_active(bridge)) {
        __atomic_fetch_add(&coordinator->backprop_gate_skip_count, 1,
                           __ATOMIC_RELAXED);
        return 0;
    }

    /* Phase 8: Send heartbeat at start of plasticity update */
    plasticity_heartbeat("plasticity_update", 0.0f);

    nimcp_platform_mutex_lock(coordinator->mutex);

    uint64_t start_time_us = get_current_time_ms() * 1000;
    int mechanisms_updated = 0;

    /* Update energy tracking window */
    uint64_t time_in_window = current_time_ms - coordinator->energy_tracking_start;
    if (time_in_window >= 1000) {  /* 1 second window */
        coordinator->stats.current_energy_rate =
            coordinator->energy_consumed_this_second;
        coordinator->energy_consumed_this_second = 0.0f;
        coordinator->energy_tracking_start = current_time_ms;

        /* Check energy budget */
        if (coordinator->config.enable_energy_tracking &&
            coordinator->stats.current_energy_rate >
            coordinator->config.energy_budget_per_second) {
            coordinator->stats.low_energy_events++;
        }
    }

    /* Check if low energy mode */
    bool low_energy = plasticity_coordinator_is_low_energy(coordinator);

    /* Collect mechanisms due for update while holding lock, then call outside */
    typedef struct {
        int (*update_fn)(void*, float);
        void* handle;
        float energy_cost;
        uint32_t index;
        uint32_t type;
    } pending_update_t;

    /* Stack-allocate for typical case; if mechanism_count > 256, only first 256 run */
    pending_update_t pending[256];
    uint32_t num_pending = 0;

    for (uint32_t i = 0; i < coordinator->mechanism_count; i++) {
        plasticity_mechanism_entry_t* entry = &coordinator->mechanisms[i];

        /* Skip if disabled */
        if (!entry->enabled) continue;

        /* Skip if in low energy and not critical */
        if (low_energy && entry->priority < 0.7f) continue;

        /* Check if due for update */
        uint64_t elapsed = current_time_ms - entry->last_update_time;
        if (elapsed < entry->update_interval_ms) continue;

        /* Skip declaration-only mechanisms with no callback */
        if (!entry->update_fn) continue;

        if (num_pending < 256) {
            pending[num_pending].update_fn = entry->update_fn;
            pending[num_pending].handle = entry->handle;
            pending[num_pending].energy_cost = entry->energy_cost;
            pending[num_pending].index = i;
            pending[num_pending].type = entry->type;
            num_pending++;
        }
    }

    nimcp_platform_mutex_unlock(coordinator->mutex);

    /* Call update functions OUTSIDE the lock to avoid deadlock */
    for (uint32_t p = 0; p < num_pending; p++) {
        if (pending[p].update_fn(pending[p].handle, dt) == 0) {
            /* Re-acquire lock to update stats */
            nimcp_platform_mutex_lock(coordinator->mutex);

            plasticity_mechanism_entry_t* entry =
                &coordinator->mechanisms[pending[p].index];
            entry->update_count++;
            entry->last_update_time = current_time_ms;

            /* Track energy */
            if (coordinator->config.enable_energy_tracking) {
                entry->total_energy_consumed += pending[p].energy_cost;
                coordinator->energy_consumed_this_second += pending[p].energy_cost;
                coordinator->stats.total_energy_consumed += pending[p].energy_cost;
            }

            mechanisms_updated++;
            coordinator->stats.total_mechanism_updates++;

            /* Update per-mechanism stats */
            coordinator->stats.mechanism_stats[pending[p].type].total_updates++;
            coordinator->stats.mechanism_stats[pending[p].type].total_energy_consumed +=
                pending[p].energy_cost;

            nimcp_platform_mutex_unlock(coordinator->mutex);
        }

        /* Phase 8: Send heartbeat for progress tracking in large mechanism sets */
        if ((p & 0x1F) == 0 && num_pending > 32) {
            plasticity_heartbeat("plasticity_update",
                                (float)(p + 1) / (float)num_pending);
        }
    }

    nimcp_platform_mutex_lock(coordinator->mutex);

    /* Update statistics */
    coordinator->stats.total_update_cycles++;
    coordinator->last_update_time = current_time_ms;
    coordinator->stats.time_in_current_state_ms =
        current_time_ms - coordinator->start_time;

    /* Check for auto-consolidation trigger */
    if (coordinator->config.auto_state_transitions &&
        coordinator->state == PLASTICITY_STATE_ACQUISITION) {
        uint64_t since_consolidation =
            current_time_ms - coordinator->last_consolidation_time;
        if (since_consolidation >=
            coordinator->config.consolidation_trigger_interval_ms) {
            plasticity_coordinator_trigger_consolidation(coordinator);
        }
    }

    /* Update timing stats */
    uint64_t end_time_us = get_current_time_ms() * 1000;
    float cycle_time_us = (float)(end_time_us - start_time_us);

    /* Running average */
    if (coordinator->stats.total_update_cycles > 0) {
        coordinator->stats.avg_cycle_time_us =
            (coordinator->stats.avg_cycle_time_us *
             (coordinator->stats.total_update_cycles - 1) + cycle_time_us) /
            coordinator->stats.total_update_cycles;
    }

    nimcp_platform_mutex_unlock(coordinator->mutex);

    return mechanisms_updated;
}

int plasticity_coordinator_update_mechanism(
    plasticity_coordinator_t* coordinator,
    uint32_t mechanism_id,
    float dt
) {
    /* Guard clauses */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in update_mechanism");
        return -1;
    }
    if (dt <= 0.0f) return -1;

    nimcp_platform_mutex_lock(coordinator->mutex);

    /* Find mechanism and copy callback data under lock */
    int (*update_fn)(void*, float) = NULL;
    void* handle = NULL;
    float energy_cost = 0.0f;
    uint32_t found_idx = UINT32_MAX;

    for (uint32_t i = 0; i < coordinator->mechanism_count; i++) {
        if (coordinator->mechanisms[i].mechanism_id == mechanism_id) {
            plasticity_mechanism_entry_t* entry = &coordinator->mechanisms[i];

            /* Skip declaration-only mechanisms with no callback */
            if (!entry->update_fn) {
                nimcp_platform_mutex_unlock(coordinator->mutex);
                return 0;
            }

            update_fn = entry->update_fn;
            handle = entry->handle;
            energy_cost = entry->energy_cost;
            found_idx = i;
            break;
        }
    }

    if (found_idx == UINT32_MAX) {
        nimcp_platform_mutex_unlock(coordinator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_coordinator_update_mechanism: operation failed");
        return -1;
    }

    nimcp_platform_mutex_unlock(coordinator->mutex);

    /* Call update function OUTSIDE the lock to avoid deadlock */
    int result = update_fn(handle, dt);

    if (result == 0) {
        nimcp_platform_mutex_lock(coordinator->mutex);
        plasticity_mechanism_entry_t* entry = &coordinator->mechanisms[found_idx];
        entry->update_count++;
        entry->last_update_time = get_current_time_ms();

        /* Track energy */
        if (coordinator->config.enable_energy_tracking) {
            entry->total_energy_consumed += energy_cost;
            coordinator->stats.total_energy_consumed += energy_cost;
        }
        nimcp_platform_mutex_unlock(coordinator->mutex);
    }

    return result;
}

/* ============================================================================
 * State Management API Implementation
 * ============================================================================ */

int plasticity_coordinator_set_state(
    plasticity_coordinator_t* coordinator,
    plasticity_coordinator_state_t new_state
) {
    /* Guard clauses */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in set_state");
        return -1;
    }
    if (new_state >= PLASTICITY_STATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED, "Invalid plasticity state: %d", new_state);
        return -1;
    }

    nimcp_platform_mutex_lock(coordinator->mutex);

    plasticity_coordinator_state_t old_state = coordinator->state;
    coordinator->state = new_state;
    coordinator->stats.current_state = new_state;
    coordinator->stats.state_transition_count++;

    /* Apply state configuration */
    apply_state_configuration(coordinator, new_state);

    nimcp_platform_mutex_unlock(coordinator->mutex);

    if (coordinator->config.enable_logging) {
        NIMCP_LOGGING_INFO("State transition: %s → %s",
            plasticity_coordinator_state_to_string(old_state),
            plasticity_coordinator_state_to_string(new_state));
    }

    return 0;
}

plasticity_coordinator_state_t plasticity_coordinator_get_state(
    const plasticity_coordinator_t* coordinator
) {
    if (!coordinator) return PLASTICITY_STATE_ACQUISITION;
    return coordinator->state;
}

int plasticity_coordinator_trigger_consolidation(
    plasticity_coordinator_t* coordinator
) {
    /* Guard clause */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in trigger_consolidation");
        return -1;
    }

    /* Transition to consolidation state */
    int result = plasticity_coordinator_set_state(
        coordinator,
        PLASTICITY_STATE_CONSOLIDATION
    );

    if (result == 0) {
        coordinator->last_consolidation_time = get_current_time_ms();

        if (coordinator->config.enable_logging) {
            NIMCP_LOGGING_INFO("Consolidation triggered");
        }
    }

    return result;
}

/* ============================================================================
 * Conflict Resolution API Implementation
 * ============================================================================ */

int plasticity_coordinator_resolve_conflict(
    plasticity_coordinator_t* coordinator,
    uint32_t synapse_id,
    plasticity_mechanism_type_t type_a,
    float weight_change_a,
    plasticity_mechanism_type_t type_b,
    float weight_change_b,
    float* resolved_change_out
) {
    /* Guard clauses */
    if (!coordinator || !resolved_change_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in resolve_conflict");
        return -1;
    }

    /* Check if actually a conflict */
    float diff = fabsf(weight_change_a - weight_change_b);
    if (diff < coordinator->config.conflict_threshold) {
        *resolved_change_out = (weight_change_a + weight_change_b) / 2.0f;
        return 0;
    }

    nimcp_platform_mutex_lock(coordinator->mutex);

    float resolved = 0.0f;
    const plasticity_state_config_t* state_config =
        &coordinator->config.state_configs[coordinator->state];

    /* Get priorities */
    float priority_a = 0.5f, priority_b = 0.5f;

    switch (type_a) {
        case PLASTICITY_TYPE_STDP: priority_a = state_config->stdp_priority; break;
        case PLASTICITY_TYPE_BCM: priority_a = state_config->bcm_priority; break;
        case PLASTICITY_TYPE_HOMEOSTATIC: priority_a = state_config->homeostatic_priority; break;
        case PLASTICITY_TYPE_ELIGIBILITY: priority_a = state_config->eligibility_priority; break;
        case PLASTICITY_TYPE_DENDRITIC: priority_a = state_config->dendritic_priority; break;
        case PLASTICITY_TYPE_STP: priority_a = state_config->stp_priority; break;
        case PLASTICITY_TYPE_ADAPTIVE: priority_a = state_config->adaptive_priority; break;
        case PLASTICITY_TYPE_PREDICTIVE: priority_a = state_config->predictive_priority; break;
        default: break;
    }

    switch (type_b) {
        case PLASTICITY_TYPE_STDP: priority_b = state_config->stdp_priority; break;
        case PLASTICITY_TYPE_BCM: priority_b = state_config->bcm_priority; break;
        case PLASTICITY_TYPE_HOMEOSTATIC: priority_b = state_config->homeostatic_priority; break;
        case PLASTICITY_TYPE_ELIGIBILITY: priority_b = state_config->eligibility_priority; break;
        case PLASTICITY_TYPE_DENDRITIC: priority_b = state_config->dendritic_priority; break;
        case PLASTICITY_TYPE_STP: priority_b = state_config->stp_priority; break;
        case PLASTICITY_TYPE_ADAPTIVE: priority_b = state_config->adaptive_priority; break;
        case PLASTICITY_TYPE_PREDICTIVE: priority_b = state_config->predictive_priority; break;
        default: break;
    }

    /* Apply resolution strategy */
    switch (coordinator->config.conflict_strategy) {
        case CONFLICT_RESOLUTION_STDP_DOMINANT:
            resolved = (type_a == PLASTICITY_TYPE_STDP) ? weight_change_a : weight_change_b;
            break;

        case CONFLICT_RESOLUTION_BCM_DOMINANT:
            resolved = (type_a == PLASTICITY_TYPE_BCM) ? weight_change_a : weight_change_b;
            break;

        case CONFLICT_RESOLUTION_AVERAGE:
            resolved = (weight_change_a + weight_change_b) / 2.0f;
            break;

        case CONFLICT_RESOLUTION_WEIGHTED_AVERAGE:
            resolved = (weight_change_a * priority_a + weight_change_b * priority_b) /
                       (priority_a + priority_b);
            break;

        case CONFLICT_RESOLUTION_IMMUNE_MODULATED:
            /* TODO: Get immune modulation factor */
            resolved = (weight_change_a * priority_a + weight_change_b * priority_b) /
                       (priority_a + priority_b);
            break;

        case CONFLICT_RESOLUTION_ENERGY_LIMITED:
            /* Choose lower energy mechanism */
            resolved = (priority_a < priority_b) ? weight_change_a : weight_change_b;
            break;

        default:
            resolved = (weight_change_a + weight_change_b) / 2.0f;
            break;
    }

    *resolved_change_out = resolved;

    /* Record conflict if statistics enabled */
    if (coordinator->config.enable_statistics &&
        coordinator->stats.recent_conflict_count <
        PLASTICITY_COORDINATOR_MAX_CONFLICTS) {

        plasticity_conflict_event_t* event =
            &coordinator->stats.recent_conflicts[coordinator->stats.recent_conflict_count++];

        event->synapse_id = synapse_id;
        event->mechanism_a = type_a;
        event->mechanism_b = type_b;
        event->weight_change_a = weight_change_a;
        event->weight_change_b = weight_change_b;
        event->resolved_weight_change = resolved;
        event->strategy_used = coordinator->config.conflict_strategy;
        event->timestamp = get_current_time_ms();
    }

    coordinator->stats.total_conflicts++;
    coordinator->stats.conflicts_resolved++;

    /* Update mechanism stats */
    coordinator->stats.mechanism_stats[type_a].conflicts_participated++;
    coordinator->stats.mechanism_stats[type_b].conflicts_participated++;

    nimcp_platform_mutex_unlock(coordinator->mutex);

    return 0;
}

int plasticity_coordinator_set_conflict_strategy(
    plasticity_coordinator_t* coordinator,
    conflict_resolution_strategy_t strategy
) {
    /* Guard clauses */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in set_conflict_strategy");
        return -1;
    }
    if (strategy >= CONFLICT_RESOLUTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED, "Invalid conflict strategy: %d", strategy);
        return -1;
    }

    coordinator->config.conflict_strategy = strategy;

    if (coordinator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Conflict strategy set to: %s",
            conflict_resolution_strategy_to_string(strategy));
    }

    return 0;
}

int plasticity_coordinator_set_plasticity_bridge(
    plasticity_coordinator_t* coordinator,
    tpb_context_t* tpb
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "plasticity_coordinator_set_plasticity_bridge: coordinator is NULL");
        return -1;
    }

    /* Pair with the __ATOMIC_ACQUIRE load in plasticity_coordinator_update().
     * Mutex is still held so the config-side invariants are consistent, but
     * the store itself is atomic-release so readers that skip the lock (the
     * update loop) see a coherent pointer without mutex contention. */
    nimcp_platform_mutex_lock(coordinator->mutex);
    __atomic_store_n(&coordinator->plasticity_bridge, tpb, __ATOMIC_RELEASE);
    __atomic_store_n(&coordinator->backprop_gate_skip_count, 0U, __ATOMIC_RELAXED);
    nimcp_platform_mutex_unlock(coordinator->mutex);

    if (coordinator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Plasticity bridge %s — STDP/BCM/homeostatic will be "
                           "gated during backprop",
                           tpb ? "wired" : "cleared");
    }
    return 0;
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int plasticity_coordinator_connect_brain_immune(
    plasticity_coordinator_t* coordinator,
    brain_immune_system_t* immune
) {
    /* Guard clauses */
    if (!coordinator || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in connect_brain_immune");
        return -1;
    }

    coordinator->brain_immune = immune;
    coordinator->immune_connected = true;

    NIMCP_LOGGING_INFO("Connected to brain immune system");
    return 0;
}

int plasticity_coordinator_disconnect_brain_immune(
    plasticity_coordinator_t* coordinator
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in disconnect_brain_immune");
        return -1;
    }

    coordinator->brain_immune = NULL;
    coordinator->immune_connected = false;

    NIMCP_LOGGING_INFO("Disconnected from brain immune system");
    return 0;
}

int plasticity_coordinator_connect_bio_async(
    plasticity_coordinator_t* coordinator
) {
    /* Guard clause */
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in connect_bio_async");
        return -1;
    }
    if (coordinator->bio_async_connected) return 0;

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = 0x0E00,  /* Plasticity coordinator module ID */
        .module_name = PLASTICITY_COORDINATOR_MODULE_NAME,
        .inbox_capacity = 64,
        .user_data = coordinator
    };

    coordinator->bio_context = bio_router_register_module(&info);
    if (coordinator->bio_context) {
        coordinator->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_coordinator_connect_bio_async: validation failed");
    return -1;
}

int plasticity_coordinator_disconnect_bio_async(
    plasticity_coordinator_t* coordinator
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Coordinator is NULL in disconnect_bio_async");
        return -1;
    }
    if (!coordinator->bio_async_connected) return 0;

    if (coordinator->bio_context) {
        bio_router_unregister_module(coordinator->bio_context);
        coordinator->bio_context = NULL;
    }

    coordinator->bio_async_connected = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

/* ============================================================================
 * Statistics and Monitoring API Implementation
 * ============================================================================ */

int plasticity_coordinator_get_stats(
    const plasticity_coordinator_t* coordinator,
    plasticity_coordinator_stats_t* stats
) {
    /* Guard clauses */
    if (!coordinator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in get_stats");
        return -1;
    }

    nimcp_platform_mutex_lock(coordinator->mutex);
    *stats = coordinator->stats;

    /* Count active mechanisms */
    stats->active_mechanisms = 0;
    for (uint32_t i = 0; i < coordinator->mechanism_count; i++) {
        if (coordinator->mechanisms[i].enabled) {
            stats->active_mechanisms++;
        }
    }

    nimcp_platform_mutex_unlock(coordinator->mutex);
    return 0;
}

void plasticity_coordinator_reset_stats(plasticity_coordinator_t* coordinator) {
    if (!coordinator) return;

    nimcp_platform_mutex_lock(coordinator->mutex);
    memset(&coordinator->stats, 0, sizeof(plasticity_coordinator_stats_t));
    coordinator->stats.current_state = coordinator->state;
    nimcp_platform_mutex_unlock(coordinator->mutex);

    NIMCP_LOGGING_INFO("Statistics reset");
}

float plasticity_coordinator_get_energy_rate(
    const plasticity_coordinator_t* coordinator
) {
    if (!coordinator) return 0.0f;
    return coordinator->stats.current_energy_rate;
}

bool plasticity_coordinator_is_low_energy(
    const plasticity_coordinator_t* coordinator
) {
    if (!coordinator || !coordinator->config.enable_energy_tracking) {
        return false;
    }

    return coordinator->stats.current_energy_rate >
           coordinator->config.low_energy_threshold;
}

/* ============================================================================
 * String Conversion API Implementation
 * ============================================================================ */

const char* plasticity_mechanism_type_to_string(plasticity_mechanism_type_t type) {
    switch (type) {
        case PLASTICITY_TYPE_STDP: return "STDP";
        case PLASTICITY_TYPE_BCM: return "BCM";
        case PLASTICITY_TYPE_HOMEOSTATIC: return "Homeostatic";
        case PLASTICITY_TYPE_ELIGIBILITY: return "Eligibility";
        case PLASTICITY_TYPE_DENDRITIC: return "Dendritic";
        case PLASTICITY_TYPE_STP: return "STP";
        case PLASTICITY_TYPE_ADAPTIVE: return "Adaptive";
        case PLASTICITY_TYPE_PREDICTIVE: return "Predictive";
        default: return "Unknown";
    }
}

const char* plasticity_coordinator_state_to_string(
    plasticity_coordinator_state_t state
) {
    switch (state) {
        case PLASTICITY_STATE_ACQUISITION: return "ACQUISITION";
        case PLASTICITY_STATE_CONSOLIDATION: return "CONSOLIDATION";
        case PLASTICITY_STATE_MAINTENANCE: return "MAINTENANCE";
        case PLASTICITY_STATE_STABILIZING: return "STABILIZING";
        default: return "UNKNOWN";
    }
}

const char* conflict_resolution_strategy_to_string(
    conflict_resolution_strategy_t strategy
) {
    switch (strategy) {
        case CONFLICT_RESOLUTION_STDP_DOMINANT: return "STDP_DOMINANT";
        case CONFLICT_RESOLUTION_BCM_DOMINANT: return "BCM_DOMINANT";
        case CONFLICT_RESOLUTION_AVERAGE: return "AVERAGE";
        case CONFLICT_RESOLUTION_WEIGHTED_AVERAGE: return "WEIGHTED_AVERAGE";
        case CONFLICT_RESOLUTION_IMMUNE_MODULATED: return "IMMUNE_MODULATED";
        case CONFLICT_RESOLUTION_ENERGY_LIMITED: return "ENERGY_LIMITED";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * KG Reader Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allow plasticity coordinator to introspect its own capabilities and connections
 * WHY:  Self-awareness enables adaptive behavior and system introspection
 * HOW:  Query KG for Plasticity_Coordinator entity and its relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 if not found or error
 */
int plasticity_coordinator_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    const kg_entity_t* self = kg_reader_get_entity(kg, "Plasticity_Coordinator");
    if (self) {
        /* Coordinator now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Plasticity Coordinator self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand what mechanisms we coordinate */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Plasticity_Coordinator");
    if (connections) {
        NIMCP_LOGGING_DEBUG("Plasticity Coordinator has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections to understand what depends on us */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Plasticity_Coordinator");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("Plasticity Coordinator has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
