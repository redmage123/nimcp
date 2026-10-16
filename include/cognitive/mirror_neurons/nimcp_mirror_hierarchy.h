/**
 * @file nimcp_mirror_hierarchy.h
 * @brief Hierarchical Action Representation for Mirror Neurons
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Hierarchical action representation separating goals from motor details
 * WHY:  Model the distinct roles of IPL (goals) and F5 (motor) in action understanding
 * HOW:  Dual-level representation with goal abstraction and motor binding
 *
 * Hierarchical Action Understanding:
 * Mirror neurons don't just code "what" action is happening - they encode both
 * the goal of the action (WHY) and its motor implementation (HOW). These are
 * processed in different brain regions:
 *
 * - Inferior Parietal Lobule (IPL): Goal-level encoding
 *   "What is the intention?" (grasp-to-eat vs grasp-to-place)
 *
 * - Premotor Cortex (F5/PMv): Motor-level encoding
 *   "What are the exact motor parameters?" (grip aperture, trajectory)
 *
 * This separation allows:
 * 1. Same goal achieved by different motor means
 * 2. Same motor action serving different goals
 * 3. Goal inference from partial observations
 * 4. Flexible imitation (match goal, adapt motor details)
 *
 * Biological Basis:
 * - Fogassi et al. (2005): IPL neurons encode action goals
 * - Rizzolatti & Sinigaglia (2010): Two-level mirror system
 * - Bonini et al. (2010): Goal coding in parietal cortex
 * - Hamilton & Grafton (2006): Goal vs. means imitation
 *
 * Integration Points:
 * - Mirror neuron observation input
 * - Motor resonance (feeds into motor layer)
 * - STDP learning (updates goal-motor bindings)
 * - Prefrontal cortex (top-down goal selection)
 *
 * @see Phase 10.11.6 - Enhanced Mirror Neuron Goal Hierarchy
 */

#ifndef NIMCP_MIRROR_HIERARCHY_H
#define NIMCP_MIRROR_HIERARCHY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Defaults
//=============================================================================

/** @brief Maximum goal representations */
#define NIMCP_HIERARCHY_MAX_GOALS           128

/** @brief Maximum motor representations */
#define NIMCP_HIERARCHY_MAX_MOTORS          512

/** @brief Maximum bindings per goal */
#define NIMCP_HIERARCHY_MAX_BINDINGS        16

/** @brief Goal activation decay constant (ms) */
#define NIMCP_HIERARCHY_TAU_GOAL_DECAY      500.0f

/** @brief Motor activation decay constant (ms) */
#define NIMCP_HIERARCHY_TAU_MOTOR_DECAY     100.0f

/** @brief Default goal-motor binding strength */
#define NIMCP_HIERARCHY_DEFAULT_BINDING     0.5f

/** @brief Threshold for goal inference */
#define NIMCP_HIERARCHY_GOAL_THRESHOLD      0.5f

//=============================================================================
// Hierarchy Types
//=============================================================================

/**
 * @brief Goal categories
 *
 * WHAT: High-level goal classifications
 * WHY:  Organize goals into semantic categories
 */
typedef enum {
    GOAL_CATEGORY_UNKNOWN = 0,
    GOAL_CATEGORY_CONSUMATORY,    /**< Eating, drinking */
    GOAL_CATEGORY_PLACING,        /**< Putting objects down */
    GOAL_CATEGORY_GIVING,         /**< Handing to another agent */
    GOAL_CATEGORY_TAKING,         /**< Taking from another agent */
    GOAL_CATEGORY_COMMUNICATIVE,  /**< Gestures, pointing */
    GOAL_CATEGORY_EXPLORATORY,    /**< Examining, manipulating */
    GOAL_CATEGORY_SOCIAL,         /**< Social interaction */
    GOAL_CATEGORY_DEFENSIVE,      /**< Protective actions */
    GOAL_CATEGORY_LOCOMOTION,     /**< Movement-related */
    GOAL_CATEGORY_CUSTOM          /**< User-defined */
} goal_category_t;

/**
 * @brief Motor primitive types
 *
 * WHAT: Basic motor building blocks
 * WHY:  Standardize motor representation
 */
typedef enum {
    MOTOR_TYPE_UNKNOWN = 0,
    MOTOR_TYPE_REACH,            /**< Reaching movement */
    MOTOR_TYPE_GRASP,            /**< Grip formation */
    MOTOR_TYPE_MANIPULATE,       /**< In-hand manipulation */
    MOTOR_TYPE_RELEASE,          /**< Opening grip */
    MOTOR_TYPE_TRANSPORT,        /**< Moving while holding */
    MOTOR_TYPE_GESTURE,          /**< Communicative hand shape */
    MOTOR_TYPE_POSTURE,          /**< Body configuration */
    MOTOR_TYPE_LOCOMOTE,         /**< Body movement */
    MOTOR_TYPE_ORIENT,           /**< Head/body orientation */
    MOTOR_TYPE_CUSTOM            /**< User-defined */
} motor_type_t;

/**
 * @brief Binding type
 *
 * WHAT: Nature of goal-motor relationship
 * WHY:  Distinguish how goals bind to motors
 */
typedef enum {
    BINDING_INVARIANT,           /**< Always uses this motor */
    BINDING_FLEXIBLE,            /**< Commonly but not always */
    BINDING_CONTEXTUAL,          /**< Depends on context */
    BINDING_LEARNED              /**< Recently learned association */
} binding_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Hierarchical system configuration
 *
 * WHAT: Configuration for goal-motor hierarchy
 * WHY:  Allow tuning of hierarchy parameters
 */
typedef struct {
    // Capacity
    uint32_t max_goals;              /**< Maximum goal representations */
    uint32_t max_motors;             /**< Maximum motor representations */
    uint32_t max_bindings_per_goal;  /**< Max motors per goal */

    // Dynamics
    float tau_goal_decay;            /**< Goal activation decay (ms) */
    float tau_motor_decay;           /**< Motor activation decay (ms) */
    float goal_inference_threshold;  /**< Threshold for goal inference */

    // Learning
    float binding_learning_rate;     /**< How fast bindings strengthen */
    float binding_decay_rate;        /**< How fast unused bindings weaken */
    float min_binding_strength;      /**< Minimum binding before removal */

    // Top-down modulation
    float goal_top_down_gain;        /**< PFC goal selection gain */
    bool enable_goal_competition;    /**< Goals compete for dominance */

    // Bottom-up inference
    float motor_to_goal_gain;        /**< Motor drives goal inference */
    bool enable_predictive_coding;   /**< Predict motor from goal */

} mirror_hierarchy_config_t;

/**
 * @brief Goal-motor binding
 *
 * WHAT: Association between a goal and a motor representation
 * WHY:  Track how goals map to motor implementations
 */
typedef struct {
    uint32_t motor_id;               /**< Bound motor representation */
    float binding_strength;          /**< Association strength [0, 1] */
    binding_type_t binding_type;     /**< Type of binding */
    float usage_count;               /**< How often this binding used */
    float last_used_time;            /**< Recency tracking */
} goal_motor_binding_t;

/**
 * @brief Goal representation (IPL-level)
 *
 * WHAT: Abstract goal-level representation
 * WHY:  Encode action intentions independent of motor details
 */
typedef struct {
    uint32_t goal_id;                /**< Unique goal identifier */
    goal_category_t category;        /**< Goal category */
    char name[64];                   /**< Human-readable name */

    // Activation state
    float activation;                /**< Current activation [0, 1] */
    float peak_activation;           /**< Recent peak */
    float target_activation;         /**< Target from input */

    // Top-down state
    float top_down_bias;             /**< PFC bias toward this goal */
    bool is_selected;                /**< Currently selected goal */

    // Motor bindings
    goal_motor_binding_t bindings[NIMCP_HIERARCHY_MAX_BINDINGS];
    uint32_t num_bindings;           /**< Number of bound motors */

    // Context features
    float context_features[16];      /**< Contextual feature vector */
    uint32_t num_context_features;

    // Statistics
    uint32_t inference_count;        /**< Times inferred from motor */
    uint32_t selection_count;        /**< Times selected as intention */
    float total_active_time;         /**< Cumulative active time (ms) */

} goal_representation_t;

/**
 * @brief Motor representation (F5-level)
 *
 * WHAT: Motor-level representation with parameters
 * WHY:  Encode specific motor implementations
 */
typedef struct {
    uint32_t motor_id;               /**< Unique motor identifier */
    motor_type_t type;               /**< Motor primitive type */
    char name[64];                   /**< Human-readable name */

    // Activation state
    float activation;                /**< Current activation [0, 1] */
    float peak_activation;           /**< Recent peak */

    // Motor parameters (normalized)
    float parameters[8];             /**< Motor parameters */
    uint32_t num_parameters;

    // Goal association
    uint32_t primary_goal;           /**< Most associated goal */
    float goal_evidence;             /**< Evidence for primary goal */

    // Prediction
    float predicted_activation;      /**< Top-down prediction from goal */
    float prediction_error;          /**< Bottom-up vs prediction */

    // Statistics
    uint32_t execution_count;        /**< Times executed */
    float total_active_time;         /**< Cumulative time */

} motor_representation_t;

/**
 * @brief Mirror hierarchy system
 *
 * WHAT: Complete goal-motor hierarchy system
 * WHY:  Manage dual-level action representation
 */
typedef struct mirror_hierarchy_system mirror_hierarchy_system_t;
typedef mirror_hierarchy_system_t* mirror_hierarchy_t;

/**
 * @brief Hierarchy statistics
 *
 * WHAT: Runtime statistics for hierarchy system
 * WHY:  Monitor system behavior
 */
typedef struct {
    // Counts
    uint32_t num_goals;              /**< Total goal representations */
    uint32_t num_motors;             /**< Total motor representations */
    uint32_t num_bindings;           /**< Total goal-motor bindings */

    // Activation
    uint32_t active_goals;           /**< Goals with activation > 0.1 */
    uint32_t active_motors;          /**< Motors with activation > 0.1 */
    float mean_goal_activation;      /**< Mean goal activation */
    float mean_motor_activation;     /**< Mean motor activation */

    // Inference
    uint32_t goal_inferences;        /**< Times goal inferred from motor */
    uint32_t motor_predictions;      /**< Times motor predicted from goal */
    float avg_prediction_error;      /**< Mean prediction error */

    // Learning
    uint32_t bindings_strengthened;  /**< Bindings that got stronger */
    uint32_t bindings_weakened;      /**< Bindings that got weaker */
    uint32_t bindings_created;       /**< New bindings formed */
    uint32_t bindings_removed;       /**< Old bindings removed */

} mirror_hierarchy_stats_t;

//=============================================================================
// Lifecycle Management
//=============================================================================

/**
 * @brief Get default hierarchy configuration
 *
 * WHAT: Return sensible defaults
 * WHY:  Provide biological starting point
 *
 * @return Default configuration
 */
mirror_hierarchy_config_t mirror_hierarchy_get_default_config(void);

/**
 * @brief Create hierarchy system
 *
 * WHAT: Initialize goal-motor hierarchy
 * WHY:  Enable hierarchical action representation
 *
 * @param config Configuration (NULL = use defaults)
 * @return Hierarchy system handle or NULL on error
 */
mirror_hierarchy_t mirror_hierarchy_create(const mirror_hierarchy_config_t* config);

/**
 * @brief Destroy hierarchy system
 *
 * WHAT: Free all hierarchy resources
 * WHY:  Prevent memory leaks
 *
 * @param hierarchy System to destroy (NULL-safe)
 */
void mirror_hierarchy_destroy(mirror_hierarchy_t hierarchy);

//=============================================================================
// Goal Management (IPL Level)
//=============================================================================

/**
 * @brief Create goal representation
 *
 * WHAT: Add a new goal to the hierarchy
 * WHY:  Enable goal-level action encoding
 *
 * @param hierarchy Hierarchy system
 * @param name Human-readable goal name
 * @param category Goal category
 * @return Goal ID or UINT32_MAX on error
 */
uint32_t mirror_hierarchy_create_goal(mirror_hierarchy_t hierarchy,
                                       const char* name,
                                       goal_category_t category);

/**
 * @brief Get goal representation
 *
 * WHAT: Query goal state
 * WHY:  Monitor goal activation
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Goal to query
 * @param out_goal Output: goal state
 * @return true on success
 */
bool mirror_hierarchy_get_goal(mirror_hierarchy_t hierarchy, uint32_t goal_id,
                                goal_representation_t* out_goal);

/**
 * @brief Activate goal (top-down)
 *
 * WHAT: Set goal activation from top-down (PFC) signal
 * WHY:  Enable intentional goal selection
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Goal to activate
 * @param activation Activation level (0-1)
 */
void mirror_hierarchy_activate_goal(mirror_hierarchy_t hierarchy,
                                     uint32_t goal_id, float activation);

/**
 * @brief Select goal (top-down)
 *
 * WHAT: Select a goal as current intention
 * WHY:  Focus processing on specific goal
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Goal to select (-1 to clear)
 */
void mirror_hierarchy_select_goal(mirror_hierarchy_t hierarchy, int32_t goal_id);

/**
 * @brief Get selected goal
 *
 * WHAT: Query which goal is currently selected
 * WHY:  Determine current intention
 *
 * @param hierarchy Hierarchy system
 * @return Selected goal ID or UINT32_MAX if none
 */
uint32_t mirror_hierarchy_get_selected_goal(mirror_hierarchy_t hierarchy);

//=============================================================================
// Motor Management (F5 Level)
//=============================================================================

/**
 * @brief Create motor representation
 *
 * WHAT: Add a motor representation
 * WHY:  Enable motor-level encoding
 *
 * @param hierarchy Hierarchy system
 * @param name Human-readable motor name
 * @param type Motor primitive type
 * @return Motor ID or UINT32_MAX on error
 */
uint32_t mirror_hierarchy_create_motor(mirror_hierarchy_t hierarchy,
                                        const char* name,
                                        motor_type_t type);

/**
 * @brief Get motor representation
 *
 * WHAT: Query motor state
 * WHY:  Monitor motor activation
 *
 * @param hierarchy Hierarchy system
 * @param motor_id Motor to query
 * @param out_motor Output: motor state
 * @return true on success
 */
bool mirror_hierarchy_get_motor(mirror_hierarchy_t hierarchy, uint32_t motor_id,
                                 motor_representation_t* out_motor);

/**
 * @brief Activate motor (bottom-up)
 *
 * WHAT: Set motor activation from observation
 * WHY:  Enable motor-level processing
 *
 * @param hierarchy Hierarchy system
 * @param motor_id Motor to activate
 * @param activation Activation level (0-1)
 */
void mirror_hierarchy_activate_motor(mirror_hierarchy_t hierarchy,
                                      uint32_t motor_id, float activation);

/**
 * @brief Set motor parameters
 *
 * WHAT: Set specific motor parameters
 * WHY:  Encode motor details (grip aperture, etc.)
 *
 * @param hierarchy Hierarchy system
 * @param motor_id Motor to configure
 * @param parameters Array of parameters
 * @param num_params Number of parameters
 */
void mirror_hierarchy_set_motor_params(mirror_hierarchy_t hierarchy,
                                        uint32_t motor_id,
                                        const float* parameters,
                                        uint32_t num_params);

//=============================================================================
// Goal-Motor Binding
//=============================================================================

/**
 * @brief Create goal-motor binding
 *
 * WHAT: Associate a motor with a goal
 * WHY:  Enable goal-to-motor mapping
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Goal to bind
 * @param motor_id Motor to bind
 * @param strength Initial binding strength
 * @param type Binding type
 * @return true on success
 */
bool mirror_hierarchy_create_binding(mirror_hierarchy_t hierarchy,
                                      uint32_t goal_id,
                                      uint32_t motor_id,
                                      float strength,
                                      binding_type_t type);

/**
 * @brief Strengthen binding
 *
 * WHAT: Increase goal-motor association
 * WHY:  Hebbian learning of goal-motor pairs
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Goal in binding
 * @param motor_id Motor in binding
 * @param delta Strength increase
 */
void mirror_hierarchy_strengthen_binding(mirror_hierarchy_t hierarchy,
                                          uint32_t goal_id,
                                          uint32_t motor_id,
                                          float delta);

/**
 * @brief Get binding strength
 *
 * WHAT: Query goal-motor association strength
 * WHY:  Check binding state
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Goal to query
 * @param motor_id Motor to query
 * @return Binding strength or -1 if not bound
 */
float mirror_hierarchy_get_binding(mirror_hierarchy_t hierarchy,
                                    uint32_t goal_id,
                                    uint32_t motor_id);

//=============================================================================
// Inference
//=============================================================================

/**
 * @brief Infer goal from motor (bottom-up)
 *
 * WHAT: Determine likely goal given observed motor
 * WHY:  Understand action intention from motor observation
 *
 * @param hierarchy Hierarchy system
 * @param motor_id Observed motor
 * @param out_goal_ids Output: likely goals (sorted by probability)
 * @param out_probs Output: goal probabilities
 * @param max_goals Maximum goals to return
 * @return Number of likely goals
 */
uint32_t mirror_hierarchy_infer_goal(mirror_hierarchy_t hierarchy,
                                      uint32_t motor_id,
                                      uint32_t* out_goal_ids,
                                      float* out_probs,
                                      uint32_t max_goals);

/**
 * @brief Predict motor from goal (top-down)
 *
 * WHAT: Predict likely motor given goal
 * WHY:  Anticipate action implementation
 *
 * @param hierarchy Hierarchy system
 * @param goal_id Selected goal
 * @param out_motor_ids Output: likely motors
 * @param out_probs Output: motor probabilities
 * @param max_motors Maximum motors to return
 * @return Number of likely motors
 */
uint32_t mirror_hierarchy_predict_motor(mirror_hierarchy_t hierarchy,
                                         uint32_t goal_id,
                                         uint32_t* out_motor_ids,
                                         float* out_probs,
                                         uint32_t max_motors);

/**
 * @brief Context-dependent goal inference
 *
 * WHAT: Infer goal considering context
 * WHY:  Same motor can serve different goals in different contexts
 *
 * @param hierarchy Hierarchy system
 * @param motor_id Observed motor
 * @param context_features Context feature vector
 * @param num_features Number of context features
 * @return Most likely goal ID
 */
uint32_t mirror_hierarchy_infer_goal_contextual(mirror_hierarchy_t hierarchy,
                                                 uint32_t motor_id,
                                                 const float* context_features,
                                                 uint32_t num_features);

//=============================================================================
// Time Update
//=============================================================================

/**
 * @brief Step hierarchy simulation
 *
 * WHAT: Advance system by one timestep
 * WHY:  Update decay, competition, prediction
 *
 * @param hierarchy Hierarchy system
 * @param dt_ms Time step in milliseconds
 */
void mirror_hierarchy_step(mirror_hierarchy_t hierarchy, float dt_ms);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get hierarchy statistics
 *
 * WHAT: Retrieve comprehensive statistics
 * WHY:  Monitor system behavior
 *
 * @param hierarchy Hierarchy system
 * @param stats Output: statistics
 * @return true on success
 */
bool mirror_hierarchy_get_stats(mirror_hierarchy_t hierarchy, mirror_hierarchy_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement
 *
 * @param hierarchy Hierarchy system
 */
void mirror_hierarchy_reset_stats(mirror_hierarchy_t hierarchy);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIRROR_HIERARCHY_H
