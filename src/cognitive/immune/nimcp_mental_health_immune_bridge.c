/**
 * @file nimcp_mental_health_immune_bridge.c
 * @brief Mental Health-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and mental health systems
 * WHY:  Biological realism - cytokines affect mental health, disorders affect immunity
 * HOW:  Monitor cytokine levels to modulate disorder risk, monitor disorders to trigger immune responses
 */

#include "cognitive/immune/nimcp_mental_health_immune_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mental_health_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mental_health_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mental_health_immune_bridge_mesh_registry = NULL;

nimcp_error_t mental_health_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mental_health_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mental_health_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mental_health_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mental_health_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mental_health_immune_bridge_mesh_registry = registry;
    return err;
}

void mental_health_immune_bridge_mesh_unregister(void) {
    if (g_mental_health_immune_bridge_mesh_registry && g_mental_health_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mental_health_immune_bridge_mesh_registry, g_mental_health_immune_bridge_mesh_id);
        g_mental_health_immune_bridge_mesh_id = 0;
        g_mental_health_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mental_health_immune_bridge module (instance-level) */
static inline void mental_health_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mental_health_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mental_health_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mental_health_immune_bridge_health_agent) {
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
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>14 days) has different mental health effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query immune system for inflammation sites */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return 0.0f;
    }

    /* If no inflammation, duration is zero */
    if (stats.inflammation_sites == 0) {
        return 0.0f;
    }

    /* Approximate duration - would need actual timestamp tracking */
    /* For now, assume sites persist for a while if count is high */
    return (float)(stats.inflammation_sites * 3600.0f); /* 1 hour per site */
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines mental health impact
 * HOW:  Query immune system for inflammation state
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return INFLAMMATION_NONE;
    }

    /* Map site count to inflammation level */
    if (stats.inflammation_sites == 0) return INFLAMMATION_NONE;
    if (stats.inflammation_sites < 2) return INFLAMMATION_LOCAL;
    if (stats.inflammation_sites < 4) return INFLAMMATION_REGIONAL;
    if (stats.inflammation_sites < 6) return INFLAMMATION_SYSTEMIC;
    return INFLAMMATION_STORM;
}

/**
 * @brief Get cytokine level estimate
 *
 * WHAT: Estimate cytokine concentration from immune state
 * WHY:  Cytokines not directly accessible, estimate from inflammation
 * HOW:  Map inflammation level to cytokine concentration
 */
static float estimate_cytokine_level(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    brain_inflammation_level_t level = get_max_inflammation_level(immune);
    float base_level = (float)level / (float)INFLAMMATION_STORM;

    /* Different cytokines scale differently */
    switch (type) {
        case BRAIN_CYTOKINE_IL1:
            return base_level * 0.8f; /* IL-1β peaks early */
        case BRAIN_CYTOKINE_IL6:
            return base_level * 1.0f; /* IL-6 tracks inflammation closely */
        case BRAIN_CYTOKINE_TNF:
            return base_level * 0.9f; /* TNF-α strong but variable */
        case BRAIN_CYTOKINE_IFN_GAMMA:
            return base_level * 0.6f; /* IFN-γ lower levels */
        case BRAIN_CYTOKINE_IL10:
            /* IL-10 is anti-inflammatory, inversely related */
            return (1.0f - base_level) * 0.5f;
        default:
            return 0.0f;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int mental_health_immune_default_config(mental_health_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    config->enable_cytokine_disorder_modulation = true;
    config->enable_inflammation_depression = true;
    config->enable_inflammation_anxiety = true;
    config->enable_disorder_immune_trigger = true;
    config->enable_recovery_immune_boost = true;
    config->enable_neurotransmitter_modulation = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->disorder_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->depression_trigger_threshold = DEPRESSION_IMMUNE_TRIGGER_THRESHOLD;
    config->anxiety_trigger_threshold = ANXIETY_IMMUNE_TRIGGER_THRESHOLD;
    config->inflammation_depression_threshold = INFLAMMATION_DEPRESSION_THRESHOLD;
    config->inflammation_anxiety_threshold = INFLAMMATION_ANXIETY_THRESHOLD;

    return 0;
}

mental_health_immune_bridge_t* mental_health_immune_bridge_create(
    const mental_health_immune_config_t* config,
    brain_immune_system_t* immune_system,
    mental_health_monitor_t* mental_health_monitor
) {
    /* Guard: require immune and mental health systems */
    if (!immune_system || !mental_health_monitor) {
        LOG_MODULE_ERROR("mental_health_immune_bridge",
                  "Cannot create bridge without immune and mental health systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_bridge_create: required parameter is NULL (immune_system, mental_health_monitor)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_create", 0.0f);


    mental_health_immune_bridge_t* bridge = (mental_health_immune_bridge_t*)
        nimcp_malloc(sizeof(mental_health_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("mental_health_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mental_health_immune_bridge_create: malloc returned NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(mental_health_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->mental_health_monitor = mental_health_monitor;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_disorder_modulation = config->enable_cytokine_disorder_modulation;
        bridge->enable_inflammation_depression = config->enable_inflammation_depression;
        bridge->enable_inflammation_anxiety = config->enable_inflammation_anxiety;
        bridge->enable_disorder_immune_trigger = config->enable_disorder_immune_trigger;
        bridge->enable_recovery_immune_boost = config->enable_recovery_immune_boost;
        bridge->enable_neurotransmitter_modulation = config->enable_neurotransmitter_modulation;
    } else {
        /* Use defaults */
        mental_health_immune_config_t default_cfg;
        mental_health_immune_default_config(&default_cfg);
        bridge->enable_cytokine_disorder_modulation = default_cfg.enable_cytokine_disorder_modulation;
        bridge->enable_inflammation_depression = default_cfg.enable_inflammation_depression;
        bridge->enable_inflammation_anxiety = default_cfg.enable_inflammation_anxiety;
        bridge->enable_disorder_immune_trigger = default_cfg.enable_disorder_immune_trigger;
        bridge->enable_recovery_immune_boost = default_cfg.enable_recovery_immune_boost;
        bridge->enable_neurotransmitter_modulation = default_cfg.enable_neurotransmitter_modulation;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "mental_health_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("mental_health_immune_bridge", "Bridge created successfully");
    return bridge;
}

void mental_health_immune_bridge_destroy(mental_health_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("mental_health_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Mental Health Implementation
 * ============================================================================ */

int mental_health_immune_apply_cytokine_effects(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_disorder_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_mental_health_effects_t* effects = &bridge->cytokine_effects;

    /* Estimate cytokine levels from immune state */
    float il1_level = estimate_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6_level = estimate_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf_level = estimate_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn_gamma_level = estimate_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10_level = estimate_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Pro-inflammatory cytokines → depression risk */
    effects->il1_depression_risk = il1_level * CYTOKINE_IL1_DEPRESSION_RISK;
    effects->il6_depression_risk = il6_level * CYTOKINE_IL6_DEPRESSION_RISK;
    effects->tnf_depression_risk = tnf_level * CYTOKINE_TNF_DEPRESSION_RISK;
    effects->ifn_gamma_depression_risk = ifn_gamma_level * CYTOKINE_IFN_GAMMA_DEPRESSION_RISK;

    /* Pro-inflammatory cytokines → anxiety risk */
    effects->il1_anxiety_risk = il1_level * CYTOKINE_IL1_ANXIETY_RISK;
    effects->il6_anxiety_risk = il6_level * CYTOKINE_IL6_ANXIETY_RISK;
    effects->tnf_anxiety_risk = tnf_level * CYTOKINE_TNF_ANXIETY_RISK;

    /* Anti-inflammatory cytokines → recovery */
    effects->il10_recovery_benefit = il10_level * CYTOKINE_IL10_RECOVERY_BENEFIT;

    /* Aggregate effects */
    effects->total_depression_risk_shift =
        effects->il1_depression_risk +
        effects->il6_depression_risk +
        effects->tnf_depression_risk +
        effects->ifn_gamma_depression_risk +
        effects->il10_recovery_benefit;

    effects->total_anxiety_risk_shift =
        effects->il1_anxiety_risk +
        effects->il6_anxiety_risk +
        effects->tnf_anxiety_risk +
        effects->il10_recovery_benefit;

    /* Neurotransmitter suppression */
    float proinflam_total = il1_level + il6_level + tnf_level;
    effects->neurotransmitter_suppression = clamp_f(proinflam_total * 0.3f, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_immune_apply_inflammation_effects(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->immune_system) return -1;

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    inflammation_mental_health_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_MH_INFLAMMATION_THRESHOLD);

    /* Map inflammation level to intensity */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;

    /* Depression risk multiplier (chronic inflammation is major risk factor) */
    if (bridge->enable_inflammation_depression) {
        if (state->is_chronic && inflammation_intensity >= INFLAMMATION_DEPRESSION_THRESHOLD) {
            /* Chronic inflammation → high depression risk */
            float duration_factor = clamp_f(
                state->inflammation_duration_sec / (CHRONIC_MH_INFLAMMATION_THRESHOLD * 2.0f),
                0.0f, 1.0f
            );
            state->depression_risk_multiplier = 1.0f + (inflammation_intensity * 2.0f * duration_factor);
        } else if (inflammation_intensity >= INFLAMMATION_DEPRESSION_THRESHOLD) {
            /* Acute inflammation → moderate depression risk */
            state->depression_risk_multiplier = 1.0f + (inflammation_intensity * 1.0f);
        } else {
            state->depression_risk_multiplier = 1.0f;
        }
    } else {
        state->depression_risk_multiplier = 1.0f;
    }

    /* Anxiety risk multiplier */
    if (bridge->enable_inflammation_anxiety) {
        if (inflammation_intensity >= INFLAMMATION_ANXIETY_THRESHOLD) {
            state->anxiety_risk_multiplier = 1.0f + (inflammation_intensity * 1.5f);
        } else {
            state->anxiety_risk_multiplier = 1.0f;
        }
    } else {
        state->anxiety_risk_multiplier = 1.0f;
    }

    /* Psychosis risk from cytokine storm */
    if (state->current_level == INFLAMMATION_STORM) {
        state->psychosis_risk = clamp_f(inflammation_intensity * 0.7f, 0.0f, 1.0f);
    } else {
        state->psychosis_risk = 0.0f;
    }

    /* Cognitive impairment */
    state->cognitive_impairment = clamp_f(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    /* Neurotransmitter suppression */
    state->serotonin_suppression = clamp_f(inflammation_intensity * 0.5f, 0.0f, 1.0f);
    state->dopamine_suppression = clamp_f(inflammation_intensity * 0.4f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_immune_modulate_neurotransmitters(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_neurotransmitter_modulation) return 0;

    /* Neurotransmitter modulation would integrate with neuromodulator system */
    /* For now, just track the suppression levels in our state */

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    float total_suppression = bridge->cytokine_effects.neurotransmitter_suppression;
    if (bridge->inflammation_state.serotonin_suppression > total_suppression) {
        total_suppression = bridge->inflammation_state.serotonin_suppression;
    }

    /* Would apply suppression to actual neuromodulator levels here */
    /* This would require integration with nimcp_neuromodulators.h */

    return 0;
}

float mental_health_immune_compute_depression_risk(const mental_health_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    return bridge->inflammation_state.depression_risk_multiplier;
}

float mental_health_immune_compute_anxiety_risk(const mental_health_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    return bridge->inflammation_state.anxiety_risk_multiplier;
}

/* ============================================================================
 * Mental Health → Immune Implementation
 * ============================================================================ */

int mental_health_immune_trigger_from_depression(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_disorder_immune_trigger) return 0;
    if (!bridge->mental_health_monitor || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_trigger_from_depression: required parameter is NULL (bridge->mental_health_monitor, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mental_disorder_immune_trigger_t* trigger = &bridge->disorder_trigger;

    /* Get depression severity from mental health monitor */
    /* TODO: replace placeholder with actual mental_health_check_specific() query once API is available */
    trigger->depression_severity = 0.0f; /* Would query mental_health_check_specific() */

    bool do_release_cytokines = false;
    float il6_amount = 0.0f;
    float tnf_amount = 0.0f;

    /* High depression triggers immune response */
    if (trigger->depression_severity >= DEPRESSION_IMMUNE_TRIGGER_THRESHOLD) {
        trigger->depression_triggered = true;

        /* Depression → increased pro-inflammatory cytokines */
        il6_amount = trigger->depression_severity * 0.6f;
        tnf_amount = trigger->depression_severity * 0.5f;
        do_release_cytokines = true;

        bridge->depression_triggers++;
        bridge->disorder_triggered_responses++;
    } else {
        trigger->depression_triggered = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokines outside lock to avoid deadlock */
    if (do_release_cytokines) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(bridge->immune_system,
                                     BRAIN_CYTOKINE_IL6,
                                     0, /* source cell */
                                     il6_amount,
                                     0, /* broadcast */
                                     &cytokine_id);

        brain_immune_release_cytokine(bridge->immune_system,
                                     BRAIN_CYTOKINE_TNF,
                                     0,
                                     tnf_amount,
                                     0,
                                     &cytokine_id);
    }
    return 0;
}

int mental_health_immune_trigger_from_anxiety(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_disorder_immune_trigger) return 0;
    if (!bridge->mental_health_monitor || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_trigger_from_anxiety: required parameter is NULL (bridge->mental_health_monitor, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mental_disorder_immune_trigger_t* trigger = &bridge->disorder_trigger;

    /* Get anxiety severity */
    /* TODO: replace placeholder with actual mental_health_check_specific() query once API is available */
    trigger->anxiety_severity = 0.0f; /* Would query mental_health_check_specific() */

    bool do_release_il6 = false;
    float il6_amount = 0.0f;

    /* High anxiety triggers HPA axis and immune response */
    if (trigger->anxiety_severity >= ANXIETY_IMMUNE_TRIGGER_THRESHOLD) {
        trigger->anxiety_triggered = true;

        /* Anxiety → HPA axis activation → cortisol → initial immune suppression */
        trigger->cortisol_activation = clamp_f((trigger->anxiety_severity - 0.5f) * 2.0f, 0.0f, 1.0f);
        trigger->immune_suppression = trigger->cortisol_activation * 0.4f;

        /* Followed by inflammatory rebound */
        trigger->inflammatory_rebound = trigger->anxiety_severity * 0.5f;

        /* Prepare cytokine release (rebound effect) - applied outside lock */
        il6_amount = trigger->inflammatory_rebound;
        do_release_il6 = true;

        bridge->anxiety_triggers++;
        bridge->disorder_triggered_responses++;
    } else {
        trigger->anxiety_triggered = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokines outside lock to avoid deadlock */
    if (do_release_il6) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(bridge->immune_system,
                                     BRAIN_CYTOKINE_IL6,
                                     0,
                                     il6_amount,
                                     0,
                                     &cytokine_id);
    }
    return 0;
}

int mental_health_immune_trigger_from_ptsd(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_disorder_immune_trigger) return 0;
    if (!bridge->mental_health_monitor || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_trigger_from_ptsd: required parameter is NULL (bridge->mental_health_monitor, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mental_disorder_immune_trigger_t* trigger = &bridge->disorder_trigger;

    /* Get PTSD severity */
    /* TODO: replace placeholder with actual mental_health_check_specific() query once API is available */
    trigger->ptsd_severity = 0.0f; /* Would query mental_health_check_specific() */

    bool do_release_ptsd_cytokines = false;
    float ptsd_cytokine_level = 0.0f;

    /* High PTSD triggers chronic inflammation */
    if (trigger->ptsd_severity >= PTSD_IMMUNE_TRIGGER_THRESHOLD) {
        trigger->ptsd_triggered = true;

        /* PTSD → chronic inflammatory state */
        trigger->chronic_inflammation_level = trigger->ptsd_severity * 0.7f;
        ptsd_cytokine_level = trigger->chronic_inflammation_level;
        do_release_ptsd_cytokines = true;

        bridge->ptsd_triggers++;
        bridge->disorder_triggered_responses++;
    } else {
        trigger->ptsd_triggered = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokines outside lock to avoid deadlock */
    if (do_release_ptsd_cytokines) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(bridge->immune_system,
                                     BRAIN_CYTOKINE_IL1,
                                     0,
                                     ptsd_cytokine_level,
                                     0,
                                     &cytokine_id);

        brain_immune_release_cytokine(bridge->immune_system,
                                     BRAIN_CYTOKINE_IL6,
                                     0,
                                     ptsd_cytokine_level,
                                     0,
                                     &cytokine_id);
    }
    return 0;
}

int mental_health_immune_boost_from_recovery(mental_health_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_recovery_immune_boost) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_boost_from_recovery: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    recovery_immune_enhancement_t* boost = &bridge->recovery_boost;

    /* Check for recent intervention */
    /* Would need to query mental health monitor for intervention history */
    /* For now, assume no recent intervention */
    boost->recent_intervention = false;

    bool do_release_il10 = false;
    float il10_amount = 0.0f;

    if (boost->recent_intervention && boost->intervention_effectiveness > 0.5f) {
        /* Successful intervention → immune normalization */
        boost->immune_normalization = boost->intervention_effectiveness * 0.6f;

        /* Release IL-10 (anti-inflammatory) */
        boost->il10_release_boost = RECOVERY_IL10_BOOST * boost->intervention_effectiveness;
        il10_amount = boost->il10_release_boost;
        do_release_il10 = true;

        /* Reduce inflammation */
        boost->inflammation_reduction = boost->intervention_effectiveness * 0.5f;

        /* Reduce stress response */
        boost->stress_response_reduction = boost->intervention_effectiveness * 0.4f;

        bridge->recovery_boosts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokines outside lock to avoid deadlock */
    if (do_release_il10) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(bridge->immune_system,
                                     BRAIN_CYTOKINE_IL10,
                                     0,
                                     il10_amount,
                                     0,
                                     &cytokine_id);
    }
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int mental_health_immune_bridge_update(
    mental_health_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_update", 0.0f);


    (void)delta_ms; /* Unused for now */

    /* Apply all bidirectional effects */

    /* Immune → Mental Health */
    mental_health_immune_apply_cytokine_effects(bridge);
    mental_health_immune_apply_inflammation_effects(bridge);
    mental_health_immune_modulate_neurotransmitters(bridge);

    /* Mental Health → Immune */
    mental_health_immune_trigger_from_depression(bridge);
    mental_health_immune_trigger_from_anxiety(bridge);
    mental_health_immune_trigger_from_ptsd(bridge);
    mental_health_immune_boost_from_recovery(bridge);

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int mental_health_immune_get_cytokine_effects(
    const mental_health_immune_bridge_t* bridge,
    cytokine_mental_health_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_mental_health_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_immune_get_inflammation_state(
    const mental_health_immune_bridge_t* bridge,
    inflammation_mental_health_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_mental_health_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool mental_health_immune_is_cytokine_depression(const mental_health_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Cytokine-induced depression threshold */
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    return bridge->cytokine_effects.total_depression_risk_shift >= 0.3f;
}

float mental_health_immune_get_neurotransmitter_suppression(const mental_health_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    return bridge->cytokine_effects.neurotransmitter_suppression;
}

int mental_health_immune_get_stats(
    const mental_health_immune_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* depression_triggers,
    uint32_t* anxiety_triggers
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (total_updates) *total_updates = bridge->total_updates;
    if (depression_triggers) *depression_triggers = bridge->depression_triggers;
    if (anxiety_triggers) *anxiety_triggers = bridge->anxiety_triggers;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define MENTAL_HEALTH_IMMUNE_MODULE_NAME "mental_health_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int mental_health_immune_connect_bio_async(mental_health_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_MENTAL_HEALTH,
        .module_name = MENTAL_HEALTH_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("mental_health_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int mental_health_immune_disconnect_bio_async(mental_health_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("mental_health_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool mental_health_immune_is_bio_async_connected(const mental_health_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about mental health immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int mental_health_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mental_health_immune_bridge_heartbeat("mental_healt_mental_health_immune", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mental_Health_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mental_health_immune_bridge_heartbeat("mental_healt_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mental health immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mental_Health_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mental_Health_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mental_health_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mental_health_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mental_health_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    mental_health_immune_bridge_heartbeat_instance(NULL, "mental_health_immune_bridge_training_begin", 0.0f);
    return 0;
}

int mental_health_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    mental_health_immune_bridge_heartbeat_instance(NULL, "mental_health_immune_bridge_training_end", 1.0f);
    return 0;
}

int mental_health_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    mental_health_immune_bridge_heartbeat_instance(NULL, "mental_health_immune_bridge_training_step", progress);
    return 0;
}
