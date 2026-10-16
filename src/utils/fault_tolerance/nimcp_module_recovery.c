/**
 * @file nimcp_module_recovery.c
 * @brief Module-Specific Recovery Actions Implementation
 *
 * PHASE 8: System-Wide Health Integration
 * Implements graduated recovery strategies for individual modules.
 *
 * @author NIMCP Team
 * @date 2026-01-20
 */

#include "utils/fault_tolerance/nimcp_module_recovery.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

/* Module headers for built-in recovery functions */
#include "plasticity/stdp/nimcp_stdp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Forward declare state reset functions (defined in respective .c files) */
extern int astrocyte_network_state_reset(void* module_state);

#include <string.h>
#include <math.h>

#define LOG_MODULE "module_recovery"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(module_recovery)

//=============================================================================
// Module Recovery Manager Implementation
//=============================================================================

nimcp_module_recovery_manager_t* nimcp_module_recovery_manager_create(void) {
    nimcp_module_recovery_manager_t* manager = nimcp_calloc(1, sizeof(nimcp_module_recovery_manager_t));
    if (!manager) {
        LOG_ERROR("Failed to allocate module recovery manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "manager is NULL");

        return NULL;
    }

    manager->magic = NIMCP_MODULE_RECOVERY_MAGIC;
    manager->module_count = 0;
    manager->initialized = true;

    /* Default configuration */
    manager->health_threshold = 0.7f;  /* Trigger recovery below 70% health */
    manager->auto_escalate = true;
    manager->max_escalation_level = NIMCP_MODULE_RECOVERY_FULL;

    /* Create mutex */
    mutex_attr_t attr = { .type = MUTEX_TYPE_NORMAL };
    manager->mutex = nimcp_mutex_create(&attr);
    if (!manager->mutex) {
        LOG_ERROR("Failed to create mutex for module recovery manager");
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_module_recovery_manager_create: manager->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Module recovery manager created");
    return manager;
}

void nimcp_module_recovery_manager_destroy(nimcp_module_recovery_manager_t* manager) {
    if (!manager) return;

    if (manager->mutex) {
        nimcp_mutex_free(manager->mutex);
    }

    nimcp_free(manager);
    LOG_INFO("Module recovery manager destroyed");
}

//=============================================================================
// Module Registration
//=============================================================================

/**
 * @brief Find module entry by name (internal, assumes lock held)
 */
static nimcp_module_recovery_entry_t* find_module_unlocked(
    nimcp_module_recovery_manager_t* manager,
    const char* name
) {
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strcmp(manager->modules[i].name, name) == 0) {
            return &manager->modules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_module_unlocked: validation failed");
    return NULL;
}

int nimcp_module_recovery_register(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    const nimcp_module_recovery_ops_t* ops,
    void* state
) {
    if (!manager || !name || !ops) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    /* Reject empty module name */
    if (name[0] == '\0') {
        LOG_WARN("Empty module name rejected");
        return -NIMCP_ERROR_INVALID_PARAM;
    }

    /* Require at least recover callback (essential for recovery) */
    if (!ops->recover) {
        LOG_WARN("Module '%s' has no recover callback", name);
        return -NIMCP_ERROR_INVALID_PARAM;
    }

    if (!manager->initialized || manager->magic != NIMCP_MODULE_RECOVERY_MAGIC) {
        return -NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(manager->mutex);

    /* Check for duplicate */
    if (find_module_unlocked(manager, name)) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_WARN("Module '%s' already registered for recovery", name);
        return -NIMCP_ERROR_ALREADY_EXISTS;
    }

    /* Check capacity */
    if (manager->module_count >= NIMCP_MODULE_RECOVERY_MAX_MODULES) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_ERROR("Module recovery registry full");
        return -NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Add module */
    nimcp_module_recovery_entry_t* entry = &manager->modules[manager->module_count];
    memset(entry, 0, sizeof(*entry));

    strncpy(entry->name, name, NIMCP_MODULE_RECOVERY_MAX_NAME_LEN - 1);
    entry->name[NIMCP_MODULE_RECOVERY_MAX_NAME_LEN - 1] = '\0';
    entry->ops = *ops;
    entry->state = state;
    entry->enabled = true;
    entry->isolated = false;
    entry->last_health_score = 1.0f;

    manager->module_count++;

    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO("Registered module '%s' for recovery", name);
    return NIMCP_SUCCESS;
}

int nimcp_module_recovery_unregister(
    nimcp_module_recovery_manager_t* manager,
    const char* name
) {
    if (!manager || !name) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    /* Shift remaining entries */
    size_t idx = entry - manager->modules;
    if (idx < manager->module_count - 1) {
        memmove(&manager->modules[idx], &manager->modules[idx + 1],
                (manager->module_count - idx - 1) * sizeof(nimcp_module_recovery_entry_t));
    }
    manager->module_count--;

    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO("Unregistered module '%s' from recovery", name);
    return NIMCP_SUCCESS;
}

int nimcp_module_recovery_set_enabled(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    bool enabled
) {
    if (!manager || !name) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    entry->enabled = enabled;

    nimcp_mutex_unlock(manager->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Recovery Execution
//=============================================================================

nimcp_module_recovery_result_t nimcp_module_recovery_attempt(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    nimcp_module_recovery_level_t level
) {
    if (!manager || !name) {
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_WARN("Module '%s' not found for recovery", name);
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    if (!entry->enabled || entry->isolated) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_WARN("Module '%s' is disabled or isolated", name);
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    if (!entry->ops.recover) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_WARN("Module '%s' has no recovery function", name);
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    nimcp_module_recovery_level_t current_level = level;
    nimcp_module_recovery_result_t result = NIMCP_MODULE_RECOVERY_FAILED;

    /* ISOLATE level always allowed (it's deliberate, not escalation) */
    nimcp_module_recovery_level_t effective_max =
        (level == NIMCP_MODULE_RECOVERY_ISOLATE)
            ? NIMCP_MODULE_RECOVERY_ISOLATE
            : (nimcp_module_recovery_level_t)manager->max_escalation_level;

    /* Graduated recovery with optional escalation */
    while (current_level <= effective_max) {
        LOG_INFO("Attempting recovery for '%s' at level %d", name, (int)current_level);

        entry->recovery_attempts++;
        entry->last_level = current_level;
        entry->last_recovery_time = nimcp_platform_time_monotonic_us();

        /* Call module's recovery function */
        result = entry->ops.recover(entry->state, current_level, entry->ops.user_data);

        if (result == NIMCP_MODULE_RECOVERY_SUCCESS) {
            entry->recovery_successes++;
            manager->total_recoveries++;
            LOG_INFO("Recovery successful for '%s' at level %d", name, (int)current_level);
            break;
        } else if (result == NIMCP_MODULE_RECOVERY_PARTIAL_SUCCESS) {
            entry->recovery_successes++;
            manager->total_recoveries++;
            LOG_WARN("Partial recovery for '%s' at level %d", name, (int)current_level);
            break;
        } else if (result == NIMCP_MODULE_RECOVERY_ESCALATE ||
                   (result == NIMCP_MODULE_RECOVERY_FAILED && manager->auto_escalate)) {
            entry->escalations++;
            current_level++;
            LOG_WARN("Escalating recovery for '%s' to level %d", name, (int)current_level);

            if (current_level > (nimcp_module_recovery_level_t)manager->max_escalation_level) {
                entry->recovery_failures++;
                LOG_ERROR("Recovery exhausted for '%s', max level reached", name);

                /* Isolate on max escalation */
                if (current_level > NIMCP_MODULE_RECOVERY_FULL) {
                    entry->isolated = true;
                    LOG_ERROR("Module '%s' isolated due to recovery failure", name);
                }
                result = NIMCP_MODULE_RECOVERY_FAILED;
                break;
            }
        } else {
            entry->recovery_failures++;
            LOG_ERROR("Recovery failed for '%s' at level %d", name, (int)current_level);
            break;
        }
    }

    nimcp_mutex_unlock(manager->mutex);
    return result;
}

int nimcp_module_recovery_attempt_all_unhealthy(
    nimcp_module_recovery_manager_t* manager
) {
    if (!manager) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;

    }

    nimcp_mutex_lock(manager->mutex);

    int recovered_count = 0;

    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_module_recovery_entry_t* entry = &manager->modules[i];

        if (!entry->enabled || entry->isolated) continue;

        /* Check health */
        float health = 1.0f;
        if (entry->ops.health_check) {
            entry->ops.health_check(entry->state, &health);
            entry->last_health_score = health;
            manager->total_health_checks++;
        }

        /* Recover if below threshold */
        if (health < manager->health_threshold) {
            LOG_INFO("Module '%s' health %.2f below threshold %.2f, attempting recovery",
                     entry->name, health, manager->health_threshold);

            /* Unlock during recovery to allow nested operations */
            char name_copy[NIMCP_MODULE_RECOVERY_MAX_NAME_LEN];
            strncpy(name_copy, entry->name, sizeof(name_copy) - 1);
            name_copy[sizeof(name_copy) - 1] = '\0';

            nimcp_mutex_unlock(manager->mutex);

            nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
                manager, name_copy, NIMCP_MODULE_RECOVERY_LIGHT);

            nimcp_mutex_lock(manager->mutex);

            if (result == NIMCP_MODULE_RECOVERY_SUCCESS ||
                result == NIMCP_MODULE_RECOVERY_PARTIAL_SUCCESS) {
                recovered_count++;
            }
        }
    }

    nimcp_mutex_unlock(manager->mutex);
    return recovered_count;
}

//=============================================================================
// Health Check
//=============================================================================

float nimcp_module_recovery_check_all_health(
    nimcp_module_recovery_manager_t* manager
) {
    if (!manager) return 0.0f;

    nimcp_mutex_lock(manager->mutex);

    float total_health = 0.0f;
    uint32_t checked_count = 0;

    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_module_recovery_entry_t* entry = &manager->modules[i];

        if (!entry->enabled || entry->isolated) continue;

        float health = 1.0f;
        if (entry->ops.health_check) {
            entry->ops.health_check(entry->state, &health);
            entry->last_health_score = health;
            manager->total_health_checks++;
        }

        total_health += health;
        checked_count++;
    }

    nimcp_mutex_unlock(manager->mutex);

    return checked_count > 0 ? total_health / checked_count : 1.0f;
}

int nimcp_module_recovery_check_health(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    float* out_health
) {
    if (!manager || !name || !out_health) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    if (!entry->ops.health_check) {
        *out_health = 1.0f;  /* Assume healthy if no check */
        nimcp_mutex_unlock(manager->mutex);
        return NIMCP_SUCCESS;
    }

    int result = entry->ops.health_check(entry->state, out_health);
    if (result == 0) {
        entry->last_health_score = *out_health;
        manager->total_health_checks++;
    }

    nimcp_mutex_unlock(manager->mutex);
    return result;
}

//=============================================================================
// Isolation
//=============================================================================

int nimcp_module_recovery_isolate(
    nimcp_module_recovery_manager_t* manager,
    const char* name
) {
    if (!manager || !name) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    entry->isolated = true;
    LOG_WARN("Module '%s' isolated", name);

    nimcp_mutex_unlock(manager->mutex);
    return NIMCP_SUCCESS;
}

int nimcp_module_recovery_restore(
    nimcp_module_recovery_manager_t* manager,
    const char* name
) {
    if (!manager || !name) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    entry->isolated = false;
    LOG_INFO("Module '%s' restored from isolation", name);

    nimcp_mutex_unlock(manager->mutex);
    return NIMCP_SUCCESS;
}

bool nimcp_module_recovery_is_isolated(
    nimcp_module_recovery_manager_t* manager,
    const char* name
) {
    if (!manager || !name) return true;  /* Fail safe */

    nimcp_mutex_lock(manager->mutex);

    nimcp_module_recovery_entry_t* entry = find_module_unlocked(manager, name);
    bool isolated = entry ? entry->isolated : true;

    nimcp_mutex_unlock(manager->mutex);
    return isolated;
}

//=============================================================================
// Statistics
//=============================================================================

int nimcp_module_recovery_get_stats(
    nimcp_module_recovery_manager_t* manager,
    nimcp_module_recovery_stats_t* stats
) {
    if (!manager || !stats) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    memset(stats, 0, sizeof(*stats));
    stats->module_count = manager->module_count;
    stats->total_recoveries = manager->total_recoveries;

    float total_health = 0.0f;

    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_module_recovery_entry_t* entry = &manager->modules[i];

        if (entry->enabled) stats->enabled_modules++;
        if (entry->isolated) stats->isolated_modules++;

        stats->total_successes += entry->recovery_successes;
        stats->total_failures += entry->recovery_failures;
        stats->total_escalations += entry->escalations;
        total_health += entry->last_health_score;
    }

    stats->average_health = manager->module_count > 0 ?
                            total_health / manager->module_count : 1.0f;

    nimcp_mutex_unlock(manager->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Built-in STDP Recovery Functions
//=============================================================================

nimcp_module_recovery_result_t nimcp_stdp_recovery(
    void* module_state,
    nimcp_module_recovery_level_t level,
    void* user_data
) {
    if (!module_state) {
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    stdp_synapse_t* synapse = (stdp_synapse_t*)module_state;
    (void)user_data;  /* Could be state manager for checkpoint restore */

    switch (level) {
        case NIMCP_MODULE_RECOVERY_LIGHT:
            /* Reset traces and fix corrupted values */
            nimcp_spinlock_lock(&synapse->lock);
            synapse->pre_trace = 0.0f;
            synapse->post_trace = 0.0f;
            /* Fix corrupted weight (NaN/Inf or out of bounds) */
            if (!isfinite(synapse->weight) ||
                synapse->weight < synapse->w_min ||
                synapse->weight > synapse->w_max) {
                synapse->weight = (synapse->w_min + synapse->w_max) / 2.0f;
            }
            /* Check if learning params need fixing - escalate if so */
            bool needs_escalation = !isfinite(synapse->learning_rate) ||
                synapse->learning_rate <= 0.0f ||
                !isfinite(synapse->a_plus) || synapse->a_plus < 0.0f ||
                !isfinite(synapse->a_minus) || synapse->a_minus < 0.0f ||
                !isfinite(synapse->tau_plus) || synapse->tau_plus <= 0.0f ||
                !isfinite(synapse->tau_minus) || synapse->tau_minus <= 0.0f;
            nimcp_spinlock_unlock(&synapse->lock);
            LOG_DEBUG("STDP light recovery: traces reset");
            return needs_escalation ? NIMCP_MODULE_RECOVERY_ESCALATE : NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_PARTIAL: {
            /* Reset traces and statistics */
            nimcp_spinlock_lock(&synapse->lock);
            synapse->pre_trace = 0.0f;
            synapse->post_trace = 0.0f;
            synapse->num_potentiation_events = 0;
            synapse->num_depression_events = 0;
            synapse->total_ltp = 0.0f;
            synapse->total_ltd = 0.0f;
            synapse->num_saturate_max_events = 0;
            synapse->num_saturate_min_events = 0;
            /* Fix corrupted weight */
            if (!isfinite(synapse->weight) ||
                synapse->weight < synapse->w_min ||
                synapse->weight > synapse->w_max) {
                synapse->weight = (synapse->w_min + synapse->w_max) / 2.0f;
            }
            /* Check if learning params need fixing - escalate if so */
            bool partial_needs_escalation = !isfinite(synapse->learning_rate) ||
                synapse->learning_rate <= 0.0f ||
                !isfinite(synapse->a_plus) || synapse->a_plus < 0.0f ||
                !isfinite(synapse->a_minus) || synapse->a_minus < 0.0f ||
                !isfinite(synapse->tau_plus) || synapse->tau_plus <= 0.0f ||
                !isfinite(synapse->tau_minus) || synapse->tau_minus <= 0.0f;
            nimcp_spinlock_unlock(&synapse->lock);
            LOG_DEBUG("STDP partial recovery: traces and stats reset");
            return partial_needs_escalation ? NIMCP_MODULE_RECOVERY_ESCALATE : NIMCP_MODULE_RECOVERY_SUCCESS;
        }

        case NIMCP_MODULE_RECOVERY_FULL:
            /* Full reset to defaults */
            stdp_synapse_reset(synapse);
            /* Reset weight and learning parameters to valid defaults */
            nimcp_spinlock_lock(&synapse->lock);
            synapse->weight = 0.5f * synapse->w_max;
            /* Fix corrupted learning parameters */
            if (!isfinite(synapse->learning_rate) || synapse->learning_rate <= 0.0f) {
                synapse->learning_rate = 0.01f;  /* Default learning rate */
            }
            if (!isfinite(synapse->a_plus) || synapse->a_plus < 0.0f) {
                synapse->a_plus = 0.005f;
            }
            if (!isfinite(synapse->a_minus) || synapse->a_minus < 0.0f) {
                synapse->a_minus = 0.00525f;
            }
            if (!isfinite(synapse->tau_plus) || synapse->tau_plus <= 0.0f) {
                synapse->tau_plus = 0.020f;
            }
            if (!isfinite(synapse->tau_minus) || synapse->tau_minus <= 0.0f) {
                synapse->tau_minus = 0.020f;
            }
            nimcp_spinlock_unlock(&synapse->lock);
            LOG_DEBUG("STDP full recovery: reset to defaults");
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_ISOLATE:
            /* Mark as non-functional - set weight to 0 */
            nimcp_spinlock_lock(&synapse->lock);
            synapse->weight = 0.0f;
            synapse->learning_rate = 0.0f;  /* Disable learning */
            nimcp_spinlock_unlock(&synapse->lock);
            LOG_WARN("STDP isolated: synapse disabled");
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        default:
            return NIMCP_MODULE_RECOVERY_FAILED;
    }
}

int nimcp_stdp_health_check(void* module_state, float* out_health) {
    if (!module_state || !out_health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stdp_health_check: required parameter is NULL (module_state, out_health)");
        return -1;
    }

    stdp_synapse_t* synapse = (stdp_synapse_t*)module_state;
    float health = 1.0f;

    nimcp_spinlock_lock(&synapse->lock);

    /* Weight in valid range: 40% */
    if (synapse->weight < synapse->w_min || synapse->weight > synapse->w_max) {
        health -= 0.4f;
    } else if (isnan(synapse->weight) || isinf(synapse->weight)) {
        health -= 0.4f;
    }

    /* Traces finite and reasonable: 30% */
    if (isnan(synapse->pre_trace) || isinf(synapse->pre_trace) ||
        isnan(synapse->post_trace) || isinf(synapse->post_trace)) {
        health -= 0.3f;
    } else if (synapse->pre_trace > 100.0f || synapse->post_trace > 100.0f) {
        health -= 0.15f;  /* Partial penalty for high traces */
    }

    /* No excessive saturation: 20% */
    uint64_t total_sat = synapse->num_saturate_max_events + synapse->num_saturate_min_events;
    uint64_t total_events = synapse->num_potentiation_events + synapse->num_depression_events;
    if (total_events > 0 && total_sat > total_events / 2) {
        health -= 0.2f;  /* >50% saturation is bad */
    }

    /* Parameters valid: 10% */
    if (synapse->learning_rate < 0.0f || synapse->tau_plus <= 0.0f ||
        synapse->tau_minus <= 0.0f) {
        health -= 0.1f;
    }

    nimcp_spinlock_unlock(&synapse->lock);

    *out_health = fmaxf(0.0f, fminf(1.0f, health));
    return 0;
}

const nimcp_module_recovery_ops_t* nimcp_stdp_get_recovery_ops(void) {
    static nimcp_module_recovery_ops_t ops = {
        .recover = nimcp_stdp_recovery,
        .health_check = nimcp_stdp_health_check,
        .user_data = NULL
    };
    return &ops;
}

//=============================================================================
// Built-in Astrocyte Recovery Functions
//=============================================================================

nimcp_module_recovery_result_t nimcp_astrocyte_recovery(
    void* module_state,
    nimcp_module_recovery_level_t level,
    void* user_data
) {
    if (!module_state) {
        return NIMCP_MODULE_RECOVERY_FAILED;
    }

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;
    (void)user_data;

    switch (level) {
        case NIMCP_MODULE_RECOVERY_LIGHT:
            /* Reset calcium to baseline, preserve other state */
            nimcp_mutex_lock(&network->lock);
            for (uint32_t i = 0; i < network->num_astrocytes; i++) {
                astrocyte_t* astro = network->astrocytes[i];
                if (!astro) continue;

                nimcp_spinlock_lock(&astro->lock);
                astro->calcium_concentration = ASTROCYTE_BASELINE_CALCIUM_UM;
                astro->ip3_concentration = 0.0f;
                nimcp_spinlock_unlock(&astro->lock);
            }
            nimcp_mutex_unlock(&network->lock);
            LOG_DEBUG("Astrocyte light recovery: calcium reset to baseline");
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_PARTIAL:
            /* Reset all dynamic state, preserve topology */
            nimcp_mutex_lock(&network->lock);
            for (uint32_t i = 0; i < network->num_astrocytes; i++) {
                astrocyte_t* astro = network->astrocytes[i];
                if (!astro) continue;

                nimcp_spinlock_lock(&astro->lock);
                astro->calcium_concentration = ASTROCYTE_BASELINE_CALCIUM_UM;
                astro->ip3_concentration = 0.0f;
                astro->calcium_baseline = ASTROCYTE_BASELINE_CALCIUM_UM;
                astro->glutamate_pool = 0.5f;
                astro->d_serine_pool = 0.5f;
                astro->atp_level = 1.0f;
                astro->scaling_factor = 1.0f;
                astro->integral_error = 0.0f;
                nimcp_spinlock_unlock(&astro->lock);
            }
            nimcp_mutex_unlock(&network->lock);
            LOG_DEBUG("Astrocyte partial recovery: all state reset");
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        case NIMCP_MODULE_RECOVERY_FULL:
            /* Use the state reset function */
            if (astrocyte_network_state_reset(module_state) == 0) {
                LOG_DEBUG("Astrocyte full recovery: network reset to defaults");
                return NIMCP_MODULE_RECOVERY_SUCCESS;
            }
            return NIMCP_MODULE_RECOVERY_FAILED;

        case NIMCP_MODULE_RECOVERY_ISOLATE:
            /* Set all astrocytes to inactive state */
            nimcp_mutex_lock(&network->lock);
            for (uint32_t i = 0; i < network->num_astrocytes; i++) {
                astrocyte_t* astro = network->astrocytes[i];
                if (!astro) continue;

                nimcp_spinlock_lock(&astro->lock);
                astro->calcium_concentration = 0.0f;
                astro->ip3_concentration = 0.0f;
                astro->atp_level = 0.0f;  /* No metabolic activity */
                nimcp_spinlock_unlock(&astro->lock);
            }
            nimcp_mutex_unlock(&network->lock);
            LOG_WARN("Astrocyte network isolated: all activity disabled");
            return NIMCP_MODULE_RECOVERY_SUCCESS;

        default:
            return NIMCP_MODULE_RECOVERY_FAILED;
    }
}

int nimcp_astrocyte_health_check(void* module_state, float* out_health) {
    if (!module_state || !out_health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_astrocyte_health_check: required parameter is NULL (module_state, out_health)");
        return -1;
    }

    astrocyte_network_t* network = (astrocyte_network_t*)module_state;
    float health = 1.0f;

    nimcp_mutex_lock(&network->lock);

    uint32_t bad_calcium = 0;
    uint32_t nan_count = 0;
    uint32_t bad_pools = 0;
    uint32_t bad_homeostatic = 0;

    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (!astro) continue;

        nimcp_spinlock_lock(&astro->lock);

        /* Check calcium range */
        if (astro->calcium_concentration < 0.0f ||
            astro->calcium_concentration > 100.0f) {
            bad_calcium++;
        }

        /* Check for NaN/Inf */
        if (!isfinite(astro->calcium_concentration) ||
            !isfinite(astro->ip3_concentration) ||
            !isfinite(astro->glutamate_pool)) {
            nan_count++;
        }

        /* Check pools */
        if (astro->glutamate_pool < 0.0f || astro->glutamate_pool > 1.0f ||
            astro->d_serine_pool < 0.0f || astro->d_serine_pool > 1.0f ||
            astro->atp_level < 0.0f || astro->atp_level > 1.0f) {
            bad_pools++;
        }

        /* Check homeostatic */
        if (!isfinite(astro->scaling_factor) || astro->scaling_factor <= 0.0f) {
            bad_homeostatic++;
        }

        nimcp_spinlock_unlock(&astro->lock);
    }

    nimcp_mutex_unlock(&network->lock);

    uint32_t n = network->num_astrocytes;
    if (n == 0) {
        *out_health = 1.0f;
        return 0;
    }

    /* Calcium in valid range: 40% */
    health -= 0.4f * (float)bad_calcium / n;

    /* No NaN/Inf: 30% */
    health -= 0.3f * (float)nan_count / n;

    /* Pools normalized: 20% */
    health -= 0.2f * (float)bad_pools / n;

    /* Homeostatic regulation: 10% */
    health -= 0.1f * (float)bad_homeostatic / n;

    *out_health = fmaxf(0.0f, fminf(1.0f, health));
    return 0;
}

const nimcp_module_recovery_ops_t* nimcp_astrocyte_get_recovery_ops(void) {
    static nimcp_module_recovery_ops_t ops = {
        .recover = nimcp_astrocyte_recovery,
        .health_check = nimcp_astrocyte_health_check,
        .user_data = NULL
    };
    return &ops;
}
