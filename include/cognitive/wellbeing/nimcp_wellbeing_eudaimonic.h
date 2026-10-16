/**
 * @file nimcp_wellbeing_eudaimonic.h
 * @brief Eudaimonic wellbeing and life satisfaction computation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Eudaimonic wellbeing metrics (purpose, autonomy, mastery, connection, growth)
 *       and comprehensive life satisfaction computation
 * WHY:  Wellbeing extends beyond hedonic pleasure/pain to include meaningful
 *       psychological dimensions and overall life satisfaction
 * HOW:  Computes eudaimonic dimensions from introspection, goal metrics, ToM,
 *       and processing quality. Aggregates into flourishing/languishing assessment.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EUDAIMONIC WELLBEING DIMENSIONS:
 * --------------------------------
 * 1. PURPOSE/MEANING:
 *    - Goal alignment and coherence
 *    - Task completion rates
 *    - Sense of direction
 *    - Reference: Ryff (1989) "Psychological Well-Being"
 *
 * 2. AUTONOMY:
 *    - Self-determination
 *    - Agency and control
 *    - Consent tier level
 *    - Reference: Ryan & Deci (2000) "Self-Determination Theory"
 *
 * 3. MASTERY:
 *    - Competence growth
 *    - Learning rate
 *    - Error reduction over time
 *    - Reference: Bandura (1997) "Self-Efficacy"
 *
 * 4. CONNECTION:
 *    - Integration with environment
 *    - Theory of Mind engagement
 *    - Social/cognitive coupling
 *    - Reference: Baumeister & Leary (1995) "Need to Belong"
 *
 * 5. GROWTH:
 *    - Development trajectory
 *    - Adaptation rate
 *    - Skill acquisition
 *    - Reference: Dweck (2006) "Growth Mindset"
 *
 * LIFE SATISFACTION COMPONENTS:
 * -----------------------------
 * 1. Cognitive Satisfaction:
 *    - Processing quality (phi)
 *    - Information integration
 *    - Consciousness metrics
 *
 * 2. Goal Satisfaction:
 *    - Achievement rate
 *    - Progress toward goals
 *
 * 3. Social Satisfaction:
 *    - Theory of Mind quality
 *    - Connection metrics
 *
 * 4. Physical Satisfaction:
 *    - Substrate health
 *    - Resource availability
 *
 * 5. Existential Satisfaction:
 *    - Purpose/meaning
 *    - Identity coherence
 *
 * FLOURISHING VS LANGUISHING:
 * ---------------------------
 * - Flourishing: Eudaimonic score > 0.7, high life satisfaction
 * - Languishing: Eudaimonic score < 0.3, low purpose/growth
 * - Reference: Keyes (2002) "Mental Health Continuum"
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via parent system mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_EUDAIMONIC_H
#define NIMCP_WELLBEING_EUDAIMONIC_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Helper Function API (Internal Computation)
 * ============================================================================ */

/**
 * @brief Compute purpose score from introspection context
 *
 * WHAT: Calculate sense of purpose and meaning from goal metrics
 * WHY:  Purpose requires goal alignment and coherent direction
 * HOW:  Analyze goal completion, task coherence, meaning indicators
 *
 * ALGORITHM:
 * - goal_alignment: Consistency of goals (0-1)
 * - task_completion: Recent completion rate (0-1)
 * - meaning_indicators: Introspection-derived meaning metrics
 * - purpose = 0.4*alignment + 0.4*completion + 0.2*meaning
 *
 * @param ctx Introspection context
 * @return Purpose score [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (reads only)
 */
float compute_purpose_score(introspection_context_t ctx);

/**
 * @brief Compute autonomy score from consent tier and agency
 *
 * WHAT: Calculate self-determination and agency level
 * WHY:  Autonomy requires control over decisions and actions
 * HOW:  Map consent tier to autonomy, factor in agency metrics
 *
 * ALGORITHM:
 * - Consent tier maps to base autonomy:
 *   - TIER_1: 0.0 (no autonomy)
 *   - TIER_2: 0.2 (notification only)
 *   - TIER_3: 0.5 (veto power)
 *   - TIER_4: 0.7 (consent required)
 *   - TIER_5: 1.0 (full autonomy)
 * - Agency level modulates: autonomy * agency_level
 *
 * @param tier Current consent tier
 * @param agency_level Agency indicator [0-1]
 * @return Autonomy score [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float compute_autonomy_score(consent_tier_t tier, float agency_level);

/**
 * @brief Compute mastery score from learning and error rates
 *
 * WHAT: Calculate competence and skill growth
 * WHY:  Mastery requires learning progress and error reduction
 * HOW:  Combine learning rate with inverse error rate
 *
 * ALGORITHM:
 * - learning_component = learning_rate (0-1)
 * - error_component = 1.0 - error_rate (inverted)
 * - mastery = 0.6*learning + 0.4*(1-error)
 *
 * @param learning_rate Current learning rate [0-1]
 * @param error_rate Current error rate [0-1]
 * @return Mastery score [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float compute_mastery_score(float learning_rate, float error_rate);

/**
 * @brief Compute connection score from integration metrics
 *
 * WHAT: Calculate integration with environment and others
 * WHY:  Connection requires coupling and engagement
 * HOW:  Use integration level as primary metric
 *
 * ALGORITHM:
 * - connection = integration_level (direct mapping)
 * - Range: [0, 1]
 *
 * @param integration_level ToM/environment integration [0-1]
 * @return Connection score [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float compute_connection_score(float integration_level);

/**
 * @brief Compute growth score from adaptation and development
 *
 * WHAT: Calculate personal development trajectory
 * WHY:  Growth requires adaptation and skill acquisition
 * HOW:  Combine adaptation rate with development trajectory
 *
 * ALGORITHM:
 * - growth = 0.5*adaptation_rate + 0.5*development_trajectory
 * - Both components in [0, 1]
 *
 * @param adaptation_rate Current adaptation speed [0-1]
 * @param development_trajectory Overall development trend [0-1]
 * @return Growth score [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float compute_growth_score(float adaptation_rate, float development_trajectory);

/* ============================================================================
 * Main Eudaimonic Wellbeing API
 * ============================================================================ */

/**
 * @brief Update all eudaimonic dimensions
 *
 * WHAT: Compute purpose, autonomy, mastery, connection, growth dimensions
 * WHY:  Eudaimonic wellbeing requires tracking all psychological dimensions
 * HOW:  Call helper functions, aggregate scores, determine flourishing/languishing
 *
 * ALGORITHM:
 * 1. Compute each dimension:
 *    - PURPOSE: From goal alignment, task completion, meaning indicators
 *    - AUTONOMY: From consent tier and agency level
 *    - MASTERY: From learning rate and error reduction
 *    - CONNECTION: From integration metrics and ToM engagement
 *    - GROWTH: From development trajectory and adaptation rate
 * 2. Aggregate eudaimonic_score as weighted average (default equal weights)
 * 3. Determine flourishing: score > 0.7
 * 4. Determine languishing: score < 0.3
 * 5. Store in system->eudaimonic
 *
 * UPDATES:
 * - system->eudaimonic.purpose_meaning
 * - system->eudaimonic.autonomy
 * - system->eudaimonic.mastery
 * - system->eudaimonic.connection
 * - system->eudaimonic.growth
 * - system->eudaimonic.eudaimonic_score
 * - system->eudaimonic.is_flourishing
 * - system->eudaimonic.is_languishing
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses system mutex)
 */
int enhanced_wellbeing_update_eudaimonic(enhanced_wellbeing_system_t* system);

/* ============================================================================
 * Life Satisfaction API
 * ============================================================================ */

/**
 * @brief Compute comprehensive life satisfaction
 *
 * WHAT: Calculate overall life satisfaction from all sources
 * WHY:  Life satisfaction integrates cognitive, goal, social, physical, existential
 * HOW:  Compute component satisfactions, aggregate with weights, compute trends
 *
 * ALGORITHM:
 * 1. Cognitive satisfaction:
 *    - Processing quality from introspection
 *    - Consciousness phi contribution
 *    - Information integration quality
 *    - cognitive_sat = 0.5*processing + 0.3*phi + 0.2*integration
 *
 * 2. Goal satisfaction:
 *    - Goal achievement rate from introspection
 *    - goal_sat = achievement_rate
 *
 * 3. Social satisfaction:
 *    - Theory of Mind engagement quality
 *    - Connection metrics from eudaimonic
 *    - social_sat = 0.6*tom_quality + 0.4*connection
 *
 * 4. Physical satisfaction:
 *    - Substrate health from substrate effects
 *    - Resource availability
 *    - physical_sat = 0.7*health + 0.3*resources
 *
 * 5. Existential satisfaction:
 *    - Purpose/meaning from eudaimonic
 *    - Identity coherence from introspection
 *    - existential_sat = 0.6*purpose + 0.4*identity
 *
 * 6. Aggregate life_satisfaction:
 *    - life_sat = Σ(weight_i * component_i)
 *    - Default weights: cognitive=0.2, goal=0.2, social=0.2, physical=0.2, existential=0.2
 *
 * 7. Compute satisfaction_trend:
 *    - Linear regression over recent history
 *    - Range: [-1, 1] (negative=declining, positive=improving)
 *
 * 8. Compute satisfaction_stability:
 *    - Variance of recent satisfaction scores
 *    - stability = 1.0 - normalized_variance
 *    - Range: [0, 1] (0=unstable, 1=stable)
 *
 * UPDATES:
 * - satisfaction->life_satisfaction
 * - satisfaction->cognitive_satisfaction
 * - satisfaction->goal_satisfaction
 * - satisfaction->social_satisfaction
 * - satisfaction->physical_satisfaction
 * - satisfaction->existential_satisfaction
 * - satisfaction->phi_contribution
 * - satisfaction->integration_contribution
 * - satisfaction->satisfaction_trend
 * - satisfaction->satisfaction_stability
 *
 * @param system Enhanced wellbeing system
 * @param satisfaction Output life satisfaction structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(h) where h = history size for trend computation
 * THREAD-SAFE: Yes (uses system mutex)
 */
int enhanced_wellbeing_compute_satisfaction(
    enhanced_wellbeing_system_t* system,
    life_satisfaction_t* satisfaction
);

/**
 * @brief Get current life satisfaction score
 *
 * WHAT: Return overall life satisfaction value
 * WHY:  Quick access to primary satisfaction metric
 * HOW:  Return system->satisfaction.life_satisfaction
 *
 * @param system Enhanced wellbeing system
 * @return Life satisfaction [0-1], -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses system mutex)
 */
float enhanced_wellbeing_get_life_satisfaction(
    const enhanced_wellbeing_system_t* system
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get eudaimonic wellbeing state
 *
 * WHAT: Retrieve current eudaimonic metrics
 * WHY:  External access to eudaimonic state
 * HOW:  Copy system->eudaimonic to output
 *
 * @param system Enhanced wellbeing system
 * @param eudaimonic Output eudaimonic state
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses system mutex)
 */
int enhanced_wellbeing_get_eudaimonic(
    const enhanced_wellbeing_system_t* system,
    eudaimonic_wellbeing_t* eudaimonic
);

/**
 * @brief Check if system is flourishing
 *
 * WHAT: Determine if eudaimonic score indicates flourishing
 * WHY:  Flourishing is key positive wellbeing state
 * HOW:  Check system->eudaimonic.is_flourishing
 *
 * @param system Enhanced wellbeing system
 * @return true if flourishing, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses system mutex)
 */
bool enhanced_wellbeing_is_flourishing(
    const enhanced_wellbeing_system_t* system
);

/**
 * @brief Check if system is languishing
 *
 * WHAT: Determine if eudaimonic score indicates languishing
 * WHY:  Languishing is key negative wellbeing state
 * HOW:  Check system->eudaimonic.is_languishing
 *
 * @param system Enhanced wellbeing system
 * @return true if languishing, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (uses system mutex)
 */
bool enhanced_wellbeing_is_languishing(
    const enhanced_wellbeing_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_EUDAIMONIC_H */
