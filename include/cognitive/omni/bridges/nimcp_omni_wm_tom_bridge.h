/**
 * @file nimcp_omni_wm_tom_bridge.h
 * @brief World Model Theory of Mind Bridge - Social World Modeling Integration
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Theory of Mind systems
 * WHY:  Enable social world modeling by integrating mental state prediction with physical world prediction
 * HOW:  ToM informs WM about agent mental states; WM provides counterfactual simulations for ToM reasoning
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * BELIEF-DESIRE-INTENTION (BDI) MODEL:
 * ------------------------------------
 * Model others' beliefs, desires, and intentions as state variables in the world model:
 *
 *   s_agent = (belief_state, desire_state, intention_vector, emotional_state)
 *
 * The RSSM can then predict how mental states evolve:
 *   s_agent(t+1) = f(s_agent(t), observation(t), action(t))
 *
 * MENTAL STATE DYNAMICS:
 * ----------------------
 * Use RSSM to predict how beliefs update over time based on:
 * - Observed actions
 * - Inferred perceptions
 * - Social context
 * - Emotional state transitions
 *
 * COUNTERFACTUAL SOCIAL REASONING:
 * --------------------------------
 * "What would agent X do if they believed Y?"
 * The world model enables simulation of alternative belief states to predict
 * behavior divergence from actual trajectory.
 *
 * FALSE BELIEF MODELING:
 * ----------------------
 * Track divergence between reality (WM state) and beliefs (ToM state):
 *   gap = ||reality_state - believed_state||
 * When gap > threshold, agent has false belief.
 *
 * DATA FLOW:
 * ----------
 *   ToM -> WM: Agent belief/desire/intention states for social prediction
 *   WM -> ToM: Predicted future states for belief updating
 *   ToM -> WM: Observed agent actions for trajectory training
 *   WM -> ToM: Counterfactual simulations for false belief reasoning
 *   Mirror -> WM: Action understanding for intention prediction
 *   WM -> Mirror: Predicted actions for empathetic simulation
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - Theory of Mind (nimcp_theory_of_mind.h): BDI model, perspective-taking
 *   - Mirror Neurons (nimcp_mirror_neurons.h): Action understanding
 *   - Social Bridge (nimcp_tom_social_bridge.h): Social cognition
 *   - World Model (nimcp_omni_world_model.h): RSSM, predictions
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E6C
 *   Message Range: 0x6C00-0x6CFF
 */

#ifndef NIMCP_OMNI_WM_TOM_BRIDGE_H
#define NIMCP_OMNI_WM_TOM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_TOM_* message types */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Theory of Mind (from nimcp_theory_of_mind.h) */
typedef struct theory_of_mind_s* theory_of_mind_t;
typedef uint32_t agent_id_t;

/* Mirror Neuron System (from nimcp_mirror_neurons.h) */
typedef struct mirror_neurons_system mirror_neurons_system_t;
typedef mirror_neurons_system_t* mirror_neurons_t;

/* Social Bridge (from nimcp_tom_social_bridge.h) */
typedef struct tom_social_bridge tom_social_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model ToM Bridge */
#define BIO_MODULE_WM_TOM_BRIDGE            0x0E6C

/** Maximum number of agents to track simultaneously */
#define WM_TOM_MAX_AGENTS                   32

/** Maximum belief state dimensionality */
#define WM_TOM_MAX_BELIEF_DIM               256

/** Maximum desire state dimensionality */
#define WM_TOM_MAX_DESIRE_DIM               128

/** Maximum intention/action dimensionality */
#define WM_TOM_MAX_ACTION_DIM               64

/** Maximum trajectory prediction horizon */
#define WM_TOM_MAX_HORIZON                  32

/** Maximum counterfactual trajectories */
#define WM_TOM_MAX_COUNTERFACTUALS          8

/** Default divergence threshold for false belief detection */
#define WM_TOM_DEFAULT_FALSE_BELIEF_THRESHOLD 0.5f

/** State dimension for world model integration */
#define OMNI_WM_STATE_DIM                   256

/** Action dimension for world model integration */
#define OMNI_WM_ACTION_DIM                  64

/* ============================================================================
 * Bio-Async Message Types (0x6C00-0x6CFF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_TOM_MENTAL_STATE_PRED (0x6C00): Mental state prediction request
 *   - BIO_MSG_WM_TOM_TRAJECTORY_PRED (0x6C10): Social trajectory prediction
 *   - BIO_MSG_WM_TOM_COUNTERFACTUAL_REQ (0x6C20): Counterfactual reasoning
 *   - BIO_MSG_WM_TOM_FALSE_BELIEF_DETECT (0x6C30): False belief detection
 *   - BIO_MSG_WM_TOM_SOCIAL_INTERACTION (0x6C40): Social interaction training
 *   - BIO_MSG_WM_TOM_JOINT_PREDICTION (0x6C50): Multi-agent prediction
 *   - BIO_MSG_WM_TOM_EMPATHY_SIMULATION (0x6C60): Empathetic simulation
 * ============================================================================ */

/** @brief Message type alias for ToM bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_tom_msg_type_t;

/* ============================================================================
 * Emotion Type (from Theory of Mind)
 * ============================================================================ */

/**
 * @brief Basic emotional states for Theory of Mind inference
 * @note Mirrors tom_emotion_t from nimcp_theory_of_mind.h
 */
typedef enum {
    WM_TOM_EMOTION_UNKNOWN,      /**< Cannot infer emotion */
    WM_TOM_EMOTION_NEUTRAL,      /**< No strong emotion detected */
    WM_TOM_EMOTION_JOY,          /**< Positive, high arousal */
    WM_TOM_EMOTION_SADNESS,      /**< Negative, low arousal */
    WM_TOM_EMOTION_ANGER,        /**< Negative, high arousal */
    WM_TOM_EMOTION_FEAR,         /**< Negative, high arousal, threat-focused */
    WM_TOM_EMOTION_SURPRISE,     /**< Neutral/positive, high arousal, unexpected */
    WM_TOM_EMOTION_DISGUST,      /**< Negative, moderate arousal, aversion */
    WM_TOM_EMOTION_ANXIETY,      /**< Negative, moderate arousal, anticipatory */
    WM_TOM_EMOTION_CALM,         /**< Positive, low arousal, peaceful */
    WM_TOM_EMOTION_COUNT
} wm_tom_emotion_t;

/* ============================================================================
 * Agent Mental State Structures
 * ============================================================================ */

/**
 * @brief Agent mental state for world model integration
 *
 * WHAT: Complete BDI representation of an agent's mental state
 * WHY:  Enable mental state prediction using RSSM dynamics
 * HOW:  Track beliefs, desires, intentions, and emotions as state vectors
 */
typedef struct {
    agent_id_t agent_id;                            /**< Unique agent identifier */
    float belief_state[OMNI_WM_STATE_DIM];          /**< What agent believes about world */
    float desire_state[OMNI_WM_STATE_DIM];          /**< Agent's goal states */
    float intention_vector[OMNI_WM_ACTION_DIM];     /**< Planned actions */
    wm_tom_emotion_t emotional_state;               /**< Current emotion */
    float emotional_intensity;                      /**< Emotion intensity [0-1] */
    float confidence;                               /**< ToM inference confidence [0-1] */
    uint64_t last_update_us;                        /**< Last update timestamp */
} tom_agent_mental_state_t;

/**
 * @brief Belief-reality gap tracking
 *
 * WHAT: Tracks divergence between agent's beliefs and actual world state
 * WHY:  Enable false belief detection for social reasoning
 * HOW:  Compare agent's belief state with WM ground truth
 */
typedef struct {
    agent_id_t agent_id;                            /**< Which agent */
    float reality_state[OMNI_WM_STATE_DIM];         /**< WM ground truth */
    float believed_state[OMNI_WM_STATE_DIM];        /**< Agent's belief */
    float divergence_score;                         /**< ||reality - belief|| */
    bool has_false_belief;                          /**< Significant divergence detected */
    uint32_t false_belief_dimensions;               /**< Number of dimensions with divergence */
    float max_dimension_divergence;                 /**< Maximum divergence on single dimension */
    uint64_t last_update_us;                        /**< Last update timestamp */
} tom_belief_reality_gap_t;

/**
 * @brief Social trajectory prediction
 *
 * WHAT: Predicted sequence of agent positions, actions, and emotions
 * WHY:  Enable anticipation of social behavior
 * HOW:  Roll out RSSM with mental state as context
 */
typedef struct {
    agent_id_t agent_id;                            /**< Which agent */
    uint32_t horizon_steps;                         /**< Number of prediction steps */
    float predicted_positions[WM_TOM_MAX_HORIZON][3];    /**< x, y, z positions */
    float predicted_actions[WM_TOM_MAX_HORIZON][OMNI_WM_ACTION_DIM]; /**< Action sequence */
    wm_tom_emotion_t predicted_emotions[WM_TOM_MAX_HORIZON]; /**< Emotion sequence */
    float trajectory_confidence;                    /**< Overall trajectory confidence */
    float step_confidences[WM_TOM_MAX_HORIZON];     /**< Per-step confidence */
    uint64_t prediction_timestamp_us;               /**< When prediction was made */
} tom_social_trajectory_t;

/**
 * @brief Social interaction record for training
 *
 * WHAT: Observed social interaction between agents
 * WHY:  Train world model on social dynamics
 * HOW:  Record interaction context, actions, and outcomes
 */
typedef struct {
    agent_id_t initiator_id;                        /**< Interaction initiator */
    agent_id_t responder_id;                        /**< Interaction responder */
    float initiator_state[OMNI_WM_STATE_DIM];       /**< Initiator mental state */
    float responder_state[OMNI_WM_STATE_DIM];       /**< Responder mental state */
    float initiator_action[OMNI_WM_ACTION_DIM];     /**< Initiator's action */
    float responder_action[OMNI_WM_ACTION_DIM];     /**< Responder's action */
    float outcome_state[OMNI_WM_STATE_DIM];         /**< Resulting state */
    float reward;                                   /**< Interaction reward/outcome */
    bool is_cooperative;                            /**< Cooperative interaction */
    bool is_competitive;                            /**< Competitive interaction */
    uint64_t timestamp_us;                          /**< Interaction timestamp */
} tom_social_interaction_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to Theory of Mind
 *
 * WHAT: WM predictions and simulations flowing to ToM
 * WHY:  Provide predicted states and counterfactual results for mental reasoning
 * HOW:  State predictions, counterfactual trajectories, action outcomes
 */
typedef struct {
    /* Predicted future states for belief updating */
    float predicted_world_state[OMNI_WM_STATE_DIM]; /**< Predicted world state */
    uint32_t prediction_horizon;                    /**< Horizon in steps */
    float prediction_confidence;                    /**< Confidence in prediction */

    /* Counterfactual results */
    uint32_t counterfactual_count;                  /**< Number of CF results */
    tom_social_trajectory_t* counterfactual_trajectories; /**< CF trajectories (allocated) */

    /* Action consequence predictions */
    float action_outcome_confidence;                /**< Confidence in action outcomes */
    float predicted_reward;                         /**< Predicted reward */
    float predicted_emotional_impact;               /**< Predicted emotional change */

    /* Perspective-taking results */
    float empathy_simulation_result[OMNI_WM_STATE_DIM]; /**< Simulated other's perspective */
    float perspective_confidence;                   /**< Perspective-taking confidence */
} omni_wm_to_tom_effects_t;

/**
 * @brief Effects from Theory of Mind to World Model
 *
 * WHAT: ToM-derived information flowing to world model
 * WHY:  Provide mental state context for social predictions
 * HOW:  Multi-agent states, belief gaps, social context
 */
typedef struct {
    /* Multi-agent mental states */
    uint32_t agent_count;                           /**< Number of tracked agents */
    tom_agent_mental_state_t* agent_states;         /**< Agent mental states (allocated) */

    /* Belief-reality gaps for training */
    uint32_t gap_count;                             /**< Number of tracked gaps */
    tom_belief_reality_gap_t* belief_gaps;          /**< Belief-reality gaps (allocated) */

    /* Social context for prediction */
    float social_attention_weights[WM_TOM_MAX_AGENTS]; /**< Attention to each agent */
    bool is_cooperative_context;                    /**< Cooperative social context */
    bool is_competitive_context;                    /**< Competitive social context */
    float social_density;                           /**< Social environment density */

    /* Observed social interactions for training */
    uint32_t interaction_count;                     /**< Number of recent interactions */
    tom_social_interaction_t* interactions;         /**< Recent interactions (allocated) */

    /* Mirror neuron integration */
    bool mirror_action_observed;                    /**< Action observed via mirror system */
    float observed_action[OMNI_WM_ACTION_DIM];      /**< Observed action vector */
    float action_understanding_confidence;          /**< Mirror neuron confidence */
    agent_id_t observed_action_agent;               /**< Agent performing observed action */
} tom_to_omni_wm_effects_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model ToM Bridge configuration
 *
 * WHAT: Parameters controlling WM-ToM integration
 * WHY:  Tune mental state prediction, false belief detection, and training
 * HOW:  Configurable thresholds, horizons, and feature flags
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;                         /**< Enable bidirectional modulation */
    float sensitivity;                              /**< General sensitivity [0.5-2.0] */

    /* Mental State Prediction Settings */
    bool enable_mental_state_prediction;            /**< Predict mental state evolution */
    uint32_t default_prediction_horizon;            /**< Default horizon in steps */
    float mental_state_learning_rate;               /**< Learning rate for mental dynamics */
    float belief_update_rate;                       /**< Rate of belief updates [0-1] */

    /* False Belief Detection Settings */
    bool enable_false_belief_detection;             /**< Detect false beliefs */
    float false_belief_threshold;                   /**< Divergence threshold */
    float false_belief_persistence;                 /**< How long false belief persists [0-1] */

    /* Counterfactual Settings */
    bool enable_counterfactual_reasoning;           /**< Enable "what if" queries */
    uint32_t max_counterfactuals;                   /**< Max CF trajectories to compute */
    float counterfactual_noise;                     /**< Exploration noise in CF */

    /* Social Training Settings */
    bool enable_social_training;                    /**< Train from social interactions */
    float social_training_learning_rate;            /**< Learning rate for social dynamics */
    uint32_t interaction_buffer_size;               /**< Size of interaction buffer */
    float interaction_priority_decay;               /**< Priority decay for old interactions */

    /* Multi-Agent Settings */
    uint32_t max_tracked_agents;                    /**< Max agents to track */
    float agent_attention_decay;                    /**< Decay rate for agent attention */
    bool enable_joint_prediction;                   /**< Enable multi-agent prediction */

    /* Mirror Neuron Integration Settings */
    bool enable_mirror_integration;                 /**< Integrate with mirror neurons */
    float mirror_action_threshold;                  /**< Min confidence for mirror actions */

    /* Empathy Settings */
    bool enable_empathy_simulation;                 /**< Enable perspective-taking */
    float empathy_strength;                         /**< Empathy modulation strength */

    /* Bio-async Settings */
    bool enable_bio_async;                          /**< Enable bio-async messaging */
} omni_wm_tom_bridge_config_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model ToM Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and error metrics
 */
typedef struct {
    /* Mental State Prediction Statistics */
    uint64_t mental_state_predictions;              /**< Total mental state predictions */
    uint64_t successful_predictions;                /**< Predictions within error bound */
    float mean_prediction_error;                    /**< Average prediction error */
    float mean_prediction_confidence;               /**< Average prediction confidence */

    /* False Belief Statistics */
    uint64_t false_beliefs_detected;                /**< Total false beliefs detected */
    uint64_t belief_reality_gaps_tracked;           /**< Total gaps tracked */
    float mean_divergence_score;                    /**< Average belief-reality divergence */

    /* Trajectory Prediction Statistics */
    uint64_t trajectory_predictions;                /**< Total trajectory predictions */
    float mean_trajectory_error;                    /**< Average trajectory prediction error */
    float mean_trajectory_confidence;               /**< Average trajectory confidence */

    /* Counterfactual Statistics */
    uint64_t counterfactual_queries;                /**< Total counterfactual queries */
    float mean_counterfactual_divergence;           /**< Average CF divergence from actual */

    /* Social Training Statistics */
    uint64_t interactions_processed;                /**< Total interactions processed */
    uint64_t training_updates;                      /**< Training updates from interactions */
    float mean_training_loss;                       /**< Average training loss */

    /* Multi-Agent Statistics */
    uint32_t current_tracked_agents;                /**< Currently tracked agents */
    uint64_t joint_predictions;                     /**< Multi-agent joint predictions */

    /* Empathy Statistics */
    uint64_t empathy_simulations;                   /**< Total empathy simulations */
    float mean_empathy_accuracy;                    /**< Empathy simulation accuracy */

    /* Mirror Integration Statistics */
    uint64_t mirror_actions_received;               /**< Actions from mirror neurons */
    float mean_mirror_confidence;                   /**< Average mirror action confidence */

    /* Timing Statistics */
    uint64_t total_updates;                         /**< Total update cycles */
    double total_processing_time_ms;                /**< Total processing time */
    double mean_update_time_ms;                     /**< Average update duration */
    uint64_t last_update_time_us;                   /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                          /**< Total errors encountered */
    uint64_t errors_prediction;                     /**< Prediction-related errors */
    uint64_t errors_training;                       /**< Training-related errors */
    uint64_t errors_counterfactual;                 /**< Counterfactual-related errors */
} omni_wm_tom_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Theory of Mind Bridge
 *
 * WHAT: Main bridge structure connecting WM with ToM systems
 * WHY:  Orchestrates bidirectional information flow for social world modeling
 * HOW:  Maintains connections, effects, mental state tracking, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_tom_bridge {
    bridge_base_t base;                             /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_tom_bridge_config_t config;             /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;                /**< World model (RSSM) */
    theory_of_mind_t tom;                           /**< Theory of Mind system */
    mirror_neurons_t mirror;                        /**< Mirror neuron system */
    tom_social_bridge_t* social_bridge;             /**< Social cognition bridge */

    /* Bidirectional Effects */
    omni_wm_to_tom_effects_t wm_to_tom;             /**< Effects: WM -> ToM */
    tom_to_omni_wm_effects_t tom_to_wm;             /**< Effects: ToM -> WM */

    /* Agent Tracking */
    tom_agent_mental_state_t* tracked_agents;       /**< Array of tracked agent states */
    uint32_t tracked_agent_count;                   /**< Current number of tracked agents */
    uint32_t tracked_agent_capacity;                /**< Capacity of tracked agents array */

    /* Belief-Reality Gap Tracking */
    tom_belief_reality_gap_t* belief_gaps;          /**< Array of belief-reality gaps */
    uint32_t belief_gap_count;                      /**< Current number of gaps */
    uint32_t belief_gap_capacity;                   /**< Capacity of gaps array */

    /* Interaction Buffer */
    tom_social_interaction_t* interaction_buffer;   /**< Buffer for recent interactions */
    uint32_t interaction_buffer_head;               /**< Buffer head index */
    uint32_t interaction_buffer_tail;               /**< Buffer tail index */
    uint32_t interaction_buffer_capacity;           /**< Buffer capacity */

    /* Counterfactual Cache */
    tom_social_trajectory_t* cf_cache;              /**< Counterfactual trajectory cache */
    uint32_t cf_cache_size;                         /**< Current cache size */
    uint32_t cf_cache_capacity;                     /**< Cache capacity */

    /* Internal State */
    float current_social_context;                   /**< Current social context [-1, 1] */
    bool cooperative_mode;                          /**< In cooperative social mode */
    bool competitive_mode;                          /**< In competitive social mode */
    agent_id_t focus_agent;                         /**< Currently focused agent */

    /* Statistics */
    omni_wm_tom_bridge_stats_t stats;               /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;              /**< Per-instance health agent */
} omni_wm_tom_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_default_config(
    omni_wm_tom_bridge_config_t* config);

/**
 * @brief Create World Model ToM Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_tom_bridge_t* omni_wm_tom_bridge_create(
    const omni_wm_tom_bridge_config_t* config);

/**
 * @brief Destroy World Model ToM Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_tom_bridge_destroy(omni_wm_tom_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_reset(omni_wm_tom_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all systems to bridge
 *
 * WHAT: Establish connections to WM, ToM, mirror, and social systems
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param tom Theory of Mind system - required
 * @param mirror Mirror neuron system - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_connect(
    omni_wm_tom_bridge_t* bridge,
    omni_world_model_t* world_model,
    theory_of_mind_t tom,
    mirror_neurons_t mirror);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_connect_world_model(
    omni_wm_tom_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect Theory of Mind
 *
 * @param bridge Bridge instance
 * @param tom Theory of Mind to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_connect_tom(
    omni_wm_tom_bridge_t* bridge,
    theory_of_mind_t tom);

/**
 * @brief Connect mirror neuron system
 *
 * @param bridge Bridge instance
 * @param mirror Mirror neuron system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_connect_mirror(
    omni_wm_tom_bridge_t* bridge,
    mirror_neurons_t mirror);

/**
 * @brief Connect social cognition bridge
 *
 * @param bridge Bridge instance
 * @param social_bridge Social bridge to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_connect_social(
    omni_wm_tom_bridge_t* bridge,
    tom_social_bridge_t* social_bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model and ToM connected (minimum requirement)
 */
bool omni_wm_tom_bridge_is_connected(const omni_wm_tom_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and ToM systems
 * HOW:  Gather ToM effects, compute WM effects, update mental states
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_update(
    omni_wm_tom_bridge_t* bridge,
    float dt);

/* ============================================================================
 * Mental State Prediction API
 * ============================================================================ */

/**
 * @brief Predict how an agent's mental state will evolve
 *
 * WHAT: Use RSSM to model belief/desire/intention dynamics
 * WHY:  Anticipate changes in others' mental states
 * HOW:  Roll out mental state through world model dynamics
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to predict
 * @param horizon_steps Number of steps to predict
 * @param out_predicted_state Output: predicted mental state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_predict_mental_state(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    uint32_t horizon_steps,
    tom_agent_mental_state_t* out_predicted_state);

/**
 * @brief Update mental state for an agent
 *
 * WHAT: Update tracked mental state based on observation
 * WHY:  Keep mental state models current
 * HOW:  Integrate new observation with existing state
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to update
 * @param observed_state Observed mental state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_update_mental_state(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const tom_agent_mental_state_t* observed_state);

/* ============================================================================
 * Social Trajectory Prediction API
 * ============================================================================ */

/**
 * @brief Predict agent behavior trajectory
 *
 * WHAT: Predict sequence of positions, actions, and emotions
 * WHY:  Enable anticipation of social behavior
 * HOW:  Roll out RSSM with mental state context
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to predict
 * @param horizon_steps Number of steps to predict
 * @param out_trajectory Output: predicted trajectory
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_predict_social_trajectory(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    uint32_t horizon_steps,
    tom_social_trajectory_t* out_trajectory);

/**
 * @brief Predict joint trajectory for multiple agents
 *
 * WHAT: Predict coordinated behavior of multiple agents
 * WHY:  Model group dynamics and interactions
 * HOW:  Joint rollout with inter-agent influence
 *
 * @param bridge Bridge instance
 * @param agent_ids Array of agent IDs
 * @param agent_count Number of agents
 * @param horizon_steps Prediction horizon
 * @param out_trajectories Output: array of trajectories (pre-allocated)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_predict_joint_trajectory(
    omni_wm_tom_bridge_t* bridge,
    const agent_id_t* agent_ids,
    uint32_t agent_count,
    uint32_t horizon_steps,
    tom_social_trajectory_t* out_trajectories);

/* ============================================================================
 * Counterfactual Reasoning API
 * ============================================================================ */

/**
 * @brief Simulate "What if agent believed X?"
 *
 * WHAT: Counterfactual reasoning about belief states
 * WHY:  Enable reasoning about alternative mental states
 * HOW:  Replace agent's belief, roll out trajectory
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to simulate
 * @param hypothetical_belief Hypothetical belief state
 * @param out_trajectory Output: predicted trajectory under hypothesis
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_counterfactual_belief(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const float* hypothetical_belief,
    tom_social_trajectory_t* out_trajectory);

/**
 * @brief Simulate "What if agent took action X?"
 *
 * WHAT: Counterfactual reasoning about actions
 * WHY:  Enable reasoning about alternative actions
 * HOW:  Replace agent's action, simulate consequences
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to simulate
 * @param hypothetical_action Hypothetical action
 * @param action_dim Action dimensionality
 * @param out_trajectory Output: predicted trajectory
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_counterfactual_action(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const float* hypothetical_action,
    uint32_t action_dim,
    tom_social_trajectory_t* out_trajectory);

/* ============================================================================
 * False Belief Detection API
 * ============================================================================ */

/**
 * @brief Detect false beliefs across tracked agents
 *
 * WHAT: Identify agents with beliefs that diverge from reality
 * WHY:  Enable sophisticated social reasoning
 * HOW:  Compare belief states with WM ground truth
 *
 * @param bridge Bridge instance
 * @param out_gaps Output: array of belief-reality gaps (pre-allocated)
 * @param out_gap_count Output: number of gaps detected
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_detect_false_beliefs(
    omni_wm_tom_bridge_t* bridge,
    tom_belief_reality_gap_t* out_gaps,
    uint32_t* out_gap_count);

/**
 * @brief Get belief-reality gap for specific agent
 *
 * WHAT: Check if specific agent has false belief
 * WHY:  Query false belief status for decision-making
 * HOW:  Compare agent's belief with current WM state
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to check
 * @param out_gap Output: belief-reality gap
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_get_belief_gap(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    tom_belief_reality_gap_t* out_gap);

/* ============================================================================
 * Social Training API
 * ============================================================================ */

/**
 * @brief Train from observed social interaction
 *
 * WHAT: Update world model from interaction observation
 * WHY:  Learn social dynamics from experience
 * HOW:  Extract transition, compute gradients, update weights
 *
 * @param bridge Bridge instance
 * @param interaction Observed social interaction
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_train_from_interaction(
    omni_wm_tom_bridge_t* bridge,
    const tom_social_interaction_t* interaction);

/**
 * @brief Process batch of interactions for training
 *
 * WHAT: Batch training from multiple interactions
 * WHY:  More efficient learning from interaction buffer
 * HOW:  Sample from buffer, compute batch gradient
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_train_batch(omni_wm_tom_bridge_t* bridge);

/* ============================================================================
 * Mirror Neuron Integration API
 * ============================================================================ */

/**
 * @brief Process observed action from mirror neuron system
 *
 * WHAT: Integrate action understanding into mental state prediction
 * WHY:  Mirror neurons provide intention signals
 * HOW:  Update agent's intention state based on observed action
 *
 * @param bridge Bridge instance
 * @param agent_id Agent performing action
 * @param action Observed action vector
 * @param action_dim Action dimensionality
 * @param confidence Mirror neuron confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_on_mirror_action(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const float* action,
    uint32_t action_dim,
    float confidence);

/* ============================================================================
 * Empathy Simulation API
 * ============================================================================ */

/**
 * @brief Simulate being in another agent's situation
 *
 * WHAT: Perspective-taking through world model simulation
 * WHY:  Enable empathetic understanding
 * HOW:  Set WM state to agent's believed state, simulate
 *
 * @param bridge Bridge instance
 * @param agent_id Agent whose perspective to take
 * @param out_simulated_state Output: simulated perspective state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_empathy_simulation(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    float* out_simulated_state);

/* ============================================================================
 * Agent Tracking API
 * ============================================================================ */

/**
 * @brief Start tracking an agent
 *
 * WHAT: Begin tracking mental state for an agent
 * WHY:  Necessary before prediction or training
 * HOW:  Allocate slot, initialize state
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to track
 * @param initial_state Initial mental state (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_track_agent(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    const tom_agent_mental_state_t* initial_state);

/**
 * @brief Stop tracking an agent
 *
 * WHAT: Stop tracking mental state for an agent
 * WHY:  Free resources when agent leaves context
 * HOW:  Remove from tracking, clear state
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to stop tracking
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_untrack_agent(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id);

/**
 * @brief Get mental state for tracked agent
 *
 * WHAT: Retrieve current mental state estimate
 * WHY:  Query agent state for decision-making
 * HOW:  Copy from tracking buffer
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to query
 * @param out_state Output: current mental state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_get_agent_state(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id,
    tom_agent_mental_state_t* out_state);

/**
 * @brief Set focus agent for primary attention
 *
 * WHAT: Set which agent receives most prediction resources
 * WHY:  Attention-based resource allocation
 * HOW:  Update focus agent ID
 *
 * @param bridge Bridge instance
 * @param agent_id Agent to focus on
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_set_focus_agent(
    omni_wm_tom_bridge_t* bridge,
    agent_id_t agent_id);

/* ============================================================================
 * Social Context API
 * ============================================================================ */

/**
 * @brief Set cooperative social context
 *
 * WHAT: Signal that current context is cooperative
 * WHY:  Modulates prediction and training
 * HOW:  Update context flag
 *
 * @param bridge Bridge instance
 * @param is_cooperative true for cooperative context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_set_cooperative_context(
    omni_wm_tom_bridge_t* bridge,
    bool is_cooperative);

/**
 * @brief Set competitive social context
 *
 * WHAT: Signal that current context is competitive
 * WHY:  Modulates prediction and training
 * HOW:  Update context flag
 *
 * @param bridge Bridge instance
 * @param is_competitive true for competitive context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_set_competitive_context(
    omni_wm_tom_bridge_t* bridge,
    bool is_competitive);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to ToM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_tom_effects_t* omni_wm_tom_bridge_get_wm_effects(
    const omni_wm_tom_bridge_t* bridge);

/**
 * @brief Get current effects from ToM to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const tom_to_omni_wm_effects_t* omni_wm_tom_bridge_get_tom_effects(
    const omni_wm_tom_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_get_stats(
    const omni_wm_tom_bridge_t* bridge,
    omni_wm_tom_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_reset_stats(
    omni_wm_tom_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_connect_bio_async(
    omni_wm_tom_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_tom_bridge_disconnect_bio_async(
    omni_wm_tom_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_tom_bridge_is_bio_async_connected(
    const omni_wm_tom_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_tom_msg_type_to_string(omni_wm_tom_msg_type_t msg_type);

/**
 * @brief Get emotion name string
 *
 * @param emotion Emotion type
 * @return Human-readable emotion name
 */
const char* omni_wm_tom_emotion_to_string(wm_tom_emotion_t emotion);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_tom_bridge_validate_config(
    const omni_wm_tom_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_TOM_BRIDGE_H */
