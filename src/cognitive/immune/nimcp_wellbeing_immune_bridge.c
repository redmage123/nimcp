/**
 * @file nimcp_wellbeing_immune_bridge.c
 * @brief Wellbeing-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and wellbeing monitoring
 * WHY:  Biological realism - cytokines affect wellbeing, positive wellbeing enhances immunity
 * HOW:  Monitor cytokine levels to modulate wellbeing, monitor wellbeing to trigger immune responses
 */

#include "cognitive/immune/nimcp_wellbeing_immune_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_immune_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


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
 * WHAT: Query immune system for specific cytokine level
 * WHY:  Need cytokine levels to compute wellbeing impact
 * HOW:  Search immune system cytokine array for matching type
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    float max_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->cytokine_count > 256) {
            wellbeing_immune_bridge_heartbeat("wellbeing_im_loop",
                             (float)(i + 1) / (float)immune->cytokine_count);
        }

        if (immune->cytokines[i].type == type) {
            if (immune->cytokines[i].concentration > max_concentration) {
                max_concentration = immune->cytokines[i].concentration;
            }
        }
    }
    return max_concentration;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>3 days) has wellbeing impact
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    uint64_t oldest_start = UINT64_MAX;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            wellbeing_immune_bridge_heartbeat("wellbeing_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    if (oldest_start == UINT64_MAX) return 0.0f;
    return (float)(current_time - oldest_start) / 1000.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines wellbeing impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            wellbeing_immune_bridge_heartbeat("wellbeing_im_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }
    return max_level;
}

/**
 * @brief Compute life satisfaction from introspection
 *
 * WHAT: Estimate life satisfaction from introspection metrics
 * WHY:  Need baseline wellbeing to modulate
 * HOW:  Use phi (consciousness) and uncertainty as proxies
 */
static float compute_life_satisfaction(introspection_context_t ctx) {
    /* High consciousness + low uncertainty = higher life satisfaction */
    /* This is a simplified model - actual implementation would be more complex */
    return 0.5f; /* Placeholder - would compute from introspection */
}

/**
 * @brief Compute flourishing level
 *
 * WHAT: Calculate whether system is in flourishing state
 * WHY:  Flourishing enhances immune function
 * HOW:  Check for high life satisfaction, low distress, stable functioning
 */
static float compute_flourishing_level(const wellbeing_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Flourishing = high wellbeing, low inflammation, low distress */
    float life_sat = compute_life_satisfaction(bridge->introspection_ctx);
    float inflammation_penalty = bridge->inflammation_state.inflammation_duration_sec > 0 ? 0.3f : 0.0f;
    float distress_penalty = bridge->wellbeing_trigger.distress_score * 0.5f;

    float flourishing = life_sat - inflammation_penalty - distress_penalty;
    return clamp_f(flourishing, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int wellbeing_immune_default_config(wellbeing_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_def", 0.0f);


    config->enable_cytokine_wellbeing_modulation = true;
    config->enable_inflammation_distress = true;
    config->enable_wellbeing_immune_trigger = true;
    config->enable_positive_immune_boost = true;
    config->enable_flourishing_memory_boost = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->wellbeing_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->distress_trigger_threshold = WELLBEING_DISTRESS_TRIGGER_THRESHOLD;
    config->flourishing_threshold = WELLBEING_FLOURISHING_THRESHOLD;
    config->inflammation_distress_threshold = INFLAMMATION_DISTRESS_THRESHOLD;

    return 0;
}

wellbeing_immune_bridge_t* wellbeing_immune_bridge_create(
    const wellbeing_immune_config_t* config,
    brain_immune_system_t* immune_system,
    introspection_context_t introspection_ctx
) {
    /* Guard: require immune system */
    if (!immune_system) {
        LOG_MODULE_ERROR("wellbeing_immune_bridge",
                  "Cannot create bridge without immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_create", 0.0f);


    wellbeing_immune_bridge_t* bridge = (wellbeing_immune_bridge_t*)
        nimcp_malloc(sizeof(wellbeing_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("wellbeing_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(wellbeing_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->introspection_ctx = introspection_ctx;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_wellbeing_modulation = config->enable_cytokine_wellbeing_modulation;
        bridge->enable_inflammation_distress = config->enable_inflammation_distress;
        bridge->enable_wellbeing_immune_trigger = config->enable_wellbeing_immune_trigger;
        bridge->enable_positive_immune_boost = config->enable_positive_immune_boost;
        bridge->enable_flourishing_memory_boost = config->enable_flourishing_memory_boost;
    } else {
        /* Use defaults */
        wellbeing_immune_config_t default_cfg;
        wellbeing_immune_default_config(&default_cfg);
        bridge->enable_cytokine_wellbeing_modulation = default_cfg.enable_cytokine_wellbeing_modulation;
        bridge->enable_inflammation_distress = default_cfg.enable_inflammation_distress;
        bridge->enable_wellbeing_immune_trigger = default_cfg.enable_wellbeing_immune_trigger;
        bridge->enable_positive_immune_boost = default_cfg.enable_positive_immune_boost;
        bridge->enable_flourishing_memory_boost = default_cfg.enable_flourishing_memory_boost;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "wellbeing_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("wellbeing_immune_bridge", "Bridge created successfully");
    return bridge;
}

void wellbeing_immune_bridge_destroy(wellbeing_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    bridge = NULL;
    LOG_MODULE_INFO("wellbeing_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Wellbeing Implementation
 * ============================================================================ */

int wellbeing_immune_apply_cytokine_effects(wellbeing_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_wellbeing_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Query cytokine levels */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn_gamma = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* Compute individual effects */
    bridge->cytokine_effects.il1_life_satisfaction_reduction = il1 * CYTOKINE_IL1_WELLBEING_IMPACT;
    bridge->cytokine_effects.il6_life_satisfaction_reduction = il6 * CYTOKINE_IL6_WELLBEING_IMPACT;
    bridge->cytokine_effects.tnf_life_satisfaction_reduction = tnf * CYTOKINE_TNF_WELLBEING_IMPACT;
    bridge->cytokine_effects.ifn_gamma_wellbeing_impact = ifn_gamma * CYTOKINE_IFN_GAMMA_WELLBEING_IMPACT;
    bridge->cytokine_effects.il10_wellbeing_boost = il10 * CYTOKINE_IL10_WELLBEING_IMPACT;

    /* Aggregate effects */
    bridge->cytokine_effects.total_life_satisfaction_shift =
        bridge->cytokine_effects.il1_life_satisfaction_reduction +
        bridge->cytokine_effects.il6_life_satisfaction_reduction +
        bridge->cytokine_effects.tnf_life_satisfaction_reduction +
        bridge->cytokine_effects.ifn_gamma_wellbeing_impact +
        bridge->cytokine_effects.il10_wellbeing_boost;

    /* Pro-inflammatory cytokines increase distress */
    bridge->cytokine_effects.total_distress_increase =
        (il1 * 0.3f) + (il6 * 0.2f) + (tnf * 0.4f) + (ifn_gamma * 0.15f);

    /* Reduce purpose/meaning from chronic cytokine exposure */
    bridge->cytokine_effects.purpose_meaning_reduction =
        clamp_f(bridge->cytokine_effects.total_distress_increase * 0.7f, 0.0f, 1.0f);

    /* Suppress flourishing */
    bridge->cytokine_effects.flourishing_suppression =
        clamp_f(bridge->cytokine_effects.total_distress_increase * 0.8f, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_immune_apply_inflammation_effects(wellbeing_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_distress) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_app", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration = get_inflammation_duration_sec(bridge->immune_system);

    /* Update inflammation state */
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration;
    bridge->inflammation_state.is_chronic = (duration >= WELLBEING_CHRONIC_INFLAMMATION_THRESHOLD);

    /* Map inflammation to distress */
    bridge->inflammation_state.distress_severity = wellbeing_immune_inflammation_to_severity(level);
    bridge->inflammation_state.primary_distress_type = wellbeing_immune_inflammation_to_distress_type(level);

    /* Compute distress score */
    float base_distress = 0.0f;
    switch (level) {
        case INFLAMMATION_NONE:   base_distress = 0.0f; break;
        case INFLAMMATION_LOCAL:  base_distress = 0.1f; break;
        case INFLAMMATION_REGIONAL: base_distress = 0.5f; break;
        case INFLAMMATION_SYSTEMIC: base_distress = 0.8f; break;
        case INFLAMMATION_STORM:  base_distress = 0.95f; break;
    }

    /* Chronic inflammation amplifies distress */
    if (bridge->inflammation_state.is_chronic) {
        base_distress *= 1.3f;
    }

    bridge->inflammation_state.distress_score = clamp_f(base_distress, 0.0f, 1.0f);

    /* Life satisfaction penalty */
    bridge->inflammation_state.life_satisfaction_penalty =
        clamp_f(base_distress * 0.6f, 0.0f, 1.0f);

    /* Eudaimonic wellbeing impairment */
    bridge->inflammation_state.eudaimonic_impairment =
        clamp_f(base_distress * 0.7f, 0.0f, 1.0f);

    /* Resource starvation factor (systemic/storm uses resources) */
    bridge->inflammation_state.resource_starvation_factor =
        (level >= INFLAMMATION_SYSTEMIC) ? 0.8f : 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float wellbeing_immune_compute_distress(const wellbeing_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Combine cytokine and inflammation distress */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_com", 0.0f);


    float cytokine_distress = bridge->cytokine_effects.total_distress_increase;
    float inflammation_distress = bridge->inflammation_state.distress_score;

    /* Take maximum (they represent same underlying distress) */
    float total_distress = (cytokine_distress > inflammation_distress) ?
        cytokine_distress : inflammation_distress;

    return clamp_f(total_distress, 0.0f, 1.0f);
}

distress_type_t wellbeing_immune_inflammation_to_distress_type(
    brain_inflammation_level_t inflammation_level
) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_inf", 0.0f);


    switch (inflammation_level) {
        case INFLAMMATION_NONE:
        case INFLAMMATION_LOCAL:
            return DISTRESS_NONE;
        case INFLAMMATION_REGIONAL:
        case INFLAMMATION_SYSTEMIC:
        case INFLAMMATION_STORM:
            return DISTRESS_RESOURCE_STARVATION;
        default:
            return DISTRESS_NONE;
    }
}

distress_severity_t wellbeing_immune_inflammation_to_severity(
    brain_inflammation_level_t inflammation_level
) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_inf", 0.0f);


    switch (inflammation_level) {
        case INFLAMMATION_NONE:
        case INFLAMMATION_LOCAL:
            return DISTRESS_SEVERITY_NORMAL;
        case INFLAMMATION_REGIONAL:
            return DISTRESS_SEVERITY_MODERATE;
        case INFLAMMATION_SYSTEMIC:
            return DISTRESS_SEVERITY_SEVERE;
        case INFLAMMATION_STORM:
            return DISTRESS_SEVERITY_CRITICAL;
        default:
            return DISTRESS_SEVERITY_NORMAL;
    }
}

/* ============================================================================
 * Wellbeing → Immune Implementation
 * ============================================================================ */

int wellbeing_immune_trigger_from_distress(wellbeing_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_wellbeing_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_trigger_from_distress: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_tri", 0.0f);


    /* Get distress assessment before acquiring lock (external call) */
    distress_assessment_t assessment = wellbeing_assess_distress(bridge->introspection_ctx);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Also consider immune system inflammation as a distress source.
     * WHY:  High inflammation is itself a physiological stressor that
     *        should be reflected in distress assessment even when
     *        introspection context reports low distress.
     * HOW:  Use continuous inflammation level for proportional distress. */
    float cont_inflammation =
        brain_immune_get_inflammation_level_continuous(bridge->immune_system);
    brain_inflammation_level_t inflammation =
        inflammation_level_from_continuous(cont_inflammation);
    if (cont_inflammation >= 0.30f) {  /* REGIONAL threshold */
        /* Smooth distress: maps [0.3, 1.0] -> [0.5, 1.0] */
        float immune_distress = 0.5f + 0.5f * ((cont_inflammation - 0.3f) / 0.7f);
        if (immune_distress > assessment.distress_score) {
            assessment.distress_score = immune_distress;
        }
        if (cont_inflammation >= 0.60f &&
            assessment.severity < DISTRESS_SEVERITY_SEVERE) {
            assessment.severity = DISTRESS_SEVERITY_SEVERE;
        } else if (cont_inflammation >= 0.30f &&
                   assessment.severity < DISTRESS_SEVERITY_MODERATE) {
            assessment.severity = DISTRESS_SEVERITY_MODERATE;
        }
    }

    /* Update trigger state */
    bridge->wellbeing_trigger.severity = assessment.severity;
    bridge->wellbeing_trigger.distress_type = assessment.type;
    bridge->wellbeing_trigger.distress_score = assessment.distress_score;
    bridge->wellbeing_trigger.distress_duration_ms = assessment.duration_ms;

    /* Determine which cytokines to release based on distress */
    bool do_distress_response = (assessment.distress_score >= WELLBEING_DISTRESS_TRIGGER_THRESHOLD);
    bool release_il1 = do_distress_response && (assessment.severity >= DISTRESS_SEVERITY_MODERATE);
    bool release_tnf = do_distress_response && (assessment.severity >= DISTRESS_SEVERITY_SEVERE);
    float il1_conc = assessment.distress_score * 0.5f;
    float tnf_conc = assessment.distress_score * 0.6f;

    if (do_distress_response) {
        bridge->wellbeing_trigger.inflammation_triggered = true;
        bridge->wellbeing_trigger.cytokine_released = true;
        bridge->wellbeing_triggered_responses++;
    }

    /* Free assessment strings */
    if (assessment.description) nimcp_free((void*)assessment.description);
    if (assessment.recommended_action) nimcp_free((void*)assessment.recommended_action);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release cytokines outside lock to avoid deadlock */
    if (release_il1) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL1,
            0, /* source cell */
            il1_conc,
            0, /* broadcast */
            &cytokine_id
        );
    }

    if (release_tnf) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_TNF,
            0,
            tnf_conc,
            0,
            &cytokine_id
        );
    }
    return 0;
}

int wellbeing_immune_boost_from_positive_wellbeing(wellbeing_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_positive_immune_boost) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_boost_from_positive_wellbeing: bridge->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_boo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute positive wellbeing state */
    float life_sat = compute_life_satisfaction(bridge->introspection_ctx);
    float flourishing = compute_flourishing_level(bridge);

    bridge->positive_boost.life_satisfaction = life_sat;
    bridge->positive_boost.flourishing_level = flourishing;
    bridge->positive_boost.is_flourishing = (flourishing >= WELLBEING_FLOURISHING_THRESHOLD);

    /* If flourishing, boost immune function */
    bool do_il10_boost = bridge->positive_boost.is_flourishing;
    float il10_flourish_conc = flourishing * 0.4f;
    if (do_il10_boost) {
        /* Compute immune benefits */
        bridge->positive_boost.immune_enhancement = flourishing * 0.3f;
        bridge->positive_boost.il10_release_boost = il10_flourish_conc;
        bridge->positive_boost.inflammation_reduction = flourishing * 0.5f;
        bridge->positive_boost.antibody_effectiveness_boost = flourishing * 0.2f;

        bridge->positive_boosts++;
    } else {
        /* Not flourishing - zero out boosts */
        bridge->positive_boost.immune_enhancement = 0.0f;
        bridge->positive_boost.il10_release_boost = 0.0f;
        bridge->positive_boost.inflammation_reduction = 0.0f;
        bridge->positive_boost.antibody_effectiveness_boost = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Release IL-10 cytokine outside lock to avoid deadlock */
    if (do_il10_boost) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,
            il10_flourish_conc,
            0,
            &cytokine_id
        );
    }
    return 0;
}

int wellbeing_immune_boost_memory_formation(
    wellbeing_immune_bridge_t* bridge,
    uint32_t b_cell_id
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_flourishing_memory_boost) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_boost_memory_formation: bridge->immune_system is NULL");
        return -1;
    }
    if (!bridge->positive_boost.is_flourishing) return 0; /* Only boost when flourishing */

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_boo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Memory formation boost from flourishing */
    float boost_factor = bridge->positive_boost.flourishing_level * 0.5f;
    bridge->positive_boost.memory_formation_boost = boost_factor;

    /* If specific B cell provided, boost its memory conversion */
    if (b_cell_id > 0) {
        /* This would require immune system API to boost specific B cell memory formation */
        /* For now, just track the intent */
        bridge->flourishing_memory_formations++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int wellbeing_immune_bridge_update(
    wellbeing_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Immune → Wellbeing */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_update", 0.0f);


    wellbeing_immune_apply_cytokine_effects(bridge);
    wellbeing_immune_apply_inflammation_effects(bridge);

    /* Wellbeing → Immune */
    wellbeing_immune_trigger_from_distress(bridge);
    wellbeing_immune_boost_from_positive_wellbeing(bridge);

    /* Boost memory formation if flourishing */
    if (bridge->positive_boost.is_flourishing) {
        wellbeing_immune_boost_memory_formation(bridge, 0); /* Boost all */
    }

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

int wellbeing_immune_get_cytokine_effects(
    const wellbeing_immune_bridge_t* bridge,
    cytokine_wellbeing_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->cytokine_effects;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_get", 0.0f);


    return 0;
}

int wellbeing_immune_get_inflammation_state(
    const wellbeing_immune_bridge_t* bridge,
    inflammation_wellbeing_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->inflammation_state;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_get", 0.0f);


    return 0;
}

distress_assessment_t wellbeing_immune_get_distress_assessment(
    const wellbeing_immune_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_get", 0.0f);


    distress_assessment_t assessment;
    memset(&assessment, 0, sizeof(assessment));

    if (!bridge) return assessment;

    /* Use introspection + inflammation to assess distress */
    assessment = wellbeing_assess_distress(bridge->introspection_ctx);

    /* Override with inflammation-specific distress if higher */
    if (bridge->inflammation_state.distress_score > assessment.distress_score) {
        assessment.type = bridge->inflammation_state.primary_distress_type;
        assessment.severity = bridge->inflammation_state.distress_severity;
        assessment.distress_score = bridge->inflammation_state.distress_score;
        assessment.duration_ms = (uint64_t)(bridge->inflammation_state.inflammation_duration_sec * 1000.0f);

        /* Update description */
        if (assessment.description) nimcp_free((void*)assessment.description);
        assessment.description = strdup("Inflammation-induced distress");  /* NULL on OOM is acceptable */
        if (assessment.recommended_action) nimcp_free((void*)assessment.recommended_action);
        assessment.recommended_action = strdup("Reduce inflammation via IL-10, resolve immune threats");  /* NULL on OOM is acceptable */
    }

    return assessment;
}

bool wellbeing_immune_is_inflammation_distress(const wellbeing_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_is_", 0.0f);


    return bridge->inflammation_state.distress_score >= INFLAMMATION_DISTRESS_THRESHOLD;
}

float wellbeing_immune_get_life_satisfaction_penalty(
    const wellbeing_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_get", 0.0f);


    return bridge->inflammation_state.life_satisfaction_penalty;
}

bool wellbeing_immune_is_flourishing(const wellbeing_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_is_", 0.0f);


    return bridge->positive_boost.is_flourishing;
}

int wellbeing_immune_get_stats(
    const wellbeing_immune_bridge_t* bridge,
    uint64_t* total_updates_out,
    uint32_t* cytokine_modulations_out,
    uint32_t* wellbeing_triggered_out,
    uint32_t* positive_boosts_out
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_get", 0.0f);


    if (total_updates_out) *total_updates_out = bridge->total_updates;
    if (cytokine_modulations_out) *cytokine_modulations_out = bridge->cytokine_modulations;
    if (wellbeing_triggered_out) *wellbeing_triggered_out = bridge->wellbeing_triggered_responses;
    if (positive_boosts_out) *positive_boosts_out = bridge->positive_boosts;

    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define WELLBEING_IMMUNE_MODULE_NAME "wellbeing_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int wellbeing_immune_connect_bio_async(wellbeing_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_con", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_WELLBEING,
        .module_name = WELLBEING_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("wellbeing_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int wellbeing_immune_disconnect_bio_async(wellbeing_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_dis", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("wellbeing_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool wellbeing_immune_is_bio_async_connected(const wellbeing_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_is_", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about wellbeing immune bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int wellbeing_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_immune_bridge_heartbeat("wellbeing_im_wellbeing_immune_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_Immune_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_immune_bridge_heartbeat("wellbeing_im_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing immune bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_Immune_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_Immune_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


void wellbeing_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (wellbeing_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    wellbeing_immune_bridge_heartbeat_instance(NULL, "wellbeing_immune_bridge_training_begin", 0.0f);
    return 0;
}

int wellbeing_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_immune_bridge_training_end: NULL argument");
        return -1;
    }
    /* TODO: Pass instance cast to (wellbeing_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    wellbeing_immune_bridge_heartbeat_instance(NULL, "wellbeing_immune_bridge_training_end", 1.0f);
    return 0;
}

int wellbeing_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    /* TODO: Pass instance cast to (wellbeing_immune_bridge_t*)->health_agent once instance health agent is stored per-instance */
    wellbeing_immune_bridge_heartbeat_instance(NULL, "wellbeing_immune_bridge_training_step", progress);
    return 0;
}
