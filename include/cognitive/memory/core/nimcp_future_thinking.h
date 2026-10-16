//=============================================================================
// nimcp_future_thinking.h - Episodic Future Thinking for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_future_thinking.h
 * @brief Mental simulation of future events and prospective planning
 *
 * WHAT: Episodic future thinking - constructing mental simulations of personal
 *       future events, prospection, and goal-directed planning
 * WHY:  Future thinking enables prediction, planning, and decision-making by
 *       recombining elements from past experience into novel scenarios
 * HOW:  Samples fragments from episodic memory, recombines via entanglement,
 *       evaluates coherence and probability, predicts emotional outcomes
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Episodic Future Thinking Model:
 *   +-----------------------------------------------------------------------+
 *   |  Episodic future thinking shares neural substrates with episodic      |
 *   |  memory (hippocampus, medial PFC, posterior parietal cortex)          |
 *   |                                                                       |
 *   |  Key Brain Regions:                                                   |
 *   |  - Hippocampus: Scene construction from memory fragments              |
 *   |  - Medial PFC: Self-referential processing, valuation                 |
 *   |  - Posterior Parietal: Spatial scene representation                   |
 *   |  - Temporal Pole: Semantic knowledge integration                      |
 *   |  - Amygdala: Emotional anticipation                                   |
 *   |                                                                       |
 *   |  Constructive Episodic Simulation Hypothesis:                         |
 *   |  "Future events are constructed by flexibly recombining elements      |
 *   |   from past experiences into novel scenarios"                         |
 *   |   - Schacter & Addis, 2007                                            |
 *   +-----------------------------------------------------------------------+
 *
 *   Future Event Types:
 *   +-----------------------------------------------------------------------+
 *   |  SPECIFIC:       Concrete personal future event                       |
 *   |                  "My birthday party next Saturday"                    |
 *   |                  Neural: Hippocampal-neocortical simulation           |
 *   |                                                                       |
 *   |  SEMANTIC:       General future knowledge                              |
 *   |                  "Summer will be hot, winter will be cold"            |
 *   |                  Neural: Temporal lobe semantic stores                |
 *   |                                                                       |
 *   |  HYPOTHETICAL:   What-if scenarios                                    |
 *   |                  "If I took that job in another city..."              |
 *   |                  Neural: PFC counterfactual reasoning                 |
 *   |                                                                       |
 *   |  GOAL:           Goal-directed planning                               |
 *   |                  "To get promoted, I need to..."                      |
 *   |                  Neural: Dorsolateral PFC-striatal planning           |
 *   +-----------------------------------------------------------------------+
 *
 *   Scene Construction Process:
 *   +-----------------------------------------------------------------------+
 *   |  1. RETRIEVE: Sample relevant episodic memory fragments               |
 *   |     - Query entanglement graph with goal/context                      |
 *   |     - Select fragments via resonance with current situation           |
 *   |                                                                       |
 *   |  2. EXTRACT: Decompose memories into reusable elements                |
 *   |     - People, places, objects, actions, emotions                      |
 *   |     - Maintain relational structure where appropriate                 |
 *   |                                                                       |
 *   |  3. RECOMBINE: Flexibly combine into novel scenario                   |
 *   |     - Quaternion blending for emotional tone                          |
 *   |     - Prime signature composition for content                         |
 *   |                                                                       |
 *   |  4. ELABORATE: Add details for coherent scene                         |
 *   |     - Spatial layout, temporal sequence, causal chains                |
 *   |     - Fill gaps with semantic knowledge                               |
 *   |                                                                       |
 *   |  5. EVALUATE: Assess coherence and probability                        |
 *   |     - Spatial/temporal/causal consistency checks                      |
 *   |     - Estimate likelihood based on past experience                    |
 *   +-----------------------------------------------------------------------+
 *
 *   Temporal Discounting:
 *   +-----------------------------------------------------------------------+
 *   |  Future rewards are discounted hyperbolically:                         |
 *   |  V(t) = V(0) / (1 + k*t)                                               |
 *   |                                                                       |
 *   |  Where:                                                               |
 *   |  - V(t) = subjective value at delay t                                 |
 *   |  - V(0) = immediate value                                             |
 *   |  - k = discount rate (individual parameter)                           |
 *   |                                                                       |
 *   |  Implications for future thinking:                                    |
 *   |  - Distant futures are psychologically "fuzzy"                        |
 *   |  - Near futures are more vivid, emotionally engaging                  |
 *   |  - Episodic simulation can reduce discounting                         |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Event simulation: ~1-5ms (depends on fragment sampling)
 * - Scene construction: ~500us
 * - Coherence evaluation: ~100us
 * - Probability estimation: ~50us
 * - Emotional forecast: ~30us
 *
 * MEMORY:
 * - future_thinking_t: ~2KB (core state)
 * - future_event_t: ~256 bytes per event
 * - future_scene_t: ~512 bytes per scene
 * - Fragment pool: configurable, default ~64KB
 *
 * INTEGRATION:
 * - Core: PR memory nodes, entanglement graph, resonance
 * - Middleware: Goal manager, planning system
 * - API: Future simulation queries
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_FUTURE_THINKING_H
#define NIMCP_FUTURE_THINKING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_resonance.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum description length for future events */
#define FUTURE_MAX_DESCRIPTION_LEN      1024

/** Maximum participants in a future event */
#define FUTURE_MAX_PARTICIPANTS         32

/** Maximum source memories for event construction */
#define FUTURE_MAX_SOURCE_MEMORIES      64

/** Maximum scene elements */
#define FUTURE_MAX_SCENE_ELEMENTS       128

/** Maximum active goals */
#define FUTURE_MAX_ACTIVE_GOALS         16

/** Maximum subgoals per goal */
#define FUTURE_MAX_SUBGOALS             32

/** Maximum events in future thinking system */
#define FUTURE_DEFAULT_MAX_EVENTS       256

/** Default fragment pool size */
#define FUTURE_DEFAULT_POOL_SIZE        128

/** Default discount rate (k) for temporal discounting */
#define FUTURE_DEFAULT_DISCOUNT_RATE    0.01f

/** Minimum coherence threshold for valid scene */
#define FUTURE_MIN_COHERENCE_THRESHOLD  0.3f

/** Default probability threshold for likely events */
#define FUTURE_DEFAULT_PROB_THRESHOLD   0.5f

/** Temporal horizon: maximum simulation distance (seconds) */
#define FUTURE_MAX_TEMPORAL_HORIZON     (365.0f * 24.0f * 3600.0f)  // 1 year

/** Epsilon for floating point comparisons */
#define FUTURE_EPSILON                  1e-6f

/** Invalid event ID sentinel */
#define FUTURE_INVALID_EVENT_ID         UINT64_MAX

/** Invalid goal ID sentinel */
#define FUTURE_INVALID_GOAL_ID          UINT64_MAX

//=============================================================================
// Type Definitions - Core Enums
//=============================================================================

/**
 * @brief Types of future events
 *
 * Categorizes future simulations by their nature and cognitive function
 */
typedef enum {
    FUTURE_SPECIFIC = 0,    /**< Concrete personal future event ("My wedding") */
    FUTURE_SEMANTIC,        /**< General future knowledge ("Summer will be hot") */
    FUTURE_HYPOTHETICAL,    /**< What-if counterfactual ("If I moved...") */
    FUTURE_GOAL,            /**< Goal-directed planning ("To achieve X...") */
    FUTURE_TYPE_COUNT       /**< Number of event types (for arrays) */
} future_event_type_t;

/**
 * @brief Simulation status
 */
typedef enum {
    FUTURE_SIM_IDLE = 0,        /**< No active simulation */
    FUTURE_SIM_SAMPLING,        /**< Sampling memory fragments */
    FUTURE_SIM_EXTRACTING,      /**< Extracting elements from fragments */
    FUTURE_SIM_CONSTRUCTING,    /**< Building the scene */
    FUTURE_SIM_EVALUATING,      /**< Evaluating coherence */
    FUTURE_SIM_COMPLETE,        /**< Simulation complete */
    FUTURE_SIM_FAILED           /**< Simulation failed */
} future_sim_status_t;

/**
 * @brief Scene element types
 */
typedef enum {
    SCENE_ELEMENT_PERSON = 0,   /**< A person/agent in the scene */
    SCENE_ELEMENT_PLACE,        /**< A location/setting */
    SCENE_ELEMENT_OBJECT,       /**< A physical object */
    SCENE_ELEMENT_ACTION,       /**< An action/event */
    SCENE_ELEMENT_EMOTION,      /**< An emotional state */
    SCENE_ELEMENT_TIME,         /**< Temporal marker */
    SCENE_ELEMENT_RELATION,     /**< Relationship between elements */
    SCENE_ELEMENT_TYPE_COUNT    /**< Number of element types */
} scene_element_type_t;

/**
 * @brief Goal status
 */
typedef enum {
    GOAL_STATUS_INACTIVE = 0,   /**< Goal not currently pursued */
    GOAL_STATUS_ACTIVE,         /**< Goal being actively pursued */
    GOAL_STATUS_ACHIEVED,       /**< Goal has been achieved */
    GOAL_STATUS_ABANDONED,      /**< Goal was abandoned */
    GOAL_STATUS_BLOCKED         /**< Goal is blocked by obstacles */
} goal_status_t;

/**
 * @brief Error codes for future thinking operations
 */
typedef enum {
    FUTURE_SUCCESS = 0,                     /**< Operation succeeded */
    FUTURE_ERROR_NULL_POINTER = -1,         /**< NULL pointer argument */
    FUTURE_ERROR_INVALID_CONFIG = -2,       /**< Invalid configuration */
    FUTURE_ERROR_NO_MEMORY = -3,            /**< Memory allocation failed */
    FUTURE_ERROR_INVALID_EVENT = -4,        /**< Invalid event reference */
    FUTURE_ERROR_INVALID_GOAL = -5,         /**< Invalid goal reference */
    FUTURE_ERROR_MAX_EVENTS = -6,           /**< Maximum events reached */
    FUTURE_ERROR_MAX_GOALS = -7,            /**< Maximum goals reached */
    FUTURE_ERROR_SIMULATION_FAILED = -8,    /**< Simulation construction failed */
    FUTURE_ERROR_LOW_COHERENCE = -9,        /**< Scene coherence too low */
    FUTURE_ERROR_NO_FRAGMENTS = -10,        /**< No suitable fragments found */
    FUTURE_ERROR_INVALID_TIME = -11,        /**< Invalid temporal specification */
    FUTURE_ERROR_ALREADY_SIMULATING = -12   /**< Simulation already in progress */
} future_error_t;

//=============================================================================
// Type Definitions - Scene Elements
//=============================================================================

/**
 * @brief Individual element within a future scene
 *
 * Represents a single component (person, place, object, etc.) that makes
 * up part of a constructed future scenario.
 */
typedef struct {
    scene_element_type_t type;          /**< Type of element */
    uint64_t element_id;                /**< Unique element identifier */
    prime_signature_t signature;        /**< Content signature of element */
    nimcp_quaternion_t state;           /**< Element's semantic state */

    // Source tracking
    uint64_t source_memory_id;          /**< Memory this was extracted from */
    float extraction_confidence;        /**< Confidence in extraction [0,1] */

    // Relational properties
    uint64_t related_to[8];             /**< IDs of related elements */
    size_t num_relations;               /**< Number of relations */

    // Spatial/temporal properties
    float position[3];                  /**< 3D position in scene (if applicable) */
    float temporal_offset;              /**< Time offset in scene (seconds) */
} scene_element_t;

/**
 * @brief Constructed future scene
 *
 * A coherent mental simulation composed of scene elements arranged
 * spatially and temporally.
 */
typedef struct {
    // Scene identity
    uint64_t scene_id;                      /**< Unique scene identifier */

    // Scene elements
    scene_element_t* elements;              /**< Array of scene elements */
    size_t num_elements;                    /**< Number of elements */
    size_t max_elements;                    /**< Capacity */

    // Coherence metrics
    float spatial_coherence;                /**< Elements fit spatially [0,1] */
    float temporal_coherence;               /**< Events fit temporally [0,1] */
    float causal_coherence;                 /**< Events causally connected [0,1] */
    float overall_coherence;                /**< Combined coherence score */

    // Vividness metrics
    float visual_detail;                    /**< Visual imagery vividness [0,1] */
    float auditory_detail;                  /**< Auditory detail [0,1] */
    float kinesthetic_detail;               /**< Movement/sensation detail [0,1] */
    float overall_vividness;                /**< Combined vividness */

    // Emotional quality
    nimcp_quaternion_t emotional_tone;      /**< Overall emotional character */

    // Metadata
    uint64_t construction_time_ms;          /**< When scene was built */
    uint32_t construction_iterations;       /**< Iterations to construct */
} future_scene_t;

//=============================================================================
// Type Definitions - Future Events
//=============================================================================

/**
 * @brief A simulated future event
 *
 * Represents a complete episodic future thinking simulation including
 * content, temporal properties, participants, emotional anticipation,
 * and planning relevance.
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t event_id;                      /**< Unique event identifier */
    future_event_type_t type;               /**< Type of future event */

    //-------------------------------------------------------------------------
    // Content
    //-------------------------------------------------------------------------
    char description[FUTURE_MAX_DESCRIPTION_LEN]; /**< Text description */
    prime_signature_t event_signature;      /**< Content signature */
    nimcp_quaternion_t event_quaternion;    /**< Semantic state encoding */

    //-------------------------------------------------------------------------
    // Temporal Properties
    //-------------------------------------------------------------------------
    float expected_time;                    /**< Expected time (seconds from now) */
    float time_uncertainty;                 /**< Temporal uncertainty (std dev) */
    float temporal_distance;                /**< Psychological distance [0,1] */
    float discounted_value;                 /**< Temporally discounted value */

    //-------------------------------------------------------------------------
    // Spatial Properties
    //-------------------------------------------------------------------------
    prime_signature_t location_signature;   /**< Where (location encoding) */
    float location_familiarity;             /**< How familiar is location [0,1] */

    //-------------------------------------------------------------------------
    // Participants
    //-------------------------------------------------------------------------
    uint64_t participant_ids[FUTURE_MAX_PARTICIPANTS]; /**< Who's involved */
    size_t num_participants;                /**< Number of participants */
    float self_relevance;                   /**< Self-referential relevance [0,1] */

    //-------------------------------------------------------------------------
    // Source Memories
    //-------------------------------------------------------------------------
    uint64_t source_memory_ids[FUTURE_MAX_SOURCE_MEMORIES]; /**< Source memories */
    float source_weights[FUTURE_MAX_SOURCE_MEMORIES];       /**< Fragment weights */
    size_t num_sources;                     /**< Number of source memories */

    //-------------------------------------------------------------------------
    // Constructed Scene
    //-------------------------------------------------------------------------
    future_scene_t scene;                   /**< The mental scene */

    //-------------------------------------------------------------------------
    // Emotional Anticipation
    //-------------------------------------------------------------------------
    float anticipated_valence;              /**< Expected emotion [-1,+1] */
    float anticipated_arousal;              /**< Expected intensity [0,1] */
    float anticipation_confidence;          /**< Confidence in forecast [0,1] */
    float approach_avoidance;               /**< +1 approach, -1 avoid */

    //-------------------------------------------------------------------------
    // Planning Relevance
    //-------------------------------------------------------------------------
    float goal_relevance;                   /**< Relevance to current goals [0,1] */
    float probability;                      /**< Estimated probability [0,1] */
    float desirability;                     /**< How desirable [0,1] */
    float controllability;                  /**< How much control [0,1] */

    //-------------------------------------------------------------------------
    // Goal Linkage
    //-------------------------------------------------------------------------
    uint64_t linked_goal_ids[FUTURE_MAX_ACTIVE_GOALS];  /**< Linked goals */
    size_t num_linked_goals;                /**< Number of linked goals */

    //-------------------------------------------------------------------------
    // Metadata
    //-------------------------------------------------------------------------
    uint64_t created_time_ms;               /**< When event was created */
    uint64_t last_simulated_ms;             /**< Last simulation time */
    uint32_t simulation_count;              /**< Times this was simulated */
    float simulation_stability;             /**< How stable across simulations */

} future_event_t;

//=============================================================================
// Type Definitions - Goals
//=============================================================================

/**
 * @brief Goal structure for goal-directed future thinking
 */
typedef struct {
    uint64_t goal_id;                       /**< Unique goal identifier */
    goal_status_t status;                   /**< Current status */

    // Goal content
    char description[FUTURE_MAX_DESCRIPTION_LEN]; /**< Goal description */
    prime_signature_t goal_signature;       /**< Goal content signature */
    nimcp_quaternion_t goal_state;          /**< Goal semantic state */

    // Priority and value
    float priority;                         /**< Priority level [0,1] */
    float expected_value;                   /**< Expected reward value */
    float current_progress;                 /**< Progress toward goal [0,1] */

    // Temporal properties
    float deadline;                         /**< Deadline (seconds from now) */
    float estimated_duration;               /**< Estimated time to complete */

    // Subgoals
    uint64_t subgoal_ids[FUTURE_MAX_SUBGOALS];  /**< Subgoal chain */
    size_t num_subgoals;                    /**< Number of subgoals */
    uint64_t parent_goal_id;                /**< Parent goal (if subgoal) */

    // Associated future events
    uint64_t* associated_events;            /**< Events relevant to this goal */
    size_t num_associated_events;           /**< Number of associated events */

    // Metadata
    uint64_t created_time_ms;               /**< When goal was created */
    uint64_t last_updated_ms;               /**< Last update time */

} future_goal_t;

//=============================================================================
// Type Definitions - Configuration
//=============================================================================

/**
 * @brief Configuration for future thinking system
 */
typedef struct {
    // Capacity settings
    size_t max_events;                      /**< Maximum stored events */
    size_t max_goals;                       /**< Maximum active goals */
    size_t fragment_pool_size;              /**< Memory fragment pool size */
    size_t max_scene_elements;              /**< Max elements per scene */

    // Sampling parameters
    size_t sample_count;                    /**< Fragments to sample per sim */
    float resonance_threshold;              /**< Min resonance for fragments */
    float diversity_weight;                 /**< Weight for diverse sampling */

    // Temporal discounting
    float discount_rate;                    /**< Hyperbolic discount rate k */
    float temporal_horizon;                 /**< Max simulation distance */

    // Coherence requirements
    float min_spatial_coherence;            /**< Minimum spatial coherence */
    float min_temporal_coherence;           /**< Minimum temporal coherence */
    float min_causal_coherence;             /**< Minimum causal coherence */

    // Emotional parameters
    float emotional_weight;                 /**< Weight for emotional salience */
    float approach_avoidance_threshold;     /**< Threshold for approach/avoid */

    // Scene construction
    uint32_t max_construction_iterations;   /**< Max iterations for scene */
    float construction_timeout_ms;          /**< Timeout for construction */

    // Resonance configuration
    resonance_config_t resonance_config;    /**< Resonance computation config */

} future_thinking_config_t;

/**
 * @brief Scenario comparison result
 */
typedef struct {
    uint64_t event_id_a;                    /**< First event */
    uint64_t event_id_b;                    /**< Second event */

    // Comparison metrics
    float content_similarity;               /**< How similar in content */
    float emotional_similarity;             /**< How similar emotionally */
    float temporal_difference;              /**< Absolute time difference */
    float probability_difference;           /**< Probability difference */
    float value_difference;                 /**< Discounted value difference */

    // Preference
    float preference_score;                 /**< Which is preferred (>0 = A) */
    char preference_reason[256];            /**< Explanation for preference */

} scenario_comparison_t;

/**
 * @brief Path optimization result
 */
typedef struct {
    uint64_t goal_id;                       /**< Target goal */

    // Optimal path
    uint64_t* step_event_ids;               /**< Events along path */
    size_t num_steps;                       /**< Number of steps */

    // Path metrics
    float total_expected_value;             /**< Total expected reward */
    float total_expected_cost;              /**< Total expected cost */
    float path_probability;                 /**< Probability of success */
    float estimated_time;                   /**< Total estimated time */

    // Alternative paths
    size_t num_alternatives;                /**< Number of alternative paths */
    float* alternative_values;              /**< Values of alternatives */

} path_optimization_result_t;

/**
 * @brief Statistics for future thinking operations
 */
typedef struct {
    // Event counts
    uint64_t total_events_created;          /**< Total events created */
    uint64_t total_events_simulated;        /**< Total simulations run */
    uint64_t current_event_count;           /**< Current stored events */

    // Goal counts
    uint64_t total_goals_created;           /**< Total goals created */
    uint64_t goals_achieved;                /**< Goals successfully achieved */
    uint64_t current_goal_count;            /**< Current active goals */

    // Simulation metrics
    float avg_simulation_time_ms;           /**< Average simulation time */
    float avg_coherence;                    /**< Average scene coherence */
    float avg_probability;                  /**< Average event probability */

    // Fragment usage
    uint64_t fragments_sampled;             /**< Total fragments sampled */
    float avg_fragments_per_sim;            /**< Avg fragments per simulation */

    // Memory usage
    size_t memory_bytes;                    /**< Approximate memory usage */

} future_thinking_stats_t;

//=============================================================================
// Main System Handle
//=============================================================================

/**
 * @brief Opaque handle to future thinking system
 */
typedef struct future_thinking_struct* future_thinking_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default future thinking configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - max_events: 256
 *         - max_goals: 16
 *         - fragment_pool_size: 128
 *         - sample_count: 8
 *         - discount_rate: 0.01
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT future_thinking_config_t future_thinking_config_default(void);

/**
 * @brief Validate future thinking configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT bool future_thinking_config_validate(
    const future_thinking_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create future thinking system
 *
 * WHAT: Allocates and initializes future thinking system
 * WHY:  Entry point for episodic future simulation capabilities
 * HOW:  Creates event storage, goal tracking, fragment pool
 *
 * @param entanglement Entanglement graph for memory access (required)
 * @param node_manager Memory node manager (required)
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * Performance: O(max_events + pool_size) initialization
 * Memory: ~2KB + event storage + pool
 *
 * Example:
 *   future_thinking_config_t cfg = future_thinking_config_default();
 *   future_thinking_t ft = future_thinking_create(
 *       entanglement, node_manager, &cfg);
 */
NIMCP_EXPORT future_thinking_t future_thinking_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const future_thinking_config_t* config
);

/**
 * @brief Destroy future thinking system
 *
 * WHAT: Deallocates all resources
 * WHY:  Clean shutdown and resource recovery
 * HOW:  Frees all events, goals, scenes, and internal structures
 *
 * @param ft System to destroy (NULL safe)
 *
 * Performance: O(num_events + num_goals)
 */
NIMCP_EXPORT void future_thinking_destroy(future_thinking_t ft);

/**
 * @brief Reset future thinking system to initial state
 *
 * WHAT: Clears all events and goals while keeping config
 * WHY:  Start fresh without reallocation
 *
 * @param ft Future thinking system
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_reset(future_thinking_t ft);

//=============================================================================
// Core Simulation Functions
//=============================================================================

/**
 * @brief Simulate a future event (main entry point)
 *
 * WHAT: Constructs mental simulation of specified future event
 * WHY:  Core episodic future thinking capability
 * HOW:  Samples fragments, recombines, evaluates coherence
 *
 * ALGORITHM:
 *   1. Retrieve relevant episodic memories (high resonance)
 *   2. Extract fragments (people, places, objects, actions)
 *   3. Recombine fragments into novel scenario
 *   4. Evaluate coherence and probability
 *   5. Compute emotional anticipation
 *
 * @param ft Future thinking system
 * @param description Event description (for query generation)
 * @param expected_time When event is expected (seconds from now)
 * @param type Type of future event
 * @param event_out Output: constructed event
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: 1-5ms typical
 *
 * Example:
 *   future_event_t event;
 *   int err = future_thinking_simulate(ft,
 *       "My graduation ceremony next spring",
 *       6 * 30 * 24 * 3600,  // ~6 months
 *       FUTURE_SPECIFIC,
 *       &event);
 */
NIMCP_EXPORT future_error_t future_thinking_simulate(
    future_thinking_t ft,
    const char* description,
    float expected_time,
    future_event_type_t type,
    future_event_t* event_out
);

/**
 * @brief Simulate with explicit context signature
 *
 * @param ft Future thinking system
 * @param context_signature Context for fragment sampling
 * @param expected_time When event is expected
 * @param type Event type
 * @param event_out Output event
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_simulate_with_context(
    future_thinking_t ft,
    const prime_signature_t* context_signature,
    float expected_time,
    future_event_type_t type,
    future_event_t* event_out
);

/**
 * @brief Re-simulate an existing event
 *
 * WHAT: Reconstructs a previously simulated event
 * WHY:  Events may change with repeated simulation (reconsolidation)
 *
 * @param ft Future thinking system
 * @param event_id Event to re-simulate
 * @param event_out Output: updated event
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_resimulate(
    future_thinking_t ft,
    uint64_t event_id,
    future_event_t* event_out
);

//=============================================================================
// Scene Construction Functions
//=============================================================================

/**
 * @brief Construct a coherent future scene
 *
 * WHAT: Builds a spatial-temporal scene from elements
 * WHY:  Scene construction is core to episodic simulation
 * HOW:  Arranges elements, checks coherence, elaborates
 *
 * @param ft Future thinking system
 * @param elements Array of scene elements
 * @param num_elements Number of elements
 * @param scene_out Output: constructed scene
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~500us
 */
NIMCP_EXPORT future_error_t future_thinking_construct_scene(
    future_thinking_t ft,
    const scene_element_t* elements,
    size_t num_elements,
    future_scene_t* scene_out
);

/**
 * @brief Elaborate scene with additional details
 *
 * WHAT: Adds detail to an existing scene
 * WHY:  Vivid scenes support better planning
 *
 * @param ft Future thinking system
 * @param scene Scene to elaborate (modified in place)
 * @param detail_level Desired detail level [0,1]
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_elaborate_scene(
    future_thinking_t ft,
    future_scene_t* scene,
    float detail_level
);

//=============================================================================
// Fragment Sampling Functions
//=============================================================================

/**
 * @brief Sample memory fragments for event construction
 *
 * WHAT: Retrieves relevant episodic memory fragments
 * WHY:  Fragments are the building blocks of future events
 * HOW:  Queries entanglement graph with context
 *
 * @param ft Future thinking system
 * @param context Context signature for sampling
 * @param max_samples Maximum fragments to sample
 * @param fragments_out Output array of fragment IDs (caller-allocated)
 * @param weights_out Output array of weights (caller-allocated)
 * @param count_out Output: number of fragments sampled
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~100us per sample
 */
NIMCP_EXPORT future_error_t future_thinking_sample_fragments(
    future_thinking_t ft,
    const prime_signature_t* context,
    size_t max_samples,
    uint64_t* fragments_out,
    float* weights_out,
    size_t* count_out
);

/**
 * @brief Combine fragments into a new scenario
 *
 * WHAT: Recombines memory fragments into novel configuration
 * WHY:  Novel future events emerge from recombination
 *
 * @param ft Future thinking system
 * @param fragment_ids Array of fragment memory IDs
 * @param weights Fragment weights
 * @param num_fragments Number of fragments
 * @param combined_signature Output: combined content signature
 * @param combined_state Output: combined emotional state
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~200us
 */
NIMCP_EXPORT future_error_t future_thinking_combine_fragments(
    future_thinking_t ft,
    const uint64_t* fragment_ids,
    const float* weights,
    size_t num_fragments,
    prime_signature_t* combined_signature,
    nimcp_quaternion_t* combined_state
);

//=============================================================================
// Evaluation Functions
//=============================================================================

/**
 * @brief Evaluate scene coherence
 *
 * WHAT: Assesses how coherent a constructed scene is
 * WHY:  Coherence indicates simulation quality
 *
 * @param ft Future thinking system
 * @param scene Scene to evaluate
 * @param spatial_out Output: spatial coherence [0,1]
 * @param temporal_out Output: temporal coherence [0,1]
 * @param causal_out Output: causal coherence [0,1]
 * @return Overall coherence [0,1] or -1 on error
 *
 * Performance: ~100us
 */
NIMCP_EXPORT float future_thinking_evaluate_coherence(
    future_thinking_t ft,
    const future_scene_t* scene,
    float* spatial_out,
    float* temporal_out,
    float* causal_out
);

/**
 * @brief Estimate probability of future event
 *
 * WHAT: Estimates how likely an event is to occur
 * WHY:  Probability affects planning and emotional response
 * HOW:  Based on past experience frequency and coherence
 *
 * @param ft Future thinking system
 * @param event Event to evaluate
 * @return Probability [0,1] or -1 on error
 *
 * Performance: ~50us
 */
NIMCP_EXPORT float future_thinking_estimate_probability(
    future_thinking_t ft,
    const future_event_t* event
);

/**
 * @brief Forecast emotional response to future event
 *
 * WHAT: Predicts emotional reaction to simulated event
 * WHY:  Emotional anticipation guides decision-making
 *
 * @param ft Future thinking system
 * @param event Event to evaluate
 * @param valence_out Output: predicted valence [-1,+1]
 * @param arousal_out Output: predicted arousal [0,1]
 * @param confidence_out Output: forecast confidence [0,1]
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~30us
 */
NIMCP_EXPORT future_error_t future_thinking_emotional_forecast(
    future_thinking_t ft,
    const future_event_t* event,
    float* valence_out,
    float* arousal_out,
    float* confidence_out
);

//=============================================================================
// Temporal Discounting Functions
//=============================================================================

/**
 * @brief Apply temporal discounting to value
 *
 * WHAT: Reduces value based on temporal distance
 * WHY:  Models psychological discounting of future rewards
 * HOW:  Hyperbolic: V(t) = V(0) / (1 + k*t)
 *
 * @param ft Future thinking system
 * @param value Immediate value V(0)
 * @param delay Delay in seconds
 * @return Discounted value V(t)
 *
 * Performance: ~5ns
 *
 * Example:
 *   float now_value = 100.0f;
 *   float year_delay = 365 * 24 * 3600;
 *   float discounted = future_thinking_temporal_discount(ft, now_value, year_delay);
 *   // With k=0.01, discounted = 100 / (1 + 0.01 * 31536000) = ~0.00003
 *   // (which is unrealistic - use smaller k in practice)
 */
NIMCP_EXPORT float future_thinking_temporal_discount(
    future_thinking_t ft,
    float value,
    float delay
);

/**
 * @brief Set discount rate
 *
 * @param ft Future thinking system
 * @param discount_rate New discount rate k
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_set_discount_rate(
    future_thinking_t ft,
    float discount_rate
);

/**
 * @brief Compute psychological distance
 *
 * WHAT: Calculates subjective "distance" to future event
 * WHY:  Psychological distance affects vividness and planning
 *
 * @param ft Future thinking system
 * @param temporal_distance Time in seconds
 * @param spatial_familiarity Familiarity with location [0,1]
 * @param social_connection Connection to participants [0,1]
 * @return Psychological distance [0,1] (0=close, 1=far)
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT float future_thinking_psychological_distance(
    future_thinking_t ft,
    float temporal_distance,
    float spatial_familiarity,
    float social_connection
);

//=============================================================================
// Goal Management Functions
//=============================================================================

/**
 * @brief Connect event to a goal
 *
 * WHAT: Links future event to an active goal
 * WHY:  Goal-relevance affects motivation and planning
 *
 * @param ft Future thinking system
 * @param event_id Event to connect
 * @param goal_id Goal to connect to
 * @param relevance Relevance strength [0,1]
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT future_error_t future_thinking_connect_to_goal(
    future_thinking_t ft,
    uint64_t event_id,
    uint64_t goal_id,
    float relevance
);

/**
 * @brief Create a new goal
 *
 * @param ft Future thinking system
 * @param description Goal description
 * @param priority Goal priority [0,1]
 * @param deadline Deadline (seconds from now)
 * @param goal_out Output: created goal
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_create_goal(
    future_thinking_t ft,
    const char* description,
    float priority,
    float deadline,
    future_goal_t* goal_out
);

/**
 * @brief Generate subgoals for a goal
 *
 * WHAT: Breaks a goal into smaller subgoals
 * WHY:  Hierarchical planning with subgoal decomposition
 *
 * @param ft Future thinking system
 * @param goal_id Parent goal ID
 * @param max_subgoals Maximum subgoals to generate
 * @param subgoals_out Output array of subgoals
 * @param count_out Output: number of subgoals generated
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~1ms
 */
NIMCP_EXPORT future_error_t future_thinking_generate_subgoals(
    future_thinking_t ft,
    uint64_t goal_id,
    size_t max_subgoals,
    future_goal_t* subgoals_out,
    size_t* count_out
);

/**
 * @brief Update goal progress
 *
 * @param ft Future thinking system
 * @param goal_id Goal to update
 * @param new_progress New progress value [0,1]
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_update_goal_progress(
    future_thinking_t ft,
    uint64_t goal_id,
    float new_progress
);

/**
 * @brief Get goal by ID
 *
 * @param ft Future thinking system
 * @param goal_id Goal ID to retrieve
 * @param goal_out Output: goal data
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_get_goal(
    future_thinking_t ft,
    uint64_t goal_id,
    future_goal_t* goal_out
);

//=============================================================================
// Comparison and Planning Functions
//=============================================================================

/**
 * @brief Compare two alternative future scenarios
 *
 * WHAT: Evaluates two possible futures against each other
 * WHY:  Decision-making requires comparison of alternatives
 *
 * @param ft Future thinking system
 * @param event_id_a First scenario
 * @param event_id_b Second scenario
 * @param comparison_out Output: comparison result
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~100us
 */
NIMCP_EXPORT future_error_t future_thinking_compare_scenarios(
    future_thinking_t ft,
    uint64_t event_id_a,
    uint64_t event_id_b,
    scenario_comparison_t* comparison_out
);

/**
 * @brief Find optimal path to goal
 *
 * WHAT: Determines best sequence of events to achieve goal
 * WHY:  Planning requires identifying optimal action sequences
 *
 * @param ft Future thinking system
 * @param goal_id Target goal
 * @param max_steps Maximum path length
 * @param result_out Output: optimization result
 * @return FUTURE_SUCCESS or error code
 *
 * Performance: ~5ms
 */
NIMCP_EXPORT future_error_t future_thinking_optimize_path(
    future_thinking_t ft,
    uint64_t goal_id,
    size_t max_steps,
    path_optimization_result_t* result_out
);

/**
 * @brief Free path optimization result resources
 *
 * @param result Result to free
 */
NIMCP_EXPORT void future_thinking_free_path_result(
    path_optimization_result_t* result
);

//=============================================================================
// Event Management Functions
//=============================================================================

/**
 * @brief Store a simulated event
 *
 * @param ft Future thinking system
 * @param event Event to store
 * @return Event ID or FUTURE_INVALID_EVENT_ID on error
 */
NIMCP_EXPORT uint64_t future_thinking_store_event(
    future_thinking_t ft,
    const future_event_t* event
);

/**
 * @brief Retrieve stored event by ID
 *
 * @param ft Future thinking system
 * @param event_id Event ID
 * @param event_out Output: event data
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_get_event(
    future_thinking_t ft,
    uint64_t event_id,
    future_event_t* event_out
);

/**
 * @brief Delete stored event
 *
 * @param ft Future thinking system
 * @param event_id Event to delete
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_delete_event(
    future_thinking_t ft,
    uint64_t event_id
);

/**
 * @brief Query events by time range
 *
 * @param ft Future thinking system
 * @param min_time Minimum expected time (seconds from now)
 * @param max_time Maximum expected time
 * @param events_out Output array (caller-allocated)
 * @param max_events Maximum to return
 * @param count_out Output: actual count
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_query_events_by_time(
    future_thinking_t ft,
    float min_time,
    float max_time,
    future_event_t* events_out,
    size_t max_events,
    size_t* count_out
);

/**
 * @brief Query events by goal relevance
 *
 * @param ft Future thinking system
 * @param goal_id Goal to query for
 * @param min_relevance Minimum relevance threshold
 * @param events_out Output array
 * @param max_events Maximum to return
 * @param count_out Output: actual count
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_query_events_by_goal(
    future_thinking_t ft,
    uint64_t goal_id,
    float min_relevance,
    future_event_t* events_out,
    size_t max_events,
    size_t* count_out
);

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

/**
 * @brief Get future thinking statistics
 *
 * @param ft Future thinking system
 * @param stats_out Output: statistics
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_thinking_get_stats(
    future_thinking_t ft,
    future_thinking_stats_t* stats_out
);

/**
 * @brief Reset statistics counters
 *
 * @param ft Future thinking system
 */
NIMCP_EXPORT void future_thinking_reset_stats(future_thinking_t ft);

/**
 * @brief Get current simulation status
 *
 * @param ft Future thinking system
 * @return Current simulation status
 */
NIMCP_EXPORT future_sim_status_t future_thinking_get_status(
    future_thinking_t ft
);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* future_error_string(future_error_t error);

/**
 * @brief Get last error message (thread-local)
 *
 * @return Error string, or NULL if no error
 *
 * Thread safety: Error messages are thread-local
 */
NIMCP_EXPORT const char* future_thinking_get_last_error(void);

/**
 * @brief Get event type name
 *
 * @param type Event type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* future_event_type_name(future_event_type_t type);

/**
 * @brief Get goal status name
 *
 * @param status Goal status
 * @return Human-readable status name
 */
NIMCP_EXPORT const char* future_goal_status_name(goal_status_t status);

//=============================================================================
// Scene Element Functions
//=============================================================================

/**
 * @brief Initialize a scene element
 *
 * @param element Element to initialize
 * @param type Element type
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t scene_element_init(
    scene_element_t* element,
    scene_element_type_t type
);

/**
 * @brief Initialize a future scene
 *
 * @param scene Scene to initialize
 * @param max_elements Maximum elements capacity
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_scene_init(
    future_scene_t* scene,
    size_t max_elements
);

/**
 * @brief Free future scene resources
 *
 * @param scene Scene to free
 */
NIMCP_EXPORT void future_scene_free(future_scene_t* scene);

/**
 * @brief Add element to scene
 *
 * @param scene Scene to add to
 * @param element Element to add (copied)
 * @return Element ID or FUTURE_INVALID_EVENT_ID on error
 */
NIMCP_EXPORT uint64_t future_scene_add_element(
    future_scene_t* scene,
    const scene_element_t* element
);

/**
 * @brief Get scene element type name
 *
 * @param type Element type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* scene_element_type_name(scene_element_type_t type);

//=============================================================================
// Event Initialization and Cleanup
//=============================================================================

/**
 * @brief Initialize a future event structure
 *
 * @param event Event to initialize
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_event_init(future_event_t* event);

/**
 * @brief Free future event internal resources
 *
 * Note: Does not free the event struct itself.
 *
 * @param event Event to clean up
 */
NIMCP_EXPORT void future_event_cleanup(future_event_t* event);

/**
 * @brief Initialize a goal structure
 *
 * @param goal Goal to initialize
 * @return FUTURE_SUCCESS or error code
 */
NIMCP_EXPORT future_error_t future_goal_init(future_goal_t* goal);

/**
 * @brief Free goal internal resources
 *
 * @param goal Goal to clean up
 */
NIMCP_EXPORT void future_goal_cleanup(future_goal_t* goal);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FUTURE_THINKING_H
