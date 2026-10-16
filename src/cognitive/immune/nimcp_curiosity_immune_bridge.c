/**
 * @file nimcp_curiosity_immune_bridge.c
 * @brief Curiosity-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and curiosity systems
 * WHY:  Biological realism - sickness behavior suppresses exploration, novelty triggers immune vigilance
 * HOW:  Monitor cytokine/inflammation to suppress curiosity drive, monitor novelty to trigger immune alertness
 */

#include "cognitive/immune/nimcp_curiosity_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "curiosity_immune_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(curiosity_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_curiosity_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_curiosity_immune_bridge_mesh_registry = NULL;

nimcp_error_t curiosity_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_curiosity_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "curiosity_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "curiosity_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_curiosity_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_curiosity_immune_bridge_mesh_registry = registry;
    return err;
}

void curiosity_immune_bridge_mesh_unregister(void) {
    if (g_curiosity_immune_bridge_mesh_registry && g_curiosity_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_curiosity_immune_bridge_mesh_registry, g_curiosity_immune_bridge_mesh_id);
        g_curiosity_immune_bridge_mesh_id = 0;
        g_curiosity_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from curiosity_immune_bridge module (instance-level) */
static inline void curiosity_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_curiosity_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_curiosity_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_curiosity_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Clamp value to range
 *
 * WHAT: Constrain value to [min, max]
 * WHY:  Prevent overflow/underflow
 * HOW:  Return min if below, max if above, value otherwise
 */
static inline float clamp_f(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Compute cytokine concentration for specific type
 *
 * WHAT: Calculate aggregate cytokine level from immune system
 * WHY:  Need overall cytokine concentration for sickness behavior
 * HOW:  Sum all matching cytokines, normalize by count
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    /* Access immune cytokine array */
    float total = 0.0f;
    uint32_t count = 0;

    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            curiosity_immune_bridge_heartbeat("curiosity_im_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type) {
            total += immune->cytokines[i].concentration;
            count++;
        }
    }

    return (count > 0) ? (total / count) : 0.0f;
}

/**
 * @brief Get max inflammation level in system
 *
 * WHAT: Find highest inflammation severity
 * WHY:  Peak inflammation drives sickness behavior
 * HOW:  Iterate inflammation sites, return max level
 */
static brain_inflammation_level_t get_max_inflammation(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            curiosity_immune_bridge_heartbeat("curiosity_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Check if inflammation is chronic
 *
 * WHAT: Determine if inflammation has persisted beyond threshold
 * WHY:  Chronic inflammation has different effects than acute
 * HOW:  Find oldest active inflammation, compare to threshold
 */
static bool is_chronic_inflammation(
    const brain_immune_system_t* immune,
    uint64_t chronic_threshold_ms
) {
    if (!immune) {
        return false;
    }

    uint64_t now = nimcp_time_get_ms();

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            curiosity_immune_bridge_heartbeat("curiosity_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        uint64_t duration = now - immune->inflammation_sites[i].start_time;
        if (duration >= chronic_threshold_ms) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Map inflammation level to suppression factor
 *
 * WHAT: Convert inflammation severity to curiosity suppression
 * WHY:  Different inflammation levels have different behavioral effects
 * HOW:  Lookup table based on inflammation level
 */
static float inflammation_to_suppression(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont, 1.0f, 0.1f);  /* 1.0 at healthy, 0.1 at storm */
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int curiosity_immune_default_config(curiosity_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Enable all features by default */
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_def", 0.0f);


    config->enable_sickness_behavior = true;
    config->enable_inflammation_suppression = true;
    config->enable_chronic_anhedonia = true;
    config->enable_resolution_recovery = true;
    config->enable_novelty_vigilance = true;
    config->enable_exploration_immune_boost = true;
    config->enable_learning_stress_response = true;

    /* Biologically-based sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->novelty_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->sickness_threshold = SICKNESS_EXPLORATION_THRESHOLD;
    config->novelty_trigger_threshold = NOVELTY_IMMUNE_TRIGGER_THRESHOLD;
    config->chronic_inflammation_days = 7.0f;

    return 0;
}

curiosity_immune_bridge_t* curiosity_immune_bridge_create(
    const curiosity_immune_config_t* config,
    brain_immune_system_t* immune_system,
    curiosity_engine_t curiosity_engine
) {
    /* Guard: require both systems */
    if (!immune_system || !curiosity_engine) {
        LOG_MODULE_ERROR(LOG_MODULE, "Cannot create bridge without immune and curiosity systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_immune_bridge_create: required parameter is NULL (immune_system, curiosity_engine)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_create", 0.0f);


    curiosity_immune_bridge_t* bridge = (curiosity_immune_bridge_t*)
        nimcp_malloc(sizeof(curiosity_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR(LOG_MODULE, "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_immune_bridge_create: allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(curiosity_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->curiosity_engine = curiosity_engine;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        curiosity_immune_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->current_sickness_level = 0.0f;
    bridge->curiosity_suppression_factor = 1.0f;
    bridge->baseline_curiosity_backup = curiosity_get_drive(curiosity_engine);
    bridge->in_sickness_state = false;
    bridge->sickness_onset_time = 0;

    bridge->current_novelty_level = 0.0f;
    bridge->immune_vigilance_boost = 1.0f;
    bridge->immune_vigilance_active = false;
    bridge->last_novelty_trigger = 0;

    /* Initialize stats */
    bridge->sickness_episodes = 0;
    bridge->novelty_triggers = 0;
    bridge->chronic_suppression_duration = 0;
    bridge->max_suppression_observed = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "curiosity_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        LOG_MODULE_ERROR(LOG_MODULE, "Mutex creation failed");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Curiosity-immune bridge created");
    return bridge;
}

void curiosity_immune_bridge_destroy(curiosity_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_destroy", 0.0f);


    LOG_MODULE_INFO(LOG_MODULE, "Destroying curiosity-immune bridge");

    /* Restore original curiosity if suppressed */
    if (bridge->in_sickness_state && bridge->curiosity_engine) {
        curiosity_set_baseline(bridge->curiosity_engine,
                               bridge->baseline_curiosity_backup);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Immune → Curiosity (Sickness Behavior)
 * ============================================================================ */

int curiosity_immune_update_sickness_behavior(curiosity_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_sickness_behavior) return 0;
    if (!bridge->immune_system || !bridge->curiosity_engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_immune_update_sickness_behavior: required parameter is NULL (bridge->immune_system, bridge->curiosity_engine)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_upd", 0.0f);


    /* Compute sickness level before acquiring lock (accesses immune_system->cytokines) */
    float sickness = curiosity_immune_compute_sickness_level(bridge);

    float adjusted_curiosity = -1.0f; /* -1 means no set needed */
    bool do_suppress = false;
    bool do_restore = false;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->current_sickness_level = sickness;

    /* Check if entering sickness state */
    if (!bridge->in_sickness_state && sickness >= bridge->config.sickness_threshold) {
        bridge->in_sickness_state = true;
        bridge->sickness_onset_time = nimcp_time_get_ms();
        bridge->sickness_episodes++;
        LOG_MODULE_INFO(LOG_MODULE, "Entering sickness state (level: %.2f)", sickness);
    }

    /* Check if exiting sickness state */
    if (bridge->in_sickness_state && sickness < bridge->config.sickness_threshold * 0.5f) {
        bridge->in_sickness_state = false;
        LOG_MODULE_INFO(LOG_MODULE, "Exiting sickness state");
    }

    /* Compute adjusted_curiosity inside lock, apply externally outside lock */
    if (bridge->in_sickness_state) {
        float suppression_factor = 1.0f - (sickness * 0.9f);
        suppression_factor = clamp_f(suppression_factor, MAX_CURIOSITY_SUPPRESSION, 1.0f);
        bridge->curiosity_suppression_factor = suppression_factor;
        if (suppression_factor < bridge->max_suppression_observed) {
            bridge->max_suppression_observed = suppression_factor;
        }
        adjusted_curiosity = bridge->baseline_curiosity_backup * suppression_factor;
        do_suppress = true;
    } else if (bridge->curiosity_suppression_factor < 1.0f) {
        float recovery_rate = 0.1f;
        bridge->curiosity_suppression_factor +=
            (1.0f - bridge->curiosity_suppression_factor) * recovery_rate;
        bridge->curiosity_suppression_factor =
            clamp_f(bridge->curiosity_suppression_factor, 0.0f, 1.0f);
        adjusted_curiosity = bridge->baseline_curiosity_backup * bridge->curiosity_suppression_factor;
        do_restore = true;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Apply to curiosity engine outside lock to avoid deadlock */
    if (adjusted_curiosity >= 0.0f) {
        curiosity_set_baseline(bridge->curiosity_engine, adjusted_curiosity);
        if (do_suppress) {
            LOG_MODULE_DEBUG(LOG_MODULE, "Applied curiosity suppression: adjusted=%.2f", adjusted_curiosity);
        } else if (do_restore) {
            LOG_MODULE_DEBUG(LOG_MODULE, "Restoring curiosity: adjusted=%.2f", adjusted_curiosity);
        }
    }

    return 0;
}

int curiosity_immune_on_cytokine_release(
    curiosity_immune_bridge_t* bridge,
    const brain_cytokine_t* cytokine
) {
    /* Guard clauses */
    if (!bridge || !cytokine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_immune_on_cytokine_release: required parameter is NULL (bridge, cytokine)");
        return -1;
    }
    if (!bridge->config.enable_sickness_behavior) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_on_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Pro-inflammatory cytokines suppress curiosity */
    if (cytokine->pro_inflammatory) {
        float effect = cytokine->concentration * bridge->config.cytokine_sensitivity;
        bridge->current_sickness_level += effect * 0.2f;  /* Incremental effect */
        bridge->current_sickness_level = clamp_f(bridge->current_sickness_level, 0.0f, 1.0f);

        LOG_MODULE_DEBUG(LOG_MODULE,
                  "Cytokine release: type=%s, concentration=%.2f, sickness=%.2f",
                  brain_immune_cytokine_to_string(cytokine->type),
                  cytokine->concentration,
                  bridge->current_sickness_level);
    }

    /* Anti-inflammatory cytokines (IL-10) aid recovery */
    if (cytokine->type == BRAIN_CYTOKINE_IL10 && !cytokine->pro_inflammatory) {
        if (bridge->config.enable_resolution_recovery) {
            bridge->current_sickness_level -= cytokine->concentration * 0.3f;
            bridge->current_sickness_level = clamp_f(bridge->current_sickness_level, 0.0f, 1.0f);

            LOG_MODULE_DEBUG(LOG_MODULE, "IL-10 recovery: sickness reduced to %.2f",
                      bridge->current_sickness_level);
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_immune_on_inflammation(
    curiosity_immune_bridge_t* bridge,
    const brain_inflammation_site_t* site
) {
    /* Guard clauses */
    if (!bridge || !site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_immune_on_inflammation: required parameter is NULL (bridge, site)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_suppression) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_on_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute inflammation effect */
    float suppression = inflammation_to_suppression(site->level);
    suppression *= bridge->config.inflammation_sensitivity;

    /* Apply more severe suppression for systemic/storm */
    if (site->level >= INFLAMMATION_SYSTEMIC) {
        bridge->current_sickness_level = clamp_f(
            bridge->current_sickness_level + 0.3f, 0.0f, 1.0f
        );
        LOG_MODULE_WARN(LOG_MODULE, "Systemic inflammation detected, severe curiosity suppression");
    }

    /* Check for chronic inflammation */
    if (bridge->config.enable_chronic_anhedonia) {
        uint64_t chronic_threshold_ms =
            (uint64_t)(bridge->config.chronic_inflammation_days * 24 * 3600 * 1000);

        if (is_chronic_inflammation(bridge->immune_system, chronic_threshold_ms)) {
            /* Chronic inflammation → sustained anhedonia */
            bridge->current_sickness_level = clamp_f(
                bridge->current_sickness_level + 0.2f, 0.0f, 1.0f
            );
            bridge->chronic_suppression_duration++;

            LOG_MODULE_WARN(LOG_MODULE, "Chronic inflammation detected, sustained curiosity suppression");
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float curiosity_immune_compute_sickness_level(
    const curiosity_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->immune_system) return 0.0f;

    /* Get pro-inflammatory cytokine levels */
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_com", 0.0f);


    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);

    /* Get anti-inflammatory cytokine (IL-10) */
    float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Weighted combination (IL-1β has strongest sickness effect) */
    float cytokine_sickness = (il1 * 0.5f) + (il6 * 0.3f) + (tnf * 0.2f);

    /* IL-10 anti-inflammatory effect reduces sickness (recovery) */
    float il10_recovery = il10 * 0.3f;
    cytokine_sickness = cytokine_sickness - il10_recovery;

    /* Add inflammation contribution */
    brain_inflammation_level_t max_inflammation =
        get_max_inflammation(bridge->immune_system);
    float inflammation_contribution =
        (1.0f - inflammation_to_suppression(max_inflammation)) * 0.5f;

    /* Combine effects */
    float total_sickness = cytokine_sickness + inflammation_contribution;
    return clamp_f(total_sickness, 0.0f, 1.0f);
}

int curiosity_immune_apply_suppression(
    curiosity_immune_bridge_t* bridge,
    float sickness_level
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->curiosity_engine) return -1;

    /* Compute suppression factor (1.0 = no suppression, 0.1 = max suppression) */
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_app", 0.0f);


    float suppression_factor = 1.0f - (sickness_level * 0.9f);
    suppression_factor = clamp_f(suppression_factor, MAX_CURIOSITY_SUPPRESSION, 1.0f);

    bridge->curiosity_suppression_factor = suppression_factor;

    /* Track max suppression */
    if (suppression_factor < bridge->max_suppression_observed) {
        bridge->max_suppression_observed = suppression_factor;
    }

    /* Apply to curiosity engine */
    float adjusted_curiosity = bridge->baseline_curiosity_backup * suppression_factor;
    curiosity_set_baseline(bridge->curiosity_engine, adjusted_curiosity);

    LOG_MODULE_DEBUG(LOG_MODULE, "Applied curiosity suppression: factor=%.2f, adjusted=%.2f",
              suppression_factor, adjusted_curiosity);

    return 0;
}

int curiosity_immune_restore_curiosity(curiosity_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->curiosity_engine) return -1;

    /* Gradually restore (10% per update) */
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_res", 0.0f);


    float recovery_rate = 0.1f;
    bridge->curiosity_suppression_factor +=
        (1.0f - bridge->curiosity_suppression_factor) * recovery_rate;

    bridge->curiosity_suppression_factor =
        clamp_f(bridge->curiosity_suppression_factor, 0.0f, 1.0f);

    /* Apply to curiosity engine */
    float adjusted_curiosity =
        bridge->baseline_curiosity_backup * bridge->curiosity_suppression_factor;
    curiosity_set_baseline(bridge->curiosity_engine, adjusted_curiosity);

    LOG_MODULE_DEBUG(LOG_MODULE, "Restoring curiosity: factor=%.2f",
              bridge->curiosity_suppression_factor);

    return 0;
}

/* ============================================================================
 * Curiosity → Immune (Novelty Vigilance)
 * ============================================================================ */

int curiosity_immune_update_novelty_vigilance(curiosity_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_novelty_vigilance) return 0;
    if (!bridge->immune_system || !bridge->curiosity_engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_immune_update_novelty_vigilance: required parameter is NULL (bridge->immune_system, bridge->curiosity_engine)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_upd", 0.0f);


    /* Get current curiosity drive before acquiring lock (external call) */
    float curiosity_drive = curiosity_get_drive(bridge->curiosity_engine);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->current_novelty_level = curiosity_drive;

    /* Trigger vigilance if above threshold */
    if (curiosity_drive >= bridge->config.novelty_trigger_threshold) {
        if (!bridge->immune_vigilance_active) {
            curiosity_immune_trigger_vigilance(bridge, curiosity_drive);
        }
    } else {
        /* Decay vigilance */
        if (bridge->immune_vigilance_active) {
            bridge->immune_vigilance_boost -= 0.05f;
            if (bridge->immune_vigilance_boost <= 1.0f) {
                bridge->immune_vigilance_boost = 1.0f;
                bridge->immune_vigilance_active = false;
                LOG_MODULE_DEBUG(LOG_MODULE, "Immune vigilance deactivated");
            }
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_immune_trigger_vigilance(
    curiosity_immune_bridge_t* bridge,
    float novelty_level
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Compute vigilance boost */
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_tri", 0.0f);


    float boost = 1.0f + (novelty_level * 0.2f);  /* Up to 1.2x */
    boost *= bridge->config.novelty_sensitivity;
    boost = clamp_f(boost, 1.0f, CURIOSITY_IMMUNE_BOOST);

    bridge->immune_vigilance_boost = boost;
    bridge->immune_vigilance_active = true;
    bridge->last_novelty_trigger = nimcp_time_get_ms();
    bridge->novelty_triggers++;

    LOG_MODULE_INFO(LOG_MODULE, "Immune vigilance triggered: novelty=%.2f, boost=%.2fx",
              novelty_level, boost);

    return 0;
}

int curiosity_immune_on_knowledge_gap(
    curiosity_immune_bridge_t* bridge,
    const knowledge_gap_t* gap
) {
    /* Guard clauses */
    if (!bridge || !gap) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_immune_on_knowledge_gap: required parameter is NULL (bridge, gap)");
        return -1;
    }
    if (!bridge->config.enable_learning_stress_response) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_on_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Large knowledge gaps indicate high novelty */
    if (gap->gap_size >= 0.7f && gap->curiosity_intensity >= 0.6f) {
        /* Trigger immune vigilance for significant novel concepts */
        curiosity_immune_trigger_vigilance(bridge, gap->gap_size);

        LOG_MODULE_DEBUG(LOG_MODULE, "Knowledge gap triggered immune vigilance: topic='%s', gap_size=%.2f",
                  gap->topic, gap->gap_size);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float curiosity_immune_compute_novelty_boost(
    const curiosity_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_com", 0.0f);


    return bridge->immune_vigilance_boost;
}

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

int curiosity_immune_bridge_update(
    curiosity_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_update", 0.0f);


    (void)delta_ms;  /* Currently unused, for future decay logic */

    /* Update sickness behavior */
    if (bridge->config.enable_sickness_behavior) {
        curiosity_immune_update_sickness_behavior(bridge);
    }

    /* Update novelty vigilance */
    if (bridge->config.enable_novelty_vigilance) {
        curiosity_immune_update_novelty_vigilance(bridge);
    }

    return 0;
}

float curiosity_immune_get_sickness_level(
    const curiosity_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_get", 0.0f);


    return bridge->current_sickness_level;
}

float curiosity_immune_get_suppression_factor(
    const curiosity_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_get", 0.0f);


    return bridge->curiosity_suppression_factor;
}

float curiosity_immune_get_vigilance_boost(
    const curiosity_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_get", 0.0f);


    return bridge->immune_vigilance_boost;
}

bool curiosity_immune_is_chronic_inflammation(
    const curiosity_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->immune_system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_is_", 0.0f);


    uint64_t chronic_threshold_ms =
        (uint64_t)(bridge->config.chronic_inflammation_days * 24 * 3600 * 1000);

    return is_chronic_inflammation(bridge->immune_system, chronic_threshold_ms);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define CURIOSITY_IMMUNE_MODULE_NAME "curiosity_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int curiosity_immune_connect_bio_async(curiosity_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_con", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_CURIOSITY,
        .module_name = CURIOSITY_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("curiosity_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int curiosity_immune_disconnect_bio_async(curiosity_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_dis", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("curiosity_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool curiosity_immune_is_bio_async_connected(const curiosity_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about curiosity immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int curiosity_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    curiosity_immune_bridge_heartbeat("curiosity_im_curiosity_immune_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_immune_bridge_heartbeat("curiosity_im_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Curiosity immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_curiosity_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    curiosity_immune_bridge_heartbeat_instance(NULL, "curiosity_immune_bridge_training_begin", 0.0f);
    return 0;
}

int curiosity_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_immune_bridge_training_end: NULL argument");
        return -1;
    }
    curiosity_immune_bridge_heartbeat_instance(NULL, "curiosity_immune_bridge_training_end", 1.0f);
    return 0;
}

int curiosity_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_immune_bridge_heartbeat_instance(NULL, "curiosity_immune_bridge_training_step", progress);
    return 0;
}
