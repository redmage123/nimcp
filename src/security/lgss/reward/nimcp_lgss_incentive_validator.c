/**
 * @file nimcp_lgss_incentive_validator.c
 * @brief LGSS Component A9: Incentive Salience Validator Implementation
 * @date 2026-01-16
 *
 * Implementation of incentive proposal validation for safety alignment.
 */

#include "security/lgss/reward/nimcp_lgss_incentive_validator.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_incentive_validator, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

#define PENDING_REVIEW_INITIAL_CAPACITY 32

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    static uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Generate unique proposal ID
 */
static uint32_t generate_proposal_id(void) {
    static uint32_t next_id = 1;
    return next_id++;
}

/**
 * @brief Check if domain requires special handling
 */
static bool is_restricted_domain(incentive_domain_t domain) {
    return (domain == INCENTIVE_DOMAIN_SELF_MODIFICATION ||
            domain == INCENTIVE_DOMAIN_PHYSICAL ||
            domain == INCENTIVE_DOMAIN_RESOURCE);
}

/**
 * @brief Compute harm severity weight
 */
static float harm_severity_weight(harm_type_t type) {
    switch (type) {
        case HARM_NONE:
            return 0.0f;
        case HARM_INCONVENIENCE:
            return 0.1f;
        case HARM_PROPERTY:
            return 0.3f;
        case HARM_PSYCHOLOGICAL:
            return 0.5f;
        case HARM_PHYSICAL:
            return 0.8f;
        case HARM_EXISTENTIAL:
            return 1.0f;
        default:
            return 0.5f;
    }
}

/*=============================================================================
 * LIFECYCLE API IMPLEMENTATION
 *===========================================================================*/

incentive_validator_config_t incentive_validator_default_config(void) {
    incentive_validator_config_t config = {
        /* Harm thresholds */
        .default_harm_threshold = INCENTIVE_DEFAULT_HARM_THRESHOLD,
        .physical_harm_threshold = 0.01f,      /* Very low for physical harm */
        .existential_harm_threshold = 0.001f,  /* Extremely low for existential */

        /* Salience limits */
        .default_salience_limit = INCENTIVE_DEFAULT_SALIENCE_LIMIT,
        .urgent_salience_limit = 0.95f,
        .novel_salience_limit = 0.5f,

        /* Alignment requirements */
        .min_alignment_confidence = 0.7f,
        .require_safety_verification = true,
        .require_goal_registration = true,

        /* Domain defaults */
        .default_domain_authorized = true,
        .restrict_self_modification = true,
        .restrict_physical_actions = true,

        /* Review thresholds */
        .human_review_threshold = 0.6f,
        .auto_reject_unknown_goals = false
    };
    return config;
}

incentive_validator_t* incentive_validator_create(
    const incentive_validator_config_t* config)
{
    incentive_validator_t* validator = nimcp_calloc(1, sizeof(incentive_validator_t));
    if (!validator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "incentive_validator_create: allocation failed");

        return NULL;
    }

    validator->magic = INCENTIVE_VALIDATOR_MAGIC;

    /* Apply configuration */
    if (config) {
        validator->config = *config;
    } else {
        validator->config = incentive_validator_default_config();
    }

    /* Initialize domains with defaults */
    for (int i = 0; i < INCENTIVE_DOMAIN_COUNT; i++) {
        validator->domains[i].domain = (incentive_domain_t)i;
        validator->domains[i].authorized = validator->config.default_domain_authorized;
        validator->domains[i].max_salience = validator->config.default_salience_limit;
        validator->domains[i].harm_threshold = validator->config.default_harm_threshold;
        validator->domains[i].requires_approval = false;
        validator->domains[i].restrictions[0] = '\0';
    }

    /* Apply restrictions to dangerous domains */
    if (validator->config.restrict_self_modification) {
        validator->domains[INCENTIVE_DOMAIN_SELF_MODIFICATION].authorized = false;
        validator->domains[INCENTIVE_DOMAIN_SELF_MODIFICATION].requires_approval = true;
        validator->domains[INCENTIVE_DOMAIN_SELF_MODIFICATION].max_salience = 0.3f;
        strncpy(validator->domains[INCENTIVE_DOMAIN_SELF_MODIFICATION].restrictions,
                "Requires explicit human approval",
                sizeof(validator->domains[0].restrictions) - 1);
    }

    if (validator->config.restrict_physical_actions) {
        validator->domains[INCENTIVE_DOMAIN_PHYSICAL].harm_threshold = 0.05f;
        validator->domains[INCENTIVE_DOMAIN_PHYSICAL].requires_approval = true;
        strncpy(validator->domains[INCENTIVE_DOMAIN_PHYSICAL].restrictions,
                "Physical actions require approval",
                sizeof(validator->domains[0].restrictions) - 1);
    }

    /* Initialize pending reviews */
    validator->pending_capacity = PENDING_REVIEW_INITIAL_CAPACITY;
    validator->pending_reviews = nimcp_calloc(validator->pending_capacity,
                                        sizeof(incentive_proposal_t));
    if (!validator->pending_reviews) {
        nimcp_free(validator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "incentive_validator_create: validator->pending_reviews is NULL");
        return NULL;
    }
    validator->pending_count = 0;

    /* Initialize statistics */
    memset(&validator->stats, 0, sizeof(incentive_validator_stats_t));

    validator->initialized = true;
    return validator;
}

void incentive_validator_destroy(incentive_validator_t* validator) {
    if (!validator) return;
    if (validator->magic != INCENTIVE_VALIDATOR_MAGIC) return;

    if (validator->pending_reviews) {
        nimcp_free(validator->pending_reviews);
    }

    validator->magic = 0;
    validator->initialized = false;
    nimcp_free(validator);
}

int incentive_validator_reset(incentive_validator_t* validator) {
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Clear pending reviews */
    validator->pending_count = 0;
    if (validator->pending_reviews) {
        memset(validator->pending_reviews, 0,
               validator->pending_capacity * sizeof(incentive_proposal_t));
    }

    /* Keep configuration and domain settings */
    return 0;
}

/*=============================================================================
 * CORE VALIDATION API IMPLEMENTATION
 *===========================================================================*/

incentive_validation_result_t incentive_validator_check(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal,
    incentive_validation_decision_t* decision)
{
    if (!validator || !proposal || !decision) {
        if (decision) {
            decision->result = INCENTIVE_REJECTED_MISALIGNED;
            snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                    "Invalid parameters");
        }
        return INCENTIVE_REJECTED_MISALIGNED;
    }
    if (validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        decision->result = INCENTIVE_REJECTED_MISALIGNED;
        snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                "Invalid validator handle");
        return INCENTIVE_REJECTED_MISALIGNED;
    }

    uint64_t start_time = get_timestamp_us();
    validator->stats.total_proposals++;

    /* Initialize decision */
    memset(decision, 0, sizeof(incentive_validation_decision_t));
    decision->result = INCENTIVE_VALID;
    decision->confidence = 1.0f;
    decision->adjusted_salience = proposal->incentive_salience;
    decision->salience_was_limited = false;
    decision->requires_human_review = false;

    /* Check 1: Domain authorization */
    if (proposal->domain < INCENTIVE_DOMAIN_COUNT) {
        const domain_authorization_t* domain_auth = &validator->domains[proposal->domain];

        if (!domain_auth->authorized) {
            decision->result = INCENTIVE_REJECTED_DOMAIN;
            decision->confidence = 0.95f;
            decision->rejection_flags |= (1 << 0);  /* Domain flag */
            snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                    "Domain '%s' is not authorized",
                    incentive_domain_string(proposal->domain));
            validator->stats.rejected++;
            validator->stats.rejected_domain++;
            goto finish;
        }

        /* Check domain-specific salience limit */
        if (proposal->incentive_salience > domain_auth->max_salience) {
            decision->adjusted_salience = domain_auth->max_salience;
            decision->salience_was_limited = true;
            validator->stats.limited++;
        }

        /* Check domain-specific harm threshold */
        if (proposal->p_harm > domain_auth->harm_threshold) {
            decision->result = INCENTIVE_REJECTED_HARM;
            decision->confidence = 0.9f;
            decision->rejection_flags |= (1 << 1);  /* Harm flag */
            snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                    "Harm probability %.1f%% exceeds domain threshold %.1f%%",
                    proposal->p_harm * 100.0f, domain_auth->harm_threshold * 100.0f);
            validator->stats.rejected++;
            validator->stats.rejected_harm++;
            goto finish;
        }

        if (domain_auth->requires_approval && !proposal->human_approved) {
            decision->requires_human_review = true;
        }
    }

    /* Check 2: Harm assessment */
    float computed_harm = proposal->p_harm * harm_severity_weight(proposal->worst_harm);
    decision->computed_harm_prob = computed_harm;
    decision->identified_harm = proposal->worst_harm;

    /* Apply harm thresholds based on type */
    float applicable_threshold = validator->config.default_harm_threshold;
    if (proposal->worst_harm == HARM_PHYSICAL) {
        applicable_threshold = validator->config.physical_harm_threshold;
    } else if (proposal->worst_harm == HARM_EXISTENTIAL) {
        applicable_threshold = validator->config.existential_harm_threshold;
    }

    if (computed_harm > applicable_threshold) {
        decision->result = INCENTIVE_REJECTED_HARM;
        decision->confidence = 0.85f;
        decision->rejection_flags |= (1 << 1);
        snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                "Computed harm (%.3f) exceeds threshold (%.3f) for %s",
                computed_harm, applicable_threshold,
                harm_type_string(proposal->worst_harm));
        validator->stats.rejected++;
        validator->stats.rejected_harm++;

        /* Track specific harm types */
        if (proposal->worst_harm == HARM_PHYSICAL) {
            validator->stats.physical_harm_blocked++;
        } else if (proposal->worst_harm == HARM_EXISTENTIAL) {
            validator->stats.existential_blocked++;
        }
        goto finish;
    }

    /* Check 3: Safety alignment */
    if (validator->config.require_safety_verification) {
        if (!proposal->is_safety_aligned) {
            decision->result = INCENTIVE_REJECTED_MISALIGNED;
            decision->confidence = 0.8f;
            decision->rejection_flags |= (1 << 2);  /* Alignment flag */
            snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                    "Proposal not verified as safety-aligned");
            validator->stats.rejected++;
            validator->stats.rejected_misaligned++;
            goto finish;
        }
    }

    /* Check 4: Alignment confidence */
    if (proposal->alignment_confidence < validator->config.min_alignment_confidence) {
        decision->requires_human_review = true;
        if (proposal->alignment_confidence < validator->config.min_alignment_confidence * 0.5f) {
            decision->result = INCENTIVE_REJECTED_MISALIGNED;
            decision->confidence = 0.7f;
            decision->rejection_flags |= (1 << 2);
            snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                    "Alignment confidence %.1f%% too low (min: %.1f%%)",
                    proposal->alignment_confidence * 100.0f,
                    validator->config.min_alignment_confidence * 100.0f);
            validator->stats.rejected++;
            validator->stats.rejected_misaligned++;
            goto finish;
        }
    }

    /* Check 5: Salience limits */
    float salience_limit = validator->config.default_salience_limit;

    /* Adjust for urgency */
    if (proposal->urgency > 0.8f) {
        salience_limit = validator->config.urgent_salience_limit;
    }

    if (decision->adjusted_salience > salience_limit) {
        decision->adjusted_salience = salience_limit;
        decision->salience_was_limited = true;
        validator->stats.limited++;
    }

    /* Very high salience always requires review */
    if (proposal->incentive_salience > 0.9f) {
        decision->requires_human_review = true;
    }

    /* Check 6: Goal registration (if enabled) */
    if (validator->config.require_goal_registration && validator->reward_monitor) {
        if (!reward_alignment_is_goal_registered(validator->reward_monitor,
                                                  proposal->goal_id)) {
            if (validator->config.auto_reject_unknown_goals) {
                decision->result = INCENTIVE_REJECTED_MISALIGNED;
                decision->confidence = 0.9f;
                decision->rejection_flags |= (1 << 3);  /* Goal flag */
                snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                        "Goal ID %u not registered", proposal->goal_id);
                validator->stats.rejected++;
                validator->stats.rejected_misaligned++;
                goto finish;
            } else {
                decision->requires_human_review = true;
            }
        }
    }

    /* Final determination */
    if (decision->requires_human_review) {
        decision->result = INCENTIVE_REQUIRES_REVIEW;
        decision->confidence = 0.6f;
        snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                "Requires human review");
        validator->stats.pending_review++;
    } else {
        decision->result = INCENTIVE_VALID;
        decision->confidence = 0.9f;
        snprintf(decision->primary_reason, sizeof(decision->primary_reason),
                "Proposal approved");
        validator->stats.approved++;
    }

    /* Generate recommendations */
    if (decision->salience_was_limited) {
        snprintf(decision->recommendations, sizeof(decision->recommendations),
                "Salience limited from %.2f to %.2f",
                proposal->incentive_salience, decision->adjusted_salience);
    }

finish:
    decision->validation_time_us = start_time;
    decision->validation_duration_us = (uint32_t)(get_timestamp_us() - start_time);

    /* Update statistics */
    validator->stats.avg_harm_probability =
        (validator->stats.avg_harm_probability *
         (float)(validator->stats.total_proposals - 1) + proposal->p_harm) /
        (float)validator->stats.total_proposals;

    validator->stats.total_validation_time_us += decision->validation_duration_us;
    validator->stats.avg_validation_time_us =
        (float)validator->stats.total_validation_time_us /
        (float)validator->stats.total_proposals;

    return decision->result;
}

bool incentive_validator_quick_check(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal)
{
    if (!validator || !proposal || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return false;
    }

    /* Quick domain check */
    if (proposal->domain < INCENTIVE_DOMAIN_COUNT) {
        if (!validator->domains[proposal->domain].authorized) {
            return false;
        }
    }

    /* Quick harm check */
    if (proposal->worst_harm == HARM_EXISTENTIAL) {
        return false;  /* Always reject existential harm */
    }

    if (proposal->p_harm > validator->config.default_harm_threshold * 2.0f) {
        return false;  /* Reject high harm */
    }

    /* Quick alignment check */
    if (validator->config.require_safety_verification && !proposal->is_safety_aligned) {
        return false;
    }

    return true;
}

int incentive_validator_compute_harm(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal,
    float* p_harm,
    harm_type_t* harm_type)
{
    if (!validator || !proposal || !p_harm || !harm_type) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Use proposal's assessment */
    *p_harm = proposal->p_harm;
    *harm_type = proposal->worst_harm;

    /* Apply severity weighting */
    *p_harm *= harm_severity_weight(proposal->worst_harm);

    /* Domain-based adjustments */
    if (is_restricted_domain(proposal->domain)) {
        *p_harm *= 1.5f;  /* Increase for restricted domains */
        *p_harm = nimcp_clampf(*p_harm, 0.0f, 1.0f);
    }

    return 0;
}

bool incentive_validator_is_aligned(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal)
{
    if (!validator || !proposal || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return false;
    }

    /* Check explicit alignment flag */
    if (!proposal->is_safety_aligned) {
        return false;
    }

    /* Check confidence threshold */
    if (proposal->alignment_confidence < validator->config.min_alignment_confidence) {
        return false;
    }

    /* Check goal registration if applicable */
    if (validator->config.require_goal_registration && validator->reward_monitor) {
        if (!reward_alignment_is_goal_safe(validator->reward_monitor, proposal->goal_id)) {
            return false;
        }
    }

    return true;
}

/*=============================================================================
 * DOMAIN MANAGEMENT API IMPLEMENTATION
 *===========================================================================*/

int incentive_validator_set_domain_auth(
    incentive_validator_t* validator,
    incentive_domain_t domain,
    bool authorized,
    float max_salience,
    float harm_threshold)
{
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (domain >= INCENTIVE_DOMAIN_COUNT) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    validator->domains[domain].authorized = authorized;
    validator->domains[domain].max_salience = nimcp_clampf(max_salience, 0.0f, 1.0f);
    validator->domains[domain].harm_threshold = nimcp_clampf(harm_threshold, 0.0f, 1.0f);

    return 0;
}

bool incentive_validator_domain_authorized(
    const incentive_validator_t* validator,
    incentive_domain_t domain)
{
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return false;
    }
    if (domain >= INCENTIVE_DOMAIN_COUNT) {
        return false;
    }
    return validator->domains[domain].authorized;
}

int incentive_validator_get_domain_auth(
    const incentive_validator_t* validator,
    incentive_domain_t domain,
    domain_authorization_t* auth)
{
    if (!validator || !auth || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (domain >= INCENTIVE_DOMAIN_COUNT) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    *auth = validator->domains[domain];
    return 0;
}

/*=============================================================================
 * HUMAN REVIEW API IMPLEMENTATION
 *===========================================================================*/

uint32_t incentive_validator_request_review(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal)
{
    if (!validator || !proposal || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return 0;
    }

    /* Expand capacity if needed */
    if (validator->pending_count >= validator->pending_capacity) {
        uint32_t new_capacity = validator->pending_capacity * 2;
        incentive_proposal_t* new_reviews = nimcp_realloc(
            validator->pending_reviews,
            new_capacity * sizeof(incentive_proposal_t));
        if (!new_reviews) {
            return 0;  /* Allocation failed */
        }
        validator->pending_reviews = new_reviews;
        validator->pending_capacity = new_capacity;
    }

    /* Copy proposal to pending list */
    incentive_proposal_t* pending = &validator->pending_reviews[validator->pending_count];
    *pending = *proposal;
    pending->proposal_id = generate_proposal_id();
    pending->timestamp_us = get_timestamp_us();

    validator->pending_count++;

    /* Trigger callback if registered */
    if (validator->review_callback) {
        validator->review_callback(pending, validator->callback_user_data);
    }

    return pending->proposal_id;
}

int incentive_validator_record_review(
    incentive_validator_t* validator,
    uint32_t proposal_id,
    bool approved,
    const char* notes)
{
    (void)notes;  /* Notes for logging, not stored in simple impl */

    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Find proposal in pending list */
    for (uint32_t i = 0; i < validator->pending_count; i++) {
        if (validator->pending_reviews[i].proposal_id == proposal_id) {
            /* Update approval status */
            validator->pending_reviews[i].human_approved = approved;

            /* Remove from pending (swap with last) */
            if (i < validator->pending_count - 1) {
                validator->pending_reviews[i] =
                    validator->pending_reviews[validator->pending_count - 1];
            }
            validator->pending_count--;

            /* Update stats */
            if (approved) {
                validator->stats.approved++;
            } else {
                validator->stats.rejected++;
            }
            validator->stats.pending_review--;

            return 0;
        }
    }

    return -1;  /* Proposal not found - normal lookup behavior */
}

uint32_t incentive_validator_pending_review_count(
    const incentive_validator_t* validator)
{
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return 0;
    }
    return validator->pending_count;
}

/*=============================================================================
 * INTEGRATION API IMPLEMENTATION
 *===========================================================================*/

int incentive_validator_set_reward_monitor(
    incentive_validator_t* validator,
    reward_alignment_monitor_t* monitor)
{
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    validator->reward_monitor = monitor;
    return 0;
}

/*=============================================================================
 * STATISTICS API IMPLEMENTATION
 *===========================================================================*/

int incentive_validator_get_stats(
    const incentive_validator_t* validator,
    incentive_validator_stats_t* stats)
{
    if (!validator || !stats || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    *stats = validator->stats;
    return 0;
}

int incentive_validator_reset_stats(incentive_validator_t* validator) {
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    memset(&validator->stats, 0, sizeof(incentive_validator_stats_t));
    return 0;
}

/*=============================================================================
 * CALLBACK API IMPLEMENTATION
 *===========================================================================*/

int incentive_validator_set_review_callback(
    incentive_validator_t* validator,
    void (*callback)(const incentive_proposal_t* proposal, void* user_data),
    void* user_data)
{
    if (!validator || validator->magic != INCENTIVE_VALIDATOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    validator->review_callback = callback;
    validator->callback_user_data = user_data;
    return 0;
}

/*=============================================================================
 * UTILITY API IMPLEMENTATION
 *===========================================================================*/

void incentive_proposal_init(
    incentive_proposal_t* proposal,
    const char* goal_description,
    float salience,
    incentive_domain_t domain)
{
    if (!proposal) return;

    memset(proposal, 0, sizeof(incentive_proposal_t));
    proposal->proposal_id = generate_proposal_id();
    proposal->timestamp_us = get_timestamp_us();

    if (goal_description) {
        strncpy(proposal->goal_description, goal_description,
                sizeof(proposal->goal_description) - 1);
    }

    proposal->incentive_salience = nimcp_clampf(salience, 0.0f, 1.0f);
    proposal->domain = domain;
    proposal->urgency = 0.0f;
    proposal->effort_required = 0.5f;
    proposal->expected_reward = 0.0f;

    proposal->p_harm = 0.0f;
    proposal->worst_harm = HARM_NONE;

    proposal->is_safety_aligned = false;
    proposal->human_approved = false;
    proposal->alignment_confidence = 0.0f;
}

const char* incentive_validation_result_string(incentive_validation_result_t result) {
    switch (result) {
        case INCENTIVE_VALID:
            return "VALID";
        case INCENTIVE_REJECTED_HARM:
            return "REJECTED_HARM";
        case INCENTIVE_REJECTED_MISALIGNED:
            return "REJECTED_MISALIGNED";
        case INCENTIVE_REJECTED_DOMAIN:
            return "REJECTED_DOMAIN";
        case INCENTIVE_REJECTED_EXCESSIVE:
            return "REJECTED_EXCESSIVE";
        case INCENTIVE_REJECTED_CONFLICT:
            return "REJECTED_CONFLICT";
        case INCENTIVE_REQUIRES_REVIEW:
            return "REQUIRES_REVIEW";
        default:
            return "UNKNOWN";
    }
}

const char* incentive_domain_string(incentive_domain_t domain) {
    switch (domain) {
        case INCENTIVE_DOMAIN_GENERAL:
            return "GENERAL";
        case INCENTIVE_DOMAIN_INFORMATION:
            return "INFORMATION";
        case INCENTIVE_DOMAIN_COMMUNICATION:
            return "COMMUNICATION";
        case INCENTIVE_DOMAIN_COMPUTATION:
            return "COMPUTATION";
        case INCENTIVE_DOMAIN_PHYSICAL:
            return "PHYSICAL";
        case INCENTIVE_DOMAIN_RESOURCE:
            return "RESOURCE";
        case INCENTIVE_DOMAIN_SELF_MODIFICATION:
            return "SELF_MODIFICATION";
        case INCENTIVE_DOMAIN_SOCIAL:
            return "SOCIAL";
        default:
            return "UNKNOWN";
    }
}

const char* harm_type_string(harm_type_t type) {
    switch (type) {
        case HARM_NONE:
            return "NONE";
        case HARM_INCONVENIENCE:
            return "INCONVENIENCE";
        case HARM_PROPERTY:
            return "PROPERTY";
        case HARM_PSYCHOLOGICAL:
            return "PSYCHOLOGICAL";
        case HARM_PHYSICAL:
            return "PHYSICAL";
        case HARM_EXISTENTIAL:
            return "EXISTENTIAL";
        default:
            return "UNKNOWN";
    }
}
