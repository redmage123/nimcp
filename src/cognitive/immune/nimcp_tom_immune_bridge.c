/**
 * @file nimcp_tom_immune_bridge.c
 * @brief Theory of Mind-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and theory of mind systems
 * WHY:  Biological realism - cytokines impair social cognition, social stress triggers inflammation
 * HOW:  Monitor cytokine levels to impair ToM, monitor social stress to trigger immune responses
 */

#include "cognitive/immune/nimcp_tom_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tom_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_tom_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_tom_immune_bridge_mesh_registry = NULL;

nimcp_error_t tom_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_tom_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "tom_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "tom_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_tom_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_tom_immune_bridge_mesh_registry = registry;
    return err;
}

void tom_immune_bridge_mesh_unregister(void) {
    if (g_tom_immune_bridge_mesh_registry && g_tom_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_tom_immune_bridge_mesh_registry, g_tom_immune_bridge_mesh_id);
        g_tom_immune_bridge_mesh_id = 0;
        g_tom_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from tom_immune_bridge module (instance-level) */
static inline void tom_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_tom_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_tom_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_tom_immune_bridge_health_agent) {
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
 * @brief Query cytokine concentration from immune system
 *
 * WHAT: Get current concentration of specific cytokine
 * WHY:  Need cytokine levels to compute ToM impairment
 * HOW:  Iterate through immune system cytokines, find matching type
 */
static float query_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune || !immune->cytokines) return 0.0f;

    float total_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            tom_immune_bridge_heartbeat("tom_immune_b_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type) {
            total_concentration += immune->cytokines[i].concentration;
        }
    }

    return clamp_f(total_concentration, 0.0f, 1.0f);
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different ToM effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || !immune->inflammation_sites) return 0.0f;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    uint64_t oldest_start = current_time;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            tom_immune_bridge_heartbeat("tom_immune_b_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    return (float)(current_time - oldest_start) / 1000.0f;  /* Convert ms to sec */
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines ToM impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || !immune->inflammation_sites) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            tom_immune_bridge_heartbeat("tom_immune_b_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int tom_immune_default_config(tom_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_default_c", 0.0f);


    config->enable_cytokine_tom_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_social_stress_immune_trigger = true;
    config->enable_social_connection_boost = true;
    config->enable_rejection_inflammation = true;
    config->enable_isolation_chronic_inflammation = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->social_stress_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->social_stress_threshold = SOCIAL_STRESS_IMMUNE_THRESHOLD;
    config->inflammation_tom_threshold = INFLAMMATION_TOM_THRESHOLD;

    return 0;
}

tom_immune_bridge_t* tom_immune_bridge_create(
    const tom_immune_config_t* config,
    theory_of_mind_t tom_system,
    brain_immune_system_t* immune_system
) {
    /* Guard: require both systems */
    if (!tom_system || !immune_system) {
        LOG_MODULE_ERROR("tom_immune_bridge",
                  "Cannot create bridge without ToM and immune systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_bridge_create: required parameter is NULL (tom_system, immune_system)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_create", 0.0f);


    tom_immune_bridge_t* bridge = (tom_immune_bridge_t*)
        nimcp_malloc(sizeof(tom_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("tom_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(tom_immune_bridge_t));

    /* Link systems */
    bridge->tom_system = tom_system;
    bridge->immune_system = immune_system;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_tom_modulation = config->enable_cytokine_tom_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_social_stress_immune_trigger = config->enable_social_stress_immune_trigger;
        bridge->enable_social_connection_boost = config->enable_social_connection_boost;
        bridge->enable_rejection_inflammation = config->enable_rejection_inflammation;
        bridge->enable_isolation_chronic_inflammation = config->enable_isolation_chronic_inflammation;
    } else {
        /* Use defaults */
        tom_immune_config_t default_cfg;
        tom_immune_default_config(&default_cfg);
        bridge->enable_cytokine_tom_modulation = default_cfg.enable_cytokine_tom_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_social_stress_immune_trigger = default_cfg.enable_social_stress_immune_trigger;
        bridge->enable_social_connection_boost = default_cfg.enable_social_connection_boost;
        bridge->enable_rejection_inflammation = default_cfg.enable_rejection_inflammation;
        bridge->enable_isolation_chronic_inflammation = default_cfg.enable_isolation_chronic_inflammation;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "tom_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("tom_immune_bridge", "Bridge created successfully");
    return bridge;
}

void tom_immune_bridge_destroy(tom_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("tom_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → ToM Implementation
 * ============================================================================ */

int tom_immune_apply_cytokine_effects(tom_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_tom_modulation) return 0;
    if (!bridge->immune_system || !bridge->tom_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->tom_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_apply_cyt", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_tom_effects_t* effects = &bridge->cytokine_effects;

    /* Query cytokine concentrations */
    float il6_level = query_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf_level = query_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il1_level = query_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float ifn_gamma_level = query_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10_level = query_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Pro-inflammatory cytokines → ToM impairment */
    effects->il6_tom_impairment = il6_level * CYTOKINE_IL6_TOM_IMPAIRMENT;
    effects->tnf_tom_impairment = tnf_level * CYTOKINE_TNF_TOM_IMPAIRMENT;
    effects->il1_tom_impairment = il1_level * CYTOKINE_IL1_TOM_IMPAIRMENT;
    effects->ifn_gamma_tom_impairment = ifn_gamma_level * CYTOKINE_IFN_GAMMA_TOM_IMPAIRMENT;

    /* Anti-inflammatory cytokines → ToM recovery */
    effects->il10_tom_recovery = il10_level * CYTOKINE_IL10_TOM_RECOVERY;

    /* Aggregate effects */
    float proinflam_total = effects->il6_tom_impairment +
                           effects->tnf_tom_impairment +
                           effects->il1_tom_impairment +
                           effects->ifn_gamma_tom_impairment;

    effects->total_perspective_impairment = clamp_f(
        proinflam_total - effects->il10_tom_recovery,
        0.0f, 1.0f
    );

    /* Specific ToM impairments */
    effects->empathy_reduction = clamp_f(proinflam_total * 0.7f, 0.0f, 1.0f);
    effects->social_motivation_loss = clamp_f(proinflam_total * 0.6f, 0.0f, 1.0f);
    effects->mentalizing_accuracy_loss = clamp_f(proinflam_total * 0.8f, 0.0f, 1.0f);

    bridge->cytokine_impairments++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_immune_apply_inflammation_effects(tom_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_apply_inf", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    inflammation_tom_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= (86400.0f * 7));  /* 7 days */

    /* Inflammation intensity */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;

    /* Chronic inflammation → persistent ToM deficits */
    if (state->is_chronic) {
        float duration_factor = clamp_f(
            state->inflammation_duration_sec / (86400.0f * 14),  /* 14 days */
            0.0f, 1.0f
        );
        state->perspective_score_reduction = clamp_f(duration_factor * 0.8f, 0.0f, 1.0f);
    } else {
        /* Higher multiplier for stronger inflammation impact on perspective-taking */
        state->perspective_score_reduction = clamp_f(inflammation_intensity * 0.7f, 0.0f, 1.0f);
    }

    /* Specific ToM impairments based on inflammation */
    state->false_belief_impairment = clamp_f(inflammation_intensity * 0.8f, 0.0f, 1.0f);
    state->empathy_capacity_loss = clamp_f(inflammation_intensity * 0.9f, 0.0f, 1.0f);
    /* Full social withdrawal at systemic inflammation */
    state->social_withdrawal = clamp_f(inflammation_intensity * 1.0f, 0.0f, 1.0f);

    /* Inference impairments */
    state->emotion_inference_impairment = clamp_f(inflammation_intensity * 0.65f, 0.0f, 1.0f);
    state->goal_inference_impairment = clamp_f(inflammation_intensity * 0.70f, 0.0f, 1.0f);
    state->intention_inference_impairment = clamp_f(inflammation_intensity * 0.60f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float tom_immune_compute_impairment(const tom_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Combine cytokine-induced and inflammation-induced impairment */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_compute_i", 0.0f);


    float cytokine_impairment = bridge->cytokine_effects.total_perspective_impairment;
    float inflammation_impairment = bridge->inflammation_state.perspective_score_reduction;

    /* Take maximum (not additive) */
    float total_impairment = fmaxf(cytokine_impairment, inflammation_impairment);
    return clamp_f(total_impairment, 0.0f, 1.0f);
}

int tom_immune_impair_perspective_taking(tom_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->tom_system) return -1;

    /* Compute impairment */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_impair_pe", 0.0f);


    float impairment = tom_immune_compute_impairment(bridge);

    /* This would reduce the perspective score in the ToM system */
    /* For now, we track it in the bridge state */
    /* Actual ToM system modification would go here */

    return 0;
}

int tom_immune_impair_empathy(tom_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->tom_system) return -1;

    /* Get empathy reduction from both cytokines and inflammation */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_impair_em", 0.0f);


    float cytokine_empathy_loss = bridge->cytokine_effects.empathy_reduction;
    float inflammation_empathy_loss = bridge->inflammation_state.empathy_capacity_loss;

    /* Take maximum */
    float total_empathy_loss = fmaxf(cytokine_empathy_loss, inflammation_empathy_loss);
    total_empathy_loss = clamp_f(total_empathy_loss, 0.0f, 1.0f);

    /* This would reduce empathy accuracy in ToM system */
    /* Actual ToM system modification would go here */

    return 0;
}

/* ============================================================================
 * ToM → Immune Implementation
 * ============================================================================ */

int tom_immune_trigger_from_rejection(
    tom_immune_bridge_t* bridge,
    float rejection_severity
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_rejection_inflammation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_trigger_from_rejection: bridge->immune_system is NULL");
        return -1;
    }
    if (rejection_severity < 0.0f || rejection_severity > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_immune_trigger_from_rejection: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_trigger_f", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update social stress state */
    bridge->social_stress_trigger.rejection_severity = rejection_severity;
    bridge->social_stress_trigger.cortisol_triggered = true;

    /* Social rejection triggers IL-6 release */
    bool do_release = (rejection_severity >= SOCIAL_STRESS_IMMUNE_THRESHOLD);
    float il6_concentration = 0.0f;
    if (do_release) {
        il6_concentration = clamp_f(rejection_severity * REJECTION_INFLAMMATION_MULTIPLIER, 0.0f, 1.0f);
        bridge->social_stress_trigger.inflammatory_response = true;
        bridge->rejection_inflammations++;
    }

    bridge->social_stress_triggers++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokine outside lock to avoid deadlock */
    if (do_release) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL6,
            0,  /* Source cell ID */
            il6_concentration,
            0,  /* Broadcast */
            &cytokine_id
        );
    }

    return 0;
}

int tom_immune_trigger_from_prediction_error(
    tom_immune_bridge_t* bridge,
    float prediction_error
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_social_stress_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_trigger_from_prediction_error: bridge->immune_system is NULL");
        return -1;
    }
    if (prediction_error < 0.0f || prediction_error > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_immune_trigger_from_prediction_error: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_trigger_f", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update social stress state */
    bridge->social_stress_trigger.prediction_error = prediction_error;

    /* Prediction errors trigger IL-1β release */
    bool do_il1_release = (prediction_error >= SOCIAL_STRESS_IMMUNE_THRESHOLD);
    float il1_concentration = prediction_error * 0.8f;
    if (do_il1_release) {
        bridge->social_stress_trigger.inflammatory_response = true;
        bridge->social_stress_triggers++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokine outside lock to avoid deadlock */
    if (do_il1_release) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL1,
            0,
            il1_concentration,
            0,
            &cytokine_id
        );
    }
    return 0;
}

int tom_immune_trigger_from_isolation(
    tom_immune_bridge_t* bridge,
    float isolation_duration_sec
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_isolation_chronic_inflammation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_trigger_from_isolation: bridge->immune_system is NULL");
        return -1;
    }
    if (isolation_duration_sec < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_immune_trigger_from_isolation: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_trigger_f", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update isolation state */
    bridge->social_stress_trigger.isolation_duration_sec = isolation_duration_sec;
    bridge->social_stress_trigger.is_chronic_isolation =
        (isolation_duration_sec >= ISOLATION_CHRONIC_THRESHOLD);

    /* Chronic isolation triggers sustained inflammation */
    bool do_chronic_release = bridge->social_stress_trigger.is_chronic_isolation;
    float duration_factor = 0.0f;
    if (do_chronic_release) {
        duration_factor = clamp_f(
            isolation_duration_sec / (ISOLATION_CHRONIC_THRESHOLD * 2.0f),
            0.0f, 1.0f
        );
        bridge->social_stress_trigger.chronic_inflammation_risk = duration_factor;
        bridge->social_stress_trigger.gene_expression_changes = duration_factor * 0.8f;
        bridge->isolation_inflammations++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokines outside lock to avoid deadlock */
    if (do_chronic_release) {
        uint32_t cytokine_id = 0;

        /* IL-6 (chronic inflammation marker) */
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL6,
            0,
            duration_factor * 0.7f,
            0,
            &cytokine_id
        );

        /* TNF-α (severe chronic inflammation) */
        if (duration_factor > 0.5f) {
            brain_immune_release_cytokine(
                bridge->immune_system,
                BRAIN_CYTOKINE_TNF,
                0,
                (duration_factor - 0.5f) * 0.6f,
                0,
                &cytokine_id
            );
        }
    }
    return 0;
}

int tom_immune_boost_from_social_connection(
    tom_immune_bridge_t* bridge,
    float connection_strength
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_social_connection_boost) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_boost_from_social_connection: bridge->immune_system is NULL");
        return -1;
    }
    if (connection_strength < 0.0f || connection_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_immune_boost_from_social_connection: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_boost_fro", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update social connection state */
    bridge->social_connection_boost.social_bond_strength = connection_strength;
    bridge->social_connection_boost.mentalizing_success_rate = connection_strength * 0.9f;
    bridge->social_connection_boost.empathy_engagement = connection_strength * 0.85f;

    /* Strong social connection releases IL-10 (anti-inflammatory) */
    bool do_il10_release = (connection_strength >= 0.6f);
    float il10_concentration = connection_strength * 0.5f;
    if (do_il10_release) {
        bridge->social_connection_boost.immune_enhancement = connection_strength * 0.4f;
        bridge->social_connection_boost.il10_release_boost = il10_concentration;
        bridge->social_connection_boost.inflammation_reduction = connection_strength * 0.3f;
        bridge->social_connection_boost.stress_resistance = connection_strength * 0.5f;

        bridge->social_connection_boosts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokine outside lock to avoid deadlock */
    if (do_il10_release) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            il10_concentration,
            0,
            &cytokine_id
        );
    }
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int tom_immune_bridge_update(
    tom_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply immune → ToM effects */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_update", 0.0f);


    tom_immune_apply_cytokine_effects(bridge);
    tom_immune_apply_inflammation_effects(bridge);
    tom_immune_impair_perspective_taking(bridge);
    tom_immune_impair_empathy(bridge);

    /* ToM → Immune effects would be triggered by events, not periodic updates */
    /* (e.g., rejection events, prediction errors, etc.) */

    /* Update base timestamp so callers see real wall-clock time */
    struct timespec upd_ts;
    clock_gettime(CLOCK_MONOTONIC, &upd_ts);
    bridge->base.last_update_time_ms =
        (uint64_t)upd_ts.tv_sec * 1000ULL + (uint64_t)upd_ts.tv_nsec / 1000000ULL;

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int tom_immune_get_cytokine_effects(
    const tom_immune_bridge_t* bridge,
    cytokine_tom_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_get_cytok", 0.0f);


    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_tom_effects_t));
    return 0;
}

int tom_immune_get_inflammation_state(
    const tom_immune_bridge_t* bridge,
    inflammation_tom_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_get_infla", 0.0f);


    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_tom_state_t));
    return 0;
}

bool tom_immune_is_social_withdrawal(const tom_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Social withdrawal if high social motivation loss or sickness behavior */
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_is_social", 0.0f);


    float motivation_loss = bridge->cytokine_effects.social_motivation_loss;
    float social_withdrawal = bridge->inflammation_state.social_withdrawal;

    return (motivation_loss > 0.6f) || (social_withdrawal > 0.7f);
}

float tom_immune_get_impairment_severity(const tom_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_get_impai", 0.0f);


    return tom_immune_compute_impairment(bridge);
}

float tom_immune_get_perspective_impairment(const tom_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_get_persp", 0.0f);


    return bridge->cytokine_effects.total_perspective_impairment;
}

float tom_immune_get_empathy_impairment(const tom_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_get_empat", 0.0f);


    float cytokine_empathy = bridge->cytokine_effects.empathy_reduction;
    float inflammation_empathy = bridge->inflammation_state.empathy_capacity_loss;

    return fmaxf(cytokine_empathy, inflammation_empathy);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define TOM_IMMUNE_MODULE_NAME "tom_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int tom_immune_connect_bio_async(tom_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_connect_b", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_TOM,
        .module_name = TOM_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("tom_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int tom_immune_disconnect_bio_async(tom_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_disconnec", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("tom_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool tom_immune_is_bio_async_connected(const tom_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_is_bio_as", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about theory of mind immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int tom_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    tom_immune_bridge_heartbeat("tom_immune_b_tom_immune_query_sel", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Tom_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                tom_immune_bridge_heartbeat("tom_immune_b_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Theory of mind immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Tom_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Tom_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void tom_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_tom_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int tom_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (tom_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    tom_immune_bridge_heartbeat_instance(NULL, "tom_immune_bridge_training_begin", 0.0f);
    return 0;
}

int tom_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (tom_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    tom_immune_bridge_heartbeat_instance(NULL, "tom_immune_bridge_training_end", 1.0f);
    return 0;
}

int tom_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass instance cast to (tom_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    tom_immune_bridge_heartbeat_instance(NULL, "tom_immune_bridge_training_step", progress);
    return 0;
}
