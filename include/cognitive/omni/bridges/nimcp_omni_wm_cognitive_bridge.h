/**
 * @file nimcp_omni_wm_cognitive_bridge.h
 * @brief World Model Cognitive Bridge - Integration with Executive, Attention, Working Memory,
 *        Salience, and Meta-Learning Systems
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with cognitive layer systems
 * WHY:  Enable prediction-informed planning and goal-conditioned world modeling
 * HOW:  World model predictions guide executive planning; cognitive goals condition predictions
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE PROCESSING & EXECUTIVE FUNCTION (Friston, 2010):
 * -----------------------------------------------------------
 * The prefrontal cortex uses world model predictions for planning and decision-making:
 *
 *   World Model -> State Predictions -> Executive Planning -> Goal-Directed Action
 *   Executive Goals -> Conditional Predictions -> World Model -> Refined Predictions
 *
 * ATTENTION-PREDICTION INTERACTION (Feldman & Friston, 2010):
 * ----------------------------------------------------------
 * Attention modulates which predictions are prioritized and updated:
 *
 *   Attention Focus -> Selective Prediction -> Reduced Uncertainty on Attended Items
 *   Prediction Errors -> Salience Signals -> Attention Reallocation
 *
 * DATA FLOW:
 * ----------
 *   WM -> Cognitive: State predictions for planning and decision-making
 *   Cognitive -> WM: Goals and intentions for conditional prediction
 *   WM -> Executive: Action consequences and expected outcomes
 *   Attention -> WM: Focus signals for selective prediction
 *   Salience -> WM: Priority signals for prediction resource allocation
 *   WM -> Meta-Learning: Prediction errors for learning rate adaptation
 *   Working Memory -> WM: Active representations for state context
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - Executive Function (nimcp_executive.h): Task planning, inhibition, goal decomposition
 *   - Attention (nimcp_attention.h): Focus signals, selective processing
 *   - Working Memory (nimcp_working_memory.h): Active representations, capacity limits
 *   - Salience (nimcp_salience.h): Novelty, surprise, urgency signals
 *   - Meta-Learning (nimcp_meta_learning.h): Learning rate adaptation, task similarity
 *   - World Model (nimcp_omni_world_model.h): RSSM, predictions, counterfactuals
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E65
 *   Message Range: 0x6500-0x65FF
 */

#ifndef NIMCP_OMNI_WM_COGNITIVE_BRIDGE_H
#define NIMCP_OMNI_WM_COGNITIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"
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

/* Executive Function (from nimcp_executive.h) */
typedef struct executive_controller executive_controller_t;

/* Working Memory (from nimcp_working_memory.h) */
typedef struct working_memory working_memory_t;

/* Salience Evaluator (from nimcp_salience.h) */
typedef struct salience_evaluator_struct* salience_evaluator_t;

/* Meta-Learner (from nimcp_meta_learning.h) */
typedef struct meta_learner_s* meta_learner_t;

/* Attention System - forward declare to avoid circular deps */
typedef struct attention_system attention_system_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Cognitive Bridge */
#define BIO_MODULE_WM_COGNITIVE_BRIDGE         0x0E65

/** Maximum goals that can be tracked simultaneously */
#define WM_COGNITIVE_MAX_GOALS                 16

/** Maximum action sequence length for planning */
#define WM_COGNITIVE_MAX_ACTION_SEQUENCE       32

/** Maximum working memory items to consider */
#define WM_COGNITIVE_MAX_WM_ITEMS              16

/** Default prediction horizon for planning */
#define WM_COGNITIVE_DEFAULT_HORIZON           10

/** Default attention focus decay rate */
#define WM_COGNITIVE_DEFAULT_FOCUS_DECAY       0.1f

/* ============================================================================
 * Bio-Async Message Types (0x6500-0x65FF)
 *
 * NOTE: Message types are defined in async/nimcp_bio_messages.h to avoid
 * duplication and ensure consistent message IDs across the system.
 *
 * Key message types from nimcp_bio_messages.h:
 *   BIO_MSG_WM_COGNITIVE_STATE_PRED          = 0x6500
 *   BIO_MSG_WM_COGNITIVE_GOAL_UPDATE         = 0x6501
 *   BIO_MSG_WM_COGNITIVE_ACTION_CONSEQUENCE  = 0x6502
 *   BIO_MSG_WM_ATTENTION_FOCUS               = 0x6510
 *   BIO_MSG_WM_COGNITIVE_WORKING_MEM         = 0x6511
 *   BIO_MSG_WM_COGNITIVE_SALIENCE            = 0x6512
 *   BIO_MSG_WM_COGNITIVE_META_LEARNING       = 0x6513
 * ============================================================================ */

/** Message types are uint32_t values from the bio_msg enum in nimcp_bio_messages.h */
typedef uint32_t omni_wm_cognitive_msg_type_t;

/* ============================================================================
 * Goal and Intention Structures
 * ============================================================================ */

/**
 * @brief Goal state representation for world model conditioning
 *
 * WHAT: Target state the executive is trying to achieve
 * WHY:  World model can generate goal-conditioned predictions
 * HOW:  Represented as target state vector with priority and deadline
 */
typedef struct {
    uint32_t goal_id;                           /**< Unique goal identifier */
    float target_state[64];                     /**< Target state vector */
    uint32_t state_dim;                         /**< Dimensionality of target state */
    float priority;                             /**< Goal priority [0-1] */
    float progress;                             /**< Progress toward goal [0-1] */
    uint64_t deadline_us;                       /**< Deadline timestamp (0=no deadline) */
    uint64_t created_us;                        /**< Creation timestamp */
    bool is_active;                             /**< Currently being pursued */
    char description[64];                       /**< Human-readable description */
} wm_cognitive_goal_t;

/**
 * @brief Intention vector for action planning
 *
 * WHAT: Current action intentions from executive
 * WHY:  World model uses intentions to predict action outcomes
 * HOW:  Action sequence with confidence and horizon
 */
typedef struct {
    float action_sequence[WM_COGNITIVE_MAX_ACTION_SEQUENCE][16]; /**< Planned actions */
    uint32_t action_dim;                        /**< Action dimensionality */
    uint32_t sequence_length;                   /**< Number of planned actions */
    float confidence;                           /**< Confidence in intention [0-1] */
    uint32_t goal_id;                           /**< Associated goal */
    uint64_t timestamp_us;                      /**< When intention was formed */
} wm_cognitive_intention_t;

/* ============================================================================
 * Attention and Focus Structures
 * ============================================================================ */

/**
 * @brief Attention focus signal for selective prediction
 *
 * WHAT: Where attention is currently focused
 * WHY:  World model prioritizes predictions for attended items
 * HOW:  Focus location with strength and decay
 */
typedef struct {
    float focus_location[64];                   /**< Focus target state space location */
    uint32_t focus_dim;                         /**< Dimensionality of focus */
    float focus_strength;                       /**< Attention strength [0-1] */
    float focus_bandwidth;                      /**< Spatial spread of attention */
    uint64_t focus_start_us;                    /**< When focus started */
    float decay_rate;                           /**< Focus strength decay rate */
} wm_cognitive_focus_t;

/**
 * @brief Salience signals for prediction prioritization
 *
 * WHAT: Novelty, surprise, and urgency from salience system
 * WHY:  High salience items get prediction priority
 * HOW:  Aggregate salience metrics with weights
 */
typedef struct {
    float novelty;                              /**< Novelty score [0-1] */
    float surprise;                             /**< Prediction error surprise [0-1] */
    float urgency;                              /**< Urgency for immediate attention [0-1] */
    float combined_salience;                    /**< Weighted combination [0-1] */
    uint64_t timestamp_us;                      /**< When salience was computed */
} wm_cognitive_salience_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Cognitive Bridge configuration
 *
 * WHAT: Parameters controlling WM-Cognitive integration
 * WHY:  Tune goal conditioning, attention effects, and learning modulation
 * HOW:  Configurable weights, thresholds, and enable flags
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;                     /**< Enable bidirectional modulation */
    float sensitivity;                          /**< General sensitivity [0.5-2.0] */

    /* Goal Conditioning Settings */
    bool enable_goal_conditioning;              /**< Use goals to condition predictions */
    uint32_t max_active_goals;                  /**< Maximum simultaneous goals */
    float goal_progress_threshold;              /**< Progress threshold for completion */
    float goal_priority_decay;                  /**< Priority decay rate [0-1] */

    /* Attention Integration Settings */
    bool enable_attention_modulation;           /**< Attention affects prediction priority */
    float attention_prediction_boost;           /**< Boost for attended items [1.0-3.0] */
    float attention_bandwidth_min;              /**< Minimum attention bandwidth */
    float attention_bandwidth_max;              /**< Maximum attention bandwidth */
    float focus_decay_rate;                     /**< Default focus decay rate */

    /* Executive Integration Settings */
    bool enable_executive_integration;          /**< Connect to executive function */
    uint32_t action_consequence_horizon;        /**< Steps ahead for consequence pred */
    float plan_evaluation_threshold;            /**< Min confidence for plan execution */
    bool enable_inhibition_check;               /**< Check inhibition via WM predictions */

    /* Salience Integration Settings */
    bool enable_salience_integration;           /**< Use salience for prioritization */
    float novelty_weight;                       /**< Weight for novelty [0-1] */
    float surprise_weight;                      /**< Weight for surprise [0-1] */
    float urgency_weight;                       /**< Weight for urgency [0-1] */
    float salience_threshold;                   /**< Min salience for priority boost */

    /* Working Memory Integration Settings */
    bool enable_working_memory_context;         /**< Use WM for prediction context */
    uint32_t max_wm_context_items;              /**< Max WM items for context */
    float wm_context_decay;                     /**< Context contribution decay */

    /* Meta-Learning Integration Settings */
    bool enable_meta_learning;                  /**< Modulate learning via meta-learner */
    float meta_lr_scale;                        /**< Learning rate scaling factor */
    float adaptation_threshold;                 /**< PE threshold for adaptation trigger */

    /* Bio-async Settings */
    bool enable_bio_async;                      /**< Enable bio-async messaging */
} omni_wm_cognitive_bridge_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to Cognitive Systems
 *
 * WHAT: WM predictions and states flowing to cognitive systems
 * WHY:  Provide predictions for planning, attention allocation, and learning
 * HOW:  State predictions, action consequences, uncertainty estimates
 */
typedef struct {
    /* State Predictions for Planning */
    float* predicted_state;                     /**< Predicted next state */
    uint32_t predicted_state_dim;               /**< State dimensionality */
    float prediction_confidence;                /**< Confidence in prediction [0-1] */
    float prediction_uncertainty;               /**< Uncertainty estimate */
    uint32_t prediction_horizon;                /**< Steps predicted ahead */

    /* Action Consequence Predictions */
    float** action_consequences;                /**< Consequences per action [num_actions x state_dim] */
    float* action_values;                       /**< Expected value per action */
    uint32_t num_actions;                       /**< Number of actions evaluated */
    uint32_t consequence_state_dim;             /**< State dim for consequences */

    /* Goal Progress Predictions */
    float* goal_progress_predictions;           /**< Predicted progress per goal [num_goals] */
    float* goal_achievement_probs;              /**< Achievement probability per goal */
    uint32_t num_goals_tracked;                 /**< Number of goals being tracked */

    /* Attention Guidance */
    float* prediction_error_map;                /**< PE map for attention guidance */
    uint32_t pe_map_dim;                        /**< Dimensionality of PE map */
    float max_prediction_error;                 /**< Maximum PE location */
    float avg_prediction_error;                 /**< Average PE across state */

    /* Meta-Learning Feedback */
    float recommended_lr;                       /**< Recommended learning rate */
    float task_difficulty;                      /**< Estimated task difficulty [0-1] */
    float transfer_potential;                   /**< Potential for transfer learning */
} omni_wm_to_cognitive_effects_t;

/**
 * @brief Effects from Cognitive Systems to World Model
 *
 * WHAT: Cognitive information flowing to world model
 * WHY:  Provide goals, attention focus, and learning signals
 * HOW:  Goal states, attention focus, salience, WM context
 */
typedef struct {
    /* Goal Information */
    wm_cognitive_goal_t* active_goals;          /**< Currently active goals */
    uint32_t num_active_goals;                  /**< Number of active goals */
    wm_cognitive_intention_t current_intention; /**< Current action intention */

    /* Attention Focus */
    wm_cognitive_focus_t attention_focus;       /**< Current attention focus */
    bool attention_active;                      /**< Whether attention is focused */
    float attention_bandwidth;                  /**< Current attention bandwidth */

    /* Salience Signals */
    wm_cognitive_salience_t salience;           /**< Current salience signals */
    bool high_salience_event;                   /**< Whether high salience detected */

    /* Working Memory Context */
    float* wm_context;                          /**< Aggregated WM context */
    uint32_t wm_context_dim;                    /**< Context dimensionality */
    uint32_t wm_item_count;                     /**< Number of WM items used */
    float wm_utilization;                       /**< WM utilization [0-1] */

    /* Executive State */
    float cognitive_load;                       /**< Current cognitive load [0-1] */
    uint32_t active_task_count;                 /**< Number of active tasks */
    bool inhibition_active;                     /**< Inhibition currently active */

    /* Meta-Learning State */
    float current_learning_rate;                /**< Current effective learning rate */
    float adaptation_progress;                  /**< Task adaptation progress */
} cognitive_to_omni_wm_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Cognitive Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and quality metrics
 */
typedef struct {
    /* Prediction Statistics */
    uint64_t state_predictions;                 /**< Total state predictions */
    uint64_t trajectory_predictions;            /**< Total trajectory predictions */
    uint64_t action_consequence_preds;          /**< Action consequence predictions */
    float mean_prediction_confidence;           /**< Average prediction confidence */
    float mean_prediction_error;                /**< Average prediction error */

    /* Goal Statistics */
    uint64_t goals_received;                    /**< Total goals received */
    uint64_t goals_achieved;                    /**< Goals successfully achieved */
    uint64_t goals_failed;                      /**< Goals that failed */
    float mean_goal_progress;                   /**< Average goal progress */

    /* Attention Statistics */
    uint64_t attention_focus_events;            /**< Attention focus events */
    uint64_t attention_shifts;                  /**< Attention shift events */
    float mean_focus_strength;                  /**< Average focus strength */
    float mean_focus_duration_ms;               /**< Average focus duration */

    /* Salience Statistics */
    uint64_t high_novelty_events;               /**< High novelty detections */
    uint64_t high_surprise_events;              /**< High surprise detections */
    uint64_t high_urgency_events;               /**< High urgency detections */
    float mean_salience;                        /**< Average combined salience */

    /* Executive Statistics */
    uint64_t plan_evaluations;                  /**< Plan evaluations performed */
    uint64_t inhibition_checks;                 /**< Inhibition checks via WM */
    float mean_plan_confidence;                 /**< Average plan confidence */

    /* Meta-Learning Statistics */
    uint64_t adaptation_triggers;               /**< Adaptation trigger events */
    float mean_recommended_lr;                  /**< Average recommended LR */

    /* Timing Statistics */
    uint64_t total_updates;                     /**< Total update cycles */
    double total_processing_time_ms;            /**< Total processing time */
    double mean_update_time_ms;                 /**< Average update duration */
    uint64_t last_update_time_us;               /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                      /**< Total errors encountered */
    uint64_t errors_prediction;                 /**< Prediction-related errors */
    uint64_t errors_goal;                       /**< Goal-related errors */
    uint64_t errors_attention;                  /**< Attention-related errors */
} omni_wm_cognitive_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Cognitive Bridge
 *
 * WHAT: Main bridge structure connecting WM with cognitive systems
 * WHY:  Orchestrates bidirectional information flow
 * HOW:  Maintains connections, effects, goals, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_cognitive_bridge {
    bridge_base_t base;                         /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_cognitive_bridge_config_t config;   /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;            /**< World model (RSSM) */
    executive_controller_t* executive;          /**< Executive function controller */
    working_memory_t* working_memory;           /**< Working memory system */
    salience_evaluator_t salience;              /**< Salience evaluator */
    meta_learner_t meta_learner;                /**< Meta-learning system */
    attention_system_t* attention;              /**< Attention system */

    /* Bidirectional Effects */
    omni_wm_to_cognitive_effects_t wm_to_cognitive; /**< Effects: WM -> Cognitive */
    cognitive_to_omni_wm_effects_t cognitive_to_wm; /**< Effects: Cognitive -> WM */

    /* Goal Tracking */
    wm_cognitive_goal_t goals[WM_COGNITIVE_MAX_GOALS]; /**< Tracked goals */
    uint32_t num_goals;                         /**< Number of active goals */
    uint32_t next_goal_id;                      /**< Next goal ID to assign */

    /* Attention State */
    wm_cognitive_focus_t current_focus;         /**< Current attention focus */
    bool focus_active;                          /**< Whether focus is active */
    uint64_t last_focus_update_us;              /**< Last focus update time */

    /* Salience State */
    wm_cognitive_salience_t current_salience;   /**< Current salience state */
    uint64_t last_salience_update_us;           /**< Last salience update time */

    /* Prediction Buffers */
    float* state_prediction_buffer;             /**< Buffer for state predictions */
    uint32_t state_prediction_dim;              /**< Prediction buffer dimension */
    float** action_consequence_buffer;          /**< Buffer for action consequences */
    uint32_t action_consequence_capacity;       /**< Action buffer capacity */

    /* Working Memory Context Cache */
    float* wm_context_cache;                    /**< Cached WM context */
    uint32_t wm_context_cache_dim;              /**< Context cache dimension */
    bool wm_context_valid;                      /**< Is cache valid */
    uint64_t wm_context_time;                   /**< Cache timestamp */

    /* Statistics */
    omni_wm_cognitive_bridge_stats_t stats;     /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;          /**< Per-instance health agent */
} omni_wm_cognitive_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Returns struct with all fields set to defaults
 *
 * @return Default configuration structure
 */
omni_wm_cognitive_bridge_config_t omni_wm_cognitive_bridge_default_config(void);

/**
 * @brief Create World Model Cognitive Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_cognitive_bridge_t* omni_wm_cognitive_bridge_create(
    const omni_wm_cognitive_bridge_config_t* config);

/**
 * @brief Destroy World Model Cognitive Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_cognitive_bridge_destroy(omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, clear goals, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_reset(omni_wm_cognitive_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all cognitive systems to bridge
 *
 * WHAT: Establish connections to WM, executive, working memory, salience, meta-learning
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param executive Executive controller - optional
 * @param working_memory Working memory system - optional
 * @param salience Salience evaluator - optional
 * @param meta_learner Meta-learner - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect(
    omni_wm_cognitive_bridge_t* bridge,
    omni_world_model_t* world_model,
    executive_controller_t* executive,
    working_memory_t* working_memory,
    salience_evaluator_t salience,
    meta_learner_t meta_learner);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_world_model(
    omni_wm_cognitive_bridge_t* bridge,
    omni_world_model_t* world_model);

/** Alias for connect_world_model (backward compatibility) */
#define omni_wm_cognitive_bridge_connect_wm omni_wm_cognitive_bridge_connect_world_model

/**
 * @brief Connect executive controller
 *
 * @param bridge Bridge instance
 * @param executive Executive controller to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_executive(
    omni_wm_cognitive_bridge_t* bridge,
    executive_controller_t* executive);

/**
 * @brief Connect working memory
 *
 * @param bridge Bridge instance
 * @param working_memory Working memory to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_working_memory(
    omni_wm_cognitive_bridge_t* bridge,
    working_memory_t* working_memory);

/**
 * @brief Connect salience evaluator
 *
 * @param bridge Bridge instance
 * @param salience Salience evaluator to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_salience(
    omni_wm_cognitive_bridge_t* bridge,
    salience_evaluator_t salience);

/**
 * @brief Connect meta-learner
 *
 * @param bridge Bridge instance
 * @param meta_learner Meta-learner to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_meta_learner(
    omni_wm_cognitive_bridge_t* bridge,
    meta_learner_t meta_learner);

/**
 * @brief Connect attention system
 *
 * @param bridge Bridge instance
 * @param attention Attention system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_attention(
    omni_wm_cognitive_bridge_t* bridge,
    attention_system_t* attention);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_cognitive_bridge_is_connected(const omni_wm_cognitive_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and cognitive systems
 * HOW:  Gather cognitive effects, compute WM effects, apply both
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_update(
    omni_wm_cognitive_bridge_t* bridge,
    float dt);

/* ============================================================================
 * Goal Management API
 * ============================================================================ */

/**
 * @brief Register a goal for world model conditioning
 *
 * WHAT: Add a goal for the world model to condition predictions
 * WHY:  Enable goal-directed state predictions
 * HOW:  Store goal state, track progress, provide to WM
 *
 * @param bridge Bridge instance
 * @param target_state Target state vector
 * @param state_dim Dimensionality of target state
 * @param priority Goal priority [0-1]
 * @param deadline_us Deadline timestamp (0=no deadline)
 * @param description Human-readable description
 * @param goal_id_out Output: assigned goal ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_register_goal(
    omni_wm_cognitive_bridge_t* bridge,
    const float* target_state,
    uint32_t state_dim,
    float priority,
    uint64_t deadline_us,
    const char* description,
    uint32_t* goal_id_out);

/**
 * @brief Update goal progress
 *
 * WHAT: Update progress toward a goal
 * WHY:  Track goal achievement, adjust predictions
 * HOW:  Update progress metric, check completion
 *
 * @param bridge Bridge instance
 * @param goal_id Goal to update
 * @param progress New progress value [0-1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_update_goal_progress(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id,
    float progress);

/**
 * @brief Mark goal as achieved
 *
 * WHAT: Signal goal completion
 * WHY:  Update tracking, trigger notifications
 * HOW:  Mark achieved, send notifications
 *
 * @param bridge Bridge instance
 * @param goal_id Goal to mark achieved
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_goal_achieved(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id);

/**
 * @brief Mark goal as failed
 *
 * WHAT: Signal goal failure
 * WHY:  Update tracking, allow recovery
 * HOW:  Mark failed, send notifications
 *
 * @param bridge Bridge instance
 * @param goal_id Goal to mark failed
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_goal_failed(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id);

/**
 * @brief Remove a goal
 *
 * WHAT: Remove goal from tracking
 * WHY:  Clean up completed or abandoned goals
 * HOW:  Remove from array, update indices
 *
 * @param bridge Bridge instance
 * @param goal_id Goal to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_remove_goal(
    omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id);

/**
 * @brief Set current action intention
 *
 * WHAT: Set the current action plan/intention
 * WHY:  World model uses intentions for consequence predictions
 * HOW:  Store action sequence with associated goal
 *
 * @param bridge Bridge instance
 * @param action_sequence Planned action sequence [length x action_dim]
 * @param action_dim Action dimensionality
 * @param sequence_length Number of actions in sequence
 * @param goal_id Associated goal ID (0=no goal)
 * @param confidence Confidence in intention
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_set_intention(
    omni_wm_cognitive_bridge_t* bridge,
    const float** action_sequence,
    uint32_t action_dim,
    uint32_t sequence_length,
    uint32_t goal_id,
    float confidence);

/* ============================================================================
 * Attention API
 * ============================================================================ */

/**
 * @brief Set attention focus for selective prediction
 *
 * WHAT: Specify where attention is focused
 * WHY:  World model prioritizes predictions for attended regions
 * HOW:  Store focus location, strength, and bandwidth
 *
 * @param bridge Bridge instance
 * @param focus_location Focus target in state space
 * @param focus_dim Dimensionality of focus
 * @param strength Attention strength [0-1]
 * @param bandwidth Spatial spread of attention
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_set_attention_focus(
    omni_wm_cognitive_bridge_t* bridge,
    const float* focus_location,
    uint32_t focus_dim,
    float strength,
    float bandwidth);

/**
 * @brief Handle attention shift event
 *
 * WHAT: Process attention shift from attention system
 * WHY:  Update prediction priorities based on new focus
 * HOW:  Update focus, decay old focus, boost new focus predictions
 *
 * @param bridge Bridge instance
 * @param new_focus_location New focus target
 * @param focus_dim Focus dimensionality
 * @param new_strength New attention strength
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_attention_shift(
    omni_wm_cognitive_bridge_t* bridge,
    const float* new_focus_location,
    uint32_t focus_dim,
    float new_strength);

/**
 * @brief Clear attention focus
 *
 * WHAT: Remove current attention focus
 * WHY:  Return to unfocused prediction mode
 * HOW:  Clear focus state, reset priorities
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_clear_attention(
    omni_wm_cognitive_bridge_t* bridge);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Get state prediction for planning
 *
 * WHAT: Request state prediction from world model
 * WHY:  Executive uses predictions for planning
 * HOW:  Query WM, apply goal conditioning, return prediction
 *
 * @param bridge Bridge instance
 * @param current_state Current state (NULL to use internal state)
 * @param state_dim State dimensionality
 * @param action Action to predict consequence of (NULL for autonomous)
 * @param action_dim Action dimensionality
 * @param predicted_state_out Output: predicted next state
 * @param confidence_out Output: prediction confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_predict_state(
    omni_wm_cognitive_bridge_t* bridge,
    const float* current_state,
    uint32_t state_dim,
    const float* action,
    uint32_t action_dim,
    float* predicted_state_out,
    float* confidence_out);

/**
 * @brief Predict action consequences for planning
 *
 * WHAT: Predict consequences of multiple actions
 * WHY:  Executive can compare action outcomes for selection
 * HOW:  Query WM for each action, return consequence predictions
 *
 * @param bridge Bridge instance
 * @param current_state Current state
 * @param state_dim State dimensionality
 * @param actions Array of actions to evaluate [num_actions x action_dim]
 * @param action_dim Action dimensionality
 * @param num_actions Number of actions to evaluate
 * @param consequences_out Output: predicted consequences [num_actions x state_dim]
 * @param values_out Output: expected values per action (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_predict_action_consequences(
    omni_wm_cognitive_bridge_t* bridge,
    const float* current_state,
    uint32_t state_dim,
    const float** actions,
    uint32_t action_dim,
    uint32_t num_actions,
    float** consequences_out,
    float* values_out);

/**
 * @brief Evaluate plan using world model
 *
 * WHAT: Evaluate a multi-step plan
 * WHY:  Executive can assess plan quality before execution
 * HOW:  Simulate plan execution, compute expected outcomes
 *
 * @param bridge Bridge instance
 * @param initial_state Starting state
 * @param state_dim State dimensionality
 * @param action_sequence Planned action sequence
 * @param action_dim Action dimensionality
 * @param sequence_length Number of actions
 * @param goal_id Target goal (0=no goal)
 * @param expected_value_out Output: expected plan value
 * @param success_prob_out Output: success probability
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_evaluate_plan(
    omni_wm_cognitive_bridge_t* bridge,
    const float* initial_state,
    uint32_t state_dim,
    const float** action_sequence,
    uint32_t action_dim,
    uint32_t sequence_length,
    uint32_t goal_id,
    float* expected_value_out,
    float* success_prob_out);

/* ============================================================================
 * Salience API
 * ============================================================================ */

/**
 * @brief Update salience signals
 *
 * WHAT: Update current salience metrics
 * WHY:  Salience affects prediction prioritization
 * HOW:  Store salience, update priorities
 *
 * @param bridge Bridge instance
 * @param novelty Novelty score [0-1]
 * @param surprise Surprise score [0-1]
 * @param urgency Urgency score [0-1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_update_salience(
    omni_wm_cognitive_bridge_t* bridge,
    float novelty,
    float surprise,
    float urgency);

/**
 * @brief Get prediction error map for attention guidance
 *
 * WHAT: Get spatial map of prediction errors
 * WHY:  High PE regions should receive attention
 * HOW:  Query WM for PE distribution
 *
 * @param bridge Bridge instance
 * @param pe_map_out Output: prediction error map
 * @param map_dim Dimensionality of map
 * @param max_pe_out Output: maximum PE location (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_get_prediction_error_map(
    omni_wm_cognitive_bridge_t* bridge,
    float* pe_map_out,
    uint32_t map_dim,
    float* max_pe_out);

/* ============================================================================
 * Working Memory Context API
 * ============================================================================ */

/**
 * @brief Update working memory context
 *
 * WHAT: Provide current WM contents as context for predictions
 * WHY:  WM items provide context for state predictions
 * HOW:  Aggregate WM items, cache as context vector
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_update_wm_context(
    omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Get current working memory context
 *
 * WHAT: Retrieve aggregated WM context
 * WHY:  Inspect what context is being used for predictions
 * HOW:  Return cached context vector
 *
 * @param bridge Bridge instance
 * @param context_out Output: context vector
 * @param context_dim Output buffer size
 * @param utilization_out Output: WM utilization (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_get_wm_context(
    const omni_wm_cognitive_bridge_t* bridge,
    float* context_out,
    uint32_t context_dim,
    float* utilization_out);

/* ============================================================================
 * Meta-Learning API
 * ============================================================================ */

/**
 * @brief Get recommended learning rate
 *
 * WHAT: Get learning rate recommendation from meta-learner integration
 * WHY:  Adapt learning based on prediction performance
 * HOW:  Compute based on PE, task difficulty, adaptation progress
 *
 * @param bridge Bridge instance
 * @param recommended_lr_out Output: recommended learning rate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_get_recommended_lr(
    const omni_wm_cognitive_bridge_t* bridge,
    float* recommended_lr_out);

/**
 * @brief Trigger task adaptation
 *
 * WHAT: Signal that task adaptation is needed
 * WHY:  High prediction errors may indicate new task
 * HOW:  Notify meta-learner, adjust parameters
 *
 * @param bridge Bridge instance
 * @param prediction_error Current prediction error
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_trigger_adaptation(
    omni_wm_cognitive_bridge_t* bridge,
    float prediction_error);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to cognitive
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_cognitive_effects_t* omni_wm_cognitive_bridge_get_wm_effects(
    const omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Get current effects from cognitive to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const cognitive_to_omni_wm_effects_t* omni_wm_cognitive_bridge_get_cognitive_effects(
    const omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_get_stats(
    const omni_wm_cognitive_bridge_t* bridge,
    omni_wm_cognitive_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_reset_stats(
    omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Get goal by ID
 *
 * @param bridge Bridge instance
 * @param goal_id Goal ID to retrieve
 * @return Pointer to goal or NULL if not found
 */
const wm_cognitive_goal_t* omni_wm_cognitive_bridge_get_goal(
    const omni_wm_cognitive_bridge_t* bridge,
    uint32_t goal_id);

/**
 * @brief Get number of active goals
 *
 * @param bridge Bridge instance
 * @return Number of currently active goals
 */
uint32_t omni_wm_cognitive_bridge_get_num_goals(
    const omni_wm_cognitive_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_connect_bio_async(
    omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_cognitive_bridge_disconnect_bio_async(
    omni_wm_cognitive_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_cognitive_bridge_is_bio_async_connected(
    const omni_wm_cognitive_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_cognitive_msg_type_to_string(omni_wm_cognitive_msg_type_t msg_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_cognitive_bridge_validate_config(
    const omni_wm_cognitive_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_COGNITIVE_BRIDGE_H */
