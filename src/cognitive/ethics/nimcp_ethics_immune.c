/**
 * @file nimcp_ethics_immune.c
 * @brief Ethics Engine - Brain Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration between ethics engine and brain immune system
 * WHY:  Ethics violations trigger immune-like responses; inflammation affects moral reasoning
 * HOW:  Map violations → antigens, query immune state for decision modulation
 *
 * BIOLOGICAL ANALOGY:
 * - Ethics violations = "moral pathogens" threatening system integrity
 * - Severity → danger signal
 * - Repeated violations → adaptive immune memory
 * - Inflammation → impaired moral reasoning (cognitive fog)
 *
 * @author NIMCP Development Team
 */

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_internal.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "ethics.immune"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(ethics_immune, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Immune System Integration Functions
//=============================================================================

/**
 * @brief Set brain immune system for ethics violation response
 *
 * WHAT: Associate ethics engine with immune system
 * WHY:  Enable violations → antigen mapping and inflammation-aware reasoning
 * HOW:  Store immune system reference, enable integration
 *
 * COMPLEXITY: O(1)
 */
void ethics_set_immune_system(ethics_engine_t engine, brain_immune_system_t* immune)
{
    // Guard: NULL check
    if (!engine) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_immune_heartbeat("ethics_immun_ethics_set_immune_sy", 0.0f);


    engine->immune_system = immune;
    engine->immune_integration_enabled = (immune != NULL);
    engine->last_inflammation_level = 0.0F;
    engine->last_immune_check_ms = 0;
    engine->immune_violations_triggered = 0;

    if (immune) {
        LOG_MODULE_INFO(LOG_MODULE, "Ethics engine integrated with brain immune system");
    }
}

/**
 * @brief Get current inflammation level from immune system (cached)
 *
 * WHAT: Query inflammation level with 100ms caching
 * WHY:  Avoid excessive immune system queries
 * HOW:  Check cache expiry, refresh if needed
 *
 * COMPLEXITY: O(1)
 */
static float get_current_inflammation_level(ethics_engine_t engine)
{
    // Guard: Immune integration not enabled
    if (!engine || !engine->immune_integration_enabled || !engine->immune_system) {
        return 0.0F;
    }

    // WHAT: Cache inflammation level for 100ms to avoid excessive queries
    // WHY:  Immune state changes slowly, no need to query every call
    // HOW:  Check timestamp, refresh if expired
    uint64_t now = nimcp_time_monotonic_ms();
    if (now - engine->last_immune_check_ms > 100) {
        brain_immune_stats_t stats;
        if (brain_immune_get_stats(engine->immune_system, &stats) == 0) {
            // WHAT: Compute overall inflammation from sites
            // WHY:  Multiple inflammation sites contribute to systemic effect
            // HOW:  Normalize by max sites, clamp to [0, 1]
            if (stats.inflammation_sites > 0) {
                engine->last_inflammation_level = (float)stats.inflammation_sites /
                                                  (float)BRAIN_IMMUNE_MAX_INFLAMMATION;
                engine->last_inflammation_level = fminf(engine->last_inflammation_level, 1.0F);
            } else {
                engine->last_inflammation_level = 0.0F;
            }
        }
        engine->last_immune_check_ms = now;
    }

    return engine->last_inflammation_level;
}

/**
 * @brief Evaluate action with immune health check
 *
 * WHAT: Perform ethical evaluation with inflammation-adjusted confidence
 * WHY:  High inflammation impairs moral reasoning
 * HOW:  Standard evaluation + inflammation penalty
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (moral reasoning center) impaired by cytokines
 * - Sickness behavior reduces social cognition
 * - High inflammation → lower confidence in nuanced decisions
 *
 * COMPLEXITY: O(1) + standard evaluation
 */
bool ethics_evaluate_with_immune_check(ethics_engine_t engine,
                                         const action_context_t* action,
                                         ethics_evaluation_t* evaluation,
                                         float* inflammation_penalty)
{
    // Guard: Validate parameters
    if (!engine || !action || !evaluation || !inflammation_penalty) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "ethics_evaluate_with_immune_check: invalid parameters");

            return false;
    }

    // WHAT: Perform standard ethical evaluation
    // WHY:  Get base ethical assessment
    // HOW:  Call standard evaluation function
    *evaluation = ethics_engine_evaluate_action(engine, action);

    // Guard: Immune integration not enabled
    if (!engine->immune_integration_enabled || !engine->immune_system) {
        *inflammation_penalty = 0.0F;
        return true;
    }

    // WHAT: Query current inflammation level
    // WHY:  Determine cognitive impairment
    // HOW:  Use cached inflammation query
    /* Phase 8: Heartbeat at operation start */
    ethics_immune_heartbeat("ethics_immun_ethics_evaluate_with", 0.0f);


    float inflammation = get_current_inflammation_level(engine);

    // WHAT: Calculate confidence penalty from inflammation
    // WHY:  High inflammation reduces confidence in complex moral reasoning
    // HOW:  Linear scaling: inflammation 0.0 → penalty 0.0, inflammation 1.0 → penalty 0.5
    //
    // BIOLOGY: Prefrontal cortex sensitivity to pro-inflammatory cytokines
    // - Mild inflammation (0.3): ~15% confidence reduction
    // - Moderate inflammation (0.6): ~30% confidence reduction
    // - Severe inflammation (1.0): ~50% confidence reduction
    *inflammation_penalty = inflammation * 0.5F;

    // WHAT: Apply confidence penalty to evaluation
    // WHY:  Reflect impaired moral reasoning
    // HOW:  Reduce confidence, never below 0.1
    evaluation->confidence = fmaxf(evaluation->confidence - *inflammation_penalty, 0.1F);

    // WHAT: Add inflammation note to explanation
    // WHY:  Transparency about reasoning impairment
    // HOW:  Append to existing explanation
    if (inflammation > 0.3F) {
        char inflammation_note[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(inflammation_note, sizeof(inflammation_note),
                 " [Note: Elevated inflammation (%.1f%%) may impair moral reasoning]",
                 inflammation * 100.0F);
        strncat(evaluation->explanation, inflammation_note,
                sizeof(evaluation->explanation) - strlen(evaluation->explanation) - 1);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Ethics evaluation with immune check: "
                     "base_confidence=%.2f, inflammation=%.2f, penalty=%.2f, final=%.2f",
                     evaluation->confidence + *inflammation_penalty,
                     inflammation, *inflammation_penalty, evaluation->confidence);

    return true;
}

/**
 * @brief Trigger immune response for ethics violation
 *
 * WHAT: Present ethics violation as antigen to immune system
 * WHY:  Build adaptive response to ethical threats
 * HOW:  Create antigen from violation signature, present to immune system
 *
 * DESIGN: Ethics violations treated as "moral pathogens"
 * - Violation type → epitope signature
 * - Severity → danger signal
 * - Repeated violations → memory formation (adaptive immunity)
 * - Immune response escalates with frequency
 *
 * COMPLEXITY: O(1)
 */
bool ethics_trigger_immune_response(ethics_engine_t engine,
                                      ethics_violation_type_t violation,
                                      float severity,
                                      const char* description)
{
    // Guard: Validate parameters
    if (!engine || !description) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "ethics_trigger_immune_response: invalid parameters");

            return false;
    }

    // Guard: Immune integration not enabled
    if (!engine->immune_integration_enabled) {
        NIMCP_LOGGING_WARN("ethics_trigger_immune_response: immune integration not enabled");
        return false;
    }
    if (!engine->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_trigger_immune_response: engine->immune_system is NULL");
        return false;
    }

    // WHAT: Create epitope signature from violation
    // WHY:  Map ethical violation to immune system antigen
    // HOW:  Hash violation type + description into epitope
    /* Phase 8: Heartbeat at operation start */
    ethics_immune_heartbeat("ethics_immun_ethics_trigger_immun", 0.0f);


    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));

    // Simple epitope encoding: violation type + severity + hash of description
    epitope[0] = (uint8_t)violation;
    epitope[1] = (uint8_t)(severity * 255.0F);

    // Hash description into remaining epitope bytes
    size_t desc_len = strlen(description);
    for (size_t i = 0; i < desc_len && i < (BRAIN_IMMUNE_EPITOPE_SIZE - 2); i++) {
        epitope[i + 2] = (uint8_t)description[i];
    }

    // WHAT: Map severity to immune system severity scale
    // WHY:  Ethics severity [0, 1] → immune severity [1, 10]
    // HOW:  Linear scaling with floor at 1
    uint32_t immune_severity = (uint32_t)(severity * 9.0F) + 1;  // [1, 10]

    // WHAT: Present violation as antigen to immune system
    // WHY:  Trigger adaptive immune response
    // HOW:  Use immune system antigen presentation API
    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(
        engine->immune_system,
        ANTIGEN_SOURCE_MANUAL,  // Manually reported (ethics module)
        epitope,
        sizeof(epitope),
        immune_severity,
        0,  // source_node (N/A for ethics violations)
        &antigen_id
    );

    if (result == 0) {
        engine->immune_violations_triggered++;
        LOG_MODULE_INFO(LOG_MODULE, "Ethics violation triggered immune response: "
                        "type=%s, severity=%.2f, antigen_id=%u",
                        ethics_violation_type_name(violation), severity, antigen_id);
        return true;
    } else {
        LOG_MODULE_WARN(LOG_MODULE, "Failed to trigger immune response for ethics violation: "
                        "type=%s, severity=%.2f, error=%d",
                        ethics_violation_type_name(violation), severity, result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_trigger_immune_response: operation failed");
        return false;
    }
}

/**
 * @brief Get immune-adjusted ethical decision threshold
 *
 * WHAT: Calculate decision threshold adjusted for inflammation
 * WHY:  High inflammation → more conservative, risk-averse decisions
 * HOW:  Increase threshold based on inflammation level
 *
 * BIOLOGY: Inflamed state triggers behavioral conservatism
 * - Sickness behavior → reduced exploration
 * - High cytokines → risk aversion
 * - Goal: preserve resources during immune challenge
 *
 * COMPLEXITY: O(1)
 */
float ethics_get_immune_adjusted_threshold(ethics_engine_t engine, float base_threshold)
{
    // Guard: NULL check
    if (!engine) {
        return base_threshold;
    }

    // Guard: Immune integration not enabled
    if (!engine->immune_integration_enabled || !engine->immune_system) {
        return base_threshold;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_immune_heartbeat("ethics_immun_ethics_get_immune_ad", 0.0f);


    float inflammation = get_current_inflammation_level(engine);

    // WHAT: Map inflammation [0, 1] to threshold increase [0.0, 0.2]
    // WHY:  Higher threshold = more conservative decisions
    // HOW:  Linear scaling
    //
    // inflammation=0.0 → offset=0.0 (normal threshold)
    // inflammation=0.5 → offset=0.1 (moderate conservatism)
    // inflammation=1.0 → offset=0.2 (high conservatism)
    float offset = inflammation * 0.2F;
    float adjusted = base_threshold + offset;

    // Clamp to [0, 1]
    return fminf(adjusted, 1.0F);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Immune self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_immune_heartbeat("ethics_immun_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Immune_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_immune_heartbeat("ethics_immun_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_MODULE_DEBUG(LOG_MODULE, "Ethics immune self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Immune_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Immune_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_immune_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_ethics_immune_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_immune_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_immune_training_begin: NULL argument");
        return -1;
    }
    ethics_immune_heartbeat_instance(NULL, "ethics_immune_training_begin", 0.0f);
    return 0;
}

int ethics_immune_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_immune_training_end: NULL argument");
        return -1;
    }
    ethics_immune_heartbeat_instance(NULL, "ethics_immune_training_end", 1.0f);
    return 0;
}

int ethics_immune_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_immune_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_immune_heartbeat_instance(NULL, "ethics_immune_training_step", progress);
    return 0;
}
