/**
 * @file nimcp_astrocyte_immune_bridge.c
 * @brief Astrocyte-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional coupling between brain immune and astrocyte systems
 * WHY:  Cytokines trigger reactive astrogliosis affecting synaptic function
 * HOW:  Monitor cytokines to modulate astrocyte state, monitor astrocytes for dysfunction
 */

#include "plasticity/astrocyte/nimcp_astrocyte_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(plasticity_astrocyte_immune)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(astrocyte_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Map inflammation level to D-serine factor
 */
static float inflammation_to_d_serine_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_ASTRO_NONE_D_SERINE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_ASTRO_LOCAL_D_SERINE;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_ASTRO_REGIONAL_D_SERINE;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_ASTRO_SYSTEMIC_D_SERINE;
        case INFLAMMATION_STORM:    return INFLAMMATION_ASTRO_STORM_D_SERINE;
        default:                    return INFLAMMATION_ASTRO_NONE_D_SERINE;
    }
}

/**
 * @brief Map inflammation level to glutamate uptake factor
 */
static float inflammation_to_glu_uptake_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_ASTRO_NONE_GLU_UPTAKE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_ASTRO_LOCAL_GLU_UPTAKE;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_ASTRO_REGIONAL_GLU_UPTAKE;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_ASTRO_SYSTEMIC_GLU_UPTAKE;
        case INFLAMMATION_STORM:    return INFLAMMATION_ASTRO_STORM_GLU_UPTAKE;
        default:                    return INFLAMMATION_ASTRO_NONE_GLU_UPTAKE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int astrocyte_immune_default_config(astrocyte_immune_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "Astrocyte-immune config is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_default_config: config is NULL");

    /* All features enabled by default */
    config->enable_cytokine_modulation = true;
    config->enable_inflammation_effects = true;
    config->enable_dysfunction_detection = true;
    config->enable_reactive_state_control = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->dysfunction_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->glu_uptake_critical_threshold = ASTROCYTE_GLU_UPTAKE_CRITICAL_THRESHOLD;
    config->d_serine_critical_threshold = ASTROCYTE_D_SERINE_CRITICAL_THRESHOLD;
    config->ca_wave_excessive_threshold = ASTROCYTE_CA_WAVE_EXCESSIVE_THRESHOLD;

    return 0;
}

astrocyte_immune_bridge_t* astrocyte_immune_bridge_create(
    const astrocyte_immune_config_t* config,
    brain_immune_system_t* immune_system,
    astrocyte_plasticity_t astrocyte_system
) {
    /* Guard: require both systems */
    NIMCP_API_CHECK_NULL_RET_NULL(immune_system, "Immune system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_bridge_create: immune_system is NULL");
    NIMCP_API_CHECK_NULL_RET_NULL(astrocyte_system, "Astrocyte system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_bridge_create: astrocyte_system is NULL");

    /* Allocate bridge */
    astrocyte_immune_bridge_t* bridge = (astrocyte_immune_bridge_t*)
        nimcp_malloc(sizeof(astrocyte_immune_bridge_t));
    if (!bridge) {
        LOG_ERROR("Astrocyte-immune bridge allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "astrocyte_immune_bridge_create: bridge allocation failed");
        return NULL;
    }
    memset(bridge, 0, sizeof(astrocyte_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->astrocyte_system = astrocyte_system;

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(astrocyte_immune_config_t));
    } else {
        astrocyte_immune_default_config(&bridge->config);
    }

    /* Create mutex */
    bridge->base.mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("Astrocyte-immune bridge mutex allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Astrocyte-immune bridge mutex allocation failed");
        return NULL;
    }
    nimcp_mutex_init((nimcp_mutex_t*)bridge->base.mutex, NULL);

    NIMCP_LOGGING_INFO("Astrocyte-immune bridge created successfully");
    return bridge;
}

void astrocyte_immune_bridge_destroy(astrocyte_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        astrocyte_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge */
    nimcp_free(bridge);
    NIMCP_LOGGING_DEBUG("Astrocyte-immune bridge destroyed");
}

/* ============================================================================
 * Immune → Astrocyte Implementation
 * ============================================================================ */

int astrocyte_immune_apply_cytokine_effects(astrocyte_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_apply_cytokine_effects: bridge is NULL");
    if (!bridge->config.enable_cytokine_modulation) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "Immune system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "astrocyte_immune_apply_cytokine_effects: immune_system is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    cytokine_astrocyte_effects_t* effects = &bridge->cytokine_effects;

    /* Compute cytokine effects */
    /* Note: Would query actual cytokine levels from immune system */

    /* Pro-inflammatory cytokines → D-serine reduction */
    effects->il1_d_serine_reduction = CYTOKINE_IL1_D_SERINE_REDUCTION;
    effects->il6_d_serine_reduction = CYTOKINE_IL6_D_SERINE_REDUCTION;
    effects->tnf_d_serine_reduction = CYTOKINE_TNF_D_SERINE_REDUCTION;
    effects->ifn_gamma_d_serine_reduction = CYTOKINE_IFN_GAMMA_D_SERINE_REDUCTION;

    /* Pro-inflammatory cytokines → glutamate uptake impairment */
    effects->il1_glu_uptake_impairment = CYTOKINE_IL1_GLU_UPTAKE_IMPAIRMENT;
    effects->il6_glu_uptake_impairment = CYTOKINE_IL6_GLU_UPTAKE_IMPAIRMENT;
    effects->tnf_glu_uptake_impairment = CYTOKINE_TNF_GLU_UPTAKE_IMPAIRMENT;
    effects->ifn_gamma_glu_uptake_impairment = CYTOKINE_IFN_GAMMA_GLU_UPTAKE_IMPAIRMENT;

    /* Anti-inflammatory cytokines → restoration */
    effects->il10_d_serine_restoration = CYTOKINE_IL10_D_SERINE_RESTORATION;
    effects->il10_glu_uptake_restoration = CYTOKINE_IL10_GLU_UPTAKE_RESTORATION;

    /* Aggregate D-serine modulation (multiplicative) */
    effects->total_d_serine_modulation =
        effects->il1_d_serine_reduction *
        effects->il6_d_serine_reduction *
        effects->tnf_d_serine_reduction *
        effects->ifn_gamma_d_serine_reduction *
        effects->il10_d_serine_restoration;

    /* Aggregate glutamate uptake modulation */
    effects->total_glu_uptake_modulation =
        effects->il1_glu_uptake_impairment *
        effects->il6_glu_uptake_impairment *
        effects->tnf_glu_uptake_impairment *
        effects->ifn_gamma_glu_uptake_impairment *
        effects->il10_glu_uptake_restoration;

    /* Reactive state intensity (how far from resting) */
    float pro_inflammatory_factor =
        (2.0f - effects->il1_d_serine_reduction -
                effects->tnf_d_serine_reduction) / 2.0f;
    float anti_inflammatory_factor =
        (effects->il10_d_serine_restoration - 1.0f);
    effects->reactive_state_intensity = nimcp_clampf(
        pro_inflammatory_factor - anti_inflammatory_factor, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int astrocyte_immune_apply_inflammation_effects(
    astrocyte_immune_bridge_t* bridge
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_apply_inflammation_effects: bridge is NULL");
    if (!bridge->config.enable_inflammation_effects) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "Immune system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "astrocyte_immune_apply_inflammation_effects: immune_system is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_astrocyte_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state (placeholder - would query immune system) */
    state->current_level = INFLAMMATION_NONE;
    state->inflammation_duration_sec = 0.0f;
    state->is_chronic = false;

    /* Map inflammation to astrocyte effects */
    state->d_serine_reduction =
        1.0f - inflammation_to_d_serine_factor(state->current_level);
    state->glu_uptake_impairment =
        1.0f - inflammation_to_glu_uptake_factor(state->current_level);

    /* Calcium dysregulation increases with inflammation */
    float inflammation_intensity =
        (float)state->current_level / (float)INFLAMMATION_STORM;
    state->calcium_dysregulation = nimcp_clampf(inflammation_intensity * 0.5f, 0.0f, 0.8f);

    /* ATP/adenosine balance alteration */
    state->atp_release_alteration = inflammation_intensity * 0.3f;

    /* Determine target reactive state */
    if (state->current_level >= INFLAMMATION_REGIONAL) {
        state->target_state = ASTROCYTE_A1_REACTIVE;
        state->a1_transition_progress = inflammation_intensity;
        state->a2_transition_progress = 0.0f;
    } else if (state->current_level == INFLAMMATION_NONE &&
               bridge->cytokine_effects.il10_d_serine_restoration > 1.0f) {
        state->target_state = ASTROCYTE_A2_REACTIVE;
        state->a1_transition_progress = 0.0f;
        state->a2_transition_progress = 0.5f;
    } else {
        state->target_state = ASTROCYTE_RESTING;
        state->a1_transition_progress = 0.0f;
        state->a2_transition_progress = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int astrocyte_immune_transition_reactive_state(
    astrocyte_immune_bridge_t* bridge,
    uint32_t astrocyte_id
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_transition_reactive_state: bridge is NULL");
    if (!bridge->config.enable_reactive_state_control) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    astrocyte_reactive_state_t target_state =
        bridge->inflammation_state.target_state;
    float intensity = bridge->cytokine_effects.reactive_state_intensity;

    /* Transition astrocyte to reactive state */
    int result = astrocyte_plasticity_set_reactive_state(
        bridge->astrocyte_system,
        astrocyte_id,
        target_state,
        intensity
    );

    if (result == 0) {
        bridge->reactive_state_transitions++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Astrocyte → Immune Implementation
 * ============================================================================ */

int astrocyte_immune_detect_dysfunction(astrocyte_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_detect_dysfunction: bridge is NULL");
    if (!bridge->config.enable_dysfunction_detection) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    astrocyte_dysfunction_state_t* state = &bridge->dysfunction_state;

    /* Query all astrocytes for average dysfunction */
    uint32_t num_astrocytes =
        astrocyte_plasticity_get_num_astrocytes(bridge->astrocyte_system);

    float total_glu_uptake = 0.0f;
    float total_d_serine = 0.0f;
    float total_ca_activity = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < num_astrocytes; i++) {
        astrocyte_state_t astro_state;
        if (astrocyte_plasticity_get_state(bridge->astrocyte_system, i,
                                            &astro_state) == 0) {
            total_glu_uptake += astro_state.glutamate_uptake_rate;
            total_d_serine += astro_state.d_serine_level;
            total_ca_activity += astro_state.calcium_wave_amplitude;
            count++;
        }
    }

    if (count > 0) {
        state->current_glu_uptake = total_glu_uptake / count;
        state->current_d_serine = total_d_serine / count;
        state->calcium_wave_frequency = total_ca_activity / count;
    }

    /* Check for dysfunction conditions */
    state->glu_uptake_critical =
        state->current_glu_uptake < bridge->config.glu_uptake_critical_threshold;
    state->d_serine_depleted =
        state->current_d_serine < bridge->config.d_serine_critical_threshold;
    state->calcium_excessive =
        state->calcium_wave_frequency > bridge->config.ca_wave_excessive_threshold;
    state->excitotoxicity_risk =
        state->glu_uptake_critical && state->calcium_excessive;

    /* Compute overall dysfunction severity */
    float severity = 0.0f;
    if (state->glu_uptake_critical) severity += 0.4f;
    if (state->d_serine_depleted) severity += 0.3f;
    if (state->calcium_excessive) severity += 0.2f;
    if (state->excitotoxicity_risk) severity += 0.3f;
    state->dysfunction_severity = nimcp_clampf(severity, 0.0f, 1.0f);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int astrocyte_immune_alert_dysfunction(
    astrocyte_immune_bridge_t* bridge,
    uint32_t* antigen_id
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_alert_dysfunction: bridge is NULL");
    NIMCP_API_CHECK_NULL(antigen_id, -1, "Antigen ID output pointer is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_alert_dysfunction: antigen_id is NULL");
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "Immune system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "astrocyte_immune_alert_dysfunction: immune_system is NULL");

    /* Check if dysfunction is significant */
    if (bridge->dysfunction_state.dysfunction_severity < 0.5f) {
        return -1;  /* No significant dysfunction */
    }

    /* Create epitope from dysfunction signature */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Encode dysfunction type */
    epitope[0] = 0xBB;  /* Astrocyte dysfunction marker */
    if (bridge->dysfunction_state.excitotoxicity_risk) {
        epitope[1] = 0x01;  /* Excitotoxicity risk */
    } else if (bridge->dysfunction_state.glu_uptake_critical) {
        epitope[1] = 0x02;  /* Glutamate uptake failure */
    } else if (bridge->dysfunction_state.d_serine_depleted) {
        epitope[1] = 0x03;  /* D-serine depletion */
    }

    /* Encode severity */
    uint32_t severity =
        (uint32_t)(bridge->dysfunction_state.dysfunction_severity * 10.0f);
    severity = (severity < 1) ? 1 : ((severity > 10) ? 10 : severity);

    /* Present to immune system */
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(epitope),
        severity,
        0,
        antigen_id
    );

    if (result == 0) {
        bridge->dysfunction_alerts++;
    }

    return result;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int astrocyte_immune_bridge_update(
    astrocyte_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_bridge_update: bridge is NULL");
    (void)delta_ms;

    /* Apply all bidirectional effects */

    /* Immune → Astrocyte */
    astrocyte_immune_apply_cytokine_effects(bridge);
    astrocyte_immune_apply_inflammation_effects(bridge);

    /* Apply reactive state transitions to all astrocytes */
    uint32_t num_astrocytes =
        astrocyte_plasticity_get_num_astrocytes(bridge->astrocyte_system);
    for (uint32_t i = 0; i < num_astrocytes; i++) {
        astrocyte_immune_transition_reactive_state(bridge, i);
    }

    /* Astrocyte → Immune */
    astrocyte_immune_detect_dysfunction(bridge);

    /* Alert immune system if dysfunction detected */
    if (bridge->dysfunction_state.dysfunction_severity >= 0.5f) {
        uint32_t antigen_id;
        astrocyte_immune_alert_dysfunction(bridge, &antigen_id);
    }

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int astrocyte_immune_get_cytokine_effects(
    const astrocyte_immune_bridge_t* bridge,
    cytokine_astrocyte_effects_t* effects
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_get_cytokine_effects: bridge is NULL");
    NIMCP_API_CHECK_NULL(effects, -1, "Effects output pointer is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_get_cytokine_effects: effects is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_astrocyte_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int astrocyte_immune_get_inflammation_state(
    const astrocyte_immune_bridge_t* bridge,
    inflammation_astrocyte_state_t* state
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_get_inflammation_state: bridge is NULL");
    NIMCP_API_CHECK_NULL(state, -1, "State output pointer is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_get_inflammation_state: state is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_astrocyte_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int astrocyte_immune_get_dysfunction_state(
    const astrocyte_immune_bridge_t* bridge,
    astrocyte_dysfunction_state_t* state
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_get_dysfunction_state: bridge is NULL");
    NIMCP_API_CHECK_NULL(state, -1, "State output pointer is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_get_dysfunction_state: state is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->dysfunction_state, sizeof(astrocyte_dysfunction_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool astrocyte_immune_is_function_impaired(
    const astrocyte_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    return (bridge->cytokine_effects.total_d_serine_modulation < 1.0f) ||
           (bridge->cytokine_effects.total_glu_uptake_modulation < 1.0f);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define ASTROCYTE_IMMUNE_MODULE_NAME "astrocyte_immune_bridge"

int astrocyte_immune_connect_bio_async(astrocyte_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_connect_bio_async: bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ASTROCYTE,
        .module_name = ASTROCYTE_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Astrocyte-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int astrocyte_immune_disconnect_bio_async(astrocyte_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-immune bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_immune_disconnect_bio_async: bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("Astrocyte-immune bridge disconnected from bio-async router");
    return 0;
}

bool astrocyte_immune_is_bio_async_connected(
    const astrocyte_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
