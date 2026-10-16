/**
 * @file nimcp_astrocyte_immune_plasticity.c
 * @brief Plasticity-Focused Astrocyte-Immune Bridge Implementation
 * @version 2.0.0
 * @date 2025-12-27
 *
 * WHAT: Derived bridge for synaptic plasticity modulation
 * WHY:  D-serine/glutamate uptake directly affect NMDA-dependent LTP
 * HOW:  Inherits from base, adds plasticity-specific dysfunction detection
 */

#include "glial/immune/nimcp_astrocyte_immune_plasticity.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocyte_immune_plasticity)

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
 * Virtual Function Implementations
 * ============================================================================ */

static void plasticity_destroy(astrocyte_immune_base_t* base) {
    astro_plasticity_bridge_t* bridge = (astro_plasticity_bridge_t*)base;
    if (!bridge) return;

    /* Cleanup base */
    astro_immune_base_cleanup(&bridge->base);

    /* Free derived */
    nimcp_free(bridge);
    NIMCP_LOGGING_DEBUG("Plasticity astrocyte-immune bridge destroyed");
}

static int plasticity_apply_cytokine_effects(astrocyte_immune_base_t* base) {
    astro_plasticity_bridge_t* bridge = (astro_plasticity_bridge_t*)base;
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_cytokine_modulation) return NIMCP_SUCCESS;
    NIMCP_CHECK_THROW(base->immune_system, NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

    nimcp_platform_mutex_lock(base->infra.mutex);

    /* Compute D-serine modulation (multiplicative) */
    bridge->d_serine_factor =
        CYTOKINE_IL1_D_SERINE_REDUCTION *
        CYTOKINE_IL6_D_SERINE_REDUCTION *
        CYTOKINE_TNF_D_SERINE_REDUCTION *
        CYTOKINE_IFN_GAMMA_D_SERINE_REDUCTION *
        CYTOKINE_IL10_D_SERINE_RESTORATION;

    /* Compute glutamate uptake modulation */
    bridge->glu_uptake_factor =
        CYTOKINE_IL1_GLU_UPTAKE_IMPAIRMENT *
        CYTOKINE_IL6_GLU_UPTAKE_IMPAIRMENT *
        CYTOKINE_TNF_GLU_UPTAKE_IMPAIRMENT *
        CYTOKINE_IFN_GAMMA_GLU_UPTAKE_IMPAIRMENT *
        CYTOKINE_IL10_GLU_UPTAKE_RESTORATION;

    /* Update base state */
    base->cytokine_state.d_serine_modulation = bridge->d_serine_factor;
    base->cytokine_state.glutamate_clearance = bridge->glu_uptake_factor;

    /* Compute reactivity intensity */
    float pro_inflammatory =
        (2.0f - CYTOKINE_IL1_D_SERINE_REDUCTION - CYTOKINE_TNF_D_SERINE_REDUCTION) / 2.0f;
    float anti_inflammatory = CYTOKINE_IL10_D_SERINE_RESTORATION - 1.0f;
    base->cytokine_state.total_reactivity = nimcp_clampf(
        pro_inflammatory - anti_inflammatory, 0.0f, 1.0f);

    base->cytokine_state.is_reactive = base->cytokine_state.total_reactivity > 0.4f;
    base->cytokine_state.is_astrogliosis = base->cytokine_state.total_reactivity > 0.7f;

    bridge->cytokine_modulations++;
    nimcp_platform_mutex_unlock(base->infra.mutex);
    return 0;
}

static int plasticity_apply_inflammation_effects(astrocyte_immune_base_t* base) {
    astro_plasticity_bridge_t* bridge = (astro_plasticity_bridge_t*)base;
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_inflammation_effects) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(base->infra.mutex);

    /* Get inflammation level (placeholder - would query immune system) */
    brain_inflammation_level_t level = base->inflammation.level;

    /* Map to astrocyte effects */
    float d_serine_reduction = 1.0f - inflammation_to_d_serine_factor(level);
    float glu_uptake_impairment = 1.0f - inflammation_to_glu_uptake_factor(level);

    /* Combine with cytokine effects */
    bridge->d_serine_factor *= (1.0f - d_serine_reduction);
    bridge->glu_uptake_factor *= (1.0f - glu_uptake_impairment);

    /* Update inflammation state */
    float intensity = (float)level / (float)INFLAMMATION_STORM;
    base->inflammation.reactive_fraction = intensity;

    if (level >= INFLAMMATION_REGIONAL) {
        base->chronic_accumulator += 1.0f;
        base->inflammation.is_chronic = base->chronic_accumulator >= 30.0f;
    } else {
        base->chronic_accumulator *= NIMCP_EMA_WEIGHT_SLOW;
    }

    nimcp_platform_mutex_unlock(base->infra.mutex);
    return 0;
}

static float plasticity_compute_reactivity(const astrocyte_immune_base_t* base) {
    if (!base) return 0.0f;
    return base->cytokine_state.total_reactivity;
}

static float plasticity_compute_glutamate_clearance(const astrocyte_immune_base_t* base) {
    const astro_plasticity_bridge_t* bridge = (const astro_plasticity_bridge_t*)base;
    if (!bridge) return 1.0f;
    return bridge->glu_uptake_factor;
}

static int plasticity_release_reactive_cytokines(astrocyte_immune_base_t* base) {
    astro_plasticity_bridge_t* bridge = (astro_plasticity_bridge_t*)base;
    NIMCP_CHECK_THROW(bridge && base->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge or immune_system is NULL");
    if (!base->enable_reactive_cytokines) return NIMCP_SUCCESS;

    float reactivity = base->cytokine_state.total_reactivity;
    if (reactivity < 0.4f) return 0; /* Not reactive enough */

    /* Would release cytokines to immune system here */
    base->stats.cytokine_releases++;
    return 0;
}

static astrocyte_phenotype_t plasticity_get_phenotype(const astrocyte_immune_base_t* base) {
    const astro_plasticity_bridge_t* bridge = (const astro_plasticity_bridge_t*)base;
    if (!bridge) return ASTROCYTE_PHENOTYPE_RESTING;

    /* Map from plasticity module's reactive state */
    switch (bridge->current_phenotype) {
        case ASTROCYTE_A1_REACTIVE: return ASTROCYTE_PHENOTYPE_A1_REACTIVE;
        case ASTROCYTE_A2_REACTIVE: return ASTROCYTE_PHENOTYPE_A2_REACTIVE;
        default: return ASTROCYTE_PHENOTYPE_RESTING;
    }
}

static int plasticity_update(astrocyte_immune_base_t* base, uint64_t delta_ms) {
    astro_plasticity_bridge_t* bridge = (astro_plasticity_bridge_t*)base;
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    (void)delta_ms;

    /* Apply immune -> astrocyte */
    plasticity_apply_cytokine_effects(base);
    plasticity_apply_inflammation_effects(base);

    /* Transition reactive states for all astrocytes */
    if (bridge->astrocyte_system && bridge->config.enable_reactive_state_control) {
        uint32_t num = astrocyte_plasticity_get_num_astrocytes(bridge->astrocyte_system);
        for (uint32_t i = 0; i < num; i++) {
            astro_plasticity_transition_state(bridge, i);
        }
    }

    /* Detect dysfunction */
    if (bridge->config.enable_dysfunction_detection) {
        /* Query astrocytes for average state */
        if (bridge->astrocyte_system) {
            uint32_t num = astrocyte_plasticity_get_num_astrocytes(bridge->astrocyte_system);
            float total_glu = 0, total_d = 0, total_ca = 0;
            uint32_t count = 0;

            for (uint32_t i = 0; i < num; i++) {
                astrocyte_state_t state;
                if (astrocyte_plasticity_get_state(bridge->astrocyte_system, i, &state) == 0) {
                    total_glu += state.glutamate_uptake_rate;
                    total_d += state.d_serine_level;
                    total_ca += state.calcium_wave_amplitude;
                    count++;
                }
            }

            if (count > 0) {
                bridge->dysfunction.current_glu_uptake = total_glu / count;
                bridge->dysfunction.current_d_serine = total_d / count;
                bridge->dysfunction.calcium_wave_frequency = total_ca / count;
            }
        }

        /* Check thresholds */
        bridge->dysfunction.glu_uptake_critical =
            bridge->dysfunction.current_glu_uptake < bridge->config.glu_uptake_critical_threshold;
        bridge->dysfunction.d_serine_depleted =
            bridge->dysfunction.current_d_serine < bridge->config.d_serine_critical_threshold;
        bridge->dysfunction.calcium_excessive =
            bridge->dysfunction.calcium_wave_frequency > bridge->config.ca_wave_excessive_threshold;
        bridge->dysfunction.excitotoxicity_risk =
            bridge->dysfunction.glu_uptake_critical && bridge->dysfunction.calcium_excessive;

        /* Compute severity */
        float severity = 0.0f;
        if (bridge->dysfunction.glu_uptake_critical) severity += 0.4f;
        if (bridge->dysfunction.d_serine_depleted) severity += 0.3f;
        if (bridge->dysfunction.calcium_excessive) severity += 0.2f;
        if (bridge->dysfunction.excitotoxicity_risk) severity += 0.3f;
        bridge->dysfunction.dysfunction_severity = nimcp_clampf(severity, 0.0f, 1.0f);

        /* Alert if severe */
        if (bridge->dysfunction.dysfunction_severity >= 0.5f) {
            uint32_t antigen_id;
            astro_plasticity_alert_dysfunction(bridge, &antigen_id);
        }
    }

    /* Apply astrocyte -> immune */
    plasticity_release_reactive_cytokines(base);

    base->stats.total_updates++;
    return 0;
}

/* ============================================================================
 * Virtual Function Table
 * ============================================================================ */

static const astrocyte_immune_ops_t plasticity_ops = {
    .destroy = plasticity_destroy,
    .update = plasticity_update,
    .apply_cytokine_effects = plasticity_apply_cytokine_effects,
    .apply_inflammation_effects = plasticity_apply_inflammation_effects,
    .compute_reactivity = plasticity_compute_reactivity,
    .compute_glutamate_clearance = plasticity_compute_glutamate_clearance,
    .release_reactive_cytokines = plasticity_release_reactive_cytokines,
    .get_phenotype = plasticity_get_phenotype,
    .extension = NULL
};

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int astro_plasticity_default_config(astro_plasticity_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->enable_cytokine_modulation = true;
    config->enable_inflammation_effects = true;
    config->enable_dysfunction_detection = true;
    config->enable_reactive_state_control = true;

    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->dysfunction_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->glu_uptake_critical_threshold = ASTROCYTE_GLU_UPTAKE_CRITICAL_THRESHOLD;
    config->d_serine_critical_threshold = ASTROCYTE_D_SERINE_CRITICAL_THRESHOLD;
    config->ca_wave_excessive_threshold = ASTROCYTE_CA_WAVE_EXCESSIVE_THRESHOLD;

    return 0;
}

astro_plasticity_bridge_t* astro_plasticity_bridge_create(
    const astro_plasticity_config_t* config,
    brain_immune_system_t* immune_system,
    astrocyte_plasticity_t astrocyte_system
) {
    if (!immune_system || !astrocyte_system) {
        NIMCP_LOGGING_ERROR("astro_plasticity_bridge_create: NULL required parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_plasticity_bridge_create: required parameter is NULL (immune_system, astrocyte_system)");
        return NULL;
    }

    /* Allocate derived struct */
    astro_plasticity_bridge_t* bridge = nimcp_malloc(sizeof(astro_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("astro_plasticity_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(astro_plasticity_bridge_t));

    /* Initialize base */
    if (astro_immune_base_init(&bridge->base, ASTRO_IMMUNE_TYPE_PLASTICITY,
                               &plasticity_ops, immune_system) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_plasticity_bridge_create: operation failed");
        return NULL;
    }

    /* Store plasticity-specific references */
    bridge->astrocyte_system = astrocyte_system;

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        astro_plasticity_default_config(&bridge->config);
    }

    /* Initialize plasticity-specific state */
    bridge->d_serine_factor = 1.0f;
    bridge->glu_uptake_factor = 1.0f;
    bridge->current_phenotype = ASTROCYTE_RESTING;

    NIMCP_LOGGING_INFO("Created plasticity astrocyte-immune bridge");
    return bridge;
}

void astro_plasticity_bridge_destroy(astro_plasticity_bridge_t* bridge) {
    if (bridge) {
        plasticity_destroy(&bridge->base);
    }
}

/* ============================================================================
 * Plasticity-Specific API
 * ============================================================================ */

float astro_plasticity_get_d_serine_factor(const astro_plasticity_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->d_serine_factor;
}

float astro_plasticity_get_glu_uptake_factor(const astro_plasticity_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->glu_uptake_factor;
}

int astro_plasticity_get_dysfunction(
    const astro_plasticity_bridge_t* bridge,
    plasticity_dysfunction_state_t* out_state
) {
    NIMCP_CHECK_THROW(bridge && out_state, NIMCP_ERROR_NULL_POINTER, "bridge or out_state is NULL");

    nimcp_platform_mutex_lock(bridge->base.infra.mutex);
    memcpy(out_state, &bridge->dysfunction, sizeof(plasticity_dysfunction_state_t));
    nimcp_platform_mutex_unlock(bridge->base.infra.mutex);

    return 0;
}

bool astro_plasticity_is_impaired(const astro_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->d_serine_factor < 1.0f || bridge->glu_uptake_factor < 1.0f;
}

int astro_plasticity_transition_state(
    astro_plasticity_bridge_t* bridge,
    uint32_t astrocyte_id
) {
    NIMCP_CHECK_THROW(bridge && bridge->astrocyte_system, NIMCP_ERROR_NULL_POINTER, "bridge or astrocyte_system is NULL");
    if (!bridge->config.enable_reactive_state_control) return NIMCP_SUCCESS;

    nimcp_platform_mutex_lock(bridge->base.infra.mutex);

    /* Determine target state based on reactivity */
    astrocyte_reactive_state_t target = ASTROCYTE_RESTING;
    float intensity = bridge->base.cytokine_state.total_reactivity;

    if (bridge->base.inflammation.level >= INFLAMMATION_REGIONAL) {
        target = ASTROCYTE_A1_REACTIVE;
    } else if (bridge->base.cytokine_state.il10_effect > 0.5f) {
        target = ASTROCYTE_A2_REACTIVE;
    }

    int result = astrocyte_plasticity_set_reactive_state(
        bridge->astrocyte_system, astrocyte_id, target, intensity);

    if (result == 0) {
        bridge->reactive_state_transitions++;
    }

    nimcp_platform_mutex_unlock(bridge->base.infra.mutex);
    return result;
}

int astro_plasticity_alert_dysfunction(
    astro_plasticity_bridge_t* bridge,
    uint32_t* antigen_id
) {
    NIMCP_CHECK_THROW(bridge && antigen_id && bridge->base.immune_system, NIMCP_ERROR_NULL_POINTER, "bridge, antigen_id, or immune_system is NULL");

    if (bridge->dysfunction.dysfunction_severity < 0.5f) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "not significant"); /* Not significant */
    }

    /* Create epitope from dysfunction signature */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    epitope[0] = 0xBB; /* Astrocyte dysfunction marker */
    if (bridge->dysfunction.excitotoxicity_risk) {
        epitope[1] = 0x01;
    } else if (bridge->dysfunction.glu_uptake_critical) {
        epitope[1] = 0x02;
    } else if (bridge->dysfunction.d_serine_depleted) {
        epitope[1] = 0x03;
    }

    uint32_t severity = (uint32_t)(bridge->dysfunction.dysfunction_severity * 10.0f);
    severity = nimcp_clampf(severity, 1, 10);

    int result = brain_immune_present_antigen(
        bridge->base.immune_system,
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
