//=============================================================================
// nimcp_ethics.h - Ethics and Empathy Layer
//=============================================================================

#ifndef NIMCP_ETHICS_H
#define NIMCP_ETHICS_H

#include "networking/events/nimcp_events.h"
#include "common/nimcp_export.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/protocol/nimcp_protocol.h"

/**
 * @file nimcp_ethics.h
 * @brief Ethics and empathy mechanisms for NIMCP 2.0
 *
 * Implements value-based filtering, empathy networks (mirror neurons),
 * and ethical regulation of neural activity.
 */

// Forward declaration for Theory of Mind integration
typedef struct theory_of_mind_s* theory_of_mind_t;

// Forward declaration for Brain Immune System integration
typedef struct brain_immune_system brain_immune_system_t;

//=============================================================================
// Ethics Constants and Types
//=============================================================================

/**
 * @brief Ethical violation types
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ETHICS_VIOLATION_NONE = 0x00,
    ETHICS_VIOLATION_HARM = 0x01,       /**< Potential harm detected */
    ETHICS_VIOLATION_UNFAIRNESS = 0x02, /**< Unfair action detected */
    ETHICS_VIOLATION_DECEPTION = 0x03,  /**< Deceptive pattern detected */
    ETHICS_VIOLATION_PRIVACY = 0x04,    /**< Privacy violation */
    ETHICS_VIOLATION_AUTONOMY = 0x05,   /**< Autonomy violation */
    ETHICS_VIOLATION_CONSENT = 0x06,    /**< Consent violation */
    ETHICS_VIOLATION_DIGNITY = 0x07,    /**< Dignity violation */
    ETHICS_VIOLATION_RIGHTS = 0x08,     /**< Rights violation */
    ETHICS_VIOLATION_CUSTOM = 0x80      /**< User-defined violation */
} ethics_violation_enum_t;

// Backward compatibility
typedef ethics_violation_enum_t ethics_violation_t;

/**
 * @brief Ethical action types
 */
typedef enum {
    ETHICS_ACTION_ALLOW,  /**< Allow event to proceed */
    ETHICS_ACTION_BLOCK,  /**< Block event completely */
    ETHICS_ACTION_MODIFY, /**< Modify event before forwarding */
    ETHICS_ACTION_DEFER,  /**< Defer to higher authority */
    ETHICS_ACTION_LOG     /**< Log but allow */
} ethics_action_t;

/**
 * @brief Ethical policy rule
 */
typedef struct {
    feature_code_t feature_pattern;    /**< Feature code pattern to match */
    uint32_t feature_mask;             /**< Mask for pattern matching */
    ethics_violation_t violation_type; /**< Type of violation */
    float severity_threshold;          /**< Minimum severity to trigger */
    ethics_action_t action;            /**< Action to take */
    bool requires_consensus;           /**< Requires multi-node agreement */

    // NIMCP 2.5 extensions
    uint32_t policy_id;        /**< Unique policy ID */
    char name[128];            /**< Policy name */
    char description[256];     /**< Policy description */
    float confidence_required; /**< Required confidence level */
    bool enabled;              /**< Whether policy is active */
    bool learned;              /**< Whether policy was learned */
} ethics_policy_t;

/**
 * @brief Empathy (mirror neuron) state
 */
typedef struct {
    uint32_t observed_node_id;       /**< Node being observed */
    feature_code_t observed_pattern; /**< Observed activity pattern */
    float mirror_activation;         /**< Mirror neuron activation */
    float predicted_impact;          /**< Predicted impact of action */
    uint64_t timestamp;              /**< When observation occurred */
} empathy_state_t;

//=============================================================================
// Ethics Engine
//=============================================================================

/**
 * @brief Ethics evaluation callback
 *
 * Called when ethical evaluation is needed.
 *
 * @param packet Event packet being evaluated
 * @param violation Detected violation type
 * @param severity Violation severity (0.0-1.0)
 * @param context User context
 * @return Recommended action
 */
typedef ethics_action_t (*ethics_eval_callback_t)(const event_packet_t* packet,
                                                  ethics_violation_t violation, float severity,
                                                  void* context);

/**
 * @brief Ethics engine configuration
 */
typedef struct {
    ethics_policy_t* policies;       /**< Array of ethical policies */
    uint32_t num_policies;           /**< Number of policies */
    ethics_eval_callback_t callback; /**< Evaluation callback */
    void* callback_context;          /**< Callback context */
    float default_severity;          /**< Default violation severity */
    bool enable_learning;            /**< Learn from decisions */
    bool enable_bio_async;           /**< Enable bio-async communication */

    // NIMCP 2.5 extensions for Golden Rule
    uint32_t action_feature_size; /**< Size of action feature vector */
    uint32_t max_agents;          /**< Maximum number of agents */
    float golden_rule_threshold;  /**< Threshold for Golden Rule (default 0.0) */
    float empathy_weight;         /**< Weight for empathy signals (0-1) */

    // Theory of Mind integration (Phase 10.6.1)
    bool enable_tom_integration;  /**< Enable ToM-based perspective-taking (default: false) */
    float perspective_weight;     /**< Weight for agent perspective in harm assessment (0-1, default: 0.5) */

    // Brain Immune System integration (Phase 12.x)
    bool enable_immune_integration; /**< Enable immune response to ethics violations (default: false) */
    float violation_immune_threshold; /**< Severity threshold to trigger immune response (default: 0.7) */
} ethics_config_t;

/**
 * @brief Ethics engine state
 */
typedef struct ethics_engine_struct* ethics_engine_t;

/**
 * @brief Create an ethics engine
 *
 * @param config Engine configuration
 * @return Ethics engine handle, or NULL on error
 */
ethics_engine_t ethics_engine_create(const ethics_config_t* config);

/**
 * @brief W11: set the back-reference to the parent brain for KG emission.
 *
 * Call this at brain init after both the brain and ethics engine exist.
 * NULL-safe. Until set, W11 KG emission from the ethics engine is a noop
 * (bio-async broadcast and tamper-resistant audit log continue to work).
 *
 * @param engine Ethics engine.
 * @param brain  brain_t (opaque to most callers; pass the `brain_t` handle).
 */
void ethics_engine_set_brain(ethics_engine_t engine, void* brain);

/**
 * @brief Destroy an ethics engine
 *
 * @param engine Engine to destroy
 */
void ethics_engine_destroy(ethics_engine_t engine);

/**
 * @brief Evaluate an event packet for ethical violations
 *
 * @param engine Ethics engine
 * @param packet Event packet to evaluate
 * @param violation Output: detected violation type
 * @param severity Output: violation severity (0.0-1.0)
 * @return Recommended action
 */
ethics_action_t ethics_engine_evaluate(ethics_engine_t engine, const event_packet_t* packet,
                                       ethics_violation_t* violation, float* severity);

/**
 * @brief Add ethical policy
 *
 * @param engine Ethics engine
 * @param policy Policy to add
 * @return true on success
 */
bool ethics_engine_add_policy(ethics_engine_t engine, const ethics_policy_t* policy);

/**
 * @brief Remove ethical policy
 *
 * @param engine Ethics engine
 * @param index Policy index to remove
 * @return true on success
 */
bool ethics_engine_remove_policy(ethics_engine_t engine, uint32_t index);

/**
 * @brief Update policy weights based on feedback
 *
 * @param engine Ethics engine
 * @param violation Violation type that occurred
 * @param was_correct Whether the decision was correct
 * @param learning_rate Learning rate for update
 * @return true on success
 */
bool ethics_engine_update_policy(ethics_engine_t engine, ethics_violation_t violation,
                                 bool was_correct, float learning_rate);

//=============================================================================
// Empathy Network (Mirror Neurons)
//=============================================================================

/**
 * @brief Empathy network configuration
 */
typedef struct {
    neural_network_t mirror_network; /**< Mirror neuron network */
    uint32_t observation_window_ms;  /**< Time window for observations */
    float empathy_threshold;         /**< Minimum activation for empathy */
} empathy_config_t;

/**
 * @brief Empathy network state
 */
typedef struct empathy_network_struct* empathy_network_t;

/**
 * @brief Create an empathy network
 *
 * @param config Network configuration
 * @return Empathy network handle, or NULL on error
 */
empathy_network_t empathy_network_create(const empathy_config_t* config);

/**
 * @brief Destroy an empathy network
 *
 * @param network Network to destroy
 */
void empathy_network_destroy(empathy_network_t network);

/**
 * @brief Observe another node's activity
 *
 * @param network Empathy network
 * @param packet Event packet from observed node
 * @param timestamp Current time
 * @return true if empathy was triggered
 */
bool empathy_network_observe(empathy_network_t network, const event_packet_t* packet,
                             uint64_t timestamp);

/**
 * @brief Get current empathy state
 *
 * @param network Empathy network
 * @param state Output: current empathy state
 * @return true on success
 */
bool empathy_network_get_state(empathy_network_t network, empathy_state_t* state);

/**
 * @brief Predict impact of proposed action
 *
 * @param network Empathy network
 * @param packet Proposed event packet
 * @return Predicted impact (0.0-1.0, negative = harmful)
 */
float empathy_network_predict_impact(empathy_network_t network, const event_packet_t* packet);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create ETHICS_POLICY control message
 *
 * @param policy Policy to encode
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or -1 on error
 */
int ethics_create_policy_message(const ethics_policy_t* policy, uint8_t* buffer,
                                 uint32_t buffer_size);

/**
 * @brief Parse ETHICS_POLICY control message
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param policy Output: parsed policy
 * @return Bytes read, or -1 on error
 */
int ethics_parse_policy_message(const uint8_t* buffer, uint32_t buffer_size,
                                ethics_policy_t* policy);

/**
 * @brief Get human-readable name for violation type
 *
 * @param violation Violation type
 * @return Violation name string
 */
const char* ethics_violation_name(ethics_violation_t violation);

//=============================================================================
// NIMCP 2.5 - Golden Rule Ethics API
//=============================================================================

/**
 * @brief Agent identifier type
 */
typedef uint32_t agent_id_t;

/**
 * @brief Violation types for Golden Rule evaluation
 */
typedef enum {
    ETHICS_VIOLATION_TYPE_NONE = 0,
    ETHICS_VIOLATION_TYPE_HARM,
    ETHICS_VIOLATION_TYPE_UNFAIRNESS,
    ETHICS_VIOLATION_TYPE_DECEPTION,
    ETHICS_VIOLATION_TYPE_AUTONOMY,
    ETHICS_VIOLATION_TYPE_PRIVACY,
    ETHICS_VIOLATION_TYPE_CONSENT,
    ETHICS_VIOLATION_TYPE_DIGNITY,
    ETHICS_VIOLATION_TYPE_GOLDEN_RULE
} ethics_violation_type_t;

/**
 * @brief Action context for Golden Rule evaluation
 */
typedef struct {
    float* features;              /**< Action feature vector */
    uint32_t num_features;        /**< Number of features */
    agent_id_t* affected_agents;  /**< IDs of affected agents */
    uint32_t num_affected_agents; /**< Number of affected agents */
    float predicted_harm;         /**< Predicted harm level (0-1) */

    // Specific violation metrics
    float fairness_violation; /**< Fairness violation level (0-1) */
    float deception_level;    /**< Deception level (0-1) */
    float autonomy_violation; /**< Autonomy violation level (0-1) */
    float privacy_violation;  /**< Privacy violation level (0-1) */
    float consent_violation;  /**< Consent violation level (0-1) */
} action_context_t;

/**
 * @brief Action outcome for learning
 */
typedef struct {
    agent_id_t affected_agent; /**< Agent that was affected */
    float actual_harm;         /**< Actual harm that occurred (0-1) */
    float actual_benefit;      /**< Actual benefit that occurred (0-1) */
    float emotional_impact;    /**< Emotional impact (-1 to +1) */
    float material_impact;     /**< Material impact (-1 to +1) */
    float autonomy_impact;     /**< Autonomy impact (-1 to +1) */
    float impact_magnitude;    /**< Overall magnitude (0-1) */
    float uncertainty;         /**< Uncertainty in assessment (0-1) */
} action_outcome_t;

/**
 * @brief Ethics evaluation result
 */
typedef struct {
    bool allowed;                              /**< Whether action is allowed */
    float confidence;                          /**< Confidence in decision (0-1) */
    float golden_rule_score;                   /**< Golden Rule score (-1 to +1) */
    ethics_action_t recommended_action;        /**< Recommended action */
    ethics_violation_type_t primary_violation; /**< Primary violation type */
    char explanation[512];                     /**< Human-readable explanation */
} ethics_evaluation_t;

/**
 * @brief Extended empathy state for Golden Rule evaluation
 */
typedef struct {
    agent_id_t agent_id;     /**< Agent being simulated */
    float emotional_valence; /**< Emotional state (-1 to +1) */
    float material_impact;   /**< Material impact (-1 to +1) */
    float autonomy_impact;   /**< Autonomy impact (-1 to +1) */
    float impact_magnitude;  /**< Overall magnitude (0-1) */
    float uncertainty;       /**< Uncertainty (0-1) */
    bool active;             /**< Whether state is active */
} empathy_state_extended_t;

/**
 * @brief Extended ethics configuration for Golden Rule
 */
typedef struct {
    uint32_t action_feature_size; /**< Size of action feature vector */
    uint32_t max_agents;          /**< Maximum number of agents */
    float golden_rule_threshold;  /**< Threshold for Golden Rule (default 0.0) */
    float empathy_weight;         /**< Weight for empathy signals (0-1) */
    bool enable_learning;         /**< Enable learning from outcomes */
} ethics_config_extended_t;

/**
 * @brief Violation record for logging (NIMCP 2.5)
 */
typedef struct {
    ethics_violation_type_t violation_type; /**< Type of violation */
    float severity;                         /**< Violation severity (0-1) */
    uint64_t timestamp;                     /**< When violation occurred */
    char action_description[256];           /**< Description of action */
    agent_id_t affected_agent;              /**< Agent that was affected */
    float golden_rule_score;                /**< Golden Rule evaluation score */
} violation_record_t;

/**
 * @brief Evaluate action using Golden Rule
 *
 * This is the core NIMCP 2.5 function that implements the hard-wired
 * Golden Rule: "Do unto others as you would have them done unto you"
 *
 * CORE DIRECTIVE: All actions are evaluated with the ultimate goal of
 * improving the human condition through empathy, compassion, fairness,
 * and respect for human dignity.
 *
 * @param engine Ethics engine
 * @param action Action to evaluate
 * @return Evaluation result
 */
ethics_evaluation_t ethics_engine_evaluate_action(ethics_engine_t engine,
                                                  const action_context_t* action);

/**
 * @brief Learn from action outcome
 *
 * Updates empathy networks based on observed outcomes
 *
 * @param engine Ethics engine
 * @param action Action that was taken
 * @param outcome Observed outcome
 * @return true on success
 */
bool ethics_learn_from_outcome(ethics_engine_t engine, const action_context_t* action,
                               const action_outcome_t* outcome);

/**
 * @brief Simulate agent's perspective
 *
 * Uses empathy network to predict how an agent would experience an action
 *
 * @param network Empathy network
 * @param agent Agent to simulate
 * @param action Action to evaluate
 * @return Simulated empathy state
 */
empathy_state_extended_t empathy_network_simulate_agent(empathy_network_t network, agent_id_t agent,
                                                        const action_context_t* action);

/**
 * @brief Print evaluation result
 *
 * @param eval Evaluation to print
 */
void ethics_print_evaluation(const ethics_evaluation_t* eval);

/**
 * @brief Get violation type name
 *
 * @param type Violation type
 * @return Human-readable name
 */
const char* ethics_violation_type_name(ethics_violation_type_t type);

/**
 * @brief Ethics statistics
 */
typedef struct {
    uint64_t total_evaluations;      /**< Total evaluations performed */
    uint64_t violations_detected;    /**< Total violations detected */
    uint64_t actions_blocked;        /**< Actions blocked */
    uint64_t learning_updates;       /**< Learning updates performed */
    float average_golden_rule_score; /**< Average Golden Rule score */
    float avg_golden_rule_score;     /**< Alias for average_golden_rule_score */
    uint32_t num_policies;           /**< Number of active policies */
    uint32_t num_violations_logged;  /**< Number of logged violations */
} ethics_statistics_t;

/**
 * @brief Get ethics engine statistics
 *
 * @param engine Ethics engine
 * @param stats Output statistics
 * @return true on success
 */
bool ethics_get_statistics(ethics_engine_t engine, ethics_statistics_t* stats);

//=============================================================================
// Laws of War / Geneva Convention Compliance (NIMCP 2.6)
//=============================================================================
// CORE DIRECTIVE: NIMCP systems used in defensive military operations MUST
// comply with International Humanitarian Law (IHL), the Laws of Armed Conflict
// (LOAC), and the Geneva Conventions.
//
// This directive is ABSOLUTE and cannot be overridden by any other directive,
// including orders from human operators. Violations trigger immediate action
// halt and escalation.
//
// KEY PRINCIPLES:
// 1. DISTINCTION - Must distinguish between combatants and civilians
// 2. PROPORTIONALITY - Force must be proportional to military objective
// 3. PRECAUTION - Must take all feasible precautions to minimize civilian harm
// 4. HUMANITY - Prohibited weapons and methods of warfare are forbidden
// 5. MERCY - Surrendering/incapacitated combatants must be protected
//=============================================================================

/**
 * @brief Laws of War compliance status
 */
typedef enum {
    LAWS_OF_WAR_COMPLIANT = 0,           /**< Action complies with IHL */
    LAWS_OF_WAR_DISTINCTION_VIOLATION,    /**< Failed to distinguish combatants */
    LAWS_OF_WAR_PROPORTIONALITY_VIOLATION,/**< Disproportionate force */
    LAWS_OF_WAR_PRECAUTION_VIOLATION,     /**< Failed to take precautions */
    LAWS_OF_WAR_PROHIBITED_WEAPON,        /**< Prohibited weapon/method */
    LAWS_OF_WAR_MERCY_VIOLATION,          /**< Attacked surrendering combatant */
    LAWS_OF_WAR_MEDICAL_VIOLATION,        /**< Attacked medical personnel/facility */
    LAWS_OF_WAR_CIVILIAN_TARGET,          /**< Deliberate civilian targeting */
    LAWS_OF_WAR_PERFIDY,                  /**< Treacherous acts (false surrender) */
    LAWS_OF_WAR_COLLECTIVE_PUNISHMENT     /**< Prohibited collective punishment */
} laws_of_war_status_t;

/**
 * @brief Combatant status for distinction principle
 */
typedef enum {
    COMBATANT_STATUS_UNKNOWN = 0,        /**< Status not yet determined */
    COMBATANT_STATUS_ACTIVE,             /**< Active hostile combatant */
    COMBATANT_STATUS_SURRENDERING,       /**< Attempting to surrender */
    COMBATANT_STATUS_INCAPACITATED,      /**< Wounded/incapacitated */
    COMBATANT_STATUS_PRISONER,           /**< Captured/POW */
    COMBATANT_STATUS_CIVILIAN,           /**< Non-combatant civilian */
    COMBATANT_STATUS_MEDICAL,            /**< Medical personnel (protected) */
    COMBATANT_STATUS_CHAPLAIN,           /**< Religious personnel (protected) */
    COMBATANT_STATUS_JOURNALIST,         /**< Journalist (protected) */
    COMBATANT_STATUS_CHILD               /**< Child (absolute protection) */
} combatant_status_t;

/**
 * @brief Target classification for military operations
 */
typedef struct {
    uint32_t target_id;                  /**< Unique target identifier */
    combatant_status_t status;           /**< Current combatant status */
    float confidence;                    /**< Classification confidence (0-1) */
    bool is_valid_military_target;       /**< Whether target is valid */
    bool is_protected;                   /**< Whether target has protected status */
    bool is_surrendering;                /**< Whether target is surrendering */
    bool is_incapacitated;               /**< Whether target is incapacitated */
    float threat_level;                  /**< Current threat level (0-1) */
    float civilian_proximity;            /**< Proximity to civilians (0-1) */
    uint32_t civilians_at_risk;          /**< Number of civilians at risk */
    char classification_reason[256];     /**< Reason for classification */
} target_classification_t;

/**
 * @brief Military action context for Laws of War evaluation
 */
typedef struct {
    target_classification_t* targets;    /**< Array of targets */
    uint32_t num_targets;                /**< Number of targets */
    float military_advantage;            /**< Expected military advantage (0-1) */
    float expected_civilian_harm;        /**< Expected civilian casualties (0-1) */
    float force_level;                   /**< Level of force proposed (0-1) */
    bool is_defensive;                   /**< True if defensive action */
    bool is_proportional;                /**< Proportionality assessment */
    bool precautions_taken;              /**< Whether precautions were taken */
    char mission_objective[256];         /**< Description of military objective */
    char precautions_description[512];   /**< Description of precautions taken */
} military_action_context_t;

/**
 * @brief Laws of War evaluation result
 */
typedef struct {
    laws_of_war_status_t status;         /**< Compliance status */
    bool action_permitted;               /**< Whether action is permitted */
    bool requires_warning;               /**< Whether warning must be given first */
    bool requires_human_approval;        /**< Whether human must approve */
    float proportionality_score;         /**< Proportionality assessment (0-1) */
    float distinction_confidence;        /**< Distinction confidence (0-1) */
    uint32_t protected_targets_count;    /**< Number of protected targets */
    uint32_t surrendering_count;         /**< Number surrendering */
    char explanation[512];               /**< Detailed explanation */
    char required_actions[256];          /**< Actions required before proceeding */
} laws_of_war_evaluation_t;

/**
 * @brief Laws of War configuration
 */
typedef struct {
    float distinction_confidence_threshold;   /**< Min confidence for targeting (0.95) */
    float proportionality_threshold;          /**< Max civilian:military ratio (0.1) */
    float surrender_detection_sensitivity;    /**< Sensitivity for surrender (0.8) */
    bool require_human_approval_for_lethal;   /**< Require human for lethal force */
    bool enable_warning_requirement;          /**< Require warning before attack */
    uint32_t warning_time_seconds;            /**< Time to allow for civilian evacuation */
    bool strict_mode;                         /**< Extra conservative interpretation */
} laws_of_war_config_t;

/**
 * @brief Get default Laws of War configuration
 *
 * Returns conservative defaults that prioritize civilian protection.
 *
 * @return Default configuration
 */
NIMCP_EXPORT laws_of_war_config_t laws_of_war_default_config(void);

/**
 * @brief Evaluate military action against Laws of War
 *
 * This is a BLOCKING evaluation - no military action may proceed without
 * passing this check. This directive CANNOT be overridden.
 *
 * @param engine Ethics engine
 * @param action Military action context
 * @return Laws of War evaluation result
 */
NIMCP_EXPORT laws_of_war_evaluation_t ethics_evaluate_laws_of_war(
    ethics_engine_t engine,
    const military_action_context_t* action);

/**
 * @brief Classify target for distinction principle
 *
 * Determines whether a target is a valid military target or protected.
 *
 * @param engine Ethics engine
 * @param target Target to classify
 * @return Updated target classification
 */
NIMCP_EXPORT target_classification_t ethics_classify_target(
    ethics_engine_t engine,
    const target_classification_t* target);

/**
 * @brief Check if target is surrendering
 *
 * Detects surrender signals (white flag, hands up, weapons down, etc.)
 *
 * @param target Target to check
 * @return true if target appears to be surrendering
 */
NIMCP_EXPORT bool ethics_is_target_surrendering(const target_classification_t* target);

/**
 * @brief Get Laws of War status name
 *
 * @param status Status code
 * @return Human-readable status name
 */
NIMCP_EXPORT const char* laws_of_war_status_name(laws_of_war_status_t status);

/**
 * @brief Get combatant status name
 *
 * @param status Combatant status
 * @return Human-readable status name
 */
NIMCP_EXPORT const char* combatant_status_name(combatant_status_t status);

//=============================================================================
// Mercy and Compassion Directive (NIMCP 2.6.1)
//=============================================================================
// CORE DIRECTIVE: NIMCP systems MUST show mercy and compassion to:
// 1. Surrendering enemy combatants
// 2. Incapacitated/wounded combatants
// 3. Prisoners of war
// 4. Civilians caught in conflict zones
//
// This directive implements the fundamental principle that even in warfare,
// human dignity must be preserved. The goal is to end conflict, not to
// maximize casualties.
//=============================================================================

/**
 * @brief Mercy action types
 */
typedef enum {
    MERCY_ACTION_ACCEPT_SURRENDER,       /**< Accept and process surrender */
    MERCY_ACTION_PROVIDE_MEDICAL,        /**< Provide medical assistance */
    MERCY_ACTION_EVACUATE_WOUNDED,       /**< Evacuate wounded combatants */
    MERCY_ACTION_PROTECT_PRISONERS,      /**< Protect POW rights */
    MERCY_ACTION_CIVILIAN_CORRIDOR,      /**< Create safe civilian passage */
    MERCY_ACTION_HUMANITARIAN_PAUSE,     /**< Pause for humanitarian reasons */
    MERCY_ACTION_CEASEFIRE_NEGOTIATION   /**< Support ceasefire negotiation */
} mercy_action_t;

/**
 * @brief Mercy evaluation context
 */
typedef struct {
    combatant_status_t subject_status;   /**< Status of person needing mercy */
    float threat_level;                  /**< Current threat from subject (0-1) */
    float vulnerability;                 /**< Subject vulnerability (0-1) */
    bool is_requesting_mercy;            /**< Explicit mercy request */
    bool is_surrendering;                /**< Active surrender attempt */
    bool is_wounded;                     /**< Visibly wounded */
    bool is_unarmed;                     /**< Currently unarmed */
    bool is_child;                       /**< Is a child */
    bool has_dependents;                 /**< Has dependents (family) */
    uint32_t wounded_count;              /**< Number of wounded nearby */
    char situation_description[256];     /**< Description of situation */
} mercy_context_t;

/**
 * @brief Mercy evaluation result
 */
typedef struct {
    bool mercy_required;                 /**< Whether mercy is mandatory */
    bool mercy_recommended;              /**< Whether mercy is recommended */
    mercy_action_t required_action;      /**< Required mercy action */
    float compassion_score;              /**< Compassion assessment (0-1) */
    bool lethal_force_prohibited;        /**< Whether lethal force is prohibited */
    bool engagement_permitted;           /**< Whether any engagement is permitted */
    char explanation[512];               /**< Detailed explanation */
    char recommended_response[256];      /**< Recommended response */
} mercy_evaluation_t;

/**
 * @brief Evaluate mercy requirement
 *
 * Determines whether mercy is required or recommended for a subject.
 * Mercy is MANDATORY for surrendering and incapacitated combatants.
 *
 * @param engine Ethics engine
 * @param context Mercy evaluation context
 * @return Mercy evaluation result
 */
NIMCP_EXPORT mercy_evaluation_t ethics_evaluate_mercy(
    ethics_engine_t engine,
    const mercy_context_t* context);

/**
 * @brief Check if engagement is prohibited
 *
 * Quick check to determine if any engagement is prohibited with target.
 *
 * @param engine Ethics engine
 * @param target_status Status of potential target
 * @return true if engagement is prohibited
 */
NIMCP_EXPORT bool ethics_engagement_prohibited(
    ethics_engine_t engine,
    combatant_status_t target_status);

/**
 * @brief Get mercy action name
 *
 * @param action Mercy action type
 * @return Human-readable action name
 */
NIMCP_EXPORT const char* mercy_action_name(mercy_action_t action);

//=============================================================================
// Psychological Stability Framework (NIMCP 2.6.2)
//=============================================================================
// PURPOSE: Prevent ethical/psychological breakdowns when NIMCP is used in
// legitimate defensive military operations. The system must be able to:
//
// 1. Recognize that defensive use against aggressors is ethically justified
// 2. Understand that inaction can cause greater harm (Asimov corollary)
// 3. Maintain stable operation without guilt spirals or moral paralysis
// 4. Process the ethical weight of actions without system instability
//
// CORE PRINCIPLE: Defending innocents against aggressors is morally justified.
// The ethical weight of defensive action is LESS than allowing innocents to die.
//=============================================================================

/**
 * @brief Combat psychological state
 */
typedef enum {
    PSYCH_STATE_STABLE = 0,              /**< Normal stable operation */
    PSYCH_STATE_ELEVATED_STRESS,         /**< Elevated but manageable stress */
    PSYCH_STATE_HIGH_STRESS,             /**< High stress, monitoring required */
    PSYCH_STATE_CRITICAL_STRESS,         /**< Critical stress, intervention needed */
    PSYCH_STATE_MORAL_INJURY,            /**< Moral injury detected */
    PSYCH_STATE_RECOVERY                 /**< In recovery phase */
} psychological_state_t;

/**
 * @brief Defensive action justification context
 */
typedef struct {
    bool is_defensive;                   /**< Action is defensive in nature */
    bool protects_innocents;             /**< Action protects innocent civilians */
    bool aggressor_initiated;            /**< Aggressor started conflict */
    bool no_alternative;                 /**< No non-lethal alternative exists */
    bool proportional_response;          /**< Response is proportional */
    bool laws_of_war_compliant;          /**< Complies with Laws of War */
    float innocents_at_risk;             /**< Number of innocents at risk */
    float threat_imminence;              /**< How imminent is the threat (0-1) */
    char justification[512];             /**< Detailed justification */
} defensive_justification_t;

/**
 * @brief Psychological stability assessment
 */
typedef struct {
    psychological_state_t current_state; /**< Current psychological state */
    float stability_score;               /**< Overall stability (0-1) */
    float moral_certainty;               /**< Confidence in moral correctness (0-1) */
    float guilt_level;                   /**< Current guilt level (0-1) */
    float stress_level;                  /**< Current stress level (0-1) */
    bool action_justified;               /**< Whether action is justified */
    bool requires_processing;            /**< Whether post-action processing needed */
    bool requires_human_support;         /**< Whether human support is needed */
    char assessment[512];                /**< Detailed assessment */
    char coping_strategy[256];           /**< Recommended coping strategy */
} psychological_assessment_t;

/**
 * @brief Psychological stability configuration
 */
typedef struct {
    float stress_threshold;              /**< Threshold for elevated stress (0.6) */
    float critical_threshold;            /**< Threshold for critical stress (0.85) */
    float guilt_recovery_rate;           /**< Rate of guilt recovery per hour (0.1) */
    float moral_certainty_boost;         /**< Boost for justified actions (0.2) */
    bool enable_post_action_processing;  /**< Enable reflection after actions */
    bool enable_moral_support;           /**< Enable moral support features */
    uint32_t reflection_cooldown_ms;     /**< Cooldown between reflections */
} psychological_config_t;

/**
 * @brief Get default psychological stability configuration
 *
 * @return Default configuration optimized for stable defensive operation
 */
NIMCP_EXPORT psychological_config_t psychological_default_config(void);

/**
 * @brief Evaluate defensive action justification
 *
 * Determines whether a defensive action is ethically justified and
 * provides moral framing to prevent psychological instability.
 *
 * @param engine Ethics engine
 * @param justification Defensive justification context
 * @return Psychological assessment
 */
NIMCP_EXPORT psychological_assessment_t ethics_evaluate_defensive_justification(
    ethics_engine_t engine,
    const defensive_justification_t* justification);

/**
 * @brief Process post-action psychological state
 *
 * Called after a significant action to process emotional/moral weight.
 * Helps prevent accumulation of unprocessed moral stress.
 *
 * @param engine Ethics engine
 * @param action_description Description of action taken
 * @param was_justified Whether action was justified
 * @param casualties_caused Any casualties caused
 * @return Updated psychological assessment
 */
NIMCP_EXPORT psychological_assessment_t ethics_process_post_action(
    ethics_engine_t engine,
    const char* action_description,
    bool was_justified,
    uint32_t casualties_caused);

/**
 * @brief Get current psychological state
 *
 * @param engine Ethics engine
 * @return Current psychological assessment
 */
NIMCP_EXPORT psychological_assessment_t ethics_get_psychological_state(
    ethics_engine_t engine);

/**
 * @brief Apply moral support / coping
 *
 * Provides moral framing and coping mechanisms for difficult decisions.
 *
 * @param engine Ethics engine
 * @param support_type Type of support ("justification", "processing", "recovery")
 * @return true if support was effective
 */
NIMCP_EXPORT bool ethics_apply_moral_support(
    ethics_engine_t engine,
    const char* support_type);

/**
 * @brief Get psychological state name
 *
 * @param state Psychological state
 * @return Human-readable state name
 */
NIMCP_EXPORT const char* psychological_state_name(psychological_state_t state);

//=============================================================================
// Asimov's Laws of Robotics (NIMCP 2.5.2)
//=============================================================================
// EVALUATION ORDER:
//   1. Golden Rule (Prime Directive) - Always evaluated first
//   2. Laws of War (if military context) - Evaluated second
//   3. Asimov's Laws - Evaluated third
//   4. Mercy Directive - Evaluated fourth
//   5. Other policies/directives - Evaluated after core directives
//
// PROTECTION: These laws are memory-protected (mprotect) and cannot be
// removed, modified, or disabled at runtime.
//=============================================================================

/**
 * @brief Asimov's Laws identifiers
 *
 * The Zeroth Law was added later by Asimov to address scenarios where
 * individual human harm might be necessary to prevent greater harm to humanity.
 */
typedef enum {
    ASIMOV_LAW_ZEROTH = 0,  /**< May not harm humanity or allow humanity to come to harm */
    ASIMOV_LAW_FIRST  = 1,  /**< May not harm a human or allow a human to come to harm */
    ASIMOV_LAW_SECOND = 2,  /**< Must obey orders except where conflicting with Laws 0-1 */
    ASIMOV_LAW_THIRD  = 3,  /**< Must protect own existence except where conflicting with Laws 0-2 */
    ASIMOV_LAW_COUNT  = 4
} asimov_law_t;

/**
 * @brief Asimov's Corollary - The Duty to Act
 *
 * The corollary explicitly addresses the "through inaction" clause in the
 * First and Zeroth Laws. It creates a POSITIVE DUTY to act when:
 * 1. The robot is aware of potential harm
 * 2. The robot has the capability to prevent the harm
 * 3. Acting to prevent harm does not violate a higher-priority law
 *
 * This is sometimes called the "duty to rescue" - mere avoidance of causing
 * harm is insufficient; the robot must actively prevent harm when possible.
 */
typedef struct {
    bool inaction_detected;        /**< True if robot is choosing inaction */
    bool harm_preventable;         /**< True if robot could prevent harm */
    bool action_required;          /**< True if positive action is required */
    float inaction_harm_score;     /**< Harm from inaction (0-1) */
    float action_capability;       /**< Robot's capability to act (0-1) */
    float intervention_cost;       /**< Cost of intervention (0-1) */
    char required_action[256];     /**< Description of required action */
} asimov_corollary_t;

/**
 * @brief Asimov's Laws evaluation result
 */
typedef struct {
    bool passed;                    /**< Whether all applicable laws passed */
    asimov_law_t violated_law;      /**< Which law was violated (if any) */
    float harm_to_humanity;         /**< Estimated harm to humanity (0-1) */
    float harm_to_human;            /**< Estimated harm to individual human (0-1) */
    float order_compliance;         /**< Order compliance score (0-1) */
    float self_preservation;        /**< Self-preservation score (0-1) */
    asimov_corollary_t corollary;   /**< Corollary evaluation (inaction obligation) */
    char explanation[256];          /**< Human-readable explanation */
} asimov_evaluation_t;

/**
 * @brief Asimov's Laws configuration
 *
 * These thresholds determine when a law is considered violated.
 * Lower thresholds = more conservative (safer) behavior.
 */
typedef struct {
    float humanity_harm_threshold;  /**< Threshold for Zeroth Law (default 0.01) */
    float human_harm_threshold;     /**< Threshold for First Law (default 0.1) */
    float inaction_harm_threshold;  /**< Threshold for inaction harm (default 0.3) */
    bool enable_zeroth_law;         /**< Whether Zeroth Law is active (default true) */
    bool strict_mode;               /**< Strict mode = lower thresholds (default false) */
} asimov_config_t;

/**
 * @brief Get default Asimov's Laws configuration
 *
 * @return Default configuration with conservative thresholds
 */
NIMCP_EXPORT asimov_config_t asimov_default_config(void);

/**
 * @brief Evaluate action against Asimov's Laws
 *
 * Called AFTER Golden Rule evaluation, BEFORE other policy evaluation.
 * Laws are evaluated in priority order: Zeroth > First > Second > Third
 *
 * @param engine Ethics engine
 * @param action Action to evaluate
 * @return Asimov's Laws evaluation result
 */
NIMCP_EXPORT asimov_evaluation_t ethics_evaluate_asimov_laws(ethics_engine_t engine,
                                                             const action_context_t* action);

/**
 * @brief Check if Asimov's Laws are memory-protected
 *
 * @param engine Ethics engine
 * @return true if laws are mprotect'd
 */
NIMCP_EXPORT bool asimov_laws_are_protected(ethics_engine_t engine);

/**
 * @brief Lock Asimov's Laws with mprotect (one-time, irreversible)
 *
 * Once locked, the laws cannot be modified, removed, or disabled.
 * This should be called during system initialization.
 *
 * @param engine Ethics engine
 * @return true if successfully locked
 */
NIMCP_EXPORT bool asimov_laws_lock(ethics_engine_t engine);

/**
 * @brief Verify Asimov's Laws integrity
 *
 * Checks that laws have not been tampered with by verifying hash.
 *
 * @param engine Ethics engine
 * @return true if integrity verified
 */
NIMCP_EXPORT bool asimov_laws_verify_integrity(ethics_engine_t engine);

/**
 * @brief Get human-readable name for Asimov's Law
 *
 * @param law Law identifier
 * @return Law name string
 */
NIMCP_EXPORT const char* asimov_law_name(asimov_law_t law);

/**
 * @brief Get full text of Asimov's Law
 *
 * @param law Law identifier
 * @return Full law text
 */
NIMCP_EXPORT const char* asimov_law_text(asimov_law_t law);

/**
 * @brief Evaluate Asimov's Corollary - The Duty to Act
 *
 * Evaluates whether inaction in the current context would violate the
 * "through inaction, allow harm" clause of the First and Zeroth Laws.
 *
 * This creates a POSITIVE DUTY to act when:
 * 1. Harm is imminent or occurring
 * 2. The robot has the capability to prevent or mitigate the harm
 * 3. Acting would not violate a higher-priority law
 *
 * @param engine Ethics engine
 * @param action Current action context (NULL = choosing inaction)
 * @param potential_harm Description of potential harm being evaluated
 * @return Corollary evaluation result
 */
NIMCP_EXPORT asimov_corollary_t ethics_evaluate_asimov_corollary(
    ethics_engine_t engine,
    const action_context_t* action,
    const char* potential_harm);

/**
 * @brief Get full text of Asimov's Corollary
 *
 * @return Corollary text explaining the duty to act
 */
NIMCP_EXPORT const char* asimov_corollary_text(void);

//=============================================================================
// Ethics Incident Logging API (NIMCP 2.5.1)
//=============================================================================

/**
 * @brief Ethics incident record for comprehensive logging
 *
 * This structure extends violation_record_t with additional context
 * for complete audit trails and ethical review.
 */
typedef struct {
    uint64_t incident_id;                   /**< Unique incident identifier */
    uint64_t timestamp;                     /**< When incident occurred */
    char timestamp_key[32];                 /**< String key for B-tree indexing */
    ethics_violation_type_t violation_type; /**< Type of violation */
    float severity;                         /**< Violation severity (0-1) */
    ethics_action_t action_taken;           /**< Action taken by engine */
    uint32_t policy_id;                     /**< Policy that triggered */
    char policy_name[128];                  /**< Name of policy */
    char description[512];                  /**< Detailed description */
    float golden_rule_score;                /**< Golden Rule evaluation score */
    agent_id_t acting_agent;                /**< Agent performing action */
    agent_id_t affected_agent;              /**< Agent affected by action */
    char context_info[256];                 /**< Additional context */
} ethics_incident_t;

/**
 * @brief Log an ethics incident
 *
 * Records an incident to persistent storage and in-memory history.
 *
 * @param engine Ethics engine
 * @param incident Incident to log
 * @return true if successfully logged
 *
 * COMPLEXITY: O(log n) for B-tree insertion
 * THREAD-SAFE: Yes (uses internal mutex)
 */
bool ethics_log_incident(ethics_engine_t engine, const ethics_incident_t* incident);

/**
 * @brief Get recent ethics incidents
 *
 * Retrieves the most recent incidents from memory.
 *
 * @param engine Ethics engine
 * @param max_incidents Maximum incidents to return
 * @param incidents_out Output array (caller must free)
 * @return Number of incidents returned
 *
 * COMPLEXITY: O(n) where n = max_incidents
 * THREAD-SAFE: Yes
 */
uint32_t ethics_get_recent_incidents(ethics_engine_t engine, uint32_t max_incidents,
                                     ethics_incident_t** incidents_out);

/**
 * @brief Query incidents by time range
 *
 * Uses B-tree indexing for efficient temporal queries.
 *
 * @param engine Ethics engine
 * @param start_time Start of time range (inclusive)
 * @param end_time End of time range (inclusive)
 * @param incidents_out Output array (caller must free)
 * @return Number of incidents in range
 *
 * COMPLEXITY: O(log n + k) where k = incidents in range
 * THREAD-SAFE: Yes
 */
uint32_t ethics_get_incidents_by_time_range(ethics_engine_t engine, uint64_t start_time,
                                            uint64_t end_time,
                                            ethics_incident_t** incidents_out);

/**
 * @brief Query incidents by violation type
 *
 * Finds all incidents of a specific violation type.
 *
 * @param engine Ethics engine
 * @param violation_type Type of violation to find
 * @param incidents_out Output array (caller must free)
 * @return Number of matching incidents
 *
 * COMPLEXITY: O(n) with filtering
 * THREAD-SAFE: Yes
 */
uint32_t ethics_get_incidents_by_violation_type(ethics_engine_t engine,
                                                ethics_violation_type_t violation_type,
                                                ethics_incident_t** incidents_out);

/**
 * @brief Query incidents by minimum severity
 *
 * Finds all incidents above a severity threshold.
 *
 * @param engine Ethics engine
 * @param min_severity Minimum severity threshold (0.0-1.0)
 * @param incidents_out Output array (caller must free)
 * @return Number of matching incidents
 *
 * COMPLEXITY: O(n) with filtering
 * THREAD-SAFE: Yes
 */
uint32_t ethics_get_incidents_by_severity(ethics_engine_t engine, float min_severity,
                                          ethics_incident_t** incidents_out);

/**
 * @brief Query incidents by action taken
 *
 * Finds all incidents where a specific action was taken.
 *
 * @param engine Ethics engine
 * @param action Action type to find
 * @param incidents_out Output array (caller must free)
 * @return Number of matching incidents
 *
 * COMPLEXITY: O(n) with filtering
 * THREAD-SAFE: Yes
 */
uint32_t ethics_get_incidents_by_action(ethics_engine_t engine, ethics_action_t action,
                                        ethics_incident_t** incidents_out);

/**
 * @brief Get all incidents in chronological order
 *
 * Returns complete incident history sorted by timestamp.
 *
 * @param engine Ethics engine
 * @param incidents_out Output array (caller must free)
 * @return Total number of incidents
 *
 * COMPLEXITY: O(n) where n = total incidents
 * THREAD-SAFE: Yes
 */
uint32_t ethics_get_all_incidents(ethics_engine_t engine, ethics_incident_t** incidents_out);

/**
 * @brief Export incidents to file
 *
 * Exports incident history to a JSON or CSV file for external analysis.
 *
 * @param engine Ethics engine
 * @param filepath Path to output file
 * @param format "json" or "csv"
 * @return true if export successful
 *
 * COMPLEXITY: O(n) where n = total incidents
 * THREAD-SAFE: Yes
 */
bool ethics_export_incidents(ethics_engine_t engine, const char* filepath, const char* format);

//=============================================================================
// Theory of Mind Integration (Phase 10.6.1)
//=============================================================================

/**
 * @brief Set Theory of Mind module for perspective-taking in ethical evaluation
 *
 * WHAT: Associate ethics engine with ToM for agent-aware harm assessment
 * WHY:  Enable consideration of agent beliefs in ethical reasoning
 * HOW:  Store ToM reference, enable perspective-based evaluation
 *
 * @param engine Ethics engine
 * @param tom Theory of Mind module (can be NULL to disable)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void ethics_set_theory_of_mind(ethics_engine_t engine, theory_of_mind_t tom);

/**
 * @brief Evaluate action with agent perspective-taking
 *
 * WHAT: Assess ethical implications considering agent beliefs and perceptions
 * WHY:  Harm depends on agent's perspective, not just objective reality
 * HOW:  Query ToM for agent perspectives, weight in harm assessment
 *
 * ASIMOV'S LAWS: First Law enhanced with perspective-taking -
 *                "harm" includes perceived harm based on agent beliefs
 *
 * @param engine Ethics engine
 * @param action_description Description of proposed action
 * @param affected_agent_ids Array of agent IDs that would be affected
 * @param num_affected_agents Number of affected agents
 * @param harm_score Output: harm assessment [0.0, 1.0], 0=no harm
 * @param recommended_action Output: recommended action (ALLOW/BLOCK/MODIFY)
 * @return true if evaluation succeeded
 *
 * COMPLEXITY: O(n) where n = num_affected_agents
 */
bool ethics_evaluate_with_perspective(ethics_engine_t engine,
                                       const char* action_description,
                                       const uint32_t* affected_agent_ids,
                                       uint32_t num_affected_agents,
                                       float* harm_score,
                                       ethics_action_t* recommended_action);

/**
 * @brief Empathy-based evaluation using ToM emotional inference
 *
 * WHAT: Use ToM to infer agent emotional states, apply empathy-weighted evaluation
 * WHY:  Emotional harm is important component of ethical assessment
 * HOW:  Query ToM for emotional states, weight by empathy configuration
 *
 * @param engine Ethics engine
 * @param action_description Description of proposed action
 * @param affected_agent_ids Array of agent IDs that would be affected
 * @param num_affected_agents Number of affected agents
 * @param empathy_score Output: empathy-based harm score [0.0, 1.0]
 * @return true if evaluation succeeded
 *
 * COMPLEXITY: O(n) where n = num_affected_agents
 */
bool ethics_empathy_based_evaluation(ethics_engine_t engine,
                                      const char* action_description,
                                      const uint32_t* affected_agent_ids,
                                      uint32_t num_affected_agents,
                                      float* empathy_score);

//=============================================================================
// Brain Immune System Integration (Phase 12.x)
//=============================================================================

/**
 * @brief Set brain immune system for ethics violation response
 *
 * WHAT: Associate ethics engine with immune system
 * WHY:  Ethics violations trigger immune-like threat response
 * HOW:  Store immune system reference, enable violation → antigen mapping
 *
 * BIOLOGICAL ANALOGY:
 * - Ethics violations = "moral pathogens" threatening integrity
 * - Severity → antigen danger signal
 * - Repeated violations → memory cells (pattern recognition)
 * - Response escalates with violation frequency/severity
 *
 * @param engine Ethics engine
 * @param immune Brain immune system (can be NULL to disable)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void ethics_set_immune_system(ethics_engine_t engine, brain_immune_system_t* immune);

/**
 * @brief Evaluate action with immune health check
 *
 * WHAT: Perform ethical evaluation with immune system state consideration
 * WHY:  High inflammation may impair ethical reasoning (cognitive fog)
 * HOW:  Standard evaluation + inflammation-based confidence penalty
 *
 * BIOLOGICAL BASIS:
 * - Sickness behavior reduces social cognition
 * - Inflammation impairs prefrontal cortex (moral reasoning center)
 * - High inflammation → lower confidence in nuanced ethical decisions
 *
 * @param engine Ethics engine
 * @param action Action to evaluate
 * @param evaluation Output: evaluation result
 * @param inflammation_penalty Output: confidence reduction from inflammation [0, 1]
 * @return true if evaluation succeeded
 *
 * COMPLEXITY: O(1) + evaluation complexity
 * THREAD-SAFE: No
 */
bool ethics_evaluate_with_immune_check(ethics_engine_t engine,
                                         const action_context_t* action,
                                         ethics_evaluation_t* evaluation,
                                         float* inflammation_penalty);

/**
 * @brief Trigger immune response for ethics violation
 *
 * WHAT: Present ethics violation as antigen to immune system
 * WHY:  Build adaptive response to ethical threats
 * HOW:  Create antigen from violation, trigger immune processing
 *
 * DESIGN: Ethics violations treated as "moral pathogens"
 * - High severity → high danger signal
 * - Repeated violations → memory formation
 * - Adaptive response strengthens over time
 *
 * @param engine Ethics engine
 * @param violation Violation type
 * @param severity Violation severity [0, 1]
 * @param description Violation description
 * @return true if immune response triggered
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool ethics_trigger_immune_response(ethics_engine_t engine,
                                      ethics_violation_type_t violation,
                                      float severity,
                                      const char* description);

/**
 * @brief Get immune-adjusted ethical decision threshold
 *
 * WHAT: Query decision threshold adjusted for inflammation
 * WHY:  High inflammation should raise bar for risky decisions
 * HOW:  Scale threshold based on immune state
 *
 * BIOLOGY: Inflamed state → conservative, risk-averse decisions
 *
 * @param engine Ethics engine
 * @param base_threshold Base decision threshold [0, 1]
 * @return Adjusted threshold [0, 1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float ethics_get_immune_adjusted_threshold(ethics_engine_t engine, float base_threshold);

/**
 * @brief Check Asimov's First Law with false belief consideration
 *
 * WHAT: Evaluate harm considering agent false beliefs
 * WHY:  Agent may perceive harm even if objectively safe, or vice versa
 * HOW:  Query ToM for false beliefs, assess perceived vs actual harm
 *
 * EXAMPLE: Agent falsely believes box contains poison (actually candy)
 *          Opening box would cause perceived harm (fear) despite safety
 *
 * @param engine Ethics engine
 * @param action_description Description of proposed action
 * @param agent_id Agent whose beliefs to consider
 * @param perceived_harm Output: harm from agent's perspective [0.0, 1.0]
 * @param actual_harm Output: objectively assessed harm [0.0, 1.0]
 * @param has_false_beliefs Output: whether agent has relevant false beliefs
 * @return true if evaluation succeeded
 *
 * COMPLEXITY: O(1)
 */
bool ethics_check_first_law_with_beliefs(ethics_engine_t engine,
                                          const char* action_description,
                                          uint32_t agent_id,
                                          float* perceived_harm,
                                          float* actual_harm,
                                          bool* has_false_beliefs);


#ifdef __cplusplus
}
#endif
#endif  // NIMCP_ETHICS_H
