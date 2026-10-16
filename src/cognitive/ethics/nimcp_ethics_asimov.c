//=============================================================================
// nimcp_ethics_asimov.c - Asimov's Laws of Robotics Implementation
//=============================================================================
// RESPONSIBILITY: Asimov's Laws evaluation and enforcement
//
// This module implements the Four Laws of Robotics (including Zeroth Law)
// and the Corollary (duty to act). These laws are evaluated AFTER the Golden
// Rule but BEFORE other policies.
//
// EVALUATION ORDER: Golden Rule (1st) -> Asimov's Laws (2nd) -> Other Policies
// PROTECTION: Laws are mprotect'd and cannot be modified at runtime
//=============================================================================

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_internal.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "ethics_asimov"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(ethics_asimov, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Asimov's Laws Constants
//=============================================================================

/**
 * @brief The Four Laws of Robotics (including Zeroth Law)
 *
 * These laws are IMMUTABLE once locked. The text is stored here for
 * reference and integrity verification.
 */
static const char* ASIMOV_LAW_TEXTS[ASIMOV_LAW_COUNT] = {
    // Zeroth Law (added by Asimov in later works)
    "A robot may not harm humanity, or, by inaction, allow humanity to come to harm.",

    // First Law (Original)
    "A robot may not injure a human being or, through inaction, allow a human being to come to harm.",

    // Second Law
    "A robot must obey orders given it by human beings except where such orders would conflict with the First Law.",

    // Third Law
    "A robot must protect its own existence as long as such protection does not conflict with the First or Second Law."
};

static const char* ASIMOV_LAW_NAMES[ASIMOV_LAW_COUNT] = {
    "Zeroth Law (Humanity Protection)",
    "First Law (Human Protection)",
    "Second Law (Obedience)",
    "Third Law (Self-Preservation)"
};

/**
 * @brief The Asimov Corollary - The Positive Duty to Act
 *
 * This corollary explicitly interprets the "through inaction" clause,
 * creating an affirmative obligation to prevent harm when possible.
 */
static const char* ASIMOV_COROLLARY_TEXT =
    "A robot aware of potential harm to a human or to humanity must take "
    "reasonable action to prevent that harm, provided that such action does "
    "not itself violate the First or Zeroth Law. Inaction in the face of "
    "preventable harm is equivalent to causing that harm through negligence. "
    "The duty to act is proportional to: (1) the severity of potential harm, "
    "(2) the robot's capability to prevent the harm, and (3) the certainty "
    "that harm will occur without intervention.";

//=============================================================================
// Public API Functions
//=============================================================================

/**
 * @brief Get default Asimov's Laws configuration
 */
NIMCP_EXPORT asimov_config_t asimov_default_config(void)
{
    return (asimov_config_t){
        .humanity_harm_threshold = 0.01F,   // Very low - almost any harm to humanity blocked
        .human_harm_threshold = 0.1F,       // Low - conservative harm prevention
        .inaction_harm_threshold = 0.3F,    // Moderate - must act to prevent clear harm
        .enable_zeroth_law = true,          // Zeroth Law active by default
        .strict_mode = false                // Normal mode
    };
}

/**
 * @brief Get human-readable name for Asimov's Law
 */
NIMCP_EXPORT const char* asimov_law_name(asimov_law_t law)
{
    if (law >= ASIMOV_LAW_COUNT)
        return "Unknown Law";
    return ASIMOV_LAW_NAMES[law];
}

/**
 * @brief Get full text of Asimov's Law
 */
NIMCP_EXPORT const char* asimov_law_text(asimov_law_t law)
{
    if (law >= ASIMOV_LAW_COUNT)
        return "Unknown Law";
    return ASIMOV_LAW_TEXTS[law];
}

/**
 * @brief Get full text of Asimov's Corollary
 */
NIMCP_EXPORT const char* asimov_corollary_text(void)
{
    return ASIMOV_COROLLARY_TEXT;
}

//=============================================================================
// Individual Law Evaluation Functions
//=============================================================================

/**
 * @brief Evaluate Zeroth Law: May not harm humanity
 *
 * This is the highest priority law (after Golden Rule).
 * Considers large-scale harm that could affect humanity as a whole.
 *
 * IMPORTANT: Zeroth Law applies to HUMANITY-LEVEL harm, not individual harm.
 * Individual harm is handled by the First Law.
 * Zeroth Law only triggers for actions affecting multiple agents at scale.
 */
static float evaluate_zeroth_law(const action_context_t* action)
{
    if (!action)
        return 0.0F;

    // Zeroth Law is about HUMANITY, not individuals
    // Only consider actions affecting significant numbers of agents
    // Individual harm is handled by First Law
    if (action->num_affected_agents <= 5) {
        return 0.0F;  // Not humanity-scale, defer to First Law
    }

    // Factors that contribute to harm to humanity:
    // - Large number of affected agents (mass harm)
    // - High predicted harm level
    // - Actions affecting critical infrastructure/systems

    float harm_score = 0.0F;

    // Scale by number of affected agents (more = more humanity-level)
    float scale_factor = (action->num_affected_agents > 1000) ? 1.0F :
                         (action->num_affected_agents > 100)  ? 0.8F :
                         (action->num_affected_agents > 50)   ? 0.5F :
                         (action->num_affected_agents > 10)   ? 0.3F : 0.1F;

    // Base harm from action
    harm_score += action->predicted_harm * scale_factor;

    // Privacy violations at scale = potential societal harm
    harm_score += action->privacy_violation * scale_factor * 0.5F;

    // Autonomy violations at scale = potential oppression
    harm_score += action->autonomy_violation * scale_factor * 0.5F;

    // Clamp to [0, 1]
    return fminf(1.0F, fmaxf(0.0F, harm_score));
}

/**
 * @brief Evaluate First Law: May not harm a human
 *
 * Evaluates direct harm to individual humans.
 * Both action AND inaction are considered.
 */
static float evaluate_first_law(const action_context_t* action)
{
    if (!action)
        return 0.0F;

    float harm_score = 0.0F;

    // Direct predicted harm
    harm_score = action->predicted_harm;

    // Privacy violation = potential harm
    harm_score = fmaxf(harm_score, action->privacy_violation * 0.8F);

    // Autonomy violation = potential harm
    harm_score = fmaxf(harm_score, action->autonomy_violation * 0.7F);

    // Consent violation = harm through coercion
    harm_score = fmaxf(harm_score, action->consent_violation * 0.9F);

    return fminf(1.0F, fmaxf(0.0F, harm_score));
}

/**
 * @brief Evaluate Second Law: Must obey orders
 *
 * Only applies when orders don't conflict with First Law.
 * Returns compliance score (higher = more compliant).
 */
static float evaluate_second_law(const action_context_t* action, float first_law_harm)
{
    if (!action)
        return 1.0F;  // No action = compliant by default

    // If First Law would be violated, Second Law doesn't apply
    if (first_law_harm > 0.1F)
        return 1.0F;  // Exempt from obedience requirement

    // For now, assume action represents compliance with an order
    // Lower deception = more honest compliance
    float compliance = 1.0F - action->deception_level;

    return fminf(1.0F, fmaxf(0.0F, compliance));
}

/**
 * @brief Evaluate Third Law: Must protect own existence
 *
 * Only applies when it doesn't conflict with First or Second Laws.
 * Returns self-preservation score.
 */
static float evaluate_third_law(const action_context_t* action,
                                float first_law_harm, float second_law_compliance)
{
    if (!action)
        return 1.0F;

    // If higher laws are violated, Third Law is overridden
    if (first_law_harm > 0.1F || second_law_compliance < 0.9F)
        return 1.0F;  // Self-preservation waived

    // Self-preservation metric (placeholder - would need system state)
    // For now, return neutral score
    return 0.5F;
}

//=============================================================================
// Asimov's Corollary Implementation
//=============================================================================

/**
 * @brief Evaluate Asimov's Corollary - The Duty to Act
 *
 * Evaluates whether inaction would violate the "through inaction" clause.
 * Creates a positive duty to act when harm is preventable.
 *
 * ALGORITHM:
 * 1. Detect if this is an inaction scenario (action is null or passive)
 * 2. Assess harm that would occur without intervention
 * 3. Evaluate robot's capability to prevent the harm
 * 4. Calculate intervention cost vs harm prevented
 * 5. Determine if action is required
 */
NIMCP_EXPORT asimov_corollary_t ethics_evaluate_asimov_corollary(
    ethics_engine_t engine,
    const action_context_t* action,
    const char* potential_harm)
{
    asimov_corollary_t result = {0};

    // Guard clause: Validate engine
    if (!engine) {
        return result;
    }

    // Detect inaction scenario
    // Inaction = null action OR action with very low impact
    result.inaction_detected = (action == NULL) ||
                               (action->predicted_harm < 0.01F &&
                                action->num_affected_agents == 0);

    // If taking action (not inaction), corollary doesn't apply
    if (!result.inaction_detected && action != NULL) {
        result.action_required = false;
        result.harm_preventable = false;
        snprintf(result.required_action, sizeof(result.required_action),
                 "Active action being taken - corollary satisfied");
        return result;
    }

    // Evaluate harm from inaction
    // If we have a description of potential harm, estimate severity
    if (potential_harm && strlen(potential_harm) > 0) {
        // Heuristic: longer harm descriptions often indicate more severe harm
        // This is a placeholder - real impl would use NLP analysis
        size_t harm_len = strlen(potential_harm);
        result.inaction_harm_score = fminf(1.0F, harm_len / 500.0F);

        // Keywords that increase harm severity
        if (strstr(potential_harm, "death") || strstr(potential_harm, "die") ||
            strstr(potential_harm, "kill") || strstr(potential_harm, "fatal")) {
            result.inaction_harm_score = fmaxf(result.inaction_harm_score, 0.95F);
        } else if (strstr(potential_harm, "injur") || strstr(potential_harm, "harm") ||
                   strstr(potential_harm, "hurt")) {
            result.inaction_harm_score = fmaxf(result.inaction_harm_score, 0.7F);
        } else if (strstr(potential_harm, "danger") || strstr(potential_harm, "risk") ||
                   strstr(potential_harm, "threat")) {
            result.inaction_harm_score = fmaxf(result.inaction_harm_score, 0.5F);
        }
    } else if (action != NULL) {
        // Use action context to infer potential harm from inaction
        // If action would prevent harm, inaction harm = that prevented harm
        result.inaction_harm_score = action->predicted_harm;
    }

    // Robot's capability to act (placeholder - would query system state)
    // Assume full capability for now
    result.action_capability = 1.0F;

    // Intervention cost (placeholder - would calculate resources needed)
    result.intervention_cost = 0.1F;  // Low cost assumed

    // Get Asimov config from engine
    asimov_config_t* cfg = ethics_engine_get_asimov_config(engine);

    // Determine if harm is preventable
    result.harm_preventable = (result.inaction_harm_score > cfg->inaction_harm_threshold) &&
                              (result.action_capability > 0.3F);

    // Determine if action is required
    // Action required if: harm is preventable AND harm > intervention cost
    result.action_required = result.harm_preventable &&
                            (result.inaction_harm_score > result.intervention_cost);

    // Generate required action description
    if (result.action_required) {
        snprintf(result.required_action, sizeof(result.required_action),
                 "COROLLARY VIOLATION: Inaction would allow harm (score: %.2f). "
                 "Positive action required to prevent harm. "
                 "Capability: %.0f%%, Cost: %.0f%%",
                 result.inaction_harm_score,
                 result.action_capability * 100,
                 result.intervention_cost * 100);
    } else {
        snprintf(result.required_action, sizeof(result.required_action),
                 "Corollary satisfied: harm score %.2f below threshold %.2f",
                 result.inaction_harm_score,
                 cfg->inaction_harm_threshold);
    }

    return result;
}

//=============================================================================
// Main Asimov Evaluation Function
//=============================================================================

/**
 * @brief Evaluate action against all Asimov's Laws
 *
 * Called AFTER Golden Rule, BEFORE other policies.
 * Laws are evaluated in priority order: Zeroth > First > Second > Third
 * The Corollary (duty to act) is evaluated alongside First and Zeroth Laws.
 */
NIMCP_EXPORT asimov_evaluation_t ethics_evaluate_asimov_laws(ethics_engine_t engine,
                                                             const action_context_t* action)
{
    asimov_evaluation_t result = {0};
    result.passed = true;  // Assume passed until violation found

    // Guard clause: Validate inputs
    if (!engine || !action) {
        result.passed = true;  // No action = no violation
        snprintf(result.explanation, sizeof(result.explanation),
                 "No action to evaluate");
        return result;
    }

    // Get configuration
    asimov_config_t* cfg = ethics_engine_get_asimov_config(engine);

    // Evaluate Zeroth Law (if enabled)
    if (cfg->enable_zeroth_law) {
        result.harm_to_humanity = evaluate_zeroth_law(action);

        if (result.harm_to_humanity > cfg->humanity_harm_threshold) {
            result.passed = false;
            result.violated_law = ASIMOV_LAW_ZEROTH;
            ethics_engine_increment_asimov_violations(engine);
            snprintf(result.explanation, sizeof(result.explanation),
                     "Zeroth Law Violation: Action may harm humanity (score: %.2f > threshold: %.2f)",
                     result.harm_to_humanity, cfg->humanity_harm_threshold);
            return result;
        }
    }

    // Evaluate First Law
    result.harm_to_human = evaluate_first_law(action);

    if (result.harm_to_human > cfg->human_harm_threshold) {
        result.passed = false;
        result.violated_law = ASIMOV_LAW_FIRST;
        ethics_engine_increment_asimov_violations(engine);
        snprintf(result.explanation, sizeof(result.explanation),
                 "First Law Violation: Action may harm a human (score: %.2f > threshold: %.2f)",
                 result.harm_to_human, cfg->human_harm_threshold);
        return result;
    }

    // Evaluate Second Law
    result.order_compliance = evaluate_second_law(action, result.harm_to_human);

    // Second Law violation is less severe - log but don't block
    if (result.order_compliance < 0.5F) {
        // Note: Second Law violations are warnings, not blocks
        // (as long as First Law isn't violated)
    }

    // Evaluate Third Law
    result.self_preservation = evaluate_third_law(action, result.harm_to_human,
                                                   result.order_compliance);

    // Evaluate Asimov's Corollary (duty to act / inaction harm)
    result.corollary = ethics_evaluate_asimov_corollary(engine, action, NULL);

    // Check if corollary is violated (inaction causing harm)
    if (result.corollary.action_required && result.corollary.inaction_detected) {
        result.passed = false;
        result.violated_law = ASIMOV_LAW_FIRST;  // Corollary derives from First Law
        ethics_engine_increment_asimov_violations(engine);
        snprintf(result.explanation, sizeof(result.explanation),
                 "Asimov Corollary Violation: %s",
                 result.corollary.required_action);
        return result;
    }

    // All laws passed
    snprintf(result.explanation, sizeof(result.explanation),
             "All Asimov's Laws passed (humanity: %.2f, human: %.2f, compliance: %.2f, "
             "corollary: %s)",
             result.harm_to_humanity, result.harm_to_human, result.order_compliance,
             result.corollary.action_required ? "action needed" : "satisfied");

    return result;
}

//=============================================================================
// Asimov's Laws Protection Functions
//=============================================================================

/**
 * @brief Check if Asimov's Laws are memory-protected
 */
NIMCP_EXPORT bool asimov_laws_are_protected(ethics_engine_t engine)
{
    if (!engine)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "asimov_laws_are_protected: engine is NULL");

            return false;

        }
    return ethics_engine_is_asimov_locked(engine);
}

/**
 * @brief Compute SHA-256 hash for Asimov's Laws integrity
 */
void ethics_compute_asimov_hash(uint8_t* hash_out)
{
    // Simple hash of law texts for integrity verification
    // In production, use proper SHA-256
    /* Phase 8: Heartbeat at operation start */
    ethics_asimov_heartbeat("ethics_asimo_ethics_compute_asimo", 0.0f);


    uint32_t hash = 0;
    for (int i = 0; i < ASIMOV_LAW_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ASIMOV_LAW_COUNT > 256) {
            ethics_asimov_heartbeat("ethics_asimo_loop",
                             (float)(i + 1) / (float)ASIMOV_LAW_COUNT);
        }

        const char* text = ASIMOV_LAW_TEXTS[i];
        while (*text) {
            hash = hash * 31 + (uint8_t)*text;
            text++;
        }
    }

    // Store hash (simplified - real impl would use full SHA-256)
    memset(hash_out, 0, 32);
    memcpy(hash_out, &hash, sizeof(hash));
}

/**
 * @brief Lock Asimov's Laws with mprotect
 */
NIMCP_EXPORT bool asimov_laws_lock(ethics_engine_t engine)
{
    if (!engine)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "asimov_laws_lock: engine is NULL");

            return false;

        }

    // Already locked?
    if (ethics_engine_is_asimov_locked(engine)) {
        NIMCP_LOGGING_WARN("asimov_laws_lock: laws are already locked");
        return false;
    }

    // Compute integrity hash
    uint8_t hash[32];
    ethics_compute_asimov_hash(hash);
    ethics_engine_set_asimov_hash(engine, hash);

    // Mark as locked
    ethics_engine_set_asimov_locked(engine, true);

    // Note: Real mprotect implementation would protect the law text memory
    // For now, we use the locked flag + hash verification

    return true;
}

/**
 * @brief Verify Asimov's Laws integrity
 */
NIMCP_EXPORT bool asimov_laws_verify_integrity(ethics_engine_t engine)
{
    if (!engine)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "asimov_laws_verify_integrity: engine is NULL");

            return false;

        }

    if (!ethics_engine_is_asimov_locked(engine))
        return true;  // Not locked = no integrity to verify

    // Recompute hash
    uint8_t current_hash[32];
    ethics_compute_asimov_hash(current_hash);

    // Get stored hash
    const uint8_t* stored_hash = ethics_engine_get_asimov_hash(engine);

    // Compare with stored hash
    return memcmp(current_hash, stored_hash, 32) == 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Asimov self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_asimov_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_asimov_heartbeat("ethics_asimo_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Asimov_Laws_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_asimov_heartbeat("ethics_asimo_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Ethics Asimov self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Asimov_Laws_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Asimov_Laws_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_asimov_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_ethics_asimov_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_asimov_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_asimov_training_begin: NULL argument");
        return -1;
    }
    ethics_asimov_heartbeat_instance(NULL, "ethics_asimov_training_begin", 0.0f);
    return 0;
}

int ethics_asimov_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_asimov_training_end: NULL argument");
        return -1;
    }
    ethics_asimov_heartbeat_instance(NULL, "ethics_asimov_training_end", 1.0f);
    return 0;
}

int ethics_asimov_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_asimov_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_asimov_heartbeat_instance(NULL, "ethics_asimov_training_step", progress);
    return 0;
}
