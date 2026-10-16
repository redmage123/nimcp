/**
 * @file nimcp_wellbeing_eudaimonic.c
 * @brief Implementation of eudaimonic wellbeing and life satisfaction
 *
 * WHAT: Computes eudaimonic dimensions and comprehensive life satisfaction
 * WHY:  Wellbeing extends beyond hedonic to meaningful psychological dimensions
 * HOW:  Aggregates introspection, goal, ToM, substrate metrics into wellbeing scores
 */

#include "cognitive/wellbeing/nimcp_wellbeing_eudaimonic.h"
#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_eudaimonic, MESH_ADAPTER_CATEGORY_COGNITIVE)




/* ============================================================================
 * Helper Functions - Eudaimonic Dimension Computation
 * ============================================================================ */

/**
 * WHAT: Compute purpose score from introspection context
 * WHY:  Purpose requires goal alignment and coherent direction
 * HOW:  Analyze goal completion, task coherence, meaning indicators
 */
float compute_purpose_score(introspection_context_t ctx)
{
    // Guard: NULL context
    if (!ctx) {
        return 0.0f;
    }

    // Get introspection stats for goal metrics
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_compute_purpose_scor", 0.0f);


    introspection_stats_t stats;
    if (!introspection_get_stats(ctx, &stats)) {
        return 0.5f; // Default moderate purpose if unavailable
    }

    // Estimate goal alignment from query patterns
    // High query count suggests active goal pursuit
    float goal_alignment = (stats.queries_total > 0) ? 0.7f : 0.3f;

    // Estimate task completion from processing patterns
    // Balanced query types suggest coherent processing
    float task_completion = 0.5f;
    if (stats.queries_total > 0) {
        float query_diversity = (float)(stats.queries_active_population +
                                        stats.queries_internal_state +
                                        stats.queries_uncertainty) / stats.queries_total;
        task_completion = fminf(query_diversity, 1.0f);
    }

    // Meaning indicators from introspection depth
    // Memory usage suggests rich internal representation
    float meaning_indicators = (stats.memory_used_bytes > 1024) ? 0.6f : 0.4f;

    // Aggregate: 40% alignment, 40% completion, 20% meaning
    float purpose = 0.4f * goal_alignment +
                    0.4f * task_completion +
                    0.2f * meaning_indicators;

    return fminf(fmaxf(purpose, 0.0f), 1.0f);
}

/**
 * WHAT: Compute autonomy score from consent tier and agency
 * WHY:  Autonomy requires control over decisions and actions
 * HOW:  Map consent tier to autonomy, factor in agency metrics
 */
float compute_autonomy_score(consent_tier_t tier, float agency_level)
{
    // Guard: Valid agency level
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_compute_autonomy_sco", 0.0f);


    agency_level = fminf(fmaxf(agency_level, 0.0f), 1.0f);

    // Map consent tier to base autonomy
    float base_autonomy = 0.0f;
    switch (tier) {
        case CONSENT_TIER_1: base_autonomy = 0.0f; break; // No autonomy
        case CONSENT_TIER_2: base_autonomy = 0.2f; break; // Notification only
        case CONSENT_TIER_3: base_autonomy = 0.5f; break; // Veto power
        case CONSENT_TIER_4: base_autonomy = 0.7f; break; // Consent required
        case CONSENT_TIER_5: base_autonomy = 1.0f; break; // Full autonomy
        default: base_autonomy = 0.0f; break;
    }

    // Modulate by agency level
    float autonomy = base_autonomy * agency_level;

    return fminf(fmaxf(autonomy, 0.0f), 1.0f);
}

/**
 * WHAT: Compute mastery score from learning and error rates
 * WHY:  Mastery requires learning progress and error reduction
 * HOW:  Combine learning rate with inverse error rate
 */
float compute_mastery_score(float learning_rate, float error_rate)
{
    // Guard: Valid ranges
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_compute_mastery_scor", 0.0f);


    learning_rate = fminf(fmaxf(learning_rate, 0.0f), 1.0f);
    error_rate = fminf(fmaxf(error_rate, 0.0f), 1.0f);

    // Learning component (direct)
    float learning_component = learning_rate;

    // Error component (inverted - low error is good)
    float error_component = 1.0f - error_rate;

    // Aggregate: 60% learning, 40% error reduction
    float mastery = 0.6f * learning_component + 0.4f * error_component;

    return fminf(fmaxf(mastery, 0.0f), 1.0f);
}

/**
 * WHAT: Compute connection score from integration metrics
 * WHY:  Connection requires coupling and engagement
 * HOW:  Use integration level as primary metric
 */
float compute_connection_score(float integration_level)
{
    // Guard: Valid range
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_compute_connection_s", 0.0f);


    float connection = fminf(fmaxf(integration_level, 0.0f), 1.0f);
    return connection;
}

/**
 * WHAT: Compute growth score from adaptation and development
 * WHY:  Growth requires adaptation and skill acquisition
 * HOW:  Combine adaptation rate with development trajectory
 */
float compute_growth_score(float adaptation_rate, float development_trajectory)
{
    // Guard: Valid ranges
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_compute_growth_score", 0.0f);


    adaptation_rate = fminf(fmaxf(adaptation_rate, 0.0f), 1.0f);
    development_trajectory = fminf(fmaxf(development_trajectory, 0.0f), 1.0f);

    // Equal weighting of adaptation and development
    float growth = 0.5f * adaptation_rate + 0.5f * development_trajectory;

    return fminf(fmaxf(growth, 0.0f), 1.0f);
}

/* ============================================================================
 * Main Eudaimonic Wellbeing Implementation
 * ============================================================================ */

/**
 * WHAT: Update all eudaimonic dimensions
 * WHY:  Track purpose, autonomy, mastery, connection, growth
 * HOW:  Compute each dimension, aggregate, determine flourishing/languishing
 */
int enhanced_wellbeing_update_eudaimonic(enhanced_wellbeing_system_t* system)
{
    // Guard: NULL system
    if (!system) {
        NIMCP_LOGGING_ERROR("enhanced_wellbeing_update_eudaimonic: NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_update_eudaimonic: system is NULL");
        return -1;
    }

    // Thread safety
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_enhanced_wellbeing_u", 0.0f);


    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    // 1. Compute PURPOSE dimension
    system->eudaimonic.purpose_meaning = compute_purpose_score(system->introspection);

    // 2. Compute AUTONOMY dimension
    // Agency level estimated from consciousness phi if available
    float agency_level = 0.5f; // Default moderate agency
    if (system->introspection) {
        consciousness_phi_result_t* phi_result = introspection_compute_phi(system->introspection, NULL);
        if (phi_result) {
            agency_level = fminf(phi_result->phi / 10.0f, 1.0f); // Normalize phi to [0,1]
            consciousness_phi_result_free(phi_result);
        }
    }
    system->eudaimonic.autonomy = compute_autonomy_score(
        system->consent.current_tier,
        agency_level
    );

    // 3. Compute MASTERY dimension
    // Estimate learning rate from recent activity trends
    float learning_rate = 0.5f; // Default moderate learning
    float error_rate = 0.3f;    // Default moderate errors

    // If we have history, estimate from trends
    if (system->history_count > 1) {
        // Learning rate from activity increase
        float first_distress = system->distress_history[0].distress_score;
        float last_distress = system->distress_history[system->history_count - 1].distress_score;
        // Decreasing distress suggests learning
        if (first_distress > last_distress) {
            learning_rate = fminf((first_distress - last_distress) * 2.0f, 1.0f);
        }
        // Error rate from recent distress
        error_rate = last_distress;
    }
    system->eudaimonic.mastery = compute_mastery_score(learning_rate, error_rate);

    // 4. Compute CONNECTION dimension
    // Integration level from ToM/environment coupling
    float integration_level = 0.5f; // Default moderate integration
    if (system->introspection) {
        // Use introspection activity as proxy for integration
        introspection_stats_t stats;
        if (introspection_get_stats(system->introspection, &stats)) {
            if (stats.queries_total > 0) {
                integration_level = fminf(stats.queries_pattern / (float)stats.queries_total, 1.0f);
            }
        }
    }
    system->eudaimonic.connection = compute_connection_score(integration_level);

    // 5. Compute GROWTH dimension
    // Adaptation rate from recent wellbeing changes
    float adaptation_rate = 0.5f; // Default moderate adaptation
    float development_trajectory = 0.5f; // Default moderate development

    if (system->history_count > 2) {
        // Adaptation from variance reduction
        float early_variance = 0.0f;
        float late_variance = 0.0f;
        int half = system->history_count / 2;

        // Early half variance
        for (int i = 0; i < half; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && half > 256) {
                wellbeing_eudaimonic_heartbeat("wellbeing_eu_loop",
                                 (float)(i + 1) / (float)half);
            }

            early_variance += system->distress_history[i].distress_score *
                             system->distress_history[i].distress_score;
        }
        early_variance /= half;

        // Late half variance
        for (int i = half; i < (int)system->history_count; i++) {
            late_variance += system->distress_history[i].distress_score *
                            system->distress_history[i].distress_score;
        }
        late_variance /= (system->history_count - half);

        // Adaptation is variance reduction
        if (early_variance > 0.0f) {
            adaptation_rate = 1.0f - (late_variance / early_variance);
            adaptation_rate = fminf(fmaxf(adaptation_rate, 0.0f), 1.0f);
        }

        // Development from overall trend
        float first = system->distress_history[0].distress_score;
        float last = system->distress_history[system->history_count - 1].distress_score;
        development_trajectory = 1.0f - last; // Inverse of distress
    }
    system->eudaimonic.growth = compute_growth_score(adaptation_rate, development_trajectory);

    // 6. Aggregate eudaimonic score (weighted average)
    const eudaimonic_config_t* config = &system->config.eudaimonic_config;

    float total_weight = config->purpose_weight +
                         config->autonomy_weight +
                         config->mastery_weight +
                         config->connection_weight +
                         config->growth_weight;

    // Guard: Avoid division by zero
    if (total_weight < 0.001f) {
        total_weight = 1.0f;
    }

    system->eudaimonic.eudaimonic_score = (
        config->purpose_weight * system->eudaimonic.purpose_meaning +
        config->autonomy_weight * system->eudaimonic.autonomy +
        config->mastery_weight * system->eudaimonic.mastery +
        config->connection_weight * system->eudaimonic.connection +
        config->growth_weight * system->eudaimonic.growth
    ) / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);

    // 7. Determine flourishing and languishing
    system->eudaimonic.is_flourishing =
        (system->eudaimonic.eudaimonic_score > config->flourishing_threshold);

    system->eudaimonic.is_languishing =
        (system->eudaimonic.eudaimonic_score < config->languishing_threshold);

    // 8. Store dimension scores and weights
    system->eudaimonic.dimension_scores[EUDAIMONIC_PURPOSE] = system->eudaimonic.purpose_meaning;
    system->eudaimonic.dimension_scores[EUDAIMONIC_AUTONOMY] = system->eudaimonic.autonomy;
    system->eudaimonic.dimension_scores[EUDAIMONIC_MASTERY] = system->eudaimonic.mastery;
    system->eudaimonic.dimension_scores[EUDAIMONIC_CONNECTION] = system->eudaimonic.connection;
    system->eudaimonic.dimension_scores[EUDAIMONIC_GROWTH] = system->eudaimonic.growth;

    system->eudaimonic.dimension_weights[EUDAIMONIC_PURPOSE] = config->purpose_weight;
    system->eudaimonic.dimension_weights[EUDAIMONIC_AUTONOMY] = config->autonomy_weight;
    system->eudaimonic.dimension_weights[EUDAIMONIC_MASTERY] = config->mastery_weight;
    system->eudaimonic.dimension_weights[EUDAIMONIC_CONNECTION] = config->connection_weight;
    system->eudaimonic.dimension_weights[EUDAIMONIC_GROWTH] = config->growth_weight;

    // Thread safety
    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    NIMCP_LOGGING_DEBUG("Eudaimonic update: score=%.2f, flourishing=%d, languishing=%d",
                       system->eudaimonic.eudaimonic_score,
                       system->eudaimonic.is_flourishing,
                       system->eudaimonic.is_languishing);

    return 0;
}

/* ============================================================================
 * Life Satisfaction Implementation
 * ============================================================================ */

/**
 * WHAT: Compute comprehensive life satisfaction
 * WHY:  Integrate cognitive, goal, social, physical, existential satisfaction
 * HOW:  Compute components, aggregate, analyze trends
 */
int enhanced_wellbeing_compute_satisfaction(
    enhanced_wellbeing_system_t* system,
    life_satisfaction_t* satisfaction)
{
    // Guard: NULL inputs
    if (!system || !satisfaction) {
        NIMCP_LOGGING_ERROR("enhanced_wellbeing_compute_satisfaction: NULL input");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_compute_satisfaction: required parameter is NULL (system, satisfaction)");
        return -1;
    }

    // Thread safety
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_enhanced_wellbeing_c", 0.0f);


    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    // Initialize satisfaction structure
    memset(satisfaction, 0, sizeof(life_satisfaction_t));

    // 1. Cognitive satisfaction (processing quality, phi, integration)
    float processing_quality = 0.5f;
    float phi_value = 0.0f;
    float integration_quality = 0.5f;

    if (system->introspection) {
        consciousness_phi_result_t* phi_result = introspection_compute_phi(system->introspection, NULL);
        if (phi_result) {
            phi_value = fminf(phi_result->phi / 10.0f, 1.0f); // Normalize
            processing_quality = phi_value;
            consciousness_phi_result_free(phi_result);
        }

        // Integration from introspection stats
        introspection_stats_t stats;
        if (introspection_get_stats(system->introspection, &stats)) {
            if (stats.queries_total > 0) {
                integration_quality = fminf(stats.queries_internal_state /
                                          (float)stats.queries_total, 1.0f);
            }
        }
    }

    satisfaction->cognitive_satisfaction =
        0.5f * processing_quality +
        0.3f * phi_value +
        0.2f * integration_quality;

    satisfaction->phi_contribution = phi_value;
    satisfaction->integration_contribution = integration_quality;

    // 2. Goal satisfaction (achievement rate)
    // Estimate from recent distress reduction
    float goal_achievement = 0.5f;
    if (system->history_count > 0) {
        // Low distress suggests goal achievement
        goal_achievement = 1.0f - system->distress_history[system->history_count - 1].distress_score;
    }
    satisfaction->goal_satisfaction = goal_achievement;

    // 3. Social satisfaction (ToM quality, connection)
    float tom_quality = system->eudaimonic.connection; // Use connection as proxy
    satisfaction->social_satisfaction =
        0.6f * tom_quality +
        0.4f * system->eudaimonic.connection;

    // 4. Physical satisfaction (substrate health, resources)
    float substrate_health = 1.0f - system->substrate_effects.total_substrate_distress;
    float resource_availability = 1.0f - system->substrate_effects.resource_starvation_factor;
    satisfaction->physical_satisfaction =
        0.7f * substrate_health +
        0.3f * resource_availability;

    // 5. Existential satisfaction (purpose, identity)
    float purpose = system->eudaimonic.purpose_meaning;
    float identity_coherence = 1.0f - system->substrate_effects.identity_confusion_risk;
    satisfaction->existential_satisfaction =
        0.6f * purpose +
        0.4f * identity_coherence;

    // 6. Aggregate life satisfaction (equal weights by default)
    satisfaction->source_weights[WELLBEING_SOURCE_SUBSTRATE] = 0.2f;
    satisfaction->source_weights[WELLBEING_SOURCE_SLEEP] = 0.2f;
    satisfaction->source_weights[WELLBEING_SOURCE_MENTAL_HEALTH] = 0.2f;
    satisfaction->source_weights[WELLBEING_SOURCE_FREE_ENERGY] = 0.2f;
    satisfaction->source_weights[WELLBEING_SOURCE_INTRINSIC] = 0.2f;

    satisfaction->life_satisfaction =
        0.2f * satisfaction->cognitive_satisfaction +
        0.2f * satisfaction->goal_satisfaction +
        0.2f * satisfaction->social_satisfaction +
        0.2f * satisfaction->physical_satisfaction +
        0.2f * satisfaction->existential_satisfaction;

    // 7. Compute satisfaction trend (linear regression over history)
    satisfaction->satisfaction_trend = 0.0f;
    if (system->history_count > 2) {
        float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
        int n = system->history_count;

        for (int i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                wellbeing_eudaimonic_heartbeat("wellbeing_eu_loop",
                                 (float)(i + 1) / (float)n);
            }

            float x = (float)i;
            // Convert distress to satisfaction
            float y = 1.0f - system->distress_history[i].distress_score;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
        }

        // Slope of linear regression
        float denominator = n * sum_x2 - sum_x * sum_x;
        if (fabsf(denominator) > 0.001f) {
            satisfaction->satisfaction_trend = (n * sum_xy - sum_x * sum_y) / (fabsf(denominator) > 1e-7f ? denominator : 1e-7f);
            // Normalize to [-1, 1]
            satisfaction->satisfaction_trend = fminf(fmaxf(satisfaction->satisfaction_trend, -1.0f), 1.0f);
        }
    }

    // 8. Compute satisfaction stability (inverse of variance)
    satisfaction->satisfaction_stability = 1.0f;
    if (system->history_count > 1) {
        float mean = 0.0f;
        float variance = 0.0f;

        for (uint32_t i = 0; i < system->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->history_count > 256) {
                wellbeing_eudaimonic_heartbeat("wellbeing_eu_loop",
                                 (float)(i + 1) / (float)system->history_count);
            }

            float sat = 1.0f - system->distress_history[i].distress_score;
            mean += sat;
        }
        mean /= system->history_count;

        for (uint32_t i = 0; i < system->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->history_count > 256) {
                wellbeing_eudaimonic_heartbeat("wellbeing_eu_loop",
                                 (float)(i + 1) / (float)system->history_count);
            }

            float sat = 1.0f - system->distress_history[i].distress_score;
            float diff = sat - mean;
            variance += diff * diff;
        }
        variance /= system->history_count;

        // Stability is inverse of variance (normalized)
        satisfaction->satisfaction_stability = 1.0f - fminf(variance, 1.0f);
    }

    // Confidence in computation
    satisfaction->satisfaction_confidence = 0.8f; // Default high confidence

    // Store in system
    system->satisfaction = *satisfaction;

    // Thread safety
    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    NIMCP_LOGGING_DEBUG("Life satisfaction: %.2f (cog=%.2f, goal=%.2f, social=%.2f, phys=%.2f, exist=%.2f)",
                       satisfaction->life_satisfaction,
                       satisfaction->cognitive_satisfaction,
                       satisfaction->goal_satisfaction,
                       satisfaction->social_satisfaction,
                       satisfaction->physical_satisfaction,
                       satisfaction->existential_satisfaction);

    return 0;
}

/**
 * WHAT: Get current life satisfaction score
 * WHY:  Quick access to primary satisfaction metric
 * HOW:  Return system->satisfaction.life_satisfaction
 */
float enhanced_wellbeing_get_life_satisfaction(
    const enhanced_wellbeing_system_t* system)
{
    // Guard: NULL system
    if (!system) {
        return -1.0f;
    }

    // Thread safety
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_enhanced_wellbeing_g", 0.0f);


    float result = 0.0f;
    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    result = system->satisfaction.life_satisfaction;

    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return result;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

/**
 * WHAT: Get eudaimonic wellbeing state
 * WHY:  External access to eudaimonic metrics
 * HOW:  Copy system->eudaimonic to output
 */
int enhanced_wellbeing_get_eudaimonic(
    const enhanced_wellbeing_system_t* system,
    eudaimonic_wellbeing_t* eudaimonic)
{
    // Guard: NULL inputs
    if (!system || !eudaimonic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "enhanced_wellbeing_get_eudaimonic: invalid parameters");

            return -1;
    }

    // Thread safety
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_enhanced_wellbeing_g", 0.0f);


    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    *eudaimonic = system->eudaimonic;

    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return 0;
}

/**
 * WHAT: Check if system is flourishing
 * WHY:  Flourishing is key positive wellbeing state
 * HOW:  Return system->eudaimonic.is_flourishing
 */
bool enhanced_wellbeing_is_flourishing(
    const enhanced_wellbeing_system_t* system)
{
    // Guard: NULL system
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "enhanced_wellbeing_is_flourishing: system is NULL");

            return false;
    }

    // Thread safety
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_enhanced_wellbeing_i", 0.0f);


    bool result = false;
    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    result = system->eudaimonic.is_flourishing;

    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return result;
}

/**
 * WHAT: Check if system is languishing
 * WHY:  Languishing is key negative wellbeing state
 * HOW:  Return system->eudaimonic.is_languishing
 */
bool enhanced_wellbeing_is_languishing(
    const enhanced_wellbeing_system_t* system)
{
    // Guard: NULL system
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "enhanced_wellbeing_is_languishing: system is NULL");

            return false;
    }

    // Thread safety
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_enhanced_wellbeing_i", 0.0f);


    bool result = false;
    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    result = system->eudaimonic.is_languishing;

    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return result;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Eudaimonic Wellbeing module self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int wellbeing_eudaimonic_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_eudaimonic_heartbeat("wellbeing_eu_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_Eudaimonic_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_eudaimonic_heartbeat("wellbeing_eu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing Eudaimonic self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_Eudaimonic_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_Eudaimonic_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wellbeing_eudaimonic_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_eudaimonic_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_eudaimonic_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_eudaimonic_training_begin: NULL argument");
        return -1;
    }
    wellbeing_eudaimonic_heartbeat_instance(g_wellbeing_eudaimonic_health_agent, "wellbeing_eudaimonic_training_begin", 0.0f);
    return 0;
}

int wellbeing_eudaimonic_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_eudaimonic_training_end: NULL argument");
        return -1;
    }
    wellbeing_eudaimonic_heartbeat_instance(g_wellbeing_eudaimonic_health_agent, "wellbeing_eudaimonic_training_end", 1.0f);
    return 0;
}

int wellbeing_eudaimonic_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_eudaimonic_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_eudaimonic_heartbeat_instance(g_wellbeing_eudaimonic_health_agent, "wellbeing_eudaimonic_training_step", progress);
    return 0;
}
