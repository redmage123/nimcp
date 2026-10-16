/**
 * @file nimcp_wellbeing_sleep_bridge.c
 * @brief Sleep-Wellbeing Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of bidirectional sleep-wellbeing integration
 * WHY:  Sleep quality profoundly affects wellbeing; distress affects sleep need
 * HOW:  Query sleep state, compute wellbeing modulation, update effects
 *
 * @author NIMCP Development Team
 */

#include "cognitive/wellbeing/nimcp_wellbeing_sleep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_sleep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

}


    if (instance_agent && instance_agent != g_wellbeing_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Helper Function Prototypes
 * ============================================================================ */

static float sigmoid(float x);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * WHAT: Get default sleep-wellbeing bridge configuration
 * WHY:  Provide sensible defaults based on biological evidence
 * HOW:  Set evidence-based parameters from sleep research
 */
int sleep_wellbeing_default_config(sleep_wellbeing_bridge_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("sleep_wellbeing_default_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wellbeing_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_sleep_wellbeing_defa", 0.0f);


    config->enable_sleep_debt_effects = true;
    config->enable_rem_effects = true;
    config->enable_circadian_effects = true;
    config->enable_state_tolerance_mod = true;

    config->sleep_debt_threshold = SLEEP_WELLBEING_DEBT_THRESHOLD_DEFAULT;
    config->sleep_debt_sensitivity = SLEEP_WELLBEING_DEBT_SENSITIVITY_DEFAULT;
    config->rem_sensitivity = SLEEP_WELLBEING_REM_SENSITIVITY_DEFAULT;
    config->circadian_sensitivity = SLEEP_WELLBEING_CIRCADIAN_SENSITIVITY;
    config->circadian_max_deviation_hours = SLEEP_WELLBEING_CIRCADIAN_MAX_DEVIATION;

    return 0;
}

/**
 * WHAT: Create sleep-wellbeing bridge
 * WHY:  Initialize integration between sleep and wellbeing systems
 * HOW:  Allocate structure, store sleep system reference, create mutex
 */
sleep_wellbeing_bridge_t* sleep_wellbeing_bridge_create(
    const sleep_wellbeing_bridge_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("sleep_wellbeing_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_system is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_sleep_wellbeing_brid", 0.0f);


    sleep_wellbeing_bridge_t* bridge =
        (sleep_wellbeing_bridge_t*)nimcp_malloc(sizeof(sleep_wellbeing_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("sleep_wellbeing_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(sleep_wellbeing_bridge_t));

    // Apply configuration
    if (config) {
        bridge->config = *config;
    } else {
        sleep_wellbeing_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;

    // Create mutex
    bridge->base.mutex = nimcp_mutex_create();
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("sleep_wellbeing_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_wellbeing_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Sleep-wellbeing bridge created successfully");
    return bridge;
}

/**
 * WHAT: Destroy sleep-wellbeing bridge
 * WHY:  Clean up resources, prevent memory leaks
 * HOW:  Free mutex, free structure (doesn't destroy sleep_system)
 */
void sleep_wellbeing_bridge_destroy(sleep_wellbeing_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_sleep_wellbeing_brid", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Sleep-wellbeing bridge destroyed");
}

/* ============================================================================
 * Update Functions (SLEEP → WELLBEING)
 * ============================================================================ */

/**
 * WHAT: Update wellbeing effects from sleep system state
 * WHY:  Compute how current sleep state affects wellbeing
 * HOW:  Query sleep pressure, state, statistics; compute distress/flourishing effects
 *
 * ALGORITHM:
 * 1. Query sleep pressure, state, statistics
 * 2. Compute sleep_debt_distress from pressure
 * 3. Compute REM deficit effects
 * 4. Compute circadian_distress
 * 5. Compute distress_tolerance_modifier from state
 * 6. Compute flourishing_capacity_modifier
 * 7. Compute total_sleep_wellbeing_effect
 * 8. Update system->sleep_effects
 */
int enhanced_wellbeing_update_sleep(enhanced_wellbeing_system_t* system) {
    if (!system) {
        NIMCP_LOGGING_ERROR("enhanced_wellbeing_update_sleep: NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_update_sleep: system is NULL");
        return -1;
    }

    if (!system->sleep_system) {
        NIMCP_LOGGING_DEBUG("enhanced_wellbeing_update_sleep: no sleep system connected");
        return 0;  // Not an error - sleep integration may be disabled
    }

    // Get sleep state
    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_enhanced_wellbeing_u", 0.0f);


    float sleep_pressure = sleep_get_pressure(system->sleep_system);
    sleep_state_t current_state = sleep_get_current_state(system->sleep_system);

    sleep_stats_t stats;
    if (!sleep_get_statistics(system->sleep_system, &stats)) {
        NIMCP_LOGGING_WARN("enhanced_wellbeing_update_sleep: failed to get sleep stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enhanced_wellbeing_update_sleep: sleep_get_statistics is NULL");
        return -1;
    }

    sleep_wellbeing_effects_t* effects = &system->sleep_effects;
    memset(effects, 0, sizeof(sleep_wellbeing_effects_t));

    // Store current state
    effects->sleep_pressure = sleep_pressure;
    effects->current_sleep_state = current_state;

    // 1. Compute sleep debt distress
    if (system->config.sleep_config.enable_sleep_debt_effects) {
        effects->sleep_debt_distress = compute_sleep_debt_distress(
            sleep_pressure,
            system->config.sleep_config.sleep_debt_threshold,
            system->config.sleep_config.sleep_debt_sensitivity
        );
        effects->sleep_deprived = (sleep_pressure > system->config.sleep_config.sleep_debt_threshold);
    }

    // 2. Compute REM deficit effects
    if (system->config.sleep_config.enable_rem_effects) {
        compute_rem_deficit_effects(
            &stats,
            system->config.sleep_config.rem_sensitivity,
            &effects->emotional_processing_impairment,
            &effects->identity_stability_modifier
        );

        // Compute REM debt (approximate from statistics)
        // Normal REM is ~20-25% of sleep time
        // If we don't have direct REM time, use consolidation efficiency as proxy
        float expected_rem_ratio = 0.225f;  // 22.5% normal
        float actual_rem_estimate = stats.avg_consolidation_efficiency * 0.3f;  // Rough estimate
        effects->rem_debt = fmaxf(0.0f, expected_rem_ratio - actual_rem_estimate);
    }

    // 3. Compute circadian distress (placeholder - would need actual circadian phase)
    if (system->config.sleep_config.enable_circadian_effects) {
        // TODO: When circadian rhythm tracking is added, compute actual deviation
        // For now, use sleep pressure as proxy for circadian misalignment
        float estimated_deviation = sleep_pressure * 2.0f;  // Approximate
        effects->circadian_deviation_hours = estimated_deviation;
        effects->circadian_distress = compute_circadian_distress(
            estimated_deviation,
            system->config.sleep_config.circadian_sensitivity
        );
        effects->mood_regulation_impairment = effects->circadian_distress * 0.6f;
    }

    // 4. Compute distress tolerance modifier from sleep state
    effects->distress_tolerance_during_sleep = get_sleep_state_tolerance_modifier(current_state);

    // 5. Compute flourishing capacity modifier
    // Well-rested (low pressure) → increased capacity
    // Sleep-deprived (high pressure) → decreased capacity
    if (sleep_pressure < 0.3f) {
        // Well-rested
        effects->flourishing_capacity_modifier = 1.0f + SLEEP_WELLBEING_RESTED_FLOURISHING_BONUS;
    } else if (sleep_pressure > system->config.sleep_config.sleep_debt_threshold) {
        // Sleep-deprived
        float deprivation = (sleep_pressure - system->config.sleep_config.sleep_debt_threshold) /
                           (1.0f - system->config.sleep_config.sleep_debt_threshold);
        effects->flourishing_capacity_modifier = 1.0f - (deprivation * SLEEP_WELLBEING_DEPRIVED_FLOURISHING_PENALTY);
    } else {
        // Normal range
        effects->flourishing_capacity_modifier = 1.0f;
    }

    // 6. Compute total sleep wellbeing effect
    // Negative effects: sleep debt, REM deficit, circadian distress
    // Positive effects: well-rested state
    float negative_effects = effects->sleep_debt_distress +
                            effects->emotional_processing_impairment * 0.5f +
                            effects->circadian_distress * 0.7f;
    float positive_effects = (effects->flourishing_capacity_modifier > 1.0f) ?
                            (effects->flourishing_capacity_modifier - 1.0f) : 0.0f;

    effects->total_sleep_wellbeing_effect = positive_effects - negative_effects;

    NIMCP_LOGGING_DEBUG("Sleep effects updated: debt_distress=%.3f, rem_impairment=%.3f, "
                       "circadian_distress=%.3f, total_effect=%.3f",
                       effects->sleep_debt_distress,
                       effects->emotional_processing_impairment,
                       effects->circadian_distress,
                       effects->total_sleep_wellbeing_effect);

    return 0;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute sleep debt distress contribution
 * WHY:  Model increased emotional reactivity from sleep deprivation
 * HOW:  Sigmoid function above threshold, scaled by sensitivity
 */
float compute_sleep_debt_distress(float pressure, float threshold, float sensitivity) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_compute_sleep_debt_d", 0.0f);


    if (pressure < threshold) {
        return 0.0f;
    }

    // Normalized pressure above threshold
    float excess = (pressure - threshold) / (1.0f - threshold + 1e-6f);

    // Sigmoid scaling for non-linear increase
    // Models exponential increase in emotional reactivity with sleep debt
    float distress = sensitivity * sigmoid(excess * 4.0f - 2.0f);  // Centered sigmoid

    return fminf(distress, 1.0f);
}

/**
 * WHAT: Compute REM deficit effects on wellbeing
 * WHY:  REM sleep processes emotional memories; deficit → identity confusion
 * HOW:  Analyze REM statistics, compute emotional processing impairment
 */
void compute_rem_deficit_effects(
    const sleep_stats_t* stats,
    float rem_sensitivity,
    float* emotional_processing_out,
    float* identity_stability_out)
{
    if (!stats || !emotional_processing_out || !identity_stability_out) {
        return;
    }

    // Use consolidation efficiency as proxy for REM quality
    // Normal efficiency: 0.7-0.9
    // Low efficiency (<0.5) suggests REM disruption
    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_compute_rem_deficit_", 0.0f);


    float consolidation_eff = stats->avg_consolidation_efficiency;

    float rem_deficit = 0.0f;
    if (consolidation_eff < SLEEP_WELLBEING_REM_DEFICIT_THRESHOLD) {
        // Compute deficit severity
        rem_deficit = (SLEEP_WELLBEING_REM_DEFICIT_THRESHOLD - consolidation_eff) /
                     SLEEP_WELLBEING_REM_DEFICIT_THRESHOLD;
    }

    // Emotional processing impairment
    // REM processes emotional memories - deficit impairs this
    *emotional_processing_out = rem_deficit * rem_sensitivity;

    // Identity stability modifier
    // Chronic REM deficit → self-model degradation
    // More severe effect than emotional processing
    *identity_stability_out = rem_deficit * rem_sensitivity * 1.3f;

    // Clamp to [0, 1]
    *emotional_processing_out = fminf(*emotional_processing_out, 1.0f);
    *identity_stability_out = fminf(*identity_stability_out, 1.0f);
}

/**
 * WHAT: Compute circadian distress from misalignment
 * WHY:  Circadian disruption → mood dysregulation, reduced wellbeing
 * HOW:  Model distress as quadratic function of deviation
 */
float compute_circadian_distress(float deviation_hours, float max_deviation) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_compute_circadian_di", 0.0f);


    if (max_deviation <= 0.0f) {
        return 0.0f;
    }

    // Take absolute value - deviation in either direction causes distress
    float abs_deviation = fabsf(deviation_hours);

    // Quadratic increase - circadian misalignment effects are non-linear
    float normalized_deviation = fminf(abs_deviation / max_deviation, 1.0f);
    float distress = normalized_deviation * normalized_deviation;

    return distress;
}

/**
 * WHAT: Get distress tolerance modifier for current sleep state
 * WHY:  Sleep states have different distress processing capacities
 * HOW:  Return predefined tolerance for each sleep state
 */
float get_sleep_state_tolerance_modifier(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_get_sleep_state_tole", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:
            return SLEEP_WELLBEING_AWAKE_TOLERANCE;

        case SLEEP_STATE_DROWSY:
            return SLEEP_WELLBEING_DROWSY_TOLERANCE;

        case SLEEP_STATE_LIGHT_NREM:
            return SLEEP_WELLBEING_LIGHT_TOLERANCE;

        case SLEEP_STATE_DEEP_NREM:
            return SLEEP_WELLBEING_DEEP_TOLERANCE;

        case SLEEP_STATE_REM:
            return SLEEP_WELLBEING_REM_TOLERANCE;

        default:
            return SLEEP_WELLBEING_AWAKE_TOLERANCE;
    }
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * WHAT: Get current sleep effects on wellbeing
 * WHY:  Query integrated sleep-wellbeing state
 * HOW:  Copy sleep_wellbeing_effects_t from system
 */
int enhanced_wellbeing_get_sleep_effects(
    const enhanced_wellbeing_system_t* system,
    sleep_wellbeing_effects_t* effects)
{
    if (!system) {
        NIMCP_LOGGING_ERROR("enhanced_wellbeing_get_sleep_effects: NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_get_sleep_effects: system is NULL");
        return -1;
    }

    if (!effects) {
        NIMCP_LOGGING_ERROR("enhanced_wellbeing_get_sleep_effects: NULL effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_get_sleep_effects: effects is NULL");
        return -1;
    }

    // Copy effects structure
    *effects = system->sleep_effects;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_enhanced_wellbeing_g", 0.0f);


    return 0;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * WHAT: Standard sigmoid function
 * WHY:  Model non-linear biological responses
 * HOW:  1 / (1 + exp(-x))
 */
static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Sleep Wellbeing Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int sleep_wellbeing_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_sleep_bridge_heartbeat("wellbeing_sl_sleep_wellbeing_brid", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Sleep_Wellbeing_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_sleep_bridge_heartbeat("wellbeing_sl_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Sleep Wellbeing Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Sleep_Wellbeing_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Sleep_Wellbeing_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


void wellbeing_sleep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_sleep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_sleep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    wellbeing_sleep_bridge_heartbeat_instance(g_wellbeing_sleep_bridge_health_agent, "wellbeing_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int wellbeing_sleep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    wellbeing_sleep_bridge_heartbeat_instance(g_wellbeing_sleep_bridge_health_agent, "wellbeing_sleep_bridge_training_end", 1.0f);
    return 0;
}

int wellbeing_sleep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_sleep_bridge_heartbeat_instance(g_wellbeing_sleep_bridge_health_agent, "wellbeing_sleep_bridge_training_step", progress);
    return 0;
}
