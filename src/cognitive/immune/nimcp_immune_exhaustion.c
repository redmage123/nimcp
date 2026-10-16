/**
 * @file nimcp_immune_exhaustion.c
 * @brief Immune Exhaustion Modeling Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implements T cell exhaustion tracking and recovery
 * WHY:  Models biological immune fatigue during chronic responses
 * HOW:  Tracks activation time, updates markers/capacity, manages recovery
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_immune_exhaustion.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(immune_exhaustion, MESH_ADAPTER_CATEGORY_SECURITY)



/* Mutex convenience macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* Helper macros */
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define HOURS_TO_MS(h) ((uint64_t)(h) * 3600000ULL)
#define MS_TO_HOURS(ms) ((float)(ms) / 3600000.0f)

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static exhaustion_cell_record_t* find_cell_record(
    exhaustion_system_t* system,
    uint32_t t_cell_id
);
static exhaustion_cell_record_t* get_or_create_cell_record(
    exhaustion_system_t* system,
    uint32_t t_cell_id
);
static void update_cell_exhaustion(
    exhaustion_system_t* system,
    exhaustion_cell_record_t* cell,
    uint64_t delta_ms
);
static void update_exhaustion_markers(
    exhaustion_cell_record_t* cell,
    float hours_elapsed
);
static void update_functional_capacity(
    exhaustion_cell_record_t* cell,
    float hours_elapsed
);
static void check_state_transition(
    exhaustion_system_t* system,
    exhaustion_cell_record_t* cell
);
static void update_recovery_progress(
    exhaustion_system_t* system,
    exhaustion_cell_record_t* cell,
    uint64_t delta_ms
);
static void update_system_stats(exhaustion_system_t* system);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Convert exhaustion state to string
 */
const char* exhaustion_state_to_string(exhaustion_state_t state) {
    switch (state) {
        case EXHAUSTION_STATE_NAIVE:      return "NAIVE";
        case EXHAUSTION_STATE_EFFECTOR:   return "EFFECTOR";
        case EXHAUSTION_STATE_MEMORY:     return "MEMORY";
        case EXHAUSTION_STATE_EXHAUSTED:  return "EXHAUSTED";
        case EXHAUSTION_STATE_TERMINAL:   return "TERMINAL";
        case EXHAUSTION_STATE_RECOVERING: return "RECOVERING";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert recovery strategy to string
 */
const char* exhaustion_recovery_strategy_to_string(exhaustion_recovery_strategy_t strategy) {
    switch (strategy) {
        case RECOVERY_STRATEGY_NATURAL:     return "NATURAL";
        case RECOVERY_STRATEGY_CHECKPOINT:  return "CHECKPOINT_BLOCKADE";
        case RECOVERY_STRATEGY_COMBINED:    return "COMBINED";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Find cell record by T cell ID
 */
static exhaustion_cell_record_t* find_cell_record(
    exhaustion_system_t* system,
    uint32_t t_cell_id
) {
    if (!system || !system->cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cell_record: required parameter is NULL (system, system->cells)");
        return NULL;
    }

    for (size_t i = 0; i < system->cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cell_count > 256) {
            immune_exhaustion_heartbeat("immune_exhau_loop",
                             (float)(i + 1) / (float)system->cell_count);
        }

        if (system->cells[i].active && system->cells[i].t_cell_id == t_cell_id) {
            return &system->cells[i];
        }
    }
    return NULL;
}

/**
 * @brief Get or create cell record
 *
 * WHAT: Find existing record or allocate new one
 * WHY:  Auto-track T cells as they become active
 * HOW:  Search array, reuse inactive slot or expand
 */
static exhaustion_cell_record_t* get_or_create_cell_record(
    exhaustion_system_t* system,
    uint32_t t_cell_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Check if already tracking */
    exhaustion_cell_record_t* existing = find_cell_record(system, t_cell_id);
    if (existing) return existing;

    /* Find inactive slot */
    for (size_t i = 0; i < system->cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cell_count > 256) {
            immune_exhaustion_heartbeat("immune_exhau_loop",
                             (float)(i + 1) / (float)system->cell_count);
        }

        if (!system->cells[i].active) {
            memset(&system->cells[i], 0, sizeof(exhaustion_cell_record_t));
            system->cells[i].active = true;
            system->cells[i].t_cell_id = t_cell_id;
            system->cells[i].state = EXHAUSTION_STATE_NAIVE;
            system->cells[i].capacity.overall_capacity = 1.0f;
            system->cells[i].capacity.il2_production = 1.0f;
            system->cells[i].capacity.tnf_production = 1.0f;
            system->cells[i].capacity.ifng_production = 1.0f;
            system->cells[i].capacity.killing_capacity = 1.0f;
            return &system->cells[i];
        }
    }

    /* Need to expand array */
    if (system->cell_count >= system->cell_capacity) {
        NIMCP_LOGGING_WARN("Exhaustion tracking capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_or_create_cell_record: capacity exceeded");
        return NULL;
    }

    exhaustion_cell_record_t* cell = &system->cells[system->cell_count++];
    memset(cell, 0, sizeof(exhaustion_cell_record_t));
    cell->active = true;
    cell->t_cell_id = t_cell_id;
    cell->state = EXHAUSTION_STATE_NAIVE;
    cell->capacity.overall_capacity = 1.0f;
    cell->capacity.il2_production = 1.0f;
    cell->capacity.tnf_production = 1.0f;
    cell->capacity.ifng_production = 1.0f;
    cell->capacity.killing_capacity = 1.0f;
    return cell;
}

/**
 * @brief Update exhaustion markers based on activation time
 *
 * BIOLOGICAL BASIS:
 * PD-1, LAG-3, and TIM-3 accumulate progressively during chronic activation.
 * PD-1 rises fastest, TIM-3 rises slowest.
 */
static void update_exhaustion_markers(
    exhaustion_cell_record_t* cell,
    float hours_elapsed
) {
    if (!cell || hours_elapsed <= 0.0f) return;

    /* Progressive marker accumulation */
    cell->markers.pd1_level += hours_elapsed * 0.05f;   /* 5% per hour */
    cell->markers.lag3_level += hours_elapsed * 0.03f;  /* 3% per hour */
    cell->markers.tim3_level += hours_elapsed * 0.02f;  /* 2% per hour */

    /* Clamp to [0.0, 1.0] */
    cell->markers.pd1_level = CLAMP(cell->markers.pd1_level, 0.0f, 1.0f);
    cell->markers.lag3_level = CLAMP(cell->markers.lag3_level, 0.0f, 1.0f);
    cell->markers.tim3_level = CLAMP(cell->markers.tim3_level, 0.0f, 1.0f);

    /* Composite score: weighted average (PD-1 most important) */
    cell->markers.composite =
        0.5f * cell->markers.pd1_level +
        0.3f * cell->markers.lag3_level +
        0.2f * cell->markers.tim3_level;
}

/**
 * @brief Update functional capacity based on activation time
 *
 * BIOLOGICAL BASIS:
 * IL-2 production is lost first (proliferation defect)
 * Then TNF-α (co-stimulation defect)
 * Then IFN-γ (effector function defect)
 * Killing capacity depends on IFN-γ
 */
static void update_functional_capacity(
    exhaustion_cell_record_t* cell,
    float hours_elapsed
) {
    if (!cell || hours_elapsed <= 0.0f) return;

    /* Progressive functional decline (hierarchical loss) */
    cell->capacity.il2_production -= hours_elapsed * 0.08f;   /* 8% per hour - lost first */
    cell->capacity.tnf_production -= hours_elapsed * 0.05f;   /* 5% per hour - lost second */
    cell->capacity.ifng_production -= hours_elapsed * 0.03f;  /* 3% per hour - lost last */

    /* Clamp to [0.0, 1.0] */
    cell->capacity.il2_production = CLAMP(cell->capacity.il2_production, 0.0f, 1.0f);
    cell->capacity.tnf_production = CLAMP(cell->capacity.tnf_production, 0.0f, 1.0f);
    cell->capacity.ifng_production = CLAMP(cell->capacity.ifng_production, 0.0f, 1.0f);

    /* Killing capacity depends on IFN-γ */
    cell->capacity.killing_capacity = cell->capacity.ifng_production * 0.8f +
                                      cell->capacity.tnf_production * 0.2f;

    /* Overall capacity: weighted average */
    cell->capacity.overall_capacity =
        0.3f * cell->capacity.il2_production +
        0.3f * cell->capacity.tnf_production +
        0.4f * cell->capacity.ifng_production;
}

/**
 * @brief Check and execute state transitions
 *
 * WHAT: Determine if cell should transition states
 * WHY:  Model progression from effector → exhausted → terminal
 * HOW:  Check activation duration against thresholds
 */
static void check_state_transition(
    exhaustion_system_t* system,
    exhaustion_cell_record_t* cell
) {
    if (!system || !cell || cell->recovering) return;

    exhaustion_state_t old_state = cell->state;
    exhaustion_state_t new_state = old_state;

    /* State transitions based on activation duration */
    if (cell->total_activation_ms >= system->config.terminal_exhaustion_threshold_ms) {
        new_state = EXHAUSTION_STATE_TERMINAL;
    } else if (cell->total_activation_ms >= system->config.advanced_exhaustion_threshold_ms) {
        new_state = EXHAUSTION_STATE_EXHAUSTED;
    } else if (cell->total_activation_ms >= system->config.early_exhaustion_threshold_ms) {
        /* Early exhaustion - begin transition */
        if (cell->capacity.overall_capacity < 0.7f) {
            new_state = EXHAUSTION_STATE_EXHAUSTED;
        }
    } else if (cell->total_activation_ms > 0) {
        new_state = EXHAUSTION_STATE_EFFECTOR;
    }

    /* Execute transition if changed */
    if (new_state != old_state) {
        cell->previous_state = old_state;
        cell->state = new_state;

        if (new_state == EXHAUSTION_STATE_EXHAUSTED ||
            new_state == EXHAUSTION_STATE_TERMINAL) {
            cell->exhaustion_cycles++;
            system->stats.total_exhaustion_events++;
        }

        /* Fire callback */
        if (system->on_exhaustion) {
            system->on_exhaustion(system, cell->t_cell_id, old_state, new_state,
                                  system->callback_user_data);
        }

        if (system->config.enable_logging) {
            NIMCP_LOGGING_INFO("T cell %u: %s -> %s (activation: %.1f hours)",
                cell->t_cell_id,
                exhaustion_state_to_string(old_state),
                exhaustion_state_to_string(new_state),
                MS_TO_HOURS(cell->total_activation_ms));
        }
    }
}

/**
 * @brief Update recovery progress
 *
 * WHAT: Process gradual recovery from exhaustion
 * WHY:  Restore functional capacity over time
 * HOW:  Increment recovery, restore capacity proportionally
 */
static void update_recovery_progress(
    exhaustion_system_t* system,
    exhaustion_cell_record_t* cell,
    uint64_t delta_ms
) {
    if (!system || !cell || !cell->recovering) return;

    uint64_t recovery_duration = system->config.natural_recovery_duration_ms;
    if (cell->recovery_strategy == RECOVERY_STRATEGY_CHECKPOINT) {
        recovery_duration /= 2;  /* Checkpoint blockade is faster */
    }

    /* Update recovery progress */
    uint64_t elapsed = get_timestamp_ms() - cell->recovery_start_ms;
    cell->recovery_progress = (float)elapsed / (float)recovery_duration;
    cell->recovery_progress = CLAMP(cell->recovery_progress, 0.0f, 1.0f);

    /* Gradually restore capacity */
    float target_capacity = 0.0f;
    if (cell->recovery_strategy == RECOVERY_STRATEGY_CHECKPOINT) {
        target_capacity = system->config.checkpoint_blockade_efficacy;
    } else {
        target_capacity = 0.85f;  /* Natural recovery restores to 85% */
    }

    float current_capacity = cell->capacity.overall_capacity;
    float restored_capacity = current_capacity +
        (target_capacity - current_capacity) * cell->recovery_progress;

    /* Restore cytokine production proportionally */
    cell->capacity.il2_production = restored_capacity;
    cell->capacity.tnf_production = restored_capacity * 0.9f;
    cell->capacity.ifng_production = restored_capacity * 0.8f;
    cell->capacity.killing_capacity = cell->capacity.ifng_production;
    cell->capacity.overall_capacity = restored_capacity;

    /* Reduce exhaustion markers */
    cell->markers.pd1_level *= (1.0f - cell->recovery_progress * 0.6f);
    cell->markers.lag3_level *= (1.0f - cell->recovery_progress * 0.5f);
    cell->markers.tim3_level *= (1.0f - cell->recovery_progress * 0.4f);

    /* Recovery complete? */
    if (cell->recovery_progress >= 1.0f) {
        cell->recovering = false;
        cell->state = EXHAUSTION_STATE_MEMORY;
        cell->recovery_cycles++;
        system->stats.total_recovery_events++;

        if (system->on_recovery) {
            system->on_recovery(system, cell->t_cell_id,
                                cell->capacity.overall_capacity,
                                system->callback_user_data);
        }

        if (system->config.enable_logging) {
            NIMCP_LOGGING_INFO("T cell %u recovery complete: %.1f%% capacity restored",
                cell->t_cell_id, cell->capacity.overall_capacity * 100.0f);
        }
    }
}

/**
 * @brief Update cell exhaustion state
 */
static void update_cell_exhaustion(
    exhaustion_system_t* system,
    exhaustion_cell_record_t* cell,
    uint64_t delta_ms
) {
    if (!system || !cell || !cell->active) return;

    cell->last_update_ms = get_timestamp_ms();

    /* If recovering, process recovery */
    if (cell->recovering) {
        update_recovery_progress(system, cell, delta_ms);
        return;
    }

    /* Check if T cell is still active in immune system */
    const brain_t_cell_t* t_cell = NULL;
    for (size_t i = 0; i < system->immune_system->t_cell_count; i++) {
        if (system->immune_system->t_cells[i].id == cell->t_cell_id) {
            t_cell = &system->immune_system->t_cells[i];
            break;
        }
    }

    if (!t_cell || t_cell->activation_level < 0.1f) {
        /* T cell not active - no exhaustion progression */
        return;
    }

    /* Update activation time */
    cell->total_activation_ms += delta_ms;
    float hours_elapsed = (float)delta_ms / 3600000.0f;

    /* Update exhaustion markers and capacity */
    update_exhaustion_markers(cell, hours_elapsed);
    update_functional_capacity(cell, hours_elapsed);

    /* Check for state transitions */
    check_state_transition(system, cell);

    /* Auto-recovery if enabled and exhausted */
    if (system->config.allow_auto_recovery &&
        cell->state == EXHAUSTION_STATE_EXHAUSTED &&
        cell->capacity.overall_capacity < 0.3f) {
        exhaustion_initiate_recovery(system, cell->t_cell_id);
    }
}

/**
 * @brief Update system-wide statistics
 */
static void update_system_stats(exhaustion_system_t* system) {
    if (!system) return;

    /* Save cumulative counters before clearing stats */
    uint64_t saved_exhaustion = system->stats.total_exhaustion_events;
    uint64_t saved_recovery = system->stats.total_recovery_events;
    uint64_t saved_blockade = system->stats.checkpoint_blockade_uses;
    memset(&system->stats, 0, sizeof(exhaustion_stats_t));
    system->stats.total_exhaustion_events = saved_exhaustion;
    system->stats.total_recovery_events = saved_recovery;
    system->stats.checkpoint_blockade_uses = saved_blockade;

    float total_capacity = 0.0f;
    float total_activation_ms = 0.0f;
    uint32_t active_count = 0;

    for (size_t i = 0; i < system->cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cell_count > 256) {
            immune_exhaustion_heartbeat("immune_exhau_loop",
                             (float)(i + 1) / (float)system->cell_count);
        }

        exhaustion_cell_record_t* cell = &system->cells[i];
        if (!cell->active) continue;

        active_count++;
        total_capacity += cell->capacity.overall_capacity;
        total_activation_ms += (float)cell->total_activation_ms;

        /* Count by state */
        switch (cell->state) {
            case EXHAUSTION_STATE_NAIVE:      system->stats.naive_cells++; break;
            case EXHAUSTION_STATE_EFFECTOR:   system->stats.effector_cells++; break;
            case EXHAUSTION_STATE_MEMORY:     system->stats.memory_cells++; break;
            case EXHAUSTION_STATE_EXHAUSTED:  system->stats.exhausted_cells++; break;
            case EXHAUSTION_STATE_TERMINAL:   system->stats.terminal_cells++; break;
            case EXHAUSTION_STATE_RECOVERING: system->stats.recovering_cells++; break;
        }
    }

    /* Compute averages */
    if (active_count > 0) {
        system->stats.avg_effector_capacity = total_capacity / (float)active_count;
        system->stats.avg_activation_duration_ms = total_activation_ms / (float)active_count;

        /* System fatigue: weighted by exhaustion severity */
        float fatigue_score =
            (system->stats.exhausted_cells * 0.7f +
             system->stats.terminal_cells * 1.0f +
             system->stats.recovering_cells * 0.4f) / (float)active_count;
        system->stats.system_fatigue = CLAMP(fatigue_score, 0.0f, 1.0f);
    } else {
        system->stats.avg_effector_capacity = 1.0f;
        system->stats.system_fatigue = 0.0f;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int exhaustion_default_config(exhaustion_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_default_c", 0.0f);


    memset(config, 0, sizeof(exhaustion_config_t));

    /* Thresholds */
    config->early_exhaustion_threshold_ms = HOURS_TO_MS(6);     /* 6 hours */
    config->advanced_exhaustion_threshold_ms = HOURS_TO_MS(24); /* 24 hours */
    config->terminal_exhaustion_threshold_ms = HOURS_TO_MS(48); /* 48 hours */

    /* Recovery */
    config->natural_recovery_duration_ms = HOURS_TO_MS(12);     /* 12 hours */
    config->checkpoint_blockade_efficacy = 0.65f;               /* 65% restoration */
    config->allow_auto_recovery = false;

    /* Decline rates (per hour) */
    config->il2_decline_rate = 0.08f;
    config->tnf_decline_rate = 0.05f;
    config->ifng_decline_rate = 0.03f;

    /* Accumulation rates (per hour) */
    config->pd1_accumulation_rate = 0.05f;
    config->lag3_accumulation_rate = 0.03f;
    config->tim3_accumulation_rate = 0.02f;

    /* System */
    config->max_tracked_cells = EXHAUSTION_MAX_CELLS;
    config->enable_logging = true;

    return 0;
}

/**
 * @brief Create exhaustion tracking system
 */
exhaustion_system_t* exhaustion_create(
    const exhaustion_config_t* config,
    brain_immune_system_t* immune_system
) {
    if (!immune_system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_create", 0.0f);


    exhaustion_system_t* system = (exhaustion_system_t*)nimcp_malloc(
        sizeof(exhaustion_system_t)
    );
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    memset(system, 0, sizeof(exhaustion_system_t));

    /* Apply config */
    if (config) {
        memcpy(&system->config, config, sizeof(exhaustion_config_t));
    } else {
        exhaustion_default_config(&system->config);
    }

    /* Link to immune system */
    system->immune_system = immune_system;

    /* Allocate cell tracking array */
    system->cell_capacity = system->config.max_tracked_cells;
    system->cells = (exhaustion_cell_record_t*)nimcp_malloc(
        system->cell_capacity * sizeof(exhaustion_cell_record_t)
    );
    if (!system->cells) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "exhaustion_create: system->cells is NULL");
        return NULL;
    }
    memset(system->cells, 0, system->cell_capacity * sizeof(exhaustion_cell_record_t));

    /* Create mutex */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) {
        nimcp_free(system->cells);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "exhaustion_create: system->mutex is NULL");
        return NULL;
    }

    system->running = true;
    system->start_time_ms = get_timestamp_ms();

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Exhaustion tracking system created (max cells: %zu)",
            system->cell_capacity);
    }

    return system;
}

/**
 * @brief Destroy exhaustion system
 */
void exhaustion_destroy(exhaustion_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_destroy", 0.0f);


    system->running = false;

    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
        system->mutex = NULL;
    }

    if (system->cells) {
        nimcp_free(system->cells);
        system->cells = NULL;
    }

    nimcp_free(system);
    system = NULL;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update exhaustion state for all tracked cells
 */
int exhaustion_update(
    exhaustion_system_t* system,
    uint64_t delta_ms
) {
    if (!system || !system->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_update: required parameter is NULL (system, system->running)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_update", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Update each tracked cell */
    for (size_t i = 0; i < system->cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cell_count > 256) {
            immune_exhaustion_heartbeat("immune_exhau_loop",
                             (float)(i + 1) / (float)system->cell_count);
        }

        if (system->cells[i].active) {
            update_cell_exhaustion(system, &system->cells[i], delta_ms);
        }
    }

    /* Update system statistics */
    update_system_stats(system);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get exhaustion state for a T cell
 */
exhaustion_state_t exhaustion_get_cell_state(
    exhaustion_system_t* system,
    uint32_t t_cell_id
) {
    if (!system) return EXHAUSTION_STATE_NAIVE;

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_get_cell_", 0.0f);


    nimcp_mutex_lock(system->mutex);
    exhaustion_cell_record_t* cell = find_cell_record(system, t_cell_id);
    exhaustion_state_t state = cell ? cell->state : EXHAUSTION_STATE_NAIVE;
    nimcp_mutex_unlock(system->mutex);

    return state;
}

/**
 * @brief Get effector capacity for a T cell
 */
float exhaustion_get_effector_capacity(
    exhaustion_system_t* system,
    uint32_t t_cell_id
) {
    if (!system) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_get_effec", 0.0f);


    nimcp_mutex_lock(system->mutex);
    exhaustion_cell_record_t* cell = find_cell_record(system, t_cell_id);
    float capacity = cell ? cell->capacity.overall_capacity : 1.0f;
    nimcp_mutex_unlock(system->mutex);

    return capacity;
}

/**
 * @brief Get exhaustion markers for a T cell
 */
int exhaustion_get_markers(
    exhaustion_system_t* system,
    uint32_t t_cell_id,
    exhaustion_markers_t* markers
) {
    if (!system || !markers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_get_markers: required parameter is NULL (system, markers)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_get_marke", 0.0f);


    nimcp_mutex_lock(system->mutex);
    exhaustion_cell_record_t* cell = find_cell_record(system, t_cell_id);

    if (cell) {
        memcpy(markers, &cell->markers, sizeof(exhaustion_markers_t));
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "exhaustion_get_markers: validation failed");
    return -1;
}

/**
 * @brief Get system-wide fatigue level
 */
float exhaustion_get_system_fatigue(exhaustion_system_t* system) {
    if (!system) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_get_syste", 0.0f);


    nimcp_mutex_lock(system->mutex);
    float fatigue = system->stats.system_fatigue;
    nimcp_mutex_unlock(system->mutex);

    return fatigue;
}

/**
 * @brief Get exhaustion statistics
 */
int exhaustion_get_stats(
    exhaustion_system_t* system,
    exhaustion_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_get_stats", 0.0f);


    nimcp_mutex_lock(system->mutex);
    memcpy(stats, &system->stats, sizeof(exhaustion_stats_t));
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Recovery API
 * ============================================================================ */

/**
 * @brief Initiate natural recovery for a T cell
 */
int exhaustion_initiate_recovery(
    exhaustion_system_t* system,
    uint32_t t_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_initiate_recovery: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_initiate_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    exhaustion_cell_record_t* cell = get_or_create_cell_record(system, t_cell_id);
    if (!cell) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_initiate_recovery: cell is NULL");
        return -1;
    }

    /* Can only recover if exhausted or terminal */
    if (cell->state != EXHAUSTION_STATE_EXHAUSTED &&
        cell->state != EXHAUSTION_STATE_TERMINAL) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_initiate_recovery: cell is NULL");
        return -1;
    }

    /* Initiate recovery */
    cell->recovering = true;
    cell->recovery_strategy = RECOVERY_STRATEGY_NATURAL;
    cell->recovery_start_ms = get_timestamp_ms();
    cell->recovery_progress = 0.0f;
    cell->state = EXHAUSTION_STATE_RECOVERING;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Natural recovery initiated for T cell %u", t_cell_id);
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Apply checkpoint blockade therapy
 */
int exhaustion_checkpoint_blockade(
    exhaustion_system_t* system,
    uint32_t t_cell_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_checkpoint_blockade: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_checkpoin", 0.0f);


    nimcp_mutex_lock(system->mutex);

    exhaustion_cell_record_t* cell = get_or_create_cell_record(system, t_cell_id);
    if (!cell) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_checkpoint_blockade: cell is NULL");
        return -1;
    }

    /* Can only treat if exhausted or terminal */
    if (cell->state != EXHAUSTION_STATE_EXHAUSTED &&
        cell->state != EXHAUSTION_STATE_TERMINAL) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_checkpoint_blockade: cell is NULL");
        return -1;
    }

    /* Apply checkpoint blockade */
    cell->recovering = true;
    cell->recovery_strategy = RECOVERY_STRATEGY_CHECKPOINT;
    cell->recovery_start_ms = get_timestamp_ms();
    cell->recovery_progress = 0.0f;
    cell->state = EXHAUSTION_STATE_RECOVERING;

    /* Immediate partial effect: reduce PD-1 */
    cell->markers.pd1_level *= 0.4f;  /* 60% reduction in PD-1 */

    system->stats.checkpoint_blockade_uses++;

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Checkpoint blockade applied to T cell %u", t_cell_id);
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set exhaustion event callback
 */
int exhaustion_set_exhaustion_callback(
    exhaustion_system_t* system,
    exhaustion_event_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_set_exhaustion_callback: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_set_exhau", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_exhaustion = callback;
    system->callback_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

/**
 * @brief Set recovery completion callback
 */
int exhaustion_set_recovery_callback(
    exhaustion_system_t* system,
    exhaustion_recovery_cb_t callback,
    void* user_data
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exhaustion_set_recovery_callback: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_set_recov", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->on_recovery = callback;
    system->callback_user_data = user_data;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about immune exhaustion
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int exhaustion_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    immune_exhaustion_heartbeat("immune_exhau_exhaustion_query_sel", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Immune_Exhaustion");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                immune_exhaustion_heartbeat("immune_exhau_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Immune exhaustion self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Immune_Exhaustion");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Immune_Exhaustion");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void immune_exhaustion_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_immune_exhaustion_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int immune_exhaustion_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_exhaustion_training_begin: NULL argument");
        return -1;
    }
    immune_exhaustion_heartbeat_instance(NULL, "immune_exhaustion_training_begin", 0.0f);
    return 0;
}

int immune_exhaustion_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_exhaustion_training_end: NULL argument");
        return -1;
    }
    immune_exhaustion_heartbeat_instance(NULL, "immune_exhaustion_training_end", 1.0f);
    return 0;
}

int immune_exhaustion_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_exhaustion_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    immune_exhaustion_heartbeat_instance(NULL, "immune_exhaustion_training_step", progress);
    return 0;
}
