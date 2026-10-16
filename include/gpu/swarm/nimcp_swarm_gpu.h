//=============================================================================
// nimcp_swarm_gpu.h - GPU-Accelerated Swarm Intelligence Algorithms
//=============================================================================
/**
 * @file nimcp_swarm_gpu.h
 * @brief GPU-accelerated Swarm Intelligence operations using CUDA
 *
 * WHAT: CUDA kernels for parallel swarm algorithm computation
 * WHY:  Swarm algorithms are embarrassingly parallel - each agent processes
 *       independently, enabling massive GPU acceleration
 * HOW:  Custom kernels for flocking, consensus, pheromone, quorum, task allocation
 *
 * ARCHITECTURE:
 * - Flocking/Boids: Parallel force computation with spatial hashing
 * - Consensus: Parallel averaging and belief propagation
 * - Pheromone: 2D/3D grid convolution for diffusion and decay
 * - Quorum Sensing: Parallel signal concentration and threshold detection
 * - Task Allocation: Parallel auction algorithms
 * - Collision Detection: Spatial partitioning with grid/octree
 *
 * KEY INSIGHT:
 * Each agent can be processed by one CUDA thread. Neighbor interactions
 * are computed in parallel using spatial data structures.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SWARM_GPU_H
#define NIMCP_SWARM_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of agents supported per GPU */
#define SWARM_GPU_MAX_AGENTS 1000000

/** Maximum grid dimension for spatial hashing */
#define SWARM_GPU_MAX_GRID_DIM 256

/** Maximum pheromone types */
#define SWARM_GPU_MAX_PHEROMONE_TYPES 8

/** Maximum signal types for quorum sensing */
#define SWARM_GPU_MAX_SIGNAL_TYPES 16

/** Maximum neighbors per agent for flocking */
#define SWARM_GPU_MAX_NEIGHBORS 64

//=============================================================================
// 3D Vector Type (GPU-compatible)
//=============================================================================

/**
 * @brief 3D vector for GPU computations (aligned for coalesced access)
 */
typedef struct {
    float x;    /**< X component */
    float y;    /**< Y component */
    float z;    /**< Z component */
    float w;    /**< Padding for alignment (can be used for mass/weight) */
} nimcp_gpu_vec4_t;

//=============================================================================
// Flocking/Boids GPU Structures
//=============================================================================

/**
 * @brief Flocking parameters for GPU kernels
 */
typedef struct {
    float separation_weight;    /**< Weight for separation force */
    float alignment_weight;     /**< Weight for alignment force */
    float cohesion_weight;      /**< Weight for cohesion force */

    float separation_radius;    /**< Radius for separation detection */
    float alignment_radius;     /**< Radius for alignment detection */
    float cohesion_radius;      /**< Radius for cohesion detection */

    float max_speed;            /**< Maximum agent speed */
    float max_force;            /**< Maximum steering force */

    float obstacle_weight;      /**< Weight for obstacle avoidance */
    float goal_weight;          /**< Weight for goal seeking */
    float boundary_weight;      /**< Weight for boundary containment */

    float dt;                   /**< Time step */
} nimcp_flocking_gpu_params_t;

/**
 * @brief GPU flocking state (SoA layout for coalesced access)
 *
 * Structure-of-Arrays layout for efficient GPU memory access:
 * - positions: [x0,y0,z0,w0, x1,y1,z1,w1, ...]
 * - velocities: [vx0,vy0,vz0,vw0, vx1,vy1,vz1,vw1, ...]
 */
typedef struct {
    nimcp_gpu_tensor_t* positions;     /**< Agent positions [N x 4] */
    nimcp_gpu_tensor_t* velocities;    /**< Agent velocities [N x 4] */
    nimcp_gpu_tensor_t* accelerations; /**< Agent accelerations [N x 4] */
    nimcp_gpu_tensor_t* forces;        /**< Computed forces [N x 4] */

    nimcp_gpu_tensor_t* neighbor_counts;  /**< Number of neighbors [N] */
    nimcp_gpu_tensor_t* neighbor_indices; /**< Neighbor indices [N x max_neighbors] */

    size_t n_agents;                   /**< Number of agents */
    size_t max_neighbors;              /**< Max neighbors per agent */

    nimcp_flocking_gpu_params_t params; /**< Flocking parameters */
} nimcp_flocking_gpu_state_t;

/**
 * @brief Spatial hash grid for neighbor search
 */
typedef struct {
    nimcp_gpu_tensor_t* cell_start;    /**< Start index for each cell [cells] */
    nimcp_gpu_tensor_t* cell_end;      /**< End index for each cell [cells] */
    nimcp_gpu_tensor_t* particle_cells; /**< Cell ID for each particle [N] */
    nimcp_gpu_tensor_t* sorted_indices; /**< Sorted particle indices [N] */

    float cell_size;                   /**< Size of each cell */
    uint32_t grid_dim_x;               /**< Grid dimension X */
    uint32_t grid_dim_y;               /**< Grid dimension Y */
    uint32_t grid_dim_z;               /**< Grid dimension Z */
    float origin_x, origin_y, origin_z; /**< Grid origin */
} nimcp_spatial_hash_t;

//=============================================================================
// Consensus GPU Structures
//=============================================================================

/**
 * @brief Consensus parameters for GPU kernels
 */
typedef struct {
    float learning_rate;        /**< Averaging/update rate */
    float min_confidence;       /**< Minimum confidence threshold */
    float byzantine_threshold;  /**< Byzantine fault threshold (1/3) */
    uint32_t max_iterations;    /**< Max averaging iterations */
} nimcp_consensus_gpu_params_t;

/**
 * @brief GPU consensus state
 */
typedef struct {
    nimcp_gpu_tensor_t* beliefs;       /**< Agent beliefs [N x belief_dim] */
    nimcp_gpu_tensor_t* confidences;   /**< Agent confidences [N] */
    nimcp_gpu_tensor_t* weights;       /**< Neighbor weights (adjacency) [N x N] or sparse */
    nimcp_gpu_tensor_t* new_beliefs;   /**< Updated beliefs (double buffer) [N x belief_dim] */

    size_t n_agents;                   /**< Number of agents */
    size_t belief_dim;                 /**< Dimension of belief vectors */

    nimcp_consensus_gpu_params_t params; /**< Consensus parameters */
} nimcp_consensus_gpu_state_t;

//=============================================================================
// Pheromone GPU Structures
//=============================================================================

/**
 * @brief Pheromone parameters for GPU kernels
 */
typedef struct {
    float decay_rates[SWARM_GPU_MAX_PHEROMONE_TYPES];  /**< Decay rate per type */
    float diffusion_rate;       /**< Diffusion coefficient */
    float evaporation_rate;     /**< Evaporation rate */
    float max_concentration;    /**< Maximum concentration */
    float deposit_amount;       /**< Default deposit amount */
} nimcp_pheromone_gpu_params_t;

/**
 * @brief GPU pheromone grid state
 */
typedef struct {
    nimcp_gpu_tensor_t* concentration; /**< Pheromone grid [Z x Y x X x types] or [Y x X x types] */
    nimcp_gpu_tensor_t* gradient;      /**< Gradient field [Z x Y x X x 3] (optional) */
    nimcp_gpu_tensor_t* temp_buffer;   /**< Temp buffer for diffusion (double buffer) */

    uint32_t grid_x, grid_y, grid_z;   /**< Grid dimensions */
    uint32_t n_types;                  /**< Number of pheromone types */
    float voxel_size;                  /**< Size of each voxel */
    float origin_x, origin_y, origin_z; /**< Grid origin */

    nimcp_pheromone_gpu_params_t params; /**< Pheromone parameters */
} nimcp_pheromone_gpu_state_t;

//=============================================================================
// Quorum Sensing GPU Structures
//=============================================================================

/**
 * @brief Quorum sensing parameters for GPU kernels
 */
typedef struct {
    float base_threshold;       /**< Base activation threshold */
    float decay_rate;           /**< Signal decay rate */
    float amplification;        /**< Positive feedback amplification */
    float inhibition;           /**< Cross-inhibition strength */
    float commitment_low;       /**< Low commitment threshold */
    float commitment_high;      /**< High commitment threshold */
} nimcp_quorum_gpu_params_t;

/**
 * @brief GPU quorum sensing state
 */
typedef struct {
    nimcp_gpu_tensor_t* signal_concentrations; /**< Signal concentrations [n_types] */
    nimcp_gpu_tensor_t* agent_signals;         /**< Per-agent signals [N x n_types] */
    nimcp_gpu_tensor_t* agent_commitments;     /**< Agent commitment states [N] */
    nimcp_gpu_tensor_t* agent_strengths;       /**< Commitment strengths [N] */
    nimcp_gpu_tensor_t* threshold_reached;     /**< Threshold flags [n_types] */

    size_t n_agents;                   /**< Number of agents */
    size_t n_signal_types;             /**< Number of signal types */

    nimcp_quorum_gpu_params_t params;  /**< Quorum parameters */
} nimcp_quorum_gpu_state_t;

//=============================================================================
// Task Allocation GPU Structures
//=============================================================================

/**
 * @brief Task allocation parameters for GPU kernels
 */
typedef struct {
    float bid_increment;        /**< Minimum bid increment */
    float epsilon;              /**< Convergence threshold */
    uint32_t max_rounds;        /**< Maximum auction rounds */
} nimcp_task_alloc_gpu_params_t;

/**
 * @brief GPU task allocation state (auction-based)
 */
typedef struct {
    nimcp_gpu_tensor_t* agent_capabilities; /**< Agent capabilities [N x n_cap_types] */
    nimcp_gpu_tensor_t* task_requirements;  /**< Task requirements [M x n_cap_types] */
    nimcp_gpu_tensor_t* bids;               /**< Current bids [N x M] */
    nimcp_gpu_tensor_t* prices;             /**< Task prices [M] */
    nimcp_gpu_tensor_t* assignments;        /**< Task assignments [N] (task ID or -1) */
    nimcp_gpu_tensor_t* best_bids;          /**< Best bid per task [M] */
    nimcp_gpu_tensor_t* best_agents;        /**< Best agent per task [M] */

    size_t n_agents;                   /**< Number of agents */
    size_t n_tasks;                    /**< Number of tasks */
    size_t n_capability_types;         /**< Number of capability dimensions */

    nimcp_task_alloc_gpu_params_t params; /**< Allocation parameters */
} nimcp_task_alloc_gpu_state_t;

//=============================================================================
// Collision Detection GPU Structures
//=============================================================================

/**
 * @brief Collision detection parameters
 */
typedef struct {
    float collision_radius;     /**< Default collision radius */
    float grid_cell_size;       /**< Spatial grid cell size */
    bool use_variable_radius;   /**< Use per-agent radius */
} nimcp_collision_gpu_params_t;

/**
 * @brief GPU collision detection state
 */
typedef struct {
    nimcp_gpu_tensor_t* positions;      /**< Agent positions [N x 4] */
    nimcp_gpu_tensor_t* radii;          /**< Agent radii [N] (optional) */
    nimcp_gpu_tensor_t* collision_flags; /**< Collision detected [N] */
    nimcp_gpu_tensor_t* collision_pairs; /**< Collision pairs [max_pairs x 2] */
    nimcp_gpu_tensor_t* pair_count;     /**< Number of collision pairs */

    nimcp_spatial_hash_t* spatial_hash; /**< Spatial hash for broad phase */

    size_t n_agents;                    /**< Number of agents */
    size_t max_pairs;                   /**< Maximum collision pairs */

    nimcp_collision_gpu_params_t params; /**< Collision parameters */
} nimcp_collision_gpu_state_t;

//=============================================================================
// Flocking/Boids GPU API
//=============================================================================

/**
 * @brief Create GPU flocking state
 *
 * @param ctx GPU context
 * @param n_agents Number of agents
 * @param max_neighbors Maximum neighbors per agent
 * @param params Flocking parameters (NULL for defaults)
 * @return GPU flocking state or NULL on failure
 */
NIMCP_EXPORT nimcp_flocking_gpu_state_t* nimcp_flocking_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t max_neighbors,
    const nimcp_flocking_gpu_params_t* params
);

/**
 * @brief Destroy GPU flocking state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_flocking_gpu_destroy(nimcp_flocking_gpu_state_t* state);

/**
 * @brief Get default flocking parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_flocking_gpu_default_params(nimcp_flocking_gpu_params_t* params);

/**
 * @brief Compute separation force (avoid crowding)
 *
 * Each agent steers away from nearby neighbors within separation_radius.
 * Force magnitude is inversely proportional to distance.
 *
 * @param ctx GPU context
 * @param state Flocking state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_flocking_separation(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state
);

/**
 * @brief Compute alignment force (match velocity with neighbors)
 *
 * Each agent steers toward average heading of neighbors within alignment_radius.
 *
 * @param ctx GPU context
 * @param state Flocking state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_flocking_alignment(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state
);

/**
 * @brief Compute cohesion force (move toward center of mass)
 *
 * Each agent steers toward center of mass of neighbors within cohesion_radius.
 *
 * @param ctx GPU context
 * @param state Flocking state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_flocking_cohesion(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state
);

/**
 * @brief Compute all flocking forces combined
 *
 * Efficiently computes separation + alignment + cohesion in single pass.
 *
 * @param ctx GPU context
 * @param state Flocking state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_flocking_compute_forces(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state
);

/**
 * @brief Update agent positions and velocities
 *
 * Applies computed forces to update agent kinematics.
 *
 * @param ctx GPU context
 * @param state Flocking state
 * @param dt Time step (0 to use params.dt)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_flocking_update(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state,
    float dt
);

/**
 * @brief Build spatial hash for neighbor search
 *
 * @param ctx GPU context
 * @param hash Spatial hash structure
 * @param positions Agent positions [N x 4]
 * @param n_agents Number of agents
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_spatial_hash_build(
    nimcp_gpu_context_t* ctx,
    nimcp_spatial_hash_t* hash,
    const nimcp_gpu_tensor_t* positions,
    size_t n_agents
);

/**
 * @brief Find neighbors using spatial hash
 *
 * @param ctx GPU context
 * @param state Flocking state with spatial hash
 * @param radius Search radius
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_flocking_find_neighbors(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state,
    float radius
);

//=============================================================================
// Consensus GPU API
//=============================================================================

/**
 * @brief Create GPU consensus state
 *
 * @param ctx GPU context
 * @param n_agents Number of agents
 * @param belief_dim Dimension of belief vectors
 * @param params Consensus parameters (NULL for defaults)
 * @return GPU consensus state or NULL on failure
 */
NIMCP_EXPORT nimcp_consensus_gpu_state_t* nimcp_consensus_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t belief_dim,
    const nimcp_consensus_gpu_params_t* params
);

/**
 * @brief Destroy GPU consensus state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_consensus_gpu_destroy(nimcp_consensus_gpu_state_t* state);

/**
 * @brief Get default consensus parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_consensus_gpu_default_params(nimcp_consensus_gpu_params_t* params);

/**
 * @brief Parallel averaging protocol step
 *
 * Each agent updates belief as weighted average of neighbors' beliefs.
 * new_belief[i] = sum(w[i,j] * belief[j]) for all neighbors j
 *
 * @param ctx GPU context
 * @param state Consensus state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_consensus_averaging(
    nimcp_gpu_context_t* ctx,
    nimcp_consensus_gpu_state_t* state
);

/**
 * @brief Belief propagation step
 *
 * Propagates beliefs through network using message passing.
 *
 * @param ctx GPU context
 * @param state Consensus state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_consensus_belief_propagation(
    nimcp_gpu_context_t* ctx,
    nimcp_consensus_gpu_state_t* state
);

/**
 * @brief Opinion dynamics update
 *
 * Updates agent opinions based on social influence model.
 *
 * @param ctx GPU context
 * @param state Consensus state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_consensus_opinion_dynamics(
    nimcp_gpu_context_t* ctx,
    nimcp_consensus_gpu_state_t* state
);

/**
 * @brief Check consensus convergence
 *
 * @param ctx GPU context
 * @param state Consensus state
 * @param converged Output: true if converged
 * @param variance Output: current belief variance
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_consensus_check_convergence(
    nimcp_gpu_context_t* ctx,
    const nimcp_consensus_gpu_state_t* state,
    bool* converged,
    float* variance
);

//=============================================================================
// Pheromone GPU API
//=============================================================================

/**
 * @brief Create GPU pheromone state
 *
 * @param ctx GPU context
 * @param grid_x Grid dimension X
 * @param grid_y Grid dimension Y
 * @param grid_z Grid dimension Z (1 for 2D)
 * @param n_types Number of pheromone types
 * @param voxel_size Size of each voxel
 * @param params Pheromone parameters (NULL for defaults)
 * @return GPU pheromone state or NULL on failure
 */
NIMCP_EXPORT nimcp_pheromone_gpu_state_t* nimcp_pheromone_gpu_create(
    nimcp_gpu_context_t* ctx,
    uint32_t grid_x,
    uint32_t grid_y,
    uint32_t grid_z,
    uint32_t n_types,
    float voxel_size,
    const nimcp_pheromone_gpu_params_t* params
);

/**
 * @brief Destroy GPU pheromone state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_pheromone_gpu_destroy(nimcp_pheromone_gpu_state_t* state);

/**
 * @brief Get default pheromone parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_pheromone_gpu_default_params(nimcp_pheromone_gpu_params_t* params);

/**
 * @brief Apply pheromone diffusion
 *
 * 3D convolution with diffusion kernel (Laplacian).
 *
 * @param ctx GPU context
 * @param state Pheromone state
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pheromone_diffusion(
    nimcp_gpu_context_t* ctx,
    nimcp_pheromone_gpu_state_t* state,
    float dt
);

/**
 * @brief Apply pheromone decay
 *
 * Exponential decay: c = c * exp(-decay_rate * dt)
 *
 * @param ctx GPU context
 * @param state Pheromone state
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pheromone_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_pheromone_gpu_state_t* state,
    float dt
);

/**
 * @brief Deposit pheromone at positions
 *
 * @param ctx GPU context
 * @param state Pheromone state
 * @param positions Deposit positions [N x 3]
 * @param types Pheromone types [N]
 * @param amounts Deposit amounts [N]
 * @param n_deposits Number of deposits
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pheromone_deposit(
    nimcp_gpu_context_t* ctx,
    nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions,
    const nimcp_gpu_tensor_t* types,
    const nimcp_gpu_tensor_t* amounts,
    size_t n_deposits
);

/**
 * @brief Sample pheromone concentration at positions
 *
 * @param ctx GPU context
 * @param state Pheromone state
 * @param positions Query positions [N x 3]
 * @param type Pheromone type to sample
 * @param output Sampled concentrations [N]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pheromone_sample(
    nimcp_gpu_context_t* ctx,
    const nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions,
    uint32_t type,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Compute pheromone gradient at positions
 *
 * @param ctx GPU context
 * @param state Pheromone state
 * @param positions Query positions [N x 3]
 * @param type Pheromone type
 * @param gradients Output gradients [N x 3]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pheromone_gradient(
    nimcp_gpu_context_t* ctx,
    const nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions,
    uint32_t type,
    nimcp_gpu_tensor_t* gradients
);

//=============================================================================
// Quorum Sensing GPU API
//=============================================================================

/**
 * @brief Create GPU quorum sensing state
 *
 * @param ctx GPU context
 * @param n_agents Number of agents
 * @param n_signal_types Number of signal types
 * @param params Quorum parameters (NULL for defaults)
 * @return GPU quorum state or NULL on failure
 */
NIMCP_EXPORT nimcp_quorum_gpu_state_t* nimcp_quorum_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t n_signal_types,
    const nimcp_quorum_gpu_params_t* params
);

/**
 * @brief Destroy GPU quorum sensing state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_quorum_gpu_destroy(nimcp_quorum_gpu_state_t* state);

/**
 * @brief Get default quorum parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_quorum_gpu_default_params(nimcp_quorum_gpu_params_t* params);

/**
 * @brief Compute signal concentration from all agents
 *
 * Parallel reduction to sum agent signal contributions.
 *
 * @param ctx GPU context
 * @param state Quorum state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_quorum_compute_concentration(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state
);

/**
 * @brief Check threshold activation for all signals
 *
 * @param ctx GPU context
 * @param state Quorum state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_quorum_check_thresholds(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state
);

/**
 * @brief Update agent commitments based on signals
 *
 * @param ctx GPU context
 * @param state Quorum state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_quorum_update_commitments(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state
);

/**
 * @brief Apply signal decay
 *
 * @param ctx GPU context
 * @param state Quorum state
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_quorum_decay_signals(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state,
    float dt
);

//=============================================================================
// Task Allocation GPU API
//=============================================================================

/**
 * @brief Create GPU task allocation state
 *
 * @param ctx GPU context
 * @param n_agents Number of agents
 * @param n_tasks Number of tasks
 * @param n_capability_types Number of capability dimensions
 * @param params Allocation parameters (NULL for defaults)
 * @return GPU task allocation state or NULL on failure
 */
NIMCP_EXPORT nimcp_task_alloc_gpu_state_t* nimcp_task_alloc_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t n_tasks,
    size_t n_capability_types,
    const nimcp_task_alloc_gpu_params_t* params
);

/**
 * @brief Destroy GPU task allocation state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_task_alloc_gpu_destroy(nimcp_task_alloc_gpu_state_t* state);

/**
 * @brief Get default task allocation parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_task_alloc_gpu_default_params(nimcp_task_alloc_gpu_params_t* params);

/**
 * @brief Run parallel auction round
 *
 * Agents bid on tasks based on capability match and current prices.
 *
 * @param ctx GPU context
 * @param state Task allocation state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_task_auction_round(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state
);

/**
 * @brief Update task prices based on bids
 *
 * @param ctx GPU context
 * @param state Task allocation state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_task_update_prices(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state
);

/**
 * @brief Finalize task assignments
 *
 * @param ctx GPU context
 * @param state Task allocation state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_task_finalize_assignments(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state
);

/**
 * @brief Compute capability match matrix
 *
 * match[i,j] = how well agent i matches task j requirements
 *
 * @param ctx GPU context
 * @param state Task allocation state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_task_compute_matches(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state
);

//=============================================================================
// Collision Detection GPU API
//=============================================================================

/**
 * @brief Create GPU collision detection state
 *
 * @param ctx GPU context
 * @param n_agents Number of agents
 * @param max_pairs Maximum collision pairs to track
 * @param params Collision parameters (NULL for defaults)
 * @return GPU collision state or NULL on failure
 */
NIMCP_EXPORT nimcp_collision_gpu_state_t* nimcp_collision_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t max_pairs,
    const nimcp_collision_gpu_params_t* params
);

/**
 * @brief Destroy GPU collision detection state
 *
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_collision_gpu_destroy(nimcp_collision_gpu_state_t* state);

/**
 * @brief Get default collision parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_collision_gpu_default_params(nimcp_collision_gpu_params_t* params);

/**
 * @brief Detect collisions (broad + narrow phase)
 *
 * Uses spatial hash for broad phase, then exact distance check.
 *
 * @param ctx GPU context
 * @param state Collision state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_collision_detect(
    nimcp_gpu_context_t* ctx,
    nimcp_collision_gpu_state_t* state
);

/**
 * @brief Compute pairwise distances
 *
 * @param ctx GPU context
 * @param positions Agent positions [N x 4]
 * @param distances Output distance matrix [N x N] (or sparse)
 * @param n_agents Number of agents
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pairwise_distances(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* positions,
    nimcp_gpu_tensor_t* distances,
    size_t n_agents
);

/**
 * @brief Get collision pairs
 *
 * @param ctx GPU context
 * @param state Collision state
 * @param pairs_out Output array for collision pairs
 * @param max_pairs Maximum pairs to return
 * @param count_out Number of pairs found
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_collision_get_pairs(
    nimcp_gpu_context_t* ctx,
    const nimcp_collision_gpu_state_t* state,
    uint32_t* pairs_out,
    size_t max_pairs,
    size_t* count_out
);

//=============================================================================
// Spatial Hash Utility API
//=============================================================================

/**
 * @brief Create spatial hash
 *
 * @param ctx GPU context
 * @param cell_size Size of each cell
 * @param grid_dim_x Grid dimension X
 * @param grid_dim_y Grid dimension Y
 * @param grid_dim_z Grid dimension Z
 * @param max_particles Maximum particles
 * @return Spatial hash or NULL on failure
 */
NIMCP_EXPORT nimcp_spatial_hash_t* nimcp_spatial_hash_create(
    nimcp_gpu_context_t* ctx,
    float cell_size,
    uint32_t grid_dim_x,
    uint32_t grid_dim_y,
    uint32_t grid_dim_z,
    size_t max_particles
);

/**
 * @brief Destroy spatial hash
 *
 * @param hash Spatial hash to destroy
 */
NIMCP_EXPORT void nimcp_spatial_hash_destroy(nimcp_spatial_hash_t* hash);

/**
 * @brief Clear spatial hash
 *
 * @param ctx GPU context
 * @param hash Spatial hash
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_spatial_hash_clear(
    nimcp_gpu_context_t* ctx,
    nimcp_spatial_hash_t* hash
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SWARM_GPU_H
