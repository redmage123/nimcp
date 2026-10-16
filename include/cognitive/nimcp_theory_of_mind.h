/**
 * @file nimcp_theory_of_mind.h
 * @brief Theory of Mind (ToM) module - inferring mental states of others
 *
 * WHAT: Belief-Desire-Intention (BDI) model for social cognition
 * WHY:  Enable understanding of others' beliefs, goals, emotions, and intentions
 * HOW:  Simulate other agents' mental states through perspective-taking
 *
 * BIOLOGICAL BASIS:
 * - Temporoparietal junction (TPJ) supports perspective-taking
 * - Medial prefrontal cortex (mPFC) models others' mental states
 * - Mirror neuron system enables empathy and action understanding
 * - False belief task: Understanding that others can hold incorrect beliefs
 *
 * CAPABILITIES:
 * - Emotion inference: Infer others' emotional states from behavior
 * - Goal inference: Deduce intentions and desires from actions
 * - Belief tracking: Model what others know vs. what actually is true
 * - Action prediction: Anticipate what others will do next
 * - Empathy: Mirror and respond to others' emotional states
 * - False belief understanding: Recognize belief-reality mismatches
 *
 * PHASE: 10.6 (Theory of Mind)
 * DEPENDENCIES: Emotional Tagging (Phase 10.3), Executive Functions (Phase 10.3)
 * TRAINING_IMPACT: None (inference-only, meta-cognitive reasoning)
 *
 * @author NIMCP Development Team - Phase 10.6
 * @date 2025-11-09
 * @version 2.7.0 Phase 10.6
 */

#ifndef NIMCP_THEORY_OF_MIND_H
#define NIMCP_THEORY_OF_MIND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // for size_t

/* Imagination callbacks for ToM integration */
#include "cognitive/imagination/nimcp_imagination_callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declare brain struct to avoid circular dependency
// NOTE: brain.h defines: typedef struct brain_struct* brain_t;
// We only forward-declare the struct here
struct brain_struct;

// Forward declare imagination engine for ToM integration
struct imagination_engine;
typedef struct imagination_engine imagination_engine_t;

// Forward declare imagination scenario for perspective simulation
struct imagination_scenario;
typedef struct imagination_scenario imagination_scenario_t;

// Forward declare tensor for state representation
// Use the correct struct name (nimcp_tensor_s) and guard against redefinition
#ifndef NIMCP_TENSOR_T_DEFINED
struct nimcp_tensor_s;
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

// If brain_t hasn't been typedef'd yet (e.g., brain.h not included),
// create a compatible typedef. This allows theory_of_mind.h to be
// included standalone while remaining compatible with brain.h.
#ifndef NIMCP_BRAIN_H
typedef struct brain_struct* brain_t;
#endif

//=============================================================================
// Emotion Types (Simplified for Theory of Mind)
//=============================================================================

/**
 * @brief Basic emotional states for Theory of Mind inference
 *
 * Based on Ekman's basic emotions + social emotions
 */
typedef enum {
    TOM_EMOTION_UNKNOWN,     /**< Cannot infer emotion */
    TOM_EMOTION_NEUTRAL,     /**< No strong emotion detected */
    TOM_EMOTION_JOY,         /**< Positive, high arousal */
    TOM_EMOTION_SADNESS,     /**< Negative, low arousal */
    TOM_EMOTION_ANGER,       /**< Negative, high arousal */
    TOM_EMOTION_FEAR,        /**< Negative, high arousal, threat-focused */
    TOM_EMOTION_SURPRISE,    /**< Neutral/positive, high arousal, unexpected */
    TOM_EMOTION_DISGUST,     /**< Negative, moderate arousal, aversion */
    TOM_EMOTION_ANXIETY,     /**< Negative, moderate arousal, anticipatory */
    TOM_EMOTION_PRIDE,       /**< Positive, moderate arousal, self-focused */
    TOM_EMOTION_SHAME,       /**< Negative, moderate arousal, self-focused */
    TOM_EMOTION_CALM,        /**< Positive, low arousal, peaceful/relaxed */
    TOM_EMOTION_COUNT
} tom_emotion_t;

//=============================================================================
// Belief-Desire-Intention (BDI) Model
//=============================================================================

/**
 * @brief Belief state (what the other agent believes)
 *
 * WHAT: Representation of another agent's beliefs about the world
 * WHY:  Others' beliefs may differ from reality (false beliefs)
 * HOW:  Track observed beliefs vs. actual world state
 */
typedef struct {
    char belief_content[256];    /**< What they believe ("the box contains candy") */
    float confidence;            /**< Confidence in this belief [0.0, 1.0] */
    bool is_false_belief;        /**< Does this belief contradict reality? */
    uint64_t last_updated_ms;    /**< When this belief was last observed */
} tom_belief_t;

/**
 * @brief Desire state (what the other agent wants)
 *
 * WHAT: Goals and preferences of another agent
 * WHY:  Understanding desires helps predict actions
 * HOW:  Infer from observed goal-directed behavior
 */
typedef struct {
    char goal_description[256];  /**< What they want ("get the candy") */
    float intensity;             /**< How badly they want it [0.0, 1.0] */
    float satisfaction;          /**< How satisfied is this desire [0.0, 1.0] */
} tom_desire_t;

/**
 * @brief Intention state (what the other agent plans to do)
 *
 * WHAT: Planned actions of another agent
 * WHY:  Intentions bridge beliefs/desires and actions
 * HOW:  Infer from current context and observed preparations
 */
typedef struct {
    char action_description[256]; /**< What they plan to do ("open the box") */
    float likelihood;             /**< Probability of executing [0.0, 1.0] */
    bool action_in_progress;      /**< Are they currently doing this? */
} tom_intention_t;

//=============================================================================
// Theory of Mind State
//=============================================================================

/**
 * @brief Complete Theory of Mind model of another agent
 *
 * WHAT: Mental state representation (BDI + emotion + knowledge)
 * WHY:  Unified model for social reasoning
 * HOW:  Maintain beliefs, desires, intentions, emotions, and knowledge state
 *
 * DESIGN: Opaque handle for encapsulation
 */
typedef struct theory_of_mind_s* theory_of_mind_t;

/**
 * @brief Observable behavior input for ToM inference
 *
 * WHAT: Behavioral cues that reveal mental states
 * WHY:  ToM infers from observable actions and context
 * HOW:  Package multiple behavioral signals
 */
typedef struct {
    const float* action_vector;   /**< Action representation (e.g., from motor cortex) */
    uint32_t action_dim;           /**< Dimension of action vector */
    const char* verbal_context;    /**< Spoken words or dialogue context */
    tom_emotion_t observed_emotion; /**< Emotion displayed (if observable) */
    const float* situational_context; /**< Environmental/social context */
    uint32_t context_dim;          /**< Dimension of context vector */
} tom_observation_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create Theory of Mind module
 *
 * WHAT: Initialize ToM with reference to self brain
 * WHY:  ToM simulates others using self-model as template
 * HOW:  Allocate structures, copy self-brain parameters
 *
 * @param self_brain Brain instance of self (for simulation template)
 * @return ToM handle, or NULL on error
 *
 * SIDE EFFECTS:
 * - Allocates memory
 * - May create internal brain simulation
 *
 * COMPLEXITY: O(1)
 */
theory_of_mind_t tom_create(brain_t self_brain);

/**
 * @brief Destroy Theory of Mind module
 *
 * WHAT: Free all ToM resources
 * WHY:  Clean memory management
 * HOW:  Free beliefs, desires, intentions, and simulated brain
 *
 * @param tom ToM handle to destroy
 *
 * SIDE EFFECTS:
 * - Frees memory
 * - Invalidates handle
 *
 * COMPLEXITY: O(1)
 */
void tom_destroy(theory_of_mind_t tom);

/**
 * @brief Observe another agent's behavior
 *
 * WHAT: Update ToM model based on observed behavior
 * WHY:  New observations refine mental state estimates
 * HOW:  Process behavior → update beliefs/desires/intentions
 *
 * @param tom ToM handle
 * @param observation Behavioral observation
 * @return true if observation processed successfully
 *
 * SIDE EFFECTS:
 * - Updates internal belief/desire/intention state
 * - May trigger empathy response
 *
 * COMPLEXITY: O(1) for behavior encoding
 */
bool tom_observe(theory_of_mind_t tom, const tom_observation_t* observation);

/**
 * @brief Infer other agent's emotional state
 *
 * WHAT: Estimate current emotion from observed behavior
 * WHY:  Emotions guide social interaction
 * HOW:  Analyze behavior patterns → map to emotion categories
 *
 * @param tom ToM handle
 * @param confidence Output: confidence in inference [0.0, 1.0]
 * @return Inferred emotion category
 *
 * COMPLEXITY: O(1)
 */
tom_emotion_t tom_infer_emotion(theory_of_mind_t tom, float* confidence);

/**
 * @brief Infer other agent's goal/desire
 *
 * WHAT: Deduce what the other agent wants to achieve
 * WHY:  Goals explain and predict behavior
 * HOW:  Analyze action patterns → infer desired end state
 *
 * @param tom ToM handle
 * @param goal_buffer Output: string description of goal (min 256 bytes)
 * @param buffer_size Size of goal_buffer
 * @param confidence Output: confidence in inference [0.0, 1.0]
 * @return true if goal inferred successfully
 *
 * COMPLEXITY: O(1)
 */
bool tom_infer_goal(theory_of_mind_t tom, char* goal_buffer, size_t buffer_size, float* confidence);

/**
 * @brief Predict other agent's next action
 *
 * WHAT: Forecast what they will do next
 * WHY:  Enables proactive social coordination
 * HOW:  BDI model + current context → likely action
 *
 * @param tom ToM handle
 * @param predicted_action Output: action descriptor
 * @param action_buffer_size Size of action descriptor buffer
 * @param likelihood Output: probability of this action [0.0, 1.0]
 * @return true if prediction generated
 *
 * COMPLEXITY: O(1)
 */
bool tom_predict_action(theory_of_mind_t tom, char* predicted_action, size_t action_buffer_size, float* likelihood);

/**
 * @brief Generate empathetic response to observed emotion
 *
 * WHAT: Mirror and respond to other's emotional state
 * WHY:  Empathy builds social rapport
 * HOW:  Observe emotion → generate appropriate emotional response
 *
 * @param tom ToM handle
 * @param observed_emotion Emotion detected in other agent
 * @param empathy_response Output: recommended empathetic response emotion
 * @return true if empathy generated
 *
 * SIDE EFFECTS:
 * - May update self-brain's emotional state
 *
 * COMPLEXITY: O(1)
 */
bool tom_empathize(theory_of_mind_t tom, tom_emotion_t observed_emotion, tom_emotion_t* empathy_response);

/**
 * @brief Test for false belief understanding
 *
 * WHAT: Check if ToM recognizes belief-reality mismatch
 * WHY:  False belief is hallmark of mature Theory of Mind
 * HOW:  Compare agent's believed state vs. actual state
 *
 * @param tom ToM handle
 * @param reality_state What actually is true
 * @param believed_state What the other agent believes
 * @param is_false_belief Output: does belief contradict reality?
 * @return true if false belief test completed
 *
 * EXAMPLE:
 * - Reality: Box contains pencils
 * - Belief: Box contains candy (false belief)
 *
 * COMPLEXITY: O(1)
 */
bool tom_detect_false_belief(theory_of_mind_t tom,
                             const char* reality_state,
                             const char* believed_state,
                             bool* is_false_belief);

/**
 * @brief Get current BDI state
 *
 * WHAT: Retrieve belief, desire, and intention state
 * WHY:  External systems may need complete mental model
 * HOW:  Copy internal state to output structs
 *
 * @param tom ToM handle
 * @param belief Output: current belief (NULL to skip)
 * @param desire Output: current desire (NULL to skip)
 * @param intention Output: current intention (NULL to skip)
 * @return true if state retrieved
 *
 * COMPLEXITY: O(1)
 */
bool tom_get_bdi_state(theory_of_mind_t tom,
                       tom_belief_t* belief,
                       tom_desire_t* desire,
                       tom_intention_t* intention);

/**
 * @brief Get perspective-taking capacity
 *
 * WHAT: Check if ToM can distinguish self vs. other perspectives
 * WHY:  Perspective confusion indicates ToM failure
 * HOW:  Test if other's beliefs are tracked separately from reality
 *
 * @param tom ToM handle
 * @return Perspective-taking score [0.0, 1.0], 1.0 = perfect separation
 *
 * COMPLEXITY: O(1)
 */
float tom_get_perspective_score(theory_of_mind_t tom);

//=============================================================================
// Statistics & Diagnostics
//=============================================================================

/**
 * @brief Theory of Mind statistics
 */
typedef struct {
    uint32_t total_observations;      /**< Total behavior observations processed */
    uint32_t emotion_inferences;       /**< Number of emotion inferences made */
    uint32_t goal_inferences;          /**< Number of goal inferences made */
    uint32_t action_predictions;       /**< Number of action predictions made */
    uint32_t empathy_responses;        /**< Number of empathetic responses generated */
    uint32_t false_beliefs_detected;   /**< Number of false beliefs recognized */
    float average_inference_confidence; /**< Mean confidence across inferences */
    float perspective_taking_score;    /**< Current perspective-taking ability [0,1] */
} tom_statistics_t;

/**
 * @brief Get Theory of Mind statistics
 *
 * WHAT: Retrieve diagnostic information
 * WHY:  Monitor ToM performance
 * HOW:  Copy internal counters
 *
 * @param tom ToM handle
 * @param stats Output: statistics structure
 * @return true if stats retrieved
 *
 * COMPLEXITY: O(1)
 */
bool tom_get_statistics(theory_of_mind_t tom, tom_statistics_t* stats);

/**
 * @brief Update Theory of Mind self-model with own decision
 *
 * WHAT: Record brain's own decision as mental state
 * WHY:  Build self-model required for understanding others (simulation theory)
 * HOW:  Store features→decision mapping in BDI framework
 *
 * @param tom ToM handle
 * @param features Input features that led to decision
 * @param num_features Number of features
 * @param action_label Human-readable action/decision label
 * @param confidence Decision confidence [0.0, 1.0]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 */
bool tom_update_self_model(theory_of_mind_t tom,
                           const float* features,
                           uint32_t num_features,
                           const char* action_label,
                           float confidence);

/**
 * @brief Reset Theory of Mind state
 *
 * WHAT: Clear all BDI state and start fresh
 * WHY:  New agent requires new model
 * HOW:  Reset beliefs, desires, intentions to defaults
 *
 * @param tom ToM handle
 * @return true if reset successful
 *
 * SIDE EFFECTS:
 * - Clears BDI state
 * - Preserves statistics (unless requested to clear)
 *
 * COMPLEXITY: O(1)
 */
bool tom_reset(theory_of_mind_t tom);

/**
 * @brief Convert ToM emotion to string
 *
 * WHAT: Get human-readable emotion name
 * WHY:  Logging and debugging
 * HOW:  Lookup table
 *
 * @param emotion Emotion type
 * @return String name (never NULL)
 */
const char* tom_emotion_to_string(tom_emotion_t emotion);

//=============================================================================
// Bio-Async Integration API (Phase 10.6.1 - ToM Integration)
//=============================================================================

/**
 * @brief Agent identifier for multi-agent scenarios
 */
typedef uint32_t agent_id_t;

/**
 * @brief Agent belief update (broadcasts via bio-async when beliefs change)
 *
 * WHAT: Notification that an agent's belief state has been updated
 * WHY:  Enable executive and ethics modules to react to belief changes
 * HOW:  Broadcast via BIO_MSG_AGENT_BELIEF_UPDATE when beliefs are inferred
 */
typedef struct {
    agent_id_t agent_id;              /**< Which agent's belief changed */
    char belief_content[256];         /**< What the agent believes */
    float confidence;                 /**< Confidence in this belief [0.0, 1.0] */
    bool is_false_belief;             /**< Does this contradict reality? */
    uint64_t timestamp_ms;            /**< When belief was updated */
} tom_agent_belief_update_t;

/**
 * @brief Agent intention update (broadcasts via bio-async when intentions change)
 *
 * WHAT: Notification that an agent's planned action has been inferred
 * WHY:  Enable proactive coordination and ethical evaluation
 * HOW:  Broadcast via BIO_MSG_AGENT_INTENTION_INFERRED when intentions are detected
 */
typedef struct {
    agent_id_t agent_id;              /**< Which agent's intention was inferred */
    char action_description[256];     /**< What the agent plans to do */
    float likelihood;                 /**< Probability of executing [0.0, 1.0] */
    tom_emotion_t emotional_state;    /**< Agent's current emotional state */
    char goal_description[256];       /**< Agent's underlying goal */
    uint64_t timestamp_ms;            /**< When intention was inferred */
} tom_agent_intention_update_t;

/**
 * @brief Query agent's perspective for a specific action context
 *
 * WHAT: Request ToM to simulate how an agent would perceive an action
 * WHY:  Enable ethics to consider agent beliefs for harm assessment
 * HOW:  Query ToM with action context, get perspective-based response
 *
 * @param tom ToM handle
 * @param agent_id Which agent to simulate
 * @param action_description Description of the action
 * @param perspective_output Output: agent's perspective (buffer must be at least 512 bytes)
 * @param perceived_harm Output: how much harm agent would perceive [0.0, 1.0]
 * @return true if perspective successfully simulated
 *
 * USAGE: Called by ethics module before making ethical decisions
 * COMPLEXITY: O(1) for inference
 */
bool tom_query_agent_perspective(theory_of_mind_t tom,
                                  agent_id_t agent_id,
                                  const char* action_description,
                                  char* perspective_output,
                                  float* perceived_harm);

/**
 * @brief Query agent's likely response to a proposed action
 *
 * WHAT: Predict how an agent would respond to a specific action
 * WHY:  Enable executive to plan agent-aware decisions
 * HOW:  Use BDI model to simulate agent's reaction
 *
 * @param tom ToM handle
 * @param agent_id Which agent to query
 * @param proposed_action Description of proposed action
 * @param response_output Output: predicted response (buffer must be at least 256 bytes)
 * @param response_likelihood Output: confidence in this response [0.0, 1.0]
 * @return true if response predicted successfully
 *
 * USAGE: Called by executive module before making decisions affecting other agents
 * COMPLEXITY: O(1) for prediction
 */
bool tom_query_agent_response(theory_of_mind_t tom,
                               agent_id_t agent_id,
                               const char* proposed_action,
                               char* response_output,
                               float* response_likelihood);

/**
 * @brief Update ToM with observed outcome of an action
 *
 * WHAT: Inform ToM about actual outcome of an action involving an agent
 * WHY:  Enable ToM to learn and refine agent models
 * HOW:  Update BDI state based on observed response
 *
 * @param tom ToM handle
 * @param agent_id Which agent was involved
 * @param action_taken What action was performed
 * @param actual_outcome What actually happened
 * @param agent_satisfaction Agent's satisfaction level [0.0, 1.0]
 * @return true if model updated successfully
 *
 * USAGE: Called by executive/ethics after action execution
 * COMPLEXITY: O(1) for update
 */
bool tom_update_agent_model(theory_of_mind_t tom,
                             agent_id_t agent_id,
                             const char* action_taken,
                             const char* actual_outcome,
                             float agent_satisfaction);

/**
 * @brief Get agent's current mental state summary
 *
 * WHAT: Retrieve complete BDI + emotion state for an agent
 * WHY:  Provide context for executive and ethics modules
 * HOW:  Copy internal state to output structure
 *
 * @param tom ToM handle
 * @param agent_id Which agent to query
 * @param belief Output: current belief (can be NULL)
 * @param desire Output: current desire (can be NULL)
 * @param intention Output: current intention (can be NULL)
 * @param emotion Output: current emotion (can be NULL)
 * @param emotion_confidence Output: confidence in emotion inference (can be NULL)
 * @return true if state retrieved
 *
 * COMPLEXITY: O(1)
 */
bool tom_get_agent_state(theory_of_mind_t tom,
                         agent_id_t agent_id,
                         tom_belief_t* belief,
                         tom_desire_t* desire,
                         tom_intention_t* intention,
                         tom_emotion_t* emotion,
                         float* emotion_confidence);

/**
 * @brief Get error message for last operation
 *
 * @return Error string (never NULL, may be empty)
 */
const char* tom_get_last_error(void);

//=============================================================================
// Brain Immune System Integration API
//=============================================================================

// Forward declare immune system
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

/**
 * @brief Connect ToM to brain immune system
 *
 * WHAT: Establish bidirectional communication between ToM and immune system
 * WHY:  Inflammation affects social cognition; social stress affects immunity
 * HOW:  Store immune handle, register callbacks for cytokine effects
 *
 * BIOLOGICAL BASIS:
 * - IL-6 and pro-inflammatory cytokines impair mentalizing and perspective-taking
 * - Failed social predictions trigger stress response (cortisol, inflammation)
 * - Sickness behavior reduces social engagement and theory of mind capacity
 *
 * INTEGRATION:
 * - Immune → ToM: High inflammation reduces perspective_score
 * - Immune → ToM: Cytokine storm impairs emotion inference confidence
 * - ToM → Immune: Failed predictions trigger stress cytokine release
 * - ToM → Immune: Social isolation increases inflammatory markers
 *
 * @param tom ToM handle
 * @param immune Immune system handle
 * @return true on success, false on error
 *
 * SIDE EFFECTS:
 * - Registers callbacks with immune system
 * - Enables inflammation monitoring
 *
 * COMPLEXITY: O(1)
 */
bool tom_connect_immune(theory_of_mind_t tom, brain_immune_system_t* immune);

/**
 * @brief Get current social cognition impairment level
 *
 * WHAT: Calculate how much inflammation is impairing ToM capacity
 * WHY:  Model sickness behavior effects on social cognition
 * HOW:  Query immune inflammation level, map to impairment score
 *
 * BIOLOGICAL BASIS:
 * Studies show IL-6 and TNF-α reduce theory of mind performance by
 * affecting prefrontal and temporoparietal junction activity.
 *
 * @param tom ToM handle
 * @return Impairment level [0.0, 1.0], 0 = no impairment, 1 = severe
 *
 * COMPLEXITY: O(1)
 */
float tom_get_immune_impairment(theory_of_mind_t tom);

/**
 * @brief Trigger stress response from social prediction failure
 *
 * WHAT: Signal immune system when social predictions fail
 * WHY:  Social stress triggers inflammatory response
 * HOW:  Release pro-inflammatory cytokines proportional to prediction error
 *
 * BIOLOGICAL BASIS:
 * Social rejection and failed social predictions activate stress pathways,
 * leading to increased cortisol and inflammatory markers (IL-6, CRP).
 *
 * @param tom ToM handle
 * @param prediction_error Magnitude of prediction failure [0.0, 1.0]
 * @param is_social_rejection true if failure was social rejection
 * @return true if stress response triggered
 *
 * SIDE EFFECTS:
 * - Releases IL-1 or IL-6 cytokines via immune system
 * - May initiate localized inflammation
 *
 * COMPLEXITY: O(1)
 */
bool tom_trigger_social_stress(theory_of_mind_t tom, float prediction_error, bool is_social_rejection);

//=============================================================================
// Imagination Engine Integration API
//=============================================================================

/**
 * @brief Connect ToM to imagination engine
 *
 * WHAT: Establish bidirectional communication between ToM and imagination
 * WHY:  Imagination enables mental simulation of other agents' perspectives
 * HOW:  Store imagination handle, register callbacks for social imagination
 *
 * BIOLOGICAL BASIS:
 * - Theory of Mind and imagination share neural substrates in mPFC and TPJ
 * - Perspective-taking recruits the same networks as mental simulation
 * - Default Mode Network active in both ToM and spontaneous imagination
 * - Social imagination: simulating what others think/feel/will do
 *
 * INTEGRATION:
 * - ToM -> Imagination: Request perspective-taking simulations
 * - Imagination -> ToM: Report social simulation results
 * - ToM + Imagination: False belief reasoning via counterfactual simulation
 *
 * @param tom ToM handle
 * @param engine Imagination engine handle
 * @return 0 on success, -1 on error
 *
 * SIDE EFFECTS:
 * - Registers callbacks with imagination engine
 * - Enables social imagination mode
 *
 * COMPLEXITY: O(1)
 */
int tom_connect_imagination(theory_of_mind_t tom, imagination_engine_t* engine);

/**
 * @brief Set callback for receiving imagination results
 *
 * WHAT: Register callback invoked when imagination completes perspective simulation
 * WHY:  Enable async processing of imagination-based perspective-taking
 * HOW:  Store callback and user_data in ToM, invoke on imagination completion
 *
 * @param tom ToM handle
 * @param cb Callback function invoked with imagination results
 * @param user_data Opaque pointer passed to callback
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 */
int tom_set_imagination_callback(theory_of_mind_t tom, imagination_result_callback_t cb, void* user_data);

/**
 * @brief Simulate another agent's perspective using imagination
 *
 * WHAT: Use imagination engine to mentally simulate how an agent perceives the world
 * WHY:  Rich perspective-taking requires full mental simulation, not just BDI tracking
 * HOW:  Create imagination scenario with agent's believed state as initial condition
 *
 * BIOLOGICAL BASIS:
 * - TPJ activation during perspective-taking correlates with simulation accuracy
 * - Mental simulation uses same circuits as self-referential imagination
 * - False belief understanding requires imagining agent's (incorrect) world model
 *
 * @param tom ToM handle
 * @param agent_id ID of the agent whose perspective to simulate
 * @param believed_state What the agent believes about the world (latent representation)
 * @return Imagination scenario with simulated perspective, or NULL on error
 *
 * NOTE: Caller is responsible for managing the returned scenario lifecycle
 *
 * SIDE EFFECTS:
 * - Creates active imagination scenario
 * - May consume imagination workspace capacity
 *
 * COMPLEXITY: O(1) for scenario creation, O(n) for simulation
 */
imagination_scenario_t* tom_simulate_other_perspective(theory_of_mind_t tom, uint64_t agent_id, nimcp_tensor_t* believed_state);

/**
 * @brief Request social imagination based on observation
 *
 * WHAT: Trigger imagination to simulate social context of observation
 * WHY:  Complex social situations benefit from imagination-based simulation
 * HOW:  Submit observation to imagination engine in SOCIAL mode
 *
 * Use cases:
 * - Predicting how an agent will react to an action
 * - Understanding emotional impact of social events
 * - Simulating group dynamics and social interactions
 * - Counterfactual reasoning: "How would they have reacted if..."
 *
 * @param tom ToM handle
 * @param obs Behavioral observation triggering social imagination
 * @return 0 on success (imagination request submitted), -1 on error
 *
 * SIDE EFFECTS:
 * - Submits async imagination request
 * - Result delivered via registered callback (tom_set_imagination_callback)
 *
 * COMPLEXITY: O(1) for request submission
 */
int tom_request_social_imagination(theory_of_mind_t tom, const tom_observation_t* obs);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_THEORY_OF_MIND_H
