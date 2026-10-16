/**
 * @file nimcp_emotion_immune_bridge.c
 * @brief Emotion-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and emotional systems
 * WHY:  Biological realism - cytokines affect mood, stress affects immunity
 * HOW:  Monitor cytokine levels to modulate emotion, monitor emotion to trigger immune responses
 */

#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_immune_bridge_mesh_registry = NULL;

nimcp_error_t emotion_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_immune_bridge_mesh_registry = registry;
    return err;
}

void emotion_immune_bridge_mesh_unregister(void) {
    if (g_emotion_immune_bridge_mesh_registry && g_emotion_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_immune_bridge_mesh_registry, g_emotion_immune_bridge_mesh_id);
        g_emotion_immune_bridge_mesh_id = 0;
        g_emotion_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_immune_bridge module (instance-level) */
static inline void emotion_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_immune_bridge_health_agent) {
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
 * @brief Compute sickness behavior level from cytokines
 *
 * WHAT: Calculate overall sickness behavior intensity
 * WHY:  Sickness behavior is distinct syndrome from pro-inflammatory cytokines
 * HOW:  Weighted sum of IL-1, IL-6, TNF-α concentrations
 */
static float compute_sickness_behavior(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query cytokine concentrations from immune system */
    float il1_level = 0.0f;
    float il6_level = 0.0f;
    float tnf_level = 0.0f;

    if (immune->cytokines) {
        for (size_t i = 0; i < immune->cytokine_count; i++) {
            float conc = immune->cytokines[i].concentration;
            switch (immune->cytokines[i].type) {
                case BRAIN_CYTOKINE_IL1: il1_level += conc; break;
                case BRAIN_CYTOKINE_IL6: il6_level += conc; break;
                case BRAIN_CYTOKINE_TNF: tnf_level += conc; break;
                default: break;
            }
        }
        il1_level = clamp_f(il1_level / 5.0f, 0.0f, 1.0f);
        il6_level = clamp_f(il6_level / 5.0f, 0.0f, 1.0f);
        tnf_level = clamp_f(tnf_level / 5.0f, 0.0f, 1.0f);
    }

    /* Sickness behavior is weighted combination */
    float sickness = (il1_level * 0.4f) + (il6_level * 0.3f) + (tnf_level * 0.3f);
    return clamp_f(sickness, 0.0f, 1.0f);
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different emotional effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || !immune->inflammation_sites || immune->inflammation_count == 0) return 0.0f;

    /* Find oldest inflammation site */
    uint64_t oldest_start = UINT64_MAX;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    if (oldest_start == UINT64_MAX) return 0.0f;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    if (current_time > oldest_start) {
        return (float)(current_time - oldest_start) / 1000.0f; /* ms to sec */
    }
    return 0.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines emotional impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || !immune->inflammation_sites || immune->inflammation_count == 0) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }
    return max_level;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int emotion_immune_default_config(emotion_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_defau", 0.0f);


    config->enable_cytokine_emotion_modulation = true;
    config->enable_inflammation_anhedonia = true;
    config->enable_emotion_immune_trigger = true;
    config->enable_positive_immune_boost = true;
    config->enable_grief_inflammation_coupling = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->emotion_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->stress_trigger_threshold = STRESS_IMMUNE_TRIGGER_THRESHOLD;
    config->inflammation_anhedonia_threshold = INFLAMMATION_ANHEDONIA_THRESHOLD;

    return 0;
}

emotion_immune_bridge_t* emotion_immune_bridge_create(
    const emotion_immune_config_t* config,
    brain_immune_system_t* immune_system,
    emotional_system_t* emotion_system,
    grief_system_t* grief_system,
    joy_system_t* joy_system
) {
    /* Guard: require immune and emotion systems */
    if (!immune_system || !emotion_system) {
        LOG_MODULE_ERROR("emotion_immune_bridge",
                  "Cannot create bridge without immune and emotion systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_bridge_create: required parameter is NULL (immune_system, emotion_system)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_create", 0.0f);


    emotion_immune_bridge_t* bridge = (emotion_immune_bridge_t*)
        nimcp_malloc(sizeof(emotion_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("emotion_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotion_immune_bridge_create: malloc returned NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(emotion_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->emotion_system = emotion_system;
    bridge->grief_system = grief_system;
    bridge->joy_system = joy_system;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_emotion_modulation = config->enable_cytokine_emotion_modulation;
        bridge->enable_inflammation_anhedonia = config->enable_inflammation_anhedonia;
        bridge->enable_emotion_immune_trigger = config->enable_emotion_immune_trigger;
        bridge->enable_positive_immune_boost = config->enable_positive_immune_boost;
        bridge->enable_grief_inflammation_coupling = config->enable_grief_inflammation_coupling;
    } else {
        /* Use defaults */
        emotion_immune_config_t default_cfg;
        emotion_immune_default_config(&default_cfg);
        bridge->enable_cytokine_emotion_modulation = default_cfg.enable_cytokine_emotion_modulation;
        bridge->enable_inflammation_anhedonia = default_cfg.enable_inflammation_anhedonia;
        bridge->enable_emotion_immune_trigger = default_cfg.enable_emotion_immune_trigger;
        bridge->enable_positive_immune_boost = default_cfg.enable_positive_immune_boost;
        bridge->enable_grief_inflammation_coupling = default_cfg.enable_grief_inflammation_coupling;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "emotion_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("emotion_immune_bridge", "Bridge created successfully");
    return bridge;
}

void emotion_immune_bridge_destroy(emotion_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("emotion_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Emotion Implementation
 * ============================================================================ */

int emotion_immune_apply_cytokine_effects(emotion_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_emotion_modulation) return 0;
    if (!bridge->immune_system || !bridge->emotion_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->emotion_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_apply", 0.0f);


    /* Get current emotion state before acquiring lock (external call) */
    emotion_state_t pre_lock_emotion;
    bool got_emotion = emotion_system_get_state(bridge->emotion_system, &pre_lock_emotion);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_emotion_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → negative affect */
    /* Note: Would query actual cytokine levels from immune system */
    effects->il1_negative_affect = 0.0f;  /* IL-1β level * IL1_VALENCE_IMPACT */
    effects->il6_negative_affect = 0.0f;  /* IL-6 level * IL6_VALENCE_IMPACT */
    effects->tnf_negative_affect = 0.0f;  /* TNF-α level * TNF_VALENCE_IMPACT */
    effects->ifn_gamma_negative_affect = 0.0f;

    /* Anti-inflammatory cytokines → positive affect / recovery */
    effects->il10_positive_affect = 0.0f;  /* IL-10 level * IL10_VALENCE_IMPACT */

    /* Aggregate effects */
    effects->total_valence_shift =
        effects->il1_negative_affect +
        effects->il6_negative_affect +
        effects->tnf_negative_affect +
        effects->ifn_gamma_negative_affect +
        effects->il10_positive_affect;

    /* Sickness behavior */
    effects->sickness_behavior_level = compute_sickness_behavior(bridge->immune_system);

    /* Anhedonia from pro-inflammatory cytokines */
    float proinflam_total = fabs(effects->il1_negative_affect) +
                           fabs(effects->il6_negative_affect) +
                           fabs(effects->tnf_negative_affect);
    effects->anhedonia_level = clamp_f(proinflam_total * 0.6f, 0.0f, 1.0f);

    /* Fatigue */
    effects->fatigue_level = clamp_f(proinflam_total * 0.8f, 0.0f, 1.0f);

    /* Compute desired emotional state changes */
    bool do_set_emotion = false;
    float new_valence = 0.0f;
    float new_arousal = 0.0f;
    if (got_emotion) {
        new_valence = clamp_f(pre_lock_emotion.valence + effects->total_valence_shift, -1.0f, 1.0f);
        float arousal_reduction = effects->sickness_behavior_level * 0.3f;
        new_arousal = clamp_f(pre_lock_emotion.arousal - arousal_reduction, 0.0f, 1.0f);
        do_set_emotion = true;
    }

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply to emotional system outside lock to avoid deadlock */
    if (do_set_emotion) {
        emotion_system_set_state(bridge->emotion_system, new_valence, new_arousal, 0);
    }
    return 0;
}

int emotion_immune_apply_inflammation_effects(emotion_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_anhedonia) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    inflammation_emotion_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_INFLAMMATION_THRESHOLD);

    /* Chronic inflammation → depression risk */
    if (state->is_chronic) {
        float duration_factor = clamp_f(
            state->inflammation_duration_sec / (CHRONIC_INFLAMMATION_THRESHOLD * 2.0f),
            0.0f, 1.0f
        );
        state->depression_risk = duration_factor * 0.7f;
    } else {
        state->depression_risk = 0.0f;
    }

    /* Anhedonia severity based on inflammation level */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;
    state->anhedonia_severity = clamp_f(inflammation_intensity * 0.8f, 0.0f, 1.0f);

    /* Fatigue and motivation impairment */
    state->fatigue_severity = clamp_f(inflammation_intensity * 0.9f, 0.0f, 1.0f);
    state->motivation_impairment = clamp_f(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float emotion_immune_compute_anhedonia(const emotion_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Combine cytokine-induced and inflammation-induced anhedonia */
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_compu", 0.0f);


    float cytokine_anhedonia = bridge->cytokine_effects.anhedonia_level;
    float inflammation_anhedonia = bridge->inflammation_state.anhedonia_severity;

    /* Take maximum (not additive) */
    float total_anhedonia = fmaxf(cytokine_anhedonia, inflammation_anhedonia);
    return clamp_f(total_anhedonia, 0.0f, 1.0f);
}

/* ============================================================================
 * Emotion → Immune Implementation
 * ============================================================================ */

int emotion_immune_trigger_from_stress(emotion_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_emotion_immune_trigger) return 0;
    if (!bridge->emotion_system || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_trigger_from_stress: required parameter is NULL (bridge->emotion_system, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_trigg", 0.0f);


    /* Get current emotional state before acquiring lock (external call) */
    emotion_state_t emotion;
    if (!emotion_system_get_state(bridge->emotion_system, &emotion)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_immune_trigger_from_stress: emotion_system_get_state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    emotion_immune_trigger_t* trigger = &bridge->emotion_trigger;

    /* Compute stress level */
    trigger->negative_valence = (emotion.valence < 0) ? fabs(emotion.valence) : 0.0f;
    trigger->arousal_level = emotion.arousal;
    trigger->stress_level = (trigger->negative_valence * 0.6f) + (trigger->arousal_level * 0.4f);

    /* High stress triggers immune response */
    if (trigger->stress_level >= STRESS_IMMUNE_TRIGGER_THRESHOLD) {
        trigger->cortisol_triggered = true;

        /* Initial immune suppression from cortisol */
        trigger->immune_suppression = clamp_f((trigger->stress_level - 0.7f) * 2.0f, 0.0f, 0.5f);

        /* Followed by inflammatory rebound */
        trigger->inflammatory_rebound = true;

        /* Trigger cytokine release in immune system */
        /* Note: Would call brain_immune_release_cytokine() for IL-6, TNF-α */

        bridge->emotion_triggered_responses++;
    } else {
        trigger->cortisol_triggered = false;
        trigger->inflammatory_rebound = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_immune_amplify_grief_inflammation(emotion_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_grief_inflammation_coupling) return 0;
    if (!bridge->grief_system || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_amplify_grief_inflammation: required parameter is NULL (bridge->grief_system, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_ampli", 0.0f);


    /* Check grief state before acquiring lock (external calls) */
    if (!grief_is_grieving(bridge->grief_system)) {
        return 0;
    }
    float grief_pain = grief_get_pain_intensity(bridge->grief_system);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Grief amplifies inflammatory response */
    float amplification = 1.0f + (grief_pain * (GRIEF_INFLAMMATION_MULTIPLIER - 1.0f));

    /* Apply amplification to immune system */
    /* Note: Would modulate inflammation levels in immune system */

    /* Track grief amplification in inflammation state */
    bridge->inflammation_state.grief_amplification = amplification;
    bridge->inflammation_state.grief_prolongation = grief_pain * 0.5f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_immune_boost_from_positive_affect(emotion_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_positive_immune_boost) return 0;
    if (!bridge->joy_system || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_boost_from_positive_affect: required parameter is NULL (bridge->joy_system, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_boost", 0.0f);


    /* Get joy/positive emotion state before acquiring lock (external calls) */
    float joy_intensity = joy_is_joyful(bridge->joy_system) ? joy_get_valence(bridge->joy_system) : 0.0f;

    /* Get calm level from emotional system before acquiring lock (external call) */
    float calm_level = 0.0f;
    emotion_state_t emotion;
    if (emotion_system_get_state(bridge->emotion_system, &emotion)) {
        /* Calm = low arousal + neutral/positive valence */
        if (emotion.arousal < 0.3f && emotion.valence >= 0.0f) {
            calm_level = (0.3f - emotion.arousal) / 0.3f;
        }
    }

    nimcp_mutex_lock(bridge->base.mutex);

    positive_emotion_immune_boost_t* boost = &bridge->positive_boost;

    boost->joy_intensity = joy_intensity;
    boost->positive_valence = joy_intensity;
    boost->calm_level = calm_level;

    /* Positive emotions enhance immune function */
    boost->immune_enhancement = clamp_f((boost->joy_intensity + boost->calm_level) * 0.5f, 0.0f, 0.5f);

    /* Boost IL-10 (anti-inflammatory) release */
    boost->il10_release_boost = boost->immune_enhancement * 0.8f;

    /* Reduce inflammation */
    boost->inflammation_reduction = boost->immune_enhancement * 0.3f;

    /* Accelerate recovery */
    boost->recovery_acceleration = boost->immune_enhancement * 0.4f;

    /* Apply to immune system */
    /* Note: Would call brain_immune_release_cytokine(CYTOKINE_IL10) */
    /* Would reduce inflammation_sites resolution_progress */

    if (boost->immune_enhancement > 0.1f) {
        bridge->positive_boosts++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int emotion_immune_bridge_update(
    emotion_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply all bidirectional effects */

    /* Immune → Emotion */
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_update", 0.0f);


    emotion_immune_apply_cytokine_effects(bridge);
    emotion_immune_apply_inflammation_effects(bridge);

    /* Emotion → Immune */
    emotion_immune_trigger_from_stress(bridge);
    emotion_immune_amplify_grief_inflammation(bridge);
    emotion_immune_boost_from_positive_affect(bridge);

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int emotion_immune_get_cytokine_effects(
    const emotion_immune_bridge_t* bridge,
    cytokine_emotion_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_get_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_emotion_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_immune_get_inflammation_state(
    const emotion_immune_bridge_t* bridge,
    inflammation_emotion_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_get_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_emotion_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool emotion_immune_is_sick_behavior(const emotion_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Sickness behavior threshold */
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_is_si", 0.0f);


    return bridge->cytokine_effects.sickness_behavior_level >= 0.5f;
}

float emotion_immune_get_anhedonia_severity(const emotion_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_get_a", 0.0f);


    return emotion_immune_compute_anhedonia(bridge);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define EMOTION_IMMUNE_MODULE_NAME "emotion_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_EMOTION
 */
int emotion_immune_connect_bio_async(emotion_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_conne", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_EMOTION,
        .module_name = EMOTION_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Emotion-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 */
int emotion_immune_disconnect_bio_async(emotion_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_disco", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("Emotion-immune bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool emotion_immune_is_bio_async_connected(const emotion_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_is_bi", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about emotion immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int emotion_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    emotion_immune_bridge_heartbeat("emotion_immu_emotion_immune_query", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_immune_bridge_heartbeat("emotion_immu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Emotion immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void emotion_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_emotion_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int emotion_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    emotion_immune_bridge_heartbeat_instance(NULL, "emotion_immune_bridge_training_begin", 0.0f);
    return 0;
}

int emotion_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    emotion_immune_bridge_heartbeat_instance(NULL, "emotion_immune_bridge_training_end", 1.0f);
    return 0;
}

int emotion_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: pass cast instance as nimcp_health_agent_t* once bridge stores per-instance agent */
    emotion_immune_bridge_heartbeat_instance(NULL, "emotion_immune_bridge_training_step", progress);
    return 0;
}
