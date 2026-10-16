/**
 * @file nimcp_game_theory_fep_bridge.c
 * @brief Game Theory - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for game theory
 * WHY:  Enable coordinated free energy minimization for strategic reasoning
 * HOW:  Compute free energy from strategy uncertainty, opponent modeling, Nash distance
 *
 * @author NIMCP Development Team
 */

#include "cognitive/game_theory/nimcp_game_theory_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(game_theory_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_game_theory_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_game_theory_fep_bridge_mesh_registry = NULL;

nimcp_error_t game_theory_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_game_theory_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "game_theory_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "game_theory_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_game_theory_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_game_theory_fep_bridge_mesh_registry = registry;
    return err;
}

void game_theory_fep_bridge_mesh_unregister(void) {
    if (g_game_theory_fep_bridge_mesh_registry && g_game_theory_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_game_theory_fep_bridge_mesh_registry, g_game_theory_fep_bridge_mesh_id);
        g_game_theory_fep_bridge_mesh_id = 0;
        g_game_theory_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from game_theory_fep_bridge module (instance-level) */
static inline void game_theory_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_game_theory_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_game_theory_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_game_theory_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "GAME_THEORY_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct gt_fep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    gt_fep_config_t config;

    /* State */
    gt_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    nimcp_gt_system_t gt_system;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    gt_fep_metrics_t metrics;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_strategy_uncertainty;
    float prev_opponent_error;
    float prev_nash_distance;

    /* Running averages */
    float running_avg_fe;
    uint64_t running_count;

    /* Statistics */
    gt_fep_stats_t stats;

    /* Callbacks */
    gt_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    gt_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    gt_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from game theory metrics
 *
 * Free energy model for strategic reasoning:
 * FE = baseline + strategy_contrib + opponent_contrib + nash_contrib
 *
 * Where:
 * - strategy_contrib = strategy_uncertainty * strategy_weight
 *   (higher uncertainty in optimal strategy = higher FE)
 * - opponent_contrib = opponent_prediction_error * opponent_weight
 *   (worse opponent modeling = higher FE)
 * - nash_contrib = nash_distance * nash_weight
 *   (further from equilibrium = higher FE)
 *
 * Nash equilibrium represents minimum free energy state where all agents
 * have accurate predictions of each other's behavior.
 */
static void compute_free_energy(gt_fep_bridge_t* bridge) {
    gt_fep_metrics_t* m = &bridge->metrics;
    const gt_fep_config_t* cfg = &bridge->config;

    /* Clamp input metrics */
    m->strategy_uncertainty = nimcp_clampf(m->strategy_uncertainty, 0.0f, 1.0f);
    m->opponent_prediction_error = nimcp_clampf(m->opponent_prediction_error, 0.0f, 1.0f);
    m->nash_distance = nimcp_clampf(m->nash_distance, 0.0f, 1.0f);

    /* Strategy uncertainty contribution:
     * High uncertainty about which strategy is optimal = prediction error */
    m->strategy_contribution = m->strategy_uncertainty * cfg->strategy_uncertainty_weight;

    /* Opponent modeling contribution:
     * Poor predictions of opponent behavior = prediction error */
    m->opponent_contribution = m->opponent_prediction_error * cfg->opponent_modeling_weight;

    /* Nash convergence contribution:
     * Distance from Nash equilibrium represents system-level prediction error
     * At Nash, all agents have correct mutual predictions */
    m->nash_contribution = m->nash_distance * cfg->nash_convergence_weight;

    /* Total free energy */
    float total_fe = cfg->baseline_free_energy +
                     (m->strategy_contribution +
                      m->opponent_contribution +
                      m->nash_contribution) * cfg->free_energy_weight;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /* At Nash equilibrium check */
    m->at_nash_equilibrium = (m->nash_distance < cfg->nash_epsilon);

    /* Prediction error: weighted combination of all uncertainty sources */
    float new_prediction_error = (m->strategy_uncertainty * 0.4f +
                                   m->opponent_prediction_error * 0.4f +
                                   m->nash_distance * 0.2f);

    /* Apply decay to smooth transitions */
    m->prediction_error = nimcp_clampf(
        new_prediction_error * cfg->error_decay_rate +
        bridge->prev_prediction_error * (1.0f - cfg->error_decay_rate),
        0.0f, 1.0f
    );

    /* Surprise: unexpected changes in game state */
    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float strategy_change = fabsf(m->strategy_uncertainty - bridge->prev_strategy_uncertainty);
    float opponent_change = fabsf(m->opponent_prediction_error - bridge->prev_opponent_error);
    float nash_change = fabsf(m->nash_distance - bridge->prev_nash_distance);

    m->surprise = nimcp_clampf(
        (fe_change * 0.3f + strategy_change * 0.3f +
         opponent_change * 0.2f + nash_change * 0.2f),
        0.0f, 1.0f
    );

    /* Entropy: based on strategy type
     * Mixed strategies have higher entropy than pure strategies
     * Approximate based on uncertainty */
    m->entropy = nimcp_clampf(
        m->strategy_uncertainty * 0.7f + m->opponent_prediction_error * 0.3f,
        0.0f, 1.0f
    );

    /* Payoff variance approximation from uncertainty */
    m->payoff_variance = m->strategy_uncertainty * m->opponent_prediction_error;
}

/**
 * @brief Check and trigger callbacks
 */
static void check_callbacks(gt_fep_bridge_t* bridge) {
    gt_fep_metrics_t* m = &bridge->metrics;
    const gt_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != GT_FEP_STATE_DEGRADED) {
            bridge->state = GT_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                bridge->high_fe_callback(bridge, m->free_energy,
                                         bridge->high_fe_user_data);
            }
        }
    } else if (bridge->state == GT_FEP_STATE_DEGRADED) {
        bridge->state = GT_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->strategy_contribution > m->opponent_contribution &&
                m->strategy_contribution > m->nash_contribution) {
                source = "strategy";
            } else if (m->opponent_contribution > m->nash_contribution) {
                source = "opponent";
            } else {
                source = "nash";
            }
            bridge->surprise_callback(bridge, m->surprise, source,
                                      bridge->surprise_user_data);
        }
    }

    /* Metrics callback */
    if (bridge->metrics_callback) {
        bridge->metrics_callback(bridge, m, bridge->metrics_user_data);
    }
}

/**
 * @brief Update statistics
 */
static void update_stats(gt_fep_bridge_t* bridge, uint64_t update_time_us) {
    gt_fep_stats_t* s = &bridge->stats;
    gt_fep_metrics_t* m = &bridge->metrics;

    s->total_updates++;
    s->total_update_time_us += update_time_us;
    s->avg_update_time_us = (float)s->total_update_time_us / (float)s->total_updates;

    /* Track FE contribution */
    s->total_free_energy_contribution += m->free_energy;

    /* Peak tracking */
    if (m->free_energy > s->peak_free_energy) {
        s->peak_free_energy = m->free_energy;
    }

    /* Running averages */
    bridge->running_count++;
    bridge->running_avg_fe = (bridge->running_avg_fe * (bridge->running_count - 1) +
                              m->free_energy) / bridge->running_count;
    s->avg_free_energy = bridge->running_avg_fe;

    /* Update metrics timing */
    m->last_update_time_ms = get_time_ms();
    m->update_count++;
}

/**
 * @brief Store previous state for delta computation
 */
static void store_previous_state(gt_fep_bridge_t* bridge) {
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_strategy_uncertainty = bridge->metrics.strategy_uncertainty;
    bridge->prev_opponent_error = bridge->metrics.opponent_prediction_error;
    bridge->prev_nash_distance = bridge->metrics.nash_distance;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

gt_fep_config_t gt_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_config_defaul", 0.0f);


    gt_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Feature enables */
    config.enable_logging = false;
    config.update_interval_ms = 50;  /* Cognitive timescale */

    /* Weighting parameters */
    config.free_energy_weight = 1.0f;
    config.strategy_uncertainty_weight = GT_FEP_STRATEGY_WEIGHT;
    config.opponent_modeling_weight = GT_FEP_OPPONENT_WEIGHT;
    config.nash_convergence_weight = GT_FEP_NASH_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.nash_epsilon = 0.01f;

    /* Normalization */
    config.baseline_free_energy = GT_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = GT_FEP_MAX_FREE_ENERGY;
    config.error_decay_rate = GT_FEP_ERROR_DECAY_RATE;

    return config;
}

gt_fep_bridge_t* gt_fep_bridge_create(const gt_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_create", 0.0f);


    gt_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(gt_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = gt_fep_config_default();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "gt_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "gt_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = GT_FEP_STATE_UNINITIALIZED;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.strategy_uncertainty = 0.0f;
    bridge->metrics.opponent_prediction_error = 0.0f;
    bridge->metrics.nash_distance = 1.0f;  /* Start far from equilibrium */
    bridge->metrics.at_nash_equilibrium = false;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_strategy_uncertainty = 0.0f;
    bridge->prev_opponent_error = 0.0f;
    bridge->prev_nash_distance = 1.0f;

    bridge->state = GT_FEP_STATE_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "game_theory_fep");
    return bridge;
}

void gt_fep_bridge_destroy(gt_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "game_theory_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_destro", 0.0f);


    if (bridge->registered) {
        gt_fep_bridge_unregister(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int gt_fep_bridge_reset(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(gt_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.nash_distance = 1.0f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_strategy_uncertainty = 0.0f;
    bridge->prev_opponent_error = 0.0f;
    bridge->prev_nash_distance = 1.0f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(gt_fep_stats_t));

    bridge->state = GT_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int gt_fep_bridge_register(
    gt_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    nimcp_gt_system_t gt_system,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_register: required parameter is NULL (bridge, orchestrator)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        if (bridge_id_out) {
            *bridge_id_out = bridge->bridge_id;
        }
        return 0;  /* Already registered, success */
    }

    /* Store references */
    bridge->orchestrator = orchestrator;
    bridge->gt_system = gt_system;

    /* Register with FEP orchestrator */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "game_theory",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        gt_fep_update_callback,
        gt_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->gt_system = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gt_fep_bridge_register: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = GT_FEP_STATE_ACTIVE;

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gt_fep_bridge_unregister(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_unregister: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_unregi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not registered, nothing to do */
    }

    /* Unregister from orchestrator */
    if (bridge->orchestrator) {
        fep_orchestrator_unregister_bridge(bridge->orchestrator, bridge->bridge_id);
    }

    bridge->registered = false;
    bridge->bridge_id = 0;
    bridge->orchestrator = NULL;
    bridge->gt_system = NULL;
    bridge->state = GT_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool gt_fep_bridge_is_registered(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_is_reg", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t gt_fep_bridge_get_id(gt_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_id", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int gt_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_update_callba", 0.0f);


    gt_fep_bridge_t* bridge = (gt_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_update_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered */
    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Not registered — normal condition during teardown */
    }

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    store_previous_state(bridge);

    /* If we have a game theory system, query it for statistics */
    if (bridge->gt_system) {
        nimcp_game_stats_t gt_stats;
        memset(&gt_stats, 0, sizeof(gt_stats));

        nimcp_error_t err = nimcp_gt_get_stats(bridge->gt_system, &gt_stats);
        if (err == NIMCP_SUCCESS) {
            /* Update metrics from game theory statistics */
            bridge->stats.strategy_computations++;

            /* Estimate strategy uncertainty from convergence metrics */
            if (gt_stats.avg_convergence_iterations > 0) {
                /* More iterations to converge = higher uncertainty */
                bridge->metrics.strategy_uncertainty = nimcp_clampf(
                    gt_stats.avg_convergence_iterations / 100.0f,
                    0.0f, 1.0f
                );
            }

            /* Estimate Nash distance from equilibrium finding success */
            if (gt_stats.games_played > 0) {
                float equilibrium_rate = (float)gt_stats.equilibria_found /
                                        (float)gt_stats.games_played;
                /* Higher equilibrium finding rate = closer to Nash */
                bridge->metrics.nash_distance = nimcp_clampf(
                    1.0f - equilibrium_rate,
                    0.0f, 1.0f
                );
                bridge->metrics.equilibria_found = (uint32_t)gt_stats.equilibria_found;
            }

            /* Opponent modeling error from bargaining success */
            if (gt_stats.bargaining_successes + gt_stats.bargaining_failures > 0) {
                float success_rate = (float)gt_stats.bargaining_successes /
                    (float)(gt_stats.bargaining_successes + gt_stats.bargaining_failures);
                /* Lower success rate = higher prediction error */
                bridge->metrics.opponent_prediction_error = nimcp_clampf(
                    1.0f - success_rate,
                    0.0f, 1.0f
                );
            }

            bridge->stats.nash_equilibrium_checks++;
        }
    }

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Check and trigger callbacks outside mutex to prevent deadlock */
    check_callbacks(bridge);

    return 0;
}

void gt_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via gt_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_destroy_callb", 0.0f);


    (void)handle;
}

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

int gt_fep_bridge_force_update(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_force_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_force_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_us = get_time_us();

    /* Store previous state */
    store_previous_state(bridge);

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Check and trigger callbacks outside mutex to prevent deadlock */
    check_callbacks(bridge);

    return 0;
}

int gt_fep_bridge_update_strategy_uncertainty(
    gt_fep_bridge_t* bridge,
    float uncertainty
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_update_strategy_uncertainty: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.strategy_uncertainty = nimcp_clampf(uncertainty, 0.0f, 1.0f);
    bridge->stats.strategy_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gt_fep_bridge_update_opponent_error(
    gt_fep_bridge_t* bridge,
    float error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_update_opponent_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.opponent_prediction_error = nimcp_clampf(error, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gt_fep_bridge_update_nash_distance(
    gt_fep_bridge_t* bridge,
    float distance
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_update_nash_distance: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.nash_distance = nimcp_clampf(distance, 0.0f, 1.0f);
    bridge->stats.nash_equilibrium_checks++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int gt_fep_bridge_get_metrics(
    const gt_fep_bridge_t* bridge,
    gt_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_get_metrics: required parameter is NULL (bridge, metrics_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_me", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int gt_fep_bridge_get_stats(
    const gt_fep_bridge_t* bridge,
    gt_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int gt_fep_bridge_reset_stats(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(gt_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float gt_fep_bridge_get_free_energy(gt_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_fr", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float gt_fep_bridge_get_strategy_uncertainty(gt_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    float su = bridge->metrics.strategy_uncertainty;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return su;
}

float gt_fep_bridge_get_prediction_error(gt_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_pr", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.prediction_error;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

gt_fep_state_t gt_fep_bridge_get_state(gt_fep_bridge_t* bridge) {
    if (!bridge) return GT_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    gt_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool gt_fep_bridge_is_degraded(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_is_deg", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == GT_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

bool gt_fep_bridge_is_at_nash(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_is_at_", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    bool at_nash = bridge->metrics.at_nash_equilibrium;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return at_nash;
}

const char* gt_fep_state_name(gt_fep_state_t state) {
    switch (state) {
        case GT_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case GT_FEP_STATE_IDLE:          return "idle";
        case GT_FEP_STATE_ACTIVE:        return "active";
        case GT_FEP_STATE_DEGRADED:      return "degraded";
        case GT_FEP_STATE_ERROR:         return "error";
        default:                          return "unknown";
    }
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int gt_fep_bridge_set_high_fe_callback(
    gt_fep_bridge_t* bridge,
    gt_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_set_high_fe_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_set_hi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gt_fep_bridge_set_surprise_callback(
    gt_fep_bridge_t* bridge,
    gt_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_set_surprise_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_set_su", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gt_fep_bridge_set_metrics_callback(
    gt_fep_bridge_t* bridge,
    gt_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_set_metrics_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_set_me", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int gt_fep_bridge_set_config(
    gt_fep_bridge_t* bridge,
    const gt_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_set_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gt_fep_bridge_get_config(
    const gt_fep_bridge_t* bridge,
    gt_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gt_fep_bridge_get_config: required parameter is NULL (bridge, config_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_fep_bridge_heartbeat("game_theory__gt_fep_bridge_get_co", 0.0f);


    nimcp_mutex_lock(((gt_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((gt_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void game_theory_fep_bridge_set_instance_health_agent(gt_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "game_theory_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int game_theory_fep_bridge_training_begin(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    game_theory_fep_bridge_heartbeat_instance(bridge->health_agent, "game_theory_fep_bridge_training_begin", 0.0f);
    return 0;
}

int game_theory_fep_bridge_training_end(gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_fep_bridge_training_end: NULL argument");
        return -1;
    }
    game_theory_fep_bridge_heartbeat_instance(bridge->health_agent, "game_theory_fep_bridge_training_end", 1.0f);
    return 0;
}

int game_theory_fep_bridge_training_step(gt_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    game_theory_fep_bridge_heartbeat_instance(bridge->health_agent, "game_theory_fep_bridge_training_step", progress);
    return 0;
}
