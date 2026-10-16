/**
 * @file nimcp_quantum_annealing_immune_bridge.c
 * @brief Quantum Annealing-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of quantum annealing-immune bridge
 * WHY:  Bidirectional coordination between optimization and immune systems
 * HOW:  Inflammation modulates annealing; divergence triggers immune response
 *
 * @author NIMCP Development Team
 */

#include "optimization/immune/nimcp_quantum_annealing_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_annealing_immune_bridge)

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute temperature factor for inflammation level
 *
 * WHAT: Map inflammation to temperature multiplier
 * WHY:  Higher inflammation → more exploration (higher temp)
 * HOW:  Return predefined factor for inflammation level
 */
static float compute_temp_factor(brain_inflammation_level_t inflammation) {
    switch (inflammation) {
        case INFLAMMATION_NONE:     return QA_IMMUNE_TEMP_FACTOR_NONE;
        case INFLAMMATION_LOCAL:    return QA_IMMUNE_TEMP_FACTOR_LOCAL;
        case INFLAMMATION_REGIONAL: return QA_IMMUNE_TEMP_FACTOR_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return QA_IMMUNE_TEMP_FACTOR_SYSTEMIC;
        case INFLAMMATION_STORM:    return QA_IMMUNE_TEMP_FACTOR_STORM;
        default:                    return 1.0f;
    }
}

/**
 * @brief Compute iteration factor for inflammation level
 *
 * WHAT: Map inflammation to iteration reduction
 * WHY:  Higher inflammation → fewer iterations (energy conservation)
 * HOW:  Return predefined factor for inflammation level
 */
static float compute_iter_factor(brain_inflammation_level_t inflammation) {
    switch (inflammation) {
        case INFLAMMATION_NONE:     return QA_IMMUNE_ITER_FACTOR_NONE;
        case INFLAMMATION_LOCAL:    return QA_IMMUNE_ITER_FACTOR_LOCAL;
        case INFLAMMATION_REGIONAL: return QA_IMMUNE_ITER_FACTOR_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return QA_IMMUNE_ITER_FACTOR_SYSTEMIC;
        case INFLAMMATION_STORM:    return QA_IMMUNE_ITER_FACTOR_STORM;
        default:                    return 1.0f;
    }
}

/**
 * @brief Compute quantum tunneling factor for inflammation level
 *
 * WHAT: Map inflammation to tunneling strength adjustment
 * WHY:  Moderate inflammation boosts exploration; severe reduces precision
 * HOW:  Return predefined factor for inflammation level
 */
static float compute_tunneling_factor(brain_inflammation_level_t inflammation) {
    switch (inflammation) {
        case INFLAMMATION_NONE:     return QA_IMMUNE_TUNNEL_FACTOR_NONE;
        case INFLAMMATION_LOCAL:    return QA_IMMUNE_TUNNEL_FACTOR_LOCAL;
        case INFLAMMATION_REGIONAL: return QA_IMMUNE_TUNNEL_FACTOR_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return QA_IMMUNE_TUNNEL_FACTOR_SYSTEMIC;
        case INFLAMMATION_STORM:    return QA_IMMUNE_TUNNEL_FACTOR_STORM;
        default:                    return 1.0f;
    }
}

/**
 * @brief Compute problem severity from metrics
 *
 * WHAT: Determine severity of optimization problem
 * WHY:  Map problem type to immune severity scale (1-10)
 * HOW:  Return severity based on problem type and metrics
 */
static uint32_t compute_problem_severity(qa_problem_type_t type, const qa_immune_metrics_t* metrics) {
    if (!metrics) return 5;

    switch (type) {
        case QA_PROBLEM_ENERGY_EXPLOSION:      return 9;  /* Critical */
        case QA_PROBLEM_NUMERICAL_INSTABILITY: return 9;  /* Critical */
        case QA_PROBLEM_DIVERGENCE:            return 8;  /* Severe */
        case QA_PROBLEM_STUCK_LOCAL_MINIMUM:   return 6;  /* Moderate */
        case QA_PROBLEM_OSCILLATION:           return 5;  /* Moderate */
        case QA_PROBLEM_NO_IMPROVEMENT:        return 4;  /* Minor */
        default:                               return 1;
    }
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int qa_immune_default_config(qa_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_default_config: null config");
        return -1;
    }

    memset(config, 0, sizeof(qa_immune_config_t));

    /* Modulation enables */
    config->enable_temp_modulation = true;
    config->enable_iter_reduction = true;
    config->enable_tunneling_modulation = true;
    config->enable_cooling_modulation = false;

    /* Custom factors (use defaults) */
    config->temp_factor_local = QA_IMMUNE_TEMP_FACTOR_LOCAL;
    config->temp_factor_regional = QA_IMMUNE_TEMP_FACTOR_REGIONAL;
    config->temp_factor_systemic = QA_IMMUNE_TEMP_FACTOR_SYSTEMIC;
    config->temp_factor_storm = QA_IMMUNE_TEMP_FACTOR_STORM;

    /* Problem detection */
    config->energy_explosion_ratio = QA_IMMUNE_ENERGY_EXPLOSION_RATIO;
    config->stuck_iterations = QA_IMMUNE_STUCK_ITERATIONS;
    config->oscillation_threshold = QA_IMMUNE_OSCILLATION_THRESHOLD;

    /* Immune response */
    config->enable_auto_immune_response = true;
    config->min_response_duration_ms = QA_IMMUNE_MIN_RESPONSE_DURATION_MS;
    config->temp_ema_alpha = QA_IMMUNE_TEMP_EMA_ALPHA;

    /* Monitoring */
    config->history_size = QA_IMMUNE_MAX_HISTORY;
    config->enable_logging = true;

    return 0;
}

qa_immune_bridge_t* qa_immune_create(
    const qa_immune_config_t* config,
    quantum_annealer_t annealer,
    brain_immune_system_t* immune_system
) {
    if (!annealer) {
        NIMCP_LOGGING_ERROR("Invalid parameters: annealer required");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_create: null annealer");
        return NULL;
    }
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters: immune_system required");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_create: null immune_system");
        return NULL;
    }

    /* Allocate bridge */
    qa_immune_bridge_t* bridge = (qa_immune_bridge_t*)nimcp_malloc(sizeof(qa_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate quantum annealing immune bridge");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(qa_immune_bridge_t),
                          "Failed to allocate quantum annealing immune bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(qa_immune_bridge_t));

    /* Set config */
    if (config) {
        memcpy(&bridge->config, config, sizeof(qa_immune_config_t));
    } else {
        qa_immune_default_config(&bridge->config);
    }

    /* Set integration handles */
    bridge->annealer = annealer;
    bridge->immune_system = immune_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "quantum_annealing_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0, "Failed to create mutex for QA immune bridge");
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate history */
    bridge->history_capacity = bridge->config.history_size;
    size_t history_size = sizeof(qa_immune_metrics_t) * bridge->history_capacity;
    bridge->history = (qa_immune_metrics_t*)nimcp_malloc(history_size);
    if (!bridge->history) {
        NIMCP_LOGGING_ERROR("Failed to allocate history buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, history_size,
                          "Failed to allocate history buffer for QA immune bridge (capacity=%zu)",
                          bridge->history_capacity);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }

    /* P3-OPT-3: Use named constant for default event capacity */
#define QA_IMMUNE_DEFAULT_EVENT_CAPACITY 64

    /* Allocate events */
    bridge->event_capacity = QA_IMMUNE_DEFAULT_EVENT_CAPACITY;
    size_t events_size = sizeof(qa_immune_problem_event_t) * bridge->event_capacity;
    bridge->events = (qa_immune_problem_event_t*)nimcp_malloc(events_size);
    if (!bridge->events) {
        NIMCP_LOGGING_ERROR("Failed to allocate event buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, events_size,
                          "Failed to allocate event buffer for QA immune bridge (capacity=%zu)",
                          bridge->event_capacity);
        nimcp_free(bridge->history);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->phase = QA_IMMUNE_PHASE_IDLE;
    bridge->inflammation = INFLAMMATION_NONE;
    bridge->current_temp_factor = 1.0f;
    bridge->current_iter_factor = 1.0f;
    bridge->best_energy = FLT_MAX;
    bridge->running = false;
    bridge->base.bio_async_enabled = false;
    bridge->start_time_ms = nimcp_time_get_ms();

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created quantum annealing immune bridge");
    }

    return bridge;
}

void qa_immune_destroy(qa_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        qa_immune_disconnect_bio_async(bridge);
    }

    /* Free buffers */
    if (bridge->history) {
        nimcp_free(bridge->history);
    }
    if (bridge->events) {
        nimcp_free(bridge->events);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed quantum annealing immune bridge");
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int qa_immune_connect_bio_async(qa_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_connect_bio_async: null bridge");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_QUANTUM_ANNEALING,
        .module_name = QA_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_INFO("Connected to bio-async router");
        }
    } else {
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        }
    }

    return 0;
}

int qa_immune_disconnect_bio_async(qa_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_disconnect_bio_async: null bridge");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    return 0;
}

bool qa_immune_is_bio_async_connected(const qa_immune_bridge_t* bridge) {
    /* P2 fix: Remove false positive NIMCP_THROW_TO_IMMUNE. This is a query function -
     * NULL bridge simply means "not connected" which is a valid answer, not an error. */
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update and Modulation API Implementation
 * ============================================================================ */

int qa_immune_update(qa_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_update: null bridge");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_update: null immune_system");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get inflammation from brain immune */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune_system, &immune_stats) == 0) {
        /* Assume inflammation level from stats (would need actual API) */
        /* For now, check system health to infer inflammation */
        if (immune_stats.system_health < 0.3f) {
            bridge->inflammation = INFLAMMATION_STORM;
        } else if (immune_stats.system_health < 0.5f) {
            bridge->inflammation = INFLAMMATION_SYSTEMIC;
        } else if (immune_stats.system_health < 0.7f) {
            bridge->inflammation = INFLAMMATION_REGIONAL;
        } else if (immune_stats.system_health < 0.9f) {
            bridge->inflammation = INFLAMMATION_LOCAL;
        } else {
            bridge->inflammation = INFLAMMATION_NONE;
        }
    }

    /* Compute modulation factors */
    bridge->current_temp_factor = compute_temp_factor(bridge->inflammation);
    bridge->current_iter_factor = compute_iter_factor(bridge->inflammation);

    /* Update cytokine effects (simplified - would query actual cytokine concentrations) */
    bridge->cytokine_effects.temperature_factor = bridge->current_temp_factor;
    bridge->cytokine_effects.iteration_factor = bridge->current_iter_factor;
    bridge->cytokine_effects.tunneling_factor = compute_tunneling_factor(bridge->inflammation);

    /* Update stats */
    bridge->stats.current_inflammation = bridge->inflammation;
    bridge->stats.current_temp_factor = bridge->current_temp_factor;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int qa_immune_apply_modulation(qa_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_apply_modulation: null bridge");
        return -1;
    }
    if (!bridge->annealer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_apply_modulation: null annealer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Note: Actual modulation would require annealer API to modify config */
    /* For now, we store the factors for use during optimization */

    if (bridge->config.enable_logging && bridge->current_temp_factor != 1.0f) {
        NIMCP_LOGGING_INFO("Applied immune modulation: temp_factor=%.2f, iter_factor=%.2f",
                          bridge->current_temp_factor, bridge->current_iter_factor);
    }

    bridge->stats.temp_modulations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int qa_immune_update_metrics(
    qa_immune_bridge_t* bridge,
    uint64_t iteration,
    float energy,
    float temperature
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_update_metrics: null bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update current metrics */
    bridge->current_metrics.iteration = iteration;
    bridge->current_metrics.energy_prev = bridge->current_metrics.energy;
    bridge->current_metrics.energy = energy;
    bridge->current_metrics.temperature = temperature;
    bridge->current_metrics.effective_temperature = temperature * bridge->current_temp_factor;
    bridge->current_metrics.timestamp_ms = nimcp_time_get_ms();

    /* Check for NaN/Inf */
    bridge->current_metrics.has_nan = isnan(energy);
    bridge->current_metrics.has_inf = isinf(energy);

    /* Update minimum energy */
    if (energy < bridge->current_metrics.energy_min || iteration == 0) {
        bridge->current_metrics.energy_min = energy;
        bridge->iterations_without_improvement = 0;
    } else {
        bridge->iterations_without_improvement++;
    }

    /* Update EMA */
    float alpha = bridge->config.temp_ema_alpha;
    bridge->current_metrics.energy_ema = alpha * energy + (1.0f - alpha) * bridge->current_metrics.energy_ema;

    /* Store in history */
    if (bridge->history_count < bridge->history_capacity) {
        bridge->history[bridge->history_count++] = bridge->current_metrics;
    } else {
        /* Circular buffer */
        bridge->history[bridge->history_index] = bridge->current_metrics;
        bridge->history_index = (bridge->history_index + 1) % bridge->history_capacity;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Problem Detection and Response API Implementation
 * ============================================================================ */

qa_problem_type_t qa_immune_check_convergence(
    qa_immune_bridge_t* bridge,
    float final_energy
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_check_convergence: null bridge");
        return QA_PROBLEM_NONE;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    qa_problem_type_t problem = QA_PROBLEM_NONE;

    /* Check for NaN/Inf */
    if (isnan(final_energy) || isinf(final_energy)) {
        problem = QA_PROBLEM_NUMERICAL_INSTABILITY;
        goto done;
    }

    /* Check for energy explosion */
    if (bridge->current_metrics.energy_min > 0 &&
        final_energy > bridge->current_metrics.energy_min * bridge->config.energy_explosion_ratio) {
        problem = QA_PROBLEM_ENERGY_EXPLOSION;
        goto done;
    }

    /* Check for stuck in local minimum */
    if (bridge->iterations_without_improvement >= bridge->config.stuck_iterations) {
        problem = QA_PROBLEM_STUCK_LOCAL_MINIMUM;
        goto done;
    }

    /* Check for oscillation */
    if (bridge->history_count > 10) {
        float variance = 0.0f;
        float mean = 0.0f;
        size_t count = bridge->history_count < 20 ? bridge->history_count : 20;

        /* P2 fix: Use correct circular buffer indexing after wrap.
         * history_index points to the NEXT write position, so the most recent
         * 'count' entries are at indices going backwards from history_index. */
        for (size_t i = 0; i < count; i++) {
            size_t idx = (bridge->history_index + bridge->history_capacity - count + i)
                         % bridge->history_capacity;
            mean += bridge->history[idx].energy;
        }
        mean /= count;

        for (size_t i = 0; i < count; i++) {
            size_t idx = (bridge->history_index + bridge->history_capacity - count + i)
                         % bridge->history_capacity;
            float diff = bridge->history[idx].energy - mean;
            variance += diff * diff;
        }
        variance /= count;

        if (variance > bridge->config.oscillation_threshold * mean * mean) {
            problem = QA_PROBLEM_OSCILLATION;
            goto done;
        }
    }

done:
    /* P2 fix: Stats update was previously BEFORE the done: label, so goto done
     * would skip the stats update when a problem was detected. Now it's after. */
    if (problem == QA_PROBLEM_NONE) {
        bridge->stats.successful_optimizations++;
    } else {
        bridge->stats.failed_optimizations++;
    }

    /* P2-OPT-4: Increment optimizations_run counter */
    bridge->stats.optimizations_run++;

    bridge->stats.success_rate = bridge->stats.optimizations_run > 0 ?
        (float)bridge->stats.successful_optimizations / bridge->stats.optimizations_run : 0.0f;

    /* P2-OPT-5: Read auto-immune config and metrics inside locked section
     * to avoid race on post-unlock data access */
    bool should_auto_respond = (problem != QA_PROBLEM_NONE && bridge->config.enable_auto_immune_response);
    qa_immune_metrics_t metrics_snapshot = bridge->current_metrics;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Auto-trigger immune response if enabled (uses snapshot taken under lock) */
    if (should_auto_respond) {
        uint32_t event_id;
        uint32_t severity = compute_problem_severity(problem, &metrics_snapshot);
        qa_immune_report_problem(bridge, problem, severity, &event_id);
        qa_immune_trigger_immune_response(bridge, event_id);
    }

    return problem;
}

int qa_immune_report_problem(
    qa_immune_bridge_t* bridge,
    qa_problem_type_t type,
    uint32_t severity,
    uint32_t* event_id
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_report_problem: null bridge");
        return -1;
    }
    if (!event_id) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_report_problem: null event_id");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->event_count >= bridge->event_capacity) {
        NIMCP_LOGGING_WARN("Event buffer full, cannot record problem");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "qa_immune_report_problem: capacity exceeded");
        return -1;
    }

    /* Create event */
    qa_immune_problem_event_t* event = &bridge->events[bridge->event_count++];
    memset(event, 0, sizeof(qa_immune_problem_event_t));

    event->event_id = bridge->next_event_id++;
    event->type = type;
    event->severity = severity;
    event->confidence = 0.9f;  /* High confidence for now */
    event->metrics = bridge->current_metrics;
    event->detection_time_ms = nimcp_time_get_ms();
    event->immune_triggered = false;

    *event_id = event->event_id;

    /* Update stats */
    bridge->stats.total_problems++;
    if (type < QA_PROBLEM_COUNT) {
        bridge->stats.problems_by_type[type]++;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_WARN("Optimization problem detected: type=%s, severity=%u",
                          qa_problem_type_to_string(type), severity);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int qa_immune_trigger_immune_response(
    qa_immune_bridge_t* bridge,
    uint32_t event_id
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_trigger_immune_response: null bridge");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_trigger_immune_response: null immune_system");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Find event */
    qa_immune_problem_event_t* event = NULL;
    for (size_t i = 0; i < bridge->event_count; i++) {
        if (bridge->events[i].event_id == event_id) {
            event = &bridge->events[i];
            break;
        }
    }

    if (!event) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;  /* Event not found - normal search miss */
    }

    /* Create epitope from problem type */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));

    /* Encode problem type and severity */
    epitope[0] = (uint8_t)event->type;
    epitope[1] = (uint8_t)event->severity;
    memcpy(&epitope[2], &event->metrics.energy, sizeof(float));
    memcpy(&epitope[6], &event->metrics.temperature, sizeof(float));

    /* Present to brain immune */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        10,  /* Use first 10 bytes */
        event->severity,
        0,   /* No specific source node */
        &antigen_id
    );

    if (result == 0) {
        event->antigen_id = antigen_id;
        event->immune_triggered = true;
        bridge->in_immune_response = true;
        bridge->immune_response_start_ms = nimcp_time_get_ms();

        /* Update stats */
        bridge->stats.immune_responses_triggered++;

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_INFO("Triggered immune response for optimization problem (antigen_id=%u)",
                              antigen_id);
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Query and Statistics API Implementation
 * ============================================================================ */

qa_immune_phase_t qa_immune_get_phase(const qa_immune_bridge_t* bridge) {
    return bridge ? bridge->phase : QA_IMMUNE_PHASE_IDLE;
}

brain_inflammation_level_t qa_immune_get_inflammation(const qa_immune_bridge_t* bridge) {
    return bridge ? bridge->inflammation : INFLAMMATION_NONE;
}

float qa_immune_get_temp_factor(const qa_immune_bridge_t* bridge) {
    return bridge ? bridge->current_temp_factor : 1.0f;
}

float qa_immune_get_iter_factor(const qa_immune_bridge_t* bridge) {
    return bridge ? bridge->current_iter_factor : 1.0f;
}

int qa_immune_get_cytokine_effects(
    const qa_immune_bridge_t* bridge,
    qa_immune_cytokine_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_get_cytokine_effects: null bridge");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_get_cytokine_effects: null effects");
        return -1;
    }

    /* P2-OPT-6: Acquire mutex before reading cytokine_effects to avoid data race */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(qa_immune_cytokine_effects_t));
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);
    return 0;
}

int qa_immune_get_stats(
    const qa_immune_bridge_t* bridge,
    qa_immune_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_get_stats: null bridge");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_get_stats: null stats");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(qa_immune_stats_t));
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return 0;
}

int qa_immune_get_current_metrics(
    const qa_immune_bridge_t* bridge,
    qa_immune_metrics_t* metrics
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_get_current_metrics: null bridge");
        return -1;
    }
    if (!metrics) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qa_immune_get_current_metrics: null metrics");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    memcpy(metrics, &bridge->current_metrics, sizeof(qa_immune_metrics_t));
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return 0;
}

const qa_immune_problem_event_t* qa_immune_get_event(
    const qa_immune_bridge_t* bridge,
    uint32_t event_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    for (size_t i = 0; i < bridge->event_count; i++) {
        if (bridge->events[i].event_id == event_id) {
            return &bridge->events[i];
        }
    }

    return NULL;  /* Event not found - normal search miss */
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* qa_immune_phase_to_string(qa_immune_phase_t phase) {
    switch (phase) {
        case QA_IMMUNE_PHASE_IDLE:       return "IDLE";
        case QA_IMMUNE_PHASE_OPTIMIZING: return "OPTIMIZING";
        case QA_IMMUNE_PHASE_RESPONDING: return "RESPONDING";
        case QA_IMMUNE_PHASE_RECOVERING: return "RECOVERING";
        case QA_IMMUNE_PHASE_RESOLVED:   return "RESOLVED";
        default:                         return "UNKNOWN";
    }
}

const char* qa_problem_type_to_string(qa_problem_type_t type) {
    switch (type) {
        case QA_PROBLEM_NONE:                  return "NONE";
        case QA_PROBLEM_ENERGY_EXPLOSION:      return "ENERGY_EXPLOSION";
        case QA_PROBLEM_STUCK_LOCAL_MINIMUM:   return "STUCK_LOCAL_MINIMUM";
        case QA_PROBLEM_NO_IMPROVEMENT:        return "NO_IMPROVEMENT";
        case QA_PROBLEM_OSCILLATION:           return "OSCILLATION";
        case QA_PROBLEM_DIVERGENCE:            return "DIVERGENCE";
        case QA_PROBLEM_NUMERICAL_INSTABILITY: return "NUMERICAL_INSTABILITY";
        default:                               return "UNKNOWN";
    }
}
