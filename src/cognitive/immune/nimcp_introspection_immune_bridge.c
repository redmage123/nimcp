/**
 * @file nimcp_introspection_immune_bridge.c
 * @brief Introspection-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and introspection/metacognition
 * WHY:  Biological realism - cytokines impair metacognition, introspection detects sickness
 * HOW:  Monitor cytokine levels to reduce Phi/accuracy, monitor Phi to detect sickness
 */

#include "cognitive/immune/nimcp_introspection_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(introspection_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_introspection_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_introspection_immune_bridge_mesh_registry = NULL;

nimcp_error_t introspection_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_introspection_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "introspection_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "introspection_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_introspection_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_introspection_immune_bridge_mesh_registry = registry;
    return err;
}

void introspection_immune_bridge_mesh_unregister(void) {
    if (g_introspection_immune_bridge_mesh_registry && g_introspection_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_introspection_immune_bridge_mesh_registry, g_introspection_immune_bridge_mesh_id);
        g_introspection_immune_bridge_mesh_id = 0;
        g_introspection_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from introspection_immune_bridge module (instance-level) */
static inline void introspection_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_introspection_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_introspection_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_introspection_immune_bridge_health_agent) {
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
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query cytokine level from brain immune system
 * WHY:  Need cytokine data to compute introspection effects
 * HOW:  Search cytokines array for matching type, return concentration
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune || !immune->cytokines) return 0.0f;

    /* Search for cytokine of this type */
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            introspection_immune_bridge_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type && !immune->cytokines[i].delivered) {
            return immune->cytokines[i].concentration;
        }
    }
    return 0.0f;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>3 days) has different consciousness effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || !immune->inflammation_sites) return 0.0f;

    uint64_t oldest_time = UINT64_MAX;
    bool found = false;

    /* Find oldest inflammation site */
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            introspection_immune_bridge_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level != INFLAMMATION_NONE) {
            if (immune->inflammation_sites[i].start_time < oldest_time) {
                oldest_time = immune->inflammation_sites[i].start_time;
                found = true;
            }
        }
    }

    if (!found) return 0.0f;

    /* Compute duration using monotonic clock */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    if (current_time > oldest_time) {
        return (float)(current_time - oldest_time) / 1000.0f; /* ms to sec */
    }
    return 0.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines consciousness impact
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
            introspection_immune_bridge_heartbeat("introspectio_loop",
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

int introspection_immune_default_config(introspection_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    config->enable_cytokine_introspection_modulation = true;
    config->enable_inflammation_phi_reduction = true;
    config->enable_sickness_detection = true;
    config->enable_pattern_immune_correlation = true;
    config->enable_uncertainty_immune_coupling = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->sickness_detection_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->phi_sickness_threshold = PHI_SICKNESS_DETECTION_THRESHOLD;
    config->uncertainty_sickness_threshold = UNCERTAINTY_SICKNESS_THRESHOLD;

    return 0;
}

introspection_immune_bridge_t* introspection_immune_bridge_create(
    const introspection_immune_config_t* config,
    brain_immune_system_t* immune_system,
    introspection_context_t introspection_context
) {
    /* Guard: require immune and introspection systems */
    if (!immune_system || !introspection_context) {
        LOG_MODULE_ERROR("introspection_immune_bridge",
                  "Cannot create bridge without immune and introspection systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_bridge_create: required parameter is NULL (immune_system, introspection_context)");
        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_create", 0.0f);


    introspection_immune_bridge_t* bridge = (introspection_immune_bridge_t*)
        nimcp_malloc(sizeof(introspection_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("introspection_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_immune_bridge_create: allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(introspection_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->introspection_context = introspection_context;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_introspection_modulation = config->enable_cytokine_introspection_modulation;
        bridge->enable_inflammation_phi_reduction = config->enable_inflammation_phi_reduction;
        bridge->enable_sickness_detection = config->enable_sickness_detection;
        bridge->enable_pattern_immune_correlation = config->enable_pattern_immune_correlation;
        bridge->enable_uncertainty_immune_coupling = config->enable_uncertainty_immune_coupling;
    } else {
        /* Use defaults */
        introspection_immune_config_t default_cfg;
        introspection_immune_default_config(&default_cfg);
        bridge->enable_cytokine_introspection_modulation = default_cfg.enable_cytokine_introspection_modulation;
        bridge->enable_inflammation_phi_reduction = default_cfg.enable_inflammation_phi_reduction;
        bridge->enable_sickness_detection = default_cfg.enable_sickness_detection;
        bridge->enable_pattern_immune_correlation = default_cfg.enable_pattern_immune_correlation;
        bridge->enable_uncertainty_immune_coupling = default_cfg.enable_uncertainty_immune_coupling;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "introspection_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    /* Initialize baselines to defaults */
    bridge->baseline_phi = 0.5f; /* Will be set via introspection_immune_set_baseline */
    bridge->baseline_uncertainty = 0.3f;
    bridge->baseline_pattern_count = 0;

    LOG_MODULE_INFO("introspection_immune_bridge", "Bridge created successfully");
    return bridge;
}

void introspection_immune_bridge_destroy(introspection_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("introspection_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Introspection Implementation
 * ============================================================================ */

int introspection_immune_apply_cytokine_effects(introspection_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_introspection_modulation) return 0;
    if (!bridge->immune_system || !bridge->introspection_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->introspection_context)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Query cytokine levels from immune system */
    float il1_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn_gamma_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Compute cytokine effects on introspection */
    cytokine_introspection_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → metacognitive impairment */
    effects->il1_accuracy_impairment = il1_level * CYTOKINE_IL1_INTROSPECTION_IMPACT;
    effects->il6_accuracy_impairment = il6_level * CYTOKINE_IL6_INTROSPECTION_IMPACT;
    effects->tnf_accuracy_impairment = tnf_level * CYTOKINE_TNF_INTROSPECTION_IMPACT;
    effects->ifn_gamma_accuracy_impairment = ifn_gamma_level * CYTOKINE_IFN_GAMMA_INTROSPECTION_IMPACT;

    /* Anti-inflammatory cytokines → clarity restoration */
    effects->il10_clarity_restoration = il10_level * CYTOKINE_IL10_INTROSPECTION_IMPACT;

    /* Aggregate effects */
    effects->total_accuracy_shift =
        effects->il1_accuracy_impairment +
        effects->il6_accuracy_impairment +
        effects->tnf_accuracy_impairment +
        effects->ifn_gamma_accuracy_impairment +
        effects->il10_clarity_restoration;

    /* Consciousness impairment level */
    float proinflam_total = fabs(effects->il1_accuracy_impairment) +
                           fabs(effects->il6_accuracy_impairment) +
                           fabs(effects->tnf_accuracy_impairment) +
                           fabs(effects->ifn_gamma_accuracy_impairment);
    effects->consciousness_impairment_level = clamp_f(proinflam_total, 0.0f, 1.0f);

    /* Phi reduction from cytokines */
    effects->phi_reduction = clamp_f(proinflam_total * 0.5f, 0.0f, INFLAMMATION_PHI_REDUCTION_MAX);

    /* Epistemic uncertainty increase */
    effects->uncertainty_increase = clamp_f(proinflam_total * 0.7f, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_immune_apply_inflammation_effects(introspection_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_phi_reduction) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    inflammation_consciousness_state_t* state = &bridge->consciousness_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_METACOGNITIVE_IMPAIRMENT_THRESHOLD);

    /* Inflammation intensity (0-1 scale) */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;

    /* Phi reduction based on inflammation level */
    state->phi_reduction = clamp_f(inflammation_intensity * INFLAMMATION_PHI_REDUCTION_MAX, 0.0f, 1.0f);

    /* Metacognitive accuracy loss */
    state->metacognitive_accuracy_loss = clamp_f(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    /* Pattern detection impairment */
    state->pattern_detection_impairment = clamp_f(inflammation_intensity * 0.5f, 0.0f, 1.0f);

    /* State clarity loss */
    state->state_clarity_loss = clamp_f(inflammation_intensity * 0.7f, 0.0f, 1.0f);

    /* Chronic inflammation amplifies effects */
    if (state->is_chronic) {
        float duration_factor = clamp_f(
            state->inflammation_duration_sec / (CHRONIC_METACOGNITIVE_IMPAIRMENT_THRESHOLD * 2.0f),
            0.0f, 1.0f
        );
        state->metacognitive_accuracy_loss += duration_factor * 0.2f;
        state->phi_reduction += duration_factor * 0.15f;
        state->phi_reduction = clamp_f(state->phi_reduction, 0.0f, INFLAMMATION_PHI_REDUCTION_MAX);
    }

    /* Epistemic uncertainty increase */
    state->epistemic_uncertainty_increase = clamp_f(inflammation_intensity * 0.8f, 0.0f, 1.0f);

    /* Confidence reduction */
    state->confidence_reduction = clamp_f(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float introspection_immune_compute_phi_reduction(const introspection_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Combine cytokine-induced and inflammation-induced Phi reduction */
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    float cytokine_phi = bridge->cytokine_effects.phi_reduction;
    float inflammation_phi = bridge->consciousness_state.phi_reduction;

    /* Take maximum (not additive) */
    float total_phi_reduction = fmaxf(cytokine_phi, inflammation_phi);
    return clamp_f(total_phi_reduction, 0.0f, INFLAMMATION_PHI_REDUCTION_MAX);
}

float introspection_immune_compute_uncertainty_increase(
    const introspection_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    /* Combine cytokine-induced and inflammation-induced uncertainty */
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    float cytokine_unc = bridge->cytokine_effects.uncertainty_increase;
    float inflammation_unc = bridge->consciousness_state.epistemic_uncertainty_increase;

    /* Take maximum */
    float total_uncertainty_increase = fmaxf(cytokine_unc, inflammation_unc);
    return clamp_f(total_uncertainty_increase, 0.0f, 1.0f);
}

/* ============================================================================
 * Introspection → Immune Implementation
 * ============================================================================ */

int introspection_immune_detect_sickness(introspection_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_sickness_detection) return 0;
    if (!bridge->introspection_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_detect_sickness: bridge->introspection_context is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    introspection_sickness_detection_t* detection = &bridge->sickness_detection;

    /* Query current introspection metrics */
    /* Note: In actual implementation, would use consciousness metrics API */
    /* For now, use simplified approach */

    /* Simulate current Phi (would use introspection_compute_phi) */
    detection->phi_current = bridge->baseline_phi - bridge->cytokine_effects.phi_reduction;
    detection->phi_drop = bridge->baseline_phi - detection->phi_current;

    /* Simulate current uncertainty (would use brain_get_uncertainty) */
    detection->uncertainty_current = bridge->baseline_uncertainty +
                                    bridge->cytokine_effects.uncertainty_increase;
    detection->uncertainty_increase = detection->uncertainty_current - bridge->baseline_uncertainty;

    /* Pattern disruptions (simplified) */
    detection->pattern_disruptions = 0; /* Would count actual disruptions */
    detection->pattern_accuracy_loss = bridge->consciousness_state.pattern_detection_impairment;

    /* Detect sickness if Phi drop or uncertainty increase exceeds threshold */
    bool phi_indicates_sickness = (detection->phi_drop >= PHI_SICKNESS_DETECTION_THRESHOLD);
    bool uncertainty_indicates_sickness = (detection->uncertainty_current >= UNCERTAINTY_SICKNESS_THRESHOLD);
    bool patterns_indicate_sickness = (detection->pattern_accuracy_loss >= 0.4f);

    /* Sickness detected if at least 2 indicators agree */
    uint32_t indicator_count = 0;
    if (phi_indicates_sickness) indicator_count++;
    if (uncertainty_indicates_sickness) indicator_count++;
    if (patterns_indicate_sickness) indicator_count++;

    detection->sickness_detected = (indicator_count >= 2);

    /* Confidence based on number of indicators */
    if (indicator_count == 3) {
        detection->sickness_confidence = 0.9f;
    } else if (indicator_count == 2) {
        detection->sickness_confidence = 0.7f;
    } else if (indicator_count == 1) {
        detection->sickness_confidence = 0.4f;
    } else {
        detection->sickness_confidence = 0.0f;
    }

    /* Record detection time if newly detected */
    if (detection->sickness_detected) {
        struct timespec det_ts;
        clock_gettime(CLOCK_MONOTONIC, &det_ts);
        detection->detection_time = (uint64_t)det_ts.tv_sec * 1000000ULL + (uint64_t)det_ts.tv_nsec / 1000ULL;
        bridge->sickness_detections++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_immune_correlate_patterns(introspection_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_pattern_immune_correlation) return 0;
    if (!bridge->introspection_context || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_correlate_patterns: required parameter is NULL (bridge->introspection_context, bridge->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    /* Get current immune phase before acquiring lock (external call) */
    brain_immune_phase_t immune_phase = brain_immune_get_phase(bridge->immune_system);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Pattern detection is impaired during immune activation */
    if (immune_phase == IMMUNE_PHASE_ACTIVATION || immune_phase == IMMUNE_PHASE_EFFECTOR) {
        /* Record pattern disruption correlation */
        bridge->sickness_detection.pattern_disruptions++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int introspection_immune_bridge_update(
    introspection_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply immune → introspection effects */
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_update", 0.0f);


    introspection_immune_apply_cytokine_effects(bridge);
    introspection_immune_apply_inflammation_effects(bridge);

    /* Apply introspection → immune detection */
    introspection_immune_detect_sickness(bridge);
    introspection_immune_correlate_patterns(bridge);

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

int introspection_immune_get_cytokine_effects(
    const introspection_immune_bridge_t* bridge,
    cytokine_introspection_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_introspection_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_immune_get_consciousness_state(
    const introspection_immune_bridge_t* bridge,
    inflammation_consciousness_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_immune_get_consciousness_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->consciousness_state, sizeof(inflammation_consciousness_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool introspection_immune_is_sickness_detected(const introspection_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    return bridge->sickness_detection.sickness_detected;
}

float introspection_immune_get_phi_reduction(const introspection_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    return introspection_immune_compute_phi_reduction(bridge);
}

float introspection_immune_get_accuracy_loss(const introspection_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    return bridge->consciousness_state.metacognitive_accuracy_loss;
}

int introspection_immune_set_baseline(introspection_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->introspection_context) return -1;

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Capture current introspection metrics as baseline */
    /* Note: In actual implementation, would query real metrics */
    bridge->baseline_phi = 0.5f; /* Would use introspection_compute_phi */
    bridge->baseline_uncertainty = 0.3f; /* Would use brain_get_uncertainty */
    bridge->baseline_pattern_count = 0; /* Would count patterns */

    /* Initialize detection baseline values */
    bridge->sickness_detection.phi_baseline = bridge->baseline_phi;
    bridge->sickness_detection.uncertainty_baseline = bridge->baseline_uncertainty;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_immune_reset_sickness_detection(introspection_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear sickness detection state */
    bridge->sickness_detection.sickness_detected = false;
    bridge->sickness_detection.sickness_confidence = 0.0f;
    bridge->sickness_detection.pattern_disruptions = 0;
    bridge->sickness_detection.detection_time = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define INTROSPECTION_IMMUNE_MODULE_NAME "introspection_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int introspection_immune_connect_bio_async(introspection_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_INTROSPECTION,
        .module_name = INTROSPECTION_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("introspection_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int introspection_immune_disconnect_bio_async(introspection_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("introspection_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool introspection_immune_is_bio_async_connected(const introspection_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about introspection immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int introspection_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    introspection_immune_bridge_heartbeat("introspectio_introspection_immune", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Introspection_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                introspection_immune_bridge_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Introspection immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Introspection_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Introspection_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void introspection_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_introspection_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int introspection_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (introspection_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    introspection_immune_bridge_heartbeat_instance(NULL, "introspection_immune_bridge_training_begin", 0.0f);
    return 0;
}

int introspection_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (introspection_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    introspection_immune_bridge_heartbeat_instance(NULL, "introspection_immune_bridge_training_end", 1.0f);
    return 0;
}

int introspection_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass instance cast to (introspection_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    introspection_immune_bridge_heartbeat_instance(NULL, "introspection_immune_bridge_training_step", progress);
    return 0;
}
