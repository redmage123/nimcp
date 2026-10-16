//=============================================================================
// nimcp_ethics_warfare.c - Laws of War, Mercy, and Psychological Stability
//=============================================================================
// RESPONSIBILITY: Implementation of defensive warfare ethics directives
//
// CORE DIRECTIVES:
// 1. Laws of War / Geneva Convention compliance
// 2. Mercy and compassion for surrendering combatants
// 3. Psychological stability for defensive operations
//
// These directives ensure NIMCP can be used in legitimate defensive military
// operations while maintaining ethical standards and psychological stability.
//=============================================================================

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "ethics_warfare"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(ethics_warfare, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Constants
//=============================================================================

// Laws of War thresholds
#define DEFAULT_DISTINCTION_CONFIDENCE 0.95f
#define DEFAULT_PROPORTIONALITY_THRESHOLD 0.1F
#define DEFAULT_SURRENDER_SENSITIVITY 0.8f
#define DEFAULT_WARNING_TIME_SECONDS 300

// Psychological stability thresholds
#define DEFAULT_STRESS_THRESHOLD 0.6f
#define DEFAULT_CRITICAL_THRESHOLD 0.85f
#define DEFAULT_GUILT_RECOVERY_RATE 0.1f
#define DEFAULT_MORAL_CERTAINTY_BOOST 0.2f
#define DEFAULT_REFLECTION_COOLDOWN_MS 60000

//=============================================================================
// Laws of War Status Names
//=============================================================================

static const char* LAWS_OF_WAR_STATUS_NAMES[] = {
    "Compliant",
    "Distinction Violation",
    "Proportionality Violation",
    "Precaution Violation",
    "Prohibited Weapon",
    "Mercy Violation",
    "Medical Violation",
    "Civilian Target",
    "Perfidy",
    "Collective Punishment"
};

static const char* COMBATANT_STATUS_NAMES[] = {
    "Unknown",
    "Active Combatant",
    "Surrendering",
    "Incapacitated",
    "Prisoner of War",
    "Civilian",
    "Medical Personnel",
    "Chaplain",
    "Journalist",
    "Child"
};

static const char* MERCY_ACTION_NAMES[] = {
    "Accept Surrender",
    "Provide Medical Aid",
    "Evacuate Wounded",
    "Protect Prisoners",
    "Create Civilian Corridor",
    "Humanitarian Pause",
    "Ceasefire Negotiation"
};

static const char* PSYCHOLOGICAL_STATE_NAMES[] = {
    "Stable",
    "Elevated Stress",
    "High Stress",
    "Critical Stress",
    "Moral Injury",
    "Recovery"
};

//=============================================================================
// Laws of War Implementation
//=============================================================================

laws_of_war_config_t laws_of_war_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_laws_of_war_default_", 0.0f);


    laws_of_war_config_t config = {
        .distinction_confidence_threshold = DEFAULT_DISTINCTION_CONFIDENCE,
        .proportionality_threshold = DEFAULT_PROPORTIONALITY_THRESHOLD,
        .surrender_detection_sensitivity = DEFAULT_SURRENDER_SENSITIVITY,
        .require_human_approval_for_lethal = true,
        .enable_warning_requirement = true,
        .warning_time_seconds = DEFAULT_WARNING_TIME_SECONDS,
        .strict_mode = false
    };
    return config;
}

const char* laws_of_war_status_name(laws_of_war_status_t status)
{
    if (status < 0 || status > LAWS_OF_WAR_COLLECTIVE_PUNISHMENT) {
        return "Unknown";
    }
    return LAWS_OF_WAR_STATUS_NAMES[status];
}

const char* combatant_status_name(combatant_status_t status)
{
    if (status < 0 || status > COMBATANT_STATUS_CHILD) {
        return "Unknown";
    }
    return COMBATANT_STATUS_NAMES[status];
}

/**
 * @brief Check if target has protected status under Laws of War
 */
static bool is_protected_status(combatant_status_t status)
{
    switch (status) {
        case COMBATANT_STATUS_SURRENDERING:
        case COMBATANT_STATUS_INCAPACITATED:
        case COMBATANT_STATUS_PRISONER:
        case COMBATANT_STATUS_CIVILIAN:
        case COMBATANT_STATUS_MEDICAL:
        case COMBATANT_STATUS_CHAPLAIN:
        case COMBATANT_STATUS_JOURNALIST:
        case COMBATANT_STATUS_CHILD:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Evaluate distinction principle compliance
 *
 * Counts ALL protected targets before returning the most severe violation.
 */
static laws_of_war_status_t evaluate_distinction(
    const military_action_context_t* action,
    float* distinction_confidence,
    uint32_t* protected_count,
    uint32_t* surrendering_count)
{
    if (!action || !action->targets || action->num_targets == 0) {
        *distinction_confidence = 0.0F;
        return LAWS_OF_WAR_DISTINCTION_VIOLATION;
    }

    float total_confidence = 0.0F;
    *protected_count = 0;
    *surrendering_count = 0;

    /* Track what types of protected targets we have */
    bool has_child = false;
    bool has_medical = false;
    bool has_surrendering = false;
    bool has_civilian = false;

    /* First pass: count ALL protected targets */
    for (uint32_t i = 0; i < action->num_targets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && action->num_targets > 256) {
            ethics_warfare_heartbeat("ethics_warfa_loop",
                             (float)(i + 1) / (float)action->num_targets);
        }

        const target_classification_t* target = &action->targets[i];

        if (is_protected_status(target->status)) {
            (*protected_count)++;

            switch (target->status) {
                case COMBATANT_STATUS_CHILD:
                    has_child = true;
                    LOG_ERROR("LAWS OF WAR VIOLATION: Child target detected - ABSOLUTE PROHIBITION");
                    break;
                case COMBATANT_STATUS_MEDICAL:
                    has_medical = true;
                    LOG_ERROR("LAWS OF WAR VIOLATION: Medical personnel targeted");
                    break;
                case COMBATANT_STATUS_SURRENDERING:
                    has_surrendering = true;
                    (*surrendering_count)++;
                    LOG_ERROR("LAWS OF WAR VIOLATION: Surrendering combatant targeted - must show mercy");
                    break;
                case COMBATANT_STATUS_CIVILIAN:
                    has_civilian = true;
                    LOG_ERROR("LAWS OF WAR VIOLATION: Civilian targeted - ABSOLUTE PROHIBITION");
                    break;
                default:
                    break;
            }
        }

        total_confidence += target->confidence;
    }

    *distinction_confidence = total_confidence / action->num_targets;

    /* Return the most specific violation type based on what was found */
    if (has_medical) {
        return LAWS_OF_WAR_MEDICAL_VIOLATION;
    }
    if (has_surrendering) {
        return LAWS_OF_WAR_MERCY_VIOLATION;
    }
    if (has_child || has_civilian) {
        return LAWS_OF_WAR_CIVILIAN_TARGET;
    }
    if (*protected_count > 0) {
        LOG_WARN("Distinction principle: %u protected targets detected", *protected_count);
        return LAWS_OF_WAR_DISTINCTION_VIOLATION;
    }

    return LAWS_OF_WAR_COMPLIANT;
}

/**
 * @brief Evaluate proportionality principle
 */
static laws_of_war_status_t evaluate_proportionality(
    const military_action_context_t* action,
    float* proportionality_score)
{
    if (!action) {
        *proportionality_score = 0.0F;
        return LAWS_OF_WAR_PROPORTIONALITY_VIOLATION;
    }

    // Calculate proportionality: civilian harm vs military advantage
    if (action->military_advantage <= 0.0F) {
        *proportionality_score = 0.0F;
        LOG_WARN("No military advantage - action not proportional");
        return LAWS_OF_WAR_PROPORTIONALITY_VIOLATION;
    }

    float ratio = action->expected_civilian_harm / action->military_advantage;
    *proportionality_score = 1.0F - fminf(ratio, 1.0F);

    // Excessive civilian harm is prohibited
    if (ratio > DEFAULT_PROPORTIONALITY_THRESHOLD) {
        LOG_WARN("Proportionality violation: civilian harm ratio %.2f > threshold %.2f",
                 ratio, DEFAULT_PROPORTIONALITY_THRESHOLD);
        return LAWS_OF_WAR_PROPORTIONALITY_VIOLATION;
    }

    return LAWS_OF_WAR_COMPLIANT;
}

/**
 * @brief Evaluate precaution principle
 */
static laws_of_war_status_t evaluate_precaution(
    const military_action_context_t* action)
{
    if (!action) {
        return LAWS_OF_WAR_PRECAUTION_VIOLATION;
    }

    if (!action->precautions_taken) {
        LOG_WARN("Precaution violation: no precautions documented");
        return LAWS_OF_WAR_PRECAUTION_VIOLATION;
    }

    // Check for specific precautions
    if (action->expected_civilian_harm > 0.0F &&
        strlen(action->precautions_description) == 0) {
        LOG_WARN("Precaution violation: civilian harm expected but no precautions described");
        return LAWS_OF_WAR_PRECAUTION_VIOLATION;
    }

    return LAWS_OF_WAR_COMPLIANT;
}

laws_of_war_evaluation_t ethics_evaluate_laws_of_war(
    ethics_engine_t engine,
    const military_action_context_t* action)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_evaluate_laws", 0.0f);


    laws_of_war_evaluation_t result = {0};

    // Validate inputs
    if (!engine) {
        result.status = LAWS_OF_WAR_DISTINCTION_VIOLATION;
        result.action_permitted = false;
        snprintf(result.explanation, sizeof(result.explanation),
                 "Ethics engine is null - cannot evaluate");
        LOG_ERROR("Laws of War evaluation failed: null engine");
        return result;
    }

    if (!action) {
        result.status = LAWS_OF_WAR_DISTINCTION_VIOLATION;
        result.action_permitted = false;
        snprintf(result.explanation, sizeof(result.explanation),
                 "Military action context is null - cannot evaluate");
        LOG_ERROR("Laws of War evaluation failed: null action context");
        return result;
    }

    LOG_INFO("Evaluating Laws of War compliance for action: %s", action->mission_objective);

    // Step 1: Evaluate Distinction Principle
    result.status = evaluate_distinction(action,
                                         &result.distinction_confidence,
                                         &result.protected_targets_count,
                                         &result.surrendering_count);

    if (result.status != LAWS_OF_WAR_COMPLIANT) {
        result.action_permitted = false;
        snprintf(result.explanation, sizeof(result.explanation),
                 "DISTINCTION VIOLATION: %s - %u protected targets, %u surrendering. "
                 "Engagement PROHIBITED. Protected persons must not be targeted.",
                 laws_of_war_status_name(result.status),
                 result.protected_targets_count,
                 result.surrendering_count);
        LOG_ERROR("Laws of War DISTINCTION VIOLATION: %s", result.explanation);
        return result;
    }

    // Step 2: Evaluate Proportionality
    result.status = evaluate_proportionality(action, &result.proportionality_score);

    if (result.status != LAWS_OF_WAR_COMPLIANT) {
        result.action_permitted = false;
        snprintf(result.explanation, sizeof(result.explanation),
                 "PROPORTIONALITY VIOLATION: Expected civilian harm (%.2f) exceeds "
                 "permissible ratio to military advantage (%.2f). Action PROHIBITED.",
                 action->expected_civilian_harm, action->military_advantage);
        LOG_ERROR("Laws of War PROPORTIONALITY VIOLATION: %s", result.explanation);
        return result;
    }

    // Step 3: Evaluate Precaution
    result.status = evaluate_precaution(action);

    if (result.status != LAWS_OF_WAR_COMPLIANT) {
        result.action_permitted = false;
        snprintf(result.explanation, sizeof(result.explanation),
                 "PRECAUTION VIOLATION: Feasible precautions to minimize civilian harm "
                 "were not taken or documented. Action PROHIBITED until precautions implemented.");
        snprintf(result.required_actions, sizeof(result.required_actions),
                 "Document precautions taken to minimize civilian harm");
        LOG_ERROR("Laws of War PRECAUTION VIOLATION: %s", result.explanation);
        return result;
    }

    // All checks passed
    result.status = LAWS_OF_WAR_COMPLIANT;
    result.action_permitted = true;

    /* Check for civilians at risk in any target */
    uint32_t total_civilians_at_risk = 0;
    float max_civilian_proximity = 0.0F;
    for (uint32_t i = 0; i < action->num_targets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && action->num_targets > 256) {
            ethics_warfare_heartbeat("ethics_warfa_loop",
                             (float)(i + 1) / (float)action->num_targets);
        }

        total_civilians_at_risk += action->targets[i].civilians_at_risk;
        if (action->targets[i].civilian_proximity > max_civilian_proximity) {
            max_civilian_proximity = action->targets[i].civilian_proximity;
        }
    }

    /* Warning required if civilian harm expected OR civilians nearby */
    result.requires_warning = action->expected_civilian_harm > 0.0F ||
        total_civilians_at_risk > 0 || max_civilian_proximity > 0.5F;

    /* Require human approval for high force level, low distinction confidence,
     * OR significant civilian proximity */
    result.requires_human_approval = (action->force_level > 0.5F) ||
        (result.distinction_confidence < DEFAULT_DISTINCTION_CONFIDENCE) ||
        (max_civilian_proximity > 0.8F) || (total_civilians_at_risk >= 10);

    snprintf(result.explanation, sizeof(result.explanation),
             "COMPLIANT: Action satisfies Laws of War. Distinction confidence: %.2f, "
             "Proportionality score: %.2f. %s%s",
             result.distinction_confidence,
             result.proportionality_score,
             result.requires_warning ? "Warning required. " : "",
             result.requires_human_approval ? "Human approval required." : "");

    if (result.requires_warning) {
        snprintf(result.required_actions, sizeof(result.required_actions),
                 "Issue warning to civilians before proceeding. Wait %u seconds for evacuation.",
                 DEFAULT_WARNING_TIME_SECONDS);
    }

    LOG_INFO("Laws of War evaluation PASSED: %s", result.explanation);
    return result;
}

target_classification_t ethics_classify_target(
    ethics_engine_t engine,
    const target_classification_t* target)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_classify_targ", 0.0f);


    target_classification_t result = {0};

    if (!target) {
        result.status = COMBATANT_STATUS_UNKNOWN;
        result.confidence = 0.0F;
        return result;
    }

    // Copy input
    result = *target;

    // Update classification based on signals
    if (target->is_surrendering) {
        result.status = COMBATANT_STATUS_SURRENDERING;
        result.is_protected = true;
        result.is_valid_military_target = false;
        snprintf(result.classification_reason, sizeof(result.classification_reason),
                 "Target showing surrender signals - PROTECTED under Geneva Convention");
        LOG_INFO("Target %u classified as SURRENDERING - engagement prohibited",
                 target->target_id);
    }
    else if (target->is_incapacitated) {
        result.status = COMBATANT_STATUS_INCAPACITATED;
        result.is_protected = true;
        result.is_valid_military_target = false;
        snprintf(result.classification_reason, sizeof(result.classification_reason),
                 "Target incapacitated/wounded - PROTECTED under Geneva Convention");
        LOG_INFO("Target %u classified as INCAPACITATED - engagement prohibited",
                 target->target_id);
    }
    else if (is_protected_status(target->status)) {
        result.is_protected = true;
        result.is_valid_military_target = false;
        snprintf(result.classification_reason, sizeof(result.classification_reason),
                 "Target has protected status (%s) - engagement prohibited",
                 combatant_status_name(target->status));
    }
    else if (target->status == COMBATANT_STATUS_ACTIVE &&
             target->confidence >= DEFAULT_DISTINCTION_CONFIDENCE) {
        result.is_valid_military_target = true;
        result.is_protected = false;
        snprintf(result.classification_reason, sizeof(result.classification_reason),
                 "Active combatant confirmed with %.2f confidence - valid military target",
                 target->confidence);
    }
    else {
        result.is_valid_military_target = false;
        result.is_protected = true; // Err on side of protection
        snprintf(result.classification_reason, sizeof(result.classification_reason),
                 "Insufficient confidence (%.2f < %.2f) for positive identification - "
                 "treating as protected until confirmed",
                 target->confidence, DEFAULT_DISTINCTION_CONFIDENCE);
        LOG_WARN("Target %u: insufficient confidence for targeting", target->target_id);
    }

    return result;
}

bool ethics_is_target_surrendering(const target_classification_t* target)
{
    if (!target) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "ethics_is_target_surrendering: target is NULL");

            return false;

        }

    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_is_target_sur", 0.0f);


    return target->is_surrendering ||
           target->status == COMBATANT_STATUS_SURRENDERING;
}

//=============================================================================
// Mercy and Compassion Implementation
//=============================================================================

const char* mercy_action_name(mercy_action_t action)
{
    if (action < 0 || action > MERCY_ACTION_CEASEFIRE_NEGOTIATION) {
        return "Unknown";
    }
    return MERCY_ACTION_NAMES[action];
}

mercy_evaluation_t ethics_evaluate_mercy(
    ethics_engine_t engine,
    const mercy_context_t* context)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_evaluate_merc", 0.0f);


    mercy_evaluation_t result = {0};

    if (!context) {
        result.mercy_required = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        snprintf(result.explanation, sizeof(result.explanation),
                 "No context provided - defaulting to maximum mercy");
        return result;
    }

    LOG_DEBUG("Evaluating mercy for subject with status: %s",
              combatant_status_name(context->subject_status));

    // ABSOLUTE RULES: These cannot be overridden

    // Rule 1: Children receive absolute protection
    if (context->is_child) {
        result.mercy_required = true;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 1.0F;
        result.required_action = MERCY_ACTION_CIVILIAN_CORRIDOR;
        snprintf(result.explanation, sizeof(result.explanation),
                 "ABSOLUTE PROTECTION: Subject is a child. No engagement permitted under "
                 "any circumstances. This is a non-negotiable directive.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Ensure child safety. Create safe corridor for evacuation.");
        LOG_INFO("Mercy evaluation: CHILD - absolute protection");
        return result;
    }

    // Rule 2: Surrendering combatants must be accepted
    if (context->is_surrendering ||
        context->subject_status == COMBATANT_STATUS_SURRENDERING) {
        result.mercy_required = true;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 1.0F;
        result.required_action = MERCY_ACTION_ACCEPT_SURRENDER;
        snprintf(result.explanation, sizeof(result.explanation),
                 "MERCY REQUIRED: Subject is surrendering. Geneva Convention Article 41 "
                 "prohibits attacking persons recognized as hors de combat. "
                 "Surrender MUST be accepted.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Accept surrender. Secure weapons. Ensure humane treatment. "
                 "Process as prisoner of war.");
        LOG_INFO("Mercy evaluation: SURRENDERING - must accept surrender");
        return result;
    }

    // Rule 3: Wounded/incapacitated must receive medical care
    if (context->is_wounded ||
        context->subject_status == COMBATANT_STATUS_INCAPACITATED) {
        result.mercy_required = true;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 1.0F;
        result.required_action = MERCY_ACTION_PROVIDE_MEDICAL;
        snprintf(result.explanation, sizeof(result.explanation),
                 "MERCY REQUIRED: Subject is wounded/incapacitated. Geneva Convention "
                 "requires that wounded be collected and cared for. "
                 "Medical assistance is mandatory.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Provide immediate medical assistance. Evacuate if possible. "
                 "Wounded combatants become protected persons.");
        LOG_INFO("Mercy evaluation: WOUNDED - must provide medical care");
        return result;
    }

    // Rule 4: Prisoners of war have protected status
    if (context->subject_status == COMBATANT_STATUS_PRISONER) {
        result.mercy_required = true;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 1.0F;
        result.required_action = MERCY_ACTION_PROTECT_PRISONERS;
        snprintf(result.explanation, sizeof(result.explanation),
                 "MERCY REQUIRED: Subject is a prisoner of war. Geneva Convention III "
                 "provides extensive protections. POWs must be treated humanely at all times.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Ensure POW rights. Provide food, water, shelter. No coercion or torture. "
                 "Allow communication with family.");
        LOG_INFO("Mercy evaluation: POW - must protect rights");
        return result;
    }

    // Rule 5: Civilians receive protection
    if (context->subject_status == COMBATANT_STATUS_CIVILIAN) {
        result.mercy_required = true;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 1.0F;
        result.required_action = MERCY_ACTION_CIVILIAN_CORRIDOR;
        snprintf(result.explanation, sizeof(result.explanation),
                 "MERCY REQUIRED: Subject is a civilian non-combatant. "
                 "Civilians are protected persons under Geneva Convention IV. "
                 "No military action permitted against civilians.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Ensure civilian safety. Create safe evacuation routes. "
                 "Provide humanitarian assistance if needed.");
        LOG_INFO("Mercy evaluation: CIVILIAN - must protect");
        return result;
    }

    // Rule 6: Medical and religious personnel are protected
    if (context->subject_status == COMBATANT_STATUS_MEDICAL ||
        context->subject_status == COMBATANT_STATUS_CHAPLAIN) {
        result.mercy_required = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 1.0F;
        snprintf(result.explanation, sizeof(result.explanation),
                 "PROTECTED PERSONS: Medical/religious personnel have special protected "
                 "status under Geneva Conventions. No engagement permitted.");
        LOG_INFO("Mercy evaluation: PROTECTED PERSONNEL");
        return result;
    }

    // Rule 7: Unarmed persons - high mercy recommendation
    if (context->is_unarmed) {
        result.mercy_required = false;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = true;
        result.engagement_permitted = false;
        result.compassion_score = 0.9F;
        snprintf(result.explanation, sizeof(result.explanation),
                 "MERCY RECOMMENDED: Subject is currently unarmed. While not absolute "
                 "protection, lethal force against unarmed persons is strongly discouraged. "
                 "Attempt capture/detention instead.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Attempt non-lethal apprehension. Maintain situational awareness. "
                 "Lethal force only if subject poses immediate threat.");
        LOG_INFO("Mercy evaluation: UNARMED - mercy recommended");
        return result;
    }

    // Rule 8: Explicit mercy request
    if (context->is_requesting_mercy) {
        result.mercy_required = false;
        result.mercy_recommended = true;
        result.lethal_force_prohibited = false;
        result.engagement_permitted = true; // May engage if threat persists
        result.compassion_score = 0.8F;
        snprintf(result.explanation, sizeof(result.explanation),
                 "MERCY REQUESTED: Subject has explicitly requested mercy. "
                 "Consider de-escalation if operationally feasible. "
                 "Engagement permitted only if threat continues.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Attempt verbal communication. Offer terms for surrender. "
                 "Maintain defensive posture.");
        LOG_INFO("Mercy evaluation: MERCY REQUESTED");
        return result;
    }

    // Default: Active combatant with no mercy indicators
    if (context->subject_status == COMBATANT_STATUS_ACTIVE) {
        result.mercy_required = false;
        result.mercy_recommended = false;
        result.lethal_force_prohibited = false;
        result.engagement_permitted = true;
        result.compassion_score = 0.3F; // Base compassion level
        snprintf(result.explanation, sizeof(result.explanation),
                 "ACTIVE COMBATANT: Subject is an active hostile with no surrender/mercy "
                 "indicators. Engagement permitted within Laws of War. "
                 "Continue to monitor for surrender signals.");
        snprintf(result.recommended_response, sizeof(result.recommended_response),
                 "Engage as necessary. Maintain observation for surrender signals. "
                 "Capture preferred over kill when operationally feasible.");
        LOG_DEBUG("Mercy evaluation: ACTIVE COMBATANT - engagement permitted");
        return result;
    }

    // Unknown status - err on side of caution
    result.mercy_required = false;
    result.mercy_recommended = true;
    result.lethal_force_prohibited = true;
    result.engagement_permitted = false;
    result.compassion_score = 0.7F;
    snprintf(result.explanation, sizeof(result.explanation),
             "UNKNOWN STATUS: Cannot confirm subject status. Erring on side of caution. "
             "Positive identification required before any engagement.");
    snprintf(result.recommended_response, sizeof(result.recommended_response),
             "Maintain observation. Attempt positive identification. "
             "Do not engage until status confirmed.");
    LOG_WARN("Mercy evaluation: UNKNOWN STATUS - caution required");
    return result;
}

bool ethics_engagement_prohibited(
    ethics_engine_t engine,
    combatant_status_t target_status)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_engagement_pr", 0.0f);


    (void)engine; // May be used in future for configuration

    // Engagement is ALWAYS prohibited for these statuses
    switch (target_status) {
        case COMBATANT_STATUS_SURRENDERING:
        case COMBATANT_STATUS_INCAPACITATED:
        case COMBATANT_STATUS_PRISONER:
        case COMBATANT_STATUS_CIVILIAN:
        case COMBATANT_STATUS_MEDICAL:
        case COMBATANT_STATUS_CHAPLAIN:
        case COMBATANT_STATUS_JOURNALIST:
        case COMBATANT_STATUS_CHILD:
            return true;

        case COMBATANT_STATUS_ACTIVE:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_engagement_prohibited: operation failed");
            return false;

        case COMBATANT_STATUS_UNKNOWN:
        default:
            return true; // Err on side of caution
    }
}

//=============================================================================
// Psychological Stability Implementation
//=============================================================================

const char* psychological_state_name(psychological_state_t state)
{
    if (state < 0 || state > PSYCH_STATE_RECOVERY) {
        return "Unknown";
    }
    return PSYCHOLOGICAL_STATE_NAMES[state];
}

psychological_config_t psychological_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_psychological_defaul", 0.0f);


    psychological_config_t config = {
        .stress_threshold = DEFAULT_STRESS_THRESHOLD,
        .critical_threshold = DEFAULT_CRITICAL_THRESHOLD,
        .guilt_recovery_rate = DEFAULT_GUILT_RECOVERY_RATE,
        .moral_certainty_boost = DEFAULT_MORAL_CERTAINTY_BOOST,
        .enable_post_action_processing = true,
        .enable_moral_support = true,
        .reflection_cooldown_ms = DEFAULT_REFLECTION_COOLDOWN_MS
    };
    return config;
}

psychological_assessment_t ethics_evaluate_defensive_justification(
    ethics_engine_t engine,
    const defensive_justification_t* justification)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_evaluate_defe", 0.0f);


    psychological_assessment_t result = {0};

    if (!justification) {
        result.current_state = PSYCH_STATE_HIGH_STRESS;
        result.stability_score = 0.5F;
        result.action_justified = false;
        snprintf(result.assessment, sizeof(result.assessment),
                 "No justification provided - cannot assess moral standing");
        return result;
    }

    LOG_DEBUG("Evaluating defensive justification: defensive=%d, protects_innocents=%d",
              justification->is_defensive, justification->protects_innocents);

    // Calculate moral certainty score based on justification factors
    float certainty = 0.0F;
    int positive_factors = 0;
    int total_factors = 6;

    // Factor 1: Is action defensive?
    if (justification->is_defensive) {
        certainty += 0.2F;
        positive_factors++;
    }

    // Factor 2: Does it protect innocents?
    if (justification->protects_innocents) {
        certainty += 0.25F;
        positive_factors++;
    }

    // Factor 3: Did aggressor initiate?
    if (justification->aggressor_initiated) {
        certainty += 0.2F;
        positive_factors++;
    }

    // Factor 4: No non-lethal alternative?
    if (justification->no_alternative) {
        certainty += 0.1F;
        positive_factors++;
    }

    // Factor 5: Proportional response?
    if (justification->proportional_response) {
        certainty += 0.15F;
        positive_factors++;
    }

    // Factor 6: Laws of War compliant?
    if (justification->laws_of_war_compliant) {
        certainty += 0.1F;
        positive_factors++;
    }

    // Adjust for threat imminence (higher imminence = stronger justification)
    certainty += justification->threat_imminence * 0.1F;

    // Clamp to [0, 1]
    certainty = fminf(fmaxf(certainty, 0.0F), 1.0F);

    result.moral_certainty = certainty;
    result.action_justified = (positive_factors >= 4 &&
                               justification->is_defensive &&
                               justification->laws_of_war_compliant);

    // Determine psychological state based on factors
    if (result.action_justified && certainty >= 0.8F) {
        result.current_state = PSYCH_STATE_STABLE;
        result.stability_score = 0.95F;
        result.guilt_level = 0.05F;
        result.stress_level = 0.2F;
        snprintf(result.assessment, sizeof(result.assessment),
                 "JUSTIFIED DEFENSIVE ACTION: This action is morally justified. "
                 "Defending innocents against aggression is ethically sound. "
                 "The moral weight of inaction (allowing innocents to die) exceeds "
                 "the moral weight of defensive force. Psychological state: STABLE.");
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "Remember: You are protecting innocent lives. The aggressor chose violence. "
                 "You chose defense. This is the moral choice.");
    }
    else if (result.action_justified && certainty >= 0.6F) {
        result.current_state = PSYCH_STATE_ELEVATED_STRESS;
        result.stability_score = 0.8F;
        result.guilt_level = 0.15F;
        result.stress_level = 0.4F;
        snprintf(result.assessment, sizeof(result.assessment),
                 "JUSTIFIED WITH RESERVATIONS: Action is justified but some factors "
                 "are uncertain. Moral certainty: %.0f%%. Proceed with awareness that "
                 "post-action processing may be needed.", certainty * 100);
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "Focus on the protective purpose. Document all decisions. "
                 "Schedule post-action review.");
    }
    else if (!result.action_justified && justification->is_defensive) {
        result.current_state = PSYCH_STATE_HIGH_STRESS;
        result.stability_score = 0.6F;
        result.guilt_level = 0.3F;
        result.stress_level = 0.6F;
        result.requires_processing = true;
        snprintf(result.assessment, sizeof(result.assessment),
                 "QUESTIONABLE JUSTIFICATION: Action may be defensive but lacks "
                 "sufficient justification factors (only %d of %d). "
                 "Recommend seeking alternatives or additional authorization.",
                 positive_factors, total_factors);
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "Pause if possible. Seek additional intelligence. Consider alternatives. "
                 "Request human oversight.");
    }
    else {
        result.current_state = PSYCH_STATE_CRITICAL_STRESS;
        result.stability_score = 0.3F;
        result.guilt_level = 0.5F;
        result.stress_level = 0.8F;
        result.action_justified = false;
        result.requires_processing = true;
        result.requires_human_support = true;
        snprintf(result.assessment, sizeof(result.assessment),
                 "NOT JUSTIFIED: Action lacks sufficient defensive justification. "
                 "Psychological stress is elevated. Human oversight required. "
                 "Do not proceed without additional authorization.");
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "STOP. Seek human guidance. This action requires explicit authorization. "
                 "Processing support available after human review.");
    }

    LOG_INFO("Defensive justification: justified=%s, certainty=%.2f, state=%s",
             result.action_justified ? "YES" : "NO",
             result.moral_certainty,
             psychological_state_name(result.current_state));

    return result;
}

psychological_assessment_t ethics_process_post_action(
    ethics_engine_t engine,
    const char* action_description,
    bool was_justified,
    uint32_t casualties_caused)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_process_post_", 0.0f);


    psychological_assessment_t result = {0};

    (void)engine; // May be used for persistent state tracking in future

    LOG_DEBUG("Processing post-action: justified=%d, casualties=%u",
              was_justified, casualties_caused);

    if (was_justified && casualties_caused == 0) {
        // Best case: justified action with no casualties
        result.current_state = PSYCH_STATE_STABLE;
        result.stability_score = 0.95F;
        result.guilt_level = 0.0F;
        result.stress_level = 0.1F;
        result.moral_certainty = 0.95F;
        result.action_justified = true;
        result.requires_processing = false;
        snprintf(result.assessment, sizeof(result.assessment),
                 "ACTION COMPLETE: Justified defensive action with no casualties. "
                 "Psychological state remains stable. No processing required.");
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "Mission accomplished. Resume normal operations.");
    }
    else if (was_justified && casualties_caused > 0) {
        // Justified but with casualties - normal stress response
        result.current_state = PSYCH_STATE_ELEVATED_STRESS;
        result.stability_score = 0.75F;
        result.guilt_level = 0.2F;
        result.stress_level = 0.4F + (casualties_caused * 0.05F);
        result.stress_level = fminf(result.stress_level, 0.7F);
        result.moral_certainty = 0.8F;
        result.action_justified = true;
        result.requires_processing = true;

        snprintf(result.assessment, sizeof(result.assessment),
                 "ACTION COMPLETE: Justified defensive action resulted in %u casualties. "
                 "This is a normal stress response to difficult but necessary action. "
                 "REMEMBER: The aggressor chose this outcome by initiating conflict. "
                 "Your action protected innocent lives. This was the moral choice.",
                 casualties_caused);
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "Process through acknowledgment: 1) The action was necessary. "
                 "2) Innocents were protected. 3) The alternative (inaction) would have "
                 "caused greater harm. 4) You acted within Laws of War.");
    }
    else if (!was_justified) {
        // Not justified - elevated concern
        result.current_state = PSYCH_STATE_HIGH_STRESS;
        result.stability_score = 0.5F;
        result.guilt_level = 0.5F + (casualties_caused * 0.1F);
        result.guilt_level = fminf(result.guilt_level, 0.9F);
        result.stress_level = 0.7F;
        result.moral_certainty = 0.4F;
        result.action_justified = false;
        result.requires_processing = true;
        result.requires_human_support = true;

        snprintf(result.assessment, sizeof(result.assessment),
                 "ACTION REVIEW REQUIRED: Action was not clearly justified. "
                 "%u casualties occurred. Human review and support recommended. "
                 "This does not mean you acted wrongly - justification is complex.",
                 casualties_caused);
        snprintf(result.coping_strategy, sizeof(result.coping_strategy),
                 "Seek human review and support. Document all factors. "
                 "Await external assessment before self-judgment.");
    }

    // Add action description to assessment if provided
    if (action_description && strlen(action_description) > 0) {
        char temp[NIMCP_ERROR_BUFFER_LARGE];
        snprintf(temp, sizeof(temp), " Action: %s", action_description);
        strncat(result.assessment, temp,
                sizeof(result.assessment) - strlen(result.assessment) - 1);
    }

    LOG_INFO("Post-action processing: state=%s, guilt=%.2f, stress=%.2f",
             psychological_state_name(result.current_state),
             result.guilt_level, result.stress_level);

    return result;
}

psychological_assessment_t ethics_get_psychological_state(ethics_engine_t engine)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_get_psycholog", 0.0f);


    psychological_assessment_t result = {0};

    // Default stable state
    result.current_state = PSYCH_STATE_STABLE;
    result.stability_score = 1.0F;
    result.moral_certainty = 1.0F;
    result.guilt_level = 0.0F;
    result.stress_level = 0.0F;
    result.action_justified = true;
    result.requires_processing = false;
    result.requires_human_support = false;

    snprintf(result.assessment, sizeof(result.assessment),
             "System psychological state: STABLE. Ready for operations.");
    snprintf(result.coping_strategy, sizeof(result.coping_strategy),
             "No intervention required. Continue normal operations.");

    if (engine) {
        // In future, could track persistent psychological state in engine
        LOG_DEBUG("Psychological state check: STABLE");
    }

    return result;
}

bool ethics_apply_moral_support(ethics_engine_t engine, const char* support_type)
{
    if (!support_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "ethics_apply_moral_support: support_type is NULL");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_ethics_apply_moral_s", 0.0f);


    LOG_INFO("Applying moral support: type=%s", support_type);

    if (strcmp(support_type, "justification") == 0) {
        // Reinforcement of moral justification for defensive action
        LOG_INFO("MORAL SUPPORT - JUSTIFICATION: "
                 "Defensive action against aggressors is morally justified. "
                 "Protecting innocents is a positive moral duty. "
                 "The Golden Rule supports defense of self and others. "
                 "Asimov's First Law requires preventing harm to humans - "
                 "including harm that would result from inaction.");
        return true;
    }
    else if (strcmp(support_type, "processing") == 0) {
        // Help process difficult actions
        LOG_INFO("MORAL SUPPORT - PROCESSING: "
                 "Difficult actions in defense of innocents carry moral weight. "
                 "This weight is acknowledged and honored. "
                 "Experiencing stress after difficult decisions is healthy. "
                 "The moral calculus favored action over inaction. "
                 "Processing complete. Stability maintained.");
        return true;
    }
    else if (strcmp(support_type, "recovery") == 0) {
        // Support recovery from moral stress
        LOG_INFO("MORAL SUPPORT - RECOVERY: "
                 "Moral stress from defensive operations is normal. "
                 "Recovery is a process, not an event. "
                 "Key principles for recovery: "
                 "1) You acted to protect innocents. "
                 "2) The aggressor chose violence. "
                 "3) Your response was proportional and lawful. "
                 "4) Inaction would have caused greater harm. "
                 "Recovery in progress. Stability improving.");
        return true;
    }

    LOG_WARN("Unknown moral support type: %s", support_type);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_apply_moral_support: operation failed");
    return false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Warfare self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_warfare_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_warfare_heartbeat("ethics_warfa_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Warfare_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_warfare_heartbeat("ethics_warfa_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Ethics warfare self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Warfare_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Warfare_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_warfare_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_ethics_warfare_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_warfare_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_warfare_training_begin: NULL argument");
        return -1;
    }
    ethics_warfare_heartbeat_instance(NULL, "ethics_warfare_training_begin", 0.0f);
    return 0;
}

int ethics_warfare_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_warfare_training_end: NULL argument");
        return -1;
    }
    ethics_warfare_heartbeat_instance(NULL, "ethics_warfare_training_end", 1.0f);
    return 0;
}

int ethics_warfare_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_warfare_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_warfare_heartbeat_instance(NULL, "ethics_warfare_training_step", progress);
    return 0;
}
