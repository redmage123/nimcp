/**
 * @file nimcp_parietal_quantum_bridge.h
 * @brief Quantum-inspired spatial reasoning optimization
 *
 * WHAT: Integrates quantum algorithms with parietal cortex functions
 * WHY: Explore multiple spatial configurations simultaneously for optimal processing
 * HOW: Quantum reasoning for attention allocation, coordinate transforms, motor planning
 *
 * BIOLOGICAL INSPIRATION:
 * - Parietal cortex maintains distributed spatial representations
 * - Attention can shift rapidly across spatial locations
 * - Multiple reaching trajectories are evaluated in parallel
 * - Coordinate transforms occur across reference frames simultaneously
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Multiple attention foci exist simultaneously until collapsed
 * - Grover search: Find optimal spatial target in O(sqrt(N))
 * - Quantum walk: Navigate spatial map efficiently
 * - Entanglement: Link spatial representations across reference frames
 * - Amplitude amplification: Boost high-salience spatial locations
 *
 * APPLICATIONS:
 * - Spatial attention: Quantum parallel search for salient locations
 * - Coordinate transforms: Superposition across reference frames
 * - Motor planning: Explore trajectory space efficiently
 * - Navigation: Quantum walk for path finding
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_PARIETAL_QUANTUM_BRIDGE_H
#define NIMCP_PARIETAL_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * TYPES
 *===========================================================================*/

typedef struct parietal_quantum_bridge parietal_quantum_bridge_t;

/**
 * @brief Quantum parietal region configuration
 * NOTE: Different from cognitive/parietal version - this is for spatial attention
 */
typedef struct parietal_region_quantum_config_s {
    bool enabled;                    /**< Enable quantum optimization */
    uint32_t spatial_grid_size;      /**< Spatial attention grid size (default: 64) */
    uint32_t max_targets;            /**< Maximum spatial targets (default: 128) */
    uint32_t max_grover_iterations;  /**< Max Grover iterations (default: 10) */
    float min_salience_threshold;    /**< Min salience for target (default: 0.1) */
    bool enable_superposition;       /**< Enable attention superposition (default: true) */
    bool enable_quantum_walk;        /**< Enable quantum spatial walk (default: true) */
    bool enable_entanglement;        /**< Enable frame entanglement (default: true) */
    uint32_t seed;                   /**< Random seed (default: 42) */
} parietal_region_quantum_config_t;

/**
 * @brief Spatial attention candidate from quantum search
 */
typedef struct {
    uint32_t location_id;            /**< Location identifier */
    parietal_cortex_position_t position;    /**< Spatial position */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float salience;                  /**< Location salience [0, 1] */
    float distance_from_focus;       /**< Distance from current focus */
    float combined_score;            /**< Combined selection score */
} quantum_spatial_candidate_t;

/**
 * @brief Quantum spatial search result
 */
typedef struct {
    quantum_spatial_candidate_t* best_location; /**< Best location found */
    uint32_t locations_evaluated;               /**< Total locations */
    float satisfaction_probability;              /**< Search success probability */
    uint32_t grover_iterations_used;            /**< Grover iterations */
    float search_speedup;                        /**< Speedup vs linear search */
} quantum_spatial_result_t;

/**
 * @brief Trajectory candidate for motor planning
 */
typedef struct {
    uint32_t trajectory_id;          /**< Trajectory identifier */
    parietal_cortex_position_t start;       /**< Start position */
    parietal_cortex_position_t end;         /**< End position */
    parietal_cortex_position_t waypoints[8];/**< Intermediate waypoints */
    uint32_t waypoint_count;         /**< Number of waypoints */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float path_length;               /**< Total path length */
    float energy_cost;               /**< Energy cost estimate */
    float collision_risk;            /**< Obstacle collision risk [0, 1] */
    float smoothness;                /**< Trajectory smoothness [0, 1] */
} quantum_trajectory_candidate_t;

/**
 * @brief Quantum trajectory optimization result
 */
typedef struct {
    quantum_trajectory_candidate_t* best_trajectory; /**< Best trajectory */
    uint32_t trajectories_evaluated;                 /**< Total trajectories */
    float optimization_score;                         /**< Overall quality */
    uint32_t grover_iterations_used;                 /**< Grover iterations */
} quantum_trajectory_result_t;

/**
 * @brief Quantum coordinate transform state
 */
typedef struct {
    parietal_cortex_spatial_frame_t frame;  /**< Reference frame */
    float amplitude;                 /**< Superposition amplitude [0, 1] */
    parietal_cortex_position_t position;    /**< Position in this frame */
} quantum_frame_state_t;

/**
 * @brief Quantum superposition of reference frames
 */
typedef struct {
    quantum_frame_state_t states[PARIETAL_CORTEX_SPATIAL_FRAME_COUNT]; /**< Frame states */
    uint32_t active_frames;                            /**< Number of active frames */
    parietal_cortex_spatial_frame_t collapsed_frame;          /**< Collapsed frame (-1 if not) */
    float total_amplitude;                             /**< Sum of amplitudes */
} quantum_frame_superposition_t;

/**
 * @brief Statistics for quantum parietal operations
 */
typedef struct {
    uint64_t spatial_searches;       /**< Total spatial searches */
    uint64_t trajectory_optimizations; /**< Total trajectory optimizations */
    uint64_t frame_transforms;       /**< Total frame transforms */
    uint64_t quantum_walks;          /**< Total quantum walks */
    float avg_spatial_speedup;       /**< Average spatial search speedup */
    float avg_trajectory_speedup;    /**< Average trajectory optimization speedup */
    float avg_satisfaction_prob;     /**< Average success probability */
    uint64_t successful_searches;    /**< Searches with high confidence */
    uint64_t failed_searches;        /**< Searches with low confidence */
} parietal_quantum_stats_t;

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

/**
 * @brief Get default quantum parietal configuration
 * @return Default configuration
 */
parietal_region_quantum_config_t parietal_region_quantum_default_config(void);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Create quantum parietal bridge
 * @param parietal Parietal adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
parietal_quantum_bridge_t* parietal_region_quantum_bridge_create(
    parietal_adapter_t* parietal,
    const parietal_region_quantum_config_t* config
);

/**
 * @brief Destroy quantum parietal bridge
 * @param bridge Bridge to destroy
 */
void parietal_region_quantum_bridge_destroy(parietal_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool parietal_region_quantum_bridge_is_enabled(const parietal_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void parietal_region_quantum_bridge_set_enabled(parietal_quantum_bridge_t* bridge, bool enabled);

/**
 * @brief Reset bridge state
 * @param bridge Quantum bridge
 * @return 0 on success, -1 on error
 */
int parietal_region_quantum_bridge_reset(parietal_quantum_bridge_t* bridge);

/*=============================================================================
 * SPATIAL ATTENTION API
 *===========================================================================*/

/**
 * @brief Search for salient spatial location using quantum Grover
 *
 * WHAT: Find most salient location in spatial attention map
 * WHY: Enable rapid attention allocation across visual field
 * HOW: Use Grover search over spatial grid with salience oracle
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) linear search
 *
 * @param bridge Quantum bridge
 * @param salience_map Salience values for spatial grid (array of floats)
 * @param grid_size Size of salience grid (N = grid_size^2)
 * @param result Output: search result
 * @return 0 on success, -1 on error
 */
int parietal_quantum_search_spatial(
    parietal_quantum_bridge_t* bridge,
    const float* salience_map,
    uint32_t grid_size,
    quantum_spatial_result_t* result
);

/**
 * @brief Create attention superposition across multiple targets
 *
 * WHAT: Maintain quantum superposition of attention across targets
 * WHY: Enable split attention and parallel target tracking
 * HOW: Create amplitude-weighted superposition of target locations
 *
 * @param bridge Quantum bridge
 * @param targets Array of spatial targets
 * @param num_targets Number of targets
 * @param amplitudes Amplitude weights for each target
 * @return 0 on success, -1 on error
 */
int parietal_quantum_superpose_attention(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_spatial_target_t* targets,
    uint32_t num_targets,
    const float* amplitudes
);

/**
 * @brief Collapse attention superposition to single target
 *
 * WHAT: Collapse attention to highest amplitude target
 * WHY: Select single focus for action
 * HOW: Measure superposition and collapse to dominant amplitude
 *
 * @param bridge Quantum bridge
 * @param selected_target Output: selected target location
 * @return 0 on success, -1 on error
 */
int parietal_quantum_collapse_attention(
    parietal_quantum_bridge_t* bridge,
    parietal_cortex_spatial_target_t* selected_target
);

/*=============================================================================
 * COORDINATE TRANSFORM API
 *===========================================================================*/

/**
 * @brief Create frame superposition for coordinate transform
 *
 * WHAT: Place position in superposition across reference frames
 * WHY: Enable parallel coordinate processing
 * HOW: Create quantum state spanning multiple frames
 *
 * @param bridge Quantum bridge
 * @param position Position in base frame
 * @param base_frame Base reference frame
 * @param superposition Output: frame superposition
 * @return 0 on success, -1 on error
 */
int parietal_quantum_frame_superposition(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* position,
    parietal_cortex_spatial_frame_t base_frame,
    quantum_frame_superposition_t* superposition
);

/**
 * @brief Collapse frame superposition to target frame
 *
 * WHAT: Extract position in specific reference frame
 * WHY: Get coordinate in required frame for action
 * HOW: Collapse superposition to target frame
 *
 * @param bridge Quantum bridge
 * @param superposition Input frame superposition
 * @param target_frame Frame to collapse to
 * @param result Output: position in target frame
 * @return 0 on success, -1 on error
 */
int parietal_quantum_collapse_frame(
    parietal_quantum_bridge_t* bridge,
    const quantum_frame_superposition_t* superposition,
    parietal_cortex_spatial_frame_t target_frame,
    parietal_cortex_position_t* result
);

/**
 * @brief Perform quantum-enhanced coordinate transform
 *
 * WHAT: Transform coordinates using quantum parallelism
 * WHY: Efficient multi-frame reasoning
 * HOW: Use frame superposition and selective collapse
 *
 * @param bridge Quantum bridge
 * @param position Input position
 * @param from_frame Source reference frame
 * @param to_frame Target reference frame
 * @param result Output: transformed position
 * @return 0 on success, -1 on error
 */
int parietal_quantum_transform(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* position,
    parietal_cortex_spatial_frame_t from_frame,
    parietal_cortex_spatial_frame_t to_frame,
    parietal_cortex_position_t* result
);

/*=============================================================================
 * TRAJECTORY OPTIMIZATION API
 *===========================================================================*/

/**
 * @brief Optimize motor trajectory using quantum search
 *
 * WHAT: Find optimal reaching trajectory
 * WHY: Enable efficient motor planning
 * HOW: Use quantum search over trajectory space
 *
 * @param bridge Quantum bridge
 * @param start Start position
 * @param target Target position
 * @param obstacles Obstacle positions (array)
 * @param num_obstacles Number of obstacles
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int parietal_quantum_optimize_trajectory(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    const parietal_cortex_position_t* target,
    const parietal_cortex_position_t* obstacles,
    uint32_t num_obstacles,
    quantum_trajectory_result_t* result
);

/**
 * @brief Create trajectory superposition for reach planning
 *
 * WHAT: Maintain multiple trajectory options in superposition
 * WHY: Keep options open until more information available
 * HOW: Weight trajectories by quality metrics
 *
 * @param bridge Quantum bridge
 * @param start Start position
 * @param target Target position
 * @param num_candidates Number of trajectory candidates to generate
 * @return 0 on success, -1 on error
 */
int parietal_quantum_superpose_trajectories(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    const parietal_cortex_position_t* target,
    uint32_t num_candidates
);

/**
 * @brief Collapse trajectory superposition to execute
 *
 * WHAT: Select final trajectory for execution
 * WHY: Commit to motor plan
 * HOW: Collapse to highest amplitude trajectory
 *
 * @param bridge Quantum bridge
 * @param trajectory Output: selected trajectory
 * @return 0 on success, -1 on error
 */
int parietal_quantum_collapse_trajectory(
    parietal_quantum_bridge_t* bridge,
    quantum_trajectory_candidate_t* trajectory
);

/*=============================================================================
 * QUANTUM WALK API
 *===========================================================================*/

/**
 * @brief Initialize quantum walk on spatial graph
 *
 * WHAT: Set up quantum walk for spatial navigation
 * WHY: Enable efficient spatial exploration
 * HOW: Create quantum walker on spatial graph
 *
 * @param bridge Quantum bridge
 * @param start Starting position on graph
 * @param graph_size Size of spatial graph
 * @return 0 on success, -1 on error
 */
int parietal_quantum_walk_init(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    uint32_t graph_size
);

/**
 * @brief Step quantum walk forward
 *
 * WHAT: Evolve quantum walker one step
 * WHY: Explore spatial graph
 * HOW: Apply coin and shift operators
 *
 * @param bridge Quantum bridge
 * @param steps Number of steps to take
 * @return 0 on success, -1 on error
 */
int parietal_quantum_walk_step(
    parietal_quantum_bridge_t* bridge,
    uint32_t steps
);

/**
 * @brief Measure quantum walk position
 *
 * WHAT: Collapse walker to position
 * WHY: Get navigation result
 * HOW: Measure position register
 *
 * @param bridge Quantum bridge
 * @param position Output: measured position
 * @param probability Output: probability of this position
 * @return 0 on success, -1 on error
 */
int parietal_quantum_walk_measure(
    parietal_quantum_bridge_t* bridge,
    parietal_cortex_position_t* position,
    float* probability
);

/**
 * @brief Use quantum walk for target search
 *
 * WHAT: Find target using quantum walk
 * WHY: O(sqrt(N)) search on spatial graph
 * HOW: Modified quantum walk with target marking
 *
 * @param bridge Quantum bridge
 * @param start Start position
 * @param target Target position (for marking)
 * @param max_steps Maximum walk steps
 * @param result Output: search result
 * @return 0 on success, -1 on error
 */
int parietal_region_quantum_walk_search(
    parietal_quantum_bridge_t* bridge,
    const parietal_cortex_position_t* start,
    const parietal_cortex_position_t* target,
    uint32_t max_steps,
    quantum_spatial_result_t* result
);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get quantum parietal statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int parietal_region_quantum_get_stats(
    const parietal_quantum_bridge_t* bridge,
    parietal_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void parietal_region_quantum_reset_stats(parietal_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int parietal_region_quantum_get_config(
    const parietal_quantum_bridge_t* bridge,
    parietal_region_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_QUANTUM_BRIDGE_H */
