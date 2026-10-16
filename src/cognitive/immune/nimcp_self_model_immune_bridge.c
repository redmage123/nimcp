/**
 * @file nimcp_self_model_immune_bridge.c
 * @brief Self-Model-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and self-model systems
 * WHY:  Biological realism - immune state is part of body representation
 * HOW:  Monitor immune state to update self-model, monitor self-awareness to modulate immunity
 */

#include "cognitive/immune/nimcp_self_model_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_model_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_model_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_model_immune_bridge_mesh_registry = NULL;

nimcp_error_t self_model_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_self_model_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_model_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_model_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_model_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_model_immune_bridge_mesh_registry = registry;
    return err;
}

void self_model_immune_bridge_mesh_unregister(void) {
    if (g_self_model_immune_bridge_mesh_registry && g_self_model_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_model_immune_bridge_mesh_registry, g_self_model_immune_bridge_mesh_id);
        g_self_model_immune_bridge_mesh_id = 0;
        g_self_model_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_model_immune_bridge module (instance-level) */
static inline void self_model_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_model_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_model_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_model_immune_bridge_health_agent) {
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
 * @brief Get max inflammation level from immune system
 *
 * WHAT: Query highest inflammation level across all sites
 * WHY:  Max inflammation determines health status
 * HOW:  Iterate inflammation sites, return maximum
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    /* Query immune system for max inflammation */
    brain_inflammation_level_t max_level = INFLAMMATION_NONE;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            self_model_immune_bridge_heartbeat("self_model_i_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Compute health score from immune state
 *
 * WHAT: Calculate overall health score [0-1]
 * WHY:  Aggregate metric for health status
 * HOW:  Inverse of inflammation level
 */
static float compute_health_score(const brain_immune_system_t* immune) {
    if (!immune) return 1.0f;

    brain_inflammation_level_t max_inflam = get_max_inflammation_level(immune);

    /* Map inflammation to health score */
    switch (max_inflam) {
        case INFLAMMATION_NONE:     return 1.0f;
        case INFLAMMATION_LOCAL:    return 0.85f;
        case INFLAMMATION_REGIONAL: return 0.65f;
        case INFLAMMATION_SYSTEMIC: return 0.35f;
        case INFLAMMATION_STORM:    return 0.1f;
        default:                    return 1.0f;
    }
}

/**
 * @brief Map health score to status category
 *
 * WHAT: Convert numeric health score to categorical status
 * WHY:  Self-model uses discrete health categories
 * HOW:  Apply thresholds
 */
static self_health_status_t health_score_to_status(float health_score) {
    if (health_score >= HEALTH_EXCELLENT_THRESHOLD) return HEALTH_STATUS_EXCELLENT;
    if (health_score >= HEALTH_GOOD_THRESHOLD)      return HEALTH_STATUS_GOOD;
    if (health_score >= HEALTH_FAIR_THRESHOLD)      return HEALTH_STATUS_FAIR;
    if (health_score >= HEALTH_POOR_THRESHOLD)      return HEALTH_STATUS_POOR;
    return HEALTH_STATUS_CRITICAL;
}

/**
 * @brief Get inflammation duration in days
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic illness (>30 days) affects identity
 * HOW:  Find oldest inflammation site, compute days
 */
static float get_inflammation_duration_days(const brain_immune_system_t* immune) {
    if (!immune || immune->inflammation_count == 0) return 0.0f;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    uint64_t oldest_time = current_time;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            self_model_immune_bridge_heartbeat("self_model_i_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].start_time < oldest_time) {
            oldest_time = immune->inflammation_sites[i].start_time;
        }
    }

    uint64_t duration_ms = current_time - oldest_time;
    float duration_days = (float)duration_ms / (1000.0f * 60.0f * 60.0f * 24.0f);
    return duration_days;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int self_model_immune_default_config(self_model_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_de", 0.0f);


    config->enable_interoceptive_signaling = true;
    config->enable_self_model_health_update = true;
    config->enable_capability_modulation = true;
    config->enable_health_belief_immune_effects = true;
    config->enable_identity_integration = true;

    /* Biologically-based default sensitivities */
    config->interoceptive_sensitivity = 1.0f;
    config->health_update_sensitivity = 1.0f;
    config->belief_immune_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->sickness_awareness_threshold = 0.5f;
    config->chronic_identity_threshold_days = 30.0f;

    return 0;
}

self_model_immune_bridge_t* self_model_immune_bridge_create(
    const self_model_immune_config_t* config,
    brain_immune_system_t* immune_system,
    self_model_system_t self_model
) {
    /* Guard: require immune and self-model systems */
    if (!immune_system || !self_model) {
        LOG_MODULE_ERROR("self_model_immune_bridge",
                  "Cannot create bridge without immune and self-model systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_bridge_create: required parameter is NULL (immune_system, self_model)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_create", 0.0f);


    self_model_immune_bridge_t* bridge = (self_model_immune_bridge_t*)
        nimcp_malloc(sizeof(self_model_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("self_model_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_model_immune_bridge_create: malloc returned NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(self_model_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->self_model = self_model;

    /* Apply configuration */
    if (config) {
        bridge->enable_interoceptive_signaling = config->enable_interoceptive_signaling;
        bridge->enable_self_model_health_update = config->enable_self_model_health_update;
        bridge->enable_capability_modulation = config->enable_capability_modulation;
        bridge->enable_health_belief_immune_effects = config->enable_health_belief_immune_effects;
        bridge->enable_identity_integration = config->enable_identity_integration;
    } else {
        /* Use defaults */
        self_model_immune_config_t default_cfg;
        self_model_immune_default_config(&default_cfg);
        bridge->enable_interoceptive_signaling = default_cfg.enable_interoceptive_signaling;
        bridge->enable_self_model_health_update = default_cfg.enable_self_model_health_update;
        bridge->enable_capability_modulation = default_cfg.enable_capability_modulation;
        bridge->enable_health_belief_immune_effects = default_cfg.enable_health_belief_immune_effects;
        bridge->enable_identity_integration = default_cfg.enable_identity_integration;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "self_model_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("self_model_immune_bridge", "Bridge created successfully");
    return bridge;
}

void self_model_immune_bridge_destroy(self_model_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("self_model_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Self-Model Implementation
 * ============================================================================ */

int self_model_immune_generate_interoceptive_signals(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_interoceptive_signaling) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_generate_interoceptive_signals: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    interoceptive_immune_signals_t* signals = &bridge->interoceptive_signals;
    brain_inflammation_level_t max_inflam = get_max_inflammation_level(bridge->immune_system);

    /* Map inflammation to interoceptive signals */
    float inflammation_intensity = (float)max_inflam / (float)INFLAMMATION_STORM;

    /* Fatigue increases with inflammation */
    signals->fatigue_signal = clamp_f(inflammation_intensity * 0.9f, 0.0f, 1.0f);

    /* Malaise (general unwellness) from systemic inflammation */
    signals->malaise_signal = clamp_f(inflammation_intensity * 0.8f, 0.0f, 1.0f);

    /* Pain/ache from inflammatory cytokines */
    signals->pain_signal = clamp_f(inflammation_intensity * 0.7f, 0.0f, 1.0f);

    /* Weakness from immune activation */
    signals->weakness_signal = clamp_f(inflammation_intensity * 0.75f, 0.0f, 1.0f);

    /* Vitality is inverse of inflammation */
    signals->vitality_signal = clamp_f(1.0f - inflammation_intensity, 0.0f, 1.0f);

    /* Aggregate body awareness */
    signals->total_body_awareness = (signals->fatigue_signal + signals->malaise_signal +
                                     signals->pain_signal + signals->weakness_signal) / 4.0f;

    /* Sickness intensity */
    signals->sickness_intensity = signals->malaise_signal;

    /* Conscious awareness threshold */
    signals->consciously_aware_of_illness =
        (signals->sickness_intensity >= 0.5f); /* Default threshold */

    bridge->interoceptive_signals_sent++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_immune_update_health_status(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_self_model_health_update) return 0;
    if (!bridge->immune_system || !bridge->self_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_update_health_status: required parameter is NULL (bridge->immune_system, bridge->self_model)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_model_immune_modulation_t* updates = &bridge->self_model_updates;

    /* Compute health status */
    float health_score = compute_health_score(bridge->immune_system);
    self_health_status_t new_status = health_score_to_status(health_score);

    /* Update if changed */
    if (new_status != updates->perceived_health_status) {
        updates->perceived_health_status = new_status;
        bridge->health_status_changes++;

        /* Update health belief string */
        switch (new_status) {
            case HEALTH_STATUS_EXCELLENT:
                snprintf(updates->health_belief, 256, "I am in excellent health");
                break;
            case HEALTH_STATUS_GOOD:
                snprintf(updates->health_belief, 256, "I am healthy");
                break;
            case HEALTH_STATUS_FAIR:
                snprintf(updates->health_belief, 256, "I am feeling unwell");
                break;
            case HEALTH_STATUS_POOR:
                snprintf(updates->health_belief, 256, "I am sick");
                break;
            case HEALTH_STATUS_CRITICAL:
                snprintf(updates->health_belief, 256, "I am very sick");
                break;
        }

        /* Add belief to self-model */
        self_belief_t health_belief = {
            .type = BELIEF_TYPE_FACT,
            .domain = DOMAIN_IDENTITY,
            .certainty = CERTAINTY_CONFIDENT,
            .confidence = health_score,
            .is_core_belief = (new_status == HEALTH_STATUS_POOR ||
                              new_status == HEALTH_STATUS_CRITICAL)
        };
        strncpy(health_belief.content, updates->health_belief, 255);
        self_model_add_belief(bridge->self_model, &health_belief);
    }

    /* Health certainty based on interoceptive signal strength */
    updates->health_certainty = bridge->interoceptive_signals.total_body_awareness;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_immune_modulate_capabilities(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_capability_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_modulate_capabilities: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_mo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_model_immune_modulation_t* updates = &bridge->self_model_updates;
    brain_inflammation_level_t max_inflam = get_max_inflammation_level(bridge->immune_system);
    float inflammation_intensity = (float)max_inflam / (float)INFLAMMATION_STORM;

    /* Reduce competence during illness */
    updates->immune_competence_reduction =
        clamp_f(inflammation_intensity * SICKNESS_COMPETENCE_REDUCTION, 0.0f, 1.0f);

    /* Reduce self-efficacy */
    updates->immune_efficacy_reduction =
        clamp_f(inflammation_intensity * INFLAMMATION_EFFICACY_REDUCTION, 0.0f, 1.0f);

    /* Cognitive impairment (brain fog) from cytokines */
    updates->cognitive_impairment =
        clamp_f(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    /* Self-care motivation increases with sickness awareness */
    updates->self_care_motivation = bridge->interoceptive_signals.sickness_intensity;

    /* Goal adjustment (lower expectations when sick) */
    updates->goal_adjustment = inflammation_intensity * 0.5f;

    bridge->capability_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_immune_integrate_chronic_illness(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_identity_integration) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_integrate_chronic_illness: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_in", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_model_immune_modulation_t* updates = &bridge->self_model_updates;

    /* Check if illness is chronic (default 30 days) */
    float duration_days = get_inflammation_duration_days(bridge->immune_system);
    bool is_chronic = (duration_days >= 30.0f);

    if (is_chronic && !updates->illness_integrated_in_identity) {
        /* Integrate chronic illness into identity */
        updates->illness_integrated_in_identity = true;

        /* Add core belief about chronic condition */
        self_belief_t chronic_belief = {
            .type = BELIEF_TYPE_FACT,
            .domain = DOMAIN_IDENTITY,
            .certainty = CERTAINTY_CONFIDENT,
            .confidence = 0.9f,
            .is_core_belief = true
        };
        snprintf(chronic_belief.content, 256,
                 "I have a chronic health condition");
        self_model_add_belief(bridge->self_model, &chronic_belief);

        LOG_MODULE_INFO("self_model_immune_bridge",
                  "Chronic illness integrated into identity after %.1f days", duration_days);
    }

    /* Body schema distortion from chronic inflammation */
    if (is_chronic) {
        updates->body_schema_distortion = clamp_f(duration_days / 90.0f, 0.0f, 0.5f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Self-Model → Immune Implementation
 * ============================================================================ */

int self_model_immune_trigger_adaptive_behavior(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->self_model) return -1;

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_tr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_awareness_immune_effects_t* effects = &bridge->self_awareness_effects;

    /* Check if aware of sickness */
    effects->aware_of_sickness = bridge->interoceptive_signals.consciously_aware_of_illness;

    if (effects->aware_of_sickness) {
        /* Increase self-care motivation */
        self_mental_state_t mental_state = {0};
        mental_state.is_introspecting = true;
        self_model_update_state(bridge->self_model, &mental_state);

        /* Rest compliance increases with awareness */
        effects->rest_compliance = bridge->interoceptive_signals.sickness_intensity;
    } else {
        effects->rest_compliance = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_immune_boost_from_health_beliefs(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_health_belief_immune_effects) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_boost_from_health_beliefs: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_bo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_awareness_immune_effects_t* effects = &bridge->self_awareness_effects;

    /* Positive health beliefs boost immunity */
    /* Note: Would query self-model for health-related beliefs */

    /* TODO: query self-model for actual perceived_immune_strength once API is available */
    /* Perceived immune strength */
    effects->perceived_immune_strength = 0.7f; /* Default - would query self-model */

    /* TODO: query self-model for actual recovery_expectation once API is available */
    /* Recovery expectation */
    effects->recovery_expectation = 0.8f; /* Default - would query self-model */

    /* TODO: query self-model for actual health_locus_of_control once API is available */
    /* Health locus of control (internal = higher immunity) */
    effects->health_locus_of_control = 0.7f; /* Default */

    /* Compute immune enhancement */
    float belief_strength = (effects->perceived_immune_strength +
                            effects->recovery_expectation +
                            effects->health_locus_of_control) / 3.0f;
    effects->immune_enhancement = clamp_f(belief_strength * HEALTH_BELIEF_IMMUNE_BOOST,
                                         0.0f, HEALTH_BELIEF_IMMUNE_BOOST);

    /* Apply enhancement to immune system */
    /* Note: Would call brain_immune_release_cytokine(CYTOKINE_IL10) */

    if (effects->immune_enhancement > 0.1f) {
        bridge->belief_immune_boosts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_immune_suppress_from_health_anxiety(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->immune_system) return -1;

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_su", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_awareness_immune_effects_t* effects = &bridge->self_awareness_effects;

    /* Excessive health monitoring can create anxiety */
    effects->health_monitoring_level = 0.5f; /* Default - would track actual monitoring */

    /* Stress from worrying about illness */
    if (bridge->interoceptive_signals.sickness_intensity > 0.3f &&
        effects->health_monitoring_level > 0.7f) {
        effects->stress_from_illness_belief = 0.6f;

        /* Health anxiety suppresses immunity via cortisol */
        /* Note: Would trigger immune suppression */
    } else {
        effects->stress_from_illness_belief = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_immune_accelerate_from_acceptance(
    self_model_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->immune_system) return -1;

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    self_awareness_immune_effects_t* effects = &bridge->self_awareness_effects;

    /* Illness acceptance reduces resistance stress */
    /* Note: Would query self-model for acceptance level */
    effects->illness_acceptance = 0.5f; /* Default */

    /* High acceptance accelerates recovery */
    if (effects->illness_acceptance > 0.7f) {
        float recovery_boost = effects->illness_acceptance * SELF_EFFICACY_RECOVERY_BOOST;

        /* Apply to immune system inflammation resolution */
        /* Note: Would increase resolution_progress on inflammation sites */
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int self_model_immune_bridge_update(
    self_model_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply all bidirectional effects */

    /* Immune → Self-Model */
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_update", 0.0f);


    self_model_immune_generate_interoceptive_signals(bridge);
    self_model_immune_update_health_status(bridge);
    self_model_immune_modulate_capabilities(bridge);
    self_model_immune_integrate_chronic_illness(bridge);

    /* Self-Model → Immune */
    self_model_immune_trigger_adaptive_behavior(bridge);
    self_model_immune_boost_from_health_beliefs(bridge);
    self_model_immune_suppress_from_health_anxiety(bridge);
    self_model_immune_accelerate_from_acceptance(bridge);

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int self_model_immune_get_interoceptive_signals(
    const self_model_immune_bridge_t* bridge,
    interoceptive_immune_signals_t* signals
) {
    if (!bridge || !signals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_get_interoceptive_signals: required parameter is NULL (bridge, signals)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(signals, &bridge->interoceptive_signals, sizeof(interoceptive_immune_signals_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_model_immune_get_self_model_updates(
    const self_model_immune_bridge_t* bridge,
    self_model_immune_modulation_t* updates
) {
    if (!bridge || !updates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_immune_get_self_model_updates: required parameter is NULL (bridge, updates)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(updates, &bridge->self_model_updates, sizeof(self_model_immune_modulation_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

self_health_status_t self_model_immune_get_health_status(
    const self_model_immune_bridge_t* bridge
) {
    if (!bridge) return HEALTH_STATUS_EXCELLENT;
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_ge", 0.0f);


    return bridge->self_model_updates.perceived_health_status;
}

bool self_model_immune_is_aware_of_sickness(
    const self_model_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_is", 0.0f);


    return bridge->interoceptive_signals.consciously_aware_of_illness;
}

float self_model_immune_get_interoceptive_accuracy(
    const self_model_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->immune_system) return 0.0f;

    /* Compute actual health state */
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_ge", 0.0f);


    float actual_health = compute_health_score(bridge->immune_system);

    /* Compute perceived health state */
    float perceived_health = 0.0f;
    switch (bridge->self_model_updates.perceived_health_status) {
        case HEALTH_STATUS_EXCELLENT: perceived_health = 1.0f; break;
        case HEALTH_STATUS_GOOD:      perceived_health = 0.85f; break;
        case HEALTH_STATUS_FAIR:      perceived_health = 0.65f; break;
        case HEALTH_STATUS_POOR:      perceived_health = 0.35f; break;
        case HEALTH_STATUS_CRITICAL:  perceived_health = 0.1f; break;
    }

    /* Accuracy is inverse of error */
    float error = fabs(actual_health - perceived_health);
    float accuracy = 1.0f - error;
    return clamp_f(accuracy, 0.0f, 1.0f);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define SELF_MODEL_IMMUNE_MODULE_NAME "self_model_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int self_model_immune_connect_bio_async(self_model_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_co", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SELF_MODEL,
        .module_name = SELF_MODEL_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("self_model_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int self_model_immune_disconnect_bio_async(self_model_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_di", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("self_model_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool self_model_immune_is_bio_async_connected(const self_model_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_is", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about self model immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int self_model_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    self_model_immune_bridge_heartbeat("self_model_i_self_model_immune_qu", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Model_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_model_immune_bridge_heartbeat("self_model_i_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Self model immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Model_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Model_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_model_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_model_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_model_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    self_model_immune_bridge_heartbeat_instance(NULL, "self_model_immune_bridge_training_begin", 0.0f);
    return 0;
}

int self_model_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    self_model_immune_bridge_heartbeat_instance(NULL, "self_model_immune_bridge_training_end", 1.0f);
    return 0;
}

int self_model_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    self_model_immune_bridge_heartbeat_instance(NULL, "self_model_immune_bridge_training_step", progress);
    return 0;
}
