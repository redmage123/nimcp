/**
 * @file nimcp_occipital_quantum_bridge.h
 * @brief Quantum-inspired visual processing optimization
 *
 * WHAT: Integrates quantum algorithms with Occipital Cortex
 * WHY: Explore multiple visual interpretations simultaneously for optimal perception
 * HOW: Quantum reasoning for visual search, feature binding, scene segmentation
 *
 * BIOLOGICAL INSPIRATION:
 * - Visual cortex explores multiple scene interpretations in parallel
 * - Binding problem: how features combine into coherent objects
 * - Visual search: finding targets among distractors
 * - Figure-ground segregation requires global integration
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Explore multiple scene interpretations simultaneously
 * - Grover search: Find visual target in O(sqrt(N)) instead of O(N)
 * - Interference: Cancel inconsistent feature bindings
 * - Amplitude amplification: Boost salient visual elements
 *
 * APPLICATIONS:
 * - Visual search: Find target among distractors (conjunction search)
 * - Feature binding: Group features into coherent objects
 * - Scene segmentation: Optimal figure-ground assignment
 * - Object recognition: Parallel template matching
 * - Motion integration: Combine local motions into global flow
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_OCCIPITAL_QUANTUM_BRIDGE_H
#define NIMCP_OCCIPITAL_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * TYPES
 *===========================================================================*/

typedef struct occipital_quantum_bridge occipital_quantum_bridge_t;

/**
 * @brief Quantum Occipital configuration
 */
typedef struct {
    bool enabled;                    /**< Enable quantum optimization */
    uint32_t visual_search_depth;    /**< Max visual field search depth (default: 1024) */
    uint32_t binding_alternatives;   /**< Max parallel feature bindings (default: 16) */
    uint32_t max_grover_iterations;  /**< Max Grover iterations (default: 15) */
    float min_detection_confidence;  /**< Min confidence for detection (default: 0.5) */
    bool enable_interference;        /**< Enable quantum interference (default: true) */
    bool use_superposition;          /**< Use superposition for alternatives (default: true) */
    uint32_t seed;                   /**< Random seed (default: 42) */
} occipital_quantum_config_t;

/**
 * @brief Visual search target specification
 */
typedef struct {
    uint32_t target_id;              /**< Target identifier */
    float target_color_h;            /**< Target hue (0-360) */
    float target_color_s;            /**< Target saturation (0-1) */
    float target_orientation;        /**< Target orientation (radians) */
    float target_size;               /**< Target size (normalized) */
    bool search_by_color;            /**< Search by color feature */
    bool search_by_orientation;      /**< Search by orientation feature */
    bool search_by_size;             /**< Search by size feature */
    bool conjunction_search;         /**< Require multiple features (harder) */
} visual_search_target_t;

/**
 * @brief Visual search candidate from quantum search
 */
typedef struct {
    uint32_t location_id;            /**< Location identifier */
    float x;                         /**< X position (normalized 0-1) */
    float y;                         /**< Y position (normalized 0-1) */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float feature_match;             /**< Feature similarity [0, 1] */
    float saliency;                  /**< Bottom-up saliency [0, 1] */
    float combined_score;            /**< Combined search score */
    bool is_target;                  /**< True if classified as target */
} quantum_search_candidate_t;

/**
 * @brief Visual search result
 */
typedef struct {
    quantum_search_candidate_t* best_candidate; /**< Best location found */
    uint32_t locations_evaluated;               /**< Total locations searched */
    float satisfaction_probability;              /**< Search success probability */
    uint32_t grover_iterations_used;            /**< Grover iterations */
    float search_speedup;                        /**< Speedup vs linear search */
    bool target_found;                           /**< Target detected */
} quantum_search_result_t;

/**
 * @brief Feature binding hypothesis
 */
typedef struct {
    uint32_t binding_id;             /**< Binding identifier */
    uint32_t* feature_ids;           /**< Array of bound feature IDs */
    uint32_t num_features;           /**< Number of features in binding */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float coherence;                 /**< Spatial/temporal coherence [0, 1] */
    float probability;               /**< Binding probability */
    float x_centroid;                /**< Object centroid X */
    float y_centroid;                /**< Object centroid Y */
} quantum_binding_hypothesis_t;

/**
 * @brief Feature binding result
 */
typedef struct {
    quantum_binding_hypothesis_t* best_binding; /**< Most likely binding */
    uint32_t hypotheses_evaluated;              /**< Total hypotheses */
    float satisfaction_probability;              /**< Solution confidence */
    uint32_t num_objects;                        /**< Number of bound objects */
} quantum_binding_result_t;

/**
 * @brief Scene segmentation candidate
 */
typedef struct {
    uint32_t segment_id;             /**< Segment identifier */
    float* segment_mask;             /**< Binary mask (normalized) */
    uint32_t mask_width;             /**< Mask width */
    uint32_t mask_height;            /**< Mask height */
    float amplitude;                 /**< Quantum amplitude */
    float boundary_cost;             /**< Boundary smoothness cost */
    float region_homogeneity;        /**< Region homogeneity score */
    bool is_figure;                  /**< True if figure, false if ground */
} quantum_segment_candidate_t;

/**
 * @brief Scene segmentation result
 */
typedef struct {
    quantum_segment_candidate_t* best_segmentation; /**< Best figure-ground */
    uint32_t segmentations_evaluated;               /**< Total evaluated */
    float optimization_score;                        /**< Segmentation quality */
} quantum_segmentation_result_t;

/**
 * @brief Motion integration candidate
 */
typedef struct {
    uint32_t integration_id;         /**< Integration identifier */
    float global_dx;                 /**< Global horizontal flow */
    float global_dy;                 /**< Global vertical flow */
    float amplitude;                 /**< Quantum amplitude */
    float coherence;                 /**< Motion coherence [0, 1] */
    float residual_error;            /**< Local motion residual */
} quantum_motion_candidate_t;

/**
 * @brief Motion integration result
 */
typedef struct {
    quantum_motion_candidate_t* best_integration; /**< Best global motion */
    uint32_t integrations_evaluated;              /**< Total evaluated */
    float coherence_score;                         /**< Motion coherence */
} quantum_motion_result_t;

/**
 * @brief Statistics for quantum occipital operations
 */
typedef struct {
    uint64_t visual_searches;        /**< Total visual searches */
    uint64_t binding_operations;     /**< Total binding operations */
    uint64_t segmentation_operations; /**< Total segmentations */
    uint64_t motion_integrations;    /**< Total motion integrations */
    float avg_search_speedup;        /**< Average search speedup */
    float avg_binding_coherence;     /**< Average binding coherence */
    float avg_satisfaction_prob;     /**< Average success probability */
    uint64_t successful_searches;    /**< Searches with high confidence */
    uint64_t failed_searches;        /**< Searches with low confidence */
} occipital_quantum_stats_t;

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

/**
 * @brief Get default quantum occipital configuration
 * @return Default configuration
 */
occipital_quantum_config_t occipital_quantum_default_config(void);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Create quantum occipital bridge
 * @param occipital Occipital adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
occipital_quantum_bridge_t* occipital_quantum_bridge_create(
    void* occipital,
    const occipital_quantum_config_t* config
);

/**
 * @brief Destroy quantum occipital bridge
 * @param bridge Bridge to destroy
 */
void occipital_quantum_bridge_destroy(occipital_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool occipital_quantum_bridge_is_enabled(const occipital_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void occipital_quantum_bridge_set_enabled(occipital_quantum_bridge_t* bridge, bool enabled);

/*=============================================================================
 * VISUAL SEARCH API
 *===========================================================================*/

/**
 * @brief Search visual field using quantum Grover algorithm
 * @param bridge Quantum bridge
 * @param target Target specification
 * @param num_locations Number of locations to search
 * @param result Output: search result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) linear search
 *
 * BIOLOGICAL BASIS:
 * - Pop-out: Simple features found in parallel (quantum superposition)
 * - Conjunction search: Multiple features require serial search (Grover speedup)
 * - Attention: Quantum amplitude represents attentional allocation
 */
int occipital_quantum_visual_search(
    occipital_quantum_bridge_t* bridge,
    const visual_search_target_t* target,
    uint32_t num_locations,
    quantum_search_result_t* result
);

/*=============================================================================
 * FEATURE BINDING API
 *===========================================================================*/

/**
 * @brief Solve feature binding problem using quantum optimization
 * @param bridge Quantum bridge
 * @param feature_locations Feature location array (x,y pairs)
 * @param feature_types Feature type array
 * @param num_features Number of features
 * @param result Output: binding result
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Binding problem: How does the brain combine features into objects?
 * - Quantum superposition explores all possible bindings
 * - Coherence constraint collapses to consistent interpretation
 * - Gamma-band synchrony may implement binding (40Hz coherence)
 */
int occipital_quantum_feature_binding(
    occipital_quantum_bridge_t* bridge,
    const float* feature_locations,
    const uint32_t* feature_types,
    uint32_t num_features,
    quantum_binding_result_t* result
);

/*=============================================================================
 * SCENE SEGMENTATION API
 *===========================================================================*/

/**
 * @brief Optimize scene segmentation using quantum search
 * @param bridge Quantum bridge
 * @param edge_map Edge strength map (width x height)
 * @param width Image width
 * @param height Image height
 * @param result Output: segmentation result
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Figure-ground segregation is crucial for object recognition
 * - V2 neurons show border ownership selectivity
 * - Quantum search finds optimal boundary assignment
 */
int occipital_quantum_segment_scene(
    occipital_quantum_bridge_t* bridge,
    const float* edge_map,
    uint32_t width,
    uint32_t height,
    quantum_segmentation_result_t* result
);

/*=============================================================================
 * MOTION INTEGRATION API
 *===========================================================================*/

/**
 * @brief Integrate local motions into global flow using quantum optimization
 * @param bridge Quantum bridge
 * @param local_dx Local horizontal motion array
 * @param local_dy Local vertical motion array
 * @param num_motions Number of local motion vectors
 * @param result Output: integration result
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - V5/MT integrates local motion signals into global optic flow
 * - Aperture problem: Local motion is ambiguous
 * - Intersection of constraints resolves ambiguity
 * - Quantum superposition explores all consistent interpretations
 */
int occipital_quantum_integrate_motion(
    occipital_quantum_bridge_t* bridge,
    const float* local_dx,
    const float* local_dy,
    uint32_t num_motions,
    quantum_motion_result_t* result
);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get quantum occipital statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int occipital_quantum_get_stats(
    const occipital_quantum_bridge_t* bridge,
    occipital_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void occipital_quantum_reset_stats(occipital_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int occipital_quantum_get_config(
    const occipital_quantum_bridge_t* bridge,
    occipital_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_QUANTUM_BRIDGE_H */
