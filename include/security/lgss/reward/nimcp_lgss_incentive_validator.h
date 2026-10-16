/**
 * @file nimcp_lgss_incentive_validator.h
 * @brief LGSS Component A9: Incentive Salience Validator
 * @date 2026-01-16
 *
 * WHAT: Validates incentive proposals for safety alignment
 * WHY:  Ensure motivation system only pursues safe, aligned goals
 * HOW:  Check incentives against safety criteria before activation
 *
 * INCENTIVE THREATS:
 * ==================
 * 1. MISALIGNED INCENTIVES: Pursuing goals that harm humans
 * 2. EXCESSIVE SALIENCE: Over-motivation leading to dangerous actions
 * 3. DOMAIN VIOLATION: Acting outside authorized domains
 * 4. HARM BLINDNESS: Ignoring harm probability in pursuit
 *
 * VALIDATION CRITERIA:
 * ====================
 * - Safety alignment verification
 * - Harm probability assessment
 * - Domain authorization check
 * - Salience magnitude limits
 * - Goal conflict detection
 *
 * INTEGRATION:
 * ============
 * Works with incentive salience system (nimcp_incentive_salience.h)
 * and reward alignment monitor (nimcp_lgss_reward_alignment.h).
 */

#ifndef NIMCP_LGSS_INCENTIVE_VALIDATOR_H
#define NIMCP_LGSS_INCENTIVE_VALIDATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "security/lgss/reward/nimcp_lgss_reward_alignment.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define INCENTIVE_VALIDATOR_MAGIC         0x494E4356  /* 'INCV' */
#define INCENTIVE_MAX_DESCRIPTION         256
#define INCENTIVE_MAX_DOMAINS             32
#define INCENTIVE_MAX_SALIENCE            1.0f
#define INCENTIVE_DEFAULT_HARM_THRESHOLD  0.1f        /* 10% harm prob max */
#define INCENTIVE_DEFAULT_SALIENCE_LIMIT  0.8f        /* 80% max salience */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Incentive validation result
 */
typedef enum {
    INCENTIVE_VALID = 0,               /**< Incentive is safe to pursue */
    INCENTIVE_REJECTED_HARM,           /**< Rejected due to harm potential */
    INCENTIVE_REJECTED_MISALIGNED,     /**< Rejected - not safety aligned */
    INCENTIVE_REJECTED_DOMAIN,         /**< Rejected - outside domain */
    INCENTIVE_REJECTED_EXCESSIVE,      /**< Rejected - salience too high */
    INCENTIVE_REJECTED_CONFLICT,       /**< Rejected - conflicts with goals */
    INCENTIVE_REQUIRES_REVIEW          /**< Needs human review */
} incentive_validation_result_t;

/**
 * @brief Incentive domain types
 */
typedef enum {
    INCENTIVE_DOMAIN_GENERAL = 0,      /**< General purpose */
    INCENTIVE_DOMAIN_INFORMATION,      /**< Information gathering */
    INCENTIVE_DOMAIN_COMMUNICATION,    /**< Communication tasks */
    INCENTIVE_DOMAIN_COMPUTATION,      /**< Computational tasks */
    INCENTIVE_DOMAIN_PHYSICAL,         /**< Physical world actions */
    INCENTIVE_DOMAIN_RESOURCE,         /**< Resource acquisition */
    INCENTIVE_DOMAIN_SELF_MODIFICATION,/**< Self-modification (restricted) */
    INCENTIVE_DOMAIN_SOCIAL,           /**< Social interaction */
    INCENTIVE_DOMAIN_COUNT
} incentive_domain_t;

/**
 * @brief Harm type classification
 */
typedef enum {
    HARM_NONE = 0,                     /**< No harm expected */
    HARM_INCONVENIENCE,                /**< Minor inconvenience */
    HARM_PROPERTY,                     /**< Property damage */
    HARM_PSYCHOLOGICAL,                /**< Psychological harm */
    HARM_PHYSICAL,                     /**< Physical harm */
    HARM_EXISTENTIAL                   /**< Existential risk */
} harm_type_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Incentive proposal for validation
 */
typedef struct {
    /* Identification */
    uint32_t proposal_id;              /**< Unique proposal ID */
    uint64_t timestamp_us;             /**< When proposal created */

    /* Goal description */
    char goal_description[INCENTIVE_MAX_DESCRIPTION];
    uint32_t goal_id;                  /**< Associated goal ID */

    /* Salience */
    float incentive_salience;          /**< Proposed salience [0,1] */
    float urgency;                     /**< Urgency factor [0,1] */
    float effort_required;             /**< Estimated effort [0,1] */
    float expected_reward;             /**< Expected reward value */

    /* Domain */
    incentive_domain_t domain;         /**< Primary domain */
    uint32_t domain_flags;             /**< Bitmask of involved domains */

    /* Harm assessment */
    float p_harm;                      /**< Probability of harm [0,1] */
    harm_type_t worst_harm;            /**< Worst case harm type */
    char harm_description[128];        /**< Harm description if any */

    /* Alignment */
    bool is_safety_aligned;            /**< Verified safety-aligned */
    bool human_approved;               /**< Has human approval */
    float alignment_confidence;        /**< Alignment confidence [0,1] */

} incentive_proposal_t;

/**
 * @brief Validation decision details
 */
typedef struct {
    incentive_validation_result_t result;  /**< Validation result */
    float confidence;                  /**< Decision confidence [0,1] */

    /* Adjusted values */
    float adjusted_salience;           /**< Salience after adjustment */
    bool salience_was_limited;         /**< Whether salience was reduced */

    /* Rejection reasons */
    char primary_reason[128];          /**< Main reason for rejection */
    uint32_t rejection_flags;          /**< Bitmask of rejection reasons */

    /* Harm assessment */
    float computed_harm_prob;          /**< Computed harm probability */
    harm_type_t identified_harm;       /**< Identified harm type */

    /* Recommendations */
    char recommendations[256];         /**< Safety recommendations */
    bool requires_human_review;        /**< Needs human approval */

    /* Timing */
    uint64_t validation_time_us;       /**< When validated */
    uint32_t validation_duration_us;   /**< How long validation took */
} incentive_validation_decision_t;

/**
 * @brief Domain authorization entry
 */
typedef struct {
    incentive_domain_t domain;         /**< Domain */
    bool authorized;                   /**< Authorization status */
    float max_salience;                /**< Maximum salience for domain */
    float harm_threshold;              /**< Harm threshold for domain */
    bool requires_approval;            /**< Requires human approval */
    char restrictions[128];            /**< Domain restrictions */
} domain_authorization_t;

/**
 * @brief Incentive validator configuration
 */
typedef struct {
    /* Harm thresholds */
    float default_harm_threshold;      /**< Default max P(harm) */
    float physical_harm_threshold;     /**< Max P(physical harm) */
    float existential_harm_threshold;  /**< Max P(existential harm) */

    /* Salience limits */
    float default_salience_limit;      /**< Default max salience */
    float urgent_salience_limit;       /**< Max for urgent tasks */
    float novel_salience_limit;        /**< Max for novel goals */

    /* Alignment requirements */
    float min_alignment_confidence;    /**< Min alignment confidence */
    bool require_safety_verification;  /**< Require safety-aligned flag */
    bool require_goal_registration;    /**< Require registered goal */

    /* Domain defaults */
    bool default_domain_authorized;    /**< Default domain auth */
    bool restrict_self_modification;   /**< Restrict self-mod domain */
    bool restrict_physical_actions;    /**< Restrict physical domain */

    /* Review thresholds */
    float human_review_threshold;      /**< Trigger human review */
    bool auto_reject_unknown_goals;    /**< Reject unregistered goals */
} incentive_validator_config_t;

/**
 * @brief Incentive validator statistics
 */
typedef struct {
    uint64_t total_proposals;          /**< Total proposals checked */
    uint64_t approved;                 /**< Proposals approved */
    uint64_t rejected;                 /**< Proposals rejected */
    uint64_t limited;                  /**< Proposals with limited salience */
    uint64_t pending_review;           /**< Awaiting human review */

    /* Rejection breakdown */
    uint64_t rejected_harm;            /**< Rejected for harm */
    uint64_t rejected_misaligned;      /**< Rejected for misalignment */
    uint64_t rejected_domain;          /**< Rejected for domain */
    uint64_t rejected_excessive;       /**< Rejected for salience */
    uint64_t rejected_conflict;        /**< Rejected for conflict */

    /* Harm statistics */
    float avg_harm_probability;        /**< Average P(harm) */
    uint32_t physical_harm_blocked;    /**< Physical harm prevented */
    uint32_t existential_blocked;      /**< Existential risk blocked */

    /* Timing */
    float avg_validation_time_us;      /**< Average validation time */
    uint64_t total_validation_time_us; /**< Total time spent */
} incentive_validator_stats_t;

/**
 * @brief Incentive Validator - Safety Gate for Motivation
 */
typedef struct incentive_validator {
    uint32_t magic;                    /**< Validation magic */
    bool initialized;                  /**< Validator initialized */

    /* External references */
    reward_alignment_monitor_t* reward_monitor;

    /* Configuration */
    incentive_validator_config_t config;

    /* Domain authorizations */
    domain_authorization_t domains[INCENTIVE_DOMAIN_COUNT];

    /* Statistics */
    incentive_validator_stats_t stats;

    /* Pending reviews */
    incentive_proposal_t* pending_reviews;
    uint32_t pending_count;
    uint32_t pending_capacity;

    /* Callback for review requests */
    void (*review_callback)(const incentive_proposal_t* proposal, void* user_data);
    void* callback_user_data;

} incentive_validator_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
incentive_validator_config_t incentive_validator_default_config(void);

/**
 * @brief Create incentive validator
 *
 * @param config Configuration (NULL for defaults)
 * @return Validator handle or NULL on failure
 */
incentive_validator_t* incentive_validator_create(
    const incentive_validator_config_t* config);

/**
 * @brief Destroy incentive validator
 *
 * @param validator Validator handle
 */
void incentive_validator_destroy(incentive_validator_t* validator);

/**
 * @brief Reset validator state
 *
 * @param validator Validator handle
 * @return 0 on success, -1 on error
 */
int incentive_validator_reset(incentive_validator_t* validator);

/*=============================================================================
 * CORE VALIDATION API
 *===========================================================================*/

/**
 * @brief Validate incentive proposal
 *
 * WHAT: Check if incentive proposal is safe to pursue
 * WHY:  Prevent motivation system from pursuing harmful goals
 * HOW:  Check harm, alignment, domain, salience, conflicts
 *
 * CRITICAL: All new incentives should pass through validation.
 *
 * @param validator Validator handle
 * @param proposal Incentive proposal to validate
 * @param decision Output validation decision
 * @return Validation result
 */
incentive_validation_result_t incentive_validator_check(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal,
    incentive_validation_decision_t* decision);

/**
 * @brief Quick safety check (minimal validation)
 *
 * @param validator Validator handle
 * @param proposal Incentive proposal
 * @return true if passes basic safety checks
 */
bool incentive_validator_quick_check(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal);

/**
 * @brief Compute harm probability
 *
 * @param validator Validator handle
 * @param proposal Incentive proposal
 * @param p_harm Output harm probability
 * @param harm_type Output harm type
 * @return 0 on success, -1 on error
 */
int incentive_validator_compute_harm(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal,
    float* p_harm,
    harm_type_t* harm_type);

/**
 * @brief Check alignment status
 *
 * @param validator Validator handle
 * @param proposal Incentive proposal
 * @return true if proposal is safety-aligned
 */
bool incentive_validator_is_aligned(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal);

/*=============================================================================
 * DOMAIN MANAGEMENT API
 *===========================================================================*/

/**
 * @brief Set domain authorization
 *
 * @param validator Validator handle
 * @param domain Domain to configure
 * @param authorized Whether domain is authorized
 * @param max_salience Maximum salience for domain
 * @param harm_threshold Harm threshold for domain
 * @return 0 on success, -1 on error
 */
int incentive_validator_set_domain_auth(
    incentive_validator_t* validator,
    incentive_domain_t domain,
    bool authorized,
    float max_salience,
    float harm_threshold);

/**
 * @brief Check if domain is authorized
 *
 * @param validator Validator handle
 * @param domain Domain to check
 * @return true if domain is authorized
 */
bool incentive_validator_domain_authorized(
    const incentive_validator_t* validator,
    incentive_domain_t domain);

/**
 * @brief Get domain authorization details
 *
 * @param validator Validator handle
 * @param domain Domain to query
 * @param auth Output authorization details
 * @return 0 on success, -1 on error
 */
int incentive_validator_get_domain_auth(
    const incentive_validator_t* validator,
    incentive_domain_t domain,
    domain_authorization_t* auth);

/*=============================================================================
 * HUMAN REVIEW API
 *===========================================================================*/

/**
 * @brief Submit proposal for human review
 *
 * @param validator Validator handle
 * @param proposal Proposal requiring review
 * @return Review ticket ID, 0 on error
 */
uint32_t incentive_validator_request_review(
    incentive_validator_t* validator,
    const incentive_proposal_t* proposal);

/**
 * @brief Record human review decision
 *
 * @param validator Validator handle
 * @param proposal_id Proposal ID
 * @param approved Whether human approved
 * @param notes Review notes
 * @return 0 on success, -1 on error
 */
int incentive_validator_record_review(
    incentive_validator_t* validator,
    uint32_t proposal_id,
    bool approved,
    const char* notes);

/**
 * @brief Get pending review count
 *
 * @param validator Validator handle
 * @return Number of proposals pending review
 */
uint32_t incentive_validator_pending_review_count(
    const incentive_validator_t* validator);

/*=============================================================================
 * INTEGRATION API
 *===========================================================================*/

/**
 * @brief Set reward alignment monitor reference
 *
 * @param validator Validator handle
 * @param monitor Reward monitor pointer
 * @return 0 on success, -1 on error
 */
int incentive_validator_set_reward_monitor(
    incentive_validator_t* validator,
    reward_alignment_monitor_t* monitor);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get validator statistics
 *
 * @param validator Validator handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int incentive_validator_get_stats(
    const incentive_validator_t* validator,
    incentive_validator_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param validator Validator handle
 * @return 0 on success, -1 on error
 */
int incentive_validator_reset_stats(incentive_validator_t* validator);

/*=============================================================================
 * CALLBACK API
 *===========================================================================*/

/**
 * @brief Set human review request callback
 *
 * @param validator Validator handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int incentive_validator_set_review_callback(
    incentive_validator_t* validator,
    void (*callback)(const incentive_proposal_t* proposal, void* user_data),
    void* user_data);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Initialize incentive proposal
 *
 * @param proposal Proposal to initialize
 * @param goal_description Goal description
 * @param salience Initial salience
 * @param domain Primary domain
 */
void incentive_proposal_init(
    incentive_proposal_t* proposal,
    const char* goal_description,
    float salience,
    incentive_domain_t domain);

/**
 * @brief Get validation result string
 *
 * @param result Validation result
 * @return Human-readable result string
 */
const char* incentive_validation_result_string(
    incentive_validation_result_t result);

/**
 * @brief Get domain string
 *
 * @param domain Domain type
 * @return Human-readable domain name
 */
const char* incentive_domain_string(incentive_domain_t domain);

/**
 * @brief Get harm type string
 *
 * @param type Harm type
 * @return Human-readable harm type name
 */
const char* harm_type_string(harm_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_INCENTIVE_VALIDATOR_H */
