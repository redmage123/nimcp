//=============================================================================
// nimcp_pr_hypo_bridge.c - Prime Resonant Hypothalamus Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_hypo_bridge.c
 * @brief Implementation of neuromodulator-quaternion mapping for PR Memory
 *
 * WHAT: Implements neuromodulator tracking, quaternion mapping, stress modulation
 * WHY:  Enable biologically-realistic neuromodulator effects on memory
 * HOW:  State tracking, mapping functions, Yerkes-Dodson curve implementation
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/memory/core/nimcp_pr_hypo_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_hypo_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_hypo_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_hypo_bridge_mesh_registry = NULL;

nimcp_error_t pr_hypo_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_hypo_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_hypo_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_hypo_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_hypo_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_hypo_bridge_mesh_registry = registry;
    return err;
}

void pr_hypo_bridge_mesh_unregister(void) {
    if (g_pr_hypo_bridge_mesh_registry && g_pr_hypo_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_hypo_bridge_mesh_registry, g_pr_hypo_bridge_mesh_id);
        g_pr_hypo_bridge_mesh_id = 0;
        g_pr_hypo_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_hypo_bridge module (instance-level) */
static inline void pr_hypo_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_hypo_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_hypo_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_hypo_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_HYPO_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Platform Abstraction
//=============================================================================

#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION pr_hypo_mutex_t;
    #define PR_HYPO_MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define PR_HYPO_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define PR_HYPO_MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define PR_HYPO_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
    #include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
    typedef nimcp_mutex_t pr_hypo_mutex_t;
    #define PR_HYPO_MUTEX_INIT(m) nimcp_mutex_init(&(m), NULL)
    #define PR_HYPO_MUTEX_DESTROY(m) nimcp_mutex_destroy(&(m))
    #define PR_HYPO_MUTEX_LOCK(m) nimcp_mutex_lock(&(m))
    #define PR_HYPO_MUTEX_UNLOCK(m) nimcp_mutex_unlock(&(m))
#endif

/* High-resolution timing */
#ifdef _WIN32
static uint64_t get_time_ms(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)((count.QuadPart * 1000) / freq.QuadPart);
}
#else
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}
#endif

//=============================================================================
// Helper Functions
//=============================================================================

static float lerp_f(float a, float b, float t) {
    return a + (b - a) * nimcp_myelin_clamp(t, 0.0f, 1.0f);
}

/**
 * @brief Compute Yerkes-Dodson inverted-U curve
 *
 * Performance = 1 - (stress - optimal)^2 / (optimal^2)
 * Peaks at optimal_stress, falls off on both sides
 */
static float compute_yerkes_dodson(float stress_level, float optimal) {
    if (optimal < PR_HYPO_EPSILON) {
        optimal = PR_HYPO_OPTIMAL_STRESS;
    }

    float deviation = stress_level - optimal;
    float factor = 1.0f - (deviation * deviation) / (optimal * optimal);

    return nimcp_myelin_clamp(factor, 0.0f, 1.0f);
}

/**
 * @brief Determine stress state from cortisol level
 */
static pr_stress_state_t determine_stress_state(float cortisol,
                                                 float optimal,
                                                 float impairment_threshold) {
    if (cortisol < optimal * 0.5f) {
        return PR_STRESS_LOW;
    } else if (cortisol < optimal * 1.2f) {
        return PR_STRESS_OPTIMAL;
    } else if (cortisol < impairment_threshold) {
        return PR_STRESS_HIGH;
    } else if (cortisol > 0.9f) {
        return PR_STRESS_ACUTE;
    } else {
        return PR_STRESS_HIGH;
    }
}

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Internal bridge structure
 */
struct pr_hypo_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    pr_hypo_config_t config;

    /* Neuromodulator states */
    pr_neuromod_state_t neuromod_states[PR_HYPO_NEUROMOD_COUNT];
    pr_hypo_mutex_t neuromod_mutex;
    bool neuromod_mutex_initialized;

    /* Stress state */
    pr_stress_state_info_t stress_state;
    pr_hypo_mutex_t stress_mutex;
    bool stress_mutex_initialized;

    /* Reward signal */
    pr_reward_signal_t current_reward;
    bool has_active_reward;
    pr_hypo_mutex_t reward_mutex;
    bool reward_mutex_initialized;

    /* History buffer */
    pr_hypo_history_entry_t* history;
    size_t history_size;
    size_t history_capacity;
    size_t history_write_idx;
    pr_hypo_mutex_t history_mutex;
    bool history_mutex_initialized;

    /* Callbacks */
    pr_neuromod_callback_t neuromod_callback;
    void* neuromod_callback_data;
    pr_reward_callback_t reward_callback;
    void* reward_callback_data;
    pr_stress_callback_t stress_callback;
    void* stress_callback_data;

    /* Statistics */
    pr_hypo_stats_t stats;
    pr_hypo_mutex_t stats_mutex;
    bool stats_mutex_initialized;

    /* State */
    bool initialized;
    uint64_t last_update_ms;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_hypo_bridge, struct pr_hypo_bridge_struct)

//=============================================================================
// Default Mappings
//=============================================================================

/**
 * @brief Get default neuromodulator-quaternion mapping
 */
static pr_neuromod_mapping_t get_default_mapping(pr_neuromod_type_t type) {
    pr_neuromod_mapping_t mapping = {0};
    mapping.threshold = 0.1f;
    mapping.saturation = 0.9f;
    mapping.is_inverted_u = false;

    switch (type) {
        case PR_NEUROMOD_DOPAMINE:
            /* DA: Boosts consolidation and accessibility */
            mapping.w_effect = PR_HYPO_DA_REWARD_BOOST;
            mapping.x_effect = 0.1f;  /* Slight positive emotion */
            mapping.y_effect = 0.0f;
            mapping.z_effect = 0.2f;  /* Accessibility boost */
            break;

        case PR_NEUROMOD_SEROTONIN:
            /* 5-HT: Mood modulation, consolidation stabilization */
            mapping.w_effect = 0.1f;
            mapping.x_effect = PR_HYPO_5HT_MOOD_FACTOR;  /* Main emotional effect */
            mapping.y_effect = -0.1f;  /* Reduces salience (calming) */
            mapping.z_effect = 0.0f;
            break;

        case PR_NEUROMOD_NOREPINEPHRINE:
            /* NE: Arousal, salience, flashbulb encoding */
            mapping.w_effect = 0.15f;
            mapping.x_effect = 0.0f;
            mapping.y_effect = PR_HYPO_NE_SALIENCE_BOOST;  /* Main salience boost */
            mapping.z_effect = 0.2f;  /* Accessibility boost */
            break;

        case PR_NEUROMOD_ACETYLCHOLINE:
            /* ACh: Attention focus, learning readiness */
            mapping.w_effect = 0.05f;
            mapping.x_effect = 0.0f;
            mapping.y_effect = PR_HYPO_ACH_ATTENTION_FACTOR;
            mapping.z_effect = PR_HYPO_ACH_ATTENTION_FACTOR;
            break;

        case PR_NEUROMOD_CORTISOL:
            /* Cortisol: Inverted-U stress response */
            mapping.w_effect = 0.3f;  /* Can boost or impair */
            mapping.x_effect = 0.0f;
            mapping.y_effect = 0.2f;  /* Increases salience */
            mapping.z_effect = -0.1f;  /* May reduce accessibility */
            mapping.is_inverted_u = true;
            break;

        case PR_NEUROMOD_OXYTOCIN:
            /* Oxytocin: Social bonding, positive emotion */
            mapping.w_effect = 0.15f;
            mapping.x_effect = 0.3f;  /* Positive emotion boost */
            mapping.y_effect = 0.1f;
            mapping.z_effect = 0.0f;
            break;

        case PR_NEUROMOD_ENDORPHIN:
            /* Endorphins: Pain reduction, positive affect */
            mapping.w_effect = 0.05f;
            mapping.x_effect = 0.2f;  /* Positive shift */
            mapping.y_effect = -0.1f;  /* Reduced salience (pain dampening) */
            mapping.z_effect = 0.0f;
            break;

        case PR_NEUROMOD_GABA:
            /* GABA: Inhibitory, calming */
            mapping.w_effect = 0.0f;
            mapping.x_effect = 0.0f;
            mapping.y_effect = -0.3f;  /* Reduces salience */
            mapping.z_effect = -0.1f;  /* May reduce accessibility */
            break;

        default:
            break;
    }

    return mapping;
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_config_t pr_hypo_config_default(void) {
    pr_hypo_config_t config;
    memset(&config, 0, sizeof(config));

    /* History tracking */
    config.history_buffer_size = PR_HYPO_DEFAULT_HISTORY_SIZE;
    config.track_history = true;

    /* Baseline levels */
    for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
        }

        config.baseline_levels[i] = PR_HYPO_BASELINE_LEVEL;
        config.decay_rates[i] = PR_HYPO_STRESS_DECAY_RATE;
    }

    /* Stress parameters */
    config.optimal_stress_level = PR_HYPO_OPTIMAL_STRESS;
    config.stress_impairment_threshold = PR_HYPO_CORTISOL_IMPAIR_THRESH;
    config.chronic_stress_factor = 0.01f;

    /* Reward parameters */
    config.reward_da_boost = PR_HYPO_DA_REWARD_BOOST;
    config.reward_decay_rate = PR_HYPO_REWARD_DECAY_RATE;
    config.prediction_error_sensitivity = 0.5f;

    /* Default mappings */
    config.use_default_mappings = true;
    for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
        }

        config.mappings[i] = get_default_mapping((pr_neuromod_type_t)i);
    }

    /* Integration */
    config.enable_z_ladder_boost = true;
    config.z_ladder_boost_factor = 0.2f;

    return config;
}

NIMCP_EXPORT bool pr_hypo_config_validate(const pr_hypo_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_hypo_config_validate: config is NULL");
        return false;
    }

    if (config->history_buffer_size == 0 && config->track_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_hypo_config_validate: config->history_buffer_size is zero");
        return false;
    }
    if (config->optimal_stress_level < 0.0f || config->optimal_stress_level > 1.0f) {
        return false;
    }
    if (config->stress_impairment_threshold < 0.0f || config->stress_impairment_threshold > 1.0f) {
        return false;
    }
    if (config->reward_decay_rate < 0.0f) {
        return false;
    }

    return true;
}

NIMCP_EXPORT pr_neuromod_mapping_t pr_hypo_default_mapping(pr_neuromod_type_t type) {
    if (type >= PR_NEUROMOD_COUNT) {
        pr_neuromod_mapping_t empty = {0};
        return empty;
    }
    return get_default_mapping(type);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_bridge_t pr_hypo_bridge_create(const pr_hypo_config_t* config) {
    pr_hypo_config_t cfg;
    if (config) {
        if (!pr_hypo_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_hypo_bridge_create: config validation failed");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_hypo_config_default();
    }

    /* Allocate bridge */
    pr_hypo_bridge_t bridge = (pr_hypo_bridge_t)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->config = cfg;

    /* Initialize bridge base infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_hypo_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_hypo_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Initialize neuromodulator states */
    for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
        }

        bridge->neuromod_states[i].type = (pr_neuromod_type_t)i;
        bridge->neuromod_states[i].concentration = cfg.baseline_levels[i];
        bridge->neuromod_states[i].baseline = cfg.baseline_levels[i];
        bridge->neuromod_states[i].velocity = 0.0f;
        bridge->neuromod_states[i].decay_rate = cfg.decay_rates[i];
        bridge->neuromod_states[i].last_update_ms = get_time_ms();
    }

    /* Initialize stress state */
    bridge->stress_state.state = PR_STRESS_LOW;
    bridge->stress_state.cortisol_level = cfg.baseline_levels[PR_NEUROMOD_CORTISOL];
    bridge->stress_state.baseline_cortisol = cfg.baseline_levels[PR_NEUROMOD_CORTISOL];
    bridge->stress_state.acute_stress_boost = 0.0f;
    bridge->stress_state.chronic_stress_factor = 0.0f;
    bridge->stress_state.stress_onset_ms = 0;
    bridge->stress_state.last_update_ms = get_time_ms();
    bridge->stress_state.performance_factor = compute_yerkes_dodson(
        bridge->stress_state.cortisol_level, cfg.optimal_stress_level);

    /* Allocate history buffer */
    if (cfg.track_history && cfg.history_buffer_size > 0) {
        bridge->history = (pr_hypo_history_entry_t*)nimcp_calloc(
            cfg.history_buffer_size, sizeof(pr_hypo_history_entry_t));
        if (!bridge->history) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_hypo_bridge_create: bridge->history is NULL");
            return NULL;
        }
        bridge->history_capacity = cfg.history_buffer_size;
    }
    bridge->history_size = 0;
    bridge->history_write_idx = 0;

    /* Initialize mutexes */
    PR_HYPO_MUTEX_INIT(bridge->neuromod_mutex);
    bridge->neuromod_mutex_initialized = true;

    PR_HYPO_MUTEX_INIT(bridge->stress_mutex);
    bridge->stress_mutex_initialized = true;

    PR_HYPO_MUTEX_INIT(bridge->reward_mutex);
    bridge->reward_mutex_initialized = true;

    PR_HYPO_MUTEX_INIT(bridge->history_mutex);
    bridge->history_mutex_initialized = true;

    PR_HYPO_MUTEX_INIT(bridge->stats_mutex);
    bridge->stats_mutex_initialized = true;

    /* Initialize state */
    bridge->has_active_reward = false;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->last_update_ms = get_time_ms();
    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_hypo");
    return bridge;
}

NIMCP_EXPORT void pr_hypo_bridge_destroy(pr_hypo_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_hypo");

    /* Destroy mutexes */
    if (bridge->neuromod_mutex_initialized) {
        PR_HYPO_MUTEX_DESTROY(bridge->neuromod_mutex);
    }
    if (bridge->stress_mutex_initialized) {
        PR_HYPO_MUTEX_DESTROY(bridge->stress_mutex);
    }
    if (bridge->reward_mutex_initialized) {
        PR_HYPO_MUTEX_DESTROY(bridge->reward_mutex);
    }
    if (bridge->history_mutex_initialized) {
        PR_HYPO_MUTEX_DESTROY(bridge->history_mutex);
    }
    if (bridge->stats_mutex_initialized) {
        PR_HYPO_MUTEX_DESTROY(bridge->stats_mutex);
    }

    /* Free history buffer */
    if (bridge->history) {
        nimcp_free(bridge->history);
    }

    /* Cleanup bridge base infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reset(pr_hypo_bridge_t bridge) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    uint64_t now = get_time_ms();

    /* Reset neuromodulator states */
    PR_HYPO_MUTEX_LOCK(bridge->neuromod_mutex);
    for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
        }

        bridge->neuromod_states[i].concentration = bridge->config.baseline_levels[i];
        bridge->neuromod_states[i].velocity = 0.0f;
        bridge->neuromod_states[i].last_update_ms = now;
    }
    PR_HYPO_MUTEX_UNLOCK(bridge->neuromod_mutex);

    /* Reset stress state */
    PR_HYPO_MUTEX_LOCK(bridge->stress_mutex);
    bridge->stress_state.state = PR_STRESS_LOW;
    bridge->stress_state.cortisol_level = bridge->stress_state.baseline_cortisol;
    bridge->stress_state.acute_stress_boost = 0.0f;
    bridge->stress_state.chronic_stress_factor = 0.0f;
    bridge->stress_state.last_update_ms = now;
    bridge->stress_state.performance_factor = compute_yerkes_dodson(
        bridge->stress_state.cortisol_level, bridge->config.optimal_stress_level);
    PR_HYPO_MUTEX_UNLOCK(bridge->stress_mutex);

    /* Reset reward state */
    PR_HYPO_MUTEX_LOCK(bridge->reward_mutex);
    bridge->has_active_reward = false;
    memset(&bridge->current_reward, 0, sizeof(bridge->current_reward));
    PR_HYPO_MUTEX_UNLOCK(bridge->reward_mutex);

    /* Clear history */
    if (bridge->config.track_history) {
        PR_HYPO_MUTEX_LOCK(bridge->history_mutex);
        bridge->history_size = 0;
        bridge->history_write_idx = 0;
        PR_HYPO_MUTEX_UNLOCK(bridge->history_mutex);
    }

    bridge->last_update_ms = now;

    return PR_HYPO_SUCCESS;
}

//=============================================================================
// Neuromodulator Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_neuromod(
    pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type,
    float concentration
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;
    if (type >= PR_NEUROMOD_COUNT) return PR_HYPO_ERROR_INVALID_NEUROMOD;

    concentration = nimcp_myelin_clamp(concentration, PR_HYPO_MIN_CONCENTRATION,
                            PR_HYPO_MAX_CONCENTRATION);

    float old_level = 0.0f;

    PR_HYPO_MUTEX_LOCK(bridge->neuromod_mutex);
    old_level = bridge->neuromod_states[type].concentration;
    bridge->neuromod_states[type].concentration = concentration;
    bridge->neuromod_states[type].last_update_ms = get_time_ms();
    PR_HYPO_MUTEX_UNLOCK(bridge->neuromod_mutex);

    /* Update cortisol sync for stress */
    if (type == PR_NEUROMOD_CORTISOL) {
        PR_HYPO_MUTEX_LOCK(bridge->stress_mutex);
        bridge->stress_state.cortisol_level = concentration;
        pr_stress_state_t old_state = bridge->stress_state.state;
        bridge->stress_state.state = determine_stress_state(
            concentration,
            bridge->config.optimal_stress_level,
            bridge->config.stress_impairment_threshold);
        bridge->stress_state.performance_factor = compute_yerkes_dodson(
            concentration, bridge->config.optimal_stress_level);
        pr_stress_state_t new_state = bridge->stress_state.state;
        PR_HYPO_MUTEX_UNLOCK(bridge->stress_mutex);

        /* Invoke stress callback if state changed */
        if (old_state != new_state && bridge->stress_callback) {
            bridge->stress_callback(old_state, new_state, concentration,
                                    bridge->stress_callback_data);
        }
    }

    /* Update statistics */
    PR_HYPO_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.modulations_applied++;
    bridge->stats.modulations_per_type[type]++;
    if (concentration > bridge->stats.peak_concentrations[type]) {
        bridge->stats.peak_concentrations[type] = concentration;
    }
    /* Running average */
    uint64_t n = bridge->stats.modulations_per_type[type];
    bridge->stats.avg_concentrations[type] =
        (bridge->stats.avg_concentrations[type] * (float)(n - 1) + concentration) / (float)n;
    bridge->stats.last_update_ms = get_time_ms();
    PR_HYPO_MUTEX_UNLOCK(bridge->stats_mutex);

    /* Invoke callback */
    if (bridge->neuromod_callback && fabsf(concentration - old_level) > PR_HYPO_EPSILON) {
        bridge->neuromod_callback(type, old_level, concentration,
                                   bridge->neuromod_callback_data);
    }

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT float pr_hypo_bridge_get_neuromod(
    const pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type
) {
    if (!bridge || type >= PR_NEUROMOD_COUNT) return -1.0f;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->neuromod_mutex);
    float concentration = bridge->neuromod_states[type].concentration;
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->neuromod_mutex);

    return concentration;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_apply_pulse(
    pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type,
    float pulse_magnitude,
    uint64_t decay_time_ms
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;
    if (type >= PR_NEUROMOD_COUNT) return PR_HYPO_ERROR_INVALID_NEUROMOD;

    pulse_magnitude = nimcp_myelin_clamp(pulse_magnitude, 0.0f, 1.0f);

    PR_HYPO_MUTEX_LOCK(bridge->neuromod_mutex);

    float current = bridge->neuromod_states[type].concentration;
    float new_level = nimcp_myelin_clamp(current + pulse_magnitude, 0.0f, 1.0f);
    float baseline = bridge->neuromod_states[type].baseline;

    /* Set velocity for decay back to baseline */
    if (decay_time_ms > 0) {
        float decay_amount = new_level - baseline;
        bridge->neuromod_states[type].velocity = -decay_amount / ((float)decay_time_ms / 1000.0f);
    }

    bridge->neuromod_states[type].concentration = new_level;
    bridge->neuromod_states[type].last_update_ms = get_time_ms();

    PR_HYPO_MUTEX_UNLOCK(bridge->neuromod_mutex);

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_neuromod_state(
    const pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type,
    pr_neuromod_state_t* state
) {
    if (!bridge || !state) return PR_HYPO_ERROR_NULL_POINTER;
    if (type >= PR_NEUROMOD_COUNT) return PR_HYPO_ERROR_INVALID_NEUROMOD;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->neuromod_mutex);
    *state = bridge->neuromod_states[type];
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->neuromod_mutex);

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_update(
    pr_hypo_bridge_t bridge,
    uint64_t elapsed_ms
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    float elapsed_sec = (float)elapsed_ms / 1000.0f;
    uint64_t now = get_time_ms();

    /* Update all neuromodulators */
    PR_HYPO_MUTEX_LOCK(bridge->neuromod_mutex);
    for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
        }

        pr_neuromod_state_t* state = &bridge->neuromod_states[i];

        /* Apply velocity */
        if (fabsf(state->velocity) > PR_HYPO_EPSILON) {
            state->concentration += state->velocity * elapsed_sec;

            /* Check if returned to baseline (if velocity pushing towards baseline) */
            if ((state->velocity < 0 && state->concentration <= state->baseline) ||
                (state->velocity > 0 && state->concentration >= state->baseline)) {
                state->concentration = state->baseline;
                state->velocity = 0.0f;
            }
        }

        /* Natural decay towards baseline */
        float diff = state->concentration - state->baseline;
        if (fabsf(diff) > PR_HYPO_EPSILON && fabsf(state->velocity) < PR_HYPO_EPSILON) {
            float decay = diff * state->decay_rate * elapsed_sec;
            state->concentration -= decay;
        }

        state->concentration = nimcp_myelin_clamp(state->concentration, 0.0f, 1.0f);
        state->last_update_ms = now;
    }
    PR_HYPO_MUTEX_UNLOCK(bridge->neuromod_mutex);

    /* Update stress state */
    PR_HYPO_MUTEX_LOCK(bridge->stress_mutex);
    float cortisol = bridge->neuromod_states[PR_NEUROMOD_CORTISOL].concentration;
    bridge->stress_state.cortisol_level = cortisol;

    /* Decay acute stress boost */
    if (bridge->stress_state.acute_stress_boost > PR_HYPO_EPSILON) {
        bridge->stress_state.acute_stress_boost -= PR_HYPO_STRESS_DECAY_RATE * elapsed_sec;
        if (bridge->stress_state.acute_stress_boost < 0) {
            bridge->stress_state.acute_stress_boost = 0.0f;
        }
    }

    bridge->stress_state.state = determine_stress_state(
        cortisol + bridge->stress_state.acute_stress_boost,
        bridge->config.optimal_stress_level,
        bridge->config.stress_impairment_threshold);
    bridge->stress_state.performance_factor = compute_yerkes_dodson(
        cortisol + bridge->stress_state.acute_stress_boost,
        bridge->config.optimal_stress_level);
    bridge->stress_state.last_update_ms = now;
    PR_HYPO_MUTEX_UNLOCK(bridge->stress_mutex);

    /* Update reward decay */
    PR_HYPO_MUTEX_LOCK(bridge->reward_mutex);
    if (bridge->has_active_reward) {
        bridge->current_reward.decay_factor -= bridge->config.reward_decay_rate * elapsed_sec;
        if (bridge->current_reward.decay_factor <= 0.0f) {
            bridge->has_active_reward = false;
            bridge->current_reward.decay_factor = 0.0f;
        }
    }
    PR_HYPO_MUTEX_UNLOCK(bridge->reward_mutex);

    /* Record history */
    if (bridge->config.track_history && bridge->history) {
        PR_HYPO_MUTEX_LOCK(bridge->history_mutex);
        size_t idx = bridge->history_write_idx;
        bridge->history[idx].timestamp_ms = now;
        for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
                pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                                 (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
            }

            bridge->history[idx].concentrations[i] =
                bridge->neuromod_states[i].concentration;
        }
        bridge->history[idx].stress_level = bridge->stress_state.cortisol_level;
        bridge->history[idx].reward_signal = bridge->has_active_reward ?
            bridge->current_reward.magnitude * bridge->current_reward.decay_factor : 0.0f;

        bridge->history_write_idx = (idx + 1) % bridge->history_capacity;
        if (bridge->history_size < bridge->history_capacity) {
            bridge->history_size++;
        }
        PR_HYPO_MUTEX_UNLOCK(bridge->history_mutex);
    }

    bridge->last_update_ms = now;

    return PR_HYPO_SUCCESS;
}

//=============================================================================
// Quaternion Mapping Functions
//=============================================================================

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_apply_neuromodulator(
    pr_hypo_bridge_t bridge,
    nimcp_quaternion_t input
) {
    if (!bridge) return input;

    nimcp_quaternion_t result = input;

    PR_HYPO_MUTEX_LOCK(bridge->neuromod_mutex);

    for (int i = 0; i < PR_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_NEUROMOD_COUNT);
        }

        pr_neuromod_type_t type = (pr_neuromod_type_t)i;
        float concentration = bridge->neuromod_states[i].concentration;
        float baseline = bridge->neuromod_states[i].baseline;

        /* Deviation from baseline */
        float deviation = concentration - baseline;
        if (fabsf(deviation) < PR_HYPO_EPSILON) continue;

        pr_neuromod_mapping_t mapping;
        if (bridge->config.use_default_mappings) {
            mapping = get_default_mapping(type);
        } else {
            mapping = bridge->config.mappings[i];
        }

        /* Check threshold */
        if (fabsf(concentration - baseline) < mapping.threshold) continue;

        /* Compute effect magnitude */
        float effect_magnitude = deviation;
        if (mapping.is_inverted_u) {
            effect_magnitude = compute_yerkes_dodson(concentration,
                bridge->config.optimal_stress_level) - 0.5f;
        }

        /* Apply to quaternion */
        result.w += mapping.w_effect * effect_magnitude;
        result.x += mapping.x_effect * effect_magnitude;
        result.y += mapping.y_effect * effect_magnitude;
        result.z += mapping.z_effect * effect_magnitude;
    }

    PR_HYPO_MUTEX_UNLOCK(bridge->neuromod_mutex);

    /* Clamp to valid ranges */
    result.w = nimcp_myelin_clamp(result.w, 0.0f, 1.0f);
    result.x = nimcp_myelin_clamp(result.x, -1.0f, 1.0f);
    result.y = nimcp_myelin_clamp(result.y, 0.0f, 1.0f);
    result.z = nimcp_myelin_clamp(result.z, 0.0f, 1.0f);

    /* Update statistics */
    PR_HYPO_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.quaternions_modified++;
    PR_HYPO_MUTEX_UNLOCK(bridge->stats_mutex);

    return result;
}

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_apply_single_neuromod(
    pr_hypo_bridge_t bridge,
    nimcp_quaternion_t input,
    pr_neuromod_type_t type
) {
    if (!bridge || type >= PR_NEUROMOD_COUNT) return input;

    nimcp_quaternion_t result = input;

    PR_HYPO_MUTEX_LOCK(bridge->neuromod_mutex);

    float concentration = bridge->neuromod_states[type].concentration;
    float baseline = bridge->neuromod_states[type].baseline;
    float deviation = concentration - baseline;

    PR_HYPO_MUTEX_UNLOCK(bridge->neuromod_mutex);

    if (fabsf(deviation) < PR_HYPO_EPSILON) return result;

    pr_neuromod_mapping_t mapping;
    if (bridge->config.use_default_mappings) {
        mapping = get_default_mapping(type);
    } else {
        mapping = bridge->config.mappings[type];
    }

    float effect_magnitude = deviation;
    if (mapping.is_inverted_u) {
        effect_magnitude = compute_yerkes_dodson(concentration,
            bridge->config.optimal_stress_level) - 0.5f;
    }

    result.w = nimcp_myelin_clamp(result.w + mapping.w_effect * effect_magnitude, 0.0f, 1.0f);
    result.x = nimcp_myelin_clamp(result.x + mapping.x_effect * effect_magnitude, -1.0f, 1.0f);
    result.y = nimcp_myelin_clamp(result.y + mapping.y_effect * effect_magnitude, 0.0f, 1.0f);
    result.z = nimcp_myelin_clamp(result.z + mapping.z_effect * effect_magnitude, 0.0f, 1.0f);

    return result;
}

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_map_dopamine_to_quaternion(
    pr_hypo_bridge_t bridge,
    float da_level
) {
    nimcp_quaternion_t delta = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!bridge) return delta;

    da_level = nimcp_myelin_clamp(da_level, 0.0f, 1.0f);

    pr_neuromod_mapping_t mapping;
    if (bridge->config.use_default_mappings) {
        mapping = get_default_mapping(PR_NEUROMOD_DOPAMINE);
    } else {
        mapping = bridge->config.mappings[PR_NEUROMOD_DOPAMINE];
    }

    float baseline = bridge->config.baseline_levels[PR_NEUROMOD_DOPAMINE];
    float deviation = da_level - baseline;

    delta.w = mapping.w_effect * deviation;
    delta.x = mapping.x_effect * deviation;
    delta.y = mapping.y_effect * deviation;
    delta.z = mapping.z_effect * deviation;

    return delta;
}

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_map_serotonin_to_quaternion(
    pr_hypo_bridge_t bridge,
    float serotonin_level
) {
    nimcp_quaternion_t delta = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!bridge) return delta;

    serotonin_level = nimcp_myelin_clamp(serotonin_level, 0.0f, 1.0f);

    pr_neuromod_mapping_t mapping;
    if (bridge->config.use_default_mappings) {
        mapping = get_default_mapping(PR_NEUROMOD_SEROTONIN);
    } else {
        mapping = bridge->config.mappings[PR_NEUROMOD_SEROTONIN];
    }

    float baseline = bridge->config.baseline_levels[PR_NEUROMOD_SEROTONIN];
    float deviation = serotonin_level - baseline;

    delta.w = mapping.w_effect * deviation;
    delta.x = mapping.x_effect * deviation;
    delta.y = mapping.y_effect * deviation;
    delta.z = mapping.z_effect * deviation;

    return delta;
}

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_map_norepinephrine_to_quaternion(
    pr_hypo_bridge_t bridge,
    float ne_level
) {
    nimcp_quaternion_t delta = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!bridge) return delta;

    ne_level = nimcp_myelin_clamp(ne_level, 0.0f, 1.0f);

    pr_neuromod_mapping_t mapping;
    if (bridge->config.use_default_mappings) {
        mapping = get_default_mapping(PR_NEUROMOD_NOREPINEPHRINE);
    } else {
        mapping = bridge->config.mappings[PR_NEUROMOD_NOREPINEPHRINE];
    }

    float baseline = bridge->config.baseline_levels[PR_NEUROMOD_NOREPINEPHRINE];
    float deviation = ne_level - baseline;

    delta.w = mapping.w_effect * deviation;
    delta.x = mapping.x_effect * deviation;
    delta.y = mapping.y_effect * deviation;
    delta.z = mapping.z_effect * deviation;

    return delta;
}

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_get_effect_delta(
    const pr_hypo_bridge_t bridge
) {
    nimcp_quaternion_t delta = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!bridge) return delta;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->neuromod_mutex);

    for (int i = 0; i < PR_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_NEUROMOD_COUNT);
        }

        float concentration = bridge->neuromod_states[i].concentration;
        float baseline = bridge->neuromod_states[i].baseline;
        float deviation = concentration - baseline;

        if (fabsf(deviation) < PR_HYPO_EPSILON) continue;

        pr_neuromod_mapping_t mapping = get_default_mapping((pr_neuromod_type_t)i);
        float effect_magnitude = deviation;
        if (mapping.is_inverted_u) {
            effect_magnitude = compute_yerkes_dodson(concentration,
                bridge->config.optimal_stress_level) - 0.5f;
        }

        delta.w += mapping.w_effect * effect_magnitude;
        delta.x += mapping.x_effect * effect_magnitude;
        delta.y += mapping.y_effect * effect_magnitude;
        delta.z += mapping.z_effect * effect_magnitude;
    }

    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->neuromod_mutex);

    return delta;
}

//=============================================================================
// Stress Modulation Functions
//=============================================================================

NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_stress_modulation(
    pr_hypo_bridge_t bridge,
    nimcp_quaternion_t input
) {
    if (!bridge) return input;

    nimcp_quaternion_t result = input;

    PR_HYPO_MUTEX_LOCK(bridge->stress_mutex);

    float cortisol = bridge->stress_state.cortisol_level +
                     bridge->stress_state.acute_stress_boost;
    float performance = bridge->stress_state.performance_factor;
    pr_stress_state_t state = bridge->stress_state.state;

    PR_HYPO_MUTEX_UNLOCK(bridge->stress_mutex);

    /* Apply Yerkes-Dodson effect on consolidation */
    float consolidation_mod = (performance - 0.5f) * 0.4f;  /* +/- 0.2 */
    result.w = nimcp_myelin_clamp(result.w + consolidation_mod, 0.0f, 1.0f);

    /* High stress increases salience */
    if (state == PR_STRESS_HIGH || state == PR_STRESS_ACUTE) {
        result.y = nimcp_myelin_clamp(result.y + 0.2f, 0.0f, 1.0f);
    }

    /* Acute stress triggers flashbulb-like encoding */
    if (state == PR_STRESS_ACUTE) {
        result.w = nimcp_myelin_clamp(result.w + 0.3f, 0.0f, 1.0f);  /* Strong consolidation */
        result.y = nimcp_myelin_clamp(result.y + 0.3f, 0.0f, 1.0f);  /* High salience */
    }

    /* Very high stress can impair accessibility */
    if (cortisol > bridge->config.stress_impairment_threshold) {
        result.z = nimcp_myelin_clamp(result.z - 0.15f, 0.0f, 1.0f);
    }

    return result;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_stress_level(
    pr_hypo_bridge_t bridge,
    float cortisol_level
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    /* Set cortisol neuromodulator */
    return pr_hypo_bridge_set_neuromod(bridge, PR_NEUROMOD_CORTISOL, cortisol_level);
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_stress_state(
    const pr_hypo_bridge_t bridge,
    pr_stress_state_info_t* state
) {
    if (!bridge || !state) return PR_HYPO_ERROR_NULL_POINTER;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->stress_mutex);
    *state = bridge->stress_state;
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->stress_mutex);

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_trigger_acute_stress(
    pr_hypo_bridge_t bridge,
    float intensity
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    intensity = nimcp_myelin_clamp(intensity, 0.0f, 1.0f);

    pr_stress_state_t old_state;

    PR_HYPO_MUTEX_LOCK(bridge->stress_mutex);
    old_state = bridge->stress_state.state;
    bridge->stress_state.acute_stress_boost = intensity;
    bridge->stress_state.stress_onset_ms = get_time_ms();

    float total = bridge->stress_state.cortisol_level + intensity;
    bridge->stress_state.state = determine_stress_state(
        total,
        bridge->config.optimal_stress_level,
        bridge->config.stress_impairment_threshold);
    bridge->stress_state.performance_factor = compute_yerkes_dodson(
        total, bridge->config.optimal_stress_level);
    pr_stress_state_t new_state = bridge->stress_state.state;
    float cortisol = total;
    PR_HYPO_MUTEX_UNLOCK(bridge->stress_mutex);

    /* Also boost norepinephrine */
    pr_hypo_bridge_apply_pulse(bridge, PR_NEUROMOD_NOREPINEPHRINE, intensity * 0.5f, 2000);

    /* Update statistics */
    PR_HYPO_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.stress_events++;
    if (new_state == PR_STRESS_ACUTE) {
        bridge->stats.flashbulb_triggers++;
    }
    PR_HYPO_MUTEX_UNLOCK(bridge->stats_mutex);

    /* Invoke callback */
    if (bridge->stress_callback && old_state != new_state) {
        bridge->stress_callback(old_state, new_state, cortisol,
                                 bridge->stress_callback_data);
    }

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT float pr_hypo_bridge_get_performance_factor(
    const pr_hypo_bridge_t bridge
) {
    if (!bridge) return 0.5f;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->stress_mutex);
    float factor = bridge->stress_state.performance_factor;
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->stress_mutex);

    return factor;
}

//=============================================================================
// Reward Signal Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reward_signal(
    pr_hypo_bridge_t bridge,
    pr_reward_type_t type,
    float magnitude,
    float prediction_error
) {
    return pr_hypo_bridge_reward_signal_with_memory(bridge, type, magnitude,
                                                      prediction_error, 0);
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reward_signal_with_memory(
    pr_hypo_bridge_t bridge,
    pr_reward_type_t type,
    float magnitude,
    float prediction_error,
    uint64_t memory_id
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;
    if (type >= PR_REWARD_TYPE_COUNT) return PR_HYPO_ERROR_OUT_OF_RANGE;

    magnitude = nimcp_myelin_clamp(magnitude, -1.0f, 1.0f);
    prediction_error = nimcp_myelin_clamp(prediction_error, -1.0f, 1.0f);

    uint64_t now = get_time_ms();

    /* Create reward signal */
    pr_reward_signal_t signal;
    signal.type = type;
    signal.magnitude = magnitude;
    signal.prediction_error = prediction_error;
    signal.memory_id = memory_id;
    signal.timestamp_ms = now;
    signal.decay_factor = 1.0f;

    PR_HYPO_MUTEX_LOCK(bridge->reward_mutex);
    bridge->current_reward = signal;
    bridge->has_active_reward = true;
    PR_HYPO_MUTEX_UNLOCK(bridge->reward_mutex);

    /* Apply neuromodulator effects based on reward type */
    float da_boost = 0.0f;
    float ne_boost = 0.0f;
    float oxy_boost = 0.0f;

    switch (type) {
        case PR_REWARD_POSITIVE:
            da_boost = fabsf(magnitude) * bridge->config.reward_da_boost;
            break;
        case PR_REWARD_NEGATIVE:
            ne_boost = fabsf(magnitude) * 0.3f;
            break;
        case PR_REWARD_PREDICTION_ERROR:
            /* DA proportional to prediction error */
            da_boost = fabsf(prediction_error) * bridge->config.prediction_error_sensitivity;
            break;
        case PR_REWARD_SOCIAL:
            oxy_boost = fabsf(magnitude) * 0.3f;
            da_boost = fabsf(magnitude) * 0.15f;
            break;
        case PR_REWARD_NEUTRAL:
        default:
            break;
    }

    if (da_boost > PR_HYPO_EPSILON) {
        pr_hypo_bridge_apply_pulse(bridge, PR_NEUROMOD_DOPAMINE, da_boost, 1000);
    }
    if (ne_boost > PR_HYPO_EPSILON) {
        pr_hypo_bridge_apply_pulse(bridge, PR_NEUROMOD_NOREPINEPHRINE, ne_boost, 1500);
    }
    if (oxy_boost > PR_HYPO_EPSILON) {
        pr_hypo_bridge_apply_pulse(bridge, PR_NEUROMOD_OXYTOCIN, oxy_boost, 2000);
    }

    /* Update statistics */
    PR_HYPO_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.rewards_processed++;
    if (magnitude > 0) {
        bridge->stats.positive_rewards++;
    } else if (magnitude < 0) {
        bridge->stats.negative_rewards++;
    }
    bridge->stats.total_reward_magnitude += fabsf(magnitude);
    uint64_t n = bridge->stats.rewards_processed;
    bridge->stats.avg_prediction_error =
        (bridge->stats.avg_prediction_error * (float)(n - 1) + fabsf(prediction_error)) / (float)n;
    PR_HYPO_MUTEX_UNLOCK(bridge->stats_mutex);

    /* Invoke callback */
    if (bridge->reward_callback) {
        bridge->reward_callback(&signal, bridge->reward_callback_data);
    }

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT bool pr_hypo_bridge_get_reward_signal(
    const pr_hypo_bridge_t bridge,
    pr_reward_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_hypo_bridge_get_reward_signal: required parameter is NULL (bridge, signal)");
        return false;
    }

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->reward_mutex);
    bool has_reward = bridge->has_active_reward;
    if (has_reward) {
        *signal = bridge->current_reward;
    }
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->reward_mutex);

    return has_reward;
}

NIMCP_EXPORT float pr_hypo_bridge_get_reward_boost(
    const pr_hypo_bridge_t bridge
) {
    if (!bridge) return 0.0f;

    float boost = 0.0f;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->reward_mutex);
    if (bridge->has_active_reward) {
        boost = bridge->current_reward.magnitude *
                bridge->current_reward.decay_factor *
                bridge->config.reward_da_boost;
        if (boost < 0) boost = 0;
    }
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->reward_mutex);

    return boost;
}

//=============================================================================
// Memory Integration Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_modulate_memory(
    pr_hypo_bridge_t bridge,
    pr_memory_node_t* node
) {
    if (!bridge || !node) return PR_HYPO_ERROR_NULL_POINTER;

    /* Get current state */
    nimcp_quaternion_t original_state = node->state;
    nimcp_quaternion_t state = node->state;

    /* Apply neuromodulator effects */
    state = pr_hypo_bridge_apply_neuromodulator(bridge, state);

    /* Apply stress modulation */
    state = pr_hypo_bridge_stress_modulation(bridge, state);

    /* Update node state */
    node->state = state;

    /* Update statistics */
    PR_HYPO_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.memories_boosted++;
    float w_diff = state.w - original_state.w;
    uint64_t n = bridge->stats.memories_boosted;
    bridge->stats.avg_consolidation_boost =
        (bridge->stats.avg_consolidation_boost * (float)(n - 1) + w_diff) / (float)n;
    PR_HYPO_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT float pr_hypo_bridge_get_promotion_boost(
    const pr_hypo_bridge_t bridge
) {
    if (!bridge) return 0.0f;
    if (!bridge->config.enable_z_ladder_boost) return 0.0f;

    float da_level = pr_hypo_bridge_get_neuromod(bridge, PR_NEUROMOD_DOPAMINE);
    float baseline = bridge->config.baseline_levels[PR_NEUROMOD_DOPAMINE];

    float deviation = da_level - baseline;
    if (deviation <= 0) return 0.0f;

    return deviation * bridge->config.z_ladder_boost_factor;
}

NIMCP_EXPORT bool pr_hypo_bridge_is_flashbulb_state(
    const pr_hypo_bridge_t bridge
) {
    if (!bridge) {
        return false;
    }

    float ne_level = pr_hypo_bridge_get_neuromod(bridge, PR_NEUROMOD_NOREPINEPHRINE);

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->stress_mutex);
    pr_stress_state_t state = bridge->stress_state.state;
    float acute_boost = bridge->stress_state.acute_stress_boost;
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->stress_mutex);

    /* Flashbulb state: high NE + acute stress OR very high acute boost */
    return (ne_level > 0.8f && state == PR_STRESS_HIGH) ||
           state == PR_STRESS_ACUTE ||
           acute_boost > 0.7f;
}

//=============================================================================
// Callback Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_neuromod_callback(
    pr_hypo_bridge_t bridge,
    pr_neuromod_callback_t callback,
    void* user_data
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    bridge->neuromod_callback = callback;
    bridge->neuromod_callback_data = user_data;

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_reward_callback(
    pr_hypo_bridge_t bridge,
    pr_reward_callback_t callback,
    void* user_data
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    bridge->reward_callback = callback;
    bridge->reward_callback_data = user_data;

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_stress_callback(
    pr_hypo_bridge_t bridge,
    pr_stress_callback_t callback,
    void* user_data
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    bridge->stress_callback = callback;
    bridge->stress_callback_data = user_data;

    return PR_HYPO_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_stats(
    const pr_hypo_bridge_t bridge,
    pr_hypo_stats_t* stats
) {
    if (!bridge || !stats) return PR_HYPO_ERROR_NULL_POINTER;

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->stats_mutex);
    *stats = bridge->stats;
    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->stats_mutex);

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reset_stats(
    pr_hypo_bridge_t bridge
) {
    if (!bridge) return PR_HYPO_ERROR_NULL_POINTER;

    PR_HYPO_MUTEX_LOCK(bridge->stats_mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_update_ms = get_time_ms();
    PR_HYPO_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_HYPO_SUCCESS;
}

NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_history(
    const pr_hypo_bridge_t bridge,
    pr_hypo_history_entry_t* entries,
    size_t max_entries,
    size_t* count
) {
    if (!bridge || !entries || !count) return PR_HYPO_ERROR_NULL_POINTER;

    *count = 0;

    if (!bridge->config.track_history || !bridge->history) {
        return PR_HYPO_SUCCESS;
    }

    PR_HYPO_MUTEX_LOCK(((pr_hypo_bridge_t)bridge)->history_mutex);

    size_t available = bridge->history_size;
    size_t to_copy = available < max_entries ? available : max_entries;

    /* Copy from circular buffer in chronological order */
    if (to_copy > 0) {
        size_t start_idx = 0;
        if (bridge->history_size < bridge->history_capacity) {
            start_idx = 0;
        } else {
            start_idx = bridge->history_write_idx;
        }

        for (size_t i = 0; i < to_copy; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && to_copy > 256) {
                pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                                 (float)(i + 1) / (float)to_copy);
            }

            size_t src_idx = (start_idx + (available - to_copy) + i) % bridge->history_capacity;
            entries[i] = bridge->history[src_idx];
        }
        *count = to_copy;
    }

    PR_HYPO_MUTEX_UNLOCK(((pr_hypo_bridge_t)bridge)->history_mutex);

    return PR_HYPO_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_hypo_error_string(pr_hypo_error_t error) {
    switch (error) {
        case PR_HYPO_SUCCESS: return "Success";
        case PR_HYPO_ERROR_NULL_POINTER: return "Null pointer";
        case PR_HYPO_ERROR_INVALID_NEUROMOD: return "Invalid neuromodulator type";
        case PR_HYPO_ERROR_NO_MEMORY: return "Memory allocation failed";
        case PR_HYPO_ERROR_NOT_INITIALIZED: return "Bridge not initialized";
        case PR_HYPO_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_HYPO_ERROR_OUT_OF_RANGE: return "Value out of range";
        case PR_HYPO_ERROR_BUFFER_FULL: return "Buffer full";
        default: return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_neuromod_type_name(pr_neuromod_type_t type) {
    switch (type) {
        case PR_NEUROMOD_DOPAMINE: return "Dopamine";
        case PR_NEUROMOD_SEROTONIN: return "Serotonin";
        case PR_NEUROMOD_NOREPINEPHRINE: return "Norepinephrine";
        case PR_NEUROMOD_ACETYLCHOLINE: return "Acetylcholine";
        case PR_NEUROMOD_CORTISOL: return "Cortisol";
        case PR_NEUROMOD_OXYTOCIN: return "Oxytocin";
        case PR_NEUROMOD_ENDORPHIN: return "Endorphin";
        case PR_NEUROMOD_GABA: return "GABA";
        default: return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_reward_type_name(pr_reward_type_t type) {
    switch (type) {
        case PR_REWARD_POSITIVE: return "Positive";
        case PR_REWARD_NEGATIVE: return "Negative";
        case PR_REWARD_NEUTRAL: return "Neutral";
        case PR_REWARD_PREDICTION_ERROR: return "Prediction Error";
        case PR_REWARD_SOCIAL: return "Social";
        default: return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_stress_state_name(pr_stress_state_t state) {
    switch (state) {
        case PR_STRESS_LOW: return "Low";
        case PR_STRESS_OPTIMAL: return "Optimal";
        case PR_STRESS_HIGH: return "High";
        case PR_STRESS_ACUTE: return "Acute";
        case PR_STRESS_CHRONIC: return "Chronic";
        default: return "Unknown";
    }
}

NIMCP_EXPORT void pr_hypo_bridge_print_summary(const pr_hypo_bridge_t bridge) {
    if (!bridge) {
        printf("pr_hypo_bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Bridge Summary ===\n");

    printf("\nNeuromodulator Levels:\n");
    for (int i = 0; i < PR_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_NEUROMOD_COUNT);
        }

        float level = pr_hypo_bridge_get_neuromod(bridge, (pr_neuromod_type_t)i);
        float baseline = bridge->config.baseline_levels[i];
        printf("  %s: %.3f (baseline: %.3f)\n",
               pr_neuromod_type_name((pr_neuromod_type_t)i), level, baseline);
    }

    pr_stress_state_info_t stress;
    pr_hypo_bridge_get_stress_state(bridge, &stress);
    printf("\nStress State:\n");
    printf("  State: %s\n", pr_stress_state_name(stress.state));
    printf("  Cortisol: %.3f\n", stress.cortisol_level);
    printf("  Acute boost: %.3f\n", stress.acute_stress_boost);
    printf("  Performance factor: %.3f\n", stress.performance_factor);

    pr_reward_signal_t reward;
    bool has_reward = pr_hypo_bridge_get_reward_signal(bridge, &reward);
    printf("\nReward Signal:\n");
    if (has_reward) {
        printf("  Type: %s\n", pr_reward_type_name(reward.type));
        printf("  Magnitude: %.3f\n", reward.magnitude);
        printf("  Prediction error: %.3f\n", reward.prediction_error);
        printf("  Decay factor: %.3f\n", reward.decay_factor);
    } else {
        printf("  No active reward signal\n");
    }

    pr_hypo_stats_t stats;
    pr_hypo_bridge_get_stats(bridge, &stats);
    printf("\nStatistics:\n");
    printf("  Modulations applied: %lu\n", (unsigned long)stats.modulations_applied);
    printf("  Rewards processed: %lu\n", (unsigned long)stats.rewards_processed);
    printf("  Stress events: %lu\n", (unsigned long)stats.stress_events);
    printf("  Flashbulb triggers: %lu\n", (unsigned long)stats.flashbulb_triggers);
    printf("  Memories boosted: %lu\n", (unsigned long)stats.memories_boosted);

    printf("===================================\n");
}

NIMCP_EXPORT uint64_t pr_hypo_current_time_ms(void) {
    return get_time_ms();
}

NIMCP_EXPORT bool pr_hypo_bridge_validate(const pr_hypo_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_hypo_bridge_validate: bridge is NULL");
        return false;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_hypo_bridge_validate: bridge not initialized");
        return false;
    }

    /* Verify neuromodulator states are in valid range */
    for (int i = 0; i < PR_HYPO_NEUROMOD_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_HYPO_NEUROMOD_COUNT > 256) {
            pr_hypo_bridge_heartbeat("pr_hypo_brid_loop",
                             (float)(i + 1) / (float)PR_HYPO_NEUROMOD_COUNT);
        }

        if (bridge->neuromod_states[i].concentration < 0.0f ||
            bridge->neuromod_states[i].concentration > 1.0f) {
            return false;
        }
    }

    /* Verify stress state is valid */
    if (bridge->stress_state.cortisol_level < 0.0f ||
        bridge->stress_state.cortisol_level > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_hypo_bridge_validate: operation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_hypo_bridge_set_instance_health_agent(
    pr_hypo_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_hypo_bridge_training_begin(pr_hypo_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_hypo_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_hypo_bridge_heartbeat_instance(bridge->health_agent, "pr_hypo_bridge_training_begin", 0.0f);
    return 0;
}

int pr_hypo_bridge_training_end(pr_hypo_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_hypo_bridge_training_end: NULL argument");
        return -1;
    }
    pr_hypo_bridge_heartbeat_instance(bridge->health_agent, "pr_hypo_bridge_training_end", 1.0f);
    return 0;
}

int pr_hypo_bridge_training_step(pr_hypo_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_hypo_bridge_training_step: NULL argument");
        return -1;
    }
    pr_hypo_bridge_heartbeat_instance(bridge->health_agent, "pr_hypo_bridge_training_step", progress);
    return 0;
}
