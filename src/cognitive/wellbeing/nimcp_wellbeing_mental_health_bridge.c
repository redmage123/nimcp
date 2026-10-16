/**
 * @file nimcp_wellbeing_mental_health_bridge.c
 * @brief Mental Health-Wellbeing Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of mental health-wellbeing bidirectional coupling
 * WHY:  Mental disorders impact wellbeing (anxiety, depression, stress)
 * HOW:  Query mental health state, compute effects on wellbeing metrics
 *
 * @author NIMCP Development Team
 */

#include "cognitive/wellbeing/nimcp_wellbeing_mental_health_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_mental_health_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

}


    if (instance_agent && instance_agent != g_wellbeing_mental_health_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with evidence-based defaults
 * HOW:  Return struct with biological parameters
 */
int mental_health_wellbeing_default_config(mental_health_wellbeing_config_t* config) {
    // Guard clause: check for NULL
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_wellbeing_default_config: config is NULL");
        return -1;
    }

    // Enable all features by default
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_mental_health_wellbe", 0.0f);


    config->enable_disorder_effects = true;
    config->enable_anxiety_modulation = true;
    config->enable_depression_modulation = true;
    config->enable_stress_tracking = true;

    // Default sensitivity multipliers (1.0 = normal sensitivity)
    config->anxiety_sensitivity = 1.0f;
    config->depression_sensitivity = 1.0f;
    config->stress_sensitivity = 1.0f;
    config->disorder_sensitivity = 1.0f;

    // Default thresholds
    config->anxiety_distress_threshold = 0.3f;
    config->depression_anhedonia_threshold = DEPRESSION_ANHEDONIA_THRESHOLD;

    return 0;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute distress contribution from disorder
 *
 * WHAT: Map disorder severity to distress contribution
 * WHY:  Different severities have different wellbeing impacts
 * HOW:  Use evidence-based severity mapping
 */
float compute_disorder_distress(disorder_type_t type, disorder_severity_t severity) {
    // Map severity to distress contribution
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_compute_disorder_dis", 0.0f);


    switch (severity) {
        case DISORDER_SEVERITY_NONE:
            return DISORDER_DISTRESS_NONE;
        case DISORDER_SEVERITY_MILD:
            return DISORDER_DISTRESS_MILD;
        case DISORDER_SEVERITY_MODERATE:
            return DISORDER_DISTRESS_MODERATE;
        case DISORDER_SEVERITY_SEVERE:
            return DISORDER_DISTRESS_SEVERE;
        case DISORDER_SEVERITY_CRITICAL:
            return DISORDER_DISTRESS_CRITICAL;
        default:
            return DISORDER_DISTRESS_NONE;
    }
}

/**
 * @brief Compute anxiety distress amplification
 *
 * WHAT: Calculate how anxiety amplifies perceived distress
 * WHY:  Anxiety makes all distress feel worse
 * HOW:  Map anxiety level to amplification factor [1.0-1.5]
 */
float compute_anxiety_amplification(float anxiety_level, float sensitivity) {
    // Clamp anxiety level to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_compute_anxiety_ampl", 0.0f);


    if (anxiety_level < 0.0f) anxiety_level = 0.0f;
    if (anxiety_level > 1.0f) anxiety_level = 1.0f;

    // Clamp sensitivity to [0.5, 2.0]
    if (sensitivity < 0.5f) sensitivity = 0.5f;
    if (sensitivity > 2.0f) sensitivity = 2.0f;

    // Linear interpolation between min and max amplification
    float range = ANXIETY_DISTRESS_MAX_AMPLIFICATION - ANXIETY_DISTRESS_MIN_AMPLIFICATION;
    float amplification = ANXIETY_DISTRESS_MIN_AMPLIFICATION + (anxiety_level * range);

    // Apply sensitivity multiplier
    amplification = 1.0f + ((amplification - 1.0f) * sensitivity);

    return amplification;
}

/**
 * @brief Compute depression flourishing suppression
 *
 * WHAT: Calculate how depression suppresses flourishing
 * WHY:  Depression reduces positive affect and meaning
 * HOW:  Map depression level to suppression factor [0-1]
 */
float compute_depression_suppression(float depression_level, float sensitivity) {
    // Clamp depression level to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_compute_depression_s", 0.0f);


    if (depression_level < 0.0f) depression_level = 0.0f;
    if (depression_level > 1.0f) depression_level = 1.0f;

    // Clamp sensitivity to [0.5, 2.0]
    if (sensitivity < 0.5f) sensitivity = 0.5f;
    if (sensitivity > 2.0f) sensitivity = 2.0f;

    // Linear interpolation between min and max suppression
    float range = DEPRESSION_FLOURISHING_MAX_SUPPRESSION - DEPRESSION_FLOURISHING_MIN_SUPPRESSION;
    float suppression = DEPRESSION_FLOURISHING_MIN_SUPPRESSION + (depression_level * range);

    // Apply sensitivity multiplier
    suppression *= sensitivity;

    // Clamp to [0, 1]
    if (suppression > 1.0f) suppression = 1.0f;

    return suppression;
}

/**
 * @brief Compute stress resilience
 *
 * WHAT: Calculate current stress resilience
 * WHY:  Chronic stress reduces ability to cope with distress
 * HOW:  Exponential decay based on chronic stress accumulation
 */
float compute_stress_resilience(float chronic_stress, float base_resilience) {
    // Clamp chronic stress to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_compute_stress_resil", 0.0f);


    if (chronic_stress < 0.0f) chronic_stress = 0.0f;
    if (chronic_stress > 1.0f) chronic_stress = 1.0f;

    // Clamp base resilience to [0, 1]
    if (base_resilience < 0.0f) base_resilience = 0.0f;
    if (base_resilience > 1.0f) base_resilience = 1.0f;

    // Exponential decay: resilience = base * exp(-stress)
    // This models how chronic stress progressively erodes resilience
    float resilience = base_resilience * expf(-chronic_stress * 2.0f);

    // Clamp to minimum resilience
    if (resilience < STRESS_RESILIENCE_MIN) {
        resilience = STRESS_RESILIENCE_MIN;
    }

    return resilience;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute anxiety effects on wellbeing
 *
 * WHAT: Calculate anxiety amplification effects
 * WHY:  Anxiety amplifies distress perception
 * HOW:  Check threshold, compute amplification factor
 */
static void compute_anxiety_effects_internal(
    mental_health_wellbeing_effects_t* effects,
    const mental_health_report_t* report,
    const mental_health_wellbeing_config_t* config
) {
    effects->anxiety_level = report->disorder_scores[DISORDER_ANXIETY];

    if (effects->anxiety_level > config->anxiety_distress_threshold) {
        effects->anxiety_distress_amplification =
            compute_anxiety_amplification(effects->anxiety_level, config->anxiety_sensitivity);
    } else {
        effects->anxiety_distress_amplification = 1.0f;
    }
}

/**
 * @brief Compute depression effects on wellbeing
 *
 * WHAT: Calculate depression suppression and anhedonia
 * WHY:  Depression suppresses flourishing
 * HOW:  Compute suppression factor, scale anhedonia
 */
static void compute_depression_effects_internal(
    mental_health_wellbeing_effects_t* effects,
    const mental_health_report_t* report,
    const mental_health_wellbeing_config_t* config
) {
    effects->depression_level = report->disorder_scores[DISORDER_DEPRESSION];

    effects->flourishing_suppression =
        compute_depression_suppression(effects->depression_level, config->depression_sensitivity);

    if (effects->depression_level > config->depression_anhedonia_threshold) {
        effects->anhedonia_level = effects->depression_level - config->depression_anhedonia_threshold;
        effects->anhedonia_level /= (1.0f - config->depression_anhedonia_threshold);
    } else {
        effects->anhedonia_level = 0.0f;
    }
}

/**
 * @brief Update chronic stress accumulation
 *
 * WHAT: Track and decay chronic stress over time
 * WHY:  Chronic stress reduces resilience
 * HOW:  Accumulate when distressed, decay when not
 */
static void update_chronic_stress_internal(
    mental_health_wellbeing_effects_t* effects
) {
    float current_distress = effects->disorder_distress_contribution;

    if (current_distress > 0.5f) {
        float stress_increment = (current_distress - 0.5f) * 0.01f;
        effects->chronic_stress_accumulation += stress_increment;
    } else {
        effects->chronic_stress_accumulation *= CHRONIC_STRESS_DECAY_RATE;
    }

    // Clamp to [0, 1]
    if (effects->chronic_stress_accumulation < 0.0f) {
        effects->chronic_stress_accumulation = 0.0f;
    }
    if (effects->chronic_stress_accumulation > 1.0f) {
        effects->chronic_stress_accumulation = 1.0f;
    }

    effects->stress_resilience =
        compute_stress_resilience(effects->chronic_stress_accumulation, STRESS_RESILIENCE_BASE);
}

/**
 * @brief Compute aggregate mental health effects
 *
 * WHAT: Combine all effects into total and recovery potential
 * WHY:  Provide unified metrics for wellbeing system
 * HOW:  Weighted sum of disorder, anxiety, depression
 */
static void compute_aggregate_effects_internal(
    mental_health_wellbeing_effects_t* effects
) {
    // Total effect (positive = negative impact on wellbeing)
    effects->total_mental_health_effect =
        effects->disorder_distress_contribution +
        (effects->anxiety_distress_amplification - 1.0f) * 0.3f +
        effects->flourishing_suppression * 0.5f;

    // Clamp to [-1, 1]
    if (effects->total_mental_health_effect < -1.0f) {
        effects->total_mental_health_effect = -1.0f;
    }
    if (effects->total_mental_health_effect > 1.0f) {
        effects->total_mental_health_effect = 1.0f;
    }

    // Recovery potential (inversely related to distress and stress)
    effects->recovery_potential = 1.0f -
        (effects->disorder_distress_contribution * 0.5f +
         effects->chronic_stress_accumulation * 0.3f +
         effects->depression_level * 0.2f);

    // Clamp to [0, 1]
    if (effects->recovery_potential < 0.0f) {
        effects->recovery_potential = 0.0f;
    }
}

/* ============================================================================
 * Mental Health → Wellbeing API
 * ============================================================================ */

/**
 * @brief Update mental health effects on wellbeing
 *
 * WHAT: Query mental health state and compute wellbeing effects
 * WHY:  Mental health disorders directly impact wellbeing metrics
 * HOW:  Get disorder report, compute anxiety/depression/stress effects
 */
int enhanced_wellbeing_update_mental_health(
    mental_health_monitor_t* mental_health,
    mental_health_wellbeing_effects_t* effects,
    const mental_health_wellbeing_config_t* config
) {
    // Guard clause: check for NULL pointers
    if (!mental_health || !effects) {
        NIMCP_LOGGING_ERROR("NULL pointer(s)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_update_mental_health: required parameter is NULL (mental_health, effects)");
        return -1;
    }

    // Use default config if not provided
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_enhanced_wellbeing_u", 0.0f);


    mental_health_wellbeing_config_t default_config;
    if (!config) {
        mental_health_wellbeing_default_config(&default_config);
        config = &default_config;
    }

    // Initialize and get report
    memset(effects, 0, sizeof(mental_health_wellbeing_effects_t));
    mental_health_report_t report;
    mental_health_get_report(mental_health, &report);

    // Store primary disorder info
    effects->primary_disorder = report.primary_disorder;
    effects->primary_severity = report.primary_severity;

    // Compute disorder distress contribution
    if (config->enable_disorder_effects) {
        effects->disorder_distress_contribution =
            compute_disorder_distress(report.primary_disorder, report.primary_severity) *
            config->disorder_sensitivity;
    }

    // Compute anxiety effects
    if (config->enable_anxiety_modulation) {
        compute_anxiety_effects_internal(effects, &report, config);
    } else {
        effects->anxiety_distress_amplification = 1.0f;
    }

    // Compute depression effects
    if (config->enable_depression_modulation) {
        compute_depression_effects_internal(effects, &report, config);
    }

    // Update chronic stress
    if (config->enable_stress_tracking) {
        update_chronic_stress_internal(effects);
    } else {
        effects->stress_resilience = STRESS_RESILIENCE_BASE;
    }

    // Compute aggregate effects
    compute_aggregate_effects_internal(effects);

    return 0;
}

/**
 * @brief Get current mental health effects
 *
 * WHAT: Retrieve current mental health-wellbeing effects
 * WHY:  Allow wellbeing system to query effects state
 * HOW:  Copy effects structure
 */
int enhanced_wellbeing_get_mental_health_effects(
    const mental_health_wellbeing_effects_t* effects,
    mental_health_wellbeing_effects_t* effects_out
) {
    // Guard clause: check for NULL pointers
    if (!effects) {
        NIMCP_LOGGING_ERROR("NULL effects pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_get_mental_health_effects: effects is NULL");
        return -1;
    }
    if (!effects_out) {
        NIMCP_LOGGING_ERROR("NULL effects_out pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_get_mental_health_effects: effects_out is NULL");
        return -1;
    }

    // Copy effects structure
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_enhanced_wellbeing_g", 0.0f);


    memcpy(effects_out, effects, sizeof(mental_health_wellbeing_effects_t));

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Mental Health Wellbeing Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int mental_health_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_mental_health_bridge_heartbeat("wellbeing_me_mental_health_bridge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mental_Health_Wellbeing_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_mental_health_bridge_heartbeat("wellbeing_me_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mental Health Wellbeing Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mental_Health_Wellbeing_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mental_Health_Wellbeing_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


void wellbeing_mental_health_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_mental_health_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_mental_health_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_mental_health_bridge_training_begin: NULL argument");
        return -1;
    }
    wellbeing_mental_health_bridge_heartbeat_instance(g_wellbeing_mental_health_bridge_health_agent, "wellbeing_mental_health_bridge_training_begin", 0.0f);
    return 0;
}

int wellbeing_mental_health_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_mental_health_bridge_training_end: NULL argument");
        return -1;
    }
    wellbeing_mental_health_bridge_heartbeat_instance(g_wellbeing_mental_health_bridge_health_agent, "wellbeing_mental_health_bridge_training_end", 1.0f);
    return 0;
}

int wellbeing_mental_health_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_mental_health_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_mental_health_bridge_heartbeat_instance(g_wellbeing_mental_health_bridge_health_agent, "wellbeing_mental_health_bridge_training_step", progress);
    return 0;
}
