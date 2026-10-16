/**
 * @file nimcp_astrocyte_immune_plasticity.h
 * @brief Plasticity-Focused Astrocyte-Immune Bridge (Derived Type)
 * @version 2.0.0
 * @date 2025-12-27
 *
 * WHAT: Astrocyte-immune bridge for synaptic plasticity modulation
 * WHY:  Cytokines affect D-serine, glutamate uptake -> NMDA-dependent plasticity
 * HOW:  Inherits from astrocyte_immune_base_t, works with astrocyte_plasticity_t
 *
 * BIOLOGICAL BASIS:
 * - IL-1β reduces D-serine by 50%, impairing LTP
 * - TNF-α impairs glutamate uptake by 60%, risking excitotoxicity
 * - IL-10 restores normal astrocyte function
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTE_IMMUNE_PLASTICITY_H
#define NIMCP_ASTROCYTE_IMMUNE_PLASTICITY_H

#include "glial/immune/nimcp_astrocyte_immune_base.h"
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine D-serine modulation factors */
#define CYTOKINE_IL1_D_SERINE_REDUCTION      0.5f   /**< IL-1β → 50% D-serine */
#define CYTOKINE_IL6_D_SERINE_REDUCTION      0.85f  /**< IL-6 → 85% D-serine */
#define CYTOKINE_TNF_D_SERINE_REDUCTION      0.6f   /**< TNF-α → 60% D-serine */
#define CYTOKINE_IFN_GAMMA_D_SERINE_REDUCTION 0.75f /**< IFN-γ → 75% D-serine */
#define CYTOKINE_IL10_D_SERINE_RESTORATION   1.1f   /**< IL-10 → 110% D-serine */

/* Cytokine glutamate uptake modulation */
#define CYTOKINE_IL1_GLU_UPTAKE_IMPAIRMENT   0.7f   /**< IL-1β → 70% uptake */
#define CYTOKINE_IL6_GLU_UPTAKE_IMPAIRMENT   0.9f   /**< IL-6 → 90% uptake */
#define CYTOKINE_TNF_GLU_UPTAKE_IMPAIRMENT   0.4f   /**< TNF-α → 40% uptake */
#define CYTOKINE_IFN_GAMMA_GLU_UPTAKE_IMPAIRMENT 0.6f /**< IFN-γ → 60% uptake */
#define CYTOKINE_IL10_GLU_UPTAKE_RESTORATION 1.15f  /**< IL-10 → 115% uptake */

/* Dysfunction thresholds */
#define ASTROCYTE_GLU_UPTAKE_CRITICAL_THRESHOLD 0.5f /**< Below = excitotoxicity risk */
#define ASTROCYTE_D_SERINE_CRITICAL_THRESHOLD   0.4f /**< Below = LTP impairment */
#define ASTROCYTE_CA_WAVE_EXCESSIVE_THRESHOLD   3.0f /**< Above = hyperactivity */

/* Inflammation D-serine factors by level */
#define INFLAMMATION_ASTRO_NONE_D_SERINE     1.0f
#define INFLAMMATION_ASTRO_LOCAL_D_SERINE    0.9f
#define INFLAMMATION_ASTRO_REGIONAL_D_SERINE 0.7f
#define INFLAMMATION_ASTRO_SYSTEMIC_D_SERINE 0.5f
#define INFLAMMATION_ASTRO_STORM_D_SERINE    0.3f

/* Inflammation glutamate uptake factors */
#define INFLAMMATION_ASTRO_NONE_GLU_UPTAKE     1.0f
#define INFLAMMATION_ASTRO_LOCAL_GLU_UPTAKE    0.9f
#define INFLAMMATION_ASTRO_REGIONAL_GLU_UPTAKE 0.7f
#define INFLAMMATION_ASTRO_SYSTEMIC_GLU_UPTAKE 0.5f
#define INFLAMMATION_ASTRO_STORM_GLU_UPTAKE    0.3f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Plasticity-specific dysfunction detection
 */
typedef struct {
    float current_glu_uptake;       /**< Average glutamate uptake [0-1] */
    float current_d_serine;         /**< Average D-serine level [0-1] */
    float calcium_wave_frequency;   /**< Calcium wave activity */
    bool glu_uptake_critical;       /**< Below critical threshold */
    bool d_serine_depleted;         /**< D-serine critically low */
    bool calcium_excessive;         /**< Hyperactive calcium */
    bool excitotoxicity_risk;       /**< Glutamate + calcium danger */
    float dysfunction_severity;     /**< Overall severity [0-1] */
} plasticity_dysfunction_state_t;

/**
 * @brief Configuration for plasticity bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_inflammation_effects;
    bool enable_dysfunction_detection;
    bool enable_reactive_state_control;

    /* Sensitivities */
    float cytokine_sensitivity;
    float inflammation_sensitivity;
    float dysfunction_sensitivity;

    /* Thresholds */
    float glu_uptake_critical_threshold;
    float d_serine_critical_threshold;
    float ca_wave_excessive_threshold;
} astro_plasticity_config_t;

/**
 * @brief Plasticity-focused astrocyte-immune bridge
 *
 * Works with astrocyte_plasticity_t to modulate synaptic function.
 */
typedef struct {
    /* Base class - MUST BE FIRST */
    astrocyte_immune_base_t base;

    /* Plasticity-specific state */
    astrocyte_plasticity_t astrocyte_system;  /**< Connected plasticity module */
    plasticity_dysfunction_state_t dysfunction; /**< Dysfunction detection */
    astro_plasticity_config_t config;          /**< Configuration */

    /* Plasticity-specific metrics */
    float d_serine_factor;              /**< Current D-serine modulation */
    float glu_uptake_factor;            /**< Current glutamate uptake factor */
    astrocyte_reactive_state_t current_phenotype; /**< From plasticity module */

    /* Statistics */
    uint32_t dysfunction_alerts;
    uint32_t reactive_state_transitions;
    uint32_t cytokine_modulations;
} astro_plasticity_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default plasticity bridge configuration
 */
int astro_plasticity_default_config(astro_plasticity_config_t* config);

/**
 * @brief Create plasticity-focused astrocyte-immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param astrocyte_system Astrocyte plasticity module
 * @return New bridge or NULL on failure
 */
astro_plasticity_bridge_t* astro_plasticity_bridge_create(
    const astro_plasticity_config_t* config,
    brain_immune_system_t* immune_system,
    astrocyte_plasticity_t astrocyte_system
);

/**
 * @brief Destroy plasticity bridge
 */
void astro_plasticity_bridge_destroy(astro_plasticity_bridge_t* bridge);

/* ============================================================================
 * Plasticity-Specific API
 * ============================================================================ */

/**
 * @brief Get current D-serine modulation factor
 * @return D-serine factor (1.0 = normal, <1.0 = reduced)
 */
float astro_plasticity_get_d_serine_factor(const astro_plasticity_bridge_t* bridge);

/**
 * @brief Get current glutamate uptake factor
 * @return Uptake factor (1.0 = normal, <1.0 = impaired)
 */
float astro_plasticity_get_glu_uptake_factor(const astro_plasticity_bridge_t* bridge);

/**
 * @brief Get dysfunction state
 */
int astro_plasticity_get_dysfunction(
    const astro_plasticity_bridge_t* bridge,
    plasticity_dysfunction_state_t* out_state
);

/**
 * @brief Check if plasticity is impaired
 */
bool astro_plasticity_is_impaired(const astro_plasticity_bridge_t* bridge);

/**
 * @brief Transition astrocyte to reactive state
 * @param astrocyte_id Which astrocyte to transition
 */
int astro_plasticity_transition_state(
    astro_plasticity_bridge_t* bridge,
    uint32_t astrocyte_id
);

/**
 * @brief Detect dysfunction and alert immune system
 * @param antigen_id Output: ID of created antigen (if any)
 */
int astro_plasticity_alert_dysfunction(
    astro_plasticity_bridge_t* bridge,
    uint32_t* antigen_id
);

/* ============================================================================
 * Polymorphic Cast (safe upcast to base)
 * ============================================================================ */

/**
 * @brief Cast plasticity bridge to base pointer
 */
static inline astrocyte_immune_base_t* astro_plasticity_to_base(
    astro_plasticity_bridge_t* bridge
) {
    return bridge ? &bridge->base : NULL;
}

/**
 * @brief Cast base pointer to plasticity bridge (with type check)
 */
static inline astro_plasticity_bridge_t* astro_plasticity_from_base(
    astrocyte_immune_base_t* base
) {
    if (!base || base->type != ASTRO_IMMUNE_TYPE_PLASTICITY) return NULL;
    return (astro_plasticity_bridge_t*)base;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTE_IMMUNE_PLASTICITY_H */
