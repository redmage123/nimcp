/**
 * @file nimcp_social_fep_bridge.c
 * @brief Social Cognition - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for social cognition
 * WHY:  Enable coordinated free energy minimization across social processing
 * HOW:  Compute free energy from social prediction error, relationship uncertainty,
 *       and social norm violations
 *
 * FREE ENERGY MODEL FOR SOCIAL COGNITION:
 * - Social prediction error: mismatch between expected and actual social behavior
 * - Relationship uncertainty: unpredictability of social bonds and trust
 * - Norm violations: unexpected social behavior (high surprise events)
 * - Accurate social predictions reduce free energy
 * - Stable relationships minimize free energy
 *
 * @author NIMCP Development Team
 */

#include "cognitive/social/nimcp_social_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(social_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_social_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_social_fep_bridge_mesh_registry = NULL;

nimcp_error_t social_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_social_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "social_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "social_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_social_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_social_fep_bridge_mesh_registry = registry;
    return err;
}

void social_fep_bridge_mesh_unregister(void) {
    if (g_social_fep_bridge_mesh_registry && g_social_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_social_fep_bridge_mesh_registry, g_social_fep_bridge_mesh_id);
        g_social_fep_bridge_mesh_id = 0;
        g_social_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from social_fep_bridge module (instance-level) */
static inline void social_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_social_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_social_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_social_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SOCIAL_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct social_fep_bridge {
    bridge_base_t base;  /* MUST be first member */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    social_fep_config_t config;

    /* State */
    social_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    social_bond_system_t* social;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    social_fep_metrics_t metrics;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_relationship_uncertainty;
    float prev_closeness;
    float prev_trust;

    /* Running averages */
    float running_avg_fe;
    float running_avg_pe;
    uint64_t running_count;

    /* Statistics */
    social_fep_stats_t stats;

    /* Callbacks */
    social_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    social_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    social_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

BRIDGE_DEFINE_SECURITY_SETTERS(social_fep_bridge)

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from social cognition metrics
 *
 * Free energy model:
 * FE = baseline + social_prediction_contrib + uncertainty_contrib + norm_violation_contrib
 *
 * Where:
 * - social_prediction_contrib = prediction_error * prediction_weight
 * - uncertainty_contrib = relationship_uncertainty * uncertainty_weight
 * - norm_violation_contrib = norm_violations * norm_weight
 *
 * Key insights:
 * - Accurate social predictions reduce free energy
 * - Stable relationships minimize uncertainty contribution
 * - Social norm violations are high-surprise events
 * - Loneliness increases prediction error (less social data)
 */
static void compute_free_energy(social_fep_bridge_t* bridge) {
    social_fep_metrics_t* m = &bridge->metrics;
    const social_fep_config_t* cfg = &bridge->config;

    if (!bridge->social) {
        /* No social system attached, use baseline */
        m->free_energy = cfg->baseline_free_energy;
        return;
    }

    /*=========================================================================
     * EXTRACT SOCIAL STATE
     *=========================================================================*/

    /* Get oxytocin level as indicator of social bonding */
    m->oxytocin_level = social_get_oxytocin_level(bridge->social);

    /* Get close friend count */
    m->close_friends_count = social_get_close_friend_count(bridge->social);

    /* Check loneliness */
    m->loneliness = social_is_lonely(bridge->social) ? 0.7f : 0.2f;

    /* Get social warmth and closeness from the social module */
    /* We don't have direct accessors, so we'll compute from available data */

    /*=========================================================================
     * COMPUTE SOCIAL PREDICTION ERROR
     *=========================================================================*/

    /* Social prediction error increases with:
     * - Loneliness (less social data for accurate predictions)
     * - Low oxytocin (weaker bonds, less predictable interactions)
     * - Few close friends (less practice at social prediction)
     */
    float loneliness_contribution = m->loneliness * 0.4f;
    float oxytocin_inverse = (1.0f - m->oxytocin_level) * 0.3f;
    float friend_contribution = (m->close_friends_count == 0) ? 0.3f :
                                (m->close_friends_count < 3) ? 0.1f : 0.0f;

    m->social_prediction_error = nimcp_clampf(
        loneliness_contribution + oxytocin_inverse + friend_contribution,
        0.0f, 1.0f
    );

    /*=========================================================================
     * COMPUTE RELATIONSHIP UNCERTAINTY
     *=========================================================================*/

    /* Relationship uncertainty based on:
     * - Number of relationships (more relationships = more potential uncertainty)
     * - Trust levels (low trust = high uncertainty)
     * - Oxytocin levels (low oxytocin = weaker bonds, more uncertainty)
     */
    float trust_uncertainty = (1.0f - m->oxytocin_level) * 0.5f;
    float relationship_base = (m->close_friends_count > 0) ? 0.2f : 0.5f;

    m->relationship_uncertainty = nimcp_clampf(
        trust_uncertainty + relationship_base * 0.5f,
        0.0f, 1.0f
    );

    /*=========================================================================
     * COMPUTE NORM VIOLATION SURPRISE
     *=========================================================================*/

    /* Norm violations would normally come from actual violation events
     * For now, we use loneliness as a proxy (isolation = unexpected social absence)
     */
    m->norm_violation_surprise = nimcp_clampf(
        m->loneliness * 0.5f,
        0.0f, 1.0f
    );

    /*=========================================================================
     * COMPUTE TRUST AND COOPERATION PREDICTION ERRORS
     *=========================================================================*/

    /* Trust prediction error: how wrong our trust predictions are */
    m->trust_prediction_error = nimcp_clampf(
        (1.0f - m->oxytocin_level) * 0.4f,
        0.0f, 1.0f
    );

    /* Cooperation prediction error */
    m->cooperation_prediction_error = nimcp_clampf(
        m->social_prediction_error * 0.8f,
        0.0f, 1.0f
    );

    /*=========================================================================
     * COMPUTE TOTAL FREE ENERGY
     *=========================================================================*/

    float social_prediction_contrib = m->social_prediction_error *
                                      cfg->social_prediction_error_weight;
    float uncertainty_contrib = m->relationship_uncertainty *
                                cfg->relationship_uncertainty_weight;
    float norm_contrib = m->norm_violation_surprise * cfg->norm_violation_weight;

    float total_fe = cfg->baseline_free_energy +
                     social_prediction_contrib +
                     uncertainty_contrib +
                     norm_contrib;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /*=========================================================================
     * COMPUTE AGGREGATE PREDICTION ERROR
     *=========================================================================*/

    /* Overall prediction error is weighted average */
    float new_error = (m->social_prediction_error * 0.4f +
                       m->trust_prediction_error * 0.3f +
                       m->cooperation_prediction_error * 0.3f);

    /* Apply decay and blend with previous error */
    m->prediction_error = nimcp_clampf(
        new_error * cfg->error_decay_rate +
        bridge->prev_prediction_error * (1.0f - cfg->error_decay_rate),
        0.0f, 1.0f
    );

    /*=========================================================================
     * COMPUTE SURPRISE
     *=========================================================================*/

    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float uncertainty_change = fabsf(m->relationship_uncertainty -
                                     bridge->prev_relationship_uncertainty);
    float prediction_change = fabsf(m->social_prediction_error -
                                    bridge->prev_prediction_error);

    m->surprise = nimcp_clampf(
        (fe_change * 0.4f + uncertainty_change * 0.3f + prediction_change * 0.3f),
        0.0f, 1.0f
    );

    /*=========================================================================
     * COMPUTE ENTROPY
     *=========================================================================*/

    /* Entropy based on relationship uncertainty and social state variability */
    m->entropy = nimcp_clampf(
        m->relationship_uncertainty * 0.5f +
        m->loneliness * 0.3f +
        (1.0f - m->oxytocin_level) * 0.2f,
        0.0f, 1.0f
    );

    /*=========================================================================
     * UPDATE RELATIONSHIP METRICS
     *=========================================================================*/

    m->active_relationships = (m->close_friends_count > 0) ?
                              m->close_friends_count + 2 : 0;  /* Estimate */
    m->avg_relationship_closeness = m->oxytocin_level * 0.7f;
    m->avg_relationship_trust = m->oxytocin_level * 0.8f;
    m->social_warmth = (1.0f - m->loneliness) * m->oxytocin_level;
}

/**
 * @brief Check and trigger callbacks based on current state
 */
static void check_callbacks(social_fep_bridge_t* bridge) {
    social_fep_metrics_t* m = &bridge->metrics;
    const social_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != SOCIAL_FEP_STATE_DEGRADED) {
            bridge->state = SOCIAL_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                bridge->high_fe_callback(bridge, m->free_energy,
                                         bridge->high_fe_user_data);
            }
        }
    } else if (bridge->state == SOCIAL_FEP_STATE_DEGRADED) {
        bridge->state = SOCIAL_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (cfg->enable_surprise_callbacks &&
        m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->social_prediction_error > m->relationship_uncertainty &&
                m->social_prediction_error > m->norm_violation_surprise) {
                source = "social_prediction";
            } else if (m->relationship_uncertainty > m->norm_violation_surprise) {
                source = "relationship";
            } else {
                source = "norm_violation";
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
static void update_stats(social_fep_bridge_t* bridge, uint64_t update_time_us) {
    social_fep_stats_t* s = &bridge->stats;
    social_fep_metrics_t* m = &bridge->metrics;

    s->total_updates++;
    s->total_update_time_us += update_time_us;

    /* Track social predictions and relationship updates */
    s->social_predictions++;
    if (m->active_relationships > 0) {
        s->relationship_updates++;
    }

    /* Track norm violations */
    if (m->norm_violation_surprise > 0.5f) {
        s->norm_violation_events++;
    }

    /* Peak tracking */
    if (m->free_energy > s->peak_free_energy) {
        s->peak_free_energy = m->free_energy;
    }

    /* Running averages */
    bridge->running_count++;
    bridge->running_avg_fe = (bridge->running_avg_fe * (bridge->running_count - 1) +
                              m->free_energy) / bridge->running_count;
    bridge->running_avg_pe = (bridge->running_avg_pe * (bridge->running_count - 1) +
                              m->prediction_error) / bridge->running_count;

    s->avg_free_energy = bridge->running_avg_fe;
    s->avg_prediction_error = bridge->running_avg_pe;
    s->total_free_energy_contribution += m->free_energy;

    /* Update metrics timing */
    m->last_update_time_ms = get_time_ms();
    m->update_count++;
    s->avg_update_time_us = (float)s->total_update_time_us / (float)s->total_updates;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

social_fep_config_t social_fep_config_default(void) {
    social_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Behavior */
    config.enable_logging = false;

    /* Timing */
    config.update_interval_ms = SOCIAL_FEP_DEFAULT_UPDATE_MS;

    /* Weighting parameters */
    config.free_energy_weight = 1.0f;
    config.social_prediction_error_weight = SOCIAL_FEP_PREDICTION_WEIGHT;
    config.relationship_uncertainty_weight = SOCIAL_FEP_UNCERTAINTY_WEIGHT;
    config.norm_violation_weight = SOCIAL_FEP_NORM_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.uncertainty_threshold = 0.6f;

    /* Normalization */
    config.baseline_free_energy = SOCIAL_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = SOCIAL_FEP_MAX_FREE_ENERGY;
    config.error_decay_rate = SOCIAL_FEP_ERROR_DECAY_RATE;

    /* Behavior flags */
    config.enable_adaptive_weights = true;
    config.enable_degraded_mode = true;
    config.enable_surprise_callbacks = true;

    return config;
}

social_fep_bridge_t* social_fep_bridge_create(const social_fep_config_t* config) {
    social_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(social_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = social_fep_config_default();
    }

    /* Initialize base bridge (includes mutex creation) */
    if (bridge_base_init(&bridge->base, 0, "social_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "social_fep_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = SOCIAL_FEP_STATE_IDLE;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.social_prediction_error = 0.0f;
    bridge->metrics.relationship_uncertainty = 0.0f;
    bridge->metrics.norm_violation_surprise = 0.0f;
    bridge->metrics.trust_prediction_error = 0.0f;
    bridge->metrics.cooperation_prediction_error = 0.0f;
    bridge->metrics.loneliness = 0.0f;
    bridge->metrics.oxytocin_level = 0.5f;
    bridge->metrics.social_warmth = 0.5f;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_relationship_uncertainty = 0.0f;
    bridge->prev_closeness = 0.5f;
    bridge->prev_trust = 0.5f;

    NIMCP_LOGGING_INFO("Created %s bridge", "social_fep");
    return bridge;
}

void social_fep_bridge_destroy(social_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "social_fep");

    /* Unregister if still registered */
    if (bridge->registered) {
        social_fep_bridge_unregister(bridge);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int social_fep_bridge_reset(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(social_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.oxytocin_level = 0.5f;
    bridge->metrics.social_warmth = 0.5f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_relationship_uncertainty = 0.0f;
    bridge->prev_closeness = 0.5f;
    bridge->prev_trust = 0.5f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_avg_pe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(social_fep_stats_t));

    bridge->state = SOCIAL_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int social_fep_bridge_register(
    social_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    social_bond_system_t* social,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_register: required parameter is NULL (bridge, orchestrator)");
        return -1;
    }

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
    bridge->social = social;

    /* Register with FEP orchestrator */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "social_cognition",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        social_fep_update_callback,
        social_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->social = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "social_fep_bridge_register: orchestrator registration failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = SOCIAL_FEP_STATE_ACTIVE;

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_fep_bridge_unregister(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_unregister: bridge is NULL");
        return -1;
    }

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
    bridge->social = NULL;
    bridge->state = SOCIAL_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool social_fep_bridge_is_registered(social_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t social_fep_bridge_get_id(social_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE FUNCTIONS
 *===========================================================================*/

int social_fep_update_callback(void* handle) {
    social_fep_bridge_t* bridge = (social_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_update_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered and have a social system */
    if (!bridge->registered || !bridge->social) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_update_callback: required parameter is NULL (bridge->registered, bridge->social)");
        return -1;
    }

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_relationship_uncertainty = bridge->metrics.relationship_uncertainty;

    /* Compute free energy from social metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Check and trigger callbacks */
    check_callbacks(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

void social_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via social_fep_bridge_destroy() */
    (void)handle;
}

int social_fep_bridge_update(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_update: bridge is NULL");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "social_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "social_fep_bridge_update");

    /* If registered with orchestrator and has social system, do full update */
    if (bridge->registered && bridge->social) {
        return social_fep_update_callback(bridge);
    }

    return social_fep_bridge_force_update(bridge);
}

int social_fep_bridge_force_update(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_force_update: bridge is NULL");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "social_fep_bridge_force_update");
    BRIDGE_LGSS_GATE(bridge, "social_fep_bridge_force_update");

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_us = get_time_us();

    /* Store previous state */
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;

    /* If we have a social system attached, compute full metrics */
    if (bridge->social) {
        compute_free_energy(bridge);
    } else {
        /* Minimal update without social system */
        bridge->metrics.last_update_time_ms = get_time_ms();
        bridge->metrics.update_count++;
    }

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update stats */
    bridge->stats.total_updates++;
    bridge->stats.total_update_time_us += update_time_us;
    bridge->stats.avg_update_time_us = (float)bridge->stats.total_update_time_us /
                                       (float)bridge->stats.total_updates;

    /* Invoke callbacks even without full social data */
    check_callbacks(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int social_fep_bridge_get_metrics(
    const social_fep_bridge_t* bridge,
    social_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_get_metrics: required parameter is NULL (bridge, metrics_out)");
        return -1;
    }

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int social_fep_bridge_get_stats(
    const social_fep_bridge_t* bridge,
    social_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int social_fep_bridge_reset_stats(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(social_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_avg_pe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * FREE ENERGY ACCESSORS
 *===========================================================================*/

float social_fep_bridge_get_free_energy_contribution(social_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float social_fep_bridge_get_social_prediction_error(social_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.social_prediction_error;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

float social_fep_bridge_get_relationship_uncertainty(social_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    float uncertainty = bridge->metrics.relationship_uncertainty;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return uncertainty;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

social_fep_state_t social_fep_bridge_get_state(social_fep_bridge_t* bridge) {
    if (!bridge) return SOCIAL_FEP_STATE_ERROR;

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    social_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool social_fep_bridge_is_degraded(social_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == SOCIAL_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

const char* social_fep_state_name(social_fep_state_t state) {
    switch (state) {
        case SOCIAL_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case SOCIAL_FEP_STATE_IDLE:          return "idle";
        case SOCIAL_FEP_STATE_ACTIVE:        return "active";
        case SOCIAL_FEP_STATE_DEGRADED:      return "degraded";
        case SOCIAL_FEP_STATE_ERROR:         return "error";
        default:                              return "unknown";
    }
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int social_fep_bridge_set_high_fe_callback(
    social_fep_bridge_t* bridge,
    social_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_set_high_fe_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_fep_bridge_set_surprise_callback(
    social_fep_bridge_t* bridge,
    social_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_set_surprise_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_fep_bridge_set_metrics_callback(
    social_fep_bridge_t* bridge,
    social_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_set_metrics_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int social_fep_bridge_set_config(
    social_fep_bridge_t* bridge,
    const social_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_fep_bridge_get_config(
    const social_fep_bridge_t* bridge,
    social_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_fep_bridge_get_config: required parameter is NULL (bridge, config_out)");
        return -1;
    }

    nimcp_mutex_lock(((social_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((social_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void social_fep_bridge_set_instance_health_agent(social_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "social_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int social_fep_bridge_training_begin(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    social_fep_bridge_heartbeat_instance(bridge->health_agent, "social_fep_bridge_training_begin", 0.0f);
    return 0;
}

int social_fep_bridge_training_end(social_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_fep_bridge_training_end: NULL argument");
        return -1;
    }
    social_fep_bridge_heartbeat_instance(bridge->health_agent, "social_fep_bridge_training_end", 1.0f);
    return 0;
}

int social_fep_bridge_training_step(social_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    social_fep_bridge_heartbeat_instance(bridge->health_agent, "social_fep_bridge_training_step", progress);
    return 0;
}
