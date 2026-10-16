/**
 * @file nimcp_occipital_logic_bridge.h
 * @brief Bridge between Occipital Cortex and Neural Logic Gates
 *
 * WHAT: Connects visual perception to neural logic for visual reasoning
 * WHY: Enable perception-to-logic conversion (visual predicates, scene logic)
 * HOW: Extracts visual features and converts them to logical propositions
 *
 * BIOLOGICAL BASIS:
 * - Higher visual areas (IT, PFC) perform categorical visual reasoning
 * - Visual predicates: "object is red", "above", "occluded", "moving"
 * - Scene understanding requires logical inference over visual features
 * - Parietal cortex integrates spatial logic ("A is left of B")
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                   OCCIPITAL-LOGIC BRIDGE                                │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  occipital_adapter_t              neural_logic_network_t                │
 * │  ┌─────────────────┐             ┌────────────────────────┐            │
 * │  │ Visual Features │────────────▶│ Visual Predicates      │            │
 * │  │ V4 Objects      │  Grounding  │ IS_RED(x), ABOVE(x,y)  │            │
 * │  │ V5 Motion       │             │ MOVING(x), OCCLUDED(x) │            │
 * │  └─────────────────┘             └────────────────────────┘            │
 * │         │                                  │                            │
 * │         │                                  ▼                            │
 * │         │              ┌─────────────────────────────────┐             │
 * │         └─────────────▶│ Logic Circuit Builder           │             │
 * │                        │ Scene Graph → Logic Graph       │             │
 * │                        │ Spatial Relations → Predicates  │             │
 * │                        └─────────────────────────────────┘             │
 * │                                    │                                    │
 * │                                    ▼                                    │
 * │                        ┌─────────────────────────────────┐             │
 * │                        │ brain_neural_logic_evaluate()   │             │
 * │                        │ Neuromodulated reasoning        │             │
 * │                        └─────────────────────────────────┘             │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * VISUAL PREDICATES:
 * - OBJECT_PRESENT(id): Object detected in scene
 * - HAS_COLOR(id, color): Object has specified color
 * - HAS_SHAPE(id, shape): Object has specified shape
 * - IS_MOVING(id): Object motion detected
 * - ABOVE(a, b): Spatial relation "a above b"
 * - LEFT_OF(a, b): Spatial relation "a left of b"
 * - OCCLUDES(a, b): Object a occludes object b
 * - SAME_CATEGORY(a, b): Objects in same category
 *
 * @version Phase O1: Occipital Logic Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_OCCIPITAL_LOGIC_BRIDGE_H
#define NIMCP_OCCIPITAL_LOGIC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct occipital_adapter occipital_adapter_t;
typedef struct neural_logic_network neural_logic_network_t;
typedef struct brain_struct* brain_t;

/* Forward declare bio_router_struct for bio-async (defined in nimcp_bio_router.h) */
struct bio_router_struct;

/* Opaque bridge type */
typedef struct occipital_logic_bridge occipital_logic_bridge_t;

/*=============================================================================
 * Visual Predicate Definitions
 *===========================================================================*/

/**
 * @brief Visual predicate types
 */
typedef enum {
    /* Unary predicates (single object) */
    PRED_OBJECT_PRESENT = 0,     /**< Object exists in scene */
    PRED_IS_MOVING,              /**< Object is moving */
    PRED_IS_OCCLUDED,            /**< Object is partially occluded */
    PRED_IN_FOVEA,               /**< Object in foveal region */
    PRED_HAS_COLOR,              /**< Object has specific color */
    PRED_HAS_SHAPE,              /**< Object has specific shape */
    PRED_IS_SALIENT,             /**< Object is visually salient */

    /* Binary predicates (two objects) */
    PRED_ABOVE = 32,             /**< A is above B */
    PRED_BELOW,                  /**< A is below B */
    PRED_LEFT_OF,                /**< A is left of B */
    PRED_RIGHT_OF,               /**< A is right of B */
    PRED_OCCLUDES,               /**< A occludes B */
    PRED_NEAR,                   /**< A is near B */
    PRED_SAME_CATEGORY,          /**< A and B same category */
    PRED_SAME_COLOR,             /**< A and B same color */
    PRED_MOVING_TOWARD,          /**< A moving toward B */

    /* Scene-level predicates */
    PRED_SCENE_CROWDED = 64,     /**< Scene is crowded */
    PRED_SCENE_MOVING,           /**< Scene has motion */
    PRED_SCENE_COHERENT,         /**< Scene is coherent/interpretable */

    PRED_COUNT
} visual_predicate_type_t;

/**
 * @brief Visual predicate instance
 */
typedef struct {
    visual_predicate_type_t type; /**< Predicate type */
    uint32_t object_a;            /**< First object ID (or scene ID) */
    uint32_t object_b;            /**< Second object ID (for binary) */
    uint32_t parameter;           /**< Additional parameter (color/shape ID) */
    float confidence;             /**< Detection confidence [0-1] */
    float truth_value;            /**< Fuzzy truth value [0-1] */
    uint64_t timestamp_us;        /**< When predicate was evaluated */
} visual_predicate_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Logic integration mode
 */
typedef enum {
    OCC_LOGIC_MODE_GROUND_ONLY = 0,  /**< Only ground predicates, no inference */
    OCC_LOGIC_MODE_FORWARD_CHAIN,     /**< Forward chaining inference */
    OCC_LOGIC_MODE_BACKWARD_CHAIN,    /**< Backward chaining (goal-directed) */
    OCC_LOGIC_MODE_HYBRID             /**< Both forward and backward */
} occipital_logic_mode_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Predicate extraction */
    bool enable_unary_predicates;     /**< Enable object predicates */
    bool enable_binary_predicates;    /**< Enable relational predicates */
    bool enable_scene_predicates;     /**< Enable scene-level predicates */

    /* Thresholds */
    float detection_threshold;        /**< Min detection confidence */
    float truth_threshold;            /**< Min truth value for assertion */
    float spatial_threshold;          /**< Spatial relation threshold (pixels) */

    /* Logic settings */
    occipital_logic_mode_t mode;      /**< Inference mode */
    uint32_t max_predicates;          /**< Maximum active predicates */
    uint32_t max_inference_depth;     /**< Max inference chain depth */
    bool use_fuzzy_logic;             /**< Use fuzzy truth values */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
} occipital_logic_config_t;

/**
 * @brief Inference result
 */
typedef struct {
    visual_predicate_t conclusion;    /**< Inferred predicate */
    uint32_t premise_count;           /**< Number of premises used */
    uint32_t premise_ids[8];          /**< IDs of premises */
    float inference_confidence;        /**< Overall confidence */
    uint32_t inference_depth;          /**< Depth in inference chain */
} logic_inference_result_t;

/**
 * @brief Bridge effects on processing
 */
typedef struct {
    /* Predicate grounding */
    uint32_t active_predicates;       /**< Currently active predicates */
    float avg_predicate_confidence;   /**< Average confidence */
    float avg_truth_value;            /**< Average truth value */

    /* Inference */
    uint32_t inferences_made;         /**< Inferences this frame */
    float inference_quality;          /**< Quality of inferences */

    /* Logic network effects */
    float logic_network_load;         /**< Logic network utilization */
    float reasoning_coherence;        /**< Coherence of conclusions */
} occipital_logic_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t predicates_grounded;     /**< Total predicates grounded */
    uint64_t predicates_asserted;     /**< Predicates asserted to logic */
    uint64_t inferences_performed;    /**< Total inferences */
    uint64_t queries_processed;       /**< Logic queries processed */
    float avg_inference_depth;        /**< Average inference depth */
    float avg_query_time_us;          /**< Average query time */
    uint64_t messages_sent;           /**< Bio-async messages sent */
    uint64_t messages_received;       /**< Bio-async messages received */
} occipital_logic_stats_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
occipital_logic_config_t occipital_logic_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create logic bridge
 *
 * @param occipital Occipital adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
occipital_logic_bridge_t* occipital_logic_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_logic_config_t* config);

/**
 * @brief Destroy logic bridge
 */
void occipital_logic_bridge_destroy(occipital_logic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int occipital_logic_bridge_reset(occipital_logic_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to brain's neural logic network
 */
int occipital_logic_connect_brain(
    occipital_logic_bridge_t* bridge,
    brain_t brain);

/**
 * @brief Connect directly to neural logic network
 */
int occipital_logic_connect_network(
    occipital_logic_bridge_t* bridge,
    neural_logic_network_t* network);

/**
 * @brief Register with bio-async router
 */
int occipital_logic_bridge_register_bio_async(
    occipital_logic_bridge_t* bridge,
    struct bio_router_struct* router);

/*=============================================================================
 * Predicate API
 *===========================================================================*/

/**
 * @brief Ground visual predicates from current scene
 *
 * Extracts predicates from visual features and prepares them
 * for logical inference.
 *
 * @param bridge Bridge instance
 * @return Number of predicates grounded, -1 on error
 */
int occipital_logic_ground_predicates(occipital_logic_bridge_t* bridge);

/**
 * @brief Assert predicate to logic network
 */
int occipital_logic_assert_predicate(
    occipital_logic_bridge_t* bridge,
    const visual_predicate_t* predicate);

/**
 * @brief Query predicate truth value
 */
int occipital_logic_query_predicate(
    occipital_logic_bridge_t* bridge,
    visual_predicate_type_t type,
    uint32_t object_a,
    uint32_t object_b,
    float* truth_value,
    float* confidence);

/**
 * @brief Get all active predicates
 */
int occipital_logic_get_predicates(
    const occipital_logic_bridge_t* bridge,
    visual_predicate_t* predicates,
    uint32_t max_predicates,
    uint32_t* count);

/*=============================================================================
 * Inference API
 *===========================================================================*/

/**
 * @brief Run inference on current predicates
 *
 * @param bridge Bridge instance
 * @return Number of new inferences, -1 on error
 */
int occipital_logic_run_inference(occipital_logic_bridge_t* bridge);

/**
 * @brief Get inference results
 */
int occipital_logic_get_inferences(
    const occipital_logic_bridge_t* bridge,
    logic_inference_result_t* results,
    uint32_t max_results,
    uint32_t* count);

/**
 * @brief Query with backward chaining
 *
 * @param bridge Bridge instance
 * @param goal Goal predicate to prove
 * @param provable Output: true if goal can be proven
 * @param confidence Output: confidence in proof
 * @return 0 on success, -1 on failure
 */
int occipital_logic_prove_goal(
    occipital_logic_bridge_t* bridge,
    const visual_predicate_t* goal,
    bool* provable,
    float* confidence);

/*=============================================================================
 * Processing API
 *===========================================================================*/

/**
 * @brief Update bridge state
 */
int occipital_logic_bridge_update(occipital_logic_bridge_t* bridge);

/**
 * @brief Get current effects
 */
int occipital_logic_bridge_get_effects(
    const occipital_logic_bridge_t* bridge,
    occipital_logic_effects_t* effects);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 */
int occipital_logic_bridge_get_stats(
    const occipital_logic_bridge_t* bridge,
    occipital_logic_stats_t* stats);

/**
 * @brief Reset statistics
 */
void occipital_logic_bridge_reset_stats(occipital_logic_bridge_t* bridge);

/**
 * @brief Check if brain connected
 */
bool occipital_logic_is_brain_connected(const occipital_logic_bridge_t* bridge);

/**
 * @brief Get configuration
 */
int occipital_logic_bridge_get_config(
    const occipital_logic_bridge_t* bridge,
    occipital_logic_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_LOGIC_BRIDGE_H */
