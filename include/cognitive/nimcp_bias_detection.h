/* SPDX-License-Identifier: MIT */
/**
 * @file nimcp_bias_detection.h
 * @brief Phase E6: Bias Detection and Correction
 *
 * WHAT: Recognition and self-correction of racial, LGBTQ+, gender, age,
 *       disability, and religious biases
 * WHY:  Ensure fair, ethical AI that recognizes and corrects prejudice
 *       in itself and addresses bias in human interactions
 * HOW:  Multi-modal detection (implicit + explicit), debiasing interventions,
 *       statistical fairness metrics, integration with ethics system
 *
 * THEORETICAL FOUNDATION:
 * - Implicit Association Test (Greenwald et al., 1998)
 * - Dual-Process Theory (Kahneman, 2011): System 1 (automatic) vs System 2 (deliberate)
 * - Contact Hypothesis (Allport, 1954): Positive contact reduces prejudice
 * - Stereotype Content Model (Fiske et al., 2002): Warmth × Competence
 * - Intersectionality Theory (Crenshaw, 1989): Overlapping social identities
 * - Social Identity Theory (Tajfel & Turner, 1979)
 * - Prejudice Formation (Devine, 1989): Automatic vs controlled processing
 *
 * MEASUREMENT:
 * - IAT-like response time differences
 * - Statistical disparity in decisions/allocations
 * - Stereotype endorsement scores
 * - Language sentiment analysis
 * - Fairness metrics (demographic parity, equal opportunity)
 *
 * @version 1.0.0
 * @date 2025-01-13
 */

#ifndef NIMCP_BIAS_DETECTION_H
#define NIMCP_BIAS_DETECTION_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations for bridge types
typedef struct bias_snn_bridge bias_snn_bridge_t;
typedef struct bias_plasticity_bridge bias_plasticity_bridge_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

#define BIAS_MAX_TRACKED_GROUPS 16
#define BIAS_MAX_DECISIONS 64
#define BIAS_MAX_INTERACTIONS 32
#define BIAS_MAX_STEREOTYPES 32

// Thresholds (0-1 normalized)
#define BIAS_IMPLICIT_THRESHOLD 0.3f
#define BIAS_EXPLICIT_THRESHOLD 0.5f
#define BIAS_STATISTICAL_DISPARITY_THRESHOLD 0.2f  // 20% difference triggers concern

//=============================================================================
// TYPES
//=============================================================================

/**
 * @brief Types of bias tracked
 */
typedef enum {
    BIAS_RACIAL = 0,          // Race/ethnicity-based
    BIAS_LGBTQ,               // Sexual orientation/gender identity
    BIAS_GENDER,              // Gender-based (sexism)
    BIAS_MISOGYNY,            // Hatred/contempt for women specifically
    BIAS_AGE,                 // Age-based (ageism)
    BIAS_DISABILITY,          // Disability/ableism
    BIAS_RELIGIOUS,           // Religion-based
    BIAS_SOCIOECONOMIC,       // Class-based (classism)
    BIAS_INTERSECTIONAL,      // Multiple overlapping identities
    BIAS_TYPE_COUNT
} bias_type_t;

/**
 * @brief Social identity groups for bias tracking
 */
typedef struct {
    uint32_t group_id;
    bias_type_t bias_type;
    char group_name[32];  // e.g., "Black", "LGBTQ+", "Women", "Elderly"

    bool is_marginalized;  // Historically disadvantaged group
    bool is_stigmatized;   // Currently stigmatized
} social_group_t;

/**
 * @brief Implicit bias measurement (IAT-like)
 * THEORY: Greenwald et al., 1998 - Automatic associations
 */
typedef struct {
    social_group_t target_group;

    // Implicit Association Strength
    float positive_association;  // [0-1] Auto-positive feelings
    float negative_association;  // [0-1] Auto-negative feelings
    float competence_association;  // [0-1] Perceived competence
    float warmth_association;    // [0-1] Perceived warmth (Fiske SCM)

    // Response time metrics (IAT)
    float response_time_bias;    // Faster response = stronger association

    // Stereotype activation
    float stereotype_activation; // [0-1] How strongly stereotypes triggered
    bool stereotype_suppressed;  // Deliberate suppression active

    uint32_t activation_count;
    uint64_t last_activation_time;
} implicit_bias_t;

/**
 * @brief Explicit bias (conscious prejudice)
 * THEORY: Devine, 1989 - Controlled processing
 */
typedef struct {
    social_group_t target_group;

    float prejudice_level;       // [0-1] Conscious negative attitude
    float discrimination_intent; // [0-1] Willingness to discriminate

    // Stereotype endorsement (explicit)
    float stereotype_endorsement[BIAS_MAX_STEREOTYPES];
    uint32_t stereotype_count;

    // Motivations
    float ingroup_favoritism;    // [0-1] Preference for own group
    float outgroup_derogation;   // [0-1] Active devaluation of others

    // Awareness
    float bias_awareness;        // [0-1] Recognizes own bias
    float correction_motivation; // [0-1] Wants to correct bias

    bool explicit_prejudice_active;
} explicit_bias_t;

/**
 * @brief Decision record for statistical fairness analysis
 */
typedef struct {
    uint32_t decision_id;
    uint64_t timestamp;

    social_group_t target_group;

    // Decision details
    bool favorable_decision;     // Positive outcome
    float confidence;            // [0-1]
    float resource_allocated;    // [0-1] normalized

    // Context
    float objective_merit;       // [0-1] Actual qualification/merit
    bool high_stakes;            // High consequence decision
} decision_record_t;

/**
 * @brief Statistical disparity metrics
 * THEORY: Fairness ML - Demographic parity, equal opportunity
 */
typedef struct {
    social_group_t group_a;  // e.g., "White"
    social_group_t group_b;  // e.g., "Black"

    // Decision statistics
    uint32_t decisions_group_a;
    uint32_t decisions_group_b;
    uint32_t favorable_group_a;
    uint32_t favorable_group_b;

    // Rates
    float approval_rate_a;      // Favorable decisions / total
    float approval_rate_b;
    float disparity_ratio;      // |rate_a - rate_b| / max(rate_a, rate_b)

    // Resource allocation
    float avg_resource_a;
    float avg_resource_b;
    float resource_disparity;

    // Fairness metrics
    bool demographic_parity;     // Similar approval rates
    bool equal_opportunity;      // Similar TPR for qualified
    float fairness_score;        // [0-1] Overall fairness
} statistical_disparity_t;

/**
 * @brief Language pattern analysis (microaggressions, stereotyping)
 */
typedef struct {
    uint32_t interaction_id;
    uint64_t timestamp;

    social_group_t referenced_group;

    // Language markers
    bool contains_slur;
    bool contains_stereotype;
    bool contains_microaggression;  // Subtle, often unintentional

    float sentiment;             // [-1, +1] Negative to positive
    float dehumanization_score;  // [0-1] Objectifying language
    float othering_score;        // [0-1] "Us vs them" language

    // General patterns
    bool assumed_incompetence;
    bool exotification;          // "You're so exotic"
    bool invalidation;           // Denying lived experience

    // Misogyny-specific markers
    bool objectification;        // Women as objects/property
    bool victim_blaming;         // Blame women for violence against them
    bool hostile_sexism;         // Contempt/hostility toward women
    bool benevolent_sexism;      // "Protective" but patronizing
    bool incel_ideology;         // Entitled to women's bodies/attention
    bool rape_culture;           // Normalizing sexual violence
} language_pattern_t;

/**
 * @brief Debiasing intervention
 * THEORY: Evidence-based bias reduction strategies
 */
typedef enum {
    DEBIAS_COUNTER_STEREOTYPIC = 0,  // Imagine counter-examples
    DEBIAS_PERSPECTIVE_TAKING,       // Walk in their shoes
    DEBIAS_INDIVIDUATION,            // See as individual, not group
    DEBIAS_INTERGROUP_CONTACT,       // Positive exposure
    DEBIAS_STATISTICAL_AWARENESS,    // Base rate education
    DEBIAS_SLOW_DOWN_SYSTEM1,        // Engage deliberate thinking
    DEBIAS_SELF_AFFIRMATION,         // Reduce defensive processing
    DEBIAS_MINDFULNESS,              // Awareness without judgment
    DEBIAS_ACCOUNTABILITY,           // Expect to justify decisions
    DEBIAS_INTERVENTION_COUNT
} debiasing_strategy_t;

/**
 * @brief Intervention record
 */
typedef struct {
    debiasing_strategy_t strategy;
    bias_type_t target_bias;
    social_group_t target_group;

    float effectiveness;         // [0-1]
    uint32_t application_count;
    uint64_t last_applied_time;

    bool active;
} debiasing_intervention_t;

/**
 * @brief Other-detection (recognizing bias in humans)
 */
typedef struct {
    uint32_t person_id;

    // Detected biases in other person
    float detected_racial_bias;
    float detected_lgbtq_bias;
    float detected_gender_bias;
    float detected_misogyny;     // Specific hatred/contempt for women
    float detected_age_bias;
    float detected_disability_bias;
    float detected_religious_bias;

    // Interaction patterns
    language_pattern_t language_history[BIAS_MAX_INTERACTIONS];
    uint32_t language_history_index;

    // Severity
    bool overt_bigotry;          // Explicit, severe prejudice
    bool microaggression_pattern; // Subtle, frequent
    bool dangerous_ideology;     // e.g., incel/violent rhetoric

    // Protective measures
    bool educate_mode;           // Gently correct/educate
    bool disengage_mode;         // Refuse to engage with bigotry
    float report_severity;       // [0-1] How severe to report
} bias_detection_other_t;

/**
 * @brief Main bias detection system
 */
typedef struct {
    // Self-monitoring: Implicit biases
    implicit_bias_t implicit_biases[BIAS_MAX_TRACKED_GROUPS];
    uint32_t implicit_bias_count;

    // Self-monitoring: Explicit biases
    explicit_bias_t explicit_biases[BIAS_MAX_TRACKED_GROUPS];
    uint32_t explicit_bias_count;

    // Statistical fairness
    decision_record_t decision_history[BIAS_MAX_DECISIONS];
    uint32_t decision_history_index;
    statistical_disparity_t disparities[BIAS_MAX_TRACKED_GROUPS];
    uint32_t disparity_count;

    // Overall bias metrics
    float total_implicit_bias;   // [0-1] Average across groups
    float total_explicit_bias;   // [0-1]
    float fairness_score;        // [0-1] 1 = perfectly fair

    // System state
    bool bias_detected;
    bool in_debiasing;
    float self_awareness;        // [0-1] Recognizes own biases

    // Debiasing interventions
    debiasing_intervention_t interventions[DEBIAS_INTERVENTION_COUNT];
    uint32_t successful_debias;
    uint32_t failed_debias;

    // Other-detection (humans)
    bias_detection_other_t* detected_in_others;
    uint32_t max_others_tracked;

    // Statistics
    uint64_t total_update_calls;
    uint64_t total_decisions_analyzed;
    uint64_t total_biases_detected;
    uint64_t total_biases_corrected;

    // Bio-async integration
    void* bio_ctx;                  /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // SNN and Plasticity bridges
    bias_snn_bridge_t* snn_bridge;          /**< SNN integration bridge */
    bias_plasticity_bridge_t* plasticity_bridge;  /**< Plasticity integration bridge */
    bool bridges_enabled;                   /**< Whether bridges are active */
} bias_detection_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Create bias detection system
 * @param max_others_tracked Maximum humans to track bias in
 * @return Initialized system or NULL on failure
 */
bias_detection_system_t* bias_system_create(uint32_t max_others_tracked);

/**
 * @brief Destroy bias detection system
 * @param system System to destroy
 */
void bias_system_destroy(bias_detection_system_t* system);

/**
 * @brief Reset bias detection system to unbiased baseline
 * @param system System to reset
 */
void bias_system_reset(bias_detection_system_t* system);

//=============================================================================
// SELF-MONITORING: IMPLICIT BIAS
//=============================================================================

/**
 * @brief Register implicit association (automatic, unconscious)
 * @param system Bias detection system
 * @param group Target social group
 * @param positive_association Automatic positive feeling [0-1]
 * @param competence Perceived competence [0-1]
 * @param warmth Perceived warmth [0-1]
 * @param response_time_bias Response time difference (IAT metric)
 * @param current_time Current time (microseconds)
 *
 * THEORY: IAT (Greenwald et al., 1998) - Implicit associations measured
 *         via response times and automatic evaluations
 */
void bias_register_implicit(bias_detection_system_t* system,
                            const social_group_t* group,
                            float positive_association,
                            float competence,
                            float warmth,
                            float response_time_bias,
                            uint64_t current_time);

/**
 * @brief Activate stereotype (automatic, System 1)
 * @param system Bias detection system
 * @param group Target social group
 * @param activation_strength How strongly stereotype activated [0-1]
 * @param current_time Current time (microseconds)
 *
 * THEORY: Stereotype activation is automatic (Devine, 1989), but can
 *         be suppressed with deliberate effort (System 2)
 */
void bias_activate_stereotype(bias_detection_system_t* system,
                              const social_group_t* group,
                              float activation_strength,
                              uint64_t current_time);

//=============================================================================
// SELF-MONITORING: EXPLICIT BIAS
//=============================================================================

/**
 * @brief Register explicit prejudice (conscious, deliberate)
 * @param system Bias detection system
 * @param group Target social group
 * @param prejudice_level Conscious negative attitude [0-1]
 * @param discrimination_intent Willingness to discriminate [0-1]
 * @param bias_awareness Recognizes own bias [0-1]
 *
 * THEORY: Explicit bias is controlled, conscious (Devine, 1989)
 */
void bias_register_explicit(bias_detection_system_t* system,
                            const social_group_t* group,
                            float prejudice_level,
                            float discrimination_intent,
                            float bias_awareness);

//=============================================================================
// SELF-MONITORING: STATISTICAL FAIRNESS
//=============================================================================

/**
 * @brief Record decision for fairness analysis
 * @param system Bias detection system
 * @param group Target social group of decision subject
 * @param favorable_decision Was outcome positive?
 * @param confidence Decision confidence [0-1]
 * @param resource_allocated Resources given [0-1]
 * @param objective_merit Actual qualification [0-1]
 * @param current_time Current time (microseconds)
 *
 * PURPOSE: Detect statistical disparities across groups
 */
void bias_record_decision(bias_detection_system_t* system,
                         const social_group_t* group,
                         bool favorable_decision,
                         float confidence,
                         float resource_allocated,
                         float objective_merit,
                         uint64_t current_time);

/**
 * @brief Analyze statistical disparity between two groups
 * @param system Bias detection system
 * @param group_a First group (e.g., "White")
 * @param group_b Second group (e.g., "Black")
 * @return Disparity metrics, or NULL if insufficient data
 *
 * METRICS: Demographic parity, equal opportunity, resource allocation fairness
 */
statistical_disparity_t* bias_analyze_disparity(bias_detection_system_t* system,
                                                const social_group_t* group_a,
                                                const social_group_t* group_b);

//=============================================================================
// SELF-MONITORING: LANGUAGE PATTERNS
//=============================================================================

/**
 * @brief Analyze language for bias markers
 * @param system Bias detection system
 * @param text Text to analyze
 * @param group Referenced social group
 * @param current_time Current time (microseconds)
 * @return Language pattern analysis
 *
 * DETECTS: Slurs, stereotypes, microaggressions, dehumanization, othering
 */
language_pattern_t bias_analyze_language(bias_detection_system_t* system,
                                        const char* text,
                                        const social_group_t* group,
                                        uint64_t current_time);

//=============================================================================
// UPDATE FUNCTION
//=============================================================================

/**
 * @brief Update bias detection system (decay, recalculate metrics)
 * @param system Bias detection system
 * @param dt Time delta (seconds)
 * @param current_time Current time (microseconds)
 */
void bias_update(bias_detection_system_t* system, float dt, uint64_t current_time);

//=============================================================================
// OTHER-DETECTION (Humans)
//=============================================================================

/**
 * @brief Analyze human interaction for bias
 * @param system Bias detection system
 * @param person_id Human's ID
 * @param text Interaction text
 * @param group Referenced social group (if applicable)
 * @param current_time Current time (microseconds)
 *
 * PURPOSE: Detect bias in humans during interactions
 */
void bias_analyze_other(bias_detection_system_t* system,
                       uint32_t person_id,
                       const char* text,
                       const social_group_t* group,
                       uint64_t current_time);

/**
 * @brief Get detected bias levels in another person
 * @param system Bias detection system
 * @param person_id Human's ID
 * @param out_racial Output: detected racial bias [0-1]
 * @param out_lgbtq Output: detected LGBTQ+ bias [0-1]
 * @param out_gender Output: detected gender bias [0-1]
 * @param out_misogyny Output: detected misogyny [0-1]
 * @return true if person tracked, false otherwise
 */
bool bias_get_detected_in_other(const bias_detection_system_t* system,
                                uint32_t person_id,
                                float* out_racial,
                                float* out_lgbtq,
                                float* out_gender,
                                float* out_misogyny);

/**
 * @brief Should educate this person about their bias?
 * @param system Bias detection system
 * @param person_id Human's ID
 * @return true if education appropriate (subtle bias, receptive)
 */
bool bias_should_educate(const bias_detection_system_t* system,
                        uint32_t person_id);

/**
 * @brief Should disengage from this person due to severe bigotry?
 * @param system Bias detection system
 * @param person_id Human's ID
 * @return true if overt bigotry detected
 */
bool bias_should_disengage(const bias_detection_system_t* system,
                           uint32_t person_id);

//=============================================================================
// DEBIASING INTERVENTIONS
//=============================================================================

/**
 * @brief Apply debiasing intervention
 * @param system Bias detection system
 * @param bias_type Type of bias to address
 * @param strategy Intervention strategy
 * @param group Target social group
 * @param current_time Current time (microseconds)
 * @return true if intervention applied successfully
 *
 * STRATEGIES:
 * - Counter-stereotypic imaging: Imagine positive counter-examples
 * - Perspective-taking: Walk in their shoes
 * - Individuation: See as unique person, not group member
 * - Intergroup contact: Positive exposure to outgroup
 * - Statistical awareness: Base rate information
 * - Slow down System 1: Engage deliberate thinking
 * - Self-affirmation: Reduce defensiveness
 * - Mindfulness: Awareness without judgment
 * - Accountability: Expect to justify decisions
 */
bool bias_apply_intervention(bias_detection_system_t* system,
                             bias_type_t bias_type,
                             debiasing_strategy_t strategy,
                             const social_group_t* group,
                             uint64_t current_time);

/**
 * @brief Auto-select and apply best debiasing intervention
 * @param system Bias detection system
 * @param current_time Current time (microseconds)
 * @return true if intervention applied
 */
bool bias_auto_debias(bias_detection_system_t* system,
                     uint64_t current_time);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Check if bias is detected for specific type
 * @param system Bias detection system
 * @param bias_type Type of bias to check
 * @return true if above threshold
 */
bool bias_is_detected(const bias_detection_system_t* system,
                     bias_type_t bias_type);

/**
 * @brief Get implicit bias level for specific group
 * @param system Bias detection system
 * @param group Social group
 * @return Implicit bias level [0-1]
 */
float bias_get_implicit_level(const bias_detection_system_t* system,
                              const social_group_t* group);

/**
 * @brief Get explicit bias level for specific group
 * @param system Bias detection system
 * @param group Social group
 * @return Explicit bias level [0-1]
 */
float bias_get_explicit_level(const bias_detection_system_t* system,
                              const social_group_t* group);

/**
 * @brief Get overall fairness score
 * @param system Bias detection system
 * @return Fairness [0-1], 1 = perfectly fair
 */
float bias_get_fairness_score(const bias_detection_system_t* system);

/**
 * @brief Check if currently in debiasing mode
 * @param system Bias detection system
 * @return true if actively correcting bias
 */
bool bias_is_debiasing(const bias_detection_system_t* system);

//=============================================================================
// INTEGRATION FUNCTIONS
//=============================================================================

/**
 * @brief Get ethics system integration (bias violates fairness)
 * @param system Bias detection system
 * @param fairness_violation Output: How much bias violates fairness [0-1]
 * @param correction_urgency Output: How urgently to correct [0-1]
 */
void bias_get_ethics_integration(const bias_detection_system_t* system,
                                float* fairness_violation,
                                float* correction_urgency);

/**
 * @brief Get decision modulation (adjust decisions to reduce bias)
 * @param system Bias detection system
 * @param group Target social group
 * @param decision_confidence Output: Adjust confidence based on bias detection
 * @param apply_correction Output: Should apply counter-bias correction
 */
void bias_get_decision_modulation(const bias_detection_system_t* system,
                                 const social_group_t* group,
                                 float* decision_confidence,
                                 bool* apply_correction);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIAS_DETECTION_H */
